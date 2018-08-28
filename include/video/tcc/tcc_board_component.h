/* linux/arch/arm/mach-tcc89xx/include/mach/tcc_board_COMPONENT.h
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
	unsigned component_port;
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
	void (*send_component_event)(void *pswitch_data, unsigned int component_state);
};
#endif

#endif
