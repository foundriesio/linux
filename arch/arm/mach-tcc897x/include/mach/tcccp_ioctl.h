/*
 * File:        arch/arm/mach-tcc893x/include/mach/cp_ioctl.h
 */
/****************************************************************************
One line to give the program's name and a brief idea of what it does.
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


#ifndef __TCC_CP_DEV_H__
#define __TCC_CP_DEV_H__

#define CP_DEV_MAJOR_NUM 237
#define CP_DEV_MINOR_NUM 1

#define IOCTL_CP_CTRL_INIT   	      _IO(CP_DEV_MAJOR_NUM, 100)
#define IOCTL_CP_CTRL_PWR             	_IO(CP_DEV_MAJOR_NUM, 101)
#define IOCTL_CP_CTRL_RESET               _IO(CP_DEV_MAJOR_NUM, 102)
#define IOCTL_CP_CTRL_ALL               _IO(CP_DEV_MAJOR_NUM, 103)
#define IOCTL_CP_GET_VERSION         _IO(CP_DEV_MAJOR_NUM, 104)
#define IOCTL_CP_GET_CHANNEL         _IO(CP_DEV_MAJOR_NUM, 105)
#define IOCTL_CP_SET_STATE         _IO(CP_DEV_MAJOR_NUM, 106)
#define IOCTL_CP_GET_STATE         _IO(CP_DEV_MAJOR_NUM, 107)
#define IOCTL_CP_GET_WITH_IAP2       _IO(CP_DEV_MAJOR_NUM, 108)

#endif  /* __TCC_CP_DEV_H__ */

