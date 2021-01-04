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
};

#define read_bit_field(field) \
	((readl((field)->reg) >> (field)->shift) & (field)->mask)

struct pm_fw_drvdata {
	struct platform_device *pdev;

	/* suspend/resume event handler */
	struct notifier_block pm_noti;

	/* /sys/power/str sysfs */
	struct kobject pwrstr;

	struct bit_field boot_reason;
	bool application_ready;

	/* interface for MCU-AP communication */
	struct mbox_chan *mbox_chan;

	/* power contrl for DRAM retention */
#if defined(CONFIG_MFD_DA9062)
	struct da9062 *pmic;
#endif
};

#define to_pm_fw_drvdata(ptr, member) \
	(container_of(ptr, struct pm_fw_drvdata, member))

static ssize_t application_ready_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	struct pm_fw_drvdata *drvdata = to_pm_fw_drvdata(kobj, pwrstr);

	if (drvdata == NULL) {
		return sprintf(buf, "-1\n");
	}

	return sprintf(buf, "%d\n", drvdata->application_ready ? 1 : 0);
}

static ssize_t application_ready_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	struct pm_fw_drvdata *drvdata = to_pm_fw_drvdata(kobj, pwrstr);
	struct tcc_mbox_data msg;
	s32 ret;

	if (drvdata == NULL) {
		return (ssize_t)count;
	}

	if (drvdata->application_ready) {
		/* Ignore duplicated application ready events */
		return (ssize_t)count;
	}

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
	struct pm_fw_drvdata *drvdata = to_pm_fw_drvdata(kobj, pwrstr);
	u32 val;

	if (drvdata == NULL) {
		return sprintf(buf, "-1\n");
	}

	val = read_bit_field(&drvdata->boot_reason);

	return sprintf(buf, "%u\n", val);
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

ATTRIBUTE_GROUPS(pwrstr);

static s32 pm_fw_power_sysfs_init(struct kobject *kobj)
{
	s32 ret;

	if (power_kobj == NULL) {
		return -EINVAL;
	}

	ret = kobject_init_and_add(kobj, power_kobj->ktype, power_kobj, "str");
	if (ret != 0) {
		return ret;
	}

	ret = sysfs_create_groups(kobj, pwrstr_groups);
	if (ret != 0) {
		kobject_put(kobj);
	}

	return ret;
}

static void pm_fw_power_sysfs_free(struct kobject *kobj)
{
	sysfs_remove_groups(kobj, pwrstr_groups);
	kobject_put(kobj);
}

static s32 pmic_ctrl_str_mode(struct da9062 *pmic, bool enter)
{
	struct reg_default set[2] = {
		{ DA9062AA_BUCK1_CONT,	(u32)1 << DA9062AA_BUCK1_CONF_SHIFT },
		{ DA9062AA_LDO2_CONT,	(u32)1 << DA9062AA_LDO2_CONF_SHIFT },
	};
	struct regmap *map;
	u32 i;

	if (pmic == NULL) {
		/* No PMIC to control (e.g. subcore doesn't control PMIC) */
		return 0;
	}

	map = pmic->regmap;

	for (i = 0; i < ARRAY_SIZE(set); i++) {
		u32 val = enter ? set[i].def : (u32)0;
		s32 ret = regmap_update_bits(map, set[i].reg, set[i].def, val);

		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static int pm_fw_pm_notifier_call(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct pm_fw_drvdata *drvdata = to_pm_fw_drvdata(nb, pm_noti);
	s32 ret = 0;

	if (drvdata == NULL) {
		return -ENODEV;
	}

	switch (action) {
	case PM_SUSPEND_PREPARE:
		drvdata->application_ready = (bool)false;
		ret = pmic_ctrl_str_mode(drvdata->pmic, (bool)true);
		break;
	case PM_POST_SUSPEND:
		ret = pmic_ctrl_str_mode(drvdata->pmic, (bool)false);
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

	if (reason->reg == NULL) {
		return -ENOMEM;
	}

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

static s32 pm_fw_get_pmic(struct da9062 **pmic, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct i2c_client *cl;

	np = of_parse_phandle(np, "pmic", 0);
	if (np == NULL) {
		return 0;
	}

	cl = of_find_i2c_device_by_node(np);
	if (cl == NULL) {
		return -ENOENT;
	}

	*pmic = i2c_get_clientdata(cl);

	return 0;
}

static int pm_fw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pm_fw_drvdata *drvdata;
	s32 ret = -ENODEV;

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
	ret = pm_fw_power_sysfs_init(&drvdata->pwrstr);
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
		ret = (s32)PTR_ERR(drvdata->pmic);
		pm_fw_err(dev, "get i2c client for pmic", ret);
		goto pmic_init_error;
	}

	/* Now we can register driver data */
	platform_set_drvdata(pdev, drvdata);
	drvdata->pdev = pdev;

	return 0;

pmic_init_error:
	pm_fw_mbox_free(drvdata->mbox_chan, dev);
mbox_init_error:
	pm_fw_boot_reason_free(&drvdata->boot_reason);
boot_reason_init_error:
	pm_fw_pm_notifier_free(&drvdata->pm_noti);
pm_notifier_init_error:
	pm_fw_power_sysfs_free(&drvdata->pwrstr);
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
