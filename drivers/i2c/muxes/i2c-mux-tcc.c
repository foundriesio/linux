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
	uint8_t port;
	struct pinctrl_state *pin_state;
};

struct tcc_i2c_mux {
	struct device			*dev;
	void __iomem			*base;
	const struct tcc_i2c_mux_soc	*soc_data;
	struct pinctrl			*pinctrl;
	int				core;

	struct i2c_mux_pcfg		*pcfg_list;
	int				pcfg_len;
};

struct tcc_i2c_mux_soc {
	int (*mux_select)(struct i2c_mux_core *muxc, u32 chan);
	int (*mux_deselect)(struct i2c_mux_core *muxc, u32 chan);
};

static uint8_t tcc_i2c_get_pcfg(void __iomem *base, uint32_t core)
{
	uint32_t res, shift;

	if (core < 4)
		base += I2C_PORT_CFG0;
	else
		base += I2C_PORT_CFG2;

	res = readl(base);
	shift = ((core & GENMASK(1, 0)) << 3);
	res >>= shift;

	return (res & 0xFFU);
}

static void tcc_i2c_set_pcfg(void __iomem *base, uint32_t core, uint8_t port)
{
	uint32_t val, shift;

	if (core < 4)
		base += I2C_PORT_CFG0;
	else
		base += I2C_PORT_CFG2;

	val = readl(base);
	shift = ((core & GENMASK(1, 0)) << 3);
	val &= ~(0xFFU << shift);
	val |= (port << shift);
	writel(val, base);
}

static int tcc_i2c_mux_select(struct i2c_mux_core *muxc, u32 chan)
{
	struct tcc_i2c_mux *mux = i2c_mux_priv(muxc);
	struct i2c_mux_pcfg *elem;
	uint8_t port;
	int32_t i, ret;

	elem = &mux->pcfg_list[chan];

	/* select pinctrl for i2c mux */
	ret = pinctrl_select_state(mux->pinctrl, elem->pin_state);
	if (ret < 0) {
		dev_err(mux->dev, "[ERROR][I2C] %s: Cannot select pinctrl state %d\n",
				__func__, ret);
		return ret;
	}

	/* clear conflict for each i2c master core */
	for (i = 0; i < 8; i++) {
		port = tcc_i2c_get_pcfg(mux->base, i);
		if ((port == elem->port) && (i != mux->core))
			/* clear conflict */
			tcc_i2c_set_pcfg(mux->base, i, 0xff);
	}
	/* clear conflict for each i2c slave core */
	for (i = 0; i < 4; i++) {
		port = tcc_i2c_get_pcfg(mux->base + I2C_PORT_CFG1, i);
		if (port == elem->port)
			/* clear conflict */
			tcc_i2c_set_pcfg(mux->base + I2C_PORT_CFG1, i, 0xff);
	}

	/* save default value */
	elem->default_port = tcc_i2c_get_pcfg(mux->base, mux->core);

	/* finally set i2c port mux */
	tcc_i2c_set_pcfg(mux->base, mux->core, elem->port);

	dev_dbg(mux->dev, "[DEBUG][I2C] %s: mux %d-%d, change port %d->%d\n",
			__func__,
			mux->core,
			chan,
			elem->default_port,
			elem->port);
	return 0;
}

static int tcc_i2c_mux_deselect(struct i2c_mux_core *muxc, u32 chan)
{
	struct tcc_i2c_mux *mux = i2c_mux_priv(muxc);
	struct i2c_mux_pcfg *elem;

	elem = &mux->pcfg_list[chan];
	/* set default value */
	tcc_i2c_set_pcfg(mux->base, mux->core, elem->default_port);

	dev_dbg(mux->dev, "[DEBUG][I2C] %s: mux %d-%d, change port %d -> %d\n",
			__func__,
			mux->core,
			chan,
			elem->port,
			elem->default_port);
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
	const char *name;
	char pin_name[20];

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
	mux->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(mux->pinctrl)) {
		ret = PTR_ERR(mux->pinctrl);
		goto err_put_parent;
	}

	/* allocate mux core */
	muxc = i2c_mux_alloc(adapter, dev, child_cnt, 0,
			I2C_MUX_LOCKED,
			mux->soc_data->mux_select,
			mux->soc_data->mux_deselect);
	if (!muxc) {
		ret = -ENOMEM;
		goto err_put_pinctrl;
	}
	muxc->priv = mux;
	platform_set_drvdata(pdev, muxc);

	/* get child node */
	mux->pcfg_list = devm_kzalloc(mux->dev,
			sizeof(struct i2c_mux_pcfg) * child_cnt,
			GFP_KERNEL);
	if (!mux->pcfg_list) {
		ret = -ENOMEM;
		goto err_put_pinctrl;
	}
	elem = mux->pcfg_list;
	for_each_child_of_node(np, child) {
		/* get sub bus id */
		ret = of_property_read_u32(child, "reg", &elem->chan);
		if (ret < 0) {
			dev_err(mux->dev, "[ERROR][I2C] no reg property for node '%s'\n",
					child->name);
			goto err_del_mux_adapters;
		}
		/* get pinctrl */
		ret = of_property_read_string_index(np,
				"pinctrl-names",
				elem->chan,
				&name);
		if (ret < 0) {
			dev_err(dev, "[ERROR][I2C] Cannot parse pinctrl-names (%d)\n",
					ret);
			goto err_del_mux_adapters;
		}
		elem->pin_state = pinctrl_lookup_state(mux->pinctrl, name);
		if (IS_ERR(elem->pin_state)) {
			ret = PTR_ERR(elem->pin_state);
			dev_err(dev, "[ERROR][I2C] Cannot look up pinctrl state %s: %d\n",
					name, ret);
			goto err_del_mux_adapters;
		}
		/* get port number from pinctrl name  */
		snprintf(pin_name, sizeof(pin_name), "pinctrl-%d", elem->chan);
		pin_np = of_parse_phandle(np, pin_name, 0);
		if (!pin_np) {
			dev_err(mux->dev, "[ERROR][I2C] Cannot parse %s\n",
					pin_name);
			ret = -ENODEV;
			goto err_del_mux_adapters;
		}
		ret = sscanf(pin_np->name, "i2c%hhd_bus", &elem->port);
		if (ret != 1) {
			dev_err(mux->dev, "[ERROR][I2C] pinctrl-0 can't find pattern i2c<port>_bus...(%s)\n",
					pin_np->name);
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
err_put_pinctrl:
	devm_pinctrl_put(mux->pinctrl);
err_put_parent:
	i2c_put_adapter(adapter);
	return ret;
}

static int tcc_i2c_mux_remove(struct platform_device *pdev)
{
	struct i2c_mux_core *muxc = platform_get_drvdata(pdev);
	struct tcc_i2c_mux *mux = i2c_mux_priv(muxc);

	devm_pinctrl_put(mux->pinctrl);
	i2c_put_adapter(muxc->parent);
	i2c_mux_del_adapters(muxc);

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

static __init int tcc_i2c_mux_init(void)
{
	return platform_driver_register(&tcc_i2c_mux_driver);
}

static __exit void tcc_i2c_mux_exit(void)
{
	return platform_driver_unregister(&tcc_i2c_mux_driver);
}

subsys_initcall(tcc_i2c_mux_init);
module_exit(tcc_i2c_mux_exit);

MODULE_AUTHOR("Telechips Inc. androidce@telechips.com");
MODULE_DESCRIPTION("Telechips I2C port mux driver");
MODULE_LICENSE("GPL");
