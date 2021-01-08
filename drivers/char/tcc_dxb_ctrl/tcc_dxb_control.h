// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_DXB_CONTROL_H_
#define TCC_DXB_CONTROL_H_

enum {
	BOARD_DXB_UNDEFINED = 0,
/* don't not modify or remove above of this line.
 * when add new device, add to below of this line
 */
	BOARD_DXB_TCC3171 = 15,
	BOARD_AMFM_TUNER = 30
};

#define DXB_CTRL_DEV_FILE		("/dev/tcc_dxb_ctrl")
#define DXB_CTRL_DEV_NAME		("tcc_dxb_ctrl")
#define DXB_CTRL_DEV_MINOR		(0)

#define DXB_CTRL_IOCTL_BASE		(251)

#define IOCTL_DXB_CTRL_OFF          (_IO(DXB_CTRL_IOCTL_BASE, 1))
#define IOCTL_DXB_CTRL_ON           (_IO(DXB_CTRL_IOCTL_BASE, 2))
#define IOCTL_DXB_CTRL_RESET        (_IO(DXB_CTRL_IOCTL_BASE, 3))
#define IOCTL_DXB_CTRL_SET_BOARD    (_IO(DXB_CTRL_IOCTL_BASE, 4))
#define IOCTL_DXB_CTRL_RESET_LOW    (_IO(DXB_CTRL_IOCTL_BASE, 8))
#define IOCTL_DXB_CTRL_RESET_HIGH   (_IO(DXB_CTRL_IOCTL_BASE, 9))
#define IOCTL_DXB_CTRL_PURE_ON      (_IO(DXB_CTRL_IOCTL_BASE, 10))
#define IOCTL_DXB_CTRL_PURE_OFF     (_IO(DXB_CTRL_IOCTL_BASE, 11))

#endif // TCC_DXB_CONTROL_H_
