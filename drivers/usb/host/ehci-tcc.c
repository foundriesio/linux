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
#include "tcc-hcd.h"
#include <asm/io.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/dma-mapping.h>
#include "ehci.h"
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <linux/module.h>

#define DRIVER_DESC "USB 2.0 'Enhanced' Host Controller (EHCI-TCC) Driver"
//#define ON	1
//#define OFF 0
//
//#define	TCC_EHCI_ISSET(X, MASK)				( (unsigned long)(X) & ((unsigned long)(MASK)) )
static const char hcd_name[] = "tcc-ehci";

#define tcc_ehci_readl(r)		readl(r)
#define tcc_ehci_writel(v,r)		writel(v,r)

/* TCC897x USB PHY */
#define TCC_EHCI_PHY_BCFG			0x00
#define TCC_EHCI_PHY_PCFG0			0x04
#define TCC_EHCI_PHY_PCFG1			0x08
#define TCC_EHCI_PHY_PCFG2			0x0C
#define TCC_EHCI_PHY_PCFG3			0x10
#define TCC_EHCI_PHY_PCFG4			0x14
#define TCC_EHCI_PHY_STS			0x18
#define TCC_EHCI_PHY_LCFG0			0x1C
#define TCC_EHCI_PHY_LCFG1			0x20
#define TCC_EHCI_HSIO_CFG			0x40

struct tcc_ehci_hcd {
	struct ehci_hcd *ehci;
	struct device			*dev;
	int	hosten_ctrl_able;
	int host_en_gpio;

	int vbus_ctrl_able;
	int vbus_gpio;

	unsigned int vbus_status;

	unsigned int TXVRT;
	unsigned int TXRISET;
	unsigned int TXAT;

	int vbus_source_ctrl;
	struct regulator *vbus_source;

	struct clk *hclk;
	struct clk *pclk;
	struct clk *phy_clk;
	unsigned int core_clk_rate;
	unsigned int core_clk_rate_phy;
	struct clk *isol;

	struct tcc_usb_phy *phy;
	struct usb_phy *transceiver;
	int host_resumed;
	int port_resuming;

	/* USB PHY */
	void __iomem		*phy_regs;		/* device memory/io */

	unsigned int hcd_tpl_support;	/* TPL support */
};

extern unsigned long usb_hcds_loaded;
extern const struct hc_driver* get_ehci_hcd_driver(void);

static int tcc_ehci_parse_dt(struct platform_device *pdev, struct tcc_ehci_hcd *tcc_ehci);
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
	char* 		reg_name;
	uint32_t	offset;
	uint32_t	mask;
	char str[256];
};

struct pcfg1_unit USB20_PCFG1[] =
{
	/*name, 	offset, mask*/
	{"TXVRT  ",	0,		(0xF<<0)},
	{"CDT    ",	4,		(0x7<<4)},
	{"TXPPT  ",	7,		(0x1<<7)},
	{"TP     ",	8,		(0x3<<8)},
	{"TXRT   ",	10,		(0x3<<10)},
	{"TXREST ",	12,		(0x3<<12)},
	{"TXHSXVT", 14,		(0x3<<14)},
};

int ehci_phy_set = 0;
EXPORT_SYMBOL_GPL(ehci_phy_set);

static void tcc_ehci_phy_ctrl(struct tcc_ehci_hcd *tcc_ehci, int on_off)
{
	struct usb_phy *phy = tcc_ehci->transceiver;

	if (!phy || !phy->set_phy_state) {
		printk("[%s:%d]Phy driver is needed\n", __func__, __LINE__);
	} else {
		phy->set_phy_state(phy, on_off);
		ehci_phy_set = 1;
	}

#if 0		/* 017.03.02 */
	if(on_off == ON) {
		if(tcc_ehci->isol) {
			if(clk_prepare_enable(tcc_ehci->isol) != 0) {
				dev_err(tcc_ehci->dev,
				"can't do usb 2.0 phy enable\n");
			}
		}
		if(tcc_ehci->phy_clk) {
			if(clk_prepare_enable(tcc_ehci->phy_clk) != 0) {
				dev_err(tcc_ehci->dev,
					"can't do usb 2.0 phy clk clock enable\n");
			}
			clk_set_rate(tcc_ehci->phy_clk, tcc_ehci->core_clk_rate_phy);
		}
		ehci_phy_set = 1;
	}
	else if(on_off == OFF) {
		if(tcc_ehci->phy_clk) {
			clk_disable_unprepare(tcc_ehci->phy_clk);
		}
		if(tcc_ehci->isol) {
			clk_disable_unprepare(tcc_ehci->isol);
		}

		ehci_phy_set = 0;
	}
#endif /* 0 */
}

/*-------------------------------------------------------------------------*/
int tcc_ehci_clk_ctrl(struct tcc_ehci_hcd *tcc_ehci, int on_off)
{
	if(on_off == ON) {
		if(tcc_ehci->hclk) {
			if(clk_prepare_enable(tcc_ehci->hclk) != 0) {
				dev_err(tcc_ehci->dev,
					"can't do usb 2.0 hclk clock enable\n");
				return -1;
			}
		}
		if(tcc_ehci->pclk) {
			if(clk_prepare_enable(tcc_ehci->pclk) != 0) {
				dev_err(tcc_ehci->dev,
					"can't do usb 2.0 hclk clock enable\n");
				return -1;
			}
			clk_set_rate(tcc_ehci->pclk, tcc_ehci->core_clk_rate);
		}
		//printk("usb host 2.0 clk rate: %d\n", tcc_ehci->core_clk_rate);
	} else {
		if(tcc_ehci->hclk && __clk_is_enabled(tcc_ehci->hclk))
			clk_disable_unprepare(tcc_ehci->hclk);
		if(tcc_ehci->pclk && __clk_is_enabled(tcc_ehci->pclk))
			clk_disable_unprepare(tcc_ehci->pclk);
	}

	return 0;
}

static char* ehci_pcfg1_display(uint32_t old_reg, uint32_t new_reg, char* str)
{
	uint32_t new_val, old_val;
	int i;

	for(i=0;i<PCFG1_MAX;i++){
		old_val = (ISSET(old_reg, USB20_PCFG1[i].mask)) >> (USB20_PCFG1[i].offset);
		new_val = (ISSET(new_reg, USB20_PCFG1[i].mask)) >> (USB20_PCFG1[i].offset);

		if(old_val != new_val) {
			sprintf(USB20_PCFG1[i].str, "%s = 0x%X -> \x1b[1;33m0x%X\x1b[1;0m*\n",
										USB20_PCFG1[i].reg_name, old_val,new_val);
		} else
			sprintf(USB20_PCFG1[i].str, "%s = 0x%X\n", USB20_PCFG1[i].reg_name, old_val);

		strcat(str,USB20_PCFG1[i].str);
	}

	return str;
}
/**
 * Show the current value of the USB20H PHY Configuration 1 Register (U20H_PCFG1)
 */
static ssize_t ehci_pcfg1_show(struct device *_dev,
	          struct device_attribute *attr, char *buf)
{
	//PHSIOBUSCFG pEHCIPHYCFG = (PHSIOBUSCFG)tcc_p2v(HwHSIOBUSCFG_BASE);
	//uint32_t reg = readl(&pEHCIPHYCFG->USB20H_PCFG1.nREG);

	char str[256]={0};
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(_dev);
	uint32_t reg = tcc_ehci_readl(tcc_ehci->phy_regs+TCC_EHCI_PHY_PCFG1);

	return sprintf(buf,
		    "USB20H_PCFG1 = 0x%08X\n%s", reg, ehci_pcfg1_display(reg,reg,str));
}

/**
 * HS DC Voltage Level is set
 */
static ssize_t ehci_pcfg1_store(struct device *_dev,
	           struct device_attribute *attr,
	           const char *buf, size_t count)
{
	//PHSIOBUSCFG pEHCIPHYCFG = (PHSIOBUSCFG)tcc_p2v(HwHSIOBUSCFG_BASE);
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(_dev);
	uint32_t old_reg = readl(tcc_ehci->phy_regs + TCC_EHCI_PHY_PCFG1);
	//readl(&pEHCIPHYCFG->USB20H_PCFG1.nREG);
	uint32_t new_reg = simple_strtoul(buf, NULL, 16);
	char str[256]={0};
	int i;

	if(count - 1 < 10 || 10 < count - 1 ) {
		printk("\nThis argument length is \x1b[1;33mnot 10\x1b[0m\n\n");
		printk("\tUsage : echo \x1b[1;31m0xXXXXXXXX\x1b[0m > ehci_pcfg1\n\n");
		printk("\t\t1) length of \x1b[1;32m0xXXXXXXXX\x1b[0m is 10\n");
		printk("\t\t2) \x1b[1;32mX\x1b[0m is hex number(\x1b[1;31m0\x1b[0m to \x1b[1;31mf\x1b[0m)\n\n");
		return count;
	}
	if((buf[0] != '0') || (buf[1] != 'x')) {
		printk("\n\techo \x1b[1;32m%c%c\x1b[1;0mXXXXXXXX is \x1b[1;33mnot Ox\x1b[0m\n\n", buf[0], buf[1]);
		printk("\tUsage : echo \x1b[1;32m0x\x1b[1;31mXXXXXXXX\x1b[0m > ehci_pcfg1\n\n");
		printk("\t\t1) \x1b[1;32m0\x1b[0m is binary number\x1b[0m)\n\n");
		return count;
	}

	for(i=2;i<10;i++) {
		if( (buf[i]>='0'&&buf[i]<='9') ||
			(buf[i]>='a'&&buf[i]<='f') ||
			(buf[i]>='A'&&buf[i]<='F') )
			continue;
		else {
			printk("\necho 0x%c%c%c%c%c%c%c%c is \x1b[1;33mnot hex\x1b[0m\n\n", 
				buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]);
			printk("\tUsage : echo \x1b[1;31m0xXXXXXXXX\x1b[0m > ehci_pcfg1\n\n");
			printk("\t\t2) \x1b[1;32mX\x1b[0m is hex number(\x1b[1;31m0\x1b[0m to \x1b[1;31mf\x1b[0m)\n\n");
			return count;
		}
	}

	printk("USB20H_PCFG1 = 0x%08X\n", old_reg);
	tcc_ehci_writel(new_reg, tcc_ehci->phy_regs + TCC_EHCI_PHY_PCFG1);
	new_reg = tcc_ehci_readl(tcc_ehci->phy_regs + TCC_EHCI_PHY_PCFG1);
	//writel(new_reg, &pEHCIPHYCFG->USB20H_PCFG1.nREG);
	//new_reg = readl(&pEHCIPHYCFG->USB20H_PCFG1.nREG);

	ehci_pcfg1_display(old_reg,new_reg,str);
	printk("%sUSB20H_PCFG1 = \x1b[1;33m0x%08X\x1b[1;0m\n",str, new_reg);

	return count;
}

DEVICE_ATTR(ehci_pcfg1, S_IRUGO | S_IWUSR, ehci_pcfg1_show, ehci_pcfg1_store);

int tcc_ehci_vbus_ctrl(struct tcc_ehci_hcd *tcc_ehci, int on_off);
static ssize_t ehci_tcc_vbus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	return sprintf(buf, "ehci vbus - %s\n",(tcc_ehci->vbus_status) ? "on":"off");
}

static ssize_t ehci_tcc_vbus_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2)) {
		tcc_ehci_vbus_ctrl(tcc_ehci, ON);
	}

	if (!strncmp(buf, "off", 3)) {
		tcc_ehci_vbus_ctrl(tcc_ehci, OFF);
	}

	return count;
}

static DEVICE_ATTR(vbus, S_IRUGO | S_IWUSR, ehci_tcc_vbus_show, ehci_tcc_vbus_store);

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
static ssize_t ehci_tpl_support_show(struct device *_dev,
	          struct device_attribute *attr, char *buf)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(_dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	return sprintf(buf, "tpl support : %s\n", hcd->tpl_support ? "on" : "off");
}
static ssize_t ehci_tpl_support_store(struct device *_dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(_dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	if (!strncmp(buf, "on", 2)) {
		tcc_ehci ->hcd_tpl_support = ON;
	}

	if (!strncmp(buf, "off", 3)) {
		tcc_ehci ->hcd_tpl_support = OFF;
	}

	hcd->tpl_support = tcc_ehci ->hcd_tpl_support;

	return count;
}
DEVICE_ATTR(ehci_tpl_support, S_IRUGO | S_IWUSR, ehci_tpl_support_show, ehci_tpl_support_store);

/*
 * tcc ehci phy configruation set fuction.
 */
static void tcc_ehci_phy_init(struct tcc_ehci_hcd *tcc_ehci)
{
	struct usb_phy *phy = tcc_ehci->transceiver;

	if (!phy && !phy->init)
		printk("[%s:%d]Phy driver is needed\n", __func__, __LINE__);
	else
		phy->init(phy);
}

int tcc_ehci_power_ctrl(struct tcc_ehci_hcd *tcc_ehci, int on_off)
{
	int err = 0;

	if (tcc_ehci->vbus_source_ctrl == 1) {
		if(on_off == ON) {
			if (tcc_ehci->vbus_source) {
				err = regulator_enable(tcc_ehci->vbus_source);
				if(err) {
					dev_err(tcc_ehci->dev,
						"can't enable vbus source\n");
					return err;
				}
			}
		}
	}

	if (tcc_ehci->hosten_ctrl_able == 1) {
		err = gpio_direction_output(tcc_ehci->host_en_gpio, 1);	/* Don't control gpio_hs_host_en because this power also supported in USB core. */
		if(err) {
			dev_err(tcc_ehci->dev,
				"can't enable host\n");
			return err;
		}
	}

	if (tcc_ehci->vbus_source_ctrl == 1) {
		if(on_off == OFF)
			if (tcc_ehci->vbus_source)
				regulator_disable(tcc_ehci->vbus_source);
	}
	
	return err;
}

#ifdef CONFIG_VBUS_CTRL_DEF_ENABLE		/* 017.08.30 */
static unsigned int vbus_control_enable = 1;  /* 2017/03/23, */
#else
static unsigned int vbus_control_enable = 0;  /* 2017/03/23, */
#endif /* CONFIG_VBUS_CTRL_DEF_ON */
module_param(vbus_control_enable, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vbus_control_enable, "ehci vbus control enable");

int tcc_ehci_vbus_ctrl(struct tcc_ehci_hcd *tcc_ehci, int on_off)
{
	struct usb_phy *phy = tcc_ehci->transceiver;

	if (!vbus_control_enable) {
		printk("ehci vbus ctrl disable.\n");
		return -1;
	}

	if (!phy || !phy->set_vbus) {
		printk("[%s:%d]Phy driver is needed\n", __func__, __LINE__);
		return -1;
	}

	tcc_ehci->vbus_status = on_off;

	return phy->set_vbus(phy, on_off);
}

/*-------------------------------------------------------------------------*/
#ifdef CONFIG_PM
static int tcc_ehci_suspend(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);
	bool do_wakeup = device_may_wakeup(dev);

	ehci_suspend(hcd, do_wakeup);

	/* Telechips specific routine */
	#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
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
	#endif /* 0 */

	return 0;
}

extern int ehci_reset(struct ehci_hcd *ehci);

static int tcc_ehci_resume(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	#if defined(CONFIG_ARCH_TCC896X)
	//writel(0x1, tcc_p2v(0x16370004));	// uart: suspend debug
	#endif

	/* Telechips specific routine */
	#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
	tcc_ehci_power_ctrl(tcc_ehci, ON);
	tcc_ehci_phy_ctrl(tcc_ehci, ON);
	tcc_ehci_vbus_ctrl(tcc_ehci, ON);
	tcc_ehci_clk_ctrl(tcc_ehci, ON);
	//tcc_ehci_phy_init(tcc_ehci);
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
	#endif /* 0 */

	//ehci_reset(tcc_ehci->ehci);
	msleep(1);//for compatibility issue(suspend/resume). Some usb devices are failed to connect when resume.
	ehci_resume(hcd, false);

	return 0;
}
static SIMPLE_DEV_PM_OPS(ehci_tcc_pmops, tcc_ehci_suspend,
	tcc_ehci_resume);
/*
static const struct dev_pm_ops ehci_tcc_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_ehci_suspend,tcc_ehci_resume)
};
*/
//void disp_ehci(unsigned int addr)
//{
//	unsigned int base = addr;
//
//	printk("\x1b[1;33m-------------------------------------------------\x1b[0m\n");
//	for(base = addr; base < (addr+0x108); base += 4)
//	{
//		printk("\x1b[1;33m0x%08X : [0x%08X]\x1b[0m\n",base,*(volatile unsigned int *)(base));
//	}
//	printk("\x1b[1;33m-------------------------------------------------\x1b[0m\n");
//}

#define EHCI_TCC_PMOPS &ehci_tcc_pmops
#else
#define EHCI_TCC_PMOPS NULL
#endif	/* CONFIG_PM */

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */
static struct hc_driver __read_mostly ehci_tcc_hc_driver;

static int ehci_tcc_drv_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct tcc_ehci_hcd *tcc_ehci;
	struct resource *res;
	int irq;
	int retval;

	if (usb_disabled())
		return -ENODEV;

	retval = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (retval)
		return retval;

	tcc_ehci = devm_kzalloc(&pdev->dev, sizeof(struct tcc_ehci_hcd), GFP_KERNEL);
	if (!tcc_ehci) {
		retval = -ENOMEM;
		goto fail_create_hcd;
	}

	/* Parsing the device table */
	retval = tcc_ehci_parse_dt(pdev, tcc_ehci);
	if(retval != 0){
		if(retval != -1)
			printk(KERN_ERR "ehci-tcc: Device table parsing failed\n");
		retval = -EIO;
		goto fail_create_hcd;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev,
			"Found HC with no IRQ. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto fail_create_hcd;
	}

	hcd = usb_create_hcd(&ehci_tcc_hc_driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto fail_create_hcd;
	}

	platform_set_drvdata(pdev, tcc_ehci);
	tcc_ehci->dev = &(pdev->dev);

	/* USB ECHI Base Address*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto fail_request_resource;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1;
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);

	tcc_ehci_clk_ctrl(tcc_ehci, ON);
	/* USB HS Phy Enable */
	tcc_ehci_phy_ctrl(tcc_ehci, ON);

	/* USB HOST Power Enable */
	if (tcc_ehci_power_ctrl(tcc_ehci, ON) != 0) {
		printk(KERN_ERR "ehci-tcc: USB HOST VBUS Ctrl failed\n");
		retval = -EIO;
		goto fail_request_resource;
	}
	
	tcc_ehci_phy_init(tcc_ehci);

	tcc_ehci_vbus_ctrl(tcc_ehci, ON);

	/* ehci setup */
	tcc_ehci->ehci = hcd_to_ehci(hcd);
	tcc_ehci->ehci->caps = hcd->regs;		/* registers start at offset 0x0 */
	tcc_ehci->ehci->regs = hcd->regs + HC_LENGTH(tcc_ehci->ehci, ehci_readl(tcc_ehci->ehci, &tcc_ehci->ehci->caps->hc_capbase));
	tcc_ehci->ehci->hcs_params = ehci_readl(tcc_ehci->ehci, &tcc_ehci->ehci->caps->hcs_params);	/* cache this readonly data; minimize chip reads */

	/* TPL Support Set */
	hcd->tpl_support = tcc_ehci->hcd_tpl_support;
	
	retval = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (retval)
		goto fail_add_hcd;
	//disp_ehci(res->start);

	/* connect the hcd phy pointer */
	hcd->usb_phy = tcc_ehci->transceiver;

	retval = sysfs_create_group(&pdev->dev.kobj, &usb_sq_attr_group);
	if (retval < 0) {
		printk(KERN_ERR "Cannot register USB SQ sysfs attributes: %d\n",
		       retval);
		goto fail_add_hcd;
	}

	retval = device_create_file(&pdev->dev, &dev_attr_ehci_tpl_support);
	if (retval < 0) {
		printk(KERN_ERR "Cannot register USB TPL Support attributes: %d\n",
		       retval);
		goto fail_add_hcd;
	}

	return retval;

fail_add_hcd:
	tcc_ehci_clk_ctrl(tcc_ehci, OFF);
fail_request_resource:
	usb_put_hcd(hcd);
fail_create_hcd:
	dev_err(&pdev->dev, "init %s fail, %d\n",
		dev_name(&pdev->dev), retval);
	
	return retval;
}

extern void ehci_shutdown(struct usb_hcd *hcd);
static int __exit ehci_tcc_drv_remove(struct platform_device *pdev)
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

	gpio_free(tcc_ehci->host_en_gpio);
	gpio_free(tcc_ehci->vbus_gpio);	

	if (tcc_ehci->vbus_source) {
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

extern bool of_usb_host_tpl_support(struct device_node *np);
static int tcc_ehci_parse_dt(struct platform_device *pdev, struct tcc_ehci_hcd *tcc_ehci)
{
	int err = 0;
	
	//===============================================
	// Check Host enable pin	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "hosten-ctrl-able", 0)) {
		tcc_ehci->hosten_ctrl_able = 1;

		tcc_ehci->host_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						"hosten-gpio", 0);
		if(!gpio_is_valid(tcc_ehci->host_en_gpio)){
			dev_err(&pdev->dev, "can't find dev of node: host en gpio\n");
			return -ENODEV;
		}

		err = gpio_request(tcc_ehci->host_en_gpio, "host_en_gpio");
		if(err) {
			dev_err(&pdev->dev, "can't requeest host_en gpio\n");
			return err;
		}
	} else {
		tcc_ehci->hosten_ctrl_able = 0;	// can not control vbus
	}

	//===============================================
	// Check vbus enable pin	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "telechips,ehci_phy", 0)) {
		tcc_ehci->transceiver = devm_usb_get_phy_by_phandle(&pdev->dev, "telechips,ehci_phy", 0);
#ifdef CONFIG_ARCH_TCC803X
		err = tcc_ehci->transceiver->set_vbus_resource(tcc_ehci->transceiver);
		if (err) {
			dev_err(&pdev->dev, "failed to set a vbus resource\n");
		}
#endif 
		if (IS_ERR(tcc_ehci->transceiver)) {
			tcc_ehci->transceiver = NULL;
			return -ENODEV;
		}
		tcc_ehci->phy_regs = tcc_ehci->transceiver->base;
	}

	//===============================================
	// Check VBUS Source enable	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-source-ctrl", 0)) {
		tcc_ehci->vbus_source_ctrl = 1;

		tcc_ehci->vbus_source = regulator_get(&pdev->dev, "vdd_v5p0");
		if (IS_ERR(tcc_ehci->vbus_source)) {
			dev_err(&pdev->dev, "failed to get ehci vdd_source\n");
			tcc_ehci->vbus_source = NULL;
		}
	} else {
		tcc_ehci->vbus_source_ctrl = 0;
	}
	
	tcc_ehci->hclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(tcc_ehci->hclk))
		tcc_ehci->hclk = NULL;

	tcc_ehci->isol = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(tcc_ehci->isol))
		tcc_ehci->isol = NULL;

	tcc_ehci->pclk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR(tcc_ehci->pclk))
		tcc_ehci->pclk = NULL;

	tcc_ehci->phy_clk = of_clk_get(pdev->dev.of_node, 3);
	if (IS_ERR(tcc_ehci->phy_clk))
		tcc_ehci->phy_clk = NULL;

	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &tcc_ehci->core_clk_rate);
	of_property_read_u32(pdev->dev.of_node, "clock-frequency-phy", &tcc_ehci->core_clk_rate_phy);
	//===============================================
	// Check TPL Support
	//===============================================
	if(of_usb_host_tpl_support(pdev->dev.of_node))
	{
		//printk("\x1b[1;33m[%s:%d] EHCI Support TPL\x1b[0m\n", __func__, __LINE__);
		tcc_ehci->hcd_tpl_support = 1;
	}
	else
	{
		//printk("\x1b[1;31m[%s:%d] EHCI Not Support TPL\x1b[0m\n", __func__, __LINE__);
		tcc_ehci->hcd_tpl_support = 0;
	}

	return err;		
}
#else
static int tcc_ehci_parse_dt(struct platform_device *pdev, struct tcc_ehci_hcd *tcc_ehci)
{
	return 0;
}
#endif

static void tcc_ehci_hcd_shutdown(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

static struct platform_driver ehci_tcc_driver = {
	.probe		= ehci_tcc_drv_probe,
	.remove		= __exit_p(ehci_tcc_drv_remove),
	.shutdown	= tcc_ehci_hcd_shutdown,
	.driver = {
		.name	= "tcc-ehci",
		.owner	= THIS_MODULE,
		.pm		= EHCI_TCC_PMOPS,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_ehci_match),
#endif
	}
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE ("GPL");

static int __init tcc_ehci_hcd_init(void)
{
	int retval = 0;

	ehci_init_driver(&ehci_tcc_hc_driver, NULL);

	set_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
	retval = platform_driver_register(&ehci_tcc_driver);
	if (retval < 0)
		clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	return retval;
}
module_init(tcc_ehci_hcd_init);

static void __exit tcc_ehci_hcd_cleanup(void)
{
	platform_driver_unregister(&ehci_tcc_driver);
	clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
}
module_exit(tcc_ehci_hcd_cleanup);
