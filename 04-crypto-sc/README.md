# Side channel against cryptographic code

The code in this folder implements a side channel attack that leverages contention on the ring interconnect.
The `attacker` collects a number of latency samples while the `victim` runs a vulnerable cryptographic function.
These samples are then used to infer key bits.

## Preliminaries

- Build the attacker with `make`.
- From the `victim` directory, clone, patch and build the RSA victim as follows:
```sh
# Clone official libgcrypt
git clone --depth 1 --branch libgcrypt-1.5.2 https://github.com/gpg/libgcrypt.git libgcrypt-1.5.2

# Apply our patch (use --dry-run to check before doing this)
patch -p 3 -i ./libgcrypt-1.5.2.patch

# Compile the victim
# You may have to apt install dependencies (e.g., libpgp-error-dev, fig2dev, texinfo)
cd libgcrypt-1.5.2
automake --add-missing		# To fix a bug with libgcrypt 1.5.2
./autogen.sh && ./configure --enable-maintainer-mode && make -j`nproc`
```
- From the `victim` directory, clone, patch and build the EdDSA victim as follows:
```sh
# Clone official libgcrypt
git clone --depth 1 --branch libgcrypt-1.6.3 https://github.com/gpg/libgcrypt.git libgcrypt-1.6.3

# Apply our patch (use --dry-run to check before doing this)
patch -p 3 -i ./libgcrypt-1.6.3.patch

# Compile the victim
# You may have to apt install dependencies (e.g., libpgp-error-dev, fig2dev, texinfo)
cd libgcrypt-1.6.3
./autogen.sh && ./configure --enable-maintainer-mode && make -j`nproc`
```
- We recommend that you run the **`../run-first.sh`** script before taking timing measurements.
This script disables hyperthreading, enables the prefetchers (see paper), enables some hugepages (just in case they were disabled in your system), and fixes the CPU frequency.
- Set up a Python virtual environment as follows: 
```sh
virtualenv -p python3 venv
source venv/bin/activate
pip install matplotlib
pip install psutil
pip install sklearn
deactivate
```

## Run

Make sure that your system is idle and minimize the number of background processes that are running and may add noise to the experiment.
Then, to reproduce the results of the paper, follow the steps below.
Note that you will use a script called `orchestrator.py`, that is responsible for orchestrating the victim and the attacker/monitor processes and carrying out the attack.

1. Make sure that victims and monitor are compiled (see build instructions above).
2. Make sure you are in the `04-crypto-sc` directory.
3. Make sure that no other victim is running (if you ran the attack before, see step 7 below).
4. Start **one** victim in the background with `sudo ./victim/libgcrypt-1.5.2/tests/ringbus-victim &` (for the RSA victim) or `sudo ./victim/libgcrypt-1.6.3/tests/ringbus-victim &` (for the EdDSA victim).
Once started, the victim will wait for the attacker to request a decryption.
5. Run the orchestrator like this: for RSA, `sudo ./venv/bin/python orchestrator.py --collect 300 --parse --plot data/rsa-0-parsed data/rsa-1-parsed`; for EdDSA, `sudo ./venv/bin/python orchestrator.py --collect 300 --parse --plot data/eddsa-0-parsed data/eddsa-1-parsed`.
You can change the number of repetitions by changing the number after the flag `--collect`.
6. The results (plot of the paper) will be in the `plot` subdirectory.
7. Make sure to terminate ringbus-victim with `sudo pkill -f ringbus-victim` at the end (before you, e.g., repeat on another victim).

For the ml part, repeat the above but collecting more samples (e.g., `3000`).
It should take less than 5 minutes to run with `3000` repetitions.
Finally, train and test the model on the parsed data with `sudo ./venv/bin/python orchestrator.py --train data/rsa-0-parsed data/rsa-1-parsed` (for RSA) or `sudo ./venv/bin/python orchestrator.py --train data/eddsa-0-parsed data/eddsa-1-parsed` (for EdDSA).
This script should also take less than 5 minutes, and will print the accuracy of the chosen classifier.

### Extra Notes

In our experiment, `ringbus-victim` always uses the same cryptographic key. 
Additionally, `ringbus-monitor` always monitors the same two fixed iterations of the victim's loop, one where we know bit=0 and one where we know bit=1.
This is done to prepare the plots in paper, visualize the difference between bit=0 and bit=1, and collect the datasets for the classifier.
If you want, however, you can change the victim's cryptographic key and the loop iterations that the monitor targets (see also the orchestrator).
Also, if you need to see the ground truth for any other configuration you may choose to monitor, you can uncomment the respective lines in `lotr.c` (look for the `FIXME` comments).

## Output

You can find the output of the experiment in the file `plot/plot-side-channel.pdf`.

### My results do not look like the ones in the paper! What do I do?

Some variance (both in the plots and in the classifier accuracy) is expected due to noise in the collected data and/or differences in the hardware/software.
To try to reduce this variance, we recommend the following steps.

First, double-check that your system is idle and that you minimized the number of background processes.
Second, make sure that you only have one `ringbus-victim` and no `ringbus-monitor` processes running before you start the orchestrator.
Third, to make the plots look cleaner, we recommend to plot the moving average instead of the raw trace.
You can do this by changing the `if (False):` to an `if (True):` in line 79 (look for the `FIXME` comment).
Fourth, you can try to tune the variables `low_thres` and `high_thres` in `orchestrator.py` (look for the `FIXME` comments).
Also, note that it may take a couple of runs for the results to stabilize.

## Note

This proof of concept implementation uses root privileges to get the physical address in the slice mapping function.
However, as discussed in the paper, the slice mapping of an address can be computed with unprivileged access too (by using timing information).
That is, root access is not a requirement of the attack and is used in our implementation only for convenience.