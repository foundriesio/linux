/*
 * linux/include/video/tcc/vioc_v_dv.h
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
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
 */
#include <video/tcc/tccfb.h>

#ifdef CONFIG_ARCH_TCC898X
#include "tcc898x/vioc_v_dv.h"
#endif

#ifdef CONFIG_ARCH_TCC899X
#include "tcc899x/vioc_v_dv.h"
#include "tcc899x/vioc_dv_in.h"
#endif


#ifdef CONFIG_ARCH_TCC901X
#include "tcc901x/vioc_v_dv.h"
#include "tcc901x/vioc_dv_in.h"
#endif
//#define DOLBY_VISION_CHECK_SEQUENCE
#if defined(DOLBY_VISION_CHECK_SEQUENCE)
#define dprintk_dv_sequence(msg...) printk("" msg);
#else
#define dprintk_dv_sequence(msg...)
#endif

