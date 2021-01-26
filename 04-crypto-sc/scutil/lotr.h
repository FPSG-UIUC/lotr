
#ifndef _LOTR_H
#define _LOTR_H

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

#include "../../util/util.h"

/*
 * Shared memory / synchronization functions simulating the preemptive scheduling attack.
 * 
 * The victim starts and waits for the attacker to request a decription/signature.
 * To request a decryption, the attacker specifies what iteration of the victim loop it wants
 * to monitor in the sign_requested variable (simulating preemptive scheduling).
 * The victim then enters the decryption function loop. Right before the iteration specified
 * in sign_requested, the victim cleanses its cache (simulating a context switch due to being
 * preempted) and sets variable iteration_of_interest_running to 1.
 * The attacker monitors ring contention during this time and records the timing data.
 * Later, the victim ends the decryption and goes back to waiting for other requests.
 * Note that there is no need for the attacker to know the plaintext / ciphertext.
 */

#define SYNCFILE "/var/tmp/.lotr-syncfile"

// Should fit in one cache line
struct sharestruct {
	volatile int sign_requested; // 4B
	volatile int iteration_of_interest_running;	 // 4B
	volatile uint8_t cleansing_mechanism;	// 1 for clflush the address space; 2 for evicting L1 and L2 only
};

static int createfile(const char *fn)
{
	int fd;
	struct stat sb;
	char sharebuf[PAGE];
	if (stat(fn, &sb) != 0 || sb.st_size != PAGE) {
		fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			perror("open");
			fprintf(stderr, "createfile: couldn't create shared file %s\n", fn);
			exit(1);
		}
		if (write(fd, sharebuf, PAGE) != PAGE) {
			fprintf(stderr, "createfile: couldn't write shared file\n");
			exit(1);
		}
		return fd;
	}
	fd = open(fn, O_RDWR, 0644);
	if (fd < 0) {
		perror(fn);
		fprintf(stderr, "createfile: couldn't open shared file\n");
		exit(1);
	}
	return fd;
}

static volatile struct sharestruct *get_sharestruct(void)
{
	int fd = createfile(SYNCFILE);
	volatile struct sharestruct *ret;
	ret = (volatile struct sharestruct *)mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
	if (ret == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	return ret;
}

// Functions added to sync with the victim
void prepare_for_attack(uint8_t *attacking);
void check_attack_iteration(uint8_t *attacking);
void cryptoloop_check_a(uint8_t *attacking);
void cryptoloop_check_b(uint8_t *attacking);

#endif