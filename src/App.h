#pragma once

#include <vector>

#include "IApp.h"
#include "ISocket.h"

#include "ConfigParser.h"
#include "MiddleWare.h"

namespace rgc
{

class App : public IApp
{
public:
    App(IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets);
    virtual ~App();
    virtual void deliverMessage(MessageId msgId, payload_t const &payload) const;
    virtual void run();

private:
    MiddleWare m_middleWare;
};

}

