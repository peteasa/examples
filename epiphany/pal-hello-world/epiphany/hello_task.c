#include <stdlib.h>
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
    struct item_data data;
    int rank;

    data.start = platform_clock();

    dev = p_init(P_DEV_EPIPHANY, 0);           // Used for p_query()
    rank = p_team_rank(P_TEAM_DEFAULT);

    strcpy(epiphany_results[rank].name, "hello task");

    data.end = platform_clock();

    if ((0 <= rank) && (rank < 16))
    {
        epiphany_results[rank].ns = data.end - data.start;

        epiphany_results[rank].size = (uint64_t) sizeof(struct result);

        epiphany_status->done = 1;
    }
    else
    {
        epiphany_status->done = -rank;
    }

    epiphany_status->num_results = p_team_size(P_TEAM_DEFAULT);

    return EXIT_SUCCESS;
}
