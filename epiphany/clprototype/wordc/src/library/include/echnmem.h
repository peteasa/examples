/************************************************************
 *
 * echnmem.h
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

#ifndef ECHNMEM
#define ECHNMEM

#include <ebase.h>
#include <emsgbase.h>

#define CHMEND (~0)

typedef enum {
    MEM_IDLE = 0,
    MEM_BUSY
} ec_mem_status;

// For compatibility between arm and epiphany
//    el_t is 3 32 bit words, epiphany gcc adds another 32 bit word
//    for 64 bit alignment.  Use the extra space for busy
struct ecl_t;
typedef struct ecl_t {
    struct ecl_t *head;     // Same as el_t
    struct ecl_t *fwd;      // Same as el_t
    struct ecl_t *bck;      // Same as el_t
    ec_mem_status busy;     // Note different packing of el_t on Epiphany and Arm
                            // e_gcc uses 32 bit padding for 64 bit alignment
} ecl_t;

typedef union ec_memhdr_t {
    ecl_t hd;
    el_t q;
} ec_memhdr_t;

typedef struct {
    ec_memhdr_t hdr;
    unsigned data;            // start of data
} ec_header_t;

// mapping e_lib and e-hal functions to a single api
extern size_t ec_read(const msg_channel_t *channel, void *dst, size_t offset, size_t n);
extern size_t ec_write(const msg_channel_t *channel, const void *src, size_t offset, size_t n);

// find / check channel space
extern int ec_channel_space(msg_channel_t *channel, const size_t offset, const size_t size);
extern int ecl_find_space(msg_channel_t *channel, size_t n, size_t *offset);

//
// WARNING these methods use direct physical memory access
//         so use with caution!
//

// allocate channel on host
#ifndef __epiphany__
extern int ecl_alloc_mem_channel(msg_channel_t *channel, const char* name, const size_t size);
extern void ecl_free_mem_channel(msg_channel_t *channel);
#endif

// initial previously allocated channel on epiphany
extern int ec_init_mem(msg_channel_t *channel, const char* name);

// channel administration
extern int ec_msg_reset(msg_channel_t *channel);

// allocate new fifo item
extern int ecl_alloc(msg_channel_t *channel, size_t n, size_t *offset);
extern void ecl_free(msg_channel_t *channel, size_t offset);

// mark memory chunk free
extern int ec_mark_mem_free(const msg_channel_t *channel, size_t offset);

// free space at current allocation offset
extern int ecl_free_mem_space(msg_channel_t *ecchannel, size_t memsize);

// information about allocated items
extern int ec_busy(const msg_channel_t *channel, size_t loc);
extern size_t ecl_remaining(const msg_channel_t *channel);
extern size_t ecl_size(const msg_channel_t *channel, size_t loc);

// moving to next data
extern int ecl_next_data(const msg_channel_t *channel, size_t *offset);

// inserting data into chunk
extern int ecl_insert(const msg_channel_t *channel, size_t chunkloc, size_t offset, char *src, size_t size);

// physical address
extern size_t ec_phyaddr(const msg_channel_t *channel, size_t offset);

#ifdef __epiphany__
extern int ec_dmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    size_t size,
    e_dma_id_t dma_id,
    e_dma_config_t cfg);
extern int ec_mdmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    unsigned ssep,
    unsigned dsep,
    unsigned count,
    e_dma_id_t dma_id,
    e_dma_config_t cfg);
extern int ec_dma_append(
    msg_channel_t *channel,
    e_dma_desc_t *desc,
    e_dma_id_t dma_id,
    const size_t src,
    const size_t n);
#endif

#endif
