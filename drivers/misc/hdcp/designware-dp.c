/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) Telechips Inc.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "hdcp_interface.h"

#define HDCP_DRV_VERSION		("0.2.0")

/* Internal Options */
// #define HDCP22_INT_USED

/* DP Link register */
#define SWRESET				(0x0204)
#define SWRESET_HDCP			(1<<2)

/* Register offsets */
#define HDCPCFG				(0x0E00)
#define HDCPOBS				(0x0E04)
#define HDCPAPIINTCLR			(0x0E08)
#define HDCPAPIINTSTAT			(0x0E0C)
#define HDCPAPIINTMSK			(0x0E10)
#define HDCPKSVMEMCTRL			(0x0E18)
#define HDCP_REVOC_RAM			(0x0E20)
#define HDCP_REVOC_LIST			(0x10B8)
#define HDCPREG_BKSV0			(0x3600)
#define HDCPREG_BKSV1			(0x3604)
#define HDCPREG_ANCONF			(0x3608)
#define HDCPREG_AN0			(0x360C)
#define HDCPREG_AN1			(0x3610)
#define HDCPREG_RMLCTL			(0x3614)
#define HDCPREG_RMLSTS			(0x3618)
#define HDCPREG_SEED			(0x361C)
#define HDCPREG_DPK0			(0x3620)
#define HDCPREG_DPK1			(0x3624)
#define HDCP22GPIOSTS			(0x3628)
#define HDCP22GPIOCHNGSTS		(0x362C)
#define HDCPREG_DPK_CRC			(0x3630)

/* HDCP Configures HDCP controller Register bits filed */
#define HDCPCFG_DPCD12PLUS		(1<<7)
#define HDCPCFG_CP_IRQ			(1<<6)
#define HDCPCFG_BYPENCRYPTION		(1<<5)
#define HDCPCFG_HDCP_LOCK		(1<<4)
#define HDCPCFG_ENCRYPTIONDISABLE	(1<<3)
#define HDCPCFG_ENABLE_HDCP_13		(1<<2)
#define HDCPCFG_ENABLE_HDCP		(1<<1)

/* HDCP Observation Register bits filed */
#define HDCPOBS_BSTATUS_OFFSET		(19)
#define HDCPOBS_BSTATUS_MASK		(0xF)
#define HDCPOBS_BCAPS_OFFSET		(17)
#define HDCPOBS_BCAPS_MASK		(0x3)

/* Interrupt bits field */
#define INT_HDCP22_GPIO			(1<<8)
#define INT_HDCP_ENGAGED		(1<<7)
#define INT_HDCP_FAILED			(1<<6)
#define INT_KSVSHA1CALCDONE		(1<<5)
#define INT_AUXRESPNACK7TIMES		(1<<4)
#define INT_AUXRESPTIMEOUT		(1<<3)
#define INT_AUXRESPDEFER7TIMES		(1<<2)
#define INT_KSVACCESS			(1<<0)
#define INT_MASK_VALUE			(0x1FF)
#ifdef HDCP22_INT_USED
#define INT_UNMASK_VALUE		(0x000)
#else
#define INT_UNMASK_VALUE		(0x100)
#endif

struct hdcp_device {
	void __iomem		*reg;
	struct device		*dev;
	uint8_t			open_cs;	/* dev open count */
	uint8_t			status;		/* hdcp status */
	uint8_t			protocal;
};

static int hdcp_start_authentication(struct hdcp_device *hdcp)
{
	if (hdcp->protocal != HDCP_PROTOCAL_HDCP_1_X)
		return -EPERM;

	iowrite32(ioread32(hdcp->reg + HDCPCFG) | HDCPCFG_ENABLE_HDCP,
		  hdcp->reg + HDCPCFG);

	return 0;
}

static int hdcp_stop_authentication(struct hdcp_device *hdcp)
{
	if (hdcp->protocal != HDCP_PROTOCAL_HDCP_1_X)
		return -EPERM;

	iowrite32(ioread32(hdcp->reg + HDCPCFG) & ~HDCPCFG_ENABLE_HDCP,
		  hdcp->reg + HDCPCFG);

	hdcp->status = HDCP_STATUS_IDLE;

	return 0;
}

static int hdcp_get_bcaps(struct hdcp_device *hdcp,
			  struct HDCPBcaps __user *ubcaps)
{
	struct HDCPBcaps bcaps;

	if (copy_from_user(&bcaps, ubcaps, sizeof(bcaps)))
		return -EFAULT;

	/*
	 * HDCP on DP Bcaps
	 *
	 * bits 7-2: Reserved (must be zero)
	 * bit 1: REPEATER, HDCP Repeater capability
	 * bit 0: HDCP_CAPABLE
	 */
	bcaps.bcaps = (ioread32(hdcp->reg + HDCPOBS) >> HDCPOBS_BCAPS_OFFSET)
		      & HDCPOBS_BCAPS_MASK;

	if (copy_to_user(ubcaps, &bcaps, sizeof(bcaps)))
		return -EFAULT;

	return 0;
}

static int hdcp_get_bstatus(struct hdcp_device *hdcp,
			    struct HDCPBstatus __user *ubstatus)
{
	struct HDCPBstatus bstatus;

	if (copy_from_user(&bstatus, ubstatus, sizeof(bstatus)))
		return -EFAULT;

	/*
	 * HDCP on DP Bstatus
	 *
	 * bits 7-4: Reserved. Read as zero
	 * bit 3: REAUTHENTICATION_REQUEST
	 * bit 2: LINK_INTEGRITY_FAILURE
	 * bit 1: R0'_AVAILABLE
	 * bit 0: READY
	 */
	bstatus.bstatus = (ioread32(hdcp->reg + HDCPOBS)
			  >> HDCPOBS_BSTATUS_OFFSET) & HDCPOBS_BSTATUS_MASK;

	if (copy_to_user(ubstatus, &bstatus, sizeof(bstatus)))
		return -EFAULT;

	return 0;
}

static int hdcp_send_srm_list(struct hdcp_device *hdcp,
			      struct HDCPSrm __user *usrm)
{
	struct HDCPSrm srm;

	if (copy_from_user(&srm, usrm, sizeof(srm)))
		return -EFAULT;

	return 0;
}

static int hdcp_encryption_control(struct hdcp_device *hdcp,
				   uint32_t enable)
{
	if (hdcp->protocal != HDCP_PROTOCAL_HDCP_1_X)
		return -EPERM;

	if (enable) {
		iowrite32(ioread32(hdcp->reg + HDCPCFG)
			  & ~HDCPCFG_ENCRYPTIONDISABLE, hdcp->reg + HDCPCFG);
	} else {
		iowrite32(ioread32(hdcp->reg + HDCPCFG)
			  | HDCPCFG_ENCRYPTIONDISABLE, hdcp->reg + HDCPCFG);
	}

	return 0;
}

static int hdcp_get_status(struct hdcp_device *hdcp,
			   struct HDCPStatus __user *ustatus)
{
	struct HDCPStatus status;

	status.status = hdcp->status;
	if (copy_to_user(ustatus, &status, sizeof(status)))
		return -EFAULT;

	return 0;
}

static int hdcp_configure(struct hdcp_device *hdcp,
			  struct HDCPConfiguration __user *config)
{
	return -EPERM;
}

static int hdcp_set_protocal(struct hdcp_device *hdcp, uint32_t protocal)
{
	switch (protocal) {
	case HDCP_PROTOCAL_HDCP_1_X:
		iowrite32(HDCPCFG_DPCD12PLUS | HDCPCFG_ENCRYPTIONDISABLE |
			  HDCPCFG_ENABLE_HDCP_13, hdcp->reg + HDCPCFG);
		hdcp->protocal = HDCP_PROTOCAL_HDCP_1_X;
		hdcp->status = HDCP_STATUS_IDLE;
		break;
	case HDCP_PROTOCAL_HDCP_2_2:
		/* must be eanble hdcp */
		iowrite32(HDCPCFG_DPCD12PLUS | HDCPCFG_ENABLE_HDCP /* 0x0 */,
			  hdcp->reg + HDCPCFG);
		hdcp->protocal = HDCP_PROTOCAL_HDCP_2_2;
		hdcp->status = HDCP_STATUS_IDLE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int hdcp_get_hdcp2version(struct hdcp_device *hdcp,
			    struct HDCP2Version __user *version)
{
	return -EPERM;
}

static int hdcp_set_content_stream_type(struct hdcp_device *hdcp,
					uint32_t content_type)
{
	return -EPERM;
}

static irqreturn_t hdcp_irq(int irq, void *dev_id)
{
	struct hdcp_device *hdcp = dev_id;

	if (hdcp) {
		if (ioread32(hdcp->reg + HDCPAPIINTSTAT) & ~INT_UNMASK_VALUE) {
			/* mask interrupt */
			iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTMSK);
			return IRQ_WAKE_THREAD;
		}
	} else {
		pr_err("hdcp resource is not exist\n");
	}

	return IRQ_NONE;
}

static irqreturn_t hdcp_isr(int irq, void *dev_id)
{
	struct hdcp_device *hdcp = dev_id;
	uint32_t int_stat;

	if (!hdcp)
		return IRQ_NONE;

	int_stat = ioread32(hdcp->reg + HDCPAPIINTSTAT);

	/* clear interrupt signals */
	iowrite32(int_stat, hdcp->reg + HDCPAPIINTCLR);

	// TODO: Check HPD and rxsense

#ifdef HDCP22_INT_USED
	if (int_stat & INT_HDCP22_GPIO) {
		uint32_t hdcp22_gpio = ioread32(hdcp->reg + HDCP22GPIOCHNGSTS);

		iowrite32(hdcp22_gpio, hdcp->reg + HDCP22GPIOCHNGSTS);
		dev_dbg(hdcp->dev, "HDCP22: sts:0x%x\n", hdcp22_gpio);

		if (hdcp22_gpio & (1<<0)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Capable");
		}
		if (hdcp22_gpio & (1<<1)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Does not support");
		}
		if (hdcp22_gpio & (1<<2)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Successfully completed authentication");
		}
		if (hdcp22_gpio & (1<<3)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Not able to authenticate");
		}
		if (hdcp22_gpio & (1<<4)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Cipher syncronization has been lost");
		}
		if (hdcp22_gpio & (1<<5)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "receiver is requesting re-authentication");
		}
	}
#endif

	if (int_stat & INT_HDCP_ENGAGED) {
		dev_info(hdcp->dev, "HDCP_ENGAGED\n");
		hdcp->status = HDCP_STATUS_1_x_AUTENTICATED;
	} else if (int_stat & INT_HDCP_FAILED) {
		dev_info(hdcp->dev, "HDCP_FAILED\n");
		hdcp->status = HDCP_STATUS_1_x_AUTHENTICATION_FAILED;
	} else if (int_stat & INT_KSVSHA1CALCDONE) {
		dev_info(hdcp->dev, "KSVSHA1CALCDONE\n");
	} else if (int_stat & INT_AUXRESPNACK7TIMES) {
		dev_info(hdcp->dev, "AUXRESPNACK7TIMES\n");
	} else if (int_stat & INT_AUXRESPTIMEOUT) {
		dev_info(hdcp->dev, "AUXRESPTIMEOUT\n");
	} else if (int_stat & INT_AUXRESPDEFER7TIMES) {
		dev_info(hdcp->dev, "AUXRESPDEFER7TIMES\n");
	} else if (int_stat & INT_KSVACCESS) {
		dev_info(hdcp->dev, "KSVACCESS\n");
	}

	/* unmask insterrupt */
	iowrite32(INT_UNMASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

	return IRQ_HANDLED;
}

static long hdcp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = (struct miscdevice *)f->private_data;
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);
	void __user *data = (void __user *)arg;
	int ret;

	switch (cmd) {
	case HDCP_START_AUTHENTICATION:
		ret = hdcp_start_authentication(hdcp);
		break;
	case HDCP_STOP_AUTHENTICATION:
		ret = hdcp_stop_authentication(hdcp);
		break;
	case HDCP_GET_BCAPS:
		ret = hdcp_get_bcaps(hdcp, (struct HDCPBcaps *)data);
		break;
	case HDCP_GET_BSTATUS:
		ret = hdcp_get_bstatus(hdcp, (struct HDCPBstatus *)data);
		break;
	case HDCP_SEND_SRM_LIST:
		ret = hdcp_send_srm_list(hdcp, (struct HDCPSrm *)data);
		break;
	case HDCP_ENCRYPTION_CONTROL:
		ret = hdcp_encryption_control(hdcp, (uint32_t)arg);
		break;
	case HDCP_GET_STATUS:
		ret = hdcp_get_status(hdcp, (struct HDCPStatus *)data);
		break;
	case HDCP_CONFIGURE:
		ret = hdcp_configure(hdcp, (struct HDCPConfiguration *)data);
		break;
	case HDCP_SET_PROTOCAL:
		ret = hdcp_set_protocal(hdcp, (uint32_t)arg);
		break;
	case HDCP_GET_HDCP2VERSION:
		ret = hdcp_get_hdcp2version(hdcp, (struct HDCP2Version *)data);
		break;
	case HDCP_SET_CONTENT_STREAM_TYPE:
		ret = hdcp_set_content_stream_type(hdcp, (uint32_t)arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return (long)ret;
}

static int hdcp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = (struct miscdevice *)file->private_data;
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp->open_cs) {
		uint32_t soft_reset;

		dev_info(dev->parent, "%s\n", __func__);

		/* software reset */
		soft_reset = ioread32(hdcp->reg + SWRESET);
		iowrite32(soft_reset | SWRESET_HDCP, hdcp->reg + SWRESET);
		udelay(10); /* at least 5usec to insure a successful reset. */
		iowrite32(soft_reset & ~SWRESET_HDCP, hdcp->reg + SWRESET);

		/* clear interrupts */
		iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTCLR);

		/* unmask insterrupt */
		iowrite32(INT_UNMASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

		/*
		 * Switch to HDCP2.2 and enable hdcp.
		 * HDCP2.2 can be enabled only with '/dev/hd_dev'
		 */
		iowrite32(HDCPCFG_DPCD12PLUS | HDCPCFG_ENABLE_HDCP /* 0x0 */,
			  hdcp->reg + HDCPCFG);
	}

	hdcp->open_cs++;

	return 0;
}

static int hdcp_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = (struct miscdevice *)file->private_data;
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp->open_cs) {
		dev_err(dev->parent, "%s\n", __func__);
		return -1;
	}

	hdcp->open_cs--;

	if (!hdcp->open_cs) {
		/* mask insterrupt */
		iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

		dev_info(dev->parent, "%s\n", __func__);
	}

	return 0;
}

static const struct file_operations misc_hdcp_fops = {
	.owner			= THIS_MODULE,
	.open			= hdcp_open,
	.release		= hdcp_release,
	.unlocked_ioctl		= hdcp_ioctl,
};

static struct miscdevice misc_hdcp_driver = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hdcp",
	.fops = &misc_hdcp_fops,
};

static int hdcp_probe(struct platform_device *pdev)
{
	struct hdcp_device *hdcp;
	int ret, irq;

	hdcp = kzalloc(sizeof(struct hdcp_device), GFP_KERNEL);
	if (!hdcp)
		return -ENOMEM;

	hdcp->dev = &pdev->dev;
	hdcp->reg = of_iomap(pdev->dev.of_node, 0);
	if (!hdcp->reg) {
		dev_err(&pdev->dev, "failed to get controller\n");
		ret = -ENODEV;
		goto err;
	}

	/* hdcp irq shared with dp */
	irq = platform_get_irq(pdev, 0);
	ret = devm_request_threaded_irq(&pdev->dev, irq, hdcp_irq, hdcp_isr,
					IRQF_SHARED, "hdcp", hdcp);
	if (ret) {
		dev_err(&pdev->dev, "Could not register interrupt\n");
		goto err;
	}

	misc_hdcp_driver.parent = &pdev->dev;
	ret = misc_register(&misc_hdcp_driver);
	if (ret) {
		dev_err(&pdev->dev, "failed to register misc device.\n");
		goto err;
	}

	platform_set_drvdata(pdev, hdcp);
	dev_info(&pdev->dev, "driver probed. (ver: %s)\n", HDCP_DRV_VERSION);

	return 0;

err:
	kfree(hdcp);

	return ret;
}

static int hdcp_remove(struct platform_device *pdev)
{
	struct hdcp_device *hdcp = platform_get_drvdata(pdev);

	misc_deregister(&misc_hdcp_driver);
	kfree(hdcp);

	return 0;
}

static int hdcp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct hdcp_device *hdcp = platform_get_drvdata(pdev);

	/* clear interrupts */
	iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTCLR);

	/* mask insterrupt */
	iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

	return 0;
}

static int hdcp_resume(struct platform_device *pdev)
{
	struct hdcp_device *hdcp = platform_get_drvdata(pdev);

	/* clear interrupts */
	iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTCLR);

	/* unmask insterrupt */
	iowrite32(INT_UNMASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

	return 0;
}


static const struct of_device_id hdcp_of_match[] = {
	{ .compatible =	"dwc,hdcp-dp" },
	{ }
};
MODULE_DEVICE_TABLE(of, hdcp_of_match);

static struct platform_driver __refdata dwc_hdcp_dp_driver = {
	.driver		= {
		.name	= "dwc,hdcp-dp",
		.of_match_table = hdcp_of_match,
	},
	.probe		= hdcp_probe,
	.remove		= hdcp_remove,
	.suspend	= hdcp_suspend,
	.resume		= hdcp_resume,
};
module_platform_driver(dwc_hdcp_dp_driver);

MODULE_VERSION(HDCP_DRV_VERSION);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("HDCP for DesignWare Cores DisplayPort");
MODULE_LICENSE("GPL");
