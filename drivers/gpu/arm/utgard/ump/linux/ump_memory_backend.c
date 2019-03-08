/*
 * Copyright (C) 2013~2017 Telechips Inc.
 * Copyright (C) 2010, 2013-2014, 2017 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>            /* kernel module definitions */
#include <linux/ioport.h>            /* request_mem_region */

#include "config.h"                  /* Configuration for current platform. The symlink for arch is set by Makefile */

#include "ump_osk.h"
#include "ump_kernel_common.h"
#include "ump_kernel_memory_backend_os.h"
#include "ump_kernel_memory_backend_dedicated.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)   /*TCC*/
#include <plat/pmap.h>
#else
#include <soc/tcc/pmap.h>
#endif

/* Configure which dynamic memory allocator to use */
//int ump_backend = ARCH_UMP_BACKEND_DEFAULT;
//module_param(ump_backend, int, S_IRUGO); /* r--r--r-- */
//MODULE_PARM_DESC(ump_backend, "0 = dedicated memory backend (default), 1 = OS memory backend");

/* The base address of the memory block for the dedicated memory backend */
unsigned int ump_memory_address = ARCH_UMP_MEMORY_ADDRESS_DEFAULT;
module_param(ump_memory_address, uint, S_IRUGO); /* r--r--r-- */
MODULE_PARM_DESC(ump_memory_address, "The physical address to map for the dedicated memory backend");

/* The size of the memory block for the dedicated memory backend */
unsigned int ump_memory_size = ARCH_UMP_MEMORY_SIZE_DEFAULT;
module_param(ump_memory_size, uint, S_IRUGO); /* r--r--r-- */
MODULE_PARM_DESC(ump_memory_size, "The size of fixed memory to map in the dedicated memory backend");

ump_memory_backend* ump_memory_backend_create ( int ump_backend  )
{
	ump_memory_backend *backend = NULL;

	/* Create the dynamic memory allocator backend */
	if (0 == ump_backend)
	{
		pmap_t pmap_ump_reserved;

		if(0 > pmap_get_info("ump_reserved", &pmap_ump_reserved)){
			printk("%s-%d : ump_reserved allocation is failed.\n", __func__, __LINE__);
			return NULL;
		}
		ump_memory_address = pmap_ump_reserved.base;
		ump_memory_size = pmap_ump_reserved.size;
		DBG_MSG(2, ("Using dedicated memory backend\n"));

		DBG_MSG(2, ("Requesting dedicated memory: 0x%08x, size: %u\n", ump_memory_address, ump_memory_size));
		/* Ask the OS if we can use the specified physical memory */
		if (NULL == request_mem_region(ump_memory_address, ump_memory_size, "UMP Memory"))
		{
			MSG_ERR(("Failed to request memory region (0x%08X - 0x%08X). Is Mali DD already loaded?\n", ump_memory_address, ump_memory_address + ump_memory_size - 1));
			return NULL;
		}
		backend = ump_block_allocator_create(ump_memory_address, ump_memory_size);
	}
	else if (1 == ump_backend)
	{
		DBG_MSG(2, ("Using OS memory backend, allocation limit: %d\n", ump_memory_size));
		backend = ump_os_memory_backend_create(ump_memory_size);
	}

	return backend;
}

void ump_memory_backend_destroy( int ump_backend )
{
	if (0 == ump_backend)
	{
		DBG_MSG(2, ("Releasing dedicated memory: 0x%08x\n", ump_memory_address));
		release_mem_region(ump_memory_address, ump_memory_size);
		pmap_release_info("ump_reserved");
	}
}
