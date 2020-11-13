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
#include <video/tcc/tcc_mem_ioctl.h>

#define ION_CARVEOUT_ALLOCATE_FAIL	-1

#define ION_AUTOFREE_DEBUG 0

#define AUTO_FREE_BLK_LENGTH 6
static int AUTO_FREE_BUF_LENGTH = 50;

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

#ifdef USE_UMP_RESERVED_SW_PMAP
#define TCC_MEM "/dev/tmem"
static struct file *tccmem_file;

static void ump_reserved_sw_manager(unsigned int cmd, unsigned int paddr,
		    unsigned int size, unsigned int width, unsigned int height)
{
	if (tccmem_file == NULL) {
		tccmem_file = filp_open(TCC_MEM, O_RDWR, 0666);
		if (tccmem_file == NULL)
			pr_err("driver open fail (%s)\n", TCC_MEM);
	}

	if (tccmem_file) {
		if (cmd == TCC_REGISTER_UMP_SW_INFO_KERNEL) {
			stIonBuff_info info;

			info.phy_addr = paddr;
			info.size = size;
			info.width = width;
			info.height = height;

			if (tccmem_file->f_op->unlocked_ioctl(tccmem_file,
						 cmd, (unsigned long)&info) < 0)
				pr_err("REGISTER fail paddr:0x%x\n", paddr);
		} else if ((cmd == TCC_DEREGISTER_UMP_SW_INFO_KERNEL) ||
			 (cmd == TCC_AUTOFREE_DEREGISTER_UMP_SW_INFO_KERNEL)) {
			if (tccmem_file->f_op->unlocked_ioctl(tccmem_file,
						cmd, (unsigned long)&paddr) < 0)
				pr_err("DEREGISTER fail paddr:0x%x\n", paddr);
		} else
			pr_err("UnKnown command 0x%x\n", cmd);
	}

	if (0) { //tccmem_file)
		filp_close(tccmem_file, 0);
		tccmem_file = NULL;
	}
}
#endif

static void af_alloc(struct ion_buffer *buffer, struct ion_heap *heap)
{
	struct sg_table *table;
	struct scatterlist *sg;
	unsigned long paddr;
	int index_check_cnt = 0;
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	struct af_desc *af_bufinfo = carveout_heap->af_buf;

	table = buffer->sg_table;
	sg = table->sgl;
	paddr = page_to_phys(sg_page(sg));

	while (index_check_cnt <= (AUTO_FREE_BUF_LENGTH-1)) {
		if (af_bufinfo[carveout_heap->af_alloc_index].valid == 0) {
			af_bufinfo[carveout_heap->af_alloc_index].buffer =
							buffer;
			af_bufinfo[carveout_heap->af_alloc_index].valid = 1;
			af_bufinfo[carveout_heap->af_alloc_index].buffer->size
							= table->sgl->length;
			#ifdef ION_AUTOFREE_DEBUG
			pr_debug("%s af_alloc_index=%d, af_free_index=%d,",
				 __func__, carveout_heap->af_alloc_index,
				carveout_heap->af_free_index);
			pr_debug("addr:0x%lx, size:0x%x, index_check_cnt:%d\n",
				 paddr,
				 buffer->size, index_check_cnt);
			#endif
			carveout_heap->af_alloc_index =
				(carveout_heap->af_alloc_index + 1)
				% AUTO_FREE_BUF_LENGTH;
			break;
		} else {
			carveout_heap->af_alloc_index =
				(carveout_heap->af_alloc_index + 1)
				% AUTO_FREE_BUF_LENGTH;
			index_check_cnt++;
		}
	}
	if (index_check_cnt == AUTO_FREE_BUF_LENGTH) {
		AUTO_FREE_BUF_LENGTH += 10;
		#ifdef ION_ATUOFREE_DEBUG
		pr_debug("%s avail_size=0x%x but, autofree index is full.",
			 __func__, gen_pool_avail(carveout_heap->pool));
		pr_debug("You have to increase  AUTO_FREE_BUF_LENGTH :%d\n",
			 AUTO_FREE_BUF_LENGTH);
		#endif
	}
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
	while (buffer_end == 0) {
		if (af_bufinfo[i].valid && af_bufinfo[i].buffer == buffer) {
			#ifdef ION_AUTOFREE_DEBUG
			pr_debug("%s, buffer:%p is now free af_free_index=%d\n",
				  __func__, af_bufinfo[i].buffer, i);
			#endif
			af_bufinfo[i].buffer = NULL;
			af_bufinfo[i].valid = 0;
			carveout_heap->af_free_index = i;
			return 0;
		}

		i = (i + 1) % AUTO_FREE_BUF_LENGTH;

		if (i == buffer_start_index)
			buffer_end = 1;
	}
	#ifdef ION_AUTOFREE_DEBUG
	pr_debug("%s buffer:%p is already free or can't be freed",
		 __func__, buffer);
	pr_debug("because af_alloc_index is full\n");
	#endif
	return 1;	/* already free  */
}

static int block_auto_free(struct ion_buffer *buffer, struct ion_heap *heap,
			   unsigned long size)
{
	int i, j, buffer_start_index;
	int buffer_end = 0;
	struct sg_table *table;
	struct scatterlist *sg;
	unsigned long paddr;
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	struct af_desc *af_bufinfo = carveout_heap->af_buf;

	if (gen_pool_avail(carveout_heap->pool) >= size) {
		i = AUTO_FREE_BLK_LENGTH;
		j = buffer_start_index = carveout_heap->af_free_index;
		while ((buffer_end == 0) || (i > 0)) {
			if (af_bufinfo[j].valid) {
				table = af_bufinfo[j].buffer->sg_table;
				sg = table->sgl;
				paddr = page_to_phys(sg_page(sg));
				gen_pool_free(carveout_heap->pool, paddr,
					      af_bufinfo[j].buffer->size);
				af_bufinfo[j].buffer = NULL;
				af_bufinfo[j].valid = 0;
				i--;
			#ifdef USE_UMP_RESERVED_SW_PMAP
				ump_reserved_sw_manager(
				  TCC_AUTOFREE_DEREGISTER_UMP_SW_INFO_KERNEL,
							 paddr, 0, 0, 0);
			#endif
			}
			j = (j + 1) % AUTO_FREE_BUF_LENGTH;

			if (j == buffer_start_index)
				buffer_end = 1;
		}
	} else {
		j = buffer_start_index = carveout_heap->af_free_index;
		while (gen_pool_avail(carveout_heap->pool) <= size) {
			if (af_bufinfo[j].valid) {
				table = af_bufinfo[j].buffer->sg_table;
				sg = table->sgl;
				paddr = page_to_phys(sg_page(sg));
				gen_pool_free(carveout_heap->pool, paddr,
					      af_bufinfo[j].buffer->size);
				af_bufinfo[j].buffer = NULL;
				af_bufinfo[j].valid = 0;
			#ifdef USE_UMP_RESERVED_SW_PMAP
				ump_reserved_sw_manager(
				  TCC_AUTOFREE_DEREGISTER_UMP_SW_INFO_KERNEL,
							 paddr, 0, 0, 0);
			#endif
			}
			j = (j + 1) % AUTO_FREE_BUF_LENGTH;

			if (j == buffer_start_index) {
				pr_err("%s req. size is larger than heap size",
					__func__);
				pr_err("avail_size:0x%zx, size:0x%lx\n",
					gen_pool_avail(carveout_heap->pool),
					size);
				break;
			}
		}
	}
	#ifdef ION_AUTOFREE_DEBUG
	pr_debug("%s Success. af_alloc_index=%d, af_free_index=%d",
		 __func__, carveout_heap->af_alloc_index,
		 carveout_heap->af_free_index);
	pr_debug(" size=0x%lx, avail_size=0x%x\n",
		 size, gen_pool_avail(carveout_heap->pool));
	#endif
	return 1;
}

static unsigned long ion_carveout_allocate(struct ion_heap *heap,
					 unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	#ifdef ION_AUTOFREE_DEBUG
	pr_debug("%s-heap_name:%s Alloc %lx - 0x%lx avail_mem:0x%x\n",
		 __func__, heap->name, offset, size,
		 gen_pool_avail(carveout_heap->pool));
	#endif
	if (!offset) {
		pr_err("%s-heap_name:%s Alloc %lx - 0x%lx avail_mem:0x%zx\n",
			__func__, heap->name, offset, size,
			gen_pool_avail(carveout_heap->pool));
		return ION_CARVEOUT_ALLOCATE_FAIL;
	}

#ifdef USE_UMP_RESERVED_SW_PMAP
	ump_reserved_sw_manager(TCC_REGISTER_UMP_SW_INFO_KERNEL, offset, size,
				 0, 0);
#endif

	return offset;
}

static void ion_carveout_free(struct ion_heap *heap, unsigned long addr,
			      unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
	#ifdef ION_AUTOFREE_DEBUG
	pr_debug("%s-heap_name:%s, Free 0x%lx - 0x%lx avail_mem:0x%x\n",
		 __func__, heap->name, addr, size,
		 gen_pool_avail(carveout_heap->pool));
	#endif

#ifdef USE_UMP_RESERVED_SW_PMAP
	ump_reserved_sw_manager(TCC_DEREGISTER_UMP_SW_INFO_KERNEL, addr, size,
				 0, 0);
#endif
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct sg_table *table;
	unsigned long paddr;
	int ret;

#if 0//!defined(CONFIG_SUPPORT_TCC_HEVC_4K)
//HDMI IN ->1920x1080x4 = 0x7e9000
	if (size > 0x800000) {
		pr_err("%sfailed. The requested size is too big.\n", __func__);
		pr_err("Please check CONFIG_SUPPORT_TCC_HEVC_4K\n");
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
		if (buffer->flags == ION_FLAG_AUTOFREE_ENABLE)
			block_auto_free(buffer, heap, size);
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;
	if (buffer->flags == ION_FLAG_AUTOFREE_ENABLE)
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
	struct scatterlist *sg = table->sgl;
	unsigned long paddr = page_to_phys(sg_page(sg));

	ion_heap_buffer_zero(buffer);

	if (buffer->flags == ION_FLAG_AUTOFREE_ENABLE)
		already_free = af_free(buffer);

	if (!already_free)
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

static int ion_carveout_heap_debug_show(struct ion_heap *heap,
					struct seq_file *s, void *unused)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	seq_printf(s, "total_mem:%zu bytes, free_mem:%zu bytes\n",
				gen_pool_size(carveout_heap->pool),
				gen_pool_avail(carveout_heap->pool));
	return 0;
}

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
	carveout_heap->heap.debug_show = ion_carveout_heap_debug_show;
	pr_info("%s heap size : 0x%zx\n", __func__, heap_data->size);

	carveout_heap->af_buf =
		kzalloc(sizeof(af_desc)*AUTO_FREE_BUF_LENGTH, GFP_KERNEL);
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

struct ion_heap *ion_carveout_cam_heap_create(
					struct ion_platform_heap *heap_data)
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
	pr_info("%s heap size : 0x%zx\n", __func__, heap_data->size);

	carveout_heap->af_buf =
		 kzalloc(sizeof(af_desc)*AUTO_FREE_BUF_LENGTH, GFP_KERNEL);
	carveout_heap->af_alloc_index = 0;
	carveout_heap->af_free_index = 0;

	return &carveout_heap->heap;
}
#endif
