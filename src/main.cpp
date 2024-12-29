#include <memory>
#include <sstream>
#include <filesystem>
#include <fmt/core.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "App.h"
#include "ConfigParser.h"
#include "UdpSocket.h"

using namespace std;
using namespace rgc; // reliable group comm
using namespace std::filesystem;

int main(int argc, char *argv[])
{
    optional<config_t> optConfig = getConfigFromOptions(argc, argv);
    if (!optConfig.has_value())
    {
        printUsage(argv[0]);
        exit(1);
    }

    // interactive UI: client gives commands to server via named pipe, which
    // the server creates and diposes of
    string pipe_path = fmt::format("/tmp/peer_pipe_{}", (*optConfig).Id);
   
    if (!exists(path(pipe_path)))
    {
        // Server Mode, pipe does not exist yet
        auto udpRxSocket = make_unique<UdpRxSocket>(std::string("127.0.0.1"), (*optConfig).udpPort);
        vector<unique_ptr<UdpTxSocket>> udpTxSockets;
        vector<ITxSocket *> txSockets;

        for (auto const &peer : (*optConfig).peers)
        {
            udpTxSockets.push_back(std::make_unique<UdpTxSocket>(peer));
            txSockets.push_back(udpTxSockets.back().get());
        }    

        App myApp(udpRxSocket.get(), txSockets, (*optConfig).logFile, pipe_path);
        myApp.run();
    }
    else
    {
        // Server already executes, client mode
        if (!(*optConfig).freeParams.empty())
        {
            int pipe = open(pipe_path.c_str(), O_WRONLY);
            if (pipe < 0)
            {
                // FIXME: Error handling
            }

            stringstream ss;
            for (auto const &freeParam : (*optConfig).freeParams)
            {
                ss << freeParam << " ";
            }
            ss << "\n";

            string cmd = ss.str();
            if (write(pipe, cmd.c_str(), cmd.length()) < 0)
            {
                // FIXME: Error handling
            }

            close(pipe);
        }
    }


    return 0;
}
