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

#include <linux/delay.h> 
#include <linux/module.h>

#include <linux/gpio.h>

#include "tcc_hdin_ctrl.h"
#include "tcc_hdin_video.h"
#include "tcc_hdin_main.h"

static int debug	   = 0;

#define LOG_MODULE_NAME "HDMI"

#define logl(level, fmt, ...) printk(level "[%s][%s] %s - " pr_fmt(fmt), #level + 5, LOG_MODULE_NAME, __FUNCTION__, ##__VA_ARGS__)
#define log(fmt, ...) logl(KERN_INFO, fmt, ##__VA_ARGS__)
#define loge(fmt, ...) logl(KERN_ERR, fmt, ##__VA_ARGS__)
#define logw(fmt, ...) logl(KERN_WARNING, fmt, ##__VA_ARGS__)
#define logd(fmt, ...) if (debug) logl(KERN_DEBUG, fmt, ##__VA_ARGS__)

int hdin_ctrl_get_resolution(struct file *file)
{
	struct tcc_hdin_device *dev = video_drvdata(file);
	unsigned int ret = 0x00;

	ret = dev->func.GetVideoSize(file);
		
	return ret;
}

int hdin_ctrl_get_fps(int *nFrameRate)
{
	*nFrameRate = SENSOR_FRAMERATE;

	if(*nFrameRate)
	{
		return 0;
	}
	else
	{
		log("Sensor Driver dosen't have frame rate information!!\n");
		return -1;
	}
}

int hdin_ctrl_get_audio_samplerate(struct tcc_hdin_device *vdev)
{
	struct tcc_hdin_device *dev = vdev;
	return dev->func.GetAudioSR();
}

int hdin_ctrl_get_audio_type(struct tcc_hdin_device *vdev)
{
	struct tcc_hdin_device *dev = vdev;	
	return dev->func.GetAudioType();
}

int hdin_ctrl_get_audio_output_signal(struct tcc_hdin_device *vdev)
{
#if 0
	struct tcc_hdin_device *dev = 	vdev;

	if(dev->hdmi_spdif_op_addr == 0)
	{
		return true;
	}
	else
	{
		
		if((*dev->hdmi_spdif_op_addr & dev->hdmi_spdif_op_value) == dev->hdmi_spdif_op_value)
		{
			return true;
		}
		else
		{
			log("hdin audio is not operation....\n");
			return false;
		}
	}
#endif
return true;
}

void hdin_ctrl_get_gpio(struct tcc_hdin_device *vdev)
{
	struct tcc_hdin_device *dev = vdev;
	struct hdin_gpio *gpio = &dev->gpio;
	struct device_node *hdin_np = dev->np;
	struct device_node *module_np;

	
	module_np = of_find_node_by_name(hdin_np, MODULE_NODE);
	
	if(module_np)
	{
		gpio->pwr_port	= of_get_named_gpio(module_np,"pwr-gpios",0);
		gpio->key_port	= of_get_named_gpio(module_np,"key-gpios",0);
		gpio->rst_port	= of_get_named_gpio(module_np,"rst-gpios",0);
		gpio->int_port	= of_get_named_gpio(module_np,"int-gpios",0);		
		
		gpio_request(gpio->pwr_port, "HDIN_PWR");
		gpio_direction_output(gpio->pwr_port, 0);

		gpio_request(gpio->rst_port, "HDIN_RST");
		gpio_direction_output(gpio->rst_port, 0);

		gpio_request(gpio->key_port, "HDIN_KEY");
		gpio_direction_output(gpio->key_port, 0);

		gpio_request(gpio->int_port, "HDIN_INT");
		gpio_direction_input(gpio->int_port);


		hdin_ctrl_port_enable(gpio->pwr_port);
		hdin_ctrl_port_enable(gpio->rst_port);

	}
}

void hdin_ctrl_port_enable(int port)
{
	gpio_set_value(port,1);
}

void hdin_ctrl_port_disable(int port)
{
	gpio_set_value(port,0);
}


int hdin_ctrl_init(struct file *file)
{
	struct tcc_hdin_device *dev = video_drvdata(file);
	struct hdin_gpio *gpio = &dev->gpio;
	
	logd("interrupt pin value = %d \n",gpio_get_value(gpio->int_port));

	hdin_ctrl_port_disable(gpio->key_port);
	hdin_ctrl_port_enable(gpio->key_port);

	module_init_fnc(&dev->func);

	if(!dev->hdin_enabled)
	{
		dev->hdin_enabled = ON;
	}

	if(dev->func.Open(file) < 0)
	{
		return -1;
	}

	return 0;

}

void hdin_ctrl_cleanup(struct file *file)
{

	struct tcc_hdin_device *dev = video_drvdata(file);
	logd("hdin_enabled = [%d]\n", dev->hdin_enabled);
	
	if(dev->hdin_enabled)
	{
		dev->func.Close(file);
		dev->hdin_enabled = OFF;
	}
	
    return;
}

void hdin_ctrl_delay(int ms)
{
	unsigned int msec;

	msec = ms / 10; //10msec unit

	if(!msec)	msleep(1);
	else		msleep(msec);
	
}

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_ALIAS(DRIVER_NAME);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);

