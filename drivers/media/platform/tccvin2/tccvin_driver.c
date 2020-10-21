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
#include <linux/of_graph.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>

#include "tccvin_video.h"

struct framesize framesize_list[] = {
	{	 320,	 240	},
	{	 640,	 480	},
	{	1280,	 720	},
	{	1920,	 720	},
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
			//frame->dwMaxVideoFrameBufferSize	= frame->wWidth * frame->wHeight *  streamimg_format->bpp / 8;
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
	nformats = tccvin_format_num();
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

 	if (dev->vdev.dev)
 		v4l2_device_unregister(&dev->vdev);

	kfree(streaming->format);
	kfree(streaming);

	kfree(dev);
}

static void tccvin_release(struct video_device *vdev)
{
	struct tccvin_streaming *stream = video_get_drvdata(vdev);
	struct tccvin_device *dev = stream->dev;

	kref_put(&dev->ref, tccvin_delete);
}

/*
 * Unregister the video devices.
 */
static void tccvin_unregister_video(struct tccvin_device *dev)
{
	struct tccvin_streaming *stream;
	int ret;

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
}

static int tccvin_register_video(struct tccvin_device *dev,
		struct tccvin_streaming *stream)
{
	struct video_device *vdev = &stream->vdev;
	int ret;

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
	return 0;
}

int tccvin_async_bound(struct v4l2_async_notifier *notifier, struct v4l2_subdev *subdev, struct v4l2_async_subdev *asd) {
	struct tccvin_device	* dev		= NULL;
	int			enable		= 1;
	struct v4l2_dv_timings	*timings	= NULL;
	int			idxTry		= 0;
	int			nTry		= 3;
	unsigned int		status		= 0;
	int			ret		= 0;

	loge("a v4l2 sub device %s is bounded\n", subdev->name);

	dev = container_of(notifier, struct tccvin_device, notifier);

	// register subdevice here
	dev->subdevs[dev->bounded_subdevs++] = subdev;

	timings = &dev->stream->dv_timings;
	ret = v4l2_subdev_call(subdev, video, g_dv_timings, timings);
	if (ret) {
		loge("v4l2_subdev_call(video, g_dv_timings) is wrong\n");
	} else {
		logd("width: %d, height: %d, interalced: %d, polarities: 0x%08x\n", \
		     timings->bt.width, timings->bt.height, timings->bt.interlaced, timings->bt.polarities);
	}

	// power-up sequence & initial i2c setting
	ret = v4l2_subdev_call(subdev, core, s_power, enable);
	if (ret) {
		loge("v4l2_subdev_call(subdev, core, s_power, enable) is wrong\n");
	}

	ret = v4l2_subdev_call(subdev, video, s_stream, enable);
	if (ret) {
		loge("v4l2_subdev_call(subdev, video, s_stream, enable) is wrong\n");
	}

	for(idxTry = 0; idxTry < nTry; idxTry++) {
		ret = v4l2_subdev_call(subdev, video, g_input_status, &status);
		if(ret < 0) {
			loge("subdev is not avaliable\n");
		} else {
			if(status & V4L2_IN_ST_NO_SIGNAL) {
				loge("subdev is in V4L2_IN_ST_NO_SIGNAL status\n");
			} else {
				loge("subdev is available\n");
				break;
			}
		}
		msleep(10);
	}


	return ret;
}

void tccvin_async_unbind(struct v4l2_async_notifier *notifier,
			struct v4l2_subdev *subdev,
			struct v4l2_async_subdev *asd) {
}

int tccvin_async_complete(struct v4l2_async_notifier *notifier) {
	return 0;
}

int idxSubDev = 0;

int tccvin_init_endpoint_gragh(struct tccvin_device *vdev, struct device_node *base_node) {
	struct device_node		*remt_node	= NULL;		// the remote of the base

	struct device_node		*repa_node	= NULL;		// the parent of the remote
	struct device_node		*endp_node	= NULL;
	int				ret		= 0;

	loge("loop: %d\n", idxSubDev);

	loge("base_node: 0x%px\n", base_node);

	remt_node = of_graph_get_remote_endpoint(base_node);
	if(!remt_node) {
		loge("to find a remote endpoint of the base endpoint node\n");
		ret = -ENODEV;
	} else {
		loge("remt_node: 0x%px\n", remt_node);

		repa_node = of_graph_get_remote_port_parent(base_node);
		if(!repa_node) {
			loge("to find a remote parent node\n");
			ret = -ENODEV;
		} else {
			loge("repa_node: 0x%px\n", repa_node);

			while(1) {
				endp_node = of_graph_get_next_endpoint(repa_node, endp_node);
				if(!endp_node) {
					loge("to find an endpoint of the parent of the remote node\n");
					ret = -ENODEV;
					break;
				} else {
					loge("endp_node: 0x%px\n", endp_node);

					if(endp_node != remt_node) {
						loge("endp_node != remt_node\n");
						tccvin_init_endpoint_gragh(vdev, endp_node);
						of_node_put(endp_node);
					} else {
						loge("endp_node == remt_node\n");
						ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(base_node), &vdev->fw_ep[idxSubDev]);
					        if(ret) {
							loge("to parse v4l2 fwnode endpoint\n");
					        } else {
							loge("bus-type: %d\n", vdev->fw_ep[idxSubDev].bus_type);
							switch(vdev->fw_ep[idxSubDev].bus_type) {
							case V4L2_MBUS_PARALLEL:
							case V4L2_MBUS_BT656:
								loge("bus.parallel.flags: 0x%08x\n", vdev->fw_ep[idxSubDev].bus.parallel.flags);
								// hsync-active
								if(vdev->fw_ep[idxSubDev].bus.parallel.flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH)
									loge("hsync-active: V4L2_MBUS_HSYNC_ACTIVE_HIGH\n");
								else if(vdev->fw_ep[idxSubDev].bus.parallel.flags & V4L2_MBUS_HSYNC_ACTIVE_LOW)
									loge("hsync-active: V4L2_MBUS_HSYNC_ACTIVE_LOW\n");
								// vsync-active
								if(vdev->fw_ep[idxSubDev].bus.parallel.flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH)
									loge("vsync-active: V4L2_MBUS_VSYNC_ACTIVE_HIGH\n");
								else if(vdev->fw_ep[idxSubDev].bus.parallel.flags & V4L2_MBUS_VSYNC_ACTIVE_LOW)
									loge("vsync-active: V4L2_MBUS_VSYNC_ACTIVE_LOW\n");
								// pclk-sample
								vdev->stream->vs_sync_info.pclk_polarity = (vdev->fw_ep[idxSubDev].bus.parallel.flags & V4L2_MBUS_PCLK_SAMPLE_RISING) ? 1 : 0;
								loge("pclk-sample: %s\n", vdev->stream->vs_sync_info.pclk_polarity ? "V4L2_MBUS_PCLK_SAMPLE_RISING" : "V4L2_MBUS_PCLK_SAMPLE_FALLING");
								// data-active
								vdev->stream->vs_sync_info.de_active_low = (vdev->fw_ep[idxSubDev].bus.parallel.flags & V4L2_MBUS_DATA_ACTIVE_HIGH) ? 1 : 0;
								loge("data-active: %s\n", vdev->stream->vs_sync_info.de_active_low ? "V4L2_MBUS_DATA_ACTIVE_HIGH" : "V4L2_MBUS_DATA_ACTIVE_LOW");
								// conv_en
								vdev->stream->vs_sync_info.conv_en = (vdev->fw_ep[idxSubDev].bus_type == V4L2_MBUS_BT656) ? 1 : 0;
								loge("conv_en: %s\n", vdev->stream->vs_sync_info.conv_en ? "V4L2_MBUS_BT656" : "V4L2_MBUS_PARALLEL");
								// bus width
								loge("bus-width: %d\n", vdev->fw_ep[idxSubDev].bus.parallel.bus_width);
								break;
							case V4L2_MBUS_CSI2:
								break;
							default:
								break;
							}
						}

						// common video source sync info
						{
							struct fwnode_handle *fwnode = of_fwnode_handle(base_node);

							// data-order
							if(!fwnode_property_read_u32(fwnode, "data-order", &vdev->stream->vs_sync_info.data_order))
								loge("data-order: %d\n", vdev->stream->vs_sync_info.data_order);
							// data-format
							if(!fwnode_property_read_u32(fwnode, "data-format", &vdev->stream->vs_sync_info.data_format))
								loge("data-format: %d\n", vdev->stream->vs_sync_info.data_format);
							// stream-enable
							if(!fwnode_property_read_u32(fwnode, "stream-enable", &vdev->stream->vs_sync_info.stream_enable))
								loge("stream-enable: %d\n", vdev->stream->vs_sync_info.stream_enable);
							// conv-en
							if(!fwnode_property_read_u32(fwnode, "conv-en", &vdev->stream->vs_sync_info.conv_en))
								loge("conv-en: %d\n", vdev->stream->vs_sync_info.conv_en);
							// flush-vsync
							if(!fwnode_property_read_u32(fwnode, "flush-vsync", &vdev->stream->vs_sync_info.flush_vsync))
								loge("flush-vsync: %d\n", vdev->stream->vs_sync_info.flush_vsync);
						}

						vdev->asd[idxSubDev] = kmalloc(sizeof(struct v4l2_async_subdev), GFP_KERNEL);
						vdev->asd[idxSubDev]->match_type		= V4L2_ASYNC_MATCH_FWNODE;
						vdev->asd[idxSubDev]->match.fwnode.fwnode	= of_fwnode_handle(repa_node);
						idxSubDev++;

						of_node_put(endp_node);
						break;
					}
//					of_node_put(endp_node);
				}
			}
			of_node_put(repa_node);
		}
		of_node_put(remt_node);
	}

	return ret;
}

int tccvin_init_subdevices(struct tccvin_device *vdev) {
	struct device_node		*main_node	= vdev->stream->dev->pdev->dev.of_node;
	struct device_node		*endp_node	= NULL;
	struct v4l2_async_notifier	*notifier	= &vdev->notifier;
	int				ret		= 0;

	endp_node = of_graph_get_next_endpoint(main_node, endp_node);
	if(!endp_node) {
		loge("to find an endpoint node\n");
		ret = -ENODEV;
	} else {
		tccvin_init_endpoint_gragh(vdev, endp_node);
		of_node_put(endp_node);
	}

	loge("idxSubDev: %d\n", idxSubDev);
	notifier->num_subdevs	= idxSubDev;
	notifier->subdevs	= vdev->asd;
	notifier->bound		= tccvin_async_bound;
	notifier->complete	= tccvin_async_complete;
	notifier->unbind	= tccvin_async_unbind;
	ret = v4l2_async_notifier_register(vdev->stream->vdev.v4l2_dev, notifier);
	if(ret < 0) {
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

	// Get the index from its alias
	pdev->id = of_alias_get_id(pdev->dev.of_node, "videoinput");
	logd("Platform Device index: %d, name: %s\n", pdev->id, pdev->name);

	/* Allocate memory for the device and initialize it. */
	if ((dev = kzalloc(sizeof *dev, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	dev->bounded_subdevs = 0;
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

	return 0;

error:
	tccvin_unregister_video(dev);
	kref_put(&dev->ref, tccvin_delete);
	return -ENODEV;
}

static int tccvin_core_remove(struct platform_device * pdev) {
	struct tccvin_device *dev	= platform_get_drvdata(pdev);

	tccvin_unregister_video(dev);
	kref_put(&dev->ref, tccvin_delete);

	// free the memory for the camera device
	kfree(dev);
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
