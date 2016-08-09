#pragma once

#include <stdint.h>
#include <inttypes.h>

#define MEM_STATUS (0x8f200000)

// Test status common to all cores
struct status {
    uint32_t done;
    uint32_t _pad1;
    uint32_t num_results;
    uint32_t _pad2;
} __attribute__((packed));

#define MEM_RESULTS (0x8f300000)

// Test results, one per core
struct result {
    char name[64];
    uint64_t ns;
    uint64_t size;    // Size of struct result
    uint32_t all;
    uint32_t argc;
    char args0[16];
    uint32_t args1;
    uint32_t team_size;
    uint32_t coreid;
    uint32_t rank;
    uint32_t row;
    uint32_t col;
    uint32_t north_rank;
    uint32_t west_rank;
} __attribute__((packed));
