# Peer
Tiny project demonstrating reliable group communication

## Build

```
cd Peer
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j
ctest --output-on-failure
```

## ToDos

* Error Injection
* Command Interface/Named Pipe
* RFC 1071 CRC Implementation
* Send every message to "ourselves"
* For Message Id/CRC calculation in payload use uint8_t* instead of char*
* Check if we lose incoming Udp Packets due to async Rx processing

## Open for Clarification

### IP Standard

Assume: Only use IPV4

### Udp Packet Format

We use: 

Message Datagram:
* 2 Bytes Peer-Id (Network Byte Order)
* 2 Bytes Sequence Number (Network Byte Order)
* 1..n Bytes Message
* 2 Bytes RFC 1071 (Network Byte Order)

ACK Datagram:
* 2 Bytes Peer-Id (Network Byte Order)
* 2 Bytes Sequence Number (Network Byte Order)
* 2 Bytes RFC 1071 (Network Byte Order)

### Accepted Sequence Numbers

Anything in the range [expectedSeqNr, expectedSeqNr + 9], including wrap-around at 2^16-1.

### Timeouts/Repeats

* One second timeout w/o ACK before resending the frame
* At most 3 transmissions, after 3rd transmission timeout, we give up

### Message Resends

We assume for resending that the original MessageId [Peer-Id, Seq#] is used for resending, otherwise, peers cannot distinguish if the Message is a new one, or a resent one.

