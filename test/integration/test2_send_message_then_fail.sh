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
        exit 1
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
    echo "Executing test2..."
    echo "send Hello_World!" >/tmp/peer_pipe_1
    # wait for 1500msecs, then terminate peer 1, ensuring the first remote peer got the message, the second one did not
    sleep 1.5
    echo "stop" > /tmp/peer_pipe_1

    # Give second remote peer enough time to send the message 3 times to the terminated peer and wait for the tx timeout
    sleep 9
}

verify()
{
    echo "Analyzing logs for test2..."
    DELIVERD_PEER=$(cat peer1.log | grep "Delivered" | grep "Hello_World!" | grep "\[1,0\]")
    if [ ! -z "${DELIVERD_PEER}" ]; then
        echo "Test failed, peer1 *did* deliver message, although it should have terminated!" >&2
        exit 1
    fi
    DELIVERD_PEER=$(cat peer2.log | grep "Delivered" | grep "Hello_World!" | grep "\[1,0\]")
    if [ -z "${DELIVERD_PEER}" ]; then
        echo "Test failed, peer2 did not deliver message!" >&2
        exit 1
    fi
    DELIVERD_PEER=$(cat peer3.log | grep "Delivered" | grep "Hello_World!" | grep "\[1,0\]")
    if [ -z "${DELIVERD_PEER}" ]; then
        echo "Test failed, peer3 did not deliver message!" >&2
        exit 1
    fi
}

if [ "$#" -ne 1 ]; then
    echo "Usage: $1 <PeerBinary>" >&2
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
