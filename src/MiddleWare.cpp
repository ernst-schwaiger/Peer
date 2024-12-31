#include <numeric>
#include <fmt/core.h>
#include <sstream>
#include <iomanip>

#include "MiddleWare.h"

using namespace std;
using namespace rgc;
using namespace std::chrono;

static constexpr duration<int64_t, std::milli> ACK_TIMEOUT = milliseconds(1000);

void MiddleWare::rxTxLoop(system_clock::time_point const &now)
{
    listenRxSocket(now);
    checkPendingTxMessages(now);
}

void MiddleWare::sendMessage(string &message, system_clock::time_point const &now)
{
    MessageId msgId = MessageId(m_ownPeerId, m_nextSeqNr);
    payload_t payload;
    payload.reserve(message.length() + 6);
    payload.push_back(m_ownPeerId >> 8);
    payload.push_back(m_ownPeerId & 0xff);
    payload.push_back(m_nextSeqNr >> 8);
    payload.push_back(m_nextSeqNr & 0xff);
    payload.insert(end(payload), begin(message), end(message));
    checksum_t checksum = rfc1071Checksum(payload.data(), payload.size());
    payload.push_back(checksum >> 8);
    payload.push_back(checksum & 0xff);

    m_txMessageStates.emplace_front(msgId, m_txSockets, payload, now);
    ++m_nextSeqNr;
}

void MiddleWare::listenRxSocket(system_clock::time_point const &now)
{
    rx_buffer_t buf;
    struct sockaddr_in remoteSockAddr;
    
    // Polling for incoming data until there is nothing left to receive or an error happens 
    for (;;)
    {
        rgc::TransmitStatus status = m_pRxSocket->receive(buf, remoteSockAddr);

        if (status.status != 0)
        {
             m_pApp->log(IApp::LOG_TYPE::ERR, fmt::format("Error reading from Rx Socket, error code: {}", status.status));
        }
        else
        {
            if (status.transmitBytes > 0)
            {
                payload_t payload(&buf[0], &buf[status.transmitBytes]);
                injectError(payload);
                processRxMessage(payload, remoteSockAddr, now);
            }
        }

        if ((status.transmitBytes == 0) || (status.status != 0))
        {
            break;
        }
    }
}

void MiddleWare::injectError(rgc::payload_t &payload) const
{
    // FIXME: Implement this
}

void MiddleWare::checkPendingTxMessages(system_clock::time_point const &now)
{
    auto itBefore = m_txMessageStates.before_begin();
    for (auto it = begin(m_txMessageStates); it != end(m_txMessageStates); ++it)
    {
        auto &txMsgState = *it;

        vector<TxState> &txStates = txMsgState.getTxStates();
        for (auto &txState : txStates)
        {
            if (!txState.isAcknowledged() && txState.isTimeoutElapsed(now))
            {
                processTxMessage(txState, txMsgState.getPayload(), now);
            }
        }

        bool allAck = accumulate(begin(txStates), end(txStates), true, 
            [](bool acc, auto &e)
            {
                return (acc && e.isAcknowledged());
            });

        if (allAck)
        {
            // Deliver message to app
            m_pApp->deliverMessage(txMsgState.getMsgId(), txMsgState.getPayload());
            // Dispose of this element
            m_txMessageStates.erase_after(itBefore);
            // Step back to previous element, ours is invalid now
            it = itBefore;
        }

        itBefore = it;
    }
}

void MiddleWare::processTxMessage(TxState &txState, payload_t const &msg, system_clock::time_point const &now)
{
    uint8_t remainingTxAttempts = txState.getRemainingTxAttempts();
    if (remainingTxAttempts == 0)
    {
        // we did not get an ACK after the third tx attempt. we handle this case
        // as if it got an ACK
        m_pApp->log(IApp::LOG_TYPE::WARN, "Got no ACK after max number of retries, giving up.");
        txState.setAcknowledged();
    }
    else
    {
        auto result = txState.getSocket()->send(msg);
        auto const &remoteSockAddr = txState.getSocket()->getRemoteSocketAddr();
        if (result.status != 0)
        {
            m_pApp->log(IApp::LOG_TYPE::ERR, fmt::format("Failed to send message {} to {}.", toString(msg), toString(remoteSockAddr)));
        }
        else
        {
            m_pApp->log(IApp::LOG_TYPE::MSG, fmt::format("Sending message {} to {}.", toString(msg), toString(remoteSockAddr)));
        }

        system_clock::time_point timeout = now + ACK_TIMEOUT;
        txState.setTimeout(timeout);
        txState.setRemainingTxAttempts(remainingTxAttempts - 1);
    }
}

void MiddleWare::processRxMessage(rgc::payload_t const &payload, struct sockaddr_in const &remoteSockAddr, system_clock::time_point const &now)
{
    ITxSocket *txSocket = getTxSocketForRemoteAddress(remoteSockAddr);

    if (txSocket == nullptr)
    {
        m_pApp->log(IApp::LOG_TYPE::WARN, fmt::format("Discarding rx message from unknown IP/Port: {}.", toString(remoteSockAddr)));
        return;
    }

    // Truncated frame, discard
    if (payload.size() < sizeof(peerId_t) + sizeof(seqNr_t) + sizeof(checksum_t))
    {
        m_pApp->log(IApp::LOG_TYPE::WARN, "Discarding rx message: Truncated.");
        return;
    }

    // Checksum error, discard
    if (!verifyChecksum(payload.data(), payload.size()))
    {
        m_pApp->log(IApp::LOG_TYPE::WARN, "Discarding rx message: Checksum error.");
        return;
    }

    peerId_t peerId = (payload[0] << 8) + payload[1];
    if (!isPeerSupported(peerId))
    {
        m_pApp->log(IApp::LOG_TYPE::WARN, fmt::format("Discarding rx message: Unknown peer id: {}", peerId));
        return;
    }

    bool isAckMessage = (payload.size() == sizeof(peerId_t) + sizeof(seqNr_t) + sizeof(checksum_t));

    if (isAckMessage)
    {
        processRxAckMessage(payload, peerId, remoteSockAddr);
    }
    else
    {
        processRxDataMessage(payload, peerId, txSocket, now);
    }
}

void MiddleWare::processRxAckMessage(rgc::payload_t const &payload, peerId_t peerId, struct sockaddr_in const &remoteSockAddr)
{
    seqNr_t seqNr = (payload[2] << 8) + payload[3];
    MessageId msgId = MessageId(peerId, seqNr);
    TxMessageState *txMsgState = findTxMsgState(msgId);

    if (txMsgState != nullptr)
    {
        // Message found, check content
        TxState *txState = txMsgState->findTxState(remoteSockAddr);
        if (txState != nullptr)
        {
            txState->setAcknowledged();
            m_pApp->log(IApp::LOG_TYPE::MSG, fmt::format("Received ACK for sent message {} from {}.", toString(payload), toString(remoteSockAddr)));
        }
    }
}

void MiddleWare::processRxDataMessage(rgc::payload_t const &payload, peerId_t peerId, ITxSocket *txSocket, system_clock::time_point const &now)
{
    // Send back an ACK in any case, even if we already delivered that message to the app
    txSocket->send(makeAckMessage(payload));

    seqNr_t seqNr = (payload[2] << 8) + payload[3];
    struct sockaddr_in const &remoteSockAddr = txSocket->getRemoteSocketAddr();

    if (!isSeqNrOfPeerAccepted(peerId, seqNr))
    {
        m_pApp->log(IApp::LOG_TYPE::MSG, fmt::format("Discarding message due to SeqNr: {} from {}.", toString(payload), toString(remoteSockAddr)));
        return;
    }

    MessageId msgId = MessageId(peerId, seqNr);
    TxMessageState *txMsgState = findTxMsgState(msgId);

    if (txMsgState == nullptr)
    {
        // No such message found in the state, set up anew
        m_pApp->log(IApp::LOG_TYPE::MSG, fmt::format("Received data message {} from {}.", toString(payload), toString(remoteSockAddr)));
        m_txMessageStates.emplace_front(msgId, m_txSockets, payload, now);
        setAcceptedSeqNrOfPeer(peerId, seqNr + 1);
    }
    else
    {
        // We have received that message already, ignore it here
        m_pApp->log(IApp::LOG_TYPE::MSG, fmt::format("Discarding already received message {} from {}.", toString(payload), toString(remoteSockAddr)));
    }
}

payload_t MiddleWare::makeAckMessage(payload_t const &dataMessage) const
{
    // Peer-Id
    payload_t ret(begin(dataMessage), begin(dataMessage) + sizeof(peerId_t) + sizeof(seqNr_t));
    checksum_t checksum = rfc1071Checksum(ret.data(), ret.size());
    ret.push_back(checksum >> 8);
    ret.push_back(checksum & 0xff);
    return ret;
}


bool MiddleWare::isPeerSupported(peerId_t peerId) const
{
    auto it = std::find_if(begin(m_txSockets), end(m_txSockets),
        [&peerId](auto const &txSocket) { return (txSocket->getPeerId() == peerId); });
    return (it != end(m_txSockets) || m_ownPeerId == peerId);
}

ITxSocket *MiddleWare::getTxSocketForRemoteAddress(struct sockaddr_in const &remoteSockAddr) const
{
    auto it = std::find_if(begin(m_txSockets), end(m_txSockets),
        [&remoteSockAddr](auto const *txSocket)
        {
            struct ::sockaddr_in const &addr = txSocket->getRemoteSocketAddr();
            return ( (remoteSockAddr.sin_addr.s_addr == addr.sin_addr.s_addr) &&
                     (remoteSockAddr.sin_family == addr.sin_family) &&
                     (remoteSockAddr.sin_port == addr.sin_port));
        }
    );

    return (it != end(m_txSockets)) ? *it : nullptr;
}


bool MiddleWare::isSeqNrOfPeerAccepted(peerId_t peerId, seqNr_t seqNr) const
{
    bool ret = false;
    auto it = std::find_if(begin(m_nextSeqNrs), end(m_nextSeqNrs),
        [&peerId](auto const &nsn) { return (nsn.peerId == peerId); });

    if (it != end(m_nextSeqNrs))
    {
        auto diff = (it->nextSeqNr <= seqNr) ?
            seqNr - it->nextSeqNr : 
            seqNr + (1 << (sizeof(seqNr_t) * 8)) - it->nextSeqNr;

        ret = (diff < 10);
    }

    return ret;
}

void MiddleWare::setAcceptedSeqNrOfPeer(peerId_t peerId, seqNr_t seqNr)
{
    auto it = std::find_if(begin(m_nextSeqNrs), end(m_nextSeqNrs),
        [&peerId](auto const &nsn) { return (nsn.peerId == peerId); });

    if (it != end(m_nextSeqNrs))
    {
        it->nextSeqNr = seqNr;
    }
}

std::string MiddleWare::toString(struct sockaddr_in const &sockAddr)
{
    uint8_t const *pIP = reinterpret_cast<uint8_t const *>(&sockAddr.sin_addr.s_addr);
    uint8_t const *pPort = reinterpret_cast<uint8_t const *>(&sockAddr.sin_port);
    return fmt::format("{}.{}.{}.{}:{}", pIP[0], pIP[1], pIP[2], pIP[3], (pPort[0] << 8) + pPort[1]);
}

std::string MiddleWare::toString(rgc::payload_t const &payload)
{
    stringstream ss;

    if (payload.size() >= 4)
    {
        uint8_t const *pHeader = reinterpret_cast<uint8_t const *>(payload.data());
        ss << fmt::format("({},{})", (pHeader[0] << 8) + pHeader[1], (pHeader[2] << 8) + pHeader[3]);
    }

    // We have got data
    if (payload.size() > 4 + 2)
    {
        auto itStart = begin(payload) + 4;
        auto itEnd = end(payload) - 2;

        bool isPrintable = std::accumulate(itStart, itEnd, true, [](bool a, auto const &el) { return (a && (std::isprint(el))); });
        ss << " - ";

        if (isPrintable)
        {
            ss << "\"" << string(itStart, itEnd) << "\"";
        }
        else
        {
            size_t count = 0;
           
            for (auto it = itStart; it < itEnd; ++it)
            {
                ss << "0x" << std::setw(2) << std::hex << std::setfill('0') << static_cast<uint16_t>(*it);
                ss << ((it + 1 < itEnd) ? "," : "");
                ss << (((++count) % 8 == 0) ? "\n" : " "); 
            }
        }
    }

    if (payload.size() >= 4 + 2)
    {
        uint8_t const *pCRC = reinterpret_cast<uint8_t const *>(&(payload.data()[payload.size() - 2]));
        ss << " - (0x" << std::setw(4) << std::hex << std::setfill('0') << static_cast<uint16_t>((pCRC[0] << 8) + pCRC[1]) << ")";
    }
    return ss.str();
}


bool MiddleWare::verifyChecksum(uint8_t const *pl, size_t size)
{
    checksum_t checksum = (pl[size - 2] << 8) + pl[size - 1];
    checksum_t calcChecksum = rfc1071Checksum(pl, size);
    return (calcChecksum == checksum);
}

checksum_t MiddleWare::rfc1071Checksum(uint8_t const *pl, size_t size)
{
    // FIXME: Implement this
    return 0xaffe;
}
