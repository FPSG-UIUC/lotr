import numpy as np
import matplotlib.pyplot as plt
import sys
import os


def read_from_file(filename):
    times = []
    with open(filename) as f:
        for line in f:
            val = int(line.strip())
            if (val <= 300):
                times.append(val)

    return times


def plot_distributions(*args):
    plt.figure(figsize=(12.8, 3.8))

    lower_bound = 10000
    upper_bound = 0
    minimum = 10000
    maximum = 0
    datas = []
    labels = []

    # Each arg is in the format [data, label]
    for data, label in args:

        # Store data for bins
        minimum = min(min(data), minimum)
        maximum = max(max(data), maximum)

        # Store data for bars
        datas.append(data)
        labels.append(label)

    # Plot all data
    bins = np.arange(minimum, maximum)
    normalized_counts, bins, _ = plt.hist(datas, bins=bins, alpha=0.5, label=labels, density=True, align="left", edgecolor="black")

    if len(args) < 2:
        normalized_counts = [normalized_counts]

    # Find a reasonable lower bound for the x axis
    for single_counts in normalized_counts:
        for i, count in enumerate(single_counts):
            if count > 0.005:
                low = bins[i] - 1
                lower_bound = min(lower_bound, low)
                break

    # Find a reasonable upper bound for the x axis
    for single_counts in normalized_counts:
        for i, count in reversed(list(enumerate(single_counts))):
            if count > 0.005:
                high = bins[i] + 1
                upper_bound = max(upper_bound, high)
                break

    # Set x range
    plt.xticks(np.arange(lower_bound, upper_bound, 5))
    plt.xlim(lower_bound, upper_bound)

    # Show grid
    plt.grid(axis='y', alpha=0.75)

    # Set labels
    plt.xlabel('Cycles')
    plt.ylabel('Probability Density')

    # Save plot to file
    plt.legend()
    plt.tight_layout()
    plt.savefig("./plot/distributions.pdf")
    plt.clf()


def main():
    # Prepare output directory
    out_dir = 'plot'
    try:
        os.makedirs(out_dir)
    except:
        pass

    # Parse args
    data = []
    for arg in sys.argv[1:]:
        filename = arg
        data.append((read_from_file(filename), filename))

    plot_distributions(*data)


if __name__ == "__main__":
    main()
