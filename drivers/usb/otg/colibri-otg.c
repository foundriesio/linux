/*
 * drivers/usb/otg/colibri-otg.c
 *
 * OTG transceiver driver for Tegra UTMI phy with GPIO VBUS detection
 *
 * Copyright (C) 2010 NVIDIA Corp.
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2012 Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/colibri_usb.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>

#include <mach/gpio.h>

struct colibri_otg_data {
	struct otg_transceiver otg;
	spinlock_t lock;
	int irq;
	struct platform_device *host;
	struct platform_device *pdev;
	struct work_struct work;
};

static const char *tegra_state_name(enum usb_otg_state state)
{
	if (state == OTG_STATE_A_HOST)
		return "HOST";
	if (state == OTG_STATE_B_PERIPHERAL)
		return "PERIPHERAL";
	if (state == OTG_STATE_A_SUSPEND)
		return "SUSPEND";
	return "INVALID";
}

void tegra_start_host(struct colibri_otg_data *tegra)
{
	struct colibri_otg_platform_data *pdata = tegra->otg.dev->platform_data;
	if (!tegra->pdev) {
		tegra->pdev = pdata->host_register();
	}
}

void tegra_stop_host(struct colibri_otg_data *tegra)
{
	struct colibri_otg_platform_data *pdata = tegra->otg.dev->platform_data;
	if (tegra->pdev) {
		pdata->host_unregister(tegra->pdev);
		tegra->pdev = NULL;
	}
}

static void tegra_otg_notify_event(struct otg_transceiver *otg,
					enum usb_xceiv_events event)
{
	otg->last_event = event;
	atomic_notifier_call_chain(&otg->notifier, event, NULL);
}

static void irq_work(struct work_struct *work)
{
	struct colibri_otg_data *tegra =
		container_of(work, struct colibri_otg_data, work);
	struct otg_transceiver *otg = &tegra->otg;
	enum usb_otg_state from = otg->state;
	enum usb_otg_state to = OTG_STATE_UNDEFINED;
	unsigned long flags;

	spin_lock_irqsave(&tegra->lock, flags);

	/* Check client detect (High active) */
	mdelay(100);
	if (gpio_get_value(irq_to_gpio(tegra->irq)))
		to = OTG_STATE_B_PERIPHERAL;
	else
		to = OTG_STATE_A_HOST;

	spin_unlock_irqrestore(&tegra->lock, flags);

	if (to != OTG_STATE_UNDEFINED) {
		otg->state = to;

		if (from != to) {
			dev_info(tegra->otg.dev, "%s --> %s\n",
				 tegra_state_name(from), tegra_state_name(to));

			if (to == OTG_STATE_B_PERIPHERAL) {
				if (from == OTG_STATE_A_HOST) {
					tegra_stop_host(tegra);
					tegra_otg_notify_event(otg, USB_EVENT_NONE);
				}
				if (otg->gadget) {
					usb_gadget_vbus_connect(otg->gadget);
					tegra_otg_notify_event(otg, USB_EVENT_VBUS);
				}
			} else if (to == OTG_STATE_A_HOST) {
				if (otg->gadget && (from == OTG_STATE_B_PERIPHERAL)) {
					usb_gadget_vbus_disconnect(otg->gadget);
					tegra_otg_notify_event(otg, USB_EVENT_NONE);
				}
				tegra_start_host(tegra);
				tegra_otg_notify_event(otg, USB_EVENT_ID);
			}
		}
	}
}

static irqreturn_t colibri_otg_irq(int irq, void *data)
{
	struct colibri_otg_data *tegra = data;

	schedule_work(&tegra->work);

	return IRQ_HANDLED;
}

static int colibri_otg_set_peripheral(struct otg_transceiver *otg,
				struct usb_gadget *gadget)
{
	struct colibri_otg_data *tegra;

	tegra = container_of(otg, struct colibri_otg_data, otg);
	otg->gadget = gadget;

	/* Set initial state */
	schedule_work(&tegra->work);

	return 0;
}

static int colibri_otg_set_host(struct otg_transceiver *otg,
				struct usb_bus *host)
{
	struct colibri_otg_data *tegra;

	tegra = container_of(otg, struct colibri_otg_data, otg);
	otg->host = host;

	return 0;
}

static int colibri_otg_set_power(struct otg_transceiver *otg, unsigned mA)
{
	return 0;
}

static int colibri_otg_set_suspend(struct otg_transceiver *otg, int suspend)
{
	return 0;
}

static int colibri_otg_probe(struct platform_device *pdev)
{
	struct colibri_otg_data *tegra;
	struct colibri_otg_platform_data *plat = pdev->dev.platform_data;
	int err, gpio_cd;

	if (!plat) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -ENODEV;
	}

	tegra = kzalloc(sizeof(struct colibri_otg_data), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	tegra->otg.dev = &pdev->dev;
	tegra->otg.label = "colibri-otg";
	tegra->otg.state = OTG_STATE_UNDEFINED;
	tegra->otg.set_host = colibri_otg_set_host;
	tegra->otg.set_peripheral = colibri_otg_set_peripheral;
	tegra->otg.set_suspend = colibri_otg_set_suspend;
	tegra->otg.set_power = colibri_otg_set_power;
	spin_lock_init(&tegra->lock);

	platform_set_drvdata(pdev, tegra);

	tegra->otg.state = OTG_STATE_A_SUSPEND;

	err = otg_set_transceiver(&tegra->otg);
	if (err) {
		dev_err(&pdev->dev, "can't register transceiver (%d)\n", err);
		goto err_otg;
	}

	gpio_cd = plat->cable_detect_gpio;
	err = gpio_request(gpio_cd, "USBC_DET");
	if (err)
		goto err_gpio;
	gpio_direction_input(gpio_cd);

	tegra->irq = gpio_to_irq(gpio_cd);
	err = request_threaded_irq(tegra->irq, colibri_otg_irq, NULL,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "colibri-otg USBC_DET", tegra);
	if (err) {
		dev_err(&pdev->dev, "Failed to register USB client detect IRQ\n");
		goto err_irq;
	}

	INIT_WORK (&tegra->work, irq_work);

	dev_info(&pdev->dev, "otg transceiver registered\n");

	return 0;

err_irq:
	gpio_free(gpio_cd);
err_gpio:
	otg_set_transceiver(NULL);
err_otg:
	platform_set_drvdata(pdev, NULL);
	kfree(tegra);
	return err;
}

static int __exit colibri_otg_remove(struct platform_device *pdev)
{
	struct colibri_otg_data *tegra = platform_get_drvdata(pdev);

	free_irq(tegra->irq, tegra);
	gpio_free(irq_to_gpio(tegra->irq));
	otg_set_transceiver(NULL);
	platform_set_drvdata(pdev, NULL);
	kfree(tegra);

	return 0;
}

static struct platform_driver colibri_otg_driver = {
	.driver = {
		.name  = "colibri-otg",
	},
	.remove  = __exit_p(colibri_otg_remove),
	.probe   = colibri_otg_probe,
};

static int __init colibri_otg_init(void)
{
	return platform_driver_register(&colibri_otg_driver);
}
subsys_initcall(colibri_otg_init);

static void __exit colibri_otg_exit(void)
{
	platform_driver_unregister(&colibri_otg_driver);
}
module_exit(colibri_otg_exit);
