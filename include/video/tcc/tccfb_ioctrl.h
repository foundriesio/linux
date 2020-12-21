/*
 * Copyright (C) Telechips, Inc.
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
#ifndef _TCC_FB_IOCTL_H
#define _TCC_FB_IOCTL_H

#include "tcc_video_private.h"

#ifndef ALIGNED_BUFF
#define ALIGNED_BUFF(buf, mul) ( ( (unsigned int)buf + (mul-1) ) & ~(mul-1) )
#endif

#define PRESENTATION_LIMIT_RESOLUTION (1024*600)

#define TCCFB_IOCTL_MAGIC 'f'

#define TCC_LCD_FB_IOCTL (0x46FF)

extern unsigned int HDMI_video_width;
extern unsigned int HDMI_video_height;

typedef enum {
	TCC_HDMI_SUSEPNED,
	TCC_HDMI_RESUME,
	TCC_HDMI_VIDEO_START,
	TCC_HDMI_VIDEO_END,
} TCC_HDMI_M;


typedef enum {
	LCDC_TVOUT_NOE_MODE,
	LCDC_TVOUT_UI_MODE,
	LCDC_TVOUT_VIDEO_ONLY_MODE,
	LCDC_TVOUT_OVERALY_MODE,
	LCDC_TVOUT_MODE_MAX
} LCDC_TVOUT_MODE;

typedef enum {
	LCDC_COMPOSITE_NONE_MODE,
	LCDC_COMPOSITE_UI_MODE,
	LCDC_COMPOSITE_VIDEO_ONLY_MODE,
	LCDC_COMPOSITE_OVERALY_MODE,
	LCDC_COMPOSITE_MODE_MAX
} LCDC_COMPOSITE_MODE;

typedef enum {
	LCDC_COMPONENT_NONE_MODE,
	LCDC_COMPONENT_UI_MODE,
	LCDC_COMPONENT_VIDEO_ONLY_MODE,
	LCDC_COMPONENT_OVERALY_MODE,
	LCDC_COMPONENT_MODE_MAX
} LCDC_COMPONENT_MODE;

typedef enum {
	OUTPUT_NONE,
	OUTPUT_HDMI,
	OUTPUT_COMPOSITE,
	OUTPUT_COMPONENT,
	OUTPUT_MAX
} OUTPUT_SELECT;

typedef enum {
	OUTPUT_COMPOSITE_NTSC,
	OUTPUT_COMPOSITE_PAL,
	OUTPUT_COMPOSITE_MAX
} OUTPUT_COMPOSITE_MODE;

typedef enum {
	OUTPUT_COMPONENT_720P,
	OUTPUT_COMPONENT_1080I,
	OUTPUT_COMPONENT_MAX
} OUTPUT_COMPONENT_MODE;

typedef enum {
	OUTPUT_SELECT_NONE,
	OUTPUT_SELECT_HDMI_ENABLE,
	OUTPUT_SELECT_HDMI_TIMING_SET,
	OUTPUT_SELECT_COMPONENT_ENABLE,
	OUTPUT_SELECT_COMPOSITE_ENABLE,
	OUTPUT_SELECT_MAX
} OUTPUT_SELECT_MODE;

struct display_platform_data {
	uint32_t resolution;
	uint32_t output;
	uint32_t hdmi_resolution;
	uint32_t composite_resolution;
	uint32_t component_resolution;
	uint32_t hdmi_mode;
};

#define TCCFB_ROT_90    0x4
#define TCCFB_ROT_180   0x8
#define TCCFB_ROT_270   0x10

typedef struct {
	int width;
	int height;
	int frame_hz;
} tcc_display_size;

typedef struct {
	int resize_up;
	int resize_down;
	int resize_left;
	int resize_right;
} tcc_display_resize;

typedef struct {
	int x;
	int y;
} tcc_mouse;

typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned int option;
	unsigned char *buf;
} tcc_mouse_icon;

typedef struct {
	unsigned int index;
	unsigned int width;
	unsigned int height;
	char *buffer1;
	char *buffer2;
	char *pbuf1;
	char *pbuf2;
	char *vbuf1;
	char *vbuf2;
} tcc_display_mouse;

#define MOUSE_CURSOR_MAX_WIDTH  (50)
#define MOUSE_CURSOR_MAX_HEIGHT (50)
#define MOUSE_CURSOR_BUFF_SIZE \
	(MOUSE_CURSOR_MAX_WIDTH*MOUSE_CURSOR_MAX_HEIGHT*4*2)

#define MAX_ATTACH_BUFF_CNT 3

typedef struct {
	char flag;
	unsigned char index;	// array index must has unsigned type
	char update_flag;
	char update_start;
	char update_started;
	char plugin;
	unsigned int pbuf[MAX_ATTACH_BUFF_CNT];
} tcc_display_attach;

typedef struct {
	struct vioc_intr_type   *vioc_intr;
	int lcdc_num;
	int irq_num;
} tcc_display_attach_intr;

typedef struct {
	char enable;
	char mode;
	char count;
	char fbtype;
} tcc_display_divide;

typedef enum {
	LCDC_LAYER_0,
	LCDC_LAYER_1,
	LCDC_LAYER_2,
	LCDC_LAYER_3,
	LCDC_LAYER_MAX
} LCD_IMG_LAYER_TYPE;

typedef enum {
	TCC_LCDC_IMG_FMT_1BPP,
	TCC_LCDC_IMG_FMT_2BPP,
	TCC_LCDC_IMG_FMT_4BPP,
	TCC_LCDC_IMG_FMT_8BPP,
	TCC_LCDC_IMG_FMT_RGB332 = 8,
	TCC_LCDC_IMG_FMT_RGB444 = 9,
	TCC_LCDC_IMG_FMT_RGB565 = 10,
	TCC_LCDC_IMG_FMT_RGB555 = 11,
	TCC_LCDC_IMG_FMT_RGB888 = 12,
	TCC_LCDC_IMG_FMT_RGB666 = 13,
	TCC_LCDC_IMG_FMT_RGB888_3   = 14,
	TCC_LCDC_IMG_FMT_ARGB6666_3 = 15,
	TCC_LCDC_IMG_FMT_COMP = 16,
	TCC_LCDC_IMG_FMT_DECOMP = (TCC_LCDC_IMG_FMT_COMP),
	TCC_LCDC_IMG_FMT_444SEP = 21,
	TCC_LCDC_IMG_FMT_UYVY = 22,
	TCC_LCDC_IMG_FMT_VYUY = 23,

	TCC_LCDC_IMG_FMT_YUV420SP = 24,
	TCC_LCDC_IMG_FMT_YUV422SP = 25,
	TCC_LCDC_IMG_FMT_YUYV = 26,
	TCC_LCDC_IMG_FMT_YVYU = 27,

	TCC_LCDC_IMG_FMT_YUV420ITL0 = 28,
	TCC_LCDC_IMG_FMT_YUV420ITL1 = 29,
	TCC_LCDC_IMG_FMT_YUV422ITL0 = 30,
	TCC_LCDC_IMG_FMT_YUV422ITL1 = 31,
	TCC_LCDC_IMG_FMT_MAX
} TCC_LCDC_IMG_FMT_TYPE;

struct tcc_lcdc_image_update {
	unsigned int Lcdc_layer;
	unsigned int enable;
	unsigned int Frame_width;
	unsigned int Frame_height;

	unsigned int Image_width;
	unsigned int Image_height;
	unsigned int offset_x; //position
	unsigned int offset_y;  //position
	unsigned int addr0;
	unsigned int addr1;
	unsigned int addr2;
	unsigned int fmt;   //TCC_LCDC_IMG_FMT_TYPE
	unsigned int on_the_fly; // 0: not use , 1 : scaler0 ,  2 :scaler1

// +----------------+----
// | src            |
// |   +------+     |---- crop_top
// |   | dest |     |
// |   |      |     |
// |   +------+     |---- crop_bottom (crop_top + width of dest)
// |                |
// +----------------+-----
// |<->|  crop_left
//
// |<-------->|  crop_right (crop_top + height of dest)
//
	int crop_top;
	int crop_bottom;
	int crop_left;
	int crop_right;

#if defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT) \
	|| defined(TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
	int time_stamp;
	int sync_time;
	int first_frame_after_seek;
	unsigned int buffer_unique_id;
	unsigned int overlay_used_flag;
	OUTPUT_SELECT outputMode;
	int output_path;

	int deinterlace_mode;
	int m2m_mode;
	int output_toMemory;
	int frameInfo_interlace;
#endif
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	int mosaic_mode;
	unsigned int buffer_offset_x;
	unsigned int buffer_offset_y;
	unsigned int buffer_Image_width;
	unsigned int buffer_Image_height;
#endif
	int odd_first_flag;
	int one_field_only_interlace;
	int viqe_queued;

	int MVCframeView;
	unsigned int MVC_Base_addr0;
	unsigned int MVC_Base_addr1;
	unsigned int MVC_Base_addr2;

	unsigned int dst_addr0;
	unsigned int dst_addr1;
	unsigned int dst_addr2;

	int max_buffer;
	int ex_output;
	unsigned int fps;
	int bit_depth;

	unsigned int codec_id;
	int bSize_Changed; // just usage for kernel itself.
	TCC_PLATFORM_PRIVATE_PMEM_INFO private_data;
};

struct tcc_lcdc_get_image_info {
	unsigned int lcdc_num;
	unsigned int layer;
	unsigned int enable;
	unsigned int lcd_width;
	unsigned int lcd_height;
	unsigned int image_width;
	unsigned int image_height;
	unsigned int offset_x; //position
	unsigned int offset_y;  //position
	unsigned int addr0;
	unsigned int addr1;
	unsigned int addr2;
	unsigned int fmt;   //TCC_LCDC_IMG_FMT_TYPE
};

typedef enum {
	LCDC_HDMI_NONE_MODE,
	LCDC_HDMI_UI_MODE,
	LCDC_HDMI_VIDEO_ONLY_MODE,
	LCDC_HDMI_OVERALY_MODE,
	LCDC_HDMI_MODE_MAX
} LCDC_HDMI_MODE;

struct lcdc_timimg_parms_t {
	unsigned int id;
	unsigned int iv;
	unsigned int ih;
	unsigned int ip;
	unsigned int dp;
	unsigned int ni;
	unsigned int tv;
	unsigned int tft;
	unsigned int stn;

	//LHTIME1
	unsigned int lpw;
	unsigned int lpc;
	//LHTIME2
	unsigned int lswc;
	unsigned int lewc;
	//LVTIME1
	unsigned int vdb;
	unsigned int vdf;
	unsigned int fpw;
	unsigned int flc;
	//LVTIME2
	unsigned int fswc;
	unsigned int fewc;
	//LVTIME3
	unsigned int fpw2;
	unsigned int flc2;
	//LVTIME4
	unsigned int fswc2;
	unsigned int fewc2;

	unsigned int framepacking;
	unsigned int format;    // 0: rgb, 1:ycc444, 2:ycc422, 3:ycc420

#if 1//defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	unsigned int dv_reg_phyaddr;
	unsigned int dv_md_phyaddr;
	unsigned int dv_ll_mode;
	unsigned int dv_noYUV422_SDR;
	unsigned int dv_hdmi_clk_khz;
	unsigned int dv_vsvdb[12];
	unsigned int dv_vsvdb_size;
	unsigned int dv_rgb_tunneling;
#endif
};

typedef struct {
	unsigned int lcdc_num;
	unsigned int layer_num;
} lcdc_layerctrl_params;

typedef struct {
	unsigned int lcdc_num;
	unsigned int red;
	unsigned int green;
	unsigned int blue;
} lcdc_bgcolor_params;

typedef struct {
	unsigned int lcdc_num;
	unsigned int layer_num;
	unsigned int layer_alpha;
} lcdc_alpha_params;

// chromaRGB 0~7 bit is chorma key color
// chromaRGB 16~23 bit is chorma key mask value
typedef struct {
	unsigned int lcdc_num;
	unsigned int layer_num;
	unsigned int enable;
	unsigned int chromaR; // 0~7 key color , 16~ 24 mask
	unsigned int chromaG; // 0~7 key color , 16~ 24 mask
	unsigned int chromaB; // 0~7 key color , 16~ 24 mask
} lcdc_chromakey_params;

typedef struct {
	unsigned int enable;
	unsigned int color;
} lcdc_chroma_params;


typedef struct {
	unsigned int lcdc_num;
	unsigned int onoff;
	unsigned int GammaBGR[256];
} lcdc_gamma_params;

typedef struct {
	unsigned int lcdc;
	unsigned int wd;
	unsigned int ht;
	unsigned int index;
	struct tcc_lcdc_image_update image;
} exclusive_ui_update;

typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned int conversion;
} exclusive_ui_size;

typedef struct {
	unsigned int index;
	unsigned int buf_wd;
	unsigned int buf_ht;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_wd;
	unsigned int src_ht;
	unsigned int dst_x;
	unsigned int dst_y;
	void *pBaseAddr;
} exclusive_ui_frame;

typedef struct {
	unsigned int ratio_x;
	unsigned int ratio_y;
}exclusive_ui_ratio;

typedef struct {
	unsigned int enable;
	unsigned int bd;
	unsigned int plane_width;
	unsigned int plane_height;
	unsigned int base_addr_dma;
	unsigned int base_addr_cpu;
	unsigned int interlace;
	unsigned int process;
	unsigned int update;
	unsigned int clear;
	struct tcc_lcdc_image_update update_img;
} exclusive_ui_params;

typedef struct {
	unsigned int enable;
	unsigned int ratio;
	unsigned int ratio_x;
	unsigned int ratio_y;
	unsigned int ar_wd;
	unsigned int ar_ht;
	unsigned int config;
} exclusive_ui_ar_params;

typedef struct {
	unsigned int num; // wmixer number
	unsigned int ovp; // wmixer overlay priority
} tccfb_set_wmix_order_type;

typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned int bits_per_pixel;
	unsigned int offset;
	unsigned int xres_virtual;
	unsigned int fence_fd;
} external_fbioput_vscreeninfo;

typedef struct {
	unsigned int onoff;
	unsigned int brightness;
} lut_ctrl_params;

struct tcc_ext_dispregion {
	unsigned int start_x;
	unsigned int start_y;
	unsigned int width;
	unsigned int height;
};

struct  tcc_fb_extra_data {
	unsigned int pxdw;
	unsigned int r2y;
	unsigned int r2ymd;
	unsigned int swapbf;
};

#if defined(CONFIG_ARCH_TCC898X) \
	|| defined(CONFIG_ARCH_TCC899X) \
	|| defined(CONFIG_ARCH_TCC901X)
struct lcdc_colorenhance_params {
	unsigned int contrast;		// 0~0x3FF, default value 0x80
	unsigned int saturation;	// 0~0x3FF, default value 0x0
	unsigned int brightness;	// 0~0x3FF, default value 0x0
	unsigned int hue;			// 0~0x1FF, default value 0x0
	unsigned int lcdc_type;		//0 Main, 1: sencond
	bool check_hue_onoff;		// on:1, off:0
	bool check_colE_onoff;		// on:1, off:0
};
#else
struct lcdc_colorenhance_params {
	int contrast;		// default value 0x20
	int brightness;		// default value 0x0
	int hue;			// default value 0x0
	unsigned int lcdc_type;  //0 Main, 1: sencond
};
#endif

#define LASTFRAME_FOR_RESOLUTION_CHANGE 0x1
#define LASTFRAME_FOR_CODEC_CHANGE  0x2


#define TCC_LCDC_HDMI_START             0x0010
#define TCC_LCDC_HDMI_TIMING            0x0011
#define TCC_LCDC_HDMI_DISPLAY           0x0012
#define TCC_LCDC_HDMI_END               0x0013
#define TCC_LCDC_HDMI_CHECK             0x0014
#define TCC_LCDC_HDMI_MODE_SET          0x0015
#define TCC_LCDC_HDMI_SET_SIZE          0x0016
#define TCC_LCDC_HDMI_GET_SIZE          0x0017

#define TCC_LCDC_TVOUT_MODE_SET         0x0018
#define TCC_LCDC_HDMI_STATUS_SET        0x0019
#define TCC_LCDC_HDMI_EXTRA_SET         0x001A
#define TCC_LCDC_HDMI_DISPDEV_ID        0x001B

#define TCC_LCDC_COMPOSITE_CHECK        0x0020
#define TCC_LCDC_COMPOSITE_MODE_SET     0x0021

#define TCC_LCDC_COMPONENT_CHECK        0x0030
#define TCC_LCDC_COMPONENT_MODE_SET     0x0031

#define TCC_LCDC_OUTPUT_MODE_CHECK      0x0040
#define TCC_LCDC_OUTPUT_MODE_SET        0x0041

#define TCC_LCDC_SET_OUTPUT_RESIZE_MODE 0x0043
#define TCC_SECONDARY_OUTPUT_RESIZE_MODE_STB    0x0044
#define TCC_LCDC_SET_OUTPUT_ATTACH_RESIZE_MODE  0x0045
#define TCC_LCDC_SET_HDMI_R2YMD_MAGIC           0xB0AA
#define TCC_LCDC_SET_HDMI_R2YMD                 0x004D

#define TCC_LCDC_3D_UI_ENABLE           0x005B
#define TCC_LCDC_3D_UI_DISABLE          0x005C

#define TCC_LCDC_VIDEO_SET_MVC_STATUS   0x006F

#define TCC_LCD_FBIOPUT_VSCREENINFO     0x008F
#define TCC_LCDC_UI_UPDATE_CTRL         0x0090
#define FBIO_DISABLE                    0x0091
#define TCC_LCDC_REFER_VSYNC_ENABLE     0x0092
#define TCC_LCDC_REFER_VSYNC_DISABLE    0x0093
#define TCC_HDMI_FBIOPUT_VSCREENINFO    0x0095
#define TCC_CVBS_FBIOPUT_VSCREENINFO    0x0096
#define TCC_COMPONENT_FBIOPUT_VSCREENINFO   0x0099

#define TCC_LCDC_DISPLAY_UPDATE         0x0097
#define TCC_LCDC_DISPLAY_END            0x0098

#define TCC_LCD_BL_SET                  0x0100
#define TCC_LCDC_SET_LUT_DVI            0x0103

/*
 * Common interface for second display
 */
#define TCC_EXT_FBIOPUT_VSCREENINFO     0x0110

#define TCC_SH_DISPLAY_FBIOPUT_VSCREENINFO 0x300
#define TCC_LCDC_MOUSE_SHOW             0x0200
#define TCC_LCDC_MOUSE_MOVE             0x0201
#define TCC_LCDC_MOUSE_ICON             0x0202
#define TCC_LCDC_FBCHANNEL_ONOFF        0x0203
#define TCC_LCDC_FB_CHROMAKEY_CONTROL   0x0204
#define TCC_LCDC_FB_CHROMAKEY_CONTROL_KERNEL 0x1204


#define TCC_LCDC_GET_DISPLAY_TYPE       0x0401
#define TCC_FB_UPDATE_LOCK              0x0402

#define GET_2D_COMPRESSION_FB_INFO      0x0501
#define CHECK_2D_COMPRESSION_EN         0x0502

#define TCC_LCDC_GET_DISP_FU_STATUS     0x0601

//#define TCC_LCDC_FB_SKIP_FRAME_START	0x0701
//#define TCC_LCDC_FB_SKIP_FRAME_END	0x0702
#define TCC_LCDC_FB_SWAP_VPU_FRAME		0x0703

#define TCC_LCDC_SET_COLOR_ENHANCE \
	_IOW(TCCFB_IOCTL_MAGIC, 0x1111, struct lcdc_colorenhance_params)
#define TCC_LCDC_GET_COLOR_ENHANCE \
	_IOW(TCCFB_IOCTL_MAGIC, 0x1112, struct lcdc_colorenhance_params)
#define TCC_LCDC_SET_M2M_LASTFRAME \
	_IOW(TCCFB_IOCTL_MAGIC, 0x1110, unsigned int)

#endif
