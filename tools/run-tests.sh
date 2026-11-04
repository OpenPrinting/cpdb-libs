#!/bin/bash

set -e -u

LOG=run-tests.log
FRONTEND=./cpdb-text-frontend

export LD_LIBRARY_PATH=`pwd`/cpdb/.libs

cleanup() {
    # Show log
    cat $LOG
    # Remove the log file
    rm -f $LOG
}

trap cleanup 0 EXIT INT QUIT ABRT PIPE TERM

# Create the log file
rm -f $LOG
touch $LOG

# Run the test frontend with a session D-Bus and feed in commands.
( \
  sleep 1; \
  echo stop \
) | dbus-run-session -- $FRONTEND > $LOG 2>&1 &

# Give the frontend a maximum of 5 seconds to run and then kill it, to avoid
# the script getting stuck if stopping it fails.
i=0
FRONTEND_PID=$!
while kill -0 $FRONTEND_PID >/dev/null 2>&1; do
    i=$((i+1))
    if test $i -ge 5; then
	kill -KILL $FRONTEND_PID >/dev/null 2>&1 || true
	echo "FAIL: Frontend keeps running!"
	exit 1
    fi
    sleep 1
done


if grep -q "Stopping front end" $LOG; then
    echo "Frontend correctly shut down"
else
    echo "FAIL: Frontend not correctly stopped!"
    exit 1
fi

if ! dbus-run-session -- ./test-capability-counts >> $LOG 2>&1; then
    echo "FAIL: Capability counts test failed!"
    exit 1
fi

echo "SUCCESS!"

exit 0
