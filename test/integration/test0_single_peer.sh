#!/bin/bash

startup_peers() 
{
    # Reset logs so we only find the output of this test case in them
    echo "" >peer1.log

    #
    # Start the Peer processes
    #
    echo "Starting Peers..."
    ${PEER} -i1 -p4201 -c ./single_peer_local.cfg -l ./peer1.log &
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
    # Test Execution: Send 100 Messages only to itself, no other peer is present
    #
    echo "Executing test..."

    for i in $(seq 1 100);
    do
        echo "send Hello_World_${i}!" >/tmp/peer_pipe_1
    done

    # Since no other peer is present, the test should finish virtually instantly
    sleep 1
}

verify()
{
    echo "Analyzing logs..."
    cat peer1.log | grep "Delivered" | grep "Hello_World" > peer1_delivered.log
    DELIVERED_MSG_NR=$(wc -l peer1_delivered.log | sed 's/\([0-9]*\).*/\1/')

    if [ "${DELIVERED_MSG_NR}" -ne "100" ]; then
        echo "Test failed, peer1 did not deliver 100 messages, it delivered ${DELIVERED_MSG_NR} messages!" >&2
        exit 1
    fi

    for i in $(seq 1 100);
    do
        # gets the ith line of peer1_delivered.log, then verifies that the "Hello_World_<i>!"
        # string is contained in it
        NTH_DELIVERED=$(sed "${i}q;d" peer1_delivered.log | grep "Hello_World_${i}!")
        if [ -z "${NTH_DELIVERED}" ]; then
            echo "Test failed, the message \"Hello_World_${i}!\" was delivered out-of-order." >&2
        fi
    done    

    rm -f peer1_delivered.log
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
