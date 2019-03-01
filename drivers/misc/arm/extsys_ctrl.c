// SPDX-License-Identifier: GPL-2.0
/*
 * Arm Corstone700 external system reset control driver
 *
 * Copyright (C) 2019 Arm Ltd.
 *
 */

#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/init.h>

#define DRV_NAME "extsys_ctrl"
#define EXTSYS_MAX_DEVS 4

#define EXTSYS_RST_SIZE        U(0x8)
#define EXTSYS_RST_CTRL_OFF    U(0x0)
#define EXTSYS_RST_ST_OFF      U(0x4)

/* External system reset control indexes */
#define EXTSYS_CPU_WAIT        (0x0)
#define EXTSYS_RST_REQ         (0x1)

/* External system reset status masks */
#define EXTSYS_RST_ST_ACK_OFF     U(0x1)
/* No Reset Requested */
#define EXTSYS_RST_ST_ACK_NRR     (0x0 << EXTSYS_RST_ST_ACK_OFF)
/* Reset Request Complete */
#define EXTSYS_RST_ST_ACK_RRC     (0x2 << EXTSYS_RST_ST_ACK_OFF)
/* Reset Request Unable to Complete */
#define EXTSYS_RST_ST_ACK_RRUC    (0x3 << EXTSYS_RST_ST_ACK_OFF)

/* IOCTL commands */
#define EXTSYS_CPU_WAIT_DISABLE 0x0

struct extsys_ctrl {
	struct miscdevice miscdev;
	void __iomem *rstreg;
	void __iomem *streg;
};

#define CLEAR_BIT(addr, index) writel(readl(addr) & ~(1UL << index), addr)

static long extsys_ctrl_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	struct extsys_ctrl *drvdata = container_of(f->private_data,
		struct extsys_ctrl, miscdev);
	switch (cmd) {
	case EXTSYS_CPU_WAIT_DISABLE:
		CLEAR_BIT(drvdata->rstreg, EXTSYS_CPU_WAIT);
		break;
	default:
		break;
	}

	return 0;
}

const static struct file_operations extsys_ctrl_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = extsys_ctrl_ioctl,
};

static int extsys_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct extsys_ctrl *extsys;
	struct resource *res;
	void __iomem *rstreg;
	void __iomem *streg;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rstreg");
	rstreg = devm_ioremap_resource(dev, res);
	if (IS_ERR(rstreg))
		return PTR_ERR(rstreg);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "streg");
	streg = devm_ioremap_resource(dev, res);
	if (IS_ERR(streg))
		return PTR_ERR(streg);

	extsys = devm_kzalloc(dev, sizeof(*extsys), GFP_KERNEL);
	if (!extsys)
		return -ENOMEM;

	extsys->rstreg = rstreg;
	extsys->streg = streg;

	dev_set_drvdata(dev, (void *)extsys);

	extsys->miscdev.minor = MISC_DYNAMIC_MINOR;
	extsys->miscdev.name = DRV_NAME;
	extsys->miscdev.fops = &extsys_ctrl_fops;
	extsys->miscdev.parent = dev;

	ret = misc_register(&extsys->miscdev);
	if (ret) {
		dev_err(dev, "Unable to register external system control device\n");
		return ret;
	}

	dev_info(dev, "Arm External system control registered\n");

	return 0;
}

static int extsys_ctrl_remove(struct platform_device *pdev)
{
	struct extsys_ctrl *extsys = dev_get_drvdata(&pdev->dev);

	misc_deregister(&extsys->miscdev);
	return 0;
}

static const struct of_device_id extsys_ctrl_match[] = {
	{ .compatible = "arm,extsys_ctrl" },
	{ }
};

static struct platform_driver extsys_ctrl_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = extsys_ctrl_match,

	},
	.probe = extsys_ctrl_probe,
	.remove = extsys_ctrl_remove,
};

module_platform_driver(extsys_ctrl_driver)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Arm External System Control Driver");
MODULE_AUTHOR("Morten Borup Petersen <morten.petersen@arm.com>");
