/*
 * linux/drivers/video/tcc/viqe/tcc_util.h
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
#ifndef	_TCC_UTIL_H__
#define	_TCC_UTIL_H__

/******************************************************************************
* include 
******************************************************************************/
//#include <stdio.h>
//#include <stdlib.h>
//#include <libgen.h>
//#include <sys/vfs.h>
//#include <sys/stat.h>
//#include <sys/types.h>

//#include <unistd.h>

/******************************************************************************
* typedefs & structure
******************************************************************************/
struct f_size{
	long blocks;
	long avail;
};

typedef struct _mountinfo {
	FILE *fp;			// 파일 스트림 포인터
	char devname[80];	// 장치 이름
	char mountdir[80];	// 마운트 디렉토리 이름
	char fstype[12];	// 파일 시스템 타입
	struct f_size size;	// 파일 시스템의 총크기/사용율
} MOUNTP;

/******************************************************************************
* defines 
******************************************************************************/

/******************************************************************************
* declarations
******************************************************************************/
MOUNTP *dfopen();
MOUNTP *dfget(MOUNTP *MP, int deviceType);
int dfclose(MOUNTP *MP);

#endif //_TCC_UTIL_H__
