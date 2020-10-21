// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCCVIN_VIDEO_H_
#define _TCCVIN_VIDEO_H_

#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/of_device.h>
#include <linux/videodev2.h>
#include <media/media-device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-v4l2.h>

// vioc path
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

/* ------------------------------------------------------------------------
 * Driver specific constants.
 */
#define DRIVER_NAME			"tccvin"
#define DRIVER_VERSION			"2.0.0"

// vioc path
#define MAX_BUFFERS			4

#define PGL_FORMAT			(VIOC_IMG_FMT_ARGB8888)
#define PGL_BG_R			(0xff)
#define PGL_BG_G			(0xff)
#define PGL_BG_B			(0xff)
#define PGL_BGM_R			(0xff)
#define PGL_BGM_G			(0xff)
#define PGL_BGM_B			(0xff)

// video-input path
#define TCCVIN_CTRL_CONTROL_TIMEOUT	500
#define TCCVIN_CTRL_STREAMING_TIMEOUT	5000

/* ------------------------------------------------------------------------
 * Structures.
 */

struct tccvin_device;

typedef struct vioc_path {
	int				vin;
	int				viqe;
	int				deintl_s;
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
	unsigned long			addr0;
	unsigned long			addr1;
	unsigned long			addr2;
} buf_addr_t;

typedef struct tccvin_cif {
	// cif port
	unsigned int			cif_port;
	volatile void __iomem		* cifport_addr;

	// vioc
	struct clk			* vioc_clk;
	vioc_path_t 			vioc_path;

	unsigned int			vioc_irq_reg;
	unsigned int			vioc_irq_num;
	struct vioc_intr_type		vioc_intr;

	// usage status pgl
	unsigned int				use_pgl;

	// optional pmap
	struct pmap					pmap_pgl;
	struct pmap					pmap_viqe;
    struct pmap                      pmap_preview;

	// framebuffer
	buf_addr_t			* preview_buf_addr;
	void __iomem			* vir;

	unsigned int			skip_frame;

	struct work_struct		wdma_work;

	struct mutex			lock;
} tccvin_cif_t;

typedef enum preview_method {
	PREVIEW_V4L2	= 0,
	PREVIEW_DD,
} preview_method_t;

struct tccvin_format_desc {
	char				*name;
	__u32				guid;
	__u32				fcc;
	__u8				bpp;
};

/* The term 'entity' refers to both TCCVIN units and TCCVIN terminals.
 *
 * The type field is either the terminal type (wTerminalType in the terminal
 * descriptor), or the unit type (bDescriptorSubtype in the unit descriptor).
 * As the bDescriptorSubtype field is one byte long, the type value will
 * always have a null MSB for units. All terminal types defined by the TCCVIN
 * specification have a non-null MSB, so it is safe to use the MSB to
 * differentiate between units and terminals as long as the descriptor parsing
 * code makes sure terminal types have a non-null MSB.
 *
 * For terminals, the type's most significant bit stores the terminal
 * direction (either TCCVIN_TERM_INPUT or TCCVIN_TERM_OUTPUT). The type field should
 * always be accessed with the TCCVIN_ENTITY_* macros and never directly.
 */

struct tccvin_vs_sync_info {
	/* VIN_CTRL */
	unsigned int		data_order;		// data order for vin
	unsigned int		data_format;		// data format for vin
	unsigned int		stream_enable;
	unsigned int		gen_field_en;
	unsigned int		de_active_low;
	unsigned int		pclk_polarity;
	unsigned int		field_bfield_low;
	unsigned int		vs_mask;
	unsigned int		hsde_connect_en;
	unsigned int		intpl_en;
	unsigned int		interlaced;
	unsigned int		conv_en;		// OFF: BT.601 / ON: BT.656

	/* VIN_MISC */
	unsigned int		flush_vsync;
};

struct tccvin_entity {
	/* Media controller-related fields. */
	struct video_device		*vdev;
	struct v4l2_subdev		subdev;
};

struct framesize {
	unsigned int			width;
	unsigned int			height;
};

struct tccvin_frame {
	__u8  bFrameIndex;
	__u8  bmCapabilities;
	__u16 wWidth;
	__u16 wHeight;
	__u32 dwMaxVideoFrameBufferSize;	// sizeimage
	__u8  bFrameIntervalType;
	__u32 dwDefaultFrameInterval;
	__u32 *dwFrameInterval;
};

struct tccvin_format {
	__u8 index;
	__u8 bpp;
	__u8 colorspace;
	__u32 fcc;
	__u32 flags;

	char name[32];

	unsigned int nframes;
	struct tccvin_frame *frame;
};

enum tccvin_buffer_state {
	TCCVIN_BUF_STATE_IDLE	= 0,
	TCCVIN_BUF_STATE_QUEUED	= 1,
	TCCVIN_BUF_STATE_ACTIVE	= 2,
	TCCVIN_BUF_STATE_READY	= 3,
	TCCVIN_BUF_STATE_DONE	= 4,
	TCCVIN_BUF_STATE_ERROR	= 5,
};

struct tccvin_buffer {
	struct vb2_v4l2_buffer buf;
	struct list_head queue;

	enum tccvin_buffer_state state;
	unsigned int error;

	void *mem;
	unsigned int length;
	unsigned int bytesused;
};

#define TCCVIN_QUEUE_DISCONNECTED		(1 << 0)
#define TCCVIN_QUEUE_DROP_CORRUPTED		(1 << 1)

struct tccvin_video_queue {
	struct vb2_queue queue;
	struct mutex mutex;			/* Protects queue */

	unsigned int flags;
	unsigned int buf_used;

	spinlock_t irqlock;			/* Protects irqqueue */
	struct list_head irqqueue;
};

struct tccvin_streaming {
	struct tccvin_device			*dev;
	struct video_device			vdev;
	atomic_t				active;

	struct v4l2_dv_timings			dv_timings;
	struct tccvin_vs_sync_info		vs_sync_info;
	enum v4l2_buf_type			type;

	unsigned int				nformats;
	struct tccvin_format			*format;

	struct tccvin_format			*def_format;
	struct tccvin_format			*cur_format;
	struct tccvin_frame			*cur_frame;

	/* Protect access to ctrl, cur_format, cur_frame and hardware video
	 * probe control.
	 */
	struct mutex				mutex;

	/* Buffers queue. */
	unsigned int				frozen : 1;
	struct tccvin_video_queue		queue;

	__u32 sequence;

	// video-input path device
	struct tccvin_cif			cif;

	int					preview_method;
	int					is_handover_needed;
	int					cam_streaming;
};

#define TCCVIN_MAX_VIDEOSOURCE			4

struct tccvin_device {
	struct platform_device			*pdev;
	char					name[32];

	struct mutex				lock;		/* Protects users */
	unsigned int				users;

	/* Video control interface */
	struct v4l2_device			vdev;

	int					bounded_subdevs;
	int					current_subdev_idx;

        struct v4l2_fwnode_endpoint		fw_ep[TCCVIN_MAX_VIDEOSOURCE];
	struct v4l2_async_subdev		*asd[TCCVIN_MAX_VIDEOSOURCE];
	struct v4l2_subdev			*subdevs[TCCVIN_MAX_VIDEOSOURCE];
	struct v4l2_async_notifier		notifier;

	struct list_head			entities;

	/* Video Streaming interfaces */
	struct tccvin_streaming			*stream;
	struct kref				ref;
};

enum tccvin_handle_state {
	TCCVIN_HANDLE_PASSIVE	= 0,
	TCCVIN_HANDLE_ACTIVE	= 1,
};

struct tccvin_fh {
	struct v4l2_fh vfh;
	struct tccvin_streaming *stream;
	enum tccvin_handle_state state;
};

/* ------------------------------------------------------------------------
 * Debugging, printing and logging
 */

extern unsigned int tccvin_no_drop_param;
extern unsigned int tccvin_timeout_param;

#define LOG_TAG			"VIN"

#define loge(fmt, ...)		pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logw(fmt, ...)		pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logd(fmt, ...)		pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logi(fmt, ...)		pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define dlog(fmt, ...)

/* --------------------------------------------------------------------------
 * Internal functions.
 */

/* Video buffers queue management. */
extern int tccvin_queue_init(struct tccvin_video_queue *queue, enum v4l2_buf_type type, int drop_corrupted);
extern void tccvin_queue_release(struct tccvin_video_queue *queue);
extern int tccvin_request_buffers(struct tccvin_video_queue *queue, struct v4l2_requestbuffers *rb);
extern int tccvin_query_buffer(struct tccvin_video_queue *queue, struct v4l2_buffer *v4l2_buf);
extern int tccvin_queue_buffer(struct tccvin_video_queue *queue, struct v4l2_buffer *v4l2_buf);
extern int tccvin_dequeue_buffer(struct tccvin_video_queue *queue, struct v4l2_buffer *v4l2_buf, int nonblocking);
extern int tccvin_queue_streamon(struct tccvin_video_queue *queue, enum v4l2_buf_type type);
extern int tccvin_queue_streamoff(struct tccvin_video_queue *queue, enum v4l2_buf_type type);
extern struct tccvin_buffer *tccvin_queue_next_buffer(struct tccvin_video_queue *queue, struct tccvin_buffer *buf);
extern int tccvin_queue_mmap(struct tccvin_video_queue *queue, struct vm_area_struct *vma);
extern unsigned int tccvin_queue_poll(struct tccvin_video_queue *queue, struct file *file, poll_table *wait);
extern int tccvin_queue_is_allocated(struct tccvin_video_queue *queue);
static inline int tccvin_queue_is_streaming(struct tccvin_video_queue *queue)
{
	return vb2_is_streaming(&queue->queue);
}

/* V4L2 interface */
extern const struct v4l2_ioctl_ops tccvin_ioctl_ops;
extern const struct v4l2_file_operations tccvin_fops;

/* Video */
extern struct tccvin_format_desc tccvin_fmts[];
extern int tccvin_format_num(void);
extern struct tccvin_format_desc *tccvin_format_by_guid(const __u32 guid);
extern int tccvin_video_init(struct tccvin_streaming *stream);
extern int tccvin_video_deinit(struct tccvin_streaming *stream);
extern int tccvin_video_streamon(struct tccvin_streaming *stream, int is_handover_needed);
extern int tccvin_video_streamoff(struct tccvin_streaming *stream, int is_handover_needed);

/* Utility functions */
extern void tccvin_simplify_fraction(uint32_t *numerator, uint32_t *denominator, unsigned int n_terms, unsigned int threshold);

#endif
