/*
  e_mailbox.c (based on e_hello_world)

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

// This is the DEVICE side of the Hello World example.
// The host may load this program to any eCore. When
// launched, the program queries the CoreID and prints
// a message identifying itself to the shared external
// memory buffer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <e-lib.h>

struct e_message {
	uint32_t from;
	uint32_t data;
} __attribute__((packed)) __attribute__((aligned(8)));

#define FOO_ADDR 0x8e000000
#define MAILBOX_ADDR 0x810F0730
/* TODO: Move to e-lib */
void e_send_message(uint32_t data)
{
	volatile struct e_message *mailbox = (struct e_message *) MAILBOX_ADDR;
	struct e_message msg;
	int i;

	msg.from = e_get_coreid();
	msg.data = data;
	msg.from = data;

	/* FIXME: 64-bit burst writes to same address is broken in FPGA elink.
	 * For now resort to 32-bit messages */
	__asm__ __volatile__ (
		"str %[msg],[%[mailbox]]"
		:
		: [msg] "r" (msg), [mailbox] "r" (mailbox)
		: "memory");
}

int main(void) 
{
	const char	ShmName[] = "hello_shm"; 
	const char      Msg[] = "Hello World from core 0x%03x!";
	char            buf[256] = { 0 };
	e_coreid_t coreid;
	e_memseg_t   	emem;
	unsigned        my_row;
	unsigned        my_col;
	
	// Who am I? Query the CoreID from hardware.
	coreid = e_get_coreid();
	e_coords_from_coreid(coreid, &my_row, &my_col);

	if ( E_OK != e_shm_attach(&emem, ShmName) ) {
		return EXIT_FAILURE;
	}
	
	// The PRINTF family of functions do not fit
	// in the internal memory, so we link against
	// the FAST.LDF linker script, where these
	// functions are placed in external memory.
	// Attach to the shm segment
	snprintf(buf, sizeof(buf), Msg, coreid);
	
	if ( emem.size >= strlen(buf) + 1 ) {
		// Write the message (including the null terminating
		// character) to shared memory
		e_write((void*)&emem, buf, my_row, my_col, NULL, strlen(buf) + 1);
	} else {
		// Shared memory region is too small for the message
		return EXIT_FAILURE;
	}

	/* Create delay so host app have to wait */
	int i;
	for (i = 0; i < 10000000; i++)
		__asm__ __volatile__ ("nop" ::: "memory");
		
	// Write coreid to the mailbox
	e_send_message(coreid);

	return EXIT_SUCCESS;
}
