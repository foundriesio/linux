/*******************************************************************************
 *      tccvin_video.c  --  Video handling
 *
 *		Copyright (c) 2020-
 *			Telechips Inc.
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *		SPDX-license-Identifier : GPL-2.0+
 *
 *******************************************************************************/

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

static struct tccvin_format_desc tccvin_fmts[] = {
	/* RGB */
	{
		.name		= "RGB 8:8:8 (RGB3)",
		.guid		= VIOC_IMG_FMT_RGB888,		// B1[31:24] R[23:16] G[15:8] B0[7:0]
		.fcc		= V4L2_PIX_FMT_RGB24,		// 'RBG3' 24 RGB-8-8-8
	},
	{
		.name		= "ARGB 8:8:8:8 (RGB4)",
		.guid		= VIOC_IMG_FMT_ARGB8888,	// A[31:24] R[23:16] G[15:8] B[7:0]
		.fcc		= V4L2_PIX_FMT_RGB32,		// 'RGB4' 32 RGB-8-8-8-8
	},

	/* sequential (YUV packed) */
	{
		.name		= "YUV 4:2:2 (UYVY)",
		.guid		= VIOC_IMG_FMT_UYVY,		// LSB [Y/U/Y/V] MSB : YCbCr 4:2:2 sequential
		.fcc		= V4L2_PIX_FMT_UYVY,		// 'UYVY' 16 YUV 4:2:2
	},
	{
		.name		= "YUV 4:2:2 (VYUY)",
		.guid		= VIOC_IMG_FMT_VYUY,		// LSB [Y/V/Y/U] MSB : YCbCr 4:2:2 sequential
		.fcc		= V4L2_PIX_FMT_VYUY,		// 'VYUY' 16 YUV 4:2:2
	},
	{
		.name		= "YUV 4:2:2 (YUYV)",
		.guid		= VIOC_IMG_FMT_YUYV,		// LSB [Y/U/Y/V] MSB : YCbCr 4:2:2 sequential
		.fcc		= V4L2_PIX_FMT_YUYV,		// 'YUYV' 16 YUV 4:2:2
	},
	{
		.name		= "YUV 4:2:2 (YVYU)",
		.guid		= VIOC_IMG_FMT_YVYU,		// LSB [Y/V/Y/U] MSB : YCbCr 4:2:2 sequential
		.fcc		= V4L2_PIX_FMT_YVYU,		// 'YVYU' 16 YVU 4:2:2
	},

	/* sepatated (Y, U, V planar) */
	{
		.name		= "YVU 4:2:0 (YV12)",
		.guid		= VIOC_IMG_FMT_YUV420SEP,	// YCbCr 4:2:0 separated
		.fcc		= V4L2_PIX_FMT_YVU420,		// 'YV12' 12 YVU 4:2:0
	},
	{
		.name		= "YUV 4:2:0 (YU12)",
		.guid		= VIOC_IMG_FMT_YUV420SEP,	// YCbCr 4:2:0 separated
		.fcc		= V4L2_PIX_FMT_YUV420,		// 'YU12' 12 YUV 4:2:0
	},
	{
		.name		= "YUV 4:2:2 (422P)",
		.guid		= VIOC_IMG_FMT_YUV422SEP,	// YCbCr 4:2:2 separated
		.fcc		= V4L2_PIX_FMT_YUV422P,		// '422P' 16 YVU422 Planar
	},

	/* interleaved (Y palnar, UV planar) */
	{
		.name		= "YUV 4:2:0 (NV12)",
		.guid		= VIOC_IMG_FMT_YUV420IL0,	// YCbCr 4:2:0 interleaved type0
		.fcc		= V4L2_PIX_FMT_NV12,		// 'NV12' 12 Y/CbCr 4:2:0
	},
	{
		.name		= "YUV 4:2:0 (NV21)",
		.guid		= VIOC_IMG_FMT_YUV420IL1,	// YCbCr 4:2:0 interleaved type1
		.fcc		= V4L2_PIX_FMT_NV21,		// 'NV21' 12 Y/CrCb 4:2:0
	},
	{
		.name		= "YUV 4:2:0 (NV16)",
		.guid		= VIOC_IMG_FMT_YUV422IL0,	// YCbCr 4:2:2 interleaved type0
		.fcc		= V4L2_PIX_FMT_NV16,		// 'NV16' 16 Y/CbCr 4:2:2
	},
	{
		.name		= "YUV 4:2:0 (NV61)",
		.guid		= VIOC_IMG_FMT_YUV422IL1,	// YCbCr 4:2:2 interleaved type1
		.fcc		= V4L2_PIX_FMT_NV61,		// 'NV61' 16 Y/CrCb 4:2:2
	},
};

/* ------------------------------------------------------------------------
 * Utility functions
 */

//static 
struct tccvin_format_desc *tccvin_format_by_guid(const __u32 guid)
{
	unsigned int len = ARRAY_SIZE(tccvin_fmts);
	unsigned int i;
	FUNCTION_IN

	for (i = 0; i < len; ++i) {
		if (guid == tccvin_fmts[i].guid)
			return &tccvin_fmts[i];
	}

	FUNCTION_OUT
	return NULL;
}

struct tccvin_format_desc *tccvin_format_by_fcc(const __u32 fcc)
{
	unsigned int len = ARRAY_SIZE(tccvin_fmts);
	unsigned int i;
	FUNCTION_IN

	for (i = 0; i < len; ++i) {
		if (fcc == tccvin_fmts[i].fcc)
			return &tccvin_fmts[i];
	}

	FUNCTION_OUT
	return NULL;
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
static int tccvin_parse_device_tree(struct tccvin_streaming * vdev) {
	struct device_node		* main_node	= vdev->dev->pdev->dev.of_node;
	struct device_node		* vsrc_node = NULL;
	struct device_node		* vioc_node	= NULL;
	struct device_node		* np_fb = NULL, * np_fb_1st = NULL;
	volatile void __iomem	* address	= NULL;

	if(main_node == NULL) {
		logd("tcc_camera device node is not found.\n");
		return -ENODEV;
	}

	vsrc_node = of_parse_phandle(main_node, "videosource", 0);
	if(vsrc_node != NULL) {
		struct device_node *i2c_node = of_get_parent(vsrc_node);
		int addr;
		struct property *pp = NULL;
		char *strtoken = "i2c";

		if(i2c_node) {
			of_property_read_u32(vsrc_node, "reg", &addr);
			for_each_property_of_node(of_aliases, pp) {
				if(strcmp(pp->value, i2c_node->full_name) == 0) {
					sprintf(vdev->dev->subdev_name, "%s-%04x", strstr(pp->name, strtoken) + strlen(strtoken), addr);
					logd("subdev name: %s", vdev->dev->subdev_name);
				}
			}
		} else {
			loge("Failed to i2c node\n");
		}

		logd("save videosource i2c value(%s) to tccvindev\n", vdev->dev->subdev_name);
	} else {
		loge("\"videosource\" node is not found.\n");
//		return -ENODEV;
	}

	// VIN
	vdev->cif.vioc_path.vin = -1;
	vioc_node = of_parse_phandle(main_node, "vin", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "vin", 1, &vdev->cif.vioc_path.vin);
		if(vdev->cif.vioc_path.vin != -1) {
			address = VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin);
			logd("%10s[%2d]: 0x%p\n", "VIN", get_vioc_index(vdev->cif.vioc_path.vin) / 2, address);	// "divide vin's index by 2 because of vin lut's index
		}
	} else {
		loge("\"vin\" node is not found.\n");
		return -ENODEV;
	}

	// cif port
	vdev->cif.cif_port = -1;
	vioc_node = of_parse_phandle(main_node, "cifport", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "cifport", 1, &vdev->cif.cif_port);
		if(vdev->cif.cif_port != -1) {
			vdev->cif.cifport_addr = of_iomap(vioc_node, 0);
			logd("%10s[%2d]: 0x%p\n", "CIF Port", vdev->cif.cif_port, vdev->cif.cifport_addr);
		}
	} else {
		loge("The CIF port node is NULL\n");
//		return -ENODEV;
	}

#ifdef CONFIG_OVERLAY_PGL
	// Parking Guide Line
	vdev->cif.vioc_path.pgl = -1;
	// VIDEO_IN04~06 don't have RDMA
	if(vdev->cif.vioc_path.vin >= VIOC_VIN00 && vdev->cif.vioc_path.vin <= VIOC_VIN30) {
		vioc_node = of_parse_phandle(main_node, "rdma", 0);
		if(vioc_node != NULL) {
			of_property_read_u32_index(main_node, "rdma", 1, &vdev->cif.vioc_path.pgl);
			if(vdev->cif.vioc_path.pgl != -1) {
				address = VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);
				logd("%10s[%2d]: 0x%p\n", "RDMA(PGL)", get_vioc_index(vdev->cif.vioc_path.pgl), address);
			}
		} else {
			logw("\"rdma\" node is not found.\n");
//			return -ENODEV;
		}
	}
#endif//CONFIG_OVERLAY_PGL

	// VIQE
	vdev->cif.vioc_path.viqe = -1;
	vdev->cif.vioc_path.deintl_s = -1;
	vioc_node = of_parse_phandle(main_node, "viqe", 0);
	if(vioc_node) {
		of_property_read_u32_index(main_node, "viqe", 1, &vdev->cif.vioc_path.viqe);
		if(vdev->cif.vioc_path.viqe != -1) {
			address = VIOC_VIQE_GetAddress(vdev->cif.vioc_path.viqe);
			logd("%10s[%2d]: 0x%p\n", "VIQE", get_vioc_index(vdev->cif.vioc_path.viqe), address);
		}
	} else {
		logw("\"viqe\" node is not found.\n");

		// DEINTL_S
		vioc_node = of_parse_phandle(main_node, "deintls", 0);
		if(vioc_node) {
			of_property_read_u32_index(main_node, "deintls", 1, &vdev->cif.vioc_path.deintl_s);
			if(vdev->cif.vioc_path.deintl_s != -1) {
				address = VIOC_DEINTLS_GetAddress();//vdev->cif.vioc_path.deintl_s);
				logd("%10s[%2d]: 0x%p\n", "DEINTL_S", get_vioc_index(vdev->cif.vioc_path.deintl_s), address);
			}
		} else {
			logw("\"deintls\" node is not found.\n");
		}
	}

	// SCALER
	vdev->cif.vioc_path.scaler = -1;
	vioc_node = of_parse_phandle(main_node, "scaler", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "scaler", 1, &vdev->cif.vioc_path.scaler);
		if(vdev->cif.vioc_path.scaler != -1) {
			address = VIOC_SC_GetAddress(vdev->cif.vioc_path.scaler);
			logd("%10s[%2d]: 0x%p\n", "SCALER", get_vioc_index(vdev->cif.vioc_path.scaler), address);
		}
	} else {
		logw("\"scaler\" node is not found.\n");
	}

	// WMIXER
	vdev->cif.vioc_path.wmixer = -1;
	vioc_node = of_parse_phandle(main_node, "wmixer", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "wmixer", 1, &vdev->cif.vioc_path.wmixer);
		if(vdev->cif.vioc_path.wmixer != -1) {
			address = VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer);
			logd("%10s[%2d]: 0x%p\n", "WMIXER", get_vioc_index(vdev->cif.vioc_path.wmixer), address);
		}
	} else {
		logw("\"wmixer\" node is not found.\n");
	}

	// WDMA
	vdev->cif.vioc_path.wdma = -1;
	vdev->cif.vioc_irq_num   = -1;
	vioc_node = of_parse_phandle(main_node, "wdma", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "wdma", 1, &vdev->cif.vioc_path.wdma);
		if(vdev->cif.vioc_path.wdma != -1) {
			address = VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
			logd("%10s[%2d]: 0x%p\n", "WDMA", get_vioc_index(vdev->cif.vioc_path.wdma), address);
		}

		// WDMA interrupt
		vdev->cif.vioc_irq_num		= irq_of_parse_and_map(vioc_node, get_vioc_index(vdev->cif.vioc_path.wdma));
		logd("vdev->cif.vioc_irq_num: %d\n", vdev->cif.vioc_irq_num);
	} else {
		loge("\"wdma\" node is not found.\n");
		return -ENODEV;
	}

	// FIFO
	vdev->cif.vioc_path.fifo = -1;
	vioc_node = of_parse_phandle(main_node, "fifo", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "fifo", 1, &vdev->cif.vioc_path.fifo);
		if(vdev->cif.vioc_path.fifo != -1) {
			address = VIOC_FIFO_GetAddress(vdev->cif.vioc_path.fifo);
			logd("%10s[%2d]: 0x%p\n", "FIFO", get_vioc_index(vdev->cif.vioc_path.fifo), address);
		}
	} else {
		logw("\"fifo\" node is not found.\n");
	}

	/* get the information of vioc-fb device node */
	np_fb = of_find_compatible_node(NULL, NULL, "telechips,vioc-fb");
	if(np_fb != NULL) {
		int		fbdisplay_num = 0;
		if(!of_property_read_u32(np_fb, "telechips,fbdisplay_num", &fbdisplay_num)) {
			/* get register address for main output */
			np_fb_1st = of_find_node_by_name(np_fb, (fbdisplay_num ? "fbdisplay1" : "fbdisplay0"));
			if(np_fb_1st != NULL) {
				// RDMA
				vdev->cif.vioc_path.rdma = -1;
				vioc_node = of_parse_phandle(np_fb_1st, "telechips,rdma", 0);
				if(vioc_node != NULL) {
					of_property_read_u32_index(np_fb_1st, "telechips,rdma", 1 + 1, &vdev->cif.vioc_path.rdma);
					if(vdev->cif.vioc_path.rdma != -1) {
						address = VIOC_RDMA_GetAddress(vdev->cif.vioc_path.rdma);
						logd("%10s[%2d]: 0x%p\n", "RDMA", get_vioc_index(vdev->cif.vioc_path.rdma), address);
					}
				} else {
					loge("\"rdma\" node is not found.\n");
					return -ENODEV;
				}

				// WMIXER_OUTPUT
				vdev->cif.vioc_path.wmixer_out = -1;
				vioc_node = of_parse_phandle(np_fb_1st, "telechips,wmixer", 0);
				if(vioc_node != NULL) {
					of_property_read_u32_index(np_fb_1st, "telechips,wmixer", 1, &vdev->cif.vioc_path.wmixer_out);
					if(vdev->cif.vioc_path.wmixer_out != -1) {
						address = VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer_out);
						logd("%10s[%2d]: 0x%p\n", "WMIX", get_vioc_index(vdev->cif.vioc_path.wmixer_out), address);
					}
				} else {
					loge("\"wmixer_out\" node is not found.\n");
					return -ENODEV;
				}

				// DISP
				vdev->cif.vioc_path.disp = -1;
				vioc_node = of_parse_phandle(np_fb_1st, "telechips,disp", 0);
				if(vioc_node != NULL) {
					of_property_read_u32_index(np_fb_1st, "telechips,disp", 1, &vdev->cif.vioc_path.disp);
					if(vdev->cif.vioc_path.disp != -1) {
						address = VIOC_DISP_GetAddress(vdev->cif.vioc_path.disp);
						logd("%10s[%2d]: 0x%p\n", "DISP", get_vioc_index(vdev->cif.vioc_path.disp), address);
					}
				} else {
					loge("\"telechips,disp\" node is not found.\n");
					return -ENODEV;
				}
			} else {
				loge("\"fbdisplayn\" node is not found.\n");
				return -ENODEV;
			}
		} else {
			loge("\"telechips,fbdisplay_num\" node is not found.\n");
			return -ENODEV;
		}
	} else {
		loge("\"telechips,vioc-fb\" node is not found.\n");
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
 *	If no responce comes from main core, do it directly.
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-1:		The vioc manager device is not found.
 */
static int tccvin_reset_vioc_path(struct tccvin_streaming * vdev) {
#if 0
	vioc_path_t				* vioc	= &vdev->cif.vioc_path;
	int						vioc_component[] = {
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
	int						idxComponent = 0, nComponent = 0;
#if 0//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	struct file				* file		= NULL;
	char					name[1024];
	struct tcc_mbox_data	data;
	int						ret = 0;
#endif

	FUNCTION_IN

	nComponent = sizeof(vioc_component) / sizeof(vioc_component[0]);

#if 0//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	// open video-input driver
	sprintf(name, "/dev/vioc_mgr_a7s");

	file = filp_open(name, O_RDWR, 0666);
	if(file == NULL) {
		logi("error: driver open fail (%s)\n", name);
		return -1;
	}

	// reset
	for(idxComponent=nComponent - 1; 0 <= idxComponent; idxComponent--) {
		if(vioc_component[idxComponent] != -1) {
			memset(&data, 0x0, sizeof(struct tcc_mbox_data));
			data.cmd[1] = (VIOC_CMD_RESET & 0xFFFF) << 16;
			data.data[0] = (unsigned int)vioc_component[idxComponent];
			data.data[1] = VIOC_CONFIG_RESET;
			data.data_len = 2;

			ret = file->f_op->unlocked_ioctl(file, IOCTL_VIOC_MGR_SET_RESET_KERNEL, (unsigned long)&data);
			if(ret <= 0) {
				loge("[%d] IOCTL_VIOC_MGR_SET_RESET_KERNEL\n", idxComponent);
				goto err_viocmg;
			}
		}
	}

	// reset clear
	for(idxComponent=0; idxComponent<nComponent; idxComponent++) {
		if(vioc_component[idxComponent] != -1) {
			memset(&data, 0x0, sizeof(struct tcc_mbox_data));
			data.cmd[1] = (VIOC_CMD_RESET & 0xFFFF) << 16;
			data.data[0] = (unsigned int)vioc_component[idxComponent];
			data.data[1] = VIOC_CONFIG_CLEAR;
			data.data_len = 2;

			ret = file->f_op->unlocked_ioctl(file, IOCTL_VIOC_MGR_SET_RESET_KERNEL, (unsigned long)&data);
			if(ret <= 0) {
				loge("[%d] IOCTL_VIOC_MGR_SET_RESET_KERNEL\n", idxComponent);
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
	for(idxComponent=nComponent - 1; 0 <= idxComponent; idxComponent--) {
		if(vioc_component[idxComponent] != -1)
			VIOC_CONFIG_SWReset_RAW((unsigned int)vioc_component[idxComponent], VIOC_CONFIG_RESET);
	}

	// reset clear
	for(idxComponent=0; idxComponent<nComponent; idxComponent++) {
		if(vioc_component[idxComponent] != -1)
			VIOC_CONFIG_SWReset_RAW((unsigned int)vioc_component[idxComponent], VIOC_CONFIG_CLEAR);
	}

#if 0//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
end:
#endif//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)

	FUNCTION_OUT
#endif
	return 0;
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
static int tccvin_map_cif_port(struct tccvin_streaming * vdev) {
	unsigned int	vin_index	= (get_vioc_index(vdev->cif.vioc_path.vin) / 2);
	unsigned int	value		= 0;

	value = ((__raw_readl(vdev->cif.cifport_addr) & ~(0xF << (vin_index * 4))) | (vdev->cif.cif_port << (vin_index * 4)));
	__raw_writel(value, vdev->cif.cifport_addr);

	value = __raw_readl(vdev->cif.cifport_addr);
	logd("CIF Port: %d, VIN Index: %d, Register Value: 0x%08x\n", vdev->cif.cif_port, vin_index, value);

	return 0;
}

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
static int tccvin_check_cif_port_mapping(struct tccvin_streaming * vdev) {
	unsigned int	vin_index	= (get_vioc_index(vdev->cif.vioc_path.vin) / 2);
	unsigned int	cif_port	= 0;
	int				ret			= 0;

	cif_port = ((__raw_readl(vdev->cif.cifport_addr) >> (vin_index * 4)) & (0xF));
	if(cif_port != vdev->cif.cif_port) {
		logi("CIF Port Mapping is WRONG - CIF Port: %d / %d, VIN Index: %d\n", cif_port, vdev->cif.cif_port, vin_index);
		ret = -1;
	}

	return ret;
}

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
static int tccvin_set_vin(struct tccvin_streaming * vdev) {
	volatile void __iomem	* pVIN		= VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin);
	videosource_format_t	* vs_info	= vdev->cif.videosource_info;

	unsigned int	data_order			= vs_info->data_order;
	unsigned int	data_format			= vs_info->data_format;
	unsigned int	gen_field_en		= vs_info->gen_field_en;

	unsigned int	de_active_low		= vs_info->de_active_low;
	unsigned int	field_bfield_low	= vs_info->field_bfield_low;
	unsigned int	vs_active_low		= !!(vs_info->polarities & V4L2_DV_VSYNC_POS_POL);
	unsigned int	hs_active_low		= !!(vs_info->polarities & V4L2_DV_HSYNC_POS_POL);
	unsigned int	pxclk_pol			= !!(vs_info->polarities & V4L2_DV_PCLK_POS_POL);
	unsigned int	vs_mask				= vs_info->vs_mask;
	unsigned int	hsde_connect_en		= vs_info->hsde_connect_en;
	unsigned int	intpl_en			= vs_info->intpl_en;
	unsigned int	conv_en				= vs_info->conv_en;
	unsigned int	interlaced			= !!(vs_info->interlaced & V4L2_DV_INTERLACED);
	unsigned int	width				= vs_info->width;
	unsigned int	height				= vs_info->height >> interlaced;
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	unsigned int	se					= vs_info->se;
	unsigned int	fvs 				= vs_info->fvs;
#endif//defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)

	FUNCTION_IN
	logd("VIN: 0x%p, Source Size - width: %d, height: %d\n", pVIN, width, height);

	VIOC_VIN_SetSyncPolarity(pVIN, hs_active_low, vs_active_low, field_bfield_low, de_active_low, gen_field_en, pxclk_pol);
	VIOC_VIN_SetCtrl(pVIN, conv_en, hsde_connect_en, vs_mask, data_format, data_order);
	VIOC_VIN_SetInterlaceMode(pVIN, interlaced, intpl_en);
	VIOC_VIN_SetImageSize(pVIN, width, height);
	VIOC_VIN_SetImageOffset(pVIN, 0, 0, 0);
	VIOC_VIN_SetImageCropSize(pVIN, width, height);
	VIOC_VIN_SetImageCropOffset(pVIN, 0, 0);
	VIOC_VIN_SetY2RMode(pVIN, 2);

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	VIOC_VIN_SetSEEnable(pVIN, se);
	VIOC_VIN_SetFlushBufferEnable(pVIN, fvs);
#endif//defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)

	logd("vdev->cif.videosource_format.data_format: 0x%08x, FMT_YUV422_16BIT: 0x%08x, FMT_YUV422_8BIT: 0x%08x\n", vdev->cif.videosource_info->data_format, FMT_YUV422_16BIT, FMT_YUV422_8BIT);
	logd("vdev->cur_format->fcc: 0x%08x, V4L2_PIX_FMT_RGB24: 0x%08x, V4L2_PIX_FMT_RGB32: 0x%08x\n", vdev->cur_format->fcc, V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_RGB32);
	if(((vdev->cif.videosource_info->data_format == FMT_YUV422_16BIT) || \
		(vdev->cif.videosource_info->data_format == FMT_YUV422_8BIT)) && \
		((vdev->cur_format->fcc == V4L2_PIX_FMT_RGB24) ||	\
		(vdev->cur_format->fcc == V4L2_PIX_FMT_RGB32))) {

		logi("vdev->cif.videosource_format.interlaced: 0x%08x\n", vdev->cif.videosource_info->interlaced);
		if(!((vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED) && (vdev->cif.vioc_path.viqe != -1)))
			VIOC_VIN_SetY2REnable(pVIN, ON);
	} else {
		VIOC_VIN_SetY2REnable(pVIN, OFF);
	}

	VIOC_VIN_SetEnable(pVIN, ON);

	FUNCTION_OUT
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
static int tccvin_set_deinterlacer(struct tccvin_streaming * vdev) {
	int		ret	= 0;

	FUNCTION_IN

	if(vdev->cif.vioc_path.viqe != -1) {
		volatile void __iomem	* pVIQE			= VIOC_VIQE_GetAddress(vdev->cif.vioc_path.viqe);

		unsigned int			interlaced		= !!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED);
		unsigned int			width			= vdev->cif.videosource_info->width;
		unsigned int			height			= vdev->cif.videosource_info->height >> interlaced;

		unsigned int			viqe_width		= 0;
		unsigned int			viqe_height		= 0;
		unsigned int			format			= VIOC_VIQE_FMT_YUV422;
		unsigned int			bypass_deintl	= VIOC_VIQE_DEINTL_MODE_3D;
		unsigned int			offset			= width * height * 2 * 2;
		unsigned int			deintl_base0	= vdev->cif.pmap_viqe.base;
		unsigned int			deintl_base1	= deintl_base0 + offset;
		unsigned int			deintl_base2	= deintl_base1 + offset;
		unsigned int			deintl_base3	= deintl_base2 + offset;

		unsigned int			cdf_lut_en		= OFF;
		unsigned int			his_en			= OFF;
		unsigned int			gamut_en		= OFF;
		unsigned int			d3d_en			= OFF;
		unsigned int			deintl_en		= ON;

//		struct device_node		* node			= NULL;
//		unsigned int			* viqe_set_reg1	= NULL;
//		unsigned int			* viqe_set_reg2	= NULL;

		logd("VIQE: 0x%p, Source Size - width: %d, height: %d\n", pVIQE, width, height);

#if 0
		if(!(hdl_np = of_parse_phandle(vdev->dev->pdev->dev.of_node, "viqe_set", 0))) {
			loge("could not find cam_viqe_set node!!\n");
		} else {
			viqe_set_reg1 = (unsigned int *)of_iomap(hdl_np, 0);
			viqe_set_reg2 = (unsigned int *)of_iomap(hdl_np, 1);

			BITCSET(*viqe_set_reg1,1<<3,1<<3);
			BITCSET(*viqe_set_reg2,1<<8 | 1<<9 , 1<<8 | 1<<9);
		}
#endif

		if(vdev->cif.vioc_path.vin <= VIOC_VIN30) {
			VIOC_CONFIG_PlugIn(vdev->cif.vioc_path.viqe, vdev->cif.vioc_path.vin);

			if(((vdev->cif.videosource_info->data_format == FMT_YUV422_16BIT) || \
				(vdev->cif.videosource_info->data_format == FMT_YUV422_8BIT)) && \
			   ((vdev->cur_format->fcc == V4L2_PIX_FMT_RGB24) ||	\
				(vdev->cur_format->fcc == V4L2_PIX_FMT_RGB32))) {
				VIOC_VIQE_SetImageY2RMode(pVIQE, 2);
				VIOC_VIQE_SetImageY2REnable(pVIQE, ON);
			}
			VIOC_VIQE_SetControlRegister(pVIQE, viqe_width, viqe_height, format);
			VIOC_VIQE_SetDeintlRegister(pVIQE, format, OFF, viqe_width, viqe_height, bypass_deintl,	\
				deintl_base0, deintl_base1, deintl_base2, deintl_base3);
			VIOC_VIQE_SetControlEnable(pVIQE, cdf_lut_en, his_en, gamut_en, d3d_en, deintl_en);
			VIOC_VIQE_SetDeintlModeWeave(pVIQE);
		}
	} else if(vdev->cif.vioc_path.deintl_s != -1) {
		if(vdev->cif.vioc_path.vin <= VIOC_VIN30) {
			VIOC_CONFIG_PlugIn(vdev->cif.vioc_path.deintl_s, vdev->cif.vioc_path.vin);
		}
	} else {
		logi("There is no available deinterlacer\n");
		ret = -1;
	}

	FUNCTION_OUT
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
static int tccvin_set_scaler(tccvin_cif_t * cif, struct tccvin_frame *frame) {
	volatile void __iomem	* pSC		= VIOC_SC_GetAddress(cif->vioc_path.scaler);

	unsigned int	width	= frame->wWidth;
	unsigned int	height	= frame->wHeight;

	unsigned int	crop_x	= cif->videosource_info->crop_x;
	unsigned int	crop_y	= cif->videosource_info->crop_y;

	unsigned int	crop_w	= cif->videosource_info->crop_w;
	unsigned int	crop_h	= cif->videosource_info->crop_h;

	FUNCTION_IN
	logd("SC: 0x%p, Output Size - width: %d, height: %d\n", pSC, width, height);

	if((cif->videosource_info->width != frame->wWidth || cif->videosource_info->height != frame->wHeight) || \
			(crop_w != 0 || crop_h != 0)) {
			// Plug the scaler in
			VIOC_CONFIG_PlugIn(cif->vioc_path.scaler, cif->vioc_path.vin);

			// Configure the scaler
			VIOC_SC_SetBypass(pSC, OFF);
			VIOC_SC_SetDstSize(pSC, width + crop_w, height + crop_h + 2);		// workaround: scaler margin
			VIOC_SC_SetOutPosition(pSC, crop_x, crop_y);
			VIOC_SC_SetOutSize(pSC, width, height);
			VIOC_SC_SetUpdate(pSC);
	}

	FUNCTION_OUT
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
static int tccvin_set_wmixer(struct tccvin_streaming * vdev) {
	volatile void __iomem	* pWMIXer	= VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer);

	unsigned int	width	= vdev->cur_frame->wWidth;
	unsigned int	height	= vdev->cur_frame->wHeight;
#ifdef CONFIG_OVERLAY_PGL
	unsigned int layer		= 0x0;
	unsigned int key_R		= PGL_BG_R;
	unsigned int key_G		= PGL_BG_G;
	unsigned int key_B		= PGL_BG_B;
	unsigned int key_mask_R	= ((PGL_BGM_R >> 3) << 3 );
	unsigned int key_mask_G	= ((PGL_BGM_G >> 3) << 3 );
	unsigned int key_mask_B	= ((PGL_BGM_B >> 3) << 3 );
#endif//CONFIG_OVERLAY_PGL

	FUNCTION_IN
	logd("WMIXer: 0x%p, Size - width: %d, height: %d\n", pWMIXer, width, height);

	// Configure the wmixer
#ifdef CONFIG_OVERLAY_PGL
	VIOC_WMIX_SetSize(pWMIXer, width, height);
	VIOC_WMIX_SetPosition(pWMIXer, 1, 0, 0);
	VIOC_WMIX_SetChromaKey(pWMIXer, layer, ON, key_R, key_G, key_B, key_mask_R, key_mask_G, key_mask_B);
	VIOC_WMIX_SetUpdate(pWMIXer);
	VIOC_CONFIG_WMIXPath(vdev->cif.vioc_path.vin, ON);	// ON: Mixing mode / OFF: Bypass mode
#else
	VIOC_CONFIG_WMIXPath(vdev->cif.vioc_path.vin, OFF);	// ON: Mixing mode / OFF: Bypass mode
#endif

	FUNCTION_OUT
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
static int tccvin_set_wdma(struct tccvin_streaming * vdev) {
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);

	unsigned int	width	= vdev->cur_frame->wWidth;
	unsigned int	height	= vdev->cur_frame->wHeight;
	struct tccvin_format_desc *format_desc = tccvin_format_by_fcc(vdev->cur_format->fcc);
	unsigned int	format	= format_desc->guid;
	unsigned int	addr0	= 0;
	unsigned int	addr1	= 0;
	unsigned int	addr2	= 0;

	FUNCTION_IN
	logd("WDMA: 0x%p, size[%d x %d], format[%d]. \n", pWDMA, width, height, format);

	VIOC_WDMA_SetImageFormat(pWDMA, format);
	VIOC_WDMA_SetImageSize(pWDMA, width, height);

#if 0
	if((((vdev->cur_format->fcc == V4L2_PIX_FMT_YVU420) || (vdev->cur_format->fcc == V4L2_PIX_FMT_YUV420)) && \
		((width/2)%C_STRIDE_ALIGN!=0))) {
		VIOC_WDMA_SetImageOffset_withYV12(pWDMA, width);
	}
	else
#endif
	{
		VIOC_WDMA_SetImageOffset(pWDMA, format, width);
	}

#if 0
	if(vdev->preview_method == PREVIEW_V4L2) {
		struct tccvin_buf	* buf	= list_entry(vdev->v4l2.capture_buf_list.next, struct tccvin_buf, buf_list);
		switch(buf->buf.memory) {
		case V4L2_MEMORY_MMAP:
//			addr0	= (unsigned long)vdev->cif.pmap_preview.base + buf->buf.m.offset;
			addr0	= buf->buf.m.offset;
			break;
		case V4L2_MEMORY_USERPTR:
			addr0	= buf->buf.m.userptr;
			break;
		}
	} else if(vdev->preview_method == PREVIEW_DD) {
		buf_addr_t			* buf	= &vdev->cif.preview_buf_addr[0];
		addr0 = buf->addr0;
	}
#else
	addr0 = vdev->cif.pmap_preview.base;
#endif
	logd("ADDR0: 0x%08x, ADDR1: 0x%08x, ADDR2: 0x%08x\n", addr0, addr1, addr2);
	VIOC_WDMA_SetImageBase(pWDMA, addr0, addr1, addr2);
	VIOC_WDMA_SetImageEnable(pWDMA, ON);

	FUNCTION_OUT
	return 0;
}

static void tccvin_work_thread(struct work_struct * data) {
	tccvin_cif_t				* cif		= container_of(data, tccvin_cif_t, wdma_work);
	struct tccvin_streaming		* stream	= container_of(cif, struct tccvin_streaming, cif);
	struct tccvin_video_queue	* queue		= &stream->queue;
	struct tccvin_buffer		* buf		= NULL;
	unsigned long				flags;
	struct timespec				ts			= { 0, };

	volatile void __iomem		* pWDMABase	= VIOC_WDMA_GetAddress(stream->cif.vioc_path.wdma);
	unsigned int				addr0		= 0;
	unsigned int				addr1		= 0;
	unsigned int				addr2		= 0;

	if(stream == NULL) {
		loge("stream is NULL\n");
		return;
	}

	if(queue == NULL) {
		loge("queue is NULL\n");
		return;
	}

	if(&queue->irqqueue == NULL) {
		loge("&queue->irqqueue is NULL\n");
		return;
	}

	spin_lock_irqsave(&queue->irqlock, flags);
	if (!list_empty(&queue->irqqueue))
		buf = list_first_entry(&queue->irqqueue, struct tccvin_buffer, queue);
	else
		loge("There is no entry of the incoming buffer list\n");
	spin_unlock_irqrestore(&queue->irqlock, flags);

	/* Store the payload FID bit and return immediately when the buffer is
	 * NULL.
	 */
	if (buf == NULL) {
		loge("buf is NULL\n");
		return;
	}

	buf->buf.vb2_buf.timestamp = timespec_to_ns(&ts);
	buf->buf.field = V4L2_FIELD_NONE;
	buf->buf.sequence = stream->sequence++;
	buf->state = TCCVIN_BUF_STATE_READY;
	dlog("VIN[%d] buf->length: 0x%08x\n", stream->vdev.num, buf->length);
	memcpy(buf->mem, cif->vir, buf->length);
	buf->bytesused = buf->length;

	/*
	 *	queue the current buffer to the outgoing list
	 *	and get a next buffer from the incoming list to write
	 */
	buf = tccvin_queue_next_buffer(&stream->queue, buf);	// get a next buffer

	mutex_lock(&cif->lock);
	addr0 = stream->cif.pmap_preview.base;//queue->queue.mem_ops->get_dmabuf()
	addr1 = 0;
	addr2 = 0;
	VIOC_WDMA_SetImageBase(pWDMABase, addr0, addr1, addr2);
	VIOC_WDMA_SetImageEnable(pWDMABase, OFF);
	mutex_unlock(&cif->lock);
}

static irqreturn_t tccvin_wdma_isr(int irq, void * client_data) {
	struct tccvin_streaming	* vdev		= (struct tccvin_streaming *)client_data;
	volatile void __iomem	* pWDMABase	= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	unsigned int			status;

	if(is_vioc_intr_activatied(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits) == false) {
		return IRQ_NONE;
	}

	// preview operation.
	VIOC_WDMA_GetStatus(pWDMABase, &status);
	if(status & VIOC_WDMA_IREQ_EOFR_MASK) {
		schedule_work(&vdev->cif.wdma_work);

		vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
	} else {
		logd("the interrupt condition is matched - status:0x%08x, VIOC_WDMA_IREQ_EOFR_MASK: 0x%08x\n", status, VIOC_WDMA_IREQ_EOFR_MASK);
	}

	return IRQ_HANDLED;
}

static int tccvin_allocate_essential_buffers(struct tccvin_streaming * vdev) {
	char				pmap_preview_name[1024];
	int					ret = 0;

	if(0 < vdev->vdev.num) {
		sprintf(pmap_preview_name, "rearcamera%d", vdev->vdev.num);
	} else {
		sprintf(pmap_preview_name, "rearcamera");
	}
	strcpy(vdev->cif.pmap_preview.name, pmap_preview_name);
	ret = pmap_get_info(vdev->cif.pmap_preview.name, &vdev->cif.pmap_preview);
	if(ret == 1) {
		logn("name: %20s, base: 0x%08llx, size: 0x%08llx\n",
			vdev->cif.pmap_preview.name, \
			vdev->cif.pmap_preview.base, \
			vdev->cif.pmap_preview.size);
		vdev->cif.vir = ioremap_nocache(vdev->cif.pmap_preview.base, PAGE_ALIGN(vdev->cif.pmap_preview.size));
		logd("name: %20s, phy base: 0x%08x, vir base: 0x%p\n", \
			vdev->cif.pmap_preview.name, vdev->cif.pmap_preview.base, vdev->cif.vir);
    } else {
		loge("get \"rearcamera\" pmap information.\n");
		return -1;
    }

	strcpy(vdev->cif.pmap_viqe.name, "rearcamera_viqe");
	if(pmap_get_info(vdev->cif.pmap_viqe.name, &(vdev->cif.pmap_viqe)) == 1) {
		logd("[PMAP] %s: 0x%08x ~ 0x%08llx (0x%08llx)\n",
			vdev->cif.pmap_viqe.name, \
			vdev->cif.pmap_viqe.base, \
			vdev->cif.pmap_viqe.base + vdev->cif.pmap_viqe.size, \
			vdev->cif.pmap_viqe.size);
	} else {
		loge("get \"rearcamera_viqe\" pmap information.\n");
		ret = -1;
	}

#if defined(CONFIG_OVERLAY_PGL) && !defined(CONFIG_OVERLAY_DPGL)
	strcpy(vdev->cif.pmap_pgl.name, "parking_gui");
	if(pmap_get_info(vdev->cif.pmap_pgl.name, &(vdev->cif.pmap_pgl)) == 1) {
		logd("[PMAP] %s: 0x%08x ~ 0x%08llx (0x%08llx)\n",
			vdev->cif.pmap_pgl.name, \
			vdev->cif.pmap_pgl.base, \
			vdev->cif.pmap_pgl.base + vdev->cif.pmap_pgl.size, \
			vdev->cif.pmap_pgl.size);
	} else {
		loge("get \"parking_gui\" pmap information.\n");
		ret = -1;
	}
#endif//CONFIG_OVERLAY_PGL

    return ret;
}

static int tccvin_start_stream(struct tccvin_streaming * vdev) {
	tccvin_cif_t	* cif	= &vdev->cif;

	FUNCTION_IN

	// size info
	logd("tgt: %d * %d\n", vdev->cur_frame->wWidth, vdev->cur_frame->wHeight);

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
	if(!!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED)) {
		// set Deinterlacer
		tccvin_set_deinterlacer(vdev);
	}

	// set scaler
	if(cif->vioc_path.vin <= VIOC_VIN30) {
		tccvin_set_scaler(cif, vdev->cur_frame);
	}

	// set wmixer
	if(cif->vioc_path.vin <= VIOC_VIN30) {
		tccvin_set_wmixer(vdev);
	}

	// set wdma
	tccvin_set_wdma(vdev);
	mdelay(16 * 3);	// wait for the 3 frames in case of viqe 3d mode (16ms * 3 frames)

	FUNCTION_OUT
	return 0;
}

static int tccvin_stop_stream(struct tccvin_streaming * vdev) {
#ifdef CONFIG_OVERLAY_PGL
	volatile void __iomem	* pPGL		= VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);
#endif//CONFIG_OVERLAY_PGL
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	VIOC_PlugInOutCheck		vioc_plug_state;
	tccvin_cif_t	* cif	= &vdev->cif;

	int idxLoop;
	unsigned int status;

	FUNCTION_IN

	VIOC_WDMA_SetIreqMask(pWDMA, VIOC_WDMA_IREQ_ALL_MASK, 0x1);	// disable WDMA interrupt
	VIOC_WDMA_SetImageDisable(pWDMA);

	if (VIOC_WDMA_Get_CAddress(pWDMA) > 0UL) {
		/* We set a criteria for a worst-case within 10-fps and
		 * time-out to disable WDMA as 200ms. To be specific, in the
		 * 10-fps environment, the worst case we assume, it has to
		 * wait 200ms at least for 2 frame. */
		for(idxLoop = 0; idxLoop < 20; idxLoop++) {
			VIOC_WDMA_GetStatus(pWDMA, &status);
			if(status & VIOC_WDMA_IREQ_EOFR_MASK) {
				break;
			}
			else {
				msleep(10);
			}
		}
	}

	if(cif->vioc_path.vin <= VIOC_VIN30) {
		VIOC_CONFIG_Device_PlugState(vdev->cif.vioc_path.scaler, &vioc_plug_state);
		if(vioc_plug_state.enable && vioc_plug_state.connect_statue == VIOC_PATH_CONNECTED) {
			VIOC_CONFIG_PlugOut(vdev->cif.vioc_path.scaler);
		}

		if(!!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED)) {
			if(vdev->cif.vioc_path.viqe != -1) {
				VIOC_CONFIG_Device_PlugState(vdev->cif.vioc_path.viqe, &vioc_plug_state);
				if(vioc_plug_state.enable && vioc_plug_state.connect_statue == VIOC_PATH_CONNECTED) {
					VIOC_CONFIG_PlugOut(vdev->cif.vioc_path.viqe);
				}
			} else if(vdev->cif.vioc_path.deintl_s != -1) {
				VIOC_CONFIG_Device_PlugState(vdev->cif.vioc_path.deintl_s, &vioc_plug_state);
				if(vioc_plug_state.enable && vioc_plug_state.connect_statue == VIOC_PATH_CONNECTED) {
					VIOC_CONFIG_PlugOut(vdev->cif.vioc_path.deintl_s);
				}
			} else {
				loge("There is no available deinterlacer\n");
			}
		}
	}

	VIOC_VIN_SetEnable(VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin), OFF); // disable VIN

#if defined(CONFIG_OVERLAY_PGL) && !defined(CONFIG_OVERLAY_DPGL)
	// disable pgl
	VIOC_RDMA_SetImageDisable(pPGL);
#endif//CONFIG_OVERLAY_PGL

	// reset vioc path
	tccvin_reset_vioc_path(vdev);

	FUNCTION_OUT
	return 0;
}

static int tccvin_request_irq(struct tccvin_streaming * vdev) {
	int					ret			= 0;

	FUNCTION_IN

	if((vdev->cif.vioc_irq_num != -1) && (vdev->cif.vioc_irq_reg == 0)) {
		vdev->cif.vioc_intr.id   = -1;
		vdev->cif.vioc_intr.bits = -1;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		if(vdev->cif.vioc_path.wdma < VIOC_WDMA09)
			vdev->cif.vioc_intr.id		= VIOC_INTR_WD0 + get_vioc_index(vdev->cif.vioc_path.wdma);
		else
			vdev->cif.vioc_intr.id		= VIOC_INTR_WD9 + (get_vioc_index(vdev->cif.vioc_path.wdma) - get_vioc_index(VIOC_WDMA09));
#else
		vdev->cif.vioc_intr.id		= VIOC_INTR_WD0 + get_vioc_index(vdev->cif.vioc_path.wdma);
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC805X)
		vdev->cif.vioc_intr.bits	= VIOC_WDMA_IREQ_EOFR_MASK;

		if(vdev->cif.vioc_irq_reg == 0) {
			vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			ret = request_irq(vdev->cif.vioc_irq_num, tccvin_wdma_isr, IRQF_SHARED, vdev->vdev.name, vdev);
			vioc_intr_enable(vdev->cif.vioc_irq_num, vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			vdev->cif.vioc_irq_reg = 1;
		} else {
			loge("VIN[%d] The irq(%d) is already registered.\n", vdev->vdev.num, vdev->cif.vioc_irq_num);
			return -1;
		}
	} else {
		loge("VIN[%d] The irq node is NOT found.\n", vdev->vdev.num);
		return -1;
	}

	FUNCTION_OUT
	return ret;
}

static int tccvin_free_irq(struct tccvin_streaming * vdev) {
	int					ret			= 0;

	FUNCTION_IN

	if((vdev->cif.vioc_irq_num != -1) && (vdev->cif.vioc_irq_reg == 1)) {
		if(vdev->cif.vioc_irq_reg == 1) {
			vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			vioc_intr_disable(vdev->cif.vioc_irq_num, vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			free_irq(vdev->cif.vioc_irq_num, vdev);
			vdev->cif.vioc_irq_reg = 0;
		} else {
			loge("The irq(%d) is NOT registered.\n", vdev->cif.vioc_irq_num);
			return -1;
		}
	} else {
		loge("The irq node is NOT found.\n");
		return -1;
	}

	FUNCTION_OUT
	return ret;
}

/**************************************************
 *	PUBLIC FUNCTION LIST
 **************************************************/
static int tccvin_get_clock(struct tccvin_streaming * vdev) {
	struct device_node	* main_node	= vdev->dev->pdev->dev.of_node;
	int					ret			= 0;

	FUNCTION_IN

	vdev->cif.vioc_clk = of_clk_get(main_node, 0);
	ret = -IS_ERR(vdev->cif.vioc_clk);
	if(ret != 0) {
		loge("Find the \"clock\" node\n");
	}

	FUNCTION_OUT
	return ret;
}

static void tccvin_put_clock(struct tccvin_streaming * vdev) {
	FUNCTION_IN

	if(!IS_ERR(vdev->cif.vioc_clk))
		clk_put(vdev->cif.vioc_clk);

	FUNCTION_OUT
}

static int tccvin_enable_clock(struct tccvin_streaming * vdev) {
	int		ret = 0;

	FUNCTION_IN

	if(!IS_ERR(vdev->cif.vioc_clk)) {
		ret = clk_prepare_enable(vdev->cif.vioc_clk);
		if(ret) {
			loge("clk_prepare_enable returns %d\n", ret);
		}
	}

	FUNCTION_OUT
	return ret;
}

static void tccvin_disable_clock(struct tccvin_streaming * vdev) {
	FUNCTION_IN

	if(!IS_ERR(vdev->cif.vioc_clk))
		clk_disable_unprepare(vdev->cif.vioc_clk);

	FUNCTION_OUT
}

int tccvin_video_init(struct tccvin_streaming *stream) {
	struct tccvin_format *format = NULL;
	struct tccvin_frame *frame = NULL;
	int		ret = 0;

	FUNCTION_IN

	// parse device tree
	ret = tccvin_parse_device_tree(stream);
	if(ret != 0) {
		loge("Parse the device tree.\n");
		return -ENODEV;
	}

	// get the vioc's clock
	ret = tccvin_get_clock(stream);
	if(ret != 0) {
		loge("tccvin_get_clock returns %d\n", ret);
		return ret;
	}

	// enable the vioc's clock
	ret = tccvin_enable_clock(stream);
	if(ret != 0) {
		loge("tccvin_enable_clock returns %d\n", ret);
		return ret;
	}

	// get a video source
#if 0//def CONFIG_VIDEO_VIDEOSOURCE
	stream->cif.videosource_info				= &videosource_info;
	videosource_if_initialize();
	if(stream->cif.videosource_info == NULL) {
		logi("ERROR: videosource_info is NULL.\n");
		return -1;
	}

	stream->cif.skip_frame					= stream->cif.videosource_info->capture_skip_frame;
#endif//CONFIG_VIDEO_VIDEOSOURCE

	stream->is_handover_needed				= 0;

	// preview method
	stream->preview_method					= PREVIEW_V4L2;	//PREVIEW_DD;	// v4l2 buffering is default

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
		logi("No frame descriptor found for the "
			"default format.\n");
		return -EINVAL;
	}

	// use the first frame in default
	frame = &format->frame[0];

	// set the default format
	stream->def_format = format;
	stream->cur_format = format;
	stream->cur_frame = frame;

	FUNCTION_OUT
	return ret;
}

int tccvin_video_deinit(struct tccvin_streaming *stream) {
	int		ret = 0;

	FUNCTION_IN

	// deinit v4l2 resources
	mutex_destroy(&(stream->cif.lock));

	// disable the vioc's clock
	tccvin_disable_clock(stream);

	// put the vioc's clock
	tccvin_put_clock(stream);

	FUNCTION_OUT
	return ret;
}

int tccvin_video_streamon(struct tccvin_streaming *stream, int is_handover_needed) {
	int		ret = 0;
	FUNCTION_IN

	// update the is_handover_needed
	stream->is_handover_needed	= is_handover_needed;

	logi("preview method: %s\n", (stream->preview_method == PREVIEW_V4L2) ? "PREVIEW_V4L2" : "PREVIEW_DD");

	if(stream->is_handover_needed) {
		stream->is_handover_needed = 0;
		logi("#### Handover - Skip to set the vioc path\n");
		return 0;
	}

	logi("Video-Input Path(%d) is NOT working\n", stream->vdev.num);
	ret = tccvin_start_stream(stream);
	if(ret < 0) {
		loge("Start Stream\n");
		return -1;
	}

	if(stream->preview_method == PREVIEW_V4L2) {
		ret = tccvin_request_irq(stream);
		if(ret < 0) {
			loge("Request IRQ\n");
			return ret;
		}
	}

	stream->cam_streaming = 1;

	FUNCTION_OUT
	return ret;
}

int tccvin_video_streamoff(struct tccvin_streaming *stream, int is_handover_needed) {
	int		ret = 0;

	FUNCTION_IN

	stream->cam_streaming = 0;

	if(stream->preview_method == PREVIEW_V4L2) {
		ret = tccvin_free_irq(stream);
		if(ret < 0) {
			loge("Free IRQ\n");
			return ret;
		}
	}

	ret = tccvin_stop_stream(stream);
	if(ret < 0) {
		loge("Stop Stream\n");
		return -1;
	}

	FUNCTION_OUT
	return ret;
}

