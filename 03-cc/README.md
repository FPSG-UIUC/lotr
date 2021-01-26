# Covert channel on the ring interconnect

The code in this folder implements a basic cross-core covert-channel on the ring interconnect.
The `sender` transmits a sequence of zeros and ones.
The `receiver` collects a number of latency samples.
Then these samples are parsed into a sequence of bits.

## Preliminaries

- Build the scripts with `make`.
- This time, we recommend that you run the **`../run-first.sh`** script before taking timing measurements.
This script disables hyperthreading, enables the prefetchers (see paper), enables some hugepages (just in case they were disabled in your system), and fixes the CPU frequency.
- Set up a Python virtual environment as follows: 
```sh
virtualenv -p python3 venv
source venv/bin/activate
pip install matplotlib
deactivate
```

## Run

Make sure that your system is idle and minimize the number of background processes that are running and may add noise to the experiment.
Then, to reproduce the results of the paper, you can run the following two commands.

- `./run-all-covert.sh`: this script runs the covert channel test of the paper, with the sender transmitting alternating bits to the receiver.
It outputs a plot with a sample of the trace collected by the receiver.
The default interval used is 4000 cycles, but you can use a different interval by passing it as an argument to the script.
- `./run-all-capacity.sh`: this script runs the covert channel test with different interval values.
It outputs the covert channel capacity plot of the paper.

The other files of this folder are used by the scripts above to run the experiments.

## Output

You can find the outputs of the experiments in the files `plot/covert-channel-bits.pdf` and `plot/capacity-plot.pdf`.

### My results do not look like the ones in the paper! What do I do?

Some variance is expected due to noise in the collected data and/or differences in the hardware/software.
To try to reduce this variance, we recommend the following steps.

First, double-check that your system is idle and that you minimized the number of background processes.
Second, you can try to customize the `print-errors.py` script and the way it classifies ones and zeros (look for the `FIXME` comments).
Also, note that it may take a couple of runs for the results to stabilize.

## Note

This proof of concept implementation uses root privileges to get the physical address in the slice mapping function.
However, as discussed in the paper, the slice mapping of an address can be computed with unprivileged access too (by using timing information).
That is, root access is not a requirement of the attack and is used in our implementation only for convenience.