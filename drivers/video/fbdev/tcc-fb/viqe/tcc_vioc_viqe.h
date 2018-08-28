/****************************************************************************
linux/drivers/video/tcc/viqe/tcc_vioc_viqe.h
Description: TCC VIOC h/w block 

Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#define IOCTL_VIQE_OFF				0xA10
#define IOCTL_VIQE_ON				0xA20
#define IOCTL_VIQE_SET_INIT			0xA30
#define IOCTL_VIQE_SET_FRMINFO		0xA40
#define IOCTL_VIQE_SET_DMA			0xA50
#define IOCTL_VIQE_SET_MODE			0xA60
#define IOCTL_VIQE_RESTART			0xA70
#define IOCTL_VIQE_FINALIZE			0xA80
#define IOCTL_VIQE_REGION			0xA90
#define IOCTL_VIQE_STRENGTH			0xAA0
#define IOCTL_VIQE_SWRESET			0xAB0
#define IOCTL_VIQE_SET_STATE		0xAC0
#define IOCTL_VIQE_DEINT_BYPASS		0xAD0
/*****************************************************************************
*
* structures
*
******************************************************************************/
typedef enum
{
	VIQE_MODE_NONE		= 1 << 0,
	VIQE_MODE_DI		= 1 << 1,
	VIQE_MODE_DISP		= 1 << 2,
	VIQE_MODE_DNTS		= 1 << 3,
	VIQE_MODE_LPF		= 1 << 4,
	VIQE_MODE_HPF		= 1 << 5,
	VIQE_MODE_HIST		= 1 << 6,
	VIQE_MODE_ODD_FIRST	= 1 << 7,
	VIQE_MODE_GAMUT_A1	= 1 << 8,
	VIQE_MODE_GAMUT_A2	= 1 << 9,
	VIQE_MODE_MAXNUM
} VIQEMODE;


typedef struct 
{
	int Mode;
	int StrengthY;	
	int StrengthC;	
}viqe_strengthInfo;


typedef struct{
	int di;
	int dnts;
	int pf;
	int hist;
}viqe_proregion_t;

int tcc_viqe_ctrl(unsigned int cmd, void *arg);

