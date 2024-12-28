#pragma once

#include "ISocket.h"
#include "ConfigParser.h"

namespace rgc {

class UdpRxSocket : public IRxSocket
{
public:
    UdpRxSocket(std::string const &localIp, uint16_t localPort);
    virtual ~UdpRxSocket();
    virtual TransmitStatus receive(rx_buffer_t &buf, struct sockaddr_in &remoteAddr) const;

private:
    int m_socketDesc;
    struct ::sockaddr_in m_localSockAddr;
};


class UdpTxSocket : public ITxSocket
{
public:
    UdpTxSocket(peer_t const &peer);
    virtual ~UdpTxSocket();
    virtual TransmitStatus send(payload_t const &payload) const;

    virtual struct ::sockaddr_in const &getRemoteSocketAddr() const
    {
        return m_remoteSockAddr;
    }

    virtual peerId_t getPeerId() const
    {
        return m_peerId;
    }

private:
    peerId_t m_peerId;
    int m_socketDesc;
    struct ::sockaddr_in m_remoteSockAddr;
};

} // namespace rgc
