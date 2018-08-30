/*
 * linux/drivers/video/tcc/tcc_vioc_fb.c
 *
 * Based on:    Based on s3c2410fb.c, sa1100fb.c and others
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC LCD Controller Frame Buffer Driver
 *
 * Copyright (C) 2008- Telechips
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

#include <asm/io.h>

#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/rbtree.h>
#include <linux/dma-buf.h>
#include <linux/rtmutex.h>
#include <linux/debugfs.h>
#include <linux/memblock.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#endif

#include <soc/tcc/pmap.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_config.h>

/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { printk( "extfb: " msg); }

#define FB_NUM_BUFFERS		3
#define PANEL_MAX_NUM		2
#define EXTFB_STAGE_FB               0
#define EXTFB_STAGE_OUTPUTSTARTER    1

static struct lcd_panel *extfb_panel[PANEL_MAX_NUM];

#ifdef CONFIG_OF
static struct of_device_id extfb_of_match[] = {
	{ .compatible = "telechips,ext-fb" },
	{}
};
MODULE_DEVICE_TABLE(of, extfb_of_match);
#endif

extern void tca_fb_wait_for_vsync(struct tcc_dp_device *pdata);
extern int tca_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
extern void tcafb_activate_var(struct tccfb_info *fbi,  struct fb_var_screeninfo *var);
extern void tca_fb_activate_var(unsigned int dma_addr,  struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data);

#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
extern void hdmi_prepare_blank(void);
extern void hdmi_phy_standby(void);
extern void hdmi_set_activate_callback(void(*)(int, int), int, int);
extern void tccfb_extoutput_activate(int fb, int stage);
extern void tca_vioc_displayblock_timing_set(unsigned int outDevice, struct tcc_dp_device *pDisplayInfo,  struct lcdc_timimg_parms_t *mode);
extern void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo, int specific_pclk);
extern void tca_vioc_displayblock_powerOff(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_disable(struct tcc_dp_device *pDisplayInfo);
#endif // CONFIG_TCC_HDMI_DRIVER_V2_0

static irqreturn_t extfb_display_handler(int irq, void *dev_id)
{
	struct tccfb_info *fbdev = dev_id;
	unsigned int dispblock_status;
	#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
	static unsigned int VIOCFifoUnderRun = 0;
	#endif

	if (dev_id == NULL) {
		pr_err("%s irq: %d dev_id:%p \n",__func__, irq, dev_id);
		return IRQ_HANDLED;
	}

	fbdev->vsync_timestamp = ktime_get();
	
	dispblock_status = vioc_intr_get_status(fbdev->pdata.lcdc_number);
//	dprintk("%s  sync_struct.state:0x%x dispblock_status:0x%x, fbdev:%p \n",
//				__func__, fbdev->pdata.Mdp_data.pandisp_sync.state, dispblock_status, fbdev);

	if (dispblock_status & (1<<VIOC_DISP_INTR_RU)) {
		vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_RU));
		if (fbdev->active_vsync)
			schedule_work(&fbdev->vsync_work);
		if (fbdev->pdata.Mdp_data.pandisp_sync.state == 0) {
			fbdev->pdata.Mdp_data.pandisp_sync.state = 1;
			wake_up_interruptible(&fbdev->pdata.Mdp_data.pandisp_sync.waitq);
		}
	}

	#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
	if(dispblock_status & (1<<VIOC_DISP_INTR_FU)) {
		pr_err(" FIFO UNDERUN STATUS:0x%x device type:%d \n",dispblock_status, fbdev->pdata.Mdp_data.DispDeviceType);

		vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_FU));
		vioc_intr_disable(irq, fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_FU));
		VIOCFifoUnderRun++;
	}
	#endif

	if(dispblock_status & (1<<VIOC_DISP_INTR_DD)) {
		pr_info("%s DISABEL DONE Lcdc_num:%d 0x%p  STATUS:0x%x  \n",
			__func__,fbdev->pdata.lcdc_number, fbdev->pdata.Mdp_data.ddc_info.virt_addr, dispblock_status);

		vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_DD));
		if (fbdev->pdata.Mdp_data.disp_dd_sync.state == 0) {
			fbdev->pdata.Mdp_data.disp_dd_sync.state = 1;
			wake_up_interruptible(&fbdev->pdata.Mdp_data.disp_dd_sync.waitq);
		}
		#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
		VIOCFifoUnderRun = 0;
		#endif
	}

	if(dispblock_status & (1<<VIOC_DISP_INTR_SREQ))
		vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_SREQ));
	
	return IRQ_HANDLED;
}

int extfb_interrupt_reg(char SetClear, struct tccfb_info *info)
{
	struct fb_info *fbinfo = info->fb;
	const char *name = kasprintf(GFP_KERNEL,"extfb%d", (fbinfo->node-1));
	int ret = 0;

	if(SetClear) {
		vioc_intr_clear(info->pdata.lcdc_number, 0x39);
		ret = request_irq(info->pdata.Mdp_data.ddc_info.irq_num, extfb_display_handler, IRQF_SHARED, name, info);
		vioc_intr_enable(info->pdata.Mdp_data.ddc_info.irq_num, info->pdata.lcdc_number, VIOC_DISP_INTR_DISPLAY);
	} else {
		vioc_intr_disable(info->pdata.Mdp_data.ddc_info.irq_num, info->pdata.lcdc_number, VIOC_DISP_INTR_DISPLAY);
		free_irq( info->pdata.Mdp_data.ddc_info.irq_num, info);
	}

	return ret;
}

static int extfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	/* validate bpp */
	if (var->bits_per_pixel > 32)
		var->bits_per_pixel = 32;
	else if (var->bits_per_pixel < 16)
		var->bits_per_pixel = 16;

	/* set r/g/b positions */
	if (var->bits_per_pixel == 16) {
		var->red.offset 	= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length 	= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		var->transp.length	= 0;
	} else if (var->bits_per_pixel == 32) {
		var->red.offset 	= 16;
		var->green.offset	= 8;
		var->blue.offset	= 0;
		var->transp.offset	= 24;
		var->red.length 	= 8;
		var->green.length	= 8;
		var->blue.length	= 8;
		var->transp.length	= 8;
	} else {
		var->red.length 	= var->bits_per_pixel;
		var->red.offset 	= 0;
		var->green.length	= var->bits_per_pixel;
		var->green.offset	= 0;
		var->blue.length	= var->bits_per_pixel;
		var->blue.offset	= 0;
		var->transp.length	= 0;
	}

	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	return 0;
}

/* extfb_pan_display
 *
 * pandisplay (set) the controller from the given framebuffer
 * information
*/
static int extfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	tca_fb_pan_display(var, info);
	return 0;
}

/*
 * extfb_set_par - Optional function. Alters the hardware state.
 * @info: frame buffer structure that represents a single frame buffer
 *
 */
static int extfb_set_par(struct fb_info *info)
{
	struct tccfb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;

	printk("- extfb_set_par pwr:%d  \n", fbi->pdata.Mdp_data.FbPowerState);

	if (var->bits_per_pixel == 16)
		fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;
	else if (var->bits_per_pixel == 32)
		fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;
	else
		fbi->fb->fix.visual = FB_VISUAL_PSEUDOCOLOR;

	fbi->fb->fix.line_length = (var->xres*var->bits_per_pixel)/8;
	if(fbi->fb->var.rotate != 0)	{
		pr_info("fb rotation not support \n");
		return -1;
	}
	
	return 0;
}

static int extfb_ioctl(struct fb_info *info, unsigned int cmd,unsigned long arg)
{
	struct tccfb_info *ptccfb_info = info->par;

	switch(cmd)
	{
		case FBIO_WAITFORVSYNC:
			tca_fb_wait_for_vsync(&ptccfb_info->pdata.Mdp_data);
			break;
		#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
		case TCC_LCDC_SET_HDMI_R2YMD:
			{
				struct tcc_dp_device *pdp_data = NULL;
				unsigned int r2ymd;
				if(copy_from_user((void *)&r2ymd, (const void *)arg, sizeof(r2ymd)))
					return -EFAULT;

				if((ptccfb_info->pdata.Mdp_data.FbPowerState != true) || (ptccfb_info->pdata.Mdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else
					pr_err("error in %s(%d) : can't find HDMI voic display block \n", __func__, __LINE__);

				if(pdp_data && ((r2ymd >> 16) == TCC_LCDC_SET_HDMI_R2YMD_MAGIC))
					VIOC_DISP_SetR2YMD(pdp_data->ddc_info.virt_addr, (unsigned char)(r2ymd & 0xFF));   
			}
			break;

		case TCC_LCDC_HDMI_START:
			{
				struct tcc_dp_device *pdp_data = NULL;
				if((ptccfb_info->pdata.Mdp_data.FbPowerState != true) || (ptccfb_info->pdata.Mdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else
					pr_err("error in %s(%d) : can't find HDMI voic display block \n", __func__, __LINE__);

				if(pdp_data)
				{
					if(pdp_data->FbPowerState == true)
					{
						pr_info("HDMI voic display block power off  \n");
						tca_vioc_displayblock_disable(pdp_data);
						tca_vioc_displayblock_powerOff(pdp_data);
					}

					pdp_data->DispDeviceType = TCC_OUTPUT_HDMI;
					tca_vioc_displayblock_powerOn(pdp_data, 0);
				}
			}
			break;

		case TCC_LCDC_HDMI_TIMING:
			{
				struct tcc_dp_device *pdp_data = NULL;
				struct lcdc_timimg_parms_t lcdc_timing;
				if (copy_from_user((void*)&lcdc_timing, (const void*)arg, sizeof(struct lcdc_timimg_parms_t)))
					return -EFAULT;

				if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else
					pr_err("error in %s(%d) : can't find HDMI voic display block \n", __func__, __LINE__);

				if(pdp_data != NULL) {
					tca_vioc_displayblock_timing_set(VIOC_OUTCFG_HDMI, pdp_data, &lcdc_timing);
					hdmi_set_activate_callback(tccfb_extoutput_activate, info->node, EXTFB_STAGE_FB);
				}
			}
			break;


		case TCC_LCDC_HDMI_END:
			{
				struct tcc_dp_device *pdp_data = NULL;
				if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else
					pr_err("error in %s(%d) : can't find HDMI voic display block \n", __func__, __LINE__);

				if(pdp_data != NULL){
					tca_vioc_displayblock_disable(pdp_data);
					tca_vioc_displayblock_powerOff(pdp_data);
					pdp_data->DispDeviceType = TCC_OUTPUT_NONE;
				}
				hdmi_phy_standby();
			}
			break;
		#endif
		default:
			dprintk("ioctl: Unknown [%d/0x%X]", cmd, cmd);
			break;
	}

	return 0;
}

static void schedule_palette_update(struct tccfb_info *fbi,
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

static int extfb_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	unsigned int val;
	struct tccfb_info *fbi = info->par;

	dprintk("setcol: regno=%d, rgb=%d,%d,%d\n", regno, red, green, blue);

	switch (fbi->fb->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseuo-palette */
		if (regno < 16) {
			u32 *pal = fbi->fb->pseudo_palette;

			val  = chan_to_field(red,   &fbi->fb->var.red);
			val |= chan_to_field(green, &fbi->fb->var.green);
			val |= chan_to_field(blue,  &fbi->fb->var.blue);

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
			schedule_palette_update(fbi, regno, val);
		}
		break;
	default:
		return 1;   /* unknown type */
	}

	return 0;
}


/**
 *  extfb_blank
 *	@blank_mode: the blank mode we want.
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *	blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *	video mode which doesn't support it. Implements VESA suspend
 *	and powerdown modes on hardware that supports disabling hsync/vsync:
 *	blank_mode == 2: suspend vsync
 *	blank_mode == 3: suspend hsync
 *	blank_mode == 4: powerdown
 *
 *	Returns negative errno on error, or zero on success.
 *
 */
static int extfb_blank(int blank_mode, struct fb_info *info)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
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

static void *dmabuf_kmap(struct dma_buf *buf, unsigned long page)
{
	struct fb_info *priv = buf->priv;
	return priv->screen_base + page;
}

static void dmabuf_kunmap(struct dma_buf *buf, unsigned long page, void *vaddr)
{
}

static void *dmabuf_kmap_atomic(struct dma_buf *buf, unsigned long page)
{
	return NULL;
}

static void dmabuf_kunmap_atomic(struct dma_buf *buf, unsigned long page,
				 void *vaddr)
{
}

static void dmabuf_vm_open(struct vm_area_struct *vma)
{
}

static void dmabuf_vm_close(struct vm_area_struct *vma)
{
}

static int dmabuf_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
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
	.kmap_atomic = dmabuf_kmap_atomic,
	.kunmap_atomic = dmabuf_kunmap_atomic,
	.kmap = dmabuf_kmap,
	.kunmap = dmabuf_kunmap,
	.mmap = dmabuf_mmap,
	.vmap = dmabuf_vmap,
	.vunmap = dmabuf_vunmap,
};

struct dma_buf* extfb_dmabuf_export(struct fb_info *info)
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
#endif	//CONFIG_DMA_SHARED_BUFFER


static struct fb_ops extfb_ops = {
	.owner			= THIS_MODULE,
	.fb_check_var	= extfb_check_var,
	.fb_set_par		= extfb_set_par,
	.fb_blank		= extfb_blank,
	.fb_setcolreg	= extfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_ioctl		= extfb_ioctl,
	.fb_pan_display = extfb_pan_display,
	#ifdef CONFIG_DMA_SHARED_BUFFER
	.fb_dmabuf_export = extfb_dmabuf_export,
	#endif	
};

/*
 * extfb_map_video_memory():
 *	Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *	allow palette and pixel writes to occur without flushing the
 *	cache.  Once this area is remapped, all virtual memory
 *	access to the video memory should occur at the new region.
 */

static int __init extfb_map_video_memory(struct tccfb_info *fbi)
{
	struct device_node *of_node = fbi->dev->of_node;
	struct device_node *mem_region;
	struct resource res;
	int ret;

	mem_region = of_parse_phandle(of_node, "memory-region", 0);
	if (!mem_region) {
		dev_err(fbi->dev, "no memory regions\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret)
		dev_warn(fbi->dev, "failed to translate memory regions (%d)\n", ret);

	if (ret || resource_size(&res) == 0) {
		fbi->map_size  = fbi->fb->var.xres_virtual * fbi->fb->var.yres_virtual * (fbi->fb->var.bits_per_pixel/ 8);
		fbi->map_cpu = dma_alloc_writecombine(fbi->dev, fbi->map_size, &fbi->map_dma, GFP_KERNEL);
		if (fbi->map_cpu == NULL) {
			pr_err("%s: error dma_alloc map_cpu\n", __func__);
			goto exit;
		}
		dprintk("%s by dma_alloc_writecombine()\n", __func__);
	} else {
		fbi->map_dma = res.start;
		fbi->map_size = resource_size(&res);
		fbi->map_cpu = ioremap_nocache(fbi->map_dma, fbi->map_size);
		if (fbi->map_cpu == NULL) {
			pr_err("%s: error ioremap map_cpu\n", __func__);
			goto exit;
		}
		dprintk("%s by ioremap_nocache()\n", __func__);
	}

	#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	dprintk("%s: fbi(%p) phy(0x%llx)=>virt(%p) size(0x%x)\n", __func__, fbi, fbi->map_dma, fbi->map_cpu, fbi->map_size);
	#else
	dprintk("%s: fbi(%p) phy(0x%x)=>virt(%p) size(0x%x)\n", __func__, fbi, fbi->map_dma, fbi->map_cpu, fbi->map_size);
	#endif

	if (fbi->map_cpu) {
		memset_io(fbi->map_cpu, 0x00, fbi->map_size);
		dprintk("%s: clear fb mem\n", __func__);

		fbi->screen_dma		= fbi->map_dma;
		fbi->fb->screen_base	= fbi->map_cpu;
		fbi->fb->fix.smem_start  = fbi->screen_dma;
		fbi->fb->fix.smem_len = fbi->map_size;
	}

exit:
	return fbi->map_cpu ? 0 : -ENOMEM;
}

static inline void extfb_unmap_video_memory(struct tccfb_info *fbi)
{
	if (fbi->map_cpu)
		dma_free_writecombine(fbi->dev,fbi->map_size,fbi->map_cpu, fbi->map_dma);
}

static int ext_dp_dt_parse_data(struct tccfb_info *info)
{
	int ret = 0;
	struct device_node *dev_np;
		
	if (info->dev->of_node) {
		// Get default display number.					 
		if (of_property_read_u32(info->dev->of_node, "telechips,extfb_num", &info->pdata.lcdc_number)){
			pr_err( "could not find  telechips,fbdisplay_nubmer\n");
			ret = -ENODEV;
		}

		dev_np = of_find_node_by_name(info->dev->of_node, "extfb0");
		if (!dev_np) {
			pr_err( "could not find fb node number %d\n", info->pdata.lcdc_number);
			return -ENODEV;
		}

		tcc_vioc_display_dt_parse(dev_np, &info->pdata.Mdp_data);
		info->pdata.lcdc_number = info->pdata.Mdp_data.DispNum;
	}
	return ret;
}
	
static char extfb_driver_name[]="extfb";
static int extfb_probe(struct platform_device *pdev)
{
	struct tccfb_info *info;
	struct fb_info *fbinfo;

	int ret = 0;

	fbinfo = framebuffer_alloc(sizeof(struct tccfb_info), &pdev->dev);
	info = fbinfo->par;
	info->fb = fbinfo;
	info->dev = &pdev->dev;

	platform_set_drvdata(pdev, info);
	ext_dp_dt_parse_data(info);

	if(extfb_panel[fbinfo->node] == NULL) {
		pr_err("extfb: no panel deivce\n");
		goto free_framebuffer;
	}

	pr_info("\x1b[1;38m   LCD panel is %s %s %d x %d \x1b[0m \n",
		extfb_panel[fbinfo->node]->manufacturer, extfb_panel[fbinfo->node]->name,
		extfb_panel[fbinfo->node]->xres, extfb_panel[fbinfo->node]->yres);

	strcpy(fbinfo->fix.id, extfb_driver_name);

	fbinfo->fix.type			= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.type_aux		= 0;
	fbinfo->fix.xpanstep		= 0;
	fbinfo->fix.ypanstep		= 1;
	fbinfo->fix.ywrapstep		= 0;
	fbinfo->fix.accel			= FB_ACCEL_NONE;
	fbinfo->fix.visual			= FB_VISUAL_TRUECOLOR;
	fbinfo->fix.type			= FB_TYPE_PACKED_PIXELS;

	fbinfo->var.nonstd			= 0;
	fbinfo->var.activate		= FB_ACTIVATE_VBL;		// 160614 inbest
	fbinfo->var.accel_flags		= 0;
	fbinfo->var.vmode			= FB_VMODE_NONINTERLACED;

	fbinfo->fbops				= &extfb_ops;
	fbinfo->flags				= FBINFO_FLAG_DEFAULT;

	fbinfo->var.xres			= extfb_panel[fbinfo->node]->xres;
	fbinfo->var.yres			= extfb_panel[fbinfo->node]->yres;

	fbinfo->var.xres_virtual	= fbinfo->var.xres;
	fbinfo->var.yres_virtual	= fbinfo->var.yres * FB_NUM_BUFFERS;
	fbinfo->var.bits_per_pixel	= 32;
	fbinfo->fix.line_length 	= fbinfo->var.xres * fbinfo->var.bits_per_pixel/8;

	fbinfo->pseudo_palette          = devm_kzalloc(&pdev->dev, sizeof(u32) * 16, GFP_KERNEL);
	if (!fbinfo->pseudo_palette) {
		printk( KERN_ERR "Failed to allocate pseudo_palette\r\n");
		ret = -ENOMEM;
		goto free_framebuffer;
	}

	info->output_on = true;
	info->pdata.Mdp_data.DispOrder = DD_MAIN;
	info->pdata.Mdp_data.FbPowerState = true;
	info->pdata.Mdp_data.FbUpdateType = FB_RDMA_UPDATE;
	info->pdata.Mdp_data.DispDeviceType = TCC_OUTPUT_LCD;

	extfb_check_var(&fbinfo->var, fbinfo);

	/* Initialize video memory */
	ret = extfb_map_video_memory(info);
	if (ret  < 0) {
		printk( KERN_ERR "Failed to allocate video RAM: %d\n", ret);
		ret = -ENOMEM;
		goto free_palette;
	}

	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		pr_err(KERN_ERR "Failed to register framebuffer device: %d\n", ret);
		goto free_video_memory;
	}

	if (fb_prepare_logo(fbinfo, FB_ROTATE_UR)) {
		/* Start display and show logo on boot */
		pr_info("fb_show_logo\n");
		// So, we use fb_alloc_cmap_gfp function(fb_default_camp(default_16_colors))
		fb_alloc_cmap_gfp(&fbinfo->cmap, 16, 0, GFP_KERNEL);
		fb_show_logo(fbinfo, FB_ROTATE_UR);
	}

	info->pdata.Mdp_data.pandisp_sync.state = 1;
	init_waitqueue_head(&info->pdata.Mdp_data.pandisp_sync.waitq);

	pr_info("fb%d: %s frame buffer device info->dev:0x%p  \n", fbinfo->node, fbinfo->fix.id, info->dev);

	if(extfb_panel[fbinfo->node-1]->init)
		extfb_panel[fbinfo->node-1]->init(extfb_panel[fbinfo->node-1], &info->pdata.Mdp_data);

	clk_prepare_enable(info->pdata.Mdp_data.vioc_clock);
	clk_prepare_enable(info->pdata.Mdp_data.ddc_clock);

	#ifndef CONFIG_DRM_TCC
	extfb_interrupt_reg(true, info);
	#endif

	#ifdef CONFIG_PLATFORM_AVN
	tcafb_activate_var(info, &fbinfo->var);
	extfb_set_par(fbinfo);
	#endif
	return 0;

#ifdef CONFIG_PLATFORM_AVN
free_palette:
#endif
	devm_kfree(&pdev->dev, fbinfo->pseudo_palette);
free_framebuffer:
	framebuffer_release(fbinfo);

free_video_memory:
	extfb_unmap_video_memory(info);
	pr_err("EXT fb init failed.\n");
	return ret;
}

/*
 *  Cleanup
 */
static int extfb_remove(struct platform_device *pdev)
{
	struct tccfb_info	 *info = platform_get_drvdata(pdev);
	struct fb_info	   *fbinfo = info->fb;

	pr_info(" %s  \n", __func__);

	extfb_interrupt_reg(false, info);

	extfb_unmap_video_memory(info);

	clk_put(info->pdata.Mdp_data.vioc_clock);
	clk_put(info->pdata.Mdp_data.ddc_clock);

	unregister_framebuffer(fbinfo);

	return 0;
}

int extfb_register_panel(struct lcd_panel *panel)
{
	static unsigned int idx = 0;

	if(panel == NULL)
		return 0;

	dprintk(" %s  name:%s \n", __func__, panel->name);

	if(idx >= PANEL_MAX_NUM) {
		pr_err("error in %s: can not register panel_info (%d)\n", __func__, idx);
		return 0;
	}

	extfb_panel[idx++] = panel;
	return 1;
}
EXPORT_SYMBOL(extfb_register_panel);

struct lcd_panel *extfb_get_panel(int num)
{
	if((num >= PANEL_MAX_NUM) || (num < 0)) {
		pr_err("error in %s: invalid argument(%d)\n", __func__, num);
		return NULL;
	}

	if(extfb_panel[num] == NULL) {
		pr_err("error in %s: can not get panel_info \n", __func__, num);
		return NULL;
	}
		
	return extfb_panel[num];
}
EXPORT_SYMBOL(extfb_get_panel);

static struct platform_driver extfb_driver = {
	.probe		= extfb_probe,
	.remove		= extfb_remove,
	.driver		= {
		.name	= "extfb",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(extfb_of_match),
	},
};

//int extfb_init(void)
static int __init extfb_init(void)
{
	printk(" %s \n", __func__);
	return platform_driver_register(&extfb_driver);
}

static void __exit extfb_exit(void)
{
	dprintk(" %s \n", __func__);
//	tca_fb_exit();

	platform_driver_unregister(&extfb_driver);
}


module_init(extfb_init);
module_exit(extfb_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips Extended Framebuffer driver");
MODULE_LICENSE("GPL");
