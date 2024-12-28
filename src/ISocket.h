#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include <arpa/inet.h> // struct struct ::sockaddr_in

#include "CommonTypes.h"

namespace rgc {

static constexpr size_t BUFFER_SIZE = 1024;

typedef std::array<char,  BUFFER_SIZE> rx_buffer_t; 

struct TransmitStatus
{
    size_t transmitBytes;
    uint8_t status;
};

class IRxSocket
{
public:
    virtual ~IRxSocket() {};
    virtual TransmitStatus receive(rx_buffer_t &buf, struct sockaddr_in &remoteAddr) const = 0;
};

class ITxSocket
{
public:
    virtual ~ITxSocket() {};
    virtual peerId_t getPeerId() const = 0;
    virtual TransmitStatus send(payload_t const &payload) const = 0;
    virtual struct ::sockaddr_in const &getRemoteSocketAddr() const = 0;
};

} // namespace