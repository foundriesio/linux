// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <asm/ioctl.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "dxb_ctrl_defs.h"
#include "dxb_ctrl_gpio.h"
#include "tcc_dxb_control.h"

static void tcc3171_init(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	(void)deviceIdx; /* this line is add to avoid QAC/codesonar warning */

	dxb_ctrl_gpio_out_init(ctrl->gpio_dxb_on);
	dxb_ctrl_gpio_out_init(ctrl->gpio_dxb_0_rst);
	dxb_ctrl_gpio_out_init(ctrl->gpio_dxb_1_rst);

	return;
}

static void tcc3171_reset(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 1);
		break;
	case 1:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 1);
		break;
	default:
		/* do nothing */
		break;
	}
	return;
}

static void tcc3171_on(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_on, 1);

	switch (deviceIdx) {
	case 0:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 1);
		break;

	case 1:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 1);
		break;
	default:
		/* do nothing */
		break;
	}
	return;
}

static void tcc3171_off(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 0);
		break;
	case 1:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 0);
		break;
	default:
		/* do nothing */
		break;
	}

	dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_on, 0);

	return;
}

static void tcc3171_pure_on(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	(void)deviceIdx;
	dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_on, 1);

	return;
}

static void tcc3171_pure_off(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	(void)deviceIdx;
	dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_on, 0);

	return;
}

static void tcc3171_reset_low(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 0);
		break;
	case 1:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 0);
		break;
	default:
		/* do nothing */
		break;
	}

	return;
}

static void tcc3171_reset_high(struct tcc_dxb_ctrl_t *ctrl, int32_t deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_0_rst, 1);
		break;
	case 1:
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		dxb_ctrl_gpio_set_value(ctrl->gpio_dxb_1_rst, 1);
		break;
	default:
		/* do nothing */
		break;
	}

	return;
}

long_t tcc3171_ioctl(struct tcc_dxb_ctrl_t *ctrl,
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
		tcc3171_init(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_OFF:
		tcc3171_off(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_ON:
		tcc3171_on(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_RESET:
		tcc3171_reset(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_RESET_LOW:
		tcc3171_reset_low(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_RESET_HIGH:
		tcc3171_reset_high(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_PURE_ON:
		tcc3171_pure_on(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_PURE_OFF:
		tcc3171_pure_off(ctrl, deviceIdx);
		break;

	default:
		/* do nothing */
		break;
	}
	return 0;
}
