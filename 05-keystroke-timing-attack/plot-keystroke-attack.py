import numpy as np
import matplotlib.pyplot as plt
import os
from distutils.dir_util import remove_tree
from detecta import detect_peaks


def moving_average(x, N):
    cumsum = np.cumsum(np.insert(x, 0, 0))
    return (cumsum[N:] - cumsum[:-N]) / float(N)


# 1-column file -> array of int
def parse_file_1c(fn):
    with open(fn) as f:
        lines = [int(line.strip()) for line in f]
    return np.array(lines)


# 2-column file -> two arrays of int
def parse_file_2c(fn):
    first = []
    second = []
    with open(fn) as f:
        for line in f:
            a, b = line.strip().split()
            first.append(int(a))
            second.append(int(b))
    return np.array(first), np.array(second)


def parse_runs(directory='out'):

    # Prepare clean output directory
    try:
        remove_tree('plot')
    except:
        pass
    out_dir = 'plot'
    try:
        os.makedirs(out_dir)
    except:
        pass

    # Parse output files
    print("[+] Reading input files")
    all_samples_x, all_samples = parse_file_2c("./" + directory + "/timing-samples.out")  # contains all the measurements from the monitor
    keystroke_cycles = parse_file_1c("./" + directory + "/keystroke-cycles.out")  # contains the ground truth

    # Find where keystrokes happened in the trace
    i = 0
    all_keystrokes = []
    for keystroke_cycle in keystroke_cycles:
        while i < len(all_samples_x) - 1 and not (all_samples_x[i] <= keystroke_cycle and all_samples_x[i + 1] > keystroke_cycle):
            i += 1
        if i == len(all_samples_x) - 1:
            break
        all_keystrokes.append(i)

    print("[+] Preparing plots zoomed-in around keystroke events")
    delta = 6200		# FIXME: change to see more/less zoomed-in plots
    low_thres = 120		# FIXME: change to other ranges if needed
    high_thres = 250	# FIXME: change to other ranges if needed

    # Print how the trace looks like around keystrokes
    for index, keystroke in enumerate(all_keystrokes):

        # Get measurements around the keystroke event
        samples = all_samples[keystroke - delta: keystroke + delta]

        # Take the x values that we need for this zoomed-in plot
        samples_x = []
        for sample in all_samples_x[keystroke - delta: keystroke + delta]:
            samples_x.append(sample)
        samples_x = np.array(samples_x)

        # Shift the x axis so that it starts from 0
        prev = samples_x[0]
        first = np.float64(0)
        for i in range(len(samples_x)):

            # Deal with overflowing the uint64_t cycle counter
            if samples_x[i] < prev:
                toadd = (2**64-1) - prev
                toadd += samples_x[i]
            else:
                toadd = samples_x[i] - prev

            prev = samples_x[i]
            samples_x[i] = first + toadd
            first = samples_x[i]

        # Filter out the outliers
        todelete = []
        for i in range(len(samples)):
            if samples[i] < low_thres or samples[i] > high_thres:
                # print("id=", i, "x=", samples_x[i], "timing=", samples[i])
                todelete.append(i)

        samples = np.delete(samples, todelete)
        samples_x = np.delete(samples_x, todelete)

        # Plot the zoomed-in plot
        plt.figure(figsize=(6.4, 3.1))

        # Plot raw data
        plt.subplot(2, 1, 1)
        lower = min(samples)
        upper = max(samples)
        plt.ylim(lower, upper)
        plt.axvline(x=samples_x[delta], color='r', linewidth=1)     # Mark keystroke event
        plt.xlim(0, samples_x[-1])
        plt.tick_params(
            axis='x',          # changes apply to the x-axis
            which='both',      # both major and minor ticks are affected
            bottom=False,      # ticks along the bottom edge are off
            top=False,         # ticks along the top edge are off
            labelbottom=False)  # labels along the bottom edge are off
        plt.ylabel("Latency (cycles)")
        plt.grid(which='both')
        plt.plot(samples_x, samples, label='raw data', linewidth=0.5)
        plt.legend()

        # Plot moving average
        plt.subplot(2, 1, 2)
        window = 50
        ma = moving_average(samples, window)
        lower = min(ma)  # 160
        upper = max(ma)  # min(ma + 20) # 180
        plt.ylim(lower, upper)
        plt.ylabel("Latency (cycles)")
        plt.grid(which='both')  # plt.grid(axis='y')
        if (True):	# FIXME: toggle runtime on the x axis
            plt.xlabel("Time (cycles)")
            plt.axvline(x=samples_x[delta], color='r', linewidth=1)     # Mark keystroke event
            plt.xlim(0, samples_x[-1])
            plt.ticklabel_format(style='sci', axis='x', scilimits=(0, 0))
            plt.plot(samples_x[:len(ma)], ma, label='moving average ' + str(window), linewidth=0.5)
        else:
            plt.xlabel("Sample ID")
            plt.plot(ma, label='moving average ' + str(window), linewidth=0.5)
        plt.legend()

        # # Try to detect peaks automatically
        # peaks = detect_peaks(ma, mph=168.6, mpd=20)
        # for peak in peaks:
        #    plt.axvline(x=samples_x[peak], color='C2', alpha=.5)

        # Save figure to disk
        figname = (out_dir + '/plot-%03d.pdf') % (index)  # 'plot/plot' + number + '-%03d.pdf' % (index)
        print('[+] plotting', figname)
        plt.tight_layout()
        plt.savefig(figname)
        plt.close()

    # Now prepare to plot the zoomed-out version of the plot
    # Filter out measurements before the first and after the last keystroke
    startfrom = all_keystrokes[0] - 1000000
    endat = all_keystrokes[-2] + 1000000
    samples = all_samples[startfrom: endat]

    # Shift the x axis so that it starts from 0
    all_samples_x = all_samples_x[startfrom: startfrom + len(samples)]
    prev = all_samples_x[0]
    first = np.float64(0)
    for i in range(len(all_samples_x)):

        # Deal with overflowing the uint64_t cycle counter
        if all_samples_x[i] < prev:
            toadd = (2**64-1) - prev
            toadd += all_samples_x[i]
        else:
            toadd = all_samples_x[i] - prev

        prev = all_samples_x[i]
        all_samples_x[i] = first + toadd
        first = all_samples_x[i]

    # Get the shifted x value of the keystroke events
    final_keystrokes = []
    for i in all_keystrokes[:-1]:
        final_keystrokes.append(all_samples_x[i - startfrom])

    # Filter out the outliers
    todelete = []
    for i in range(len(samples)):
        if samples[i] < low_thres or samples[i] > high_thres:
            todelete.append(i)

    samples = np.delete(samples, todelete)
    all_samples_x = np.delete(all_samples_x, todelete)

    # Compute the moving average of the measurements
    window = 3000  # 1750    # FIXME: change to smaller value for tty
    ma = moving_average(samples, window)
    lower = min(ma)
    upper = max(ma)

    # If the zoomed-out plot shows peaks that are not associated with a keystroke, then those peaks
    # can be visualized with the code below. These peaks should look different than the keystroke
    # ones. FIXME: Change to True if needed.
    if (False):

        # Detect all peaks (not just keystroke ones) so that we can see how the false positives look like too
        peaks = detect_peaks(ma, mph=166, mpd=20)
        i = 0
        index = 0
        while True:
            if i >= len(peaks):
                break

            # Look only at windows of 10,000 measurements where there are enough detected peaks
            # This is a heuristic to exclude the windows that are not interesting
            current = peaks[i]
            isbigpeak = 1
            j = 1
            while (i + j) < len(peaks) and peaks[i + j] - current < 10000:
                j += 1

            if j < 10:
                isbigpeak = 0
                i += 1

            if isbigpeak == 1:

                # Code to print how the peaks look like
                figname = 'plot/plot-peak-%03d.pdf' % (index)
                index += 1

                tmpma = moving_average(samples[peaks[i+j//2] - delta: peaks[i+j//2] + delta], 50)
                tmplower = min(tmpma)
                tmpupper = max(tmpma)
                plt.figure(figsize=(6.4, 2))
                plt.axvline(x=delta, color='r', linewidth=2)
                plt.ylim(tmplower, tmpupper)
                plt.ylabel('cycles')
                plt.grid(True, which='both')
                plt.plot(tmpma, label='moving average ' + str(window), linewidth=0.5)

                print('[+] plotting', figname)
                plt.tight_layout()
                plt.savefig(figname)
                plt.close()

                i += j

    # Prepare the zoomed-out plot with all keystrokes
    print("[+] Preparing final plot")
    plt.figure(figsize=(6.4, 2))
    plt.xlim(0, all_samples_x[len(ma) - 1])
    plt.ylim(lower - 0.8, upper)
    plt.ylabel("Load latency (cycles)")
    plt.xlabel("Time (cycles)")

    # Add keystroke events to the zoomed-out figure
    characters = "password123000000000000000000000000000000000000000000000000000000000000000000"
    for i, keystroke in enumerate(final_keystrokes):
        plt.axvline(x=keystroke, color='y', alpha=.2, linewidth=5)
        plt.annotate(characters[i], (keystroke, lower - 0.5), fontsize=7, ha='center')

    # Add moving average of timing trace to the zoomed-out figure
    plt.plot(all_samples_x[: len(ma)], ma, label='moving average ' + str(window), linewidth=0.5)
    plt.legend(loc="upper right", fontsize=7)

    # Save figure to disk
    figname = out_dir + '/plot.pdf'
    print('[+] plotting', figname)
    plt.tight_layout()
    plt.savefig(figname)
    plt.close()


if __name__ == '__main__':
    parse_runs()
