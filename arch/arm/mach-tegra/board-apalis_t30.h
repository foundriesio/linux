/*
 * arch/arm/mach-tegra/board-apalis_t30.h
 *
 * Copyright (c) 2013 Toradex, Inc.
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

#ifndef _MACH_TEGRA_BOARD_APALIS_T30_H
#define _MACH_TEGRA_BOARD_APALIS_T30_H

#include <linux/mfd/tps6591x.h>

#include <mach/gpio.h>
#include <mach/irqs.h>

#include "gpio-names.h"

//#define FORCE_OFF_GPIO	APALIS_GPIO6
//#define POWER_GPIO	APALIS_GPIO5

/* GPIO */

#define APALIS_GPIO1	TEGRA_GPIO_PS2
#define APALIS_GPIO2	TEGRA_GPIO_PS3
#define APALIS_GPIO3	TEGRA_GPIO_PS4
#define APALIS_GPIO4	TEGRA_GPIO_PS5
#define APALIS_GPIO5	TEGRA_GPIO_PS6
#define APALIS_GPIO6	TEGRA_GPIO_PQ0
#define APALIS_GPIO7	TEGRA_GPIO_PS7
#define APALIS_GPIO8	TEGRA_GPIO_PQ1

#define BKL1_ON		TEGRA_GPIO_PV2
#define BKL1_PWM_EN_N	TEGRA_GPIO_PA1

#define CAN1_INT	TEGRA_GPIO_PW2
#define CAN2_INT	TEGRA_GPIO_PW3

#define FAN_EN		APALIS_GPIO8

#define HDMI1_HPD	TEGRA_GPIO_PN7

#define I2C1_SCL	TEGRA_GPIO_PC4
#define I2C1_SDA	TEGRA_GPIO_PC5

#define I2C2_SCL	TEGRA_GPIO_PV4
#define I2C2_SDA	TEGRA_GPIO_PV5

#define I2C3_SCL	TEGRA_GPIO_PBB1
#define I2C3_SDA	TEGRA_GPIO_PBB2

#define LAN_SMB_ALERT_N	TEGRA_GPIO_PZ5

#define LVDS_MODE	TEGRA_GPIO_PBB0
#define LVDS_6B_8B_N	TEGRA_GPIO_PBB3
#define LVDS_OE		TEGRA_GPIO_PBB4
#define LVDS_PDWN_N	TEGRA_GPIO_PBB5
#define LVDS_R_F_N	TEGRA_GPIO_PBB6
#define LVDS_MAP	TEGRA_GPIO_PBB7
#define LVDS_RS		TEGRA_GPIO_PCC1
#define LVDS_DDR_N	TEGRA_GPIO_PCC2

#define MMC1_CD_N	TEGRA_GPIO_PV3

#define PEX_PERST_N	APALIS_GPIO7

#define PWR_I2C_SCL	TEGRA_GPIO_PZ6
#define PWR_I2C_SDA	TEGRA_GPIO_PZ7

#define RESET_MOCI_N	TEGRA_GPIO_PI4

#define SATA1_ACT_N	TEGRA_GPIO_PDD0

#define SD1_CD_N	TEGRA_GPIO_PCC5

#define THERMD_ALERT_N	TEGRA_GPIO_PD2

#define TOUCH_PEN_INT	TEGRA_GPIO_PV0
#define TOUCH_WIPER	APALIS_GPIO6

#define TS1		TEGRA_GPIO_PI1
#define TS2		TEGRA_GPIO_PQ7
#define TS3		TEGRA_GPIO_PQ5
#define TS4		TEGRA_GPIO_PQ6
#define TS5		TEGRA_GPIO_PS0
#define TS6		TEGRA_GPIO_PS1

#define USBH_EN		TEGRA_GPIO_PDD1
#define USBH_OC_N	TEGRA_GPIO_PDD2
#define USBO1_EN	TEGRA_GPIO_PT5
#define USBO1_OC_N	TEGRA_GPIO_PT6

#define WAKE1_MICO	TEGRA_GPIO_PV1

/*
 * Uncomment to use MXM3 pins 144, 146, 152, 156, 160, 162 & 164 for LEDs,
 * PCIE1_WDISABLE_N, SW3 and UART2_3_RS232_FOFF_N on Ixora carrier board
 */
//#define IXORA
#ifdef IXORA
#define LED4_GREEN		TEGRA_GPIO_PB6
#define LED4_RED		TEGRA_GPIO_PB4
#define LED5_GREEN		TEGRA_GPIO_PD0
#define LED5_RED		TEGRA_GPIO_PD3
#define PCIE1_WDISABLE_N	TEGRA_GPIO_PB5
#define SW3			TEGRA_GPIO_PB7
#define UART2_3_RS232_FOFF_N	TEGRA_GPIO_PV3
#endif /* IXORA */

/* STMPE811 IRQs */
#define STMPE811_IRQ_BASE	TEGRA_NR_IRQS
#define STMPE811_IRQ_END	(STMPE811_IRQ_BASE + 22)

#define TDIODE_OFFSET	(10000)	/* in millicelsius */
#define TCRIT_LOCAL 95000 /* board temp to switch off PMIC in millicelsius*/

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

int apalis_t30_regulator_init(void);
int apalis_t30_suspend_init(void);
int apalis_t30_pinmux_init(void);
int apalis_t30_panel_init(void);
int apalis_t30_sensors_init(void);
int apalis_t30_gpio_switch_regulator_init(void);
int apalis_t30_pins_state_init(void);
int apalis_t30_emc_init(void);
int apalis_t30_power_off_init(void);
int apalis_t30_edp_init(void);

#endif
