/*
  e_memory.c (based on e_hello_world)

  Copyright (c) 2015-2016 Peter Saunderson <peteasa@gmail.com>

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

#define ELINK_BASE	0x81000000
#define E_MAILBOXLO	0xF0730

#define USE_DMA 1
#ifdef USE_DMA
#define e_memcopy(dst, src, size) e_dma_copy(dst, src, size)
#else
#define e_memcopy(dst, src, size) memcpy(dst, src, size)
#endif

#define _BuffSize   (0x2000)
#define _NoMessages (0)
#define _LocalBuffOffset (0x5000)
//following clears the test buffer! so instead use hard coded value
//char shared[_BuffSize] SECTION("shared_dram");
char *shared = (char*)0x8f005000;
char *local = (char*)0x5000;

void checkandupdate(int count, void *message, int size, int * location, int *failures, char * errorval, char * check, char *val);

int main(void)
{
	int count;
	int *mailbox = (int *)(ELINK_BASE + E_MAILBOXLO);
	char sharedval = 0;
	char localval = 0;
	char sharedcheck = 0xff; //_BuffSize - 1;
	char localcheck = sharedcheck;
	int localfailures = 0;
	int sharedfailures = 0;
	char localfailure = 0;
	char sharedfailure = 0;
	int locallocation = 0;
	int sharedlocation = 0;

	long long message; 

	char * dst;

	for (count=0; count<_BuffSize/8; count++)
	{
		// Read/Write from/to local memory
		dst = (char*)(_LocalBuffOffset + count*sizeof(message));
		e_read(&e_group_config, &message, 0,0, dst, sizeof(message));

		// Limit the number of samples sent to the mailbox!
		if (_NoMessages>count) e_memcopy(mailbox, &message, sizeof(message));
	  
		checkandupdate(count, (void*)&message, sizeof(message), &localfailures, &locallocation, &localfailure, &localcheck, &localval);

		e_write(&e_group_config, &message, 0,0, dst, sizeof(message));
	  
		// Read/Write from/to shared memory
		dst = (char*)(shared + count*sizeof(message));
		e_memcopy((void *)&message, (void *)dst, sizeof(message));

		// Limit the number of samples sent to the mailbox!
		if (_NoMessages>count) e_memcopy(mailbox, &message, sizeof(message));
	  
		checkandupdate(count, (void*)&message, sizeof(message), &sharedfailures, &sharedlocation, &sharedfailure, &sharedcheck, &sharedval);
	  
		// Write to shared memory
		e_memcopy((void *)dst, (void *)&message, sizeof(message));
	}

	//message = 0;
	//message = 0xfedcba9876543210;
	//message = (long long)dst; - 0xffffffff8f000ff8

	// message = (long long)&message; - 0x7fc0
	// message = (long long)&shared - 0xffffffff8f000000
	// message = (long long)&shared[_BuffSize];- 0xffffffff8f001000
  
	for (count = 0; count<_NoMessages; count++)
	{
		e_memcopy(mailbox, &message, sizeof(message));
		//message++;
	}

	// In last message report the number of read failures
	message = (long long)sharedlocation;
	message = message << 16;
	////message += (long long)sharedfailure;
	////message = message << 16;
	message += (long long)sharedfailures;
	message = message << 16;
	message += (long long)locallocation;
	message = message << 16;
	message += (long long)localfailures;
	e_memcopy(mailbox, &message, sizeof(message));
}

void checkandupdate(int count, void *message, int size, int * failures, int * location, char * errorval, char * check, char * val)
{
	int index;
	char *pos;
	
	pos = (char*) message;
	for (index=0; index<size; index++)
	{
		if (*check != *pos)
		{
			(*failures)++;
			if (0xff==*failures)
			{
				*location = count * size + index;
				//*errorval = *pos;
				*errorval = *check;
			}
		}

		// update with write value
		*pos = *val;
		pos++;
		(*val)++;
		(*check)--;
	  }
}
