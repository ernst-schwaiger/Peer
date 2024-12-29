#pragma once

#include <vector>
#include <string>

#include "IApp.h"
#include "ISocket.h"

#include "ConfigParser.h"
#include "MiddleWare.h"
#include "Logger.h"

namespace rgc
{

class App : public IApp
{
public:
    App(IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets, std::string const &logFile, std::string const &pipe_path);
    virtual ~App();
    virtual void deliverMessage(MessageId msgId, payload_t const &payload) const;
    virtual void run();
    virtual void log(LOG_TYPE, std::string const &msg);

private:
    void processPendingUserCommands();
    std::string getNextUserCommand();

    MiddleWare m_middleWare;
    Logger m_logger;
    std::string m_pipe_path;
    int m_pipe;
    std::vector<char> userCmdBuf;
    bool m_stop;
};

}

