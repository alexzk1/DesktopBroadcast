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
#include "png_out.hpp"
#include "pixels.h"

//--------------------------------------------------------------------------------------------------------
using namespace protocol::broadcast;

using SocketWriteLock = spinlock;
using ImageVector     = std::vector<uint8_t>;

constexpr static int32_t IMAGE_NOFLAGS = 0;
constexpr static int32_t IMAGE_DELTA   = 1;
constexpr static int32_t IMAGE_PNG     = 2;

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


    void ExtractAndConvertToBGRA(const SL::Screen_Capture::Image &img, reply::frame& dst) const
    {
        using namespace pixel_format;
        using namespace SL::Screen_Capture;

        const size_t w = Width(img);
        const size_t h = Height(img);
        const size_t pixel_count = w * h;

        static_assert(sizeof(ImageBGRA) == 4, "Expecting 4 bytes/pixel!");
        pools::PooledVector<uint8_t> rgb;
        rgb.resize(pixel_count * 3);

        //extracting BGRA image to RGB vector
        auto startsrc = StartSrc(img);
        const auto cast_src = [&startsrc]()
        {
            uint8_t *p;
            memcpy(&p, &startsrc, sizeof(void*));
            return p;
        };

        if (isDataContiguous(img))
            convertBGRA8888_to_RGB888(Iterator8888::start(cast_src(), pixel_count), pixel_count,
                                      Iterator888::start(rgb, pixel_count));
        else
        {
            //todo: this is not tested yet, as never happened
            auto out = Iterator888::start(rgb, pixel_count);
            for (size_t i = 0; i < h; ++i)
            {
                out = convertBGRA8888_to_RGB888(Iterator8888::start(cast_src(), w), w, out);
                startsrc = GotoNextRow(img, startsrc);      // advance to the next row
            }
        }

        //checking if we can reduce image as destination has smaller screen
        const int shrinkW = w / clientVersion.screen_width;
        const int shrinkH = h / clientVersion.screen_height;


        const auto make_png = [&dst](const auto & src, int w, int h)
        {
            dst.w = w;
            dst.h = h;
            dst.flags |= IMAGE_PNG;

            dst.data.clear();
            dst.data.reserve(w * h * 3 + 100);
            TinyPngOut png(w, h, [&dst](const uint8_t* src, size_t sz)
            {
                std::copy_n(src, sz, std::back_inserter(dst.data));
            });
            png.write(src);
        };


        if (shrinkW > 1 || shrinkH > 1)
        {
            //need to shrink image
            //TODO: NOT TESTED! cannot run on my device yet as it's almost same as monitor resolution
            size_t nw = w / shrinkW;
            size_t nh = h / shrinkH;
            const size_t pixel_count = nw * nh;
            pools::PooledVector<uint8_t> rgb2;
            rgb2.resize(pixel_count * 3);
            auto out = Iterator888::start(rgb2, pixel_count);

            for (size_t j = 0; j < h; j += shrinkH)
                for (size_t i = 0; i < w; i += shrinkW)
                {
                    PixelAvr<uint8_t, 3> avr;
                    for (int k = 0; k < shrinkH; ++k)
                    {
                        const auto srcw(Iterator888::start(rgb.data() + (j + k) * w + i, pixel_count));
                        ALG_NS::for_each(OffsetIterator(0), OffsetIterator(shrinkW), [&srcw, &avr](size_t x)
                        {
                            avr.add(srcw[x]);
                        });
                    }
                    out = avr;
                    ++out;
                }
            make_png(rgb2, nw, nh);
        }
        else
            make_png(rgb, w, h);
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
