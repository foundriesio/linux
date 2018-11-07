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

#include <media/v4l2-common.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#ifndef TCC_HDIN_VIDEO_H
#define TCC_HDIN_VIDEO_H

#define TCC_HDIN_MAX_BUFNBRS 		10
#define ALIGNED_BUFF(buf, mul) ( ( (unsigned int)buf + (mul-1) ) & ~(mul-1) )

#ifndef ON
#define ON                              1
#endif
#ifndef OFF
#define OFF                             0
#endif

typedef struct
{
	int (*Open)(struct file *);
	int (*Close)(struct file *);
	int (*GetVideoSize)(struct file *);
	int (*GetAudioSR)(void);
	int (*GetAudioType)(void);
}
MODULE_FUNC_TYPE;

typedef struct {
    unsigned int            p_Y;  
    unsigned int            p_Cb;
    unsigned int            p_Cr;
}img_buf_t;

typedef struct {
	unsigned short            target_x;
    unsigned short            target_y;
}hdin_main_set;

enum hdin_out_fmt {
	M420_EVEN = 0,
	M420_ZERO = 1,
	M420_ODD = 2,
	RGB565 = 3,
	ARGB8888 = 4,

};

enum hdin_RGB_Swap_mode {
	SWAP_RGB = 0,
	SWAP_RBG = 1,
	SWAP_GRB = 2,
	SWAP_GBR = 3,
	SWAP_BRG = 4,
	SWAP_BGR = 5,
};

enum hdin_stream_state {
	STREAM_OFF,
	STREAM_INTERRUPT,
	STREAM_ON,
};

typedef struct hdin_c_t {
	hdin_main_set			main_set;
	unsigned char           pp_num;        /* used pingpong memory number */
	img_buf_t               preview_buf[TCC_HDIN_MAX_BUFNBRS];
	enum hdin_out_fmt      fmt;

} hdin_cfg_t;

struct tcc_hdin_buffer {
	struct list_head buf_list;
	struct v4l2_buffer v4lbuf;
};

struct hdin_gpio {
	int	pwr_port;
	int	key_port;
	int	rst_port;
	int	int_port;
};

struct VIOC_dt_parse{
	unsigned int  index;
	volatile void __iomem *address;	
};

struct TCC_VIOC {
	struct VIOC_dt_parse 	wmixer;
	struct VIOC_dt_parse 	vin;
	struct VIOC_dt_parse 	lut;	
	struct VIOC_dt_parse 	wdma;
	struct VIOC_dt_parse 	scaler;
	struct VIOC_dt_parse 	viqe;
	struct VIOC_dt_parse 	config;

	unsigned int	polarity_hsync;
	unsigned int	polarity_vsync;	
	unsigned int	field_bfield_low;
	unsigned int	polarity_data_en;
	unsigned int	gen_field_en;
	unsigned int	polarity_pclk;
	unsigned int	conv_en;
	unsigned int	hsde_connect_en;
	unsigned int	vs_mask;
	unsigned int	input_fmt;
	unsigned int	data_order;
	unsigned int	intpl_en;
	unsigned int	wdma_swap_order;

	
};

struct TCC_HDIN{
	struct vioc_intr_type	*vioc_intr;
	hdin_cfg_t hdin_cfg;
	struct list_head active_empty_buf_q;
	struct list_head wait_capture_buf_q;
	struct list_head capture_done_buf_q;
	struct tcc_hdin_buffer static_buf[TCC_HDIN_MAX_BUFNBRS];
	struct tcc_hdin_buffer *prev_buf;
	unsigned int wakeup_int;
	wait_queue_head_t frame_wait;	/* Waiting on frame data */	
	struct mutex	lock;
	spinlock_t 		spin_lock;  
	enum hdin_stream_state stream_state;
	
};

struct tcc_hdin_device{
	
	struct device_node	*np;
	struct video_device *vfd;
	struct TCC_HDIN data;
	struct v4l2_pix_format pix_format;
	struct hdin_gpio	gpio;
	struct clk *hdin_clk;
	struct TCC_VIOC vioc;

	MODULE_FUNC_TYPE func;
	
	int hdin_enabled;
	int hdin_interlaced_mode;
	int current_resolution;
	int interrupt_status;
	
	unsigned char hdin_irq;
	unsigned char hdin_opend;
	
	unsigned int input_width;
	unsigned int input_height;

	unsigned int viqe_area;

};

extern int	hdin_video_buffer_set(struct tcc_hdin_device *vdev,struct v4l2_requestbuffers *req);
extern int  hdin_video_start_stream(struct tcc_hdin_device *vdev);
extern int  hdin_video_stop_stream(struct tcc_hdin_device *vdev);
extern int 	hdin_video_set_resolution(struct tcc_hdin_device *vdev, unsigned int pixel_fmt, unsigned short width, unsigned short height);
extern int 	hdin_video_open(struct tcc_hdin_device *vdev);
extern int 	hdin_video_close(struct file *file);
extern int 	hdin_video_init(struct tcc_hdin_device *vdev);
extern void hdin_video_set_addr(struct tcc_hdin_device *vdev, int index, unsigned int addr);

#endif
