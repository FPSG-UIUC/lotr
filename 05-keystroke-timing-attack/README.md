# Keystroke Timing Attack

This directory contains the code to perform the keystroke timing attack described in the paper.
The `attacker` (receiver/monitor) collects a number of samples while the `victim` (sender) types some keystrokes.
Then these samples are used to infer _when_ keystrokes happened.

## Preliminaries

- Build the attacker and the victim with `make`.
- We recommend that you run the **`../run-first.sh`** script before taking timing measurements.
This script disables hyperthreading, enables the prefetchers (see paper), enables some hugepages (just in case they were disabled in your system), and fixes the CPU frequency.
- Set up a Python virtual environment as follows: 
```sh
virtualenv -p python3 venv
source venv/bin/activate
pip install matplotlib
pip install detecta
deactivate
```

## Run

Make sure that your system is idle and minimize the number of background processes that are running and may add noise to the experiment.
Then, to reproduce the results of the paper, follow the steps below.

1. Make sure that victim and monitor are compiled (see build instructions above). 
2. Open two separate shells. 
In the SSH setting this means opening two separate, independent SSH sessions. 
In the local setting, this means opening one first session that is on the local tty (for the victim) and one second session that is either on another local tty or on an SSH session (for the attacker). Note that the input goes on the local tty.
3. Run the victim on the victim shell with `./bin/victim`.
4. Run the attacker on the other shell with `sudo ./bin/monitor 1 0`.
As soon as the attacker starts, type some characters on the victim shell, such as `password1230` (the `0` will terminate the string).
Once you see `# DONE, storing data` appear on the attacker side, it indicates the attacker has stopped. 
All keystrokes should be finished before that point (be quick!).
5. Prepare the plots by running `./venv/bin/python plot-keystroke-attack.py` (it takes a couple of minutes).

## Output

You can find the zoomed-out plot in the file `plot/plot.pdf` and the zoomed-in plots in the files `plot/plot-***.pdf`.
There is one zoomed-in plot per recorded keystroke event.

### My results do not look like the ones in the paper! What do I do?

If you do not get clean plots, you may have to adjust a few settings in the the script `plot-keystroke-attack.py`.

First, you may be using a non-optimal moving average `window` (line 194, look for the `FIXME` comment).
To find an optimal `window` value, we suggest to change the line `if (True): # runtime on the x axis` to an `if (False):` in line 133 (look for the `FIXME` comment) and look at the zoomed in plots.
How many samples does contention approximately last for in those zoomed-in plots?
Once you know that number (e.g., 2000) you can set the moving average `window` (line 194) to that value.

Second, you may have to adjust the `low_thres` and `high_thres` variables (line 63, look for the `FIXME` comment).
These variables are used to filter out outliers in the timing samples that may be due to noise.
It may be that your system needs a different threshold.

If even after adjusting both `window`, `low_thres` and `high_thres` you still see spurious peaks in the final `plot.pdf`, it may be either due to system noise or due to peculiarities of your machine's hardware or software configuration.
To better visualize the difference between spurious peaks and peaks that are due to keystrokes, you can change the `if (False):` statement to an `if (True):` in line 202 (look for the `FIXME` comment).
The script should then produce additional zoomed in plots named `plot-peak-***`.
These plots are for every detected peak (including the spurious ones) based on a heuristics called `mph` (i.e., minimum peak height, which you may have to tune manually to find an optimal value).
If you then compare the `plot-peak-***` plots of the real keystrokes with the ones of the spurious peaks, you should see that they look different.
That is how an attacker can tell that those spurious peaks are not due to keystrokes.

## Note

This proof of concept implementation uses root privileges to get the physical address in the slice mapping function.
However, as discussed in the paper, the slice mapping of an address can be computed with unprivileged access too (by using timing information).
That is, root access is not a requirement of the attack and is used in our implementation only for convenience.