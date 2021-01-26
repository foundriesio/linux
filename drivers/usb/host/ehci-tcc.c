/*
 * linux/drivers/usb/host/ehci-tcc.c
 *
 * Description: EHCI HCD (Host Controller Driver) for USB.
 *              Bus Glue for Telechips-SoC
 *
 *  Copyright (C) 2009 Atmel Corporation,
 *                     Nicolas Ferre <nicolas.ferre@atmel.com>
 *
 *  Modified for Telechips SoC from ehci-atmel.c
 *                     by Telechips Team Linux <linux@telechips.com> 25-01-2011
 *
 *  Based on various ehci-*.c drivers
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/dma-mapping.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <linux/module.h>
#include "ehci.h"
#include "tcc-hcd.h"

#define DRIVER_DESC "USB 2.0 'Enhanced' Host Controller (EHCI-TCC) Driver"

#define tcc_ehci_readl(r)	(readl((r)))
#define tcc_ehci_writel(v, r)	(writel((v), (r)))

/* TCC897x USB PHY */
#define TCC_EHCI_PHY_BCFG	(0x00)
#define TCC_EHCI_PHY_PCFG0	(0x04)
#define TCC_EHCI_PHY_PCFG1	(0x08)
#define TCC_EHCI_PHY_PCFG2	(0x0C)
#define TCC_EHCI_PHY_PCFG3	(0x10)
#define TCC_EHCI_PHY_PCFG4	(0x14)
#define TCC_EHCI_PHY_STS	(0x18)
#define TCC_EHCI_PHY_LCFG0	(0x1C)
#define TCC_EHCI_PHY_LCFG1	(0x20)
#define TCC_EHCI_HSIO_CFG	(0x40)

static const char hcd_name[] = "tcc-ehci";

struct tcc_ehci_hcd {
	struct ehci_hcd *ehci;
	struct device *dev;
	int32_t hosten_ctrl_able;
	int32_t host_en_gpio;

	int32_t vbus_ctrl_able;
	int32_t vbus_gpio;

	uint32_t vbus_status;

	uint32_t TXVRT;
	uint32_t TXRISET;
	uint32_t TXAT;

	int32_t vbus_source_ctrl;
	struct regulator *vbus_source;

	struct clk *hclk;
	struct clk *pclk;
	struct clk *phy_clk;
	uint32_t core_clk_rate;
	uint32_t core_clk_rate_phy;
	struct clk *isol;

	struct tcc_usb_phy *phy;
	struct usb_phy *transceiver;
	int32_t host_resumed;
	int32_t port_resuming;

	/* USB PHY */
	void __iomem *phy_regs; /* device memory/io */

	uint32_t hcd_tpl_support; /* TPL support */
};

static int32_t tcc_ehci_parse_dt(struct platform_device *pdev,
		struct tcc_ehci_hcd *tcc_ehci);
static void tcc_ehci_phy_init(struct tcc_ehci_hcd *tcc_ehci);

/** @name Functions for Show/Store of Attributes */
enum {
	TXVRT,
	CDT,
	TXPPT,
	TP,
	TXRT,
	TXREST,
	TXHSXVT,
	PCFG1_MAX
};

struct pcfg1_unit {
	char *reg_name;
	uint32_t offset;
	uint32_t mask;
	char str[256];
};

struct pcfg1_unit USB20_PCFG1[7] = {
	/* name, offset, mask */
	{"TXVRT  ",  0, (0xF<<0)},
	{"CDT    ",  4, (0x7<<4)},
	{"TXPPT  ",  7, (0x1<<7)},
	{"TP     ",  8, (0x3<<8)},
	{"TXRT   ", 10, (0x3<<10)},
	{"TXREST ", 12, (0x3<<12)},
	{"TXHSXVT", 14, (0x3<<14)},
};
EXPORT_SYMBOL_GPL(USB20_PCFG1);

int32_t ehci_phy_set = -1;
EXPORT_SYMBOL_GPL(ehci_phy_set);

static void tcc_ehci_phy_ctrl(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off)
{
	struct usb_phy *phy;

	if (tcc_ehci == NULL) {
		return;
	}

	phy = tcc_ehci->transceiver;

	if (!phy || !phy->set_phy_state) {
		pr_info("[INFO][USB] [%s:%d]Phy driver is needed\n",
				__func__, __LINE__);
	} else {
		phy->set_phy_state(phy, on_off);
		ehci_phy_set = 1;
	}
}

int32_t tcc_ehci_clk_ctrl(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off)
{
	if (tcc_ehci == NULL) {
		return -ENODEV;
	}

	if (on_off == ON) {
		if (tcc_ehci->hclk != NULL) {
			if (clk_prepare_enable(tcc_ehci->hclk) != 0) {
				dev_err(tcc_ehci->dev,
						"[ERROR][USB] can't do usb 2.0 hclk clock enable\n");
				return -1;
			}
		}

		if (tcc_ehci->pclk != NULL) {
			if (clk_prepare_enable(tcc_ehci->pclk) != 0) {
				dev_err(tcc_ehci->dev,
						"[ERROR][USB] can't do usb 2.0 hclk clock enable\n");
				return -1;
			}

			clk_set_rate(tcc_ehci->pclk, tcc_ehci->core_clk_rate);
		}
	} else {
		if (tcc_ehci->hclk) {
			if (__clk_is_enabled(tcc_ehci->hclk)) {
				clk_disable_unprepare(tcc_ehci->hclk);
			}
		}

		if (tcc_ehci->pclk) {
			if (__clk_is_enabled(tcc_ehci->pclk)) {
				clk_disable_unprepare(tcc_ehci->pclk);
			}
		}
	}

	return 0;
}

static char *ehci_pcfg1_display(uint32_t old_reg, uint32_t new_reg, char *str)
{
	ulong new_val;
	ulong old_val;
	int32_t i;

	for (i = 0; i < (int32_t)PCFG1_MAX; i++) {
		old_val = (ISSET(old_reg, USB20_PCFG1[i].mask)) >>
			(USB20_PCFG1[i].offset);
		new_val = (ISSET(new_reg, USB20_PCFG1[i].mask)) >>
			(USB20_PCFG1[i].offset);

		if (old_val != new_val) {
			sprintf(USB20_PCFG1[i].str,
					"%s = 0x%lX -> 0x%lX\n",
					USB20_PCFG1[i].reg_name,
					old_val, new_val);
		} else {
			sprintf(USB20_PCFG1[i].str, "%s = 0x%lX\n",
					USB20_PCFG1[i].reg_name, old_val);
		}

		strncat(str, USB20_PCFG1[i].str, 256 - strlen(str) - 1);
	}

	return str;
}

/*
 * Show the current value of the USB20H PHY Configuration 1 Register(U20H_PCFG1)
 */
static ssize_t ehci_pcfg1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char str[256] = {0};
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	uint32_t reg = tcc_ehci_readl(tcc_ehci->phy_regs+TCC_EHCI_PHY_PCFG1);

	return sprintf(buf, "USB20H_PCFG1 = 0x%08X\n%s", reg,
			ehci_pcfg1_display(reg, reg, str));
}

/*
 * HS DC Voltage Level is set
 */
static ssize_t ehci_pcfg1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	uint32_t old_reg = readl(tcc_ehci->phy_regs + TCC_EHCI_PHY_PCFG1);
	uint32_t new_reg = simple_strtoul(buf, NULL, 16);
	char str[256] = {0};
	int32_t i;

	if (((count - 1U) < 10U) || (10U < (count - 1U))) {
		pr_info("[INFO][USB]\nThis argument length is not 10\n\n");
		pr_info("\tUsage : echo 0xXXXXXXXX > ehci_pcfg1\n\n");
		pr_info("\t\t1) length of 0xXXXXXXXX is 10\n");
		pr_info("\t\t2) X is hex number(0 to f)\n\n");
		return (ssize_t)count;
	}

	if (buf == NULL) {
		return -ENXIO;
	}

	if ((buf[0] != '0') || (buf[1] != 'x')) {
		pr_info("[INFO][USB]\n\techo %c%cXXXXXXXX is not Ox\n\n",
				buf[0], buf[1]);
		pr_info("\tUsage : echo 0xXXXXXXXX > ehci_pcfg1\n\n");
		pr_info("\t\t1) 0 is binary number)\n\n");
		return (ssize_t)count;
	}

	for (i = 2; i < 10; i++) {
		if (((buf[i] >= '0') && (buf[i] <= '9')) ||
				((buf[i] >= 'a') && (buf[i] <= 'f')) ||
				((buf[i] >= 'A') && (buf[i] <= 'F'))) {
			continue;
		} else {
			pr_info("[INFO][USB]\necho 0x%c%c%c%c%c%c%c%c is not hex\n\n",
					buf[2], buf[3], buf[4], buf[5],
					buf[6], buf[7], buf[8], buf[9]);
			pr_info("\tUsage : echo 0xXXXXXXXX > ehci_pcfg1\n\n");
			pr_info("\t\t2) X is hex number(0 to f)\n\n");

			return (ssize_t)count;
		}
	}

	pr_info("[INFO][USB]\nUSB20H_PCFG1 = 0x%08X\n", old_reg);
	tcc_ehci_writel(new_reg, tcc_ehci->phy_regs + TCC_EHCI_PHY_PCFG1);
	new_reg = tcc_ehci_readl(tcc_ehci->phy_regs + TCC_EHCI_PHY_PCFG1);

	ehci_pcfg1_display(old_reg, new_reg, str);
	pr_info("[INFO][USB]\n%sUSB20H_PCFG1 = 0x%08X\n",
			str, new_reg);

	return (ssize_t)count;
}

DEVICE_ATTR(ehci_pcfg1, 0644, ehci_pcfg1_show, ehci_pcfg1_store);

#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static uint32_t vbus_control_enable = 1;
#else
static uint32_t vbus_control_enable;
#endif
module_param(vbus_control_enable, uint, 0644);
MODULE_PARM_DESC(vbus_control_enable, "ehci vbus control enable");

int32_t tcc_ehci_vbus_ctrl(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off)
{
	struct usb_phy *phy;

	if (tcc_ehci == NULL) {
		return -ENODEV;
	}

	phy = tcc_ehci->transceiver;

	if (vbus_control_enable == 0U) {
		pr_info("[INFO][USB] ehci vbus ctrl disable.\n");
		return -1;
	}

	if (!phy) {
		pr_info("[INFO][USB] [%s:%d]Phy driver is needed\n",
				__func__, __LINE__);
		return -1;
	}

	tcc_ehci->vbus_status = (uint32_t)on_off;

	return phy->set_vbus(phy, on_off);
}

static ssize_t ehci_tcc_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	return sprintf(buf, "ehci vbus - %s\n",
			((tcc_ehci->vbus_status) != 0U) ? "on" : "off");
}

static ssize_t ehci_tcc_vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	if (strncmp(buf, "on", 2) == 0) {
		tcc_ehci_vbus_ctrl(tcc_ehci, ON);
	}

	if (strncmp(buf, "off", 3) == 0) {
		tcc_ehci_vbus_ctrl(tcc_ehci, OFF);
	}

	return (ssize_t)count;
}

static DEVICE_ATTR(vbus, 0644, ehci_tcc_vbus_show, ehci_tcc_vbus_store);

/* Group all the eHCI SQ attributes */
static struct attribute *usb_sq_attrs[] = {
		&dev_attr_ehci_pcfg1.attr,
		&dev_attr_vbus.attr,
		NULL,
};

static struct attribute_group usb_sq_attr_group = {
	.name = NULL,	/* we want them in the same directory */
	.attrs = usb_sq_attrs,
};

/* TPL Support Attribute */
static ssize_t ehci_tpl_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	if (hcd == NULL) {
		return -ENODEV;
	}

	return sprintf(buf, "tpl support : %s\n",
			(hcd->tpl_support != 0U) ? "on" : "off");
}

static ssize_t ehci_tpl_support_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	if (hcd == NULL) {
		return -ENODEV;
	}

	if (strncmp(buf, "on", 2) == 0) {
		tcc_ehci->hcd_tpl_support = ON;
	}

	if (strncmp(buf, "off", 3) == 0) {
		tcc_ehci->hcd_tpl_support = OFF;
	}

	hcd->tpl_support = tcc_ehci->hcd_tpl_support;

	return (ssize_t)count;
}

DEVICE_ATTR(ehci_tpl_support, 0644,
		ehci_tpl_support_show, ehci_tpl_support_store);

/*
 * tcc ehci phy configruation set function.
 */
static void tcc_ehci_phy_init(struct tcc_ehci_hcd *tcc_ehci)
{
	struct usb_phy *phy = tcc_ehci->transceiver;

	if (phy == NULL) {
		return;
	}

	if (!phy->init && !phy) {
		pr_info("[INFO][USB] [%s:%d]Phy driver is needed\n",
				__func__, __LINE__);
	} else {
		phy->init(phy);
	}
}

int32_t tcc_ehci_power_ctrl(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off)
{
	int32_t err = 0;

	if (tcc_ehci == NULL) {
		return -ENODEV;
	}

	if (tcc_ehci->vbus_source_ctrl == 1) {
		if (on_off == ON) {
			if (tcc_ehci->vbus_source != NULL) {
				err = regulator_enable(tcc_ehci->vbus_source);

				if (err != 0) {
					dev_err(tcc_ehci->dev, "[ERROR][USB] can't enable vbus source\n");
					return err;
				}
			}
		}
	}

	if (tcc_ehci->hosten_ctrl_able == 1) {
		/*
		 * Don't control gpio_hs_host_en because this power also
		 * supported in USB core.
		 */
		err = (int32_t)gpio_direction_output(tcc_ehci->host_en_gpio, 1);

		if (err != 0) {
			dev_err(tcc_ehci->dev, "[ERROR][USB] can't enable host\n");
			return err;
		}
	}

	if (tcc_ehci->vbus_source_ctrl == 1) {
		if (on_off == OFF) {
			if (tcc_ehci->vbus_source != NULL) {
				regulator_disable(tcc_ehci->vbus_source);
			}
		}
	}

	return err;
}

#ifdef CONFIG_PM
static int32_t tcc_ehci_suspend(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);
	bool do_wakeup = device_may_wakeup(dev);

	ehci_suspend(hcd, do_wakeup);

	/* Telechips specific routine */
#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC898X) ||	\
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X) ||	\
	defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC805X)
	tcc_ehci_phy_ctrl(tcc_ehci, OFF);
	tcc_ehci_clk_ctrl(tcc_ehci, OFF);
	tcc_ehci_vbus_ctrl(tcc_ehci, OFF);
	tcc_ehci_power_ctrl(tcc_ehci, OFF);
#else
#if !defined(CONFIG_USB_OHCI_HCD) && !defined(CONFIG_USB_OHCI_HCD_MODULE)
	tcc_ehci_phy_ctrl(tcc_ehci, OFF);
#endif
	tcc_ehci_clk_ctrl(tcc_ehci, OFF);
#if !defined(CONFIG_USB_OHCI_HCD) && !defined(CONFIG_USB_OHCI_HCD_MODULE)
	tcc_ehci_vbus_ctrl(tcc_ehci, OFF);
#endif
	tcc_ehci_power_ctrl(tcc_ehci, OFF);
#endif

	return 0;
}

static int32_t tcc_ehci_resume(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	/* Telechips specific routine */
#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC898X) ||	\
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X) ||	\
	defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC805X)
	tcc_ehci_power_ctrl(tcc_ehci, ON);
	tcc_ehci_phy_ctrl(tcc_ehci, ON);
	tcc_ehci_vbus_ctrl(tcc_ehci, ON);
	tcc_ehci_clk_ctrl(tcc_ehci, ON);
	tcc_ehci_phy_init(tcc_ehci);
#else
	tcc_ehci_power_ctrl(tcc_ehci, ON);
#if !defined(CONFIG_USB_OHCI_HCD) && !defined(CONFIG_USB_OHCI_HCD_MODULE)
	tcc_ehci_phy_ctrl(tcc_ehci, ON);
	tcc_ehci_vbus_ctrl(tcc_ehci, ON);
#endif
	tcc_ehci_clk_ctrl(tcc_ehci, ON);
#if !defined(CONFIG_USB_OHCI_HCD) && !defined(CONFIG_USB_OHCI_HCD_MODULE)
	tcc_ehci_phy_init(tcc_ehci);
#endif
#endif

	/*
	 * for compatibility issue(suspend/resume). Some usb devices are failed
	 * to connect when resume.
	 */
	usleep_range(1000, 2000);
	ehci_resume(hcd, false);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ehci_tcc_pmops, tcc_ehci_suspend, tcc_ehci_resume);
#define EHCI_TCC_PMOPS (&ehci_tcc_pmops)
#else
#define EHCI_TCC_PMOPS (NULL)
#endif	/* CONFIG_PM */

/*
 * configure so an HC device and id are always provided and called with process
 * context; sleeping is OK
 */
static struct hc_driver __read_mostly ehci_tcc_hc_driver;

static int32_t ehci_tcc_drv_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct tcc_ehci_hcd *tcc_ehci;
	struct resource *res;
	int32_t irq;
	int32_t retval;

	if (usb_disabled() != 0) {
		return -ENODEV;
	}

	retval = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (retval != 0)
		return retval;

	tcc_ehci = devm_kzalloc(&pdev->dev, sizeof(struct tcc_ehci_hcd),
			GFP_KERNEL);

	if (!tcc_ehci) {
		retval = -ENOMEM;
		goto fail_create_hcd;
	}

	/* Parsing the device table */
	retval = tcc_ehci_parse_dt(pdev, tcc_ehci);
	if (retval != 0) {
		/*
		 * If the return value of tcc_ehci_parse_dt() is -EPROBE_DEFER,
		 * the VBus GPIO number is set to an invalid number in
		 * set_vbus_resource() called in tcc_ehci_parse_dt(). In such a
		 * case, set the value of the ehci_phy_set variable to
		 * -EPROBE_DEFER and usb_hcd_tcc_probe() in ohci-tcc.c check the
		 * value of the ehci_phy_set variable. If the value of the
		 * ehci_phy_set variable is -EPROBE_DEFER, the
		 * usb_hcd_tcc_probe() also fails.
		 */
		if (retval == -EPROBE_DEFER) {
			ehci_phy_set = -EPROBE_DEFER;
		} else if (retval == -1) {
			retval = -EIO;
		} else {
			dev_err(&pdev->dev, "[ERROR][USB] Device table parsing failed.\n");
			retval = -EIO;
		}

		goto fail_create_hcd;
	}

	irq = platform_get_irq(pdev, 0);

	if (irq <= 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(&pdev->dev));
		retval = -ENODEV;
		goto fail_create_hcd;
	}

	hcd = usb_create_hcd(&ehci_tcc_hc_driver, &pdev->dev,
			dev_name(&pdev->dev));

	if (!hcd) {
		retval = -ENOMEM;
		goto fail_create_hcd;
	}

	platform_set_drvdata(pdev, tcc_ehci);
	tcc_ehci->dev = &(pdev->dev);

	/* USB ECHI Base Address*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no register addr. Check %s setup!\n",
				dev_name(&pdev->dev));
		retval = -ENODEV;
		goto fail_request_resource;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1U;
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);

	tcc_ehci_clk_ctrl(tcc_ehci, ON);

	/* USB HS Phy Enable */
	tcc_ehci_phy_ctrl(tcc_ehci, ON);

	/* USB HOST Power Enable */
	if (tcc_ehci_power_ctrl(tcc_ehci, ON) != 0) {
		pr_err("[ERROR][USB] ehci-tcc: USB HOST VBUS Ctrl failed\n");
		retval = -EIO;
		goto fail_request_resource;
	}

	tcc_ehci_phy_init(tcc_ehci);

	tcc_ehci_vbus_ctrl(tcc_ehci, ON);

	/* ehci setup */
	tcc_ehci->ehci = hcd_to_ehci(hcd);

	/* registers start at offset 0x0 */
	tcc_ehci->ehci->caps = hcd->regs;
	tcc_ehci->ehci->regs = hcd->regs + HC_LENGTH(tcc_ehci->ehci,
			ehci_readl(tcc_ehci->ehci,
				&tcc_ehci->ehci->caps->hc_capbase));

	/* cache this readonly data; minimize chip reads */
	tcc_ehci->ehci->hcs_params = ehci_readl(tcc_ehci->ehci,
			&tcc_ehci->ehci->caps->hcs_params);

	/* TPL Support Set */
	hcd->tpl_support = tcc_ehci->hcd_tpl_support;

	retval = (int32_t)usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (retval != 0) {
		goto fail_add_hcd;
	}

	/* connect the hcd phy pointer */
	hcd->usb_phy = tcc_ehci->transceiver;

	retval = sysfs_create_group(&pdev->dev.kobj, &usb_sq_attr_group);
	if (retval < 0) {
		pr_err("[ERROR][USB] Cannot register USB SQ sysfs attributes: %d\n",
				retval);
		goto fail_add_hcd;
	}

	retval = device_create_file(&pdev->dev, &dev_attr_ehci_tpl_support);
	if (retval < 0) {
		pr_err("[ERROR][USB] Cannot register USB TPL Support attributes: %d\n",
				retval);
		goto fail_add_hcd;
	}

	return retval;

fail_add_hcd:
	tcc_ehci_clk_ctrl(tcc_ehci, OFF);
fail_request_resource:
	usb_put_hcd(hcd);
fail_create_hcd:
	dev_err(&pdev->dev, "[ERROR][USB] init %s fail, %d\n",
			dev_name(&pdev->dev), retval);

	return retval;
}

static int32_t __exit ehci_tcc_drv_remove(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	sysfs_remove_group(&pdev->dev.kobj, &usb_sq_attr_group);

	device_remove_file(&pdev->dev, &dev_attr_ehci_tpl_support);

	ehci_shutdown(hcd);
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	tcc_ehci_phy_ctrl(tcc_ehci, OFF);
	tcc_ehci_clk_ctrl(tcc_ehci, OFF);
	tcc_ehci_vbus_ctrl(tcc_ehci, OFF);
	tcc_ehci_power_ctrl(tcc_ehci, OFF);

	gpio_free((uint32_t)tcc_ehci->host_en_gpio);
	gpio_free((uint32_t)tcc_ehci->vbus_gpio);

	if (tcc_ehci->vbus_source != NULL) {
		regulator_disable(tcc_ehci->vbus_source);
		regulator_put(tcc_ehci->vbus_source);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_ehci_match[] = {
	{ .compatible = "telechips,tcc-ehci" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_ehci_match);

static int32_t tcc_ehci_parse_dt(struct platform_device *pdev,
		struct tcc_ehci_hcd *tcc_ehci)
{
	int32_t err = 0;

	// Check Host enable pin
	if (of_find_property(pdev->dev.of_node, "hosten-ctrl-able", NULL) != NULL) {
		tcc_ehci->hosten_ctrl_able = 1;
		tcc_ehci->host_en_gpio = of_get_named_gpio(pdev->dev.of_node,
				"hosten-gpio", 0);

		if (!gpio_is_valid(tcc_ehci->host_en_gpio)) {
			dev_err(&pdev->dev, "[ERROR][USB] can't find dev of node: host en gpio\n");

			return -ENODEV;
		}

		err = gpio_request((uint32_t)tcc_ehci->host_en_gpio, "host_en_gpio");

		if (err != 0) {
			dev_err(&pdev->dev, "[ERROR][USB] can't requeest host_en gpio\n");

			return err;
		}
	} else {
		tcc_ehci->hosten_ctrl_able = 0;	// can not control vbus
	}

	// Check vbus enable pin
	if (of_find_property(pdev->dev.of_node, "telechips,ehci_phy", NULL) != NULL) {
		tcc_ehci->transceiver = devm_usb_get_phy_by_phandle(&pdev->dev,
				"telechips,ehci_phy", 0);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		err = tcc_ehci->transceiver->set_vbus_resource(
				tcc_ehci->transceiver);
#endif
		if (IS_ERR(tcc_ehci->transceiver)) {
			if (PTR_ERR(tcc_ehci->transceiver) == -EPROBE_DEFER) {
				err = -EPROBE_DEFER;
			} else {
				err = -ENODEV;
			}

			tcc_ehci->transceiver = NULL;

			return err;
		}

		tcc_ehci->phy_regs = tcc_ehci->transceiver->base;
	}

	// Check VBUS Source enable
	if (of_find_property(pdev->dev.of_node, "vbus-source-ctrl", NULL) != NULL) {
		tcc_ehci->vbus_source_ctrl = 1;
		tcc_ehci->vbus_source = regulator_get(&pdev->dev, "vdd_v5p0");

		if (IS_ERR(tcc_ehci->vbus_source)) {
			dev_err(&pdev->dev, "[ERROR][USB] failed to get ehci vdd_source\n");
			tcc_ehci->vbus_source = NULL;
		}
	} else {
		tcc_ehci->vbus_source_ctrl = 0;
	}

	tcc_ehci->hclk = of_clk_get(pdev->dev.of_node, 0);

	if (IS_ERR(tcc_ehci->hclk)) {
		tcc_ehci->hclk = NULL;
	}

	tcc_ehci->isol = of_clk_get(pdev->dev.of_node, 1);

	if (IS_ERR(tcc_ehci->isol)) {
		tcc_ehci->isol = NULL;
	}

	tcc_ehci->pclk = of_clk_get(pdev->dev.of_node, 2);

	if (IS_ERR(tcc_ehci->pclk)) {
		tcc_ehci->pclk = NULL;
	}

	tcc_ehci->phy_clk = of_clk_get(pdev->dev.of_node, 3);

	if (IS_ERR(tcc_ehci->phy_clk)) {
		tcc_ehci->phy_clk = NULL;
	}

	of_property_read_u32(pdev->dev.of_node, "clock-frequency",
			&tcc_ehci->core_clk_rate);
	of_property_read_u32(pdev->dev.of_node, "clock-frequency-phy",
			&tcc_ehci->core_clk_rate_phy);

	// Check TPL Support
	if (of_usb_host_tpl_support(pdev->dev.of_node)) {
		tcc_ehci->hcd_tpl_support = 1;
	} else {
		tcc_ehci->hcd_tpl_support = 0;
	}

	return err;
}
#else
static int32_t tcc_ehci_parse_dt(struct platform_device *pdev,
		struct tcc_ehci_hcd *tcc_ehci)
{
	return 0;
}
#endif /* CONFIG_OF */

static void tcc_ehci_hcd_shutdown(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	if (hcd == NULL) {
		return;
	}

	if (hcd->driver->shutdown != NULL) {
		hcd->driver->shutdown(hcd);
	}
}

static struct platform_driver ehci_tcc_driver = {
	.probe		= ehci_tcc_drv_probe,
	.remove		= __exit_p(ehci_tcc_drv_remove),
	.shutdown	= tcc_ehci_hcd_shutdown,
	.driver = {
		.name		= "tcc-ehci",
		.owner		= THIS_MODULE,
		.pm		= EHCI_TCC_PMOPS,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_ehci_match),
#endif
	}
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static int32_t __init tcc_ehci_hcd_init(void)
{
	int32_t retval = 0;

	ehci_init_driver(&ehci_tcc_hc_driver, NULL);
	set_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	retval = platform_driver_register(&ehci_tcc_driver);
	if (retval < 0) {
		clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
	}

	return retval;
}
module_init(tcc_ehci_hcd_init);

static void __exit tcc_ehci_hcd_cleanup(void)
{
	platform_driver_unregister(&ehci_tcc_driver);
	clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
}
module_exit(tcc_ehci_hcd_cleanup);
