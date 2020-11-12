/*
 * Copyright (C) Telechips, Inc.
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
#ifndef __TCC_WDMA_IOCTRL_H__
#define __TCC_WDMA_IOCTRL_H__

#define WDMA_IOC_MAGIC  'w'


#define TCC_WDMA_IOCTRL 0x9003
#define TCC_WDMA_START  0x9002
#define TCC_WDMA_END    0x9001


struct vioc_wdma_frame_info {
	unsigned int buff_addr;
	unsigned int buff_size;
	unsigned int frame_fmt;
	unsigned int frame_x;
	unsigned int frame_y;
	unsigned int buffer_num; //buffer number
};

struct vioc_wdma_get_buffer {
	unsigned int buff_Yaddr;
	unsigned int buff_Uaddr;
	unsigned int buff_Vaddr;
	unsigned int frame_fmt;
	unsigned int frame_x;
	unsigned int frame_y;
	int index; //if index >0  is success
};


#define TC_WDRV_COUNT_START			0x8000
#define TC_WDRV_COUNT_GET_DATA		0x8001
#define TC_WDRV_GET_CUR_DATA		0x8011
#define TC_WDRV_COUNT_END			0x8002

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT  (0x8-1)
#define BIT_0      (3)
#define GET_ADDR_YUV42X_spY(Base_addr) \
	(((((unsigned int)Base_addr) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) \
	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#endif

enum WDMA_RESPONSE_TYPE {
	WDMA_POLLING,
	WDMA_INTERRUPT,
	WDMA_NOWAIT
};

#endif

