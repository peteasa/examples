/************************************************************
 
  mailbox.c
 
  A linux kernel character driver to use the parallella oh mailbox
 
  Copyright (c) 2015 Peter Saunderson <peteasa@gmail.com>

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
#include <linux/ioctl.h>
#include <uapi/linux/epiphany.h> // for ioctl numbers

// Following goes into epiphany.h

typedef struct _MAILBOX_NOTIFIER
{
	int old_notifier;
	int new_notifier;
} mailbox_notifier_t;

#define EPIPHANY_IOC_MB_NOTIFIER_CMD	27

#define EPIPHANY_IOC_MAXNR	27
 
#define EPIPHANY_IOC_MB_NOTIFIER _IOW(EPIPHANY_IOC_MAGIC, EPIPHANY_IOC_MB_NOTIFIER_CMD, mailbox_notifier_t *)

/**
 * mailbox notifier file
 */
#define MAILBOX_NOTIFIER "/sys/class/epiphany/epiphany/mailbox_notifier"
//

// Test message store
#define _BufSize   (128)
#define _BufOffset (0x01000000)
static e_mem_t emem;

// Mailbox defines
// Should move to e-hal
#define ELINK_MAILBOXLO	(0xF0320)
#define ELINK_MAILBOXHI	(0xF0324)
#define ELINK_MAILBOXSTAT (0xF0328)
#define ELINK_TXCFG	(0xF0210)
#define ELINK_RXCFG	(0xF0300)
#define EPIPHANY_DEV "/dev/epiphany"
#define MAILBOXSTATUSSIZE 10
typedef struct _MAILBOX_CONTROL
{
	int running;	// 1 if ready; otherwise not ready
	int devfd;	// handle for epiphany device driver
	int epollfd; 	// handle for blocking wait on notification
	int kernelEventfd;	// handle for kernel notification
	char mailboxStatus[MAILBOXSTATUSSIZE]; // last read message
	int cancelfd;	// handle for cancel
	struct epoll_event *events;
	mailbox_notifier_t mailbox_notifier;
} mailbox_control_t;
static mailbox_control_t mc;

// Epiphany data
static e_platform_t platform;

static e_epiphany_t dev;
static char emsg[_BufSize];

// Test control stuff
#define _SeqLen    (32)
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
int OpenKernelEventMonitor();
int OpenCancelEventMonitor();
void CloseMailboxNotifier();
void CancelMailboxNotifier();
int ArmMailboxNotifier();
int WaitForMailboxNotifier();

int main(int argc, char *argv[])
{
	pthread_t definedTestTimeThread;
	int threadRtn, returns;

	returns = -1;
	tc.cancelNow = 0;
	tc.keepalive = 0;
	
	if (0 == InitialTest())
	{
		printf("main(): devfd %d, epollfd %d, kernelEventfd %d, cancelfd %d\n", mc.devfd, mc.epollfd, mc.kernelEventfd, mc.cancelfd);
		// Create a thread to monitor the progress of the test
		threadRtn = pthread_create(&definedTestTimeThread, NULL, SleepThenCancel, (void *) &tc);

		// Run the test
		returns = RunTest(&tc);

		// If the test finishes early cancel the monitor progress
		tc.cancelNow = 1;

		// Wait for monitor thread to complete
		pthread_join(definedTestTimeThread, NULL);

		CloseTest();
	}
	
	return returns;	
}

int RunTest(test_control_t *tc)
{
	int returns;
	unsigned row, col, coreid, i;

	// Keep the test alive
	tc->keepalive++;

	row = 0;
	col = 0;
	for (i=0; i<_SeqLen; i++)
	{
		if (tc->cancelNow)
		{
			break;
		}

		ArmMailboxNotifier();
		
		// Visit each core
		unsigned lastCol = col;
		col = col % platform.cols;

		if (col < lastCol)
		{
			row++;
			unsigned lastRow = row;
			row = row % platform.rows;

			if (row < lastRow)
			{
				break;
			}
		}
		
		// Calculate the coreid
		coreid = (row + platform.row) * 64 + col + platform.col;
		printf("RunTest(): %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);

		// Open the single-core workgroup and reset the core, in
		// case a previous process is running. Note that we used
		// core coordinates relative to the workgroup.
		e_open(&dev, row, col, 1, 1);
		e_reset_group(&dev);

		// Load the device program onto the selected eCore
		// and launch after loading.
		e_return_stat_t result;
		result = e_load("/usr/epiphany/bin/e_mailbox.srec", &dev, 0, 0, E_TRUE);
		if (result != E_OK)
		{
			printf("RunTest(): Error in e_load %i\n", result);
		}

		// Keep the test alive
		tc->keepalive++;

		// Enable the mailbox interrupt
		//if ( -1 == ioctl(devfd, EPIPHANY_IOC_MB_ENABLE) )
		//{
		//	printf("main(): Failed to enable mailbox "
		//	  "Error is %s\n", strerror(errno));
		//}

		// Enable the mailbox interrupt
		int rxcfg = ee_read_esys(ELINK_RXCFG);
		if (sizeof(int) != ee_write_esys(ELINK_RXCFG, rxcfg | (0x1 << 28)))
		{
			printf("RunTest(): Failed to enable mailbox interrupt\n");
		}
	
		int pre_stat    = ee_read_esys(ELINK_MAILBOXSTAT);
		printf("RunTest(): pre_stat: %x\n", pre_stat);
		int mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
		printf("RunTest(): mbox_lo: %x\n", mbox_lo);
		mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
		printf("RunTest(): mbox_lo: %x\n", mbox_lo);
		int mbox_hi     = ee_read_esys(ELINK_MAILBOXHI);
		printf("RunTest(): mbox_hi: %x\n", mbox_hi);
		mbox_hi     = ee_read_esys(ELINK_MAILBOXHI);
		printf("RunTest(): mbox_hi: %x\n", mbox_hi);
		mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
		printf("RunTest(): mbox_lo: %x\n", mbox_lo);
		pre_stat    = ee_read_esys(ELINK_MAILBOXSTAT);
		printf("RunTest(): post_stat: %x\n", pre_stat);
		
		
		// Wait for core program execution to finish, then
		// read message from shared buffer.
		returns = WaitForMailboxNotifier();

		// ArmMailboxNotifier();

		// Keep the test alive
		tc->keepalive++;
	
		e_read(&emem, 0, 0, 0x0, emsg, _BufSize);

		// Print the message and close the workgroup.
		printf("\"%s\"\n", emsg);

		/* Temp removal of the following
                   Seems to cause problems with visiting each core. 
		// Read the mailbox
		int mbentries;
		for (mbentries = 0; mbentries < _SeqLen; mbentries++)
		{
			int pre_stat    = ee_read_esys(ELINK_MAILBOXSTAT);
			if (0 == pre_stat)
			{
				break;
			}

			int mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
			int mbox_hi     = ee_read_esys(ELINK_MAILBOXHI);
			int post_stat   = ee_read_esys(ELINK_MAILBOXSTAT);
			printf ("main(): PRE_STAT=%08x POST_STAT=%08x LO=%08x HI=%08x\n", pre_stat, post_stat, mbox_lo, mbox_hi);
		}
		*/
		usleep(1000);
		
		e_close(&dev);
	
		col++;
	}
	
	// Read the mailbox
	int mbentries;
	for (mbentries = 0; mbentries < _SeqLen; mbentries++)
	{
		int pre_stat    = ee_read_esys(ELINK_MAILBOXSTAT);
		if (0 == pre_stat)
		{
			break;
		}
		int mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
		int mbox_hi     = ee_read_esys(ELINK_MAILBOXHI);
		int post_stat   = ee_read_esys(ELINK_MAILBOXSTAT);
		printf ("main(): PRE_STAT=%08x POST_STAT=%08x LO=%08x HI=%08x\n", pre_stat, post_stat, mbox_lo, mbox_hi);
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
		if (tc->cancelNow || (tc->keepalive == oldkeepalive))
		{
			printf ("\nINFO: SleepThenCancel(): Cancelling!\n");
			break;
		}
	}

	tc->cancelNow = 1;
	CancelMailboxNotifier();
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
	CloseMailboxNotifier();
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
	// Allocate a buffer in shared external memory
	// for message passing from eCore to host.
	return e_alloc(&emem, _BufOffset, _BufSize);
}

void CloseTestMessageStore()
{
	e_free(&emem);
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

	returns = OpenKernelEventMonitor();
	if (returns)
	{
		printf("InitialMailboxNotifier(): mailbox sysfs monitor open failure.");
		// Tidy up
		close(mc.devfd);
		close(mc.epollfd);
		return returns;
	}

	returns = OpenCancelEventMonitor();
	if (returns)
	{
		printf("InitialMailboxNotifier(): cancel event monitor open failure.");
		// Tidy up
		struct epoll_event event;
		epoll_ctl (mc.epollfd, EPOLL_CTL_DEL, mc.kernelEventfd, &event);
		close(mc.kernelEventfd);
		close(mc.devfd);
		close(mc.epollfd);
		return returns;
	}	

	// Now allocate the event list
	mc.events = calloc(2, sizeof(struct epoll_event));
	if (NULL == mc.events)
	{
		printf("InitialMailboxNotifier(): malloc of event memory failure.");
		// Tidy up
		struct epoll_event event;
		epoll_ctl (mc.epollfd, EPOLL_CTL_DEL, mc.cancelfd, &event);
		epoll_ctl (mc.epollfd, EPOLL_CTL_DEL, mc.kernelEventfd, &event);
		close(mc.cancelfd);
		close(mc.kernelEventfd);
		close(mc.devfd);
		close(mc.epollfd);
		return returns;
	}

	// empty the last known message
	mc.mailboxStatus[0] = 0;	
	mc.mailboxStatus[MAILBOXSTATUSSIZE-1] = 0;
	mc.running = 1;
	return returns;
}

int OpenEpiphanyDevice()
{
	// Now open the epiphany device for mailbox interrupt control
	mc.devfd = open(EPIPHANY_DEV, O_RDWR | O_SYNC);
	// printf ("OpenEpiphanyDevice(): mc.devfd %d\n", mc.devfd);
	if ( -1 == mc.devfd )
	{
		int rtn = errno;
		printf ("InitialMaiboxNotifier(): epiphany device open failed! %s errno %d\n", strerror(rtn), rtn);

		return E_ERR;
	}

	return 0;
}

int OpenKernelEventMonitor()
{
	int returns;
	char notifier[8];
	int notifierfd;
	int oldNotifier;

	// Open the kernel Event Notifier
	mc.kernelEventfd = eventfd(0, EFD_NONBLOCK);
	if ( -1 == mc.kernelEventfd )
	{
		int rtn = errno;
		printf ("InitialMailboxNotifier(): Kernel Event Notifier open failure! %s errno: %d\n", strerror(rtn), rtn);

		return E_ERR;
	}

	// Add the kernelEventfd handle to epoll
	returns = ModifyNotifier(mc.kernelEventfd, EPOLL_CTL_ADD);
	if (returns)
	{
		int rtn = errno;
		printf ("InitialMailboxNotifier(): EPOLL_CTL_ADD kernelEventfd failed! %s errno: %d kernelEventfd: %d\n", strerror(rtn), rtn, mc.kernelEventfd);

		// Tidy up
		close(mc.kernelEventfd);
		return rtn;
	}

	// Starting from scratch with no other application running
	// read the current kernel mailbox_notifier fd handle
	// If the current kernel mailbox_notifier fd handle is -1 there is no
	// other application using the mailbox.
	notifierfd = open(MAILBOX_NOTIFIER, O_RDONLY);
	oldNotifier = -1;
	if (0 < notifierfd)
	{
		int rtn = read(notifierfd, notifier, 8);

		// printf ("InitialMailboxNotifier(): returns: %d, Old notifier fd: %s\n", rtn, notifier);
		if (rtn)
		{
			sscanf(notifier, "%d", &oldNotifier);
		}

		close(notifierfd);
	}
	
	// Starting from scratch ignore other applications and override them
	// by passing the old kernel mailbox_notifier fd handle to the driver
	// and replace this with the new fd
	mc.mailbox_notifier.old_notifier = oldNotifier;
	mc.mailbox_notifier.new_notifier = mc.kernelEventfd;
	if ( -1 == ioctl(mc.devfd, EPIPHANY_IOC_MB_NOTIFIER, &mc.mailbox_notifier) )
	{
		int rtn = errno;
		printf("InitialMailboxNotifier(): Failed to send notifier to driver. %s errno: %d kernelEventfd: %d\n", strerror(rtn), rtn, mc.kernelEventfd);

		// Tidy up
		struct epoll_event event;
		epoll_ctl (mc.epollfd, EPOLL_CTL_DEL, mc.kernelEventfd, &event);
		close(mc.kernelEventfd);
		mc.kernelEventfd = -1;
		return rtn;
	}

	return returns;
}

int ArmMailboxNotifier()
{
	if (mc.running)
	{
		return ModifyNotifier(mc.kernelEventfd, EPOLL_CTL_MOD);
	}

	return E_ERR;
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
	
	// Add the kernelEventfd handle to epoll
	event.data.fd = fd;
	event.events = waitOnEvent;
	returns = epoll_ctl (mc.epollfd, operation, fd, &event);
	if (returns)
	{
		returns = errno;
		printf ("InitialMailboxNotifier(): epoll_ctl failed! %s errno: %d operation: %d, event: %d, fd: %d\n", strerror(returns), returns, operation, waitOnEvent, fd);
	}

	return returns;
}

int WaitForMailboxNotifier()
{
	int numberOfEvents;
	size_t bytesRead;
	int64_t eventfdCount;

	numberOfEvents = epoll_wait(mc.epollfd, mc.events, 2, -1);
	if ((1 < numberOfEvents) || (mc.events[0].data.fd != mc.kernelEventfd))
	{
		printf("INFO: WaitForMailboxNotifier(): Cancelled!\n");
	}

	if (0 > numberOfEvents)
	{
		int epollerrno = errno;
		printf("WaitForMailboxNotifier(): epoll_wait failed! %s errno %d\n", strerror(epollerrno), epollerrno);
	}

	bytesRead = read(mc.kernelEventfd, &eventfdCount, sizeof(int64_t));
	if (0 > bytesRead)
	{
		// failure to reset the eventfd counter to zero
		// can cause lockups!
		int eventfderrno = errno;
		printf("ERROR: WaitForMailboxNotifier(): lockup likely: eventfd counter reset failed! %s errno %d\n", strerror(eventfderrno), eventfderrno);
	}
	
	// printf("WaitForMailboxNotifier(): bytesRead: %d, eventfdCount: %d\n", bytesRead, eventfdCount);

	return numberOfEvents;
}

int OpenCancelEventMonitor()
{
	int returns;

	// Create a descriptor to cancel epoll wait
	mc.cancelfd = eventfd(0, 0);
	if ( -1 == mc.cancelfd )
	{
		int rtn = errno;
		printf ("InitialMailboxNotifier(): open eventfd failure! %s errno: %d\n", strerror(rtn), rtn);
		
		return E_ERR;
	}

	// Add the cancelfd handle to epoll
	returns = ModifyNotifier(mc.cancelfd, EPOLL_CTL_ADD);
	if (returns)
	{
		int rtn = errno;
		printf ("InitialMailboxNotifier(): EPOLL_CTL_ADD cancelfd failed! %s errno: %d cancelfd: %d\n", strerror(rtn), rtn, mc.cancelfd);

		// Tidy up
		close(mc.cancelfd);
		return rtn;
	}

	return returns;
}

void CloseMailboxNotifier()
{
	//printf ("INFO: MailboxNotifier Closing\n");
	if (mc.running)
	{
		CancelMailboxNotifier();

		if (0 < mc.kernelEventfd)
		{
			mc.mailbox_notifier.old_notifier = mc.kernelEventfd;
			mc.mailbox_notifier.new_notifier = -1;
			ioctl(mc.devfd, EPIPHANY_IOC_MB_NOTIFIER, &mc.mailbox_notifier);
		}
		
		struct epoll_event event;
		epoll_ctl (mc.epollfd, EPOLL_CTL_DEL, mc.kernelEventfd, &event);
		epoll_ctl (mc.epollfd, EPOLL_CTL_DEL, mc.cancelfd, &event);
		free((void *)mc.events);
		close(mc.cancelfd);
		close(mc.kernelEventfd);
		mc.kernelEventfd = -1;
		close(mc.devfd);
		close(mc.epollfd);
	}
}

void CancelMailboxNotifier()
{
	int64_t cancel = 1;
	if (sizeof(int64_t) != write(mc.cancelfd, &cancel, sizeof(int64_t)))
	{
		printf ("CancelMailboxNotifier(): Cancelled failed!\n");
	}
}

