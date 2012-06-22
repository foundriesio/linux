/*
 * Copyright (C) 2012 Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
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

#ifndef _COLIBRI_USB_H_
#define _COLIBRI_USB_H_

struct colibri_otg_platform_data {
	int cable_detect_gpio;
	struct platform_device* (*host_register)(void);
	void (*host_unregister)(struct platform_device*);
};

#endif /* _COLIBRI_USB_H_ */
