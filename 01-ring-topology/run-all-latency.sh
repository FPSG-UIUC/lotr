#!/usr/bin/env bash

# Parse args
if [ $# -eq 1 ]; then
	SET=$1
elif [ $# -lt 1 ]; then
	SET=5
else
	echo "ERROR: Incorrect number of arguments"
	echo "./run.sh [set-number]"
	exit
fi

TOTAL_PHYSICAL_CORE=`grep '^core id' /proc/cpuinfo |sort -u|wc -l`
MAX_PHYSICAL_CORE=$(($TOTAL_PHYSICAL_CORE-1))

sudo rm -rf out/*.out

for CORE in $(seq 0 $MAX_PHYSICAL_CORE); do
	for SLICE in $(seq 0 $MAX_PHYSICAL_CORE); do		
		until sudo ./bin/measure-slice-latency $CORE $SLICE $SET; do
			echo "Try again"
		done
	done
done

echo "Generating plot"

# Plot results
source venv/bin/activate
python plot-histograms-core-slice.py $SET
deactivate
