/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef VIDEOSOURCE_I2C_H
#define VIDEOSOURCE_I2C_H

#include <linux/i2c.h>

#define REG_TERM 0xFF
#define VAL_TERM 0xFF

struct videosource_reg {
	unsigned short reg;
	unsigned short val;
};

extern int DDI_I2C_Write(struct i2c_client * client, char * buf, int reg_bytes, int val_bytes);
extern int DDI_I2C_Read(struct i2c_client * client, char * reg, int reg_bytes, char * val, int val_bytes);

#endif//VIDEOSOURCE_I2C_H

