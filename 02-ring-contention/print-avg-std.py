import numpy as np
import sys


def read_from_file(filename):
    times = []
    with open(filename) as f:
        for line in f:
            val = int(line.strip())
            if (val <= 300):
                times.append(val)

    return times


def main():
    data = read_from_file(sys.argv[1])
    # print("%d +/- %.2f" % (np.median(data), np.std(data)), end="")
    print("%.2f +/- %.2f" % (np.mean(data), np.std(data)), end="")


if __name__ == "__main__":
    main()
