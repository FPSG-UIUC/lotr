#include "../util/util.h"
#include <sys/resource.h>

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */

int main(int argc, char **argv)
{
	int i;

	// Check arguments
	if (argc != 4) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice ID, and set ID!\n");
		fprintf(stderr, "Enter: %s <core_ID> <slice_ID> <set_ID>\n", argv[0]);
		exit(1);
	}

	// Parse core ID
	int core_ID;
	sscanf(argv[1], "%d", &core_ID);
	if (core_ID > NUMBER_OF_LOGICAL_CORES - 1 || core_ID < 0) {
		fprintf(stderr, "Wrong core! core_ID should be less than %d and more than 0!\n", NUMBER_OF_LOGICAL_CORES);
		exit(1);
	}

	// Parse slice number
	int slice_ID;
	sscanf(argv[2], "%d", &slice_ID);
	if (slice_ID > LLC_CACHE_SLICES - 1 || slice_ID < 0) {
		fprintf(stderr, "Wrong slice! slice_ID should be less than %d and more than 0!\n", LLC_CACHE_SLICES);
		exit(1);
	}

	// Parse set number
	int set_ID;
	sscanf(argv[3], "%d", &set_ID);
	if (set_ID > LLC_CACHE_SETS_PER_SLICE - 1 || set_ID < 0) {
		fprintf(stderr, "Wrong set! set_ID should be less than %d and more than 0!\n", LLC_CACHE_SETS_PER_SLICE);
		exit(1);
	}

	// Prepare output filename
	int len = strlen("./out/core10_slice10_set2048.out") + 1;
	char *output_filename = malloc(len);
	snprintf(output_filename, len, "./out/core%02d_slice%02d_set%04d.out", core_ID, slice_ID, set_ID);
	FILE *output_file = fopen(output_filename, "w");
	if (output_file == NULL) {
		perror("output file");
	}

	// Allocate large buffer (pool of addresses)
	void *buffer = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	// Write data to the buffer so that any copy-on-write
	// mechanisms will give us our own copies of the pages.
	memset(buffer, 0, BUF_SIZE);

	// Pin the monitoring program to the desired core
	pin_cpu(core_ID);

	// Set the scheduling priority to high to avoid interruptions
	// (lower priorities cause more favorable scheduling, and -20 is the max)
	setpriority(PRIO_PROCESS, 0, -20);

	// Prepare monitoring set
	// Note that this time we use the optimization (no eviction set)
	unsigned long long monitoring_set_size = (unsigned long long)LLC_CACHE_WAYS;
	void **monitoring_set = NULL;
	void **current = NULL, **previous = NULL;
	uint64_t index1, index2, offset;

	// Find first address in our desired slice and given set
	offset = find_next_address_on_slice_and_set(buffer, slice_ID, set_ID);

	// Save this address in the monitoring set
	monitoring_set = (void **)(buffer + offset);

	// Get the L1 and L2 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)monitoring_set, 2);
	index1 = get_cache_set_index((uint64_t)monitoring_set, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	// These addresses will distribute across 2 LLC sets
	current = monitoring_set;
	for (i = 1; i < monitoring_set_size; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		while (index1 != get_cache_set_index((uint64_t)current + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current + offset, 2) ||
			   slice_ID != get_cache_slice_index(current + offset)) {
			offset += L2_INDEX_STRIDE;
		}

		// Set up pointer chasing. The idea is: *addr1 = addr2; *addr2 = addr3; and so on.
		*current = current + offset;
		current = *current;
	}

	// Make last item point back to the first one (useful for the loop)
	*current = monitoring_set;

	// Flush monitoring set
	current = monitoring_set;
	for (i = 0; i < monitoring_set_size; i++) {
		previous = current;
		current = *current;
		_mm_clflush(previous);
	}

	// Prepare samples array
	const int repetitions = 100000 * monitoring_set_size;
	uint32_t *samples = (uint32_t *)malloc(sizeof(*samples) * repetitions);

	// Read monitoring set from memory into cache
	// The addresses should all fit in the LLC
	current = monitoring_set;
	for (i = 0; i < 10 * monitoring_set_size; i++) {
		asm volatile("movq (%0), %0"
					 : "+rm"(current) /* output */ ::"memory");
	}

	// Time LLC loads
	current = monitoring_set;
	for (i = 0; i < repetitions; i++) {

		// Access the addresses sequentially.
		// Pointer chasing ensures that they are serialized.
		// By the time we reach each address, it probably won't be in the L1 and L2 anymore.
		// So this should give the LLC access time, without the need for an eviction set.
		asm volatile(
			".align 32\n\t"
			"lfence\n\t"
			"rdtsc\n\t"				/* eax = TSC (timestamp counter) */
			"movl %%eax, %%r8d\n\t" /* r8d = eax */
			"movq (%1), %1\n\t"		/* current = *current; LOAD */
			"rdtscp\n\t"			/* eax = TSC (timestamp counter) */
			"sub %%r8d, %%eax\n\t"	/* eax = eax - r8d; get timing difference between the second timestamp and the first one */
			"movl %%eax, %0\n\t"	/* samples[i] = eax */

			: "=rm"(samples[i]), "+rm"(current) /*output*/
			:
			: "rax", "rcx", "rdx", "r8", "memory");
	}

	// Store the samples to disk
	for (i = 0; i < repetitions; i++) {
		fprintf(output_file, "%" PRIu32 "\n", samples[i]);
	}

	// Free the buffers and file
	munmap(buffer, BUF_SIZE);
	fclose(output_file);
	free(output_filename);
	free(samples);

	return 0;
}
