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

/****************************************************************************

drivers/media/video/tcccam/hdmi_in/ep9553e.h

Register definitions for the ep9553E HDMI Receiver Chip.

Author: swhwang (swhwang@telechips.com)

Copyright (C) 2013 Telechips, Inc.

****************************************************************************/

#include <generated/autoconf.h>

#include "../tcc_hdin_ctrl.h"

#ifndef __EP9553T_H__
#define __EP9553T_H__


#define SENSOR_I2C_ADDR 0x78

#define SENSOR_FRAMERATE	60

#define MODULE_NODE		"ep9553t"

#define true 1
#define false 0

extern void module_init_fnc(MODULE_FUNC_TYPE *sensor_func);

#endif /*__EP9553T_H__*/

