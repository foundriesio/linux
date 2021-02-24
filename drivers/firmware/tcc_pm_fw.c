// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>

#include <linux/i2c.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/tcc_multi_mbox.h>

#include <linux/regulator/consumer.h>
#include <linux/mfd/da9062/core.h>
#include <linux/regmap.h>

#define pm_fw_err(dev, msg, err) \
	(dev_err((dev), "[ERROR][pmfw] Failed to " msg " (err: %d)\n", (err)))

#define pm_fw_info(dev, msg, ...) \
	(dev_info((dev), "[INFO][pmfw] " msg "\n", ##__VA_ARGS__))

struct bit_field {
	void __iomem *reg;
	u32 mask;
	u32 shift;
	u32 val;
};

static inline void update_bit_field_val(struct bit_field *field)
{
	u32 regval;

	if (field != NULL) {
		regval = readl(field->reg);
		field->val = (regval >> field->shift) & field->mask;
	}
}

struct pmic {
	struct regmap *da9062_regmap;
	struct regmap *da9131_regmap[2];
};

struct pm_fw_drvdata {
	struct platform_device *pdev;

	/* suspend/resume event handler */
	struct notifier_block pm_noti;

	/* /sys/power/str sysfs */
	struct bit_field boot_reason;
	bool application_ready;

	/* interface for MCU-AP communication */
	struct mbox_chan *mbox_chan;

	/* power contrl for DRAM retention */
	struct pmic pmic;
};

struct pm_fw_drvdata *pm_fw_drvdata_ptr;

static inline struct pm_fw_drvdata *get_pm_fw_drvdata(void)
{
	return pm_fw_drvdata_ptr;
}

static ssize_t application_ready_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	struct pm_fw_drvdata *drvdata = get_pm_fw_drvdata();

	if (drvdata == NULL) {
		return sprintf(buf, "-1\n");
	}

	return sprintf(buf, "%d\n", drvdata->application_ready ? 1 : 0);
}

static ssize_t application_ready_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	struct pm_fw_drvdata *drvdata = get_pm_fw_drvdata();
	struct tcc_mbox_data msg;
	s32 ret;

	if (drvdata == NULL) {
		return (ssize_t)count;
	}

	if (drvdata->application_ready) {
		/* Ignore duplicated application ready events */
		return (ssize_t)count;
	}

	update_bit_field_val(&drvdata->boot_reason);

	(void)memset(msg.cmd, 0, sizeof(*msg.cmd) * (size_t)MBOX_CMD_FIFO_SIZE);
	msg.cmd[0] = 1U;
	msg.data_len = 0;

	ret = mbox_send_message(drvdata->mbox_chan, &msg);

	if (ret < 0) {
		pm_fw_err(&drvdata->pdev->dev, "notify application ready", ret);
		return (ssize_t)ret;
	}

	drvdata->application_ready = (bool)true;

	return (ssize_t)count;
}

static struct kobj_attribute application_ready_attr = {
	.attr = {
		.name = "application_ready",
		.mode = 0600,
	},
	.show = application_ready_show,
	.store = application_ready_store,
};

static ssize_t boot_reason_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct pm_fw_drvdata *drvdata = get_pm_fw_drvdata();

	if (drvdata == NULL) {
		return sprintf(buf, "-1\n");
	}

	if (drvdata->boot_reason.val == 0) {
		update_bit_field_val(&drvdata->boot_reason);
	}

	return sprintf(buf, "%u\n", drvdata->boot_reason.val);
}

static struct kobj_attribute boot_reason_attr = {
	.attr = {
		.name = "boot_reason",
		.mode = 0400,
	},
	.show = boot_reason_show,
	.store = NULL,
};

static struct attribute *pwrstr_attrs[] = {
	&application_ready_attr.attr,
	&boot_reason_attr.attr,
	NULL,
};

static const struct attribute_group pwrstr_group = {
	.name = "str",
	.attrs = pwrstr_attrs,
};

static s32 pm_fw_power_sysfs_init(void)
{
	s32 ret;

	if (power_kobj == NULL) {
		return -EINVAL;
	}

	return sysfs_create_group(power_kobj, &pwrstr_group);
}

static void pm_fw_power_sysfs_free(void)
{
	sysfs_remove_group(power_kobj, &pwrstr_group);
}

static s32 pmic_ctrl_str_mode(struct pmic *pmic, u32 enter)
{
	struct regmap *map;
	s32 i;
	s32 ret;

	if (pmic == NULL) {
		/* No PMIC to control (e.g. subcore doesn't control PMIC) */
		return 0;
	}

#if defined(CONFIG_MFD_DA9062)
	map = pmic->da9062_regmap;
	if (map == NULL) {
		return 0;
	}

	/*
	 * Keep BUCK1/LDO2 on in power-down for STR support.
	 * Must be reset to default config on wake-up.
	 */
	ret = regmap_update_bits(map, DA9062AA_BUCK1_CONT,
				 DA9062AA_BUCK1_CONF_MASK,
				 enter << DA9062AA_BUCK1_CONF_SHIFT);
	if (ret < 0) {
		return ret;
	}

	ret = regmap_update_bits(map, DA9062AA_LDO2_CONT,
				 DA9062AA_LDO2_CONF_MASK,
				 enter << DA9062AA_LDO2_CONF_SHIFT);
	if (ret < 0) {
		return ret;
	}

	/*
	 * S/W Workaround for power sequence issue (DA9062)
	 * - Add 16.4 ms delay for CORE 0P8 power off
	 * - Enable IRQ for SYS_EN
	 */
	ret = regmap_update_bits(map, DA9062AA_WAIT, 0xff, 0x96);
	if (ret < 0) {
		return ret;
	}

	ret = regmap_update_bits(map, DA9062AA_ID_32_31, 0xff, 0x03);
	if (ret < 0) {
		return ret;
	}

	ret = regmap_update_bits(map, DA9062AA_IRQ_MASK_C, 0x10, 0x00);
	if (ret < 0) {
		return ret;
	}
#endif
#if defined(CONFIG_REGULATOR_DA9121)
	/*
	 * S/W Workaround for power sequence issue (DA9131 OTP-42/43 v1)
	 * - Modify voltage slew rate
	 * - Modify GPIO2 pulldown enable
	 */
	for (i = 0; i < 2; i++) {
		map = pmic->da9131_regmap[i];
		if (map == NULL) {
			return 0;
		}

		ret = regmap_update_bits(map, 0x20, 0xff, 0x49);
		if (ret < 0) {
			return ret;
		}

		ret = regmap_update_bits(map, 0x21, 0xff, 0x49);
		if (ret < 0) {
			return ret;
		}

		ret = regmap_update_bits(map, 0x28, 0xff, 0x49);
		if (ret < 0) {
			return ret;
		}

		ret = regmap_update_bits(map, 0x29, 0xff, 0x49);
		if (ret < 0) {
			return ret;
		}

		ret = regmap_update_bits(map, 0x13, 0xff, 0x08);
		if (ret < 0) {
			return ret;
		}

		ret = regmap_update_bits(map, 0x15, 0xff, 0x08);
		if (ret < 0) {
			return ret;
		}
	}
#endif
	return 0;
}

static int pm_fw_pm_notifier_call(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct pm_fw_drvdata *drvdata = get_pm_fw_drvdata();
	s32 ret = 0;

	if (drvdata == NULL) {
		return -ENODEV;
	}

	switch (action) {
	case PM_SUSPEND_PREPARE:
		drvdata->application_ready = (bool)false;
		drvdata->boot_reason.val = 0;
		ret = pmic_ctrl_str_mode(&drvdata->pmic, (u32)1);
		break;
	case PM_POST_SUSPEND:
		ret = pmic_ctrl_str_mode(&drvdata->pmic, (u32)0);
		break;
	default:
		/* Nothing to do */
		break;
	}

	if (ret < 0) {
		pm_fw_err(&drvdata->pdev->dev, "set PMIC for STR mode", ret);
	}

	return ret;
}

static s32 pm_fw_pm_notifier_init(struct notifier_block *nb)
{
	nb->notifier_call = &pm_fw_pm_notifier_call;

	return register_pm_notifier(nb);
}

static void pm_fw_pm_notifier_free(struct notifier_block *nb)
{
	(void)unregister_pm_notifier(nb);
}

static s32 pm_fw_boot_reason_init(struct bit_field *reason, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 prop[3];
	s32 ret;

	ret = of_property_read_u32_array(np, "boot-reason", prop, 3);
	if (ret != 0) {
		return ret;
	}

	reason->reg = ioremap(prop[0], sizeof(u32));
	reason->mask = prop[1];
	reason->shift = prop[2];
	reason->val = 0;

	if (reason->reg == NULL) {
		return -ENOMEM;
	}

	update_bit_field_val(reason);

	return 0;
}

static void pm_fw_boot_reason_free(struct bit_field *boot_reason)
{
	iounmap(boot_reason->reg);
}

static void pm_fw_event_listener(struct mbox_client *client, void *message)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *) message;
	char env[13]; /* "PM_EVENT=___\0", where ___ is decimal in 0 ~ 255 */
	char *envp[2] = {env, NULL};
	u32 message_type = 0;
	s32 ret;

	if ((client == NULL) || (msg == NULL)) {
		/* Never be happened, but check for just in case ... */
		return;
	}

	message_type = msg->cmd[0] & 0xFFU;

	pm_fw_info(client->dev, "Power event %u raised", message_type);
	(void)sprintf(env, "PM_EVENT=%u", message_type);

	ret = kobject_uevent_env(&(client->dev->kobj), KOBJ_CHANGE, envp);

	if (ret != 0) {
		pm_fw_err(client->dev, "set uevent env", ret);
	} else {
		pm_fw_info(client->dev, "PM_EVENT env updated");
	}
}

static s32 pm_fw_mbox_init(struct mbox_chan **ch, struct device *dev)
{
	struct device_node *np = dev->of_node;
	const char *mbox_name;
	struct mbox_client *cl;
	bool err;
	s32 ret;

	ret = of_property_read_string(np, "mbox-names", &mbox_name);
	if (ret != 0) {
		return ret;
	}

	cl = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
	if (cl == NULL) {
		return -ENOMEM;
	}

	cl->dev = dev;
	cl->tx_block = (bool)true;
	cl->tx_tout = 100;
	cl->knows_txdone = (bool)false;
	cl->rx_callback = &pm_fw_event_listener;

	*ch = mbox_request_channel_byname(cl, mbox_name);
	err = IS_ERR(*ch);

	if (err) {
		devm_kfree(dev, cl);
		return (s32)PTR_ERR(*ch);
	}

	return 0;
}

static void pm_fw_mbox_free(struct mbox_chan *ch, struct device *dev)
{
	struct mbox_client *cl;

	if (ch == NULL) {
		/* XXX: Should not happen */
		return;
	}

	cl = ch->cl;

	mbox_free_channel(ch);
	devm_kfree(dev, cl);
}

static s32 pm_fw_get_pmic(struct pmic *pmic, struct device *dev)
{
	struct regulator *regulator;
	bool err;

#if defined(CONFIG_MFD_DA9062)
	regulator = devm_regulator_get_exclusive(dev, "memq_1p1");
	err = IS_ERR(regulator);
	if (err) {
		pmic->da9062_regmap = NULL;
		return 0;
	}
	pmic->da9062_regmap = regulator_get_regmap(regulator);
	err = IS_ERR(pmic->da9062_regmap);
	if (err) {
		pmic->da9062_regmap = NULL;
		return -ENOENT;
	}
#endif

#if defined(CONFIG_REGULATOR_DA9121)
	/* D0 */
	regulator = devm_regulator_get_exclusive(dev, "core_0p8");
	err = IS_ERR(regulator);
	if (err) {
		pmic->da9131_regmap[0] = NULL;
		return 0;
	}
	pmic->da9131_regmap[0] = regulator_get_regmap(regulator);
	err = IS_ERR(pmic->da9131_regmap[0]);
	if (err) {
		pmic->da9131_regmap[0] = NULL;
		return 0;
	}

	/* D2 */
	regulator = devm_regulator_get_exclusive(dev, "cpu_1p0");
	err = IS_ERR(regulator);
	if (err) {
		pmic->da9131_regmap[1] = NULL;
		return 0;
	}
	pmic->da9131_regmap[1] = regulator_get_regmap(regulator);
	err = IS_ERR(pmic->da9131_regmap[0]);
	if (err) {
		pmic->da9131_regmap[1] = NULL;
		return 0;
	}
#endif
	return 0;
}

static int pm_fw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pm_fw_drvdata *drvdata;
	s32 ret = -ENODEV;

	pm_fw_drvdata_ptr = NULL;

	if (dev == NULL) {
		pm_fw_err(dev, "get device", ret);
		goto pre_init_error;
	}

	/* Allocate memory for pm_fw interface driver data */
	drvdata = devm_kzalloc(dev, sizeof(struct pm_fw_drvdata), GFP_KERNEL);
	if (drvdata == NULL) {
		ret = -ENOMEM;
		pm_fw_err(dev, "allocate driver data", ret);
		goto pre_init_error;
	}

	/* Initialize '/sys/power/str/' sysfs kobject */
	ret = pm_fw_power_sysfs_init();
	if (ret != 0) {
		pm_fw_err(dev, "init '/sys/power/str' sysfs", ret);
		goto power_sysfs_init_error;
	}

	/* Initialize notifier for power event handling */
	ret = pm_fw_pm_notifier_init(&drvdata->pm_noti);
	if (ret != 0) {
		pm_fw_err(dev, "register pm notifier", ret);
		goto pm_notifier_init_error;
	}

	/* Initialize access to boot-reason register field */
	ret = pm_fw_boot_reason_init(&drvdata->boot_reason, dev);
	if (ret != 0) {
		pm_fw_err(dev, "init access to boot reason", ret);
		goto boot_reason_init_error;
	}

	/* Initialize AP-MC mailbox channel to get ACC_ON/OFF events */
	ret = pm_fw_mbox_init(&drvdata->mbox_chan, dev);
	if (ret != 0) {
		pm_fw_err(dev, "init AP-MC mailbox channel", ret);
		goto mbox_init_error;
	}

	/* Get PMIC struct for entering/exiting STR mode */
	ret = pm_fw_get_pmic(&drvdata->pmic, dev);
	if (ret != 0) {
		pm_fw_err(dev, "get regmap of regulator", ret);
		goto pmic_init_error;
	}

	/* Now we can register driver data */
	platform_set_drvdata(pdev, drvdata);
	drvdata->pdev = pdev;
	pm_fw_drvdata_ptr = drvdata;

	return 0;

pmic_init_error:
	pm_fw_mbox_free(drvdata->mbox_chan, dev);
mbox_init_error:
	pm_fw_boot_reason_free(&drvdata->boot_reason);
boot_reason_init_error:
	pm_fw_pm_notifier_free(&drvdata->pm_noti);
pm_notifier_init_error:
	pm_fw_power_sysfs_free();
power_sysfs_init_error:
	devm_kfree(dev, drvdata);
pre_init_error:
	return ret;
}

static const struct of_device_id pm_fw_match[2] = {
	{ .compatible = "telechips,pm-fw" },
	{ .compatible = "" }
};

static struct platform_driver pm_fw_driver = {
	.driver = {
		.name = "tcc-pm-fw",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pm_fw_match),
	},

	.probe  = pm_fw_probe,
};

builtin_platform_driver(pm_fw_driver);

MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips power management firmware driver");
MODULE_LICENSE("GPL");
