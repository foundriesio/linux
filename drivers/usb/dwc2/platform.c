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

#ifdef CONFIG_USB_DWC2_TCC
static int dwc2_set_dr_mode(struct dwc2_hsotg *hsotg, enum usb_dr_mode mode);

int dwc2_tcc_power_ctrl(struct dwc2_hsotg *hsotg, int on_off)
{
    int err = 0;

    if ((hsotg->vbus_source_ctrl == 1) && (hsotg->vbus_source)) {
        if(on_off == 1) {
            err = regulator_enable(hsotg->vbus_source);
            if(err) {
                printk("dwc_otg: can't enable vbus source\n");
                return err;
            }
            mdelay(1);
        } else if(on_off == 0) {
            regulator_disable(hsotg->vbus_source);
        }
    }

    return err;
}

static void dwc2_tcc_vbus_init(struct dwc2_hsotg *hsotg)
{
    dwc2_tcc_power_ctrl(hsotg, 1);
}

static void dwc2_tcc_vbus_exit(struct dwc2_hsotg *hsotg)
{
    dwc2_tcc_power_ctrl(hsotg, 0);
}

#ifdef CONFIG_VBUS_CTRL_DEF_ENABLE      /* 017.08.30 */
static unsigned int vbus_control_enable = 1;  /* 2017/03/23, */
#else
static unsigned int vbus_control_enable = 0;  /* 2017/03/23, */
#endif /* CONFIG_VBUS_CTRL_DEF_ENABLE */



int dwc2_tcc_vbus_ctrl(struct dwc2_hsotg *hsotg, int on_off)
{
	struct usb_phy *phy = hsotg->uphy;

	if (!vbus_control_enable) {
		dev_err(hsotg->dev, "dwc_otg vbus ctrl disable.\n");
		return -1;
	}
	if (!phy || !phy->set_vbus) {
		dev_err(hsotg->dev, "[%s:%d]PHY driver is needed\n", __func__, __LINE__);
		return -1;
	}
	dev_err(hsotg->dev, "why???\n");
	hsotg->vbus_status = on_off;

	return phy->set_vbus(phy, on_off);
}

static ssize_t dwc2_tcc_vbus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

    return sprintf(buf, "dwc2 vbus - %s\n",(hsotg->vbus_status) ? "on":"off");
}

static ssize_t dwc2_tcc_vbus_store(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count)
{
    struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

    if (!strncmp(buf, "on", 2)) {
        dwc2_tcc_vbus_ctrl(hsotg, 1);
    }

    if (!strncmp(buf, "off", 3)) {
        dwc2_tcc_vbus_ctrl(hsotg, 0);
    }

    return count;
}
static DEVICE_ATTR(vbus, S_IRUGO | S_IWUSR, dwc2_tcc_vbus_show, dwc2_tcc_vbus_store);

static ssize_t dwc2_tcc_drd_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

	return sprintf(buf, "dwc2 dr_mode - %s\n", hsotg->dr_mode == USB_DR_MODE_HOST ? "HOST":"DEVICE");
}

static int dwc2_change_dr_mode(struct dwc2_hsotg *);
static ssize_t dwc2_tcc_drd_mode_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	int retval = 0;
	if (!strncmp(buf, "host", 4)) {
		if(!dwc2_set_dr_mode(hsotg, USB_DR_MODE_HOST)) {
			retval = dwc2_change_dr_mode(hsotg);
			if (retval)
				dev_err(hsotg->dev, "Failed to dr_mode Change \n");
		} else {
		}
	} else if (!strncmp(buf, "device", 6)) {
		if(!dwc2_set_dr_mode(hsotg, USB_DR_MODE_PERIPHERAL)) {
			dwc2_change_dr_mode(hsotg);
			if (retval)
				dev_err(hsotg->dev, "Failed to dr_mode Change \n");
		} else {
		}
	} else {
		dev_err(hsotg->dev, "Value is invalid!\n");
	}
	
	return count;
}
static DEVICE_ATTR(dr_mode, S_IRUGO | S_IWUSR, dwc2_tcc_drd_mode_show, dwc2_tcc_drd_mode_store);

static int dwc2_set_dr_mode(struct dwc2_hsotg *hsotg, enum usb_dr_mode mode)
{
    hsotg->dr_mode = mode;
    if (hsotg->dr_mode == USB_DR_MODE_UNKNOWN)
        hsotg->dr_mode = USB_DR_MODE_OTG;

    mode = hsotg->dr_mode;

    if (dwc2_hw_is_device(hsotg)) {
        if (IS_ENABLED(CONFIG_USB_DWC2_HOST)) {
            dev_err(hsotg->dev,
                "Controller does not support host mode.\n");
            return -EINVAL;
        }
        mode = USB_DR_MODE_PERIPHERAL;
    } else if (dwc2_hw_is_host(hsotg)) {
        if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL)) {
            dev_err(hsotg->dev,
                "Controller does not support device mode.\n");
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
             "Configuration mismatch. dr_mode forced to %s\n",
            mode == USB_DR_MODE_HOST ? "host" : "device");

        hsotg->dr_mode = mode;
    }

    return 0;
	
}
#ifdef CONFIG_USB_DWC2_TCC_MUX
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb/hcd.h>

struct usb_mux_hcd_device{
	struct platform_device *ehci_dev;
	struct platform_device *ohci_dev;
	u32 enable_flags;
};

static const struct usb_ehci_pdata ehci_pdata = {
};

static const struct usb_ohci_pdata ohci_pdata = {
};

static struct platform_device *dwc2_create_mux_hcd_pdev(struct dwc2_hsotg *hsotg, bool ohci, u32 res_start, u32 size)
{
	struct platform_device *hci_dev;
	struct resource hci_res[2];
	struct usb_hcd *hcd;
	int ret = -ENOMEM;

	memset(hci_res, 0, sizeof(hci_res));

	hci_res[0].start = res_start;
	hci_res[0].end 	= res_start + size - 1;
	hci_res[0].flags = IORESOURCE_MEM;

	hci_res[1].start = hsotg->ehci_irq;
	hci_res[1].flags = IORESOURCE_IRQ;

	hci_dev = platform_device_alloc(ohci ? "ohci-mux" :
					"ehci-mux" , 0);

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
		printk("\x1b[1;31m[%s:%d](hcd == NULL)\x1b[0m\n", __func__, __LINE__);
		while(1);
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
	int start, size;

	usb_dev = kzalloc(sizeof(struct usb_mux_hcd_device), GFP_KERNEL);
	if (!usb_dev)
		return -ENOMEM;

	start = (int)hsotg->ohci_regs;
	size = (int)hsotg->ohci_regs_size;
	usb_dev->ohci_dev = dwc2_create_mux_hcd_pdev(hsotg, true, start, size);
	if (IS_ERR(usb_dev->ohci_dev)) {
		err = PTR_ERR(usb_dev->ohci_dev);
		goto err_free_usb_dev;
	}

	start = (int)hsotg->ehci_regs;
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

static int is_device = -1;
module_param_named(is_device, is_device, int, 0644);
MODULE_PARM_DESC(is_device, "1. 0 == host, 2. 1 == device");
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
				"Controller does not support host mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_PERIPHERAL;
	} else if (dwc2_hw_is_host(hsotg)) {
		if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL)) {
			dev_err(hsotg->dev,
				"Controller does not support device mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_HOST;
#ifdef CONFIG_USB_DWC2_TCC
	} else {
		if (is_device == 0)
			mode = USB_DR_MODE_HOST;
		else if (is_device == 1)
			mode = USB_DR_MODE_PERIPHERAL;
		else {
			dev_err(hsotg->dev,
				"This is Not a Module or is_device's value is invalid.\n");
			return -EINVAL;
		}
	}
#else
	} else {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST))
			mode = USB_DR_MODE_HOST;
		else if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL))
			mode = USB_DR_MODE_PERIPHERAL;
	}
#endif
	if (mode != hsotg->dr_mode) {
		dev_warn(hsotg->dev,
			 "Configuration mismatch. dr_mode forced to %s\n",
			mode == USB_DR_MODE_HOST ? "host" : "device");

		hsotg->dr_mode = mode;
	}

	return 0;
}

static int __dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret;
#ifdef CONFIG_USB_DWC2_TCC
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
#ifdef CONFIG_USB_DWC2_TCC_MUX
	if (hsotg->mhst_uphy && is_device == 0) 
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

	if (ret == 0)
		hsotg->ll_hw_enabled = true;
	return ret;
}

static int __dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret = 0;

#ifdef CONFIG_USB_DWC2_TCC_MUX
	if (hsotg->mhst_uphy && is_device == 0)
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
#ifdef CONFIG_USB_DWC2_TCC
	ret = dwc2_tcc_vbus_ctrl(hsotg, 0);
#else
	ret = regulator_bulk_disable(ARRAY_SIZE(hsotg->supplies),
				     hsotg->supplies);
#endif
	return ret;
}

/**
 * dwc2_lowlevel_hw_disable - disable platform lowlevel hw resources
 /* @hsotg: The driver state
 *
 * A wrapper for platform code responsible for controlling
 * low-level USB platform resources (phy, clock, regulators)
 */
int dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	int ret = __dwc2_lowlevel_hw_disable(hsotg);

	if (ret == 0)
		hsotg->ll_hw_enabled = false;
	return ret;
}

static int dwc2_lowlevel_hw_init(struct dwc2_hsotg *hsotg)
{
	int i, ret;

	hsotg->reset = devm_reset_control_get_optional(hsotg->dev, "dwc2");
	if (IS_ERR(hsotg->reset)) {
		ret = PTR_ERR(hsotg->reset);
		dev_err(hsotg->dev, "error getting reset control %d\n", ret);
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
		dev_err(hsotg->dev, "[KJS]usb-2phy is ERR %s\n", __func__);	
		ret = PTR_ERR(hsotg->phy);
		switch (ret) {
		case -ENODEV:
		case -ENOSYS:
			hsotg->phy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(hsotg->dev, "error getting phy %d\n", ret);
			return ret;
		}
	}

	if (!hsotg->phy) {
		dev_err(hsotg->dev, "[KJS]hsotg->phy is NULL %s\n", __func__);
#ifndef CONFIG_USB_DWC2_TCC
		hsotg->uphy = devm_usb_get_phy(hsotg->dev, USB_PHY_TYPE_USB2);
#else
		hsotg->uphy = devm_usb_get_phy_by_phandle(hsotg->dev, "phy", 0);
#endif
		if (IS_ERR(hsotg->uphy)) {
			dev_err(hsotg->dev, "[KJS]hsotg->uphy is NULL %s\n", __func__);
			ret = PTR_ERR(hsotg->uphy);
			switch (ret) {
			case -ENODEV:
			case -ENXIO:
				hsotg->uphy = NULL;
				break;
			case -EPROBE_DEFER:
				return ret;
			default:
				dev_err(hsotg->dev, "error getting usb phy %d\n",
					ret);
				return ret;
			}
		}
	}
#ifdef CONFIG_USB_DWC2_TCC_MUX
	hsotg->mhst_uphy = devm_usb_get_phy_by_phandle(hsotg->dev, "telechips,mhst_phy", 0);
	if (IS_ERR(hsotg->mhst_uphy)) {
		dev_err(hsotg->dev, "[KJS]hsotg->mhst_uphy is NULL %s\n", __func__);
		ret = PTR_ERR(hsotg->uphy);
		switch (ret) {
		case -ENODEV:
		case -ENXIO:
			hsotg->mhst_uphy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(hsotg->dev, "error getting usb mhst phy %d\n",
				ret);
			return ret;
		}
	}
	hsotg->mhst_uphy->otg->mux_cfg_addr = hsotg->uphy->base + 0x28; //otg phy's mux switching register
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
		dev_err(hsotg->dev, "cannot get otg clock\n");
	}
#ifdef CONFIG_USB_DWC2_TCC
	/* TCC vbus */
	ret = dwc2_tcc_vbus_ctrl(hsotg, 1);
#else
	/* Regulators */
	for (i = 0; i < ARRAY_SIZE(hsotg->supplies); i++)
		hsotg->supplies[i].supply = dwc2_hsotg_supply_names[i];

	ret = devm_regulator_bulk_get(hsotg->dev, ARRAY_SIZE(hsotg->supplies),
				      hsotg->supplies);
	if (ret) {
		dev_err(hsotg->dev, "failed to request supplies: %d\n", ret);
		return ret;
	}
#endif
	return 0;
}
#ifdef CONFIG_USB_DWC2_TCC
static int dwc2_change_dr_mode(struct dwc2_hsotg *hsotg)
{
	dwc2_manual_change(hsotg);

	return 0;
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
#ifdef CONFIG_USB_DWC2_TCC_MUX
		dwc2_mux_hcd_remove(hsotg);
#else
		dwc2_hcd_remove(hsotg);
#endif
	if (hsotg->gadget_enabled)
		dwc2_hsotg_remove(hsotg);

	if (hsotg->ll_hw_enabled)
		dwc2_lowlevel_hw_disable(hsotg);

	reset_control_assert(hsotg->reset);
#ifdef CONFIG_USB_DWC2_TCC
	device_remove_file(&dev->dev, &dev_attr_dr_mode);
	device_remove_file(&dev->dev, &dev_attr_vbus);
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
#ifdef CONFIG_USB_DWC2_TCC_MUX
	disable_irq(hsotg->ehci_irq);
#endif
	disable_irq(hsotg->irq);
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

	dev_err(&dev->dev, "dwc2 controller mapped PA %08lx to VA %p\n",
		(unsigned long)res->start, hsotg->regs);
#ifdef CONFIG_USB_DWC2_TCC_MUX
	/*
	 * Get ehci's register base for MUX
	 */
	hsotg->ehci_regs = dev->resource[1].start;
	hsotg->ehci_regs_size = dev->resource[1].end - dev->resource[1].start + 1;
	if (IS_ERR(hsotg->ehci_regs))
		return PTR_ERR(hsotg->ehci_regs);

	dev_err(&dev->dev, "ehci controller mapped PA %08lx to VA %p SIZE %d\n",
		(unsigned long)res->start, hsotg->ehci_regs, hsotg->ehci_regs_size);

	/*
	 * Get ohci's register base for MUX
	 */
	hsotg->ohci_regs = dev->resource[2].start;
	hsotg->ohci_regs_size = dev->resource[2].end - dev->resource[2].start + 1;
	if (IS_ERR(hsotg->ohci_regs))
		return PTR_ERR(hsotg->ohci_regs);

	dev_err(&dev->dev, "ohci controller mapped PA %08lx to VA %p SIZE %d\n",
		(unsigned long)res->start, hsotg->ohci_regs, hsotg->ohci_regs_size);

#endif
	retval = dwc2_lowlevel_hw_init(hsotg);
	if (retval)
		return retval;
	
	spin_lock_init(&hsotg->lock);

	hsotg->irq = platform_get_irq(dev, 0);
	if (hsotg->irq < 0) {
		dev_err(&dev->dev, "missing IRQ resource\n");
		return hsotg->irq;
	}

	dev_err(hsotg->dev, "registering common handler for irq%d\n",
		hsotg->irq);
	retval = devm_request_irq(hsotg->dev, hsotg->irq,
				  dwc2_handle_common_intr, IRQF_SHARED,
				  dev_name(hsotg->dev), hsotg);
	if (retval)
		return retval;
#ifdef CONFIG_USB_DWC2_TCC_MUX
	hsotg->ehci_irq = platform_get_irq(dev, 1);
	if (hsotg->irq < 0) {
		dev_err(&dev->dev, "missing IRQ resource\n");
		return hsotg->ehci_irq;
	}
	dev_dbg(hsotg->dev, "ehci_irq's number is %d\n", hsotg->ehci_irq);	
#endif
	retval = dwc2_lowlevel_hw_enable(hsotg);
	if (retval)
		return retval;

	retval = dwc2_get_dr_mode(hsotg);
	if (retval)
		goto error;

	/*
	 * Reset before dwc2_get_hwparams() then it could get power-on real
	 * reset value form registers.
	 */
	dwc2_core_reset_and_force_dr_mode(hsotg);

	/* Detect config values from hardware */
	retval = dwc2_get_hwparams(hsotg);
	if (retval)
		goto error;

	dwc2_force_dr_mode(hsotg);

	retval = dwc2_init_params(hsotg);
	if (retval)
		goto error;

	if (hsotg->dr_mode != USB_DR_MODE_HOST) {
		retval = dwc2_gadget_init(hsotg, hsotg->irq);
		if (retval)
			goto error;
		hsotg->gadget_enabled = 1;
	}

	if (hsotg->dr_mode != USB_DR_MODE_PERIPHERAL) {
#ifdef CONFIG_USB_DWC2_TCC_MUX
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
	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL)
		dwc2_lowlevel_hw_disable(hsotg);
#ifdef CONFIG_USB_DWC2_TCC
	retval = device_create_file(&dev->dev, &dev_attr_vbus);
	if (retval) 
		dev_err(hsotg->dev, "failed to create vbus\n");
	
	retval = device_create_file(&dev->dev, &dev_attr_dr_mode);
	if (retval)
		dev_err(hsotg->dev, "failed to create dr_mode\n");

#endif
	return 0;

error:
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
