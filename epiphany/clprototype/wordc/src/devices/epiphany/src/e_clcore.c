/************************************************************
 *
 * e_clcore.c
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "e_lib.h"

#include <libecomms.h>
#include "e_clcore.h"

#define MAXRANK (NOPEER)

// The linker labels of interest
extern int end, __stack, __heap_start, __heap_end, __clz_tab;

#define ENDDRAM1 (0x8fffffff)
#define STARTDRAM0 (0x8E000000)
#define STARTFREEDRAM ( (&__clz_tab + 0x124) > STARTDRAM0 ? &__clz_tab + 0x124 : STARTDRAM0 )
#define BUILDID (&__clz_tab + 0x100)

#define STARTFREELOCALRAM (&end )
register void * const stackpointer asm("sp");
#define APPROXSTACKSTART ( (stackpointer - 0x10) > (void*)STARTFREELOCALRAM ? (stackpointer - 0x10) : STARTFREELOCALRAM )
void* debug_size = STARTFREELOCALRAM;
#define WORSTCASESTACKSIZE()({})
#define WORSTCASESTACKSIZE_RENAME() ({                                                \
            void* tmpstacksize = APPROXSTACKSTART;    \
            if (tmpstacksize>debug_size)         \
            {                                         \
                debug_size = tmpstacksize;       \
            }                                         \
        })

// word_t for debug
typedef struct {
    el_t q;
    unsigned count;
    unsigned short size;
    unsigned char hash;
} word_t;
// word_t for debug

int debug_totalprocessed = 0;

char _buf[256] = { 0 };
char *buf = _buf;
size_t bufs = sizeof(_buf);

// Following could be allocated / freed
msg_channel_t _msg_channel_core;
msg_channel_t _msg_channel_consumer;
msg_channel_t _msg_channel_consumer_data;
msg_channel_t _msg_channel_producer;
msg_channel_t _ecrmsg_channel_producer_data;
msg_channel_t _msg_uchannel_inqueue_prd;
msg_channel_t _msg_uchannel_inqueue_cons;
msg_channel_t _msg_uchannel_store_cons;

msg_channel_t *msg_channel_core = &_msg_channel_core;
msg_channel_t *msg_channel_consumer = &_msg_channel_consumer;
msg_channel_t *msg_channel_consumer_data = &_msg_channel_consumer_data;
msg_channel_t *msg_channel_producer = &_msg_channel_producer;
msg_channel_t *ecrmsg_channel_producer_data = &_ecrmsg_channel_producer_data;
msg_channel_t *msg_uchannel_inqueue_prd = &_msg_uchannel_inqueue_prd;
msg_channel_t *msg_uchannel_inqueue_cons = &_msg_uchannel_inqueue_cons;
msg_channel_t *msg_uchannel_store_cons = &_msg_uchannel_store_cons;

//
// Workspace used to perform tasks
//
typedef enum {
    WS_IDLE = 0,
    WS_BUSY,
    WS_DMA,  // TODO is this necessary?
    WS_RDY,
    WS_DIST,
} wk_status_t;

// Distribution cache used to output from workspace
typedef struct dst_list_t {
    el_t q;
    unsigned dbgcount;
} dst_list_t;
typedef enum {
    DST_NTFL = 0,
    DST_FULL,
} dst_status_t;

typedef struct cl_wk_hdr_t {
    wk_status_t wkactive;
    char *wklimit;
    unsigned wkpeer;
    char *wkplaceptr;
    dst_list_t *dstnxtfree;
    char *dstlimit;
    dst_status_t dstfull;
    ec_command_t wkcmd;
} cl_wk_hdr_t;

cl_wk_hdr_t *workspace;

typedef struct {
    el_t q;
    unsigned peer;
    ec_status_t entry;
} work_list_t;
work_list_t worklist[MAXRANK]; // contains list of work to do

// Message stores used to send data to other cores
#define TMPSTORESIZE (0x20) // expected data size of the application
char tmpstore[TMPSTORESIZE + sizeof(cl_wk_hdr_t)];
char inqueuestore[MAXRANK][TMPSTORESIZE + sizeof(ec_command_t)];
ec_status_t InQueue[MAXRANK]; // contains Order and Rand for work scheduling

// eCore Status
// TODO consider changing this to a different type
ec_status_t eCoreStatus;

// Producer data
dst_list_t* uConsumers[MAXRANK];
size_t uConsumersPackSize[MAXRANK];
ec_command uConsumersCmd[MAXRANK];

unsigned my_rank;

// DMA: row and column of the neigbour used in the isr
unsigned row0, col0, row1, col1;
// dma descriptors
e_dma_desc_t dma_desc[2];

//
// clcore api function pointers
//
cl_mr_ops_t *mr_ops;

// Forward declarations
void consumehostmessages();
void consumeecoremessages();
void consumeecoremessage(
    unsigned peer,
    unsigned entry,
    unsigned *processed);
int consumeworkorder(work_list_t *worktodo, unsigned *processed);
void parseecoremessage(
    unsigned peer,
    ec_command cmdid,
    wk_status_t wkactivestart,
    int *ack);
void unpackworkspace(unsigned peer, unsigned entry, ec_command cmdid, ec_command_t *wktmpcmd);
void unpackinqueuemsg(
    unsigned peer,
    unsigned entry,
    ec_command cmdid,
    ec_command_t *wktmpcmd);
void processworkspace();
void (*do_processing)();

//
// Workspace helpers
//
int wkactive();
int wkstate(wk_status_t wkstate);
int wkavailable();
void buildworklist(work_list_t *worktodo);
void additemlowfirst(work_list_t *item, work_list_t *worktodo);

// Set workspace active
void setwkactive(unsigned peer, cl_wk_hdr_t *wkspace);

// Initialize the uConsumers used during distribution
void inituConsumers();

//
// InQueue helpers
//
int freeecore(unsigned peer);
int findfreeecore(unsigned *peer);
void ackinqueuepeer(unsigned peer);
int fetchstore(msg_channel_t *ecrchannel, size_t srccmdoffset, ec_command_t *memformsg, size_t limit);

//
// Handle commands from Host
//
void do_clear_consumer();
int do_host_echo(ec_command_t *cmd);
int do_host_fwd(ec_command_t *cmd, ec_command fwdcmd, ec_status_t fwdstatus);

//
// Handle commands from eCore
//
void do_ecore_echo(unsigned peer);
void do_ecore_fwd();
void do_ecore_map(wk_status_t wkactivestart, int *ack);
void do_ecore_rdc(wk_status_t wkactivestart, int *ack);

//
// Initialization
//
int initchannels();

//
// Map Sort Distribute
//
int mapsortfillbucket();
int distributebucket();
int distributeremote(unsigned peer);
int distributelocal();

//
// ISRs
//
void user_isr();
//void dma0_isr();
//void dma1_isr();

// Run the core engine
int e_clcorerun(void)
{
    int rtn = SUCCESS;

    while (rtn)
    {
        // Main loop, should not exit

        // Now put the CPU to sleep till an interrupt occurs
        __asm__ __volatile__ ("idle");

        // Enable the global interrupt
        e_irq_global_mask(E_FALSE);

        // Call to user code
        if (mr_ops->mr_evstrt)
        {
            mr_ops->mr_evstrt();
        }

        // Finish processing of workspace if possible
        processworkspace();

        // Per eCore message handling
        // Even if no free workspace continue the processing
        // The next message might be a cancel!
        consumeecoremessages();

        // Now if possible take on new work items from the host
        if (msg_channel_consumer)
        {
            // Host allocates one consumer per chip
            consumehostmessages();
        } else {
            /*const char  Msg[] = ".";
            snprintf(buf, bufs, Msg);
            if (!msg_append_str(msg_channel_core, buf))
            {
                msg_reset(msg_channel_core);
                msg_append_str(msg_channel_core, buf);
                }*/
        }

        // Call to user code
        if (mr_ops->mr_evend)
        {
            mr_ops->mr_evend();
        }
    }

    //mr_ops->mr_finalize(void);

    return rtn;
}

// Initial the workspace and distribution cache
int e_clwkinit(
    const ec_command_t *cmd,
    cl_wk_hdr_t *wkspace,
    size_t wksize,
    dst_list_t *wkdstspace,
    size_t wkdstsize)
{
    int rtn = SUCCESS;

    if ( (sizeof(cl_wk_hdr_t) > wksize) ||
         (sizeof(dst_list_t) > wkdstsize) )
    {
        rtn = FAIL;
    }

    if (rtn)
    {
        wkspace->wkactive = WS_BUSY;
        wkspace->wklimit = (char *)((size_t)wkspace + wksize);
        wkspace->wkpeer = NOPEER;
        wkspace->wkplaceptr = (char *)(wkspace + 1);
        wkspace->wkcmd.cmdstatus = cmd->cmdstatus;
        wkspace->wkcmd.cmd = cmd->cmd;
        wkspace->wkcmd.datlength = 0;
        wkspace->wkcmd.offset = 0;

        // TODO double check this.. it is correct to init the cache here ..
        //                          do we need a separate uConsumers for each workspace??
        inituConsumers();
        wkspace->dstnxtfree = wkdstspace;
        // cache space must be at least dst_list_t for a new header
        wkspace->dstlimit = (char *)((size_t)wkdstspace + wkdstsize - sizeof(dst_list_t));
        wkspace->dstfull = DST_NTFL;

        // NOTE workspace is set active later once dma is complete
        //      workspace = (cl_wk_hdr_t *)wkspace
    }

    return rtn;
}

// Physical address of data part of workspace
size_t e_wkdataphyaddr(cl_wk_hdr_t *wkspace)
{
    WORSTCASESTACKSIZE();
    return (size_t)(wkspace + 1);
}

// Construct dma descriptor
int e_cldmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    size_t size,
    e_dma_config_t cfg)
{
    WORSTCASESTACKSIZE();
    return ec_dmadesc(src,dst,desc,size,E_DMA_0,cfg);
}

int e_clmdmadesc(
    size_t src,
    size_t dst,
    e_dma_desc_t *desc,
    unsigned ssep,
    unsigned dsep,
    unsigned count,
    e_dma_config_t cfg)
{
    WORSTCASESTACKSIZE();
    return ec_mdmadesc(src,dst,desc,ssep,dsep,count,E_DMA_0,cfg);
}

// Perform dma read of data into workspace
int e_cldmadread(cl_wk_hdr_t *wkspace, e_dma_desc_t *desc, size_t size)
{
    // start the dma transfer
    int rtn = SUCCESS;

    if (e_dma_start(desc, E_DMA_0))
    {
        rtn = FAIL;
    }

    wkspace->wkcmd.datlength = size;
    wkspace->wklimit = (char *)((size_t)wkspace + sizeof(cl_wk_hdr_t) + size);
    wkspace->wkcmd.offset = (size_t)&wkspace->wkcmd;

    WORSTCASESTACKSIZE();
    return rtn;
}

// Mark workspace done
void e_clsetwkdone(cl_wk_hdr_t *wkspace)
{
    wkspace->wkactive = WS_IDLE;
    WORSTCASESTACKSIZE();
}

// Initialization
int e_clcoreinit(cl_mr_ops_t *ops, unsigned *rank)
{
    int rtn = SUCCESS;
    if (!ops)
    {
        rtn = FAIL;
    }

    if (rtn && !initchannels())
    {
        rtn = FAIL;
    }

    mr_ops = ops;
    if ( rtn && (!mr_ops->mr_unpackhst ||
                 !mr_ops->mr_map ||
                 !mr_ops->mr_allocate ||
                 !mr_ops->mr_dstitems ||
                 !mr_ops->mr_packditem ||
                 !mr_ops->mr_unpackcr ||
                 !mr_ops->mr_unpackwk ||
                 !mr_ops->mr_mvuc ||
                 !mr_ops->mr_reduce ||
                 !mr_ops->mr_reportpack ||
                 !mr_ops->mr_do
                 ) )
    {
        rtn = FAIL;
    }

    if (rtn && mr_ops->mr_init && !mr_ops->mr_init())
    {
        rtn = FAIL;
    }

    // Identify the CPU
    e_coreid_t	coreid;
    coreid = e_get_coreid();
    const char  Msg[] = "core 0x%03x end %p ";
    snprintf(buf, bufs, Msg, coreid, debug_size);
    if (rtn && !msg_append_str(msg_channel_core, buf))
    {
        // Shared memory region is too small for the message
        rtn = FAIL;
    }

    if (rtn)
    {
        *rank = my_rank;

        e_irq_attach(E_USER_INT, user_isr);
        //e_irq_attach(E_DMA0_INT, dma0_isr);
        //e_irq_attach(E_DMA1_INT, dma1_isr);
        e_irq_mask(E_USER_INT, E_FALSE);
        //e_irq_mask(E_DMA0_INT, E_FALSE);
        //e_irq_mask(E_DMA1_INT, E_FALSE);

        // Enable the global interrupt
        e_irq_global_mask(E_FALSE);
    }

    WORSTCASESTACKSIZE();
    return rtn;
}

void consumehostmessages()
{
    int ok = SUCCESS;

    // process commands from host
    while (ok && msg_channel_producer && msg_is_command_ready(msg_channel_producer))
    {
        // Note that the command is in shared memory.  It remains in place
        // till accepted by the eCore.  Thus if the Host wishes to cancel
        // the command it can cancel by changing the command to cancel
        ec_command_t cmd;
        msg_get_command(msg_channel_producer, &cmd);

        /*const char  Msg[] = "%p "; //s%xi%xl%lxo%lx ";
        snprintf(buf, bufs, Msg, (void*)ec_get_channel_curr_pos(msg_channel_producer)); //, cmd.cmdstatus, cmd.cmd, cmd.datlength, cmd.offset);
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
            }*/

        // TODO handle EC_SYNC
        // if EC_SYNC == ORD(eCoreStatus)
        // only process CANCEL from host until all cores done
        switch (cmd.cmd)
        {
        case CMD_CLEAR_CONSUMER:
            do_clear_consumer();
            ok = FAIL;
            break;
        case CMD_ECHO:
            ok = do_host_echo(&cmd);
            break;
        case CMD_FWD:
            ok = do_host_fwd(&cmd, CMD_ECHO, cmd.cmdstatus);
            break;
        case CMD_FMAP:
            ok = do_host_fwd(&cmd, CMD_MAP, cmd.cmdstatus);
            break;
        default:
            break;
        }

        if (ok && msg_channel_producer)
        {
            msg_mark_command_done(msg_channel_producer);
            msg_goto_next_cmd(msg_channel_producer);
        }

        // TODO consider handling workspace done here.
        //      set idle in e_clsetwkdone in generic command handler
        //      do we need to ack the workspace peer as well?
    }
}

void consumeecoremessages()
{
    unsigned processed = 0;
    work_list_t worktodo;
    worktodo.entry = cmd_gen_status(RNDMASK, RNDMASK);
    while (worktodo.entry)
    {
        // create ordered list of work
        buildworklist(&worktodo);

        /*const char  Msg1[] = "p%uo%ur%u.";
        snprintf(buf, bufs, Msg1, worktodo.peer, ORD(worktodo.entry), RND(worktodo.entry));
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
            }*/

        if (!worktodo.entry)
        {
            // No more pending work
            break;
        }

        if (!consumeworkorder(&worktodo, &processed))
        {
            // cant do more work now
            break;
        }
    }

    if (processed)
    {
        const char  Msg2[] = "p%d.";
        snprintf(buf, bufs, Msg2, debug_totalprocessed); //processed);
        //const char  Msg2[] = "p%p.";
        //snprintf(buf, bufs, Msg2, debug_size); //processed);
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }
    }
}

int consumeworkorder(work_list_t *worktodo, unsigned *processed)
{
    int rtn = SUCCESS;

    work_list_t *workitem = (work_list_t *)worktodo->q.fwd;
    unsigned i;
    for (i=0; i<MAXRANK; i++)
    {
        if (!workitem)
        {
            // End of current work list
            // rebuild the worklist
            break;
        }

        if ( workitem->entry == worktodo->entry )
        {
            consumeecoremessage(
                workitem->peer,
                workitem->entry,
                processed);

            if (wkactive() || EC_SYNC == ORD(worktodo->entry))
            {
                // TODO what about non-zero random number.. also need to handle that case
                //                                          for regular command
                // TODO if EC_SYNC
                // eCoreStatus = cmd_gen_status(ORD(workitem->entry), workitem->peer);
                // ??interrupt peer to instruct that we are now paused??
                // cant do more work now, continue later
                rtn = FAIL;
                const char  Msg[] = "w%p.";
                snprintf(buf, bufs, Msg, workspace);
                if (!msg_append_str(msg_channel_core, buf))
                {
                    msg_reset(msg_channel_core);
                    msg_append_str(msg_channel_core, buf);
                }

                break;
            }
        } else {
            // before moving on
            // rebuild the worklist
            break;
        }

        workitem = (work_list_t *)workitem->q.fwd;
    }

    return rtn;
}

void consumeecoremessage(
    unsigned peer,
    unsigned entry,
    unsigned *processed)
{
    /*const char  Msg1[] = "p%dn%xw%p.";
    snprintf(buf, bufs, Msg1, peer, entry, workspace);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    wk_status_t wkactivestart = wkactive();
    msg_reset(msg_uchannel_store_cons);
    msg_uchannel_set_rank(msg_uchannel_store_cons, peer);

    cl_wk_hdr_t *wktmpspace = (cl_wk_hdr_t *)tmpstore;
    ec_command_t *wktmpcmd = &wktmpspace->wkcmd;
    ec_command cmdid;
    msg_get_cmdcmdid(msg_uchannel_store_cons, &cmdid);
    wktmpcmd->cmd = cmdid;
    if ( (CMD_ECHO != cmdid) &&
         (CMD_FWD != cmdid) &&
         !wkactivestart)
    {
        unpackworkspace(peer, entry, cmdid, wktmpcmd);
    }

    /*//const char  Msg[] = "p%ds%xi%xl%lxo%lx-"; // "r%uc%uo%lx-";//
    //snprintf(buf, bufs, Msg, peer, cmd->cmdstatus, cmd->cmd, cmd->datlength, cmd->offset); //(*channel)->row, (*channel)->col, ec_get_channel_curr_pos(*channel) ); //
    //const char  Msg[] = "%s";
    //snprintf(buf, bufs, Msg, (char *)(workspace + 1));
    const char  Msg[] = "p%ui%xl%lxo%lx-";
    snprintf(buf, bufs, Msg, peer, cmd->cmd, cmd->datlength, cmd->offset);
    //snprintf(buf, bufs, Msg, peer, workspace->wkcmd.cmd, workspace->wkcmd.datlength, workspace->wkcmd.offset);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    int ack;
    // TODO sort the logic of this... when to acknowledge and when to unpack the message
    //      or dma and unpack after parse command??
    parseecoremessage(peer, cmdid, wkactivestart, &ack);

    if (ack)
    {
        ackinqueuepeer(peer);
    }

    processworkspace();

    (*processed)++;
    debug_totalprocessed++;
}

void parseecoremessage(
    unsigned peer,
    ec_command cmdid,
    wk_status_t wkactivestart,
    int *ack)
{
    *ack = SUCCESS;
    // if workspace full still parse command in case of cancel
    switch (cmdid)
    {
    case CMD_ECHO:
        do_ecore_echo(peer);
        break;
    case CMD_FWD:
        do_ecore_fwd();
        break;
    case CMD_MAP:
        do_ecore_map(wkactivestart, ack);
        break; 
    case CMD_RDC:
        // TODO what if the workspace is full?
        // if wkactive == WS_RDY then perhaps ok to acknowledge
        //    if not then dont acknowledge
        //    TODO check that this would result in the channel being re-read
        //    TODO check what happens if workspace is stalled then goes back to WS_RDY..
        //         need to know the state of the workspace before the loading of the workspace!
        //    TODO tidy this design .. actions taken on the transition between states rather than
        //         during the state might help!
        do_ecore_rdc(wkactivestart, ack);
        break;
    default:
        break;
    }
}

void unpackworkspace(unsigned peer, unsigned entry, ec_command cmdid, ec_command_t *wktmpcmd)
{
    // now dma the whole command and data
    size_t wktmplimit = sizeof(tmpstore) + sizeof(ec_command_t) - sizeof(cl_wk_hdr_t);
    if (!fetchstore(msg_uchannel_store_cons, ec_get_channel_base(msg_uchannel_store_cons), wktmpcmd, wktmplimit))
    {
        const char  Msg[] = "Eul%lxm%lx.";
        snprintf(buf, bufs, Msg, wktmpcmd->datlength, wktmplimit);
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }
    } else {
        // TODO somewhere here mark eCoreStatus with the current command in progress....

        // TODO EC_SYNC is a no-op work out how to handle this
        unpackinqueuemsg(peer, entry, cmdid, wktmpcmd);
    }
}

void unpackinqueuemsg(
    unsigned peer,
    unsigned entry,
    ec_command cmdid,
    ec_command_t *wktmpcmd)
{
    int ok = SUCCESS;

    workspace = (cl_wk_hdr_t *)tmpstore;
    workspace->wkactive = WS_BUSY;
    workspace->wklimit = (char *)((size_t)workspace + sizeof(cl_wk_hdr_t) + wktmpcmd->datlength);
    workspace->wkpeer = peer;
    workspace->wkplaceptr = (char *)(workspace + 1);
    workspace->wkcmd.cmdstatus = cmd_gen_status(EC_BUSY, RND(wktmpcmd->cmdstatus));
    workspace->dstfull = DST_FULL;

    cl_wk_hdr_t *unpacked;
    // TODO in future use cmd to provide cmd and info about msg source ie unpack type
    switch (cmdid)
    {
    case CMD_MAP:
        if (!mr_ops->mr_unpackhst(&workspace->wkcmd, &workspace->wkplaceptr, workspace->wklimit, &unpacked))
        {
            const char  Msg[] = "Ep%un%xi%x.";
            snprintf(buf, bufs, Msg, peer, entry, wktmpcmd->cmd);
            if (!msg_append_str(msg_channel_core, buf))
            {
                msg_reset(msg_channel_core);
                msg_append_str(msg_channel_core, buf);
            }

            ok = FAIL;
        }
        break;
    case CMD_RDC:
        if (!mr_ops->mr_unpackcr(peer, &workspace->wkcmd, &workspace->wkplaceptr, workspace->wklimit, &unpacked))
        {
            const char  Msg[] = "Er%un%xi%x.";
            snprintf(buf, bufs, Msg, peer, entry, wktmpcmd->cmd);
            if (!msg_append_str(msg_channel_core, buf))
            {
                msg_reset(msg_channel_core);
                msg_append_str(msg_channel_core, buf);
            }

            ok = FAIL;
        }
        break;
    default:
        // TODO fix this up for each msg source / command type
        //      for now keep it simple
        unpacked = workspace;
        workspace->wkcmd.cmdstatus = cmd_gen_status(EC_CURR, RND(wktmpcmd->cmdstatus));
    }

    // wait till dma done
    while (e_dma_busy(E_DMA_0));

    // Now set the workspace active
    if (ok)
    {
        setwkactive(peer, unpacked);
    } else {
        // TODO work out how to handle this.. return failure??
        //      do we want to ack the peer also?
        workspace = NULL;
    }

    // TODO check the logic of this!
    eCoreStatus = cmd_gen_status(ORD(entry), peer);

    WORSTCASESTACKSIZE();
}

void processworkspace()
{
    if (do_processing)
    {
        do_processing();
    }

    if ( workspace && !wkactive() )
    {
        ackinqueuepeer(workspace->wkpeer);

        /*const char  Msg[] = "ack%dc%d";
        snprintf(buf, bufs, Msg, wkpeer, wkcmd.cmd);
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }
        */
    }
}

//
// Workspace helpers
//
int wkactive()
{
    return workspace && workspace->wkactive;
}

int wkstate(wk_status_t wkstate)
{
    return workspace && wkstate == workspace->wkactive;
}

int wkavailable()
{
    return workspace &&
        WS_IDLE != workspace->wkactive &&
        WS_BUSY != workspace->wkactive;
    // != WS_DMA
}

int distfull()
{
    return workspace && workspace->dstfull;
}

void buildworklist(work_list_t *worktodo)
{
    unsigned peer;
    el_init(&worktodo->q, &worktodo->q);
    worktodo->entry = cmd_gen_status(RNDMASK, RNDMASK);
    worktodo->peer = NOPEER;
    
    msg_reset(msg_uchannel_inqueue_cons);
    for (peer = 0; peer<MAXRANK; peer++)
    {
        unsigned *entry;
        msg_get_sgmnt(msg_uchannel_inqueue_cons, (void**)&entry);
        work_list_t *item = &worklist[peer];
        item->peer = peer;
        if (ORD(*entry) && EC_BUSY != ORD(*entry))
        {
            item->entry = *entry;
        } else {
            item->entry = EC_IDLE;
        }

        /*if (item->entry)
        {
            const char  Msg[] = "p%dn%x.";
            snprintf(buf, bufs, Msg, peer, *entry);
            if (!msg_append_str(msg_channel_core, buf))
            {
                msg_reset(msg_channel_core);
                msg_append_str(msg_channel_core, buf);
            }
            }*/

        if (item->entry && (peer != my_rank))
        {
            if (worktodo->entry > item->entry)
            {
                // EC_SYNC,RND or ec_cmd_order less than
                // cmd_gen_status(RNDMASK, RNDMASK) and greater
                // than 0: found new higher priority work
                worktodo->entry = item->entry;
                worktodo->peer = item->peer;
            }

            additemlowfirst(item, worktodo);
        }

        if (wkactive() && (item->peer == workspace->wkpeer))
        {
            // Include current workspace work into the list
            // Allow current InQueue to override old blocked work
            if (!item->entry)
            {
                item->entry = cmd_gen_status(EC_CURR, RND_NO_SYNC);
                if (worktodo->entry > item->entry)
                {
                    worktodo->entry = item->entry;
                    worktodo->peer = item->peer;
                }

                additemlowfirst(item, worktodo);
            }
        }

        msg_goto_next_sgmnt(msg_uchannel_inqueue_cons);
    }
    
    if (NOPEER == worktodo->peer)
    {
        worktodo->peer = NOPEER;
        worktodo->entry = EC_IDLE;
    }
}

void additemlowfirst(work_list_t *item, work_list_t *worktodo)
{
    el_init(&item->q, &worktodo->q);
    el_t *pos = &worktodo->q;
    work_list_t *wi = (work_list_t *)pos->fwd;
    unsigned i;
    for (i=0; i<MAXRANK; i++)
    {
        if (!wi)
        {
            // No more items add to end of list
            el_insert(&item->q, pos);

            break;
        }

        if (wi->entry > item->entry)
        {
            // lowest value at top of list
            el_insert(&item->q, pos);
            break;
        }

        pos = pos->fwd;
        wi = (work_list_t *)pos->fwd;
    }

    WORSTCASESTACKSIZE();
}

// Set workspace active
void setwkactive(unsigned peer, cl_wk_hdr_t *wkspace)
{
    wkspace->wkactive = WS_RDY;
    wkspace->wkpeer = peer;
    workspace = wkspace;
}

// Initialize the uConsumers used during distribution
void inituConsumers()
{
    int i;
    for (i=0; i<MAXRANK; i++)
    {
        uConsumers[i] = NULL;
        uConsumersPackSize[i] = 0;
        uConsumersCmd[i] = CMD_IDLE;
    }
}

//
// InQueue helpers
//
int freeecore(unsigned peer)
{
    int rtn;

    // check receiver
    unsigned inqueue = 0;
    ec_command_t *storecmd = (ec_command_t *)inqueuestore[peer];
    msg_reset(msg_uchannel_inqueue_prd);
    msg_uchannel_set_rank(msg_uchannel_inqueue_prd, peer);
    uc_read(msg_uchannel_inqueue_prd, &inqueue, sizeof(ec_status_t));

    rtn = !inqueue && !msg_command_inuse(storecmd);

    if (!rtn)
    {
        const char  Msg[] = "wrn%do%xr%xs%x ";
        snprintf(buf, bufs, Msg, peer, ORD(inqueue), RND(inqueue), storecmd->cmdstatus );
        //const char  Msg[] = "q%p o0x%lu r%u c%u ";
        //snprintf(buf, bufs, Msg, &InQueue[my_rank], msg_uchannel_inqueue_prd->initialoffset, msg_uchannel_inqueue_prd->row, msg_uchannel_inqueue_prd->col);
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }
    }

    return rtn;
}

int findfreeecore(unsigned *peer)
{
    int ok = FAIL;
    *peer = 0;
    while (*peer < MAXRANK)
    {
        if (my_rank != *peer)
        {
            // check receiver
            if (freeecore(*peer))
            {
                ok = SUCCESS;
                break;
            }
        }

        (*peer)++;
    }

    return ok;
}

void ackinqueuepeer(unsigned peer)
{
    msg_reset(msg_uchannel_store_cons);
    msg_uchannel_set_rank(msg_uchannel_store_cons, peer);
    msg_mark_command_done(msg_uchannel_store_cons);

    InQueue[peer] = EC_IDLE;

    if (!wkactive())
    {
        workspace = NULL;
        do_processing = NULL;
    }

    // now interrupt the remote core
    msg_uchannel_int_peer(peer);
}

int fetchstore(msg_channel_t *ecrchannel, size_t srccmdoffset, ec_command_t *memformsg, size_t limit)
{
    int rtn = SUCCESS;
    size_t msglength;
    memformsg->cmdstatus = cmd_gen_status(EC_BUSY, RND_NO_SYNC);
    memformsg->cmd = CMD_IDLE;

    // fetch datlength from remote core
    ec_get_cmddatlength(ecrchannel, srccmdoffset, &msglength);
    memformsg->datlength = msglength;
    msglength += sizeof(ec_command_t);

    if (msglength <= limit)
    {
        // now dma the whole command and data
        // Use temporary workspace to get packed command
        // cmd->cmdstatus, cmd and datlength are re-written here
        size_t dst = (size_t)memformsg;
        size_t src = ec_phyaddr(ecrchannel, srccmdoffset);
        e_cldmadesc(src, dst, dma_desc, msglength, 0);
        if (e_dma_start(dma_desc, E_DMA_0))
        {
            rtn = FAIL;
        }

        while (e_dma_busy(E_DMA_0));
    } else {
        // cmd->cmdstatus, cmd and datlength are left unchanged
        const char  Msg[] = "Efl%lxm%lx.";
        snprintf(buf, bufs, Msg, msglength, limit);
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }

        rtn = FAIL;
    }

    return rtn;
}

//
// Handle commands from Host
//
void do_clear_consumer()
{
    // Consume the command before clearing the channel!
    msg_mark_command_done(msg_channel_producer);
    msg_goto_next_cmd(msg_channel_producer);

    msg_channel_consumer = NULL;
    msg_channel_consumer_data = NULL;
    msg_channel_producer = NULL;
    ecrmsg_channel_producer_data = NULL;

    // reset the per core channel just to make the debug
    // easier to see.
    msg_reset(msg_channel_core);
    const char  Msg[] = "q";
    snprintf(buf, bufs, Msg);
    msg_append_str(msg_channel_core, buf);
}

int do_host_echo(ec_command_t *cmd)
{
    int ok = SUCCESS;
    size_t cons_offset = 0;

    if (cmd->datlength)
    {
        const char  Msg[] = "o%p ";
        snprintf( buf, bufs, Msg, (void*)cmd->offset);
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }

        // read command and data from data store
        msg_copy_data(ecrmsg_channel_producer_data, (void *)tmpstore, cmd, sizeof(tmpstore));

        ec_command_t *dcmd = (ec_command_t *)tmpstore;

        // TODO in future dont unpack.. for now use this to test
        //      dma read
        char *wkp = (char *)((size_t)tmpstore + sizeof(ec_command_t));
        char *wklimit = (char *)((size_t)wkp + dcmd->datlength);
        cl_wk_hdr_t *unpacked;
        if (!mr_ops->mr_unpackhst(dcmd, &wkp, wklimit, &unpacked))
        {
            ok = FAIL;
        }

        // wait till dma done
        while (e_dma_busy(E_DMA_0));

        // Now set the workspace active
        if (ok)
        {
            setwkactive(NOPEER, unpacked);
        }

        // Now test the generic command handling
        if (ok && !mr_ops->mr_do(NOPEER, unpacked, &(unpacked->wkcmd)))
        {
            ok = FAIL;
        }

        // Now complete e_clsetwkdone() and set the workspace inactive
        // equivalent to work done in ackinqueuepeer for ecore messages
        workspace = NULL;
        do_processing = NULL;

        // End of TODO
        // TODO in this case the workspace now needs to be set to null..

        if (ok)
        {
            // Mark producer data command done
            msg_mark_datacmd_done(ecrmsg_channel_producer_data, cmd);

            if (!msg_space(
                msg_channel_consumer_data,
                cmd->datlength+sizeof(ec_command_t)))
            {
                // TODO for now keep this simple and simply overwrite anything not received!
                //      works provided msg_channel_consumer_data is large
                //      works provided the host is fast
                msg_reset(msg_channel_consumer_data);
            }

            cons_offset = ec_get_channel_curr_pos(msg_channel_consumer_data);
            msg_append(
                msg_channel_consumer_data,
                tmpstore,
                cmd->datlength+sizeof(ec_command_t));
        }
    }

    if (ok)
    {
        ok = msg_send_command(msg_channel_consumer, cmd_gen_status(EC_SEQ, RND_NO_SYNC), CMD_FWD, cmd->datlength, cons_offset);
    }

    WORSTCASESTACKSIZE();
    return ok;
}

int do_host_fwd(ec_command_t *cmd, ec_command fwdcmd, ec_status_t fwdstatus)
{
    int ok = SUCCESS;

    // Host generated command points to the data in ecrmsg_channel_producer_data
    size_t pdoffset = cmd->offset;

    unsigned peer;
    // TODO get destination from inbound command / or choose best
    // Consider moving this so that the user code can choose how to forward
    ok = findfreeecore(&peer);
    ec_command_t *memformsg = (ec_command_t *)inqueuestore[peer];

    if (ok)
    {
        size_t wkslimit = sizeof(inqueuestore[peer]);
        ok = fetchstore(ecrmsg_channel_producer_data, pdoffset, memformsg, wkslimit);
    }

    if (ok)
    {
        // acknowledge the ecrmsg_channel_producer_data
        msg_mark_datacmd_done(ecrmsg_channel_producer_data, cmd);

        // pddatlength: unchanged when forwarding so dont change cmd->datlength
        memformsg->cmd = fwdcmd;
        memformsg->cmdstatus = fwdstatus;
        uc_write(msg_uchannel_inqueue_prd, &memformsg->cmdstatus, sizeof(ec_status_t));

        // now interrupt the remote core
        msg_uchannel_int_peer(peer);
    }

    // Debug print pdoffset
    //const char  Msg[] = "-%dc%xl%lxs%xo%lx-";
    //snprintf( buf, bufs, Msg, ok, fwdcmd, cmd->datlength, fwdstatus, pdoffset );
    /*const char  Msg[] = "o%lx-";
    snprintf( buf, bufs, Msg, pdoffset);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    WORSTCASESTACKSIZE();
    return ok;
}

//
// Handle commands from eCore
//
void do_ecore_echo(unsigned peer)
{
    // check receiver
    ec_status_t inqueue;
    ec_command_t *memformsg = (ec_command_t *)inqueuestore[peer];
    msg_reset(msg_uchannel_inqueue_prd);
    msg_uchannel_set_rank(msg_uchannel_inqueue_prd, peer);
    uc_read(msg_uchannel_inqueue_prd, &inqueue, sizeof(ec_status_t));

    /*const char  Msg[] = "p%uq%xs%x ";
    snprintf(buf, bufs, Msg, peer, inqueue, memformsg->cmdstatus);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    // channel contains command and data
    // data is after the command so
    if (!inqueue && !msg_command_inuse(memformsg)) 
    {
        // for now keep it simple
        // dma the data from the remote store to the local store
        size_t wkslimit = sizeof(inqueuestore[peer]);
        fetchstore(msg_uchannel_store_cons, ec_get_channel_base(msg_uchannel_store_cons), memformsg, wkslimit);

        ec_status_t status = cmd_gen_status(EC_SEQ, RND_NO_SYNC);
        uc_write(msg_uchannel_inqueue_prd, &status, sizeof(unsigned));
        memformsg->cmd = CMD_FWD;
        memformsg->cmdstatus = status;
    }
    // TODO what if busy?? for now just discard the message!

    WORSTCASESTACKSIZE();
}

void do_ecore_fwd()
{
    int ok = SUCCESS;
    size_t datlength;
    msg_get_cmddatlength(msg_uchannel_store_cons, &datlength);
    if (msg_channel_consumer)
    {
        // TODO check that the send_command will work
        //      if not then apply back pressure and prevent further comms
        //      till host has processed the consumer channel
        // TODO following is dma version of msg_send_command
        size_t n = datlength+sizeof(ec_command_t);
        if ( !msg_space(msg_channel_consumer_data, n) )
        {
            msg_reset(msg_channel_consumer_data);
        }

        size_t src = ec_phyaddr(msg_uchannel_store_cons, ec_get_channel_base(msg_uchannel_store_cons));
        size_t choffset = ec_get_channel_curr_pos(msg_channel_consumer_data);
        if (ec_dma_append(msg_channel_consumer_data, dma_desc, E_DMA_0, src, n))
        {
            while (e_dma_busy(E_DMA_0));
            ok = msg_send_command(msg_channel_consumer, cmd_gen_status(EC_SEQ, RND_NO_SYNC), CMD_FWD, datlength, choffset);
        } else {
            const char  Msg[] = "Ea%lxl%lxo%lx.";
            snprintf(buf, bufs, Msg, src, n, choffset);
            if (!msg_append_str(msg_channel_core, buf))
            {
                msg_reset(msg_channel_core);
                msg_append_str(msg_channel_core, buf);
            }
        }
    } else {
        // for now keep it simple
        size_t wkslimit = sizeof(tmpstore);
        ec_command_t *memformsg = (ec_command_t *)tmpstore;
        ok = fetchstore(msg_uchannel_store_cons, ec_get_channel_base(msg_uchannel_store_cons), memformsg, wkslimit);

        // dma read and unpack
        char *wkp = (char *)((size_t)tmpstore + sizeof(ec_command_t));
        char *wklimit = (char *)((size_t)wkp + datlength);
        cl_wk_hdr_t *unpacked;
        if (ok && !mr_ops->mr_unpackhst(memformsg, &wkp, wklimit, &unpacked))
        {
            ok = FAIL;
        }

        // wait till dma done
        while (e_dma_busy(E_DMA_0));

        // Now set the workspace active
        if (ok)
        {
            setwkactive(NOPEER, unpacked);
        }

        // Now test the generic command handling
        if (ok && mr_ops->mr_do && !mr_ops->mr_do(NOPEER, unpacked, &(unpacked->wkcmd)))
        {
            ok = FAIL;
        }
    }

    if (!ok)
    {
        const char  Msg[] = "Ewl%lxo%lx.";
        snprintf(buf, bufs, Msg, datlength, ec_get_channel_curr_pos(msg_uchannel_store_cons));
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }
    }

    /*const char  Msg2[] = ".f%luw%p.";
    snprintf(buf, bufs, Msg2, memformsg->datlength, workspace);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    WORSTCASESTACKSIZE();
}

void do_ecore_map(wk_status_t wkactivestart, int *ack)
{
    // TODO setup the processworkspace for map operation
    if (wkactivestart)
    {
        *ack = FAIL;
    } else {
        do_processing = mr_processmap;
    }
}

void do_ecore_rdc(wk_status_t wkactivestart, int *ack)
{
    if (wkactivestart)
    {
        *ack = FAIL;
    } else {
        do_processing = mr_processreduce;
    }
}

//
// Initialization
//
int initchannels()
{
    int rtn = SUCCESS;

    unsigned my_row;
    unsigned my_col;

    // Who am I? Query the CoreID from hardware.
    my_row = e_group_config.core_row;
    my_col = e_group_config.core_col;
    my_rank = my_row * e_group_config.group_cols + my_col;

    const char	ShmPerCoreName[] = "ecore";
    if (rtn && !msg_channel_shared_init(
        msg_channel_core,
        ShmPerCoreName,
        e_group_config.group_rows * e_group_config.group_cols,
        my_rank) )
    {
        rtn = FAIL;
    }

    //wktmp = inqueuestore[my_rank];

    const char ShmProducerName[] = "eproducer";
    if (rtn && !msg_channel_init(msg_channel_producer,
                           ShmProducerName) )
    {
        rtn = FAIL;
    }

    const char ShmProducerDataName[] = "eproducerdata";
    if (rtn && !ec_init_mem(ecrmsg_channel_producer_data,
                           ShmProducerDataName) )
    {
        rtn = FAIL;
    }

    const char ShmConsumerName[] = "econsumer";
    if (rtn && !msg_channel_init(msg_channel_consumer,
                           ShmConsumerName) )
    {
        rtn = FAIL;
    }

    const char ShmConsumerDataName[] = "econsumerdata";
    // TODO consider changing this to ec_init_mem
    if (rtn && !msg_channel_init(msg_channel_consumer_data,
                           ShmConsumerDataName) )
    {
        rtn = FAIL;
    }

    if (rtn)
    {
        msg_uchannel_init_rank(&e_group_config);
        // this eCores producer channel
        msg_uchannel_init(
            msg_uchannel_inqueue_prd,
            &e_group_config,
            (size_t)&InQueue[my_rank],
            sizeof(unsigned));

        // this eCores consumer channel
        msg_uchannel_local_init(
            msg_uchannel_inqueue_cons,
            (size_t)InQueue,
            sizeof(InQueue),
            sizeof(unsigned));
        // this eCores consumer store
        msg_uchannel_init(
                msg_uchannel_store_cons,
                &e_group_config,
                (size_t)inqueuestore[my_rank],
                sizeof(inqueuestore[my_rank]));

        int i;
        for (i=0; i<MAXRANK; i++)
        {
            // this eCores consumer channels
            InQueue[i] = 0;
            ec_command_t cmd;
            cmd.cmd = CMD_IDLE;
            cmd.cmdstatus = EC_IDLE;
            cmd.datlength = 0;
            cmd.offset = 0;
            memcpy(inqueuestore[i], &cmd, sizeof(ec_command_t));
        }
    }

    WORSTCASESTACKSIZE();
    return rtn;
}

//
// Map Sort Reduce Pack
//
void mr_processmap()
{
    // If DMA not yet finished state is WS_DMA and no processing is done

    if (wkstate(WS_RDY))
    {
        // split words sort into MAXRANK buckets
        // moves words from workspace to uConsumer cache space
        // in preparation for distributing to eCores
        if (mapsortfillbucket())
        {
            workspace->wkactive = WS_DIST;
        } else {
            workspace->dstfull = DST_FULL;
        }
    }

    if (wkstate(WS_DIST) || distfull())
    {
        // distribute words to uConsumers
        // moves words from linked list in cache space to store space
        // interrupts the consumer
        if (distributebucket())
        {
            if (wkstate(WS_DIST))
            {
                e_clsetwkdone(workspace);
            }
        }

        workspace->dstfull = DST_NTFL;
    }

    WORSTCASESTACKSIZE();
}

void mr_processreduce()
{
    if (wkavailable())
    {
        char *wkp = workspace->wkplaceptr;

        if (mr_ops->mr_unpackwk(&wkp, workspace->wklimit))
        {
            e_clsetwkdone(workspace);
        } else {
            workspace->wkplaceptr = wkp;
        }
    }
}

void mr_mapemit(el_t *ucitem, size_t packsize)
{
    // call user provided function to allocate an eCore
    // that will run the reduce algorithm
    unsigned peer = mr_ops->mr_allocate(ucitem, MAXRANK);

    // application can use variable size buffer so we need to
    // ensure alignment once the application has filled the buffer
    workspace->dstnxtfree = (dst_list_t *)em_align((size_t) workspace->dstnxtfree);

    if (!uConsumers[peer])
    {
        // New peer so init the uConsumers entry
        uConsumers[peer] = workspace->dstnxtfree;
        el_init(&workspace->dstnxtfree->q, &workspace->dstnxtfree->q);
        uConsumersPackSize[peer] = 0;
        uConsumers[peer]->dbgcount = 0;

        (workspace->dstnxtfree)++;
    }

    // Init item
    el_init(ucitem, &uConsumers[peer]->q);

    // Insert item into the list
    el_insert(ucitem, &uConsumers[peer]->q);
    uConsumersPackSize[peer] += packsize;
    uConsumers[peer]->dbgcount++;

    /*const char  Msg[] = "-p%u.c%u.l%lx%s-";
    snprintf( buf, bufs, Msg, peer, uConsumers[peer]->dbgcount, packsize, (char *)((word_t *)(uConsumers[peer]->q.fwd)+1) );
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    WORSTCASESTACKSIZE();
}

// TODO check these out perhaps they can be moved to mr specific?
// TODO change this to use msg_channel_store_prd??
//      memcpy not as fast as cpu because memcpy stored in remote memory
//      perhaps implement suitable helpers in msg_??
int mr_dstpackitems(unsigned peer, char *store, char *slimit, size_t *memsize)
{
    int rtn = SUCCESS;
    char *sp;
    sp = store;

    //unsigned *debug;
    //debug = (unsigned *)store;

    *memsize = 0;
    el_t *item = (el_t *)uConsumers[peer]->q.fwd;

    while (item)
    {
        size_t size;
        if (!mr_ops->mr_packditem(item, &sp, slimit, &size))
        {
            // store is full!
            rtn = FAIL;
            break;
        }

        *memsize += (size_t) size;

        /*//const char  Msg1[] = "-%lu-";
        const char  Msg1[] = "-%x.%x.%x.%s-";
        snprintf( buf, bufs, Msg1, *(debug++), *(debug++), *(debug++), (char *)(debug++) );
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
            }*/

        char *oldsp = sp;
        sp = (char*)em_align((size_t) sp);

        // Add in any additional padding added by alignment
        *memsize += (size_t)sp - (size_t)oldsp;
        //debug = sp;
        el_t *olditem = item;
        item = item->fwd;

        if (!el_remove(olditem))
        {
            const char  Msg2[] = "ERR%p.%s-";
            snprintf( buf, bufs, Msg2, olditem, (char *)((word_t *)olditem + 1) );
            if (!msg_append_str(msg_channel_core, buf))
            {
                msg_reset(msg_channel_core);
                msg_append_str(msg_channel_core, buf);
            }
        }
    }

    WORSTCASESTACKSIZE();
    return rtn;
}

// TODO check these out perhaps they can be moved to mr specific?
// TODO change this to use msg_channel_store_prd??
void mr_dstprepcrmsg(unsigned peer,
                     char **store,
                     char **slimit)
{
    ec_command_t *storecmd = (ec_command_t *)inqueuestore[peer];

    storecmd->datlength = 0;

    *store = (char*)(storecmd + 1);
    *slimit = inqueuestore[peer] + sizeof(inqueuestore[peer]);
}

void mr_dstsendcrmsg(unsigned peer, size_t datlength)
{
    ec_command_t *storecmd = (ec_command_t *)inqueuestore[peer];

    storecmd->cmd = CMD_RDC;
    storecmd->datlength = datlength;
    storecmd->offset = (size_t)storecmd;

    storecmd->cmdstatus = cmd_gen_status(EC_SEQ, RND_NO_SYNC);
    ec_status_t qint = cmd_gen_status(EC_SEQ, RND_NO_SYNC);

    msg_reset(msg_uchannel_inqueue_prd);
    msg_uchannel_set_rank(msg_uchannel_inqueue_prd, peer);
    uc_write(msg_uchannel_inqueue_prd, &qint, sizeof(unsigned));
    
    // now interrupt the remote core
    msg_uchannel_int_peer(peer);
}

void mr_emitus(el_t *usitem)
{
    mr_ops->mr_reduce(usitem);
}

//
// Map Sort Distribute
//
int mapsortfillbucket()
{
    int rtn = SUCCESS;

    char *wkp = workspace->wkplaceptr;

    if (!mr_ops->mr_map(&wkp, workspace->wklimit, (el_t**)&workspace->dstnxtfree, workspace->dstlimit))
    {
        // no more space in cscache
        workspace->wkplaceptr = wkp;
        rtn = FAIL;
    }

    WORSTCASESTACKSIZE();
    return rtn;
}

int distributebucket()
{
    int rtn = SUCCESS;

    unsigned i;
    for (i=0; i<MAXRANK; i++)
    {
        /*if (uConsumers[i])
        {
            const char  Msg[] = "-%u.%s-";
            snprintf( buf, bufs, Msg, i, (char *)((word_t *)uConsumers[i]->q.fwd + 1) );
            if (!msg_append_str(msg_channel_core, buf))
            {
                msg_reset(msg_channel_core);
                msg_append_str(msg_channel_core, buf);
            }
            }*/

        if ( (i != my_rank) && !distributeremote(i))
        {
            rtn = FAIL;
        }

        if ( (i == my_rank) && !distributelocal())
        {
            rtn = FAIL;
        }
    }

    WORSTCASESTACKSIZE();
    return rtn;
}

int distributeremote(unsigned peer)
{
    int rtn = SUCCESS;

    if (uConsumers[peer] && freeecore(peer))
    {
        // Request distribution
        if (!mr_ops->mr_dstitems(peer, uConsumersPackSize[peer]))
        {
            rtn = FAIL;
        }
    } else {
        if (uConsumers[peer])
        {
            // eCore is busy
            rtn = FAIL;
        }
    }

    return rtn;
}

int distributelocal()
{
    int rtn = SUCCESS;

    if (!mr_ops->mr_mvuc)
    {
        rtn = FAIL;
    }
    
    if (rtn && uConsumers[my_rank])
    {
        el_t *ucitem = uConsumers[my_rank]->q.fwd;
        while (ucitem)
        {
            if (!mr_ops->mr_mvuc(ucitem))
            {
                rtn = FAIL;
                break;
            }

            el_t *olditem = ucitem;
            ucitem = ucitem->fwd;
            if (!el_remove(olditem))
            {
                const char  Msg1[] = "ERR%p.%s-";
                snprintf( buf, bufs, Msg1, olditem, (char *)((word_t *)olditem + 1) );
                if (!msg_append_str(msg_channel_core, buf))
                {
                    msg_reset(msg_channel_core);
                    msg_append_str(msg_channel_core, buf);
                }
            }
        }
    }

    return rtn;
}

//
// ISRs
//

void __attribute__((interrupt)) user_isr()
{
	// Enable the interrupt
	e_irq_mask(E_USER_INT, E_FALSE);
}

/*void __attribute__((interrupt)) dma0_isr()
{
	/ When the dma transfer finished tell the neighbour
	e_irq_set(row0, col0, E_USER_INT);

	/ Enable the interrupt
	e_irq_mask(E_DMA0_INT, E_FALSE);
}

void __attribute__((interrupt)) dma1_isr()
{
	/ When the dma transfer finished tell the neighbour
	e_irq_set(row1, col1, E_USER_INT);

	/ Enable the interrupt
	e_irq_mask(E_DMA1_INT, E_FALSE);
}
*/
