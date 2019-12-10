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
                (This->lastConn = pools::allocShared<BrcConnection>(socket))->start();

            if (!goingDown())
                This->do_accept();
        }
    });
}

std::shared_ptr<BrcServer> BrcServer::create(boost::asio::io_service &ios, const tcp::endpoint &endpoint)
{
    std::shared_ptr<BrcServer> p{new BrcServer(ios, endpoint)};
    p->do_accept();
    return p;
}
