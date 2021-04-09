// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/input/tcc_tsc_serdes.h>

#include "max96751.h"
#include "max96851.h"
#include "max96878.h"

#define MODULE_NAME "tsc_serdes"

static const struct regmap_config tsc_serdes_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
};

int tcc_tsc_serdes_update(
		struct i2c_client *client,
		struct regmap *regmap,
		u8 des_num,
		int evb_board,
		unsigned int revision)
{
	unsigned short addr = client->addr;
	struct i2c_data *ser_reg;
	struct i2c_data *des_reg;
	unsigned char buf[3] = {0,};
	int i, ret;

	dev_info(&client->dev, "[INFO][TSC_SERDES] evb_board 0x%x, des_num %d revision %d\n",
			evb_board,
			des_num,
			revision);
	switch (evb_board) {
	case TCC803X_EVB:
		ser_reg = hdmi_max96751_ser_regs;
		des_reg = hdmi_max96878_des_regs;
		break;
	case TCC805X_EVB:
	case TCC8050_EVB:
	case TCC8059_EVB:
		ser_reg = dp_max96851_ser_regs;
		des_reg = dp_max96878_des_regs;
		break;
	default:
		dev_warn(&client->dev, "[WARN][TSC_SERDES] %s: Not supported TCC EVB(0x%x)\n",
				__func__, evb_board);
		ser_reg = NULL;
		des_reg = NULL;
		break;
	}

	if ((ser_reg == NULL) || (des_reg == NULL)) {
		return -ENOMEM;
	}

	/* update register of serializer */
	for (i = 0; ser_reg[i].addr != 0U; i++) {
		if (des_num < ser_reg[i].display_num)
			continue;
		if ((ser_reg[i].board != TCC805X_EVB)
				&& (ser_reg[i].board != evb_board))
			continue;

		client->addr = (ser_reg[i].addr >> 1U);
		if (regmap != NULL) {
			ret = regmap_write(regmap,
					ser_reg[i].reg,
					ser_reg[i].val);
		} else {
			buf[0] = (unsigned char)(ser_reg[i].reg >> 8);
			buf[1] = (unsigned char)(ser_reg[i].reg & 0xFFU);
			buf[2] = ser_reg[i].val;
			ret = i2c_master_send(client, buf, SER_DES_W_LEN);
		}
		if (ret < 0) {
			dev_err(&client->dev, "[ERR][TSC_SERDES]%s: failed to write i2c data[%d] (%d)\n",
					__func__, i, ret);
			return ret;
		}
		dev_dbg(&client->dev, "[DEBUG][TSC_SERDES][%d] A_0x%02x R_0x%02x V_0x%02x\n",
				i, ser_reg[i].addr,
				ser_reg[i].reg, ser_reg[i].val);
	}

	/* update register of deserializer */
	for (i = 0; des_reg[i].addr != 0U; i++) {
		if (des_num < des_reg[i].display_num)
			continue;
		if ((des_reg[i].board != TCC805X_EVB)
				&& (des_reg[i].board != evb_board))
			continue;

		client->addr = (des_reg[i].addr >> 1U);
		if (regmap != NULL) {
			ret = regmap_write(regmap,
					des_reg[i].reg,
					des_reg[i].val);
		} else {
			buf[0] = (unsigned char)(des_reg[i].reg >> 8);
			buf[1] = (unsigned char)(des_reg[i].reg & 0xFFU);
			buf[2] = des_reg[i].val;
			ret = i2c_master_send(client, buf, SER_DES_W_LEN);
		}

		if (ret < 0) {
			dev_err(&client->dev, "[ERR][TSC_SERDES]%s: failed to write i2c data[%d] (%d)\n",
					__func__, i, ret);
			return ret;
		}
		dev_dbg(&client->dev, "[DEBUG][TSC_SERDES][%d] A_0x%02x R_0x%02x V_0x%02x\n",
				i, des_reg[i].addr,
				des_reg[i].reg, des_reg[i].val);
	}

	client->addr = addr;
	return 0;
}

static int tsc_serdes_get_info(struct serdes_info *info)
{
	struct i2c_client *client = info->client;
	unsigned int val;
	int ret;

	/* get serializer revision */
	client->addr = (HDMI_SER_ADDR >> 1);
	ret = regmap_read(info->regmap, DEV_REV_REG, &val);
	if (ret < 0) {
		dev_err(&client->dev, "[ERR][TSC_SERDES] %s: failed to get serializer device revision\n",
				__func__);
		return ret;
	}
	info->revision = val;
	info->des_num = 1U;

	return 0;
}

static const struct of_device_id tsc_serdes_of_match[] = {
	{	.compatible = "telechips,tcc803x-tsc-serdes",
		.data = (void *)TCC803X_EVB,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, tsc_serdes_of_match);

static int tsc_serdes_probe(
		struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct serdes_info *info;
	const struct of_device_id *match;
	struct device_node *np = client->dev.of_node;
	int error;

	match = of_match_node(tsc_serdes_of_match, np);
	if (match == NULL) {
		return -EINVAL;
	}

	info = kzalloc(sizeof(struct serdes_info), GFP_KERNEL);
	if (info == NULL) {
		return -ENOMEM;
	}

	info->board_type = (int)match->data;
	info->client = client;
	info->regmap = devm_regmap_init_i2c(client, &tsc_serdes_regmap_config);
	if (IS_ERR(info->regmap)) {
		error = PTR_ERR(info->regmap);
		dev_err(&client->dev, "[ERR][TSC_SERDES]failed to initialize regmap\n");
		goto err;
	}
	/* get information of device */
	error = tsc_serdes_get_info(info);
	if (error < 0) {
		dev_err(&client->dev, "[ERR][TSC_SERDES]failed to get device information\n");
		goto err;
	}
	i2c_set_clientdata(client, info);

	/* update serdes regs */
	error = tcc_tsc_serdes_update(
		client,
		info->regmap,
		info->des_num,
		info->board_type,
		info->revision);
	if (error < 0) {
		dev_err(&client->dev, "[ERR][TSC_SERDES]failed to update Ser/Des register\n");
		goto err;
	}

	dev_dbg(&client->dev, "[DEBUG][TSC_SERDES]Success to update Ser/Des register for Touch (board:0x%x)\n",
			info->board_type);
	return 0;
err:
	kfree(info);
	return error;
}

static int tsc_serdes_remove(struct i2c_client *client)
{
	struct serdes_info *info = i2c_get_clientdata(client);

	regmap_exit(info->regmap);
	kfree(info);
	i2c_set_clientdata(client, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tsc_serdes_suspend(struct device *dev)
{
	return 0;
}

static int tsc_serdes_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct serdes_info *info = i2c_get_clientdata(client);
	int error;

	/* update serdes regs */
	error = tcc_tsc_serdes_update(
		client,
		info->regmap,
		info->des_num,
		info->board_type,
		info->revision);
	if (error < 0) {
		dev_err(&client->dev, "[ERR][TSC_SERDES]failed to update serdes reg\n");
		return error;
	}

	return 0;
}
static SIMPLE_DEV_PM_OPS(tsc_serdes_pm, tsc_serdes_suspend, tsc_serdes_resume);
#endif

static const struct i2c_device_id serdes_id_table[] = {
	{MODULE_NAME, 0},
	{},
};

static struct i2c_driver tsc_serdes_driver = {
	.probe = tsc_serdes_probe,
	.remove = tsc_serdes_remove,
	.driver = {
		.name   = MODULE_NAME,
		.of_match_table = of_match_ptr(tsc_serdes_of_match),
		.pm = &tsc_serdes_pm,
	},
	.id_table = serdes_id_table,
};

static int __init tsc_serdes_init(void)
{
	return i2c_add_driver(&tsc_serdes_driver);
}
subsys_initcall(tsc_serdes_init);

static void __exit tsc_serdes_exit(void)
{
	i2c_del_driver(&tsc_serdes_driver);
}
module_exit(tsc_serdes_exit);
