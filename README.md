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

* Filter out unknown remote IP/Port Numbers
* Logger
* Error Injection
* Command Interface/Named Pipe
* Send an ACK back to sending node instead of the same Data 
* RFC 1071 CRC Implementation
* Add sanitizers to debug build
* Add stricter warnings to debug and release builds
* Check if we lose incoming Udp Packets due to async Rx processing

## Open for Clarification

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

