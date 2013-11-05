/* Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2013 Toradex AG
 *
 * Freescale Faraday Quad ADC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mvf_adc.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define DRIVER_NAME "mvf-adc"
#define DRV_VERSION "1.2"

#define MVF_ADC_MAX_DEVICES 4
#define MVF_ADC_MAX ((1 << 12) - 1)

/*
 * wait_event_interruptible(wait_queue_head_t suspendq, int suspend_flag)
 */
static struct class *adc_class;
static int mvf_adc_major;

struct adc_client {
	struct platform_device	*pdev;
	struct list_head	 list;
	wait_queue_head_t	*wait;

	unsigned int	result;
	unsigned int	channel;
};

static LIST_HEAD(client_list);/* Initialization client list head */
static DECLARE_COMPLETION(adc_tsi);

struct adc_device {
	struct platform_device	*pdev;
	struct clk		*clk;
	struct adc_client	*cur;
	void __iomem		*regs;
	spinlock_t		 lock;

	struct device		*dev;
	struct cdev		 cdev;

	int			 irq;
};

static struct adc_device *adc_devices[MVF_ADC_MAX_DEVICES];

struct data {
	unsigned int res_value;
	bool flag;
};

struct data data_array[32];

#define adc_dbg(_adc, msg...) dev_dbg(&(_adc)->pdev->dev, msg)

struct adc_client *adc_register(struct platform_device *pdev,
		unsigned char channel);

/* select channel and enable conversion  */
static inline void adc_convert(struct adc_device *adc,
		struct adc_client *client)
{

	unsigned con = readl(adc->regs + ADC_HC0);
	con  = ADCHC0(client->channel);
	con |= IRQ_EN;
	writel(con, adc->regs + ADC_HC0);
}

/* find next conversion client */
static void adc_try(struct adc_device *adc)
{
	struct adc_client *next = adc->cur;
	if (!list_empty(&client_list)) {
		next = list_entry(client_list.next,
				struct adc_client, list);
		list_del(client_list.next);

		adc->cur = next;
		adc_convert(adc, adc->cur);
	}
}

static int adc_initiate(struct adc_device *adc_dev)
{
	unsigned long reg, tmp, pin;
	struct adc_device *adc = adc_dev;


	/* PCTL: pin control 5 for Sliding rheostat */
	pin = readl(adc->regs+ADC_PCTL);
	pin &= ~CLEPCTL23;
	pin &= SETPCTL5;
	writel(pin, adc->regs+ADC_PCTL);

	/* CFG: Feature set */
	reg = readl(adc->regs+ADC_CFG);
	reg &= ~CLECFG16;
	reg |= SETCFG;
	writel(reg, adc->regs+ADC_CFG);

	/* GC: software trigger */
	tmp = readl(adc->regs+ADC_GC);
	tmp = 0;
	writel(tmp, adc->regs+ADC_GC);

	return 0;
}

static int adc_set(struct adc_device *adc_dev, struct adc_feature *adc_fea)
{
	struct adc_device *adc = adc_dev;
	int con, res;
	con = readl(adc->regs+ADC_CFG);
	res = readl(adc->regs+ADC_GC);

	/* clock select and clock devide */
	switch (adc_fea->clk_sel) {
	case ADCIOC_BUSCLK_SET:
			/* clear 1-0,6-5 */
		con &= ~CLEAR_CLK_BIT;
		switch (adc_fea->clk_div_num) {
		case 1:
			break;
		case 2:
			con |= CLK_DIV2;
			break;
		case 4:
			con |= CLK_DIV4;
			break;
		case 8:
			con |= CLK_DIV8;
			break;
		case 16:
			con |= BUSCLK2_SEL | CLK_DIV8;
			break;
		default:
			return -EINVAL;
	}

	break;

	case ADCIOC_ALTCLK_SET:
	/* clear 1-0,6-5 */
		con &= ~CLEAR_CLK_BIT;

		con |= ALTCLK_SEL;
	switch (adc_fea->clk_div_num) {
	case 1:
		break;
	case 2:
		con |= CLK_DIV2;
		break;
	case 4:
		con |= CLK_DIV4;
		break;
	case 8:
		con |= CLK_DIV8;
		break;
	default:
		return -EINVAL;
	}

	break;

	case ADCIOC_ADACK_SET:
		/* clear 1-0,6-5 */
		con &= ~CLEAR_CLK_BIT;

		con |= ADACK_SEL;
		switch (adc_fea->clk_div_num) {
		case 1:
			break;
		case 2:
			con |= CLK_DIV2;
			break;
		case 4:
			con |= CLK_DIV4;
			break;
		case 8:
			con |= CLK_DIV8;
			break;
		default:
			return -EINVAL;
		}
	break;

	default:
		pr_debug("adc_ioctl: unsupported ioctl command 0x%x\n",
				adc_fea->clk_sel);
		return -EINVAL;
	}

	/* resolution mode,clear 3-2 */
	con &= ~CLEAR_MODE_BIT;
	switch (adc_fea->res_mode) {
	case 8:
		break;
	case 10:
		con |= BIT10;
		break;
	case 12:
		con |= BIT12;
		break;
	default:
		pr_debug("adc_ioctl: unsupported ioctl resolution mode 0x%x\n",
				adc_fea->res_mode);
		return -EINVAL;
	}

	/* Defines the sample time duration */
	/* clear 4, 9-8 */
	con &= ~CLEAR_LSTC_BIT;
	switch (adc_fea->sam_time) {
	case 2:
		break;
	case 4:
		con |= ADSTS_SHORT;
		break;
	case 6:
		con |= ADSTS_NORMAL;
		break;
	case 8:
		con |= ADSTS_LONG;
		break;
	case 12:
		con |= ADLSMP_LONG;
		break;
	case 16:
		con |= ADLSMP_LONG | ADSTS_SHORT;
		break;
	case 20:
		con |= ADLSMP_LONG | ADSTS_NORMAL;
		break;
	case 24:
		con |= ADLSMP_LONG | ADSTS_LONG;
		break;
	default:
		pr_debug("adc_ioctl: unsupported ioctl sample"
				"time duration 0x%x\n",
				adc_fea->sam_time);
		return -EINVAL;
	}

	/* low power configuration */
	/* */
	switch (adc_fea->lp_con) {
	case ADCIOC_LPOFF_SET:
		con &= ~CLEAR_ADLPC_BIT;
		break;

	case ADCIOC_LPON_SET:
		con &= ~CLEAR_ADLPC_BIT;
		con |= ADLPC_EN;
		break;
	default:
		return -EINVAL;

	}
	/* high speed operation */
	switch (adc_fea->hs_oper) {
	case ADCIOC_HSON_SET:
		con &= ~CLEAR_ADHSC_BIT;
		con |= ADHSC_EN;
		break;

	case ADCIOC_HSOFF_SET:
		con &= ~CLEAR_ADHSC_BIT;
		break;
	default:
		return -EINVAL;
	}
	/* voltage reference*/
	switch (adc_fea->vol_ref) {
	case ADCIOC_VR_VREF_SET:
		con &= ~CLEAR_REFSEL_BIT;
		break;

	case ADCIOC_VR_VALT_SET:
		con &= ~CLEAR_REFSEL_BIT;
		con |= REFSEL_VALT;
		break;

	case ADCIOC_VR_VBG_SET:
		con &= ~CLEAR_REFSEL_BIT;
		con |= REFSEL_VBG;
		break;
	default:
		return -EINVAL;
	}

	/* trigger select */
	switch (adc_fea->tri_sel) {
	case ADCIOC_SOFTTS_SET:
		con &= ~CLEAR_ADTRG_BIT;
		break;

	case ADCIOC_HARDTS_SET:
		con &= ~CLEAR_ADTRG_BIT;
		con |= ADTRG_HARD;
		break;
	default:
		return -EINVAL;
	}

	/* hardware average select */
	switch (adc_fea->ha_sel) {
	case ADCIOC_HA_DIS:
		con &= ~CLEAR_AVGS_BIT;
		res &= ~CLEAR_AVGE_BIT;
		break;

	case ADCIOC_HA_SET:
		con &= ~CLEAR_AVGS_BIT;
		switch (adc_fea->ha_sam) {
		case 4:
			break;
		case 8:
			con |= AVGS_8;
			break;
		case 16:
			con |= AVGS_16;
			break;
		case 32:
			con |= AVGS_32;
			break;
		default:
			return -EINVAL;
		}
		res &= ~CLEAR_AVGE_BIT;
		res |= AVGEN;

		break;
	default:
		return -EINVAL;
	}

	/* data overwrite enable */
	switch (adc_fea->do_ena) {
	case ADCIOC_DOEON_SET:
		con |= OVWREN;
		break;

	case ADCIOC_DOEOFF_SET:
		con &= ~CLEAR_OVWREN_BIT;
		break;
	default:
		return -EINVAL;
	}

	/* Asynchronous clock output enable */
	switch (adc_fea->ac_ena) {
	case ADCIOC_ADACKENON_SET:
		res &= ~CLEAR_ADACKEN_BIT;
		res |= ADACKEN;
		break;

	case ADCIOC_ADACKENOFF_SET:
		res &= ~CLEAR_ADACKEN_BIT;
		break;
	default:
		return -EINVAL;
	}

	/* dma enable */
	switch (adc_fea->dma_ena) {
	case ADCIDC_DMAON_SET:
		res &= ~CLEAR_DMAEN_BIT;
		res |= DMAEN;
		break;

	case ADCIDC_DMAOFF_SET:
		res &= ~CLEAR_DMAEN_BIT;
		break;
	default:
		return -EINVAL;
	}

	/* continue function enable */
	switch (adc_fea->cc_ena) {
	case ADCIOC_CCEOFF_SET:
		res &= ~CLEAR_ADCO_BIT;
		break;

	case ADCIOC_CCEON_SET:
		res &= ~CLEAR_ADCO_BIT;
		res |= ADCON;
		break;
	default:
		return -EINVAL;
	}

	/* compare function enable */
	switch (adc_fea->compare_func_ena) {
	case ADCIOC_ACFEON_SET:
		res &= ~CLEAR_ACFE_BIT;
		res |= ACFE;
		break;

	case ADCIOC_ACFEOFF_SET:
		res &= ~CLEAR_ACFE_BIT;
		break;
	default:
		return -EINVAL;
	}

	/* greater than enable */
	switch (adc_fea->greater_ena) {
	case ADCIOC_ACFGTON_SET:
		res &= ~CLEAR_ACFGT_BIT;
		res |= ACFGT;
		break;

	case ADCIOC_ACFGTOFF_SET:
		res &= ~CLEAR_ACFGT_BIT;
		break;
	default:
		return -EINVAL;
	}

	/* range enable */
	switch (adc_fea->range_ena) {
	case ADCIOC_ACRENON_SET:
		res &= ~CLEAR_ACREN_BIT;
		res |= ACREN;
		break;

	case ADCIOC_ACRENOFF_SET:
		res &= ~CLEAR_ACREN_BIT;
		break;

	default: return -ENOTTY;
	}

	/* write register once */
	writel(con, adc->regs+ADC_CFG);
	writel(res, adc->regs+ADC_GC);

	return 0;
}

static int adc_convert_wait(struct adc_device *adc_dev, unsigned char channel)
{
	INIT_COMPLETION(adc_tsi);
	adc_try(adc_dev);
	wait_for_completion(&adc_tsi);

	if (!data_array[channel].flag)
		return -EINVAL;

	data_array[channel].flag = 0;
	return data_array[channel].res_value;
}

/**
 * mvf_adc_initiate - Initiate a given ADC converter
 *
 * @adc: ADC block to initiate
 */
int mvf_adc_initiate(unsigned int adc)
{
	return adc_initiate(adc_devices[adc]);
}
EXPORT_SYMBOL(mvf_adc_initiate);

/**
 * mvf_adc_set - Configure a given ADC converter
 *
 * @adc: ADC block to configure
 * @adc_fea: Features to enable
 *
 * Returns zero on success, error number otherwise
 */
int mvf_adc_set(unsigned int adc, struct adc_feature *adc_fea)
{
	return adc_set(adc_devices[adc], adc_fea);
}
EXPORT_SYMBOL(mvf_adc_set);

/**
 * mvf_adc_register_and_convert - Register a client and start a convertion
 *
 * @adc: ADC block
 * @channel: Channel to convert
 *
 * Returns converted value or error code
 */
int mvf_adc_register_and_convert(unsigned int adc, unsigned char channel)
{
	struct adc_client *client;
	int result;

	/* Register client... */
	client = adc_register(adc_devices[adc]->pdev, channel);
	if (!client)
		return -ENOMEM;

	/* Start convertion */
	result = adc_convert_wait(adc_devices[adc], channel);

	/* Free client */
	kfree(client);

	return result;
}
EXPORT_SYMBOL(mvf_adc_register_and_convert);

static int adc_open(struct inode *inode, struct file *file)
{
	struct adc_device *dev = container_of(inode->i_cdev,
			struct adc_device, cdev);

	file->private_data = dev;

	return 0;
}

static long adc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct adc_device *adc_dev = file->private_data;
	void __user *argp = (void __user *)arg;
	struct adc_feature feature;
	int channel;

	if (_IOC_TYPE(cmd) != 'p')
		return -ENOTTY;

	if (copy_from_user(&feature, (struct adc_feature *)argp,
				sizeof(feature)))
		return -EFAULT;

	if (feature.channel > 31)
		return -EINVAL;

	switch (cmd) {
	case ADC_INIT:
		return adc_initiate(adc_dev);
		break;

	case ADC_CONFIGURATION:
		return adc_set(adc_dev, &feature);
		break;

	case ADC_REG_CLIENT:
		channel = feature.channel;
		adc_register(adc_dev->pdev, channel);
		break;

	case ADC_CONVERT:
		feature.result0 = adc_convert_wait(adc_dev, feature.channel);

		if (copy_to_user((struct adc_feature *)argp, &feature,
				sizeof(feature)))
			return -EFAULT;

		kfree(adc_dev->cur);
		break;

	default:
		pr_debug("adc_ioctl: unsupported ioctl command 0x%x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

/* client register */
struct adc_client *adc_register(struct platform_device *pdev,
		unsigned char channel)
{
	struct adc_client *client = kzalloc(sizeof(struct adc_client),
			GFP_KERNEL);

	WARN_ON(!pdev);

	if (!pdev)
		return ERR_PTR(-EINVAL);


	if (!client) {
		dev_err(&pdev->dev, "no memory for adc client\n");
		return ERR_PTR(-ENOMEM);
	}

	client->pdev = pdev;
	client->channel = channel;
	list_add(&client->list, &client_list);

	return client;
}

static irqreturn_t adc_irq(int irq, void *pw)
{
	int coco;
	struct adc_device *adc = pw;
	struct adc_client *client = adc->cur;

	if (!client) {
		dev_warn(&adc->pdev->dev, "%s: no adc pending\n", __func__);
		goto exit;
	}

	coco = readl(adc->regs + ADC_HS);
	if (coco & 1) {
		data_array[client->channel].res_value = 
			readl(adc->regs + ADC_R0);
		data_array[client->channel].flag = 1;
		complete(&adc_tsi);
	}

exit:
	return IRQ_NONE;
}

static const struct file_operations adc_fops = {
	.owner			= THIS_MODULE,
	.open			= adc_open,
	.unlocked_ioctl		= adc_ioctl,
	.read			= NULL,
};

/* probe */
static int __devinit adc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct adc_device *adc;
	struct resource *regs;
	dev_t devt;
	int ret;

	adc = kzalloc(sizeof(struct adc_device), GFP_KERNEL);
	if (adc == NULL) {
		dev_err(dev, "failed to allocate adc_device\n");
		return -ENOMEM;
	}
	adc->pdev = pdev;

	/* Initialize spin_lock */
	spin_lock_init(&adc->lock);

	adc->irq = platform_get_irq(pdev, 0);
	if (adc->irq <= 0) {
		dev_err(dev, "failed to get adc irq\n");
		ret = -EINVAL;
		goto err_alloc;
	}

	ret = request_irq(adc->irq, adc_irq, 0, dev_name(dev), adc);
	if (ret < 0) {
		dev_err(dev, "failed to attach adc irq\n");
		goto err_alloc;
	}

	adc->clk = clk_get(dev, NULL);
	if (IS_ERR(adc->clk)) {
		dev_err(dev, "failed to get adc clock\n");
		ret = PTR_ERR(adc->clk);
		goto err_irq;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(dev, "failed to find registers\n");
		ret = -ENXIO;
		goto err_clk;
	}

	adc->regs = ioremap(regs->start, resource_size(regs));
	if (!adc->regs) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
		goto err_clk;
	}

	/* clk enable */
	clk_enable(adc->clk);

	/* Save device structure by Platform device ID for touch */
	adc_devices[pdev->id] = adc;

	/* Create character device for ADC */
	cdev_init(&adc->cdev, &adc_fops);
	adc->cdev.owner = THIS_MODULE;
	devt = MKDEV(mvf_adc_major, pdev->id);
	ret = cdev_add(&adc->cdev, devt, 1);
	if (ret < 0)
		goto err_clk;

	adc->dev = device_create(adc_class, &pdev->dev, devt,
			NULL, "mvf-adc.%d", pdev->id);
	if (IS_ERR(adc->dev)) {
		dev_err(dev, "failed to create device\n");
		goto err_cdev;
	}

	/* Associated structures */
	platform_set_drvdata(pdev, adc);

	dev_info(dev, "attached adc driver\n");

	return 0;

err_cdev:
	cdev_del(&adc->cdev);

err_clk:
	clk_put(adc->clk);

err_irq:
	free_irq(adc->irq, adc);

err_alloc:
	kfree(adc);

	return ret;
}

static int __devexit adc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct adc_device *adc = platform_get_drvdata(pdev);

	dev_info(dev, "remove adc driver\n");

	device_destroy(adc_class, adc->dev->devt);
	cdev_del(&adc->cdev);
	iounmap(adc->regs);
	free_irq(adc->irq, adc);
	clk_disable(adc->clk);
	clk_put(adc->clk);
	adc_devices[pdev->id] = NULL;
	kfree(adc);

	return 0;
}

static struct platform_driver adc_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= adc_probe,
	.remove		= __devexit_p(adc_remove),
};

static int __init adc_init(void)
{
	int ret;
	dev_t dev;

	adc_class = class_create(THIS_MODULE, "mvf-adc");
	if (IS_ERR(adc_class)) {
		ret = PTR_ERR(adc_class);
		printk(KERN_ERR "%s: can't register mvf-adc class\n",__func__);
		goto err;
	}

	/* Obtain device numbers and register char device */
	ret = alloc_chrdev_region(&dev, 0, MVF_ADC_MAX_DEVICES, "mvf-adc");
	if (ret)
	{
		printk(KERN_ERR "%s: can't register character device\n", 
				__func__);
		goto err_class;
	}
	mvf_adc_major = MAJOR(dev);

	ret = platform_driver_register(&adc_driver);
	if (ret)
		printk(KERN_ERR "%s: failed to add adc driver\n", __func__);

	return 0;
err_class:
	class_destroy(adc_class);
err:
	return ret;
}

static void __exit adc_exit(void)
{
	platform_driver_unregister(&adc_driver);
	class_destroy(adc_class);
}
module_init(adc_init);
module_exit(adc_exit);

MODULE_AUTHOR("Xiaojun Wang, Stefan Agner");
MODULE_DESCRIPTION("Vybrid ADC driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
