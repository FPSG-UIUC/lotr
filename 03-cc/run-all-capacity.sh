#!/bin/bash

CPU_GHZ=4

rm -rf out/capacity-data.out

for BITRATE in 0.5 1 1.5 2 2.5 3 3.5 4 4.5 5 5.5 6 6.5 7 7.5 8; do
	
	# Compute interval for the desired bitrate
	source venv/bin/activate
	INTERVAL=$(python print-interval-for-rate.py $CPU_GHZ $BITRATE)
	deactivate

	# Kill previous processes
	sudo killall -9 sender &> /dev/null
	sudo killall -9 receiver &> /dev/null

	# Run
	until
		sudo ./bin/sender 1 0 0 1 $INTERVAL > /dev/null &
		sleep 1
		sudo ./bin/receiver 2 1 ./out/receiver-contention.out $INTERVAL > /dev/null
	do
		echo "Repeating iteration $i because it failed"
		sudo killall sender &> /dev/null
	done

	sleep 0.05
	sudo killall -9 sender &> /dev/null
	sudo killall -9 receiver &> /dev/null

	source venv/bin/activate
	ERRORS=$(python print-errors.py out/receiver-contention.out $INTERVAL)
	deactivate

	echo "$BITRATE $ERRORS" >> out/capacity-data.out
done

echo "Generating plot"
source venv/bin/activate
python plot-capacity-figure.py out/capacity-data.out
deactivate