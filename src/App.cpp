#include <unistd.h>
#include "App.h"
#include "MiddleWare.h"

using namespace std;

namespace rgc
{

App::App(IRxSocket *pRxSocket, vector<ITxSocket *> &txSockets) :
    m_middleWare(this, pRxSocket, txSockets)
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
    for (;;)
    {
        m_middleWare.rxTxLoop();
        usleep(100000); // Sleep for 100 milliseconds
    }
}


 
}