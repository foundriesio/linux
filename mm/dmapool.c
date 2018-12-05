/*
 * DMA Pool allocator
 *
 * Copyright 2001 David Brownell
 * Copyright 2007 Intel Corporation
 *   Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * This software may be redistributed and/or modified under the terms of
 * the GNU General Public License ("GPL") version 2 as published by the
 * Free Software Foundation.
 *
 * This allocator returns small blocks of a given size which are DMA-able by
 * the given device.  It uses the dma_alloc_coherent page allocator to get
 * new pages, then splits them up into blocks of the required size.
 * Many older drivers still have their own code to do this.
 *
 * The current design of this allocator is fairly simple.  The pool is
 * represented by the 'struct dma_pool'.  Each allocated page is split into
 * blocks of at least 'size' bytes.  Free blocks are tracked in an unsorted
 * singly-linked list of free blocks within the page.  Used blocks aren't
 * tracked, but we keep a count of how many are currently allocated from each
 * page.
 *
 * The pool keeps two doubly-linked list of allocated pages.  The 'available'
 * list tracks pages that have one or more free blocks, and the 'full' list
 * tracks pages that have no free blocks.  Pages are moved from one list to
 * the other as their blocks are allocated and freed.
 *
 * When allocating DMA pages, we use some available space in 'struct page' to
 * store data private to dmapool; search 'dma_pool' in the definition of
 * 'struct page' for details.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/poison.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/wait.h>

#if defined(CONFIG_DEBUG_SLAB) || defined(CONFIG_SLUB_DEBUG_ON)
#define DMAPOOL_DEBUG 1
#endif

struct dma_pool {		/* the pool */
#define POOL_FULL_IDX   0
#define POOL_AVAIL_IDX  1
#define POOL_MAX_IDX    2
	struct list_head page_list[POOL_MAX_IDX];
	spinlock_t lock;
	unsigned int size;
	struct device *dev;
	unsigned int allocation;
	unsigned int boundary;
	unsigned int blks_per_alloc;
	char name[32];
	struct list_head pools;
};

static DEFINE_MUTEX(pools_lock);
static DEFINE_MUTEX(pools_reg_lock);

static ssize_t
show_pools(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned temp;
	unsigned size;
	char *next;
	struct dma_pool *pool;

	next = buf;
	size = PAGE_SIZE;

	temp = scnprintf(next, size, "poolinfo - 0.1\n");
	size -= temp;
	next += temp;

	mutex_lock(&pools_lock);
	list_for_each_entry(pool, &dev->dma_pools, pools) {
		unsigned pages = 0;
		size_t blocks = 0;
		int list_idx;

		spin_lock_irq(&pool->lock);
		for (list_idx = 0; list_idx < POOL_MAX_IDX; list_idx++) {
			struct page *page;

			list_for_each_entry(page,
					    &pool->page_list[list_idx],
					    dma_list) {
				pages++;
				blocks += page->dma_in_use;
			}
		}
		spin_unlock_irq(&pool->lock);

		/* per-pool info, no real statistics yet */
		temp = scnprintf(next, size, "%-16s %4zu %4zu %4u %2u\n",
				 pool->name, blocks,
				 (size_t) pages * pool->blks_per_alloc,
				 pool->size, pages);
		size -= temp;
		next += temp;
	}
	mutex_unlock(&pools_lock);

	return PAGE_SIZE - size;
}

static DEVICE_ATTR(pools, 0444, show_pools, NULL);

/**
 * dma_pool_create - Creates a pool of consistent memory blocks, for dma.
 * @name: name of pool, for diagnostics
 * @dev: device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @boundary: returned blocks won't cross this power of two boundary
 * Context: !in_interrupt()
 *
 * Returns a dma allocation pool with the requested characteristics, or
 * null if one can't be created.  Given one of these pools, dma_pool_alloc()
 * may be used to allocate memory.  Such memory will all have "consistent"
 * DMA mappings, accessible by the device and its driver without using
 * cache flushing primitives.  The actual size of blocks allocated may be
 * larger than requested because of alignment.
 *
 * If @boundary is nonzero, objects returned from dma_pool_alloc() won't
 * cross that size boundary.  This is useful for devices which have
 * addressing restrictions on individual DMA transfers, such as not crossing
 * boundaries of 4KBytes.
 */
struct dma_pool *dma_pool_create(const char *name, struct device *dev,
				 size_t size, size_t align, size_t boundary)
{
	struct dma_pool *retval;
	size_t allocation;
	bool empty = false;

	if (align == 0)
		align = 1;
	else if (align & (align - 1))
		return NULL;

	if (size == 0 || size > INT_MAX)
		return NULL;
	else if (size < 4)
		size = 4;

	if ((size % align) != 0)
		size = ALIGN(size, align);

	allocation = max_t(size_t, size, PAGE_SIZE);

	if (!boundary)
		boundary = allocation;
	else if ((boundary < size) || (boundary & (boundary - 1)))
		return NULL;

	boundary = min(boundary, allocation);

	retval = kmalloc_node(sizeof(*retval), GFP_KERNEL, dev_to_node(dev));
	if (!retval)
		return retval;

	strlcpy(retval->name, name, sizeof(retval->name));

	retval->dev = dev;

	INIT_LIST_HEAD(&retval->page_list[POOL_FULL_IDX]);
	INIT_LIST_HEAD(&retval->page_list[POOL_AVAIL_IDX]);
	spin_lock_init(&retval->lock);
	retval->size = size;
	retval->boundary = boundary;
	retval->allocation = allocation;
	retval->blks_per_alloc =
		(allocation / boundary) * (boundary / size) +
		(allocation % boundary) / size;

	INIT_LIST_HEAD(&retval->pools);

	/*
	 * pools_lock ensures that the ->dma_pools list does not get corrupted.
	 * pools_reg_lock ensures that there is not a race between
	 * dma_pool_create() and dma_pool_destroy() or within dma_pool_create()
	 * when the first invocation of dma_pool_create() failed on
	 * device_create_file() and the second assumes that it has been done (I
	 * know it is a short window).
	 */
	mutex_lock(&pools_reg_lock);
	mutex_lock(&pools_lock);
	if (list_empty(&dev->dma_pools))
		empty = true;
	list_add(&retval->pools, &dev->dma_pools);
	mutex_unlock(&pools_lock);
	if (empty) {
		int err;

		err = device_create_file(dev, &dev_attr_pools);
		if (err) {
			mutex_lock(&pools_lock);
			list_del(&retval->pools);
			mutex_unlock(&pools_lock);
			mutex_unlock(&pools_reg_lock);
			kfree(retval);
			return NULL;
		}
	}
	mutex_unlock(&pools_reg_lock);
	return retval;
}
EXPORT_SYMBOL(dma_pool_create);

static void pool_initialize_free_block_list(struct dma_pool *pool, void *vaddr)
{
	unsigned int offset = 0;
	unsigned int next_boundary = pool->boundary;

	do {
		unsigned int next = offset + pool->size;
		if (unlikely((next + pool->size) > next_boundary)) {
			next = next_boundary;
			next_boundary += pool->boundary;
		}
		*(int *)(vaddr + offset) = next;
		offset = next;
	} while (offset < pool->allocation);
}

static struct page *pool_alloc_page(struct dma_pool *pool, gfp_t mem_flags)
{
	struct page *page;
	dma_addr_t dma;
	void *vaddr;

	vaddr = dma_alloc_coherent(pool->dev, pool->allocation, &dma,
				   mem_flags);
	if (!vaddr)
		return NULL;

#ifdef	DMAPOOL_DEBUG
	memset(vaddr, POOL_POISON_FREED, pool->allocation);
#endif
	pool_initialize_free_block_list(pool, vaddr);

	page = virt_to_page(vaddr);
	page->dma = dma;
	page->dma_free_off = 0;
	page->dma_in_use = 0;

	return page;
}

static inline bool is_page_busy(struct page *page)
{
	return page->dma_in_use != 0;
}

static void pool_free_page(struct dma_pool *pool, struct page *page)
{
	/* Save local copies of some page fields. */
	void *vaddr = page_to_virt(page);
	bool busy = is_page_busy(page);
	dma_addr_t dma = page->dma;

	list_del(&page->dma_list);

	/* Clear all the page fields we use. */
	page->dma_list.next = NULL;
	page->dma_list.prev = NULL;
	page->dma = 0;
	page->dma_free_off = 0;
	page_mapcount_reset(page); /* clear dma_in_use */

	if (busy) {
		dev_err(pool->dev,
			"dma_pool_destroy %s, %p busy\n",
			pool->name, vaddr);
		/* leak the still-in-use consistent memory */
	} else {
#ifdef	DMAPOOL_DEBUG
		memset(vaddr, POOL_POISON_FREED, pool->allocation);
#endif
		dma_free_coherent(pool->dev, pool->allocation, vaddr, dma);
	}
}

/**
 * dma_pool_destroy - destroys a pool of dma memory blocks.
 * @pool: dma pool that will be destroyed
 * Context: !in_interrupt()
 *
 * Caller guarantees that no more memory from the pool is in use,
 * and that nothing will try to use the pool after this call.
 */
void dma_pool_destroy(struct dma_pool *pool)
{
	bool empty = false;
	int list_idx;

	if (unlikely(!pool))
		return;

	mutex_lock(&pools_reg_lock);
	mutex_lock(&pools_lock);
	list_del(&pool->pools);
	if (list_empty(&pool->dev->dma_pools))
		empty = true;
	mutex_unlock(&pools_lock);
	if (empty)
		device_remove_file(pool->dev, &dev_attr_pools);
	mutex_unlock(&pools_reg_lock);

	for (list_idx = 0; list_idx < POOL_MAX_IDX; list_idx++) {
		struct page *page;

		while ((page = list_first_entry_or_null(
					&pool->page_list[list_idx],
					struct page,
					dma_list))) {
			pool_free_page(pool, page);
		}
	}

	kfree(pool);
}
EXPORT_SYMBOL(dma_pool_destroy);

/**
 * dma_pool_alloc - get a block of consistent memory
 * @pool: dma pool that will produce the block
 * @mem_flags: GFP_* bitmask
 * @handle: pointer to dma address of block
 *
 * This returns the kernel virtual address of a currently unused block,
 * and reports its dma address through the handle.
 * If such a memory block can't be allocated, %NULL is returned.
 */
void *dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
		     dma_addr_t *handle)
{
	unsigned long flags;
	struct page *page;
	unsigned int offset;
	void *retval;
	void *vaddr;

	might_sleep_if(gfpflags_allow_blocking(mem_flags));

	spin_lock_irqsave(&pool->lock, flags);
	page = list_first_entry_or_null(&pool->page_list[POOL_AVAIL_IDX],
					struct page,
					dma_list);
	if (page)
		goto ready;

	/* pool_alloc_page() might sleep, so temporarily drop &pool->lock */
	spin_unlock_irqrestore(&pool->lock, flags);

	page = pool_alloc_page(pool, mem_flags & (~__GFP_ZERO));
	if (!page)
		return NULL;

	spin_lock_irqsave(&pool->lock, flags);

	list_add(&page->dma_list, &pool->page_list[POOL_AVAIL_IDX]);
 ready:
	vaddr = page_to_virt(page);
	page->dma_in_use++;
	offset = page->dma_free_off;
	page->dma_free_off = *(int *)(vaddr + offset);
	if (page->dma_free_off >= pool->allocation)
		/* Move page from the "available" list to the "full" list. */
		list_move_tail(&page->dma_list,
			       &pool->page_list[POOL_FULL_IDX]);
	retval = offset + vaddr;
	*handle = offset + page->dma;
#ifdef	DMAPOOL_DEBUG
	{
		int i;
		u8 *data = retval;
		/* page->dma_free_off is stored in first 4 bytes */
		for (i = sizeof(page->dma_free_off); i < pool->size; i++) {
			if (data[i] == POOL_POISON_FREED)
				continue;
			dev_err(pool->dev,
				"dma_pool_alloc %s, %p (corrupted)\n",
				pool->name, retval);

			/*
			 * Dump the first 4 bytes even if they are not
			 * POOL_POISON_FREED
			 */
			print_hex_dump(KERN_ERR, "", DUMP_PREFIX_OFFSET, 16, 1,
					data, pool->size, 1);
			break;
		}
	}
	if (!(mem_flags & __GFP_ZERO))
		memset(retval, POOL_POISON_ALLOCATED, pool->size);
#endif
	spin_unlock_irqrestore(&pool->lock, flags);

	if (mem_flags & __GFP_ZERO)
		memset(retval, 0, pool->size);

	return retval;
}
EXPORT_SYMBOL(dma_pool_alloc);

/**
 * dma_pool_free - put block back into dma pool
 * @pool: the dma pool holding the block
 * @vaddr: virtual address of block
 * @dma: dma address of block
 *
 * Caller promises neither device nor driver will again touch this block
 * unless it is first re-allocated.
 */
void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma)
{
	struct page *page;
	unsigned long flags;
	unsigned int offset;

	if (unlikely(!virt_addr_valid(vaddr))) {
		dev_err(pool->dev,
			"dma_pool_free %s, %p (bad vaddr)/%pad\n",
			pool->name, vaddr, &dma);
		return;
	}

	page = virt_to_page(vaddr);
	offset = offset_in_page(vaddr);

	if (unlikely((dma - page->dma) != offset)) {
		dev_err(pool->dev,
			"dma_pool_free %s, %p (bad vaddr)/%pad (or bad dma)\n",
			pool->name, vaddr, &dma);
		return;
	}

	spin_lock_irqsave(&pool->lock, flags);
#ifdef	DMAPOOL_DEBUG
	{
		void *page_vaddr = vaddr - offset;
		unsigned int chain = page->dma_free_off;
		while (chain < pool->allocation) {
			if (chain != offset) {
				chain = *(int *)(page_vaddr + chain);
				continue;
			}
			spin_unlock_irqrestore(&pool->lock, flags);
			dev_err(pool->dev,
				"dma_pool_free %s, dma %pad already free\n",
				pool->name, &dma);
			return;
		}
	}
	memset(vaddr, POOL_POISON_FREED, pool->size);
#endif

	page->dma_in_use--;
	if (page->dma_free_off >= pool->allocation)
		/* Move page from the "full" list to the "available" list. */
		list_move(&page->dma_list, &pool->page_list[POOL_AVAIL_IDX]);
	*(int *)vaddr = page->dma_free_off;
	page->dma_free_off = offset;
	/*
	 * Resist a temptation to do
	 *    if (!is_page_busy(page)) pool_free_page(pool, page);
	 * Better have a few empty pages hang around.
	 */
	spin_unlock_irqrestore(&pool->lock, flags);
}
EXPORT_SYMBOL(dma_pool_free);

/*
 * Managed DMA pool
 */
static void dmam_pool_release(struct device *dev, void *res)
{
	struct dma_pool *pool = *(struct dma_pool **)res;

	dma_pool_destroy(pool);
}

static int dmam_pool_match(struct device *dev, void *res, void *match_data)
{
	return *(struct dma_pool **)res == match_data;
}

/**
 * dmam_pool_create - Managed dma_pool_create()
 * @name: name of pool, for diagnostics
 * @dev: device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @allocation: returned blocks won't cross this boundary (or zero)
 *
 * Managed dma_pool_create().  DMA pool created with this function is
 * automatically destroyed on driver detach.
 */
struct dma_pool *dmam_pool_create(const char *name, struct device *dev,
				  size_t size, size_t align, size_t allocation)
{
	struct dma_pool **ptr, *pool;

	ptr = devres_alloc(dmam_pool_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	pool = *ptr = dma_pool_create(name, dev, size, align, allocation);
	if (pool)
		devres_add(dev, ptr);
	else
		devres_free(ptr);

	return pool;
}
EXPORT_SYMBOL(dmam_pool_create);

/**
 * dmam_pool_destroy - Managed dma_pool_destroy()
 * @pool: dma pool that will be destroyed
 *
 * Managed dma_pool_destroy().
 */
void dmam_pool_destroy(struct dma_pool *pool)
{
	struct device *dev = pool->dev;

	WARN_ON(devres_release(dev, dmam_pool_release, dmam_pool_match, pool));
}
EXPORT_SYMBOL(dmam_pool_destroy);
