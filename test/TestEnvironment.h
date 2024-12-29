#pragma once

#include <vector>
#include <iostream>
#include <unistd.h>


#include "ISocket.h"
#include "IApp.h"
#include "MiddleWare.h"
#include "Logger.h"


using namespace std;

namespace rgc {

static constexpr std::chrono::duration<int64_t, std::milli> LOOP_TIME_100_MS = std::chrono::milliseconds(100);        

typedef struct MsgIdAndPayload
{
    MessageId msgId;
    payload_t payload;
} msgId_payload_t;

typedef struct 
{
    peer_t peer;
    payload_t payload;
} sender_payload_t;


class TestRxSocket : public IRxSocket
{
public:
    TestRxSocket() {}
    virtual ~TestRxSocket() {};
    virtual TransmitStatus receive(rx_buffer_t &buf, struct sockaddr_in &remoteAddr) const
    {
        TransmitStatus ret = { 0, 0 };

        if (!m_receivedPayloads.empty())
        {
            sender_payload_t rxSenderPayload = m_receivedPayloads.back();
            m_receivedPayloads.pop_back();

            copy(begin(rxSenderPayload.payload), end(rxSenderPayload.payload), &buf[0]);
            ret.transmitBytes = rxSenderPayload.payload.size();
            remoteAddr.sin_addr.s_addr = rxSenderPayload.peer.peerIpAddress;
            remoteAddr.sin_port = htons(rxSenderPayload.peer.peerUdpPort);
            remoteAddr.sin_family = AF_INET;
        }

        return ret; 
    }

    mutable vector<sender_payload_t> m_receivedPayloads;

private:
};

class TestTxSocket : public ITxSocket
{
public:
    TestTxSocket(peer_t const &peer) : m_peerId(peer.peerId)
    {
        m_remoteSockAddr.sin_family = AF_INET;
        m_remoteSockAddr.sin_addr.s_addr = peer.peerIpAddress;
        m_remoteSockAddr.sin_port = htons(peer.peerUdpPort);        
    }
    virtual ~TestTxSocket() {}
    virtual TransmitStatus send(payload_t const &payload) const
    {
        m_sentPayloads.push_back(payload);
        TransmitStatus ret = { payload.size(), 0 };
        return ret;
    }

    virtual struct ::sockaddr_in const &getRemoteSocketAddr() const
    {
        return m_remoteSockAddr;
    }

    virtual peerId_t getPeerId() const
    {
        return m_peerId;
    }

    mutable vector<payload_t> m_sentPayloads;

private:
    peerId_t m_peerId;
    struct ::sockaddr_in m_remoteSockAddr;
};

class TestApp : public IApp
{
public:
    TestApp(IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets, size_t numLoops = 10) : 
        m_middleWare(this, pRxSocket, txSockets),
        m_logger(),
        m_numLoops(numLoops),
        m_now() 
    {}
    virtual ~TestApp() {}
    virtual void deliverMessage(MessageId msgId, payload_t const &payload) const 
    { 
        msgId_payload_t entry { msgId, payload };
        deliveredMsgs.push_back(entry); 
    }

    virtual void run()
    {
        for (size_t i = 0; i < m_numLoops; i++)
        {
            m_middleWare.rxTxLoop(m_now);
            m_now += LOOP_TIME_100_MS; // simulate that time passes
        } 
    }

    TestApp &numLoops(size_t numLoops)
    {
        m_numLoops = numLoops;
        return *this;
    }

    void log(LOG_TYPE type, std::string const &msg)
    {
        switch(type)
        {
            case LOG_TYPE::DEBUG:
                m_logger.logDebug(msg, m_now);
                break;
            case LOG_TYPE::ERR:
                m_logger.logErr(msg, m_now);
                break;
            case LOG_TYPE::WARN:
                m_logger.logWarn(msg, m_now);
                break;
            case LOG_TYPE::MSG:
                m_logger.logMsg(msg, m_now);
                break;
            default:
                assert(false);
        }
    }

    virtual std::string getUserCommand()
    {
        // FIXME
        return "";
    }

    mutable std::vector<msgId_payload_t> deliveredMsgs;

private:
    MiddleWare m_middleWare;
    Logger m_logger;
    size_t m_numLoops;
    std::chrono::system_clock::time_point m_now;
};

}
