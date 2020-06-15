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
	struct tccvin_frame *frame;
	__u32 guid;
	int idxFrame;
	int ret = 0;

	format->index		= 1;
	format->bpp			= 16;
	format->colorspace	= 0;
	format->flags		= 0;

	/* Find the format descriptor from its GUID. */
	guid = VIOC_IMG_FMT_YUYV;
	fmtdesc = tccvin_format_by_guid(guid);

	if (fmtdesc != NULL) {
		strlcpy(format->name, fmtdesc->name,
			sizeof format->name);
		format->fcc = fmtdesc->fcc;
	} else {
		logi("Unknown video format\n");
		snprintf(format->name, sizeof(format->name), "%s\n", "Unknown");
		format->fcc = 0;
	}

	logd("index: %d, bpp: %d, colorspace: 0x%08x, fcc: 0x%08x, flags: 0x%08x, name: %s, nframes: %d\n", \
		format->index, format->bpp, format->colorspace, format->fcc, format->flags, format->name, format->nframes);

	for(idxFrame=0; idxFrame<format->nframes; idxFrame++) {
		frame = &format->frame[idxFrame];
		frame->bFrameIndex					= idxFrame;
		frame->bmCapabilities				= 0x00000000;
		frame->wWidth						= framesize_list[idxFrame].width;
		frame->wHeight						= framesize_list[idxFrame].height;
		frame->dwMaxVideoFrameBufferSize	= frame->wWidth * frame->wHeight * format->bpp / 8;;
		frame->dwDefaultFrameInterval		= 333333;
		frame->bFrameIntervalType			= 2;
		frame->dwFrameInterval				= *intervals;
		frame->dwFrameInterval[0]			= 333333;
		frame->dwFrameInterval[1]			= 666667;
		frame->dwDefaultFrameInterval =
			min(frame->dwFrameInterval[1],
			    max(frame->dwFrameInterval[0],
				frame->dwDefaultFrameInterval));
		logd("format->nframes: %d, bFrameIndex: %d, bmCapabilities: 0x%08x, wWidth: %d, wHeight: %d, dwMaxVideoFrameBufferSize: %d, dwDefaultFrameInterval: %d, bFrameIntervalType: %d, dwFrameInterval: %d", \
			format->nframes, frame->bFrameIndex, frame->bmCapabilities, frame->wWidth, frame->wHeight, frame->dwMaxVideoFrameBufferSize, frame->dwDefaultFrameInterval, frame->bFrameIntervalType, *frame->dwFrameInterval);
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
	nformats = 1;
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

	/* Initialize the streaming interface with default streaming
	 * parameters.
	 */
	ret = tccvin_video_init(stream);
	if (ret < 0) {
		loge("Failed to initialize the device "
			"(%d).\n", ret);
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
	vdev->release = tccvin_release;
	strlcpy(vdev->name, dev->name, sizeof vdev->name);

	/* Set the driver data before calling video_register_device, otherwise
	 * tccvin_v4l2_open might race us.
	 */
	video_set_drvdata(vdev, stream);

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		loge("Failed to register video device (%d).\n",
			   ret);
		return ret;
	}

	kref_get(&dev->ref);
	FUNCTION_OUT
	return 0;
}

int tccvin_async_bound(struct v4l2_async_notifier *notifier, struct v4l2_subdev *subdev, struct v4l2_async_subdev *asd) {
	struct tccvin_device * dev = NULL;
	videosource_t * vs = NULL;
	int	enable = 1;
	int	ret = 0;

	printk("The callback function is invoked from found node: %s\n",
	       asd->match.device_name.name);

	printk("Subdevice name: %s\n", subdev->name);

	dev = container_of(notifier, struct tccvin_device, notifier);

	// register subdevice here
	dev->subdevs[dev->num_registered_subdev++] = subdev;

	// get videosource format
	vs = container_of(subdev, struct videosource, sd);
	dev->stream->cif.videosource_info		= &vs->format;

	// power-up sequence & initial i2c setting
	ret = v4l2_subdev_call(subdev, core, s_power, enable);
	ret = v4l2_subdev_call(subdev, video, s_stream, enable);

	return 0;
}

void tccvin_async_unbind(struct v4l2_async_notifier *notifier,
			struct v4l2_subdev *subdev,
			struct v4l2_async_subdev *asd) {
	printk("%s is invoked !! \n", __func__);
}

int tccvin_async_complete(struct v4l2_async_notifier *notifier) {
	// use subdevice at index 0(isl79988) as default

	return 0;
}

int tccvin_init_subdevices(struct tccvin_device *vdev) {
	struct v4l2_async_notifier *notifier = &(vdev->notifier);
	struct v4l2_async_subdev *asd;
//	const char* dname = "isl79988";

	int err, i;

	vdev->num_registered_subdev = 0;

	/* Set the number of all sub-devices to 1 temporarily. This
	 * should be changed according to the working environment */
	notifier->subdevs = vdev->asd;
	notifier->num_subdevs = 1;

	for (i = 0; i < notifier->num_subdevs; i++) {
		vdev->asd[i] = kmalloc(sizeof(struct v4l2_async_subdev), GFP_KERNEL);
	}

	/* after allocation, set asd */
	for (i = 0; i < notifier->num_subdevs; i++) {
		asd = vdev->asd[i];

		/* For match type, it could be device name and i2c like; V4L2_ASYNC_MATCH_I2C */
		asd->match_type = V4L2_ASYNC_MATCH_DEVNAME;
		asd->match.device_name.name = "0-0044";

		// check include/media/v4l2-async.h
	}

	printk("isl79988 should be %s\n", asd->match.device_name.name);

	notifier->bound = tccvin_async_bound;
	notifier->complete = tccvin_async_complete;
	notifier->unbind = tccvin_async_unbind;
	err = v4l2_async_notifier_register(vdev->stream->vdev.v4l2_dev, notifier);

	if (err < 0) {
		printk(KERN_ERR "Error registering async notifier for tccvin\n");
		v4l2_device_unregister(vdev->stream->vdev.v4l2_dev);
		return -EINVAL;
	} else {
		printk(KERN_ERR "Succeed to register async notifier for tccvin\n");
	}
	return 0;
}

/* ------------------------------------------------------------------------
 * Driver initialization and cleanup
 */

static int tccvin_core_probe(struct platform_device * pdev) {
	struct tccvin_device *dev;
	FUNCTION_IN

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

