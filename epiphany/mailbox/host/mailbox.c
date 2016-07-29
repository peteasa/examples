/************************************************************
 
  mailbox.c
 
  A test for the oh mailbox using the updated epiphany kernel driver
 
  Copyright (c) 2015-2016 Peter Saunderson <peteasa@gmail.com>

  Major modifications but based on hello_world.c:
 
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
 *************************************************************/

// This is the HOST side of the mailbox example.
// The program initializes the Epiphany system,
// randomly draws an eCore and then loads and launches
// the device program on that eCore. It then reads the
// shared external memory buffer for the core's output
// message.

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <string.h>
#include <unistd.h>	// for _SC_PAGESIZE
#include <sys/mman.h>	// for mmap
#include <fcntl.h>	// for O_RDWR
#include <errno.h>	// for errno
#include <e-hal.h>
#include <sys/ioctl.h>
//#include <misc/epiphany.h> // needed for ioctl numbers
#include "epiphany.h"

struct e_message {
	uint32_t from;
	uint32_t data;
} __attribute__((packed)) __attribute__((aligned(8)));

#define EPIPHANY_DEV "/dev/epiphany/elink0"

// Test message store
const unsigned ShmSize = 128;
const char ShmName[] = "hello_shm"; 

e_mem_t   mbuf;

typedef struct _MAILBOX_CONTROL
{
	int running;	// 1 if ready; otherwise not ready
	int devfd;	// handle for epiphany device driver
	int epollfd; 	// handle for blocking wait on notification
	int cancelfd;	// handle for cancel
	struct epoll_event *events;
} mailbox_control_t;
static mailbox_control_t mc;

// Epiphany data
static e_platform_t platform;

static e_epiphany_t dev;

// Test control stuff
#define SeqLen    32
typedef struct _TEST_CONTROL
{
	int cancelNow;  // TODO replace with a semaphore
	int keepalive;
} test_control_t;
static test_control_t tc;

// Prototypes
void *SleepThenCancel(void *ptr);
int RunTest(test_control_t *tc);
int InitialTest();
void CloseTest();
int InitialEpiphany();
void CloseEpiphany();
int InitialTestMessageStore();
void CloseTestMessageStore();
int InitialMailboxNotifier();
int OpenEpiphanyDevice();
int UpdateEpoll(int fd, int operation, uint32_t waitOnEvent);
int OpenCancelEventMonitor();
void CloseCancelEventMonitor();
void CancelEventMonitor();
int WaitForMailboxNotifier();
int e_mailbox_count();
int e_mailbox_read(struct e_message *msg, int flags);

int main(int argc, char *argv[])
{
	pthread_t definedTestTimeThread;
	int threadRtn, returns;

	returns = -1;
	tc.cancelNow = 0;
	tc.keepalive = 0;
	
	if (0 == InitialTest())
	{
		printf("main(): devfd %d, epollfd %d, cancelfd %d\n", mc.devfd, mc.epollfd, mc.cancelfd);
	
		// Create a thread to monitor the progress of the test
		threadRtn = pthread_create(&definedTestTimeThread, NULL, SleepThenCancel, (void *) &tc);

		// Run the test
		returns = RunTest(&tc);

		// If the test finishes early cancel the monitor progress
		if (!tc.cancelNow)
		{
			printf("main(): wait for SleepThenCancel thread to terminate\n");
			tc.cancelNow = 1;
		}

		// Wait for monitor thread to complete
		pthread_join(definedTestTimeThread, NULL);

		CloseTest();
	}
	
	return returns;	
}

int RunTest(test_control_t *tc)
{
	int returns;
	unsigned row, col, coreid, i, j;
	struct e_message msg = { 0, 0 };
 
	// Keep the test alive
	tc->keepalive++;

	row = 0;
	col = 0;
	for (i=0; i<SeqLen; i++)
	{
		char emesg[ShmSize];
		if (tc->cancelNow)
		{
			break;
		}

		// Visit each core
		unsigned lastCol = col;
		col = col % platform.cols;

		if (col < lastCol)
		{
			row++;
			unsigned lastRow = row;
			row = row % platform.rows;

			/*if (row < lastRow)
			{
				break;
			}*/
		}

		// Calculate the coreid
		coreid = (row + platform.row) * 64 + col + platform.col;

		// Open the single-core workgroup and reset the core, in
		// case a previous process is running. Note that we used
		// core coordinates relative to the workgroup.
		e_open(&dev, row, col, 1, 1);
		e_reset_group(&dev);

		// printf("RunTest(%d,%d): Load Core\n", row, col);
		
		// Load the device program onto the selected eCore
		// e_load_group core 0,0 group size 1,1 and wait
		e_return_stat_t result;
		result = e_load_group("e_mailbox.elf", &dev, 0, 0, 1, 1, E_FALSE);
		if (result != E_OK)
		{
			printf("RunTest(%d,%d): Error in e_load %i\n", row, col, result);
		}

		// printf("RunTest(%d,%d): Core loaded\n", row, col);

		// Keep the test alive
		tc->keepalive++;

		// e_start group core 0,0
		e_start(&dev, 0, 0);
		usleep(10000);

		// Wait for core program execution to finish, then
		// read message from shared buffer.
		// returns = WaitForMailboxNotifier();
		// Blocking wait for Epiphany core to send mailbox
		e_mailbox_read(&msg, 0);

		// Keep the test alive
		tc->keepalive++;

		// Read the core message
		e_read(&mbuf, 0, 0, 0x0, emesg, ShmSize);

		// Print the message
		printf("RunTest(): %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);
		printf("\"%s\"\n", emesg);

		unsigned count;
		count = e_mailbox_count() + 1;
		
		// Print mailbox message
		printf("%d messages from: 0x%08x data: 0x%08x\n", count--,
		       msg.from, msg.data);
 
		// Read the mailbox
		unsigned mbentries = 1;
		while (0 < count)
		{
			if (e_mailbox_read(&msg, 0)) {
				printf("ERROR: e_mailbox_read\n");
				break;
			}

			printf("message %d from: 0x%08x data: 0x%08x\n", mbentries++,
			       msg.from, msg.data);
			count = e_mailbox_count();
		}

		e_close(&dev);

		col++;
	}
	
	return 0;
}

void *SleepThenCancel( void *ptr )
{
	struct epoll_event event;
	
	test_control_t *tc = (test_control_t *)ptr;
	
	// printf ("\nSleepThenCancel(): Starting Test keepalive thread\n");
	
	int i;
	for (i=0; i<100; i++)
	{
		int oldkeepalive = tc->keepalive;
		usleep(4000000);
		if (tc->cancelNow)
		{
			break;
		}

		if (tc->keepalive == oldkeepalive)
		{
			printf ("\nINFO: SleepThenCancel(): Cancelling!\n");
			break;
		}
	}

	tc->cancelNow = 1;
	CancelEventMonitor();
}

int InitialTest()
{
	int returns;
	
	returns = InitialEpiphany();
	if (0 > returns)
	{
		printf("ERROR: InitialTest(): InitialEpiphany() failed with: %d!\n", returns);
		return returns;
	}

	returns = InitialTestMessageStore();
	if (0 > returns)
	{
		printf("ERROR: InitialTest(): InitialTestMessageStore() failed with: %d!\n", returns);

		// Tidy up
		CloseEpiphany();
		return returns;
	}

	returns = InitialMailboxNotifier();
	if (0 > returns)
	{
		printf("ERROR: InitialTest(): InitialTestMessageStore() failed with: %d!\n", returns);

		// Tidy up
		CloseTestMessageStore();
		CloseEpiphany();
		return returns;
	}
	
	return returns;
}

void CloseTest()
{
	CloseTestMessageStore();
	CloseEpiphany();
}

int InitialEpiphany()
{
	int returns;
	
	// initialize system, read platform params from
	// default HDF. Then, reset the platform and
	// get the actual system parameters.
	returns = e_init(NULL);
	if (0 > returns)
	{
		return returns;
	}

	returns = e_reset_system();
	if (0 > returns)
	{
		return returns;
	}

	returns = e_get_platform_info(&platform);
	if (0 > returns)
	{
		return returns;
	}
 
	printf("InitialEpiphany(): rows,cols: (%2d,%2d)\n", platform.rows, platform.cols);

	return returns;
}

void CloseEpiphany()
{
	e_finalize();
}

int InitialTestMessageStore()
{
	int rc;

	// Allocate a buffer in shared external memory
	// for message passing from eCore to host.
	rc = e_shm_alloc(&mbuf, ShmName, ShmSize);
	if (rc != E_OK)
	{
		rc = e_shm_attach(&mbuf, ShmName);
	}

	return rc;
}

void CloseTestMessageStore()
{
	e_shm_release(ShmName);
}

int InitialMailboxNotifier()
{
	int returns;
	
	mc.running = 0;

	// Open an epoll object
	mc.epollfd = epoll_create(1);
	if ( -1 == mc.epollfd ) {
		printf("InitialMailboxNotifier(): epoll open failure.");
		return E_ERR;
	}

	returns = OpenEpiphanyDevice();
	if (returns)
	{
		printf("InitialMailboxNotifier(): epiphany open failure.");
		close(mc.epollfd);
		return returns;
	}

	returns = OpenCancelEventMonitor();
	if (returns)
	{
		printf("InitialMailboxNotifier(): cancel event monitor open failure.");
		// Tidy up
		close(mc.devfd);
		close(mc.epollfd);
		return returns;
	}	

	// Now allocate the event list
	// Max Events (mc.events size): Cancel or Mailbox (maxevents = 2)
	mc.events = calloc(2, sizeof(struct epoll_event));
	if (NULL == mc.events)
	{
		printf("InitialMailboxNotifier(): malloc of event memory failure.");
		// Tidy up
		CloseCancelEventMonitor();
		close(mc.devfd);
		close(mc.epollfd);
		return returns;
	}

	mc.running = 1;
	return returns;
}

int OpenEpiphanyDevice()
{
	// Now open the epiphany device for mailbox interrupt control
	//mc.devfd = open(EPIPHANY_DEV, O_NONBLOCK | O_RDONLY;
	mc.devfd = open(EPIPHANY_DEV, O_RDONLY);
	
	// printf ("OpenEpiphanyDevice(): mc.devfd %d\n", mc.devfd);
	if ( mc.devfd < 0 )
	{
		int rtn = errno;
		printf("InitialMaiboxNotifier(): epiphany device open failed! %s errno %d\n", strerror(rtn), rtn);

		return E_ERR;
	}

	return 0;
}

int ModifyNotifier(int fd, int operation)
{
	return UpdateEpoll(fd, operation, EPOLLIN | EPOLLET);
}

int UpdateEpoll(int fd, int operation, uint32_t waitOnEvent)
{
	int returns;
	struct epoll_event event;
	
	returns = E_ERR;
	
	// Add the handle to epoll
	event.data.fd = fd;
	event.events = waitOnEvent;
	returns = epoll_ctl (mc.epollfd, operation, fd, &event);
	if (returns)
	{
		returns = errno;
		printf("InitialMailboxNotifier(): epoll_ctl failed! %s errno: %d operation: %d, event: %d, fd: %d\n", strerror(returns), returns, operation, waitOnEvent, fd);
	}

	return returns;
}

int WaitForMailboxNotifier()
{
	int numberOfEvents;
	ssize_t size = 0;
	int messages = 0;

	// Max Events (mc.events size): Cancel or Mailbox (maxevents = 2)
	// Wait forever (timeout = -1)
	numberOfEvents = epoll_wait(mc.epollfd, mc.events, 2, -1);
	if ((1 < numberOfEvents) || (mc.events[0].data.fd != mc.devfd))
	{
		printf("INFO: WaitForMailboxNotifier(): Cancelled!\n");
	}

	if (0 > numberOfEvents)
	{
		int epollerrno = errno;
		printf("WaitForMailboxNotifier(): epoll_wait failed! %s errno %d\n", strerror(epollerrno), epollerrno);
	}
	else
	{
		// TODO when mailbox supports epoll
		// App cancel but mailbox may still need to be
		// read then perhaps ephiphany reset to stop
		// further mailbox messages?
	}
	
	return messages;
}

int OpenCancelEventMonitor()
{
	int returns;

	// Create a descriptor to cancel epoll wait
	mc.cancelfd = eventfd(0, 0);
	if ( -1 == mc.cancelfd )
	{
		int rtn = errno;
		printf("InitialMailboxNotifier(): open eventfd failure! %s errno: %d\n", strerror(rtn), rtn);
		
		return E_ERR;
	}

	// Add the cancelfd handle to epoll
	returns = ModifyNotifier(mc.cancelfd, EPOLL_CTL_ADD);
	if (returns)
	{
		int rtn = errno;
		printf("InitialMailboxNotifier(): EPOLL_CTL_ADD cancelfd failed! %s errno: %d cancelfd: %d\n", strerror(rtn), rtn, mc.cancelfd);

		// Tidy up
		close(mc.cancelfd);
		return rtn;
	}

	return returns;
}

void CloseCancelEventMonitor()
{
	ModifyNotifier(mc.cancelfd, EPOLL_CTL_DEL);
	close(mc.cancelfd);
}

void CancelEventMonitor()
{
	int64_t cancel = 1;
	if (sizeof(int64_t) != write(mc.cancelfd, &cancel, sizeof(int64_t)))
	{
		printf("CancelEventMonitor(): Cancelled failed!\n");
	}
}

int e_mailbox_read(struct e_message *msg, int flags)
{
	int rc;
	struct e_mailbox_msg kernel_msg;

	rc = ioctl(mc.devfd, E_IOCTL_MAILBOX_READ, &kernel_msg);
	if (rc < 0)
	{
		printf("e_mailbox_read(): ioctl failed! %s errno %d\n", strerror(rc), rc);
		return rc;
	}

#if 0
	msg->from = kernel_msg.from;
	msg->data = kernel_msg.data;
#else
	/* work around FPGA Elink 64-bit burst bug
	 * Message can only be 32-bits for now so use lower bits for data
	 * and let sender be 0 */
	msg->from = 0;
	msg->data = kernel_msg.from;
#endif

	return 0;
}

int e_mailbox_count()
{
	int rc;

	rc = ioctl(mc.devfd, E_IOCTL_MAILBOX_COUNT);
	if (rc < 0)
	{
		printf("e_mailbox_count(): ioctl failed! %s errno %d\n", strerror(rc), rc);
	}

	return rc;
}
