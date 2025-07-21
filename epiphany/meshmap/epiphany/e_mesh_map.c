/*
  e_mem_map.c

  Copyright (c) 2015-2025 Peter Saunderson <peteasa@gmail.com>

  based on hello_world.c:

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

#include <stdlib.h>
#include <string.h>

#include "e_lib.h"

// Not obvious here why shared_outbuf is at the start of shared_dram
// Might be better to store the location and send a message to the host with the address?
char shared_outbuf[128*16] SECTION("shared_dram");

int main(void) {
    e_coreid_t coreid;
    int i, j;

    // Who am I? Query the CoreID from hardware.
    coreid = e_get_coreid();

    unsigned myrow, mycol, row, col;
    e_coords_from_coreid(coreid, &myrow, &mycol);

    // Calculate the outbuf for this core
    uint32_t *outbuf;
    outbuf = (uint32_t *) (shared_outbuf + (myrow * 4 + mycol) * 128);

    *outbuf++ = (uint32_t) coreid;

    e_coreid_t neighbourid;

    // Output the coreid of the surrounding 4 cores
    for (i=0; i<2; i++)
    {
        e_neighbor_id(E_PREV_CORE + i, E_ROW_WRAP, &row, &col);
        neighbourid = e_coreid_from_coords(row, col);
        *outbuf++ = (uint32_t) neighbourid;
    }

    for (i=0; i<2; i++)
    {
        e_neighbor_id(E_PREV_CORE + i, E_COL_WRAP, &row, &col);
        neighbourid = e_coreid_from_coords(row, col);
        *outbuf++ = (uint32_t) neighbourid;
    }

    void * dst;
    dst = (void *)(0x5000 + (myrow * 4 + mycol) * sizeof(e_coreid_t));
    for (i=0; i<e_group_config.group_rows; i++)
    {
        for (j=0; j<e_group_config.group_cols; j++)
        {
            // Visit all the cores in the group and write my coreid to the memory of the core
            e_write(&e_group_config, &coreid, i, j, dst, sizeof(e_coreid_t));
        }
    }

    // write an end of buffer sequence
    *outbuf++ = 0xAAAAAAAA;
    *outbuf++ = 0x55555555;
    *outbuf++ = 0xA5A5A5A5;
    *outbuf++ = 0;

    return EXIT_SUCCESS;
}
