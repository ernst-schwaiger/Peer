#pragma once

#include <optional>
#include <vector>
#include <string>
#include <cstdint>

#include "CommonTypes.h"

namespace rgc {

typedef struct 
{
    peerId_t Id;
    std::string ipaddr;
    uint16_t udpPort;
    std::string logFile;
    std::string errorInjection;
    std::vector<peer_t> peers;
    std::vector<std::string> freeParams;
} config_t;

extern std::optional<config_t> getConfigFromOptions(int argc, char *argv[]);
extern void printUsage(char *argv0);
}

