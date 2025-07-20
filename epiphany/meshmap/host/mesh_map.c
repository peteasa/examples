/*
  mesh_map.c

  A simple test to explore the Epiphany mesh

  Copyright (c) 2015-2025 Peter Saunderson <peteasa@gmail.com>

  based on hello_world.c:

  Copyright (C) 2012 Adapteva, Inc.
  Contributed by Yaniv Sapir <yaniv@adapteva.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program, see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>.
*/

// This is the HOST side of the Hello World example.
// The program initializes the Epiphany system,
// randomly draws an eCore and then loads and launches
// the device program on that eCore. It then reads the
// shared external memory buffer for the core's output
// message.

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unistring/stdbool.h>
#include <time.h>

#include <e-hal.h>
#include <e-loader.h>  // for e_load_group

// Not obvious what the magic is with this e_alloc
// One approach would be to send the core the address allocated in emem.
// Another approach would be to get the address from the core after the run.
// This approach boarders on black magic.
// See matmul for an explicit linker file that allocates space in the linker.
#define _BufSize   (128)
#define _BufOffset (0x01000000)
#define _SeqLen    (16)

#define TIMESTAMP
#ifdef TIMESTAMP
#define _TimeSize  (20)
static long offset = 0;
static long last_nsec = 0;
#endif

inline static int get_uptime(struct timespec *t);
inline static double get_duration(struct timespec *start, struct timespec *end);
inline static char* get_time(char * buf, struct timespec *start);

static int get_uptime(struct timespec *t)
{
    return clock_gettime(CLOCK_MONOTONIC, t);
}

static double get_duration(struct timespec *start, struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) + (double)(end->tv_nsec - start->tv_nsec) / 1000000000;
}

static char* get_time(char * buf, struct timespec *start)
{
    long tv_sec = start->tv_sec;
    long tv_nsec = start->tv_nsec;
    last_nsec = start->tv_nsec;

    tv_nsec += offset;
    tv_nsec = tv_nsec % 1000000000;
    if (1000000000 < start->tv_nsec + offset) tv_sec++;
    sprintf(buf, "[ %ld.%06ld]", tv_sec, tv_nsec/1000);

    return buf;
}

int mesh_map(char* elfFile)
{
    unsigned row, col, coreid, i;
    e_platform_t platform;
    e_epiphany_t dev;
    e_mem_t emem;
    char emsg[_BufSize];
    struct timespec stime, etime;
    char cstime[_TimeSize], cetime[_TimeSize];

#ifdef TIMESTAMP
    bool timestamp = true;
#else
    bool timestamp = false;
#endif

    // initialize system, read platform params from
    // default HDF. Then, reset the platform and
    // get the actual system parameters.
    if (timestamp) get_uptime(&stime);
    e_init(NULL);
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_init() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));
    if (timestamp) get_uptime(&stime);
    e_reset_system();
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_reset_system() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));
    e_get_platform_info(&platform);

    // Allocate a buffer in shared external memory
    // for message passing from eCore to host.
    // In epiphany code link to SECTION("shared_dram")
    // ie char shared_outbuf[128*16] SECTION("shared_dram");
    if (timestamp) get_uptime(&stime);
    e_alloc(&emem, _BufOffset, _BufSize*16);
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_alloc() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));

    // How to find the number of cores available?

    // How to communicate the size of the group to the core?

    // How to write to core memory?

    // Open a workgroup with all cores and reset the cores, in
    // case a previous process is running. Note that we used
    // core coordinates relative to the workgroup.
    if (timestamp) get_uptime(&stime);
    e_open(&dev, 0, 0, 4, 4);
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_open() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));
    if (timestamp) get_uptime(&stime);
    e_reset_group(&dev);
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_reset_group() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));

    row = 0;
    col = 0;
    for (i=0; i<_SeqLen; i++)
    {
        // Visit each core
        unsigned lastCol = col;
        col = col % platform.cols;

        if (col < lastCol)
        {
            row++;
        }
        row = row % platform.rows;

        coreid = (row + platform.row) * 64 + col + platform.col;

        // Load the device program onto the selected eCore
        e_return_stat_t result;
        if (timestamp) get_uptime(&stime);
        result = e_load(elfFile, &dev, row, col, E_FALSE);
        if (timestamp) get_uptime(&etime);
        if (result != E_OK)
        {
            fprintf(stderr, "main: 0x%03x Error in e_load %i\n", coreid, result);
        }

        if (timestamp) fprintf(stderr, "%s %s e_load() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));

        col++;
    }

    // Start the run
    if (timestamp) get_uptime(&stime);
    e_start_group(&dev);
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_start_group() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));

    // Wait for core program execution to finish, then
    // read message from shared buffer.
    usleep(10000);

    row = 0;
    col = 0;
    for (i=0; i<_SeqLen; i++)
    {
        // Visit each core
        unsigned lastCol = col;
        col = col % platform.cols;

        if (col < lastCol)
        {
            row++;
        }
        row = row % platform.rows;

        coreid = (row + platform.row) * 64 + col + platform.col;

        fprintf(stderr, "main: %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);

        // read message from shared buffer.
        if (timestamp) get_uptime(&stime);
        e_read(&emem, 0, 0, _BufSize * (row * 4 + col), emsg, _BufSize);
        if (timestamp) get_uptime(&etime);

        // Print the message
        fprintf(stderr, "\"%s\"\n", emsg);
        if (timestamp) fprintf(stderr, "%s %s e_read() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));

        // read the neighbour ids from the core
        int p,q;
        unsigned vrow, vcol;
        vrow = 0;
        vcol = 0;
        for (p=0; p<_SeqLen; p++)
        {
            // Visit each core
            unsigned lastvCol = vcol;
            vcol = vcol % platform.cols;

            if (vcol < lastvCol)
            {
                vrow++;
            }
            vrow = vrow % platform.rows;

            // Now calculate the local memory address
            off_t dest;
            dest = (off_t)(0x5000 + (vrow * 4 + vcol) * sizeof(e_coreid_t));
            // Now print the entry
            e_coreid_t vcoreid;
            if (p==0 && timestamp) get_uptime(&stime);
            e_read(&dev, row, col, dest, &vcoreid, sizeof(e_coreid_t));
            if (p==0 && timestamp) get_uptime(&etime);
            if (p==0 && timestamp) fprintf(stderr, "%s %s e_read() 0x%x took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), vcoreid, get_duration(&stime, &etime));
            fprintf(stderr, "0x%x ", vcoreid);

            vcol++;
        }
        fprintf(stderr, "\n");

        col++;
    }

    // and close the workgroup.
    if (timestamp) get_uptime(&stime);
    e_close(&dev);
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_close() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));

    // Release the allocated buffer and finalize the
    // e-platform connection.
    if (timestamp) get_uptime(&stime);
    e_free(&emem);
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_free() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));
    if (timestamp) get_uptime(&stime);
    e_finalize();
    if (timestamp) get_uptime(&etime);
    if (timestamp) fprintf(stderr, "%s %s e_finalize() took: %f\n", get_time(cstime, &stime), get_time(cetime, &etime), get_duration(&stime, &etime));

    return 0;
}

void print_usage(const char* argv0)
{
    printf("Usage: %s [--user_time|-u usec] [--dmesg_time|-d usec] filename\n",
           argv0);
}

void process_args(int argc, char *argv[], long *user_time, long *dmesg_time, char *elfFile)
{
    int c;

    static struct option long_options[] = {
        {"user_time",     optional_argument, 0, 'u'},
        {"dmesg_time",    optional_argument, 0, 'd'},
        {0, 0, 0, 0}
    };

    while (1) {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "u:d:", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 0:
            /* this should never be able to happen as all options have both
             * short and long flags. */
            printf ("unsupported option %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            exit(EXIT_FAILURE);
            break;

        case 'u':
            *user_time = (long) strtol(optarg, NULL, 10) * 1000;
            break;

        case 'd':
            *dmesg_time = (long) strtol(optarg, NULL, 10) * 1000;
            break;

        case '?':
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
            break;

        default:
            abort();
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    strcpy(elfFile, argv[optind]);
}

int main(int argc, char **argv)
{
    long user_time = 0;
    long dmesg_time = 0;
    char elfFile[4096];

    process_args(argc, argv, &user_time, &dmesg_time, elfFile);
    printf("using elf file: %s\n", elfFile);

#ifdef TIMESTAMP
    int correction = 2161;
    if (user_time && dmesg_time) offset = dmesg_time - user_time + correction*1000;
#endif

    int rtn = mesh_map(elfFile);

#ifdef TIMESTAMP
    // the time stamp of dmesg and user space will never be the same but its possible to get close
    // use raw last usec plus dmesg last usec value plus the old correction to estimate the new correction
    fprintf(stderr, "using offset of: %ld last usec: %ld correction: %ld\n", offset/1000, last_nsec/1000, correction);
#endif

    return rtn;
}
