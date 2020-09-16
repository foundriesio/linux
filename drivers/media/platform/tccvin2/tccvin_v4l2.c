/*******************************************************************************
 *      tccvin_v4l2.c  --  v4l2 interface
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
static __u32 tccvin_try_frame_interval(struct tccvin_frame *frame, __u32 interval)
{
	unsigned int i;

	if (frame->bFrameIntervalType) {
		__u32 best = -1, dist;

		for (i = 0; i < frame->bFrameIntervalType; ++i) {
			dist = interval > frame->dwFrameInterval[i]
			     ? interval - frame->dwFrameInterval[i]
			     : frame->dwFrameInterval[i] - interval;

			if (dist > best)
				break;

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
	struct v4l2_format *fmt,
	struct tccvin_format **tccvin_format, struct tccvin_frame **tccvin_frame)
{
	struct tccvin_format *format = NULL;
	struct tccvin_frame *frame = NULL;
	__u16 rw, rh;
	unsigned int d, maxd;
	unsigned int i;
	__u32 interval;
	int ret = 0;
	__u8 *fcc;
	FUNCTION_IN

	if (fmt->type != stream->type) {
		loge("fmt->type: 0x%08x, stream->type: 0x%08x\n", fmt->type, stream->type);
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
		if (format->fcc == fmt->fmt.pix.pixelformat)
			break;
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

		if (maxd == 0)
			break;
	}

	if (frame == NULL) {
		loge("Unsupported size %ux%u.\n",
				fmt->fmt.pix.width, fmt->fmt.pix.height);
		return -EINVAL;
	}

	/* Use the default frame interval. */
	interval = frame->dwDefaultFrameInterval;
	logd("Using default frame interval %u.%u us "
		"(%u.%u fps).\n", interval/10, interval%10, 10000000/interval,
		(100000000/interval)%10);

	/* Set the format index, frame index and frame interval. */
	tccvin_try_frame_interval(frame, interval);

	fmt->fmt.pix.width = frame->wWidth;
	fmt->fmt.pix.height = frame->wHeight;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.bytesperline = tccvin_v4l2_get_bytesperline(format, frame);
	frame->dwMaxVideoFrameBufferSize = frame->wWidth * frame->wHeight *  format->bpp / 8;
	fmt->fmt.pix.sizeimage = frame->dwMaxVideoFrameBufferSize;
	fmt->fmt.pix.colorspace = format->colorspace;
	fmt->fmt.pix.priv = 0;
	logd("width: %d, height: %d, field: 0x%08x, bytesperline: 0x%08x, sizeimage: 0x%08x, colorspace: 0x%08x\n", \
		fmt->fmt.pix.width, fmt->fmt.pix.height, fmt->fmt.pix.field, fmt->fmt.pix.bytesperline, fmt->fmt.pix.sizeimage, fmt->fmt.pix.colorspace);

	if (tccvin_format != NULL)
		*tccvin_format = format;
	if (tccvin_frame != NULL)
		*tccvin_frame = frame;

	FUNCTION_OUT
	return ret;
}

static int tccvin_v4l2_get_format(struct tccvin_streaming *stream,
	struct v4l2_format *fmt)
{
	struct tccvin_format *format;
	struct tccvin_frame *frame;
	int ret = 0;

	if (fmt->type != stream->type)
		return -EINVAL;

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
	fmt->fmt.pix.bytesperline = tccvin_v4l2_get_bytesperline(format, frame);
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
	FUNCTION_IN

	if (fmt->type != stream->type) {
		loge("fmt->type: 0x%08x, stream->type: 0x%08x\n", fmt->type, stream->type);
		return -EINVAL;
	}

	ret = tccvin_v4l2_try_format(stream, fmt, &format, &frame);
	if (ret < 0) {
		loge("tccvin_v4l2_try_format, ret: %d\n", ret);
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

	FUNCTION_OUT
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
	if (handle->state == TCCVIN_HANDLE_ACTIVE)
		return 0;

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
	if (handle->state == TCCVIN_HANDLE_ACTIVE)
		atomic_dec(&handle->stream->active);

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
	FUNCTION_IN

	stream = video_drvdata(file);

	/* Create the device handle. */
	handle = kzalloc(sizeof *handle, GFP_KERNEL);
	if (handle == NULL) {
		return -ENOMEM;
	}

	mutex_lock(&stream->dev->lock);
	stream->dev->users++;
	mutex_unlock(&stream->dev->lock);

	v4l2_fh_init(&handle->vfh, &stream->vdev);
	v4l2_fh_add(&handle->vfh);
	handle->stream = stream;
	handle->state = TCCVIN_HANDLE_PASSIVE;
	file->private_data = handle;

	FUNCTION_OUT
	return 0;
}

static int tccvin_v4l2_release(struct file *file)
{
	struct tccvin_fh *handle = file->private_data;
	struct tccvin_streaming *stream = handle->stream;
	FUNCTION_IN

	/* Only free resources if this is a privileged handle. */
	if (tccvin_has_privileges(handle))
		tccvin_queue_release(&stream->queue);

	/* Release the file handle. */
	tccvin_dismiss_privileges(handle);
	v4l2_fh_del(&handle->vfh);
	v4l2_fh_exit(&handle->vfh);
	kfree(handle);
	file->private_data = NULL;

	mutex_lock(&stream->dev->lock);
	--stream->dev->users;
	mutex_unlock(&stream->dev->lock);

	FUNCTION_OUT
	return 0;
}

static int tccvin_ioctl_querycap(struct file *file, void *fh,
			      struct v4l2_capability *cap)
{
	struct video_device *vdev = video_devdata(file);
	FUNCTION_IN

	strlcpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
	strlcpy(cap->card, vdev->name, sizeof(cap->card));
	logd("num: %d\n", vdev->num);		// !@#
	logd("index: %d\n", vdev->index);	// !@#
	snprintf(cap->bus_info, sizeof(cap->bus_info), "video-capture-%s", cap->driver);
	cap->version = KERNEL_VERSION(4, 14, 00);
	cap->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE;
	cap->capabilities = V4L2_CAP_DEVICE_CAPS | cap->device_caps;

	logd("driver: %s\n", cap->driver);
	logd("card: %s\n", cap->card);
	logd("bus_info: %s\n", cap->bus_info);
	logd("version: %u.%u.%u\n", (cap->version >> 16) & 0xFF, (cap->version >> 8) & 0xFF, cap->version & 0xFF);
	logd("device_caps: 0x%08x\n", cap->device_caps);
	logd("capabilities: 0x%08x\n", cap->capabilities);

	FUNCTION_OUT
	return 0;
}

static int tccvin_ioctl_enum_fmt(struct tccvin_streaming *stream,
			      struct v4l2_fmtdesc *fmt)
{
	struct tccvin_format *format;
	enum v4l2_buf_type type = fmt->type;
	__u32 index = fmt->index;
	FUNCTION_IN

	if (fmt->type != stream->type || fmt->index >= stream->nformats)
		return -EINVAL;

	memset(fmt, 0, sizeof(*fmt));
	fmt->index = index;
	fmt->type = type;

	format = &stream->format[fmt->index];
	fmt->flags = 0;
	strlcpy(fmt->description, format->name, sizeof(fmt->description));
	fmt->description[sizeof(fmt->description) - 1] = 0;
	fmt->pixelformat = format->fcc;
	logd("index: %d, type: 0x%08x, flags: 0x%08x, description: %s, pixelformat: 0x%08x\n", fmt->index, fmt->type, fmt->flags, fmt->description, fmt->pixelformat);

	FUNCTION_OUT
	return 0;
}

static int tccvin_ioctl_enum_fmt_vid_cap(struct file *file, void *fh,
				      struct v4l2_fmtdesc *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int		ret = 0;
	FUNCTION_IN

	ret = tccvin_ioctl_enum_fmt(stream, fmt);

	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_g_fmt_vid_cap(struct file *file, void *fh,
				   struct v4l2_format *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int	ret;
	FUNCTION_IN

	ret = tccvin_v4l2_get_format(stream, fmt);
	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_s_fmt_vid_cap(struct file *file, void *fh,
				   struct v4l2_format *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;
	FUNCTION_IN

	ret = tccvin_acquire_privileges(handle);
	if (ret < 0) {
		loge("tccvin_acquire_privileges, ret: %d\n", ret);
		return ret;
	}

	ret = tccvin_v4l2_set_format(stream, fmt);
	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_try_fmt_vid_cap(struct file *file, void *fh,
				     struct v4l2_format *fmt)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int	ret;
	FUNCTION_IN

	ret = tccvin_v4l2_try_format(stream, fmt, NULL, NULL);
	if (ret < 0)
		loge("tccvin_v4l2_try_format, ret: %d\n", ret);

	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_reqbufs(struct file *file, void *fh,
			     struct v4l2_requestbuffers *rb)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;
	FUNCTION_IN

	logd("count: %d, type: 0x%08x, memory: 0x%08x, res[1]: 0x%08x, res[0]: 0x%08x\n", rb->count, rb->type, rb->memory, rb->reserved[1], rb->reserved[0]);

	ret = tccvin_acquire_privileges(handle);
	if (ret < 0) {
		loge("tccvin_acquire_privileges, ret: %d\n", ret);
		return ret;
	}

	mutex_lock(&stream->mutex);
	ret = tccvin_request_buffers(&stream->queue, rb);
	mutex_unlock(&stream->mutex);
	if (ret < 0) {
		loge("tccvin_request_buffers, ret: %d\n", ret);
		return ret;
	}

	if (ret == 0)
		tccvin_dismiss_privileges(handle);

	logd("requested count: %d, type: 0x%08x, memory: 0x%08x, res[1]: 0x%08x, res[0]: 0x%08x\n", rb->count, rb->type, rb->memory, rb->reserved[1], rb->reserved[0]);

	FUNCTION_OUT
	return 0;
}

static int tccvin_ioctl_querybuf(struct file *file, void *fh,
			      struct v4l2_buffer *buf)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;
	FUNCTION_IN

	if (!tccvin_has_privileges(handle)) {
		return -EBUSY;
	}

	ret = tccvin_query_buffer(&stream->queue, buf);
	logd("index: %d, type: 0x%08x, bytesused: 0x%08x, flags: 0x%08x, field: 0x%08x, memory: 0x%08x, m.offset: 0x%08x, lengh: 0x%08x\n", \
		buf->index, buf->type, buf->bytesused, buf->flags, buf->field, buf->memory, buf->m.offset, buf->length);

	if (buf->memory == V4L2_MEMORY_MMAP) {
		struct vb2_queue* q = &stream->queue.queue;
		q->bufs[buf->index]->planes[0].m.offset = stream->cif.pmap_preview.base + buf->m.offset;
		buf->m.offset = stream->cif.pmap_preview.base + buf->m.offset;
	} else {
		/*
		 * TODO: Need to implement to handle other memory
		 * types. Because of the limit of memory address
		 * between virtual addresses and physical address in
		 * 64-bit environment, we need to use IOMMU to handle
		 * this issue.
		 */
		logd("%s - Need to be implemented\n", __func__);
	}

	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;
//	FUNCTION_IN

	if (!tccvin_has_privileges(handle)) {
		loge("!tccvin_has_privileges\n");
		return -EBUSY;
	}

	if(buf != NULL)
		dlog("&stream->queue: %p, buf[%d].flags: 0x%08x\n", &stream->queue, buf->index, buf->flags);

	ret = tccvin_queue_buffer(&stream->queue, buf);

	dlog("tccvin_queue_buffer, ret: %d\n", ret);

//	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;
//	FUNCTION_IN

	if (!tccvin_has_privileges(handle))
		return -EBUSY;

	ret = tccvin_dequeue_buffer(&stream->queue, buf,
				  file->f_flags & O_NONBLOCK);

	if(buf != NULL)
		dlog("&stream->queue: %p, buf[%d].flags: 0x%08x, file->f_flags: 0x%08x, O_NONBLOCK: 0x%08x, ret: %d\n", &stream->queue, buf->index, buf->flags, file->f_flags, O_NONBLOCK, ret);

	dlog("tccvin_dqueue_buffer, ret: %d\n", ret);

//	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_streamon(struct file *file, void *fh,
			      enum v4l2_buf_type type)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;
	FUNCTION_IN

	if (!tccvin_has_privileges(handle))
		return -EBUSY;

	mutex_lock(&stream->mutex);
	ret = tccvin_queue_streamon(&stream->queue, type);
	if (ret < 0)
		loge("tccvin_queue_streamon, v4l2_buf_type: %d, ret: %d\n", type, ret);
	mutex_unlock(&stream->mutex);

	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_streamoff(struct file *file, void *fh,
			       enum v4l2_buf_type type)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	int ret;
	FUNCTION_IN

	if (!tccvin_has_privileges(handle))
		return -EBUSY;

	mutex_lock(&stream->mutex);
	ret = tccvin_queue_streamoff(&stream->queue, type);
	if (ret < 0)
		loge("tccvin_queue_streamoff, queue->streaming: %d, ret: %d\n", stream->queue.queue.streaming, ret);
	mutex_unlock(&stream->mutex);

	FUNCTION_OUT
	return ret;
}

static int tccvin_ioctl_enum_input(struct file *file, void *fh,
				struct v4l2_input *input)
{
	u32 index = input->index;
	FUNCTION_IN

	if(index != 0)
		return -EINVAL;

	memset(input, 0, sizeof(*input));
	input->index = index;
	strlcpy(input->name, "videosource", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;
	logd("index: %d, name: %s, type: 0x%08x\n", input->index, input->name, input->type);

	FUNCTION_OUT
	return 0;
}

static int tccvin_ioctl_g_input(struct file *file, void *fh, unsigned int *input)
{
	FUNCTION_IN

	*input = 0;
	logd("input index: %d\n", *input);

	FUNCTION_OUT
	return 0;
}

static int tccvin_ioctl_enum_framesizes(struct file *file, void *fh,
				     struct v4l2_frmsizeenum *fsize)
{
	struct tccvin_fh *handle = fh;
	struct tccvin_streaming *stream = handle->stream;
	struct tccvin_format *format = NULL;
	struct tccvin_frame *frame;
	int i;
	FUNCTION_IN

	/* Look for the given pixel format */
	for (i = 0; i < stream->nformats; i++) {
		if (stream->format[i].fcc == fsize->pixel_format) {
			format = &stream->format[i];
			break;
		}
	}
	if (format == NULL) {
		loge("There is no supported format.\n");
		return -EINVAL;
	}

	if (fsize->index >= format->nframes)
		return -EINVAL;

	frame = &format->frame[fsize->index];
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = frame->wWidth;
	fsize->discrete.height = frame->wHeight;
	logd("index: %d, pixel_format: 0x%08x, type: 0x%08x, width: %d, height: %d\n", fsize->index, fsize->pixel_format, fsize->type, fsize->discrete.width, fsize->discrete.height);

	FUNCTION_OUT
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
	FUNCTION_IN

	/* Look for the given pixel format and frame size */
	for (i = 0; i < stream->nformats; i++) {
		if (stream->format[i].fcc == fival->pixel_format) {
			format = &stream->format[i];
			break;
		}
	}
	if (format == NULL) {
		loge("There is no supported format.\n");
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
		loge("There is no supported frame.\n");
		return -EINVAL;
	}

	if (frame->bFrameIntervalType) {
		if (fival->index >= frame->bFrameIntervalType)
			return -EINVAL;

		fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
		fival->discrete.numerator =
			frame->dwFrameInterval[fival->index];
		fival->discrete.denominator = 10000000;
		tccvin_simplify_fraction(&fival->discrete.numerator,
			&fival->discrete.denominator, 8, 333);
		logd("index: %d, pixel_format: 0x%08x, width: %d, height: %d, type: %d, frameinterval: %d\n", \
			fival->index, fival->pixel_format, fival->width, fival->height, fival->type, fival->discrete.denominator);
	}

	FUNCTION_OUT
	return 0;
}

static int tccvin_v4l2_mmap(struct file *file, struct vm_area_struct *vma) {
	struct tccvin_fh *handle = file->private_data;
	struct tccvin_streaming *stream = handle->stream;
	int	ret = 0;
	FUNCTION_IN

	ret = tccvin_queue_mmap(&stream->queue, vma);
	logd("start: 0x%08lx, end: 0x%08lx, off: 0x%08lx\n", vma->vm_start, vma->vm_end, vma->vm_pgoff);

	FUNCTION_OUT
	return ret;
}

static unsigned int tccvin_v4l2_poll(struct file *file, poll_table *wait)
{
	struct tccvin_fh *handle = file->private_data;
	struct tccvin_streaming *stream = handle->stream;
	int		ret;
//	FUNCTION_IN

	ret = tccvin_queue_poll(&stream->queue, file, wait);
	if (ret < 0)
		logw("_qproc: %p, _key: 0x%08lx, ret: %d\n", wait->_qproc, wait->_key, ret);

//	FUNCTION_OUT
	return ret;
}

const struct v4l2_ioctl_ops tccvin_ioctl_ops = {
	/* VIDIOC_QUERYCAP handler */
	.vidioc_querycap				= tccvin_ioctl_querycap,

	/* VIDIOC_ENUM_FMT handlers */
	.vidioc_enum_fmt_vid_cap		= tccvin_ioctl_enum_fmt_vid_cap,

	/* VIDIOC_G_FMT handlers */
	.vidioc_g_fmt_vid_cap			= tccvin_ioctl_g_fmt_vid_cap,

	/* VIDIOC_S_FMT handlers */
	.vidioc_s_fmt_vid_cap			= tccvin_ioctl_s_fmt_vid_cap,

	/* VIDIOC_TRY_FMT handlers */
	.vidioc_try_fmt_vid_cap			= tccvin_ioctl_try_fmt_vid_cap,

	/* Buffer handlers */
	.vidioc_reqbufs					= tccvin_ioctl_reqbufs,
	.vidioc_querybuf				= tccvin_ioctl_querybuf,
	.vidioc_qbuf					= tccvin_ioctl_qbuf,
	.vidioc_dqbuf					= tccvin_ioctl_dqbuf,

	/* Stream on/off */
	.vidioc_streamon				= tccvin_ioctl_streamon,
	.vidioc_streamoff				= tccvin_ioctl_streamoff,

	/* Input handling */
	.vidioc_enum_input				= tccvin_ioctl_enum_input,
	.vidioc_g_input					= tccvin_ioctl_g_input,

	.vidioc_enum_framesizes			= tccvin_ioctl_enum_framesizes,
	.vidioc_enum_frameintervals		= tccvin_ioctl_enum_frameintervals,
};

const struct v4l2_file_operations tccvin_fops = {
	.owner			= THIS_MODULE,
	.open			= tccvin_v4l2_open,
	.release		= tccvin_v4l2_release,
	.unlocked_ioctl	= video_ioctl2,
	.mmap			= tccvin_v4l2_mmap,
	.poll			= tccvin_v4l2_poll,
};

