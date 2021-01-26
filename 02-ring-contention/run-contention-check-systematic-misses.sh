#!/usr/bin/env bash

sudo rm -rf plot/table-misses.csv
touch plot/table-misses.csv
TOTAL_PHYSICAL_CORE=`grep '^core id' /proc/cpuinfo |sort -u|wc -l`
MAX_PHYSICAL_CORE=$(($TOTAL_PHYSICAL_CORE-1))


for RECEIVER_SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do 

	for RECEIVER_CORE in $(seq 0 $MAX_PHYSICAL_CORE); do 

		echo "receiver $RECEIVER_CORE $RECEIVER_SLICE"
		echo "Receiver Core $RECEIVER_CORE Slice $RECEIVER_SLICE" >> plot/table-misses.csv

		# Print the header
		for SENDER_SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do 
			echo -n ",Slice $SENDER_SLICE" >> plot/table-misses.csv
		done

		echo "" >> plot/table-misses.csv

		# Run the various iterations with contention (sender LLC misses)
		for SENDER_CORE in $(seq 0 $MAX_PHYSICAL_CORE); do 

			echo -n "Core $SENDER_CORE," >> plot/table-misses.csv

			for SENDER_SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do	

				if [[ "$SENDER_CORE" == "$RECEIVER_CORE" ]]; then
					echo -n "-," >> plot/table-misses.csv
					continue
				fi

				# Kill previous processes
				sudo killall -9 sender-no-loads &> /dev/null
				sudo killall -9 sender &> /dev/null
				sleep 0.1

				until
					echo "sender $SENDER_CORE $SENDER_SLICE"
					sudo ./bin/sender $SENDER_CORE $SENDER_SLICE 50 1 > /dev/null &
					sleep 0.5
					sudo ./bin/receiver $RECEIVER_CORE $RECEIVER_SLICE ./out/sender-loads-$SENDER_SLICE-N.out > /dev/null
				do
					echo "Repeating because it failed"
					sudo killall -9 sender &> /dev/null
				done

				sudo killall -9 sender &> /dev/null

				source venv/bin/activate
				python print-avg-std.py ./out/sender-loads-$SENDER_SLICE-N.out >> plot/table-misses.csv
				deactivate

				echo -n "," >> plot/table-misses.csv
			done

			echo "" >> plot/table-misses.csv
		done

		echo "" >> plot/table-misses.csv
	done

done
