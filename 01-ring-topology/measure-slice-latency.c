#include "../util/util.h"
#include <sys/resource.h>

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */

int main(int argc, char **argv)
{
	int i, j, k;

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

	// Prepare monitoring set and eviction set (see paper for details)
	unsigned long long monitoring_set_size = (unsigned long long)LLC_CACHE_WAYS;
	unsigned long long eviction_set_size = (unsigned long long)L1_CACHE_WAYS;
	struct Node *monitoring_set = NULL;
	struct Node *eviction_set = NULL;
	struct Node *current = NULL;
	uint64_t index1, index2, index3, offset;

	// Find first address in our desired slice and given set
	offset = find_next_address_on_slice_and_set(buffer, slice_ID, set_ID);

	// Save this address in the monitoring set
	append_string_to_linked_list(&monitoring_set, buffer + offset);

	// Get the L1, L2 and L3 cache set indexes of the monitoring set
	index3 = get_cache_set_index((uint64_t)monitoring_set->address, 3);
	index2 = get_cache_set_index((uint64_t)monitoring_set->address, 2);
	index1 = get_cache_set_index((uint64_t)monitoring_set->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L3/L2/L1
	current = monitoring_set;
	for (i = 1; i < monitoring_set_size; i++) {
		offset = LLC_INDEX_STRIDE; // skip to the next address with the same LLC cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   index3 != get_cache_set_index((uint64_t)current->address + offset, 3) ||
			   slice_ID != get_cache_slice_index(current->address + offset)) {
			offset += LLC_INDEX_STRIDE;
		}

		append_string_to_linked_list(&monitoring_set, current->address + offset);
		current = current->next;
	}

	// Find the first address of the eviction set (same L1/L2 sets but different L3 set)
	offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
	while (index1 != get_cache_set_index((uint64_t)monitoring_set->address + offset, 1) ||
		   index2 != get_cache_set_index((uint64_t)monitoring_set->address + offset, 2) ||
		   index3 == get_cache_set_index((uint64_t)monitoring_set->address + offset, 3) ||
		   slice_ID != get_cache_slice_index(monitoring_set->address + offset)) {
		offset += L2_INDEX_STRIDE;
	}

	// Save this address in the eviction set
	append_string_to_linked_list(&eviction_set, monitoring_set->address + offset);

	// Find more addresses of the eviction set
	current = eviction_set;
	for (i = 1; i < eviction_set_size; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same LLC cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   index3 == get_cache_set_index((uint64_t)current->address + offset, 3) ||
			   slice_ID != get_cache_slice_index(current->address + offset)) {
			offset += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&eviction_set, current->address + offset);
		current = current->next;
	}

	// Flush monitoring set
	current = monitoring_set;
	for (i = 0; i < monitoring_set_size; i++) {
		_mm_clflush(current->address);
		current = current->next;
	}

	// Prepare samples array
	const int repetitions = 100000;
	uint32_t *samples = (uint32_t *)malloc(sizeof(*samples) * repetitions * monitoring_set_size);

	// Read monitoring set from memory into cache
	// The addresses should all fit in the LLC
	current = monitoring_set;
	for (i = 0; i < monitoring_set_size; i++) {
		_mm_lfence();
		maccess(current->address);
		current = current->next;
	}

	// Time LLC loads
	for (j = 0, k = 0; k < repetitions; k++) {

		// Evict addresses of the monitoring set from L1 and L2.
		// This trick ensures that the timed loads below don't hit in the L1 or L2.
		// We are performing loads that conflict with the ones below in L1 and L2.
		//
		// We chose the eviction set traversal algorithm that worked best for our CPU,
		// and you can find it and other ones here:
		//     https://github.com/cgvwzq/evsets/blob/master/cache.c
		current = eviction_set;
		// while (current && current->next && current->next->next) {
		while (current && current->next) {
			maccess(current->address);
			maccess(current->next->address);
			// maccess(current->next->next->address);
			maccess(current->address);
			maccess(current->next->address);
			// maccess(current->next->next->address);
			current = current->next;
		}

		// Access the addresses again.
		// They should not be in the L1 and L2 anymore.
		// So this gives us the LLC access time.
		current = monitoring_set;
		for (i = 0; i < monitoring_set_size; i++) {
			asm volatile(
				".align 32\n\t"
				"lfence\n\t"
				"rdtsc\n\t"				/* eax = TSC (timestamp counter) */
				"movl %%eax, %%r8d\n\t" /* r8d = eax */
				"movq (%1), %%r9\n\t"	/* r9 = *(current->address); LOAD */
				"rdtscp\n\t"			/* eax = TSC (timestamp counter) */
				"sub %%r8d, %%eax\n\t"	/* eax = eax - r8d; get timing difference between the second timestamp and the first one */
				"movl %%eax, %0\n\t"	/* samples[j++] = eax */

				: "=rm"(samples[j++]) /*output*/
				: "r"(current->address)
				: "rax", "rcx", "rdx", "r8", "r9", "memory");

			current = current->next;
		}
	}

	// Store the samples to disk
	for (j = 0; j < repetitions * monitoring_set_size; j++) {
		fprintf(output_file, "%" PRIu32 "\n", samples[j]);
	}

	// Free the buffers and file
	munmap(buffer, BUF_SIZE);
	fclose(output_file);
	free(output_filename);
	free(samples);

	// Clean up lists
	struct Node *tmp = NULL;
	for (current = monitoring_set; current != NULL; tmp = current, current = current->next, free(tmp));
	for (current = eviction_set; current != NULL; tmp = current, current = current->next, free(tmp));

	return 0;
}
