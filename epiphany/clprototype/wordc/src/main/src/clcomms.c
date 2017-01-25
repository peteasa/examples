/************************************************************
 *
 * clcomms.c
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

#include <stdio.h>
#include <string.h>
#include <zmq.h>
#include "clcomms.h"

static clc_channel_t client_chan;

int clc_init_src(char *endpoint)
{
    int rtn = CLC_FAIL;

    client_chan.zmqcontext = zmq_ctx_new();
    client_chan.zmqsocket = zmq_socket(client_chan.zmqcontext, ZMQ_PAIR);
    size_t endpoint_size = strlen(endpoint);
    if (endpoint_size <= ENDPOINTLENGTH)
    {
        rtn = CLC_SUCCESS;
        memcpy(client_chan.endpoint, endpoint, endpoint_size+1); // copy the terminating 0
        int rc = zmq_connect(client_chan.zmqsocket, endpoint);
        if (rc)
        {
            rtn = CLC_FAIL;
            fprintf(stderr, "clc_init_src: connect failed. Error is %s\n",
                    strerror(errno));
        }
    }

    return rtn;
}

void clc_destroy_src()
{
    // TODO if client terminates with THE END cant disconnect because server
    //      side has gone
    //      if server side terminates with THE END cant send "THE END"
    //      Need to store state in clien_chan to handle termination
    int txed = zmq_send(client_chan.zmqsocket, "THE END", strlen("THE END"), 0);
    /*int rc = zmq_disconnect(client_chan.zmqsocket, client_chan.endpoint);
    if (rc)
    {
        fprintf(stderr, "clc_destroy_src: disconnect failed. Error is %s\n",
                strerror(errno));
                }

    rc = zmq_ctx_destroy(client_chan.zmqcontext);
    if (rc)
    {
        fprintf(stderr, "clc_destroy_src: ctx destroy failed. Error is %s\n",
                strerror(errno));
                }*/
}

size_t clc_get_line(char *Line, size_t maxsize)
{
    int txed = zmq_send(client_chan.zmqsocket, "next", strlen("next"), 0);

    int rxed = zmq_recv(client_chan.zmqsocket, Line, MAXLINELENGTH, 0);
    if (-1 != rxed)
    {
        if (rxed < MAXLINELENGTH)
        {
            Line[rxed] = 0;
        } else {
            Line[rxed-1] = 0;
        }

        rxed += 1; // for end of string null
    } else {
        rxed = 0;
        fprintf(stderr, "clc_get_line: zmq_recv failed. Error is %s\n",
                strerror(errno));
    }

    // TODO improve handling of end
    if (rxed == strlen("THE END")+1)
    {
        if (!strcmp(Line, "THE END"))
        {
            rxed = 0;
        }
    }

    return rxed;
}
