#!/bin/bash
# executes all tests in a row provided they follow the pattern test*.sh

if [ "$#" -ne 1 ]; then
    echo "Usage: $1 <PeerBinary>" >&2
    exit 1
fi

if [ ! -f "$1" ]; then
    echo "PeerBinary $1 does not exist." >&2
    exit 1
fi

PEER_BIN=${1}
all_passed=true
ALL_TESTS=$(find . -name "test*.sh" | sort)
failed_tests=""

for test in ${ALL_TESTS}; do
    echo "--- running test ${test}..."
    ${test} ${PEER_BIN}
    if [ $? -ne 0 ]; then
        failed_tests="${failed_tests} ${test}"
    fi
done

if [ -z "${failed_tests}" ]; then
    echo "all tests passed."
else
    echo "failed tests:${failed_tests}"
fi
