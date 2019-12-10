#ifndef BRCCONNECTION_H
#define BRCCONNECTION_H
#include "network.h"
#include "cm_ctors.h"
#include "runners.h"
#include <boost/version.hpp>

#define MY_BOOST_MINOR ((BOOST_VERSION / 100) % 1000)


class BrcConnection : public std::enable_shared_from_this<BrcConnection>
{
public:
    BrcConnection() = delete;
    ~BrcConnection();
    NO_COPYMOVE(BrcConnection);

    explicit BrcConnection(const network::SocketPtr& socket);
    void start();
private:
    network::SocketPtr socket;
    utility::runner_t cThread{nullptr};
};

#endif // BRCCONNECTION_H
