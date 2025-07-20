/************************************************************

  emonitor.c

  Monitor the state of the epiphany cores

  Copyright (c) 2025 Peter Saunderson <peteasa@gmail.com>

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
 *************************************************************/

#include <e-hal.h>
#include <e-loader.h>  // for e_load_group
#include <errno.h>     // for errno
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EPIPHANY_DEV "/dev/epiphany/elink0"

// Prototypes

uint32_t get_register(e_epiphany_t *reg_dev, uint32_t reg)
{
    uint32_t ret;
    ssize_t err = e_read(reg_dev, 0, 0, reg, &ret, sizeof(uint32_t));
    return ret;
}

void output_sets(int n_iter, int n_cores, uint32_t *cores, e_epiphany_t *reg_devs,
                 int n_regs, char **titles, uint32_t *regs,
                 uint32_t (*get_reg)(e_epiphany_t *reg_dev, uint32_t reg))
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
        printf("%04x  ", get_register(&reg_devs[c], regs[0]));
        for (int r = 1; r < n_regs; r++) {
            // read and print register values
            printf("%08x  ", get_reg(&reg_devs[c], regs[r]));
        }

        printf("\n");
    }

    printf("\n");
}

void print_usage(const char* argv0)
{
    printf("Usage: %s filename\n",
           argv0);
}

void process_args(int argc, char *argv[], char *elfFile)
{
    static struct option long_options[] = {
        {0, 0, 0, 0}
    };

    while (1) {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        int c = getopt_long (argc, argv, ":", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
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

int main(int argc, char *argv[])
{
    int returns = EXIT_SUCCESS;

    // init and open
    int n_rows = 4;
    int n_cols = 4;
    e_platform_t platform;
    e_epiphany_t reg_devs[n_rows * n_cols];
    e_set_host_verbosity(H_D0);
    e_init(NULL);
    e_get_platform_info(&platform);

    char elfFile[4096];
    process_args(argc, argv, elfFile);
    printf("using elf file: %s\n", elfFile);

    uint32_t regs[] = { E_REG_COREID, E_REG_CONFIG, E_REG_STATUS, E_REG_PC, E_REG_CTIMER0, E_REG_CTIMER1,
                        E_REG_DMA0STATUS, E_REG_DMA1STATUS, E_REG_IRET, E_REG_IMASK, E_REG_ILAT, E_REG_IPEND };
    char *titles[] = {"core", " config ", " status ", "   pc   ", "ctimer0 ", "ctimer1 ",
                      "dma0sts ", "dma1sts ", "  iret  ", " imask ", "   ilat  ", " ipend ",
    };

    // open the Epiphany devices
    uint32_t cores[n_rows * n_cols];
    for (int r = 0; r < n_rows; r++) {
        for (int c = 0; c < n_cols; c++) {
            int n_core = r * n_cols + c;
            cores[n_core] = r * 0x100 + c;
            e_open(&reg_devs[n_core], r, c, 1, 1);
            e_load_group(elfFile, &reg_devs[n_core], 0, 0, 1, 1, E_TRUE);
        }
    }

    // prepare the table
    int n_cores = sizeof(cores) / sizeof(uint32_t);
    int n_regs = sizeof(regs) / sizeof(uint32_t);
    int n_titles = sizeof(titles) / sizeof(char *);

    // output the register values
    for (int i = 0; i < 10; i++) {
        output_sets(i, n_cores, cores, reg_devs, n_regs, titles, regs, &get_register);
        sleep(1);
    }

    // tidy up the Epiphany devices
    for (int r = 0; r < n_rows; r++) {
        for (int c = 0; c < n_cols; c++) {
            int n_core = r * n_cols + c;
            e_close(&reg_devs[n_core]);
        }
    }

    e_finalize();

    return returns;
}

