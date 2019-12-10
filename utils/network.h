#pragma once

#include <boost/asio.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/asio.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/positioning.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>

#include <streambuf>
#include "strutils.h"
#include "fatal_err.h"
#include "pooled_shared.h"

namespace network
{
    //that is single global object to deal with OS. It is thread-safe
    inline auto& context()
    {
        static boost::asio::io_context cont;
        return cont;
    }

    //global object to hold external IP, must be set on program launch and never changed
    //it is thread-unsafe to write to it
    //should be manually set by user in case of routers/lan/wan
    inline auto& externIP()
    {
        static std::string tmp;
        return tmp;
    }

    //same as IP above, but port
    inline auto& externPort()
    {
        static unsigned short port;
        return port;
    }

    using boost::asio::ip::tcp;
    using SocketPtr = std::shared_ptr<tcp::socket>;

    inline boost::asio::ip::tcp::endpoint getLocalEndPoint()
    {
        return {boost::asio::ip::make_address(externIP()), externPort()};
    }


    inline size_t readableBytes(const SocketPtr& socket)
    {
        boost::asio::socket_base::bytes_readable command;
        socket->io_control(command);
        return command.get();
    }

    //used for serialization which accept any object with read/write methods
    class tcpstream
    {
    private:
        boost::asio::io_context clientCont;
        const bool client_conn;
        SocketPtr socket{nullptr};
        size_t readPos{0}; //counts how many bytes read/written over connection
        size_t writePos{0};


        inline SocketPtr connectClientSocket(const std::string& address)
        {
            using boost::asio::deadline_timer;
            using boost::asio::ip::tcp;
            using boost::lambda::bind;
            using boost::lambda::var;
            using boost::lambda::_1;

            const std::string host = address.substr( 0, address.find(':') );
            const std::string port = address.substr( host.size() + 1 );
            tcp::resolver::query q(host, port);

            tcp::resolver resolver(clientCont);
            tcp::resolver::iterator iter = resolver.resolve(q);
            auto socket = pools::allocShared<tcp::socket>(clientCont);

            // Set up the variable that receives the result of the asynchronous
            // operation. The error code is set to would_block to signal that the
            // operation is incomplete. Asio guarantees that its asynchronous
            // operations will never fail with would_block, so any other value in
            // ec indicates completion.
            boost::system::error_code ec = boost::asio::error::would_block;

            // Start the asynchronous operation itself. The boost::lambda function
            // object is used as a callback and will update the ec variable when the
            // operation completes. The blocking_udp_client.cpp example shows how you
            // can use boost::bind rather than boost::lambda.
            boost::asio::async_connect(*socket, iter, var(ec) = _1);

            // Block until the asynchronous operation has completed.
            auto deadline = pools::allocShared<deadline_timer>(clientCont);
            deadline->expires_from_now( boost::posix_time::seconds(30));

            deadline->async_wait([socket, deadline, address](const boost::system::error_code&)
            {
                if (deadline->expires_at() <= deadline_timer::traits_type::now())
                {
                    std::cerr << "Aborting connection by timeout to " << address << std::endl;
                    boost::system::error_code ignored_ec;
                    socket->close(ignored_ec);
                }
            });


            do
            {
                clientCont.run_one();
            }
            while (ec == boost::asio::error::would_block);

            // Determine whether a connection was successfully established. The
            // deadline actor may have had a chance to run and close our socket, even
            // though the connect operation notionally succeeded. Therefore we must
            // check whether the socket is still open before deciding if we succeeded
            // or failed.
            if (ec || !socket->is_open())
            {
                std::cerr << "Could not connect to " << address << std::endl;
                throw boost::system::system_error(ec ? ec : boost::asio::error::operation_aborted);
            }

            return socket;
        }

    public:
        tcpstream() = delete;
        tcpstream(const SocketPtr& socket): clientCont(), client_conn{false}, socket(socket) {}
        tcpstream(SocketPtr&& socket): clientCont(), client_conn{false}, socket(std::move(socket)) {}
        tcpstream(const std::string& address): clientCont(), client_conn{true}, socket(connectClientSocket(address)) {}

        //good reference
        //http://charette.no-ip.com:81/programming/doxygen/boost/group__read.html#details

        bool connected() const
        {
            return socket->is_open();
        }

        bool write(const char* buf, size_t size)
        {
            assert(socket);
            //throws boost::system::system_error
            try
            {
                boost::asio::write(*socket, boost::asio::buffer(buf, size));
                writePos += size;
                return true;
            }
            catch (boost::system::system_error& s)
            {
                throw std::runtime_error(stringfmt("Failed to write to socket: %s", s.what()));
            }
            return false;
        }

        void read(char* buf, size_t size)
        {
            assert(socket);
            //throws boost::system::system_error
            try
            {
                boost::asio::read(*socket, boost::asio::buffer(buf, size));
                readPos += size;
            }
            catch (boost::system::system_error& s)
            {
                throw std::runtime_error(stringfmt("Failed to read from socket: %s", s.what()));
            }
        }

        //reads some bytes and just drop it
        void readIgnore(size_t size)
        {
            pools::PooledVector<char> tmp(size);
            read(tmp.data(), size);
        }

        void keep_alive(bool enable)
        {
            if (socket)
            {
                boost::asio::socket_base::keep_alive option(enable);
                socket->set_option(option);
            }
        }

        size_t readableBytes() const
        {
            return network::readableBytes(socket);
        }

        auto pos_read() const
        {
            return readPos;
        }

        auto pos_write() const
        {
            return  writePos;
        }

        auto remote_endpoint() const
        {
            return socket->remote_endpoint();
        }
        virtual ~tcpstream() = default;
    };

    size_t inline readFromSocket(const SocketPtr& src, boost::asio::streambuf& dest, size_t avail = 0)
    {
        if (!avail)
            avail = readableBytes(src);
        // reserve bytes in output sequence
        const size_t n = src->receive(dest.prepare(avail));
        // received data is "committed" from output sequence to input sequence
        dest.commit(n);
        return n;
    }

    size_t inline writeToSocket(boost::asio::streambuf& src, const SocketPtr& dest)
    {
        // try sending some data in input sequence
        const size_t n = dest->send(src.data());
        src.consume(n); // sent data is removed from input sequence
        return n;
    }

//--------------------------------------------------------------------------------------------------------
    //doing stream out of socket
//--------------------------------------------------------------------------------------------------------
    class sock_iostream_device
    {
    public:
        typedef char char_type;
        struct category
                : public virtual boost::iostreams::seekable_device_tag
        {};

        sock_iostream_device(boost::asio::ip::tcp::socket& sock)
            : m_sock(sock)
        {
        }

        std::streamsize read(char* buffer, std::streamsize n)
        {
            boost::system::error_code ec;
            std::streamsize len = m_sock.receive(boost::asio::buffer(buffer, n), 0, ec);
            if (ec)
                len = 0;
            return len;
        }

        std::streamsize write(const char* buffer, std::streamsize n)
        {

            boost::system::error_code ec;
            std::streamsize len = m_sock.send(boost::asio::buffer(buffer, n), 0, ec);
            if (ec)
                len = 0;
            return len;
        }

        std::streamsize seek(std::streamsize, std::ios_base::seekdir)
        {
            // not supported
            return 0;
        }

    private:
        boost::asio::ip::tcp::socket& m_sock;
    };

    using sock_iostream = boost::iostreams::stream< sock_iostream_device>;
}
