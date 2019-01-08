/*
	
	Copyright (C) 2006-2015 ILITEK TECHNOLOGY CORP.
	
	Description:	ILITEK based touchscreen  driver .
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, see the file COPYING, or write
	to the Free Software Foundation, Inc.,
	51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


	ilitek USB touch screen driver for linux base platform
*/
#include <linux/module.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/moduleparam.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/cdev.h>
#include "usbhid.h"
//#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/usb/input.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,8)
	#include <linux/input/mt.h>
#endif
#include <linux/hiddev.h>
#include <linux/fs.h>
#include "ilitek.h"

#define ILITEK_HID_DRIVER_NAME		"ilitek_hid"
#define ILITEK_FILE_DRIVER_NAME		"ilitek_hid_file"
#define ILITEK_DEBUG_LEVEL		KERN_ALERT
#define ILITEK_ERROR_LEVEL		KERN_ALERT

// i2c command for ilitek touch screen
#define ILITEK_TP_CMD_READ_DATA				0x10
#define ILITEK_TP_CMD_READ_SUB_DATA			0x11
#define ILITEK_TP_CMD_GET_RESOLUTION		0x20
#define ILITEK_TP_CMD_GET_KEY_INFORMATION	0x22
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION	0x40
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION	0x42
#define	ILITEK_TP_CMD_CALIBRATION			0xCC
#define	ILITEK_TP_CMD_CALIBRATION_STATUS	0xCD
#define ILITEK_TP_CMD_ERASE_BACKGROUND		0xCE

// define the application command
#define ILITEK_IOCTL_BASE                       100
#define ILITEK_IOCTL_I2C_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 0, unsigned char*)
#define ILITEK_IOCTL_I2C_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 2, unsigned char*)
#define ILITEK_IOCTL_I2C_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 3, int)
#define ILITEK_IOCTL_USB_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 4, unsigned char*)
#define ILITEK_IOCTL_USB_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 5, int)
#define ILITEK_IOCTL_USB_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 6, unsigned char*)
#define ILITEK_IOCTL_USB_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 7, int)
#define ILITEK_IOCTL_DRIVER_INFORMATION		    _IOWR(ILITEK_IOCTL_BASE, 8, int)
#define ILITEK_IOCTL_USB_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 9, int)
#define ILITEK_IOCTL_I2C_INT_FLAG	            _IOWR(ILITEK_IOCTL_BASE, 10, int)
#define ILITEK_IOCTL_I2C_UPDATE                 _IOWR(ILITEK_IOCTL_BASE, 11, int)
#define ILITEK_IOCTL_STOP_READ_DATA             _IOWR(ILITEK_IOCTL_BASE, 12, int)
#define ILITEK_IOCTL_START_READ_DATA            _IOWR(ILITEK_IOCTL_BASE, 13, int)
#define ILITEK_IOCTL_GET_INTERFANCE				_IOWR(ILITEK_IOCTL_BASE, 14, int)//default setting is usb interface
#define ILITEK_IOCTL_I2C_SWITCH_IRQ				_IOWR(ILITEK_IOCTL_BASE, 15, int)
#define ILITEK_IOCTL_UPDATE_FLAG				_IOWR(ILITEK_IOCTL_BASE, 16, int)
#define ILITEK_IOCTL_I2C_UPDATE_FW				_IOWR(ILITEK_IOCTL_BASE, 18, int)
#define ILITEK_IOCTL_USB_READ_PID				_IOWR(ILITEK_IOCTL_BASE, 19, int)
#define DBG(fmt, args...)   if (DBG_FLAG)printk("%s(%d): " fmt, __func__,__LINE__,  ## args)

//driver information
#define DERVER_VERSION_MAJOR 		3
#define DERVER_VERSION_MINOR 		8
#define RELEASE_VERSION				2

int driver_information[] = {DERVER_VERSION_MAJOR,DERVER_VERSION_MINOR,RELEASE_VERSION};

MODULE_AUTHOR("ilitek");
MODULE_DESCRIPTION("Linux driver for ilitek touch screen");
MODULE_LICENSE("GPL");

struct touch_data
{
	int x, y, z;
	int valid;
	int id;
};

struct hid_data 
{
	struct input_dev *input_dev;
	struct touch_data tp[10];
	int max_x;
	int max_y;
	int valid_input_register;
	int valid_hid_register;
	int support_hid_device;
	int support_touch_point;
	struct hid_device *hid_dev;
	volatile int disconnect;
	int bootloader_mode;
	unsigned int pid;
	unsigned char firmware_ver[4];
	unsigned char protocol_ver[4];
	int ch_x;
	int ch_y;
	struct hid_field *field;
	struct hid_report *report;
	int inputmode;
};
struct cmd_data 
{
	struct input_dev *input_dev;
	struct hid_device *hid_dev;
	struct hid_field *field;
};

struct dev_data 
{
	dev_t devno;
	struct cdev cdev;
	struct class *class;
	int remove_flag;
	int mode;
};

struct ilitek_data 
{
	struct hid_data hid;
	struct dev_data dev;
	struct cmd_data cmd;
};
static struct ilitek_data ilitek;

struct hiddev_list 
{
	struct hiddev_usage_ref buffer[2048];
	int head;
	int tail;
	unsigned flags;
	struct fasync_struct *fasync;
	struct hiddev *hiddev;
	struct list_head node;
	struct mutex thread_lock;
};

static void ilitek_input_report(struct input_dev*, struct touch_data*, int);
static int ilitek_register_input_device(struct input_dev**, int, int, int, char*);
static int ilitek_register_hid_device(struct hid_data*);
static int ilitek_init(void);
static void ilitek_exit(void);
static int ilitek_hid_set_report(unsigned char type, unsigned char id, void *buf, int size);
static int hid_input_mapping(struct hid_device*, struct hid_input*, struct hid_field*, struct hid_usage*, unsigned long**, int*);
static int hid_input_mapped(struct hid_device*, struct hid_input*, struct hid_field*, struct hid_usage*, unsigned long**bit, int*);
static int hid_event(struct hid_device*, struct hid_field*, struct hid_usage*, __s32);
static int hid_probe(struct hid_device*, const struct hid_device_id*);
static void hid_remove(struct hid_device*);
static int ilitek_file_open(struct inode*, struct file*);
static ssize_t ilitek_file_write(struct file*, const char*, size_t, loff_t*);
static int ilitek_file_close(struct inode*, struct file*);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,8)
static void hid_feature_mapping(struct hid_device *hdev,struct hid_field *field, struct hid_usage *usage);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
    static int ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
struct file_operations ilitek_hid_fops =
{
	.write = ilitek_file_write,
	.open = ilitek_file_open,
	.release = ilitek_file_close,
	.unlocked_ioctl = ilitek_file_ioctl,
};

static char DBG_FLAG;

static const struct hid_device_id hid_devices[] = 
{
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, USB_DEVICE_ID_ILITEK_MT) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x0006) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x0010) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x0001) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x001C) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x001F) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x0015) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x0041) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ILITEK, 0x0094) },
	{ }
};
MODULE_DEVICE_TABLE(hid, hid_devices);

static const struct hid_usage_id hid_id[] = 
{
	{ HID_ANY_ID, HID_ANY_ID, HID_ANY_ID },
	{ HID_ANY_ID - 1, HID_ANY_ID - 1, HID_ANY_ID - 1}
};

static struct hid_driver hid_driver = 
{
	.name = ILITEK_HID_DRIVER_NAME,
	.id_table = hid_devices,
	.probe = hid_probe,
	.remove = hid_remove,
	.input_mapping = hid_input_mapping,
	.input_mapped = hid_input_mapped,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,8)
	.feature_mapping = hid_feature_mapping,
#endif
	.usage_table = hid_id,
	.event = hid_event,
};

static int
ilitek_hid_set_report(unsigned char type, unsigned char id, void *buf, int size)
{
	return usb_control_msg(hid_to_usb_dev(ilitek.hid.hid_dev),
		usb_sndctrlpipe(hid_to_usb_dev(ilitek.hid.hid_dev), 0),
		0x09, USB_TYPE_CLASS | USB_RECIP_INTERFACE, (type << 8) + id,
		0, buf, size, HZ);
}

static int ilitek_file_open(struct inode *inode, struct file *filp)
{
	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
	return 0; 
}

static ssize_t ilitek_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	uint8_t buffer[128] = {0};
	int i;
	if(count > 128){
		return -1;
	}
	
	memcpy(buffer, buf, count - 1);
	if(strcmp(buffer, "calibrate") == 0)
	{
		if(ilitek.hid.support_hid_device)
		{
			if(ilitek.hid.valid_hid_register)
			{
				if(ilitek.hid.hid_dev)
				{
					unsigned char buf[128] = {0};
					buf[0] = 0x03;
					buf[1] = ILITEK_TP_CMD_CALIBRATION;
					buf[2] = 0x01;
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					else{
						printk("%s, usb calibrate, success\n", __func__);
					}
				}
			}
		}
	}else if(strcmp(buffer, "protocol") == 0)
	{
		if(ilitek.hid.support_hid_device)
		{
			if(ilitek.hid.valid_hid_register)
			{
				if(ilitek.hid.hid_dev)
				{
					unsigned char buf[128] = {0};
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x02;
					buf[3] = 0x00;
					buf[4] = 0xF2;
					buf[5] = 0x01;
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x01;
					buf[3] = 0x03;
					buf[4] = 0x42;
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					else{
						msleep(10);
						printk("%s, Protocol:%d.%d.%d\n", __func__,ilitek.hid.field->value[4],ilitek.hid.field->value[5],ilitek.hid.field->value[6]);
					}
					for(i = 0; i<64; i++)
					{
						printk("value[%d]=0x%x,",i,ilitek.hid.field->value[i]);
					}

					printk("\n");
				}
			}
		}	
	}else if(strcmp(buffer, "bl") == 0)
	{
		if(ilitek.hid.support_hid_device)
		{
			if(ilitek.hid.valid_hid_register)
			{
				if(ilitek.hid.hid_dev)
				{
					unsigned char buf[128] = {0};
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x03;
					buf[3] = 0x00;
					buf[4] = 0xC4;
					buf[5] = 0x5A;
					buf[6] = 0xA5;
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x01;
					buf[3] = 0x00;
					buf[4] = 0xC2;
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x01;
					buf[3] = 0x00;
					buf[4] = 0xC0;
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					else{
						msleep(10);
						printk("%s, return = 0x%02X\n", __func__,ilitek.hid.field->value[3]);
					}
				}
			}
		}
	}else if(strcmp(buffer, "ap") == 0)
	{
		if(ilitek.hid.support_hid_device)
		{
			if(ilitek.hid.valid_hid_register)
			{
				if(ilitek.hid.hid_dev){
					unsigned char buf[128] = {0};
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x03;
					buf[3] = 0x00;
					buf[4] = 0xC4;
					buf[5] = 0x5A;
					buf[6] = 0xA5;
					//ilitek.hid.hid_dev->hid_output_raw_report(ilitek.hid.hid_dev, buf, 7, HID_FEATURE_REPORT);
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x01;
					buf[3] = 0x00;
					buf[4] = 0xC1;
					//ilitek.hid.hid_dev->hid_output_raw_report(ilitek.hid.hid_dev, buf, 5, HID_FEATURE_REPORT);
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					buf[0] = 0x03;
					buf[1] = 0xa3;
					buf[2] = 0x01;
					buf[3] = 0x01;
					buf[4] = 0xC0;
					//ilitek.hid.hid_dev->hid_output_raw_report(ilitek.hid.hid_dev, buf, 5, HID_FEATURE_REPORT);
					if(ilitek_hid_set_report(0x02, 0x03, buf, 0x40) < 0)
					{
						printk("%s, usb calibrate, failed\n", __func__);
					}
					else{
						msleep(10);
					}
				}
			}
		}
	}

	else if(strcmp(buffer, "dbg") == 0)
	{
		DBG_FLAG =! DBG_FLAG;
		printk("%s, %s message(%X).\n",__func__,DBG_FLAG?"Enabled":"Disabled",DBG_FLAG);
	}
	else
	{
		printk("%s, invalid command\n", __func__);
	}
	return -1;
}
/*
description
        ioctl function for character device driver
prarmeters
	inode
		file node
        filp
            file pointer
        cmd
            command
        arg
            arguments
return
        status
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	static unsigned char buffer[128] = {0};
	static int write_len = 0,read_len = 0, i;
	int ret;
	char *buf = kmalloc(64, GFP_KERNEL);
	memset(buf, 0, 64);
	buf[0] = 0x03;
	buf[1] = 0xA3;
	// parsing ioctl command
	switch(cmd){
		case ILITEK_IOCTL_USB_WRITE_DATA:
		//printk("---ILITEK_IOCTL_USB_WRITE_DATA,write_len=%d----\n",write_len);
		
			ret = copy_from_user(buffer, (unsigned char*)arg, write_len + 2);
			for(i = 2; i < buffer[0]+4; i++)
			{
				buf[i] = buffer[i-2];
			}
			ilitek_hid_set_report(0x02, 0x03, buf, 0x40);
			kfree(buf);
			break;
		case ILITEK_IOCTL_USB_READ_DATA:
			for(i = 0; i < read_len; i++)
			{
				buffer[i] = ilitek.hid.field->value[i+3];
				//printk("value[%d]=0x%x\n,",i,buffer[i]);
			}
			ret = copy_to_user((unsigned char*)arg, buffer, read_len);
			kfree(buf);
			if(ret < 0){
				printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
				return -1;
			}
			break;
		case ILITEK_IOCTL_USB_WRITE_LENGTH:
			write_len = arg;
			break;
		case ILITEK_IOCTL_USB_READ_LENGTH:
			read_len = arg;
			break;
		case ILITEK_IOCTL_USB_READ_PID:
			read_len = 1;
			buffer[0] = ilitek.hid.hid_dev->product;
			printk("PID=%x,buffer=%x\n",ilitek.hid.hid_dev->product,buffer[0]);
			buffer[0] = ilitek.hid.hid_dev->product;
			ret = copy_to_user((unsigned char*)arg, buffer, read_len);
			break;
		case ILITEK_IOCTL_DRIVER_INFORMATION:
			for(i = 0; i < 3; i++)
			{
				buffer[i] = driver_information[i];
			}
			ret = copy_to_user((unsigned char*)arg, buffer, 7);
			break;
		default:
			return -1;
	}
    	return 0;
}

static int 
ilitek_file_close(struct inode *inode, struct file *filp)
{
	printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
	return 0;
}

static int ilitek_register_input_device(struct input_dev **input, int support_touch_point, int max_x, int max_y, char* name)
{
	if(support_touch_point == 1){
		ilitek.hid.input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		ilitek.hid.input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
		#ifdef ROTATE_FLAG_90
		input_set_abs_params(ilitek.hid.input_dev, ABS_X, 0, max_y, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_Y, 0, max_x, 0, 0);
		printk(ILITEK_DEBUG_LEVEL "%s, single-touch, max_x %d, max_y %d\n", __func__, max_y, max_x);
		#elif defined ROTATE_FLAG_270
		input_set_abs_params(ilitek.hid.input_dev, ABS_X, 0, max_y, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_Y, 0, max_x, 0, 0);
		printk(ILITEK_DEBUG_LEVEL "%s, single-touch, max_x %d, max_y %d\n", __func__, max_y, max_x);
		#else
		input_set_abs_params(ilitek.hid.input_dev, ABS_X, 0, max_x, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_Y, 0, max_y, 0, 0);
		printk(ILITEK_DEBUG_LEVEL "%s, single-touch, max_x %d, max_y %d\n", __func__, max_x, max_y);
		#endif
		input_set_abs_params(ilitek.hid.input_dev, ABS_PRESSURE, 0, 255, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_TOOL_WIDTH, 0, 1, 0, 0);
	}
	else{
		set_bit(EV_ABS, ilitek.hid.input_dev->evbit);
		#ifdef ROTATE_FLAG_90
		printk("ROTATE_FLAG_90");
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_POSITION_X, 0, max_y, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_POSITION_Y, 0, max_x, 0, 0);
		printk(ILITEK_DEBUG_LEVEL "%s, multi-touch, max_x %d, max_y %d\n", __func__, max_y, max_x);
		#elif defined ROTATE_FLAG_270
		printk("ROTATE_FLAG_270");
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_POSITION_X, 0, max_y, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_POSITION_Y, 0, max_x, 0, 0);
		printk(ILITEK_DEBUG_LEVEL "%s, multi-touch, max_x %d, max_y %d\n", __func__, max_y, max_x);
		#else
		printk("ROTATE_FLAG_0||ROTATE_FLAG_180");
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
		printk(ILITEK_DEBUG_LEVEL "%s, multi-touch, max_x %d, max_y %d\n", __func__, max_x, max_y);
		#endif
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_WIDTH_MAJOR, 0, 1, 0, 0);
		#ifndef TOUCH_PROTOCOL_B
		input_set_abs_params(ilitek.hid.input_dev, ABS_MT_TRACKING_ID, 0, support_touch_point, 0, 0);
		#endif
	}
	return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,8)
static void hid_feature_mapping(struct hid_device *hdev,struct hid_field *field, struct hid_usage *usage)
{
	switch (usage->hid) 
	{
		case HID_DG_INPUTMODE:
			break;
		case HID_DG_CONTACTMAX:
			#ifndef SINGLE_TOUCH
			ilitek.hid.support_touch_point = field->value[0];
			//20131216 add by tigers
			if(ilitek.hid.support_touch_point == 0){
				ilitek.hid.support_touch_point = field->logical_maximum;
			}
			//20131216 end by tigers
			#endif
			break;
	}
}
#endif
static int hid_input_mapping(struct hid_device *hdev, struct hid_input *hi, struct hid_field *field, struct hid_usage *usage, unsigned long **bit, int *max)
{
	if(usage->hid == 0xff000001){
		ilitek.cmd.field = field;
		ilitek.cmd.hid_dev = hdev;
	}
//	/* Only map fields from TouchScreen or TouchPad collections.
//   * We need to ignore fields that belong to other collections
//     such as Mouse that might have the same GenericDesktop usages. */
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,8)
 if (field->application == HID_DG_TOUCHSCREEN)
	{
		set_bit(INPUT_PROP_DIRECT, hi->input->propbit);
	}
	else
		return 0;
#endif
	
	//ilitek.hid.input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	//ilitek.hid.input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	//set_bit(EV_ABS, ilitek.hid.input_dev->evbit);
 //   printk("%s,hdev type:%d,max=%d\n", __func__, hdev->type, *max);
	switch(usage->hid & HID_USAGE_PAGE)
	{
		case HID_UP_GENDESK:
			switch(usage->hid)
			{
				case HID_GD_X:
					ilitek.hid.input_dev = field->hidinput->input;
					ilitek.hid.field = field;
					ilitek.hid.hid_dev = hdev;
					//printk("X ilitek.hid.support_touch_point =%d\n",ilitek.hid.support_touch_point);
					if(ilitek.hid.support_touch_point == 1)
					{
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_X);
						input_set_abs_params(hi->input, ABS_X, field->logical_minimum, field->logical_maximum, 0, 0);
					}
					else{
						#ifdef ROTATE_FLAG_90
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_Y);
						#elif defined ROTATE_FLAG_270
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_Y);
						#else
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_X);
						#endif
						input_set_abs_params(hi->input, ABS_MT_POSITION_X, field->logical_minimum, field->logical_maximum, 0, 0);
					}
					ilitek.hid.max_x = field->logical_maximum;
					return 1;
				case HID_GD_Y:
					//printk("Y ilitek.hid.support_touch_point =%d\n",ilitek.hid.support_touch_point);
					if(ilitek.hid.support_touch_point == 1){
					#ifdef ROTATE_FLAG_90
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_X);
					#elif defined ROTATE_FLAG_270
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_X);
					#else
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_Y);
					#endif
						input_set_abs_params(hi->input, ABS_Y, field->logical_minimum, field->logical_maximum, 0, 0);
					}
					else{
					#ifdef ROTATE_FLAG_90
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_X);
					#elif defined ROTATE_FLAG_270
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_X);
					#else
						hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_POSITION_Y);
					#endif
						input_set_abs_params(hi->input, ABS_MT_POSITION_Y, field->logical_minimum, field->logical_maximum, 0, 0);
					}
					ilitek.hid.max_y = field->logical_maximum;

					if(ilitek.hid.valid_input_register == 0)
					{
						int ret = ilitek_register_input_device(&(hdev), ilitek.hid.support_touch_point,
						ilitek.hid.max_x, ilitek.hid.max_y, ILITEK_HID_DRIVER_NAME);
						if(ret)
						{
							printk(ILITEK_ERROR_LEVEL "%s, input register device, error\n", __func__);
						}
						else{
							ilitek.hid.valid_input_register = 1;
							printk(ILITEK_ERROR_LEVEL "%s, input register device, success\n", __func__);
						}
					}
					return 1;
			}
			return 0;
		case HID_UP_DIGITIZER:
			switch(usage->hid)
			{
				case HID_DG_TIPSWITCH:
					hid_map_usage(hi, usage, bit, max, EV_KEY, BTN_TOUCH);
					return 1;
				case HID_DG_INRANGE:
				case HID_DG_CONFIDENCE:
				case HID_DG_CONTACTCOUNT:
				case HID_DG_CONTACTMAX:
					return -1;
				case HID_DG_CONTACTID:
				#ifdef TOUCH_PROTOCOL_B		
					#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
					input_mt_init_slots(hi->input,ilitek.hid.support_touch_point,INPUT_MT_DIRECT);
					#else
					input_mt_init_slots(hi->input,ilitek.hid.support_touch_point);
					#endif
				#endif
					return -1;
				case HID_DG_TIPPRESSURE:
					hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_PRESSURE);
					return 1;
				case 0xff000000:
					printk("0xff000000\n");
					/* we do not want to map these: no input-oriented meaning */
					return -1;
			}
			//return 0;
	}
	return -1;
}

static int 
hid_input_mapped(struct hid_device *hdev, struct hid_input *hi, struct hid_field *field, struct hid_usage *usage, unsigned long **bit, int *max)
{
	if(usage->type == EV_KEY || usage->type == EV_ABS)
	{
		clear_bit(usage->code, *bit);
	}
	return 0;
}

static int hid_event(struct hid_device *hid, struct hid_field *field, struct hid_usage *usage, __s32 value)
{
	static int index = 0;
	if(hid->claimed & HID_CLAIMED_INPUT)
	{
		switch (usage->hid) 
		{
			case HID_DG_INRANGE:
				break;
			case HID_DG_CONFIDENCE:
				//ilitek.hid.tp[index].valid = !!value;
				break;
			case HID_DG_TIPSWITCH:
				//printk("id=%d,HID_DG_TIPSWITCH:%d\n",ilitek.hid.tp[index].id,value);
				ilitek.hid.tp[index].valid = value;
				break;
			case HID_DG_TIPPRESSURE:
				//printk("HID_DG_TIPPRESSURE=%d\n",value);
				ilitek.hid.tp[index].z = value;
				break;
			case HID_DG_CONTACTID:
				//printk("HID_DG_CONTACTID:%d\n",value);
				//index = value;
				ilitek.hid.tp[index].id = value;
				break;
			case HID_GD_X:
			#ifdef ROTATE_FLAG_90 
				ilitek.hid.tp[index].y = value;
			#elif defined ROTATE_FLAG_180
				ilitek.hid.tp[index].x = ilitek.hid.max_x - value;
			#elif defined ROTATE_FLAG_270
				ilitek.hid.tp[index].y = ilitek.hid.max_x - value;
			#else
				ilitek.hid.tp[index].x = value;
			#endif
				//printk("x[%d]=%d",index,value);
				break;
			case HID_GD_Y:
			#ifdef ROTATE_FLAG_90 
				ilitek.hid.tp[index].x = ilitek.hid.max_y - value;
			#elif defined ROTATE_FLAG_180
				ilitek.hid.tp[index].y = ilitek.hid.max_y - value;
			#elif defined ROTATE_FLAG_270
				ilitek.hid.tp[index].x = value;
			#else
				ilitek.hid.tp[index].y = value;
			#endif
				//printk("y[%d]=%d",index,value);
				index+= 1;
				break;
			case HID_DG_CONTACTCOUNT:
			//printk("HID_DG_CONTACTCOUNT=%d\n",value);
				ilitek_input_report(ilitek.hid.input_dev, ilitek.hid.tp, value);
				index = 0;
				break;
			default:
				return 0;
		}
	}

	if(hid->claimed & HID_CLAIMED_HIDDEV && hid->hiddev_hid_event)
	{
		hid->hiddev_hid_event(hid, field, usage, value);
	}
	return 1;
}

static int hid_probe(struct hid_device *hdev, const struct hid_device_id *dev_id)
{
	int ret, i;
	struct hid_report *report;
	struct hid_report *r;
	struct hid_report_enum *re;
	re = &(hdev->report_enum[HID_FEATURE_REPORT]);
	r = re->report_id_hash[ilitek.hid.inputmode];
	if (r) {
		r->field[0]->value[0] = 0x02;
		usbhid_submit_report(hdev, r, USB_DIR_OUT);
	}
	ret = hid_parse(hdev);
	if(ret)
	{
		printk(ILITEK_ERROR_LEVEL "%s, hid parse error, ret %d\n", __func__, ret);
		return ret;
	}
	
	ret = hid_hw_start(hdev,  HID_CONNECT_DEFAULT);
	//ret = hid_hw_start(hdev,  HID_CONNECT_HIDINPUT | HID_CONNECT_HIDRAW);
	if(ret)
	{
		printk(ILITEK_ERROR_LEVEL "%s, hid start error, ret %d\n", __func__, ret);
		return ret;
	}

	report = hdev->report_enum[HID_FEATURE_REPORT].report_id_hash[5];
	ilitek.hid.report = report;
	if(report)
	{
		//ilitek.hid.hid_dev = hdev;
		report->field[0]->value[0] = 2;
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
		hid_hw_request(hdev, report, HID_REQ_SET_REPORT);
		#else
		usbhid_submit_report(hdev, report, USB_DIR_OUT);
		report = hdev->report_enum[HID_INPUT_REPORT].report_id_hash[4];
		if(report){
			printk("HID_INPUT_REPORT pass\n");
			usbhid_submit_report(hdev, report, USB_DIR_IN);
		}
		else
			printk("HID_INPUT_REPORT fail\n");
		#endif
	}
	else{
		printk("HID_INPUT_REPORT report fail\n");
	}
	// read driver version
	printk( "%s, Driver Version:%d.%d.%d\n", __func__, driver_information[0], driver_information[1], driver_information[2]);

	if(!ilitek.dev.remove_flag){
		ret = alloc_chrdev_region(&ilitek.dev.devno, 0, 1, ILITEK_FILE_DRIVER_NAME);
		if(ret)
		{
			printk(ILITEK_ERROR_LEVEL "%s, can't alloc chrdev\n", __func__);
		}
		printk(ILITEK_DEBUG_LEVEL "%s, register chrdev(%d, %d)\n", __func__, MAJOR(ilitek.dev.devno), MINOR(ilitek.dev.devno));
		cdev_init(&ilitek.dev.cdev, &ilitek_hid_fops);
		ilitek.dev.cdev.owner = THIS_MODULE;
		ret = cdev_add(&ilitek.dev.cdev, ilitek.dev.devno, 1);
		if(ret < 0)
		{
			printk(ILITEK_ERROR_LEVEL "%s, add char devive error, ret %d\n", __func__, ret);
		}
	
		ilitek.dev.class = class_create(THIS_MODULE, ILITEK_FILE_DRIVER_NAME);
		if(IS_ERR(ilitek.dev.class))
		{
			printk(ILITEK_ERROR_LEVEL "%s, creating class error\n", __func__);
		}
	
		device_create(ilitek.dev.class, NULL, ilitek.dev.devno, NULL, "ilitek_ctrl_usb");
		//proc_create("ilitek_ctrl_usb", 0666, NULL, &ilitek_hid_fops);
	}

	ilitek.dev.remove_flag = 1;
	printk(ILITEK_DEBUG_LEVEL "%s,%d,ilitek.dev.remove_flag =%d\n", __func__,__LINE__,ilitek.dev.remove_flag);
	return ret;
}

static void hid_remove(struct hid_device *hdev)
{
	printk(ILITEK_DEBUG_LEVEL "%s, hid_remove entry ,ilitek.dev.remove_flag =%d\n", __func__,ilitek.dev.remove_flag);
	if(ilitek.dev.remove_flag){
		ilitek.hid.valid_input_register = 0;
		cdev_del(&ilitek.dev.cdev);
		unregister_chrdev_region(ilitek.dev.devno, 1);
		device_destroy(ilitek.dev.class, ilitek.dev.devno);
		class_destroy(ilitek.dev.class);
	}
	hid_hw_stop(hdev);
	ilitek.dev.remove_flag = 0;
}
#ifndef TOUCH_PROTOCOL_B
static void ilitek_input_report(struct input_dev *input_dev, struct touch_data *tp, int tpnum)
{
    int i, ispressed = 0;
    DBG("%s,enter,tpnum=%d\n", __func__, tpnum);
    #ifdef SINGLE_TOUCH
	{
        if(tp[0].valid) 
		{
            input_event(input_dev, EV_ABS, ABS_X, tp[0].x);
            input_event(input_dev, EV_ABS, ABS_Y, tp[0].y);
            input_event(input_dev, EV_KEY, BTN_TOUCH, 1);
			input_report_key(input_dev, BTN_LEFT, 1);
			DBG("touch down,x%d=%d,y%d=%d\n", i, tp[i].x, i, tp[i].y);
        }
        else 
		{
			DBG("touch up ,x%d=%d,y%d=%d\n", tp[i].id, tp[i].x, tp[i].id, tp[i].y);
            input_event(input_dev, EV_KEY, BTN_TOUCH, 0);
			input_report_key(input_dev, BTN_LEFT, 0);
        }
    }
    #else 
	{
        for(i = 0; i < tpnum; i++) 
		{
            if(tp[i].valid) 
			{
                input_event(input_dev, EV_ABS, ABS_MT_TRACKING_ID, tp[i].id);
                input_event(input_dev, EV_ABS, ABS_MT_POSITION_X, tp[i].x);
                input_event(input_dev, EV_ABS, ABS_MT_POSITION_Y, tp[i].y);
                input_event(input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
                //20131216 add by tigers
                ispressed = 1;
                //20131216 end by tigers
                input_mt_sync(input_dev);
                DBG("touch down,x%d=%d,y%d=%d\n", i, tp[i].x, i, tp[i].y);
            }
            //20131216 mark by tigers
            /* else {
            	input_event(input_dev, EV_ABS, ABS_MT_TRACKING_ID, tp[i].id);
            	DBG("touch up ,x%d=%d,y%d=%d\n",i,tp[i].x,i,tp[i].y);
            	release_count++;
            	if(release_count == tpnum){
            		input_event(input_dev, EV_KEY, BTN_TOUCH, 0);
            		DBG("touch release\n");
            	}
            	input_mt_sync(input_dev);
            }*/
            //20131216 end by tigers
        }
        //20131216 move to this from for loop by tigers
        input_event(input_dev, EV_KEY, BTN_TOUCH, ispressed);
        //20131216 end by tigers
    }
	#endif
    input_sync(input_dev);
}
#else
static void ilitek_input_report(struct input_dev *input_dev,struct touch_data *tp, int tpnum)
{
    int i;
    DBG("%s,enter,tpnum=%d\n", __func__, tpnum);
    if(ilitek.hid.support_touch_point == 1) 
	{
        if(tp[0].valid) 
		{
            input_event(input_dev, EV_ABS, ABS_X, tp[0].x);
            input_event(input_dev, EV_ABS, ABS_Y, tp[0].y);
            input_event(input_dev, EV_KEY, BTN_TOUCH, 1);
        }
        else 
		{
            input_event(input_dev, EV_KEY, BTN_TOUCH, 0);
        }
    }
    else {
        for(i = 0; i < tpnum; i++) 
		{
            input_mt_slot(input_dev, tp[i].id);
            if(tp[i].valid) 
			{
                input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
                input_event(input_dev, EV_ABS, ABS_MT_POSITION_X, tp[i].x);
                input_event(input_dev, EV_ABS, ABS_MT_POSITION_Y, tp[i].y);
                input_event(input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
                DBG("touch down,x%d=%d,y%d=%d\n", tp[i].id, tp[i].x, tp[i].id, tp[i].y);
            }
            else 
			{
                input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
                DBG("touch up ,x%d=%d,y%d=%d\n", tp[i].id, tp[i].x, tp[i].id, tp[i].y);
            }
        }
    }
    input_mt_report_pointer_emulation(input_dev, true);
    input_sync(input_dev);
}
#endif
static int ilitek_register_hid_device(struct hid_data *hid)
{
	int ret;

	if(hid == NULL)
	{
		printk(ILITEK_ERROR_LEVEL "%s, invalid parameter\n", __func__);
		return -1;
	}

	ret = hid_register_driver(&hid_driver);
	if(ret < 0)
	{
		printk(ILITEK_ERROR_LEVEL "%s, register hid driver error, ret %d\n", __func__, ret);
		return ret;
	}
	hid->valid_hid_register = 1;
	return ret;
}

static int ilitek_init(void)
{
	int ret = 0;
	printk(ILITEK_DEBUG_LEVEL "%s,%d,ilitek.dev.remove_flag =%d\n", __func__,__LINE__,ilitek.dev.remove_flag);
    memset(&ilitek, 0, sizeof(struct ilitek_data));
    ilitek.hid.support_hid_device = 1;
	#ifdef SINGLE_TOUCH 
		ilitek.hid.support_touch_point = 1;
	#else
		ilitek.hid.support_touch_point = 10;
	#endif
	if(ilitek.hid.support_hid_device)
	{
		ret = ilitek_register_hid_device(&ilitek.hid);
		if(ret < 0)
		{
			printk(ILITEK_ERROR_LEVEL "%s, register hid device, error\n", __func__);
		}
	}
	
	return 0;
}

static void ilitek_exit(void)
{
	printk(ILITEK_DEBUG_LEVEL "%s,%d,ilitek.dev.remove_flag =%d\n", __func__,__LINE__,ilitek.dev.remove_flag);
	if(ilitek.hid.support_hid_device)
	{
		if(ilitek.hid.valid_hid_register)
		{
			printk(ILITEK_DEBUG_LEVEL "%s, unregister hid driver\n", __func__);
			hid_unregister_driver(&hid_driver);
		}
		if(ilitek.hid.valid_input_register)
		{
			printk(ILITEK_DEBUG_LEVEL "%s, unregister hid input device\n", __func__);
		}
	}
	
	cdev_del(&ilitek.dev.cdev);
	unregister_chrdev_region(ilitek.dev.devno, 1);
	device_destroy(ilitek.dev.class, ilitek.dev.devno);
    class_destroy(ilitek.dev.class);
}

module_init(ilitek_init);
module_exit(ilitek_exit);

