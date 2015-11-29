/*
  mailbox.c

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
#include <e-hal.h>

#define _BufSize   (128)
#define _BufOffset (0x01000000)
#define _SeqLen    (32)

//TODO: Remove globals?
unsigned page_size = 0;
int      mem_fd = -1;

//Declarations
void e_debug_unmap(void *ptr);
int e_debug_read(unsigned addr, unsigned *data);
int e_debug_write(unsigned addr, unsigned data);
int e_debug_map(unsigned addr, void **ptr, unsigned *offset);
void e_debug_init(int version);

#define ELINK_BASE	0x81000000
#define ELINK_MAILBOXLO	(ELINK_BASE + 0xF0310)
#define ELINK_MAILBOXHI	(ELINK_BASE + 0xF0314)
#define ELINK_TXCFG	(ELINK_BASE + 0xF0210)
#define ELINK_RXCFG	(ELINK_BASE + 0xF0300)

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
			unsigned lastRow = row;
			row = row % platform.rows;

			if (row < lastRow)
			{
				break;
			}
		}
		
		// Draw a random core
		//row = rand() % platform.rows;
		//col = rand() % platform.cols;
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
		usleep(10000);
		e_read(&emem, 0, 0, 0x0, emsg, _BufSize);

		// Print the message and close the workgroup.
		printf("\"%s\"\n", emsg);
		e_close(&dev);
	
		col++;
	}

	// Release the allocated buffer and finalize the
	// e-platform connection.
	e_free(&emem);
	e_finalize();

	// TODO move this into the for next loop above
	// once there is a way to fetch the mailbox content with the
	// normal e-hal
	unsigned int mailboxEntry;
	e_debug_read(ELINK_MAILBOXHI, &mailboxEntry);
	printf("HI: 0x%x: 0x%x;  ", ELINK_MAILBOXHI, mailboxEntry);
	e_debug_read(ELINK_MAILBOXLO, &mailboxEntry);
	printf("LO: 0x%x: 0x%x\n", ELINK_MAILBOXLO, mailboxEntry);
	e_debug_read(ELINK_TXCFG, &mailboxEntry);
	printf("TxCFG: 0x%x: 0x%x\n", ELINK_TXCFG, mailboxEntry);
	e_debug_read(ELINK_RXCFG, &mailboxEntry);
	printf("RxCFG: 0x%x: 0x%x\n", ELINK_RXCFG, mailboxEntry);	
	return 0;
}

//############################################
//# Read from device
//############################################
int e_debug_read(unsigned addr, unsigned *data) {

  int  ret;
  unsigned offset;
  char *ptr;

  //Debug
  //printf("read addr=%08x data=%08x\n", addr, *data);
  //fflush(stdout);

  //Map device into memory
  ret = e_debug_map(addr, (void **)&ptr, &offset);

  //Read value from the device register  
  *data = *((unsigned *)(ptr + offset));

  //Unmap device memory
  e_debug_unmap(ptr);

  return 0;
}
//############################################
//# Write to device
//############################################
int e_debug_write(unsigned addr, unsigned data) {
  int  ret;
  unsigned offset;
  char *ptr;

  //Debug
  //printf("write addr=%08x data=%08x\n", addr, data);
  //fflush(stdout);
  //Map device into memory
  ret = e_debug_map(addr, (void **)&ptr, &offset);

  //Write to register
  *((unsigned *)(ptr + offset)) = data;

  //Unmap device memory
  e_debug_unmap(ptr);

  return 0;
}

//############################################
//# Map Memory Using Generic Epiphany driver
//############################################
int e_debug_map(unsigned addr, void **ptr, unsigned *offset) {

  unsigned page_addr; 

  //What does this do??
  if(!page_size)
    page_size = sysconf(_SC_PAGESIZE);

  //Open /dev/mem file if not already
  if(mem_fd < 1) {
    mem_fd = open ("/dev/epiphany", O_RDWR);
    if (mem_fd < 1) {
      perror("f_map");
      return -1;
    }
  }

  //Get page address
  page_addr = (addr & (~(page_size-1)));

  if(offset != NULL)
    *offset = addr - page_addr;

  //Perform mmap
  *ptr = mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_SHARED, 
	      mem_fd, page_addr);

  //Check for errors
  if(*ptr == MAP_FAILED || !*ptr)
      return -2;
  else
      return 0;
}	

//#########################################
//# Unmap Memory
//#########################################
void e_debug_unmap(void *ptr) {
  
    //Unmap memory
    if(ptr && page_size){
	munmap(ptr, page_size);
    }
}
