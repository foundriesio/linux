/*
 * arch/arm/mach-tegra/board-colibri_t20.h
 *
 * Copyright (C) 2011 Toradex, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_BOARD_COLIBRI_T20_H
#define _MACH_TEGRA_BOARD_COLIBRI_T20_H

/* Uncomment for camera interface support on Colibri Evaluation carrier
   board */
#if defined(CONFIG_TEGRA_CAMERA) || defined(CONFIG_VIDEO_TEGRA) || defined(CONFIG_VIDEO_TEGRA_MODULE)
#define COLIBRI_T20_VI
#endif

/* Uncomment to activate 32-bit GMI address/databus */
//#define GMI_32BIT

/* GPIO */

#define FF_DCD		TEGRA_GPIO_PC6	/* SODIMM 31 */
#define FF_DSR		TEGRA_GPIO_PC1	/* SODIMM 29 */

#define I2C_SCL		TEGRA_GPIO_PC4	/* SODIMM 196 */
#define I2C_SDA		TEGRA_GPIO_PC5	/* SODIMM 194 */

#define LAN_EXT_WAKEUP	TEGRA GPIO_PV5
#define LAN_PME		TEGRA_GPIO_PV6
#define LAN_RESET	TEGRA_GPIO_PV4
#define LAN_V_BUS	TEGRA_GPIO_PBB1

#define MECS_USB_HUB_RESET	TEGRA_GPIO_PBB3	/* SODIMM 127 */

#define MMC_CD		TEGRA_GPIO_PC7	/* SODIMM 43 */

#define NAND_WP_N	TEGRA_GPIO_PS0

#define PWR_I2C_SCL	TEGRA_GPIO_PZ6
#define PWR_I2C_SDA	TEGRA_GPIO_PZ7

#define THERMD_ALERT	TEGRA_GPIO_PV7

#define TOUCH_PEN_INT	TEGRA_GPIO_PV2

#define USB3340_RESETB	TEGRA_GPIO_PV1
//conflicts with MECS Tellurium xPOD2 SSPTXD2
#define USBC_DET	TEGRA_GPIO_PK5	/* SODIMM 137 */
#define USBH_OC		TEGRA_GPIO_PW3	/* SODIMM 131 */
#define USBH_PEN	TEGRA_GPIO_PW2	/* SODIMM 129 */

/* Use SODIMM pin 73 as DAC power save on Iris carrier board */
#define IRIS

/* Uncomment for back light and USB hub support on MECS Tellurium carrier
   board */
//#define MECS_TELLURIUM

/* Uncomment to use the xPOD2 which due to its Colibri T20 incompatible wiring
   uses GPIO bit banging SPI driver rather than a hardware SPI controller */
//#define MECS_TELLURIUM_XPOD2

/* Uncomment for 8-bit SDHCI on HSMMC controller (requires custom carrier
   board) */
//#define SDHCI_8BIT

/* TPS6586X gpios */
#define TPS6586X_GPIO_BASE	TEGRA_NR_GPIOS

/* Interrupt numbers from external peripherals */
#define TPS6586X_INT_BASE	TEGRA_NR_IRQS
#define TPS6586X_INT_END	(TPS6586X_INT_BASE + 32)

int colibri_t20_emc_init(void);
int colibri_t20_panel_init(void);
int colibri_t20_pinmux_init(void);
int colibri_t20_regulator_init(void);

#endif
