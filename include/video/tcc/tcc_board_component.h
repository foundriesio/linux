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
#ifndef __TCC_COMPONENT_H
#define __TCC_COMPONENT_H

#include <linux/types.h>
#include <linux/device.h>

#define TCC_COMPONENT_ON     1
#define TCC_COMPONENT_OFF    0

struct tcc_component_platform_data {
	unsigned int open_cnt;
	struct device *pdev;
};

struct tcc_component_hpd_platform_data {
	unsigned int component_port;
};

#if defined(CONFIG_SWITCH_GPIO_COMPONENT)
#include <linux/switch.h>
struct component_gpio_switch_data {
	struct switch_dev sdev;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	unsigned int state_val;
	struct work_struct work;
	void (*send_component_event)(void *pswitch_data,
		unsigned int component_state);
};
#endif

#endif
