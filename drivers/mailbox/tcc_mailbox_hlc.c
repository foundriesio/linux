/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
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
#include <asm/io.h>
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

#include <soc/tcc/pmap.h>

#define MBOX_HLC_DEV_NAME        "mailbox-hlc"
#define MBOX_HLC_DEV_MINOR       0

#define DRV_STATUS_NO_INIT 0
#define DRV_STATUS_READY 1

#define CMD_TIMEOUT		        			msecs_to_jiffies(1000)

#define MBOX_HLC_DEBUG

#define LOG_TAG    "MBOX_HLC"

#ifdef MBOX_HLC_DEBUG    
#define dprintk(msg...)    printk(KERN_INFO LOG_TAG  msg);		
#else
#define dprintk(msg...)    do {} while (0) 			
#endif

#define eprintk(msg...)    printk(KERN_ERR LOG_TAG  msg);

struct mbox_hlc_device
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

#ifndef CONFIG_OF
static struct mbox_hlc_device *global_hlc_dev;

struct mbox_hlc_device *get_global_hlc_dev(void)
{
    return global_hlc_dev;
}
#endif

struct mbox_hlc_a7sinfo {
	unsigned int *boot_base;
	unsigned int *boot_back;
	unsigned int boot_size;
	
	unsigned int *dtb_base;
	unsigned int *dtb_back;
	unsigned int dtb_size;

	unsigned int *rootfs_base;
	unsigned int *rootfs_back;
	unsigned int rootfs_size;
};


/*****************************************************************************
 * APIs for send message
 *****************************************************************************/
static int tcc_mbox_hlc_send_message_to_channel(struct mbox_hlc_device *hlc_dev, struct tcc_mbox_data *mbox_data)
{
    int ret;

	ret = mbox_send_message(hlc_dev->mbox_ch, mbox_data);
	mbox_client_txdone(hlc_dev->mbox_ch,0);

	if (ret < 0) {
        eprintk("%s : Failed to send message via mailbox\n", __FUNCTION__);
	}

	return ret;
}

/*****************************************************************************
 * message sent notify (not used)
 ******************************************************************************/
static void tcc_mbox_hlc_message_sent(struct mbox_client *client, void *message, int r)
{
    struct mbox_client *cl;
	const char *name;
	cl = (struct mbox_client *)client;

	name = dev_name(cl->dev);

	if (r) {
		eprintk("%s : Message could not be sent: %d\n", __FUNCTION__, r);
	} else {
        dprintk("%s, Message sent!! from devt = %d, dev_name = %s\n", __FUNCTION__, cl->dev->devt ,name);
	}
}

/*****************************************************************************
 * process rx work
 *****************************************************************************/
/*****************************************************************************
 * message received callback
 ******************************************************************************/
void tcc_mbox_hlc_message_received(struct mbox_client *client, void *message)
{

	struct platform_device *pdev = to_platform_device(client->dev);
	struct mbox_hlc_device *hlc_dev = platform_get_drvdata(pdev);
	struct mbox_hlc_a7sinfo a7sinfo;
	struct tcc_mbox_data *mbox_data = (struct tcc_mbox_data *)message;
	struct tcc_mbox_data mbox_done;

	printk(KERN_INFO "start \n");
	a7sinfo.boot_size = mbox_data->data[2];
	a7sinfo.boot_base = ioremap_nocache(mbox_data->data[0], a7sinfo.boot_size);
	a7sinfo.boot_back = ioremap_nocache(mbox_data->data[1], a7sinfo.boot_size);

	memcpy(a7sinfo.boot_base, a7sinfo.boot_back, a7sinfo.boot_size);

	iounmap((void*)(a7sinfo.boot_base));
	iounmap((void*)(a7sinfo.boot_back));

	a7sinfo.dtb_size = mbox_data->data[6];
	a7sinfo.dtb_base = ioremap_nocache(mbox_data->data[4], a7sinfo.dtb_size);
	a7sinfo.dtb_back = ioremap_nocache(mbox_data->data[5], a7sinfo.dtb_size);

	memcpy(a7sinfo.dtb_base, a7sinfo.dtb_back, a7sinfo.dtb_size);

	iounmap((void*)(a7sinfo.dtb_base));
	iounmap((void*)(a7sinfo.dtb_back));

	a7sinfo.rootfs_size = mbox_data->data[10];
	a7sinfo.rootfs_base = ioremap_nocache(mbox_data->data[8], a7sinfo.rootfs_size);
	a7sinfo.rootfs_back = ioremap_nocache(mbox_data->data[9], a7sinfo.rootfs_size);

	memcpy(a7sinfo.rootfs_base, a7sinfo.rootfs_back, a7sinfo.rootfs_size);

	iounmap((void*)(a7sinfo.rootfs_base));
	iounmap((void*)(a7sinfo.rootfs_back));

	printk(KERN_INFO "DONE \n");

	mbox_done.cmd[0] = 0;
	mbox_done.cmd[1] = 2<<16;
	mbox_done.cmd[2] = 0;
	mbox_done.cmd[3] = 0;
	mbox_done.cmd[4] = 0;
	mbox_done.cmd[5] = 0;
	mbox_done.cmd[6] = 0;

	mbox_done.data_len = 0;

	tcc_mbox_hlc_send_message_to_channel(hlc_dev, &mbox_done);

#if 0
	unsigned int index;
	pmap_t pmap_a7s;
	pmap_t pmap_a7s_backup;
	pmap_get_info("a7s_avm", &pmap_a7s);
	pmap_get_info("a7s_avm_backup", &pmap_a7s_backup);

	printk(KERN_INFO " a7so_start: %x, a7so_size : %x , a7ab_start : %x, a7sb_size :%x\n", 
			pmap_a7s.base, pmap_a7s.size, pmap_a7s_backup.base, pmap_a7s_backup.size);


	base_addr = ioremap_nocache(pmap_a7s.base, 0x6400000);
	printk(KERN_INFO "virt addr : %x \n", *base_addr);
	iounmap((void*)base_addr);
	
	printk(KERN_INFO "mbox cmd: %x\n", mbox_data->cmd[0]);
	printk(KERN_INFO "data_len: %x\n", mbox_data->data_len);

	for(index =0; index<mbox_data->data_len; index++)
		printk(KERN_INFO "data %x\n", mbox_data->data[index]);
#endif
#if 0
	struct platform_device *pdev = to_platform_device(client->dev);
	struct mbox_hlc_device *hlc_dev = platform_get_drvdata(pdev);
	struct tcc_mbox_data *mbox_data = (struct tcc_mbox_data *)message;
	int rx_queue_handle;

	int i;

	unsigned short cmd_type;
	struct arm_smccc_res res;
#endif
}

/*****************************************************************************
 * register mbox client, channel and received callback
 ******************************************************************************/
static struct mbox_chan *hlc_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->tx_block = false;
	client->rx_callback = tcc_mbox_hlc_message_received;
	client->tx_done = tcc_mbox_hlc_message_sent;
	client->knows_txdone = false;
	client->tx_tout = 500;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		eprintk("%s : Failed to request %s channel\n", __FUNCTION__, name);
		return NULL;
	}

	return channel;
}

struct file_operations tcc_mbox_hlc_fops = {
    .owner = THIS_MODULE,
    .open = NULL, 
    .read = NULL,
    .write = NULL, 
    .release = NULL,
    .unlocked_ioctl = NULL, 
    .poll = NULL,
};


/*****************************************************************************

 * Module Init/Exit
 ******************************************************************************/
static int mbox_hlc_probe(struct platform_device *pdev) {
	
	int result = 0;
	struct mbox_hlc_device *hlc_dev = NULL;


	
	hlc_dev = devm_kzalloc(&pdev->dev, sizeof(struct mbox_hlc_device), GFP_KERNEL);
	if(!hlc_dev) {
		eprintk("%s : Cannot alloc hlc device..\n", __FUNCTION__);
		return -ENOMEM;
	}

    hlc_dev->dev = &pdev->dev;
	
	platform_set_drvdata(pdev, hlc_dev);

    //get device / mbox name
	of_property_read_string(pdev->dev.of_node,"device-name", &hlc_dev->dev_name);
	of_property_read_string(pdev->dev.of_node,"mbox-names", &hlc_dev->mbox_name);

    //create and register character device
	result = alloc_chrdev_region(&hlc_dev->dev_num, MBOX_HLC_DEV_MINOR, 1, hlc_dev->dev_name);
	if (result) {
		eprintk("%s : alloc_chrdev_region error %d\n", __FUNCTION__, result);
		return result;
	}

    cdev_init(&hlc_dev->c_dev, &tcc_mbox_hlc_fops);
	hlc_dev->c_dev.owner = THIS_MODULE;

	result = cdev_add(&hlc_dev->c_dev, hlc_dev->dev_num, 1);
	if (result) {
		eprintk("%s : cdev_add error %d\n",  __FUNCTION__, result);
		goto cdev_add_error;
	}	

	hlc_dev->class = class_create(THIS_MODULE, hlc_dev->dev_name);
	if (IS_ERR(hlc_dev->class)) {
		result = PTR_ERR(hlc_dev->class);
		eprintk("%s : class_create error %d\n", __FUNCTION__, result);
		goto class_create_error;
	}

    //create device node
	hlc_dev->dev = device_create(hlc_dev->class, &pdev->dev, hlc_dev->dev_num, NULL, hlc_dev->dev_name);
	if (IS_ERR(hlc_dev->dev)) {
		result = PTR_ERR(hlc_dev->dev);
		eprintk("%s : device_create error %d\n", __FUNCTION__, result);
		goto device_create_error;
	}

	hlc_dev->mbox_ch = hlc_request_channel(pdev, hlc_dev->mbox_name);
	if (hlc_dev->mbox_ch == NULL) {
		result = -EPROBE_DEFER;
		eprintk("%s : hlc_request_channel error: %d\n", __FUNCTION__, result);
		goto mbox_request_channel_error;
	}

    hlc_dev->pdev = pdev;

    mutex_init(&hlc_dev->lock);


#ifndef CONFIG_OF	
	global_hlc_dev = hlc_dev;
    dprintk("%s : global_hlc_dev dev_name:%s, mbox_name:%s, dev_num:%d\n", 
			__FUNCTION__, global_hlc_dev->dev_name, global_hlc_dev->mbox_name, global_hlc_dev->dev_num);
#endif

	dev_info(hlc_dev->dev, "device register done\n");
	return 0;

mbox_request_channel_error:
	device_destroy(hlc_dev->class, hlc_dev->dev_num);

device_create_error:
	class_destroy(hlc_dev->class);

class_create_error:
	cdev_del(&hlc_dev->c_dev);

cdev_add_error:
	unregister_chrdev_region(hlc_dev->dev_num, 1);

	return result;
}

static int mbox_hlc_remove(struct platform_device * pdev)
{
	struct mbox_hlc_device *hlc_dev = platform_get_drvdata(pdev);

    //free mbox channel
    if(hlc_dev->mbox_ch != NULL) {
		mbox_free_channel(hlc_dev->mbox_ch);
		hlc_dev->mbox_ch= NULL;
	}

    mutex_destroy(&hlc_dev->lock);

    //unregister device driver
	device_destroy(hlc_dev->class, hlc_dev->dev_num);
	class_destroy(hlc_dev->class);
	cdev_del(&hlc_dev->c_dev);
	unregister_chrdev_region(hlc_dev->dev_num, 1);
	
	return 0;
}

static const struct of_device_id mbox_hlc_match[] = {
    { .compatible = "telechips,hlc-mbox" },
    {},
};
MODULE_DEVICE_TABLE(of, mbox_hlc_match);

static struct platform_driver mbox_hlc_driver = {
    .probe  = mbox_hlc_probe,
    .remove = mbox_hlc_remove,

    .driver = {
        .name = "mailbox-hlc",
        .of_match_table = of_match_ptr(mbox_hlc_match),
    },
};

module_platform_driver(mbox_hlc_driver);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips HLC MailBox");
MODULE_LICENSE("GPL v2");
