/*
 * Copyright (C) 2013~2015 Telechips Inc.
 * Copyright (C) 2010, 2012, 2014 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __ARCH_CONFIG_H__
#define __ARCH_CONFIG_H__

#ifdef CONFIG_UMP_OS_MEMORY
#define ARCH_UMP_BACKEND_DEFAULT          1
#else
#define ARCH_UMP_BACKEND_DEFAULT          0
#endif

#define ARCH_UMP_MEMORY_ADDRESS_DEFAULT   CONFIG_UMP_MEMORY_ADDRESS
#define ARCH_UMP_MEMORY_SIZE_DEFAULT      (CONFIG_UMP_MEMORY_SIZE * 1024UL * 1024UL)


#endif /* __ARCH_CONFIG_H__ */
