/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
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

#ifndef MAX9286_H
#define MAX9286_H

#include "../videosource_types.h"

extern struct videosource videosource_max9286;

// MAX9286 Standard Selection Register Bits
#define VIDEO_STD_NTSC_M_BIT	0x0
#define VIDEO_STD_NTSC_443_BIT	0x3

#define MAX9286_STATUS_1		0x03
#define MAX9286_STD_SELECTION	0x1C

#endif//MAX9286_H
