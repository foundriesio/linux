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
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/miscdevice.h>
#include <linux/i2c.h>

#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

#include <linux/gpio.h>

#include "tcc_ep9553t_cec.h"
#include "tcc_ep9553t_cec_ioctl.h"

static int debug = 0;
#define dprintk(msg...) if(debug) { printk( "\e[33mep9553t_cec: \e[0m" msg); }

#define SEND_MESSAGE_TIMEOUT 	(1000)
#define RX_MSG_TIMEOUT			(1000)

//#define CONFIG_EP9553T_CEC_LISTEN_MODE

struct tcc_ep9553t_cec_t {
	struct i2c_client *client;
	unsigned short physical_address;
	unsigned char device_type1;
	unsigned char device_type2;
	unsigned int language;
	unsigned int vendor_id;
	char osd_name[OSD_NAME_MAX+1];

	struct mutex cec_mutex; 
	struct completion send_completion;
	wait_queue_head_t rx_wq;
	int rx_data_valid;

	spinlock_t lock;
	
	//gpio config
	int pwr_port;
	int rst_port;
	int irq_port;
};

static int tcc_ep9553t_i2c_write(struct i2c_client *cli, unsigned short reg, unsigned char* data, unsigned short data_bytes)
{
	unsigned short bytes = REG_LEN + data_bytes;
	unsigned char buffer[32];
	
#if defined(DUMP_I2C_WRITE)	
	int i=0;  	//for debug
#endif	

	dprintk("%s\n", __func__);
	
	memset(&buffer,0x00,sizeof(buffer));
	buffer[0]= reg>>8;
	buffer[1]= (u8)reg&0xff;
	memcpy(&buffer[2], data, data_bytes);

#if defined(DUMP_I2C_WRITE)
	printk("%s: writing addr = 0x", __func__);
	for(i=0;i<REG_LEN;i++)
		printk("%02x",buffer[i]);
	printk(", writing value = 0x");
	for(i=REG_LEN;i<bytes;i++)
		printk("%02x",buffer[i]);
	printk(" \n");
#endif
	if(i2c_master_send(cli, buffer, bytes) != bytes)
	{
		printk("write error!!!! \n");
		return -EIO; 
	}
	return 0;
}

int tcc_ep9553t_i2c_read(struct i2c_client *cli, unsigned short reg, unsigned char *val, unsigned short val_bytes)
{
	unsigned char data[2];

#if defined(DUMP_I2C_READ)	
	int i=0;  	//for debug
#endif	

	dprintk("%s\n", __func__);
	
	data[0]= reg>>8;
	data[1]= (u8)reg&0xff;

	if(i2c_master_send(cli, data, REG_LEN) != REG_LEN)
	{
	     printk("write error for read!!!! \n");			
	     return -EIO; 
	}

	if(i2c_master_recv(cli, val, val_bytes) != val_bytes)
	{
		printk("read error!!!! \n");
		return -EIO; 
	}
#if defined(DUMP_I2C_READ)
	printk("%s: read addr = 0x", __func__);
	for(i=0;i<REG_LEN;i++)
		printk("%02x",data[i]);
	printk(", read value = 0x%02x \n",*val);
#endif
    return 0;
}

#if 0
ssize_t tcc_ep9553t_cec_read(struct file *flip, char __user *ibuf, size_t count, loff_t *f_pos)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t*)dev_get_drvdata(misc->parent);

	return 0;
}
ssize_t tcc_ep9553t_cec_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t*)dev_get_drvdata(misc->parent);

	return 0;
}
#endif

irqreturn_t tcc_ep9553t_cec_handler(int irq, void *dev)
{
	dprintk("%s\n", __func__);
	return IRQ_WAKE_THREAD;
}

int tcc_ep9553t_get_rx_msg(struct tcc_ep9553t_cec_t *p_cec, struct tcc_cec_msg_rx_t *p)
{
	unsigned long flags;

	spin_lock_irqsave(&p_cec->lock, flags);
	p_cec->rx_data_valid = 0;
	spin_unlock_irqrestore(&p_cec->lock, flags);

	tcc_ep9553t_i2c_read(p_cec->client, REG_EVENT_STATUS_BUFFER_00, (char*)p, sizeof(*p));	

	return 0;
}

irqreturn_t tcc_ep9553t_thread_cec_handler(int irq, void *dev)
{
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t*)dev;
	unsigned char intr_flags;
	unsigned long flags;

	dprintk("%s - intr_flags(0x%x)\n", __func__, intr_flags);
	tcc_ep9553t_i2c_read(p_cec->client, REG_INTERRUPT_FLAGS, &intr_flags, 1);	

	if (intr_flags & INTERRUPT_FLAG_CCF) {
		complete(&p_cec->send_completion);
	} 

	if (intr_flags & INTERRUPT_FLAG_ECF) {
		spin_lock_irqsave(&p_cec->lock, flags);
		p_cec->rx_data_valid = 1;
		spin_unlock_irqrestore(&p_cec->lock, flags);
		wake_up_interruptible(&p_cec->rx_wq);
	}

	return IRQ_HANDLED;
}

unsigned int tcc_ep9553t_cec_poll(struct file *flip, struct poll_table_struct *wait)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t*)dev_get_drvdata(misc->parent);
	
	poll_wait(flip, &p_cec->rx_wq, wait);
	if (p_cec->rx_data_valid == 1)
		return POLL_IN;

	return 0;
}

int tcc_ep9553t_get_collected_osd_name(struct i2c_client *client, struct cec_collected_osd_name *p)
{
	unsigned short addr;

	addr =  (p->logical_addr == CEC_LA_TV) ? REG_COLLECTED_OSD_TV: 
		(p->logical_addr == CEC_LA_TUNER1) ? REG_COLLECTED_OSD_TUNER_1:
		(p->logical_addr == CEC_LA_TUNER2) ? REG_COLLECTED_OSD_TUNER_2:
		(p->logical_addr == CEC_LA_TUNER3) ? REG_COLLECTED_OSD_TUNER_3:
		(p->logical_addr == CEC_LA_TUNER4) ? REG_COLLECTED_OSD_TUNER_4:
		(p->logical_addr == CEC_LA_AUDIO_SYSTEM) ? REG_COLLECTED_OSD_AUDIO:
		(p->logical_addr == CEC_LA_RECODING_DEVICE1) ? REG_COLLECTED_OSD_REC_1:
		(p->logical_addr == CEC_LA_RECODING_DEVICE2) ? REG_COLLECTED_OSD_REC_2:
		(p->logical_addr == CEC_LA_RECODING_DEVICE3) ? REG_COLLECTED_OSD_REC_3:
		(p->logical_addr == CEC_LA_PLAYBACK_DEVICE1) ? REG_COLLECTED_OSD_PLAY_1:
		(p->logical_addr == CEC_LA_PLAYBACK_DEVICE2) ? REG_COLLECTED_OSD_PLAY_2:
		(p->logical_addr == CEC_LA_PLAYBACK_DEVICE3) ? REG_COLLECTED_OSD_PLAY_3: 0;

	if (addr == 0) {
		return -EINVAL;
	}

	tcc_ep9553t_i2c_read(client, addr, &p->osd_name[0], OSD_NAME_MAX);

	return 0;
}

int tcc_ep9553t_get_collected_info(struct i2c_client *client, struct cec_collected_info *p)
{
	unsigned short addr; 
	addr =  (p->logical_addr == CEC_LA_TV) ? REG_COLLECTED_INFO_TV: 
		(p->logical_addr == CEC_LA_TUNER1) ? REG_COLLECTED_INFO_TUNER_1:
		(p->logical_addr == CEC_LA_TUNER2) ? REG_COLLECTED_INFO_TUNER_2:
		(p->logical_addr == CEC_LA_TUNER3) ? REG_COLLECTED_INFO_TUNER_3:
		(p->logical_addr == CEC_LA_TUNER4) ? REG_COLLECTED_INFO_TUNER_4:
		(p->logical_addr == CEC_LA_AUDIO_SYSTEM) ? REG_COLLECTED_INFO_AUDIO:
		(p->logical_addr == CEC_LA_RECODING_DEVICE1) ? REG_COLLECTED_INFO_REC_1:
		(p->logical_addr == CEC_LA_RECODING_DEVICE2) ? REG_COLLECTED_INFO_REC_2:
		(p->logical_addr == CEC_LA_RECODING_DEVICE3) ? REG_COLLECTED_INFO_REC_3:
		(p->logical_addr == CEC_LA_PLAYBACK_DEVICE1) ? REG_COLLECTED_INFO_PLAY_1:
		(p->logical_addr == CEC_LA_PLAYBACK_DEVICE2) ? REG_COLLECTED_INFO_PLAY_2:
		(p->logical_addr == CEC_LA_PLAYBACK_DEVICE3) ? REG_COLLECTED_INFO_PLAY_3:
		(p->logical_addr == CEC_LA_SPECIFIC_USE) ? REG_COLLECTED_INFO_FREE_USE: 0;

	if (addr == 0) {
		return -EINVAL;
	}

	tcc_ep9553t_i2c_read(client, addr, (unsigned char*)&p->info, sizeof(p->info));

	return 0;
}

int tcc_ep9553t_send_command(struct i2c_client *client, char cmd, char *data, int datasize)
{
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t *)i2c_get_clientdata(client);
	unsigned char status;

	tcc_ep9553t_i2c_write(client, REG_COMMAND_PARAMETER_00, data, datasize);
	tcc_ep9553t_i2c_write(client, REG_SEND_COMMAND, &cmd, 1);

	if (!wait_for_completion_io_timeout(&p_cec->send_completion, msecs_to_jiffies(SEND_MESSAGE_TIMEOUT)))  {
		printk("%s timeout\n", __func__);
		return -EIO;
	}

	tcc_ep9553t_i2c_read(client, REG_COMMAND_STATUS, &status, 1);
	
	return status;
}

int _tcc_ep9553t_alloc_logical_addr(struct i2c_client *client, unsigned char *candi_list, int candi_cnt)
{
	int i;
	struct cec_collected_info param;

	for (i=0; i<candi_cnt; i++) {
		param.logical_addr = candi_list[i];
		tcc_ep9553t_get_collected_info(client, &param);

		if (param.info.device_type == 0xff) {
			printk("%s - 0x%x\n", __func__, param.logical_addr);
			return tcc_ep9553t_send_command(client, SEND_CODE_SET_LOGICAL_ADDRESS, (unsigned char*)&param.logical_addr, 1);
		}
	}

	return -1;
}

int tcc_ep9553t_alloc_logical_addr(struct i2c_client *client, enum CEC_DEV_TYPE type)
{
	unsigned char *candi_list;
	int count;
	unsigned char candicate_type_tv[] = {
		CEC_LA_TV, 
		CEC_LA_SPECIFIC_USE
	};
	unsigned char candicate_type_rec[] = {
		CEC_LA_RECODING_DEVICE1, 
		CEC_LA_RECODING_DEVICE2, 
		CEC_LA_RECODING_DEVICE3
	};
	unsigned char candicate_type_play[] = {
		CEC_LA_PLAYBACK_DEVICE1, 
		CEC_LA_PLAYBACK_DEVICE2, 
		CEC_LA_PLAYBACK_DEVICE3
	};
	unsigned char candicate_type_tuner[] = {
		CEC_LA_TUNER1, 
		CEC_LA_TUNER2, 
		CEC_LA_TUNER3, 
		CEC_LA_TUNER4, 
	};
	unsigned char candicate_type_video_processor[] = {
		CEC_LA_SPECIFIC_USE
	};

	switch(type) {
		case DEV_TYPE_TV:
			count = sizeof(candicate_type_tv) / sizeof(unsigned char);
			candi_list = candicate_type_tv;
			break;
		case DEV_TYPE_RECORDING_DEVICE:
			count = sizeof(candicate_type_rec) / sizeof(unsigned char);
			candi_list = candicate_type_rec;
			break;
		case DEV_TYPE_PLAYBACK_DEVICE:
			count = sizeof(candicate_type_play) / sizeof(unsigned char);
			candi_list = candicate_type_play;
			break;
		case DEV_TYPE_TUNER:
			count = sizeof(candicate_type_tuner) / sizeof(unsigned char);
			candi_list = candicate_type_tuner;
			break;
		case DEV_TYPE_VIDEO_PROCESSOR:
			count = sizeof(candicate_type_video_processor) / sizeof(unsigned char);
			candi_list = candicate_type_video_processor;
			break;
		case DEV_TYPE_PURE_CEC_SWITCH:
		default:
			printk("%s - Invalid device type(%d)\n", __func__, type);
			return -EINVAL;
	}

	return _tcc_ep9553t_alloc_logical_addr(client, candi_list, count);
}

long tcc_ep9553t_cec_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t*)dev_get_drvdata(misc->parent);
	struct i2c_client *client = p_cec->client;
	char buffer[16];
	int ret = 0;

	mutex_lock(&p_cec->cec_mutex);
	switch (cmd) {
		case IOCTL_CEC_SET_PHYSICAL_ADDRESS:
			if (p_cec->physical_address != 0x0) {
				ret = -EINVAL;
			}
			else {
				p_cec->physical_address = arg & 0x0000ffff;
				buffer[0] = p_cec->physical_address >> 8;
				buffer[1] = p_cec->physical_address & 0x0ff;
				tcc_ep9553t_i2c_write(client, REG_PHYSICAL_ADDR_HIGH, buffer, 2);
			}
			break;
		case IOCTL_CEC_GET_PHYSICAL_ADDRESS:
			{
				tcc_ep9553t_i2c_read(client, REG_PHYSICAL_ADDR_HIGH, buffer, 2);
				p_cec->physical_address = (buffer[0] << 8) | buffer[1];

				ret = p_cec->physical_address;
			}
			break;
		case IOCTL_CEC_SET_DEVICE_TYPE1:
			{
				p_cec->device_type1 = arg & 0x0ff;
				tcc_ep9553t_i2c_write(client, REG_DEVICE_TYPE_1, &p_cec->device_type1, 1);
			}
			break;
		case IOCTL_CEC_GET_DEVICE_TYPE1:
			{
				tcc_ep9553t_i2c_read(client, REG_DEVICE_TYPE_1, buffer, 1);
				ret = buffer[0];
			}
			break;
		case IOCTL_CEC_SET_DEVICE_TYPE2:
			{
				p_cec->device_type2 = arg & 0x0ff;
				tcc_ep9553t_i2c_write(client, REG_DEVICE_TYPE_2, &p_cec->device_type2, 1);
			}
			break;
		case IOCTL_CEC_GET_DEVICE_TYPE2:
			{
				tcc_ep9553t_i2c_read(client, REG_DEVICE_TYPE_2, buffer, 1);
				ret = buffer[0];
			}
			break;
		case IOCTL_CEC_SET_POWER_STATUS:
			{
				buffer[0] = arg & 0x0ff;
				tcc_ep9553t_i2c_write(client, REG_POWER_STATUS, buffer, 1);
			}
			break;
		case IOCTL_CEC_GET_POWER_STATUS:
			{
				tcc_ep9553t_i2c_read(client, REG_POWER_STATUS, buffer, 1);
				ret = buffer[0];
			}
			break;
		case IOCTL_CEC_SET_LANGUAGE:
			if (p_cec->language != 0x0) {
				ret = -EINVAL;		
			}
			else {
				p_cec->language = arg & 0x00ffffff;
				buffer[0] = (p_cec->language >> 16) & 0x0ff;
				buffer[0] = (p_cec->language >> 8) & 0x0ff;
				buffer[1] = p_cec->language & 0x0ff;
				tcc_ep9553t_i2c_write(client, REG_LANGUAGE_0, buffer, 2);
			}
			break;
		case IOCTL_CEC_GET_LANGUAGE:
			{
				tcc_ep9553t_i2c_read(client, REG_LANGUAGE_0, buffer, 2);
				p_cec->language = (buffer[0] << 8) | (buffer[0] << 8) | buffer[1];

				ret = p_cec->language;
			}
			break;
		case IOCTL_CEC_SET_VENDOR_ID:
			if (p_cec->vendor_id != 0x0) {
				ret = -EINVAL;		
			}
			else {
				p_cec->vendor_id = arg & 0x00ffffff;
				buffer[0] = (p_cec->vendor_id >> 16) & 0x0ff;
				buffer[0] = (p_cec->vendor_id >> 8) & 0x0ff;
				buffer[1] = p_cec->vendor_id & 0x0ff;
				tcc_ep9553t_i2c_write(client, REG_VENDOR_ID_0, buffer, 2);
			}
			break;
		case IOCTL_CEC_GET_VENDOR_ID:
			{
				tcc_ep9553t_i2c_read(client, REG_VENDOR_ID_0, buffer, 2);
				p_cec->vendor_id = (buffer[0] << 8) | (buffer[0] << 8) | buffer[1];

				ret = p_cec->vendor_id;
			}
			break;
		case IOCTL_CEC_SET_OSD_NAME:
			{
				memset(p_cec->osd_name, 0, OSD_NAME_MAX);
				if (copy_from_user((void*)p_cec->osd_name, (const void*)arg, OSD_NAME_MAX)) {
					printk("%s(%d) - invalid args\n", __func__, __LINE__);
					ret = -EINVAL;
					break;
				}
				tcc_ep9553t_i2c_write(client, REG_OSD_NAME_0, p_cec->osd_name, OSD_NAME_MAX);
			}
			break;
		case IOCTL_CEC_GET_OSD_NAME:
			{
				tcc_ep9553t_i2c_read(client, REG_OSD_NAME_0, p_cec->osd_name, OSD_NAME_MAX);

				if (copy_to_user((void*)arg, (const void*)p_cec->osd_name, OSD_NAME_MAX)) {
					printk("%s(%d) - invalid args\n", __func__, __LINE__);
					ret = -EINVAL;
					break;
				}
			}
			break;
		case IOCTL_CEC_GET_COLLECTED_INFO:
			{
				struct cec_collected_info param;

				if (copy_from_user((void*)&param, (const void*)arg, sizeof(param))) {
					printk("%s(%d) - invalid args\n", __func__, __LINE__);
					ret = -EINVAL;
					break;
				}

				tcc_ep9553t_get_collected_info(client, &param);

				if (copy_to_user((void*)arg, (const void*)&param, sizeof(param))) {
					printk("%s(%d) - invalid args\n", __func__, __LINE__);
					ret = -EINVAL;
					break;
				}
			}
			break;
		case IOCTL_CEC_GET_COLLECTED_OSD_NAME:
			{
				struct cec_collected_osd_name param;

				if (copy_from_user((void*)&param, (const void*)arg, sizeof(param))) {
					printk("%s(%d) - invalid args\n", __func__, __LINE__);
					ret = -EINVAL;
					break;
				}
			
				tcc_ep9553t_get_collected_osd_name(client, &param);
				
				if (copy_to_user((void*)arg, (const void*)&param, sizeof(param))) {
					printk("%s(%d) - invalid args\n", __func__, __LINE__);
					ret = -EINVAL;
					break;
				}
			}
			break;
		case IOCTL_CEC_TX_MSG:
			{
				struct tcc_cec_msg_tx_t tx_msg;
				if (copy_from_user((void*)&tx_msg, (const void*)arg, sizeof(tx_msg))) {
					printk("%s(%d) - invalid args\n", __func__, __LINE__);
					ret = -EINVAL;
					break;
				}

				ret = tcc_ep9553t_send_command(client, SEND_CODE_TRANSMIT_CEC_MESSAGE, (unsigned char*)&tx_msg, sizeof(tx_msg));
			}
			break;
		case IOCTL_CEC_RX_MSG:
			{
				struct tcc_cec_msg_rx_t rx;

				if (wait_event_interruptible_timeout(p_cec->rx_wq, p_cec->rx_data_valid, msecs_to_jiffies(RX_MSG_TIMEOUT))) {
					tcc_ep9553t_get_rx_msg(p_cec, &rx);

					if (copy_to_user((void*)arg, (const void*)&rx, sizeof(rx))) {
						printk("%s(%d) - invalid args\n", __func__, __LINE__);
						ret = -EINVAL;
						break;
					}
				} else {
					ret = -ETIME;
				}
			}
			break;
		case IOCTL_CEC_ALLOC_LOGICAL_ADDRESS:
			{
				if (p_cec->device_type1 != 0xff) {
					ret = tcc_ep9553t_alloc_logical_addr(client, p_cec->device_type1);
					if (ret < 0) {
						printk("failed to alloc logical_addr(type1)\n");
					}
					else {
						tcc_ep9553t_i2c_read(p_cec->client, REG_SYSTEM_1, buffer, 1);
						buffer[0] &= ~SYSTEM_1_DEV_SEL;
						tcc_ep9553t_i2c_write(p_cec->client, REG_SYSTEM_1, buffer, 1);
					}
				} else if (p_cec->device_type2 != 0xff || ret < 0) {
					ret = tcc_ep9553t_alloc_logical_addr(client, p_cec->device_type2);
					if (ret < 0) {
						printk("failed to alloc logical_addr(type2)\n");
					}
					else {
						tcc_ep9553t_i2c_read(p_cec->client, REG_SYSTEM_1, buffer, 1);
						buffer[0] |= SYSTEM_1_DEV_SEL;
						tcc_ep9553t_i2c_write(p_cec->client, REG_SYSTEM_1, buffer, 1);
					}
				}
			}
			break;
		default:
			break;
	}
	mutex_unlock(&p_cec->cec_mutex);

	return ret;
}

int tcc_ep9553t_cec_open(struct inode *inode, struct file *flip)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t*)dev_get_drvdata(misc->parent);

	char buffer[2];
	int ret;

	//setup gpios
	gpio_request(p_cec->pwr_port, "ep9553t_cec_pwr");
	gpio_request(p_cec->rst_port, "ep9553t_cec_rst");
	gpio_request(p_cec->irq_port, "ep9553t_cec_int");

	gpio_direction_output(p_cec->pwr_port, 1);
	gpio_direction_output(p_cec->rst_port, 1);
	gpio_direction_input(p_cec->irq_port);


	tcc_ep9553t_i2c_read(p_cec->client, REG_SYSTEM_2, buffer, 1);
	buffer[0] = SYSTEM_2_ECI_EN | SYSTEM_2_CCI_EN;
#ifdef CONFIG_EP9553T_CEC_LISTEN_MODE
	buffer[0] |= SYSTEM_2_LISTEN;
#endif
	tcc_ep9553t_i2c_write(p_cec->client, REG_SYSTEM_2, buffer, 1);

	p_cec->client->irq = gpio_to_irq(p_cec->irq_port);

	tcc_ep9553t_i2c_read(p_cec->client, REG_REVISION_0, buffer, 2);
	printk("EP9553T Firmware Revision : 0x%02x%02x\n", buffer[0], buffer[1]);

	ret = request_threaded_irq(p_cec->client->irq, tcc_ep9553t_cec_handler, tcc_ep9553t_thread_cec_handler, IRQF_SHARED|IRQF_TRIGGER_FALLING, "ep9553t_cec", p_cec);
	if (ret < 0) {
		printk("%s - request_irq failed(%d)\n", __func__, ret);
	}

	return 0;
}

int tcc_ep9553t_cec_release(struct inode *inode, struct file *flip)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t*)dev_get_drvdata(misc->parent);

	//free gpios
	gpio_free(p_cec->pwr_port);
	gpio_free(p_cec->rst_port);
	gpio_free(p_cec->irq_port);

	free_irq(p_cec->client->irq, p_cec);

	return 0;
}

static const struct file_operations tcc_ep9553t_cec_fops =
{
	.owner          = THIS_MODULE,
//	.read			= tcc_ep9553t_cec_read,
//	.write			= tcc_ep9553t_cec_write,
	.poll			= tcc_ep9553t_cec_poll,
	.unlocked_ioctl = tcc_ep9553t_cec_ioctl,
	.open           = tcc_ep9553t_cec_open,
	.release        = tcc_ep9553t_cec_release,
};

static struct miscdevice ep9553t_cec_misc_device =
{
	MISC_DYNAMIC_MINOR,
	"tcc_ep9553t_cec",
	&tcc_ep9553t_cec_fops,
};

int ep9553t_cec_parse_dt(struct tcc_ep9553t_cec_t *p_cec, struct i2c_client *client)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "telechips-hdin");
	if (np) {
		p_cec->pwr_port = of_get_named_gpio(np,"pwr-gpios",0);
		p_cec->rst_port = of_get_named_gpio(np,"rst-gpios",0);
		p_cec->irq_port = of_get_named_gpio(np,"int-gpios",0);
	} else {
		printk(KERN_WARNING "telechip-hdin isn't exist\n");
	}

	return 0;
}

static int ep9553t_cec_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tcc_ep9553t_cec_t *p_cec;

	p_cec = kzalloc(sizeof(struct tcc_ep9553t_cec_t), GFP_KERNEL);
	if (p_cec == NULL) {
		printk(KERN_WARNING "%s - kzalloc failed.\n", __func__);
		return -ENOMEM;
	}

	/*
	 * setup ep9553t cec information
	 */
	p_cec->device_type1 = 0xff;
	p_cec->device_type2 = 0xff;
	p_cec->rx_data_valid = 0;
	p_cec->client = client;

	ep9553t_cec_parse_dt(p_cec, client);

	spin_lock_init(&p_cec->lock);
	mutex_init(&p_cec->cec_mutex);
	init_completion(&p_cec->send_completion);
	init_waitqueue_head(&p_cec->rx_wq);

	i2c_set_clientdata(client, p_cec);
	ep9553t_cec_misc_device.parent = &client->dev;

	if (misc_register(&ep9553t_cec_misc_device)) {
		printk(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}
	return 0;
}

static int ep9553t_cec_remove(struct i2c_client *client)
{
	struct tcc_ep9553t_cec_t *p_cec = (struct tcc_ep9553t_cec_t *)i2c_get_clientdata(client);

	kfree(p_cec);
	return 0;
}

static const struct i2c_device_id ep9553t_cec_id[] = {
	{ "tcc-ep9553t-cec", 0, },
	{ }
};

static struct of_device_id ep9553t_cec_of_match[] = {
	{ .compatible = "telechips-ep9553t-cec" },
	{}
};
MODULE_DEVICE_TABLE(of, ep9553t_cec_of_match);

static struct i2c_driver ep9553t_cec_driver = {
	.driver = {
		.name	= "tcc-ep9553t-cec",
		.of_match_table = of_match_ptr(ep9553t_cec_of_match),			
	},
	.probe		= ep9553t_cec_probe,
	.remove		= ep9553t_cec_remove,
	.id_table	= ep9553t_cec_id,
};

static int __init ep9553t_cec_init(void)
{
	int ret;
	ret = i2c_add_driver(&ep9553t_cec_driver);
	return ret;
}
module_init(ep9553t_cec_init);

static void __exit ep9553t_cec_exit(void)
{
	i2c_del_driver(&ep9553t_cec_driver);
}
module_exit(ep9553t_cec_exit);
