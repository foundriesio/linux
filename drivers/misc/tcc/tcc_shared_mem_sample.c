// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/cdev.h>         //struct cdev
#include <linux/tcc_shmem.h>
#include <linux/tcc_shmem_sample.h>
#include <linux/uaccess.h>

#ifdef CONFIG_OF

DEFINE_SPINLOCK(shm_sample_spinlock);


struct shmem_sample_data {
	struct cdev tcc_shmem_sample_cdev;
	char *shmem_buf;
	int shmem_receive_num;
	int shmem_port;
};

static struct tcc_shm_callback tcc_shmem_sample_callback;

static struct class *tcc_shmem_sample_class;
static int tcc_shmem_sample_major;
dev_t tcc_shmem_sample_devt;

const static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = tcc_shmem_sample_open,
	.read = tcc_shmem_sample_read,
	.write = tcc_shmem_sample_write,
	.release = tcc_shmem_sample_release,
};

static const struct of_device_id tcc_shmem_sample_of_match[] = {
	{ .compatible = "tcc_shmem_sample", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_shmem_sample_of_match);

static int tcc_shmem_sample_open(struct inode *inode, struct file *file)
{
	struct shmem_sample_data *shdata;

	spin_lock_irq(&shm_sample_spinlock);

	shdata = container_of(inode->i_cdev, struct shmem_sample_data,
						    tcc_shmem_sample_cdev);
	file->private_data = shdata;

	spin_unlock_irq(&shm_sample_spinlock);

	return 0;

}

static int tcc_shmem_sample_release(struct inode *inode, struct file *file)
{
	return 0;

}

ssize_t tcc_shmem_sample_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *offp)
{
	struct shmem_sample_data *shdata;
	int ret_count = 0;

	spin_lock_irq(&shm_sample_spinlock);

	shdata = (struct shmem_sample_data *)file->private_data;

	if (count < shdata->shmem_receive_num)
		return -EINVAL;

	if(shdata->shmem_receive_num > 0) {
		//printk("\n%d\n", shdata->shmem_receive_num);
		if (copy_to_user(ubuf, shdata->shmem_buf,
				    shdata->shmem_receive_num) != 0) {
			pr_err("%s: copy_to_user fail\n", __func__);
			spin_unlock_irq(&shm_sample_spinlock);
			return -EFAULT;
		}

		memset(shdata->shmem_buf, 0, sizeof(char)*1024);
		ret_count = shdata->shmem_receive_num;
		shdata->shmem_receive_num = 0;
	}

	spin_unlock_irq(&shm_sample_spinlock);
	return ret_count;
}

ssize_t tcc_shmem_sample_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *offp)
{
	struct shmem_sample_data *shdata;
	int val = 0;
	int ret = 0;

	spin_lock_irq(&shm_sample_spinlock);

	shdata = (struct shmem_sample_data *)file->private_data;

	spin_unlock_irq(&shm_sample_spinlock);
	//pr_info("shmem port : %d\n", shdata->shmem_port);
	ret = tcc_shmem_transfer_port_nodev(shdata->shmem_port,
						    count, (char *)ubuf);

	if(ret < 0) {

		val = tcc_shmem_is_valid();

		if (val != 0) {
			shdata->shmem_port = tcc_shmem_find_port_by_name("tcc_shmem_sample");
			printk("shmem_port %d\n", shdata->shmem_port);

			if (!(shdata->shmem_port < 0)) {
				tcc_shmem_sample_callback.data = (unsigned long)shdata;
				tcc_shmem_sample_callback.callback_func =
							tcc_shmem_sample_rx;
				tcc_shmem_register_callback(shdata->shmem_port,
						    tcc_shmem_sample_callback);

			}
		} else
			pr_err("%s: tcc shared memory is not valid\n", __func__);

	}

	return count;
}

int32_t tcc_shmem_sample_rx(unsigned long data, char *received_data,
				uint32_t received_num)
{

	struct shmem_sample_data *shdata = (struct shmem_sample_data *)data;

	shdata->shmem_receive_num = received_num;

	if(shdata->shmem_receive_num < 1024) {
		memcpy(shdata->shmem_buf,
				    received_data, received_num);
	} else {
		shdata->shmem_receive_num = 0;
	}


	return 0;

}

static int tcc_shmem_sample_probe(struct platform_device *pdev)
{
	int val = 0;
	//struct device *dev = &pdev->dev;
	struct shmem_sample_data *shdata;
	struct device *shmem_device;

	shdata = devm_kzalloc(&pdev->dev, sizeof(struct shmem_sample_data),
							GFP_KERNEL);
	if(!shdata)
		return -ENOMEM;

	shdata->shmem_buf = devm_kzalloc(&pdev->dev,
		sizeof(char)*1024, GFP_KERNEL);

	if ((alloc_chrdev_region(&tcc_shmem_sample_devt, 0, 1,
				    "TCC_SHMEM_DEV")) < 0) {
		pr_err("Cannot allocate major number\n");
		return -1;
	}

	tcc_shmem_sample_major = MAJOR(tcc_shmem_sample_devt);

	cdev_init(&shdata->tcc_shmem_sample_cdev, &fops);

	if ((cdev_add(&shdata->tcc_shmem_sample_cdev, MKDEV(tcc_shmem_sample_major, 0),
					1)) < 0) {
		pr_err("Cannot add the device to the system\n");
		return -1;
	}

	tcc_shmem_sample_class = class_create(THIS_MODULE, "tcc_shmem_sample");
	if (IS_ERR(tcc_shmem_sample_class))
		return -1;

	shmem_device = device_create(tcc_shmem_sample_class, NULL,
		MKDEV(tcc_shmem_sample_major, 0), NULL, "tcc_shmem_sample");
        if (IS_ERR(shmem_device)) {
		pr_err("Can't create device\n");
		return -1;
        }

	val = tcc_shmem_is_valid();

	if (val != 0) {
		shdata->shmem_port = tcc_shmem_find_port_by_name("tcc_shmem_sample");
		printk("shmem_port %d\n", shdata->shmem_port);

		if (!(shdata->shmem_port < 0)) {
			tcc_shmem_sample_callback.data = (unsigned long)shdata;
			tcc_shmem_sample_callback.callback_func =
							tcc_shmem_sample_rx;
			tcc_shmem_register_callback(shdata->shmem_port,
						tcc_shmem_sample_callback);

		}
	} else
		pr_err("%s: tcc shared memory is not valid\n", __func__);

	platform_set_drvdata(pdev, shdata);

	pr_err("TCC SHMEM SAMPLE\n");

	return 0;

}

static int tcc_shmem_sample_remove(struct platform_device *pdev)
{
	return 0;
}



static struct platform_driver tcc_shmem_sample_device_driver = {
	.probe		= tcc_shmem_sample_probe,
	.remove		= tcc_shmem_sample_remove,
	.driver		= {
		.name	= "tcc-shmem-sample",
		.of_match_table = of_match_ptr(tcc_shmem_sample_of_match),
	}
};

static int __init tcc_shmem_sample_init(void)
{
	return platform_driver_register(&tcc_shmem_sample_device_driver);
}

static void __exit tcc_shmem_sample_exit(void)
{
	platform_driver_unregister(&tcc_shmem_sample_device_driver);
}

late_initcall(tcc_shmem_sample_init);
module_exit(tcc_shmem_sample_exit);

#endif

MODULE_AUTHOR("Telechips.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO sample driver");
MODULE_ALIAS("platform:tcc-shmem-sample");
