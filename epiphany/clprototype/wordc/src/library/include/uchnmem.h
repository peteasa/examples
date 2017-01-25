/************************************************************
 *
 * uchnmem.h
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

#ifndef UCHNMEM
#define UCHNMEM

#include <emsgbase.h>

// channel allocation helpers
#ifdef __epiphany__
extern int ec_init_mem_uchannel(msg_channel_t *channel, const e_group_config_t *e_group_config, const size_t offset, size_t size);
#else
extern int ec_init_mem_uchannel(msg_channel_t *channel, const e_epiphany_t *dev, const size_t offset, size_t size);
#endif

#ifdef __epiphany__
extern int ecl_alloc_local_mem_uchannel(msg_channel_t *channel, const size_t offset, size_t size);
#endif

#endif
