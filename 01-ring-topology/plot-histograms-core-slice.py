import matplotlib.pyplot as plt
import numpy as np
import sys
import os


################################
# This script parses the outputs of measure-slice-latency (or run-all-latency.sh) for each core and each slice
# NOTE: This script is CPU specific

num_cores = 4
num_slices = 4


def plot_distributions(set_ID):
    _, axs = plt.subplots(1, 4, figsize=(6.4, 1.6))     # rows, columns (change this if testing on CPUs with more core-slice combinations)

    y_bottom = 1000
    y_top = 0

    # For each core
    for core_no in range(num_cores):
        boxplot_data = []
        boxplot_labels = []

        print("")

        # For each slice
        for slice_no in range(num_slices):
            boxplot_data.append([])

            less_than = 0
            more_than = 0
            with open("./out/core%02d_slice%02d_set%04d.out" % (core_no, slice_no, set_ID)) as f:

                # i = 0
                for line in f:
                    # i += 1
                    # if (i == 100):
                    #     break

                    val = int(line.strip())

                    # Exclude the L1/L2/DRAM accesses (heuristically measured thresholds)
                    if (val >= 38 and val <= 100):
                        boxplot_data[slice_no].append(val)
                    elif val < 38:
                        less_than += 1
                    else:
                        more_than += 1

            # Print info about these (optional)
            print("core %d slice %d (%d less; %d more)" %
                            (core_no, slice_no, less_than, more_than))
            print('%10s %10s %10s %10s %10s' %
                            ("MIN", "25 PERC", "MEDIAN", "75 PERC", "MAX"))
            print('%10.3f %10.3f %10.3f %10.3f %10.3f' % (min(boxplot_data[slice_no]), np.percentile(boxplot_data[slice_no], 25), np.percentile(
                boxplot_data[slice_no], 50), np.percentile(boxplot_data[slice_no], 75), max(boxplot_data[slice_no])))

            # print(boxplot_data[slice_no][:10])
            boxplot_labels.append("S" + str(slice_no))

        # Add plot to figure
        # axs[core_no].set_ylim([70, 106])
        axs[core_no].boxplot(boxplot_data, 0, labels=boxplot_labels, showfliers=False)
        # if (core_no // 4 == 0):
        #     axs[core_no].get_xaxis().set_visible(False)
        # if (core_no % 4 != 0):
        #     axs[core_no].get_yaxis().set_visible(False)
        # axs[core_no].grid(axis='y')
        axs[core_no].set_title('Core ' + str(core_no))

        new_bottom, new_top = axs[core_no].get_ylim()
        if (new_bottom < y_bottom or new_top > y_top):
            y_bottom = min(y_bottom, new_bottom)
            y_top = max(y_top, new_top)

    for core_no in range(num_cores):
        axs[core_no].set_ylim([y_bottom, y_top])

    # Store figure
    plt.tight_layout()
    plt.savefig("./plot/per-core.pdf")
    plt.clf()


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

    plot_distributions(set_ID)


if __name__ == "__main__":
    main()
