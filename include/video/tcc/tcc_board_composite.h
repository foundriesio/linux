/* linux/arch/arm/mach-tcc89xx/include/mach/tcc_board_composite.h
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
	unsigned composite_port;
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
	void (*send_composite_event)(void *pswitch_data, unsigned int composite_state);
};
#endif

#endif
