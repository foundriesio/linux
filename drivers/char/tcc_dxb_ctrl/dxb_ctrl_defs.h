// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DXB_CTRL_DEFS_H_
#define DXB_CTRL_DEFS_H_

#include <linux/types.h>
#include <linux/cdev.h>

#ifndef long_t
typedef long long_t;
#endif

struct tcc_dxb_ctrl_t {
	struct device *dev;
	struct cdev   cdev;
	struct class *class;
	dev_t devnum;

	int32_t board_type;
	int32_t bb_index;
	int32_t gpio_dxb_on;
	int32_t gpio_dxb_0_pwdn;
	int32_t gpio_dxb_0_rst;
	int32_t gpio_dxb_1_pwdn;
	int32_t gpio_dxb_1_rst;

	int32_t gpio_tuner_pwr;
	int32_t gpio_tuner_rst;
};
#endif // DXB_CTRL_DEFS_H_

