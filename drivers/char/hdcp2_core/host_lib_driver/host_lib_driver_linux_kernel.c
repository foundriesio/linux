// ------------------------------------------------------------------------
//
//              (C) COPYRIGHT 2014 - 2015 SYNOPSYS, INC.
//              (C) COPYRIGHT Telechips, INC.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// ------------------------------------------------------------------------

#include <linux/kernel.h>
#include <linux/module.h>
//#include <linux/io.h>
//#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <linux/random.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/of_device.h>
#include "include/host_lib_driver_linux_if.h"
#include "../../hdmi_v2_0/include/hdmi_includes.h"
#include "../../hdmi_v2_0/hdmi_api_lib/include/hdcp/hdcp.h"

#if defined(CONFIG_PM)
#include <linux/pm.h>
#endif

#ifdef CONFIG_ANDROID
#include <linux/wakelock.h>
#endif

#if defined(CONFIG_ARCH_TCC899X) && defined(CONFIG_ANDROID)
#define OPTEE_BASE_HDCP
#endif

#define HDCP_DRV_VERSION "1.0.9"

#define USE_HDMI_PWR_CTRL

#define DWC_PHY_STAT0		(0x3004 * 4)

#define DWC_HDCP_CFG1		(0x5001 * 4)
#define DWC_HDCP_INTCLR		(0x5006 * 4)	/* a_apiintclr */
#define DWC_HDCP_INTSTAT	(0x5007 * 4)	/* a_apiintstat */
#define DWC_HDCP_INTMASK	(0x5008 * 4)	/* a_apiintmask */
#define DWC_HDCP_VIDPOLCFG	(0x5009 * 4)

#define DWC_HDCP22_ID		(0x7900 * 4)	/* hdcp22reg_id */
#define DWC_HDCP22_CTRL		(0x7904 * 4)	/* hdcp22reg_ctrl */
#define DWC_HDCP22_CTRL1	(0x7905 * 4)	/* hdcp22reg_ctrl1 */
#define DWC_HDCP22_INTSTS	(0x7908 * 4)	/* hdcp22reg_sts */
#define DWC_HDCP22_INTMASK	(0x790c * 4)	/* hdcp22reg_mask */
#define DWC_HDCP22_INTSTAT	(0x790d * 4)	/* hdcp22reg_stat */
#define DWC_HDCP22_INTMUTE	(0x790e * 4)	/* hdcp22reg_mute */

#define DWC_HPI_SIZE		128
#define DWC_ESM_CODE_SIZE	0x30000

MODULE_PARM_DESC(noverify, "Wipe memory allocations on startup (for debug)");

typedef struct {
	void __iomem	*link;
	void __iomem	*hdcp22;
	void __iomem	*i2c;

	uint16_t	hdcp_status;
	uint8_t		hpd_status;
	uint8_t		rxsense_status;

	uintptr_t	hpi_paddr;
	uintptr_t	esm_paddr;
	uintptr_t	esm_vaddr;
	uint32_t	esm_size;
	uint32_t	code_size;
	uint32_t	data_offs;
	uint32_t	data_size;

	struct hdmi_tx_dev *dev;
	hdcpParams_t *params;

	/** Device node */
	struct device *parent_dev;

	/** Misc Device */
	struct miscdevice *misc;

	/** Device Open Count */
	uint32_t open_cs;

#ifdef CONFIG_ANDROID
	struct wake_lock wakelock;
#endif
	bool		initialized;
	bool		code_loaded;
} esm_device;

#ifdef USE_HDMI_PWR_CTRL
extern void hdmi_api_power_control(int enable);
#endif


static uint8_t dwc_get_hpd_status(esm_device *esm)
{
	if (!esm)
		return 0;
	esm->hpd_status = (ioread32(esm->link + DWC_PHY_STAT0) & (1<<1)) ? 1 : 0;
	return esm->hpd_status;
}

static uint8_t dwc_get_rxsense_status(esm_device *esm)
{
	if (!esm)
		return 0;
	esm->rxsense_status = (ioread32(esm->link + DWC_PHY_STAT0) >> 4) & 0xF;
	return esm->rxsense_status;
}

static irqreturn_t hdcp_irq(int irq, void *dev_id)
{
	esm_device *esm = dev_id;
	if(esm) {
		if(ioread32(esm->link + DWC_HDCP_INTSTAT) | ioread32(esm->link + DWC_HDCP22_INTSTAT)) {
			iowrite32(0xff, esm->link + DWC_HDCP_INTMASK);
			iowrite32(0xff, esm->link + DWC_HDCP22_INTMASK);
			iowrite32(0xff, esm->link + DWC_HDCP22_INTMUTE);
			return IRQ_WAKE_THREAD;
		}
	}
	return IRQ_NONE;
}

static irqreturn_t hdcp_isr(int irq, void *dev_id)
{
	esm_device *esm = dev_id;
	uint32_t hdcp = 0, hdcp22=0, hdcpcfg1;

	if (!esm)
		return IRQ_NONE;

	hdcp = ioread32(esm->link + DWC_HDCP_INTSTAT);
	hdcp22 = ioread32(esm->link + DWC_HDCP22_INTSTAT);

	/* clear interrupt signals */
	iowrite32(hdcp, esm->link + DWC_HDCP_INTCLR);
	iowrite32(hdcp22, esm->link + DWC_HDCP22_INTSTAT);

	//printk("\x1b[1;33m hdcp:%08x, hdcp22:%02x\x1b[0m\n", hdcp, hdcp22);

	if (!dwc_get_hpd_status(esm) || !dwc_get_rxsense_status(esm)) {
		printk("%s: HDCP_IDLE - hpd: %d, rxsense: %d\n", __func__, esm->hpd_status, esm->rxsense_status);
		esm->hdcp_status = HDCP_IDLE;
		goto hdcp_isr_exit;
	}

	if (hdcp) {
		hdcpcfg1 = ioread32(esm->link + DWC_HDCP_CFG1);
		if (hdcp & (1<<2)) {
			/*
			 * - keepouterrorint -
			 * We met an issue what shows the black screen at the lower resolution.
			 * It fixed by changing VSYNC polarity.
			 */
			uint32_t vidpol = ioread32(esm->link + DWC_HDCP_VIDPOLCFG);
			if (vidpol & (1<<3))
				vidpol &= ~(1<<3);
			else
				vidpol |= (1<<3);
			iowrite32(vidpol, esm->link + DWC_HDCP_VIDPOLCFG);
		}
		if (hdcp & (1<<5)) {
			printk(KERN_INFO "%s: INT_KSV_SHA1_DONE\n", __func__);
			esm->hdcp_status = HDCP_KSV_LIST_READY;
		}
		if (hdcp & (1<<6)) {
			if (!(hdcpcfg1 & (1<<1)))
				iowrite32(hdcpcfg1 | (1<<1), esm->link + DWC_HDCP_CFG1);
			printk(KERN_INFO "%s: HDCP 1.4 Authentication process - HDCP_FAILED \n", __func__);
			esm->hdcp_status = HDCP_FAILED;
		}
		if (hdcp & (1<<7)) {
			if (hdcpcfg1 & (1<<1))
				iowrite32(hdcpcfg1 & ~(1<<1), esm->link + DWC_HDCP_CFG1);
			printk(KERN_INFO "%s: HDCP 1.4 Authentication process - HDCP_ENGAGED \n", __func__);
			esm->hdcp_status = HDCP_ENGAGED;
		}
	}

	if (hdcp22) {
		if (hdcp22 & (1<<0)) {
			esm->hdcp_status = HDCP2_CAPABLE;
			printk(KERN_INFO "%s: HDCP22REG_STAT_ST_HDCP2_CAPABLE\n", __func__);
		}
		if (hdcp22 & (1<<1)) {
			esm->hdcp_status = HDCP2_NOT_CAPABLE;
			printk(KERN_INFO "%s: %s\n", __func__, "HDCP22REG_STAT_ST_HDCP2_NOT_CAPABLE");
		}
		if (hdcp22 & (1<<2)) {
			esm->hdcp_status = HDCP2_AUTHENTICATION_LOST;
			printk("%s: %s\n", __func__, "HDCP22REG_STAT_ST_HDCP_AUTHENTICATION_LOST");
			if (!esm->hpd_status || !esm->rxsense_status) {
				printk(KERN_INFO
					"%s: %s, hpd : %d, rxsense : %d\n", __func__, "HDCP_IDLE", esm->hpd_status,
					esm->rxsense_status);
				esm->hdcp_status = HDCP_IDLE;
			}
		}
		if (hdcp22 & (1<<3)) {
			esm->hdcp_status = HDCP2_AUTHENTICATED;
			printk(KERN_INFO "%s: %s\n", __func__, "HDCP22REG_STAT_ST_HDCP_AUTHENTICATED");
		}
		if (hdcp22 & (1<<4)) {
			esm->hdcp_status = HDCP2_AUTHENTICATION_FAIL;
			printk(KERN_INFO "%s: %s\n", __func__, "HDCP22REG_STAT_ST_HDCP_AUTHENTICATION_FAIL");
			if (!esm->hpd_status || !esm->rxsense_status) {
				printk(KERN_INFO
					"%s: %s, hpd : %d, rxsense : %d\n", __func__, "HDCP_IDLE", esm->hpd_status,
					esm->rxsense_status);
				esm->hdcp_status = HDCP_IDLE;
			}
		}
		if (hdcp22 & (1<<5)) {
			printk(KERN_INFO "%s: %s\n", __func__, "HDCP22REG_STAT_ST_HDCP_DECRYPTED_CHG");
			esm->hdcp_status = HDCP2_DECRYPTED_CHG;
		}
	}

hdcp_isr_exit:
	esm->dev->hdcp_status = (int)esm->hdcp_status;

/* unmask insterrupt */
	iowrite32(0, esm->link + DWC_HDCP_INTMASK);
	iowrite32(0, esm->link + DWC_HDCP22_INTMASK);
	iowrite32(0, esm->link + DWC_HDCP22_INTMUTE);

	return IRQ_HANDLED;
}

static irqreturn_t hdcp22_irq(int irq, void *dev_id)
{
	printk("\x1b[1;33m %s \x1b[0m\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t hdcp22_isr(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

/* ESM_IOC_MEMINFO implementation */
static long get_meminfo(esm_device *esm, void __user *arg)
{
	struct esm_ioc_meminfo info = {
		.hpi_base = (uint32_t)esm->hpi_paddr,
		.code_base = (uint32_t)esm->esm_paddr,
		.code_size = esm->code_size,
		.data_base = (uint32_t)esm->esm_paddr + esm->data_offs,
		.data_size = esm->data_size,
	};

	if (copy_to_user(arg, &info, sizeof info) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_LOAD_CODE implementation */
static long load_code(esm_device *esm, struct esm_ioc_code __user *arg)
{
	struct esm_ioc_code head;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (head.len > esm->code_size)
		return -ENOSPC;
#if 0 // remove by yonghee, we need to load/unload according hdmi status
	if (esm->code_loaded)
		return -EBUSY;
#endif

	if (copy_from_user((void *)esm->esm_vaddr, &arg->data, head.len) != 0)
		return -EFAULT;

	esm->code_loaded = true;
	return 0;
}

/* ESM_IOC_WRITE_DATA implementation */
static long write_data(esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_from_user((void *)(esm->esm_vaddr + esm->data_offs + head.offset), &arg->data, head.len) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_READ_DATA implementation */
static long read_data(esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_to_user(&arg->data, (void *)(esm->esm_vaddr + esm->data_offs + head.offset), head.len) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_MEMSET_DATA implementation */
static long set_data(esm_device *esm, void __user *arg)
{
	union {
		struct esm_ioc_data data;
		unsigned char buf[sizeof(struct esm_ioc_data) + 1];
	} u;

	if (copy_from_user(&u.data, arg, sizeof u.buf) != 0)
		return -EFAULT;

	if (esm->data_size < u.data.len)
		return -ENOSPC;
	if (esm->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset((void *)(esm->esm_vaddr + esm->data_offs + u.data.offset), u.data.data[0], u.data.len);
	return 0;
}

/* ESM_IOC_READ_HPI implementation */
static long hpi_read(esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= DWC_HPI_SIZE)
		return -EINVAL;

	reg.value = ioread32(esm->hdcp22 + reg.offset);

	if (copy_to_user(arg, &reg, sizeof reg) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_WRITE_HPI implementation */
static long hpi_write(esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= DWC_HPI_SIZE)
		return -EINVAL;

	iowrite32(reg.value, esm->hdcp22 + reg.offset);
	return 0;
}

/* HDCP1_INIT implementation */
static long hdcp1Initialize(esm_device *esm, void __user *arg)
{
	struct dwc_hdmi_hdcp_data hdcpData;

	if (copy_from_user(&hdcpData, arg, sizeof hdcpData) != 0)
		return -EFAULT;

	esm->params = &hdcpData.hdcpParam;
	esm->dev = dwc_hdmi_api_get_dev();

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 1;
	hdcp1p4Configure(esm->dev, esm->params);
	hdcp1p4SwitchSet(esm->dev);
	mutex_unlock(&esm->dev->mutex);

	/* clear interrupt */
	iowrite32(0xff, esm->link + DWC_HDCP_INTCLR);

	/*unmask interrupt*/
	iowrite32(0xff, esm->link + DWC_HDCP_INTMASK);

	return 0;
}

/* HDCP1_START implementation */
static long hdcp1Start(esm_device *esm, void __user *arg)
{
	struct dwc_hdmi_hdcp_data hdcpData;

	if (copy_from_user(&hdcpData, arg, sizeof hdcpData) != 0)
		return -EFAULT;

	if (!esm->dev)
		return -ENOSPC;

	esm->params = &hdcpData.hdcpParam;
	mutex_lock(&esm->dev->mutex);
	mc_hdcp_clock_enable(esm->dev, 0);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	hdcp1p4Start(esm->dev, esm->params);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP1_STOP implementation */
static long hdcp1Stop(esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
	hdcp1p4Stop(esm->dev);
	mc_hdcp_clock_enable(esm->dev, 1);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP_GET_STATUS implementation */
static long hdcpGetStatus(esm_device *esm, void __user *arg)
{
	struct hdcp_ioc_data data;

	if (copy_from_user(&data, arg, sizeof data) != 0)
		return -EFAULT;

	data.status = esm->hdcp_status;

	if (copy_to_user(arg, &data, sizeof data) != 0)
		return -EFAULT;

	return 0;
}

/* HDCP2_INIT implementation */
static long hdcp2Initialize(esm_device *esm, void __user *arg)
{
	esm->dev = dwc_hdmi_api_get_dev();

	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 2;
	hdcp2p2SwitchSet(esm->dev);
	mc_hdcp_clock_enable(esm->dev, 0);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	mutex_unlock(&esm->dev->mutex);

	/* clear interrupt */
	iowrite32(0xff, esm->link + DWC_HDCP22_INTSTAT);

	/* unmask interrupt */
	iowrite32(0x0, esm->link + DWC_HDCP22_INTMASK);
	iowrite32(0x0, esm->link + DWC_HDCP22_INTMUTE);

	return 0;
}

/* HDCP2_STOP implementation */
static long hdcp2Stop(esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	pr_info("%s[%d] \n",__func__, __LINE__);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
	mc_hdcp_clock_enable(esm->dev, 1);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	hdcp2p2Stop(esm->dev);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

#ifdef CONFIG_PM
/* HDCP2_STOP implementation */
static long hdcpBlank(esm_device *esm, void __user *arg)
{
	struct hdcp_ioc_data data;
	int ret = 0;

	if (!esm)
		return -EFAULT;

	if (!esm->dev)
		return -ENOSPC;

	if (copy_from_user(&data, arg, sizeof data) != 0) {
		pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
		return -EFAULT;
	}

	pr_info("%s : blank(mode=%d)\n",__func__, data.status);

	if (esm->parent_dev !=  NULL) {
		switch(data.status)
		{
			case FB_BLANK_POWERDOWN:
			case FB_BLANK_NORMAL:
				pr_info("%s[%d] : blank(mode=%d)\n",__func__, __LINE__, data.status);
				break;
			case FB_BLANK_UNBLANK:
				pr_info("%s[%d] : blank(mode=%d)\n",__func__, __LINE__, data.status);
				break;
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_VSYNC_SUSPEND:
			default:
				pr_info("%s[%d] : blank(mode=%d)\n",__func__, __LINE__, data.status);
				ret = -EINVAL;
		}
	}

	return ret;

}
#endif

/* ESM_IOC_INIT implementation */
static long init(esm_device *esm, void __user *arg)
{
	struct esm_ioc_meminfo info;

	if (!esm)
		return -EMFILE;

	if (copy_from_user(&info, arg, sizeof info) != 0)
		return -EFAULT;


	if (!esm->initialized) {
#ifndef OPTEE_BASE_HDCP
		if (!esm->esm_vaddr && !esm->esm_size)
			return -ENOMEM;

		esm->code_size = (info.code_size < DWC_ESM_CODE_SIZE) ? DWC_ESM_CODE_SIZE : info.code_size;
		esm->data_offs = (info.code_size + 0xFFF) & ~0xFFF;
		esm->data_size = info.data_size ? info.data_size : esm->esm_size - esm->code_size;
		if (esm->esm_size < (esm->data_offs+info.data_size))
			goto err_free;
#endif
		esm->initialized = true;
	}

	return 0;

#ifndef OPTEE_BASE_HDCP
err_free:
	esm->initialized = false;
	esm->code_size = 0;
	esm->data_offs = 0;
	esm->data_size = 0;

	return -ENOMEM;
#endif
}

static long tcc_hdcp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = (struct miscdevice *)f->private_data;
	esm_device *esm = dev_get_drvdata(dev->parent);
	void __user *data = (void __user *)arg;
	char cmd_name[30];
	long ret;

	if (!esm)
		return -EAGAIN;

	switch (cmd) {
	case ESM_IOC_INIT:
		sprintf(cmd_name, "ESM_IOC_INIT");
		ret = init(esm, data);
		break;
	case ESM_IOC_MEMINFO:
		sprintf(cmd_name, "ESM_IOC_MEMINFO");
		ret = get_meminfo(esm, data);
		break;
	case ESM_IOC_READ_HPI:
		sprintf(cmd_name, "ESM_IOC_READ_HPI");
		ret = hpi_read(esm, data);
		break;
	case ESM_IOC_WRITE_HPI:
		sprintf(cmd_name, "ESM_IOC_WRITE_HPI");
		ret = hpi_write(esm, data);
		break;
	case ESM_IOC_LOAD_CODE:
		sprintf(cmd_name, "ESM_IOC_LOAD_CODE");
		ret = load_code(esm, data);
		break;
	case ESM_IOC_WRITE_DATA:
		sprintf(cmd_name, "ESM_IOC_WRITE_DATA");
		ret = write_data(esm, data);
		break;
	case ESM_IOC_READ_DATA:
		sprintf(cmd_name, "ESM_IOC_READ_DATA");
		ret = read_data(esm, data);
		break;
	case ESM_IOC_MEMSET_DATA:
		sprintf(cmd_name, "ESM_IOC_MEMSET_DATA");
		ret = set_data(esm, data);
		break;
	case HDCP1_INIT:
		sprintf(cmd_name, "HDCP1_INIT");
		ret = hdcp1Initialize(esm, data);
		break;
	case HDCP1_START:
		sprintf(cmd_name, "HDCP1_START");
		ret = hdcp1Start(esm, data);
		break;
	case HDCP1_STOP:
		sprintf(cmd_name, "HDCP1_STOP");
		ret = hdcp1Stop(esm, data);
		break;
	case HDCP_GET_STATUS:
		sprintf(cmd_name, "HDCP_GET_STATUS");
		ret = hdcpGetStatus(esm, data);
		break;
	case HDCP2_INIT:
		sprintf(cmd_name, "HDCP2_INIT");
		ret = hdcp2Initialize(esm, data);
		break;
	case HDCP2_STOP:
		sprintf(cmd_name, "HDCP2_STOP");
		ret = hdcp2Stop(esm, data);
		break;
	case HDCP_IOC_BLANK:
		sprintf(cmd_name, "HDCP_IOC_BLANK");
		ret = hdcpBlank(esm, data);
		break;
	default:
		sprintf(cmd_name, "bad cmd");
		ret = -ENOTTY;
	}

	if (ret)
		printk("\x1b[1;33m cmd:0x%x(%s): ret:%ld \x1b[0m\n", cmd, cmd_name, ret);

	return ret;
}

static int
tcc_hdcp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = (struct miscdevice *)file->private_data;
	esm_device *esm = dev_get_drvdata(dev->parent);

	if (!esm->open_cs) {
#ifdef CONFIG_ANDROID
		wake_lock_init(&esm->wakelock, WAKE_LOCK_SUSPEND, "hdcp_wake_lock");
#endif
#ifdef USE_HDMI_PWR_CTRL
		hdmi_api_power_control(1);
#endif
		dev_info(dev->parent, "%s\n", __func__);

		/* clear interrupts */
		iowrite32(0xff, esm->link + DWC_HDCP_INTCLR);
		iowrite32(0xff, esm->link + DWC_HDCP22_INTSTAT);

		/* mask insterrupt */
		iowrite32(0xff, esm->link + DWC_HDCP_INTMASK);
		iowrite32(0xff, esm->link + DWC_HDCP22_INTMASK);
		iowrite32(0xff, esm->link + DWC_HDCP22_INTMUTE);
	}

	esm->open_cs++;
	return 0;
}

static int
tcc_hdcp_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = (struct miscdevice *)file->private_data;
	esm_device *esm = dev_get_drvdata(dev->parent);

	//esm_device *dev = (esm_device *)file->private_data;

	if (!esm->open_cs) {
		dev_err(dev->parent, "%s\n", __func__);
		return -1;
	}

	esm->open_cs--;

	if (!esm->open_cs) {
		/* mask insterrupt */
		iowrite32(0xff, esm->link + DWC_HDCP_INTMASK);
		iowrite32(0xff, esm->link + DWC_HDCP22_INTMASK);
		iowrite32(0xff, esm->link + DWC_HDCP22_INTMUTE);

		dev_info(dev->parent, "%s\n", __func__);
#ifdef USE_HDMI_PWR_CTRL
		hdmi_api_power_control(0);
#endif
#ifdef CONFIG_ANDROID
		wake_lock_destroy(&esm->wakelock);
#endif
	}
	return 0;
}

static ssize_t
tcc_hdcp_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t
tcc_hdcp_write(struct file *file, const char *buf, size_t count, loff_t *f_pos)
{
	return count;
}

static unsigned int tcc_hdcp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	return mask;
}


static const struct file_operations tcc_hdcp_fops =
{
	.owner			= THIS_MODULE,
	.open			= tcc_hdcp_open,
	.release		= tcc_hdcp_release,
	.read			= tcc_hdcp_read,
	.write			= tcc_hdcp_write,
	.unlocked_ioctl		= tcc_hdcp_ioctl,
	.poll			= tcc_hdcp_poll,
};


/**
 * @short misc register routine
 * @param[in] dev pointer to the esm_device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int tcc_hdcp_misc_register(esm_device *dev)
{
	int ret = 0;

	dev->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if(dev->misc == NULL) {
		ret = -1;
	}else {
		dev->misc->minor = MISC_DYNAMIC_MINOR;
		dev->misc->name = "esm";
		dev->misc->fops = &tcc_hdcp_fops;
		dev->misc->parent = dev->parent_dev;
		ret = misc_register(dev->misc);
#ifndef OPTEE_BASE_HDCP
		if (unlikely(ret)) {
			printk(KERN_ERR " %s - failed to register !!!\n", __func__);
			return ret;
		}
#endif
	}

	if(ret < 0) {
		goto end_process;
	}

	dev_set_drvdata(dev->parent_dev, dev);

end_process:

	return ret;
}

/**
 * @short misc deregister routine
 * @param[in] dev pointer to the esm_device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int tcc_hdcp_misc_deregister(esm_device *dev)
{
	if(dev->misc) {
		misc_deregister(dev->misc);
		kfree(dev->misc);
		dev->misc = 0;
	}

	return 0;
}

static int hdcp_probe(struct platform_device *pdev)
{
	esm_device *esm = NULL;
	struct device_node *mem_region;
	struct resource res;
	int ret = 0;

	mem_region = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!mem_region) {
		dev_err(&pdev->dev, "no memory regions\n");
		return -ENOMEM;
	} else {
		ret = of_address_to_resource(mem_region, 0, &res);
		of_node_put(mem_region);
		if (ret || resource_size(&res) == 0) {
			dev_err(&pdev->dev, "failed to obtain protected buffer. (res = %d)\n", ret);
		} else {
			esm = kzalloc(sizeof(esm_device), GFP_KERNEL);
			if(!esm) {
				dev_err(&pdev->dev, "%s:Could not allocated hdcp device driver\n", __func__);
				return -ENOMEM;
			}
			esm->esm_paddr = res.start;
			esm->esm_size = resource_size(&res);
			dev_info(&pdev->dev, "esm: paddr:%p, size:%08x\n", (void *)esm->esm_paddr, esm->esm_size);
			esm->esm_vaddr = (uintptr_t)ioremap(esm->esm_paddr, esm->esm_size);
			if (!esm->esm_vaddr) {
				dev_err(&pdev->dev, "error ioremap protected buffer\n");
				kfree(esm);
				esm = NULL;
				return -ENOMEM;
			}
		}
	}

	if (of_address_to_resource(pdev->dev.of_node, 1, &res)) {
		esm->hdcp22 = 0;
		esm->hpi_paddr = 0;
	}
	else {
		esm->hpi_paddr = res.start;
		esm->hdcp22 = ioremap(res.start, resource_size(&res));
	}

	esm->link = of_iomap(pdev->dev.of_node, 0);
	esm->i2c = of_iomap(pdev->dev.of_node, 2);

	// Update the device node
	esm->parent_dev = &pdev->dev;

	/* hdcp irq shared with hdmi */
	ret = devm_request_threaded_irq(&pdev->dev, platform_get_irq(pdev, 0), hdcp_irq, hdcp_isr,
			IRQF_SHARED, "hdcp", esm);
	if (ret)
		pr_err("Could not register hdcp interrupt\n");

	ret = devm_request_threaded_irq(&pdev->dev, platform_get_irq(pdev, 1), hdcp22_irq, hdcp22_isr,
			IRQF_ONESHOT, "hdcp22", esm);
	if (ret)
		pr_err("Could not register hdcp22 interrupt\n");

	tcc_hdcp_misc_register(esm);
	platform_set_drvdata(pdev, esm);

	dev_info(&pdev->dev, "driver probed. ver: %s\n", HDCP_DRV_VERSION);

	return 0;
}

static int hdcp_remove(struct platform_device *pdev)
{
	esm_device *esm = platform_get_drvdata(pdev);

	if (esm) {
		tcc_hdcp_misc_deregister(esm);
		if (esm->esm_vaddr)
			iounmap((void *)esm->esm_vaddr);
		kfree(esm);
	}

	return 0;
}

static int hdcp_suspend(struct platform_device *pdev, pm_message_t state)
{
	//esm_device *esm = platform_get_drvdata(pdev);
	return 0;
}

static int hdcp_resume(struct platform_device *pdev)
{
	//esm_device *esm= platform_get_drvdata(pdev);
	return 0;
}

static const struct of_device_id hdcp_of_match[] = {
	{ .compatible =	"telechips,dw-hdcp" },
	{ }
};
MODULE_DEVICE_TABLE(of, hdcp_of_match);

static struct platform_driver __refdata dw_hdcp_driver = {
	.driver		= {
		.name	= "telechips,dw-hdcp",
		.of_match_table = hdcp_of_match,
	},
	.probe		= hdcp_probe,
	.remove		= hdcp_remove,
	.suspend	= hdcp_suspend,
	.resume		= hdcp_resume,
};
module_platform_driver(dw_hdcp_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Inc.");
MODULE_VERSION("HDCP_DRV_VERSION");
