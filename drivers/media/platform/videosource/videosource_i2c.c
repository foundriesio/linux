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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#include "videosource_i2c.h"
#include "videodecoder/videodecoder.h"

static struct i2c_client * videosource_i2c_client = NULL;

int DDI_I2C_Write(unsigned char* data, unsigned short reg_bytes, unsigned short data_bytes) {
	unsigned short bytes = reg_bytes + data_bytes;
#ifdef CONFIG_VIDEO_VIDEOSOURCE_VIDEODECODER_DM5886
	static int try = 0;
#endif//CONFIG_VIDEO_VIDEOSOURCE_VIDEODECODER_DM5886

	if(i2c_master_send(videosource_i2c_client, data, bytes) != bytes) {
		printk("write error!!!! \n");
		printk(KERN_INFO "addr = 0x%x\n", videosource_i2c_client->addr);
#ifdef CONFIG_VIDEO_VIDEOSOURCE_VIDEODECODER_DM5886
		try = (try + 1) % 4;
		videosource_i2c_client->addr = 0x60 + (0x01 * try);
#endif//CONFIG_VIDEO_VIDEOSOURCE_VIDEODECODER_DM5886
		return -EIO;
	}

	return 0;
}

int DDI_I2C_Write_Remote(unsigned short remote_addr, unsigned char* data, unsigned short reg_bytes, unsigned short data_bytes) {
	unsigned short bytes = reg_bytes + data_bytes;
	unsigned short source_addr;
	static int try = 0;

	source_addr = videosource_i2c_client->addr;

	videosource_i2c_client->addr = remote_addr;

	if(i2c_master_send(videosource_i2c_client, data, bytes) != bytes) {
		printk("write error!!!! \n");
		printk(KERN_INFO "addr = 0x%x\n", videosource_i2c_client->addr);
		try = (try + 1) % 4;
		videosource_i2c_client->addr = 0x60 + (0x01 * try);

		videosource_i2c_client->addr = source_addr;

		return -EIO;
	}

	videosource_i2c_client->addr = source_addr;

	return 0;
}

int cam_i2c_read(const struct i2c_client * client, unsigned char * cmd, unsigned char cmd_len, unsigned char * val, unsigned char val_len) {
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[2];

	/* msg for cmd */
	msgs[0].addr = client->addr;
	msgs[0].len = cmd_len;
	msgs[0].buf = cmd;
	msgs[0].flags = client->flags & I2C_M_TEN;

	/* msg for read */
	msgs[1].addr = client->addr;
	msgs[1].len = val_len;
	msgs[1].buf = val;
	msgs[1].flags = client->flags & I2C_M_TEN;
	msgs[1].flags |= I2C_M_RD;

	ret = i2c_transfer(adap, msgs, 2);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 2) ? 0 : ret;
}

int DDI_I2C_Read(unsigned short reg, unsigned char reg_bytes, unsigned char * val, unsigned short val_bytes) {
	unsigned char data[2];

	if(reg_bytes == 2) {
		data[0]= reg>>8;
		data[1]= (u8)reg&0xff;
	} else {
		data[0]= (u8)reg&0xff;
	}

	if(i2c_master_send(videosource_i2c_client, data, reg_bytes) != reg_bytes) {
		printk("write error for read!!!! \n");
		return -EIO;
	}

	if(i2c_master_recv(videosource_i2c_client, val, val_bytes) != val_bytes) {
		printk("read error!!!! \n");
		return -EIO;
	}

	return 0;
}

int DDI_I2C_Read_Remote(unsigned short remote_addr, unsigned short reg, unsigned char reg_bytes, unsigned char * val, unsigned short val_bytes) {
	unsigned char data[2];
	unsigned short source_addr;

	if(reg_bytes == 2) {
		data[0]= reg>>8;
		data[1]= (u8)reg&0xff;
	} else {
		data[0]= (u8)reg&0xff;
	}

	source_addr = videosource_i2c_client->addr;

	videosource_i2c_client->addr = remote_addr;

	if(i2c_master_send(videosource_i2c_client, data, reg_bytes) != reg_bytes) {
		printk("write error for read!!!! \n");
		videosource_i2c_client->addr = source_addr;
		return -EIO;
	}

	if(i2c_master_recv(videosource_i2c_client, val, val_bytes) != val_bytes) {
		printk("read error!!!! \n");
		videosource_i2c_client->addr = source_addr;
		return -EIO;
	}

	videosource_i2c_client->addr = source_addr;

	return 0;
}

static const struct i2c_device_id videosource_i2c_id[] = {
	{ "tcc-cam-sensor", 0, },
	{ }
};

static struct of_device_id videosource_i2c_of_match[] = {
	{ .compatible = CAM_I2C_NAME },
	{}
};
MODULE_DEVICE_TABLE(of, videosource_i2c_of_match);

static int videosource_i2c_probe(struct i2c_client * client, const struct i2c_device_id * id) {
	videosource_i2c_client = client;

	printk(KERN_INFO "%s() name = %s, addr = 0x%x, client = 0x%p \n", \
		__func__, client->name, (client->addr)<<1, videosource_i2c_client);

	return 0;
}

static int videosource_i2c_remove(struct i2c_client * client) {
	videosource_i2c_client = NULL;

	return 0;
}

static struct i2c_driver videosource_i2c_driver = {
	.probe		= videosource_i2c_probe,
	.remove		= videosource_i2c_remove,
	.driver		= {
		.name			= "tcc-cam-sensor",
		.of_match_table	= of_match_ptr(videosource_i2c_of_match),
	},
	.id_table	= videosource_i2c_id,
};

static int __init videosource_i2c_init(void) {
	return i2c_add_driver(&videosource_i2c_driver);
}
module_init(videosource_i2c_init);

static void __exit videosource_i2c_exit(void) {
	i2c_del_driver(&videosource_i2c_driver);
}
module_exit(videosource_i2c_exit);

