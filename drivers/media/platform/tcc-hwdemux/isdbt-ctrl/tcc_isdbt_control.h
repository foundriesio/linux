/*
 * tcc_isdbt_control.h
 *
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux ISDBT Control DRIVER
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software can redistribute it and/or modify
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

#ifndef     _TCC_ISDBT_CONTROL_H_
#define     _TCC_ISDBT_CONTROL_H_

typedef enum
{
	BOARD_ISDBT_TCC353X,
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

#define ISDBT_CTRL_DEV_FILE		"/dev/tcc_isdbt_ctrl"
#define ISDBT_CTRL_DEV_NAME		"tcc_isdbt_ctrl"
#define ISDBT_CTRL_DEV_MAJOR		250
#define ISDBT_CTRL_DEV_MINOR		0

#define IOCTL_DXB_CTRL_OFF		    _IO(ISDBT_CTRL_DEV_MAJOR, 1)
#define IOCTL_DXB_CTRL_ON			_IO(ISDBT_CTRL_DEV_MAJOR, 2)
#define IOCTL_DXB_CTRL_RESET    	_IO(ISDBT_CTRL_DEV_MAJOR, 3)
#define IOCTL_DXB_CTRL_SET_BOARD    _IO(ISDBT_CTRL_DEV_MAJOR, 4)
#define IOCTL_DXB_CTRL_GET_CTLINFO  _IO(ISDBT_CTRL_DEV_MAJOR, 5)
#define IOCTL_DXB_CTRL_RF_PATH      _IO(ISDBT_CTRL_DEV_MAJOR, 6)
#define IOCTL_DXB_CTRL_SET_CTRLMODE _IO(ISDBT_CTRL_DEV_MAJOR, 7)
#define IOCTL_DXB_CTRL_RESET_LOW	_IO(ISDBT_CTRL_DEV_MAJOR, 8)
#define IOCTL_DXB_CTRL_RESET_HIGH	_IO(ISDBT_CTRL_DEV_MAJOR, 9)
#define IOCTL_DXB_CTRL_PURE_ON		_IO(ISDBT_CTRL_DEV_MAJOR, 10)
#define IOCTL_DXB_CTRL_PURE_OFF		_IO(ISDBT_CTRL_DEV_MAJOR, 11)
/* add for HWDEMUX cipher */
#define IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_ALGORITHM	    _IO(ISDBT_CTRL_DEV_MAJOR, 12)
#define IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_KEY		    _IO(ISDBT_CTRL_DEV_MAJOR, 13)
#define IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_VECTOR	    _IO(ISDBT_CTRL_DEV_MAJOR, 14)
#define IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_DATA	    _IO(ISDBT_CTRL_DEV_MAJOR, 15)
#define IOCTL_DXB_CTRL_HWDEMUX_CIPHER_EXECUTE	    _IO(ISDBT_CTRL_DEV_MAJOR, 16)
#define IOCTL_DXB_CTRL_UNLOCK                       _IO(ISDBT_CTRL_DEV_MAJOR, 17)
#define IOCTL_DXB_CTRL_LOCK                         _IO(ISDBT_CTRL_DEV_MAJOR, 18)
#endif
