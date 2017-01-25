/************************************************************
 *
 * ebase.c
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

#include <stdlib.h>
#include <ebase.h>

size_t em_align(const size_t n)
{
    // Ensure that there are no alignment issues
    size_t align = n % 4;
    if (align)
    {
        align = n + 4 - align;
    } else {
        // is aligned
        align = n;
    }

    return align;
}

void el_init(el_t *item, el_t *head)
{
    item->fwd = NULL;
    item->bck = NULL;
    item->head = head;
}

int el_insert(el_t *item, el_t *pos)
{
    int rtn = FAIL;

    if ( el_can_join(pos, item) )
    {
        el_t *next = pos->fwd;
        item->fwd = next;
        item->bck = pos;

        // link the item to pos
        if (next)
        {
            next->bck = item;
        }
        pos->fwd = item;
        
        rtn = SUCCESS;
    }

    return rtn;
}

int el_remove(el_t *item)
{
    int rtn = FAIL;

    if (item && item->bck)
    {
        el_t *prev = item->bck;
        el_t *next = item->fwd;

        // unlink item
        prev->fwd = next;
        if (next)
        {
            next->bck = prev;
        }

        item->bck = NULL;
        item->fwd = NULL;

        rtn = SUCCESS;
    }

    return rtn;
}

int el_move_to(el_t *item, el_t *queue)
{
    int rtn = FAIL;
    if ( el_can_join(item, queue) &&
         item->bck )
    {
        el_t *prev = item->bck;
        el_t *next = item->fwd;
        el_t *qnxt = queue->fwd;

        if (queue->fwd != item)
        {
            // unlink item
            prev->fwd = next;
            if (next)
            {
                next->bck = prev;
            }

            item->fwd = qnxt;
            item->bck = queue;

            // link the item into the queue
            if (qnxt)
            {
                qnxt->bck = item;
            }

            queue->fwd = item;
        }
        
        rtn = SUCCESS;
    }

    return rtn;
}

int el_can_join(el_t *item1, el_t *item2)
{
    int rtn = SUCCESS;

    if (NULL == item1 ||
        NULL == item2 ||
        item1->head != item2->head)
    {
        rtn = FAIL;
    }

    return rtn;
}

unsigned int lfsr113_Bits(void)
{
    static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
    unsigned int b;
    b  = ((z1 << 6) ^ z1) >> 13;
    z1 = ((z1 & 4294967294U) << 18) ^ b;
    b  = ((z2 << 2) ^ z2) >> 27; 
    z2 = ((z2 & 4294967288U) << 2) ^ b;
    b  = ((z3 << 13) ^ z3) >> 21;
    z3 = ((z3 & 4294967280U) << 7) ^ b;
    b  = ((z4 << 3) ^ z4) >> 12;
    z4 = ((z4 & 4294967168U) << 13) ^ b;
    return (z1 ^ z2 ^ z3 ^ z4);
}

