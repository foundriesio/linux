// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TSC_SERDES_H
#define TSC_SERDES_H

#include <linux/regmap.h>

#define DEV_REV_REG 0x000E

#define HDMI_SER_ADDR 0x44
#define HDMI_DES_ADDR 0xD4

#define DP_SER_ADDR   0xC0
#define DP_DES1_ADDR  0x90
#define DP_DES2_ADDR  0x94
#define DP_DES3_ADDR  0x98

#define SER_DES_W_LEN 3 /* byte */
#define DISPLAY_MAX_NUM 3

#define TCC803X_EVB 0x803
#define TCC805X_EVB 0xF
#define TCC8050_EVB 1
#define TCC8059_EVB 0

/* serializer device revision */
#define REV_ALL 0x0
#define REV_ES4 0x3

struct serdes_info {
	struct i2c_client *client;
	struct regmap *regmap;

	int board_type;
	unsigned int revision;
	u8 des_num;
};

struct i2c_data {
	unsigned short addr;
	unsigned short reg;
	unsigned char val;
	int board;
	int display_num;
	unsigned int revision;
};

int tcc_tsc_serdes_update(
		struct i2c_client *client,
		struct regmap *regmap,
		u8 des_num,
		int evb_board,
		unsigned int revision);

#endif /* TSC_SERDES_H */
