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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mm.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

#include <video/tcc/tcc_vout_ioctl.h>
#include <video/tcc/tcc_mem_ioctl.h>

#include "tcc_vout.h"
#include "tcc_vout_core.h"
#include "tcc_vout_dbg.h"
#include "tcc_vout_attr.h"

#define TO_KERNEL_SPACE		1
#define TO_USER_SPACE		0

#ifdef CONFIG_OF
static struct of_device_id tccvout_of_match[] = {
	{ .compatible = "telechips,v4l2_vout" },
	{}
};
MODULE_DEVICE_TABLE(of, tccvout_of_match);
#endif

static void *tcc_vout[VOUT_MAX_CH];


/* list of image formats supported by tcc video pipelines */
const struct v4l2_fmtdesc tcc_fmtdesc[] = {
	/* .reserved[0] : vioc image format */
/* YUV formats */
	{
		.description = "'YU12' 12 YUV 4:2:0",
		.pixelformat = V4L2_PIX_FMT_YUV420,
		.reserved[0] = VIOC_IMG_FMT_YUV420SEP,
	},
	{
		.description = "'YV12' 12 YVU 4:2:0",
		.pixelformat = V4L2_PIX_FMT_YVU420,
		.reserved[0] = VIOC_IMG_FMT_YUV420SEP,
	},
	{
		.description = "'422P' 16 YUV422 planar",
		.pixelformat = V4L2_PIX_FMT_YUV422P,
		.reserved[0] = VIOC_IMG_FMT_YUV422SEP,
	},
	{
		.description = "'NV12' 12 Y/CbCr 4:2:0",
		.pixelformat = V4L2_PIX_FMT_NV12,
		.reserved[0] = VIOC_IMG_FMT_YUV420IL0,
	},
/* RGB formats */
	{
		/* R[7:5] G[4:2] B[1:0] */
		.description = "'RGB1' 8 RGB-3-3-2",
		.pixelformat = V4L2_PIX_FMT_RGB332,
		.reserved[0] = VIOC_IMG_FMT_RGB332,
	},
	{
		/* xxxxrrrr ggggbbbb */
		/* A[15:12] R[11:8] G[7:4] B[3:0] */
		.description = "'R444' 16 RGB-4-4-4",
		.pixelformat = V4L2_PIX_FMT_RGB444,
		.reserved[0] = VIOC_IMG_FMT_ARGB4444,
	},
	{
		/* A[15] R[14:10] G[9:5] B[4:0] */
		.description = "'RGB0' 16 RGB-5-5-5",
		.pixelformat = V4L2_PIX_FMT_RGB555,
		.reserved[0] = VIOC_IMG_FMT_ARGB1555,
	},
	{
		/* R[15:11] G[10:5] B[4:0] */
		.description = "'RGBP' 16 RGB-5-6-5",
		.pixelformat = V4L2_PIX_FMT_RGB565,
		.reserved[0] = VIOC_IMG_FMT_RGB565,
	},
	{
		/* B1[31:24] R[23:16] G[15:8] B0[7:0] */
		.description = "'RBG3' 24 RGB-8-8-8",
		.pixelformat = V4L2_PIX_FMT_RGB24,
		.reserved[0] = VIOC_IMG_FMT_RGB888,
	},
	{
		/* RGB swap -> BGR */
		.description = "'BGR3' 24 BGR-8-8-8",
		.pixelformat = V4L2_PIX_FMT_BGR24,
		.reserved[0] = VIOC_IMG_FMT_RGB888,
	},
	{
		/* A[31:24] R[23:16] G[15:8] B[7:0] */
		.description = "'RGB4' 32 RGB-8-8-8-8",
		.pixelformat = V4L2_PIX_FMT_RGB32,
		.reserved[0] = VIOC_IMG_FMT_ARGB8888,
	},
	{
		/* RGB swap -> BGR */
		.description = "'BGR4' 32 BGR-8-8-8-8",
		.pixelformat = V4L2_PIX_FMT_BGR32,
		.reserved[0] = VIOC_IMG_FMT_ARGB8888,
	},
};
#define NUM_TCC_FMTDESC (ARRAY_SIZE(tcc_fmtdesc))

/*
 * export symbol
 */
void tcc_vout_disable_hwlayer(unsigned int type)
{
	struct tcc_vout_device *vout =
		(struct tcc_vout_device *)tcc_vout[get_vioc_index(type)];

	if (vout == NULL)
		return;

	if (vout->status == TCC_VOUT_RUNNING) {
		struct tcc_vout_vioc *vioc = vout->vioc;

		if (vout->onthefly_mode)
			VIOC_RDMA_SetImageDisable(vioc->rdma.addr);
		else
			VIOC_RDMA_SetImageDisableNW(vioc->rdma.addr);
		dprintk("RDMA%d(0x%p) is disabled by force !!!\n",
			vioc->rdma.id, vioc->rdma.addr);
	}
}
EXPORT_SYMBOL(tcc_vout_disable_hwlayer);

int tcc_vout_get_status(unsigned int type)
{
	struct tcc_vout_device *vout =
		(struct tcc_vout_device *)tcc_vout[get_vioc_index(type)];
	int ret = -1;

	if (vout)
		ret = vout->status;
	return ret;
}
EXPORT_SYMBOL(tcc_vout_get_status);

#ifdef CONFIG_VOUT_USE_VSYNC_INT
void tcc_vout_hdmi_end(unsigned int type)
{
	struct tcc_vout_device *vout =
		(struct tcc_vout_device *)tcc_vout[get_vioc_index(type)];
	if (vout->status == TCC_VOUT_RUNNING) {
		pr_info("[INF][VOUT] vsync interrupt disable\n");

		vout_intr_onoff(0, vout);
		vout_ktimer_ctrl(vout, ON);
	}
}
EXPORT_SYMBOL(tcc_vout_hdmi_end);

void tcc_vout_hdmi_start(unsigned int type)
{
	struct tcc_vout_device *vout =
		(struct tcc_vout_device *)tcc_vout[get_vioc_index(type)];
	if (vout->status == TCC_VOUT_RUNNING) {
		pr_info("[INF][VOUT] vsync interrupt enable\n");

		vout_ktimer_ctrl(vout, OFF);
		vout_intr_onoff(1, vout);
	}
}
EXPORT_SYMBOL(tcc_vout_hdmi_start);
#endif

static int tcc_vout_try_fmt(unsigned int pixelformat)
{
	int ifmt;

	for (ifmt = 0; ifmt < NUM_TCC_FMTDESC; ifmt++) {
		if (tcc_fmtdesc[ifmt].pixelformat == pixelformat) {
			return ifmt;
		}
	}

	return ifmt;
}

int tcc_vout_try_bpp(unsigned int pixelformat, enum v4l2_colorspace *colorspace)
{
	int bpp, color;

	switch (pixelformat) {
	/* RGB formats */
	case V4L2_PIX_FMT_RGB332:
		color = V4L2_COLORSPACE_SRGB;
		bpp = 1;
		break;
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_RGB565X:
		color = V4L2_COLORSPACE_SRGB;
		bpp = 2;
		break;
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		color = V4L2_COLORSPACE_SRGB;
		bpp = 3;
		break;
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_RGB32:
		color = V4L2_COLORSPACE_SRGB;
		bpp = 4;
		break;

	/* YUV formats */
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_HM12:
	case V4L2_PIX_FMT_M420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12MT:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		color = V4L2_COLORSPACE_JPEG;
		bpp = 1;
		break;

	/* YUV422 Sequential formats */
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YYUV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		color = V4L2_COLORSPACE_JPEG;
		bpp = 2;
		break;

	default:
		pr_err("[ERR][VOUT] Not supported pixelformat(%c%c%c%c)\n",
			fourcc2char(pixelformat));
		color = V4L2_COLORSPACE_JPEG;
		bpp = 2;
		break;
	}

	if (colorspace != NULL)
		*colorspace = color;

	return bpp;
}

int tcc_vout_try_pix(unsigned int pixelformat)
{
	enum tcc_pix_fmt pfmt;

	switch (pixelformat) {
	/* YUV420 formats */
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_HM12:
	case V4L2_PIX_FMT_M420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12MT:
	case V4L2_PIX_FMT_YUV420M:
		pfmt = TCC_PFMT_YUV420;
		break;
	/* YUV422 formats */
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YYUV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		pfmt = TCC_PFMT_YUV422;
		break;
	/* RGB formats */
	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_RGB32:
		pfmt = TCC_PFMT_RGB;
		break;
	default:
		pr_err("[ERR][VOUT] Not supported pixelformat(%c%c%c%c)\n",
			fourcc2char(pixelformat));
		pfmt = TCC_PFMT_YUV420;
		break;
	}

	return pfmt;
}

static int tcc_vout_buffer_copy(struct v4l2_buffer *dst,
	struct v4l2_buffer *src, int mode)
{
	int ret = 0;
	/* remove previous frame info  */
	memset(dst->m.planes, 0x0, sizeof(struct v4l2_plane) * MPLANE_NUM);

	/* copy 'v4l2_buffer' structure */
	dst->index = src->index;
	dst->type = src->type;
	dst->bytesused = src->bytesused;
	dst->flags = src->flags;
	dst->field = src->field;
	dst->timestamp = src->timestamp;
	dst->timecode = src->timecode;
	dst->sequence = src->sequence;
	dst->memory = src->memory;
	dst->length = src->length;
	dst->reserved2 = src->reserved2;
	dst->reserved = src->reserved;

	if (mode) {
		ret = copy_from_user((void *)dst->m.planes,
			(const void *)src->m.planes,
			sizeof(struct v4l2_plane) * MPLANE_NUM);
	} else {
		ret = copy_to_user((void *)dst->m.planes,
			(const void *)src->m.planes,
			sizeof(struct v4l2_plane) * MPLANE_NUM);
	}

	return ret;
}

static int tcc_v4l2_buffer_set(struct tcc_vout_device *vout,
	struct v4l2_requestbuffers *req)
{
	struct tcc_v4l2_buffer *b = vout->qbufs;
	unsigned int index, y_offset = 0, uv_offset = 0, total_offset = 0;

	/* for sub_plane */
	for (index = vout->nr_qbufs;
		index < (vout->nr_qbufs + req->count); index++) {
		b[index].buf.m.planes = kzalloc(
			sizeof(struct v4l2_plane) * MPLANE_NUM, GFP_KERNEL);
	}

	if (req->memory == V4L2_MEMORY_USERPTR) {
		for (index = vout->nr_qbufs;
			index < (vout->nr_qbufs + req->count); index++) {
			b[index].index = index;
		}

		/* update current number of created buffer */
		vout->nr_qbufs += req->count;

		/*
		 * We don't have to set v4l2 bufs.
		 */
		return 0;
	}

	switch (vout->pfmt) {
	case TCC_PFMT_YUV422:
		y_offset = vout->src_pix.width * vout->src_pix.height;
		uv_offset = y_offset / 2;
		break;
	case TCC_PFMT_YUV420:
		y_offset = vout->src_pix.width * vout->src_pix.height;
		uv_offset = y_offset / 4;
		break;
	case TCC_PFMT_RGB:
	default:
		break;
	}
	total_offset = PAGE_ALIGN(y_offset + uv_offset * 2);

	/* set base address each buffers
	 */
	dprintk("  alloc_buf    img_base0   img_base1   img_base2\n");
	for (index = vout->nr_qbufs;
		index < (vout->nr_qbufs + req->count); index++) {
		b[index].index = index;

		switch (vout->src_pix.pixelformat) {
		/* YCbCr 4:2:0 separated (VIOC_IMG_FMT_YUV420SEP) */
		/* YCbCr 4:2:0 separated (VIOC_IMG_FMT_YUV420SEP) */
		/* YCbCr 4:2:2 separated (VIOC_IMG_FMT_YUV422SEP) */
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_YVU420:
		case V4L2_PIX_FMT_YUV422P:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = b[index].img_base0 + y_offset;
			b[index].img_base2 = b[index].img_base1 + uv_offset;
			break;

		/* YCbCr 4:2:0 interleaved type0 (VIOC_IMG_FMT_YUV420IL0) */
		/* YCbCr 4:2:0 interleaved type1 (VIOC_IMG_FMT_YUV420IL1) */
		/* YCbCr 4:2:2 interleaved type0 (VIOC_IMG_FMT_YUV422IL0) */
		/* YCbCr 4:2:2 interleaved type1 (VIOC_IMG_FMT_YUV422IL1) */
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = b[index].img_base0 + y_offset;
			b[index].img_base2 = 0;
			break;

		/* YCbCr 4:2:2 UYVY Sequential (VIOC_IMG_FMT_UYVY) */
		/* YCbCr 4:2:2 VYUY Sequential (VIOC_IMG_FMT_VYUY) */
		/* YCbCr 4:2:2 YUYV Sequential (VIOC_IMG_FMT_YUYV) */
		/* YCbCr 4:2:2 YVYU Sequential (VIOC_IMG_FMT_YVYU) */
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_VYUY:
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_YVYU:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = 0;
			b[index].img_base2 = 0;
			break;

		/* RGB and etc... */
		default:
			b[index].img_base0 = vout->pmap.base +
				(vout->src_pix.sizeimage * index);
			b[index].img_base1 = 0;
			b[index].img_base2 = 0;
		}

		dprintk("     [%02d]     0x%08x  0x%08x  0x%08x\n",
			index, b[index].img_base0,
			b[index].img_base1, b[index].img_base2);
	}
	dprintk("-----------------------------------------------------\n");

	// update current number of created buffer
	vout->nr_qbufs += req->count;

	print_v4l2_reqbufs_format(vout->pfmt, vout->src_pix.pixelformat,
		"tcc_v4l2_buffer_set");
	return 0;
}

#if 0
static int tcc_v4l2_buffer_add(struct tcc_vout_device *vout,
	struct v4l2_create_buffers *create)
{
	struct tcc_v4l2_buffer *b = vout->qbufs;
	unsigned int index, y_offset = 0, uv_offset = 0, total_offset = 0;

	/* for sub_plane */
	for (index = vout->nr_qbufs;
		index < (vout->nr_qbufs + create->count); index++) {
		b[index].buf.m.planes = kzalloc(
			sizeof(struct v4l2_plane) * MPLANE_NUM, GFP_KERNEL);
	}

	if (create->memory == V4L2_MEMORY_USERPTR) {
		for (index = vout->nr_qbufs;
			index < (vout->nr_qbufs + create->count); index++) {
			b[index].index = index;
		}

		/* update current number of created buffer */
		vout->nr_qbufs += create->count;

		/*
		 * We don't have to set v4l2 bufs.
		 */
		return 0;
	}

	switch (vout->pfmt) {
	case TCC_PFMT_YUV422:
		y_offset = vout->src_pix.width * vout->src_pix.height;
		uv_offset = y_offset / 2;
		break;
	case TCC_PFMT_YUV420:
		y_offset = vout->src_pix.width * vout->src_pix.height;
		uv_offset = y_offset / 4;
		break;
	case TCC_PFMT_RGB:
	default:
		break;
	}
	total_offset = PAGE_ALIGN(y_offset + uv_offset * 2);

	/* set base address each buffers
	 */
	dprintk("  alloc_buf    img_base0   img_base1   img_base2\n");
	for (index = vout->nr_qbufs;
		index < (vout->nr_qbufs + create->count); index++) {
		b[index].index = index;

		switch (vout->src_pix.pixelformat) {

		/* YCbCr 4:2:0 separated (VIOC_IMG_FMT_YUV420SEP) */
		/* YCbCr 4:2:0 separated (VIOC_IMG_FMT_YUV420SEP) */
		/* YCbCr 4:2:2 separated (VIOC_IMG_FMT_YUV422SEP) */
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_YVU420:
		case V4L2_PIX_FMT_YUV422P:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = b[index].img_base0 + y_offset;
			b[index].img_base2 = b[index].img_base1 + uv_offset;
			break;

		/* YCbCr 4:2:0 interleaved type0 (VIOC_IMG_FMT_YUV420IL0) */
		/* YCbCr 4:2:0 interleaved type1 (VIOC_IMG_FMT_YUV420IL1) */
		/* YCbCr 4:2:2 interleaved type0 (VIOC_IMG_FMT_YUV422IL0) */
		/* YCbCr 4:2:2 interleaved type1 (VIOC_IMG_FMT_YUV422IL1) */
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = b[index].img_base0 + y_offset;
			b[index].img_base2 = 0;
			break;

		/* YCbCr 4:2:2 UYVY Sequential (VIOC_IMG_FMT_UYVY) */
		/* YCbCr 4:2:2 VYUY Sequential (VIOC_IMG_FMT_VYUY) */
		/* YCbCr 4:2:2 YUYV Sequential (VIOC_IMG_FMT_YUYV) */
		/* YCbCr 4:2:2 YVYU Sequential (VIOC_IMG_FMT_YVYU) */
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_VYUY:
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_YVYU:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = 0;
			b[index].img_base2 = 0;
			break;

		/* RGB and etc... */
		default:
			b[index].img_base0 = vout->pmap.base +
				(vout->src_pix.sizeimage * index);
			b[index].img_base1 = 0;
			b[index].img_base2 = 0;
		}

		dprintk("     [%02d]     0x%08x  0x%08x  0x%08x\n",
			index, b[index].img_base0,
			b[index].img_base1, b[index].img_base2);
	}
	dprintk("-----------------------------------------------------\n");

	// update current number of created buffer
	vout->nr_qbufs += create->count;

	print_v4l2_reqbufs_format(vout->pfmt, vout->src_pix.pixelformat,
		"tcc_v4l2_buffer_set");

	return 0;
}
#endif

static int tcc_deintl_buffer_set(struct tcc_vout_device *vout)
{
	struct tcc_v4l2_buffer *b;
	unsigned int total_offset, y_offset = 0;
	unsigned int i;
	unsigned int actual_bufs;
	unsigned int width, height;

	/*
	 * CAUTION!!!:
	 *  deintl_buf (m2m_wdma buf) is always YUV420 Interleaved Type0.
	 */

	/* calculate buf count
	 * Size of display image can't be bigger than panel size
	 */
	if (vout->deintl_buf_size)
		actual_bufs = ((unsigned int)vout->deintl_pmap.size) / vout->deintl_buf_size;
	else
		actual_bufs = MIN_DEINTLBUF_NUM;

	if (actual_bufs < 2) {
		pr_err("[ERR][VOUT] deintl_nr_bufs is not enough (%d).\n",
			actual_bufs);
		return -ENOMEM;
	}
	dprintk("request deintl_nr_bufs(%d), actual buf(%d)\n",
		vout->deintl_nr_bufs, actual_bufs);
	if ((vout->deintl_nr_bufs > actual_bufs) || (vout->deintl_nr_bufs <= 0))
		vout->deintl_nr_bufs = actual_bufs;

	vout->deintl_bufs = kzalloc(sizeof(struct tcc_v4l2_buffer) *
		vout->deintl_nr_bufs, GFP_KERNEL);
	if (!vout->deintl_bufs)
		return -ENOMEM;
	b = vout->deintl_bufs;

	/* The deintl_buf_size is calculated by the panel size.
	 *  - 20190809 alanK
	 */
	#if 0
	width = vout->disp_rect.width;
	height = vout->disp_rect.height;
	#else
	if (vout->id == VOUT_MAIN) {
		#if defined(CONFIG_TCC_DUAL_DISPLAY)
			width = DEINTL_WIDTH;
			height = DEINTL_HEIGHT;
		#else
			vout_wmix_getsize(vout, &width, &height);
		#endif
	} else {
		width = vout->disp_rect.width;
		height = vout->disp_rect.height;
	}
	#endif

	y_offset = width * height;
	if (vout->vioc->m2m_wdma.fmt == VIOC_IMG_FMT_YUV420IL0)
		total_offset = PAGE_ALIGN(y_offset * 3 / 2);
	else
		total_offset = PAGE_ALIGN(y_offset * 2);

	/* set base address each buffers
	 */
	dprintk("  alloc_buf    img_base0   img_base1   img_base2\n");
	for (i = 0; i < vout->deintl_nr_bufs; i++) {
		b[i].index = i;
		b[i].img_base0 = vout->deintl_pmap.base + (total_offset * i);
		if (vout->vioc->m2m_wdma.fmt == VIOC_IMG_FMT_YUV420IL0)
			b[i].img_base1 = b[i].img_base0 + y_offset;
		else
			b[i].img_base1 = 0;
		b[i].img_base2 = 0;
		dprintk("     [%02d]     0x%08x  0x%08x  0x%08x\n",
			i, b[i].img_base0, b[i].img_base1, b[i].img_base2);
	}
	dprintk("-----------------------------------------------------\n");

	print_v4l2_reqbufs_format(vout->pfmt, vout->src_pix.pixelformat,
		"tcc_v4l2_buffer_set");
	return 0;
}

#if defined(CONFIG_TCC_DUAL_DISPLAY)
static int tcc_m2m_dual_buffer_set(struct tcc_vout_device *vout)
{
	struct tcc_v4l2_buffer *b, *bd;
	unsigned int total_offset, y_offset = 0;
	unsigned int i;
	unsigned int actual_bufs;
	unsigned int width, height;

	/* calculate buf count
	 * Size of display image can't be bigger than panel size
	 */
	if(vout->m2m_dual_buf_size)
		actual_bufs = ((unsigned int)vout->m2m_dual_pmap.size) / vout->m2m_dual_buf_size;
	else
		actual_bufs = MIN_DEINTLBUF_NUM;

	if (2 > actual_bufs) {
		pr_err("[ERR][VOUT] m2m_dual_nr_bufs is not enough (%d).\n", actual_bufs);
		return -ENOMEM;
	}

	dprintk("request m2m_dual_nr_bufs(%d), actual buf(%d)\n", vout->m2m_dual_nr_bufs, actual_bufs);
	if ((vout->m2m_dual_nr_bufs > actual_bufs) || (vout->m2m_dual_nr_bufs <= 0))
		vout->m2m_dual_nr_bufs = actual_bufs;

	vout->m2m_dual_bufs = kzalloc(sizeof(struct tcc_v4l2_buffer) * vout->m2m_dual_nr_bufs, GFP_KERNEL);
	if (!vout->m2m_dual_bufs)
		return -ENOMEM;
	b = vout->m2m_dual_bufs;

	vout->m2m_dual_bufs_hdmi = kzalloc(sizeof(struct tcc_v4l2_buffer) * vout->m2m_dual_nr_bufs, GFP_KERNEL);
	if (!vout->m2m_dual_bufs_hdmi)
		return -ENOMEM;
	bd = vout->m2m_dual_bufs_hdmi;

	if(vout->id == VOUT_MAIN) {
		width = DEINTL_WIDTH;
		height = DEINTL_HEIGHT;
	} else {
		width = vout->disp_rect.width;
		height = vout->disp_rect.height;
	}

	y_offset = width * height;
	if(vout->vioc->m2m_wdma.fmt == VIOC_IMG_FMT_YUV420IL0)
		total_offset = PAGE_ALIGN(y_offset * 3 / 2);
	else
		total_offset = PAGE_ALIGN(y_offset * 2);

	/* set base address each buffers
	 */
	dprintk("  alloc_buf    img_base0   img_base1   img_base2\n");
	for (i = 0; i < vout->m2m_dual_nr_bufs; i++) {
		b[i].index = i;
		bd[i].index = i;
		b[i].img_base0 = vout->m2m_dual_pmap.base + (total_offset * i);
		bd[i].img_base0 = vout->m2m_dual_pmap.base + (total_offset * (i+vout->m2m_dual_nr_bufs));

		if(vout->vioc->m2m_wdma.fmt == VIOC_IMG_FMT_YUV420IL0) {
			b[i].img_base1 = b[i].img_base0 + y_offset;
			bd[i].img_base1 = bd[i].img_base0 + y_offset;
		}
		else {
			b[i].img_base1 = 0;
			bd[i].img_base1 = 0;
		}
		b[i].img_base2 = 0;
		bd[i].img_base2 = 0;

		dprintk("   [%02d]     0x%08x  0x%08x  0x%08x\n", i, b[i].img_base0, b[i].img_base1, b[i].img_base2);
		dprintk("   [%02d]     0x%08x  0x%08x  0x%08x\n", i, bd[i].img_base0, bd[i].img_base1, bd[i].img_base2);
	}
	dprintk("-----------------------------------------------------\n");

	print_v4l2_reqbufs_format(vout->pfmt, vout->src_pix.pixelformat, "tcc_m2m_dual_buffer_set");
	return 0;
}
#endif

/*
 * V4L2 ioctls
 */

//TODO: gst_v4l2_fill_lists()
static int vidioc_enum_input(struct file *file, void *fh,
	struct v4l2_input *input)
{
	diprintk("\n");
	return -EINVAL;
}

static int vidioc_queryctrl(struct file *file, void *fh,
	struct v4l2_queryctrl *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ctrl->flags = V4L2_CTRL_FLAG_DISABLED;
		break;
	default:
		ctrl->name[0] = '\0';
		ret = -EINVAL;
		break;
	}

	diprintk("\n");
	return ret;
}

static int vidioc_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	int ret = 0;

	diprintk("v4l2_control id(0x%x)\n", ctrl->id);
	return ret;
}

static int vidioc_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	int ret = 0;

	diprintk("v4l2_control id(0x%x) value(0x%x)\n", ctrl->id, ctrl->value);
	return ret;
}

static int vidioc_querycap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	struct tcc_vout_device *vout = fh;

	strlcpy(cap->driver, VOUT_NAME, sizeof(cap->driver));
	snprintf(cap->card, sizeof(cap->card), "Telechips video output");
	snprintf(cap->bus_info, sizeof(cap->bus_info), "vout:%d", vout->id);
	cap->device_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_OUTPUT_OVERLAY;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	cap->version = VOUT_VERSION;

	diprintk("\n");
	#ifdef CONFIG_TCC_VOUT_DBG_INFO
	print_v4l2_capability(cap, "vidioc_querycap");
	#endif
	return 0;
}

static int vidioc_enum_fmt_vid_out(struct file *file, void *fh,
	struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;

	dprintk("\n");

	if (index >= NUM_TCC_FMTDESC) {
		return -EINVAL;
	}

	memcpy(fmt, &tcc_fmtdesc[index], sizeof(struct v4l2_fmtdesc));

	#ifdef CONFIG_TCC_VOUT_DBG_INFO
	print_v4l2_fmtdesc(fmt, "vidioc_enum_fmt_vid_out");
	#endif
	return 0;
}

static int vidioc_try_fmt_vid_out(struct file *file, void *fh,
	struct v4l2_format *f)
{
	int /*ifmt,*/ bpp, pfmt;
	unsigned int bytesperline;
	struct v4l2_pix_format *pix = &f->fmt.pix;

	dprintk("\n");

	//ifmt = tcc_vout_try_fmt(f->fmt.pix_mp.pixelformat);
	bpp = tcc_vout_try_bpp(pix->pixelformat, &pix->colorspace);
	pfmt = tcc_vout_try_pix(pix->pixelformat);
	bytesperline = pix->width * bpp;

	switch (pfmt) {
	case TCC_PFMT_YUV422:
		pix->sizeimage = pix->width * pix->height * 2;
		break;
	case TCC_PFMT_YUV420:
		pix->sizeimage = pix->width * pix->height * 3 / 2;
		break;
	case TCC_PFMT_RGB:
	default:
		pix->sizeimage = bytesperline * pix->height;
		break;
	}

	pix->bytesperline = bytesperline;

	#ifdef CONFIG_TCC_VOUT_DBG_INFO
	dprintk("pixelformat: %c%c%c%c\n", fourcc2char(pix->pixelformat));
	#endif
	return 0;
}

//static int vidioc_try_fmt_vid_out_mplane(struct file *file, void *fh,
//	struct v4l2_format *f)
//{
//	//int ifmt;
//	int bpp, pfmt;
//	unsigned int sizeimage;
//	unsigned short bytesperline;
//	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
//	unsigned int subtitle_w, subtitle_h, subtitle_fmt;
//
//	/* 1st plane (video) */
//	//ifmt = tcc_vout_try_fmt(f->fmt.pix_mp.pixelformat);
//	bpp = tcc_vout_try_bpp(pix_mp->pixelformat, &pix_mp->colorspace);
//	pfmt = tcc_vout_try_pix(pix_mp->pixelformat);
//	bytesperline = pix_mp->width * bpp;
//
//	switch (pfmt) {
//	case TCC_PFMT_YUV422:
//		sizeimage = pix_mp->width * pix_mp->height * 2;
//		break;
//	case TCC_PFMT_YUV420:
//		sizeimage = pix_mp->width * pix_mp->height * 3 / 2;
//		break;
//	case TCC_PFMT_RGB:
//	default:
//		sizeimage = bytesperline * pix_mp->height;
//		break;
//	}
//
//	pix_mp->plane_fmt[0].bytesperline = bytesperline;
//	pix_mp->plane_fmt[0].sizeimage = PAGE_ALIGN(sizeimage);
//
//	/* 2nd plnae (subtitle) */
//	/* Fill tcc subtitle information */
//	pix_mp->plane_fmt[1].reserved[0] = TCC_V4L2_OUTPUT_SRC_SUBTITLE;
//	subtitle_fmt = V4L2_PIX_FMT_RGB32;
//	subtitle_w = 1920;
//	subtitle_h = 1080;
//
//	if (pix_mp->plane_fmt[1].reserved[0] == TCC_V4L2_OUTPUT_SRC_SUBTITLE) {
//		bpp = tcc_vout_try_bpp(subtitle_fmt, NULL);
//		pfmt = tcc_vout_try_pix(subtitle_fmt);
//		bytesperline = subtitle_w * bpp;
//
//		switch (pfmt) {
//		case TCC_PFMT_YUV422:
//			sizeimage = subtitle_w * subtitle_h * 2;
//			break;
//		case TCC_PFMT_YUV420:
//			sizeimage = subtitle_w * subtitle_h * 3 / 2;
//			break;
//		case TCC_PFMT_RGB:
//		default:
//			sizeimage = bytesperline * subtitle_h;
//			break;
//		}
//
//		pix_mp->plane_fmt[1].bytesperline = bytesperline;
//		pix_mp->plane_fmt[1].sizeimage = PAGE_ALIGN(sizeimage);
//	} else {
//		pix_mp->plane_fmt[1].bytesperline =
//			pix_mp->plane_fmt[0].bytesperline;
//		pix_mp->plane_fmt[1].sizeimage = pix_mp->plane_fmt[0].sizeimage;
//	}
//
//	dprintk("pixelformat: %c%c%c%c\n", fourcc2char(pix_mp->pixelformat));
//	return 0;
//}

/**
 * vidioc_g_fmt_vid_out
 * @f: return current image src informations.
 */
static int vidioc_g_fmt_vid_out(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct tcc_vout_device *vout = fh;

	f->fmt.pix = vout->src_pix;

	print_v4l2_pix_format(&f->fmt.pix, "vidioc_g_fmt_vid_out");
	return 0;
}

//static int vidioc_g_fmt_vid_out_mplane(struct file *file, void *fh,
//	struct v4l2_format *f)
//{
//	struct tcc_vout_device *vout = fh;
//
//	f->fmt.pix_mp.width = vout->src_pix.width;
//	f->fmt.pix_mp.height = vout->src_pix.height;
//	f->fmt.pix_mp.pixelformat = vout->src_pix.pixelformat;
//	f->fmt.pix_mp.field = vout->src_pix.field;
//	f->fmt.pix_mp.colorspace = vout->src_pix.colorspace;
//
//	f->fmt.pix_mp.num_planes = MPLANE_NUM;
//
//	f->fmt.pix_mp.plane_fmt[MPLANE_VID].reserved[0] = MPLANE_VID;
//	f->fmt.pix_mp.plane_fmt[MPLANE_VID].sizeimage = vout->src_pix.sizeimage;
//	f->fmt.pix_mp.plane_fmt[MPLANE_VID].bytesperline =
//		vout->src_pix.bytesperline;
//
//	f->fmt.pix_mp.plane_fmt[MPLANE_SUB].reserved[0] = MPLANE_SUB;
//	f->fmt.pix_mp.plane_fmt[MPLANE_SUB].reserved[1] =
//		(vout->sub_pix.pixelformat & 0x0000ffff);
//	f->fmt.pix_mp.plane_fmt[MPLANE_SUB].reserved[2] =
//		(vout->sub_pix.pixelformat & 0xffff0000) >> 16;
//	f->fmt.pix_mp.plane_fmt[MPLANE_SUB].sizeimage = vout->sub_pix.sizeimage;
//	f->fmt.pix_mp.plane_fmt[MPLANE_SUB].bytesperline =
//		vout->sub_pix.bytesperline;
//
//	print_v4l2_pix_format_mplane(&f->fmt.pix_mp,
//		"vidioc_g_fmt_vid_out_mplane");
//	return 0;
//}

/**
 * vidioc_g_fmt_vid_overlay
 * @f: return current panel (screen) informations.
 */
static int vidioc_g_fmt_vid_out_overlay(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct tcc_vout_device *vout = fh;

	/* display info - OVERLAY_GET_POSITION */
	f->fmt.win.w.left = vout->disp_rect.left;
	f->fmt.win.w.top = vout->disp_rect.top;
	f->fmt.win.w.width = vout->disp_rect.width;
	f->fmt.win.w.height = vout->disp_rect.height;

	print_v4l2_rect(&f->fmt.win.w, "vidioc_g_fmt_overlay output info");

	return 0;
}

/**
 * vidioc_s_fmt_vid_out
 * @f: src image format from application
 */
static int vidioc_s_fmt_vid_out(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct tcc_vout_device *vout = fh;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int panel_width, panel_height;
	int ret = 0;

	if (f->fmt.pix.width == 0 || f->fmt.pix.height == 0) {
		pr_err(VOUT_NAME ": [warrning] retry %s(%dx%d)\n",
			__func__, f->fmt.pix.width, f->fmt.pix.height);
		return -EINVAL;
	}

	#ifndef CONFIG_ARCH_TCC898X
	if (f->fmt.pix.width > 4000 || f->fmt.pix.height > 4000) {
		pr_err(VOUT_NAME
			": [error] Image resolution not supported (%dx%d)\n",
			f->fmt.pix.width, f->fmt.pix.height);
		return -EINVAL;
	}
	#endif

	mutex_lock(&vout->lock);

	/* Set src image infomations */
	memcpy(&vout->src_pix, &f->fmt.pix, sizeof(struct v4l2_pix_format));

	/* init crop_src = src_pix */
	vout->crop_src.left = 0;
	vout->crop_src.top = 0;
	vout->crop_src.width = vout->src_pix.width;
	vout->crop_src.height = vout->src_pix.height;

	vout->fmt_idx = tcc_vout_try_fmt(vout->src_pix.pixelformat);
	vout->bpp = tcc_vout_try_bpp(vout->src_pix.pixelformat,
		&vout->src_pix.colorspace);
	vout->pfmt = tcc_vout_try_pix(f->fmt.pix.pixelformat);
	vout->previous_field = vout->src_pix.field;

	switch (vout->pfmt) {
	case TCC_PFMT_YUV422:
		vout->src_pix.sizeimage =
			vout->src_pix.width * vout->src_pix.height * 2;
		break;
	case TCC_PFMT_YUV420:
		vout->src_pix.sizeimage =
			vout->src_pix.width * vout->src_pix.height * 3 / 2;
		break;
	case TCC_PFMT_RGB:
	default:
		vout->src_pix.sizeimage = PAGE_ALIGN(vout->src_pix.width *
			vout->src_pix.height * vout->bpp);
		break;
	}

	/* Gst1.4 needs sizeimage */
	f->fmt.pix.sizeimage = vout->src_pix.sizeimage;

	/* FORMAT CONVERSION
	 *===================
	 * - VIQE and DEINTL_S must receive YUV format.
	 * - WMIX must receive RGB format.
	 * - Interlaced video has only YUV format.
	 *
	 * .y2r is YUV-to-RGB converter enable bit. (*** USE .y2rmd = 1)
	 * .r2y is RGB-to-YUV converter enable bit. (*** USE .r2ymd = 3)
	 *
	 * | input frame     | m2m_rdma | deinterlacer | m2m_wdma | (disp)rdma |
	 * |-----------------|----------|--------------|----------|------------|
	 * | Progressive RGB | y2r = 0  | N/A          | r2y = 1  | y2r = 1    |
	 * | Progressive YUV | y2r = 1  | N/A          | r2y = 1  | y2r = 1    |
	 * | Interlace YUV   | y2r = 0  | VIQE.y2r = 1 | r2y = 1  | y2r = 1    |
	 * | Interlace YUV   | y2r = 0  | DEINTL_S N/A | r2y = 0  | y2r = 1    |
	 *
	 * In case deintl_path uses VIQE,
	 * the properties of the frame can be changed
	 * between interlace and progessive.
	 * Therefore, 'm2m_rdma.y2r changing codes' existed
	 * in the vout_deintl_qbuf().
	 *
	 * Known-bug:
	 * In case of DEINTL_S + subtitle, It has a alpha mixing issue,
	 * because DEINTL_S doesn't exist y2r register.
	 */
	if (vout->onthefly_mode) {
		vioc->rdma.fmt = tcc_fmtdesc[vout->fmt_idx].reserved[0];
		vioc->rdma.swap = 0;	// R-G-B
		switch (vout->src_pix.pixelformat) {
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_YVU420:
		case V4L2_PIX_FMT_YUV422P:
			vout->src_pix.width =
				ROUND_UP_4(vout->src_pix.width);
			vout->src_pix.height =
				ROUND_UP_2(vout->src_pix.height);
			break;
		case V4L2_PIX_FMT_RGB24:
		case V4L2_PIX_FMT_RGB32:
			vioc->rdma.swap = 5;	// B-G-R in the rdma
			break;
		default:
			vioc->rdma.swap = 0;	// R-G-B
			break;
		}

		vioc->rdma.y2r = 0;
		vioc->rdma.y2rmd = 2;    // 2 = Studio Color
		vioc->viqe.y2r = 1;
		vioc->viqe.y2rmd = 2;    // 2 = Studio Color
		if ((vout->deinterlace == VOUT_DEINTL_S)
			|| (vout->deinterlace == VOUT_DEINTL_NONE)) {
			vioc->rdma.y2r = 1;
		}

#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
		vioc->rdma.y2r = 0;
		vioc->viqe.y2r = 0;
		vioc->viqe.y2rmd = 0;
#endif

		dprintk("rdma.y2r(%d)-viqe.y2r(%d)",
			vioc->rdma.y2r, vioc->viqe.y2r);

		vout->first_frame = 1;
		ret = vout_otf_init(vout);
		if (ret) {
			pr_err(VOUT_NAME ": [error] onthefly setup\n");
			goto out_exit;
		}
	} else {
		vioc->m2m_rdma.fmt = tcc_fmtdesc[vout->fmt_idx].reserved[0];
		vioc->m2m_rdma.swap = 0;	// R-G-B
		switch (vout->src_pix.pixelformat) {
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_YVU420:
		case V4L2_PIX_FMT_YUV422P:
			vout->src_pix.width =
				ROUND_UP_4(vout->src_pix.width);
			vout->src_pix.height =
				ROUND_UP_2(vout->src_pix.height);
			break;
		case V4L2_PIX_FMT_RGB24:
		case V4L2_PIX_FMT_RGB32:
			vioc->m2m_rdma.swap = 5; // B-G-R in the rdma
			break;
		default:
			vioc->m2m_rdma.swap = 0; // R-G-B
			break;
		}

		vioc->m2m_rdma.y2r = 0;
		vioc->m2m_rdma.y2rmd = 2; // 2 = Studio Color
		vioc->viqe.y2r = 1;
		vioc->viqe.y2rmd = 2;     // 2 = Studio Color
		vioc->m2m_wdma.r2y = 1;
		vioc->m2m_wdma.r2ymd = 2; // 2 = Studio Color
		vioc->rdma.y2r = 1;
		vioc->rdma.y2rmd = 2;     // 2 = Studio Color
		if ((vout->deinterlace == VOUT_DEINTL_S)
			|| (vout->deinterlace == VOUT_DEINTL_NONE)) {
			vioc->m2m_rdma.y2r = 1;
		}

#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
		vioc->m2m_rdma.y2r = 0;
		vioc->viqe.y2r = 0;
		vioc->m2m_wdma.r2y = 0;
		vioc->rdma.y2r = 0;
#endif

		dprintk("rdma.y2r(%d)-viqe.y2r(%d)-wdma.r2y(%d)-rdma.y2r(%d)",
			vioc->m2m_rdma.y2r, vioc->viqe.y2r,
			vioc->m2m_wdma.r2y, vioc->rdma.y2r);

		/* de-interlace path setting */
		if (vout->id == VOUT_MAIN) {
			/* The deintl_buf_size is calculated by the panel size.
			 *  - 20190809 alanK
			 */
			#if 0
			vout->deintl_buf_size =
				PAGE_ALIGN(vout->disp_rect.width *
					vout->disp_rect.height * 3 / 2);
			#else
			vout_wmix_getsize(vout, &panel_width, &panel_height);

		#if defined(CONFIG_TCC_DUAL_DISPLAY)
			vout->deintl_buf_size = 
				PAGE_ALIGN(vout->disp_rect.width * vout->disp_rect.height
							* 3 / 2);
			vout->m2m_dual_buf_size = 
				PAGE_ALIGN(vout->disp_rect.width * vout->disp_rect.height
							* 3 / 2);
		#else
			vout->deintl_buf_size = 
				PAGE_ALIGN(panel_width * panel_height * 3 / 2);
		#endif
			#endif
			vioc->m2m_wdma.fmt = VIOC_IMG_FMT_YUV420IL0;
		} else {
			vout->deintl_buf_size =
				PAGE_ALIGN(vout->disp_rect.width *
					vout->disp_rect.height * 2);
			// for GRDMA support
			vioc->m2m_wdma.fmt = VIOC_IMG_FMT_YUYV;
		}

		vout->first_frame = 1;
		ret = vout_m2m_init(vout);

	#if defined(CONFIG_TCC_DUAL_DISPLAY)
		ret |= vout_m2m_dual_init(vout);
	#endif

		if (ret) {
			pr_err("[ERR][VOUT] deinterlace setup\n");
			goto out_exit;
		}

		#ifdef CONFIG_VOUT_USE_SUB_PLANE
		vioc->subplane_init = 0;
		#endif

		/* vout path setting */
		vioc->rdma.fmt = vioc->m2m_wdma.fmt;
		vout_rdma_setup(vout);
		vout_wmix_setup(vout);
	}

out_exit:
	mutex_unlock(&vout->lock);

	dprintk("pixelformat: %c%c%c%c -> %s\n",
		fourcc2char(f->fmt.pix.pixelformat),
		tcc_fmtdesc[vout->fmt_idx].description);
	print_v4l2_pix_format(&vout->src_pix, "vidioc_s_fmt_vid_out user args");
	print_v4l2_rect(&vout->disp_rect,
		"vidioc_s_fmt_vid_out output display");
	print_vioc_vout_path(vout, "vidioc_s_fmt_vid_out");
	return ret;
}

/**
 * vidioc_s_fmt_vid_overlay
 * @f: modified src image format by scaling or crop.
 */
static int vidioc_s_fmt_vid_out_overlay(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct tcc_vout_device *vout = fh;
	unsigned int width, height;
	int ret = 0;

	mutex_lock(&vout->lock);

	/* 2-pixel alignment */
	f->fmt.win.w.left = ROUND_DOWN_2(f->fmt.win.w.left);
	f->fmt.win.w.top = ROUND_DOWN_2(f->fmt.win.w.top);
	f->fmt.win.w.width = ROUND_DOWN_2(f->fmt.win.w.width);
	f->fmt.win.w.height = ROUND_DOWN_2(f->fmt.win.w.height);

	if (vout->disp_rect.left == f->fmt.win.w.left &&
		vout->disp_rect.top == f->fmt.win.w.top &&
		vout->disp_rect.width == f->fmt.win.w.width &&
		vout->disp_rect.height == f->fmt.win.w.height) {
		dprintk("Nothing to do because setting the same size\n");
	#if !defined(CONFIG_TCC_DUAL_DISPLAY)
		goto overlay_exit;
	#endif
	}

	memcpy(&vout->disp_rect, &f->fmt.win.w, sizeof(struct v4l2_rect));

#if defined(CONFIG_TCC_DUAL_DISPLAY)
	if (f->fmt.win.clipcount>3)
		vout->disp_mode = 0;
	else
		vout->disp_mode = f->fmt.win.clipcount;

	if (f->fmt.win.bitmap == NULL) {
		memcpy(&vout->dual_disp_rect, &f->fmt.win.w,
			sizeof(struct v4l2_rect));
	} else {
		memcpy(&vout->dual_disp_rect, f->fmt.win.bitmap,
			sizeof(struct v4l2_rect));
	}

		if (!vout->dual_disp_rect.width)
			vout->dual_disp_rect.width = f->fmt.win.w.width;
		if (!vout->dual_disp_rect.height)
			vout->dual_disp_rect.height = f->fmt.win.w.height;
#endif

	vout_wmix_getsize(vout, &width, &height);
	if ((width == 0) || (height == 0)) {
		pr_err("[ERR][VOUT] output device size (%dx%d)\n",
			width, height);
		ret = -EBUSY;
		goto overlay_exit;
	}

	/* The image fits in the panel by cropping.
	 */
	if (vout->disp_rect.left < 0) {
		vout->disp_rect.width =
			vout->disp_rect.width + vout->disp_rect.left;
		if (width < vout->disp_rect.width) {
			vout->disp_rect.left = 0;
			vout->disp_rect.width = width;
		}
	} else {
		if (width < vout->disp_rect.left + vout->disp_rect.width)
			vout->disp_rect.width = width - vout->disp_rect.left;
	}

	if (vout->disp_rect.top < 0) {
		vout->disp_rect.height =
			vout->disp_rect.height + vout->disp_rect.top;
		if (height < vout->disp_rect.height) {
			vout->disp_rect.top = 0;
			vout->disp_rect.height = height;
		}
	} else {
		if (height < vout->disp_rect.top + vout->disp_rect.height)
			vout->disp_rect.height = height - vout->disp_rect.top;
	}

	vout->disp_rect.width = ROUND_DOWN_2(vout->disp_rect.width);
	vout->disp_rect.height = ROUND_DOWN_2(vout->disp_rect.height);

#if defined(CONFIG_TCC_DUAL_DISPLAY)
	if (vout->dual_disp_rect.left < 0) {
		vout->dual_disp_rect.width = vout->dual_disp_rect.width
			+ vout->dual_disp_rect.left;
		if (width < vout->dual_disp_rect.width) {
			vout->dual_disp_rect.left = 0;
			vout->dual_disp_rect.width = width;
		}
	} else {
		if (width < vout->dual_disp_rect.left + vout->dual_disp_rect.width)
			vout->dual_disp_rect.width = width - vout->dual_disp_rect.left;
	}

	if (vout->dual_disp_rect.top < 0) {
		vout->dual_disp_rect.height = vout->dual_disp_rect.height
			+ vout->dual_disp_rect.top;
		if(height < vout->dual_disp_rect.height) {
			vout->dual_disp_rect.top = 0;
			vout->dual_disp_rect.height = height;
		}
	} else {
		if (height < vout->dual_disp_rect.top + vout->dual_disp_rect.height)
			vout->dual_disp_rect.height = height - vout->dual_disp_rect.top;
	}

	vout->dual_disp_rect.width = ROUND_DOWN_2(vout->dual_disp_rect.width);
	vout->dual_disp_rect.height = ROUND_DOWN_2(vout->dual_disp_rect.height);
#endif

	if (vout->disp_rect.width <= 0 || vout->disp_rect.height <= 0) {
		vout->status = TCC_VOUT_STOP;
		goto overlay_exit;
	}
#if defined(CONFIG_TCC_DUAL_DISPLAY)
	else if(vout->dual_disp_rect.width <= 0 || vout->dual_disp_rect.height <= 0)
	{
		vout->status = TCC_VOUT_STOP;
		goto overlay_exit;
	}
#endif
	else {
		vout->status = TCC_VOUT_RUNNING;
	}
	vout_video_overlay(vout);

overlay_exit:
	mutex_unlock(&vout->lock);

	dprintk("\n");
	print_v4l2_rect(&f->fmt.win.w, "vidioc_s_fmt_overlay: user args");
	print_v4l2_rect(&vout->disp_rect, "vidioc_s_fmt_overlay: disp_rect");
	return ret;
}

static int vidioc_cropcap(struct file *file, void *fh,
	struct v4l2_cropcap *cropcap)
{
	struct tcc_vout_device *vout = fh;

	if (cropcap->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	/* We crop scaled image.
	 * input image -> scaling -> cropping -> display
	 */
	cropcap->bounds.width = vout->disp_rect.width;
	cropcap->bounds.height = vout->disp_rect.height;
	cropcap->defrect.width = vout->disp_rect.width;
	cropcap->defrect.height = vout->disp_rect.height;
	cropcap->defrect.left = 0;
	cropcap->defrect.top = 0;
	cropcap->pixelaspect.numerator = 1;
	cropcap->pixelaspect.denominator = 1;

	dprintk("%d/%d\n", cropcap->pixelaspect.numerator,
		cropcap->pixelaspect.denominator);
	print_v4l2_rect(&cropcap->bounds, "vidioc_cropcap - bounds");
	print_v4l2_rect(&cropcap->defrect, "vidioc_cropcap - defrect");
	return 0;
}

static int vidioc_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	dprintk("\n");

	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;
	return 0;
}

static int vidioc_s_crop(struct file *file, void *fh,
	const struct v4l2_crop *crop)
{
	int ret = -EINVAL;
	struct tcc_vout_device *vout = fh;

	dprintk("\n");

	mutex_lock(&vout->lock);

	if (((crop->c.left) < 0 || (crop->c.left >= vout->disp_rect.left))
		&& ((crop->c.top) < 0 || (crop->c.top >= vout->disp_rect.top))
		&& ((crop->c.width <= 0) ||
		((crop->c.left + crop->c.width) >= vout->disp_rect.width))
		&& ((crop->c.height <= 0) ||
		((crop->c.top + crop->c.height) >= vout->disp_rect.height))) {
		pr_err(VOUT_NAME ": [error] Invalid crop area (%d,%d)~(%dx%d)\n",
			crop->c.left, crop->c.top,
			crop->c.width, crop->c.height);
		goto s_crop_exit;
	}

	/* TODO: cropping codes */

s_crop_exit:
	mutex_unlock(&vout->lock);

	print_v4l2_rect(&crop->c, "vidioc_s_crop - user args");
	return ret;
}

static int vidioc_reqbufs(struct file *file, void *fh,
	struct v4l2_requestbuffers *req)
{
	struct tcc_vout_device *vout = fh;
	unsigned int max_frame = VIDEO_MAX_FRAME - vout->nr_qbufs;
	int i, ret = 0;

	dprintk("\n");

	/* free all buffers  */
	if (vout->qbufs && req->count == 0) {
		dprintk("free all buffers(%d) -> 0\n", vout->nr_qbufs);
		if (vout->nr_qbufs) {
			for (i = 0; i < vout->nr_qbufs; i++)
				kfree(vout->qbufs[i].buf.m.planes);
			kfree(vout->qbufs);
		}
		vout->nr_qbufs = 0;

		/* update buffer status */
		vout->mapped = OFF;
		goto reqbufs_exit;
	}

	if (!vout->mapped) {
		if (unlikely(req->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)) {
			pr_err(VOUT_NAME
				": [error] Invalid buf type(%d)\n", req->type);
			return -EINVAL;
		}

		mutex_lock(&vout->lock);

		vout->memory =
			vout->force_userptr ? V4L2_MEMORY_USERPTR : req->memory;

		if (vout->memory == V4L2_MEMORY_MMAP) {
			unsigned int actual_bufs, pmap_size;

			/* get pmap */
			ret = vout_get_pmap(&vout->pmap);
			if (ret) {
				pr_err("[ERR][VOUT] vout_get_pmap(%s)\n",
					vout->pmap.name);
				ret = -ENOMEM;
				goto reqbufs_exit_err;
			} else {
				if (tcc_vout_get_status(VOUT_SUB) >= 0) {
					pmap_size = vout->pmap.size/2;
					vout->pmap.base =
						(vout->id == VOUT_MAIN)
						? vout->pmap.base
						: (vout->pmap.base + pmap_size);
				} else {
					pmap_size = vout->pmap.size;
				}
			}

			/* calculate buf count */
			actual_bufs = pmap_size / vout->src_pix.sizeimage;
			dprintk(
				"V4L2_MEMORY_MMAP: request buf(%d), actual buf(%d)\n",
				req->count, actual_bufs);
			if ((req->count > actual_bufs)
				|| ((__s32)req->count < 0)) {
				req->count = actual_bufs;
			}
		} else if (vout->memory == V4L2_MEMORY_USERPTR) {
			dprintk("V4L2_MEMORY_USERPTR: request buf(%d)\n",
				req->count);
		} else {
			pr_err("[ERR][VOUT] Invalid memory type(%d)\n",
				req->memory);
			ret = -EINVAL;
			goto reqbufs_exit_err;
		}

		if (max_frame < req->count)
			req->count = max_frame;

		/* init driver data */
		vout->qbufs = kzalloc(sizeof(struct tcc_v4l2_buffer) *
			req->count, GFP_KERNEL);
		if (!vout->qbufs)
			return -ENOMEM;

		ret = tcc_v4l2_buffer_set(vout, req);
		if (ret < 0) {
			pr_err("[ERR][VOUT] tcc_v4l2_buffer_set(%d)\n", ret);
			goto reqbufs_exit;
		}

		vout->deintl_nr_bufs_count = 0;
		ret = tcc_deintl_buffer_set(vout);
		if (ret < 0) {
			pr_err("[ERR][VOUT] tcc_deintl_buffer_set(%d)\n", ret);
			goto reqbufs_deintl_err;
		}

	#if defined(CONFIG_TCC_DUAL_DISPLAY)
		ret = tcc_m2m_dual_buffer_set(vout);
		if (ret < 0) {
			pr_err("[ERR][VOUT] tcc_m2m_dual_buffer_set(%d)\n", ret);
			goto reqbufs_deintl_err;
		}
	#endif

		mutex_unlock(&vout->lock);

		/* update buffer status */
		vout->mapped = ON;

		print_v4l2_pix_format(&vout->src_pix,
			"vidioc_reqbufs src info");
		print_v4l2_buf_type(req->type, "vidioc_reqbufs");
		print_v4l2_memory(req->memory, "vidioc_reqbufs");
		return ret;

reqbufs_deintl_err:
		/* free tcc_v4l2_buffer_set */
		for (i = 0; i < vout->nr_qbufs; i++)
			kfree(vout->qbufs[i].buf.m.planes);
		kfree(vout->qbufs);
reqbufs_exit_err:
		mutex_unlock(&vout->lock);
	} else {
		pr_warn("[WAR][VOUT] any buffers are still mapped(%d)\n",
			vout->nr_qbufs);
		req->count = vout->nr_qbufs;
	}
reqbufs_exit:
	return ret;
}

#if 0
static int vidioc_create_bufs(struct file *file, void *fh,
	struct v4l2_create_buffers *create)
{
	struct tcc_vout_device *vout = fh;
	unsigned int max_frame = VIDEO_MAX_FRAME - vout->nr_qbufs;
	int ret = 0;

	/* check the validity of format.type field */
	if (unlikely(create->format.type != V4L2_BUF_TYPE_VIDEO_OUTPUT)) {
		pr_err(VOUT_NAME ": [error] Invalid buf type(%d)\n",
			create->format.type);
		ret = -EINVAL;
		goto create_bufs_exit;
	}

	/* check the validity of memory field */
	if (create->memory == V4L2_MEMORY_MMAP) {
		unsigned int actual_bufs;

		/* get pmap */
		ret = vout_get_pmap(&vout->pmap);
		if (ret) {
			pr_err(VOUT_NAME ": [error] vout_get_pmap(%s)\n",
				vout->pmap.name);
			ret = -ENOMEM;
			goto create_bufs_exit;
		}

		/* calculate buf count */
		actual_bufs = vout->pmap.size / vout->src_pix.sizeimage;
		dprintk("V4L2_MEMORY_MMAP: create buf(%d), actual buf(%d)\n",
			create->count, actual_bufs);
		if ((create->count > actual_bufs) || ((__s32)create->count < 0))
			create->count = actual_bufs;
	} else if (create->memory == V4L2_MEMORY_USERPTR) {
		dprintk("V4L2_MEMORY_USERPTR: create buf(%d)\n", create->count);
	} else {
		pr_err(VOUT_NAME ": [error] Invalid memory type(%d)\n",
			create->memory);
		ret = -EINVAL;
		goto create_bufs_exit;
	}

	if (create->count == 0) {
		dprintk(" current number of created buffers(%d)\n",
			vout->nr_qbufs);
		create->index = vout->nr_qbufs;
		goto create_bufs_exit;
	}

	mutex_lock(&vout->lock);

	if (max_frame < create->count) {
		create->count = max_frame;
	}

	ret = tcc_v4l2_buffer_add(vout, create);
	if (ret < 0) {
		pr_err(VOUT_NAME ": [error] tcc_v4l2_buffer_add(%d)\n", ret);
		goto create_bufs_exit_err;
	}

	mutex_unlock(&vout->lock);

	dprintk("buffer cout %d\n", create->count);

	print_v4l2_pix_format(&vout->src_pix, "vidioc_create_bufs src info");
	print_v4l2_buf_type(create->format.type, "vidioc_create_bufs");
	print_v4l2_memory(create->memory, "vidioc_create_bufs");

create_bufs_exit_err:
	mutex_unlock(&vout->lock);
create_bufs_exit:
	return ret;
}
#endif

static int vidioc_querybuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct tcc_vout_device *vout = fh;
	struct tcc_v4l2_buffer *qbuf = vout->qbufs + buf->index;

	dprintk("buffer index(%d) type(%d) memory(%d)\n", buf->index,
		buf->type, buf->memory);

	/* input (from v4l2sink) */
	qbuf->buf.index = buf->index;
	qbuf->buf.type = buf->type;
	qbuf->buf.memory = buf->memory;

	/* output (to v4l2sink) */
	buf->flags = qbuf->buf.flags = V4L2_BUF_FLAG_MAPPED;
	buf->length = qbuf->buf.length = vout->src_pix.sizeimage;

	qbuf->buf.m.planes[MPLANE_VID].m.mem_offset = qbuf->img_base0;
	qbuf->buf.m.planes[MPLANE_VID].length = vout->src_pix.sizeimage;

	if (copy_to_user(buf->m.planes, qbuf->buf.m.planes,
		sizeof(struct v4l2_buffer) * MPLANE_NUM)) {
		pr_err("%s: copy_to_user failed\n", __func__);
	}

	return 0;
}

static int vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct tcc_vout_device *vout = fh;
	int backupIdx;

	if (atomic_read(&vout->readable_buff_count) == vout->nr_qbufs) {
		pr_err("[ERR][VOUT] buffer full\n");
		return -EIO;
	}

	backupIdx = vout->pushIdx;
	if (++vout->pushIdx >= vout->nr_qbufs)
		vout->pushIdx = 0;

	if (tcc_vout_buffer_copy(&vout->qbufs[vout->pushIdx].buf, buf,
		TO_KERNEL_SPACE) < 0) {
		pr_err("[ERR][VOUT] memory copy fail\n");
		vout->pushIdx = backupIdx;
		return -EINVAL;
	}

	mutex_lock(&vout->lock);
	atomic_inc(&vout->readable_buff_count);

	/* buffer handling */
	buf->flags |= V4L2_BUF_FLAG_QUEUED;
	buf->flags &= ~V4L2_BUF_FLAG_DONE;

	dbprintk("q(%d %d)(%d %d)(%ldmsec)\n", vout->pushIdx,
		atomic_read(&vout->readable_buff_count),
			buf->index, buf->reserved2,
			(buf->timestamp.tv_sec * 1000) +
			(buf->timestamp.tv_usec / 1000));

	#ifndef CONFIG_VOUT_USE_VSYNC_INT
	vout_m2m_display_update(vout, &vout->qbufs[vout->popIdx].buf);
	if (++vout->popIdx >= vout->nr_qbufs)
		vout->popIdx = 0;
	atomic_dec(&vout->readable_buff_count);
	if (atomic_read(&vout->readable_buff_count) < 0)
		atomic_set(&vout->readable_buff_count, 0);
	#endif

	mutex_unlock(&vout->lock);

	return 0;
}

static int vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct tcc_vout_device *vout = fh;

	if (atomic_read(&vout->displayed_buff_count) == 0)
		return -EIO;

	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	if (tcc_vout_buffer_copy(buf, &vout->qbufs[vout->clearIdx].buf,
		TO_USER_SPACE) < 0) {
		pr_err("[ERR][VOUT] memory copy fail\n");
		return -EINVAL;
	}
	#else
	buf->index = vout->qbufs[vout->clearIdx].buf.index;
	buf->sequence = vout->qbufs[vout->clearIdx].buf.sequence;
	goto force_dqbuf;
	#endif

	if (vout->clearFrameMode)
		goto force_dqbuf;

	// check buffer info
	if (buf->index == vout->last_displayed_buf_idx)
		return -EIO;
#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
	{
		unsigned int iQueue_Idx = buf->index;

		if (++iQueue_Idx >= vout->nr_qbufs)
			iQueue_Idx = 0;

		if (iQueue_Idx == vout->last_displayed_buf_idx)
			return -EIO;

		//pr_debug("[DBG][VOUT] @@@@@@@@ ZzaU :: %d -> %d =? %d\n",
		//	buf->index, iQueue_Idx, vout->last_displayed_buf_idx);
	}
#endif

force_dqbuf:
	mutex_lock(&vout->lock);
	dbprintk("dq(%d %d)(%d %d)\n", vout->clearIdx,
		atomic_read(&vout->displayed_buff_count),
		buf->index, buf->reserved2);

	if (++vout->clearIdx >= vout->nr_qbufs)
		vout->clearIdx = 0;
	atomic_dec(&vout->displayed_buff_count);
	if (atomic_read(&vout->displayed_buff_count) < 0)
		atomic_set(&vout->displayed_buff_count, 0);

	/* buffer handling */
	buf->flags &= ~V4L2_BUF_FLAG_QUEUED;
	buf->flags &= ~V4L2_BUF_FLAG_DONE;
	if (atomic_read(&vout->displayed_buff_count) > 0) {
		buf->reserved = true;
	} else {
		buf->reserved = false;
		if (vout->clearFrameMode) {
			vout->popIdx = vout->clearIdx = 0;
			vout->pushIdx = -1;

			atomic_set(&vout->displayed_buff_count, 0);
			atomic_set(&vout->readable_buff_count, 0);
			vout->last_cleared_buffer = NULL;
			vout->last_displayed_buf_idx = 0;
			vout->display_done = OFF;

			vout->clearFrameMode = OFF; // disable celar frame mode
			dprintk("Disable clearFrameMode(%d)\n",
				vout->clearFrameMode);
		}
	}
	mutex_unlock(&vout->lock);
	return 0;
}

static int vidioc_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct tcc_vout_device *vout = fh;
	struct tcc_vout_vioc *vioc = vout->vioc;

	mutex_lock(&vout->lock);

	if (i != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		pr_err("[ERR][VOUT] invalid v4l2_buf_type(%d)\n", i);
		return -EINVAL;
	}

	/* If status is TCC_VOUT_STOP then we have to enable */
	if (vout->status == TCC_VOUT_STOP) {
		vout_disp_ctrl(vioc, 1);	// enable disp_path
		vout_m2m_ctrl(vioc, 1);		// enable deintl_path

		#if defined(CONFIG_TCC_DUAL_DISPLAY)
		vout_m2m_dual_ctrl(vioc, 1, M2M_DUAL_1);
		vout_m2m_dual_ctrl(vioc, 1, M2M_DUAL_0);
		#endif

		#ifdef CONFIG_VOUT_USE_SUB_PLANE
		vout_subplane_ctrl(vout, 1);	// enable subplane_path
		#endif
	}

	vout->status = TCC_VOUT_RUNNING;

	mutex_unlock(&vout->lock);
	dprintk("\n");
	return 0;
}

static int vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct tcc_vout_device *vout = fh;
	struct tcc_vout_vioc *vioc = vout->vioc;

	mutex_lock(&vout->lock);

	if (i != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		pr_err("[ERR][VOUT] invalid v4l2_buf_type(%d)\n", i);
		return -EINVAL;
	}

	vout->status = TCC_VOUT_STOP;

	vout_disp_ctrl(vioc, 0);	// disable disp_path
	if (!vout->onthefly_mode) {
		#if defined(CONFIG_TCC_DUAL_DISPLAY)
		vout_m2m_dual_ctrl(vioc, 0, M2M_DUAL_1);
		vout_m2m_dual_ctrl(vioc, 0, M2M_DUAL_0);
		#endif
		vout_m2m_ctrl(vioc, 0);		// disable deintl_path
	}

	#ifdef CONFIG_VOUT_USE_SUB_PLANE
	vout_subplane_ctrl(vout, 0);	// disalbe subplane_path
	#endif

	mutex_unlock(&vout->lock);
	dprintk("\n");
	return 0;
}

static long vout_param_handler(struct file *file, void *fh, bool valid_prio,
	unsigned int cmd, void *arg)
{
	struct tcc_vout_device *vout = fh;
	int ret = 0;

	if (vout == NULL)
		return -EINVAL;

	if (vout->status != TCC_VOUT_RUNNING) {
		/* only allowed if streaming is started */
		pr_err("[ERR][VOUT] device not started yet(%d)\n",
			vout->status);
		return -EBUSY;
	}

	mutex_lock(&vout->lock);

	switch (cmd) {
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	case VIDIOC_USER_CLEAR_FRAME:
		if (!vout->first_frame) {
			dprintk("Flushing all buffers [R:%d D:%d]\n",
				atomic_read(&vout->readable_buff_count),
				atomic_read(&vout->displayed_buff_count));
			vout_pop_all_buffer(vout);
		}
		break;
	case VIDIOC_USER_SET_OUTPUT_MODE:
		vout_video_set_output_mode(vout, *(int *)arg);
		break;
	#endif
	#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
	case VIDIOC_USER_CAPTURE_LASTFRAME:
		if (!vout->first_frame) {
			tcc_vout_buffer_copy(&vout->lastframe,
				(struct v4l2_buffer *)arg, TO_KERNEL_SPACE);
			vout_capture_last_frame(vout, &vout->lastframe);
		}
		break;
	case VIDIOC_USER_DISPLAY_LASTFRAME:
		vout_disp_last_frame(vout);
		break;
	#endif
	default:
		ret = -ENOTTY;
	}

	mutex_unlock(&vout->lock);
	dprintk("\n");

	return ret;
}

/*
 * File operations
 */
static int tcc_vout_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct video_device *vdev = video_devdata(file);
	struct tcc_vout_device *vout = video_get_drvdata(vdev);

	if (vout->memory == V4L2_MEMORY_USERPTR) {
		vma->vm_pgoff = 0;
		goto exit;
	}

	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		pr_err("[ERR][VOUT] range_is_allowed() failed\n");
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		pr_err("[ERR][VOUT] remap_pfn_range() failed\n");
		return -EAGAIN;
	}

exit:
	vma->vm_ops = NULL;
	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	return 0;
}

static void tcc_vout_setup_video_data(struct tcc_vout_device *vout)
{
	vout->status = TCC_VOUT_IDLE;

	/* TCC solution uses only V4L2_MEMORY_USERPTR by gstreamer */
	vout->force_userptr = 0;
	vout->deintl_force = 0;
}

static int tcc_vout_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct tcc_vout_device *vout;
	struct tcc_vout_vioc *vioc;
	int ret = 0;

	vout = video_get_drvdata(vdev);
	if (vout == NULL) {
		pr_err("[ERR][VOUT] open failed(nodev)\n");
		return -ENODEV;
	}
	if (vout->opened) {
		pr_err("[ERR][VOUT] tcc_vout only supports single open.\n");
		return -EBUSY;
	}

	vioc = vout->vioc;
	vout->qbufs = NULL;
	vout->deintl_bufs = NULL;

	file->private_data = vout;
	clk_prepare_enable(vioc->vout_clk);

	/* Telechips specific video data
	 */
	tcc_vout_setup_video_data(vout);

	/* Initialize TCC VIOC block
	 */
	ret = vout_vioc_init(vout);
	if (ret < 0) {
		pr_err("[ERR][VOUT] FAIL(%d) Could not open %s device.\n",
			ret, vdev->name);
		vout->status = TCC_VOUT_IDLE;
		return ret;
	}

	init_waitqueue_head(&vout->frame_wait);
	vioc->m2m_wdma.irq_enable = 0;

	vout->status = TCC_VOUT_INITIALISING;

	vout->opened++;
	dprintk("\n");
	return ret;
}

static int tcc_vout_release(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct tcc_vout_device *vout = video_get_drvdata(vdev);
	struct tcc_vout_vioc *vioc = vout->vioc;
	int index;

	vout_deinit(vout);			// deinit disp_path
	if (vout->onthefly_mode)
		vout_otf_deinit(vout);
	else
		vout_m2m_deinit(vout);		// deinit deintl_path

	#ifdef CONFIG_VOUT_USE_SUB_PLANE
	vout_subplane_deinit(vout);		// deinit subplane_path
	#endif

	/* we need to re-initialize becase gst_v4l2_object_set_format() */
	memset(&vout->src_pix, 0, sizeof(struct v4l2_pix_format));

	vout->popIdx = vout->clearIdx = 0;
	vout->pushIdx = -1;

	atomic_set(&vout->displayed_buff_count, 0);
	atomic_set(&vout->readable_buff_count, 0);

	clk_disable_unprepare(vioc->vout_clk);

	if (vout->qbufs && vout->nr_qbufs) {
		for (index = 0; index < vout->nr_qbufs; index++)
			kfree(vout->qbufs[index].buf.m.planes);
		kfree(vout->qbufs);
	}

	kfree(vout->deintl_bufs);
	#if defined(CONFIG_TCC_DUAL_DISPLAY)
	kfree(vout->m2m_dual_bufs);
	#endif
	kfree(vioc->m2m_wdma.vioc_intr);
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	kfree(vioc->disp.vioc_intr);
	#endif

	/* deinit buffer info */
	vout->nr_qbufs = 0;
	vout->mapped = OFF;

	vout->status = TCC_VOUT_IDLE;
	vout->opened--;
	dprintk("\n");
	return 0;
}


static const struct v4l2_ioctl_ops tcc_vout_ioctl_ops = {
	/* VIDIOC_QUERYCAP handler */
	.vidioc_querycap			= vidioc_querycap,

	/* VIDIOC_ENUM_FMT handlers */
	.vidioc_enum_fmt_vid_out		= vidioc_enum_fmt_vid_out,
	//.vidioc_enum_fmt_vid_out_mplane	= vidioc_enum_fmt_vid_out,

	/* VIDIOC_G_FMT handlers */
	.vidioc_g_fmt_vid_out			= vidioc_g_fmt_vid_out,
	.vidioc_g_fmt_vid_out_overlay		= vidioc_g_fmt_vid_out_overlay,
	//.vidioc_g_fmt_vid_out_mplane		= vidioc_g_fmt_vid_out_mplane,

	/* VIDIOC_S_FMT handlers */
	.vidioc_s_fmt_vid_out			= vidioc_s_fmt_vid_out,
	.vidioc_s_fmt_vid_out_overlay		= vidioc_s_fmt_vid_out_overlay,
	//.vidioc_s_fmt_vid_out_mplane		= vidioc_s_fmt_vid_out_mplane,

	/* VIDIOC_TRY_FMT handlers */
	.vidioc_try_fmt_vid_out			= vidioc_try_fmt_vid_out,
	//.vidioc_try_fmt_vid_out_mplane	= vidioc_try_fmt_vid_out_mplane,

	/* Buffer handlers */
	.vidioc_reqbufs				= vidioc_reqbufs,
	//.vidioc_create_bufs			= vidioc_create_bufs,
	.vidioc_querybuf			= vidioc_querybuf,
	.vidioc_qbuf				= vidioc_qbuf,
	.vidioc_dqbuf				= vidioc_dqbuf,

	/* Stream on/off */
	.vidioc_streamon			= vidioc_streamon,
	.vidioc_streamoff			= vidioc_streamoff,

	/* Standard handling (ENUMSTD is handled by videodev.c) */
	//.vidioc_s_std				= vidioc_s_std,
	//.vidioc_g_std				= vidioc_g_std,

	/* Input handling */
	.vidioc_enum_input			= vidioc_enum_input,

	/* Output handling */
	//.vidioc_enum_output			= vidioc_enum_output,
	//.vidioc_g_output			= vidioc_g_output,
	//.vidioc_s_output			= vidioc_s_output,

	/* Control handling */
	.vidioc_queryctrl			= vidioc_queryctrl,
	.vidioc_g_ctrl				= vidioc_g_ctrl,
	.vidioc_s_ctrl				= vidioc_s_ctrl,

	/* Crop ioctls */
	.vidioc_cropcap				= vidioc_cropcap,
	.vidioc_g_crop				= vidioc_g_crop,
	.vidioc_s_crop				= vidioc_s_crop,

	/* Stream type-dependent parameter ioctls */
	//.vidioc_g_parm			= vidioc_g_parm,
	//.vidioc_s_parm			= vidioc_s_parm,

	/* Debugging ioctls */
	//.vidioc_enum_framesizes		= vidioc_enum_framesizes,
	.vidioc_default				= vout_param_handler,
};

static const struct v4l2_file_operations tcc_vout_fops = {
	.owner			= THIS_MODULE,
	.open			= tcc_vout_open,
	.release		= tcc_vout_release,
	.unlocked_ioctl	= video_ioctl2,
	.mmap			= tcc_vout_mmap,
};

/*
 * platfome driver functions
 */
static int tcc_vout_v4l2_probe(struct platform_device *pdev)
{
	struct video_device *vdev = NULL;
	struct tcc_vout_device *vout = NULL;
	struct tcc_vout_vioc *vioc = NULL;

	int ret;

	vout = kzalloc(sizeof(struct tcc_vout_device), GFP_KERNEL);
	if (vout == NULL)
		return -ENOMEM;

	vioc = kzalloc(sizeof(struct tcc_vout_vioc), GFP_KERNEL);
	if (vioc == NULL)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		vout->v4l2_np = pdev->dev.of_node;
	} else {
		pr_err("[ERR][VOUT] could not find vout driver node.\n");
		ret = -ENODEV;
		goto probe_err0;
	}

	if (v4l2_device_register(&pdev->dev, &vout->v4l2_dev) < 0) {
		pr_err("[ERR][VOUT] v4l2 register failed\n");
		ret = -ENODEV;
		goto probe_err0;
	}

	/* Register the Video device with V4L2
	 */
	vdev = vout->vdev = video_device_alloc();
	if (!vdev) {
		pr_err("[ERR][VOUT] could not allocate video device struct\n");
		ret = -ENODEV;
		goto probe_err1;
	}

	vout->id = of_alias_get_id(vout->v4l2_np, "vout-drv");

	vdev->release = video_device_release;
	vdev->ioctl_ops = &tcc_vout_ioctl_ops;

	snprintf(vdev->name, sizeof(vdev->name), "vout_drv%d", vout->id);

	vdev->fops = &tcc_vout_fops;
	vdev->v4l2_dev = &vout->v4l2_dev;
	vdev->vfl_dir = VFL_DIR_TX;
	mutex_init(&vout->lock);

	//vdev->minor = -1;
	vdev->minor = 10 + vout->id;

	vout->vioc = vioc;
	pdev->dev.platform_data = vout;

	/* default vioc path
	 */
	ret = vout_vioc_set_default(vout);
	if (ret < 0) {
		pr_err("[ERR][VOUT] invalid vout vioc path\n");
		goto probe_err1;
	}

	/* buffer management initialize
	 */
	vout->popIdx = 0;
	vout->pushIdx = -1;
	vout->clearIdx = 0;
	vout->last_cleared_buffer = NULL;

	atomic_set(&vout->displayed_buff_count, 0);
	atomic_set(&vout->readable_buff_count, 0);

	/* Register the Video device width V4L2
	 */
	if (video_register_device(vdev, VFL_TYPE_GRABBER, vdev->minor) < 0) {
		pr_err("[ERR][VOUT] Counld not register Video for Linux device\n");
		ret = -ENODEV;
		goto probe_err2;
	}
	video_set_drvdata(vdev, vout);

	/* get vout clock
	 */
	vioc->vout_clk = of_clk_get(vout->v4l2_np, 0);

	tcc_vout_attr_create(pdev);

	tcc_vout[vout->id] = (void *)vout;

#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
	memset(&vout->lastframe, 0x0, sizeof(struct v4l2_buffer));
	vout->lastframe.m.planes =
		kzalloc(sizeof(struct v4l2_plane) * MPLANE_NUM, GFP_KERNEL);
	if (!vout->lastframe.m.planes) {
		pr_err("[ERR][VOUT] memory allocation fail\n");
		ret = -ENOMEM;
		goto probe_err2;
	}
#endif
	dprintk(": registered and initialized video device %d\n", vdev->minor);
	return 0;

probe_err2:
	video_unregister_device(vdev);
	video_device_release(vdev);
probe_err1:
	v4l2_device_unregister(&vout->v4l2_dev);
probe_err0:
	if (vout->vioc)
		kfree(vout->vioc);
	if (vout)
		kfree(vout);
	return ret;
}

static int tcc_vout_v4l2_remove(struct platform_device *pdev)
{
	struct tcc_vout_device *vout =
		(struct tcc_vout_device *)platform_get_drvdata(pdev);
#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
	struct v4l2_buffer *plastframe = &vout->lastframe;

	if (plastframe->m.planes)
		kfree(plastframe->m.planes);
#endif
	tcc_vout_attr_remove(pdev);

	if (vout->vioc)
		kfree(vout->vioc);
	if (vout)
		kfree(vout);
	dprintk("\n");
	return 0;
}

//static int tcc_vout_v4l2_suspend(struct platform_device *pdev,
//	pm_message_t state)
//{
//	dprintk("\n");
//	return 0;
//}

//static int tcc_vout_v4l2_resume(struct platform_device *pdev)
//{
//	dprintk("\n");
//	return 0;
//}

static struct platform_driver tcc_vout_v4l2_pdrv = {
	.driver = {
		.name	= VOUT_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tccvout_of_match),
#endif
	},
	.probe		= tcc_vout_v4l2_probe,
	.remove		= tcc_vout_v4l2_remove,
	//.suspend	= tcc_vout_v4l2_suspend,
	//.resume	= tcc_vout_v4l2_resume,
};

static int __init tcc_vout_v4l2_init(void)
{
	dprintk("\n");
	return 0;
}

static void __exit tcc_vout_v4l2_exit(void)
{
	dprintk("\n");
}

module_init(tcc_vout_v4l2_init);
module_exit(tcc_vout_v4l2_exit);
module_platform_driver(tcc_vout_v4l2_pdrv);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("VIOC VOUT driver");
MODULE_VERSION("v1.0");
MODULE_LICENSE("GPL");
