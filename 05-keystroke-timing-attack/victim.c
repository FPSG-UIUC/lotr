#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <x86intrin.h>

int main(int argc, char **argv)
{
	// Open output file
	FILE *output_keypresses;
	if (!(output_keypresses = fopen("./out/keystroke-cycles.out", "w"))) {
		perror("fopen");
		exit(1);
	}

	// Configure the terminal so that getchar returns on any char
	// (without the need to type enter). Source:
	//     https://stackoverflow.com/a/1798833/5192980
	static struct termios oldt, newt;

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	tcgetattr(STDIN_FILENO, &oldt);

	// now the settings will be copied
	newt = oldt;

	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	newt.c_lflag &= ~(ICANON);

	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	char c;
	uint64_t cycles, end;
	uint64_t keypresscycle;
	uint64_t keypresscycles[100];
	int i = 0;

	for (i = 99; i >= 0; i--) {
		keypresscycles[i] = 0;
	}

	i = 0;
	while (1) {

		// Read one character
		c = getchar();

		// Read timestamp immediately
		asm volatile("rdtsc\n\t"
					 "shl $32, %%rdx\n\t"
					 "or %%rdx, %0\n\t"
					 : "=a"(keypresscycle)
					 :
					 : "rdx", "memory");

		keypresscycles[i++] = keypresscycle;

		// If the last character is received stop
		if (c == '0') {
			break;
		}
	}

	_mm_lfence();

	// Store the timing measurements
	int samples = i;
	for (i = 0; i < samples; i++) {
		fprintf(output_keypresses, "%" PRIu64 "\n", keypresscycles[i]);
	}

	// Restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fclose(output_keypresses);

	return 0;
}
