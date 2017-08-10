/*
 * arch/arm/mach-tegra/board-apalis-tk1.h
 *
 * Copyright (c) 2016, Toradex AG. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#ifndef _MACH_TEGRA_BOARD_APALIS_TK1_H
#define _MACH_TEGRA_BOARD_APALIS_TK1_H

#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include "gpio-names.h"

//#define APALIS_TK1_EDP

/* GPIO */

#define APALIS_GPIO1	TEGRA_GPIO_PFF2
#define APALIS_GPIO2	TEGRA_GPIO_PFF0
#define APALIS_GPIO3	TEGRA_GPIO_PV4	/* open-drain only */
#define APALIS_GPIO4	TEGRA_GPIO_PV5	/* open-drain only */
#define APALIS_GPIO5	TEGRA_GPIO_PDD5
#define APALIS_GPIO6	TEGRA_GPIO_PDD6
#define APALIS_GPIO7	TEGRA_GPIO_PDD1
#define APALIS_GPIO8	TEGRA_GPIO_PDD2

#define BKL1_ON		TEGRA_GPIO_PBB5

/* TBD: Camera */
#define CAM_RSTN TEGRA_GPIO_PBB3
#define CAM_FLASH_STROBE TEGRA_GPIO_PBB4
#define CAM2_PWDN TEGRA_GPIO_PBB6
#define CAM1_PWDN TEGRA_GPIO_PBB5
#define CAM_AF_PWDN TEGRA_GPIO_PBB7
#define CAM_BOARD_E1806

#define FAN_EN		APALIS_GPIO8

#define HDMI1_HPD	TEGRA_GPIO_PN7

#define I2C1_SCL	TEGRA_GPIO_PC4
#define I2C1_SDA	TEGRA_GPIO_PC5

#define I2C2_SCL	TEGRA_GPIO_PT5
#define I2C2_SDA	TEGRA_GPIO_PT6

#define I2C3_SCL	TEGRA_GPIO_PBB1
#define I2C3_SDA	TEGRA_GPIO_PBB2

#define LAN_DEV_OFF_N	TEGRA_GPIO_PO6
#define LAN_RESET_N	TEGRA_GPIO_PS2
#define LAN_WAKE_N	TEGRA_GPIO_PO5

#define MMC1_CD_N	TEGRA_GPIO_PV3

#define PEX_PERST_N	APALIS_GPIO7

#define PWR_I2C_SCL	TEGRA_GPIO_PZ6
#define PWR_I2C_SDA	TEGRA_GPIO_PZ7

#define RESET_MOCI_CTRL	TEGRA_GPIO_PU4

#define SATA1_ACT_N	TEGRA_GPIO_PN2

#define SD1_CD_N	TEGRA_GPIO_PEE4

#define THERMD_ALERT_N	TEGRA_GPIO_PI6

#define TOUCH_WIPER	APALIS_GPIO6

/* TS1 reserved for recovery circuit */
/* TS2 reserved for PMIC power button */
/* TS3, TS4, TS5 and TS6 are not connected by default */

#define USBH_EN		TEGRA_GPIO_PN5
#define USBH_OC_N	TEGRA_GPIO_PBB0
#define USBO1_EN	TEGRA_GPIO_PN4
#define USBO1_OC_N	TEGRA_GPIO_PBB4

#define WAKE1_MICO	TEGRA_GPIO_PDD3

/*
 * Uncomment to use MXM3 pins 144, 146, 152, 156, 160, 162 & 164 for LEDs,
 * PCIE1_WDISABLE_N, SW3 and UART2_3_RS232_FOFF_N on Ixora carrier board
 */
//#define IXORA
#ifdef IXORA
#define LED4_GREEN		TEGRA_GPIO_PY5
#define LED4_RED		TEGRA_GPIO_PY4
#define LED5_GREEN		TEGRA_GPIO_PW5
#define LED5_RED		TEGRA_GPIO_PEE5
#define PCIE1_WDISABLE_N	TEGRA_GPIO_PY7
#define SW3			TEGRA_GPIO_PY6
#define UART2_3_RS232_FOFF_N	TEGRA_GPIO_PV3
#endif /* IXORA */

/* Uncomment for Apalis TK1 V1.0A prototypes and V1.0B samples */
//#define APALIS_TK1_V10

/* generated soc_therm OC interrupts */
#define TEGRA_SOC_OC_IRQ_BASE	TEGRA_NR_IRQS
#define TEGRA_SOC_OC_NUM_IRQ	TEGRA_SOC_OC_IRQ_MAX

/* External peripheral act as interrupt controller */

#define UTMI1_PORT_OWNER_XUSB   0x1
#define UTMI2_PORT_OWNER_XUSB   0x2
#define UTMI3_PORT_OWNER_XUSB	0x4

/* HDMI Hotplug detection pin */
#define apalis_tk1_hdmi_hpd	HDMI1_HPD

/* I2C related GPIOs */
#define TEGRA_GPIO_I2C1_SCL	I2C1_SCL
#define TEGRA_GPIO_I2C1_SDA	I2C1_SDA
#define TEGRA_GPIO_I2C2_SCL	I2C2_SCL
#define TEGRA_GPIO_I2C2_SDA	I2C2_SDA
#define TEGRA_GPIO_I2C3_SCL	I2C3_SCL
#define TEGRA_GPIO_I2C3_SDA	I2C3_SDA
#define TEGRA_GPIO_I2C5_SCL	PWR_I2C_SCL
#define TEGRA_GPIO_I2C5_SDA	PWR_I2C_SDA

/* SATA Specific */

#define CLK_RST_CNTRL_RST_DEV_W_SET 0x7000E438
#define CLK_RST_CNTRL_RST_DEV_V_SET 0x7000E430
#define SET_CEC_RST 0x100

int apalis_tk1_display_init(void);
int apalis_tk1_edp_init(void);
int apalis_tk1_emc_init(void);
int apalis_tk1_panel_init(void);
int apalis_tk1_rail_alignment_init(void);
int apalis_tk1_regulator_init(void);
int apalis_tk1_sdhci_init(void);
int apalis_tk1_sensors_init(void);
int apalis_tk1_soctherm_init(void);
int apalis_tk1_suspend_init(void);
void apalis_tk1_camera_auxdata(void *);
void apalis_tk1_new_sysedp_init(void);
void apalis_tk1_sysedp_batmon_init(void);
void apalis_tk1_sysedp_dynamic_capping_init(void);

#endif
