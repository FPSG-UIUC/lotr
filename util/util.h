#ifndef _UTIL_H
#define _UTIL_H

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

#include "util-cpu-specific.h"

uint64_t get_cache_set_index(uint64_t addr, int cache_level);
uint64_t find_next_address_on_slice(void *va, uint8_t desired_slice);
uint64_t find_next_address_on_slice_and_set(void *va, uint8_t desired_slice, uint32_t desired_set);

/* 
 * Gets the value Time Stamp Counter 
 */
inline uint64_t get_time(void)
{
	uint64_t cycles;
	asm volatile("rdtscp\n\t"
				 "shl $32, %%rdx\n\t"
				 "or %%rdx, %0\n\t"
				 : "=a"(cycles)
				 :
				 : "rcx", "rdx", "memory");

	return cycles;
}

uint64_t start_time(void);
uint64_t stop_time(void);

inline void wait_cycles(uint64_t delay)
{
	uint64_t cycles, end;
	cycles = get_time();
	end = cycles + delay;
	while (cycles < end) {
		cycles = get_time();
	}
}

inline void maccess(void *p)
{
	asm volatile("movq (%0), %%rax" ::"r"(p)
				 : "rax");
}

void zeroize(void *pointer, size_t size_data);

uint64_t rand_uint64_slow(void);

struct Node {
	void *address;
	struct Node *next;
};

void append_string_to_linked_list(struct Node **head, void *addr);

void hexToBin(char *hex_string, char *bin_string);

static void pin_cpu(size_t core_ID)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(core_ID, &set);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &set) < 0) {
		printf("Unable to Set Affinity\n");
		exit(EXIT_FAILURE);
	}
}

void flush_l1i(void);

#endif