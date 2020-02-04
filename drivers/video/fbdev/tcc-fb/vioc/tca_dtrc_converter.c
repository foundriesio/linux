/****************************************************************************
 * linux/drivers/video/tca_dtrc_converter.c
 *
 * Author:  <linux@telechips.com>
 * Created: March 18, 2012
 * Description: TCC lcd Driver
 *
 * Copyright (C) 20010-2011 Telechips
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
 *****************************************************************************/

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
#include <linux/clk-provider.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/div64.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/tcc_ccfb_ioctl.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_dtrc.h>
#include <video/tcc/vioc_config.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/tccfb.h>
#endif

#define ROUND_UP_16(num) (((num) + 15) & ~15)

extern unsigned int tca_get_main_decompressor_num(void);

void tca_dtrc_convter_wait_done(unsigned int component_num)
{
#define MAX_WAIT_TIEM 0x10000000

	volatile unsigned int loop = 0, upd_loop = 0;
	// volatile unsigned int ctrl_reg;
	volatile void __iomem *HwVIOC_DTRC_RDMA =
		VIOC_DTRC_GetAddress(component_num);

	if (HwVIOC_DTRC_RDMA == NULL) {
		pr_err("[ERR][DTRC] %s: HwVIOC_DTRC_RDMA is null\n", __func__);
		return;
	}

	for (loop = 0; loop <= MAX_WAIT_TIEM; loop++) {
		udelay(10);
		if (__raw_readl(HwVIOC_DTRC_RDMA + DTRC_IRQ) &
		    DTRC_IRQ_REW_MASK)
			break;
	}


	if (loop >= MAX_WAIT_TIEM) {
		pr_info("[INF][DTRC] %s wait max count stauts info :  uCTRL:0x%08x loop: %08x  STATUS %08x \n",
		       __func__, __raw_readl(HwVIOC_DTRC_RDMA + DTRC_CTRL),
		       loop, __raw_readl(HwVIOC_DTRC_RDMA + DTRC_IRQ));
	}

	for (upd_loop = 0; upd_loop <= MAX_WAIT_TIEM; upd_loop++) {
		if (!(__raw_readl(HwVIOC_DTRC_RDMA + DTRC_CTRL) &
		      DTRC_CTRL_UPD_MASK))
			break;
	}

	if (loop >= MAX_WAIT_TIEM) {
		pr_info("[INF][DTRC] %s UPD wait max count stauts info :  uCTRL:0x%08x loop: %08x  STATUS %08x \n",
		       __func__, __raw_readl(HwVIOC_DTRC_RDMA + DTRC_CTRL),
		       loop, __raw_readl(HwVIOC_DTRC_RDMA + DTRC_IRQ));
	}
}

void tca_dtrc_convter_onoff(unsigned int component_num, unsigned int onoff,
			    unsigned int wait_done)
{
	volatile void __iomem *HwVIOC_DTRC_RDMA =
		VIOC_DTRC_GetAddress(component_num);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (!onoff)
		VIOC_CONFIG_DV_EX_VIOC_PROC(component_num);
#endif

	if (!onoff)
		VIOC_DTRC_RDMA_SetImageSize(HwVIOC_DTRC_RDMA, 0, 0);

	VIOC_DTRC_RDMA_SetImageUpdate(HwVIOC_DTRC_RDMA, onoff);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (get_vioc_index(component_num) == 0) {
		volatile void __iomem *pDisp_DV =
			VIOC_DV_GetAddress((DV_DISP_TYPE)EDR_BL);

		if (onoff)
			VIOC_V_DV_Turnon(pDisp_DV, NULL);
		else
			VIOC_V_DV_Turnoff(pDisp_DV, NULL);
	}
#endif

	if (!onoff && wait_done) {
		tca_dtrc_convter_wait_done(
			component_num); // no need to wait because
				   // VIOC_CONFIG_DMAPath_UnSet() will check it.
		// pr_info("[INF][DTRC] %s-%d :: OFF done \n", __func__, __LINE__);
	}
}

void tca_dtrc_convter_swreset(unsigned int component_num)
{
	// pr_info("[INF][DTRC] %s-%d :: %d\n", __func__, __LINE__, component_num);
	VIOC_CONFIG_SWReset(component_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(component_num, VIOC_CONFIG_CLEAR);
}

void tca_dtrc_convter_set(unsigned int component_num,
			  struct tcc_lcdc_image_update *ImageInfo, int y2r)
{
	unsigned int YTable;
	unsigned int UVTable;
	unsigned int YCmp;
	unsigned int UVCmp;

	unsigned int width;
	unsigned int height;
	unsigned int offset0;
	unsigned int offset1;

	unsigned int arid_reg_ctrl;
	unsigned int clk_dis;
	unsigned int soft_reset;
	unsigned int detile_by_addr_en;
	unsigned int detile_by_addr_dis;
	unsigned int pf_cnt_en;

	unsigned int frm0_cbsr_width;
	unsigned int frm0_cbsr_height;
	unsigned int frm1_cbsr_width;
	unsigned int frm1_cbsr_height;

	unsigned int frm0_data_rdy;
	unsigned int frm0_main8;
	unsigned int frm0_vinv;
	unsigned int frm0_byp;
	unsigned int frm1_data_rdy;
	unsigned int frm1_main8;
	unsigned int frm1_vinv;
	unsigned int frm1_byp;
	unsigned int ARID_REG;
	unsigned int bpp = 8;

	volatile void __iomem *HwVIOC_DTRC_RDMA =
		VIOC_DTRC_GetAddress(component_num);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH() &&
	    get_vioc_index(component_num) == tca_get_main_decompressor_num())
		y2r = 0;
#endif

#if 0  // debug log
	{
		pr_info("[INF][DTRC] DTRC[%d] >> R[0x%x/0x%x/0x%x] M[%d] idx[%d], ID[%d] W:%d(%d) H:%d(%d) S(%d/%d) C:0x%8x/0x%8x T:0x%8x/0x%8x Str(%d) bpp(%d/%d) crop(%d/%d~%dx%d)\n", dtrc_num,
			__raw_readl(HwVIOC_DTRC_RDMA+DTRC_CTRL), __raw_readl(HwVIOC_DTRC_RDMA+DTRC_BASE0), __raw_readl(HwVIOC_DTRC_RDMA+DTRC_IRQ),
			ImageInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO], ImageInfo->private_data.optional_info[VID_OPT_DISP_OUT_IDX],
			ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID], ImageInfo->Frame_width, ImageInfo->private_data.dtrcConv_info.m_iWidth,
			ImageInfo->Frame_height, ImageInfo->private_data.dtrcConv_info.m_iHeight,
			ImageInfo->private_data.dtrcConv_info.m_iCompressionTableLumaSize, ImageInfo->private_data.dtrcConv_info.m_iCompressionTableChromaSize,
			ImageInfo->private_data.dtrcConv_info.m_CompressedY[0], ImageInfo->private_data.dtrcConv_info.m_CompressedCb[0],
			ImageInfo->private_data.dtrcConv_info.m_CompressionTableLuma[0], ImageInfo->private_data.dtrcConv_info.m_CompressionTableChroma[0],
			ImageInfo->private_data.dtrcConv_info.m_iPicStride,
			ImageInfo->private_data.dtrcConv_info.m_iBitDepthLuma,
			ImageInfo->private_data.dtrcConv_info.m_iBitDepthChroma,
			ImageInfo->crop_left, ImageInfo->crop_top, (ImageInfo->crop_right-ImageInfo->crop_left),(ImageInfo->crop_bottom-ImageInfo->crop_top));
	}
#endif //

	// pr_info("[INF][DTRC] D-Unique[%d] :: %d[0x%x] Curr[0x%x/0x%x]\n",
	// ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID],
	//		ImageInfo->private_data.optional_info[VID_OPT_DISP_OUT_IDX],
	//ImageInfo->private_data.dtrcConv_info.m_CompressedY[0],
	//__raw_readl(HwVIOC_DTRC_RDMA+DTRC_BASE0),
	//__raw_readl(HwVIOC_DTRC_RDMA+DTRC_CUR));
	width = ImageInfo->private_data.dtrcConv_info.m_iWidth;
	height = ImageInfo->private_data.dtrcConv_info.m_iHeight;
	offset0 = ROUND_UP_16(width);
	offset1 = ROUND_UP_16(offset0 / 2);

	if (ImageInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] ==
	    0x1) { // Compressed data
		frm0_byp = 0x0;
		bpp = ImageInfo->private_data.dtrcConv_info.m_iBitDepthLuma;
		YTable = ImageInfo->private_data.dtrcConv_info
				 .m_CompressionTableLuma[PA];
		UVTable = ImageInfo->private_data.dtrcConv_info
				  .m_CompressionTableChroma[PA];
		YCmp = ImageInfo->private_data.dtrcConv_info.m_CompressedY[PA];
		UVCmp = ImageInfo->private_data.dtrcConv_info
				.m_CompressedCb[PA];
	} else if (ImageInfo->private_data
			   .optional_info[VID_OPT_HAVE_DTRC_INFO] ==
		   0x2) { // Tiled data
		frm0_byp = 0x1;
		bpp = ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH];
		YTable = 0x00;
		UVTable = 0x00;
		YCmp = ImageInfo->private_data.offset[0];
		UVCmp = ImageInfo->private_data.offset[1];
	} else {
		pr_warn("[WAR][DTRC] do not support this kind of data format \n");
		return;
	}

	arid_reg_ctrl = 0;
	clk_dis = 0;
	soft_reset = 0;
	detile_by_addr_en = 0;
	detile_by_addr_dis = 0;
	pf_cnt_en = 1;

	frm0_cbsr_width = width / 8;
	frm0_cbsr_height = height / 8;
	frm1_cbsr_width = width / 8;
	frm1_cbsr_height = height / 8;

	frm0_data_rdy = 0x0;
	if (bpp == 8) {
		frm0_main8 = 0x1;
		frm1_main8 = 0x1;
	} else {
		frm0_main8 = 0x0;
		frm1_main8 = 0x0;
	}
	frm0_vinv = 0x0;
	frm1_data_rdy = 0x0;
	frm1_vinv = 0x0;
	frm1_byp = 0x0;

	ARID_REG = 0x0F0E0100;

	if (!YCmp)
		return;

	VIOC_DTRC_RDMA_SetImageConfig(HwVIOC_DTRC_RDMA);
	if (bpp == 8)
		VIOC_DTRC_RDMA_SetImageFormat(HwVIOC_DTRC_RDMA, 0);
	else
		VIOC_DTRC_RDMA_SetImageFormat(HwVIOC_DTRC_RDMA, 1);

	VIOC_DTRC_RDMA_SetImageSize(
		HwVIOC_DTRC_RDMA,
		(ImageInfo->crop_right - ImageInfo->crop_left),
		(ImageInfo->crop_bottom - ImageInfo->crop_top));
	VIOC_DTRC_RDMA_SetImageBase(HwVIOC_DTRC_RDMA, YCmp, UVCmp);

	if (bpp == 8)
		VIOC_DTRC_RDMA_SetImageOffset(HwVIOC_DTRC_RDMA, offset0,
					      offset1);
	else
		VIOC_DTRC_RDMA_SetImageOffset(
			HwVIOC_DTRC_RDMA, ROUND_UP_16((offset0 * 125) / 100),
			ROUND_UP_16((offset1 * 125) / 100));

	VIOC_DTRC_RDMA_Setuvintpl(HwVIOC_DTRC_RDMA, 1);
	VIOC_DTRC_RDMA_SetY2R(HwVIOC_DTRC_RDMA, y2r, 0x2);
	VIOC_DTRC_RDMA_SetIDSEL(HwVIOC_DTRC_RDMA, 0x101, 0, 0);

	VIOC_DTRC_RDMA_DTCTRL(HwVIOC_DTRC_RDMA, 0, 0x0, 0, 0, 0xff, 0, 0, 0, 0);
	VIOC_DTRC_RDM_CTRL(HwVIOC_DTRC_RDMA, arid_reg_ctrl, clk_dis, soft_reset,
			   detile_by_addr_en, pf_cnt_en);
	VIOC_DTRC_RDMA_F0_ADDR(HwVIOC_DTRC_RDMA, YCmp, YTable, UVCmp, UVTable);
	VIOC_DTRC_RDMA_F0_SIZE(HwVIOC_DTRC_RDMA, frm0_cbsr_width,
			       frm0_cbsr_height);
	VIOC_DTRC_RDMA_FRM0_CROP(
		HwVIOC_DTRC_RDMA, 1, ImageInfo->crop_left, ImageInfo->crop_top,
		(ImageInfo->crop_right - ImageInfo->crop_left),
		(ImageInfo->crop_bottom - ImageInfo->crop_top));

	//	VIOC_DTRC_RDMA_F0_BYP_DETILE_ADDR	(HwVIOC_DTRC_RDMA,
	//frm0_y_byp_detile_addr_s, frm0_y_byp_detile_addr_e,
	//frm0_uv_byp_detile_addr_s, frm0_uv_byp_detile_addr_e);
	VIOC_DTRC_RDMA_ARIDR(HwVIOC_DTRC_RDMA, ARID_REG);
	VIOC_DTRC_RDMA_FETCH_ARID(HwVIOC_DTRC_RDMA, 0, 1);
	//	VIOC_DTRC_RDMA_DCTL			        (HwVIOC_DTRC_RDMA,
	//frm0_data_rdy, frm0_main8, frm0_vinv, frm0_byp, frm1_data_rdy,
	//frm1_main8, frm1_vinv, frm1_byp); 	VIOC_DTRC_RDMA_F1_ADDR
	//(HwVIOC_DTRC_RDMA, YCmp, YTable, UVCmp, UVTable);
	//	VIOC_DTRC_RDMA_F1_SIZE		        (HwVIOC_DTRC_RDMA,
	//frm1_cbsr_width, frm1_cbsr_height); 	VIOC_DTRC_RDMA_FRM1_CROP
	//(HwVIOC_DTRC_RDMA, 1, 0, 0, width, height);
	//	VIOC_DTRC_RDMA_F1_BYP_DETILE_ADDR   (HwVIOC_DTRC_RDMA,
	//frm1_y_byp_detile_addr_s, frm1_y_byp_detile_addr_e,
	//frm1_uv_byp_detile_addr_s, frm1_uv_byp_detile_addr_e);
	VIOC_DTRC_RDMA_DCTL(HwVIOC_DTRC_RDMA, 0x1, frm0_main8, frm0_vinv,
			    frm0_byp, 0x1, frm1_main8, frm1_vinv, frm1_byp);

	VIOC_DTRC_RDMA_SetImageUpdate(HwVIOC_DTRC_RDMA, 1);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH() &&
	    get_vioc_index(component_num) == tca_get_main_decompressor_num()) {
		volatile void __iomem *pDisp_DV =
			VIOC_DV_GetAddress((DV_DISP_TYPE)EDR_BL);

		if (ImageInfo->Lcdc_layer == RDMA_VIDEO || ImageInfo->Lcdc_layer == RDMA_LASTFRM) {
			if (vioc_get_out_type() == ImageInfo->private_data.dolbyVision_info.reg_out_type) {
				vioc_v_dv_prog(	ImageInfo->private_data.dolbyVision_info.md_hdmi_addr,
								ImageInfo->private_data.dolbyVision_info.reg_addr,
								ImageInfo->private_data.optional_info[VID_OPT_CONTENT_TYPE],
								1);

				VIOC_V_DV_SetPXDW(pDisp_DV, NULL, VIOC_PXDW_FMT_24_RGB888);
				VIOC_V_DV_SetSize(pDisp_DV, NULL, ImageInfo->offset_x, ImageInfo->offset_y, Hactive, Vactive);
				VIOC_V_DV_Turnon(pDisp_DV, NULL);
			}
		} else {
			pr_err("[ERR][DTRC] @@@@@@@@@@ 2 @@@@@@@@@ Should be implement other layer configuration. \n");
			return;
		}
	}
#endif
}
EXPORT_SYMBOL(tca_dtrc_convter_set);

void tca_dtrc_convter_driver_set(unsigned int component_num, unsigned int Fwidth,
				 unsigned int Fheight, unsigned int pos_x,
				 unsigned int pos_y, unsigned int y2r,
				 vp9_compressed_info_t *dtrcConv_info)
{
	unsigned int YTable;
	unsigned int UVTable;
	unsigned int YCmp;
	unsigned int UVCmp;

	unsigned int width;
	unsigned int height;
	unsigned int offset0;
	unsigned int offset1;

	unsigned int arid_reg_ctrl;
	unsigned int clk_dis;
	unsigned int soft_reset;
	unsigned int detile_by_addr_en;
	unsigned int detile_by_addr_dis;
	unsigned int pf_cnt_en;

	unsigned int frm0_cbsr_width;
	unsigned int frm0_cbsr_height;
	//	unsigned int frm1_cbsr_width;
	//	unsigned int frm1_cbsr_height;

	unsigned int frm0_data_rdy;
	unsigned int frm0_main8;
	unsigned int frm0_vinv;
	unsigned int frm0_byp;
	unsigned int frm1_data_rdy;
	unsigned int frm1_main8;
	unsigned int frm1_vinv;
	unsigned int frm1_byp;
	unsigned int ARID_REG;
	unsigned int bpp = 8;

	volatile void __iomem *HwVIOC_DTRC_RDMA =
		VIOC_DTRC_GetAddress(component_num);

#if 0  // debug log
	printk("[DBG][DTRC] >>>>[%d] DTRC width: %d(%d) height: %d(%d) Compressed: 0x%8x / 0x%8x   Table: 0x%8x / 0x%8x \n",
		dtrc_num, Fwidth, dtrcConv_info->m_iWidth,
		Fheight, dtrcConv_info->m_iHeight,
		dtrcConv_info->m_CompressedY[0], dtrcConv_info->m_CompressedCb[0],
		dtrcConv_info->m_CompressionTableLuma[0], dtrcConv_info->m_CompressionTableChroma[0]);

	printk("[DBG][DTRC] Stride (%d) bitDepth (Luma:%d Croma:%d) \n",
		dtrcConv_info->m_iPicStride,
		dtrcConv_info->m_iBitDepthLuma,
		dtrcConv_info->m_iBitDepthChroma);

	printk("[DBG][DTRC] * dtrc converter info end  *  \n");
#endif //
	width = dtrcConv_info->m_iWidth;
	height = dtrcConv_info->m_iHeight;
	offset0 = ROUND_UP_16(width);
	offset1 = ROUND_UP_16(offset0 / 2);
	bpp = dtrcConv_info->m_iBitDepthLuma;
	YTable = dtrcConv_info->m_CompressionTableLuma[PA];
	UVTable = dtrcConv_info->m_CompressionTableChroma[PA];
	YCmp = dtrcConv_info->m_CompressedY[PA];
	UVCmp = dtrcConv_info->m_CompressedCb[PA];

	arid_reg_ctrl = 0;
	clk_dis = 0;
	soft_reset = 0;
	detile_by_addr_en = 0;
	detile_by_addr_dis = 0;
	pf_cnt_en = 1;

	frm0_cbsr_width = width / 8;
	frm0_cbsr_height = height / 8;
	//	frm1_cbsr_width 	= width/8;
	//	frm1_cbsr_height 	= height/8;

	frm0_data_rdy = 0x0;
	if (bpp == 8) {
		frm0_main8 = 0x1;
		frm1_main8 = 0x1;
	} else {
		frm0_main8 = 0x0;
		frm1_main8 = 0x0;
	}
	frm0_vinv = 0x0;
	frm0_byp = 0x0;
	frm1_data_rdy = 0x0;
	frm1_vinv = 0x0;
	frm1_byp = 0x0;

	ARID_REG = 0x0F0E0100;


	if (!YCmp || !YTable)
		return;

	VIOC_DTRC_RDMA_SetImageConfig(HwVIOC_DTRC_RDMA);
	if (bpp == 8)
		VIOC_DTRC_RDMA_SetImageFormat(HwVIOC_DTRC_RDMA, 0);
	else
		VIOC_DTRC_RDMA_SetImageFormat(HwVIOC_DTRC_RDMA, 1);

	VIOC_DTRC_RDMA_SetImageSize(HwVIOC_DTRC_RDMA, Fwidth, Fheight);
	VIOC_DTRC_RDMA_SetImageBase(HwVIOC_DTRC_RDMA, YCmp, UVCmp);

	if (bpp == 8)
		VIOC_DTRC_RDMA_SetImageOffset(HwVIOC_DTRC_RDMA, offset0,
					      offset1);
	else
		VIOC_DTRC_RDMA_SetImageOffset(
			HwVIOC_DTRC_RDMA, ROUND_UP_16((offset0 * 125) / 100),
			ROUND_UP_16((offset1 * 125) / 100));

	VIOC_DTRC_RDMA_Setuvintpl(HwVIOC_DTRC_RDMA, 1);
	VIOC_DTRC_RDMA_SetY2R(HwVIOC_DTRC_RDMA, y2r, 0x2);
	VIOC_DTRC_RDMA_SetIDSEL(HwVIOC_DTRC_RDMA, 0x101, 0, 0);

	VIOC_DTRC_RDMA_DTCTRL(HwVIOC_DTRC_RDMA, 0, 0x0, 0, 0, 0xff, 0, 0, 0, 0);
	VIOC_DTRC_RDM_CTRL(HwVIOC_DTRC_RDMA, arid_reg_ctrl, clk_dis, soft_reset,
			   detile_by_addr_en, pf_cnt_en);
	VIOC_DTRC_RDMA_F0_ADDR(HwVIOC_DTRC_RDMA, YCmp, YTable, UVCmp, UVTable);
	VIOC_DTRC_RDMA_F0_SIZE(HwVIOC_DTRC_RDMA, frm0_cbsr_width,
			       frm0_cbsr_height);
	VIOC_DTRC_RDMA_FRM0_CROP(HwVIOC_DTRC_RDMA, 1, pos_x, pos_y, Fwidth,
				 Fheight);
	//	VIOC_DTRC_RDMA_F0_BYP_DETILE_ADDR	(HwVIOC_DTRC_RDMA,
	//frm0_y_byp_detile_addr_s, frm0_y_byp_detile_addr_e,
	//frm0_uv_byp_detile_addr_s, frm0_uv_byp_detile_addr_e);
	VIOC_DTRC_RDMA_ARIDR(HwVIOC_DTRC_RDMA, ARID_REG);
	VIOC_DTRC_RDMA_FETCH_ARID(HwVIOC_DTRC_RDMA, 0, 1);
	//	VIOC_DTRC_RDMA_DCTL			        (HwVIOC_DTRC_RDMA,
	//frm0_data_rdy, frm0_main8, frm0_vinv, frm0_byp, frm1_data_rdy,
	//frm1_main8, frm1_vinv, frm1_byp); 	VIOC_DTRC_RDMA_F1_ADDR
	//(HwVIOC_DTRC_RDMA, YCmp, YTable, UVCmp, UVTable);
	//	VIOC_DTRC_RDMA_F1_SIZE		        (HwVIOC_DTRC_RDMA,
	//frm1_cbsr_width, frm1_cbsr_height); 	VIOC_DTRC_RDMA_FRM1_CROP
	//(HwVIOC_DTRC_RDMA, 1, 0, 0, width, height);
	//	VIOC_DTRC_RDMA_F1_BYP_DETILE_ADDR   (HwVIOC_DTRC_RDMA,
	//frm1_y_byp_detile_addr_s, frm1_y_byp_detile_addr_e,
	//frm1_uv_byp_detile_addr_s, frm1_uv_byp_detile_addr_e);
	VIOC_DTRC_RDMA_DCTL(HwVIOC_DTRC_RDMA, 0x1, frm0_main8, frm0_vinv,
			    frm0_byp, 0x1, frm1_main8, frm1_vinv, frm1_byp);
	VIOC_DTRC_RDMA_SetImageUpdate(HwVIOC_DTRC_RDMA, 1);
}
EXPORT_SYMBOL(tca_dtrc_convter_driver_set);
