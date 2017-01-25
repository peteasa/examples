/************************************************************
 *
 * clcore.c
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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <e-loader.h>
#include <e-hal.h>
#include <libecomms.h>
#include "clcore.h"

e_platform_t platform;
e_epiphany_t edev;

msg_channel_t _msg_channel_core;
msg_channel_t _msg_channel_consumer;
msg_channel_t _msg_channel_consumer_data;
msg_channel_t _msg_channel_producer;
msg_channel_t _eclmsg_channel_producer_data;

msg_channel_t *msg_channel_core = &_msg_channel_core;
msg_channel_t *msg_channel_consumer = &_msg_channel_consumer;
msg_channel_t *msg_channel_consumer_data = &_msg_channel_consumer_data;
msg_channel_t *msg_channel_producer = &_msg_channel_producer;
msg_channel_t *eclmsg_channel_producer_data = &_eclmsg_channel_producer_data;

// Debug
#define MAXRANK (16)
#define BUFSIZE (256)
#define CONSUMERRANK (11)
unsigned rowc, colc;

char emsg[BUFSIZE];
char *buf = emsg;
size_t bufs = BUFSIZE;

int lines = 0;

// Command and Data buffer size
#define CMDBUFSIZE (128)

// Operations
cl_mr_ops_t *cl_ops;

// Forward declarations
void readresponse();

int freechanneldata(msg_channel_t *channel, size_t datasize);
int initsystem();
int initchannels();
int loadcores(char *executable);
int initconsumers();
unsigned updaterowandcol(unsigned *row, unsigned *col);
void debugprint(unsigned coreid, unsigned row, unsigned col);

int clcoreinit(cl_mr_ops_t *ops, e_epiphany_t **dev, char *executable)
{
    int rtn = SUCCESS;
    *dev = NULL;

    if (!ops)
    {
        rtn = FAIL;
    }

    cl_ops = ops;

    if (rtn && !initsystem())
    {
        rtn = FAIL;
        fprintf(stderr, "clcoreinit: Failed to allocate shared memory. Error is %s\n",
                strerror(errno));
    }

    if (rtn && cl_ops->mr_init && !cl_ops->mr_init())
    {
        rtn = FAIL;
        fprintf(stderr, "cl_ops: Failed to allocate shared memory. Error is %s\n",
                strerror(errno));
    }

    if (rtn)
    {
        // Open a workgroup with all cores and reset the cores, in
        // case a previous process is running. Note that we used
        // core coordinates relative to the workgroup.
        e_open(&edev, 0, 0, platform.rows, platform.cols);
        e_reset_group(&edev);
        *dev = &edev;
    }

    if (rtn && !loadcores(executable))
    {
        rtn = FAIL;
        fprintf(stderr, "clcoreinit: Failed to load cores. Error is %s\n",
                strerror(errno));
    }

    if (rtn && !initconsumers())
    {
        rtn = FAIL;
        fprintf(stderr, "clcoreinit: Failed to init consumers %s\n",
                strerror(errno));
    }

    return rtn;
}

void clcorefinalize(void)
{
	// and close the workgroup.
	e_close(&edev);

	// Release the allocated buffer and finalize the
	// e-platform connection.
	msg_channel_free(msg_channel_producer);
    ecl_free_mem_channel(eclmsg_channel_producer_data);
    msg_channel_free(msg_channel_consumer);
    msg_channel_free(msg_channel_consumer_data);
    msg_channel_free(msg_channel_core);

    if (cl_ops->mr_finalize)
    {
        cl_ops->mr_finalize();
    }

	e_finalize();
}

int clcorerun(void)
{
    unsigned row, col, coreid;
    unsigned r,c, j;

    row = CONSUMERRANK / platform.cols; //0;
    col = CONSUMERRANK % platform.cols; //0;
    coreid = updaterowandcol(&row, &col);

    //for (i=0; i<MAXRANK; i++)
    while (SUCCESS)
	{
        if (cl_ops->mr_evstrt)
        {
            if (!cl_ops->mr_evstrt())
            {
                break;
            }
        }

        // Allow time for the processing to complete
        usleep(10000);

        /*for (j=0; j<MAXRANK; j++)
        {
            r = j/platform.cols;
            c = j%platform.cols;
            debugprint(j, r, c);
            }*/

        // read responses from consumer
        readresponse();

        if (cl_ops->mr_evend)
        {
            cl_ops->mr_evend();
        }
    }

    // flush out any final messages
    clcoresendcmd(cmd_gen_status(EC_SEQ, RND_NO_SYNC), CMD_ECHO, 0, 0);
    usleep(10000);
    readresponse();

    for (j=0; j<MAXRANK; j++)
    {
        r = j/platform.cols;
        c = j%platform.cols;
        debugprint(j, r, c);
    }

    return SUCCESS;
}

// Insert data into eclmsg_channel_producer_data after the command
int clcorewrite(cl_ref *pdoffset, char *mrdata, size_t mrsize)
{
    size_t msgoffset = (size_t) *pdoffset;
    size_t mrdataoffset = msgoffset + sizeof(ec_command_t);

    //printf("clcorewrite: 0x%lx 0x%lx\n", msgsize, msgoffset);
    //printf("clcorewrite: 0x%lx 0x%lx\n", mrsize, mrdataoffset);

    return ecl_insert(eclmsg_channel_producer_data, msgoffset, mrdataoffset, mrdata, mrsize);
}

int clcorepackhst(void *mrref, size_t mrsize, size_t *pdoffset)
{
    int rtn = SUCCESS;

    size_t cmdsize = sizeof(ec_command_t);
    if ( !msg_space(msg_channel_producer, cmdsize) )
    {
        //printf("clcorepackhst: reset producer channel\n");
        msg_reset(msg_channel_producer);
    }

    if ( msg_is_command_ready(msg_channel_producer) )
    {
        printf("clcorepackhst: previous command not acknowledged\n");
        rtn = FAIL;
    }

    size_t pdlen = cmdsize + mrsize;
    if (rtn && !freechanneldata(eclmsg_channel_producer_data, pdlen))
    {
        printf("clcorepackhst: no channel data space\n");
        rtn = FAIL;
    }

    if (rtn && !ecl_alloc(eclmsg_channel_producer_data, pdlen, pdoffset))
    {
        printf("clcorepackhst: data allocation failed\n");
        rtn = FAIL;
    }

    if (rtn && cl_ops->mr_packhst && !cl_ops->mr_packhst((cl_ref *)pdoffset, mrref, mrsize))
    {
        printf("clcorepackhst: packing failed\n");
        rtn = FAIL;
        ecl_free(eclmsg_channel_producer_data, *pdoffset);
    }

    return rtn;
}

// Send a command
void clcoresendcmd(ec_status_t cmdstatus, ec_command cmd, size_t pddatlength, size_t pdoffset)
{
    printf("clcoresendcmd(%d): poff 0x%lx pddatlength 0x%lx pdoffset 0x%lx\n", lines++, msg_channel_producer->offset, pddatlength, pdoffset);

    if (pddatlength)
    {
        // Insert command into eclmsg_channel_producer_data
        if (!ecl_insert_command(eclmsg_channel_producer_data, pdoffset, cmdstatus, cmd, pddatlength))
        {
            fprintf(
                stderr,
                "clcoresendcmd: failed to insert command at pdoffset 0x%lx\n",
                pdoffset);
        }

        //printf("clcoresendcmd: pdoffset 0x%lx\n", pdoffset);
    }

    if (!msg_send_command(msg_channel_producer, cmdstatus, cmd, pddatlength, pdoffset))
    {
        fprintf(stderr, "main: failed to send command %p\n", msg_channel_producer->offset);
    }

    // Send E_USER_INT
    if (E_OK != e_signal(&edev, rowc, colc))
    {
        fprintf(stderr, "clcoresendcmd: failed to send interrupt to (%2d,%2d)\n", rowc, colc);
    }
}

void readresponse()
{
    int printnl = FAIL;

    if (msg_is_command_ready(msg_channel_consumer))
    {
        printnl = SUCCESS;
        printf("consumer sends: ");
    }

    while (msg_is_command_ready(msg_channel_consumer))
    {
        ec_command_t cmd;
        msg_get_command(msg_channel_consumer, &cmd);
        msg_mark_command_done(msg_channel_consumer);
        msg_goto_next_cmd(msg_channel_consumer);
        printf("c%x l0x%x o0x%x ", cmd.cmd, cmd.datlength, cmd.offset);

        if (cmd.datlength)
        {
            // Unpack the response data from the consumer
            size_t cdoffset_data = cmd.offset + sizeof(ec_command_t);
            ec_read(
                msg_channel_consumer_data,
                (void *)buf,
                cdoffset_data,
                cmd.datlength);
            size_t *memsize = (size_t *)buf;
            size_t *memoffset = (size_t *)((size_t)memsize + sizeof(size_t));
            printf("size 0x%x, offset 0x%x\n", *memsize, *memoffset);

            // TODO update to mark consumer_data done
            printnl = FAIL; // no need to terminate with new line
        } else {
            printnl = SUCCESS;
        }
    }

    if (printnl)
    {
        printf("\n");
    }
}

int freechanneldata(msg_channel_t *ecchannel, size_t datasize)
{
    int rtn = SUCCESS;

    // find offset for chunk large enough for data
    size_t offset;
    rtn = ecl_find_space(ecchannel, datasize, &offset);

    // check if chunk is free
    size_t choffset = offset;
    while (rtn)
    {
        if (rtn && ec_busy(ecchannel, choffset))
        {
            // check if command is busy
            ec_command_t cmd;
            ec_read(ecchannel, (void *)&cmd, choffset, sizeof(ec_command_t));
            if (msg_command_inuse(&cmd))
            {
                rtn = FAIL;
                break;
            } else {
                ecl_free(ecchannel, choffset);
                rtn = ecl_find_space(ecchannel, datasize, &offset);
                choffset = offset;
            }
        }

        // check if space
        size_t available = ecl_size(ecchannel, offset);
        if (0 == available)
        {
            rtn = FAIL;
            break;
        }

        if (available >= datasize)
        {
            break;
        }

        // goto next chunk
        if (!ecl_next_data(ecchannel, &choffset))
        {
            rtn = FAIL;
            break;
        }
    }

    return rtn;
}

int initsystem(void)
{
    int rtn = SUCCESS;

    // initialize system, read platform params from
    // default HDF. Then, reset the platform and
    // get the actual system parameters.
    e_init(NULL);
    e_reset_system();
    e_get_platform_info(&platform);

    rowc = CONSUMERRANK / platform.cols;
    colc = CONSUMERRANK % platform.cols;

    if (rtn && !initchannels())
    {
        rtn = FAIL;
    }

    return rtn;
}

int initchannels(void)
{
    int rtn = SUCCESS;
    const unsigned ShmPerCoreSize = BUFSIZE*16;
    const char ShmPerCoreName[] = "ecore";
    const unsigned ShmConsumerSize = CMDBUFSIZE;
    const char ShmConsumerName[] = "econsumer";
    const unsigned ShmConsumerDataSize = BUFSIZE;
    const char ShmConsumerDataName[] = "econsumerdata";
    const unsigned ShmProducerSize = CMDBUFSIZE;
    const char ShmProducerName[] = "eproducer";
    const unsigned ShmProducerDataSize = CMDBUFSIZE;
    const char ShmProducerDataName[] = "eproducerdata";

    // Allocate a buffer in shared external memory
    // for messages from host to eCore
    if (rtn && !msg_channel_shared_alloc(msg_channel_core, ShmPerCoreName, ShmPerCoreSize, MAXRANK))
    {
        fprintf(stderr, "main: Failed to allocate per core shared memory. Error is %s\n",
                strerror(errno));
        rtn = FAIL;
    }

    // Allocate a buffer in shared external memory
    // for messages from host to eCore
    if (rtn && !msg_channel_alloc(msg_channel_producer, ShmProducerName, ShmProducerSize))
    {
        fprintf(stderr, "main: Failed to allocate producer shared memory. Error is %s\n",
                strerror(errno));
        rtn = FAIL;
    }

    // for command data from host to eCore
    if (rtn && !ecl_alloc_mem_channel(eclmsg_channel_producer_data, ShmProducerDataName, ShmProducerDataSize))
    {
        fprintf(stderr, "main: Failed to allocate producer data shared memory. Error is %s\n",
                strerror(errno));
        rtn = FAIL;
    }

    // for messages from eCore to host
    if (rtn && !msg_channel_alloc(msg_channel_consumer, ShmConsumerName, ShmConsumerSize))
    {
        fprintf(stderr, "main: Failed to allocate consumer shared memory. Error is %s\n",
                strerror(errno));
        rtn = FAIL;
    }

    // for command data from eCore to host
    // TODO consider changing this to ecl_alloc_mem_channel
    if (rtn && !msg_channel_alloc(msg_channel_consumer_data, ShmConsumerDataName, ShmConsumerDataSize))
    {
        fprintf(stderr, "main: Failed to allocate consumer data shared memory. Error is %s\n",
                strerror(errno));
        rtn = FAIL;
    }

    return rtn;
}

int loadcores(char *executable)
{
    int rtn = SUCCESS;
    unsigned row, col, coreid, i;

    row = 0;
    col = 0;
    for (i=0; i<platform.rows * platform.cols; i++)
    {
        // Visit each core
        coreid = updaterowandcol(&row, &col);

        // Load the device program onto the selected eCore
        // TODO get the name of the file from the user side
        e_return_stat_t result;
        result = e_load(executable, &edev, row, col, E_FALSE);
        if (result != E_OK)
        {
            fprintf(stderr, "main: 0x%03x Error in e_load %i\n", coreid, result);
        }

        col++;
    }

    // Start the run
    e_start_group(&edev);

    // Allow time for the processing to complete
    usleep(100000);

    return rtn;
}

int initconsumers()
{
    int rtn = SUCCESS;
    unsigned row, col, coreid, i;

    row = 0;
    col = 0;
    for (i=0; i<platform.rows * platform.cols; i++)
    {
        // Visit each core
        coreid = updaterowandcol(&row, &col);

        // reset the channel so that each eCore gets the message
        // from the first slot
        msg_reset(msg_channel_producer);
        if ((row == rowc) && (col == colc))
        {
            if (!msg_send_command(msg_channel_producer, cmd_gen_status(EC_SEQ, RND_NO_SYNC), CMD_ECHO, 0, 0))
            {
                fprintf(stderr, "initconsumers (%2d,%2d): failed to send command %p\n", row,col, msg_channel_producer->offset);
            }
        } else {
            if (!msg_send_command(msg_channel_producer, cmd_gen_status(EC_SEQ, RND_NO_SYNC), CMD_CLEAR_CONSUMER, 0, 0))
            {
                fprintf(stderr, "initconsumers (%2d,%2d): failed to send command %p\n", row,col, msg_channel_producer->offset);
            }
        }

        // Send E_USER_INT
        if (E_OK != e_signal(&edev, row, col))
        {
            fprintf(stderr, "initconsumers: failed to send interrupt to (%2d,%2d)\n", row, col);
        }

        // Allow time for the processing to complete
        usleep(10000);

        //debugprint(coreid, row, col);

        if ((row == rowc) && (col == colc))
        {
            // read response from consumer
            readresponse();
        }

        col++;
    }

    return rtn;
}

unsigned updaterowandcol(unsigned *row, unsigned *col)
{
    unsigned coreid;

    // Visit each core
    unsigned lastCol = *col;
    *col = *col % platform.cols;

    if (*col < lastCol)
    {
        (*row)++;
    }

    *row = *row % platform.rows;

    coreid = (*row + platform.row) * 64 + *col + platform.col;

    return coreid;
}

void debugprint(unsigned coreid, unsigned row, unsigned col)
{
    printf("main: Message from eCore 0x%03x (%2d,%2d): ", coreid, row, col);

    // read message from shared buffer.
    msg_goto_sgmnt(msg_channel_core, (row * 4 + col));
    uc_read(msg_channel_core, buf, bufs);

    // Print the message
    printf("\"%s\"\n", buf);
}
