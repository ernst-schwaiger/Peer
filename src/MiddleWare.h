#pragma once

#include <vector>
#include <forward_list>
#include <chrono>
#include <algorithm>
#include <cctype>

#include "CommonTypes.h"
#include "ConfigParser.h"

#include "ISocket.h"
#include "IApp.h"

namespace rgc {

static constexpr uint8_t MAX_TX_ATTEMPTS = 3;

class MiddleWare;

class TxState final
{
public:
    explicit TxState(rgc::ITxSocket *pTxSocket, std::chrono::system_clock::time_point now) : 
        m_timeout(now),
        m_pTxSocket(pTxSocket), 
        m_remainingTxAttempts(MAX_TX_ATTEMPTS), 
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
        bool ret = (m_timeout <= now);
        return ret;
    }

    bool alreadySent() const
    {
        return (getRemainingTxAttempts() < MAX_TX_ATTEMPTS);
    }

    void setTimeout(std::chrono::system_clock::time_point &timeout)
    {
        m_timeout = timeout;
    }

    uint8_t getRemainingTxAttempts() const
    {
        return m_remainingTxAttempts;
    }

    void setRemainingTxAttempts(uint8_t remainingTxAttempts)
    {
        m_remainingTxAttempts = remainingTxAttempts;
    }

private:
    std::chrono::system_clock::time_point m_timeout;
    rgc::ITxSocket *m_pTxSocket;
    uint8_t m_remainingTxAttempts;
    bool m_txAcknowledged;
};

class TxMessageState final
{
public:
    TxMessageState(MessageId msgId, std::vector<ITxSocket *> txSockets, rgc::payload_t const &payload, std::chrono::system_clock::time_point now) :
        m_msgId(msgId),
        m_payload(payload)
    {
        std::chrono::system_clock::time_point sendTime = now;
        std::chrono::duration<int64_t, std::milli> tx_client_delay = std::chrono::milliseconds(1000);

        for (ITxSocket *pTxSocket: txSockets)
        {
            m_txStates.emplace_back(pTxSocket, sendTime);
            // See: "Zwischen den Sendevorgaengen an unterschiedliche Peers soll dabei eine konstante Wartezeit von 
            // 1 Sekunde abgewartet werden, um den Ausfall des Sende-Peers waehrend der Uebertragung testen zu koennen"
            sendTime += tx_client_delay; 
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
                        txState.alreadySent() && // paranoia check: has the message of the ACK already been sent?
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

class MiddleWare final
{
public:
    MiddleWare(rgc::IApp *pApp, rgc::IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets) : 
        m_pApp(pApp),
        m_pRxSocket(pRxSocket),
        m_txSockets(txSockets)
    {
        for (auto const &txSocket : txSockets)
        {
            m_nextSeqNrs.push_back({txSocket->getPeerId(), 0});
        }
    }

    void rxTxLoop(std::chrono::system_clock::time_point const &now);

    static bool verifyChecksum(uint8_t const *pl, size_t size);
    static checksum_t rfc1071Checksum(uint8_t const *pl, size_t size);

private:
    typedef struct
    {
        peerId_t peerId;
        seqNr_t nextSeqNr;
    } nextSeqNr_t;


    void listenRxSocket(std::chrono::system_clock::time_point const &now);
    void checkPendingTxMessages(std::chrono::system_clock::time_point const &now);
    void checkPendingCommands(std::chrono::system_clock::time_point const &now);
    void processTxMessage(TxState &txState, payload_t const &msg, std::chrono::system_clock::time_point const &now);
    void processRxMessage(rgc::payload_t const &payload, struct sockaddr_in const &remoteSockAddr, std::chrono::system_clock::time_point const &now);
    void processRxAckMessage(rgc::payload_t const &payload, peerId_t peerId, struct sockaddr_in const &remoteSockAddr);
    void processRxDataMessage(rgc::payload_t const &payload, peerId_t peerId, ITxSocket *txSocket, std::chrono::system_clock::time_point const &now);
    rgc::payload_t makeAckMessage(rgc::payload_t const &dataMessage) const;
    

    ITxSocket *getTxSocketForRemoteAddress(struct sockaddr_in const &remoteSockAddr) const;
    bool isPeerSupported(peerId_t peerId) const;
    bool isSeqNrOfPeerAccepted(peerId_t peerId, seqNr_t seqNr) const;
    void setAcceptedSeqNrOfPeer(peerId_t peerId, seqNr_t seqNr);

    static std::string toString(struct sockaddr_in const &sockAddr);
    static std::string toString(rgc::payload_t const &payload);

    void injectError(rgc::payload_t &payload) const;

    TxMessageState *findTxMsgState(MessageId const &msgId)
    {
        auto it = std::find_if(begin(m_txMessageStates), end(m_txMessageStates), 
            [&msgId](auto const &other) 
            { 
                return (other.getMsgId() == msgId);
            }
        );

        return (it == end(m_txMessageStates)) ? nullptr : &(*it);
    }

    rgc::IApp *m_pApp;
    rgc::IRxSocket *m_pRxSocket;
    std::vector<ITxSocket *> &m_txSockets;
    std::vector<nextSeqNr_t> m_nextSeqNrs;

    std::forward_list<TxMessageState> m_txMessageStates;
};

}
