/*
  mailbox.c (based on hello_world)

  Updated by Peter Saunderson

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
#include <unistd.h>	// for _SC_PAGESIZE
#include <sys/mman.h>	// for mmap
#include <fcntl.h>	// for O_RDWR
#include <errno.h>	// for errno
#include <e-hal.h>
#include <linux/ioctl.h>

#define _BufSize   (128)
#define _BufOffset (0x01000000)
#define _SeqLen    (32)

#define ELINK_MAILBOXLO	(0xF0320)
#define ELINK_MAILBOXHI	(0xF0324)
#define ELINK_MAILBOXSTAT (0xF0328)
#define ELINK_TXCFG	(0xF0210)
#define ELINK_RXCFG	(0xF0300)

//
// Following should be in epiphany.h, keep them here for now
//
#define EPIPHANY_DEV "/dev/epiphany"

#define EPIPHANY_IOC_MAGIC  'k'

#define EPIPHANY_IOC_GETSHM_CMD   24
#define EPIPHANY_IOC_MB_DISABLE_CMD   25
#define EPIPHANY_IOC_MB_ENABLE_CMD    26

#define EPIPHANY_IOC_MAXNR        26
 
#define EPIPHANY_IOC_GETSHM _IOWR(EPIPHANY_IOC_MAGIC, EPIPHANY_IOC_GETSHM_CMD, epiphany_alloc_t *)
#define EPIPHANY_IOC_MB_ENABLE _IO(EPIPHANY_IOC_MAGIC, EPIPHANY_IOC_MB_ENABLE_CMD)
#define EPIPHANY_IOC_MB_DISABLE _IO(EPIPHANY_IOC_MAGIC, EPIPHANY_IOC_MB_DISABLE_CMD)
//
// Above should be in epiphany.h, keep them here for now
//

int main(int argc, char *argv[])
{
	unsigned row, col, coreid, i;
	e_platform_t platform;
	e_epiphany_t dev;
	e_mem_t emem;
	char emsg[_BufSize];

	srand(1);

	// initialize system, read platform params from
	// default HDF. Then, reset the platform and
	// get the actual system parameters.
	e_init(NULL);
	e_reset_system();
	e_get_platform_info(&platform);
	printf("main: rows,cols: (%2d,%2d)\n", platform.rows, platform.cols);
 
	// Allocate a buffer in shared external memory
	// for message passing from eCore to host.
	e_alloc(&emem, _BufOffset, _BufSize);

	// Now open the epiphany device for mailbox interrupt control
	int devfd = open(EPIPHANY_DEV, O_RDWR | O_SYNC);
	if ( -1 == devfd ) {
		printf("main(): EPIPHANY_DEV file open failure.");
		return E_ERR;
	}

	row = 0;
	col = 0;
	for (i=0; i<_SeqLen; i++)
	{		
	        // Enable the mailbox interrupt
		if ( -1 == ioctl(devfd, EPIPHANY_IOC_MB_ENABLE) )
		{
			printf("main(): Failed to enable mailbox "
			  "Error is %s\n", strerror(errno));
		}

		// Visit each core
		unsigned lastCol = col;
		col = col % platform.cols;

		if (col < lastCol)
		{
			row++;
			unsigned lastRow = row;
			row = row % platform.rows;

			if (row < lastRow)
			{
				break;
			}
		}
		
		// Calculate the coreid
		coreid = (row + platform.row) * 64 + col + platform.col;
		printf("main: %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);

		// Open the single-core workgroup and reset the core, in
		// case a previous process is running. Note that we used
		// core coordinates relative to the workgroup.
		e_open(&dev, row, col, 1, 1);
		e_reset_group(&dev);

		// Load the device program onto the selected eCore
		// and launch after loading.
		e_return_stat_t result;
		e_load("/usr/epiphany/bin/e_mailbox.srec", &dev, 0, 0, E_TRUE);
		if (result != E_OK)
		{
			printf("main: Error in e_load %i\n", result);
		}

		// Wait for core program execution to finish, then
		// read message from shared buffer.
		// TODO replace wait with wait on interrupt.. perhaps
		//      implement blocking read of sysfs file filled by
		//      interrupt handler
		usleep(40000);
		e_read(&emem, 0, 0, 0x0, emsg, _BufSize);

		// Print the message and close the workgroup.
		printf("\"%s\"\n", emsg);

		/* Temp removal of the following
                   Seems to cause problems with visiting each core.
		int items = ee_read_esys(ELINK_MAILBOXSTAT);
		if (0 == items)
		{
			printf ("main(): ERROR: mailbox txfer stopped!");
			break;
		}
		
		// Read the mailbox
		int mbentries;
		for (mbentries = 0; mbentries < 8; mbentries++)
		{
			int pre_stat    = ee_read_esys(ELINK_MAILBOXSTAT);
			if (0 == pre_stat)
			{
				break;
			}

			int mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
			int mbox_hi     = ee_read_esys(ELINK_MAILBOXHI);
			int post_stat   = ee_read_esys(ELINK_MAILBOXSTAT);
			printf ("main(): PRE_STAT=%08x POST_STAT=%08x LO=%08x HI=%08x\n", pre_stat, post_stat, mbox_lo, mbox_hi);
		}
		*/
		
		e_close(&dev);
	
		col++;
	}
	
	// Read the mailbox
	int mbentries;
	for (mbentries = 0; mbentries < 20; mbentries++)
	{
		int pre_stat    = ee_read_esys(ELINK_MAILBOXSTAT);
		if (0 == pre_stat)
		{
			break;
		}
		int mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
		int mbox_hi     = ee_read_esys(ELINK_MAILBOXHI);
		int post_stat   = ee_read_esys(ELINK_MAILBOXSTAT);
		printf ("main(): PRE_STAT=%08x POST_STAT=%08x LO=%08x HI=%08x\n", pre_stat, post_stat, mbox_lo, mbox_hi);
	}
		
	// Now close the epiphany device
	close(devfd);
	
	// Release the allocated buffer and finalize the
	// e-platform connection.
	e_free(&emem);
	e_finalize();
	
	return 0;
}
