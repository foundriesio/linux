/*
 * drivers/usb/core/otg_whitelist.h
 *
 * Copyright (C) 2004 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * This OTG and Embedded Host Whitelist is "Targeted Peripheral List".
 * It should mostly use of USB_DEVICE() or USB_DEVICE_VER() entries..
 *
 * YOU _SHOULD_ CHANGE THIS LIST TO MATCH YOUR PRODUCT AND ITS TESTING!
 */

#define USB_INTERFACE_CLASS_INFO(cl) \
	.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS, \
		.bInterfaceClass = (cl)
static struct usb_device_id whitelist_table[] = {

/* hubs are optional in OTG, but very handy ... */
{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 0), },
{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 1), },
{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 2), },      /* 2: high-speed multiple-TT */
{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 3), },      /* 3: high-speed */

#ifdef	CONFIG_USB_PRINTER		/* ignoring nonstatic linkage! */
/* FIXME actually, printers are NOT supposed to use device classes;
 * they're supposed to use interface classes...
 */
{ USB_DEVICE_INFO(7, 1, 1) },
{ USB_DEVICE_INFO(7, 1, 2) },
{ USB_DEVICE_INFO(7, 1, 3) },
#endif

#ifdef	CONFIG_USB_NET_CDCETHER
/* Linux-USB CDC Ethernet gadget */
{ USB_DEVICE(0x0525, 0xa4a1), },
/* Linux-USB CDC Ethernet + RNDIS gadget */
{ USB_DEVICE(0x0525, 0xa4a2), },
#endif

#if	IS_ENABLED(CONFIG_USB_TEST)
/* gadget zero, for testing */
{ USB_DEVICE(0x0525, 0xa4a0), },
#endif
#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
{ USB_INTERFACE_CLASS_INFO(USB_CLASS_MASS_STORAGE) },
{ USB_INTERFACE_CLASS_INFO(USB_CLASS_HID) },
{ USB_INTERFACE_CLASS_INFO(USB_CLASS_VENDOR_SPEC) },
{ USB_VENDOR_AND_INTERFACE_INFO(0x05ac, USB_CLASS_AUDIO, 0, 0) }, //apple device
#endif
{ }	/* Terminating entry */
};

static bool match_int_class(struct usb_device_id *id, struct usb_device *udev)
{
	struct usb_host_config *c;
	int num_configs, i;

	/* Copy the code from generic.c */
	c = udev->config;
	num_configs = udev->descriptor.bNumConfigurations;
	for (i = 0; i < num_configs; (i++, c++)) {
		struct usb_interface_descriptor	*desc = NULL;

		/* It's possible that a config has no interfaces! */
		if (c->desc.bNumInterfaces > 0)
			desc = &c->intf_cache[0]->altsetting->desc;

		dev_info(&udev->dev,
			"%s : desc->bInterfaceClass = 0x%x / id->bInterfaceClass = 0x%x\n",
			__func__, desc->bInterfaceClass, id->bInterfaceClass);
		if (desc && (desc->bInterfaceClass == id->bInterfaceClass))
			return true;
	}

	return false;
}


#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
#include <linux/usb.h>
#endif /* CONFIG_TCC_DWC_HS_ELECT_TST */

static int is_targeted(struct usb_device *dev)
{
	struct usb_device_id	*id = whitelist_table;

#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
	dev->bus->otg_vbus_off = 0;
#endif

	/* HNP test device is _never_ targeted (see OTG spec 6.6.6) */
	if ((le16_to_cpu(dev->descriptor.idVendor) == 0x1a0a &&
	     le16_to_cpu(dev->descriptor.idProduct) == 0xbadd))
		return 0;

	/* OTG PET device is always targeted (see OTG 2.0 ECN 6.4.2) */
	if ((le16_to_cpu(dev->descriptor.idVendor) == 0x1a0a &&
	     le16_to_cpu(dev->descriptor.idProduct) == 0x0200)) {
#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
		if (dev->descriptor.bcdDevice & 0x1) {
			dev_info(&dev->dev,
				"\x1b[1;33m[%s:%d] 'otg_vbus_off' is set (bcdDevice : %x)\x1b[0m\n",
				__func__, __LINE__, dev->descriptor.bcdDevice);
			dev->bus->otg_vbus_off = 1;
		} else {
			dev->bus->otg_vbus_off = 0;
		}
#endif /* CONFIG_TCC_DWC_HS_ELECT_TST */
		dev_info(&dev->dev, "%s:%d\n", __func__, __LINE__);
		return 1;
	}

// For OPT Test
#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
	if (le16_to_cpu(dev->descriptor.idVendor) == 0x1a0a) {
		unsigned int pid = 0;

		pid = le16_to_cpu(dev->descriptor.idProduct);
		if (pid >= 0x0101 && pid <= 0x0108) {
			dev_info(&dev->dev,
				"\x1b[1;33m[%s:%d] USB OPT Test Device!!!\x1b[0m\n",
				__func__, __LINE__);
			return 1;
		}
	}

#endif


	/* NOTE: can't use usb_match_id() since interface caches
	 * aren't set up yet. this is cut/paste from that code.
	 */
	for (id = whitelist_table; id->match_flags; id++) {
		if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		    id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		    id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
			continue;

		/* No need to test id->bcdDevice_lo != 0, since 0 is never
		   greater than any unsigned number. */
		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
		    (id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
		    (id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
		    (id->bDeviceClass != dev->descriptor.bDeviceClass))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
		    (id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
		    (id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
		    (!match_int_class(id, dev)))
			continue;
#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
		dev_emerg(&dev->dev,
				"Attached device (v%04x p%04x)\n",
				le16_to_cpu(dev->descriptor.idVendor),
				le16_to_cpu(dev->descriptor.idProduct));
#endif /* CONFIG_TCC_DWC_HS_ELECT_TST */

		return 1;
	}

	/* add other match criteria here ... */


	/* OTG MESSAGE: report errors here, customize to match your product */
#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
	dev_emerg(&dev->dev,
			"\x1b[1;31mdevice v%04x p%04x is not supported\x1b[0m\n",
			le16_to_cpu(dev->descriptor.idVendor),
			le16_to_cpu(dev->descriptor.idProduct));
#else

	dev_err(&dev->dev, "device v%04x p%04x is not supported\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));
#endif /* CONFIG_TCC_DWC_HS_ELECT_TST */



	return 0;
}

