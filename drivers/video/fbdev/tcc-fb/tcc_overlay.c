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
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/videodev2.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/io.h>
#include <asm/div64.h>
#include <linux/poll.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

#include <soc/tcc/pmap.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tcc_overlay_ioctl.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tca_display_config.h>

#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tcc_mem_ioctl.h>

#if defined(CONFIG_VIOC_AFBCDEC)
#include <video/tcc/vioc_afbcdec.h>
#endif

#if 0
static int debug	   = 1;
#else
static int debug;
#endif

#define dprintk(msg...)					\
	do {						\
		if (debug) {				\
			pr_info("[DBG][OVERLAY] " msg);	\
		}					\
	} while (0)

#define DEFAULT_OVERLAY_N 3
#define OVERLAY_LAYER_MAX (4)

struct overlay_drv_vioc {
	volatile void __iomem *reg;
	unsigned int id;
};

struct overlay_drv_type {
	struct miscdevice *misc;
	struct clk *clk;

	unsigned int layer_n;
	unsigned int layer_nlast;
	struct overlay_drv_vioc rdma[4];
	struct overlay_drv_vioc wmix;
#if defined(CONFIG_VIOC_AFBCDEC)
	struct overlay_drv_vioc afbc_dec;
#endif

	// extend infomation
	unsigned int id;
	unsigned int fb_dd_num;
	unsigned int open_cnt;

	// to back up image  infomation.
	overlay_video_buffer_t overBuffCfg;
};
#ifdef CONFIG_DISPLAY_EXT_FRAME
extern int tcc_ctrl_ext_frame(char enable);
#endif

#if defined(CONFIG_VIOC_AFBCDEC)
static void tcc_overlay_configure_AFBCDEC(
	volatile void __iomem *pAFBC_Dec, unsigned int afbc_dec_id,
	volatile void __iomem *pRDMA, unsigned int rdmaPath,
	unsigned int bSet_Comp, unsigned int onthefly, unsigned int bFirst,
	unsigned int base_addr, unsigned int fmt, unsigned int bSplitMode,
	unsigned int bWideMode, unsigned int width, unsigned int height)
{
	if (!pAFBC_Dec)
		return;

	if (bSet_Comp) {
		if (bFirst) {
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_CONFIG_FBCDECPath(afbc_dec_id, rdmaPath, 1);
			VIOC_AFBCDec_SurfaceCfg(
				pAFBC_Dec, base_addr, fmt, width, height, 0,
				bSplitMode, bWideMode, VIOC_AFBCDEC_SURFACE_0,
				1);
			VIOC_AFBCDec_SetContiDecEnable(pAFBC_Dec, onthefly);
			VIOC_AFBCDec_SetSurfaceN(
				pAFBC_Dec, VIOC_AFBCDEC_SURFACE_0, 1);
			VIOC_AFBCDec_SetIrqMask(
				pAFBC_Dec, 0, AFBCDEC_IRQ_ALL); // disable all
		} else {
			VIOC_AFBCDec_SurfaceCfg(
				pAFBC_Dec, base_addr, fmt, width, height, 0,
				bSplitMode, bWideMode, VIOC_AFBCDEC_SURFACE_0,
				0);
		}

		if (onthefly) {
			if (bFirst)
				VIOC_AFBCDec_TurnOn(
					pAFBC_Dec, VIOC_AFBCDEC_SWAP_DIRECT);
			else
				VIOC_AFBCDec_TurnOn(
					pAFBC_Dec, VIOC_AFBCDEC_SWAP_PENDING);
		} else {
			VIOC_AFBCDec_TurnOn(
				pAFBC_Dec, VIOC_AFBCDEC_SWAP_DIRECT);
		}
	} else {
		VIOC_RDMA_SetImageDisable(pRDMA);
		VIOC_AFBCDec_TurnOFF(pAFBC_Dec);
		VIOC_CONFIG_FBCDECPath(afbc_dec_id, rdmaPath, 0);

		//VIOC_CONFIG_SWReset(VIOC_AFBCDEC + dec_num, VIOC_CONFIG_RESET);
		//VIOC_CONFIG_SWReset(VIOC_AFBCDEC + dec_num, VIOC_CONFIG_CLEAR);
	}
}
#endif

static int tcc_overlay_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		pr_err("[ERR][OVERLAY] this address is not allowed\n");
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(
		    vma, vma->vm_start, vma->vm_pgoff,
		    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = NULL;
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;

	return 0;
}

DECLARE_WAIT_QUEUE_HEAD(overlay_wait);

static unsigned int
tcc_overlay_poll(struct file *file, struct poll_table_struct *wait)
{
	poll_wait(file, &(overlay_wait), wait);

	return POLLIN;
}

#if defined(CONFIG_TCC_SCREEN_SHARE)
static int tcc_overlay_display_shared_screen(
	overlay_shared_buffer_t buffer_cfg,
	struct overlay_drv_type *overlay_drv)
{
	unsigned int layer = 0;

	dprintk("%s addr:0x%x  fmt : 0x%x position:%d %d  size: %d %d\n",
		__func__, buffer_cfg.src_addr, buffer_cfg.fmt, buffer_cfg.dst_x,
		buffer_cfg.dst_y, buffer_cfg.dst_w, buffer_cfg.dst_h);
	if ((buffer_cfg.layer < 0) || (buffer_cfg.layer >= OVERLAY_LAYER_MAX)) {
		pr_err("[ERR][OVERLAY] %s: invalid layer:%d\n", __func__,
		       buffer_cfg.layer);
		return -1;
	}
	dprintk("layer number :%d  last layer:%d  chage : %d\n",
		overlay_drv->layer_n, overlay_drv->layer_nlast,
		buffer_cfg.layer);
	layer = overlay_drv->layer_n = buffer_cfg.layer;
	if (overlay_drv->rdma[layer].reg && overlay_drv->wmix.reg) {
		dprintk("%s SET[%d] : %p %p\n", __func__, layer,
			overlay_drv->rdma[layer].reg, overlay_drv->wmix.reg);

		//VIOC_RDMA_SetImageSize(overlay_drv->rdma[layer].reg,
		//buffer_cfg.src_w, buffer_cfg.src_h); //need to scaler
		VIOC_RDMA_SetImageSize(
			overlay_drv->rdma[layer].reg, buffer_cfg.dst_w,
			buffer_cfg.dst_h);
		VIOC_RDMA_SetImageFormat(
			overlay_drv->rdma[layer].reg, buffer_cfg.fmt);
		VIOC_RDMA_SetImageOffset(
			overlay_drv->rdma[layer].reg, buffer_cfg.fmt,
			buffer_cfg.frm_w);

		VIOC_RDMA_SetImageBase(
			overlay_drv->rdma[layer].reg, buffer_cfg.src_addr,
			buffer_cfg.src_addr, buffer_cfg.src_addr);

		VIOC_WMIX_SetPosition(
			overlay_drv->wmix.reg, layer, buffer_cfg.dst_x,
			buffer_cfg.dst_y);
		VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);

		VIOC_RDMA_SetImageEnable(overlay_drv->rdma[layer].reg);

		if (layer != overlay_drv->layer_nlast) {
			VIOC_RDMA_SetImageDisable(
				overlay_drv->rdma[overlay_drv->layer_nlast]
					.reg);
		}

		overlay_drv->layer_nlast = overlay_drv->layer_n;

#ifdef CONFIG_DISPLAY_EXT_FRAME
		tcc_ctrl_ext_frame(0);
#endif // CONFIG_DISPLAY_EXT_FRAME
	}
	return 0;
}
#endif

static int tcc_overlay_display_video_buffer(
	overlay_video_buffer_t buffer_cfg,
	struct overlay_drv_type *overlay_drv)
{
	unsigned int layer = 0, base0, base1, base2;
#if defined(CONFIG_VIOC_AFBCDEC)
	unsigned int afbc_changed = 0;
#endif

	dprintk("%s addr:0x%x  fmt : 0x%x position:%d %d  size: %d %d\n",
		__func__, buffer_cfg.addr, buffer_cfg.cfg.format,
		buffer_cfg.cfg.sx, buffer_cfg.cfg.sy, buffer_cfg.cfg.width,
		buffer_cfg.cfg.height);
	layer = overlay_drv->layer_n;

	if (overlay_drv->rdma[layer].reg && overlay_drv->wmix.reg) {
		dprintk("%s SET[%d] : %p %p\n", __func__, layer,
			overlay_drv->rdma[layer].reg, overlay_drv->wmix.reg);

#if defined(CONFIG_VIOC_AFBCDEC)
#if 0 // test
		if (buffer_cfg.afbc_dec_need != 0) {
			buffer_cfg.addr = 0x7d000000;
			buffer_cfg.cfg.format = TCC_LCDC_IMG_FMT_RGB888;
			buffer_cfg.cfg.sx = 256;
			buffer_cfg.cfg.sy = 256;
			buffer_cfg.cfg.width = 256;
			buffer_cfg.cfg.height = 256;
		}
		dprintk(
		"AFBC:: old_afbc(0x%p, %d),need(%d with %d), Layer(%d -> %d)\n",
		overlay_drv->afbc_dec.reg,
		get_vioc_index(overlay_drv->afbc_dec.id),
		buffer_cfg.afbc_dec_need, buffer_cfg.afbc_dec_num,
		overlay_drv->layer_nlast, layer);
#endif

		if ((overlay_drv->afbc_dec.reg != NULL)
		    && (buffer_cfg.afbc_dec_need == 0
			|| (get_vioc_index(overlay_drv->afbc_dec.id)
			    != buffer_cfg.afbc_dec_num)
			|| (layer != overlay_drv->layer_nlast))) {
			volatile void __iomem *reg =
				overlay_drv->rdma[layer].reg;
			unsigned int id = overlay_drv->rdma[layer].id;

			if (layer != overlay_drv->layer_nlast) {
				reg = overlay_drv
					      ->rdma[overlay_drv->layer_nlast]
					      .reg;
				id = overlay_drv->rdma[overlay_drv->layer_nlast]
					     .id;
			}

			tcc_overlay_configure_AFBCDEC(
				overlay_drv->afbc_dec.reg,
				overlay_drv->afbc_dec.id, reg, id,
				0 /*for plug-out*/, 0, 0, buffer_cfg.addr,
				buffer_cfg.cfg.format, 1, 0,
				buffer_cfg.cfg.width, buffer_cfg.cfg.height);
			overlay_drv->afbc_dec.reg = NULL;
			afbc_changed = 1;
		}
#endif
		VIOC_RDMA_SetImageSize(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.width,
			buffer_cfg.cfg.height);
		VIOC_RDMA_SetImageFormat(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.format);
		VIOC_RDMA_SetImageOffset(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.format,
			buffer_cfg.cfg.width);

		base0 = buffer_cfg.addr;
		base1 = buffer_cfg.addr1;
		base2 = buffer_cfg.addr2;

		if (base1 == 0)
			base1 = GET_ADDR_YUV42X_spU(
				buffer_cfg.addr, buffer_cfg.cfg.width,
				buffer_cfg.cfg.height);
		if (base2 == 0) {
			if (buffer_cfg.cfg.format == TCC_LCDC_IMG_FMT_444SEP)
				base2 = (unsigned int)GET_ADDR_YUV42X_spU(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
			else if (
				buffer_cfg.cfg.format
				== TCC_LCDC_IMG_FMT_YUV422SP)
				base2 = (unsigned int)GET_ADDR_YUV422_spV(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
			else
				base2 = (unsigned int)GET_ADDR_YUV420_spV(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
		}
		VIOC_RDMA_SetImageBase(
			overlay_drv->rdma[layer].reg, base0, base1, base2);
		VIOC_RDMA_SetImageRGBSwapMode(overlay_drv->rdma[layer].reg, 0);

		if (buffer_cfg.cfg.format >= VIOC_IMG_FMT_COMP) {
			VIOC_RDMA_SetImageY2RMode(
				overlay_drv->rdma[layer].reg, 2);
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg, 1);
		} else {
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg, 0);
		}

		VIOC_WMIX_SetPosition(
			overlay_drv->wmix.reg, layer, buffer_cfg.cfg.sx,
			buffer_cfg.cfg.sy);
		VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);

#if defined(CONFIG_VIOC_AFBCDEC)
		if (buffer_cfg.afbc_dec_need != 0) {
			if (overlay_drv->afbc_dec.reg == NULL)
				afbc_changed = 1;

			if (afbc_changed) {
				overlay_drv->afbc_dec.id =
					VIOC_FBCDEC0 + buffer_cfg.afbc_dec_num;
				overlay_drv->afbc_dec.reg =
					VIOC_AFBCDec_GetAddress(
						overlay_drv->afbc_dec.id);
			}

			tcc_overlay_configure_AFBCDEC(
				overlay_drv->afbc_dec.reg,
				overlay_drv->afbc_dec.id,
				overlay_drv->rdma[layer].reg,
				overlay_drv->rdma[layer].id,
				buffer_cfg.afbc_dec_need, 1, afbc_changed,
				buffer_cfg.addr, buffer_cfg.cfg.format, 1, 0,
				buffer_cfg.cfg.width, buffer_cfg.cfg.height);
		}

		if (overlay_drv->afbc_dec.reg != NULL)
			VIOC_RDMA_SetIssue(overlay_drv->rdma[layer].reg, 7, 16);
		else
#endif
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) \
	|| defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
			VIOC_RDMA_SetIssue(
				overlay_drv->rdma[layer].reg, 15, 16);
#endif

		VIOC_RDMA_SetImageEnable(overlay_drv->rdma[layer].reg);

		if (layer != overlay_drv->layer_nlast) {
			VIOC_RDMA_SetImageDisable(
				overlay_drv->rdma[overlay_drv->layer_nlast]
					.reg);
		}

		overlay_drv->layer_nlast = overlay_drv->layer_n;

#ifdef CONFIG_DISPLAY_EXT_FRAME
		tcc_ctrl_ext_frame(0);
#endif // CONFIG_DISPLAY_EXT_FRAME
	}
	return 0;
}

static long
tcc_overlay_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice *)file->private_data;
	struct overlay_drv_type *overlay_drv = dev_get_drvdata(misc->parent);

	switch (cmd) {
#if defined(CONFIG_TCC_SCREEN_SHARE)
	case OVERLAY_PUSH_SHARED_BUFFER: {
		overlay_shared_buffer_t overBuffCfg;

		memcpy(&overBuffCfg, (overlay_shared_buffer_t *)arg,
		       sizeof(overlay_shared_buffer_t));
		return tcc_overlay_display_shared_screen(
			overBuffCfg, overlay_drv);
	}
#endif

	case OVERLAY_PUSH_VIDEO_BUFFER: {
		overlay_video_buffer_t overBuffCfg;

		if (copy_from_user(
			    &overBuffCfg, (overlay_video_buffer_t *)arg,
			    sizeof(overlay_video_buffer_t)))
			return -EFAULT;

		overlay_drv->overBuffCfg = overBuffCfg;

		return tcc_overlay_display_video_buffer(
			overBuffCfg, overlay_drv);
	}

	case OVERLAY_SET_CONFIGURE: {
		overlay_config_t overCfg;

		if (copy_from_user(
			    &overCfg, (overlay_config_t *)arg,
			    sizeof(overlay_config_t)))
			return -EFAULT;

		return 0;
	} break;

	case OVERLAY_SET_LAYER: {
		unsigned int overlay_layer;

		if (copy_from_user(
			    &overlay_layer, (unsigned int *)arg,
			    sizeof(unsigned int)))
			return -EFAULT;

		VIOC_WMIX_SetOverlayPriority(overlay_drv->wmix.reg, 0xC);
		if (overlay_layer < 4) {
			pr_info("[INF][OVERLAY] layer number :%d  last layer:%d  chage : %d\n",
				overlay_drv->layer_n, overlay_drv->layer_nlast,
				overlay_layer);
			overlay_drv->layer_n = overlay_layer;
		} else {
			pr_err("[ERR][OVERLAY] wrong layer number :%d  cur layer:%d\n",
			       overlay_layer, overlay_drv->layer_nlast);
		}
		return 0;
	} break;

	case OVERLAY_DISALBE_LAYER: {
		VIOC_RDMA_SetImageDisable(
			overlay_drv->rdma[overlay_drv->layer_n].reg);
	} break;

	case OVERLAY_GET_LAYER:
		if (copy_to_user(
			    (unsigned int *)arg, &overlay_drv->layer_n,
			    sizeof(unsigned int)))
			return -EFAULT;
		break;

	case OVERLAY_SET_OVP:
	case OVERLAY_SET_OVP_KERNEL: {
		unsigned int ovp;

		if (cmd == OVERLAY_SET_OVP_KERNEL) {
			ovp = (unsigned int)arg;
		} else {
			if (copy_from_user(
				    &ovp, (unsigned int *)arg,
				    sizeof(unsigned int))) {
				pr_err("[ERR][OVERLAY] OVERLAY_SET_OVP copy_from_user failed\n");
				return -EFAULT;
			}
		}

		if (ovp > 29) {
			pr_err("[ERR][OVERLAY] wrong ovp number: %d\n", ovp);
			return -EINVAL;
		}

		VIOC_WMIX_SetOverlayPriority(overlay_drv->wmix.reg, ovp);
		VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);
	} break;

	case OVERLAY_GET_OVP:
	case OVERLAY_GET_OVP_KERNEL: {
		unsigned int ovp;

		VIOC_WMIX_GetOverlayPriority(overlay_drv->wmix.reg, &ovp);
		if (cmd == OVERLAY_GET_OVP_KERNEL) {
			*(unsigned long *)arg = ovp;
		} else {
			if (copy_to_user(
				    (unsigned int *)arg, &ovp,
				    sizeof(unsigned int))) {
				pr_err("[ERR][OVERLAY] OVERLAY_SET_OVP copy_to_user failed\n");
				return -EFAULT;
			}
		}
	} break;

	default:
		pr_err(" Unsupported IOCTL(%d)!!!\n", cmd);
		break;
	}

	return 0;
}

static int tcc_overlay_release(struct inode *inode, struct file *file)
{
	unsigned int layer = 0;
	struct miscdevice *misc = (struct miscdevice *)file->private_data;
	struct overlay_drv_type *overlay_drv = dev_get_drvdata(misc->parent);

	overlay_drv->open_cnt--;

	dprintk("%s num:%d\n", __func__, overlay_drv->open_cnt);

	if (overlay_drv->open_cnt == 0) {
		//VIOC_PlugInOutCheck VIOC_PlugIn;

		layer = overlay_drv->layer_nlast;

		if (overlay_drv->rdma[layer].reg)
			VIOC_RDMA_SetImageDisable(overlay_drv->rdma[layer].reg);

#if defined(CONFIG_VIOC_AFBCDEC)
		if (overlay_drv->afbc_dec.reg != NULL) {
			tcc_overlay_configure_AFBCDEC(
				overlay_drv->afbc_dec.reg,
				overlay_drv->afbc_dec.id,
				overlay_drv->rdma[layer].reg,
				overlay_drv->rdma[layer].id, 0, 0, 0, 0, 0, 0,
				0, 0, 0);
			overlay_drv->afbc_dec.reg = NULL;
		}
#endif

		if (overlay_drv->wmix.reg) {
			VIOC_WMIX_SetPosition(
				overlay_drv->wmix.reg, layer, 0, 0);
			VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);
		}
#if 0 /* TODO */
		VIOC_CONFIG_Device_PlugState(VIOC_FCDEC0, &VIOC_PlugIn);

		if (VIOC_PlugIn.enable && (overlay_drv->rdma[layer].id ==
			VIOC_PlugIn.connect_device)) {
			pr_info(
			"tcc overlay drv fcdec plug out  from rdma : %d\n",
			overlay_drv->rdma[layer].id);

			VIOC_CONFIG_PlugOut(VIOC_FCDEC0);

			VIOC_CONFIG_SWReset(overlay_drv->pIREQConfig,
			VIOC_CONFIG_FCDEC, 0, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(overlay_drv->pIREQConfig,
			VIOC_CONFIG_FCDEC, 0, VIOC_CONFIG_CLEAR);
		}
#endif
		clk_disable_unprepare(overlay_drv->clk);

#ifdef CONFIG_DISPLAY_EXT_FRAME
		tcc_ctrl_ext_frame(0);
#endif //
	}

	return 0;
}

static int tcc_overlay_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = (struct miscdevice *)file->private_data;
	struct overlay_drv_type *overlay_drv = dev_get_drvdata(misc->parent);

	overlay_drv->open_cnt++;
	clk_prepare_enable(overlay_drv->clk);

	dprintk("%s num:%d\n", __func__, overlay_drv->open_cnt);

	return 0;
}

static const struct file_operations tcc_overlay_fops = {
	.owner = THIS_MODULE,
	.poll = tcc_overlay_poll,
	.unlocked_ioctl = tcc_overlay_ioctl,
	.mmap = tcc_overlay_mmap,
	.open = tcc_overlay_open,
	.release = tcc_overlay_release,
};

static int tcc_overlay_probe(struct platform_device *pdev)
{
	struct overlay_drv_type *overlay_drv;
	struct device_node *vioc_node, *tmp_node;
	unsigned int index;
	int i = 0, ret = -ENODEV;

	overlay_drv = kzalloc(sizeof(struct overlay_drv_type), GFP_KERNEL);

	if (!overlay_drv)
		return -ENOMEM;

	overlay_drv->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);

	if (!overlay_drv->misc)
		goto err_overlay_drv_misc;

	overlay_drv->fb_dd_num = -1;
	vioc_node = of_parse_phandle(pdev->dev.of_node, "fbdisplay-overlay", 0);
	if (vioc_node) {
		of_property_read_u32(
			vioc_node, "telechips,fbdisplay_num",
			&overlay_drv->fb_dd_num);
	} else {
		of_property_read_u32_index(
			pdev->dev.of_node, "display_num", 0,
			&overlay_drv->fb_dd_num);
		pr_info("[INF][OVERLAY] display_num = %d\n",
			overlay_drv->fb_dd_num);
	}

	if (overlay_drv->fb_dd_num < 0 || overlay_drv->fb_dd_num > 1)
		goto err_overlay_drv_init;

	overlay_drv->clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR((void *)overlay_drv->clk))
		overlay_drv->clk = NULL;

	/* register scaler discdevice */
	overlay_drv->id = of_alias_get_id(pdev->dev.of_node, "tcc-overlay-drv");
	overlay_drv->misc->minor = MISC_DYNAMIC_MINOR;
	overlay_drv->misc->fops = &tcc_overlay_fops;
	if ((overlay_drv->id == 0) || (overlay_drv->id == -ENODEV)) {
		overlay_drv->misc->name = kasprintf(GFP_KERNEL, "overlay");
	} else {
		overlay_drv->misc->name =
			kasprintf(GFP_KERNEL, "overlay%d", overlay_drv->id);
	}
	overlay_drv->misc->parent = &pdev->dev;
	ret = misc_register(overlay_drv->misc);
	if (ret)
		goto err_overlay_drv_init;

	tmp_node = of_parse_phandle(pdev->dev.of_node, "rdmas", 0);
	for (i = 0; i < OVERLAY_LAYER_MAX; i++) {
		of_property_read_u32_index(
			pdev->dev.of_node, "rdmas",
			(overlay_drv->fb_dd_num == 0) ?
				i + 1 :
				(OVERLAY_LAYER_MAX + i + 1),
			&index);
		if (tmp_node) {
			overlay_drv->rdma[i].reg = VIOC_RDMA_GetAddress(index);
			overlay_drv->rdma[i].id = index;
#if defined(CONFIG_TCC_SCREEN_SHARE)
			if ((overlay_drv->fb_dd_num == 1) && (i == 1))
				VIOC_RDMA_SetImageDisable(
					overlay_drv->rdma[i].reg);
#endif
		}
		dprintk("%s-%d :: fd_num(%d) => rdma[%d]: %d %d/%p\n", __func__,
			__LINE__, overlay_drv->fb_dd_num, i, index,
			overlay_drv->rdma[i].id, overlay_drv->rdma[i].reg);

		if (IS_ERR((void *)overlay_drv->rdma[i].reg)) {
			pr_info("[INF][OVERLAY] rdma node of layer%d is n/a\n",
				i);
			overlay_drv->rdma[i].reg = NULL;
		}
	}

	overlay_drv->layer_nlast = overlay_drv->layer_n = DEFAULT_OVERLAY_N;
	ret = of_property_read_u32(
		pdev->dev.of_node, "rdma_init_layer", &overlay_drv->layer_n);
	if (ret || overlay_drv->layer_n > 3)
		overlay_drv->layer_nlast = overlay_drv->layer_n =
			DEFAULT_OVERLAY_N;
	pr_info("[INF][OVERLAY] overlay driver init layer :%d\n",
		overlay_drv->layer_n);

#if defined(CONFIG_VIOC_AFBCDEC)
	overlay_drv->afbc_dec.reg = NULL;
	overlay_drv->afbc_dec.id = 0;
#endif

	tmp_node = of_parse_phandle(pdev->dev.of_node, "wmixs", 0);
	of_property_read_u32_index(
		pdev->dev.of_node, "wmixs",
		(overlay_drv->fb_dd_num == 0) ? 1 : 2, &index);
	if (tmp_node) {
		overlay_drv->wmix.reg = VIOC_WMIX_GetAddress(index);
		overlay_drv->wmix.id = index;
	}
	dprintk("%s-%d :: fd_num(%d) => wmix[%d]: %d %d/%p\n", __func__,
		__LINE__, overlay_drv->fb_dd_num, i, index,
		overlay_drv->wmix.id, overlay_drv->wmix.reg);

	if (IS_ERR((void *)overlay_drv->wmix.reg)) {
		pr_err("[ERR][OVERLAY] could not find wmix node of %s driver.\n",
		       overlay_drv->misc->name);
		overlay_drv->wmix.reg = NULL;
	}
	overlay_drv->open_cnt = 0;

	platform_set_drvdata(pdev, overlay_drv);

	return 0;

err_overlay_drv_init:
	kfree(overlay_drv->misc);
	pr_err("[ERR][OVERLAY] err_overlay_drv_init.\n");

err_overlay_drv_misc:
	kfree(overlay_drv);
	pr_err("[ERR][OVERLAY] err_overlay_drv_misc.\n");

	return ret;
}

static int tcc_overlay_remove(struct platform_device *pdev)
{
	struct overlay_drv_type *overlay_drv =
		(struct overlay_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(overlay_drv->misc);

	kfree(overlay_drv->misc);
	kfree(overlay_drv);
	return 0;
}

#ifdef CONFIG_PM
static int tcc_overlay_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct overlay_drv_type *overlay_drv =
		(struct overlay_drv_type *)platform_get_drvdata(pdev);

	if (overlay_drv->open_cnt != 0) {
		pr_info("[INF][OVERLAY] %s %d opened\n", __func__,
			overlay_drv->open_cnt);
		clk_disable_unprepare(overlay_drv->clk);
	}
	return 0;
}

static int tcc_overlay_resume(struct platform_device *pdev)
{
	struct overlay_drv_type *overlay_drv =
		(struct overlay_drv_type *)platform_get_drvdata(pdev);

	if (overlay_drv->open_cnt != 0) {
		pr_info("[INF][OVERLAY] %s %d opened\n", __func__,
			overlay_drv->open_cnt);
		clk_prepare_enable(overlay_drv->clk);
	}
	return 0;
}
#else // CONFIG_PM
#define tcc_overlay_suspend NULL
#define tcc_overlay_resume NULL
#endif // CONFIG_PM

#ifdef CONFIG_OF
static const struct of_device_id tcc_overlay_of_match[] = {
	{.compatible = "telechips,tcc_overlay"},
	{}
};
MODULE_DEVICE_TABLE(of, tcc_overlay_of_match);
#endif

static struct platform_driver tcc_overlay_driver = {
	.probe = tcc_overlay_probe,
	.remove = tcc_overlay_remove,
	.suspend = tcc_overlay_suspend,
	.resume = tcc_overlay_resume,
	.driver = {
			.name = "tcc_overlay",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(tcc_overlay_of_match),
#endif
		},
};

static void __exit tcc_overlay_cleanup(void)
{
	platform_driver_unregister(&tcc_overlay_driver);
}

static int __init tcc_overlay_init(void)
{
	pr_info("%s\n", __func__);
	platform_driver_register(&tcc_overlay_driver);
	return 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC Video for Linux overlay driver");
MODULE_LICENSE("GPL");

module_init(tcc_overlay_init);
module_exit(tcc_overlay_cleanup);
