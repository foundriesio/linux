/*
 *      tccvin_queue.c  --  Buffers management
 *
 *      Copyright (c) 2020-
 *          Telechips Inc.
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      SPDX-license-Identifier : GPL-2.0+
 *
 */

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>

#include "tccvin_video.h"

/* ------------------------------------------------------------------------
 * Video buffers queue management.
 *
 * Video queues is initialized by tccvin_queue_init(). The function performs
 * basic initialization of the tccvin_video_queue struct and never fails.
 *
 * Video buffers are managed by videobuf2. The driver uses a mutex to protect
 * the videobuf2 queue operations by serializing calls to videobuf2 and a
 * spinlock to protect the IRQ queue that holds the buffers to be processed by
 * the driver.
 */

static inline struct tccvin_streaming *
tccvin_queue_to_stream(struct tccvin_video_queue *queue)
{
	return container_of(queue, struct tccvin_streaming, queue);
}

static inline struct tccvin_buffer *tccvin_vbuf_to_buffer(
	struct vb2_v4l2_buffer *buf)
{
	return container_of(buf, struct tccvin_buffer, buf);
}

/*
 * Return all queued buffers to videobuf2 in the requested state.
 *
 * This function must be called with the queue spinlock held.
 */
static void tccvin_queue_return_buffers(struct tccvin_video_queue *queue,
			       enum tccvin_buffer_state state)
{
	enum vb2_buffer_state vb2_state = state == TCCVIN_BUF_STATE_ERROR
					? VB2_BUF_STATE_ERROR
					: VB2_BUF_STATE_QUEUED;

	while (!list_empty(&queue->irqqueue)) {
		struct tccvin_buffer *buf = list_first_entry(&queue->irqqueue,
							  struct tccvin_buffer,
							  queue);
		list_del(&buf->queue);
		buf->state = state;
		vb2_buffer_done(&buf->buf.vb2_buf, vb2_state);
	}
}

/* -----------------------------------------------------------------------------
 * videobuf2 queue operations
 */

static int tccvin_queue_setup(struct vb2_queue *vq,
			   unsigned int *nbuffers, unsigned int *nplanes,
			   unsigned int sizes[], struct device *alloc_devs[])
{
	struct tccvin_video_queue *queue = vb2_get_drv_priv(vq);
	struct tccvin_streaming *stream = tccvin_queue_to_stream(queue);
	unsigned int size = stream->cur_frame->dwMaxVideoFrameBufferSize;

	/*
	 * When called with plane sizes, validate them. The driver supports
	 * single planar formats only, and requires buffers to be large enough
	 * to store a complete frame.
	 */
	if (*nplanes) {
		return *nplanes != 1 || sizes[0] < size ? -EINVAL : 0;
	}

	*nplanes = 1;
	sizes[0] = size;

	return 0;
}

static int tccvin_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct tccvin_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct tccvin_buffer *buf = tccvin_vbuf_to_buffer(vbuf);

	if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT &&
	    vb2_get_plane_payload(vb, 0) > vb2_plane_size(vb, 0)) {
		loge("Bytes used out of bounds.\n");
		return -EINVAL;
	}

	if (unlikely(queue->flags & TCCVIN_QUEUE_DISCONNECTED)) {
		return -ENODEV;
	}

	buf->state = TCCVIN_BUF_STATE_QUEUED;
	buf->error = 0;
	buf->mem = vb2_plane_vaddr(vb, 0);
	buf->length = vb2_plane_size(vb, 0);
	if (vb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		buf->bytesused = 0;
	} else {
		buf->bytesused = vb2_get_plane_payload(vb, 0);
	}

	return 0;
}

static void tccvin_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct tccvin_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct tccvin_buffer *buf = tccvin_vbuf_to_buffer(vbuf);
	unsigned long flags;

	spin_lock_irqsave(&queue->irqlock, flags);
	if (likely(!(queue->flags & TCCVIN_QUEUE_DISCONNECTED))) {
		list_add_tail(&buf->queue, &queue->irqqueue);
	} else {
		/* If the device is disconnected return the buffer to userspace
		 * directly. The next QBUF call will fail with -ENODEV.
		 */
		buf->state = TCCVIN_BUF_STATE_ERROR;
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
	}

	spin_unlock_irqrestore(&queue->irqlock, flags);
}

static int tccvin_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct tccvin_video_queue *queue = vb2_get_drv_priv(vq);
	struct tccvin_streaming *stream = tccvin_queue_to_stream(queue);
	unsigned long flags;
	int ret;

	queue->buf_used = 0;

	ret = tccvin_video_streamon(stream, 0);
	if (ret == 0) {
		return 0;
	}

	spin_lock_irqsave(&queue->irqlock, flags);
	tccvin_queue_return_buffers(queue, TCCVIN_BUF_STATE_QUEUED);
	spin_unlock_irqrestore(&queue->irqlock, flags);

	return ret;
}

static void tccvin_stop_streaming(struct vb2_queue *vq)
{
	struct tccvin_video_queue *queue = vb2_get_drv_priv(vq);
	struct tccvin_streaming *stream = tccvin_queue_to_stream(queue);
	unsigned long flags;

	spin_lock_irqsave(&queue->irqlock, flags);
	tccvin_queue_return_buffers(queue, TCCVIN_BUF_STATE_ERROR);
	spin_unlock_irqrestore(&queue->irqlock, flags);

	tccvin_video_streamoff(stream, 0);
}

static const struct vb2_ops tccvin_queue_qops = {
	.queue_setup = tccvin_queue_setup,
	.buf_prepare = tccvin_buffer_prepare,
	.buf_queue = tccvin_buffer_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = tccvin_start_streaming,
	.stop_streaming = tccvin_stop_streaming,
};

int tccvin_queue_init(struct tccvin_video_queue *queue, enum v4l2_buf_type type,
		    int drop_corrupted)
{
	struct tccvin_streaming *stream = tccvin_queue_to_stream(queue);
	int ret;

	queue->queue.type = type;
	queue->queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	queue->queue.bidirectional = 0;
	queue->queue.is_output = DMA_TO_DEVICE;
	queue->queue.drv_priv = queue;
	queue->queue.buf_struct_size = sizeof(struct tccvin_buffer);
	queue->queue.ops = &tccvin_queue_qops;
	queue->queue.mem_ops = &vb2_vmalloc_memops;
	queue->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC
		| V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	queue->queue.lock = &queue->mutex;
	queue->queue.dev = &stream->dev->pdev->dev;
	ret = vb2_queue_init(&queue->queue);
	if (ret) {
		return ret;
	}

	mutex_init(&queue->mutex);
	spin_lock_init(&queue->irqlock);
	INIT_LIST_HEAD(&queue->irqqueue);
	queue->flags = drop_corrupted ? TCCVIN_QUEUE_DROP_CORRUPTED : 0;
	logd("drop_corrupted: %d, queue->flags: 0x%08x",
		drop_corrupted, queue->flags);

	return 0;
}

void tccvin_queue_release(struct tccvin_video_queue *queue)
{
	mutex_lock(&queue->mutex);
	vb2_queue_release(&queue->queue);
	mutex_unlock(&queue->mutex);
}

/* -----------------------------------------------------------------------------
 * V4L2 queue operations
 */

int tccvin_request_buffers(struct tccvin_video_queue *queue,
			struct v4l2_requestbuffers *rb)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_reqbufs(&queue->queue, rb);
	mutex_unlock(&queue->mutex);

	return ret ? ret : rb->count;
}

int tccvin_query_buffer(struct tccvin_video_queue *queue,
	struct v4l2_buffer *buf)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_querybuf(&queue->queue, buf);
	mutex_unlock(&queue->mutex);

	return ret;
}

int tccvin_queue_buffer(struct tccvin_video_queue *queue,
	struct v4l2_buffer *buf)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_qbuf(&queue->queue, buf);
	mutex_unlock(&queue->mutex);

	return ret;
}

int tccvin_export_buffer(struct tccvin_video_queue *queue,
		      struct v4l2_exportbuffer *exp)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_expbuf(&queue->queue, exp);
	mutex_unlock(&queue->mutex);

	return ret;
}

int tccvin_dequeue_buffer(struct tccvin_video_queue *queue,
	struct v4l2_buffer *buf, int nonblocking)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_dqbuf(&queue->queue, buf, nonblocking);
	mutex_unlock(&queue->mutex);

	return ret;
}

int tccvin_queue_streamon(struct tccvin_video_queue *queue,
	enum v4l2_buf_type type)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_streamon(&queue->queue, type);
	mutex_unlock(&queue->mutex);

	return ret;
}

int tccvin_queue_streamoff(struct tccvin_video_queue *queue,
	enum v4l2_buf_type type)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_streamoff(&queue->queue, type);
	mutex_unlock(&queue->mutex);

	return ret;
}

int tccvin_queue_mmap(struct tccvin_video_queue *queue,
	struct vm_area_struct *vma)
{
	return vb2_mmap(&queue->queue, vma);
}

unsigned int tccvin_queue_poll(struct tccvin_video_queue *queue,
	struct file *file, poll_table *wait)
{
	unsigned int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_poll(&queue->queue, file, wait);
	mutex_unlock(&queue->mutex);

	return ret;
}

/* -----------------------------------------------------------------------------
 *
 */

/*
 * Check if buffers have been allocated.
 */
int tccvin_queue_is_allocated(struct tccvin_video_queue *queue)
{
	int allocated;

	mutex_lock(&queue->mutex);
	allocated = vb2_is_busy(&queue->queue);
	mutex_unlock(&queue->mutex);

	return allocated;
}

struct tccvin_buffer *tccvin_queue_next_buffer(struct tccvin_video_queue *queue,
					       struct tccvin_buffer *buf)
{
	struct tccvin_buffer *nextbuf;
	unsigned long flags;

	if ((queue->flags & TCCVIN_QUEUE_DROP_CORRUPTED) && buf->error) {
		buf->error = 0;
		buf->state = TCCVIN_BUF_STATE_QUEUED;
		buf->bytesused = 0;
		vb2_set_plane_payload(&buf->buf.vb2_buf, 0, 0);
		return buf;
	}

	spin_lock_irqsave(&queue->irqlock, flags);
	list_del(&buf->queue);
	if (!list_empty(&queue->irqqueue)) {
		nextbuf = list_first_entry(&queue->irqqueue,
			struct tccvin_buffer, queue);
		dlog("buf[%d], type: 0x%08x, memory: 0x%08x\n",
			nextbuf->buf.vb2_buf.index, nextbuf->buf.vb2_buf.type,
			nextbuf->buf.vb2_buf.memory);
	} else {
		nextbuf = NULL;
		logd("nextbuf == NULL\n");
	}
	spin_unlock_irqrestore(&queue->irqlock, flags);

	buf->state = buf->error ?
		TCCVIN_BUF_STATE_ERROR : TCCVIN_BUF_STATE_DONE;
	vb2_set_plane_payload(&buf->buf.vb2_buf, 0, buf->bytesused);
	vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_DONE);

	return nextbuf;
}
