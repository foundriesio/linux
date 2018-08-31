/*
 * drivers/staging/android/ion/ion_carveout_heap.c
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

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
};

size_t gHeapSize;
#define AUTO_FREE_LENGTH 12

typedef struct af_desc {
	struct ion_buffer *buffer;
	struct gen_pool *pool;
	int valid;
} af_desc;

static int af_desc_head = 0;
static int af_desc_tail = AUTO_FREE_LENGTH-1;
static af_desc af_descbuf[AUTO_FREE_LENGTH];

static int is_af_descbuf_full(void)
{
	int i;

	for(i=0; i<AUTO_FREE_LENGTH; i++)
	{
		if (af_descbuf[i].valid == 0) 
		{
			af_desc_head = i;

			return 0;
		}
	}
	af_desc_tail = (af_desc_tail + 1) % AUTO_FREE_LENGTH;
	return 1;
}


static void af_alloc(struct ion_buffer *buffer, struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	
	if (is_af_descbuf_full()) 
	{
		if (af_descbuf[af_desc_tail].valid) 
		{
			struct sg_table *table = af_descbuf[af_desc_tail].buffer->sg_table;
			struct page *page = sg_page(table->sgl);
			phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

			dprintk("autofree before buffer allocation head=%d, tail=%d, buffer:0x%x, addr:0x%x, size:0x%x\n",af_desc_head, af_desc_tail, af_descbuf[af_desc_tail].buffer, paddr, af_descbuf[af_desc_tail].buffer->size);
			gen_pool_free(af_descbuf[af_desc_tail].pool, paddr, af_descbuf[af_desc_tail].buffer->size);

			af_descbuf[af_desc_tail].buffer = NULL;
			af_descbuf[af_desc_tail].pool = NULL;
			af_descbuf[af_desc_tail].valid = 0;
			af_desc_head = af_desc_tail;
		}
	}
	
	af_descbuf[af_desc_head].buffer = buffer;
	af_descbuf[af_desc_head].pool = carveout_heap->pool;
	af_descbuf[af_desc_head].valid = 1;
    struct sg_table *table = af_descbuf[af_desc_head].buffer->sg_table;
    struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));
	af_descbuf[af_desc_head].buffer->size = table->sgl->length;
	dprintk("af_alloc head=%d, tail=%d, buffer:0x%x, addr:0x%x, size:0x%x\n",af_desc_head, af_desc_tail, af_descbuf[af_desc_head].buffer, paddr, af_descbuf[af_desc_head].buffer->size);
	af_desc_head = (af_desc_head + 1) % AUTO_FREE_LENGTH;
}

static int af_free(struct ion_buffer *buffer)
{
	int i, buffer_start_index;
	int buffer_end = 0;

	i = buffer_start_index = af_desc_tail;
	while(buffer_end==0)
	{
		if (af_descbuf[i].valid && af_descbuf[i].buffer == buffer) 
		{
			dprintk("%s Now Free i=%d, buffer:0x%x\n", __func__, i, af_descbuf[i].buffer);
			af_descbuf[i].buffer = NULL;
			af_descbuf[i].pool = NULL;
			af_descbuf[i].valid = 0;
			return 0;
		}
		i = (i + 1) % AUTO_FREE_LENGTH;
		
		if(i==buffer_start_index)
			buffer_end = 1;
	}
	dprintk("%s Already Free buffer:0x%x\n", __func__, buffer);
	return 1;	/* already free  */
}

static int block_auto_free(struct ion_buffer *buffer, struct ion_heap *heap)
{
	int i, buffer_start_index;
	int buffer_end = 0;
	int existingNumfree;
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	i = buffer_start_index = af_desc_tail;
	while(buffer_end==0)
	{
		if (af_descbuf[i].valid) 
		{
			struct sg_table *table = af_descbuf[i].buffer->sg_table;
			struct page *page = sg_page(table->sgl);
			phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));
			gen_pool_free(af_descbuf[i].pool, paddr, af_descbuf[i].buffer->size);
			af_descbuf[i].buffer = NULL;
			af_descbuf[i].pool = NULL;
			af_descbuf[i].valid = 0;
		}
		af_desc_tail = (af_desc_tail + 1) % AUTO_FREE_LENGTH;
		if(gen_pool_avail(carveout_heap->pool) >= buffer->size)
		{
			pr_info("%s, Success af buffer head=%d, af buffer tail=%d, free=0x%x\n", __func__, af_desc_head, af_desc_tail, gen_pool_avail(carveout_heap->pool));
			return 1;
		}
		i = (i + 1) % AUTO_FREE_LENGTH;
		
		if(i==buffer_start_index)
			buffer_end = 1;
	}

	printk("%s, Fail autofree buffer head=%d, af buffer tail=%d\n", __func__, af_desc_head, af_desc_tail);
	return 0;
}

static phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
					 unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	//printk("Alloc %p - 0x%x\n", offset, size);
	if (!offset)
		return ION_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

static void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
			      unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	//printk("Free %p - 0x%x \n", addr, size);
	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
}

static int ion_carveout_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	*addr = paddr;
	*len = buffer->size;
	return 0;
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
			block_auto_free(buffer, heap);
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
	{
		ion_carveout_free(heap, paddr, buffer->size);
	}
	
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.phys = ion_carveout_heap_phys,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
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
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
//	carveout_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	printk("%s heap size : 0x%x\n", __func__, heap_data->size);
	gHeapSize = heap_data->size;

	return &carveout_heap->heap;
}
