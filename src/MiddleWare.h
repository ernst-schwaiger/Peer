#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

#include "CommonTypes.h"
#include "ConfigParser.h"

#include "ISocket.h"
#include "IApp.h"

namespace rgc {

// FIXME: Proper constexpr
#define MAX_TX_ATTEMPTS (3U)

class MiddleWare;

class TxState
{
public:
    explicit TxState(rgc::ITxSocket *pTxSocket) : 
        m_timeout(std::chrono::system_clock::now()),
        m_pTxSocket(pTxSocket), 
        m_TxAttemptsRemaining(MAX_TX_ATTEMPTS), 
        m_txAcknowledged(false)
    {}

    rgc::ITxSocket const *getSocket() const
    {
        return m_pTxSocket;
    }

    void setAcknowledged()
    {
        m_txAcknowledged = true; 
    }

    bool isAcknowledged() const
    {
        return m_txAcknowledged;
    }

    bool isTimeoutElapsed(std::chrono::system_clock::time_point now) const
    {
        return m_timeout <= now;
    }

    void setTimeout(std::chrono::system_clock::time_point &timeout)
    {
        m_timeout = timeout;
    }

    uint8_t getRemainingTxAttempts() const
    {
        return m_TxAttemptsRemaining;
    }

    void setRemainingTxAttempts(uint8_t remainingTxAttempts)
    {
        m_TxAttemptsRemaining = remainingTxAttempts;
    }

private:
    std::chrono::system_clock::time_point m_timeout;
    rgc::ITxSocket *m_pTxSocket;
    uint8_t m_TxAttemptsRemaining;
    bool m_txAcknowledged;
};

class TxMessageState
{
public:
    TxMessageState(MessageId msgId, std::vector<ITxSocket *> txSockets, rgc::payload_t &payload) :
        m_msgId(msgId),
        m_payload(payload)
    {
        for (ITxSocket *pTxSocket: txSockets)
        {
            m_txStates.emplace_back(TxState(pTxSocket));
        }
    }

    MessageId const &getMsgId() const
    {
        return m_msgId;
    }

    TxState *findTxState(struct sockaddr_in const &remoteSockAddr)
    {
        auto it = std::find_if(begin(m_txStates), end(m_txStates),
            [&remoteSockAddr](auto const &txState)
            {
                struct sockaddr_in const &sockAddr = txState.getSocket()->getRemoteSocketAddr();
                return (
                        (sockAddr.sin_addr.s_addr == remoteSockAddr.sin_addr.s_addr) &&
                        (sockAddr.sin_family == remoteSockAddr.sin_family) &&
                        (sockAddr.sin_port == remoteSockAddr.sin_port)
                    );
            });

        return (it == end(m_txStates)) ? nullptr : &m_txStates[std::distance(begin(m_txStates), it)];
    }

    rgc::payload_t const &getPayload() const
    {
        return m_payload;
    }

    std::vector<TxState> &getTxStates()
    {
        return m_txStates;
    }

private:
    MessageId m_msgId;
    rgc::payload_t m_payload;
    std::vector<TxState> m_txStates;
};

class MiddleWare
{
public:
    MiddleWare(config_t const &config, rgc::IApp *pApp, rgc::IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets) : 
        m_config(config),
        m_pApp(pApp),
        m_pRxSocket(pRxSocket),
        m_txSockets(txSockets),
        m_stop(false)
    {
    }

    void rxTxLoop();

    void doSend();

private:

    void listenRxSocket();
    void checkPendingTxMessages();
    void checkPendingCommands();
    void processTxMessage(TxState &txState, std::vector<char> const &msg, std::chrono::system_clock::time_point const &now);
    void processRxMessage(rgc::payload_t &payload, struct sockaddr_in const &remoteSockAddr);

    TxMessageState *findTxMsgState(MessageId const &msgId)
    {
        auto it = std::find_if(begin(m_txMessageStates), end(m_txMessageStates), 
            [&msgId](auto const &other) 
            { 
                return (other.getMsgId() == msgId);
            }
        );

        return (it == end(m_txMessageStates)) ? nullptr : &m_txMessageStates[std::distance(begin(m_txMessageStates), it)];
    }


    config_t const &m_config;
    rgc::IApp *m_pApp;
    rgc::IRxSocket *m_pRxSocket;
    std::vector<ITxSocket *> &m_txSockets;
    std::vector<TxMessageState> m_txMessageStates;

    bool m_stop;
};

}
