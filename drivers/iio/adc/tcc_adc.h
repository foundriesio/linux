/****************************************************************************
 * tcc_adc.h
 * Copyright (C) 2014 Telechips Inc.
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
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#ifndef __TCC_ADC_H__
#define __TCC_ADC_H__

#define PMU_TSADC_STOP_SHIFT	0
#define PMU_TSADC_PWREN_SHIFT	16

#define ADCCON_REG	0x00
#define ADCTSC_REG	0x04
#define ADCDLY_REG	0x08
#define ADCDAT0_REG	0x0C
#define ADCDAT1_REG	0x10
#define ADCUPDN_REG	0x14
#define ADCINTEOC_REG	0x18
#define ADCINTWKU_REG	0x20

#define ADCCON_ADCNUM(x)	(((x)&0xF)<<18)
#define ADCCON_12BIT_RES	(1<<17)
#define ADCCON_E_FLG		(1<<16)
#define ADCCON_PS_EN		(1<<15)
#define ADCCON_PS_VAL_MASK	0xFF
#define ADCCON_PS_VAL(x)	(((x)&ADCCON_PS_VAL_MASK)<<7)
#define ADCCON_ASEL(x)		(((x)&0xF)<<3)
#define ADCCON_STBY		(1<<2)
#define ADCCON_RD_ST		(1<<1)
#define ADCCON_EN_ST		(1<<0)

#define ADCTSC_XY_PST_MASK	(3<<0)
#define ADCTSC_XY_PST(x)	((x&ADCTSC_XY_PST_MASK)<<0)
#define ADCTSC_AUTO		(1<<2)
#define ADCTSC_PUON		(1<<3)
#define ADCTSC_XPEN		(1<<4)
#define ADCTSC_XMEN		(1<<5)
#define ADCTSC_YPEN		(1<<6)
#define ADCTSC_YMEN		(1<<7)
#define ADCTSC_UDEN		(1<<8)
#define ADCTSC_MASK	    0x1FF

#define ADCDLY_DELAY_MASK	0xFFFF
#define ADCDLY_DELAY(x)		((x)&ADCDLY_DELAY_MASK)

#define ADCDAT0_UPDN		(1<<15)

#define ADCDAT1_UPDN		(1<<15)

#define ADCUPDN_UP		(1<<1)
#define ADCUPDN_DOWN		(1<<0)

#define ADCINTEOC_CLR		(1<<0)
#define ADCINTWKU_CLR		(1<<0)

#define GENERAL_ADC 1
enum {
	ADC_CH0 = 0,
	ADC_CH1,
	ADC_CH2,
	ADC_CH3,
	ADC_CH4,
	ADC_CH5,
#if defined(GENERAL_ADC)
	ADC_CH6,
	ADC_CH7,
	ADC_CH8,
	ADC_CH9,
#else
	ADC_TOUCHSCREEN,
#endif
	ADC_CH10= 10,
	ADC_CH11,
	ADC_CH12,
	ADC_CH13,
	ADC_CH14,
	ADC_CH15,
	ADC_CH_MAX = ADC_CH15,
};

struct tcc_adc_client {
	struct device		*dev;
	struct list_head	list;
	unsigned char		channel;
	unsigned long		data;
};

extern struct tcc_adc_client *tcc_adc_register(struct device *dev, int ch);
extern void tcc_adc_release(struct tcc_adc_client *client);
extern unsigned long tcc_adc_getdata(struct tcc_adc_client *client);
#endif /* __TCC_ADC_H__ */
