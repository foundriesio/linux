/*
 * Copyright (C) 2013 by Stefan Agner <stefan.agner@toradex.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef ASMARM_ARCH_COLIBRI_TS_H
#define ASMARM_ARCH_COLIBRI_TS_H

struct colibri_ts_platform_data {
	int (*init)(struct platform_device *pdev);
	void (*exit)(struct platform_device *pdev);
	unsigned int gpio_pen;
	int (*mux_pen_interrupt)(struct platform_device *pdev);
	int (*mux_adc)(struct platform_device *pdev);
};

#endif
