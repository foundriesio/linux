/*
 * drivers/gpu/vivante/bare_metal/2d_rotation_drv.c
 *
 * Vivante 2D Rotation driver with Bare-Metal case
 *
 * Copyright (C) 2009 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include "v2d_ioctl.h"

static int debug = 0;
#define dprintk(msg...)	if(debug) { 	printk( "############## V2D_ROT:  " msg); 	}

#define CLK_ALWAYS_ON // clk on when driver probe.

//#define TEST_V2D_USING_FB
#ifdef TEST_V2D_USING_FB
#include <soc/tcc/pmap.h>

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
#include <mach/tccfb_ioctrl.h>
#include <mach/vioc_rdma.h>
#include <mach/tcc_wmixer_ioctrl.h>
#define NUM_FB_RDMA 4
#else
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/tcc_wmixer_ioctrl.h>
#define NUM_FB_RDMA 0
#endif
#define WMIXER_PATH	"/dev/wmixer0"
#endif

//#define TEST_V2D_768X576_ROT
#ifdef TEST_V2D_768X576_ROT
#include "v2d_test.h"
#include <soc/tcc/pmap.h>

static unsigned long _v2d_GetTimediff_us(struct timeval time1, struct timeval time2)
{
	unsigned long time_diff_ms = 0;

	time_diff_ms = (time2.tv_sec- time1.tv_sec)*1000000;
	time_diff_ms += (time2.tv_usec-time1.tv_usec);

	return time_diff_ms;
}

#endif

#define DEVICE_NAME "v2d_rot"

struct v2d_data {
	// wait for poll
	wait_queue_head_t	poll_wq;
	spinlock_t			poll_lock;
	unsigned int		poll_count;

	// wait for ioctl command
	wait_queue_head_t	cmd_wq;
	spinlock_t			cmd_lock;
	unsigned int		cmd_count;

	struct mutex		io_mutex;
	unsigned char		block_operating;
	unsigned char		block_waiting;
	unsigned char		irq_reged;
	unsigned int		dev_opened;
};

struct v2d_drv_type {
	unsigned int		id;
	unsigned int		irq;
	void __iomem 		*reg_vaddr;

	struct miscdevice	*misc;
#ifdef CONFIG_OF
	struct clk 			*v2d_clk;
	#ifdef CONFIG_ARCH_TCC897X
	struct clk 			*v2d_ddi_clk;
	#endif
#endif
	struct v2d_data		*data;
};

extern int range_is_allowed(unsigned long pfn, unsigned long size);
static int v2d_drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		printk(KERN_ERR	 "%s():  This address is not allowed. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		printk(KERN_ERR	 "%s():  Virtual address page port error. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_ops	= NULL;
	vma->vm_flags 	|= VM_IO;
	vma->vm_flags 	|= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

static unsigned int v2d_drv_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct v2d_drv_type	*v2d = dev_get_drvdata(misc->parent);
	int ret = 0;

	if(v2d->data == NULL)
		return -EFAULT;

	poll_wait(filp, &(v2d->data->poll_wq), wait);
	spin_lock_irq(&(v2d->data->poll_lock));
	if (v2d->data->block_operating == 0)
		ret = (POLLIN|POLLRDNORM);
	spin_unlock_irq(&(v2d->data->poll_lock));

	return ret;
}

static irqreturn_t v2d_drv_handler(int irq, void *client_data)
{
	struct v2d_drv_type *v2d = (struct v2d_drv_type *)client_data;


	return IRQ_HANDLED;
}

/** For usage in kernel :: V2D_IOCTL_ROTATION_KERENL
 
struct file *file;
stRot_Info stRotInfo;
int ret;

stRotInfo.cmd_phy_addr = xx;
stRotInfo.cmd_len      = xx; //at least, 0x4000
stRotInfo.src_phy_addr = xx;
stRotInfo.dst_phy_addr = xx;
stRotInfo.src_resolution = xx;
stRotInfo.rotation = xx;

file = filp_open("/dev/v2d_rot", O_RDWR, 0666);
if( 0 > (ret = file->f_op->unlocked_ioctl(file, V2D_IOCTL_ROTATION_KERENL, (unsigned long)&stRotInfo)))
{
	printk(" Error :: v2d_rot ioctl(0x%x) \n", V2D_IOCTL_ROTATION_KERENL);
}
filp_close(file, 0);

**/

static int _v2d_wait_done(struct v2d_drv_type *v2d)
{
	int nMax_Count = 160;
	int i = 0;

	while(i < nMax_Count)
	{
		udelay(100);
		if(*((unsigned int *)(v2d->reg_vaddr + 0x4/*AQHiIdle*/)) == 0x7fffffff)
			break;
		i++;
	}

	if(i >= nMax_Count){
		printk("%s-%d :: Timeout => wait-time(%d ms)\n", __func__, __LINE__, i);
		return -1;
	}

	return 0;
}

#ifdef TEST_V2D_USING_FB
static int _v2dTest_set_source(stRot_Info stRotInfo)
{
	struct file *file = NULL;
	int ret = -1;
	pmap_t fb_pmap;
	WMIXER_INFO_TYPE WmixerInfo;
	VIOC_RDMA *pRDMA = VIOC_RDMA_GetAddress(NUM_FB_RDMA);

	pmap_get_info("fb_video", &fb_pmap);

	file = filp_open(WMIXER_PATH, O_RDWR, 0666);

	if(file != NULL)
	{
		unsigned int src_w = 0, src_h = 0;

#ifdef SUPPORT_VARIABLE_RESOLUTION
		src_w		= stRotInfo.src_resolution.width;
		src_h		= stRotInfo.src_resolution.height;
#else
		src_w		= resolution_info[stRotInfo.src_resolution].width;
		src_h		= resolution_info[stRotInfo.src_resolution].height;
#endif
		
		memset(&WmixerInfo, 0x00, sizeof(WMIXER_INFO_TYPE));

		WmixerInfo.rsp_type		= WMIXER_POLLING;

		//source info
		WmixerInfo.src_fmt			= TCC_LCDC_IMG_FMT_RGB888;
		VIOC_RDMA_GetImageSize(pRDMA, &WmixerInfo.src_img_width, &WmixerInfo.src_img_height);
		WmixerInfo.src_win_left		= 100;
		WmixerInfo.src_win_top		= 0;
		WmixerInfo.src_win_right	= WmixerInfo.src_win_left + src_w;
		WmixerInfo.src_win_bottom	= WmixerInfo.src_win_top + src_h;

		if(src_h > WmixerInfo.src_img_height || src_w > WmixerInfo.src_img_width){
			printk("%s-%d :: Error - RDMA(0x%x) doesn't support this size (%d x %d) < (%d x %d) \n", __func__, __LINE__,
						pRDMA, src_w, src_h, WmixerInfo.src_img_width, WmixerInfo.src_img_height);
			ret = -1;
		}
		else{		
			WmixerInfo.src_y_addr		= (unsigned int)fb_pmap.base;

			//destination info
			WmixerInfo.dst_fmt			= TCC_LCDC_IMG_FMT_RGB888;
			WmixerInfo.dst_img_width   	= src_w;
			WmixerInfo.dst_img_height  	= src_h;
			WmixerInfo.dst_win_left		= 0;
			WmixerInfo.dst_win_top		= 0;
			WmixerInfo.dst_win_right	= WmixerInfo.dst_win_left + WmixerInfo.dst_img_width;
			WmixerInfo.dst_win_bottom	= WmixerInfo.dst_win_top + WmixerInfo.dst_img_height;
			WmixerInfo.dst_y_addr		= stRotInfo.src_phy_addr;

			ret = file->f_op->unlocked_ioctl(file, TCC_WMIXER_IOCTRL_KERNEL, (unsigned long)&WmixerInfo);
		}
		filp_close(file, 0);
	}

	return ret;
}

static void _v2dTest_display_result(stRot_Info stRotInfo)
{
	unsigned int dst_w = 0, dst_h = 0;
	unsigned int src_w = 0, src_h = 0;

	VIOC_RDMA *pRDMA = VIOC_RDMA_GetAddress(NUM_FB_RDMA);

#ifdef SUPPORT_VARIABLE_RESOLUTION
	src_w		= stRotInfo.src_resolution.width;
	src_h		= stRotInfo.src_resolution.height;
#else
	src_w		= resolution_info[stRotInfo.src_resolution].width;
	src_h		= resolution_info[stRotInfo.src_resolution].height;
#endif

	if(stRotInfo.rotation == R_90 || stRotInfo.rotation == R_270) {
		dst_w		= src_h;
		dst_h		= src_w;
	}
	else {
		dst_w		= src_w;
		dst_h		= src_h;
	}

	VIOC_RDMA_GetImageSize(pRDMA, &src_w, &src_h);
#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
	VIOC_RDMA_SetImageSize(pRDMA, src_w/*dst_w*/, src_h/*dst_h*/);
#else
	VIOC_RDMA_SetImageSize(pRDMA, dst_w, dst_h);
#endif
	VIOC_RDMA_SetImageBase(pRDMA, stRotInfo.dst_phy_addr, 0x00, 0x00);
	VIOC_RDMA_SetImageOffset(pRDMA, TCC_LCDC_IMG_FMT_RGB888, dst_w);
	VIOC_RDMA_SetImageEnable(pRDMA);
}
#endif

static long v2d_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct v2d_drv_type	*v2d = dev_get_drvdata(misc->parent);
	int ret = 0;
	stRot_Info stRotInfo;

#if defined(TEST_V2D_768X576_ROT) && !defined(CLK_ALWAYS_ON)
	mdelay(100);
#endif

	mutex_lock(&v2d->data->io_mutex);

	dprintk("%s-%d :: cmd(0x%x) \n", __func__, __LINE__, cmd);

	switch(cmd)
	{
		case V2D_IOCTL_ROTATION:
		case V2D_IOCTL_ROTATION_KERENL:
		{
			if(cmd == V2D_IOCTL_ROTATION_KERENL) {
				memcpy(&stRotInfo, (stRot_Info*)arg, sizeof(stRot_Info));
			} else {
				if(copy_from_user(&stRotInfo,(int*)arg, sizeof(stRot_Info))){
					ret = -EFAULT;
				}
			}

#ifdef SUPPORT_VARIABLE_RESOLUTION
			dprintk("CMD(0x%x) / SRC(0x%x) / DST(0x%x) / Resolution(%dx%d) / Rotation(%d) \n", stRotInfo.cmd_phy_addr, stRotInfo.src_phy_addr, stRotInfo.dst_phy_addr, stRotInfo.src_resolution.width, stRotInfo.src_resolution.height, stRotInfo.rotation);
#else
			dprintk("CMD(0x%x) / SRC(0x%x) / DST(0x%x) / Resolution(%d) / Rotation(%d) \n", stRotInfo.cmd_phy_addr, stRotInfo.src_phy_addr, stRotInfo.dst_phy_addr, stRotInfo.src_resolution, stRotInfo.rotation);
#endif
			if(ret >= 0){
				ret = v2d_proc_rotation((unsigned int)v2d->reg_vaddr, stRotInfo);
				if(ret >= 0)
					_v2d_wait_done(v2d);
			}
		}
		break;

#ifdef TEST_V2D_USING_FB
		case 0x1234:
		case 0x1235: // for kernel call
		{
			if(cmd == 0x1235) {
				memcpy(&stRotInfo, (stRot_Info*)arg, sizeof(stRot_Info));
			} else {
				if(copy_from_user(&stRotInfo,(int*)arg, sizeof(stRot_Info))){
					ret = -EFAULT;
				}
			}

#ifdef SUPPORT_VARIABLE_RESOLUTION
			dprintk("CMD(0x%x) / SRC(0x%x) / DST(0x%x) / Resolution(%dx%d) / Rotation(%d) \n", stRotInfo.cmd_phy_addr, stRotInfo.src_phy_addr, stRotInfo.dst_phy_addr, stRotInfo.src_resolution.width, stRotInfo.src_resolution.height, stRotInfo.rotation);
#else
			dprintk("CMD(0x%x) / SRC(0x%x) / DST(0x%x) / Resolution(%d) / Rotation(%d) \n", stRotInfo.cmd_phy_addr, stRotInfo.src_phy_addr, stRotInfo.dst_phy_addr, stRotInfo.src_resolution, stRotInfo.rotation);
#endif

			if(ret >= 0 && 0 <= _v2dTest_set_source(stRotInfo)){
				ret = v2d_proc_rotation((unsigned int)v2d->reg_vaddr, stRotInfo);
				if(ret >= 0)
					_v2d_wait_done(v2d);
				_v2dTest_display_result(stRotInfo);
			}
		}
		break;
#endif

#ifdef TEST_V2D_768X576_ROT
		case 0x1001:
		{
			int i = 0;
			int cnt = 0;
			int max_cnt = 100;
			pmap_t pmap_rot;
			unsigned int *gd_buffer = NULL;
			unsigned char *remap_rot = NULL;
			unsigned int args_rot[4];

//arg[0] = physical address
//arg[1] = memory length, at least 6MB
//arg[2] = test_gap
//arg[3] = rotation type

			if(copy_from_user(&args_rot,(int*)arg, sizeof(unsigned int) * 4)){
				ret = -EFAULT;
				goto Error;
			}

			stRotInfo.rotation = args_rot[3];

		#ifdef V2D_768X576_ROT_90
			if(stRotInfo.rotation == R_90)
				gd_buffer = R90_gd_buffer;
			else
		#endif
		#ifdef V2D_768X576_ROT_270
			if(stRotInfo.rotation == R_270)
				gd_buffer = R270_gd_buffer;
			else
		#endif
		#ifdef V2D_768X576_ROT_HFLIP
			if(stRotInfo.rotation == R_H_FLIP)
				gd_buffer = hflip_gd_buffer;
			else
		#endif
		#ifdef V2D_768X576_ROT_VFLIP
			if(stRotInfo.rotation == R_V_FLIP)
				gd_buffer = vflip_gd_buffer;
			else
		#endif
			{
				printk("%s-%d :: error strange rotation(%d) \n", __func__, __LINE__, stRotInfo.rotation);
				ret = -EFAULT;
				goto Error;
			}

//			pmap_get_info("video", &pmap_rot);
			pmap_rot.base = args_rot[0];
			pmap_rot.size = args_rot[1];
			max_cnt = ((pmap_rot.size-0x600000/* src/cmd/dst memory */)/args_rot[2]) + 1;

			remap_rot = (unsigned char*)ioremap_nocache((phys_addr_t)pmap_rot.base, pmap_rot.size);
			if(remap_rot)
			{
				unsigned int *proc_data = NULL;
				unsigned int count = sizeof(src_buffer)/sizeof(unsigned int);
				unsigned int nDifference = 0;
				unsigned int dst_offset = 0x0;
				unsigned int cmd_offset = 0x0;
				unsigned int src_offset = 0x100000;
				struct timeval t1 , t2;
				unsigned long time_us;

				memcpy((void*)(remap_rot + 0x100000), src_buffer, sizeof(src_buffer));
TEST_LOOP:
				dst_offset = 0x400000 + (args_rot[2] * cnt);
				proc_data = (unsigned int *)(remap_rot + dst_offset);
				memset((void*)proc_data, 0x00, sizeof(src_buffer));

				stRotInfo.cmd_phy_addr 	= pmap_rot.base + cmd_offset;
				stRotInfo.cmd_len		= 0x100000;
				stRotInfo.src_phy_addr	= pmap_rot.base + src_offset;
				stRotInfo.dst_phy_addr	= pmap_rot.base + dst_offset;
			#ifdef SUPPORT_VARIABLE_RESOLUTION
				stRotInfo.src_resolution.width = 768;
				stRotInfo.src_resolution.height = 576;
			#else
				stRotInfo.src_resolution = S_768X576;
			#endif
				stRotInfo.rotation		= stRotInfo.rotation;

				do_gettimeofday( &t1 );
				ret = v2d_proc_rotation( v2d->reg_vaddr, stRotInfo);
				if(ret >= 0){
					_v2d_wait_done(v2d);
				}
				do_gettimeofday( &t2 );
				time_us = _v2d_GetTimediff_us(t1, t2);
				printk("%s-%d :: %d'th Proc-time => %d.%3d ms \n", __func__, __LINE__, cnt, time_us/1000, time_us%1000);

				nDifference = 0;
				for(i=0; i<count; i++){
					if(proc_data[i] != gd_buffer[i])
						nDifference += 1;
				}
/*
				for(i=0; i<768*576; i+=20){
					printk("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,\n",
						proc_data[i+0], proc_data[i+1], proc_data[i+2], proc_data[i+3], proc_data[i+4], proc_data[i+5], proc_data[i+6],
						proc_data[i+7], proc_data[i+8], proc_data[i+9], proc_data[i+10], proc_data[i+11], proc_data[i+12], proc_data[i+13],
						proc_data[i+14], proc_data[i+15], proc_data[i+16],	proc_data[i+17], proc_data[i+18], proc_data[i+19]);
				}
*/
				dprintk("%d - %s-%d :: Cmd(0x%x) Src(0x%x) Dst(0x%x) Rot(%d) => Result(0x%x) @@@@@@@@@@ nDifference count = %d / %d \n",
							cnt, __func__, __LINE__,
							pmap_rot.base + cmd_offset,
							pmap_rot.base + src_offset,
							pmap_rot.base + dst_offset,
							stRotInfo.rotation,
							*((unsigned int *)(v2d->reg_vaddr + 0x4/*AQHiIdle*/)), nDifference, count);

				cnt++;
				if(cnt < max_cnt){
					goto TEST_LOOP;
				}

				iounmap((void*)remap_rot);
			}
			else
			{
				printk("%s-%d :: cmd(0x%x) phy[0x%p - 0x%x] remap ALLOC_MEMORY failed.\n", __func__, __LINE__, cmd, pmap_rot.base, pmap_rot.size);
			}
		}
		break;

		case 0x1002:
		{
			int i = 0;
			pmap_t pmap_cmd, pmap_src, pmap_dst;
			unsigned int *gd_buffer = NULL;
			unsigned char *remap_src = NULL, *remap_dst = NULL;
			unsigned int args_rot[7];
			int cnt = 0;
			int max_cnt = 100;
			struct timeval t1 , t2;
			unsigned long time_us;

//arg[0] = cmd addr (1MB)
//arg[1] = src addr (2MB)
//arg[2] = dst addr (2MB)
//arg[3] = rotation type
//arg[4] = looping count
//arg[5] = width
//arg[6] = height

			if(copy_from_user(&args_rot,(int*)arg, sizeof(unsigned int) * 7)){
				ret = -EFAULT;
				goto Error;
			}

			printk("%s-%d :: Cmd(0x%x) Src(0x%x) Dst(0x%x) Rot(%d) loop(0x%x) Res(%d x %d) \n", __func__, __LINE__,
							args_rot[0], args_rot[1], args_rot[2],
							args_rot[3], args_rot[4], args_rot[5], args_rot[6]);

			pmap_cmd.base = args_rot[0];
			pmap_cmd.size = 0x100000;
			pmap_src.base = args_rot[1];
			pmap_src.size = 0x200000;
			pmap_dst.base = args_rot[2];
			pmap_dst.size = 0x200000;
			stRotInfo.rotation = args_rot[3];
			max_cnt = args_rot[4];

		#ifdef V2D_768X576_ROT_90
			if(stRotInfo.rotation == R_90)
				gd_buffer = R90_gd_buffer;
			else
		#endif
		#ifdef V2D_768X576_ROT_270
			if(stRotInfo.rotation == R_270)
				gd_buffer = R270_gd_buffer;
			else
		#endif
		#ifdef V2D_768X576_ROT_HFLIP
			if(stRotInfo.rotation == R_H_FLIP)
				gd_buffer = hflip_gd_buffer;
			else
		#endif
		#ifdef V2D_768X576_ROT_VFLIP
			if(stRotInfo.rotation == R_V_FLIP)
				gd_buffer = vflip_gd_buffer;
			else
		#endif
			{
				printk("%s-%d :: error strange rotation(%d) \n", __func__, __LINE__, stRotInfo.rotation);
				ret = -EFAULT;
				goto Error;
			}

			remap_src = (unsigned char*)ioremap_nocache((phys_addr_t)pmap_src.base, pmap_src.size);
			remap_dst = (unsigned char*)ioremap_nocache((phys_addr_t)pmap_dst.base, pmap_dst.size);
			if(remap_src && remap_dst)
			{
				unsigned int *proc_data = NULL;
				unsigned int count = sizeof(src_buffer)/sizeof(unsigned int);
				unsigned int nDifference = 0;

TEST_LOOP1:
				memcpy((void*)remap_src, src_buffer, sizeof(src_buffer));
				proc_data = (unsigned int *)remap_dst;
				memset((void*)proc_data, 0x00, sizeof(src_buffer));

				stRotInfo.cmd_phy_addr 	= pmap_cmd.base;
				stRotInfo.cmd_len		= pmap_cmd.size;
				stRotInfo.src_phy_addr	= pmap_src.base;
				stRotInfo.dst_phy_addr	= pmap_dst.base;
			#ifdef SUPPORT_VARIABLE_RESOLUTION
				stRotInfo.src_resolution.width = args_rot[5];//768;
				stRotInfo.src_resolution.height = args_rot[6];//576;
			#else
				stRotInfo.src_resolution = S_768X576;
			#endif
				stRotInfo.rotation		= stRotInfo.rotation;

				do_gettimeofday( &t1 );
				ret = v2d_proc_rotation( v2d->reg_vaddr, stRotInfo);
				if(ret >= 0)
					_v2d_wait_done(v2d);
				do_gettimeofday( &t2 );
				time_us = _v2d_GetTimediff_us(t1, t2);

				if(time_us > 10000)
					printk("%s-%d :: %d'th Proc-time => %d.%3d ms >                       10000\n", __func__, __LINE__, cnt, time_us/1000, time_us%1000);
				else if(time_us > 9000)
					printk("%s-%d :: %d'th Proc-time => %d.%3d ms >                   9000\n", __func__, __LINE__, cnt, time_us/1000, time_us%1000);
				else if(time_us > 8000)
					printk("%s-%d :: %d'th Proc-time => %d.%3d ms >               8000\n", __func__, __LINE__, cnt, time_us/1000, time_us%1000);
				else if(time_us > 7000)
					printk("%s-%d :: %d'th Proc-time => %d.%3d ms >           7000\n", __func__, __LINE__, cnt, time_us/1000, time_us%1000);
				else if(time_us > 6000)
					printk("%s-%d :: %d'th Proc-time => %d.%3d ms >      6000\n", __func__, __LINE__, cnt, time_us/1000, time_us%1000);
				else if(time_us > 5000)
					printk("%s-%d :: %d'th Proc-time => %d.%3d ms > 5000\n", __func__, __LINE__, cnt, time_us/1000, time_us%1000);

				nDifference = 0;
				for(i=0; i<count; i++){
					if(proc_data[i] != gd_buffer[i])
						nDifference += 1;
				}

				dprintk("%s-%d :: Cmd(0x%x) Src(0x%x) Dst(0x%x) Rot(%d) => Result(0x%x) @@@@@@@@@@ nDifference count = %d / %d \n", __func__, __LINE__,
							pmap_cmd.base, pmap_src.base, pmap_dst.base,
							stRotInfo.rotation,
							*((unsigned int *)(v2d->reg_vaddr + 0x4/*AQHiIdle*/)), nDifference, count);

				cnt++;
				if(cnt < max_cnt){
					goto TEST_LOOP1;
				}

				iounmap((void*)remap_src);
				iounmap((void*)remap_dst);
			}
			else
			{
				printk("%s-%d :: cmd(0x%x) phy[0x%p / 0x%x] remap ALLOC_MEMORY failed.\n", __func__, __LINE__, cmd, pmap_src.base, pmap_dst.base);
			}
		}
		break;
#endif

		default:
			printk("Unsupported cmd(0x%x) for v2d_ioctl. \n", cmd);
			ret = -EFAULT;
			break;
	}

Error:
	mutex_unlock(&v2d->data->io_mutex);

#if defined(TEST_V2D_768X576_ROT) && !defined(CLK_ALWAYS_ON)
	mdelay(100);
#endif

	return ret;
}

static int v2d_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct v2d_drv_type	*v2d = dev_get_drvdata(misc->parent);

#if !defined(CLK_ALWAYS_ON)
#ifdef CONFIG_OF
#ifdef CONFIG_ARCH_TCC897X
	if(v2d->v2d_ddi_clk){
    	clk_disable_unprepare(v2d->v2d_ddi_clk);
		dprintk("%s-%d :: 2d-clk with ddi off\n", __func__, __LINE__);
	}
#endif
	if(v2d->v2d_clk){
		clk_disable_unprepare(v2d->v2d_clk);
		dprintk("%s-%d :: 2d-clk off\n", __func__, __LINE__);
	}
#endif
#endif

	dprintk("%s-%d \n", __func__, __LINE__);
	
	return 0;
}

static int v2d_drv_open(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct v2d_drv_type	*v2d = dev_get_drvdata(misc->parent);

#if !defined(CLK_ALWAYS_ON)
#ifdef CONFIG_OF
	if(v2d->v2d_clk){
		clk_prepare_enable(v2d->v2d_clk);
		dprintk("%s-%d :: 2d-clk on\n", __func__, __LINE__);
	}
#ifdef CONFIG_ARCH_TCC897X
    if(v2d->v2d_ddi_clk){
    	clk_prepare_enable(v2d->v2d_ddi_clk);
		dprintk("%s-%d :: 2d-clk with ddi on\n", __func__, __LINE__);
    }
#endif
#endif
#endif

	dprintk("%s-%d \n", __func__, __LINE__);
	
	return 0;
}

static struct file_operations v2d_drv_fops = {
	.owner				= THIS_MODULE,
	.unlocked_ioctl		= v2d_drv_ioctl,
	.mmap				= v2d_drv_mmap,
	.open				= v2d_drv_open,
	.release			= v2d_drv_release,
	.poll				= v2d_drv_poll,
};

static int v2d_drv_probe(struct platform_device *pdev)
{
	struct v2d_drv_type *v2d;
#ifdef CONFIG_OF
	struct device_node *dnp = pdev->dev.of_node; /* device node */
#endif
	int ret = -ENODEV;

	dprintk("%s-%d \n", __func__, __LINE__);

	v2d = kzalloc(sizeof(struct v2d_drv_type), GFP_KERNEL);
	if (!v2d)
		return -ENOMEM;

	v2d->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (v2d->misc == 0){
		ret = -1;
		goto err_misc_alloc;
	}

	v2d->data = kzalloc(sizeof(struct v2d_data), GFP_KERNEL);
	if (v2d->data == 0){
		ret = -2;
		goto err_data_alloc;
	}

 	/* register v2d discdevice */
	v2d->misc->minor = MISC_DYNAMIC_MINOR;
	v2d->misc->fops = &v2d_drv_fops;
	v2d->misc->name = DEVICE_NAME;
	v2d->misc->parent = &pdev->dev;
	ret = misc_register(v2d->misc);
	if (ret){
		ret = -3;
		goto err_misc_register;
	}

	v2d->id = of_alias_get_id(dnp, "gc");

#ifdef CONFIG_OF
	v2d->irq = platform_get_irq(pdev, 0);
	if(!v2d->irq){
		ret = -4;
		goto err_data_alloc;
	}
	v2d->reg_vaddr = of_iomap(dnp, 0);
	if(!v2d->irq){
		ret = -5;
		goto err_data_alloc;
	}

	v2d->v2d_clk = of_clk_get(dnp, 0);
	if(v2d->v2d_clk == NULL){
		ret = -6;
		goto err_start_g2d;
	}
	#if defined(CLK_ALWAYS_ON)	
	clk_prepare_enable(v2d->v2d_clk);
	#endif

#ifdef CONFIG_ARCH_TCC897X
    v2d->v2d_ddi_clk = of_clk_get(dnp, 1);
    if(v2d->v2d_ddi_clk == NULL){
		ret = -7;
		goto err_start_g2d;
    }
	#if defined(CLK_ALWAYS_ON)
    clk_prepare_enable(v2d->v2d_ddi_clk);
	#endif
	dprintk("%s-%d :: irq(%d) / reg(0x%x) / clk(0x%x/0x%x) \n", __func__, __LINE__, v2d->irq, v2d->reg_vaddr, v2d->v2d_clk, v2d->v2d_ddi_clk);
#else
	dprintk("%s-%d :: irq(%d) / reg(0x%x) / clk(0x%x/0x%x) \n", __func__, __LINE__, v2d->irq, v2d->reg_vaddr, v2d->v2d_clk, NULL);
#endif
#endif

	spin_lock_init(&(v2d->data->poll_lock));
	spin_lock_init(&(v2d->data->cmd_lock));

	mutex_init(&(v2d->data->io_mutex));
	
	init_waitqueue_head(&(v2d->data->poll_wq));
	init_waitqueue_head(&(v2d->data->cmd_wq));

	platform_set_drvdata(pdev, v2d);

	return 0;

err_start_g2d:
	misc_deregister(v2d->misc);

err_misc_register:
	kfree(v2d->data);
err_data_alloc:
	kfree(v2d->misc);
err_misc_alloc:
	kfree(v2d);

	printk("%s: %s: error => ret:%d \n", __func__, pdev->name, ret);
	return ret;
}

static int v2d_drv_remove(struct platform_device *pdev)
{
	struct v2d_drv_type *v2d = (struct v2d_drv_type *)platform_get_drvdata(pdev);
#ifdef CONFIG_OF
	struct device_node *dnp = pdev->dev.of_node; /* device node */
#endif

	dprintk("%s-%d \n", __func__, __LINE__);

#if defined(CLK_ALWAYS_ON)
#ifdef CONFIG_OF
	#ifdef CONFIG_ARCH_TCC897X
	{
		if(v2d->v2d_ddi_clk != NULL)
			clk_disable_unprepare(v2d->v2d_ddi_clk);
	}
	#endif
	if(v2d->v2d_clk != NULL)
		clk_disable_unprepare(v2d->v2d_clk);
#endif
#endif

	misc_deregister(v2d->misc);
	kfree(v2d->data);	
	kfree(v2d->misc);
	kfree(v2d);
	return 0;
}

static int v2d_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct v2d_drv_type *v2d = (struct v2d_drv_type *)platform_get_drvdata(pdev);
	
	dprintk("%s-%d \n", __func__, __LINE__);

	return 0;
}

static int v2d_drv_resume(struct platform_device *pdev)
{
	struct v2d_drv_type *v2d = (struct v2d_drv_type *)platform_get_drvdata(pdev);

	dprintk("%s-%d \n", __func__, __LINE__);

	return 0;
}

static struct of_device_id v2d_of_match[] = {
	{ .compatible = "vivante.gc" },
	{}
};
MODULE_DEVICE_TABLE(of, v2d_of_match);

static struct platform_driver v2d_driver = {
	.probe		= v2d_drv_probe,
	.remove		= v2d_drv_remove,
	.suspend	= v2d_drv_suspend,
	.resume		= v2d_drv_resume,
	.driver 	= {
		.name	= "v2d_pdev",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(v2d_of_match),
#endif
	},
};

static int __init v2d_drv_init(void)
{	
	dprintk("%s-%d \n", __func__, __LINE__);

	return platform_driver_register(&v2d_driver);
}

static void __exit v2d_drv_exit(void)
{	
	dprintk("%s-%d \n", __func__, __LINE__);
	
	platform_driver_unregister(&v2d_driver);
}

module_init(v2d_drv_init);
module_exit(v2d_drv_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips v2d Driver");
MODULE_LICENSE("GPL");


