#pragma once

#include <vector>
#include <unistd.h>

#include "ISocket.h"
#include "IApp.h"
#include "MiddleWare.h"

using namespace std;

namespace rgc {

static constexpr useconds_t LOOP_TIME_100_MS = 100000; // 100 milliseconds

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
            payload_t rxPayload = m_receivedPayloads.back();
            m_receivedPayloads.pop_back();

            copy(begin(rxPayload), end(rxPayload), &buf[0]);
            ret.transmitBytes = rxPayload.size();
        }

        return ret; 
    }

    mutable vector<payload_t> m_receivedPayloads;

private:
};

class TestTxSocket : public ITxSocket
{
public:
    TestTxSocket(peer_t const &peer) 
    {
        m_remoteSockAddr.sin_family = AF_INET;
        m_remoteSockAddr.sin_addr.s_addr = inet_addr(peer.peerIpAddress.c_str());
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

    mutable vector<payload_t> m_sentPayloads;

private:
    struct ::sockaddr_in m_remoteSockAddr;
};

class TestApp : public IApp
{
public:
    TestApp(IRxSocket *pRxSocket, std::vector<ITxSocket *> &txSockets, size_t numLoops) : 
        m_middleWare(this, pRxSocket, txSockets),
        m_numLoops(numLoops) 
    {}
    virtual ~TestApp() {}
    virtual void deliverMessage(MessageId msgId, payload_t const &payload) const {};
    virtual void run()
    {
        for (size_t i = 0; i < m_numLoops; i++)
        {
            m_middleWare.rxTxLoop(); 
            usleep(LOOP_TIME_100_MS);
        } 
    }

private:
    MiddleWare m_middleWare;
    size_t m_numLoops;
};

}
