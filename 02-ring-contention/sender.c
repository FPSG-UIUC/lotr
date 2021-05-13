#include "../util/util.h"
#include <semaphore.h>
#include <sys/resource.h>

#define BUF_SIZE 100 * 1024UL * 1024 /* Buffer Size -> 100*1MB */

int main(int argc, char **argv)
{
	int i;

	// Check arguments
	if (argc != 5) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice ID, N of extra addresses (for the monitoring set to incur LLC misses), and set ID!\n");
		printf("Enter: %s <core_ID> <slice_ID> <N of extra addresses> <set_ID>\n", argv[0]);
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

	// Parse N of extra addresses for the monitoring set
	int n_of_extra_addresses;
	sscanf(argv[3], "%d", &n_of_extra_addresses);
	if (n_of_extra_addresses < 0) {
		printf("Wrong N! N should be more than 0!\n");
		exit(1);
	}

	// Parse set number
	int set_ID;
	sscanf(argv[4], "%d", &set_ID);
	if (set_ID > LLC_CACHE_SETS_PER_SLICE - 1 || set_ID < 0) {
		fprintf(stderr, "Wrong set! set_ID should be less than %d and more than 0!\n", LLC_CACHE_SETS_PER_SLICE);
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
	pin_cpu(core_ID);

	// Set the scheduling priority to high to avoid interruptions
	// (lower priorities cause more favorable scheduling, and -20 is the max)
	setpriority(PRIO_PROCESS, 0, -20);

	// Prepare monitoring set
	// We choose the sender so that it uses a larger monitoring set, filling up 2 entire LLC cache sets
	// NOTE: the monitoring set may be even larger than that (depending on n_of_extra_addresses) because
	// we want the sender to miss in the LLC sometimes (see paper)
	unsigned long long monitoring_set_size = (unsigned long long)((2 * LLC_CACHE_WAYS) + n_of_extra_addresses);
	struct Node *monitoring_set = NULL;
	struct Node *current = NULL;
	uint64_t index1, index2, offset;

	// Find first address in our desired slice and given set
	offset = find_next_address_on_slice_and_set(buffer, slice_ID, set_ID);

	// Save this address in the monitoring set
	append_string_to_linked_list(&monitoring_set, (void *)((uint64_t)buffer + offset));

	// Get the L1 and L2 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)monitoring_set->address, 2);
	index1 = get_cache_set_index((uint64_t)monitoring_set->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	// These addresses will distribute across 2 LLC sets
	current = monitoring_set;
	for (i = 1; i < monitoring_set_size; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   slice_ID != get_cache_slice_index((void *)((uint64_t)current->address + offset))) {
			offset += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&monitoring_set, (void *)((uint64_t)current->address + offset));
		current = current->next;
	}

	// Flush monitoring set
	current = monitoring_set;
	while (current != NULL) {
		_mm_clflush(current->address);
		current = current->next;
	}

	// Create semaphore (sender starts first)
	// NOTE: If using multiple senders, comment out the semaphore lines
	sem_unlink("sem1");
	sem_t *sem1 = sem_open("sem1", O_CREAT | O_EXCL, 0600, 0);
	if (sem1 == SEM_FAILED) {
		perror("sem_open sem1");
		return -1;
	}

	// Read monitoring set from memory into cache
	current = monitoring_set;
	while (current != NULL) {
		_mm_lfence();
		maccess(current->address);
		current = current->next;
	}

	_mm_lfence();

	// Wait for the receiver to be ready
	sem_wait(sem1);

	// Spam the ring interconnect (until killed)
	while (1) {

		// Send these loads concurrently (no serialization)
		current = monitoring_set;
		while (current != NULL) {
			maccess(current->address);
			current = current->next;
		}
	}

	// Free the buffer
	munmap(buffer, BUF_SIZE);

	// NOTE: When using multiple senders, comment out the semaphore lines
	sem_close(sem1);
	sem_unlink("sem1");

	// Clean up lists
	struct Node *tmp = NULL;
	for (current = monitoring_set; current != NULL; tmp = current, current = current->next, free(tmp));

	return 0;
}
