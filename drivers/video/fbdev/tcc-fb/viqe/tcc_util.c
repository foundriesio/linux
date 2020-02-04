/*
 * linux/drivers/video/tcc/viqe/tcc_util.c
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
#include "tcc_util.h"

const char *MMOUNT = "/proc/mounts";

MOUNTP *dfopen()
{
	MOUNTP *MP;    // /proc/mounts 파일을 연다.
	MP = (MOUNTP *)malloc(sizeof(MOUNTP));
	if(!(MP->fp = fopen(MMOUNT, "r")))
	{
		free(MP);
		return NULL;
	}
	else
		return MP;
}

MOUNTP *dfget(MOUNTP *MP, int deviceType)
{
	char buf[256];
	char *bname;
	char null[16];
	struct statfs lstatfs;
	struct stat lstat;
	int is_root = 0;

	// /proc/mounts로 부터 마운트된 파티션의 정보를 얻어온다.
	while(fgets(buf, 255, MP->fp))
	{
		is_root = 0;
		sscanf(buf, "%s%s%s",MP->devname, MP->mountdir, MP->fstype);
	#if 0
		if (strcmp(MP->mountdir,"/") == 0)
			is_root=1;
	#endif

		if(deviceType == 0 && strcmp(MP->mountdir, "/nand") != 0)
			continue;
		else if(deviceType == 1 && strcmp(MP->mountdir, "/sdcard") != 0)
			continue;

		if (stat(MP->devname, &lstat) == 0 || is_root)
		{
			if (strstr(buf, MP->mountdir) && S_ISBLK(lstat.st_mode) || is_root)
			{
				// 파일시스템의 총 할당된 크기와 사용량을 구한다.
				statfs(MP->mountdir, &lstatfs);
				MP->size.blocks = lstatfs.f_blocks * (lstatfs.f_bsize/1024);
				MP->size.avail  = lstatfs.f_bavail * (lstatfs.f_bsize/1024);
				rewind(MP->fp);
				return MP;
			}
		}
	}
	rewind(MP->fp);
	return NULL;
}

int dfclose(MOUNTP *MP)
{
	if(MP->fp != 0)
		fclose(MP->fp);
	free(MP);
	return 1;
}

enum
{
	PHYMEM_GET_PMEM,
	PHYMEM_GET_VIQE,
	PHYMEM_GET_CAMERA,
	PHYMEM_GET_OVERLAY,
	PHYMEM_GET_VIDEO,
	PHYMEM_GET_FB,
	PHYMEM_GET_JPEG_HEADER,
	PHYMEM_GET_JPEG_RAW,
	PHYMEM_GET_JPEG_STREAM,		
	PHYMEM_GET_MAX
};

int tccutil_get_phy_mem_info(unsigned int cmd, unsigned int *puiBase, unsigned int *puiSize)
{
	FILE *fp;
	char buf[1024], nametofind[64];
	const char *p;
	ssize_t nbytes;
	int matches;
	unsigned int base_addr;
	unsigned int size;
	char name[128];

	if(!(fp = fopen("/proc/phys_mem", "r")))		
	{
		pr_err("[ERR][UTIL] /proc/phys_mem Open Error!!![%d]", cmd);
		return 1;
	}
	
	nbytes = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);
	p = buf;
	switch(cmd)
	{
	case PHYMEM_GET_PMEM:
		strcpy(nametofind, "pmem");
		break;
	case PHYMEM_GET_VIQE:
		strcpy(nametofind, "viqe");
		break;
	case PHYMEM_GET_CAMERA:
		strcpy(nametofind, "camera");
		break;
	case PHYMEM_GET_OVERLAY:
		strcpy(nametofind, "overlay");
		break;
	case PHYMEM_GET_VIDEO:
		strcpy(nametofind, "video");
		break;
	case PHYMEM_GET_FB:
		strcpy(nametofind, "fb");
		break;
	case PHYMEM_GET_JPEG_HEADER:
		strcpy(nametofind, "jpeg_header");
		break;
	case PHYMEM_GET_JPEG_RAW:
		strcpy(nametofind, "jpeg_raw");
		break;
	case PHYMEM_GET_JPEG_STREAM:
		strcpy(nametofind, "jpeg_stream");
		break;		
	default:
		return 1;
		break;		
	}
	
	while(1) 
	{
		matches = sscanf(p, "%x %x %s", &base_addr, &size, name);
		if (matches == 3) 
		{			
			if(strcmp(name, nametofind) == 0)
			{
				pr_info("[INF][UTIL] %s: 0x%x 0x%x\n", name, base_addr, size);
				*puiBase = base_addr;
				*puiSize = size;
				return 0;
			}				
		}
		p = strchr(p, '\n');
		if (p == NULL)
			break;
		p++;
	}

	pr_err("[ERR][UTIL] %s :: Getting information fail(%d)", __func__, cmd);
	return 1; //error can't find
}

int TCCUTIL_PHYGetVideo(unsigned int *puiBase, unsigned int *puiSize)
{
	return tccutil_get_phy_mem_info(PHYMEM_GET_VIDEO, puiBase, puiSize);
}

int TCCUTIL_PHYGetCamera(unsigned int *puiBase, unsigned int *puiSize)
{
	return tccutil_get_phy_mem_info(PHYMEM_GET_CAMERA, puiBase, puiSize);
}

int TCCUTIL_PHYGetOverlay(unsigned int *puiBase, unsigned int *puiSize)
{
	return tccutil_get_phy_mem_info(PHYMEM_GET_OVERLAY, puiBase, puiSize);
}

int TCCUTIL_PHYGetViqe(unsigned int *puiBase, unsigned int *puiSize)
{
	return tccutil_get_phy_mem_info(PHYMEM_GET_VIQE, puiBase, puiSize);
}

