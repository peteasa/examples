/************************************************************
 *
 * dma.c
 * 
 * An application that runs the Map Reduce algorithm to count
 * words in a book
 *
 * Copyright (c) 2016 Peter Saunderson <peteasa@gmail.com>
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

typedef enum {
    FAIL = 0,
    SUCCESS,
} rtn_code_t;

typedef enum {
    C_UNKNOWN = 0,
    C_IDLE,
    C_RUNNING,
    C_ENDED,
} core_status_t;

typedef struct {
    size_t initialoffset;
    size_t offset;
    unsigned row;
    unsigned col;
    e_memseg_t emem;
} msg_channel_t;

int msg_channel_init(msg_channel_t *channel, const char* name, const unsigned row, const unsigned col, const size_t offset);
int msg_reset(msg_channel_t *channel);
int msg_append(msg_channel_t *channel, const void *src, const size_t n);
int msg_append_str(msg_channel_t *channel, const void *src);

// The linker labels of interest
extern int end, __stack, __heap_start, __heap_end, __clz_tab;

#define ENDDRAM1 (0x8fffffff)
#define STARTDRAM0 (0x8E000000)
#define STARTFREEDRAM ( (void*)(&__clz_tab + 0x124) > (void*)STARTDRAM0 ? (void*)&__clz_tab + 0x124 : (void*)STARTDRAM0 )
#define BUILDID (&__clz_tab + 0x100)

#define STARTFREELOCALRAM ((char*) &end )
#define APPROXSTACKSTART ( (void*)(stackpointer - 0x10) > (void*)STARTFREELOCALRAM ? (void*)(stackpointer - 0x10) : (void*)STARTFREELOCALRAM )

register char * const stackpointer asm("sp");

// Status: visible to all cores
volatile core_status_t CoreStatus;

// DMA: row and column of the neigbour used in the isr
unsigned row0, col0, row1, col1;

// DMA: distance from origin: visible to all cores
unsigned dest SECTION(".data_bank2");

void user_isr();
void dma0_isr();
void dma1_isr();

int main(void) {
    //int rtn;
    const char	ShmName[] = "econsumer";
    const char      Msg1[] = "core 0x%03x %d ";
    const char      Msg2[] = "0x%03x %d %d %d start ";
    const char      Msg3[] = "0x%03x %d done  ";
    char buf[256] = { 0 };
    e_coreid_t	coreid;
    msg_channel_t msg_channel;
    unsigned        my_row;
    unsigned        my_col;

	CoreStatus = C_IDLE;
    //rtn = EXIT_SUCCESS;
    dest = 0;
	e_dma_desc_t dma_desc0;
	e_dma_desc_t dma_desc1;

    // Who am I? Query the CoreID from hardware.
	coreid = e_get_coreid();
	e_coords_from_coreid(coreid, &my_row, &my_col);

    if (!msg_channel_init(&msg_channel,
                          ShmName,
                          my_row,
                          my_col,
                          (my_row * 4 + my_col) * 128) )
    {
        CoreStatus = C_ENDED;
        return EXIT_FAILURE;
    }

	e_irq_attach(E_USER_INT, user_isr);
	e_irq_attach(E_DMA0_INT, dma0_isr);
	e_irq_attach(E_DMA1_INT, dma1_isr);
	e_irq_mask(E_USER_INT, E_FALSE);
	e_irq_mask(E_DMA0_INT, E_FALSE);
	e_irq_mask(E_DMA1_INT, E_FALSE);

	// Enable the global interrupt
	e_irq_global_mask(E_FALSE);

	// Now put the CPU to sleep till an interrupt occurs
	__asm__ __volatile__ ("idle");
	CoreStatus = C_RUNNING;

	e_irq_mask(E_DMA0_INT, E_FALSE);
	e_irq_mask(E_DMA1_INT, E_FALSE);

	// Enable the global interrupt
	e_irq_global_mask(E_FALSE);

    // Identify the CPU that has come out of sleep
    snprintf(buf, sizeof(buf), Msg1, coreid, dest);
    if (!msg_append_str(&msg_channel, buf))
    {
        // Shared memory region is too small for the message
        return EXIT_FAILURE;
    }

	e_coreid_t neighbourid;
    volatile core_status_t neighbourstatus;

	// Find next neighbour
	e_neighbor_id(E_NEXT_CORE, E_ROW_WRAP, &row0, &col0);
	neighbourid = e_coreid_from_coords(row0, col0);

	// Read the neighbours flag
	e_read(&e_group_config, (void *)&neighbourstatus, row0, col0, (void *)&CoreStatus, sizeof(int));

	if (C_IDLE == neighbourstatus)
	{
		// DMA dest to the neighbour
		unsigned *dst;
        unsigned number;
        number = dest+1;

		// Address of neighbouring buffer
		dst = (unsigned*) e_get_global_address(row0, col0, (void*)&dest);

		// Create the dma descriptor
		unsigned stride_i_src, stride_i_dst, count_i, count_o, stride_o_src, stride_o_dst;

		stride_i_src = 4; // each txfer is 4 bytes
		stride_i_dst = 4;
		count_i = 1; // only 1 txfer
		count_o = 1;
		stride_o_src = 1; // contiguous txfers
		stride_o_dst = 1;
		e_dma_set_desc(
			E_DMA_0,
			(E_DMA_ENABLE|E_DMA_MASTER|E_DMA_WORD|E_DMA_IRQEN),
			0x0000,		// next desc
			stride_i_src, stride_i_dst,
			count_i, count_o,
			stride_o_src, stride_o_dst,
			(void *)&number,(void *)dst,
			&dma_desc0);

		// start the dma transfer
		e_dma_start(&dma_desc0, E_DMA_0);

        // report starting neighbour id
        snprintf(buf, sizeof(buf), Msg2, neighbourid, e_dma_busy(E_DMA_0), e_dma_busy(E_DMA_1), dest);
        if (!msg_append_str(&msg_channel, buf))
        {
            // Shared memory region is too small for the message
            CoreStatus = C_ENDED;
            return EXIT_FAILURE;
        }
	}
	else
	{
        // report neighbour already finished
        snprintf(buf, sizeof(buf), Msg3, neighbourid, dest);
        if (!msg_append_str(&msg_channel, buf))
        {
            // Shared memory region is too small for the message
            CoreStatus = C_ENDED;
            return EXIT_FAILURE;
        }
	}

	e_neighbor_id(E_NEXT_CORE, E_COL_WRAP, &row1, &col1);
	neighbourid = e_coreid_from_coords(row1, col1);

	// Read the neighbours flag
	e_read(&e_group_config, (void *)&neighbourstatus, row1, col1, (void *)&CoreStatus, sizeof(int));

	if (C_IDLE == neighbourstatus)
	{
		// DMA dest to the neighbour
		unsigned number;
		unsigned *dst;
		number = dest+1;

		// Address of neighbouring buffer
		dst = (unsigned*) e_get_global_address(row1, col1, (void*)&dest);

		// Create the dma descriptor
		unsigned stride_i_src, stride_i_dst, count_i, count_o, stride_o_src, stride_o_dst;

		stride_i_src = 4;
		stride_i_dst = 4;
		count_i = 1;
		count_o = 1;
		stride_o_src = 1;
		stride_o_dst = 1;
		e_dma_set_desc(
			E_DMA_1,
			(E_DMA_ENABLE|E_DMA_MASTER|E_DMA_WORD|E_DMA_IRQEN),
			0x0000,		// next desc
			stride_i_src, stride_i_dst,
			count_i, count_o,
			stride_o_src, stride_o_dst,
			(void *)&number,(void *)dst,
			&dma_desc1);

		// start the dma transfer
		e_dma_start(&dma_desc1, E_DMA_1);

        // report starting neighbour id
        snprintf(buf, sizeof(buf), Msg2, neighbourid, e_dma_busy(E_DMA_0), e_dma_busy(E_DMA_1), dest);
        if (!msg_append_str(&msg_channel, buf))
        {
            // Shared memory region is too small for the message
            CoreStatus = C_ENDED;
            return EXIT_FAILURE;
        }
	}
	else
	{
        // report neighbour already finished
        snprintf(buf, sizeof(buf), Msg3, neighbourid, dest);
        if (!msg_append_str(&msg_channel, buf))
        {
            // Shared memory region is too small for the message
            CoreStatus = C_ENDED;
            return EXIT_FAILURE;
        }
	}

    CoreStatus = C_ENDED;
	return EXIT_SUCCESS;
}

void __attribute__((interrupt)) user_isr()
{
	// Enable the interrupt
	e_irq_mask(E_USER_INT, E_FALSE);
}

void __attribute__((interrupt)) dma0_isr()
{
	// When the dma transfer finished tell the neighbour
	e_irq_set(row0, col0, E_USER_INT);
	
	// Enable the interrupt
	e_irq_mask(E_DMA0_INT, E_FALSE);
}

void __attribute__((interrupt)) dma1_isr()
{
	// When the dma transfer finished tell the neighbour
	e_irq_set(row1, col1, E_USER_INT);
	
	// Enable the interrupt
	e_irq_mask(E_DMA1_INT, E_FALSE);
}

int msg_channel_init(msg_channel_t *channel, const char* name, const unsigned row, const unsigned col, const size_t offset)
{
    if (E_OK != e_shm_attach(&channel->emem, name) ) {
		return FAIL;
	}

    channel->initialoffset = offset;
    channel->row = row;
    channel->col = col;
    msg_reset(channel);

    return SUCCESS;
}

int msg_reset(msg_channel_t *channel)
{
    channel->offset = channel->initialoffset;
    return SUCCESS;
}

int msg_append(msg_channel_t *channel, const void *src, const size_t n)
{
    if ( channel->emem.size >= channel->offset + n )
    {
        // Write the message (including the null terminating
        // character) to shared memory
        e_write((void*)&channel->emem, src, channel->row, channel->col, (void*)channel->offset, n);

        channel->offset += n;
    } else {
        // Shared memory region is too small for the message
        return FAIL;
    }

    return SUCCESS;
}

int msg_append_str(msg_channel_t *channel, const void *src)
{

    if (!msg_append(channel, src, strlen(src)+1) )
    {
        // Shared memory region is too small for the message
        return FAIL;
    }

    // Remove the trailing null byte from the offset calculation
    channel->offset--;

    return SUCCESS;
}
