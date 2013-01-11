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

/* TPS6586X gpios */
#define TPS6586X_GPIO_BASE	TEGRA_NR_GPIOS
#define AVDD_DSI_CSI_ENB_GPIO	(TPS6586X_GPIO_BASE + 1) /* gpio2 */

/* Interrupt numbers from external peripherals */
#define TPS6586X_INT_BASE	TEGRA_NR_IRQS
#define TPS6586X_INT_END	(TPS6586X_INT_BASE + 32)

/* Uncomment for camera interface support on Colibri Evaluation carrier board */
#ifdef CONFIG_TEGRA_CAMERA
#define COLIBRI_T20_VI
#endif

/* Use SODIMM pin 73 as DAC power save on Iris carrier board */
#define IRIS

/* Uncomment for back light and USB hub support on MECS Tellurium carrier board */
//#define MECS_TELLURIUM

/* Uncomment for 8-bit SDHCI on HSMMC controller (requires custom carrier board) */
//#define SDHCI_8BIT

/* Run framebuffer in VGA mode */
#ifndef CONFIG_ANDROID
#define TEGRA_FB_VGA
#endif

int colibri_t20_emc_init(void);
int colibri_t20_panel_init(void);
int colibri_t20_pinmux_init(void);
int colibri_t20_regulator_init(void);

#endif
