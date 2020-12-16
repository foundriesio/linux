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
#ifndef __TCC_VIQE_IOCTL_H__
#define __TCC_VIQE_IOCTL_H__

#define VIQE_DEV_NAME		"viqe"
#define VIQE_DEV_MAJOR		230
#define VIQE_DEV_MINOR		0

#define IOCTL_VIQE_INITIALIZE	0xAF0
#define IOCTL_VIQE_EXCUTE		0xAF1
#define IOCTL_VIQE_DEINITIALIZE	0xAF2
#define IOCTL_VIQE_SETTING		0xAF3

#define IOCTL_VIQE_INITIALIZE_KERNEL	0xBF0
#define IOCTL_VIQE_EXCUTE_KERNEL		0xBF1
#define IOCTL_VIQE_DEINITIALIZE_KERNEL	0xBF2
#define IOCTL_VIQE_SETTING_KERNEL		0xBF3

struct VIQE_DI_TYPE {
	int lcdCtrlNo;
	int scalerCh;
	int onTheFly;
	unsigned int use_sDeintls;
	unsigned int srcWidth;
	unsigned int srcHeight;
	unsigned int crop_top, crop_bottom, crop_left, crop_right;
	int deinterlaceM2M;
	int renderCnt;
	int OddFirst;
	int DI_use;
	int multi_hwr;

	unsigned int address[6];
	unsigned int dstAddr;

	// only for Testing!!
	unsigned int nRDMA;
	unsigned int use_Viqe0;
};

#endif

