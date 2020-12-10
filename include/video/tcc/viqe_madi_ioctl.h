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
#ifndef _TCC_VIQE_MADI_IOCTL_H_
#define _TCC_VIQE_MADI_IOCTL_H_

#define TCC_VIQE_MADI_INIT   (0x100)
#define TCC_VIQE_MADI_DEINIT (0x102)

#define TCC_VIQE_MADI_PROC       (0x103)
#define TCC_VIQE_MADI_GET_RESULT (0x104)

#define MAX_SUPPORT_MADI_WIDTH  (1920)
#define MAX_SUPPORT_MADI_HEIGHT (1088)

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT   (0x8-1)
#define BIT_0       (3)
#define GET_ADDR_YUV42X_spY(Base_addr) \
	(((((unsigned int)Base_addr) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) \
	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#endif

enum VIQE_MADI_ResponseType {
	VIQE_MADI_POLLING,
	VIQE_MADI_INTERRUPT,
	VIQE_MADI_NOWAIT
};

enum VIQE_MADI_DATA_FMT {
	VIQE_MADI_YUV420_sp = 0,
	VIQE_MADI_YUV420_inter,
	VIQE_MADI_YUV422_sp,
	VIQE_MADI_YUV422_inter,
	VIQE_MADI_FMT_MAX
};

struct stVIQE_MADI_INIT_TYPE {
	unsigned int odd_first;
	unsigned int src_one_field_only_frame;
	enum VIQE_MADI_DATA_FMT src_fmt; // source image format
	unsigned int src_bit_depth;
	unsigned int src_ImgWidth;  // source image width
	unsigned int src_ImgHeight; // source image height
	unsigned int src_winLeft;
	unsigned int src_winTop;
	unsigned int src_winRight;
	unsigned int src_winBottom;
	enum VIQE_MADI_DATA_FMT dest_fmt; // destination image format
	unsigned int dest_bit_depth;
};

struct stVIQE_MADI_PROC_TYPE {
	enum VIQE_MADI_ResponseType responsetype; //scaler response type
	unsigned int second_field_proc;
	unsigned int src_Yaddr; // source address
	unsigned int src_Uaddr; // source address
	unsigned int src_Vaddr; // source address

// output address
	unsigned int dest_Yaddr; // width * height
	unsigned int dest_Caddr; // YUV420: (w * h) / 2, YUV422: w * h
	unsigned int dest_Aaddr; // (width * height) / 8
};

struct stVIQE_MADI_RESULT_TYPE {
	unsigned int dest_Yaddr; // target address
	unsigned int dest_Uaddr; // target address
	unsigned int dest_Vaddr; // target address
};

#endif
