/*
 * linux/include/linux/i2c/tcc_i2c_slave.h
 *
 * Copyright (C) 2017 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __TCC_I2C_SLAVE_H__
#define __TCC_I2C_SLAVE_H__

#define TCC_I2C_SLAVE_IOCTL	253

/* Set and get address */
#define IOCTL_I2C_SLAVE_SET_ADDR		_IO(TCC_I2C_SLAVE_IOCTL, 1)
#define IOCTL_I2C_SLAVE_GET_ADDR		_IO(TCC_I2C_SLAVE_IOCTL, 2)
/* Access memory buffer (MB0/MB1), sub address from 0x00 to 0x07 */
#define IOCTL_I2C_SLAVE_SET_MB			_IO(TCC_I2C_SLAVE_IOCTL, 3)
#define IOCTL_I2C_SLAVE_GET_MB			_IO(TCC_I2C_SLAVE_IOCTL, 4)

#define IOCTL_I2C_SLAVE_SET_POLL_CNT	_IO(TCC_I2C_SLAVE_IOCTL, 5)

#endif
