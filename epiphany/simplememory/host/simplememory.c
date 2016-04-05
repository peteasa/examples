/************************************************************
 
  simplememory.c
 
  A memory test using the updated epiphany kernel driver
 
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
#include <linux/ioctl.h>
#include <uapi/linux/epiphany.h> // for ioctl numbers

// Test message store
#define _BuffSize   (0x2000)
#define _SharedBuffOffset (0x01005000)
#define _LocalBuffOffset (0x5000)
static e_mem_t emem;

// Mailbox defines
// Should move to e-hal
#define ELINK_MAILBOXLO	(0xF0730)
#define ELINK_MAILBOXHI	(0xF0734)
#define ELINK_MAILBOXSTAT (0xF0738)
#define E_SYS_TXSTATUS	(0xF0214)
#define E_SYS_RXSTATUS	(0xF0304)
#define EPIPHANY_DEV "/dev/epiphany"
#define MAILBOXSTATUSSIZE 10

#ifdef EPIPHANY_IOC_MB_ENABLE
typedef struct _MAILBOX_CONTROL
{
	int running;	// 1 if ready; otherwise not ready
	int devfd;	// handle for epiphany device driver
	int epollfd; 	// handle for blocking wait on notification
	int kernelEventfd;	// handle for kernel notification
	int cancelfd;	// handle for cancel
	struct epoll_event *events;
	mailbox_notifier_t mailbox_notifier;
} mailbox_control_t;
static mailbox_control_t mc;
#endif

static e_sys_rxcfg_t rxcfg         = { .reg = 0 };

#define E_SYS_RXIDELAY0 (0xF0310)
#define E_SYS_RXIDELAY1 (0xF0314)

#define TAPS 64
static int idelay[TAPS]={0x00000000,0x00000000,//0
		  0x11111111,0x00000001,//1
		  0x22222222,0x00000002,//2
		  0x33333333,0x00000003,//3
		  0x44444444,0x00000004,//4
		  0x55555555,0x00000005,//5
		  0x66666666,0x00000006,//6
		  0x77777777,0x00000007,//7
		  0x88888888,0x00000008,//8
		  0x99999999,0x00000009,//9
		  0xaaaaaaaa,0x0000000a,//10
		  0xbbbbbbbb,0x0000000b,//11
		  0xcccccccc,0x0000000c,//12
		  0xdddddddd,0x0000000d,//13
		  0xeeeeeeee,0x0000000e,//14
		  0xffffffff,0x0000000f,//15
		  0x00000000,0x00000010,//16
		  0x11111111,0x00000011,//17
		  0x22222222,0x00000012,//18
		  0x33333333,0x00000013,//29
		  0x44444444,0x00000014,//20
		  0x55555555,0x00000015,//21
		  0x66666666,0x00000016,//22
		  0x77777777,0x00000017,//23
		  0x88888888,0x00000018,//24
		  0x99999999,0x00000019,//25
		  0xaaaaaaaa,0x0000001a,//26
		  0xbbbbbbbb,0x0000001b,//27
		  0xcccccccc,0x0000001c,//28
		  0xdddddddd,0x0000001d,//29
		  0xeeeeeeee,0x0000001e,//30
		  0xffffffff,0x0000001f};//31

// Epiphany data
static e_platform_t platform;

static e_epiphany_t dev;
static char emsg[_BuffSize];

// Test control stuff
#define _SeqLen    (100)
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
void PrintStuffOfInterest();
int setup_system(void);
void WriteTestPatternsToMemory(e_mem_t *emem, e_epiphany_t *dev);
void ReadTestPatternsFromMemory(e_mem_t *emem, e_epiphany_t *dev, int initval, int incr);
int CheckTestPatterns(int initval, int incr);

int main(int argc, char *argv[])
{
	pthread_t definedTestTimeThread;
	int threadRtn, returns;

	returns = -1;
	tc.cancelNow = 0;
	tc.keepalive = 0;
	
	if (0 == InitialTest())
	{
#ifdef EPIPHANY_IOC_MB_ENABLE
		printf("main(): devfd %d, epollfd %d, kernelEventfd %d, cancelfd %d\n", mc.devfd, mc.epollfd, mc.kernelEventfd, mc.cancelfd);
#endif		
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
	unsigned row, col, coreid, i;

	if (setup_system())
	{
		return -1;
	}
	rxcfg.reg = ee_read_esys(E_SYS_RXCFG);
	printf("RunTest(): write shadow ERX_CFG_REG = 0x%08x\n", rxcfg.reg);
	       
	if ( -1 == ioctl(mc.devfd, EPIPHANY_IOC_ERX_CFG_REG, &rxcfg.reg) )
	{
		printf("RunTest(): Failed to write shadow ERX_CFG_REG"
		  "Error is %s\n", strerror(errno));
	}

	// This should go into the epiphany initialization
	int delayNo = 7;
	if (sizeof(int) != ee_write_esys(E_SYS_RXIDELAY0, idelay[((delayNo+1)*2)-2]))
	{
		printf("INFO: setting idelay0 failed\n");
	}

	if (sizeof(int) != ee_write_esys(E_SYS_RXIDELAY1, idelay[((delayNo+1)*2)-1]))
	{
		printf("INFO: setting idelay1 failed\n");
	}
    
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

		// Configure epoll to listen for interrupt event
		returns = ArmMailboxNotifier();
		if (returns)
		{
			break;
		}

		// Enable the mailbox interrupt
		// set: static remap, remap mask 0xf00, pattern 0x300 0x300f004
		//rxcfg.reg = rxcfg.reg | (0x1 << 28);
		//if (sizeof(int) != ee_write_esys(E_SYS_RXCFG, rxcfg.reg))
		//{
		//	printf("RunTest(): Failed set rxcfg register\n");
		//}

		// Enable the mailbox interrupt
		if ( -1 == ioctl(mc.devfd, EPIPHANY_IOC_MB_ENABLE) )
		{
			printf("RunTest(): Failed to enable mailbox "
			  "Error is %s\n", strerror(errno));
		}
		usleep(1000);

		// Calculate the coreid
		coreid = (row + platform.row) * 64 + col + platform.col;
		printf("RunTest(): %3d: Message from eCore 0x%03x (%2d,%2d): ", i, coreid, row, col);

		// Open the single-core workgroup and reset the core, in
		// case a previous process is running. Note that we used
		// core coordinates relative to the workgroup.
		e_open(&dev, row, col, 1, 1);
		e_reset_group(&dev);

		WriteTestPatternsToMemory(&emem, &dev);
		ReadTestPatternsFromMemory(&emem, &dev, 0xff, -1);

		// Load the device program onto the selected eCore
		// and launch after loading.
		printf("RunTest(%d,%d): %3d, Load core\n", row, col, i);

		e_return_stat_t result;
		result = e_load("/usr/epiphany/bin/e_memory.srec", &dev, 0, 0, E_TRUE);
		if (result != E_OK)
		{
			printf("RunTest(): Error in e_load %i\n", result);
		}

		// Keep the test alive
		tc->keepalive++;

		usleep(1000);
		printf("main(%d,%d): Core Loaded\n", row, col);

		// Wait for core program execution to finish, then
		// read test results from shared buffer.
		returns = WaitForMailboxNotifier();

		// Keep the test alive
		tc->keepalive++;

		// if multiple mailbox accesses then slow the host side down
		// to avoid read and write of the mailbox at the same time
		// usleep(100000);
		
		ReadTestPatternsFromMemory(&emem, &dev, 0x0, 1);

		// after all the mailbox activity from the epiphany side, read various registers
		PrintStuffOfInterest();
		
		// Read the mailbox
		int mbentries;
		for (mbentries = 0; mbentries < 32; mbentries++)
		{
			int pre_stat    = ee_read_esys(ELINK_MAILBOXSTAT);
			int mbox_lo     = ee_read_esys(ELINK_MAILBOXLO);
			int mbox_hi     = ee_read_esys(ELINK_MAILBOXHI);
			int post_stat   = ee_read_esys(ELINK_MAILBOXSTAT);
			printf ("RunTest(): PRE_STAT=%08x POST_STAT=%08x LO=%08x HI=%08x\n", pre_stat, post_stat, mbox_lo, mbox_hi);

			if (0 == post_stat)
			{
				if (mbox_lo) printf("ERROR: e-task 0x%x read errors for local memory, last error at 0x%x\n", mbox_lo & 0xffff, (mbox_lo>>16) & 0xffff);
				if (mbox_hi) printf("ERROR: e-task 0x%x read errors for shared memory, last error at 0x%x\n", mbox_hi & 0xffff, (mbox_hi>>16) & 0xffff);

				break;
			}
		}

		e_close(&dev);

		col++;
	}
	
	return 0;
}

int CheckTestPatterns(int initval, int incr)
{
	char val = initval;
	int count;
	int failures=0;
	int passes = 0;
	int reports = 0;

	for (count=0; count<_BuffSize; count++)
	{
		if (emsg[count] != val)
		{
			failures++;
		}
		else
		{
			passes++;
		}

		if (0<failures && 10>reports)
		{
			printf("INFO: Read 0x%x, should be 0x%x\n", emsg[count], val);
			reports++;
		}
		val += incr;
	}

	if (0<failures)
	{
		printf("ERROR: 0x%x memory read failures, 0x%x passes\n", failures, passes);
	}

	return failures;
}

void ReadTestPatternsFromMemory(e_mem_t *emem, e_epiphany_t *dev, int initval, int incr)
{
	// read message from shared buffer.
	e_read(emem, 0, 0, (off_t)0x0, (void*)emsg, _BuffSize);

	if (CheckTestPatterns(initval, incr))
	{
		printf("ERROR: Shared memory check failed!\n");
	}    

	// read message from local buffer.
	e_read(dev, 0, 0, (off_t)(_LocalBuffOffset), (void*)emsg, _BuffSize);

	if (CheckTestPatterns(initval, incr))
	{
		printf("ERROR: Local memory check failed!\n");
	}
}

void WriteTestPatternsToMemory(e_mem_t *emem, e_epiphany_t *dev)
{
	char val = 0xff; //_BuffSize - 1;
	int count;
	int failures=0;
	int passes = 0;
	int reports = 0;

	for (count=0; count<_BuffSize; count++)
	{
		emsg[count] = val;
		val--;
	}

	// write message to shared buffer.
	e_write((void*)emem, 0, 0, (off_t)0x0, (void*)emsg, _BuffSize);

	// write message to local buffer.
	e_write((void*)dev, 0, 0, (off_t)(_LocalBuffOffset), (void*)emsg,  _BuffSize);
}

void PrintStuffOfInterest()
{
	// Print stuff of interest
	int etx_status = ee_read_esys(E_SYS_TXSTATUS);
	int etx_config = ee_read_esys(E_SYS_TXCFG);
	int erx_status = ee_read_esys(E_SYS_RXSTATUS);
	int erx_config = ee_read_esys(E_SYS_RXCFG);
	printf("RunTest(): etx_status: 0x%x, etx_config: 0x%x, erx_status: 0x%x, erx_config: 0x%x\n", etx_status, etx_config, erx_status, erx_config);
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
	return e_alloc(&emem, _SharedBuffOffset, _BuffSize);
}

void CloseTestMessageStore()
{
	e_free(&emem);
}

int InitialMailboxNotifier()
{
#ifdef EPIPHANY_IOC_MB_ENABLE
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

	mc.running = 1;
	return returns;
#endif
	return 0;
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
#ifdef EPIPHANY_IOC_MB_ENABLE
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
#endif
	return 0;
}

int ArmMailboxNotifier()
{
#ifdef EPIPHANY_IOC_MB_ENABLE
	if (mc.running)
	{
		return ModifyNotifier(mc.kernelEventfd, EPOLL_CTL_MOD);
	}

	return E_ERR;
#endif
	return 0;
}

int ModifyNotifier(int fd, int operation)
{
	return UpdateEpoll(fd, operation, EPOLLIN | EPOLLET);
}

int UpdateEpoll(int fd, int operation, uint32_t waitOnEvent)
{
#ifdef EPIPHANY_IOC_MB_ENABLE
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
#endif
	return 0;
}

int WaitForMailboxNotifier()
{
#ifdef EPIPHANY_IOC_MB_ENABLE
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
#else
	// do the best we can and wait
	usleep(100000);
	return 0;
#endif	
}

int OpenCancelEventMonitor()
{
#ifdef EPIPHANY_IOC_MB_ENABLE
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
#endif
	return 0;
}

void CloseMailboxNotifier()
{
#ifdef EPIPHANY_IOC_MB_ENABLE
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
#endif
}

void CancelMailboxNotifier()
{
#ifdef EPIPHANY_IOC_MB_ENABLE
	int64_t cancel = 1;
	if (sizeof(int64_t) != write(mc.cancelfd, &cancel, sizeof(int64_t)))
	{
		printf ("CancelMailboxNotifier(): Cancelled failed!\n");
	}
#endif
}

int setup_system(void)
{
	int rc = 0;
	uint32_t divider;
	uint32_t chipid;
	e_sys_txcfg_t txcfg         = { .reg = 0 };
	e_sys_rx_dmacfg_t rx_dmacfg = { .reg = 0 };
	e_sys_clkcfg_t clkcfg       = { .reg = 0 };
	e_sys_reset_t resetcfg      = { .reg = 0 };
	e_epiphany_t dev;

#if 1 // ???
	chipid = 0x808 /* >> 2 */;
	if (sizeof(int) != ee_write_esys(E_SYS_CHIPID, chipid /* << 2 */))
		goto err;
	usleep(1000);
#endif

#if 1
	txcfg.enable = 1;
	txcfg.mmu_enable = 0;
	if (sizeof(int) != ee_write_esys(E_SYS_TXCFG, txcfg.reg))
		goto err;
	usleep(1000);
#endif

	rxcfg.testmode = 0; /* bug/(feature?) workaround */
	rxcfg.mmu_enable = 0;

	// I have no idea how this works!  mailbox data addressed to 810 -> 310??
	rxcfg.remap_cfg = 1; // "static" remap_addr
	rxcfg.remap_mask = 0xf00;
	rxcfg.remap_base = 0x300;//turn 8 into 3
	// But changing to this has no effect!
	//rxcfg.remap_mask = 0xff0;
	//rxcfg.remap_base = 0x810;//route all to 810
	if (sizeof(int) != ee_write_esys(E_SYS_RXCFG, rxcfg.reg))
		goto err;
	usleep(1000);

#if 0 // ?
	rx_dmacfg.enable = 1;
	if (sizeof(int) != ee_write_esys(E_SYS_RXDMACFG, rx_dmacfg.reg))
		goto err;
	usleep(1000);
#endif

	int delayNo = 7;
	if (sizeof(int) != ee_write_esys(E_SYS_RXIDELAY0, idelay[((delayNo+1)*2)-2]))
	{
		printf("ERROR: setting idelay0 failed\n");
		goto err;
	}

	if (sizeof(int) != ee_write_esys(E_SYS_RXIDELAY1, idelay[((delayNo+1)*2)-1]))
	{
		printf("ERROR: setting idelay1 failed\n");
		goto err;
	}

	rc = E_ERR;
	
	if ( E_OK != e_open(&dev, 2, 3, 1, 1) ) {
	  warnx("e_reset_system(): e_open() failure.");
	  goto err;
	}
	
	txcfg.ctrlmode = 0x5; /* Force east */
	txcfg.ctrlmode_select = 0x1; /* */
	usleep(1000);
	if (sizeof(int) != ee_write_esys(E_SYS_TXCFG, txcfg.reg))
	  goto cleanup_platform;
	
	//divider = 1; /* Divide by 4, see data sheet */
	divider = 0; /* Divide by 2, see data sheet */
	usleep(1000);
	if (sizeof(int) != e_write(&dev, 0, 0, E_REG_LINKCFG, &divider, sizeof(int)))
	  goto cleanup_platform;
	
	txcfg.ctrlmode = 0x0;
	txcfg.ctrlmode_select = 0x0; /* */
	usleep(1000);
	if (sizeof(int) != ee_write_esys(E_SYS_TXCFG, txcfg.reg))
	  goto cleanup_platform;

	rc = E_OK;
	
cleanup_platform:
	if (E_OK != rc) printf("e_reset_system(): fails\n");
	e_close(&dev);
	
	usleep(1000);
	return rc;

err:
	warnx("e_reset_system(): Failed\n");
	usleep(1000);
	return E_ERR;
}
