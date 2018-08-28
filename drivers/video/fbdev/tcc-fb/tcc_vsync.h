/****************************************************************************
tcc_vsync.h
Description: TCC Vsync Driver 

Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

/*****************************************************************************
*
* structures
*
******************************************************************************/
#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_video_private.h>

#ifdef CONFIG_USE_SUB_MULTI_FRAME
typedef enum {
	VSYNC_MAIN = 0,
	VSYNC_SUB0,
	VSYNC_SUB1,
	VSYNC_SUB2,
	VSYNC_SUB3,
	VSYNC_MAX
} VSYNC_CH_TYPE;
#else
typedef enum {
	VSYNC_MAIN = 0,
	VSYNC_SUB0,
	VSYNC_MAX
} VSYNC_CH_TYPE;
#endif

typedef enum
{
	DRM_INIT = 0,
	DRM_ON,
	DRM_OFF
}HDMI_DRM_MODE;

typedef struct{
	int readIdx ;
	int writeIdx ;
	int clearIdx;

	atomic_t valid_buff_count;
	atomic_t readable_buff_count;

	int max_buff_num ;
	int last_cleared_buff_id;
	int available_buffer_id_on_vpu;
	struct tcc_lcdc_image_update stImage[VPU_BUFFER_MANAGE_COUNT] ;
	struct tcc_lcdc_image_update curr_displaying_imgInfo;
}tcc_vsync_buffer_t;

#define TIME_BUFFER_COUNT			30

typedef struct {
	int Resolution; // resolution changed..
	int Codec; 		//codec changed..
}tcc_lastframe_reason;

typedef struct {
	int support;

	struct tcc_lcdc_image_update 		CurrImage;
	struct tcc_lcdc_image_update		LastImage;
	int enabled;
	pmap_t pmapBuff;

	volatile void __iomem * pRDMA;

	tcc_lastframe_reason reason;
	unsigned int nCount;
}tcc_video_lastframe;

typedef struct {
	tcc_vsync_buffer_t vsync_buffer;
	int type;

	int isVsyncRunning;
	// for time sync
	unsigned int unVsyncCnt ;
	int baseTime;
	unsigned int timeGapIdx ;
	unsigned int timeGapBufferFullFlag;
	int timeGap[TIME_BUFFER_COUNT];
	int timeGapTotal;
	int updateGapTime;
	int vsync_interval;
	int perfect_vsync_flag;

	int skipFrameStatus;
	int overlayUsedFlag;
	int outputMode;
	int video_frame_rate;

	//for deinterlace mode
	int deinterlace_mode;
	int firstFrameFlag;
	int frameInfo_interlace;
	int m2m_mode;
	int output_toMemory;
	int nDeinterProcCount;
	int nTimeGapToNextField;
	int interlace_output;
	int interlace_bypass_lcdc;
	int mvcMode;
	int duplicateUseFlag;
	int vsync_started;
	int lastUdateTime;
	int time_gap_sign;

	int player_no_sync_time;
	int prev_time_stamp;
	int prev_buffer_unique_id;
	int player_seek;
	int wait_too_long_indicate;
	int prevImageWidth;
	int prevImageHeight;

	struct tcc_lcdc_image_update *pIntlNextImage;
	
}tcc_video_disp ;

struct tcc_vsync_display_info_t{
	struct vioc_intr_type	*vioc_intr;

	int lcdc_num;
	int irq_num;
	int irq_reged;
	volatile void __iomem *virt_addr;
};

#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
enum {
	LCD_START_VSYNC,
	HDMI_START_VSYNC
};
#endif

/*****************************************************************************
*
* functions
*
******************************************************************************/
void tca_vsync_video_display_enable(void);
void tca_vsync_video_display_disable(void);
void tcc_vsync_set_firstFrameFlag_all(int firstFrameFlag);
int tcc_video_get_displayed(tcc_video_disp *p, VSYNC_CH_TYPE type);
void tcc_video_clear_frame(tcc_video_disp *p, int idx, int who_call);
void tcc_video_set_framerate(tcc_video_disp *p, int fps);
int tcc_video_check_framerate(tcc_video_disp *p, int fps);
void tcc_video_skip_frame_start(tcc_video_disp *p, VSYNC_CH_TYPE type);
void tcc_video_skip_frame_end(tcc_video_disp *p, VSYNC_CH_TYPE type);
void tcc_video_skip_one_frame(tcc_video_disp *p, int frame_id);
int tcc_video_get_readable_count(tcc_video_disp *p);
int tcc_video_get_valid_count(tcc_video_disp *p);
void tcc_vsync_set_output_mode(tcc_video_disp *p, int mode);
void tcc_vsync_reset_all(void);
void tcc_vsync_if_pop_all_buffer(tcc_video_disp *p);
void tcc_vsync_set_output_mode_all(int mode);
int tcc_vsync_isVsyncRunning(VSYNC_CH_TYPE type);
int tcc_vsync_get_isVsyncRunning(VSYNC_CH_TYPE type);
int tcc_vsync_get_output_toMemory(tcc_video_disp *p);
int tcc_vsync_get_interlace_bypass_lcdc(tcc_video_disp *p);
int tcc_vsync_get_deinterlace_mode(tcc_video_disp *p);
int is_deinterlace_enabled(VSYNC_CH_TYPE type);
void tcc_vsync_hdmi_start(struct tcc_dp_device *pdp_data,int* lcd_video_started);
void tcc_vsync_hdmi_end(struct tcc_dp_device *pdp_data);

void tcc_video_clear_last_frame(unsigned int lcdc_layer, bool reset);
VSYNC_CH_TYPE tcc_vsync_get_video_ch_type(unsigned int lcdc_layer);
int tcc_video_check_last_frame(struct tcc_lcdc_image_update *ImageInfo);

