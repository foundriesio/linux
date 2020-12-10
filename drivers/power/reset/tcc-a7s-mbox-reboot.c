// SPDX-License-Identifier: GPL-2.0
/*
 * Poweroff driver for Telechips SoCs
 *
 * Copyright (C) 2016-2018 Telechips Inc.
 */

#include <linux/io.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <asm/system_misc.h>
#include <asm/system_info.h>

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpufreq.h>
#include <linux/err.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/poll.h>


#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/kdev_t.h>

#include <soc/tcc/pmap.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/cdev.h>
#include <asm/atomic.h>


#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/psci.h>
#include <linux/arm-smccc.h>
#include <linux/suspend.h>


struct mbox_a7s_reboot_device
{
	struct platform_device  *pdev;
	struct device *dev;
	struct cdev c_dev;
	struct class *class;
	dev_t dev_num;
	const char *dev_name;
	const char *mbox_name;
	struct mbox_chan *mbox_ch;
	struct mutex lock;
};

static struct mbox_a7s_reboot_device *reboot_dev_gd;

struct mbox_a7s_reboot_device *get_gd_reboot_dev(void)
{
	return reboot_dev_gd;
}

static struct mbox_chan *reboot_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(struct mbox_client), GFP_KERNEL);

	if(!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->tx_block = false;
	client->rx_callback = NULL;
	client->tx_done = NULL;
	client->knows_txdone = false;
	client->tx_tout = 500;

	channel = mbox_request_channel_byname(client, name);
	if(IS_ERR(channel)){
		printk(KERN_ERR "[A7S_MBOX_REBOOT]" "%s: Failed to request %s Channel\n", __func__, name);
		return NULL;
	}

	return channel;
}

static void reset_cpu(enum reboot_mode mode, const char* cmd)
{
	struct tcc_mbox_data reboot_msg;
	struct mbox_a7s_reboot_device *reboot_dev = get_gd_reboot_dev();

	reboot_msg.cmd[0] = 0x0;
	reboot_msg.cmd[1] = 0x1<<16;
	reboot_msg.cmd[2] = 0x0;
	reboot_msg.cmd[3] = 0x0;
	reboot_msg.cmd[4] = 0x0;
	reboot_msg.cmd[5] = 0x0;
	reboot_msg.cmd[6] = 0x0;

	reboot_msg.data_len = 0;

	mutex_lock(&reboot_dev->lock);
	mbox_send_message(reboot_dev->mbox_ch, &reboot_msg);
	mbox_client_txdone(reboot_dev->mbox_ch,0);
	mutex_unlock(&reboot_dev->lock);

	do {
		wfe();
	} while(1);
}

static void do_tcc_poweroff(void)
{

	do {
		wfi();
	} while(1);
}

struct file_operations tcc_a7s_mbox_fops = {
    .owner = THIS_MODULE,
    .open = NULL, 
    .read = NULL,
    .write = NULL,
    .release = NULL,
    .unlocked_ioctl = NULL, 
    .poll =  NULL,
};


static int tcc_restart_probe(struct platform_device *pdev)
{

	int ret = 0;

	struct mbox_a7s_reboot_device * reboot_dev = NULL;

	reboot_dev = devm_kzalloc(&pdev->dev, sizeof(struct mbox_a7s_reboot_device), GFP_KERNEL);
	if(!reboot_dev){
		printk(KERN_ERR "[A7S_MBOX_REBOOT]" "%s : Cannot alloc Reboot Device \n", __func__);
		return -ENOMEM;
	}

	reboot_dev->dev = &pdev->dev;

	platform_set_drvdata(pdev, reboot_dev);

	of_property_read_string(pdev->dev.of_node, "device-name", &reboot_dev->dev_name);
	of_property_read_string(pdev->dev.of_node, "mbox-names", &reboot_dev->mbox_name);

	printk(KERN_INFO "[A7S_REBOOT] " "device name : %s \n", reboot_dev->dev_name);
	printk(KERN_INFO "[A7S_REBOOT] " "mbox name : %s \n", reboot_dev->mbox_name);

	ret = alloc_chrdev_region(&reboot_dev->dev_num, 0, 1, reboot_dev->dev_name);
	if(ret){
		printk(KERN_ERR "A7S_MBOX_REBOOT" "%s : Cannot Create Char Deivce (%d)\n", __func__, ret);
		return ret;
	}

	cdev_init(&reboot_dev->c_dev, &tcc_a7s_mbox_fops);
	reboot_dev->c_dev.owner = THIS_MODULE;

	ret = cdev_add(&reboot_dev->c_dev, reboot_dev->dev_num, 1);
	if(ret){
		printk(KERN_ERR "A7S_MBOX_REBOOT" "%s : Char Deivce Add Fail (%d) \n", __func__, ret);
		goto cdev_add_error;
	}
	
	reboot_dev->class = class_create(THIS_MODULE, reboot_dev->dev_name);
	if(IS_ERR(reboot_dev->class)){
		ret = PTR_ERR(reboot_dev->class);
		printk(KERN_ERR "A7S_MBOX_REBOOT" "%s : Class Device Create Fail (%d)\n", __func__, ret);
		goto class_create_error;
	}

	reboot_dev->dev = device_create(reboot_dev->class, &pdev->dev, reboot_dev->dev_num, NULL, reboot_dev->dev_name);
	if(IS_ERR(reboot_dev->dev)) {
		ret = PTR_ERR(reboot_dev->dev);
		printk(KERN_ERR "A7S_MBOX_REBOOT" "%s : device create error (%d)\n", __func__, ret);
		goto device_create_error;
	}

	reboot_dev->mbox_ch = reboot_request_channel(pdev, reboot_dev->mbox_name);
	if(reboot_dev->mbox_ch == NULL){
		ret = -EPROBE_DEFER;
		printk(KERN_ERR "A7S_MBOX_REBOOT" "%s : request mbox reboot channel error : (%d)\n", __func__, ret);
		goto mbox_reboot_request_channel_error;
	}

	reboot_dev->pdev = pdev;
	mutex_init(&reboot_dev->lock);

	reboot_dev_gd = reboot_dev;

	pm_power_off = do_tcc_poweroff;

	arm_pm_restart = reset_cpu;

	printk(KERN_INFO "A7S_MBOX_REBOOT" "Device Register Complete\n");

	return 0;

mbox_reboot_request_channel_error:
	device_destroy(reboot_dev->class, reboot_dev->dev_num);

device_create_error:
	class_destroy(reboot_dev->class);

class_create_error:
	cdev_del(&reboot_dev->c_dev);

cdev_add_error:
	unregister_chrdev_region(reboot_dev->dev_num, 1);

	return ret;
}

static const struct of_device_id of_tcc_restart_match[] = {
	{ .compatible = "telechips,tcc_a7s_mbox_reboot", },
	{},
};
MODULE_DEVICE_TABLE(of, of_tcc_restart_match);

static struct platform_driver tcc_restart_driver = {
	.probe = tcc_restart_probe,
	.driver = {
		.name = "a7s_mbox_reboot",
		.of_match_table = of_match_ptr(of_tcc_restart_match),
	},
};

static int __init tcc_restart_init(void)
{
	return platform_driver_register(&tcc_restart_driver);
}
device_initcall(tcc_restart_init);
