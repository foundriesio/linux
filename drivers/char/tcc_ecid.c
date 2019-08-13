/*
 * TCC CPU ID
 *
 * (C) 2007-2009 by smit Inc.
 * Author: jianjun jiang <jerryjianjun@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <asm/delay.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <linux/io.h>
#include <asm/system_info.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#define MODE    (1<<31)
#define CS         (1<<30)
#define FSET     (1<<29)
#define MCK      (1<<28)
#define PRCHG   (1<<27)
#define PROG     (1<<26)
#define SCK       (1<<25)
#define SDI       (1<<24)
#define SIGDEV  (1<<23)
#define A2        (1<<19)
#define A1        (1<<18)
#define A0        (1<<17)
//#define SEC     Hw16
//#define USER    (~Hw16)


#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
#define SEL	  14
#else
#define SEL	  15
#endif

// ECID Code
// -------- 31 ------------- 15 ----------- 0 --------
// [0]     |****************|****************|    : '*' is valid
// [1]     |0000000000000000|****************|    : 
//


struct ecid_platform_data
{
	void __iomem *ecid0;
	void __iomem *ecid2;
	void __iomem *ecid3;
	void __iomem *PMU;
	unsigned gPKG;
	unsigned gECID[2];
};

static void IO_UTIL_ReadECID (struct ecid_platform_data *pdata, int iA)
{
	int ecid_num, ecid_addr;
	unsigned int ecid_data_parallel[4][2];

	for(ecid_num=0;ecid_num<4;ecid_num++) { // 0: USER0, 1: SEC, 2:USR1, 3:SEC
		writel(MODE | (ecid_num<<SEL), pdata->ecid0);
		writel(MODE | (ecid_num<<SEL) | CS, pdata->ecid0) ;

		for(ecid_addr=0;ecid_addr<8;ecid_addr++) {
			writel(MODE | CS | (ecid_addr<<17) | (ecid_num<<SEL), pdata->ecid0);
			writel(MODE | CS | (ecid_addr<<17) | (ecid_num<<SEL) | SIGDEV, pdata->ecid0);
			writel(MODE | CS | (ecid_addr<<17) | (ecid_num<<SEL) | SIGDEV | PRCHG, pdata->ecid0);
			writel(MODE | CS | (ecid_addr<<17) | (ecid_num<<SEL) | SIGDEV | PRCHG | FSET, pdata->ecid0);
			writel(MODE | CS | (ecid_addr<<17) | (ecid_num<<SEL) | PRCHG  | FSET, pdata->ecid0);
			writel(MODE | CS | (ecid_addr<<17) | (ecid_num<<SEL) | FSET, pdata->ecid0);
			writel(MODE | CS | (ecid_addr<<17) | (ecid_num<<SEL), pdata->ecid0);
		}

		ecid_data_parallel[ecid_num][1] = readl_relaxed(pdata->ecid3);    // High 16 Bit
		ecid_data_parallel[ecid_num][0] = readl_relaxed(pdata->ecid2);    // Low  32 Bit
		writel(readl_relaxed(pdata->ecid0) & ~((0x7)<<17), pdata->ecid0);   // A2,A1,A0 are LOW
		writel(readl_relaxed(pdata->ecid0) & ~PRCHG, pdata->ecid0);
		//printk("ECID[%d] Parallel Read = 0x%04X%08X\n",ecid_num,ecid_data_parallel[ecid_num][1],ecid_data_parallel[ecid_num][0]);

		if (ecid_num == iA) {
			pdata->gECID[0] = ecid_data_parallel[ecid_num][0];
			pdata->gECID[1] = ecid_data_parallel[ecid_num][1];
		}
	}
}

static void IO_UTIL_ReadPKG (struct ecid_platform_data *pdata)
{
	printk("PMU = %08x\n", readl_relaxed(pdata->PMU));
	pdata->gPKG = ((readl_relaxed(pdata->PMU)>>16)&(0x7));
}

static ssize_t cpu_id_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ecid_platform_data *pdata = dev_get_drvdata(dev);
	IO_UTIL_ReadECID(pdata, 1);
	return sprintf(buf, "%08X%08X", pdata->gECID[1], pdata->gECID[0]);	// chip id
}

static ssize_t cpu_id_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return 1;
}

static ssize_t cpu_pkg_read(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_NAME, 0, 0, 0, 0, 0, 0, 0, &res);
	return sprintf(buf, "%08X", res.a0);	// chip id
}

static ssize_t cpu_pkg_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return 1;
}

static ssize_t cpu_rev_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%08X", system_rev);	// chip revision
}

static ssize_t cpu_rev_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return 1;
}



static DEVICE_ATTR(chip_id, 0644, cpu_id_read, cpu_id_write);
static DEVICE_ATTR(chip_pkg, 0644, cpu_pkg_read, cpu_pkg_write);
static DEVICE_ATTR(chip_rev, 0644, cpu_rev_read, cpu_rev_write);

static struct attribute * cpu_rev_sysfs_entries[] = {
	&dev_attr_chip_rev.attr,
	NULL,
};

static struct attribute_group cpu_rev_attr_group = {
	.name	= NULL,
	.attrs	= cpu_rev_sysfs_entries,
};

static struct attribute * cpu_pkg_sysfs_entries[] = {
	&dev_attr_chip_pkg.attr,
	NULL,
};

static struct attribute_group cpu_pkg_attr_group = {
	.name	= NULL,
	.attrs	= cpu_pkg_sysfs_entries,
};


static struct attribute * cpu_id_sysfs_entries[] = {
	&dev_attr_chip_id.attr,
	NULL,
};

static struct attribute_group cpu_id_attr_group = {
	.name	= NULL,
	.attrs	= cpu_id_sysfs_entries,
};

static int cpu_id_probe(struct platform_device *pdev)
{
	struct ecid_platform_data *pdata = NULL;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct ecid_platform_data), GFP_KERNEL);

	if(np)
		pdata->ecid0 = of_iomap(np, 0);
	else
	       ; // TODO: get platform resource

	if(np)
		pdata->ecid2 = of_iomap(np, 1);
	else
	       ; // TODO: get platform resource

	if(np)
		pdata->ecid3 = of_iomap(np, 2);
	else
	       ; // TODO: get platform resource

	if(np)
		pdata->PMU = of_iomap(np, 3);
	else
	       ; // TODO: get platform resource


	pdata->gECID[0] = 0;
	pdata->gECID[1] = 0;

	platform_set_drvdata(pdev, pdata);
		   
	ret = sysfs_create_group(&pdev->dev.kobj, &cpu_id_attr_group);
	ret = sysfs_create_group(&pdev->dev.kobj, &cpu_pkg_attr_group);
	ret = sysfs_create_group(&pdev->dev.kobj, &cpu_rev_attr_group);

	if(ret)
		printk("[CPU_ID] error creating sysfs entries\n");

	printk("[CPU_ID] probe is done\n");
	
	return 0;
}

static int cpu_id_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &cpu_id_attr_group);
	sysfs_remove_group(&pdev->dev.kobj, &cpu_pkg_attr_group);	
	sysfs_remove_group(&pdev->dev.kobj, &cpu_rev_attr_group);	
	return 0;
}

#ifdef CONFIG_PM
static int cpu_id_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int cpu_id_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define cpu_id_suspend	NULL
#define cpu_id_resume	NULL
#endif

static const struct of_device_id tcc_ecid_id_table[] = {
    {    .compatible = "telechips,tcc-cpu-id",    },
    {}
};
MODULE_DEVICE_TABLE(of, tcc_ecid_id_table);

static struct platform_driver cpu_id_driver = {
	.probe		= cpu_id_probe,
	.remove		= cpu_id_remove,
	.suspend	= cpu_id_suspend,
	.resume		= cpu_id_resume,
	.driver		= {
		.name	= "tcc-cpu-id",
		.of_match_table = of_match_ptr(tcc_ecid_id_table),
	},
};


module_platform_driver(cpu_id_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("jianjun jiang <jerryjianjun@gmail.com>");
MODULE_DESCRIPTION("Get CPU ID");
