// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <asm/ioctl.h>
#include <linux/uaccess.h>

#include "dxb_ctrl_defs.h"
#include "dxb_ctrl_gpio.h"
#include "tcc_dxb_control.h"

static void amfmtuner_init(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	(void)deviceIdx; /* this line is add to avoid QAC/codesonar warning */

	dxb_ctrl_gpio_out_init(ctrl->gpio_tuner_pwr);
	dxb_ctrl_gpio_out_init(ctrl->gpio_tuner_rst);
	return;
}

static void amfmtuner_on(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	(void)deviceIdx;

	dxb_ctrl_gpio_set_value(ctrl->gpio_tuner_pwr, 1);
	return;
}

static void amfmtuner_off(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	(void)deviceIdx;

	dxb_ctrl_gpio_set_value(ctrl->gpio_tuner_pwr, 0);
	return;
}

static void amfmtuner_reset_low(struct tcc_dxb_ctrl_t *ctrl,
				int32_t deviceIdx)
{
	(void)deviceIdx;

	dxb_ctrl_gpio_set_value(ctrl->gpio_tuner_rst, 0);
	return;
}

static void amfmtuner_reset_high(struct tcc_dxb_ctrl_t *ctrl,
				int32_t deviceIdx)
{
	(void)deviceIdx;

	dxb_ctrl_gpio_set_value(ctrl->gpio_tuner_rst, 1);
	return;
}

long_t amfmtuner_ioctl(struct tcc_dxb_ctrl_t *ctrl,
				uint32_t cmd, ulong arg)
{
	int32_t deviceIdx;
	ulong result;

	(void)pr_info("[INFO][TCC_DXB_CTRL] %s cmd[0x%X]\n", __func__, cmd);

	if (ctrl == NULL) {
		return (-ENODEV);
	}

	if (arg == (ulong)0) {
		deviceIdx = 0;
	} else {
		result = copy_from_user((void *)&deviceIdx, (const void *)arg,
		                    sizeof(int32_t));
		if (result != (ulong)0) {
			return 0;
		}
	}

	switch (cmd) {
	case IOCTL_DXB_CTRL_SET_BOARD:
		amfmtuner_init(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_OFF:
		amfmtuner_off(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_ON:
		amfmtuner_on(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_RESET_LOW:
		amfmtuner_reset_low(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_RESET_HIGH:
		amfmtuner_reset_high(ctrl, deviceIdx);
		break;

	default:
		/* do nothing */
		break;

	}
	return 0;
}

