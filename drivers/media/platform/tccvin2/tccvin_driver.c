/*******************************************************************************
 *      tccvin_driver.c  --  Driver management
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

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>

#include "tccvin_video.h"

struct framesize framesize_list[] = {
	{	 320,	 240	},
	{	 640,	 480	},
	{	1280,	 720	},
	{	1920,	1080	},
};

unsigned int tccvin_no_drop_param;
unsigned int tccvin_timeout_param = TCCVIN_CTRL_STREAMING_TIMEOUT;

/* Simplify a fraction using a simple continued fraction decomposition. The
 * idea here is to convert fractions such as 333333/10000000 to 1/30 using
 * 32 bit arithmetic only. The algorithm is not perfect and relies upon two
 * arbitrary parameters to remove non-significative terms from the simple
 * continued fraction decomposition. Using 8 and 333 for n_terms and threshold
 * respectively seems to give nice results.
 */
void tccvin_simplify_fraction(uint32_t *numerator, uint32_t *denominator,
		unsigned int n_terms, unsigned int threshold)
{
	uint32_t *an;
	uint32_t x, y, r;
	unsigned int i, n;

	an = kmalloc(n_terms * sizeof *an, GFP_KERNEL);
	if (an == NULL)
		return;

	/* Convert the fraction to a simple continued fraction. See
	 * http://mathforum.org/dr.math/faq/faq.fractions.html
	 * Stop if the current term is bigger than or equal to the given
	 * threshold.
	 */
	x = *numerator;
	y = *denominator;

	for (n = 0; n < n_terms && y != 0; ++n) {
		an[n] = x / y;
		if (an[n] >= threshold) {
			if (n < 2)
				n++;
			break;
		}

		r = x - an[n] * y;
		x = y;
		y = r;
	}

	/* Expand the simple continued fraction back to an integer fraction. */
	x = 0;
	y = 1;

	for (i = n; i > 0; --i) {
		r = y;
		y = an[i-1] * y + x;
		x = r;
	}

	*numerator = y;
	*denominator = x;
	kfree(an);
}

/* ------------------------------------------------------------------------
 * Descriptors parsing
 */

static int tccvin_parse_format(struct tccvin_device *dev,
	struct tccvin_streaming *streaming, struct tccvin_format *format,
	__u32 **intervals)
{
	struct tccvin_format_desc *fmtdesc;
	struct tccvin_format *streamimg_format;
	struct tccvin_frame *frame;
	int idxFormat, idxFrame;
	int ret = 0;

	for(idxFormat=0; idxFormat<streaming->nformats; idxFormat++) {
		fmtdesc = &tccvin_fmts[idxFormat];

		streamimg_format = &streaming->format[idxFormat];
		streamimg_format->index = idxFormat;
		strlcpy(streamimg_format->name, fmtdesc->name, sizeof streamimg_format->name);
		streamimg_format->fcc = fmtdesc->fcc;
		streamimg_format->bpp = fmtdesc->bpp;
		streamimg_format->colorspace = 0;
		streamimg_format->flags = 0;
		streamimg_format->nframes = format->nframes;
		streamimg_format->frame = format->frame;

		logd("index: %d, bpp: %d, colorspace: 0x%08x, fcc: 0x%08x, flags: 0x%08x, name: %s, nframes: %d\n", \
			 streamimg_format->index, streamimg_format->bpp, streamimg_format->colorspace, \
			streamimg_format->fcc, streamimg_format->flags, streamimg_format->name, streamimg_format->nframes);

		for(idxFrame=0; idxFrame<format->nframes; idxFrame++) {
			frame = &format[idxFormat].frame[idxFrame];
			frame->bFrameIndex					= idxFrame;
			frame->bmCapabilities				= 0x00000000;
			frame->wWidth						= framesize_list[idxFrame].width;
			frame->wHeight						= framesize_list[idxFrame].height;
			frame->dwMaxVideoFrameBufferSize	= frame->wWidth * frame->wHeight *  format->bpp / 8;
			frame->dwDefaultFrameInterval		= 333333;
			frame->bFrameIntervalType			= 2;
			frame->dwFrameInterval				= *intervals;
			frame->dwFrameInterval[0]			= 333333;
			frame->dwFrameInterval[1]			= 666667;
			frame->dwDefaultFrameInterval =
				min(frame->dwFrameInterval[1],
				    max(frame->dwFrameInterval[0],
					frame->dwDefaultFrameInterval));
			logd("bFrameIndex: %d, bmCapabilities: 0x%08x, wWidth: %d, wHeight: %d, dwMaxVideoFrameBufferSize: %d, dwDefaultFrameInterval: %d, bFrameIntervalType: %d, dwFrameInterval: %d", \
				frame->bFrameIndex, frame->bmCapabilities, frame->wWidth, frame->wHeight, frame->dwMaxVideoFrameBufferSize, frame->dwDefaultFrameInterval, frame->bFrameIntervalType, *frame->dwFrameInterval);
		}
	}

	return ret;
}

static int tccvin_parse_streaming(struct tccvin_device *dev)
{
	struct tccvin_streaming *streaming = NULL;
	struct tccvin_format *format;
	struct tccvin_frame *frame;
	unsigned int nformats = 0, nframes = 0, nintervals = 0;
	unsigned int size;
	__u32 *interval;
	int ret = -EINVAL;

	streaming = kzalloc(sizeof *streaming, GFP_KERNEL);
	if (streaming == NULL) {
		return -EINVAL;
	}

	mutex_init(&streaming->mutex);
	streaming->dev = dev;

	/* Parse the header descriptor. */
	streaming->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* Count the format and frame descriptors. */
	while(tccvin_fmts[nformats+1].name != NULL) {
		nformats++;
		//logd("nformats count: %d", nformats);
	}
	nframes = sizeof(framesize_list) / sizeof(framesize_list[0]);
	nintervals = 2;

	if (nformats == 0) {
		loge("device videostreaming interface "
			"has no supported formats defined.\n");
		goto error;
	}

	logd("nformats: %d, nframes: %d, nintervals: %d", nformats, nframes, nintervals);
	size = nformats * sizeof *format + nframes * sizeof *frame
	     + nintervals * sizeof *interval;
	format = kzalloc(size, GFP_KERNEL);
	if (format == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	frame = (struct tccvin_frame *)&format[nformats];
	interval = (__u32 *)&frame[nframes];

	streaming->format = format;
	streaming->nformats = nformats;

	/* Parse the format descriptors. */
	format->frame = frame;
	format->nframes = nframes;
	ret = tccvin_parse_format(dev, streaming, format,
		&interval);
	if (ret < 0)
		goto error;

	dev->stream = streaming;
	return 0;

error:
	kfree(streaming->format);
	kfree(streaming);
	return ret;
}

static int tccvin_parse_standard_control(struct tccvin_device *dev)
{
	tccvin_parse_streaming(dev);

	return 0;
}

static int tccvin_parse_control(struct tccvin_device *dev)
{
	int ret;

	if ((ret = tccvin_parse_standard_control(dev)) < 0)
		return ret;

	return 0;
}

/* ------------------------------------------------------------------------
 * Video device registration and unregistration
 */

/*
 * Delete the TCCVIN device.
 *
 * Called by the kernel when the last reference to the tccvin_device structure
 * is released.
 *
 * As this function is called after or during disconnect(), all URBs have
 * already been canceled by the TCCVIN core. There is no need to kill the
 * interrupt URB manually.
 */
static void tccvin_delete(struct kref *kref)
{
	struct tccvin_device *dev = container_of(kref, struct tccvin_device, ref);
	struct tccvin_streaming *streaming = dev->stream;
	FUNCTION_IN

 	if (dev->vdev.dev)
 		v4l2_device_unregister(&dev->vdev);

	kfree(streaming->format);
	kfree(streaming);

	kfree(dev);
	FUNCTION_OUT
}

static void tccvin_release(struct video_device *vdev)
{
	struct tccvin_streaming *stream = video_get_drvdata(vdev);
	struct tccvin_device *dev = stream->dev;
	FUNCTION_IN

	kref_put(&dev->ref, tccvin_delete);
	FUNCTION_OUT
}

/*
 * Unregister the video devices.
 */
static void tccvin_unregister_video(struct tccvin_device *dev)
{
	struct tccvin_streaming *stream;
	int ret;
	FUNCTION_IN

	stream = dev->stream;
	if (video_is_registered(&stream->vdev))
		video_unregister_device(&stream->vdev);

	/* Deitialize the streaming interface with default streaming
	 * parameters.
	 */
	ret = tccvin_video_deinit(stream);
	if (ret < 0) {
		loge("Failed to initialize the device "
			"(%d).\n", ret);
	}

	FUNCTION_OUT
}

static int tccvin_register_video(struct tccvin_device *dev,
		struct tccvin_streaming *stream)
{
	struct video_device *vdev = &stream->vdev;
	int ret;
	FUNCTION_IN

	/* Initialize the video buffers queue. */
	ret = tccvin_queue_init(&stream->queue, stream->type, !tccvin_no_drop_param);
	if (ret) {
		loge("tccvin_queue_init, ret: %d\n", ret);
		return ret;
	}

	/* Register the device with V4L. */

	/* We already hold a reference to dev->udev. The video device will be
	 * unregistered before the reference is released, so we don't need to
	 * get another one.
	 */
	vdev->v4l2_dev = &dev->vdev;
	vdev->fops = &tccvin_fops;
	vdev->ioctl_ops = &tccvin_ioctl_ops;
	vdev->minor = dev->pdev->id;
	vdev->release = tccvin_release;
	strlcpy(vdev->name, dev->name, sizeof vdev->name);

	/* Set the driver data before calling video_register_device, otherwise
	 * tccvin_v4l2_open might race us.
	 */
	video_set_drvdata(vdev, stream);

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, vdev->minor);
	if (ret < 0) {
		loge("Failed to register video device (%d).\n",
			   ret);
		return ret;
	}

	/* Initialize the streaming interface with default streaming
	 * parameters.
	 */
	ret = tccvin_video_init(stream);
	if (ret < 0) {
		loge("Failed to initialize the device "
			"(%d).\n", ret);
		return ret;
	}

	kref_get(&dev->ref);
	FUNCTION_OUT
	return 0;
}

int subdevs_list_initialized;
struct tccvin_subdev {
	struct v4l2_subdev	*subdev;
	struct list_head	subdev_list;
};

struct tccvin_subdev tccvin_subdev_list;

int tccvin_async_bound(struct v4l2_async_notifier *notifier, struct v4l2_subdev *subdev, struct v4l2_async_subdev *asd) {
	struct tccvin_device * dev = NULL;
	videosource_t * vs = NULL;
	int	enable = 1;
	struct tccvin_subdev * new_subdev;
	int	ret = 0;

	logd("The callback function is invoked from found node: %s\n", asd->match.device_name.name);

	dev = container_of(notifier, struct tccvin_device, notifier);

	// register subdevice here
	dev->subdevs[dev->num_registered_subdev++] = subdev;

	new_subdev = kmalloc(sizeof(struct tccvin_subdev), GFP_KERNEL);

	new_subdev->subdev = subdev;
	INIT_LIST_HEAD(&new_subdev->subdev_list);
	list_add_tail(&new_subdev->subdev_list, &tccvin_subdev_list.subdev_list);
	
	subdevs_list_initialized = 1;

	// get videosource format
	vs = container_of(subdev, struct videosource, sd);
	dev->stream->cif.videosource_info		= &vs->format;
	if(dev->stream->cif.cif_port == -1)
		dev->stream->cif.cif_port			= vs->format.cif_port;

	// power-up sequence & initial i2c setting
	ret = v4l2_subdev_call(subdev, core, s_power, enable);
	ret = v4l2_subdev_call(subdev, video, s_stream, enable);

	return ret;
}

void tccvin_async_unbind(struct v4l2_async_notifier *notifier,
			struct v4l2_subdev *subdev,
			struct v4l2_async_subdev *asd) {
	logd("%s is invoked !! \n", __func__);
}

int tccvin_async_complete(struct v4l2_async_notifier *notifier) {
	// use subdevice at index 0(isl79988) as default

	return 0;
}

int tccvin_init_subdevices(struct tccvin_device *vdev) {
	struct v4l2_async_notifier *notifier = &vdev->notifier;
	struct v4l2_async_subdev *asd = NULL;
	char *subdev_name = vdev->subdev_name;
	int idxSubDev = 0;
	int	ret = 0;

	vdev->num_registered_subdev = 0;

	if(subdevs_list_initialized == 0) {
		INIT_LIST_HEAD(&tccvin_subdev_list.subdev_list);
	} else {
		struct list_head	* list	= NULL;
		struct tccvin_subdev * p = NULL;
		videosource_t * vs = NULL;

		list_for_each(list, &tccvin_subdev_list.subdev_list) {
			p = list_entry(list, struct tccvin_subdev, subdev_list);
			logd("subdev name: %s\n", dev_name(p->subdev->dev));
			if(!strcmp(subdev_name, dev_name(p->subdev->dev))) {
				vdev->subdevs[vdev->num_registered_subdev++] = p->subdev;
	
				// get videosource format
				vs = container_of(p->subdev, struct videosource, sd);
				vdev->stream->cif.videosource_info		= &vs->format;
				if(vdev->stream->cif.cif_port == -1)
					vdev->stream->cif.cif_port			= vs->format.cif_port;

				return ret;
			}
		}
	}

	/* Set the number of all sub-devices to 1 temporarily. This
	 * should be changed according to the working environment */
	notifier->subdevs = vdev->asd;
	notifier->num_subdevs = 1;	//V4L2_MAX_SUBDEVS

	for (idxSubDev = 0; idxSubDev < notifier->num_subdevs; idxSubDev++) {
		vdev->asd[idxSubDev] = kmalloc(sizeof(struct v4l2_async_subdev), GFP_KERNEL);
	}

	/* after allocation, set asd */
	// check include/media/v4l2-async.h
	for (idxSubDev = 0; idxSubDev < notifier->num_subdevs; idxSubDev++) {
		asd = vdev->asd[idxSubDev];

		/* For match type, it could be device name and i2c like; V4L2_ASYNC_MATCH_I2C */
		asd->match_type = V4L2_ASYNC_MATCH_DEVNAME;
		asd->match.device_name.name = subdev_name;

		logd("v4l2 sub device - slave address: %s\n", asd->match.device_name.name);
	}

	notifier->bound = tccvin_async_bound;
	notifier->complete = tccvin_async_complete;
	notifier->unbind = tccvin_async_unbind;
	ret = v4l2_async_notifier_register(vdev->stream->vdev.v4l2_dev, notifier);
	if (ret < 0) {
		loge("Error registering async notifier for tccvin\n");
		v4l2_device_unregister(vdev->stream->vdev.v4l2_dev);
		ret = -EINVAL;
	} else {
		logd("Succeed to register async notifier for tccvin\n");
	}

	return ret;
}

/* ------------------------------------------------------------------------
 * Driver initialization and cleanup
 */

static int tccvin_core_probe(struct platform_device * pdev) {
	struct tccvin_device *dev;
	FUNCTION_IN

	// Get the index from its alias
	pdev->id = of_alias_get_id(pdev->dev.of_node, "videoinput");
	logd("Platform Device index: %d, name: %s\n", pdev->id, pdev->name);

	/* Allocate memory for the device and initialize it. */
	if ((dev = kzalloc(sizeof *dev, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	dev->current_subdev_idx = 0;

	INIT_LIST_HEAD(&dev->entities);
	kref_init(&dev->ref);
	mutex_init(&dev->lock);

	dev->pdev = pdev;

	strlcpy(dev->name, "video-input path", sizeof dev->name);

	/* Parse the Video Class control descriptor. */
	if (tccvin_parse_control(dev) < 0) {
		loge("Unable to parse TCCVIN "
			"descriptors.\n");
		goto error;
	}

	/* Initialize the media device and register the V4L2 device. */
	if (v4l2_device_register(&pdev->dev, &dev->vdev) < 0)
		goto error;

	tccvin_register_video(dev, dev->stream);

	platform_set_drvdata(pdev, dev);

	tccvin_init_subdevices(dev);

	FUNCTION_OUT
	return 0;

error:
	tccvin_unregister_video(dev);
	kref_put(&dev->ref, tccvin_delete);
	FUNCTION_OUT
	return -ENODEV;
}

static int tccvin_core_remove(struct platform_device * pdev) {
	struct tccvin_device *dev	= platform_get_drvdata(pdev);

	FUNCTION_IN

	tccvin_unregister_video(dev);
	kref_put(&dev->ref, tccvin_delete);

	// free the memory for the camera device
	kfree(dev);

	FUNCTION_OUT
	return 0;
}

static struct of_device_id tccvin_of_match[] = {
	{ .compatible = "telechips,video-input" },
};
MODULE_DEVICE_TABLE(of, tccvin_of_match);

static struct platform_driver tccvin_core_driver = {
	.probe		= tccvin_core_probe,
	.remove		= tccvin_core_remove,
	.driver		= {
		.name			= DRIVER_NAME,
		.owner			= THIS_MODULE,
		.of_match_table	= of_match_ptr(tccvin_of_match),
	},
};

module_platform_driver(tccvin_core_driver);

MODULE_AUTHOR("Telechips.Co.Ltd");
MODULE_DESCRIPTION("Telechips Video-Input Path(V4L2-Capture) Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

