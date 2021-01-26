import numpy as np
import matplotlib.pyplot as plt
import sys
import os
import numpy as np


num_cores = 4
num_slices = 4


def moving_average(x, N):
    cumsum = np.cumsum(np.insert(x, 0, 0))
    return (cumsum[N:] - cumsum[:-N]) / float(N)


def plot_distributions(core_no, slice_no, set_ID):
    print("core %d slice %d" % (core_no, slice_no))

    plt.figure(figsize=(20.4, 2.4))

    plt.xlabel('Sample ID')
    plt.ylabel('Latency (cycles)')

    data = []
    i = 0
    with open("./out/core%02d_slice%02d_set%04d.out" % (core_no, slice_no, set_ID)) as f:

        for line in f:
            val = int(line.strip())
            data.append(val)

            i += 1
            if (i == 70000):
                break

    window = 50
    ma = moving_average(data[50000:], window)
    plt.plot(ma)

    # Save plot to file
    plt.ylim(np.median(ma) - 2, np.median(ma) + 2)
    plt.tight_layout()
    plt.savefig("./plot/time_series_core%02d_slice%02d_set%04d.png" % (core_no, slice_no, set_ID))
    plt.clf()
    plt.close()


def parse_distributions(set_ID):

    # For each core
    for core_no in range(num_cores):

        # For each slice
        for slice_no in range(num_slices):

            plot_distributions(core_no, slice_no, set_ID)


def main():
    # Prepare output directory
    out_dir = 'plot'
    try:
        os.makedirs(out_dir)
    except:
        pass

    # Check arguments
    if (len(sys.argv) > 1):
        set_ID = int(sys.argv[1])
    else:
        set_ID = 5

    parse_distributions(set_ID)


if __name__ == "__main__":
    main()
