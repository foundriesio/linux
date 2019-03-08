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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>

#include <video/tcc/tcc_cam_ioctrl.h>

#include "tccvin_core.h"
#include "tccvin_dev.h"
#ifdef CONFIG_VIDEO_TCCVIN_SWITCHMANAGER
#include "tccvin_switchmanager.h"
#endif//CONFIG_VIDEO_TCCVIN_SWITCHMANAGER

static int					debug = 0;
#define TAG					"tccvin_core"
#define log(msg, arg...)	do { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } while(0)
#define dlog(msg, arg...)	do { if(debug) { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } } while(0)
#define FUNCTION_IN			dlog("IN\n");
#define FUNCTION_OUT		dlog("OUT\n");

#define MODULE_NAME			"telechips,video-input"

long tccvin_core_do_ioctl(struct file * file, unsigned int cmd, void * arg) {
	tccvin_dev_t	* vdev	= video_drvdata(file);
	int				ret		= 0;

	FUNCTION_IN

	dlog("path index: %d, cmd: 0x%08x\n", vdev->plt_dev->id, cmd);

	switch(cmd) {
	case VIDIOC_QUERYCAP:
		tccvin_v4l2_querycap(vdev, (struct v4l2_capability *)arg);
		break;

	case VIDIOC_ENUM_FMT:
		ret = tccvin_v4l2_enum_fmt((struct v4l2_fmtdesc *)arg);
		break;

	case VIDIOC_G_FMT:
		tccvin_v4l2_g_fmt(vdev, (struct v4l2_format *)arg);
		break;

	case VIDIOC_S_FMT:
		ret = tccvin_v4l2_s_fmt(vdev, (struct v4l2_format *)arg);
		break;

	case VIDIOC_REQBUFS:
		ret = tccvin_v4l2_reqbufs(vdev, (struct v4l2_requestbuffers *)arg);
		break;

	case VIDIOC_ASSIGN_ALLOCATED_BUF:
		ret = tccvin_v4l2_assign_allocated_buf(vdev, (struct v4l2_buffer *)arg);
		break;

	case VIDIOC_QUERYBUF:
		ret = tccvin_v4l2_querybuf(vdev, (struct v4l2_buffer *)arg);
		break;

	case VIDIOC_G_FBUF:
	case VIDIOC_S_FBUF:
	case VIDIOC_OVERLAY:
		ret = -EINVAL;
		break;

	case VIDIOC_QBUF:
		if((vdev->v4l2.preview_method & PREVIEW_METHOD_MASK) == PREVIEW_DD) {
			dlog("VIDIOC_DQBUF is not supported in DirectDisplay Mode.\n");
			ret = -EINVAL;
		} else {
			ret = tccvin_v4l2_qbuf(vdev, (struct v4l2_buffer *)arg);
		}
		break;

	case VIDIOC_EXPBUF:
		ret = -EINVAL;
		break;

	case VIDIOC_DQBUF:
		if((vdev->v4l2.preview_method & PREVIEW_METHOD_MASK) == PREVIEW_DD) {
			dlog("VIDIOC_DQBUF is not supported in DirectDisplay Mode.\n");
			ret = -EINVAL;
		} else {
			ret = tccvin_v4l2_dqbuf(file, (struct v4l2_buffer *)arg);
		}
		break;

	case VIDIOC_STREAMON:
		ret = tccvin_v4l2_streamon(vdev, (int *)arg);
		break;

	case VIDIOC_STREAMOFF:
		ret = tccvin_v4l2_streamoff(vdev, (int *)arg);
		break;

	case VIDIOC_G_PARM:
		ret = tccvin_v4l2_g_param(vdev, (struct v4l2_streamparm *)arg);
		break;

	case VIDIOC_S_PARM:
		ret = tccvin_v4l2_s_param((struct v4l2_streamparm *)arg);
		break;

	case VIDIOC_G_STD:
	case VIDIOC_S_STD:
	case VIDIOC_ENUMSTD:
		ret = -EINVAL;
		break;

	case VIDIOC_ENUMINPUT:
		ret = tccvin_v4l2_enum_input((struct v4l2_input *)arg);
		break;

	case VIDIOC_G_CTRL:
	case VIDIOC_S_CTRL:
	case VIDIOC_G_TUNER:
	case VIDIOC_S_TUNER:
	case VIDIOC_G_AUDIO:
	case VIDIOC_S_AUDIO:
	case VIDIOC_QUERYCTRL:
	case VIDIOC_QUERYMENU:
		ret = -EINVAL;
		break;

	case VIDIOC_G_INPUT:
		ret = tccvin_v4l2_g_input(vdev, (struct v4l2_input *) arg);
		break;

	case VIDIOC_S_INPUT:
		ret = tccvin_v4l2_s_input(vdev, (struct v4l2_input *) arg);
		break;

	case VIDIOC_G_EDID:
	case VIDIOC_S_EDID:
	case VIDIOC_G_OUTPUT:
	case VIDIOC_S_OUTPUT:
	case VIDIOC_ENUMOUTPUT:
	case VIDIOC_G_AUDOUT:
	case VIDIOC_S_AUDOUT:
	case VIDIOC_G_MODULATOR:
	case VIDIOC_S_MODULATOR:
	case VIDIOC_G_FREQUENCY:
	case VIDIOC_S_FREQUENCY:
	case VIDIOC_CROPCAP:
	case VIDIOC_G_CROP:
	case VIDIOC_S_CROP:
	case VIDIOC_G_JPEGCOMP:
	case VIDIOC_S_JPEGCOMP:
	case VIDIOC_QUERYSTD:
		ret = -EINVAL;
		break;

	case VIDIOC_TRY_FMT:
		ret = tccvin_v4l2_try_fmt((struct v4l2_format *)arg);
		break;

	case VIDIOC_ENUMAUDIO:
	case VIDIOC_ENUMAUDOUT:
	case VIDIOC_G_PRIORITY:
	case VIDIOC_S_PRIORITY:
	case VIDIOC_G_SLICED_VBI_CAP:
	case VIDIOC_LOG_STATUS:
	case VIDIOC_G_EXT_CTRLS:
	case VIDIOC_S_EXT_CTRLS:
	case VIDIOC_TRY_EXT_CTRLS:
	case VIDIOC_ENUM_FRAMESIZES:
	case VIDIOC_ENUM_FRAMEINTERVALS:
	case VIDIOC_G_ENC_INDEX:
	case VIDIOC_ENCODER_CMD:
	case VIDIOC_TRY_ENCODER_CMD:
		ret = -EINVAL;
		break;

	case VIDIOC_SET_WMIXER_OVP:
		ret = tccvin_set_wmixer_out(&vdev->cif, *(int *)arg);
		break;

	case VIDIOC_CHECK_PATH_STATUS:
		tccvin_check_path_status(vdev, (int *)arg);
		break;

    case VIDIOC_SET_ALPHA_BLENDING:
        tccvin_set_ovp_value(&vdev->cif);
        break;

	case VIDIOC_USER_JPEG_CAPTURE:
		dlog("VIDIOC_USER_JPEG_CAPTURE\n");
		break;

	case VIDIOC_USER_PROC_AUTOFOCUS:
		dlog("VIDIOC_USER_PROC_AUTOFOCUS\n");
		break;

	case VIDIOC_USER_SET_CAMINFO_TOBEOPEN:
		dlog("VIDIOC_USER_SET_CAMINFO_TOBEOPEN\n");
		break;

	case VIDIOC_USER_GET_MAX_RESOLUTION:
		dlog("VIDIOC_USER_GET_MAX_RESOLUTION\n");
		ret = XGA;
		break;

	case VIDIOC_USER_GET_SENSOR_FRAMERATE:
		dlog("VIDIOC_USER_GET_SENSOR_FRAMERATE\n");
		break;

	case VIDIOC_USER_GET_ZOOM_SUPPORT:
		dlog("VIDIOC_USER_GET_ZOOM_SUPPORT\n");
		break;

	case VIDIOC_USER_SET_CAMERA_ADDR:
		dlog("VIDIOC_USER_SET_CAMERA_ADDR\n");
		break;

	case VIDIOC_USER_GET_CAM_STATUS:
		dlog("VIDIOC_USER_GET_CAM_STATUS\n");
		break;

	default:
		log("ERROR: VIDIOC command(0x%08x) is WRONG.\n", cmd);
		WARN_ON(1);
	}
	FUNCTION_OUT
	return ret;
}

unsigned int tccvin_core_poll(struct file * file, struct poll_table_struct * wait) {
	tccvin_dev_t	* vdev = video_drvdata(file);

	poll_wait(file, &(vdev->v4l2.frame_wait), wait);

	return 0;
}

extern int range_is_allowed(unsigned long pfn, unsigned long size);
int tccvin_core_mmap(struct file * file, struct vm_area_struct * vma) {
	if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		printk(KERN_ERR  "this address is not allowed \n");
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops 	= NULL;
	vma->vm_flags	|= VM_IO;
	vma->vm_flags	|= VM_DONTEXPAND | VM_DONTDUMP;

	return 0;
}

long tccvin_core_ioctl(struct file * file, unsigned int cmd, unsigned long arg) {
	return video_usercopy(file, cmd, arg, tccvin_core_do_ioctl);
}

int tccvin_core_open(struct file * file) {
	tccvin_dev_t	* vdev	= video_drvdata(file);
	int				minor	= video_devdata(file)->minor;

	FUNCTION_IN

	if((vdev->vid_dev != NULL) && (vdev->vid_dev->minor == minor)) {
		dlog("[%d] is_dev_opened: %d\n", vdev->plt_dev->id, vdev->is_dev_opened);
		if(vdev->is_dev_opened == DISABLE) {
			// init the v4l2 data
			tccvin_v4l2_init(vdev);

			vdev->is_dev_opened  = ENABLE;
		}
		dlog("[%d] is_dev_opened: %d\n", vdev->plt_dev->id, vdev->is_dev_opened);
	} else {
		dlog("[%s] video dev: 0x%p, video minor: %d, minor: %d\n", vdev->vid_dev->name, vdev->vid_dev, vdev->vid_dev->minor, minor);
		return -ENODEV;
	}

	FUNCTION_OUT
	return	0;
}

int tccvin_core_release(struct file * file) {
	tccvin_dev_t	* vdev	= video_drvdata(file);
	int				minor	= video_devdata(file)->minor;

	FUNCTION_IN

	if((vdev->vid_dev != NULL) && (vdev->vid_dev->minor == minor)) {
		dlog("[%d] is_dev_opened: %d\n", vdev->plt_dev->id, vdev->is_dev_opened);
		if(vdev->is_dev_opened == ENABLE) {
			// deinit the v4l2 data
			tccvin_v4l2_deinit(vdev);

			vdev->is_dev_opened  = DISABLE;
		}
		dlog("[%d] is_dev_opened: %d\n", vdev->plt_dev->id, vdev->is_dev_opened);
	} else {
		dlog("[%s] video dev: 0x%p, video minor: %d, minor: %d\n", vdev->vid_dev->name, vdev->vid_dev, vdev->vid_dev->minor, minor);
		return -ENODEV;
	}

	FUNCTION_OUT
	return 0;
}

struct v4l2_file_operations tccvin_core_fops = {
	.owner			= THIS_MODULE,
	.poll			= tccvin_core_poll,
	.unlocked_ioctl = tccvin_core_ioctl,
	.mmap			= tccvin_core_mmap,
	.open			= tccvin_core_open,
	.release		= tccvin_core_release
};

int tccvin_core_probe(struct platform_device * pdev) {
	tccvin_dev_t	* vdev	= NULL;
	int				ret		= 0;

	FUNCTION_IN

	// Get the index from its alias
	pdev->id = of_alias_get_id(pdev->dev.of_node, "videoinput");
	log("Platform Device index: %d, name: %s\n", pdev->id, pdev->name);

	// allocate and clear memory for a camera device
	vdev = kzalloc(sizeof(tccvin_dev_t), GFP_KERNEL);
	if(vdev == NULL) {
		log("ERROR: Allocate a tccvin device struct.\n");
		return -ENOMEM;
	}
	memset(vdev, 0x00, sizeof(tccvin_dev_t));

	if(pdev->dev.of_node) {
		vdev->plt_dev = pdev;

		memset(&vdev->cif, 0x00, sizeof(tccvin_cif_t));
		memset(&vdev->v4l2, 0x00, sizeof(tccvin_v4l2_t));
	} else {
		log("ERROR: Find a tccvin device tree.\n");
		return -ENODEV;
	}

	// allocate and clear memory for a video device
	vdev->vid_dev = video_device_alloc();
	if(vdev->vid_dev == NULL) {
		log("ERROR: Allocate a video device struct.\n");
		return -ENOMEM;
	}

	// set the device ops
	vdev->vid_dev->fops = &tccvin_core_fops;

	// allocate and clear memory for a v4l2 device
	vdev->vid_dev->v4l2_dev = kzalloc(sizeof(struct v4l2_device), GFP_KERNEL);
	if(vdev->vid_dev->v4l2_dev == NULL) {
		log("ERROR: Allocate a v4l2 device struct.\n");
		return -ENOMEM;
	}

	// set the name of the video device as the platform device's one
	strlcpy(vdev->vid_dev->name, pdev->name, sizeof(vdev->vid_dev->name));

	// register a v4l2 device
	ret = v4l2_device_register(&pdev->dev, vdev->vid_dev->v4l2_dev);
	if(!ret) {
		dlog("Registered as a v4l2 device(%s).\n", pdev->name);
	} else {
		log("ERROR: Register a v4l2 device(%s) struct.\n", pdev->name);
		return -ENODEV;
	}

	// set the video device's minor as -1 (when failed)
	vdev->vid_dev->minor = -1;

	// set the release function
	// it must be set for the failure to register a video device
	vdev->vid_dev->release = video_device_release;

	// register a video device
	ret = video_register_device(vdev->vid_dev, VFL_TYPE_GRABBER, vdev->vid_dev->minor);
	if(!ret) {
		dlog("Registered as a video device(%s).\n", vdev->vid_dev->name);
	} else {
		log("ERROR: Register a video device(%d) struct.\n", vdev->vid_dev->minor);
		video_device_release(vdev->vid_dev);
		return -ENODEV;
	}

	// set the video driver's data
	video_set_drvdata(vdev->vid_dev, vdev);

#ifdef CONFIG_VIDEO_TCCVIN_SWITCHMANAGER
	// switchmanager
	tccvin_switchmanager_probe(vdev);
#endif//CONFIG_VIDEO_TCCVIN_SWITCHMANAGER

	FUNCTION_OUT
	return 0;
}

int tccvin_core_remove(struct platform_device * pdev) {
	tccvin_dev_t	* vdev	= platform_get_drvdata(pdev);

	FUNCTION_IN

	// unregister the video device
	video_unregister_device(vdev->vid_dev);

	// unregister the v4l2 device
	v4l2_device_unregister(vdev->vid_dev->v4l2_dev);
	if(vdev->vid_dev->v4l2_dev != NULL) {
		// free the memory for the v4l2 device
		kfree(vdev->vid_dev->v4l2_dev);
	}

	// release(free) the video device
	video_device_release(vdev->vid_dev);

	// free the memory for the camera device
	kfree(vdev);

	FUNCTION_OUT
	return 0;
}

int tccvin_core_suspend(struct platform_device * pdev, pm_message_t state) {
	FUNCTION_IN

	// DO NOT anything because the context was saved already, so it won't be affected whatever do you.
	// If you print any data in context, you can see strange data. I don't know why.
	log("");

	FUNCTION_OUT
	return 0;
}

int tccvin_core_resume(struct platform_device * pdev) {
	FUNCTION_IN

	log("");

	FUNCTION_OUT
	return 0;
}

static struct of_device_id tccvin_of_match[] = {
	{ .compatible = MODULE_NAME },
};
MODULE_DEVICE_TABLE(of, tccvin_of_match);

static struct platform_driver tccvin_core_driver = {
	.probe		= tccvin_core_probe,
	.remove		= tccvin_core_remove,
	.suspend	= tccvin_core_suspend,
	.resume		= tccvin_core_resume,
	.driver		= {
		.name			= MODULE_NAME,
		.owner			= THIS_MODULE,
		.of_match_table	= of_match_ptr(tccvin_of_match)
	},
};
module_platform_driver(tccvin_core_driver);

MODULE_DESCRIPTION("Telechips Video-Input Driver");
MODULE_VERSION("v1.0");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips.Co.Ltd");

