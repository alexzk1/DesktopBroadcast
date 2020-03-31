#ifndef BRCCONNECTION_H
#define BRCCONNECTION_H
#include "network.h"
#include "cm_ctors.h"
#include "runners.h"
#include "brc_conn_ptr.h"
#include "pooled_shared.h"
#include <boost/version.hpp>

#define MY_BOOST_MINOR ((BOOST_VERSION / 100) % 1000)


class BrcConnection : public std::enable_shared_from_this<BrcConnection>
{
public:
    BrcConnection() = delete;
    ~BrcConnection();
    NO_COPYMOVE(BrcConnection);

    void start();

    static BrcConnectionPtr allocate(const network::SocketPtr& socket)
    {
        return pools::allocShared<BrcConnection>(socket);
    }
private:
    DECLARE_FRIEND_POOL; //giving access to private ctor for pools::allocShared
    explicit BrcConnection(const network::SocketPtr& socket);
    network::SocketPtr socket;
    utility::runner_t cThread{nullptr};
};

#endif // BRCCONNECTION_H
