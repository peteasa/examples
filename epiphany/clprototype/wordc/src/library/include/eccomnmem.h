/************************************************************
 *
 * eccomnmem.h
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

#ifndef ECCOMNMEM
#define ECCOMNMEM

#include <ebase.h>
#include <emsgbase.h>

//
// ec_status_t:
//     ec_cmd_order in the high order bytes
//     0 or a random number in the low order bytes
//     lowest non-zero is run first
// ec_cmd_order<<16 | rand & 0xFFFF
typedef unsigned ec_status_t;

#define RNDMASK ((1 << 4 * sizeof(unsigned)) - 1)
#define ORDMASK (~RNDMASK)
#define ORD(v) \
    ((((unsigned)(v) & ORDMASK) >> 4 * sizeof(unsigned)) & RNDMASK)
#define RND(v) \
    ((unsigned)(v) & RNDMASK)

typedef enum {
    EC_IDLE = 0,
    EC_CANCEL = 1,
    EC_BUSY = 2,
    EC_CURR,
    EC_SEQ,           // numbers to 0xFFFD are available to sequence commands
    EC_SYNC = 0xFFFE, // wait for all work to complete
} ec_cmd_order;

typedef enum {
    RND_NO_SYNC = 0,  // set zero
    RND_SYNC,         // insert non-zero random number in status
} ec_cmd_rnd;

//
// ec_command_t
//
typedef enum {
    CMD_IDLE = 0,
    CMD_CLEAR_CONSUMER,
    CMD_ECHO,
    CMD_FWD,
    CMD_CANCEL,
    CMD_BLOCK,
    CMD_FMAP,
    CMD_MAP,
    CMD_RDC,
} ec_command;

typedef struct {
    ec_status_t cmdstatus;
    ec_command cmd;
    size_t datlength;
    size_t offset;
} ec_command_t;

extern ec_status_t cmd_gen_status(ec_cmd_order ord, unsigned rnd);

extern int ecl_insert_command(msg_channel_t *channel, size_t pdoffset, ec_status_t cmdstatus, ec_command cmd, size_t pddatlength);

extern void ec_get_cmdcmdid(msg_channel_t *channel, size_t cmdoffset, ec_command *cmd);
extern void ec_get_cmddatlength(msg_channel_t *channel, size_t cmdoffset, size_t *datlength);
extern void ec_get_cmdoffset(msg_channel_t *channel, size_t cmdoffset, size_t *datlength);

inline size_t ec_get_channel_base(msg_channel_t *channel)
{
    return channel->initialoffset;
}

inline size_t ec_get_channel_curr_pos(msg_channel_t *channel)
{
    return channel->offset;
}

#endif
