// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_I2C_SLAVE_H
#define TCC_I2C_SLAVE_H

#define TCC_I2C_SLAVE_IOCTL	253

/* Set and get address */
#define IOCTL_I2C_SLAVE_SET_ADDR		_IO(TCC_I2C_SLAVE_IOCTL, 1)
#define IOCTL_I2C_SLAVE_GET_ADDR		_IO(TCC_I2C_SLAVE_IOCTL, 2)
/* Access memory buffer (MB0/MB1), sub address from 0x00 to 0x07 */
#define IOCTL_I2C_SLAVE_SET_MB			_IO(TCC_I2C_SLAVE_IOCTL, 3)
#define IOCTL_I2C_SLAVE_GET_MB			_IO(TCC_I2C_SLAVE_IOCTL, 4)

#define IOCTL_I2C_SLAVE_SET_POLL_CNT	_IO(TCC_I2C_SLAVE_IOCTL, 5)

#endif /* TCC_I2C_SLAVE_H */
