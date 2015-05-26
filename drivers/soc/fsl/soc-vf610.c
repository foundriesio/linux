/*
 * Copyright 2015 Toradex AG
 *
 * Author: Sanchayan Maity <sanchayan.maity@toradex.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

#define DRIVER_NAME "vf610-soc-bus"

#define MSCM_CPxCOUNT_OFFSET   0x0000002C
#define MSCM_CPxCFG1_OFFSET    0x00000014

static struct soc_device_attribute *soc_dev_attr;
static struct soc_device *soc_dev;

static int vf610_soc_probe(struct platform_device *pdev)
{
	struct regmap *ocotp_regmap, *mscm_regmap, *rom_regmap;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *soc_node;
	struct of_phandle_args pargs;
	char soc_type[] = "xx0";
	u32 cfg0_offset, cfg1_offset, rom_rev_offset;
	u32 soc_id1, soc_id2, rom_rev;
	u32 cpxcount, cpxcfg1;
	u64 soc_id;
	int ret;

	mscm_regmap = syscon_node_to_regmap(node);
	if (IS_ERR(mscm_regmap)) {
		dev_err(dev, "regmap lookup for mscm failed\n");
		return PTR_ERR(mscm_regmap);
	}

	soc_node = of_find_node_by_path("/soc");

	ret = of_parse_phandle_with_fixed_args(soc_node,
					"ocotp-cfg", 2, 0, &pargs);
	if (ret) {
		dev_err(dev, "lookup failed for ocotp-cfg node %d\n", ret);
		return ret;
	}

	ocotp_regmap = syscon_node_to_regmap(pargs.np);
	if (IS_ERR(ocotp_regmap)) {
		of_node_put(pargs.np);
		dev_err(dev, "regmap lookup for ocotp failed\n");
		return PTR_ERR(ocotp_regmap);
	}

	cfg0_offset = pargs.args[0];
	cfg1_offset = pargs.args[1];
	of_node_put(pargs.np);

	ret = of_parse_phandle_with_fixed_args(soc_node,
					"rom-revision", 1, 0, &pargs);
	if (ret) {
		dev_err(dev, "lookup failed for rom-revision node %d\n", ret);
		return ret;
	}

	rom_regmap = syscon_node_to_regmap(pargs.np);
	if (IS_ERR(rom_regmap)) {
		of_node_put(pargs.np);
		dev_err(dev, "regmap lookup for ocrom failed\n");
		return PTR_ERR(rom_regmap);
	}

	rom_rev_offset = pargs.args[0];
	of_node_put(pargs.np);

	ret = regmap_read(ocotp_regmap, cfg0_offset, &soc_id1);
	if (ret)
		return -ENODEV;

	ret = regmap_read(ocotp_regmap, cfg1_offset, &soc_id2);
	if (ret)
		return -ENODEV;

	soc_id = (u64) soc_id1 << 32 | soc_id2;
	add_device_randomness(&soc_id, sizeof(soc_id));

	ret = regmap_read(mscm_regmap, MSCM_CPxCOUNT_OFFSET, &cpxcount);
	if (ret)
		return -ENODEV;

	ret = regmap_read(mscm_regmap, MSCM_CPxCFG1_OFFSET, &cpxcfg1);
	if (ret)
		return -ENODEV;

	soc_type[0] = cpxcount ? '6' : '5'; /* Dual Core => VF6x0 */
	soc_type[1] = cpxcfg1 ? '1' : '0'; /* L2 Cache => VFx10 */

	ret = regmap_read(rom_regmap, rom_rev_offset, &rom_rev);
	if (ret)
		return -ENODEV;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENOMEM;

	soc_dev_attr->machine = kasprintf(GFP_KERNEL, "Freescale Vybrid");
	soc_dev_attr->soc_id = kasprintf(GFP_KERNEL, "%016llx", soc_id);
	soc_dev_attr->family = kasprintf(GFP_KERNEL, "Freescale Vybrid VF%s",
								 soc_type);
	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "%08x", rom_rev);

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr->revision);
		kfree(soc_dev_attr->family);
		kfree(soc_dev_attr->soc_id);
		kfree(soc_dev_attr->machine);
		kfree(soc_dev_attr);
		return -ENODEV;
	}

	return 0;
}

static int vf610_soc_remove(struct platform_device *pdev)
{
	if (soc_dev_attr) {
		kfree(soc_dev_attr->revision);
		kfree(soc_dev_attr->family);
		kfree(soc_dev_attr->soc_id);
		kfree(soc_dev_attr->machine);
		kfree(soc_dev_attr);
	}

	if (soc_dev)
		soc_device_unregister(soc_dev);

	return 0;
}

static const struct of_device_id vf610_soc_bus_match[] = {
	{ .compatible = "fsl,vf610-mscm-cpucfg", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vf610_soc_bus_match);

static struct platform_driver vf610_soc_driver = {
	.probe          = vf610_soc_probe,
	.remove         = vf610_soc_remove,
	.driver         = {
		.name   = DRIVER_NAME,
		.of_match_table = vf610_soc_bus_match,
	},
};

module_platform_driver(vf610_soc_driver);

MODULE_DESCRIPTION("Freescale VF610 SoC bus driver");
MODULE_LICENSE("GPL v2");
