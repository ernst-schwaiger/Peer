#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "UdpSocket.h"

using namespace std;
using namespace rgc;

UdpRxSocket::UdpRxSocket(string const &localIp, uint16_t localPort)
{
    // FIXME: Throw if we don't get a valid descriptor
    m_socketDesc = socket(AF_INET, SOCK_DGRAM, 0);
    // configure for async rx
    int flags = fcntl(m_socketDesc, F_GETFL, 0); 
    fcntl(m_socketDesc, F_SETFL, flags | O_NONBLOCK);
    memset(&m_localSockAddr, 0, sizeof(m_localSockAddr)); 
    m_localSockAddr.sin_family = AF_INET; 
    m_localSockAddr.sin_addr.s_addr = INADDR_ANY; 
    m_localSockAddr.sin_port = htons(localPort);

    // Bind the socket to the specified port 
    if (bind(m_socketDesc, reinterpret_cast<const struct sockaddr *>(&m_localSockAddr), sizeof(m_localSockAddr)) < 0) 
    { 
        // FIXME error handling
    }
}

UdpRxSocket::~UdpRxSocket()
{
    cout << "Closing tx socket.\n";
    close(m_socketDesc);
}

TransmitStatus UdpRxSocket::receive(rx_buffer_t &buf, struct sockaddr_in &remoteAddr) const
{
    socklen_t remoteAddrLen = sizeof(remoteAddr);     
    memset(&remoteAddr, 0, sizeof(struct sockaddr_in));
    TransmitStatus ret = { 0, 0 };
    ssize_t rxBytes = recvfrom(m_socketDesc, buf.data(), buf.size(), 0, (struct sockaddr *)&remoteAddr, &remoteAddrLen);

    if (rxBytes < 0)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            ret.status = errno;
        }
    }
    else
    {
        ret.transmitBytes = static_cast<size_t>(rxBytes);
    }

    return ret;
}


UdpTxSocket::UdpTxSocket(peer_t const &peer) : m_peer(peer)
{
    std::memset(&m_remoteSockAddr, 0, sizeof(m_remoteSockAddr));
    // FIXME: Only IP V4?
    m_remoteSockAddr.sin_family = AF_INET;
    m_remoteSockAddr.sin_addr.s_addr = inet_addr(m_peer.peerIpAddress.c_str());
    m_remoteSockAddr.sin_port = htons(m_peer.peerUdpPort);
    // FIXME: Throw if we don't get a valid descriptor
    m_socketDesc = socket(AF_INET, SOCK_DGRAM, 0);
}

UdpTxSocket::~UdpTxSocket()
{
    cout << "Closing tx socket.\n";
    close(m_socketDesc);
}

TransmitStatus UdpTxSocket::send(payload_t const &payload) const
{
    TransmitStatus ret = { 0, 0 };
    ssize_t sentBytes = sendto(
        m_socketDesc, 
        payload.data(), 
        payload.size(), 
        0, 
        reinterpret_cast<struct sockaddr const *>(&m_remoteSockAddr), 
        sizeof(m_remoteSockAddr));

    if (sentBytes < 0)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            ret.status = errno;
        }
    }
    else
    {
        ret.transmitBytes = static_cast<size_t>(sentBytes);
    }

    return ret;
}