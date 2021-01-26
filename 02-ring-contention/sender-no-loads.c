#include "../util/util.h"
#include <semaphore.h>

int main(int argc, char **argv)
{
	// Check arguments: should contain core ID
	if (argc != 2) {
		printf("Wrong Input! Enter desired coreID!\n");
		printf("Enter: %s <coreID>\n", argv[0]);
		exit(1);
	}

	// Parse core ID
	int core_ID;
	sscanf(argv[1], "%d", &core_ID);
	if (core_ID > NUMBER_OF_LOGICAL_CORES - 1 || core_ID < 0) {
		fprintf(stderr, "Wrong core! core_ID should be less than %d and more than 0!\n", NUMBER_OF_LOGICAL_CORES);
		exit(1);
	}

	// Pin the monitoring program to the desired core
	pin_cpu(core_ID);

	// Create semaphore (sender starts first)
	sem_unlink("sem1");
	sem_t *sem1 = sem_open("sem1", O_CREAT | O_EXCL, 0600, 0);
	if (sem1 == SEM_FAILED) {
		perror("sem_open sem1");
		return -1;
	}

	// Wait for the receiver to be ready
	sem_wait(sem1);

	// Do nothing (busy loop until killed)
	while (1) {}

	// Clean up
	sem_close(sem1);
	sem_unlink("sem1");

	return 0;
}
