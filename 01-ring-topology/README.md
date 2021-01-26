# Measure Slice Latency

We used these scripts to plot the latency of performing a load from each core to each LLC slice.
The results are the histograms in the paper.

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
Then, to reproduce the results of the paper, run:
```sh
./run-all-latency.sh
```

It takes a few minutes to run, but if you want to do a quick test you can reduce the number of repetitions in `measure-slice-latency.c`.
If you have a specific preference of the cache set to use (on some machines the cache set influences the final latency), you can provide that as an argument to the `run-all-latency.sh` script.

## Output

You can find the output of the experiment in the file `plot/per-core.pdf`.

## Notes

If you want to plot a time series plot of the data you collected (e.g., for debugging purposes, if the histogram looks noisy), you can use the script `plot-time-series.py`.
It will produce a plot for each core/slice configuration in the `plot` folder, so you can see what's going on.

We also included an optimized version of the monitoring program, called `monitor-slice-latency-optimized.c` which does not use an eviction set, but simply does pointer chasing on the monitoring set (as described in the paper).
If you want to try it, you can change the `run-all-latency.sh` script to use the optimized version.

## Credits

Some of the code used for these experiments is inspired by the repositories:
- https://github.com/aliireza/slice-aware
- https://github.com/clementine-m/msr-uncore-cbo