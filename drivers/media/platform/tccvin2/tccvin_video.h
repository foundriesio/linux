/* SPDX-License-Identifier: GPL-2.0 */

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

/* vioc path */
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
#include <video/tcc/tcc_cam_ioctrl.h>

/* reserved memory */
#include <linux/ioport.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/mutex.h>

/* ------------------------------------------------------------------------
 * Driver specific constants.
 */
#define DRIVER_NAME			"tccvin"
#define DRIVER_VERSION			"2.0.0"

/* vioc path */
#define MAX_BUFFERS			(4)
#ifdef	VIDEO_MAX_PLANES
#undef	VIDEO_MAX_PLANES
#define	VIDEO_MAX_PLANES		(3)
#endif

#define	MAX_FRAMEWIDTH			(8192)
#define	MAX_FRAMEHEIGHT			(8192)

#define PGL_FORMAT			(VIOC_IMG_FMT_ARGB8888)
#define PGL_BG_R			(0xff)
#define PGL_BG_G			(0xff)
#define PGL_BG_B			(0xff)
#define PGL_BGM_R			(0xff)
#define PGL_BGM_G			(0xff)
#define PGL_BGM_B			(0xff)

/* video-input path */
#define TCCVIN_CTRL_CONTROL_TIMEOUT	500
#define TCCVIN_CTRL_STREAMING_TIMEOUT	5000

/* ------------------------------------------------------------------------
 * Structures.
 */

struct tccvin_device;

struct vioc_path {
	int32_t				vin;
	int32_t				viqe;
	int32_t				deintl_s;
	int32_t				scaler;
	int32_t				pgl;
	int32_t				wmixer;
	int32_t				wdma;
	int32_t				fifo;
	int32_t				rdma;
	int32_t				wmixer_out;
	int32_t				disp;
};

struct buf_addr {
	unsigned long			addr0;
	unsigned long			addr1;
	unsigned long			addr2;
};

struct tccvin_cif {
	/* cif port */
	int32_t				cif_port;

	void __iomem			*cifport_addr;

	/* vioc */
	struct clk			*vioc_clk;
	struct vioc_path		vioc_path;

	unsigned int			vioc_irq_reg;
	unsigned int			vioc_irq_num;
	struct vioc_intr_type		vioc_intr;

	/* usage status pgl */
	unsigned int			use_pgl;

	/* optional pmap */
	struct resource			pmap_pgl;
	struct resource			pmap_viqe;
	struct resource			pmap_prev;

	/* framebuffer */
	struct buf_addr			preview_buf_addr[VB2_MAX_FRAME];

	unsigned int			skip_frame;

	struct work_struct		wdma_work;

	struct mutex			lock;

	atomic_t			recovery_trigger;
};

enum preview_method {
	PREVIEW_V4L2	= 0,
	PREVIEW_DD,
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
 * direction (either TCCVIN_TERM_INPUT or TCCVIN_TERM_OUTPUT). The type field
 * should always be accessed with the TCCVIN_ENTITY_* macros and never directly.
 */

struct tccvin_vs_info {
	/* VIN_CTRL */
	unsigned int		data_order;		// [22:20]
	unsigned int		data_format;		// [19:16]
	unsigned int		stream_enable;		// [14]
	unsigned int		gen_field_en;		// [13]
	unsigned int		de_low;			// [12]
	unsigned int		field_low;		// [11]
	unsigned int		vs_low;			// [10]
	unsigned int		hs_low;			// [ 9]
	unsigned int		pclk_polarity;		// [ 8]
	unsigned int		vs_mask;		// [ 6]
	unsigned int		hsde_connect_en;	// [ 4]
	unsigned int		intpl_en;		// [ 3]
	unsigned int		interlaced;		// [ 2]
	unsigned int		conv_en;		// [ 1]

	/* VIN_MISC */
	unsigned int		flush_vsync;		// [16]

	/* VIN_SIZE */
	unsigned int		height;			// [31:16]
	unsigned int		width;			// [15: 0]
};

struct tccvin_entity {
	/* Media controller-related fields. */
	struct video_device		*vdev;
	struct v4l2_subdev		subdev;
};

struct tccvin_frame {
	unsigned int			width;
	unsigned int			height;
};

struct tccvin_format {
	__u8 index;
	__u8 bpp;
	__u8 colorspace;
	__u32 fcc;
	__s32 mbus_code;
	__u32 num_planes;
	__u32 flags;
	__u32 guid;

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
	struct tccvin_vs_info			vs_info;
	struct v4l2_subdev_mbus_code_enum	mbus_code;
	struct v4l2_mbus_config			mbus_config;
	enum v4l2_buf_type			type;

	struct v4l2_rect			rect_bound;
	struct v4l2_rect			rect_crop;
	struct v4l2_rect			rect_compose;

	struct tccvin_frame 			frame;
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
	struct tccvin_buffer			*prev_buf;
	struct tccvin_buffer			*next_buf;

	__u32 sequence;

	/* video-input path device */
	struct tccvin_cif			cif;

	int					preview_method;
	int					is_handover_needed;

	atomic_t				timestamp;
	struct timespec				ts_prev;
	struct timespec				ts_next;
	struct timespec				ts_diff;

	int					skip_frame_cnt;
};

#define TCCVIN_MAX_VIDEOSOURCE 255

struct tccvin_subdev {
	struct v4l2_async_subdev asd;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_format fmt;
};

struct tccvin_device {
	struct platform_device		*pdev;
	char				name[32];

	struct mutex			lock;
	unsigned int			users;

	/* Video control interface */
	struct v4l2_device		vdev;

	int				bounded_subdevs;
	int				current_subdev_idx;

	struct v4l2_fwnode_endpoint	fw_ep[TCCVIN_MAX_VIDEOSOURCE];
	int				num_ep;

	struct v4l2_async_subdev	*async_subdevs[TCCVIN_MAX_VIDEOSOURCE];
	struct tccvin_subdev		linked_subdevs[TCCVIN_MAX_VIDEOSOURCE];
	/* struct v4l2_subdev		*subdevs[TCCVIN_MAX_VIDEOSOURCE]; */
	int				num_asd;
	struct v4l2_async_notifier	notifier;

	struct list_head		entities;

	/* Video Streaming interfaces */
	struct tccvin_streaming		*stream;
	struct kref			ref;
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

#define loge(fmt, ...)		{ pr_err("[ERROR][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
#define logw(fmt, ...)		{ pr_warn("[WARN][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
#define logd(fmt, ...)		{ pr_debug("[DEBUG][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
#define logi(fmt, ...)		{ pr_info("[INFO][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }

/* --------------------------------------------------------------------------
 * Internal functions.
 */

extern struct tccvin_frame cur_frame;

/* Video buffers queue management. */
extern int tccvin_queue_init(struct tccvin_video_queue *queue,
	enum v4l2_buf_type type, int drop_corrupted);
extern void tccvin_queue_release(struct tccvin_video_queue *queue);
extern int tccvin_get_imagesize(unsigned int width, unsigned int height,
	unsigned int fcc, unsigned int (*planes)[]);
extern int tccvin_request_buffers(struct tccvin_video_queue *queue,
	struct v4l2_requestbuffers *rb);
extern int tccvin_query_buffer(struct tccvin_video_queue *queue,
	struct v4l2_buffer *v4l2_buf);
extern int tccvin_queue_buffer(struct tccvin_video_queue *queue,
	struct v4l2_buffer *v4l2_buf);
extern int tccvin_export_buffer(struct tccvin_video_queue *queue,
	struct v4l2_exportbuffer *exp);
extern int tccvin_dequeue_buffer(struct tccvin_video_queue *queue,
	struct v4l2_buffer *v4l2_buf, int nonblocking);
extern int tccvin_conv_to_paddr(struct tccvin_video_queue *queue,
	struct v4l2_buffer *buf);
extern int tccvin_queue_streamon(struct tccvin_video_queue *queue,
	enum v4l2_buf_type type);
extern int tccvin_queue_streamoff(struct tccvin_video_queue *queue,
	enum v4l2_buf_type type);
extern struct tccvin_buffer *
tccvin_queue_next_buffer(struct tccvin_video_queue *queue,
	struct tccvin_buffer *buf);
extern int tccvin_queue_mmap(struct tccvin_video_queue *queue,
	struct vm_area_struct *vma);
extern unsigned int tccvin_queue_poll(struct tccvin_video_queue *queue,
	struct file *file, poll_table *wait);
extern int tccvin_queue_is_allocated(struct tccvin_video_queue *queue);
static inline int tccvin_queue_is_streaming(struct tccvin_video_queue *queue)
{
	return vb2_is_streaming(&queue->queue);
}

/* V4L2 interface */
extern const struct v4l2_ioctl_ops tccvin_ioctl_ops;
extern const struct v4l2_file_operations tccvin_fops;

/* Video */
extern int tccvin_create_recovery_trigger(struct device *dev);
extern int tccvin_create_timestamp(struct device *dev);
extern unsigned int tccvin_count_supported_formats(void);
extern struct tccvin_format *tccvin_format_by_index(int index);
extern struct tccvin_format *tccvin_format_by_fcc(const __u32 fcc);
extern int32_t tccvin_video_subdevs_set_fmt(struct tccvin_streaming *stream);
extern int32_t tccvin_video_subdevs_get_config(struct tccvin_streaming *stream);
extern void tccvin_get_default_rect(struct tccvin_streaming *stream,
	struct v4l2_rect *rect, __u32 type);
extern int tccvin_video_init(struct tccvin_streaming *stream);
extern int tccvin_video_deinit(struct tccvin_streaming *stream);
extern int tccvin_video_streamon(struct tccvin_streaming *stream);
extern int tccvin_video_streamoff(struct tccvin_streaming *stream);
extern void tccvin_check_path_status(struct tccvin_streaming *stream,
	int *status);
extern int32_t tccvin_s_handover(struct tccvin_streaming *stream,
	int32_t *is_handover_needed);
#endif
