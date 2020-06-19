/* tcc_drm_gem.h
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authoer: Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _TCC_DRM_GEM_H_
#define _TCC_DRM_GEM_H_
#include <linux/reservation.h>
#include <linux/version.h>
#include <linux/dma-buf.h>
#include <drm/drm_gem.h>
#define to_tcc_gem(x)	container_of(x, struct tcc_drm_gem, base)

#define IS_NONCONTIG_BUFFER(f)		(f & TCC_BO_NONCONTIG)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/dma-resv.h>
#else
#include <linux/reservation.h>

/* Reservation object types */
#define dma_resv                        reservation_object
#define dma_resv_list                   reservation_object_list

/* Reservation object functions */
#define dma_resv_add_excl_fence         reservation_object_add_excl_fence
#define dma_resv_add_shared_fence       reservation_object_add_shared_fence
#define dma_resv_fini                   reservation_object_fini
#define dma_resv_get_excl               reservation_object_get_excl
#define dma_resv_get_list               reservation_object_get_list
#define dma_resv_held                   reservation_object_held
#define dma_resv_init                   reservation_object_init
#define dma_resv_reserve_shared         reservation_object_reserve_shared
#define dma_resv_test_signaled_rcu      reservation_object_test_signaled_rcu
#define dma_resv_wait_timeout_rcu       reservation_object_wait_timeout_rcu
#endif

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
	unsigned long		dma_attrs;
	struct page		**pages;
	struct sg_table		*sgt;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0))
	struct dma_resv _resv;
#endif
	struct dma_resv *resv;

	bool cpu_prep;
};

struct dma_resv *tcc_gem_prime_res_obj(struct drm_gem_object *obj);

/* destroy a buffer with gem object */
void tcc_drm_gem_destroy(struct tcc_drm_gem *tcc_gem);

struct dma_buf *tcc_drm_gem_prime_export(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
				     struct drm_device *dev,
#endif
				     struct drm_gem_object *obj,
				     int flags);

struct drm_gem_object *tcc_drm_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf);

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

/* get buffer information to memory region allocated by gem. */
int tcc_drm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv);

int tcc_gem_cpu_prep_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file);

int tcc_gem_cpu_fini_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file);

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

/* page fault handler and mmap fault address(virtual) to physical memory. */
int tcc_drm_gem_fault(struct vm_fault *vmf);

/* set vm_flags and we can change the vm attribute to other one at here. */
int tcc_drm_gem_mmap(struct file *filp, 
				struct vm_area_struct *vma);

int tcc_drm_gem_prime_mmap(struct dma_buf *dma_buf,
			      struct vm_area_struct *vma);
/* low-level interface prime helpers */
struct sg_table *tcc_drm_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *
tcc_drm_gem_prime_import_sg_table(struct drm_device *dev,
				     struct dma_buf_attachment *attach,
				     struct sg_table *sgt);
void *tcc_drm_gem_prime_vmap(struct drm_gem_object *obj);
void tcc_drm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
#endif
