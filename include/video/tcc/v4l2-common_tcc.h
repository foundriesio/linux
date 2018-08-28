/* linux/arch/arm/plat-tcc/v4l2-common_tcc.h
 *
 * Copyright (C) 2012 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __V4L2_COMMON_TCC_H__
#define __V4L2_COMMON_TCC_H__

extern unsigned int vioc2fourcc(unsigned int vioc_fmt);
extern unsigned int fourcc2vioc(unsigned int fourcc);

#endif //__V4L2_COMMON_TCC_H__