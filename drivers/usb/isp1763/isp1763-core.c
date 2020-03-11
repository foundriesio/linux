// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the ST ISP1763 chip
 *
 * Largely based on isp1760 driver by:
 * Sebastian Siewior <bigeasy@linutronix.de>
 * Arvid Brodin <arvid.brodin@enea.com>
 *
 * And on the patch posted by:
 * Richard Retanubun <richardretanubun@ruggedcom.com>
 *
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include "isp1763-core.h"
#include "isp1763-hcd.h"
#include "isp1763-regs.h"

static void isp1763_init_core(struct isp1763_device *isp)
{
	u16 scratch, hwmode;

	/* Low-level chip reset */
	if (isp->rst_gpio) {
		gpiod_set_value_cansleep(isp->rst_gpio, 1);
		msleep(50);
		gpiod_set_value_cansleep(isp->rst_gpio, 0);
	}

	/* Read Chip ID register a few times to stabilize the host
	 * controller access */
	scratch = isp1763_read16(isp->regs, HC_CHIP_ID_REG);
	mdelay(20);
	scratch = isp1763_read16(isp->regs, HC_CHIP_ID_REG);
	scratch = isp1763_read16(isp->regs, HC_CHIP_ID_REG);

	/*
	 * Reset the host controller, including the CPU interface
	 * configuration.
	 */
	isp1763_write16(isp->regs, HC_RESET_REG, SW_RESET_RESET_ALL);
	msleep(100);

	/* Setup HW Mode Control: Initialized to HW default of 0x0000 */
	hwmode = 0;

	if (isp->devflags & ISP1763_FLAG_BUS_WIDTH_8)
		hwmode |= HW_DATA_BUS_8BIT;
	if (isp->devflags & ISP1763_FLAG_DACK_POL_HIGH)
		hwmode |= HW_DACK_POL_HIGH;
	if (isp->devflags & ISP1763_FLAG_DREQ_POL_HIGH)
		hwmode |= HW_DREQ_POL_HIGH;
	if (isp->devflags & ISP1763_FLAG_INTR_POL_HIGH)
		hwmode |= HW_INTR_HIGH_ACT;
	if (isp->devflags & ISP1763_FLAG_INTR_EDGE_TRIG)
		hwmode |= HW_INTR_EDGE_TRIG;

	pr_info("%s: devflags 0x%x hwmode 0x%x\n", __func__,
		isp->devflags, hwmode);

	/*
	 * We have to set this first in case we're in 8-bit mode.
	 * Write it twice to ensure correct upper bits if switching
	 * to 8-bit mode.
	 */
	isp1763_write16(isp->regs, HC_HW_MODE_CTRL, hwmode);
	isp1763_write16(isp->regs, HC_HW_MODE_CTRL, hwmode);

	dev_info(isp->dev, "bus width: %u\n",
		 isp->devflags & ISP1763_FLAG_BUS_WIDTH_8 ? 8 : 16);
}

int isp1763_register(struct resource *mem, int irq, unsigned long irqflags,
		     struct device *dev, unsigned int devflags)
{
	struct isp1763_device *isp;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	isp = devm_kzalloc(dev, sizeof(*isp), GFP_KERNEL);
	if (!isp)
		return -ENOMEM;

	isp->dev = dev;
	isp->devflags = devflags;

	isp->rst_gpio = devm_gpiod_get_optional(dev, NULL, GPIOD_OUT_HIGH);
	if (IS_ERR(isp->rst_gpio))
		return PTR_ERR(isp->rst_gpio);

	isp->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(isp->regs))
		return PTR_ERR(isp->regs);

	isp1763_init_core(isp);

	ret = isp1763_hcd_register(&isp->hcd, isp->regs, mem, irq,
				   irqflags | IRQF_SHARED, dev);
	if (ret < 0)
		return ret;

	dev_set_drvdata(dev, isp);

	return 0;
}

void isp1763_unregister(struct device *dev)
{
	struct isp1763_device *isp = dev_get_drvdata(dev);

	isp1763_hcd_unregister(&isp->hcd);
}

MODULE_DESCRIPTION("ISP1763 USB host-controller from ST");
MODULE_AUTHOR("Richard Retanubun <richardretanubun@ruggedcom.com>");
MODULE_LICENSE("GPL v2");
