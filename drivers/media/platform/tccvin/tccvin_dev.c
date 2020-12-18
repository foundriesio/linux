/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/kthread.h>

#include <media/v4l2-common.h>
#include <video/tcc/tcc_gpu_align.h>

#if defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
#include <linux/fs.h>
#include <video/tcc/vioc_mgr.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#endif//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)

#include "tccvin_common.h"
#include "tccvin_dev.h"

#define	DRIVER_NAME		"avn-camera"

#ifndef CONFIG_TCC803X_CA7S
static unsigned int NORMAL_OVP = 24;
static unsigned int RCAM_OVP = 16;
#endif//CONFIG_TCC803X_CA7S

#define ALIGNED_BUFF(buf, mul) ( ( (unsigned int)buf + (mul-1) ) & ~(mul-1) )

#define ROUND_UP_2(num) 	(((num)+1)&~1)
#define ROUND_UP_4(num) 	(((num)+3)&~3)

enum format_conversion_type {
	FORMAT_TYPE_VIOC,
	FORMAT_TYPE_V4L2,
	FORMAT_TYPE_MAX,
};

struct format_conversion {
	unsigned int	format[FORMAT_TYPE_MAX];
};

struct format_conversion format_conversion_table[] = {
	/* RGB */
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_RGB888,	// B1[31:24] R[23:16] G[15:8] B0[7:0]
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_RGB24,		// 'RBG3' 24 RGB-8-8-8
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_ARGB8888,	// A[31:24] R[23:16] G[15:8] B[7:0]
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_RGB32,		// 'RGB4' 32 RGB-8-8-8-8
	},

	/* sequential (YUV packed) */
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_UYVY,		// LSB [Y/U/Y/V] MSB : YCbCr 4:2:2 sequential
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_UYVY,		// 'UYVY' 16 YUV 4:2:2
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_VYUY,		// LSB [Y/V/Y/U] MSB : YCbCr 4:2:2 sequential
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_VYUY,		// 'VYUY' 16 YUV 4:2:2
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUYV,		// LSB [Y/U/Y/V] MSB : YCbCr 4:2:2 sequential
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_YUYV,		// 'YUYV' 16 YUV 4:2:2
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YVYU,		// LSB [Y/V/Y/U] MSB : YCbCr 4:2:2 sequential
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_YVYU,		// 'YVYU' 16 YVU 4:2:2
	},

	/* sepatated (Y, U, V planar) */
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUV420SEP,	// YCbCr 4:2:0 separated
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_YVU420,	// 'YV12' 12 YVU 4:2:0
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUV420SEP, // YCbCr 4:2:0 separated
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_YUV420,	// 'YU12' 12 YUV 4:2:0
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUV422SEP,	// YCbCr 4:2:2 separated
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_YUV422P,	// '422P' 16 YVU422 Planar
	},

	/* interleaved (Y palnar, UV planar) */
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUV420IL0,	// YCbCr 4:2:0 interleaved type0
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_NV12,		// 'NV12' 12 Y/CbCr 4:2:0
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUV420IL1,	// YCbCr 4:2:0 interleaved type1
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_NV21,		// 'NV21' 12 Y/CrCb 4:2:0
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUV422IL0,	// YCbCr 4:2:2 interleaved type0
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_NV16,		// 'NV16' 16 Y/CbCr 4:2:2
	},
	{
		.format[FORMAT_TYPE_VIOC] = VIOC_IMG_FMT_YUV422IL1,	// YCbCr 4:2:2 interleaved type1
		.format[FORMAT_TYPE_V4L2] = V4L2_PIX_FMT_NV61,		// 'NV61' 16 Y/CrCb 4:2:2
	},
};

ssize_t tccvin_attr_diagnostics_show(struct device * dev, struct device_attribute * attr, char * buf) {
	tccvin_dev_t * vdev = (tccvin_dev_t *)dev->platform_data;
	long val = atomic_read(&vdev->tccvin_attr_diagnostics);

	sprintf(buf, "%ld\n", val);

	return val;
}

ssize_t tccvin_attr_diagnostics_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	tccvin_dev_t * vdev = (tccvin_dev_t *)dev->platform_data;
	long val;
	int error = kstrtoul(buf, 10, &val);
	if(error)
		return error;

	atomic_set(&vdev->tccvin_attr_diagnostics, val);

	return count;
}

static DEVICE_ATTR(tccvin_attr_diagnostics, S_IRUGO|S_IWUSR|S_IWGRP, tccvin_attr_diagnostics_show, tccvin_attr_diagnostics_store);

int tccvin_create_attr_diagnostics(tccvin_dev_t * vdev) {
	int		ret = 0;

	ret = device_create_file(vdev->dev_plt, &dev_attr_tccvin_attr_diagnostics);
	if(ret < 0)
		log("failed create sysfs: tccvin_attr_diagnostics\n");

	return ret;
}

/*
 * convert_format
 *
 * - DESCRIPTION:
 *	Convert pixel color format between vioc and v4l2
 *
 * - PARAMETERS:
 *	@format:		pixel format (can be a vioc color format or a v4l2 color format)
 *	@from_type:		source pixel color type
 *	@to_type:		destination pixel color type
 *
 * - RETURNS:
 *	-1:		any proper pixel color format is not found.
 *	Others:	destination pixel color format
 */
unsigned int convert_format(unsigned int format, unsigned int from_type, unsigned int to_type) {
	int idxEntry = 0, nEntry = sizeof(format_conversion_table) / sizeof(format_conversion_table[0]);
	unsigned int	ret = (unsigned int)-1;

	for(idxEntry=0; idxEntry<nEntry; idxEntry++) {
		if(format_conversion_table[idxEntry].format[from_type] == format) {
			ret = format_conversion_table[idxEntry].format[to_type];
			dlog("type: %d, format: 0x%08x -> type: %d, format: 0x%08x\n", from_type, format, to_type, ret);
			break;
		}
	}

	return ret;
}

/*
 * tccvin_set_wdma_buf_addr
 *
 * - DESCRIPTION:
 *	Set wdma's base address as a certain buffer address
 *
 * - PARAMETERS:
 *	@vdev:		video-input path device's data
 *	@idxBuf:	index of buffer to be set
 */
void tccvin_set_wdma_buf_addr(tccvin_dev_t * vdev, unsigned int idxBuf) {
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	unsigned int			format		= vdev->v4l2.format.fmt.pix.pixelformat;
	buf_addr_t				* buf_addr	= &vdev->cif.preview_buf_addr[idxBuf];
	unsigned int			addr0 = 0, addr1 = 0, addr2 = 0;

	switch(format) {
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_YUYV:
		addr0	= buf_addr->y;
		break;

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		addr0	= buf_addr->y;
		addr1	= buf_addr->u;
		addr2	= buf_addr->v;
		break;

	case V4L2_PIX_FMT_YUV420:
		addr0	= buf_addr->y;
		addr1	= buf_addr->u;
		addr2	= buf_addr->v;
		break;

	case V4L2_PIX_FMT_YVU420:
		addr0	= buf_addr->y;
		addr1	= buf_addr->v;
		addr2	= buf_addr->u;
		break;

	default:
		log("The color format(0x%08x) is WRONG.\n", format);
		WARN_ON(1);
	}

	dlog("[%2d] ADDR0: 0x%08x, ADDR1: 0x%08x, ADDR2: 0x%08x\n", idxBuf, addr0, addr1, addr2);
	VIOC_WDMA_SetImageBase(pWDMA, addr0, addr1, addr2);
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
int tccvin_parse_device_tree(tccvin_dev_t * vdev) {
	struct device_node		* main_node	= vdev->dev_plt->of_node;
	struct device_node		* vioc_node	= NULL;
	struct device_node		* np_fb = NULL, * np_fb_1st = NULL;
	volatile void __iomem	* address	= NULL;

	if(main_node == NULL) {
		logd("tcc_camera device node is not found.\n");
		return -ENODEV;
	}

	// cif port
	vdev->cif.cifport_addr = NULL;
	vdev->cif.cif_port = -1;
	vioc_node = of_find_compatible_node(NULL, NULL, "telechips,cif-port");
	if(vioc_node != NULL) {
		vdev->cif.cifport_addr = of_iomap(vioc_node, 0);
		dlog("%10s: 0x%p\n", "CIF Port", vdev->cif.cifport_addr);
	} else {
		loge("The CIF port node is NULL\n");
		return -ENODEV;
	}

	// VIN
	vdev->cif.vioc_path.vin = -1;
	vioc_node = of_parse_phandle(main_node, "vin", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "vin", 1, &vdev->cif.vioc_path.vin);
		if(vdev->cif.vioc_path.vin != -1) {
			address = VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin);
			dlog("%10s[%2d]: 0x%p\n", "VIN", get_vioc_index(vdev->cif.vioc_path.vin) / 2, address);	// "divide vin's index by 2 because of vin lut's index
		}
	} else {
		loge("\"vin\" node is not found.\n");
		return -ENODEV;
	}

#if defined(CONFIG_OVERLAY_PGL)
	vdev->cif.vioc_path.pgl = -1;
	vdev->cif.use_pgl = -1;
	// VIDEO_IN04~06 don't have RDMA
	if(vdev->cif.vioc_path.vin >= VIOC_VIN00 && vdev->cif.vioc_path.vin <= VIOC_VIN30) {
		vioc_node = of_parse_phandle(main_node, "rdma", 0);
		if(vioc_node != NULL) {
			of_property_read_u32_index(main_node, "rdma", 1, &vdev->cif.vioc_path.pgl);
			if(vdev->cif.vioc_path.pgl != -1) {
				address = VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);
				dlog("%10s[%2d]: 0x%p\n", "RDMA(PGL)", get_vioc_index(vdev->cif.vioc_path.pgl), address);
			}

			// Parking Guide Line
			vioc_node = of_parse_phandle(main_node, "use_pgl", 0);
			if(vioc_node != NULL) {
				of_property_read_u32_index(main_node, "use_pgl", 0, &vdev->cif.use_pgl);
				dlog("%10s[%2d]: %d\n", "usage status pgl", vdev->vid_dev.minor, vdev->cif.use_pgl);
			} else {
				loge("\"use_pgl\" node is not found.\n");
				return -ENODEV;
			}
		} else {
			loge("\"rdma\" node is not found.\n");
			return -ENODEV;
		}
	}
#endif//defined(CONFIG_OVERLAY_PGL)

	// DEINTLS
	vdev->cif.vioc_path.deintl_s = -1;
	vioc_node = of_parse_phandle(main_node, "deintls", 0);
	if(vioc_node) {
		of_property_read_u32_index(main_node, "deintls", 1, &vdev->cif.vioc_path.deintl_s);
		if(vdev->cif.vioc_path.deintl_s != -1) {
			address = VIOC_DEINTLS_GetAddress(/*vdev->cif.vioc_path.deintl_s*/);
			dlog("%10s[%2d]: 0x%p\n", "DEINTL_S", get_vioc_index(vdev->cif.vioc_path.deintl_s), address);
		}
	} else {
		dlog("\"deintls\" node is not found.\n");
	}

	// VIQE
	vdev->cif.vioc_path.viqe = -1;
	vioc_node = of_parse_phandle(main_node, "viqe", 0);
	if(vioc_node) {
		of_property_read_u32_index(main_node, "viqe", 1, &vdev->cif.vioc_path.viqe);
		if(vdev->cif.vioc_path.viqe != -1) {
			address = VIOC_VIQE_GetAddress(vdev->cif.vioc_path.viqe);
			dlog("%10s[%2d]: 0x%p\n", "VIQE", get_vioc_index(vdev->cif.vioc_path.viqe), address);
		}
	} else {
		dlog("\"viqe\" node is not found.\n");
	}

	// SCALER
	vdev->cif.vioc_path.scaler = -1;
	vioc_node = of_parse_phandle(main_node, "scaler", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "scaler", 1, &vdev->cif.vioc_path.scaler);
		if(vdev->cif.vioc_path.scaler != -1) {
			address = VIOC_SC_GetAddress(vdev->cif.vioc_path.scaler);
			dlog("%10s[%2d]: 0x%p\n", "SCALER", get_vioc_index(vdev->cif.vioc_path.scaler), address);
		}
	} else {
		dlog("\"scaler\" node is not found.\n");
	}

	// WMIXER
	vdev->cif.vioc_path.wmixer = -1;
	vioc_node = of_parse_phandle(main_node, "wmixer", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "wmixer", 1, &vdev->cif.vioc_path.wmixer);
		if(vdev->cif.vioc_path.wmixer != -1) {
			address = VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer);
			dlog("%10s[%2d]: 0x%p\n", "WMIXER", get_vioc_index(vdev->cif.vioc_path.wmixer), address);
		}
	} else {
		dlog("\"wmixer\" node is not found.\n");
	}

	// WDMA
	vdev->cif.vioc_path.wdma = -1;
	vdev->cif.vioc_irq_num   = -1;
	vioc_node = of_parse_phandle(main_node, "wdma", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "wdma", 1, &vdev->cif.vioc_path.wdma);
		if(vdev->cif.vioc_path.wdma != -1) {
			address = VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
			dlog("%10s[%2d]: 0x%p\n", "WDMA", get_vioc_index(vdev->cif.vioc_path.wdma), address);
		}

		// WDMA interrupt
		vdev->cif.vioc_irq_num		= irq_of_parse_and_map(vioc_node, get_vioc_index(vdev->cif.vioc_path.wdma));
		dlog("vdev->cif.vioc_irq_num: %d\n", vdev->cif.vioc_irq_num);
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
			dlog("%10s[%2d]: 0x%p\n", "FIFO", get_vioc_index(vdev->cif.vioc_path.fifo), address);
		}
	} else {
		dlog("\"fifo\" node is not found.\n");
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
						dlog("%10s[%2d]: 0x%p\n", "RDMA", get_vioc_index(vdev->cif.vioc_path.rdma), address);
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
						dlog("%10s[%2d]: 0x%p\n", "WMIX", get_vioc_index(vdev->cif.vioc_path.wmixer_out), address);
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
						dlog("%10s[%2d]: 0x%p\n", "DISP", get_vioc_index(vdev->cif.vioc_path.disp), address);
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
int tccvin_reset_vioc_path(tccvin_dev_t * vdev) {
	vioc_path_t				* vioc	= &vdev->cif.vioc_path;
	int						vioc_component[] = {
#if defined(CONFIG_OVERLAY_PGL)
		vioc->pgl,
#endif//defined(CONFIG_OVERLAY_PGL)
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
#endif
	int						ret = 0;

	FUNCTION_IN

	nComponent = sizeof(vioc_component) / sizeof(vioc_component[0]);

#if 0//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	// open video-input driver
	sprintf(name, "/dev/vioc_mgr_a7s");

	file = filp_open(name, O_RDWR, 0666);
	if(file == NULL) {
		log("error: driver open fail (%s)\n", name);
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
int tccvin_map_cif_port(tccvin_dev_t * vdev) {
	unsigned int	vin_index	= (get_vioc_index(vdev->cif.vioc_path.vin) / 2);
	unsigned int	value		= 0;

	value = ((__raw_readl(vdev->cif.cifport_addr) & ~(0xF << (vin_index * 4))) | (vdev->cif.cif_port << (vin_index * 4)));
	__raw_writel(value, vdev->cif.cifport_addr);

	value = __raw_readl(vdev->cif.cifport_addr);
	dlog("CIF Port: %d, VIN Index: %d, Register Value: 0x%08x\n", vdev->cif.cif_port, vin_index, value);

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
int tccvin_check_cif_port_mapping(tccvin_dev_t * vdev) {
	unsigned int	vin_index	= (get_vioc_index(vdev->cif.vioc_path.vin) / 2);
	unsigned int	cif_port	= 0;
	int				ret			= 0;

	cif_port = ((__raw_readl(vdev->cif.cifport_addr) >> (vin_index * 4)) & (0xF));
	if(cif_port != vdev->cif.cif_port) {
		log("CIF Port Mapping is WRONG - CIF Port: %d / %d, VIN Index: %d\n", cif_port, vdev->cif.cif_port, vin_index);
		ret = -1;
	}

	return ret;
}

/*
 * tccvin_clear_buffer
 *
 * - DESCRIPTION:
 *	Clear all buffers
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 */
void tccvin_clear_buffer(tccvin_dev_t * vdev) {
//	struct v4l2_pix_format	* pix_format = &vdev->v4l2.pix_format;
	int						idxBuf = 0, nBuf = vdev->v4l2.pp_num;

	for(idxBuf=0; idxBuf<nBuf; idxBuf++) {
//		memset(vdev->cif.preview_buf_addr, 0x00, req->count * sizeof(struct tccvin_buf));
	}
}

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
int tccvin_set_pgl(tccvin_dev_t * vdev) {
	volatile void __iomem	* pRDMA		= VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);

	unsigned int	width		= vdev->v4l2.format.fmt.pix.width;
	unsigned int	height		= vdev->v4l2.format.fmt.pix.height;
	unsigned int	format		= PGL_FORMAT;
	unsigned int	buf_addr	= vdev->cif.pmap_pgl.base;

	FUNCTION_IN
	dlog("RDMA: 0x%p, size[%d x %d], format[%d]. \n", pRDMA, width, height, format);

	VIOC_RDMA_SetImageFormat(pRDMA, format);
	VIOC_RDMA_SetImageSize(pRDMA, width, height);
	VIOC_RDMA_SetImageOffset(pRDMA, format, width);
	VIOC_RDMA_SetImageBase(pRDMA, buf_addr, 0, 0);
//	VIOC_RDMA_SetImageAlphaEnable(pRDMA, ON);
//	VIOC_RDMA_SetImageAlpha(pRDMA, 0xff, 0xff);
	VIOC_RDMA_SetImageEnable(pRDMA);
	VIOC_RDMA_SetImageUpdate(pRDMA);

	return 0;
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
int tccvin_set_vin(tccvin_dev_t * vdev) {
	volatile void __iomem	* pVIN		= VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin);
	videosource_format_t	* vs_info	= &vdev->cif.videosource_format;

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
#endif//defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)

	unsigned int    crop_x			= vs_info->crop_x;
	unsigned int    crop_y			= vs_info->crop_y;

	unsigned int    crop_w			= (vs_info->crop_w != 0) ? vs_info->crop_w : width;
	unsigned int    crop_h			= (vs_info->crop_h != 0) ? vs_info->crop_h >> interlaced : height;

	FUNCTION_IN
	dlog("VIN: 0x%p, Source Size - width: %d, height: %d\n", pVIN, width, height);
	dlog("VIN: 0x%p, Crop Size - (%d, %d) %d * %d\n", pVIN, crop_x, crop_y, crop_w, crop_h);

	VIOC_VIN_SetSyncPolarity(pVIN, hs_active_low, vs_active_low, field_bfield_low, de_active_low, gen_field_en, pxclk_pol);
	VIOC_VIN_SetCtrl(pVIN, conv_en, hsde_connect_en, vs_mask, data_format, data_order);
	VIOC_VIN_SetInterlaceMode(pVIN, interlaced, intpl_en);
	VIOC_VIN_SetImageSize(pVIN, width, height);
	VIOC_VIN_SetImageOffset(pVIN, 0, 0, 0);
	VIOC_VIN_SetImageCropSize(pVIN, crop_w, crop_h);
	VIOC_VIN_SetImageCropOffset(pVIN, crop_x, crop_y);
	VIOC_VIN_SetY2RMode(pVIN, 2);

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	VIOC_VIN_SetSEEnable(pVIN, se);
	VIOC_VIN_SetFlushBufferEnable(pVIN, fvs);
#endif//defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)

	dlog("vdev->cif.videosource_format.data_format: 0x%08x, FMT_YUV422_16BIT: 0x%08x, FMT_YUV422_8BIT: 0x%08x\n", vdev->cif.videosource_format.data_format, FMT_YUV422_16BIT, FMT_YUV422_8BIT);
	dlog("vdev->v4l2.format.fmt.pix.pixelformat: 0x%08x, V4L2_PIX_FMT_RGB24: 0x%08x, V4L2_PIX_FMT_RGB32: 0x%08x\n", vdev->v4l2.format.fmt.pix.pixelformat, V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_RGB32);
	if(((vdev->cif.videosource_format.data_format == FMT_YUV422_16BIT) || \
		(vdev->cif.videosource_format.data_format == FMT_YUV422_8BIT)) && \
		((vdev->v4l2.format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24) ||	\
		(vdev->v4l2.format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB32))) {

		dlog("vdev->cif.videosource_format.interlaced: 0x%08x\n", vdev->cif.videosource_format.interlaced);
		if(!((vdev->cif.videosource_format.interlaced & V4L2_DV_INTERLACED) && (vdev->cif.vioc_path.viqe != -1)))
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
int tccvin_set_deinterlacer(tccvin_dev_t * vdev) {
	int		ret	= 0;

	FUNCTION_IN

	if(vdev->cif.vioc_path.viqe != -1) {
		volatile void __iomem	* pVIQE			= VIOC_VIQE_GetAddress(vdev->cif.vioc_path.viqe);

		unsigned int			interlaced		= !!(vdev->cif.videosource_format.interlaced & V4L2_DV_INTERLACED);
		unsigned int			width			= vdev->cif.videosource_format.width;
		unsigned int			height			= vdev->cif.videosource_format.height >> interlaced;

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

		dlog("VIQE: 0x%p, Source Size - width: %d, height: %d\n", pVIQE, width, height);

#if 0
		if(!(hdl_np = of_parse_phandle(vdev->dev_plt->of_node, "viqe_set", 0))) {
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

			if(((vdev->cif.videosource_format.data_format == FMT_YUV422_16BIT) || \
				(vdev->cif.videosource_format.data_format == FMT_YUV422_8BIT)) && \
			   ((vdev->v4l2.format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24) ||	\
				(vdev->v4l2.format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB32))) {
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
		log("There is no available deinterlacer\n");
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
int tccvin_set_scaler(tccvin_cif_t * cif, struct tccvin_v4l2 * v4l2) {
	volatile void __iomem	* pSC		= VIOC_SC_GetAddress(cif->vioc_path.scaler);
	unsigned int		width		= v4l2->format.fmt.pix.width;
	unsigned int		height		= v4l2->format.fmt.pix.height;
	unsigned int		crop_ratio_hor	= 100;
	unsigned int		crop_ratio_ver	= 100;
	unsigned int		dst_width	= width;
	unsigned int		dst_height	= height;
	unsigned int		out_posx	= 0;
	unsigned int		out_posy	= 0;
	unsigned int		out_width	= width;
	unsigned int		out_height	= height;
	videosource_format_t	* vs_info	= &cif->videosource_format;

	if (vs_info->width > 4096) {
		logd("input size does not supported in scaler\n");
		goto end;
	}

	if (!((v4l2->selection.r.left == 0) &&
	      (v4l2->selection.r.top == 0) &&
	      (v4l2->selection.r.width == 0) &&
	      (v4l2->selection.r.height == 0))) {
		if (v4l2->selection.flags == V4L2_SEL_FLAG_GE) {
			crop_ratio_hor	= width * 100 /	v4l2->selection.r.width;
			crop_ratio_ver	= height * 100 / v4l2->selection.r.height;
			dst_width	= width * crop_ratio_hor / 100;
			dst_height	= height * crop_ratio_ver / 100;
			out_posx	= v4l2->selection.r.left * crop_ratio_hor / 100;
			out_posy	= v4l2->selection.r.top * crop_ratio_ver / 100;
			out_width	= /*v4l2->selection.r.width * crop_ratio_hor / 100*/width;
			out_height	= /*v4l2->selection.r.height * crop_ratio_ver / 100*/height;
		} else {
			out_posx	= v4l2->selection.r.left;
			out_posy	= v4l2->selection.r.top;
			out_width	= v4l2->selection.r.width;
			out_height	= v4l2->selection.r.height;
		}
	}

	FUNCTION_IN
	logd("SC: 0x%p, DST: %d * %d\n", pSC, dst_width, dst_height);
	logd("SC: 0x%p, OUT: (%d, %d) %d * %d\n", pSC, out_posx, out_posy, out_width, out_height);

	// Plug the scaler in
	VIOC_CONFIG_PlugIn(cif->vioc_path.scaler, cif->vioc_path.vin);

	// Configure the scaler
	VIOC_SC_SetBypass(pSC, OFF);
	VIOC_SC_SetDstSize(pSC, dst_width, dst_height);
	VIOC_SC_SetOutPosition(pSC, out_posx, out_posy);
	// workaround: scaler margin
	VIOC_SC_SetOutSize(pSC, out_width, out_height + 1);
	VIOC_SC_SetUpdate(pSC);
end:
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
int tccvin_set_wmixer(tccvin_dev_t * vdev) {
	volatile void __iomem	* pWMIXer	= VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer);

	unsigned int		width		= vdev->v4l2.format.fmt.pix.width;
	unsigned int		height		= vdev->v4l2.format.fmt.pix.height;
	unsigned int		out_posx	= 0;
	unsigned int		out_posy	= 0;
	unsigned int		ovp		= 5;
	unsigned int		vs_ch		= 0;
#if defined(CONFIG_OVERLAY_PGL)
	unsigned int		pgl_ch		= 1;
	unsigned int		chrom_layer	= 0;
	unsigned int		key_R		= PGL_BG_R;
	unsigned int		key_G		= PGL_BG_G;
	unsigned int		key_B		= PGL_BG_B;
	unsigned int		mask_R		= ((PGL_BGM_R >> 3) << 3 );
	unsigned int		mask_G		= ((PGL_BGM_G >> 3) << 3 );
	unsigned int		mask_B		= ((PGL_BGM_B >> 3) << 3 );
#endif//defined(CONFIG_OVERLAY_PGL)
	videosource_format_t	* vs_info	= &vdev->cif.videosource_format;

	if (vs_info->width > 4096) {
		logd("input size does not supported in wmixer\n");
		VIOC_CONFIG_WMIXPath(vdev->cif.vioc_path.vin, OFF);	// ON: Mixing mode / OFF: Bypass mode
		goto end;
	}

	if (!((vdev->v4l2.selection.r.left == 0) && (vdev->v4l2.selection.r.top == 0))) {
		if (vdev->v4l2.selection.flags != V4L2_SEL_FLAG_GE) {
			out_posx	= vdev->v4l2.selection.r.left;
			out_posy	= vdev->v4l2.selection.r.top;
		}
	}

	FUNCTION_IN
	logd("WMIXer: 0x%p, Size - width: %d, height: %d\n", pWMIXer, width, height);
	logd("WMIXer: 0x%p, CH0: (%d, %d)\n", pWMIXer, out_posx, out_posy);

	// Configure the wmixer
	VIOC_WMIX_SetSize(pWMIXer, width, height);
	VIOC_WMIX_SetOverlayPriority(pWMIXer, ovp);
	VIOC_WMIX_SetPosition(pWMIXer, vs_ch, out_posx, out_posy);
#if defined(CONFIG_OVERLAY_PGL)
	VIOC_WMIX_SetPosition(pWMIXer, pgl_ch, 0, 0);
	VIOC_WMIX_SetChromaKey(pWMIXer, chrom_layer, ON, key_R, key_G, key_B,
		mask_R, mask_G, mask_B);
#endif//defined(CONFIG_OVERLAY_PGL)
	VIOC_WMIX_SetUpdate(pWMIXer);
	VIOC_CONFIG_WMIXPath(vdev->cif.vioc_path.vin, ON);
end:

	FUNCTION_OUT
	return 0;
}

int tccvin_set_wmixer_out(tccvin_cif_t * cif, unsigned int ovp) {
	volatile void __iomem   * pWMIXer   = VIOC_WMIX_GetAddress(cif->vioc_path.wmixer_out);

	FUNCTION_IN
	dlog("WMIXer: 0x%p, OVP : %d \n", pWMIXer, ovp);

	VIOC_WMIX_SetOverlayPriority(pWMIXer, ovp);
	VIOC_WMIX_SetUpdate(pWMIXer);

	FUNCTION_OUT
	return 0;
}

int tccvin_set_ovp_value(tccvin_cif_t * cif) {
#if 0
	volatile void __iomem   * pWMIXer   = VIOC_WMIX_GetAddress(cif->vioc_path.wmixer_out);

	FUNCTION_IN
	dlog("WMIXer: 0x%p, OVP : %d \n");

    VIOC_WMIX_ALPHA_SetAlphaSelection(pWMIXer, 0, VIOC_WMIX_ALPHA_SEL3);
    VIOC_WMIX_ALPHA_SetROPMode(pWMIXer, 0, 0x18);
#endif
	FUNCTION_IN

    RCAM_OVP = 16;//24;

    dlog("set ovp %d \n", RCAM_OVP);
    tccvin_set_wmixer_out(cif, RCAM_OVP);

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
int tccvin_set_wdma(tccvin_dev_t * vdev) {
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);

	unsigned int	width	= vdev->v4l2.format.fmt.pix.width;
	unsigned int	height	= vdev->v4l2.format.fmt.pix.height;
	unsigned int	format	= convert_format(vdev->v4l2.format.fmt.pix.pixelformat, FORMAT_TYPE_V4L2, FORMAT_TYPE_VIOC);

	FUNCTION_IN
	dlog("WDMA: 0x%p, size[%d x %d], format[%d]. \n", pWDMA, width, height, format);

	VIOC_WDMA_SetImageFormat(pWDMA, format);
	VIOC_WDMA_SetImageSize(pWDMA, width, height);

#if 0
	if((((vdev->v4l2.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420) || (vdev->v4l2.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)) && \
		((width/2)%C_STRIDE_ALIGN!=0))) {// && (vdev->v4l2.oper_mode == OPER_PREVIEW)) {
		VIOC_WDMA_SetImageOffset_withYV12(pWDMA, width);
	}
	else
#endif
	{
		VIOC_WDMA_SetImageOffset(pWDMA, format, width);
	}

	tccvin_set_wdma_buf_addr(vdev, 0);
	VIOC_WDMA_SetImageEnable(pWDMA, ON);

	FUNCTION_OUT
	return 0;
}

int tccvin_set_rdma(tccvin_dev_t * vdev) {
	volatile void __iomem	* pRDMA		= VIOC_RDMA_GetAddress(vdev->cif.vioc_path.rdma);

	unsigned int	width		= vdev->v4l2.format.fmt.pix.width;
	unsigned int	height		= vdev->v4l2.format.fmt.pix.height;
	unsigned int	format		= convert_format(vdev->v4l2.format.fmt.pix.pixelformat, FORMAT_TYPE_V4L2, FORMAT_TYPE_VIOC);
	unsigned int	y2r			= 0;

	buf_addr_t		* buf_addr	= &vdev->cif.preview_buf_addr[0];

	FUNCTION_IN
	dlog("RDMA: 0x%p, size[%d x %d], format[%d]. \n", pRDMA, width, height, format);

//	VIOC_RDMA_SetImageR2YEnable(pRDMA, FALSE);
//	VIOC_RDMA_SetImageY2REnable(pRDMA, FALSE);

	VIOC_RDMA_SetImageFormat(pRDMA, format);
	switch(vdev->v4l2.format.fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		y2r	= 1;
		break;
	}
	VIOC_RDMA_SetImageY2REnable(pRDMA, y2r);
	VIOC_RDMA_SetImageSize(pRDMA, width, height);
	VIOC_RDMA_SetImageOffset(pRDMA, format, width);
	VIOC_RDMA_SetImageBase(pRDMA, buf_addr->y, buf_addr->u, buf_addr->v);
	VIOC_RDMA_SetImageEnable(pRDMA);
	VIOC_RDMA_SetImageUpdate(pRDMA);

	return 0;
}

void tccvin_set_fifo(tccvin_dev_t * vdev, int enable) {
	volatile void __iomem	* pFIFO		= VIOC_FIFO_GetAddress(vdev->cif.vioc_path.fifo);

	unsigned int			nWDMA		= get_vioc_index(vdev->cif.vioc_path.wdma);
	unsigned int			nRDMA		= get_vioc_index(vdev->cif.vioc_path.rdma);

	unsigned int			base_addr[4];
	int						idxBuf;

	dlog("WDMA: 0x%x, RDMA: 0x%x\n", nWDMA, nRDMA);

	if(enable) {
		for(idxBuf=0; idxBuf<4; idxBuf++) {
			base_addr[idxBuf] = vdev->cif.preview_buf_addr[idxBuf].y;
		}
		VIOC_FIFO_ConfigDMA(pFIFO, nWDMA, nRDMA, nRDMA, nRDMA);
		VIOC_FIFO_ConfigEntry(pFIFO, base_addr);
		VIOC_FIFO_SetEnable(pFIFO, 1, 1, 0, 0);
	} else {
		VIOC_FIFO_SetEnable(pFIFO, 0, 0, 0, 0);
	}
}

/*
 * list_get_entry_count
 *
 * - DESCRIPTION:
 *	Set rdma component to read video data
 *
 * - PARAMETERS:
 *	@vdev:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
unsigned int list_get_entry_count(struct list_head * head) {
	struct list_head	* list	= NULL;
	struct tccvin_buf	* p = NULL;
	unsigned int		count	= 0;

	list_for_each(list, head) {
		p = list_entry(list, struct tccvin_buf, buf_list);
//		printk("entry idx: %d\n", p->buf.index);
		count++;
	}
	return count;
}

int tccvin_v4l2_init_buffer_list(tccvin_dev_t * vdev) {
	struct tccvin_buf	* buf	= NULL;

	int idxBuf, nBuf = vdev->v4l2.pp_num;

	int capture_buf_entry_count	= 0;
	int display_buf_entry_count = 0;

	// Initialize the incoming and outgoing buffer list.
	INIT_LIST_HEAD(&vdev->v4l2.capture_buf_list);
	INIT_LIST_HEAD(&vdev->v4l2.display_buf_list);

	capture_buf_entry_count	= list_get_entry_count(&vdev->v4l2.capture_buf_list);
	display_buf_entry_count = list_get_entry_count(&vdev->v4l2.display_buf_list);
	dlog("cap count: %d, disp count: %d\n", capture_buf_entry_count, display_buf_entry_count);

	for(idxBuf=0; idxBuf<nBuf; idxBuf++) {
		buf = &vdev->v4l2.static_buf[idxBuf];
		INIT_LIST_HEAD(&buf->buf_list);
		list_add_tail(&buf->buf_list, &vdev->v4l2.capture_buf_list);
		buf->buf.flags |= V4L2_BUF_FLAG_QUEUED;
		buf->buf.flags &= ~V4L2_BUF_FLAG_DONE;
	}

	capture_buf_entry_count	= list_get_entry_count(&vdev->v4l2.capture_buf_list);
	display_buf_entry_count = list_get_entry_count(&vdev->v4l2.display_buf_list);
	dlog("cap count: %d, disp count: %d\n", capture_buf_entry_count, display_buf_entry_count);

	return 0;
}

void wdma_work_thread(struct work_struct * data) {
	tccvin_v4l2_t			* v4l2	= container_of(data, tccvin_v4l2_t, wdma_work);
	tccvin_dev_t			* vdev	= container_of(v4l2, tccvin_dev_t, v4l2);
	volatile void __iomem	* pWDMABase	= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);

	int						capture_buf_entry_count	= 0;
	struct tccvin_buf		* curr_buf	= NULL;
	struct tccvin_buf		* next_buf	= NULL;

	mutex_lock(&v4l2->lock);

	// Move the current capture buffer to the display buffer list,
	// only if there are more capture buffer than 2 in the capture buffer list.
	if(vdev->v4l2.skip_frame <= 0) {
		capture_buf_entry_count	= list_get_entry_count(&vdev->v4l2.capture_buf_list);
		if(1 < capture_buf_entry_count) {
			// Get the current capture buffer.
			curr_buf = list_entry(vdev->v4l2.capture_buf_list.next, struct tccvin_buf, buf_list);

			// Move the current capture buffer to the display buffer list.
			list_move_tail(&curr_buf->buf_list, &vdev->v4l2.display_buf_list);

			curr_buf->buf.flags |= V4L2_BUF_FLAG_DONE;

			// Get the next capture buffer.
			next_buf = list_entry(vdev->v4l2.capture_buf_list.next, struct tccvin_buf, buf_list);

			// Update the wdma's buffer address.
			tccvin_set_wdma_buf_addr(vdev, next_buf->buf.index);
			VIOC_WDMA_SetImageUpdate(pWDMABase); // update WDMA
		} else {
			dlog("The capture buffer is NOT changed.\n");
		}
	} else {
		log("skip frame count is %d \n", vdev->v4l2.skip_frame--);
	}

	mutex_unlock(&v4l2->lock);
}

static irqreturn_t tccvin_wdma_isr(int irq, void * client_data) {
	tccvin_dev_t			* vdev		= (tccvin_dev_t *)client_data;
	volatile void __iomem	* pWDMABase	= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	unsigned int			status;

	if(is_vioc_intr_activatied(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits) == false)
		return IRQ_NONE;

	// preview operation.
	if(vdev->v4l2.oper_mode == OPER_PREVIEW) {
		VIOC_WDMA_GetStatus(pWDMABase, &status);
		if(status & VIOC_WDMA_IREQ_EOFR_MASK) {
			schedule_work(&vdev->v4l2.wdma_work);

			vdev->v4l2.wakeup_int = 1;
			wake_up_interruptible(&(vdev->v4l2.frame_wait));
			vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
		}
	}

	return IRQ_HANDLED;
}

int tccvin_cif_set_resolution(tccvin_dev_t * vdev, unsigned int width, unsigned int height, unsigned int pixelformat) {
	vdev->v4l2.format.fmt.pix.width			= width;
	vdev->v4l2.format.fmt.pix.height		= height;
	vdev->v4l2.format.fmt.pix.pixelformat	= pixelformat;

	return 0;
}

int tccvin_allocate_essential_buffers(tccvin_dev_t * vdev) {
	int					ret = 0;

	strcpy(vdev->cif.pmap_viqe.name, "rearcamera_viqe");
	if(pmap_get_info(vdev->cif.pmap_viqe.name, &(vdev->cif.pmap_viqe)) == 1) {
		dlog("[PMAP] %s: 0x%08x ~ 0x%08x (0x%08x)\n",
			vdev->cif.pmap_viqe.name, \
			vdev->cif.pmap_viqe.base, \
			vdev->cif.pmap_viqe.base + vdev->cif.pmap_viqe.size, \
			vdev->cif.pmap_viqe.size);
	} else {
		loge("get \"rearcamera_viqe\" pmap information.\n");
		ret = -1;
	}

#if defined(CONFIG_OVERLAY_PGL)
	strcpy(vdev->cif.pmap_pgl.name, "parking_gui");
	if(pmap_get_info(vdev->cif.pmap_pgl.name, &(vdev->cif.pmap_pgl)) == 1) {
		dlog("[PMAP] %s: 0x%08x ~ 0x%08x (0x%08x)\n",
			vdev->cif.pmap_pgl.name, \
			vdev->cif.pmap_pgl.base, \
			vdev->cif.pmap_pgl.base + vdev->cif.pmap_pgl.size, \
			vdev->cif.pmap_pgl.size);
	} else {
		loge("get \"parking_gui\" pmap information.\n");
		ret = -1;
	}
#endif//defined(CONFIG_OVERLAY_PGL)

    return ret;
}

int tccvin_allocate_preview_buffers(tccvin_dev_t * vdev) {
	struct v4l2_buffer	req;
	int					idxBuf = 0, nBuf = 4;
	int					ret = 0;

	strcpy(vdev->cif.pmap_preview.name, "rearcamera");
	if(pmap_get_info(vdev->cif.pmap_preview.name, &(vdev->cif.pmap_preview)) == 1) {
		dlog("[PMAP] %s: 0x%08x ~ 0x%08x (0x%08x)\n",
			vdev->cif.pmap_preview.name, \
			vdev->cif.pmap_preview.base, \
			vdev->cif.pmap_preview.base + vdev->cif.pmap_preview.size, \
			vdev->cif.pmap_preview.size);
    } else {
		loge("get \"rearcamera\" pmap information.\n");
		ret = -1;
    }

	for(idxBuf=0; idxBuf<nBuf; idxBuf++) {
		req.index = idxBuf;
		req.reserved = vdev->cif.pmap_preview.base + (vdev->v4l2.format.fmt.pix.width * vdev->v4l2.format.fmt.pix.height * 4 * idxBuf);
		tccvin_v4l2_set_buffer_address(vdev, &req);
	}

    return ret;
}

int tccvin_start_stream(tccvin_dev_t * vdev) {
	tccvin_cif_t	* cif	= &vdev->cif;

	FUNCTION_IN

	// size info
	dlog("tgt: %d * %d\n", vdev->v4l2.format.fmt.pix.width, vdev->v4l2.format.fmt.pix.height);

	vdev->v4l2.oper_mode  = OPER_PREVIEW;

	// clear framebuffer
	tccvin_clear_buffer(vdev);

	// map cif-port
	tccvin_map_cif_port(vdev);

	// reset vioc path
	tccvin_reset_vioc_path(vdev);

	// set rdma for Parking Guide Line
#if defined(CONFIG_OVERLAY_PGL)
	if (vdev->cif.use_pgl == 1) {
		// enable rdma
		tccvin_set_pgl(vdev);
	}
#endif//defined(CONFIG_OVERLAY_PGL)

	// set vin
	tccvin_set_vin(vdev);

	// set deinterlacer
	if(!!(vdev->cif.videosource_format.interlaced & V4L2_DV_INTERLACED)) {
		// set Deinterlacer
		tccvin_set_deinterlacer(vdev);
	}

	// set scaler
	if(cif->vioc_path.vin <= VIOC_VIN30) {
		tccvin_set_scaler(cif, &vdev->v4l2);
	}

	// set wmixer
	if(cif->vioc_path.vin <= VIOC_VIN30) {
		tccvin_set_wmixer(vdev);
	}

	// set wdma
	tccvin_set_wdma(vdev);
	msleep(16 * 3);	// wait for the 3 frames in case of viqe 3d mode (16ms * 3 frames)

	if(vdev->v4l2.preview_method == PREVIEW_DD) {
		// set rdma
		tccvin_set_rdma(vdev);

		// set fifo
		tccvin_set_fifo(vdev, 1);

		// set wmixer_out
#ifndef CONFIG_TCC803X_CA7S
		tccvin_set_wmixer_out(cif, RCAM_OVP);
#endif//CONFIG_TCC803X_CA7S
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_stop_stream(tccvin_dev_t * vdev) {
#if defined(CONFIG_OVERLAY_PGL)
	volatile void __iomem	* pPGL		= VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);
#endif//defined(CONFIG_OVERLAY_PGL)
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	volatile void __iomem	* pRDMA		= VIOC_RDMA_GetAddress(vdev->cif.vioc_path.rdma);
	VIOC_PlugInOutCheck		vioc_plug_state;
	tccvin_cif_t	* cif	= &vdev->cif;

	int idxLoop;
	unsigned int status;

	FUNCTION_IN

	if(vdev->v4l2.preview_method == PREVIEW_DD) {
		// set wmixer_out
#ifndef CONFIG_TCC803X_CA7S
		tccvin_set_wmixer_out(cif, NORMAL_OVP);
#endif//CONFIG_TCC803X_CA7S

		// set fifo
		tccvin_set_fifo(vdev, 0);

		// set rdma
		VIOC_RDMA_SetImageDisable(pRDMA);
	}

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

		if(!!(vdev->cif.videosource_format.interlaced & V4L2_DV_INTERLACED)) {
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

#if defined(CONFIG_OVERLAY_PGL)
	if (vdev->cif.use_pgl == 1) {
		// disable rdma
		VIOC_RDMA_SetImageDisable(pPGL);
	}
#endif//defined(CONFIG_OVERLAY_PGL)

	// reset vioc path
	tccvin_reset_vioc_path(vdev);

	// clear framebuffer
	tccvin_clear_buffer(vdev);

	FUNCTION_OUT
	return 0;
}

int tccvin_request_irq(tccvin_dev_t * vdev) {
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
#endif
		vdev->cif.vioc_intr.bits	= VIOC_WDMA_IREQ_EOFR_MASK;

		if(vdev->cif.vioc_irq_reg == 0) {
			vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			ret = request_irq(vdev->cif.vioc_irq_num, tccvin_wdma_isr, IRQF_SHARED, vdev->vid_dev.name, vdev);
			vioc_intr_enable(vdev->cif.vioc_irq_num, vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			vdev->cif.vioc_irq_reg = 1;
		} else {
			loge("VIN[%d] The irq(%d) is already registered.\n", vdev->vid_dev.num, vdev->cif.vioc_irq_num);
			return -1;
		}
	} else {
		loge("VIN[%d] The irq node is NOT found.\n", vdev->vid_dev.num);
		return -1;
	}

	FUNCTION_OUT
	return ret;
}

int tccvin_free_irq(tccvin_dev_t * vdev) {
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

int tccvin_check_wdma_counter(tccvin_dev_t * vdev) {
	volatile void __iomem	* pWDMA	= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);

	volatile unsigned int prev_addr, curr_addr;
	volatile int nCheck, idxCheck, delay = 20;

	curr_addr = VIOC_WDMA_Get_CAddress(pWDMA);
	msleep(delay);

	nCheck = 4;
	for(idxCheck=0; idxCheck<nCheck; idxCheck++) {
		prev_addr = curr_addr;
		msleep(delay);
		curr_addr = VIOC_WDMA_Get_CAddress(pWDMA);

		if(prev_addr != curr_addr)
			return 0;
		else
			dlog("[%d] prev_addr: 0x%08x, curr_addr: 0x%08x\n", idxCheck, prev_addr, curr_addr);
	}
	return -1;
}

void tccvin_dump_register(unsigned long * addr, unsigned int word) {
	unsigned int	idxReg, nReg = word;

	for(idxReg=0; idxReg<nReg; idxReg++) {
		if((idxReg % 4) == 0)
			dlog("%p: ", (addr + idxReg));

		dlog("%08x ", *(addr + idxReg));
		if(((idxReg + 1) % 4) == 0)
			dlog("\n");
	}
	dlog("\n");
}

int tccvin_diagnostics_cif_port_mapping(tccvin_dev_t * vdev) {
	int		ret	= 0;

	log("Diagnostics of cif port mapping\n");

	ret = tccvin_check_cif_port_mapping(vdev);
	log("CIF Port Mapping is %s\n", (ret == 0) ? "OKAY" : "WRONG");
	if(ret < 0) {
		tccvin_dump_register((unsigned long *)vdev->cif.cifport_addr, 1);
	}
	
	return ret;
}

int tccvin_diagnostics_videoinput_path(tccvin_dev_t * vdev) {
	struct tccvin_diagnostics_reg_info {
		unsigned int	* addr;
		unsigned int	count;
	};
	struct tccvin_diagnostics_reg_info	list[] = {
#if defined(CONFIG_OVERLAY_PGL)
		{ (unsigned int *)VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl),	12 },
#endif//defined(CONFIG_OVERLAY_PGL)
		{ (unsigned int *)VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin),		16 },
//		{ (unsigned int *)VIOC_VIQE_GetAddress(vdev->cif.vioc_path.viqe),	 4 },
//		{ (unsigned int *)VIOC_VIQE_GetAddress(vdev->cif.vioc_path.deintl_s),	 4 },
		{ (unsigned int *)VIOC_SC_GetAddress(vdev->cif.vioc_path.scaler),	 8 },
		{ (unsigned int *)VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer),	28 },
		{ (unsigned int *)VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma),	18 },
	};
	unsigned int idxList = 0, nList = sizeof(list)/sizeof(list[0]);
	int		ret	= 0;

	dlog("Diagnostics of video-input path\n");

	for(idxList=0; idxList<nList; idxList++) {
		tccvin_dump_register((unsigned long *)list[idxList].addr, list[idxList].count);
	}
	dlog("\n\n");

	return ret;
}

int tccvin_diagnostics(tccvin_dev_t * vdev) {
	int		diagnostics_enable = 0;
	int		idxLoop = 0, nLoop = 1;
	int		ret	= 0;

	diagnostics_enable = atomic_read(&vdev->tccvin_attr_diagnostics);
	if(diagnostics_enable == 1) {
		for(idxLoop=0; idxLoop<nLoop; idxLoop++) {
			// check cif port mapping
			tccvin_diagnostics_cif_port_mapping(vdev);

			// check video-input path
			tccvin_diagnostics_videoinput_path(vdev);
		}
	}

	return ret;
}


/**************************************************
 *	PUBLIC FUNCTION LIST
 **************************************************/
int tccvin_get_clock(tccvin_dev_t * vdev) {
	struct device_node	* main_node	= vdev->dev_plt->of_node;
	int			ret		= 0;

	FUNCTION_IN

	vdev->cif.vioc_clk = of_clk_get(main_node, 0);
	ret = -IS_ERR(vdev->cif.vioc_clk);
	if(ret != 0) {
		loge("Find the \"clock\" node\n");
	}

	FUNCTION_OUT
	return ret;
}

void tccvin_put_clock(tccvin_dev_t * vdev) {
	FUNCTION_IN

	if(!IS_ERR(vdev->cif.vioc_clk))
		clk_put(vdev->cif.vioc_clk);

	FUNCTION_OUT
}

int tccvin_enable_clock(tccvin_dev_t * vdev) {
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

void tccvin_disable_clock(tccvin_dev_t * vdev) {
	FUNCTION_IN

	if(!IS_ERR(vdev->cif.vioc_clk))
		clk_disable_unprepare(vdev->cif.vioc_clk);

	FUNCTION_OUT
}

int tccvin_v4l2_init(tccvin_dev_t * vdev) {
	int		ret = 0;

	FUNCTION_IN

	// parse device tree
	ret = tccvin_parse_device_tree(vdev);
	if(ret != 0) {
		loge("Parse the device tree.\n");
		return -ENODEV;
	}

	// get the vioc's clock
	ret = tccvin_get_clock(vdev);
	if(ret != 0) {
		loge("tccvin_get_clock returns %d\n", ret);
		return ret;
	}

	// enable the vioc's clock
	ret = tccvin_enable_clock(vdev);
	if(ret != 0) {
		loge("tccvin_enable_clock returns %d\n", ret);
		return ret;
	}

	vdev->cif.is_handover_needed			= 0;

	vdev->v4l2.pp_num						= 0;
	vdev->v4l2.oper_mode					= OPER_PREVIEW;

	// pixel format
	vdev->v4l2.format.fmt.pix.width				= 1920;
	vdev->v4l2.format.fmt.pix.height			= 720;
	vdev->v4l2.format.fmt.pix.field				= V4L2_FIELD_ANY;
	vdev->v4l2.format.fmt.pix.sizeimage			= vdev->v4l2.format.fmt.pix.width * vdev->v4l2.format.fmt.pix.height * 4;
	vdev->v4l2.format.fmt.pix.pixelformat		= V4L2_PIX_FMT_RGB32;	//V4L2_PIX_FMT_YUYV;	// YUV422 is default

	// vin window
	vdev->v4l2.selection.r.left				= 0;
	vdev->v4l2.selection.r.top				= 0;
	vdev->v4l2.selection.r.width				= 0;
	vdev->v4l2.selection.r.height				= 0;

	// preview method
	vdev->v4l2.preview_method				= PREVIEW_V4L2;	//PREVIEW_DD;	// v4l2 buffering is default

	// Initialize the incoming and outgoing buffer list.
	INIT_LIST_HEAD(&vdev->v4l2.capture_buf_list);
	INIT_LIST_HEAD(&vdev->v4l2.display_buf_list);

	// init v4l2 resources
	mutex_init(&vdev->v4l2.lock);

	// init wait queue head and work thread
	init_waitqueue_head(&vdev->v4l2.frame_wait);
	INIT_WORK(&vdev->v4l2.wdma_work, wdma_work_thread);

	// allocate essential buffers
	tccvin_allocate_essential_buffers(vdev);

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_deinit(tccvin_dev_t * vdev) {
	int		ret = 0;

	FUNCTION_IN

	// deinit v4l2 resources
	mutex_destroy(&(vdev->v4l2.lock));

	// disable the vioc's clock
	tccvin_disable_clock(vdev);

	// put the vioc's clock
	tccvin_put_clock(vdev);

	FUNCTION_OUT
	return ret;
}

void tccvin_v4l2_querycap(tccvin_dev_t * vdev, struct v4l2_capability * cap) {
	FUNCTION_IN

	memset(cap, 0, sizeof(struct v4l2_capability));
	strlcpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
	strlcpy(cap->card, vdev->vid_dev.name, sizeof(cap->card));
	cap->bus_info[0] = '\0';
	cap->version = KERNEL_VERSION(4, 4, 00);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
						V4L2_CAP_VIDEO_OVERLAY |
						V4L2_CAP_READWRITE |
						V4L2_CAP_STREAMING;

	dlog("driver: %s, card: %s\n", cap->driver, cap->card);
	FUNCTION_OUT
}

int tccvin_v4l2_enum_fmt(struct v4l2_fmtdesc * fmt) {
	int		ret = 0;

	FUNCTION_IN

	FUNCTION_OUT
	return ret;
}

int tccvin_g_operation_mode(tccvin_dev_t * vdev, int * mode) {
	int		ret = 0;

	* mode = vdev->v4l2.preview_method;
	dlog("mode: 0x%08x\n", * mode);

	return ret;
}

int tccvin_s_operation_mode(tccvin_dev_t * vdev, int * mode) {
	int		ret = 0;

	if((* mode == PREVIEW_V4L2) || (* mode == PREVIEW_DD)) {
		vdev->v4l2.preview_method = * mode;
		dlog("mode: 0x%08x\n", vdev->v4l2.preview_method);
	} else {
		loge("The operation mode is WRONG, mode: 0x%08x\n", * mode);
		ret = -1;
	}

	return ret;
}

void tccvin_v4l2_g_fmt(tccvin_dev_t * vdev, struct v4l2_format * format) {
	FUNCTION_IN

	// return a struct data of v4l2_format depending on V4L2_BUF_TYPE_XXX
	switch(format->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		*format	= vdev->v4l2.format;
		break;

	default:
		loge("The v4l2 type(0x%x) is WRONG.\n", format->type);
		WARN_ON(1);
	}

	FUNCTION_OUT
}

int tccvin_v4l2_s_fmt(tccvin_dev_t * vdev, struct v4l2_format * format) {
	int		ret = 0;

	FUNCTION_IN

	// return a struct data of v4l2_format depending on V4L2_BUF_TYPE_XXX
	switch(format->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		vdev->v4l2.format	= *format;
		break;

	default:
		loge("The v4l2 type(0x%x) is WRONG.\n", format->type);
		WARN_ON(1);
	}

	ret = tccvin_cif_set_resolution(vdev, format->fmt.pix.width, format->fmt.pix.height, format->fmt.pix.pixelformat);

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_reqbufs(tccvin_dev_t * vdev, struct v4l2_requestbuffers * req) {
	int		ret = 0;

	FUNCTION_IN

	// check if total size asked to allocate memory is enough

	if(req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		loge("video type invalid: %d\n", req->type);
		return -EINVAL;
	}

	switch(req->memory) {
	case V4L2_MEMORY_MMAP:
		break;

	case V4L2_MEMORY_USERPTR:
		// allocate frame buffer memory
		tccvin_allocate_preview_buffers(vdev);
		break;

	case V4L2_MEMORY_OVERLAY:
		break;

	case V4L2_MEMORY_DMABUF:
		break;

	default:
		loge("reqbufs: memory type invalid\n");
		return -EINVAL;
	}

	vdev->v4l2.pp_num = req->count;

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_set_buffer_address(tccvin_dev_t * vdev, struct v4l2_buffer * req) {
	struct tccvin_buf	* buf = NULL;
	unsigned int		y_offset = 0, uv_offset = 0;//, stride = 0;

	FUNCTION_IN

	buf			= &vdev->v4l2.static_buf[req->index];
	buf->buf	= * req;

#if 0
	stride = ALIGNED_BUFF(vdev->v4l2.format.fmt.pix.width, L_STRIDE_ALIGN);
	y_offset = stride * vdev->v4l2.format.fmt.pix.height;
	uv_offset = ALIGNED_BUFF((stride/2), C_STRIDE_ALIGN) * (vdev->v4l2.format.fmt.pix.height/2);
#else
	y_offset = ROUND_UP_4(vdev->v4l2.format.fmt.pix.width) * ROUND_UP_2(vdev->v4l2.format.fmt.pix.height);
	uv_offset = (ROUND_UP_4(vdev->v4l2.format.fmt.pix.width) / 2) * (ROUND_UP_2(vdev->v4l2.format.fmt.pix.height) / 2);
#endif

	vdev->cif.preview_buf_addr[req->index].y = (unsigned long)req->reserved;
	vdev->cif.preview_buf_addr[req->index].u = (unsigned long)vdev->cif.preview_buf_addr[req->index].y + y_offset;
	vdev->cif.preview_buf_addr[req->index].v = (unsigned long)vdev->cif.preview_buf_addr[req->index].u + uv_offset;

	vdev->v4l2.pp_num = (req->index + 1);

	dlog("buf[%2d] - Y: 0x%x, U: 0x%x, V: 0x%x\n", req->index, vdev->cif.preview_buf_addr[req->index].y, vdev->cif.preview_buf_addr[req->index].u, vdev->cif.preview_buf_addr[req->index].v);

	FUNCTION_OUT
	return 0;
}

int tccvin_v4l2_querybuf(tccvin_dev_t * vdev, struct v4l2_buffer * buf) {
	int							index		= buf->index;

	FUNCTION_IN

	// Check the buffer index is valid.
	if(!((0 <= buf->index) && (buf->index < vdev->v4l2.pp_num))) {
		loge("index: %d, total: %d", index, vdev->v4l2.pp_num);
		return -EINVAL;
	} else {
		* buf = vdev->v4l2.static_buf[index].buf;
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_v4l2_qbuf(tccvin_dev_t * vdev, struct v4l2_buffer * buf) {
	struct tccvin_buf	* cif_buf	= &vdev->v4l2.static_buf[buf->index];

//	FUNCTION_IN

	// Check the buffer index is valid.
	if(!((0 <= buf->index) && \
		(buf->index < vdev->v4l2.pp_num) && \
		((cif_buf->buf.flags & V4L2_BUF_FLAG_QUEUED) != V4L2_BUF_FLAG_QUEUED))) {
		loge("The buffer index(%d) is WRONG.\n", buf->index);
		return -EAGAIN;
	}

	// Check the buffer type is valid.
	if(buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		loge("The buffer type(0x%08x) is WRONG.\n", buf->type);
		return -EAGAIN;
	}

	// Queue the buffer to the capture buffer list
	list_add_tail(&cif_buf->buf_list, &(vdev->v4l2.capture_buf_list));
	dlog("cap count: %d\n", list_get_entry_count(&vdev->v4l2.capture_buf_list));

	// Set the flag V4L2_BUF_FLAG_QUEUED
	cif_buf->buf.flags |= V4L2_BUF_FLAG_QUEUED;

	// clear the flag V4L2_BUF_FLAG_DONE
	cif_buf->buf.flags &= ~V4L2_BUF_FLAG_DONE;

//	FUNCTION_OUT
	return 0;
}

int tccvin_v4l2_dqbuf(struct file * file, struct v4l2_buffer * buf) {
	tccvin_dev_t		* vdev		= video_drvdata(file);
	struct tccvin_buf	* cif_buf	= NULL;
	int					display_buf_entry_count	= 0;
	int					timeout		= 100;	// 16ms * 3 frames => 100ms
	int					ret			= 0;

//	FUNCTION_IN

	display_buf_entry_count = list_get_entry_count(&vdev->v4l2.display_buf_list);
	dlog("disp count: %d\n", display_buf_entry_count);

	if(1 < display_buf_entry_count) {
		// get a buffer from the display buffer list to display it
		cif_buf = list_entry(vdev->v4l2.display_buf_list.next, struct tccvin_buf, buf_list);

		// delete the buffer from the display buffer list, not free the budder memory
		list_del(vdev->v4l2.display_buf_list.next);

		// Clear the flag V4L2_BUF_FLAG_QUEUED
		cif_buf->buf.flags &= ~V4L2_BUF_FLAG_QUEUED;

		memcpy(buf, &(cif_buf->buf), sizeof(struct v4l2_buffer));
	}

	if(1 >= display_buf_entry_count) {
		dlog("The display buffer list is EMPTY!!\n");
		vdev->v4l2.wakeup_int = 0;
		if(wait_event_interruptible_timeout(vdev->v4l2.frame_wait, vdev->v4l2.wakeup_int == 1, msecs_to_jiffies(timeout)) <= 0) {
			dlog("wait_event_interruptible_timeout %dms!!\n", timeout);
			return -EFAULT;
		}
		ret = -ENOMEM;
	}

//	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_streamon(tccvin_dev_t * vdev, int is_handover_needed) {
	int		ret = 0;
    int     skip = 0;
	FUNCTION_IN

	// update the is_handover_needed
	vdev->cif.is_handover_needed	= is_handover_needed;

	log("preview method: %s\n", (vdev->v4l2.preview_method == PREVIEW_V4L2) ? "PREVIEW_V4L2" : "PREVIEW_DD");

	if(vdev->v4l2.preview_method == PREVIEW_V4L2) {
		// init the v4l2 buffer list
		tccvin_v4l2_init_buffer_list(vdev);
	} else if((vdev->v4l2.preview_method & PREVIEW_METHOD_MASK) == PREVIEW_DD) {
        // If it is handover, Skip start stream function
		if((vdev->v4l2.preview_method & PREVIEW_DD_HANDOVER_MASK) == PREVIEW_DD_HANDOVER)
			skip = 1;
	}

	if(vdev->cif.is_handover_needed) {
		vdev->cif.is_handover_needed = 0;
		log("#### Handover - Skip to set the vioc path\n");
		return 0;
	}

	if(0) {//tccvin_check_wdma_counter(vdev) == 0) {
		dlog("Video-Input Path(%d) is already working\n", vdev->vid_dev.num);
	} else {
		dlog("Video-Input Path(%d) is NOT working\n", vdev->vid_dev.num);
		ret = tccvin_start_stream(vdev);
		if(ret < 0) {
			loge("Start Stream\n");
			return -1;
		}
	}

	if(vdev->v4l2.preview_method == PREVIEW_V4L2) {
		ret = tccvin_request_irq(vdev);
		if(ret < 0) {
			loge("Request IRQ\n");
			return ret;
		}
	}

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_streamoff(tccvin_dev_t * vdev, int is_handover_needed) {
	int		ret = 0;

	FUNCTION_IN

	if(vdev->v4l2.preview_method == PREVIEW_V4L2) {
		ret = tccvin_free_irq(vdev);
		if(ret < 0) {
			loge("Free IRQ\n");
			return ret;
		}
	}

	ret = tccvin_stop_stream(vdev);
	if(ret < 0) {
		loge("Stop Stream\n");
		return -1;
	}

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_g_param(tccvin_dev_t * vdev, struct v4l2_streamparm * gparam) {
	memset(gparam, 0x00, sizeof(* gparam));
	gparam->parm.capture.capturemode = vdev->v4l2.oper_mode;

	return 0;
}

int tccvin_v4l2_s_param(struct v4l2_streamparm * sparam) {
	return 0;
}

int tccvin_v4l2_enum_input(struct v4l2_input * input) {
	int index = input->index;

	memset(input, 0, sizeof(* input));
	input->index = index;

	if (index > 0)
		return -EINVAL;

	strlcpy(input->name, "camera", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;

	return 0;
}

int tccvin_v4l2_g_input(tccvin_dev_t * vdev, struct v4l2_input * input) {
	* input = vdev->v4l2.input;

	return 0;
}

int tccvin_v4l2_s_input(tccvin_dev_t * vdev, struct v4l2_input * input) {
	vdev->v4l2.input = * input;

	return 0;
}

int tccvin_v4l2_try_fmt(struct v4l2_format * fmt) {
	return 0;
}

void tccvin_check_path_status(tccvin_dev_t * vdev, int * status) {
	if(vdev->v4l2.preview_method == PREVIEW_V4L2) {
		* status = vdev->v4l2.wakeup_int;
	} else {
		* status = (tccvin_check_wdma_counter(vdev) == 0) ? 1 : 0;
	}
	dlog("status: %d\n", * status);
}

