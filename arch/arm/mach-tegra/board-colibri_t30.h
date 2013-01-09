/*
 * arch/arm/mach-tegra/board-colibri_t30.h
 *
 * Copyright (c) 2012 Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _MACH_TEGRA_BOARD_COLIBRI_T30_H
#define _MACH_TEGRA_BOARD_COLIBRI_T30_H

#include <linux/mfd/tps6591x.h>

#include <mach/gpio.h>
#include <mach/irqs.h>

/* External peripheral act as gpio */
/* TPS6591x GPIOs */
#define TPS6591X_GPIO_BASE	TEGRA_NR_GPIOS
#define TPS6591X_GPIO_0		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP0)
#define TPS6591X_GPIO_1		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP1)
#define TPS6591X_GPIO_2		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP2)
#define TPS6591X_GPIO_3		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP3)
#define TPS6591X_GPIO_4		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP4)
#define TPS6591X_GPIO_5		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP5)
#define TPS6591X_GPIO_6		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP6)
#define TPS6591X_GPIO_7		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP7)
#define TPS6591X_GPIO_8		(TPS6591X_GPIO_BASE + TPS6591X_GPIO_GP8)
#define TPS6591X_GPIO_END	(TPS6591X_GPIO_BASE + TPS6591X_GPIO_NR)

#define AC_PRESENT_GPIO		TPS6591X_GPIO_4

/*****************Interrupt tables ******************/
/* External peripheral act as interrupt controller */
/* TPS6591x IRQs */
#define TPS6591X_IRQ_BASE	TEGRA_NR_IRQS
#define TPS6591X_IRQ_END	(TPS6591X_IRQ_BASE + 18)

#define AC_PRESENT_INT		(TPS6591X_INT_GPIO4 + TPS6591X_IRQ_BASE)

/* STMPE811 IRQs */
#define STMPE811_IRQ_BASE	TPS6591X_IRQ_END
#define STMPE811_IRQ_END	(STMPE811_IRQ_BASE + 22)

/* Uncomment for SD-card on SDMMC4B rather than SDMMC2 */
//#define COLIBRI_T30_SDMMC4B

/* Uncomment for Colibri T30 V1.0a prototypes */
//#define COLIBRI_T30_V10

/* Uncomment for camera interface support on Colibri Evaluation carrier board */
//#define COLIBRI_T30_VI

#define TDIODE_OFFSET	(10000)	/* in millicelsius */

/* Run framebuffer in VGA mode */
#define TEGRA_FB_VGA

int colibri_t30_regulator_init(void);
int colibri_t30_suspend_init(void);
int colibri_t30_pinmux_init(void);
int colibri_t30_panel_init(void);
int colibri_t30_sensors_init(void);
int colibri_t30_gpio_switch_regulator_init(void);
int colibri_t30_pins_state_init(void);
int colibri_t30_emc_init(void);
int colibri_t30_power_off_init(void);
int colibri_t30_edp_init(void);

#endif
