// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>

void dxb_ctrl_gpio_out_init(int32_t pin)
{
	int32_t rc;

	if (!gpio_is_valid(pin)) {
		(void)pr_err("[ERROR][TCC_DXB_CTRL] %s pin(0x%X) error\n",
		       __func__, pin);
		return;
	}
	rc = gpio_request((uint32_t)pin, NULL);
	if (rc!=0) {
		(void)pr_err("[ERROR][TCC_DXB_CTRL] %s pin(0x%X) error(%d)\n",
		       __func__, pin, rc);
		return;
	}
	(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[0x%X] value[0]\n", __func__, pin);
	(void)gpio_direction_output((uint32_t)pin, 0);
}

#if 0 /* comment out to avoid QAC/Codesonar warning, enable later if required */
void dxb_ctrl_gpio_in_init(int32_t pin)
{
	int32_t rc;

	if (!gpio_is_valid(pin)) {
		(void)pr_err("[ERROR][TCC_DXB_CTRL] %s pin(0x%X) error\n",
		       ___func__, pin);
		return;
	}
	rc = gpio_request((uint32_t)pin, NULL);
	if (rc!=0) {
		(void)pr_err("[ERROR][TCC_DXB_CTRL] %s pin(0x%X) error(%d)\n",
		       ___func__, pin, rc);
		return;
	}
	(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[0x%X]\n", __func__, pin);
	(void)gpio_direction_input((uint32_t)pin);
}
#endif

void dxb_ctrl_gpio_set_value(int32_t pin, int32_t value)
{
	if (!gpio_is_valid(pin)) {
		(void)pr_err("[ERROR][TCC_DXB_CTRL] %s pin(0x%X) error\n",
		       __func__, pin);
		return;
	}
	(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[0x%X] value[%d]\n", __func__, pin,
		value);

	if (gpio_cansleep((uint32_t)pin)!=0) {
		gpio_set_value_cansleep((uint32_t)pin, value);
	} else {
		gpio_set_value((uint32_t)pin, value);
	}
}

#if 0 /* comment out to avoid QAC/Codesonar warning, enable later if required */
int32_t dxb_ctrl_gpio_get_value(int32_t pin)
{
	if (!gpio_is_valid(pin)) {
		(void)pr_err("[ERROR][TCC_DXB_CTRL] %s pin(0x%X) error\n",
		       __func__, pin);
		return -1;
	 }
	(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[0x%X]\n", __func__, pin);

	if (gpio_cansleep((uint32_t)pin)!=0) {
		return gpio_get_value_cansleep((uint32_t)pin);
	} else {
		return gpio_get_value((uint32_t)pin);
	}
}
#endif

void dxb_ctrl_gpio_free(int32_t pin)
{
	(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[0x%X]\n", __func__, pin);

	 if (gpio_is_valid(pin)) {
		gpio_free((uint32_t)pin);
	}
}

