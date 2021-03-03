/*
 * drivers/staging/android/ion/ion_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Memory allocation - DMABUF


 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/genalloc.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>

#include "tccvin_video.h"

/* #define TCCVIN_DMABUF_IOC_MAGIC		'D' */

struct tccvin_dmabuf_buffer {
	struct tccvin_dmabuf_heap *heap;
	struct sg_table *sgt;
	unsigned long flags;
	size_t size;
	struct mutex lock;
	int kmap_cnt;
	void *vaddr;
	struct list_head attachments;
};

struct tccvin_dmabuf_dmabuf_attachment {
	struct device *dev;
	struct sg_table sgt;
	struct list_head list;
	bool mapped;
};

struct tccvin_dmabuf_device {
	struct device *dev;
	struct miscdevice mdev;
	struct tccvin_dmabuf_heap *heap;
	struct rw_semaphore lock;
};

static inline struct tccvin_dmabuf_device *to_tccvin_dmabuf_device(
	struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;

	return container_of(miscdev, struct tccvin_dmabuf_device, mdev);
}

struct tccvin_dmabuf_heap *tccvin_dmabuf_heap_create(
	struct tccvin_streaming *stream)
{
	struct tccvin_dmabuf_heap *heap;
	int ret;

	heap = devm_kzalloc(&stream->dev->pdev->dev,
			    sizeof(struct tccvin_dmabuf_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);

	heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!heap->pool)
		return ERR_PTR(-ENOMEM);

	heap->base = stream->cif.pmap_preview.base;
	heap->size = stream->cif.pmap_preview.size;
	heap->dev = &stream->dev->pdev->dev;

	ret = gen_pool_add(heap->pool, heap->base, heap->size, -1);
	if (ret)
		return ERR_PTR(ret);

	return heap;
}

int tccvin_dmabuf_heap_alloc(struct tccvin_dmabuf_heap *heap,
			  struct tccvin_dmabuf_buffer *buffer,
			  size_t size, unsigned long flags)
{
	struct sg_table *sgt;
	phys_addr_t paddr;
	int ret;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret)
		goto err_kfree;

	paddr = gen_pool_alloc(heap->pool, size);
	if (unlikely(!paddr)) {
		ret = -ENOMEM;
		goto err_free_sgt;
	}

	sg_set_page(sgt->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->heap = heap;
	buffer->sgt = sgt;
	buffer->size = size;
	buffer->flags = flags;

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);

	logd("0x%08zx @ 0x%pa (0x%08zx avail)\n",
	     buffer->size, &paddr, gen_pool_avail(heap->pool));

	return 0;

err_free_sgt:
	sg_free_table(sgt);
err_kfree:
	kfree(sgt);
	return ret;
}

void tccvin_dmabuf_heap_free(struct tccvin_dmabuf_buffer *buffer)
{
	struct tccvin_dmabuf_heap *heap = buffer->heap;
	struct sg_table *sgt = buffer->sgt;
	struct scatterlist *sg = sgt->sgl;
	phys_addr_t paddr = page_to_phys(sg_page(sg));

	logd("0x%08zx @ 0x%pa\n", buffer->size, &paddr);

	gen_pool_free(heap->pool, paddr, buffer->size);
	sg_free_table(sgt);
	kfree(sgt);
}

static int tccvin_dmabuf_dmabuf_attach(struct dma_buf *dmabuf,
					struct device *dev,
					struct dma_buf_attachment *attachment)
{
	struct tccvin_dmabuf_buffer *buffer = dmabuf->priv;
	struct tccvin_dmabuf_dmabuf_attachment *attach;
	struct sg_table *sgt = buffer->sgt;
	struct sg_table *new_sgt;
	struct scatterlist *sg, *new_sg;
	int i;
	int ret;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	new_sgt = &attach->sgt;

	ret = sg_alloc_table(new_sgt, sgt->nents, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}

	new_sg = new_sgt->sgl;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		new_sg = sg_next(new_sg);
	}

	attach->dev = dev;
	INIT_LIST_HEAD(&attach->list);
	attach->mapped = false;

	attachment->priv = attach;

	mutex_lock(&buffer->lock);
	list_add(&attach->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void tccvin_dmabuf_dmabuf_detach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct tccvin_dmabuf_dmabuf_attachment *attach = attachment->priv;
	struct tccvin_dmabuf_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	list_del(&attach->list);
	mutex_unlock(&buffer->lock);
	sg_free_table(&attach->sgt);
	kfree(attach);
}

static void *tccvin_dmabuf_heap_map_kernel(struct tccvin_dmabuf_heap *heap,
				 struct tccvin_dmabuf_buffer *buffer)
{
	struct scatterlist *sg;
	int i, j;
	void *vaddr;
	pgprot_t pgprot;
	struct sg_table *sgt = buffer->sgt;
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		int npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		BUG_ON(i >= npages);
		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static int tccvin_dmabuf_heap_map_user(struct tccvin_dmabuf_heap *heap,
				    struct tccvin_dmabuf_buffer *buffer,
				    struct vm_area_struct *vma)
{
	struct sg_table *sgt = buffer->sgt;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i;
	int ret;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remain = vma->vm_end - addr;
		unsigned long len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remain);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

void tccvin_dmabuf_heap_unmap_kernel(struct tccvin_dmabuf_heap *heap,
			   struct tccvin_dmabuf_buffer *buffer)
{
	vunmap(buffer->vaddr);
}


static int tccvin_dmabuf_dmabuf_begin_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	struct tccvin_dmabuf_buffer *buffer = dmabuf->priv;
	struct tccvin_dmabuf_dmabuf_attachment *a;

	mutex_lock(&buffer->lock);
	if (!buffer->kmap_cnt) {
		void *vaddr;

		vaddr = tccvin_dmabuf_heap_map_kernel(buffer->heap, buffer);
		if (IS_ERR(vaddr))
			return PTR_ERR(vaddr);
		buffer->vaddr = vaddr;
	}
	buffer->kmap_cnt++;

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sg_for_cpu(a->dev, a->sgt.sgl, a->sgt.nents,
				    direction);
	}
	return 0;
}

static int tccvin_dmabuf_dmabuf_end_cpu_access(struct dma_buf *dmabuf,
				     enum dma_data_direction direction)
{
	struct tccvin_dmabuf_buffer *buffer = dmabuf->priv;
	struct tccvin_dmabuf_dmabuf_attachment *a;

	mutex_lock(&buffer->lock);
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		tccvin_dmabuf_heap_unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sg_for_device(a->dev, a->sgt.sgl, a->sgt.nents,
				       direction);
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int tccvin_dmabuf_mmap(struct dma_buf *dmabuf,
				struct vm_area_struct *vma)
{
	struct tccvin_dmabuf_buffer *buffer = dmabuf->priv;
	struct tccvin_dmabuf_heap *heap = buffer->heap;
	int ret;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	ret = tccvin_dmabuf_heap_map_user(heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		loge("Failed to map buffer to userspace (%d)\n", ret);

	return ret;
}

static struct sg_table *tccvin_dmabuf_map_dmabuf(
	struct dma_buf_attachment *attachment,
	enum dma_data_direction direction)
{
	struct tccvin_dmabuf_dmabuf_attachment *a = attachment->priv;
	struct sg_table *sgt;

	sgt = &a->sgt;

	if (!dma_map_sg(attachment->dev, sgt->sgl, sgt->nents, direction))
		return ERR_PTR(-ENOMEM);

	a->mapped = true;
	return sgt;
}

static void tccvin_dmabuf_unmap_dmabuf(struct dma_buf_attachment *attachment,
					struct sg_table *sgt,
					enum dma_data_direction direction)
{
	struct tccvin_dmabuf_dmabuf_attachment *a = attachment->priv;

	a->mapped = false;
	dma_unmap_sg(attachment->dev, sgt->sgl, sgt->nents, direction);
}

void tccvin_dmabuf_dmabuf_release(struct dma_buf *dmabuf)
{
	struct tccvin_dmabuf_buffer *buffer = dmabuf->priv;

	tccvin_dmabuf_heap_free(buffer);
}

static void *tccvin_dmabuf_dmabuf_kmap(
	struct dma_buf *dmabuf, unsigned long offset)
{
	struct tccvin_dmabuf_buffer *buffer = dmabuf->priv;

	return buffer->vaddr ? buffer->vaddr + offset * PAGE_SIZE : NULL;
}

static void tccvin_dmabuf_dmabuf_kunmap(struct dma_buf *dmabuf,
					unsigned long offset, void *ptr)
{
}

static const struct dma_buf_ops tccvin_dmabuf_dmabuf_ops = {
	.attach = tccvin_dmabuf_dmabuf_attach,
	.detach = tccvin_dmabuf_dmabuf_detach,
	.begin_cpu_access = tccvin_dmabuf_dmabuf_begin_cpu_access,
	.end_cpu_access = tccvin_dmabuf_dmabuf_end_cpu_access,
	.mmap = tccvin_dmabuf_mmap,
	.map_dma_buf = tccvin_dmabuf_map_dmabuf,
	.unmap_dma_buf = tccvin_dmabuf_unmap_dmabuf,
	.release = tccvin_dmabuf_dmabuf_release,
	.map = tccvin_dmabuf_dmabuf_kmap,
	.map_atomic = tccvin_dmabuf_dmabuf_kmap,
	.unmap = tccvin_dmabuf_dmabuf_kunmap,
	.unmap_atomic = tccvin_dmabuf_dmabuf_kunmap,
};

int tccvin_dmabuf_alloc(struct tccvin_dmabuf_heap *heap,
			size_t size, unsigned int flags)
{
	struct tccvin_dmabuf_buffer *buffer;
	struct dma_buf *dmabuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	int fd;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	ret = tccvin_dmabuf_heap_alloc(heap, buffer, size, flags);
	if (ret)
		return ret;

	exp_info.ops = &tccvin_dmabuf_dmabuf_ops;
	exp_info.size = PAGE_ALIGN(buffer->size);
	exp_info.flags = flags | O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (unlikely(IS_ERR(dmabuf))) {
		tccvin_dmabuf_heap_free(buffer);
		return PTR_ERR(dmabuf);
	}

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0)
		dma_buf_put(dmabuf);

	return fd;
}

int tccvin_dmabuf_phys(struct platform_device *pdev, int fd,
			phys_addr_t *addr, size_t *len)
{
	struct dma_buf *dmabuf;
	struct page *page;
	struct tccvin_dmabuf_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	buffer = dmabuf->priv;
	if (!buffer) {
		dma_buf_put(dmabuf);
		return -ENODEV;
	}
	page = sg_page(buffer->sgt->sgl);
	*addr = PFN_PHYS(page_to_pfn(page));
	*len = buffer->size;
	dma_buf_put(dmabuf);

	return 0;
}

long tccvin_dmabuf_g_phys(struct tccvin_fh *fh, struct v4l2_buffer *buf)
{
	struct tccvin_streaming *stream = fh->stream;
	struct tccvin_dmabuf_phys_data phys;

	phys.fd = stream->queue.queue.bufs[buf->index]->planes[0].m.fd;
	if (tccvin_dmabuf_phys(stream->dev->pdev, phys.fd,
				&phys.paddr,
				&phys.len) < 0) {
		loge("Fail conversion of physical address using fd\n");
		return -EFAULT;
	}

	logd("Successful conversion of physical(0x%llx) address using fd(%d)\n",
		phys.paddr,
		stream->queue.queue.bufs[buf->index]->planes[0].m.fd);

	return phys.paddr;
}
