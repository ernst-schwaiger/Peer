#include <fmt/core.h>
#include <fmt/ranges.h>
#include <sstream>
#include <iomanip>

#include "MiddleWare.h"

using namespace std;
using namespace rgc;
using namespace std::chrono;

static constexpr size_t MSG_ID_SIZE = 4;
static constexpr size_t CRC_SIZE = 2;

static constexpr duration<int64_t, std::milli> ACK_TIMEOUT = milliseconds(1000);

void MiddleWare::rxTxLoop(system_clock::time_point const &now)
{
    listenRxSocket(now);
    checkPendingTxMessages(now);
}

void MiddleWare::sendMessage(string const &message, system_clock::time_point const &now)
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

    m_txMessageStates.emplace_back(msgId, m_txSockets, payload, now);
    ++m_nextSeqNr;

#if defined (PEER_SENDS_TO_ITSELF)
    setAcceptedSeqNrOfPeer(m_ownPeerId, m_nextSeqNr);
#endif    
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
    for (auto it = begin(m_bitFlipInfos); it != end(m_bitFlipInfos); ++it)
    {
        uint16_t peerIdtemp = (payload[0] << 8) + payload[1];
        uint16_t seqNrIdIdtemp = (payload[2] << 8) + payload[3];
        if (it->peerId == peerIdtemp && it->seqNrId == seqNrIdIdtemp && (it->bitOffset + 16 < static_cast<uint16_t>(payload.size()*8))){
            size_t bytePos = (it->bitOffset + 16) / 8;
            size_t bitPos  = (it->bitOffset + 16) % 8;
            payload[bytePos] ^= static_cast<unsigned char>(1 << bitPos);
        }
    }
}

void MiddleWare::checkPendingTxMessages(system_clock::time_point const &now)
{
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
    }

    if (!m_txMessageStates.empty())
    {
        auto deliver_if_acked = [&](const TxMessageState &s){ if (s.isAllAcknowledged()) { m_pApp->deliverMessage(s.getMsgId(), s.getPayload()); } };
        for_each(begin(m_txMessageStates), end(m_txMessageStates), deliver_if_acked);
        m_txMessageStates.erase(remove_if(begin(m_txMessageStates), end(m_txMessageStates), 
            [](const TxMessageState &s){ return s.isAllAcknowledged() || s.isTxToSelfFailed(); }), end(m_txMessageStates));
    }
}

void MiddleWare::processTxMessage(TxState &txState, payload_t const &msg, system_clock::time_point const &now)
{
    uint8_t remainingTxAttempts = txState.getRemainingTxAttempts();
    if (remainingTxAttempts == 0)
    {
        // we did not get an ACK after the third tx attempt
        m_pApp->log(IApp::LOG_TYPE::DEBUG, 
            fmt::format("Got no ACK for message {} after max number of retries, giving up.", toString(msg)));

        if (txState.getSocket()->getPeerId() == m_ownPeerId)
        {
            // This peer is broken since it can't propertly receive its own tx message -> Drop the message
            txState.setTxToSelfFailed();
        }
        else
        {
            // The remote peer is broken. We handle this as if we got an ACK
            txState.setAcknowledged();
        }
    }
    else
    {
        auto result = txState.getSocket()->send(msg);
        auto const &remoteSockAddr = txState.getSocket()->getRemoteSocketAddr();
        if (result.status != 0)
        {
            m_pApp->log(IApp::LOG_TYPE::ERR, 
                fmt::format("Failed to send message {} to {}; error code: {}", toString(msg), toString(remoteSockAddr), result.status));
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
        m_pApp->log(IApp::LOG_TYPE::WARN, fmt::format("Discarding rx message: Unknown IP/Port: {}.", toString(remoteSockAddr)));
        return;
    }

    // Truncated frame, discard
    if (payload.size() < MSG_ID_SIZE + CRC_SIZE)
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

    bool isAckMessage = (payload.size() == MSG_ID_SIZE + CRC_SIZE);

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
            m_pApp->log(IApp::LOG_TYPE::DEBUG, fmt::format("Received ACK for sent message {} from {}.", toString(payload), toString(remoteSockAddr)));
        }
    }
}

void MiddleWare::processRxDataMessage(rgc::payload_t const &payload, peerId_t peerId, ITxSocket *txSocket, system_clock::time_point const &now)
{
    // Send back an ACK in any case, even if we already delivered that message to the app
    struct sockaddr_in const &remoteSockAddr = txSocket->getRemoteSocketAddr();
    TransmitStatus txStatus = txSocket->send(makeAckMessage(payload));
    if (txStatus.status != 0)
    {
        m_pApp->log(IApp::LOG_TYPE::ERR, 
            fmt::format("Failed to send ACK for message: {} from {}; error code: {}.", toString(payload), toString(remoteSockAddr), txStatus.status));
    }

    seqNr_t seqNr = (payload[2] << 8) + payload[3];

    if (!isSeqNrOfPeerAccepted(peerId, seqNr))
    {
        m_pApp->log(IApp::LOG_TYPE::DEBUG, fmt::format("Discarding message due to SeqNr: {} from {}.", toString(payload), toString(remoteSockAddr)));
        return;
    }

    MessageId msgId = MessageId(peerId, seqNr);
    TxMessageState *txMsgState = findTxMsgState(msgId);

    if (txMsgState == nullptr)
    {
        // No such message found in the state, set up anew
        m_pApp->log(IApp::LOG_TYPE::DEBUG, fmt::format("Received data message {} from {}.", toString(payload), toString(remoteSockAddr)));
        m_txMessageStates.emplace_back(msgId, m_txSockets, payload, now);
        setAcceptedSeqNrOfPeer(peerId, seqNr + 1);
    }
    else
    {
        // We have received that message already, ignore it here
        m_pApp->log(IApp::LOG_TYPE::DEBUG, fmt::format("Discarding already received message {} from {}.", toString(payload), toString(remoteSockAddr)));
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

std::string MiddleWare::toString(rgc::MessageId const &msgId)
{
    return fmt::format("[{},{}]", msgId.getPeerId(), msgId.getSeqNr());
}

std::string MiddleWare::toString(rgc::payload_t const &payload)
{
    stringstream ss;

    if (payload.size() >= MSG_ID_SIZE)
    {
        uint8_t const *pHeader = payload.data();
        MessageId msgId((pHeader[0] << 8) + pHeader[1], (pHeader[2] << 8) + pHeader[3]);
        ss << toString(msgId);
    }

    // We have got data
    if (payload.size() > MSG_ID_SIZE + CRC_SIZE)
    {
        auto itStart = begin(payload) + MSG_ID_SIZE;
        auto itEnd = end(payload) - CRC_SIZE;

        bool isPrintable = std::accumulate(itStart, itEnd, true, [](bool a, auto const &el) { return (a && (std::isprint(el))); });
        ss << "[";
        if (isPrintable)
        {
            ss << "\"" << string(itStart, itEnd) << "\"";
        }
        else
        {
            ss << "0x";
            for (auto it = itStart; it < itEnd; it++)
            {
                ss << fmt::format("{:02x}", *it);
            }
        }
        ss << "]";
    }

    if (payload.size() >= MSG_ID_SIZE + CRC_SIZE)
    {
        uint8_t const *pCRC = &(payload.data()[payload.size() - 2]);
        ss << fmt::format("[0x{:04x}]", (pCRC[0] << 8) + pCRC[1]);
    }
    return ss.str();
}


bool MiddleWare::verifyChecksum(uint8_t const *pl, size_t size)
{
    checksum_t plChecksum = (pl[size - 2] << 8) + pl[size - 1];
    checksum_t sum = checksumMethod(pl, size - 2) + plChecksum;

    // If all 1s, checksum is valid
    return (sum == 0xFFFF); 
}

checksum_t MiddleWare::rfc1071Checksum(uint8_t const *pl, size_t size)
{
    checksum_t sum = checksumMethod(pl, size);

    // Return the one's complement of the sum
    return static_cast<checksum_t>(~sum);
}


checksum_t MiddleWare::checksumMethod(uint8_t const *pl, size_t size)
{
    uint32_t sum = 0;

    // Process 16-bit words
    while (size > 1) 
    {
        sum += (pl[0] << 8) | pl[1];
        pl += 2;
        size -= 2;
    }

    // Handle an odd byte, if present
    if (size > 0) {
        sum += pl[0] << 8;
    }

    // Fold 32-bit sum into 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<checksum_t>(sum);
}
