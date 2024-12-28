#pragma once

#include <vector>

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
    App(IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets, std::string const &logFile);
    virtual ~App();
    virtual void deliverMessage(MessageId msgId, payload_t const &payload) const;
    virtual void run();
    virtual void log(LOG_TYPE, std::string const &msg);

private:
    MiddleWare m_middleWare;
    Logger m_logger;
};

}

