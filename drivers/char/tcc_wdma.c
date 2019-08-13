/*
 * drivers/char/tcc_wdma.c
 *
 * Copyright (C) 2004 Texas Instruments, Inc. 
 *
 * Video-for-Linux (Version 2) graphic capture driver for
 * the OMAP H2 and H3 camera controller.
 *
 * Adapted from omap24xx driver written by Andy Lowe (source@mvista.com)
 * Copyright (C) 2003-2004 MontaVista Software, Inc.
 * 
 * This package is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. 
 * 
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED 
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 *
 * History:
 *   27/03/05   Vladimir Barinov - Added support for power management
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#include <asm/io.h>
#include <asm/div64.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <soc/tcc/pmap.h>
#include <video/tcc/irqs.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_wdma_ioctrl.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tca_display_config.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>

#define WDMA_DEBUG 	0
#define dprintk(msg...) 	if(WDMA_DEBUG) { printk("WDMA: " msg); }
// to debug wdma image : image buffer set to 0x12 before writing the image data by WDMA block.
//#define WDMA_IMAGE_DEBUG
#define CHECKING_NUM 					(0x12)
#define CHECKING_START_POS(x, y)		(x * (y/3))
#define CHECKING_AREA(x, y)				(x*(y/10))

enum {
	PREPARED_S,
	WRITING_S,
	WRITED_S,
	UPLOAD_S,
	WDMA_S_MAX
};

struct wdma_buffer_list {
	unsigned int status;
	unsigned int base_Yaddr;
	unsigned int base_Uaddr;
	unsigned int base_Vaddr;
	char *vbase_Yaddr;
};

struct wdma_queue_list {
	unsigned int q_max_cnt;
	unsigned int q_index;		//curreunt writing buffer index
	struct wdma_buffer_list *wbuf_list;

	struct vioc_wdma_frame_info *data;
};

struct tcc_wdma_dev_vioc {
	volatile void __iomem *reg;
	unsigned int id;
	//unsigned int path;
};

struct tcc_wdma_dev{
	struct vioc_intr_type	*vioc_intr;
	unsigned int		irq;

	struct miscdevice	*misc;

	struct tcc_vioc_block 		disp_info;
	struct tcc_wdma_dev_vioc   	rdma;
	struct tcc_wdma_dev_vioc	sc;
	struct tcc_wdma_dev_vioc   	wdma;
	struct tcc_wdma_dev_vioc   	wmix;	
	// wait for poll  
	wait_queue_head_t 	poll_wq;
	spinlock_t 			poll_lock;
	unsigned int 			poll_count;

	// wait for ioctl command
	wait_queue_head_t 	cmd_wq;
	spinlock_t 			cmd_lock;
	unsigned int 			cmd_count;

	struct mutex 			io_mutex;
	unsigned char 		block_operating;
	unsigned char 		block_waiting;
	unsigned char 		irq_reged;
	unsigned int  			dev_opened;

	unsigned char			wdma_contiuous;
	struct wdma_queue_list frame_list;
	struct clk *wdma_clk;

	//extend infomation
	unsigned int fb_dd_num;
};


extern void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr, unsigned int src_imgx, unsigned int  src_imgy,
									unsigned int start_x, unsigned int start_y, unsigned int* Y, unsigned int* U,unsigned int* V);

extern int range_is_allowed(unsigned long pfn, unsigned long size);
static int tccxxx_wdma_mmap(struct file *file, struct vm_area_struct *vma)
{
	if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		printk(KERN_ERR	"wdma: this address is not allowed. \n");
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops		=  NULL;
	vma->vm_flags 	|= VM_IO;
	vma->vm_flags 	|= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}




unsigned int wdma_queue_list_init(struct wdma_queue_list *frame_list, struct vioc_wdma_frame_info *cmd_data)
{
	unsigned int i, frame_size;
	unsigned int addrY= 0, addrU= 0, addrV= 0; 

	frame_list->data = kmalloc(sizeof(struct vioc_wdma_frame_info ), GFP_KERNEL);

	if(!frame_list->data)
	{
		printk(KERN_ERR "wdma queue list kmalloc() failed!\n");
		return -1;
	}

	*frame_list->data = *cmd_data;

	// Frame format is set to  TCC_LCDC_IMG_FMT_YUV420SP.
	frame_size = (cmd_data->frame_x * cmd_data->frame_y * 3) /2;

	frame_list->data->buffer_num = frame_list->q_max_cnt =  cmd_data->buff_size / frame_size;
	frame_list->q_index = frame_list->q_max_cnt;
	
	frame_list->wbuf_list = kmalloc(frame_list->q_max_cnt * sizeof(struct wdma_buffer_list), GFP_KERNEL);

	if(!frame_list->wbuf_list)	{
		pr_err("list alloc error \n");
		goto list_alloc_fail;
	}
	
	for(i = 0; i < frame_list->q_max_cnt; i++)
	{
		unsigned int base_addr;
		frame_list->wbuf_list[i].status = PREPARED_S;
		base_addr = cmd_data->buff_addr + (i * frame_size);
		addrY = 0; addrU = 0; addrV = 0;
		tccxxx_GetAddress(cmd_data->frame_fmt, base_addr, cmd_data->frame_x, cmd_data->frame_y,
		 						0, 0, &addrY, &addrU,&addrV);

		frame_list->wbuf_list[i].base_Yaddr = addrY;
		frame_list->wbuf_list[i].base_Uaddr = addrU;
		frame_list->wbuf_list[i].base_Vaddr = addrV;
		#ifdef WDMA_IMAGE_DEBUG        
		frame_list->wbuf_list[i].vbase_Yaddr = (char *)ioremap_nocache(addrY, cmd_data->frame_x * cmd_data->frame_y);
		#endif//
		dprintk("id : %3d addr : 0x%08x 0x%08x 0x%08x  \n", i, frame_list->wbuf_list[i].base_Yaddr, frame_list->wbuf_list[i].base_Uaddr, frame_list->wbuf_list[i].base_Vaddr);
	}

	dprintk("start list :size : %d x %d  buffer_number:%d \n", frame_list->data->frame_y, frame_list->data->frame_x, frame_list->data->buffer_num);

	return frame_list->q_max_cnt;

list_alloc_fail:
	kfree(frame_list->data);
	printk(KERN_ERR "wdma queue list kmalloc() failed!\n");
	return -1;
}

unsigned int wdma_queue_list_exit(struct wdma_queue_list *frame_list)
{
	if(frame_list->wbuf_list)
		kfree(frame_list->wbuf_list);

	if(frame_list->data)
		kfree(frame_list->data);

	return 0;
}

unsigned int wdma_get_writable_buffer_index(struct wdma_queue_list *frame_list)
{
	int i, cur_index = 0;

	cur_index = frame_list->q_index;

	for(i = 0; i < frame_list->q_max_cnt; i++)
	{
		cur_index++;
		if(cur_index >= frame_list->q_max_cnt)
			cur_index = 0;
		
		if((frame_list->wbuf_list[cur_index].status == PREPARED_S) || (frame_list->wbuf_list[cur_index].status == WRITED_S))
		{
			break;
		}
	}

	return cur_index;
}

// find index of wdma_s status buffer index.
int wdma_find_index_of_status(struct wdma_queue_list *frame_list, unsigned int wdma_s)
{
	int i, find_index = 0;
	if(wdma_s >= WDMA_S_MAX)
		return -1;

	if(wdma_s == WRITED_S)
	{
		if(frame_list->q_index == 0)
			find_index = frame_list->q_max_cnt -2;
		else if(frame_list->q_index == 1)
			find_index = frame_list->q_max_cnt -1;
		else
			find_index = frame_list->q_index -2;
	}
	else
		find_index = 0;
		
	for(i = 0; i <= frame_list->q_max_cnt; i++)
	{
		if(frame_list->wbuf_list[find_index].status == wdma_s)
			break;

		if(find_index == 0)	
			find_index = frame_list->q_max_cnt - 1;
		else
			find_index--;
	}

	if(i >= frame_list->q_max_cnt)
		return -1;
	
	return find_index;
}
// set status 
int wdma_set_index_of_status(struct wdma_queue_list *frame_list, unsigned int index, unsigned int wdma_s)
{
	if((wdma_s >= WDMA_S_MAX) ||(frame_list->q_max_cnt <= index))
	{
		return -1;
	}
	
	frame_list->wbuf_list[index].status = wdma_s;
	if(wdma_s == WRITING_S)
		frame_list->q_index = index;
	
	return 0;
}



int tc_wdrv_scaler_set(struct tcc_wdma_dev *pwdma_data, unsigned int out_w, unsigned int out_h)
{
	struct lcd_panel *panel;
	panel = tccfb_get_panel();

	/* scaler setting */
	if((panel->xres == out_w) || (panel->yres == out_h))
		VIOC_SC_SetBypass(pwdma_data->sc.reg, 1);
	else
		VIOC_SC_SetBypass(pwdma_data->sc.reg, 0);
	VIOC_SC_SetDstSize(pwdma_data->sc.reg, out_w, out_h);
	VIOC_SC_SetOutSize(pwdma_data->sc.reg, out_w, out_h);
	VIOC_SC_SetOutPosition(pwdma_data->sc.reg, 0, 0);
	VIOC_CONFIG_PlugIn(pwdma_data->sc.id, pwdma_data->wdma.id);
	VIOC_SC_SetUpdate(pwdma_data->sc.reg);

	return 0;
}

char tc_wdrv_wdma_path_set(struct tcc_wdma_dev *pwdma_data, struct vioc_wdma_frame_info *data_info, struct wdma_buffer_list *buffer)
{
	struct lcd_panel *panel;
	int dd_rgb = 0;
	struct DisplayBlock_Info DDinfo;	
	
	panel = tccfb_get_panel();
	
	printk("%s pwdma_data:%p , data_info: %p, buffer: %p ...panel:%p \n", __func__, pwdma_data, data_info, buffer, panel);
	spin_lock_irq(&(pwdma_data->cmd_lock));

	buffer->status = WRITING_S;

	if(pwdma_data->fb_dd_num)	{
		VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 1 /* Mixing */);
		VIOC_CONFIG_WMIXPath(VIOC_RDMA07, 1 /* Mixing */); 
	}
	else 	{
		VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 1 /* Mixing */);
		VIOC_CONFIG_WMIXPath(VIOC_RDMA03, 1 /* Mixing */);
	}
	VIOC_WDMA_SetImageSize(pwdma_data->wdma.reg, data_info->frame_x, data_info->frame_y);
	VIOC_WDMA_SetImageFormat(pwdma_data->wdma.reg, data_info->frame_fmt);
	VIOC_WDMA_SetImageOffset(pwdma_data->wdma.reg, data_info->frame_fmt, data_info->frame_x);
	VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg, buffer->base_Yaddr, buffer->base_Uaddr, buffer->base_Vaddr);
	VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);

	// check display device format
	VIOC_DISP_GetDisplayBlock_Info(pwdma_data->disp_info.virt_addr, &DDinfo);
	dd_rgb = VIOC_DISP_FMT_isRGB(DDinfo.pCtrlParam.pxdw);
	
	if(data_info->frame_fmt <= TCC_LCDC_IMG_FMT_ARGB6666_3) 
	{
		if(dd_rgb) 
		{
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		}
		else
		{
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		}
	}
	else
	{
		if(dd_rgb) 
		{
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		}
		else
		{
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		}
	}

	VIOC_WDMA_SetImageRGBSwapMode(pwdma_data->wdma.reg, 0);
	VIOC_WDMA_SetImageEnable(pwdma_data->wdma.reg, pwdma_data->wdma_contiuous);
	spin_unlock_irq(&(pwdma_data->cmd_lock));

	return 0;
}


char tc_wdrv_wdma_addr_set(struct tcc_wdma_dev *pwdma_data, unsigned int addrY, unsigned int addrU, unsigned int addrV)
{
//	dprintk("0x%x 0x%x 0x%x \n",addrY, addrU, addrV);
	VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg, addrY, addrU, addrV);
	VIOC_WDMA_SetImageUpdate(pwdma_data->wdma.reg);
	return 0;
}


char tccxxx_wdma_ctrl(unsigned long argp, struct tcc_wdma_dev *pwdma_data)
{
	struct lcd_panel *panel;
	VIOC_WDMA_IMAGE_INFO_Type ImageCfg;
	unsigned int base_addr = 0, Wmix_Height = 0, Wmix_Width = 0, DDevice = 0;
	int ret = 0, dd_rgb = 0;
	int addr_Y,  addr_U, addr_V;
	struct DisplayBlock_Info DDinfo;

	memset(&ImageCfg, 0x00, sizeof(ImageCfg));
	panel = tccfb_get_panel();

	if(copy_from_user((void *)&ImageCfg, (const void __user *)argp, sizeof(ImageCfg))){
		pr_err("### %s error \n",__func__);
		return -EFAULT;
	}

	ImageCfg.Interlaced = 0;
	ImageCfg.ContinuousMode = 0;
	ImageCfg.SyncMode = 0;
	ImageCfg.ImgSizeWidth = panel->xres;
	ImageCfg.ImgSizeHeight = panel->yres;

	base_addr = (unsigned int )ImageCfg.BaseAddress;
	
	tccxxx_GetAddress(ImageCfg.ImgFormat, (unsigned int)base_addr, ImageCfg.TargetWidth, ImageCfg.TargetHeight,
	 						0, 0, &addr_Y, &addr_U,&addr_V);

	if(ImageCfg.ImgFormat == TCC_LCDC_IMG_FMT_YUV420SP || ImageCfg.ImgFormat == TCC_LCDC_IMG_FMT_RGB888)
	{
		addr_U = GET_ADDR_YUV42X_spU(base_addr, ImageCfg.TargetWidth, ImageCfg.TargetHeight);

		if(ImageCfg.ImgFormat == TCC_LCDC_IMG_FMT_YUV420SP)
			addr_V = GET_ADDR_YUV420_spV(addr_U, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
		else
			addr_V = GET_ADDR_YUV422_spV(addr_U, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
	}
	ImageCfg.BaseAddress = addr_Y;
	ImageCfg.BaseAddress1 = addr_U;
	ImageCfg.BaseAddress2 = addr_V;

	
	VIOC_WMIX_GetSize(pwdma_data->wmix.reg, &Wmix_Width, &Wmix_Height);

	DDevice = VIOC_DISP_Get_TurnOnOff(pwdma_data->disp_info.virt_addr);
	if((Wmix_Width ==0) || (Wmix_Height ==0) || (DDevice == 0))
	{
		pwdma_data->block_operating = 0;
		printk("Error tccxxx_wdma_ctrl W:%d H:%d DD-Power:%d \n", Wmix_Width, Wmix_Height, DDevice);
		return 0;
	}

	dprintk("src  w:%d h:%d base:0x%08x  \n",ImageCfg.ImgSizeWidth,ImageCfg.ImgSizeHeight,ImageCfg.BaseAddress);
	dprintk("dest w:%d h:%d fb_dd_num:%d  \n",ImageCfg.TargetWidth,ImageCfg.TargetHeight, pwdma_data->fb_dd_num);
 	dprintk("wmixer size  %d %d  : base1:0x%08x  base2:0x%08x  \n",Wmix_Width, Wmix_Height, ImageCfg.BaseAddress1, ImageCfg.BaseAddress2);
	dprintk("ImgFormat:%d  set effect : hue:%d , bright:%d contrast:%d \n",ImageCfg.ImgFormat, ImageCfg.Hue, ImageCfg.Bright, ImageCfg.Contrast);

	if(pwdma_data->sc.reg) {
		VIOC_SC_SetBypass(pwdma_data->sc.reg, 0);
		VIOC_SC_SetDstSize(pwdma_data->sc.reg, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
		VIOC_SC_SetOutSize(pwdma_data->sc.reg, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
		VIOC_SC_SetOutPosition(pwdma_data->sc.reg, 0, 0);
		VIOC_CONFIG_PlugIn(pwdma_data->sc.id, pwdma_data->wdma.id);
		VIOC_SC_SetUpdate(pwdma_data->sc.reg);
	}

	spin_lock_irq(&(pwdma_data->cmd_lock));

	if(pwdma_data->fb_dd_num) {
		VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 1 /* Mixing */);
		VIOC_CONFIG_WMIXPath(VIOC_RDMA07, 1 /* Mixing */); 
	}
	else {
		VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 1 /* Mixing */);
		VIOC_CONFIG_WMIXPath(VIOC_RDMA03, 1 /* Mixing */);
	}

	// check display device format
	VIOC_DISP_GetDisplayBlock_Info(pwdma_data->disp_info.virt_addr, &DDinfo);
	dd_rgb = VIOC_DISP_FMT_isRGB(DDinfo.pCtrlParam.pxdw);

	if(ImageCfg.ImgFormat <= TCC_LCDC_IMG_FMT_ARGB6666_3) 
	{
		if(dd_rgb) 
		{
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		}
		else
		{
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		}
	}
	else
	{
		if(dd_rgb) 
		{
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		}
		else
		{
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		}
	}

	VIOC_WDMA_SetImageSize(pwdma_data->wdma.reg, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
	VIOC_WDMA_SetImageFormat(pwdma_data->wdma.reg, ImageCfg.ImgFormat);
	VIOC_WDMA_SetImageOffset(pwdma_data->wdma.reg, ImageCfg.ImgFormat, ImageCfg.TargetWidth);
	VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg, ImageCfg.BaseAddress, ImageCfg.BaseAddress1, ImageCfg.BaseAddress2);
	VIOC_WDMA_SetImageRGBSwapMode(pwdma_data->wdma.reg, 0);
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	VIOC_WDMA_SetImageEnhancer(pwdma_data->wdma.reg, ImageCfg.Contrast, ImageCfg.Bright, ImageCfg.Hue);
	#endif
	VIOC_WDMA_SetImageEnable(pwdma_data->wdma.reg, ImageCfg.ContinuousMode);

	spin_unlock_irq(&(pwdma_data->cmd_lock));
	pwdma_data->vioc_intr->bits = (1 << VIOC_WDMA_INTR_EOFR);
	vioc_intr_clear(pwdma_data->vioc_intr->id, pwdma_data->vioc_intr->bits);
	vioc_intr_enable(pwdma_data->irq, pwdma_data->vioc_intr->id, pwdma_data->vioc_intr->bits);
	ret = wait_event_interruptible_timeout(pwdma_data->poll_wq, pwdma_data->block_operating == 0, msecs_to_jiffies(100));
	if(ret <= 0) {
		pwdma_data->block_operating = 0;
		printk("wdma time out: %d, Line: %d. \n", ret, __LINE__);
	}

	if(pwdma_data->sc.reg)
		VIOC_CONFIG_PlugOut(pwdma_data->sc.id); 

	if (copy_to_user((void __user *)argp, &ImageCfg, sizeof(ImageCfg))) {
		return -EFAULT;
	}
	
	return ret;
}

static unsigned int tccxxx_wdma_poll(struct file *filp, poll_table *wait)
{
	int ret = 0;

	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct tcc_wdma_dev *wdma_data = dev_get_drvdata(misc->parent);
	
	if(wdma_data == NULL) 	return -EFAULT;

	poll_wait(filp, &(wdma_data->poll_wq), wait);

	spin_lock_irq(&(wdma_data->poll_lock));

	if(wdma_data->block_operating == 0) 	ret = (POLLIN|POLLRDNORM);

	spin_unlock_irq(&(wdma_data->poll_lock));

	return ret;
}

static irqreturn_t tccxxx_wdma_handler(int irq, void *client_data)
{  	
	struct tcc_wdma_dev *wdma_data = client_data;

	if (!is_vioc_intr_activatied(wdma_data->vioc_intr->id, wdma_data->vioc_intr->bits)) {
		return IRQ_NONE;
	}

	if(wdma_data->block_operating >= 1)
		wdma_data->block_operating = 0;

	wake_up_interruptible(&(wdma_data->poll_wq));

	if(wdma_data->block_waiting)
		wake_up_interruptible(&wdma_data->cmd_wq);


	if(wdma_data->wdma_contiuous)
	{
		unsigned int next_id, cur_id;
		cur_id = wdma_find_index_of_status(&wdma_data->frame_list, WRITING_S);
		next_id = wdma_get_writable_buffer_index(&wdma_data->frame_list);
		tc_wdrv_wdma_addr_set(wdma_data, wdma_data->frame_list.wbuf_list[next_id].base_Yaddr, wdma_data->frame_list.wbuf_list[next_id].base_Uaddr, wdma_data->frame_list.wbuf_list[next_id].base_Vaddr);
		wdma_set_index_of_status(&wdma_data->frame_list, next_id, WRITING_S);
		wdma_set_index_of_status(&wdma_data->frame_list, cur_id, WRITED_S);	
		
		#ifdef WDMA_IMAGE_DEBUG
		memset(wdma_data->frame_list.wbuf_list[next_id].vbase_Yaddr + CHECKING_START_POS(wdma_data->frame_list.data->frame_x , wdma_data->frame_list.data->frame_y), CHECKING_NUM, CHECKING_AREA(wdma_data->frame_list.data->frame_x , wdma_data->frame_list.data->frame_y));
		#endif
#if 0
		if(wdma_data->block_waiting){
			printk("   handler index :C %d  N:%d  addr: Y:0x%08x  U:0x%08x  V:0x%08x \n", 
			cur_id, next_id,
			wdma_data->frame_list.wbuf_list[next_id].base_Yaddr,
			wdma_data->frame_list.wbuf_list[next_id].base_Uaddr,
			wdma_data->frame_list.wbuf_list[next_id].base_Vaddr);
		}
#endif//		
	}
	vioc_intr_clear(wdma_data->vioc_intr->id, VIOC_WDMA_INT_MASK);

	return IRQ_HANDLED;
}

long tccxxx_wdma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct miscdevice	*misc = (struct miscdevice *)file->private_data;
	struct tcc_wdma_dev *wdma_data = dev_get_drvdata(misc->parent);

 	dprintk("wdma:  cmd(0x%x), block_operating(0x%x), block_waiting(0x%x), cmd_count(0x%x), poll_count(0x%x). \n", 	\
 					cmd, wdma_data->block_operating, wdma_data->block_waiting, wdma_data->cmd_count, wdma_data->poll_count);
	switch(cmd) {


		case 0x100:
			{
				struct vioc_wdma_frame_info cmd_data;
				pmap_t pmap_fb_video;
				pmap_get_info("overlay", &pmap_fb_video);
				cmd_data.buff_addr = pmap_fb_video.base;
				cmd_data.buff_size = pmap_fb_video.size;
				cmd_data.frame_fmt = TCC_LCDC_IMG_FMT_YUV420SP;
				cmd_data.frame_x = 800;
				cmd_data.frame_y = 480;
				cmd_data.buffer_num = wdma_queue_list_init(&wdma_data->frame_list, &cmd_data);
				wdma_data->wdma_contiuous = 1;

				if(wdma_data->sc.reg)
					tc_wdrv_scaler_set(wdma_data, cmd_data.frame_x, cmd_data.frame_y);
				wdma_data->frame_list.q_index = 0;
				tc_wdrv_wdma_path_set(wdma_data, &cmd_data, (struct wdma_buffer_list *)&wdma_data->frame_list.wbuf_list[0]);

				wdma_data->dev_opened++;
				return 0;
			}
			break;

		case 0x101:
			{
				struct vioc_wdma_get_buffer wbuffer;
				 int before_id;
				memset(&wbuffer, 0, sizeof(wbuffer));
				
				mutex_lock(&wdma_data->io_mutex);
				spin_lock_irq(&(wdma_data->cmd_lock));
				
				before_id = wdma_find_index_of_status(&wdma_data->frame_list, UPLOAD_S);
				wbuffer.index = wdma_find_index_of_status(&wdma_data->frame_list, WRITED_S);
				
				if(wbuffer.index >= 0)
				{
					wbuffer.buff_Yaddr = wdma_data->frame_list.wbuf_list[wbuffer.index].base_Yaddr;
					wbuffer.buff_Uaddr = wdma_data->frame_list.wbuf_list[wbuffer.index].base_Uaddr;
					wbuffer.buff_Vaddr = wdma_data->frame_list.wbuf_list[wbuffer.index].base_Vaddr;
					wbuffer.frame_fmt = wdma_data->frame_list.data->frame_fmt;
					wbuffer.frame_x =  wdma_data->frame_list.data->frame_x;
					wbuffer.frame_y =  wdma_data->frame_list.data->frame_y;

					 printk("\n[WDMA Driver] Y : 0x%08x U : 0x%08x V : 0x%08x fmt : %d X : %d Y : %d \n",
					 		wbuffer.buff_Yaddr , wbuffer.buff_Uaddr , wbuffer.buff_Vaddr , wbuffer.frame_fmt , wbuffer.frame_x , wbuffer.frame_y);

					wdma_set_index_of_status(&wdma_data->frame_list, wbuffer.index, UPLOAD_S);
				}
				
				if(before_id >=0)
					wdma_set_index_of_status(&wdma_data->frame_list, before_id, PREPARED_S);	
				
				spin_unlock_irq(&(wdma_data->cmd_lock));

				mutex_unlock(&wdma_data->io_mutex);
				
				printk("~~~~~~~~~~~~~~ index : %d  before_id : %d \n",wbuffer.index,  before_id);
				return 0;
			}
		case 0x102:
			{
				unsigned int before_id, cur_id;
				before_id = wdma_find_index_of_status(&wdma_data->frame_list, UPLOAD_S);
				cur_id = wdma_find_index_of_status(&wdma_data->frame_list, WRITED_S);
				printk("~~~~~~~~~~~~~~ index : %d  before_id : %d \n",cur_id,  before_id);
				return 0;
			}

		case 0x103:
			mutex_lock(&wdma_data->io_mutex);
			ret = wdma_queue_list_exit(&wdma_data->frame_list);

			wdma_data->wdma_contiuous = 0;
			VIOC_WDMA_SetImageDisable(wdma_data->wdma.reg);
			
			mutex_unlock(&wdma_data->io_mutex);
			   wdma_data->dev_opened--;

			return ret;




							
		case TCC_WDMA_IOCTRL:
				mutex_lock(&wdma_data->io_mutex);

	            if(wdma_data->block_operating) {
				wdma_data->block_waiting = 1;
				ret = wait_event_interruptible_timeout(wdma_data->cmd_wq, wdma_data->block_operating == 0, msecs_to_jiffies(200));
				if(ret <= 0) {
					wdma_data->block_operating = 0;
					printk("ret: %d : wdma 0 timed_out block_operation:%d!! cmd_count:%d \n", ret, wdma_data->block_waiting, wdma_data->cmd_count);
				}
				ret = 0;
			}
			
			if(ret >= 0) {
				if(wdma_data->block_operating >= 1) {
					printk("wdma driver :: block_operating(%d) - block_waiting(%d) - cmd_count(%d) - poll_count(%d)!!!\n", 	\
								wdma_data->block_operating, wdma_data->block_waiting, wdma_data->cmd_count, wdma_data->poll_count);
				}

				wdma_data->block_waiting = 0;
				wdma_data->block_operating = 1;

				ret = tccxxx_wdma_ctrl(arg, wdma_data);// function call

				if(ret >= 0) 
		                    wdma_data->block_operating = 0;
			}
			mutex_unlock(&wdma_data->io_mutex);
			return 0;


		case TC_WDRV_COUNT_START:
			{
				struct vioc_wdma_frame_info cmd_data;

				mutex_lock(&wdma_data->io_mutex);

	            		if(wdma_data->block_operating) {
					wdma_data->block_waiting = 1;
					ret = wait_event_interruptible_timeout(wdma_data->cmd_wq, wdma_data->block_operating == 0, msecs_to_jiffies(200));
					if(ret <= 0) {
						wdma_data->block_operating = 0;
						printk("[%d]: wdma 0 timed_out block_operation:%d!! cmd_count:%d \n", ret, wdma_data->block_waiting, wdma_data->cmd_count);
					}
					wdma_data->block_waiting = 0;					
					ret = 0;
				}

				if(copy_from_user(&cmd_data, (const void __user *)arg, sizeof(struct vioc_wdma_frame_info))){
					mutex_unlock(&wdma_data->io_mutex);
					return -EFAULT;
				}

				if(!cmd_data.frame_x || !cmd_data.frame_y)
				{
					printk("TC_WDRV_COUNT_START size error : size : %d x %d \n", cmd_data.frame_x , cmd_data.frame_y);
					mutex_unlock(&wdma_data->io_mutex);
					return -EFAULT;
				}
				cmd_data.buffer_num = wdma_queue_list_init(&wdma_data->frame_list, &cmd_data);
				wdma_data->wdma_contiuous = 1;

				if(wdma_data->sc.reg)
					tc_wdrv_scaler_set(wdma_data, cmd_data.frame_x, cmd_data.frame_y);
				
				wdma_data->vioc_intr->bits = (1 << VIOC_WDMA_INTR_UPD);
				vioc_intr_clear(wdma_data->vioc_intr->id, wdma_data->vioc_intr->bits);
				vioc_intr_enable(wdma_data->irq, wdma_data->vioc_intr->id, wdma_data->vioc_intr->bits);
				tc_wdrv_wdma_path_set(wdma_data, &cmd_data, (struct wdma_buffer_list *)&wdma_data->frame_list.wbuf_list[0]);
				if(copy_to_user((struct vioc_wdma_frame_info *)arg, &cmd_data, sizeof(struct vioc_wdma_frame_info))){
					mutex_unlock(&wdma_data->io_mutex);
					return -EFAULT;
				}
				
				mutex_unlock(&wdma_data->io_mutex);
				return 0;
			}



		case TC_WDRV_COUNT_GET_DATA:
			{
				struct vioc_wdma_get_buffer wbuffer;
				unsigned int before_id;
				memset(&wbuffer, 0, sizeof(wbuffer));
				
				mutex_lock(&wdma_data->io_mutex);
				spin_lock_irq(&(wdma_data->cmd_lock));
				before_id = wdma_find_index_of_status(&wdma_data->frame_list, UPLOAD_S);
				wbuffer.index = wdma_find_index_of_status(&wdma_data->frame_list, WRITED_S);
				
				if(wbuffer.index >= 0)
				{
					wbuffer.buff_Yaddr = (unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Yaddr;
					wbuffer.buff_Uaddr = (unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Uaddr;
					wbuffer.buff_Vaddr = (unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Vaddr;
					wbuffer.frame_fmt = wdma_data->frame_list.data->frame_fmt;
					wbuffer.frame_x =  wdma_data->frame_list.data->frame_x;
					wbuffer.frame_y =  wdma_data->frame_list.data->frame_y;


					#ifdef WDMA_IMAGE_DEBUG
					{
						unsigned int checking_loop;
						
						for(checking_loop = 0; checking_loop < CHECKING_AREA(wdma_data->frame_list.data->frame_x, wdma_data->frame_list.data->frame_y); checking_loop++)
						{
							if(wdma_data->frame_list.wbuf_list[wbuffer.index].vbase_Yaddr[CHECKING_START_POS(wdma_data->frame_list.data->frame_x, wdma_data->frame_list.data->frame_y) + checking_loop] != CHECKING_NUM)
								break;
						}

						if(checking_loop >= CHECKING_AREA(wdma_data->frame_list.data->frame_x, wdma_data->frame_list.data->frame_y))
							pr_err("get buffer image is same with CHECKING_NUM  checking_area : %d  \n", 
							CHECKING_AREA(wdma_data->frame_list.data->frame_x, wdma_data->frame_list.data->frame_y));
					}
					#endif//

					//printk("\n[WDMA Driver] Y : 0x%08x U : 0x%08x V : 0x%08x fmt : %d X : %d Y : %d \n",
					//		wbuffer.buff_Yaddr , wbuffer.buff_Uaddr , wbuffer.buff_Vaddr , wbuffer.frame_fmt , wbuffer.frame_x , wbuffer.frame_y);
					wdma_set_index_of_status(&wdma_data->frame_list, wbuffer.index, UPLOAD_S);
				}
				
				if(before_id >=0)
					wdma_set_index_of_status(&wdma_data->frame_list, before_id, PREPARED_S);	
				
				spin_unlock_irq(&(wdma_data->cmd_lock));

				if(copy_to_user( (struct vioc_wdma_get_buffer *)arg, (struct vioc_wdma_get_buffer *)&wbuffer, sizeof(struct vioc_wdma_get_buffer))){
					mutex_unlock(&wdma_data->io_mutex);
					return -EFAULT;
				}
				mutex_unlock(&wdma_data->io_mutex);
			}
			return ret;

		case TC_WDRV_GET_CUR_DATA:
			{
				struct vioc_wdma_get_buffer wbuffer;

				memset(&wbuffer, 0, sizeof(wbuffer));
			
				mutex_lock(&wdma_data->io_mutex);

				wdma_data->block_operating = 1;
				wdma_data->block_waiting = 1;

				ret = wait_event_interruptible_timeout(wdma_data->cmd_wq, wdma_data->block_operating == 0, msecs_to_jiffies(50));

				wbuffer.index = wdma_find_index_of_status(&wdma_data->frame_list, WRITED_S);

				if(ret <= 0) {
					wdma_data->block_operating = 0;
					printk("ret: %d : wdma 0 timed_out block_operation:%d!! cmd_count:%d \n", ret, wdma_data->block_waiting, wdma_data->cmd_count);
				}
				wdma_data->block_waiting = 0;

				if(wbuffer.index >= 0)
				{
					wbuffer.buff_Yaddr = (unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Yaddr;
					wbuffer.buff_Uaddr = (unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Uaddr;
					wbuffer.buff_Vaddr = (unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Vaddr;
					wbuffer.frame_fmt = wdma_data->frame_list.data->frame_fmt;
					wbuffer.frame_x =  wdma_data->frame_list.data->frame_x;
					wbuffer.frame_y =  wdma_data->frame_list.data->frame_y;

					dprintk("[WDMA Driver] index:%d Y : 0x%08x U : 0x%08x V : 0x%08x fmt : %d X : %d Y : %d \n",
						wbuffer.index, wbuffer.buff_Yaddr , wbuffer.buff_Uaddr , wbuffer.buff_Vaddr,
						wbuffer.frame_fmt , wbuffer.frame_x , wbuffer.frame_y);
				}
				
				
				if(copy_to_user( (struct vioc_wdma_get_buffer *)arg, (struct vioc_wdma_get_buffer *)&wbuffer, sizeof(struct vioc_wdma_get_buffer))){
					mutex_unlock(&wdma_data->io_mutex);
					return -EFAULT;
				}
				mutex_unlock(&wdma_data->io_mutex);
			}
			return 0;

		case TC_WDRV_COUNT_END:
			mutex_lock(&wdma_data->io_mutex);

			wdma_data->wdma_contiuous = 0;
			VIOC_WDMA_SetImageDisable(wdma_data->wdma.reg);

			wdma_data->block_operating = 1;
			wdma_data->block_waiting = 1;
			
			ret = wait_event_interruptible_timeout(wdma_data->cmd_wq, wdma_data->block_operating == 0, msecs_to_jiffies(30));

			if(ret <= 0)	{
				printk("wdma [%d]  block_operation:%d!! cmd_count:%d \n", ret, wdma_data->block_waiting, wdma_data->cmd_count);
			}
			wdma_data->block_waiting = 0;
			wdma_data->block_operating = 0;

			vioc_intr_clear(wdma_data->vioc_intr->id, wdma_data->vioc_intr->bits);
			vioc_intr_disable(wdma_data->irq, wdma_data->vioc_intr->id, wdma_data->vioc_intr->bits);

			ret = wdma_queue_list_exit(&wdma_data->frame_list);
			mutex_unlock(&wdma_data->io_mutex);
			return ret;

			
		default:
			printk(KERN_ALERT "not supported WMIXER IOCTL(0x%x). \n", cmd);
			break;			
	}

	return 0;
}
EXPORT_SYMBOL(tccxxx_wdma_ioctl);

int tccxxx_wdma_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct tcc_wdma_dev *pwdma_data = dev_get_drvdata(misc->parent);
	
	dprintk("wdma_release IN:  %d'th, block(%d/%d), cmd(%d), irq(%d). \n", pwdma_data->dev_opened, 			\
			pwdma_data->block_operating, pwdma_data->block_waiting, pwdma_data->cmd_count, pwdma_data->irq_reged);

	if(pwdma_data->dev_opened > 0) 	
        pwdma_data->dev_opened--;

	if(pwdma_data->dev_opened == 0) {
		if(pwdma_data->block_operating) {
			ret = wait_event_interruptible_timeout(pwdma_data->cmd_wq, pwdma_data->block_operating == 0, msecs_to_jiffies(200));
		}

		if(ret <= 0) {
 			printk("[%d]: wdma timed_out block_operation: %d, block_waiting:%d  cmd_count: %d. \n", 
				ret, (pwdma_data->block_operating), pwdma_data->block_waiting, pwdma_data->cmd_count);
		}

		if(pwdma_data->irq_reged) {
			free_irq(pwdma_data->irq, pwdma_data);
			pwdma_data->irq_reged = 0;
		}

		pwdma_data->block_operating = pwdma_data->block_waiting = 0;
		pwdma_data->poll_count = pwdma_data->cmd_count = 0;
	}

	clk_disable_unprepare(pwdma_data->wdma_clk);

	dprintk("wdma_release OUT:  %d'th. \n", pwdma_data->dev_opened);
	return 0;
}
EXPORT_SYMBOL(tccxxx_wdma_release);



int tccxxx_wdma_open(struct inode *inode, struct file *filp)
{	
	int ret = 0;
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct tcc_wdma_dev *pwdma_data = dev_get_drvdata(misc->parent);

	dprintk("wdma_open IN:  %d'th, block(%d/%d), cmd(%d), irq(%d). \n", pwdma_data->dev_opened, 				\
			pwdma_data->block_operating, pwdma_data->block_waiting, pwdma_data->cmd_count, pwdma_data->irq_reged);

	clk_prepare_enable(pwdma_data->wdma_clk);

	if(!pwdma_data->irq_reged) {
		dprintk("wdma irq id:0x%x bits :0x%x  \n", pwdma_data->vioc_intr->id, pwdma_data->vioc_intr->bits);
		vioc_intr_clear(pwdma_data->vioc_intr->id, pwdma_data->vioc_intr->bits);
		vioc_intr_enable(pwdma_data->irq, pwdma_data->vioc_intr->id, pwdma_data->vioc_intr->bits);

		if( request_irq(pwdma_data->irq, tccxxx_wdma_handler, IRQF_SHARED, "wdma", pwdma_data) < 0){
			pr_err("failed to aquire irq\n");
			return -EFAULT;
		}

		if(ret) {
			clk_disable_unprepare(pwdma_data->wdma_clk);
			pr_err("failed to aquire wdma request_irq. \n");
			return -EFAULT;
		}

		pwdma_data->irq_reged = 1;
	}

	pwdma_data->dev_opened++;
	
	dprintk("wdma_open OUT:  %d'th. \n", pwdma_data->dev_opened);
	return ret;	
}
EXPORT_SYMBOL(tccxxx_wdma_open);

static int tcc_wdma_parse_dt(struct platform_device *pdev, struct tcc_wdma_dev *pwdma_data)
{
	int ret;
	unsigned int index;
	struct device_node *vioc_node, *disp_node;
	struct device_node *np;

	vioc_node = of_parse_phandle(pdev->dev.of_node, "wdma-fbdisplay", 0);

	if(vioc_node)
	{
		if(of_property_read_u32(vioc_node, "telechips,fbdisplay_num",&pwdma_data->fb_dd_num)){
			pr_err( "could not find  telechips,fbdisplay_nubmer\n");
			ret = -ENODEV;
		}

		if(pwdma_data->fb_dd_num)
			np = of_find_node_by_name(vioc_node, "fbdisplay1");
		else
			np = of_find_node_by_name(vioc_node, "fbdisplay0");

		if(!np){
			pr_err(" %s could not fine fb node \n",__func__);
			return -ENODEV;
		}

		disp_node = of_parse_phandle(np, "telechips,disp", 0);
		of_property_read_u32_index(np, "telechips,disp", 1, &index);

		if (!disp_node) {
			pr_err( "could not find disp node\n");
			ret = -ENODEV;
		}else {
			pwdma_data->disp_info.virt_addr= VIOC_DISP_GetAddress(index);
		}
	}
	return ret;

}


static struct file_operations tcc_wdma_fops = {
	.owner 				= THIS_MODULE,
	.unlocked_ioctl 	= tccxxx_wdma_ioctl,
	.mmap 			= tccxxx_wdma_mmap,
	.open 			= tccxxx_wdma_open,
	.release 			= tccxxx_wdma_release,
	.poll 			= tccxxx_wdma_poll,
};

static char banner[] = KERN_INFO "TCC WDMA Driver Initializing. \n";


static int  tcc_wdma_probe(struct platform_device *pdev)
{
	struct tcc_wdma_dev *wdma_data;
	struct device_node *dev_np;
	int ret = -ENOMEM;

	printk(banner);

	wdma_data = kzalloc(sizeof(struct tcc_wdma_dev ), GFP_KERNEL);
	if(!wdma_data)
		return -ENOMEM;

	wdma_data->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (wdma_data->misc == 0)
		goto err_misc_alloc;

	wdma_data->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (wdma_data->vioc_intr == 0)
		goto err_misc_regist;
	
	tcc_wdma_parse_dt(pdev, wdma_data);
	
	wdma_data->misc->minor = MISC_DYNAMIC_MINOR;
	wdma_data->misc->fops = &tcc_wdma_fops;
	wdma_data->misc->name = pdev->name;
	wdma_data->misc->parent = &pdev->dev;
	ret = misc_register(wdma_data->misc);
	if (ret)
		goto err_misc_regist;	

	dev_np = of_parse_phandle(pdev->dev.of_node, "scalers", 0);
	if(dev_np) {
		of_property_read_u32_index(pdev->dev.of_node, "scalers", 1, &wdma_data->sc.id);
		wdma_data->sc.reg = VIOC_SC_GetAddress(wdma_data->sc.id);
	} else {
		printk("could not find wdma_data node of %s driver. \n", wdma_data->misc->name);
		wdma_data->sc.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wmixs", 0);
	if(dev_np) {
		of_property_read_u32_index(pdev->dev.of_node, "wmixs", 1, &wdma_data->wmix.id);
		wdma_data->wmix.reg = VIOC_WMIX_GetAddress(wdma_data->wmix.id);
	} else {
		printk("could not find wmix node of %s driver. \n", wdma_data->misc->name);
		wdma_data->wmix.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wdmas", 0);
	if(dev_np) {
		of_property_read_u32_index(pdev->dev.of_node, "wdmas", 1, &wdma_data->wdma.id);
		wdma_data->wdma.reg = VIOC_WDMA_GetAddress(wdma_data->wdma.id);
		wdma_data->irq = irq_of_parse_and_map(dev_np, get_vioc_index(wdma_data->wdma.id));
		wdma_data->vioc_intr->id	= VIOC_INTR_WD0 + get_vioc_index(wdma_data->wdma.id);
		wdma_data->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
	} else {
		printk("could not find wdma node of %s driver. \n", wdma_data->misc->name);
		wdma_data->wdma.reg = NULL;
	}
	spin_lock_init(&(wdma_data->poll_lock));
	spin_lock_init(&(wdma_data->cmd_lock));

	mutex_init(&(wdma_data->io_mutex));
	
	init_waitqueue_head(&(wdma_data->poll_wq));
	init_waitqueue_head(&(wdma_data->cmd_wq));

	platform_set_drvdata(pdev, (void *)wdma_data);

	return 0;

err_misc_regist:
	kfree(wdma_data->misc);
err_misc_alloc:
	kfree(wdma_data);

	return ret;
}

static int tcc_wdma_remove(struct platform_device *pdev)
{
	struct tcc_wdma_dev *pwdma_data = (struct tcc_wdma_dev *)platform_get_drvdata(pdev);
	misc_deregister(pwdma_data->misc);
	kfree(pwdma_data->misc);
	kfree(pwdma_data);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id tcc_wdma_of_match[] = {
	{.compatible = "telechips,tcc_wdma"},
	{}
};
MODULE_DEVICE_TABLE(of, tcc_wdma_of_match);
#endif

static struct platform_driver tcc_wdma_driver = {
	.probe 		= tcc_wdma_probe,
	.remove		= tcc_wdma_remove,
	.driver		= {
		.name	= "wdma",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_wdma_of_match),
#endif
	},
};



static int __init tcc_wdma_init(void)
{
	printk(KERN_INFO " %s\n", __func__);
	return platform_driver_register(&tcc_wdma_driver);
}

static void __exit tcc_wdma_exit(void)
{
	platform_driver_unregister(&tcc_wdma_driver);
}




MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC WDMA Driver");
MODULE_LICENSE("GPL");


module_init(tcc_wdma_init);
module_exit(tcc_wdma_exit);


