import sys


def main():
    # Parse args
    assert len(sys.argv) == 3, "Specify the CPU frequency in GHz and the desired bitrate in Mbps"
    proc_frequency = float(sys.argv[1]) * 10**9   # e.g., 3*10^9
    goal_Mbps = float(sys.argv[2])                # e.g., 1

    # Compute the interval that the channel needs to use to achieve the desired bitrate
    need_interval = int(proc_frequency / (goal_Mbps * 10**6))
    print(need_interval, end="")


if __name__ == "__main__":
    main()
