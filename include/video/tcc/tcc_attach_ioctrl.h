/*
 * Copyright (C) Telechips Inc.
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

#ifndef _TCC_ATTACH_IOCTL_H
#define _TCC_ATTACH_IOCTL_H

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT 							(0x8-1)
#define BIT_0 								(3)
#define GET_ADDR_YUV42X_spY(Base_addr) 		(((((unsigned int)Base_addr) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) 	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) 	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) 	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#endif

#define ATTACH_BUF_NUM		(3)
#define ATTACH_NONE_MODE		(0)
#define ATTACH_PREVIEW_MODE		(1)
#define ATTACH_CAPTURE_MODE		(2)

typedef struct {
	unsigned int 			addr_y[ATTACH_BUF_NUM];		// destination image address
	unsigned int 			addr_u[ATTACH_BUF_NUM];		// destination image address
	unsigned int 			addr_v[ATTACH_BUF_NUM]; 	// destination image address
	unsigned int 			fmt;		// destination image format
	unsigned int 			img_width;		// destination image width
	unsigned int 			img_height;		// destination image height
	unsigned int 			offset_x;		// destination image position
	unsigned int 			offset_y;		// destination image position
	unsigned int			mode;
} ATTACH_INFO_TYPE;

#define TCC_ATTACH_IOCTRL 					0x01
#define TCC_ATTACH_IOCTRL_KERNEL 			0x02
#define TCC_ATTACH_GET_BUF_IOCTRL 			0x04
#define TCC_ATTACH_GET_BUF_IOCTRL_KERNEL 	0x08

#endif
