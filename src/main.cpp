#include  <memory>

#include "App.h"
#include "ConfigParser.h"
#include "UdpSocket.h"

using namespace std;
using namespace rgc; // reliable group comm

int main(int argc, char *argv[])
{ 
    optional<config_t> optConfig = getConfigFromOptions(argc, argv);
    if (!optConfig.has_value())
    {
        printUsage(argv[0]);
        exit(1);
    }

    auto udpRxSocket = make_unique<UdpRxSocket>(std::string("127.0.0.1"), (*optConfig).udpPort);
    vector<unique_ptr<UdpTxSocket>> udpTxSockets;
    vector<ITxSocket *> txSockets;

    for (auto const &peer : (*optConfig).peers)
    {
        udpTxSockets.push_back(std::make_unique<UdpTxSocket>(peer));
        txSockets.push_back(udpTxSockets.back().get());
    }    

    App myApp(udpRxSocket.get(), txSockets);
    myApp.run();

    return 0;
}
