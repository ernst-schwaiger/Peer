#include <memory>
#include <sstream>
#include <stdexcept>

#include <fmt/core.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "App.h"
#include "ConfigParser.h"
#include "UdpSocket.h"

using namespace std;
using namespace rgc; // reliable group comm

int main(int argc, char *argv[])
{
    try
    {
        optional<config_t> optConfig = getConfigFromOptions(argc, argv);
        if (!optConfig.has_value())
        {
            printUsage(argv[0]);
            return 1;
        }

        // Named pipe for receiving user commands
        string pipe_path = fmt::format("/tmp/peer_pipe_{}", (*optConfig).Id);
        // Rx Socket for receiving messages from other peers
        auto udpRxSocket = make_unique<UdpRxSocket>(std::string("127.0.0.1"), (*optConfig).udpPort);
        vector<unique_ptr<UdpTxSocket>> udpTxSockets;
        vector<ITxSocket *> txSockets;
        // Tx Sockets for each remote peer for sending messages
        for (auto const &peer : (*optConfig).peers)
        {
            udpTxSockets.push_back(std::make_unique<UdpTxSocket>(peer, udpRxSocket->getSocketDescriptor()));
            txSockets.push_back(udpTxSockets.back().get());
        }    

        App myApp((*optConfig).Id, udpRxSocket.get(), txSockets, (*optConfig).logFile, pipe_path);
        myApp.run();
    }
    catch(const std::runtime_error& e)
    {
        std::cerr << "Runtime error: " << e.what() << '\n';
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}
