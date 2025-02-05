#include <filesystem>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>

#include "ConfigParser.h"

using namespace rgc;
using namespace std;
using namespace std::filesystem;

static constexpr peerId_t INVALID_PEER_ID = UINT16_MAX;
static constexpr uint16_t INVALID_PORT_NUM = UINT16_MAX;
static constexpr uint32_t INVALID_BIT_FLIP = UINT32_MAX;
static constexpr peerId_t DEFAULT_PEER_ID = 1;
static constexpr uint16_t DEFAULT_PORT_NUM = 4201;
static constexpr char const * DEFAULT_IP_ADDRESS = "127.0.0.1";
static constexpr in_addr_t DEFAULT_IP = (1 << 24) + 127; // same address as above, represented as octet stream
static constexpr char const * DEFAULT_CONFIG_FILE = "./peer.cfg";
static constexpr char SEPARATOR_CONFIG_FILE = ',';
static constexpr char SEPARATOR_BIT_FLIP = ':';
static constexpr char COMMENT_TOKEN_CONFIG_FILE = '#';

template<typename T>
static T safeStrToI(char const *str, T defaultValue)
{
    T val;
    stringstream ss;
    ss << str;
    ss >> val;
    return (ss.eof()) ? val : defaultValue;
}

static optional<peer_t> parseLine(string const &line)
{
    optional<peer_t> ret;
    string field;
    peer_t tmp = { INVALID_PEER_ID, INVALID_PORT_NUM, 0 };

    if ((line.size() > 0) && (line[0] != COMMENT_TOKEN_CONFIG_FILE))
    {
        stringstream ss(line);
        getline(ss, field, SEPARATOR_CONFIG_FILE);
        tmp.peerId = safeStrToI(field.c_str(), INVALID_PEER_ID);
        getline(ss, field, SEPARATOR_CONFIG_FILE);

        in_addr tmpAddr;
        if (inet_aton(field.c_str(), &tmpAddr) < 0)
        {
            cerr << "Detected invalid IP V4 address: " << field << ".\n";
            return ret;
        }
        tmp.peerIpAddress = tmpAddr.s_addr;

        getline(ss, field, SEPARATOR_CONFIG_FILE);
        tmp.peerUdpPort = safeStrToI(field.c_str(), INVALID_PORT_NUM);
        ret = tmp;
    }

    return ret;
}

static bool isValidPeerId(peerId_t peerId)
{
    return (peerId != INVALID_PEER_ID);
}

static bool isValidUdpPort(uint16_t udpPort)
{
    return ((udpPort > 1024) && (udpPort != INVALID_PORT_NUM));
}

static optional<bitflip_t> getBitFlipInfo(string const &bitFlip)
{
    optional<bitflip_t> ret = std::nullopt;
    string peerId;
    stringstream ss(bitFlip);
    getline(ss, peerId, SEPARATOR_BIT_FLIP);
    uint32_t intPeerId = safeStrToI(peerId.c_str(), INVALID_BIT_FLIP);

    string msgIdx;
    getline(ss, msgIdx, SEPARATOR_BIT_FLIP);
    uint32_t intMsgIdx = safeStrToI(msgIdx.c_str(), INVALID_BIT_FLIP);

    string bitOffset;
    getline(ss, bitOffset, SEPARATOR_BIT_FLIP);
    uint32_t intBitOffset = safeStrToI(bitOffset.c_str(), INVALID_BIT_FLIP);

    if (((intPeerId != INVALID_BIT_FLIP) && (intMsgIdx != INVALID_BIT_FLIP) && (intBitOffset != INVALID_BIT_FLIP)))
    {
        // extra ugly code for MACs :-)
        bitflip_t tmp;
        tmp.peerId = static_cast<peerId_t>(intPeerId);
        tmp.seqNrId = static_cast<seqNr_t>(intMsgIdx);
        tmp.bitOffset = static_cast<uint16_t>(intBitOffset);
        ret = make_optional<bitflip_t>(tmp);
    }

    return ret;
}

static bool isValid(peer_t const &peer, vector<peer_t> const &otherPeers)
{
    bool ret = (isValidPeerId(peer.peerId) && isValidUdpPort(peer.peerUdpPort));

    if (ret)
    {
        auto it = find_if(begin(otherPeers), end(otherPeers), 
            [peer](auto const &other) 
            { 
                return other.peerId == peer.peerId;
            }
        );

        if (it != end(otherPeers))
        {
            cerr << "Found duplicate Peer ID: " << peer.peerId << ".\n";
            ret = false;
        }
        else
        {
            auto it = find_if(begin(otherPeers), end(otherPeers), 
                [peer](auto const &other) 
                { 
                    return ((other.peerIpAddress == peer.peerIpAddress) && 
                            (other.peerUdpPort == peer.peerUdpPort));
                }
            );
            if (it != end(otherPeers))
            {
                cerr << "Found duplicate Ip Address/Udp Port entry: " << peer.peerIpAddress << "/" << peer.peerUdpPort << ".\n";
                ret = false;
            }
        }
    }

    return ret;
}

static vector<peer_t> readConfigFile(string const &configFilePath)
{
    vector<peer_t> ret; 
    string line;

    ifstream ifs(configFilePath);

    while (ifs.good() && !ifs.eof())
    {
        getline(ifs, line);
        optional<peer_t> optPeer = parseLine(line);
        if (optPeer.has_value() && isValid(*optPeer, ret))
        {
            ret.push_back(*optPeer);
        }
    }
    
    return ret;
}

std::optional<config_t> rgc::getConfigFromOptions(int argc, char *argv[])
{
    optional<config_t> ret;
    config_t parsed_values{ DEFAULT_PEER_ID, DEFAULT_IP_ADDRESS, DEFAULT_IP, DEFAULT_PORT_NUM, "", {}, {}, {}, std::nullopt };
    bool error = false;   
    int8_t c; // in contrast to Intel, char seems to be unsigned on ARM, int8_t works on both architectures
    string configFile = DEFAULT_CONFIG_FILE;

    while ((c = getopt (argc, argv, "i:a:p:c:l:e:")) != -1)
    {
        switch (c)
        {
        case 'i':
            parsed_values.Id = safeStrToI(optarg, INVALID_PEER_ID);
        break;
        case 'a':
            parsed_values.ipaddr_string = optarg;
        break;
        case 'p':
            parsed_values.udpPort = safeStrToI(optarg, INVALID_PORT_NUM);
        break;
        case 'c':
            configFile = optarg;
        break;
        case 'l':
            parsed_values.logFile = optarg;
        break;
        case 'e':
            parsed_values.errorInjection = optarg;
        break;
        case '?':
        {
            if (optopt == 'i' || optopt == 'a' || optopt == 'p' || optopt == 'c' || optopt == 'l' || optopt == 'e')
            {
                cerr << "Option -" << optopt << "requires an argument\n";
            }
            else if (isprint(optopt))
            {
                cerr << "Unknown option -" << optopt << ".\n";
            }
            else
            {
                cerr << "Unknown option character -" << static_cast<uint16_t>(optopt) << ".\n";
            }
            error = true;
            break;
        }
        default:
            cerr << "Unexpected error parsing command line arguments.\n";
            abort();
        }
    }

    // Additional parameters without 
    for (int i = optind; i < argc; i++)
    { 
        parsed_values.freeParams.push_back(argv[i]);
    }

    // parsing of parameters worked, check value ranges
    if (!error)
    {
        if (!isValidPeerId(parsed_values.Id))
        {
            cerr << "Peer ID must be in the range [0.." << (INVALID_PEER_ID - 1) << "]\n";
            error = true;
        }

        if (!isValidUdpPort(parsed_values.udpPort))
        {
            cerr << "Udp port number must be in the range [1025.." << (INVALID_PORT_NUM - 1) << "]\n";
            error = true;
        }

        in_addr tmpAddr;
        if (inet_aton(parsed_values.ipaddr_string.c_str(), &tmpAddr) < 0)
        {
            cerr << "Detected invalid IP V4 address: " << parsed_values.ipaddr_string.c_str() << ".\n";
            return ret;
        }
        parsed_values.ipaddr = tmpAddr.s_addr;

        if (parsed_values.logFile.length())
        {
            path logFilePath(parsed_values.logFile);
            path logFileFolderPath = logFilePath.parent_path();
            if (!exists(logFileFolderPath) || !is_directory(logFileFolderPath))
            {
                cerr << parsed_values.logFile << " must be in a folder which already exists.\n";
                error = true;
            }
        }


        if (!parsed_values.errorInjection.empty())
        {
            auto bitFlipInfo = getBitFlipInfo(parsed_values.errorInjection);

            if (!bitFlipInfo.has_value())
            {
                cerr << "Invalid bit flip configuration detected: " << parsed_values.errorInjection.c_str() << ".\n";
                return ret;            
            }
            else
            {
                parsed_values.bitFlipInfo = bitFlipInfo;
            }
        }

        path cfgFilePath(configFile);
        if (!exists(cfgFilePath) || !is_regular_file(cfgFilePath))
        {
            cerr << configFile << " must be a valid regular file\n";
            error = true;
        }
        else
        {
            parsed_values.peers = readConfigFile(configFile);
        }
    }

    if (!error)
    {
        ret = parsed_values;
    }

    return ret;
}

void rgc::printUsage(char *argv0)
{
    cerr << "Usage: " << argv0 << " [-i <peerId>] [-a <ipaddr>] [-p <udpPort>] [-c <configFile>] [-l <logFile>]\n";
    cerr << "   <peerId>        unique peer id in the range [0.." << INVALID_PEER_ID - 1 << "], default is " << DEFAULT_PEER_ID <<".\n";
    cerr << "   <ipaddr>        local IPV4 address, default is " << DEFAULT_IP_ADDRESS <<".\n";
    cerr << "   <udpPort>       local udp port in the range [1025.." << INVALID_PORT_NUM - 1 << "], default is " << DEFAULT_PORT_NUM << ".\n";
    cerr << "   <configFile>    path to an already existing configuration file, default is " << DEFAULT_CONFIG_FILE << ".\n";
    cerr << "   <logFile>       path to log file. If none is provided stdout/stderr is used.\n";
}

