/*
  e_mutex.c

  Copyright (c) 2015-2025 Peter Saunderson <peteasa@gmail.com>

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

#include <stdlib.h>
#include <string.h>

#include "e_lib.h"

char shared_outbuf[128*16] SECTION("shared_dram");
e_mutex_t mutex SECTION(".data_bank3");
volatile e_coreid_t *registerEntry SECTION(".data_bank3");
volatile e_coreid_t coreregister[16] SECTION(".data_bank2");
volatile int interruptResult SECTION(".data_bank3");

void user_isr();

int main(void) {
    interruptResult = 0x0;
    e_irq_attach(E_USER_INT, user_isr);
    e_irq_mask(E_USER_INT, E_FALSE);
    e_irq_global_mask(E_FALSE);

    e_coreid_t coreid;
    int i, j;

    // init the queue pointer for this core
    registerEntry = coreregister;

    // Who am I? Query the CoreID from hardware.
    coreid = e_get_coreid();

    unsigned myrow, mycol, row, col;
    e_coords_from_coreid(coreid, &myrow, &mycol);

    // Calculate the outbuf for this core
    uint32_t *outbuf;
    outbuf = (uint32_t *) (shared_outbuf + (myrow * 4 + mycol) * 128);

    *outbuf++ = (uint32_t) coreid;

    e_coreid_t neighbourid;

    // Print the coreid of the surrounding 4 cores
    for (i=0; i<2; i++)
    {
        e_neighbor_id(E_PREV_CORE + i, E_ROW_WRAP, &row, &col);
        neighbourid = e_coreid_from_coords(row, col);
        *outbuf++ = (uint32_t) neighbourid;

        // Send an interrupt to the neighbour
        e_irq_set(row, col, E_USER_INT);
    }

    for (i=0; i<2; i++)
    {
        e_neighbor_id(E_PREV_CORE + i, E_COL_WRAP, &row, &col);
        neighbourid = e_coreid_from_coords(row, col);
        *outbuf++ = (uint32_t) neighbourid;

        // Send an interrupt to the neighbour
        e_irq_set(row, col, E_USER_INT);
    }

    // flags has a bit for every core in the group
    unsigned flags;

    // compiler fails to process operator *
    // flags = (0x1 << (e_group_config.group_rows * e_group_config.group_cols)) - 1;
    flags = (0x1 << (e_group_config.group_rows * e_group_config.group_cols)) - 1;

    // No need to visit this core
    // compiler fails to process operator *
    // flags = flags & ~(0x1 << (myrow * e_group_config.group_cols + mycol));
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
                    e_coreid_t *dst;
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

    *outbuf++ = (uint32_t) interruptResult;
    interruptResult = 0;

    // Now put the CPU to sleep till and interrupt occurs
    __asm__ __volatile__ ("idle");

    *outbuf++ = (uint32_t) interruptResult;

    // write an end of buffer sequence
    *outbuf++ = 0xAAAAAAAA;
    *outbuf++ = 0x55555555;
    *outbuf++ = 0xA5A5A5A5;
    *outbuf++ = 0;

    return EXIT_SUCCESS;
}

void __attribute__((interrupt)) user_isr()
{
    coreregister[15]++;
    interruptResult++;

    // Enable the interrupt
    e_irq_mask(E_USER_INT, E_FALSE);
}
