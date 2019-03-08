/*
 * tcc_nsk_sc.c
 *
 * Author:  <linux@telechips.com>
 * Created: March 24, 2016
 * Description: TCC SmartCard driver
 *
 * Copyright (C) 20010-2011 Telechips
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/tcc_nsk_sc_ioctl.h>
//#include <soc/tcc/tcc_nsk_sc_ioctl.h>

#include <asm/io.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
//#include <asm/mach/map.h>

#include "tca_nsk_sc.h"

#include <linux/gpio.h>

#define TCC_NSK_SC_DEVICE_NAME "tcc-nsk-sc"
#define MAJOR_ID 235
#define MINOR_ID 1

//#define DEBUG_TCC_NSK_SC
#ifdef DEBUG_TCC_NSK_SC
#undef dprintk
#define dprintk(msg...) printk("tcc_nsk_sc: " msg);
#undef eprintk
#define eprintk(msg...) printk("tcc_nsk_sc: err: " msg);
#else
#undef dprintk
#define dprintk(msg...)
#undef eprintk
#define eprintk(msg...)
#endif

static DEFINE_MUTEX(g_hNSKSCMutex);

static int g_iNSKSCState = TCC_NSK_SC_STATE_NONE;

//---------------------------------------------------------------------------
// DEFINITION OF LOCAL FUNCTIONS
//---------------------------------------------------------------------------
static int tcc_nsk_sc_enable(unsigned uEnable)
{
	dprintk("%s(%d)\n", __func__, uEnable);

	if (uEnable) {
		if (g_iNSKSCState != TCC_NSK_SC_STATE_OPEN) {
			eprintk("%s : invalid state(%d)\n", __func__, g_iNSKSCState);
			return TCC_NSK_SC_ERROR_INVALID_STATE;
		}

		tca_nsk_sc_enable();
		g_iNSKSCState = TCC_NSK_SC_STATE_ENABLE;
	} else {
		tca_nsk_sc_disable();
		g_iNSKSCState = TCC_NSK_SC_STATE_OPEN;
	}

	return TCC_NSK_SC_SUCCESS;
}

static int tcc_nsk_sc_activate(unsigned uActivate)
{
	dprintk("%s(%d)\n", __func__, uActivate);

	if (uActivate) {
		if (g_iNSKSCState != TCC_NSK_SC_STATE_ENABLE) {
			eprintk("%s : invalid state(%d)\n", __func__, g_iNSKSCState);
			return TCC_NSK_SC_ERROR_INVALID_STATE;
		}

		tca_nsk_sc_activate();
		g_iNSKSCState = TCC_NSK_SC_STATE_ACTIVATE;
	} else {
		if (g_iNSKSCState != TCC_NSK_SC_STATE_ACTIVATE) {
			eprintk("%s : invalid state(%d)\n", __func__, g_iNSKSCState);
			return TCC_NSK_SC_ERROR_INVALID_STATE;
		}

		tca_nsk_sc_deactivate();
		g_iNSKSCState = TCC_NSK_SC_STATE_ENABLE;
	}

	return TCC_NSK_SC_SUCCESS;
}

static int tcc_nsk_sc_reset(unsigned char *pATR, unsigned *puATRLen)
{
	int iRet;

	dprintk("%s\n", __func__);

	if (g_iNSKSCState == TCC_NSK_SC_STATE_OPEN) {
		tca_nsk_sc_enable();
		g_iNSKSCState = TCC_NSK_SC_STATE_ENABLE;
	}

	if (g_iNSKSCState == TCC_NSK_SC_STATE_ENABLE) {
		tca_nsk_sc_activate();
		g_iNSKSCState = TCC_NSK_SC_STATE_ACTIVATE;
	}

	// msleep(100);

	iRet = tca_nsk_sc_warm_reset(pATR, puATRLen);
	if (iRet) {
		eprintk("%s(%d)\n", __func__, iRet);
		return iRet;
	}

	return iRet;
}

static int tcc_nsk_sc_select_vcc_level(unsigned uVccLevel)
{
	int iRet;

	dprintk("%s(%d)\n", __func__, uVccLevel);

	if (uVccLevel != TCC_NSK_SC_VOLTAGE_LEVEL_3V && uVccLevel != TCC_NSK_SC_VOLTAGE_LEVEL_5V) {
		eprintk("%s : invalid param(%d)\n", __func__, uVccLevel);
		return TCC_NSK_SC_ERROR_INVALID_PARAM;
	}

	iRet = tca_nsk_sc_select_vcc_level(uVccLevel);
	if (iRet) {
		eprintk("%s(%d)\n", __func__, iRet);
		return iRet;
	}

	return iRet;
}

static unsigned tcc_nsk_sc_detect_card(void)
{
	int iPrecense;

	//	dprintk("%s\n", __func__);

	iPrecense = tca_nsk_sc_detect_card();
	if (!iPrecense) {
		if (g_iNSKSCState == TCC_NSK_SC_STATE_ACTIVATE) {
			tca_nsk_sc_deactivate();
			g_iNSKSCState = TCC_NSK_SC_STATE_ENABLE;
		}

		if (g_iNSKSCState == TCC_NSK_SC_STATE_ENABLE) {
			tca_nsk_sc_disable();
			g_iNSKSCState = TCC_NSK_SC_STATE_OPEN;
		}
	}

	return iPrecense;
}

static void tcc_nsk_sc_set_config(stTCC_NSK_SC_CONFIG stSCConfig)
{
	dprintk(
		"%s(%d, %d, %d, %d, %d, %d, %d, %d, %d)\n", __func__, stSCConfig.uiProtocol,
		stSCConfig.uiConvention, stSCConfig.uiParity, stSCConfig.uiErrorSignal,
		stSCConfig.uiFlowControl, stSCConfig.uiClock, stSCConfig.uiBaudrate, stSCConfig.uiWaitTime,
		stSCConfig.uiGuardTime);

	tca_nsk_sc_set_config(stSCConfig);

	return;
}

static void tcc_nsk_sc_get_config(stTCC_NSK_SC_CONFIG *pstSCConfig)
{
	dprintk("%s\n", __func__);

	tca_nsk_sc_get_config(pstSCConfig);

	dprintk(
		"%s(%d, %d, %d, %d, %d, %d, %d, %d, %d)\n", __func__, pstSCConfig->uiProtocol,
		pstSCConfig->uiConvention, pstSCConfig->uiParity, pstSCConfig->uiErrorSignal,
		pstSCConfig->uiFlowControl, pstSCConfig->uiClock, pstSCConfig->uiBaudrate,
		pstSCConfig->uiWaitTime, pstSCConfig->uiGuardTime);

	return;
}

static unsigned tcc_nsk_sc_set_io_pin_config(unsigned uPinMask)
{
	int iRet;

	dprintk("%s(%d)\n", __func__, uPinMask);

	iRet = tca_nsk_sc_set_io_pin_config(uPinMask);
	if (iRet) {
		eprintk("%s(%d)\n", __func__, iRet);
	}

	return iRet;
}

static void tcc_nsk_sc_get_io_pin_config(unsigned *puPinMask)
{
	dprintk("%s\n", __func__);

	tca_nsk_sc_get_io_pin_config(puPinMask);

	dprintk("%s(%d)\n", __func__, *puPinMask);

	return;
}

static int tcc_nsk_sc_send_receive(stTCC_NSK_SC_BUF stSCBuf)
{
	int iRet;

	if (g_iNSKSCState != TCC_NSK_SC_STATE_ACTIVATE) {
		eprintk("%s : invalid state(%d)\n", __func__, g_iNSKSCState);
		return TCC_NSK_SC_ERROR_INVALID_STATE;
	}

	iRet = tca_nsk_sc_send_receive(
		stSCBuf.pucTxBuf, stSCBuf.uiTxBufLen, stSCBuf.pucRxBuf, stSCBuf.puiRxBufLen,
		stSCBuf.uiDirection, stSCBuf.uiTimeOut);
	if (iRet) {
		eprintk("%s(%d)\n", __func__, iRet);
		return iRet;
	}

	return iRet;
}

//---------------------------------------------------------------------------
// DEFINITION OF FILE OPERATION FUNCTIONS
//---------------------------------------------------------------------------
static int tcc_nsk_sc_open(struct inode *inode, struct file *filp)
{
	int iRet = 0;

	mutex_lock(&g_hNSKSCMutex);

	if (g_iNSKSCState != TCC_NSK_SC_STATE_NONE) {
		eprintk("%s : invalid state(%d)\n", __func__, g_iNSKSCState);
		mutex_unlock(&g_hNSKSCMutex);
		return TCC_NSK_SC_ERROR_INVALID_STATE;
	}

	iRet = tca_nsk_sc_open(inode, filp);
	if (iRet) {
		eprintk("%s(%d)\n", __func__, iRet);
		mutex_unlock(&g_hNSKSCMutex);
		return iRet;
	}

	g_iNSKSCState = TCC_NSK_SC_STATE_OPEN;

	mutex_unlock(&g_hNSKSCMutex);

	return iRet;
}

static ssize_t tcc_nsk_sc_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	eprintk("%s : unsupport function\n", __func__);

	return 0;
}

static ssize_t tcc_nsk_sc_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	eprintk("%s : unsupport function\n", __func__);

	return 0;
}

static long tcc_nsk_sc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int iRet = TCC_NSK_SC_SUCCESS;

	mutex_lock(&g_hNSKSCMutex);

	// dprintk("%s(%d)\n", __func__, cmd);

	if (g_iNSKSCState == TCC_NSK_SC_STATE_NONE) {
		eprintk("%s : invalid state(%d)\n", __func__, g_iNSKSCState);
		mutex_unlock(&g_hNSKSCMutex);
		return TCC_NSK_SC_ERROR_INVALID_STATE;
	}

	switch (cmd) {
	case TCC_NSK_SC_IOCTL_ENABLE: {
		unsigned uEnable;
		if (copy_from_user((void *)&uEnable, (const void *)arg, sizeof(unsigned))) {
			eprintk("%s : copy_from_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
		iRet = tcc_nsk_sc_enable(uEnable);
	} break;

	case TCC_NSK_SC_IOCTL_ACTIVATE: {
		unsigned uActivate;
		if (copy_from_user((void *)&uActivate, (const void *)arg, sizeof(unsigned))) {
			eprintk("%s : copy_from_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
		iRet = tcc_nsk_sc_activate(uActivate);
	} break;

	case TCC_NSK_SC_IOCTL_RESET: {
		stTCC_NSK_SC_BUF stSCBuf;
		if (copy_from_user((void *)&stSCBuf, (const void *)arg, sizeof(stTCC_NSK_SC_BUF))) {
			eprintk("%s : copy_from_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}

		iRet = tcc_nsk_sc_reset(stSCBuf.pucRxBuf, stSCBuf.puiRxBufLen);
	} break;

	case TCC_NSK_SC_IOCTL_SET_VCC_LEVEL: {
		unsigned uVccLevel;
		if (copy_from_user((void *)&uVccLevel, (const void *)arg, sizeof(unsigned))) {
			eprintk("%s : copy_from_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
		iRet = tcc_nsk_sc_select_vcc_level(uVccLevel);
	} break;

	case TCC_NSK_SC_IOCTL_DETECT_CARD: {
		unsigned uPresence;
		uPresence = tcc_nsk_sc_detect_card();
		if (copy_to_user((unsigned *)arg, &uPresence, sizeof(unsigned))) {
			eprintk("%s : copy_to_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
	} break;

	case TCC_NSK_SC_IOCTL_SET_CONFIG: {
		stTCC_NSK_SC_CONFIG stSCConfig;
		if (copy_from_user((void *)&stSCConfig, (const void *)arg, sizeof(stTCC_NSK_SC_CONFIG))) {
			eprintk("%s : copy_from_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
		tcc_nsk_sc_set_config(stSCConfig);
	} break;

	case TCC_NSK_SC_IOCTL_GET_CONFIG: {
		stTCC_NSK_SC_CONFIG stSCConfig;
		tcc_nsk_sc_get_config(&stSCConfig);
		if (copy_to_user((unsigned *)arg, &stSCConfig, sizeof(stTCC_NSK_SC_CONFIG))) {
			eprintk("%s : copy_to_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
	} break;

	case TCC_NSK_SC_IOCTL_SET_IO_PIN_CONFIG: {
		unsigned uPinMask;
		if (copy_from_user((void *)&uPinMask, (const void *)arg, sizeof(unsigned))) {
			eprintk("%s : copy_from_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
		iRet = tcc_nsk_sc_set_io_pin_config(uPinMask);
	} break;

	case TCC_NSK_SC_IOCTL_GET_IO_PIN_CONFIG: {
		unsigned uPinMask;
		tcc_nsk_sc_get_io_pin_config(&uPinMask);
		if (copy_to_user((unsigned *)arg, &uPinMask, sizeof(unsigned))) {
			eprintk("%s : copy_to_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
	} break;

	case TCC_NSK_SC_IOCTL_SEND_RCV: {
		stTCC_NSK_SC_BUF stSCBuf;
		if (copy_from_user((void *)&stSCBuf, (const void *)arg, sizeof(stTCC_NSK_SC_BUF))) {
			eprintk("%s : copy_from_user failed\n", __func__);
			mutex_unlock(&g_hNSKSCMutex);
			return TCC_NSK_SC_ERROR_UNKNOWN;
		}
		iRet = tcc_nsk_sc_send_receive(stSCBuf);
	} break;

	default: {
		eprintk("%s: unkown cmd(%d)\n", __func__, cmd);
		iRet = -1;
	} break;
	}

	mutex_unlock(&g_hNSKSCMutex);

	return iRet;
}

static int tcc_nsk_sc_close(struct inode *inode, struct file *file)
{
	mutex_lock(&g_hNSKSCMutex);

	if (g_iNSKSCState == TCC_NSK_SC_STATE_NONE) {
		eprintk("%s : invalid state(%d)\n", __func__, g_iNSKSCState);
		mutex_unlock(&g_hNSKSCMutex);
		return -1;
	}

	if (g_iNSKSCState == TCC_NSK_SC_STATE_ACTIVATE) {
		tca_nsk_sc_deactivate();
		g_iNSKSCState = TCC_NSK_SC_STATE_ENABLE;
	}

	if (g_iNSKSCState == TCC_NSK_SC_STATE_ENABLE) {
		tca_nsk_sc_disable();
		g_iNSKSCState = TCC_NSK_SC_STATE_OPEN;
	}

	tca_nsk_sc_close(inode, file);

	g_iNSKSCState = TCC_NSK_SC_STATE_NONE;

	mutex_unlock(&g_hNSKSCMutex);

	return 0;
}

static struct file_operations tcc_nsk_sc_fops = {
	.owner = THIS_MODULE,
	.open = tcc_nsk_sc_open,
	.read = tcc_nsk_sc_read,
	.write = tcc_nsk_sc_write,
	.unlocked_ioctl = tcc_nsk_sc_ioctl,
	.release = tcc_nsk_sc_close,
};

static struct miscdevice tcc_nsk_sc_misc_device = {
	.minor = MISC_DYNAMIC_MINOR, // MINOR_ID,
	.name = TCC_NSK_SC_DEVICE_NAME,
	.fops = &tcc_nsk_sc_fops,
};

static int tcc_nsk_sc_probe(struct platform_device *pdev)
{
	int iRet;

	dprintk("%s\n", __func__);

	if (misc_register(&tcc_nsk_sc_misc_device)) {
		eprintk("%s: Couldn't register device %d.\n", __func__, MINOR_ID);
		return -EBUSY;
	}

	iRet = tca_nsk_sc_probe(pdev);
	if (iRet) {
		eprintk("%s: tca_nsk_sc_probe(%d)\n", __func__, iRet);
		return -EBUSY;
	}

	return 0;
}

static int tcc_nsk_sc_remove(struct platform_device *pdev)
{
	tca_nsk_sc_remove();

	misc_deregister(&tcc_nsk_sc_misc_device);

	return 0;
}

//#define CONFIG_OF
#ifdef CONFIG_OF
static struct of_device_id tcc_nsk_sc_of_match[] = {
	{
		.compatible = "telechips,tcc-nsk-sc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, tcc_nsk_sc_of_match);
#endif

#ifndef CONFIG_OF
static struct platform_device tcc_nsk_sc_device = {
	.name = TCC_NSK_SC_DEVICE_NAME,
	.dev =
		{
			.release = NULL,
		},
	.id = 0,
};
#endif

static struct platform_driver tcc_nsk_sc_driver = {
	.probe = tcc_nsk_sc_probe,
	.remove = tcc_nsk_sc_remove,
	.driver = {.name = TCC_NSK_SC_DEVICE_NAME,
			   .owner = THIS_MODULE,
#ifdef CONFIG_OF
			   .of_match_table = of_match_ptr(tcc_nsk_sc_of_match)
#endif
	},
};

static int __init tcc_nsk_sc_init(void)
{
	dprintk("%s\n", __func__);

#ifndef CONFIG_OF
	iRet = platform_device_register(&tcc_nsk_sc_device);
	if (iRet) {
		eprintk("%s: platform_device_register err(%d) \n", __func__, iRet);
		return 0;
	}
#endif

	return platform_driver_register(&tcc_nsk_sc_driver);
}

static void __exit tcc_nsk_sc_exit(void)
{
	dprintk("%s\n", __func__);

	platform_driver_unregister(&tcc_nsk_sc_driver);
#ifndef CONFIG_OF
	platform_device_unregister(&tcc_nsk_sc_device);
#endif

	return;
}

module_init(tcc_nsk_sc_init);
module_exit(tcc_nsk_sc_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC NDS SmartCard driver");
MODULE_LICENSE("GPL");
