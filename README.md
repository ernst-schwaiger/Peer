# Peer
Tiny project demonstrating reliable group communication
Prerequisites:
```
sudo apt install -Y g++ cmake make gcovr
```
## Build

Build the `Peer` release binary as `Peer/release/Peer`:
```
cd Peer
mkdir release
cd release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

## Execute 
```
Usage: ../../build/Peer [-i <peerId>] [-a <ipaddr>] [-p <udpPort>] [-c <configFile>] [-l <logFile>] [-e <errorInject>]
   <peerId>        unique peer id in the range [0..65534], default is 1.
   <ipaddr>        local IPV4 address, default is 127.0.0.1.
   <udpPort>       local udp port in the range [1025..65534], default is 4201.
   <configFile>    path to an already existing configuration file, default is ./peer.cfg.
   <logFile>       path to log file. If none is provided stdout/stderr is used.
   <errorInject>   string of format <peer id>:<msg seq#>:<bit offset> to inject a bit error on the given offset in the specified message of the given peer.
```
`Peer/peer.cfg` contains example configuration data.
After the `Peer` process started, it creates a named pipe, e.g. `/tmp/peer_pipe_<peerId>` and listens for user commands, e.g.
```
echo send foobar >/tmp/peer_pipe_1
echo inject 1:10:33 >/tmp/peer_pipe_1 # injects bit flip on msg 10 of peer 1 at bit offset 33
echo stop >/tmp/peer_pipe_1
```
The "stop" command terminates the `Peer` process and removes the named pipe.

## Test and Coverage
Build the `Peer` test binary in `Peer/debug/Peer`:
```
cd Peer
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j
```

### Execute Tests
Either `ctest --output-on-failure` or `./PeerTest`.
### Generate Coverage Report
```
make coverage
```
generates a `Peer/debug/coverage.html` indicating covered/not covered parts of the code.

## Project Assumptions and Decisions

### IP Version Support

Only IPV4 is supported

### Udp Packet Format

We use: 

Message Datagram:
* 2 Bytes Peer-Id (Network Byte Order)
* 2 Bytes Sequence Number (Network Byte Order)
* 1..n Bytes Message
* 2 Bytes RFC 1071 (Network Byte Order)

Messages may contain arbitrary byte sequences

ACK Datagram:
* 2 Bytes Peer-Id (Network Byte Order)
* 2 Bytes Sequence Number (Network Byte Order)
* 2 Bytes RFC 1071 (Network Byte Order)

### Accepted Sequence Numbers

Anything in the range [expectedSeqNr, expectedSeqNr + 9], including wrap-around at 2^16-1.

### Timeouts/Repeats

* One second timeout w/o ACK before resending the frame
* At most 4 transmissions, after 4rd transmission timeout, we give up

### Message Resends

We assume for resending that the original MessageId [Peer-Id, Seq#] is used, otherwise, peers cannot distinguish if the Message is a new one, or a resent one.
