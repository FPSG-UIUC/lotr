#include "util-cpu-specific.h"

// Find these numbers here:
//  https://software.intel.com/sites/default/files/managed/39/c5/325462-sdm-vol-1-2abcd-3abcd.pdf
//
// Find original code here:
//  https://github.com/clementine-m/msr-uncore-cbo
//
// Skylake client has 4 C-boxes (meaning 4 slices?)
//
// Coffeelake has 8 C-boxes which are not documented in the Intel manual
// The 8th one is weird and fortunately documented here: https://lore.kernel.org/patchwork/patch/1002033/

const unsigned long long MSR_UNC_PERF_GLOBAL_CTRL = 0xe01;

const unsigned long long *MSR_UNC_CBO_PERFEVTSEL0 = (unsigned long long[]){0x700, 0x710, 0x720, 0x730,
																		   0x740, 0x750, 0x760, 0xf70};

const unsigned long long SELECT_EVENT_CORE = 0x408f34;

const unsigned long long *MSR_UNC_CBO_PERFCTR0 = (unsigned long long[]){0x706, 0x716, 0x726, 0x736,
																		0x746, 0x756, 0x766, 0xf76};

const unsigned long long ENABLE_CTRS = 0x20000000;

const unsigned long long DISABLE_CTRS = 0x0;

const unsigned long long RESET_CTRS = 0x0;

/*
 * Read an MSR on CPU 0
 */
static uint64_t rdmsr_on_cpu_0(uint32_t reg)
{
	uint64_t data;
	int cpu = 0;
	char *msr_file_name = "/dev/cpu/0/msr";

	static int fd = -1;

	if (fd < 0) {
		fd = open(msr_file_name, O_RDONLY);
		if (fd < 0) {
			if (errno == ENXIO) {
				fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
				exit(2);
			} else if (errno == EIO) {
				fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
						cpu);
				exit(3);
			} else {
				perror("rdmsr: open");
				exit(127);
			}
		}
	}

	if (pread(fd, &data, sizeof data, reg) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "rdmsr: CPU %d cannot read "
							"MSR 0x%08" PRIx32 "\n",
					cpu, reg);
			exit(4);
		} else {
			perror("rdmsr: pread");
			exit(127);
		}
	}

	return data;
}

/*
 * Write to an MSR on CPU 0
 */
static void wrmsr_on_cpu_0(uint32_t reg, int valcnt, uint64_t *regvals)
{
	uint64_t data;
	char *msr_file_name = "/dev/cpu/0/msr";
	int cpu = 0;

	static int fd = -1;

	if (fd < 0) {
		fd = open(msr_file_name, O_WRONLY);
		if (fd < 0) {
			if (errno == ENXIO) {
				fprintf(stderr, "wrmsr: No CPU %d\n", cpu);
				exit(2);
			} else if (errno == EIO) {
				fprintf(stderr, "wrmsr: CPU %d doesn't support MSRs\n",
						cpu);
				exit(3);
			} else {
				perror("wrmsr: open");
				exit(127);
			}
		}
	}

	while (valcnt--) {
		data = *regvals++;
		if (pwrite(fd, &data, sizeof data, reg) != sizeof data) {
			if (errno == EIO) {
				fprintf(stderr,
						"wrmsr: CPU %d cannot set MSR "
						"0x%08" PRIx32 " to 0x%016" PRIx64 "\n",
						cpu, reg, data);
				exit(4);
			} else {
				perror("wrmsr: pwrite");
				exit(127);
			}
		}
	}

	return;
}

/*
 * Initialize uncore registers (CBo/CHA and Global MSR) before polling
 */
static void uncore_init()
{
	int i;

	// Disable counters
	uint64_t register_value[] = {DISABLE_CTRS};
	wrmsr_on_cpu_0(MSR_UNC_PERF_GLOBAL_CTRL, 1, register_value);

	// Reset counters
	register_value[0] = RESET_CTRS;
	for (i = 0; i < LLC_CACHE_SLICES; i++) {
		wrmsr_on_cpu_0(MSR_UNC_CBO_PERFCTR0[i], 1, register_value);
	}

	// Select the event to monitor
	register_value[0] = SELECT_EVENT_CORE;
	for (i = 0; i < LLC_CACHE_SLICES; i++) {
		wrmsr_on_cpu_0(MSR_UNC_CBO_PERFEVTSEL0[i], 1, register_value);
	}

	// Enable counting
	register_value[0] = ENABLE_CTRS;
	wrmsr_on_cpu_0(MSR_UNC_PERF_GLOBAL_CTRL, 1, register_value);
}

/*
 * Polling one address
 */
#define NUMBER_POLLING 1500
static void polling(void *address)
{
	unsigned long i;
	for (i = 0; i < NUMBER_POLLING; i++) {
		_mm_clflush(address);
	}
}

/*
 * Read the CBo counters' value and return the one with maximum number
 */
static int find_CBO()
{
	int i;
	int *cboxes = calloc(LLC_CACHE_SLICES, sizeof(int));

	// Read CBox counter's value
	for (i = 0; i < LLC_CACHE_SLICES; i++) {
		cboxes[i] = rdmsr_on_cpu_0(MSR_UNC_CBO_PERFCTR0[i]);
		// printf(" % 10d", cboxes[i]);
	}
	// printf("\n");

	// Find maximum
	int max_value = 0;
	int max_index = 0;
	for (i = 0; i < LLC_CACHE_SLICES; i++) {
		if (cboxes[i] > max_value) {
			max_value = cboxes[i];
			max_index = i;
		}
	}

	return max_index;
}

static int skylake_i7_6700_cache_slice_alg(uint64_t i_addr)
{
	int bit0 = ((i_addr >> 6) & 1) ^ ((i_addr >> 10) & 1) ^ ((i_addr >> 12) & 1) ^ ((i_addr >> 14) & 1)
			 ^ ((i_addr >> 16) & 1) ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 18) & 1) ^ ((i_addr >> 20) & 1)
			 ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 24) & 1) ^ ((i_addr >> 25) & 1) ^ ((i_addr >> 26) & 1)
			 ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 28) & 1) ^ ((i_addr >> 30) & 1) ^ ((i_addr >> 32) & 1)
			 ^ ((i_addr >> 33) & 1) ^ ((i_addr >> 35) & 1);

	int bit1 = ((i_addr >> 7) & 1) ^ ((i_addr >> 11) & 1) ^ ((i_addr >> 13) & 1) ^ ((i_addr >> 15) & 1)
			 ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 19) & 1) ^ ((i_addr >> 20) & 1) ^ ((i_addr >> 21) & 1)
			 ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 23) & 1) ^ ((i_addr >> 24) & 1) ^ ((i_addr >> 26) & 1)
			 ^ ((i_addr >> 28) & 1) ^ ((i_addr >> 29) & 1) ^ ((i_addr >> 31) & 1) ^ ((i_addr >> 33) & 1)
			 ^ ((i_addr >> 34) & 1) ^ ((i_addr >> 35) & 1);

	int result = ((bit1 << 1) | bit0);
	return result;
}

static int coffee_lake_i7_9700_cache_slice_alg(uint64_t i_addr)
{
	int bit0 = ((i_addr >> 6) & 1) ^ ((i_addr >> 10) & 1) ^ ((i_addr >> 12) & 1) ^ ((i_addr >> 14) & 1)
			 ^ ((i_addr >> 16) & 1) ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 18) & 1) ^ ((i_addr >> 20) & 1)
			 ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 24) & 1) ^ ((i_addr >> 25) & 1) ^ ((i_addr >> 26) & 1)
			 ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 28) & 1) ^ ((i_addr >> 30) & 1) ^ ((i_addr >> 32) & 1)
			 ^ ((i_addr >> 33) & 1);

	int bit1 = ((i_addr >> 9) & 1) ^ ((i_addr >> 12) & 1) ^ ((i_addr >> 16) & 1) ^ ((i_addr >> 17) & 1)
			 ^ ((i_addr >> 19) & 1) ^ ((i_addr >> 21) & 1) ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 23) & 1)
			 ^ ((i_addr >> 25) & 1) ^ ((i_addr >> 26) & 1) ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 29) & 1)
			 ^ ((i_addr >> 31) & 1) ^ ((i_addr >> 32) & 1);

	int bit2 = ((i_addr >> 10) & 1) ^ ((i_addr >> 11) & 1) ^ ((i_addr >> 13) & 1) ^ ((i_addr >> 16) & 1)
			 ^ ((i_addr >> 17) & 1) ^ ((i_addr >> 18) & 1) ^ ((i_addr >> 19) & 1) ^ ((i_addr >> 20) & 1)
			 ^ ((i_addr >> 21) & 1) ^ ((i_addr >> 22) & 1) ^ ((i_addr >> 27) & 1) ^ ((i_addr >> 28) & 1)
			 ^ ((i_addr >> 30) & 1) ^ ((i_addr >> 31) & 1) ^ ((i_addr >> 33) & 1) ^ ((i_addr >> 34) & 1);

	int result = ((bit2 << 2) | (bit1 << 1) | bit0);
	return result;
}

/* 
 * Get the page frame number
 */
static uint64_t get_page_frame_number_of_address(void *address)
{
	/* Open the pagemap file for the current process */
	FILE *pagemap = fopen("/proc/self/pagemap", "rb");

	/* Seek to the page that the buffer is on */
	uint64_t offset = (uint64_t)((uint64_t)address >> PAGE_SHIFT) * (uint64_t)PAGEMAP_LENGTH;
	if (fseek(pagemap, (uint64_t)offset, SEEK_SET) != 0) {
		fprintf(stderr, "Failed to seek pagemap to proper location\n");
		exit(1);
	}

	/* The page frame number is in bits 0-54 so read the first 7 bytes and clear the 55th bit */
	uint64_t page_frame_number = 0;
	int ret = fread(&page_frame_number, 1, PAGEMAP_LENGTH - 1, pagemap);
	page_frame_number &= 0x7FFFFFFFFFFFFF; // Mastik uses 0x3FFFFFFFFFFFFF
	fclose(pagemap);
	return page_frame_number;
}

/*
 * Get the physical address of a page
 */
uint64_t get_physical_address(void *address)
{
	/* Get page frame number */
	unsigned int page_frame_number = get_page_frame_number_of_address(address);

	/* Find the difference from the buffer to the page boundary */
	uint64_t distance_from_page_boundary = (uint64_t)address % getpagesize();

	/* Determine how far to seek into memory to find the buffer */
	uint64_t physical_address = (uint64_t)((uint64_t)page_frame_number << PAGE_SHIFT) + (uint64_t)distance_from_page_boundary;

	return physical_address;
}

static void check_reversed_function_correct(uint64_t slice_id, void *va)
{
	uncore_init();
	polling(va);
	uint64_t ground_truth = find_CBO();
	uint64_t pa = get_physical_address(va);

	if (slice_id != ground_truth) {
		printf("%" PRIu64 " should be %" PRIu64 "\n", slice_id, ground_truth);
	}
}

/*
 * Calculate the slice based on a given virtual address - Haswell and SkyLake 
 */
uint64_t get_cache_slice_index(void *va)
{
	// uncore_init();
	// polling(va);
	// uint64_t slice_id = find_CBO();

	uint64_t pa = get_physical_address(va);
	uint64_t slice_id = coffee_lake_i7_9700_cache_slice_alg(pa);

	// // Code to verify that our slice mapping function works as expected
	// check_reversed_function_correct(slice_id, va);

	// // Code to print the binary representation of an address and its slice ID
	// char hex_key[17];
	// char bin_key[65];
	// sprintf(hex_key, "%09lX", pa);
	// hexToBin(hex_key, bin_key);
	// printf("%s %d\n", bin_key, slice_id);

	return slice_id;
}
