/*
 * RZ/N1 pinctrl access API
 *
 * Copyright (C) 2014-2016 Renesas Electronics Europe Limited
 *
 * Michel Pollet <michel.pollet@bp.renesas.com>, <buserror@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __PINCTRL_RZN1__
#define __PINCTRL_RZN1__

#include <dt-bindings/pinctrl/pinctrl-rzn1.h>

enum {
	MDIO_MUX_HIGHZ = 0,
	MDIO_MUX_MAC0,
	MDIO_MUX_MAC1,
	MDIO_MUX_ECAT,
	MDIO_MUX_S3_MDIO0,
	MDIO_MUX_S3_MDIO1,
	MDIO_MUX_HWRTOS,
	MDIO_MUX_SWITCH,
};

void rzn1_pinctrl_mdio_select(u8 mdio, u32 func);

enum {
	GPIOINT_MUX_GPIO0A = 0,
	GPIOINT_MUX_GPIO1A = (1 << 5),
	GPIOINT_MUX_GPIO2A = (2 << 5),
};
/*
 * Configures the GPIO interrupt dispatcher mux to route GPIO blocks 0:2 A
 * block to RZN1_IRQ_GPIO_* -- note that this doesn't do any interupt clearing
 * or anything else, the interrupt code is responsible for that.
 *
 * Use rzn1_pinctrl_gpioint_select(
 * 		RZN1_IRQ_GPIO_[0:7],
 * 		GPIOINT_MUX_GPIO[0:2]A + <0:31>);
 * Note that this doesn't configure the GPIO as input or anything.
 */
void rzn1_pinctrl_gpioint_select(u8 gpioint, u8 gpio_source);
u8 rzn1_pinctrl_gpioint_get(u8 gpioint);

#endif /* __PINCTRL_RZN1__ */
