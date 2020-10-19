/*
 * Copyright (C) 2008-2019 Telechips Inc.
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
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <asm/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif
#include <linux/regmap.h>


#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <video/tcc/tccfb.h>

#define LOG_TAG "HDMI_PVLBJT"

//#define CONFIG_TCCFB_PVLBT_020_10_BACKLIGHT_TEST

/**
  * If power gpio of pvlbjt_020_01 set to low, hotplug detect pin of HDMI also goes to low.
  * If you do not use hot plug detect pin of HDMI port, please unmask the below define. */
//#define CONFIG_TCCFB_CONTROL_HDMIPANEL_POWER

extern int tccfb_register_ext_panel(struct lcd_panel *panel);
extern void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo, int specific_pclk);

struct pvlbjt_020_01_dev {
        struct i2c_client *client;
        struct regmap *i2c_regmap;

        /* gpio */
        #if defined(CONFIG_TCCFB_PVLBT_020_10_BACKLIGHT_TEST)
        int gpio_backlight_on;
        #endif
        int gpio_power_on;
        int gpio_reset;

        int power_status; /* 0 : turn off by fb driver, 1: turn on by fb driver, 2: turn off by external app, 3: turn on by external app */

        struct tcc_dp_device *fb_pdata;
 };

struct i2c_data_reg {
        unsigned char addr;
        unsigned short reg;
        unsigned char val;
        unsigned char mismatch;
};

static struct i2c_data_reg hdmi_rd_info[] = {
        {0xD4, 0x000D, 0xC0, 0},
        {0x44, 0x000D, 0x83, 0},
        {0x00, 0x0000, 0x00, 0},
};

static struct i2c_data_reg panel_regs[] = {
        /* 1920x720@60 PVLBJT_020_01 */
        {0xD4, 0x0005, 0x70, 0},
        {0xD4, 0x01CE, 0x4E, 0},
        {0x44, 0x0001, 0xC8, 0},
        /* ##Audio */
        {0x44, 0x0120, 0x80, 0},
        {0x44, 0x0130, 0x80, 0},
        {0x44, 0x2129, 0x1C, 0},
        {0xD4, 0x0140, 0x01, 0},

        /* ##GPIO Setting */
        /* IO RX_EN should be '1' */
        {0xD4, 0x022f, 0x2f, 0},
        {0xD4, 0x022d, 0x63, 1},
        /* OLDI DES PT1 enable */
        {0xD4, 0x0006, 0x01, 0},
        /* OLDI DES ROUTE PT1 pins to GMSLPT1 */
        {0xD4, 0x0071, 0x02, 0},
        /* HDMI Ser PT1 Enable */
        {0x44, 0x0001, 0xD8, 0},
        /*-PALEN GPIO----------------------------------------------*/
        /* GPIO# 6 RX/TX TX ID 6 */
        {0x44, 0x0212, 0x41, 1},
        {0x44, 0x0213, 0x46, 0},
        {0x44, 0x0214, 0x40, 0},
        /* GPIO# 7 RX/TX TX ID 7 */
        {0x44, 0x0215, 0x41, 1},
        {0x44, 0x0216, 0x47, 0},
        {0x44, 0x0217, 0x40, 0},
        /* GPIO# 8 RX/TX TX ID 8 */
        {0x44, 0x0218, 0x41, 1},
        {0x44, 0x0219, 0x48, 0},
        {0x44, 0x021a, 0x40, 0},
        {0x44, 0x0030, 0x48, 0},
        {0x44, 0x0031, 0x88, 0},

        /* GPIO# 2 RX/TX RX ID 6 - BLEN SER_GPIO6->DES_GPIO2 */
        {0xD4, 0x0206, 0x84, 1},
        {0xD4, 0x0207, 0xb0, 0},
        {0xD4, 0x0208, 0x26, 0},
        {0x44, 0x0212, 0x43, 1},

        /* GPIO# 17 RX/TX RX ID 8 LCDRST SER_GPIO8->DES_GPIO17 */
        {0xD4, 0x0233, 0x84, 1},
        {0xD4, 0x0234, 0xb0, 0},
        {0xD4, 0x0235, 0x28, 0},
        {0x44, 0x0218, 0x43, 1},

        /* GPIO# 18 RX/TX RX ID 7 LCDON SER_GPIO7->DES_GPIO18 */
        {0xD4, 0x0236, 0x84, 1},
        {0xD4, 0x0237, 0xb0, 0},
        {0xD4, 0x0238, 0x27, 0},
        {0x44, 0x0215, 0x43, 1},
        /*---------------------------------------------------------*/

        {0x00, 0x0000, 0x00, 0},
};

static int pvlbjt_020_01_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
        int ret;
        struct pvlbjt_020_01_dev *dev;
        if(panel == NULL) {
                pr_err("[ERROR][%s] %s panel is NULL\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        if(fb_pdata == NULL) {
                pr_err("[ERROR][%s] %s fb device is NULL\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }
        dev = (struct pvlbjt_020_01_dev *)panel->panel_data;
        if(dev == NULL) {
                pr_err("[ERROR][%s] %s pvlbjt_020_01_dev is NULL\r\n",
                                                        LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        /* store display device */
        dev ->fb_pdata = fb_pdata;

        /* turn on by fb driver */
        dev->power_status = 1;

        fb_pdata->FbPowerState = true;
        fb_pdata->FbUpdateType = FB_SC_RDMA_UPDATE;
        fb_pdata->DispDeviceType = TCC_OUTPUT_HDMI;

        tca_vioc_displayblock_powerOn(fb_pdata, 1 /* SKIP */);

        return 0;

err_out:
        return ret;
}

static int pvlbjt_020_01_check_i2c(struct pvlbjt_020_01_dev *dev, struct i2c_data_reg *i2c_data_reg)
{

        int ret;
        int val, loop, retry = 0;
        struct regmap *regmap;
        struct i2c_client *client;

        if(dev == NULL) {
                pr_err("[ERROR][%s] %s dev is null\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        regmap = dev->i2c_regmap;
        if(regmap == NULL) {
                pr_err("[ERROR][%s] %s regmap is null\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        client = dev->client;
        if(client == NULL) {
                pr_err("[ERROR][%s] %s client is null\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        for(loop = 0;
                !(i2c_data_reg[loop].addr == 0 &&
                        i2c_data_reg[loop].reg == 0
                        && i2c_data_reg[loop].val == 0); loop++) {
                retry = 3;
                do {
                        client->addr = (i2c_data_reg[loop].addr >> 1);
                        regmap_read(regmap, i2c_data_reg[loop].reg, &val);
                        if(val != i2c_data_reg[loop].val) {
                                pr_info(" WARNING [%02d] A_0x%02x R_0x%02x 0x%02x : 0x%02x\r\n",
                                        loop, i2c_data_reg[loop].addr,
                                        i2c_data_reg[loop].reg, i2c_data_reg[loop].val, val);
                        } else {
                                break;
                        }
                }while((--retry) > 0);

                if(retry <= 0) {
                        ret = -EINVAL;
                        goto err_out;
                }
        }
        return 0;

err_out:
        return ret;
}


static int pvlbjt_020_01_update_i2c(struct pvlbjt_020_01_dev *dev, struct i2c_data_reg *i2c_data_reg)
{

        int ret;
        int val, loop, retry = 0;
        struct regmap *regmap;
        struct i2c_client *client;
        if(dev == NULL) {
                pr_err("[ERROR][%s] %s dev is null\r\n",
                                        LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        regmap = dev->i2c_regmap;
        if(regmap == NULL) {
                pr_err("[ERROR][%s] %s regmap is null\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        client = dev->client;
        if(client == NULL) {
                pr_err("[ERROR][%s] %s client is null\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        for(loop = 0;
                !(i2c_data_reg[loop].addr == 0 &&
                        i2c_data_reg[loop].reg == 0
                        && i2c_data_reg[loop].val == 0); loop++) {
                retry = 3;
                do {
                        client->addr = (i2c_data_reg[loop].addr >> 1);
                        ret = regmap_write(regmap, i2c_data_reg[loop].reg, i2c_data_reg[loop].val); udelay(1);
                        regmap_read(regmap, i2c_data_reg[loop].reg, &val);
                        if(!i2c_data_reg[loop].val && val != i2c_data_reg[loop].val) {
                                pr_info(" WARNING [%02d] A_0x%02x R_0x%02x 0x%02x : 0x%02x\r\n",
                                        loop, i2c_data_reg[loop].addr,
                                        i2c_data_reg[loop].reg, i2c_data_reg[loop].val, val);
                        } else {
                                break;
                        }
                }while((--retry) > 0);
        }
        return 0;

err_out:
        return ret;
}

static int pvlbjt_020_01_set_power_internal(struct pvlbjt_020_01_dev *dev, int on)
{
        int ret;
        struct i2c_client *client;
        struct tcc_dp_device *fb_pdata;
        if(dev == NULL) {
                pr_err("[ERROR][%s] %s dev is null\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }
        if(dev->fb_pdata == NULL) {
                pr_err("[ERROR][%s] %s display device is null\r\n",
                                                        LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        client = dev->client;

        /* overwrite display device */
        fb_pdata = dev->fb_pdata;

        if(on) {
                pr_info("%s power on\r\n", __func__);
                if(gpio_is_valid(dev->gpio_power_on))
                        gpio_set_value(dev->gpio_power_on, 1);

                if(gpio_is_valid(dev->gpio_reset))
                        gpio_set_value(dev->gpio_reset, 0);

                mdelay(1);
                if(gpio_is_valid(dev->gpio_reset))
                        gpio_set_value(dev->gpio_reset, 1);

                if(pvlbjt_020_01_check_i2c(dev, hdmi_rd_info) < 0) {
                        pr_err("[ERROR][%s] %s failed set main reg\r\n",
                                                                LOG_TAG, __func__);
                        ret = -EIO;
                        goto err_out;
                }

                if(pvlbjt_020_01_update_i2c(dev, panel_regs) < 0) {
                        pr_err("[ERROR][%s] %s failed set main reg\r\n",
                                                                LOG_TAG, __func__);
                        ret = -EIO;
                        goto err_out;
                }

                #if defined(CONFIG_TCCFB_PVLBT_020_10_BACKLIGHT_TEST)
                if(gpio_is_valid(dev->gpio_backlight_on))
                        gpio_set_value(dev->gpio_backlight_on, 1);
                #endif
        } else {
                pr_info("%s power off\r\n", __func__);
                #if defined(CONFIG_TCCFB_PVLBT_020_10_BACKLIGHT_TEST)
                if(gpio_is_valid(dev->gpio_backlight_on))
                        gpio_set_value(dev->gpio_backlight_on, 0);
                #endif

                #if defined(CONFIG_TCCFB_CONTROL_HDMIPANEL_POWER)
                if(gpio_is_valid(dev->gpio_power_on))
                        gpio_set_value(dev->gpio_power_on, 0);
                #endif

                if(gpio_is_valid(dev->gpio_reset))
                        gpio_set_value(dev->gpio_reset, 0);
        }
        return 0;

err_out:
        return ret;
}

static int pvlbjt_020_01_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
        int ret;
        struct i2c_client *client;
        struct pvlbjt_020_01_dev *dev = (struct pvlbjt_020_01_dev *)panel->panel_data;

        if(dev == NULL) {
                pr_err("[ERROR][%s] %s dev is null\r\n",
                                                LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }
        if(dev->fb_pdata == NULL) {
                pr_err("[ERROR][%s] %s display device is null\r\n",
                                                        LOG_TAG, __func__);
                ret = -EINVAL;
                goto err_out;
        }

        client = dev->client;

        /**
         * power_status    | current command | status result
         * ----------------|-----------------|---------------
         * turn off by fb  | turn on by fb   |turn on panel\n set power_status to turn on by fb
         * turn off by fb  | turn on by app  |turn on panel\n set power_status to turn on by app
         * turn off by app | turn on by fb   |SKIP
         * turn off by app | turn on by app  |turn on panel\n set power_status to turn on by app
         * turn on  by fb  | turn on by app  |set power_status to turn on by app
         * turn on  by app | turn on by fb   |SKIP
         * turn on  by app | turn off by fb  |turn off panel\n set power_status to turn off by fb
         */
        switch(on) {
                case 0: /* turn off by fb driver */
                        pr_info("%s turn off by fb, power status is %d\r\n", __func__, dev->power_status);
                        if(dev->power_status == 1 || dev->power_status == 3) {
                                pvlbjt_020_01_set_power_internal(dev, 0);
                                dev->power_status = 0;
                        }
                        break;

                case 1: /* turn on by fb driver */
                        pr_info("%s turn on by fb, power status is %d\r\n", __func__, dev->power_status);
                        if(dev->power_status == 0) {
                                pvlbjt_020_01_set_power_internal(dev, 1);
                                dev->power_status = 1;
                        }
                        break;

                case 2: /* turn off by external app */
                        pr_info("%s turn off by external app, power status is %d\r\n", __func__, dev->power_status);
                        if(dev->power_status == 1 || dev->power_status == 3) {
                                pvlbjt_020_01_set_power_internal(dev, 0);
                                dev->power_status = 2;
                        }
                        break;

                case 3: /* turn on by external app */
                        pr_info("%s turn on by external app, power status is %d\r\n", __func__, dev->power_status);
                        if(dev->power_status < 3) {
                                if(dev->power_status != 1) {
                                        pvlbjt_020_01_set_power_internal(dev, 1);
                                }
                                dev->power_status = 3;
                        }
                        break;
        }
        return 0
err_out:
        return ret;
}

static struct lcd_panel pvlbjt_020_01_panel = {
        .name           = "HDMI_pvlbjt_020_01",
        .manufacturer   = "Telechips",
        .id             = PANEL_ID_HDMI,
        .xres           = 1920,
        .yres           = 720,
        .init           = pvlbjt_020_01_panel_init,
        .set_power      = pvlbjt_020_01_set_power,
};

static const struct regmap_config hdmi_pvlbjt_020_01_regmap_config = {
        .reg_bits = 16,
        .val_bits = 8,
};

static int hdmi_pvlbjt_020_01_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        int ret = -ENOMEM;
        int hdmi_ext_panel = 0;

        struct pvlbjt_020_01_dev *dev;
        struct device_node *np = client->dev.of_node;


        dev = devm_kzalloc(&client->dev, sizeof(struct pvlbjt_020_01_dev), GFP_KERNEL);
        if(dev == NULL) {
                pr_err("[ERROR][%s] %s failed create dev\r\n",
                                                LOG_TAG, __func__);
                goto api_end;
        }

        dev->client = client;
        i2c_set_clientdata(client, dev);

        of_property_read_u32(np, "hdmi-ext-panel", (u32 *)&hdmi_ext_panel);

        dev->i2c_regmap = devm_regmap_init_i2c(client, &hdmi_pvlbjt_020_01_regmap_config);

        #if defined(CONFIG_TCCFB_PVLBT_020_10_BACKLIGHT_TEST)
        dev->gpio_backlight_on = of_get_named_gpio(np, "backlight-on-gpios", 0);
        if(gpio_is_valid(dev->gpio_backlight_on)) {
                gpio_request(dev->gpio_backlight_on, "backlight_on");
                gpio_direction_output(dev->gpio_backlight_on, 0);
        }
        #endif

        dev->gpio_power_on = of_get_named_gpio(np, "power-on-gpios", 0);
        if(gpio_is_valid(dev->gpio_power_on)) {
                gpio_request(dev->gpio_power_on, "panel_power");
                gpio_direction_output(dev->gpio_power_on, 1);
        }

        dev->gpio_reset = of_get_named_gpio(np, "reset-gpios", 0);
        if(gpio_is_valid(dev->gpio_reset)) {
                gpio_request(dev->gpio_reset, "panel_reset");
                gpio_direction_output(dev->gpio_reset, 1);
        }

        pvlbjt_020_01_panel.panel_data = (void*)dev;
#ifdef CONFIG_FB_VIOC
        if(hdmi_ext_panel) {
                tccfb_register_ext_panel(&pvlbjt_020_01_panel);
        } else {
                tccfb_register_panel(&pvlbjt_020_01_panel);
        }
#endif
        ret = 0;

api_end:
        return ret;
}

static int hdmi_pvlbjt_020_01_remove(struct i2c_client *client)
{
        struct pvlbjt_020_01_dev *dev = i2c_get_clientdata(client);
        if(dev != NULL) {
                devm_kfree(&client->dev, dev);
        }
        return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hdmi_pvlbjt_020_01of_match[] = {
        { .compatible = "telechips,pvlbjt_020_01", },
        {},
};
MODULE_DEVICE_TABLE(of, hdmi_pvlbjt_020_01of_match);
#endif

static const struct i2c_device_id hdmi_pvlbjt_020_01_id[] = {
        { "pvlbjt_020_01", 0 },
        {}
};
MODULE_DEVICE_TABLE(i2c, hdmi_pvlbjt_020_01_id);

static struct i2c_driver hdmi_pvlbjt_020_01_driver = {
        .driver = {
                .name = "hdmi-pvlbjt_020_01",
                .owner = THIS_MODULE,
                #ifdef CONFIG_OF
                .of_match_table = of_match_ptr(hdmi_pvlbjt_020_01of_match),
                #endif
        },
        .probe = hdmi_pvlbjt_020_01_probe,
        .remove = hdmi_pvlbjt_020_01_remove,
        .id_table = hdmi_pvlbjt_020_01_id,
};

static int __init pvlbjt_020_01_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&hdmi_pvlbjt_020_01_driver);
	if (ret != 0) {
		pr_err("[ERROR][%s] pvlbjt_020_01 I2C registration failed %d\n",
                                                                LOG_TAG, __func__, ret);
		return ret;
	}

	return 0;
}
subsys_initcall(pvlbjt_020_01_i2c_init);

static void __exit pvlbjt_020_01_i2c_exit(void)
{
	i2c_del_driver(&hdmi_pvlbjt_020_01_driver);
}
module_exit(pvlbjt_020_01_i2c_exit);

MODULE_DESCRIPTION("pvlbjt_020_01 Driver");
MODULE_LICENSE("GPL");
