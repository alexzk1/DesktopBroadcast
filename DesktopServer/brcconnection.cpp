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

//--------------------------------------------------------------------------------------------------------
using namespace protocol::broadcast;

using SocketWriteLock = spinlock;
using ImageVector     = std::vector<uint8_t>;

static void ExtractAndConvertToBGRA(const SL::Screen_Capture::Image &img, ImageVector& dst)
{
    dst.resize(static_cast<size_t>(SL::Screen_Capture::Width(img) * SL::Screen_Capture::Height(img) * sizeof(SL::Screen_Capture::ImageBGRA)));
    SL::Screen_Capture::Extract(img, dst.data(), dst.size());
}

constexpr static int32_t IMAGE_NOFLAGS = 0;
constexpr static int32_t IMAGE_DELTA   = 1;
constexpr static int32_t IMAGE_LZ4     = 2;

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
        }
        startGrab();
    }
private:
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
            ExtractAndConvertToBGRA(img, frame_new->data);

            LOCK_GUARD_ON(socket_write_lock);
            frame_new->marshal(os);

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
                boost::asio::streambuf in_buffer;
                boost::asio::streambuf out_buffer;
                std::istream is(&in_buffer);
                std::ostream os(&out_buffer);

                SocketWriteLock socket_write_lock;
                FromClientFsm fsm(os, socket_write_lock);

                const auto should_break_loop = [&should_stop, this]()
                {
                    return *should_stop || BrcServer::goingDown() || !socket->is_open();
                };

                while (!should_break_loop())
                {
                    if (network::readableBytes(socket) && network::readFromSocket(socket, in_buffer))
                    {
                        //class FromClientFsm handles current message in stream and puts output to os
                        request::Base::unmarshal(is)->deliverTo(fsm);
                    }

                    lock_guard_conditional grd(socket_write_lock, should_break_loop);
                    if (grd.isLocked())
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
