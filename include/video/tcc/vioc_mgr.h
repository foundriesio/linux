/****************************************************************************
linux/include/video/tcc/vioc_mgr.h
Description: TCC Vsync Driver 

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
#ifndef __VIOC_MGR_H__
#define __VIOC_MGR_H__

#define VIOC_MGR_SEND		0x1
#define VIOC_MGR_ACK		0x10

/* control commands */
enum {
	VIOC_CMD_NULL = 0,
	VIOC_CMD_OVP,
	VIOC_CMD_POS,
	VIOC_CMD_RESET,
	VIOC_CMD_READY,
	VIOC_CMD_MAX,
};

/* driver status */
enum {
	VIOC_STS_NULL = 0,
	VIOC_STS_INIT,
	VIOC_STS_READY,
	VIOC_MAX_STS,
};


#define VIOC_MGR_MAGIC 'I'
#define IOCTL_VIOC_MGR_SET_OVP	_IO(VIOC_MGR_MAGIC ,1)
#define IOCTL_VIOC_MGR_SET_POS	_IO(VIOC_MGR_MAGIC ,2)
#define IOCTL_VIOC_MGR_SET_RESET	_IO(VIOC_MGR_MAGIC ,3)
#define IOCTL_VIOC_MGR_SET_READY	_IO(VIOC_MGR_MAGIC ,4)

#define IOCTL_VIOC_MGR_SET_OVP_KERNEL		0x100
#define IOCTL_VIOC_MGR_SET_POS_KERNEL		0x101
#define IOCTL_VIOC_MGR_SET_RESET_KERNEL		0x102
#define IOCTL_VIOC_MGR_SET_READY_KERNEL		0x103
#endif
