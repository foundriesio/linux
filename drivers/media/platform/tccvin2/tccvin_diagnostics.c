/*
 *      uvc_driver.c  --  USB Video Class driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Driver management


 *****************************************************************************/

#include <media/v4l2-common.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

#include "tccvin_video.h"

#include "../../../pinctrl/core.h"
#include "../../../pinctrl/tcc/pinctrl-tcc.h"

struct tcc_pin_group {
	const char *name;
	const u32 *pins;
	u32 npins;
	u32 func;
};

#define GPIO_FUNC	0x30

static void tcc_pin_to_reg(struct tcc_pinctrl *pctl, unsigned pin,
		       void __iomem **reg, unsigned *offset)
{
	struct tcc_pin_bank *bank = pctl->pin_banks;

	while (pin >= bank->base && (bank->base + bank->npins - 1) < pin)
		++bank;

	*reg = pctl->base + bank->reg_base;
	*offset = pin - bank->base;
}

static int tccvin_diag_cif_port_pinctrl(struct device * dev, const char * funcname)
{
	struct device_node	*vs_node	= NULL;
	const char		*pin_names	= NULL;
	struct pinctrl		*pinctrl	= NULL;
	struct pinctrl_state	*state		= NULL;
	struct pinctrl_setting	*setting	= NULL;
	int			ret		= 0;

	vs_node	= dev->of_node;
	if (vs_node != NULL) {
		of_property_read_string(vs_node, "pinctrl-names", &pin_names);
		if (pin_names != NULL) {
			// get pinctrl
			pinctrl = pinctrl_get(dev);
			if (IS_ERR(pinctrl)) {
				loge("ERROR: pinctrl_get returned %p\n", pinctrl);
				return -1;
			}

			// get state of pinctrl
			state = pinctrl_lookup_state(pinctrl, funcname);
			if (IS_ERR(state)) {
				loge("ERROR: pinctrl_lookup_state returned %p\n", state);
				pinctrl_put(pinctrl);
				return -1;
			}

			// check if the current port configuration is the same as the expected one
			list_for_each_entry(setting, &state->settings, node) {
				if (setting->type == PIN_MAP_TYPE_CONFIGS_GROUP) {
					struct pinctrl_dev		* pctldev	= setting->pctldev;
					const struct pinctrl_ops	* pctlops	= pctldev->desc->pctlops;
					struct tcc_pinctrl		* pctl		= pinctrl_dev_get_drvdata(pctldev);

					// get_group_pins function is not NULL
					if (pctlops->get_group_pins) {
						unsigned		group		= 0;
						char			group_name[64]	= "";
						char			pins_name[64]	= "";
						const unsigned		*pins		= NULL;
						unsigned		num_pins	= 0;
						int			idxPin		= 0;
						unsigned long		expected_func	= 0;
						unsigned long		current_func	= 0;

						group	= setting->data.mux.group;

						// get pin group name
						sprintf(group_name, "%s", pctlops->get_group_name(pctldev, group));

						// get the pin list
						ret = pctlops->get_group_pins(pctldev, group, &pins, &num_pins);

						// get the pin function to use
						expected_func = tcc_pinconf_unpack_value(*setting->data.configs.configs);

						// get pins name
						if (pctl->pins->name != NULL) {
							char		*p	= NULL;

							// get pins name
							sprintf(pins_name, "%s", pctl->pins->name);
							p = strstr(pins_name, "-");
							*p = '\0';
						}

						// get the current pinctrl configuration information
						for (idxPin = 0; idxPin < num_pins; idxPin++) {
							void __iomem	*reg	= NULL;
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
							logi("**** group name: %-20s - gpio: %s[%3d], func: %lu -> %lu\n", group_name, pins_name, pins[idxPin], expected_func, current_func);
							if(expected_func != current_func) {
								loge("**** group name: %-20s - gpio: %s[%3d], func: %lu -> %lu\n", group_name, pins_name, pins[idxPin], expected_func, current_func);
								ret = -1;
							}
						}
					}
				}
			}
		}
	}

	return ret;
}

int tccvin_diag_cif_port(struct v4l2_subdev *sd)
{
	struct i2c_client	*client	= NULL;
	int			ret	= 0;

	/* v4l2_get_subdevdata */
	client = v4l2_get_subdevdata(sd);
	if (client == NULL) {
		/* client is NULL */
		loge("client is NULL\n");
	}

	/* check pinctrl */
	ret = tccvin_diag_cif_port_pinctrl(&client->dev, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		/* failure of tccvin_diag_cif_port_pinctrl */
		loge("tccvin_diag_cif_port_pinctrl, ret: %d\n", ret);
	}

	return ret;
}

