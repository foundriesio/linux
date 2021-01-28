/*
 *      uvc_driver.c  --  USB Video Class driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Driver management


 *****************************************************************************/

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
#include <asm/unaligned.h>

#include <media/v4l2-common.h>

#include "tccvin_video.h"
#include "tccvin_diagnostics.h"

static struct v4l2_subdev *founded_subdev[255] = { 0, };
static int founded_subdev_num;
static DEFINE_MUTEX(founded_subdev_list_lock);

static inline struct v4l2_subdev *tccvin_search_subdev(struct device_node *e)
{
	int i = 0;

	if (e == NULL) {
		loge("input is null\n");
		return NULL;
	}

	mutex_lock(&founded_subdev_list_lock);

	for (i = 0; i < founded_subdev_num; i++) {
		logi("check founded subdev %d\n", i);
		if (founded_subdev[i]->dev->of_node == e) {
			mutex_unlock(&founded_subdev_list_lock);
			return founded_subdev[i];
		}
	}

	mutex_unlock(&founded_subdev_list_lock);

	return NULL;
}

static inline void tccvin_add_subdev_list(struct v4l2_subdev *e)
{
	mutex_lock(&founded_subdev_list_lock);
	logi("%s added\n", dev_name(e->dev));
	founded_subdev[founded_subdev_num++] = e;
	mutex_unlock(&founded_subdev_list_lock);
}

unsigned int tccvin_no_drop_param;
unsigned int tccvin_timeout_param = TCCVIN_CTRL_STREAMING_TIMEOUT;

/* ------------------------------------------------------------------------
 * Descriptors parsing
 */

static int tccvin_parse_format(struct tccvin_device *dev,
	struct tccvin_streaming *streaming, struct tccvin_format *format,
	__u32 **intervals, int nintervals)
{
	struct tccvin_format_desc *fmtdesc;
	struct tccvin_format *streamimg_format;
	struct tccvin_frame *frame;
	struct framesize *framesize;
	int idxFormat, idxFrame, idxInterval;
	int ret = 0;

	for (idxFormat = 0; idxFormat < streaming->nformats; idxFormat++) {
		fmtdesc = tccvin_format_by_index(idxFormat);

		streamimg_format = &streaming->format[idxFormat];
		streamimg_format->index = idxFormat;
		strlcpy(streamimg_format->name, fmtdesc->name,
			sizeof(streamimg_format->name));
		streamimg_format->fcc = fmtdesc->fcc;
		streamimg_format->bpp = fmtdesc->bpp;
		streamimg_format->colorspace = 0;
		streamimg_format->flags = 0;
		streamimg_format->nframes = format->nframes;
		streamimg_format->frame = format->frame;

		logd("index: %d\n",		streamimg_format->index);
		logd("bpp: %d\n",		streamimg_format->bpp);
		logd("colorspace: 0x%08x\n",	streamimg_format->colorspace);
		logd("fcc: 0x%08x\n",		streamimg_format->fcc);
		logd("flags: 0x%08x\n",		streamimg_format->flags);
		logd("name: %s\n",		streamimg_format->name);
		logd("nframes: %d\n",		streamimg_format->nframes);

		for (idxFrame = 0; idxFrame < format->nframes; idxFrame++) {
			frame = &format[idxFormat].frame[idxFrame];
			frame->bFrameIndex = idxFrame;
			framesize = tccvin_framesize_by_index(idxFrame);
			frame->wWidth = framesize->width;
			frame->wHeight = framesize->height;
			frame->dwMaxVideoFrameBufferSize =
				frame->wWidth * frame->wHeight *
				streamimg_format->bpp / 8;
			frame->bFrameIntervalType = nintervals;
			for (idxInterval = 0; idxInterval < nintervals;
				idxInterval++) {
				(*intervals)[idxInterval] =
					tccvin_framerate_by_index(idxInterval);
				frame->dwDefaultFrameInterval =
					(*intervals)[idxInterval];
			}
			frame->dwFrameInterval = *intervals;

			logd("index: %d\n",	frame->bFrameIndex);
			logd("width: %d\n",	frame->wWidth);
			logd("height: %d\n",	frame->wHeight);
			logd("max_buf_size: 0x%08x\n",
				frame->dwMaxVideoFrameBufferSize);
			logd("default_frame_interval: %d\n",
				frame->dwDefaultFrameInterval);
			logd("frame_interval_type: %d\n",
				frame->bFrameIntervalType);
			logd("frame_interval: %d\n",
				*frame->dwFrameInterval);
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

	streaming = kzalloc(sizeof(*streaming), GFP_KERNEL);
	if (streaming == NULL) {
		/* streaming is NULL */
		return -EINVAL;
	}

	mutex_init(&streaming->mutex);
	streaming->dev = dev;

	/* Parse the header descriptor. */
	streaming->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* Count the format and frame descriptors. */
	nformats = tccvin_count_supported_formats();
	nframes = tccvin_count_supported_framesizes();
	nintervals = tccvin_count_supported_framerates();

	if (nformats == 0) {
		loge("no format is supported\n");
		goto error;
	}

	size = nformats * sizeof(*format) + nframes * sizeof(*frame)
	     + nintervals * sizeof(*interval);
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
		&interval, nintervals);
	if (ret < 0) {
		/* failure of parsing format */
		goto error;
	}

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

	ret = tccvin_parse_standard_control(dev);
	if (ret < 0) {
		/* failure of parsing standard control */
		return ret;
	}

	return 0;
}

/* ------------------------------------------------------------------------
 * Video device registration and unregistration
 */

/*
 * Delete the tccvin device.
 *
 * Called by the kernel when the last reference to the tccvin_device structure
 * is released.
 *
 * As this function is called after or during disconnect(), all URBs have
 * already been canceled by the tccvin core. There is no need to kill the
 * interrupt URB manually.
 */
static void tccvin_delete(struct kref *kref)
{
	struct tccvin_device *dev =
		container_of(kref, struct tccvin_device, ref);
	struct tccvin_streaming *streaming = dev->stream;

	if (dev->vdev.dev) {
		/* unregister v4l2 device */
		v4l2_device_unregister(&dev->vdev);
	}

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
	if (video_is_registered(&stream->vdev)) {
		/* unregister video device  */
		video_unregister_device(&stream->vdev);
	}

	/* Deitialize the streaming interface with default streaming
	 * parameters.
	 */
	ret = tccvin_video_deinit(stream);
	if (ret < 0) {
		/* failure of video deinit */
		loge("Failed to initialize the device (%d).\n", ret);
	}
}

static int tccvin_register_video(struct tccvin_device *dev,
		struct tccvin_streaming *stream)
{
	struct video_device *vdev = &stream->vdev;
	int ret;

	/* Initialize the video buffers queue. */
	ret = tccvin_queue_init(&stream->queue, stream->type,
		!tccvin_no_drop_param);
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
	strlcpy(vdev->name, dev->name, sizeof(vdev->name));

	/* Set the driver data before calling video_register_device, otherwise
	 * tccvin_v4l2_open might race us.
	 */
	video_set_drvdata(vdev, stream);

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, vdev->minor);
	if (ret < 0) {
		loge("Failed to register video device (%d).\n", ret);
		return ret;
	}

	/* Initialize the streaming interface with default streaming
	 * parameters.
	 */
	ret = tccvin_video_init(stream);
	if (ret < 0) {
		loge("Failed to initialize the device (%d).\n", ret);
		return ret;
	}

	kref_get(&dev->ref);
	return 0;
}

int tccvin_async_bound(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct tccvin_device	*dev		= NULL;
	struct tccvin_subdev	*tc_subdev	= NULL;
	int			ret		= 0;

	logi("a v4l2 sub device %s is bounded\n", subdev->name);

	dev = container_of(notifier, struct tccvin_device, notifier);
	tc_subdev = container_of(asd, struct tccvin_subdev, asd);

	/* register subdevice here */
	tc_subdev->sd = subdev;
	/* dev->subdevs[dev->bounded_subdevs] = subdev; */
	tccvin_add_subdev_list(subdev);
	dev->bounded_subdevs++;

#if defined(CONFIG_VIDEO_TCCVIN2_DIAG)
	ret = tccvin_diag_cif_port(subdev);
#endif//defined(CONFIG_VIDEO_TCCVIN2_DIAG)

	return ret;
}

void tccvin_async_unbind(struct v4l2_async_notifier *notifier,
			struct v4l2_subdev *subdev,
			struct v4l2_async_subdev *asd)
{
}

int tccvin_async_complete(struct v4l2_async_notifier *notifier)
{
	struct tccvin_device	*dev		= NULL;
	int			ret		= 0;

	dev = container_of(notifier, struct tccvin_device, notifier);

	ret = v4l2_device_register_subdev_nodes(&dev->vdev);
	if (ret != 0) {
		loge("FAIL - register subdev nodes\n");
		goto err;
	}


err:
	return ret;
}

void tccvin_print_fw_node_info(struct tccvin_device *vdev,
	struct device_node *ep)
{
	struct device_node *parent_node = of_graph_get_port_parent(ep);
	const char *io = NULL;
	unsigned int	flags;

	of_property_read_string(ep, "io-direction", &io);

	logi("end point is %s %s\n", parent_node->name, io);

	logi("bus-type: %d\n", vdev->fw_ep[vdev->num_ep].bus_type);
	switch (vdev->fw_ep[vdev->num_ep].bus_type) {
	case V4L2_MBUS_PARALLEL:
	case V4L2_MBUS_BT656:
		flags = vdev->fw_ep[vdev->num_ep].bus.parallel.flags;

		logd("flags: 0x%08x\n", flags);
		/* hsync-active */
		logd("hsync-active: %s\n",
			(flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH) ?
				"V4L2_MBUS_HSYNC_ACTIVE_HIGH" :
				"V4L2_MBUS_HSYNC_ACTIVE_LOW");
		/* vsync-active */
		logd("vsync-active: %s\n",
			(flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH) ?
				"V4L2_MBUS_VSYNC_ACTIVE_HIGH" :
				"V4L2_MBUS_VSYNC_ACTIVE_LOW");
		/* pclk-sample */
		logd("pclk-sample: %s\n",
			(flags & V4L2_MBUS_PCLK_SAMPLE_RISING) ?
				"V4L2_MBUS_PCLK_SAMPLE_RISING" :
				"V4L2_MBUS_PCLK_SAMPLE_FALLING");
		/* data-active */
		logd("data-active: %s\n",
			(flags & V4L2_MBUS_DATA_ACTIVE_HIGH) ?
				"V4L2_MBUS_DATA_ACTIVE_HIGH" :
				"V4L2_MBUS_DATA_ACTIVE_LOW");
		/* conv_en */
		logd("conv_en: %s\n",
			(vdev->fw_ep[vdev->num_ep].bus_type ==
				V4L2_MBUS_BT656) ?
				"V4L2_MBUS_BT656" :
				"V4L2_MBUS_PARALLEL");
		/* bus width */
		logd("bus-width: %d\n",
			vdev->fw_ep[vdev->num_ep].bus.parallel.bus_width);
		break;
	case V4L2_MBUS_CSI2:
		break;
	default:
		break;
	}
}

static void tccvin_fwnode_endpoint_parse(struct tccvin_device *vdev,
				struct device_node *ep)
{
	struct fwnode_handle *fwnode = of_fwnode_handle(ep);
	int channel = -1;

	v4l2_fwnode_endpoint_parse(fwnode, &vdev->fw_ep[vdev->num_ep++]);

	tccvin_print_fw_node_info(vdev, ep);
	if (!fwnode_property_read_u32(fwnode, "channel", &channel))
		logi("channel: %d\n", channel);
	fwnode_property_read_u32(fwnode, "stream-enable",
		&vdev->stream->vs_info.stream_enable);
	fwnode_property_read_u32(fwnode, "gen-field-en",
		&vdev->stream->vs_info.gen_field_en);
	fwnode_property_read_u32(fwnode, "field-low",
		&vdev->stream->vs_info.field_low);
	fwnode_property_read_u32(fwnode, "vs-mask",
		&vdev->stream->vs_info.vs_mask);
	fwnode_property_read_u32(fwnode, "hsde-connect-en",
		&vdev->stream->vs_info.hsde_connect_en);
	fwnode_property_read_u32(fwnode, "intpl-en",
		&vdev->stream->vs_info.intpl_en);
	fwnode_property_read_u32(fwnode, "flush-vsync",
		&vdev->stream->vs_info.flush_vsync);
}

static inline void tccvin_add_async_subdev(struct tccvin_device *vdev,
					   struct device_node *node)
{
	struct v4l2_async_subdev *asd = NULL;

	asd = &(vdev->linked_subdevs[vdev->num_asd].asd);

	asd->match_type = V4L2_ASYNC_MATCH_FWNODE;
	asd->match.fwnode.fwnode = of_fwnode_handle(node);

	vdev->async_subdevs[vdev->num_asd] = asd;
	vdev->num_asd++;

	logi("alloc async subdev for %s\n", node->name);
}

static int tccvin_traversal_subdevices(struct tccvin_device *vdev,
	struct device_node *node, int target_ch)
{
	struct device_node *local_ep = NULL;
	struct device_node *remote_ep = NULL;
	struct device_node *remote_dev = NULL;
	struct v4l2_subdev *founded_sd = NULL;
	bool skip_traversal = false;
	int remote_output_ch = 0;
	int local_input_ch = 0;
	const char *io = NULL;

	logi("========== current node is %s ==========\n", node->name);

	if (node == vdev->pdev->dev.of_node)
		goto skip_alloc_async_subdev;

	founded_sd = tccvin_search_subdev(node);
	if (founded_sd == NULL) {
		tccvin_add_async_subdev(vdev, node);
	} else {
		vdev->linked_subdevs[vdev->num_asd + vdev->bounded_subdevs].sd =
			founded_sd;
		vdev->bounded_subdevs++;
		logi("already subdev(%s) is founded\n", node->name);
	}

skip_alloc_async_subdev:
	for_each_endpoint_of_node(node, local_ep) {
		of_property_read_string(local_ep, "io-direction", &io);
		if (io != NULL && !strcmp(io, "input")) {
			remote_dev = of_graph_get_remote_port_parent(local_ep);
			remote_ep = of_graph_get_remote_endpoint(local_ep);

			if (of_property_read_u32(remote_ep, "channel",
				&(remote_output_ch)) < 0) {
				remote_output_ch = -1;
			}

			if (of_property_read_u32(local_ep, "channel",
				&(local_input_ch)) < 0) {
				local_input_ch = -1;
			}

			if (target_ch != -1 && target_ch == local_input_ch) {
				logi("found matched target ch(%d)\n",
					target_ch);
				target_ch = -1;
				skip_traversal = true;
			}

			if (target_ch != -1 && local_input_ch != -1 &&
				target_ch != local_input_ch) {
				logi("skip this ep... ch is not matched\n");
				logi("target_ch(%d), input_ch(%d)\n",
					target_ch, local_input_ch);
				of_node_put(remote_dev);
				of_node_put(remote_ep);
				continue;
			}

			tccvin_fwnode_endpoint_parse(vdev, local_ep);
			tccvin_traversal_subdevices(vdev, remote_dev,
					(target_ch) != -1 ?
					target_ch : remote_output_ch);

			of_node_put(remote_dev);
			of_node_put(remote_ep);

			if (skip_traversal)
				break;
		}
		of_node_put(local_ep);
	}

	return 0;
}

int tccvin_init_subdevices(struct tccvin_device *vdev)
{
	struct v4l2_async_notifier *notifier = &vdev->notifier;
	int ret = 0;

	tccvin_traversal_subdevices(vdev, vdev->pdev->dev.of_node, -1);

	logi("the nubmer of async subdevices : %d\n", vdev->num_asd);
	if (vdev->num_asd <= 0) {
		logi("Nothing to register\n");
		goto end;
	}

	logd("the num of subdevs to be registered: %d\n", vdev->num_asd);
	notifier->num_subdevs	= vdev->num_asd;
	notifier->subdevs	= vdev->async_subdevs;
	notifier->bound		= tccvin_async_bound;
	notifier->complete	= tccvin_async_complete;
	notifier->unbind	= tccvin_async_unbind;
	ret = v4l2_async_notifier_register(vdev->stream->vdev.v4l2_dev,
		notifier);
	if (ret < 0) {
		loge("Error registering async notifier for tccvin\n");
		ret = -EINVAL;
		goto end;
	}

	logi("Succeed to register async notifier for tccvin\n");

end:
	return ret;
}

/* ------------------------------------------------------------------------
 * Driver initialization and cleanup
 */

static int tccvin_core_probe(struct platform_device *pdev)
{
	struct tccvin_device *dev;

	/* Get the index from its alias */
	pdev->id = of_alias_get_id(pdev->dev.of_node, "videoinput");
	logd("Platform Device index: %d, name: %s\n", pdev->id, pdev->name);

	/* Allocate memory for the device and initialize it. */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		/* dev is null */
		return -ENOMEM;
	}

	dev->bounded_subdevs = 0;
	dev->current_subdev_idx = 0;

	INIT_LIST_HEAD(&dev->entities);
	kref_init(&dev->ref);
	mutex_init(&dev->lock);

	dev->pdev = pdev;

	strlcpy(dev->name, "video-input path", sizeof(dev->name));

	/* Parse the Video Class control descriptor. */
	if (tccvin_parse_control(dev) < 0) {
		loge("Unable to parse tccvin descriptors.\n");
		goto error;
	}

	/* Initialize the media device and register the V4L2 device. */
	if (v4l2_device_register(&pdev->dev, &dev->vdev) < 0) {
		/* failure of registering v4l2 device */
		goto error;
	}

	tccvin_register_video(dev, dev->stream);

	platform_set_drvdata(pdev, dev);

	if (tccvin_init_subdevices(dev) < 0)
		goto e_v4l2_dev_unregister;

	/* Create the tccvin_recovery_trigger sysfs */
	tccvin_create_recovery_trigger(&dev->pdev->dev);

	/* Create the tccvin_timestamp sysfs */
	tccvin_create_timestamp(&dev->pdev->dev);

	return 0;

e_v4l2_dev_unregister:
	v4l2_device_unregister(dev->stream->vdev.v4l2_dev);
error:
	tccvin_unregister_video(dev);
	kref_put(&dev->ref, tccvin_delete);
	return -ENODEV;
}

static int tccvin_core_remove(struct platform_device *pdev)
{
	struct tccvin_device *dev	= platform_get_drvdata(pdev);

	tccvin_unregister_video(dev);
	kref_put(&dev->ref, tccvin_delete);

	/* free the memory for the camera device */
	kfree(dev);
	return 0;
}

const static struct of_device_id tccvin_of_match[] = {
	{ .compatible = "telechips,video-input" },
};

MODULE_DEVICE_TABLE(of, tccvin_of_match);

static struct platform_driver tccvin_core_driver = {
	.probe		= tccvin_core_probe,
	.remove		= tccvin_core_remove,
	.driver		= {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tccvin_of_match),
	},
};

module_platform_driver(tccvin_core_driver);

MODULE_AUTHOR("Telechips.Co.Ltd");
MODULE_DESCRIPTION("Telechips Video-Input Path(V4L2-Capture) Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
