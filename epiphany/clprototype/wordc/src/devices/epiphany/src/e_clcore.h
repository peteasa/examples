/************************************************************
 *
 * e_clcore.h
 *
 * A prototype for a Map Reduce algorithm to count
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

#ifndef E_CLCORE
#define E_CLCORE

#include <stdlib.h>
#include <libecomms.h>

#define NOPEER (16)

// Workspace
typedef struct cl_wk_hdr_t cl_wk_hdr_t;
typedef struct dst_list_t dst_list_t;
//
// e_clcore api function pointers
//
typedef struct {
    int (*mr_init)(void);
    void (*mr_finalize)(void);
    int (*mr_unpackhst)(const ec_command_t *wkcmd, char **wkp, const char *wklimit, cl_wk_hdr_t **unpkd);

    int (*mr_map)(char **wkp, char *wklimit, el_t **dstnxtfree, char *dstlimit);
    unsigned (*mr_allocate)(el_t *item, unsigned maxid);
    int (*mr_dstitems)(unsigned peer, size_t size);
    int (*mr_packditem)(el_t *ucitem, char **msp, char* msplimit, size_t *size);
    int (*mr_unpackcr)(unsigned peer, const ec_command_t *wkcmd, char **wkp, const char *wklimit, cl_wk_hdr_t **unpkd);
    int (*mr_unpackwk)(char **wkp, char *wklimit);
    int (*mr_mvuc)(el_t *ucitem);
    void (*mr_reduce)(el_t *usitem);
    int (*mr_reportpack)();//char **msp, char* msplimit, size_t *size);

    void (*mr_evstrt)(void);   // called at the start of each cycle
    void (*mr_evend)(void);    // called at the end of each cycle
    int (*mr_do)(unsigned peer, cl_wk_hdr_t *wkspace, const ec_command_t *wkcmd);
} cl_mr_ops_t;

// Run the core engine
extern int e_clcorerun(void);

// Initial the workspace and distribution cache
extern int e_clwkinit(
    const ec_command_t *cmd,
    cl_wk_hdr_t *wkspace,
    size_t wkdsize,
    dst_list_t *wkdstspace,
    size_t wkdstsize);

// Map reduce algorithm
extern void mr_processmap();
extern void mr_processreduce();
extern void mr_mapemit(el_t *ucitem, size_t packsize);
extern int mr_dstpackitems(unsigned peer, char *store, char *slimit, size_t *memsize);
extern void mr_dstprepcrmsg(unsigned peer, char **store, char **slimit);
extern void mr_dstsendcrmsg(unsigned peer, size_t datlength);
extern void mr_emitus(el_t *usitem);

// Physical address of data part of workspace
extern size_t e_wkdataphyaddr(cl_wk_hdr_t *wkspace);

// Construct dma descriptor
extern int e_cldmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    size_t size,
    e_dma_config_t cfg);
extern int e_clmdmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    unsigned ssep,
    unsigned dsep,
    unsigned count,
    e_dma_config_t cfg);

// Perform dma read of data into workspace
extern int e_cldmadread(cl_wk_hdr_t *wkspace, e_dma_desc_t *desc, size_t size);

// Mark workspace done
extern void e_clsetwkdone(cl_wk_hdr_t *wkspace);

// Initialization
extern int e_clcoreinit(cl_mr_ops_t *ops, unsigned *my_rank);

//
// Debug
//
extern char *buf;
extern size_t bufs;
extern msg_channel_t *msg_channel_core;

#endif
