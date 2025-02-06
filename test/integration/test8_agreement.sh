#!/bin/bash

# (Agreement Check) --> Prüft, dass entweder alle oder keiner die Nachricht erhält. Seite 123.

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
    echo "Executing test8 (Agreement Test)..."
    echo "send Agreement_Test!" >/tmp/peer_pipe_1
    sleep 3
}

verify()
{
    echo "Analyzing logs for test8..."
    # Prüfe, ob die Nachricht inkonsistent verteilt wurde (nur 1 oder 2 Peers haben sie erhalten)
    PEER1=$(cat peer1.log | grep "Delivered" | grep "Agreement_Test!")
    PEER2=$(cat peer2.log | grep "Delivered" | grep "Agreement_Test!")
    PEER3=$(cat peer3.log | grep "Delivered" | grep "Agreement_Test!")
    
    DELIVERED_COUNT=0
    [ ! -z "${PEER1}" ] && ((DELIVERED_COUNT++))
    [ ! -z "${PEER2}" ] && ((DELIVERED_COUNT++))
    [ ! -z "${PEER3}" ] && ((DELIVERED_COUNT++))
    
    if [ "${DELIVERED_COUNT}" -eq 1 ] || [ "${DELIVERED_COUNT}" -eq 2 ]; then
        echo "Test failed, only ${DELIVERED_COUNT} peer(s) delivered the message instead of all or none!" >&2
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