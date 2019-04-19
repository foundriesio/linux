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
#include <linux/psci.h>
#include <linux/arm-smccc.h>
#include <linux/suspend.h>

#define MBOX_SUSPEND_DEV_NAME        "mailbox-suspend"
#define MBOX_SUSPEND_DEV_MINOR       0

#define DRV_STATUS_NO_INIT 0
#define DRV_STATUS_READY 1

#define CMD_TIMEOUT		        			msecs_to_jiffies(1000)

#define AUDIO_MBOX_FOR_A53					0
#define AUDIO_MBOX_FOR_A7S					1

//#define MBOX_SUSPEND_DEBUG

#define LOG_TAG    "MBOX_SUSPEND"

#ifdef MBOX_SUSPEND_DEBUG    
#define dprintk(msg...)    printk(LOG_TAG  msg);		
#else
#define dprintk(msg...)    do {} while (0) 			
#endif

#define eprintk(msg...)    printk(KERN_ERR LOG_TAG  msg);

struct mbox_suspend_device
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
static struct mbox_suspend_device *global_suspend_dev;

struct mbox_suspend_device *get_global_suspend_dev(void)
{
    return global_suspend_dev;
}
#endif

static struct device *android_device;

/*****************************************************************************
 * APIs for send message
 *****************************************************************************/
static int tcc_mbox_suspend_send_message_to_channel(struct mbox_suspend_device *suspend_dev, struct tcc_mbox_data *mbox_data)
{
    int ret;

	ret = mbox_send_message(suspend_dev->mbox_ch, mbox_data);
	mbox_client_txdone(suspend_dev->mbox_ch,0);

	if (ret < 0) {
        eprintk("%s : Failed to send message via mailbox\n", __FUNCTION__);
	}

	return ret;
}

/*****************************************************************************
 * message sent notify (not used)
 ******************************************************************************/
static void tcc_mbox_suspend_message_sent(struct mbox_client *client, void *message, int r)
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


extern int pm_suspend(suspend_state_t state);
/*****************************************************************************
 * process rx work
 *****************************************************************************/
/*****************************************************************************
 * message received callback
 ******************************************************************************/
static void tcc_mbox_suspend_message_received(struct mbox_client *client, void *message)
{
	struct platform_device *pdev = to_platform_device(client->dev);
	struct mbox_suspend_device *suspend_dev = platform_get_drvdata(pdev);
	struct tcc_mbox_data *mbox_data = (struct tcc_mbox_data *)message;
	int rx_queue_handle;
	char *suspend_enter[2]    = { "SUSPEND_STATE=ENTER", NULL };

	int i;

	unsigned short cmd_type;
	struct arm_smccc_res res;

	printk("%s : suspend received start++\n", __FUNCTION__);

	kobject_uevent_env(&suspend_dev->dev->kobj,
			KOBJ_CHANGE, suspend_enter);

	//pm_suspend(3);
	//arm_smccc_smc(0x84000001, 0, 0, 0, 0, 0, 0, 0, &res);
	dprintk("%s : received end--\n", __FUNCTION__);


}

/*****************************************************************************
 * register mbox client, channel and received callback
 ******************************************************************************/
static struct mbox_chan *suspend_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->tx_block = false;
	client->rx_callback = tcc_mbox_suspend_message_received;
	client->tx_done = tcc_mbox_suspend_message_sent;
	client->knows_txdone = false;
	client->tx_tout = 500;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		eprintk("%s : Failed to request %s channel\n", __FUNCTION__, name);
		return NULL;
	}

	return channel;
}

/*****************************************************************************
 * for user app
 ******************************************************************************/

static unsigned int tcc_mbox_suspend_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct mbox_suspend_device *suspend_dev = (struct mbox_suspend_device *)filp->private_data;

    if (suspend_dev == NULL) {
        eprintk("%s : Cannot get suspend mbox device..\n", __FUNCTION__);
        return 0;
    }

    return 0;
}

static int tcc_mbox_suspend_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct mbox_suspend_device *suspend_dev = (struct mbox_suspend_device *)filp->private_data;

    const unsigned int byte_size_per_mbox_data = MBOX_CMD_FIFO_SIZE * 4;
    unsigned int mbox_data_num = 0;
    unsigned int copy_byte_size = 0;

    // error handling
    if (count < byte_size_per_mbox_data || count % byte_size_per_mbox_data != 0) {
        eprintk("%s : Buffer size should be MBOX_CMD_FIFO_SIZE * 4 times..\n", __FUNCTION__);
        return -EINVAL;
    }
    if (suspend_dev == NULL) {
        eprintk("%s : Cannot get suspend mbox device..\n", __FUNCTION__);
        return -ENODEV;
    }

    dprintk("%s : copy_byte_size = %d, mbox_data_num = %d\n", __FUNCTION__, copy_byte_size, mbox_data_num);

    return copy_byte_size;

}

static int tcc_mbox_suspend_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static int tcc_mbox_suspend_open(struct inode * inode, struct file * filp)
{
    struct mbox_suspend_device *suspend_dev = container_of(inode->i_cdev, struct mbox_suspend_device, c_dev);

	if (suspend_dev == NULL) {
		eprintk("%s : Cannot get suspend mbox device..\n", __FUNCTION__);
		return -ENODEV;
	}


	filp->private_data = suspend_dev;

	return 0;
}

static int tcc_mbox_suspend_release(struct inode * inode, struct file * filp) 
{
    struct mbox_suspend_device *suspend_dev = container_of(inode->i_cdev, struct mbox_suspend_device, c_dev);

	if (suspend_dev == NULL) {
		eprintk("%s : Cannot get suspend device..\n", __FUNCTION__);
		return -ENODEV;
	}

	return 0;
}

static int tcc_mbox_suspend_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
    struct mbox_suspend_device *suspend_dev = (struct mbox_suspend_device *)filp->private_data;
	struct tcc_mbox_data mbox_data;

	struct tcc_mbox_data __user *argp = (struct tcc_mbox_data __user *)arg;


	long ret = 0;

    // error handling
    if (suspend_dev == NULL) {
		eprintk("%s : Cannot get suspend mbox device..\n", __FUNCTION__);
		return -ENODEV;
	}

	ret = copy_from_user(&mbox_data, argp, sizeof(struct tcc_mbox_data));
	if(ret) {
		eprintk("%s: unable to copy user paramters(%ld) \n", __FUNCTION__, ret);
		goto err_ioctl;
	}

	

err_ioctl:
	return ret;

}


struct file_operations tcc_mbox_suspend_fops = {
    .owner = THIS_MODULE,
    .open = tcc_mbox_suspend_open,
    .read = tcc_mbox_suspend_read,
    .write = tcc_mbox_suspend_write,
    .release = tcc_mbox_suspend_release,
    .unlocked_ioctl = tcc_mbox_suspend_ioctl,
    .poll = tcc_mbox_suspend_poll,
};

/*****************************************************************************
 * Module Init/Exit
 ******************************************************************************/
static int mbox_suspend_probe(struct platform_device *pdev) {
	int result = 0;

	struct mbox_suspend_device *suspend_dev = NULL;

	char rx_name[20];
	int i;

	suspend_dev = devm_kzalloc(&pdev->dev, sizeof(struct mbox_suspend_device), GFP_KERNEL);
	if(!suspend_dev) {
		eprintk("%s : Cannot alloc suspend device..\n", __FUNCTION__);
		return -ENOMEM;
	}

    suspend_dev->dev = &pdev->dev;
	
	platform_set_drvdata(pdev, suspend_dev);


    //get device / mbox name
	of_property_read_string(pdev->dev.of_node,"device-name", &suspend_dev->dev_name);
	of_property_read_string(pdev->dev.of_node,"mbox-names", &suspend_dev->mbox_name);

    //create and register character device
	result = alloc_chrdev_region(&suspend_dev->dev_num, MBOX_SUSPEND_DEV_MINOR, 1, suspend_dev->dev_name);
	if (result) {
		eprintk("%s : alloc_chrdev_region error %d\n", __FUNCTION__, result);
		return result;
	}

    cdev_init(&suspend_dev->c_dev, &tcc_mbox_suspend_fops);
	suspend_dev->c_dev.owner = THIS_MODULE;

	result = cdev_add(&suspend_dev->c_dev, suspend_dev->dev_num, 1);
	if (result) {
		eprintk("%s : cdev_add error %d\n",  __FUNCTION__, result);
		goto cdev_add_error;
	}	

	suspend_dev->class = class_create(THIS_MODULE, suspend_dev->dev_name);
	if (IS_ERR(suspend_dev->class)) {
		result = PTR_ERR(suspend_dev->class);
		eprintk("%s : class_create error %d\n", __FUNCTION__, result);
		goto class_create_error;
	}

    //create device node
	suspend_dev->dev = device_create(suspend_dev->class, &pdev->dev, suspend_dev->dev_num, NULL, suspend_dev->dev_name);
	if (IS_ERR(suspend_dev->dev)) {
		result = PTR_ERR(suspend_dev->dev);
		eprintk("%s : device_create error %d\n", __FUNCTION__, result);
		goto device_create_error;
	}

	suspend_dev->mbox_ch = suspend_request_channel(pdev, suspend_dev->mbox_name);
	if (suspend_dev->mbox_ch == NULL) {
		result = -EPROBE_DEFER;
		eprintk("%s : suspend_request_channel error: %d\n", __FUNCTION__, result);
		goto mbox_request_channel_error;
	}

    suspend_dev->pdev = pdev;

    mutex_init(&suspend_dev->lock);




    /*for (i = 0; i <= MBOX_AUDIO_CMD_TYPE_MAX; i++) {
		//suspend_dev->client[i].dev = NULL; //It seems do not need..
		suspend_dev->client[i].set_callback = NULL;
	    suspend_dev->client[i].is_used = 0;
    }*/

#ifndef CONFIG_OF	
	global_suspend_dev = suspend_dev;
    dprintk("%s : global_suspend_dev dev_name:%s, mbox_name:%s, dev_num:%d\n", __FUNCTION__, global_suspend_dev->dev_name, global_suspend_dev->mbox_name, global_suspend_dev->dev_num);
#endif

	dprintk("%s : Successfully registered!!!\n", __FUNCTION__);

	return 0;

mbox_init_tx_queue_error:
mbox_init_rx_queue_error:
mbox_request_channel_error:
	device_destroy(suspend_dev->class, suspend_dev->dev_num);

device_create_error:
	class_destroy(suspend_dev->class);

class_create_error:
	cdev_del(&suspend_dev->c_dev);

cdev_add_error:
	unregister_chrdev_region(suspend_dev->dev_num, 1);

	return result;
}

static int mbox_suspend_remove(struct platform_device * pdev)
{
	struct mbox_suspend_device *suspend_dev = platform_get_drvdata(pdev);

    //free mbox channel
    if(suspend_dev->mbox_ch != NULL) {
		mbox_free_channel(suspend_dev->mbox_ch);
		suspend_dev->mbox_ch= NULL;
	}

    mutex_destroy(&suspend_dev->lock);

    //unregister device driver
	device_destroy(suspend_dev->class, suspend_dev->dev_num);
	class_destroy(suspend_dev->class);
	cdev_del(&suspend_dev->c_dev);
	unregister_chrdev_region(suspend_dev->dev_num, 1);
	
	return 0;
}

static int tcc_mailbox_suspend(struct device *dev)
{
	printk("%s %d mailbox suspend enter \n",__func__,__LINE__);
	return 0;
}

static int tcc_mailbox_resume(struct device *dev)
{
	//struct platform_device *pdev = to_platform_device(client->dev);
	struct mbox_suspend_device *suspend_dev = platform_get_drvdata(dev);
	char *suspend_wake[2]    = { "SUSPEND_STATE=WAKE", NULL };

	printk("%s %d mailbox suspend resume \n",__func__,__LINE__);

	kobject_uevent_env(&suspend_dev->dev->kobj,
			KOBJ_CHANGE, suspend_wake);

	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id mbox_suspend_match[] = {
    { .compatible = "telechips,mailbox-suspend" },
    {},
};
MODULE_DEVICE_TABLE(of, mbox_suspend_match);
#endif

static struct platform_driver mbox_suspend_driver = {
    .driver = {
        .name = MBOX_SUSPEND_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = mbox_suspend_match,
#endif
    },
#ifdef CONFIG_OF
	.suspend	= tcc_mailbox_suspend,
	.resume		= tcc_mailbox_resume,
#endif
    .probe  = mbox_suspend_probe,
    .remove = mbox_suspend_remove,
};

module_platform_driver(mbox_suspend_driver);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips Suspend MailBox");
MODULE_LICENSE("GPL v2");
