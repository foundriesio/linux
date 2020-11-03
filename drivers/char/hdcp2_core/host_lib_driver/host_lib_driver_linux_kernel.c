/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) Telechips Inc.
 * (C) COPYRIGHT 2014 - 2015 SYNOPSYS, INC.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/random.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/of_device.h>
#include <linux/tee_drv.h>
#include "include/host_lib_driver_linux_if.h"
#include "../../hdmi_v2_0/include/hdmi_includes.h"
#include "../../hdmi_v2_0/hdmi_api_lib/include/hdcp/hdcp.h"

#if defined(CONFIG_PM)
#include <linux/pm.h>
#endif

#ifdef CONFIG_ANDROID
#include <linux/wakelock.h>
#endif

#define HDCP_DRV_VERSION "1.1.4"

#define USE_HDMI_PWR_CTRL

#define DWC_PHY_STAT0		(0x3004 * 4)

#define DWC_HDCP_CFG0		(0x5000 * 4)
#define DWC_HDCP_CFG1		(0x5001 * 4)
#define DWC_HDCP_OBS0		(0x5002 * 4)
#define DWC_HDCP_OBS1		(0x5003 * 4)
#define DWC_HDCP_OBS2		(0x5004 * 4)
#define DWC_HDCP_OBS3		(0x5005 * 4)
#define DWC_HDCP_INTCLR		(0x5006 * 4)	/* a_apiintclr */
#define DWC_HDCP_INTSTAT	(0x5007 * 4)	/* a_apiintstat */
#define DWC_HDCP_INTMASK	(0x5008 * 4)	/* a_apiintmask */
#define DWC_HDCP_VIDPOLCFG	(0x5009 * 4)
#define DWC_HDCP_KSVMEMCTRL	(0x5016 * 4)	/* a_ksvmemctrl */
#define DWC_HDCP_REVOC_SIZE_0	(0x52b9 * 4)	/* hdcp_revoc_size_0 */
#define DWC_HDCP_REVOC_SIZE_1	(0x52ba * 4)	/* hdcp_revoc_size_1 */
#define DWC_HDCP_REVOC_LIST	(0x52bb * 4)	/* hdcp_revoc_list */
#define DWC_HDCP_REVOC_MAXSIZE	(5060)

#define DWC_HDCP22_ID		(0x7900 * 4)	/* hdcp22reg_id */
#define DWC_HDCP22_CTRL		(0x7904 * 4)	/* hdcp22reg_ctrl */
#define DWC_HDCP22_CTRL1	(0x7905 * 4)	/* hdcp22reg_ctrl1 */
#define DWC_HDCP22_INTSTS	(0x7908 * 4)	/* hdcp22reg_sts */
#define DWC_HDCP22_INTMASK	(0x790c * 4)	/* hdcp22reg_mask */
#define DWC_HDCP22_INTSTAT	(0x790d * 4)	/* hdcp22reg_stat */
#define DWC_HDCP22_INTMUTE	(0x790e * 4)	/* hdcp22reg_mute */

#define DWC_HPI_SIZE		128
#define DWC_ESM_DATA_SIZE	0x20000UL
#define DWC_ESM_CODE_SIZE	0x30000UL

#define HDCP_UUID { 0x35d3a3b6, 0x8656, 0x4718, \
		    { 0x8f, 0xdb, 0xcd, 0x6a, 0xa9, 0x27, 0xa5, 0x58 } }

// For communicating with TA
#define TEE_CMD_LOAD_ESM_FIRMWARE	0x1
#define TEE_CMD_READ_HPI		0x2
#define TEE_CMD_WRITE_HPI		0x3
#define TEE_CMD_READ_DATA		0x4
#define TEE_CMD_WRITE_DATA		0x5
#define TEE_CMD_SET_DATA		0x6
#define TEE_CMD_GET_CODE_ADDR		0x7
#define TEE_CMD_GET_DATA_ADDR		0x8
#define TEE_CMD_GET_DATA_SIZE		0x9

#define TEE_CMD_SET_TUR_HDCP_TYPE	0xA
#define TEE_CMD_GET_TUR_HDCP_TYPE	0xB
#define TEE_CMD_SET_TUR_HDCP_ENABLE	0xC
#define TEE_CMD_GET_TUR_HDCP_ENABLE	0xD
#define TEE_CMD_SET_OPC_HDCP_VER	0xE
#define TEE_CMD_GET_OPC_HDCP_VER	0xF

#define COMPLETION_DEFAULT_TIME	100

MODULE_PARM_DESC(noverify, "Wipe memory allocations on startup (for debug)");

struct esm_device {
	void __iomem	*link;
	void __iomem	*hdcp22;
	void __iomem	*i2c;

	uint16_t	hdcp_status;
	uint8_t		hpd_status;
	uint8_t		rxsense_status;

	uintptr_t	hpi_paddr;
	dma_addr_t	code_paddr;
	void		*code_vaddr;
	uint32_t	code_size;
	dma_addr_t	data_paddr;
	void		*data_vaddr;
	uint32_t	data_size;

	struct hdmi_tx_dev *dev;
	hdcpParams_t	*params;

	/** Device node */
	struct device	*parent_dev;

	/** Misc Device */
	struct miscdevice *misc;

	/** Device Open Count */
	uint32_t	open_cs;

#ifdef CONFIG_ANDROID
	struct wake_lock	wakelock;
#endif
	bool			initialized;

#ifdef CONFIG_OPTEE
	struct tee_client_context *context;
	struct tee_client_params *tee_params;
#endif
	struct delayed_work	avmute_work;
	struct completion	avmute_completion;
};

#ifdef USE_HDMI_PWR_CTRL
extern void hdmi_api_power_control(int enable);
#endif
extern void hdmi_api_avmute_core(struct hdmi_tx_dev *dev,
				 int enable, uint8_t caller);

static struct esm_device *st_esm;

static void avmute_delay_work(struct work_struct *work)
{
	struct esm_device *esm = container_of(
		(struct delayed_work *)work, struct esm_device, avmute_work);

	if (!esm || !esm->dev)
		return;

	hdmi_api_avmute_core(esm->dev, 0, 1);
}

static void dwc_avmute(struct esm_device *esm, uint32_t en, uint32_t delay)
{
	if (!esm || !esm->dev)
		return;

	cancel_delayed_work(&esm->avmute_work);

	if (en) {
		hdmi_api_avmute_core(esm->dev, 1, 1);
		/* CEA8610F F.3.6 Video Timing Transition
		 * (AVMUTE Recommendation)
		 */
		msleep(90);
	} else {
		schedule_delayed_work(
			&esm->avmute_work, usecs_to_jiffies(delay * 1000));
	}
}

static uint8_t dwc_get_hpd_status(struct esm_device *esm)
{
	if (!esm)
		return 0;

	esm->hpd_status =
		(ioread32(esm->link + DWC_PHY_STAT0) & (1 << 1)) ? 1 : 0;
	return esm->hpd_status;
}

static uint8_t dwc_get_rxsense_status(struct esm_device *esm)
{
	if (!esm)
		return 0;
	esm->rxsense_status = (ioread32(esm->link + DWC_PHY_STAT0) >> 4) & 0xF;
	return esm->rxsense_status;
}

static irqreturn_t hdcp_irq(int irq, void *dev_id)
{
	struct esm_device *esm = dev_id;

	if (esm) {
		if (ioread32(esm->link + DWC_HDCP_INTSTAT)
		    | ioread32(esm->link + DWC_HDCP22_INTSTAT)) {
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
	struct esm_device *esm = dev_id;
	uint32_t hdcp = 0, hdcp22 = 0, mode;
	uint32_t hdcpcfg0, hdcpcfg1;
	uint32_t hdcpobs0, hdcpobs1, hdcpobs2, hdcpobs3;

	if (!esm)
		return IRQ_NONE;

	hdcp = ioread32(esm->link + DWC_HDCP_INTSTAT);
	hdcp22 = ioread32(esm->link + DWC_HDCP22_INTSTAT);

	/* clear interrupt signals */
	iowrite32(hdcp, esm->link + DWC_HDCP_INTCLR);
	iowrite32(hdcp22, esm->link + DWC_HDCP22_INTSTAT);

	if (!dwc_get_hpd_status(esm) || !dwc_get_rxsense_status(esm)) {
		pr_warn("%s: HDCP_IDLE - hpd: %d, rxsense: %d\n", __func__,
			esm->hpd_status, esm->rxsense_status);
		esm->hdcp_status = HDCP_IDLE;
		goto hdcp_isr_exit;
	}

	if (hdcp) {
		/* check hdmi mode */
		hdcpcfg0 = ioread32(esm->link + DWC_HDCP_CFG0);
		hdcpcfg1 = ioread32(esm->link + DWC_HDCP_CFG1);
		hdcpobs0 = ioread32(esm->link + DWC_HDCP_OBS0);
		hdcpobs1 = ioread32(esm->link + DWC_HDCP_OBS1);
		hdcpobs2 = ioread32(esm->link + DWC_HDCP_OBS2);
		hdcpobs3 = ioread32(esm->link + DWC_HDCP_OBS3);

#if (0)
		pr_debug("hdcp:%08x, cfg:%02x %02x, obs: %02x %02x %02x %02x\n",
			 hdcp, hdcpcfg0, hdcpcfg1,
			 hdcpobs0, hdcpobs1, hdcpobs2, hdcpobs3);
#endif

		mode = (hdcpobs3 & (1 << 2)) ? 1 : 0;
		if ((hdcpcfg0 & (1 << 0)) != mode) {
			iowrite32((ioread32(esm->link + DWC_HDCP_CFG0)
				   & ~(1 << 0)) | (mode << 0),
				  esm->link + DWC_HDCP_CFG0);
		}

		if (hdcp & (1 << 2)) {
			/*
			 * - keepouterrorint -
			 * We met an issue what shows the black screen at the
			 * lower resolution. It fixed by changing VSYNC
			 * polarity.
			 */
			uint32_t vidpol =
				ioread32(esm->link + DWC_HDCP_VIDPOLCFG);
			if (vidpol & (1 << 3))
				vidpol &= ~(1 << 3);
			else
				vidpol |= (1 << 3);
			iowrite32(vidpol, esm->link + DWC_HDCP_VIDPOLCFG);
		}
		if (hdcp & (1 << 5)) {
			pr_info("%s: HDCP 1.4 INT_KSV_SHA1_DONE\n", __func__);
			esm->hdcp_status = HDCP_KSV_LIST_READY;
		}
		if (hdcp & (1 << 6)) {
			if (!(hdcpcfg1 & (1 << 1)))
				iowrite32(hdcpcfg1 | (1 << 1),
					  esm->link + DWC_HDCP_CFG1);
			pr_info("%s: HDCP 1.4 Authentication Failed.\n",
				__func__);
			esm->hdcp_status = HDCP_FAILED;
		}
		if (hdcp & (1 << 7)) {
			if (hdcpcfg1 & (1 << 1))
				iowrite32(hdcpcfg1 & ~(1 << 1),
					  esm->link + DWC_HDCP_CFG1);
			pr_info("%s: HDCP 1.4 Authenticated.\n",
				__func__);
			esm->hdcp_status = HDCP_ENGAGED;
			dwc_avmute(esm, 0, 0);
		}
	}

	if (hdcp22) {
#if (0)
		pr_debug("hdcp22:%02x\n", hdcp22);
#endif
		if (hdcp22 & (1 << 0)) {
			esm->hdcp_status = HDCP2_CAPABLE;
			pr_info("%s: HDCP 2.2 Capable\n", __func__);
		}
		if (hdcp22 & (1 << 1)) {
			esm->hdcp_status = HDCP2_NOT_CAPABLE;
			pr_info("%s: HDCP 2.2 Not Capable\n", __func__);
		}
		if (hdcp22 & (1 << 2)) {
			esm->hdcp_status = HDCP2_AUTHENTICATION_LOST;
			pr_info("%s: HDCP 2.2 Authentication Lost\n", __func__);
			if (!esm->hpd_status || !esm->rxsense_status) {
				pr_info("%s: Idle, hpd: %d, rxsense: %d\n",
					__func__, esm->hpd_status,
					esm->rxsense_status);
				esm->hdcp_status = HDCP_IDLE;
			}
			dwc_avmute(esm, 0, 500);
		}
		if (hdcp22 & (1 << 3)) {
			esm->hdcp_status = HDCP2_AUTHENTICATED;
			pr_info("%s: HDCP 2.2 Authenticated\n", __func__);
			dwc_avmute(esm, 0, 0);
		}
		if (hdcp22 & (1 << 4)) {
			esm->hdcp_status = HDCP2_AUTHENTICATION_FAIL;
			pr_info("%s: HDCP 2.2 Authentication Failed\n",
				 __func__);
			if (!esm->hpd_status || !esm->rxsense_status) {
				pr_info("%s: Idle, hpd: %d, rxsense: %d\n",
					__func__, esm->hpd_status,
					esm->rxsense_status);
				esm->hdcp_status = HDCP_IDLE;
			}
			dwc_avmute(esm, 0, 500);
		}
		if (hdcp22 & (1 << 5)) {
			pr_info("%s: HDCP 2.2 Decrypted Change\n", __func__);
			/**
			 * Decrypted change status occurs even after
			 * authentication failed. Since customer knows value of
			 * 12 as a success, only change the value when the
			 * previous is a success.
			 */
			if (esm->hdcp_status == HDCP2_AUTHENTICATED)
				esm->hdcp_status = HDCP2_DECRYPTED_CHG;

			complete(&esm->avmute_completion);
		}
	}

hdcp_isr_exit:
	esm->dev->hdcp_status = (int)esm->hdcp_status;

	/* unmask insterrupt */
	iowrite32(0, esm->link + DWC_HDCP_INTMASK);
	iowrite32(0, esm->link + DWC_HDCP22_INTMASK);
	iowrite32(0, esm->link + DWC_HDCP22_INTMUTE);

	if (esm->hdcp_status == HDCP_IDLE)
		dwc_avmute(esm, 0, 0);

#ifdef CONFIG_OPTEE
	if (esm->context)
		tee_client_execute_command(esm->context, NULL,
					   TEE_CMD_SET_OPC_HDCP_VER);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t hdcp22_irq(int irq, void *dev_id)
{
	struct esm_device *esm = (struct esm_device *)dev_id;
	uint32_t sts;

	if (!esm)
		return IRQ_HANDLED;

	sts = ioread32(esm->hdcp22 + 0x24);
#if (0)
	pr_debug("%s sts:0x%08x oob:0x%08x, err:0x%08x\n", __func__, sts,
		 ioread32(esm->hdcp22 + 0x5c), ioread32(esm->hdcp22 + 0x60));
#endif
	iowrite32(~sts, esm->hdcp22 + 0x20);

	//	if (ioread32(esm->hdcp22 + 0x24))
	//		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

static irqreturn_t hdcp22_isr(int irq, void *dev_id)
{
	struct esm_device *esm = (struct esm_device *)dev_id;
	uint32_t sts;

	sts = ioread32(esm->hdcp22 + 0x24);
	pr_debug("%s sts:0x%08x oob:0x%08x, err:0x%08x\n", __func__, sts,
		 ioread32(esm->hdcp22 + 0x5c), ioread32(esm->hdcp22 + 0x60));

	return IRQ_HANDLED;
}

/* ESM_IOC_MEMINFO implementation */
static long get_meminfo(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_meminfo info = {
		.hpi_base = (uint32_t)esm->hpi_paddr,
		.code_base = (uint32_t)esm->code_paddr,
		.code_size = esm->code_size,
		.data_base = (uint32_t)esm->data_paddr,
		.data_size = esm->data_size,
	};

	if (copy_to_user(arg, &info, sizeof(info)) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_LOAD_CODE implementation */
static long load_code(struct esm_device *esm, struct esm_ioc_code __user *arg)
{
	struct esm_ioc_code head;
	int res = 0;

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (head.len > esm->code_size)
		return -ENOSPC;

	if (head.len) {
		if (copy_from_user(esm->code_vaddr, &arg->data, head.len) != 0)
			return -EFAULT;
	} else {
#ifdef CONFIG_OPTEE
		if (esm->context) {
			memset(esm->tee_params, 0x0,
			       sizeof(struct tee_client_params));
			esm->tee_params->params[0].type =
				TEE_CLIENT_PARAM_VALUE_OUT;
			res = tee_client_execute_command(esm->context,
				esm->tee_params, TEE_CMD_LOAD_ESM_FIRMWARE);
			if (res)
				return -24; /* ESM_HL_ESM_FIRMWARE_NOT_EXIST  */

			esm->code_paddr = (dma_addr_t)esm->tee_params->params[0]
					  .tee_client_value.a;
			return res;
		}
#endif
		// printk("Code size of ESM is zero.\n");
		res = -EFAULT;
	}

	return res;
}

/* ESM_IOC_WRITE_DATA implementation */
static long write_data(struct esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_from_user(esm->data_vaddr + head.offset, &arg->data, head.len)
	    != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_READ_DATA implementation */
static long read_data(struct esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_to_user(&arg->data, esm->data_vaddr + head.offset, head.len)
	    != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_MEMSET_DATA implementation */
static long set_data(struct esm_device *esm, void __user *arg)
{
	union {
		struct esm_ioc_data data;
		unsigned char buf[sizeof(struct esm_ioc_data) + 1];
	} u;

	if (copy_from_user(&u.data, arg, sizeof(u.buf)) != 0)
		return -EFAULT;

	if (esm->data_size < u.data.len)
		return -ENOSPC;
	if (esm->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset(esm->data_vaddr + u.data.offset, u.data.data[0], u.data.len);

	return 0;
}

/* ESM_IOC_READ_HPI implementation */
static long hpi_read(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;
	int res = 0;

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= DWC_HPI_SIZE)
		return -EINVAL;

#ifdef CONFIG_OPTEE
	if (esm->context) {
		memset(esm->tee_params, 0x0, sizeof(struct tee_client_params));
		esm->tee_params->params[0].type = TEE_CLIENT_PARAM_VALUE_INOUT;
		esm->tee_params->params[0].tee_client_value.a = reg.value;
		esm->tee_params->params[0].tee_client_value.b = reg.offset;
		res = tee_client_execute_command(
			esm->context, esm->tee_params, TEE_CMD_READ_HPI);
		if (res)
			return res;

		reg.value = esm->tee_params->params[0].tee_client_value.a;
	} else
#endif
		reg.value = ioread32(esm->hdcp22 + reg.offset);

	if (copy_to_user(arg, &reg, sizeof(reg)) != 0)
		return -EFAULT;

	return res;
}

/* ESM_IOC_WRITE_HPI implementation */
static long hpi_write(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;
	int res = 0;

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= DWC_HPI_SIZE)
		return -EINVAL;

#ifdef CONFIG_OPTEE
	if (esm->context) {
		memset(esm->tee_params, 0x0, sizeof(struct tee_client_params));
		esm->tee_params->params[0].type = TEE_CLIENT_PARAM_VALUE_IN;
		esm->tee_params->params[0].tee_client_value.a = reg.value;
		esm->tee_params->params[0].tee_client_value.b = reg.offset;
		res = tee_client_execute_command(
			esm->context, esm->tee_params, TEE_CMD_WRITE_HPI);
		if (res)
			return res;
	} else
#endif
		iowrite32(reg.value, esm->hdcp22 + reg.offset);

	return res;
}

/* HDCP1_INIT implementation */
static long hdcp1Initialize(struct esm_device *esm, void __user *arg)
{
	struct dwc_hdmi_hdcp_data hdcpData;

	if (copy_from_user(&hdcpData, arg, sizeof(hdcpData)) != 0)
		return -EFAULT;

	esm->params = &hdcpData.hdcpParam;
	if (!esm->dev)
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
static long hdcp1Start(struct esm_device *esm, void __user *arg)
{
	struct dwc_hdmi_hdcp_data hdcpData;

	if (copy_from_user(&hdcpData, arg, sizeof(hdcpData)) != 0)
		return -EFAULT;

	if (!esm->dev)
		return -ENOSPC;

	dwc_avmute(esm, 1, 0);
	esm->params = &hdcpData.hdcpParam;
	mutex_lock(&esm->dev->mutex);
	mc_hdcp_clock_enable(esm->dev, 0);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	hdcp1p4Start(esm->dev, esm->params);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP1_STOP implementation */
static long hdcp1Stop(struct esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	dwc_avmute(esm, 1, 0);

	/* set revoc sizt to 0 */
	iowrite32(1, esm->link + DWC_HDCP_KSVMEMCTRL);
	iowrite32(0, esm->link + DWC_HDCP_REVOC_SIZE_0);
	iowrite32(0, esm->link + DWC_HDCP_REVOC_SIZE_1);
	iowrite32(0, esm->link + DWC_HDCP_KSVMEMCTRL);

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
	hdcp1p4Stop(esm->dev);
	mc_hdcp_clock_enable(esm->dev, 1);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	esm->dev->hdcp_status = esm->hdcp_status = HDCP_IDLE;
	mutex_unlock(&esm->dev->mutex);
	dwc_avmute(esm, 0, 1000);

#ifdef CONFIG_OPTEE
	if (esm->context)
		tee_client_execute_command(
			esm->context, NULL, TEE_CMD_SET_OPC_HDCP_VER);
#endif

	return 0;
}

static long hdcpSetSRM(struct esm_device *esm, struct esm_ioc_code __user *arg)
{
	struct esm_ioc_code head;
	long ret = 0;
	char *srm = NULL;
	uint32_t cnt, revoc_size;

	if (!esm || !esm->dev)
		return -EFAULT;

	ret = copy_from_user(&head, arg, sizeof(head));
	if (ret)
		goto exit;

	revoc_size = head.len / 5;

	iowrite32(1, esm->link + DWC_HDCP_KSVMEMCTRL);
	iowrite32((revoc_size)&0xFF, esm->link + DWC_HDCP_REVOC_SIZE_0);
	iowrite32(
		((revoc_size) >> 8) & 0xFF, esm->link + DWC_HDCP_REVOC_SIZE_1);

	if (!revoc_size)
		goto exit;

	srm = kmalloc(head.len, GFP_KERNEL);
	if (!srm) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = copy_from_user(srm, &arg->data, head.len);
	if (ret)
		goto exit;

	for (cnt = 0; cnt < head.len; cnt++)
		iowrite32(srm[cnt], esm->link + DWC_HDCP_REVOC_LIST + cnt * 4);

exit:
	iowrite32(0, esm->link + DWC_HDCP_KSVMEMCTRL);
	kfree(srm);
	return ret;
}

/* HDCP_GET_STATUS implementation */
static long hdcpGetStatus(struct esm_device *esm, void __user *arg)
{
	struct hdcp_ioc_data data;

	if (copy_from_user(&data, arg, sizeof(data)) != 0)
		return -EFAULT;

	data.status = esm->hdcp_status;

#ifdef CONFIG_OPTEE
	if (esm->context)
		tee_client_execute_command(
			esm->context, NULL, TEE_CMD_SET_OPC_HDCP_VER);
#endif

	if (copy_to_user(arg, &data, sizeof(data)) != 0)
		return -EFAULT;

	return 0;
}

/* HDCP2_INIT implementation */
static long hdcp2Initialize(struct esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		esm->dev = dwc_hdmi_api_get_dev();

	if (!esm->dev)
		return -ENOSPC;

	dwc_avmute(esm, 1, 0);

	// iowrite32(0xffffffff, esm->hdcp22 + 0x20);
	// iowrite32(0xffffffff, esm->hdcp22 + 0x24);

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
static long hdcp2Stop(struct esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	dwc_avmute(esm, 1, 0);
	pr_info("%s[%d]\n", __func__, __LINE__);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
	mc_hdcp_clock_enable(esm->dev, 1);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	hdcp2p2Stop(esm->dev);
	mutex_unlock(&esm->dev->mutex);
	dwc_avmute(esm, 0, 1000);

	// iowrite32(0x0, esm->hdcp22 + 0x20);
	// iowrite32(0xffffffff, esm->hdcp22 + 0x24);

#ifdef CONFIG_OPTEE
	if (esm->context)
		tee_client_execute_command(
			esm->context, NULL, TEE_CMD_SET_OPC_HDCP_VER);
#endif

	return 0;
}

/* ESM_IOC_INIT implementation */
static long init(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_meminfo info;

	if (!esm)
		return -EMFILE;

	if (copy_from_user(&info, arg, sizeof(info)) != 0)
		return -EFAULT;

	if (!esm->initialized) {
		if (!esm->code_vaddr || !esm->data_vaddr)
			return -ENOMEM;

		if ((info.code_size > DWC_ESM_CODE_SIZE)
		    || (info.data_size > DWC_ESM_CODE_SIZE))
			return -ENOSPC;

		esm->code_size = info.code_size;
		esm->data_size = info.data_size;

		esm->initialized = true;
	}

	return 0;
}

static long tcc_hdcp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = (struct miscdevice *)f->private_data;
	struct esm_device *esm = dev_get_drvdata(dev->parent);
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
	case HDCP_SET_SRM:
		sprintf(cmd_name, "HDCP_SET_SRM");
		ret = hdcpSetSRM(esm, data);
		break;
	default:
		sprintf(cmd_name, "bad cmd");
		ret = -ENOTTY;
	}

	if (ret && (cmd != ESM_IOC_LOAD_CODE))
		pr_err("cmd:0x%x(%s): ret:%ld\n", cmd, cmd_name, ret);

	return ret;
}

static int tcc_hdcp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = (struct miscdevice *)file->private_data;
	struct esm_device *esm = dev_get_drvdata(dev->parent);
#ifdef CONFIG_OPTEE
	struct tee_client_uuid uuid = HDCP_UUID;
	int ret;
#endif

	if (!esm->dev)
		esm->dev = dwc_hdmi_api_get_dev();

	if (!esm->open_cs) {
#ifdef CONFIG_ANDROID
		wake_lock_init(
			&esm->wakelock, WAKE_LOCK_SUSPEND, "hdcp_wake_lock");
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

#ifdef CONFIG_OPTEE
		esm->tee_params =
			kzalloc(sizeof(struct tee_client_params), GFP_KERNEL);
		if (esm->tee_params) {
			ret = tee_client_open_ta(&uuid, NULL, &esm->context);
			if (ret) {
#if (0)
				pr_err("%s: hdcp ta open failed ret:0x%x\n",
					__func__, ret);
#endif
				esm->context = NULL;
				kfree(esm->tee_params);
				esm->tee_params = NULL;
			}
		}
#endif
	}

	esm->open_cs++;
	return 0;
}

static int tcc_hdcp_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = (struct miscdevice *)file->private_data;
	struct esm_device *esm = dev_get_drvdata(dev->parent);

	// struct esm_device *dev = (struct esm_device *)file->private_data;

	if (!esm->open_cs) {
		dev_err(dev->parent, "%s\n", __func__);
		return -1;
	}

	esm->open_cs--;

	if (!esm->open_cs) {
#ifdef CONFIG_OPTEE
		if (esm->context) {
			kfree(esm->tee_params);
			esm->tee_params = NULL;
			tee_client_close_ta(esm->context);
			esm->context = NULL;
		}
#endif
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
	pr_info("avmute avunmute bypassen bypassdis\n");
	return 0;
}

static ssize_t
tcc_hdcp_write(struct file *file, const char *buf, size_t count, loff_t *f_pos)
{
	struct miscdevice *dev = (struct miscdevice *)file->private_data;
	struct esm_device *esm = dev_get_drvdata(dev->parent);

	if (!strncmp(buf, "avmute", 6))
		dwc_avmute(esm, 1, 0);
	else if (!strncmp(buf, "avunmute", 8))
		dwc_avmute(esm, 0, 0);
	return count;
}

static unsigned int tcc_hdcp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	return mask;
}

static const struct file_operations tcc_hdcp_fops = {
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
int tcc_hdcp_misc_register(struct esm_device *dev)
{
	int ret = 0;

	dev->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (dev->misc == NULL) {
		ret = -1;
	} else {
		dev->misc->minor = MISC_DYNAMIC_MINOR;
		dev->misc->name = "esm";
		dev->misc->fops = &tcc_hdcp_fops;
		dev->misc->parent = dev->parent_dev;
		ret = misc_register(dev->misc);
		if (unlikely(ret)) {
			pr_err("%s - failed to register !!!\n", __func__);
			return ret;
		}
	}

	if (ret < 0)
		goto end_process;

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
int tcc_hdcp_misc_deregister(struct esm_device *dev)
{
	if (dev->misc) {
		misc_deregister(dev->misc);
		kfree(dev->misc);
		dev->misc = 0;
	}

	return 0;
}

static int hdcp_probe(struct platform_device *pdev)
{
	struct esm_device *esm = NULL;
	struct resource res;
	int ret = 0;

	esm = kzalloc(sizeof(struct esm_device), GFP_KERNEL);
	if (!esm)
		return -ENOMEM;

	esm->code_vaddr = dma_alloc_coherent(
		&pdev->dev, DWC_ESM_CODE_SIZE, &esm->code_paddr, GFP_KERNEL);
	if (esm->code_vaddr) {
		esm->data_vaddr = dma_alloc_coherent(&pdev->dev,
			DWC_ESM_DATA_SIZE, &esm->data_paddr, GFP_KERNEL);
		if (!esm->data_vaddr) {
			dev_err(&pdev->dev, "error alloc memories for DATA\n");
			esm->data_vaddr = NULL;
			esm->data_paddr = (dma_addr_t)0;
			esm->code_vaddr = NULL;
			esm->code_paddr = (dma_addr_t)0;
			dma_free_coherent(&pdev->dev, DWC_ESM_CODE_SIZE,
				esm->code_vaddr, esm->code_paddr);
		}
	} else {
		dev_err(&pdev->dev, "error alloc memories for CODE\n");
		esm->code_vaddr = NULL;
		esm->code_paddr = (dma_addr_t)0;
	}

	if (of_address_to_resource(pdev->dev.of_node, 1, &res)) {
		esm->hdcp22 = 0;
		esm->hpi_paddr = 0;
	} else {
		esm->hpi_paddr = res.start;
		esm->hdcp22 = ioremap(res.start, resource_size(&res));
	}

	esm->link = of_iomap(pdev->dev.of_node, 0);
	esm->i2c = of_iomap(pdev->dev.of_node, 2);

	// Update the device node
	esm->parent_dev = &pdev->dev;

	/* hdcp irq shared with hdmi */
	ret = devm_request_threaded_irq(&pdev->dev, platform_get_irq(pdev, 0),
		hdcp_irq, hdcp_isr, IRQF_SHARED, "hdcp", esm);
	if (ret)
		pr_err("Could not register hdcp interrupt\n");

	ret = devm_request_threaded_irq(&pdev->dev, platform_get_irq(pdev, 1),
		hdcp22_irq, hdcp22_isr, IRQF_ONESHOT, "hdcp22", esm);
	if (ret)
		pr_err("Could not register hdcp22 interrupt\n");

	tcc_hdcp_misc_register(esm);
	platform_set_drvdata(pdev, esm);

	INIT_DELAYED_WORK(&esm->avmute_work, avmute_delay_work);
	init_completion(&esm->avmute_completion);
	st_esm = esm;

	dev_info(&pdev->dev, "driver probed. ver: %s\n", HDCP_DRV_VERSION);

	return 0;
}

static int hdcp_remove(struct platform_device *pdev)
{
	struct esm_device *esm = platform_get_drvdata(pdev);

	if (esm) {
		tcc_hdcp_misc_deregister(esm);
		if (esm->code_vaddr)
			dma_free_coherent(&pdev->dev, DWC_ESM_CODE_SIZE,
				esm->code_vaddr, esm->code_paddr);
		if (esm->data_vaddr)
			dma_free_coherent(&pdev->dev, DWC_ESM_DATA_SIZE,
				esm->data_vaddr, esm->data_paddr);
		kfree(esm);
	}

	return 0;
}

static int hdcp_suspend(struct platform_device *pdev, pm_message_t state)
{
	// struct esm_device *esm = platform_get_drvdata(pdev);
	return 0;
}

static int hdcp_resume(struct platform_device *pdev)
{
	// struct esm_device *esm= platform_get_drvdata(pdev);
	return 0;
}

void dwc_hdcp_avmute(int mute)
{
	int cnt;

	if (!st_esm)
		return;

	if (st_esm->open_cs <= 0)
		return;

	if (mute) {
		iowrite32(ioread32(st_esm->link + DWC_HDCP22_CTRL1) | 0x3,
			st_esm->link + DWC_HDCP22_CTRL1);
		iowrite32(ioread32(st_esm->link + DWC_HDCP_CFG1) | (1 << 1),
			st_esm->link + DWC_HDCP_CFG1);
		if (ioread32(st_esm->link + DWC_HDCP22_INTSTS) & (1 << 2)) {
			reinit_completion(&st_esm->avmute_completion);
			wait_for_completion_timeout(&st_esm->avmute_completion,
				msecs_to_jiffies(COMPLETION_DEFAULT_TIME));
		} else {
			cnt = 10;
			do {
				if ((ioread32(st_esm->link + DWC_HDCP_OBS2)
					      & ((1 << 2) | (1 << 1))) == 0)
					break;
			} while (cnt--);
		}
	} else {
		switch (st_esm->hdcp_status) {
		case HDCP_ENGAGED:
			iowrite32(ioread32(st_esm->link + DWC_HDCP_CFG1)
				& ~(1 << 1), st_esm->link + DWC_HDCP_CFG1);
			break;
		case HDCP2_AUTHENTICATED:
		case HDCP2_DECRYPTED_CHG:
			iowrite32(ioread32(st_esm->link + DWC_HDCP22_CTRL1)
				& ~0x3, st_esm->link + DWC_HDCP22_CTRL1);
			break;
		default:
			break;
		}
	}
}
EXPORT_SYMBOL(dwc_hdcp_avmute);

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

MODULE_VERSION(HDCP_DRV_VERSION);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Synopsys DesignWare HDCP driver");
