#include "brcconnection.h"
#include "brcserver.h"
#include "broadcast.h"
#include "cm_ctors.h"
#include "server_version.h"
#include "ScreenCapture.h"

//--------------------------------------------------------------------------------------------------------
using namespace protocol::broadcast;

// this holds requests from client according to protocol and basicaly is finite state machine
class FromClientFsm : public protocol::broadcast::request::Receiver
{

private:
    std::ostream& os;
    request::connect clientVersion;
public:
    FromClientFsm() = delete;
    NO_COPYMOVE(FromClientFsm);
    ~FromClientFsm() override = default;
    explicit FromClientFsm(std::ostream& os): os(os) {}

public:
    void handle(request::connect& msg) final
    {
        clientVersion = msg;

        reply::connected rply;
        rply.server_version = SERVER_INT_VERSION;
        rply.marshal(os);
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

                FromClientFsm fsm(os);
                while (!*should_stop && !BrcServer::goingDown() && socket->is_open())
                {
                    if (network::readableBytes(socket) && network::readFromSocket(socket, in_buffer))
                    {
                        //class FromClientFsm handles current message in stream and puts output to os
                        request::Base::unmarshal(is)->deliverTo(fsm);
                        network::writeToSocket(out_buffer, socket);
                    }
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
