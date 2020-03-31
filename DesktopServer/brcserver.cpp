#include "brcserver.h"
#include "brcconnection.h"

using namespace boost::asio::ip;
BrcServer::BrcServer(boost::asio::io_service &ios, const boost::asio::ip::tcp::endpoint &endpoint):
    acceptor(ios, endpoint)
{
}

void BrcServer::do_accept()
{
#if MY_BOOST_MINOR > 69
    const auto socket = pools::allocShared<tcp::socket>(acceptor.get_executor());
#else
    const auto socket = pools::allocShared<tcp::socket>(acceptor.get_io_service());
#endif
    const std::weak_ptr<BrcServer> WThis {shared_from_this()};
    acceptor.async_accept(*socket, [WThis, socket](boost::system::error_code ec)
    {
        if (auto This = WThis.lock())
        {
            if (!ec)
                (This->lastConn = BrcConnection::allocate(socket))->start();

            if (!goingDown())
                This->do_accept();
        }
    });
}

BrcServerPtr BrcServer::create(boost::asio::io_service &ios, const tcp::endpoint &endpoint)
{
    BrcServerPtr p{new BrcServer(ios, endpoint)};
    p->do_accept();
    return p;
}
