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
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#include <video/tcc/tcc_cam_ioctrl.h>

#include "videosource_if.h"
#include "videodecoder/videodecoder.h"

#include <linux/clk.h>
#include "mipi-csi2/mipi-csi2.h"

static int					debug = 0;
#define TAG					"videosource_if"
#define log(msg, arg...)	do { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } while(0)
#define dlog(msg, arg...)	do { if(debug) { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } } while(0)
#define FUNCTION_IN			dlog("IN\n");
#define FUNCTION_OUT		dlog("OUT\n");

TCC_SENSOR_INFO_TYPE videosource_info;
SENSOR_FUNC_TYPE videosource_func;

volatile void __iomem *  ddicfg_base;
#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
static struct clk * 	 mipi_csi2_clk;
static unsigned int 	 mipi_csi2_frequency;
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

//extern irqreturn_t ds90ub964_irq_thread_handler(int irq, void * client_data);

/* list of image formats supported by sensor sensor */
const static struct v4l2_fmtdesc videosource_format[] = {
	{
		.description = "RGB565, le",
		.pixelformat = V4L2_PIX_FMT_RGB565,
	},
	{
		.description = "RGB565, be",
		.pixelformat = V4L2_PIX_FMT_RGB565X,
	},
	{
		.description = "RGB888, packed",
		.pixelformat = V4L2_PIX_FMT_RGB24,
	},
	{
		.description = "YUYV (YUV 4:2:2), packed",
		.pixelformat = V4L2_PIX_FMT_YUYV,
	},
	{
		.description = "UYVY, packed",
		.pixelformat = V4L2_PIX_FMT_UYVY,
	},
	{
		.description = "RGB555, le",
		.pixelformat = V4L2_PIX_FMT_RGB555,
	},
	{
		.description = "RGB555, be",
		.pixelformat = V4L2_PIX_FMT_RGB555X,
	}
};
#define NUM_CAPTURE_FORMATS ARRAY_SIZE(videosource_format)

unsigned int sensor_control[] = {
	V4L2_CID_BRIGHTNESS,
	V4L2_CID_AUTO_WHITE_BALANCE,
	V4L2_CID_ISO,
	V4L2_CID_EFFECT,
	V4L2_CID_FLIP,
	V4L2_CID_SCENE,
	V4L2_CID_METERING_EXPOSURE,
	V4L2_CID_EXPOSURE,
	V4L2_CID_FOCUS_MODE,
	V4L2_CID_FLASH
};

#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
static irqreturn_t videosource_if_mipi_csi2_isr(int irq, void * client_data) {
    unsigned int intr_status0 = 0;
    unsigned int intr_status1 = 0;

    intr_status0 = MIPI_CSIS_Get_CSIS_Interrupt_Src(0);
    intr_status1 = MIPI_CSIS_Get_CSIS_Interrupt_Src(1);

    printk("%s interrupt status 0x%x / 0x%x \n", __func__, intr_status0, intr_status1);

    // clear interrupt
    MIPI_CSIS_Set_CSIS_Interrupt_Src(0, intr_status0);
    MIPI_CSIS_Set_CSIS_Interrupt_Src(0, intr_status1);    

	return IRQ_HANDLED;
//    return IRQ_WAKE_THREAD;
}
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

int videosource_if_enum_pixformat(struct v4l2_fmtdesc * fmt) {
	int					index	= fmt->index;
	enum v4l2_buf_type	type	= fmt->type;

	memset(fmt, 0, sizeof(* fmt));
	fmt->index			= index;
	fmt->type			= type;
	switch(fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if(NUM_CAPTURE_FORMATS <= index)
			return -EINVAL;
		break;

	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		if(NUM_OVERLAY_FORMATS <= index)
			return -EINVAL;
		break;

	default:
		return -EINVAL;
	}
	fmt->flags			= videosource_format[index].flags;
	strlcpy(fmt->description, videosource_format[index].description, sizeof(fmt->description));
	fmt->pixelformat	= videosource_format[index].pixelformat;

	return 0;
}

#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
int videosource_if_parse_mipi_csi2_dt_data(struct device_node * dev_node) {
	struct device_node * mipi_csis_node = NULL;
	struct device_node * ddicfg_node	= NULL;

	mipi_csis_node = of_find_compatible_node(NULL, NULL, "telechips,mipi_csi2");
//		mipi_csis_node = of_parse_phandle(videosource_node, "mipi_csi2", 0);

	if(mipi_csis_node) {
		// Configure the MIPI clock
		mipi_csi2_clk = of_clk_get(mipi_csis_node, 0);

		if(IS_ERR(mipi_csi2_clk)) {
			log("failed to get mipi_csi2_clk\n");
			return -ENODEV;
		}
		else {
			of_property_read_u32(mipi_csis_node, "clock-frequency", &mipi_csi2_frequency);
			if(mipi_csi2_frequency == 0) {
				log("failed to get mipi_csi2_frequency\n");
				return -ENODEV;
			}
			else {
				clk_set_rate(mipi_csi2_clk, mipi_csi2_frequency);
				clk_prepare_enable(mipi_csi2_clk);
			}
		}

		videosource_info.des_info.csi2_irq = irq_of_parse_and_map(mipi_csis_node, 0);
		videosource_info.des_info.gdb_irq = irq_of_parse_and_map(mipi_csis_node,1);

		log("%s mipi clock : %d Hz\n", __func__, mipi_csi2_frequency);
		log("csi2 irq num : %d, Generic data buffer irq num : %d \n", \
				videosource_info.des_info.csi2_irq, videosource_info.des_info.gdb_irq);
	}
	else {
		log("Fail mipi_csi2_node \n");
		return -ENODEV;
	}

	ddicfg_node = of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
	if (ddicfg_node == NULL) {
		log("cann't find DDI Config node \n");
		return -ENODEV;
	}
	else {
		ddicfg_base = (volatile void __iomem *)of_iomap(ddicfg_node, 0);
		log("%s addr :%p \n", __func__, ddicfg_base);
	}

	return 0;
}
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

int videosource_if_parse_gpio_dt_data(struct device_node * dev_node) {
	struct device_node * videosource_node = NULL;

	videosource_node = of_find_node_by_name(dev_node, MODULE_NODE);
	if(videosource_node) {
		videosource_info.gpio.pwr_port = of_get_named_gpio_flags(videosource_node, "pwr-gpios", 0, &videosource_info.gpio.pwr_value);
		videosource_info.gpio.pwd_port = of_get_named_gpio_flags(videosource_node, "pwd-gpios", 0, &videosource_info.gpio.pwd_value);
		videosource_info.gpio.rst_port = of_get_named_gpio_flags(videosource_node, "rst-gpios", 0, &videosource_info.gpio.rst_value);
		videosource_info.gpio.intb_port = of_get_named_gpio_flags(videosource_node, "intb-gpios", 0, &videosource_info.gpio.intb_value);

		if(0 < videosource_info.gpio.pwr_port) {
			log("pwr: port = %3d, curr val = %d, set val = %d\n",	\
				videosource_info.gpio.pwr_port, gpio_get_value(videosource_info.gpio.pwr_port), videosource_info.gpio.pwr_value);
			gpio_request(videosource_info.gpio.pwr_port, "camera power");
			gpio_direction_output(videosource_info.gpio.pwr_port, videosource_info.gpio.pwr_value);
		}
		if(0 < videosource_info.gpio.pwd_port) {
			log("pwd: port = %3d, curr val = %d, set val = %d\n",	\
				videosource_info.gpio.pwd_port, gpio_get_value(videosource_info.gpio.pwd_port), videosource_info.gpio.pwd_value);
			gpio_request(videosource_info.gpio.pwd_port, "camera power down");
			gpio_direction_output(videosource_info.gpio.pwd_port, videosource_info.gpio.pwd_value);
		}
		if(0 < videosource_info.gpio.rst_port) {
			log("rst: port = %3d, curr val = %d, set val = %d\n",	\
				videosource_info.gpio.rst_port, gpio_get_value(videosource_info.gpio.rst_port), videosource_info.gpio.rst_value);
			gpio_request(videosource_info.gpio.rst_port, "camera reset");
			gpio_direction_output(videosource_info.gpio.rst_port, videosource_info.gpio.rst_value);
		}
		if(0 < videosource_info.gpio.intb_port) {
			log("intb: port = %3d, curr val = %d \n",	\
				videosource_info.gpio.intb_port, gpio_get_value(videosource_info.gpio.intb_port));
			gpio_request(videosource_info.gpio.intb_port, "camera interrupt");
			gpio_direction_input(videosource_info.gpio.intb_port);
		}

	} else {
		printk("could not find sensor module node!! \n");
		return -ENODEV;
	}

	return 0;
}

#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
int videosource_if_init_mipi_csi2_interface(TCC_SENSOR_INFO_TYPE * sensor_info, unsigned int onOff) {
	unsigned int idx = 0;

	if(onOff) {
		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(ddicfg_base, 0);

		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(ddicfg_base, 0);

		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(1);

		/*
		 * Set D-PHY control(Master, Slave)
		 * Refer to 7.2.3(B_DPHYCTL) in D-PHY datasheet
		 * 500 means ULPS EXIT counter value.
		 */
		MIPI_CSIS_Set_DPHY_B_Control(0x00000000, 0x000001f4);

		/*
		 * Set D-PHY control(Slave)
		 * Refer to 7.2.5(S_DPHYCTL) in D-PHY datasheet
		 */
		MIPI_CSIS_Set_DPHY_S_Control(0x00000000, 0xfd008000);

		/*
		 * Set D-PHY Common control
		 */
		log("hssettle value : 0x%x \n", sensor_info->des_info.hssettle);
		MIPI_CSIS_Set_DPHY_Common_Control(sensor_info->des_info.hssettle, \
											sensor_info->des_info.clksettlectl, \
											ON, \
											OFF, \
											OFF, \
											sensor_info->des_info.data_lane_num, \
											ON);


		for(idx = 0; idx < sensor_info->des_info.input_ch_num ; idx++) {
			MIPI_CSIS_Set_ISP_Configuration(idx, \
											sensor_info->des_info.pixel_mode, \
											OFF, \
											OFF, \
											sensor_info->des_info.data_format, \
											idx);
			MIPI_CSIS_Set_ISP_Resolution(idx, sensor_info->width , sensor_info->height);
		}

		MIPI_CSIS_Set_CSIS_Clock_Control(0x0, 0xf);

		MIPI_CSIS_Set_CSIS_Common_Control(sensor_info->des_info.input_ch_num, \
											0x0, \
											ON, \
											sensor_info->des_info.interleave_mode, \
											sensor_info->des_info.data_lane_num, \
											OFF, \
											OFF, \
											ON);
	}
	else {
		MIPI_CSIS_Set_Enable(OFF);

		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(1);

		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(ddicfg_base, 1);

		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(ddicfg_base, 1);
	}

	return 0;
}

int videosource_if_set_mipi_csi2_interrupt(TCC_SENSOR_INFO_TYPE * sensor_info, unsigned int onOff) {
	if(onOff) {
		// clear interrupt
		MIPI_CSIS_Set_CSIS_Interrupt_Src(0, \
                CIM_MSK_FrameStart_MASK | CIM_MSK_FrameEnd_MASK | CIM_MSK_ERR_SOT_HS_MASK | \
                CIM_MSK_ERR_LOST_FS_MASK | CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK | \
                CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK | CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK);

		MIPI_CSIS_Set_CSIS_Interrupt_Src(1, \
						CIM_MSK_LINE_END_MASK);

//		  if(request_threaded_irq(videosource_info.des_info.csi2_irq, videosource_if_mipi_csi2_isr, ds90ub964_irq_thread_handler, 0, "mipi_csi2", NULL)) {
//		  if(request_irq(videosource_info.des_info.csi2_irq, videosource_if_mipi_csi2_isr, IRQF_SHARED, "mipi_csi2", "mipi_csi2")) {
        if(request_irq(videosource_info.des_info.csi2_irq, videosource_if_mipi_csi2_isr, 0, "mipi_csi2", NULL)) {
			printk("fail request irq(%d) \n", videosource_info.des_info.csi2_irq);
		}
		else {
			// unmask interrupt
			MIPI_CSIS_Set_CSIS_Interrupt_Mask(0, \
                    CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK | CIM_MSK_ERR_LOST_FE_MASK | \
                    CIM_MSK_ERR_OVER_MASK | CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK | \
						CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK, \
						0);

			MIPI_CSIS_Set_CSIS_Interrupt_Mask(1, \
						CIM_MSK_LINE_END_MASK, \
						0);
		}
	}
	else {
//		  free_irq(videosource_info.des_info.csi2_irq, "mipi_csi2");
		free_irq(videosource_info.des_info.csi2_irq, NULL);
	}

	return 0;
}
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

/* Initialize the sensor.
 * This routine allocates and initializes the data structure for the sensor,
 * powers up the sensor, registers the I2C driver, and sets a default image
 * capture format in pix.  The capture format is not actually programmed
 * into the sensor by this routine.
 * This function must return a non-NULL value to indicate that
 * initialization is successful.
 */
int videosource_if_init(void) {
#if 0
	/* Make the default capture format QVGA YUV422 */
	videosource_info.width = videosource_info.sensor_sizes[0].width;
	videosource_info.height = videosource_info.sensor_sizes[0].height;
	videosource_info.data_format = V4L2_PIX_FMT_YUYV;
	sensor_try_format();
#endif

	return 0;
}

void videosource_if_set(int index) {
	sensor_info_init(&videosource_info);
}

void videosource_if_api_connect(int index) {
	sensor_init_fnc(&videosource_func);
}

void sensor_delay(int ms) {
	unsigned int msec = ms / 10; //10msec unit

	if(!msec)	msleep(1);
	else		msleep(msec);
}

void sensor_port_enable(int port) {
	if(0 < port) gpio_set_value_cansleep(port, ON);
}

void sensor_port_disable(int port) {
	if(0 < port) gpio_set_value_cansleep(port, OFF);
}

int sensor_get_port(int port) {
	if(port < 0) return -1;
	return gpio_get_value(port);
}

int videosource_if_set_port(struct device * dev, int enable) {
	struct pinctrl		* pinctrl	= NULL;
	int					ret			= 0;

	FUNCTION_IN

	// pinctrl
	pinctrl = pinctrl_get_select(dev, (enable == ENABLE) ? "active" : "idle");
	if(IS_ERR(pinctrl)) {
		pinctrl_put(pinctrl);
		printk(KERN_ERR "%s: pinctrl select failed\n", __func__);
		return -1;
	}

	FUNCTION_OUT
	return ret;
}

int videosource_if_change_mode(int mode) {
	int		ret	= 0;

	dlog("mode: 0x%08x\n", mode);
	if(videosource_func.change_mode == NULL) {
		log("The function to change the mode is NULL\n");
		return -1;
	} else {
		ret = videosource_func.change_mode(mode);
	}

	return ret;
}

int enabled;

int videosource_open(void) {
	int	ret = 0;

	FUNCTION_IN

	if(enabled == DISABLE) {
		videosource_func.open();
		videosource_if_change_mode(MODE_INIT);

#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
        if(!(strncmp(MODULE_NODE, "des_", 4))) {            
            // init remote serializer
            videosource_if_change_mode(MODE_SERDES_REMOTE_SER);

            // enable deserializer frame sync
            videosource_if_change_mode(MODE_SERDES_FSYNC);

#if 0
            // allocate deserializer interrupt 
            videosource_func.set_irq_handler(&videosource_info, ON);
            
            // enable deserializer interrupt
    		videosource_if_change_mode(MODE_SERDES_INTERRUPT);
#endif

            // set mipi-csi2 interface
            videosource_if_init_mipi_csi2_interface(&videosource_info, ON);

            // enable mipi-csi2 interrupt
            videosource_if_set_mipi_csi2_interrupt(&videosource_info, ON);
        }
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

		enabled = ENABLE;
	} else {
		ret = -1;
	}

	FUNCTION_OUT
	return ret;
}

int videosource_close(void) {
	int	ret	= 0;

	FUNCTION_IN

	if(enabled == ENABLE) {
		videosource_func.close();

#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
		if(!(strncmp(MODULE_NODE, "des_", 4))) {
			videosource_if_init_mipi_csi2_interface(&videosource_info, OFF);

			// disable mipi-csi2 interrupt
			videosource_if_set_mipi_csi2_interrupt(&videosource_info, OFF);

#if 0
			// disable deserializer interrupt
			videosource_func.set_irq_handler(&videosource_info, OFF);
#endif
		}
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

		enabled = DISABLE;
	} else {
		ret = -1;
	}

	FUNCTION_OUT
	return ret;
}

int videosource_if_open(struct inode * inode, struct file * file) {
	return videosource_open();
}

int videosource_if_release(struct inode * inode, struct file * file) {
	return videosource_close();
}

long videosource_if_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
//	tccvin_dev_t * vdev = video_drvdata(file);
	int ret = 0;

	dlog("cmd: 0x%08x\n", cmd);
	switch(cmd) {
	default:
		log("The ioctl command(0x%x) is WRONG.\n", cmd);
		return -EINVAL;
	}

	return ret;
}

struct file_operations videosource_if_fops = {
	.owner			= THIS_MODULE,
	.open			= videosource_if_open,
	.release		= videosource_if_release,
	.unlocked_ioctl = videosource_if_ioctl,
};

#define VIDEOSOURCE_IF_MODULE_NAME	"videosource"
#define VIDEOSOURCE_IF_DEV_MAJOR	227
#define VIDEOSOURCE_IF_DEV_MINOR	0

static struct class * videosource_if_class;

int videosource_if_probe(struct platform_device * pdev) {
	struct device		* dev	= &pdev->dev;
	int					ret		= 0;

	FUNCTION_IN

	videosource_if_set(0/* id */);

	if((ret = videosource_if_parse_gpio_dt_data(dev->of_node)) < 0) {
		printk(KERN_ERR "ERROR: cannot initialize gpio port\n");
		return ret;
	}

#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
	if(!(strncmp(MODULE_NODE, "des_", 4))) {
		if((ret = videosource_if_parse_mipi_csi2_dt_data(dev->of_node)) < 0) {
			printk(KERN_ERR "ERROR: videosource_if_parse_mipi_csi2_dt_data\n");
			return ret;
		}
	}
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

	videosource_if_api_connect(0);

	if((ret = videosource_if_init() < 0)) {
		printk(KERN_ERR "ERROR: cannot initialize sensor\n");
		return ret;
	}

	// open port
	videosource_if_set_port(dev, ENABLE);

	if((ret = register_chrdev(VIDEOSOURCE_IF_DEV_MAJOR, VIDEOSOURCE_IF_MODULE_NAME, &videosource_if_fops)) < 0) {
		printk(KERN_ERR "ERROR: cannot register the videosource_if driver\n");
		return ret;
	}

	videosource_if_class = class_create(THIS_MODULE, VIDEOSOURCE_IF_MODULE_NAME);
	if(NULL == device_create(videosource_if_class, NULL, MKDEV(VIDEOSOURCE_IF_DEV_MAJOR, VIDEOSOURCE_IF_DEV_MINOR), NULL, VIDEOSOURCE_IF_MODULE_NAME)) {
		printk(KERN_INFO "%s device_create failed\n", __FUNCTION__);
		return ret;
	}

	FUNCTION_OUT
	return ret;
}

int videosource_if_remove(struct platform_device * pdev) {
	struct device		* dev	= &pdev->dev;

	FUNCTION_IN

	// close port
	videosource_if_set_port(dev, DISABLE);

	// free port gpios
	if(0 < videosource_info.gpio.pwr_port) gpio_free(videosource_info.gpio.pwr_port);
	if(0 < videosource_info.gpio.pwd_port) gpio_free(videosource_info.gpio.pwd_port);
	if(0 < videosource_info.gpio.rst_port) gpio_free(videosource_info.gpio.rst_port);

#if defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)
	if(!(strncmp(MODULE_NODE, "des_", 4))) {
		clk_disable(mipi_csi2_clk);
	}
#endif//defined(VIDEO_VIDEOSOURCE_VIDEODECODER_DS90UB964)

	// unregister the videosource_if device as a char device.
	device_destroy(videosource_if_class, MKDEV(VIDEOSOURCE_IF_DEV_MAJOR, VIDEOSOURCE_IF_DEV_MINOR));
	class_destroy(videosource_if_class);
	unregister_chrdev(VIDEOSOURCE_IF_DEV_MAJOR, VIDEOSOURCE_IF_MODULE_NAME);

	FUNCTION_OUT
	return 0;
}

static struct of_device_id videosource_if_of_match[] = {
	{ .compatible = "telechips,videosource" },
};
MODULE_DEVICE_TABLE(of, videosource_if_of_match);

static struct platform_driver videosource_if_driver = {
	.probe		= videosource_if_probe,
	.remove		= videosource_if_remove,
	.driver		= {
		.name			= VIDEOSOURCE_IF_MODULE_NAME,
		.owner			= THIS_MODULE,
		.of_match_table	= of_match_ptr(videosource_if_of_match)
	},
};
module_platform_driver(videosource_if_driver);

MODULE_DESCRIPTION("Telechips Video-Source Interface Driver");
MODULE_VERSION("v1.0");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips.Co.Ltd");

