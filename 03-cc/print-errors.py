import sys


def read_from_file(filename):
    result_x = []
    result_y = []

    # Results of all runs are all in the same file
    with open(filename) as f:
        for line in f:
            x, y = line.strip().split()
            result_x.append(int(x))
            result_y.append(int(y))

    return result_x, result_y


def diff_letters(a, b):
    return sum(a[i] != b[i] for i in range(len(a)))


def parse_intervals_into_bits(intervals, threshold):

    # Parse the intervals into bits
    # If more than 30% of the measurements in an interval
    # are greater than the threshold, then we classify
    # the interval as a 1.
    result = ""
    for _, samples in intervals.items():
        contention = 0
        for sample in samples:
            if sample >= threshold:
                contention += 1
        if (contention > 0.3 * len(samples)):		# FIXME: change this heuristic
            result += "1"
        else:
            result += "0"

    return result


def main():
    # Check that there is exactly 1 argument
    assert len(sys.argv) == 3, "Specify the files with the results as argument, and the interval"
    result_x, result_y = read_from_file(sys.argv[1])

    interval = int(sys.argv[2])

    # Parse trace into intervals
    cur_interval_no = 0
    train_intervals = {}
    test_intervals = {}
    for i in range(len(result_x)):
        x = result_x[i]
        y = result_y[i]
        if (x > interval * (cur_interval_no + 1)):
            cur_interval_no += 1

        # The first 100 intervals are discarded
        # The following 900 intervals are the training set
        # The following 1000 intervals are the testing set
        if (cur_interval_no < 100):
            continue
        if (100 <= cur_interval_no < 1000):
            train_intervals.setdefault(cur_interval_no, []).append(y)
        elif (1000 <= cur_interval_no < 2000):
            test_intervals.setdefault(cur_interval_no, []).append(y)
        else:
            break

    # We use the training set to find the best threshold
    # for this interval (offline) and the remaining parsed data
    # to get the error rate (online). We try different thresholds
    # because, depending on the CC interval, the best threshold
    # to use is different (e.g., with larger intervals we can use a
    # lower threshold than with shorter intervals).
    thresholds = range(140, 240)	# FIXME: adjust these thresholds for your CPU
    best_threshold = (1, 500)	# threshold, score
    for threshold in thresholds:

        # Parse the intervals into bits
        result = parse_intervals_into_bits(train_intervals, threshold)

        # Compare these bits with the ground truth
        # The ground truth is either 0101... or 1010...
        candidate_1 = "01" * (len(result) // 2)
        candidate_2 = "10" * (len(result) // 2)

        # Get the number of bit flips between the
        # decoded stream and the (correct) ground truth
        score_1 = diff_letters(result, candidate_1)
        score_2 = diff_letters(result, candidate_2)
        score = min(score_1, score_2)
        if (score <= best_threshold[1]):
            best_threshold = (threshold, score)

    # Parse the intervals into bits
    result = parse_intervals_into_bits(test_intervals, best_threshold[0])

    # Print the decoded string
    # print(result)

    # Compare these bits with the ground truth
    # The ground truth is either 0101... or 1010...
    # NOTE: we tried with other bit patterns too and
    # the results were the same
    candidate_1 = "01" * (len(result) // 2)
    candidate_2 = "10" * (len(result) // 2)

    # Print the number of bit flips between the
    # decoded stream and the (correct) ground truth
    score_1 = diff_letters(result, candidate_1)
    score_2 = diff_letters(result, candidate_2)
    score = min(score_1, score_2)
    print(score)


if __name__ == "__main__":
    main()
