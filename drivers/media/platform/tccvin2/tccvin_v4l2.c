/*
 *      uvc_v4l2.c  --  USB Video Class driver - V4L2 API
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


 *   Description : v4l2 interface


 *****************************************************************************/

#include <linux/compat.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <linux/atomic.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>

#include "tccvin_video.h"

/* ------------------------------------------------------------------------
 * V4L2 interface
 */

/*
 * Find the frame interval closest to the requested frame interval for the
 * given frame format and size. This should be done by the device as part of
 * the Video Probe and Commit negotiation, but some hardware don't implement
 * that feature.
 */
static __u32 tccvin_try_frame_interval(struct tccvin_frame *frame,
	__u32 interval)
{
	unsigned int i;

	if (frame->bFrameIntervalType) {
		__u32 best = -1, dist;

		for (i = 0; i < frame->bFrameIntervalType; ++i) {
			dist = interval > frame->dwFrameInterval[i]
			     ? interval - frame->dwFrameInterval[i]
			     : frame->dwFrameInterval[i] - interval;

			if (dist > best) {
				/* dist > best */
				break;
			}

			best = dist;
		}

		interval = frame->dwFrameInterval[i-1];
	}

	return interval;
}

static __u32 tccvin_v4l2_get_bytesperline(const struct tccvin_format *format,
	const struct tccvin_frame *frame)
{
	switch (format->fcc) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_M420:
		return frame->wWidth;

	default:
		return format->bpp * frame->wWidth / 8;
	}
}

static int tccvin_v4l2_try_format(struct tccvin_streaming *stream,
	struct v4l2_format *fmt, struct tccvin_format **tccvin_format,
	struct tccvin_frame **tccvin_frame)
{
	struct tccvin_format *format = NULL;
	struct tccvin_frame *frame = NULL;
	__u16 rw, rh;
	unsigned int d, maxd;
	unsigned int i;
	__u32 interval;
	int ret = 0;
	__u8 *fcc;

	if (fmt->type != stream->type) {
		loge("fmt->type: 0x%08x, stream->type: 0x%08x\n",
			fmt->type, stream->type);
		return -EINVAL;
	}

	fcc = (__u8 *)&fmt->fmt.pix.pixelformat;
	logd("Trying format 0x%08x (%c%c%c%c): %ux%u.\n",
			fmt->fmt.pix.pixelformat,
			fcc[0], fcc[1], fcc[2], fcc[3],
			fmt->fmt.pix.width, fmt->fmt.pix.height);

	/* Check if the hardware supports the requested format, use the default
	 * format otherwise.
	 */
	for (i = 0; i < stream->nformats; ++i) {
		format = &stream->format[i];
		if (format->fcc == fmt->fmt.pix.pixelformat) {
			/* fcc is available */
			break;
		}
	}

	if (i == stream->nformats) {
		format = stream->def_format;
		fmt->fmt.pix.pixelformat = format->fcc;
	}

	/* Find the closest image size. The distance between image sizes is
	 * the size in pixels of the non-overlapping regions between the
	 * requested size and the frame-specified size.
	 */
	rw = fmt->fmt.pix.width;
	rh = fmt->fmt.pix.height;
	maxd = (unsigned int)-1;

	for (i = 0; i < format->nframes; ++i) {
		__u16 w = format->frame[i].wWidth;
		__u16 h = format->frame[i].wHeight;

		d = min(w, rw) * min(h, rh);
		d = w*h + rw*rh - 2*d;
		if (d < maxd) {
			maxd = d;
			frame = &format->frame[i];
		}

		if (maxd == 0) {
			/* max is 0 */
			break;
		}
	}

	if (frame == NULL) {
		loge("Unsupported size %ux%u.\n",
				fmt->fmt.pix.width, fmt->fmt.pix.height);
		return -EINVAL;
	}

	/* Use the default frame interval. */
	interval = frame->dwDefaultFrameInterval;
	logd("Using default frame interval %u.%u us (%u.%u fps).\n",
		interval/10, interval%10, 10000000/interval,
		(100000000/interval)%10);

	/* Set the format index, frame index and frame interval. */
	tccvin_try_frame_interval(frame, interval);

	fmt->fmt.pix.width = frame->wWidth;
	fmt->fmt.pix.height = frame->wHeight;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.bytesperline =
		tccvin_v4l2_get_bytesperline(format, frame);
	frame->dwMaxVideoFrameBufferSize =
		frame->wWidth * frame->wHeight * format->bpp / 8;
	fmt->fmt.pix.sizeimage = frame->dwMaxVideoFrameBufferSize;
	fmt->fmt.pix.colorspace = format->colorspace;
	fmt->fmt.pix.priv = 0;

	logd("width: %d, height: %d\n",
		fmt->fmt.pix.width, fmt->fmt.pix.height);
	logd("field: 0x%08x\n",		fmt->fmt.pix.field);
	logd("bytesperline: 0x%08x\n",	fmt->fmt.pix.bytesperline);
	logd("sizeimage: 0x%08x\n",	fmt->fmt.pix.sizeimage);
	logd("colorspace: 0x%08x\n",	fmt->fmt.pix.colorspace);

	if (tccvin_format != NULL) {
		/* format is available */
		*tccvin_format = format;
	}
	if (tccvin_frame != NULL) {
		/* frame is available */
		*tccvin_frame = frame;
	}

	return ret;
}

static int tccvin_v4l2_get_format(struct tccvin_streaming *stream,
	struct v4l2_format *fmt)
{
	struct tccvin_format *format;
	struct tccvin_frame *frame;
	int ret = 0;

	if (fmt->type != stream->type) {
		/* type is wrong */
		return -EINVAL;
	}

	mutex_lock(&stream->mutex);
	format = stream->cur_format;
	frame = stream->cur_frame;

	if (format == NULL || frame == NULL) {
		ret = -EINVAL;
		goto done;
	}

	fmt->fmt.pix.pixelformat = format->fcc;
	fmt->fmt.pix.width = frame->wWidth;
	fmt->fmt.pix.height = frame->wHeight;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.bytesperline =
		tccvin_v4l2_get_bytesperline(format, frame);
	fmt->fmt.pix.sizeimage = frame->dwMaxVideoFrameBufferSize;
	fmt->fmt.pix.colorspace = format->colorspace;
	fmt->fmt.pix.priv = 0;

done:
	mutex_unlock(&stream->mutex);
	return ret;
}

static int tccvin_v4l2_set_format(struct tccvin_streaming *stream,
	struct v4l2_format *fmt)
{
	struct tccvin_format *format;
	struct tccvin_frame *frame;
	int ret;

	if (fmt->type != stream->type) {
		/* type is wrong */
		return -EINVAL;
	}

	ret = tccvin_v4l2_try_format(stream, fmt, &format, &frame);
	if (ret < 0) {
		/* failure of trying format */
		return ret;
	}

	mutex_lock(&stream->mutex);

	if (tccvin_queue_is_allocated(&stream->queue)) {
		ret = -EBUSY;
		goto done;
	}

	stream->cur_format = format;
	stream->cur_frame = frame;

done:
	mutex_unlock(&stream->mutex);
	return ret;
}

/* ------------------------------------------------------------------------
 * Privilege management
 */

/*
 * Privilege management is the multiple-open implementation basis. The current
 * implementation is completely transparent for the end-user and doesn't
 * require explicit use of the VIDIOC_G_PRIORITY and VIDIOC_S_PRIORITY ioctls.
 * Those ioctls enable finer control on the device (by making possible for a
 * user to request exclusive access to a device), but are not mature yet.
 * Switching to the V4L2 priority mechanism might be considered in the future
 * if this situation changes.
 *
 * Each open instance of a TCCVIN device can either be in a privileged or
 * unprivileged state. Only a single instance can be in a privileged state at
 * a given time. Trying to perform an operation that requires privileges will
 * automatically acquire the required privileges if possible, or return -EBUSY
 * otherwise. Privileges are dismissed when closing the instance or when
 * freeing the video buffers using VIDIOC_REQBUFS.
 *
 * Operations that require privileges are:
 *
 * - VIDIOC_S_INPUT
 * - VIDIOC_S_PARM
 * - VIDIOC_S_FMT
 * - VIDIOC_REQBUFS
 */
static int tccvin_acquire_privileges(struct tccvin_fh *handle)
{
	/* Always succeed if the handle is already privileged. */
	if (handle->state == TCCVIN_HANDLE_ACTIVE) {
		/* state is active */
		return 0;
	}

	/* Check if the device already has a privileged handle. */
	if (atomic_inc_return(&handle->stream->active) != 1) {
		atomic_dec(&handle->stream->active);
		return -EBUSY;
	}

	handle->state = TCCVIN_HANDLE_ACTIVE;
	return 0;
}

static void tccvin_dismiss_privileges(struct tccvin_fh *handle)
{
	if (handle->state == TCCVIN_HANDLE_ACTIVE) {
		/* state is not active */
		atomic_dec(&handle->stream->active);
	}

	handle->state = TCCVIN_HANDLE_PASSIVE;
}

static int tccvin_has_privileges(struct tccvin_fh *handle)
{
	return handle->state == TCCVIN_HANDLE_ACTIVE;
}

/* ------------------------------------------------------------------------
 * V4L2 file operations
 */

static int tccvin_v4l2_open(struct file *file)
{
	struct tccvin_streaming *stream;
	struct tccvin_fh *handle;

	stream = video_drvdata(file);

	/* Create the device handle. */
	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (handle == NULL) {
		/* failure of allocating memory */
		return -ENOMEM;
	}

	mutex_lock(&stream->dev->lock);
	stream->dev->users++;
	stream->sequence = 0;
	mutex_unlock(&stream->dev->lock);

	v4l2_fh_init(&handle->vfh, &stream->vdev);
	v4l2_fh_add(&handle->vfh);
	handle->stream = stream;
	handle->state = TCCVIN_HANDLE_PASSIVE;
	file->private_data = handle;

	return 0;
}

static int tccvin_v4l2_release(struct file *file)
{
	struct tccvin_fh *handle = file->private_data;
	struct tccvin_streaming *stream = handle->stream;

	/* Only free resources if this is a privileged handle. */
	if (tccvin_has_privileges(handle)) {
		/* release privileges */
		tccvin_queue_release(&stream->queue);
	}

	/* Release the file handle. */
	tccvin_dismiss_privileges(handle);
	v4l2_fh_del(&handle->vfh);
	v4l2_fh_exit(&handle->vfh);
	kfree(handle);
	file->private_data = NULL;

	mutex_lock(&stream->dev->lock);
	--stream->dev->users;
	mutex_unlock(&stream->dev->lock);

	return 0;
}

static int tccvin_ioctl_querycap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	struct video_device *vdev = video_devdata(file);

	strlcpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
	strlcpy(cap->card, vdev->name, sizeof(cap->card));
	logd("num: %d\n", vdev->num);
	logd("index: %d\n", vdev->index);
	snprintf(cap->bus_info, sizeof(cap->bus_info), "video-capture-%s",
		cap->driver);
	cap->version = KERNEL_VERSION(4, 14, 00);
	cap->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE;
	cap->capabilities = V4L2_CAP_DEVICE_CAPS | cap->device_caps;

	logd("driver: %s\n", cap->driver);
	logd("card: %s\n", cap->card);
	logd("bus_info: %s\n", cap->bus_info);
	logd("version: %u.%u.%u\n", (cap->version >> 16) & 0xFF,
		(cap->version >> 8) & 0xFF, cap->version & 0xFF);
	logd("device_caps: 0x%08x\n", cap->device_caps);
	logd("capabilities: 0x%08x\n", cap->capabilities);

	return 0;
}

static int tccvin_ioctl_enum_fmt(struct tccvin_streaming *stream,
	struct v4l2_fmtdesc *fmt)
{
	struct tccvin_format *format;
	enum v4l2_buf_type type = fmt->type;
	__u32 index = fmt->index;

	if (fmt->type != stream->type || fmt->index >= stream->nformats) {
		/* type or format is unavailable */
		return -EINVAL;
	}

	memset(fmt, 0, sizeof(*fmt));
	fmt->index = index;
	fmt->type = type;

	format = &stream->format[fmt->index];
	fmt->flags = 0;
	strlcpy(fmt->description, format->name, sizeof(fmt->description));
	fmt->description[sizeof(fmt->description) - 1] = 0;
	fmt->pixelformat = format->fcc;

	logd("index: %d\n",		fmt->index);
	logd("type: 0x%08x,\n",		fmt->type);
	logd("flags: 0x%08x\n",		fmt->flags);
	logd("description: %s\n",	fmt->description);
	logd("pixelformat: 0x%08x\n",	fmt->pixelformat);

	return 0;
}

static int tccvin_ioctl_enum_fmt_vid_cap(struct file *file, void *fh,
	struct v4l2_fmtdesc *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int		ret = 0;

	ret = tccvin_ioctl_enum_fmt(stream, fmt);

	return ret;
}

static int tccvin_ioctl_g_fmt_vid_cap(struct file *file, void *fh,
				   struct v4l2_format *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	return tccvin_v4l2_get_format(stream, fmt);
}

static int tccvin_ioctl_s_fmt_vid_cap(struct file *file, void *fh,
	struct v4l2_format *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;

	ret = tccvin_acquire_privileges(handle);
	if (ret < 0) {
		/* failure of acquiring privileges */
		return ret;
	}

	return tccvin_v4l2_set_format(stream, fmt);
}

static int tccvin_ioctl_try_fmt_vid_cap(struct file *file, void *fh,
	struct v4l2_format *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int	ret;

	ret = tccvin_v4l2_try_format(stream, fmt, NULL, NULL);
	if (ret < 0) {
		/* failure of trying format */
		loge("tccvin_v4l2_try_format, ret: %d\n", ret);
	}

	return ret;
}

static int tccvin_ioctl_reqbufs(struct file *file, void *fh,
	struct v4l2_requestbuffers *rb)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;

	ret = tccvin_acquire_privileges(handle);
	if (ret < 0) {
		/* failure of acquiring privileges */
		return ret;
	}

	mutex_lock(&stream->mutex);
	ret = tccvin_request_buffers(&stream->queue, rb);
	mutex_unlock(&stream->mutex);
	if (ret < 0) {
		/* failure of requesting buffers */
		return ret;
	}

	if (ret == 0) {
		/* dismiss privileges */
		tccvin_dismiss_privileges(handle);
	}

	if (rb->memory == V4L2_MEMORY_DMABUF) {
		ret = tccvin_allocated_dmabuf(stream, rb->count);
		if (ret < 0) {
			loge("fail to allocated dmabuf\n");
			return ret;
		}
	}

	return 0;
}

static int tccvin_ioctl_querybuf(struct file *file, void *fh,
	struct v4l2_buffer *buf)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;

	if (!tccvin_has_privileges(handle)) {
		/* device is busy */
		return -EBUSY;
	}

	ret = tccvin_query_buffer(&stream->queue, buf);
	logd("index: %d\n",		buf->index);
	logd("type: 0x%08x\n",		buf->type);
	logd("bytesused: 0x%08x\n",	buf->bytesused);
	logd("flags: 0x%08x\n",		buf->flags);
	logd("field: 0x%08x\n",		buf->field);
	logd("memory: 0x%08x\n",	buf->memory);
	logd("m.offset: 0x%08x\n",	buf->m.offset);
	logd("lengh: 0x%08x\n",		buf->length);

	ret = tccvin_set_buffer_address(stream, buf);
	if (ret < 0) {
		loge("Fail to set buffer address, buffer index %d\n",
			buf->index);
	}

	return ret;
}

static int tccvin_ioctl_qbuf(struct file *file, void *fh,
	struct v4l2_buffer *buf)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	if (!tccvin_has_privileges(handle)) {
		/* device is busy */
		return -EBUSY;
	}

	return tccvin_queue_buffer(&stream->queue, buf);
}

static int tccvin_ioctl_expbuf(struct file *file, void *fh,
			       struct v4l2_exportbuffer *exp)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	if (!tccvin_has_privileges(handle))
		return -EBUSY;

	return tccvin_export_buffer(&stream->queue, exp);
}

static int tccvin_ioctl_dqbuf(struct file *file, void *fh,
	struct v4l2_buffer *buf)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	if (!tccvin_has_privileges(handle)) {
		/* device is busy */
		return -EBUSY;
	}

	return tccvin_dequeue_buffer(&stream->queue, buf,
				  file->f_flags & O_NONBLOCK);
}

static int tccvin_ioctl_streamon(struct file *file, void *fh,
	enum v4l2_buf_type type)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;

	if (!tccvin_has_privileges(handle)) {
		/* device is busy */
		return -EBUSY;
	}

	mutex_lock(&stream->mutex);
	ret = tccvin_queue_streamon(&stream->queue, type);
	mutex_unlock(&stream->mutex);

	return ret;
}

static int tccvin_ioctl_streamoff(struct file *file, void *fh,
	enum v4l2_buf_type type)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	if (!tccvin_has_privileges(handle)) {
		/* device is busy */
		return -EBUSY;
	}

	mutex_lock(&stream->mutex);
	tccvin_queue_streamoff(&stream->queue, type);
	mutex_unlock(&stream->mutex);

	return 0;
}

static int tccvin_ioctl_enum_input(struct file *file, void *fh,
	struct v4l2_input *input)
{
	u32 index = input->index;

	if (index != 0) {
		/* index is not 0 */
		return -EINVAL;
	}

	memset(input, 0, sizeof(*input));
	input->index = index;
	strlcpy(input->name, "videosource", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;
	logd("index: %d, name: %s, type: 0x%08x\n",
		input->index, input->name, input->type);

	return 0;
}

static int tccvin_ioctl_g_input(struct file *file, void *fh,
	unsigned int *input)
{
	*input = 0;
	logd("input index: %d\n", *input);

	return 0;
}

static int tccvin_ioctl_g_selection(struct file *file, void *fh,
	struct v4l2_selection *s)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	memset(s, 0, sizeof(*s));
	*s = stream->selection;

	mutex_lock(&stream->mutex);
	s->r.width = stream->cur_frame->wWidth;
	s->r.height = stream->cur_frame->wHeight;
	mutex_unlock(&stream->mutex);

	return 0;
}

static int tccvin_ioctl_s_selection(struct file *file, void *fh,
	struct v4l2_selection *s)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	stream->selection = *s;

	return 0;
}

static int tccvin_ioctl_g_parm(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	struct v4l2_fract *fract = NULL;
	int ret = 0;

	memset(a, 0, sizeof(*a));
	a->type = stream->type;
	switch (a->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		fract = &(a->parm.capture.timeperframe);
		fract->numerator	= 1;
		fract->denominator	=
			stream->cur_frame->dwDefaultFrameInterval;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int tccvin_ioctl_s_parm(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret = 0;

	switch (stream->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Does the video-input path support this framerate? */
		/* Do all the video sources support this framerate? */
		stream->cur_frame->dwDefaultFrameInterval =
			tccvin_try_frame_interval(stream->cur_frame,
				a->parm.capture.timeperframe.denominator);
		logd("numerator: %d, denominator: %d\n",
			1, stream->cur_frame->dwDefaultFrameInterval);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int tccvin_ioctl_enum_framesizes(struct file *file, void *fh,
	struct v4l2_frmsizeenum *fsize)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	struct tccvin_format *format = NULL;
	struct tccvin_frame *frame;
	int i;

	/* Look for the given pixel format */
	for (i = 0; i < stream->nformats; i++) {
		if (stream->format[i].fcc == fsize->pixel_format) {
			format = &stream->format[i];
			break;
		}
	}
	if (format == NULL) {
		/* format is NULL */
		return -EINVAL;
	}

	if (fsize->index >= format->nframes) {
		/* index is wrong */
		return -EINVAL;
	}

	frame = &format->frame[fsize->index];
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = frame->wWidth;
	fsize->discrete.height = frame->wHeight;
	return 0;
}

static int tccvin_ioctl_enum_frameintervals(struct file *file, void *fh,
	struct v4l2_frmivalenum *fival)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	struct tccvin_format *format = NULL;
	struct tccvin_frame *frame = NULL;
	int i;

	/* Look for the given pixel format and frame size */
	for (i = 0; i < stream->nformats; i++) {
		if (stream->format[i].fcc == fival->pixel_format) {
			format = &stream->format[i];
			break;
		}
	}
	if (format == NULL) {
		/* frame is NULL */
		return -EINVAL;
	}

	for (i = 0; i < format->nframes; i++) {
		if (format->frame[i].wWidth == fival->width &&
		    format->frame[i].wHeight == fival->height) {
			frame = &format->frame[i];
			break;
		}
	}
	if (frame == NULL) {
		/* frame is NULL */
		return -EINVAL;
	}

	if (frame->bFrameIntervalType) {
		if (fival->index >= frame->bFrameIntervalType) {
			/* interval index is wrong */
			return -EINVAL;
		}

		fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
		fival->discrete.numerator = 1;
		fival->discrete.denominator =
			frame->dwFrameInterval[fival->index];
		logd("index: %d, pixel_format: 0x%08x, width: %d, height: %d\n",
			fival->index, fival->pixel_format,
			fival->width, fival->height);
		logd("numerator: %d, denominator: %d\n",
			fival->discrete.numerator, fival->discrete.denominator);
	}

	return 0;
}

static long tccvin_ioctl_default(struct file *file, void *fh, bool valid_prio,
	unsigned int cmd, void *arg)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;

	switch (cmd) {
	case VIDIOC_CHECK_PATH_STATUS:
		tccvin_check_path_status(stream, (int *)arg);
		return 0;

	case VIDIOC_S_HANDOVER:
		return tccvin_s_handover(stream, (int *)arg);

	default:
		return -ENOTTY;
	}
}


static int tccvin_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct tccvin_fh *handle = file->private_data;
	struct tccvin_streaming *stream = handle->stream;

	return tccvin_queue_mmap(&stream->queue, vma);
}

static unsigned int tccvin_v4l2_poll(struct file *file, poll_table *wait)
{
	struct tccvin_fh *handle = file->private_data;
	struct tccvin_streaming *stream = handle->stream;
	int		ret;

	ret = tccvin_queue_poll(&stream->queue, file, wait);
	if (ret < 0) {
		logw("_qproc: %p, _key: 0x%08lx, ret: %d\n",
			wait->_qproc, wait->_key, ret);
	}

	return ret;
}

const struct v4l2_ioctl_ops tccvin_ioctl_ops = {
	.vidioc_querycap		= tccvin_ioctl_querycap,
	.vidioc_enum_fmt_vid_cap	= tccvin_ioctl_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= tccvin_ioctl_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= tccvin_ioctl_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= tccvin_ioctl_try_fmt_vid_cap,
	.vidioc_reqbufs			= tccvin_ioctl_reqbufs,
	.vidioc_querybuf		= tccvin_ioctl_querybuf,
	.vidioc_qbuf			= tccvin_ioctl_qbuf,
	.vidioc_expbuf			= tccvin_ioctl_expbuf,
	.vidioc_dqbuf			= tccvin_ioctl_dqbuf,
	.vidioc_streamon		= tccvin_ioctl_streamon,
	.vidioc_streamoff		= tccvin_ioctl_streamoff,
	.vidioc_enum_input		= tccvin_ioctl_enum_input,
	.vidioc_g_input			= tccvin_ioctl_g_input,
	.vidioc_g_selection		= tccvin_ioctl_g_selection,
	.vidioc_s_selection		= tccvin_ioctl_s_selection,
	.vidioc_g_parm			= tccvin_ioctl_g_parm,
	.vidioc_s_parm			= tccvin_ioctl_s_parm,
	.vidioc_enum_framesizes		= tccvin_ioctl_enum_framesizes,
	.vidioc_enum_frameintervals	= tccvin_ioctl_enum_frameintervals,
	.vidioc_default			= tccvin_ioctl_default,
};

const struct v4l2_file_operations tccvin_fops = {
	.owner		= THIS_MODULE,
	.open		= tccvin_v4l2_open,
	.release	= tccvin_v4l2_release,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= tccvin_v4l2_mmap,
	.poll		= tccvin_v4l2_poll,
};

