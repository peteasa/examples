/************************************************************
 *
 * ebase.h
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
 * lfsr113_Bits under separate copyright as follows:
 *
 * published by Pierre L'Ecuyer 1999
 * http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme2.ps
 *
 *************************************************************/

#ifndef EBASE
#define EBASE

#include <stddef.h> // for offsetof
#include <stdlib.h>

// see http://www.linuxjournal.com/files/linuxjournal.com/linuxjournal/articles/067/6717/6717s2.html
#define container_of(ptr, type, member)         \
    ({\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);\
        (type *)( (char *)__mptr - offsetof(type,member) );\
    })

typedef enum {
    FAIL = 0,
    SUCCESS = !0,
} rtn_code_t;

struct el_t;
typedef struct el_t {
    struct el_t *head;
    struct el_t *fwd;
    struct el_t *bck;
} el_t;

// memory alignment
size_t em_align(const size_t n);

// linked list handlers
void el_init(el_t *item, el_t *head);
int el_insert(el_t *item, el_t *pos);
int el_remove(el_t *item);
int el_move_to(el_t *item, el_t *queue);
int el_can_join(el_t *item1, el_t *item2);

// Generic helper
extern unsigned int lfsr113_Bits(void);

#endif
