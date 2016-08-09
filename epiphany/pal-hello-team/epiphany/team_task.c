#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pal.h>
#include <e-lib.h> // for c_timer* used with platform_clock
#include "common.h"

volatile struct status *epiphany_status = (struct status *) MEM_STATUS;
struct result *epiphany_results = (struct result *) MEM_RESULTS;

struct item_data
{
    uint64_t start;
    uint64_t end;
};

static uint64_t platform_clock(void)
{
    // Assuming 600MHz clock 10^9 / (600 * 10^6)
    const float factor = 1.6666666666666666666f;

    static uint64_t clock = 0;
    uint64_t diff, nanosec;

    static bool initialized = false;

    if (!initialized) {
        e_ctimer_stop(E_CTIMER_0);
        e_ctimer_set(E_CTIMER_0, E_CTIMER_MAX);
        e_ctimer_start(E_CTIMER_0, E_CTIMER_CLK);
        initialized = true;
        return 0;
    }

    /* Clock will drift here */
    diff = E_CTIMER_MAX - e_ctimer_get(E_CTIMER_0);

    e_ctimer_stop(E_CTIMER_0);
    e_ctimer_set(E_CTIMER_0, E_CTIMER_MAX);
    e_ctimer_start(E_CTIMER_0, E_CTIMER_CLK);

    clock += diff;

    nanosec = (uint64_t) ((float) clock * factor);

    return nanosec;
}

int main(int argc, char *argv[])
{
    p_dev_t dev;
    p_coords_t my_coords, tmp_coords;
    int err, rank;
    uint32_t coreid;
    struct item_data data;
    p_coords_t test_coords;

    data.start = platform_clock();
    
    dev = p_init(P_DEV_EPIPHANY, 0);           // Used for p_query()
    
    // whoamiid = p_query(dev, P_PROP_WHOAMI); // Use when implemented
    coreid = e_get_coreid();                   // Not used
    
    rank = p_team_rank(P_TEAM_DEFAULT);
    err = p_rank_to_coords(P_TEAM_DEFAULT, rank, &my_coords, 0);

    if ((0 <= rank) && (rank < 16))
    {
        epiphany_results[rank].argc = (uint32_t) argc;
        sprintf(epiphany_results[rank].args0, "%s", argv[0]);
        epiphany_results[rank].args1 = 0; //(uint32_t) atoi(argv[1]);

        epiphany_results[rank].size = (uint64_t) sizeof(struct result);

        sprintf(epiphany_results[rank].name, "member %d", rank);

        epiphany_results[rank].coreid = coreid;
        epiphany_results[rank].all = p_query(dev, P_PROP_NODES);
        epiphany_results[rank].team_size = p_team_size(P_TEAM_DEFAULT);

        epiphany_results[rank].rank = (uint32_t) rank;
        epiphany_results[rank].row = (uint32_t) my_coords.row;
        epiphany_results[rank].col = (uint32_t) my_coords.col;

        tmp_coords.row = -1;
        tmp_coords.col = 0;
        epiphany_results[rank].north_rank = p_rel_coords_to_rank(P_TEAM_DEFAULT, rank, &tmp_coords, P_COORDS_WRAP);
        tmp_coords.row = 0;
        tmp_coords.col = -1;
        epiphany_results[rank].west_rank = p_rel_coords_to_rank(P_TEAM_DEFAULT, rank, &tmp_coords, P_COORDS_WRAP);

        data.end = platform_clock();
        
        epiphany_results[rank].ns = data.end - data.start;
        epiphany_status->done = 1;
    }
    else
    {
        epiphany_status->done = -rank;
    }
 
    epiphany_status->num_results = p_team_size(P_TEAM_DEFAULT);

    return EXIT_SUCCESS;
}
