/*
  e_hello_world.c

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

#include "e_lib.h"

char shared_outbuf[128*16] SECTION("shared_dram");
e_mutex_t mutex SECTION(".data_bank3");
volatile e_coreid_t *registerEntry SECTION(".data_bank3");
volatile e_coreid_t coreregister[16] SECTION(".data_bank2");
volatile int interruptResult SECTION(".data_bank3");

void user_isr(int);

int main(void) {
	interruptResult = 0x99;
	e_irq_attach(E_USER_INT, user_isr);
	e_irq_mask(E_USER_INT, E_FALSE);
	e_irq_global_mask(E_FALSE);

	e_coreid_t coreid;
	int i, j;

	// initial the mutex for this core
	e_mutex_init(e_group_config.core_row, e_group_config.core_col, &mutex, MUTEXATTR_NULL);

	// init the queue pointer for this core
	registerEntry = coreregister;

	// Who am I? Query the CoreID from hardware.
	coreid = e_get_coreid();

	unsigned myrow, mycol, row, col;
	e_coords_from_coreid(coreid, &myrow, &mycol);

	// Calculate the outbuf for this core
	char * outbuf;
	outbuf = shared_outbuf + (myrow * 4 + mycol) * 128;

	// Remove sprintf and link against fastest libraries?

	// The PRINTF family of functions do not fit
	// in the internal memory, so we link against
	// the FAST.LDF linker script, where these
	// functions are placed in external memory.
	sprintf(outbuf, "core 0x%03x ", coreid);

	e_coreid_t neighbourid;

	// Print the coreid of the surrounding 4 cores
	for (i=0; i<2; i++)
	{
		e_neighbor_id(E_PREV_CORE + i, E_ROW_WRAP, &row, &col);
		neighbourid = e_coreid_from_coords(row, col);
		sprintf(outbuf + strlen(outbuf), "0x%03x ", neighbourid);

		// Send an interrupt to the neighbour
		e_irq_set(row, col, E_USER_INT);
	}

	for (i=0; i<2; i++)
	{
		e_neighbor_id(E_PREV_CORE + i, E_COL_WRAP, &row, &col);
		neighbourid = e_coreid_from_coords(row, col);
		sprintf(outbuf + strlen(outbuf), "0x%03x ", neighbourid);

		// Send an interrupt to the neighbour
		e_irq_set(row, col, E_USER_INT);
	}

	// flags has a bit for every core in the group
	unsigned flags;
	flags = (0x1 << (e_group_config.group_rows * e_group_config.group_cols)) - 1;

	// No need to visit this core
	flags = flags & ~(0x1 << (myrow * e_group_config.group_cols + mycol));

	while (flags != 0)
	{
		// Visit all the cores in the group
		int rowmulti;
		rowmulti = 0;
		for (i=0; i<e_group_config.group_rows; i++)
		{
			for (j=0; j<e_group_config.group_cols; j++)
			{
				if (0 != (flags & (0x1 << (rowmulti + j))))
				{
					e_coreid_t * dst;
					if ((0 == e_mutex_trylock(i,j, &mutex)))
					{
						// write my coreid to the memory of the core
						// First find the next free location
						e_read(&e_group_config, &dst, i, j, &registerEntry, sizeof(e_coreid_t*));
	
						// Write the coreid to the list
						e_write(&e_group_config, &coreid, i, j, (void *)dst, sizeof(e_coreid_t));

						// Update the queue pointer
						dst++;
						e_write(&e_group_config, &dst, i, j, &registerEntry, sizeof(e_coreid_t*));
						e_mutex_unlock(i,j, &mutex);
				
						// mark as done
						flags = flags & ~(0x1 << (rowmulti + j));
					}
				}
			}

			rowmulti += e_group_config.group_cols;
		}
	}

	sprintf(outbuf + strlen(outbuf), "bf: 0x%x ", interruptResult);
	interruptResult = 0;

	// Now put the CPU to sleep till and interrupt occurs
	__asm__ __volatile__ ("idle");

	sprintf(outbuf + strlen(outbuf), "Int: 0x%x", interruptResult);

	return EXIT_SUCCESS;
}

void __attribute__((interrupt)) user_isr(int signum)
{
	coreregister[15]++;
	interruptResult += signum;

	// Enable the interrupt
	e_irq_mask(E_USER_INT, E_FALSE);
}
