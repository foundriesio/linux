// SPDX-License-Identifier: GPL-2.0
/*
 * Glue code for the ISP1763 driver and bus
 * Currently there is support for:
 * - OpenFirmware
 * - PCI (not-tested; as-is port from isp1760)
 * - PDEV (generic platform device centralized driver model)
 *
 * Based on isp1760 driver by:
 * Sebastian Siewior <bigeasy@linutronix.de>
 * Arvid Brodin <arvid.brodin@enea.com>
 *
 * And on the patch posted by:
 * Richard Retanubun <richardretanubun@ruggedcom.com>
 *
 */

#include <linux/usb.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/isp1763.h>
#include <linux/usb/hcd.h>

#include "isp1763-core.h"
#include "isp1763-regs.h"

static int isp1763_plat_probe(struct platform_device *pdev)
{
	unsigned long irqflags;
	unsigned int devflags = 0;
	struct resource *mem_res;
	struct resource *irq_res;
	int ret;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq_res) {
		pr_warn("isp1763: IRQ resource not available\n");
		return -ENODEV;
	}

	irqflags = irq_res->flags & IRQF_TRIGGER_MASK;

	if (IS_ENABLED(CONFIG_OF) && pdev->dev.of_node) {
		struct device_node *dp = pdev->dev.of_node;
		u32 bus_width = 0;

		/* Some systems wire up only 8 of the 16 data lines */
		of_property_read_u32(dp, "bus-width", &bus_width);
		if (bus_width == 8)
			devflags |= ISP1763_FLAG_BUS_WIDTH_8;

		if (of_property_read_bool(dp, "port1-otg"))
			devflags |= ISP1763_FLAG_OTG_EN;

		if (of_property_read_bool(dp, "dack-polarity"))
			devflags |= ISP1763_FLAG_DACK_POL_HIGH;

		if (of_property_read_bool(dp, "dreq-polarity"))
			devflags |= ISP1763_FLAG_DREQ_POL_HIGH;

		if (of_property_read_bool(dp, "intr-polarity"))
			devflags |= ISP1763_FLAG_INTR_POL_HIGH;

		if (of_property_read_bool(dp, "intr-level"))
			devflags |= ISP1763_FLAG_INTR_EDGE_TRIG;
	} else if (dev_get_platdata(&pdev->dev)) {
		struct isp1763_platform_data *pdata =
			dev_get_platdata(&pdev->dev);

		if (pdata->bus_width_8)
			devflags |= ISP1763_FLAG_BUS_WIDTH_8;
		if (pdata->port1_otg)
			devflags |= ISP1763_FLAG_OTG_EN;
		if (pdata->dack_polarity_high)
			devflags |= ISP1763_FLAG_DACK_POL_HIGH;
		if (pdata->dreq_polarity_high)
			devflags |= ISP1763_FLAG_DREQ_POL_HIGH;
		if (pdata->intr_polarity_high)
			devflags |= ISP1763_FLAG_INTR_POL_HIGH;
		if (pdata->intr_edge_trigger)
			devflags |= ISP1763_FLAG_INTR_EDGE_TRIG;
	}

	ret = isp1763_register(mem_res, irq_res->start, irqflags, &pdev->dev,
			       devflags);
	if (ret < 0)
		return ret;

	pr_info("ISP1763 USB device initialised\n");
	return 0;
}

static int isp1763_plat_remove(struct platform_device *pdev)
{
	isp1763_unregister(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id isp1763_of_match[] = {
	{ .compatible = "st,usb-isp1763", },
	{ },
};
MODULE_DEVICE_TABLE(of, isp1763_of_match);
#endif

static struct platform_driver isp1763_plat_driver = {
	.probe	= isp1763_plat_probe,
	.remove	= isp1763_plat_remove,
	.driver	= {
		.name	= "isp1763",
		.of_match_table = of_match_ptr(isp1763_of_match),
	},
};

static int __init isp1763_init(void)
{
	int ret, any_ret = -ENODEV;

	isp1763_init_kmem_once();

	ret = platform_driver_register(&isp1763_plat_driver);
	if (!ret)
		any_ret = 0;

	if (any_ret)
		isp1763_deinit_kmem_cache();
	return any_ret;
}
module_init(isp1763_init);

static void __exit isp1763_exit(void)
{
	platform_driver_unregister(&isp1763_plat_driver);
	isp1763_deinit_kmem_cache();
}
module_exit(isp1763_exit);
