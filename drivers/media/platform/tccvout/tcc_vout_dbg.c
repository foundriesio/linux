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
#include <linux/videodev2.h>

#include "tcc_vout.h"
#include "tcc_vout_core.h"
#include "tcc_vout_dbg.h"

#ifdef CONFIG_TCC_VOUT_DEBUG
const char *name_v4l2_colorspace[] = {
	/* enum v4l2_colorspace */
	/* ITU-R 601 -- broadcast NTSC/PAL */
	[1] = "V4L2_COLORSPACE_SMPTE170M",

	/* 1125-Line (US) HDTV */
	[2] = "V4L2_COLORSPACE_SMPTE240M",

	/* HD and modern captures. */
	[3] = "V4L2_COLORSPACE_REC709",

	/* broken BT878 extents (601, luma range 16-253 instead of 16-235) */
	[4] = "V4L2_COLORSPACE_BT878",

	/* These should be useful.  Assume 601 extents. */
	[5] = "V4L2_COLORSPACE_470_SYSTEM_M",
	[6] = "V4L2_COLORSPACE_470_SYSTEM_BG",

	/* I know there will be cameras that send this.  So, this is
	 * unspecified chromaticities and full 0-255 on each of the
	 * Y'CbCr components
	 */
	[7] = "V4L2_COLORSPACE_JPEG",

	/* For RGB colourspaces, this is probably a good start. */
	[8] = "V4L2_COLORSPACE_SRGB",
};

const char *name_v4l2_field[] = {
	/* enum v4l2_field */

	/* driver can choose from none, top, bottom,
	 * interlaced depending on whatever it thinks is approximate ...
	 */
	[0] = "V4L2_FIELD_ANY",

	/* this device has no fields ... */
	[1] = "V4L2_FIELD_NONE",

	/* top field only */
	[2] = "V4L2_FIELD_TOP",

	/* bottom field only */
	[3] = "V4L2_FIELD_BOTTOM",

	/* both fields interlaced */
	[4] = "V4L2_FIELD_INTERLACED",

	/* both fields sequential into one buffer, top-bottom order */
	[5] = "V4L2_FIELD_SEQ_TB",

	/* same as above + bottom-top order */
	[6] = "V4L2_FIELD_SEQ_BT",

	/* both fields alternating into separate buffers */
	[7] = "V4L2_FIELD_ALTERNATE",

	/* both fields interlaced, top field first
	 * and the top field is transmitted first
	 */
	[8] = "V4L2_FIELD_INTERLACED_TB",

	/* both fields interlaced, top field first
	 * and the bottom field is transmitted first
	 */
	[9] = "V4L2_FIELD_INTERLACED_BT",
};

const char *name_v4l2_buf_type[] = {
	/* enum v4l2_buf_type */
	[1] = "V4L2_BUF_TYPE_VIDEO_CAPTURE",
	[2] = "V4L2_BUF_TYPE_VIDEO_OUTPUT",
	[3] = "V4L2_BUF_TYPE_VIDEO_OVERLAY",
	[4] = "V4L2_BUF_TYPE_VBI_CAPTURE",
	[5] = "V4L2_BUF_TYPE_VBI_OUTPUT",
	[6] = "V4L2_BUF_TYPE_SLICED_VBI_CAPTURE",
	[7] = "V4L2_BUF_TYPE_SLICED_VBI_OUTPUT",
	[8] = "V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY",	/* Experimental */
	[9] = "V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE",
	[10] = "V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE",
	[0x80] = "V4L2_BUF_TYPE_PRIVATE",
};

const char *name_v4l2_memory[] = {
	/* enum v4l2_memory */
	[1] = "V4L2_MEMORY_MMAP",
	[2] = "V4L2_MEMORY_USERPTR",
	[3] = "V4L2_MEMORY_OVERLAY",
};

const char *name_vioc_deintl_type[] = {
	[0] = "DEINTL_NONE",
	[1] = "VIQE_BYPASS",
	[2] = "VIQE_2D",
	[3] = "VIQE_3D",
	[4] = "DEINTL_S",
};

char *fourcc2str(unsigned int fourcc, char buf[4])
{
	buf[0] = (fourcc >>  0) & 0xFF;
	buf[1] = (fourcc >>  8) & 0xFF;
	buf[2] = (fourcc >> 16) & 0xFF;
	buf[3] = (fourcc >> 24) & 0xFF;
	return buf;
}

void print_v4l2_capability(struct v4l2_capability *cap, const char *str)
{
	pr_info("  [v4l2_capability] %s\n", str);
	pr_info("         driver: %s\n", cap->driver);
	pr_info("           card: %s\n", cap->card);
	pr_info("       bus_info: %s\n", cap->bus_info);
	pr_info("        version: %d\n", cap->version);
	pr_info("   capabilities: 0x%08x\n", cap->capabilities);
}

void print_v4l2_buf_type(enum v4l2_buf_type type, const char *str)
{
	pr_info("  [v4l2_buf_type] %s\n", str);
	pr_info("   type: %s\n", name_v4l2_buf_type[type]);
}

void print_v4l2_memory(enum v4l2_memory mem, const char *str)
{
	pr_info("  [v4l2_memory] %s\n", str);
	pr_info("   type: %s\n", name_v4l2_memory[mem]);
}

void print_v4l2_pix_format(struct v4l2_pix_format *pix, const char *str)
{
	char buf[5];

	buf[4] = 0;
	pr_info("[v4l2_pix_format] %s\n", str);
	pr_info("           width: %d\n", pix->width);
	pr_info("          height: %d\n", pix->height);
	pr_info("     pixelformat: %s\n", fourcc2str(pix->pixelformat, buf));
	pr_info("           field: %s\n", name_v4l2_field[pix->field]);
	pr_info("    bytesperline: %d\n", pix->bytesperline);
	pr_info("       sizeimage: %d\n", pix->sizeimage);
	pr_info("      colorspace: %s\n",
		name_v4l2_colorspace[pix->colorspace]);
	pr_info("            priv: %d\n", pix->priv);
}

void print_v4l2_pix_format_mplane(struct v4l2_pix_format_mplane *pix_mp,
	const char *str)
{
	char buf[5];

	buf[4] = 0;
	pr_info("  [v4l2_pix_format_mplane] %s\n", str);
	pr_info("                    width: %d\n", pix_mp->width);
	pr_info("                   height: %d\n", pix_mp->height);
	pr_info("              pixelformat: %s\n",
		fourcc2str(pix_mp->pixelformat, buf));
	pr_info("                    field: %s\n",
		name_v4l2_field[pix_mp->field]);
	pr_info("               colorspace: %s\n",
		name_v4l2_colorspace[pix_mp->colorspace]);
	pr_info("               num_planes: %d\n", pix_mp->num_planes);
	pr_info("plane_fmt[0].bytesperline: %d\n",
		pix_mp->plane_fmt[0].bytesperline);
	pr_info("plane_fmt[0].sizeimage   : %d\n",
		pix_mp->plane_fmt[0].sizeimage);
	pr_info("plane_fmt[0].reserved[0] : %d\n",
		pix_mp->plane_fmt[0].reserved[0]);
	pr_info("plane_fmt[1].bytesperline: %d\n",
		pix_mp->plane_fmt[1].bytesperline);
	pr_info("plane_fmt[1].sizeimage   : %d\n",
		pix_mp->plane_fmt[1].sizeimage);
	pr_info("plane_fmt[1].reserved[0] : %d\n",
		pix_mp->plane_fmt[1].reserved[0]);
	pr_info("plane_fmt[1].reserved[1] : %d\n",
		pix_mp->plane_fmt[1].reserved[1]);
	pr_info("plane_fmt[1].reserved[2] : %d\n",
		pix_mp->plane_fmt[1].reserved[2]);
}

void print_v4l2_rect(const struct v4l2_rect *rect, const char *str)
{
	pr_info("  [v4l2_rect] %s\n", str);
	pr_info("     left: %d\n", rect->left);
	pr_info("      top: %d\n", rect->top);
	pr_info("    width: %d\n", rect->width);
	pr_info("   height: %d\n", rect->height);
}

void print_v4l2_fmtdesc(struct v4l2_fmtdesc *fmt, const char *str)
{
	char buf[5];

	buf[4] = 0;
	pr_info("  [v4l2_fmtdesc] %s\n", str);
	pr_info("                    type: %s\n",
					name_v4l2_buf_type[fmt->type]);
	pr_info("                   flags: %d\n", fmt->flags);
	pr_info("             description: %s\n", fmt->description);
	pr_info("             pixelformat: %s\n",
					fourcc2str(fmt->pixelformat, buf));
	pr_info("   reserved[0](vioc fmt): %d\n", fmt->reserved[0]);
	pr_info("     reserved[1](y2r en): %d\n", fmt->reserved[1]);
}

void print_v4l2_buffer(struct v4l2_buffer *buf, const char *str)
{
	pr_info("  [v4l2_buffer] %s\n", str);
	pr_info("       index: %d\n", buf->index);
	pr_info("        type: %s\n", name_v4l2_buf_type[buf->type]);
	pr_info("   bytesused: %d\n", buf->bytesused);
	pr_info("       flags: %d\n", buf->flags);
	pr_info("       field: %s\n", name_v4l2_field[buf->field]);
	pr_info("      memory: %s\n", name_v4l2_memory[buf->memory]);
	pr_info("     userptr: 0x%08lx\n", buf->m.userptr);
	pr_info("      length: %d\n", buf->length);
//	pr_info("       input: %d\n", buf->input);
	pr_info("    reserved: %d\n", buf->reserved);
}

void print_v4l2_reqbufs_format(enum tcc_pix_fmt pfmt, unsigned int pixelformat,
	const char *str)
{
	char buf[5];

	buf[4] = 0;
	pr_info("  [v4l2_reqbufs_format] %s\n", str);
	pr_info("  pixelformat: %s\n", fourcc2str(pixelformat, buf));

	switch (pfmt) {
	case TCC_PFMT_YUV422:
		pr_info("         pfmt: TCC_PFMT_YUV422\n");
		break;
	case TCC_PFMT_YUV420:
		pr_info("         pfmt: TCC_PFMT_YUV420\n");
		break;
	case TCC_PFMT_RGB:
		pr_info("         pfmt: TCC_PFMT_RGB\n");
		break;
	default:
		pr_info("         pfmt: N/A\n");
		break;
	}

	switch (pixelformat) {
	case V4L2_PIX_FMT_YUV420:  // YUV420 separated (VIOC_IMG_FMT_YUV420SEP)
	case V4L2_PIX_FMT_YVU420:  // YUV420 separated (VIOC_IMG_FMT_YUV420SEP)
	case V4L2_PIX_FMT_YUV422P: // YUV422 separated (VIOC_IMG_FMT_YUV422SEP)
		pr_info("         type: separated\n");
		break;
	case V4L2_PIX_FMT_NV12: // YUV420 interleaved 0 (VIOC_IMG_FMT_YUV420IL0)
	case V4L2_PIX_FMT_NV21: // YUV420 interleaved 1 (VIOC_IMG_FMT_YUV420IL1)
	case V4L2_PIX_FMT_NV16: // YUV422 interleaved 0 (VIOC_IMG_FMT_YUV422IL0)
	case V4L2_PIX_FMT_NV61: // YUV422 interleaved 1 (VIOC_IMG_FMT_YUV422IL1)
		pr_info("         type: interleaved\n");
		break;
	case V4L2_PIX_FMT_UYVY: // YUV422 UYVY Sequential (VIOC_IMG_FMT_UYVY)
	case V4L2_PIX_FMT_VYUY: // YUV422 VYUY Sequential (VIOC_IMG_FMT_VYUY)
	case V4L2_PIX_FMT_YUYV: // YUV422 YUYV Sequential (VIOC_IMG_FMT_YUYV)
	case V4L2_PIX_FMT_YVYU: // YUV422 YVYU Sequential (VIOC_IMG_FMT_YVYU)
		pr_info("         type: Sequential\n");
		break;
	default:			/* RGB and etc... */
		pr_info("         type: RGB or etc...\n");
	}
}

void print_vioc_vout_path(struct tcc_vout_device *vout, const char *str)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	char buf_sc[8];

	sprintf(buf_sc, " - SC%d", get_vioc_index(vioc->sc.id));

	pr_info("\n\e[33m[RDMA%d%s - WMIX%d - DISP%d]\e[0m %s\n",
		get_vioc_index(vioc->rdma.id),
		vout->onthefly_mode  ? buf_sc : "",
		get_vioc_index(vioc->wmix.id),
		get_vioc_index(vioc->disp.id), str);
	pr_info("  RDMA%d: %dx%d\n", get_vioc_index(vioc->rdma.id),
		vioc->rdma.width, vioc->rdma.height);
	if (vout->onthefly_mode) {
		pr_info("  SC%d: %dx%d -> %dx%d\n",
			get_vioc_index(vioc->sc.id),
			vioc->rdma.width, vioc->rdma.height,
			vout->disp_rect.width, vout->disp_rect.height);
	}
}

void print_vioc_deintl_path(struct tcc_vout_device *vout, const char *str)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	pr_info("\n\e[33m[RDMA%d - %s -  WMIX%d - SC%d - WDMA%d]\e[0m %s\n",
		get_vioc_index(vioc->m2m_rdma.id),
		name_vioc_deintl_type[vout->deinterlace],
		get_vioc_index(vioc->m2m_wmix.id),
		get_vioc_index(vioc->sc.id),
		get_vioc_index(vioc->m2m_wdma.id), str);
	pr_info("  RDMA%d: %dx%d\n", get_vioc_index(vioc->m2m_rdma.id),
		vioc->m2m_rdma.width, vioc->m2m_rdma.height);
	pr_info("  %s\n", name_vioc_deintl_type[vout->deinterlace]);
	pr_info("  WMIX%d: %dx%d\n", get_vioc_index(vioc->m2m_wmix.id),
		vout->crop_src.width, vout->crop_src.height);
	pr_info("  SC%d: %dx%d -> %dx%d\n", get_vioc_index(vioc->sc.id),
		vioc->m2m_rdma.width, vioc->m2m_rdma.height,
		vout->disp_rect.width, vout->disp_rect.height);
	pr_info("  WDMA%d: %dx%d\n\n", get_vioc_index(vioc->m2m_wdma.id),
		vout->disp_rect.width, vout->disp_rect.height);
}

void print_vioc_subplane_info(struct tcc_vout_device *vout, const char *str)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	pr_info("\n\e[33m[RDMA%d - WMIX%d]\e[0m %s\n",
		get_vioc_index(vioc->m2m_subplane_rdma.id),
		get_vioc_index(vioc->m2m_subplane_wmix.id), str);
	pr_info("  RDMA%d: %dx%d\n",
		get_vioc_index(vioc->m2m_subplane_rdma.id),
		vioc->m2m_subplane_rdma.width,
		vioc->m2m_subplane_rdma.height);
	pr_info("  WMIX%d: (%d, %d)\n",
		get_vioc_index(vioc->m2m_subplane_wmix.id),
		vioc->m2m_subplane_wmix.left,
		vioc->m2m_subplane_wmix.top);
}

#endif	//CONFIG_TCC_VOUT_DEBUG
