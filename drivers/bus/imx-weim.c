/*
 * EIM driver for Freescale's i.MX chips
 *
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

struct imx_weim_devtype {
	unsigned int	cs_count;
	unsigned int	cs_regs_count;
	unsigned int	cs_stride;
};

static const struct imx_weim_devtype imx1_weim_devtype = {
	.cs_count	= 6,
	.cs_regs_count	= 2,
	.cs_stride	= 0x08,
};

static const struct imx_weim_devtype imx27_weim_devtype = {
	.cs_count	= 6,
	.cs_regs_count	= 3,
	.cs_stride	= 0x10,
};

static const struct imx_weim_devtype imx50_weim_devtype = {
	.cs_count	= 4,
	.cs_regs_count	= 6,
	.cs_stride	= 0x18,
};

static const struct imx_weim_devtype imx51_weim_devtype = {
	.cs_count	= 6,
	.cs_regs_count	= 6,
	.cs_stride	= 0x18,
};

static const struct of_device_id weim_id_table[] = {
	/* i.MX1/21 */
	{ .compatible = "fsl,imx1-weim", .data = &imx1_weim_devtype, },
	/* i.MX25/27/31/35 */
	{ .compatible = "fsl,imx27-weim", .data = &imx27_weim_devtype, },
	/* i.MX50/53/6Q */
	{ .compatible = "fsl,imx50-weim", .data = &imx50_weim_devtype, },
	{ .compatible = "fsl,imx6q-weim", .data = &imx50_weim_devtype, },
	/* i.MX51 */
	{ .compatible = "fsl,imx51-weim", .data = &imx51_weim_devtype, },
	{ }
};
MODULE_DEVICE_TABLE(of, weim_id_table);

#define CS_MEM_RANGES_LEN 4

/* Parse and set the timing for this device. */
static int __init weim_timing_setup(struct device_node *np, void __iomem *base,
				    const struct imx_weim_devtype *devtype)
{
	u32 cs_idx, value[devtype->cs_regs_count];
	int i, ret;

	/* get the CS index from this child node's "reg" property. */
	ret = of_property_read_u32(np, "reg", &cs_idx);
	if (ret)
		return ret;

	if (cs_idx >= devtype->cs_count)
		return -EINVAL;

	ret = of_property_read_u32_array(np, "fsl,weim-cs-timing",
					 value, devtype->cs_regs_count);
	if (ret)
		return ret;

	/* set the timing for WEIM */
	for (i = 0; i < devtype->cs_regs_count; i++)
		writel(value[i], base + cs_idx * devtype->cs_stride + i * 4);

	return 0;
}

static int __init weim_parse_dt(struct platform_device *pdev,
				void __iomem *base)
{
	const struct of_device_id *of_id = of_match_device(weim_id_table,
							   &pdev->dev);
	const struct imx_weim_devtype *devtype = of_id->data;
	struct device_node *child;
	unsigned gpr_val, i;
	struct regmap *gpr;
	int ret;
	unsigned int start_addr = 0x08000000; /* StartAddr of next CS range */
	u32 value[devtype->cs_count * CS_MEM_RANGES_LEN];

	for_each_child_of_node(pdev->dev.of_node, child) {
		if (!child->name)
			continue;

		ret = weim_timing_setup(child, base, devtype);
		if (ret) {
			dev_err(&pdev->dev, "%s set timing failed.\n",
				child->full_name);
			return ret;
		}
	}

	/* set the CS mem ranges, IOMUXC_GPR1 to 32M,32M,32M,32M */
	gpr = syscon_regmap_lookup_by_compatible("fsl,imx6q-iomuxc-gpr");
	if (IS_ERR(gpr)) {
		dev_err(&pdev->dev,
			"failed to find fsl,imx6q-iomuxc-gpr regmap\n");
		return ret;
	}
	i = 0;
	do {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"ranges", i, &value[i]);
		if (ret)
			break;
		i++;
	} while (i < (devtype->cs_count * CS_MEM_RANGES_LEN));

	if (i && (i % CS_MEM_RANGES_LEN)) {
		dev_err(&pdev->dev, "ranges property must be a multiple "
				"of 4 32bit values but is %d\n",
			i);
		return -EINVAL;
	}
	gpr_val = 0;
	/* CS 0, 128M, 64M 32M) */
	if ((value[0] != 0) || (value[2] != start_addr)) {
		dev_err(&pdev->dev, "physical start addr for CS0 must be %x\n",
			start_addr);
		return -EINVAL;
	} else {
		switch (value[3]) {
		case (128 * 1024 * 1024):
				gpr_val = 0x5 << 0;
			start_addr = 0xffffffff;
			break;
		case (64 * 1024 * 1024):
				gpr_val = 0x3 << 0;
			start_addr += 64 * 1024 * 1024;
			break;
		case (32 * 1024 * 1024):
				gpr_val = 0x1 << 0;
			start_addr += 32 * 1024 * 1024;
			break;
		default:
			dev_err(&pdev->dev,
				"size for CS0 must be 128M|64M|32M\n");
			return -EINVAL;
		}
	}
	if (i > 4) {
		/* CS 1, 64M 32M) */
		if ((value[4] != 1) || (value[4+2] != start_addr)) {
			dev_err(&pdev->dev,
				"physical start addr for CS1 must be %x\n",
				start_addr);
		return -EINVAL;
		} else {
			switch (value[4+3]) {
			case (64 * 1024 * 1024):
				gpr_val |= 0x003 << 3;
				start_addr += 0xffffffff;
				break;
			case (32 * 1024 * 1024):
				gpr_val |= 0x001 << 3;
				start_addr += 32 * 1024 * 1024;
				break;
			default:
				dev_err(&pdev->dev,
					"size for CS1 must be 64M|32M\n");
				return -EINVAL;
			}
		}
	}
	if (i > 8) {
		/* CS 2, 32M) */
		if ((value[8] != 2) || (value[8+2] != start_addr)) {
			dev_err(&pdev->dev,
				"physical start addr for CS2 must be %x\n",
				start_addr);
		return -EINVAL;
		} else {
			switch (value[8+3]) {
			case (32 * 1024 * 1024):
				gpr_val |= 0x001 << 6;
				start_addr += 32 * 1024 * 1024;
				break;
			default:
				dev_err(&pdev->dev,
					"size for CS2 must be 32M\n");
				return -EINVAL;
			}
		}
	}
	if (i > 12) {
		/* CS 3, 32M) */
		if ((value[12] != 3) || (value[12+2] != start_addr)) {
			dev_err(&pdev->dev,
				"physical start addr for CS3 must be %x\n",
				start_addr);
		return -EINVAL;
		} else {
			switch (value[12+3]) {
			case (32 * 1024 * 1024):
				gpr_val |= 0x001 << 9;
				break;
			default:
				dev_err(&pdev->dev,
					"size for CS3 must be 32M\n");
				return -EINVAL;
			}
		}
	}
	dev_dbg(&pdev->dev, "weim CS setup in IOMUXC_GPR1 %x\n", gpr_val);
	regmap_update_bits(gpr, IOMUXC_GPR1, 0xfff, gpr_val);

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "%s fail to create devices.\n",
			pdev->dev.of_node->full_name);
	return ret;
}

static int __init weim_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *clk;
	void __iomem *base;
	int ret;

	/* get the resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	/* get the clock */
	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	ret = clk_prepare_enable(clk);
	if (ret)
		return ret;

	/* parse the device node */
	ret = weim_parse_dt(pdev, base);
	if (ret)
		clk_disable_unprepare(clk);
	else
		dev_info(&pdev->dev, "Driver registered.\n");

	return ret;
}

static struct platform_driver weim_driver = {
	.driver = {
		.name		= "imx-weim",
		.owner		= THIS_MODULE,
		.of_match_table	= weim_id_table,
	},
};
module_platform_driver_probe(weim_driver, weim_probe);

MODULE_AUTHOR("Freescale Semiconductor Inc.");
MODULE_DESCRIPTION("i.MX EIM Controller Driver");
MODULE_LICENSE("GPL");
