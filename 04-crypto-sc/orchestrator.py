from multiprocessing import Process
import os
from distutils.dir_util import remove_tree
from distutils.file_util import copy_file
import argparse
import numpy as np
import matplotlib.pyplot as plt
import subprocess
import statistics
import glob
import psutil
from sklearn.model_selection import train_test_split
from sklearn.svm import SVC


# -------------------------------------------------------------------------------------------------------------------
# Utility Functions
# -------------------------------------------------------------------------------------------------------------------
def moving_average(x, N):
    cumsum = np.cumsum(np.insert(x, 0, 0))
    return (cumsum[N:] - cumsum[:-N]) / float(N)


# 1-column file -> array of int
def parse_file_1c(fn):
    with open(fn) as f:
        lines = [int(line.strip()) for line in f]
    return np.array(lines)


# 1-column file -> array of int
def parse_file_2c(fn):
    lines = []
    with open(fn) as f:
        for line in f:
            first, second = line.strip().split()
            lines.append((int(first), int(second)))
    return np.array(lines)


# -------------------------------------------------------------------------------------------------------------------
# Plotting Functions
# -------------------------------------------------------------------------------------------------------------------
def plot_runs(left, right):
    low_thres = 50		# FIXME: change to other ranges if needed
    high_thres = 250	# FIXME: change to other ranges if needed

    # Read the 0 traces
    out_files = sorted(glob.glob(left + "/trace*"))
    trace_0_dict = {}
    trace_0 = []
    for f in out_files:
        samples = parse_file_1c(f)  # this array contains all the samples
        for i, t in enumerate(samples):
            if t >= low_thres and t <= high_thres:    # filter out outliers
                trace_0_dict.setdefault(i, []).append(t)

    # Compute average value of each sample
    for i, v in trace_0_dict.items():
        avg = np.mean(v)
        trace_0.append(avg)

    # Read the 1 traces
    out_files = sorted(glob.glob(right + "/trace*"))
    trace_1_dict = {}
    trace_1 = []
    for f in out_files:
        samples = parse_file_1c(f)  # this array contains all the samples
        for i, t in enumerate(samples):
            if t >= low_thres and t <= high_thres:    # filter out outliers
                trace_1_dict.setdefault(i, []).append(t)

    # Compute average value of each sample
    for i, v in trace_1_dict.items():
        avg = np.mean(v)
        trace_1.append(avg)

    # Change to True if you want to plot the moving average
    # FIXME: Turn on moving average
    if (False):
        window = 8
        trace_0_avg = moving_average(trace_0, window)
        trace_1_avg = moving_average(trace_1, window)
    else:
        trace_0_avg = trace_0
        trace_1_avg = trace_1

    # Prepare figure
    plt.figure(figsize=(6.4, 2))
    xmax = min(len(trace_0_avg), len(trace_1_avg)) - 1
    ymin = min(min(trace_0_avg), min(trace_1_avg))
    ymax = max(max(trace_0_avg), max(trace_1_avg))

    # Plot data to figure
    i = 1
    for samples in [trace_0_avg, trace_1_avg]:
        plt.subplot(1, 2, i)
        plt.plot(samples)
        plt.xlim(0, xmax)
        plt.ylim(ymin, ymax)
        plt.grid(True, which='both')

        plt.xlabel("Latency sample ID")
        if (i == 1):
            plt.title("Bit = 0")
            plt.ylabel('Load latency (cycles)')
        else:
            plt.title("Bit = 1")
        i += 1

    # Save figure to disk
    figname = 'plot/plot-side-channel.pdf'
    print('plotting', figname)
    plt.tight_layout()
    plt.savefig(figname)
    plt.close()


# -------------------------------------------------------------------------------------------------------------------
# Orchestrating Functions
# -------------------------------------------------------------------------------------------------------------------
def do_runs(iteration_number_1, directory_1, iteration_number_2, directory_2):
    if iteration_number_1 <= 0 or iteration_number_2 <= 0:
        print("Iteration number should be greater than 0")
        exit(0)

    # Delete previous output files
    prev_files = glob.glob("out/*.out")
    for x in prev_files:
        os.remove(x)

    # Run monitor (attacker) until it succeeds:
    monitor_err = 1
    trials = 0
    while (monitor_err != 0 and trials < 3):
        cl = [monitor_path, str(monitor_coreno), str(monitor_sliceno), str(no_victim_runs), str(iteration_number_1), str(iteration_number_2), str(cleansing_mechanism)]
        print(cl)
        monitor_popen = subprocess.Popen(cl)

        # Wait for monitor to complete (FIXME: tune timeout if necessary)
        try:
            monitor_err = monitor_popen.wait(timeout=600)
        except:
            print('monitor out of time')
            monitor_err = -1
        print('monitor returned %d.' % (monitor_err))

        trials += 1

    if (trials == 3):
        exit(0)

    # Save output into the desired directory
    remove_tree(directory_1)
    remove_tree(directory_2)
    os.makedirs(directory_1)
    os.makedirs(directory_2)

    files = glob.glob("out/*_1.out")
    for x in files:
        copy_file(x, directory_1)

    files = glob.glob("out/*_2.out")
    for x in files:
        copy_file(x, directory_2)


def parse_runs(directory, victim_iteration_no):
    # Create output directory
    output_dir_name = directory + "-parsed"
    try:
        os.makedirs(output_dir_name)
    except:
        print("[" + str(victim_iteration_no) + "]", "Errors creating -parsed directory (it may already exist)")
        pass

    # Prepare to read data from the experiments
    out_files = sorted(glob.glob(directory + "/*_data*.out"))
    trace_counter = 0
    all_lengths = []
    all_traces = []

    # Now process the results of the experiments
    for f in out_files[100:]:	# Exclude 100 warmup traces. FIXME: change this if needed

        # Parse trace number
        trace_number = f.split('/')[-1].split('_')[0]
        if (int(trace_number) % 500 == 0):
            print("[" + str(victim_iteration_no) + "]", 'Processing trace', trace_number)

        # Parse file
        samples = parse_file_1c(f)  # this array contains all the samples
        all_lengths.append(len(samples))
        all_traces.append(samples)

    # all_traces is a list of lists
    #   each sublist consists of a sequence of latencies [102, 99, 104, ...]
    print("[" + str(victim_iteration_no) + "]", "all_traces contains", len(all_traces), "traces")
    print("[" + str(victim_iteration_no) + "]", "example of samples from trace 0:", all_traces[0])

    # Find median number of samples
    median_length = statistics.median(all_lengths)
    lowerbound = max(int(median_length * 0.9), min(all_lengths))  # For EdDSA where there are more samples we can also add: int(np.percentile(all_lengths, 5)))
    print("[" + str(victim_iteration_no) + "]", "Median number of samples is", median_length, ". Keeping", lowerbound)

    # Write the iterations to disk (exclude the ones that are too short)
    for samples in all_traces:
        if len(samples) < lowerbound:
            print("[" + str(victim_iteration_no) + "]", "Excluding trace because bound is", lowerbound, "but this trace had length", len(samples))
            continue

        with open(output_dir_name + "/trace-" + str(trace_counter), "w") as f:
            for t in samples[:lowerbound]:
                print(t, file=f)

        trace_counter += 1

    print("[" + str(victim_iteration_no) + "]", "In total we kept", trace_counter, "traces")


def samples2vector(zeroset):
    threshold = np.mean(zeroset)
    s = np.std(zeroset)

    # This works with numpy and subtracts the mean from each item of the array
    # And then divides it by the standard deviation
    #
    # In a nutshell, this says how many standard deviations away from the mean each item is:
    # https://statisticsbyjim.com/glossary/standardization/
    return (zeroset - threshold)/s


def traintest(directory0, directory1):
    traces = []
    labels = []

    # Read the 0 traces
    out_files = sorted(glob.glob(directory0 + "/trace*"), reverse=True)
    for f in out_files[:2500]:
        samples = parse_file_1c(f)  # this array contains all the samples
        traces.append(samples)
        # traces.append(samples2vector(samples))     # This is to normalize the samples
        labels.append(0)

    print("Parsed files with label 0")

    # Read the 1 traces
    cut_at_len = len(traces[0])
    out_files = sorted(glob.glob(directory1 + "/trace*"), reverse=True)
    printed = 1
    for f in out_files[:2500]:
        samples = parse_file_1c(f)  # this array contains all the samples
        if (printed):
            print("0 len = ", cut_at_len, "\n1 len =", len(samples))
            printed = 0
        traces.append(samples[:cut_at_len])
        # traces.append(samples2vector(samples[:cut_at_len]))    # This is to normalize the samples
        labels.append(1)

    print("Parsed files with label 1")
    print("In total, we have %d samples" % (len(traces)))

    # Split
    X_train, X_test, y_train, y_test = train_test_split(traces, labels)
    print("Shape of train is", np.array(X_train).shape)
    print("Shape of test is", np.array(X_test).shape)

    # Try different SVM classifiers
    # The larger the C, the more it tries to avoid misclassifying training set
    classifiers = [
        ("Linear SVM C=0.001", SVC(kernel="linear", C=0.001)),
        ("Linear SVM C=0.025", SVC(kernel="linear", C=0.025)),
        ("Linear SVM C=0.1", SVC(kernel="linear", C=0.1)),
        ("Linear SVM C=0.2", SVC(kernel="linear", C=0.2)),
        ("Linear SVM C=0.5", SVC(kernel="linear", C=0.5)),
        ("Linear SVM C=0.75", SVC(kernel="linear", C=0.75)),
        ("RBF SVM C=0.1", SVC(kernel="rbf", C=0.1)),
        ("RBF SVM C=0.2", SVC(kernel="rbf", C=0.2)),
        ("RBF SVM C=0.5", SVC(kernel="rbf", C=0.5)),
        ("RBF SVM C=0.75", SVC(kernel="rbf", C=0.75)),
        ("RBF SVM C=1", SVC(kernel="rbf", C=1)),
        ("RBF SVM C=10", SVC(kernel="rbf", C=10)),
        ("RBF SVM C=50", SVC(kernel="rbf", C=50)),
        ("RBF SVM C=70", SVC(kernel="rbf", C=70)),
        ("RBF SVM C=100", SVC(kernel="rbf", C=100)),
        ("RBF SVM C=200", SVC(kernel="rbf", C=200)),
        ("RBF SVM C=500", SVC(kernel="rbf", C=500)),
        ("RBF SVM C=1000", SVC(kernel="rbf", C=1000)),
        ("RBF SVM C=10000", SVC(kernel="rbf", C=10000))
        # ("SVM Poly", SVC(kernel="poly"))  # slow, even if sometimes it is better
    ]

    # Iterate over classifiers (for now using only SVM, can switch it back later)
    print("Training classifier. Wait (a few minutes).")
    best_score = 0
    best_classifier = ""
    for name, model in classifiers:
        model.fit(X_train, y_train)
        score = model.score(X_test, y_test)     # This computes the accuracy
        print(name, score)
        if score > best_score:
            best_score = score
            best_classifier = name

    print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
    print("Final classifier =", best_classifier)
    print("Accuracy =", best_score)
    print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~")


# -------------------------------------------------------------------------------------------------------------------
# Miscellaneous
# -------------------------------------------------------------------------------------------------------------------
def is_process_running(processName):
    # Iterate over the all the running process
    for proc in psutil.process_iter():
        try:
            # Check if process name contains the given name string.
            if processName in proc.cmdline():
                return True
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            pass

    # print("Run the victim in the background first")
    return False


# -------------------------------------------------------------------------------------------------------------------
# This is the orchestrator
# -------------------------------------------------------------------------------------------------------------------
if __name__ == '__main__':

    # Set configuration
    monitor_path = './bin/ringbus-monitor'

    monitor_coreno = 2      # 2 is best for the full cache flush; 5 for the L1/L2 flush only
    monitor_sliceno = 1     # 1 is best for the full cache flush; 4 for the L1/L2 flush only

    cleansing_mechanism = 1      # 1 for clflush the address space; 2 for L1 and L2 flushing

    eddsa_path = './victim/libgcrypt-1.6.3/tests/ringbus-victim'            # EdDSA
    rsa_path = './victim/libgcrypt-1.5.2/tests/ringbus-victim'            # RSA

    victim_path = rsa_path
    if (is_process_running(rsa_path)):
        victim_path = rsa_path
    elif (is_process_running(eddsa_path)):
        victim_path = eddsa_path
    else:
        print("Neither victim is running. Assuming", victim_path)

    # Parse arguments
    parser = argparse.ArgumentParser(description='Orchestrate monitor and victim execution.')
    parser.add_argument('--collect', type=int, default=0)
    parser.add_argument('--parse', action='store_true')
    parser.add_argument('--train', nargs=2)
    parser.add_argument('--plot', nargs=2)
    args = parser.parse_args()
    no_victim_runs = args.collect

    # Prepare output directories
    try:
        os.makedirs('data')
    except:
        pass
    try:
        os.makedirs('data/eddsa-0')
    except:
        pass
    try:
        os.makedirs('data/eddsa-1')
    except:
        pass
    try:
        os.makedirs('data/rsa-0')
    except:
        pass
    try:
        os.makedirs('data/rsa-1')
    except:
        pass
    try:
        os.makedirs('plot')
    except:
        pass

    if victim_path == eddsa_path:
        directory5 = 'data/eddsa-0'
        directory6 = 'data/eddsa-1'
    elif victim_path == rsa_path:
        directory5 = 'data/rsa-1'
        directory6 = 'data/rsa-0'

    # Run the orchestrator
    if args.collect:
        do_runs(5, directory5, 6, directory6)

    if args.parse:

        # Remove old directories
        if victim_path == eddsa_path:
            try:
                remove_tree('data/eddsa-0-parsed')
            except:
                pass
            try:
                remove_tree('data/eddsa-1-parsed')
            except:
                pass
        elif victim_path == rsa_path:
            try:
                remove_tree('data/rsa-0-parsed')
            except:
                pass
            try:
                remove_tree('data/rsa-1-parsed')
            except:
                pass

        # Parse collected data
        p1 = Process(target=parse_runs, args=(directory5, 5))
        p2 = Process(target=parse_runs, args=(directory6, 6))
        p1.start()
        p2.start()
        p1.join()
        p2.join()

    # Plot parsed data
    if args.plot:
        plot_runs(args.plot[0], args.plot[1])

    # Train and test classifier on the data
    if args.train:
        traintest(args.train[0], args.train[1])
