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

#define TCC_PM_FW_DEV_NAME "tcc-pm-fw"

#define tcc_pm_fw_dev_err(dev, msg, err) \
	dev_err((dev), "[ERROR][pmfw] Failed to %s. (err: %d)\n", (msg), err)

#define tcc_pm_fw_dev_info(dev, msg, ...) \
	dev_info((dev), "[INFO][pmfw] " msg "\n", ##__VA_ARGS__)

struct bit_field {
	void __iomem *reg;
	u32 mask;
	u32 shift;
};

struct tcc_pm_fw_drvdata {
	struct platform_device *pdev;
	struct mbox_chan *mbox_chan;

	struct da9062 *pmic;
	struct notifier_block pm_noti;

	/* /sys/power/str sysfs */
	struct kobject pwrstr;
	struct bit_field *boot_reason;
	bool application_ready;
};

#define to_tcc_pm_fw_drvdata(ptr, member) \
	container_of(ptr, struct tcc_pm_fw_drvdata, member)

static ssize_t application_ready_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	struct tcc_pm_fw_drvdata *data = to_tcc_pm_fw_drvdata(kobj, pwrstr);

	return sprintf(buf, "%d\n", data->application_ready ? 1 : 0);
}

static ssize_t application_ready_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	struct tcc_pm_fw_drvdata *data = to_tcc_pm_fw_drvdata(kobj, pwrstr);
	struct mbox_chan *mbox_chan = data->mbox_chan;
	struct tcc_mbox_data msg;
	s32 ret;

	if (data->application_ready) {
		/* Ignore duplicated application ready events */
		return (ssize_t)count;
	}

	(void)memset(msg.cmd, 0, sizeof(*msg.cmd) * (size_t)MBOX_CMD_FIFO_SIZE);
	msg.cmd[0] = 1U;
	msg.data_len = 0;

	ret = mbox_send_message(mbox_chan, &msg);

	if (ret < 0) {
		tcc_pm_fw_dev_err(&data->pdev->dev,
				  "notify MCU of application ready", ret);
		return (ssize_t)ret;
	}

	data->application_ready = (bool)true;

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
	struct tcc_pm_fw_drvdata *data = to_tcc_pm_fw_drvdata(kobj, pwrstr);
	struct bit_field *field = data->boot_reason;

	u32 boot_reason = (readl(field->reg) >> field->shift) & field->mask;

	return sprintf(buf, "%u\n", boot_reason);
}

static struct kobj_attribute boot_reason_attr = {
	.attr = {
		.name = "boot_reason",
		.mode = 0400,
	},
	.show = boot_reason_show,
};

static struct attribute *pwrstr_attrs[] = {
	&application_ready_attr.attr,
	&boot_reason_attr.attr,
	NULL,
};

static const struct attribute_group pwrstr_attr_group = {
	.attrs = pwrstr_attrs,
};

static s32 tcc_pm_fw_power_sysfs_init(struct kobject *kobj)
{
	s32 ret;

	ret = kobject_init_and_add(kobj, power_kobj->ktype, power_kobj, "str");
	if (ret != 0) {
		return ret;
	}

	ret = sysfs_create_group(kobj, &pwrstr_attr_group);
	if (ret != 0) {
		kobject_put(kobj);
	}

	return ret;
}

static void tcc_pm_fw_power_sysfs_free(struct kobject *kobj)
{
	sysfs_remove_group(kobj, &pwrstr_attr_group);
	kobject_put(kobj);
}

static s32 pmic_ctrl_str_mode(struct da9062 *pmic, bool enter)
{
	struct reg_default set[2] = {
		{ DA9062AA_BUCK1_CONT, (s32)DA9062AA_BUCK1_CONF_MASK },
		{ DA9062AA_LDO2_CONT, (s32)DA9062AA_LDO2_CONF_MASK },
	};
	struct regmap *map;
	u32 val, i;
	s32 ret;

	if (pmic == NULL) {
		/* No PMIC to control (e.g. subcore doesn't control PMIC) */
		return 0;
	}

	map = pmic->regmap;

	for (i = 0; i < ARRAY_SIZE(set); i++) {
		val = enter ? set[i].def : (u32)0;
		ret = regmap_update_bits(map, set[i].reg, set[i].def, val);

		if (ret < 0) {
			/* XXX: Need to rollback register fields? */
			return ret;
		}
	}

	return 0;
}

static int tcc_pm_fw_pm_notifier_call(struct notifier_block *nb,
				      unsigned long action, void *data)
{
	struct tcc_pm_fw_drvdata *drvdata = to_tcc_pm_fw_drvdata(nb, pm_noti);
	s32 ret = 0;

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
		tcc_pm_fw_dev_err(&drvdata->pdev->dev, "set PMIC for STR mode",
				  ret);
	}

	return ret;
}

static s32 tcc_pm_fw_pm_notifier_init(struct notifier_block *nb)
{
	nb->notifier_call = tcc_pm_fw_pm_notifier_call;

	return register_pm_notifier(nb);
}

static void tcc_pm_fw_pm_notifier_free(struct notifier_block *nb)
{
	(void)unregister_pm_notifier(nb);
}

static struct bit_field *tcc_pm_fw_boot_reason_init(struct device *dev,
						    u32 *prop)
{
	struct bit_field *boot_reason;

	boot_reason = devm_kzalloc(dev, sizeof(struct bit_field), GFP_KERNEL);
	if (boot_reason == NULL) {
		return ERR_PTR(-ENOMEM);
	}

	boot_reason->reg = ioremap(prop[0], sizeof(u32));
	boot_reason->mask = prop[1];
	boot_reason->shift = prop[2];

	if (boot_reason->reg == NULL) {
		devm_kfree(dev, boot_reason);
		boot_reason = ERR_PTR(-ENOMEM);
	}

	return boot_reason;
}

static void tcc_pm_fw_boot_reason_free(struct device *dev,
				       struct bit_field *boot_reason)
{
	iounmap(boot_reason->reg);
	devm_kfree(dev, boot_reason);
}

static void tcc_pm_fw_event_listener(struct mbox_client *client, void *message)
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

	tcc_pm_fw_dev_info(client->dev, "Power event %u raised.", message_type);
	(void)sprintf(env, "PM_EVENT=%u", message_type);

	ret = kobject_uevent_env(&(client->dev->kobj), KOBJ_CHANGE, envp);

	if (ret != 0) {
		tcc_pm_fw_dev_err(client->dev, "set uevent env", ret);
	} else {
		tcc_pm_fw_dev_info(client->dev, "PM_EVENT env updated.");
	}
}

static struct mbox_chan *tcc_pm_fw_mbox_init(struct device *dev,
					     const char *name)
{
	struct mbox_client *cl;

	cl = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
	if (cl == NULL) {
		return ERR_PTR(-ENOMEM);
	}

	cl->dev = dev;
	cl->tx_block = (bool)true;
	cl->tx_tout = 100;
	cl->knows_txdone = (bool)false;
	cl->rx_callback = tcc_pm_fw_event_listener;

	return mbox_request_channel_byname(cl, name);
}

static void tcc_pm_fw_mbox_free(struct device *dev, struct mbox_chan *ch)
{
	struct mbox_client *cl = ch->cl;

	mbox_free_channel(ch);
	devm_kfree(dev, cl);
}

static struct da9062 *tcc_pm_fw_get_pmic(struct device_node *np)
{
	struct i2c_client *cl;

	if (np == NULL) {
		return NULL;
	}

	cl = of_find_i2c_device_by_node(np);
	if (cl == NULL) {
		return ERR_PTR(-ENOENT);
	}

	return i2c_get_clientdata(cl);
}

static int tcc_pm_fw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct tcc_pm_fw_drvdata *data;

	u32 boot_reason_pr[3];
	const char *mbox_name;
	struct device_node *pmic_np;
	bool err;
	s32 ret;

	/* Parse properties needed to initialize pm_fw interface driver */
	ret = of_property_read_u32_array(np, "boot-reason", boot_reason_pr, 3);
	if (ret != 0) {
		tcc_pm_fw_dev_err(dev, "read 'boot-reason' peroperty", ret);
		goto pre_init_error;
	}

	ret = of_property_read_string(np, "mbox-names", &mbox_name);
	if (ret != 0) {
		tcc_pm_fw_dev_err(dev, "read 'mbox-names' peroperty", ret);
		goto pre_init_error;
	}

	pmic_np = of_parse_phandle(np, "pmic", 0);

	/* Allocate memory for pm_fw interface driver data */
	data = devm_kzalloc(dev, sizeof(struct tcc_pm_fw_drvdata), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		tcc_pm_fw_dev_err(dev, "allocate driver data", ret);
		goto pre_init_error;
	}

	/* Initialize '/sys/power/str/' sysfs kobject */
	ret = tcc_pm_fw_power_sysfs_init(&data->pwrstr);
	if (ret != 0) {
		tcc_pm_fw_dev_err(dev, "init '/sys/power/str' sysfs", ret);
		goto power_sysfs_init_error;
	}

	/* Initialize notifier for power event handling */
	ret = tcc_pm_fw_pm_notifier_init(&data->pm_noti);
	if (ret != 0) {
		tcc_pm_fw_dev_err(dev, "register pm notifier", ret);
		goto pm_notifier_init_error;
	}

	/* Initialize access to boot-reason register field */
	data->boot_reason = tcc_pm_fw_boot_reason_init(dev, boot_reason_pr);
	err = IS_ERR(data->boot_reason);
	if (err) {
		tcc_pm_fw_dev_err(dev, "init access to boot reason", ret);
		goto boot_reason_init_error;
	}

	/* Initialize AP-MC mailbox channel to get ACC_ON/OFF events */
	data->mbox_chan = tcc_pm_fw_mbox_init(dev, mbox_name);
	err = IS_ERR(data->mbox_chan);
	if (err) {
		ret = (s32)PTR_ERR(data->mbox_chan);
		tcc_pm_fw_dev_err(dev, "init AP-MC mailbox channel", ret);
		goto mbox_init_error;
	}

	/* Get PMIC struct for entering/exiting STR mode */
	data->pmic = tcc_pm_fw_get_pmic(pmic_np);
	err = IS_ERR(data->pmic);
	if (err) {
		ret = (s32)PTR_ERR(data->pmic);
		tcc_pm_fw_dev_err(dev, "get i2c client for pmic", ret);
		goto pmic_init_error;
	}

	/* Now we can register driver data */
	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	return 0;

pmic_init_error:
	tcc_pm_fw_mbox_free(dev, data->mbox_chan);
mbox_init_error:
	tcc_pm_fw_boot_reason_free(dev, data->boot_reason);
boot_reason_init_error:
	tcc_pm_fw_pm_notifier_free(&data->pm_noti);
pm_notifier_init_error:
	tcc_pm_fw_power_sysfs_free(&data->pwrstr);
power_sysfs_init_error:
	devm_kfree(dev, data);
pre_init_error:
	return ret;
}

static const struct of_device_id tcc_pm_fw_match[2] = {
	{ .compatible = "telechips,pm-fw" },
	{ .compatible = "" }
};

static struct platform_driver tcc_pm_fw_driver = {
	.driver = {
		.name = TCC_PM_FW_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_pm_fw_match),
	},

	.probe  = tcc_pm_fw_probe,
};

builtin_platform_driver(tcc_pm_fw_driver);

MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips power management firmware driver");
MODULE_LICENSE("GPL");
