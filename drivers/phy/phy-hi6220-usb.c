/*
 * Copyright (c) 2015 Linaro Ltd.
 * Copyright (c) 2015 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/usb/phy_companion.h>
#include <linux/usb/otg.h>
#include <linux/usb/gadget.h>
#include <linux/usb/phy.h>
#include <linux/regmap.h>
#include <linux/extcon.h>
#include <linux/spinlock.h>

#define SC_PERIPH_CTRL4			0x00c

#define CTRL4_PICO_SIDDQ		BIT(6)
#define CTRL4_PICO_OGDISABLE		BIT(8)
#define CTRL4_PICO_VBUSVLDEXT		BIT(10)
#define CTRL4_PICO_VBUSVLDEXTSEL	BIT(11)
#define CTRL4_OTG_PHY_SEL		BIT(21)

#define SC_PERIPH_CTRL5			0x010

#define CTRL5_USBOTG_RES_SEL		BIT(3)
#define CTRL5_PICOPHY_ACAENB		BIT(4)
#define CTRL5_PICOPHY_BC_MODE		BIT(5)
#define CTRL5_PICOPHY_CHRGSEL		BIT(6)
#define CTRL5_PICOPHY_VDATSRCEND	BIT(7)
#define CTRL5_PICOPHY_VDATDETENB	BIT(8)
#define CTRL5_PICOPHY_DCDENB		BIT(9)
#define CTRL5_PICOPHY_IDDIG		BIT(10)

#define SC_PERIPH_CTRL8			0x018
#define SC_PERIPH_RSTEN0		0x300
#define SC_PERIPH_RSTDIS0		0x304

#define RST0_USBOTG_BUS			BIT(4)
#define RST0_POR_PICOPHY		BIT(5)
#define RST0_USBOTG			BIT(6)
#define RST0_USBOTG_32K			BIT(7)

#define EYE_PATTERN_PARA		0x7053348c


struct hi6220_usb_cable {
	struct notifier_block		nb;
	struct extcon_dev		*extcon;
	int state;
};

struct hi6220_priv {
	struct regmap *reg;
	struct device *dev;
	struct usb_phy phy;

	struct delayed_work work;
	struct hi6220_usb_cable vbus;
	struct hi6220_usb_cable id;
	spinlock_t lock;
};

static void hi6220_phy_init(struct hi6220_priv *priv)
{
	struct regmap *reg = priv->reg;
	u32 val, mask;

	val = RST0_USBOTG_BUS | RST0_POR_PICOPHY |
	      RST0_USBOTG | RST0_USBOTG_32K;
	mask = val;
	regmap_update_bits(reg, SC_PERIPH_RSTEN0, mask, val);
	regmap_update_bits(reg, SC_PERIPH_RSTDIS0, mask, val);
}

static int hi6220_phy_setup(struct hi6220_priv *priv, bool on)
{
	struct regmap *reg = priv->reg;
	u32 val, mask;
	int ret;

	if (on) {
		val = CTRL5_USBOTG_RES_SEL | CTRL5_PICOPHY_ACAENB;
		mask = val | CTRL5_PICOPHY_BC_MODE;
		ret = regmap_update_bits(reg, SC_PERIPH_CTRL5, mask, val);
		if (ret)
			goto out;

		val =  CTRL4_PICO_VBUSVLDEXT | CTRL4_PICO_VBUSVLDEXTSEL |
		       CTRL4_OTG_PHY_SEL;
		mask = val | CTRL4_PICO_SIDDQ | CTRL4_PICO_OGDISABLE;
		ret = regmap_update_bits(reg, SC_PERIPH_CTRL4, mask, val);
		if (ret)
			goto out;

		ret = regmap_write(reg, SC_PERIPH_CTRL8, EYE_PATTERN_PARA);
		if (ret)
			goto out;
	} else {
		val = CTRL4_PICO_SIDDQ;
		mask = val;
		ret = regmap_update_bits(reg, SC_PERIPH_CTRL4, mask, val);
		if (ret)
			goto out;
	}

	return 0;
out:
	dev_err(priv->dev, "failed to setup phy ret: %d\n", ret);
	return ret;
}

static int hi6220_phy_start(struct phy *phy)
{
	struct hi6220_priv *priv = phy_get_drvdata(phy);

	return hi6220_phy_setup(priv, true);
}

static int hi6220_phy_exit(struct phy *phy)
{
	struct hi6220_priv *priv = phy_get_drvdata(phy);

	return hi6220_phy_setup(priv, false);
}


static struct phy_ops hi6220_phy_ops = {
	.init		= hi6220_phy_start,
	.exit		= hi6220_phy_exit,
	.owner		= THIS_MODULE,
};

static void hi6220_detect_work(struct work_struct *work)
{
	struct hi6220_priv *priv =
		container_of(to_delayed_work(work), struct hi6220_priv, work);
	struct usb_otg *otg = priv->phy.otg;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	if (!IS_ERR(priv->vbus.extcon))
		priv->vbus.state = extcon_get_cable_state_(priv->vbus.extcon, EXTCON_USB);
	if (!IS_ERR(priv->id.extcon))
		priv->id.state = extcon_get_cable_state_(priv->id.extcon, EXTCON_USB_HOST);

	if (otg->gadget) {
		if (priv->id.state)
			usb_gadget_connect(otg->gadget);
		else
			usb_gadget_disconnect(otg->gadget);
	}

	printk("JDB: hi6220_detect_work! (vbus: %i id: %i)\n", priv->vbus.state, priv->id.state);
	spin_unlock_irqrestore(&priv->lock, flags);
}

static int hi6220_otg_vbus_notifier(struct notifier_block *nb, unsigned long event,
					void *ptr)
{
	struct hi6220_usb_cable *vbus = container_of(nb, struct hi6220_usb_cable, nb);
	struct hi6220_priv *priv = container_of(vbus, struct hi6220_priv, vbus);
	unsigned long flags;


	spin_lock_irqsave(&priv->lock, flags);
	printk("JDB: hi6220_otg_vbus_notifier: event: %ld\n", event);
	schedule_delayed_work(&priv->work, msecs_to_jiffies(100));
	spin_unlock_irqrestore(&priv->lock, flags);

	return NOTIFY_DONE;
}

static int hi6220_otg_id_notifier(struct notifier_block *nb, unsigned long event,
					void *ptr)
{
	struct hi6220_usb_cable *id = container_of(nb, struct hi6220_usb_cable, nb);
	struct hi6220_priv *priv = container_of(id, struct hi6220_priv, id);
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	printk("JDB: hi6220_otg_id_notifier: event: %ld\n", event);
	schedule_delayed_work(&priv->work, msecs_to_jiffies(100));
	spin_unlock_irqrestore(&priv->lock, flags);

	return NOTIFY_DONE;
}

static int hi6220_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	otg->host = host;
	return 0;
}

static int hi6220_otg_set_peripheral(struct usb_otg *otg,
					struct usb_gadget *gadget)
{
	printk("JDB: hi6220_otg_set_peripheral!\n");
	otg->gadget = gadget;
	return 0;
}

static int hi6220_phy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct device *dev = &pdev->dev;
	struct phy *phy;
	struct usb_otg *otg;
	struct hi6220_priv *priv;
	struct extcon_dev *ext_id, *ext_vbus;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	spin_lock_init(&priv->lock);
	INIT_DELAYED_WORK(&priv->work, hi6220_detect_work);

	priv->dev = dev;
	priv->reg = syscon_regmap_lookup_by_phandle(dev->of_node,
					"hisilicon,peripheral-syscon");
	if (IS_ERR(priv->reg)) {
		dev_err(dev, "no hisilicon,peripheral-syscon\n");
		return PTR_ERR(priv->reg);
	}


	ext_id = ERR_PTR(-ENODEV);
	ext_vbus = ERR_PTR(-ENODEV);
	if (of_property_read_bool(dev->of_node, "extcon")) {
		/* Each one of them is not mandatory */
		ext_vbus = extcon_get_edev_by_phandle(&pdev->dev, 0);
		if (IS_ERR(ext_vbus) && PTR_ERR(ext_vbus) != -ENODEV)
			return PTR_ERR(ext_vbus);

		ext_id = extcon_get_edev_by_phandle(&pdev->dev, 1);
		if (IS_ERR(ext_id) && PTR_ERR(ext_id) != -ENODEV)
			return PTR_ERR(ext_id);
	}

	priv->vbus.extcon = ext_vbus;
	if (!IS_ERR(ext_vbus)) {
		priv->vbus.nb.notifier_call = hi6220_otg_vbus_notifier;
		ret = extcon_register_notifier(ext_vbus, EXTCON_USB,
                                                &priv->vbus.nb);
		if (ret < 0) {
			dev_err(&pdev->dev, "register VBUS notifier failed\n");
			return ret;
		}

		priv->vbus.state = extcon_get_cable_state_(ext_vbus, EXTCON_USB);
	}

	priv->id.extcon = ext_id;
	if (!IS_ERR(ext_id)) {
		priv->id.nb.notifier_call = hi6220_otg_id_notifier;
		ret = extcon_register_notifier(ext_id, EXTCON_USB_HOST,
						&priv->id.nb);
		if (ret < 0) {
			dev_err(&pdev->dev, "register ID notifier failed\n");
			return ret;
		}

		priv->id.state = extcon_get_cable_state_(ext_id, EXTCON_USB_HOST);
	}

	hi6220_phy_init(priv);

	phy = devm_phy_create(dev, NULL, &hi6220_phy_ops);
	if (IS_ERR(phy))
		return PTR_ERR(phy);


        otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
        if (!otg)
                return -ENOMEM;

        priv->dev = &pdev->dev;
        priv->phy.dev = priv->dev;
        priv->phy.label = "hi6220_usb_phy";
        priv->phy.otg = otg;
        priv->phy.type = USB_PHY_TYPE_USB2;
	otg->set_host = hi6220_otg_set_host;
	otg->set_peripheral = hi6220_otg_set_peripheral;
        otg->usb_phy = &priv->phy;

	phy_set_drvdata(phy, priv);
        platform_set_drvdata(pdev, phy);

        usb_add_phy(&priv->phy, USB_PHY_TYPE_USB2);


	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id hi6220_phy_of_match[] = {
	{.compatible = "hisilicon,hi6220-usb-phy",},
	{ },
};
MODULE_DEVICE_TABLE(of, hi6220_phy_of_match);

static struct platform_driver hi6220_phy_driver = {
	.probe	= hi6220_phy_probe,
	.driver = {
		.name	= "hi6220-usb-phy",
		.of_match_table	= hi6220_phy_of_match,
	}
};
module_platform_driver(hi6220_phy_driver);

MODULE_DESCRIPTION("HISILICON HI6220 USB PHY driver");
MODULE_ALIAS("platform:hi6220-usb-phy");
MODULE_LICENSE("GPL");
