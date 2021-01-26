#!/usr/bin/env bash

sudo rm -rf plot/table-hits.csv
touch plot/table-hits.csv
TOTAL_PHYSICAL_CORE=`grep '^core id' /proc/cpuinfo |sort -u|wc -l`
MAX_PHYSICAL_CORE=$(($TOTAL_PHYSICAL_CORE-1))


for RECEIVER_SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do 

	for RECEIVER_CORE in $(seq 0 $MAX_PHYSICAL_CORE); do 

		echo "receiver $RECEIVER_CORE $RECEIVER_SLICE"
		echo "Receiver Core $RECEIVER_CORE Slice $RECEIVER_SLICE" >> plot/table-hits.csv

		# Print the header
		for SENDER_SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do 
			echo -n ",Slice $SENDER_SLICE" >> plot/table-hits.csv
		done

		echo "" >> plot/table-hits.csv

		# Run the various iterations with contention (sender LLC hits)
		for SENDER_CORE in $(seq 0 $MAX_PHYSICAL_CORE); do 

			echo -n "Core $SENDER_CORE," >> plot/table-hits.csv

			for SENDER_SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do	

				if [[ "$SENDER_CORE" == "$RECEIVER_CORE" ]]; then
					echo -n "-," >> plot/table-hits.csv
					continue
				fi

				# Kill previous processes
				sudo killall -9 sender-no-loads &> /dev/null
				sudo killall -9 sender &> /dev/null
				sleep 0.1

				until
					echo "sender $SENDER_CORE $SENDER_SLICE"
					sudo ./bin/sender $SENDER_CORE $SENDER_SLICE 0 1 > /dev/null &
					sleep 0.5
					sudo ./bin/receiver $RECEIVER_CORE $RECEIVER_SLICE ./out/sender-loads-$SENDER_SLICE.out > /dev/null
				do
					echo "Repeating because it failed"
					sudo killall -9 sender &> /dev/null
				done

				sudo killall -9 sender &> /dev/null

				source venv/bin/activate
				python print-avg-std.py ./out/sender-loads-$SENDER_SLICE.out >> plot/table-hits.csv
				deactivate

				echo -n "," >> plot/table-hits.csv
			done

			echo "" >> plot/table-hits.csv
		done

		echo "" >> plot/table-hits.csv
	done

done
