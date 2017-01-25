/************************************************************
 *
 * uchannel.c
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

#include <stdlib.h>
#include <ecommsbase.h>
#include <uchannel.h>

// Peer list
typedef struct {
    unsigned row;
    unsigned col;
} uc_peer;
uc_peer RankToPeer[16];

#ifdef __epiphany__
int msg_uchannel_init(msg_channel_t *channel, const e_group_config_t *e_group_config, const size_t offset, size_t size)
{
    channel->mem = (void*)e_group_config;
    channel->phy_base = (size_t)NULL;
#else
int msg_uchannel_init(msg_channel_t *channel, const e_epiphany_t *dev, const size_t offset, size_t size)
{
    channel->mem = (void*)dev;
    channel->phy_base = (size_t)NULL;
#endif
    channel->row = 0; // default to eCore 0,0 as the remote eCore
    channel->col = 0; // default to eCore 0,0 as the remote eCore
    channel->initialoffset = offset;
    channel->size = size;
    channel->sgmntsize = channel->size;
    channel->limit = channel->initialoffset + channel->size;

    msg_reset(channel);

    return SUCCESS;
}

int msg_uchannel_local_init(msg_channel_t *channel, const size_t offset, size_t size, size_t sgmntsize)
{
    channel->mem = NULL; // Not used
    channel->phy_base = (size_t)NULL; // Not used
    channel->row = 0; // Not used
    channel->col = 0; // Not used
    channel->initialoffset = offset;
    channel->size = size;
    channel->sgmntsize = sgmntsize;
    channel->limit = channel->initialoffset + channel->size;

    msg_reset(channel);

    return SUCCESS;
}

void msg_uchannel_init_rank(const e_group_config_t *e_group_config)
{
    unsigned rank;
    for (rank = 0; rank<e_group_config->group_rows * e_group_config->group_cols; rank++)
    {
        RankToPeer[rank].row = rank / e_group_config->group_cols;
        RankToPeer[rank].col = rank % e_group_config->group_cols;
    }
}

void msg_uchannel_set_ecore(msg_channel_t *channel, unsigned row, unsigned col)
{
    channel->row = row;
    channel->col = col;
#ifdef __epiphany__
    char *ecorebase = NULL;
    channel->phy_base = (size_t)e_get_global_address(row, col, ecorebase);
#else
    e_epiphany_t *dev = (e_epiphany_t *)channel->mem;
    channel->phy_base = (size_t)dev->core[row][col].mems.base;
#endif
}

void msg_uchannel_set_rank(msg_channel_t *channel, unsigned rank)
{
    channel->row = RankToPeer[rank].row;
    channel->col = RankToPeer[rank].col;
#ifdef __epiphany__
    char *ecorebase = NULL;
    channel->phy_base = (size_t)e_get_global_address(channel->row, channel->col, ecorebase);
#else
    e_epiphany_t *dev = (e_epiphany_t *)channel->mem;
    channel->phy_base = (size_t)dev->core[channel->row][channel->col].mems.base;
#endif
}

#ifdef __epiphany__
void msg_uchannel_int_peer(unsigned peer)
{
    e_irq_set(RankToPeer[peer].row, RankToPeer[peer].col, E_USER_INT);
}
#endif
