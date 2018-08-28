/*
 * tcc_vout.h
 *
 * Copyright (C) 2013 Telechips, Inc. 
 *
 * Video-for-Linux (Version 2) video output driver for Telechips SoC.
 *
 * This package is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. 
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED 
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 *
 */
#ifndef __TCC_VOUT_H__
#define __TCC_VOUT_H__

#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include <soc/tcc/pmap.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_deintls.h>
#include <video/tcc/tcc_wmixer_ioctrl.h>

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tcc_video_private.h>
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/TCC_VP9DEC.h>
#endif

#include <soc/tcc/timer.h>
#include <video/tcc/tcc_vout_v4l2.h>

#define VOUT_NAME		"tcc-vout-video"
#define VOUT_VERSION	KERNEL_VERSION(0, 1, 0)
#define VOUT_CLK_SRC	"lcdc1"
#define VOUT_MEM_PATH_PMAP_NAME	"v4l2_vout0"
#define VOUT_DISP_PATH_PMAP_NAME	"video"		/* using vpu pmap */

#ifdef CONFIG_VOUT_USE_VSYNC_INT
#define VOUT_DRV_ERR_DROPFRM	(-2)
#define VOUT_DRV_ERR_WAITFRM	(-1)
#define VOUT_DRV_NOERR			(0)
#endif

#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
#define WMIXER_PATH "/dev/wmixer0"
#endif

/* to check error state */
#define MAX_ERROR_CNT		10

/* disp_path uses 2nd scaler */
//#define VOUT_DISP_PATH_SCALER

/* Rounds an integer value up to the next multiple of num */
#define ROUND_UP_2(num)		(((num)+1)&~1)
#define ROUND_UP_4(num)		(((num)+3)&~3)
#define ROUND_UP_8(num)		(((num)+7)&~7)
#define ROUND_UP_16(num)	(((num)+15)&~15)
#define ROUND_UP_32(num)	(((num)+31)&~31)
#define ROUND_UP_64(num)	(((num)+63)&~63)
/* Rounds an integer value down to the next multiple of num */
#define ROUND_DOWN_2(num)	((num)&(~1))
#define ROUND_DOWN_4(num)	((num)&(~3))
#define ROUND_DOWN_8(num)	((num)&(~7))
#define ROUND_DOWN_16(num)	((num)&(~15))
#define ROUND_DOWN_32(num)	((num)&(~31))
#define ROUND_DOWN_64(num)	((num)&(~63))

/* Still-image format */
#define STILL_IMGAGE			0xFF00

/* rdma alpha */
#define RDMA_ALPHA_ASEL_GLOBAL	0
#define RDMA_ALPHA_ASEL_PIXEL	1

/* mplane */
#define MPLANE_NUM	2
#define MPLANE_VID	0
#define MPLANE_SUB	1

#define MIN_DEINTLBUF_NUM      (3)

enum VOUT_CH_TYPE {
	VOUT_MAIN,
	VOUT_SUB,
	VOUT_MAX_CH,
};

#ifdef CONFIG_VIOC_MAP_DECOMP
enum vioc_mapconv_list {
	VIOC_MC_00,
	VIOC_MC_01,
};

/* map_conv compressed information */
enum mplane_mapconv_component {
	VID_HEVC_FRAME_BASE_Y0 = 11,
	VID_HEVC_FRAME_BASE_Y1 = 12,
	VID_HEVC_FRAME_BASE_C0 = 13,
	VID_HEVC_FRAME_BASE_C1 = 14,
	VID_HEVC_OFFSET_BASE_Y0 = 15,
	VID_HEVC_OFFSET_BASE_Y1 = 16,
	VID_HEVC_OFFSET_BASE_C0 = 17,
	VID_HEVC_OFFSET_BASE_C1 = 18,
	VID_HEVC_LUMA_STRIDE = 19,
	VID_HEVC_CHROMA_STRIDE =20,
	VID_HEVC_LUMA_BIT_DEPTH	= 21,
	VID_HEVC_CHROMA_BIT_DEPTH = 22,
	VID_HEVC_FRMAE_ENDIAN = 23,
};
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
enum vioc_dtrc_list {
	VIOC_DTRC_00,
	VIOC_DTRC_01,
};

/* dtrc compressed information */
enum mplane_dtrc_component {
	VID_DTRC_FRAME_BASE_Y0 = 11,
	VID_DTRC_FRAME_BASE_Y1 = 12,
	VID_DTRC_FRAME_BASE_C0 = 13,
	VID_DTRC_FRAME_BASE_C1 = 14,
	VID_DTRC_TABLE_LUMA0 = 15,
	VID_DTRC_TABLE_LUMA1 = 16,
	VID_DTRC_TABLE_CHROMA0 = 17,
	VID_DTRC_TABLE_CHROMA1 = 18,
	VID_DTRC_LUMA_TABLE_SIZE = 19,
	VID_DTRC_CHROMA_TABLE_SIZE = 20,
	VID_DTRC_LUMA_BIT_DEPTH = 21,
	VID_DTRC_CHROMA_BIT_DEPTH = 22,
	VID_DTRC_FRAME_HEIGHT = 23,
	VID_DTRC_FRAME_WIDTH = 24,
	VID_DTRC_FRAME_STRIDE = 25,
};
#endif

enum dec_vid_format {
	/* video pixelformat */
	DEC_FMT_420IL0 = 0,		// yuv420 interleaved type 0 foramt (0x1C)
	DEC_FMT_422SEP = 1,		// yuv422 seperate format (0x19)
	DEC_FMT_422V = 2,		// unknown format
	DEC_FMT_444SEP = 3,		// yuv444 seperate format (0x15)
	DEC_FMT_400 = 4,		// unknown formatt
	DEC_FMT_420SEP = 5,		// yuv420 seperate format (0x18)
};

enum mplane_vid_component {
	/* video information */
	VID_SRC = 0,			// MPLANE_VID 0x0
	VID_NUM = 1,			// num of mplanes
	VID_BASE1 = 2,			// base1 address of video src (U/Cb)
	VID_BASE2 = 3,			// base1 address of video src (V/Cr)
	VID_WIDTH = 4,			// width/height of video src
	VID_HEIGHT = 5,
	VID_CROP_LEFT = 6,		// crop-[left/top/width/height] of video src
	VID_CROP_TOP = 7,
	VID_CROP_WIDTH = 8,
	VID_CROP_HEIGHT = 9,
	VID_CONVERTER_EN = 10,

	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	VID_HDR_VERSION = 26,
	VID_HDR_STRUCT_SIZE = 27,
	VID_HDR_BIT_DEPTH = 28,
	VID_HDR_COLOR_PRIMARY = 29,
	VID_HDR_TC = 30,
	VID_HDR_MC = 31,
	VID_HDR_PRIMARY_X0 = 32,
	VID_HDR_PRIMARY_X1 = 33,
	VID_HDR_PRIMARY_X2 = 34,
	VID_HDR_PRIMARY_Y0 = 35,
	VID_HDR_PRIMARY_Y1 = 36,
	VID_HDR_PRIMARY_Y2 = 37,
	VID_HDR_WPOINT_X = 38,
	VID_HDR_WPOINT_Y = 39,
	VID_HDR_MAX_LUMINANCE = 40,
	VID_HDR_MIN_LUMINANCE = 41,
	VID_HDR_MAX_CONTENT = 42,
	VID_HDR_MAX_PIC_AVR	= 43,
	VID_HDR_EOTF		= 44,
	VID_HDR_DESCRIPTOR_ID = 45,
	#endif

	VID_MJPEG_FORMAT	= 46,
};

enum mplane_sub_component {
	/* subtitle information */
	SUB_SRC = 0,			// MPLANE_SUB 0x1
	SUB_ON = 1,				// subtitle on(1) or off(0)
	SUB_BUF_INDEX = 2,		// index of subtitle buf
	SUB_FOURCC = 3,			// forcc format
	SUB_WIDTH = 4,			// width
	SUB_HEIGHT = 5,			// height
	SUB_OFFSET_X = 6,		// x position
	SUB_OFFSET_Y = 7,		// y position
};

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
enum mplane_dolby_component {
	VID_DOLBY_EN = 64,
	VID_DOLBY_REG_ADDR = 65,
	VID_DOLBY_MD_HDMI_ADDR = 66,
	VID_DOLBY_EL_OFFSET0 = 67,
	VID_DOLBY_EL_OFFSET1 = 68,
	VID_DOLBY_EL_OFFSET2 = 69,
	VID_DOLBY_EL_BUFF_WIDTH = 70,
	VID_DOLBY_EL_BUFF_HEIGHT = 71,
	VID_DOLBY_EL_FRAME_WIDTH = 72,
	VID_DOLBY_EL_FRAME_HEIGHT = 73,
	VID_DOLBY_REG_OUT_TYPE = 74,
	VID_DOLBY_CONTENT_TYPE = 75,
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
	VID_DOLBY_OSD_OFFSET0 = 76,
#endif
};
#endif

struct tcc_v4l2_img {
	unsigned int base0;
	unsigned int base1;
	unsigned int base2;
};

struct vioc_disp {
	int id;
	volatile void __iomem *addr;
	unsigned int irq;
	unsigned int irq_enable;			// avoid overlapping irq
	struct vioc_intr_type	*vioc_intr;
};

struct vioc_rdma {
	int id;
	volatile void __iomem *addr;
	struct tcc_v4l2_img img;
	unsigned int width;
	unsigned int height;
	unsigned int fmt;
	unsigned int bf;		// BFIELD indication. 0: top, 1: bottom
	unsigned int y2r;
	unsigned int y2rmd;
	unsigned int intl;
	unsigned int swap;		// RGB Swap Register
	unsigned int y_stride;	// Y-stride
};

struct vioc_wdma {
	int id;
	volatile void __iomem *addr;
	struct tcc_v4l2_img img;
	unsigned int width;
	unsigned int height;
	unsigned int fmt;
	unsigned int r2y;
	unsigned int r2ymd;
	unsigned int cont;
	unsigned int irq;
	unsigned int irq_enable;			// avoid overlapping irq
	struct vioc_intr_type	*vioc_intr;
};

struct vioc_wmix {
	int id;
	volatile void __iomem *addr;
	unsigned int width;
	unsigned int height;
	unsigned int pos;					// wmix image position
	unsigned int ovp;					// wmix overlay priority
	int left;
	int top;
};

struct vioc_alpha {
	unsigned int src_type;		// v4l2_buffer.m.planes[1].reserved[0]
	unsigned int on;			// v4l2_buffer.m.planes[1].reserved[1]
	unsigned int buf_index;		// v4l2_buffer.m.planes[1].reserved[2]
	unsigned int fourcc;		// v4l2_buffer.m.planes[1].reserved[3]
	unsigned int width;			// v4l2_buffer.m.planes[1].reserved[4]
	unsigned int height;		// v4l2_buffer.m.planes[1].reserved[5]
	unsigned int offset_x;		// v4l2_buffer.m.planes[1].reserved[6]
	unsigned int offset_y;		// v4l2_buffer.m.planes[1].reserved[7]

	unsigned int reserved8;
	unsigned int reserved9;
	unsigned int reserved10;
};

struct vioc_sc {
	int id;
	volatile void __iomem *addr;
};

struct vioc_viqe {
	int id;
	volatile void __iomem *addr;
	unsigned int y2r;
	unsigned int y2rmd;
};

struct vioc_deintls {
	unsigned int id;
};

#ifdef CONFIG_VIOC_MAP_DECOMP
struct vioc_mc {
	int id;
	hevc_MapConv_info_t mapConv_info;
};
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
struct vioc_dtrc {
	int id;
	vp9_compressed_info_t dtrcConv_info;
};
#endif

enum tcc_pix_fmt {
	TCC_PFMT_YUV420,
	TCC_PFMT_YUV422,
	TCC_PFMT_RGB,
};

enum deintl_type {
	VOUT_DEINTL_NONE,
	VOUT_DEINTL_VIQE_BYPASS,
	VOUT_DEINTL_VIQE_2D,
	VOUT_DEINTL_VIQE_3D,
	VOUT_DEINTL_S,
};

struct tcc_v4l2_buffer {
	int index;

	struct v4l2_buffer buf;
	unsigned int img_base0;				// RDMABASE0
	unsigned int img_base1;				// RDMABASE1
	unsigned int img_base2;				// RDMABASE2
};

struct tcc_vout_vioc {
	struct clk *vout_clk;

	/* display path */
	struct vioc_disp disp;
	struct vioc_rdma rdma;
	struct vioc_wmix wmix;
	struct vioc_sc sc;

	/* deinterlace path */
	struct vioc_rdma m2m_rdma;
	struct vioc_wmix m2m_wmix;
	struct vioc_wdma m2m_wdma;
	struct vioc_viqe viqe;
	struct vioc_deintls deintl_s;

	/* subtitle path */
	struct vioc_rdma m2m_subplane_rdma;
	struct vioc_wmix m2m_subplane_wmix;
	struct vioc_rdma subplane_rdma;
	struct vioc_wmix subplane_wmix;
	struct vioc_alpha subplane_alpha;
	int subplane_init;
	int subplane_buf_index;
	int subplane_enable;
#ifdef CONFIG_VIOC_MAP_DECOMP
	struct vioc_mc map_converter;
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
	struct vioc_dtrc dtrc;
#endif

#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
	struct vioc_rdma lastframe_rdma;
#endif
};

struct tcc_vout_device {
	int id;
	int opened;
	struct device_node *v4l2_np;
	struct v4l2_device v4l2_dev;
	struct video_device *vdev;
	void __iomem *base;
	struct mutex lock;					// lock to protect the shared data in ioctl

	struct tcc_vout_vioc *vioc;
	enum tcc_vout_status status;

	int fmt_idx;
	int bpp;
	int pfmt;

	struct v4l2_pix_format src_pix;		// src image format
	struct v4l2_rect disp_rect;			// display output area
	struct v4l2_rect crop_src;			// to crop source video (deintl_rdma crop)

	/* vout */
	unsigned int force_userptr;			// force V4L2_MEMORY_USERPTR
	pmap_t pmap;						// for only V4L2_MEMORY_MMAP

	/* reqbuf */
	struct tcc_v4l2_buffer *qbufs;
	enum v4l2_memory memory;
	int nr_qbufs;
	int mapped;
	int clearFrameMode;					//

	/* deinterlce */
	enum v4l2_field previous_field;		// previous field for deinterlace
	enum deintl_type deinterlace;		// deinterlacer type
	pmap_t deintl_pmap;
	int deintl_nr_bufs, deintl_nr_bufs_count;
	unsigned int deintl_buf_size;		// full size of deintl_buf, it is depended on panel size.
	struct tcc_v4l2_buffer *deintl_bufs;
	wait_queue_head_t frame_wait;
	int wakeup_int;
	int frame_count;
	int deintl_force;					// 0: depend on stream info 1: depend on 'deinterlace'
	int first_frame;
	enum v4l2_field first_field;
	int firstFieldFlag;

	int baseTime;
	unsigned int update_gap_time;

	//onthefly
	int onthefly_mode;
	int display_done;
	int last_displayed_buf_idx;
	int force_sync;

	//buffer management
	int popIdx;
	int pushIdx;
	int clearIdx;
	atomic_t displayed_buff_count;
	atomic_t readable_buff_count;
	struct v4l2_buffer *last_cleared_buffer;

	// kernel timer
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	struct tcc_timer *vout_timer;
	struct timer_list ktimer;
	int ktimer_enable;
	#endif

	#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
	struct v4l2_buffer lastframe;
	#endif

	#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	unsigned int dolby_frame_count;
	#endif

	#ifdef CONFIG_TCC_VIOCMG
	int is_viqe_shared;
	#endif
};

int tcc_vout_try_bpp(unsigned int pixelformat, enum v4l2_colorspace *colorspace);
int tcc_vout_try_pix(unsigned int pixelformat);
#endif //__TCC_VOUT_H__
