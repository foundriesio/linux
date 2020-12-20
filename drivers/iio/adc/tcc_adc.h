// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ADC_H
#define TCC_ADC_H

#define PMU_TSADC_STOP_SHIFT	0
#define PMU_TSADC_PWREN_SHIFT	16

/*
 * TCC MICOM ADC Register
 */
#define ADCCMD			0x00U
#define ADCIRQ			0x04U
#define ADCCTRL			0x08U
#define ADCDATA0		0x80U
#define ADC_CLK_OUT_EN		BIT(30)
#define ADC_CLK_DIV_EN		BIT(29)

#define ADCCMD_DONE		BIT(31)
#define ADCCMD_SMP_CMD(x)	BIT(x)
#define ADCDATA_MASK		0xFFFU

/*
 * TCC ADC Register
 */
#define ADCCON_REG		0x00U
#define ADCTSC_REG		0x04U
#define ADCDLY_REG		0x08U
#define ADCDAT0_REG		0x0CU
#define ADCDAT1_REG		0x10U
#define ADCUPDN_REG		0x14U
#define ADCINTEOC_REG		0x18U
#define ADCINTWKU_REG		0x20U

#define ADCCON_ADCNUM(x)	(((x) & 0xFU) << 18U)
#define ADCCON_12BIT_RES	BIT(17)
#define ADCCON_E_FLG		BIT(16)
#define ADCCON_PS_EN		BIT(15)
#define ADCCON_PS_VAL_MASK	0xFFU
#define ADCCON_PS_VAL(x)	(((x) & ADCCON_PS_VAL_MASK) << 7U)
#define ADCCON_ASEL(x)		(((x) & 0xFU) << 3U)
#define ADCCON_STBY		BIT(2)
#define ADCCON_RD_ST		BIT(1)
#define ADCCON_EN_ST		BIT(0)

#define ADCTSC_XY_PST_MASK	0x3U
#define ADCTSC_XY_PST(x)	((x) & ADCTSC_XY_PST_MASK)
#define ADCTSC_AUTO		BIT(2)
#define ADCTSC_PUON		BIT(3)
#define ADCTSC_XPEN		BIT(4)
#define ADCTSC_XMEN		BIT(5)
#define ADCTSC_YPEN		BIT(6)
#define ADCTSC_YMEN		BIT(7)
#define ADCTSC_UDEN		BIT(8)
#define ADCTSC_MASK		0x1FFU

#define ADCDLY_DELAY_MASK	0xFFFFU
#define ADCDLY_DELAY(x)		((x) & ADCDLY_DELAY_MASK)

#define ADCDAT_UPDN		BIT(15)

#define ADCUPDN_UP		BIT(1)
#define ADCUPDN_DOWN		BIT(0)

#define ADCINTEOC_CLR		BIT(0)
#define ADCINTWKU_CLR		BIT(0)

#define ADC_CH0 0
#define ADC_CH1 1
#define ADC_CH2 2
#define ADC_CH3 3
#define ADC_CH4 4
#define ADC_CH5 5
#define ADC_TOUCHSCREEN 6
#define ADC_CH6 6
#define ADC_CH7 7
#define ADC_CH8 8
#define ADC_CH9 9
#define ADC_CH10 10
#define ADC_CH11 11
#define ADC_CH12 12
#define ADC_CH13 13
#define ADC_CH14 14
#define ADC_CH15 15

#endif /* TCC_ADC_H */
