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
    ${PEER} -i1 -p4201 -c ./single_peer_local.cfg -l ./peer1.log -e 1:0:18&
    sleep 0.5 # wait for the peers proper startup, creation of named pipes
    if [ ! -p /tmp/peer_pipe_1 ]; then
        echo "Test Failed, named pipe \"/tmp/peer_pipe_1\" does not exist!" >&2
        echo "Is ${PEER} the proper binary?" >&2
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
    # Test Execution: Peer sends messages only to itself, but does not
    # deliver it since bits got flipped
    #
    echo "Executing test9..."
    echo "inject 1:1:22" >>/tmp/peer_pipe_1
    # This one gets its bit flipped via command line...
    echo "send Hello_World!" >/tmp/peer_pipe_1
    # ...and the following one via the named pipe
    echo "send Hello_World2!" >/tmp/peer_pipe_1
    # Wait for the timeout for both messages
    sleep 4
}

verify()
{
    echo "Analyzing logs from test9..."
    # this one greps for both messages at the same time
    DELIVERD_PEER=$(cat peer1.log | grep "Delivered" | grep "Hello" | grep "\[1,0\]")

    if [ -n "${DELIVERD_PEER}" ]; then
        echo "Test failed, peer1 did deliver a message, although all receptions should have failed due to bit flips!" >&2
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
