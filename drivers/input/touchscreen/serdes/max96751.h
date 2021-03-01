// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef MAX96751_H
#define MAX96751_H

#include <linux/input/tcc_tsc_serdes.h>

struct i2c_data hdmi_max96751_ser_regs[] = {
	/* Touch GPIO------------------------------- */
	/* Touch IRQ : Ser GPIO 5 RX/TX RX ID 15 */
	{HDMI_SER_ADDR, 0x020F, 0x84, TCC803X_EVB, 1, REV_ALL},
	{HDMI_SER_ADDR, 0x0211, 0x4F, TCC803X_EVB, 1, REV_ALL},
	/* Touch RST : Ser GPIO 9 RX/TX TX ID 9 */
	{HDMI_SER_ADDR, 0x021B, 0x41, TCC803X_EVB, 1, REV_ALL},
	{HDMI_SER_ADDR, 0x021C, 0x49, TCC803X_EVB, 1, REV_ALL},
	{HDMI_SER_ADDR, 0x021D, 0x40, TCC803X_EVB, 1, REV_ALL},
	/* Touch RST : Des GPIO 16 RX/TX RX ID 9 */
	{HDMI_SER_ADDR, 0x021B, 0x43, TCC803X_EVB, 1, REV_ALL},
	/* Touch GPIO------------------------------- */

	/* set I2C_AUTO_CFG, I2C_SRC_CNT */
	{HDMI_SER_ADDR, 0x0046, 0x08, TCC803X_EVB, 1, REV_ALL},
	/* HDMI Ser PT1 Enable */
	{HDMI_SER_ADDR, 0x0001, 0xD8, TCC803X_EVB, 1, REV_ALL},

	{0, 0, 0, 0, 0, 0},
};

#endif /* MAX96751_H */
