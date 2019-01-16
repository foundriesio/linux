/*
 * drivers/staging/android/ion/ion_carveout_heap.c
 *
 * Copyright (C) 2018 Telechips Inc.
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
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"

#define ION_CARVEOUT_ALLOCATE_FAIL	-1

static int autofree_debug = 0;
#define dprintk(msg...)	if (autofree_debug) { printk( "autofree: " msg); }

#define AUTO_FREE_BUF_LENGTH 50
#define AUTO_FREE_BLK_LENGTH 6

typedef struct af_desc {
	struct ion_buffer *buffer;
	int valid;
} af_desc;

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
	struct af_desc *af_buf;
	int af_alloc_index;
	int af_free_index;
};

static void af_alloc(struct ion_buffer *buffer, struct ion_heap *heap)
{
	struct sg_table *table;
	struct page *page;
	phys_addr_t paddr;
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	struct af_desc *af_bufinfo = carveout_heap->af_buf;
	
	table = buffer->sg_table;
	page = sg_page(table->sgl);
	paddr = PFN_PHYS(page_to_pfn(page));

	af_bufinfo[carveout_heap->af_alloc_index].buffer = buffer;
	af_bufinfo[carveout_heap->af_alloc_index].valid = 1;
	af_bufinfo[carveout_heap->af_alloc_index].buffer->size = table->sgl->length;
	dprintk("%s af_alloc_index=%d, af_free_index=%d, addr:%p, size:0x%x\n",__func__, carveout_heap->af_alloc_index,  carveout_heap->af_free_index, paddr, buffer->size);
	carveout_heap->af_alloc_index = (carveout_heap->af_alloc_index + 1) % AUTO_FREE_BUF_LENGTH;

}

static int af_free(struct ion_buffer *buffer)
{
	int i, buffer_start_index;
	int buffer_end = 0;
	struct ion_heap *heap = buffer->heap;
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	struct af_desc *af_bufinfo = carveout_heap->af_buf;

	i = buffer_start_index = carveout_heap->af_free_index;
	while(buffer_end==0)
	{
		if (af_bufinfo[i].valid && af_bufinfo[i].buffer == buffer) 
		{
			dprintk("%s Now Free i=%d, buffer:%p\n", __func__, i, af_bufinfo[i].buffer);
			af_bufinfo[i].buffer = NULL;
			af_bufinfo[i].valid = 0;
			return 0;
		}
		i = (i + 1) % AUTO_FREE_BUF_LENGTH;
		
		if(i==buffer_start_index)
			buffer_end = 1;
	}
	dprintk("%s Already Free buffer:%p\n", __func__, buffer);
	return 1;	/* already free  */
}

static int block_auto_free(struct ion_buffer *buffer, struct ion_heap *heap, unsigned long size)

{
	struct sg_table *table;
	struct page *page;
	phys_addr_t paddr;
	int i;
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	struct af_desc *af_bufinfo = carveout_heap->af_buf;

	if(gen_pool_avail(carveout_heap->pool) >= size)
	{
		for(i=0; i<AUTO_FREE_BLK_LENGTH; i++)
		{
			table = af_bufinfo[carveout_heap->af_free_index].buffer->sg_table;
			page = sg_page(table->sgl);
			paddr = PFN_PHYS(page_to_pfn(page));
			gen_pool_free(carveout_heap->pool, paddr, af_bufinfo[carveout_heap->af_free_index].buffer->size);
			af_bufinfo[carveout_heap->af_free_index].buffer = NULL;
			af_bufinfo[carveout_heap->af_free_index].valid = 0;
			carveout_heap->af_free_index = (carveout_heap->af_free_index + 1) % AUTO_FREE_BUF_LENGTH;
		}
	}
	else
	{
		while (gen_pool_avail(carveout_heap->pool) <= size)
		{
			table = af_bufinfo[carveout_heap->af_free_index].buffer->sg_table;
			page = sg_page(table->sgl);
			paddr = PFN_PHYS(page_to_pfn(page));
			gen_pool_free(carveout_heap->pool, paddr, af_bufinfo[carveout_heap->af_free_index].buffer->size);
			af_bufinfo[carveout_heap->af_free_index].buffer = NULL;
			af_bufinfo[carveout_heap->af_free_index].valid = 0;
			carveout_heap->af_free_index = (carveout_heap->af_free_index + 1) % AUTO_FREE_BUF_LENGTH;
		}
	}
	dprintk("%s Success. af_alloc_index=%d, af_free_index=%d, size=0x%lx, avail_size=0x%x\n", __func__, carveout_heap->af_alloc_index, carveout_heap->af_free_index, size, gen_pool_avail(carveout_heap->pool));
	return 1;
}

static phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
					 unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	dprintk("%s-heap_name:%s Alloc %lx - 0x%lx avail_mem:0x%x\n", __func__, heap->name, offset, size, gen_pool_avail(carveout_heap->pool));
	if (!offset)
		return ION_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

static void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
			      unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
	dprintk("%s-heap_name:%s, Free %p - 0x%lx avail_mem:0x%x\n", __func__, heap->name, addr, size, gen_pool_avail(carveout_heap->pool));
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;

#if 0//!defined(CONFIG_SUPPORT_TCC_HEVC_4K)
	if(size>0x800000)               //HDMI IN ->1920x1080x4 = 0x7e9000	
	{
		printk("%s is failed. The requested size is too big.\nPlease check CONFIG_SUPPORT_TCC_HEVC_4K\n", __func__);
		return -ENOMEM;
	}
#endif

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_carveout_allocate(heap, size);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		if(buffer->flags == ION_FLAG_AUTOFREE_ENABLE)
			block_auto_free(buffer, heap, size);
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;
	if(buffer->flags == ION_FLAG_AUTOFREE_ENABLE)
		af_alloc(buffer, heap);

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	int already_free = 0;
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_heap_buffer_zero(buffer);

	if (ion_buffer_cached(buffer))
		dma_sync_sg_for_device(NULL, table->sgl, table->nents,
							DMA_BIDIRECTIONAL);

	if(buffer->flags == ION_FLAG_AUTOFREE_ENABLE)
		already_free = af_free(buffer);

	if(!already_free)
		ion_carveout_free(heap, paddr, buffer->size);
	
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	struct page *page;
	size_t size;
	int ret;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

#ifndef CONFIG_TCC803X_CA7S
	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);
#endif

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
		     -1);
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
	pr_info("%s heap size : 0x%x\n", __func__, heap_data->size);
	
	carveout_heap->af_buf = kzalloc(sizeof(af_desc)*AUTO_FREE_BUF_LENGTH, GFP_KERNEL);
	carveout_heap->af_alloc_index = 0;
	carveout_heap->af_free_index = 0;
	return &carveout_heap->heap;
}

#ifdef CONFIG_ION_CARVEOUT_CAM_HEAP
static struct ion_heap_ops carveout_cam_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_carveout_cam_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

#ifndef CONFIG_TCC803X_CA7S
	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);
#endif

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
		     -1);
	carveout_heap->heap.ops = &carveout_cam_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT_CAM;
	pr_info("%s heap size : 0x%x\n", __func__, heap_data->size);
	
	carveout_heap->af_buf = kzalloc(sizeof(af_desc)*AUTO_FREE_BUF_LENGTH, GFP_KERNEL);
	carveout_heap->af_alloc_index = 0;
	carveout_heap->af_free_index = 0;

	return &carveout_heap->heap;
}
#endif
