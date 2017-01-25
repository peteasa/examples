/************************************************************
 *
 * uchnmem.c
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
#include <uchannel.h>

// Forward declarations
void _ec_init_u(msg_channel_t *channel, ec_memhdr_t *item);

// channel allocation helpers
#ifdef __epiphany__
int ec_init_mem_uchannel(msg_channel_t *channel, const e_group_config_t *e_group_config, const size_t offset, size_t size)
{
    int rtn = msg_uchannel_init(channel, e_group_config, offset, size + sizeof(ec_memhdr_t));
#else

int ec_init_mem_uchannel(msg_channel_t *channel, const e_epiphany_t *dev, const size_t offset, size_t size)
{
    int rtn = msg_uchannel_init(channel, dev, offset, size + sizeof(ec_memhdr_t));
#endif

    if (rtn)
    {
        // Note that channel->offset will not be used on the consumer side
        //      at the moment.  Instead the offset is in the command sent
        //      by the producer.
        channel->offset += sizeof(ec_memhdr_t);
    }

    return rtn;
}

#ifdef __epiphany__

int ecl_alloc_local_mem_uchannel(msg_channel_t *channel, const size_t offset, size_t size)
{
    int rtn = msg_uchannel_local_init(channel, offset, size, size + sizeof(ec_memhdr_t));

    if (rtn)
    {
        ec_memhdr_t *head = (ec_memhdr_t *)(channel->phy_base + channel->initialoffset);
        _ec_init_u(channel, head);
        channel->offset += sizeof(ec_memhdr_t);
    }
 
    return rtn;
}

// memory allocation linking
void _ec_init_u(msg_channel_t *channel, ec_memhdr_t *item)
{
    el_init(&item->q, (el_t *)(channel->phy_base + channel->initialoffset));
    item->hd.busy = MEM_IDLE;
}

#endif

