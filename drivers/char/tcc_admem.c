/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <soc/tcc/pmap.h>
#if defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
#include <mach/bsp.h>
#include <mach/tcc_mem_ioctl.h>
#include <mach/tccfb_ioctrl.h>
#include <mach/tcc_vsync_ioctl.h>
#else
#include <video/tcc/tcc_mem_ioctl.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_vsync_ioctl.h>
#endif

#define DEV_MAJOR	236
#define DEV_MINOR	1
#define DEV_NAME    "admem"

//#define dbg_admem
#ifdef dbg_admem
#define dprintk(msg...) pr_info("adMEM: " msg)
#else
#define dprintk(msg...)
#endif

//static int sync_once = 1;

long admem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	default:
		pr_err("Unsupported cmd(0x%x) for %s\n", cmd, __func__);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int admem_open(struct inode *inode, struct file *filp)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int admem_release(struct inode *inode, struct file *filp)
{
	dprintk("%s\n", __func__);
	return 0;
}

static void mmap_mem_open(struct vm_area_struct *vma)
{
	/* do nothing */
}

static void mmap_mem_close(struct vm_area_struct *vma)
{
	/* do nothing */
}

static struct vm_operations_struct admem_mmap_ops = {
	.open = mmap_mem_open,
	.close = mmap_mem_close,
};

static inline int uncached_access(struct file *file, unsigned long addr)
{
	if (file->f_flags & O_SYNC)
		return 1;
	return addr >= __pa(high_memory);
}

#if !(KERNEL_VERSION(2, 6, 39) <= LINUX_VERSION_CODE)
static pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
	unsigned long size, pgprot_t vma_prot)
{
	unsigned long offset = pfn << PAGE_SHIFT;

	if (uncached_access(file, offset))
		return pgprot_noncached(vma_prot);
	return vma_prot;
}
#endif


static int admem_mmap(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;

	if (range_is_allowed(vma->vm_pgoff, size) < 0) {
		pr_err("admem: this address is not allowed\n");
		return -EPERM;
	}

	vma->vm_page_prot = phys_mem_access_prot(file,
		vma->vm_pgoff, size, vma->vm_page_prot);
	vma->vm_ops = &admem_mmap_ops;

	if (remap_pfn_range(vma,
		vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) {
		pr_err("admem: remap_pfn_range failed\n");
		return -EAGAIN;
	}
	return 0;
}

static struct file_operations admem_fops = {
	.unlocked_ioctl = admem_ioctl,
	.open = admem_open,
	.release = admem_release,
	.mmap = admem_mmap,
};

static void __exit admem_cleanup(void)
{
	unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

static struct class *admem_class;

static int __init admem_init(void)
{
	if (register_chrdev(DEV_MAJOR, DEV_NAME, &admem_fops))
		pr_err("unable to get major %d for tMEM device\n", DEV_MAJOR);
	admem_class = class_create(THIS_MODULE, DEV_NAME);
	device_create(admem_class, NULL, MKDEV(DEV_MAJOR, DEV_MINOR), NULL,
		DEV_NAME);

	return 0;
}

module_init(admem_init)
module_exit(admem_cleanup)
