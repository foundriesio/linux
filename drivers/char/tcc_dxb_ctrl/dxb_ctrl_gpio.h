// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DXB_CTRL_GPIO_H_
#define DXB_CTRL_GPIO_H_

#include "dxb_ctrl_defs.h"

extern void dxb_ctrl_gpio_out_init(int32_t pin);
extern void dxb_ctrl_gpio_set_value(int32_t pin, int32_t value);
extern void dxb_ctrl_gpio_free(int32_t pin);

#if 0 /* comment out to avoid QAC/Codesonar warning, enable later if required */
extern void dxb_ctrl_gpio_in_init(int32_t pin);
extern int32_t dxb_ctrl_gpio_get_value(int32_t pin);
#endif

#endif // DXB_CTRL_GPIO_H_

