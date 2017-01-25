/************************************************************
 *
 * eccomnmem.c
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

#define SETORD(o)                               \
    (((unsigned)(o) << 4 * sizeof(unsigned)) & ORDMASK)
#define SETENTRY(o,r)                          \
    ((((unsigned)(o) << 4 * sizeof(unsigned)) & ORDMASK) + ((unsigned)(r) & RNDMASK))

ec_status_t cmd_gen_status(ec_cmd_order ord, ec_cmd_rnd rnd)
{
    unsigned rand = 0;
    ec_status_t rtn = 0;

    if (RND_SYNC == rnd)
    {
        rand = lfsr113_Bits() & RNDMASK;
        if (0 == rand)
        {
            // random part must be non-zero to participate in sync
            rand = 1;
        }

        rtn = SETENTRY(ord, rand);
    } else {
        rtn = SETORD(ord);
    }

    return rtn;
}

int ecl_insert_command(msg_channel_t *channel, size_t pdoffset, ec_status_t cmdstatus, ec_command cmd, size_t pddatlength)
{
    int rtn = SUCCESS;
    ec_command_t pdcommand;
    pdcommand.cmd = cmd;

    pdcommand.cmdstatus = cmd_gen_status(EC_BUSY, RND_NO_SYNC);
    pdcommand.datlength = pddatlength;
    pdcommand.offset = pdoffset;

    if (!ecl_insert(channel, pdoffset, pdoffset, (char *)&pdcommand, sizeof(ec_command_t)))
    {
        rtn = FAIL;
    }

    if (rtn)
    {
        ec_status_t pdcmdstatus = cmdstatus;
        ec_write(channel, (void *)&pdcmdstatus, pdoffset, sizeof(ec_status_t));
    }

    return rtn;
}

void ec_get_cmdcmdid(msg_channel_t *channel, size_t cmdoffset, ec_command *cmd)
{
    ec_read(
        channel,
        (void *)cmd,
        cmdoffset + offsetof(ec_command_t, cmd),
        sizeof(ec_command));
}

void ec_get_cmddatlength(msg_channel_t *channel, size_t cmdoffset, size_t *datlength)
{
    ec_read(
        channel,
        (void *)datlength,
        cmdoffset + offsetof(ec_command_t, datlength),
        sizeof(size_t));
}

void ec_get_cmdoffset(msg_channel_t *channel, size_t cmdoffset, size_t *datlength)
{
    ec_read(
        channel,
        (void *)datlength,
        cmdoffset + offsetof(ec_command_t, datlength),
        sizeof(size_t));
}

