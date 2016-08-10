/*
 * Freescale VF610 Cortex-M4 Remote Processor driver
 *
 * Copyright (C) 2016 Toradex AG
 *
 * Based on wkup_m3_rproc.c by:
 * Copyright (C) 2014-2015 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>

#include "remoteproc_internal.h"

#define CM4_MEM_MAX	2

/*
 * struct vf610_m4_mem - Cm4 internal memory structure
 * @cpu_addr: MPU virtual address of the memory region
 * @bus_addr: Bus address used to access the memory region
 * @dev_addr: Device address from Wakeup M4 view
 * @size: Size of the memory region
 */
struct vf610_m4_mem {
	void __iomem *cpu_addr;
	phys_addr_t bus_addr;
	u32 dev_addr;
	size_t size;
};

/*
 * struct vf610_m4_rproc - Cm4 remote processor state
 * @rproc: rproc handle
 * @pdev: pointer to platform device
 * @mem: Cm4 memory information
 */
struct vf610_m4_rproc {
	struct rproc *rproc;
	struct platform_device *pdev;
	struct vf610_m4_mem mem[CM4_MEM_MAX];
	struct regmap *ccm;
	struct regmap *src;
};

static int vf610_m4_rproc_start(struct rproc *rproc)
{
	struct vf610_m4_rproc *cm4 = rproc->priv;

	regmap_write(cm4->src, 0x28, rproc->bootaddr);
	regmap_write(cm4->ccm, 0x8c, 0x00015a5a);

	return 0;
}

static int vf610_m4_rproc_stop(struct rproc *rproc)
{
	return -EINVAL;
}

static void *vf610_m4_rproc_da_to_va(struct rproc *rproc, u64 da, int len)
{
	struct vf610_m4_rproc *cm4 = rproc->priv;
	void *va = NULL;
	u32 offset;
	int i;

	if (len <= 0)
		return NULL;

	for (i = 0; i < CM4_MEM_MAX; i++) {
		if (da >= cm4->mem[i].dev_addr && da + len <=
		    cm4->mem[i].dev_addr +  cm4->mem[i].size) {
			offset = da -  cm4->mem[i].dev_addr;
			/* __force to make sparse happy with type conversion */
			va = (__force void *)(cm4->mem[i].cpu_addr + offset);
			break;
		}
	}

	return va;
}

static void vf610_m4_rproc_kick(struct rproc *rproc, int vqid)
{
	/*
	 * We provide this just to prevent Linux from complaining
	 * and causing a kernel panic.
	 */
}

static struct rproc_ops vf610_m4_rproc_ops = {
	.start		= vf610_m4_rproc_start,
	.stop		= vf610_m4_rproc_stop,
	.da_to_va	= vf610_m4_rproc_da_to_va,
	.kick		= vf610_m4_rproc_kick,
};

static const struct of_device_id vf610_m4_rproc_of_match[] = {
	{ .compatible = "fsl,vf610-m4", },
	{},
};

static int vf610_m4_rproc_probe(struct platform_device *pdev)
{
	const char *mem_names[CM4_MEM_MAX] = { "pc_ocram", "ps_ocram" };
	struct device *dev = &pdev->dev;
	struct vf610_m4_rproc *cm4;
	const char *fw_name;
	struct rproc *rproc;
	struct resource *res;
	const __be32 *addrp;
	u32 l4_offset = 0;
	u64 size;
	int ret;
	int i, j;

	ret = of_property_read_string(dev->of_node, "fsl,firmware",
				&fw_name);
	if (ret) {
		dev_err(dev, "No firmware filename given\n");
		return -ENODEV;
	}

	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm_runtime_get_sync() failed\n");
		goto err;
	}

	rproc = rproc_alloc(dev, "vf610_m4", &vf610_m4_rproc_ops,
			fw_name, sizeof(*cm4));
	if (!rproc) {
		ret = -ENOMEM;
		goto err;
	}

	cm4 = rproc->priv;
	cm4->rproc = rproc;
	cm4->pdev = pdev;

	cm4->src = syscon_regmap_lookup_by_compatible("fsl,vf610-src");
	cm4->ccm = syscon_regmap_lookup_by_compatible("fsl,vf610-ccm");

	for (i = 0; i < ARRAY_SIZE(mem_names); i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					mem_names[i]);

		/*
		  * The Cortex-M4 has address aliases, which means we
		  * have two device addresses which map to the same
		  * bus address. From Linux side, just use the already
		  * mapped region for those cases.
		  */
		for (j = i - 1; j >= 0; j--) {
			if (res->start == cm4->mem[j].bus_addr) {
				cm4->mem[i].cpu_addr =
					cm4->mem[j].cpu_addr;
				break;
			}
		}

		/* No previous mapping found, create a new mapping */
		if (j < 0) {
			cm4->mem[i].cpu_addr = devm_ioremap_resource(dev, res);
			if (IS_ERR(cm4->mem[i].cpu_addr)) {
				dev_err(&pdev->dev,
					"devm_ioremap_resource failed for resource %d\n", i);
				ret = PTR_ERR(cm4->mem[i].cpu_addr);
				goto err;
			}
		}

		cm4->mem[i].bus_addr = res->start;
		cm4->mem[i].size = resource_size(res);
		addrp = of_get_address(dev->of_node, i, &size, NULL);
		cm4->mem[i].dev_addr = be32_to_cpu(*addrp) - l4_offset;
	}

	dev_set_drvdata(dev, rproc);

	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "rproc_add failed\n");
		goto err_put_rproc;
	}

	rproc_boot(rproc);

	return 0;

err_put_rproc:
	rproc_put(rproc);
err:
	pm_runtime_put_noidle(dev);
	pm_runtime_disable(dev);
	return ret;
}

static int vf610_m4_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);

	rproc_del(rproc);
	rproc_put(rproc);
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM
static int vf610_m4_rpm_suspend(struct device *dev)
{
	return -EBUSY;
}

static int vf610_m4_rpm_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops vf610_m4_rproc_pm_ops = {
	SET_RUNTIME_PM_OPS(vf610_m4_rpm_suspend, vf610_m4_rpm_resume, NULL)
};

static struct platform_driver vf610_m4_rproc_driver = {
	.probe = vf610_m4_rproc_probe,
	.remove = vf610_m4_rproc_remove,
	.driver = {
		.name = "vf610_m4_rproc",
		.of_match_table = vf610_m4_rproc_of_match,
		.pm = &vf610_m4_rproc_pm_ops,
	},
};
module_platform_driver(vf610_m4_rproc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Freescale Vybrid Cortex-M4 Remote Processor driver");
MODULE_AUTHOR("Stefan Agner <stefan.agner@toradex.com>");
