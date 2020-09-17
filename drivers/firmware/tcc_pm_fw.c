// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define TCC_PM_FW_DEV_NAME "tcc-pm-fw"

struct bit_field {
	void __iomem *reg;
	u32 mask;
	u32 shift;
};

struct tcc_pm_fw_drvdata {
	struct platform_device *pdev;
	struct mbox_chan *mbox_chan;

	/* /sys/power/str sysfs */
	struct kobject pwrstr;
	struct bit_field *boot_reason;
	bool application_ready;
};

#define pwrstr_to_tcc_pm_fw_drvdata(kobj) \
	container_of(kobj, struct tcc_pm_fw_drvdata, pwrstr)

inline void tcc_pm_fw_dev_err(struct device *dev, const char *msg, int err)
{
	dev_err(dev, "[ERROR][PM_FW] Failed to %s. (err: %d)\n", msg, err);
}

ssize_t application_ready_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct tcc_pm_fw_drvdata *data = pwrstr_to_tcc_pm_fw_drvdata(kobj);
	return sprintf(buf, "%d\n", data->application_ready ? 1: 0);
}

ssize_t application_ready_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct tcc_pm_fw_drvdata *data = pwrstr_to_tcc_pm_fw_drvdata(kobj);
	struct mbox_chan *mbox_chan = data->mbox_chan;
	struct tcc_mbox_data msg;
	int ret;

	if (data->application_ready)
		return count;

	memset(msg.cmd, 0, sizeof(*msg.cmd) * MBOX_CMD_FIFO_SIZE);
	msg.cmd[0] = 1U;
	msg.data_len = 0;

	ret = mbox_send_message(mbox_chan, &msg);

	if (ret < 0) {
		tcc_pm_fw_dev_err(&data->pdev->dev,
				"notify MCU of application ready", ret);
		return ret;
	}

	data->application_ready = true;

	return count;
}

static struct kobj_attribute application_ready_attr = {
	.attr = {
		.name = "application_ready",
		.mode = 0600,
	},
	.show = application_ready_show,
	.store = application_ready_store,
};

ssize_t boot_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct tcc_pm_fw_drvdata *data = pwrstr_to_tcc_pm_fw_drvdata(kobj);
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

static int tcc_pm_fw_power_sysfs_init(struct kobject *kobj)
{
	int ret;

	ret = kobject_init_and_add(kobj, power_kobj->ktype, power_kobj, "str");
	if (ret != 0)
		return ret;

	ret = sysfs_create_group(kobj, &pwrstr_attr_group);
	if (ret != 0)
		kobject_put(kobj);

	return ret;
}

static inline void tcc_pm_fw_power_sysfs_free(struct kobject *kobj)
{
	sysfs_remove_group(kobj, &pwrstr_attr_group);
	kobject_put(kobj);
}

static struct bit_field *tcc_pm_fw_boot_reason_init(struct device *dev,
		u32 *prop)
{
	struct bit_field *boot_reason;

	boot_reason = devm_kzalloc(dev, sizeof(struct bit_field), GFP_KERNEL);
	if (boot_reason == NULL)
		return ERR_PTR(-ENOMEM);

	boot_reason->reg = ioremap(prop[0], sizeof(u32));
	boot_reason->mask = prop[1];
	boot_reason->shift = prop[2];

	if (boot_reason->reg == NULL) {
		devm_kfree(dev, boot_reason);
		boot_reason = ERR_PTR(-ENOMEM);
	}

	return boot_reason;
}

static inline void tcc_pm_fw_boot_reason_free(struct device *dev,
		struct bit_field *boot_reason)
{
	iounmap(boot_reason->reg);
	devm_kfree(dev, boot_reason);
}

static void tcc_pm_fw_event_listener(struct mbox_client *client, void *message)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *) message;
	unsigned int message_type = 0;
	char env[13]; /* "PM_EVENT=___\0", where ___ is decimal in 0 ~ 255 */
	char *envp[2] = {env, NULL};
	int ret;

	if (client == NULL || msg == NULL)
		return;

	message_type = msg->cmd[0] & 0xFFU;

	sprintf(env, "PM_EVENT=%u", message_type);

	ret = kobject_uevent_env(&(client->dev->kobj), KOBJ_CHANGE, envp);

	if (ret != 0)
		tcc_pm_fw_dev_err(client->dev, "set uevent env", ret);
}

static struct mbox_chan *tcc_pm_fw_mbox_init(struct device *dev,
		const char *name)
{
	struct mbox_client *cl;

	cl = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
	if (cl == NULL)
		return ERR_PTR(-ENOMEM);

	cl->dev = dev;
	cl->tx_block = (bool) true;
	cl->tx_tout = 100;
	cl->knows_txdone = (bool) false;
	cl->rx_callback = tcc_pm_fw_event_listener;

	return mbox_request_channel_byname(cl, name);
}

static inline void tcc_pm_fw_mbox_free(struct device *dev, struct mbox_chan *ch)
{
	struct mbox_client *cl = ch->cl;

	mbox_free_channel(ch);
	devm_kfree(dev, cl);
}

int tcc_pm_fw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct tcc_pm_fw_drvdata *data;

	u32 boot_reason_pr[3];
	const char *mbox_name;
	int ret;

	/* Parse properties needed to initialize pm_fw interface driver */
	ret = of_property_read_u32_array(np, "boot-reason", boot_reason_pr, 3);
	if (ret != 0) {
		tcc_pm_fw_dev_err(dev, "read `boot-reason` peroperty", ret);
		goto pre_init_error;
	}

	ret = of_property_read_string(np, "mbox-names", &mbox_name);
	if (ret != 0) {
		tcc_pm_fw_dev_err(dev, "read `mbox-names` peroperty", ret);
		goto pre_init_error;
	}

	/* Allocate memory for pm_fw interface driver data */
	data = devm_kzalloc(dev, sizeof(struct tcc_pm_fw_drvdata), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		tcc_pm_fw_dev_err(dev, "allocate driver data", ret);
		goto pre_init_error;
	}

	/* Initialize `/sys/power/str/` sysfs kobject */
	ret = tcc_pm_fw_power_sysfs_init(&data->pwrstr);
	if (ret != 0) {
		tcc_pm_fw_dev_err(dev, "init `/sys/power/str` sysfs", ret);
		goto power_sysfs_init_error;
	}

	/* Initialize access to boot-reason register field */
	data->boot_reason = tcc_pm_fw_boot_reason_init(dev, boot_reason_pr);
	if (IS_ERR(data->boot_reason)) {
		ret = PTR_ERR(data->boot_reason);
		tcc_pm_fw_dev_err(dev, "init access to boot reason", ret);
		goto boot_reason_init_error;
	}

	/* Initialize AP-MC mailbox channel to get ACC_ON/OFF events */
	data->mbox_chan = tcc_pm_fw_mbox_init(dev, mbox_name);
	if (IS_ERR(data->mbox_chan)) {
		ret = PTR_ERR(data->mbox_chan);
		tcc_pm_fw_dev_err(dev, "init AP-MC mailbox channel", ret);
		goto mbox_init_error;
	}

	/* Now we can register driver data */
	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	return 0;

	// Below line will never gonna happened, but keep in comments for
	// future usage:
	// tcc_pm_fw_mbox_free(dev, data->mbox_chan);
mbox_init_error:
	tcc_pm_fw_boot_reason_free(dev, data->boot_reason);
boot_reason_init_error:
	tcc_pm_fw_power_sysfs_free(&data->pwrstr);
power_sysfs_init_error:
	devm_kfree(dev, data);
pre_init_error:
	return ret;
}

int tcc_pm_fw_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tcc_pm_fw_drvdata *data = platform_get_drvdata(pdev);

	/*
	 * Unflag `application_ready`. This must be reflagged by
	 * an user-level process when it is ready after resumption.
	 */
	data->application_ready = false;

	return 0;
}

static const struct of_device_id tcc_pm_fw_match[] = {
	{ .compatible = "telechips,pm-fw" },
	{ /* sentinel */ }
};

static struct platform_driver tcc_pm_fw_driver = {
	.driver = {
		.name = TCC_PM_FW_DEV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_pm_fw_match),
	},

	.probe  = tcc_pm_fw_probe,
	.suspend = tcc_pm_fw_suspend,
};

builtin_platform_driver(tcc_pm_fw_driver);

MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips power management firmware driver");
MODULE_LICENSE("GPL v2");
