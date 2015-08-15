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

#define _BufSize   (128)
#define _BufOffset (0x01000000)
#define _SeqLen    (16)

int main(int argc, char *argv[])
{
	unsigned row, col, coreid, i;
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
		result = e_load("/usr/epiphany/bin/e_mutex.srec", &dev, row, col, E_FALSE);
		if (result != E_OK)
		{
			fprintf(stderr, "main: 0x%03x Error in e_load %i\n", coreid, result);
		}

		col++;
	}

	// Start the run
	e_start_group(&dev);

	// Wait for core program execution to finish, then
	// read message from shared buffer.
	usleep(50000);

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

		if (E_OK != e_signal(&dev, row, col))
		{
			fprintf(stderr, "main: failed to send interrupt to (%2d,%2d)", row, col);
		}

		// Allow time for the interrupt and sprintf
		usleep(1000);

		coreid = (row + platform.row) * 64 + col + platform.col;

		fprintf(stderr, "main: %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);
		
		// read message from shared buffer.
		e_read(&emem, 0, 0, _BufSize * (row * 4 + col), emsg, _BufSize);

		// Print the message
		fprintf(stderr, "\"%s\"\n", emsg);

		// read the neighbour ids from the core
		int p,q;
		unsigned vrow, vcol;
		vrow = 0;
		vcol = 0;
		for (p=0; p<_SeqLen; p++)
		{
			// Visit each entry in the list
			unsigned lastvCol = vcol;
			vcol = vcol % platform.cols;

			if (vcol < lastvCol)
			{
				vrow++;
			}
			vrow = vrow % platform.rows;

			// Now calculate the address
			off_t dest;
			dest = (off_t)(0x4000 + (vrow * 4 + vcol) * sizeof(e_coreid_t));
			// Now print the entry
			e_coreid_t vcoreid;
			e_read(&dev, row, col, dest, &vcoreid, sizeof(e_coreid_t));
			fprintf(stderr, "0x%x ", vcoreid);

			vcol++;
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

