/****************************************************************************
 * Copyright (C) 2020 Telechips Inc.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/arm-smccc.h>
#include <linux/proc_fs.h>

//#include <linux/ioctl.h>

#include <linux/uaccess.h> //copy_to_*
#include <linux/slab.h> // kmalloc

#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define START_MP_INDEX			1

static int start_mp = 0;
const unsigned int *property_temp=0;
unsigned int smc_time_val=0;
unsigned int smc_time_num=0;

static void __iomem *timer_tc32mcnt_addr = NULL;

#if defined(CONFIG_TCC803X_CA7S)
module_param(start_mp, int, 0644);
MODULE_PARM_DESC(int,"time info. for START_MP from cmdline");
#endif

extern unsigned int basic_setup_done_time;

dev_t boot_time_devt;
static struct class *boot_time_dev_class;
static struct cdev boot_time_cdev;


static int boot_time_open(struct inode *inode, struct file *file);
static int boot_time_release(struct inode *inode, struct file *file);
static long boot_time_ioctl(struct file *flip, unsigned int cmd, unsigned long arg);


static struct file_operations fops =
{
	.owner          = THIS_MODULE,
	.open           = boot_time_open,
	.unlocked_ioctl = boot_time_ioctl,
	.release        = boot_time_release,
};

static int boot_time_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Driver Open Function Called...!!!\n");
	return 0;
}


static int boot_time_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Driver Release Function Called...!!!\n");
	return 0;
}

const char *boot_stamp_desc [] = {
	"start MICOM BL1",
	"start MP",
	"start MICOM FW",
	"start ARM-TF",
	"start storage-boot",
	"complete load SP image",
	"complete initialize storage device",
	"complete load/verify key-table",
	"complete load/verify arm-tf bl31 header",
	"complete load/verify arm-tf bl31 image",
	"complete load/verify bl3 header",
	"complete load dram parameters",
	"complete initialize dram",
	"complete load/verify secure firmware",
	"complete load/verify bl3 image",
	"start bl3",
	"complete board_init_f",
	"complete board_init_r",
	"jump to ca53",
	"kernel decompression image",
	"kernel_startup",
	"init thread",
	"initcalls",
	"execute init process",
};

struct boot_time{

	unsigned int boot_stamp_length;
	unsigned int time_in_us;
	unsigned int current_time_val;
	unsigned int basic_setup_done_time;
	unsigned int boot_stamp_num; // from user
	unsigned int boot_stamp_desc_num;
	unsigned int boot_stamp_time_num;
	unsigned char *boot_stamp;

};

#define BOOT_TIME_IOC_MAGIC 'b'

#define BOOT_STAMP_DESCRIPTION          _IOWR(BOOT_TIME_IOC_MAGIC, 1, struct boot_time)
#define BOOT_STAMP_DESCRIPTION_ALL      _IOWR(BOOT_TIME_IOC_MAGIC, 2, struct boot_time)
#define BOOT_TIME                       _IOWR(BOOT_TIME_IOC_MAGIC, 3, struct boot_time)
#define GET_BOOT_STAMP_NUM              _IOR(BOOT_TIME_IOC_MAGIC, 4, struct boot_time)
#define GET_CURRENT_TIME_VAL            _IOR(BOOT_TIME_IOC_MAGIC, 5, struct boot_time)
#define GET_INITCALL_DONE               _IOR(BOOT_TIME_IOC_MAGIC, 6, struct boot_time)


static long boot_time_ioctl(struct file *flip, unsigned int cmd, unsigned long arg){

	int ret = -ENOTTY;
	struct boot_time boot_time_data;
	struct arm_smccc_res res;

	switch(cmd){
		case BOOT_STAMP_DESCRIPTION:
		case BOOT_STAMP_DESCRIPTION_ALL:
			copy_from_user(&boot_time_data, (struct boot_time *)arg, sizeof(struct boot_time));
			copy_to_user((unsigned char *)((struct boot_time *)arg)->boot_stamp, boot_stamp_desc[boot_time_data.boot_stamp_num], 100);
			break;

		case BOOT_TIME:
			copy_from_user(&boot_time_data, (struct boot_time *)arg, sizeof(struct boot_time));
			arm_smccc_smc(smc_time_val, (unsigned long)boot_time_data.boot_stamp_num, 0, 0, 0, 0, 0, 0, &res);
			boot_time_data.time_in_us=(uint32_t)res.a1;
			copy_to_user((struct boot_time *)arg, &boot_time_data, sizeof(struct boot_time));
			break;

		case GET_BOOT_STAMP_NUM:
			arm_smccc_smc(smc_time_num, 0, 0, 0, 0, 0, 0, 0, &res);

			boot_time_data.boot_stamp_desc_num = sizeof(boot_stamp_desc)/4;
			boot_time_data.boot_stamp_time_num = res.a0;

			copy_to_user((struct boot_time *)arg, &boot_time_data, sizeof(struct boot_time));

			break;

		case GET_CURRENT_TIME_VAL:
                        boot_time_data.current_time_val=readl(timer_tc32mcnt_addr);
#if defined(CONFIG_TCC803X_CA7S)
                        boot_time_data.current_time_val+=start_mp;
#else
			arm_smccc_smc(smc_time_val, (unsigned long)START_MP_INDEX, 0, 0, 0, 0, 0, 0, &res);
			boot_time_data.current_time_val+=(uint32_t)res.a1;
#endif

			copy_to_user((struct boot_time *)arg, &boot_time_data, sizeof(struct boot_time));
			break;

		case GET_INITCALL_DONE:
			boot_time_data.basic_setup_done_time=basic_setup_done_time;
#if defined(CONFIG_TCC803x_CA7S)
			boot_time_data.basic_setup_done_time+=start_mp;
#else
                        arm_smccc_smc(smc_time_val, (unsigned long)START_MP_INDEX, 0, 0, 0, 0, 0, 0, &res);
                        boot_time_data.basic_setup_done_time+=(uint32_t)res.a1;
#endif


			copy_to_user((struct boot_time *)arg, &boot_time_data, sizeof(struct boot_time));
			break;

		default :
			printk("default boot time ioctl\n");

	}

	return ret;
}

#if 0
static ssize_t boot_time_show(struct device *dev, struct device_attribute *attr, char *buf){


	//struct gpio_sample_platform_data *pdata = dev_get_drvdata(dev);
	struct arm_smccc_res res;
	int cnt=0, i;

	arm_smccc_smc(0x82007005, 0, 0, 0, 0, 0, 0, 0, &res);

	cnt = res.a0;

	printk("num : %d\n", cnt);

	for(i=0; i<cnt ; i++){
		arm_smccc_smc(0x82007004, (unsigned long)i, 0, 0, 0, 0, 0, 0, &res);
		printk("%s : %d\n", boot_stamp_desc[i], (uint32_t)res.a1);;
		//printk("%d\n", (uint32_t)res.a1);
	}

	return sprintf(buf,"boot time\n");

}

static DEVICE_ATTR(boot_time, S_IRUGO|S_IWUSR, boot_time_show, NULL);

static struct attribute *boot_time_attrs[] = {

	&dev_attr_boot_time.attr,
	NULL,

};

static const struct attribute_group boot_time_attr_group = {

	.attrs = boot_time_attrs,

};
#endif

static int boot_time_probe(struct platform_device *pdev){

	struct device *dev = &pdev->dev;
	int error=0;

#if 0
	error = sysfs_create_group(&pdev->dev.kobj, &boot_time_attr_group);
#endif

	timer_tc32mcnt_addr = of_iomap(pdev->dev.of_node, 0);
	property_temp = of_get_property(pdev->dev.of_node, "smc-time-val", NULL);
	smc_time_val = be32_to_cpup(property_temp);
	property_temp = of_get_property(pdev->dev.of_node, "smc-time-num", NULL);
	smc_time_num = be32_to_cpup(property_temp);

	if((alloc_chrdev_region(&boot_time_devt, 0, 1, "Boot_Time_Dev")) <0){
		printk(KERN_INFO "Cannot allocate major number\n");
		error=1;
	}

	printk(KERN_INFO "Major = %d Minor = %d \n",MAJOR(boot_time_devt), MINOR(boot_time_devt));

	cdev_init(&boot_time_cdev,&fops);

	if((cdev_add(&boot_time_cdev,boot_time_devt,1)) < 0){
		printk(KERN_INFO "Cannot add the device to the system\n");
		goto r_class;
	}

	if((boot_time_dev_class = class_create(THIS_MODULE,"Boot_Time_class")) == NULL){
		printk(KERN_INFO "Cannot create the struct class\n");
		goto r_class;
	}

	if((device_create(boot_time_dev_class,NULL,boot_time_devt,NULL,"boot_time_device")) == NULL){
		printk(KERN_INFO "Cannot create the Device 1\n");
		goto r_device;
	}
	printk(KERN_INFO "Device Driver Insert...Done!!!\n");
	return 0;


	if(error){
		dev_err(dev, "boot time probe failed\n");
		return -1;
	}


r_device:
	class_destroy(boot_time_dev_class);
r_class:
	unregister_chrdev_region(boot_time_devt,1);
	return -1;


}


static const struct of_device_id boot_time_match_table[] = {
	{ .compatible = "telechips,boot_time" },
	{ }
};
MODULE_DEVICE_TABLE(of, boot_time_match_table);

static struct platform_driver boot_time_driver = {
	.probe		= boot_time_probe,
	.driver		= {
		.name	= "tcc-boot_time",
		.of_match_table = boot_time_match_table,
	},
};


static int __init boot_time_init(void)
{

	return platform_driver_register(&boot_time_driver);

}

static void __exit boot_time_exit(void)
{

	device_destroy(boot_time_dev_class,boot_time_devt);
	class_destroy(boot_time_dev_class);
	cdev_del(&boot_time_cdev);
	unregister_chrdev_region(boot_time_devt, 1);

	platform_driver_unregister(&boot_time_driver);
}

arch_initcall(boot_time_init);
module_exit(boot_time_exit);

MODULE_DESCRIPTION("Telechips Boot Time driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:boot_time");
MODULE_AUTHOR("Jaeyoung Park <dwayne.park@telechips.com>");


