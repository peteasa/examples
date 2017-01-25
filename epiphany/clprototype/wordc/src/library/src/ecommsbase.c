/************************************************************
 *
 * ecommsbase.c
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

//#include <stdio.h>
#include <string.h> // for strlen / memcpy
#include <echnmem.h> // for ec_* functions
#include <ecommsbase.h>

// From epiphany_elf_gcc pre compiler defines
#ifdef __epiphany__
#include <e_lib.h>
#else
#include <e-hal.h>
#endif

// forward declarations 
void _msg_channel_init(msg_channel_t *channel);

size_t uc_read(const msg_channel_t *channel, void *dst, size_t n)
{
    return ec_read(channel, dst, channel->offset, n);
}

size_t uc_write(const msg_channel_t *channel, const void *src, size_t n)
{
    return ec_write(channel, src, channel->offset, n);
}

#ifndef __epiphany__

int msg_channel_alloc(msg_channel_t *channel, const char* name, const size_t size)
{
    int rc;
    channel->size = size;
    channel->sgmntsize = size;
    channel->mem = &channel->emem;
    channel->row = 0;
    channel->col = 0;
    
    rc = e_shm_alloc(channel->mem, name, size);
    if (E_OK != rc)
    {
        //printf("msg_channel_alloc: mapping already exists: %d\n", rc);
        if (!msg_channel_init(channel, name) )
        {
            return FAIL;
        }
    }
    else
    {
        _msg_channel_init(channel);
    }

    return SUCCESS;
}

int msg_channel_shared_alloc(msg_channel_t *channel, const char* name, const size_t size, const unsigned grpsize)
{
    int ok = msg_channel_alloc(channel, name, size);

    if (ok)
    {
        channel->sgmntsize = channel->size / grpsize;
    }

    return ok;
}

void msg_channel_free(msg_channel_t *channel)
{
    e_free(channel->mem);
}

#endif

int msg_channel_init(msg_channel_t *channel, const char* name)
{
    channel->mem = &channel->emem;
    channel->row = 0;
    channel->col = 0;
    if (E_OK != e_shm_attach(channel->mem, name) ) {
		return FAIL;
	}

    _msg_channel_init(channel);

    return SUCCESS;
}

int msg_channel_shared_init(msg_channel_t *channel, const char* name, const unsigned grpsize, const unsigned rank)
{
    int rtn = SUCCESS;

    if (rank >= grpsize)
    {
        rtn = FAIL;
    }

    if (rtn && msg_channel_init(channel, name))
    {
        channel->sgmntsize = channel->size / grpsize;
        channel->initialoffset = (size_t)(channel->sgmntsize * rank);
        channel->limit = channel->initialoffset + channel->sgmntsize;
        channel->offset = channel->initialoffset;
    }
    else
    {
        rtn = FAIL;
    }

    return rtn;
}

void _msg_channel_init(msg_channel_t *channel)
{
    // start address of channel memory
    channel->initialoffset = 0;

#ifdef __epiphany__
    channel->size = channel->emem.size;
    channel->phy_base = (size_t)channel->emem.phy_base;
#else
    channel->phy_base = (size_t)channel->emem.base;
#endif
    channel->sgmntsize = channel->size;
    channel->limit = channel->size;

    msg_reset(channel);
}

int msg_reset(msg_channel_t *channel)
{
    channel->offset = channel->initialoffset;
    return SUCCESS;
}

int msg_space(msg_channel_t *channel, const size_t size)
{
    return ec_channel_space(channel, channel->offset, size);
}

int msg_append(msg_channel_t *channel, const void *src, const size_t n)
{
    if ( msg_space(channel, n) )
    {
        // Write the message to memory
        uc_write(channel, src, n);

        channel->offset += n;
    } else {
        // Memory region is too small for the message
        return FAIL;
    }

    return SUCCESS;
}

int msg_append_str(msg_channel_t *channel, const void *src)
{
    // Write the message (including the null terminating
    // character) to shared memory 
    if (!msg_append(channel, src, strlen(src)+1) )
    {
        // Shared memory region is too small for the message
        return FAIL;
    }

    // Remove the trailing null byte from the offset calculation
    channel->offset--;

    return SUCCESS;
}

void msg_get_sgmnt(msg_channel_t *channel, void** item)
{
    *item = (void*)(ec_phyaddr(channel, channel->offset));
}

void msg_goto_next_sgmnt(msg_channel_t *channel)
{
    channel->offset += channel->sgmntsize;
    if ( !msg_space(channel, channel->sgmntsize) )
    {
        // Not enought space for the next segment
        msg_reset(channel);
    }
}

void msg_goto_sgmnt(msg_channel_t *channel, unsigned sgmnt)
{
    msg_reset(channel);
    channel->offset += channel->sgmntsize * sgmnt;
    if ( !msg_space(channel, channel->sgmntsize) )
    {
        // Not enought space for the next segment
        msg_reset(channel);
    }
}
