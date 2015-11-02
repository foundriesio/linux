/*
 * Copyright (c) 2014 Linaro Ltd.
 * Copyright (c) 2014 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/regulator/consumer.h>

#define SC_PERIPH_CTRL4			0x00c

#define CTRL4_PICO_SIDDQ		BIT(6)
#define CTRL4_PICO_OGDISABLE	BIT(8)
#define CTRL4_PICO_VBUSVLDEXT	BIT(10)
#define CTRL4_PICO_VBUSVLDEXTSEL	BIT(11)
#define CTRL4_OTG_PHY_SEL	BIT(21)

#define SC_PERIPH_CTRL5			0x010

#define CTRL5_USBOTG_RES_SEL	BIT(3)
#define CTRL5_PICOPHY_ACAENB	BIT(4)
#define CTRL5_PICOPHY_BC_MODE	BIT(5)
#define CTRL5_PICOPHY_CHRGSEL	BIT(6)
#define CTRL5_PICOPHY_VDATSRCEND	BIT(7)
#define CTRL5_PICOPHY_VDATDETENB	BIT(8)
#define CTRL5_PICOPHY_DCDENB	BIT(9)
#define CTRL5_PICOPHY_IDDIG	BIT(10)

#define SC_PERIPH_CTRL8			0x018

#define SC_PERIPH_RSTEN0		0x300
#define SC_PERIPH_RSTDIS0		0x304

#define RST0_USBOTG_BUS			BIT(4)
#define RST0_POR_PICOPHY		BIT(5)
#define RST0_USBOTG			BIT(6)
#define RST0_USBOTG_32K			BIT(7)

#define EYE_PATTERN_PARA		0x7053348c

#define to_hi6220_phy(p) container_of((p), struct hi6220_priv, phy)

struct hi6220_priv {
	struct usb_phy phy;
	struct delayed_work work;
	struct device *dev;
	void __iomem *base;
	struct regmap *reg;
	struct clk *clk;
	int gpio_vbus;
	int gpio_id;
	enum usb_otg_state state;
};

static void hi6220_start_periphrals(struct hi6220_priv *priv, bool on)
{
	struct usb_otg *otg = priv->phy.otg;

	if (!otg->gadget)
		return;

	if (on)
		usb_gadget_connect(otg->gadget);
	else
		usb_gadget_disconnect(otg->gadget);
}

static void hi6220_phy_init(struct hi6220_priv *priv)
{
	struct regmap *reg = priv->reg;
	u32 val, mask;

	if (priv->reg == NULL)
		return;

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

	if (priv->reg == NULL)
		return 0;

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

static void hi6220_detect_work(struct work_struct *work)
{
	struct hi6220_priv *priv =
		container_of(work, struct hi6220_priv, work.work);
	int gpio_id, gpio_vbus;
	enum usb_otg_state state;

	if (!gpio_is_valid(priv->gpio_id) || !gpio_is_valid(priv->gpio_vbus))
		return;

	gpio_id = gpio_get_value_cansleep(priv->gpio_id);
	gpio_vbus = gpio_get_value_cansleep(priv->gpio_vbus);

	if (gpio_vbus == 0) {
		if (gpio_id == 1)
			state = OTG_STATE_B_PERIPHERAL;
		else
			state = OTG_STATE_A_HOST;
	} else {
		state = OTG_STATE_A_HOST;
	}

	if (priv->state != state) {
		hi6220_start_periphrals(priv, state == OTG_STATE_B_PERIPHERAL);
		priv->state = state;
	}
}

static irqreturn_t hiusb_gpio_intr(int irq, void *data)
{
	struct hi6220_priv *priv = (struct hi6220_priv *)data;

	/* add debounce time */
	schedule_delayed_work(&priv->work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static int hi6220_phy_on_connect(struct usb_phy *phy,
		enum usb_device_speed speed)
{
	dev_dbg(phy->dev, "%s speed device has connected\n",
		(speed == USB_SPEED_HIGH) ? "high" : "non-high");
	return 0;
}

static int hi6220_phy_on_disconnect(struct usb_phy *phy,
		enum usb_device_speed speed)
{
	dev_dbg(phy->dev, "%s speed device has disconnected\n",
		(speed == USB_SPEED_HIGH) ? "high" : "non-high");

	return 0;
}

static int mv_otg_set_host(struct usb_otg *otg,
			   struct usb_bus *host)
{
	otg->host = host;
	return 0;
}

static int mv_otg_set_peripheral(struct usb_otg *otg,
				 struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	return 0;
}

static int hi6220_phy_probe(struct platform_device *pdev)
{
	struct hi6220_priv *priv;
	struct usb_otg *otg;
	struct device_node *np = pdev->dev.of_node;
	struct regulator *reg_vhub;
	int ret, irq;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	reg_vhub = devm_regulator_get(&pdev->dev, "vhub");
	if (!IS_ERR(reg_vhub)) {
		ret = regulator_enable(reg_vhub);
		if (ret) {
			dev_err(&pdev->dev, "Failed to enable vhub regulator: %d\n", ret);
			return -ENODEV;
		}
	}

	priv->dev = &pdev->dev;
	priv->phy.dev = priv->dev;
	priv->phy.otg = otg;
	priv->phy.label = "hi6220";
	priv->phy.notify_connect = hi6220_phy_on_connect;
	priv->phy.notify_disconnect = hi6220_phy_on_disconnect;
	platform_set_drvdata(pdev, priv);

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);
	clk_prepare_enable(priv->clk);

	otg->set_host = mv_otg_set_host;
	otg->set_peripheral = mv_otg_set_peripheral;

	priv->gpio_vbus = of_get_named_gpio(np, "huawei,gpio_vbus_det", 0);
	if (priv->gpio_vbus== -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (!gpio_is_valid(priv->gpio_vbus)) {
		dev_err(&pdev->dev, "invalid gpio pins %d\n", priv->gpio_vbus);
		return -ENODEV;
	}

	priv->gpio_id = of_get_named_gpio(np, "huawei,gpio_id_det", 0);
	if (priv->gpio_id== -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (!gpio_is_valid(priv->gpio_id)) {
		dev_err(&pdev->dev, "invalid gpio pins %d\n", priv->gpio_id);
		return -ENODEV;
	}

	priv->reg = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
					"hisilicon,peripheral-syscon");
	if (IS_ERR(priv->reg))
		priv->reg = NULL;

	hi6220_phy_init(priv);
	INIT_DELAYED_WORK(&priv->work, hi6220_detect_work);

	ret = gpio_request(priv->gpio_vbus, "gpio_vbus_det");
        if (ret < 0) {
		dev_err(&pdev->dev, "gpio request failed for otg-int-gpio\n");
		return ret;
        }

	ret = gpio_request(priv->gpio_id, "gpio_id_det");
        if (ret < 0) {
		dev_err(&pdev->dev, "gpio request failed for otg-int-gpio\n");
		return ret;
        }

        gpio_direction_input(priv->gpio_vbus);
        gpio_direction_input(priv->gpio_id);
	irq = gpio_to_irq(priv->gpio_vbus);
	ret = devm_request_irq(&pdev->dev, gpio_to_irq(priv->gpio_vbus),
			       hiusb_gpio_intr, IRQF_NO_SUSPEND |
			       IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			       "vbus_gpio_intr", priv);
        if (ret) {
		dev_err(&pdev->dev, "request gpio irq failed.\n");
		goto err_irq;
        }
	hi6220_phy_setup(priv, true);

	ret = usb_add_phy(&priv->phy, USB_PHY_TYPE_USB2);
	if (ret) {
		dev_err(&pdev->dev, "Can't register transceiver\n");
		goto err_irq;
	}

	return 0;

err_irq:
	gpio_free(priv->gpio_vbus);
	gpio_free(priv->gpio_id);
	return ret;
}

static int hi6220_phy_remove(struct platform_device *pdev)
{
	struct hi6220_priv *priv = platform_get_drvdata(pdev);

	clk_disable_unprepare(priv->clk);
	hi6220_phy_setup(priv, false);
	gpio_free(priv->gpio_vbus);
	gpio_free(priv->gpio_id);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int hi6220_phy_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hi6220_priv *priv = platform_get_drvdata(pdev);

	hi6220_phy_setup(priv, false);
	clk_disable_unprepare(priv->clk);
	return 0;

}

static int hi6220_phy_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hi6220_priv *priv = platform_get_drvdata(pdev);

	clk_prepare_enable(priv->clk);
	hi6220_phy_setup(priv, true);
	return 0;

}
#endif

static const struct of_device_id hi6220_phy_of_match[] = {
	{.compatible = "hisilicon,hi6220-usb-phy",},
	{ },
};
MODULE_DEVICE_TABLE(of, hi6220_phy_of_match);

static const struct dev_pm_ops hi6220_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hi6220_phy_suspend, hi6220_phy_resume)
};

static struct platform_driver hi6220_phy_driver = {
	.probe	= hi6220_phy_probe,
	.remove	= hi6220_phy_remove,
	.driver = {
		.name	= "hi6220-usb-phy",
		.of_match_table	= hi6220_phy_of_match,
		.pm = &hi6220_dev_pm_ops,
	}
};
module_platform_driver(hi6220_phy_driver);

MODULE_DESCRIPTION("HISILICON HISI USB PHY driver");
MODULE_ALIAS("platform:hi6220-usb-phy");
MODULE_LICENSE("GPL");
