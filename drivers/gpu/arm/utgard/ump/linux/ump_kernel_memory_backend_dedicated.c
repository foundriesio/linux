/*
 * Copyright (C) 2013~2017 Telechips Inc.
 * Copyright (C) 2010-2011, 2013-2014, 2017 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* needed to detect kernel version specific code */
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else /* pre 2.6.26 the file was in the arch specific location */
#include <asm/semaphore.h>
#endif

#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <linux/vmalloc.h>
#include "ump_kernel_common.h"
#include "ump_kernel_memory_backend.h"



#define UMP_BLOCK_SIZE (256UL * 1024UL)  /* 256kB, remember to keep the ()s */
#define AUTO_FREE_LENGTH 12

typedef struct af_desc {
	ump_dd_mem *descriptor;
	int valid;
} af_desc;

typedef struct block_info
{
	struct block_info * next;
} block_info;

typedef struct block_allocator
{
	struct semaphore mutex;
	block_info * all_blocks;
	block_info * first_free;
	u32 first_free_start;
	u32 base;
	u32 num_blocks;
	u32 num_free;
} block_allocator;

static int af_desc_head = 0;
static int af_desc_tail = 0;
static af_desc af_descbuf[AUTO_FREE_LENGTH];

static void block_allocator_shutdown(ump_memory_backend * backend);
static int block_allocator_allocate(void* ctx, ump_dd_mem * mem);
static void block_allocator_release(void * ctx, ump_dd_mem * handle);
static inline u32 get_phys(block_allocator * allocator, block_info * block);
static u32 block_allocator_stat(struct ump_memory_backend *backend);


/*
 * Create dedicated memory backend
 */
ump_memory_backend * ump_block_allocator_create(u32 base_address, u32 size)
{
	ump_memory_backend * backend;
	block_allocator * allocator;
	u32 usable_size;
	u32 num_blocks;

	usable_size = (size + UMP_BLOCK_SIZE - 1) & ~(UMP_BLOCK_SIZE - 1);
	num_blocks = usable_size / UMP_BLOCK_SIZE;

	if (0 == usable_size)
	{
		DBG_MSG(1, ("Memory block of size %u is unusable\n", size));
		return NULL;
	}

	DBG_MSG(5, ("Creating dedicated UMP memory backend. Base address: 0x%08x, size: 0x%08x\n", base_address, size));
	DBG_MSG(6, ("%u usable bytes which becomes %u blocks\n", usable_size, num_blocks));

	backend = kzalloc(sizeof(ump_memory_backend), GFP_KERNEL);
	if (NULL != backend)
	{
		allocator = kmalloc(sizeof(block_allocator), GFP_KERNEL);
		if (NULL != allocator)
		{
			allocator->all_blocks = kmalloc(sizeof(block_allocator) * num_blocks, GFP_KERNEL);
			if (NULL != allocator->all_blocks)
			{
				int i;

				allocator->first_free = NULL;
				allocator->num_blocks = num_blocks;
				allocator->num_free = num_blocks;
				allocator->base = base_address;
				sema_init(&allocator->mutex, 1);

				for (i = num_blocks-1; i >= 0; i--)
				{
					allocator->all_blocks[i].next = allocator->first_free;
					allocator->first_free = &allocator->all_blocks[i];
				}
				allocator->first_free_start = (unsigned long)allocator->first_free;
				backend->ctx = allocator;
				backend->allocate = block_allocator_allocate;
				backend->release = block_allocator_release;
				backend->shutdown = block_allocator_shutdown;
				backend->stat = block_allocator_stat;
				backend->pre_allocate_physical_check = NULL;
				backend->adjust_to_mali_phys = NULL;

				return backend;
			}
			kfree(allocator);
		}
		kfree(backend);
	}

	return NULL;
}


/*
 * Destroy specified dedicated memory backend
 */
static void block_allocator_shutdown(ump_memory_backend * backend)
{
	block_allocator * allocator;

	BUG_ON(!backend);
	BUG_ON(!backend->ctx);

	allocator = (block_allocator*)backend->ctx;

	DBG_MSG_IF(1, allocator->num_free != allocator->num_blocks, ("%u blocks still in use during shutdown\n", allocator->num_blocks - allocator->num_free));

	kfree(allocator->all_blocks);
	kfree(allocator);
	kfree(backend);
}


static void block_free_desc(block_allocator *allocator, ump_dd_mem *descriptor)
{
	int i;
	block_info * block, * next;
	unsigned int temp1, temp2, temp3;
	unsigned int *temp4;

	block = (block_info*)descriptor->backend_info;

	while (block)
	{
		next = block->next;

		BUG_ON( (block < allocator->all_blocks) || (block > (allocator->all_blocks + allocator->num_blocks)));


		temp1 = (unsigned int)block + 4;
		temp2 = (unsigned int)allocator->first_free;
		
		if(temp1  == temp2)
		{
			block->next = allocator->first_free;
			allocator->first_free = block;
		}
		else
		{
			block->next =(block_info *)temp1;
			if(allocator->first_free_start != temp2)
				allocator->first_free = block;
		}
		allocator->num_free++;
		block = next;

	}

	for(i=0; i< allocator->num_blocks -1; i++)
	{
		temp3 = (unsigned int)&allocator->all_blocks[i];
		temp4 = (unsigned int*)allocator->all_blocks[i].next;
		if(temp3 + 4 == (unsigned int)temp4)
		{
			allocator->first_free = &allocator->all_blocks[i];
			DBG_MSG(5, ("test %d temp3:0x%08x, temp4:0x%08x\n",i, temp3, temp4));
			break;
		}
	}
	DBG_MSG(3, ("ID:%d, %d blocks free after release call, block:0x%08x\n", descriptor->secure_id, allocator->num_free, allocator->first_free));
	
	vfree(descriptor->block_array);
	descriptor->block_array = NULL;
	
}


static int is_af_descbuf_full(void)
{
	if (af_desc_tail == (af_desc_head + 1) % AUTO_FREE_LENGTH)
		return 1;
	return 0;
}


static void af_alloc(block_allocator *allocator, ump_dd_mem *descriptor)
{
	DBG_MSG(5, ("%s, autofree_enable%d\n", __func__, descriptor->autofree_enable));

	if(!descriptor->autofree_enable)
		return ;

	if (is_af_descbuf_full()) 
	{
		if (af_descbuf[af_desc_tail].valid) 
		{
			DBG_MSG(5, ("autofree before buffer allocation head=%d, tail=%d, nr=%d, alloc=%d, free=%d\n",af_desc_head, af_desc_tail, af_descbuf[af_desc_tail].descriptor->nr_blocks, allocator->num_blocks, allocator->num_free));
			block_free_desc(allocator, af_descbuf[af_desc_tail].descriptor);

			af_descbuf[af_desc_tail].descriptor = NULL;
			af_descbuf[af_desc_tail].valid = 0;
		}
		af_desc_tail = (af_desc_tail + 1) % AUTO_FREE_LENGTH;
	}
	af_descbuf[af_desc_head].descriptor = descriptor;
	af_descbuf[af_desc_head].valid = 1;
	af_desc_head = (af_desc_head + 1) % AUTO_FREE_LENGTH;
}


static int af_free(ump_dd_mem *descriptor)
{
	int i;
	DBG_MSG(5, ("%s, autofree_enable%d\n", __func__, descriptor->autofree_enable));
	
	if(!descriptor->autofree_enable)
		return 0;

	for (i = af_desc_tail; i != af_desc_head; i = (i + 1) % AUTO_FREE_LENGTH) 
	{
		if (af_descbuf[i].valid && af_descbuf[i].descriptor == descriptor) 
		{
			af_descbuf[i].descriptor = NULL;
			af_descbuf[i].valid = 0;
			DBG_MSG(5, ("%s Now Free\n", __func__));
			return 0;
		}
	}
	DBG_MSG(5, ("%s Already Free\n", __func__));
	return 1;	/* already free  */
}


static int block_auto_free(block_allocator *allocator, ump_dd_mem *descriptor)
{
	int i;
	int existingNumfree;

	existingNumfree = allocator->num_free;
	
	for (i = af_desc_tail; i != af_desc_head; i = (i + 1) % AUTO_FREE_LENGTH) 
	{
		if (af_descbuf[i].valid) 
		{
			block_free_desc(allocator, af_descbuf[af_desc_tail].descriptor);	
			af_descbuf[i].descriptor = NULL;
			af_descbuf[i].valid = 0;
		}
		af_desc_tail = (af_desc_tail + 1) % AUTO_FREE_LENGTH;
		if(allocator->num_free - existingNumfree >= descriptor->nr_blocks)
		{
			DBG_MSG(5, ("%s, Success af buffer head=%d, af buffer tail=%d, free=%d\n", __func__, af_desc_head, af_desc_tail, allocator->num_free));
			return 1;
		}
	}

	DBG_MSG(5, ("%s, Fail af buffer head=%d, af buffer tail=%d\n", __func__, af_desc_head, af_desc_tail));
	return 0;
}


static int block_allocator_allocate(void* ctx, ump_dd_mem * mem)
{
	block_allocator * allocator;
	u32 left;
	block_info * last_allocated = NULL;
	int i = 0;
	int free_en = 0;

	BUG_ON(!ctx);
	BUG_ON(!mem);

	allocator = (block_allocator*)ctx;
	left = mem->size_bytes;

	BUG_ON(!left);
	BUG_ON(!&allocator->mutex);

	mem->nr_blocks = ((left + UMP_BLOCK_SIZE - 1) & ~(UMP_BLOCK_SIZE - 1)) / UMP_BLOCK_SIZE;
	mem->block_array = (ump_dd_physical_block*)vmalloc(sizeof(ump_dd_physical_block) * mem->nr_blocks);
	if (NULL == mem->block_array)
	{
		MSG_ERR(("Failed to allocate block array\n"));
		return 0;
	}
	
	DBG_MSG(5, ("%s head=%d, tail=%d, left=%d, free=%d\n", __func__, af_desc_head, af_desc_tail, left, allocator->num_free));
	if (down_interruptible(&allocator->mutex))
	{
		MSG_ERR(("Could not get mutex to do block_allocate\n"));
		return 0;
	}

	if((allocator->num_free== 0) || (allocator->num_free <= mem->nr_blocks))
	{
		DBG_MSG(5, ("try autofree num_free:%d, nr_blocks=%d\n", allocator->num_free, mem->nr_blocks));
		if(block_auto_free(allocator, mem) == 0)
		{
			up(&allocator->mutex);
			return 0;
		}
	}
	
	mem->size_bytes = 0;

	//search for first-fit
	{
		int i, j;
		unsigned int temp1, temp2;
		unsigned int *temp3, *temp4;
		int check;

		for(i=0; i< allocator->num_blocks -1; i++)
		{
			temp1 = (unsigned int)allocator->first_free;
			if((unsigned int)allocator->first_free->next > temp1)//!=NULL)
			{
				temp2 = (unsigned int)allocator->first_free->next;
				temp3 = (unsigned int*)allocator->all_blocks[i].next;
				if(temp2 == (unsigned int) temp3)
				{
					check=0;
					for(j=i; j<mem->nr_blocks +i; j++)
					{
						if(j==allocator->num_blocks-1)
						{
							check++;
							break;
						}
						temp4 = (unsigned int*)allocator->all_blocks[j].next;
						if(temp4 == NULL)
						{
							allocator->first_free = &allocator->all_blocks[j+1];
							break;
						}
						else
							check++;
					}		
					if(check == mem->nr_blocks)
					{
						 free_en=1;
						break;
					}		
				}
			}
			else
			{
				for(j=i;j<allocator->num_blocks -1;j++)
				{
					if((unsigned int)allocator->all_blocks[j].next > temp1)
					{
						allocator->first_free = &allocator->all_blocks[j];
						i=j-1;
						break;
					}					
				}
			}
		}
	}	

	while ((left > 0) && (free_en))//(allocator->first_free))
	{
		block_info * block;

		block = allocator->first_free;
		allocator->first_free = allocator->first_free->next;
		block->next = last_allocated;
		last_allocated = block;
		allocator->num_free--;

		mem->block_array[i].addr = get_phys(allocator, block);
		mem->block_array[i].size = UMP_BLOCK_SIZE;
		mem->size_bytes += UMP_BLOCK_SIZE;

		i++;

		if (left < UMP_BLOCK_SIZE) left = 0;
		else left -= UMP_BLOCK_SIZE;
	}

	if (left)
	{
		block_info * block;
		/* release all memory back to the pool */
		while (last_allocated)
		{
			block = last_allocated->next;
			last_allocated->next = allocator->first_free;
			allocator->first_free = last_allocated;
			last_allocated = block;
			allocator->num_free++;
		}

		vfree(mem->block_array);
		mem->backend_info = NULL;
		mem->block_array = NULL;

		DBG_MSG(1, ("Could not find a mem-block for the allocation. left:%d\n", left));
		block_auto_free(allocator, mem);
		
		up(&allocator->mutex);

		return 0;
	}

	mem->backend_info = last_allocated;

	af_alloc(allocator, mem);
	
	DBG_MSG(5, ("alloc ID:%d, phy address: 0x%08x, size: 0x%08x, num_free:%d\n", mem->secure_id, mem->block_array[0].addr, mem->size_bytes, allocator->num_free));

	up(&allocator->mutex);
	mem->is_cached=0;

	return 1;
}



static void block_allocator_release(void * ctx, ump_dd_mem * handle)
{
	block_allocator * allocator;
	block_info * block;
	int already_free;

	BUG_ON(!ctx);
	BUG_ON(!handle);

	allocator = (block_allocator*)ctx;
	block = (block_info*)handle->backend_info;
	BUG_ON(!block);
	DBG_MSG(5, ("%s af=%d, free=%d\n", __func__, handle->autofree_enable, allocator->num_free));

	if (down_interruptible(&allocator->mutex))
	{
		MSG_ERR(("Allocator release: Failed to get mutex - memory leak\n"));
		return;
	}

	already_free = af_free(handle);

	if(!already_free)
		block_free_desc(allocator, handle);

	up(&allocator->mutex);	 
}



/*
 * Helper function for calculating the physical base adderss of a memory block
 */
static inline u32 get_phys(block_allocator * allocator, block_info * block)
{
	return allocator->base + ((block - allocator->all_blocks) * UMP_BLOCK_SIZE);
}

static u32 block_allocator_stat(struct ump_memory_backend *backend)
{
	block_allocator *allocator;
	BUG_ON(!backend);
	allocator = (block_allocator*)backend->ctx;
	BUG_ON(!allocator);

	return (allocator->num_blocks - allocator->num_free)* UMP_BLOCK_SIZE;
}
