#ifndef BRCSERVER_H
#define BRCSERVER_H
#include <boost/asio.hpp>
#include <list>
#include "pooled_shared.h"
#include "cm_ctors.h"
#include "brc_conn_ptr.h"


class BrcServer : public std::enable_shared_from_this<BrcServer>
{
public:
    BrcServer() = delete;
    NO_COPYMOVE(BrcServer);
    ~BrcServer() = default;
    static auto& goingDown()
    {
        static std::atomic<bool> tmp{false};
        return tmp;
    }

    BrcServerPtr static create(boost::asio::io_service& ios, const boost::asio::ip::tcp::endpoint& endpoint);
private:
    BrcServer(boost::asio::io_service& ios, const boost::asio::ip::tcp::endpoint& endpoint);
    boost::asio::ip::tcp::acceptor acceptor;

    //only 1 connection will be active, each new drops old for now
    BrcConnectionPtr lastConn;
    void do_accept();
};

#endif // BRCSERVER_H
