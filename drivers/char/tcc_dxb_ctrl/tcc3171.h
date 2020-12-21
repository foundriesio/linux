/*
 * tcc3171.h
 *
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux DxB Control DRIVER
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

static inline int tcc3171_init(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_OUT_INIT(ctrl->gpio_dxb_on);

//	GPIO_OUT_INIT(ctrl->gpio_dxb_0_pwdn);
	GPIO_OUT_INIT(ctrl->gpio_dxb_0_rst);

//	GPIO_OUT_INIT(ctrl->gpio_dxb_1_pwdn);
	GPIO_OUT_INIT(ctrl->gpio_dxb_1_rst);

	return 0;
}

static inline int tcc3171_reset(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 1);
		break;
	case 1:
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 1);
		break;
	default:
		break;
	}
	return 0;
}

static inline int tcc3171_on(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_SET_VALUE(ctrl->gpio_dxb_on, 1);

	switch (deviceIdx) {
	case 0:
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
//		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 0);
//		msleep(20);
//		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 1);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 1);
		break;

	case 1:
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
//		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 0);
//		msleep(20);
//		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 1);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 1);
		break;
	default:
		break;
	}
	return 0;
}

static inline int tcc3171_off(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	switch (deviceIdx) {
	case 0:
//		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 0);
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		break;
	case 1:
//		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 0);
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		break;
	default:
		break;
	}

	GPIO_SET_VALUE(ctrl->gpio_dxb_on, 0);

	return 0;
}

static inline int tcc3171_pure_on(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_SET_VALUE(ctrl->gpio_dxb_on, 1);

	switch (deviceIdx) {
	case 0:
//		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 0);
		msleep(20);
//		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 1);
		msleep(20);

		break;
	case 1:
//		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 0);
		msleep(20);
//		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 1);
		msleep(20);
		break;
	default:
		break;
	}

	return 0;
}

static inline int tcc3171_pure_off(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	switch (deviceIdx) {
	case 0:
//		GPIO_SET_VALUE(ctrl->gpio_dxb_0_pwdn, 0);
		break;
	case 1:
//		GPIO_SET_VALUE(ctrl->gpio_dxb_1_pwdn, 0);
		break;
	default:
		break;
	}

	GPIO_SET_VALUE(ctrl->gpio_dxb_on, 0);

	return 0;
}

static inline int tcc3171_reset_low(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		break;
	case 1:
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		break;
	default:
		break;
	}

	return 0;
}

static inline int tcc3171_reset_high(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	switch (deviceIdx) {
	case 0:
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_0_rst, 1);
		break;
	case 1:
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 0);
		msleep(20);
		GPIO_SET_VALUE(ctrl->gpio_dxb_1_rst, 1);
		break;
	default:
		break;
	}

	return 0;
}

static inline int TCC3171_IOCTL(struct tcc_dxb_ctrl_t *ctrl,
				unsigned int cmd, unsigned long arg)
{
	unsigned int deviceIdx;
	unsigned int parg;

	pr_info("[INFO][TCC_DXB_CTRL] %s cmd[0x%X]\n", __func__, cmd);

	if (copy_from_user((void *)&parg,
	   (const void *)arg, sizeof(unsigned int)) != 0)
	{
		return 0;
	}
	deviceIdx = (parg == 0) ? 0 : parg;

	switch (cmd)
	{
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

	case IOCTL_DXB_CTRL_GET_CTLINFO:
	case IOCTL_DXB_CTRL_RF_PATH:
	default:
		break;

	}
	return 0;
}
