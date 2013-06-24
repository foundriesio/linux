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

/* Uncomment for camera interface support on Colibri Evaluation carrier
   board */
#if defined(CONFIG_TEGRA_CAMERA) || defined(CONFIG_VIDEO_TEGRA)
#define COLIBRI_T30_VI
#endif

/* Run framebuffer in VGA mode */
#define TEGRA_FB_VGA

/* GPIO */

#define DDC_SCL		TEGRA_GPIO_PV4	/* X2-15 */
#define DDC_SDA		TEGRA_GPIO_PV5	/* X2-16 */

#ifdef COLIBRI_T30_V10
#define EMMC_DETECT	TEGRA_GPIO_PC7
#endif

#define EN_MIC_GND	TEGRA_GPIO_PT1

#define FUSION_PEN_DOWN	TEGRA_GPIO_PY6	/* SODIMM 103 */
#define FUSION_RESET	TEGRA_GPIO_PY7	/* SODIMM 101 */

#define I2C_SCL		TEGRA_GPIO_PC4	/* SODIMM 196 */
#define I2C_SDA		TEGRA_GPIO_PC5	/* SODIMM 194 */

#define LAN_EXT_WAKEUP	TEGRA_GPIO_PDD1
#define LAN_PME		TEGRA_GPIO_PDD3
#define LAN_RESET	TEGRA_GPIO_PDD0
#define LAN_V_BUS	TEGRA_GPIO_PDD2

#ifdef COLIBRI_T30_V10
#define MMC_CD		TEGRA_GPIO_PU6	/* SODIMM 43 */
#else
#define MMC_CD		TEGRA_GPIO_PC7	/* SODIMM 43 */
#endif

#define PWR_I2C_SCL	TEGRA_GPIO_PZ6
#define PWR_I2C_SDA	TEGRA_GPIO_PZ7

#define THERMD_ALERT	TEGRA_GPIO_PD2

#define TOUCH_PEN_INT	TEGRA_GPIO_PV0

#define USBC_DET	TEGRA_GPIO_PK5	/* SODIMM 137 */
#define USBH_OC		TEGRA_GPIO_PW3	/* SODIMM 131 */
#define USBH_PEN	TEGRA_GPIO_PW2	/* SODIMM 129 */

/* Uncomment for Colibri T30 V1.0a prototypes */
//#define COLIBRI_T30_V10

/* Uncomment for SD-card on SDMMC4B rather than SDMMC2 */
//#define COLIBRI_T30_SDMMC4B

/* STMPE811 IRQs */
#define STMPE811_IRQ_BASE	TEGRA_NR_IRQS
#define STMPE811_IRQ_END	(STMPE811_IRQ_BASE + 22)

#define TDIODE_OFFSET	(10000)	/* in millicelsius */
#define TCRIT_LOCAL 95000 /* board temperature which switches off PMIC in millicelsius*/

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

/*****************Interrupt tables ******************/
/* External peripheral act as interrupt controller */
/* TPS6591x IRQs */
#define TPS6591X_IRQ_BASE	STMPE811_IRQ_END
#define TPS6591X_IRQ_END	(TPS6591X_IRQ_BASE + 18)

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
