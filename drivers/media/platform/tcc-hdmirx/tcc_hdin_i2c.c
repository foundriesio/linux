/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include "tcc_hdin_i2c.h"
#include <asm/delay.h>
#include <linux/delay.h>

#include "tcc_hdin_ctrl.h"
#include <asm/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <linux/i2c.h>

static int debug	   = 0;

#define LOG_MODULE_NAME "HDMI"

#define logl(level, fmt, ...) printk(level "[%s][%s] %s - " pr_fmt(fmt), #level + 5, LOG_MODULE_NAME, __FUNCTION__, ##__VA_ARGS__)
#define log(fmt, ...) logl(KERN_INFO, fmt, ##__VA_ARGS__)
#define loge(fmt, ...) logl(KERN_ERR, fmt, ##__VA_ARGS__)
#define logw(fmt, ...) logl(KERN_WARNING, fmt, ##__VA_ARGS__)
#define logd(fmt, ...) if (debug) logl(KERN_DEBUG, fmt, ##__VA_ARGS__)

// check for send & receive message.
//#define HDIN_I2C_SEND
//#define HDIN_I2C_RECEIVE

#define 		I2C_WR		0
#define 		I2C_RD		1

static const struct i2c_device_id hdin_i2c_id[] = {
	{ "tcc-hdmirx-i2c", 0, },
	{ }
};

struct hdin_i2c_chip_info {
	unsigned gpio_start;
	uint16_t reg_output;
	uint16_t reg_direction;

	struct i2c_client *client;
	struct gpio_chip gpio_chip;
};

static struct i2c_client *hdin_i2c_client = NULL;

static int hdin_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct hdin_i2c_chip_info 		*chip;

	chip = kzalloc(sizeof(struct hdin_i2c_chip_info), GFP_KERNEL);
	if(chip == NULL)
	{
		log("no chip info. \n");
		return -ENOMEM;
	}

	chip->client = client;
	i2c_set_clientdata(client, chip);
	hdin_i2c_client = client;
	log("addr = 0x%x , hdin_i2c_client = 0x%p \n", (client->addr)<<1,hdin_i2c_client);

	return 0;
}

static int hdin_i2c_remove(struct i2c_client *client)
{
	struct hdin_i2c_chip_info 		*chip  = i2c_get_clientdata(client);
	
	kfree(chip);
	hdin_i2c_client = NULL;
	
	return 0;
}

static struct of_device_id hdin_i2c_of_match[] = {
	{ .compatible = "tcc-hdmirx-i2c" },
	{}
};
MODULE_DEVICE_TABLE(of, hdin_i2c_of_match);

static struct i2c_driver hdin_i2c_driver = {
	.driver = {
		.name	= "tcc-hdmirx-i2c",
		.of_match_table = of_match_ptr(hdin_i2c_of_match),			
	},
	.probe		= hdin_i2c_probe,
	.remove		= hdin_i2c_remove,
	.id_table	= hdin_i2c_id,
};

int DDI_I2C_Write_HDMI_IN(unsigned short reg, unsigned char* data, unsigned short reg_bytes, unsigned short data_bytes)
{
	unsigned short bytes = reg_bytes + data_bytes;
	unsigned char buffer[32];
	
#if defined(HDIN_I2C_SEND)	
	int i=0;  	//for debug
#endif	
	logd("DDI_I2C_Write_HDMI_IN \n");

	if(reg_bytes == 2)
	{
		
		memset(&buffer,0x00,sizeof(buffer));
		buffer[0]= reg>>8;
		buffer[1]= (u8)reg&0xff;
	//	buffer[2] = data[0]&0xff;
		memcpy(&buffer[2],data ,data_bytes);
	}
	else
	{
		memset(&buffer,0x00,sizeof(buffer));
		buffer[0]=reg&0xff;
		memcpy(&buffer[1],data,data_bytes);
	}
#if defined(HDIN_I2C_SEND)
	log("writing addr = 0x");
	for(i=0;i<reg_bytes;i++) log("%02x",buffer[i]);
	log(", writing value = 0x");
	for(i=reg_bytes;i<bytes;i++) log("%02x",buffer[i]);
	log(" \n");
#endif
	if(i2c_master_send(hdin_i2c_client, buffer, bytes) != bytes)
	{
		log("write error!!!! \n");
		return -EIO; 
	}
	return 0;
}
int DDI_I2C_Read_HDMI_IN(unsigned short reg, unsigned char reg_bytes, unsigned char *val, unsigned short val_bytes)
{
	unsigned char data[2];

#if defined(HDIN_I2C_RECEIVE)	
	int i=0;  	//for debug
#endif	

	
	if(reg_bytes == 2)
	{
		data[0]= reg>>8;
		data[1]= (u8)reg&0xff;
	}
	else
	{
		data[0]= (u8)reg&0xff;
	}
	if(i2c_master_send(hdin_i2c_client, data, reg_bytes) != reg_bytes)
	{
	     log("write error for read!!!! \n");
	     return -EIO; 
	}


	if(i2c_master_recv(hdin_i2c_client, val, val_bytes) != val_bytes)
	{
		log("read error!!!! \n");
		return -EIO; 
	}
#if defined(HDIN_I2C_RECEIVE)
	log("I2C_Read_HDMI_IN : read addr = 0x");
	for(i=0;i<reg_bytes;i++) log("%02x",data[i]);
	log(", read value = 0x%02x \n",*val);
#endif
    return 0;
}

static int __init hdin_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&hdin_i2c_driver);
	
	return ret;
}
module_init(hdin_i2c_init);

static void __exit hdin_i2c_exit(void)
{
	i2c_del_driver(&hdin_i2c_driver);
}
module_exit(hdin_i2c_exit);