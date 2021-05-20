// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/pinctrl/consumer.h>

/* I2C Port Configuration Reg. */
#define I2C_PORT_CFG0           0x00
#define I2C_PORT_CFG1           0x04
#define I2C_PORT_CFG2           0x08

struct i2c_mux_pcfg {
	int chan;
	int default_port;
	int port;
	struct pinctrl *pinctrl;
};

struct tcc_i2c_mux {
	struct device			*dev;
	void __iomem			*base;
	const struct tcc_i2c_mux_soc	*soc_data;
	int				core;

	struct i2c_mux_pcfg		*pcfg_list;
	int				pcfg_len;
};

struct tcc_i2c_mux_soc {
	int (*mux_select)(struct i2c_mux_core *muxc, u32 chan);
	int (*mux_deselect)(struct i2c_mux_core *muxc, u32 chan);
};

static int tcc_i2c_mux_select(struct i2c_mux_core *muxc, u32 chan)
{
	struct tcc_i2c_mux *mux = i2c_mux_priv(muxc);
	uint32_t port = mux->pcfg_list[chan].port;
	uint32_t default_port;
	uint32_t reg_shift, pcfg_val;
	int32_t offset;

	if (mux->core < 4) {
		offset = I2C_PORT_CFG0;
		reg_shift = mux->core << 3;
	} else {
		offset = I2C_PORT_CFG2;
		reg_shift = (mux->core - 4U) << 3U;
	}

	/* save default value */
	pcfg_val = readl(mux->base + offset);
	default_port = (pcfg_val >> reg_shift) & 0xFFU;
	mux->pcfg_list[chan].default_port = default_port;

	/* Set i2c port-mux */
	pcfg_val &= ~((u32)0xFFU << reg_shift);
	pcfg_val |= ((u32)port << reg_shift);
	writel(pcfg_val, mux->base + offset);

	dev_dbg(mux->dev, "[DEBUG][I2C] %s: mux %d-%d, port %d (PCFG 0x%x)\n",
			__func__,
			mux->core,
			chan,
			port,
			pcfg_val);
	return 0;
}

static int tcc_i2c_mux_deselect(struct i2c_mux_core *muxc, u32 chan)
{
	struct tcc_i2c_mux *mux = i2c_mux_priv(muxc);
	uint32_t default_port = mux->pcfg_list[chan].default_port;
	uint32_t reg_shift, pcfg_val;
	int32_t offset;

	/* set default value */
	if (mux->core < 4) {
		offset = I2C_PORT_CFG0;
		reg_shift = mux->core << 3;
	} else {
		offset = I2C_PORT_CFG2;
		reg_shift = (mux->core - 4U) << 3U;
	}
	pcfg_val = readl(mux->base + offset);
	pcfg_val &= ~((u32)0xFFU << reg_shift);
	pcfg_val |= ((u32)default_port << reg_shift);
	writel(pcfg_val, mux->base + offset);

	dev_dbg(mux->dev, "[DEBUG][I2C] %s: mux %d-%d, port %d (PCFG 0x%x)\n",
			__func__,
			mux->core,
			chan,
			mux->pcfg_list[chan].port,
			pcfg_val);
	return 0;
}

static const struct tcc_i2c_mux_soc tcc_soc_data = {
	.mux_select = tcc_i2c_mux_select,
	.mux_deselect = tcc_i2c_mux_deselect,
};

static const struct of_device_id tcc_i2c_mux_of_match[] = {
	{ .compatible = "telechips,i2c-mux",
		.data       = &tcc_soc_data, },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_i2c_mux_of_match);

static int tcc_i2c_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *parent_np, *child, *pin_np;
	struct i2c_adapter *adapter;
	struct i2c_mux_core *muxc;
	struct tcc_i2c_mux *mux;
	struct i2c_mux_pcfg *elem;
	struct resource *res;
	const struct of_device_id *match;
	int child_cnt, ret;

	if (!np)
		return -ENODEV;

	match = of_match_node(tcc_i2c_mux_of_match, np);
	if (!match)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "[ERROR][I2C] %s: no resource\n", __func__);
		return -ENODEV;
	}

	/* get i2c core */
	parent_np = of_parse_phandle(np, "i2c-parent", 0);
	if (!parent_np) {
		dev_err(dev, "[ERROR][I2C] Cannot parse i2c-parent\n");
		return -ENODEV;
	}
	adapter = of_find_i2c_adapter_by_node(parent_np);
	of_node_put(parent_np);
	if (!adapter) {
		dev_err(dev, "[ERROR][I2C] failed to get i2c-parent adapter\n");
		return -EPROBE_DEFER;
	}

	/* get sub bus count */
	child_cnt = of_get_child_count(np);

	/* allocate tcc_i2c_mux */
	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (mux == NULL)
		return -ENOMEM;

	mux->dev = dev;
	mux->soc_data = match->data;
	mux->core = i2c_adapter_id(adapter);
	put_device(&adapter->dev);
	mux->pcfg_len = child_cnt;
	mux->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mux->base)) {
		ret = PTR_ERR(mux->base);
		goto err_put_parent;
	}

	/* allocate mux core */
	muxc = i2c_mux_alloc(adapter, dev, child_cnt, 0,
			I2C_MUX_LOCKED,
			mux->soc_data->mux_select,
			mux->soc_data->mux_deselect);
	if (!muxc) {
		ret = -ENOMEM;
		goto err_put_parent;
	}
	muxc->priv = mux;
	platform_set_drvdata(pdev, muxc);

	/* get child node */
	mux->pcfg_list = devm_kzalloc(mux->dev,
			sizeof(struct i2c_mux_pcfg) * child_cnt,
			GFP_KERNEL);
	if (!mux->pcfg_list) {
		ret = -ENOMEM;
		goto err_put_parent;
	}
	elem = mux->pcfg_list;
	for_each_child_of_node(np, child) {
		/* get pinctrl */
		pin_np = of_parse_phandle(child, "pinctrl-0", 0);
		if (!pin_np) {
			dev_err(mux->dev, "[ERROR][I2C] Cannot parse pinctrl-0\n");
			ret = -ENODEV;
			goto err_del_mux_adapters;
		}
		/* get port number from pinctrl name  */
		ret = sscanf(pin_np->name, "i2c%d_bus", &elem->port);
		if (ret != 1) {
			dev_err(mux->dev, "[ERROR][I2C] pinctrl-0 can't find pattern i2c<port>_bus...(%s)\n",
					pin_np->name);
			goto err_del_mux_adapters;
		}
		/* get sub bus id */
		ret = of_property_read_u32(child, "reg", &elem->chan);
		if (ret < 0) {
			dev_err(mux->dev, "[ERROR][I2C] no reg property for node '%s'\n",
					child->name);
			goto err_del_mux_adapters;
		}
		/* register i2c sub bus */
		ret = i2c_mux_add_adapter(muxc, 0, elem->chan, 0);
		if (ret < 0) {
			dev_err(mux->dev, "[ERROR][I2C] fail to add mux (id:%d)\n",
					elem->chan);
			goto err_del_mux_adapters;
		}

		dev_info(dev, "[INFO][I2C] adpater %d, sub bus %d - port mux %d\n",
				mux->core, elem->chan, elem->port);
		elem++;
	}

	return 0;

err_del_mux_adapters:
	i2c_mux_del_adapters(muxc);
err_put_parent:
	i2c_put_adapter(adapter);
	return ret;
}

static int tcc_i2c_mux_remove(struct platform_device *pdev)
{
	struct i2c_mux_core *muxc = platform_get_drvdata(pdev);

	i2c_mux_del_adapters(muxc);
	i2c_put_adapter(muxc->parent);

	return 0;
}

static struct platform_driver tcc_i2c_mux_driver = {
	.probe	= tcc_i2c_mux_probe,
	.remove	= tcc_i2c_mux_remove,
	.driver	= {
		.name	= "i2c-mux-tcc",
		.of_match_table = of_match_ptr(tcc_i2c_mux_of_match),
	},
};
module_platform_driver(tcc_i2c_mux_driver);

MODULE_AUTHOR("Telechips Inc. androidce@telechips.com");
MODULE_DESCRIPTION("Telechips I2C port mux driver");
MODULE_LICENSE("GPL");
