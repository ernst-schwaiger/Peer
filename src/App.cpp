#include <unistd.h>
#include <assert.h>

#include "App.h"
#include "MiddleWare.h"

using namespace std;

namespace rgc
{

App::App(IRxSocket *pRxSocket, vector<ITxSocket *> &txSockets, std::string const &logFile) :
    m_middleWare(this, pRxSocket, txSockets),
    m_logger(logFile)
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

void App::log(LOG_TYPE type, std::string const &msg)
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

 
}