/*
*   tcc_vioc_viqe_interface.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: support for vioc viqe driver
 *
 *   Copyright (C) 2008-2009 Telechips
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
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/fb.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>

#include <soc/tcc/pmap.h>
#include <video/tcc/tcc_types.h>

#include <video/tcc/tccfb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_deintls.h>

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tca_map_converter.h>
#endif
#ifdef CONFIG_VIOC_DTRC
#include <video/tcc/tca_dtrc_converter.h>
#endif
#define MAIN_USED 0x1
#define SUB_USED  0x2

#include "../tcc_vsync.h"
#include "tcc_vioc_viqe.h"
#include "tcc_vioc_viqe_interface.h"

#ifdef CONFIG_TCC_VIOCMG
#include <video/tcc/viocmg.h>
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>

#define DEF_DV_CHECK_NUM 1// 2
static int bStep_Check = DEF_DV_CHECK_NUM;
static unsigned int nFrame = 0;

extern unsigned int DV_PROC_CHECK;

extern int tca_edr_el_configure(struct tcc_lcdc_image_update *Src_ImageInfo, struct tcc_lcdc_image_update *El_ImageInfo, unsigned int *ratio);
#endif

extern TCC_OUTPUT_TYPE Output_SelectMode;

extern struct tcc_dp_device *tca_fb_get_displayType(TCC_OUTPUT_TYPE check_type);

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
extern void set_hdmi_drm(HDMI_DRM_MODE mode, struct tcc_lcdc_image_update *pImage, unsigned int layer);
#endif

#ifdef CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME
extern void tcc_video_frame_backup(struct tcc_lcdc_image_update *Image);
#endif
extern void tcc_video_post_process(struct tcc_lcdc_image_update *ImageInfo);

/*******			define extern symbol	******/
extern void tca_lcdc_set_onthefly(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);
extern void tca_scale_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);

//#define USE_DEINTERLACE_S_IN30Hz //for external M2M
//#define USE_DEINTERLACE_S_IN60Hz //for internal M2M and on-the-fly

//#define DYNAMIC_USE_DEINTL_COMPONENT
//#define MAX_VIQE_FRAMEBUFFER

#define BUFFER_CNT_FOR_M2M_MODE (3)

#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
static unsigned int gPMEM_VIQE_SUB_BASE = 0;
static unsigned int gPMEM_VIQE_SUB_SIZE = 0;

static int gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
atomic_t gFrmCnt_Sub_60Hz;
static int gVIQE1_Init_State = 0;
#endif // CONFIG_TCC_VIQE_FOR_SUB_M2M

static unsigned int gPMEM_VIQE_BASE = 0;
static unsigned int gPMEM_VIQE_SIZE = 0;
#define MAX_MAIN_VIQE (((1920 * (1088 / 2 ) * 4 * 3) / 2) * 4)

static struct tcc_viqe_common_virt_addr_info_t viqe_common_info;
static struct tcc_viqe_m2m_virt_addr_info_t main_m2m_info;
static struct tcc_viqe_m2m_virt_addr_info_t sub_m2m_info;
static struct tcc_viqe_60hz_virt_addr_info_t viqe_60hz_lcd_info;
static struct tcc_viqe_60hz_virt_addr_info_t viqe_60hz_external_info;
static struct tcc_viqe_60hz_virt_addr_info_t viqe_60hz_external_sub_info;
static struct tcc_viqe_m2m_scaler_type_t *scaler;
static struct tcc_viqe_m2m_scaler_type_t *scaler_sub;

static struct tcc_lcdc_image_update  output_m2m_image;
static struct tcc_lcdc_image_update  output_m2m_image_sub;

static unsigned int overlay_buffer[2][BUFFER_CNT_FOR_M2M_MODE];
static int current_buffer_idx[2];

static int gFrmCnt_30Hz = 0;
static int gUse_sDeintls = 0;

static int gOutputMode = 0;
atomic_t gFrmCnt_60Hz;
static int gLcdc_layer_60Hz = -1;
static VIOC_VIQE_FMT_TYPE gViqe_fmt_60Hz;
static int gImg_fmt_60Hz = -1;
static int gDeintlS_Use_60Hz = 0;
static int gDeintlS_Use_Plugin = 0;

static int not_exist_viqe1 = 0;
static int gUse_MapConverter = 0;
#ifdef CONFIG_VIOC_DTRC
static int gUse_DtrcConverter = 0;
#endif

#ifndef USE_DEINTERLACE_S_IN30Hz
static int gusingDI_S = 0;
static int gbfield_30Hz =0;
static VIOC_VIQE_DEINTL_MODE gDI_mode_30Hz = VIOC_VIQE_DEINTL_MODE_2D;
#else
static int gusingDI_S = 0;
#endif

#ifndef USE_DEINTERLACE_S_IN60Hz
static VIOC_VIQE_DEINTL_MODE gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
#else
static VIOC_VIQE_DEINTL_MODE gDI_mode_60Hz = VIOC_VIQE_DEINTL_S;
#endif

#ifdef CONFIG_USE_SUB_MULTI_FRAME
struct list_head video_queue_list;
static struct tcc_lcdc_image_update  sub_m2m_image[VSYNC_MAX];
#endif
static atomic_t viqe_sub_m2m_init;

static int gVIQE_Init_State = 0;

static int debug = 0;
#define dprintk(msg...)	 if (debug) { printk("[DBG][VIQE] " msg); }
#define iprintk(msg...)  if (debug) { printk("[DBG][VIQE] " msg); }
#define dvprintk(msg...)  if (debug) { printk("[DBG][VIQE] DV_M2M: " msg); }

extern void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr, unsigned int src_imgx, unsigned int  src_imgy,
					unsigned int start_x, unsigned int start_y, unsigned int* Y, unsigned int* U,unsigned int* V);

extern int TCC_VIQE_Scaler_Init_Buffer_M2M(void);
extern void TCC_VIQE_Scaler_DeInit_Buffer_M2M(void);

#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
extern unsigned char hdmi_get_hdmimode(void);
#endif

//#define PRINT_OUT_M2M_MS
#ifdef PRINT_OUT_M2M_MS
static struct timeval t1;
static unsigned long time_gap_max_us = 0;
static unsigned long time_gap_min_us = 0;
static unsigned long time_gap_total_us = 0;
static unsigned long time_gap_total_count = 0;

void tcc_GetTimediff_ms(struct timeval time1)
{
	unsigned int time_diff_us = 0;
	struct timeval t2;

	do_gettimeofday( &t2 );

	if( t2.tv_sec <= time1.tv_sec && t2.tv_usec <= time1.tv_usec)
	{
		pr_warn("[WAR][VIQE] strange Time-diff %2d.%2d <= %2d.%2d \n", t2.tv_sec, t2.tv_usec, time1.tv_sec, time1.tv_usec);
	}

	time_diff_us = (t2.tv_sec- time1.tv_sec)*1000*1000;
	time_diff_us += (t2.tv_usec-time1.tv_usec);

	if(time_gap_min_us == 0)
		time_gap_min_us = time_diff_us;

	if(time_gap_max_us < time_diff_us)
		time_gap_max_us = time_diff_us;

	if(time_gap_min_us > time_diff_us)
		time_gap_min_us = time_diff_us;

	if(time_gap_total_count % 60 == 0){
		dprintk("Time Diff(ms) :: Max(%2d.%2d)/Min(%2d.%2d) => Avg(%2d.%2d) \n", (unsigned int)(time_gap_max_us/1000), (unsigned int)(time_gap_max_us%1000),
					(unsigned int)(time_gap_min_us/1000), (unsigned int)(time_gap_min_us%1000),
					(unsigned int)((time_gap_total_us/time_gap_total_count)/1000), (unsigned int)((time_gap_total_us/time_gap_total_count)%1000));
	}
	time_gap_total_us += time_diff_us;
	time_gap_total_count++;
}
#endif

void tcc_sdintls_swreset(void)
{
	VIOC_CONFIG_SWReset(VIOC_DEINTLS0, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_DEINTLS0, VIOC_CONFIG_CLEAR);
}

void tcc_viqe_swreset(unsigned int viqe_num)
{
	if(get_vioc_index(viqe_num) > 2)
		return;

	VIOC_CONFIG_SWReset(viqe_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(viqe_num, VIOC_CONFIG_CLEAR);
}

void tcc_scaler_swreset(unsigned int scaler_num)
{
	if(get_vioc_index(scaler_num) > 4)
		return;
	VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
}

static bool viqe_y2r_mode_check(void)
{
	bool y2r_on = false;

#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	y2r_on = false;
#else
	#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
	if(gOutputMode != TCC_OUTPUT_HDMI || hdmi_get_hdmimode())
		y2r_on = false;
	else
		y2r_on = true;
#elif defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
	if(gOutputMode == TCC_OUTPUT_COMPOSITE)
		y2r_on = false;
	else
		y2r_on = true;
#else
	y2r_on = true;
#endif
#endif

	return y2r_on;
}

void TCC_VIQE_DI_PlugInOut_forAlphablending(int plugIn)
{
	if(gUse_sDeintls)
	{
		if(plugIn)
		{
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
			VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, main_m2m_info.gVIQE_RDMA_num_m2m);
		}
		else
		{
			VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
		}			
	}
	else
	{
		if(plugIn)
		{
			VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_VIQE1, main_m2m_info.gVIQE_RDMA_num_m2m);
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
		}
		else
		{
			VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
			VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_VIQE1);
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
			gFrmCnt_30Hz = 0;
			atomic_set(&gFrmCnt_60Hz, 0);
		}
	}
}

/* VIQE Set */
//////////////////////////////////////////////////////////////////////////////////////////
void TCC_VIQE_DI_Init(VIQE_DI_TYPE *viqe_arg)
{
#ifndef USE_DEINTERLACE_S_IN30Hz
	unsigned int deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3;
	int imgSize;
	int top_size_dont_use = OFF;		//If this value is OFF, The size information is get from VIOC modules.
#endif
	unsigned int framebufWidth, framebufHeight;
	VIOC_VIQE_FMT_TYPE img_fmt = VIOC_VIQE_FMT_YUV420;
	volatile void __iomem *pRDMA, *pVIQE;
	int nRDMA = 0, nVIQE = 0;

	bool y2r_on = false;

	if(0 > TCC_VIQE_Scaler_Init_Buffer_M2M())
	{
		pr_err("[ERR][VIQE] %s-%d : memory allocation is failed.\n", __func__, __LINE__);
		return;
	}

	if(viqe_arg->nRDMA != 0) {
		nRDMA = VIOC_RDMA00 + viqe_arg->nRDMA;
		pRDMA = VIOC_RDMA_GetAddress(nRDMA);
	} else {
		pRDMA = main_m2m_info.pRDMABase_m2m;
		nRDMA = main_m2m_info.gVIQE_RDMA_num_m2m;
	}

	viqe_arg->crop_top = (viqe_arg->crop_top >>1)<<1;
#ifdef MAX_VIQE_FRAMEBUFFER
	framebufWidth = 1920;
	framebufHeight = 1088;
#else
	framebufWidth = ((viqe_arg->srcWidth - viqe_arg->crop_left - viqe_arg->crop_right) >> 3) << 3;			// 8bit align
	framebufHeight = ((viqe_arg->srcHeight - viqe_arg->crop_top - viqe_arg->crop_bottom) >> 2) << 2;		// 4bit align
#endif

	pr_info("[INF][VIQE] TCC_VIQE_DI_Init, W:%d, H:%d, FMT:%s, OddFirst:%d, %s-%d\n",
			framebufWidth, framebufHeight, (img_fmt?"YUV422":"YUV420"), viqe_arg->OddFirst, 
			viqe_arg->use_sDeintls ? "S-Deintls" : "VIQE", viqe_arg->use_Viqe0 ? 0:1);
	
	if(viqe_common_info.gBoard_stb == 1) {
		VIOC_RDMA_SetImageDisable(pRDMA);
		//TCC_OUTPUT_FB_ClearVideoImg();
	}

	VIOC_RDMA_SetImageY2REnable(pRDMA, false);
	VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02); /* Y2RMode Default 0 (Studio Color) */
	VIOC_RDMA_SetImageIntl(pRDMA, 1);
	VIOC_RDMA_SetImageBfield(pRDMA, viqe_arg->OddFirst);
	
	if(viqe_arg->use_sDeintls)
	{
		gUse_sDeintls = 1;
		tcc_sdintls_swreset();
		VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
		pr_info("[INF][VIQE] DEINTL-Simple\n");
	}
	else
	{
		gUse_sDeintls = 0;
#ifndef USE_DEINTERLACE_S_IN30Hz
		imgSize = (framebufWidth * (framebufHeight / 2 ) * 4 * 3 / 2);

		if(not_exist_viqe1 || viqe_arg->use_Viqe0)
		{
			nVIQE = viqe_common_info.gVIOC_VIQE0;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);

			if(gPMEM_VIQE_SIZE < (imgSize*4))
				pr_warn("[WAR][VIQE] 0: Increase VIQE pmap size for VIQE!! Current(0x%x)/Need(0x%x) \n", gPMEM_VIQE_SIZE, (imgSize*4));
			deintl_dma_base0	= gPMEM_VIQE_BASE;
		}
		else
		{
			nVIQE = viqe_common_info.gVIOC_VIQE1;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);

			if((gPMEM_VIQE_SIZE/2) < (imgSize*4)){
				pr_warn("[WAR][VIQE] 1: Increase VIQE pmap size for VIQE1!! Current(0x%x)/Need(0x%x) \n", gPMEM_VIQE_SIZE, (imgSize*4));
				deintl_dma_base0	= gPMEM_VIQE_BASE;
			}
			else{
				deintl_dma_base0	= gPMEM_VIQE_BASE + (gPMEM_VIQE_SIZE/2);
			}
		}

		tcc_viqe_swreset(nVIQE);
		deintl_dma_base1	= deintl_dma_base0 + imgSize;
		deintl_dma_base2	= deintl_dma_base1 + imgSize;
		deintl_dma_base3	= deintl_dma_base2 + imgSize;

		if (top_size_dont_use == OFF)
		{
			framebufWidth  = 0;
			framebufHeight = 0;
		}

		VIOC_VIQE_IgnoreDecError(pVIQE, ON, ON, ON);
		VIOC_VIQE_SetControlRegister(pVIQE, framebufWidth, framebufHeight, img_fmt);
		VIOC_VIQE_SetDeintlRegister(pVIQE, img_fmt, top_size_dont_use, framebufWidth, framebufHeight, gDI_mode_30Hz, deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3);
		VIOC_VIQE_SetControlEnable(pVIQE, OFF, OFF, OFF, OFF, ON);

		if(get_vioc_index(nRDMA) == get_vioc_index(VIOC_RDMA03))
		{
			y2r_on = viqe_y2r_mode_check();
			VIOC_VIQE_SetImageY2RMode(pVIQE, 0x02);
			VIOC_VIQE_SetImageY2REnable(pVIQE, y2r_on);
		}

		VIOC_CONFIG_PlugIn(nVIQE, nRDMA);

		if(viqe_arg->OddFirst)
			gbfield_30Hz =1;
		else
			gbfield_30Hz =0;

		pr_info("[INF][VIQE] DEINTL-VIQE(%d)\n", nVIQE);
#else
		tcc_sdintls_swreset();
		VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
		gusingDI_S = 1;
		pr_info("[INF][VIQE] DEINTL-S\n");
#endif
	}

	gFrmCnt_30Hz = 0;
	gVIQE_Init_State = 1;
}

void TCC_VIQE_DI_Run(VIQE_DI_TYPE *viqe_arg)
{
#ifndef USE_DEINTERLACE_S_IN30Hz
	VIOC_PlugInOutCheck VIOC_PlugIn;
	unsigned int JudderCnt = 0;
#endif
	volatile void __iomem *pRDMA, *pVIQE;
	int nRDMA = 0, nVIQE;

	if(viqe_arg->nRDMA != 0)
	{
 		nRDMA = VIOC_RDMA00 + viqe_arg->nRDMA;
		pRDMA = VIOC_RDMA_GetAddress(nRDMA);
	}
	else
	{
		pRDMA = main_m2m_info.pRDMABase_m2m;
		nRDMA = main_m2m_info.gVIQE_RDMA_num_m2m;
	}


	if(viqe_common_info.gBoard_stb == 1) {
		if(gVIQE_Init_State == 0)
		{
			dprintk("%s VIQE block isn't initailized\n", __func__);
			return;
		}
	}

	if(gUse_sDeintls)
	{
		VIOC_RDMA_SetImageY2REnable(pRDMA, false);
		VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02); /* Y2RMode Default 0 (Studio Color) */

		if(viqe_arg->DI_use)
		{
			VIOC_RDMA_SetImageIntl(pRDMA, 1);
			VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
		}
		else
		{
			VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
			VIOC_RDMA_SetImageIntl(pRDMA, 0);
		}
	}
	else
	{
	#ifndef USE_DEINTERLACE_S_IN30Hz
		if(not_exist_viqe1 || viqe_arg->use_Viqe0)
		{
			nVIQE = viqe_common_info.gVIOC_VIQE0;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		}
		else
		{
			nVIQE = viqe_common_info.gVIOC_VIQE1;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		}

		if (VIOC_CONFIG_Device_PlugState(nVIQE, &VIOC_PlugIn) == VIOC_DEVICE_INVALID){
			return;
		}
		else{
			if(!VIOC_PlugIn.enable) {
				pr_warn("[WAR][VIQE] %s VIQE block isn't pluged!!!\n", __func__);
				return;
			}
		}

		if(viqe_arg->DI_use)
		{
			if (gbfield_30Hz) {				// end fied of bottom field
				VIOC_RDMA_SetImageBfield(pRDMA, 1);				// change the bottom to top field
				// if you want to change the base address, you call the RDMA SetImageBase function in this line.
				gbfield_30Hz = 0;
			} else {
				VIOC_RDMA_SetImageBfield(pRDMA, 0);				// change the top to bottom field
				gbfield_30Hz = 1;
			}
			VIOC_RDMA_SetImageY2REnable(pRDMA, false);
			VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02); /* Y2RMode Default 0 (Studio Color) */

			if(gFrmCnt_30Hz >= 3) {
				VIOC_VIQE_SetDeintlMode(pVIQE, VIOC_VIQE_DEINTL_MODE_3D);
				// Process JUDDER CNTS
				JudderCnt = (viqe_arg->srcWidth + 64)/64 -1;
				VIOC_VIQE_SetDeintlJudderCnt(pVIQE, JudderCnt);
				gDI_mode_30Hz = VIOC_VIQE_DEINTL_MODE_3D;
				dprintk("VIQE 3D and BField(%d)\n", gbfield_30Hz);
			} else {
				VIOC_VIQE_SetDeintlMode(pVIQE, VIOC_VIQE_DEINTL_MODE_2D);
				gDI_mode_30Hz = VIOC_VIQE_DEINTL_MODE_2D;
				dprintk("VIQE 2D and BField(%d)\n", gbfield_30Hz);
			}
			VIOC_VIQE_SetControlMode(pVIQE, OFF, OFF, OFF, OFF, ON);
			VIOC_RDMA_SetImageIntl(pRDMA, 1);
		}
		else
		{
			VIOC_RDMA_SetImageY2REnable(pRDMA, true);
			VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_VIQE_SetControlMode(pVIQE, OFF, OFF, OFF, OFF, OFF);
			VIOC_RDMA_SetImageIntl(pRDMA, 0);
			gFrmCnt_30Hz = 0;
		}
	#else
		VIOC_RDMA_SetImageY2REnable(pRDMA, false);
		VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02); /* Y2RMode Default 0 (Studio Color) */
		if(viqe_arg->DI_use) {
			VIOC_RDMA_SetImageIntl(pRDMA, 1);
			if(!gusingDI_S) {
				VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
				gusingDI_S = 1;
			}
		} else {
			VIOC_RDMA_SetImageIntl(pRDMA, 0);
			if(gusingDI_S) {
				VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
				gusingDI_S = 0;
			}
		}
	#endif
	}

	gFrmCnt_30Hz++;	
}

void TCC_VIQE_DI_DeInit(VIQE_DI_TYPE *viqe_arg)
{
	volatile void __iomem *pVIQE = NULL;
	int nVIQE = 0;

	gFrmCnt_30Hz = 0;

	pr_info("[INF][VIQE] TCC_VIQE_DI_DeInit\n");
	if(gUse_sDeintls)
	{
		VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
		tcc_sdintls_swreset();
		gUse_sDeintls = 0;
	}
	else
	{
#ifndef USE_DEINTERLACE_S_IN30Hz
		if(not_exist_viqe1 || viqe_arg->use_Viqe0)
		{
			nVIQE = viqe_common_info.gVIOC_VIQE0;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		}
		else
		{
			nVIQE = viqe_common_info.gVIOC_VIQE1;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		}

		VIOC_CONFIG_PlugOut(nVIQE);
		tcc_viqe_swreset(nVIQE);
#else
		VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
		gusingDI_S = 0;
		tcc_sdintls_swreset();
#endif
	}

	TCC_VIQE_Scaler_DeInit_Buffer_M2M();

	gVIQE_Init_State = 0;
}

#ifdef CONFIG_USE_SUB_MULTI_FRAME
static int video_queue_delete(struct video_queue_t *q)
{
	if(q == NULL) {
		pr_err("[ERR][VIQE] queue data is NULL\n");
		return -1;
	}

	list_del(&q->list);
	kfree(q);
	return 0;
}

static struct video_queue_t *video_queue_pop(void)
{
	struct video_queue_t *q;
	q = list_first_entry(&video_queue_list, struct video_queue_t, list);
	return q;
}

static int video_queue_delete_all(void)
{
	struct video_queue_t *q;
	int ret;
	while (!list_empty(&video_queue_list)){
		q = video_queue_pop();
		ret = video_queue_delete(q);
	}
	return ret;
}

static int video_queue_push( int type)
{
	struct video_queue_t *q;
	q = kzalloc(sizeof(struct video_queue_t), GFP_KERNEL);
	if(!q) {
		pr_err("[ERR][VIQE] %s can not alloc queue \n", __func__);
		return -1;
	}
	q->type = type;
	list_add_tail(&q->list, &video_queue_list);
	return 0;
}

void TCC_VIQE_DI_Push60Hz_M2M(struct tcc_lcdc_image_update *input_image, int type)
{
	video_queue_push(type);
	memcpy(&sub_m2m_image[type], input_image, sizeof(struct tcc_lcdc_image_update));
}
EXPORT_SYMBOL(TCC_VIQE_DI_Push60Hz_M2M);
#endif

/////////////////////////////////////////////////////////////////////////////////////////
void TCC_VIQE_DI_Init60Hz_M2M(TCC_OUTPUT_TYPE outputMode, struct tcc_lcdc_image_update *input_image)
{
	unsigned int deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3;
	int imgSize;
	int top_size_dont_use = OFF;		//If this value is OFF, The size information is get from VIOC modules.
	unsigned int framebufWidth, framebufHeight;
	VIOC_VIQE_FMT_TYPE img_fmt = VIOC_VIQE_FMT_YUV420;
#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_lock = 0;
#endif

#ifdef PRINT_OUT_M2M_MS
	time_gap_max_us = 0;
	time_gap_min_us = 0;
	time_gap_total_us = 0;
	time_gap_total_count = 0;
#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_INIT, input_image, input_image->Lcdc_layer);
#endif

	if(0 > TCC_VIQE_Scaler_Init_Buffer_M2M())
	{
		pr_err("[ERR][VIQE] %s-%d : memory allocation is failed.\n", __func__, __LINE__);
		return;
	}

	gOutputMode = outputMode;
	gUse_sDeintls = 0;

	if (!input_image->deinterlace_mode)
		goto m2m_init_scaler;

#ifdef MAX_VIQE_FRAMEBUFFER
	framebufWidth = 1920;
	framebufHeight = 1088;
#else
	framebufWidth = ((input_image->Frame_width) >> 3) << 3;			// 8bit align
	framebufHeight = ((input_image->Frame_height) >> 2) << 2;		// 4bit align
#endif

	dprintk("%s, W:%d, H:%d, FMT:%s, OddFirst:%d \n", __func__, framebufWidth, framebufHeight, (img_fmt?"YUV422":"YUV420"), input_image->odd_first_flag);

	if(viqe_common_info.gBoard_stb == 1) {
		if (input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
		} else if(input_image->Lcdc_layer == RDMA_VIDEO) {
			VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
		}

	    //TCC_OUTPUT_FB_ClearVideoImg();
	}

	if (input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, false);
		VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
		VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);

		tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE1);

		// If you use 3D(temporal) De-interlace mode, you have to set physical address for using DMA register.
		//If 2D(spatial) mode, these registers are ignored
		imgSize = (framebufWidth * (framebufHeight / 2 ) * 4 * 3 / 2);
		if(gPMEM_VIQE_SUB_SIZE < (imgSize*4)){
			pr_err("[ERR][VIQE] 2: Increase VIQE pmap size for Sub_m2m!! Current(0x%x)/Need(0x%x) \n", gPMEM_VIQE_SUB_SIZE, (imgSize*4));
			deintl_dma_base0	= gPMEM_VIQE_BASE;
		}
		else{
			deintl_dma_base0	= gPMEM_VIQE_SUB_BASE;
		}
		deintl_dma_base1	= deintl_dma_base0 + imgSize;
		deintl_dma_base2	= deintl_dma_base1 + imgSize;
		deintl_dma_base3	= deintl_dma_base2 + imgSize;

		if (top_size_dont_use == OFF)
		{
			framebufWidth  = 0;
			framebufHeight = 0;
		}

		VIOC_VIQE_SetControlRegister(viqe_common_info.pVIQE1, framebufWidth, framebufHeight, img_fmt);
		VIOC_VIQE_SetDeintlRegister(viqe_common_info.pVIQE1, img_fmt, top_size_dont_use, framebufWidth, framebufHeight, gDI_mode_60Hz, deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3);
		VIOC_VIQE_SetControlEnable(viqe_common_info.pVIQE1, OFF, OFF, OFF, OFF, ON);

		VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_VIQE1, sub_m2m_info.gVIQE_RDMA_num_m2m);

		atomic_set(&gFrmCnt_Sub_60Hz, 0);
		gVIQE1_Init_State = 1;
		pr_info("[INF][VIQE] %s - VIQE1\n", __func__);
#else
		VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
		VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
		VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);

		tcc_sdintls_swreset();
		VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, sub_m2m_info.gDEINTLS_RDMA_num_m2m);
		gusingDI_S = 1;
		dprintk("%s - DEINTL-S\n", __func__);
#endif
	}else if (input_image->Lcdc_layer == RDMA_VIDEO) {
		VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, false);
		VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
		VIOC_RDMA_SetImageBfield(main_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);

		#ifdef CONFIG_TCC_VIOCMG
		viqe_lock = viocmg_lock_viqe(VIOCMG_CALLERID_VOUT);
		if(viqe_lock == 0)
		#endif
		{
			tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE0);

			// If you use 3D(temporal) De-interlace mode, you have to set physical address for using DMA register.
			//If 2D(spatial) mode, these registers are ignored
			imgSize = (framebufWidth * (framebufHeight / 2 ) * 4 * 3 / 2);
			if(gPMEM_VIQE_SIZE < (imgSize*4)){
				pr_warn("[WAR][VIQE] 3: Increase VIQE pmap size for Main_m2m!! Current(0x%x)/Need(0x%x) \n", gPMEM_VIQE_SIZE, (imgSize*4));
			}
			deintl_dma_base0	= gPMEM_VIQE_BASE;
			deintl_dma_base1	= deintl_dma_base0 + imgSize;
			deintl_dma_base2	= deintl_dma_base1 + imgSize;
			deintl_dma_base3	= deintl_dma_base2 + imgSize;

			if (top_size_dont_use == OFF)
			{
				framebufWidth  = 0;
				framebufHeight = 0;
			}

			VIOC_VIQE_IgnoreDecError(viqe_common_info.pVIQE0, ON, ON, ON);
			VIOC_VIQE_SetControlRegister(viqe_common_info.pVIQE0, framebufWidth, framebufHeight, img_fmt);
			VIOC_VIQE_SetDeintlRegister(viqe_common_info.pVIQE0, img_fmt, top_size_dont_use, framebufWidth, framebufHeight, gDI_mode_60Hz, deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3);
			VIOC_VIQE_SetControlEnable(viqe_common_info.pVIQE0, OFF, OFF, OFF, OFF, ON);

			VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_VIQE0, main_m2m_info.gVIQE_RDMA_num_m2m);

			atomic_set(&gFrmCnt_60Hz, 0);
			gVIQE_Init_State = 1;
		}
		pr_info("[INF][VIQE] %s - VIQE0\n", __func__);

		#ifdef CONFIG_TCC_VIOCMG
		if(viqe_lock == 0)
			viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
		#endif
	}

m2m_init_scaler:
	if (input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
		TCC_VIQE_Scaler_Sub_Init60Hz_M2M(input_image);
	} else if (input_image->Lcdc_layer == RDMA_VIDEO) {
		TCC_VIQE_Scaler_Init60Hz_M2M(input_image);
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
		if(input_image->private_data.dolbyVision_info.el_offset[0] != 0x00){
			struct tcc_lcdc_image_update ImageInfo_sub;
			memcpy(&ImageInfo_sub, input_image, sizeof(struct tcc_lcdc_image_update));
			ImageInfo_sub.Lcdc_layer = RDMA_VIDEO_SUB;
			ImageInfo_sub.private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
			ImageInfo_sub.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
			TCC_VIQE_Scaler_Sub_Init60Hz_M2M(&ImageInfo_sub);
			DV_PROC_CHECK = DV_DUAL_MODE;
			dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
		}
#endif
	}
}
EXPORT_SYMBOL(TCC_VIQE_DI_Init60Hz_M2M);

void TCC_VIQE_DI_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image, int reset_frmCnt)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	unsigned int JudderCnt = 0;

#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_lock = 0;
#endif

#ifdef PRINT_OUT_M2M_MS
	do_gettimeofday( &t1 );
#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_ON, input_image, input_image->Lcdc_layer);
#endif

	if(Output_SelectMode == TCC_OUTPUT_NONE)
		return;

	if (!input_image->deinterlace_mode) {
		if (input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
		} else if (input_image->Lcdc_layer == RDMA_VIDEO) {
			VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, false);
			VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
			TCC_VIQE_Scaler_Run60Hz_M2M(input_image);
		}
	} else {
		if (input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			if(gVIQE1_Init_State == 0)
			{
				pr_err("[ERR][VIQE] %s VIQE1 block isn't initailized\n", __func__);
				return;
			}

			VIOC_CONFIG_Device_PlugState(viqe_common_info.gVIOC_VIQE1, &VIOC_PlugIn);
			if(VIOC_PlugIn.enable == 0) {
				pr_err("[ERR][VIQE] %s VIQE1 block isn't pluged!!!\n", __func__);
				return;
			}

			if(reset_frmCnt){
				atomic_set(&gFrmCnt_Sub_60Hz,0);
				dprintk("VIQE1 3D -> 2D \n");
			}

			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, false);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

			if(input_image->frameInfo_interlace)
			{
				if(atomic_read(&gFrmCnt_Sub_60Hz) >= 3)
				{
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE1, VIOC_VIQE_DEINTL_MODE_3D);
					// Process JUDDER CNTS
					JudderCnt = (input_image->Frame_width + 64)/64 -1;
					VIOC_VIQE_SetDeintlJudderCnt(viqe_common_info.pVIQE1, JudderCnt);
					gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_3D;
				}
				else
				{
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE1, VIOC_VIQE_DEINTL_MODE_2D);
					gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
				}
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE1, OFF, OFF, OFF, OFF, ON);
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			}
			else
			{
				VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE1, VIOC_VIQE_DEINTL_MODE_BYPASS);
				gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_BYPASS;
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE1, OFF, OFF, OFF, OFF, OFF);
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
				atomic_set(&gFrmCnt_Sub_60Hz, 0);
			}
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
			atomic_inc(&gFrmCnt_Sub_60Hz);
#else
			if(input_image->frameInfo_interlace)
			{
				if(!gusingDI_S) {
					VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, sub_m2m_info.gDEINTLS_RDMA_num_m2m);
					gusingDI_S = 1;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			}
			else
			{
				if(gusingDI_S) {
					VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
					gusingDI_S = 0;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			}
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
#endif
		}
		else if(input_image->Lcdc_layer == RDMA_VIDEO) {
			#ifdef CONFIG_TCC_VIOCMG
			if(viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) < 0) {
				if(gVIQE_Init_State) {
					if(scaler->data->dev_opened > 0)
						scaler->data->dev_opened--;
					gVIQE_Init_State =0;
				}
				return;
			} else {
				viqe_lock = 1;
				if(gVIQE_Init_State == 0)
					TCC_VIQE_DI_Init60Hz_M2M(gOutputMode, input_image);
			}
			#endif

			if(gVIQE_Init_State == 0)
			{
				pr_err("[ERR][VIQE] %s VIQE block isn't initailized\n", __func__);
				return;
			}

			VIOC_CONFIG_Device_PlugState(viqe_common_info.gVIOC_VIQE0, &VIOC_PlugIn);
			if(VIOC_PlugIn.enable) {

			}
			else {
				pr_err("[ERR][VIQE] %s VIQE block isn't pluged!!!\n", __func__);
				return;
			}

			if(reset_frmCnt){
				atomic_set(&gFrmCnt_60Hz, 0);
				dprintk("VIQE 3D -> 2D \n");
			}

			VIOC_RDMA_SetImageBfield(main_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);
			VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, false);
			VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

			if(input_image->frameInfo_interlace)
			{
				if(atomic_read(&gFrmCnt_60Hz) >= 3)
				{
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE0, VIOC_VIQE_DEINTL_MODE_3D);
					// Process JUDDER CNTS
					JudderCnt = (input_image->Frame_width + 64)/64 -1;
					VIOC_VIQE_SetDeintlJudderCnt(viqe_common_info.pVIQE0, JudderCnt);
					gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_3D;
				}
				else
				{
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE0, VIOC_VIQE_DEINTL_MODE_2D);
					gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
				}
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE0, OFF, OFF, OFF, OFF, ON);
				VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
			}
			else
			{
				VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE0, VIOC_VIQE_DEINTL_MODE_BYPASS);
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_BYPASS;
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE0, OFF, OFF, OFF, OFF, OFF);
				VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
				atomic_set(&gFrmCnt_60Hz, 0);
			}

			TCC_VIQE_Scaler_Run60Hz_M2M(input_image);

			atomic_inc(&gFrmCnt_60Hz);
		}
	}

	#ifdef CONFIG_TCC_VIOCMG
	if(viqe_lock)
		viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
	#endif

	return;
}
EXPORT_SYMBOL(TCC_VIQE_DI_Run60Hz_M2M);

void TCC_VIQE_DI_DeInit60Hz_M2M(int layer)
{
	pr_info("[INF][VIQE] %s - Layer:%d +\n", __func__, layer);
	#ifdef PRINT_OUT_M2M_MS
	pr_info("[INF][VIQE] Time Diff(ms) :: Max(%2d.%2d)/Min(%2d.%2d) => Avg(%2d.%2d) \n", (unsigned int)(time_gap_max_us/1000), (unsigned int)(time_gap_max_us%1000),
				(unsigned int)(time_gap_min_us/1000), (unsigned int)(time_gap_min_us%1000),
				(unsigned int)((time_gap_total_us/time_gap_total_count)/1000), (unsigned int)((time_gap_total_us/time_gap_total_count)%1000));
	#endif
	if (layer == RDMA_VIDEO_SUB) {
		VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
		VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
		VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

		if (gusingDI_S) {
			VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
			tcc_sdintls_swreset();

			gusingDI_S = 0;
		}

		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		atomic_set(&gFrmCnt_Sub_60Hz, 0);

		if (gVIQE1_Init_State) {
			VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_VIQE1);
			tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE1);

			gVIQE1_Init_State = 0;
		}
		#endif
		TCC_VIQE_Scaler_Sub_Release60Hz_M2M();
	}

	if (layer == RDMA_VIDEO) {
		atomic_set(&gFrmCnt_60Hz, 0);

		VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
		VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, true);
		VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

		if (gVIQE_Init_State) {
			#ifdef CONFIG_TCC_VIOCMG
			if(viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) == 0)
			{
				VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_VIQE0);
				tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE0);

				viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
			}
			#else
			VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_VIQE0);
			tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE0);
			#endif
			gVIQE_Init_State = 0;
		}

		TCC_VIQE_Scaler_Release60Hz_M2M();
		#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
		dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
		if( (DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE ){
			pr_info("[INF][VIQE] %s - Layer:%d +\n", __func__, RDMA_VIDEO_SUB);
			VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

			TCC_VIQE_Scaler_Sub_Release60Hz_M2M();
			pr_info("[INF][VIQE] %s - Layer:%d -\n", __func__, RDMA_VIDEO_SUB);
			//DV_PROC_CHECK = 0; // will be inited tcc_scale_display_update()
			dvprintk("%s-%d : DV_PROC_CHECK(%d) \n", __func__, __LINE__, DV_PROC_CHECK);
		}
		#endif
	}
	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_OFF, NULL, layer);
	#endif

	TCC_VIQE_Scaler_DeInit_Buffer_M2M();

	pr_info("[INF][VIQE] %s - Layer:%d -\n", __func__, layer);
}
EXPORT_SYMBOL(TCC_VIQE_DI_DeInit60Hz_M2M);

void TCC_VIQE_DI_Sub_Init60Hz_M2M(TCC_OUTPUT_TYPE outputMode, struct tcc_lcdc_image_update *input_image)
{
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	int type = VSYNC_MAIN;
#endif
#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_INIT, input_image, input_image->Lcdc_layer);
#endif

	if(atomic_read(&viqe_sub_m2m_init) == 0) {
		gOutputMode = outputMode;
		gUse_sDeintls = 0;

		if(0 > TCC_VIQE_Scaler_Init_Buffer_M2M())
		{
			pr_err("[ERR][VIQE] %s-%d : memory allocation is failed.\n", __func__, __LINE__);
			goto m2m_init_scaler;
		}

		if (!input_image->deinterlace_mode)
			goto m2m_init_scaler;

		if(viqe_common_info.gBoard_stb == 1) {
			if (input_image->Lcdc_layer == RDMA_VIDEO_SUB)
				VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
			else if(input_image->Lcdc_layer == RDMA_VIDEO)
				VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
		}

		if (input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);

			tcc_sdintls_swreset();
			VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, sub_m2m_info.gDEINTLS_RDMA_num_m2m);
			gusingDI_S = 1;
			dprintk("%s - DEINTL-S\n", __func__);
		}
m2m_init_scaler:
		TCC_VIQE_Scaler_Sub_Init60Hz_M2M(input_image);
		atomic_set(&viqe_sub_m2m_init, 1);

#ifdef CONFIG_USE_SUB_MULTI_FRAME
		if(input_image->mosaic_mode) {
			for(type = VSYNC_MAIN; type < VSYNC_MAX; type++)
				memset(&sub_m2m_image[type], 0x00, sizeof(struct tcc_lcdc_image_update));
		}
#endif
	}
}
EXPORT_SYMBOL(TCC_VIQE_DI_Sub_Init60Hz_M2M);

void TCC_VIQE_DI_Sub_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image, int reset_frmCnt)
{
#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_ON, input_image, input_image->Lcdc_layer);
#endif

	if(atomic_read(&viqe_sub_m2m_init)) {
		if(Output_SelectMode == TCC_OUTPUT_NONE)
			return;

		if (!input_image->deinterlace_mode) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
		} else {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);
			if(input_image->frameInfo_interlace)
			{
				if(!gusingDI_S) {
					VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, sub_m2m_info.gDEINTLS_RDMA_num_m2m);
					gusingDI_S = 1;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			}
			else
			{
				if(gusingDI_S) {
					VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
					gusingDI_S = 0;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			}
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
		}
	}
	return;
}
EXPORT_SYMBOL(TCC_VIQE_DI_Sub_Run60Hz_M2M);

void TCC_VIQE_DI_Sub_DeInit60Hz_M2M(int layer)
{
	pr_info("[INF][VIQE] %s - Layer:%d +\n", __func__, layer);

	if(atomic_read(&viqe_sub_m2m_init)) {
		if (layer == RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, true);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

			if (gusingDI_S) {
				VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
				tcc_sdintls_swreset();

				gusingDI_S = 0;
			}

			TCC_VIQE_Scaler_Sub_Release60Hz_M2M();
		}
		TCC_VIQE_Scaler_DeInit_Buffer_M2M();

		atomic_set(&viqe_sub_m2m_init, 0);
	}

	pr_info("[INF][VIQE] %s - Layer:%d -\n", __func__, layer);
}
EXPORT_SYMBOL(TCC_VIQE_DI_Sub_DeInit60Hz_M2M);

int TCC_VIQE_Scaler_Init_Buffer_M2M(void)
{
	pmap_t pmap_info;
	unsigned int szBuffer = 0;
	int buffer_cnt = 0;

	if(0 > pmap_get_info("viqe", &pmap_info)){
		pr_err("[ERR][VIQE] %s-%d : viqe allocation is failed.\n", __func__, __LINE__);
		return -1;
	}
	gPMEM_VIQE_BASE = pmap_info.base;
	gPMEM_VIQE_SIZE = pmap_info.size;

#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	if(0 > pmap_get_info("viqe1", &pmap_info)){
		pr_err("[ERR][VIQE] %s-%d : viqe1 allocation is failed.\n", __func__, __LINE__);
		return -2;
	}
	gPMEM_VIQE_SUB_BASE = pmap_info.base;
	gPMEM_VIQE_SUB_SIZE = pmap_info.size;
	pr_info("[INF][VIQE] pmap :: 0: 0x%x/0x%x, 1: 0x%x/0x%x \n", gPMEM_VIQE_BASE, gPMEM_VIQE_SIZE, gPMEM_VIQE_SUB_BASE, gPMEM_VIQE_SUB_SIZE);
#else
	pr_info("[INF][VIQE] pmap :: 0: 0x%x/0x%x, 1: 0x%x/0x%x \n", gPMEM_VIQE_BASE, gPMEM_VIQE_SIZE, 0, 0);
#endif

	if(0 > pmap_get_info("overlay", &pmap_info)){
		pr_err("[ERR][VIQE] %s-%d : overlay allocation is failed.\n", __func__, __LINE__);
		return -3;
	}

	if(pmap_info.size)
	{
		szBuffer = pmap_info.size / BUFFER_CNT_FOR_M2M_MODE;
		
		for(buffer_cnt=0; buffer_cnt < BUFFER_CNT_FOR_M2M_MODE; buffer_cnt++)
		{
			overlay_buffer[0][buffer_cnt] = (unsigned int)pmap_info.base + (szBuffer * buffer_cnt);
			pr_info("[INF][VIQE] Overlay-0 :: %d/%d = 0x%x  \n", buffer_cnt, BUFFER_CNT_FOR_M2M_MODE, overlay_buffer[0][buffer_cnt]);
		}
	}

	pmap_get_info("overlay1", &pmap_info);
	if(0 > pmap_get_info("overlay1", &pmap_info)){
		pr_err("[ERR][VIQE] %s-%d : overlay1 allocation is failed.\n", __func__, __LINE__);
		return -4;
	}
	if(pmap_info.size)
	{
		szBuffer = pmap_info.size / BUFFER_CNT_FOR_M2M_MODE;

		for(buffer_cnt=0; buffer_cnt < BUFFER_CNT_FOR_M2M_MODE; buffer_cnt++)
		{
			overlay_buffer[1][buffer_cnt] = (unsigned int)pmap_info.base + (szBuffer * buffer_cnt);
			pr_info("[INF][VIQE] Overlay-1 :: %d/%d = 0x%x  \n", buffer_cnt, BUFFER_CNT_FOR_M2M_MODE, overlay_buffer[1][buffer_cnt]);
		}
	}

	current_buffer_idx[0] = 0;
	current_buffer_idx[1] = 0;

	return 0;
}

void TCC_VIQE_Scaler_DeInit_Buffer_M2M(void)
{
	pmap_release_info("viqe");
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	pmap_release_info("viqe1");
#endif
	pmap_release_info("overlay");
	pmap_release_info("overlay1");
}

unsigned int TCC_VIQE_Scaler_Get_Buffer_M2M(struct tcc_lcdc_image_update* input_image)
{
	unsigned int buffer_addr = 0;

	if(input_image->Lcdc_layer != RDMA_VIDEO)
		return 0x00;

	buffer_addr = overlay_buffer[0][current_buffer_idx[0]];
	current_buffer_idx[0] += 1;
	if(current_buffer_idx[0] >= BUFFER_CNT_FOR_M2M_MODE)
		current_buffer_idx[0] = 0;

	return buffer_addr;
}

unsigned int TCC_VIQE_Scaler_Sub_Get_Buffer_M2M(struct tcc_lcdc_image_update* input_image)
{
	unsigned int buffer_addr = 0;

	if(input_image->Lcdc_layer != RDMA_VIDEO_SUB)
		return 0x00;

	buffer_addr = overlay_buffer[1][current_buffer_idx[1]];
	current_buffer_idx[1] += 1;
	if(current_buffer_idx[1] >= BUFFER_CNT_FOR_M2M_MODE)
		current_buffer_idx[1] = 0;

	return buffer_addr;
}

void TCC_VIQE_Scaler_Init60Hz_M2M(struct tcc_lcdc_image_update *input_image)
{
    int ret = 0;

    if (!scaler->data->irq_reged) {
		// VIOC_CONFIG_StopRequest(1);
		synchronize_irq(scaler->irq);
		vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);
		ret = request_irq(scaler->irq, TCC_VIQE_Scaler_Handler60Hz_M2M, IRQF_SHARED, "m2m-main", scaler);

		if (ret) {
			pr_err("[ERR][VIQE] failed to acquire scaler1 request_irq. \n");
		}

		vioc_intr_enable(scaler->irq, scaler->vioc_intr->id, scaler->vioc_intr->bits);
		scaler->data->irq_reged = 1;
		pr_info("[INF][VIQE] %s() : Out - M2M-Main ISR(%d) registered(%d). \n", __func__, scaler->irq, scaler->data->irq_reged);

#ifdef CONFIG_VIOC_MAP_DECOMP
		if(!gUse_MapConverter && input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0){
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter |= MAIN_USED;
			pr_info("[INF][VIQE] Main-M2M :: MC Used. \n");
		}
#endif
#ifdef CONFIG_VIOC_DTRC
		if(!gUse_DtrcConverter && input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0){
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter |= MAIN_USED;
			pr_info("[INF][VIQE] Main-M2M :: DTRC Used. \n");
		}
#endif

    }

    scaler->data->dev_opened++;

	current_buffer_idx[0] = 0;

    dvprintk("%s() : Out - open(%d). \n", __func__, scaler->data->dev_opened);
}

void TCC_VIQE_DI_GET_SOURCE_INFO(struct tcc_lcdc_image_update* input_image, unsigned int layer)
{
	if(layer == RDMA_VIDEO)
		memcpy((struct tcc_lcdc_image_update*)input_image, scaler->info, sizeof(struct tcc_lcdc_image_update));
	else
		memcpy((struct tcc_lcdc_image_update*)input_image, &scaler_sub->info, sizeof(struct tcc_lcdc_image_update));
}
EXPORT_SYMBOL(TCC_VIQE_DI_GET_SOURCE_INFO);

void TCC_VIQE_Scaler_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image)
{
    unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
    unsigned int crop_width;

	unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUYV;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_COMP;

#ifndef CONFIG_OF_VIOC
    volatile void __iomem *pSC_RDMABase = scaler->rdma->reg;
    volatile void __iomem *pSC_WMIXBase = scaler->wmix->reg;
    volatile void __iomem *pSC_WDMABase = scaler->wdma->reg;
	volatile void __iomem *pSC_SCALERBase = scaler->sc->reg;
#endif

	dvprintk("%s() : IN.\n", __func__);

	memcpy(scaler->info, (struct tcc_lcdc_image_update*)input_image, sizeof(struct tcc_lcdc_image_update));
	memcpy(&output_m2m_image, (struct tcc_lcdc_image_update*)input_image, sizeof(struct tcc_lcdc_image_update));
	#ifdef CONFIG_VIOC_MAP_DECOMP
	output_m2m_image.private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
	output_m2m_image.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0;
	#endif
	#ifdef CONFIG_VIOC_DTRC
	output_m2m_image.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
	output_m2m_image.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0;
	#endif

	dprintk("%s : %d(%d), %d(%d), intl(%d/%d), addr(0x%x)\n",__func__, output_m2m_image.buffer_unique_id, output_m2m_image.odd_first_flag,
			input_image->buffer_unique_id, input_image->odd_first_flag,  input_image->frameInfo_interlace, input_image->deinterlace_mode,
			input_image->addr0);
    crop_width = scaler->info->crop_right - scaler->info->crop_left;

	/* For Test
	 * Please check it - inbest
	 */
	scaler->info->offset_x = (scaler->info->offset_x >> 1) << 1;
	scaler->info->offset_y = (scaler->info->offset_y >> 1) << 1;
	scaler->info->Image_width = (scaler->info->Image_width >> 1) << 1;
	scaler->info->Image_height = (scaler->info->Image_height >> 1) << 1;

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if(!gUse_MapConverter && input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0){
		//tca_map_convter_swreset(VIOC_MC1);
		gUse_MapConverter |= MAIN_USED;
		pr_info("[INF][VIQE] Main-M2M :: MC Used. \n");
	}

	if(scaler->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0 && !(gUse_MapConverter&MAIN_USED)){
		pr_err("[ERR][VIQE] Main Map-Converter can't work because of gUse_MapConverter(0x%x) \n", gUse_MapConverter);
		return;
	}
	#endif
	#ifdef CONFIG_VIOC_DTRC
	if(!gUse_DtrcConverter && input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0){
		//tca_dtrc_convter_swreset(VIOC_DTRC1);
		gUse_DtrcConverter |= MAIN_USED;
		pr_info("[INF][VIQE] Main-M2M :: DTRC Used. \n");
	}

	if(scaler->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0 && !(gUse_DtrcConverter&MAIN_USED)){
		pr_err("[ERR][VIQE] Dtrc-Converter can't work because of gUse_DtrcConverter(0x%x) \n", gUse_DtrcConverter);
		return;
	}
	#endif

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if( (gUse_MapConverter&MAIN_USED) && scaler->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
	{
		dprintk("%s : Map-Converter Operation %d \n", __func__, scaler->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO]);

		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if(scaler->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler->rdma->id))
		{
			// rdma
			if(__raw_readl(pSC_RDMABase+RDMACTRL) & RDMACTRL_IEN_MASK)
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
			VIOC_CONFIG_PlugIn(scaler->sc->id, scaler->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if (VIOC_CONFIG_DMAPath_Support()) {
			VIOC_CONFIG_DMAPath_UnSet(scaler->rdma->id);
			VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, VIOC_MC1);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(scaler->wmix->id, VIOC_MC1);
			#endif
		}

		tca_map_convter_set(VIOC_MC1, scaler->info, 0);
	}
	else
	#endif
	#ifdef CONFIG_VIOC_DTRC
	if( (gUse_DtrcConverter&MAIN_USED) && scaler->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0)
	{
		dprintk("%s : Dtrc-Converter Operation %d \n", __func__, scaler->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO]);

		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if(scaler->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler->rdma->id))
		{
			// rdma
			if(__raw_readl(pSC_RDMABase+RDMACTRL) & RDMACTRL_IEN_MASK)
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
			VIOC_CONFIG_PlugIn(scaler->sc->id, scaler->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if (VIOC_CONFIG_DMAPath_Support()) {
			VIOC_CONFIG_DMAPath_UnSet(scaler->rdma->id);
			VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, VIOC_DTRC1);
		}

		tca_dtrc_convter_set(VIOC_DTRC1, scaler->info, 0);
	}
	else
	#endif
	{
	#ifdef CONFIG_VIOC_MAP_DECOMP
		if(gUse_MapConverter&MAIN_USED){
			if (VIOC_CONFIG_DMAPath_Support()) {
				VIOC_CONFIG_DMAPath_UnSet(VIOC_MC1);
				VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			} else {
				#ifdef CONFIG_ARCH_TCC803X
				VIOC_CONFIG_MCPath(scaler->wmix->id, scaler->rdma->id);
				#endif
			}
			gUse_MapConverter &= ~(MAIN_USED);
		}
	#endif//
	#ifdef CONFIG_VIOC_DTRC
		if(gUse_DtrcConverter&MAIN_USED){
			if (VIOC_CONFIG_DMAPath_Support()) {
				VIOC_CONFIG_DMAPath_UnSet(VIOC_DTRC1);
				VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			}
			gUse_DtrcConverter &= ~(MAIN_USED);
		}
	#endif//
		dprintk("%s : Normal Operation %d, RDMA(0x%p/0x%p) \n", __func__, scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH],
					pSC_RDMABase, VIOC_RDMA_GetAddress(scaler->rdma->id));
		if (scaler->settop_support) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		}
		VIOC_RDMA_SetImageFormat(pSC_RDMABase, scaler->info->fmt);

		//interlaced frame process ex) MPEG2
		if (0) {//(scaler->info->interlaced) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top)/2);
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width*2);
		}
		#ifdef CONFIG_VIOC_10BIT
		else if(scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10){
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width * 2);
		} 
		else if(scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width*125/100);
		}
		#endif//
		else {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width);
		}

		pSrcBase0 = (unsigned int)scaler->info->addr0;
		pSrcBase1 = (unsigned int)scaler->info->addr1;
		pSrcBase2 = (unsigned int)scaler->info->addr2;
		if (scaler->info->fmt >= VIOC_IMG_FMT_444SEP) { // address limitation!!
			dprintk("%s():  src addr is not allocate. \n", __func__);
			scaler->info->crop_left  = (scaler->info->crop_left>>3)<<3;
			scaler->info->crop_right = scaler->info->crop_left + crop_width;
			scaler->info->crop_right = scaler->info->crop_left + (scaler->info->crop_right - scaler->info->crop_left);
			tccxxx_GetAddress(scaler->info->fmt, (unsigned int)scaler->info->addr0,                 \
								scaler->info->Frame_width, scaler->info->Frame_height,        \
								scaler->info->crop_left, scaler->info->crop_top,            \
								&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
		VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);

		#ifdef CONFIG_VIOC_10BIT
		if(scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);	/* YUV 10bit support */
		else if(scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);	/* YUV real 10bit support */
		else
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);	/* YUV 8bit support */
		#endif

		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if(scaler->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler->rdma->id))
		{
			// rdma
			if(__raw_readl(pSC_RDMABase+RDMACTRL) & RDMACTRL_IEN_MASK)
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
			VIOC_CONFIG_PlugIn(scaler->sc->id, scaler->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	}

	VIOC_CONFIG_WMIXPath(scaler->rdma->id, 1); // wmixer op is mixing mode.
	VIOC_WMIX_SetSize(pSC_WMIXBase, scaler->info->Image_width, scaler->info->Image_height);
	VIOC_WMIX_SetUpdate(pSC_WMIXBase);

	VIOC_WDMA_SetImageFormat(pSC_WDMABase, dest_fmt);
	VIOC_WDMA_SetImageSize(pSC_WDMABase, scaler->info->Image_width, scaler->info->Image_height);
	VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, scaler->info->Image_width);
	if(scaler->info->dst_addr0 == 0)
		scaler->info->dst_addr0 = TCC_VIQE_Scaler_Get_Buffer_M2M(input_image);
	VIOC_WDMA_SetImageBase(pSC_WDMABase, (unsigned int)scaler->info->dst_addr0, (unsigned int)scaler->info->dst_addr1, (unsigned int)scaler->info->dst_addr2);

	VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK); // wdma status register all clear.

	output_m2m_image.Frame_width = scaler->info->Image_width;
	output_m2m_image.Frame_height = scaler->info->Image_height;
	output_m2m_image.Image_width = scaler->info->Image_width;
	output_m2m_image.Image_height = scaler->info->Image_height;
	output_m2m_image.crop_left= 0;
	output_m2m_image.crop_top= 0;
	output_m2m_image.crop_right= scaler->info->Image_width;
	output_m2m_image.crop_bottom= scaler->info->Image_height;
	output_m2m_image.fmt = dest_fmt;
	output_m2m_image.addr0 = scaler->info->dst_addr0;
	output_m2m_image.addr1 = scaler->info->dst_addr1;
	output_m2m_image.addr2 = scaler->info->dst_addr2;

	#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
	if( (DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE ){
		struct tcc_lcdc_image_update el_ImageInfo;
		unsigned int ratio = 1;

		if(0 <= tca_edr_el_configure(input_image, &el_ImageInfo, &ratio))
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(&el_ImageInfo);
	}
	#endif

	dvprintk("%s() : OUT.\n", __func__);
}

void TCC_VIQE_Scaler_Release60Hz_M2M(void)
{
	dprintk("%s():  In -open(%d), irq(%d) \n", __func__, scaler->data->dev_opened, scaler->data->irq_reged);

	if (scaler->data->dev_opened > 0) {
		scaler->data->dev_opened--;
	}

	if (scaler->data->dev_opened == 0) {
		if (scaler->data->irq_reged) {
			vioc_intr_disable(scaler->irq, scaler->vioc_intr->id, scaler->vioc_intr->bits);
			free_irq(scaler->irq, scaler);
			vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);
			scaler->data->irq_reged = 0;
		}

	#ifdef CONFIG_VIOC_MAP_DECOMP
		if(gUse_MapConverter&MAIN_USED){
			if(VIOC_CONFIG_DMAPath_Support()) {
				VIOC_CONFIG_DMAPath_UnSet(VIOC_MC1);
				VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			} else {
				#ifdef CONFIG_ARCH_TCC803X
				VIOC_CONFIG_MCPath(scaler->wmix->id, scaler->rdma->id);
				#endif
			}
		}
	#endif//
	#ifdef CONFIG_VIOC_DTRC
		if(gUse_DtrcConverter&MAIN_USED){
			if(VIOC_CONFIG_DMAPath_Support()) {
				VIOC_CONFIG_DMAPath_UnSet(VIOC_DTRC1);
				VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			}
		}
	#endif//
		if(VIOC_CONFIG_DMAPath_Support()) {
			VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(scaler->wmix->id, scaler->rdma->id);
			#endif
		}
	
		VIOC_CONFIG_PlugOut(scaler->sc->id);

		VIOC_CONFIG_SWReset(scaler->wdma->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->wmix->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->sc->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma->id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler->rdma->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wmix->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wdma->id, VIOC_CONFIG_CLEAR);

	#ifdef CONFIG_VIOC_MAP_DECOMP
		if(gUse_MapConverter&MAIN_USED){
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter &= ~(MAIN_USED);
		}
	#endif
	#ifdef CONFIG_VIOC_DTRC
		if(gUse_DtrcConverter&MAIN_USED){
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter &= ~(MAIN_USED);
		}
	#endif	
	}

	dprintk("%s() : Out - open(%d). \n", __func__, scaler->data->dev_opened);
}

void TCC_VIQE_Scaler_Sub_Init60Hz_M2M(struct tcc_lcdc_image_update *input_image)
{
	int ret = 0;

	if (!scaler_sub->data->irq_reged) {
		// VIOC_CONFIG_StopRequest(1);
		synchronize_irq(scaler_sub->irq);
		vioc_intr_clear(scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
		ret = request_irq(scaler_sub->irq, TCC_VIQE_Scaler_Sub_Handler60Hz_M2M, IRQF_SHARED, "m2m-sub", scaler_sub);

		if (ret) printk("failed to acquire scaler3 request_irq. \n");

		vioc_intr_enable(scaler_sub->irq, scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
		scaler_sub->data->irq_reged = 1;
		pr_info("[INF][VIQE] %s() : Out - M2M-Sub ISR(%d) registered(%d). \n", __func__, scaler_sub->irq, scaler_sub->data->irq_reged);

#ifdef CONFIG_VIOC_MAP_DECOMP
		if(!gUse_MapConverter && input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0){
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter |= SUB_USED;
			pr_info("[INF][VIQE] Sub-M2M :: MC Used. \n");
		}
#endif
#ifdef CONFIG_VIOC_DTRC
		if(!gUse_DtrcConverter && input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0){
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter |= SUB_USED;
			pr_info("[INF][VIQE] Sub-M2M :: DTRC Used. \n");
		}
#endif
	}

	scaler_sub->data->dev_opened++;

	current_buffer_idx[1] = 0;

	dvprintk("%s() : Out - open(%d). \n", __func__, scaler_sub->data->dev_opened);
}

#ifdef CONFIG_USE_SUB_MULTI_FRAME
void TCC_VIQE_Scaler_Sub_Repeat60Hz_M2M(struct tcc_lcdc_image_update *input_image)
{
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int crop_width;
	unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUYV;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_COMP;

#ifndef CONFIG_OF_VIOC
	volatile void __iomem *pSC_RDMABase = scaler_sub->rdma->reg;
	//    volatile void __iomem *pSC_WMIXBase = scaler_sub->wmix->reg;
	volatile void __iomem *pSC_WDMABase = scaler_sub->wdma->reg;
	volatile void __iomem *pSC_SCALERBase = scaler_sub->sc->reg;
#endif
	dvprintk("%s() : IN.\n", __func__);

	if(input_image->mosaic_mode) {
		if (scaler_sub->settop_support) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		}
		VIOC_RDMA_SetImageFormat(pSC_RDMABase, input_image->fmt);

#ifdef CONFIG_VIOC_10BIT
		if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
		{
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (input_image->crop_right - input_image->crop_left), (input_image->crop_bottom - input_image->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, input_image->fmt, input_image->Frame_width * 2);
		}
		else if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
		{
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (input_image->crop_right - input_image->crop_left), (input_image->crop_bottom - input_image->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, input_image->fmt, input_image->Frame_width * 125 / 100);
		}
#endif//
		else {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (input_image->crop_right - input_image->crop_left), (input_image->crop_bottom - input_image->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, input_image->fmt, input_image->Frame_width);
		}

		pSrcBase0 = (unsigned int)input_image->addr0;
		pSrcBase1 = (unsigned int)input_image->addr1;
		pSrcBase2 = (unsigned int)input_image->addr2;
		if (input_image->fmt >= VIOC_IMG_FMT_444SEP) { // address limitation!!
			dprintk("%s():  src addr is not allocate. \n", __func__);
			input_image->crop_left       = (input_image->crop_left>>3)<<3;
			input_image->crop_right = input_image->crop_left + crop_width;
			input_image->crop_right = input_image->crop_left + (input_image->crop_right - input_image->crop_left);
			tccxxx_GetAddress(input_image->fmt, (unsigned int)input_image->addr0,                 \
					input_image->Frame_width, input_image->Frame_height,        \
					input_image->crop_left, input_image->crop_top,            \
					&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
		VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);

#ifdef CONFIG_VIOC_10BIT
		if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);	/* YUV 10bit support */
		else if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);	/* YUV real 10bit support */
		else
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);	/* YUV 8bit support */
#endif

		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, input_image->buffer_Image_width, input_image->buffer_Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, input_image->buffer_Image_width, input_image->buffer_Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		VIOC_CONFIG_PlugIn(scaler_sub->sc->id, scaler_sub->rdma->id);
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.

		VIOC_CONFIG_WMIXPath(scaler_sub->rdma->id, 0); // wmixer op is bypass mode.

		VIOC_WDMA_SetImageFormat(pSC_WDMABase, dest_fmt);
		VIOC_WDMA_SetImageSize(pSC_WDMABase, input_image->buffer_Image_width, input_image->buffer_Image_height);
		VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, input_image->Image_width);

		input_image->dst_addr0 = scaler_sub->info->dst_addr0;
		if(input_image->dst_addr0) {
			unsigned int pDstBase0 = 0, pDstBase1 = 0, pDstBase2 = 0;
			tccxxx_GetAddress(dest_fmt, (unsigned int)input_image->dst_addr0,
					input_image->Image_width, input_image->Image_height,
					((input_image->buffer_offset_x - input_image->offset_x) >> 1) << 1, ((input_image->buffer_offset_y - input_image->offset_y) >> 1) << 1,
					&pDstBase0, &pDstBase1, &pDstBase2);
			VIOC_WDMA_SetImageBase(pSC_WDMABase, pDstBase0, pDstBase1, pDstBase2);
		}
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
		VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
		VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK); // wdma status register all clear.
	}

	dvprintk("%s() : OUT.\n", __func__);
}
#endif

void TCC_VIQE_Scaler_Sub_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image)
{
    unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
    unsigned int crop_width;
	unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUYV;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_COMP;

#ifndef CONFIG_OF_VIOC
    volatile void __iomem *pSC_RDMABase = scaler_sub->rdma->reg;
//    volatile void __iomem *pSC_WMIXBase = scaler_sub->wmix->reg;
    volatile void __iomem *pSC_WDMABase = scaler_sub->wdma->reg;
	volatile void __iomem *pSC_SCALERBase = scaler_sub->sc->reg;
#endif

	dvprintk("%s() : IN.\n", __func__);

	memcpy(scaler_sub->info, (struct tcc_lcdc_image_update*)input_image, sizeof(struct tcc_lcdc_image_update));
	memcpy(&output_m2m_image_sub, (struct tcc_lcdc_image_update*)input_image, sizeof(struct tcc_lcdc_image_update));
#ifdef CONFIG_VIOC_MAP_DECOMP
	output_m2m_image_sub.private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
	output_m2m_image_sub.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0;
#endif
#ifdef CONFIG_VIOC_DTRC
	output_m2m_image_sub.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
	output_m2m_image_sub.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0;
#endif

	dvprintk("%s : %d(%d), %d(%d), intl(%d/%d)\n",__func__, output_m2m_image_sub.buffer_unique_id, output_m2m_image_sub.odd_first_flag, input_image->buffer_unique_id,
			input_image->odd_first_flag,  input_image->frameInfo_interlace, input_image->deinterlace_mode);
    crop_width = scaler_sub->info->crop_right - scaler_sub->info->crop_left;

#ifdef CONFIG_VIOC_MAP_DECOMP
	if(!gUse_MapConverter && input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0){
		//tca_map_convter_swreset(VIOC_MC1);
		gUse_MapConverter |= SUB_USED;
		pr_info("[INF][VIQE] Sub-M2M :: MC Used. \n");
	}

	if(scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0 && !(gUse_MapConverter&SUB_USED)){
		pr_err("[ERR][VIQE] Sub Map-Converter can't work because of gUse_MapConverter(0x%x) \n", gUse_MapConverter);
		return;
	}
#endif
#ifdef CONFIG_VIOC_DTRC
	if(!gUse_DtrcConverter && input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0){
		//tca_dtrc_convter_swreset(VIOC_DTRC1);
		gUse_DtrcConverter |= SUB_USED;
		pr_info("[INF][VIQE] Sub-M2M :: DTRC Used. \n");
	}

	if(scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0 && !(gUse_DtrcConverter&SUB_USED)){
		pr_err("[ERR][VIQE] Dtrc-Converter can't work because of gUse_MapConverter(0x%x) \n", gUse_DtrcConverter);
		return;
	}
#endif
#ifdef CONFIG_VIOC_MAP_DECOMP
	if((gUse_MapConverter&SUB_USED) && scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
	{

		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if(scaler_sub->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler_sub->rdma->id))
		{
			// rdma
			if(__raw_readl(pSC_RDMABase+RDMACTRL) & RDMACTRL_IEN_MASK)
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
			VIOC_CONFIG_PlugIn(scaler_sub->sc->id, scaler_sub->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if(VIOC_CONFIG_DMAPath_Support()) {
           VIOC_CONFIG_DMAPath_UnSet(scaler_sub->rdma->id);
           VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, VIOC_MC1);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(scaler_sub->wmix->id, VIOC_MC1);
			#endif
		}

		tca_map_convter_set(VIOC_MC1, scaler_sub->info, 0);
	}
	else
#endif
#ifdef CONFIG_VIOC_DTRC
	if((gUse_DtrcConverter&SUB_USED) && scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0)
	{

		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if(scaler_sub->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler_sub->rdma->id))
		{
			// rdma
			if(__raw_readl(pSC_RDMABase+RDMACTRL) & RDMACTRL_IEN_MASK)
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
			VIOC_CONFIG_PlugIn(scaler_sub->sc->id, scaler_sub->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if(VIOC_CONFIG_DMAPath_Support()) {
			VIOC_CONFIG_DMAPath_UnSet(scaler_sub->rdma->id);
			VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, VIOC_DTRC1);
		}

		tca_dtrc_convter_set(VIOC_DTRC1, scaler_sub->info, 0);
	}
	else
#endif
	{
#ifdef CONFIG_VIOC_MAP_DECOMP
		if(gUse_MapConverter&SUB_USED){
			if(VIOC_CONFIG_DMAPath_Support()) {
				 VIOC_CONFIG_DMAPath_UnSet(VIOC_MC1);
				 VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			} else {
				#ifdef CONFIG_ARCH_TCC803X
				VIOC_CONFIG_MCPath(scaler_sub->wmix->id, scaler_sub->rdma->id);
				#endif
			}
			gUse_MapConverter &= ~(SUB_USED);
		}
#endif//
#ifdef CONFIG_VIOC_DTRC
		if(gUse_DtrcConverter&SUB_USED){
			if(VIOC_CONFIG_DMAPath_Support()) {
				 VIOC_CONFIG_DMAPath_UnSet(VIOC_DTRC1);
				 VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			}
			gUse_DtrcConverter &= ~(SUB_USED);
		}
#endif//
		if(VIOC_CONFIG_DMAPath_Support())
			VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
	
		if (scaler_sub->settop_support) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		}
		VIOC_RDMA_SetImageFormat(pSC_RDMABase, scaler_sub->info->fmt);

		//interlaced frame process ex) MPEG2
		if (0) {//(scaler_sub->info->interlaced) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top)/2);
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width*2);
		}
#ifdef CONFIG_VIOC_10BIT
		else if(scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
		{
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width * 2);
		}
		else if(scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
		{
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width * 125 / 100);
		}
#endif//
		else {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width);
		}

		pSrcBase0 = (unsigned int)scaler_sub->info->addr0;
		pSrcBase1 = (unsigned int)scaler_sub->info->addr1;
		pSrcBase2 = (unsigned int)scaler_sub->info->addr2;
		if (scaler_sub->info->fmt >= VIOC_IMG_FMT_444SEP) { // address limitation!!
			dprintk("%s():  src addr is not allocate. \n", __func__);
			scaler_sub->info->crop_left       = (scaler_sub->info->crop_left>>3)<<3;
			scaler_sub->info->crop_right = scaler_sub->info->crop_left + crop_width;
			scaler_sub->info->crop_right = scaler_sub->info->crop_left + (scaler_sub->info->crop_right - scaler_sub->info->crop_left);
			tccxxx_GetAddress(scaler_sub->info->fmt, (unsigned int)scaler_sub->info->addr0,                 \
								scaler_sub->info->Frame_width, scaler_sub->info->Frame_height,        \
								scaler_sub->info->crop_left, scaler_sub->info->crop_top,            \
								&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
		VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);

#ifdef CONFIG_VIOC_10BIT
		if(scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);	/* YUV 10bit support */
		else if(scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);	/* YUV real 10bit support */
		else
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);	/* YUV 8bit support */
#endif

		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
#ifdef CONFIG_USE_SUB_MULTI_FRAME
		if(scaler_sub->info->mosaic_mode) {
			VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->buffer_Image_width, scaler_sub->info->buffer_Image_height);
			VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->buffer_Image_width, scaler_sub->info->buffer_Image_height);
		}
		else
#endif
		{
			VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
			VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		}
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if(scaler_sub->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler_sub->rdma->id))
		{
			// rdma
			if(__raw_readl(pSC_RDMABase+RDMACTRL) & RDMACTRL_IEN_MASK)
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
			VIOC_CONFIG_PlugIn(scaler_sub->sc->id, scaler_sub->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	}
#if 0
    VIOC_CONFIG_WMIXPath(scaler_sub->rdma->id, 1); // wmixer op is mixing mode.
    VIOC_WMIX_SetSize(pSC_WMIXBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
    VIOC_WMIX_SetUpdate(pSC_WMIXBase);
#else
    VIOC_CONFIG_WMIXPath(scaler_sub->rdma->id, 0); // wmixer op is bypass mode.
#endif

    VIOC_WDMA_SetImageFormat(pSC_WDMABase, dest_fmt);
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	if(scaler_sub->info->mosaic_mode) {
		VIOC_WDMA_SetImageSize(pSC_WDMABase, scaler_sub->info->buffer_Image_width, scaler_sub->info->buffer_Image_height);
		VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, scaler_sub->info->Image_width);
		if(scaler_sub->info->dst_addr0 == 0)
			scaler_sub->info->dst_addr0 = TCC_VIQE_Scaler_Sub_Get_Buffer_M2M(input_image);
		if(scaler_sub->info->dst_addr0) {
			unsigned int pDstBase0 = 0, pDstBase1 = 0, pDstBase2 = 0;
			tccxxx_GetAddress(dest_fmt, (unsigned int)scaler_sub->info->dst_addr0,
					scaler_sub->info->Image_width, scaler_sub->info->Image_height,
					((scaler_sub->info->buffer_offset_x - scaler_sub->info->offset_x) >> 1) << 1, ((scaler_sub->info->buffer_offset_y - scaler_sub->info->offset_y) >> 1) << 1,
					&pDstBase0, &pDstBase1, &pDstBase2);
			VIOC_WDMA_SetImageBase(pSC_WDMABase, pDstBase0, pDstBase1, pDstBase2);
			video_queue_delete_all();
		}
	}
	else
#endif
	{
		VIOC_WDMA_SetImageSize(pSC_WDMABase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, scaler_sub->info->Image_width);
		if(scaler_sub->info->dst_addr0 == 0)
			scaler_sub->info->dst_addr0 = TCC_VIQE_Scaler_Sub_Get_Buffer_M2M(input_image);
		VIOC_WDMA_SetImageBase(pSC_WDMABase, (unsigned int)scaler_sub->info->dst_addr0, (unsigned int)scaler_sub->info->dst_addr1, (unsigned int)scaler_sub->info->dst_addr2);
	}

	VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK); // wdma status register all clear.

	output_m2m_image_sub.Frame_width = scaler_sub->info->Image_width;
	output_m2m_image_sub.Frame_height = scaler_sub->info->Image_height;
	output_m2m_image_sub.Image_width = scaler_sub->info->Image_width;
	output_m2m_image_sub.Image_height = scaler_sub->info->Image_height;
	output_m2m_image_sub.crop_left= 0;
	output_m2m_image_sub.crop_top= 0;
	output_m2m_image_sub.crop_right= scaler_sub->info->Image_width;
	output_m2m_image_sub.crop_bottom= scaler_sub->info->Image_height;
	output_m2m_image_sub.fmt = dest_fmt;
	output_m2m_image_sub.addr0 = scaler_sub->info->dst_addr0;
	output_m2m_image_sub.addr1 = scaler_sub->info->dst_addr1;
	output_m2m_image_sub.addr2 = scaler_sub->info->dst_addr2;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
	if( (DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE )
	{
		output_m2m_image.private_data.dolbyVision_info.el_offset[0] 	= output_m2m_image_sub.addr0;
		output_m2m_image.private_data.dolbyVision_info.el_buffer_width 	= output_m2m_image_sub.Frame_width;
		output_m2m_image.private_data.dolbyVision_info.el_buffer_height = output_m2m_image_sub.Frame_height;
		output_m2m_image.private_data.dolbyVision_info.el_frame_width 	= (output_m2m_image_sub.crop_right-output_m2m_image_sub.crop_left);
		output_m2m_image.private_data.dolbyVision_info.el_frame_height 	= (output_m2m_image_sub.crop_bottom-output_m2m_image_sub.crop_top);
	}
#endif

	dvprintk("%s() : OUT.\n", __func__);
}

void TCC_VIQE_Scaler_Sub_Release60Hz_M2M(void)
{
	dprintk("%s():  In -open(%d), irq(%d) \n", __func__, scaler_sub->data->dev_opened, scaler_sub->data->irq_reged);

	if (scaler_sub->data->dev_opened > 0) {
		scaler_sub->data->dev_opened--;
	}

	if (scaler_sub->data->dev_opened == 0) {
		if (scaler_sub->data->irq_reged) {
			vioc_intr_disable(scaler_sub->irq, scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
			free_irq(scaler_sub->irq, scaler_sub);
			vioc_intr_clear(scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
			scaler_sub->data->irq_reged = 0;
		}

#ifdef CONFIG_VIOC_MAP_DECOMP
		if(gUse_MapConverter&SUB_USED){
			if(VIOC_CONFIG_DMAPath_Support()) {
				VIOC_CONFIG_DMAPath_UnSet(VIOC_MC1);
				VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			} else {
				#ifdef CONFIG_ARCH_TCC803X
				VIOC_CONFIG_MCPath(scaler_sub->wmix->id, scaler_sub->rdma->id);
				#endif
			}
		}
#endif//
#ifdef CONFIG_VIOC_DTRC
		if(gUse_DtrcConverter&SUB_USED){
			if(VIOC_CONFIG_DMAPath_Support()) {
				VIOC_CONFIG_DMAPath_UnSet(VIOC_DTRC1);
				VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			}
		}
#endif//
		if(VIOC_CONFIG_DMAPath_Support())
           VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
	
		VIOC_CONFIG_PlugOut(scaler_sub->sc->id);

		VIOC_CONFIG_SWReset(scaler_sub->wdma->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_sub->wmix->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_sub->sc->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_sub->rdma->id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler_sub->rdma->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler_sub->sc->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler_sub->wmix->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler_sub->wdma->id, VIOC_CONFIG_CLEAR);

#ifdef CONFIG_VIOC_MAP_DECOMP
		if(gUse_MapConverter&SUB_USED){
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter &= ~(SUB_USED);
		}
#endif
#ifdef CONFIG_VIOC_DTRC
		if(gUse_DtrcConverter&SUB_USED){
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter &= ~(SUB_USED);
		}
#endif

	}

	dprintk("%s() : Out - open(%d). \n", __func__, scaler_sub->data->dev_opened);
}

irqreturn_t TCC_VIQE_Scaler_Handler60Hz_M2M(int irq, void *client_data)
{
#ifndef CONFIG_OF_VIOC
    volatile void __iomem *pSC_WDMABase = scaler->wdma->reg;
#endif

    if (is_vioc_intr_activatied(scaler->vioc_intr->id, scaler->vioc_intr->bits) == false){
		//dprintk("%s-%d \n", __func__, __LINE__);
		return IRQ_NONE;
    }

    if((__raw_readl(pSC_WDMABase + WDMAIRQSTS_OFFSET)) & VIOC_WDMA_IREQ_EOFR_MASK) {
        dprintk("Main WDMA Interrupt is VIOC_WDMA_IREQ_EOFR_MASK. \n");
		//vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);
		VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK); // wdma status register all clear.

#if !defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
		if(tcc_vsync_isVsyncRunning(VSYNC_MAIN))
#endif
		{
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
			if( (DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE )
			{
				DV_PROC_CHECK |= DV_MAIN_DONE;
				dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
				if( (DV_PROC_CHECK & DV_ALL_DONE) == DV_ALL_DONE )
					TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image);
			}
			else
#endif
			{
				TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image);
			}
	    }
    }

#ifdef PRINT_OUT_M2M_MS
	tcc_GetTimediff_ms(t1);
#endif

	return IRQ_HANDLED;
}

irqreturn_t TCC_VIQE_Scaler_Sub_Handler60Hz_M2M(int irq, void *client_data)
{
#ifndef CONFIG_OF_VIOC
	volatile void __iomem *pSC_WDMABase = scaler_sub->wdma->reg;
#endif

	if (is_vioc_intr_activatied(scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits) == false)
		return IRQ_NONE;

	if((__raw_readl(pSC_WDMABase + WDMAIRQSTS_OFFSET)) & VIOC_WDMA_IREQ_EOFR_MASK) {
		dprintk("Sub WDMA Interrupt is VIOC_WDMA_IREQ_EOFR_MASK.  VSync_Run(%d)\n", tcc_vsync_isVsyncRunning(VSYNC_SUB0));
		//vioc_intr_clear(scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
		VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK); // wdma status register all clear.

		if(tcc_vsync_isVsyncRunning(VSYNC_SUB0))
		{
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
			if( (DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE )
			{
				DV_PROC_CHECK |= DV_SUB_DONE;
				dvprintk("%s-%d : Done(0x%x) \n", __func__, __LINE__, DV_PROC_CHECK);
				if( (DV_PROC_CHECK & DV_ALL_DONE) == DV_ALL_DONE )
					TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image);
				return IRQ_HANDLED;
			}
#endif

#ifdef CONFIG_USE_SUB_MULTI_FRAME
			if(scaler_sub->info->mosaic_mode) {
				if(!list_empty(&video_queue_list)) {
					struct video_queue_t *q;
					q = video_queue_pop();
					if(q) {
						TCC_VIQE_Scaler_Sub_Repeat60Hz_M2M(&sub_m2m_image[q->type]);
						video_queue_delete(q);
					}
					return IRQ_HANDLED;
				}
			}
#endif
			TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image_sub);
		}
	}

	return IRQ_HANDLED;
}

void TCC_VIQE_Display_Update60Hz_M2M(struct tcc_lcdc_image_update *input_image)
{
	struct tcc_dp_device *dp_device = tca_fb_get_displayType(gOutputMode);

	dprintk("update IN : %d\n", input_image->first_frame_after_seek);

	if(!input_image->first_frame_after_seek)
	{
		switch(gOutputMode)
		{
			case TCC_OUTPUT_NONE:
				break;

			#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
			case TCC_OUTPUT_LCD:
				tca_scale_display_update(dp_device, input_image);
				break;
			#endif

			case TCC_OUTPUT_HDMI:
			case TCC_OUTPUT_COMPONENT:
			case TCC_OUTPUT_COMPOSITE:
				dprintk("%s : Layer(%d) update %d, %d, %d\n",__func__,
						input_image->Lcdc_layer, input_image->buffer_unique_id, input_image->addr0, input_image->odd_first_flag);
				tca_scale_display_update(dp_device, input_image);
				break;

			default:
				break;
		}
	}
//	pr_info("[INF][VIQE] update OUT\n");
}
/////////////////////////////////////////////////////////////////////////////////////////

/* 
	//img_fmt
	TCC_LCDC_IMG_FMT_YUV420SP = 24,	
	TCC_LCDC_IMG_FMT_YUV422SP = 25, 
	TCC_LCDC_IMG_FMT_YUYV = 26, 
	TCC_LCDC_IMG_FMT_YVYU = 27,
	
	TCC_LCDC_IMG_FMT_YUV420ITL0 = 28, 
	TCC_LCDC_IMG_FMT_YUV420ITL1 = 29, 
	TCC_LCDC_IMG_FMT_YUV422ITL0 = 30, 
	TCC_LCDC_IMG_FMT_YUV422ITL1 = 31, 
*/

//////////////////////////////////////////////////////////////////////////////////////////
void TCC_VIQE_DI_Init60Hz(TCC_OUTPUT_TYPE outputMode, int lcdCtrlNum, struct tcc_lcdc_image_update *input_image)
{
	unsigned int deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3;
	int imgSize;
	int top_size_dont_use = OFF;		//If this value is OFF, The size information is get from VIOC modules.
	unsigned int framebufWidth, framebufHeight;
	unsigned int lcd_width = 0, lcd_height = 0;

	struct tcc_viqe_60hz_virt_addr_info_t *pViqe_60hz_info = NULL;
	volatile void __iomem *pVIQE_Info = NULL;
	int nVIOC_VIQE = 0;

	int cropWidth, cropHeight;
	int crop_top = input_image->crop_top;
	int crop_bottom = input_image->crop_bottom;
	int crop_left = input_image->crop_left;
	int crop_right = input_image->crop_right;

	bool y2r_on = false;

	if(0 > TCC_VIQE_Scaler_Init_Buffer_M2M())
	{
		pr_err("[ERR][VIQE] %s-%d : memory allocation is failed.\n", __func__, __LINE__);
		return;
	}

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_INIT, input_image, input_image->Lcdc_layer);
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	bStep_Check = DEF_DV_CHECK_NUM;
#endif

	gLcdc_layer_60Hz = input_image->Lcdc_layer;

	if(input_image->fmt == 24 || input_image->fmt == 28 || input_image->fmt==29)
		gViqe_fmt_60Hz = VIOC_VIQE_FMT_YUV420;
	else
		gViqe_fmt_60Hz = VIOC_VIQE_FMT_YUV422;
	gImg_fmt_60Hz = input_image->fmt;

	gOutputMode = outputMode;
	if(gOutputMode == TCC_OUTPUT_LCD){
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		pViqe_60hz_info = &viqe_60hz_lcd_info;
	}
	else {
		if( input_image->Lcdc_layer == RDMA_VIDEO ){
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			pViqe_60hz_info = &viqe_60hz_external_info;
		}
		else if( input_image->Lcdc_layer == RDMA_VIDEO_SUB ){
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
			pViqe_60hz_info = &viqe_60hz_external_sub_info;
		}
	}

#ifdef MAX_VIQE_FRAMEBUFFER
	framebufWidth = 1920;
	framebufHeight = 1088;
#else
	framebufWidth = ((input_image->Frame_width) >> 3) << 3;			// 8bit align
	framebufHeight = ((input_image->Frame_height) >> 2) << 2;		// 4bit align
#endif

	crop_top = (crop_top >> 1) << 1;
	crop_left = (crop_left >> 1) << 1;
	cropWidth = ((crop_right - crop_left) >> 3) << 3;		//8bit align
	cropHeight = ((crop_bottom - crop_top) >> 2) << 2;		//4bit align

	dprintk("TCC_VIQE_DI_Init60Hz, FB:%dx%d, SRC:%dx%d, DST:%dx%d(%d,%d), FMT:%d, VFMT:%s, OddFirst:%d\n", framebufWidth, framebufHeight, cropWidth, cropHeight, input_image->Image_width, input_image->Image_height, input_image->offset_x, input_image->offset_y, input_image->fmt, (gViqe_fmt_60Hz?"YUV422":"YUV420"), input_image->odd_first_flag);
//	printk("TCC_VIQE_DI_Init60Hz, W:%d, H:%d, DW:%d, DH:%d, FMT:%d, VFMT:%s, OddFirst:%d\n", framebufWidth, framebufHeight, input_image->Image_width, input_image->Image_height, input_image->fmt, (gViqe_fmt_60Hz?"YUV422":"YUV420"), input_image->odd_first_flag);

	VIOC_DISP_GetSize(pViqe_60hz_info->pDISPBase_60Hz, &lcd_width, &lcd_height);
	if((!lcd_width) || (!lcd_height)) {
		pr_err("[ERR][VIQE] %s invalid lcd size\n", __func__);
		return;
	}

	#if defined(USE_DEINTERLACE_S_IN60Hz)
		gDeintlS_Use_60Hz = 1;
	#else
		#if defined(DYNAMIC_USE_DEINTL_COMPONENT)
			if((framebufWidth >= 1280) && (framebufHeight >= 720))
			{
				gDeintlS_Use_60Hz = 1;
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_S;
			}
			else
			{
				gDeintlS_Use_60Hz = 0;
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
			}
		#endif
	#endif

	if(viqe_common_info.gBoard_stb == 1) {
		VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
		//TCC_OUTPUT_FB_ClearVideoImg();
	}

	//RDMA SETTING
	if(gDeintlS_Use_60Hz) {
		VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, true);
		VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02); /* Y2RMode Default 0 (Studio Color) */
	} else {
		VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, false);
		VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02); /* Y2RMode Default 0 (Studio Color) */
	}

	VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz,input_image->fmt, framebufWidth);
	VIOC_RDMA_SetImageFormat(pViqe_60hz_info->pRDMABase_60Hz, input_image->fmt);
	VIOC_RDMA_SetImageSize(pViqe_60hz_info->pRDMABase_60Hz, framebufWidth, framebufHeight);
	VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 1);
	VIOC_RDMA_SetImageBfield(pViqe_60hz_info->pRDMABase_60Hz, input_image->odd_first_flag);

	if(gDeintlS_Use_60Hz)
	{
		dprintk("%s, DeinterlacerS is used!!\n", __func__);
		
		deintl_dma_base0	= 0;
		deintl_dma_base1	= 0;
		deintl_dma_base2	= 0;
		deintl_dma_base3	= 0;
		tcc_sdintls_swreset();
		VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
		gDeintlS_Use_Plugin = 1;
	}
	else
	{
		dprintk("%s, VIQE%d is used!!\n", __func__, get_vioc_index(nVIOC_VIQE));
		
		// If you use 3D(temporal) De-interlace mode, you have to set physical address for using DMA register.
		//If 2D(spatial) mode, these registers are ignored
		imgSize = (framebufWidth * (framebufHeight / 2 ) * 4 * 3 / 2);
		if (input_image->Lcdc_layer == RDMA_VIDEO)
			deintl_dma_base0	= gPMEM_VIQE_BASE;
	#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB)
			deintl_dma_base0	= gPMEM_VIQE_SUB_BASE;
	#endif

		deintl_dma_base1	= deintl_dma_base0 + imgSize;
		deintl_dma_base2	= deintl_dma_base1 + imgSize;
		deintl_dma_base3	= deintl_dma_base2 + imgSize;	

		VIOC_VIQE_IgnoreDecError(pVIQE_Info, ON, ON, ON);
		VIOC_VIQE_SetControlRegister(pVIQE_Info, framebufWidth, framebufHeight, gViqe_fmt_60Hz);
		VIOC_VIQE_SetDeintlRegister(pVIQE_Info, gViqe_fmt_60Hz, top_size_dont_use, framebufWidth, framebufHeight, gDI_mode_60Hz, deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3);
		//VIOC_VIQE_SetDenoise(pVIQE_Info, gViqe_fmt_60Hz, framebufWidth, framebufHeight, 1, 0, deintl_dma_base0, deintl_dma_base1); 	//for bypass path on progressive frame
		VIOC_VIQE_SetControlEnable(pVIQE_Info, OFF, OFF, OFF, OFF, ON);

		y2r_on = viqe_y2r_mode_check();
		VIOC_VIQE_SetImageY2RMode(pVIQE_Info, 0x02);

	#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if ((VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF) && (input_image->Lcdc_layer == RDMA_VIDEO))
			y2r_on = false;
	#endif

		VIOC_VIQE_SetImageY2REnable(pVIQE_Info, y2r_on);
		VIOC_CONFIG_PlugIn(nVIOC_VIQE, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
	}

	//SCALER SETTING
	if(input_image->on_the_fly)
	{
		VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pViqe_60hz_info->gVIQE_RDMA_num_60Hz) != -1)
			VIOC_CONFIG_PlugOut(pViqe_60hz_info->gSCALER_num_60Hz);
		tcc_scaler_swreset(pViqe_60hz_info->gSCALER_num_60Hz);
		VIOC_CONFIG_PlugIn(pViqe_60hz_info->gSCALER_num_60Hz, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
		VIOC_SC_SetBypass (pViqe_60hz_info->pSCALERBase_60Hz, OFF);
		VIOC_SC_SetDstSize (pViqe_60hz_info->pSCALERBase_60Hz, input_image->Image_width, input_image->Image_height);			// set destination size in scaler
		VIOC_SC_SetOutSize (pViqe_60hz_info->pSCALERBase_60Hz, input_image->Image_width, input_image->Image_height);			// set output size in scaer
		VIOC_SC_SetUpdate (pViqe_60hz_info->pSCALERBase_60Hz);
	}

	VIOC_WMIX_SetPosition(pViqe_60hz_info->pWMIXBase_60Hz, gLcdc_layer_60Hz,  input_image->offset_x, input_image->offset_y);
	VIOC_WMIX_SetUpdate(pViqe_60hz_info->pWMIXBase_60Hz);

	if(input_image->Lcdc_layer == RDMA_VIDEO) {
		atomic_set( &gFrmCnt_60Hz, 0 );
		gVIQE_Init_State = 1;
	}
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
		atomic_set( &gFrmCnt_Sub_60Hz, 0 );
		gVIQE1_Init_State = 1;
	}
#endif
}


void TCC_VIQE_DI_Swap60Hz(int mode)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	volatile void __iomem *pVIQE_Info = NULL;
	int nVIOC_VIQE = 0;
	
	if(gOutputMode == TCC_OUTPUT_LCD){
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
	}
	else {
		if( gLcdc_layer_60Hz == RDMA_VIDEO ){
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		}
		else if( gLcdc_layer_60Hz == RDMA_VIDEO_SUB ){
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
		}
	}

	VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &VIOC_PlugIn);
	dprintk("%s - gOutputMode(%d), gLcdc_layer_60Hz(%d), Plug-In(%d), Mode(%d) \n", __func__, gOutputMode, gLcdc_layer_60Hz, VIOC_PlugIn.enable, mode);

	if(VIOC_PlugIn.enable)
		VIOC_VIQE_SwapDeintlBase(pVIQE_Info, mode);
}

void TCC_VIQE_DI_SetFMT60Hz(int enable)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	volatile void __iomem *pVIQE_Info = NULL;
	int nVIOC_VIQE = 0;
	
	if(gOutputMode == TCC_OUTPUT_LCD){
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
	}
	else {
		if( gLcdc_layer_60Hz == RDMA_VIDEO ){
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		}
		else if( gLcdc_layer_60Hz == RDMA_VIDEO_SUB ){
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
		}
	}

	VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &VIOC_PlugIn);
	if(VIOC_PlugIn.enable)
		VIOC_VIQE_SetDeintlFMT(pVIQE_Info, enable);
}

void TCC_VIQE_DI_Run60Hz(struct tcc_lcdc_image_update *input_image, int reset_frmCnt)
{
	unsigned int lcd_width = 0, lcd_height = 0;
	int cropWidth, cropHeight;
	int crop_top = input_image->crop_top;
	int crop_bottom = input_image->crop_bottom;
	int crop_left = input_image->crop_left;
	int crop_right = input_image->crop_right;
	int addr_Y = (unsigned int)input_image->addr0;
	int addr_U = (unsigned int)input_image->addr1;
	int addr_V = (unsigned int)input_image->addr2;

	struct tcc_viqe_60hz_virt_addr_info_t *pViqe_60hz_info = NULL;
	volatile void __iomem *pVIQE_Info = NULL;
	int nVIOC_VIQE = 0;

#ifndef USE_DEINTERLACE_S_IN60Hz
	VIOC_PlugInOutCheck VIOC_PlugIn;
	unsigned int JudderCnt = 0;
#endif

#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_lock = 0;
#endif

	bool y2r_on = false;

	unsigned int frmCnt_60hz = 0;

	if(gOutputMode == TCC_OUTPUT_LCD){
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		pViqe_60hz_info = &viqe_60hz_lcd_info;
	}
	else {
		if( input_image->Lcdc_layer == RDMA_VIDEO ){
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			pViqe_60hz_info = &viqe_60hz_external_info;
		}
		else if( input_image->Lcdc_layer == RDMA_VIDEO_SUB ){
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
			pViqe_60hz_info = &viqe_60hz_external_sub_info;
		}
	}		

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_ON, input_image, input_image->Lcdc_layer);
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if(input_image->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO] != 0)
	{
		if(!bStep_Check){
			nFrame++;
		}
		else{
			if(bStep_Check == DEF_DV_CHECK_NUM){
				nFrame = 1;//0;
				bStep_Check--;
			}
			else{
				bStep_Check--;
				return;
			}
		}
	}
#endif

	if(viqe_common_info.gBoard_stb == 1) {
		if(input_image->Lcdc_layer == RDMA_VIDEO) {
			if(gVIQE_Init_State == 0)
			{
				dprintk("%s VIQE block isn't initailized\n", __func__);
				return;
			}
		} 
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB) {
			if(gVIQE1_Init_State == 0)
			{
				dprintk("%s VIQE block isn't initailized\n", __func__);
				return;
			}
		}
		#endif
	}

#ifndef USE_DEINTERLACE_S_IN60Hz
	VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &VIOC_PlugIn);
	if(VIOC_PlugIn.enable == 0) {
		dprintk("%s VIQE block isn't pluged!!!\n", __func__);
		return;		
	}
#endif

	VIOC_DISP_GetSize(pViqe_60hz_info->pDISPBase_60Hz, &lcd_width, &lcd_height);
	if((!lcd_width) || (!lcd_height)) {
		pr_err("[ERR][VIQE] %s invalid lcd size\n", __func__);
		return;
	}

	#ifdef CONFIG_TCC_VIOCMG
	if(!gDeintlS_Use_60Hz)
	{
		if(viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) < 0) {
			unsigned int enable = 0;
			VIOC_RDMA_GetImageEnable(pViqe_60hz_info->pRDMABase_60Hz, &enable);
			if(enable)
				VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
			return;
		} else {
			viqe_lock = 1;

			VIOC_PlugInOutCheck plug_status;
			VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &plug_status);
			if(!plug_status.enable || plug_status.connect_statue != VIOC_PATH_CONNECTED)
				reset_frmCnt = VIQE_RESET_RECOVERY;
		}
	}
	#endif

	if( reset_frmCnt != VIQE_RESET_NONE && reset_frmCnt != VIQE_RESET_CROP_CHANGED){
		if(input_image->Lcdc_layer == RDMA_VIDEO)
			atomic_set( &gFrmCnt_60Hz, 0 );
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB)
			atomic_set( &gFrmCnt_Sub_60Hz, 0 );
#endif
		dprintk("VIQE%d 3D -> 2D :: %d -> %dx%d \n", get_vioc_index(nVIOC_VIQE), reset_frmCnt, (crop_right - crop_left), (crop_bottom - crop_top));
	}

	if(input_image->Lcdc_layer == RDMA_VIDEO)
		frmCnt_60hz = atomic_read(&gFrmCnt_60Hz);
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB)
		frmCnt_60hz = atomic_read(&gFrmCnt_Sub_60Hz);
#endif

	crop_top = (crop_top >> 1) << 1;
	crop_left = (crop_left >> 1) << 1;
	cropWidth = ((crop_right - crop_left) >> 3) << 3;		//8bit align
	cropHeight = ((crop_bottom - crop_top) >> 2) << 2;		//4bit align

	//Prevent Display Block from FIFO under-run, when current_crop size is smaller than prev_crop size(screen mode: PANSCAN<->BOX)
	//So, Soc team suggested VIQE block resetting
	if(frmCnt_60hz == 0 && (gDeintlS_Use_60Hz == 0)) {
		unsigned int deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3;
		int top_size_dont_use = OFF;		//If this value is OFF, The size information is get from VIOC modules.
		int imgSize;

		dprintk("VIQE%d reset 3D -> 2D :: %dx%d \n", get_vioc_index(nVIOC_VIQE), (crop_right - crop_left), (crop_bottom - crop_top));
		VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
		VIOC_CONFIG_PlugOut(nVIOC_VIQE);

		tcc_viqe_swreset(nVIOC_VIQE);

		// If you use 3D(temporal) De-interlace mode, you have to set physical address for using DMA register.
		//If 2D(spatial) mode, these registers are ignored
		imgSize = (input_image->Frame_width * (input_image->Frame_height / 2 ) * 4 * 3 / 2);
		if (input_image->Lcdc_layer == RDMA_VIDEO)
			deintl_dma_base0	= gPMEM_VIQE_BASE;
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB)
			deintl_dma_base0	= gPMEM_VIQE_SUB_BASE;
#endif
		deintl_dma_base1	= deintl_dma_base0 + imgSize;
		deintl_dma_base2	= deintl_dma_base1 + imgSize;
		deintl_dma_base3	= deintl_dma_base2 + imgSize;

		VIOC_VIQE_IgnoreDecError(pVIQE_Info, ON, ON, ON);
		VIOC_VIQE_SetControlRegister(pVIQE_Info, input_image->Frame_width, input_image->Frame_height, gViqe_fmt_60Hz);
		VIOC_VIQE_SetDeintlRegister(pVIQE_Info, gViqe_fmt_60Hz, top_size_dont_use, input_image->Frame_width, input_image->Frame_height, gDI_mode_60Hz, deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3);
		//VIOC_VIQE_SetDenoise(pVIQE_Info, gViqe_fmt_60Hz, srcWidth, srcHeight, 1, 0, deintl_dma_base0, deintl_dma_base1); 	//for bypass path on progressive frame

		y2r_on = viqe_y2r_mode_check();
		VIOC_VIQE_SetImageY2RMode(pVIQE_Info, 0x02);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if ((VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF) && (input_image->Lcdc_layer == RDMA_VIDEO))
		y2r_on = false;
#endif

		VIOC_VIQE_SetImageY2REnable(pVIQE_Info, y2r_on);
		VIOC_CONFIG_PlugIn(nVIOC_VIQE, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
	}

	tccxxx_GetAddress(gImg_fmt_60Hz, (unsigned int)input_image->addr0, input_image->Frame_width, input_image->Frame_height, crop_left, crop_top, &addr_Y, &addr_U, &addr_V);
	VIOC_RDMA_SetImageSize(pViqe_60hz_info->pRDMABase_60Hz, cropWidth, cropHeight);
	VIOC_RDMA_SetImageBase(pViqe_60hz_info->pRDMABase_60Hz, addr_Y, addr_U, addr_V);

#ifdef CONFIG_VIOC_10BIT
	if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
		VIOC_RDMA_SetDataFormat(pViqe_60hz_info->pRDMABase_60Hz, 0x1, 0x1);	/* YUV 10bit support */
	else if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
		VIOC_RDMA_SetDataFormat(pViqe_60hz_info->pRDMABase_60Hz, 0x3, 0x0);	/* YUV real 10bit support */
	else
		VIOC_RDMA_SetDataFormat(pViqe_60hz_info->pRDMABase_60Hz, 0x0, 0x0);	/* YUV 8bit support */

	if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
		VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz, gImg_fmt_60Hz, input_image->Frame_width * 2);
	else if(input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
		VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz, gImg_fmt_60Hz, input_image->Frame_width * 125 / 100);
	else
#endif
		VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz, gImg_fmt_60Hz, input_image->Frame_width);

	VIOC_RDMA_SetImageBfield(pViqe_60hz_info->pRDMABase_60Hz, input_image->odd_first_flag);

	if(input_image->odd_first_flag)
		dprintk("ID[%03d] Layer[%d] Otf:%d, Addr:0x%x/0x%x, Crop[l:%d, r:%d, t:%d, b:%d], Res[W:%d, H:%d] odd:%d frm-I:%d bpp:%d \n",
				input_image->private_data.optional_info[VID_OPT_BUFFER_ID],
				input_image->Lcdc_layer, input_image->on_the_fly, addr_Y, addr_U,
				crop_left, crop_right, crop_top, crop_bottom, cropWidth, cropHeight,
				input_image->odd_first_flag, input_image->frameInfo_interlace,
				input_image->private_data.optional_info[VID_OPT_BIT_DEPTH]);

	if(gDeintlS_Use_60Hz)
	{
		//VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, false);
		//VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02); /* Y2RMode Default 0 (Studio Color) */

		if(input_image->frameInfo_interlace)
		{
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 1);
			if(!gDeintlS_Use_Plugin)
			{
				VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
				gDeintlS_Use_Plugin = 1;
			}
		}
		else
		{
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 0);
			if(gDeintlS_Use_Plugin)
			{
				VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
				gDeintlS_Use_Plugin = 0;
			}
		}		
	}
	else
	{
		if(input_image->frameInfo_interlace)
		{
			unsigned int frmCnt_60hz = 0;
			if(input_image->Lcdc_layer == RDMA_VIDEO)
				frmCnt_60hz = atomic_read(&gFrmCnt_60Hz);
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB)
				frmCnt_60hz = atomic_read(&gFrmCnt_Sub_60Hz);
#endif
			VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, false);
			VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02); /* Y2RMode Default 0 (Studio Color) */
			if(frmCnt_60hz >= 3)
			{
				VIOC_VIQE_SetDeintlMode(pVIQE_Info, VIOC_VIQE_DEINTL_MODE_3D);
#ifndef USE_DEINTERLACE_S_IN60Hz
				// Process JUDDER CNTS
				JudderCnt = (input_image->Frame_width + 64)/64 -1;
				VIOC_VIQE_SetDeintlJudderCnt(pVIQE_Info, JudderCnt);
#endif
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_3D;
				dprintk("VIQE%d 3D %d x %d \n", get_vioc_index(nVIOC_VIQE), cropWidth, cropHeight);
			}
			else
			{
				VIOC_VIQE_SetDeintlMode(pVIQE_Info, VIOC_VIQE_DEINTL_MODE_2D);
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
				dprintk("VIQE%d 2D %d x %d \n", get_vioc_index(nVIOC_VIQE), cropWidth, cropHeight);
			}

			VIOC_VIQE_SetControlMode(pVIQE_Info, OFF, OFF, OFF, OFF, ON);
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 1);
		}
		else
		{
			VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, true);
			VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_VIQE_SetControlMode(pVIQE_Info, OFF, OFF, OFF, OFF, OFF);
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 0);
			if(input_image->Lcdc_layer == RDMA_VIDEO)
				atomic_set( &gFrmCnt_60Hz, 0 );
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			else if(input_image->Lcdc_layer == RDMA_VIDEO_SUB)
				atomic_set( &gFrmCnt_Sub_60Hz, 0 );
#endif
		}
	}

	if(input_image->on_the_fly)
	{
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pViqe_60hz_info->gVIQE_RDMA_num_60Hz) != pViqe_60hz_info->gSCALER_num_60Hz) {
			VIOC_CONFIG_PlugIn (pViqe_60hz_info->gSCALER_num_60Hz, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
			VIOC_SC_SetBypass (pViqe_60hz_info->pSCALERBase_60Hz, OFF);
		}

		VIOC_SC_SetDstSize (pViqe_60hz_info->pSCALERBase_60Hz, input_image->Image_width, input_image->Image_height);			// set destination size in scaler
		VIOC_SC_SetOutSize (pViqe_60hz_info->pSCALERBase_60Hz, input_image->Image_width, input_image->Image_height);			// set output size in scaer
		VIOC_SC_SetUpdate (pViqe_60hz_info->pSCALERBase_60Hz);
	}
	else
	{
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pViqe_60hz_info->gVIQE_RDMA_num_60Hz) == pViqe_60hz_info->gSCALER_num_60Hz)	{
			VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
			VIOC_CONFIG_PlugOut(pViqe_60hz_info->gSCALER_num_60Hz);
		}
	}
	// position
	VIOC_WMIX_SetPosition(pViqe_60hz_info->pWMIXBase_60Hz, gLcdc_layer_60Hz,  input_image->offset_x, input_image->offset_y);
	VIOC_WMIX_SetUpdate(pViqe_60hz_info->pWMIXBase_60Hz);
	VIOC_RDMA_SetImageEnable(pViqe_60hz_info->pRDMABase_60Hz);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH())
	{
		VIOC_RDMA_SetImageUVIEnable(pViqe_60hz_info->pRDMABase_60Hz, 0);
		VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 0);

		if(input_image->Lcdc_layer == RDMA_VIDEO || input_image->Lcdc_layer == RDMA_LASTFRM){

			if(input_image->Lcdc_layer == RDMA_LASTFRM
				&& (input_image->fmt >= TCC_LCDC_IMG_FMT_444SEP && input_image->fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1))
			{
				VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, true);
				VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x2);
			}
			
			if(vioc_get_out_type() == input_image->private_data.dolbyVision_info.reg_out_type)
			{
				VIOC_V_DV_SetSize(NULL, pViqe_60hz_info->pRDMABase_60Hz, input_image->offset_x, input_image->offset_y, Hactive, Vactive);
				vioc_v_dv_prog( input_image->private_data.dolbyVision_info.md_hdmi_addr,
								input_image->private_data.dolbyVision_info.reg_addr,
								input_image->private_data.optional_info[VID_OPT_CONTENT_TYPE],
								1);
			}
		}
		else{
			pr_err("[ERR][VIQE] -4- Should be implement other layer configuration. \n");
			return;
		}
	}
#endif

#ifdef CONFIG_TCC_VIOCMG
	if(viqe_lock)
		viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
#endif

	tcc_video_post_process(input_image);
	if(input_image->Lcdc_layer == RDMA_VIDEO)
		atomic_inc(&gFrmCnt_60Hz);
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	else if (input_image->Lcdc_layer == RDMA_VIDEO_SUB)
		atomic_inc(&gFrmCnt_Sub_60Hz);
#endif
}

void TCC_VIQE_DI_DeInit60Hz(void)
{
	struct tcc_viqe_60hz_virt_addr_info_t *pViqe_60hz_info = NULL;
	volatile void __iomem *pVIQE_Info = NULL;
	int nVIOC_VIQE = 0;

	if(gOutputMode == TCC_OUTPUT_LCD){
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		pViqe_60hz_info = &viqe_60hz_lcd_info;
	}
	else {
		if( gLcdc_layer_60Hz == RDMA_VIDEO ){
			atomic_set( &gFrmCnt_60Hz, 0 );
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			pViqe_60hz_info = &viqe_60hz_external_info;
		}
		else if( gLcdc_layer_60Hz == RDMA_VIDEO_SUB ){
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			atomic_set( &gFrmCnt_Sub_60Hz, 0 );
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
			pViqe_60hz_info = &viqe_60hz_external_sub_info;
		}
	}

	pr_info("[INF][VIQE] %s\n", __func__);

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_OFF, NULL, RDMA_VIDEO);
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	bStep_Check = DEF_DV_CHECK_NUM;
#endif

	VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
	VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, true);
	VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02); /* Y2RMode Default 0 (Studio Color) */
	if(gDeintlS_Use_60Hz)
	{
		VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
		tcc_sdintls_swreset();
		gDeintlS_Use_Plugin = 0;
	}
	else
	{
		#ifdef CONFIG_TCC_VIOCMG
		if(viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) == 0) {
			VIOC_CONFIG_PlugOut(nVIOC_VIQE);
			tcc_viqe_swreset(nVIOC_VIQE);

			viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
		}
		#else
		VIOC_CONFIG_PlugOut(nVIOC_VIQE);
		tcc_viqe_swreset(nVIOC_VIQE);
		#endif
	}
	VIOC_CONFIG_PlugOut(pViqe_60hz_info->gSCALER_num_60Hz);
	VIOC_CONFIG_SWReset(pViqe_60hz_info->gSCALER_num_60Hz, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pViqe_60hz_info->gSCALER_num_60Hz, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_SWReset(pViqe_60hz_info->gVIQE_RDMA_num_60Hz, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pViqe_60hz_info->gVIQE_RDMA_num_60Hz, VIOC_CONFIG_CLEAR);

	TCC_VIQE_Scaler_DeInit_Buffer_M2M();

	gVIQE_Init_State = 0;
}

unsigned int tcc_viqe_commom_dt_parse(struct device_node *np, struct tcc_viqe_common_virt_addr_info_t *viqe_common_info)
{
	struct device_node *viqe_node0, *viqe_node1, *deintls_node;

	#ifdef CONFIG_OF
	viqe_node0 = of_parse_phandle(np,"telechips,viqe,0", 0);
	of_property_read_u32_index(np,"telechips,viqe,0", 1, &viqe_common_info->gVIOC_VIQE0);
	if (!viqe_node0) {
		pr_err("[ERR][VIQE] could not find viqe_video viqe0 node\n");
	}
	else {
		viqe_common_info->pVIQE0= VIOC_VIQE_GetAddress(viqe_common_info->gVIOC_VIQE0);
		//pr_info("[INF][VIQE] viqe_video viqe0 %d %x  \n", viqe_common_info->gVIOC_VIQE0, viqe_common_info->pVIQE0);
	}

	viqe_node1 = of_parse_phandle(np,"telechips,viqe,1", 0);
	of_property_read_u32_index(np,"telechips,viqe,1", 1, &viqe_common_info->gVIOC_VIQE1);
	if (!viqe_node1) {
		pr_err("[ERR][VIQE] could not find viqe_video viqe1 node\n");

		viqe_common_info->pVIQE1= viqe_common_info->pVIQE0;
		viqe_common_info->gVIOC_VIQE1 = viqe_common_info->gVIOC_VIQE0;
		not_exist_viqe1 = 1;
		//pr_info("[INF][VIQE] viqe_video viqe1(viqe0) %d %x  \n", index, viqe_common_info->pVIQE1);
	}
	else {
		viqe_common_info->pVIQE1 = VIOC_VIQE_GetAddress(viqe_common_info->gVIOC_VIQE1);
		//pr_info("[INF][VIQE] viqe_video viqe1 %d %x  \n", viqe_common_info->gVIOC_VIQE1, viqe_common_info->pVIQE1);
	}

	deintls_node = of_find_compatible_node(NULL, NULL, "telechips,vioc_deintls");
	of_property_read_u32(np, "vioc_deintls", &viqe_common_info->gVIOC_Deintls);
	if(deintls_node == NULL) {
		pr_err("[ERR][VIQE] can not find vioc deintls \n");
	} else {
		viqe_common_info->pDEINTLS = VIOC_DEINTLS_GetAddress();
		if (viqe_common_info->pDEINTLS == NULL)
			pr_err("can not find vioc deintls address \n");
		//pr_info("[INF][VIQE] deintls 0x%p\n",viqe_common_info->pDEINTLS);
	}

	of_property_read_u32(np, "board_num", &viqe_common_info->gBoard_stb);

	iprintk("VIQE Common Information. \n");
	iprintk("Deintls(0x%x) / Viqe0(0x%x) / Viqe1(0x%x) / gBoard_stb(%d) \n", viqe_common_info->gVIOC_Deintls, viqe_common_info->gVIOC_VIQE0, viqe_common_info->gVIOC_VIQE1, viqe_common_info->gBoard_stb);
	#endif

	return 0;
}

unsigned int tcc_viqe_60hz_dt_parse(struct device_node *np, struct tcc_viqe_60hz_virt_addr_info_t *viqe_60hz_info)
{
	unsigned int index;
	struct device_node *rdma_node, *wmixer_node, *disp_node, *scaler_node;

	#ifdef CONFIG_OF
	rdma_node = of_parse_phandle(np,"telechips,rdma,60", 0);
	of_property_read_u32_index(np,"telechips,rdma,60", 1, &viqe_60hz_info->gVIQE_RDMA_num_60Hz);
	if (!rdma_node) {
		pr_err("[ERR][VIQE] could not find viqe_video rdma_60hz node\n");
	}
	else {
		viqe_60hz_info->pRDMABase_60Hz = VIOC_RDMA_GetAddress(viqe_60hz_info->gVIQE_RDMA_num_60Hz);
		//pr_info("[INF][VIQE] viqe_video rdma_60hz %d %x  \n", viqe_60hz_info->gVIQE_RDMA_num_60Hz, viqe_60hz_info->pRDMABase_60Hz);
	}

	wmixer_node = of_parse_phandle(np,"telechips,wmixer", 0);
	of_property_read_u32_index(np,"telechips,wmixer", 1, &index);
	if (!wmixer_node) {
		pr_err("[ERR][VIQE] could not find viqe_video wmixer node\n");
	}
	else {
		viqe_60hz_info->pWMIXBase_60Hz = VIOC_WMIX_GetAddress(index);
		//pr_info("[INF][VIQE] viqe_video wmixe_60hz %d %x  \n", index, viqe_60hz_info->pWMIXBase_60Hz);
	}

	disp_node = of_parse_phandle(np, "telechips,disp", 0);
	of_property_read_u32_index(np,"telechips,disp", 1, &index);
	if (!disp_node) {
		pr_err("[ERR][VIQE] could not find viqe_video display node\n");
	}
	else {
		viqe_60hz_info->pDISPBase_60Hz= VIOC_DISP_GetAddress(index);
		pr_info("[INF][VIQE] viqe_video display_60hz 0x%x %p  \n", index, viqe_60hz_info->pDISPBase_60Hz);
	}

	scaler_node = of_parse_phandle(np, "telechips,sc", 0);
	of_property_read_u32_index(np,"telechips,sc", 1, &viqe_60hz_info->gSCALER_num_60Hz);
	if (!scaler_node) {
		pr_err("[ERR][VIQE] could not find viqe_video scaler node\n");
	}
	else {
		viqe_60hz_info->pSCALERBase_60Hz= VIOC_SC_GetAddress(viqe_60hz_info->gSCALER_num_60Hz);
		//pr_info("[INF][VIQE] viqe_video scaler_60hz %d %x  \n", viqe_60hz_info->gSCALER_num_60Hz, viqe_60hz_info->pSCALERBase_60Hz);
	}

	iprintk("VIQE_60Hz Information. \n");
	iprintk("Reg_Base :: RDMA / Scaler / WMIX / DISP = 0x%p / 0x%p / 0x%p / 0x%p \n",
			viqe_60hz_info->pRDMABase_60Hz, viqe_60hz_info->pSCALERBase_60Hz, viqe_60hz_info->pWMIXBase_60Hz, viqe_60hz_info->pDISPBase_60Hz);
	iprintk("NUM :: RDMA / SC = 0x%x / 0x%x \n",
			viqe_60hz_info->gVIQE_RDMA_num_60Hz, viqe_60hz_info->gSCALER_num_60Hz);
	#endif

	return 0;
}

// ok
int tcc_viqe_m2m_dt_parse(struct device_node *main_deIntl, struct device_node *sub_deIntl, struct device_node *sc_main, struct device_node *sc_sub,
	struct tcc_viqe_m2m_virt_addr_info_t *main_m2m_info, struct tcc_viqe_m2m_virt_addr_info_t *sub_m2m_info)
{
	struct device_node *main_deintl_rdma_node, *sub_deintl_rdma_node;
	struct device_node *sc_rdma_node, *sc_wmixer_node, *sc_scaler_node, *sc_wdma_node;
	int ret = -ENOMEM;

	//Main :: m2m_deIntl_info
	main_deintl_rdma_node = of_parse_phandle(main_deIntl,"telechips,main_rdma,m2m", 0);
	of_property_read_u32_index(main_deIntl,"telechips,main_rdma,m2m", 1, &main_m2m_info->gVIQE_RDMA_num_m2m);
	if (!main_deintl_rdma_node) {
		pr_err("[ERR][VIQE] could not find viqe_video main_rdma_m2m node\n");
	}
	else {
		main_m2m_info->pRDMABase_m2m = VIOC_RDMA_GetAddress(main_m2m_info->gVIQE_RDMA_num_m2m);
		//pr_info("[INF][VIQE] main_video rdma_m2m %d %x  \n", index, main_m2m_info->pRDMABase_m2m);
	}

	//Sub :: m2m_deIntl_info
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	// VIQE1
	sub_deintl_rdma_node = of_parse_phandle(sub_deIntl,"telechips,sub_rdma,m2m", 0);
	of_property_read_u32_index(sub_deIntl,"telechips,sub_rdma,m2m", 1, &sub_m2m_info->gVIQE_RDMA_num_m2m);
	if (!sub_deintl_rdma_node) {
		pr_err("[ERR][VIQE] could not find viqe_video sub_rdma_m2m node\n");
	}
	else {
		sub_m2m_info->pRDMABase_m2m = VIOC_RDMA_GetAddress(sub_m2m_info->gVIQE_RDMA_num_m2m);
		//pr_info("[INF][VIQE] sub_video rdma_m2m %d %x  \n", index, main_m2m_info->pRDMABase_m2m);
	}
#else
	// Simple DeInterlacer
	sub_deintl_rdma_node = of_parse_phandle(sub_deIntl,"telechips,sub_rdma,m2m", 0);
	of_property_read_u32_index(sub_deIntl,"telechips,sub_rdma,m2m", 1, &sub_m2m_info->gDEINTLS_RDMA_num_m2m);
	if (!sub_deintl_rdma_node) {
		pr_err("[ERR][VIQE] could not find deintls_video sub_rdma_m2m node\n");
	}
	else {
		sub_m2m_info->pRDMABase_m2m= VIOC_RDMA_GetAddress(sub_m2m_info->gDEINTLS_RDMA_num_m2m);
		//pr_info("[INF][VIQE] sub_video rdma_m2m %d %x  \n", index, sub_m2m_info->pRDMABase_m2m);
	}
#endif

	//Main :: m2m_deIntl_scaler
	scaler = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_type_t), GFP_KERNEL);
	if (scaler == NULL) {
		pr_err("[ERR][VIQE] fail scaler kzalloc\n");
		return ret;
	}

	scaler->info = kzalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);
	if (scaler->info == NULL) {
		pr_err("[ERR][VIQE] fail scaler->info kzalloc\n");
		return ret;
	}

	scaler->data = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_data), GFP_KERNEL);
	if (scaler->data == NULL) {
		pr_err("[ERR][VIQE] fail scaler->data kzalloc\n");
		return ret;
	}

	scaler->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (scaler->vioc_intr == NULL) {
		pr_err("[ERR][VIQE] fail scaler->vioc_intr kzalloc\n");
		return ret;
	}

	scaler->rdma = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->rdma == NULL) {
		pr_err("[ERR][VIQE] ail scaler->rdma kzalloc\n");
		return ret;
	}

	scaler->wmix= kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->wmix == NULL) {
		pr_err("[ERR][VIQE] fail scaler->wmix kzalloc\n");
		return ret;
	}
	scaler->sc= kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->sc == NULL) {
		pr_err("[ERR][VIQE] fail scaler->sc kzalloc\n");
		return ret;
	}

	scaler->wdma= kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->wdma == NULL) {
		pr_err("[ERR][VIQE] fail scaler->wdma kzalloc\n");
		return ret;
        }

	 sc_rdma_node = of_parse_phandle(sc_main,"m2m_sc_rdma", 0);
	 of_property_read_u32_index(sc_main,"m2m_sc_rdma", 1, &scaler->rdma->id);
	if (!sc_rdma_node) {
		pr_err("[ERR][VIQE] could not find viqe_video rdma node of scaler\n");
	}
	else {
		scaler->rdma->reg = VIOC_RDMA_GetAddress(scaler->rdma->id);
		//pr_info("[INF][VIQE] main_video scaler->rdma[%d] %x  \n", scaler->rdma->id, scaler->rdma->reg);
	}

	 sc_wmixer_node = of_parse_phandle(sc_main,"m2m_sc_wmix", 0);
	 of_property_read_u32_index(sc_main,"m2m_sc_wmix", 1, &scaler->wmix->id);
	if (!sc_wmixer_node) {
		pr_err("[ERR][VIQE] could not find viqe_video wmixer node of scaler\n");
	}
	else {
		scaler->wmix->reg = VIOC_WMIX_GetAddress(scaler->wmix->id);
		//of_property_read_u32(sc_main, "m2m_sc_wmix_path", &scaler->wmix->path);
		//pr_info("[INF][VIQE] main_video scaler->wmix[%d] %x  \n", scaler->wmix->id, scaler->wmix->reg);
	}

	 sc_scaler_node = of_parse_phandle(sc_main,"m2m_sc_scaler", 0);
	 of_property_read_u32_index(sc_main,"m2m_sc_scaler", 1, &scaler->sc->id);
	if (!sc_scaler_node) {
		pr_err("[ERR][VIQE] could not find viqe_video scaler node of scaler\n");
	}
	else {
		scaler->sc->reg = VIOC_SC_GetAddress(scaler->sc->id);
		//pr_info("[INF][VIQE] main_video scaler->scalers[%d] %x  \n", scaler->sc->id, scaler->sc->reg);
	}

	 sc_wdma_node = of_parse_phandle(sc_main,"m2m_sc_wdma", 0);
	 of_property_read_u32_index(sc_main,"m2m_sc_wdma", 1, &scaler->wdma->id);
	if (!sc_wdma_node) {
		pr_err("[ERR][VIQE] could not find viqe_videowdma node of scaler\n");
	}
	else {
		scaler->wdma->reg  = VIOC_WDMA_GetAddress(scaler->wdma->id);

		scaler->irq = irq_of_parse_and_map(sc_wdma_node, get_vioc_index(scaler->wdma->id));
		scaler->vioc_intr->id   = VIOC_INTR_WD0 + get_vioc_index(scaler->wdma->id);
		scaler->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
		//pr_info("[INF][VIQE] main_video scaler->wdmas[%d] %x  \n", scaler->wdma->id, scaler->wdma->reg);
	}

	of_property_read_u32(sc_main, "m2m_sc_settop_support", &scaler->settop_support);

	//Sub :: m2m_deIntl_scaler
	scaler_sub = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_type_t), GFP_KERNEL);
	if (scaler_sub == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub kzalloc\n");
		return ret;
	}

	scaler_sub->info = kzalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);
	if (scaler_sub->info == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub->info kzalloc\n");
		return ret;
	}

	scaler_sub->data = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_data), GFP_KERNEL);
	if (scaler_sub->data == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub->data kzalloc\n");
		return ret;
	}

	scaler_sub->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (scaler_sub->vioc_intr == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub->vioc_intr kzalloc\n");
		return ret;
	}

	scaler_sub->rdma = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->rdma == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub->rdma kzalloc\n");
		return ret;
	}

	scaler_sub->wmix= kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->wmix == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub->wmix kzalloc\n");
		return ret;
	}
	scaler_sub->sc= kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->sc == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub->sc kzalloc\n");
		return ret;
	}

	scaler_sub->wdma= kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->wdma == NULL) {
		pr_err("[ERR][VIQE] fail scaler_sub->wdma kzalloc\n");
		return ret;
	}

	sc_rdma_node = of_parse_phandle(sc_sub,"m2m_sc_rdma", 0);
	of_property_read_u32_index(sc_sub,"m2m_sc_rdma", 1, &scaler_sub->rdma->id);
	if (!sc_rdma_node) {
		pr_err( "could not find viqe_video rdma node of scaler_sub\n");
	}
	else {
		scaler_sub->rdma->reg = VIOC_RDMA_GetAddress(scaler_sub->rdma->id);
		//pr_info("[INF][VIQE] sub_video scaler_sub->rdma[%d] %x  \n", scaler_sub->rdma->id, scaler_sub->rdma->reg);
	}

	sc_wmixer_node = of_parse_phandle(sc_sub,"m2m_sc_wmix", 0);
	of_property_read_u32_index(sc_sub,"m2m_sc_wmix", 1, &scaler_sub->wmix->id);
	if (!sc_wmixer_node) {
		pr_err("[ERR][VIQE] could not find viqe_video wmixer node of scaler_sub\n");
	}
	else {
		scaler_sub->wmix->reg = VIOC_WMIX_GetAddress(scaler_sub->wmix->id);
		//of_property_read_u32(sc_sub, "m2m_sc_wmix_path", &scaler_sub->wmix->path);
		//pr_info("[INF][VIQE] sub_video scaler_sub->wmix[%d] %x  \n", scaler_sub->wmix->id, scaler_sub->wmix->reg);
	}

	sc_scaler_node = of_parse_phandle(sc_sub,"m2m_sc_scaler", 0);
	of_property_read_u32_index(sc_sub,"m2m_sc_scaler", 1, &scaler_sub->sc->id);
	if (!sc_scaler_node) {
		pr_err("[ERR][VIQE] could not find viqe_video scaler_sub node of scaler_sub\n");
	}
	else {
		scaler_sub->sc->reg = VIOC_SC_GetAddress(scaler_sub->sc->id);
		//pr_info("[INF][VIQE] sub_video scaler_sub->scalers[%d] %x  \n", scaler_sub->sc->id, scaler_sub->sc->reg);
	}

	sc_wdma_node = of_parse_phandle(sc_sub,"m2m_sc_wdma", 0);
	of_property_read_u32_index(sc_sub,"m2m_sc_wdma", 1, &scaler_sub->wdma->id);
	if (!sc_wdma_node) {
		pr_err("[ERR][VIQE] could not find viqe_video wdma node of scaler_sub\n");
	}
	else {
		scaler_sub->wdma->reg  = VIOC_WDMA_GetAddress(scaler_sub->wdma->id);

		scaler_sub->irq = irq_of_parse_and_map(sc_wdma_node, get_vioc_index(scaler_sub->wdma->id));
		scaler_sub->vioc_intr->id   = VIOC_INTR_WD0 + get_vioc_index(scaler_sub->wdma->id);
		scaler_sub->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
		//pr_info("[INF][VIQE] sub_video scaler_sub->wdmas[%d] %x  \n", scaler_sub->wdma->id, scaler_sub->wdma->reg);
	}

	of_property_read_u32(sc_sub, "m2m_sc_settop_support", &scaler_sub->settop_support);

	iprintk("Main/Sub M2M Information. \n");
	iprintk("RDMA  : (0x%x)0x%p / (0x%x)0x%p \n", scaler->rdma->id, scaler->rdma->reg, scaler_sub->rdma->id, scaler_sub->rdma->reg);
	iprintk("Scaler: (0x%x)0x%p / (0x%x)0x%p \n", scaler->sc->id, scaler->sc->reg, scaler_sub->sc->id, scaler_sub->sc->reg);
	iprintk("WMIX  : (0x%x)0x%p / (0x%x)0x%p \n", scaler->wmix->id, scaler->wmix->reg, scaler_sub->wmix->id, scaler_sub->wmix->reg);
	iprintk("WDMA  : (0x%x)0x%p / (0x%x)0x%p \n", scaler->wdma->id, scaler->wdma->reg, scaler_sub->wdma->id, scaler_sub->wdma->reg);
	iprintk("Intr  : (%d)0x%08x / (%d)0x%08x \n", scaler->vioc_intr->id, scaler->irq, scaler_sub->vioc_intr->id, scaler_sub->irq);

	iprintk("VIQE  : (0x%x)0x%p \n", main_m2m_info->gVIQE_RDMA_num_m2m, main_m2m_info->pRDMABase_m2m);
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	iprintk("VIQE1 : (0x%x)0x%p \n", sub_m2m_info->gVIQE_RDMA_num_m2m, sub_m2m_info->pRDMABase_m2m);
#else
	iprintk("sDeIntl: (0x%x)0x%p \n", sub_m2m_info->gDEINTLS_RDMA_num_m2m, sub_m2m_info->pRDMABase_m2m);
#endif

	return 0;
}

/*****************************************************************************
 * TCC_VIQE Probe
 ******************************************************************************/
static int tcc_viqe_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *viqe_common, *lcd_60hz_node, *external_60Hz_node, *external_sub_60Hz_node;
	struct device_node *m2m_deintl_main_node, *m2m_deintl_sub_node, *m2m_scaler_main_node, *m2m_scaler_sub_node;

#ifdef CONFIG_OF
	//viqe_common
	viqe_common = of_find_node_by_name(dev->of_node, "tcc_viqe_viqe_common");
	if(!viqe_common) {
		pr_err("[ERR][VIQE] could not find viqe_common node\n");
		return -ENODEV;
	}
	tcc_viqe_commom_dt_parse(viqe_common, &viqe_common_info);

	//viqe_60hz_info
	lcd_60hz_node = of_find_node_by_name(dev->of_node, "tcc_video_viqe_lcd");
	external_60Hz_node = of_find_node_by_name(dev->of_node, "tcc_video_viqe_external");
	external_sub_60Hz_node = of_find_node_by_name(dev->of_node, "tcc_video_viqe_external_sub");

	if(viqe_common_info.gBoard_stb == 0) {
		if (!lcd_60hz_node) {
			pr_err("[ERR][VIQE] could not find viqe_lcd node\n");
			return -ENODEV;
		}
	} else {
		if (!external_60Hz_node) {
			pr_err("[ERR][VIQE] could not find viqe_external node\n");
			return -ENODEV;
		}
		if (!external_sub_60Hz_node) {
			pr_err("[ERR][VIQE] could not find viqe_external node\n");
			return -ENODEV;
		}
	}

	#ifdef CONFIG_USE_SUB_MULTI_FRAME
	atomic_set(&viqe_sub_m2m_init, 0);
	INIT_LIST_HEAD(&video_queue_list);
	#endif

	tcc_viqe_60hz_dt_parse(lcd_60hz_node, &viqe_60hz_lcd_info);
	tcc_viqe_60hz_dt_parse(external_60Hz_node, &viqe_60hz_external_info);
	tcc_viqe_60hz_dt_parse(external_sub_60Hz_node, &viqe_60hz_external_sub_info);

	//m2m_viqe
	m2m_deintl_main_node = of_find_node_by_name(dev->of_node, "tcc_video_main_m2m");
	m2m_deintl_sub_node = of_find_node_by_name(dev->of_node, "tcc_video_sub_m2m");

	m2m_scaler_main_node = of_find_node_by_name(dev->of_node, "tcc_video_scaler_main_m2m");
	m2m_scaler_sub_node = of_find_node_by_name(dev->of_node, "tcc_video_scaler_sub_m2m");

	if(!m2m_deintl_main_node || !m2m_scaler_main_node || !m2m_deintl_sub_node || !m2m_scaler_sub_node) {
		pr_err("[ERR][VIQE] could not find m2m_viqe node\n");
		return -ENODEV;
	}

	tcc_viqe_m2m_dt_parse(m2m_deintl_main_node, m2m_deintl_sub_node, m2m_scaler_main_node, m2m_scaler_sub_node, &main_m2m_info, &sub_m2m_info);
#endif

	return 0;
}

/*****************************************************************************
 * TCC_VIQE Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id tcc_viqe_of_match[] = {
	{.compatible = "telechips,tcc_viqe", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_viqe_of_match);
#endif

static struct platform_driver tcc_viqe = {
	.probe	= tcc_viqe_probe,
	.driver	= {
	.name	= "tcc_viqe",
	.owner	= THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = tcc_viqe_of_match,
#endif
	},
};

static int __init tcc_viqe_init(void)
{
	return platform_driver_register(&tcc_viqe);
}

static void __exit tcc_viqe_exit(void)
{
	platform_driver_unregister(&tcc_viqe);
}

module_init(tcc_viqe_init);
module_exit(tcc_viqe_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC viqe driver");
MODULE_LICENSE("GPL");

