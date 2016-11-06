/*
  e_hello_world.c

  Copyright (C) 2012 Adapteva, Inc.
  Contributed by Yaniv Sapir <yaniv@adapteva.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program, see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>.
*/

// This is the DEVICE side of the Hello World example.
// The host may load this program to any eCore. When
// launched, the program queries the CoreID and prints
// a message identifying itself to the shared external
// memory buffer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_lib.h"

#define LENGTH (256)

char shared_outbuf[128*16] SECTION("shared_dram");
volatile int CPUActive SECTION(".data_bank3");
volatile int dma0int, dma1int SECTION(".data_bank3");
unsigned row0, col0, row1, col1 SECTION(".data_bank3");
unsigned dest SECTION(".data_bank2");

void user_isr();
void dma0_isr();
void dma1_isr();

int main(void) {
	CPUActive = 0;
	dma0int = 0;
	dma1int = 0;
	e_dma_desc_t dma_desc0;
	e_dma_desc_t dma_desc1;
	
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
	
	e_irq_mask(E_DMA0_INT, E_FALSE);
	e_irq_mask(E_DMA1_INT, E_FALSE);

	// Enable the global interrupt
	e_irq_global_mask(E_FALSE);
	
	CPUActive = 1;
	
	e_coreid_t coreid;

	// Who am I? Query the CoreID from hardware.
	coreid = e_get_coreid();

	// Calculate the outbuf for this core
	char * outbuf;
	outbuf = shared_outbuf + (e_group_config.core_row * 4 + e_group_config.core_col) * 128;

	// Identify the CPU that has come out of sleep
	sprintf(outbuf, "core 0x%03x ", coreid);

	e_coreid_t neighbourid;
	
	// Find next neighbour
	e_neighbor_id(E_NEXT_CORE, E_ROW_WRAP, &row0, &col0);
	neighbourid = e_coreid_from_coords(row0, col0);

	volatile int NeighbourActive;

	// Read the neighbours flag
	e_read(&e_group_config, (void *)&NeighbourActive, row0, col0, (void *)&CPUActive, sizeof(int));

	if (1 != NeighbourActive)
	{
		// DMA dest to the neighbour
		unsigned number;
		unsigned *dst;
		number = dest + 1;

		// Address of neighbouring buffer
		dst = (unsigned*) e_get_global_address(row0, col0, &dest);

		// Create the dma descriptor
		unsigned stride_i_src, stride_i_dst, count_i, count_o, stride_o_src, stride_o_dst;

		stride_i_src = 4;
		stride_i_dst = 4;
		count_i = 1;
		count_o = number;
		stride_o_src = 4;
		stride_o_dst = 4;
		e_dma_set_desc(
			E_DMA_0,
			(E_DMA_ENABLE|E_DMA_MASTER|E_DMA_WORD|E_DMA_IRQEN),
			0x0000,		// next desc
			stride_i_src, stride_i_dst,
			count_i, count_o,
			stride_o_src, stride_o_dst,
			(void *)&dest,(void *)dst,
			&dma_desc0);

		// calculation takes time so do it now
		char* outb;
		outb = outbuf + strlen(outbuf);
		
		// start the dma transfer
		e_dma_start(&dma_desc0, E_DMA_0);

		// print out progress, small transfer will already be finished
		sprintf(outb, "0x%03x %d %d start ", neighbourid, dma0int, dma1int);
	}
	else
	{
		sprintf(outbuf + strlen(outbuf), "0x%03x done  ", neighbourid);
	}

	e_neighbor_id(E_NEXT_CORE, E_COL_WRAP, &row1, &col1);
	neighbourid = e_coreid_from_coords(row1, col1);

	// Read the neighbours flag
	e_read(&e_group_config, (void *)&NeighbourActive, row1, col1, (void *)&CPUActive, sizeof(int));

	if (1 != NeighbourActive)
	{
		// DMA dest to the neighbour
		unsigned number;
		unsigned *dst;
		number = dest + 1;

		// Address of neighbouring buffer
		dst = (unsigned*) e_get_global_address(row1, col1, &dest);

		// Create the dma descriptor
		unsigned stride_i_src, stride_i_dst, count_i, count_o, stride_o_src, stride_o_dst;

		stride_i_src = 4;
		stride_i_dst = 4;
		count_i = 1;
		count_o = number;
		stride_o_src = 4;
		stride_o_dst = 4;
		e_dma_set_desc(
			E_DMA_1,
			(E_DMA_ENABLE|E_DMA_MASTER|E_DMA_WORD|E_DMA_IRQEN),
			0x0000,		// next desc
			stride_i_src, stride_i_dst,
			count_i, count_o,
			stride_o_src, stride_o_dst,
			(void *)&dest,(void *)dst,
			&dma_desc1);

		// calculation takes time so do it now
		char* outb;
		outb = outbuf + strlen(outbuf);

		// start the dma transfer
		e_dma_start(&dma_desc1, E_DMA_1);

		// print out progress, small transfer will already be finished
		sprintf(outb, "0x%03x %d %d start ", neighbourid, dma0int, dma1int);
	}
	else
	{
		sprintf(outbuf + strlen(outbuf), "0x%03x done  ", neighbourid);
	}

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
	dma0int = 1;
	e_irq_set(row0, col0, E_USER_INT);
	
	// Enable the interrupt
	e_irq_mask(E_DMA0_INT, E_FALSE);
}

void __attribute__((interrupt)) dma1_isr()
{
	// When the dma transfer finished tell the neighbour
	dma1int = 1;
	e_irq_set(row1, col1, E_USER_INT);
	
	// Enable the interrupt
	e_irq_mask(E_DMA1_INT, E_FALSE);
}
