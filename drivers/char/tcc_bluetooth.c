/*
 * Bluetooth Broadcomm  and low power control via GPIO
 *
 *  Copyright (C) 2011 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/rfkill.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

static struct rfkill *bt_rfkill;
static bool bt_enabled;
//static bool host_wake_uart_enabled;
//static bool wake_uart_enabled;

static int bt_wake_port		= 0;
static int bt_hwake_port	= 0;
static int bt_reg_on_port	= 0;
static int bt_power_port	= 0;
static int bt_power_1p8v_port	= 0;
static int bt_power_3p3v_port	= 0;

/*
struct bcm_bt_lpm {
	int wake;
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uport;

	struct wake_lock wake_lock;
	char wake_lock_name[100];
} bt_lpm;
*/

static int bt_reg_on_skip = 1;

/* rfkill_ops callback. Turn transmitter on when blocked is false */
static int tcc_bt_rfkill_set_power(void *data, bool blocked)
{
    printk("[## BT ##] tcc_bt_rfkill_set_power [%d]\n", blocked);

    if (!blocked)
    {
        if (bt_reg_on_skip)
            bt_reg_on_skip = 0;
        else
        {
            if (gpio_is_valid(bt_wake_port))
                gpio_direction_output(bt_wake_port, 1);
            if (gpio_is_valid(bt_reg_on_port))
                gpio_set_value_cansleep(bt_reg_on_port, 1);
        }
    }
    else
    {
        if (gpio_is_valid(bt_reg_on_port))
            gpio_set_value_cansleep(bt_reg_on_port, 0);
    }

    bt_enabled = !blocked;
    return 0;
}

static const struct rfkill_ops tcc_bt_rfkill_ops = {
	.set_block = tcc_bt_rfkill_set_power,
};

#if 0
static void set_wake_locked(int wake)
{
	bt_lpm.wake = wake;

	if (!wake)
		wake_unlock(&bt_lpm.wake_lock);

	if (!wake_uart_enabled && wake)
		omap_uart_enable(2);

	gpio_set_value(BT_WAKE_GPIO, wake);

	if (wake_uart_enabled && !wake)
		omap_uart_disable(2);

	wake_uart_enabled = wake;
}
#endif

#if 0
static enum hrtimer_restart enter_lpm(struct hrtimer *timer) {
	unsigned long flags;
	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	set_wake_locked(0);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);
	return HRTIMER_NORESTART;
}
#endif

void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport) {
/*	bt_lpm.uport = uport;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);
*/
}
EXPORT_SYMBOL(bcm_bt_lpm_exit_lpm_locked);

#if 0
static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;
	bt_lpm.host_wake = host_wake;
	if (host_wake) {
		wake_lock(&bt_lpm.wake_lock);
		if (!host_wake_uart_enabled)
			omap_uart_enable(2);
	} else  {
		if (host_wake_uart_enabled)
			omap_uart_disable(2);
		// Take a timed wakelock, so that upper layers can take it.
		// The chipset deasserts the hostwake lock, when there is no
		// more data to send.
		wake_lock_timeout(&bt_lpm.wake_lock, HZ/2);
	}
	host_wake_uart_enabled = host_wake;
}
#endif

#if 0
static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;
	unsigned long flags;
	host_wake = gpio_get_value(BT_HOST_WAKE_GPIO);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}
	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	update_host_wake_locked(host_wake);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);
	return IRQ_HANDLED;
}
#endif

#if 0
static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int irq;
	int ret;
	int rc;

	rc = gpio_request(BT_WAKE_GPIO, "bcm4330_wake_gpio");
	if (unlikely(rc)) {
		return rc;
	}

	rc = gpio_request(BT_HOST_WAKE_GPIO, "bcm4330_host_wake_gpio");
	if (unlikely(rc)) {
		gpio_free(BT_WAKE_GPIO);
		return rc;
	}

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;

	irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
		"bt host_wake", NULL);
	if (ret) {
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		return ret;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		return ret;
	}

	gpio_direction_output(BT_WAKE_GPIO, 0);
	gpio_direction_input(BT_HOST_WAKE_GPIO);

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bt_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.wake_lock_name);
	
	return 0;
}
#endif

static struct of_device_id tcc_bluetooth_dt_match[2] = {
    {.name = "", .type = "", .compatible = "telechips, tcc_bluetooth", NULL},
    {.name = "", .type = "", .compatible = "", NULL},
};
MODULE_DEVICE_TABLE(of, tcc_bluetooth_dt_match);

static int tcc_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret = 0;
	const struct of_device_id *match;	
    struct device_node *np = pdev->dev.of_node;

	printk("[## BT ##] tcc_bluetooth_probe\n");
	
	match = of_match_device(tcc_bluetooth_dt_match, &pdev->dev);
	if(!match)
	{
        printk("[## BT ##] of_match_device fail %s \n", __func__);
		return -EINVAL;
	}
	
	bt_power_1p8v_port	= 0;
	bt_power_3p3v_port	= 0;
	bt_power_port		= 0;
	bt_wake_port		= 0;
	bt_hwake_port		= 0;
	bt_reg_on_port		= 0;

    if (np) {
	bt_power_1p8v_port = of_get_named_gpio(np, "bt_power_1p8v-gpio", 0);
        if (gpio_is_valid(bt_power_1p8v_port)) {
            gpio_request(bt_power_1p8v_port, "bt_power_1p8v");
            gpio_direction_output(bt_power_1p8v_port, 1);
        } else {
            //printk("[## BT ##] %s: err to get gpios: ret:%x\n", __func__, bt_power_1p8v_port);
            bt_power_1p8v_port = -1;
        }

	bt_power_3p3v_port = of_get_named_gpio(np, "bt_power_3p3v-gpio", 0);
        if (gpio_is_valid(bt_power_3p3v_port)) {
            gpio_request(bt_power_3p3v_port, "bt_power_3p3v");
            gpio_direction_output(bt_power_3p3v_port, 1);
        } else {
            //printk("[## BT ##] %s: err to get gpios: ret:%x\n", __func__, bt_power_3p3v_port);
            bt_power_3p3v_port = -1;
        }

        bt_power_port = of_get_named_gpio(np, "bt_power-gpio", 0);
        if (gpio_is_valid(bt_power_port)) {
            gpio_request(bt_power_port, "bt_power");
            gpio_direction_output(bt_power_port, 1);
            mdelay(10);
        } else {
            //printk("[## BT ##] %s: err to get gpios: ret:%x\n", __func__, bt_power_port);
            bt_power_port = -1;
        }

        bt_reg_on_port = of_get_named_gpio(np, "bt_reg_on-gpio", 0);
        if (gpio_is_valid(bt_reg_on_port)) {
            gpio_request(bt_reg_on_port, "bt_reg_on");
            gpio_direction_output(bt_reg_on_port, 0);
        } else {
            printk("[## BT ##] %s: err to get gpios: ret:%x\n", __func__, bt_reg_on_port);
            bt_reg_on_port = -1;
        }

        bt_wake_port = of_get_named_gpio(np, "bt_wake-gpio", 0);
        if (gpio_is_valid(bt_wake_port)) {
            gpio_request(bt_wake_port, "bt_wake");
            gpio_direction_output(bt_wake_port, 1);
        } else {
            //printk("[## BT ##] %s: err to get gpios: ret:%x\n", __func__, bt_wake_port);
            bt_wake_port = -1;
        }

        bt_hwake_port = of_get_named_gpio(np, "bt_hwake-gpio", 0);
        if (gpio_is_valid(bt_hwake_port)) {
            gpio_request(bt_hwake_port, "bt_hwake");
            gpio_direction_input(bt_hwake_port);
        } else {
            //printk("[## BT ##] %s: err to get gpios: ret:%x\n", __func__, bt_hwake_port);
            bt_hwake_port = -1;
        }
    }
/*
	rc = gpio_request(BT_RESET_GPIO, "bcm4330_nreset_gpip");
	if (unlikely(rc)) {
		return rc;
	}

	rc = gpio_request(BT_REG_GPIO, "bcm4330_nshutdown_gpio");
	if (unlikely(rc)) {
		gpio_free(BT_RESET_GPIO);
		return rc;
	}
*/
	bt_rfkill = rfkill_alloc("Telechips Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &tcc_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		//gpio_free(BT_RESET_GPIO);
		//gpio_free(BT_REG_GPIO);
		printk("[## BT ##] rfkill_alloc failed \n");
		return -ENOMEM;
	}

	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		printk("[## BT ##] rfkill_register failed \n");
		rfkill_destroy(bt_rfkill);
		//gpio_free(BT_RESET_GPIO);
		//gpio_free(BT_REG_GPIO);
		return -1;
	}
	printk("[## BT ##] rfkill_register Telechips Bluetooth \n");

	rfkill_set_states(bt_rfkill, true, false);
//	tcc_bt_rfkill_set_power(NULL, true);

#if 0//defined (CONFIG_TCC_BRCM_BCM4330_MODULE_SUPPORT)
	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);

		//gpio_free(BT_RESET_GPIO);
		//gpio_free(BT_REG_GPIO);
	}
#endif

	return ret;
}

static int tcc_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	printk("[## BT ##] tcc_bluetooth_remove \n");
/*
	gpio_free(BT_REG_GPIO);
	gpio_free(BT_RESET_GPIO);
	gpio_free(BT_WAKE_GPIO);
	gpio_free(BT_HOST_WAKE_GPIO);

	wake_lock_destroy(&bt_lpm.wake_lock);
*/
	return 0;
}

int tcc_bluetooth_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("[## BT ##] tcc_bluetooth_suspend \n");
/*
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	int host_wake;

	disable_irq(irq);
	host_wake = gpio_get_value(BT_HOST_WAKE_GPIO);

	if (host_wake) {
		enable_irq(irq);
		return -EBUSY;
	}
*/
	return 0;
}

int tcc_bluetooth_resume(struct platform_device *pdev)
{
	printk("[## BT ##] tcc_bluetooth_resume \n");
/*
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	enable_irq(irq);
*/
	return 0;
}

static struct platform_driver tcc_bluetooth_platform_driver = {
	.probe = tcc_bluetooth_probe,
	.remove = tcc_bluetooth_remove,
	.suspend = tcc_bluetooth_suspend,
	.resume = tcc_bluetooth_resume,
	.driver = {
		   .name = "tcc_bluetooth_rfkill",
		   .owner = THIS_MODULE,
		   .of_match_table = tcc_bluetooth_dt_match,
		   },
};

static int __init tcc_bluetooth_init(void)
{
	bt_enabled = false;

	printk("[## BT ##] tcc_bluetooth_init\n");
	return platform_driver_register(&tcc_bluetooth_platform_driver);
}

static void __exit tcc_bluetooth_exit(void)
{
	platform_driver_unregister(&tcc_bluetooth_platform_driver);
}

module_init(tcc_bluetooth_init);
module_exit(tcc_bluetooth_exit);

MODULE_DESCRIPTION("telechips_bluetooth");
MODULE_AUTHOR("Jaikumar Ganesh <jaikumar@google.com>");
MODULE_LICENSE("GPL");
