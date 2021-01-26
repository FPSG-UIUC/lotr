# Measure contention on the ring interconnect

We used these scripts to reverse engineer the ring interconnect protocols and understand the ring contention patterns.
These scripts consist of running both a `receiver` and a `sender`, the latter of which is meant to create ring contention to delay the receiver's measurements.
The `receiver` is based on the `measure-slice-latency-optimized` program that we used before, but this time it times 4 sequential loads (see paper).
The results are the heatmaps in the paper.

## Preliminaries

- Build the scripts with `make`.
- We recommend that you run the `../run-first-prefoff.sh` script before taking timing measurements. 
This script disables hyperthreading, disables the prefetchers, enables some hugepages (just in case they were disabled in your system), and fixes the CPU frequency.
You may alternatively use the `../run-first.sh` script to enable the prefetchers, although for the reverse engineering section in the paper we had disabled them (see paper).
- Set up a Python virtual environment as follows: 
```sh
virtualenv -p python3 venv
source venv/bin/activate
pip install matplotlib
deactivate
```

## Run

Make sure that your system is idle and minimize the number of background processes that are running and may add noise to the experiment.
Then, to reproduce the results of the paper, you can run the three scripts below in order.

- `./run-contention-check-systematic-baseline.sh`: this script measures the latency measured by the receiver under all its configurations, when the sender (pinned to different cores) does not perform any loads (i.e., in the absence of contention). 
The results of this script are the same even without running any sender at all.
It takes around 15 minutes to run.
- `./run-contention-check-systematic-hits.sh`: this script measures the latency measured by the receiver under all its configurations, with different senders running in the background (all configurations tested).
The senders used in this script always hit in the LLC. 
It takes around 2 hours to run.
- `./run-contention-check-systematic-misses.sh`: this script measures the latency measured by the receiver under all its configurations, with different senders running in the background (all configurations tested).
The senders used in this script always miss in the LLC.
It takes around 2 hours to run.

## Output

After running each script, you can find the output in the file in different csv tables inside the `plot` directory.

Each csv table contains the mean and variance latency measured by each receiver under every sender configuration (in the same ordering of the heatmaps of the paper).
We analyzed these latencies in the reverse engineering section of the paper (where they are reported as a heatmap).

Note that sometimes the difference in latency due to contention may be small.
You can run the experiment with more than 4 loads (by changing `receiver.c`) if you want to get additional data points.

## Notes

There are also two more scripts in this folder, which are not necessary to reproduce the results but may come handy:

- `./run-test.sh`: this script can be used to play with specific receiver/sender configurations (by changing Rc Rs Sc Ss directy in the script), without re-running the full 2-hour long tests above.
It's quick to run.
- `./run-compare-distributions.sh`: this script is what we used to compare individual distributions of latency samples with specific fixed Rc Rs and specific varying Sc Ss configurations (to customize directly in the script). 
The result is a histogram with the probability densities of the different configurations.
It's relatively quick to run and its output is in `plot/distributions.pdf`.
