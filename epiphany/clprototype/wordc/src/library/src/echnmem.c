/************************************************************
 *
 * echnmem.c
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

#include <echnmem.h>
#include <ecommsbase.h>

// Forward declarations
void _ec_alloc(msg_channel_t *channel, ec_memhdr_t *cur, ec_memhdr_t *next);
void _ec_init(msg_channel_t *channel, ec_memhdr_t *item);

#ifdef __epiphany__

//void *e_read(const void *remote, void *dst, unsigned row, unsigned col, const void *src, size_t n);
//void *e_write(const void *remote, const void *src, unsigned row, unsigned col, void *dst, size_t n);

size_t ec_read(const msg_channel_t *channel, void *dst, size_t offset, size_t n)
{
    e_read(channel->mem,
           dst,
           channel->row,
           channel->col,
           (void*)offset,
           n);
    return n;
}

size_t ec_write(const msg_channel_t *channel, const void *src, size_t offset, size_t n)
{
    e_write(channel->mem,
            src,
            channel->row,
            channel->col,
            (void*)offset,
            n);
    return n;
}

#else

// Its ARM (for now)

//ssize_t e_read(void *dev, unsigned row, unsigned col, off_t from_addr, void *buf, size_t size);
//ssize_t e_write(void *dev, unsigned row, unsigned col, off_t to_addr, const void *buf, size_t size);

size_t ec_read(const msg_channel_t *channel, void *dst, size_t offset, size_t n)
{
    ssize_t rtn;
    rtn = e_read(channel->mem,
                 channel->row,
                 channel->col,
                 (off_t)offset,
                 dst,
                 n);
    return (size_t)rtn;
}

size_t ec_write(const msg_channel_t *channel, const void *src, size_t offset, size_t n)
{
    ssize_t rtn;
    rtn = e_write(channel->mem,
                  channel->row,
                  channel->col,
                  (off_t)offset,
                  (void*)src,
                  n);
    return (size_t)rtn;
}

#endif

// find / check channel space
int ec_channel_space(msg_channel_t *channel, const size_t offset, const size_t size)
{
    return channel->limit >= offset + size;
}

int ecl_find_space(msg_channel_t *channel, const size_t n, size_t *offset)
{
    int rtn = SUCCESS;

    *offset = channel->offset;

    if ( !ec_channel_space(channel, *offset, n) )
    {
        *offset = channel->initialoffset + sizeof(ec_memhdr_t);
        if ( !ec_channel_space(channel, *offset, n) )
        {
            rtn = FAIL;
        }
    }

    return rtn;
}

//
// WARNING the following methods use direct physical memory access
//         so use with caution!
//

#ifndef __epiphany__
// allocate channel on host
int ecl_alloc_mem_channel(msg_channel_t *channel, const char* name, const size_t size)
{
    int rtn = msg_channel_alloc(channel, name, size + sizeof(ec_memhdr_t));
    if (rtn)
    {
        ec_memhdr_t *head = (ec_memhdr_t *)(channel->phy_base + channel->initialoffset);
        _ec_init(channel, head);
        channel->offset += sizeof(ec_memhdr_t);
    }

    return rtn;
}

void ecl_free_mem_channel(msg_channel_t *channel)
{
    ec_memhdr_t *head = (ec_memhdr_t *)(channel->phy_base + channel->initialoffset);
    _ec_init(channel, head);
    msg_reset(channel);

    msg_channel_free(channel);
}

#endif

// initial previously allocated channel on epiphany
int ec_init_mem(msg_channel_t *channel, const char* name)
{
    int rtn = msg_channel_init(channel, name);

    if (rtn)
    {
        ec_memhdr_t head;
        uc_read(channel, &head, sizeof(ec_memhdr_t));

        if (head.q.bck)
        {
            // Note all the pointer addresses are local to the producer
            //      particularly important for host - ecore comms
            rtn = FAIL;
        }

        // Note that channel->offset will not be used on the consumer side
        //      at the moment.  Instead the offset is in the command sent
        //      by the producer.
        channel->offset += sizeof(ec_memhdr_t);
    }

    return rtn;
}

// channel administration
int ec_msg_reset(msg_channel_t *channel)
{
    channel->offset = channel->initialoffset + sizeof(ec_memhdr_t);
    return SUCCESS;
}

// allocate new fifo item
// assume that the memory is used like a fifo to keep mem free / alloc easier
int ecl_alloc(msg_channel_t *channel, size_t n, size_t *offset)
{
    size_t aligned = em_align(n);
    int rtn = ecl_find_space(channel, aligned, offset);

    ec_header_t *curh;
    ec_memhdr_t *cur;
    curh = container_of((unsigned *)(*offset + channel->phy_base), ec_header_t, data);
    cur = (ec_memhdr_t *)curh;
    if (rtn && ( curh->hdr.hd.busy || ((channel->phy_base  + channel->initialoffset) != (size_t)curh->hdr.q.head) ) )
    {
        rtn = FAIL;
    }

    ec_memhdr_t *next = (ec_memhdr_t *)((size_t)cur + sizeof(ec_memhdr_t) + aligned);
    if (rtn && cur->q.fwd)
    {
        if ( (size_t)next > (size_t)cur->q.fwd )
        {
            // No room
            rtn = FAIL;
        }
    }

    if (rtn)
    {
        channel->offset = *offset;
        _ec_alloc(channel, cur, next);
    }

    if (!rtn)
    {
        *offset = (size_t)CHMEND;
    }

    return rtn;
}

void ecl_free(msg_channel_t *channel, size_t offset)
{
    ec_header_t *curh;
    ec_memhdr_t *cur;
    curh = container_of((unsigned *)(offset + channel->phy_base), ec_header_t, data);
    cur = (ec_memhdr_t *)curh;
    if ((size_t)cur->q.head != (channel->phy_base  + channel->initialoffset))
    {
        return;
    }

    ec_memhdr_t *fwd = (ec_memhdr_t *)cur->q.fwd;
    ec_memhdr_t *bck = (ec_memhdr_t *)cur->q.bck;
    cur->hd.busy = MEM_IDLE;

    if ( bck && !bck->hd.busy )
    {
        el_remove((el_t *)cur);
        ec_header_t *bckh = (ec_header_t *)bck;
        if (bckh && (channel->offset == (size_t)&curh->data - channel->phy_base))
        {
            channel->offset = (size_t)&bckh->data - channel->phy_base;
        }
    }

    if (fwd && !fwd->hd.busy)
    {
        curh = (ec_header_t *)fwd;
        ec_header_t *bckh = (ec_header_t *)curh->hdr.q.bck;
        if (bckh && (channel->offset == (size_t)&curh->data - channel->phy_base))
        {
            channel->offset = (size_t)&bckh->data - channel->phy_base;
        }

        el_remove((el_t *)fwd);
    }
}

void _ec_alloc(msg_channel_t *channel, ec_memhdr_t *cur, ec_memhdr_t *next)
{
    size_t full = sizeof(ec_header_t) + (size_t)next - (size_t)cur;
    if (ecl_size(channel, channel->offset) >= full)
    {
        // Insert a new header
        _ec_init(channel, next);
        el_insert((el_t *)next, (el_t *)cur);
        ec_header_t *nxt = (ec_header_t *)next;
        ec_header_t *curh = (ec_header_t *)cur;
        channel->offset += (size_t)&nxt->data - (size_t)&curh->data;
    } else {
        // Use existing header
        ec_header_t *nxt = (ec_header_t *)cur->q.fwd;
        if (nxt)
        {
            ec_header_t *curh = (ec_header_t *)cur;
            channel->offset += (size_t)(&nxt->data) - (size_t)(&curh->data);
        } else {
            channel->offset = channel->initialoffset + sizeof(ec_memhdr_t);
        }
    }

    cur->hd.busy = MEM_BUSY;
}

// memory allocation linking
void _ec_init(msg_channel_t *channel, ec_memhdr_t *item)
{
    el_init(&item->q, (el_t *)(channel->phy_base + channel->initialoffset));
    item->hd.busy = MEM_IDLE;
}

// mark memory chunk free
int ec_mark_mem_free(const msg_channel_t *channel, size_t offset)
{
    ec_header_t *curh;
    curh = container_of((unsigned *)(offset + channel->phy_base), ec_header_t, data);

    curh->hdr.hd.busy = MEM_IDLE;

    return SUCCESS;
}

// free space at current allocation offset
int ecl_free_mem_space(msg_channel_t *ecchannel, size_t memsize)
{
    int rtn = SUCCESS;

    // find offset for chunk large enough for data
    size_t offset;
    rtn = ecl_find_space(ecchannel, memsize, &offset);

    // check if chunk is free
    size_t choffset = offset;
    while (rtn)
    {
        if (rtn && ec_busy(ecchannel, choffset))
        {
            rtn = FAIL;
            break;
        } else {
            ecl_free(ecchannel, choffset);
            rtn = ecl_find_space(ecchannel, memsize, &offset);
            choffset = offset;
        }

        // check if space
        size_t available = ecl_size(ecchannel, offset);
        if (0 == available)
        {
            rtn = FAIL;
            break;
        }

        if (available >= memsize)
        {
            break;
        }

        // goto next chunk
        if (!ecl_next_data(ecchannel, &choffset))
        {
            rtn = FAIL;
            break;
        }
    }

    return rtn;
}

// information about allocated items
int ec_busy(const msg_channel_t *channel, size_t loc)
{
    ec_header_t *curh;
    curh = container_of((unsigned *)(loc + channel->phy_base), ec_header_t, data);

    return curh->hdr.hd.busy;
}

// remaining space
size_t ecl_remaining(const msg_channel_t *channel)
{
    size_t size = 0;

    if (!ec_busy(channel, channel->offset))
    {
        size = ecl_size(channel, channel->offset);
    }

    return size;
}

// size of chunk
size_t ecl_size(const msg_channel_t *channel, size_t loc)
{
    size_t size = 0;
    ec_header_t *curh;
    curh = container_of((unsigned *)(loc + channel->phy_base), ec_header_t, data);

    if (curh->hdr.q.fwd)
    {
        if ((size_t)curh->hdr.q.fwd > (size_t)&curh->data)
        {
            size = (size_t)curh->hdr.q.fwd - (size_t)&curh->data;
        } else {
            size = 0;
        }
    } else {
        size_t inuse = (size_t)&curh->data - (channel->phy_base + channel->initialoffset);
        if (channel->size > inuse)
        {
            size = channel->size - inuse;
        } else {
            size = 0;
        }
    }

    return size;
}

// moving to next data
int ecl_next_data(const msg_channel_t *channel, size_t *offset)
{
    int rtn = SUCCESS;

    ec_header_t *curh;
    ec_memhdr_t *cur;
    curh = container_of((unsigned *)(*offset + channel->phy_base), ec_header_t, data);
    cur = (ec_memhdr_t *)curh;
    ec_header_t *nxt = (ec_header_t *)cur->q.fwd;
    if (nxt)
    {
        *offset += (size_t)(&nxt->data) - (size_t)(&curh->data);
    } else {
        rtn = FAIL;
    }

    return rtn;
}

// inserting data into chunk
int ecl_insert(const msg_channel_t *channel, size_t chunkloc, size_t offset, char *src, size_t size)
{
    int rtn = SUCCESS;
    size_t space = ecl_size(channel, chunkloc);
    size_t end = chunkloc + space;
    if ((offset + size) > end)
    {
        rtn = FAIL;
    }

    if (chunkloc > offset || offset > end)
    {
        rtn = FAIL;
    }

    if (rtn)
    {
        ec_write(channel, (void *)src, offset, size);
    }

    return rtn;
}

// physical address
size_t ec_phyaddr(const msg_channel_t *channel, size_t offset)
{
    return channel->phy_base + offset;
}

#ifdef __epiphany__
// Construct dma descriptor
int ec_dmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    size_t size,
    e_dma_id_t dma_id,
    e_dma_config_t cfg)
{
    // Create the dma descriptor
    e_dma_desc_t *next_desc = NULL;
    if (cfg & E_DMA_CHAIN)
    {
        next_desc = desc + 1;
    }

    unsigned stride_i_src, stride_i_dst, count_i, count_o, stride_o_src, stride_o_dst;
    stride_i_src = 4;
    stride_i_dst = 4;
    count_i = em_align(size)/4;
    count_o = 1;
    stride_o_src = 1;
    stride_o_dst = 1;
    e_dma_set_desc(
        dma_id,
        (E_DMA_ENABLE|E_DMA_MASTER|E_DMA_WORD|(cfg & E_DMA_CHAIN)|(cfg & E_DMA_IRQEN)),
        next_desc,
        stride_i_src, stride_i_dst,
        count_i, count_o,
        stride_o_src, stride_o_dst,
        (void *)src, (void *)dst,
        desc);

    return SUCCESS;
}

int ec_mdmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    unsigned ssep,
    unsigned dsep,
    unsigned count,
    e_dma_id_t dma_id,
    e_dma_config_t cfg)
{
    // Create the dma descriptor
    e_dma_desc_t *next_desc = NULL;
    if (cfg & E_DMA_CHAIN)
    {
        next_desc = desc + 1;
    }

    unsigned stride_i_src, stride_i_dst, count_i, count_o, stride_o_src, stride_o_dst;
    stride_i_src = 4 * ssep;
    stride_i_dst = 4 * dsep;
    count_i = count;
    count_o = 1;
    stride_o_src = 1;
    stride_o_dst = 1;
    e_dma_set_desc(
        dma_id,
        (E_DMA_ENABLE|E_DMA_MASTER|E_DMA_WORD|(cfg & E_DMA_CHAIN)|(cfg & E_DMA_IRQEN)),
        next_desc,
        stride_i_src, stride_i_dst,
        count_i, count_o,
        stride_o_src, stride_o_dst,
        (void *)src, (void *)dst,
        desc);

    return SUCCESS;
}

int ec_dma_append(
    msg_channel_t *channel,
    e_dma_desc_t *desc,
    e_dma_id_t dma_id,
    const size_t src,
    const size_t n)
{
    int rtn = SUCCESS;
    size_t dst = ec_phyaddr(channel, channel->offset);

    if (!ec_dmadesc(src,dst, desc, n, dma_id, 0))
    {
        rtn = FAIL;
    }

    if ( rtn && msg_space(channel, n) )
    {
        // Write the message to memory
        if (e_dma_start(desc, dma_id))
        {
            rtn = FAIL;
        } else {
            channel->offset += n;
        }
    } else {
        // Memory region is too small for the message
        rtn = FAIL;
    }

    return rtn;
}
#endif
