/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#include "videosource_types.h"
#include "videosource_common.h"
#include "videosource_if.h"
#include "videodecoder/videodecoder.h"
#include "mipi-deserializer/mipi-deserializer.h"

int DDI_I2C_Write(struct i2c_client * client, unsigned char * data, unsigned short reg_bytes, unsigned short data_bytes) {
	unsigned short bytes = reg_bytes + data_bytes;

	if(i2c_master_send(client, data, bytes) != bytes) {
		loge("addr = 0x%x, write error!!!! \n", client->addr);
		return -EIO;
	}

	return 0;
}

int DDI_I2C_Write_Remote(struct i2c_client * client, unsigned short remote_addr, unsigned char* data, unsigned short reg_bytes, unsigned short data_bytes) {
	unsigned short bytes = reg_bytes + data_bytes;
	unsigned short source_addr;
	static int try = 0;

	source_addr = client->addr;

	client->addr = remote_addr;

	if(i2c_master_send(client, data, bytes) != bytes) {
		loge("write error!!!! \n");
		log("addr = 0x%x\n", client->addr);
		try = (try + 1) % 4;
		client->addr = 0x60 + (0x01 * try);

		client->addr = source_addr;

		return -EIO;
	}

	client->addr = source_addr;

	return 0;
}

int DDI_I2C_Read(struct i2c_client * client, unsigned short reg, unsigned char reg_bytes, unsigned char * val, unsigned short val_bytes) {
	unsigned char data[2];

	if(reg_bytes == 2) {
		data[0]= reg>>8;
		data[1]= (u8)reg&0xff;
	} else {
		data[0]= (u8)reg&0xff;
	}

	if(i2c_master_send(client, data, reg_bytes) != reg_bytes) {
		loge("write error for read!!!! \n");
		return -EIO;
	}

	if(i2c_master_recv(client, val, val_bytes) != val_bytes) {
		loge("read error!!!! \n");
		return -EIO;
	}

	return 0;
}

int DDI_I2C_Read_Remote(struct i2c_client * client, unsigned short remote_addr, unsigned short reg, unsigned char reg_bytes, unsigned char * val, unsigned short val_bytes) {
	unsigned char data[2];
	unsigned short source_addr;

	if(reg_bytes == 2) {
		data[0]= reg>>8;
		data[1]= (u8)reg&0xff;
	} else {
		data[0]= (u8)reg&0xff;
	}

	source_addr = client->addr;

	client->addr = remote_addr;

	if(i2c_master_send(client, data, reg_bytes) != reg_bytes) {
		loge("write error for read!!!! \n");
		client->addr = source_addr;
		return -EIO;
	}

	if(i2c_master_recv(client, val, val_bytes) != val_bytes) {
		loge("read error!!!! \n");
		client->addr = source_addr;
		return -EIO;
	}

	client->addr = source_addr;

	return 0;
}

static const struct i2c_device_id videosource_id_table[] = {
	{ MODULE_NAME, 0, },
	{ }
};

static struct of_device_id videosource_of_match[] = {
	{
		.compatible = "analogdevices,adv7182",
		.data		= &videosource_adv7182,
	},
	{
		.compatible = "davicom,dm5886",
		.data		= &videosource_dm5886,
	},
	{
		.compatible = "intersil,isl79988",
		.data		= &videosource_isl79988,
	},
	{
		.compatible	= "maxim,max9286",
		.data		= &videosource_max9286,
	},
#if 0
	{
		.compatible	= "ti,ds90ub964"
		.data		= &videosource_ds90ub964,
	},
#endif
	{}
};
MODULE_DEVICE_TABLE(of, videosource_of_match);

int videosource_i2c_probe(struct i2c_client * client, const struct i2c_device_id * id) {
	videosource_t				* vdev		= NULL;
	const struct of_device_id	* of_id		= NULL;
	int							index		= 0;
	int							ret			= 0;

	// allocate and clear memory for a videosource device
	vdev = kzalloc(sizeof(videosource_t), GFP_KERNEL);
	if(vdev == NULL) {
		loge("Allocate a videosource device struct.\n");
		return -ENOMEM;
	}

	of_id = of_match_node(videosource_of_match, client->dev.of_node);
	if(!of_id) {
		loge("of_match_node\n");
		ret = -ENODEV;
		goto goto_free_videosource_data;
	}

	// get the videosource's index from its alias
	index = of_alias_get_id(client->dev.of_node, MODULE_NAME);

	// set the specific videosource information
	vdev = (videosource_t *)of_id->data;

	// set the i2c device
	vdev->client = client;

	log("videosource[%d] name: %s, type: %d, addr: 0x%x, client: 0x%p\n", \
		index, client->name, vdev->type, (client->addr)<<1, client);

	// register videosource_if
	videosource_if_probe(vdev);

	goto goto_end;

goto_free_videosource_data:
	// free the videosource data
	kfree(vdev);

goto_end:
	FUNCTION_OUT
	return ret;
}

int videosource_i2c_remove(struct i2c_client * client) {
	videosource_t		* vdev	= i2c_get_clientdata(client);

	FUNCTION_IN

	// unregister videosource_if
	videosource_if_remove(vdev);

	// free the videosource data
	kfree(vdev);
	client = NULL;

	FUNCTION_OUT
	return 0;
}

static struct i2c_driver videosource_i2c_driver = {
	.probe		= videosource_i2c_probe,
	.remove		= videosource_i2c_remove,
	.driver		= {
		.name			= MODULE_NAME,
		.of_match_table	= of_match_ptr(videosource_of_match),
	},
	.id_table	= videosource_id_table,
};

static int __init videosource_init(void) {
	return i2c_add_driver(&videosource_i2c_driver);
}
module_init(videosource_init);

static void __exit videosource_exit(void) {
	i2c_del_driver(&videosource_i2c_driver);
}
module_exit(videosource_exit);

