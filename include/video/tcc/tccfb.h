/*
 * linux/drivers/video/tcc/tccfb.h
 *
 * Author: <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC LCD Controller Frame Buffer Driver
 *
 * Copyright (C) 2008-2009 Telechips 
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

#ifndef __TCCFB_H
#define __TCCFB_H
#include <linux/fb.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include "tcc_fb.h"
#include "vioc_intr.h"

/*****************************************************************************
*
* Enum
*
******************************************************************************/
// Display order
enum {
	DD_MAIN = 0,
	DD_SUB = 1
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)	
struct sw_sync_timeline {
	struct kref		kref;
	char			name[32];

	/* protected by child_list_lock */
	u64			context;
	int			value;

	struct list_head	child_list_head;
	spinlock_t		child_list_lock;

	struct list_head	active_list_head;

	struct list_head	sync_timeline_list;
};
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
enum {
	RDMA_FB1		= 1, //framebuffer 1 driver
	RDMA_3D			= 1, //Layer0
	RDMA_LASTFRM	= 1, //Layer0
	RDMA_FB			= 0, //Layer1	
	RDMA_VIDEO		= 2, //Layer2
	RDMA_MOUSE		= 3, //Layer3
	RDMA_VIDEO_3D	= 3, //Layer3
	RDMA_VIDEO_SUB	= 3, //Layer3
	RDMA_MAX_NUM
};
#else
enum {
	RDMA_FB			= 0, //Layer0
	RDMA_FB1		= 1, //framebuffer 1 driver
	RDMA_3D			= 1, //Layer1
	RDMA_LASTFRM	= 1, //Layer1
	RDMA_MOUSE		= 2, //Layer2
	RDMA_VIDEO_3D	= 2, //Layer2
	RDMA_VIDEO_SUB	= 2, //Layer2
	RDMA_VIDEO		= 3, //Layer3
	RDMA_MAX_NUM
};
#endif

enum {
	FB_RDMA_UPDATE,
	FB_SC_RDMA_UPDATE,
	FB_SC_M2M_RDMA_UPDATE,
	FB_SC_G2D_RDMA_UPDATE,
	FB_ATTACH_UPDATE,
	FB_DIVIDE_UPDATE,
	FB_MVC_UPDATE,
	FB_UPDATE_MAX
};

/*****************************************************************************
*
* Structures
*
******************************************************************************/
struct tcc_vioc_block{
	volatile void __iomem		*virt_addr;		// virtual address
	unsigned int 		irq_num;
	unsigned int 		blk_num;		//block number like dma number or mixer number
};

struct display_WaitQ_struct {
	wait_queue_head_t 	waitq;
	unsigned int 			state;
};

struct tcc_dp_device {
	TCC_OUTPUT_TYPE DispDeviceType;
	unsigned int DispOrder;								//DD_MAIN , DD_SUB
	unsigned int DispNum;								//0 or 1
	unsigned int FbPowerState;							//true or false
	unsigned int FbUpdateType;							//like FB_RDMA_UDPATE or FB_SC_RDMA_UPDATE
	/*  if FB_SC_RDMA_UPDATE type plug in scaler number
	sc_num0 : scaler number for normal UI,
	sc_num1 : scaler number for 3D UI
	*/
	unsigned int sc_num0, sc_num1;
	unsigned int FbBaseAddr;							//base address of frame buffer

	struct clk				*vioc_clock;				//vioc blcok clock
	struct clk				*ddc_clock;					//display blcok clock
	struct tcc_vioc_block	ddc_info;					// display controller address
	struct tcc_vioc_block	wmixer_info;				// wmixer address
	struct tcc_vioc_block	wdma_info;					// wdma address
	struct tcc_vioc_block	rdma_info[RDMA_MAX_NUM];	// rdma address

	struct display_WaitQ_struct pandisp_sync; //update interrupt sync
	struct display_WaitQ_struct disp_dd_sync; //display device disable done

};

struct tccfb_platform_data {
// main display device infomation
	unsigned int lcdc_number;
	unsigned int FbPowerState;

// main display device infomation
	struct tcc_dp_device Mdp_data;

// sub display device infomation
	struct tcc_dp_device Sdp_data;
};

struct tccfb_info {
	struct vioc_intr_type	*vioc_intr;

	struct fb_info		*fb;
	struct device		*dev;

	//struct tccfb_mach_info *mach_info;

	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	u_int			map_size;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of buffer */
	dma_addr_t		screen_dma;	/* physical address of buffer */

	u_int			imgch;

	struct tccfb_platform_data 	pdata;
	u_char *			backup_map_cpu[6];	/* quick boot back up */
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
    struct early_suspend earlier_suspend;
#endif
	u_char  active_vsync;
	ktime_t vsync_timestamp;
	struct work_struct vsync_work;

	bool				output_on;
	struct mutex		output_lock;
	spinlock_t 			spin_lockDisp;
	
#ifdef CONFIG_SYNC_FB
	struct list_head	fb_update_regs_list;
	struct mutex		fb_timeline_lock;	

	struct kthread_worker	fb_update_regs_worker;
	struct task_struct	*fb_update_regs_thread;
	struct kthread_work	fb_update_regs_work;
	
	struct sw_sync_timeline *fb_timeline;
	int fb_timeline_max;

	struct list_head	ext_update_regs_list;
	struct mutex		ext_timeline_lock;	

	struct kthread_worker	ext_update_regs_worker;
	struct task_struct	*ext_update_regs_thread;
	struct kthread_work	ext_update_regs_work;
	
	struct sw_sync_timeline *ext_timeline;
	int ext_timeline_max;
#endif

	/* SWAP_BUFFER (for video seek) */
	int image_enable;
	unsigned int swap_buf_id;
	int swap_buf_status;
#define SWAP_BUF_START	(0xA0000001)
#define SWAP_BUF_END	(0xB0000002)
};

struct tcc_fenc_reg_data {
	struct list_head	list;
	struct fb_var_screeninfo 	var;	/* Current var */
	u_char *					screen_cpu;	/* virtual address of buffer */
	dma_addr_t				screen_dma;	/* physical address of buffer */
	int						fence_fd;	/* physical address of buffer */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)			
	struct sync_fence *fence;
#else
	struct fence *fence;
#endif
};

/*****************************************************************************
*
* Defines
*
******************************************************************************/
#define PALETTE_BUFF_CLEAR (0x80000000)	/* entry is clear/invalid */

#if defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
#define TCC_VIDEO_DISPLAY_BY_VSYNC_INT
#endif

#if defined(CONFIG_TCC_VIDEO_DISPLAY_DEINTERLACE_MODE)
#define TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
#endif

#if defined(CONFIG_TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT)
#define TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
#endif

#if defined(CONFIG_TCC_OUTPUT_STARTER)
	#if defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT)
		#define TCC_OUTPUT_STARTER_AUTO_HDMI_CVBS
	#elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_HDMI_CVBS)
		#define TCC_OUTPUT_STARTER_ATTACH_HDMI_CVBS
	#elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_AUTO)
		#define TCC_OUTPUT_STARTER_ATTACH_DUAL_AUTO
	#else
		#if defined(CONFIG_STB_BOARD_STB1)
			#define TCC_OUTPUT_STARTER_ATTACH_DUAL_AUTO
		#else
			#define TCC_OUTPUT_STARTER_NORMAL
		#endif
	#endif
#endif

/*****************************************************************************
*
* External Functions
*
******************************************************************************/
unsigned int tcc_vioc_display_dt_parse(struct device_node *np, 	struct tcc_dp_device *dp_data);
#endif
