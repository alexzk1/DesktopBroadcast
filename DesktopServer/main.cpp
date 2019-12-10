#include "mainwindow.h"
#include "brcserver.h"
#include "network.h"
#include <signal.h>
#include <QApplication>
#include <chrono>


#ifdef OS_LINUX
    #include <X11/Xlib.h>
#endif

static void my_handler(int)
{
    bool expected = false;
    if (BrcServer::goingDown().compare_exchange_strong(expected, true))
    {
        BrcServer::goingDown() = true;
        std::cout << "\nRequested stop. Shutting down nicely.\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::cout << "\tStopping server...\n";
        network::context().stop();
        exit(0);
    }
}


int main(int argc, char *argv[])
{
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);

#ifdef OS_LINUX
    XInitThreads();
#endif

    //const unsigned short server_port =  vm["lport"].as<uint16_t>();
    const unsigned short server_port =  11222;


    network::externIP() = "0.0.0.0";
    network::externPort() = server_port;

    using boost::asio::ip::tcp;
    auto s = BrcServer::create(network::context(), tcp::endpoint(tcp::v4(), server_port));

    try
    {
        network::context().run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    //    QApplication a(argc, argv);
    //    MainWindow w;
    //    w.show();

    //    return a.exec();
    return 0;
}
