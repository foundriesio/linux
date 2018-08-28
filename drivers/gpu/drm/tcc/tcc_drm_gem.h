/* tcc_drm_gem.h
 *
 * Copyright (c) 2011 Telechips Electronics Co., Ltd.
 * Authoer: Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _TCC_DRM_GEM_H_
#define _TCC_DRM_GEM_H_

#include <drm/drm_gem.h>

#define to_tcc_gem(x)	container_of(x, struct tcc_drm_gem, base)

#define IS_NONCONTIG_BUFFER(f)		(f & TCC_BO_NONCONTIG)

/*
 * tcc drm buffer structure.
 *
 * @base: a gem object.
 *	- a new handle to this gem object would be created
 *	by drm_gem_handle_create().
 * @buffer: a pointer to tcc_drm_gem_buffer object.
 *	- contain the information to memory region allocated
 *	by user request or at framebuffer creation.
 *	continuous memory region allocated by user request
 *	or at framebuffer creation.
 * @flags: indicate memory type to allocated buffer and cache attruibute.
 * @size: size requested from user, in bytes and this size is aligned
 *	in page unit.
 * @cookie: cookie returned by dma_alloc_attrs
 * @kvaddr: kernel virtual address to allocated memory region.
 * @dma_addr: bus address(accessed by dma) to allocated memory region.
 *	- this address could be physical address without IOMMU and
 *	device address with IOMMU.
 * @pages: Array of backing pages.
 * @sgt: Imported sg_table.
 *
 * P.S. this object would be transferred to user as kms_bo.handle so
 *	user can access the buffer through kms_bo.handle.
 */
struct tcc_drm_gem {
	struct drm_gem_object	base;
	unsigned int		flags;
	unsigned long		size;
	void			*cookie;
	void __iomem		*kvaddr;
	dma_addr_t		dma_addr;
	struct dma_attrs	dma_attrs;
	struct page		**pages;
	struct sg_table		*sgt;
};

struct page **tcc_gem_get_pages(struct drm_gem_object *obj, gfp_t gfpmask);

/* destroy a buffer with gem object */
void tcc_drm_gem_destroy(struct tcc_drm_gem *tcc_gem);

/* create a new buffer with gem object */
struct tcc_drm_gem *tcc_drm_gem_create(struct drm_device *dev,
					     unsigned int flags,
					     unsigned long size);

/*
 * request gem object creation and buffer allocation as the size
 * that it is calculated with framebuffer information such as width,
 * height and bpp.
 */
int tcc_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);

/* get fake-offset of gem object that can be used with mmap. */
int tcc_drm_gem_map_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);

/*
 * get dma address from gem handle and this function could be used for
 * other drivers such as 2d/3d acceleration drivers.
 * with this function call, gem object reference count would be increased.
 */
dma_addr_t *tcc_drm_gem_get_dma_addr(struct drm_device *dev,
					unsigned int gem_handle,
					struct drm_file *filp);

/*
 * put dma address from gem handle and this function could be used for
 * other drivers such as 2d/3d acceleration drivers.
 * with this function call, gem object reference count would be decreased.
 */
void tcc_drm_gem_put_dma_addr(struct drm_device *dev,
					unsigned int gem_handle,
					struct drm_file *filp);

/* map user space allocated by malloc to pages. */
int tcc_drm_gem_userptr_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv);

/* get buffer information to memory region allocated by gem. */
int tcc_drm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv);

/* get buffer size to gem handle. */
unsigned long tcc_drm_gem_get_size(struct drm_device *dev,
						unsigned int gem_handle,
						struct drm_file *file_priv);

/* free gem object. */
void tcc_drm_gem_free_object(struct drm_gem_object *obj);

/* create memory region for drm framebuffer. */
int tcc_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args);

/* map memory region for drm framebuffer to user space. */
int tcc_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				   struct drm_device *dev, uint32_t handle,
				   uint64_t *offset);

/* page fault handler and mmap fault address(virtual) to physical memory. */
int tcc_drm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

/* set vm_flags and we can change the vm attribute to other one at here. */
int tcc_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);

static inline int vma_is_io(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & (VM_IO | VM_PFNMAP));
}

/* get a copy of a virtual memory region. */
struct vm_area_struct *tcc_gem_get_vma(struct vm_area_struct *vma);

/* release a userspace virtual memory area. */
void tcc_gem_put_vma(struct vm_area_struct *vma);

/* get pages from user space. */
int tcc_gem_get_pages_from_userptr(unsigned long start,
						unsigned int npages,
						struct page **pages,
						struct vm_area_struct *vma);

/* drop the reference to pages. */
void tcc_gem_put_pages_to_userptr(struct page **pages,
					unsigned int npages,
					struct vm_area_struct *vma);

/* map sgt with dma region. */
int tcc_gem_map_sgt_with_dma(struct drm_device *drm_dev,
				struct sg_table *sgt,
				enum dma_data_direction dir);

/* unmap sgt from dma region. */
void tcc_gem_unmap_sgt_from_dma(struct drm_device *drm_dev,
				struct sg_table *sgt,
				enum dma_data_direction dir);

/* low-level interface prime helpers */
struct sg_table *tcc_drm_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *
tcc_drm_gem_prime_import_sg_table(struct drm_device *dev,
				     struct dma_buf_attachment *attach,
				     struct sg_table *sgt);
void *tcc_drm_gem_prime_vmap(struct drm_gem_object *obj);
void tcc_drm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
int tcc_drm_gem_prime_mmap(struct drm_gem_object *obj,
			      struct vm_area_struct *vma);

#endif
