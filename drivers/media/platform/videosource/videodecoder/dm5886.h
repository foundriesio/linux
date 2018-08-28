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

#include "../videosource_if.h"

#ifndef DM5886_H
#define DM5886_H

#define REG_TERM 0xFF
#define VAL_TERM 0xFF

#define MODULE_NODE "atv_dm5886"
#define CAM_I2C_NAME "videosource,dm5886"

extern void sensor_info_init(TCC_SENSOR_INFO_TYPE *sensor_info);
extern void sensor_init_fnc(SENSOR_FUNC_TYPE *sensor_func);

#endif /*DM5886_H*/

