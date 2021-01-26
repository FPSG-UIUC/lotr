import numpy as np
import matplotlib.pyplot as plt
import sys
import scipy.special as sc
import os


# Source: https://github.com/scipy/scipy/blob/master/doc/source/tutorial/special.rst
def binary_entropy(x):
    return -(sc.xlogy(x, x) + sc.xlog1py(1 - x, -x))/np.log(2)


def read_from_file(filename):
    raw_bitrate = []  # [0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 6.5, 7, 7.5, 8]
    errors = []

    # Results of all runs are all in the same file
    with open(filename) as f:
        for line in f:
            x, y = line.strip().split()
            raw_bitrate.append(float(x))
            errors.append(int(y))

    return raw_bitrate, errors


def main():
    # Prepare output directory
    out_dir = 'plot'
    try:
        os.makedirs(out_dir)
    except:
        pass

    input = read_from_file(sys.argv[1])
    raw_bitrate = input[0]      # Number of bits (in Mbps) that the sender transmits per second
    errors = input[1]            # Number of bit flips (from a receiver's standpoint) out of 1000 bits that the sender transmitted

    # Compute the probabilities of bit flip
    error_probability = []
    for i in range(len(errors)):
        prob = errors[i] / 1000
        error_probability.append(prob)

    # Compute the channel capacities
    capacity = []
    for i in range(len(raw_bitrate)):
        cap = raw_bitrate[i] * (1 - binary_entropy(error_probability[i]))
        capacity.append(cap)
        print("%.1f %.4f" % (raw_bitrate[i], cap))

    # Plot the channel capacity / error probability plot
    plt.rcParams["figure.figsize"] = (6.4, 2.2)
    fig, ax1 = plt.subplots()

    color = 'tab:blue'
    ax1.set_xlabel('Raw bandwidth (Mbps)')
    ax1.set_ylabel('Capacity (Mbps)', color=color)
    line1 = ax1.plot(raw_bitrate, capacity, color=color, marker="o", label="Capacity")
    ax1.tick_params(axis='y', labelcolor=color)

    ax2 = ax1.twinx()  # instantiate a second axes that shares the same x-axis

    color = 'tab:red'
    ax2.set_ylabel('Error probability', color=color)  # we already handled the x-label with ax1
    line2 = ax2.plot(raw_bitrate, error_probability, color=color, marker="X", label="Error probability", linestyle="--")
    ax2.tick_params(axis='y', labelcolor=color)

    lines = line2 + line1
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc="upper left", frameon=False)

    plt.grid()
    plt.tight_layout()
    plt.savefig("plot/capacity-plot.pdf")
    plt.clf()


if __name__ == "__main__":
    main()
