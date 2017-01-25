/************************************************************
 *
 * ecommand.h
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

#ifndef ECOMMAND
#define ECOMMAND

#include <ebase.h>
#include <emsgbase.h>
#include <eccomnmem.h>

// generic command helpers
extern int msg_command_inuse(ec_command_t *cmd);
extern int msg_command_busy(ec_command_t *cmd);

// channel command specific helpers
extern int msg_send_command(msg_channel_t *channel, ec_status_t cmdstatus, ec_command cmd, size_t datlength, size_t offset);

extern int msg_append_command(msg_channel_t *channel, ec_status_t cmdstatus, ec_command cmd, size_t datlength, size_t offset);
extern int msg_is_command_ready(msg_channel_t *channel);
extern void msg_get_command(msg_channel_t *channel, ec_command_t *cmd);
extern void msg_mark_command_done(msg_channel_t *channel);
extern void msg_goto_next_cmd(msg_channel_t *channel);

inline void msg_get_cmdcmdid(msg_channel_t *channel, ec_command *cmd)
{
    ec_get_cmdcmdid(channel, channel->offset, cmd);
}

inline void msg_get_cmddatlength(msg_channel_t *channel, size_t *datlength)
{
    ec_get_cmddatlength(channel, channel->offset, datlength);
}

// copy and acknowledge command from data channel
extern int msg_copy_data(msg_channel_t *channel, void* dst, ec_command_t *cmd, size_t maxsize);
extern void msg_mark_datacmd_done(msg_channel_t *channel, ec_command_t *cmd);

#endif
