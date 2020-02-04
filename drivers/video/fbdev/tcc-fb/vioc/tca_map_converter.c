/****************************************************************************
 * linux/drivers/video/tca_map_converter.c
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
#include <video/tcc/vioc_mc.h>
#include <video/tcc/vioc_config.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

//#define CHROMA_8X4	//two 8x4 grouping (block-interleaving) for compression
//unit
#define RECALC_BASE_ADDRESS // To prevent broken image when cropping with 480,0
			    // ~ 2880x2160 in 4K(3840x2160)

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/tccfb.h>
#endif

#define MAX_WAIT_TIEM 400 //0xF0000

extern unsigned int tca_get_main_decompressor_num(void);

void tca_map_convter_wait_done(unsigned int component_num)
{
	volatile unsigned int loop = 0, upd_loop = 0;
	volatile void __iomem *HwVIOC_MC;
	unsigned int value;

	HwVIOC_MC = VIOC_MC_GetAddress(component_num);
	if (HwVIOC_MC == NULL) {
		pr_err("[ERR][MAPC] %s: HwVIOC_MC is null\n", __func__);
		return;
	}

	for (loop = 0; loop <= MAX_WAIT_TIEM; loop++) {
		value = __raw_readl(HwVIOC_MC + MC_STAT);
		if (!(value & MC_STAT_BUSY_MASK))
			break;
		udelay(100);
	}

	for (upd_loop = 0; upd_loop < MAX_WAIT_TIEM; upd_loop++) {
		value = __raw_readl(HwVIOC_MC + MC_CTRL);
		if (!(value & MC_CTRL_UPD_MASK))
			break;
		udelay(100);
	}

	if ((loop >= MAX_WAIT_TIEM) || (upd_loop >= MAX_WAIT_TIEM))
		pr_err("[ERR][MAPC] %s  Loop (EOF :0x%d  UPD:0x%x) \n", __func__, loop,
		       upd_loop);
}

void tca_map_convter_swreset(unsigned int component_num)
{
	VIOC_CONFIG_SWReset(component_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(component_num, VIOC_CONFIG_CLEAR);
}

void tca_map_convter_onoff(unsigned int component_num, unsigned int onoff,
			   unsigned int wait_done)
{
	volatile void __iomem *HwVIOC_MC = VIOC_MC_GetAddress(component_num);

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

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (!onoff)
		VIOC_CONFIG_DV_EX_VIOC_PROC(component_num);
#endif

	if (!onoff){
		VIOC_MC_FRM_SIZE(HwVIOC_MC, 0, 0);
	}

	VIOC_MC_Start_OnOff(HwVIOC_MC, onoff);
	if (!onoff && wait_done) {
		 // no need to wait because
		 // VIOC_CONFIG_DMAPath_UnSet() will check it.
		tca_map_convter_wait_done(component_num);
		//tca_map_convter_swreset(component_num);
	}

#if 0//defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (get_vioc_index(component_num) == 0) {
		volatile void __iomem *pDisp_DV =
			VIOC_DV_GetAddress((DV_DISP_TYPE)EDR_BL);
		if (onoff)
			VIOC_V_DV_Turnon(pDisp_DV, NULL);
		else
			VIOC_V_DV_Turnoff(pDisp_DV, NULL);
	}
#endif
}

unsigned int __tca_convert_bit_depth(unsigned int in_bitDepth)
{
	unsigned int out_bitDepth = 0;

	if (in_bitDepth == 8)
		out_bitDepth = 0;
	else if (in_bitDepth == 16)
		out_bitDepth = 1;
	else
		out_bitDepth = 2;

	return out_bitDepth;
}

void tca_map_convter_set(unsigned int component_num,
			 struct tcc_lcdc_image_update *ImageInfo, int y2r)
{
	uint pic_height;
	uint line_height;
	uint bit_depth_y;
	uint bit_depth_c;
	uint offset_base_y, offset_base_c;
	uint frame_base_y, frame_base_c;
	volatile void __iomem *HwVIOC_MC =
		VIOC_MC_GetAddress(component_num);

#if 0
	ImageInfo->private_data.mapConv_info.m_CompressedY[0] = 0x31900000;
	ImageInfo->private_data.mapConv_info.m_CompressedCb[0] = 0x31900000+0x7F8000;
	ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0] = 0x32900000;
	ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0] = 0x32900000+0x272000;
#endif
#if 0 // cropping test~
	ImageInfo->crop_left = 480;
	ImageInfo->crop_right = ImageInfo->crop_left + 2880;
	ImageInfo->crop_top = 0;
	ImageInfo->crop_bottom = ImageInfo->crop_top + 2160;
#endif

#if 0  // debug log
	{
		printk("[DBG][MAPC] MC[0x%x] >> R[0x%lx/0x%lx/0x%lx] M[%d] idx[%d], ID[%d] Buff:[%dx%d]%dx%d F:%dx%d I:%dx%d Str(%d/%d) C:0x%08x/0x%08x T:0x%08x/0x%08x bpp(%d/%d) crop(%d/%d~%dx%d)\n", component_num,
				__raw_readl(HwVIOC_MC+MC_CTRL), __raw_readl(HwVIOC_MC+MC_FRM_BASE_Y), __raw_readl(HwVIOC_MC+MC_STAT),
				ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO], ImageInfo->private_data.optional_info[VID_OPT_DISP_OUT_IDX], ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID],
				ImageInfo->private_data.optional_info[VID_OPT_FRAME_WIDTH], ImageInfo->private_data.optional_info[VID_OPT_FRAME_HEIGHT],
				ImageInfo->private_data.optional_info[VID_OPT_BUFFER_WIDTH], ImageInfo->private_data.optional_info[VID_OPT_BUFFER_HEIGHT],
				ImageInfo->Frame_width, ImageInfo->Frame_height, ImageInfo->Image_width, ImageInfo->Image_height,
				ImageInfo->private_data.mapConv_info.m_uiLumaStride, ImageInfo->private_data.mapConv_info.m_uiChromaStride,
				ImageInfo->private_data.mapConv_info.m_CompressedY[0],	ImageInfo->private_data.mapConv_info.m_CompressedCb[0],
				ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0], ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0],
				ImageInfo->private_data.mapConv_info.m_uiLumaBitDepth, ImageInfo->private_data.mapConv_info.m_uiChromaBitDepth,
				ImageInfo->crop_left, ImageInfo->crop_top, (ImageInfo->crop_right-ImageInfo->crop_left),(ImageInfo->crop_bottom-ImageInfo->crop_top));
	}
#endif //

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH() && get_vioc_index(component_num) == tca_get_main_decompressor_num())
		y2r = 0;
#endif

	bit_depth_y = __tca_convert_bit_depth(
		ImageInfo->private_data.mapConv_info.m_uiLumaBitDepth);
	bit_depth_c = __tca_convert_bit_depth(
		ImageInfo->private_data.mapConv_info.m_uiChromaBitDepth);

#if defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	if( ImageInfo->private_data.mapConv_info.m_Reserved[0] == 16 ) //VP9
		pic_height = (((ImageInfo->Frame_height + 63) >> 6) << 6);
	else
#endif
		pic_height = (((ImageInfo->Frame_height + 15) >> 4) << 4);

#if defined(RECALC_BASE_ADDRESS)
	//--------------------------------------
	// Offset table (based on x/ypos)
	line_height = pic_height / 4; // 4 line --> 1 line
	offset_base_y =
		ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0] +
		((ImageInfo->crop_left / 256) * line_height * 2 +
		 (ImageInfo->crop_top / 4) * 2) *
			16;
#ifdef CHROMA_8X4
	height = (((pic_height / 2) + 15) / 16) * 16 / 4;
	offset_base_c =
		ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0] +
		(((ImageInfo->crop_left / 2) / 128) * height * 2 +
		 ((ImageInfo->crop_top / 2) / 4) * 2) *
			16;
#else
	offset_base_c =
		ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0] +
		(((ImageInfo->crop_left / 2) / 256) * line_height * 2 +
		 ((ImageInfo->crop_top / 2) / 2) * 2) *
			16; // 16x2
#endif

	//--------------------------------------
	// Comp. frame (based on ypos)
	frame_base_y =
		ImageInfo->private_data.mapConv_info.m_CompressedY[0] +
		(ImageInfo->crop_top / 4) *
			ImageInfo->private_data.mapConv_info.m_uiLumaStride;
	frame_base_c =
		ImageInfo->private_data.mapConv_info.m_CompressedCb[0] +
		(ImageInfo->crop_top / 4) *
			ImageInfo->private_data.mapConv_info.m_uiChromaStride;
#else
	offset_base_y =
		ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0];
	offset_base_c =
		ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0];
	frame_base_y = ImageInfo->private_data.mapConv_info.m_CompressedY[0];
	frame_base_c = ImageInfo->private_data.mapConv_info.m_CompressedCb[0];
#endif

	VIOC_MC_ENDIAN(HwVIOC_MC,
		       ImageInfo->private_data.mapConv_info.m_uiFrameEndian,
		       ImageInfo->private_data.mapConv_info.m_uiFrameEndian);
	VIOC_MC_OFFSET_BASE(HwVIOC_MC, offset_base_y, offset_base_c);
	VIOC_MC_FRM_BASE(HwVIOC_MC, frame_base_y, frame_base_c);
	VIOC_MC_Start_BitDepth(HwVIOC_MC, bit_depth_c, bit_depth_y);
	VIOC_MC_FRM_POS(HwVIOC_MC, ImageInfo->crop_left, ImageInfo->crop_top);
	VIOC_MC_FRM_SIZE(HwVIOC_MC,
			 (ImageInfo->crop_right - ImageInfo->crop_left),
			 (ImageInfo->crop_bottom - ImageInfo->crop_top));
	VIOC_MC_FRM_SIZE_MISC(
		HwVIOC_MC, pic_height,
		ImageInfo->private_data.mapConv_info.m_uiLumaStride,
		ImageInfo->private_data.mapConv_info.m_uiChromaStride);

	VIOC_MC_DITH_CONT(HwVIOC_MC, 0, 0);
	VIOC_MC_Y2R_OnOff(HwVIOC_MC, y2r, 0x2);
	VIOC_MC_SetDefaultAlpha(HwVIOC_MC, 0x3FF);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH() &&
			get_vioc_index(component_num) == tca_get_main_decompressor_num())
	{
		volatile void __iomem *pDisp_DV =
		VIOC_DV_GetAddress((DV_DISP_TYPE)EDR_BL);

		if (ImageInfo->Lcdc_layer == RDMA_VIDEO || ImageInfo->Lcdc_layer == RDMA_LASTFRM)
		{
			if (vioc_get_out_type() == ImageInfo->private_data.dolbyVision_info.reg_out_type)
			{
				VIOC_V_DV_SetPXDW(pDisp_DV, NULL, VIOC_PXDW_FMT_24_RGB888);
				VIOC_V_DV_SetSize(pDisp_DV, NULL, ImageInfo->offset_x, ImageInfo->offset_y, Hactive, Vactive);
				VIOC_V_DV_Turnon(pDisp_DV, NULL);

				VIOC_MC_Start_OnOff(HwVIOC_MC, 1);

				vioc_v_dv_prog( ImageInfo->private_data.dolbyVision_info.md_hdmi_addr,
								ImageInfo->private_data.dolbyVision_info.reg_addr,
								ImageInfo->private_data.optional_info[VID_OPT_CONTENT_TYPE],
								1);
			}
			else
			{
				pr_err("[ERR][MAPC] 1 Dolby Out type mismatch (%d != %d)\n", vioc_get_out_type(), ImageInfo->private_data.dolbyVision_info.reg_out_type);
			}
		} else {
			pr_err("[ERR][MAPC] @@@@@@@@@ 3 @@@@@@@@@@ Should be implement other layer configuration. \n");
			return;
		}
	}
	else
#endif
	{
		VIOC_MC_Start_OnOff(HwVIOC_MC, 1);
	}
}
EXPORT_SYMBOL(tca_map_convter_set);

void tca_map_convter_driver_set(unsigned int component_num, unsigned int Fwidth,
				unsigned int Fheight, unsigned int pos_x,
				unsigned int pos_y, unsigned int Cwidth,
				unsigned int Cheight, unsigned int y2r,
				hevc_MapConv_info_t *mapConv_info)
{
	uint pic_height;
	uint line_height;
	uint bit_depth_y;
	uint bit_depth_c;
	uint offset_base_y, offset_base_c;
	uint frame_base_y, frame_base_c;
	volatile void __iomem *HwVIOC_MC =
		VIOC_MC_GetAddress(component_num);
#if 0  // debug log
	{
		printk("[DBG][MAPC] MC[%d] >> R[0x%lx/0x%lx/0x%lx] M[%d] F:%dx%d Str(%d/%d) C:0x%08x/0x%08x T:0x%08x/0x%08x bpp(%d/%d) crop(%d/%d~%dx%d) Reserved(%d)\n", get_vioc_index(component_num),
				__raw_readl(HwVIOC_MC+MC_CTRL), __raw_readl(HwVIOC_MC+MC_FRM_BASE_Y), __raw_readl(HwVIOC_MC+MC_STAT),
				1, Fwidth, Fheight,
				mapConv_info->m_uiLumaStride, mapConv_info->m_uiChromaStride,
				mapConv_info->m_CompressedY[0],	mapConv_info->m_CompressedCb[0],
				mapConv_info->m_FbcYOffsetAddr[0], mapConv_info->m_FbcCOffsetAddr[0],
				mapConv_info->m_uiLumaBitDepth, mapConv_info->m_uiChromaBitDepth,
				pos_x, pos_y, Cwidth,Cheight, mapConv_info->m_Reserved[0]);
	}
#endif //

	bit_depth_y = __tca_convert_bit_depth(mapConv_info->m_uiLumaBitDepth);
	bit_depth_c = __tca_convert_bit_depth(mapConv_info->m_uiChromaBitDepth);

#if defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	if( mapConv_info->m_Reserved[0] == 16 ) //VP9
		pic_height = (((Fheight + 63) >> 6) << 6);
	else
#endif
		pic_height = (((Fheight + 15) >> 4) << 4);

#if defined(RECALC_BASE_ADDRESS)
	//--------------------------------------
	// Offset table (based on x/ypos)
	line_height = pic_height / 4; // 4 line --> 1 line
	offset_base_y =
		mapConv_info->m_FbcYOffsetAddr[0] +
		((pos_x / 256) * line_height * 2 + (pos_y / 4) * 2) * 16;
#ifdef CHROMA_8X4
	height = (((pic_height / 2) + 15) / 16) * 16 / 4;
	offset_base_c =
		mapConv_info->m_FbcCOffsetAddr[0] +
		(((pos_x / 2) / 128) * height * 2 + ((pos_y / 2) / 4) * 2) * 16;
#else
	offset_base_c = mapConv_info->m_FbcCOffsetAddr[0] +
			(((pos_x / 2) / 256) * line_height * 2 +
			 ((pos_y / 2) / 2) * 2) *
				16; // 16x2
#endif

	//--------------------------------------
	// Comp. frame (based on ypos)
	frame_base_y = mapConv_info->m_CompressedY[0] +
		       (pos_y / 4) * mapConv_info->m_uiLumaStride;
	frame_base_c = mapConv_info->m_CompressedCb[0] +
		       (pos_y / 4) * mapConv_info->m_uiChromaStride;
#else
	offset_base_y = mapConv_info->m_FbcYOffsetAddr[0];
	offset_base_c = mapConv_info->m_FbcCOffsetAddr[0];
	frame_base_y = mapConv_info->m_CompressedY[0];
	frame_base_c = mapConv_info->m_CompressedCb[0];
#endif

	VIOC_MC_ENDIAN(HwVIOC_MC, mapConv_info->m_uiFrameEndian,
		       mapConv_info->m_uiFrameEndian);
	VIOC_MC_OFFSET_BASE(HwVIOC_MC, offset_base_y, offset_base_c);
	VIOC_MC_FRM_BASE(HwVIOC_MC, frame_base_y, frame_base_c);
	VIOC_MC_Start_BitDepth(HwVIOC_MC, bit_depth_c, bit_depth_y);
	VIOC_MC_FRM_POS(HwVIOC_MC, pos_x, pos_y);
	VIOC_MC_FRM_SIZE(HwVIOC_MC, Cwidth, Cheight);
	VIOC_MC_FRM_SIZE_MISC(HwVIOC_MC, pic_height,
			      mapConv_info->m_uiLumaStride,
			      mapConv_info->m_uiChromaStride);
	VIOC_MC_DITH_CONT(HwVIOC_MC, 0, 0);
	VIOC_MC_Y2R_OnOff(HwVIOC_MC, y2r, 0x2);
	VIOC_MC_SetDefaultAlpha(HwVIOC_MC, 0x3FF);
}
EXPORT_SYMBOL(tca_map_convter_driver_set);
