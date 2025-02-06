#!/bin/bash

# (Integrity Check) --> Prüft, dass keine Nachricht doppelt geliefert wird. Seite 123.

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
    echo "Executing test6 (Integrity Test)..."
    echo "send Integrity_Test!" >/tmp/peer_pipe_1
    sleep 3
}

verify()
{
    echo "Analyzing logs for test6..."
    # Ensure that each peer only delivers the message once (no duplicates)
    DUPLICATE_PEER1=$(cat peer1.log | grep "Delivered" | grep "Integrity_Test!" | wc -l)
    DUPLICATE_PEER2=$(cat peer2.log | grep "Delivered" | grep "Integrity_Test!" | wc -l)
    DUPLICATE_PEER3=$(cat peer3.log | grep "Delivered" | grep "Integrity_Test!" | wc -l)

    if [ "${DUPLICATE_PEER1}" -gt 1 ] || [ "${DUPLICATE_PEER2}" -gt 1 ] || [ "${DUPLICATE_PEER3}" -gt 1 ]; then
        echo "Test failed, a message WAS delivered more than once!" >&2
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