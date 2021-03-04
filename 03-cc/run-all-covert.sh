#!/bin/bash

# Parse args
if [ $# -eq 1 ]; then
	INTERVAL=$1
elif [ $# -lt 1 ]; then
	INTERVAL=3000
else
	echo "ERROR: Incorrect number of arguments"
	echo "./run.sh [interval]"
	exit
fi

echo "Running covert channel test with an interval of $INTERVAL cycles"

# Kill previous processes
sudo killall sender &> /dev/null

# Run
until
	sudo ./bin/sender 4 1 0 1 $INTERVAL > /dev/null &
	sleep 1
	sudo ./bin/receiver 3 2 ./out/receiver-contention.out $INTERVAL > /dev/null
do
	echo "Repeating iteration $i because it failed"
	sudo killall sender &> /dev/null
done

sleep 0.05
sudo killall sender &> /dev/null

echo "Generating plot"
source venv/bin/activate
python plot-channel-bits-figure.py out/receiver-contention.out $INTERVAL
python print-errors.py out/receiver-contention.out $INTERVAL
deactivate
