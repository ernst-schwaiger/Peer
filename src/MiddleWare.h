#pragma once
#include <numeric>
#include <vector>
#include <list>
#include <optional>
#include <chrono>
#include <algorithm>
#include <cctype>

#include "CommonTypes.h"
#include "ConfigParser.h"

#include "ISocket.h"
#include "IApp.h"

namespace rgc {

static constexpr uint8_t MAX_TX_ATTEMPTS = 4;

class MiddleWare;

// Represents the state of an outgoing message w.r.t. one specific sender
class TxState final
{
public:
    explicit TxState(rgc::ITxSocket *pTxSocket, std::chrono::system_clock::time_point now) : 
        m_timeout(now),
        m_pTxSocket(pTxSocket), 
        m_remainingTxAttempts(MAX_TX_ATTEMPTS), 
        m_txAcknowledged(false),
        m_txToSelfFailed(false)
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

    void setTxToSelfFailed()
    {
        m_txToSelfFailed = true;
    }

    bool isTxToSelfFailed() const
    {
        return m_txToSelfFailed;
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
    bool m_txToSelfFailed;
};

// Represents the overall state of an outgoing message
class TxMessageState final
{
public:
    TxMessageState(MessageId msgId, std::vector<ITxSocket *> const &txSockets, rgc::payload_t const &payload, std::chrono::system_clock::time_point now) :
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

    bool isAllAcknowledged() const
    {
        return std::accumulate(std::begin(m_txStates), std::end(m_txStates), true, 
            [](bool acc, auto &e)
            {
                return (acc && e.isAcknowledged());
            });
    }

    bool isTxToSelfFailed() const
    {
        return std::accumulate(std::begin(m_txStates), std::end(m_txStates), false, 
            [](bool acc, auto &e)
            {
                return (acc || e.isTxToSelfFailed());
            });
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
    MiddleWare(rgc::IApp *pApp, peerId_t ownPeerId, rgc::IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets, std::optional<bitflip_t> bitFlipInfo) : 
        m_pApp(pApp),
        m_ownPeerId(ownPeerId),
        m_nextSeqNr(0),
        m_pRxSocket(pRxSocket),
        m_txSockets(txSockets)
    {
        for (auto const &txSocket : txSockets)
        {
            m_nextSeqNrs.push_back({txSocket->getPeerId(), 0});
        }

        if (bitFlipInfo.has_value())
        {
            m_bitFlipInfos.emplace_back(*bitFlipInfo);
        }
    }

    void rxTxLoop(std::chrono::system_clock::time_point const &now);
    void sendMessage(std::string const &message, std::chrono::system_clock::time_point const &now);
    void addBitFlipInfo(bitflip_t const &bf)
    {
        m_bitFlipInfos.push_back(bf);
    }

    static bool verifyChecksum(uint8_t const *pl, size_t size);
    static checksum_t rfc1071Checksum(uint8_t const *pl, size_t size);
    static checksum_t checksumMethod(uint8_t const *pl, size_t size);

    static std::string toString(struct sockaddr_in const &sockAddr);
    static std::string toString(rgc::payload_t const &payload);
    static std::string toString(rgc::MessageId const &msgId);

private:
    typedef struct
    {
        peerId_t peerId;
        seqNr_t nextSeqNr;
    } nextSeqNr_t;

    void listenRxSocket(std::chrono::system_clock::time_point const &now);
    void checkPendingTxMessages(std::chrono::system_clock::time_point const &now);
    void processTxMessage(TxState &txState, payload_t const &msg, std::chrono::system_clock::time_point const &now);
    void processRxMessage(rgc::payload_t const &payload, struct sockaddr_in const &remoteSockAddr, std::chrono::system_clock::time_point const &now);
    void processRxAckMessage(rgc::payload_t const &payload, peerId_t peerId, struct sockaddr_in const &remoteSockAddr);
    void processRxDataMessage(rgc::payload_t const &payload, peerId_t peerId, ITxSocket *txSocket, std::chrono::system_clock::time_point const &now);
    rgc::payload_t makeAckMessage(rgc::payload_t const &dataMessage) const;
    

    ITxSocket *getTxSocketForRemoteAddress(struct sockaddr_in const &remoteSockAddr) const;
    bool isPeerSupported(peerId_t peerId) const;
    bool isSeqNrOfPeerAccepted(peerId_t peerId, seqNr_t seqNr) const;
    void setAcceptedSeqNrOfPeer(peerId_t peerId, seqNr_t seqNr);

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
    peerId_t m_ownPeerId;
    seqNr_t m_nextSeqNr;
    rgc::IRxSocket *m_pRxSocket;
    std::vector<ITxSocket *> &m_txSockets;
    std::vector<nextSeqNr_t> m_nextSeqNrs;
    std::vector<bitflip_t> m_bitFlipInfos;

    std::list<TxMessageState> m_txMessageStates;
};

}
