/****************************************************************************
One line to give the program's name and a brief idea of what it does.
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

#ifndef TCCVIN_DEV_H
#define TCCVIN_DEV_H

// video source
#include "../videosource/videosource_if.h"

// vioc
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_deintls.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_fifo.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_intr.h>

// optional pmap for viqe
#include <soc/tcc/pmap.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/mutex.h>

#define MAX_BUFFERRS			12	// The MAX_BUFFERRS should be matched with Android Camera HAL and Linux Application

typedef struct vioc_path {
	int				vin;
	int				deintl;	// deinterlacer: viqe / deintl_s
	int				scaler;
	int				pgl;
	int				wmixer;
	int				wdma;
	int				fifo;
	int				rdma;
	int				wmixer_out;
	int				disp;
} vioc_path_t;

typedef struct buf_addr {
	unsigned int	y;
	unsigned int	u;
	unsigned int	v;
} buf_addr_t;

typedef struct tccvin_cif {
	// videosource
	TCC_SENSOR_INFO_TYPE		* videosource_info;

	// cif port
	unsigned int				cif_port;
	volatile void __iomem		* cifport_addr;

	// vioc
	struct clk					* vioc_clk;
	vioc_path_t 				vioc_path;

	unsigned int				vioc_irq_reg;
	unsigned int				vioc_irq_num;
	struct vioc_intr_type		vioc_intr;

	// optional pmap
	pmap_t						pmap_viqe;

    pmap_t                      pmap_rcam_preview;
    pmap_t                      pmap_rcam_pgl;

	// framebuffer
	buf_addr_t					preview_buf_addr[MAX_BUFFERRS];

	int							is_handover_needed;
} tccvin_cif_t;

enum cifoper_mode {
	OPER_PREVIEW = 0,
	OPER_CAPTURE,
};

#define PREVIEW_METHOD_SHIFT		0
#define PREVIEW_DD_HANDOVER_SHIFT	1

#define PREVIEW_METHOD_MASK 		(1 << PREVIEW_METHOD_SHIFT)
#define PREVIEW_DD_HANDOVER_MASK	(1 << PREVIEW_DD_HANDOVER_SHIFT)

#define PREVIEW_V4L2				(0 << PREVIEW_METHOD_SHIFT)
#define PREVIEW_DD					(1 << PREVIEW_METHOD_SHIFT)
#define PREVIEW_DD_HANDOVER 		(1 << PREVIEW_DD_HANDOVER_SHIFT)

#if 0
enum PREVIEW_METHOD {
	PREVIEW_V4L2			= (0 << PREVIEW_METHOD_SHIFT),
	PREVIEW_DD				= (1 << PREVIEW_METHOD_SHIFT),
	PREVIEW_DD_HANDOVER 	= ((1 << PREVIEW_DD_HANDOVER_SHIFT) | (1 << PREVIEW_METHOD_SHIFT)),
};
#endif

#define PGL_FORMAT			(VIOC_IMG_FMT_ARGB8888)
#define PGL_BG_R			(0xff)
#define PGL_BG_G			(0xff)
#define PGL_BG_B			(0xff)

struct tccvin_buf {
	struct v4l2_buffer			buf;
	struct list_head			buf_list;
};

typedef struct tccvin_v4l2 {
	struct v4l2_input			input;			// video input
	struct v4l2_pix_format		pix_format;		// pixel format

	enum cifoper_mode			oper_mode;		// operation mode
	int							preview_method;	// 0: v4l2 / 1: directdisplay

	struct mutex				lock;

	struct tccvin_buf			static_buf[MAX_BUFFERRS];
	unsigned int				pp_num;

	// buffering
	struct list_head			capture_buf_list;
	struct list_head			display_buf_list;

	struct work_struct			wdma_work;

	wait_queue_head_t			frame_wait;	/* Waiting on frame data */
	unsigned int				wakeup_int;

	struct task_struct			* threadRecovery;
} tccvin_v4l2_t;

typedef struct tcc_dev {
	struct platform_device		* plt_dev;
	struct video_device			* vid_dev;
	unsigned int				is_dev_opened;

	struct tccvin_cif			cif;
	struct tccvin_v4l2			v4l2;

	struct task_struct			* threadSwitching;
	struct mutex				tccvin_switchmanager_lock;

	int							cam_streaming;
} tccvin_dev_t;

extern int	tccvin_v4l2_init(tccvin_dev_t * vdev);
extern int	tccvin_v4l2_deinit(tccvin_dev_t * vdev);
extern void	tccvin_v4l2_querycap(tccvin_dev_t * vdev, struct v4l2_capability * cap);
extern int	tccvin_v4l2_enum_fmt(struct v4l2_fmtdesc * fmt);
extern void	tccvin_v4l2_g_fmt(tccvin_dev_t * vdev, struct v4l2_format * format);
extern int	tccvin_v4l2_s_fmt(tccvin_dev_t * vdev, struct v4l2_format * format);
extern int	tccvin_v4l2_reqbufs(tccvin_dev_t * vdev, struct v4l2_requestbuffers * req);
extern int	tccvin_v4l2_assign_allocated_buf(tccvin_dev_t * vdev, struct v4l2_buffer * buf);
extern int	tccvin_v4l2_querybuf(tccvin_dev_t * vdev, struct v4l2_buffer * buf);
extern int	tccvin_v4l2_qbuf(tccvin_dev_t * vdev, struct v4l2_buffer * buf);
extern int	tccvin_v4l2_dqbuf(struct file * file, struct v4l2_buffer * buf);
extern int	tccvin_v4l2_streamon(tccvin_dev_t * vdev, int * preview_method);
extern int	tccvin_v4l2_streamoff(tccvin_dev_t * vdev, int * preview_method);
extern int	tccvin_v4l2_g_param(tccvin_dev_t * vdev, struct v4l2_streamparm * gparam);
extern int	tccvin_v4l2_s_param(struct v4l2_streamparm * sparam);
extern int	tccvin_v4l2_enum_input(struct v4l2_input * input);
extern int	tccvin_v4l2_g_input(tccvin_dev_t * vdev, struct v4l2_input * input);
extern int	tccvin_v4l2_s_input(tccvin_dev_t * vdev, struct v4l2_input * input);
extern int	tccvin_v4l2_try_fmt(struct v4l2_format * fmt);
extern int tccvin_set_wmixer_out(tccvin_cif_t * cif, unsigned int ovp);
extern int tccvin_set_ovp_value(tccvin_cif_t * cif);
extern void	tccvin_check_path_status(tccvin_dev_t * vdev, int * status);
extern int tccvin_cif_set_resolution(tccvin_dev_t * vdev, unsigned int width, unsigned int height, unsigned int pixelformat);

#endif//TCCVIN_DEV_H

