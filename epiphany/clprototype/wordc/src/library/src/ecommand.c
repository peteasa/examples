/************************************************************
 *
 * ecommand.c
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
#include <eccomnmem.h>
#include <ecommand.h>

int msg_command_inuse(ec_command_t *cmd)
{
    return EC_IDLE != ORD(cmd->cmdstatus);
}

int msg_command_busy(ec_command_t *cmd)
{
    return (EC_BUSY == ORD(cmd->cmdstatus) );
}

int msg_send_command(msg_channel_t *channel, ec_status_t cmdstatus, ec_command cmd, size_t datlength, size_t offset)
{
    size_t size = sizeof(ec_command_t);

    if ( !msg_space(channel, size) )
    {
        msg_reset(channel);
    }

    if ( msg_is_command_ready(channel) )
    {
        return FAIL;
    }

    return msg_append_command(channel, cmdstatus, cmd, datlength, offset);
}

int msg_append_command(msg_channel_t *channel, ec_status_t cmdstatus, ec_command cmd, size_t datlength, size_t offset)
{
    int rtn = SUCCESS;
    ec_command_t command;
    command.cmd = cmd;

    command.cmdstatus = cmd_gen_status(EC_BUSY, RND_NO_SYNC);
    command.datlength = datlength;
    command.offset = offset;

    size_t choffset = channel->offset;

    if (!msg_append(channel, &command, sizeof(ec_command_t)))
    {
        rtn = FAIL;
    }

    if (rtn)
    {
        ec_status_t status = cmdstatus;
        ec_write(channel, (void *)&status, choffset, sizeof(ec_status_t));
    }

    return rtn;
}

int msg_is_command_ready(msg_channel_t *channel)
{
    ec_status_t cmdstatus;
    size_t bytesread;
    bytesread = uc_read(channel,
                        (void*)&cmdstatus,
                        sizeof(ec_status_t));

    unsigned status = ORD(cmdstatus);
    return (EC_CANCEL == status) || (EC_BUSY < status);
}

void msg_get_command(msg_channel_t *channel, ec_command_t *cmd)
{
    uc_read(channel,
           (void*)cmd,
           sizeof(ec_command_t));
}

void msg_mark_command_done(msg_channel_t *channel)
{
    ec_status_t cmdstatus;

    cmdstatus = EC_IDLE;
    uc_write(channel,
             &cmdstatus,
             sizeof(ec_status_t));
}

void msg_goto_next_cmd(msg_channel_t *channel)
{
    channel->offset += sizeof(ec_command_t);
    if ( !msg_space(channel, sizeof(ec_command_t)) )
    {
        // Not enough space for the new command
        msg_reset(channel);
    }
}

int msg_copy_data(msg_channel_t *channel, void* dst, ec_command_t *cmd, size_t maxsize)
{
    int rtn = SUCCESS;

    size_t size = cmd->datlength + sizeof(ec_command_t);
    if (size <= maxsize)
    {
        ec_read(channel, dst, cmd->offset, cmd->datlength + sizeof(ec_command_t));
    } else {
        ec_read(channel, dst, cmd->offset, maxsize);
        rtn = FAIL;
    }

    return rtn;
}

void msg_mark_datacmd_done(msg_channel_t *channel, ec_command_t *cmd)
{
    ec_status_t cmdstatus;

    cmdstatus = EC_IDLE;
    ec_write(channel,
             &cmdstatus,
             cmd->offset,
             sizeof(ec_status_t));
}
