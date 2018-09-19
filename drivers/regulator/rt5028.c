/* drivers/regulator/rt5028.c
 *
 * Copyright (C) 2012 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
//#include <asm/mach-types.h>
#include <linux/of_device.h>

#include <linux/regulator/rt5028.h>
//#define DEBUG
#if defined(DEBUG)
#define dbg(msg...) printk("RT5028: " msg)
#else
#define dbg(msg...)
#endif

//#define RT5028_MIN_UV   700000
//#define RT5028_MAX_UV  3600000
#if !defined(CONFIG_ARCH_TCC897X) && !defined(CONFIG_ARCH_TCC570X)
extern unsigned int irq_of_parse_and_map(struct device_node *node, int index);
#endif

/********************************************************************
	Define Types
********************************************************************/
struct rt5028_voltage_t {
	int uV;
	u8  val;
};

struct rt5028_data {
	struct i2c_client *client;
	//unsigned int min_uV;
	//unsigned int max_uV;
	struct work_struct work;
	struct input_dev *input_dev;
	struct timer_list timer;
	struct regulator_dev *rdev[0];
};


/********************************************************************
	I2C Command & Values
********************************************************************/
/* Registers */
#define RT5028_DEVICE_ID_REG				0x00
#define RT5028_BUCK1_CONTROL_REG			0x01
#define RT5028_BUCK2_CONTROL_REG			0x02
#define RT5028_BUCK3_CONTROL_REG			0x03
#define RT5028_BUCK4_CONTROL_REG			0x04
#define RT5028_VRC_SETTING_REG				0x05
#define RT5028_BUCK_MODE_REG				0x06
#define RT5028_LDO1_CONTROL_REG				0x07
#define RT5028_LDO2_CONTROL_REG				0x08
#define RT5028_LDO3_CONTROL_REG				0x09
#define RT5028_LDO4_CONTROL_REG				0x0A
#define RT5028_LDO5_CONTROL_REG				0x0B
#define RT5028_LDO6_CONTROL_REG				0x0C
#define RT5028_LDO7_CONTROL_REG				0x0D
#define RT5028_LDO8_CONTROL_REG				0x0E
#define RT5028_BUCK_ON_OFF_REG				0x12
#define RT5028_LDO_ON_OFF_REG				0x13
#define RT5028_IRQ1_ENABLE_REG				0x28
#define RT5028_IRQ1_STATUS_REG				0x29
#define RT5028_IRQ2_ENABLE_REG				0x2A
#define RT5028_IRQ2_STATUS_REG				0x2B


#define RT5028_IR_OT						(1 <<7)
#define RT5028_IR_BUCK1LV					(1 <<6)
#define RT5028_IR_BUCK2LV					(1 <<5)
#define RT5028_IR_BUCK3LV					(1 <<4)
#define RT5028_IR_BUCK4LV					(1 <<3)
#define RT5028_IR_PWRONSP					(1 <<2)
#define RT5028_IR_PWRONLP					(1 <<1)
#define RT5028_IR_VINLV						(1 <<0)

#define RT5028_IR_KPSHDN					(1 <<7)
#define RT5028_IR_PWRONSR					(1 <<6)
#define RT5028_IR_PWRONLF					(1 <<5)
#define RT5028_IR_OTW125					(1 <<1)
#define RT5028_IR_OTW100					(1 <<0)

#define RT5028_IRQ1 ( 0 \
		| RT5028_IR_PWRONSP \
		| RT5028_IR_PWRONLP \
		)

#define RT5028_IRQ2 ( 0 \
		)

/* BUCK voltage level */
static struct rt5028_voltage_t buck12_voltages[] = {
    {  700000, 0x00 }, {  725000, 0x01 }, {  750000, 0x02 }, {  775000, 0x03 },
    {  800000, 0x04 }, {  825000, 0x05 }, {  850000, 0x06 }, {  875000, 0x07 },
    {  900000, 0x08 }, {  925000, 0x09 }, {  950000, 0x0A }, {  975000, 0x0B },
    { 1000000, 0x0C }, { 1025000, 0x0D }, { 1050000, 0x0E }, { 1075000, 0x0F }, // 16
    { 1100000, 0x10 }, { 1125000, 0x11 }, { 1150000, 0x12 }, { 1175000, 0x13 },
    { 1200000, 0x14 }, { 1225000, 0x15 }, { 1250000, 0x16 }, { 1275000, 0x17 },
    { 1300000, 0x18 }, { 1325000, 0x19 }, { 1350000, 0x1A }, { 1375000, 0x1B },
    { 1400000, 0x1C }, { 1425000, 0x1D }, { 1450000, 0x1E }, { 1475000, 0x1F }, // 32
    { 1500000, 0x20 }, { 1525000, 0x21 }, { 1550000, 0x22 }, { 1575000, 0x23 },
    { 1600000, 0x24 }, { 1625000, 0x25 }, { 1650000, 0x26 }, { 1675000, 0x27 },
    { 1700000, 0x28 }, { 1725000, 0x29 }, { 1750000, 0x2A }, { 1775000, 0x2B },
    { 1800000, 0x2C }, { 1800000, 0x2D }, { 1800000, 0x2E }, { 1800000, 0x2F }, // 48
    { 1800000, 0x30 }, { 1800000, 0x31 }, { 1800000, 0x32 }, { 1800000, 0x33 },
    { 1800000, 0x34 }, { 1800000, 0x35 }, { 1800000, 0x36 }, { 1800000, 0x37 },
    { 1800000, 0x38 }, { 1800000, 0x39 }, { 1800000, 0x3A }, { 1800000, 0x3B },
    { 1800000, 0x3C }, { 1800000, 0x3D }, { 1800000, 0x3E }, { 1800000, 0x3F }, // 64
}; 
#define NUM_BUCK12    ARRAY_SIZE(buck12_voltages) /* 700mV~1800mV */

static struct rt5028_voltage_t buck34_voltages[] = {
    {  700000, 0x00 }, {  750000, 0x01 }, {  800000, 0x02 }, {  850000, 0x03 },
    {  900000, 0x04 }, {  950000, 0x05 }, { 1000000, 0x06 }, { 1050000, 0x07 },
    { 1100000, 0x08 }, { 1150000, 0x09 }, { 1200000, 0x0A }, { 1250000, 0x0B },
    { 1300000, 0x0C }, { 1350000, 0x0D }, { 1400000, 0x0E }, { 1450000, 0x0F }, // 16
    { 1500000, 0x10 }, { 1550000, 0x11 }, { 1600000, 0x12 }, { 1650000, 0x13 },
    { 1700000, 0x14 }, { 1750000, 0x15 }, { 1800000, 0x16 }, { 1850000, 0x17 },
    { 1900000, 0x18 }, { 1950000, 0x19 }, { 2000000, 0x1A }, { 2050000, 0x1B },
    { 2100000, 0x1C }, { 2150000, 0x1D }, { 2200000, 0x1E }, { 2250000, 0x1F }, // 32
    { 2300000, 0x20 }, { 2350000, 0x21 }, { 2400000, 0x22 }, { 2450000, 0x23 },
    { 2500000, 0x24 }, { 2550000, 0x25 }, { 2600000, 0x26 }, { 2650000, 0x27 },
    { 2700000, 0x28 }, { 2750000, 0x29 }, { 2800000, 0x2A }, { 2850000, 0x2B },
    { 2900000, 0x2C }, { 2950000, 0x2D }, { 3000000, 0x2E }, { 3050000, 0x2F }, // 48
    { 3100000, 0x30 }, { 3150000, 0x31 }, { 3200000, 0x32 }, { 3250000, 0x33 },
    { 3300000, 0x34 }, { 3350000, 0x35 }, { 3400000, 0x36 }, { 3450000, 0x37 }, 
    { 3500000, 0x38 }, { 3550000, 0x39 }, { 3600000, 0x3A }, { 3600000, 0x3B }, 
    { 3600000, 0x3C }, { 3600000, 0x3D }, { 3600000, 0x3E }, { 3600000, 0x3F }, // 64
}; 
#define NUM_BUCK34    ARRAY_SIZE(buck34_voltages)	 /* 700mV~3600mV */


static struct rt5028_voltage_t ldo12378_voltages[] = {
    { 1600000, 0x00 }, { 1625000, 0x01 }, { 1650000, 0x02 }, { 1675000, 0x03 },
    { 1700000, 0x04 }, { 1725000, 0x05 }, { 1750000, 0x06 }, { 1775000, 0x07 },
    { 1800000, 0x08 }, { 1825000, 0x09 }, { 1850000, 0x0A }, { 1875000, 0x0B },
    { 1900000, 0x0C }, { 1925000, 0x0D }, { 1950000, 0x0E }, { 1975000, 0x0F }, // 16
    { 2000000, 0x10 }, { 2025000, 0x11 }, { 2050000, 0x12 }, { 2075000, 0x13 },
    { 2100000, 0x14 }, { 2125000, 0x15 }, { 2150000, 0x16 }, { 2175000, 0x17 },
    { 2200000, 0x18 }, { 2225000, 0x19 }, { 2250000, 0x1A }, { 2275000, 0x1B },
    { 2300000, 0x1C }, { 2325000, 0x1D }, { 2350000, 0x1E }, { 2375000, 0x1F }, // 32
    { 2400000, 0x20 }, { 2425000, 0x21 }, { 2450000, 0x22 }, { 2475000, 0x23 },
    { 2500000, 0x24 }, { 2525000, 0x25 }, { 2550000, 0x26 }, { 2575000, 0x27 },
    { 2600000, 0x28 }, { 2625000, 0x29 }, { 2650000, 0x2A }, { 2675000, 0x2B },
    { 2700000, 0x2C }, { 2725000, 0x2D }, { 2750000, 0x2E }, { 2775000, 0x2F }, // 48
    { 2800000, 0x30 }, { 2825000, 0x31 }, { 2850000, 0x32 }, { 2875000, 0x33 },
    { 2900000, 0x34 }, { 2925000, 0x35 }, { 2950000, 0x36 }, { 2975000, 0x37 },
    { 3000000, 0x38 }, { 3025000, 0x39 }, { 3050000, 0x3A }, { 3075000, 0x3B },
    { 3100000, 0x3C }, { 3125000, 0x3D }, { 3150000, 0x3E }, { 3175000, 0x3F }, // 64
    { 3200000, 0x40 }, { 3225000, 0x41 }, { 3250000, 0x42 }, { 3275000, 0x43 },
    { 3300000, 0x44 }, { 3325000, 0x45 }, { 3350000, 0x46 }, { 3375000, 0x47 },
    { 3400000, 0x48 }, { 3425000, 0x49 }, { 3450000, 0x4A }, { 3475000, 0x4B },
    { 3500000, 0x4C }, { 3525000, 0x4D }, { 3550000, 0x4E }, { 3575000, 0x4F }, // 80
    { 3600000, 0x50 }, { 3600000, 0x51 }, { 3600000, 0x52 }, { 3600000, 0x53 },
    { 3600000, 0x54 }, { 3600000, 0x55 }, { 3600000, 0x56 }, { 3600000, 0x57 },
    { 3600000, 0x58 }, { 3600000, 0x59 }, { 3600000, 0x5A }, { 3600000, 0x5B },
    { 3600000, 0x5C }, { 3600000, 0x5D }, { 3600000, 0x5E }, { 3600000, 0x5F }, // 96
    { 3600000, 0x60 }, { 3600000, 0x61 }, { 3600000, 0x62 }, { 3600000, 0x63 },
    { 3600000, 0x64 }, { 3600000, 0x65 }, { 3600000, 0x66 }, { 3600000, 0x67 },
    { 3600000, 0x68 }, { 3600000, 0x69 }, { 3600000, 0x6A }, { 3600000, 0x6B },
    { 3600000, 0x6C }, { 3600000, 0x6D }, { 3600000, 0x6E }, { 3600000, 0x6F }, // 112
    { 3600000, 0x70 }, { 3600000, 0x71 }, { 3600000, 0x72 }, { 3600000, 0x73 },
    { 3600000, 0x74 }, { 3600000, 0x75 }, { 3600000, 0x76 }, { 3600000, 0x77 },
    { 3600000, 0x78 }, { 3600000, 0x79 }, { 3600000, 0x7A }, { 3600000, 0x7B },
    { 3600000, 0x7C }, { 3600000, 0x7D }, { 3600000, 0x7E }, { 3600000, 0x7F },
};

#define NUM_LDO12378     ARRAY_SIZE(ldo12378_voltages)


static struct rt5028_voltage_t ldo456_voltages[] = {
    { 3000000, 0x00 }, { 3025000, 0x01 }, { 3050000, 0x02 }, { 3075000, 0x03 },
    { 3100000, 0x04 }, { 3125000, 0x05 }, { 3150000, 0x06 }, { 3175000, 0x07 },
    { 3200000, 0x08 }, { 3225000, 0x09 }, { 3250000, 0x0A }, { 3275000, 0x0B },
    { 3300000, 0x0C }, { 3325000, 0x0D }, { 3350000, 0x0E }, { 3375000, 0x0F }, // 16
    { 3400000, 0x10 }, { 3425000, 0x11 }, { 3450000, 0x12 }, { 3475000, 0x13 },
    { 3500000, 0x14 }, { 3525000, 0x15 }, { 3550000, 0x16 }, { 3575000, 0x17 },
    { 3600000, 0x18 }, { 3600000, 0x19 }, { 3600000, 0x1A }, { 3600000, 0x1B },
    { 3600000, 0x1C }, { 3600000, 0x1D }, { 3600000, 0x1E }, { 3600000, 0x1F }, // 32
    { 3600000, 0x20 }, { 3600000, 0x21 }, { 3600000, 0x22 }, { 3600000, 0x23 },
    { 3600000, 0x24 }, { 3600000, 0x25 }, { 3600000, 0x26 }, { 3600000, 0x27 },
    { 3600000, 0x28 }, { 3600000, 0x29 }, { 3600000, 0x2A }, { 3600000, 0x2B },
    { 3600000, 0x2C }, { 3600000, 0x2D }, { 3600000, 0x2E }, { 3600000, 0x2F }, // 48
    { 3600000, 0x30 }, { 3600000, 0x31 }, { 3600000, 0x32 }, { 3600000, 0x33 },
    { 3600000, 0x34 }, { 3600000, 0x35 }, { 3600000, 0x36 }, { 3600000, 0x37 },
    { 3600000, 0x38 }, { 3600000, 0x39 }, { 3600000, 0x3A }, { 3600000, 0x3B },
    { 3600000, 0x3C }, { 3600000, 0x3D }, { 3600000, 0x3E }, { 3600000, 0x3F }, // 64
    { 3600000, 0x40 }, { 3600000, 0x41 }, { 3600000, 0x42 }, { 3600000, 0x43 },
    { 3600000, 0x44 }, { 3600000, 0x45 }, { 3600000, 0x46 }, { 3600000, 0x47 },
    { 3600000, 0x48 }, { 3600000, 0x49 }, { 3600000, 0x4A }, { 3600000, 0x4B },
    { 3600000, 0x4C }, { 3600000, 0x4D }, { 3600000, 0x4E }, { 3600000, 0x4F }, // 80
    { 3600000, 0x50 }, { 3600000, 0x51 }, { 3600000, 0x52 }, { 3600000, 0x53 },
    { 3600000, 0x54 }, { 3600000, 0x55 }, { 3600000, 0x56 }, { 3600000, 0x57 },
    { 3600000, 0x58 }, { 3600000, 0x59 }, { 3600000, 0x5A }, { 3600000, 0x5B },
    { 3600000, 0x5C }, { 3600000, 0x5D }, { 3600000, 0x5E }, { 3600000, 0x5F }, // 96
    { 3600000, 0x60 }, { 3600000, 0x61 }, { 3600000, 0x62 }, { 3600000, 0x63 },
    { 3600000, 0x64 }, { 3600000, 0x65 }, { 3600000, 0x66 }, { 3600000, 0x67 },
    { 3600000, 0x68 }, { 3600000, 0x69 }, { 3600000, 0x6A }, { 3600000, 0x6B },
    { 3600000, 0x6C }, { 3600000, 0x6D }, { 3600000, 0x6E }, { 3600000, 0x6F }, // 112
    { 3600000, 0x70 }, { 3600000, 0x71 }, { 3600000, 0x72 }, { 3600000, 0x73 },
    { 3600000, 0x74 }, { 3600000, 0x75 }, { 3600000, 0x76 }, { 3600000, 0x77 },
    { 3600000, 0x78 }, { 3600000, 0x79 }, { 3600000, 0x7A }, { 3600000, 0x7B },
    { 3600000, 0x7C }, { 3600000, 0x7D }, { 3600000, 0x7E }, { 3600000, 0x7F },
};

#define NUM_LDO456     ARRAY_SIZE(ldo456_voltages)


/********************************************************************
	Variables
********************************************************************/
static struct workqueue_struct *rt5028_wq;
static struct i2c_client *rt5028_i2c_client = NULL;
static unsigned int rt5028_suspend_status = 0;


/********************************************************************
	Functions (Global)
********************************************************************/

/*******************************************
	Functions (Internal)
*******************************************/
static void rt5028_work_func(struct work_struct *work)
{
	struct rt5028_data* rt5028 = container_of(work, struct rt5028_data, work);
	unsigned char data[2];
	dbg("%s\n", __func__);

	data[0] = (unsigned char)i2c_smbus_read_byte_data(rt5028->client, RT5028_IRQ1_STATUS_REG);
	data[1] = (unsigned char)i2c_smbus_read_byte_data(rt5028->client, RT5028_IRQ2_STATUS_REG);
	// Reset after read. 

	if (data[0]&RT5028_IR_OT) {
		dbg("Internal over-temperature was triggered\r\n");
	}
	if (data[0]&RT5028_IR_BUCK1LV) {
		dbg("Buck1 output voltage equal 66%% x VTarget\r\n");
	}
	if (data[0]&RT5028_IR_BUCK2LV) {
		dbg("Buck2 output voltage equal 66%% x VTarget\r\n");
	}
	if (data[0]&RT5028_IR_BUCK3LV) {
		dbg("Buck3 output voltage equal 66%% x VTarget\r\n");
	}
	if (data[0]&RT5028_IR_BUCK4LV) {
		dbg("Buck4 output voltage equal 66%% x VTarget\r\n");
	}
	if (data[0]&RT5028_IR_PWRONSP) {
		del_timer(&rt5028->timer);
		input_report_key(rt5028->input_dev, KEY_POWER, 1);
		input_event(rt5028->input_dev, EV_SYN, 0, 0);
		rt5028->timer.expires = jiffies + msecs_to_jiffies(100);
		add_timer(&rt5028->timer);
		dbg("PWRON short press\r\n");
	}
	if (data[0]&RT5028_IR_PWRONLP) {
		del_timer(&rt5028->timer);
		input_report_key(rt5028->input_dev, KEY_POWER, 1);
		input_event(rt5028->input_dev, EV_SYN, 0, 0);
		rt5028->timer.expires = jiffies + msecs_to_jiffies(1000);
		add_timer(&rt5028->timer);
		dbg("PWRON long press\r\n");
	}
	if (data[0]&RT5028_IR_VINLV) {
		dbg("VIN voltage is lower than VOFF\r\n");
	}
	if (data[0]&RT5028_IR_KPSHDN) {
		dbg("Key-press forced shutdown\r\n");
	}
	
	enable_irq(rt5028->client->irq);
}

static irqreturn_t rt5028_interrupt(int irqno, void *param)
{
	struct rt5028_data *rt5028 = (struct rt5028_data *)param;
	dbg("%s\n", __func__);

	disable_irq_nosync(rt5028->client->irq);
	queue_work(rt5028_wq, &rt5028->work);
	return IRQ_HANDLED;
}

static void rt5028_timer_func(unsigned long data)
{
	struct rt5028_data *rt5028 = (struct rt5028_data *)data;
	dbg("%s\n", __func__);

	input_report_key(rt5028->input_dev, KEY_POWER, 0);
	//input_event(axp192->input_dev, EV_KEY, KEY_POWER, 0);
	input_event(rt5028->input_dev, EV_SYN, 0, 0);
}
static int rt5028_buck_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV, unsigned *selector)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 reg, old_value, value = 0;
	int i, max_num, ret;

	static struct rt5028_voltage_t *buck_voltags;
	
	if(rt5028_suspend_status)
		return -EBUSY;

	switch (id) {
		case RT5028_ID_BUCK1:
			reg = RT5028_BUCK1_CONTROL_REG;
			buck_voltags = buck12_voltages;
			max_num = NUM_BUCK12;
			break;
		case RT5028_ID_BUCK2:
			reg = RT5028_BUCK2_CONTROL_REG;
			buck_voltags = buck12_voltages;
			max_num = NUM_BUCK12;
			break;
		case RT5028_ID_BUCK3:
			reg = RT5028_BUCK3_CONTROL_REG;
			buck_voltags = buck34_voltages;
			max_num = NUM_BUCK34;
			break;
		case RT5028_ID_BUCK4:
			reg = RT5028_BUCK4_CONTROL_REG;
			buck_voltags = buck34_voltages;
			max_num = NUM_BUCK34;
			break;
		default:
			return -EINVAL;
	}

	for (i = 0; i < max_num; i++) {
		if (buck_voltags[i].uV >= min_uV) {
			value = buck_voltags[i].val;
			break;
		}
	}

	if (i == max_num)
		return -EINVAL;

	old_value = i2c_smbus_read_byte_data(rt5028->client, reg);
	value = (old_value & 0x3) | (value << 2);

	dbg("%s: reg:0x%x value:%dmV\n", __func__, reg, buck_voltags[i].uV/1000);
	ret = i2c_smbus_write_byte_data(rt5028->client, reg, value);
	udelay(50);

	if(ret != 0)
		printk("%s(reg:%x, val:%x) - fail(%d)\n", __func__, reg, value, ret);

	return ret;
}

static int rt5028_buck_get_voltage(struct regulator_dev *rdev)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 reg;
	int ret, max_num, i, voltage = 0;
	static struct rt5028_voltage_t *buck_voltags;

	switch (id) {
		case RT5028_ID_BUCK1:
			reg = RT5028_BUCK1_CONTROL_REG;
			buck_voltags = buck12_voltages;
			max_num = NUM_BUCK12;
			break;
		case RT5028_ID_BUCK2:
			reg = RT5028_BUCK2_CONTROL_REG;
			buck_voltags = buck12_voltages;
			max_num = NUM_BUCK12;
			break;
		case RT5028_ID_BUCK3:
			reg = RT5028_BUCK3_CONTROL_REG;
			buck_voltags = buck34_voltages;
			max_num = NUM_BUCK34;
			break;
		case RT5028_ID_BUCK4:
			reg = RT5028_BUCK4_CONTROL_REG;
			buck_voltags = buck34_voltages;
			max_num = NUM_BUCK34;
			break;
		default:
			return -EINVAL;
	}

	ret = i2c_smbus_read_byte_data(rt5028->client, reg);
	if (ret < 0)
		return -EINVAL;

	ret = ((ret & 0xFF) >> 2);
	for (i = 0; i < max_num; i++) {
		if (buck_voltags[i].val == ret) {
			voltage = buck_voltags[i].uV;
			break;
		}
	}
	if (i == max_num)
		return -EINVAL;

	dbg("%s: reg:0x%x value:%dmV\n", __func__, reg, voltage/1000);
	return voltage;
}

static int rt5028_buck_enable(struct regulator_dev *rdev)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 value, old_value, bit;

	dbg("%s: id:%d\n", __func__, id);
	switch (id) {
		case RT5028_ID_BUCK1:
		case RT5028_ID_BUCK2:
		case RT5028_ID_BUCK3:
		case RT5028_ID_BUCK4:
			bit = id-RT5028_ID_BUCK1;
			break;
		default:
			return -EINVAL;
	}

	old_value = (u8)i2c_smbus_read_byte_data(rt5028->client, RT5028_BUCK_ON_OFF_REG);
	value = old_value | (1 << bit);

	return i2c_smbus_write_byte_data(rt5028->client, RT5028_BUCK_ON_OFF_REG, value);
}

static int rt5028_buck_disable(struct regulator_dev *rdev)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 value, old_value, bit;

	dbg("%s: id:%d\n", __func__, id);
	switch (id) {
		case RT5028_ID_BUCK1:
		case RT5028_ID_BUCK2:
		case RT5028_ID_BUCK3:
		case RT5028_ID_BUCK4:
			bit = id-RT5028_ID_BUCK1;
			break;
		default:
			return -EINVAL;
	}

	old_value = (u8)i2c_smbus_read_byte_data(rt5028->client, RT5028_BUCK_ON_OFF_REG);
	value = old_value & ~(1 << bit);

	return i2c_smbus_write_byte_data(rt5028->client, RT5028_BUCK_ON_OFF_REG, value);
}

static struct regulator_ops rt5028_buck_ops = {
	.set_voltage = rt5028_buck_set_voltage,
	.get_voltage = rt5028_buck_get_voltage,
	.enable      = rt5028_buck_enable,
	.disable     = rt5028_buck_disable,
};

static int rt5028_ldo_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV, unsigned *selector)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 reg, value;
	int i, max_num;

	static struct rt5028_voltage_t *ldo_voltags;

		
	switch (id) {
		case RT5028_ID_LDO2:
			reg = RT5028_LDO2_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		case RT5028_ID_LDO3:
			reg = RT5028_LDO3_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		case RT5028_ID_LDO4:
			reg = RT5028_LDO4_CONTROL_REG;
			ldo_voltags = ldo456_voltages;
			max_num = NUM_LDO456;
			break;
		case RT5028_ID_LDO5:
			reg = RT5028_LDO5_CONTROL_REG;
			ldo_voltags = ldo456_voltages;
			max_num = NUM_LDO456;
			break;
		case RT5028_ID_LDO6:
			reg = RT5028_LDO6_CONTROL_REG;
			ldo_voltags = ldo456_voltages;
			max_num = NUM_LDO456;
			break;
		case RT5028_ID_LDO7:
			reg = RT5028_LDO7_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		case RT5028_ID_LDO8:
			reg = RT5028_LDO8_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		default:
			return -EINVAL;
	}

	for (i = 0; i < max_num; i++) {
		if (ldo_voltags[i].uV >= min_uV) {
			value = ldo_voltags[i].val;
			break;
		}
	}

	if (i == max_num)
		return -EINVAL;

	dbg("%s: reg:0x%x value: %dmV\n", __func__, reg, ldo_voltags[i].uV/1000);
	return i2c_smbus_write_byte_data(rt5028->client, reg, value);
}

static int rt5028_ldo_get_voltage(struct regulator_dev *rdev)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 reg;
	int ret, i, max_num, voltage = 0;

	static struct rt5028_voltage_t *ldo_voltags;
	
	switch (id) {
		case RT5028_ID_LDO1:
			reg = RT5028_LDO1_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		case RT5028_ID_LDO2:
			reg = RT5028_LDO2_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		case RT5028_ID_LDO3:
			reg = RT5028_LDO3_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		case RT5028_ID_LDO4:
			reg = RT5028_LDO4_CONTROL_REG;
			ldo_voltags = ldo456_voltages;
			max_num = NUM_LDO456;
			break;
		case RT5028_ID_LDO5:
			reg = RT5028_LDO5_CONTROL_REG;
			ldo_voltags = ldo456_voltages;
			max_num = NUM_LDO456;
			break;
		case RT5028_ID_LDO6:
			reg = RT5028_LDO6_CONTROL_REG;
			ldo_voltags = ldo456_voltages;
			max_num = NUM_LDO456;
			break;
		case RT5028_ID_LDO7:
			reg = RT5028_LDO7_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		case RT5028_ID_LDO8:
			reg = RT5028_LDO8_CONTROL_REG;
			ldo_voltags = ldo12378_voltages;
			max_num = NUM_LDO12378;
			break;
		default:
			return -EINVAL;
	}

	ret = i2c_smbus_read_byte_data(rt5028->client, reg);
	if (ret < 0)
		return -EINVAL;

	ret &= 0xFF;

	for (i = 0; i < max_num; i++) {
		if (ldo_voltags[i].val == ret) {
			voltage = ldo_voltags[i].uV;
			break;
		}
	}

	if (i == max_num)
		return -EINVAL;

	dbg("%s: reg:0x%x value: %dmV\n", __func__, reg, voltage/1000);
	return voltage;
}

static int rt5028_ldo_enable(struct regulator_dev *rdev)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 value, old_value, bit;

	dbg("%s: id:%d\n", __func__, id);
	switch (id) {
		case RT5028_ID_LDO2:
		case RT5028_ID_LDO3:
		case RT5028_ID_LDO4:
		case RT5028_ID_LDO5:
		case RT5028_ID_LDO6:
		case RT5028_ID_LDO7:
		case RT5028_ID_LDO8:
			bit = id-RT5028_ID_LDO1;
			break;
		default:
			return -EINVAL;
	}

	old_value = (u8)i2c_smbus_read_byte_data(rt5028->client, RT5028_LDO_ON_OFF_REG);
	value = old_value | (1 << bit);

	return i2c_smbus_write_byte_data(rt5028->client, RT5028_LDO_ON_OFF_REG, value);
}

static int rt5028_ldo_disable(struct regulator_dev *rdev)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 value, old_value, bit;

	dbg("%s: id:%d\n", __func__, id);
	switch (id) {
		case RT5028_ID_LDO2:
		case RT5028_ID_LDO3:
		case RT5028_ID_LDO4:
		case RT5028_ID_LDO5:
		case RT5028_ID_LDO6:
		case RT5028_ID_LDO7:
		case RT5028_ID_LDO8:
			bit = id-RT5028_ID_LDO1;
			break;
		default:
			return -EINVAL;
	}

	old_value = (u8)i2c_smbus_read_byte_data(rt5028->client, RT5028_LDO_ON_OFF_REG);
	value = old_value & ~(1 << bit);

	return i2c_smbus_write_byte_data(rt5028->client, RT5028_LDO_ON_OFF_REG, value);
}

static int rt5028_ldo_is_enabled(struct regulator_dev *rdev)
{
	struct rt5028_data* rt5028 = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	u8 old_value, bit;

	dbg("%s: id:%d\n", __func__, id);
	switch (id) {
		case RT5028_ID_LDO2:
		case RT5028_ID_LDO3:
		case RT5028_ID_LDO4:
		case RT5028_ID_LDO5:
		case RT5028_ID_LDO6:
		case RT5028_ID_LDO7:
		case RT5028_ID_LDO8:
			bit = id-RT5028_ID_LDO1;
			break;
		default:
			return -EINVAL;
	}

	old_value = (u8)i2c_smbus_read_byte_data(rt5028->client, RT5028_LDO_ON_OFF_REG);

	return (old_value & (1 << bit))?1:0;
}

static struct regulator_ops rt5028_ldo_ops = {
	.set_voltage = rt5028_ldo_set_voltage,
	.get_voltage = rt5028_ldo_get_voltage,
	.enable      = rt5028_ldo_enable,
	.disable     = rt5028_ldo_disable,
	.is_enabled  = rt5028_ldo_is_enabled,
};

static struct regulator_desc rt5028_reg[] = {
	{
		.name = "buck1",
		.id = RT5028_ID_BUCK1,
		.ops = &rt5028_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_BUCK12 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "buck2",
		.id = RT5028_ID_BUCK2,
		.ops = &rt5028_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_BUCK12 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "buck3",
		.id = RT5028_ID_BUCK3,
		.ops = &rt5028_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_BUCK34 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "buck4",
		.id = RT5028_ID_BUCK4,
		.ops = &rt5028_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_BUCK34 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo1",
		.id = RT5028_ID_LDO1,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO12378 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo2",
		.id = RT5028_ID_LDO2,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO12378 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo3",
		.id = RT5028_ID_LDO3,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO12378 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo4",
		.id = RT5028_ID_LDO4,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO456 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo5",
		.id = RT5028_ID_LDO5,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO456 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo6",
		.id = RT5028_ID_LDO6,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO456 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo7",
		.id = RT5028_ID_LDO7,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO12378 + 1,
		.owner = THIS_MODULE,
	},
	{
		.name = "ldo8",
		.id = RT5028_ID_LDO8,
		.ops = &rt5028_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = NUM_LDO12378 + 1,
		.owner = THIS_MODULE,
	},
};
#define NUM_OUPUT	ARRAY_SIZE(rt5028_reg)

#ifdef CONFIG_OF
static const struct of_device_id rt5028_of_match[] = {
	{.compatible = "richtek,rt5028", },
	{ },
};
MODULE_DEVICE_TABLE(of, rt5028_of_match);
#endif

static int rt5028_pmic_probe(struct i2c_client *client, const struct i2c_device_id *i2c_id)
{
	struct regulator_dev **rdev;
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct rt5028_data *rt5028;
	const struct of_device_id *match;
	int i=0, id, irq_port, ret = -ENOMEM;
	struct regulator_config config = { };
	char i2c_data=0; 
	
	pmic_np = client->dev.of_node;
	
	match = of_match_device(rt5028_of_match, &client->dev);
	if(!match)
	{
		dev_err(&client->dev, "Regulators(rt5028) device not found\n");
		return -ENODEV;
	}

	irq_port = of_get_named_gpio(pmic_np, "int-gpios", 0);
	if (gpio_is_valid(irq_port))
	{
		client->irq = gpio_to_irq(irq_port);
		//client->irq = irq_of_parse_and_map(pmic_np, 0); //it should be added "interrupts" property in device tree
		dev_dbg(&client->dev, "irq %d\n", client->irq);
	}
	
	regulators_np = of_find_node_by_name(pmic_np, "regulators");

	if (!regulators_np) {
		dev_err(&client->dev, "Regulators(rt5028) device node not found\n");
		return -ENODEV;
	}
	
	rt5028 = kzalloc(sizeof(struct rt5028_data) +
			sizeof(struct regulator_dev *) * (NUM_OUPUT + 1),
			GFP_KERNEL);
	
	if (!rt5028)
		goto out;

	rt5028->client = client;
	rt5028_i2c_client = client;

	//rt5028->min_uV = RT5028_MIN_UV;
	//rt5028->max_uV = RT5028_MAX_UV;

	rdev = rt5028->rdev;

	i2c_set_clientdata(client, rdev);
	i2c_data = i2c_smbus_read_byte_data(rt5028_i2c_client, RT5028_DEVICE_ID_REG);
	dev_dbg(&client->dev, "pmix id 0x%x\n", i2c_data);
	
	// gpio & external interrupt ports configuration
//	if (pdata->init_port) {
//		pdata->init_port(client->irq);
//	}

	INIT_WORK(&rt5028->work, rt5028_work_func);
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(rt5028_reg); i++)
			if (!of_node_cmp(reg_np->name, rt5028_reg[i].name))
				break;

		if (i == ARRAY_SIZE(rt5028_reg)) {
			dev_warn(&client->dev, "don't know how to configure regulator %s\n",
				 reg_np->name);
			continue;
		}

		id = rt5028_reg[i].id;
		config.init_data = of_get_regulator_init_data(&client->dev, reg_np, rt5028_reg);
		if (config.init_data == NULL) {
			rdev[i] = NULL;
			continue;
		}

		config.dev = &client->dev;
		config.driver_data = rt5028;
		rdev[i] = regulator_register(&rt5028_reg[id], &config);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(&client->dev, "failed to register %s\n",
				rt5028_reg[id].name);
			goto err;
		}
	}
	if (client->irq) {
		int status=0;
		/* irq enable */
		// TODO:
		status = i2c_smbus_write_byte_data(rt5028_i2c_client, RT5028_IRQ1_ENABLE_REG, RT5028_IRQ1);
		if (status < 0)
			dev_err(&client->dev, "failed to write IRQ1 enable (res=%d) !\n", status);

		status = i2c_smbus_write_byte_data(rt5028_i2c_client, RT5028_IRQ2_ENABLE_REG, RT5028_IRQ2);
		if (status < 0)
			dev_err(&client->dev, "failed to write IRQ2 enable (res=%d) !\n", status);

		/* clear irq status */
		// TODO:
		status = i2c_smbus_read_byte_data(rt5028_i2c_client, RT5028_IRQ1_STATUS_REG);
		if (status < 0)
			dev_err(&client->dev, "failed to read IRQ1 status (res=%d) !\n", status);

		status= i2c_smbus_read_byte_data(rt5028_i2c_client, RT5028_IRQ2_STATUS_REG);
		if (status < 0)
			dev_err(&client->dev, "failed to read IRQ2 status(res=%d) !\n", status);
		// Reset after read. 

		status = i2c_smbus_write_byte_data(rt5028_i2c_client, RT5028_IRQ1_STATUS_REG, 0xFF);
		if (status < 0)
			dev_err(&client->dev, "failed to write IRQ1 status (res=%d) !\n", status);

		status = i2c_smbus_write_byte_data(rt5028_i2c_client, RT5028_IRQ2_STATUS_REG, 0xFF);
		if (status < 0)
			dev_err(&client->dev, "failed to write IRQ2 status (res=%d) !\n", status);

		if (request_irq(client->irq, rt5028_interrupt, IRQ_TYPE_EDGE_FALLING, "rt5028_irq", rt5028)) {
			dev_err(&client->dev, "could not allocate IRQ_NO(%d) !\n", client->irq);
			ret = -EBUSY;
			goto err;
		}
	}

	// register input device for power key.
	rt5028->input_dev = input_allocate_device();
	if (rt5028->input_dev == NULL) {
		ret = -ENOMEM;
		dev_err(&client->dev, "%s: Failed to allocate input device\n", __func__);
		goto err_input_dev_alloc_failed;
	}
/*
	rt5028->input_dev->evbit[0] = BIT(EV_KEY);
	rt5028->input_dev->name = "rt5028 power-key";
	set_bit(KEY_POWER & KEY_MAX, rt5028->input_dev->keybit);
	ret = input_register_device(rt5028->input_dev);
	if (ret) {
		dev_err(&client->dev, "%s: Unable to register %s input device\n", __func__, rt5028->input_dev->name);
		goto err_input_register_device_failed;
	}
*/
	init_timer(&rt5028->timer);
	rt5028->timer.data = (unsigned long)rt5028;
	rt5028->timer.function = rt5028_timer_func;
	dev_info(&client->dev, "RT5028 regulator driver loaded\n");
	printk("RT5028 regulator driver loaded\n");
	return 0;

err_input_register_device_failed:
	input_free_device(rt5028->input_dev);
err_input_dev_alloc_failed:
err:
	while (--i >= 0)
		regulator_unregister(rdev[i]);
	kfree(rt5028);
out:
	rt5028_i2c_client = NULL;
	return ret;
}

static int rt5028_pmic_remove(struct i2c_client *client)
{
	struct regulator_dev **rdev = i2c_get_clientdata(client);
	struct rt5028_data* rt5028 = NULL;
	int i;
	
	for (i=0 ; (rdev[i] != NULL) && (i<RT5028_ID_MAX) ; i++)
		rt5028 = rdev_get_drvdata(rdev[i]);

	if (rt5028)
		del_timer(&rt5028->timer);
	for (i = 0; i <= NUM_OUPUT; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);
	kfree(rdev);
	i2c_set_clientdata(client, NULL);
	rt5028_i2c_client = NULL;

	return 0;
}
	
static int rt5028_pmic_suspend(struct device *dev)
{
	int i, ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct regulator_dev **rdev = i2c_get_clientdata(client);
	struct rt5028_data* rt5028 = NULL;
	
	for (i=0 ; (rdev[i] != NULL) && (i<RT5028_ID_MAX) ; i++)
		rt5028 = rdev_get_drvdata(rdev[i]);

	if (client->irq)
		disable_irq(client->irq);

	/* clear irq status */
	// TODO:
	i2c_smbus_write_byte_data(client, RT5028_IRQ1_STATUS_REG, 0xFF);
	i2c_smbus_write_byte_data(client, RT5028_IRQ2_STATUS_REG, 0xFF);

	if (rt5028) {
		ret = cancel_work_sync(&rt5028->work);
		flush_workqueue(rt5028_wq);
	}

	if (ret && client->irq)
		enable_irq(client->irq);

	rt5028_suspend_status = 1;

	return 0;
}

static int rt5028_pmic_resume(struct device *dev)
{
	int i;
	struct i2c_client *client = to_i2c_client(dev);
	struct regulator_dev **rdev = i2c_get_clientdata(client);
	struct rt5028_data* rt5028 = NULL;

	
	for (i=0 ; (rdev[i] != NULL) && (i<RT5028_ID_MAX) ; i++)
		rt5028 = rdev_get_drvdata(rdev[i]);

#if 1
	if (rt5028)
		queue_work(rt5028_wq, &rt5028->work);
#else
	if (client->irq)
		enable_irq(client->irq);
	/* clear irq status */
	// TODO:
	i2c_smbus_read_byte_data(rt5028->client, RT5028_IRQ1_STATUS_REG);
	i2c_smbus_read_byte_data(rt5028->client, RT5028_IRQ2_STATUS_REG);
	// Reset after read. 
	i2c_smbus_write_byte_data(client, RT5028_IRQ1_STATUS_REG, 0xFF);
	i2c_smbus_write_byte_data(client, RT5028_IRQ2_STATUS_REG, 0xFF);
#endif

	rt5028_suspend_status = 0;
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt5028_pmic_pm_ops, rt5028_pmic_suspend, rt5028_pmic_resume);

static const struct i2c_device_id rt5028_id[] = {
	{ "rt5028", 0 },
	{ }
};
//MODULE_DEVICE_TABLE(i2c, rt5028_id);


static struct i2c_driver rt5028_pmic_driver = {
	.probe   = rt5028_pmic_probe,
	.remove	 = rt5028_pmic_remove,
	.driver	 = {
		.name = "rt5028",
		.pm = &rt5028_pmic_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = rt5028_of_match,
#endif
	},
	.id_table   = rt5028_id,
};

static int __init rt5028_pmic_init(void)
{
	rt5028_wq = create_singlethread_workqueue("rt5028_wq");
	if (!rt5028_wq)
		return -ENOMEM;

	return i2c_add_driver(&rt5028_pmic_driver);
}
subsys_initcall(rt5028_pmic_init);

static void __exit rt5028_pmic_exit(void)
{
	i2c_del_driver(&rt5028_pmic_driver);
	if (rt5028_wq)
		destroy_workqueue(rt5028_wq);
}
module_exit(rt5028_pmic_exit);

/* Module information */
MODULE_DESCRIPTION("RT5028 regulator driver");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");
