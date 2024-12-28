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
        auto now = std::chrono::system_clock::now();
        m_middleWare.rxTxLoop(now);
        usleep(100000); // Sleep for 100 milliseconds
    }
}


 
}