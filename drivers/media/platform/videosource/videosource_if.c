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

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>

#include <video/tcc/vioc_ddicfg.h>

#include <linux/uaccess.h>

#include "videosource_common.h"
#include "videosource_if.h"
#ifdef CONFIG_ARCH_TCC803X
#include "mipi-csi2/mipi-csi2.h"
#endif//CONFIG_ARCH_TCC803X

#include "../../../pinctrl/core.h"
#include "../../../pinctrl/tcc/pinctrl-tcc.h"

#define NUM_OVERLAY_FORMATS 2

struct tcc_pinctrl {
	struct device *dev;

	void __iomem *base;

	struct pinctrl_desc pinctrl_desc;

	struct pinctrl_dev *pctldev;

	struct tcc_pinconf *pin_configs;
	int nconfigs;

	struct tcc_pinctrl_ops *ops;

	struct pinctrl_pin_desc *pins;
	unsigned int npins;

	struct tcc_pin_bank *pin_banks;
	unsigned int nbanks;

	struct tcc_pin_group *groups;
	unsigned int ngroups;

	struct tcc_pinmux_function *functions;
	unsigned int nfunctions;
};

#ifdef CONFIG_ARCH_TCC803X
volatile void __iomem *  ddicfg_base;
struct clk * 	 mipi_csi2_clk;
unsigned int 	 mipi_csi2_frequency;
#endif//CONFIG_ARCH_TCC803X

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

#ifdef CONFIG_ARCH_TCC803X
static irqreturn_t videosource_if_mipi_csi2_isr(int irq, void * client_data) {
	unsigned int intr_status0 = 0, intr_status1 = 0, intr_mask0 = 0, intr_mask1 = 0;
	unsigned int idx = 0;

	intr_status0 = MIPI_CSIS_Get_CSIS_Interrupt_Src(0);
	intr_status1 = MIPI_CSIS_Get_CSIS_Interrupt_Src(1);

	intr_mask0 = MIPI_CSIS_Get_CSIS_Interrupt_Mask(0);
	intr_mask1 = MIPI_CSIS_Get_CSIS_Interrupt_Mask(1);

	log("interrupt status 0x%x / 0x%x \n", intr_status0, intr_status1);

	intr_status0 &= intr_mask0;
	intr_status1 &= intr_mask1;

	/* interruptsource register 0 */
	if(intr_status0 & CIM_MSK_FrameStart_MASK) {
		for(idx = 0; idx<4; idx++) {
			if(intr_status0 & ((1 << idx) << CIM_MSK_FrameStart_SHIFT))
				log("[CH%d] FrameStart packet is received \n", idx);
		}
	}
	if(intr_status0 & CIM_MSK_FrameEnd_MASK) {
		for(idx = 0; idx<4; idx++) {
			if(intr_status0 & ((1 << idx) << CIM_MSK_FrameEnd_SHIFT))
				log("[CH%d] FrameEnd packet is received \n", idx);
		}
	}
	if(intr_status0 & CIM_MSK_ERR_SOT_HS_MASK) {
		for(idx = 0; idx<4; idx++) {
			if(intr_status0 & ((1 << idx) << CIM_MSK_ERR_SOT_HS_SHIFT) )
				log("[Lane%d] Start of transmission error \n", idx);
		}
	}
	if(intr_status0 & CIM_MSK_ERR_LOST_FS_MASK) {
		for(idx = 0; idx<4; idx++) {
			if(intr_status0 & ((1 << idx) << CIM_MSK_ERR_LOST_FS_SHIFT))
				log("[CH%d] Lost of Frame Start packet \n", idx);
		}
	}
	if(intr_status0 & CIM_MSK_ERR_LOST_FE_MASK) {
		for(idx = 0; idx<4; idx++) {
			if(intr_status0 & ((1 << idx) << CIM_MSK_ERR_LOST_FE_SHIFT))
				log("[CH%d] Lost of Frame End packet \n", idx);
		}
	}
	if(intr_status0 & CIM_MSK_ERR_OVER_MASK) {
		loge("Image FIFO overflow interrupt \n");
	}
	if(intr_status0 & CIM_MSK_ERR_WRONG_CFG_MASK) {
		loge("Wrong configuration \n");
	}
	if(intr_status0 & CIM_MSK_ERR_ECC_MASK) {
		loge("ECC error \n");
	}
	if(intr_status0 & CIM_MSK_ERR_CRC_MASK) {
		loge("CRC error \n");
	}
	if(intr_status0 & CIM_MSK_ERR_ID_MASK) {
		loge("Unknown ID error \n");
	}

	/* interruptsource register 1 */
	if(intr_status1 & CIM_MSK_LINE_END_MASK) {
		for(idx = 0; idx<4; idx++) {
			if(intr_status1 & ((1 << idx) << CIM_MSK_LINE_END_SHIFT))
				log("[CH%d] End of specific line \n", idx);
		}
	}

	// clear interrupt
	MIPI_CSIS_Set_CSIS_Interrupt_Src(0, intr_status0);
	MIPI_CSIS_Set_CSIS_Interrupt_Src(1, intr_status1);

	return IRQ_HANDLED;
//	  return IRQ_WAKE_THREAD;
}
#endif//CONFIG_ARCH_TCC803X

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

int videosource_parse_gpio_dt_data(videosource_t * vdev, struct device_node * videosource_node) {
	struct device_node	* node	= NULL;
	int					ret		= 0;

	if(videosource_node) {
		// get cif port
		of_property_read_u32_index(videosource_node, "cifport", 0, &vdev->format.cif_port);
		log("cif port: %d\n", vdev->format.cif_port);

		vdev->gpio.pwr_port = of_get_named_gpio_flags(videosource_node, "pwr-gpios", 0, &vdev->gpio.pwr_value);
		vdev->gpio.pwd_port = of_get_named_gpio_flags(videosource_node, "pwd-gpios", 0, &vdev->gpio.pwd_value);
		vdev->gpio.rst_port = of_get_named_gpio_flags(videosource_node, "rst-gpios", 0, &vdev->gpio.rst_value);

#ifdef CONFIG_ARCH_TCC803X
		if(vdev->type == VIDEOSOURCE_TYPE_MIPI) {
			// interrupt pin
			vdev->gpio.intb_port = of_get_named_gpio_flags(videosource_node, "intb-gpios", 0, &vdev->gpio.intb_value);

			// mipi-csi2
			node = of_find_compatible_node(NULL, NULL, "telechips,mipi_csi2");
			if(node) {
				// Configure the MIPI clock
				mipi_csi2_clk = of_clk_get(node, 0);
				if(IS_ERR(mipi_csi2_clk)) {
					loge("failed to get mipi_csi2_clk\n");
					return -ENODEV;
				} else {
					of_property_read_u32(node, "clock-frequency", &mipi_csi2_frequency);
					if(mipi_csi2_frequency == 0) {
						loge("failed to get mipi_csi2_frequency\n");
						return -ENODEV;
					} else {
						clk_set_rate(mipi_csi2_clk, mipi_csi2_frequency);
						clk_prepare_enable(mipi_csi2_clk);
					}
				}

				vdev->format.des_info.csi2_irq = irq_of_parse_and_map(node, 0);
				vdev->format.des_info.gdb_irq = irq_of_parse_and_map(node, 1);

				log("%s mipi clock : %d Hz\n", __func__, mipi_csi2_frequency);
				log("csi2 irq num : %d, Generic data buffer irq num : %d\n", \
					vdev->format.des_info.csi2_irq, vdev->format.des_info.gdb_irq);
			} else {
				loge("Fail mipi_csi2_node\n");
				return -ENODEV;
			}

			// ddi config
			node = of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
			if (node == NULL) {
				loge("cann't find DDI Config node \n");
				return -ENODEV;
			} else {
				ddicfg_base = (volatile void __iomem *)of_iomap(node, 0);
				log("ddicfg addr: %p\n", ddicfg_base);
			}
		}
#endif//CONFIG_ARCH_TCC803X
	} else {
		loge("could not find sensor module node!! \n");
		return -ENODEV;
	}

	return ret;
}

int videosource_request_gpio(videosource_t * vdev) {
	FUNCTION_IN

	if(0 < vdev->gpio.pwr_port) {
		log("pwr: port = %3d, curr val = %d, set val = %d\n",	\
			vdev->gpio.pwr_port, gpio_get_value(vdev->gpio.pwr_port), vdev->gpio.pwr_value);
		gpio_request(vdev->gpio.pwr_port, "camera power");
		gpio_direction_output(vdev->gpio.pwr_port, vdev->gpio.pwr_value);
	}
	if(0 < vdev->gpio.pwd_port) {
		log("pwd: port = %3d, curr val = %d, set val = %d\n",	\
			vdev->gpio.pwd_port, gpio_get_value(vdev->gpio.pwd_port), vdev->gpio.pwd_value);
		gpio_request(vdev->gpio.pwd_port, "camera power down");
		gpio_direction_output(vdev->gpio.pwd_port, vdev->gpio.pwd_value);
	}
	if(0 < vdev->gpio.rst_port) {
		log("rst: port = %3d, curr val = %d, set val = %d\n",	\
			vdev->gpio.rst_port, gpio_get_value(vdev->gpio.rst_port), vdev->gpio.rst_value);
		gpio_request(vdev->gpio.rst_port, "camera reset");
		gpio_direction_output(vdev->gpio.rst_port, vdev->gpio.rst_value);
	}
#ifdef CONFIG_ARCH_TCC803X
	if(0 < vdev->gpio.intb_port) {
		log("intb: port = %3d, curr val = %d \n",	\
			vdev->gpio.intb_port, gpio_get_value(vdev->gpio.intb_port));
		gpio_request(vdev->gpio.intb_port, "camera interrupt");
		gpio_direction_input(vdev->gpio.intb_port);
	}
#endif//CONFIG_ARCH_TCC803X

	FUNCTION_OUT
	return 0;
}

int videosource_free_gpio(videosource_t * vdev) {
	FUNCTION_IN

	// free port gpios
#ifdef CONFIG_ARCH_TCC803X
	if(0 < vdev->gpio.intb_port)	gpio_free(vdev->gpio.intb_port);
#endif//CONFIG_ARCH_TCC803X
	if(0 < vdev->gpio.pwr_port)		gpio_free(vdev->gpio.pwr_port);
	if(0 < vdev->gpio.pwd_port)		gpio_free(vdev->gpio.pwd_port);
	if(0 < vdev->gpio.rst_port)		gpio_free(vdev->gpio.rst_port);

	FUNCTION_OUT
	return 0;
}

#ifdef CONFIG_ARCH_TCC803X
int videosource_if_init_mipi_csi2_interface(videosource_t * vdev, videosource_format_t * format, unsigned int onOff) {
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
		log("hssettle value : 0x%x \n", format->des_info.hssettle);
		MIPI_CSIS_Set_DPHY_Common_Control(format->des_info.hssettle, \
											format->des_info.clksettlectl, \
											ON, \
											OFF, \
											OFF, \
											format->des_info.data_lane_num, \
											ON);


		for(idx = 0; idx < format->des_info.input_ch_num ; idx++) {
			MIPI_CSIS_Set_ISP_Configuration(idx, \
											format->des_info.pixel_mode, \
											OFF, \
											OFF, \
											format->des_info.data_format, \
											idx);
			MIPI_CSIS_Set_ISP_Resolution(idx, format->width , format->height);
		}

		MIPI_CSIS_Set_CSIS_Clock_Control(0x0, 0xf);

		MIPI_CSIS_Set_CSIS_Common_Control(format->des_info.input_ch_num, \
											0x0, \
											ON, \
											format->des_info.interleave_mode, \
											format->des_info.data_lane_num, \
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

int videosource_if_set_mipi_csi2_interrupt(videosource_t * vdev, videosource_format_t * format, unsigned int onOff) {
	if(onOff) {
		// clear interrupt
		MIPI_CSIS_Set_CSIS_Interrupt_Src(0, \
						CIM_MSK_FrameStart_MASK | CIM_MSK_FrameEnd_MASK | \
						CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK | \
						CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK | \
						CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK | \
						CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK);

		MIPI_CSIS_Set_CSIS_Interrupt_Src(1, \
						CIM_MSK_LINE_END_MASK);

//		  if(request_threaded_irq(vdev->format.des_info.csi2_irq, videosource_if_mipi_csi2_isr, ds90ub964_irq_thread_handler, 0, "mipi_csi2", NULL)) {
//		  if(request_irq(vdev->format.des_info.csi2_irq, videosource_if_mipi_csi2_isr, IRQF_SHARED, "mipi_csi2", "mipi_csi2")) {
		if(request_irq(vdev->format.des_info.csi2_irq, \
						videosource_if_mipi_csi2_isr, \
						0, \
						"mipi_csi2", \
						NULL)) {
			loge("fail request irq(%d) \n", vdev->format.des_info.csi2_irq);
		}
		else {
			// unmask interrupt
			MIPI_CSIS_Set_CSIS_Interrupt_Mask(0, \
						CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK | \
						CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK | \
						CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK | \
						CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK, \
						0);

			MIPI_CSIS_Set_CSIS_Interrupt_Mask(1, \
						CIM_MSK_LINE_END_MASK, \
						0);
		}
	}
	else {
//		  free_irq(vdev->format.des_info.csi2_irq, "mipi_csi2");
		free_irq(vdev->format.des_info.csi2_irq, NULL);
	}

	return 0;
}
#endif//CONFIG_ARCH_TCC803X

int videosource_set_port(videosource_t * vdev, int enable) {
	struct pinctrl		* pinctrl	= NULL;
	int					ret			= 0;

	FUNCTION_IN

#ifdef CONFIG_ARCH_TCC803X
	if(vdev->type != VIDEOSOURCE_TYPE_MIPI)
#endif//CONFIG_ARCH_TCC803X
	{
		// pinctrl
		pinctrl = pinctrl_get_select(&vdev->client->dev, (enable == ENABLE) ? "active" : "idle");
		if(IS_ERR(pinctrl)) {
			pinctrl_put(pinctrl);
			loge("pinctrl select failed\n");
			return -1;
		}
	}

	FUNCTION_OUT
	return ret;
}

#define GPIO_FUNC					0x30

static void tcc_pin_to_reg(struct tcc_pinctrl *pctl, unsigned pin,
		       void __iomem **reg, unsigned *offset)
{
	struct tcc_pin_bank *bank = pctl->pin_banks;

	while (pin >= bank->base && (bank->base + bank->npins - 1) < pin)
		++bank;

	*reg = pctl->base + bank->reg_base;
	*offset = pin - bank->base;
}

int videosource_if_check_cif_port(struct device * dev, int enable) {
	struct pinctrl			* pinctrl	= NULL;
	struct pinctrl_state	* state		= NULL;
	struct pinctrl_setting	* setting	= NULL;
	char					name[1024];
	int						ret			= 0;

	// get pinctrl
	pinctrl = pinctrl_get(dev);
	if(IS_ERR(pinctrl)) {
		loge("ERROR: pinctrl_get returned %p\n", pinctrl);
		return -1;//p;
	}

	// get state of pinctrl
	sprintf(name, "%s", (enable == ENABLE) ? "active" : "idle");
	state = pinctrl_lookup_state(pinctrl, name);
	if(IS_ERR(state)) {
		loge("ERROR: pinctrl_lookup_state returned %p\n", state);
		pinctrl_put(pinctrl);
		return -1;//ERR_CAST(state);
	}

	// check if the current port configuration is the same as the expected one
	list_for_each_entry(setting, &state->settings, node) {
		if(setting->type == PIN_MAP_TYPE_CONFIGS_GROUP) {
			struct pinctrl_dev			* pctldev	= setting->pctldev;
			const struct pinctrl_ops	* pctlops	= pctldev->desc->pctlops;

			struct tcc_pinctrl			* pctl		= pinctrl_dev_get_drvdata(pctldev);
	
			if(pctlops->get_group_pins) {
				const unsigned		* pins			= NULL;
				unsigned			num_pins		= 0;
				int					idxPin			= 0;
				unsigned long		expected_func	= 0;
				unsigned long		current_func	= 0;

				// get the original pinctrl configuration information
				ret = pctlops->get_group_pins(pctldev, setting->data.mux.group, &pins, &num_pins);
				expected_func = tcc_pinconf_unpack_value(*setting->data.configs.configs);

				// get the current pinctrl configuration information
				for(idxPin = 0; idxPin < num_pins; idxPin++) {
					unsigned		group	= setting->data.mux.group;
					void __iomem	* reg	= NULL;
					unsigned int	offset;
					unsigned int	shift;

					// get an offset of the pin
					tcc_pin_to_reg(pctl, pctl->groups[group].pins[idxPin], &reg, &offset);

					// get the register address
					reg += GPIO_FUNC + 4 * (offset / 8);

					// get the shift for the pin
					shift = 4 * (offset % 8);

					// get the function of the pin
					current_func = (readl(reg) >> shift) & 0xF;

					// check if the current function is the same as the expected function value
					if(expected_func != current_func) {
						log("%s [%02d] - expected_func: %lu, current_func: %lu\n", pctl->pins->name, pins[idxPin], expected_func, current_func);
						ret = -1;
					}
				}
			}
		}
	}

	return ret;
}

int videosource_if_change_mode(videosource_t * vdev, int mode) {
	int		ret	= 0;

	dlog("mode: 0x%08x\n", mode);
	if(vdev->driver.change_mode == NULL) {
		loge("The function to change the mode is NULL\n");
		return -1;
	} else {
		ret = vdev->driver.change_mode(vdev->client, mode);
	}

	return ret;
}

int videosource_if_check_status(videosource_t * vdev) {
	int		ret	= 0;

	if(vdev->driver.check_status == NULL) {
		dlog("The function to check the video source's status is NULL\n");
		return -1;
	} else {
		ret = vdev->driver.check_status(vdev->client);
		if(ret == 1)
			dlog("videosource is working\n");
	}

	return ret;
}

int videosource_if_initialize(videosource_t * vdev) {
	int					ret		= 0;

	FUNCTION_IN

	if(vdev->enabled == ENABLE) {
		// open port
		videosource_set_port(vdev, ENABLE);

		// request gpio
		videosource_request_gpio(vdev);

		// power-up sequence
		vdev->driver.open(&vdev->gpio);

		// set videosource in init mode
		videosource_if_change_mode(vdev, MODE_INIT);

#ifdef CONFIG_ARCH_TCC803X
		if(vdev->type == VIDEOSOURCE_TYPE_MIPI) {
			// init remote serializer
			videosource_if_change_mode(vdev, MODE_SERDES_REMOTE_SER);

			// enable deserializer frame sync
			videosource_if_change_mode(vdev, MODE_SERDES_FSYNC);

#if 0
			// allocate deserializer interrupt
			vdev->driver.set_irq_handler(&vdev->gpio, ON);

			// enable deserializer interrupt
			videosource_if_change_mode(vdev, MODE_SERDES_INTERRUPT);
#endif

			// set mipi-csi2 interface
			videosource_if_init_mipi_csi2_interface(vdev, &vdev->format, ON);

			// enable mipi-csi2 interrupt
			videosource_if_set_mipi_csi2_interrupt(vdev, &vdev->format, ON);
		} else {
#endif//CONFIG_ARCH_TCC803X
			videosource_if_check_status(vdev);
#ifdef CONFIG_ARCH_TCC803X
		}
#endif//CONFIG_ARCH_TCC803X
	} else {
		ret = -1;
	}

	FUNCTION_OUT
	return ret;
}

int videosource_if_deinitialize(videosource_t * vdev) {
	int					ret		= 0;

	FUNCTION_IN

	if(vdev->enabled == ENABLE) {
#ifdef CONFIG_ARCH_TCC803X
		if(vdev->type == VIDEOSOURCE_TYPE_MIPI) {
			videosource_if_init_mipi_csi2_interface(vdev, &vdev->format, OFF);

			// disable mipi-csi2 interrupt
			videosource_if_set_mipi_csi2_interrupt(vdev, &vdev->format, OFF);

#if 0
			// disable deserializer interrupt
			vdev->driver.set_irq_handler(&vdev->gpio, OFF);
#endif
		}
#endif//CONFIG_ARCH_TCC803X

		// power-down sequence
		vdev->driver.close(&vdev->gpio);

		// free gpio
		videosource_free_gpio(vdev);

		// close port
		videosource_set_port(vdev, DISABLE);
	} else {
		ret = -1;
	}

	FUNCTION_OUT
	return ret;
}

int videosource_if_open(struct inode * inode, struct file * file) {
	videosource_t		* vdev  = container_of(inode->i_cdev, videosource_t, cdev);
	int					ret		= 0;

	FUNCTION_IN

	if(vdev->enabled == DISABLE) {
		file->private_data = vdev;
		vdev->enabled = ENABLE;
	} else {
		log("videosource is already enabled\n");
		ret = -1;
	}

	FUNCTION_OUT
	return ret;
}

int videosource_if_release(struct inode * inode, struct file * file) {
	videosource_t		* vdev	= container_of(inode->i_cdev, videosource_t, cdev);
	int					ret		= 0;

	FUNCTION_IN

	if(vdev->enabled == ENABLE) {
		vdev->enabled = DISABLE;
	} else {
		log("videosource is already disabled\n");
		ret = -1;
	}

	FUNCTION_OUT
	return ret;
}

long videosource_if_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	videosource_t		* vdev	= filp->private_data;
	int					ret		= 0;

	switch(cmd) {
	case VIDEOSOURCE_IOCTL_DEINITIALIZE:
		ret = videosource_if_deinitialize(vdev);
		break;

	case VIDEOSOURCE_IOCTL_INITIALIZE:
		ret = videosource_if_initialize(vdev);
		break;

	case VIDEOSOURCE_IOCTL_CHECK_STATUS:
		ret = videosource_if_check_status(vdev);
		break;

	case VIDEOSOURCE_IOCTL_GET_VIDEOSOURCE_FORMAT:
		dlog("VIDEOSOURCE_IOCTL_GET_VIDEOSOURCE_FORMAT\n");
		ret = copy_to_user((void __user *)arg, (const void *)&vdev->format, sizeof(vdev->format));
		if(ret < 0) {
			loge("ERROR: unable to copy the paramter(%d)\n", ret);
			ret = -1;
		}
		break;

	default:
		loge("The ioctl command(0x%x) is WRONG.\n", cmd);
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

int videosource_if_probe(videosource_t * vdev) {
	int		index		= 0;
	char	name[32]	= "";
	int		ret			= 0;
	FUNCTION_IN

	// parse the videosource's device tree
	if((ret = videosource_parse_gpio_dt_data(vdev, vdev->client->dev.of_node)) < 0) {
		loge("cannot initialize gpio port\n");
		return ret;
	}

	// get the videosource's index from its alias
	index = of_alias_get_id(vdev->client->dev.of_node, MODULE_NAME);

	// set the charactor device name
	sprintf(name, "%s%d", MODULE_NAME, index);

	// allocate a charactor device region
	ret = alloc_chrdev_region(&vdev->cdev_region, 0, 1, name);
	if(ret < 0) {
		loge("ERROR: Allocate a charactor device region for the \"%s\"\n", name);
		return ret;
	}

	// create the videosource class
	vdev->cdev_class = class_create(THIS_MODULE, name);
	if(vdev->cdev_class == NULL) {
		loge("ERROR: Create the \"%s\" class\n", name);
		goto goto_unregister_chrdev_region;
	}

	// create a videosource device file system
	if(device_create(vdev->cdev_class, NULL, vdev->cdev_region, NULL, name) == NULL) {
		loge("ERROR: Create the \"%s\" device file\n", name);
		goto goto_destroy_class;
	}

	// register the videosource device as a charactor device
	cdev_init(&vdev->cdev, &videosource_if_fops);
	ret = cdev_add(&vdev->cdev, vdev->cdev_region, 1);
	if(ret < 0) {
		loge("ERROR: Register the \"%s\" device as a charactor device\n", name);
		goto goto_destroy_device;
	}

	// create video source's loglevel attribute device
	videosource_create_attr_loglevel(&vdev->client->dev);

	goto goto_end;

goto_destroy_device:
	// destroy the videosource device file system
	device_destroy(vdev->cdev_class, vdev->cdev_region);
	
goto_destroy_class:
	// destroy the videosource class
	class_destroy(vdev->cdev_class);

goto_unregister_chrdev_region:
	// unregister the charactor device region
	unregister_chrdev_region(vdev->cdev_region, 1);

goto_end:
	FUNCTION_OUT
	return ret;
}

int videosource_if_remove(videosource_t * vdev) {
	int		ret	= 0;
	FUNCTION_IN

	// unregister the videosource device
	cdev_del(&vdev->cdev);

	// destroy the videosource device file system
	device_destroy(vdev->cdev_class, vdev->cdev_region);

	// destroy the videosource class
	class_destroy(vdev->cdev_class);

	// unregister the charactor device region
	unregister_chrdev_region(vdev->cdev_region, 1);

#ifdef CONFIG_ARCH_TCC803X
	if(vdev->type == VIDEOSOURCE_TYPE_MIPI) {
		clk_disable(mipi_csi2_clk);
	}
#endif//CONFIG_ARCH_TCC803X

	FUNCTION_OUT
	return ret;
}

