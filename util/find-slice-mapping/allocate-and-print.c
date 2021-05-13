#include "../util.h"

#define BUF_SIZE 100 * 1024UL * 1024 /* Buffer Size -> 100*1MB */

int main(int argc, char **argv)
{
	int i, j, k;

	// Allocate large buffer (pool of addresses)
	void *buffer = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	// Write data to the buffer so that any copy-on-write
	// mechanisms will give us our own copies of the pages.
	memset(buffer, 0, BUF_SIZE);

	uint64_t offset = CACHE_BLOCK_SIZE; // skip to the next address with the same LLC cache set index
	while (1) {
		get_cache_slice_index((void *)((uint64_t)buffer + offset));
		offset += CACHE_BLOCK_SIZE;
	}

	return 0;
}
