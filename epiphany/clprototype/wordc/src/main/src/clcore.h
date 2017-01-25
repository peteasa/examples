/************************************************************
 *
 * clcore.h
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

#ifndef CLCORE
#define CLCORE

#include <stdlib.h>
#include <libecomms.h>

typedef size_t cl_ref;

typedef struct {
    int (*mr_init)(void);
    void (*mr_finalize)(void);
    int (*mr_evstrt)(void);   // called at the start of each cycle
    void (*mr_evend)(void);    // called at the end of each cycle
    int (*mr_packhst)(cl_ref *chan, void *mrref, size_t mrsize);      // called to add command data extension
} cl_mr_ops_t;

// Run the core
extern int clcoreinit(cl_mr_ops_t *ops, e_epiphany_t **dev, char *executable);
extern void clcorefinalize(void);
extern int clcorerun(void);

// Send a command
extern int clcorewrite(cl_ref *pdoffset, char *mrdata, size_t mrsize);
extern int clcorepackhst(void *mrref, size_t mrsize, size_t *pdoffset);
extern void clcoresendcmd(ec_status_t cmdstatus, ec_command cmd, size_t pddatlength, size_t pdoffset);

//
// Debug
//
extern char *buf;
extern size_t bufs;
extern msg_channel_t *msg_channel_core;

#endif
