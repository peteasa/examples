/************************************************************
 *
 * uchannel.h
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

#ifndef UCHANNEL
#define UCHANNEL

#include <emsgbase.h>

// channel allocation helpers
#ifdef __epiphany__
extern int msg_uchannel_init(msg_channel_t *channel, const e_group_config_t *e_group_config, const size_t offset, size_t size);
#else
extern int msg_uchannel_init(msg_channel_t *channel, const e_epiphany_t *dev, const size_t offset, size_t size);
#endif
extern int msg_uchannel_local_init(msg_channel_t *channel, const size_t offset, size_t size, size_t sgmntsize);
extern void msg_uchannel_init_rank(const e_group_config_t *e_group_config);

// channel administration
extern void msg_uchannel_set_ecore(msg_channel_t *channel, unsigned row, unsigned col);
extern void msg_uchannel_set_rank(msg_channel_t *channel, unsigned rank);

#ifdef __epiphany__
// interrupt peer ecore
void msg_uchannel_int_peer(unsigned peer);
#endif

#endif
