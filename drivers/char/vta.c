/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/random.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/tee_drv.h>

#include <video/tcc/tcc_composite_ioctl.h>
#include <video/tcc/tcc_component_ioctl.h>


/* Debugging stuff */
#if 0
#define DBG(fmt, args...) \
	pr_info("\e[33m[%s:%d]\e[0m " fmt, __func__, __LINE__, ## args)
#else
#define DBG(fmt, args...)
#endif

/* TODO: REMOVE THIS TEST CODE */
//#define __TEST_CODE__

/* ver: 0xAAAABBBB (AAAA is VIOC-TA version, BBBB is vta driver version) */
#define VTA_VERSION		0x01050002

#define DEV_NAME		"vta"
#define IOCTL_VTA_CHECK	0x0A001000
#define IOCTL_VTA_CTX	0x0A001001

static dev_t vta_dev;
static struct cdev vta_cdev;
struct class *vta_class;

struct vta_data_t {
	struct platform_device *pdev;
	struct task_struct *vta_thread;
	unsigned long vta_time;
	struct tee_client_context *context;
	unsigned long cmd;
};
struct vta_data_t *vta_data;


/*
 * UUID (Don't modify)
 * 4e276916-62f7-42c6-bf78-65456409a8fa
 */
#define VTA_UUID { 0x4e276916, 0x62f7, 0x42c6, \
	{ 0xbf, 0x78, 0x65, 0x45, 0x64, 0x09, 0xa8, 0xfa } }

/*
 * Command ID
 * VTAx = 0x565441xx
 * xx is 0x00~0x7f - VTA Kernel driver only user this command.
 * xx is 0x80~0xff - Other TA or CA only use this command.
 */
#define VTA_CMD_IOCTL(x)          (0x56544100 | x)
#define VTA_CMD_TEST              VTA_CMD_IOCTL(0x00)
#define VTA_CMD_OBSERVE           VTA_CMD_IOCTL(0x01)
#define VTA_CMD_NOTIFY_END_VSYNC  VTA_CMD_IOCTL(0x02)
#define VTA_CMD_REQ_SET_OPC       VTA_CMD_IOCTL(0x80)
#define VTA_CMD_DRM_CONTENT_START VTA_CMD_IOCTL(0x81)
#define VTA_CMD_DRM_CONTENT_STOP  VTA_CMD_IOCTL(0x82)

/* TEE error code */
#define TEE_SUCCESS 0x00000000
#define TEE_ERROR_TARGET_DEAD 0xFFFF3024


static int vta_cmd_observe(struct vta_data_t *vta);

static unsigned long random_delay(void)
{
	unsigned long lessthan1000;

	lessthan1000 = get_random_int() % 1000;
	if (lessthan1000 < 500) {
		lessthan1000 += 500;
	}
	if (lessthan1000 > 900) {
		lessthan1000 -= 100;
	}

	return lessthan1000;
}

static int vta_thread_func(void *data)
{
	struct vta_data_t *vta = data;

	DBG("\n");

	while (true) {
		//printk("vta(%ld)+++\n", vta->vta_time);
		vta_cmd_observe(vta);
		//printk("vta---\n");

		vta->vta_time = random_delay();
		schedule_timeout_interruptible(msecs_to_jiffies(vta->vta_time));
	}

	return 0;
}

static int vta_get_context(struct vta_data_t *vta, int run)
{
	struct tee_client_uuid uuid = VTA_UUID;
	int ret = 0;

	DBG("\n");

	if (vta->context && vta->context->session_initalized) {
		return 0;
	}

	ret = tee_client_open_ta(&uuid, NULL, &vta->context);
	if (ret) {
		pr_err("%s: tee_client_open_ta failed(0x%x)\n", __func__, ret);
		ret = -ECONNREFUSED;
		goto err;
	}
	pr_info("\e[33mvta opened\e[0m\n");

	if (run) {
		vta->vta_thread = kthread_run(vta_thread_func, vta,
			"vta_thread");
		if (vta->vta_thread == ERR_PTR(-ENOMEM)) {
			pr_err("%s: vta_thread failed\n", __func__);
			ret = -ENOMEM;
		}
	}

err:
	return ret;
}

#define SUPPORT_HDMI		(1 << 0)
#define SUPPORT_COMPONENT	(1 << 1)
#define SUPPORT_COMPOSITE	(1 << 2)
#define SUPPORT_LCD0		(1 << 3)
static unsigned int vta_get_display_output(void)
{
	unsigned int output = 0;
	struct device_node *hdmi_np = NULL;
	#ifdef CONFIG_FB_TCC_COMPONENT
	struct device_node *component_np = NULL;
	#endif
	#ifdef CONFIG_FB_TCC_COMPOSITE
	struct device_node *composite_np = NULL;
	#endif

	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	hdmi_np = of_find_compatible_node(NULL, NULL, "telechips,dw-hdmi-tx");
	if (hdmi_np != NULL) {
		if (of_device_is_available(hdmi_np)) {
			output |= SUPPORT_HDMI;
		}
	}
	#endif

	#ifdef CONFIG_FB_TCC_COMPONENT
	component_np = of_find_compatible_node(NULL, NULL,
		"telechips,tcc-component");
	if (component_np != NULL) {
		if (of_device_is_available(component_np)) {
			output |= SUPPORT_COMPONENT;
		}
	}
	#endif

	#ifdef CONFIG_FB_TCC_COMPOSITE
	composite_np = of_find_compatible_node(NULL, NULL,
		"telechips,tcc-composite");
	if (composite_np != NULL) {
		if (of_device_is_available(composite_np)) {
			output |= SUPPORT_COMPOSITE;
		}
	}
	#endif

	DBG("0x%x\n", output);

	return output;
}

static __u32 vta_send_command(struct tee_client_context *context,
				struct tee_client_params *params,
				int command)
{
	__u32 tee_result;

	DBG("\n");

	if (!context || !context->session_initalized) {
		#if 0
		schedule_timeout_interruptible(msecs_to_jiffies(5000));
		vta_get_context(vta_data, 1);
		context = vta_data->context;
		#else
		return 0xFFFFFFFF;
		#endif
	}

	tee_result = tee_client_execute_command(context, params, command);

	if (tee_result != TEE_SUCCESS) {
		pr_err("vta: tee_client_execute_command failed(0x%08X)\n",
			tee_result);

		if (tee_result == TEE_ERROR_TARGET_DEAD) {
			vta_get_context(vta_data, 0);
			pr_err("\e[33mvta: vioc-ta was re-opened\e[0m\n");
		}
	}

	return tee_result;
}

static void vta_link_protection(struct tee_client_param *composite,
	struct tee_client_param *component)
{
	DBG("\n");

	if (composite->tee_client_value.a != 0xffffffff) {
		struct TCC_COMPOSITE_CGMS_TYPE cgms_cfg;

		cgms_cfg.data = composite->tee_client_value.b;
		cgms_cfg.odd_field_en = composite->tee_client_value.a;
		cgms_cfg.even_field_en = composite->tee_client_value.a;

		#if defined(CONFIG_FB_TCC_COMPOSITE)
		tcc_composite_set_cgms(&cgms_cfg);
		#endif

		DBG("composite cgms-a: %s\n",
			composite->tee_client_value.a ? "on" : "off");
	} else {
		DBG("composite cgms-a: Don't care\n");
	}

	#ifndef CONFIG_FB_TCC_COMPONENT_ADV7343
	if (component->tee_client_value.a != 0xffffffff) {
		struct TCC_COMPONENT_CGMS_TYPE cgms_cfg;

		cgms_cfg.data = component->tee_client_value.b;
		cgms_cfg.enable = component->tee_client_value.a;

		#if defined(CONFIG_FB_TCC_COMPONENT)
		component_set_cgms(&cgms_cfg);
		#endif

		DBG("component cgms-a %s\n",
			component->tee_client_value.a ? "on" : "off");
	} else {
		DBG("component cgms-a: Don't care\n");
	}
	#endif
}

/*
 * vta command functions
 */
static int vta_cmd_observe(struct vta_data_t *vta)
{
	int ret = 0;
	struct tee_client_params params;

	DBG("\n");

	if (!vta->context || !vta->context->session_initalized) {
		goto exit;
	}

	memset(&params, 0, sizeof(params));
	params.params[0].type = TEE_CLIENT_PARAM_VALUE_INOUT;
	params.params[1].type = TEE_CLIENT_PARAM_VALUE_INOUT;
	params.params[0].tee_client_value.a = 0xffffffff;
	params.params[1].tee_client_value.a = 0xffffffff;

	ret = vta_send_command(vta->context, &params, VTA_CMD_OBSERVE);
	if (ret) {
		pr_err("%s failed\n", __func__);
		goto exit;
	}

	DBG("params[0] (0x%lx, 0x%lx)\n",
		params.params[0].tee_client_value.a,
		params.params[0].tee_client_value.b);
	DBG("params[1] (0x%lx, 0x%lx)\n",
		params.params[1].tee_client_value.a,
		params.params[1].tee_client_value.b);

	vta_link_protection(&params.params[0], &params.params[1]);

exit:
	return ret;
}

int vta_cmd_notify_change_status(const char *func)
{
	int ret;

	//if (!vta_data->vta_time) {
	//	DBG("Not ready VIOC-TA\n");
	//	return 0;
	//}

	ret = vta_send_command(vta_data->context, NULL,
		VTA_CMD_NOTIFY_END_VSYNC);

	switch (ret) {
	case TEE_SUCCESS:
		DBG("called by %s\n", func);
		goto exit;
		//break;
	case 0xFFFFFFFF:
		DBG("vioc-ta context isn't exist\n");
		goto exit;
		//break;
	case TEE_ERROR_TARGET_DEAD:
		DBG("vioc-ta is dead\n");
		break;
	default:
		DBG("vioc-ta command failed\n");
		break;
	}

	pr_info("%s 0x%x\n", __func__, ret);

exit:
	return ret;
}
EXPORT_SYMBOL(vta_cmd_notify_change_status);

#if defined(__TEST_CODE__)
static int vta_test_cmd(struct vta_data_t *vta)
{
	int ret;
	struct tee_client_params params;

	DBG("\n");

	memset(&params, 0, sizeof(params));
	params.params[0].tee_client_value.a = 0x1234;
	params.params[0].type = TEE_CLIENT_PARAM_VALUE_INOUT;

	ret = vta_send_command(vta->context, &params, VTA_CMD_TEST);
	if (ret) {
		pr_err("%s failed\n", __func__);
	} else {
		pr_info("%s: 0x%lx -> TA -> 0x%lx\n", __func__,
			params.params[0].tee_client_value.a,
			params.params[0].tee_client_value.b);
	}

	return ret;
}

static int vta_test_req_set_opc(struct vta_data_t *vta)
{
	int ret;

	DBG("\n");

	ret = vta_send_command(vta->context, NULL, VTA_CMD_REQ_SET_OPC);
	if (ret) {
		pr_err("%s failed\n", __func__);
		goto exit;
	}

exit:
	return ret;
}

static int vta_test_drm_content_xxx(struct vta_data_t *vta, int start)
{
	int ret, cmd;

	DBG("\n");

	cmd = start ? VTA_CMD_DRM_CONTENT_START : VTA_CMD_DRM_CONTENT_STOP;
	ret = vta_send_command(vta->context, NULL, cmd);
	if (ret) {
		pr_err("%s(%s) failed\n", __func__, start ? "start" : "stop");
		goto exit;
	}

exit:
	return ret;
}

static void vta_test_link_protection(long onoff)
{
	struct tee_client_param composite, component;
	struct TCC_COMPOSITE_CGMS_TYPE composite_cgms;
	#ifndef CONFIG_FB_TCC_COMPONENT_ADV7343
	struct TCC_COMPONENT_CGMS_TYPE component_cgms;
	#endif

	/* key:0xc0 crc=0x3a (1 1 1 0 1 0) => cgms_data=0xe80c0 */
	composite.tee_client_value.a = onoff;	// enable/disable cgms-a
	composite.tee_client_value.b = 0xe80c0;	// cgms-a data
	component.tee_client_value.a = onoff;	// enable/disable cgms-a
	component.tee_client_value.b = 0xe80c0;	// cgms-a data

	vta_link_protection(&composite, &component);

	DBG("COMPOSITE CGMS-A onoff(%d) data(0x%x)\n",
		(int)composite.tee_client_value.a,
		(int)composite.tee_client_value.b);

	memset(&composite_cgms, 0, sizeof(composite_cgms));

	#if defined(CONFIG_FB_TCC_COMPOSITE)
	tcc_composite_get_cgms(&composite_cgms);
	#endif

	DBG("CGMS-A: [%d/%d], 0x%08x(A:0x%02x|B:0x%02x|C:0x%02x) status=%d\n",
		composite_cgms.odd_field_en,
		composite_cgms.even_field_en,
		composite_cgms.data,
		composite_cgms.data & 0x3F,
		(composite_cgms.data >> 6) & 0xFF,
		(composite_cgms.data >> 14) & 0x3F,
		composite_cgms.status);

	#ifndef CONFIG_FB_TCC_COMPONENT_ADV7343
	DBG("COMPONENT CGMS-A onoff(%d) data(0x%x)\n",
		(int)component.tee_client_value.a,
		(int)component.tee_client_value.b);
	memset(&component_cgms, 0, sizeof(component_cgms));

	#if defined(CONFIG_FB_TCC_COMPONENT)
	component_get_cgms(&component_cgms);
	#endif

	DBG("%s, 0x%05x\n",
		component_cgms.enable ? "on" : "off",
		component_cgms.data);
	#endif
}

/*
 * sysfs
 */
#define VTA_IOCTL_CMD_TEST                (0x00)
#define VTA_IOCTL_CMD_OBSERVE             (0x01)
#define VTA_IOCTL_CMD_LINK_PROTECTION_ON  (0x02)
#define VTA_IOCTL_CMD_LINK_PROTECTION_OFF (0x03)
#define VTA_IOCTL_CMD_NOTIFY_END_VSYNC    (0x04)
#define VTA_IOCTL_CMD_REQ_SET_OPC         (0x05)
#define VTA_IOCTL_CMD_DRM_CONTENT_START   (0x06)
#define VTA_IOCTL_CMD_DRM_CONTENT_STOP    (0x07)

static ssize_t vta_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long cmd;
	int ret;
	struct vta_data_t *vta = dev->platform_data;

	DBG("\n");
	ret = kstrtoul(buf, 10, &cmd);
	if (ret)
		return ret;

	switch (cmd) {
	case VTA_IOCTL_CMD_TEST:
		vta_test_cmd(vta);
		break;
	case VTA_IOCTL_CMD_OBSERVE:
		vta_cmd_observe(vta);
		break;
	case VTA_IOCTL_CMD_NOTIFY_END_VSYNC:
		vta_cmd_notify_change_status(__func__);
		break;
	case VTA_IOCTL_CMD_LINK_PROTECTION_ON:
		vta_test_link_protection(1);
		break;
	case VTA_IOCTL_CMD_LINK_PROTECTION_OFF:
		vta_test_link_protection(0);
		break;
	case VTA_IOCTL_CMD_REQ_SET_OPC:
		vta_test_req_set_opc(vta);
		break;
	case VTA_IOCTL_CMD_DRM_CONTENT_START:
		vta_test_drm_content_xxx(vta, 1);
		break;
	case VTA_IOCTL_CMD_DRM_CONTENT_STOP:
		vta_test_drm_content_xxx(vta, 0);
		break;
	default:
		break;
	}

	vta->cmd = cmd;
	return count;
}

static ssize_t vta_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct vta_data_t *vta = dev->platform_data;

	DBG("\n");
	vta_get_display_output();
	return sprintf(buf, "%lu\n", vta->cmd);
}

static DEVICE_ATTR(vta, S_IRUGO|S_IWUSR|S_IWGRP, vta_show, vta_store);

static struct attribute *vta_attributes[] = {
	&dev_attr_vta.attr,
	NULL
};

static struct attribute_group vta_attribute_group = {
	.name = NULL,
	.attrs = vta_attributes,
};
#endif	//__TEST_CODE__

/*
 * ioctls
 */
static long vta_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct vta_data_t *vta = (struct vta_data_t *)filp->private_data;
	unsigned long data;
	unsigned int output;

	DBG("cmd(0x%08x)\n", cmd);

	switch (cmd) {
	case IOCTL_VTA_CTX:
		/* Get output update */
		output = vta_get_display_output();
		if (copy_to_user((unsigned int *)arg, &output, sizeof(output)))
			return -EFAULT;

		return vta_get_context(vta, 1);
		//break;
	case IOCTL_VTA_CHECK:
		if (copy_from_user((void *)&data, (const void *)arg,
			sizeof(data))) {
			return -EFAULT;
		}
		break;
	default:
		pr_err("%s: unrecognized ioctl(0x%x)\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static int vta_open(struct inode *inode, struct file *filp)
{
	filp->private_data = (void *)vta_data;
	DBG("\n");
	return 0;
}

static int vta_release(struct inode *inode, struct file *filp)
{
	struct vta_data_t *vta = (struct vta_data_t *)filp->private_data;

	vta->cmd = 0;
	DBG("%s\n", vta->pdev->name);
	return 0;
}

/*
 * register character driver
 */
static const struct file_operations ta_fops = {
	.owner			= THIS_MODULE,
	.open			= vta_open,
	.release		= vta_release,
	.unlocked_ioctl = vta_ioctl,
};

static int vta_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = NULL;

	DBG("ver.%08x\n", VTA_VERSION);

	ret = alloc_chrdev_region(&vta_dev, 0, 1, DEV_NAME);
	if (ret) {
		pr_err("%s: chrdev_region error(%d)\n", __func__, ret);
		goto err3;
	}

	vta_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(vta_class)) {
		ret = PTR_ERR(vta_class);
		goto err2;
	}

	cdev_init(&vta_cdev, &ta_fops);
	ret = cdev_add(&vta_cdev, vta_dev, 1);
	if (ret) {
		pr_err("%s: cdev_add error(%d)\n", __func__, ret);
		goto err1;
	}

	dev = device_create(vta_class, NULL, vta_dev, NULL, DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("%s: device_create error(%d)\n", __func__, ret);
		goto err;
	}

	/*
	 * alloc driver private data
	 */
	vta_data = kmalloc(sizeof(struct vta_data_t),
		GFP_KERNEL);
	if (vta_data == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	memset(vta_data, 0, sizeof(struct vta_data_t));

	vta_data->pdev = pdev;
	dev->platform_data = (void *)vta_data;
	pdev->dev.platform_data = (void *)vta_data;

	pr_info("%s\n", __func__);
	return 0;

out:
	if (vta_data)
		kfree(vta_data);
err:
	cdev_del(&vta_cdev);
err1:
	class_destroy(vta_class);
err2:
	unregister_chrdev_region(vta_dev, 1);
err3:
	return ret;
}

static int vta_remove(struct platform_device *pdev)
{
	//struct vta_data_t *vta =
	//	(struct vta_data_t *)platform_get_drvdata(pdev);
	struct vta_data_t *vta = vta_data;

	kthread_stop(vta->vta_thread);
	tee_client_close_ta(vta->context);
	kfree(vta);

	device_destroy(vta_class, vta_dev);
	cdev_del(&vta_cdev);
	class_destroy(vta_class);
	unregister_chrdev_region(vta_dev, 1);

	DBG("\n");
	return 0;
}

static int vta_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct vta_data_t *vta = vta_data;

	if (!vta->context || !vta->context->session_initalized) {
		goto exit;
	}

	/* You don't have to stop kthread. */
	//if (vta->vta_thread != NULL && vta->vta_thread != ERR_PTR(-ENOMEM)) {
	//	kthread_stop(vta->vta_thread);
	//	vta->vta_thread = NULL;
	//}

	tee_client_close_ta(vta->context);
	vta->context = NULL;

exit:
	DBG("\n");
	return 0;
}

static int vta_resume(struct platform_device *pdev)
{
	/* You don't have to open vioc-ta because
	 * tee-supp opens it using IOCTL_VTA_CTX.
	 */
	DBG("\n");
	return 0;
}

/*
 * register platform driver
 */
static struct platform_device *vta_platform_device;
static struct platform_driver vta_device_driver = {
	.probe  = vta_probe,
	.remove = vta_remove,
	.suspend = vta_suspend,
	.resume = vta_resume,
	.driver = {
		.name  = DEV_NAME,
		.owner = THIS_MODULE,
	},
};
static int __init vta_init(void)
{
	int ret;

	DBG("\n");

	vta_platform_device = platform_device_alloc(DEV_NAME, -1);
	if (!vta_platform_device) {
		pr_err("%s: platform_device_alloc error\n", __func__);
		ret = -ENOMEM;
		goto err2;
	}
	ret = platform_device_add(vta_platform_device);
	if (ret < 0) {
		pr_err("%s: platform_device_add error(%d)\n", __func__, ret);
		goto err1;
	}
	ret = platform_driver_register(&vta_device_driver);
	if (ret < 0) {
		pr_err("%s: platform_driver_register error(%d)\n",
			__func__, ret);
		goto err;
	}

#if defined(__TEST_CODE__)
	ret = sysfs_create_group(&vta_platform_device->dev.kobj,
		&vta_attribute_group);
	if (ret)
		pr_err("%s: failed create sysfs(%d)\n", __func__, ret);
#endif

	return 0;
err:
	platform_device_unregister(vta_platform_device);
err1:
	platform_device_put(vta_platform_device);
err2:
	return ret;
}

static void __exit vta_exit(void)
{
	platform_driver_unregister(&vta_device_driver);
	platform_device_unregister(vta_platform_device);
	DBG("\n");
}

late_initcall(vta_init);
module_exit(vta_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips VTA driver");
MODULE_LICENSE("GPL");
