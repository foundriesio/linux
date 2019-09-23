/*
 * linux/drivers/video/tcc/tcc_vsync.c
 *
 * Based on:    Based on s3c2410fb.c, sa1100fb.c and others
 * Author:  <linux@telechips.com>
 * Created: Aug  2014
 * Description: TCC Vsync Driver
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
#include <asm/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include <soc/tcc/pmap.h>
#include <soc/tcc/timer.h>
#include <video/tcc/tcc_types.h>

#include <video/tcc/tccfb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_vsync_ioctl.h>
#include <video/tcc/tcc_composite_ioctl.h>
#include <video/tcc/tcc_component_ioctl.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tcc_lut_ioctl.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_lut.h>

#ifdef CONFIG_HDMI_DISPLAY_LASTFRAME
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_rdma.h>
#endif

#include <video/tcc/tca_display_config.h>

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>

extern unsigned int DV_PROC_CHECK;
#define LAST_FRAME_FORMAT TCC_LCDC_IMG_FMT_YUYV//TCC_LCDC_IMG_FMT_RGB888_3
#else
#define LAST_FRAME_FORMAT TCC_LCDC_IMG_FMT_YUYV
#endif

#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
#include "tcc_vioc_viqe_interface.h"
#include "viqe.h"
#endif

#include "tcc_vsync.h"

#define DEVICE_NAME "tcc_vsync"

#define ON_ENABLE 1
#define OFF_DISABLE 0

tcc_video_disp tccvid_vsync[VSYNC_MAX];

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)

#include <video/tcc/tcc_wmixer_ioctrl.h>
#include <video/tcc/tcc_scaler_ioctrl.h>

spinlock_t LastFrame_lockDisp;
tcc_video_lastframe tccvid_lastframe[VSYNC_MAX];

#define WMIXER_PATH	"/dev/wmixer1"
//#define USE_SCALER2_FOR_LASTFRAME
#ifdef USE_SCALER2_FOR_LASTFRAME
#define SCALER_PATH	"/dev/scaler2"
#endif
extern void tcc_video_rdma_off(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo, int outputMode, int interlaced);

extern int tcc_ctrl_ext_frame(char enable);
#ifdef CONFIG_DISPLAY_EXT_FRAME
extern volatile void __iomem* tcc_video_ext_display(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);
static int bExt_Frame = 0;
static int bCamera_mode = 0;
static int bToggleBuffer = 0;
int restricted_ExtFrame = 0;
struct mutex ext_mutex;
volatile void __iomem * pExtFrame_RDMABase = NULL;
static struct tcc_lcdc_image_update last_backup;
#endif
#endif


#if defined(CONFIG_TCC_VSYNC_DRV_CONTROL_LUT)
#include <video/tcc/tcc_lut_ioctl.h>
#define UI_LUT 			LUT_COMP0
#define UI_LUT_PLUGIN_CH 	0
#define VIDEO_LUT 		LUT_COMP1
#define VIDEO_LUT_PLUGIN_CH 	3

static int debug_lut = 0;
#define lut_prinfo(msg, ...) if (debug_lut) { pr_info("\x1b[1;38m TCC VSYNC: \x1b[0m" msg, ##__VA_ARGS__); }

struct vsync_lut_into_t {
	int content; /* 0: SDR, 1: HDR */
	int output; /* 0: SDR, 1: HDR */
};

static struct vsync_lut_into_t vsync_lut_into = {0, };

extern int lut_drv_api_get_plugin(unsigned int lut_number);
extern int lut_drv_api_set_plugin(unsigned int lut_number, int plugin, int plug_in_ch);
#endif

/*******			define extern symbol	******/
#if defined(CONFIG_FB_TCC_COMPOSITE)
extern void tcc_composite_set_bypass(char bypass_mode);
#endif

#if defined(CONFIG_TCC_DISPLAY_MODE_USE) && !defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT)
extern void tca_fb_attach_update_flag(void);
#endif

extern int tca_fb_divide_get_status(void);

extern void tca_lcdc_set_onthefly(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);
extern void tca_scale_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);
extern void tca_mvc_display_update(char hdmi_lcdc, struct tcc_lcdc_image_update *ImageInfo);
extern struct tcc_dp_device *tca_fb_get_displayType(TCC_OUTPUT_TYPE check_type);

#if defined(CONFIG_TCC_VTA)
extern int vta_cmd_notify_change_status(const char *);
#endif

extern TCC_OUTPUT_TYPE Output_SelectMode;
extern unsigned int HDMI_video_hz;

#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
static int vsync_started_device = LCD_START_VSYNC;
#endif

static unsigned int EX_OUT_LCDC;
static unsigned int LCD_LCDC_NUM;
static char vsync_power_state;
static int lcdc_interrupt_onoff = 0;
static int video_display_disable_check = 0;

static int debug_v = 0;
#define vprintk(msg...) if (debug_v) { printk( "tcc_vsync: " msg); }
static int debug = 0;
#define dprintk(msg...) if (debug) { printk( "tcc_vsync: " msg); }
#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
#define dprintk_drop(msg...) if (1) { printk( "tcc_vsync: " msg); }
#define dprintk_dv_transition(msg...) if (0) { printk( "dv_transition: " msg); }
#else
#define dprintk_drop(msg...) if (debug) { printk( "tcc_vsync: " msg); }
#endif

//#define DEBUG_VSYNC_FRAME
#ifdef DEBUG_VSYNC_FRAME
//#define USE_ALWAYS_UNPLUGGED_INT
#define GAP_CNT 60
#endif

#if 0
static int testToggleBit1=0;
static int testToggleBit=0;
#endif

inline static int tcc_vsync_get_time(tcc_video_disp *p);
inline static void tcc_vsync_set_time(tcc_video_disp *p, int time);
#define USE_VSYNC_TIMER

//#define USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT

// if time gab of video frame over base time is bigger than this value, that frame is skipped.
#define VIDEO_SYNC_MARGIN_LATE		50
// if time gap of video frame over base time is less than this value, that frame is displayed immediately
#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) && defined(DV_PREVENT_FRAME_DROP_AND_DUPLICATION)
#define VIDEO_SYNC_MARGIN_EARLY 	50
#else
#define VIDEO_SYNC_MARGIN_EARLY 	50
#endif
#define GAP_FOR_RESET_SYNC_TIME 	200

#define TIME_MARK_SKIP				(-1)

spinlock_t vsync_lock ;
spinlock_t vsync_lockDisp ;

int vsync_started = 0;

DECLARE_WAIT_QUEUE_HEAD( wq_consume ) ;

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
static struct work_struct dolby_out_change_work_q;
#endif

#ifdef USE_VSYNC_TIMER
static struct tcc_timer *vsync_timer = NULL;
static int g_is_vsync0_opened = 0;
static int g_is_vsync1_opened = 0;
#ifdef CONFIG_USE_SUB_MULTI_FRAME
static int g_is_vsync2_opened = 0;
static int g_is_vsync3_opened = 0;
#endif
#endif
#ifdef USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT
static struct tcc_timer *vsync_unpluged_int_timer = NULL;
static int g_is_unplugged_int_enabled = 0;
#endif

struct tcc_vsync_display_info_t vsync_vioc1_disp;
struct tcc_vsync_display_info_t vsync_vioc0_disp;

static int tcc_vsync_mvc_status = 0;
static int tcc_vsync_mvc_overlay_priority = 0;


#if defined(CONFIG_TCC_VSYNC_DRV_CONTROL_LUT)
static int vsync_deinit_lut(void)
{
	int lut_status = lut_drv_api_get_plugin(VIDEO_LUT);

	lut_prinfo("%s \r\n", __func__);
	if(lut_status == (VIOC_RDMA00 + UI_LUT_PLUGIN_CH)) {
		lut_prinfo("%s ulplug video_lut \r\n", __func__);
		lut_drv_api_set_plugin(VIDEO_LUT, 0, VIDEO_LUT_PLUGIN_CH);
	}
	memset(&vsync_lut_into, 0, sizeof(vsync_lut_into));
}

static int vsync_process_lut(tcc_video_disp *p, struct tcc_lcdc_image_update *pImage)
{
	int ret = -1;
	int lut_status, content, output;
	hevc_userdata_output_t *uData = NULL;
	do {
		if(p == NULL) {
			break;
		}
		if(pImage == NULL) {
			break;
		}

		uData = &pImage->private_data.userData_Info;

		/* check vsync start */
		if(p->isVsyncRunning == 0) {
			break;
		}

		if( TCC_OUTPUT_HDMI != Output_SelectMode ) {
			break;
		}

		/* Check current state */
		lut_status = lut_drv_api_get_plugin(UI_LUT);
		if(lut_status == (VIOC_RDMA00 + UI_LUT_PLUGIN_CH)) {
			/* Display is HDR : SDR to HDR */
			output = 1;
			lut_prinfo("display is HDR\r\n");
		} else {
			/* Display is SDR : SDR to SDR */
			output = 0;
			lut_prinfo("display is SDR\r\n");
		}

		/* HDR support */
		if( pImage != NULL &&
			pImage->private_data.optional_info[VID_OPT_HAVE_USERDATA] == 1 ) {
			/* Content is HDR */
			content = 1;
			//lut_prinfo("content is HDR\r\n");
		} else {
			/* Content is SDR */
			content = 0;
			//lut_prinfo("content is SDR\r\n");
		}

		if(vsync_lut_into.content != content ||
				vsync_lut_into.output != output) {

			if(content == 0 && output == 1) {
				/* SDR -> HDR */
				lut_prinfo("%s contents is SDR, display is HDR >> plugin video_lut \r\n", __func__);
				lut_drv_api_set_plugin(VIDEO_LUT, 1, VIDEO_LUT_PLUGIN_CH);
			} else {
				lut_prinfo("%s contents is %s, display is Ts >> unplug video_lut \r\n",
						__func__, (content==1)?"HDR":"SDR", (output==1)?"HDR":"SDR");
				lut_drv_api_set_plugin(VIDEO_LUT, 0, VIDEO_LUT_PLUGIN_CH);
			}

			/* Store current stae */
			vsync_lut_into.content = content;
			vsync_lut_into.output = output;
		}
	} while(0);
}
#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
#include "../../../char/hdmi_v2_0/include/hdmi_ioctls.h"

#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
#define DV_PREVENT_FRAME_DROP_AND_DUPLICATION // To prevent frame dropping or duplicating!!

//static long long nPrevTS = 0;
//static long long nBaseTS = 0;
//#define GAP_TS_MS (33)
static int bStart_DV_Display = 0;
static int b1st_frame_id = 0;
static int b1st_frame_display_count = 0;
extern void tca_edr_inc_check_count(unsigned int nInt, unsigned int nTry, unsigned int nProc, unsigned int nUpdated, unsigned int bInit_all);

#define DV_TRANSITION_SKIP_CNT (4)
static int nDolbyTransition_Skipped_Count = 0;
static int nDolbyTransition_VSIF = 0;
extern int hdmi_api_vsif_update_by_index(int index);
extern void hdmi_api_AvMute(int enable);
extern int hdmi_api_update_colorimetry(colorimetry_t colorimetry,  ext_colorimetry_t ext_colorimetry );
extern int hdmi_api_update_quantization(int quantization_range);
#endif

static DRM_Packet_t gDRM_packet;
extern void hdmi_set_drm(DRM_Packet_t * drmparm);
extern void hdmi_clear_drm(void);
void set_hdmi_drm(HDMI_DRM_MODE mode, struct tcc_lcdc_image_update *pImage, unsigned int layer)
{
#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) || defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	DRM_Packet_t drmparm;
	hevc_userdata_output_t *uData = NULL;

	dprintk("===========> set_hdmi_drm(%d), layer(%d), Output_SelectMode(%d) \n", mode, layer, Output_SelectMode);
	if( layer != RDMA_VIDEO )
		return;

	if( TCC_OUTPUT_HDMI != Output_SelectMode )
		return;

	if( pImage )
		uData = &pImage->private_data.userData_Info;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH() && (HDR10 != vioc_get_out_type())){
		memset(&gDRM_packet, 0x0, sizeof(DRM_Packet_t));
		return;
	}
#endif

	if( mode == DRM_ON )
	{
		if(!uData)
			return;

		/* HDR support */
		if( pImage->private_data.optional_info[VID_OPT_HAVE_USERDATA] == 1 ) {
			dprintk(" HDR setting for HDMI-Drm \n");
			memset(&drmparm, 0x0, sizeof(DRM_Packet_t));

			drmparm.mInfoFrame.version 								= uData->version;
			drmparm.mInfoFrame.length 								= uData->struct_size;
			drmparm.mDescriptor_type1.EOTF 							= uData->eotf;//2;
			drmparm.mDescriptor_type1.Descriptor_ID 				= 0;
			drmparm.mDescriptor_type1.disp_primaries_x[0] 			= uData->display_primaries_x[0];
			drmparm.mDescriptor_type1.disp_primaries_x[1] 			= uData->display_primaries_x[1];
			drmparm.mDescriptor_type1.disp_primaries_x[2] 			= uData->display_primaries_x[2];
			drmparm.mDescriptor_type1.disp_primaries_y[0] 			= uData->display_primaries_y[0];
			drmparm.mDescriptor_type1.disp_primaries_y[1] 			= uData->display_primaries_y[1];
			drmparm.mDescriptor_type1.disp_primaries_y[2] 			= uData->display_primaries_y[2];
			drmparm.mDescriptor_type1.white_point_x 				= uData->white_point_x;
			drmparm.mDescriptor_type1.white_point_y 				= uData->white_point_y;
			drmparm.mDescriptor_type1.max_disp_mastering_luminance 	= uData->max_display_mastering_luminance;
			drmparm.mDescriptor_type1.min_disp_mastering_luminance 	= uData->min_display_mastering_luminance;
			drmparm.mDescriptor_type1.max_content_light_level 		= uData->max_content_light_level;
			drmparm.mDescriptor_type1.max_frame_avr_light_level 	= uData->max_pic_average_light_level;

			if( memcmp(&gDRM_packet, &drmparm, sizeof(DRM_Packet_t)) ) {
				dprintk("\x1b[1;31m[%s][%d] HEVC userData \x1b[0m\n", __func__, __LINE__);
				dprintk("\x1b[1;31m[%s][%d] ver %d size %d eotf(%d) \x1b[0m\n", __func__, __LINE__, uData->version, uData->struct_size, uData->eotf);
				dprintk("\x1b[1;31m[%s][%d] colour_primaries %d transfer %d matrix_coefficients %d\x1b[0m\n", __func__, __LINE__, uData->colour_primaries, uData->transfer_characteristics, uData->matrix_coefficients);
				dprintk("\x1b[1;31m[%s][%d] primaries_x[0] %d [1] %d [2] %d \x1b[0m\n", __func__, __LINE__, uData->display_primaries_x[0], uData->display_primaries_x[1], uData->display_primaries_x[2]);
				dprintk("\x1b[1;31m[%s][%d] primaries_y[0] %d [1] %d [2] %d \x1b[0m\n", __func__, __LINE__, uData->display_primaries_y[0], uData->display_primaries_y[1], uData->display_primaries_y[2]);
				dprintk("\x1b[1;31m[%s][%d] white point x %d y %d luminance max %d min %d \x1b[0m\n", __func__, __LINE__, uData->white_point_x, uData->white_point_y, uData->max_display_mastering_luminance, uData->min_display_mastering_luminance);
				dprintk("\x1b[1;31m[%s][%d] light level max %d average %d\x1b[0m\n", __func__, __LINE__, uData->max_content_light_level, uData->max_pic_average_light_level);
				hdmi_set_drm(&drmparm);
				memcpy(&gDRM_packet, &drmparm, sizeof(DRM_Packet_t));
			}
		}
	}
	else if( mode == DRM_OFF )
	{
		dprintk("===========> set_hdmi_drm(DRM_OFF) \n");
		memset(&drmparm, 0x0, sizeof(DRM_Packet_t));
//		hdmi_set_drm(&drmparm);
	}
	else
	{
		dprintk("===========> set_hdmi_drm(DRM_INIT) \n");
		memset(&gDRM_packet, 0x0, sizeof(DRM_Packet_t));
	}
#endif
}
#endif

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
VSYNC_CH_TYPE tcc_vsync_get_video_ch_type(unsigned int lcdc_layer)
{
	VSYNC_CH_TYPE type;

	if(lcdc_layer == RDMA_VIDEO)
		type = VSYNC_MAIN;
	else if(lcdc_layer == RDMA_VIDEO_SUB)
		type = VSYNC_SUB0;
	else
	{
		dprintk("error :: strange lcdc_layer(%d) \n", lcdc_layer);
		type = VSYNC_MAX;
	}
	return type;	
}
#endif

static int tcc_vsync_set_max_buffer(tcc_vsync_buffer_t * buffer_t, int buffer_count)
{
	buffer_t->max_buff_num = buffer_count;
	return buffer_count;
}

static int _tcc_vsync_print_all_buffers(tcc_vsync_buffer_t * buffer_t)
{
	int i = 0;

	for(i = 0; i < VPU_BUFFER_MANAGE_COUNT; i++)
	{
		printk("stImage[%d] :%s: ID(%d), TS(%d), Sync(%d) \n", i, (i == buffer_t->readIdx) ? "r" : "n",
					buffer_t->stImage[i].buffer_unique_id, buffer_t->stImage[i].time_stamp, buffer_t->stImage[i].sync_time);
	}
}

static int tcc_vsync_push_buffer(tcc_video_disp *p, tcc_vsync_buffer_t * buffer_t, struct tcc_lcdc_image_update* inputData)
{

	if(atomic_read(&buffer_t->valid_buff_count) >= buffer_t->max_buff_num || atomic_read( &buffer_t->readable_buff_count) >= buffer_t->max_buff_num)
	{
		printk("error: buffer full %d, max %d %d ts %d sync %d (%d)\n",
				atomic_read(&buffer_t->valid_buff_count), buffer_t->max_buff_num,
				atomic_read( &buffer_t->readable_buff_count),inputData->time_stamp, inputData->sync_time, tcc_vsync_get_time(p));
		_tcc_vsync_print_all_buffers(buffer_t);
		return -1;
	}

	memcpy(&(buffer_t->stImage[buffer_t->writeIdx]),(void*)inputData, sizeof(struct tcc_lcdc_image_update) );
	
	if(++buffer_t->writeIdx >= buffer_t->max_buff_num)
		buffer_t->writeIdx = 0;
	

	atomic_inc( &buffer_t->valid_buff_count);
	atomic_inc( &buffer_t->readable_buff_count);
	
	return 0;
}

inline static int tcc_vsync_pop_buffer(tcc_vsync_buffer_t * buffer_t)
{
	if(atomic_read( &buffer_t->readable_buff_count) == 0)
	{
		printk("error: buffer empty \n");
		return -1;
	}
	else if (atomic_read(&buffer_t->readable_buff_count) < 0)
	{
		printk("%s: readable_buff_count<0\n", __func__);
		atomic_set(&buffer_t->readable_buff_count, 0);
		return -1;
	}

	if(++buffer_t->readIdx >= buffer_t->max_buff_num)
		buffer_t->readIdx = 0;

	atomic_dec(&buffer_t->readable_buff_count);

	return atomic_read( &buffer_t->readable_buff_count);
}

inline static void* tcc_vsync_get_buffer(tcc_vsync_buffer_t * buffer_t, int offset)
{
	int readIdx;

	if((atomic_read( &buffer_t->readable_buff_count)-offset) > 0)
	{
		readIdx = buffer_t->readIdx;

		while(offset)
		{
			if(++readIdx >= buffer_t->max_buff_num)
				readIdx = 0;

			offset--;
		}

		return (void*)&(buffer_t->stImage[readIdx]);
	}
	else
	{
		return 0;
	}
}

inline static int tcc_vsync_clean_buffer(tcc_vsync_buffer_t * buffer_t)
{
	if(buffer_t->readIdx == buffer_t->clearIdx || atomic_read(&buffer_t->valid_buff_count) == 0)
	{
		vprintk("error: no clean buffer clearIdx(%d) valid_buff_count(%d) \n",	buffer_t->clearIdx,atomic_read(&buffer_t->valid_buff_count) );
		return -1;
	}

	vprintk("tcc_vsync_clean_buffer start clearIdx(%d) readIdx(%d) writeIdx(%d) valid_buff_count %d  \n", buffer_t->clearIdx,buffer_t->readIdx,buffer_t->writeIdx,atomic_read(&buffer_t->valid_buff_count));
	do
	{
		if(++buffer_t->clearIdx >= buffer_t->max_buff_num)
			buffer_t->clearIdx = 0;
		
		if(buffer_t->last_cleared_buff_id < buffer_t->stImage[buffer_t->clearIdx].buffer_unique_id)
			buffer_t->last_cleared_buff_id = buffer_t->stImage[buffer_t->clearIdx].buffer_unique_id;

		atomic_dec( &buffer_t->valid_buff_count);
		if (atomic_read(&buffer_t->valid_buff_count) < 0) {
			printk("%s: valid_buff_count<0\n", __func__);
			atomic_set(&buffer_t->valid_buff_count, 0);
		}


	}while(buffer_t->readIdx != buffer_t->clearIdx);
	wake_up_interruptible( &wq_consume ) ;
	vprintk("tcc_vsync_clean_buffer valid_buff_count %d  \n", atomic_read(&buffer_t->valid_buff_count));
	
	return atomic_read(&buffer_t->valid_buff_count);
}

static int tcc_vsync_pop_all_buffer(tcc_vsync_buffer_t * buffer_t, int who_call)
{

	if(atomic_read(&buffer_t->valid_buff_count) == 0)
	{
		vprintk("error: buffer empty \n");
		return -1;
	}
	else if (atomic_read(&buffer_t->valid_buff_count) < 0)
	{
		printk("%s: valid_buff_count<0\n", __func__);
		atomic_set(&buffer_t->valid_buff_count, 0);
	}

	while(atomic_read(&buffer_t->valid_buff_count) > 0)
	{
		if(buffer_t->last_cleared_buff_id < buffer_t->stImage[buffer_t->clearIdx].buffer_unique_id)
			buffer_t->last_cleared_buff_id = buffer_t->stImage[buffer_t->clearIdx].buffer_unique_id;

		if(++buffer_t->clearIdx >= buffer_t->max_buff_num)
			buffer_t->clearIdx = 0;

		atomic_dec( &buffer_t->valid_buff_count);
	}

	buffer_t->writeIdx = buffer_t->readIdx = buffer_t->clearIdx;
	atomic_set( &buffer_t->readable_buff_count,0);
	
	printk("[%d] pop_all_buffer readIdx %d writeIdx %d clearIdx %d valid %d readable %d \n", who_call,
				buffer_t->readIdx,buffer_t->writeIdx,buffer_t->clearIdx, atomic_read(&buffer_t->valid_buff_count), atomic_read( &buffer_t->readable_buff_count));
	return 0;
}

static int tcc_vsync_is_full_buffer(tcc_vsync_buffer_t * buffer_t)
{
	if(atomic_read(&buffer_t->valid_buff_count) >= buffer_t->max_buff_num)
		return 1;
	else
		return 0;
}

inline static int tcc_vsync_is_empty_buffer(tcc_vsync_buffer_t * buffer_t)
{
	if(atomic_read(&buffer_t->valid_buff_count) > 0)
		return 0;
	else
		return 1;
}

static void tcc_vsync_push_render_frame_for_duplicated_interlace(tcc_video_disp *p)
{
	int readCount = 0;
	int readIdx = p->vsync_buffer.readIdx;
	struct tcc_lcdc_image_update *input_image_info = NULL;
	int readable_count = atomic_read( &p->vsync_buffer.readable_buff_count );

	if(readable_count == 0)
		return;

	if(!(p->duplicateUseFlag && (p->deinterlace_mode && !p->interlace_bypass_lcdc) && (!p->output_toMemory || p->m2m_mode)))
	{
		return;
	}

	while(readCount < readable_count)
	{
		input_image_info = &(p->vsync_buffer.stImage[readIdx]);
		if(input_image_info->viqe_queued == 0)
		{
			int curTime = tcc_vsync_get_time(p);
			int reset_frmCnt = VIQE_RESET_NONE;

			// resolution change support
			if((p->prevImageWidth != 0) && (p->prevImageHeight != 0)) {
				int image_width = input_image_info->crop_right - ((input_image_info->crop_left >> 3) << 3);
				int image_height = input_image_info->crop_bottom - input_image_info->crop_top;

				if((p->prevImageWidth != image_width) || (p->prevImageHeight != image_height)) {
					vprintk("Interlaced_Res_Changed for render_field()::  %d x %d -> %d x %d \n", p->prevImageWidth, p->prevImageHeight, image_width, image_height);
					p->prevImageWidth = image_width;
					p->prevImageHeight = image_height;

					reset_frmCnt = VIQE_RESET_CROP_CHANGED;	// SoC guide info.
				} else {
					reset_frmCnt = VIQE_RESET_NONE;
				}
			} else {
				p->prevImageWidth = input_image_info->crop_right - ((input_image_info->crop_left >> 3) << 3);;
				p->prevImageHeight = input_image_info->crop_bottom - input_image_info->crop_top;;
				reset_frmCnt = VIQE_RESET_NONE;
			}

			if(Output_SelectMode != TCC_OUTPUT_NONE) {
				if ((p->duplicateUseFlag) && (input_image_info->Lcdc_layer == RDMA_VIDEO)){
					vprintk("%s: =============================> VIQE-Queued :: unique_id(%d) \n", input_image_info->Lcdc_layer == RDMA_VIDEO ? "M" : "S", input_image_info->buffer_unique_id);
					viqe_render_frame(input_image_info, p->vsync_interval, curTime, reset_frmCnt);
				}
			}

			input_image_info->viqe_queued = 1;
		}

		if(++readIdx >= p->vsync_buffer.max_buff_num)
			readIdx = 0;
		readCount++;

	}

}

static void tcc_vsync_check_interlace_output(tcc_video_disp *p, struct tcc_dp_device *pdp_data)
{
	if((__raw_readl(pdp_data->ddc_info.virt_addr+DCTRL) & DCTRL_NI_MASK))
		p->interlace_output = 0;
	else
		p->interlace_output = 1;
}

static int tcc_vsync_bypass_frame(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *pImage)
{
	unsigned int lstatus = 0;
	int ret = 0;
	
	if (pdp_data->ddc_info.virt_addr == NULL) {
		pr_err("%s: pdp_data->ddc_info.virt_addr is null\n", __func__);
		return PTR_ERR((void*)pdp_data->ddc_info.virt_addr);
	}
	
	lstatus = __raw_readl(pdp_data->ddc_info.virt_addr+DSTATUS);
	if(pImage->odd_first_flag==0){
		if(p->nDeinterProcCount ==0){
			if(lstatus & VIOC_DISP_IREQ_TFIELD_MASK)
			{
				ret = 1;
			}
		}
		else{
			if(!(lstatus & VIOC_DISP_IREQ_TFIELD_MASK))
			{
				ret = 1;
			}
		}
	}
	else{

		if(p->nDeinterProcCount ==1){
			if(lstatus & VIOC_DISP_IREQ_TFIELD_MASK)
			{
				ret = 1;
			}
		}
		else{
			if(!(lstatus & VIOC_DISP_IREQ_TFIELD_MASK))
			{
				ret = 1;
			}
		}
	}
	
	if(ret == 1){
		//printk("nDeinterProcCount(%d), odd_first_flag(%d)\n", tccvid_vsync.nDeinterProcCount, pImage->odd_first_flag) ;
		return ret;
	}
	
	switch(Output_SelectMode)
	{
		case TCC_OUTPUT_NONE:
			break;
#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
		case TCC_OUTPUT_LCD:
			tca_scale_display_update(pdp_data, pImage);
			break;
#endif
		case TCC_OUTPUT_HDMI:
		case TCC_OUTPUT_COMPONENT:
		case TCC_OUTPUT_COMPOSITE:
			tca_scale_display_update(pdp_data, pImage);
			break;

		default:
			break;
	}

	return ret;
}

static int tcc_vsync_calc_early_margin(tcc_video_disp *p)
{
#ifdef CONFIG_ANDROID
	return VIDEO_SYNC_MARGIN_EARLY;
#else
	int alpha_value = 10;

	if(p->video_frame_rate > 0)
		return -(1000/p->video_frame_rate - alpha_value);

	return 32-alpha_value;
#endif
}

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
static void tcc_video_change_dolby_out_wq_isr(struct work_struct *work)
{
//	struct tcc_vsync_display_info_t *vsync = container_of(work, struct tcc_vsync_display_info_t, dolby_out_change_work_q);

//	dprintk_dv_transition("%s-%d :: irq(%d), lcdc(%d) :: ref_count(%d) !!!\n", __func__, __LINE__,
//				vsync->irq_num, vsync->lcdc_num, nDolbyTransition_Skipped_Count);

	if(nDolbyTransition_Skipped_Count >= 0)
	{
		if(nDolbyTransition_Skipped_Count == (DV_TRANSITION_SKIP_CNT - 2)){
			OUT_TYPE dv_out_type = vioc_get_out_type();
			int rgb_tunneling = vioc_v_dv_is_rgb_tunneling();
			int out_colorimetry = 0, out_ext_colorimetry = 0;
			int out_quan_val = 0;

			switch(dv_out_type)
			{
				case 2: // SDR
					out_colorimetry = ITU709;
					out_quan_val = 1; // limited
					break;

				case 1: // HDR10
					out_colorimetry = EXTENDED_COLORIMETRY;
					if(rgb_tunneling)
						out_ext_colorimetry = BT2020YCBCR;
					else
						out_ext_colorimetry = BT2020YCCBCR;
					out_quan_val = 1;
					break;

				case 0: // DV
					out_colorimetry = EXTENDED_COLORIMETRY;
					if(rgb_tunneling)
						out_ext_colorimetry = BT2020YCBCR;
					else
						out_ext_colorimetry = BT2020YCCBCR;
					out_quan_val = 2; // full
					break;

				default:
				case 3: // DV_LL
					out_colorimetry = EXTENDED_COLORIMETRY;
					if(rgb_tunneling)
						out_ext_colorimetry = BT2020YCBCR;
					else
						out_ext_colorimetry = BT2020YCCBCR;
					out_quan_val = 1;
					break;
			}
			hdmi_api_update_colorimetry(out_colorimetry, out_ext_colorimetry);
			hdmi_api_update_quantization(out_quan_val);  //RGB:YUV = 2: full/full, 1: limited/limited, 0:default/limited ?
			/*dprintk_dv_transition*/printk("Out Format: %d, RGB_Tunneling: %d => color(%d/%d), quan(%d)",
					 dv_out_type, rgb_tunneling, out_colorimetry, out_ext_colorimetry, out_quan_val);

			hdmi_api_vsif_update_by_index(nDolbyTransition_VSIF);
			dprintk_dv_transition("%s-%d :: ref_count(%d) vsif(%d) !!!\n", __func__, __LINE__, nDolbyTransition_Skipped_Count, nDolbyTransition_VSIF);
		}
		else{
			hdmi_api_AvMute(1);
			dprintk_dv_transition("%s-%d :: ref_count(%d) AV-mute !!!\n", __func__, __LINE__, nDolbyTransition_Skipped_Count);
		}
	}
	else
	{
		hdmi_api_AvMute(0);
	}
}
#endif

static void tcc_vsync_display_update(tcc_video_disp *p)
{
	int current_time;
	int time_gap;
	int readable_buff_cnt;
	int i;
	int popped_count = 0;
	struct tcc_lcdc_image_update *pPrevImage;
	struct tcc_lcdc_image_update *pNextImage;

	current_time = tcc_vsync_get_time(p);
	tcc_vsync_clean_buffer(&p->vsync_buffer);

	readable_buff_cnt = tcc_video_get_readable_count(p);
#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) && defined(DV_PREVENT_FRAME_DROP_AND_DUPLICATION)
	if( readable_buff_cnt < 2 && !bStart_DV_Display )
		return;
	if( bStart_DV_Display && b1st_frame_display_count++ < 3 && p->vsync_buffer.curr_displaying_imgInfo.buffer_unique_id == b1st_frame_id)
	{
		tca_scale_display_update(tca_fb_get_displayType(Output_SelectMode), &p->vsync_buffer.curr_displaying_imgInfo);
		return;
	}
#endif

	readable_buff_cnt--;
	if(readable_buff_cnt < 0) {
		dprintk("readable: %d, valid: %d \n", readable_buff_cnt, tcc_video_get_valid_count(p));
	}

	for(i=0; i < readable_buff_cnt; i++)
	{
		pNextImage = tcc_vsync_get_buffer(&p->vsync_buffer, 0);
		if (pNextImage == NULL) {
			pr_info("%s: %d,%d,%d\n", __func__, i, readable_buff_cnt, atomic_read(&p->vsync_buffer.readable_buff_count));
			return;
		}
		vprintk("[%d/%d] ID[%d] pNextImage->time_stamp : %d / %d, %d\n", i, readable_buff_cnt, pNextImage->buffer_unique_id, pNextImage->time_stamp, pNextImage->sync_time,current_time) ;

		time_gap = (current_time+tcc_vsync_calc_early_margin(p)) - pNextImage->time_stamp;
#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) && defined(DV_PREVENT_FRAME_DROP_AND_DUPLICATION)
		if( !bStart_DV_Display )
			break;
#endif

		if(time_gap >= 0)
		{
			if (tcc_vsync_pop_buffer(&p->vsync_buffer) < 0) {
				if(popped_count > 0){
					dprintk_drop("%s:0 skipped buffer(%d) and no display \n", p->pIntlNextImage->Lcdc_layer == RDMA_VIDEO ? "M" : "S", popped_count);
				}
				return;
			}
			popped_count++;

#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) && defined(DV_PREVENT_FRAME_DROP_AND_DUPLICATION)
			if( bStart_DV_Display )
				break;
#endif

			if(p->perfect_vsync_flag == 1 && time_gap < 4 )
			{
				tcc_vsync_set_time(p, tcc_vsync_get_time(p) + ((p->vsync_interval>>1)*p->time_gap_sign));
				if(p->time_gap_sign > 0)
					p->time_gap_sign = -1;
				else
					p->time_gap_sign = 1;
			}
		}
		else
		{
			break;
		}
	}

	if(popped_count > 1){
		dprintk_drop("%s:1 skipped buffer(p:[%d]) r:[%d -> %d]) \n",
				pNextImage->Lcdc_layer == RDMA_VIDEO ? "M" : "S", popped_count, readable_buff_cnt+1, tcc_video_get_readable_count(p));
	}

	pPrevImage = &p->vsync_buffer.curr_displaying_imgInfo;
	pNextImage = &p->vsync_buffer.stImage[p->vsync_buffer.readIdx];

#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
	if(pPrevImage->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO])
	{
		if(nDolbyTransition_Skipped_Count >= 0)
		{
			nDolbyTransition_Skipped_Count--;

			if(nDolbyTransition_Skipped_Count == (DV_TRANSITION_SKIP_CNT - 2))
			{
				//2.1 send command so that HDMI can change VSIF.
				if (schedule_work(&dolby_out_change_work_q) == 0 ) {
					pr_err("dolby_out :cannot schedule work !!!\n");
				}	
			}

			if(nDolbyTransition_Skipped_Count >= 2)
			{
				//2.2 skip frames during several(more than 2) vsync event. ...
				dprintk_dv_transition("[%d] FrameSkip for DV Transition!!\n", nDolbyTransition_Skipped_Count);
				return;
			}
			else if(nDolbyTransition_Skipped_Count >= 0)
			{
				//3. on 3rd vsync event, restart displaying. ...
				dprintk_dv_transition("[%d] Display for DV Transition!!\n", nDolbyTransition_Skipped_Count);
			}
			else if(nDolbyTransition_Skipped_Count < 0)
			{
				//4. AV Mute OFF
				if (schedule_work(&dolby_out_change_work_q) == 0 ) {
					pr_err("dolby_out :cannot schedule work !!!\n");
				}
			}
		}

		if( nDolbyTransition_Skipped_Count < 0 &&
			(pPrevImage->private_data.dolbyVision_info.reg_out_type != pNextImage->private_data.dolbyVision_info.reg_out_type))
		{
			dprintk_dv_transition("[%d]-%d DOLBY Output mode(%d) is changed into %d\n", vioc_get_out_type(),
					pNextImage->private_data.optional_info[VID_OPT_RESERVED_3],
					pPrevImage->private_data.dolbyVision_info.reg_out_type, pNextImage->private_data.dolbyVision_info.reg_out_type);
			//1. send command for AV mute on HDMI
			nDolbyTransition_Skipped_Count = DV_TRANSITION_SKIP_CNT;
			nDolbyTransition_VSIF = pNextImage->private_data.optional_info[VID_OPT_RESERVED_3];
			if (schedule_work(&dolby_out_change_work_q) == 0 ) {
				pr_err("dolby_out :cannot schedule work !!!\n");
			}
			return;
		}
	}
#endif

#ifdef DEBUG_VSYNC_FRAME
	if((pNextImage->buffer_unique_id % GAP_CNT) == 0)	
		printk("@-%d :: ID[%d] pNextImage->time_stamp : %d %d, Count(%d/%d), Out(%d/%d)\n", 
	#if defined(USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT)
			g_is_unplugged_int_enabled,
	#else
			0,
	#endif
			pNextImage->buffer_unique_id, pNextImage->time_stamp,current_time, tcc_video_get_readable_count(p), tcc_video_get_valid_count(p), Output_SelectMode, p->outputMode) ;
#endif

	if(p->outputMode == Output_SelectMode)
	{
		struct tcc_dp_device *dp_device = tca_fb_get_displayType(Output_SelectMode);
		//printk("mt(%d), remain_buff(%d) readIdx(%d) write(%d) readaddr(%x) cleared_buff_id(%d) buffer_unique_id %d\n", pNextImage->time_stamp, p->vsync_buffer.readable_buff_count,
		//	p->vsync_buffer.readIdx,p->vsync_buffer.writeIdx,pNextImage->addr0,p->vsync_buffer.last_cleared_buff_id,p->vsync_buffer.stImage[p->vsync_buffer.readIdx].buffer_unique_id) ;

		pNextImage->bSize_Changed = 0;
		if( pPrevImage->buffer_unique_id == pNextImage->buffer_unique_id ){
			if( pPrevImage->offset_x == pNextImage->offset_x &&
				pPrevImage->offset_y == pNextImage->offset_y &&
				pPrevImage->Image_width == pNextImage->Image_width &&
				pPrevImage->Image_height == pNextImage->Image_height &&
				pPrevImage->Frame_width == pNextImage->Frame_width &&
				pPrevImage->Frame_height == pNextImage->Frame_height &&
				pPrevImage->crop_left == pNextImage->crop_left &&
				pPrevImage->crop_top == pNextImage->crop_top &&
				pPrevImage->crop_right == pNextImage->crop_right &&
				pPrevImage->crop_bottom == pNextImage->crop_bottom
			){
//#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
//				if(!VIOC_CONFIG_DV_GET_EDR_PATH())
//					return;
//#endif
#ifdef CONFIG_USE_SUB_MULTI_FRAME
				switch(p->type) {
				case VSYNC_MAIN:
					/* TODO */
					break;
				case VSYNC_SUB0:
					TCC_VIQE_DI_Sub_Run60Hz_M2M(pNextImage, VIQE_RESET_NONE);
					break;
				case VSYNC_SUB1:
				case VSYNC_SUB2:
				case VSYNC_SUB3:
					if(pNextImage->mosaic_mode)
						TCC_VIQE_DI_Push60Hz_M2M(pNextImage, p->type);
					break;
				default:
					pr_info("Invalie driver type(%d)\n", p->type);
				}
#endif
				return;
			} else
				pNextImage->bSize_Changed = 1;
		} else {
			dprintk(" POP(%d) ====> [%d/%d] ID[%d] pNextImage->time_stamp : %d / %d, Gap = %d\n", popped_count, i, readable_buff_cnt, pNextImage->buffer_unique_id, pNextImage->time_stamp, current_time, pNextImage->time_stamp - current_time);	
		}

#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) && defined(DV_PREVENT_FRAME_DROP_AND_DUPLICATION)
		if( !bStart_DV_Display ){
			bStart_DV_Display = b1st_frame_display_count = 1;
			b1st_frame_id = pNextImage->buffer_unique_id;
		}
		pNextImage->private_data.optional_info[VID_OPT_RESERVED_2] = (readable_buff_cnt << 16) | tcc_video_get_valid_count(p);
		tca_edr_inc_check_count(0, 1, 0, 0, 0);
#endif

		memcpy(pPrevImage, pNextImage, sizeof(struct tcc_lcdc_image_update));

		if(p->skipFrameStatus){
			dprintk("skip status(%d) %d/0x%x => %d/0x%x, en: %d layer: %d out: %d\n", p->skipFrameStatus,
						pPrevImage->buffer_unique_id, pPrevImage->addr0, pNextImage->buffer_unique_id, pNextImage->addr0, pNextImage->enable, pNextImage->Lcdc_layer, Output_SelectMode);
		}

		if(pNextImage->Lcdc_layer == RDMA_FB)
			return;

#if 0
		if(testToggleBit)
		{
			PGPION hwGPIOC;
			testToggleBit = 0; 

			hwGPIOC = (volatile PGPION)tcc_p2v(HwGPIOC_BASE);
			//hwGPIOC->GPEN.nREG|= (unsigned int)(0x00004000);
			//hwGPIOC->GPDAT.nREG |= (unsigned int)(0x00004000);
			hwGPIOC->GPFN1.bREG.GPFN14 = 0;
			hwGPIOC->GPEN.bREG.GP14 = 1;
			hwGPIOC->GPDAT.bREG.GP14 = 1;

			
		}
		else
		{
			PGPION hwGPIOC;
			testToggleBit = 1;

			hwGPIOC = (volatile PGPION)tcc_p2v(HwGPIOC_BASE);
			//hwGPIOC->GPEN.nREG |= (unsigned int)(0x00004000);
			//hwGPIOC->GPDAT.nREG &= (unsigned int)(~0x00004000);
			hwGPIOC->GPFN1.bREG.GPFN14 = 0;
			hwGPIOC->GPEN.bREG.GP14 = 1;
			hwGPIOC->GPDAT.bREG.GP14 = 0;

		}
#endif

		if (p->output_toMemory && p->m2m_mode) {
		#ifdef CONFIG_USE_SUB_MULTI_FRAME
			switch(p->type) {
			case VSYNC_MAIN:
				TCC_VIQE_DI_Run60Hz_M2M(pNextImage, VIQE_RESET_NONE);
				break;
			case VSYNC_SUB0:
				TCC_VIQE_DI_Sub_Run60Hz_M2M(pNextImage, VIQE_RESET_NONE);
				break;
			case VSYNC_SUB1:
			case VSYNC_SUB2:
			case VSYNC_SUB3:
				if(pNextImage->mosaic_mode)
					TCC_VIQE_DI_Push60Hz_M2M(pNextImage, p->type);
				break;
			default:
				pr_info("Invalid driver type(%d)\n", p->type);
			}
		#else
			TCC_VIQE_DI_Run60Hz_M2M(pNextImage, VIQE_RESET_NONE);
		#endif
		} else {
			switch(Output_SelectMode)
			{
				case TCC_OUTPUT_NONE:
					break;

				#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
				case TCC_OUTPUT_LCD:
					tca_scale_display_update(dp_device, pNextImage);
					break;
				#endif

				case TCC_OUTPUT_HDMI:
					{
						//if(tcc_vsync_mvc_status )
						if(tcc_vsync_mvc_status && pNextImage->MVCframeView == 1 && pNextImage->MVC_Base_addr0)
						{
							//printk("@@@@@tcc_vsync_mvc_status=%d pNextImage->MVCframeView = %d, pNextImage->MVC_Base_addr0 = 0x%08x\n",tcc_vsync_mvc_status, pNextImage->MVCframeView, pNextImage->MVC_Base_addr0);
							tca_mvc_display_update(EX_OUT_LCDC, pNextImage);
						}
						else
						{
							tca_scale_display_update(dp_device, pNextImage);
						}
					}

					break;
				case TCC_OUTPUT_COMPONENT:
				case TCC_OUTPUT_COMPOSITE:
					tca_scale_display_update(dp_device, pNextImage);
					break;

				default:
					break;
			}
		}
	}
	return;
}

static void tcc_vsync_display_update_forDeinterlaced(tcc_video_disp *p)
{
	enum VIQE_RESET_REASON reset_frmCnt = VIQE_RESET_NONE;
	int current_time;
	int time_gap, time_diff;
	int readable_buff_cnt;
	int i;
	int popped_count = 0;
	struct tcc_lcdc_image_update *pPrevImage;

	current_time = tcc_vsync_get_time(p);
	tcc_vsync_push_render_frame_for_duplicated_interlace(p);

	if(p->nDeinterProcCount == 0)
	{
		tcc_vsync_clean_buffer(&p->vsync_buffer);

		readable_buff_cnt = tcc_video_get_readable_count(p);
		readable_buff_cnt--;
		if( readable_buff_cnt < 0 ){
			dprintk_drop("%s: no available buffer(%d) \n", p->pIntlNextImage->Lcdc_layer == RDMA_VIDEO ? "M" : "S", readable_buff_cnt);
		}

		for(i=0; i < readable_buff_cnt; i++)
		{
			p->pIntlNextImage = tcc_vsync_get_buffer(&p->vsync_buffer, 0);
			if (p->pIntlNextImage == NULL) {
				pr_info("%s: %d,%d,%d\n", __func__, i, readable_buff_cnt, atomic_read(&p->vsync_buffer.readable_buff_count));
				return;
			}
			vprintk("[%d/%d] ID[%d] p->pIntlNextImage->time_stamp : %d / %d, %d\n", i, readable_buff_cnt, p->pIntlNextImage->buffer_unique_id, p->pIntlNextImage->time_stamp, p->pIntlNextImage->sync_time,current_time) ;

			time_gap = (current_time+tcc_vsync_calc_early_margin(p)) - p->pIntlNextImage->time_stamp;
			if(time_gap >= 0)
			{
				if (tcc_vsync_pop_buffer(&p->vsync_buffer) < 0){
					if(popped_count > 0){
						dprintk_drop("%s:0 skipped buffer(%d) and no display \n", p->pIntlNextImage->Lcdc_layer == RDMA_VIDEO ? "M" : "S", popped_count);
					}
					return;
				}
				popped_count++;

				if(p->perfect_vsync_flag == 1 && time_gap < 4 )
				{
					tcc_vsync_set_time(p, tcc_vsync_get_time(p) + (p->vsync_interval>>1)*p->time_gap_sign);
					if(p->time_gap_sign > 0)
						p->time_gap_sign = -1;
					else
						p->time_gap_sign = 1;
				}
			}
			else
			{
				break;
			}
		}

		if(popped_count > 1){
			dprintk_drop("%s:1 skipped buffer(p:[%d]) r:[%d -> %d]) \n",
						p->pIntlNextImage->Lcdc_layer == RDMA_VIDEO ? "M" : "S", popped_count, readable_buff_cnt+1, tcc_video_get_readable_count(p));
		}

		pPrevImage = &p->vsync_buffer.curr_displaying_imgInfo;
		p->pIntlNextImage = &p->vsync_buffer.stImage[p->vsync_buffer.readIdx];

		if(p->skipFrameStatus){
			dprintk("skip status %d/0x%x => %d/0x%x en: %d layer: %d \n",
						pPrevImage->buffer_unique_id, pPrevImage->addr0, p->pIntlNextImage->buffer_unique_id, p->pIntlNextImage->addr0, p->pIntlNextImage->enable, p->pIntlNextImage->Lcdc_layer);
		}

		if(p->pIntlNextImage->Lcdc_layer == RDMA_FB)
			return;

		if(p->vsync_buffer.last_cleared_buff_id > p->pIntlNextImage->buffer_unique_id){
			dprintk_drop("%s:error wierd buffer id[%d >= %d]\n",
						p->pIntlNextImage->Lcdc_layer == RDMA_VIDEO ? "M" : "S", p->pIntlNextImage->buffer_unique_id, p->vsync_buffer.last_cleared_buff_id);
		}
//		printk("mt(%d), st(%d)\n", p->pIntlNextImage->time_stamp, current_time) ;
		if(p->pIntlNextImage->first_frame_after_seek){
			time_diff =  p->pIntlNextImage->time_stamp - (current_time+tcc_vsync_calc_early_margin(p));
			if(time_diff > 0 && abs(time_diff) > (p->nTimeGapToNextField*2) )
			{
				printk("frame need to wait (%d) (%d)\n",time_diff, p->nTimeGapToNextField*2);
				return;
			}

			p->pIntlNextImage->first_frame_after_seek = 0;
		}

		// resolution change support
		if((p->prevImageWidth != 0) && (p->prevImageHeight != 0)) {
			int image_width = p->pIntlNextImage->crop_right - ((p->pIntlNextImage->crop_left >> 3) << 3);
			int image_height = p->pIntlNextImage->crop_bottom - p->pIntlNextImage->crop_top;

			if((p->prevImageWidth != image_width) || (p->prevImageHeight != image_height)) {
				dprintk("Interlaced_Res_Changed ::  %d x %d -> %d x %d \n", p->prevImageWidth, p->prevImageHeight, image_width, image_height);
				p->prevImageWidth = image_width;
				p->prevImageHeight = image_height;

				reset_frmCnt = VIQE_RESET_CROP_CHANGED;	// SoC guide info.
			} else {
				reset_frmCnt = VIQE_RESET_NONE;
			}
		} else {
			p->prevImageWidth = p->pIntlNextImage->crop_right - ((p->pIntlNextImage->crop_left >> 3) << 3);
			p->prevImageHeight = p->pIntlNextImage->crop_bottom - p->pIntlNextImage->crop_top;;
			reset_frmCnt = VIQE_RESET_NONE;
		}

		if( ( pPrevImage->addr0 != p->pIntlNextImage->addr0 )
			|| ( pPrevImage->addr0 == p->pIntlNextImage->addr0 &&
				pPrevImage->offset_x != p->pIntlNextImage->offset_x &&
				pPrevImage->offset_y != p->pIntlNextImage->offset_y &&
				pPrevImage->Image_width != p->pIntlNextImage->Image_width &&
				pPrevImage->Image_height != p->pIntlNextImage->Image_height &&
				pPrevImage->Frame_width != p->pIntlNextImage->Frame_width &&
				pPrevImage->Frame_height != p->pIntlNextImage->Frame_height &&
				pPrevImage->crop_left != p->pIntlNextImage->crop_left &&
				pPrevImage->crop_top != p->pIntlNextImage->crop_top &&
				pPrevImage->crop_right != p->pIntlNextImage->crop_right &&
				pPrevImage->crop_bottom != p->pIntlNextImage->crop_bottom
			)
		)
		{
			if(pPrevImage->frameInfo_interlace != p->pIntlNextImage->frameInfo_interlace) {
				dprintk("Buffer_Type_Changed :: %s -> %s \n", pPrevImage->frameInfo_interlace ? "Interlace" : "Progressive",
					p->pIntlNextImage->frameInfo_interlace ? "Interlace" : "Progressive");
				reset_frmCnt = VIQE_RESET_RECOVERY;
			}

			dprintk("Readable[%d] ID[%d] pNextImage->time_stamp : %d / %d, Gap = %d\n", readable_buff_cnt, p->pIntlNextImage->buffer_unique_id, p->pIntlNextImage->time_stamp, current_time, p->pIntlNextImage->time_stamp - current_time);	

			if(pPrevImage->addr0 == p->pIntlNextImage->addr0)
				p->pIntlNextImage->bSize_Changed = 1;
			else
				p->pIntlNextImage->bSize_Changed = 0;
			memcpy(pPrevImage, p->pIntlNextImage, sizeof(struct tcc_lcdc_image_update));

			if((Output_SelectMode != TCC_OUTPUT_NONE) && (p->outputMode == Output_SelectMode))
			{
				if(!p->interlace_bypass_lcdc)
				{
					if(p->output_toMemory && p->m2m_mode) {
					#ifdef CONFIG_USE_SUB_MULTI_FRAME
						switch(p->type) {
						case VSYNC_MAIN:
							TCC_VIQE_DI_Run60Hz_M2M(p->pIntlNextImage, reset_frmCnt);
							break;
						case VSYNC_SUB0:
							TCC_VIQE_DI_Sub_Run60Hz_M2M(p->pIntlNextImage, reset_frmCnt);
							break;
						case VSYNC_SUB1:
						case VSYNC_SUB2:
						case VSYNC_SUB3:
							if(p->pIntlNextImage->mosaic_mode)
								TCC_VIQE_DI_Push60Hz_M2M(p->pIntlNextImage, p->type);
							break;
						default:
							pr_info("Invalid driver type(%d)\n", p->type);
						}
					#else
						TCC_VIQE_DI_Run60Hz_M2M(p->pIntlNextImage, reset_frmCnt);
					#endif
					} else {
						if(p->duplicateUseFlag) {
							viqe_render_field(current_time);
						} else {
							TCC_VIQE_DI_Run60Hz(p->pIntlNextImage,reset_frmCnt);
						}
					}
				}
				else
				{
					struct tcc_dp_device *dp_device = tca_fb_get_displayType(Output_SelectMode);
					if(tcc_vsync_bypass_frame(p, dp_device, p->pIntlNextImage) == 1){
						p->nDeinterProcCount =0;
						return;
					}
				}
			}

			if(p->output_toMemory && p->m2m_mode){
				if(p->deinterlace_mode && p->frameInfo_interlace){ // To prevent progressive contents from working twice on dual-vsync mode for PIP.
					p->nDeinterProcCount ++;
				}
			}
			else{
				if(p->frameInfo_interlace){ // To prevent 60fps progressive contents from no fixing A/V Sync.
					p->nDeinterProcCount ++;
				}
			}
		}
		else
		{
			dprintk("Same Buffer(0x%x - ID: %d) --- available count [%d]) \n", p->pIntlNextImage->addr0, p->pIntlNextImage->buffer_unique_id, tcc_video_get_readable_count(p));
			if((Output_SelectMode != TCC_OUTPUT_NONE) && (p->outputMode == Output_SelectMode))
			{
				if (!p->output_toMemory && !p->m2m_mode) {
					if(!p->interlace_bypass_lcdc)
					{
						if(p->duplicateUseFlag)
							viqe_render_field(current_time);
					}
					else
					{
						struct tcc_dp_device *dp_device = tca_fb_get_displayType(Output_SelectMode);
						if(tcc_vsync_bypass_frame(p, dp_device, p->pIntlNextImage) == 1){
							p->nDeinterProcCount =0;
							return;
						}
					}
				}
				#ifdef CONFIG_USE_SUB_MULTI_FRAME
				else {
					switch(p->type) {
					case VSYNC_MAIN:
						/* TODO */
						break;
					case VSYNC_SUB0:
						TCC_VIQE_DI_Sub_Run60Hz_M2M(p->pIntlNextImage, reset_frmCnt);
						break;
					case VSYNC_SUB1:
					case VSYNC_SUB2:
					case VSYNC_SUB3:
						if(p->pIntlNextImage->mosaic_mode)
							TCC_VIQE_DI_Push60Hz_M2M(p->pIntlNextImage, p->type);
						break;
					default:
						pr_info("Invalid driver type(%d)\n", p->type);
					}
				}
				#endif
			}
		}
	}
	else
	{
		dprintk("Intl(1) Process(0x%x) --- available count [%d]) \n", p->pIntlNextImage->addr0, tcc_video_get_readable_count(p));
		if((Output_SelectMode != TCC_OUTPUT_NONE) && (p->outputMode == Output_SelectMode) )
		{
			if(!p->interlace_bypass_lcdc)
			{
				if(p->output_toMemory && p->m2m_mode) {
					p->pIntlNextImage->odd_first_flag = !p->pIntlNextImage->odd_first_flag;
					//p->pIntlNextImage->dst_addr0 = p->pIntlNextImage->dst_addr1;
				#ifdef CONFIG_USE_SUB_MULTI_FRAME
					switch(p->type) {
					case VSYNC_MAIN:
						TCC_VIQE_DI_Run60Hz_M2M(p->pIntlNextImage, VIQE_RESET_NONE);
						break;
					case VSYNC_SUB0:
						TCC_VIQE_DI_Sub_Run60Hz_M2M(p->pIntlNextImage, VIQE_RESET_NONE);
						break;
					case VSYNC_SUB1:
					case VSYNC_SUB2:
					case VSYNC_SUB3:
						if(p->pIntlNextImage->mosaic_mode)
							TCC_VIQE_DI_Push60Hz_M2M(p->pIntlNextImage, p->type);
						break;
					default:
						pr_info("Invalid driver type(%d)\n", p->type);
					}
				#else
					TCC_VIQE_DI_Run60Hz_M2M(p->pIntlNextImage, VIQE_RESET_NONE);
				#endif
				} else {
					if(p->duplicateUseFlag){
						viqe_render_field(current_time);
					} else {
						p->pIntlNextImage->odd_first_flag = !p->pIntlNextImage->odd_first_flag;
						TCC_VIQE_DI_Run60Hz(p->pIntlNextImage,VIQE_RESET_NONE);
					}
				}
			}
		}

		p->nDeinterProcCount = 0;
	}
	return;
}

int display_vsync(tcc_video_disp *p)
{
#ifndef USE_VSYNC_TIMER
   if((++p->unVsyncCnt) &0x01)
		   p->baseTime += 16;
   else
		   p->baseTime += 17;

   if(!(p->unVsyncCnt%6))
		   p->baseTime++;
#endif

	if( tcc_vsync_is_empty_buffer(&p->vsync_buffer) == 0)
	{
	#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
		if(p->deinterlace_mode && !p->output_toMemory)
			tcc_vsync_display_update_forDeinterlaced(p);
		//else if(p->deinterlace_mode && p->output_toMemory && p->m2m_mode)
		else if(p->output_toMemory && p->m2m_mode)
			tcc_vsync_display_update_forDeinterlaced(p);
		else
	#endif
			tcc_vsync_display_update(p);

	#if defined(CONFIG_TCC_DISPLAY_MODE_USE) && !defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT)
		tca_fb_attach_update_flag();
	#endif
	}
	else
	{
		if( p->isVsyncRunning && (0 >= tcc_video_get_readable_count(p)))
			return -1;
	}

	return 0;
}

#define USE_DOLBY_VISION_SUB_INTERRUPT
static unsigned int dv_video_int_he_count = 0;
static irqreturn_t tcc_vsync_handler_for_video(int irq, void *dev_id)
{
	struct tcc_vsync_display_info_t *vsync = (struct tcc_vsync_display_info_t *)dev_id;
	unsigned int type = VSYNC_MAIN;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH())
	{
		volatile void __iomem *pDV_Cfg = VIOC_DV_VEDR_GetAddress(VDV_CFG);
		unsigned int status = 0;
		VIOC_V_DV_GetInterruptPending(pDV_Cfg, &status);

		if(status & DV_VIDEO_INT){
			VIOC_V_DV_ClearInterrupt(pDV_Cfg, DV_VIDEO_INT);
	#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
			tca_edr_inc_check_count(1, 0, 0, 0, 0);
	#endif
	#ifdef USE_DOLBY_VISION_SUB_INTERRUPT
			vioc_intr_enable(vsync_vioc0_disp.irq_num, VIOC_INTR_V_DV, DV_VIDEO_INT_SUB);
			dv_video_int_he_count = 0;
			return IRQ_HANDLED;
	#endif
		}
	#ifdef USE_DOLBY_VISION_SUB_INTERRUPT
		else if(status & DV_VIDEO_INT_SUB){
			VIOC_V_DV_ClearInterrupt(pDV_Cfg, DV_VIDEO_INT_SUB);
			if(dv_video_int_he_count++ > 2)
			{
				dv_video_int_he_count = 0;
				vioc_intr_disable(vsync_vioc0_disp.irq_num, VIOC_INTR_V_DV, DV_VIDEO_INT_SUB);
			}
			else
				return IRQ_HANDLED;
		}
	#endif
		else
			return IRQ_NONE;
	}
	else
#endif
	{
		if (!is_vioc_intr_activatied(vsync->vioc_intr->id, vsync->vioc_intr->bits)) {
			return IRQ_NONE;
		}

		vioc_intr_clear(vsync->vioc_intr->id, vsync->vioc_intr->bits);
	}

	for(type = VSYNC_MAIN; type < VSYNC_MAX; type++) {
#if !defined(USE_ALWAYS_UNPLUGGED_INT)
		#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
		if(tccvid_lastframe[VSYNC_MAIN].enabled)
			return IRQ_HANDLED;
		#endif
		if(tcc_vsync_isVsyncRunning(type))
			display_vsync(&tccvid_vsync[type]);
#endif
	}

	return IRQ_HANDLED;
}

#ifdef USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT
static irqreturn_t tcc_vsync_unplugged_output_for_video(int irq, void *dev_id)
{
	unsigned int type = VSYNC_MAIN;
	for(type = VSYNC_MAIN; type < VSYNC_MAX; type++) {
	#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
		if(LastFrame[VSYNC_MAIN])
			return IRQ_HANDLED;
	#endif
		display_vsync(&tccvid_vsync[type]);
	}
	return IRQ_HANDLED;
}

static void tcc_vsync_unplugged_int_start(irq_handler_t handler)
{
	if(!vsync_unpluged_int_timer) {
		vsync_unpluged_int_timer = tcc_register_timer(NULL, 16000/*msec*/, handler); // 16.667ms
		if (IS_ERR(vsync_unpluged_int_timer)) {
			printk("%s: cannot register tcc timer for unplugged output. ret:0x%x\n", __func__, (unsigned)vsync_unpluged_int_timer);
			vsync_unpluged_int_timer = NULL;
			BUG();
		}
	}

	if (vsync_unpluged_int_timer && !g_is_unplugged_int_enabled) {
		tcc_timer_enable(vsync_unpluged_int_timer);
		g_is_unplugged_int_enabled = 1;
		msleep(0);
	}
	printk("%s \n", __func__);
}

static void tcc_vsync_unplugged_int_stop(void)
{
#if defined(USE_ALWAYS_UNPLUGGED_INT)
	tcc_vsync_unplugged_int_start(tcc_vsync_unplugged_output_for_video);
	return;
#endif

	if (vsync_unpluged_int_timer && g_is_unplugged_int_enabled){
		tcc_timer_disable(vsync_unpluged_int_timer);
		g_is_unplugged_int_enabled = 0;
	}

	if( !g_is_unplugged_int_enabled && vsync_unpluged_int_timer)
	{
		tcc_unregister_timer(vsync_unpluged_int_timer);
		vsync_unpluged_int_timer = NULL;
	}
	printk("%s \n", __func__);
}

static int tcc_vsync_check_unplugged_int(void)
{
	if (vsync_unpluged_int_timer && g_is_unplugged_int_enabled)
		return 1;
	return 0;
}
#endif

inline static void tcc_vsync_set_time(tcc_video_disp *p, int time)
{
#ifdef USE_VSYNC_TIMER
	p->baseTime = tcc_get_timer_count(vsync_timer) - time;
#else
	p->baseTime = time;
#endif
}
	
inline static int tcc_vsync_get_time(tcc_video_disp *p)
{
#ifdef USE_VSYNC_TIMER
	return tcc_get_timer_count(vsync_timer) - p->baseTime;
#else
	return p->baseTime;
#endif

}

static void tcc_vsync_reset_syncTime(tcc_video_disp *p, int currentTime, VSYNC_CH_TYPE type)
{
	memset(p->timeGap, 0x00, sizeof(p->timeGap));
	p->timeGapBufferFullFlag = 0;
	p->timeGapIdx = 0;
	p->timeGapTotal = 0;
	
	spin_lock_irq(&vsync_lock) ;
	tcc_vsync_set_time(p, currentTime);
	p->unVsyncCnt=0;
	spin_unlock_irq(&vsync_lock) ;

	if(!p->firstFrameFlag)
		printk("[%d] reset base time : %d\n", type, currentTime);
}

static int tcc_vsync_calculate_syncTime(tcc_video_disp *p, int currentTime, VSYNC_CH_TYPE type)
{
	int diffTime;
	int avgTime;
	int base_time;

	if(p->lastUdateTime == currentTime)
		return 0;

	p->lastUdateTime= currentTime;

	spin_lock_irq(&vsync_lock) ;
	base_time = tcc_vsync_get_time(p);
	spin_unlock_irq(&vsync_lock) ;
	
	diffTime =	currentTime - base_time;

	p->timeGapTotal -= p->timeGap[p->timeGapIdx];
	p->timeGapTotal += diffTime;
	p->timeGap[p->timeGapIdx++] = diffTime;

	if(p->timeGapIdx >= TIME_BUFFER_COUNT)
		p->timeGapIdx = 0;

	if(p->timeGapIdx == 0)
		p->timeGapBufferFullFlag = 1;

	if(p->timeGapBufferFullFlag)
		avgTime = p->timeGapTotal / TIME_BUFFER_COUNT;
	else
		avgTime = p->timeGapTotal / (int)(p->timeGapIdx);


	vprintk("[%d] diffTime %d, avgTime %d, base : %d\n", type, diffTime, avgTime, base_time);

	#if 0//def USE_VSYNC_TIMER
	if( p->timeGapBufferFullFlag && (avgTime > p->updateGapTime || avgTime < -(p->updateGapTime)))
	#else
	if( (p->timeGapBufferFullFlag || p->unVsyncCnt < 100) 
		&& (avgTime > p->updateGapTime || avgTime < -(p->updateGapTime)))
	#endif
	{
		memset(p->timeGap, 0x00, sizeof(p->timeGap));
		p->timeGapBufferFullFlag = 0;
		p->timeGapIdx = 0;
		p->timeGapTotal = 0;
		
		//printk("changed time base time %d kernel time %d time %d \n",
		//	p->baseTime,tcc_get_timer_count(vsync_timer),currentTime);
		spin_lock_irq(&vsync_lock) ;
		tcc_vsync_set_time(p, tcc_vsync_get_time(p)+avgTime);
		spin_unlock_irq(&vsync_lock) ;

#if !defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
		if(!p->firstFrameFlag)
			printk("changed base time : %d, add time: %d diffTime %d fps(%d)\n",base_time+avgTime, avgTime,diffTime, p->video_frame_rate);
#endif
	}
	
	return 0;
}


static int tcc_vsync_push_preprocess(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *input_image_info, VSYNC_CH_TYPE type)
{
	unsigned int lcd_width, lcd_height;
	volatile void __iomem * pDISPBase;
	int check_time=0;
	int display_hz=0;
	int ret = 0;

	if(pdp_data == NULL){
		vprintk("[%d] %s :: pdp_data is null \n", type, __func__);
		ret = -10;
		goto Error_Proc;
	}

#if !defined(USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT)
	pDISPBase = pdp_data->ddc_info.virt_addr;
	VIOC_DISP_GetSize(pDISPBase, &lcd_width, &lcd_height);

	if(!pdp_data->FbPowerState || !lcd_width || !lcd_height || !VIOC_DISP_Get_TurnOnOff(pDISPBase) || !p->isVsyncRunning) {
		dprintk("[%d] %s : FbPower(%d) Disp(%dx%d, %d) VSync(%d) Output_SelectMode(%d) \n", type, __func__, pdp_data->FbPowerState,lcd_width,lcd_height,VIOC_DISP_Get_TurnOnOff(pDISPBase) ,p->isVsyncRunning, Output_SelectMode);
		ret = -11;
		goto Error_Proc;
	}
#endif

	if(tcc_video_check_framerate(p, input_image_info->private_data.optional_info[VID_OPT_FPS]))
	{
		if(Output_SelectMode == TCC_OUTPUT_HDMI && HDMI_video_hz != 0)
			display_hz = HDMI_video_hz;
		else
			display_hz = 60;

		p->vsync_interval = (1000/display_hz);

		if( (p->video_frame_rate > 0) && (display_hz >= p->video_frame_rate)  && ((display_hz % p->video_frame_rate) == 0))
			p->perfect_vsync_flag = 1;
		else
			p->perfect_vsync_flag = 0;

		if((p->video_frame_rate > 0) && (p->video_frame_rate < display_hz/2))
			p->duplicateUseFlag = 1;
		else
			p->duplicateUseFlag = 0;

		printk("[%d]### Change_Frame_Rate with ex_out(%d) ,interval(%d), perfect_flag(%d) duplicate(%d) \n", type,
			input_image_info->ex_output, p->vsync_interval, p->perfect_vsync_flag,p->duplicateUseFlag);
	}

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if(!VIOC_CONFIG_DV_GET_EDR_PATH())
#endif
	{
	#if defined(CONFIG_TCC_M2M_USE_INTERLACE_OUTPUT)
		if( !(pDISPBase->uCTRL.nREG & HwDISP_NI )) {//interlace mode
			input_image_info->on_the_fly = 0;
		}
	#endif
	}
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
	if(0 >= tcc_video_check_last_frame(input_image_info)){
		printk("[%d]----> skip this frame for last-frame \n", type);
		ret = -12;
		goto Error_Proc;
	}
#endif

	spin_lock_irq(&vsync_lock) ;

	if(p->player_no_sync_time)
		input_image_info->sync_time = tcc_vsync_get_time(p);
	else
		check_time = abs(tcc_vsync_get_time(p) - input_image_info->sync_time);
	
	spin_unlock_irq(&vsync_lock) ;

	if(p->player_no_sync_time)
	{
		int check_index=0;
		
		input_image_info->output_path=0;
		p->skipFrameStatus = 0;
		check_index = input_image_info->buffer_unique_id -p->prev_buffer_unique_id;
		
		//printk("id:%d/%d vts:%d/%d t:%d valid:%d  \n",input_image_info->buffer_unique_id,p->prev_buffer_unique_id,
		//	input_image_info->time_stamp,p->prev_time_stamp,input_image_info->sync_time,tcc_video_get_valid_count(p));
		//this is for seek, fast play(except x2) and inline <-> full screen
		if((p->firstFrameFlag == 1) || (p->player_seek == 1) || (check_index > 1 && abs(p->prev_time_stamp - input_image_info->time_stamp) > (p->nTimeGapToNextField+2)*check_index) )
		{
			printk("[%d] Check the time gap %d, %d/%d \n", type, check_index, input_image_info->time_stamp, p->prev_time_stamp);
			if(p->firstFrameFlag != 1){
				p->skipFrameStatus = 1;
				input_image_info->output_path = 1;
			}
			//set sync time from vts
			check_time = GAP_FOR_RESET_SYNC_TIME + 1;
			input_image_info->sync_time=input_image_info->time_stamp-(tcc_vsync_calc_early_margin(p) + 20);
		}
		else if(p->wait_too_long_indicate==1)
		{
			input_image_info->output_path = 1;
		}
		/* for x2
		else if(check_index < 3 && abs(p->prev_time_stamp - input_image_info->time_stamp)*2 < (p->nTimeGapToNextField+2)*check_index){
			//p->skipFrameStatus = 1;
			input_image_info->output_path = 1;
		}
		*/

		p->prev_time_stamp = input_image_info->time_stamp;
		p->prev_buffer_unique_id = input_image_info->buffer_unique_id;
		
		p->player_seek= 0;
	}
	
	if(check_time > GAP_FOR_RESET_SYNC_TIME)
	{
#ifdef USE_VSYNC_TIMER
		printk("[%d] reset time base time %d kernel_time %ld sync_time %d :: TS %d \n", type,
					p->baseTime, tcc_get_timer_count(vsync_timer), input_image_info->sync_time, input_image_info->time_stamp);
#endif
		tcc_vsync_reset_syncTime(p, 0, type);
	}
	
	tcc_vsync_calculate_syncTime(p, input_image_info->sync_time, type);

	return ret;

Error_Proc:
	spin_lock_irq(&vsync_lockDisp);
	tcc_vsync_pop_all_buffer(&p->vsync_buffer, 0);
	p->vsync_buffer.last_cleared_buff_id = input_image_info->buffer_unique_id;
	vprintk("[%d] %s : cleared buffer (%d -> %d) \n", type, __func__, input_image_info->buffer_unique_id, p->vsync_buffer.last_cleared_buff_id);
	spin_unlock_irq(&vsync_lockDisp);

	return ret;
}

static void tcc_vsync_push_set_outputMode(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *input_image_info)
{
	switch(input_image_info->outputMode)
	{
		case OUTPUT_NONE:
			#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
			p->outputMode = TCC_OUTPUT_LCD;
			#else
			p->outputMode = TCC_OUTPUT_NONE;			
			#endif
			break;
		case OUTPUT_HDMI:
			p->outputMode = TCC_OUTPUT_HDMI;
			tcc_vsync_check_interlace_output(p, pdp_data);
			break;
		case OUTPUT_COMPOSITE:
			p->outputMode = TCC_OUTPUT_COMPOSITE; 		
			tcc_vsync_check_interlace_output(p, pdp_data);
			break;
		case OUTPUT_COMPONENT:
			p->outputMode = TCC_OUTPUT_COMPONENT; 		
			tcc_vsync_check_interlace_output(p, pdp_data);
			break;
		default:
			p->outputMode = TCC_OUTPUT_NONE;									
	}
	
	printk("set_outputMode input->outputMode:%d SelectMode:%d \n", input_image_info->outputMode,Output_SelectMode);			

	return;
}

static int tcc_vsync_push_preprocess_deinterlacing(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *input_image_info)
{
	unsigned int lcdCtrlNum, lcd_width, lcd_height;
	volatile void __iomem * pDISPBase;

	if(p->deinterlace_mode < 0)
	{
		
		pDISPBase = pdp_data->ddc_info.virt_addr;
				
		VIOC_DISP_GetSize(pDISPBase, &lcd_width, &lcd_height);
		
		p->deinterlace_mode = input_image_info->deinterlace_mode;
		p->frameInfo_interlace = input_image_info->frameInfo_interlace;
		p->output_toMemory = input_image_info->output_toMemory;
		printk("### deinterlace_mode(%d), output_toMemory(%d)\n", p->deinterlace_mode, p->output_toMemory);
		
		p->interlace_bypass_lcdc = 0;
		p->mvcMode = input_image_info->MVCframeView;

		p->m2m_mode =  input_image_info->m2m_mode;
	
		if( (p->deinterlace_mode == 1) && 
			(p->interlace_output == 1) && 
			(p->output_toMemory == 0) &&
			(input_image_info->Frame_width == input_image_info->Image_width) && (input_image_info->Frame_height == input_image_info->Image_height) )
		{
			printk("### interlace_bypass_lcdc set !!\n");
			p->interlace_bypass_lcdc = 1;
		}

		#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) || defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
		if( (p->outputMode == TCC_OUTPUT_COMPOSITE) && 
			(p->deinterlace_mode == 1) && 
			(p->interlace_output == 1) && 
			(p->output_toMemory == 0) &&
			(input_image_info->Frame_width == input_image_info->Image_width) )
		{
			if(p->interlace_bypass_lcdc == 0)
			{
				printk("### interlace_bypass_lcdc set for testing composite signal\n");
				p->interlace_bypass_lcdc = 1;
			}
		}

		#if defined(CONFIG_FB_TCC_COMPOSITE)
		tcc_composite_set_bypass(p->interlace_bypass_lcdc);
		#endif
		#endif
	}


	
	#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) || defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
	if((p->outputMode == TCC_OUTPUT_COMPOSITE) && (p->interlace_bypass_lcdc))
	{
		if(input_image_info->Frame_width <= input_image_info->Image_width)
			input_image_info->Image_width = input_image_info->Frame_width;
	}
	#endif
	
	if(p->deinterlace_mode && p->firstFrameFlag &&!p->interlace_bypass_lcdc)
	{
		if(p->outputMode == TCC_OUTPUT_LCD ||p->outputMode == TCC_OUTPUT_NONE )
			lcdCtrlNum = LCD_LCDC_NUM;
		else
			lcdCtrlNum = EX_OUT_LCDC;	

		p->nDeinterProcCount = 0;

		if(!p->output_toMemory)
		{
			dprintk("first on-the-fly TCC_VIQE_DI_Init60Hz :: Layer(%d)\n", input_image_info->Lcdc_layer) ;
			if (input_image_info->Lcdc_layer == RDMA_VIDEO)
			{
				if(VIOC_CONFIG_DMAPath_Support()) {
					#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
					int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[input_image_info->Lcdc_layer].blk_num);

					if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
						VIOC_CONFIG_DMAPath_UnSet(component_num);

					// It is default path selection(VRDMA)
					VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[input_image_info->Lcdc_layer].blk_num, pdp_data->rdma_info[input_image_info->Lcdc_layer].blk_num);
					#endif
				} else {
					#ifdef CONFIG_ARCH_TCC803X
					VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, pdp_data->rdma_info[input_image_info->Lcdc_layer].blk_num);
					#endif
				}
			}
			TCC_VIQE_DI_Init60Hz(Output_SelectMode, lcdCtrlNum, input_image_info);
		}
	}

	if (p->firstFrameFlag &&!p->interlace_bypass_lcdc) {
		if(p->output_toMemory && p->m2m_mode)
		{
			dprintk("first m2m TCC_excuteVIQE_60Hz \n") ;
			if(input_image_info->Lcdc_layer == RDMA_VIDEO)
				TCC_VIQE_DI_Init60Hz_M2M(Output_SelectMode, input_image_info);
			else
				TCC_VIQE_DI_Sub_Init60Hz_M2M(Output_SelectMode, input_image_info);
		}
	}

	p->firstFrameFlag = 0;

	return 0;
}

static int tcc_vsync_push_bypass_frame(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *input_image_info, VSYNC_CH_TYPE type)
{
	if(input_image_info->output_path)
	{
		if(p->vsync_buffer.available_buffer_id_on_vpu > input_image_info->buffer_unique_id)
			return 0;

		dprintk("### output_path[%d] buffer_unique_id %d \n", type,input_image_info->buffer_unique_id);
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
		if(tccvid_lastframe[type].enabled)
			return 0;
#endif

		input_image_info->bSize_Changed = 1;
		memcpy(&p->vsync_buffer.curr_displaying_imgInfo, input_image_info, sizeof(struct tcc_lcdc_image_update));

		spin_lock_irq(&vsync_lockDisp);
		tcc_vsync_pop_all_buffer(&p->vsync_buffer, 0);
		p->vsync_buffer.last_cleared_buff_id = input_image_info->buffer_unique_id;
		spin_unlock_irq(&vsync_lockDisp);

		if(p->deinterlace_mode && !p->interlace_bypass_lcdc)
		{
			p->nDeinterProcCount = 0;
			
			if(Output_SelectMode != TCC_OUTPUT_NONE && !p->duplicateUseFlag)
			{
				if(!p->output_toMemory) {
					if (input_image_info->Lcdc_layer == RDMA_VIDEO)
						TCC_VIQE_DI_Run60Hz(input_image_info,VIQE_RESET_NONE);
				}
				else if(p->output_toMemory && p->m2m_mode) {
					TCC_VIQE_DI_Run60Hz_M2M(input_image_info,VIQE_RESET_NONE);
				}
			}
		}
		else if(p->interlace_bypass_lcdc){
			p->nDeinterProcCount =0;
			if(tcc_vsync_bypass_frame(p, pdp_data,input_image_info) == 1){
				//p->nDeinterProcCount =0;
				//return;
			}
		}
		else if(p->output_toMemory && p->m2m_mode){
			TCC_VIQE_DI_Run60Hz_M2M(input_image_info,VIQE_RESET_NONE);
		}
		else
		{
			switch(Output_SelectMode)
			{
				case TCC_OUTPUT_NONE:
					break;
				#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
				case TCC_OUTPUT_LCD:
					tca_scale_display_update(pdp_data, input_image_info);
					break;
				#endif
				case TCC_OUTPUT_HDMI:
					//if(tcc_vsync_mvc_status )
					//printk("#####tcc_vsync_mvc_status=%d input_image_info->MVCframeView = %d, input_image_info->MVC_Base_addr0 = 0x%08x\n",tcc_vsync_mvc_status, input_image_info->MVCframeView, input_image_info->MVC_Base_addr0);

					if(tcc_vsync_mvc_status && input_image_info->MVCframeView == 1 && input_image_info->MVC_Base_addr0)
						tca_mvc_display_update(EX_OUT_LCDC, input_image_info);
					else
						tca_scale_display_update(pdp_data, input_image_info);
					break;
					
				case TCC_OUTPUT_COMPONENT:
				case TCC_OUTPUT_COMPOSITE:
					tca_scale_display_update(pdp_data, input_image_info);
					break;
				default:
					break;
			}
		}

		input_image_info->time_stamp = 0;
		
		return 1;
	}

	return 0;
}

static int tcc_vsync_push_check_error(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *input_image_info)
{
	
	int error_type = 0;
	
	if(p->skipFrameStatus)
	{
		error_type = -21;
	}
	else if(p->outputMode != Output_SelectMode)
	{
#ifdef USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT
		if(0 == tcc_vsync_check_unplugged_int())
#endif
		{
			vprintk("vsync push error : output mode different %d %d \n", p->outputMode ,Output_SelectMode);
			error_type = -22;
		}
	}
	else if(p->vsync_buffer.available_buffer_id_on_vpu > input_image_info->buffer_unique_id)
	{
		vprintk("vsync push error : buffer index sync fail omx_buf_id: %d, cur_buff_id: %d \n",
				p->vsync_buffer.available_buffer_id_on_vpu, input_image_info->buffer_unique_id);
		error_type = -23;
	}
	
	if(error_type < 0)
	{
		vprintk("vsync push error : %d buffer_unique_id %d\n", error_type,input_image_info->buffer_unique_id);
		spin_lock_irq(&vsync_lockDisp) ;
		p->vsync_buffer.last_cleared_buff_id = input_image_info->buffer_unique_id;
		spin_unlock_irq(&vsync_lockDisp) ;
		return error_type;
	}

	return 0;
}

static int tcc_vsync_push_process(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *input_image_info, VSYNC_CH_TYPE type)
{
	vprintk("tcc_vsync_push_process :[%d] %d %d\n", input_image_info->buffer_unique_id, input_image_info->time_stamp, input_image_info->sync_time);	
	if(input_image_info->time_stamp < input_image_info->sync_time)
	{
		input_image_info->time_stamp = TIME_MARK_SKIP;
	}

	if(tcc_vsync_is_full_buffer(&p->vsync_buffer))
	{
		int timeout = wait_event_interruptible_timeout( wq_consume,
										tcc_vsync_is_full_buffer(&p->vsync_buffer) == 0, 
										msecs_to_jiffies(500)) ;
		vprintk("mio push wait(%d ms) due to buffer-full \n", 500-jiffies_to_msecs(timeout));
	}
	
	spin_lock_irq(&vsync_lock) ;
	input_image_info->viqe_queued = 0;
	if(tcc_vsync_push_buffer(p, &p->vsync_buffer, input_image_info) < 0)
	{
		printk("critical error: vsync buffer full by fault buffer controll\n");
	}

	spin_unlock_irq(&vsync_lock) ;
	//printk("OddFirst = %d, interlaced %d\n", input_image_info->odd_first_flag, input_image_info->deinterlace_mode);
	vprintk("vtime : %d, curtime : %d\n", input_image_info->time_stamp, input_image_info->sync_time) ;
	dprintk("%s: PUSH_%d buffer ID(%d) - TS(v[%d ms] / s[%d ms]) : HDR(%d/%d)\n", input_image_info->Lcdc_layer == RDMA_VIDEO ? "M" : "S", type,
					input_image_info->buffer_unique_id, input_image_info->time_stamp, input_image_info->sync_time,
					input_image_info->private_data.optional_info[VID_OPT_HAVE_USERDATA], input_image_info->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO]);

	return 0;
}

static int tcc_vsync_start(tcc_video_disp *p, struct tcc_lcdc_image_update *input_image_info, VSYNC_CH_TYPE type)
{
	int backup_time, backup_frame_rate;
	int display_hz=0, ret = 0;
#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
	if(!input_image_info->ex_output){
		vsync_started_device = LCD_START_VSYNC;
		Output_SelectMode = TCC_OUTPUT_LCD;
	}
	else{
		Output_SelectMode = TCC_OUTPUT_HDMI;
		vsync_started_device = HDMI_START_VSYNC;
	}
#else
	switch(input_image_info->outputMode)
	{
	case OUTPUT_NONE:
		Output_SelectMode = TCC_OUTPUT_LCD;
		break;
	case OUTPUT_HDMI:
		Output_SelectMode = TCC_OUTPUT_HDMI;
		break;
	case OUTPUT_COMPOSITE:
		Output_SelectMode = TCC_OUTPUT_COMPOSITE;
		break;
	case OUTPUT_COMPONENT:
		Output_SelectMode = TCC_OUTPUT_COMPONENT;
		break;
	case OUTPUT_MAX:
	default:
		pr_err("%s[%d]: wrong argument(%d)\n", __func__, __LINE__, input_image_info->outputMode);
		break;
	}
#endif

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
	spin_lock_irq(&LastFrame_lockDisp);
	if(tccvid_lastframe[type].enabled && tccvid_lastframe[type].reason.Resolution == 0 && tccvid_lastframe[type].reason.Codec == 0){
		tccvid_lastframe[type].enabled = 0;
		printk("----> LastFrame[%d] = 0 in TCC_LCDC_VIDEO_START_VSYNC \n", type);
	}
	spin_unlock_irq(&LastFrame_lockDisp);
#endif

	if(p->vsync_started == 0)
	{
		struct tcc_dp_device *dp_device = tca_fb_get_displayType(Output_SelectMode);
		if(dp_device == NULL) {
			pr_err("%s[%d]: Can not get the device information(%d)\n", __func__, __LINE__, Output_SelectMode);
			ret = -1;
			goto err_vsync_start;
		} else {
			if(!dp_device->FbPowerState) {
				pr_err("%s[%d]: Output Device is turned off(%d)\n", __func__, __LINE__, Output_SelectMode);
				ret = -1;
				goto err_vsync_start;
			}
		}

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
		if(input_image_info->Lcdc_layer == RDMA_FB || input_image_info->Lcdc_layer >= RDMA_MAX_NUM)
		{
			if(type == VSYNC_MAIN)
				input_image_info->Lcdc_layer = RDMA_VIDEO;
			else
				input_image_info->Lcdc_layer = RDMA_VIDEO_SUB;
		}
		set_hdmi_drm(DRM_INIT, input_image_info, input_image_info->Lcdc_layer);
#endif
		tcc_video_set_framerate(p, input_image_info->fps);
		
		backup_time = p->nTimeGapToNextField; 
		backup_frame_rate = p->video_frame_rate;
		memset( p, 0, sizeof( tcc_video_disp) ) ; 
		p->isVsyncRunning = 1;
		p->overlayUsedFlag = -1;
		p->outputMode = -1;
		p->firstFrameFlag = 1;
		p->deinterlace_mode= -1;
		p->m2m_mode = -1;
		p->output_toMemory = -1;
		p->mvcMode = 0;
		p->duplicateUseFlag =0;
		p->nTimeGapToNextField = backup_time;
		p->video_frame_rate = backup_frame_rate;
		p->time_gap_sign = 1;

		// (input_image_info->first_frame_after_seek == -99) means vsync is started from other player which not give sync time info. ex mediacodec player
		if(input_image_info->first_frame_after_seek == -99){
			p->player_no_sync_time = 1;
		}
		else {
			p->player_no_sync_time = 0;
		}
		
		p->prev_time_stamp = 0;
		p->prev_buffer_unique_id= 0;
		p->wait_too_long_indicate= 0;
		
		if (type == VSYNC_MAIN)
			viqe_render_init();

		if(backup_time)
			p->updateGapTime = backup_time;
		else
			p->updateGapTime = 32;

		if(Output_SelectMode == TCC_OUTPUT_HDMI && HDMI_video_hz != 0)
			display_hz = HDMI_video_hz;
		else
			display_hz = 60;

		p->vsync_interval = (1000/display_hz);

		if((p->video_frame_rate > 0) && (display_hz >= p->video_frame_rate)  && ((display_hz % p->video_frame_rate) == 0))
			p->perfect_vsync_flag = 1;
		else
			p->perfect_vsync_flag = 0;

		if((p->video_frame_rate > 0) && (p->video_frame_rate < display_hz/2))
			p->duplicateUseFlag =1;

		
		//spin_lock_init(&vsync_lock);
		//spin_lock_init(&vsync_lockDisp);

		#if 0 //def USE_VSYNC_TIMER
		if (vsync_timer) {
			tcc_timer_enable(vsync_timer);
			msleep(0);
		}
		#endif

		spin_lock_irq(&vsync_lock) ;
		tcc_vsync_set_time(p, 0);
		spin_unlock_irq(&vsync_lock) ;
		printk("### START_VSYNC_%d with ex_out(%d) ,interval(%d), perfect_flag(%d) duplicate(%d) player_no_sync_time(%d) fps(%d)\n", type, input_image_info->ex_output
			, p->vsync_interval, p->perfect_vsync_flag,p->duplicateUseFlag,p->player_no_sync_time, p->video_frame_rate);

		tca_vsync_video_display_enable();
		tcc_vsync_set_max_buffer(&p->vsync_buffer, input_image_info->max_buffer);
		
		p->vsync_started = 1;
		tccvid_lastframe[type].CurrImage.enable = 0;
		tccvid_lastframe[type].CurrImage.buffer_unique_id = 0xFFFFFFFF;

		tcc_vsync_set_output_mode(p, Output_SelectMode);
#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) && defined(DV_PREVENT_FRAME_DROP_AND_DUPLICATION)
		/*nPrevTS = nBaseTS = */bStart_DV_Display = 0;
		b1st_frame_display_count = 0;
		tca_edr_inc_check_count(0, 0, 0, 0, 1);
#endif
	} else {
		p->firstFrameFlag = 1;
	}
err_vsync_start:
	return ret;
}

static void tcc_vsync_end(tcc_video_disp *p, VSYNC_CH_TYPE type)
{
	#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
	printk("### END_VSYNC_%d started:%d SelectMode:%d LastFrame[%d]\n", type, p->vsync_started, Output_SelectMode, tccvid_lastframe[type].LastImage.enable);
	#else
	printk("### END_VSYNC_%d started:%d SelectMode:%d \n", type, p->vsync_started, Output_SelectMode);
	#endif

	#if defined(CONFIG_TCC_VTA)
	vta_cmd_notify_change_status(__func__);
	#endif

	if(p->vsync_started == 1)
	{
		struct tcc_lcdc_image_update ImageInfo;

		#if 0 //def USE_VSYNC_TIMER
		if (vsync_timer)
			tcc_timer_disable(vsync_timer);
		#endif

		if(tcc_vsync_get_isVsyncRunning(type) && !video_display_disable_check)
		{
			tca_vsync_video_display_disable();
			video_display_disable_check = 1;
		}

		spin_lock_irq(&vsync_lock) ;
		tcc_vsync_pop_all_buffer(&p->vsync_buffer, 1);
		spin_unlock_irq(&vsync_lock) ;

		p->skipFrameStatus = 1;
		p->nTimeGapToNextField = 0;
		p->isVsyncRunning = 0;


		#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
		if(p->deinterlace_mode &&!p->interlace_bypass_lcdc)
		{
			if(!p->output_toMemory) {
				TCC_VIQE_DI_DeInit60Hz();
			}
			else if(p->output_toMemory && p->m2m_mode) {
				if (type == VSYNC_MAIN)
					TCC_VIQE_DI_DeInit60Hz_M2M(RDMA_VIDEO);
				else
					TCC_VIQE_DI_Sub_DeInit60Hz_M2M(RDMA_VIDEO_SUB);
			}
		}
		else if(p->output_toMemory && p->m2m_mode)
		{
			if (type == VSYNC_MAIN)
				TCC_VIQE_DI_DeInit60Hz_M2M(RDMA_VIDEO);
			else
				TCC_VIQE_DI_Sub_DeInit60Hz_M2M(RDMA_VIDEO_SUB);
		}
		#endif

		memset(&ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));
		ImageInfo.enable = 0;
		if (type == VSYNC_MAIN)
			ImageInfo.Lcdc_layer = RDMA_VIDEO;
		else
			ImageInfo.Lcdc_layer = RDMA_VIDEO_SUB;

		if(Output_SelectMode != TCC_OUTPUT_NONE) {
			struct tcc_dp_device *dp_device = tca_fb_get_displayType(Output_SelectMode);
			if(dp_device) {
				if(Output_SelectMode == TCC_OUTPUT_HDMI){
					if(tcc_vsync_mvc_status && p->mvcMode==1) {
						tca_mvc_display_update(EX_OUT_LCDC, (struct tcc_lcdc_image_update *)&ImageInfo);
					} else {
						tca_scale_display_update(dp_device, (struct tcc_lcdc_image_update *)&ImageInfo);
					}

					if(tcc_vsync_mvc_overlay_priority) {
						printk(" @@@@ %s: tcc_vsync_mvc_overlay_priority %d \n",__func__,tcc_vsync_mvc_overlay_priority);

						VIOC_WMIX_SetOverlayPriority(dp_device->wmixer_info.virt_addr, tcc_vsync_mvc_overlay_priority);		//restore
						VIOC_WMIX_SetUpdate(dp_device->wmixer_info.virt_addr);
						tcc_vsync_mvc_overlay_priority = 0;
					}
				}
				else if((Output_SelectMode == TCC_OUTPUT_COMPOSITE) || (Output_SelectMode == TCC_OUTPUT_COMPONENT)){
					tca_scale_display_update(dp_device, (struct tcc_lcdc_image_update *)&ImageInfo);
				}
				#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
				else if(Output_SelectMode == TCC_OUTPUT_LCD){
					tca_scale_display_update(dp_device, (struct tcc_lcdc_image_update *)&ImageInfo);
				}
				#endif

				#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
				set_hdmi_drm(DRM_OFF, &ImageInfo, ImageInfo.Lcdc_layer);
				#endif
			}
		}
		p->deinterlace_mode = 0;
		p->vsync_started = 0;
		p->player_no_sync_time = 0;

		#ifdef USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT
		if((0 == tcc_vsync_isVsyncRunning(VSYNC_MAIN)) && (0 == tcc_vsync_isVsyncRunning(VSYNC_SUB0)))
			tcc_vsync_unplugged_int_stop();
		#endif
	}

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
	if(!tccvid_lastframe[type].support)
		tccvid_lastframe[type].LastImage.enable = 0;

	tccvid_lastframe[type].CurrImage.enable = 0;
	tccvid_lastframe[type].CurrImage.buffer_unique_id = 0xFFFFFFFF;

	#ifdef CONFIG_DISPLAY_EXT_FRAME
	if(!restricted_ExtFrame)
	#endif
		tcc_ctrl_ext_frame(0);
#endif
}

#if  defined(CONFIG_HDMI_DISPLAY_LASTFRAME) || defined(CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME)
static int _tcc_vsync_check_region_for_cma(unsigned int start_phyaddr, unsigned int length)
{
	unsigned int end_phyaddr = start_phyaddr + length -1;
	int type = 0;

	for(type = VSYNC_MAIN; type < VSYNC_MAX; type++)
	{
		if( (start_phyaddr >= tccvid_lastframe[type].pmapBuff.base) && (end_phyaddr <= (tccvid_lastframe[type].pmapBuff.base+tccvid_lastframe[type].pmapBuff.size-1)))
			return pmap_is_cma_alloc(&tccvid_lastframe[type].pmapBuff);
	}

	return 0;
}

static void* _tcc_vsync_get_virtaddr(unsigned int start_phyaddr, unsigned int length)
{
	void *virt_address = NULL;

	if (_tcc_vsync_check_region_for_cma(start_phyaddr, length))
		virt_address = pmap_cma_remap((phys_addr_t)start_phyaddr, PAGE_ALIGN(length));
	else
		virt_address = (void*)ioremap_nocache((phys_addr_t)start_phyaddr, PAGE_ALIGN(length));

	if (virt_address == NULL) {
		pr_err("%s: error ioremap for 0x%x / 0x%x \n", __func__, start_phyaddr, length);
		return NULL;
	}

	return virt_address;
}

static void _tcc_vsync_release_virtaddr(void * target_virtaddr, unsigned int start_phyaddr, unsigned int length)
{
	if (_tcc_vsync_check_region_for_cma(start_phyaddr, length))
		pmap_cma_unmap(target_virtaddr, PAGE_ALIGN(length));
	else
		iounmap((void*)target_virtaddr);
}

//will be copyed original decoded data itself.
int tcc_move_video_frame_simple( struct file *file, struct tcc_lcdc_image_update* inFframeInfo, WMIXER_INFO_TYPE* WmixerInfo, unsigned int target_addr, unsigned int target_size, unsigned int target_format)
{
	int Frame_height = 0,  ret = 0;
	unsigned int type = tcc_vsync_get_video_ch_type(inFframeInfo->Lcdc_layer);
	unsigned int szStream = 0x00;

	if(type >= VSYNC_MAX)
		return -1;

	memset(WmixerInfo, 0x00, sizeof(WMIXER_INFO_TYPE));

	Frame_height = inFframeInfo->Frame_height;
#ifdef CONFIG_VIOC_MAP_DECOMP
	if(inFframeInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0) {
		if(inFframeInfo->private_data.mapConv_info.m_uiLumaBitDepth != 8) {
			Frame_height = Frame_height * 2;
		}
	}
	else  
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
	if(inFframeInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0) {
		if(inFframeInfo->private_data.dtrcConv_info.m_iBitDepthLuma != 8) {
			Frame_height = Frame_height * 125 / 100;
		}
	}
	else  
#endif
#ifdef CONFIG_VIOC_10BIT
	if(inFframeInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10) {
		Frame_height = Frame_height * 2;
	} 
	else if(inFframeInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
		Frame_height = Frame_height * 125 / 100;
	}
#endif

	WmixerInfo->rsp_type		= WMIXER_POLLING;

	//source info
	WmixerInfo->src_fmt			= inFframeInfo->fmt;	
	WmixerInfo->src_img_width 	= inFframeInfo->Frame_width;
	WmixerInfo->src_img_height	= Frame_height;
	WmixerInfo->src_win_left	= 0;
	WmixerInfo->src_win_top		= 0;
	WmixerInfo->src_win_right	= inFframeInfo->Frame_width;
	WmixerInfo->src_win_bottom	= Frame_height;
	WmixerInfo->src_y_addr		= (unsigned int)inFframeInfo->addr0;
	WmixerInfo->src_u_addr		= (unsigned int)inFframeInfo->addr1;
	WmixerInfo->src_v_addr		= (unsigned int)inFframeInfo->addr2;

	//destination info
	WmixerInfo->dst_fmt			= target_format;
	WmixerInfo->dst_img_width   = inFframeInfo->Frame_width;
	WmixerInfo->dst_img_height  = Frame_height;
	WmixerInfo->dst_win_left	= 0;
	WmixerInfo->dst_win_top		= 0;
	WmixerInfo->dst_win_right	= inFframeInfo->Frame_width;
	WmixerInfo->dst_win_bottom	= Frame_height;
	WmixerInfo->dst_y_addr		= target_addr;
#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
	if( inFframeInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0 || inFframeInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0 ) {
		WmixerInfo->dst_u_addr		= (unsigned int)GET_ADDR_YUV42X_spU(WmixerInfo->dst_y_addr, (((WmixerInfo->dst_img_width+31)>>5)<<5), (((WmixerInfo->dst_img_height+31)>>5)<<5));
		WmixerInfo->dst_v_addr		= (unsigned int)GET_ADDR_YUV422_spV(WmixerInfo->dst_u_addr, (((WmixerInfo->dst_img_width+31)>>5)<<5), (((WmixerInfo->dst_img_height+31)>>5)<<5));
	}
	else
#endif
	{
		WmixerInfo->dst_u_addr		= (unsigned int)GET_ADDR_YUV42X_spU(WmixerInfo->dst_y_addr, WmixerInfo->dst_img_width, WmixerInfo->dst_img_height);
		WmixerInfo->dst_v_addr		= (unsigned int)GET_ADDR_YUV422_spV(WmixerInfo->dst_u_addr, WmixerInfo->dst_img_width, WmixerInfo->dst_img_height);
	}

#ifdef CONFIG_VIOC_MAP_DECOMP
	if( ret > 0 && inFframeInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0 )
		szStream = PAGE_ALIGN((((WmixerInfo->dst_img_width+31)>>5)<<5) * (((WmixerInfo->dst_img_height+31)>>5)<<5) * 162/100);
	else
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
	if( ret > 0 && inFframeInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] == 0x1 )
		szStream = PAGE_ALIGN((((WmixerInfo->dst_img_width+31)>>5)<<5) * (((WmixerInfo->dst_img_height+31)>>5)<<5) * 180/100);
	else
#endif
		szStream = PAGE_ALIGN((((WmixerInfo->dst_img_width+31)>>5)<<5) * (((WmixerInfo->dst_img_height+31)>>5)<<5) * 3/2);

	if(szStream > target_size)
		return -2;

	ret = file->f_op->unlocked_ioctl(file, TCC_WMIXER_IOCTRL_KERNEL, (unsigned long)WmixerInfo);

#ifdef CONFIG_VIOC_MAP_DECOMP
	if( ret > 0 && inFframeInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0 )
	{
		unsigned int LumaTblSize = 0, ChromaTblSize = 0;
		unsigned char *vSrc_addr, *vDst_addr;
		unsigned int pDst_addr = target_addr + szStream;

		LumaTblSize = inFframeInfo->private_data.mapConv_info.m_uiCompressionTableLumaSize;
		ChromaTblSize = inFframeInfo->private_data.mapConv_info.m_uiCompressionTableChromaSize;

		if((szStream + PAGE_ALIGN(LumaTblSize) + PAGE_ALIGN(ChromaTblSize)) > target_size)
		{
			printk("Error :: should increase the size of 'fb_wmixer' pmap :: current(0x%x) -> need(0x%x) \n",
						target_size, szStream + PAGE_ALIGN(LumaTblSize) + PAGE_ALIGN(ChromaTblSize));
			return -3;
		}

		printk("### tcc_move_video_frame_simple pre-processing(%dx%d - BitDepth(%d) Y(0x%x-0x%x / 0x%x), Cb(0x%x-0x%x / 0x%x)\n",
					inFframeInfo->Frame_width, inFframeInfo->Frame_height, inFframeInfo->private_data.mapConv_info.m_uiLumaBitDepth,
					inFframeInfo->addr0, inFframeInfo->private_data.mapConv_info.m_CompressedY[PA], inFframeInfo->private_data.mapConv_info.m_FbcYOffsetAddr[PA],
					inFframeInfo->addr1, inFframeInfo->private_data.mapConv_info.m_CompressedCb[PA], inFframeInfo->private_data.mapConv_info.m_FbcCOffsetAddr[PA]);				

		vDst_addr = (unsigned char *)_tcc_vsync_get_virtaddr(pDst_addr, PAGE_ALIGN(LumaTblSize) + PAGE_ALIGN(ChromaTblSize));
		if(vDst_addr != NULL)
		{
			vSrc_addr = (unsigned char *)ioremap_nocache(inFframeInfo->private_data.mapConv_info.m_FbcYOffsetAddr[PA], PAGE_ALIGN(LumaTblSize));
			if(vSrc_addr != NULL){
				memcpy(vDst_addr, vSrc_addr, LumaTblSize);
				iounmap((void*)vSrc_addr);
			}

			vSrc_addr = (unsigned char *)ioremap_nocache(inFframeInfo->private_data.mapConv_info.m_FbcCOffsetAddr[PA], PAGE_ALIGN(ChromaTblSize));
			if(vSrc_addr != NULL){
				memcpy(vDst_addr+PAGE_ALIGN(LumaTblSize), vSrc_addr, ChromaTblSize);
				iounmap((void*)vSrc_addr);
			}

			_tcc_vsync_release_virtaddr((void*)vDst_addr, pDst_addr, PAGE_ALIGN(LumaTblSize) + PAGE_ALIGN(ChromaTblSize));
			inFframeInfo->addr0 = inFframeInfo->private_data.mapConv_info.m_CompressedY[PA] = WmixerInfo->dst_y_addr;
			inFframeInfo->addr1 = inFframeInfo->private_data.mapConv_info.m_CompressedCb[PA] = WmixerInfo->dst_u_addr;
			inFframeInfo->private_data.mapConv_info.m_FbcYOffsetAddr[PA] = pDst_addr;
			inFframeInfo->private_data.mapConv_info.m_FbcCOffsetAddr[PA] = pDst_addr+PAGE_ALIGN(LumaTblSize);

			printk("### tcc_move_video_frame_simple pre-processing(%dx%d - BitDepth(%d) Y(0x%x-0x%x / 0x%x), Cb(0x%x-0x%x / 0x%x) :: End(0x%x)\n",
						inFframeInfo->Frame_width, inFframeInfo->Frame_height, inFframeInfo->private_data.mapConv_info.m_uiLumaBitDepth,
						inFframeInfo->addr0, inFframeInfo->private_data.mapConv_info.m_CompressedY[PA], inFframeInfo->private_data.mapConv_info.m_FbcYOffsetAddr[PA],
						inFframeInfo->addr1, inFframeInfo->private_data.mapConv_info.m_CompressedCb[PA], inFframeInfo->private_data.mapConv_info.m_FbcCOffsetAddr[PA],
						pDst_addr+PAGE_ALIGN(LumaTblSize)+PAGE_ALIGN(ChromaTblSize));
		}
	}
	else
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
	if( ret > 0 && inFframeInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] == 0x1 )
	{
		unsigned int LumaTblSize = 0, ChromaTblSize = 0;
		unsigned char *vSrc_addr, *vDst_addr;
		unsigned int pDst_addr = target_addr + szStream;

		if((szStream + PAGE_ALIGN(LumaTblSize) + PAGE_ALIGN(ChromaTblSize)) > target_size)
		{
			printk("Error :: should increase the size of 'fb_wmixer' pmap :: current(0x%x) -> need(0x%x) \n",
						target_size, szStream + PAGE_ALIGN(LumaTblSize) + PAGE_ALIGN(ChromaTblSize));
			return -4;
		}

		LumaTblSize = inFframeInfo->private_data.dtrcConv_info.m_iCompressionTableLumaSize;
		ChromaTblSize = inFframeInfo->private_data.dtrcConv_info.m_iCompressionTableChromaSize;

		dprintk("### tcc_move_video_frame_simple pre-processing(%dx%d - BitDepth(%d) Y(0x%x-0x%x / 0x%x), Cb(0x%x-0x%x / 0x%x)\n",
					inFframeInfo->Frame_width, inFframeInfo->Frame_height, inFframeInfo->private_data.dtrcConv_info.m_iBitDepthLuma,
					inFframeInfo->addr0, inFframeInfo->private_data.dtrcConv_info.m_CompressedY[PA], inFframeInfo->private_data.dtrcConv_info.m_CompressionTableLuma[PA],
					inFframeInfo->addr1, inFframeInfo->private_data.dtrcConv_info.m_CompressedCb[PA], inFframeInfo->private_data.dtrcConv_info.m_CompressionTableChroma[PA]);				

		vDst_addr = (unsigned char *)_tcc_vsync_get_virtaddr(pDst_addr, PAGE_ALIGN(512*1024));
		if(vDst_addr != NULL)
		{
			vSrc_addr = (unsigned char *)ioremap_nocache(inFframeInfo->private_data.dtrcConv_info.m_CompressionTableLuma[PA], PAGE_ALIGN(LumaTblSize));
			if(vSrc_addr != NULL){
				memcpy(vDst_addr, vSrc_addr, LumaTblSize);
				iounmap((void*)vSrc_addr);
			}

			vSrc_addr = (unsigned char *)ioremap_nocache(inFframeInfo->private_data.dtrcConv_info.m_CompressionTableChroma[PA], PAGE_ALIGN(ChromaTblSize));
			if(vSrc_addr != NULL){
				memcpy(vDst_addr+PAGE_ALIGN(LumaTblSize), vSrc_addr, ChromaTblSize);
				iounmap((void*)vSrc_addr);
			}

			_tcc_vsync_release_virtaddr((void*)vDst_addr, pDst_addr, PAGE_ALIGN(512*1024));
			inFframeInfo->addr0 = inFframeInfo->private_data.dtrcConv_info.m_CompressedY[PA] = WmixerInfo->dst_y_addr;
			inFframeInfo->addr1 = inFframeInfo->private_data.dtrcConv_info.m_CompressedCb[PA] = WmixerInfo->dst_u_addr;
			inFframeInfo->private_data.dtrcConv_info.m_CompressionTableLuma[PA] = pDst_addr;
			inFframeInfo->private_data.dtrcConv_info.m_CompressionTableChroma[PA] = pDst_addr+PAGE_ALIGN(LumaTblSize);

			printk("### tcc_move_video_frame_simple pre-processing(%dx%d - BitDepth(%d) Y(0x%x-0x%x / 0x%x), Cb(0x%x-0x%x / 0x%x) :: End(0x%x)\n",
						inFframeInfo->Frame_width, inFframeInfo->Frame_height, inFframeInfo->private_data.dtrcConv_info.m_iBitDepthLuma,
						inFframeInfo->addr0, inFframeInfo->private_data.dtrcConv_info.m_CompressedY[PA], inFframeInfo->private_data.dtrcConv_info.m_CompressionTableLuma[PA],
						inFframeInfo->addr1, inFframeInfo->private_data.dtrcConv_info.m_CompressedCb[PA], inFframeInfo->private_data.dtrcConv_info.m_CompressionTableChroma[PA],
						pDst_addr+PAGE_ALIGN(LumaTblSize)+PAGE_ALIGN(ChromaTblSize));
		}
	}
	else
#endif

	{
		inFframeInfo->addr0 = WmixerInfo->dst_y_addr;
		inFframeInfo->addr1 = WmixerInfo->dst_u_addr;
		inFframeInfo->addr2 = WmixerInfo->dst_v_addr;

		printk("### tcc_move_video_frame_simple pre-processing(%dx%d - 0x%x) \n", inFframeInfo->Frame_width, inFframeInfo->Frame_height, inFframeInfo->addr0);
	}

	return ret;
}
#endif

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
void tcc_video_info_backup(VSYNC_CH_TYPE type, struct tcc_lcdc_image_update *input_image)
{
	memcpy(&tccvid_lastframe[type].LastImage, input_image, sizeof(struct tcc_lcdc_image_update));
}
EXPORT_SYMBOL(tcc_video_info_backup);

static int tcc_fill_black_lastframe(struct tcc_lcdc_image_update* lastUpdated, unsigned int dest_addr, unsigned int dest_size, VSYNC_CH_TYPE type)
{
	unsigned char *pDest_Vaddr = NULL;
	unsigned int nLen = lastUpdated->Image_width * lastUpdated->Image_height * 2;

	if(dest_size <= 0)
		return -1;

	pDest_Vaddr = (unsigned char *)ioremap_nocache(dest_addr, PAGE_ALIGN(nLen));
	if(pDest_Vaddr != NULL)
	{
		memset(pDest_Vaddr, 0x00, lastUpdated->Image_width * lastUpdated->Image_height);
		memset((void*)((char*)pDest_Vaddr+lastUpdated->Image_width * lastUpdated->Image_height), 0x80, lastUpdated->Image_width * lastUpdated->Image_height/2);
		iounmap((void*)pDest_Vaddr);
		return 0;
	}

	return -1;
}

int tcc_video_check_last_frame(struct tcc_lcdc_image_update *ImageInfo)
{
	unsigned int type = tcc_vsync_get_video_ch_type(ImageInfo->Lcdc_layer);

	if(type >= VSYNC_MAX)
		return 0;

	spin_lock_irq(&LastFrame_lockDisp);
	if(tccvid_lastframe[type].enabled)
	{
		if( tccvid_lastframe[type].reason.Resolution && (tccvid_lastframe[type].LastImage.Frame_width == ImageInfo->Frame_width && tccvid_lastframe[type].LastImage.Frame_height == ImageInfo->Frame_height) )
		{
			spin_unlock_irq(&LastFrame_lockDisp);
			dprintk("----> skip frame due to last-frame :: res change %dx%d -> %dx%d\n",
					tccvid_lastframe[type].LastImage.Frame_width, tccvid_lastframe[type].LastImage.Frame_height,
					ImageInfo->Frame_width, ImageInfo->Frame_height);
			return 0;
		}
		if( tccvid_lastframe[type].reason.Codec && ((tccvid_lastframe[type].LastImage.codec_id == ImageInfo->codec_id) && ImageInfo->codec_id != 0) )
		{
			spin_unlock_irq(&LastFrame_lockDisp);
			dprintk("----> skip frame due to last-frame :: codec change 0x%x -> 0x%x\n",
					tccvid_lastframe[type].LastImage.codec_id, ImageInfo->codec_id);
			return 0;
		}
		printk("----> %s :: last-frame[%d] will be cleared : %dx%d, 0x%x \n", __func__, type, ImageInfo->Frame_width, ImageInfo->Frame_height, ImageInfo->addr0);
		tccvid_lastframe[type].enabled = tccvid_lastframe[type].reason.Resolution = tccvid_lastframe[type].reason.Codec = 0;
	}
	spin_unlock_irq(&LastFrame_lockDisp);

	tcc_video_info_backup(type, ImageInfo);

	return 1;
}

int tcc_move_video_last_frame(struct tcc_lcdc_image_update* lastUpdated, char bInterlaced, unsigned int dest_addr, unsigned int dest_size, int dest_fmt)
{
	struct file    *file;
	int ret = 0;

	if(dest_addr == 0 || dest_size == 0)
		return -2;

#ifdef USE_SCALER2_FOR_LASTFRAME
	{
		SCALER_TYPE LFScaler;

		memset(&LFScaler, 0x00, sizeof(SCALER_TYPE));

		LFScaler.responsetype	= SCALER_POLLING;

		LFScaler.src_ImgWidth   = lastUpdated->Frame_width;
		LFScaler.src_ImgHeight  = lastUpdated->Frame_height;

		LFScaler.src_winLeft    = lastUpdated->crop_left;
		LFScaler.src_winTop     = lastUpdated->crop_top;
		LFScaler.src_winRight   = lastUpdated->crop_right;
		LFScaler.src_winBottom 	= lastUpdated->crop_bottom;

		//source info
		LFScaler.src_Yaddr      = (char*)lastUpdated->addr0;
		LFScaler.src_Uaddr      = (char*)lastUpdated->addr1;
		LFScaler.src_Vaddr      = (char*)lastUpdated->addr2;
		LFScaler.src_fmt 	   	= lastUpdated->fmt;

		//destination info
		LFScaler.dest_ImgWidth   = lastUpdated->Image_width;
		LFScaler.dest_ImgHeight  = lastUpdated->Image_height;
		LFScaler.dest_winLeft    = 0;
		LFScaler.dest_winTop     = 0;
		LFScaler.dest_winRight   = lastUpdated->Image_width;
		LFScaler.dest_winBottom  = lastUpdated->Image_height;
		
		if(bInterlaced > 0 || lastUpdated->one_field_only_interlace)
			LFScaler.interlaced= true;
		else
			LFScaler.interlaced = false;

		LFScaler.dest_Yaddr	    = (char*)dest_addr;
		LFScaler.dest_fmt       = dest_fmt;

		file = filp_open(SCALER_PATH, O_RDWR, 0666);
		if(file){
			ret = file->f_op->unlocked_ioctl(file, TCC_SCALER_IOCTRL_KERENL, (unsigned long)&fbmixer);
			filp_close(file, 0);
		}
		else{
			printk(" Error :: %s open \n", SCALER_PATH);
			ret = -1;
		}

		if(ret <= 0){
			printk(" Error :: %s ctrl \n", SCALER_PATH);
			return -100;
		}
		lastUpdated->addr0	 	= (unsigned int)LFScaler.dest_Yaddr;
	}
#else
	{
		WMIXER_ALPHASCALERING_INFO_TYPE fbmixer;
		#if defined(CONFIG_EXT_FRAME_VIDEO_EFFECT)
		WMIXER_VIOC_INFO wmixer_id_info;
		unsigned int lut_plugin, lut_en;
		#endif

		memset(&fbmixer, 0x00, sizeof(WMIXER_ALPHASCALERING_INFO_TYPE));

#ifdef CONFIG_VIOC_10BIT
		if(lastUpdated->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10){
			fbmixer.src_fmt_ext_info = 10;
			fbmixer.dst_fmt_ext_info = 10;
		}
		else if(lastUpdated->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11){
			fbmixer.src_fmt_ext_info = 11;
			fbmixer.dst_fmt_ext_info = 11;
		}
		else
		{
			fbmixer.src_fmt_ext_info = 8;
			fbmixer.dst_fmt_ext_info = 8;
		}
#endif

#if defined(CONFIG_VIOC_10BIT) && defined(CONFIG_USE_FORCE_8BIT_OUT_LASTFRAME)
		fbmixer.dst_fmt_ext_info = 8;
#endif
		fbmixer.rsp_type        = WMIXER_POLLING;
		
		fbmixer.src_img_width   = lastUpdated->Frame_width;
		fbmixer.src_img_height  = lastUpdated->Frame_height;

		fbmixer.src_win_left    = lastUpdated->crop_left;
		fbmixer.src_win_top     = lastUpdated->crop_top;
		fbmixer.src_win_right   = lastUpdated->crop_right;
		fbmixer.src_win_bottom  = lastUpdated->crop_bottom;

		fbmixer.src_y_addr      = (unsigned int)lastUpdated->addr0;
		fbmixer.src_u_addr      = (unsigned int)lastUpdated->addr1;
		fbmixer.src_v_addr      = (unsigned int)lastUpdated->addr2;
		fbmixer.src_fmt 		= lastUpdated->fmt;

		fbmixer.dst_img_width   = lastUpdated->Image_width;
		fbmixer.dst_img_height  = lastUpdated->Image_height;
		fbmixer.dst_win_left    = 0;
		fbmixer.dst_win_top     = 0;
		fbmixer.dst_win_right   = lastUpdated->Image_width;
		fbmixer.dst_win_bottom  = lastUpdated->Image_height;

#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
		if(lastUpdated->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0 || lastUpdated->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0) {
       #if 1//def CONFIG_ANDROID
            if(lastUpdated->output_toMemory && lastUpdated->m2m_mode)       // m2m mode
            	fbmixer.mc_num = 0;
            else    // onthelfy mode
               	fbmixer.mc_num = 1;
       #else
            if(lastUpdated->output_toMemory && lastUpdated->m2m_mode)       // m2m mode
               	fbmixer.mc_num = 1;
            else    // onthelfy mode
               	fbmixer.mc_num = 0;
       #endif
	   #if defined(CONFIG_VIOC_MAP_DECOMP)
		   if(lastUpdated->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0){
		   		fbmixer.src_fmt_ext_info = 0x10;
				memcpy(&fbmixer.mapConv_info, &lastUpdated->private_data.mapConv_info, sizeof(hevc_MapConv_info_t));
		   }
	   #endif
	   #if defined(CONFIG_VIOC_DTRC_DECOMP)
		   if(lastUpdated->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0){
		   		fbmixer.src_fmt_ext_info = 0x20;
				memcpy(&fbmixer.dtrcConv_info, &lastUpdated->private_data.dtrcConv_info, sizeof(vp9_compressed_info_t));
		   }
	   #endif
		}
#endif

		if(bInterlaced > 0 || lastUpdated->one_field_only_interlace)
			fbmixer.interlaced= true;
		else
			fbmixer.interlaced = false;

		fbmixer.dst_y_addr      = (unsigned int)dest_addr;
		fbmixer.dst_fmt         = dest_fmt;

		file = filp_open(WMIXER_PATH, O_RDWR, 0666);
		if(file){
			#if defined(CONFIG_EXT_FRAME_VIDEO_EFFECT)
			ret = file->f_op->unlocked_ioctl(file, TCC_WMIXER_VIOC_INFO_KERNEL, (unsigned long)&wmixer_id_info);

			lut_plugin = tcc_get_lut_plugin(VIOC_LUT_COMP1);
			lut_en = tcc_get_lut_enable(VIOC_LUT_COMP1);
			tcc_set_lut_enable(VIOC_LUT_COMP1, false);
			tcc_set_lut_plugin(VIOC_LUT_COMP1, wmixer_id_info.wdma);
			tcc_set_lut_enable(VIOC_LUT_COMP1, 1);
			#endif//
			ret = file->f_op->unlocked_ioctl(file, TCC_WMIXER_ALPHA_SCALING_KERNEL, (unsigned long)&fbmixer);

			#if defined(CONFIG_EXT_FRAME_VIDEO_EFFECT)
			// lut set to before state.
			tcc_set_lut_enable(VIOC_LUT_COMP1, true);
			tcc_set_lut_plugin(VIOC_LUT_COMP1, lut_plugin);
			#endif//

			filp_close(file, 0);
		}
		else{
			printk(" Error :: %s open \n", WMIXER_PATH);
			ret = -1;
		}

		if(ret <= 0){
			printk(" Error :: %s ctrl \n", WMIXER_PATH);
			return -100;
		}

		lastUpdated->addr0       = (unsigned int)fbmixer.dst_y_addr;				
	}
#endif

	return 0;
}

int tcc_origin_video_off_for_last_frame(tcc_video_disp *p, struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *lastUpdated, VSYNC_CH_TYPE type)
{
#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
	if(p->deinterlace_mode &&!p->interlace_bypass_lcdc)
	{
		if(!p->output_toMemory) {
			TCC_VIQE_DI_DeInit60Hz();
		}
        else if(p->output_toMemory && p->m2m_mode) {
			if (type == VSYNC_MAIN)
				TCC_VIQE_DI_DeInit60Hz_M2M(RDMA_VIDEO);
			else
				TCC_VIQE_DI_Sub_DeInit60Hz_M2M(RDMA_VIDEO_SUB);
		}
	}
	else if(p->output_toMemory && p->m2m_mode)
	{
		if (type == VSYNC_MAIN)
			TCC_VIQE_DI_DeInit60Hz_M2M(RDMA_VIDEO);
		else
			TCC_VIQE_DI_Sub_DeInit60Hz_M2M(RDMA_VIDEO_SUB);
	}
#endif

	lastUpdated->enable 		= 0;
	if (type == VSYNC_MAIN) {
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
		if((DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE){
			lastUpdated->Lcdc_layer = RDMA_VIDEO_SUB;
			tcc_video_rdma_off(pdp_data, lastUpdated, p->outputMode, 0);
		}
#endif
		lastUpdated->Lcdc_layer 	= RDMA_VIDEO;
	} else {
		lastUpdated->Lcdc_layer 	= RDMA_VIDEO_SUB;
	}
	tcc_video_rdma_off(pdp_data, lastUpdated, p->outputMode, 0);

	return 0;
}

int tcc_video_last_frame(void *pVSyncDisp, struct stTcc_last_frame iLastFrame, struct tcc_lcdc_image_update *lastUpdated, unsigned int type)
{
	int ret = 0;
	tcc_video_disp *p = (tcc_video_disp *)pVSyncDisp;
	struct tcc_dp_device *dp_device = NULL;
	unsigned int target_address = 0x00;
	unsigned int target_size = 0x00;

	if(!p)
	{
		p = &tccvid_vsync[type];
	}

	dp_device = tca_fb_get_displayType(p->outputMode);

	if(tca_fb_divide_get_status())
	{
		printk("[%d] %s, Skip last frame because 3D UI is displayed!!\n", type, __func__);
		return 0;
	}

	if(tcc_vsync_mvc_status == 1)
	{
		dprintk("[%d] %s is returned because of mvc frame view\n", type, __func__);
		return 0;
	}

	memcpy(lastUpdated, &tccvid_lastframe[type].LastImage, sizeof(struct tcc_lcdc_image_update));

	if(lastUpdated->enable == 0){
		printk("---->[%d] return last-frame :: The channel has already disabled. info(%dx%d), resolution_changed(%d), codec_changed(%d)\n", type,
					lastUpdated->Frame_width, lastUpdated->Frame_height, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);
		return -100;
	}

/*
	if(lastUpdated->Lcdc_layer != RDMA_VIDEO){
		printk("----> return last-frame :: The channel(%d) is not Main-Video. info(%dx%d), resolution_changed(%d), codec_changed(%d)\n",
					lastUpdated->Lcdc_layer, lastUpdated->Frame_width, lastUpdated->Frame_height, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);
		return -100;
	}
*/
	if(lastUpdated->outputMode != p->outputMode-TCC_OUTPUT_LCD){
		printk("---->[%d] return last-frame :: mode is different between %d and %d \n", type, lastUpdated->outputMode, p->outputMode-TCC_OUTPUT_LCD);
		return -100;
	}

	if(lastUpdated->Frame_width == 0 || lastUpdated->Frame_height == 0)
	{
		printk("[%d]Error :: no setting for Last-Frame \n", type);
		return -1;
	}

	if( (iLastFrame.codec_id != 0) && (lastUpdated->codec_id != iLastFrame.codec_id) )
	{
		printk("[%d]Error :: codec mismatch \n", type);
		return -1;
	}

	if(tccvid_lastframe[type].enabled){
		if(iLastFrame.reason&LASTFRAME_FOR_RESOLUTION_CHANGE)
			tccvid_lastframe[type].reason.Resolution = 1;
		else if(iLastFrame.reason&LASTFRAME_FOR_CODEC_CHANGE)
			tccvid_lastframe[type].reason.Codec = 1;
		printk("---->[%d] TCC_LCDC_VIDEO_KEEP_LASTFRAME returned due to (LastFrame %d)!! resolution_changed(%d), codec_changed(%d) \n", type, tccvid_lastframe[type].enabled, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);
		return 0;
	}

	if(!dp_device)
	{
		printk("[%d]Error :: Display device is null. outputMode is %d\n", type, p->outputMode);
		return -1;
	}

	if( tccvid_lastframe[type].CurrImage.addr0 == tccvid_lastframe[type].pmapBuff.base )
		target_address = tccvid_lastframe[type].pmapBuff.base + (tccvid_lastframe[type].pmapBuff.size / 2);
	else
		target_address = tccvid_lastframe[type].pmapBuff.base;
	target_size = (tccvid_lastframe[type].pmapBuff.size / 2);

	if(type == VSYNC_MAIN && tccvid_lastframe[type].support && target_size)
	{
		if( p->outputMode == TCC_OUTPUT_HDMI || p->outputMode== TCC_OUTPUT_COMPOSITE || p->outputMode == TCC_OUTPUT_COMPONENT
#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
				|| p->outputMode == TCC_OUTPUT_LCD 
#endif
		)
		{
			tccvid_lastframe[type].enabled = 1;
			tccvid_lastframe[type].nCount = 0;
			tccvid_lastframe[type].reason.Resolution = iLastFrame.reason&0x1;
			tccvid_lastframe[type].reason.Codec = iLastFrame.reason&0x2;

			if(p->vsync_started == 1){
				msleep(p->vsync_interval*2);
				spin_lock_irq(&vsync_lock);
				tcc_vsync_pop_all_buffer(&p->vsync_buffer, 2);
				spin_unlock_irq(&vsync_lock);				
			}
			else{
				msleep(40);
				//goto Screen_off;
			}

			memcpy(lastUpdated, &tccvid_lastframe[type].LastImage, sizeof(struct tcc_lcdc_image_update));

	#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
			//if(lastUpdated->private_data.optional_info[VID_OPT_CONTENT_TYPE] == ATTR_DV_WITHOUT_BC){
			if(lastUpdated->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO] != 0){
				spin_lock_irq(&LastFrame_lockDisp);
				tcc_origin_video_off_for_last_frame(p, dp_device, lastUpdated, type);
				spin_unlock_irq(&LastFrame_lockDisp);

				printk("---->[%d]  TCC_LCDC_VIDEO_KEEP_LASTFRAME to diable only (DV)RDMA channel. info(%dx%d), resolution_changed(%d), codec_changed(%d) \n", type,
								lastUpdated->Frame_width, lastUpdated->Frame_height, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);
				return -100;
			}
	#endif

	#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
				// this code prevent a error of transparent on layer0 after stopping v
			if(lastUpdated->Lcdc_layer == 0 && lastUpdated->enable == 0)
			{
				printk("Last-frame[%d] for a error prevention \n", type);
				spin_lock_irq(&vsync_lock);
				p->outputMode = -1;
				spin_unlock_irq(&vsync_lock);
			}
	#endif

	#if 1  // for PIP mode with dual-vsync!! // No need to display on M2M mode.
			if(p->deinterlace_mode)
			{
				if(!p->duplicateUseFlag)
				{
					if(p->output_toMemory && p->m2m_mode) {
						return 0;
					}
				}
			}
			else if(p->output_toMemory && p->m2m_mode)
			{
				return 0;
			}
	#endif
			printk("---->[%d] TCC_LCDC_VIDEO_KEEP_LASTFRAME(%d) called with reason(%d), out-Mode(%d) target(0x%x)!! \n",
					type, tccvid_lastframe[type].support, iLastFrame.reason, p->outputMode, target_address);

			if( (ret = tcc_move_video_last_frame(lastUpdated, p->deinterlace_mode, target_address, target_size, LAST_FRAME_FORMAT)) < 0 ){
				goto Screen_off;
			}

			lastUpdated->Frame_width  	= lastUpdated->Image_width;
			lastUpdated->Frame_height 	= lastUpdated->Image_height;	
			lastUpdated->fmt	 		= LAST_FRAME_FORMAT; // W x H * 2 = Max 4Mb

       	#ifdef CONFIG_VIOC_MAP_DECOMP
			if(lastUpdated->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0) {
				lastUpdated->private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
			}
       	#endif
       	#ifdef CONFIG_VIOC_DTRC_DECOMP
			if(lastUpdated->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0) {
				lastUpdated->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
			}
       	#endif

		#ifdef CONFIG_USE_FORCE_8BIT_OUT_LASTFRAME
			lastUpdated->private_data.optional_info[VID_OPT_BIT_DEPTH] = 0;
		#endif

			printk("###[%d] TCC_LCDC_VIDEO_KEEP_LASTFRAME RDMA(0x%p) :: Start info(%dx%d), Reason(0x%x) \n", type, dp_device->rdma_info[RDMA_LASTFRM].virt_addr,
						lastUpdated->Frame_width, lastUpdated->Frame_height, iLastFrame.reason);

			{
				lastUpdated->enable 		= 1;
				lastUpdated->on_the_fly		= 0;
				lastUpdated->one_field_only_interlace = 0;
				lastUpdated->crop_left 		= lastUpdated->crop_top = 0;
				lastUpdated->crop_right 	= lastUpdated->Frame_width;
				lastUpdated->crop_bottom 	= lastUpdated->Frame_height;			
				lastUpdated->Lcdc_layer 	= RDMA_LASTFRM;
				tccvid_lastframe[type].pRDMA = dp_device->rdma_info[RDMA_LASTFRM].virt_addr;

				spin_lock_irq(&LastFrame_lockDisp);
				tca_scale_display_update(dp_device, lastUpdated);
				spin_unlock_irq(&LastFrame_lockDisp);
			}
Screen_off:
			tcc_origin_video_off_for_last_frame(p, dp_device, lastUpdated, type);
			printk("###[%d] TCC_LCDC_VIDEO_KEEP_LASTFRAME End \n", type);
		}
		else
		{
			printk("[%d]TCC_LCDC_VIDEO_KEEP_LASTFRAME skip :: reason(p->outputMode == %d)  \n", type, p->outputMode);
		}

		return 0;
	}
	else
	{
		if(p->outputMode == TCC_OUTPUT_HDMI ||p->outputMode == TCC_OUTPUT_COMPOSITE || p->outputMode == TCC_OUTPUT_COMPONENT 
	#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
			|| p->outputMode ==  TCC_OUTPUT_LCD
	#endif
		)
		{
			WMIXER_INFO_TYPE WmixerInfo;

			tccvid_lastframe[type].enabled = 1;
			tccvid_lastframe[type].nCount = 0;
			tccvid_lastframe[type].reason.Resolution = iLastFrame.reason&LASTFRAME_FOR_RESOLUTION_CHANGE;
			tccvid_lastframe[type].reason.Codec = iLastFrame.reason&LASTFRAME_FOR_CODEC_CHANGE ? 1:0;

			if(p->vsync_started == 1)
				msleep(p->vsync_interval*2);
			memcpy(lastUpdated, &tccvid_lastframe[type].LastImage, sizeof(struct tcc_lcdc_image_update));

			if(p->deinterlace_mode > 0 && p->vsync_started){
				if(!tccvid_lastframe[type].reason.Codec){
					printk("---->[%d] return last-frame :: deinterlace_mode %d. info(%dx%d), resolution_changed(%d), codec_changed(%d) \n", type, p->deinterlace_mode,
								tccvid_lastframe[type].LastImage.Frame_width, tccvid_lastframe[type].LastImage.Frame_height, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);
					return -100;
				}
			}

			if(p->vsync_started == 1){
				spin_lock_irq(&vsync_lock);
				tcc_vsync_pop_all_buffer(&p->vsync_buffer, 11);
				spin_unlock_irq(&vsync_lock);
			}

			if(0)//type == VSYNC_SUB0 && tccvid_lastframe[type].support && (p->output_toMemory && p->m2m_mode))
			{				
				if(0 <= tcc_fill_black_lastframe(lastUpdated, target_address, target_size, type))
				{
					lastUpdated->enable 		= 1;
					lastUpdated->on_the_fly		= 0;
					lastUpdated->addr0			= target_address;
					lastUpdated->addr1			= target_address + (lastUpdated->Image_width*lastUpdated->Image_height);
					lastUpdated->one_field_only_interlace = 0;
					lastUpdated->crop_left 		= lastUpdated->crop_top = 0;
					lastUpdated->crop_right 	= lastUpdated->Image_width;
					lastUpdated->crop_bottom 	= lastUpdated->Image_height;			
					lastUpdated->Frame_width  	= lastUpdated->Image_width;
					lastUpdated->Frame_height 	= lastUpdated->Image_height;	
					lastUpdated->fmt	 		= TCC_LCDC_IMG_FMT_YUV420ITL0; // W x H * 2 = Max 4Mb
					lastUpdated->private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
					lastUpdated->private_data.optional_info[VID_OPT_BIT_DEPTH] = 0;
					lastUpdated->private_data.optional_info[VID_OPT_HAVE_USERDATA] = 0;
					lastUpdated->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
					tccvid_lastframe[type].pRDMA = NULL;

					{
						spin_lock_irq(&LastFrame_lockDisp);
						tca_scale_display_update(dp_device, lastUpdated);
						spin_unlock_irq(&LastFrame_lockDisp);
					}

					printk("---->[%d]  black TCC_LCDC_VIDEO_KEEP_LASTFRAME. info(%dx%d), resolution_changed(%d), codec_changed(%d) \n", type,
								lastUpdated->Frame_width, lastUpdated->Frame_height, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);
					return 0;		
				}
				else {
					//do not process anythings!!
					return -100;
				}
			}
			else if( (tccvid_lastframe[type].reason.Codec)
				|| (type == VSYNC_SUB0)
				|| (type == VSYNC_MAIN && !tccvid_lastframe[type].support)
				|| (target_size == 0)
			)
			{
				spin_lock_irq(&LastFrame_lockDisp);
				tcc_origin_video_off_for_last_frame(p, dp_device, lastUpdated, type);
				spin_unlock_irq(&LastFrame_lockDisp);

				printk("---->[%d]  fake TCC_LCDC_VIDEO_KEEP_LASTFRAME to diable only RDMA channel. info(%dx%d), resolution_changed(%d), codec_changed(%d) \n", type,
								lastUpdated->Frame_width, lastUpdated->Frame_height, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);
				return -100;
			}
			else
			{
				struct file *file;
				file = filp_open(WMIXER_PATH, O_RDWR, 0666);
				ret = tcc_move_video_frame_simple(file, lastUpdated, &WmixerInfo, target_address, target_size, LAST_FRAME_FORMAT);
				filp_close(file, 0);
				if(ret <= 0){
					printk(" [%d] Error[%d] :: wmixer ctrl \n", type, ret);
					return -100;
				}

				lastUpdated->fmt = WmixerInfo.dst_fmt;
				lastUpdated->addr0 = WmixerInfo.dst_y_addr;
				lastUpdated->addr1 = WmixerInfo.dst_u_addr;
				lastUpdated->addr2 = WmixerInfo.dst_v_addr;
			}

			spin_lock_irq(&LastFrame_lockDisp);
			printk("---->[%d] fake TCC_LCDC_VIDEO_KEEP_LASTFRAME Start info(%dx%d), resolution_changed(%d), codec_changed(%d) \n", type, lastUpdated->Frame_width, lastUpdated->Frame_height, tccvid_lastframe[type].reason.Resolution, tccvid_lastframe[type].reason.Codec);

			switch(p->outputMode)
			{
				case TCC_OUTPUT_NONE:
					break;
			#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
				case TCC_OUTPUT_LCD:
					tca_scale_display_update(dp_device, lastUpdated);
					break;
			#endif
				case TCC_OUTPUT_HDMI:
				case TCC_OUTPUT_COMPONENT:
				case TCC_OUTPUT_COMPOSITE:
					tca_scale_display_update(dp_device, lastUpdated);
					break;
				default:
					break;
			}

			printk("---->[%d] fake TCC_LCDC_VIDEO_KEEP_LASTFRAME End \n", type);
			spin_unlock_irq(&LastFrame_lockDisp);

			return -0x100;
		}
		else
		{
			printk("fake[%d] TCC_LCDC_VIDEO_KEEP_LASTFRAME skip :: reason(p->outputMode == %d)  \n", type, p->outputMode);
		}
	}

	return -1;
}
EXPORT_SYMBOL(tcc_video_last_frame);

void tcc_video_clear_last_frame(unsigned int lcdc_layer, bool reset)
{
	unsigned int type = tcc_vsync_get_video_ch_type(lcdc_layer);

	if(type >= VSYNC_MAX)
		return;

	if(tca_fb_divide_get_status())
	{
		//printk("%s, 3D UI(%s) is displayed!!\n", __func__);
		return;
	}

    if(tccvid_lastframe[type].support)
    {
        if( tccvid_lastframe[type].pRDMA != NULL /*&& tccvid_lastframe[type].nCount >= 2*/){
            VIOC_RDMA_SetImageDisable(tccvid_lastframe[type].pRDMA);
			printk("----> lastframe[%d] cleared(%d)!! 0x%p \n", type, tccvid_lastframe[type].nCount, tccvid_lastframe[type].pRDMA);
			tccvid_lastframe[type].pRDMA = NULL;
        }
    }
	tccvid_lastframe[type].nCount += 1;

	if(reset){
		dprintk("----> lastframe[%d] reset variables \n", type);
		tccvid_lastframe[type].enabled = tccvid_lastframe[type].reason.Resolution = tccvid_lastframe[type].reason.Codec = 0;
		tccvid_lastframe[type].LastImage.enable = 0;
	}
	
}

int tcc_video_ctrl_last_frame(int type, int enable)
{	
	#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME_STB_ENABLE)
	tccvid_lastframe[type].support = 1;
	#else
	tccvid_lastframe[type].support = enable;
	#endif

	return 0;
}

#ifdef CONFIG_DISPLAY_EXT_FRAME
int tcc_display_ext_frame(struct tcc_lcdc_image_update *lastUpdated, char bInterlaced, int output_mode)
{
	struct tcc_dp_device *dp_device = tca_fb_get_displayType(output_mode);
	unsigned int type = tcc_vsync_get_video_ch_type(lastUpdated->Lcdc_layer);
	int ret = 0;

	if(type != VSYNC_MAIN)
		return 0;

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if(VIOC_CONFIG_DV_GET_EDR_PATH() && (ATTR_DV_WITHOUT_BC == vioc_get_video_attribute())){
		printk("can't support it due to DV \n");
		return 0; // may need to disable original Video-RDMA.
	}
#endif

	mutex_lock(&ext_mutex);

	if(lastUpdated->enable)
	{
		unsigned int dest_addr = 0x00;
		struct tcc_lcdc_image_update iImage;

		memcpy(&iImage, lastUpdated, sizeof(struct tcc_lcdc_image_update));

		if(iImage.Frame_width == 0 || iImage.Frame_height == 0)
		{
			printk("----> TCC_LCDC_VIDEO_CTRL_EXT_FRAME Error :: no setting for Ext-Frame \n");
			ret = -1;
			goto Error;
		}

		if(bCamera_mode)
		{
			printk("----> TCC_LCDC_VIDEO_CTRL_EXT_FRAME Info Cam(%d):: already enabled. \n", bCamera_mode);
			bExt_Frame = 1;
			ret = -1;
			goto Error;
		}

		if(!dp_device)
		{
			printk("Error :: Display device is null. outputMode is %d\n", output_mode);
			ret = -1;
			goto Error;
		}

		if(bToggleBuffer)
			dest_addr = tccvid_lastframe[type].pmapBuff.base;
		else
			dest_addr = tccvid_lastframe[type].pmapBuff.base + (tccvid_lastframe[type].pmapBuff.size/2);
		bToggleBuffer = ~bToggleBuffer;

		memcpy(&last_backup, &iImage, sizeof(struct tcc_lcdc_image_update));

		if( (ret = tcc_move_video_last_frame(&iImage, bInterlaced, dest_addr, (tccvid_lastframe[type].pmapBuff.size/2), LAST_FRAME_FORMAT)) < 0 ){
			ret = -1;
			goto Error;
		}

		iImage.Frame_width  	= iImage.Image_width;
		iImage.Frame_height 	= iImage.Image_height;
		iImage.fmt	 			= LAST_FRAME_FORMAT; // W x H * 2 = Max 4Mb

	#ifdef CONFIG_VIOC_MAP_DECOMP
		if(iImage.private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0) {
			iImage.private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
		}
	#endif

	#ifdef CONFIG_VIOC_DTRC_DECOMP
		if(iImage.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0) {
			iImage.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
		}
	#endif

	#ifdef CONFIG_USE_FORCE_8BIT_OUT_LASTFRAME
		iImage.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0;
	#endif

		printk("### TCC_LCDC_VIDEO_CTRL_EXT_FRAME Phy(0x%p) :: Start info(%dx%d, %d,%d ~ %dx%d), Display(%d,%d ~ %dx%d)\n", dest_addr,
					last_backup.Frame_width, last_backup.Frame_height,
					last_backup.crop_left, last_backup.crop_top, last_backup.crop_right, last_backup.crop_bottom,
					iImage.offset_x, iImage.offset_y, iImage.Image_width, iImage.Image_height);

		{
			iImage.enable 		= 1;
			iImage.on_the_fly	= 0;
			iImage.one_field_only_interlace = 0;
			iImage.crop_left 	= iImage.crop_top = 0;
			iImage.crop_right 	= iImage.Frame_width;
			iImage.crop_bottom 	= iImage.Frame_height;
			iImage.Lcdc_layer 	= RDMA_LASTFRM;
			pExtFrame_RDMABase  = dp_device->rdma_info[RDMA_LASTFRM].virt_addr;

			spin_lock_irq(&LastFrame_lockDisp);
			tca_scale_display_update(dp_device, &iImage);
			spin_unlock_irq(&LastFrame_lockDisp);

			bExt_Frame = 1;
		}
	}
	else
	{
		int alive = bExt_Frame;
		if(bExt_Frame)
		{
			if(!bCamera_mode)
			{
				if( pExtFrame_RDMABase != NULL ) {
					VIOC_RDMA_SetImageDisable(pExtFrame_RDMABase);
					printk("----> ext frame cleared!! 0x%p \n", pExtFrame_RDMABase);
				}
			}
			last_backup.enable = bExt_Frame = 0;
		}

		ret = alive;
    }

Error:
	mutex_unlock(&ext_mutex);

	return ret;
}
#endif

void tcc_ext_mutex(char lock, char bCamera)
{
#ifdef CONFIG_DISPLAY_EXT_FRAME
	if(lock){
		mutex_lock(&ext_mutex);
	}
	else{
		mutex_unlock(&ext_mutex);
	}
	bCamera_mode = bCamera;
#endif
}
EXPORT_SYMBOL(tcc_ext_mutex);

int tcc_ctrl_ext_frame(char enable)
{
#ifdef CONFIG_DISPLAY_EXT_FRAME
	struct tcc_lcdc_image_update iImage;

	if(!enable)
	{
		if(bExt_Frame) {
			memcpy(&iImage, &last_backup, sizeof(struct tcc_lcdc_image_update));
			iImage.enable = 0;
			tcc_display_ext_frame((void*)&iImage, 0, Output_SelectMode);
		}
	}
	else
	{
		memcpy(&iImage, &last_backup, sizeof(struct tcc_lcdc_image_update));
		iImage.enable = enable;
		tcc_display_ext_frame((void*)&iImage, iImage.deinterlace_mode, Output_SelectMode);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(tcc_ctrl_ext_frame);

#endif

#ifdef CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME
void tcc_video_frame_backup(struct tcc_lcdc_image_update *Image)
{
	unsigned int type = tcc_vsync_get_video_ch_type(Image->Lcdc_layer);

	if(type >= VSYNC_MAX)
		return;

	if(Image->enable){
		if(tccvid_lastframe[type].CurrImage.enable != 0 && (tccvid_lastframe[type].CurrImage.buffer_unique_id == Image->buffer_unique_id) && Image->bSize_Changed == 0){
			dprintk("@@@@@@@@@ return same buffer_id (%d) for seek \n", Image->buffer_unique_id);
			return;
		}
		memcpy(&tccvid_lastframe[type].CurrImage, Image, sizeof(struct tcc_lcdc_image_update));
	}
}

int tcc_video_swap_vpu_frame(tcc_video_disp *p, int idx, WMIXER_INFO_TYPE *WmixerInfo, struct tcc_lcdc_image_update *TempImage, VSYNC_CH_TYPE type)
{
	int ret = 0;
	struct file *file = NULL;
	unsigned int target_address = 0x00;
	unsigned int target_size = 0x00;

	if( p->outputMode == Output_SelectMode )
	{
		if(!p->vsync_started || tccvid_lastframe[type].CurrImage.enable == 0 || tccvid_lastframe[type].enabled || tccvid_lastframe[type].CurrImage.Lcdc_layer != RDMA_VIDEO){
			if(tccvid_lastframe[type].CurrImage.Lcdc_layer != RDMA_VIDEO){
				printk("---->[%d] return tcc_video_swap_vpu_frame :: The channel(%d) is not Main-Video. info(%dx%d)\n", type,
						tccvid_lastframe[type].CurrImage.Lcdc_layer, tccvid_lastframe[type].CurrImage.Frame_width, tccvid_lastframe[type].CurrImage.Frame_height);
			}
			else {
				printk("---->[%d] return tcc_video_swap_vpu_frame :: reason %d/%d/%d \n", type, p->vsync_started, tccvid_lastframe[type].CurrImage.enable, tccvid_lastframe[type].enabled);
			}
			ret = -1;
			if(p->vsync_started && (tccvid_lastframe[type].CurrImage.enable || tccvid_lastframe[type].enabled)){
				tcc_video_clear_frame(p, idx, 12);
			}
			goto Error;
		}

		if(tccvid_lastframe[type].pmapBuff.size == 0)
			goto Error;

		file = filp_open(WMIXER_PATH, O_RDWR, 0666);
		tcc_video_clear_frame(p, idx, 3);
		msleep(p->vsync_interval*2); // wait 2 vsync based on 24hz!!

		if( tccvid_lastframe[type].CurrImage.addr0 == tccvid_lastframe[type].pmapBuff.base )
			target_address = tccvid_lastframe[type].pmapBuff.base + (tccvid_lastframe[type].pmapBuff.size / 2);
		else
			target_address = tccvid_lastframe[type].pmapBuff.base;
		target_size = (tccvid_lastframe[type].pmapBuff.size / 2);

		memcpy(TempImage, &tccvid_lastframe[type].LastImage, sizeof(struct tcc_lcdc_image_update));

		ret = tcc_move_video_frame_simple(file, TempImage, WmixerInfo, target_address, target_size, TempImage->fmt);
		if( ret > 0 )
		{
			printk("%s :: ---->[%d] start tcc_video_swap_vpu_frame :: %d En:%d/%d, vsync: %d ms, I(%d)D(%d)/M(%d/%d)\n", __func__, type,
					TempImage->buffer_unique_id, TempImage->enable, tccvid_lastframe[type].CurrImage.enable, p->vsync_interval,
					p->deinterlace_mode, p->duplicateUseFlag, p->output_toMemory, p->m2m_mode);
			//TempImage->buffer_unique_id = p->vsync_buffer.available_buffer_id_on_vpu;

			if(p->deinterlace_mode)
			{
				if(p->duplicateUseFlag)
				{
					int current_time;
					
					viqe_render_init();

					current_time = tcc_vsync_get_time(p);
					viqe_render_frame(TempImage, p->vsync_interval, current_time, VIQE_RESET_NONE);
					viqe_render_field(current_time);
				}
				else{
					if(p->output_toMemory && p->m2m_mode) {
						//TCC_VIQE_DI_Run60Hz_M2M(TempImage,VIQE_RESET_NONE); // No need to display on M2M mode.
					} else {
						if (TempImage->Lcdc_layer == RDMA_VIDEO)
							TCC_VIQE_DI_Run60Hz(TempImage, VIQE_RESET_NONE);
					}
				}
			}
			else if(p->output_toMemory && p->m2m_mode) // for PIP mode with dual-vsync!!
			{
				struct tcc_lcdc_image_update M2mImage;
				TCC_VIQE_DI_GET_SOURCE_INFO(&M2mImage, TempImage->Lcdc_layer);
				dprintk("%s : L(%d) U-%d(D-%d/%d), I-%d, addr(0x%x), dtrc(%d)\n",__func__, TempImage->Lcdc_layer, M2mImage.buffer_unique_id, M2mImage.private_data.optional_info[VID_OPT_DISP_OUT_IDX],
						M2mImage.private_data.optional_info[VID_OPT_BUFFER_ID], M2mImage.deinterlace_mode,
						M2mImage.addr0, M2mImage.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO]);
				if(M2mImage.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0){
					ret = tcc_move_video_frame_simple(file, &M2mImage, WmixerInfo, target_address, target_size, M2mImage.fmt);
					TCC_VIQE_DI_Run60Hz_M2M(&M2mImage,VIQE_RESET_NONE); // No need to display on M2M mode.
				}
			}
			else
			{
				struct tcc_dp_device *dp_device = tca_fb_get_displayType(p->outputMode);

				if(dp_device != NULL)
				{
					switch(p->outputMode)
					{
						case TCC_OUTPUT_NONE:
							break;

						#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
						case TCC_OUTPUT_LCD:
							tca_scale_display_update(dp_device, TempImage);
							break;
						#endif

						case TCC_OUTPUT_HDMI:
						case TCC_OUTPUT_COMPONENT:
						case TCC_OUTPUT_COMPOSITE:
							tca_scale_display_update(dp_device, TempImage);
							break;

						default:
							break;
					}
				}
			}
			memcpy(&tccvid_lastframe[type].CurrImage, TempImage, sizeof(struct tcc_lcdc_image_update));

			msleep(p->vsync_interval*2); // wait 2 vsync based on 24hz!!
			printk("%s :: ----> video seek %d : backup %d \n", __func__, idx, tccvid_lastframe[type].CurrImage.buffer_unique_id) ;
		}
		else
		{
			printk("%s ---->[%d] Error[%d] :: wmixer ctrl \n", __func__, type, ret);
		}
		ret = tccvid_lastframe[type].CurrImage.buffer_unique_id;
	}
	else
	{
		printk("%s :: ---->[%d] can't start tcc_video_swap_vpu_frame due to outputmode mismatch(%d/%d)\n", __func__,
					type, p->outputMode, Output_SelectMode);
	}

Error:
	if(file != NULL)
		filp_close(file, 0);
	if(ret < 0)
		printk("%s :: ---->[%d] error tcc_video_swap_vpu_frame due to %d\n", __func__, type, ret);

	return ret;
}
#endif

static void print_vsync_input(char *str, struct tcc_lcdc_image_update *pIn)
{
	#if 0
	printk("\n %s \n", str);
	printk("Layer(%d), Enable(%d), Frame(%d x %d), Image(%d x %d), Offset(%d x %d) \n", 
				pIn->Lcdc_layer, pIn->enable, pIn->Frame_width, pIn->Frame_height, pIn->Image_width, pIn->Image_height, pIn->offset_x, pIn->offset_y);
	printk("Addr(0x%x / 0x%x / 0x%x), Fmt(%d), on_the_fly(%d) Crop(%d-%d-%d-%d) \n", pIn->addr0, pIn->addr1, pIn->addr2, pIn->fmt, pIn->on_the_fly,
				pIn->crop_top, pIn->crop_bottom, pIn->crop_left, pIn->crop_right);
	printk("Time(%d-%d), First(%d), Buffer_id(%d), overlay_used(%d), outMode(%d), outPath(%d) \n", 
				pIn->time_stamp, pIn->sync_time, pIn->first_frame_after_seek, pIn->buffer_unique_id, pIn->overlay_used_flag, pIn->outputMode, pIn->output_path);
	printk("DeIntl(%d), Odd_First(%d), m2m(%d), output_toM(%d), frm_intl(%d), one_field_only(%d) \n", 
				pIn->deinterlace_mode, pIn->odd_first_flag, pIn->m2m_mode, pIn->output_toMemory, pIn->frameInfo_interlace, pIn->one_field_only_interlace);
	printk("MVC(%d), base_Addr(0x%x / 0x%x / 0x%x), dest_Addr(0x%x / 0x%x / 0x%x) \n", 
				pIn->MVCframeView, pIn->MVC_Base_addr0, pIn->MVC_Base_addr1, pIn->MVC_Base_addr2, pIn->dst_addr0, pIn->dst_addr1, pIn->dst_addr2);
	printk("Max_buff(%d), ex_out(%d), fps(%d), bit_depth(%d), codec_id(%d), bSize_Change(%d) \n", 
				pIn->max_buffer, pIn->ex_output, pIn->fps, pIn->bit_depth, pIn->codec_id, pIn->bSize_Changed);
	printk("Priv[ %d x %d, fmt %d, offset(0x%x / 0x%x / 0x%x), name(%s), unique_addr(%d), copied(%d) \n", 
				pIn->private_data.width, pIn->private_data.height, pIn->private_data.format, pIn->private_data.offset[0], pIn->private_data.offset[1], pIn->private_data.offset[2],
				pIn->private_data.name, pIn->private_data.unique_addr, pIn->private_data.copied);
	printk("optional_info[%d/%d/%d/%d/%d - %d/%d/%d/%d/%d - %d/%d/%d/%d/%d - %d/%d/%d/%d/%d - %d/%d/%d/%d/%d - %d/%d/%d/%d/%d \n", 
				pIn->private_data.optional_info[VID_OPT_BUFFER_WIDTH], pIn->private_data.optional_info[VID_OPT_BUFFER_HEIGHT],
				pIn->private_data.optional_info[VID_OPT_FRAME_WIDTH], pIn->private_data.optional_info[VID_OPT_FRAME_HEIGHT], 
				pIn->private_data.optional_info[VID_OPT_BUFFER_ID], pIn->private_data.optional_info[VID_OPT_TIMESTAMP],
				pIn->private_data.optional_info[VID_OPT_SYNC_TIME], pIn->private_data.optional_info[VID_OPT_FLAGS], 
				pIn->private_data.optional_info[VID_OPT_FPS], pIn->private_data.optional_info[VID_OPT_MVC_DISP_IDX], 
				pIn->private_data.optional_info[VID_OPT_MVC_ADDR_0], pIn->private_data.optional_info[VID_OPT_MVC_ADDR_1], pIn->private_data.optional_info[VID_OPT_MVC_ADDR_2],
				pIn->private_data.optional_info[VID_OPT_MVC_ADDR_0], pIn->private_data.optional_info[VID_OPT_DISP_OUT_IDX], pIn->private_data.optional_info[VID_OPT_VPU_INST_IDX],
				pIn->private_data.optional_info[VID_OPT_HAVE_MC_INFO], pIn->private_data.optional_info[VID_OPT_LOCAL_PLAYBACK], 
				pIn->private_data.optional_info[VID_OPT_PLAYER_IDX], pIn->private_data.optional_info[VID_OPT_WIDTH_STRIDE], pIn->private_data.optional_info[VID_OPT_HEIGHT_STRIDE],
				pIn->private_data.optional_info[VID_OPT_BIT_DEPTH], pIn->private_data.optional_info[VID_OPT_HAVE_USERDATA], pIn->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO], 
				pIn->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO], pIn->private_data.optional_info[VID_OPT_CONTENT_TYPE], pIn->private_data.optional_info[26], pIn->private_data.optional_info[27], 
				pIn->private_data.optional_info[28], pIn->private_data.optional_info[29]);

	printk("^@New^^^^^^^^^^^^^ @@@ %d/%d, %03d :: TS: %04d / %04d  %d bpp Attr(%d), #BL(0x%x, %dx%d (%dx%d), 0x%x fmt) #EL(0x%x, %dx%d (%dx%d)) #OSD(0x%x/0x%x) #Reg(0x%x) #Meta(0x%x)\n",
			0, 0, pIn->buffer_unique_id, pIn->time_stamp, pIn->sync_time,
			pIn->private_data.optional_info[VID_OPT_BIT_DEPTH],
			pIn->private_data.optional_info[VID_OPT_CONTENT_TYPE],
			pIn->addr0, //pIn->private_data.offset[0],
			pIn->Frame_width, pIn->Frame_height, //pIn->private_data.optional_info[VID_OPT_FRAME_WIDTH], pIn->private_data.optional_info[VID_OPT_FRAME_HEIGHT],
			pIn->crop_right - pIn->crop_left, //pIn->private_data.optional_info[VID_OPT_BUFFER_WIDTH],
			pIn->crop_bottom - pIn->crop_top, //pIn->private_data.optional_info[VID_OPT_BUFFER_HEIGHT],
			pIn->fmt, //pIn->private_data.format,
			pIn->private_data.dolbyVision_info.el_offset[0],
			pIn->private_data.dolbyVision_info.el_frame_width, pIn->private_data.dolbyVision_info.el_frame_height,
			pIn->private_data.dolbyVision_info.el_buffer_width, pIn->private_data.dolbyVision_info.el_buffer_height,
			pIn->private_data.dolbyVision_info.osd_addr[0], pIn->private_data.dolbyVision_info.osd_addr[1],
			pIn->private_data.dolbyVision_info.reg_addr, pIn->private_data.dolbyVision_info.md_hdmi_addr);
	#endif
}


struct mutex vsync_io_mutex;
static long tcc_vsync_do_ioctl(unsigned int cmd, unsigned long arg, VSYNC_CH_TYPE type)
{
	int ret = 0;

	tcc_video_disp *p = &tccvid_vsync[type];
	p->type = type;

	mutex_lock(&vsync_io_mutex);
	{
		switch (cmd) {
		
			case TCC_LCDC_VIDEO_START_VSYNC:
			case TCC_LCDC_VIDEO_START_VSYNC_KERNEL:
				{
					struct tcc_lcdc_image_update *input_image = (struct tcc_lcdc_image_update *)kmalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);

					if(!input_image) {
						printk("%s-%d :: kmalloc fail \n", __func__, __LINE__);
						ret = -EFAULT;
						goto Error;
					}
		
					#if 0//defined(CONFIG_TCC_DISPLAY_MODE_USE)
					if(Output_SelectMode == TCC_OUTPUT_NONE)
					{
						//printk("### Error: there is no valid output on STB\n");
						ret = -EFAULT;
						goto TCC_VSYNC_OPEN_ERROR;
					}
					#endif

					if (cmd == TCC_LCDC_VIDEO_START_VSYNC_KERNEL) {
						if (NULL == memcpy((void *)input_image , (const void *)arg, sizeof(struct tcc_lcdc_image_update)))
							ret = -EFAULT;
					} else {
						if (copy_from_user((void *)input_image , (const void *)arg, sizeof(struct tcc_lcdc_image_update)))
							ret = -EFAULT;
					}

					if (ret != 0) {
						printk("fatal error") ;
						ret = -EFAULT;
						goto TCC_VSYNC_OPEN_ERROR;
					}

					if(!vsync_power_state)
					{
						printk("##### Error ### vsync_power_state \n"); 			
						ret = -EFAULT;
						goto TCC_VSYNC_OPEN_ERROR;
					}

					#ifdef CONFIG_USE_SUB_MULTI_FRAME
					if(type != VSYNC_MAIN) {
						input_image->m2m_mode = 1;
						input_image->output_toMemory = 1;
						input_image->Lcdc_layer = RDMA_VIDEO_SUB;
					}
					#endif

					print_vsync_input("TCC_LCDC_VIDEO_START_VSYNC", input_image);
					if(tcc_vsync_start(p, input_image, type) < 0) {
						printk("##### Error ### vsync_start \n");
						ret = -EFAULT;
						goto TCC_VSYNC_OPEN_ERROR;
					}
			TCC_VSYNC_OPEN_ERROR:
					kfree((const void*)input_image);
				}
				break;

			case TCC_LCDC_VIDEO_END_VSYNC:
			case TCC_LCDC_VIDEO_END_VSYNC_KERNEL:
				tcc_vsync_end(p, type);
				break ;

			case TCC_LCDC_VIDEO_SET_SIZE_CHANGE:
				#if 0
				printk("### SET_SIZE_CHANGE, firstFrame(%d) deint_mode(%d) \n", p->firstFrameFlag, p->deinterlace_mode);
				spin_lock_irq(&vsync_lock) ;
				tcc_vsync_pop_all_buffer(&p->vsync_buffer, 4);
				spin_unlock_irq(&vsync_lock) ;
				#endif
				break ;

			case TCC_LCDC_VIDEO_SET_FRAMERATE:
				dprintk("%s-%d :: TCC_LCDC_VIDEO_SET_FRAMERATE ========= %d \n", __func__, __LINE__, (int)arg);
				tcc_video_set_framerate(p, (int)arg);
				break ;
		
			case TCC_LCDC_VIDEO_SET_MVC_STATUS:
				{
					int status;

					if(copy_from_user(&status, (int*)arg, sizeof(int))){
						ret = -EFAULT;
						goto Error;
					}

					if(status)
					{
						tcc_vsync_mvc_status = 1;
						printk("$$$ tcc_vsync_mvc_status= %d\n", tcc_vsync_mvc_status);
					}
					else
					{
						tcc_vsync_mvc_status = 0;
						printk("$$$ tcc_vsync_mvc_status= %d\n", tcc_vsync_mvc_status);
					}
				}
				break ;

		
			case TCC_LCDC_VIDEO_PUSH_VSYNC:
			case TCC_LCDC_VIDEO_PUSH_VSYNC_KERNEL:
				{
					struct tcc_lcdc_image_update *input_image = (struct tcc_lcdc_image_update *)kmalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);
					struct tcc_dp_device *dp_device = tca_fb_get_displayType(Output_SelectMode);
					int err_type = 0;

					if(!input_image) {
						printk("%s-%d :: kmalloc fail \n", __func__, __LINE__);
						ret = -EFAULT;
						goto Error;
					}

					if (cmd == TCC_LCDC_VIDEO_PUSH_VSYNC_KERNEL) {
						if (NULL == memcpy((void *)input_image , (const void *)arg, sizeof(struct tcc_lcdc_image_update)))
						{
							printk("fatal error :: copy err");
							ret = 0;
							kfree((const void*)input_image);
							goto Error;
						}
					} else {

						if (copy_from_user((void *)input_image , (const void *)arg, sizeof(struct tcc_lcdc_image_update)))
						{
							printk("fatal error :: copy err");
							ret = 0;
							kfree((const void*)input_image);
							goto Error;
						}
					}

					#ifdef CONFIG_USE_SUB_MULTI_FRAME
					if(type != VSYNC_MAIN) {
						input_image->m2m_mode = 1;
						input_image->output_toMemory = 1;
						input_image->Lcdc_layer = RDMA_VIDEO_SUB;
					}
					#endif

					if(!p->vsync_started){
						printk("fatal error :: vsync is not started");
						ret = 0;
						kfree((const void*)input_image);
						goto Error;
					}

					print_vsync_input("TCC_LCDC_VIDEO_PUSH_VSYNC", input_image);
					vprintk("%s: PUSH_%d ioctl ID(%d) - TS(v[%d ms] / s[%d ms]) \n", input_image->Lcdc_layer == RDMA_VIDEO ? "M" : "S", type,
								input_image->buffer_unique_id, input_image->time_stamp, input_image->sync_time);

					if((err_type = tcc_vsync_push_preprocess(p, dp_device, input_image, type)) < 0)
						goto TCC_VSYNC_PUSH_ERROR;

				#ifdef CONFIG_HDMI_DISPLAY_LASTFRAME
					tcc_video_info_backup(type, input_image);
				#endif

					if(p->outputMode < 0)
					{
						tcc_vsync_push_set_outputMode(p, dp_device, input_image);
					}

					tcc_vsync_push_preprocess_deinterlacing(p, dp_device, input_image);

					// This is for Display underrun issue.
					if(!p->deinterlace_mode && !p->output_toMemory )
					{
						#if 0
						if(p->outputMode == Output_SelectMode && 
							(p->outputMode == TCC_OUTPUT_HDMI || p->outputMode == TCC_OUTPUT_LCD))
							tca_set_onthefly(pdp_data,&input_image);
						#else
							tca_lcdc_set_onthefly(dp_device, input_image);
						#endif
					}

					if(tcc_vsync_push_bypass_frame(p, dp_device, input_image, type) > 0)
						goto PUSH_VIDEO_FORCE;

					if((err_type = tcc_vsync_push_check_error(p, dp_device, input_image)) < 0)
						goto TCC_VSYNC_PUSH_ERROR;

				PUSH_VIDEO_FORCE : 
					tcc_vsync_push_process(p, dp_device, input_image, type);

				TCC_VSYNC_PUSH_ERROR:
					if(err_type < 0){
						printk("%s: PUSH Error(%d)_%d ioctl ID(%d) - TS(v[%d ms] / s[%d ms]) \n", input_image->Lcdc_layer == RDMA_VIDEO ? "M" : "S", err_type, type,
								input_image->buffer_unique_id, input_image->time_stamp, input_image->sync_time);
					}
					#if defined(CONFIG_TCC_VSYNC_DRV_CONTROL_LUT)
					if(err_type == 0) {
						vsync_process_lut(p, input_image);
					}
					#endif
					ret = p->vsync_buffer.writeIdx;
					kfree((const void*)input_image);
				}
				break;
				
			case TCC_LCDC_VIDEO_GET_DISPLAYED:
			case TCC_LCDC_VIDEO_GET_DISPLAYED_KERNEL:
				ret = tcc_video_get_displayed(p, type);
				if( cmd == TCC_LCDC_VIDEO_GET_DISPLAYED_KERNEL)
				{
					if (NULL == memcpy((int *)arg, &ret, sizeof(int))){
						ret = -EFAULT;
					}
				}
				else
				{
					if (copy_to_user((int *)arg, &ret, sizeof(int))){
						ret = -EFAULT;
					}
				}
				break ;
				
			case TCC_LCDC_VIDEO_GET_VALID_COUNT:
				ret = tcc_video_get_readable_count(p);
				if (copy_to_user((int *)arg, &ret, sizeof(int))){
					ret = -EFAULT;
				}
				break ;
				
			case TCC_LCDC_VIDEO_CLEAR_FRAME:
			case TCC_LCDC_VIDEO_CLEAR_FRAME_KERNEL:
				dprintk("%s-%d :: TCC_LCDC_VIDEO_CLEAR_FRAME ========= %d \n", __func__, __LINE__, (int)arg);
				tcc_video_clear_frame(p, (int)arg, 5);
				break ;
		
			case TCC_LCDC_VIDEO_SKIP_FRAME_START:
			case TCC_LCDC_VIDEO_SKIP_FRAME_START_KERNEL:
				tcc_video_skip_frame_start(p, type);
				break ;
		
			case TCC_LCDC_VIDEO_SKIP_FRAME_END:
				tcc_video_skip_frame_end(p, type);
				break ;
				
			case TCC_LCDC_VIDEO_SKIP_ONE_FRAME:
				dprintk("%s-%d :: TCC_LCDC_VIDEO_SKIP_ONE_FRAME ========= %d \n", __func__, __LINE__, (int)arg);
				tcc_video_skip_one_frame(p, (int)arg);
				break ;

			case TCC_LCDC_VIDEO_VSYNC_STATUS:
				{
					unsigned int status = 1;
					struct tcc_dp_device *dp_device = tca_fb_get_displayType(Output_SelectMode);

					if(dp_device == NULL){
						status = 0;
					}
					else if(!dp_device->FbPowerState){
						status = 0;
					}

					if (copy_to_user((unsigned int *)arg, &status, sizeof(unsigned int)))		{
						ret = -EFAULT;
					}
				}
				break;

			case TCC_LCDC_VIDEO_SWAP_VPU_FRAME:
				{
#if defined(CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME)
					WMIXER_INFO_TYPE WmixerInfo;
					struct tcc_lcdc_image_update *TempImage = (struct tcc_lcdc_image_update *)kmalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);

					if(!TempImage) {
						printk("%s-%d :: kmalloc fail \n", __func__, __LINE__);
						ret = -EFAULT;
						goto Error;
					}

					dprintk("%s-%d :: TCC_LCDC_VIDEO_SWAP_VPU_FRAME ========= %d \n", __func__, __LINE__, (int)arg);
					ret = tcc_video_swap_vpu_frame(p, (int)arg, &WmixerInfo, TempImage, type);

					kfree((const void*)TempImage);
#endif
					if(p->player_no_sync_time)
					{
						p->player_seek=1;
						p->skipFrameStatus = 1;
					}
				}
				break;

			case TCC_LCDC_VIDEO_KEEP_LASTFRAME:
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
				//if (!g_is_vsync1_opened) 
				{
					struct stTcc_last_frame lastframe_info;
					
					if (copy_from_user((void*)&lastframe_info, (const void*)arg, sizeof(struct stTcc_last_frame)))
						ret = -EFAULT;
					else
					{
						struct tcc_lcdc_image_update *lastUpdated = (struct tcc_lcdc_image_update *)kmalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);

						if(!lastUpdated) {
							printk("%s-%d :: kmalloc fail \n", __func__, __LINE__);
							ret = -EFAULT;
							goto Error;
						}
						if((ret = tcc_video_last_frame((void*)p, lastframe_info, lastUpdated, type)) > 0)
							p->deinterlace_mode = 0;

						kfree((const void*)lastUpdated);
					}
				}
#endif
				break;

			case TCC_LCDC_VIDEO_GET_LASTFRAME_STATUS:
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
				if(tccvid_lastframe[type].support)
					ret = 0;
				else
#endif
					ret = -EFAULT;
				break;
				
			case TCC_LCDC_VIDEO_CTRL_LAST_FRAME:
				{
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
					dprintk("%s-%d :: TCC_LCDC_VIDEO_CTRL_LAST_FRAME ========= %d \n", __func__, __LINE__, (int)arg);
					//tcc_video_ctrl_last_frame(type, (int)arg);
#endif
				}
				break;

			case TCC_LCDC_VIDEO_OFF_LAST_FRAME:
				{
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
					printk("TCC_LCDC_VIDEO_OFF_LAST_FRAME type: %d\n", type);
					if(type == VSYNC_MAIN)
						tcc_video_clear_last_frame(RDMA_VIDEO,true);
					else
						tcc_video_clear_last_frame(RDMA_VIDEO_SUB,true);
#endif
				}
				break;
		  
			case TCC_LCDC_SET_M2M_LASTFRAME:
				{
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
					tccvid_lastframe[type].enabled = 0;
					Output_SelectMode = TCC_OUTPUT_NONE;

					dprintk("TCC_LCDC_SET_M2M_LASTFRAME \n");
					dprintk(" LastFrame=%d Output_SelectMode=%d\n",tccvid_lastframe[type].enabled, Output_SelectMode);

					if(Output_SelectMode == TCC_OUTPUT_NONE){
						if (LCD_LCDC_NUM)
							tccvid_lastframe[type].pRDMA = VIOC_RDMA_GetAddress(4+RDMA_LASTFRM);
						else
							tccvid_lastframe[type].pRDMA = VIOC_RDMA_GetAddress(RDMA_LASTFRM);

						if(tccvid_lastframe[type].pRDMA != NULL){
							VIOC_RDMA_SetImageDisable(tccvid_lastframe[type].pRDMA);
							dprintk("----> TCC_LCDC_SET_M2M_LASTFRAME lastframe[%d] cleared!! 0x%p \n", type, tccvid_lastframe[type].pRDMA);
						}
					}
#endif
				}
				break;

#ifdef CONFIG_DISPLAY_EXT_FRAME
			case TCC_LCDC_VIDEO_CONFIGURE_EXT_FRAME:
			case TCC_LCDC_VIDEO_CONFIGURE_EXT_FRAME_KERNEL:
				{
					struct tcc_lcdc_image_update *input_image = (struct tcc_lcdc_image_update *)kmalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);

					if(!input_image) {
						printk("%s-%d :: kmalloc fail \n", __func__, __LINE__);
						ret = -EFAULT;
						goto Error;
					}
					if(cmd == TCC_LCDC_VIDEO_CONFIGURE_EXT_FRAME_KERNEL)
					{
						if (NULL == memcpy((void*)input_image, (const void*)arg, sizeof(struct tcc_lcdc_image_update)))
							ret = -EFAULT;
						else
						{
							memcpy(&last_backup, input_image, sizeof(struct tcc_lcdc_image_update));
						}
					}
					else
					{
						if (copy_from_user((void*)input_image, (const void*)arg, sizeof(struct tcc_lcdc_image_update)))
							ret = -EFAULT;
						else
						{
							memcpy(&last_backup, input_image, sizeof(struct tcc_lcdc_image_update));
						}
					}
					kfree((const void*)input_image);
				}
				break;

			// change the region displaying!!
			case TCC_LCDC_VIDEO_CHANGE_REGION_OF_EXT_FRAME:
				{
					struct tcc_ext_dispregion disp_region;

					if (copy_from_user((void*)&disp_region, (const void*)arg, sizeof(struct tcc_ext_dispregion)))
						ret = -EFAULT;
					else
					{
						last_backup.offset_x 	 = disp_region.start_x;
						last_backup.offset_y 	 = disp_region.start_y;
						last_backup.Image_width  = disp_region.width;
						last_backup.Image_height = disp_region.height;
						if(bExt_Frame)
							tcc_display_ext_frame((void*)&last_backup, last_backup.deinterlace_mode, Output_SelectMode);
					}
				}
				break;

			// refresh current frame with LUT...
			case TCC_LCDC_VIDEO_REFRESH_EXT_FRAME:
				{
					if(bExt_Frame)
						tcc_display_ext_frame((void*)&last_backup, last_backup.deinterlace_mode, Output_SelectMode);
				}
				break;

			// control on/off!!
			case TCC_LCDC_VIDEO_CTRL_EXT_FRAME:
			case TCC_LCDC_VIDEO_CTRL_EXT_FRAME_KERNEL:
				{
					unsigned int enable;

					if(cmd == TCC_LCDC_VIDEO_CTRL_EXT_FRAME_KERNEL)
					{
						if (NULL == memcpy((void*)&enable, (const void*)arg, sizeof(unsigned int)))
							ret = -EFAULT;
						else
						{
							last_backup.enable = enable;
							tcc_display_ext_frame((void*)&last_backup, last_backup.deinterlace_mode, Output_SelectMode);
						}
					}
					else
					{
						if (copy_from_user((void*)&enable, (const void*)arg, sizeof(unsigned int)))
							ret = -EFAULT;
						else
						{
							last_backup.enable = enable;
							tcc_display_ext_frame((void*)&last_backup, last_backup.deinterlace_mode, Output_SelectMode);
						}
					}
				}
				break;
#endif
			case TCC_LCDC_VIDEO_WAIT_TOO_LONG:
				dprintk("%s-%d :: TCC_LCDC_VIDEO_WAIT_TOO_LONG ========= \n", __func__, __LINE__);
				if(p->player_no_sync_time)
				{
					p->wait_too_long_indicate=1;
				}
				break;

			// restrict access of ext_frame
			case TCC_LCDC_VIDEO_RESTRICT_EXT_FRAME:
			case TCC_LCDC_VIDEO_RESTRICT_EXT_FRAME_KERNEL:
			#ifdef CONFIG_DISPLAY_EXT_FRAME
				if(cmd == TCC_LCDC_VIDEO_RESTRICT_EXT_FRAME_KERNEL) {
					if(NULL == memcpy((void*)&restricted_ExtFrame, (const void*)arg, sizeof(unsigned int)))
						goto Error;
				} else {
					if(copy_from_user((void*)&restricted_ExtFrame, (const void*)arg, sizeof(unsigned int)))
						goto Error;
				}
			#endif
				break;

			default:
				printk("vsync: unrecognized ioctl (0x%x)\n", cmd); 
				ret = -EINVAL;
				break;
		}
	}

Error:
	mutex_unlock(&vsync_io_mutex);
	
	return ret;
}

static void _tca_vsync_intr_onoff(char on, char vsync_disp_num)
{
	char lcdc_num = EX_OUT_LCDC;
	
#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
	if(vsync_started_device == LCD_START_VSYNC)
		lcdc_num = LCD_LCDC_NUM;
#endif
	
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH())
	{
		if(on)
			vioc_intr_enable(vsync_vioc0_disp.irq_num, VIOC_INTR_V_DV, DV_VIDEO_INT);
		else
		{
			volatile void __iomem *pDV_Cfg = VIOC_DV_VEDR_GetAddress(VDV_CFG);
		#ifdef USE_DOLBY_VISION_SUB_INTERRUPT
			vioc_intr_disable(vsync_vioc0_disp.irq_num, VIOC_INTR_V_DV, DV_VIDEO_INT|DV_VIDEO_INT_SUB);
		#else
			vioc_intr_disable(vsync_vioc0_disp.irq_num, VIOC_INTR_V_DV, DV_VIDEO_INT);
		#endif
			VIOC_V_DV_ClearInterrupt(pDV_Cfg, DV_VIDEO_INT|DV_VIDEO_INT_SUB);
		}
	}
	else
#endif
	{
		if(vsync_disp_num == 0)
		{
			if(on)
				vioc_intr_enable(vsync_vioc0_disp.irq_num, lcdc_num, vsync_vioc0_disp.vioc_intr->bits);
			else
			{
				vioc_intr_disable(vsync_vioc0_disp.irq_num, lcdc_num, vsync_vioc0_disp.vioc_intr->bits);
				vioc_intr_clear(lcdc_num, vsync_vioc0_disp.vioc_intr->bits);
			}
		}
		else
		{
			if(on)
				vioc_intr_enable(vsync_vioc1_disp.irq_num, lcdc_num, vsync_vioc1_disp.vioc_intr->bits);
			else
			{
				vioc_intr_disable(vsync_vioc1_disp.irq_num, lcdc_num, vsync_vioc1_disp.vioc_intr->bits);
				vioc_intr_clear(lcdc_num, vsync_vioc1_disp.vioc_intr->bits);
			}
		}
	}
}

void tca_vsync_video_display_enable(void)
{
	int ret=-1;
	char lcdc_num = EX_OUT_LCDC;

	#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
	if(vsync_started_device == LCD_START_VSYNC)
		lcdc_num = LCD_LCDC_NUM;
	#endif

	
	if(lcdc_interrupt_onoff == 0)
	{
		if(lcdc_num){
			
		#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
			if(vsync_started_device == LCD_START_VSYNC)
			{
				_tca_vsync_intr_onoff(0, lcdc_num);
				if(!vsync_vioc1_disp.irq_reged) {
					ret = request_irq(vsync_vioc1_disp.irq_num, tcc_vsync_handler_for_video,IRQF_SHARED,"vioc_vsync1", &vsync_vioc1_disp);
					if (ret)
						printk("\n\n @@@%s: vioc1 ret=%d @@@\n\n\n", __func__, ret);
					else
						vsync_vioc1_disp.irq_reged = 1;
				}
				_tca_vsync_intr_onoff(1, lcdc_num);
			}
			else
		#endif
			{
				// TODO:
				BUG();
				
			}
		}
		else{
		#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
			if(vsync_started_device == LCD_START_VSYNC)
			{
				_tca_vsync_intr_onoff(0, lcdc_num);
				if(!vsync_vioc0_disp.irq_reged) {
					ret= request_irq(vsync_vioc0_disp.irq_num, tcc_vsync_handler_for_video,IRQF_SHARED, "vioc_vsync0", &vsync_vioc0_disp);
					if (ret)
						printk("\n\n @@@%s: vioc0 ret=%d @@@\n\n\n", __func__, ret);
					else
						vsync_vioc0_disp.irq_reged = 1;
				}
				_tca_vsync_intr_onoff(1, lcdc_num);
			}
			else
		#endif
			{
				_tca_vsync_intr_onoff(0, lcdc_num);
				if(!vsync_vioc0_disp.irq_reged) {
					ret= request_irq(vsync_vioc0_disp.irq_num, tcc_vsync_handler_for_video,IRQF_SHARED, "vioc_vsync0", &vsync_vioc0_disp);
					if (ret)
						printk("\n\n @@@%s: vioc0 ret=%d @@@\n\n\n", __func__, ret);
					else
						vsync_vioc0_disp.irq_reged = 1;
				}
				_tca_vsync_intr_onoff(1, lcdc_num);
			}
		}
		
		dprintk("%s: onoff(%d) request_irq(%d) ret(%d) \n",__func__,lcdc_interrupt_onoff,lcdc_num,ret);
		
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
		INIT_WORK(&dolby_out_change_work_q, tcc_video_change_dolby_out_wq_isr);
#endif
		lcdc_interrupt_onoff = 1;
		video_display_disable_check = 0;
	}
	else
	{
		printk("tccfb info: lcdc interrupt have been enabled already\n");
	}

#ifdef USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT
	tcc_vsync_unplugged_int_stop();
#endif

}

void tca_vsync_video_display_disable(void)
{
	char lcdc_num = EX_OUT_LCDC;

#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
	if(vsync_started_device == LCD_START_VSYNC)
		lcdc_num = LCD_LCDC_NUM;

	printk("%s: onoff(%d) started_device(%d) \n",__func__,lcdc_interrupt_onoff,vsync_started_device);
#else
	printk("%s: onoff(%d) lcdc_num(%d) \n",__func__,lcdc_interrupt_onoff, lcdc_num);
#endif
	
	if(lcdc_interrupt_onoff == 1)
	{
		if(lcdc_num){
			#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
			if(vsync_started_device == LCD_START_VSYNC)
			{
				_tca_vsync_intr_onoff(0, lcdc_num);
				//free_irq(vsync_vioc1_disp.irq_num, &vsync_vioc1_disp);
			}
			else
			#endif
			{
				// TODO:
				BUG();
			}
		}
		else {
			#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
			if(vsync_started_device == LCD_START_VSYNC)
			{
				_tca_vsync_intr_onoff(0, lcdc_num);
				//free_irq(vsync_vioc0_disp.irq_num, &vsync_vioc0_disp);
			}
			else
			#endif
			{
				_tca_vsync_intr_onoff(0, lcdc_num);
				//free_irq(vsync_vioc0_disp.irq_num, &vsync_vioc0_disp);
			}
		}
		lcdc_interrupt_onoff = 0;
	}
	else
	{
		printk("video vsync: interrupt have been disabled already\n");
	}

#ifdef USE_VSYNC_TIMER_INT_FOR_UNPLUGED_OUTPUT
	tcc_vsync_unplugged_int_start(tcc_vsync_unplugged_output_for_video);
#endif
}

int tcc_video_get_displayed(tcc_video_disp *p, VSYNC_CH_TYPE type)
{
	int buffer_id = -1;
/*
	-2: skip status
	-1 : vsync is not started.
	-100: diplaying 0'th buffer-id, should wait proper buffer-id.
*/

	if(!p->vsync_started) {
		vprintk("vsync_started is not started!! return\n");
		return -1;
	}

	if(p->skipFrameStatus) {
		vprintk("frame skip mode return\n");
		return -2;
	}

	if(p->wait_too_long_indicate == 1) {
		vprintk("wait_too_long_indicate return\n");
		return -2;
	}

	//printk("# last disp num(%d) \n", p->vsync_buffer.last_cleared_buff_id ) ;
	if(p->mvcMode == 1)
		buffer_id = p->vsync_buffer.last_cleared_buff_id >= 2 ? (p->vsync_buffer.last_cleared_buff_id-2): -100; 
	else
		buffer_id = p->vsync_buffer.last_cleared_buff_id >= 1 ? (p->vsync_buffer.last_cleared_buff_id-1): -100; 

	vprintk("%s_[%d] : cleared buffer (%d -> %d) \n", __func__, type, p->vsync_buffer.last_cleared_buff_id, buffer_id);

	return buffer_id;
}

void tcc_vsync_set_firstFrameFlag(tcc_video_disp *p, int firstFrameFlag)
{
	p->firstFrameFlag = firstFrameFlag;
}

void tcc_vsync_set_firstFrameFlag_all(int firstFrameFlag)
{
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	unsigned int type = VSYNC_MAIN;
	for(type = VSYNC_MAIN; type < VSYNC_MAX; type++)
		tcc_vsync_set_firstFrameFlag(&tccvid_vsync[type], firstFrameFlag);
#else
	tcc_vsync_set_firstFrameFlag(&tccvid_vsync[0], firstFrameFlag);
	tcc_vsync_set_firstFrameFlag(&tccvid_vsync[1], firstFrameFlag);
#endif
}

void tcc_video_clear_frame(tcc_video_disp *p, int idx, int who_call)
{
	int syncBufferIdx;
	
	syncBufferIdx = idx;

	printk("video frame clear %d called by %d\n", syncBufferIdx, who_call) ;

	spin_lock_irq(&vsync_lock) ;
	tcc_vsync_pop_all_buffer(&p->vsync_buffer, 5);
	spin_unlock_irq(&vsync_lock) ;
	//printk("valid video fram count %d\n", p->vsync_buffer.valid_buff_count) ;

	p->vsync_buffer.available_buffer_id_on_vpu = syncBufferIdx;//+1;
}

void tcc_video_set_framerate(tcc_video_disp *p, int fps)
{
	int media_time_gap;
	dprintk("### tcc_video_set_framerate %d\n", fps);

	if(!(fps < 1000))
	{
		fps += 500;
		fps /= 1000;
	}

	if(fps == 29)
		fps = 30;
	else if(fps == 23)
		fps = 24;

	p->video_frame_rate = fps;

	if(p->video_frame_rate > 0)
		media_time_gap = 1000/fps;
	else
		media_time_gap = 32;
	
	p->updateGapTime = p->nTimeGapToNextField = media_time_gap;

	dprintk("### video_frame_rate(%d), media_time_gap(%d) updateGapTime(%d)\n",p->video_frame_rate, media_time_gap, p->updateGapTime);
}

int tcc_video_check_framerate(tcc_video_disp *p, int fps)
{
	int media_time_gap;

	if(p->player_no_sync_time && fps ==0)
		return 0;
	
	if(!(fps < 1000))
	{
		fps += 500;
		fps /= 1000;
	}

	if(fps == 29)
		fps = 30;
	else if(fps == 23)
		fps = 24;

	if(p->video_frame_rate != fps )
	{

		p->video_frame_rate = fps;

		if(p->video_frame_rate > 0)
			media_time_gap = 1000/fps;
		else
			media_time_gap = 32;

		p->updateGapTime = p->nTimeGapToNextField = media_time_gap;

		printk("### video_frame_rate(%d), media_time_gap(%d) updateGapTime(%d)\n",p->video_frame_rate, media_time_gap, p->updateGapTime);

		return 1;
	}

	return 0;
}

void tcc_video_skip_frame_start(tcc_video_disp *p, VSYNC_CH_TYPE type)
{
	vprintk("[%d] video fram skip start\n", type);
	printk("TCC_LCDC_VIDEO_SKIP_FRAME_START\n") ;
	spin_lock_irq(&vsync_lock) ;
	tcc_vsync_pop_all_buffer(&p->vsync_buffer, 6);
	spin_unlock_irq(&vsync_lock);
	p->skipFrameStatus = 1;
}

void tcc_video_skip_frame_end(tcc_video_disp *p, VSYNC_CH_TYPE type)
{
	if(p->skipFrameStatus)
	{
		vprintk("[%d] video fram skip end\n", type);
		spin_lock_irq(&vsync_lock) ;
		tcc_vsync_pop_all_buffer(&p->vsync_buffer, 7);
		spin_unlock_irq(&vsync_lock);
		printk("[%d] TCC_LCDC_VIDEO_SKIP_FRAME_END\n", type);

		p->skipFrameStatus = 0;
		tcc_vsync_reset_syncTime(p, 0, type);
	}

}

void tcc_video_skip_one_frame(tcc_video_disp *p, int frame_id)
{
	spin_lock_irq(&vsync_lockDisp) ;
	//printk("TCC_LCDC_VIDEO_SKIP_ONE_FRAME frame_id %d\n",frame_id) ;
	p->vsync_buffer.last_cleared_buff_id = frame_id;
	spin_unlock_irq(&vsync_lockDisp) ;
}

int tcc_video_get_readable_count(tcc_video_disp *p)
{
	return atomic_read( & p->vsync_buffer.readable_buff_count); 
}

int tcc_video_get_valid_count(tcc_video_disp *p)
{
	return atomic_read( & p->vsync_buffer.valid_buff_count);
}


void tcc_vsync_set_output_mode(tcc_video_disp *p, int mode)
{
	spin_lock_irq(&vsync_lockDisp) ;
	p->outputMode = mode;
	spin_unlock_irq(&vsync_lockDisp) ;
}

void tcc_vsync_set_output_mode_all(int mode)
{
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	unsigned int type = VSYNC_MAIN;
	for(type = VSYNC_MAIN; type < VSYNC_MAX; type++)
		tcc_vsync_set_output_mode(&tccvid_vsync[type], mode);
#else
	tcc_vsync_set_output_mode(&tccvid_vsync[0], mode);
	tcc_vsync_set_output_mode(&tccvid_vsync[1], mode);
#endif
}

void tcc_vsync_hdmi_start(struct tcc_dp_device *pdp_data,int* lcd_video_started)
{

#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
	printk("%s : SelectMode(%d) started_device(%d) \n",__func__,Output_SelectMode,vsync_started_device);

	Output_SelectMode = TCC_OUTPUT_HDMI;
	if(tccvid_vsync[0].isVsyncRunning)
	{
		printk("VSYNC CH0 HDMI TIMING, by the way LCD and vsync enable \n");
		// interlace video on LCD -> HDMI plug in -> garbage screen debugging
		if(tccvid_vsync[0].deinterlace_mode && !tccvid_vsync[0].output_toMemory &&!tccvid_vsync[0].interlace_bypass_lcdc)
		{
			printk("and Interlaced \n");
		}
		else
		{
			struct tcc_lcdc_image_update ImageInfo;
			memset(&ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));
			ImageInfo.enable = 0;
			ImageInfo.Lcdc_layer = RDMA_VIDEO;

			tca_scale_display_update(pdp_data, &ImageInfo);
		}
	}

	if(tccvid_vsync[1].isVsyncRunning)
	{
		printk("VSYNC CH1 HDMI TIMING, by the way LCD and vsync enable \n");
		// interlace video on LCD -> HDMI plug in -> garbage screen debugging
		if(tccvid_vsync[1].deinterlace_mode && !tccvid_vsync[1].output_toMemory &&!tccvid_vsync[1].interlace_bypass_lcdc)
		{
			printk("and Interlaced \n");
		}
		else
		{
			struct tcc_lcdc_image_update ImageInfo;
			memset(&ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));
			ImageInfo.enable = 0;
			ImageInfo.Lcdc_layer = RDMA_VIDEO_SUB;

			tca_scale_display_update(pdp_data, &ImageInfo);
		}
	}
				
#else
	printk(" %s: SelectMode(%d) lcd_started(%d)\n",__func__,Output_SelectMode,*lcd_video_started);

	if(*lcd_video_started)
	{
		struct tcc_lcdc_image_update ImageInfo;
		
		printk(" TCC_LCDC_HDMI_TIMING => TCC_LCDC_DISPLAY_END \n");

		memset(&ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));
		ImageInfo.enable = 0;
		ImageInfo.Lcdc_layer = RDMA_VIDEO;

		tca_scale_display_update(pdp_data, (struct tcc_lcdc_image_update *)&ImageInfo);

		ImageInfo.enable = 0;
		ImageInfo.Lcdc_layer = RDMA_VIDEO_SUB;

		tca_scale_display_update(pdp_data, (struct tcc_lcdc_image_update *)&ImageInfo);

		*lcd_video_started = 0;
	}

	Output_SelectMode = TCC_OUTPUT_HDMI;

	if(video_display_disable_check) {
		tca_vsync_video_display_enable();
		video_display_disable_check = 0;
	}

	#if defined(CONFIG_TCC_OUTPUT_AUTO_HDMI_CVBS) || defined(CONFIG_TCC_OUTPUT_ATTACH)
	tcc_vsync_set_firstFrameFlag_all(1);
	#endif

	if(tcc_vsync_mvc_status && tccvid_vsync[0].mvcMode==1) {
		VIOC_WMIX_GetOverlayPriority(pdp_data->wmixer_info.virt_addr, &tcc_vsync_mvc_overlay_priority);	//save
		VIOC_WMIX_SetOverlayPriority(pdp_data->wmixer_info.virt_addr, 16);									//Image1 - Image0 - Image2 - Image3
		VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);
	}

	spin_lock_irq(&vsync_lock);
	tcc_vsync_set_output_mode_all(-1);
	spin_unlock_irq(&vsync_lock);
#endif

}

void tcc_vsync_hdmi_end(struct tcc_dp_device *pdp_data)
{
#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
	printk(" %s: out %d start %d \n",__func__,Output_SelectMode,vsync_started_device);
	if(Output_SelectMode == TCC_OUTPUT_HDMI) 
	{
		struct tcc_lcdc_image_update lcdc_image;

		//this check is for the case LCD video - HDMI cable plug out - plug in after short time(before HDMI video) - LCD video
		if(vsync_started_device == LCD_START_VSYNC)
			Output_SelectMode = TCC_OUTPUT_LCD;
		else
			Output_SelectMode = TCC_OUTPUT_NONE;
		
		memset(&lcdc_image, 0x00, sizeof(struct tcc_lcdc_image_update));
		lcdc_image.enable = 0;
		lcdc_image.Lcdc_layer = RDMA_VIDEO;

		tca_scale_display_update(pdp_data, &lcdc_image);
	}

	tcc_vsync_set_firstFrameFlag_all(1);
	
#else
	printk(" %s: SelectMode %d \n",__func__,Output_SelectMode);

	if(Output_SelectMode == TCC_OUTPUT_HDMI) 
	{				
		struct tcc_lcdc_image_update lcdc_image;

		if(!video_display_disable_check)
		{
			tca_vsync_video_display_disable();
			video_display_disable_check = 1;
		}

		Output_SelectMode = TCC_OUTPUT_NONE;
		memset(&lcdc_image, 0x00, sizeof(struct tcc_lcdc_image_update));
		lcdc_image.enable = 0;
		lcdc_image.Lcdc_layer = RDMA_VIDEO;

		tca_scale_display_update(pdp_data, &lcdc_image);

		lcdc_image.enable = 0;
		lcdc_image.Lcdc_layer = RDMA_VIDEO_SUB;

		tca_scale_display_update(pdp_data, &lcdc_image);

	#if !defined(CONFIG_TCC_OUTPUT_AUTO_HDMI_CVBS) && !defined(CONFIG_TCC_OUTPUT_ATTACH)
		tcc_vsync_set_firstFrameFlag_all(1);
	#endif
	}

	#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
	if (tccvid_vsync[0].vsync_started) {
		if(tccvid_vsync[0].output_toMemory && tccvid_vsync[0].m2m_mode) {
			TCC_VIQE_DI_DeInit60Hz_M2M(RDMA_VIDEO);
		} else if(tccvid_vsync[0].deinterlace_mode &&!tccvid_vsync[0].interlace_bypass_lcdc) {
			if(!tccvid_vsync[0].output_toMemory)
				TCC_VIQE_DI_DeInit60Hz();
		}
	}

	if (tccvid_vsync[1].vsync_started) {
		if(tccvid_vsync[1].output_toMemory && tccvid_vsync[1].m2m_mode) {
			TCC_VIQE_DI_Sub_DeInit60Hz_M2M(RDMA_VIDEO_SUB);
		} else if(tccvid_vsync[1].deinterlace_mode &&!tccvid_vsync[1].interlace_bypass_lcdc) {
			if(!tccvid_vsync[1].output_toMemory)
				TCC_VIQE_DI_DeInit60Hz();
		}
	}
	#endif
	//tcc_vsync_pop_all_buffer(&tccvid_vsync[0].vsync_buffer, 8);
	//tcc_vsync_pop_all_buffer(&tccvid_vsync[1].vsync_buffer, 9);
#endif

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) 
	tcc_video_clear_last_frame(RDMA_VIDEO, false);
	tcc_video_clear_last_frame(RDMA_VIDEO_SUB, false);
#endif
}

void tcc_vsync_reset_all(void)
{
	int i;
	
	for (i=0; i<VSYNC_MAX; i++) {
		memset( &tccvid_vsync[i], 0, sizeof( tcc_video_disp) );
		tccvid_vsync[i].firstFrameFlag = 1;
	}
}

void tcc_vsync_if_pop_all_buffer(tcc_video_disp *p)
{
	spin_lock_irq(&vsync_lock);
	tcc_vsync_pop_all_buffer(&p->vsync_buffer, 10);
	spin_unlock_irq(&vsync_lock);
}

void tcc_vsync_set_deinterlace_mode(tcc_video_disp *p, int mode)
{
	p->deinterlace_mode = mode;
}
EXPORT_SYMBOL(tcc_vsync_set_deinterlace_mode);

int tcc_vsync_isVsyncRunning(VSYNC_CH_TYPE type)
{
	return tccvid_vsync[type].isVsyncRunning;
}
EXPORT_SYMBOL(tcc_vsync_isVsyncRunning);

int tcc_vsync_get_isVsyncRunning(VSYNC_CH_TYPE type)
{
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	if(tccvid_vsync[type].isVsyncRunning)
		return tccvid_vsync[type].isVsyncRunning;
#else
	if(type == VSYNC_MAIN && !tccvid_vsync[VSYNC_SUB0].isVsyncRunning)
		return tccvid_vsync[type].isVsyncRunning;
	else if(type != VSYNC_MAIN && !tccvid_vsync[VSYNC_MAIN].isVsyncRunning)
		return tccvid_vsync[type].isVsyncRunning;
#endif
	return 0;
}

int tcc_vsync_get_output_toMemory(tcc_video_disp *p)
{
	return p->output_toMemory;
}

int tcc_vsync_get_interlace_bypass_lcdc(tcc_video_disp *p)
{
	return p->interlace_bypass_lcdc;
}

int tcc_vsync_get_deinterlace_mode(tcc_video_disp *p)
{
	return p->deinterlace_mode;
}

int is_deinterlace_enabled(VSYNC_CH_TYPE type)
{
	if (tcc_vsync_get_deinterlace_mode(&tccvid_vsync[type])
	&& !tcc_vsync_get_output_toMemory(&tccvid_vsync[type])
	&& !tcc_vsync_get_interlace_bypass_lcdc(&tccvid_vsync[type]))
		return 1;

	return 0;
}


#ifdef CONFIG_USE_SUB_MULTI_FRAME
static long tcc_vsync3_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, arg, VSYNC_SUB2);
}

#ifdef CONFIG_COMPAT
static long tcc_vsync3_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, (unsigned long)compat_ptr(arg), VSYNC_SUB2);
}
#endif

static int tcc_vsync3_release(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d)\n", __func__, g_is_vsync3_opened);
	if (--g_is_vsync3_opened)
		return 0;

	if(g_is_vsync3_opened == 0) {
		tcc_video_disp *p = &tccvid_vsync[VSYNC_SUB2];
		if(p->isVsyncRunning)
			tcc_vsync_end(p, VSYNC_SUB2);

		if(!g_is_vsync0_opened) {
			tca_vsync_video_display_disable();

			if(vsync_vioc0_disp.irq_reged) {
				free_irq(vsync_vioc0_disp.irq_num, &vsync_vioc0_disp);
				vsync_vioc0_disp.irq_reged = 0;
			}
			if(vsync_vioc1_disp.irq_reged) {
				free_irq(vsync_vioc1_disp.irq_num, &vsync_vioc1_disp);
				vsync_vioc1_disp.irq_reged = 0;
			}

#ifdef USE_VSYNC_TIMER
			if (vsync_timer)
				tcc_timer_disable(vsync_timer);
#endif
		}
	}

	return 0;
}

static int tcc_vsync3_open(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d)\n", __func__, g_is_vsync3_opened);
	if (g_is_vsync3_opened++)
		return 0;

#ifdef USE_VSYNC_TIMER
	if (vsync_timer && !g_is_vsync0_opened) {
		tcc_timer_enable(vsync_timer);
		msleep(0);
	}
#endif
	return 0;
}

static long tcc_vsync2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, arg, VSYNC_SUB1);
}

#ifdef CONFIG_COMPAT
static long tcc_vsync2_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, (unsigned long)compat_ptr(arg), VSYNC_SUB1);
}
#endif

static int tcc_vsync2_release(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d)\n", __func__, g_is_vsync2_opened);
	if (--g_is_vsync2_opened)
		return 0;

	if(g_is_vsync2_opened == 0) {
		tcc_video_disp *p = &tccvid_vsync[VSYNC_SUB1];
		if(p->isVsyncRunning)
			tcc_vsync_end(p, VSYNC_SUB1);

		if(!g_is_vsync0_opened) {
			tca_vsync_video_display_disable();

			if(vsync_vioc0_disp.irq_reged) {
				free_irq(vsync_vioc0_disp.irq_num, &vsync_vioc0_disp);
				vsync_vioc0_disp.irq_reged = 0;
			}

			if(vsync_vioc1_disp.irq_reged) {
				free_irq(vsync_vioc1_disp.irq_num, &vsync_vioc1_disp);
				vsync_vioc1_disp.irq_reged = 0;
			}
#ifdef USE_VSYNC_TIMER
			if (vsync_timer)
				tcc_timer_disable(vsync_timer);
#endif
		}
	}

	return 0;
}

static int tcc_vsync2_open(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d)\n", __func__, g_is_vsync2_opened);
	if (g_is_vsync2_opened++)
		return 0;

#ifdef USE_VSYNC_TIMER
	if (vsync_timer && !g_is_vsync0_opened) {
		tcc_timer_enable(vsync_timer);
		msleep(0);
	}
#endif
	return 0;
}
#endif

static long tcc_vsync1_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, arg, VSYNC_SUB0);
}

#ifdef CONFIG_COMPAT
static long tcc_vsync1_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, (unsigned long)compat_ptr(arg), VSYNC_SUB0);
}
#endif

static int tcc_vsync1_release(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d)\n", __func__, g_is_vsync1_opened);

	if (--g_is_vsync1_opened)
		return 0;

	if(g_is_vsync1_opened == 0)
	{
		tcc_video_disp *p = &tccvid_vsync[VSYNC_SUB0];
		if(p->isVsyncRunning)
			tcc_vsync_end(p, VSYNC_SUB0);

		if(!g_is_vsync0_opened)
		{
			tca_vsync_video_display_disable();

			if(vsync_vioc0_disp.irq_reged) {
				free_irq(vsync_vioc0_disp.irq_num, &vsync_vioc0_disp);
				vsync_vioc0_disp.irq_reged = 0;
			}

			if(vsync_vioc1_disp.irq_reged) {
				free_irq(vsync_vioc1_disp.irq_num, &vsync_vioc1_disp);
				vsync_vioc1_disp.irq_reged = 0;
			}

	#ifdef USE_VSYNC_TIMER
			if (vsync_timer)
				tcc_timer_disable(vsync_timer);
	#endif
		}

		if(tccvid_lastframe[VSYNC_SUB0].pmapBuff.base){
			pmap_release_info("fb_wmixer1");
	        tccvid_lastframe[VSYNC_SUB0].pmapBuff.base = 0x00;
	    }
	}

	return 0;
}

static int tcc_vsync1_open(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d)\n", __func__, g_is_vsync1_opened);

	if (g_is_vsync1_opened++)
		return 0;

	#ifdef USE_VSYNC_TIMER
	if (vsync_timer && !g_is_vsync0_opened) {
		tcc_timer_enable(vsync_timer);
		msleep(0);
	}
	#endif

	if(0 > pmap_get_info("fb_wmixer1", &tccvid_lastframe[VSYNC_SUB0].pmapBuff)){
		printk("%s-%d : fb_wmixer1 allocation is failed.\n", __func__, __LINE__);
		//return -ENOMEM;
	}
	dprintk("%s :: Sub LastFrame(%d/%d) - wmixer base:0x%08x/0x%x \n", __func__,
				tccvid_lastframe[VSYNC_SUB0].support, tccvid_lastframe[VSYNC_SUB0].enabled,
				tccvid_lastframe[VSYNC_SUB0].pmapBuff.base, tccvid_lastframe[VSYNC_SUB0].pmapBuff.size );

	return 0;
}

static long tcc_vsync0_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, arg, VSYNC_MAIN);
}

#ifdef CONFIG_COMPAT
static long tcc_vsync0_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tcc_vsync_do_ioctl(cmd, (unsigned long)compat_ptr(arg), VSYNC_MAIN);
}
#endif

static int tcc_vsync0_release(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d) In\n", __func__, g_is_vsync0_opened);

	if (--g_is_vsync0_opened)
		return 0;

	if(g_is_vsync0_opened == 0)
	{
		tcc_video_disp *p = &tccvid_vsync[VSYNC_MAIN];
		if(p->isVsyncRunning)
			tcc_vsync_end(p, VSYNC_MAIN);

		if(!g_is_vsync1_opened)
		{
			tca_vsync_video_display_disable();

			if(vsync_vioc0_disp.irq_reged) {
				free_irq(vsync_vioc0_disp.irq_num, &vsync_vioc0_disp);
				vsync_vioc0_disp.irq_reged = 0;
			}

			if(vsync_vioc1_disp.irq_reged) {
				free_irq(vsync_vioc1_disp.irq_num, &vsync_vioc1_disp);
				vsync_vioc1_disp.irq_reged = 0;
			}

	#ifdef USE_VSYNC_TIMER
			if (vsync_timer)
				tcc_timer_disable(vsync_timer);
	#endif
		}

		if(tccvid_lastframe[VSYNC_MAIN].pmapBuff.base){
			pmap_release_info("fb_wmixer");
	        tccvid_lastframe[VSYNC_MAIN].pmapBuff.base = 0x00;
	    }
	}
	dprintk("%s(%d) Out \n", __func__, g_is_vsync0_opened);

	return 0;
}

static int tcc_vsync0_open(struct inode *inode, struct file *filp)
{
	dprintk("%s(%d)\n", __func__, g_is_vsync0_opened);

	if (g_is_vsync0_opened++)
		return 0;

	#ifdef USE_VSYNC_TIMER
	if (vsync_timer && !g_is_vsync1_opened) {
		tcc_timer_enable(vsync_timer);
		msleep(0);
	}
	#endif

	if(0 > pmap_get_info("fb_wmixer", &tccvid_lastframe[VSYNC_MAIN].pmapBuff)){
		printk("%s-%d : fb_wmixer allocation is failed.\n", __func__, __LINE__);
		//return -ENOMEM;
	}
	dprintk("%s :: Main LastFrame(%d/%d) - wmixer base:0x%08x/0x%x \n", __func__,
				tccvid_lastframe[VSYNC_MAIN].support, tccvid_lastframe[VSYNC_MAIN].enabled,
				tccvid_lastframe[VSYNC_MAIN].pmapBuff.base, tccvid_lastframe[VSYNC_MAIN].pmapBuff.size );

	return 0;
}


static int tcc_vsync_parse_dt(struct device_node *np)
{
	struct device_node *vioc0_node,*vioc1_node;
	int index;
	
	if (np) {
		memset(&vsync_vioc1_disp,0x00,sizeof(struct tcc_vsync_display_info_t));
		vioc1_node = of_parse_phandle(np,"display-vioc1", 0);
		if (vioc1_node) {
			of_property_read_u32_index(np, "display-vioc1", 1, &index);
			vsync_vioc1_disp.virt_addr = VIOC_DISP_GetAddress(index);
			vsync_vioc1_disp.lcdc_num = get_vioc_index(index);
			vsync_vioc1_disp.irq_num = irq_of_parse_and_map(vioc1_node, 0);

			printk("%s: vioc1 addr(%p) lcdc_num(%d) irq_num(%d)\n", __func__,
				vsync_vioc1_disp.virt_addr,
				vsync_vioc1_disp.lcdc_num,
				vsync_vioc1_disp.irq_num);
		}
		
		memset(&vsync_vioc0_disp,0x00,sizeof(struct tcc_vsync_display_info_t));
		vioc0_node = of_parse_phandle(np,"display-vioc0", 0);
		if (!vioc0_node) {
			goto err;
		}
		of_property_read_u32_index(np, "display-vioc0", 1, &index);
		vsync_vioc0_disp.virt_addr = VIOC_DISP_GetAddress(index);
		vsync_vioc0_disp.lcdc_num = get_vioc_index(index);
		vsync_vioc0_disp.irq_num = irq_of_parse_and_map(vioc0_node, 0);

		printk("%s: vioc0 addr(%p) lcdc_num(%d) irq_num(%d)\n", __func__,
			vsync_vioc0_disp.virt_addr,
			vsync_vioc0_disp.lcdc_num,
			vsync_vioc0_disp.irq_num);
	}
	else{
		goto err;
	}

	return 0;
err:
	pr_err("%s: could not find telechips,ddc node\n", __func__);
	return -ENODEV;
}

#ifdef CONFIG_PM
static int tcc_vsync_suspend(struct device *dev)
{
	printk("%s \n", __FUNCTION__);
	
	return 0;
}

static int tcc_vsync_resume(struct device *dev)
{
	printk("%s \n", __FUNCTION__);

	return 0;
}
#else
#define tcc_vsync_suspend NULL
#define tcc_vsync_resume NULL
#endif

#ifdef CONFIG_PM
static int tcc_vsync_runtime_suspend(struct device *dev)
{
	printk("%s:  \n", __FUNCTION__);
	vsync_power_state = 0;

	return 0;
}

static int tcc_vsync_runtime_resume(struct device *dev)
{
	printk("%s:  \n", __FUNCTION__);
	vsync_power_state = 1;

	return 0;
}

static const struct dev_pm_ops tcc_vsync_pm_ops = {
	.runtime_suspend	  = tcc_vsync_runtime_suspend,
	.runtime_resume 	  = tcc_vsync_runtime_resume,
	.suspend	= tcc_vsync_suspend,
	.resume =tcc_vsync_resume,
};
#endif

static const struct file_operations tcc_vsync0_fops =
{
	.owner			= THIS_MODULE,
	.open			= tcc_vsync0_open,
	.release		= tcc_vsync0_release,
	.unlocked_ioctl = tcc_vsync0_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = tcc_vsync0_compat_ioctl, // for 32bit OMX
#endif
};

static const struct file_operations tcc_vsync1_fops =
{
	.owner			= THIS_MODULE,
	.open			= tcc_vsync1_open,
	.release		= tcc_vsync1_release,
	.unlocked_ioctl = tcc_vsync1_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = tcc_vsync1_compat_ioctl, // for 32bit OMX
#endif
};

#ifdef CONFIG_USE_SUB_MULTI_FRAME
static const struct file_operations tcc_vsync2_fops =
{
	.owner			= THIS_MODULE,
	.open			= tcc_vsync2_open,
	.release		= tcc_vsync2_release,
	.unlocked_ioctl = tcc_vsync2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = tcc_vsync2_compat_ioctl, // for 32bit OMX
#endif
};

static const struct file_operations tcc_vsync3_fops =
{
	.owner			= THIS_MODULE,
	.open			= tcc_vsync3_open,
	.release		= tcc_vsync3_release,
	.unlocked_ioctl = tcc_vsync3_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = tcc_vsync3_compat_ioctl, // for 32bit OMX
#endif
};
#endif

static struct miscdevice vsync0_misc_device =
{
	MISC_DYNAMIC_MINOR,
	"tcc_vsync0",
	&tcc_vsync0_fops,
};

static struct miscdevice vsync1_misc_device =
{
	MISC_DYNAMIC_MINOR,
	"tcc_vsync1",
	&tcc_vsync1_fops,
};

#ifdef CONFIG_USE_SUB_MULTI_FRAME
static struct miscdevice vsync2_misc_device =
{
	MISC_DYNAMIC_MINOR,
	"tcc_vsync2",
	&tcc_vsync2_fops,
};

static struct miscdevice vsync3_misc_device =
{
	MISC_DYNAMIC_MINOR,
	"tcc_vsync3",
	&tcc_vsync3_fops,
};
#endif

static int tcc_vsync_probe(struct platform_device *pdev)
{
	int ret=0;
	
	spin_lock_init(&vsync_lock) ;
	spin_lock_init(&vsync_lockDisp ) ;
	
	vsync_power_state = 1;
	
	if(tcc_vsync_parse_dt(pdev->dev.of_node) < 0){
		return -ENODEV;
	}
	#if defined(CONFIG_ANDROID) || defined(CONFIG_PLATFORM_STB)
	EX_OUT_LCDC = vsync_vioc0_disp.lcdc_num;
	LCD_LCDC_NUM = vsync_vioc1_disp.lcdc_num;
	#else // for ALS dxb : 160630 inbest
	EX_OUT_LCDC = vsync_vioc1_disp.lcdc_num;
	LCD_LCDC_NUM = vsync_vioc0_disp.lcdc_num;
	#endif

	vsync_vioc0_disp.vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (!vsync_vioc0_disp.vioc_intr)
		return -ENOMEM;

	vsync_vioc0_disp.vioc_intr->id = vsync_vioc0_disp.lcdc_num;
	vsync_vioc0_disp.vioc_intr->bits = (1<<VIOC_DISP_INTR_VSF);

	vsync_vioc1_disp.vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (!vsync_vioc1_disp.vioc_intr)
		return -ENOMEM;

	vsync_vioc1_disp.vioc_intr->id = vsync_vioc1_disp.lcdc_num;
	vsync_vioc1_disp.vioc_intr->bits = (1<<VIOC_DISP_INTR_VSF);
	
#ifdef USE_VSYNC_TIMER
	vsync_timer = tcc_register_timer(NULL, 1000/*msec*/, NULL);
	if (IS_ERR(vsync_timer)) {
		printk("%s: cannot register tcc timer. time:0x%p\n", __func__, vsync_timer);
		vsync_timer = NULL;
		BUG();
	}
#endif

	if (misc_register(&vsync0_misc_device))
	{
		dprintk(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}
	
	if (misc_register(&vsync1_misc_device))
	{
		dprintk(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}

#ifdef CONFIG_USE_SUB_MULTI_FRAME
	if (misc_register(&vsync2_misc_device))
	{
		dprintk(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}

	if (misc_register(&vsync3_misc_device))
	{
		dprintk(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}
#endif
	return ret;
}

static int tcc_vsync_remove(struct platform_device *pdev)
{
	int ret=0;
	
	misc_deregister(&vsync0_misc_device);
	misc_deregister(&vsync1_misc_device);
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	misc_deregister(&vsync2_misc_device);
	misc_deregister(&vsync3_misc_device);
#endif
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id vsync_of_match[] = {
	{ .compatible = "telechips,tcc_vsync" },
	{}
};
MODULE_DEVICE_TABLE(of, vsync_of_match);
#endif

static struct platform_driver tcc_vsync_driver = {
	.probe		= tcc_vsync_probe,
	.remove		= tcc_vsync_remove,
	.driver		= {
		.name	= "tcc_vsync",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm 	= &tcc_vsync_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(vsync_of_match),
#endif

	},
};

#define CONFIG_DUMP_REGS
#ifdef CONFIG_DUMP_REGS
volatile void __iomem *panic_disp0, *panic_disp1;
volatile void __iomem *panic_wdma;
static int dump_vioc_regs(struct notifier_block *this, unsigned long event,
		      void *ptr)
{
	if(panic_disp0)
		pr_err("[vioc disp0] M:0x%08x S:0x%08x\n", __raw_readl(panic_disp0+DIM), __raw_readl(panic_disp0+DSTATUS));	
	if(panic_disp1)
		pr_err("[vioc disp1] M:0x%08x S:0x%08x\n",  __raw_readl(panic_disp1+DIM), __raw_readl(panic_disp1+DSTATUS));
	if(panic_wdma)
		pr_err("[viod wdma2] M:0x%08x S:0x%08x\n", __raw_readl(panic_wdma + WDMAIRQMSK_OFFSET), __raw_readl(panic_wdma + WDMAIRQSTS_OFFSET));
	return NOTIFY_OK;
}

static struct notifier_block panic_vioc = {
	.notifier_call = dump_vioc_regs,
};

static void panic_vioc_init(void)
{
	panic_disp0 = VIOC_DISP_GetAddress(0);
	panic_disp1 = VIOC_DISP_GetAddress(1);
	panic_wdma = VIOC_WDMA_GetAddress(2);
	atomic_notifier_chain_register(&panic_notifier_list, &panic_vioc);
}
#endif

static int __init tcc_vsync_init(void)
{
#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
	int type = VSYNC_MAIN;

	memset(&tccvid_lastframe[0], 0x00, sizeof(tcc_video_lastframe));
	memset(&tccvid_lastframe[1], 0x00, sizeof(tcc_video_lastframe));

	tcc_video_ctrl_last_frame(type, 0);

	type = VSYNC_SUB0;
	#ifdef CONFIG_USE_SUB_MULTI_FRAME
	for(type = VSYNC_SUB0; type < VSYNC_MAX; type++)
	#endif
	{
		tcc_video_ctrl_last_frame(type, 0);
	}
	spin_lock_init(&LastFrame_lockDisp);
#endif
	mutex_init(&vsync_io_mutex);

#ifdef CONFIG_DISPLAY_EXT_FRAME
	mutex_init(&ext_mutex);
#endif
	/*
	DevMajorNum = register_chrdev(0, DEVICE_NAME, &tcc_vsync_fops);
	if (DevMajorNum < 0) {
		printk("%s: device failed widh %d\n", __func__, DevMajorNum);
		err = -ENODEV;
	}
	
	vsync_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(vsync_class)) {
		err = PTR_ERR(vsync_class);
	}
	
	printk("%s Major(%d)\n", __FUNCTION__,DevMajorNum);
	
	device_create(vsync_class,NULL,MKDEV(DevMajorNum, 0),NULL,DEVICE_NAME);
	*/

#ifdef CONFIG_DUMP_REGS
	panic_vioc_init();
#endif

	return platform_driver_register(&tcc_vsync_driver);
}

static void __exit tcc_vsync_exit(void)
{
	
	printk("%s\n", __FUNCTION__);
	platform_driver_unregister(&tcc_vsync_driver);
}

module_init(tcc_vsync_init);
module_exit(tcc_vsync_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC Vsync driver");
MODULE_LICENSE("GPL");
