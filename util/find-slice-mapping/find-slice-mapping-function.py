import sys
import itertools


def cp(string1, string2):
    count_diffs = 0
    l = len(string1)
    diff = -1

    # Scan addresses from high order bit (35) to bit 6 (which is the cache line offset)
    for i in range(l-6):

        # If a bit is different and it's the first, count it
        # If a bit is different and it's not the first, return -1
        if string1[i] != string2[i]:
            if count_diffs:
                return -1
            else:
                count_diffs += 1
            diff = l-1-i

    # Return the location of the different bit (starting from bit 0 as the rightmost)
    return diff


# Open the file with "phys_address slice" on each line
res = []
sl = []
with open(sys.argv[1]) as f:
    i = 0

    # Store the address and slice of each pair in res and sl
    for line in f:
        i += 1
        addr, slice_id = line.split()
        res.append(addr)
        sl.append(int(slice_id))

        # Stop at 50000 otherwise it takes forever
        if (i == 10000):
            break

# Get all combinations of the stored addresses
d = {}
for combination in itertools.combinations(enumerate(res), 2):
    addr1 = combination[0][1]
    addr2 = combination[1][1]

    # Check if they differ by only 1 bit
    bit = cp(addr1, addr2)

    # If they do differ by only 1 bit, save them in d
    if (bit >= 0):
        index1 = combination[0][0]
        index2 = combination[1][0]
        slice_id1 = sl[index1]
        slice_id2 = sl[index2]

        d.setdefault(bit, []).append([addr1, slice_id1, addr2, slice_id2])
        # print(bit)
        # print(addr1, slice_id1)
        # print(addr2, slice_id2)
        # print()

# Print address and slice for the addresses where we had only 1 bit difference
for key in sorted(d.keys()):
    print(key)
    for s in d[key][:4]:
        addr1, slice_id1, addr2, slice_id2 = s
        print(addr1, slice_id1)
        print(addr2, slice_id2)
        print()
