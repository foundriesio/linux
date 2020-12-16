// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

static inline int tcc353x_init(struct tcc_isdbt_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_OUT_INIT(ctrl->gpio_dxb_on);

	GPIO_OUT_INIT(ctrl->gpio_dxb_0_pwdn);
	GPIO_OUT_INIT(ctrl->gpio_dxb_0_rst);

	GPIO_OUT_INIT(ctrl->gpio_dxb_1_pwdn);
	GPIO_OUT_INIT(ctrl->gpio_dxb_1_rst);

	return 0;
}

static inline int tcc353x_reset(struct tcc_isdbt_ctrl_t *ctrl, int deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		// Reset#0 Active
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		// Reset#0 Inactive
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 1);
		break;

	case 1:
		// Reset#1 Active
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		// Reset#1 Inactive
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 1);
		break;

	default:
		break;
	}

	return 0;
}

static inline int tcc353x_on(struct tcc_isdbt_ctrl_t *ctrl, int deviceIdx)
{
	if (deviceIdx >= MAX_DEVICE_NO)
		return 0;

	if (ctrl->power_status[0] == 0 && ctrl->power_status[1] == 0
	    && ctrl->power_status[2] == 0 && ctrl->power_status[3] == 0) {
	    // Main Power Active
		GPIO_SET_VALUE(ctrl->gpio_dxb_on, 1);
	}
	ctrl->power_status[deviceIdx] = 1;

	switch (deviceIdx) {
	case 0:
		// Power#0 Active
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 1);
		// Reset#0 Active
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		// Reset#0 Inactive
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 1);
		break;

	case 1:
		// Power#1 Active
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 1);
		// Power#1 Active
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		// Power#1 Inactive
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 1);
		break;

	default:
		break;
	}

	return 0;
}

static inline int tcc353x_off(struct tcc_isdbt_ctrl_t *ctrl, int deviceIdx)
{
	if (deviceIdx >= MAX_DEVICE_NO)
		return 0;

	if (ctrl->power_status[deviceIdx] == 0)
		return 0;

	ctrl->power_status[deviceIdx] = 0;

	switch (deviceIdx) {
	case 0:
		// Power#0 Inactive
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 0);
		break;

	case 1:
		// Power#1 Inactive
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 0);
		break;

	default:
		break;
	}

	if (ctrl->power_status[0] == 0 && ctrl->power_status[1] == 0
	    && ctrl->power_status[2] == 0 && ctrl->power_status[3] == 0) {
	    // Main Power Inactive
		GPIO_SET_VALUE(ctrl->gpio_dxb_on, 0);
	}
	return 0;
}

static inline int TCC353X_IOCTL(struct tcc_isdbt_ctrl_t *ctrl, unsigned int cmd,
				unsigned long arg)
{
	unsigned int deviceIdx;

	switch (cmd) {
	case IOCTL_DXB_CTRL_SET_BOARD:
		deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
		tcc353x_init(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_OFF:
		deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
		tcc353x_off(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_ON:
		deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
		tcc353x_on(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_RESET:
		deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
		tcc353x_reset(ctrl, deviceIdx);
		break;

	case IOCTL_DXB_CTRL_GET_CTLINFO:
	case IOCTL_DXB_CTRL_RF_PATH:
	case IOCTL_DXB_CTRL_RESET_LOW:
	case IOCTL_DXB_CTRL_RESET_HIGH:
	case IOCTL_DXB_CTRL_PURE_ON:
	case IOCTL_DXB_CTRL_PURE_OFF:
	default:
		break;
	}
	return 0;
}
