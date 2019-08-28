/*
 *   FileName    : TCC_VPU_CODEC.h
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

#include "autoconf.h"

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC803X)
#include "TCC_VPU_C7_CODEC.h"
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC901X)
#include "TCC_VPU_D6.h"
#endif

#ifndef _TCC_VPU_CODEC_H_
#define _TCC_VPU_CODEC_H_

#ifndef RETCODE_MULTI_CODEC_EXIT_TIMEOUT
#define RETCODE_MULTI_CODEC_EXIT_TIMEOUT    99
#endif

#ifndef SZ_1M
#define SZ_1M   (1024 * 1024)
#endif

#define VPU_ENC_PUT_HEADER_SIZE (16 * 1024)

#endif // _TCC_VPU_CODEC_H_
