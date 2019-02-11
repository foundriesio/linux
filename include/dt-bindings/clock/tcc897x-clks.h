/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
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
 * this program; if not	 write to the Free Software Foundation	 Inc., 59 Temple Place	
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __DT_TCC897X_CLKS_H__
#define __DT_TCC897X_CLKS_H__

#ifndef BIT
#define BIT(nr)			(1 << (nr))
#endif

#define CLK_SET_RATE_GATE	BIT(0) /* must be gated across rate change */
#define CLK_SET_PARENT_GATE	BIT(1) /* must be gated across re-parent */
#define CLK_SET_RATE_PARENT	BIT(2) /* propagate rate change up one level */
#define CLK_IGNORE_UNUSED	BIT(3) /* do not gate even if unused */
				/* unused */
#define CLK_IS_BASIC		BIT(5) /* Basic clk, can't do a to_clk_foo() */
#define CLK_GET_RATE_NOCACHE	BIT(6) /* do not use the cached clk rate */
#define CLK_SET_RATE_NO_REPARENT BIT(7) /* don't re-parent on rate change */
#define CLK_GET_ACCURACY_NOCACHE BIT(8) /* do not use the cached clk accuracy */
#define CLK_RECALC_NEW_RATES	BIT(9) /* recalc rates after notifications */
#define CLK_SET_RATE_UNGATE	BIT(10) /* clock needs to run to set rate */
#define CLK_IS_CRITICAL		BIT(11) /* do not gate, ever */
/* parents need enable during gate/ungate, set rate and re-parent */
#define CLK_OPS_PARENT_ENABLE	BIT(12)

/* clkctrl */
#define FBUS_CPU0			0	// CLKCTRL0 (Cortex-A7 MP)
#define FBUS_CPU1			1	// CLKCTRL1 (Cortex-A7 SP)
#define FBUS_MEM			2	// CLKCTRL2
#define FBUS_DDI			3	// CLKCTRL3
#define FBUS_GPU			4	// CLKCTRL4
#define FBUS_IO			5	// CLKCTRL5
#define FBUS_VBUS			6	// CLKCTRL6
#define FBUS_CODA			7	// CLKCTRL7
#define FBUS_HSIO			8	// CLKCTRL8
#define FBUS_SMU			9	// CLKCTRL9
#define FBUS_G2D			10	// CLKCTRL10
#define FBUS_CMBUS			11	// CLKCTRL11 (Cortex-M4)
#define FBUS_HEVC_VCE		12	// CLKCTRL12
#define FBUS_HEVC_VCPU		13	// CLKCTRL13
#define FBUS_HEVC_BPU		14	// CLKCTRL14
#define FBUS_MAX			15

/* Peripheral Clocks */
#define PERI_TCX			0
#define PERI_TCT			1
#define PERI_TCZ			2
#define PERI_LCD0			3
#define PERI_LCDSI0		4
#define PERI_LCD1			5
#define PERI_LCDSI1		6
#define PERI_RESERVED07	7
#define PERI_LCDTIMER		8
#define PERI_USB_OTG		9
#define PERI_GMAC_PTP		10
#define PERI_GMAC			11
#define PERI_USB_OTG_ADP	12
#define PERI_IOB_CEC		13
#define PERI_RESERVED14	14
#define PERI_OUT0			15
#define PERI_RESERVED16	16
#define PERI_HDMI			17
#define PERI_HDMIA			18
#define PERI_OUT1			19
#define PERI_REMOTE		20
#define PERI_SDMMC0		21
#define PERI_SDMMC1		22
#define PERI_SDMMC2		23
#define PERI_SDMMC3		24
#define PERI_ADAI3			25
#define PERI_ADAM3			26
#define PERI_SPDIF3		27
#define PERI_ADAI0			28
#define PERI_ADAM0			29
#define PERI_SPDIF0		30
#define PERI_PDM			31
#define PERI_CMBUS_CNT		32
#define PERI_ADC			33
#define PERI_I2C0			34
#define PERI_I2C1			35
#define PERI_I2C2			36
#define PERI_I2C3			37
#define PERI_UART0			38
#define PERI_UART1			39
#define PERI_UART2			40
#define PERI_UART3			41
#define PERI_UART4			42
#define PERI_UART5			43
#define PERI_UART6			44
#define PERI_UART7			45
#define PERI_GPSB0			46
#define PERI_GPSB1			47
#define PERI_GPSB2			48
#define PERI_RESERVED49	49
#define PERI_RESERVED50	50
#define PERI_RESERVED51	51
#define PERI_I2C6			52
#define PERI_I2C7			53
#define PERI_OUT2			54
#define PERI_OUT3			55
#define PERI_OUT4			56
#define PERI_OUT5			57
#define PERI_TSTX0			58
#define PERI_RESERVED59	59
#define PERI_CMBUS_TSIF0	60
#define PERI_CMBUS_TSIF1	61
#define PERI_CMBUS_TSIF2	62
#define PERI_CMBUS_TSIF3	63
#define PERI_RESERVED64	64
#define PERI_I2C4			65
#define PERI_I2C5			66
#define PERI_HDMI_PCLK		67
#define PERI_ADAI1			68
#define PERI_ADAM1			69
#define PERI_SPDIF1		70
#define PERI_ADAI2			71
#define PERI_ADAM2			72
#define PERI_SPDIF2		73
#define PERI_MAX			74

/* I/O Bus pwdn/swreset */
#define IOBUS_RESERVED00	0
#define IOBUS_RESERVED01	1
#define IOBUS_SDMMC0		2
#define IOBUS_RESERVED03	3
#define IOBUS_SDMMC2		4
#define IOBUS_SDMMC3		5
#define IOBUS_SDMMC		6
#define IOBUS_DMA0			7
#define IOBUS_DMA1			8
#define IOBUS_DMA2			9
#define IOBUS_DMA			10
#define IOBUS_RESERVED11	11
#define IOBUS_PWM			12
#define IOBUS_SMC			13
#define IOBUS_I2C_S2		14
#define IOBUS_RTC			15
#define IOBUS_REMOCON		16
#define IOBUS_TSADC		17
#define IOBUS_EDICFG		18
#define IOBUS_EDI			19
#define IOBUS_PROT			20
#define IOBUS_ADMA0		21
#define IOBUS_DAI0			22
#define IOBUS_SPDIF0		23
#define IOBUS_AUDIO0		24
#define IOBUS_ADMA3		25
#define IOBUS_DAI3			26
#define IOBUS_SPDIF3		27
#define IOBUS_AUDIO3		28
#define IOBUS_I2C_M0		29
#define IOBUS_I2C_M1		30
#define IOBUS_I2C_M2		31
#define IOBUS_I2C_M3		32	//  0
#define IOBUS_I2C_S0		33	//  1
#define IOBUS_I2C_S1		34	//  2
#define IOBUS_I2C			35	//  3
#define IOBUS_UART0		36	//  4
#define IOBUS_UART1		37	//  5
#define IOBUS_UART2		38	//  6
#define IOBUS_UART3		39	//  7
#define IOBUS_UART4		40	//  8
#define IOBUS_UART5		41	//  9
#define IOBUS_UART6		42	// 10
#define IOBUS_UART7		43	// 11
#define IOBUS_UART			44	// 12
#define IOBUS_RESERVED45	45
#define IOBUS_NFC			46	// 14
#define IOBUS_RESERVED47	47
#define IOBUS_RESERVED48	48
#define IOBUS_RESERVED49	49
#define IOBUS_TS0			50	// 18
#define IOBUS_GPSB0		51	// 19
#define IOBUS_GPSB1		52	// 20
#define IOBUS_GPSB2		53	// 21
#define IOBUS_RESERVED54	54
#define IOBUS_RESERVED55	55
#define IOBUS_RESERVED56	56
#define IOBUS_GPSB			57	// 25
#define IOBUS_CEC			58	// 26
#define IOBUS_I2C_S3		59	// 27
#define IOBUS_I2C_M4		60	// 28
#define IOBUS_I2C_M5		61	// 29
#define IOBUS_I2C_M6		62	// 30
#define IOBUS_I2C_M7		63	// 31
#define IOBUS_DMAC0		64	//  0
#define IOBUS_DMAC1		65	//  1
#define IOBUS_DMAC2		66	//  2
#define IOBUS_DMAC			67	//  3
#define IOBUS_ADMA1		68	//  4
#define IOBUS_DAI1			69	//  5
#define IOBUS_SPDIF1		70	//  6
#define IOBUS_AUDIO1		71	//  7
#define IOBUS_ADMA2		72	//  8
#define IOBUS_DAI2			73	//  9
#define IOBUS_SPDIF2		74	// 10
#define IOBUS_AUDIO2		75	// 11
#define IOBUS_RESERVED76	76
#define IOBUS_RESERVED77	77
#define IOBUS_RESERVED78	78
#define IOBUS_RESERVED79	79
#define IOBUS_RESERVED80	80
#define IOBUS_RESERVED81	81
#define IOBUS_RESERVED82	82
#define IOBUS_RESERVED83	83
#define IOBUS_RESERVED84	84
#define IOBUS_RESERVED85	85
#define IOBUS_RESERVED86	86
#define IOBUS_RESERVED87	87
#define IOBUS_RESERVED88	88
#define IOBUS_RESERVED89	89
#define IOBUS_RESERVED90	90
#define IOBUS_RESERVED91	91
#define IOBUS_RESERVED92	92
#define IOBUS_RESERVED93	93
#define IOBUS_RESERVED94	94
#define IOBUS_RESERVED95	95
#define IOBUS_MAX			96

/* Display Bus pwdn/swreset */
#define DDIBUS_VIOC		0
#define DDIBUS_NTSCPAL		1
#define DDIBUS_HDMI		2
#define DDIBUS_G2D			3
#define DDIBUS_MAX			4

/* Graphic Bus pwdn/swreset */
#define GPUBUS_GRP			0
#define GPUBUS_MAX			1

/* Video Bus pwdn/swreset */
#define VIDEOBUS_JENC		0
#define VIDEOBUS_RESERVED01	1
#define VIDEOBUS_COD		2
#define VIDEOBUS_VBUS		3
#define VIDEOBUS_HEVC		4
#define VIDEOBUS_MAX		5

/* High-Speed I/O Bus pwdn/swreset */
#define HSIOBUS_USBOTG		0
#define HSIOBUS_RESERVED01	1
#define HSIOBUS_RESERVED02	2
#define HSIOBUS_GMAC		3
#define HSIOBUS_RESERVED04	4
#define HSIOBUS_RESERVED05	5
#define HSIOBUS_USB20H		6
#define HSIOBUS_RESERVED07	7
#define HSIOBUS_RESERVED08	8
#define HSIOBUS_RESERVED09	9
#define HSIOBUS_RESERVED10	10
#define HSIOBUS_RESERVED11	11
#define HSIOBUS_RESERVED12	12
#define HSIOBUS_RESERVED13	13
#define HSIOBUS_RESERVED14	14
#define HSIOBUS_RESERVED15	15
#define HSIOBUS_RESERVED16	16
#define HSIOBUS_RESERVED17	17
#define HSIOBUS_CIPHER		18
#define HSIOBUS_RESERVED19	19
#define HSIOBUS_RESERVED20	20
#define HSIOBUS_RESERVED21	21
#define HSIOBUS_TRNG		22
#define HSIOBUS_MAX		23

/* IP Isolation Control Register (PMU_ISOL: 0x7440005C) */
#define ISOIP_TOP_OTP		0
#define ISOIP_TOP_RTC		1
#define ISOIP_TOP_PLL		2
#define ISOIP_TOP_ECID		3
#define ISOIP_TOP_HDMI		4
#define ISOIP_TOP_VDAC_3CH	5
#define ISOIP_TOP_VDAC_1CH	6
#define ISOIP_TOP_TSADC		7
#define ISOIP_TOP_USB2P		8
#define ISOIP_TOP_USBOTG	9
#define ISOIP_TOP_RESERVED10	10
#define ISOIP_TOP_LVDS		11
#define ISOIP_TOP_MAX		12

#endif
