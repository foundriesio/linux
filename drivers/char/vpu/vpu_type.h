/*
 *   FileName    : vpu_type.h
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

#ifndef _VPU_TYPE_H_
#define _VPU_TYPE_H_

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC901X)
#define VPU_D6
#else
#define VPU_C7
#endif
#define JPU_C6

#if !defined(CONFIG_ARCH_TCC897X)
#define VIDEO_IP_DIRECT_RESET_CTRL
#endif
//#define VBUS_CLK_ALWAYS_ON
#define VBUS_CODA_CORE_CLK_CTRL

#endif /* _VPU_TYPE_H_ */

