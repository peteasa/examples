/*
  mutex.c

  A simple test to explore the Epiphany mutex

  Copyright (c) 2015-2025 Peter Saunderson <peteasa@gmail.com>

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

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <e-hal.h>
#include <e-loader.h>  // for e_load_group

#define _BufSize   (128)
#define _BufOffset (0x01000000)
#define _SeqLen    (16)

uint32_t get_register(e_epiphany_t *dev, uint32_t row, uint32_t col, uint32_t reg)
{
    uint32_t ret;
    ssize_t err = e_read(dev, row, col, reg, &ret, sizeof(uint32_t));
    return ret;
}

void output_sets(const int n_iter, int n_cores, e_epiphany_t *dev, uint32_t *rows, uint32_t *cols,
                 int n_regs, char **titles, uint32_t *regs,
                 uint32_t (*get_reg)(e_epiphany_t *dev, uint32_t row, uint32_t col, uint32_t reg))
{
    if (n_iter == 0) {
        // prepare the table
        printf("----");
        for (int t = 1; t < n_regs; t++) {
            printf("----------");
        }

        printf("\n");
        printf("%s  ", titles[0]);
        for (int t = 1; t < n_regs; t++) {
            printf("%s  ", titles[t]);
        }

        printf("\n");
    } else printf("\r\033[%iA", n_cores + 1);

    for (int c = 0; c < n_cores; c++) {
            printf("%04x  ", get_register(dev, rows[c], cols[c], regs[0]));
            for (int r = 1; r < n_regs; r++) {
                // read and print register values
                printf("%08x  ", get_reg(dev, rows[c], cols[c], regs[r]));
            }

        printf("\n");
    }

    printf("\n");
}

int mutex(char *elfFile)
{
    unsigned row, col, coreid, i;
    e_platform_t platform;
    e_epiphany_t dev;
    e_mem_t emem;

    uint32_t regs[] = { E_REG_COREID, E_REG_CONFIG, E_REG_STATUS, E_REG_PC, E_REG_CTIMER0, E_REG_CTIMER1,
                        E_REG_DMA0STATUS, E_REG_DMA1STATUS, E_REG_IRET, E_REG_IMASK, E_REG_ILAT, E_REG_IPEND };
    char *titles[] = {"core", " config ", " status ", "   pc   ", "ctimer0 ", "ctimer1 ",
                      "dma0sts ", "dma1sts ", "  iret  ", " imask ", "   ilat  ", " ipend "};

    // prepare the table
    int n_regs = sizeof(regs) / sizeof(uint32_t);
    int n_titles = sizeof(titles) / sizeof(char *);
    
    // initialize system, read platform params from
    // default HDF. Then, reset the platform and
    // get the actual system parameters.
    e_init(NULL);
    e_reset_system();
    e_get_platform_info(&platform);

    // Allocate a buffer in shared external memory
    // for message passing from eCore to host.
    // In epiphany code link to SECTION("shared_dram")
    // ie char shared_outbuf[128*16] SECTION("shared_dram");
    e_alloc(&emem, _BufOffset, _BufSize*16);

    // How to find the number of cores available?

    // How to communicate the size of the group to the core?

    // How to write to core memory?

    // Open a workgroup with all cores and reset the cores, in
    // case a previous process is running. Note that we used
    // core coordinates relative to the workgroup.
    e_open(&dev, 0, 0, 4, 4);
    e_reset_group(&dev);

    uint32_t n_cores = _SeqLen;
    uint32_t rows[_SeqLen];
    uint32_t cols[_SeqLen];
    row = 0;
    col = 0;
    for (i=0; i<n_cores; i++)
    {
        uint32_t lastCol = col;
        col = col % platform.cols;

        if (col < lastCol)
        {
            row++;
        }

        row = row % platform.rows;

        rows[i] = row;
        cols[i] = col;

        col++;
    }

    for (i=0; i<n_cores; i++)
    {
        coreid = (rows[i] + platform.row) * 64 + cols[i] + platform.col;

        // Load the device program onto the selected eCore
        e_return_stat_t result;
        result = e_load(elfFile, &dev, rows[i], cols[i], E_FALSE);
        if (result != E_OK)
        {
            fprintf(stderr, "main: 0x%03x Error in e_load %i\n", coreid, result);
        }

        // output_sets(i, n_cores, &dev, rows, cols, n_regs, titles, regs, &get_register);
    }

    // Start the run
    e_start_group(&dev);

    if (1) {
        // Wait for core program execution to finish
        // output the register values
        for (int i = 0; i < 3; i++) {
            output_sets(i, n_cores, &dev, rows, cols, n_regs, titles, regs, &get_register);
            usleep(500);
        }
    }

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

        if (E_OK != e_signal(&dev, row, col))
        {
            fprintf(stderr, "main: failed to send interrupt to (%2d,%2d)", row, col);
        }

        if (0) {
            // Allow time for the interrupt
            for (int i = 0; i < 10; i++) {
                output_sets(i, n_cores, &dev, rows, cols, n_regs, titles, regs, &get_register);
                usleep(100);
            }
        }

        coreid = (row + platform.row) * 64 + col + platform.col;

        fprintf(stderr, "main: %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);

        for (int m = 0; m < 5; m++)
        {
            // read message from shared buffer.
            uint32_t val;
            e_read(&emem, 0, 0, (row * platform.cols + col) * _BufSize + m * 4, &val, sizeof(uint32_t));
            if (0 < m) fprintf(stderr, "0x%x ", val);
            else fprintf(stderr, "core: 0x%x neigbours: ", val);
        }

        fprintf(stderr, "\n");

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

            // Now calculate the address
            off_t dest;
            dest = (off_t)(0x4000 + (vrow * 4 + vcol) * sizeof(e_coreid_t));
            // Now print the entry
            e_coreid_t vcoreid;
            e_read(&dev, row, col, dest, &vcoreid, sizeof(e_coreid_t));
            fprintf(stderr, "0x%x ", vcoreid);

            vcol++;
        }
        fprintf(stderr, "\n");

        for (int m = 5; m < 7; m++)
        {
            // read interruptResult from memory.
            uint32_t val;
            e_read(&emem, 0, 0, (row * platform.cols + col) * _BufSize + m * 4, &val, sizeof(uint32_t));
            if (5 < m) fprintf(stderr, " after: 0x%x\n", val);
            else fprintf(stderr, "before: 0x%x", val);
        }

        col++;
    }

    output_sets(0, n_cores, &dev, rows, cols, n_regs, titles, regs, &get_register);

    // and close the workgroup.
    e_close(&dev);

    // Release the allocated buffer and finalize the
    // e-platform connection.
    e_free(&emem);
    e_finalize();

    return 0;
}

void print_usage(const char* argv0)
{
    printf("Usage: %s filename\n",
           argv0);
}

void process_args(int argc, char *argv[], char *elfFile)
{
    int c;

    static struct option long_options[] = {
        {0, 0, 0, 0}
    };

    while (1) {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, ":", long_options, &option_index);

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
    char elfFile[4096];

    process_args(argc, argv, elfFile);
    printf("using elf file: %s\n", elfFile);

    int rtn = mutex(elfFile);

    return rtn;
}
