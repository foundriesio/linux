/****************************************************************************
linux/include/video/tcc/cam_ipc__mgr.h
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
#ifndef __CAM_IPC_MGR_H__
#define __CAM_IPC_MGR_H__

#define CAM_IPC_MGR_SEND		0x1
#define CAM_IPC_MGR_ACK		0x10
#define CAM_IPC_MGR_STATUS	0x11

/* control commands */
enum {
	CAM_IPC_CMD_NULL = 0,
	CAM_IPC_CMD_OVP,
	CAM_IPC_CMD_POS,
	CAM_IPC_CMD_RESET,
	CAM_IPC_CMD_READY,
	CAM_IPC_CMD_STATUS,
	CAM_IPC_CMD_MAX,
};

/* driver status */
enum {
	CAM_IPC_STS_NULL = 0,
	CAM_IPC_STS_INIT,
	CAM_IPC_STS_READY,
	CAM_IPC_MAX_STS,
};


#define CAM_IPC_MGR_MAGIC 'I'
#define IOCTL_CAM_IPC_MGR_SET_OVP	_IO(CAM_IPC_MGR_MAGIC ,1)
#define IOCTL_CAM_IPC_MGR_SET_POS	_IO(CAM_IPC_MGR_MAGIC ,2)
#define IOCTL_CAM_IPC_MGR_SET_RESET	_IO(CAM_IPC_MGR_MAGIC ,3)
#define IOCTL_CAM_IPC_MGR_SET_READY	_IO(CAM_IPC_MGR_MAGIC ,4)

#define IOCTL_CAM_IPC_MGR_SET_OVP_KERNEL		0x100
#define IOCTL_CAM_IPC_MGR_SET_POS_KERNEL		0x101
#define IOCTL_CAM_IPC_MGR_SET_RESET_KERNEL		0x102
#define IOCTL_CAM_IPC_MGR_SET_READY_KERNEL		0x103
#endif
