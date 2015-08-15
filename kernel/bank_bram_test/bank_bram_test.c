#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include "../bank_bram/bank_bram.h"

main()
{
	int memfd;
	void *bram;

	memfd = open ("/dev/mem", O_RDWR, S_IRUSR | S_IWUSR);
	if (0 == memfd)
	{
		warnx("main: /dev/mem failed to open.");
	} else {
		int * memValue = (int *) mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, memfd, BANK_BRAM_BASE);

		printf ("The system chose to map the page to %x\n", memValue);
		printf ("Contents of memory at start is %x\n", *memValue);

		*memValue += 1;

		printf ("Contents of memory at end is %x\n", *memValue);

		int rtn = munmap((void *) memValue, sizeof(int));
		if (0 > rtn)
		{
			printf ("main: Memory de-allocation failed: %s\n", strerror(errno));
		}

		close(memfd);
	}

	memfd = open ("/dev/bank_bram", O_RDWR, S_IRUSR | S_IWUSR);
	if (0 == memfd)
	{
		warnx("main: /dev/bank_bram failed to open.");
	} else {
		int * memValue = (int *) mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED, memfd, BANK_BRAM_BASE);

		printf ("The system chose to map the page to %x\n", memValue);
		printf ("Contents of memory at start is %x\n", *memValue);

		*memValue += 1;

		printf ("Contents of memory at end is %x\n", *memValue);

		int rtn = munmap((void *) memValue, sizeof(int));
		if (0 > rtn)
		{
			printf ("main: Memory de-allocation failed: %s\n", strerror(errno));
		}

		close(memfd);
	}
	return 0;
}
