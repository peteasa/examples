/************************************************************
 *
 * ecommsbase.h
 *
 * A prototype for the Map Reduce algorithm to count
 * words in a book
 *
 * Copyright (c) 2017 Peter Saunderson <peteasa@gmail.com>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *************************************************************/

#ifndef EMSGBASE
#define EMSGBASE

// From epiphany_elf_gcc pre compiler defines
#ifdef __epiphany__
#include <e_lib.h>
#else
#include <e-hal.h>
#endif

typedef struct {
    unsigned row;         // row of remote eCore (or 0 for shm)
    unsigned col;         // col of remote eCore (or 0 for shm)
    size_t initialoffset; // start address of channel memory
    size_t offset;
    size_t sgmntsize;
    size_t limit;
    size_t size;
    size_t phy_base;      // Used in DMA transfers
    void *mem;            // Either &e_group_config or &emem or e_epiphany_t *
#ifdef __epiphany__
    e_memseg_t emem;
#else
    e_mem_t emem;
#endif
} msg_channel_t;

#endif
