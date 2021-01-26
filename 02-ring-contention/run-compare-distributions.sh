#!/usr/bin/env bash

RECEIVER_CORE=2
RECEIVER_SLICE=1
SENDER_CORE=1
SENDER_SLICE=0

# Remove data from previous runs
sudo rm -rf out/*.out

# Kill previous processes
sudo killall -9 sender-no-loads &> /dev/null
sudo killall -9 sender &> /dev/null
sleep 0.1

# Run receiver with no contention
until
	sudo ./bin/sender-no-loads $SENDER_CORE > /dev/null &
	sleep 0.5
	sudo ./bin/receiver $RECEIVER_CORE $RECEIVER_SLICE ./out/sender-no-loads.out > /dev/null
do
	echo "Repeating because it failed"
	sudo killall -9 sender-no-loads &> /dev/null
done

# Kill previous processes
sudo killall -9 sender-no-loads &> /dev/null
sleep 0.1

# Run receiver with contention
until
	sudo ./bin/sender $SENDER_CORE $SENDER_SLICE 0 1 > /dev/null &
	sleep 0.5
	sudo ./bin/receiver $RECEIVER_CORE $RECEIVER_SLICE ./out/sender-loads-$SENDER_CORE-$SENDER_SLICE.out > /dev/null
do
	echo "Repeating because it failed"
	sudo killall -9 sender &> /dev/null
done

# Kill processes
sudo killall -9 sender &> /dev/null
sleep 0.1

# Generate plot
echo "Generating plot"
source venv/bin/activate
python plot-compare-distributions.py ./out/sender-no-loads.out ./out/sender-loads-$SENDER_CORE-$SENDER_SLICE.out
deactivate
