
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
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP

#include "tccvin_dev.h"

static int					debug = 0;
#define TAG					"tccvin_dev"
#define log(msg, arg...)	do { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } while(0)
#define dlog(msg, arg...)	do { if(debug) { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } } while(0)
#define FUNCTION_IN			dlog("IN\n");
#define FUNCTION_OUT		dlog("OUT\n");

#define	DRIVER_NAME		"avn-camera"

//#define NORMAL_OVP      0x18
static unsigned int NORMAL_OVP = 0x18;

//#define RCAM_OVP        0x8
static unsigned int RCAM_OVP = 0x8;

static unsigned long val_cifport0 = 0;

#define ALIGNED_BUFF(buf, mul) ( ( (unsigned int)buf + (mul-1) ) & ~(mul-1) )

#define ROUND_UP_2(num) 	(((num)+1)&~1)
#define ROUND_UP_4(num) 	(((num)+3)&~3)

enum format_conversion_type {
	FORMAT_TYPE_VIOC,
	FORMAT_TYPE_V4L2,
	FORMAT_TYPE_MAX,
};

int tccvin_direct_display_start_monitor(tccvin_dev_t * vdev);
int tccvin_direct_display_stop_monitor(tccvin_dev_t * vdev);
int tccvin_direct_display_set_pgl(tccvin_dev_t * vdev);

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

unsigned int convert_format(unsigned int format, unsigned int from_type, unsigned int to_type) {
	int idxEntry = 0, nEntry = sizeof(format_conversion_table) / sizeof(format_conversion_table[0]);
	unsigned int	ret = 0;

	for(idxEntry=0; idxEntry<nEntry; idxEntry++) {
		if(format_conversion_table[idxEntry].format[from_type] == format) {
			ret = format_conversion_table[idxEntry].format[to_type];
			dlog("type: %d, format: 0x%08x -> type: %d, format: 0x%08x\n", from_type, format, to_type, ret);
			break;
		}
	}

	return ret;
}

void tccvin_set_wdma_buf_addr(tccvin_dev_t * vdev, unsigned int idxBuf) {
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	unsigned int			format		= vdev->v4l2.pix_format.pixelformat;
	buf_addr_t				* buf_addr	= &vdev->cif.preview_buf_addr[idxBuf];
	unsigned int			addr0 = 0, addr1 = 0, addr2 = 0;

	switch(format) {
	case V4L2_PIX_FMT_RGB24:
		dlog("V4L2_PIX_FMT_RGB24\n");
	case V4L2_PIX_FMT_RGB32:
		dlog("V4L2_PIX_FMT_RGB32\n");
	case V4L2_PIX_FMT_YUYV:
		dlog("V4L2_PIX_FMT_YUYV\n");
		addr0	= buf_addr->y;
        break;

	case V4L2_PIX_FMT_NV12:
		dlog("V4L2_PIX_FMT_NV12\n");

	case V4L2_PIX_FMT_NV21:
		dlog("V4L2_PIX_FMT_NV21\n");
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

int tccvin_parse_device_tree(tccvin_dev_t * vdev) {
	struct device_node		* main_node	= vdev->plt_dev->dev.of_node;
	struct device_node		* vioc_node	= NULL;
	struct device_node		* np_fb = NULL, * np_fb_1st = NULL;
	volatile void __iomem	* address	= NULL;

	if(main_node == NULL) {
		log("ERROR: tcc_camera device node is not found.\n");
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
		log("ERROR: \"vin\" node is not found.\n");
		return -ENODEV;
	}

	// cif port
	vioc_node = of_parse_phandle(main_node, "cifport", 0);
	if(vioc_node != NULL) {
		vdev->cif.cifport_addr = of_iomap(vioc_node, 0);
		of_property_read_u32_index(main_node, "cifport", 1, &vdev->cif.cif_port);
		dlog("cifport index: %d\n", vdev->cif.cif_port);

		val_cifport0 &= ~(0xF << ((get_vioc_index(vdev->cif.vioc_path.vin) / 2) * 4));
		val_cifport0 |= (vdev->cif.cif_port << ((get_vioc_index(vdev->cif.vioc_path.vin) / 2) * 4));

		dlog("val_cifport0: %ld\n", val_cifport0);
	} else {
		log("ERROR: The CIF port node is NULL\n");
		return -ENODEV;
	}

    // Parking Guide Line
    vdev->cif.vioc_path.pgl = -1;

    // VIDEO_IN04~06 don't have RDMA
    if(vdev->cif.vioc_path.vin >= VIOC_VIN00 && vdev->cif.vioc_path.vin <= VIOC_VIN30) {
        vioc_node = of_parse_phandle(main_node, "rdma", 0);
        if(vioc_node != NULL) {
            of_property_read_u32_index(main_node, "rdma", 1, &vdev->cif.vioc_path.pgl);
            if(vdev->cif.vioc_path.pgl != -1) {
                address = VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);
                dlog("%10s[%2d]: 0x%p\n", "RDMA(PGL)", get_vioc_index(vdev->cif.vioc_path.rdma), address);
            }
        } else {
            log("ERROR: \"rdma\" node is not found.\n");
            return -ENODEV;
        }
    }

	// VIQE
	vdev->cif.vioc_path.deintl = -1;
	vioc_node = of_parse_phandle(main_node, "viqe", 0);
	if(vioc_node) {
		of_property_read_u32_index(main_node, "viqe", 1, &vdev->cif.vioc_path.deintl);
		if(vdev->cif.vioc_path.deintl != -1) {
			address = VIOC_VIQE_GetAddress(vdev->cif.vioc_path.deintl);
			dlog("%10s[%2d]: 0x%p\n", "VIQE", get_vioc_index(vdev->cif.vioc_path.deintl), address);
		}
	} else {
		dlog("\"viqe\" node is not found.\n");

		// DEINTL_S
		vioc_node = of_parse_phandle(main_node, "deintls", 0);
		if(vioc_node) {
			vdev->cif.vioc_path.deintl = -1;
			of_property_read_u32_index(main_node, "deintls", 1, &vdev->cif.vioc_path.deintl);
			if(vdev->cif.vioc_path.deintl != -1) {
				address = VIOC_DEINTLS_GetAddress();//vdev->cif.vioc_path.deintl);
				dlog("%10s[%2d]: 0x%p\n", "DEINTL_S", get_vioc_index(vdev->cif.vioc_path.deintl), address);
			}
		} else {
			dlog("\"deintls\" node is not found.\n");
		}
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
	vioc_node = of_parse_phandle(main_node, "wdma", 0);
	if(vioc_node != NULL) {
		of_property_read_u32_index(main_node, "wdma", 1, &vdev->cif.vioc_path.wdma);
		if(vdev->cif.vioc_path.wdma != -1) {
			address = VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
			dlog("%10s[%2d]: 0x%p\n", "WDMA", get_vioc_index(vdev->cif.vioc_path.wdma), address);
		}
	} else {
		log("ERROR: \"wdma\" node is not found.\n");
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
					of_property_read_u32_index(np_fb_1st, "telechips,rdma", 1 + 2, &vdev->cif.vioc_path.rdma);
					if(vdev->cif.vioc_path.rdma != -1) {
						address = VIOC_RDMA_GetAddress(vdev->cif.vioc_path.rdma);
						dlog("%10s[%2d]: 0x%p\n", "RDMA", get_vioc_index(vdev->cif.vioc_path.rdma), address);
					}
				} else {
					log("ERROR: \"rdma\" node is not found.\n");
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
					log("ERROR: \"wmixer_out\" node is not found.\n");
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
					log("ERROR: \"telechips,disp\" node is not found.\n");
					return -ENODEV;
				}
			} else {
				log("ERROR: \"fbdisplayn\" node is not found.\n");
				return -ENODEV;
			}
		} else {
			log("ERROR: \"telechips,fbdisplay_num\" node is not found.\n");
			return -ENODEV;
		}
	} else {
		log("ERROR: \"telechips,vioc-fb\" node is not found.\n");
		return -ENODEV;
	}

	return 0;
}

#if defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
#include <linux/fs.h>
#include <video/tcc/vioc_mgr.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#endif//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)

int vioc_component_reset(unsigned int component, unsigned int mode) {
#if defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	struct file				* file		= NULL;
	char					name[1024];
	struct tcc_mbox_data	data;
#endif//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	int						ret = 0;

	FUNCTION_IN

#if defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)
	// open video-input driver
	sprintf(name, "/dev/vioc_mgr_a7s");

	file = filp_open(name, O_RDWR, 0666);
	if(file == NULL) {
		log("error: driver open fail (%s)\n", name);
		return -1;
	}

	memset(&data, 0x0, sizeof(struct tcc_mbox_data));
	data.cmd[1] = (VIOC_CMD_RESET & 0xFFFF) << 16;
	data.data[0] = component;
	data.data[1] = mode;
	data.data_len = 2;

	ret = file->f_op->unlocked_ioctl(file, IOCTL_VIOC_MGR_SET_RESET_KERNEL, (unsigned long)&data);
	if(ret  < 0) {
		log("ERROR: IOCTL_VIOC_MGR_SET_RESET_KERNEL\n");
	}

	// Close the video input device
	filp_close(file, 0);
#else
	VIOC_CONFIG_SWReset(component, mode);
#endif//defined(CONFIG_TCC803X_CA7S) && defined(CONFIG_VIOC_MGR)

	FUNCTION_OUT
	return ret;
}

int tccvin_reset_vioc_path(tccvin_dev_t * vdev) {
	vioc_path_t		* vioc	= &vdev->cif.vioc_path;

	FUNCTION_IN

	if(vdev->v4l2.preview_method == PREVIEW_DD) {
		if(vioc->fifo	!= -1)	vioc_component_reset(vioc->fifo,	VIOC_CONFIG_RESET);
		if(vioc->rdma	!= -1)	vioc_component_reset(vioc->rdma,	VIOC_CONFIG_RESET);
	}
	if(vioc->wdma	!= -1)	vioc_component_reset(vioc->wdma,		VIOC_CONFIG_RESET);
	if(vioc->wmixer	!= -1)	vioc_component_reset(vioc->wmixer,		VIOC_CONFIG_RESET);
	if(vioc->scaler	!= -1)	vioc_component_reset(vioc->scaler,		VIOC_CONFIG_RESET);
	if(vioc->deintl	!= -1)	vioc_component_reset(vioc->deintl,		VIOC_CONFIG_RESET);
	if(vioc->vin	!= -1)	vioc_component_reset(vioc->vin,			VIOC_CONFIG_RESET);
#ifdef CONFIG_OVERLAY_PGL
	if(vdev->v4l2.preview_method == PREVIEW_DD) {
        if(vioc->pgl	!= -1)	vioc_component_reset(vioc->pgl,		VIOC_CONFIG_RESET);
    }
#endif
	mdelay(1);
#ifdef CONFIG_OVERLAY_PGL
    if(vdev->v4l2.preview_method == PREVIEW_DD) {
        if(vioc->pgl	!= -1)	vioc_component_reset(vioc->pgl,		VIOC_CONFIG_CLEAR);
    }
#endif
	if(vioc->vin	!= -1)	vioc_component_reset(vioc->vin,			VIOC_CONFIG_CLEAR);
	if(vioc->deintl	!= -1)	vioc_component_reset(vioc->deintl,		VIOC_CONFIG_CLEAR);
	if(vioc->scaler	!= -1)	vioc_component_reset(vioc->scaler,		VIOC_CONFIG_CLEAR);
	if(vioc->wmixer	!= -1)	vioc_component_reset(vioc->wmixer,		VIOC_CONFIG_CLEAR);
	if(vioc->wdma	!= -1)	vioc_component_reset(vioc->wdma,		VIOC_CONFIG_CLEAR);
	if(vdev->v4l2.preview_method == PREVIEW_DD) {
		if(vioc->rdma	!= -1)	vioc_component_reset(vioc->rdma,	VIOC_CONFIG_CLEAR);
		if(vioc->fifo	!= -1)	vioc_component_reset(vioc->fifo,	VIOC_CONFIG_CLEAR);
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_set_cif_port(tccvin_dev_t * vdev) {
	volatile void __iomem * cifport_addr	= vdev->cif.cifport_addr;
	unsigned int			cifport_num 	= vdev->cif.cif_port;

	dlog("cifport address: 0x%p value : 0x%lu, index: %d\n", cifport_addr, val_cifport0, cifport_num);

	__raw_writel(val_cifport0, cifport_addr);

	return 0;
}

void tccvin_clear_buffer(tccvin_dev_t * vdev) {
//	struct v4l2_pix_format	* pix_format = &vdev->v4l2.pix_format;
	int						idxBuf = 0, nBuf = vdev->v4l2.pp_num;//MAX_BUFFERRS;

	for(idxBuf=0; idxBuf<nBuf; idxBuf++) {
//		memset(vdev->cif.preview_buf_addr, 0x00, req->count * sizeof(struct tccvin_buf));
	}
}

int tccvin_set_vin(tccvin_dev_t * vdev) {
	volatile void __iomem	* pVIN		= VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin);
	TCC_SENSOR_INFO_TYPE	* vs_info	= vdev->cif.videosource_info;

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

	FUNCTION_IN
	dlog("VIN: 0x%p, Source Size - width: %d, height: %d\n", pVIN, width, height);

	VIOC_VIN_SetSyncPolarity(pVIN, hs_active_low, vs_active_low, field_bfield_low, de_active_low, gen_field_en, pxclk_pol);
	VIOC_VIN_SetCtrl(pVIN, conv_en, hsde_connect_en, vs_mask, data_format, data_order);
	VIOC_VIN_SetInterlaceMode(pVIN, interlaced, intpl_en);
	VIOC_VIN_SetImageSize(pVIN, width, height);
	VIOC_VIN_SetImageOffset(pVIN, 0, 0, 0);
	VIOC_VIN_SetImageCropSize(pVIN, width, height);
	VIOC_VIN_SetImageCropOffset(pVIN, 0, 0);
	VIOC_VIN_SetY2RMode(pVIN, 2);

	if(((vdev->cif.videosource_info->data_format == FMT_YUV422_16BIT) || \
		(vdev->cif.videosource_info->data_format == FMT_YUV422_8BIT)) && \
		((vdev->v4l2.pix_format.pixelformat == V4L2_PIX_FMT_RGB24) ||	\
		(vdev->v4l2.pix_format.pixelformat == V4L2_PIX_FMT_RGB32))) {

		if(!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED))
			VIOC_VIN_SetY2REnable(pVIN, ON);
	}
	else {
		VIOC_VIN_SetY2REnable(pVIN, OFF);
	}

	VIOC_VIN_SetEnable(pVIN, ON);

	FUNCTION_OUT
	return 0;
}

int tccvin_set_deinterlacer(tccvin_dev_t * vdev) {
//	struct device_node * main_node	= vdev->plt_dev->dev.of_node;
//	struct device_node * hdl_np		= NULL;

	volatile void __iomem	* pVIQE	= VIOC_VIQE_GetAddress(vdev->cif.vioc_path.deintl);

	unsigned int	interlaced		= !!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED);
	unsigned int	width			= vdev->cif.videosource_info->width;
	unsigned int	height			= vdev->cif.videosource_info->height >> interlaced;

	unsigned int	viqe_width		= 0;
	unsigned int	viqe_height		= 0;
	unsigned int	format			= VIOC_VIQE_FMT_YUV422;
	unsigned int	bypass_deintl	= VIOC_VIQE_DEINTL_MODE_3D;
	unsigned int	offset			= width * height * 2 * 2;
	unsigned int	deintl_base0	= vdev->cif.pmap_viqe.base;
	unsigned int	deintl_base1	= deintl_base0 + offset;
	unsigned int	deintl_base2	= deintl_base1 + offset;
	unsigned int	deintl_base3	= deintl_base2 + offset;

	unsigned int	cdf_lut_en		= OFF;
	unsigned int	his_en			= OFF;
	unsigned int	gamut_en		= OFF;
	unsigned int	d3d_en			= OFF;
	unsigned int	deintl_en		= ON;
//	unsigned int	* viqe_set_reg1	= NULL;
//	unsigned int	* viqe_set_reg2	= NULL;

	FUNCTION_IN
	dlog("VIQE: 0x%p, Source Size - width: %d, height: %d\n", pVIQE, width, height);

#if 0
	if(!(hdl_np = of_parse_phandle(main_node, "viqe_set", 0))) {
		printk("could not find cam_viqe_set node!! \n");
	} else {
		viqe_set_reg1 = (unsigned int *)of_iomap(hdl_np, is_VIOC_REMAP ? 2 : 0);
		viqe_set_reg2 = (unsigned int *)of_iomap(hdl_np, is_VIOC_REMAP ? 3 : 1);

		BITCSET(*viqe_set_reg1,1<<3,1<<3);
		BITCSET(*viqe_set_reg2,1<<8 | 1<<9 , 1<<8 | 1<<9);
	}
#endif

	if(vdev->cif.vioc_path.vin <= VIOC_VIN30) {
		VIOC_CONFIG_PlugIn(vdev->cif.vioc_path.deintl, vdev->cif.vioc_path.vin);

		if(((vdev->cif.videosource_info->data_format == FMT_YUV422_16BIT) || \
			(vdev->cif.videosource_info->data_format == FMT_YUV422_8BIT)) && \
		   ((vdev->v4l2.pix_format.pixelformat == V4L2_PIX_FMT_RGB24) ||	\
			(vdev->v4l2.pix_format.pixelformat == V4L2_PIX_FMT_RGB32))) {
			VIOC_VIQE_SetImageY2RMode(pVIQE, 2);
			VIOC_VIQE_SetImageY2REnable(pVIQE, ON);
		}
		VIOC_VIQE_SetControlRegister(pVIQE, viqe_width, viqe_height, format);
		VIOC_VIQE_SetDeintlRegister(pVIQE, format, OFF, viqe_width, viqe_height, bypass_deintl,	\
			deintl_base0, deintl_base1, deintl_base2, deintl_base3);
		VIOC_VIQE_SetControlEnable(pVIQE, cdf_lut_en, his_en, gamut_en, d3d_en, deintl_en);
		VIOC_VIQE_SetDeintlModeWeave(pVIQE);
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_set_scaler(tccvin_cif_t * cif, struct v4l2_pix_format * pix_format) {
	volatile void __iomem	* pSC		= VIOC_SC_GetAddress(cif->vioc_path.scaler);

	unsigned int	width	= pix_format->width;
	unsigned int	height	= pix_format->height;

	unsigned int	crop_x	= cif->videosource_info->crop_x;
	unsigned int	crop_y	= cif->videosource_info->crop_y;

	unsigned int	crop_w	= cif->videosource_info->crop_w;
	unsigned int	crop_h	= cif->videosource_info->crop_h;

	FUNCTION_IN
	dlog("SC: 0x%p, Output Size - width: %d, height: %d\n", pSC, width, height);

	if((cif->videosource_info->width != pix_format->width || cif->videosource_info->height != pix_format->height) || \
			(crop_w != 0 || crop_h != 0)) {
			// Plug the scaler in
			VIOC_CONFIG_PlugIn(cif->vioc_path.scaler, cif->vioc_path.vin);

			// Configure the scaler
			VIOC_SC_SetBypass(pSC, OFF);
			VIOC_SC_SetDstSize(pSC, width + crop_w, height + crop_h + 1);		// workaround: scaler margin
			VIOC_SC_SetOutPosition(pSC, crop_x, crop_y);
			VIOC_SC_SetOutSize(pSC, width, height);
			VIOC_SC_SetUpdate(pSC);
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_set_wmixer(tccvin_dev_t * vdev) {
	volatile void __iomem	* pWMIXer	= VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer);

	unsigned int	width	= vdev->v4l2.pix_format.width;
	unsigned int	height	= vdev->v4l2.pix_format.height;

	FUNCTION_IN
	dlog("WMIXer: 0x%p, Size - width: %d, height: %d\n", pWMIXer, width, height);

	// Configure the wmixer
#ifdef CONFIG_OVERLAY_PGL
    if(vdev->v4l2.preview_method == PREVIEW_DD) {
        unsigned int layer		    = 0x0;
        unsigned int key_R		    = PGL_BG_R;
        unsigned int key_G		    = PGL_BG_G;
        unsigned int key_B		    = PGL_BG_B;
        unsigned int key_mask_R 	= ((PGL_BG_R >> 3) << 3 );
        unsigned int key_mask_G 	= ((PGL_BG_G >> 3) << 3 );
        unsigned int key_mask_B 	= ((PGL_BG_B >> 3) << 3 );

        VIOC_WMIX_SetSize(pWMIXer, width, height);
        VIOC_WMIX_SetPosition(pWMIXer, 1, 0, 0);
        VIOC_WMIX_SetChromaKey(pWMIXer, layer, ON, key_R, key_G, key_B, key_mask_R, key_mask_G, key_mask_B);
        VIOC_WMIX_SetUpdate(pWMIXer);

        VIOC_CONFIG_WMIXPath(vdev->cif.vioc_path.vin, ON);	// ON: Mixing mode / OFF: Bypass mode
    }
#else
    VIOC_CONFIG_WMIXPath(vdev->cif.vioc_path.vin, OFF);	// ON: Mixing mode / OFF: Bypass mode
#endif

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

    RCAM_OVP = 24;

    dlog("set ovp %d \n", RCAM_OVP);
    tccvin_set_wmixer_out(cif, RCAM_OVP);

	FUNCTION_OUT
	return 0;
}

int tccvin_set_wdma(tccvin_dev_t * vdev) {
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);

	unsigned int	width	= vdev->v4l2.pix_format.width;
	unsigned int	height	= vdev->v4l2.pix_format.height;
	unsigned int	format	= convert_format(vdev->v4l2.pix_format.pixelformat, FORMAT_TYPE_V4L2, FORMAT_TYPE_VIOC);

	FUNCTION_IN
	dlog("WDMA: 0x%p, size[%d x %d], format[%d]. \n", pWDMA, width, height, format);

	VIOC_WDMA_SetImageFormat(pWDMA, format);
	VIOC_WDMA_SetImageSize(pWDMA, width, height);

#if 0
	if((((vdev->v4l2.pix_format.pixelformat == V4L2_PIX_FMT_YVU420) || (vdev->v4l2.pix_format.pixelformat == V4L2_PIX_FMT_YUV420)) && \
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

	unsigned int	width		= vdev->v4l2.pix_format.width;
	unsigned int	height		= vdev->v4l2.pix_format.height;
	unsigned int	format		= convert_format(vdev->v4l2.pix_format.pixelformat, FORMAT_TYPE_V4L2, FORMAT_TYPE_VIOC);
	unsigned int	y2r			= DISABLE;

	buf_addr_t		* buf_addr	= &vdev->cif.preview_buf_addr[0];

	FUNCTION_IN
	dlog("RDMA: 0x%p, size[%d x %d], format[%d]. \n", pRDMA, width, height, format);

//	VIOC_RDMA_SetImageR2YEnable(pRDMA, FALSE);
//	VIOC_RDMA_SetImageY2REnable(pRDMA, FALSE);

	VIOC_RDMA_SetImageFormat(pRDMA, format);
	switch(vdev->v4l2.pix_format.pixelformat) {
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
		y2r	= ENABLE;
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

	int idxBuf, nBuf = vdev->v4l2.pp_num;//MAX_BUFFERRS;

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
		buf->buf.flags |= (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE);
	}

	capture_buf_entry_count	= list_get_entry_count(&vdev->v4l2.capture_buf_list);
	display_buf_entry_count = list_get_entry_count(&vdev->v4l2.display_buf_list);
	dlog("cap count: %d, disp count: %d\n", capture_buf_entry_count, display_buf_entry_count);

	return 0;
}

static unsigned int skip_frame = 0;

unsigned int tccvin_set_skip_frame(unsigned int num) {
	FUNCTION_IN

	skip_frame = num;

	log("skip_frame : %d \n", skip_frame);

	FUNCTION_OUT

	return skip_frame;
}

EXPORT_SYMBOL(tccvin_set_skip_frame);

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
	if(skip_frame <= 0) {
		capture_buf_entry_count	= list_get_entry_count(&vdev->v4l2.capture_buf_list);
		if(1 < capture_buf_entry_count) {
			// Get the current capture buffer.
			curr_buf = list_entry(vdev->v4l2.capture_buf_list.next, struct tccvin_buf, buf_list);

			// Move the current capture buffer to the display buffer list.
			list_move_tail(&curr_buf->buf_list, &vdev->v4l2.display_buf_list);

			curr_buf->buf.flags &= ~V4L2_BUF_FLAG_QUEUED;
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
		log("skip frame count is %d \n", skip_frame--);
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
		if(status & VIOC_WDMA_IREQ_EOFF_MASK) {
			schedule_work(&vdev->v4l2.wdma_work);

			vdev->v4l2.wakeup_int = 1;
			wake_up_interruptible(&(vdev->v4l2.frame_wait));
			vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
		}
	}

	return IRQ_HANDLED;
}

int tccvin_cif_set_resolution(tccvin_dev_t * vdev, unsigned int width, unsigned int height, unsigned int pixelformat) {
	vdev->v4l2.pix_format.width			= width;
	vdev->v4l2.pix_format.height		= height;
	vdev->v4l2.pix_format.pixelformat	= pixelformat;

	return 0;
}

int tccvin_set_direct_display_buffer(tccvin_dev_t * vdev) {
    struct v4l2_buffer req;
    int idx = 0;
    strcpy(vdev->cif.pmap_rcam_preview.name, "rearcamera");
    strcpy(vdev->cif.pmap_viqe.name, "rearcamera_viqe");
    strcpy(vdev->cif.pmap_rcam_pgl.name, "parking_gui");


    if (pmap_get_info(vdev->cif.pmap_rcam_preview.name, &(vdev->cif.pmap_rcam_preview)) == 1) {
        dlog("[PMAP] %s: 0x%08x ~ 0x%08x (0x%08x)\n",
            vdev->cif.pmap_rcam_preview.name, \
            vdev->cif.pmap_rcam_preview.base, \
            vdev->cif.pmap_rcam_preview.base + vdev->cif.pmap_rcam_preview.size, \
            vdev->cif.pmap_rcam_preview.size);
    }
    else {
        return -1;
    }

    if (pmap_get_info(vdev->cif.pmap_viqe.name, &(vdev->cif.pmap_viqe)) == 1) {
        dlog("[PMAP] %s: 0x%08x ~ 0x%08x (0x%08x)\n",
            vdev->cif.pmap_viqe.name, \
            vdev->cif.pmap_viqe.base, \
            vdev->cif.pmap_viqe.base + vdev->cif.pmap_viqe.size, \
            vdev->cif.pmap_viqe.size);
    }
    else {
        return -1;
    }

    if (pmap_get_info(vdev->cif.pmap_rcam_pgl.name, &(vdev->cif.pmap_rcam_pgl)) == 1) {
        dlog("[PMAP] %s: 0x%08x ~ 0x%08x (0x%08x)\n",
            vdev->cif.pmap_rcam_pgl.name, \
            vdev->cif.pmap_rcam_pgl.base, \
            vdev->cif.pmap_rcam_pgl.base + vdev->cif.pmap_rcam_pgl.size, \
            vdev->cif.pmap_rcam_pgl.size);
    }
    else {
        return -1;
    }

    for(idx = 0; idx <4; idx++) {
        req.index = idx;
        req.reserved = \
            vdev->cif.pmap_rcam_preview.base + (vdev->v4l2.pix_format.width * vdev->v4l2.pix_format.height * 4 * idx);
        tccvin_v4l2_assign_allocated_buf(vdev, &req);
    }

    return 0;
}

int tccvin_set_buf(tccvin_dev_t * vdev, struct v4l2_requestbuffers * req) {
	struct tccvin_buf	* buf = NULL;
	unsigned int		y_offset = 0, uv_offset = 0;//, stride = 0;
	unsigned int		buff_size = 0;

	// clear the static buffer
	memset(&vdev->v4l2.static_buf[req->count], 0, sizeof(struct tccvin_buf));

	if(req->count == 0) {
#if 0
		stride = ALIGNED_BUFF(vdev->v4l2.pix_format.width, L_STRIDE_ALIGN);
		y_offset = stride * vdev->v4l2.pix_format.height;
#else
		y_offset = ROUND_UP_4(vdev->v4l2.pix_format.width) * ROUND_UP_2(vdev->v4l2.pix_format.height);
#endif

		if(vdev->v4l2.pix_format.pixelformat == V4L2_PIX_FMT_YUYV)
			buff_size = PAGE_ALIGN(y_offset*2);
		else
			buff_size = PAGE_ALIGN(y_offset + y_offset/2);

		vdev->v4l2.capture_buf_list.prev	= vdev->v4l2.capture_buf_list.next	= &(vdev->v4l2.capture_buf_list);
		vdev->v4l2.display_buf_list.prev	= vdev->v4l2.display_buf_list.next	= &(vdev->v4l2.display_buf_list);
	}

	buf = &(vdev->v4l2.static_buf[req->count]);

//	INIT_LIST_HEAD(&buf->buf_list);

	buf->buf.index	= req->count;
	buf->buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf->buf.field	= V4L2_FIELD_NONE;
	buf->buf.memory = V4L2_MEMORY_MMAP;
	buf->buf.length = buff_size;
#ifdef CONFIG_ION
	buf->buf.memory = V4L2_MEMORY_DMABUF;
	buf->buf.m.fd	= req->reserved[1];
#endif//CONFIG_ION

#if 0
	stride = ALIGNED_BUFF(vdev->v4l2.pix_format.width, L_STRIDE_ALIGN);
	y_offset = stride * vdev->v4l2.pix_format.height;

	uv_offset = ALIGNED_BUFF((stride/2), C_STRIDE_ALIGN) * (vdev->v4l2.pix_format.height/2);
#else
	y_offset = ROUND_UP_4(vdev->v4l2.pix_format.width) * ROUND_UP_2(vdev->v4l2.pix_format.height);
	uv_offset = (ROUND_UP_4(vdev->v4l2.pix_format.width) / 2) * (ROUND_UP_2(vdev->v4l2.pix_format.height) / 2);
#endif

	vdev->cif.preview_buf_addr[req->count].y = (unsigned int)req->reserved[0];
	vdev->cif.preview_buf_addr[req->count].u = (unsigned int)vdev->cif.preview_buf_addr[req->count].y + y_offset;
	vdev->cif.preview_buf_addr[req->count].v = (unsigned int)vdev->cif.preview_buf_addr[req->count].u + uv_offset;

	vdev->v4l2.pp_num = (req->count + 1);

	dlog("buf[%2d] - Y: 0x%x, U: 0x%x, V: 0x%x\n", req->count, vdev->cif.preview_buf_addr[req->count].y, vdev->cif.preview_buf_addr[req->count].u, vdev->cif.preview_buf_addr[req->count].v);

	return 0;
}

int tccvin_start_stream(tccvin_dev_t * vdev) {
	tccvin_cif_t	* cif	= &vdev->cif;

	FUNCTION_IN

	mutex_lock(&(vdev->v4l2.lock));

	// size info
	dlog("tgt: %d * %d\n", vdev->v4l2.pix_format.width, vdev->v4l2.pix_format.height);

	vdev->v4l2.oper_mode  = OPER_PREVIEW;

	// clear framebuffer
	tccvin_clear_buffer(vdev);

	// set cif-port
	tccvin_set_cif_port(vdev);

	// reset vioc path
	tccvin_reset_vioc_path(vdev);

	// set rdma for Parking Guide Line
#ifdef CONFIG_OVERLAY_PGL
	if(vdev->v4l2.preview_method == PREVIEW_DD) {
        tccvin_direct_display_set_pgl(vdev);
    }
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
		tccvin_set_scaler(cif, &vdev->v4l2.pix_format);
	}

	// set wmixer
	if(cif->vioc_path.vin <= VIOC_VIN30) {
		tccvin_set_wmixer(vdev);
	}

	// set wdma
	tccvin_set_wdma(vdev);
	mdelay(100);

	if(vdev->v4l2.preview_method == PREVIEW_DD) {
		// set rdma
		tccvin_set_rdma(vdev);

		// set fifo
		tccvin_set_fifo(vdev, ENABLE);

		// set wmixer_out
#ifndef CONFIG_TCC803X_CA7S
		tccvin_set_wmixer_out(cif, RCAM_OVP);
#endif//CONFIG_TCC803X_CA7S
	}

	mutex_unlock(&(vdev->v4l2.lock));

	FUNCTION_OUT
	return 0;
}

int tccvin_stop_stream(tccvin_dev_t * vdev) {
	volatile void __iomem	* pRDMA		= VIOC_RDMA_GetAddress(vdev->cif.vioc_path.rdma);
	volatile void __iomem	* pWDMA		= VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
	VIOC_PlugInOutCheck		vioc_plug_state;
	tccvin_cif_t	* cif	= &vdev->cif;

	int idxLoop;
	unsigned int status;

	FUNCTION_IN

	mutex_lock(&(vdev->v4l2.lock));

	if(vdev->v4l2.preview_method == PREVIEW_DD) {
		// set wmixer_out
		tccvin_set_wmixer_out(cif, NORMAL_OVP);

		// set fifo
		tccvin_set_fifo(vdev, DISABLE);

		// set rdma
//		tccvin_set_rdma(vdev);
		VIOC_RDMA_SetImageDisable(pRDMA);
	}

	VIOC_WDMA_SetIreqMask(pWDMA, VIOC_WDMA_IREQ_ALL_MASK, 0x1);	// disable WDMA interrupt
	VIOC_WDMA_SetImageDisable(pWDMA);

	for(idxLoop=0; idxLoop<20; idxLoop++) {
		VIOC_WDMA_GetStatus(pWDMA, &status);
		if(status & VIOC_WDMA_IREQ_EOFR_MASK)
			break;
		else
			msleep(1);
	}

	if(cif->vioc_path.vin <= VIOC_VIN30) {
		VIOC_CONFIG_Device_PlugState(vdev->cif.vioc_path.scaler, &vioc_plug_state);
		if(vioc_plug_state.enable && vioc_plug_state.connect_statue == VIOC_PATH_CONNECTED) {
			VIOC_CONFIG_PlugOut(vdev->cif.vioc_path.scaler);
		}

		if(!!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED)) {
			VIOC_CONFIG_Device_PlugState(vdev->cif.vioc_path.deintl, &vioc_plug_state);
			if(vioc_plug_state.enable && vioc_plug_state.connect_statue == VIOC_PATH_CONNECTED) {
				VIOC_CONFIG_PlugOut(vdev->cif.vioc_path.deintl);
			}
		}
	}

	VIOC_VIN_SetEnable(VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin), OFF); // disable VIN

	// reset vioc path
	tccvin_reset_vioc_path(vdev);

	// clear framebuffer
	tccvin_clear_buffer(vdev);

	mutex_unlock(&(vdev->v4l2.lock));

	FUNCTION_OUT
	return 0;
}

int tccvin_request_irq(tccvin_dev_t * vdev) {
	struct device_node	* main_node	= vdev->plt_dev->dev.of_node;
	struct device_node	* irq_node	= NULL;
	int					ret			= 0;

	FUNCTION_IN

	irq_node = of_parse_phandle(main_node, "wdma", 0);
	if(irq_node) {
		vdev->cif.vioc_irq_num		= irq_of_parse_and_map(irq_node, get_vioc_index(vdev->cif.vioc_path.wdma));
		dlog("vdev->cif.vioc_irq_num: %d\n", vdev->cif.vioc_irq_num);

#ifdef CONFIG_ARCH_TCC803X
		if(vdev->cif.vioc_path.wdma < VIOC_WDMA09)
			vdev->cif.vioc_intr.id		= VIOC_INTR_WD0 + get_vioc_index(vdev->cif.vioc_path.wdma);
		else
			vdev->cif.vioc_intr.id		= VIOC_INTR_WD9 + (get_vioc_index(vdev->cif.vioc_path.wdma) - get_vioc_index(VIOC_WDMA09));
#else
		vdev->cif.vioc_intr.id		= VIOC_INTR_WD0 + get_vioc_index(vdev->cif.vioc_path.wdma);
#endif
		vdev->cif.vioc_intr.bits	= VIOC_WDMA_IREQ_EOFF_MASK;

		if(vdev->cif.vioc_irq_reg == DISABLE) {
			vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			ret = request_irq(vdev->cif.vioc_irq_num, tccvin_wdma_isr, IRQF_SHARED, vdev->vid_dev->name, vdev);
			vioc_intr_enable(vdev->cif.vioc_irq_num, vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			vdev->cif.vioc_irq_reg = ENABLE;
		} else {
			log("ERROR: The irq(%d) is already registered.\n", vdev->cif.vioc_irq_num);
			return -1;
		}
	} else {
		log("ERROR: The irq node is NOT found.\n");
		return -1;
	}

	FUNCTION_OUT
	return ret;
}

int tccvin_free_irq(tccvin_dev_t * vdev) {
	struct device_node * main_node	= vdev->plt_dev->dev.of_node;
	struct device_node * irq_node	= NULL;
	int					ret			= 0;

	FUNCTION_IN

	irq_node = of_parse_phandle(main_node, "wdma", 0);
	if(irq_node) {
		vdev->cif.vioc_irq_num		= irq_of_parse_and_map(irq_node, get_vioc_index(vdev->cif.vioc_path.wdma));
		dlog("vdev->cif.vioc_irq_num: %d\n", vdev->cif.vioc_irq_num);

		if(vdev->cif.vioc_irq_reg == ENABLE) {
			free_irq(vdev->cif.vioc_irq_num, vdev);
			vioc_intr_disable(vdev->cif.vioc_irq_num, vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			vioc_intr_clear(vdev->cif.vioc_intr.id, vdev->cif.vioc_intr.bits);
			vdev->cif.vioc_irq_reg = DISABLE;
		} else {
			log("ERROR: The irq(%d) is NOT registered.\n", vdev->cif.vioc_irq_num);
			return -1;
		}
	} else {
		log("ERROR: The irq node is NOT found.\n");
		return -1;
	}

	FUNCTION_OUT
	return ret;
}


/**************************************************
 *	PUBLIC FUNCTION LIST
 **************************************************/
int tccvin_get_clock(tccvin_dev_t * vdev) {
	struct device_node	* main_node	= vdev->plt_dev->dev.of_node;
	int					ret			= 0;

	FUNCTION_IN

	vdev->cif.vioc_clk = of_clk_get(main_node, 0);
	ret = -IS_ERR(vdev->cif.vioc_clk);
	if(ret != 0) {
		log("ERROR: Find the \"clock\" node\n");
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
		if(ret)
			log("ERROR: clk_prepare_enable returns %d\n", ret);
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
		log("ERROR: Parse the device tree.\n");
		return -ENODEV;
	}

	// get the vioc's clock
	ret = tccvin_get_clock(vdev);
	if(ret != 0) {
		log("ERROR: tccvin_get_clock returns %d\n", ret);
		return ret;
	}

	// enable the vioc's clock
	ret = tccvin_enable_clock(vdev);
	if(ret != 0) {
		log("ERROR: tccvin_enable_clock returns %d\n", ret);
		return ret;
	}

	// get a video source
#ifdef CONFIG_VIDEO_VIDEOSOURCE
	vdev->cif.videosource_info				= &videosource_info;
#endif//CONFIG_VIDEO_VIDEOSOURCE
	if(vdev->cif.videosource_info == NULL) {
		log("ERROR: videosource_info is NULL.\n");
		return -1;
	}

	// get a pmap for capture
	if(!!(vdev->cif.videosource_info->interlaced & V4L2_DV_INTERLACED)) {
		pmap_get_info("rearcamera_viqe", &vdev->cif.pmap_viqe);
	}

	vdev->cif.is_handover_needed			= 0;

	vdev->v4l2.pp_num						= MAX_BUFFERRS;
	vdev->v4l2.oper_mode 					= OPER_PREVIEW;

	// pixel format
	vdev->v4l2.pix_format.width				= 1920;
	vdev->v4l2.pix_format.height			= 720;
	vdev->v4l2.pix_format.field				= V4L2_FIELD_ANY;
	vdev->v4l2.pix_format.sizeimage			= vdev->v4l2.pix_format.width * vdev->v4l2.pix_format.height * 4;
	vdev->v4l2.pix_format.pixelformat		= V4L2_PIX_FMT_RGB32;	//V4L2_PIX_FMT_YUYV;	// YUV422 is default

	// preview method
	vdev->v4l2.preview_method				= PREVIEW_V4L2;	//PREVIEW_DD;	// v4l2 buffering is default

	// init v4l2 resources
	mutex_init(&vdev->v4l2.lock);

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_deinit(tccvin_dev_t * vdev) {
	FUNCTION_IN

	// deinit v4l2 resources
	mutex_destroy(&(vdev->v4l2.lock));

	// disable the vioc's clock
	tccvin_disable_clock(vdev);

	// put the vioc's clock
	tccvin_put_clock(vdev);

	FUNCTION_OUT
	return 0;
}

void tccvin_v4l2_querycap(tccvin_dev_t * vdev, struct v4l2_capability * cap) {
	FUNCTION_IN

	memset(cap, 0, sizeof(struct v4l2_capability));
	strlcpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
	strlcpy(cap->card, vdev->vid_dev->name, sizeof(cap->card));
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

void tccvin_v4l2_g_fmt(tccvin_dev_t * vdev, struct v4l2_format * format) {
	FUNCTION_IN

	// return a struct data of v4l2_format depending on V4L2_BUF_TYPE_XXX
	switch(format->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		format->fmt.pix		= vdev->v4l2.pix_format;
		break;

	default:
		log("ERROR: The v4l2 type(0x%x) is WRONG.\n", format->type);
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
		vdev->v4l2.pix_format	= format->fmt.pix;
		break;

	default:
		log("ERROR: The v4l2 type(0x%x) is WRONG.\n", format->type);
		WARN_ON(1);
	}

	ret = tccvin_cif_set_resolution(vdev, format->fmt.pix.width, format->fmt.pix.height, format->fmt.pix.pixelformat);

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_reqbufs(tccvin_dev_t * vdev, struct v4l2_requestbuffers * req) {
	int		ret = 0;

	FUNCTION_IN

	if(req->memory != V4L2_MEMORY_MMAP && req->memory != V4L2_MEMORY_USERPTR \
		&& req->memory != V4L2_MEMORY_OVERLAY && req->memory != V4L2_MEMORY_DMABUF) {
		printk("reqbufs: memory type invalid\n");
		return -EINVAL;
	}

	if(req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		printk("reqbufs: video type invalid\n");
		return -EINVAL;
	}

	// allocate frame buffer memory

	// set the buffer address
	ret = tccvin_set_buf(vdev, req);

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_assign_allocated_buf(tccvin_dev_t * vdev, struct v4l2_buffer * req) {
	struct tccvin_buf	* buf = NULL;
	unsigned int		y_offset = 0, uv_offset = 0;//, stride = 0;

	FUNCTION_IN

	buf			= &vdev->v4l2.static_buf[req->index];
	buf->buf	= * req;

#if 0
	stride = ALIGNED_BUFF(vdev->v4l2.pix_format.width, L_STRIDE_ALIGN);
	y_offset = stride * vdev->v4l2.pix_format.height;
	uv_offset = ALIGNED_BUFF((stride/2), C_STRIDE_ALIGN) * (vdev->v4l2.pix_format.height/2);
#else
	y_offset = ROUND_UP_4(vdev->v4l2.pix_format.width) * ROUND_UP_2(vdev->v4l2.pix_format.height);
	uv_offset = (ROUND_UP_4(vdev->v4l2.pix_format.width) / 2) * (ROUND_UP_2(vdev->v4l2.pix_format.height) / 2);
#endif

	dlog("%s - allocated addr: 0x%08x\n", __func__, (unsigned long)req->reserved);

	vdev->cif.preview_buf_addr[req->index].y = (unsigned long)req->reserved;
	vdev->cif.preview_buf_addr[req->index].u = (unsigned long)vdev->cif.preview_buf_addr[req->index].y + y_offset;
	vdev->cif.preview_buf_addr[req->index].v = (unsigned long)vdev->cif.preview_buf_addr[req->index].u + uv_offset;

	vdev->v4l2.pp_num = MAX_BUFFERRS;//(req->index + 1);

	dlog("buf[%2d] - Y: 0x%x, U: 0x%x, V: 0x%x\n", req->index, vdev->cif.preview_buf_addr[req->index].y, vdev->cif.preview_buf_addr[req->index].u, vdev->cif.preview_buf_addr[req->index].v);

	FUNCTION_OUT
	return 0;
}

int tccvin_v4l2_querybuf(tccvin_dev_t * vdev, struct v4l2_buffer * buf) {
	int							index		= buf->index;

	FUNCTION_IN

	// Check the buffer index is valid.
	if(!((0 <= buf->index) && (buf->index < vdev->v4l2.pp_num))) {
		printk(KERN_WARNING "querybuf error : index : %d / %d", index, vdev->v4l2.pp_num);
		return -EINVAL;
	} else {
		* buf = vdev->v4l2.static_buf[index].buf;
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_v4l2_qbuf(tccvin_dev_t * vdev, struct v4l2_buffer * buf) {
	struct tccvin_buf	* cif_buf	= &vdev->v4l2.static_buf[buf->index];

	FUNCTION_IN

	// Check the buffer index is valid.
	if(!((0 <= buf->index) && (buf->index < vdev->v4l2.pp_num))) {
		log("ERROR: The buffer index(%d) is WRONG.\n", buf->index);
		return -EAGAIN;
	}

	// Check the buffer type is valid.
	if(buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		log("ERROR: The buffer type(0x%08x) is WRONG.\n", buf->type);
		return -EAGAIN;
	}

	// Queue the buffer to the capture buffer list
	list_add_tail(&cif_buf->buf_list, &(vdev->v4l2.capture_buf_list));
	dlog("cap count: %d\n", list_get_entry_count(&vdev->v4l2.capture_buf_list));

	// Set the flag V4L2_BUF_FLAG_QUEUED
	cif_buf->buf.flags |= V4L2_BUF_FLAG_QUEUED;

	// clear the flag V4L2_BUF_FLAG_DONE
	cif_buf->buf.flags &= ~V4L2_BUF_FLAG_DONE;

	FUNCTION_OUT
	return 0;
}

int tccvin_v4l2_dqbuf(struct file * file, struct v4l2_buffer * buf) {
	tccvin_dev_t		* vdev		= video_drvdata(file);
	struct tccvin_buf	* cif_buf	= NULL;
	int					display_buf_entry_count	= 0;
	int					ret			= 0;

	FUNCTION_IN

	display_buf_entry_count = list_get_entry_count(&vdev->v4l2.display_buf_list);
	dlog("disp count: %d\n", display_buf_entry_count);

	if(1 < display_buf_entry_count) {
		// get a buffer from the display buffer list to display it
		cif_buf = list_entry(vdev->v4l2.display_buf_list.next, struct tccvin_buf, buf_list);

		// delete the buffer from the display buffer list, not free the budder memory
		list_del(vdev->v4l2.display_buf_list.next);

		// Clear the flag V4L2_BUF_FLAG_QUEUED
		cif_buf->buf.flags &= ~V4L2_BUF_FLAG_QUEUED;

		// clear the flag V4L2_BUF_FLAG_DONE
		cif_buf->buf.flags &= ~V4L2_BUF_FLAG_DONE;

		memcpy(buf, &(cif_buf->buf), sizeof(struct v4l2_buffer));
	} else {
		dlog("The display buffer list is EMPTY!!\n");
		vdev->v4l2.wakeup_int = 0;
		if(wait_event_interruptible_timeout(vdev->v4l2.frame_wait, vdev->v4l2.wakeup_int == 1, msecs_to_jiffies(500)) <= 0) {
			dlog("wait_event_interruptible_timeout 500ms!!\n");
			return -EFAULT;
		}
		ret = -ENOMEM;
	}

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_streamon(tccvin_dev_t * vdev, int * preview_method) {
	int		ret = 0;
    int     skip = 0;
	FUNCTION_IN

	dlog("* preview_method: 0x%08x\n", * preview_method);

	// update the preview method
	vdev->v4l2.preview_method	= * preview_method;

	if(vdev->v4l2.preview_method == PREVIEW_V4L2) {
		dlog("PREVIEW_V4L2\n");
		// init the v4l2 buffer list
		tccvin_v4l2_init_buffer_list(vdev);
	} else if((vdev->v4l2.preview_method & PREVIEW_METHOD_MASK) == PREVIEW_DD) {
		dlog("PREVIEW_DD\n");
        tccvin_set_direct_display_buffer(vdev);

        // If it is handover, Skip start stream function
        if((vdev->v4l2.preview_method & PREVIEW_DD_HANDOVER_MASK) == PREVIEW_DD_HANDOVER)
        skip = 1;
	}

	if(vdev->cif.is_handover_needed) {
		vdev->cif.is_handover_needed = 0;
		log("#### Handover - Skip to set the vioc path\n");
		return 0;
	}

	ret = tccvin_start_stream(vdev);
	if(ret < 0) {
		log("ERROR: Start Stream\n");
		return -1;
	}

	if(vdev->v4l2.preview_method == PREVIEW_V4L2) {
		mutex_init(&vdev->v4l2.lock);
		init_waitqueue_head(&vdev->v4l2.frame_wait);
		INIT_WORK(&vdev->v4l2.wdma_work, wdma_work_thread);

		ret = tccvin_request_irq(vdev);
		if(ret < 0) {
			log("ERROR: Request IRQ\n");
			return ret;
		}
	}
	else if(vdev->v4l2.preview_method == PREVIEW_DD) {
	    ret = tccvin_direct_display_start_monitor(vdev);
        if(ret < 0) {
            log("ERROR: FAILED direct_display_start_monitor\n");
            return ret;
        }
	}

	FUNCTION_OUT
	return ret;
}

int tccvin_v4l2_streamoff(tccvin_dev_t * vdev, int * preview_method) {
	int		ret = 0;

	FUNCTION_IN

	if(vdev->v4l2.preview_method == PREVIEW_V4L2) {
		ret = tccvin_free_irq(vdev);
		if(ret < 0) {
			log("ERROR: Free IRQ\n");
			return ret;
		}
	}
	else if(vdev->v4l2.preview_method == PREVIEW_DD) {
	    ret = tccvin_direct_display_stop_monitor(vdev);
        if(ret < 0) {
            log("ERROR: FAILED direct_display_stop_monitor\n");
			return ret;
		}
	}

	ret = tccvin_stop_stream(vdev);
	if(ret < 0) {
		log("ERROR: Stop Stream\n");
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
	* status = vdev->v4l2.wakeup_int;
}

void test_registers(tccvin_dev_t * vdev) {
	struct reg_test {
		unsigned int * reg;
		unsigned int cnt;
	};

	volatile void __iomem	* pVINBase      = VIOC_VIN_GetAddress(vdev->cif.vioc_path.vin);
//	VIOC_RDMA * pRDMA_PGL = (VIOC_RDMA *)vdev->vioc.pgl.address;
	volatile void __iomem	* pVIQE         = VIOC_VIQE_GetAddress(vdev->cif.vioc_path.deintl);
	volatile void __iomem	* pSC           = VIOC_SC_GetAddress(vdev->cif.vioc_path.scaler);
    volatile void __iomem   * pWMIXer       = VIOC_WMIX_GetAddress(vdev->cif.vioc_path.wmixer);
    volatile void __iomem   * pWDMA         = VIOC_WDMA_GetAddress(vdev->cif.vioc_path.wdma);
    volatile void __iomem   * pRDMA_disp    = VIOC_RDMA_GetAddress(vdev->cif.vioc_path.rdma);

	int i = 0;

	struct reg_test regList[] = {
		{ (unsigned int *)pVINBase,  16 },
//		{ (unsigned int *)pRDMA_PGL, 12 },
//		{ (unsigned int *)pDeintls, 4 },
		{ (unsigned int *)pVIQE, 4 },
		{ (unsigned int *)pSC, 8 },
		{ (unsigned int *)pWMIXer, 28 },
		{ (unsigned int *)pWDMA, 18 },
		{ (unsigned int *)pRDMA_disp, 12 },
#if 0
		{ (unsigned int *)VIOC_CONFIG_GetPathStruct(VIOC_SC0 + vdev->vioc.scaler.index),  1 },
		{ (unsigned int *)VIOC_CONFIG_GetPathStruct(VIOC_DEINTLS), 1 },
		{ (unsigned int *)VIOC_CONFIG_GetPathStruct(VIOC_VIQE), 1 },
#endif
	};
	unsigned int * addr;
	unsigned int idxLoop, nReg, idxReg;

	for(i=0; i<3; i++) {
		printk("\n\n");

		for(idxLoop=0; idxLoop<sizeof(regList)/sizeof(regList[0]); idxLoop++) {
			addr = regList[idxLoop].reg;
			nReg = regList[idxLoop].cnt;

			for(idxReg=0; idxReg<nReg; idxReg++) {
				if((idxReg%4) == 0)
				printk("\n%08x: ", (unsigned int)(addr + idxReg));
				printk("%08x ", *(addr + idxReg));
			}
			printk("\n");
		}
		printk("\n\n");
	}
}

int tccvin_direct_display_check_wdma_counter(tccvin_dev_t * vdev) {
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

int tccvin_direct_display_monitor_thread(void * data) {
	tccvin_dev_t * vdev = (tccvin_dev_t *)data;

	FUNCTION_IN

	while(1) {
		msleep(500);

		if(kthread_should_stop())
			break;

		if(tccvin_direct_display_check_wdma_counter(vdev) == -1) {
			log("ERROR: Recovery mode will be entered\n");

			test_registers(vdev);

			tccvin_stop_stream(vdev);

#ifdef CONFIG_VIDEO_TCCVIN_SWITCHMANAGER
			videosource_close();

			videosource_open();
			videosource_if_change_mode(0);
#endif//CONFIG_VIDEO_TCCVIN_SWITCHMANAGER

			tccvin_start_stream(vdev);
		}
	}
	FUNCTION_OUT
	return 0;
}


int tccvin_direct_display_start_monitor(tccvin_dev_t * vdev) {
	vdev->v4l2.threadRecovery = kthread_run(tccvin_direct_display_monitor_thread, vdev, "threadRecovery");
	if(IS_ERR_OR_NULL(vdev->v4l2.threadRecovery)) {
		log("ERROR: FAILED kthread_run\n");
		vdev->v4l2.threadRecovery = NULL;

		return -1;
	}
	return 0;
}

int tccvin_direct_display_stop_monitor(tccvin_dev_t * vdev) {
    int retVal = 0;

	if(!IS_ERR_OR_NULL(vdev->v4l2.threadRecovery)) {
	    retVal = kthread_stop(vdev->v4l2.threadRecovery);
		if(retVal != 0) {
			log("ERROR: FAILED kthread_stop(%d) \n", retVal);
			return -1;
	    }

		vdev->v4l2.threadRecovery = NULL;
	}
	return 0;
}

int tccvin_direct_display_set_pgl(tccvin_dev_t * vdev) {
	volatile void __iomem	* pRDMA		= VIOC_RDMA_GetAddress(vdev->cif.vioc_path.pgl);

	unsigned int	width		= vdev->v4l2.pix_format.width;
	unsigned int	height		= vdev->v4l2.pix_format.height;
	unsigned int	format		= PGL_FORMAT;
	unsigned int	buf_addr	= vdev->cif.pmap_rcam_pgl.base;

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

