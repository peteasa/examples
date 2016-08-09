#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <pal.h>
#include "common.h"

#define SIZERESULTS (16 * (64 + 8 * 2 + 4 * 10 + 16))
#define NUMBEROFCORES (4)

int main(int argc, char **argv)
{
    // Pointers to opaque PAL objects
    p_dev_t dev0;      // device information
    p_prog_t prog0; // program to execute
    p_team_t team0;    // working team
    p_coords_t start_coords, size_coords;

    // Stack variables
    const char *file = "team_task.elf";
    const char *func = "_main";
    int type = P_DEV_EPIPHANY;
    int i, all, myid, err;
    ssize_t size;

    p_mem_t status_mem, results_mem;
    struct status status = { .num_results = 1 };
    struct result *results;
    uint8_t clear[SIZERESULTS] = { 0 };

    char *msg = "team work";
    uint32_t val = 99;
    int nargs = 2;
    p_arg_t args[] = {
        { msg, strlen(msg), false },
        { &val, sizeof(val), true },
    };

    printf("main(): msg: %s, size msg: %p, val: %d, size val: %d\n", args[0].ptr, args[0].size, *((int *)args[1].ptr), args[1].size);

    // PAL flow
    dev0 = p_init(type, 0);                  // initialize system
    prog0 = p_load(dev0, file, 0); // load a program from file system
    myid = p_query(dev0, P_PROP_WHOAMI);     // find my id
    all = p_query(dev0, P_PROP_NODES);       // find # of device nodes
    start_coords.row = 0;
    start_coords.col = 0;
    size_coords.row = NUMBEROFCORES/2;
    size_coords.col = NUMBEROFCORES/2;
    team0 = p_open4(dev0, P_TOPOLOGY_2D, &start_coords, &size_coords);            // open a team

    printf("main(): dev0: %p, prog0: %p, team0: %p\n", dev0, prog0, team0);
    
    status_mem = p_map(dev0, MEM_STATUS, sizeof(status));
    printf("main(): status_mem: %p\n", status_mem);
    results_mem = p_map(dev0, MEM_RESULTS, SIZERESULTS);
    printf("main(): results_mem: %p\n", results_mem);

    /* Clear */
    size = p_write(&status_mem, &status, 0, sizeof(status), 0);
    printf("main(): sizeof(status): %p, size written: %p\n", sizeof(status), size);
    size = p_write(&results_mem, clear, 0, sizeof(clear), 0);
    printf("main(): sizeof(clear): %p, size written: %p\n", sizeof(clear), size);

    err = p_run(prog0, func, team0, 0, NUMBEROFCORES, nargs, args, 0);
    printf("main(): p_run returned: %d\n", err);

    p_wait(team0);

    /* Read back */
    size = p_read(&status_mem, &status, 0, sizeof(status), 0);
    printf("main(): sizeof(status): %p, size read: %p\n", sizeof(status), size);
    results = alloca(sizeof(*results) * NUMBEROFCORES);
    size = p_read(&results_mem, results, 0, sizeof(*results) * NUMBEROFCORES, 0);
    printf("main(): sizeof(result): %p, size read: %p\n", sizeof(*results) * status.num_results, size);

    printf("main(): myid: %p, num_results: %d, done: %d\n", myid, status.num_results, status.done);
    printf("main(): name, duration (ns), size, argc, args0, args1, coreid, team_size, rank, row, col, north_rank, west_rank\n");
    for (i = 0; i < NUMBEROFCORES; i++)
        printf("main(): %s, %" PRIu64 ", %" PRIu64 ", %p, %s, %p, %p, %p, %p, %p, %p, %p, %p\n",
               results[i].name, results[i].ns, results[i].size, results[i].argc,
               results[i].args0, results[i].args1, results[i].coreid, results[i].team_size,
               results[i].rank, results[i].row, results[i].col,
               results[i].north_rank, results[i].west_rank);

    p_close(team0);                       // close down team
    p_finalize(dev0);                     // close down the device
}
