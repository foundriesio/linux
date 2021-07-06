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

static char shmem_buf[1024];
static int shmem_receive_num;
static int shmem_port;
static struct tcc_shm_callback tcc_shmem_sample_callback;

static struct class *tcc_shmem_sample_class;

static int tcc_shmem_sample_major;

dev_t tcc_shmem_sample_devt;
static struct cdev tcc_shmem_sample_cdev;

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

	return 0;

}

static int tcc_shmem_sample_release(struct inode *inode, struct file *file)
{
	return 0;

}

ssize_t tcc_shmem_sample_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *offp)
{
	int ret_count = 0;

	if (copy_to_user(ubuf, shmem_buf, shmem_receive_num) != 0) {
		pr_err("%s: copy_to_user fail\n", __func__);
		return -EFAULT;
	}

	memset(shmem_buf, 0, sizeof(char)*1024);
	ret_count = shmem_receive_num;
	shmem_receive_num = 0;


	return ret_count;
}

ssize_t tcc_shmem_sample_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *offp)
{
	int ret = 0;

	ret = tcc_shmem_transfer_port_nodev(shmem_port, count, (char *)ubuf);

	return count;
}

int32_t tcc_shmem_sample_rx(unsigned long data, char *received_data,
				uint32_t received_num)
{

	memcpy(shmem_buf+shmem_receive_num, received_data, received_num);
	shmem_receive_num += received_num;

	return 0;

}

static int tcc_shmem_sample_probe(struct platform_device *pdev)
{
	int val = 0, ret;
	struct device *dev = &pdev->dev;


	if ((alloc_chrdev_region(&tcc_shmem_sample_devt, 0, 1,
				    "TCC_SHMEM_DEV")) < 0) {
		pr_err("Cannot allocate major number\n");
		return -1;
	}

	tcc_shmem_sample_major = MAJOR(tcc_shmem_sample_devt);

	cdev_init(&tcc_shmem_sample_cdev, &fops);

	if ((cdev_add(&tcc_shmem_sample_cdev, MKDEV(tcc_shmem_sample_major, 0),
					1)) < 0) {
		pr_err("Cannot add the device to the system\n");
		return -1;
	}

	tcc_shmem_sample_class = class_create(THIS_MODULE, "tcc_shmem_sample");
	if (IS_ERR(tcc_shmem_sample_class))
		return -1;

	ret = device_create(tcc_shmem_sample_class, NULL,
		MKDEV(tcc_shmem_sample_major, 0), NULL, "tcc_shmem_sample");
        if (IS_ERR(ret)) {
		pr_err("Can't create device\n");
		return -1;
        }

	val = tcc_shmem_is_valid();

	if (val != 0) {
		shmem_port = tcc_shmem_find_port_by_name("tcc_shmem_sample");

		if (!(shmem_port < 0)) {
			tcc_shmem_sample_callback.data = (unsigned long)dev;
			tcc_shmem_sample_callback.callback_func =
							tcc_shmem_sample_rx;
			tcc_shmem_register_callback(shmem_port,
						tcc_shmem_sample_callback);

		}
	} else
		pr_err("%s: tcc shared memory is not valid\n", __func__);

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
