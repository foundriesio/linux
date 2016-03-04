/*
 * Copyright (C) 2016 Toradex AG.
 *
 * Author: Sanchayan Maity <sanchayan.maity@toradex.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

struct vf610_soc {
	struct device *dev;
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct nvmem_cell *ocotp_cfg0;
	struct nvmem_cell *ocotp_cfg1;
};

static int vf610_soc_probe(struct platform_device *pdev)
{
	struct vf610_soc *info;
	struct device *dev = &pdev->dev;
	struct device_node *soc_node;
	char soc_type[] = "xx0";
	size_t id1_len;
	size_t id2_len;
	u32 cpucount;
	u32 l2size;
	u32 rom_rev;
	u8 *socid1;
	u8 *socid2;
	int ret;

	info = devm_kzalloc(dev, sizeof(struct vf610_soc), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->dev = dev;

	info->ocotp_cfg0 = devm_nvmem_cell_get(dev, "cfg0");
	if (IS_ERR(info->ocotp_cfg0))
		return -EPROBE_DEFER;

	info->ocotp_cfg1 = devm_nvmem_cell_get(dev, "cfg1");
	if (IS_ERR(info->ocotp_cfg1))
		return -EPROBE_DEFER;

	socid1 = nvmem_cell_read(info->ocotp_cfg0, &id1_len);
	if (IS_ERR(socid1)) {
		dev_err(dev, "Could not read nvmem cell %ld\n",
			PTR_ERR(socid1));
		return PTR_ERR(socid1);
	}

	socid2 = nvmem_cell_read(info->ocotp_cfg1, &id2_len);
	if (IS_ERR(socid2)) {
		dev_err(dev, "Could not read nvmem cell %ld\n",
			PTR_ERR(socid2));
		return PTR_ERR(socid2);
	}
	add_device_randomness(socid1, id1_len);
	add_device_randomness(socid2, id2_len);

	soc_node = of_find_node_by_path("/soc");
	if (soc_node == NULL)
		return -ENODEV;

	ret = syscon_regmap_read_from_offset(soc_node,
					"fsl,rom-revision", &rom_rev);
	if (ret) {
		of_node_put(soc_node);
		return ret;
	}

	ret = syscon_regmap_read_from_offset(soc_node,
					"fsl,cpu-count", &cpucount);
	if (ret) {
		of_node_put(soc_node);
		return ret;
	}

	ret = syscon_regmap_read_from_offset(soc_node,
					"fsl,l2-size", &l2size);
	if (ret) {
		of_node_put(soc_node);
		return ret;
	}

	of_node_put(soc_node);

	soc_type[0] = cpucount ? '6' : '5'; /* Dual Core => VF6x0 */
	soc_type[1] = l2size ? '1' : '0'; /* L2 Cache => VFx10 */

	info->soc_dev_attr = devm_kzalloc(dev,
				sizeof(info->soc_dev_attr), GFP_KERNEL);
	if (!info->soc_dev_attr)
		return -ENOMEM;

	info->soc_dev_attr->machine = devm_kasprintf(dev,
					GFP_KERNEL, "Freescale Vybrid");
	info->soc_dev_attr->soc_id = devm_kasprintf(dev,
					GFP_KERNEL,
					"%02x%02x%02x%02x%02x%02x%02x%02x",
					socid1[3], socid1[2], socid1[1],
					socid1[0], socid2[3], socid2[2],
					socid2[1], socid2[0]);
	info->soc_dev_attr->family = devm_kasprintf(&pdev->dev,
					GFP_KERNEL, "Freescale Vybrid VF%s",
					soc_type);
	info->soc_dev_attr->revision = devm_kasprintf(dev,
					GFP_KERNEL, "%08x", rom_rev);

	platform_set_drvdata(pdev, info);

	info->soc_dev = soc_device_register(info->soc_dev_attr);
	if (IS_ERR(info->soc_dev))
		return -ENODEV;

	return 0;
}

static int vf610_soc_remove(struct platform_device *pdev)
{
	struct vf610_soc *info = platform_get_drvdata(pdev);

	if (info->soc_dev)
		soc_device_unregister(info->soc_dev);

	return 0;
}

static const struct of_device_id vf610_soc_bus_match[] = {
	{ .compatible = "fsl,vf610-soc-bus", },
	{ /* */ }
};

static struct platform_driver vf610_soc_driver = {
	.probe		= vf610_soc_probe,
	.remove		= vf610_soc_remove,
	.driver		= {
		.name = "vf610-soc-bus",
		.of_match_table = vf610_soc_bus_match,
	},
};
module_platform_driver(vf610_soc_driver);
