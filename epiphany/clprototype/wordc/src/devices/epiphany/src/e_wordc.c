/************************************************************
 *
 * e_wordc.c
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

#include <stdio.h>        // for snprintf
#include <string.h>
#include <libecomms.h>
#include "e_clcore.h"

// Following could be allocated / freed
msg_channel_t _ecrmsg_channel_mem;
msg_channel_t _eclmsg_uecomms_prod;
msg_channel_t _ecrmsg_uecomms_cons;
msg_channel_t *ecrmsg_channel_mem = &_ecrmsg_channel_mem;
msg_channel_t *eclmsg_uecomms_prod = &_eclmsg_uecomms_prod;
msg_channel_t *ecrmsg_uecomms_cons = &_ecrmsg_uecomms_cons;

cl_mr_ops_t wc_cl;

// Consider getting this via an api call
unsigned my_rank;

// Word store
typedef enum {
    DONT = 0,
    CMPT = !0,
} compute_t;
typedef struct {
    el_t q;
    unsigned count;
    unsigned short size;
    unsigned char hash;
} word_t;
char wordstore[1024];
char *wslimit;
word_t *wsnxtfree;
word_t wswords;

// workspace
#define WSSIZE (512)
char wkspace[WSSIZE];
char wkdstspc[2*WSSIZE];

// dma descriptors
e_dma_desc_t dma_desc[2];

// comms memory for ecmsg_uecomms_* channels
char ecorecomms[2*WSSIZE];

//
// Foward declarations
//
int wcinit(void);
void wcfinalize(void);
void wcevstrt(void);
void wcevend(void);
int wclineunpack(const ec_command_t *wkcmd, char **wkp, const char *wklimit, cl_wk_hdr_t **unpkd);
int wcdo(unsigned peer, cl_wk_hdr_t *wkspace, const ec_command_t *wkcmd);
int unpacktwosize_t(const ec_command_t *wkcmd, char **wkp, const char *wklimit, msg_channel_t *channel, cl_wk_hdr_t **unpkd);

//
// Word counting Map Sort Reduce Pack
//
unsigned wcallocate(el_t *item, unsigned maxid);
int wcmap(char **wkp, char *wklimit, el_t **dstnxtfree, char *dstlimit);
int wcdstitems(unsigned peer, size_t size);
int wcpackditem(el_t *item, char **msp, char* msplimit, size_t *size);
int wcunpackcr(unsigned peer, const ec_command_t *wkcmd, char **wkp, const char *wklimit, cl_wk_hdr_t **unpkd);
int wcunpackwk(char **wkp, char *wklimit);
int wcmvuc(el_t *ucitem);
void wcreduce(el_t *usitem);
int wcrprtpack();//char **msp, char* msplimit, size_t *size);
int wcinitwswords();

//
// Word helpers
//
void wcmvwordtows(word_t *newwrd);
int wcfindword(word_t *newwrd, word_t **wswrd);
int wcpartofword(char *p);
void wcnextword(char **p, char *limit);
int wcmvword(char **wkp, char *wklimit, el_t **dstnxtfreeq, char *dstlimit, compute_t cmpt);
int wcmvchar(char **p, char *limit, char **current, char *climit, size_t *size, unsigned char *hash, compute_t cmpt);

int main(void)
{
    int rtn = SUCCESS;

    wc_cl.mr_init = wcinit;
    wc_cl.mr_finalize = wcfinalize;
    wc_cl.mr_unpackhst = wclineunpack;

    wc_cl.mr_map = wcmap;
    wc_cl.mr_allocate = wcallocate;
    wc_cl.mr_dstitems = wcdstitems;
    wc_cl.mr_packditem = wcpackditem;
    wc_cl.mr_unpackcr = wcunpackcr;
    wc_cl.mr_unpackwk = wcunpackwk;
    wc_cl.mr_mvuc = wcmvuc;
    wc_cl.mr_reduce = wcreduce;
    wc_cl.mr_reportpack = wcrprtpack;

    wc_cl.mr_evstrt = wcevstrt;
    wc_cl.mr_evend = wcevend;
    wc_cl.mr_do = wcdo;

    // Initial the clcore
    rtn = e_clcoreinit(&wc_cl, &my_rank);

    if (rtn && e_clcorerun())
    {
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

int wcinit(void)
{
    int rtn = SUCCESS;

    const char ShmMemName[] = "ecmemts";
    if (rtn && !ec_init_mem(ecrmsg_channel_mem, ShmMemName))
    {
        rtn = FAIL;
    }

    if (rtn && !ec_init_mem_uchannel(ecrmsg_uecomms_cons, &e_group_config, (size_t)ecorecomms, sizeof(ecorecomms)))
    {
        rtn = FAIL;
    }

    if (rtn && !ecl_alloc_local_mem_uchannel(eclmsg_uecomms_prod, (size_t)ecorecomms, sizeof(ecorecomms)))
    {
        rtn = FAIL;
    }

    // Init the results store
    wcinitwswords();

    return SUCCESS;
}

void wcfinalize(void)
{
    const char  Msg[] = "mafinalize ";
    snprintf( buf, bufs, Msg);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
    }
}

void wcevstrt(void)
{
}

void wcevend(void)
{
    char *pos = (char *)(&wswords + 2);
    const char  Msg[] = "u%u%s-";
    snprintf( buf, bufs, Msg, wswords.count, pos);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
    }
}

int wclineunpack(const ec_command_t *wkcmd, char **wkp, const char *wklimit, cl_wk_hdr_t **unpkd)
{
    size_t *memsize = (size_t *)*wkp;
    size_t *memoffset = memsize + 1;
    size_t offset = *memoffset;

    int rtn = unpacktwosize_t(wkcmd, wkp, wklimit, ecrmsg_channel_mem, unpkd);

    if (rtn)
    {
        // mark memory free
        ec_mark_mem_free(ecrmsg_channel_mem, offset);
    }

    return rtn;
}

// Generic command handling
int wcdo(unsigned peer, cl_wk_hdr_t *wkspace, const ec_command_t *wkcmd)
{
    // Now print this to msg channel core
    char *msg = (char *)(wkcmd + 1);

    //const char  Msg[] = "oops 0x%lu ";
    //snprintf( buf, bufs, Msg, wkcmd->datlength);
    if (!msg_append_str(msg_channel_core, msg))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, msg);
    }

    e_clsetwkdone(wkspace);

    return SUCCESS;
}

int unpacktwosize_t(const ec_command_t *wkcmd, char **wkp, const char *wklimit, msg_channel_t *rchannel, cl_wk_hdr_t **unpkd)
{
    int rtn = SUCCESS;

    // TODO for now we only unpack one set of values
    size_t *memsize = (size_t *)*wkp;
    size_t *memoffset = memsize + 1;
    *wkp = (char *)(memoffset + 1);
    if (*wkp > wklimit)
    {
        rtn = FAIL;
    }

    /*const char  Msg[] = "ul%lxo%lx.";
    snprintf(buf, bufs, Msg, *memsize, *memoffset);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    // First initial the workspace
    if (rtn && !e_clwkinit(wkcmd, (cl_wk_hdr_t *)wkspace, sizeof(wkspace), (dst_list_t *)wkdstspc, sizeof(wkdstspc)) )
    {
        rtn = FAIL;
    }

    *unpkd = (cl_wk_hdr_t *)wkspace;

    // Now dma the data into the new workspace
    if (rtn)
    {
        size_t dst = e_wkdataphyaddr((cl_wk_hdr_t *)wkspace);
        size_t src = ec_phyaddr(rchannel, *memoffset);
        e_cldmadesc(src, dst, dma_desc, em_align(*memsize), 0);
        // Test multi descriptors and row / col translation
        /*e_clmdmadesc(src, dst, dma_desc, 2,1,em_align(*memsize)/8,E_DMA_CHAIN);
        dst += em_align(*memsize)/2;
        src += 4;
        e_clmdmadesc(src, dst, &dma_desc[1], 2,1,em_align(*memsize)/8,0);*/
        rtn = e_cldmadread((cl_wk_hdr_t *)wkspace, dma_desc, *memsize);
    }

    return rtn;
}

//
// Word counting Map Sort Reduce Pack
//
unsigned wcallocate(el_t *item, unsigned maxid)
{
    word_t *word = (word_t *)item;
    return word->hash % maxid;
}

int wcmap(char **wkp, char *wklimit, el_t **dstnxtfree, char *dstlimit)
{
    int rtn = SUCCESS;
    while (*wkp < wklimit)
    {
        word_t *word = (word_t *)*dstnxtfree;
        if (wcpartofword(*wkp))
        {
            if (!wcmvword(wkp, wklimit, dstnxtfree, dstlimit, CMPT))
            {
                // no more space in cscache
                rtn = FAIL;
                break;
            }

            size_t packsize = em_align((size_t)word->size + 1) + // aligned word size including trailing zero
                em_align(sizeof(unsigned) +                  // wcount
                         sizeof(unsigned) +                  // wsize
                         sizeof(unsigned char));             // whash
            mr_mapemit((el_t *)word, packsize);
        } else {
            wcnextword(wkp, wklimit);
        }
    }

    return rtn;
}

int wcdstitems(unsigned peer, size_t size)
{
    int rtn = SUCCESS;
    size_t memoffset;
    size_t memsize;

    if (rtn && !ecl_free_mem_space(eclmsg_uecomms_prod, size))
    {
        rtn = FAIL;
    }

    //allocate local memory with size
    if (rtn && !ecl_alloc(eclmsg_uecomms_prod, size, &memoffset))
    {
        rtn = FAIL;
    }

    //ec_header_t *curh;
    //curh = container_of((unsigned *)(memoffset), ec_header_t, data);
    //const char  Msg[] = "i%xl%lxo%lxo%lxh%pf%pb%py%x";
    //snprintf( buf, bufs, Msg, rtn, size, memoffset, ec_get_channel_curr_pos(eclmsg_uecomms_prod), curh->hdr.hd.head, curh->hdr.hd.fwd, curh->hdr.hd.bck, curh->hdr.hd.busy);

    if (rtn && mr_dstpackitems(peer, (char *)memoffset, (char *)(memoffset + size), &memsize))
    {
        size_t *wsize;
        size_t *woffset;
        char *slimit;
        mr_dstprepcrmsg(peer, (char **)&wsize, &slimit);

        if ((char *)(wsize + 2) <= slimit)
        {
            *wsize = memsize;
            woffset = (wsize + 1);
            *woffset = memoffset;
            mr_dstsendcrmsg(peer, 2 * sizeof(size_t));
        } else {
            // TODO consider some more detailed failure information!
            rtn = FAIL;
        }
    } else {
        rtn = FAIL;
    }

    /*const char  Msg[] = "i%dl%lxo%lx.";
    snprintf( buf, bufs, Msg, rtn, memsize, memoffset);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    return rtn;
}

//      perhaps implement suitable helpers in msg_??
int wcpackditem(el_t *ucitem, char **msp, char* msplimit, size_t *size)
{
    int rtn = SUCCESS;

    word_t *word = (word_t *)ucitem;
    char *pos = (char *)(word + 1);
    char *climit = pos + (size_t)word->size;
    char *srcstart = pos;
    char *headstart = *msp;

    unsigned *wcount = (unsigned *)*msp;
    *msp = (char *)(wcount + 1);
    unsigned *wsize = (unsigned *)*msp;
    *msp = (char *)(wsize + 1);
    unsigned char *whash = (unsigned char *)*msp;
    *msp = (char *)(whash + 1);
    char *oldmsp = *msp;
    *msp = (char *)em_align((size_t)*msp);
    size_t hsize = (size_t)*msp - (size_t)oldmsp + sizeof(unsigned) + sizeof(unsigned) + sizeof(unsigned char);

    char *dststart = *msp;

    if (*msp < msplimit)
    {
        *wcount = word->count;
        *wsize = (unsigned) word->size;
        *whash = word->hash;
    } else {
        rtn = FAIL;
    }

    unsigned char h;
    if (rtn && wcmvchar(&pos, climit, msp, msplimit, size, &h, DONT))
    {
        // Add the size of the header added above
        *size = (size_t)word->size + hsize;
        if (*msp < msplimit)
        {
            **msp = 0;
            (*msp)++;
            (*size)++;
        } else {
            // Restore state
            size_t tmp;
            *msp = dststart;
            wcmvchar(&dststart, msplimit, &srcstart, climit, &tmp, &h, DONT);
            *size = 0;
            *msp = headstart;
            rtn = FAIL;
        }
    } else {
        // store is full!
        rtn = FAIL;
    }

    return rtn;
}

int wcunpackcr(unsigned peer, const ec_command_t *wkcmd, char **wkp, const char *wklimit, cl_wk_hdr_t **unpkd)
{
    msg_uchannel_set_rank(ecrmsg_uecomms_cons, peer);

    size_t *memsize = (size_t *)*wkp;
    size_t *memoffset = memsize + 1;
    size_t startdata = *memoffset;

    int rtn = unpacktwosize_t(wkcmd, wkp, wklimit, ecrmsg_uecomms_cons, unpkd);

    // mark memory free
    ec_mark_mem_free(ecrmsg_uecomms_cons, startdata);

    /*const char  Msg[] = "cr%d.";
    snprintf( buf, bufs, Msg, rtn);
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
        }*/

    return rtn;
}

int wcunpackwk(char **wkp, char *wklimit)
{
    int rtn = SUCCESS;

    while (*wkp < wklimit)
    {
        char *wkpstart = *wkp;
        word_t *newwrd = wsnxtfree;
        unsigned *wcount = (unsigned *)*wkp;
        *wkp = (char *)(wcount + 1);
        unsigned *wsize = (unsigned *)*wkp;
        *wkp = (char *)(wsize + 1);
        unsigned char *whash = (unsigned char *)*wkp;
        *wkp = (char *)(whash + 1);
        *wkp = (char *)em_align((size_t)*wkp);

        // TODO infuture also pick up the count and hash?

        if (*wkp >= wklimit)
        {
            // end of workspace
            break;
        }

        if (!wcpartofword(*wkp))
        {
            // corruption? move to the next word if possible!
            wcnextword(wkp, wklimit);
        }

        if ( wcmvword(wkp, wklimit, (el_t **)&wsnxtfree, wslimit, DONT) )
        {
            newwrd->count = *wcount;
            newwrd->size = (unsigned short)*wsize;
            newwrd->hash = *whash;
        } else {
            // no more space in wordstore restore location
            *wkp = wkpstart;
            rtn = FAIL;
            break;
        }

        mr_emitus((el_t *)newwrd);
        //wcmvwordtows(newwrd);

        /*const char  Msg[] = "-%u.%s-";
        snprintf( buf, bufs, Msg, wswords.count, (char *)(newwrd + 1));
        if (!msg_append_str(msg_channel_core, buf))
        {
            msg_reset(msg_channel_core);
            msg_append_str(msg_channel_core, buf);
        }
        */

        *wkp = (char *)em_align((size_t)*wkp);
    }

    return rtn;
}

int wcmvuc(el_t *ucitem)
{
    int rtn = SUCCESS;

    word_t *word = (word_t *)ucitem;
    word_t *newwrd = wsnxtfree;
    char *ip = (char *)(word + 1);
    char *iplimit = ip + word->size;

    if ( wcmvword(&ip, iplimit, (el_t **)&wsnxtfree, wslimit, DONT) )
    {
        newwrd->count = word->count;
        newwrd->size = word->size;
        newwrd->hash = word->hash;
    } else {
        // no more space in wordstore restore location
        rtn = FAIL;
    }

    if (rtn)
    {
        mr_emitus((el_t *)newwrd);
        //wcmvwordtows(newwrd);
    }

    /*const char  Msg[] = "-%u.%s-";
    snprintf( buf, bufs, Msg, wswords.count, (char *)(newwrd + 1));
    if (!msg_append_str(msg_channel_core, buf))
    {
        msg_reset(msg_channel_core);
        msg_append_str(msg_channel_core, buf);
    }
    */

    return rtn;
}

void wcreduce(el_t *usitem)
{
    word_t *newwrd = (word_t *)usitem;
    word_t *ws;
    // search list for this word
    if (wcfindword(newwrd, &ws))
    {
        ws->count += newwrd->count;
        if (ws->count > ((word_t *)ws->q.bck)->count)
        {
            if (ws->q.bck->bck)
            {
                // shuffle highest counts to the top of the list
                el_move_to((el_t *)ws, ws->q.bck->bck);
            }
        }

        wsnxtfree = newwrd;
    } else {
        // insert word into list
        el_init(&newwrd->q, &wswords.q);
        el_insert(&newwrd->q, &wswords.q);
        wswords.count++;
    }
}

int wcrprtpack()//char **msp, char* msplimit, size_t *size)
{
    int rtn = SUCCESS;

    return rtn;
}

int wcinitwswords()
{
    el_init((el_t*)&wswords, (el_t*)&wswords);
    wswords.size = 0;
    wswords.count = 0;

    wsnxtfree = (word_t *)wordstore;
    wslimit = wordstore + sizeof(wordstore);

    return SUCCESS;
}

//
// Word helpers
//
int wcfindword(word_t *newwrd, word_t **wswrd)
{
    int found = FAIL;
    el_t *w = wswords.q.fwd;

    while (w)
    {
        *wswrd = (word_t *)w;
        if ((*wswrd)->size == newwrd->size &&
            (*wswrd)->hash == newwrd->hash)
        {
            found = SUCCESS;
            char *wlimit = (char *)(*wswrd + 1) + (size_t)(*wswrd)->size;
            char *wsc = (char *)(*wswrd + 1);
            char *nwc = (char *)(newwrd + 1);
            while (wsc < wlimit)
            {
                if (*wsc != *nwc)
                {
                    found = FAIL;
                    break;
                }
                wsc++;
                nwc++;
            }

            if (found)
            {
                break;
            }
        }

        w = w->fwd;
    }

    return found;
}

int wcpartofword(char *p)
{
    return (*p >= '!') && (*p <= '~') && (*p !=' ');
}

void wcnextword(char **p, char *limit)
{
    while (*p < limit && wcpartofword(*p))
    {
        (*p)++;
    }

    while (*p < limit && !wcpartofword(*p))
    {
        (*p)++;
    }
}

int wcmvword(char **wkp, char *wklimit, el_t **dstnxtfree, char *dstlimit, compute_t cmpt)
{
    int rtn = SUCCESS;
    word_t *word = (word_t *)*dstnxtfree;
    char *pos = (char *)(word + 1);
    size_t size;
    char *srcstart = *wkp;
    char *dststart = pos;
    unsigned char hash;

    if (wcmvchar(wkp, wklimit, &pos, dstlimit, &size, &hash, cmpt))
    {
        if (pos < dstlimit)
        {
            *pos = 0;
            pos++;
            *dstnxtfree = (el_t *)em_align((size_t) pos);
            word->size = (unsigned short)size;
            word->count = 1;
            word->hash = hash;
        } else {
            // Restore State
            wcmvchar(&dststart, dstlimit, &srcstart, wklimit, &size, &hash, DONT);
            rtn = FAIL;
        }
    } else {
        rtn = FAIL;
    }

    return rtn;
}

int wcmvchar(char **src, char *slimit, char **dst, char *dlimit, size_t *size, unsigned char *hash, compute_t cmpt)
{
    int rtn = SUCCESS;
    char *srcstart = *src;
    char *dststart = *dst;
    *size = 0;
    *hash = 0;

    while (wcpartofword(*src))
    {
        if (*dst<dlimit && *src<slimit)
        {
            **dst = **src;
            if (cmpt)
            {
                *hash = *hash ^ (unsigned char)**src;
                **src = ' ';
            }
        } else {
            rtn = FAIL;
            break;
        }

        if (cmpt)
        {
            (*size)++;
        }

        (*src)++;
        (*dst)++;
    }

    if (!rtn)
    {
        // Restore state
        *src = srcstart;
        *dst = dststart;
        size_t s;
        unsigned char h;
        wcmvchar(&dststart, dlimit, &srcstart, slimit, &s, &h, DONT);
    }

    return rtn;
}
