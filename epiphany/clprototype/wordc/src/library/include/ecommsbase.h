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

#ifndef ECOMMSBASE
#define ECOMMSBASE

#include <ebase.h>
#include <emsgbase.h>

// mapping e_lib and e-hal functions to a single api
extern size_t uc_read(const msg_channel_t *channel, void *dst, size_t n);
extern size_t uc_write(const msg_channel_t *channel, const void *src, size_t n);

// channel allocation helpers
#ifndef __epiphany__
extern int msg_channel_alloc(msg_channel_t *channel, const char* name, const size_t size);
extern int msg_channel_shared_alloc(msg_channel_t *channel, const char* name, const size_t size, const unsigned grpsize);
extern void msg_channel_free(msg_channel_t *channel);
#endif
extern int msg_channel_init(msg_channel_t *channel, const char* name);
extern int msg_channel_shared_init(msg_channel_t *channel, const char* name, const unsigned grpsize, const unsigned rank);

// channel administration
extern int msg_reset(msg_channel_t *channel);

// generic data adding helpers
extern int msg_space(msg_channel_t *channel, const size_t size);
extern int msg_append(msg_channel_t *channel, const void *src, const size_t n);
extern int msg_append_str(msg_channel_t *channel, const void *src);

// channel sgmnt specific helpers
extern void msg_get_sgmnt(msg_channel_t *channel, void** item);
extern void msg_goto_next_sgmnt(msg_channel_t *channel);
extern void msg_goto_sgmnt(msg_channel_t *channel, unsigned sgmnt);

#endif
