/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __TCC_SAR_H__
#define __TCC_SAR_H__

enum SARstrengh_CMD {
	SAR_LEVEL_BYPASS,
	SAR_LEVEL_LOW,
	SAR_LEVEL_MEDIUM,
	SAR_LEVEL_HIGH,
	SAR_LEVEL_MAX
};

struct sar_cmd {
	unsigned int enable;   // 0 : disable 1 : enable
	unsigned int strength; // enum SARstrengh_CMD
};

#endif //__TCC_SAR_H__

