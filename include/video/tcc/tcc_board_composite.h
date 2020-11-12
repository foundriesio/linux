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
#ifndef __TCC_COMPOSITE_H
#define __TCC_COMPOSITE_H

#include <linux/types.h>
#include <linux/device.h>

#define TCC_COMPOSITE_ON     1
#define TCC_COMPOSITE_OFF    0

struct tcc_composite_platform_data {
	unsigned int open_cnt;
	struct device *pdev;
};

struct tcc_composite_hpd_platform_data {
	unsigned int composite_port;
};

#if defined(CONFIG_SWITCH_GPIO_COMPOSITE)
#include <linux/switch.h>
struct composite_gpio_switch_data {
	struct switch_dev sdev;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	unsigned int state_val;
	struct work_struct work;
	void (*send_composite_event)(void *pswitch_data,
		unsigned int composite_state);
};
#endif

#endif
