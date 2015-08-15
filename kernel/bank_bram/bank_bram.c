/************************************************************
 *
 * bank_bram.c
 *
 * A linux kernel character driver to access Bank BRAM on Parallella
 *
 * Copyright (c) 2015 Peter Saunderson <peteasa@gmail.com>
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

#include <linux/init.h>		// for macros
#include <linux/module.h>	// for modules
#include <linux/kernel.h>	// for printk
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>      // for cdev_??
#include <linux/device.h>    // for class_??

// Name of the driver that will appear in /proc/devices and sysfs
#define DRIVER_NAME		"bank_bram"
#define CLASS_NAME		"parafpga"

/* Function prototypes */
static int bram_init(void);
static void bram_exit(void);
static int bram_open(struct inode *, struct file *);
static int bram_release(struct inode *, struct file *);
static int bram_mmap(struct file *, struct vm_area_struct *);
static int bram_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
static long bram_ioctl(struct file *, unsigned int, unsigned long);

// ethernet - platform_driver from platform_driver.h
// regulator - i2c_driver from i2c.h
// rtc - spi_driver from spi.h
// pci - bus_type from device.h
// pci - pci_driver from pci.h
// mmc - sdio_driver from sdio_func.h
// pcmcia - pcmcia_low_level from drivers/pcmcia/soc_common.h
// this driver - file_operations from fs.h

// linux/fs.h for file_operations
// linux/module.h for THIS_MODULE
static struct file_operations bram_fops = {
	.owner = THIS_MODULE,
	.open = bram_open,
	.release = bram_release,
	.mmap = bram_mmap,
	.unlocked_ioctl = bram_ioctl    // Ref http://lwn.net/Articles/119652/
};

static int major = 0;
static dev_t dev_no = 0;                // linux/types.h
static struct cdev *bram_cdev = 0;      // 
static struct class *class_parafpga = 0;    //
static struct device *dev_bram = 0;

static int bram_init(void)
{
	int result = 0;
	void *ptr_err = 0;

	// Register the driver with the kernel - Ref: ch03.pdf page 43-45
	// See comment in fs/char_dev.c
	// major number dynamic allocation
	// @baseminor: first of the requested range of minor numbers = 0
	// @count: the number of minor numbers required = 1
	// Test:  look at the output of ls -l /dev/bank_bram ** check **
	// linux/fs.h for alloc_chrdev_region
	result = alloc_chrdev_region(&dev_no, 0, 1, DRIVER_NAME);
	if (result < 0) {
		// linux/kernel.h for printk
		printk(KERN_ERR "Failed to register the epiphany driver\n");
		return result;
	}

	// linux/fs.h for MAJOR, MKDEV etc
	major = MAJOR(dev_no);
	dev_no = MKDEV(major, 0);

	// linux/cdev.h for cdev_??
	bram_cdev = cdev_alloc();
	bram_cdev->ops = &bram_fops;
	bram_cdev->owner = THIS_MODULE;

	// Ref ch14.pdf page 387
	// TODO not required for my simple module.. perhaps have a class parallella_fpga?
	class_parafpga = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(ptr_err = class_parafpga)) {
		goto err2;
	}

	// Create the device that will handle the new class
	dev_bram = device_create(class_parafpga, NULL, dev_no, NULL, DRIVER_NAME);
	if (IS_ERR(ptr_err = dev_bram)) {
		goto err;
	}

	// TODO add other stuff that actually does work!

	// Now the driver is completely ready so add it to the kernel
	result = cdev_add(bram_cdev, dev_no, 1);
	if (result < 0) {
	printk(KERN_INFO
			"epiphany_init() - Unable to add character device\n");
	}

	printk(KERN_INFO "bram_init() - Done\n");

	return 0;

err:
	class_destroy(class_parafpga);
err2:
	unregister_chrdev_region(dev_no, 1);

	return PTR_ERR(ptr_err);
}

static void bram_exit(void)
{

	device_destroy(class_parafpga, MKDEV(major, 0));
	class_destroy(class_parafpga);
	cdev_del(bram_cdev);
	unregister_chrdev_region(dev_no, 1);
	printk(KERN_INFO "bram_exit() - Done\n");
}

// Ref ch03 page 53
// file represents an open file release by the kernel when no more references
static int bram_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "bram_open() - Done\n");
	return 0;
}

static int bram_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "bram_release() - Done\n");
	return 0;
}

// Ref https://bitbucket.org/hollisb/openmcapi/issue/34/debuggers-cant-see-shared-memory
// Ref: mm/memory.c:access_process_vm
// Add the following to allow gdb to view memory that has been mapped
// But to use you might have to export generic_access_phys from the kernel or build the
// driver as part of the kernel rather than as an out of tree module.
static const struct vm_operations_struct mmap_mem_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
	.fault = bram_fault
};

// mmap is used to request a mapping of device memory to a process's address space
// linux/mm.h for vm_area_struct
static int bram_mmap(struct file *file, struct vm_area_struct *vma)
{
	int retval = 0;

	// TODO should add checks for memory range being valid!

	// kernel can only handle memory in page size chunks so round to nearest page
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long size = vma->vm_end - vma->vm_start;

	printk(KERN_INFO
		"bram_mmap - request to map 0x%08lx, length 0x%08lx bytes\n",
		off,
		size
		);

	// Set Access permissions and switch off caching of the memory
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	// Make the mapped memory visible from gdb
//#ifdef CONFIG_HAVE_IOREMAP_PROT
	vma->vm_ops = &mmap_mem_ops;
//#endif

	printk(KERN_INFO
		"Mapping host memory to vma 0x%08lx, size 0x%08lx, page offset 0x%08lx\n",
		vma->vm_start,
		size,
		vma->vm_pgoff
		);

	// TODO double check that this is not io memory... perhaps epiphany is io memory
	// because it is talking across a bus to the epiphany chip??
	// remap_pfn_range only gives access to reserved pages and physical memory above
	// the top memory address outside the normal linux address space.
	retval = remap_pfn_range(vma,
		vma->vm_start,
		vma->vm_pgoff,
		size,
		vma->vm_page_prot
		);

	printk(KERN_INFO "bram_mmap() - Done\n");

	return retval;
}

// Ref http://lwn.net/Articles/258113/
// nopage is replaced with fault
static int bram_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	// TODO the attempt is to stop remapping check mremap implementation
	// TODO write a test harness to prove it!
	printk(KERN_INFO "bram_fault() - Done\n");
	return VM_FAULT_SIGBUS;
}

// Ref http://lwn.net/Articles/119652/
//   must implement locking
//   inode now available via filp->f_dentry->d_inode
static long bram_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	// TODO
	printk(KERN_INFO "bram_ioctl() - Done\n");
	return 0;
}

module_init(bram_init);
module_exit(bram_exit);

MODULE_AUTHOR("Peter Saunderson");
MODULE_DESCRIPTION("Driver for BRAM AXI access");
MODULE_LICENSE("GPL");

