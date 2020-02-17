/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        hdmi_1920x720_adv7613.c
*  \brief       HDMI TX controller driver
*  \details
*  \version     1.0
*  \date        2014-2015
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular
purpose or non-infringement  of  any  patent,  copyright  or  other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source  code or the  use in the
source code.
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure
Agreement between Telechips and Company.
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


/* default i2c slave address */
#define DEFAULT_MAIN_ADDR       0x4C
#define DEFAULT_CEC_ADDR        0x40
#define DEFAULT_INFO_ADDR       0x3E
#define DEFAULT_DPLL_ADDR       0x26
#define DEFAULT_KSV_ADDR        0x32
#define DEFAULT_EDID_ADDR       0x36
#define DEFAULT_HDMI_ADDR       0x34
#define DEFAULT_CP_ADDR         0x22
#define DEFAULT_LVDS_ADDR       0x60
#define DEFAULT_DELAY_ADDR      0xFF

//#define HDMI_ADV7613_DEBUG
#ifdef HDMI_ADV7613_DEBUG
#define dprintk(fmt, args...)                                                  \
                 pr_info("\e[33m[%s:%d] \e[0m" fmt, __func__, __LINE__, ##args);
#else
#define dprintk(fmt, args...)
#endif

//#define CONFIG_TCCFB_ADV7613_BACKLIGHT_TEST

/**
  * If power gpio of ADV7613 set to low, hotplug detect pin of HDMI also goes to low.
  * If you do not use hot plug detect pin of HDMI port, please unmask the below define. */
//#define CONFIG_TCCFB_CONTROL_HDMIPANEL_POWER

extern int tccfb_register_ext_panel(struct lcd_panel *panel);
extern void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo, int specific_pclk);

struct adv7613_dev {
        struct i2c_client *client;
        struct regmap *i2c_regmap;

        /* slave address */
        struct {
                unsigned char main;
                unsigned char cec;
                unsigned char info;
                unsigned char dpll;
                unsigned char ksv;
                unsigned char edid;
                unsigned char hdmi;
                unsigned char cp;
                unsigned char lvds;
        }i2c_slave_address;

        /* gpio */
        #if defined(CONFIG_TCCFB_ADV7613_BACKLIGHT_TEST)
        int gpio_backlight_on;
        #endif
        int gpio_power_on;
        int gpio_reset;

        int power_status; /* 0 : turn off by fb driver, 1: turn on by fb driver, 2: turn off by external app, 3: turn on by external app */

        struct tcc_dp_device *fb_pdata;
 };

struct i2c_data_reg {
        unsigned char addr;
        unsigned char reg;
        unsigned char val;
};


/* 1920x720p@60 HDMI Input_LVDS Output Port A and B*/
static struct i2c_data_reg main_data_reg[] = {
        {DEFAULT_MAIN_ADDR, 0xFF, 0x80}, /*  ;default 0x00, I2C reset */
        {DEFAULT_DELAY_ADDR,0x00, 0x05}, /* wait 5ms */
        {DEFAULT_MAIN_ADDR, 0xF4, 0x80}, /* ;default 0x00, CEC Map Address set to 0x80 */
        {DEFAULT_MAIN_ADDR, 0xF5, 0x7C}, /* ;default 0x00, Infoframe Map Address set to 0x7C */
        {DEFAULT_MAIN_ADDR, 0xF8, 0x4C}, /* ;default 0x00, DPLL Map Address set to 0x4C */
        {DEFAULT_MAIN_ADDR, 0xF9, 0x64}, /* ;default 0x00, KSV Map Address set to 0x64 */
        {DEFAULT_MAIN_ADDR, 0xFA, 0x6C}, /* ;default 0x00, EDID Map Address set to 0x6C */
        {DEFAULT_MAIN_ADDR, 0xFB, 0x68}, /* ;default 0x00, HDMI Map Address set to 0x68 */
        {DEFAULT_MAIN_ADDR, 0xFD, 0x44}, /* ;default 0x00, CP Map Address set to 0x44 */
        {DEFAULT_MAIN_ADDR, 0xE9, 0xC0}, /* ;default 0x00, LVDS Map Address set to 0xC0 */
        {DEFAULT_MAIN_ADDR, 0x00, 0x12}, /* ;default 0x08, [5:0] - VID_STD[5:0] = 6'b010010 - WXGA 1360x768p@60Hz */
        {DEFAULT_MAIN_ADDR, 0x01, 0x06}, /* ;default 0x06, [3:0] - Prim_Mode[3:0] = 4'b0110 - HDMI-Gr */
        {DEFAULT_MAIN_ADDR, 0x02, 0xF2}, /* ;default 0xF0, [7:4] - INP_COLOR_SPACE[3:0] = 4'b1111 - color space determined by HDMI block, [1] RGB_OUT - 1'b1 - RGB color space output */
        {DEFAULT_MAIN_ADDR, 0x03, 0x42}, /* ;default 0x00, [7:0] - OP_FORMAT_SEL[7:0] = 8'b01000010 - 36-bit 4:4:4 SDR Mode 0 */
        {DEFAULT_MAIN_ADDR, 0x04, 0x63}, /* ;enable CP timing adjustment */
        {DEFAULT_MAIN_ADDR, 0x05, 0x20}, /* ;default 0x2C  [2] - AVCODE_INSERT_EN = 1'b0 added 0x20 disable data blanking for script reload */
        {DEFAULT_MAIN_ADDR, 0x0C, 0x42}, /* ;default 0x62, [5] - POWER_DOWN = 1'b0 - Powers up part  */
        {DEFAULT_MAIN_ADDR, 0x15, 0xAE}, /* ;default 0xBE, [4] - TRI_AUDIO = 1'b0 = untristate Audio , {3] - TRI_LLC = 1'b1 = tristate LLC, Bit{1] - TRI_PIX = 1'b1 = Tristate Pixel Pads  */
        {0x00, 0x00, 0x00}
};

struct i2c_data_reg hdmi_rd_info[] = {
        {DEFAULT_HDMI_ADDR, 0xea, 0x20},
        {DEFAULT_HDMI_ADDR, 0xeb, 0xc4},
        {DEFAULT_HDMI_ADDR, 0x00, 0x00},
        {0x98, 0xea, 0x20},
        {0x98, 0xeb, 0xc4},
        {0x00, 0x00, 0x00}
};

static struct i2c_data_reg cp_data_reg[] = {
        {DEFAULT_CP_ADDR, 0x6C, 0x00}, /* ;default 0x10, ADI Recommended write */
        {DEFAULT_CP_ADDR, 0x8B, 0x40}, /* ;HS DE adjustment */
        {DEFAULT_CP_ADDR, 0x8C, 0x02}, /* ;HS DE adjustment */
        {DEFAULT_CP_ADDR, 0x8D, 0x02}, /* ;HS DE adjustment */
        {0x00, 0x00, 0x00}
};

static struct i2c_data_reg ksv_data_reg[] = {
        {DEFAULT_KSV_ADDR, 0x40, 0x81}, /* ;default 0x83, BCAPS[7:0] - Disable HDCP 1.1 features */
        {0x00, 0x00, 0x00}
};


static struct i2c_data_reg hdmi_data_reg[] = {
        {DEFAULT_HDMI_ADDR, 0x03, 0x98}, /* ;default 0x18, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x10, 0xA5}, /* ;default 0x25, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x1B, 0x08}, /* ;default 0x18, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x45, 0x04}, /* ;default 0x00, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x97, 0xC0}, /* ;default 0x80, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x3D, 0x10}, /* ;default 0x00, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x3E, 0x7B}, /* ;default 0x79, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x3F, 0x5E}, /* ;default 0x63, ADI Recommended Write */
        {DEFAULT_HDMI_ADDR, 0x4E, 0xFE}, /* ;default 0x7B, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x4F, 0x08}, /* ;default 0x63, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x57, 0xA3}, /* ;default 0x30, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x58, 0x07}, /* ;default 0x01, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x6F, 0x08}, /* ;default 0x00, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x83, 0xFE}, /* ;default 0xFF, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x86, 0x9B}, /* ;default 0x00, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x85, 0x10}, /* ;default 0x16, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x89, 0x01}, /* ;default 0x00, ADI recommended write */
        {DEFAULT_HDMI_ADDR, 0x9B, 0x03}, /* ;default 0x0B, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x9C, 0x80}, /* ;default 0x08, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x9C, 0xC0}, /* ;default 0x08, ADI Recommended write */
        {DEFAULT_HDMI_ADDR, 0x9C, 0x00}, /* ;default 0x08, ADI Recommended write */
        {0x00, 0x00, 0x00}
};

static struct i2c_data_reg lvds_data_reg[] = {
        {DEFAULT_LVDS_ADDR, 0x40, 0x08}, /* ;default 0x02, Bit [0] tx_mode_itu656 - 1'b0 = OLDI mode, Bit [1] tx_pdn - 1'b0 = LVDS TX powered on, Bit [3] tx_pll_en - 1'b1 =  power up LVDS PLL */
        {DEFAULT_LVDS_ADDR, 0x43, 0x03}, /* ;default 0x00, ADI Recommended write */
        {DEFAULT_LVDS_ADDR, 0x44, 0x00}, /* ;default 0x00, PLL GEAR < 200MHz */
        {DEFAULT_LVDS_ADDR, 0x45, 0x04}, /* ;default 0x1E, ADI Recommended write */
        {DEFAULT_LVDS_ADDR, 0x46, 0x53}, /* ;default 0x77, ADI Recommended write */
        {DEFAULT_LVDS_ADDR, 0x47, 0x03}, /* ;default 0x02, ADI Recommended write */
        {DEFAULT_LVDS_ADDR, 0x4C, 0x19}, /* ;default 0x71, Bit [6] tx_oldi_hs_pol - 1'b0 = HS Polarity Neg, Bit [5] tx_oldi_vs_pol - 1'b0 = VS Polarity Neg, Bit [4] tx_oldi_de_pol - 1'b1 =  DE Polarity Pos, Bit [3] tx_enable_ns_mapping - 1'b0 = normal oldi 8-bit mapping, Bit [2] tx_656_all_lanes_enable - 1'b0 = disable 656 data on lanes ,Bit [1] tx_oldi_balanced_mode - 1'b0 = Non DC balanced ,Bit [0] tx_color_mode - 1'b1 = 8-bit mode */
        {DEFAULT_LVDS_ADDR, 0x4E, 0x04}, /* ;default 0x08, Bit [3:2] tx_color_depth[1:0] - 2'b01 = 8-bit Mode,Bit [1] tx_int_res - 1'b0 = Res bit 0,Bit [0] tx_mux_int_res - 1'b0 = disable programming of res bit */
        {0x00, 0x00, 0x00}
};

static int adv7613_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
        int ret = -1;
        struct adv7613_dev *dev;
        do {
                if(panel == NULL) {
                        pr_err("%s panel is NULL\r\n", __func__);
                        break;
                }
                if(fb_pdata == NULL) {
                        pr_err("%s fb device is NULL\r\n", __func__);
                        break;
                }
                dev = (struct adv7613_dev *)panel->panel_data;
                if(dev == NULL) {
                        pr_err("%s adv7613_dev is NULL\r\n", __func__);
                        break;
                }

                /* store display device */
                dev ->fb_pdata = fb_pdata;

                /* turn on by fb driver */
                dev->power_status = 1;

                fb_pdata->FbPowerState = true;
                fb_pdata->FbUpdateType = FB_SC_RDMA_UPDATE;
                fb_pdata->DispDeviceType = TCC_OUTPUT_HDMI;

                tca_vioc_displayblock_powerOn(fb_pdata, 1 /* SKIP */);

                ret = 0;
        }while(0);

        return ret;
}

static int adv763_check(struct adv7613_dev *dev, struct i2c_data_reg *i2c_data_reg)
{

        int ret = -1;
        int val, loop, retry = 0;
        struct regmap *regmap;
        do {
                if(dev == NULL) {
                        pr_err("%s dev is null\r\n", __func__);
                        break;
                }
                regmap = dev->i2c_regmap;
                if(regmap == NULL) {
                        pr_err("%s regmap is null\r\n", __func__);
                        break;
                }

                for(loop = 0;
                        !(i2c_data_reg[loop].addr == 0 &&
                          i2c_data_reg[loop].reg == 0
                          && i2c_data_reg[loop].val == 0); loop++) {
                        if(DEFAULT_DELAY_ADDR == i2c_data_reg[loop].addr) {
                                mdelay(i2c_data_reg[loop].val);
                        } else {
                                retry = 3;
                                do {
                                        regmap_read(regmap, i2c_data_reg[loop].reg, &val);
                                        if(val != i2c_data_reg[loop].val) {
                                                pr_info(" WARNING [%02d] A_0x%02x R_0x%02x 0x%02x : 0x%02x\r\n",
                                                        loop, i2c_data_reg[loop].addr,
                                                        i2c_data_reg[loop].reg, i2c_data_reg[loop].val, val);
                                        } else {
                                                break;
                                        }
                                }while((--retry) > 0);

                                if(retry <= 0)
                                return ret;
                        }
                }
                ret = 0;
        } while(0);

        return ret;
}

static int adv763_update(struct adv7613_dev *dev, struct i2c_data_reg *i2c_data_reg)
{

        int ret = -1;
        int val, loop, retry = 0;
        struct regmap *regmap;
        do {
                if(dev == NULL) {
                        pr_err("%s dev is null\r\n", __func__);
                        break;
                }
                regmap = dev->i2c_regmap;
                if(regmap == NULL) {
                        pr_err("%s regmap is null\r\n", __func__);
                        break;
                }

                for(loop = 0;
                        !(i2c_data_reg[loop].addr == 0 &&
                          i2c_data_reg[loop].reg == 0
                          && i2c_data_reg[loop].val == 0); loop++) {
                        if(DEFAULT_DELAY_ADDR == i2c_data_reg[loop].addr) {
                                mdelay(i2c_data_reg[loop].val);
                        } else {
                                retry = 3;
                                do {
                                        ret = regmap_write(regmap, i2c_data_reg[loop].reg, i2c_data_reg[loop].val); udelay(1);
                                        regmap_read(regmap, i2c_data_reg[loop].reg, &val);
                                        if(val != i2c_data_reg[loop].val) {
                                                pr_info(" WARNING [%02d] A_0x%02x R_0x%02x 0x%02x : 0x%02x\r\n",
                                                        loop, i2c_data_reg[loop].addr,
                                                        i2c_data_reg[loop].reg, i2c_data_reg[loop].val, val);
                                        } else {
                                                break;
                                        }
                                }while((--retry) > 0);
                        }
                }
                ret = 0;
        } while(0);

        return ret;
}


static int adv7613_set_power_internal(struct adv7613_dev *dev, int on)
{
        int ret = -1;
        struct i2c_client *client;
        struct tcc_dp_device *fb_pdata;
        do {
                if(dev == NULL) {
                        pr_err("%s dev is null\r\n", __func__);
                        break;
                }
                if(dev->fb_pdata == NULL) {
                        pr_err("%s display device is null\r\n", __func__);
                        break;
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

                        client->addr = dev->i2c_slave_address.main;
                        if(adv763_check(dev, hdmi_rd_info) < 0) {
                                pr_err("%s failed set main reg\r\n", __func__);
                                break;
                        }

                        client->addr = dev->i2c_slave_address.main;
                        if(adv763_update(dev, main_data_reg) < 0) {
                                pr_err("%s failed set main reg\r\n", __func__);
                                break;
                        }

                        client->addr = dev->i2c_slave_address.cp;
                        if(adv763_update(dev, cp_data_reg) < 0) {
                                pr_err("%s failed set cp reg\r\n", __func__);
                                break;
                        }

                        client->addr = dev->i2c_slave_address.ksv;
                        if(adv763_update(dev, ksv_data_reg) < 0) {
                                pr_err("%s failed set ksv reg\r\n", __func__);
                                break;
                        }

                        client->addr = dev->i2c_slave_address.hdmi;
                        if(adv763_update(dev, hdmi_data_reg) < 0) {
                                pr_err("%s failed set hdmi reg\r\n", __func__);
                                break;
                        }

                        client->addr = dev->i2c_slave_address.lvds;
                        if(adv763_update(dev, lvds_data_reg) < 0) {
                                pr_err("%s failed set lvds reg\r\n", __func__);
                                break;
                        }

                        #if defined(CONFIG_TCCFB_ADV7613_BACKLIGHT_TEST)
                        if(gpio_is_valid(dev->gpio_backlight_on))
                                gpio_set_value(dev->gpio_backlight_on, 1);
                        #endif
                } else {
                        pr_info("%s power off\r\n", __func__);
                        #if defined(CONFIG_TCCFB_ADV7613_BACKLIGHT_TEST)
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
                ret = 0;
        } while(0);
        return ret;
}

static int adv7613_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
        int ret = -1;
        struct i2c_client *client;
        struct adv7613_dev *dev = (struct adv7613_dev *)panel->panel_data;

        do {
                if(dev == NULL) {
                        pr_err("%s dev is null\r\n", __func__);
                        break;
                }
                if(dev->fb_pdata == NULL) {
                        pr_err("%s display device is null\r\n", __func__);
                        break;
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
                                        adv7613_set_power_internal(dev, 0);
                                        dev->power_status = 0;
                                }
                                break;

                        case 1: /* turn on by fb driver */
                                pr_info("%s turn on by fb, power status is %d\r\n", __func__, dev->power_status);
                                if(dev->power_status == 0) {
                                        adv7613_set_power_internal(dev, 1);
                                        dev->power_status = 1;
                                }
                                break;

                        case 2: /* turn off by external app */
                                pr_info("%s turn off by external app, power status is %d\r\n", __func__, dev->power_status);
                                if(dev->power_status == 1 || dev->power_status == 3) {
                                        adv7613_set_power_internal(dev, 0);
                                        dev->power_status = 2;
                                }
                                break;

                        case 3: /* turn on by external app */
                                pr_info("%s turn on by external app, power status is %d\r\n", __func__, dev->power_status);
                                if(dev->power_status < 3) {
                                        if(dev->power_status != 1) {
                                                adv7613_set_power_internal(dev, 1);
                                        }
                                        dev->power_status = 3;
                                }
                                break;
                }
                ret = 0;
        } while(0);
        return ret;
}

static struct lcd_panel adv7613_panel = {
        .name           = "HDMI_ADV7613",
        .manufacturer   = "Telechips",
        .id             = PANEL_ID_HDMI,
        .xres           = 1920,
        .yres           = 720,
        .init           = adv7613_panel_init,
        .set_power      = adv7613_set_power,
};

static const struct regmap_config hdmi_adv7613_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,
};

static int hdmi_adv7613_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        int ret = -ENOMEM;
        int hdmi_ext_panel = 0;

        struct adv7613_dev *dev;
        struct device_node *np = client->dev.of_node;


        dev = devm_kzalloc(&client->dev, sizeof(struct adv7613_dev), GFP_KERNEL);
        if(dev == NULL) {
                pr_err("%s failed create dev\r\n", __func__);
                goto api_end;
        }

        dev->client = client;
        i2c_set_clientdata(client, dev);


        of_property_read_u32(np, "hdmi-ext-panel", (u32 *)&hdmi_ext_panel);
        of_property_read_u32(np, "main-slave", (u32 *)&dev->i2c_slave_address.main);
        of_property_read_u32(np, "cec-slave", (u32 *)&dev->i2c_slave_address.cec);
        of_property_read_u32(np, "info-slave", (u32 *)&dev->i2c_slave_address.info);
        of_property_read_u32(np, "dpll-slave", (u32 *)&dev->i2c_slave_address.dpll);
        of_property_read_u32(np, "ksv-slave", (u32 *)&dev->i2c_slave_address.ksv);
        of_property_read_u32(np, "edid-slave", (u32 *)&dev->i2c_slave_address.edid);
        of_property_read_u32(np, "hdmi-slave", (u32 *)&dev->i2c_slave_address.hdmi);
        of_property_read_u32(np, "cp-slave", (u32 *)&dev->i2c_slave_address.cp);
        of_property_read_u32(np, "lvds-slave", (u32 *)&dev->i2c_slave_address.lvds);

        #if defined(HDMI_ADV7613_DEBUG)
        pr_info("%s DUMP\r\n", __func__);
        pr_info("hdmi-ext-panel = %d\r\n"
                "main-slave     = 0x%02x\r\n"
                "cec-slave      = 0x%02x\r\n"
                "info-slave     = 0x%02x\r\n"
                "dpll-slave     = 0x%02x\r\n"
                "ksv-slave      = 0x%02x\r\n"
                "edid-slave     = 0x%02x\r\n"
                "hdmi-slave     = 0x%02x\r\n"
                "cp-slave       = 0x%02x\r\n"
                "lvds-slave     = 0x%02x\r\n\r\n",
                hdmi_ext_panel,
                dev->i2c_slave_address.main,
                dev->i2c_slave_address.cec,
                dev->i2c_slave_address.info,
                dev->i2c_slave_address.dpll,
                dev->i2c_slave_address.ksv,
                dev->i2c_slave_address.edid,
                dev->i2c_slave_address.hdmi,
                dev->i2c_slave_address.cp,
                dev->i2c_slave_address.lvds);
        #endif

        dev->i2c_regmap = devm_regmap_init_i2c(client, &hdmi_adv7613_regmap_config);

        #if defined(CONFIG_TCCFB_ADV7613_BACKLIGHT_TEST)
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

        adv7613_panel.panel_data = (void*)dev;
#ifdef CONFIG_FB_VIOC
        if(hdmi_ext_panel) {
                tccfb_register_ext_panel(&adv7613_panel);
        } else {
                tccfb_register_panel(&adv7613_panel);
                adv7613_set_power(&adv7613_panel, 1, NULL);
        }
#endif
        ret = 0;

api_end:
        return ret;
}

static int hdmi_adv7613_remove(struct i2c_client *client)
{
        struct adv7613_dev *dev = i2c_get_clientdata(client);
        if(dev != NULL) {
                devm_kfree(&client->dev, dev);
        }
        return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hdmi_adv7613of_match[] = {
        { .compatible = "telechips,adv7613", },
        {},
};
MODULE_DEVICE_TABLE(of, hdmi_adv7613of_match);
#endif

static const struct i2c_device_id hdmi_adv7613_id[] = {
        { "adv7613", 0 },
        {}
};
MODULE_DEVICE_TABLE(i2c, hdmi_adv7613_id);

static struct i2c_driver hdmi_adv7613_driver = {
        .driver = {
                .name = "hdmi-adv7613",
                .owner = THIS_MODULE,
                #ifdef CONFIG_OF
                .of_match_table = of_match_ptr(hdmi_adv7613of_match),
                #endif
        },
        .probe = hdmi_adv7613_probe,
        .remove = hdmi_adv7613_remove,
        .id_table = hdmi_adv7613_id,
};

static int __init adv7613_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&hdmi_adv7613_driver);
	if (ret != 0) {
		pr_err("ADV7613 I2C registration failed %d\n", ret);
		return ret;
	}

	return 0;
}
subsys_initcall(adv7613_i2c_init);

static void __exit adv7613_i2c_exit(void)
{
	i2c_del_driver(&hdmi_adv7613_driver);
}
module_exit(adv7613_i2c_exit);

MODULE_DESCRIPTION("ADV7613 Driver");
MODULE_LICENSE("GPL");
