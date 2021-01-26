/*
 * platform.c - DesignWare HS OTG Controller platform driver
 *
 * Copyright (C) Matthijs Kooijman <matthijs@stdin.nl>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_data/s3c-hsotg.h>
#include <linux/reset.h>
#include <linux/kthread.h>

#include <linux/usb/of.h>

#include "core.h"
#include "hcd.h"
#include "debug.h"

static const char dwc2_driver_name[] = "dwc2";

/*
 * Check the dr_mode against the module configuration and hardware
 * capabilities.
 *
 * The hardware, module, and dr_mode, can each be set to host, device,
 * or otg. Check that all these values are compatible and adjust the
 * value of dr_mode if possible.
 *
 *                      actual
 *    HW  MOD dr_mode   dr_mode
 *  ------------------------------
 *   HST  HST  any    :  HST
 *   HST  DEV  any    :  ---
 *   HST  OTG  any    :  HST
 *
 *   DEV  HST  any    :  ---
 *   DEV  DEV  any    :  DEV
 *   DEV  OTG  any    :  DEV
 *
 *   OTG  HST  any    :  HST
 *   OTG  DEV  any    :  DEV
 *   OTG  OTG  any    :  dr_mode
 */

enum {
	TXVRT,
	CDT,
	TPT,
	TXRT,
	TXREST,
	TXHSXVT,
	PCFG1_MAX
};

struct pcfg1_unit {
	char *reg_name;
	uint32_t	offset;
	uint32_t	mask;
	char		str[256];
};

struct pcfg1_unit USB_PCFG1[] = {
	/*name,     offset, mask*/
	{"TXVRT  ", 0,      (0xF<<0)},
	{"CDT    ", 4,      (0x7<<4)},
	{"TXPPT  ", 7,      (0x1<<7)},
	{"TP     ", 8,      (0x3<<8)},
	{"TXRT   ", 10,     (0x3<<10)},
	{"TXREST ", 12,     (0x3<<12)},
	{"TXHSXVT", 14,     (0x3<<14)},
};

struct USBDEVPHYCFG {
	volatile unsigned int	U20DH_DEV_PCFG0;
	volatile unsigned int	U20DH_DEV_PCFG1;
	volatile unsigned int	U20DH_DEV_PCFG2;
	volatile unsigned int	U20DH_DEV_PCFG3;
	volatile unsigned int	U20DH_DEV_PCFG4;
	volatile unsigned int	U20DH_DEV_LSTS;
	volatile unsigned int	U20DH_DEV_LCFG0;
	volatile unsigned int	reserved[3];
	volatile unsigned int	U20DH_SEL;
	volatile unsigned int	USB_VBUSVLD_SEL;
};

#if defined(CONFIG_USB_DWC2_TCC_MUX)
struct USBMHSTPHYCFG {
	volatile unsigned int	U20DH_HST_BCFG;
	volatile unsigned int	U20DH_HST_PCFG0;
	volatile unsigned int	U20DH_HST_PCFG1;
	volatile unsigned int	U20DH_HST_PCFG2;
	volatile unsigned int	U20DH_HST_PCFG3;
	volatile unsigned int	U20DH_HST_PCFG4;
	volatile unsigned int	U20DH_HST_STS;
	volatile unsigned int	U20DH_HST_LCFG0;
	volatile unsigned int	U20DH_HST_LCFG1;
};
#endif

#if defined(CONFIG_USB_DWC2_TCC)
#define TCC_DWC_SOFFN_USE
#define TCC_DWC_SUSPSTS_USE

int dwc2_tcc_power_ctrl(struct dwc2_hsotg *hsotg, int on_off)
{
	int err = 0;

	if ((hsotg->vbus_source_ctrl == 1) && (hsotg->vbus_source)) {
		if (on_off == 1) {
			err = regulator_enable(hsotg->vbus_source);
			if (err) {
				dev_err(hsotg->dev,
					"[INFO][USB] dwc_otg: can't enable vbus source\n");
				return err;
			}
			mdelay(1);
		} else if (on_off == 0) {
			regulator_disable(hsotg->vbus_source);
		}
	}

	return err;
}

#if 0
static void dwc2_tcc_vbus_init(struct dwc2_hsotg *hsotg)
{
	dwc2_tcc_power_ctrl(hsotg, 1);
}

static void dwc2_tcc_vbus_exit(struct dwc2_hsotg *hsotg)
{
	dwc2_tcc_power_ctrl(hsotg, 0);
}
#endif

#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static unsigned int vbus_control_enable = 1;
#else
static unsigned int vbus_control_enable;
#endif /* CONFIG_VBUS_CTRL_DEF_ENABLE */
module_param(vbus_control_enable, uint, 0644);
MODULE_PARM_DESC(vbus_control_enable, "dwc2 vbus control enable");


int dwc2_tcc_vbus_ctrl(struct dwc2_hsotg *hsotg, int on_off)
{
	struct usb_phy *phy = hsotg->uphy;

	if (!vbus_control_enable) {
		dev_warn(hsotg->dev, "[WARN][USB] dwc_otg vbus ctrl disable.\n");
		return 0;
	}
	if (!phy) {
		dev_err(hsotg->dev,
			"[ERROR][USB] [%s:%d]PHY driver is needed\n",
			__func__, __LINE__);
		return -1;
	}
	dev_info(hsotg->dev,
		"[INFO][USB] %s : %s\n",
		__func__, on_off ? "on" : "off");
	hsotg->vbus_status = on_off;

	return phy->set_vbus(phy, on_off);
}

static ssize_t dwc2_tcc_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

	return sprintf(buf,
			"dwc2 vbus - %s\n",
			(hsotg->vbus_status) ? "on" : "off");
}

static ssize_t dwc2_tcc_vbus_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2))
		dwc2_tcc_vbus_ctrl(hsotg, 1);

	if (!strncmp(buf, "off", 3))
		dwc2_tcc_vbus_ctrl(hsotg, 0);

	return count;
}
static DEVICE_ATTR(vbus, 0644,
		dwc2_tcc_vbus_show, dwc2_tcc_vbus_store);

static char *dwc2_pcfg1_display(uint32_t old_reg, uint32_t new_reg, char *str)
{
	uint32_t new_val, old_val;
	int i;

	for (i = 0; i < PCFG1_MAX; i++) {
		old_val = (ISSET(old_reg, USB_PCFG1[i].mask)) >>
			(USB_PCFG1[i].offset);
		new_val = (ISSET(new_reg, USB_PCFG1[i].mask)) >>
			(USB_PCFG1[i].offset);

		if (old_val != new_val) {
			sprintf(USB_PCFG1[i].str,
					"%s = 0x%X -> \x1b[1;33m0x%X\x1b[1;0m*\n",
					USB_PCFG1[i].reg_name,
					old_val, new_val);
		} else {
			sprintf(USB_PCFG1[i].str,
					"%s = 0x%X\n",
					USB_PCFG1[i].reg_name, old_val);
		}

		strcat(str, USB_PCFG1[i].str);
	}

	return str;
}

/**
 * Show the current value of the USB 2.0 Host/Device
 * PHY Configuration 1 Register (U20DH_DEV_PCFG1)
 */
static ssize_t dwc2_pcfg1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	struct USBDEVPHYCFG *pUSBDEVPHYCFG =
		(struct USBDEVPHYCFG *)(hsotg->uphy->get_base(hsotg->uphy));
	uint32_t pcfg1_val = readl(&pUSBDEVPHYCFG->U20DH_DEV_PCFG1);
	char str[256] = {0};

	return sprintf(buf, "U20DH_DEV_PCFG1 = 0x%08X\n%s", pcfg1_val,
			dwc2_pcfg1_display(pcfg1_val, pcfg1_val, str));
}

/**
 * Configure the current value of the USB 2.0
 * Host/Device PHY Configuration 1 Register
 * (U20DH_DEV_PCFG1)
 */
static ssize_t dwc2_pcfg1_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	struct USBDEVPHYCFG *pUSBDEVPHYCFG =
		(struct USBDEVPHYCFG *)(hsotg->uphy->get_base(hsotg->uphy));
	uint32_t old_reg = readl(&pUSBDEVPHYCFG->U20DH_DEV_PCFG1);
	uint32_t new_reg = simple_strtoul(buf, NULL, 16);
	char str[256] = {0};
	int i;

	if (count - 1 < 10 || 10 < count - 1) {
		dev_info(hsotg->dev,
			"[INFO][USB]\nThis argument length is \x1b[1;33mnot 10\x1b[0m\n\n");
		dev_info(hsotg->dev,
			"[INFO][USB]\tUsage : echo \x1b[1;31m0xXXXXXXXX\x1b[0m > dwc_pcfg1\n\n");
		dev_info(hsotg->dev,
			"[INFO][USB]\t\t1) length of \x1b[1;32m0xXXXXXXXX\x1b[0m is 10\n");
		dev_info(hsotg->dev,
			"[INFO][USB]\t\t2) \x1b[1;32mX\x1b[0m is hex number(\x1b[1;31m0\x1b[0m to \x1b[1;31mf\x1b[0m)\n\n");
		return count;
	}

	if ((buf[0] != '0') || (buf[1] != 'x')) {
		dev_info(hsotg->dev,
				"[INFO][USB]\n\techo \x1b[1;32m%c%c\x1b[1;0mXXXXXXXX is \x1b[1;33mnot Ox\x1b[0m\n\n",
				buf[0], buf[1]);
		dev_info(hsotg->dev,
				"[INFO][USB]\tUsage : echo \x1b[1;32m0x\x1b[1;31mXXXXXXXX\x1b[0m > dwc_pcfg1\n\n");
		dev_info(hsotg->dev,
				"[INFO][USB]\t\t1) \x1b[1;32m0\x1b[0m is binary number\x1b[0m)\n\n");
		return count;
	}

	for (i = 2; i < 10; i++) {
		if ((buf[i] >= '0' && buf[i] <= '9') ||
				(buf[i] >= 'a' && buf[i] <= 'f') ||
				(buf[i] >= 'A' && buf[i] <= 'F'))
			continue;
		else {
			dev_info(hsotg->dev,
					"[INFO][USB]\necho 0x%c%c%c%c%c%c%c%c is \x1b[1;33mnot hex\x1b[0m\n\n",
					buf[2], buf[3], buf[4],
					buf[5], buf[6], buf[7],
					buf[8], buf[9]);
			dev_info(hsotg->dev,
					"[INFO][USB]\tUsage : echo \x1b[1;31m0xXXXXXXXX\x1b[0m > dwc_pcfg1\n\n");
			dev_info(hsotg->dev,
					"[INFO][USB]\t\t2) \x1b[1;32mX\x1b[0m is hex number(\x1b[1;31m0\x1b[0m to \x1b[1;31mf\x1b[0m)\n\n");
			return count;
		}
	}

	dev_info(hsotg->dev,
			"[INFO][USB] U20DH_DEV_PCFG1 = 0x%08X\n",
			old_reg);
	writel(new_reg, &pUSBDEVPHYCFG->U20DH_DEV_PCFG1);
	new_reg = readl(&pUSBDEVPHYCFG->U20DH_DEV_PCFG1);

	dwc2_pcfg1_display(old_reg, new_reg, str);
	dev_info(hsotg->dev,
			"[INFO][USB] %sU20DH_DEV_PCFG1 = \x1b[1;33m0x%08X\x1b[1;0m\n",
			str, new_reg);

	return count;
}
static DEVICE_ATTR(dwc_pcfg1, 0644,
		dwc2_pcfg1_show, dwc2_pcfg1_store);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
/**
 * Show the current value of the USB 2.0 Host
 * in Host/Device MUX PHY Configuration 1 Register
 * (U20DH_HST_PCFG1)
 */
static ssize_t dwc2_host_mux_pcfg1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	struct USBMHSTPHYCFG *pUSBMHSTPHYCFG =
		(struct USBMHSTPHYCFG *)
		(hsotg->mhst_uphy->get_base(hsotg->mhst_uphy));
	uint32_t pcfg1_val = readl(&pUSBMHSTPHYCFG->U20DH_HST_PCFG1);
	char str[256] = {0};

	return sprintf(buf, "U20DH_HST_PCFG1 = 0x%08X\n%s", pcfg1_val,
			dwc2_pcfg1_display(pcfg1_val, pcfg1_val, str));
}

/**
 * Configure the current value of the USB 2.0 Host
 * in Host/Device MUX PHY Configuration 1 Register
 * (U20DH_HST_PCFG1)
 */
static ssize_t dwc2_host_mux_pcfg1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	struct USBMHSTPHYCFG *pUSBMHSTPHYCFG =
		(struct USBMHSTPHYCFG *)
		(hsotg->mhst_uphy->get_base(hsotg->mhst_uphy));
	uint32_t old_reg = readl(&pUSBMHSTPHYCFG->U20DH_HST_PCFG1);
	uint32_t new_reg = simple_strtoul(buf, NULL, 16);
	char str[256] = {0};
	int i;

	if (count - 1 < 10 || 10 < count - 1) {
		dev_info(hsotg->dev,
				"[INFO][USB]\nThis argument length is \x1b[1;33mnot 10\x1b[0m\n\n");
		dev_info(hsotg->dev,
				"[INFO][USB]\tUsage : echo \x1b[1;31m0xXXXXXXXX\x1b[0m > dwc_host_mux_pcfg1\n\n");
		dev_info(hsotg->dev,
				"[INFO][USB]\t\t1) length of \x1b[1;32m0xXXXXXXXX\x1b[0m is 10\n");
		dev_info(hsotg->dev,
				"[INFO][USB]\t\t2) \x1b[1;32mX\x1b[0m is hex number(\x1b[1;31m0\x1b[0m to \x1b[1;31mf\x1b[0m)\n\n");
		return count;
	}

	if ((buf[0] != '0') || (buf[1] != 'x')) {
		dev_info(hsotg->dev,
				"[INFO][USB]\n\techo \x1b[1;32m%c%c\x1b[1;0mXXXXXXXX is \x1b[1;33mnot Ox\x1b[0m\n\n",
				buf[0], buf[1]);
		dev_info(hsotg->dev,
				"[INFO][USB]\tUsage : echo \x1b[1;32m0x\x1b[1;31mXXXXXXXX\x1b[0m > dwc_host_mux_pcfg1\n\n");
		dev_info(hsotg->dev,
				"[INFO][USB]\t\t1) \x1b[1;32m0\x1b[0m is binary number\x1b[0m)\n\n");
		return count;
	}

	for (i = 2; i < 10; i++) {
		if ((buf[i] >= '0' && buf[i] <= '9') ||
				(buf[i] >= 'a' && buf[i] <= 'f') ||
				(buf[i] >= 'A' && buf[i] <= 'F'))
			continue;
		else {
			dev_info(hsotg->dev,
					"[INFO][USB]\necho 0x%c%c%c%c%c%c%c%c is \x1b[1;33mnot hex\x1b[0m\n\n",
					buf[2], buf[3], buf[4], buf[5],
					buf[6], buf[7], buf[8], buf[9]);
			dev_info(hsotg->dev,
					"[INFO][USB]\tUsage : echo \x1b[1;31m0xXXXXXXXX\x1b[0m > dwc_host_mux_pcfg1\n\n");
			dev_info(hsotg->dev,
					"[INFO][USB]\t\t2) \x1b[1;32mX\x1b[0m is hex number(\x1b[1;31m0\x1b[0m to \x1b[1;31mf\x1b[0m)\n\n");
			return count;
		}
	}

	dev_info(hsotg->dev,
			"[INFO][USB] U20DH_HST_PCFG1 = 0x%08X\n",
			old_reg);
	writel(new_reg, &pUSBMHSTPHYCFG->U20DH_HST_PCFG1);
	new_reg = readl(&pUSBMHSTPHYCFG->U20DH_HST_PCFG1);

	dwc2_pcfg1_display(old_reg, new_reg, str);
	dev_info(hsotg->dev,
			"[INFO][USB] %sU20DH_HST_PCFG1 = \x1b[1;33m0x%08X\x1b[1;0m\n",
			str, new_reg);

	return count;
}
static DEVICE_ATTR(dwc_host_mux_pcfg1, 0644,
		dwc2_host_mux_pcfg1_show, dwc2_host_mux_pcfg1_store);
#endif

#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
static ssize_t dwc2_tcc_drd_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

	return sprintf(buf, "dwc2 dr_mode - %s\n",
			hsotg->dr_mode == USB_DR_MODE_PERIPHERAL ?
			"DEVICE":"HOST");
}

static void dwc2_change_dr_mode(struct work_struct *);
static ssize_t dwc2_tcc_drd_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	struct work_struct *work;
	enum usb_dr_mode tmp_mode;

	if (!strncmp(buf, "host", 4)) {
		if (hsotg->dr_mode == USB_DR_MODE_HOST ||
			hsotg->dr_mode == USB_DR_MODE_OTG) {
			dev_warn(hsotg->dev, "[WARN][USB] Already host mode!\n");
			goto error;
		}
		tmp_mode = USB_DR_MODE_HOST;
	} else if (!strncmp(buf, "device", 6)) {
		if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) {
			dev_err(hsotg->dev, "[ERROR][USB] Already device mode!\n");
			goto error;
		}
		tmp_mode = USB_DR_MODE_PERIPHERAL;
	} else {
error:
		dev_warn(hsotg->dev, "[WARN][USB] Value is invalid!\n");
		return count;
	}

	hsotg->dr_mode = tmp_mode;
	work = &hsotg->drd_work;

	if (work_pending(work)) {
		dev_warn(hsotg->dev, "[WARN][USB] [drd_store pending]\n");
		return count;
	}


	queue_work(hsotg->drd_wq, work);
	/* wait for operation to complete */
	flush_work(work);

	return count;
}
static DEVICE_ATTR(drdmode, 0644,
		dwc2_tcc_drd_mode_show, dwc2_tcc_drd_mode_store);

#if defined(TCC_DWC_SUSPSTS_USE)
static int dwc2_soffn_monitor_thread(void *w)
{
	struct dwc2_hsotg *hsotg = (struct dwc2_hsotg *)w;
	bool discon_chk = false;
	int retry = 100;	//wait for a sec until iPhone role-switching
	unsigned long flags;

	while (!kthread_should_stop() && retry > 0) {
		usleep_range(10000, 10020);
		/* Monitoring starts when soffn is
		 * not NULL and not in suspend state
		 * If host is not connected for 1 second
		 * after role-switch, disconnect is judged
		 */

		if (!dwc2_hsotg_read_frameno(hsotg) ||
				dwc2_hsotg_read_suspend_state(hsotg)) {
			retry--;
			if (discon_chk == true && retry <= 0)
				break;
			continue;
		}
		discon_chk = true;
		retry = 3;
	}

	dwc2_tcc_vbus_ctrl(hsotg, 0); //VBUS off
	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) {
		spin_lock_irqsave(&hsotg->lock, flags);
		dwc2_hsotg_disconnect(hsotg);
		spin_unlock_irqrestore(&hsotg->lock, flags);
		if (hsotg->driver && hsotg->driver->disconnect_tcc)
			hsotg->driver->disconnect_tcc();
	} else {
		dev_warn(hsotg->dev,
				"[WARN][USB] %s - mode changed (host)",
				__func__);
	}
	hsotg->soffn_thread = NULL;

	msleep(200);
	dev_warn(hsotg->dev,
			"[WARN][USB] dwc2 device - Host Disconnected\n");

	return 0;
}
#endif
#endif

#if defined(CONFIG_USB_DWC2_TCC_MUX)
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb/hcd.h>

struct usb_mux_hcd_device {
	struct platform_device *ehci_dev;
	struct platform_device *ohci_dev;
	u32 enable_flags;
};

static const struct usb_ehci_pdata ehci_pdata = {
};

static const struct usb_ohci_pdata ohci_pdata = {
};

static struct platform_device *dwc2_create_mux_hcd_pdev
		(struct dwc2_hsotg *hsotg, bool ohci,
		unsigned long int res_start, u32 size)
{
	struct platform_device *hci_dev;
	struct resource hci_res[2];
	struct usb_hcd *hcd;
	int ret = -ENOMEM;

	memset(hci_res, 0, sizeof(hci_res));

	hci_res[0].start = res_start;
	hci_res[0].end = res_start + size - 1;
	hci_res[0].flags = IORESOURCE_MEM;

	hci_res[1].start = hsotg->ehci_irq;
	hci_res[1].flags = IORESOURCE_IRQ;

	hci_dev = platform_device_alloc(ohci ? "ohci-mux" :
			"ehci-mux", 0);

	if (!hci_dev)
		return NULL;

	hci_dev->dev.parent = hsotg->dev;
	hci_dev->dev.dma_mask = &hci_dev->dev.coherent_dma_mask;

	ret = platform_device_add_resources(hci_dev, hci_res,
			ARRAY_SIZE(hci_res));
	if (ret)
		goto err_alloc;
	if (ohci)
		ret = platform_device_add_data(hci_dev, &ohci_pdata,
				sizeof(ohci_pdata));
	else
		ret = platform_device_add_data(hci_dev, &ehci_pdata,
				sizeof(ehci_pdata));
	if (ret)
		goto err_alloc;
	ret = platform_device_add(hci_dev);
	if (ret)
		goto err_alloc;

	hcd = dev_get_drvdata(&hci_dev->dev);
	if (hcd == NULL) {
		dev_info(hsotg->dev,
				"[INFO][USB] \x1b[1;31m[%s:%d](hcd == NULL)\x1b[0m\n",
				__func__, __LINE__);
		while (1)
			;
	}
	//hcd->tpl_support = hsotg->hcd_tpl_support;

	if (!ohci && hsotg->mhst_uphy)
		hcd->usb_phy = hsotg->mhst_uphy;

	return hci_dev;

err_alloc:
	platform_device_put(hci_dev);
	return ERR_PTR(ret);
}

static int dwc2_mux_hcd_init(struct dwc2_hsotg *hsotg)
{
	struct usb_mux_hcd_device *usb_dev;
	int err;
	unsigned long int start;
	int	size;

	usb_dev = kzalloc(sizeof(struct usb_mux_hcd_device), GFP_KERNEL);
	if (!usb_dev)
		return -ENOMEM;

	start = (unsigned long int)hsotg->ohci_regs;
	size = (int)hsotg->ohci_regs_size;
	usb_dev->ohci_dev = dwc2_create_mux_hcd_pdev(hsotg, true, start, size);
	if (IS_ERR(usb_dev->ohci_dev)) {
		err = PTR_ERR(usb_dev->ohci_dev);
		goto err_free_usb_dev;
	}

	start = (unsigned long int)hsotg->ehci_regs;
	size = hsotg->ehci_regs_size;
	usb_dev->ehci_dev = dwc2_create_mux_hcd_pdev(hsotg, false, start, size);
	if (IS_ERR(usb_dev->ehci_dev)) {
		err = PTR_ERR(usb_dev->ehci_dev);
		goto err_unregister_ohci_dev;
	}

	hsotg->mhst_dev = usb_dev;
	return 0;

err_unregister_ohci_dev:
	platform_device_unregister(usb_dev->ohci_dev);
err_free_usb_dev:
	kfree(usb_dev);
	return err;
}

static void dwc2_mux_hcd_remove(struct dwc2_hsotg *hsotg)
{
	struct usb_mux_hcd_device *usb_dev;
	struct platform_device *ohci_dev;
	struct platform_device *ehci_dev;

	usb_dev = hsotg->mhst_dev;
	ohci_dev = usb_dev->ohci_dev;
	ehci_dev = usb_dev->ehci_dev;

	if (ohci_dev)
		platform_device_unregister(ohci_dev);
	if (ehci_dev)
		platform_device_unregister(ehci_dev);

	kfree(usb_dev);
}
#endif

#endif

static int dwc2_get_dr_mode(struct dwc2_hsotg *hsotg)
{
	enum usb_dr_mode mode;

	hsotg->dr_mode = usb_get_dr_mode(hsotg->dev);
	if (hsotg->dr_mode == USB_DR_MODE_UNKNOWN)
		hsotg->dr_mode = USB_DR_MODE_OTG;

	mode = hsotg->dr_mode;

	if (dwc2_hw_is_device(hsotg)) {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST)) {
			dev_err(hsotg->dev,
					"[ERROR][USB] Controller does not support host mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_PERIPHERAL;
	} else if (dwc2_hw_is_host(hsotg)) {
		if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL)) {
			dev_err(hsotg->dev,
					"[ERROR][USB] Controller does not support device mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_HOST;
	} else {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST))
			mode = USB_DR_MODE_HOST;
		else if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL))
			mode = USB_DR_MODE_PERIPHERAL;
	}
	if (mode != hsotg->dr_mode) {
		dev_warn(hsotg->dev,
				"[WARN][USB] Configuration mismatch. dr_mode forced to %s\n",
				mode == USB_DR_MODE_HOST ? "host" : "device");

		hsotg->dr_mode = mode;
	}
	dev_warn(hsotg->dev,
		"[WARN][USB] %s : dr_mode = %d\n", __func__,
		hsotg->dr_mode);

	return 0;
}

static int __dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret;
#if defined(CONFIG_USB_DWC2_TCC)
	dwc2_tcc_vbus_ctrl(hsotg, 1);
#else
	ret = regulator_bulk_enable(ARRAY_SIZE(hsotg->supplies),
			hsotg->supplies);
	if (ret)
		return ret;
#endif
	if (hsotg->clk) {
		ret = clk_prepare_enable(hsotg->clk);
		if (ret)
			return ret;
	}

	if (hsotg->uphy) {
		ret = usb_phy_init(hsotg->uphy);
	} else if (hsotg->plat && hsotg->plat->phy_init) {
		ret = hsotg->plat->phy_init(pdev, hsotg->plat->phy_type);
	} else {
		ret = phy_power_on(hsotg->phy);
		if (ret == 0)
			ret = phy_init(hsotg->phy);
	}
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	if (hsotg->mhst_uphy &&
		((hsotg->dr_mode == USB_DR_MODE_HOST) ||
		(hsotg->dr_mode == USB_DR_MODE_OTG)))
		ret = usb_phy_init(hsotg->mhst_uphy);
#endif
	return ret;
}

/**
 * dwc2_lowlevel_hw_enable - enable platform lowlevel hw resources
 * @hsotg: The driver state
 *
 * A wrapper for platform code responsible for controlling
 * low-level USB platform resources (phy, clock, regulators)
 */
int dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg)
{
	int ret = __dwc2_lowlevel_hw_enable(hsotg);

	dev_info(hsotg->dev, "[INFO][USB] %s : %d\n", __func__, __LINE__);
	if (ret == 0)
		hsotg->ll_hw_enabled = true;
	return ret;
}

static int __dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret = 0;

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	if (hsotg->mhst_uphy)
		usb_phy_shutdown(hsotg->mhst_uphy);
#endif
	if (hsotg->uphy) {
		usb_phy_shutdown(hsotg->uphy);
	} else if (hsotg->plat && hsotg->plat->phy_exit) {
		ret = hsotg->plat->phy_exit(pdev, hsotg->plat->phy_type);
	} else {
		ret = phy_exit(hsotg->phy);
		if (ret == 0)
			ret = phy_power_off(hsotg->phy);
	}
	if (ret)
		return ret;

	if (hsotg->clk)
		clk_disable_unprepare(hsotg->clk);
#if defined(CONFIG_USB_DWC2_TCC)
	ret = dwc2_tcc_vbus_ctrl(hsotg, 0);
#else
	ret = regulator_bulk_disable(ARRAY_SIZE(hsotg->supplies),
			hsotg->supplies);
#endif
	return ret;
}

/**
 * dwc2_lowlevel_hw_disable - disable platform lowlevel hw resources
 * @hsotg: The driver state
 *
 * A wrapper for platform code responsible for controlling
 * low-level USB platform resources (phy, clock, regulators)
 */
int dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	int ret = __dwc2_lowlevel_hw_disable(hsotg);

	dev_info(hsotg->dev, "[INFO][USB] %s : %d\n", __func__, __LINE__);
	if (ret == 0)
		hsotg->ll_hw_enabled = false;
	return ret;
}

static int dwc2_lowlevel_hw_init(struct dwc2_hsotg *hsotg)
{
	int ret;

	hsotg->reset = devm_reset_control_get_optional(hsotg->dev, "dwc2");
	if (IS_ERR(hsotg->reset)) {
		ret = PTR_ERR(hsotg->reset);
		dev_err(hsotg->dev,
			"[ERROR][USB] error getting reset control %d\n",
			ret);
		return ret;
	}

	reset_control_deassert(hsotg->reset);

	/* Set default UTMI width */
	hsotg->phyif = GUSBCFG_PHYIF16;

	/*
	 * Attempt to find a generic PHY, then look for an old style
	 * USB PHY and then fall back to pdata
	 */
	hsotg->phy = devm_phy_get(hsotg->dev, "usb2-phy");
	if (IS_ERR(hsotg->phy)) {
		dev_warn(hsotg->dev,
				"[WARN][USB] [dwc2]usb-2phy is ERR %s\n",
				__func__);
		ret = PTR_ERR(hsotg->phy);
		switch (ret) {
		case -ENODEV:
		case -EINVAL:
			hsotg->phy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			hsotg->phy = NULL;
			dev_warn(hsotg->dev,
					"[WARN][USB] error getting phy %d\n",
					ret);
		}
	}

	if (!hsotg->phy) {
		dev_warn(hsotg->dev,
			"[WARN][USB] [dwc2]hsotg->phy is NULL %s\n",
			__func__);
#ifndef CONFIG_USB_DWC2_TCC
		hsotg->uphy = devm_usb_get_phy(hsotg->dev, USB_PHY_TYPE_USB2);
#else
		hsotg->uphy = devm_usb_get_phy_by_phandle(hsotg->dev, "phy", 0);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		ret = hsotg->uphy->set_vbus_resource(hsotg->uphy);
		if (ret)
			return ret;
#endif
#endif
		if (IS_ERR(hsotg->uphy)) {
			dev_warn(hsotg->dev,
				"[WARN][USB] [dwc2]hsotg->uphy is NULL %s\n",
				__func__);
			ret = PTR_ERR(hsotg->uphy);
			switch (ret) {
			case -ENODEV:
			case -ENXIO:
				hsotg->uphy = NULL;
				break;
			case -EPROBE_DEFER:
				return ret;
			default:
				dev_err(hsotg->dev,
						"[ERROR][USB] error getting usb phy %d\n",
						ret);
				return ret;
			}
		}
	}
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->mhst_uphy =
		devm_usb_get_phy_by_phandle(hsotg->dev,
				"telechips,mhst_phy", 0);

	if (IS_ERR(hsotg->mhst_uphy)) {
		dev_warn(hsotg->dev,
				"[WARN][USB] [dwc2]hsotg->mhst_uphy is NULL %s\n",
				__func__);
		ret = PTR_ERR(hsotg->uphy);
		switch (ret) {
		case -ENODEV:
		case -ENXIO:
			hsotg->mhst_uphy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(hsotg->dev, "[ERROR][USB] error getting usb mhst phy %d\n",
					ret);
			return ret;
		}
	}

	hsotg->mhst_uphy->otg->mux_cfg_addr =
		hsotg->uphy->base + 0x28; //otg phy's mux switching register
#endif
	hsotg->plat = dev_get_platdata(hsotg->dev);

	if (hsotg->phy) {
		/*
		 * If using the generic PHY framework, check if the PHY bus
		 * width is 8-bit and set the phyif appropriately.
		 */
		if (phy_get_bus_width(hsotg->phy) == 8)
			hsotg->phyif = GUSBCFG_PHYIF8;
	}
	/* Clock */
	hsotg->clk = devm_clk_get(hsotg->dev, "otg");
	if (IS_ERR(hsotg->clk)) {
		hsotg->clk = NULL;
		dev_err(hsotg->dev, "[ERROR][USB] cannot get otg clock\n");
	}
#if defined(CONFIG_USB_DWC2_TCC)
	/* TCC vbus */
	ret = dwc2_tcc_vbus_ctrl(hsotg, 1);
#else
	/* Regulators */
	for (i = 0; i < ARRAY_SIZE(hsotg->supplies); i++)
		hsotg->supplies[i].supply = dwc2_hsotg_supply_names[i];

	ret = devm_regulator_bulk_get(hsotg->dev, ARRAY_SIZE(hsotg->supplies),
			hsotg->supplies);
	if (ret) {
		dev_err(hsotg->dev,
				"[ERROR][USB] failed to request supplies: %d\n",
				ret);
		return ret;
	}
#endif
	return 0;
}
#if defined(CONFIG_USB_DWC2_TCC) && defined(CONFIG_USB_DWC2_DUAL_ROLE)
#define VBUS_CTRL_MAX 10

static void dwc2_change_dr_mode(struct work_struct *w)
{
	struct dwc2_hsotg *hsotg = container_of(w, struct dwc2_hsotg,
			drd_work);
	unsigned int	res = 0;
	unsigned long flags;

	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		dwc2_mux_hcd_remove(hsotg);
#endif
	} else {
		int retry_cnt = 0;

#if defined(TCC_DWC_SOFFN_USE)
		if (hsotg->soffn_thread != NULL) {
			kthread_stop(hsotg->soffn_thread);
			hsotg->soffn_thread = NULL;
			spin_lock_irqsave(&hsotg->lock, flags);
			dwc2_hsotg_disconnect(hsotg);
			spin_unlock_irqrestore(&hsotg->lock, flags);
		}

		do {
			if (dwc2_tcc_vbus_ctrl(hsotg, 1) == 0 ||
				!vbus_control_enable)
				break;

			retry_cnt++;
			msleep(50);
			dev_warn(hsotg->dev,
					"[WARN][USB] [%s]Retry to control vbus(%d)!\n",
					__func__, retry_cnt);
		} while (retry_cnt < VBUS_CTRL_MAX);
#endif
	}
	dwc2_manual_change(hsotg);
	if (hsotg->dr_mode == USB_DR_MODE_HOST) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		dwc2_mux_hcd_init(hsotg);
#endif
	} else {
		if (hsotg->vbus_status == 1) {
			if (hsotg->soffn_thread != NULL) {
				kthread_stop(hsotg->soffn_thread);
				hsotg->soffn_thread = NULL;
			}


			hsotg->soffn_thread =
				kthread_run(dwc2_soffn_monitor_thread,
						(void *)hsotg, "dwc2-soffn");
			if (IS_ERR(hsotg->soffn_thread)) {
				dev_warn(hsotg->dev,
						"[WARN][USB] \x1b[1;33m[%s:%d]\x1b[0m thread error\n",
						__func__, __LINE__);
				res = PTR_ERR(hsotg->soffn_thread);
			}
		}
		dev_warn(hsotg->dev,
				"[WARN][USB] Current mode is %s\n",
				(hsotg->dr_mode == USB_DR_MODE_HOST) ?
				"Host" : "Device");
	}
}
#endif

/**
 * dwc2_driver_remove() - Called when the DWC_otg core is unregistered with the
 * DWC_otg driver
 *
 * @dev: Platform device
 *
 * This routine is called, for example, when the rmmod command is executed. The
 * device may or may not be electrically present. If it is present, the driver
 * stops device processing. Any resources used on behalf of this device are
 * freed.
 */
static int dwc2_driver_remove(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(dev);

	dwc2_debugfs_exit(hsotg);
	if (hsotg->hcd_enabled)
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		if (hsotg->dr_mode == USB_DR_MODE_HOST ||
				hsotg->dr_mode == USB_DR_MODE_OTG)
			dwc2_mux_hcd_remove(hsotg);
#else
	dwc2_hcd_remove(hsotg);
#endif
	if (hsotg->gadget_enabled)
		dwc2_hsotg_remove(hsotg);

	if (hsotg->ll_hw_enabled)
		dwc2_lowlevel_hw_disable(hsotg);

	reset_control_assert(hsotg->reset);
#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
	cancel_work_sync(&hsotg->drd_work);
	destroy_workqueue(hsotg->drd_wq);

	device_remove_file(&dev->dev, &dev_attr_drdmode);
#endif
	device_remove_file(&dev->dev, &dev_attr_vbus);
#endif

	device_remove_file(&dev->dev, &dev_attr_dwc_pcfg1);
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	device_remove_file(&dev->dev, &dev_attr_dwc_host_mux_pcfg1);
#endif

	return 0;
}

/**
 * dwc2_driver_shutdown() - Called on device shutdown
 *
 * @dev: Platform device
 *
 * In specific conditions (involving usb hubs) dwc2 devices can create a
 * lot of interrupts, even to the point of overwhelming devices running
 * at low frequencies. Some devices need to do special clock handling
 * at shutdown-time which may bring the system clock below the threshold
 * of being able to handle the dwc2 interrupts. Disabling dwc2-irqs
 * prevents reboots/poweroffs from getting stuck in such cases.
 */
static void dwc2_driver_shutdown(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(dev);
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	disable_irq(hsotg->ehci_irq);
#endif
	dwc2_disable_global_interrupts(hsotg);
	synchronize_irq(hsotg->irq);
}

/**
 * dwc2_driver_probe() - Called when the DWC_otg core is bound to the DWC_otg
 * driver
 *
 * @dev: Platform device
 *
 * This routine creates the driver components required to control the device
 * (core, HCD, and PCD) and initializes the device. The driver components are
 * stored in a dwc2_hsotg structure. A reference to the dwc2_hsotg is saved
 * in the device private data. This allows the driver to access the dwc2_hsotg
 * structure on subsequent calls to driver methods for this device.
 */
static int dwc2_driver_probe(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg;
	struct resource *res;
	int retval;
	unsigned int cpu;

	hsotg = devm_kzalloc(&dev->dev, sizeof(*hsotg), GFP_KERNEL);
	if (!hsotg)
		return -ENOMEM;

	hsotg->dev = &dev->dev;

	/*
	 * Use reasonable defaults so platforms don't have to provide these.
	 */
	if (!dev->dev.dma_mask)
		dev->dev.dma_mask = &dev->dev.coherent_dma_mask;
	retval = dma_set_coherent_mask(&dev->dev, DMA_BIT_MASK(32));
	if (retval)
		return retval;

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	hsotg->regs = devm_ioremap_resource(&dev->dev, res);
	if (IS_ERR(hsotg->regs))
		return PTR_ERR(hsotg->regs);

	dev_info(&dev->dev, "[INFO][USB] dwc2 controller mapped PA %08lx to VA %p\n",
			(unsigned long)res->start, hsotg->regs);
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	/*
	 * Get ehci's register base for MUX
	 */
	hsotg->ehci_regs = (void __iomem *)(long int)dev->resource[1].start;
	hsotg->ehci_regs_size =
		dev->resource[1].end - dev->resource[1].start + 1;
	if (IS_ERR(hsotg->ehci_regs))
		return PTR_ERR(hsotg->ehci_regs);

	dev_info(&dev->dev,
			"[INFO][USB] ehci controller mapped PA %08lx to VA %p SIZE %d\n",
			(unsigned long)res->start,
			hsotg->ehci_regs, hsotg->ehci_regs_size);

	/*
	 * Get ohci's register base for MUX
	 */
	hsotg->ohci_regs = (void __iomem *)(long int)dev->resource[2].start;
	hsotg->ohci_regs_size =
		dev->resource[2].end - dev->resource[2].start + 1;
	if (IS_ERR(hsotg->ohci_regs))
		return PTR_ERR(hsotg->ohci_regs);

	dev_info(&dev->dev,
			"[INFO][USB] ohci controller mapped PA %08lx to VA %p SIZE %d\n",
			(unsigned long)res->start,
			hsotg->ohci_regs, hsotg->ohci_regs_size);

#endif
	retval = dwc2_lowlevel_hw_init(hsotg);
	if (retval) {
		dev_err(hsotg->dev,
			"[ERROR][USB] dwc2_lowlevel_hw_init() failed, errno %d.\n",
			retval);
		return retval;
	}

	spin_lock_init(&hsotg->lock);

	hsotg->irq = platform_get_irq(dev, 0);
	if (hsotg->irq < 0) {
		dev_err(&dev->dev, "[ERROR][USB] missing IRQ resource\n");
		return hsotg->irq;
	}

	dev_info(hsotg->dev,
			"[INFO][USB] registering common handler for irq%d\n",
			hsotg->irq);
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->ehci_irq = platform_get_irq(dev, 1);
	if (hsotg->irq < 0) {
		dev_err(&dev->dev, "[ERROR][USB] missing IRQ resource\n");
		return hsotg->ehci_irq;
	}
	dev_dbg(hsotg->dev,
			"[DEBUG][USB] ehci_irq's number is %d\n",
			hsotg->ehci_irq);
#endif

	/* Set the irq affinity in order to handle the irq more stably */

	cpu = 1;

	retval =
		irq_set_affinity_hint(hsotg->irq, cpumask_of(cpu));
	if (retval) {
		dev_err(hsotg->dev,
				"[ERROR][USB] failed to set the irq affinity irq %d cpu %d err %d\n",
				hsotg->irq, cpu, retval);
		return retval;
	}
	dev_info(hsotg->dev,
			"[INFO][USB] set the irq(%d) affinity to cpu(%d)\n",
			hsotg->irq, cpu);

	retval = dwc2_get_dr_mode(hsotg);
	if (retval)
		return retval;

	retval = dwc2_lowlevel_hw_enable(hsotg);
	if (retval)
		return retval;


	/*
	 * Reset before dwc2_get_hwparams() then it could get power-on real
	 * reset value form registers.
	 */
	dwc2_core_reset_and_force_dr_mode(hsotg);

	/* Detect config values from hardware */
	retval = dwc2_get_hwparams(hsotg);
	if (retval)
		goto error;

	retval = devm_request_irq(hsotg->dev, hsotg->irq,
			dwc2_handle_common_intr, IRQF_SHARED,
			dev_name(hsotg->dev), hsotg);
	if (retval)
		return retval;

	dwc2_force_dr_mode(hsotg);

	retval = dwc2_init_params(hsotg);
	if (retval)
		goto error;

	if (hsotg->dr_mode != USB_DR_MODE_HOST) {
		retval = dwc2_gadget_init(hsotg);
		if (retval)
			goto error;
		hsotg->gadget_enabled = 1;
	}

	if (hsotg->dr_mode != USB_DR_MODE_PERIPHERAL) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		retval = dwc2_mux_hcd_init(hsotg);
#else
		retval = dwc2_hcd_init(hsotg);
#endif
		if (retval) {
			if (hsotg->gadget_enabled)
				dwc2_hsotg_remove(hsotg);
			goto error;
		}
		hsotg->hcd_enabled = 1;
	}

	platform_set_drvdata(dev, hsotg);

	dwc2_debugfs_init(hsotg);

	/* Gadget code manages lowlevel hw on its own */
	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) {
		dwc2_lowlevel_hw_disable(hsotg);
		goto skip_mode_change;
	}
#if defined(CONFIG_USB_DWC2_TCC)
	retval = device_create_file(&dev->dev, &dev_attr_vbus);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] failed to create vbus\n");
#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
	hsotg->drd_wq = create_singlethread_workqueue("dwc2");
	if (!hsotg->drd_wq)
		goto error;

	INIT_WORK(&hsotg->drd_work, dwc2_change_dr_mode);
	retval = device_create_file(&dev->dev, &dev_attr_drdmode);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] failed to create dr_mode\n");

#if defined(CONFIG_USB_DWC2_TCC_FIRST_HOST) //first host
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	//NOTHING TO DO!!
#else
	hsotg->dr_mode = USB_DR_MODE_PERIPHERAL;
	dwc2_manual_change(hsotg);
	hsotg->dr_mode = USB_DR_MODE_HOST;
	dwc2_manual_change(hsotg);
#endif
#elif defined(CONFIG_USB_DWC2_TCC_FIRST_PERIPHERAL)
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->dr_mode = USB_DR_MODE_PERIPHERAL;

	struct work_struct *work;

	work = &hsotg->drd_work;

	if (work_pending(work))
		dev_warn(hsotg->dev, "[WARN][USB] [drd_store pending]\n");

	queue_work(hsotg->drd_wq, work);
	/* wait for operation to complete */
	flush_work(work);
#else
	hsotg->dr_mode = USB_DR_MODE_PERIPHERAL;
	dwc2_manual_change(hsotg);
#endif

#endif //CONFIG_USB_DWC2_TCC_FIRST
#endif //CONFIG_USB_DWC2_DUAL_ROLE
#endif //CONFIG_USB_DWC2_TCC

	retval = device_create_file(&dev->dev, &dev_attr_dwc_pcfg1);

	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] failed to create dwc_pcfg1\n");
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	retval = device_create_file(&dev->dev, &dev_attr_dwc_host_mux_pcfg1);

	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] failed to create dwc_host_mux_pcfg1\n");
#endif

skip_mode_change:
#if IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) || \
	IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	/* Postponed adding a new gadget to the udc class driver list */
	if (hsotg->gadget_enabled) {
		retval = usb_add_gadget_udc(hsotg->dev, &hsotg->gadget);
		if (retval) {
			hsotg->gadget.udc = NULL;
			dwc2_hsotg_remove(hsotg);
			goto error;
		}
	}
#endif /* CONFIG_USB_DWC2_PERIPHERAL || CONFIG_USB_DWC2_DUAL_ROLE */

	return 0;
error:
	if (hsotg->dr_mode != USB_DR_MODE_PERIPHERAL)
		dwc2_lowlevel_hw_disable(hsotg);
	return retval;
}

static int __maybe_unused dwc2_suspend(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	int ret = 0;

	if (dwc2_is_device_mode(dwc2))
		dwc2_hsotg_suspend(dwc2);

	if (dwc2->ll_hw_enabled)
		ret = __dwc2_lowlevel_hw_disable(dwc2);

	return ret;
}

static int __maybe_unused dwc2_resume(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	int ret = 0;

	if (dwc2->ll_hw_enabled) {
		ret = __dwc2_lowlevel_hw_enable(dwc2);
		if (ret)
			return ret;
	}

	if (dwc2_is_device_mode(dwc2))
		ret = dwc2_hsotg_resume(dwc2);

	return ret;
}

static const struct dev_pm_ops dwc2_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc2_suspend, dwc2_resume)
};

static struct platform_driver dwc2_platform_driver = {
	.driver = {
		.name = dwc2_driver_name,
		.of_match_table = dwc2_of_match_table,
		.pm = &dwc2_dev_pm_ops,
	},
	.probe = dwc2_driver_probe,
	.remove = dwc2_driver_remove,
	.shutdown = dwc2_driver_shutdown,
};

module_platform_driver(dwc2_platform_driver);

MODULE_DESCRIPTION("DESIGNWARE HS OTG Platform Glue");
MODULE_AUTHOR("Matthijs Kooijman <matthijs@stdin.nl>");
MODULE_LICENSE("Dual BSD/GPL");
