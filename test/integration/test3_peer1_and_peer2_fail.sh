#!/bin/bash

startup_peers() 
{
    # Reset logs so we only find the output of this test case in them
    echo "" >peer1.log
    echo "" >peer2.log
    echo "" >peer3.log

    #
    # Start the Peer processes
    #
    echo "Starting Peers..."
    ${PEER} -i1 -p4201 -c ./peer1_local.cfg -l ./peer1.log &
    ${PEER} -i2 -p4202 -c ./peer2_local.cfg -l ./peer2.log &
    ${PEER} -i3 -p4203 -c ./peer3_local.cfg -l ./peer3.log &
    sleep 0.5 # wait for the peers proper startup, creation of named pipes
    if [ ! -p /tmp/peer_pipe_1 ]; then
        echo "Test Failed, named pipe \"/tmp/peer_pipe_1\" does not exist!" >&2
        echo "Is ${PEER} the proper binary?" >&2
    fi
    if [ ! -p /tmp/peer_pipe_2 ]; then
        echo "Test Failed, named pipe \"/tmp/peer_pipe_2\" does not exist!" >&2
        exit 1
    fi
    if [ ! -p /tmp/peer_pipe_3 ]; then
        echo "Test Failed, named pipe \"/tmp/peer_pipe_3\" does not exist!" >&2
        exit 1
    fi
}


shutdown_peers() 
{
    remaining_pipes=$(find /tmp -maxdepth 1 -name "peer_pipe_*" -type p)
    for remaining_pipe in ${remaining_pipes}; do
        echo "stop" > ${remaining_pipe}
    done
    sleep 0.5 # wait for the peers proper shutdown
}

execute()
{
    #
    # Test Execution: Peer one sends a message
    #
    echo "Executing test..."
    echo "send Hello_World!" >/tmp/peer_pipe_1
    # Peer 1 receives ACK from peer 2 and then fails.
    sleep 1.5
    echo "stop" > /tmp/peer_pipe_1 
    # Peer 2 receives ACK from self, but then fails.
    sleep 1.0
    echo "stop" > /tmp/peer_pipe_2 

    # Time for the mechanism to recognize that no further communication is taking place
    sleep 3
}

verify()
{
    echo "Analyzing logs..."
    # Peer 1 should have delivered the message, but then fail
    DELIVERED_PEER=$(cat peer1.log | grep "Delivered" | grep "Hello_World!" | grep "\[1,0\]")
    if [ ! -z "${DELIVERED_PEER}" ]; then
        echo "Test failed, peer1 did NOT deliver message before failing!" >&2
        exit 1
    fi
    # Peer 2 should have received the message and sent itself ACK, but not forward it
    DELIVERED_PEER=$(cat peer2.log | grep "ACK" | grep "Hello_World!" | grep "\[1,0\]")
    if [ ! -z "${DELIVERED_PEER}" ]; then
        echo "Test failed, peer2 did NOT ACKnowledge itself before failing!" >&2
        exit 1
    fi
    # Peer 3 should NOT have received the message
    DELIVERED_PEER=$(cat peer3.log | grep "Received" | grep "Hello_World!" | grep "\[1,0\]")
    if [ ! -z "${DELIVERED_PEER}" ]; then
        echo "Test failed, peer3 should NOT have received the message!" >&2
        exit 1
    fi
}

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <PeerBinary>" >&2
    exit 1
fi

if [ ! -f "$1" ]; then
    echo "PeerBinary $1 does not exist." >&2
    exit 1
fi

PEER=$1
startup_peers
execute
shutdown_peers
verify

# if we arrive here, we are good :-)
echo "Test passed."
