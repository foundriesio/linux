
/*******************************************************************
 *	Copyright 2009 Marvell Corporation
 *	Copyright (c) 2010, Marvell International Ltd (qingx@marvell.com)
 *	DESCRIPTION:
 *
 *	AUTHOR:  O. Baron
 *
 *	Date Created:  17/1/2011 5:28PM
 *
 *	FILENAME: adi7533.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *******************************************************************/

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#define	ADV7533_PACKET				0
#define	ADV7533_MAIN				1
#define	ADV7533_CEC_DSI				2
#define	ADV7533_EDID				3
#define	ADV7533_MAX				3

#define ADV7533_I2C_MAX_NUM_OF_BYTES 		0xff
#define ADV7533_I2C_ADDR_CEC_DSI		0x58

#define ADI7533_MAIN_CEC_DSI_ADDR		0xe1

#define ADV7533_MAIN_HPD_REG			0x42
#define ADV7533_MAIN_HPD_BIT			BIT(6)
#define ADV7533_MAIN_POWER_DOWN_REG		0x41
#define ADV7533_MAIN_POWER_DOWN_BIT		BIT(6)
#define ADV7533_MAIN_SYNC_REG			0x17
#define ADV7533_MAIN_SYNC_H			BIT(5)
#define ADV7533_MAIN_SYNC_V			BIT(6)

#define ADV7533_CEC_DSI_INTERNAL_TIMING		0x27
#define ADV7533_CEC_DSI_TOTAL_WIDTH_H_REG	0x28
#define ADV7533_CEC_DSI_TOTAL_WIDTH_L_REG	0x29
#define ADV7533_CEC_DSI_HSYNC_H_REG		0x2a
#define ADV7533_CEC_DSI_HSYNC_L_REG		0x2b
#define ADV7533_CEC_DSI_HFP_H_REG		0x2c
#define ADV7533_CEC_DSI_HFP_L_REG		0x2d
#define ADV7533_CEC_DSI_HBP_H_REG		0x2e
#define ADV7533_CEC_DSI_HBP_L_REG		0x2f
#define ADV7533_CEC_DSI_TOTAL_HEIGHT_H_REG	0x30
#define ADV7533_CEC_DSI_TOTAL_HEIGHT_L_REG	0x31
#define ADV7533_CEC_DSI_VSYNC_H_REG		0x32
#define ADV7533_CEC_DSI_VSYNC_L_REG		0x33
#define ADV7533_CEC_DSI_VFP_H_REG		0x34
#define ADV7533_CEC_DSI_VFP_L_REG		0x35
#define ADV7533_CEC_DSI_VBP_H_REG		0x36
#define ADV7533_CEC_DSI_VBP_L_REG		0x37

#define ADV7533_CEC_DSI_TIMING_H_MASK		0xff0
#define ADV7533_CEC_DSI_TIMING_L_MASK		0x00f
#define ADV7533_CEC_DSI_TIMING_OFFSET		4
#define ADV7533_CEC_DSI_INTERNAL_TIMING_EN	BIT(7)

static struct i2c_client *g_i2c_client[4] = {0, 0, 0, 0};

static u8 reg_val[4][ADV7533_I2C_MAX_NUM_OF_BYTES];

static const char * const hdmi_supply_names[] = {
	"hdmi_vdd",		/* avdd dvdd */
	"hdmi_v1p2",		/* v1p2 */
};

static int gpio_pd = -EINVAL;
static int gpio_dsi_sel = -EINVAL;
static int gpio_int = -EINVAL;
static bool power_on = false;

struct adv7533_priv {
	struct regulator_bulk_data supplies[ARRAY_SIZE(hdmi_supply_names)];
	bool has_regulator;
};

struct lcd_frame_config {
	int LCD_Horizontal;
	int LCD_Vertical;
	int	LCD_HSW;
	int	LCD_VSW;
	int	LCD_BLW;
	int	LCD_ELW;
	int	LCD_BFW;
	int	LCD_EFW;
	int	LCD_Pixel_Clk;
	int	LCD_FRAME_Rate;
};

static struct lcd_frame_config global_hdmi_frame[] = {
	/*adv7533*/
	{ 640, 480, 96, 2, 48, 16, 33, 10, 25170000, 60},/*640x480p  -1*/
	{ 720, 480, 62, 6, 60, 19, 30, 9, 27000000, 60},/*720x480p  -2*/
	{ 720, 480, 62, 6, 60, 19, 30, 9, 27000000, 60},/*720x480p -3*/
	{1280, 720, 40, 5, 220, 110, 20, 5, 74170000, 60},/*1280x720p -4*/
	{1980, 1080, 44, 5, 148, 88, 15, 2, 74170000, 60},/*1980x1080i -5*/
	{ 720, 480, 62, 3, 114, 17, 15, 5, 27000000, 60},/*720x480p -6*/
	{ 720, 480, 62, 3, 114, 17, 15, 5, 27000000, 60},/*720x480p -7*/
	{1440, 240, 124, 3, 114, 38, 15, 4, 27000000, 60},/*1440x240p -8*/
	{1440, 240, 124, 3, 114, 38, 15, 4, 27000000, 60},/*1440x240p -9*/
	{2880, 480, 248, 3, 228, 76, 15, 4, 54000000, 60},/*2880x480i -10*/
	{2880, 480, 248, 3, 228, 76, 15, 4, 54000000, 60},/*2880x480i -11*/
	{2880, 240, 248, 3, 228, 76, 15, 4, 54000000, 60},/*2880x240p -12*/
	{2880, 240, 248, 3, 228, 76, 15, 4, 54000000, 60},/*2880x240p -13*/
	{1440, 480, 124, 6, 120, 32, 30, 9, 54000000, 60},/*1440x480p -14*/
	{1440, 480, 124, 6, 120, 32, 30, 9, 54000000, 60},/*1440x480p -15*/
	{1920, 1080, 44, 5, 148, 88, 36, 4, 148350000, 60},/*1920x1080p -16*/
	{ 720, 576, 64, 5, 68, 12, 39, 5, 27000000, 50},/*720x576p -17*/
	{ 720, 576, 64, 5, 68, 12, 39, 5, 27000000, 50},/*720x576p -18*/
	{1280, 720, 40, 5, 220, 440, 20, 5, 74170000, 50},/*1280x720p -19*/
	/*OR.B - Need to add all formats!!*/
};

static void get_frame_config(struct lcd_frame_config *lcd_frame, int fmt)
{
	lcd_frame->LCD_Horizontal =	global_hdmi_frame[fmt].LCD_Horizontal;
	lcd_frame->LCD_Vertical =	global_hdmi_frame[fmt].LCD_Vertical;
	lcd_frame->LCD_HSW =		global_hdmi_frame[fmt].LCD_HSW;
	lcd_frame->LCD_VSW =		global_hdmi_frame[fmt].LCD_VSW;
	lcd_frame->LCD_BLW =		global_hdmi_frame[fmt].LCD_BLW;
	lcd_frame->LCD_ELW =		global_hdmi_frame[fmt].LCD_ELW;
	lcd_frame->LCD_BFW =		global_hdmi_frame[fmt].LCD_BFW;
	lcd_frame->LCD_EFW =		global_hdmi_frame[fmt].LCD_EFW;
	lcd_frame->LCD_Pixel_Clk =	global_hdmi_frame[fmt].LCD_Pixel_Clk;
	lcd_frame->LCD_FRAME_Rate =	global_hdmi_frame[fmt].LCD_FRAME_Rate;
	printk(KERN_DEBUG "LCD_Horizontal %d\n", lcd_frame->LCD_Horizontal);
	printk(KERN_DEBUG "LCD_Vertical %d\n", lcd_frame->LCD_Vertical);
	printk(KERN_DEBUG "LCD_HSW %d\n", lcd_frame->LCD_HSW);
	printk(KERN_DEBUG "LCD_VSW %d\n", lcd_frame->LCD_VSW);
	printk(KERN_DEBUG "LCD_BLW %d\n", lcd_frame->LCD_BLW);
	printk(KERN_DEBUG "LCD_ELW %d\n", lcd_frame->LCD_ELW);
	printk(KERN_DEBUG "LCD_BFW %d\n", lcd_frame->LCD_BFW);
	printk(KERN_DEBUG "LCD_EFW %d\n", lcd_frame->LCD_EFW);
	printk(KERN_DEBUG "LCD_Pixel_Clk %d\n", lcd_frame->LCD_Pixel_Clk);
	printk(KERN_DEBUG "LCD_FRAME_Rate %d\n", lcd_frame->LCD_FRAME_Rate);

}

static int hdmi_write(int idx, u8 addr, u8 value)
{
	reg_val[idx][addr] = value;
	return i2c_smbus_write_byte_data(g_i2c_client[idx], addr, value);
}

static int hdmi_read(int idx, u8 addr)
{
	u8 val;

	val = i2c_smbus_read_byte_data(g_i2c_client[idx], addr);
	reg_val[idx][addr] = val;
	return val;
}

static int hdmi_set_bit(int idx, u8 addr, u8 bit)
{
	u8 val;

	val = reg_val[idx][addr];
	val |= bit;
	i2c_smbus_write_byte_data(g_i2c_client[idx], addr, val);
	reg_val[idx][addr] = val;
	return 0;
}

static int hdmi_clr_bit(int idx, u8 addr, u8 bit)
{
	u8 val;

	val = reg_val[idx][addr];
	val &= ~bit;
	i2c_smbus_write_byte_data(g_i2c_client[idx], addr, val);
	reg_val[idx][addr] = val;
	return 0;
}
static bool adv7533_check_hpd(void)
{
	int data;

	data = hdmi_read(ADV7533_MAIN, ADV7533_MAIN_HPD_REG);
	reg_val[ADV7533_MAIN][ADV7533_MAIN_HPD_REG] = data & 0xff;
	if (data & ADV7533_MAIN_HPD_BIT)
		return true;
	return false;
}

static int adv7533_set_power(bool enable)
{
	unsigned int data;
	if (!enable)
		return -EINVAL;
	data = hdmi_read(ADV7533_MAIN, ADV7533_MAIN_POWER_DOWN_REG);
	if (enable)
		data &= ~ADV7533_MAIN_POWER_DOWN_BIT;
	else
		data |= ADV7533_MAIN_POWER_DOWN_BIT;
	hdmi_write(ADV7533_MAIN, ADV7533_MAIN_POWER_DOWN_REG, data);

	return 0;
}

/* test pattern mode for adv7533, need dsi clk 594Mhz*/
static int __attribute__ ((unused))adv7533_test_pattern(bool enable)
{
	if (enable)	{
		/*Set to test pattern*/
		hdmi_write(ADV7533_CEC_DSI, 0x55, 0x80);

		/*Set HDMI enable*/
		hdmi_write(ADV7533_CEC_DSI, 0x03, 0x89);

		/*Set to HDMI output*/
		hdmi_write(ADV7533_MAIN, 0xAF, 0x16);
	} else {
		/*Disable test pattern*/
		hdmi_write(ADV7533_CEC_DSI, 0x55, 0x00);
	}

	return 0;
}

static void adv7533_set_3_lanes(void)
{
	/* input clk = 640M / 2 = 320M */

	/* clk divider 4 for 3 lanes, input 300M/4 = 74M */
	hdmi_write(ADV7533_CEC_DSI, 0x16, 0x24);

	/* Set Number of DSI Data Lanes to 3*/
	hdmi_set_bit(ADV7533_CEC_DSI, 0x1C, 0x30);

	/*Set HDMI enable*/
	hdmi_write(ADV7533_CEC_DSI, 0x03, 0x89);

	/*Set to HDMI output*/
	hdmi_write(ADV7533_MAIN, 0xAF, 0x16);

	/*disable timing generator*/
	hdmi_clr_bit(ADV7533_CEC_DSI, ADV7533_CEC_DSI_INTERNAL_TIMING,
		ADV7533_CEC_DSI_INTERNAL_TIMING_EN);
}

static void adv7533_set_4_lanes(void)
{
	/* clk divider 3 */
	hdmi_write(ADV7533_CEC_DSI, 0x16, 0x1c);

	/* 1280x720p@60Hz, 24bits/pixel Timing Generator Enable p18 */
	hdmi_set_bit(ADV7533_CEC_DSI, 0x27, 0x80);

	/* Set Number of DSI Data Lanes to 4 */
	hdmi_set_bit(ADV7533_CEC_DSI, 0x1C, 0x40);

	/*Set HDMI enable*/
	hdmi_write(ADV7533_CEC_DSI, 0x03, 0x89);

	/*Set to HDMI output*/
	hdmi_write(ADV7533_MAIN, 0xAF, 0x16);
}

static int adv7533_set_video(bool internal_timing)
{
	struct lcd_frame_config frame_timings;
	int fmt;
	int temp_timing;
	u8 temp_byte;

	fmt = 3;

	adv7533_set_3_lanes();

	/* HDMI Startup*/
	/* Set Vsync and Hsync Polarity according to hdmi_format*/
	/* Fixed Register Settings per PG*/
	hdmi_write(ADV7533_CEC_DSI, 0x24, 0x20);
	hdmi_write(ADV7533_CEC_DSI, 0x26, 0x3C);
	hdmi_write(ADV7533_MAIN, 0x9A, 0xE0);
	hdmi_write(ADV7533_MAIN, 0x9B, 0x19);
	hdmi_write(ADV7533_MAIN, 0xBA, 0x70);
	hdmi_write(ADV7533_MAIN, 0xDE, 0x82);
	hdmi_write(ADV7533_MAIN, 0xE4, 0x40);
	hdmi_write(ADV7533_MAIN, 0xE5, 0x80);

	/* test with fixed value */
	hdmi_write(ADV7533_MAIN, 0x16, 0x20);
	hdmi_write(ADV7533_MAIN, 0x9a, 0xe0);
	hdmi_write(ADV7533_MAIN, 0xba, 0x70);
	hdmi_write(ADV7533_MAIN, 0xde, 0x82);
	hdmi_write(ADV7533_MAIN, 0xe4, 0x40);
	hdmi_write(ADV7533_MAIN, 0xe5, 0x80);

	hdmi_write(ADV7533_CEC_DSI, 0x15, 0x10); //
	hdmi_write(ADV7533_CEC_DSI, 0x17, 0xD0);
	hdmi_write(ADV7533_CEC_DSI, 0x24, 0x20); //
	hdmi_set_bit(ADV7533_CEC_DSI, 0x57, 0x11);

	/* Ungate Audio and CEC Clocks - clear bit 4 and 5*/
	hdmi_clr_bit(ADV7533_CEC_DSI, 0x05, 0x20);
	hdmi_clr_bit(ADV7533_CEC_DSI, 0x05, 0x10);

	/* Set CEC Power Mode = Always Active. Clear bits before*/
	hdmi_clr_bit(ADV7533_CEC_DSI, 0xBE, 0x03);
	hdmi_set_bit(ADV7533_CEC_DSI, 0xBE, 0x01);

	/* Set Timing Generator Settings*/
	if (internal_timing == true) {
		get_frame_config(&frame_timings, fmt);

		/*Set Horizontal parameters*/
		temp_timing = frame_timings.LCD_Horizontal
			+ frame_timings.LCD_HSW + frame_timings.LCD_BLW
			+ frame_timings.LCD_ELW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_TOTAL_WIDTH_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_TOTAL_WIDTH_L_REG, temp_byte);

		temp_timing = frame_timings.LCD_HSW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_HSYNC_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_HSYNC_L_REG, temp_byte);

		temp_timing = frame_timings.LCD_ELW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_HFP_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_HFP_L_REG, temp_byte);

		temp_timing = frame_timings.LCD_BLW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_HBP_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_HBP_L_REG, temp_byte);

		/*Set Vertical parameters*/
		temp_timing = frame_timings.LCD_Vertical + frame_timings.LCD_VSW
			+ frame_timings.LCD_BFW + frame_timings.LCD_EFW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_TOTAL_HEIGHT_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_TOTAL_HEIGHT_L_REG, temp_byte);

		temp_timing = frame_timings.LCD_VSW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_VSYNC_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_VSYNC_L_REG, temp_byte);

		temp_timing = frame_timings.LCD_EFW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_VFP_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_VFP_L_REG, temp_byte);

		temp_timing = frame_timings.LCD_BFW;
		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_H_MASK)
			>> ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_VBP_H_REG, temp_byte);

		temp_byte = (u8)((temp_timing & ADV7533_CEC_DSI_TIMING_L_MASK)
			<< ADV7533_CEC_DSI_TIMING_OFFSET);
		hdmi_write(ADV7533_CEC_DSI,
			ADV7533_CEC_DSI_VBP_L_REG, temp_byte);

		/*Enable timing generator*/
		hdmi_set_bit(ADV7533_CEC_DSI, ADV7533_CEC_DSI_INTERNAL_TIMING,
			ADV7533_CEC_DSI_INTERNAL_TIMING_EN);
	}

	/* Enable GC Packet */
	hdmi_set_bit(ADV7533_MAIN, 0x40, 0x80);
	/* Set Color Depth to 24 Bits/Pixel. Clear field before*/
	hdmi_clr_bit(ADV7533_MAIN, 0x4C, 0x0F);
	hdmi_set_bit(ADV7533_MAIN, 0x4C, 0x04);
	/* Set Active Format Aspect Ratio = 4:3 (Center) Clear field before*/
	hdmi_clr_bit(ADV7533_MAIN, 0x56, 0x0F);
	hdmi_set_bit(ADV7533_MAIN, 0x56, 0x09);
	/* Set V1P2 Enable = +1.2V*/
	hdmi_set_bit(ADV7533_MAIN, 0xE4, 0x80);

	return 0;
}

int adi7533_set_audio(void)
{
	u32 n_value;
	u8 temp_byte;

	/* HDMI Startup*/
	n_value = 6144;
	temp_byte = (u8)((n_value&0xF0000)>>16);
	hdmi_write(ADV7533_MAIN, 0x1, temp_byte);

	temp_byte = (u8)((n_value&0xFF00)>>8);
	hdmi_write(ADV7533_MAIN, 0x2, temp_byte);

	temp_byte = (u8)(n_value&0xFF);
	hdmi_write(ADV7533_MAIN, 0x3, temp_byte);

	/* Set SPDIF Enable = SPDIF Enabled*/
	hdmi_set_bit(ADV7533_MAIN, 0x0B, 0x80);

	/* Set I2S Enable = I2S Enabled*/
	hdmi_set_bit(ADV7533_MAIN, 0x0C, 0x04);

	/* Set I2S Sampling Frequency = 48.0 kHz is 0x20, 44.1Khz is 0x0*/
	hdmi_write(ADV7533_MAIN, 0x15, 0x0);

	/* Set Audio Select = SPDIF Input is 0x10, I2S is 0x0*/
	hdmi_set_bit(ADV7533_MAIN, 0x0A, 0x00);

	return 0;
}

static irqreturn_t adv7533_irq_handler(int irq, void *dev)
{
	bool attached;
	int ret, val;

	/* clear interrupt */
	val = hdmi_read(ADV7533_MAIN, 0x96);
	hdmi_write(ADV7533_MAIN, 0x96, val);

	if (!(val & BIT(7)))
		return IRQ_HANDLED;

	/* HPD pin must be high first */
	attached = adv7533_check_hpd();
	if (attached == false) {
		adv7533_set_power(false);
		power_on = false;
	} else if (!power_on) {
		/* Power On MAIN */
		power_on = true;
		adv7533_set_power(true);
		adv7533_set_video(false);
	}
	return IRQ_HANDLED;
}

int adv7533_hdmi_reset(struct adv7533_priv *priv)
{
	/* power down pin output 0 as usual, while toggle for 10 ms */
	if (gpio_is_valid(gpio_pd)) {
		gpio_direction_output(gpio_pd, 0);
		msleep(10);
		gpio_direction_output(gpio_pd, 1);
		msleep(10);
		gpio_direction_output(gpio_pd, 0);
	}
	return 0;
}

static int adv7533_init(struct adv7533_priv *priv)
{
	int i, ret;

	adv7533_hdmi_reset(priv);

	for (i = 0; i < ADV7533_I2C_MAX_NUM_OF_BYTES; i++) {
		reg_val[ADV7533_MAIN][i] = hdmi_read(ADV7533_MAIN, i);
		reg_val[ADV7533_CEC_DSI][i] = hdmi_read(ADV7533_CEC_DSI, i);
	}
	printk(KERN_NOTICE "hdmi: id 0x%x\n", reg_val[ADV7533_CEC_DSI][0]);

	/*
	* First - check if the cable is connected to the board.
	* If not - return error.
	*/
	ret = adv7533_check_hpd();
	if (ret == false) {
		printk(KERN_ERR "hdmi: hot plug is not detected! Please insert HDMI cable.\n");
		return -EINVAL;
	}

	/*Second - power up HDMI in normal operation*/
	ret = adv7533_set_power(true);
	if (ret < 0) {
		printk(KERN_ERR "hdmi: adi7533_set_hdmi_power_down failed!!\n");
		return -EINVAL;
	}

	printk(KERN_NOTICE "hdmi: init normal operation.\n");

	power_on = true;
	/* test pattern mode*/
	/*
	ret = adv7533_test_pattern(true);
	if(ret < 0)	{
		printk("hdmi: adi7533_test_pattern failed!!\n");
		return -EINVAL;
	}
	*/

	/*
	ret = adi7533_set_audio();
	if (ret < 0) {
		printk(KERN_ERR "hdmi: adi7533_set_audio failed!!\n");
		return -EINVAL;
	}
	*/

	/*
	* Set ADV registers to output HDMI according to resolution
	* and disable internal timing generator
	*/
	ret = adv7533_set_video(false);
	if (ret < 0) {
		printk(KERN_ERR "hdmi: adv7533_set_video failed!!\n");
		return -EINVAL;
	}
	printk(KERN_NOTICE "hdmi: ready for receiving DSI data.\n");

	return 0;
}

static const struct i2c_device_id adv7533_id[] = {
	{ "adv7533-packet", 0 },
	{ "adv7533-main", 1 },
	{ "adv7533-cec-dsi", 2 },
	{ "adv7533-edid", 3 },
	{ }
};

static const struct of_device_id adv7533_of_match[] = {
	{ .compatible = "adv7533,packet", (void *)0},
	{ .compatible = "adv7533,main", (void *)1},
	{ .compatible = "adv7533,cec_dsi", (void *)2},
	{ .compatible = "adv7533,edid", (void *)3},
	{ }
};
MODULE_DEVICE_TABLE(of, adv7533_of_match);

static int adv7533_probe(struct i2c_client *client,
	const struct i2c_device_id *client_id)
{
	const struct of_device_id *oid;
	struct adv7533_priv *priv;
	struct device *dev = &client->dev;
	struct device_node *np = client->dev.of_node;
	int ret = 0, id;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	oid = of_match_node(adv7533_of_match, np);

	if (client_id)
		id = client_id->driver_data;
	else
		id = (unsigned long)oid->data;

	g_i2c_client[id] = client;
	printk(KERN_NOTICE "hdmi: %s\n", client->name);

	/* regulators */
/*
	if (of_property_read_bool(client->dev.of_node, "has-regulator"))
		priv->has_regulator = true;

	for (i = 0; i < ARRAY_SIZE(priv->supplies); i++)
		priv->supplies[i].supply = hdmi_supply_names[i];

	if (priv->has_regulator) {
		ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(priv->supplies),
				priv->supplies);
		if (ret)
			return -EPROBE_DEFER;

		ret = regulator_bulk_enable(ARRAY_SIZE(priv->supplies),
				priv->supplies);
		if (ret) {
			dev_err(dev, "failed to enable supplies: %d\n", ret);
			return ret;
		}
	}
*/
	/* gpio_pd output 0 */
	if (!gpio_is_valid(gpio_pd)) {
		gpio_pd = of_get_named_gpio(np, "gpio_pd", 0);
		if (gpio_is_valid(gpio_pd)) {
			ret = devm_gpio_request_one(dev, gpio_pd,
					GPIOF_OUT_INIT_LOW, "gpio-pd");
			if (ret) {
				dev_err(dev, "failed to request gpio %d: %d\n",
					gpio_pd, ret);
				return ret;
			}
		}
	}
#if 0
	/* gpio_dsi_sel output 0 */
	if (!gpio_is_valid(gpio_dsi_sel)) {
		gpio_dsi_sel = of_get_named_gpio(np, "gpio_dsi_sel", 0);
		if (gpio_is_valid(gpio_dsi_sel)) {
			ret = devm_gpio_request_one(dev, gpio_dsi_sel,
					GPIOF_OUT_INIT_LOW, "gpio-dsi-sel");
			if (ret) {
				dev_err(dev, "failed to request gpio %d: %d\n",
					gpio_dsi_sel, ret);
				return ret;
			}
		}
	}
#endif
	if (!gpio_is_valid(gpio_int)) {
		gpio_int = of_get_named_gpio(np, "gpio_int", 0);
		if (gpio_is_valid(gpio_int)) {
			ret = devm_request_threaded_irq(dev,
					gpio_to_irq(gpio_int), NULL,
					adv7533_irq_handler,
					IRQF_ONESHOT,
					"HDMI_INT", priv);
			if (ret < 0) {
				dev_err(dev, "unable to request IRQ\n");
				return -ENXIO;
			}
		}
	}
	i2c_set_clientdata(client, priv);

	if (id == ADV7533_MAX)
		ret = adv7533_init(priv);

	return ret;
}

static int adv7533_remove(struct i2c_client *client)
{
	struct adv7533_priv *priv = i2c_get_clientdata(client);
	int ret;

	/* disable power */
/*
	if (priv->has_regulator) {
		ret = regulator_bulk_disable(ARRAY_SIZE(priv->supplies),
				priv->supplies);
		if (ret) {
			dev_err(&client->dev, "failed to disable supplies: %d\n", ret);
			return ret;
		}
	}
*/
	return 0;
}

static struct i2c_driver adv7533_driver = {
	.driver		= {
		.name	= "adv7533-transmitter",
		.of_match_table = adv7533_of_match,
	},
	.id_table	= adv7533_id,
	.probe		= adv7533_probe,
	.remove		= adv7533_remove,
};

module_i2c_driver(adv7533_driver);

MODULE_AUTHOR("QingX");
MODULE_DESCRIPTION("HDMI driver for PXA955");
MODULE_LICENSE("GPL");

