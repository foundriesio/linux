/*
 * TCC LPDDR4 DRAM Tool
 *
 * (C) 2018 by Telechips
 * Author: Kamillen Kang <kamillen@telechips.com>
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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <linux/io.h>
#include <asm/system_info.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#define MONITOR_START_TIME 100	// ms
#define MONITOR_IDLE_TIME 500	// ms
#define MONITOR_CHECK_TIME 10	//ms

struct dram_tool_platform_data {
	struct delayed_work monitor_work;
	int cycle_time;		// ms
	bool monitor_enable;
	void __iomem *omc_base;
	void __iomem *omc_base2;
};

static unsigned int msec_to_cycle(unsigned int cycle_time)
{
	return ((cycle_time * 1000 * 1000) / 1429) * 1000;
}

static void monitor_work_func(struct work_struct *work)
{
	struct dram_tool_platform_data *pd =
	    container_of(work, struct dram_tool_platform_data,
			 monitor_work.work);
	struct arm_smccc_res res;

	if (pd->monitor_enable) {
	//pr_info("[%s] monitor status = 0
	//x%08x\n", __func__, readl(pd->omc_base + 0x798));

		if (readl(pd->omc_base + 0x798) == 0) {
			if (pd->cycle_time > 10) {
				arm_smccc_smc(SIP_DRAM_TOOL_TM_SET,
					      msec_to_cycle(pd->cycle_time), 0,
					      0, 0, 0, 0, 0, &res);
				schedule_delayed_work(&pd->monitor_work,
				      msecs_to_jiffies(pd->cycle_time
								       - 1));
			} else {
				pr_info
	    ("Bandwidth monitor error : measure time should be over 10 ms!\n");
				schedule_delayed_work(&pd->monitor_work,
						      msecs_to_jiffies
						      (MONITOR_IDLE_TIME));
			}
		} else if ((readl(pd->omc_base + 0x798) & (1 << 30)) != 0) {
			// print cycle, read_num, write_num
			pr_info
		    ("CH - 0, Cycle = %d, Read_num = %d, Write_num = %d",
			     readl(pd->omc_base + 0x79C),
			     readl(pd->omc_base + 0x7AC),
			     readl(pd->omc_base + 0x7B0));
			arm_smccc_smc(SIP_CHIP_NAME, 0, 0, 0, 0, 0, 0, 0, &res);
			if (res.a0 == 0x8990)
				pr_info
		    (" CH - 1, Cycle = %d, Read_num = %d, Write_num = %d\n",
				     readl(pd->omc_base2 + 0x79C),
				     readl(pd->omc_base2 + 0x7AC),
				     readl(pd->omc_base2 + 0x7B0));
			else
				pr_info("\n");

			if (pd->cycle_time > 10) {
				arm_smccc_smc(SIP_DRAM_TOOL_TM_SET,
					      msec_to_cycle(pd->cycle_time), 0,
					      0, 0, 0, 0, 0, &res);
				schedule_delayed_work(&pd->monitor_work,
					      msecs_to_jiffies(pd->cycle_time
								       - 1));
			} else {
				pr_info
	    ("Bandwidth monitor error : measure time should be over 10 ms!\n");
				schedule_delayed_work(&pd->monitor_work,
						      msecs_to_jiffies
						      (MONITOR_IDLE_TIME));
			}
		} else {
			pr_info("[%s] Monitor check out time\n", __func__);
			schedule_delayed_work(&pd->monitor_work,
					      msecs_to_jiffies
					      (MONITOR_CHECK_TIME));
		}

	} else {
		schedule_delayed_work(&pd->monitor_work,
				      msecs_to_jiffies(MONITOR_IDLE_TIME));
	}

}

static ssize_t monitor_enable_read(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct dram_tool_platform_data *pdata = dev_get_drvdata(dev);

	return sprintf(buf, "%d", pdata->monitor_enable);
}

static ssize_t monitor_enable_write(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct dram_tool_platform_data *pdata = dev_get_drvdata(dev);
	int ret = 0;
	bool val;

	ret = kstrtobool(buf, &val);
	if (ret < 0)
		return ret;

	pdata->monitor_enable = val;

	return 1;
}

static ssize_t cycle_time_read(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct dram_tool_platform_data *pdata = dev_get_drvdata(dev);

	return sprintf(buf, "%d", pdata->cycle_time);
}

static ssize_t cycle_time_write(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct dram_tool_platform_data *pdata = dev_get_drvdata(dev);

	sscanf(buf, "%d", &pdata->cycle_time);

	return sizeof(int);
}

static DEVICE_ATTR(monitor_enable, 0644, monitor_enable_read,
		   monitor_enable_write);
static DEVICE_ATTR(cycle_time, 0644, cycle_time_read, cycle_time_write);

static struct attribute *monitor_enable_sysfs_entries[] = {
	&dev_attr_monitor_enable.attr,
	NULL,
};

static struct attribute_group monitor_enable_attr_group = {
	.name = NULL,
	.attrs = monitor_enable_sysfs_entries,
};

static struct attribute *cycle_time_sysfs_entries[] = {
	&dev_attr_cycle_time.attr,
	NULL,
};

static struct attribute_group cycle_time_attr_group = {
	.name = NULL,
	.attrs = cycle_time_sysfs_entries,
};

static int dram_tool_probe(struct platform_device *pdev)
{
	struct dram_tool_platform_data *pdata = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct arm_smccc_res res;
	int ret;

	pdata =
	    devm_kzalloc(&pdev->dev, sizeof(struct dram_tool_platform_data),
			 GFP_KERNEL);

	if (np)
		pdata->omc_base = of_iomap(np, 0);
	else
		;

	if (np)
		pdata->omc_base2 = of_iomap(np, 1);
	else
		;

	arm_smccc_smc(SIP_DRAM_TOOL_TM_INIT, 0, 0, 0, 0, 0, 0, 0, &res);

	INIT_DELAYED_WORK(&pdata->monitor_work, monitor_work_func);

	pdata->cycle_time = 100;	//ms
	pdata->monitor_enable = 0;

	platform_set_drvdata(pdev, pdata);

	ret = sysfs_create_group(&pdev->dev.kobj, &monitor_enable_attr_group);
	if (ret)
		pr_info("[DRAM_TOOL] error creating sysfs entries\n");

	ret = sysfs_create_group(&pdev->dev.kobj, &cycle_time_attr_group);
	if (ret)
		pr_info("[DRAM_TOOL] error creating sysfs entries\n");

	pr_info("[DRAM_TOOL] probe is done\n");

	schedule_delayed_work(&pdata->monitor_work,
			      msecs_to_jiffies(MONITOR_START_TIME));

	return 0;
}

static int dram_tool_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &monitor_enable_attr_group);
	return 0;
}

static const struct of_device_id dram_tool_table[] = {
	{.compatible = "telechips,dram-tool",},
	{}
};

MODULE_DEVICE_TABLE(of, dram_tool_table);

static struct platform_driver dram_tool_driver = {
	.probe = dram_tool_probe,
	.remove = dram_tool_remove,
	.driver = {
		   .name = "dram-tool",
		   .of_match_table = of_match_ptr(dram_tool_table),
		   },
};

module_platform_driver(dram_tool_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kamillen Kang <Kamillen@telechips.com>");
MODULE_DESCRIPTION("LPDDR4 DRAM Tool");
