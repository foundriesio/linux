/*
 * tcc_dxb_control.h
 *
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux DxB Control DRIVER
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef     _TCC_DXB_CONTROL_H_
#define     _TCC_DXB_CONTROL_H_

typedef enum
{
	BOARD_DXB_UNDEFINED=0, /* don't not modify or remove this line. when add new device, add to below of this line */
	BOARD_DXB_TCC3171=15,
	BOARD_AMFM_TUNER=30,
	BOARD_MAX
}DXB_BOARD_TYPE;

#define DXB_RF_PATH_UHF 1
#define DXB_RF_PATH_VHF 2

typedef struct
{
	unsigned int uiI2C; //control channel of i2c
	unsigned int uiSPI; //control channel of spi
	unsigned int status;
}ST_CTRLINFO_ARG;

#define DXB_CTRL_DEV_FILE		"/dev/tcc_dxb_ctrl"
#define DXB_CTRL_DEV_NAME		"tcc_dxb_ctrl"
#define DXB_CTRL_DEV_MINOR		0

#define DXB_CTRL_IOCTL_BASE		251

#define IOCTL_DXB_CTRL_OFF		    _IO(DXB_CTRL_IOCTL_BASE, 1)
#define IOCTL_DXB_CTRL_ON			_IO(DXB_CTRL_IOCTL_BASE, 2)
#define IOCTL_DXB_CTRL_RESET    	_IO(DXB_CTRL_IOCTL_BASE, 3)
#define IOCTL_DXB_CTRL_SET_BOARD    _IO(DXB_CTRL_IOCTL_BASE, 4)
#define IOCTL_DXB_CTRL_GET_CTLINFO  _IO(DXB_CTRL_IOCTL_BASE, 5)
#define IOCTL_DXB_CTRL_RF_PATH      _IO(DXB_CTRL_IOCTL_BASE, 6)
#define IOCTL_DXB_CTRL_SET_CTRLMODE _IO(DXB_CTRL_IOCTL_BASE, 7)
#define IOCTL_DXB_CTRL_RESET_LOW	_IO(DXB_CTRL_IOCTL_BASE, 8)
#define IOCTL_DXB_CTRL_RESET_HIGH	_IO(DXB_CTRL_IOCTL_BASE, 9)
#define IOCTL_DXB_CTRL_PURE_ON		_IO(DXB_CTRL_IOCTL_BASE, 10)
#define IOCTL_DXB_CTRL_PURE_OFF		_IO(DXB_CTRL_IOCTL_BASE, 11)
#endif
