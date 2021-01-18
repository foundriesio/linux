/*
 * Copyright (C) 2008-2019 Telechips Inc.
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
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of_graph.h>
#include <linux/console.h>
#include <video/videomode.h>

#include "panel/panel_helper.h"

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <soc/tcc/pmap.h>

#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_scaler_ioctrl.h>
#include <video/tcc/tcc_attach_ioctrl.h>
#if defined(CONFIG_VIOC_PVRIC_FBDC)
#include <video/tcc/vioc_pvric_fbdc.h>
#define FBDC_ALIGNED(w, mul) (((unsigned int)w + (mul - 1)) & ~(mul - 1))
#endif

#define FB_BUF_MAX_NUM		(3)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
static unsigned int fbdc_dec_1st_cfg = 0;
static unsigned int fbdc_wdma_need_on = 0; /* 0: no, 1: need */
#endif

struct fb_vioc_waitq {
	wait_queue_head_t waitq;
	atomic_t state;
};

struct fb_vioc_block{
	volatile void __iomem		*virt_addr;		// virtual address
	unsigned int 		irq_num;
	unsigned int 		blk_num;		//block number like dma number or mixer number
};

struct fb_region {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};

struct fb_dp_device {
	unsigned int FbPowerState;		//true or false
	unsigned int FbDeviceType;
	unsigned int FbUpdateType;
	unsigned int FbLayerOrder;
	unsigned int FbWdmaPath;		/* 0: Display device path, 1: Wdma path */
	struct clk *vioc_clock;		//vioc blcok clock
	struct clk *ddc_clock;		//display blcok clock
	struct fb_vioc_block ddc_info;		// display controller address
	struct fb_vioc_block wdma_info;		// display controller address
	struct fb_vioc_block wmixer_info;		// wmixer address
	struct fb_vioc_block scaler_info;		// scaler address
	struct fb_vioc_block rdma_info;		// rdma address
	struct fb_vioc_block fbdc_info;		// rdma address
	struct fb_region region;
	struct file *filp;
	unsigned int dst_addr[FB_BUF_MAX_NUM];
	unsigned int buf_idx;
	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	u32 lcdc_mux;
	struct videomode vm;
	#endif
};

struct fbX_par {
	dma_addr_t map_dma;
	dma_addr_t screen_dma;
	unsigned int map_size;
	unsigned char *map_cpu;
	unsigned char *screen_cpu;
	struct fb_dp_device pdata;

	int pm_state;
	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	struct fb_panel *panel;
	#endif
};

/*
 * Here we define the default structs fb_fix_screeninfo and fb_var_screeninfo
 * if we don't use modedb. If we do use modedb see fb_init how to use it
 * to get a fb_var_screeninfo. Otherwise define a default var as well.
 */
static struct fb_fix_screeninfo fbX_fix = {
	.id =		"telechips,fb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.type_aux = 0,
	.xpanstep =	0,
	.ypanstep =	1,
	.ywrapstep =	0,
	.accel =	FB_ACCEL_NONE,
};

enum {
	FB_DEV_0 = 0,
	FB_DEV_1,
	FB_DEV_2,
	FB_DEV_3,
	FB_DEV_4,
	FB_DEV_MAX,
};

static struct fb_vioc_waitq fb_waitq[FB_DEV_MAX];

irqreturn_t fbX_display_handler(int irq, void *dev_id)
{
	struct fb_info *info;
	struct fbX_par *par;
	unsigned int block_status;

	if (dev_id == NULL) {
		pr_err("[ERR][FBX] %s irq: %d dev_id:%p \n",__func__, irq, dev_id);
		return IRQ_NONE;
	}

	info = dev_id;
	par = info->par;
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	unsigned int fbdc_status;
	fbdc_status = VIOC_PVRIC_FBDC_GetStatus(par->pdata.fbdc_info.virt_addr);
	if( fbdc_status & PVRICSYS_IRQ_ALL) {
		//pr_info("[INF][VIOC_I] FBDC(%d) INT(0x%x) ------ \n", fbdc_dec_1st_cfg, fbdc_status);
		if( fbdc_status & PVRICSTS_IDLE_MASK) {
			VIOC_PVRIC_FBDC_ClearIrq(par->pdata.fbdc_info.virt_addr, PVRICSTS_IDLE_MASK);
		}
		if( fbdc_status & PVRICSTS_UPD_MASK) {
			VIOC_PVRIC_FBDC_ClearIrq(par->pdata.fbdc_info.virt_addr, PVRICSTS_UPD_MASK);
		}
		if( fbdc_status & PVRICSTS_TILE_ERR_MASK) {
			unsigned int rdma_enable;
			VIOC_PVRIC_FBDC_ClearIrq(par->pdata.fbdc_info.virt_addr, PVRICSTS_TILE_ERR_MASK);
			pr_info("[INF][VIOC_I] FBDC(%d) INT(0x%x) ------ \n", fbdc_dec_1st_cfg, PVRICSTS_TILE_ERR_MASK);
			VIOC_RDMA_GetImageEnable(par->pdata.rdma_info.virt_addr, &rdma_enable);
			if(rdma_enable)
				VIOC_RDMA_SetImageDisable(par->pdata.rdma_info.virt_addr);
			VIOC_CONFIG_SWReset(par->pdata.fbdc_info.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(par->pdata.fbdc_info.blk_num, VIOC_CONFIG_CLEAR);
			fbdc_dec_1st_cfg = 0;
		}
		if( fbdc_status & PVRICSTS_ADDR_ERR_MASK) {
			unsigned int rdma_enable;
			VIOC_PVRIC_FBDC_ClearIrq(par->pdata.fbdc_info.virt_addr, PVRICSTS_ADDR_ERR_MASK);
			pr_info("[INF][VIOC_I] FBDC(%d) INT(0x%x) ------ \n", fbdc_dec_1st_cfg, PVRICSTS_ADDR_ERR_MASK);
			VIOC_RDMA_GetImageEnable(par->pdata.rdma_info.virt_addr, &rdma_enable);
			if(rdma_enable)
				VIOC_RDMA_SetImageDisable(par->pdata.rdma_info.virt_addr);
			VIOC_CONFIG_SWReset(par->pdata.fbdc_info.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(par->pdata.fbdc_info.blk_num, VIOC_CONFIG_CLEAR);
			fbdc_dec_1st_cfg = 0;
		}

		if( fbdc_status & PVRICSTS_EOF_ERR_MASK) {
			unsigned int rdma_enable;
			VIOC_PVRIC_FBDC_ClearIrq(par->pdata.fbdc_info.virt_addr, PVRICSTS_EOF_ERR_MASK);
			pr_info("[INF][VIOC_I] FBDC(%d) INT(0x%x) ------ \n", fbdc_dec_1st_cfg, PVRICSTS_EOF_ERR_MASK);
			VIOC_RDMA_GetImageEnable(par->pdata.rdma_info.virt_addr, &rdma_enable);
			if(rdma_enable)
				VIOC_RDMA_SetImageDisable(par->pdata.rdma_info.virt_addr);
			VIOC_CONFIG_SWReset(par->pdata.fbdc_info.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(par->pdata.fbdc_info.blk_num, VIOC_CONFIG_CLEAR);
			fbdc_dec_1st_cfg = 0;
		}
	}

#endif
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
	if(par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
		block_status = vioc_intr_get_status(VIOC_INTR_RD0 + get_vioc_index(par->pdata.rdma_info.blk_num));
		if(block_status & (0x1 << VIOC_RDMA_INTR_EOFR)) {
			vioc_intr_clear(VIOC_INTR_RD0 + get_vioc_index(par->pdata.rdma_info.blk_num), (0x1 << VIOC_RDMA_INTR_EOFR));
			if (atomic_read(&fb_waitq[info->node].state) == 0) {
				atomic_set(&fb_waitq[info->node].state, 1);
				wake_up_interruptible(&fb_waitq[info->node].waitq);
			}
		}
	} else if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#else
	 if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#endif
	{
		block_status = vioc_intr_get_status(get_vioc_index(par->pdata.ddc_info.blk_num));
		if (block_status & (0x1<<VIOC_DISP_INTR_RU)) {
			vioc_intr_clear(get_vioc_index(par->pdata.ddc_info.blk_num), (0x1<<VIOC_DISP_INTR_RU));

			if (atomic_read(&fb_waitq[info->node].state) == 0) {
				atomic_set(&fb_waitq[info->node].state, 1);
				wake_up_interruptible(&fb_waitq[info->node].waitq);
			}
		}

		if(block_status & (1<<VIOC_DISP_INTR_FU)) {
			vioc_intr_disable(par->pdata.ddc_info.irq_num, get_vioc_index(par->pdata.ddc_info.blk_num), (0x1<<VIOC_DISP_INTR_FU));
			vioc_intr_clear(get_vioc_index(par->pdata.ddc_info.blk_num), (0x1<<VIOC_DISP_INTR_FU));
		}

		if(block_status & (0x1<<VIOC_DISP_INTR_DD))
			vioc_intr_clear(get_vioc_index(par->pdata.ddc_info.blk_num), (0x1<<VIOC_DISP_INTR_DD));

		if(block_status & (1<<VIOC_DISP_INTR_SREQ))
			vioc_intr_clear(get_vioc_index(par->pdata.ddc_info.blk_num), (1<<VIOC_DISP_INTR_SREQ));
	}
	return IRQ_HANDLED;
}

/**
 *      fb_check_var - Optional function. Validates a var passed in.
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int fbX_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	/* validate bpp */
	if (var->bits_per_pixel > 32)
		var->bits_per_pixel = 32;
	else if (var->bits_per_pixel < 16)
		var->bits_per_pixel = 16;

	/* set r/g/b positions */
	switch(var->bits_per_pixel) {
	case 16:
		var->red.offset 	= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length 	= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		var->transp.length	= 0;
		break;
	case 32:
		var->red.offset 	= 16;
		var->green.offset	= 8;
		var->blue.offset	= 0;
		var->transp.offset	= 24;
		var->red.length 	= 8;
		var->green.length	= 8;
		var->blue.length	= 8;
		var->transp.length	= 8;
		break;
	default:
		var->red.length 	= var->bits_per_pixel;
		var->red.offset 	= 0;
		var->green.length	= var->bits_per_pixel;
		var->green.offset	= 0;
		var->blue.length	= var->bits_per_pixel;
		var->blue.offset	= 0;
		var->transp.length	= 0;
		break;
	}

	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	return 0;
}

/**
 *      fb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int fbX_set_par(struct fb_info *info)
{
	switch(info->var.bits_per_pixel) {
	case 16:
		info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
	case 32:
		info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
	default:
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
		break;
	}

	info->fix.line_length = (info->var.xres * info->var.bits_per_pixel)/8;
	if(info->var.rotate != 0) {
		pr_info("[INF][FBX] %s: do not support rotation \n", __func__);
		return -1;
	}

	return 0;
}

static void schedule_palette_update(struct fb_info *info,
				    unsigned int regno, unsigned int val)
{
	unsigned long flags;
	local_irq_save(flags);
	local_irq_restore(flags);
}

/* from pxafb.c */
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

/**
 *  	fb_setcolreg - Optional function. Sets a color register.
 *      @regno: Which register in the CLUT we are programming
 *      @red: The red value which can be up to 16 bits wide
 *	@green: The green value which can be up to 16 bits wide
 *	@blue:  The blue value which can be up to 16 bits wide.
 *	@transp: If supported, the alpha value which can be up to 16 bits wide.
 *      @info: frame buffer info structure
 *
 */
static int fbX_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	unsigned int val;

    if (regno >= 256)  /* no. of hw registers */
       return -EINVAL;

	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseuo-palette */
		if (regno < 16) {
			u32 *pal = info->pseudo_palette;

			val  = chan_to_field(red,   &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue,  &info->var.blue);

			pal[regno] = val;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		if (regno < 256) {
			/* currently assume RGB 5-6-5 mode */
			val  = ((red   >>  0) & 0xf800);
			val |= ((green >>  5) & 0x07e0);
			val |= ((blue  >> 11) & 0x001f);

			//writel(val, S3C2410_TFTPAL(regno));
			schedule_palette_update(info, regno, val);
		}
		break;

	default:
		pr_err("[ERR][FBX] error in %s: unknown type %d\n", __func__, info->fix.visual);
		return 1;   /* unknown type */
	}

	return 0;
}

static void fbX_vsync_activate(struct fb_info *info)
{
	struct fbX_par *par = info->par;
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
	if(par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE)
		vioc_intr_clear(VIOC_INTR_RD0 + get_vioc_index(par->pdata.rdma_info.blk_num), (0x1 << VIOC_RDMA_INTR_EOFR));
	else 	if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#else
	 if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#endif
		vioc_intr_clear(get_vioc_index(par->pdata.ddc_info.blk_num), VIOC_DISP_INTR_DISPLAY);
	atomic_set(&fb_waitq[info->node].state, 0);
}

static int fbX_prepare_m2m(struct fb_info *info)
{
	struct fbX_par *par;
	struct pmap pmap;
	char *name = NULL;
	int ret = -ENODEV;

	do {
		if(info == NULL) {
			pr_err("%s parameter is NULL\r\n", __func__);
			break;
		}

		par = info->par;
		if(par->pdata.filp != NULL) {
			/* Already opend */
			ret = 0;
			break;
		}

		if(par->pdata.FbUpdateType == FBX_ATTACH_UPDATE)
			name = kasprintf(GFP_KERNEL,"/dev/attach%d", get_vioc_index(par->pdata.ddc_info.blk_num));
		else
			name = kasprintf(GFP_KERNEL,"/dev/scaler%d", get_vioc_index(par->pdata.scaler_info.blk_num));
		if(name == NULL) {
			pr_err("%s out of memory at line(%d)\r\n", __func__, __LINE__);
			break;
		}
		par->pdata.filp = filp_open(name, O_RDWR, 666);
		kfree(name); name = NULL;

		if (IS_ERR(par->pdata.filp)) {
			pr_err("error in %s: can not open misc device(%s) \n", __func__, name);
			break;
		}

		if(par->pdata.FbUpdateType == FBX_ATTACH_UPDATE)
			name = kasprintf(GFP_KERNEL,"fb%d_video", info->node);
		else
			name = kasprintf(GFP_KERNEL,"fb_scaler%d", get_vioc_index(par->pdata.scaler_info.blk_num));
		if(name == NULL) {
			pr_err("%s out of memory at line(%d)\r\n", __func__, __LINE__);
			break;
		}
		pmap_get_info((const char *)name, &pmap);
		kfree(name); name = NULL;
		if(pmap.size > (info->var.xres * info->var.yres_virtual * (info->var.bits_per_pixel/8))) {
			unsigned int buf_offset = (info->var.xres * info->var.yres * (info->var.bits_per_pixel/8));
			unsigned int idx = 0;
			for(idx = 0; idx < ATTACH_BUF_NUM; idx++) {
				par->pdata.dst_addr[idx] = (pmap.base + (buf_offset * idx));
				pr_info("Buf%d: 0x%08x\n", idx, par->pdata.dst_addr[idx]);
			}
			ret = 0;
		} else {
			pr_err("error in %s: pmap size are not enough(%d)\n", __func__, pmap.size);
			ret = -ENODEV;
		}
	}while(0);

	return ret;
}

static void fbX_m2m_activate_var(unsigned int dma_addr, struct fb_var_screeninfo *var, struct fbX_par *par)
{
	unsigned int width, height, format, channel;

	VIOC_DISP_GetSize(par->pdata.ddc_info.virt_addr, &width, &height);
	if((width == 0) || (height == 0)) {
		pr_err("[ERR][FBX] error in %s: vioc invalid status (%dx%d)\n",
			__func__, width, height);
		return;
	}

	switch(var->bits_per_pixel) {
	case 16:
		format = VIOC_IMG_FMT_RGB565;
		break;
	case 32:
		format = VIOC_IMG_FMT_ARGB8888;
		break;
	default:
		pr_err("[ERR][FBX] error in %s: can not support bpp %d\n", __func__, var->bits_per_pixel);
		return;
	}

	if(par->pdata.FbUpdateType == FBX_M2M_RDMA_UPDATE) {
		struct SCALER_TYPE scaler;
		memset(&scaler, 0x00, sizeof(struct SCALER_TYPE));

		scaler.responsetype = SCALER_POLLING;

		scaler.src_Yaddr = dma_addr;
		scaler.src_fmt = format;
		scaler.src_ImgWidth = var->xres;
		scaler.src_ImgHeight = var->yres;
		scaler.src_winLeft = 0;
		scaler.src_winTop = 0;
		scaler.src_winRight = var->xres;
		scaler.src_winBottom = var->yres;

		scaler.dest_Yaddr = par->pdata.dst_addr[par->pdata.buf_idx++];
		if(par->pdata.buf_idx >= (var->yres_virtual/var->yres))
			par->pdata.buf_idx = 0;

		scaler.dest_fmt = format;
		scaler.dest_ImgWidth = par->pdata.region.width;		// destination image width
		scaler.dest_ImgHeight = par->pdata.region.height; 		// destination image height
		scaler.dest_winLeft = 0;
		scaler.dest_winTop = 0;
		scaler.dest_winRight = par->pdata.region.width;
		scaler.dest_winBottom = par->pdata.region.height;

		if(par->pdata.filp)
			par->pdata.filp->f_op->unlocked_ioctl(par->pdata.filp, TCC_SCALER_IOCTRL_KERENL, (unsigned long)&scaler);

		VIOC_RDMA_SetImageIntl(par->pdata.rdma_info.virt_addr, 0);
		VIOC_RDMA_SetImageFormat(par->pdata.rdma_info.virt_addr, format);
		if((scaler.dest_ImgWidth > width) || (scaler.dest_ImgHeight > height))
			VIOC_RDMA_SetImageSize(par->pdata.rdma_info.virt_addr, width, height);
		else
			VIOC_RDMA_SetImageSize(par->pdata.rdma_info.virt_addr, scaler.dest_ImgWidth, scaler.dest_ImgHeight);
		VIOC_RDMA_SetImageOffset(par->pdata.rdma_info.virt_addr, format, scaler.dest_ImgWidth);
		VIOC_RDMA_SetImageBase(par->pdata.rdma_info.virt_addr, scaler.dest_Yaddr, 0, 0);
		if(format == VIOC_IMG_FMT_ARGB8888) {
			VIOC_RDMA_SetImageAlphaSelect(par->pdata.rdma_info.virt_addr, 1);
			VIOC_RDMA_SetImageAlphaEnable(par->pdata.rdma_info.virt_addr, 1);
		}
		VIOC_RDMA_SetImageY2REnable(par->pdata.rdma_info.virt_addr, 0);
		VIOC_RDMA_SetImageR2YEnable(par->pdata.rdma_info.virt_addr, 0);

		channel =
			get_vioc_index(par->pdata.rdma_info.blk_num) - (4 * get_vioc_index(par->pdata.ddc_info.blk_num));
		VIOC_WMIX_SetPosition(par->pdata.wmixer_info.virt_addr, channel, par->pdata.region.x, par->pdata.region.y);
		VIOC_WMIX_SetChromaKey(par->pdata.wmixer_info.virt_addr, channel, 1/*ON*/, 0x0, 0x0, 0x0, 0xF8, 0xFC, 0xF8);
		VIOC_WMIX_SetUpdate(par->pdata.wmixer_info.virt_addr);

		VIOC_RDMA_SetImageEnable(par->pdata.rdma_info.virt_addr);
	} else {		/* FBX_ATTACH_UPDATE */
		ATTACH_INFO_TYPE attach;
		unsigned int idx = 0;

		memset(&attach, 0x0, sizeof(ATTACH_INFO_TYPE));

		attach.fmt = format;
		attach.img_width = par->pdata.region.width;
		attach.img_height = par->pdata.region.height;
		attach.offset_x = 0;
		attach.offset_y = 0;
		attach.mode = ATTACH_PREVIEW_MODE;
		for(idx = 0; idx < (var->yres_virtual/var->yres); idx++)
			attach.addr_y[idx] = par->pdata.dst_addr[idx];

		if(par->pdata.filp)
			par->pdata.filp->f_op->unlocked_ioctl(par->pdata.filp, TCC_ATTACH_IOCTRL_KERNEL, (unsigned long)&attach);
	}
}
#if defined(CONFIG_VIOC_PVRIC_FBDC)
static void
tca_vioc_configure_FBCDEC(unsigned int vioc_dec_id, unsigned int base_addr,
			   volatile void __iomem *pFBDC,
			   volatile void __iomem *pRDMA,
			   volatile void __iomem *pWDMA, unsigned int fmt,
			   unsigned int rdmaPath, unsigned char bSet_Comp,
			   unsigned int width, unsigned int height)
{

	if(bSet_Comp)
	{
		if(!fbdc_dec_1st_cfg){
			#if 0
			if (VIOC_WDMA_IsImageEnable(pWDMA) && VIOC_WDMA_IsContinuousMode(pWDMA)) {
				attach_data.flag = 0;
				afbc_wdma_need_on = 1;
				VIOC_WDMA_SetImageDisable(pWDMA);
			}
			#endif
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_CONFIG_FBCDECPath(vioc_dec_id, rdmaPath, 1);
			VIOC_PVRIC_FBDC_SetBasicConfiguration(pFBDC, base_addr, fmt, width, height, 0);
			VIOC_PVRIC_FBDC_SetIrqMask(pFBDC, 1, PVRICSYS_IRQ_ALL);
			VIOC_PVRIC_FBDC_TurnOn(pFBDC);
			VIOC_PVRIC_FBDC_SetUpdateInfo(pFBDC, 1);
			fbdc_dec_1st_cfg = 1;
		}

		else{
			VIOC_PVRIC_FBDC_SetBasicConfiguration(pFBDC, base_addr, fmt, width, height, 0);
//			VIOC_PVRIC_FBDC_TurnOn(pFBDC);
			VIOC_PVRIC_FBDC_SetUpdateInfo(pFBDC, 1);
			fbdc_dec_1st_cfg++;
		}
	}
	else {
		if(fbdc_dec_1st_cfg){
			#if 0
			if (VIOC_WDMA_IsImageEnable(pWDMA) && VIOC_WDMA_IsContinuousMode(pWDMA)) {
				attach_data.flag = 0;
				afbc_wdma_need_on = 1;
				VIOC_WDMA_SetImageDisable(pWDMA);
			}
			#endif
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_PVRIC_FBDC_TurnOFF(pFBDC);
			VIOC_CONFIG_FBCDECPath(vioc_dec_id, rdmaPath, 0);

			//VIOC_CONFIG_SWReset(vioc_dec_id, VIOC_CONFIG_RESET);
			//VIOC_CONFIG_SWReset(vioc_dec_id, VIOC_CONFIG_CLEAR);
			fbdc_dec_1st_cfg = 0;
		}
	}
}
#endif
static void fbX_activate_var(unsigned int dma_addr, struct fb_var_screeninfo *var, struct fbX_par *par)
{
	unsigned int width, height, format, channel;

	if(par->pdata.FbWdmaPath  == 0) {
		VIOC_DISP_GetSize(par->pdata.ddc_info.virt_addr, &width, &height);
	} else {
		/* WDMA Path */
		width = var->xres;
		height = var->yres;
	}

	if((width == 0) || (height == 0)) {
		pr_err("[ERR][FBX] error in %s: vioc invalid status (%dx%d)\n",
			__func__, width, height);
		return;
	}

	if(par->pdata.FbWdmaPath == 1 && par->pdata.wdma_info.virt_addr != NULL) {
		if(!VIOC_WDMA_IsImageEnable(par->pdata.wdma_info.virt_addr)) {
			/* WDMA Path is not Ready */
			return;
		}
	}

	switch(var->bits_per_pixel) {
	case 16:
		format = VIOC_IMG_FMT_RGB565;
		break;
	case 32:
		format = VIOC_IMG_FMT_ARGB8888;
		break;
	default:
		pr_err("[ERR][FBX] error in %s: can not support bpp %d\n", __func__, var->bits_per_pixel);
		return;
	}

	if(par->pdata.scaler_info.virt_addr) {
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(par->pdata.rdma_info.blk_num) < 0) {
			unsigned int enable;
			VIOC_RDMA_GetImageEnable(par->pdata.rdma_info.virt_addr, &enable);
			if(enable)
				VIOC_RDMA_SetImageDisable(par->pdata.rdma_info.virt_addr);
			VIOC_CONFIG_PlugIn(par->pdata.scaler_info.blk_num, par->pdata.rdma_info.blk_num);
		}
	}

	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	tca_vioc_configure_FBCDEC(par->pdata.fbdc_info.blk_num, dma_addr, par->pdata.fbdc_info.virt_addr, par->pdata.rdma_info.virt_addr, par->pdata.wdma_info.virt_addr, format, par->pdata.rdma_info.blk_num, var->reserved[3], width, height);
	#endif
	VIOC_RDMA_SetImageIntl(par->pdata.rdma_info.virt_addr, 0);
	VIOC_RDMA_SetImageFormat(par->pdata.rdma_info.virt_addr, format);
	if((var->xres > width) || (var->yres > height))
		VIOC_RDMA_SetImageSize(par->pdata.rdma_info.virt_addr, width, height);
	else
		VIOC_RDMA_SetImageSize(par->pdata.rdma_info.virt_addr, var->xres, var->yres);
	VIOC_RDMA_SetImageOffset(par->pdata.rdma_info.virt_addr, format, var->xres);
	VIOC_RDMA_SetImageBase(par->pdata.rdma_info.virt_addr, dma_addr, 0, 0);

	if(format == VIOC_IMG_FMT_ARGB8888) {
		VIOC_RDMA_SetImageAlphaSelect(par->pdata.rdma_info.virt_addr, 1);
		VIOC_RDMA_SetImageAlphaEnable(par->pdata.rdma_info.virt_addr, 1);
	}
	VIOC_RDMA_SetImageY2REnable(par->pdata.rdma_info.virt_addr, 0);
	VIOC_RDMA_SetImageR2YEnable(par->pdata.rdma_info.virt_addr, 0);


	/* Display Device Path */
	if(par->pdata.FbWdmaPath  == 0) {
	channel =
		get_vioc_index(par->pdata.rdma_info.blk_num) - (4 * get_vioc_index(par->pdata.ddc_info.blk_num));
	VIOC_WMIX_SetPosition(par->pdata.wmixer_info.virt_addr, channel, par->pdata.region.x, par->pdata.region.y);
	VIOC_WMIX_SetChromaKey(par->pdata.wmixer_info.virt_addr, channel, 1, 0x0, 0x0, 0x0, 0xF8, 0xFC, 0xF8);
	VIOC_WMIX_SetUpdate(par->pdata.wmixer_info.virt_addr);
	}

	if(par->pdata.scaler_info.virt_addr) {
		VIOC_SC_SetBypass(par->pdata.scaler_info.virt_addr, 0);
		VIOC_SC_SetDstSize(par->pdata.scaler_info.virt_addr, par->pdata.region.width, par->pdata.region.height);
		VIOC_SC_SetOutSize(par->pdata.scaler_info.virt_addr, par->pdata.region.width, par->pdata.region.height);
		VIOC_SC_SetOutPosition(par->pdata.scaler_info.virt_addr, 0, 0);
		VIOC_SC_SetUpdate(par->pdata.scaler_info.virt_addr);
	}

	VIOC_RDMA_SetImageEnable(par->pdata.rdma_info.virt_addr);
}

/**
 *      fb_pan_display - NOT a required function. Pans the display.
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int fbX_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct fbX_par *par = info->par;
	if(par->pdata.FbPowerState) {
		unsigned int dma_addr;
#if defined(CONFIG_VIOC_PVRIC_FBDC)
		if(var->reserved[3] != 0) {
			unsigned int bufnum=0;
			if(var->yoffset == 0)
				bufnum=1;
			else if (var->yoffset == var->yres)
				bufnum=2;
			else
				bufnum=3;
			dma_addr = par->map_dma +(var->xres * var->yoffset * (var->bits_per_pixel/8)) + (FBDC_ALIGNED((var->xres * var->yres/64), 256)*bufnum);
		}
		else
#else
		dma_addr = par->map_dma + (var->xres * var->yoffset * (var->bits_per_pixel/8));
#endif
//		pr_info("[INF][FBX] %s: fb%d addr:0x%08x - %s updateType:0x%x yoffset:%d reserved:%d\n", __func__, info->node, dma_addr,
//			var->activate == FB_ACTIVATE_VBL ? "VBL":"NOPE", par->pdata.FbUpdateType);

		switch(par->pdata.FbUpdateType) {
		case FBX_RDMA_UPDATE:
		case FBX_OVERLAY_UPDATE:
		case FBX_NOWAIT_UPDATE:
			fbX_activate_var(dma_addr, var, info->par);
			break;
		case FBX_M2M_RDMA_UPDATE:
			fbX_prepare_m2m(info);
			fbX_m2m_activate_var(dma_addr, var, info->par);
			break;
		case FBX_ATTACH_UPDATE:
			if(par->pdata.filp == NULL) {
				fbX_prepare_m2m(info);
				fbX_m2m_activate_var((par->map_dma + (info->var.xres * info->var.yoffset * info->var.bits_per_pixel/8)), &info->var, info->par);
			}
			break;
		default:
			return 0;
		}

		#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if(par->panel) {
			VIOC_DISP_TurnOn(par->pdata.ddc_info.virt_addr);
		}
		#endif

		if(var->activate & FB_ACTIVATE_VBL) {
			fbX_vsync_activate(info);
			if(atomic_read(&fb_waitq[info->node].state) == 0) {
				if(wait_event_interruptible_timeout(fb_waitq[info->node].waitq,
							atomic_read(&fb_waitq[info->node].state) == 1,
							msecs_to_jiffies(50)) == 0)
					pr_info("[INF][FBX] %s: vsync wait queue timeout \n",__func__);
			}
		}
	}
    	return 0;
}

/**
 *      fb_blank - NOT a required function. Blanks the display.
 *      @blank_mode: the blank mode we want.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int fbX_blank(int blank, struct fb_info *info)
{
	int ret = 0;
#ifdef CONFIG_PM
	pm_runtime_get_sync(info->dev);

	switch(blank)
	{
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		/* ... */
		break;
	case FB_BLANK_UNBLANK:
		/* ... */
		break;
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_VSYNC_SUSPEND:
		/* ... */
		break;
	default:
		pr_err("[ERR][FBX] error in %s: Invaild blank_mode %d\n", __func__, blank);
		ret = -EINVAL;
	}

	pm_runtime_put_sync(info->dev);
#endif

	return ret;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
#ifdef CONFIG_ANDROID
static struct sg_table *fb_ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	int err;
	struct sg_table *table;
	struct fb_info *info = (struct fb_info *)attachment->dmabuf->priv;

	if (info == NULL) {
		pr_err("[ERR][FBX] %s: info is null\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (table == NULL) {
		pr_err("[ERR][FBX] %s: kzalloc failed\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	err = sg_alloc_table(table, 1, GFP_KERNEL);
	if (err) {
		kfree(table);
		return ERR_PTR(err);
	}

	sg_set_page(table->sgl, NULL, info->fix.smem_len, 0);
	sg_dma_address(table->sgl) = info->fix.smem_start;
	debug_dma_map_sg(NULL, table->sgl, table->nents, table->nents, DMA_BIDIRECTIONAL);

	return table;
}

static void fb_ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	debug_dma_unmap_sg(NULL, table->sgl, table->nents, DMA_BIDIRECTIONAL);
	sg_free_table(table);

	kfree(table);
}

static int fb_ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	return -ENOSYS;
}

static void fb_ion_dma_buf_release(struct dma_buf *dmabuf)
{
	return;
}

static void *fb_ion_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	return 0;
}


struct dma_buf_ops new_fb_dma_buf_ops = {
	.map_dma_buf = fb_ion_map_dma_buf,
	.unmap_dma_buf = fb_ion_unmap_dma_buf,
	.mmap = fb_ion_mmap,
	.release = fb_ion_dma_buf_release,
	.map_atomic = fb_ion_dma_buf_kmap,
	.map = fb_ion_dma_buf_kmap,
};

struct dma_buf* new_tccfb_dmabuf_export(struct fb_info *info)
{
	struct dma_buf *dmabuf;

	#if LINUX_VERSION_CODE > KERNEL_VERSION(4,0,9)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	exp_info.ops = &new_fb_dma_buf_ops;
	exp_info.size = info->fix.smem_len;
	exp_info.flags = O_RDWR;
	exp_info.priv = info;
	dmabuf = dma_buf_export(&exp_info);
	#else
	dmabuf = dma_buf_export(info, &new_fb_dma_buf_ops, info->fix.smem_len, O_RDWR, NULL);
	#endif

	return dmabuf;
}
#else
static int dmabuf_attach(struct dma_buf *buf, struct device *dev,
			 struct dma_buf_attachment *attach)
{
	return 0;
}

static void dmabuf_detach(struct dma_buf *buf,
			  struct dma_buf_attachment *attach)
{
}

static struct sg_table *dmabuf_map_dma_buf(struct dma_buf_attachment *attach,
					   enum dma_data_direction dir)
{
	struct fb_info *priv = attach->dmabuf->priv;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	if (sg_alloc_table(sgt, 1, GFP_KERNEL)) {
		kfree(sgt);
		return NULL;
	}

	sg_dma_address(sgt->sgl) = priv->fix.smem_start;
	sg_dma_len(sgt->sgl) = priv->fix.smem_len;

	return sgt;
}

static void dmabuf_unmap_dma_buf(struct dma_buf_attachment *attach,
				 struct sg_table *sgt,
				 enum dma_data_direction dir)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static void dmabuf_release(struct dma_buf *buf)
{
}

static void *dmabuf_map(struct dma_buf *buf, unsigned long page)
{
	struct fb_info *priv = buf->priv;
	return priv->screen_base + page;
}

static void dmabuf_unmap(struct dma_buf *buf, unsigned long page, void *vaddr)
{
}

static void *dmabuf_map_atomic(struct dma_buf *buf, unsigned long page)
{
	return NULL;
}

static void dmabuf_unmap_atomic(struct dma_buf *buf, unsigned long page,
				 void *vaddr)
{
}

static void dmabuf_vm_open(struct vm_area_struct *vma)
{
}

static void dmabuf_vm_close(struct vm_area_struct *vma)
{
}

static int dmabuf_vm_fault(struct vm_fault *vmf)
{
	return 0;
}

static const struct vm_operations_struct dmabuf_vm_ops = {
	.open = dmabuf_vm_open,
	.close = dmabuf_vm_close,
	.fault = dmabuf_vm_fault,
};

static int dmabuf_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	pgprot_t prot = vm_get_page_prot(vma->vm_flags);
	struct fb_info *priv = buf->priv;

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &dmabuf_vm_ops;
	vma->vm_private_data = priv;
	vma->vm_page_prot = pgprot_writecombine(prot);

	return remap_pfn_range(vma, vma->vm_start, priv->fix.smem_start >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static void *dmabuf_vmap(struct dma_buf *buf)
{
	return NULL;
}

static void dmabuf_vunmap(struct dma_buf *buf, void *vaddr)
{
}

static const struct dma_buf_ops dmabuf_ops = {
	.attach = dmabuf_attach,
	.detach = dmabuf_detach,
	.map_dma_buf = dmabuf_map_dma_buf,
	.unmap_dma_buf = dmabuf_unmap_dma_buf,
	.release = dmabuf_release,
	.map_atomic = dmabuf_map_atomic,
	.unmap_atomic = dmabuf_unmap_atomic,
	.map = dmabuf_map,
	.unmap = dmabuf_unmap,
	.mmap = dmabuf_mmap,
	.vmap = dmabuf_vmap,
	.vunmap = dmabuf_vunmap,
};

struct dma_buf* new_tccfb_dmabuf_export(struct fb_info *info)
{
	struct dma_buf *dmabuf;

	#if LINUX_VERSION_CODE > KERNEL_VERSION(4,0,9)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	exp_info.ops = &dmabuf_ops;
	exp_info.size = info->fix.smem_len;
	exp_info.flags = O_RDWR;
	exp_info.priv = info;
	dmabuf = dma_buf_export(&exp_info);
	#else
	dmabuf = dma_buf_export(info, &dmabuf_ops, info->fix.smem_len, O_RDWR, NULL);
	#endif

	return dmabuf;
}
#endif
#endif	//CONFIG_DMA_SHARED_BUFFER

/*
 *  Frame buffer operations
 */
static struct fb_ops fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= fbX_check_var,
	.fb_set_par	= fbX_set_par,
	.fb_blank	= fbX_blank,
	.fb_setcolreg	= fbX_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_pan_display	= fbX_pan_display,
#ifdef CONFIG_DMA_SHARED_BUFFER
	.fb_dmabuf_export = new_tccfb_dmabuf_export,
#endif
};

#ifdef CONFIG_OF
static struct of_device_id fbX_of_match[] = {
	{ .compatible = "telechips,fb" },
	{}
};
MODULE_DEVICE_TABLE(of, fbX_of_match);
#endif

/*
 * fb_map_video_memory():
 *	Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *	allow palette and pixel writes to occur without flushing the
 *	cache.  Once this area is remapped, all virtual memory
 *	access to the video memory should occur at the new region.
 */
static int __init fb_map_video_memory(struct fb_info *info)
{
	struct fbX_par *par = info->par;
	struct device_node *mem_region;
	struct resource res;
	int ret;

	mem_region = of_parse_phandle(info->dev->of_node, "memory-region", 0);
	if (!mem_region) {
		dev_err(info->dev, "no memory regions\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret)
		dev_warn(info->dev, "failed to translate memory regions (%d)\n", ret);

	if (ret || resource_size(&res) == 0) {
		par->map_size  = info->var.xres_virtual * info->var.yres_virtual * (info->var.bits_per_pixel/ 8);
		par->map_cpu = dma_alloc_writecombine(info->dev, par->map_size, &par->map_dma, GFP_KERNEL);
		if (par->map_cpu == NULL) {
			pr_err("[ERR][FBX] %s: error dma_alloc map_cpu\n", __func__);
			goto exit;
		}
		pr_info("[INF][FBX] %s by dma_alloc_writecombine()\n", __func__);
	} else {
		par->map_dma = res.start;
		par->map_size = resource_size(&res);
		par->map_cpu = ioremap_nocache(par->map_dma, par->map_size);
		if (par->map_cpu == NULL) {
			pr_err("[ERR][FBX] %s: error ioremap map_cpu\n", __func__);
			goto exit;
		}
		pr_info("[INF][FBX] %s by ioremap_nocache()\n", __func__);
	}

	if (par->map_cpu) {
		/*
		 * prevent initial garbage on screen
		 */
#if !defined(CONFIG_TCC803X_CA7S) && !defined(CONFIG_TCC805X_CA53Q)
		memset_io(par->map_cpu, 0x00, par->map_size);
		pr_info("[INF][FBX] %s: clear fb mem\n", __func__);
#endif
		par->screen_dma		= par->map_dma;
		info->screen_base	= par->map_cpu;
		info->fix.smem_start  = par->screen_dma;
		info->fix.smem_len = par->map_size;
	}

exit:
	return par->map_cpu ? 0 : -ENOMEM;
}

static inline void fb_unmap_video_memory(struct fb_info *info)
{
	struct fbX_par *par = info->par;
	if (par->map_cpu)
		dma_free_writecombine(info->dev, par->map_size,par->map_cpu, par->map_dma);
}

static void fb_unregister_isr(struct fb_info *info)
{
	struct fbX_par *par = info->par;
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
	if(par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
		vioc_intr_clear(VIOC_INTR_RD0+get_vioc_index(par->pdata.rdma_info.blk_num), (0x1 << VIOC_RDMA_INTR_EOFR));
		vioc_intr_disable(par->pdata.rdma_info.irq_num,
				VIOC_INTR_RD0 + get_vioc_index(par->pdata.rdma_info.blk_num), (0x1 << VIOC_RDMA_INTR_EOFR));
		free_irq(par->pdata.rdma_info.irq_num, info);
	} else if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#else
	 if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#endif
	{
		vioc_intr_clear(get_vioc_index(par->pdata.ddc_info.blk_num), VIOC_DISP_INTR_DISPLAY);
		vioc_intr_disable(par->pdata.ddc_info.irq_num,
				get_vioc_index(par->pdata.ddc_info.blk_num), VIOC_DISP_INTR_DISPLAY);
		free_irq(par->pdata.ddc_info.irq_num, info);
	}
}

static int fb_register_isr(struct fb_info *info)
{
	struct fbX_par *par = info->par;
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
	if(par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
		vioc_intr_clear(VIOC_INTR_RD0+get_vioc_index(par->pdata.rdma_info.blk_num), (0x1 << VIOC_RDMA_INTR_EOFR));
		if(request_irq(par->pdata.rdma_info.irq_num, fbX_display_handler, IRQF_SHARED, info->fix.id, info) < 0) {
			pr_err("[ERR][FBX] error in %s: can not register isr \n", __func__);
			return -EINVAL;
		}
		vioc_intr_enable(par->pdata.rdma_info.irq_num,
				VIOC_INTR_RD0 + get_vioc_index(par->pdata.rdma_info.blk_num), (0x1 << VIOC_RDMA_INTR_EOFR));
	} else if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#else
	 if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
#endif
	{
		vioc_intr_clear(get_vioc_index(par->pdata.ddc_info.blk_num), VIOC_DISP_INTR_DISPLAY);
		if(request_irq(par->pdata.ddc_info.irq_num, fbX_display_handler, IRQF_SHARED, info->fix.id, info) < 0) {
			pr_err("[ERR][FBX] error in %s: can not register isr \n", __func__);
			return -EINVAL;
		}
		vioc_intr_enable(par->pdata.ddc_info.irq_num,
				get_vioc_index(par->pdata.ddc_info.blk_num), VIOC_DISP_INTR_DISPLAY);
	}
	return 0;
}

#ifdef CONFIG_OF
static int fb_dt_parse_data(struct fb_info *info)
{
	int ret = 0, index;
	struct fbX_par *par = info->par;
	struct device_node *np = NULL;
	int property_idx = 1;

#ifdef CONFIG_ARCH_TCC803X
	#ifdef CONFIG_FB_NEW_DISP1
	if(!strcmp("/fb@0",of_node_full_name(info->dev->of_node))){
		printk("%s fb0 disp1 support\n",__func__);
		property_idx = 2;
	}
	#endif
#endif

	if(info->dev->of_node != NULL) {
		if(of_property_read_u32(info->dev->of_node, "xres", &info->var.xres)) {
			pr_err("[ERR][FBX] error in %s: can nod find xres \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}
		info->var.xres_virtual = info->var.xres;

		if(of_property_read_u32(info->dev->of_node, "yres", &info->var.yres)) {
			pr_err("[ERR][FBX] error in %s: can nod find yres \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}

		if(of_property_read_u32(info->dev->of_node, "bpp", &info->var.bits_per_pixel)) {
			pr_err("[ERR][FBX] error in %s: can nod find bpp \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}

		if(of_property_read_u32(info->dev->of_node, "mode", &index)) {
			pr_err("[ERR][FBX] error in %s: can nod find mode \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}

		switch(index) {
		case FBX_SINGLE:
			info->var.yres_virtual = info->var.yres;
			break;
		case FBX_DOUBLE:
			info->var.yres_virtual = info->var.yres * 2;
			break;
		case FBX_TRIPLE:
			info->var.yres_virtual = info->var.yres * 3;
			break;
		default:
			pr_err("[ERR][FBX] error in %s: Invaild fb mode(%d)\n",
				__func__, index);
		}
		info->fix.line_length = info->var.xres * info->var.bits_per_pixel/8;

		if(of_property_read_u32(info->dev->of_node, "update-type", &par->pdata.FbUpdateType)) {
			pr_err("[ERR][FBX] error in %s: can nod find update-type \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}

		if(of_property_read_u32_index(info->dev->of_node, "device-priority", 0, &par->pdata.FbDeviceType)) {
			pr_err("[ERR][FBX] error in %s: can nod find update-type \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}

		np = of_parse_phandle(info->dev->of_node, "telechips,disp", 0);
		if(np != NULL) {
			of_property_read_u32_index(info->dev->of_node, "telechips,disp", property_idx, &par->pdata.ddc_info.blk_num);
		par->pdata.ddc_info.virt_addr =
			VIOC_DISP_GetAddress(par->pdata.ddc_info.blk_num);
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
		if(par->pdata.FbUpdateType != FBX_OVERLAY_UPDATE)
#endif
		{
			par->pdata.ddc_info.irq_num =
				irq_of_parse_and_map(np, get_vioc_index(par->pdata.ddc_info.blk_num));
		}
		par->pdata.vioc_clock = of_clk_get_by_name(np, "ddi-clk");
		BUG_ON(par->pdata.vioc_clock == NULL);
			par->pdata.FbWdmaPath = 0;
		}else {
			np = of_parse_phandle(info->dev->of_node, "telechips,wdma", 0);
			if(np == NULL) {
				pr_err("[ERR][FBX] error in %s: can not find telechips,wdma \n", __func__);
				ret = - ENODEV;
				goto err_dt_parse;
			}

			if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
				pr_err("[ERR][FBX] error in %s: can not support update type [%d]\n", __func__, par->pdata.FbUpdateType);
				ret = - ENODEV;
				goto err_dt_parse;
			}

			of_property_read_u32_index(info->dev->of_node, "telechips,wdma", 1, &par->pdata.wdma_info.blk_num);
			par->pdata.wdma_info.virt_addr =
				VIOC_WDMA_GetAddress(par->pdata.wdma_info.blk_num);
			par->pdata.vioc_clock = of_clk_get_by_name(info->dev->of_node, "ddi-clk");
			BUG_ON(par->pdata.vioc_clock == NULL);
			par->pdata.FbWdmaPath = 1;
			property_idx = 1;
		}

		if(par->pdata.ddc_info.virt_addr != NULL) {
		switch(par->pdata.ddc_info.blk_num) {
		case VIOC_DISP0 :
			par->pdata.ddc_clock = of_clk_get_by_name(np, "disp0-clk");
			break;
		case VIOC_DISP1 :
			par->pdata.ddc_clock = of_clk_get_by_name(np, "disp1-clk");
			break;
		case VIOC_DISP2 :
			par->pdata.ddc_clock = of_clk_get_by_name(np, "disp2-clk");
			break;
	#ifdef CONFIG_ARCH_TCC805X
		case VIOC_DISP3 :
			par->pdata.ddc_clock = of_clk_get_by_name(np, "disp3-clk");
			break;
	#endif
		default:
			pr_err("[ERR][FBX] error in %s: can not get ddc clock \n", __func__);
			par->pdata.ddc_clock = NULL;
			break;
		}
		BUG_ON(par->pdata.ddc_clock == NULL);
		}

#if defined(CONFIG_VIOC_PVRIC_FBDC)
		np = of_parse_phandle(info->dev->of_node, "telechips,fbdc", 0);
		if(!np) {
			pr_err("[ERR][FBX] error in %s: can not find telechips,fbdc \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}
		of_property_read_u32_index(info->dev->of_node, "telechips,fbdc", property_idx, &par->pdata.fbdc_info.blk_num);
		par->pdata.fbdc_info.virt_addr =
			VIOC_PVRIC_FBDC_GetAddress(get_vioc_index(par->pdata.fbdc_info.blk_num));
		pr_info("%s fbdc 0x%x\n", __func__, par->pdata.fbdc_info.virt_addr);
#endif

		np = of_parse_phandle(info->dev->of_node, "telechips,rdma", 0);
		if(!np) {
			pr_err("[ERR][FBX] error in %s: can not find telechips,rdma \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}
		of_property_read_u32_index(info->dev->of_node, "telechips,rdma", property_idx, &par->pdata.rdma_info.blk_num);
		par->pdata.rdma_info.virt_addr =
			VIOC_RDMA_GetAddress(get_vioc_index(par->pdata.rdma_info.blk_num));
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
		if( par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
			par->pdata.rdma_info.irq_num =
				irq_of_parse_and_map(np, get_vioc_index(par->pdata.rdma_info.blk_num));
		}
#endif
		np = of_parse_phandle(info->dev->of_node, "telechips,wmixer", 0);
		if(!np) {
			pr_err("[ERR][FBX] error in %s: can not find telechips,wmixer \n", __func__);
			ret = -ENODEV;
			goto err_dt_parse;
		}
		of_property_read_u32_index(info->dev->of_node, "telechips,wmixer", property_idx, &par->pdata.wmixer_info.blk_num);
		par->pdata.wmixer_info.virt_addr =
			VIOC_WMIX_GetAddress(get_vioc_index(par->pdata.wmixer_info.blk_num));
		VIOC_WMIX_GetOverlayPriority(par->pdata.wmixer_info.virt_addr, &par->pdata.FbLayerOrder);
#if defined(CONFIG_FB_NEW_HALFDISPLAY_SUPPORT)
		np = of_parse_phandle(info->dev->of_node, "telechips,scaler", 0);
#else
		np = NULL;
#endif

		if(np) {
			of_property_read_u32_index(info->dev->of_node, "telechips,scaler", property_idx, &par->pdata.scaler_info.blk_num);
			par->pdata.scaler_info.virt_addr =
				VIOC_SC_GetAddress(get_vioc_index(par->pdata.scaler_info.blk_num));
		} else
			pr_warn("[WAN][FBX] warning in %s: can not find telechips,scaler \n", __func__);

		np = of_find_node_by_name(info->dev->of_node, "fbx_region");
		if(!np)
			pr_warn("[WAN][FBX] warning in %s: can not find fbx_region \n", __func__);

		if(np) {
			if(of_property_read_u32(np, "x", &par->pdata.region.x)) {
				pr_err("[ERR][FBX] error in %s: can nod find 'x' of fbx_region  \n", __func__);
				ret = -ENODEV;
				goto err_dt_parse;
			}

			if(of_property_read_u32(np, "y", &par->pdata.region.y)) {
				pr_err("[ERR][FBX] error in %s: can nod find 'y' of fbx_region  \n", __func__);
				ret = -ENODEV;
				goto err_dt_parse;
			}

			if(of_property_read_u32(np, "width", &par->pdata.region.width)) {
				pr_err("[ERR][FBX] error in %s: can nod find 'width' of fbx_region  \n", __func__);
				ret = -ENODEV;
				goto err_dt_parse;
			}

			if(of_property_read_u32(np, "height", &par->pdata.region.height)) {
				pr_err("[ERR][FBX] error in %s: can nod find 'height' of fbx_region  \n", __func__);
				ret = -ENODEV;
				goto err_dt_parse;
			}
		} else {
			par->pdata.region.x = par->pdata.region.y = 0;
			par->pdata.region.width = info->var.xres;
			par->pdata.region.height = info->var.yres;
		}

		if(!IS_ERR_OR_NULL(par->pdata.vioc_clock)) {
		clk_prepare_enable(par->pdata.vioc_clock);
		}
		if(!IS_ERR_OR_NULL(par->pdata.ddc_clock)) {
		clk_prepare_enable(par->pdata.ddc_clock);
		}

		par->pdata.FbPowerState = 1;
	}

err_dt_parse:
	return ret;
}
#endif

static ssize_t fbX_ovp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fb_info *info = platform_get_drvdata(to_platform_device(dev));
	struct fbX_par *par = info->par;

	VIOC_WMIX_GetOverlayPriority(par->pdata.wmixer_info.virt_addr, &par->pdata.FbLayerOrder);

	return sprintf(buf, "%d\n", par->pdata.FbLayerOrder);
}

static ssize_t fbX_ovp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *info = platform_get_drvdata(to_platform_device(dev));
	struct fbX_par *par = info->par;
	unsigned int value = 0;

	value = simple_strtoul(buf, NULL, 10);
	if ((int)value < 0 || value > 29) {
		pr_err("[ERR][FBX] %s: invalid ovp%d\n", __func__, value);
		return count;
	}

	if(par->pdata.FbLayerOrder != value) {
		pr_info("[INF][FBX] %s: ovp%d -> %d \n",__func__, par->pdata.FbLayerOrder, value);

		par->pdata.FbLayerOrder = value;
		VIOC_WMIX_SetOverlayPriority(par->pdata.wmixer_info.virt_addr, par->pdata.FbLayerOrder);
		VIOC_WMIX_SetUpdate(par->pdata.wmixer_info.virt_addr);
	}

	return count;
}
static DEVICE_ATTR(ovp, S_IRUGO | S_IWUSR, fbX_ovp_show, fbX_ovp_store);

static struct attribute *fbX_dev_attrs[] = {
	&dev_attr_ovp.attr,
	NULL,
};

static struct attribute_group fbX_dev_attgrp = {
	.name = NULL,
	.attrs = fbX_dev_attrs,
};

static int __init fbX_probe (struct platform_device *pdev)
{
	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	struct device_node *panel_node;
	#endif
	unsigned int no_kernel_logo = 0;
	struct fb_info *info;
	struct fbX_par *par;
	int retval;

	info = framebuffer_alloc(sizeof(struct fbX_par), &pdev->dev);
	if (!info) {
		pr_err("[ERR][FBX] error in %s: can not allocate fb\n", __func__);
		retval = -ENOMEM;
		goto err_fb_probe;
	}

	par = info->par;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

#ifdef CONFIG_OF
	fb_dt_parse_data(info);
#endif
	if(of_property_read_u32(pdev->dev.of_node, "no-kernel-logo", &no_kernel_logo)) {
		pr_warn("[WARN][FBX] %s There is no no-kernel-logo property\n", __func__);
	}

	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	panel_node = of_graph_get_remote_node(pdev->dev.of_node, 0, -1);
	if (panel_node)
		par->panel = of_fb_find_panel(panel_node);
	if (panel_node && par->panel) {
		dev_info(
			info->dev,
			"[INFO][FBX] %s fb%d has framebuffer panel\r\n",
			__func__,
			of_alias_get_id(info->dev->of_node, "fb"));
		if (of_property_read_u32(
                        panel_node,
                        "lcdc-mux-select", &par->pdata.lcdc_mux) == 0) {
                        dev_info(
                                info->dev,
                                "[INFO][FBX] %s lcdc-mux-select=%d from remote node\r\n",
                                 __func__, par->pdata.lcdc_mux);
                } else {
                        dev_err(
                                info->dev,
                                "[ERROR][FBX] %s can not found lcdc-mux-select from remote node\r\n",
                                __func__);
                        retval = -ENODEV;
			goto err_fb_free;
		}
	}
	#endif

	snprintf(fbX_fix.id, sizeof(fbX_fix.id), "telechips,fb%d", of_alias_get_id(info->dev->of_node, "fb"));
	info->fix = fbX_fix;

	info->var.nonstd = 0;
	info->var.activate = par->pdata.FbUpdateType == FBX_NOWAIT_UPDATE ? FB_ACTIVATE_NOW : FB_ACTIVATE_VBL;
	info->var.accel_flags = 0;
	info->var.vmode = FB_VMODE_NONINTERLACED;

	info->fbops = &fb_ops;
	info->flags = FBINFO_DEFAULT;

	info->pseudo_palette = devm_kzalloc(&pdev->dev, sizeof(unsigned int) * 16, GFP_KERNEL);
	if(!info->pseudo_palette) {
		pr_err("[ERR][FBX] error in %s: can not allocate pseudo_palette\n", __func__);
		retval = -ENOMEM;
		goto err_fb_free;
	}

	retval =  fb_map_video_memory(info);
	if(retval < 0) {
		pr_err("[ERR][FBX] error in %s: can not remap framebuffer \n", __func__);
		retval = -ENOMEM;
		goto err_fb_free;
	}

	fbX_check_var(&info->var, info);
	fbX_set_par(info);

	if (register_framebuffer(info) < 0) {
		pr_err("[ERR][FBX] error in %s: can not register framebuffer device\n", __func__);
		retval = -EINVAL;
		goto err_palette_free;
	}

	if(no_kernel_logo) {
		pr_info("[INF][FBX] SKIP fb_show_logo\n");
	} else {
		if (fb_prepare_logo(info, FB_ROTATE_UR)) {
			/* Start display and show logo on boot */
			pr_info("[INF][FBX] fb_show_logo\n");
			// So, we use fb_alloc_cmap_gfp function(fb_default_camp(default_16_colors))
			fb_alloc_cmap_gfp(&info->cmap, 16, 0, GFP_KERNEL);
			fb_alloc_cmap_gfp(&info->cmap, 16, 0, GFP_KERNEL);
			fb_show_logo(info, FB_ROTATE_UR);
		}
	}

	atomic_set(&fb_waitq[info->node].state, 0);
	init_waitqueue_head(&fb_waitq[info->node].waitq);
	if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
		fb_register_isr(info);

	fb_info(info, "%s frame buffer device\n", info->fix.id);
	pr_err("%s : update_type = %d ",__func__, par->pdata.FbUpdateType);
	if(sysfs_create_group(&pdev->dev.kobj, &fbX_dev_attgrp) != 0)
		fb_warn(info, "failed to register attributes\n");

#if !defined(CONFIG_TCC803X_CA7S) && !defined(CONFIG_TCC805X_CA53Q)
	fbX_activate_var((par->map_dma + (info->var.xres * info->var.yoffset * info->var.bits_per_pixel/8)), &info->var, info->par);
#endif
	fbX_set_par(info);

	return 0;

err_palette_free:
	devm_kfree(&pdev->dev, info->pseudo_palette);

err_fb_free:
	framebuffer_release(info);

err_fb_probe:
	return retval;
}

/*
 *  Cleanup
 */
static int fbX_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct fbX_par *par = info->par;
	if (info) {
		switch(par->pdata.FbUpdateType) {
		case FBX_M2M_RDMA_UPDATE:
		case FBX_ATTACH_UPDATE:
			if(par->pdata.filp)
				filp_close(par->pdata.filp, 0);
		case FBX_RDMA_UPDATE:
		case FBX_OVERLAY_UPDATE:
			fb_unregister_isr(info);
			break;
		case FBX_NOWAIT_UPDATE:
		default:
			break;
		}
		atomic_set(&fb_waitq[info->node].state, 0);

		fb_unmap_video_memory(info);
		if(!IS_ERR_OR_NULL(par->pdata.vioc_clock)) {
		clk_put(par->pdata.vioc_clock);
		}
		if(!IS_ERR_OR_NULL(par->pdata.ddc_clock)) {
		clk_put(par->pdata.ddc_clock);
		}

		unregister_framebuffer(info);
		framebuffer_release(info);
	}
	return 0;
}

#if defined(CONFIG_FB_PANEL_LVDS_TCC)
static int fbx_set_display_controller(struct fb_info *info,
				struct videomode *vm)
{
	stLTIMING stTimingParam;
	stLCDCTR stCtrlParam;
	bool interlace;
	u32 tmp_sync;
	u32 vactive;
	struct fbX_par *par;
	int ret = -ENODEV;

	u32 channel;

	if (!info)
		goto err_null_pointer;
	par = info->par;
	if (!par)
		goto err_null_pointer;

	/* Display MUX */
	VIOC_CONFIG_LCDPath_Select(
		get_vioc_index(
			par->pdata.ddc_info.blk_num),
			par->pdata.lcdc_mux);
	dev_info(
		info->dev,
		"[INFO][FBX] %s display device(%d) to connect mux(%d)\r\n",
		__func__,
		get_vioc_index(par->pdata.ddc_info.blk_num),
		par->pdata.lcdc_mux);

	memset(&stCtrlParam, 0, sizeof(stLCDCTR));
	memset(&stTimingParam, 0, sizeof(stLTIMING));
	interlace = vm->flags & DISPLAY_FLAGS_INTERLACED;
	stCtrlParam.iv = (vm->flags & DISPLAY_FLAGS_VSYNC_LOW) ? 1 : 0;
	stCtrlParam.ih = (vm->flags & DISPLAY_FLAGS_HSYNC_LOW) ? 1 : 0;
	stCtrlParam.dp = (vm->flags & DISPLAY_FLAGS_DOUBLECLK) ? 1 : 0;

	if (interlace) {
		stCtrlParam.tv = 1;
		stCtrlParam.advi = 1;
	} else {
		stCtrlParam.ni = 1;
	}

	/* Check vactive */
	vactive = interlace ? (vm->vactive <<= 1) : vm->vactive;

	dev_info(
		info->dev,
		"[INFO][FBX] %s %dx%d panel\r\n",
		__func__, vm->hactive, vactive);

	stTimingParam.lpc = vm->hactive;
	stTimingParam.lewc = vm->hfront_porch;
	stTimingParam.lpw = (vm->hsync_len > 0) ? (vm->hsync_len - 1) : 0;
	stTimingParam.lswc = vm->hback_porch;

	if (interlace) {
		tmp_sync = vm->vsync_len << 1;
		stTimingParam.fpw = (tmp_sync > 0) ? (tmp_sync - 1) : 0;
		tmp_sync = vm->vback_porch << 1;
		stTimingParam.fswc = (tmp_sync > 0) ? (tmp_sync - 1) : 0;
		stTimingParam.fewc = vm->vfront_porch << 1;
		stTimingParam.fswc2 = stTimingParam.fswc + 1;
		stTimingParam.fewc2 =
			(stTimingParam.fewc > 0) ? (stTimingParam.fewc - 1) : 0;
		#if 0
		if (
			mode->vtotal == 1250 && vm->hactive == 1920 &&
			vm.vactive == 540)
			/* VIC 1920x1080@50i 1250 vtotal */
			stTimingParam.fewc -= 2;
		#endif
	} else {
		stTimingParam.fpw = (vm->vsync_len > 0) ? (vm->vsync_len - 1) : 0;
		stTimingParam.fswc =
			(vm->vback_porch > 0) ? (vm->vback_porch - 1) : 0;
		stTimingParam.fewc =
			(vm->vfront_porch > 0) ? (vm->vfront_porch - 1) : 0;
		stTimingParam.fswc2 = stTimingParam.fswc;
		stTimingParam.fewc2 = stTimingParam.fewc;
	}

	/* Common Timing Parameters */
	stTimingParam.flc = (vactive > 0) ? (vactive - 1) : 0;
	stTimingParam.fpw2 = stTimingParam.fpw;
	stTimingParam.flc2 = stTimingParam.flc;

	/* swreset display device */
	VIOC_CONFIG_SWReset(
		par->pdata.ddc_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(
		par->pdata.ddc_info.blk_num, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_SWReset(par->pdata.wmixer_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(par->pdata.wmixer_info.blk_num, VIOC_CONFIG_CLEAR);

	//vioc_reset_rdma_on_display_path(par->PDATA.DispNum);
	channel =
		get_vioc_index(par->pdata.rdma_info.blk_num) -
		(4 * get_vioc_index(par->pdata.ddc_info.blk_num));
	VIOC_WMIX_SetPosition(
		par->pdata.wmixer_info.virt_addr, channel, par->pdata.region.x,
		par->pdata.region.y);
	VIOC_WMIX_SetChromaKey(
		par->pdata.wmixer_info.virt_addr, channel, 1/*ON*/,
		0x0, 0x0, 0x0, 0xF8, 0xFC, 0xF8);

	VIOC_WMIX_SetOverlayPriority(
		par->pdata.wmixer_info.virt_addr, par->pdata.FbLayerOrder);
	VIOC_WMIX_SetSize(
		par->pdata.wmixer_info.virt_addr, vm->hactive, vactive);

	VIOC_WMIX_SetUpdate(par->pdata.wmixer_info.virt_addr);

	VIOC_DISP_SetTimingParam(
		par->pdata.ddc_info.virt_addr, &stTimingParam);
	VIOC_DISP_SetControlConfigure(
		par->pdata.ddc_info.virt_addr, &stCtrlParam);

	/* PXDW
	 * YCC420 with stb pxdw is 27
	 * YCC422 with stb is pxdw 21, with out stb is 8
	 * YCC444 && RGB with stb is 23, with out stb is 12
	 * TSVFB can only support RGB as format of the display device.
	 */
	VIOC_DISP_SetPXDW(par->pdata.ddc_info.virt_addr, 12);

	VIOC_DISP_SetSize(
		par->pdata.ddc_info.virt_addr, vm->hactive, vactive);
	VIOC_DISP_SetBGColor(par->pdata.ddc_info.virt_addr, 0, 0, 0, 0);

finish:

	return 0;

err_null_pointer:
	return -1;
}
#endif

static int fbx_turn_on_resource(struct fb_info *info)
{
	struct fbX_par *par = info->par;
	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	struct videomode vm;

	if(par->panel) {
		if(fb_panel_get_mode(par->panel, &vm) < 0) {
			goto next_step;
		}
		fbx_set_display_controller(info, &vm);
		if(!IS_ERR_OR_NULL(par->pdata.ddc_clock)) {
			clk_set_rate(par->pdata.ddc_clock, vm.pixelclock);
			dev_info(
				info->dev,
				"%s set ddc clock to %dHz\r\n",
				__func__, clk_get_rate(par->pdata.ddc_clock));
		}
	}
next_step:
	#endif
	if(!IS_ERR_OR_NULL(par->pdata.ddc_clock)) {
		clk_prepare_enable(par->pdata.ddc_clock);
	}

	if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
		fb_register_isr(info);

	par->pdata.FbPowerState = 1;
	fbX_pan_display(&info->var, info);
}

static int fbx_turn_off_resource(struct fb_info *info)
{
	struct fbX_par *par = info->par;
	int scnum = -1;

	par->pdata.FbPowerState = 0;

	if(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)
		fb_unregister_isr(info);

	/* Disable RDMA */
	VIOC_RDMA_SetImageDisableNW(par->pdata.rdma_info.virt_addr);

	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	if(par->panel) {
		/* Turn off display controller */
		if(VIOC_DISP_Get_TurnOnOff(par->pdata.ddc_info.virt_addr)) {
			VIOC_DISP_TurnOff(par->pdata.ddc_info.virt_addr);
			VIOC_DISP_sleep_DisplayDone(
				par->pdata.ddc_info.virt_addr);
		}
		VIOC_CONFIG_SWReset(
			par->pdata.ddc_info.blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(
			par->pdata.ddc_info.blk_num, VIOC_CONFIG_CLEAR);
	}
	#endif

	if(par->pdata.scaler_info.virt_addr) {
		scnum =
			VIOC_CONFIG_GetScaler_PluginToRDMA(
				par->pdata.rdma_info.blk_num);
	}
	if(scnum >= VIOC_SCALER0) {
		VIOC_CONFIG_PlugOut(scnum);
		VIOC_CONFIG_SWReset(scnum, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scnum, VIOC_CONFIG_CLEAR);
		dev_info(
			info->dev,
			"[INFO][FBX] reset rdma_%d with Scaler-%d\n",
			get_vioc_index(par->pdata.rdma_info.blk_num),
			get_vioc_index(scnum));
	}

	VIOC_CONFIG_SWReset(par->pdata.rdma_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(par->pdata.rdma_info.blk_num, VIOC_CONFIG_CLEAR);
	dev_info(
		info->dev, "[INFO][FBX] reset rdma_%d\n",
		get_vioc_index(par->pdata.rdma_info.blk_num));

	if(!IS_ERR_OR_NULL(par->pdata.ddc_clock)) {
		clk_disable_unprepare(par->pdata.ddc_clock);
	}
	return 0;
}

#ifdef CONFIG_PM
/**
 *	fb_suspend - Optional but recommended function. Suspend the device.
 *	@dev: platform device
 *	@msg: the suspend event code.
 *
 *      See Documentation/power/devices.txt for more information
 */
static int fbX_suspend(struct platform_device *dev, pm_message_t msg)
{
	struct fb_info *info = platform_get_drvdata(dev);
	struct fbX_par *par = info->par;

        if (msg.event == par->pm_state)
                return 0;

	console_lock();
	fb_set_suspend(info, 1);
	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	if(par->panel) {
		fb_panel_disable(par->panel);
		fb_panel_unprepare(par->panel);
	}
	#endif
	fbx_turn_off_resource(info);
	par->pm_state = msg.event;
	console_unlock();

	return 0;
}

/**
 *	fb_resume - Optional but recommended function. Resume the device.
 *	@dev: platform device
 *
 *      See Documentation/power/devices.txt for more information
 */
static int fbX_resume(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);
	struct fbX_par *par = info->par;


	console_lock();
	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	if(par->panel) {
		fb_panel_prepare(par->panel);
	}
	#endif
	fbx_turn_on_resource(info);
        fb_set_suspend(info, 0);

	#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	if(par->panel) {
		fb_panel_enable(par->panel);
	}
	#endif

	par->pm_state = PM_EVENT_ON;
	console_unlock();
	/* resume here */
	return 0;
}
#else
#define fbX_suspend NULL
#define fbX_resume NULL
#endif /* CONFIG_PM */

static struct platform_driver fbX_driver = {
	.probe = fbX_probe,
	.remove = fbX_remove,
	.suspend = fbX_suspend, /* optional but recommended */
	.resume = fbX_resume,   /* optional but recommended */
	.driver = {
		.name = "fb",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(fbX_of_match),
#endif
	},
};

static int __init fbX_init(void)
{
	int ret;
	ret = platform_driver_register(&fbX_driver);
	return ret;
}

static void __exit fbX_exit(void)
{
	platform_driver_unregister(&fbX_driver);
}

module_init(fbX_init);
module_exit(fbX_exit);

MODULE_AUTHOR("inbest <inbest@telechips.com>");
MODULE_DESCRIPTION("telechips framebuffer driver");
MODULE_LICENSE("GPL");
