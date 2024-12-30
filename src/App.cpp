#include <vector>
#include <sstream>
#include <filesystem>
#include <stdexcept>

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <fmt/core.h>

#include "App.h"
#include "MiddleWare.h"

using namespace std;
using namespace std::filesystem;

static constexpr char SEPARATOR_COMMAND = ' ';

namespace rgc
{

App::App(peerId_t ownPeerId, IRxSocket *pRxSocket, vector<ITxSocket *> &txSockets, std::string const &logFile, string const &pipe_path) :
    m_middleWare(this, ownPeerId, pRxSocket, txSockets),
    m_logger(Logger::makeLogger(logFile)),
    m_pipe_path(pipe_path),
    m_stop(false)
{
    if (!exists(path(pipe_path)))
    {
        if (mkfifo(pipe_path.c_str(), 0666) != 0)
        {
            throw std::runtime_error(fmt::format("Could not create named pipe {}", pipe_path));
        }
    }   

    m_pipe = open(m_pipe_path.c_str(), O_RDONLY | O_NONBLOCK);
    
    if (m_pipe < 0)
    {
        throw std::runtime_error(fmt::format("Could not open named pipe {}", pipe_path));
    }
}

App::~App()
{
    close(m_pipe);
    unlink(m_pipe_path.c_str());
}

void App::deliverMessage(MessageId msgId, payload_t const &payload) const
{
    log(LOG_TYPE::MSG,
        fmt::format("Delivered message {} to application layer.", MiddleWare::toString(payload)));
}

void App::run()
{
    for (;;)
    {
        auto now = std::chrono::system_clock::now();

        m_middleWare.rxTxLoop(now);
        processPendingUserCommands();

        // stop our peer
        if (m_stop)
        {
            break;
        }

        usleep(100000); // Sleep for 100 milliseconds
    }
}

void App::log(LOG_TYPE type, std::string const &msg) const
{
    auto now = std::chrono::system_clock::now();

    switch(type)
    {
        case LOG_TYPE::DEBUG:
            m_logger.logDebug(msg, now);
            break;
        case LOG_TYPE::ERR:
            m_logger.logErr(msg, now);
            break;
        case LOG_TYPE::WARN:
            m_logger.logWarn(msg, now);
            break;
        case LOG_TYPE::MSG:
            m_logger.logMsg(msg, now);
            break;
        default:
            assert(false);
    }
}

void App::processPendingUserCommands()
{
    for (;;)
    {
        string command = getNextUserCommand();
        if (command.length() == 0)
        {
            break;
        }

        log(IApp::LOG_TYPE::DEBUG, command);

        stringstream ss(command);
        string command_type;
        getline(ss, command_type, SEPARATOR_COMMAND);
        string command_arg1;
        getline(ss, command_arg1, SEPARATOR_COMMAND);
        string command_arg2;
        getline(ss, command_arg2, SEPARATOR_COMMAND);

        if (command_type == "stop")
        {
            m_stop = true;
        }
        else if (command_type == "send")
        {
            if (!command_arg1.empty())
            {
                m_middleWare.sendMessage(command_arg1, std::chrono::system_clock::now());
            }
            else
            {
                log(IApp::LOG_TYPE::WARN, fmt::format("Command: {} requires a string as argument", command_type));
            }

        }
        else if (command_type == "inject")
        {
            // FIXME: Implement
        }
        else
        {
            log(IApp::LOG_TYPE::WARN, fmt::format("Unknown command: {}, ignoring", command));
        }
    }
}

string App::getNextUserCommand()
{
    string ret = "";

    char buf[80];
    // Append whatever came through the pipe
    for (;;)
    {
        ssize_t bytesRead = read(m_pipe, buf, sizeof(buf));
        if (bytesRead > 0)
        {
            userCmdBuf.insert(end(userCmdBuf), &buf[0], &buf[bytesRead]);
        }
        else
        {
            break;
        }
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        log(IApp::LOG_TYPE::ERR, fmt::format("Error reading from named pipe {}.", m_pipe_path));
    }

    auto it = find(begin(userCmdBuf), end(userCmdBuf), '\n');

    if (it != end(userCmdBuf))
    {
        // We have a new command
        ret = string(begin(userCmdBuf), it); // without newline
        userCmdBuf.erase(begin(userCmdBuf), it + 1); // with newline

    }

    return ret;
}

 
}