/************************************************************
 *
 * clcomms.h
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

#ifndef CLCOMMS
#define CLCOMMS

#define ENDPOINTLENGTH (125)
#define MAXLINELENGTH (512)

typedef enum {
    CLC_FAIL = 0,
    CLC_SUCCESS = !0,
} clc_rtn_code_t;

typedef struct {
    void *zmqcontext;
    void *zmqsocket;
    char endpoint[ENDPOINTLENGTH];
} clc_channel_t;

int clc_init_src(char *endpoint);
void clc_destroy_src();
size_t clc_get_line(char *Line, size_t maxsize);

#endif
