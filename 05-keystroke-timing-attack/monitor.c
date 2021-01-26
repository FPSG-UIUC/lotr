#include "../util/util.h"

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */

#define MAXSAMPLES 50331648

int main(int argc, char **argv)
{
	int i;

	// Check arguments
	if (argc != 3) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice ID!\n");
		fprintf(stderr, "Enter: %s <core_ID> <slice_ID>\n", argv[0]);
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

	// For this experiment we can use a fixed cache set
	int set_ID = 5;

	// Prepare data output file
	FILE *output_data;
	if (!(output_data = fopen("./out/timing-samples.out", "w"))) {
		perror("fopen");
		exit(1);
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
	//
	// This time we do not set the priority like in the RE because
	// doing so would use root and we want our actual attack
	// to be realistic for a user space process
	pin_cpu(core_ID);

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

	// Allocate buffers for results
	uint64_t *samples_x = (uint64_t *)malloc(sizeof(*samples_x) * MAXSAMPLES);
	uint32_t *samples_y = (uint32_t *)malloc(sizeof(*samples_y) * MAXSAMPLES);

	// Read addresses from memory into cache a few times
	// These lines will all fit in the LLC
	current = monitoring_set;
	for (i = 0; i < 10 * monitoring_set_size; i++) {
		asm volatile("movq (%0), %0"
					 : "+rm"(current) /* output */ ::"memory");
	}

	// Start monitoring loop
	uint32_t time;
	current = monitoring_set;
	for (time = 0; time < MAXSAMPLES; time++) {
		asm volatile(
			".align 32\n\t"
			"lfence\n\t"
			"rdtsc\n\t"					/* eax = TSC (timestamp counter)*/
			"shl $32, %%rdx\n\t"
			"or %%rdx, %%rax\n\t"

			"movq %%rax, %%r8\n\t"		/* r8 = rax; this is to back up rax into another register */
			// "movl %%eax, %%r8d\n\t"	/* r8d = eax; this is to back up eax into another register */

			"movq (%2), %2\n\t"			/* current = *current; LOAD */
			"movq (%2), %2\n\t" 		/* current = *current; LOAD */
			"movq (%2), %2\n\t" 		/* current = *current; LOAD */
			"movq (%2), %2\n\t" 		/* current = *current; LOAD */

			"rdtscp\n\t"				/* eax = TSC (timestamp counter) */
			"shl $32, %%rdx\n\t"
			"or %%rdx, %%rax\n\t"

			"sub %%r8, %%rax\n\t"		/* rax = rax - r8; get timing difference between the second timestamp and the first one */
			// "sub %%r8d, %%eax\n\t" 	/* eax = eax - r8d; get timing difference between the second timestamp and the first one */

			"movq %%r8, %0\n\t" 		/* result_x[i] = r8 */
			"movl %%eax, %1\n\t" 		/* result_y[i] = eax */
			: "=rm"(samples_x[time]), "=rm"(samples_y[time]), "+rm"(current)		 /*output*/
			:
			: "rax", "rcx", "rdx", "r8", "memory");
	}

	fprintf(stderr, "# DONE, storing data\n");

	// Store the samples to disk
	uint32_t samples = time;
	for (time = 0; time < samples; time++) {
		fprintf(output_data, "%" PRIu64 " %" PRIu32 "\n", samples_x[time], samples_y[time]);
	}

	// Free the buffers and file
	munmap(buffer, BUF_SIZE);
	fclose(output_data);
	free(samples_x);
	free(samples_y);

	return 0;
}
