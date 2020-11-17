/*
 *      tccvin_video.c  --  Video handling
 *
 *      Copyright (c) 2020-
 *          Telechips Inc.
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      SPDX-license-Identifier : GPL-2.0+
 *
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>
#include <soc/tcc/pmap.h>

#include "tccvin_video.h"

/* ------------------------------------------------------------------------
 * Video formats
 */

struct tccvin_format_desc tccvin_format_list[] = {
	/* RGB */
	{
		.name		= "RGB 8:8:8 (RGB3)",
		// B1[31:24] R[23:16] G[15:8] B0[7:0]
		.guid		= VIOC_IMG_FMT_RGB888,
		// 'RBG3' 24 RGB-8-8-8
		.fcc		= V4L2_PIX_FMT_RGB24,
		.bpp		= 24,
	},
	{
		.name		= "ARGB 8:8:8:8 (RGB4)",
		// A[31:24] R[23:16] G[15:8] B[7:0]
		.guid		= VIOC_IMG_FMT_ARGB8888,
		// 'RGB4' 32 RGB-8-8-8-8
		.fcc		= V4L2_PIX_FMT_RGB32,
		.bpp		= 32,
	},

	/* sequential (YUV packed) */
	{
		.name		= "YUV 4:2:2 (UYVY)",
		// LSB [Y/U/Y/V] MSB : YCbCr 4:2:2 sequential
		.guid		= VIOC_IMG_FMT_UYVY,
		// 'UYVY' 16 YUV 4:2:2
		.fcc		= V4L2_PIX_FMT_UYVY,
		.bpp		= 16,
	},
	{
		.name		= "YUV 4:2:2 (VYUY)",
		// LSB [Y/V/Y/U] MSB : YCbCr 4:2:2 sequential
		.guid		= VIOC_IMG_FMT_VYUY,
		// 'VYUY' 16 YUV 4:2:2
		.fcc		= V4L2_PIX_FMT_VYUY,
		.bpp		= 16,
	},
	{
		.name		= "YUV 4:2:2 (YUYV)",
		// LSB [Y/U/Y/V] MSB : YCbCr 4:2:2 sequential
		.guid		= VIOC_IMG_FMT_YUYV,
		// 'YUYV' 16 YUV 4:2:2
		.fcc		= V4L2_PIX_FMT_YUYV,
		.bpp		= 16,
	},
	{
		.name		= "YUV 4:2:2 (YVYU)",
		// LSB [Y/V/Y/U] MSB : YCbCr 4:2:2 sequential
		.guid		= VIOC_IMG_FMT_YVYU,
		// 'YVYU' 16 YVU 4:2:2
		.fcc		= V4L2_PIX_FMT_YVYU,
		.bpp		= 16,
	},

	/* sepatated (Y, U, V planar) */
	{
		.name		= "YVU 4:2:0 (YV12)",
		// YCbCr 4:2:0 separated
		.guid		= VIOC_IMG_FMT_YUV420SEP,
		// 'YV12' 12 YVU 4:2:0
		.fcc		= V4L2_PIX_FMT_YVU420,
		.bpp		= 12,
	},
	{
		.name		= "YUV 4:2:0 (YU12)",
		// YCbCr 4:2:0 separated
		.guid		= VIOC_IMG_FMT_YUV420SEP,
		// 'YU12' 12 YUV 4:2:0
		.fcc		= V4L2_PIX_FMT_YUV420,
		.bpp		= 12,
	},
	{
		.name		= "YUV 4:2:2 (422P)",
		// YCbCr 4:2:2 separated
		.guid		= VIOC_IMG_FMT_YUV422SEP,
		// '422P' 16 YVU422 Planar
		.fcc		= V4L2_PIX_FMT_YUV422P,
		.bpp		= 16,
	},

	/* interleaved (Y palnar, UV planar) */
	{
		.name		= "YUV 4:2:0 (NV12)",
		// YCbCr 4:2:0 interleaved type0
		.guid		= VIOC_IMG_FMT_YUV420IL0,
		// 'NV12' 12 Y/CbCr 4:2:0
		.fcc		= V4L2_PIX_FMT_NV12,
		.bpp		= 12,
	},
	{
		.name		= "YUV 4:2:0 (NV21)",
		// YCbCr 4:2:0 interleaved type1
		.guid		= VIOC_IMG_FMT_YUV420IL1,
		// 'NV21' 12 Y/CrCb 4:2:0
		.fcc		= V4L2_PIX_FMT_NV21,
		.bpp		= 12,
	},
	{
		.name		= "YUV 4:2:0 (NV16)",
		// YCbCr 4:2:2 interleaved type0
		.guid		= VIOC_IMG_FMT_YUV422IL0,
		// 'NV16' 16 Y/CbCr 4:2:2
		.fcc		= V4L2_PIX_FMT_NV16,
		.bpp		= 12,
	},
	{
		.name		= "YUV 4:2:0 (NV61)",
		// YCbCr 4:2:2 interleaved type1
		.guid		= VIOC_IMG_FMT_YUV422IL1,
		// 'NV61' 16 Y/CrCb 4:2:2
		.fcc		= V4L2_PIX_FMT_NV61,
		.bpp		= 12,
	},
};

struct framesize tccvin_framesize_list[] = {
	{	 320,	 240	},
	{	 640,	 480	},
	{	 720,	 480	},
	{	1280,	 720	},
	{	1920,	 720	},
	{	1920,	1080	},
};

u32 tccvin_framerate_list[] = {
	15,
	30,
};

/* ------------------------------------------------------------------------
 * Utility functions
 */

int tccvin_count_supported_formats(void)
{
	return ARRAY_SIZE(tccvin_format_list);
}

struct tccvin_format_desc * tccvin_format_by_index(int index)
{
	return &tccvin_format_list[index];
}

int tccvin_count_supported_framesizes(void)
{
	return ARRAY_SIZE(tccvin_framesize_list);
}

struct framesize * tccvin_framesize_by_index(int index)
{
	return &tccvin_framesize_list[index];
}

int tccvin_count_supported_framerates(void)
{
	return ARRAY_SIZE(tccvin_framerate_list);
}

int tccvin_framerate_by_index(int index)
{
	return tccvin_framerate_list[index];
}

struct tccvin_format_desc *tccvin_format_by_guid(const __u32 guid)
{
	unsigned int len = ARRAY_SIZE(tccvin_format_list);
	unsigned int i;

	for (i = 0; i < len; ++i) {
		if (guid == tccvin_format_list[i].guid) {
			return &tccvin_format_list[i];
		}
	}

	return NULL;
}

struct tccvin_format_desc *tccvin_format_by_fcc(const __u32 fcc)
{
	unsigned int len = ARRAY_SIZE(tccvin_format_list);
	unsigned int i;

	for (i = 0; i < len; ++i) {
		if (fcc == tccvin_format_list[i].fcc) {
			return &tccvin_format_list[i];
		}
	}

	return NULL;
}

int tccvin_convert_to_multi_planes_buffer_addresses(
	unsigned int width, unsigned int height, unsigned int fcc,
	unsigned long *addr0, unsigned long *addr1, unsigned long *addr2)
{
	unsigned long	addr0len	= 0;
	unsigned long	addr1len	= 0;
	int		ret		= 0;

	switch (fcc) {
	/* sepatated (Y, U, V planar) */
	case V4L2_PIX_FMT_YVU420:	// 'YV12' 12 YVU 4:2:0
#if 0
		addr0len	= width * height;
		addr1len	= (width / 2) * (height / 2);
		*addr2		= *addr0 + addr0len;
		*addr1		= *addr2 + addr1len;
		break;
#endif
	case V4L2_PIX_FMT_YUV420:	// 'YU12' 12 YUV 4:2:0
		addr0len	= width * height;
		addr1len	= (width / 2) * (height / 2);
		*addr1		= *addr0 + addr0len;
		*addr2		= *addr1 + addr1len;
		break;
	case V4L2_PIX_FMT_YUV422P:	// '422P' 16 YVU422 Planar
		break;

	/* interleaved (Y palnar, UV planar) */
	case V4L2_PIX_FMT_NV12:		// 'NV12' 12 Y/CbCr 4:2:0
	case V4L2_PIX_FMT_NV21:		// 'NV21' 12 Y/CrCb 4:2:0
	case V4L2_PIX_FMT_NV16:		// 'NV16' 16 Y/CbCr 4:2:2
	case V4L2_PIX_FMT_NV61:		// 'NV61' 16 Y/CrCb 4:2:2
		addr0len	= width * height;
		*addr1		= *addr0 + addr0len;
		*addr2		= 0;
		break;

	default:
		*addr1		= 0;
		*addr2		= 0;
		break;
	}

	return ret;
}

/*
 * tccvin_parse_device_tree
 *
 * - DESCRIPTION:
 *	Parse video-input path's device tree
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:			Success
 *	-ENODEV:	a certain device node is not found.
 */
static int tccvin_parse_device_tree(struct tccvin_streaming *vdev)
{
	struct device_node	*main_node	= vdev->dev->pdev->dev.of_node;
	struct device_node	*vsrc_node	= NULL;
	struct device_node	*vioc_node	= NULL;
	struct vioc_path	*vioc_path	= NULL;
	void __iomem		*address	= NULL;
	unsigned int		vioc_id		= 0;

	if (main_node == NULL) {
		logd("tcc_camera device node is not found.\n");
		return -ENODEV;
	}

	vsrc_node = of_parse_phandle(main_node, "videosource", 0);
	if (vsrc_node != NULL) {
		struct device_node *i2c_node = of_get_parent(vsrc_node);
		int addr;
		struct property *pp = NULL;
		char *strtoken = "i2c";

		if (i2c_node) {
			of_property_read_u32(vsrc_node, "reg", &addr);
			for_each_property_of_node(of_aliases, pp) {
				if (!strcmp(pp->value, i2c_node->full_name)) {
					sprintf(vdev->dev->subdev_name,
						"%s-%04x",
						strstr(pp->name, strtoken) +
						strlen(strtoken), addr);
					logd("subdev name: %s",
						vdev->dev->subdev_name);
				}
			}
		} else {
			loge("Failed to i2c node\n");
		}

		logd("save videosource i2c value(%s) to tccvindev\n",
			vdev->dev->subdev_name);
	} else {
		loge("\"videosource\" node is not found.\n");
//		return -ENODEV;
	}

	vioc_path = &vdev->cif.vioc_path;

	// VIN
	vioc_path->vin = -1;
	vioc_node = of_parse_phandle(main_node, "vin", 0);
	if (vioc_node != NULL) {
		of_property_read_u32_index(main_node,
			"vin", 1, &vioc_path->vin);
		if (vioc_path->vin != -1) {
			address	= VIOC_VIN_GetAddress(vioc_path->vin);
			vioc_id	= get_vioc_index(vioc_path->vin) / 2;
			logd("%10s[%2d]: 0x%p\n", "VIN", vioc_id, address);
		}
	} else {
		loge("\"vin\" node is not found.\n");
		return -ENODEV;
	}

	// cif port
	vdev->cif.cif_port = -1;
	vioc_node = of_parse_phandle(main_node, "cifport", 0);
	if (vioc_node != NULL) {
		of_property_read_u32_index(main_node,
			"cifport", 1, &vdev->cif.cif_port);
		if (vdev->cif.cif_port != -1) {
			vdev->cif.cifport_addr = of_iomap(vioc_node, 0);
			logd("%10s[%2d]: 0x%p\n", "CIF Port",
				vdev->cif.cif_port, vdev->cif.cifport_addr);
		}
	} else {
		loge("The CIF port node is NULL\n");
//		return -ENODEV;
	}

#ifdef CONFIG_OVERLAY_PGL
	// Parking Guide Line
	of_property_read_u32_index(main_node, "use_pgl", 0, &vdev->cif.use_pgl);
	dlog("%10s[%2d]: %d\n", "usage status pgl", vdev->vid_dev.minor,
		vdev->cif.use_pgl);

	vioc_path->pgl = -1;
	// VIDEO_IN04~06 don't have RDMA
	if (vioc_path->vin >= VIOC_VIN00 && vioc_path->vin <= VIOC_VIN30) {
		vioc_node = of_parse_phandle(main_node, "rdma", 0);
		if (vioc_node != NULL) {
			of_property_read_u32_index(main_node,
				"rdma", 1, &vioc_path->pgl);
			if (vioc_path->pgl != -1) {
				address	= VIOC_RDMA_GetAddress(vioc_path->pgl);
				vioc_id	= get_vioc_index(vioc_path->pgl);
				logd("%10s[%2d]: 0x%p\n", "RDMA(PGL)",
					vioc_id, address);
			}
		} else {
			logw("\"rdma\" node is not found.\n");
			return -ENODEV;
		}
	}
#endif//CONFIG_OVERLAY_PGL

	// VIQE
	vioc_path->viqe = -1;
	vioc_path->deintl_s = -1;
	vioc_node = of_parse_phandle(main_node, "viqe", 0);
	if (vioc_node) {
		of_property_read_u32_index(main_node,
			"viqe", 1, &vioc_path->viqe);
		if (vioc_path->viqe != -1) {
			address	= VIOC_VIQE_GetAddress(vioc_path->viqe);
			vioc_id	= get_vioc_index(vioc_path->viqe);
			logd("%10s[%2d]: 0x%p\n", "VIQE", vioc_id, address);
		}
	} else {
		logw("\"viqe\" node is not found.\n");

		// DEINTL_S
		vioc_node = of_parse_phandle(main_node, "deintls", 0);
		if (vioc_node) {
			of_property_read_u32_index(main_node,
				"deintls", 1, &vioc_path->deintl_s);
			if (vioc_path->deintl_s != -1) {
				address	= VIOC_DEINTLS_GetAddress(
					/*vioc_path->deintl_s*/);
				vioc_id	= get_vioc_index(vioc_path->deintl_s);
				logd("%10s[%2d]: 0x%p\n", "DEINTL_S",
					vioc_id, address);
			}
		} else {
			logw("\"deintls\" node is not found.\n");
		}
	}

	// SCALER
	vioc_path->scaler = -1;
	vioc_node = of_parse_phandle(main_node, "scaler", 0);
	if (vioc_node != NULL) {
		of_property_read_u32_index(main_node,
			"scaler", 1, &vioc_path->scaler);
		if (vioc_path->scaler != -1) {
			address	= VIOC_SC_GetAddress(vioc_path->scaler);
			vioc_id	= get_vioc_index(vioc_path->scaler);
			logd("%10s[%2d]: 0x%p\n", "SCALER", vioc_id, address);
		}
	} else {
		logw("\"scaler\" node is not found.\n");
	}

	// WMIXER
	vioc_path->wmixer = -1;
	vioc_node = of_parse_phandle(main_node, "wmixer", 0);
	if (vioc_node != NULL) {
		of_property_read_u32_index(main_node,
			"wmixer", 1, &vioc_path->wmixer);
		if (vioc_path->wmixer != -1) {
			address	= VIOC_WMIX_GetAddress(vioc_path->wmixer);
			vioc_id	= get_vioc_index(vioc_path->wmixer);
			logd("%10s[%2d]: 0x%p\n", "WMIXER", vioc_id, address);
		}
	} else {
		logw("\"wmixer\" node is not found.\n");
	}

	// WDMA
	vioc_path->wdma = -1;
	vdev->cif.vioc_irq_num   = -1;
	vioc_node = of_parse_phandle(main_node, "wdma", 0);
	if (vioc_node != NULL) {
		of_property_read_u32_index(main_node,
			 "wdma", 1, &vioc_path->wdma);
		if (vioc_path->wdma != -1) {
			address = VIOC_WDMA_GetAddress(vioc_path->wdma);
			vioc_id	= get_vioc_index(vioc_path->wdma);
			logd("%10s[%2d]: 0x%p\n", "WDMA", vioc_id, address);

			// WDMA interrupt
			vdev->cif.vioc_irq_num =
				irq_of_parse_and_map(vioc_node, vioc_id);
			logd("irq_num: %d\n", vdev->cif.vioc_irq_num);
		}
	} else {
		loge("\"wdma\" node is not found.\n");
		return -ENODEV;
	}

	return 0;
}

/*
 * tccvin_reset_vioc_path
 *
 * - DESCRIPTION:
 *	Reset or clear a certain vioc component.
 *	If vioc manager is enabled, try to ask to main core to reset or clear.
 *	If no response comes from main core, do it directly.
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-1:		The vioc manager device is not found.
 */
static int tccvin_reset_vioc_path(struct tccvin_streaming *vdev)
{
	struct vioc_path	*vioc	= &vdev->cif.vioc_path;
	int			viocs[] = {
#ifdef CONFIG_OVERLAY_PGL
		vioc->pgl,
#endif
		vioc->vin,
		vioc->viqe,
		vioc->deintl_s,
		vioc->scaler,
		vioc->wmixer,
		vioc->wdma,
	};
	int			idxVioc = 0, nVioc = 0;
#if 0//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	struct file		*file		= NULL;
	char			name[1024];
	struct tcc_mbox_data	data;
#endif
	int						ret = 0;

	nVioc = ARRAY_SIZE(viocs);

#if 0//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	// open video-input driver
	sprintf(name, "/dev/vioc_mgr_a7s");

	file = filp_open(name, O_RDWR, 0666);
	if (file == NULL) {
		logi("error: driver open fail (%s)\n", name);
		return -1;
	}

	// reset
	for (idxVioc = nVioc - 1; idxVioc >= 0; idxVioc--) {
		if (viocs[idxVioc] != -1) {
			memset(&data, 0x0, sizeof(struct tcc_mbox_data));
			data.cmd[1] = (VIOC_CMD_RESET & 0xFFFF) << 16;
			data.data[0] = (unsigned int)viocs[idxVioc];
			data.data[1] = VIOC_CONFIG_RESET;
			data.data_len = 2;

			ret = file->f_op->unlocked_ioctl(file,
				IOCTL_VIOC_MGR_SET_RESET_KERNEL,
				(unsigned long)&data);
			if (ret <= 0) {
				loge("[%d] Reset\n", idxVioc);
				goto err_viocmg;
			}
		}
	}

	// reset clear
	for (idxVioc = 0; idxVioc < nVioc; idxVioc++) {
		if (viocs[idxVioc] != -1) {
			memset(&data, 0x0, sizeof(struct tcc_mbox_data));
			data.cmd[1] = (VIOC_CMD_RESET & 0xFFFF) << 16;
			data.data[0] = (unsigned int)viocs[idxVioc];
			data.data[1] = VIOC_CONFIG_CLEAR;
			data.data_len = 2;

			ret = file->f_op->unlocked_ioctl(file,
				IOCTL_VIOC_MGR_SET_RESET_KERNEL,
				(unsigned long)&data);
			if (ret <= 0) {
				loge("[%d] Reset - Clear\n", idxVioc);
				goto err_viocmg;
			}
		}
	}

	// Close the video input device
	filp_close(file, 0);
	goto end;

err_viocmg:
#endif//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)

	// reset
	for (idxVioc = nVioc - 1; idxVioc >= 0; idxVioc--) {
		if (viocs[idxVioc] != -1) {
			VIOC_CONFIG_SWReset_RAW(
				(unsigned int)viocs[idxVioc],
				VIOC_CONFIG_RESET);
		}
	}

	// reset clear
	for (idxVioc = 0; idxVioc < nVioc; idxVioc++) {
		if (viocs[idxVioc] != -1) {
			VIOC_CONFIG_SWReset_RAW(
				(unsigned int)viocs[idxVioc],
				VIOC_CONFIG_CLEAR);
		}
	}

#if 0//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
end:
#endif//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	return ret;
}

/*
 * tccvin_map_cif_port
 *
 * - DESCRIPTION:
 *	Map cif port to receive video data
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_map_cif_port(struct tccvin_streaming *vdev)
{
	unsigned int	vin_index	=
		(get_vioc_index(vdev->cif.vioc_path.vin) / 2);
	unsigned int	value		= 0;

	value = ((__raw_readl(vdev->cif.cifport_addr) &
		~(0xF << (vin_index * 4))) |
		(vdev->cif.cif_port << (vin_index * 4)));
	__raw_writel(value, vdev->cif.cifport_addr);

	value = __raw_readl(vdev->cif.cifport_addr);
	logd("CIF Port: %d, VIN Index: %d, Register Value: 0x%08x\n",
		vdev->cif.cif_port, vin_index, value);

	return 0;
}

#if 0
/*
 * tccvin_check_cif_port_mapping
 *
 * - DESCRIPTION:
 *	Check cif port mapping to receive video data
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 * -1:		Failure
 */
static int tccvin_check_cif_port_mapping(struct tccvin_streaming *vdev)
{
	unsigned int	vin_index	=
		(get_vioc_index(vdev->cif.vioc_path.vin) / 2);
	unsigned int	cif_port	= 0;
	int		ret		= 0;

	cif_port = ((__raw_readl(vdev->cif.cifport_addr) >>
		(vin_index * 4)) & (0xF));
	if (cif_port != vdev->cif.cif_port) {
		loge("CIF Port: %d / %d, VIN Index: %d\n",
			cif_port, vdev->cif.cif_port, vin_index);
		ret = -1;
	}

	return ret;
}
#endif

#if defined(CONFIG_OVERLAY_PGL) && !defined(CONFIG_OVERLAY_DPGL)
/*
 * tccvin_set_pgl
 *
 * - DESCRIPTION:
 *	Set rdma component to read parking guideline image
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
int tccvin_set_pgl(struct tccvin_streaming *vdev)
{
	void __iomem	*rdma		=
		VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);

	unsigned int	width		= vdev->cur_frame->wWidth;
	unsigned int	height		= vdev->cur_frame->wHeight;
	unsigned int	format		= PGL_FORMAT;
	unsigned int	buf_addr	= vdev->cif.pmap_pgl.base;

	loge("RDMA: 0x%p, size[%d x %d], format[%d]\n",
		rdma, width, height, format);

	VIOC_RDMA_SetImageFormat(rdma, format);
	VIOC_RDMA_SetImageSize(rdma, width, height);
	VIOC_RDMA_SetImageOffset(rdma, format, width);
	VIOC_RDMA_SetImageBase(rdma, buf_addr, 0, 0);
	if (vdev->cif.use_pgl == 1) {
		VIOC_RDMA_SetImageEnable(rdma);
		VIOC_RDMA_SetImageUpdate(rdma);
	} else {
		VIOC_RDMA_SetImageDisable(rdma);
	}

	return 0;
}
#endif//CONFIG_OVERLAY_PGL

/*
 * tccvin_set_vin
 *
 * - DESCRIPTION:
 *	Set vin component to receive video data via cif port
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_set_vin(struct tccvin_streaming *vdev)
{
	void __iomem		*vin		=
		VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin);

	videosource_format_t	*vs_info	= vdev->cif.videosource_info;

	unsigned int	data_order	= vs_info->data_order;
	unsigned int	data_format	= vs_info->data_format;
	unsigned int	gen_field_en	= vs_info->gen_field_en;
	unsigned int	de_low		= vs_info->de_active_low;
	unsigned int	field_low	= vs_info->field_bfield_low;
	unsigned int	vs_low		=
		!!(vs_info->polarities & V4L2_DV_VSYNC_POS_POL);
	unsigned int	hs_low		=
		!!(vs_info->polarities & V4L2_DV_HSYNC_POS_POL);
	unsigned int	pxclk_pol	=
		!!(vs_info->polarities & V4L2_DV_PCLK_POS_POL);
	unsigned int	vs_mask		= vs_info->vs_mask;
	unsigned int	hsde_connect_en	= vs_info->hsde_connect_en;
	unsigned int	intpl_en	= vs_info->intpl_en;
	unsigned int	conv_en		= vs_info->conv_en;
	unsigned int	interlaced	=
		!!(vs_info->interlaced & V4L2_DV_INTERLACED);
	unsigned int	width		= vs_info->width;
	unsigned int	height		= vs_info->height >> interlaced;
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	unsigned int	stream_enable	= vs_info->se;
	unsigned int	flush_vsync	= vs_info->fvs;
#endif//defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)

	logd("VIN: 0x%p, Source Size - width: %d, height: %d\n",
		vin, width, height);

	logd("data_order:	%d\n", data_order);
	logd("data_format:	%d\n", data_format);
	logd("stream_enable:	%d\n", stream_enable);
	logd("gen_field_en:	%d\n", gen_field_en);
	logd("de_low:	%d\n", de_low);
	logd("vs_mask:		%d\n", vs_mask);
	logd("hsde_connect_en:	%d\n", hsde_connect_en);
	logd("intpl_en:		%d\n", intpl_en);
	logd("interlaced:	%d\n", interlaced);
	logd("conv_en:		%d\n", conv_en);
	logd("flush_vsync:	%d\n", flush_vsync);

	VIOC_VIN_SetSyncPolarity(vin, hs_low, vs_low,
		field_low, de_low, gen_field_en, pxclk_pol);
	VIOC_VIN_SetCtrl(vin, conv_en, hsde_connect_en, vs_mask,
		data_format, data_order);
	VIOC_VIN_SetInterlaceMode(vin, interlaced, intpl_en);
	VIOC_VIN_SetImageSize(vin, width, height);
	VIOC_VIN_SetImageOffset(vin, 0, 0, 0);
	VIOC_VIN_SetImageCropSize(vin, width, height);
	VIOC_VIN_SetImageCropOffset(vin, 0, 0);
	VIOC_VIN_SetY2RMode(vin, 2);

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
    defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	VIOC_VIN_SetSEEnable(vin, stream_enable);
	VIOC_VIN_SetFlushBufferEnable(vin, flush_vsync);
#endif

	logd("v4l2 preview format(fcc): 0x%08x\n", vdev->cur_format->fcc);
	if (((data_format == FMT_YUV422_16BIT)	||
	    (data_format == FMT_YUV422_8BIT)	||
	    (data_format == FMT_YUVK4444_16BIT)	||
	    (data_format == FMT_YUVK4224_24BIT)) &&
		((vdev->cur_format->fcc == V4L2_PIX_FMT_RGB24) ||
		 (vdev->cur_format->fcc == V4L2_PIX_FMT_RGB32))) {
		if (!((interlaced) && (vdev->cif.vioc_path.viqe != -1))) {
			logd("y2r is ENABLED\n");
			VIOC_VIN_SetY2REnable(vin, ON);
		}
	} else {
		logd("y2r is DISABLED\n");
		VIOC_VIN_SetY2REnable(vin, OFF);
	}

	VIOC_VIN_SetEnable(vin, ON);

	return 0;
}

/*
 * tccvin_set_deinterlacer
 *
 * - DESCRIPTION:
 *	Set viqe component to deinterlace interlaced video data
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_set_deinterlacer(struct tccvin_streaming *vdev)
{
	struct vioc_path	*vioc		= &vdev->cif.vioc_path;
	int			ret		= 0;

	if (vdev->cif.vioc_path.viqe != -1) {
		void __iomem		*viqe		=
			VIOC_VIQE_GetAddress(vioc->viqe);

		u32	interlaced	=
			!!(vdev->cif.videosource_info->interlaced &
			V4L2_DV_INTERLACED);
		u32	width		= vdev->cif.videosource_info->width;
		u32	height		= vdev->cif.videosource_info->height >>
			interlaced;

		u32	viqe_width	= 0;
		u32	viqe_height	= 0;
		u32	format		= VIOC_VIQE_FMT_YUV422;
		u32	bypass_deintl	= VIOC_VIQE_DEINTL_MODE_3D;
		u32	offset		= width * height * 2 * 2;
		u32	deintl_base0	= vdev->cif.pmap_viqe.base;
		u32	deintl_base1	= deintl_base0 + offset;
		u32	deintl_base2	= deintl_base1 + offset;
		u32	deintl_base3	= deintl_base2 + offset;

		u32	cdf_lut_en	= OFF;
		u32	his_en		= OFF;
		u32	gamut_en	= OFF;
		u32	d3d_en		= OFF;
		u32	deintl_en	= ON;

//		struct device_node	*node		= NULL;
//		u32	*viqe_set_reg1	= NULL;
//		u32	*viqe_set_reg2	= NULL;

		u32	vioc_format	=
			vdev->cif.videosource_info->data_format;
		u32	v4l2_format	= vdev->cur_format->fcc;

		logd("VIQE: 0x%p, Source Size - width: %d, height: %d\n",
			viqe, width, height);

#if 0
		hdl_np = of_parse_phandle(vdev->dev->pdev->dev.of_node,
			"viqe_set", 0);
		if (!hdl_np) {
			loge("could not find cam_viqe_set node!!\n");
		} else {
			viqe_set_reg1 = (unsigned int *)of_iomap(hdl_np, 0);
			viqe_set_reg2 = (unsigned int *)of_iomap(hdl_np, 1);

			BITCSET(*viqe_set_reg1, 1<<3, 1<<3);
			BITCSET(*viqe_set_reg2, 1<<8 | 1<<9, 1<<8 | 1<<9);
		}
#endif

		if (vioc->vin <= VIOC_VIN30) {
			VIOC_CONFIG_PlugIn(vioc->viqe, vioc->vin);

			if (((vioc_format == FMT_YUV422_16BIT) ||
			     (vioc_format == FMT_YUV422_8BIT) ||
			     (vioc_format == FMT_YUVK4444_16BIT) ||
			     (vioc_format == FMT_YUVK4224_24BIT)) &&
				((v4l2_format == V4L2_PIX_FMT_RGB24) ||
				 (v4l2_format == V4L2_PIX_FMT_RGB32))) {
				VIOC_VIQE_SetImageY2RMode(viqe, 2);
				VIOC_VIQE_SetImageY2REnable(viqe, ON);
			}
			VIOC_VIQE_SetControlRegister(viqe,
				viqe_width, viqe_height, format);
			VIOC_VIQE_SetDeintlRegister(viqe, format, OFF,
				viqe_width, viqe_height, bypass_deintl,
				deintl_base0, deintl_base1,
				deintl_base2, deintl_base3);
			VIOC_VIQE_SetControlEnable(viqe, cdf_lut_en, his_en,
				gamut_en, d3d_en, deintl_en);
			VIOC_VIQE_SetDeintlModeWeave(viqe);
		}
	} else if (vioc->deintl_s != -1) {
		if (vioc->vin <= VIOC_VIN30) {
			VIOC_CONFIG_PlugIn(vioc->deintl_s, vioc->vin);
		}
	} else {
		logi("There is no available deinterlacer\n");
		ret = -1;
	}

	return ret;
}

/*
 * tccvin_set_scaler
 *
 * - DESCRIPTION:
 *	Set sclaer component to scale up or down
 *
 * - PARAMETERS:
 *	@vdev:			video-input path device's data
 *	@format:		destination format
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_set_scaler(struct tccvin_streaming *vdev,
	struct tccvin_frame *frame)
{
	struct tccvin_cif	*cif		= &vdev->cif;
	void __iomem		*sc		=
		VIOC_SC_GetAddress(cif->vioc_path.scaler);

	unsigned int	width	= (vdev->selection.r.width  != 0) ? vdev->selection.r.width  : frame->wWidth;
	unsigned int	height	= (vdev->selection.r.height != 0) ? vdev->selection.r.height : frame->wHeight;

	logd("SC: 0x%p, Output Size - width: %d, height: %d\n",
		sc, width, height);

	// Plug the scaler in
	VIOC_CONFIG_PlugIn(cif->vioc_path.scaler, cif->vioc_path.vin);

	// Configure the scaler
	VIOC_SC_SetBypass(sc, OFF);
	VIOC_SC_SetDstSize(sc, width, height);
	VIOC_SC_SetOutPosition(sc, 0, 0);
	// workaround: scaler margin
	VIOC_SC_SetOutSize(sc, width, height + 1);
	VIOC_SC_SetUpdate(sc);

	return 0;
}

/*
 * tccvin_set_wmixer
 *
 * - DESCRIPTION:
 *	Set video-input wmixer component to mix two or more video data
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_set_wmixer(struct tccvin_streaming *vdev)
{
	void __iomem	*wmixer	=
		VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer);

	unsigned int	width	= vdev->cur_frame->wWidth;
	unsigned int	height	= vdev->cur_frame->wHeight;
#ifdef CONFIG_OVERLAY_PGL
	unsigned int	layer	= 0x0;
	unsigned int	key_R	= PGL_BG_R;
	unsigned int	key_G	= PGL_BG_G;
	unsigned int	key_B	= PGL_BG_B;
	unsigned int	mask_R	= ((PGL_BGM_R >> 3) << 3);
	unsigned int	mask_G	= ((PGL_BGM_G >> 3) << 3);
	unsigned int	mask_B	= ((PGL_BGM_B >> 3) << 3);
#endif//CONFIG_OVERLAY_PGL

	logd("WMIXer: 0x%p, Size - width: %d, height: %d\n",
		wmixer, width, height);

	// Configure the wmixer
	VIOC_WMIX_SetSize(wmixer, width, height);
	VIOC_WMIX_SetPosition(wmixer, 0,
		vdev->selection.r.left, vdev->selection.r.top);
#ifdef CONFIG_OVERLAY_PGL
	VIOC_WMIX_SetPosition(wmixer, 1, 0, 0);
	VIOC_WMIX_SetChromaKey(wmixer, layer, ON, key_R, key_G, key_B,
		mask_R, mask_G, mask_B);
#endif
	VIOC_WMIX_SetUpdate(wmixer);
	VIOC_CONFIG_WMIXPath(vdev->cif.vioc_path.vin, ON);

	return 0;
}

/*
 * tccvin_set_wdma
 *
 * - DESCRIPTION:
 *	Set wdma component to write video data
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_set_wdma(struct tccvin_streaming *vdev)
{
	struct tccvin_video_queue	*queue		= &vdev->queue;
	struct tccvin_cif		*cif		= &vdev->cif;
	struct tccvin_buffer		*buf		= NULL;
	unsigned long			flags;

	void __iomem			*wdma		=
		VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);

	unsigned int			width		=
		vdev->cur_frame->wWidth;
	unsigned int			height		=
		vdev->cur_frame->wHeight;
	struct tccvin_format_desc	*format_desc	=
		tccvin_format_by_fcc(vdev->cur_format->fcc);
	unsigned int			format		= format_desc->guid;
	unsigned long			addr0		= 0;
	unsigned long			addr1		= 0;
	unsigned long			addr2		= 0;

	logd("WDMA: 0x%p, size[%d x %d], format[%d]\n",
		wdma, width, height, format);

	VIOC_WDMA_SetImageFormat(wdma, format);
	VIOC_WDMA_SetImageSize(wdma, width, height);
	VIOC_WDMA_SetImageOffset(wdma, format, width);

	if (vdev->preview_method == PREVIEW_V4L2) {
		spin_lock_irqsave(&queue->irqlock, flags);
		if (!list_empty(&queue->irqqueue)) {
			buf = list_first_entry(&queue->irqqueue,
				struct tccvin_buffer, queue);
		}
		spin_unlock_irqrestore(&queue->irqlock, flags);

		mutex_lock(&cif->lock);
		if (buf != NULL) {
			switch (buf->buf.vb2_buf.memory) {
			case VB2_MEMORY_MMAP:
				addr0 = buf->buf.vb2_buf.planes[0].m.offset;
				break;
			case VB2_MEMORY_USERPTR:
				addr0 = virt_to_phys((void *)
					buf->buf.vb2_buf.planes[0].m.userptr);
				break;
			default:
				loge("memory type is not supported\n");
				return -1;
			}
			tccvin_convert_to_multi_planes_buffer_addresses(
				width, height, vdev->cur_format->fcc,
				&addr0, &addr1, &addr2);
			logd("ADDR0: 0x%08lx, ADDR1: 0x%08lx, ADDR2: 0x%08lx\n",
				addr0, addr1, addr2);
			VIOC_WDMA_SetImageBase(wdma, addr0, addr1, addr2);
			VIOC_WDMA_SetImageEnable(wdma, ON);
		} else {
			dlog("Buffer is not initialized successfully.\n");
		}

		mutex_unlock(&cif->lock);
	} else {
		addr0 = vdev->cif.pmap_preview.base;
		addr1 = 0;
		addr2 = 0;

		logd("ADDR0: 0x%08lx, ADDR1: 0x%08lx, ADDR2: 0x%08lx\n",
			addr0, addr1, addr2);
		VIOC_WDMA_SetImageBase(wdma, addr0, addr1, addr2);
		VIOC_WDMA_SetImageEnable(wdma, ON);
	}

	return 0;
}

static void tccvin_work_thread(struct work_struct *data)
{
	struct tccvin_cif		*cif		=
		container_of(data, struct tccvin_cif, wdma_work);
	struct tccvin_streaming		*stream		=
		container_of(cif, struct tccvin_streaming, cif);
	struct tccvin_video_queue	*queue		= &stream->queue;
	struct tccvin_buffer		*buf		= NULL;
	unsigned long			flags;
	struct timespec			ts		= { 0, };

	void __iomem			*wdma		=
		VIOC_WDMA_GetAddress(stream->cif.vioc_path.wdma);
	unsigned long			addr0		= 0;
	unsigned long			addr1		= 0;
	unsigned long			addr2		= 0;

	if (stream == NULL) {
		loge("stream is NULL\n");
		return;
	}

	if (queue == NULL) {
		loge("queue is NULL\n");
		return;
	}

	if (&queue->irqqueue == NULL) {
		loge("&queue->irqqueue is NULL\n");
		return;
	}

	spin_lock_irqsave(&queue->irqlock, flags);
	if (!list_empty(&queue->irqqueue)) {
		buf = list_first_entry(&queue->irqqueue, struct tccvin_buffer,
			queue);
	} else {
		loge("There is no entry of the incoming buffer list\n");
	}

	/* Store the payload FID bit and return immediately when the buffer is
	 * NULL.
	 */
	if (buf == NULL) {
		loge("buf is NULL\n");
		spin_unlock_irqrestore(&queue->irqlock, flags);
		return;
	}

	/*
	 * check whether the driver has only one buffer or not
	 */
	if (list_is_last(&(buf->queue), &queue->irqqueue)) {
		logw("driver has only one buffer!!\n");
		spin_unlock_irqrestore(&queue->irqlock, flags);
		goto update_wdma;
	} else {
		spin_unlock_irqrestore(&queue->irqlock, flags);
	}

	buf->buf.vb2_buf.timestamp = timespec_to_ns(&ts);
	buf->buf.field = V4L2_FIELD_NONE;
	buf->buf.sequence = stream->sequence++;
	buf->state = TCCVIN_BUF_STATE_READY;
	buf->bytesused = buf->length;

	dlog("VIN[%d] buf->length: 0x%08x\n", stream->vdev.num, buf->length);
	if (buf->buf.vb2_buf.memory != VB2_MEMORY_MMAP &&
	   buf->buf.vb2_buf.memory != VB2_MEMORY_USERPTR) {
		memcpy(buf->mem, cif->vir, buf->length);
	}

	buf = tccvin_queue_next_buffer(&stream->queue, buf);

update_wdma:
	mutex_lock(&cif->lock);

	if (buf != NULL) {
		switch (buf->buf.vb2_buf.memory) {
		case VB2_MEMORY_MMAP:
			addr0 = buf->buf.vb2_buf.planes[0].m.offset;
			break;
		case VB2_MEMORY_USERPTR:
			addr0 = virt_to_phys((void *)
				buf->buf.vb2_buf.planes[0].m.userptr);
			break;
		default:
			loge("memory type is not supported\n");
			return;
		}
		tccvin_convert_to_multi_planes_buffer_addresses(
			stream->cur_frame->wWidth, stream->cur_frame->wHeight,
			stream->cur_format->fcc, &addr0, &addr1, &addr2);
		logd("ADDR0: 0x%08lx, ADDR1: 0x%08lx, ADDR2: 0x%08lx\n",
			addr0, addr1, addr2);
		VIOC_WDMA_SetImageBase(wdma, addr0, addr1, addr2);
		VIOC_WDMA_SetImageEnable(wdma, OFF);
	} else {
		dlog("Buffer is not initialized successfully.\n");
	}

	mutex_unlock(&cif->lock);
}

static irqreturn_t tccvin_wdma_isr(int irq, void *client_data)
{
	struct tccvin_streaming		*vdev	=
		(struct tccvin_streaming *)client_data;
	void __iomem			*wdma	=
		VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	unsigned int			status;
	bool				ret;

	ret = is_vioc_intr_activatied(vdev->cif.vioc_intr.id,
		vdev->cif.vioc_intr.bits);
	if (ret == false) {
		return IRQ_NONE;
	}

	// preview operation.
	VIOC_WDMA_GetStatus(wdma, &status);
	if (status & VIOC_WDMA_IREQ_EOFR_MASK) {
		schedule_work(&vdev->cif.wdma_work);

		vioc_intr_clear(vdev->cif.vioc_intr.id,
			vdev->cif.vioc_intr.bits);
	}

	return IRQ_HANDLED;
}

static int tccvin_allocate_essential_buffers(struct tccvin_streaming *vdev)
{
	char		pmap_preview_name[1024];
	int		ret		= 0;

	if (vdev->vdev.num > 0) {
		sprintf(pmap_preview_name, "rearcamera%d", vdev->vdev.num);
	} else {
		sprintf(pmap_preview_name, "rearcamera");
	}
	strcpy(vdev->cif.pmap_preview.name, pmap_preview_name);
	ret = pmap_get_info(vdev->cif.pmap_preview.name,
		&vdev->cif.pmap_preview);
	if (ret == 1) {
		logi("name: %20s, base: 0x%08llx, size: 0x%08llx\n",
			vdev->cif.pmap_preview.name,
			vdev->cif.pmap_preview.base,
			vdev->cif.pmap_preview.size);
		vdev->cif.vir = ioremap_nocache(vdev->cif.pmap_preview.base,
			PAGE_ALIGN(vdev->cif.pmap_preview.size));
		logd("name: %20s, phy base: 0x%08llx, vir base: 0x%p\n",
			vdev->cif.pmap_preview.name,
			vdev->cif.pmap_preview.base,
			vdev->cif.vir);
	} else {
		loge("get \"rearcamera\" pmap information.\n");
		return -1;
	}

	strcpy(vdev->cif.pmap_viqe.name, "rearcamera_viqe");
	ret = pmap_get_info(vdev->cif.pmap_viqe.name, &(vdev->cif.pmap_viqe));
	if (ret == 1) {
		logd("[PMAP] %s: 0x%08llx ~ 0x%08llx (0x%08llx)\n",
			vdev->cif.pmap_viqe.name,
			vdev->cif.pmap_viqe.base,
			vdev->cif.pmap_viqe.base + vdev->cif.pmap_viqe.size,
			vdev->cif.pmap_viqe.size);
	} else {
		loge("get \"rearcamera_viqe\" pmap information.\n");
		ret = -1;
	}

#if defined(CONFIG_OVERLAY_PGL) && !defined(CONFIG_OVERLAY_DPGL)
	strcpy(vdev->cif.pmap_pgl.name, "parking_gui");
	if (pmap_get_info(vdev->cif.pmap_pgl.name,
		&(vdev->cif.pmap_pgl)) == 1) {
		logd("[PMAP] %s: 0x%08llx ~ 0x%08llx (0x%08llx)\n",
			vdev->cif.pmap_pgl.name,
			vdev->cif.pmap_pgl.base,
			vdev->cif.pmap_pgl.base + vdev->cif.pmap_pgl.size,
			vdev->cif.pmap_pgl.size);
	} else {
		loge("get \"parking_gui\" pmap information.\n");
		ret = -1;
	}
#endif//CONFIG_OVERLAY_PGL

	return ret;
}

static int tccvin_start_stream(struct tccvin_streaming *vdev)
{
	struct tccvin_cif	*cif		= &vdev->cif;
	struct vioc_path	*vioc		= &vdev->cif.vioc_path;

	// size info
	logd("preview size: %d * %d\n",
		vdev->cur_frame->wWidth, vdev->cur_frame->wHeight);

	// map cif-port
	tccvin_map_cif_port(vdev);

	// reset vioc path
	tccvin_reset_vioc_path(vdev);

	// set rdma for Parking Guide Line
#if defined(CONFIG_OVERLAY_PGL) && !defined(CONFIG_OVERLAY_DPGL)
	tccvin_set_pgl(vdev);
#endif

	// set vin
	tccvin_set_vin(vdev);

	// set deinterlacer
	if (!!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED)) {
		// set Deinterlacer
		tccvin_set_deinterlacer(vdev);
	}

	// set scaler
	if (vioc->scaler != -1) {
		tccvin_set_scaler(vdev, vdev->cur_frame);
	}

	// set wmixer
	if (vioc->wmixer != -1) {
		tccvin_set_wmixer(vdev);
	}

	// set wdma
	tccvin_set_wdma(vdev);

	// wait for the 3 frames in case of viqe 3d mode (16ms * 3 frames)
	mdelay(16 * 3);

	return 0;
}

static int tccvin_stop_stream(struct tccvin_streaming *vdev)
{
	struct vioc_path	*vioc		= &vdev->cif.vioc_path;
#ifdef CONFIG_OVERLAY_PGL
	void __iomem		*pgl	= VIOC_RDMA_GetAddress(vioc->pgl);
#endif//CONFIG_OVERLAY_PGL
	void __iomem		*wdma	= VIOC_WDMA_GetAddress(vioc->wdma);
	VIOC_PlugInOutCheck	state;

	VIOC_WDMA_SetIreqMask(wdma, VIOC_WDMA_IREQ_ALL_MASK, 0x1);
	VIOC_WDMA_SetImageDisable(wdma);

	if (VIOC_WDMA_Get_CAddress(wdma) > 0UL) {
		int		idxLoop;
		unsigned int	status;

		/* We set a criteria for a worst-case within 10-fps and
		 * time-out to disable WDMA as 200ms. To be specific, in the
		 * 10-fps environment, the worst case we assume, it has to
		 * wait 200ms at least for 2 frame.
		 */
		for (idxLoop = 0; idxLoop < 10; idxLoop++) {
			VIOC_WDMA_GetStatus(wdma, &status);
			if (status & VIOC_WDMA_IREQ_EOFR_MASK) {
				break;
			}
			msleep(20);	// 20msec is minimum in msleep()
		}
	}

	if (vioc->scaler != -1) {
		VIOC_CONFIG_Device_PlugState(vioc->scaler, &state);
		if (state.enable &&
			state.connect_statue == VIOC_PATH_CONNECTED) {
			VIOC_CONFIG_PlugOut(vioc->scaler);
		}
	}

	if (!!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED)) {
		if (vioc->viqe != -1) {
			VIOC_CONFIG_Device_PlugState(vioc->viqe, &state);
			if (state.enable &&
				state.connect_statue == VIOC_PATH_CONNECTED) {
				VIOC_CONFIG_PlugOut(vioc->viqe);
			}
		} else if (vioc->deintl_s != -1) {
			VIOC_CONFIG_Device_PlugState(vioc->deintl_s, &state);
			if (state.enable &&
				state.connect_statue == VIOC_PATH_CONNECTED) {
				VIOC_CONFIG_PlugOut(vioc->deintl_s);
			}
		} else {
			loge("There is no available deinterlacer\n");
		}
	}

	VIOC_VIN_SetEnable(VIOC_VIN_GetAddress(vioc->vin), OFF);

#if defined(CONFIG_OVERLAY_PGL) && !defined(CONFIG_OVERLAY_DPGL)
	// disable pgl
	VIOC_RDMA_SetImageDisable(pgl);
#endif//CONFIG_OVERLAY_PGL

	// reset vioc path
	tccvin_reset_vioc_path(vdev);

	return 0;
}

static int tccvin_request_irq(struct tccvin_streaming *vdev)
{
	struct vioc_path	*vioc_path	= &vdev->cif.vioc_path;
	struct vioc_intr_type	*vioc_intr	= &vdev->cif.vioc_intr;
	unsigned int		intr_base_id	= 0;
	unsigned int		vioc_base_id	= 0;
	unsigned int		vioc_wdma_id	= 0;
	int			ret		= 0;

	if ((vdev->cif.vioc_irq_num != -1) && (vdev->cif.vioc_irq_reg == 0)) {
		vioc_intr->id   = -1;
		vioc_intr->bits = -1;

		vioc_base_id	= VIOC_WDMA00;
		intr_base_id	= VIOC_INTR_WD0;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		if (vioc_path->wdma >= VIOC_WDMA09) {
			vioc_base_id	= VIOC_WDMA09;
			intr_base_id	= VIOC_INTR_WD9;
		}
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC805X)

		// convert to raw index
		vioc_base_id	= get_vioc_index(vioc_base_id);
		vioc_wdma_id	= get_vioc_index(vioc_path->wdma);

		vioc_intr->id	= intr_base_id + (vioc_wdma_id - vioc_base_id);
		vioc_intr->bits	= VIOC_WDMA_IREQ_EOFR_MASK;

		if (vdev->cif.vioc_irq_reg == 0) {
			vioc_intr_clear(vioc_intr->id, vioc_intr->bits);
			ret = request_irq(vdev->cif.vioc_irq_num,
				tccvin_wdma_isr, IRQF_SHARED,
				vdev->vdev.name, vdev);
			vioc_intr_enable(vdev->cif.vioc_irq_num,
				vioc_intr->id, vioc_intr->bits);
			vdev->cif.vioc_irq_reg = 1;
		} else {
			loge("VIN[%d] The irq(%d) is already registered.\n",
				vdev->vdev.num, vdev->cif.vioc_irq_num);
			return -1;
		}
	} else {
		loge("VIN[%d] The irq node is NOT found.\n", vdev->vdev.num);
		return -1;
	}

	return ret;
}

static int tccvin_free_irq(struct tccvin_streaming *vdev)
{
	struct vioc_intr_type	*vioc_intr	= &vdev->cif.vioc_intr;
	int			ret		= 0;

	if ((vdev->cif.vioc_irq_num != -1) && (vdev->cif.vioc_irq_reg == 1)) {
		if (vdev->cif.vioc_irq_reg == 1) {
			vioc_intr_clear(vioc_intr->id, vioc_intr->bits);
			vioc_intr_disable(vdev->cif.vioc_irq_num,
				vioc_intr->id, vioc_intr->bits);
			free_irq(vdev->cif.vioc_irq_num, vdev);
			vdev->cif.vioc_irq_reg = 0;
		} else {
			loge("The irq(%d) is NOT registered.\n",
				vdev->cif.vioc_irq_num);
			return -1;
		}
	} else {
		loge("The irq node is NOT found.\n");
		return -1;
	}

	return ret;
}

/**************************************************
 *	PUBLIC FUNCTION LIST
 **************************************************/
static int tccvin_get_clock(struct tccvin_streaming *vdev)
{
	struct device_node	*main_node	= vdev->dev->pdev->dev.of_node;
	int			ret		= 0;

	vdev->cif.vioc_clk = of_clk_get(main_node, 0);
	ret = -IS_ERR(vdev->cif.vioc_clk);
	if (ret != 0) {
		loge("Find the \"clock\" node\n");
	}

	return ret;
}

static void tccvin_put_clock(struct tccvin_streaming *vdev)
{
	if (!IS_ERR(vdev->cif.vioc_clk)) {
		clk_put(vdev->cif.vioc_clk);
	}
}

static int tccvin_enable_clock(struct tccvin_streaming *vdev)
{
	int			ret = 0;

	if (!IS_ERR(vdev->cif.vioc_clk)) {
		ret = clk_prepare_enable(vdev->cif.vioc_clk);
		if (ret) {
			loge("clk_prepare_enable returns %d\n", ret);
		}
	}

	return ret;
}

static void tccvin_disable_clock(struct tccvin_streaming *vdev)
{
	if (!IS_ERR(vdev->cif.vioc_clk)) {
		clk_disable_unprepare(vdev->cif.vioc_clk);
	}
}

int tccvin_video_init(struct tccvin_streaming *stream)
{
	struct tccvin_format	*format	= NULL;
	struct tccvin_frame	*frame	= NULL;
	int			ret	= 0;

	// parse device tree
	ret = tccvin_parse_device_tree(stream);
	if (ret != 0) {
		loge("Parse the device tree.\n");
		return -ENODEV;
	}

	// get the vioc's clock
	ret = tccvin_get_clock(stream);
	if (ret != 0) {
		loge("tccvin_get_clock returns %d\n", ret);
		return ret;
	}

	// enable the vioc's clock
	ret = tccvin_enable_clock(stream);
	if (ret != 0) {
		loge("tccvin_enable_clock returns %d\n", ret);
		return ret;
	}

	// get a video source
#if 0//def CONFIG_VIDEO_VIDEOsOURCE
	stream->cif.videosource_info	= &videosource_info;
	videosource_if_initialize();
	if (stream->cif.videosource_info == NULL) {
		logi("ERROR: videosource_info is NULL.\n");
		return -1;
	}

	stream->cif.skip_frame		=
		stream->cif.videosource_info->capture_skip_frame;
#endif//CONFIG_VIDEO_VIDEOsOURCE

	stream->is_handover_needed	= 0;

	// vin window
	stream->selection.r.left	= 0;
	stream->selection.r.top		= 0;
	stream->selection.r.width	= 0;
	stream->selection.r.height	= 0;

	// preview method
	stream->preview_method		= PREVIEW_V4L2;

	// init v4l2 resources
	mutex_init(&stream->cif.lock);

	// init work thread
	INIT_WORK(&stream->cif.wdma_work, tccvin_work_thread);

	stream->cam_streaming = 0;

	// allocate essential buffers
	tccvin_allocate_essential_buffers(stream);

	if (stream->nformats == 0) {
		logi("No supported video formats found.\n");
		return -EINVAL;
	}

	atomic_set(&stream->active, 0);

	// use the first format in default
	format = &stream->format[0];

	if (format->nframes == 0) {
		logi("No frame descriptor found for the default format.\n");
		return -EINVAL;
	}

	// use the first frame in default
	frame = &format->frame[0];

	// set the default format
	stream->def_format = format;
	stream->cur_format = format;
	stream->cur_frame = frame;

	return ret;
}

int tccvin_video_deinit(struct tccvin_streaming *stream)
{
	int		ret = 0;

	// deinit v4l2 resources
	mutex_destroy(&(stream->cif.lock));

	// disable the vioc's clock
	tccvin_disable_clock(stream);

	// put the vioc's clock
	tccvin_put_clock(stream);

	return ret;
}

int tccvin_video_streamon(struct tccvin_streaming *stream,
	int is_handover_needed)
{
	int			ret		= 0;

	// update the is_handover_needed
	stream->is_handover_needed	= is_handover_needed;

	logi("preview method: %s\n", (stream->preview_method == PREVIEW_V4L2) ?
		"PREVIEW_V4L2" : "PREVIEW_DD");

	if (stream->is_handover_needed) {
		stream->is_handover_needed = 0;
		logi("#### Handover - Skip to set the vioc path\n");
		return 0;
	}

	logi("Video-Input Path(%d) is NOT working\n", stream->vdev.num);
	ret = tccvin_start_stream(stream);
	if (ret < 0) {
		loge("Start Stream\n");
		return -1;
	}

	if (stream->preview_method == PREVIEW_V4L2) {
		ret = tccvin_request_irq(stream);
		if (ret < 0) {
			loge("Request IRQ\n");
			return ret;
		}
	}

	stream->cam_streaming = 1;

	return ret;
}

int tccvin_video_streamoff(struct tccvin_streaming *stream,
	int is_handover_needed)
{
	int		ret = 0;

	stream->cam_streaming = 0;

	if (stream->preview_method == PREVIEW_V4L2) {
		ret = tccvin_free_irq(stream);
		if (ret < 0) {
			loge("Free IRQ\n");
			return ret;
		}
	}

	ret = tccvin_stop_stream(stream);
	if (ret < 0) {
		loge("Stop Stream\n");
		return -1;
	}

	return ret;
}
