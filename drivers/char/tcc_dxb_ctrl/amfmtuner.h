/* 
 * amfmtuner.h
 *
 * Author:  <linux@telechips.com>
 * Created: 13th Jan, 2020 
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

static inline int amfmtuner_init(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_OUT_INIT(ctrl->gpio_tuner_pwr);
	GPIO_OUT_INIT(ctrl->gpio_tuner_rst);

	return 0;
}

static inline int amfmtuner_on(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_SET_VALUE(ctrl->gpio_tuner_pwr, 1);

	return 0;
}

static inline int amfmtuner_off(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_SET_VALUE(ctrl->gpio_tuner_pwr, 0);
	return 0;
}

static inline int amfmtuner_reset_low(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_SET_VALUE(ctrl->gpio_tuner_rst, 0);

	return 0;
}

static inline int amfmtuner_reset_high(struct tcc_dxb_ctrl_t *ctrl, int deviceIdx)
{
	GPIO_SET_VALUE(ctrl->gpio_tuner_rst, 1);

	return 0;
}


static inline int amfmtuner_ioctl(struct tcc_dxb_ctrl_t *ctrl, unsigned int cmd, unsigned long arg)
{
	unsigned int deviceIdx;
	dprintk("%s cmd[0x%X]\n", __func__, cmd);
	switch (cmd)
	{
		case IOCTL_DXB_CTRL_SET_BOARD:
			deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
			amfmtuner_init(ctrl, deviceIdx);
			break;

		case IOCTL_DXB_CTRL_OFF:
			deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
			amfmtuner_off(ctrl, deviceIdx);
			break;

		case IOCTL_DXB_CTRL_ON:
			deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
			amfmtuner_on(ctrl, deviceIdx);
			break;

		case IOCTL_DXB_CTRL_RESET_LOW:
			deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
			amfmtuner_reset_low(ctrl, deviceIdx);
			break;

		case IOCTL_DXB_CTRL_RESET_HIGH:
			deviceIdx = (arg == 0) ? 0 : *(unsigned int *)arg;
			amfmtuner_reset_high(ctrl, deviceIdx);
			break;

		default:
			break;

	}
	return 0;
}

