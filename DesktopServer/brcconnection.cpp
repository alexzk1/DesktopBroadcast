#include "brcconnection.h"
#include "brcserver.h"
#include "broadcast.h"
#include "cm_ctors.h"
#include "server_version.h"
#include "ScreenCapture.h"
#include "spinlock.h"
#include "guard_on.h"
#include "strutils.h"
#include <chrono>
#include "lodepng.h"

//--------------------------------------------------------------------------------------------------------
using namespace protocol::broadcast;

using SocketWriteLock = spinlock;
using ImageVector     = std::vector<uint8_t>;

constexpr static int32_t IMAGE_NOFLAGS = 0;
constexpr static int32_t IMAGE_DELTA   = 1;
constexpr static int32_t IMAGE_PNG     = 2;


void* lodepng_malloc(size_t size)
{
#ifdef LODEPNG_MAX_ALLOC
    if (size > LODEPNG_MAX_ALLOC) return 0;
#endif
    return malloc(size);
}

void* lodepng_realloc(void* ptr, size_t new_size)
{
#ifdef LODEPNG_MAX_ALLOC
    if (new_size > LODEPNG_MAX_ALLOC) return 0;
#endif
    return realloc(ptr, new_size);
}

void lodepng_free(void* ptr)
{
    free(ptr);
}

static void ExtractAndConvertToBGRA(const SL::Screen_Capture::Image &img, reply::frame& dst)
{
    pools::PooledVector<uint8_t> tmp;
    const auto w = SL::Screen_Capture::Width(img);
    const auto h = SL::Screen_Capture::Height(img);
    tmp.resize(static_cast<size_t>(w * h * sizeof(SL::Screen_Capture::ImageBGRA)));
    SL::Screen_Capture::Extract(img, tmp.data(), tmp.size());

    //fixing colors, as PNG compressor wants RGBA
    static_assert(sizeof(SL::Screen_Capture::ImageBGRA) == 4, "Expecting 4 bytes/pixel!");
    for (size_t i = 0, sz = tmp.size(); i < sz; i += sizeof(SL::Screen_Capture::ImageBGRA))
        std::swap(*(tmp.data() + i + 0), *(tmp.data() + i + 2));

    dst.w = w;
    dst.h = h;
    dst.flags |= IMAGE_PNG;
    unsigned char* png{nullptr};
    size_t osz{0};
    lodepng_encode_memory(&png, &osz, tmp.data(), w, h, LCT_RGBA, 8);

    dst.data.resize(osz);
    memcpy(dst.data.data(), png, osz);
    lodepng_free(png);
}


// this holds requests from client according to protocol and basicaly is finite state machine
class FromClientFsm : public protocol::broadcast::request::Receiver
{

private:
    std::ostream& os;
    request::connect clientVersion;
    SocketWriteLock& socket_write_lock;
    std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> framgrabber;

public:
    FromClientFsm() = delete;
    NO_COPYMOVE(FromClientFsm);
    ~FromClientFsm() override
    {
        framgrabber.reset();
    }
    explicit FromClientFsm(std::ostream& os, SocketWriteLock& socket_write_lock): os(os), socket_write_lock(socket_write_lock) {}


public:
    void handle(request::connect& msg) final
    {
        clientVersion = msg;

        reply::connected rply;
        rply.server_version = SERVER_INT_VERSION;

        {
            LOCK_GUARD_ON(socket_write_lock);
            rply.marshal(os);
            had_write = true;
        }

        std::cout << "Client: " << msg.version_client << ", (" << msg.screen_width << " x " << msg.screen_height << ")" << std::endl;

        startGrab();
    }

    bool hadWriteCallLocked()
    {
        bool r = had_write;
        had_write = false;
        return r;
    }
private:
    bool had_write{false};
    std::chrono::steady_clock::time_point started_at{now()};

    static std::chrono::steady_clock::time_point now()
    {
        using namespace std::chrono;
        return steady_clock::now();
    }

    int64_t elapsed() const
    {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(now() - started_at).count();
    }

    void startGrab()
    {
        auto frame_new = std::make_shared<reply::frame>();


        framgrabber = SL::Screen_Capture::CreateCaptureConfiguration([this]()
        {
            auto windows = SL::Screen_Capture::GetWindows();
            std::string srchterm = utility::toLower(clientVersion.win_caption);
            decltype(windows) filtereditems;
            for (const auto &a : windows)
            {
                std::string name = utility::toLower(a.Name);
                if (utility::strcontains(name, srchterm))
                {
                    filtereditems.push_back(a);
                    break;
                }
            }
            started_at = now();
            return filtereditems;
        })->onFrameChanged([&](const SL::Screen_Capture::Image & img, const SL::Screen_Capture::Window &)
        {
            // std::cout << "Difference detected!  " << img.Bounds << std::endl;
            //            auto r = realcounter.fetch_add(1);
            //            auto s = std::to_string(r) + std::string("WINDIF_") + std::string(".jpg");
            //            auto size = Width(img) * Height(img) * sizeof(SL::Screen_Capture::ImageBGRA);

            /* auto imgbuffer(std::make_unique<unsigned char[]>(size));
            ExtractAndConvertToRGBA(img, imgbuffer.get(), size);
            tje_encode_to_file(s.c_str(), Width(img), Height(img), 4, (const unsigned char*)imgbuffer.get());
            */
        })->onNewFrame([this, frame_new](const SL::Screen_Capture::Image & img, const SL::Screen_Capture::Window &)
        {
            frame_new->timestamp_ns = elapsed();
            frame_new->flags = IMAGE_NOFLAGS;
            ExtractAndConvertToBGRA(img, *frame_new);

            LOCK_GUARD_ON(socket_write_lock);
            frame_new->marshal(os);
            had_write = true;

        })->start_capturing();
    }
};

//--------------------------------------------------------------------------------------------------------
BrcConnection::~BrcConnection()
{
    //std::cout << "Connection destructor" << std::endl;
    cThread.reset();
}

BrcConnection::BrcConnection(const network::SocketPtr &socket):
    socket(socket)
{
}

void BrcConnection::start()
{
    using namespace protocol::broadcast;
    cThread = utility::startNewRunner([this](auto should_stop)
    {
        try
        {
            if (socket)
            {
                boost::asio::streambuf out_buffer;
                std::ostream os(&out_buffer);

                SocketWriteLock socket_write_lock;
                FromClientFsm fsm(os, socket_write_lock);

                const auto should_break_loop = [&should_stop, this]()
                {
                    return *should_stop || BrcServer::goingDown() || !socket->is_open();
                };


                while (!should_break_loop())
                {
                    const auto readable = network::readableBytes(socket); //debuger friendly
                    if (readable > 5)
                    {
                        try
                        {
                            network::sock_iostream is(*socket);
                            request::Base::unmarshal(is)->deliverTo(fsm);
                        }
                        catch (...)
                        {
                        }
                    }

                    lock_guard_conditional grd(socket_write_lock, should_break_loop);
                    if (grd.isLocked() && fsm.hadWriteCallLocked())
                        network::writeToSocket(out_buffer, socket);
                }
            }
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Something realy bad happened." << std::endl;
        }

        socket.reset();
    });
}
