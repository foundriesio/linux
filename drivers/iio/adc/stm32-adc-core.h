/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of STM32 ADC driver
 *
 * Copyright (C) 2016, STMicroelectronics - All Rights Reserved
 * Author: Fabrice Gasnier <fabrice.gasnier@st.com>.
 *
 */

#ifndef __STM32_ADC_H
#define __STM32_ADC_H

/*
 * STM32 - ADC global register map
 * ________________________________________________________
 * | Offset |                 Register                    |
 * --------------------------------------------------------
 * | 0x000  |                Master ADC1                  |
 * --------------------------------------------------------
 * | 0x100  |                Slave ADC2                   |
 * --------------------------------------------------------
 * | 0x200  |                Slave ADC3                   |
 * --------------------------------------------------------
 * | 0x300  |         Master & Slave common regs          |
 * --------------------------------------------------------
 */
#define STM32_ADC_MAX_ADCS		3
#define STM32_ADC_OFFSET		0x100
#define STM32_ADCX_COMN_OFFSET		0x300

/**
 * struct stm32_adc_common - stm32 ADC driver common data (for all instances)
 * @base:		control registers base cpu addr
 * @phys_base:		control registers base physical addr
 * @rate:		clock rate used for analog circuitry
 * @vref_mv:		vref voltage (mv)
 *
 * Reserved variables for child devices, shared between regular/injected:
 * @difsel		bitmask array to set single-ended/differential channel
 * @pcsel		bitmask array to preselect channels on some devices
 * @smpr_val:		sampling time settings (e.g. smpr1 / smpr2)
 */
struct stm32_adc_common {
	void __iomem			*base;
	phys_addr_t			phys_base;
	unsigned long			rate;
	int				vref_mv;
	u32				difsel[STM32_ADC_MAX_ADCS];
	u32				pcsel[STM32_ADC_MAX_ADCS];
	u32				smpr_val[STM32_ADC_MAX_ADCS][2];
	int				prepcnt[STM32_ADC_MAX_ADCS];
	struct mutex			inj[STM32_ADC_MAX_ADCS]; /* injected */
};

#endif
