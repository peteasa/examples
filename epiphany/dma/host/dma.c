/*
  hello_world.c

  Copyright (C) 2012 Adapteva, Inc.
  Contributed by Yaniv Sapir <yaniv@adapteva.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program, see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>.
*/

// This is the HOST side of the Hello World example.
// The program initializes the Epiphany system,
// randomly draws an eCore and then loads and launches
// the device program on that eCore. It then reads the
// shared external memory buffer for the core's output
// message.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <e-hal.h>

#define destAddress (0x4000)	// Bank2 base address
#define numberOfWords (0x1ff0 / 4)
#define _BufSize   (128)
#define _BufOffset (0x01000000)
#define _SeqLen    (16)

int main(int argc, char *argv[])
{
	unsigned row, col, coreid, i, j;
	e_platform_t platform;
	e_epiphany_t dev;
	e_mem_t emem;
	char emsg[_BufSize];

	// initialize system, read platform params from
	// default HDF. Then, reset the platform and
	// get the actual system parameters.
	e_init(NULL);
	e_reset_system();
	e_get_platform_info(&platform);

	// Allocate a buffer in shared external memory
	// for message passing from eCore to host.
	// In epiphany code link to SECTION("shared_dram")
	// ie char shared_outbuf[128*16] SECTION("shared_dram");
	e_alloc(&emem, _BufOffset, _BufSize*16);

	// How to find the number of cores available?

	// How to communicate the size of the group to the core?

	// How to write to core memory?

	// Open a workgroup with all cores and reset the cores, in
	// case a previous process is running. Note that we used
	// core coordinates relative to the workgroup.
	e_open(&dev, 0, 0, 4, 4);
	e_reset_group(&dev);

	row = 0;
	col = 0;
	for (i=0; i<_SeqLen; i++)
	{
		// Visit each core
		unsigned lastCol = col;
		col = col % platform.cols;

		if (col < lastCol)
		{
			row++;
		}
		row = row % platform.rows;
		
		coreid = (row + platform.row) * 64 + col + platform.col;

		// Load the device program onto the selected eCore
		e_return_stat_t result;
		result = e_load("/usr/epiphany/bin/e_dma.srec", &dev, row, col, E_FALSE);
		if (result != E_OK)
		{
			fprintf(stderr, "main: 0x%03x Error in e_load %i\n", coreid, result);
		}

		col++;
	}

	// Start the run
	e_start_group(&dev);

	// Choose the core to start the run
	row = 1;
	col = 1;

	// Initial the buffer to DMA
	unsigned words, location;
	words = numberOfWords;
	location = 0;
	off_t dest;
	dest = (off_t)(destAddress + location);
	e_write(&dev, row, col, dest, &words, sizeof(unsigned));
	location += sizeof(unsigned);
	for (i=0; i<words; i++)
	{
		dest = (off_t)(destAddress + location);
		e_write(&dev, row, col, dest, &i, sizeof(int));
		location += sizeof(int);
	}
		
	// Send E_USER_INT
	if (E_OK != e_signal(&dev, row, col))
	{
		fprintf(stderr, "main: failed to send interrupt to (%2d,%2d)", row, col);
	}

	// Allow time for the processing to complete
	usleep(100000);

	row = 0;
	col = 0;
	for (i=0; i<_SeqLen; i++)
	{
		// Visit each core
		unsigned lastCol = col;
		col = col % platform.cols;

		if (col < lastCol)
		{
			row++;
		}
		row = row % platform.rows;

		coreid = (row + platform.row) * 64 + col + platform.col;

		fprintf(stderr, "main: %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);
		
		// read message from shared buffer.
		e_read(&emem, 0, 0, _BufSize * (row * 4 + col), emsg, _BufSize);

		// Print the message
		fprintf(stderr, "\"%s\"\n", emsg);

		// Now read the buffer
		int val;
		location = 0;
		dest = (off_t)(destAddress + location);
		e_read(&dev, row, col, dest, &val, sizeof(unsigned));
		location += sizeof(unsigned);
		fprintf(stderr, "%d ", val);

		int ok;
		ok = 0;
		for (j=0; j<words; j++)
		{
			dest = (off_t)(destAddress + location);
			e_read(&dev, row, col, dest, &val, sizeof(int));
			location += sizeof(int);
			if (val != j)
			{
				ok = val;
				break;
			}
		}
		if (0 != ok)
		{
			fprintf(stderr, "oops failure at %d", ok);
		}
		else
		{
			fprintf(stderr, " all ok!", ok);
		}
		fprintf(stderr, "\n");
		
		col++;
	}

	// and close the workgroup.
	e_close(&dev);

	// Release the allocated buffer and finalize the
	// e-platform connection.
	e_free(&emem);
	e_finalize();

	return 0;
}

