/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        tcc_drm_vioc.c
*  \brief       Telechips DRM Driver
*  \details
*  \version     1.0
*  \date        2014-2017
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular
purpose or non-infringement  of  any  patent,  copyright  or  other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source  code or the  use in the
source code.
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure
Agreement between Telechips and Company.
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>

#include <drm/drmP.h>
#include <drm/tcc_drm.h>
#include "tcc_drm_drv.h"
#include "tcc_drm_ipp.h"
#include "tcc_drm_vioc.h"

#if 0 /*defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)*/
#include <mach/bsp.h>
#include <mach/vioc_rdma.h>
#include <mach/vioc_wdma.h>
#include <mach/vioc_wmix.h>
#include <mach/vioc_config.h>
#include <mach/vioc_scaler.h>
#include <mach/vioc_global.h>

#include <mach/vioc_intr.h>
#include <mach/of_vioc_rdma.h>
#include <mach/of_vioc_wmix.h>
#include <mach/of_vioc_sc.h>
#include <mach/of_vioc_wdma.h>
#include <mach/vioc_blk.h>
#else
#include <video/tcc/vioc_blk.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_global.h>

#include <video/tcc/vioc_intr.h>
#include <video/tcc/of_vioc_rdma.h>
#include <video/tcc/of_vioc_wmix.h>
#include <video/tcc/of_vioc_sc.h>
#include <video/tcc/of_vioc_wdma.h>
#endif

#define VIOC_REFRESH_MAX	60
#define VIOC_REFRESH_MIN	15
#define VIOC_CROP_WIDTH_MAX	3840
#define VIOC_CROP_HEIGHT_MAX	2160
#define VIOC_CROP_WIDTH_MIN	100
#define VIOC_CROP_HEIGHT_MIN	100
#define VIOC_SCALE_WIDTH_MAX	3840
#define VIOC_SCALE_HEIGHT_MAX	2160
#define VIOC_SCALE_WIDTH_MIN	100
#define VIOC_SCALE_HEIGHT_MIN	100

#define get_vioc_context(dev)	platform_get_drvdata(to_platform_device(dev))
#define get_ctx_from_ippdrv(ippdrv)	container_of(ippdrv,\
					struct vioc_context, ippdrv);
/*
 * A structure of buffer context
 * @rdma : info of rdma block
 * @wmix : info of wmixer block
 * @sc : info of scaler block
 * @wdma : info of wdma block
 */
struct vioc_blk_info {
	struct vioc_rdma_device	*rdma;
	struct vioc_wmix_device	*wmix;
	struct vioc_sc_device	*sc;
	struct vioc_wdma_device	*wdma;
	void __iomem	*cfg_reg;
	u32 irq_reged;
};

/*
 * A structure of buffer context
 * @addr : gem objects and dma address, size
 * @size : size of buffer
 * @crop : crop information of buffer
 * @fmt : format type of buffer
 * @buf_id: id of buffer.
 */
struct vioc_buf_info {
	struct drm_tcc_sz size;
	struct drm_tcc_pos crop;
	struct drm_tcc_ipp_buf_info addr;
	u32 fmt;
	u32 buf_id;
};

/*
 * A structure of vioc context.
 *
 * @ippdrv: prepare initialization using ippdrv.
 * @lock: locking of operations.
 * @id: vioc id.
 * @buf: information of src/dst buffer
 * @vioc: information of vioc block
 */
struct vioc_context {
	struct tcc_drm_ippdrv	ippdrv;
	struct clk		*clk;
	struct vioc_blk_info *blk;
	struct vioc_buf_info buf[TCC_DRM_OPS_MAX];
	spinlock_t	lock;
};

struct vioc_fmt_data {
	u32 drm_format;
	u32 tcc_format;
};

static const struct vioc_fmt_data formats[] = {
	{DRM_FORMAT_RGB565, VIOC_IMG_FMT_RGB565},
	{DRM_FORMAT_RGB888, VIOC_IMG_FMT_RGB888},
	{DRM_FORMAT_ARGB8888, VIOC_IMG_FMT_ARGB8888},
	{DRM_FORMAT_YUV444, VIOC_IMG_FMT_444SEP},
	{DRM_FORMAT_YUYV, VIOC_IMG_FMT_YUYV},
	{DRM_FORMAT_YVYU, VIOC_IMG_FMT_YVYU},
	{DRM_FORMAT_UYVY, VIOC_IMG_FMT_UYVY},
	{DRM_FORMAT_VYUY, VIOC_IMG_FMT_VYUY},
	{DRM_FORMAT_YUV422, VIOC_IMG_FMT_YUV422SEP},
	{DRM_FORMAT_YUV420, VIOC_IMG_FMT_YUV420SEP},
	{DRM_FORMAT_YUV420, VIOC_IMG_FMT_YUV420SEP},
	{DRM_FORMAT_YUV420, VIOC_IMG_FMT_YUV420SEP},
	{DRM_FORMAT_NV12, VIOC_IMG_FMT_YUV420IL0},
};

extern void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr, unsigned int src_imgx, unsigned int  src_imgy,
									unsigned int start_x, unsigned int start_y, unsigned int* Y, unsigned int* U,unsigned int* V);

#define NUM_FORMATS (sizeof(formats) / sizeof(formats[0]))
static int drm_format_to_tcc_format(u32 format)
{
	int i;
	for (i = 0; i < NUM_FORMATS; i++)
		if (formats[i].drm_format == format)
			return formats[i].tcc_format;
	return 0;
}

static void vioc_sw_reset(struct vioc_context *ctx)
{
	volatile PVIOC_IREQ_CONFIG pIREQConfig = VIOC_IREQConfig_GetAddress();
	struct vioc_blk_info *vioc = ctx->blk;

	/* s/w reset */
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_WDMA, vioc->wdma->id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_WMIXER, vioc->wmix->id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_SCALER, vioc->sc->id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_RDMA, vioc->rdma->id, VIOC_CONFIG_RESET);

	/* s/w reset clear */
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_RDMA, vioc->rdma->id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_SCALER, vioc->sc->id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_WMIXER, vioc->wmix->id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pIREQConfig, VIOC_CONFIG_WDMA, vioc->wdma->id, VIOC_CONFIG_CLEAR);
}

static int vioc_src_set_fmt(struct device *dev, u32 fmt)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct vioc_blk_info *vioc = ctx->blk;
	struct vioc_buf_info *src = &ctx->buf[TCC_DRM_OPS_SRC];
	u32 format = 0;

	DRM_DEBUG_KMS("fmt[0x%x]\n", fmt);

	format = drm_format_to_tcc_format(fmt);
	if(!format) {
		DRM_ERROR("inavlid source format 0x%x.\n", fmt);
		return -EINVAL;
	}

	VIOC_RDMA_SetImageFormat(vioc->rdma->reg, format);

	/* backup buffer information */
	src->fmt = format;

	return 0;
}

static int vioc_src_set_transf(struct device *dev,
		enum drm_tcc_degree degree,
		enum drm_tcc_flip flip, bool *swap)
{
	DRM_DEBUG_KMS("degree[%d]flip[0x%x]\n", degree, flip);

	if(flip != TCC_DRM_FLIP_NONE) {
		DRM_ERROR("inavlid flip value %d.\n", flip);
		return -EINVAL;
	}

	if(degree != TCC_DRM_DEGREE_0) {
		DRM_ERROR("inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	return 0;
}

static int vioc_src_set_size(struct device *dev, int swap,
		struct drm_tcc_pos *pos, struct drm_tcc_sz *sz)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct vioc_blk_info *vioc = ctx->blk;
	struct vioc_buf_info *src = &ctx->buf[TCC_DRM_OPS_SRC];
	struct drm_tcc_pos img_pos = *pos;
	struct drm_tcc_sz img_sz = *sz;

	if(swap) {
		dev_info(ippdrv->dev, "Could not support swap %d.\n", swap);
		return -EINVAL;
	}

	DRM_DEBUG_KMS("swap[%d]hsize[%d]vsize[%d]\n", swap, sz->hsize, sz->vsize);
	DRM_DEBUG_KMS("x[%d]y[%d]w[%d]h[%d]\n", pos->x, pos->y, pos->w, pos->h);

	VIOC_RDMA_SetImageSize(vioc->rdma->reg, img_pos.w, img_pos.h);
	VIOC_RDMA_SetImageOffset(vioc->rdma->reg, src->fmt, img_sz.hsize);

	/* backup buffer information */
	memcpy(&src->size, &img_sz, sizeof(struct drm_tcc_sz));
	memcpy(&src->crop, &img_pos, sizeof(struct drm_tcc_pos));

	return 0;
}

static int vioc_src_set_addr(struct device *dev,
		struct drm_tcc_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_tcc_ipp_buf_type buf_type)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_tcc_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_tcc_ipp_property *property;
	struct vioc_blk_info *vioc = ctx->blk;
	struct vioc_buf_info *src = &ctx->buf[TCC_DRM_OPS_SRC];

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;
	DRM_DEBUG_KMS("prop_id[%d]buf_id[%d]buf_type[%d]\n",
		property->prop_id, buf_id, buf_type);

	/* set address register*/
	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		if((src->crop.w != src->size.hsize) || (src->crop.h != src->size.vsize)) {
			tccxxx_GetAddress(src->fmt, (u32)buf_info->base[TCC_DRM_PLANAR_Y],
					src->size.hsize, src->size.vsize, src->crop.x, src->crop.y,
					&buf_info->base[TCC_DRM_PLANAR_Y], &buf_info->base[TCC_DRM_PLANAR_CB], &buf_info->base[TCC_DRM_PLANAR_CR]);
		}
		VIOC_RDMA_SetImageBase(vioc->rdma->reg, buf_info->base[TCC_DRM_PLANAR_Y], buf_info->base[TCC_DRM_PLANAR_CB], buf_info->base[TCC_DRM_PLANAR_CR]);
		VIOC_RDMA_SetImageEnable(vioc->rdma->reg);
		break;
	case IPP_BUF_DEQUEUE:
		/*
		 * TODO
		 * source buffer dequeue
		 */
		break;
	default:
		dev_info(ippdrv->dev, "inavlid buf_type %d.\n", buf_type);
		return -ENOMEM;
	}

	/* backup buffer information */
	memcpy(&src->addr, buf_info, sizeof(struct drm_tcc_ipp_buf_info));
	memcpy(&src->buf_id, &buf_id, sizeof(u32));

	return 0;
}

static struct tcc_drm_ipp_ops vioc_src_ops = {
	.set_fmt = vioc_src_set_fmt,
	.set_transf = vioc_src_set_transf,
	.set_size = vioc_src_set_size,
	.set_addr = vioc_src_set_addr,
};

static int vioc_dst_set_csc(struct vioc_context *ctx)
{
	struct vioc_blk_info *vioc = ctx->blk;
	struct vioc_buf_info *src = &ctx->buf[TCC_DRM_OPS_SRC];
	struct vioc_buf_info *dst = &ctx->buf[TCC_DRM_OPS_DST];

	DRM_DEBUG_KMS("src->fmt[%d]dst->fmt[%d]\n",
		src->fmt, dst->fmt);

	if(src->fmt != dst->fmt) {
		if((src->fmt > VIOC_IMG_FMT_ARGB6666_3) && (dst->fmt < VIOC_IMG_FMT_444SEP)) {
			VIOC_RDMA_SetImageY2RMode(vioc->rdma->reg, 0x2);
			VIOC_RDMA_SetImageY2REnable(vioc->rdma->reg, 1);
			VIOC_RDMA_SetImageR2YEnable(vioc->rdma->reg, 0);
		} else if((src->fmt < VIOC_IMG_FMT_444SEP) && (dst->fmt > VIOC_IMG_FMT_ARGB6666_3)) {
			VIOC_RDMA_SetImageR2YMode(vioc->rdma->reg, 0x2);
			VIOC_RDMA_SetImageR2YEnable(vioc->rdma->reg, 1);
			VIOC_RDMA_SetImageY2REnable(vioc->rdma->reg, 0);
		} else {
			VIOC_RDMA_SetImageY2REnable(vioc->rdma->reg, 0);
			VIOC_RDMA_SetImageR2YEnable(vioc->rdma->reg, 0);
		}
	}

	return 0;
}


static int vioc_dst_set_fmt(struct device *dev, u32 fmt)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct vioc_blk_info *vioc = ctx->blk;
	struct vioc_buf_info *dst = &ctx->buf[TCC_DRM_OPS_DST];
	u32 format = 0;

	DRM_DEBUG_KMS("fmt[0x%x]\n", fmt);

	format = drm_format_to_tcc_format(fmt);
	if(!format) {
		DRM_ERROR("inavlid source format 0x%x.\n", fmt);
		return -EINVAL;
	}

	VIOC_WDMA_SetImageFormat(vioc->wdma->reg, format);

	/* backup buffer information */
	dst->fmt = format;

	return vioc_dst_set_csc(ctx);
}

static int vioc_dst_set_transf(struct device *dev,
		enum drm_tcc_degree degree,
		enum drm_tcc_flip flip, bool *swap)
{
	DRM_DEBUG_KMS("degree[%d]flip[0x%x]\n", degree, flip);

	if(flip != TCC_DRM_FLIP_NONE) {
		DRM_ERROR("inavlid flip value %d.\n", flip);
		return -EINVAL;
	}

	if(degree != TCC_DRM_DEGREE_0) {
		DRM_ERROR("inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	return 0;
}

static int vioc_set_scaler(struct vioc_context *ctx)
{
	struct vioc_blk_info *vioc = ctx->blk;
	struct vioc_buf_info *src = &ctx->buf[TCC_DRM_OPS_SRC];
	struct vioc_buf_info *dst = &ctx->buf[TCC_DRM_OPS_DST];
	u32 bypass = 0;

	if((src->crop.w == 0) || (src->crop.h == 0)) {
		DRM_ERROR("inavlid src size %dx%d.\n", src->crop.w, src->crop.h);
		return -EINVAL;
	}

	if((src->crop.w == dst->crop.w) && (src->crop.h == dst->crop.h))
		bypass = 1;

	VIOC_SC_SetBypass(vioc->sc->reg, bypass);
	VIOC_SC_SetOutPosition(vioc->sc->reg,	dst->crop.x, dst->crop.y);
	VIOC_SC_SetOutSize(vioc->sc->reg, dst->crop.w, dst->crop.h);
	VIOC_SC_SetDstSize(vioc->sc->reg, dst->size.hsize, dst->size.vsize);
	VIOC_CONFIG_PlugIn(vioc->sc->id, vioc->sc->path);
	VIOC_SC_SetUpdate(vioc->sc->reg);

	VIOC_CONFIG_WMIXPath(vioc->wmix->path, 1/* mixing path */);
	VIOC_WMIX_SetSize(vioc->wmix->reg, dst->crop.w, dst->crop.h);
	VIOC_WMIX_SetUpdate(vioc->wmix->reg);

	return 0;
}

static int vioc_dst_set_size(struct device *dev, int swap,
		struct drm_tcc_pos *pos, struct drm_tcc_sz *sz)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct drm_tcc_pos img_pos = *pos;
	struct drm_tcc_sz img_sz = *sz;
	struct vioc_blk_info *vioc = ctx->blk;
	struct vioc_buf_info *dst = &ctx->buf[TCC_DRM_OPS_DST];

	DRM_DEBUG_KMS("swap[%d]hsize[%d]vsize[%d]\n",
		swap, sz->hsize, sz->vsize);

	if(swap) {
		DRM_ERROR("Could not support swap %d.\n", swap);
		return -EINVAL;
	}

	VIOC_WDMA_SetImageSize(vioc->wdma->reg, img_pos.w, img_pos.h);
	VIOC_WDMA_SetImageOffset(vioc->wdma->reg, dst->fmt, img_sz.hsize);

	/* backup buffer information */
	memcpy(&dst->size, &img_sz, sizeof(struct drm_tcc_sz));
	memcpy(&dst->crop, &img_pos, sizeof(struct drm_tcc_pos));

	return vioc_set_scaler(ctx);
}

static int vioc_dst_set_addr(struct device *dev,
		struct drm_tcc_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_tcc_ipp_buf_type buf_type)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_tcc_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_tcc_ipp_property *property;
	struct vioc_buf_info *dst = &ctx->buf[TCC_DRM_OPS_DST];
	struct vioc_blk_info *vioc = ctx->blk;

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;

	DRM_DEBUG_KMS("prop_id[%d]buf_id[%d]buf_type[%d]\n",
		property->prop_id, buf_id, buf_type);

	/* address register set */
	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		VIOC_WDMA_SetImageBase(vioc->wdma->reg, buf_info->base[TCC_DRM_PLANAR_Y], buf_info->base[TCC_DRM_PLANAR_CB], buf_info->base[TCC_DRM_PLANAR_CR]);
		VIOC_WDMA_SetImageEnable(vioc->wdma->reg, 0/* frame-by-frame mode */);
		iowrite32((vioc->wdma->intr->bits&VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg + 0x44));
		break;
	case IPP_BUF_DEQUEUE:
		/*
		 * TODO
		 * destination buffer dequeue
		 */
		break;
	default:
		break;
	}

	/* backup buffer information */
	memcpy(&dst->addr, buf_info, sizeof(struct vioc_buf_info));
	memcpy(&dst->buf_id, &buf_id, sizeof(u32));

	return 0;
}

static struct tcc_drm_ipp_ops vioc_dst_ops = {
	.set_fmt = vioc_dst_set_fmt,
	.set_transf = vioc_dst_set_transf,
	.set_size = vioc_dst_set_size,
	.set_addr = vioc_dst_set_addr,
};

static int vioc_get_buf_id(struct vioc_context *ctx)
{
	struct vioc_buf_info *dst = &ctx->buf[TCC_DRM_OPS_DST];
	if(dst)
		return dst->buf_id;
	else
		return -EINVAL;
}

static irqreturn_t vioc_ipp_handler(int irq, void *client_data)
{
	struct vioc_context *ctx = client_data;
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_tcc_ipp_cmd_node *c_node = ippdrv->c_node;
	struct vioc_blk_info *vioc = ctx->blk;
	int buf_id;

	if (!c_node)
		return IRQ_NONE;

	if (ioread32(vioc->wdma->reg+0x48) & (vioc->wdma->intr->bits&VIOC_WDMA_INT_MASK))
		return IRQ_NONE;

	if(!(ioread32(vioc->wdma->reg+0x44) & (vioc->wdma->intr->bits&VIOC_WDMA_INT_MASK)))
		return IRQ_NONE;

	iowrite32((vioc->wdma->intr->bits&VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg + 0x44));

	buf_id = vioc_get_buf_id(ctx);
	if (buf_id < 0)
		return IRQ_HANDLED;

	DRM_DEBUG_KMS("buf_id[%d]\n", buf_id);

	if(c_node->event_work) {
		struct drm_tcc_ipp_event_work *event_work = c_node->event_work;
		event_work->ippdrv = ippdrv;
		event_work->buf_id[TCC_DRM_OPS_DST] = buf_id;
		queue_work(ippdrv->event_workq, &event_work->work);
	}
	return IRQ_HANDLED;
}

static int vioc_init_prop_list(struct tcc_drm_ippdrv *ippdrv)
{
	struct drm_tcc_ipp_prop_list *prop_list = &ippdrv->prop_list;

	memset(prop_list, 0x0, sizeof(struct drm_tcc_ipp_prop_list));

	prop_list->version = 1;
	prop_list->refresh_min = VIOC_REFRESH_MIN;
	prop_list->refresh_max = VIOC_REFRESH_MAX;
	prop_list->flip = (1 << TCC_DRM_FLIP_NONE);
	prop_list->degree = (1 << TCC_DRM_DEGREE_0);
	prop_list->csc = 1;
	prop_list->crop = 1;
	prop_list->crop_max.hsize = VIOC_CROP_WIDTH_MAX;
	prop_list->crop_max.vsize = VIOC_CROP_HEIGHT_MAX;
	prop_list->crop_min.hsize = VIOC_CROP_WIDTH_MIN;
	prop_list->crop_min.vsize = VIOC_CROP_HEIGHT_MIN;
	prop_list->scale = 1;
	prop_list->scale_max.hsize = VIOC_SCALE_WIDTH_MAX;
	prop_list->scale_max.vsize = VIOC_SCALE_HEIGHT_MAX;
	prop_list->scale_min.hsize = VIOC_SCALE_WIDTH_MIN;
	prop_list->scale_min.vsize = VIOC_SCALE_HEIGHT_MIN;

	return 0;
}

static inline bool tcc_check_drm_flip(enum drm_tcc_flip flip)
{
	switch (flip) {
	case TCC_DRM_FLIP_NONE:
		return true;
	case TCC_DRM_FLIP_VERTICAL:
	case TCC_DRM_FLIP_HORIZONTAL:
	case TCC_DRM_FLIP_BOTH:
	default:
		DRM_DEBUG_KMS("invalid flip\n");
		return false;
	}
}

static inline bool tcc_check_drm_degree(enum drm_tcc_degree degree)
{
	switch (degree) {
	case TCC_DRM_DEGREE_0:
		return true;
	case TCC_DRM_DEGREE_90:
	case TCC_DRM_DEGREE_270:
	case TCC_DRM_DEGREE_180:
	default:
		DRM_ERROR("invalid degree.\n");
		return false;
	}
}

static int vioc_ippdrv_check_property(struct device *dev,
		struct drm_tcc_ipp_property *property)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_tcc_ipp_prop_list *pp = &ippdrv->prop_list;
	struct drm_tcc_ipp_config *config;
	struct drm_tcc_pos *pos;
	struct drm_tcc_sz *sz;
	int i;

	for_each_ipp_ops(i) {
		if ((i == TCC_DRM_OPS_SRC) &&
			(property->cmd == IPP_CMD_WB))
			continue;

		config = &property->config[i];
		pos = &config->pos;
		sz = &config->sz;

		/* check for flip */
		if (!tcc_check_drm_flip(config->flip)) {
			DRM_ERROR("invalid flip.\n");
			goto err_property;
		}

		/* check for degree */
		if (!tcc_check_drm_degree(config->degree)) {
			DRM_ERROR("invalid degree.\n");
			goto err_property;
		}

		/* check for buffer bound */
		if ((pos->x + pos->w > sz->hsize) ||
			(pos->y + pos->h > sz->vsize)) {
			DRM_ERROR("out of buf bound.\n");
			goto err_property;
		}

		/* check for crop */
		if ((i == TCC_DRM_OPS_SRC) && (pp->crop)) {
			if((pos->w < pp->crop_min.hsize) ||
				(sz->hsize > pp->crop_max.hsize) ||
				(pos->h < pp->crop_min.vsize) ||
				(sz->vsize > pp->crop_max.vsize)) {
				DRM_ERROR("out of crop size.\n");
				goto err_property;
			}
		}

		/* check for scale */
		if ((i == TCC_DRM_OPS_DST) && (pp->scale)) {
			if ((pos->w < pp->scale_min.hsize) ||
				(sz->hsize > pp->scale_max.hsize) ||
				(pos->h < pp->scale_min.vsize) ||
				(sz->vsize > pp->scale_max.vsize)) {
				DRM_ERROR("out of scale size.\n");
				goto err_property;
			}
		}
	}

	return 0;

err_property:
	for_each_ipp_ops(i) {
		if ((i == TCC_DRM_OPS_SRC) &&
			(property->cmd == IPP_CMD_WB))
			continue;

		config = &property->config[i];
		pos = &config->pos;
		sz = &config->sz;

		DRM_ERROR("[%s]f[%d]r[%d]pos[%d %d %d %d]sz[%d %d]\n",
			i ? "dst" : "src", config->flip, config->degree,
			pos->x, pos->y, pos->w, pos->h,
			sz->hsize, sz->vsize);
	}

	return -EINVAL;
}

static int vioc_ippdrv_reset(struct device *dev)
{
	struct vioc_context *ctx = get_vioc_context(dev);

	/* reset h/w block */
	vioc_sw_reset(ctx);

	/* reset buffer information */
	memset(&ctx->buf, 0x0, sizeof(ctx->buf) * TCC_DRM_OPS_MAX);
	return 0;
}

static int vioc_ippdrv_start(struct device *dev, enum drm_tcc_ipp_cmd cmd)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_tcc_ipp_cmd_node *c_node = ippdrv->c_node;
	struct vioc_blk_info *vioc = ctx->blk;
	int ret = 0;

	DRM_DEBUG_KMS("cmd[%d]\n", cmd);

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	switch (cmd) {
	case IPP_CMD_M2M:
		/*
		 * TODO
		 * if start sequence is needed, please add the codes
		 */
		iowrite32(ioread32((void *)(vioc->wdma->reg+0x48)) & ~(vioc->wdma->intr->bits & VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg+0x48));
		iowrite32((1 << (vioc->wdma->intr->id - 32)), (void *)(vioc->cfg_reg+0x24));
		break;
	case IPP_CMD_WB:
	case IPP_CMD_OUTPUT:
	default:
		ret = -EINVAL;
		DRM_ERROR("invalid operations %d.\n", cmd);
		return ret;
	}

	return ret;
}

static void vioc_ippdrv_stop(struct device *dev, enum drm_tcc_ipp_cmd cmd)
{
	struct vioc_context *ctx = get_vioc_context(dev);
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_tcc_ipp_cmd_node *c_node = ippdrv->c_node;
	struct vioc_blk_info *vioc = ctx->blk;

	DRM_DEBUG_KMS("cmd[%d]\n", cmd);

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return;
	}

	switch (cmd) {
	case IPP_CMD_M2M:
		/* disable wdma */
		VIOC_WDMA_SetImageDisable(vioc->wdma->reg);
		iowrite32((vioc->wdma->intr->bits&VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg + 0x44));
		break;
	case IPP_CMD_WB:
	case IPP_CMD_OUTPUT:
	default:
		DRM_ERROR("invalid operations %d.\n", cmd);
		break;
	}

	/* disable wdma interrupt */
	iowrite32(ioread32((void *)(vioc->wdma->reg+0x48)) | (vioc->wdma->intr->bits & VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg+0x48));
	if((ioread32((void *)(vioc->wdma->reg+0x48))&VIOC_WDMA_INT_MASK) == VIOC_WDMA_INT_MASK)
		iowrite32(1 << (vioc->wdma->intr->id - 32), (void *)(vioc->cfg_reg+0x24));
}

static int vioc_parse_dt(struct platform_device *pdev, struct vioc_context *ctx)
{
	struct vioc_blk_info *vioc = ctx->blk;
	struct device_node *np;

	if(ctx == NULL)
		return -EINVAL;

	ctx->clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(ctx->clk)) {
		ctx->clk = NULL;
		return -EINVAL;
	}

	if(pdev->dev.of_node == NULL) {
		DRM_ERROR("counld not find device node.\n");
		return -EINVAL;
	}

	vioc->rdma = devm_vioc_rdma_get(&pdev->dev, 0);
	if (IS_ERR(vioc->rdma)) {
		DRM_ERROR("could not find rdma node of %s driver. \n", pdev->name);
		return -EINVAL;
	}

	vioc->wmix = devm_vioc_wmix_get(&pdev->dev, 0);
	if (IS_ERR(vioc->wmix)) {
		DRM_ERROR("could not find wmix node of %s driver. \n", pdev->name);
		return -EINVAL;
	}

	vioc->sc = devm_vioc_sc_get(&pdev->dev, 0);
	if (IS_ERR(vioc->sc)) {
		DRM_ERROR("could not find sc node of %s driver. \n", pdev->name);
		return -EINVAL;
	}

	vioc->wdma = devm_vioc_wdma_get(&pdev->dev, 0);
	if (IS_ERR(vioc->wdma)) {
		DRM_ERROR("could not find wdma node of %s driver. \n", pdev->name);
		return -EINVAL;
	}

	np = of_find_compatible_node(NULL, NULL, "telechips,vioc_config");
	if (np) {
		vioc->cfg_reg = (void __iomem *)of_iomap(np, 0);
		if(vioc->cfg_reg == NULL) {
			DRM_ERROR("could not find config node of %s driver. \n", pdev->name);
			return -EINVAL;
		}
	} else {
		DRM_ERROR("%s: ioremap fail \n", pdev->name);
		return -EINVAL;
	}

	if (ctx->clk)
		clk_prepare_enable(ctx->clk);

	if(!vioc->irq_reged) {
#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
		// Don't STOP_REQ clear because of dual output problem(DISP FIFO under-run), when video is out.
		vioc_config_stop_req(0);
#else
		vioc_config_stop_req(1);
#endif
		synchronize_irq(vioc->wdma->irq);
		iowrite32((vioc->wdma->intr->bits&VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg + 0x44));
		if(request_irq(vioc->wdma->irq, vioc_ipp_handler, IRQF_SHARED, "drm_vioc", ctx)) {
			if (ctx->clk)
				clk_disable_unprepare(ctx->clk);

			DRM_ERROR("failed to aquire drm_vioc request_irq.\n");
			return -EFAULT;
		}
		iowrite32(ioread32((void *)(vioc->wdma->reg+0x48)) | (vioc->wdma->intr->bits & VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg+0x48));
		if((ioread32((void *)(vioc->wdma->reg+0x48))&VIOC_WDMA_INT_MASK) == VIOC_WDMA_INT_MASK)
			iowrite32(1 << (vioc->wdma->intr->id - 32), (void *)(vioc->cfg_reg+0x24));
		vioc->irq_reged = 1;
	}
	return 0;
}

static int vioc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vioc_context *ctx;
	struct tcc_drm_ippdrv *ippdrv;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "device tree node not found.\n");
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->blk = devm_kzalloc(dev, sizeof(struct vioc_blk_info), GFP_KERNEL);
	if(!ctx->blk)
		return -ENOMEM;

	ctx->ippdrv.dev = dev;
	ret = vioc_parse_dt(pdev, ctx);
	if (ret < 0) {
		dev_err(dev, "failed to parse device tree.\n");
		return ret;
	}

	ippdrv = &ctx->ippdrv;
	ippdrv->ops[TCC_DRM_OPS_SRC] = &vioc_src_ops;
	ippdrv->ops[TCC_DRM_OPS_DST] = &vioc_dst_ops;
	ippdrv->check_property = vioc_ippdrv_check_property;
	ippdrv->reset = vioc_ippdrv_reset;
	ippdrv->start = vioc_ippdrv_start;
	ippdrv->stop = vioc_ippdrv_stop;
	ret = vioc_init_prop_list(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to init property list.\n");
		goto err_vioc_probe;
	}

	DRM_DEBUG_KMS("ippdrv[0x%x]\n", (int)ippdrv);

	spin_lock_init(&ctx->lock);
	platform_set_drvdata(pdev, ctx);

	ret = tcc_drm_ippdrv_register(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to register drm vioc device.\n");
		goto err_vioc_probe;
	}

	dev_info(dev, "drm vioc registered successfully.\n");

	return 0;

err_vioc_probe:
	return ret;
}

static int vioc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vioc_context *ctx = get_vioc_context(dev);
	struct tcc_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct vioc_blk_info *vioc = ctx->blk;

	if(vioc->irq_reged) {
		iowrite32(ioread32((void *)(vioc->wdma->reg+0x48)) | (vioc->wdma->intr->bits & VIOC_WDMA_INT_MASK), (void *)(vioc->wdma->reg+0x48));
		if((ioread32((void *)(vioc->wdma->reg+0x48))&VIOC_WDMA_INT_MASK) == VIOC_WDMA_INT_MASK)
			iowrite32(1 << (vioc->wdma->intr->id - 32), (void *)(vioc->cfg_reg+0x24));
		free_irq(vioc->wdma->irq, ctx);
		vioc->irq_reged = 0;
	}

	if (ctx->clk)
		clk_disable_unprepare(ctx->clk);

	tcc_drm_ippdrv_unregister(ippdrv);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int vioc_suspend(struct device *dev)
{
	struct vioc_context *ctx = get_vioc_context(dev);

	if (pm_runtime_suspended(dev))
		return 0;

	if(ctx->clk)
		clk_disable_unprepare(ctx->clk);

	return 0;
}

static int vioc_resume(struct device *dev)
{
	struct vioc_context *ctx = get_vioc_context(dev);

	if (!pm_runtime_suspended(dev)) {
		if(ctx->clk)
			clk_prepare_enable(ctx->clk);
	}

	return 0;
}
#endif

#ifdef CONFIG_PM
static int vioc_runtime_suspend(struct device *dev)
{
	struct vioc_context *ctx = get_vioc_context(dev);

	if(ctx->clk)
		clk_disable_unprepare(ctx->clk);

	return  0;
}

static int vioc_runtime_resume(struct device *dev)
{
	struct vioc_context *ctx = get_vioc_context(dev);

	if(ctx->clk)
		clk_prepare_enable(ctx->clk);

	return 0;
}
#endif

static const struct dev_pm_ops vioc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(vioc_suspend, vioc_resume)
	SET_RUNTIME_PM_OPS(vioc_runtime_suspend, vioc_runtime_resume, NULL)
};

static const struct of_device_id vioc_of_match[] = {
	{ .compatible = "telechips,tcc-drm-vioc" },
	{ },
};
MODULE_DEVICE_TABLE(of, vioc_of_match);

struct platform_driver vioc_driver = {
	.probe		= vioc_probe,
	.remove		= vioc_remove,
	.driver		= {
		.of_match_table = vioc_of_match,
		.name	= "tcc-drm-vioc",
		.owner	= THIS_MODULE,
		.pm = &vioc_pm_ops,
	},
};
