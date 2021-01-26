#!/usr/bin/env bash

sudo rm -rf plot/table-baseline.csv
touch plot/table-baseline.csv
TOTAL_PHYSICAL_CORE=`grep '^core id' /proc/cpuinfo |sort -u|wc -l`
MAX_PHYSICAL_CORE=$(($TOTAL_PHYSICAL_CORE-1))


for RECEIVER_SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do 

	for RECEIVER_CORE in $(seq 0 $MAX_PHYSICAL_CORE); do 

		echo "receiver $RECEIVER_CORE $RECEIVER_SLICE"
		echo "Receiver Core $RECEIVER_CORE Slice $RECEIVER_SLICE" >> plot/table-baseline.csv

		# Print the header
		for SENDER_CORE in $(seq 0 $MAX_PHYSICAL_CORE); do 
			echo -n "Core $SENDER_CORE," >> plot/table-baseline.csv
		done

		echo "" >> plot/table-baseline.csv

		# Run the various iterations without contention
		for SENDER_CORE in $(seq 0 $MAX_PHYSICAL_CORE); do 

			if [[ "$SENDER_CORE" == "$RECEIVER_CORE" ]]; then
				echo -n "-," >> plot/table-baseline.csv
				continue
			fi

			# Kill previous processes
			sudo killall -9 sender-no-loads &> /dev/null
			sudo killall -9 sender &> /dev/null
			sleep 0.1

			until
				echo "sender-no-loads $SENDER_CORE"
				sudo ./bin/sender-no-loads $SENDER_CORE > /dev/null &
				sleep 0.5
				sudo ./bin/receiver $RECEIVER_CORE $RECEIVER_SLICE ./out/sender-no-loads.out > /dev/null
			do
				echo "Repeating because it failed"
				sudo killall -9 sender-no-loads &> /dev/null
			done

			sudo killall -9 sender-no-loads &> /dev/null

			source venv/bin/activate
			python print-avg-std.py ./out/sender-no-loads.out >> plot/table-baseline.csv
			deactivate

			echo -n "," >> plot/table-baseline.csv
		done

		echo "" >> plot/table-baseline.csv
	done

done
