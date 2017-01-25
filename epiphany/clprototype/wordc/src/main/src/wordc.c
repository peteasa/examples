/************************************************************
 *
 * wordc.c
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
#include <unistd.h>     // for getopt, optarg
#include <libecomms.h>
#include "clcomms.h"
#include "clcore.h"

cl_mr_ops_t wc_cl;

msg_channel_t eclmsg_channel_mem;

typedef struct {
    size_t size;
    size_t offset;
} mem_sentence_t;
mem_sentence_t sentences[10];

// Socket to receive data from client
void *zmqclientsocket;

// Foward declarations
void printusage(char *argv[]);
int wc_init(void);
void wc_finalize(void);
int wc_evstrt(void);
void wc_evend(void);
int wc_linepack(cl_ref *chan, void *mrref, size_t mrsize);

int main(int argc, char *argv[])
{
    int opt;
    char *endpoint = NULL;

    if (argc < 2)
    {
        printusage(argv);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "?e:")) != -1)
    {
        switch (opt)
        {
        case 'e': endpoint = optarg;
            break;
        case '?':
            if (optopt == 'c')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else
                printusage(argv);
            exit(EXIT_FAILURE);
        default:
            printusage(argv);
            exit(EXIT_FAILURE);
        }
    }

    if ( (NULL == endpoint) )
    {
        printusage(argv);
        exit(EXIT_FAILURE);
    }

    printf("endpoint %s Type %s\n", endpoint);

    if (!clc_init_src(endpoint))
    {
        exit(EXIT_FAILURE);
    }

    e_epiphany_t *dev;

    wc_cl.mr_init = wc_init;
    wc_cl.mr_finalize = wc_finalize;
    wc_cl.mr_evstrt = wc_evstrt;
    wc_cl.mr_evend = wc_evend;
    wc_cl.mr_packhst = wc_linepack;

    if (!clcoreinit(&wc_cl, &dev, "e_wordc"))
    {
        fprintf(stderr, "main: failed to init system");
        clc_destroy_src();
        return EXIT_FAILURE;
    }

    if (!clcorerun())
    {
        printf("main: clcorerun ended\n");
    }

    clcorefinalize();
    clc_destroy_src();

    return EXIT_SUCCESS;
}

void printusage(char *argv[])
{
    fprintf(stderr, "Usage: %s [-e <endpoint> -t <ZMQ type>]\n", argv[0]);
}

int wc_init(void)
{
    int rtn = SUCCESS;

    const char ShmMemName[] = "ecmemts";
    if (rtn && !ecl_alloc_mem_channel(&eclmsg_channel_mem, ShmMemName, 0x1000))
    {
        fprintf(stderr, "main: failed to init mem test");
        rtn = FAIL;
    }

    return rtn;
}

void wc_finalize(void)
{
    printf("wc_finalize\n");
    ecl_free_mem_channel(&eclmsg_channel_mem);
}

int wc_evstrt(void)
{
    int rtn = SUCCESS;

    char Line[MAXLINELENGTH];
    size_t memsize = clc_get_line(Line, MAXLINELENGTH);

    // TODO handle end of lines properly
    if (!memsize)
    {
        printf("wc_evstrt: End of input\n");
        rtn = FAIL;
    }

    size_t memoffset;
    size_t memaligned = em_align(memsize);

    if (rtn && !ecl_free_mem_space(&eclmsg_channel_mem, memaligned))
    {
        fprintf(stderr, "wc_evstrt: no mem space\n");
        rtn = FAIL;
    }

    if (rtn && !ecl_alloc(&eclmsg_channel_mem, memaligned, &memoffset))
    {
        fprintf(stderr, "wc_evstrt: mem allocation failed\n");
        rtn = FAIL;
    }

    if (rtn)
    {
        ec_write(&eclmsg_channel_mem, Line, memoffset, memaligned);

        // now the list of offsets are packed into the data part of a message
        // in the simple case only one offset
        sentences[0].size = memsize;
        sentences[0].offset = memoffset;

        // core packs the data with the assistance of the user provided pack function
        int entries = 1;
        size_t pdoffset;
        size_t pdlength = entries * sizeof(mem_sentence_t);
        if (clcorepackhst((void *)&entries, pdlength, &pdoffset))
        {
            // once packed the packed message is sent
            ec_command cmd = CMD_FMAP;
            //printf("wc_evstrt: ml0x%lx mo0x%lx l0x%lx pdo0x%lx\n", memsize, memoffset, pdlength, pdoffset);
            clcoresendcmd(cmd_gen_status(EC_SEQ+1, RND_NO_SYNC), cmd, pdlength, pdoffset);
        } else {
            fprintf(stderr, "wc_evstrt: clcorepackhst failed\n");
        }
    }

    return rtn;
}

void wc_evend(void)
{
}

int wc_linepack(cl_ref *chan, void *mrref, size_t mrsize)
{
    int rtn = SUCCESS;
    int *topack = (int *)mrref;
    size_t size = *topack * sizeof(mem_sentence_t);

    if (size <= mrsize)
    {
        //printf("wc_linepack: 0x%lx 0x%lx\n", size, (size_t)(*chan));
        //printf("wc_linepack: 0x%lx 0x%lx\n", sentences[0].size, sentences[0].offset);
        rtn = clcorewrite(chan, (char*)sentences, size);
    } else {
        rtn = FAIL;
    }

    return rtn;
}
