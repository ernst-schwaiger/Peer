#include "App.h"
#include "MiddleWare.h"

using namespace std;

namespace rgc
{

App::App(config_t const &config, IRxSocket *pRxSocket, vector<ITxSocket *> &txSockets) :
    m_middleWare(config, this, pRxSocket, txSockets)
{

}

App::~App()
{

}

void App::deliverMessage(MessageId msgId, payload_t const &payload) const
{
    // FIXME
}

void App::run()
{
    m_middleWare.rxTxLoop();
}


 
}