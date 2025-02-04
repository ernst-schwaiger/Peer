#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include <arpa/inet.h>

namespace rgc
{

typedef uint16_t peerId_t;
typedef uint16_t seqNr_t;
typedef uint16_t checksum_t;

typedef struct
{
    peerId_t peerId;
    uint16_t peerUdpPort;
    in_addr_t peerIpAddress; // IP V4 address as uint32 in network byte order
} peer_t;

typedef struct
{
    peerId_t peerId;
    seqNr_t seqNrId;
    uint16_t bitOffset; // offset of bit to flip
} bitflip_t;

// Identifies a message originally sent from a specific peer 
class MessageId final
{
public:
    MessageId(peerId_t peerId, seqNr_t seqNr) : m_peerId(peerId), m_seqNr(seqNr) {}

    bool operator == (MessageId const &rhs) const
    {
        return (m_peerId == rhs.m_peerId) && (m_seqNr == rhs.m_seqNr);
    }

    peerId_t getPeerId() const { return m_peerId; };
    seqNr_t getSeqNr() const { return m_seqNr; };

private:
    peerId_t m_peerId;
    seqNr_t m_seqNr;
};

typedef std::vector<uint8_t> payload_t; 

}