/*
 *   FileName    : vpu_structure.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _STRUCTURES_VIDEO_H_
#define _STRUCTURES_VIDEO_H_

#include "vpu_type.h"

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#define HwVIDEOBUSCONFIG_BASE                   (0x75100000)
#else //CONFIG_ARCH_TCC898X , CONFIG_ARCH_TCC802X
	#if defined(CONFIG_ARCH_TCC896X)
	#define HwVIDEOBUSCONFIG_BASE               (0x15200000)
	#else
	#define HwVIDEOBUSCONFIG_BASE               (0x15100000)
	#endif
#endif

#endif /* _STRUCTURES_VIDEO_H_ */

