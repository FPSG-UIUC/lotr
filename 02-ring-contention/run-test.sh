#!/usr/bin/env bash

RECEIVER_CORE=3
RECEIVER_SLICE=0

echo "> receiver $RECEIVER_CORE $RECEIVER_SLICE"

for SENDER_CORE in 0; do
# for SENDER_CORE in 5; do
# for SENDER_CORE in {0..7}; do

	if [[ "$SENDER_CORE" == "$RECEIVER_CORE" ]]; then
		continue
	fi

	# for SENDER_SLICE in {0..7}; do
	for SENDER_SLICE in 1 2 5; do
	# for SENDER_SLICE in 7; do

		echo ""
		echo "sender $SENDER_CORE $SENDER_SLICE"

		for i in {0..2}; do 

			# Remove data from previous runs
			sudo rm -rf out/*.out

			# Kill previous processes
			sudo killall -9 sender-no-loads &> /dev/null
			sudo killall -9 sender &> /dev/null
			sleep 0.3

			# Run sender and receiver
			until
				# sudo ./bin/sender-no-loads $SENDER_CORE > /dev/null &
				sudo ./bin/sender $SENDER_CORE $SENDER_SLICE 0 1 > /dev/null &
				# sudo ./bin/sender $SENDER_CORE $SENDER_SLICE 50 1 > /dev/null &

				sleep 0.5
				sudo ./bin/receiver $RECEIVER_CORE $RECEIVER_SLICE ./out/sender-no-loads.out
			do
				echo "Repeating because it failed"
				sudo killall -9 sender &> /dev/null
				sudo killall -9 sender-no-loads &> /dev/null
			done

			sudo killall -9 sender-no-loads &> /dev/null
			sudo killall -9 sender &> /dev/null

			source venv/bin/activate
			python print-avg-std.py ./out/sender-no-loads.out
			deactivate

			sleep 1

			echo ""
		done
	done
done
