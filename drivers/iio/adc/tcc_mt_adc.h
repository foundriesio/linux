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

#define PMU_TSADC0_STOP_SHIFT 4
#define PMU_TSADC1_STOP_SHIFT 16

#define ADC0CON_REG			0x00
#define ADC0DATA_REG		0x04
#define ADC0MODE_REG		0x08
#define ADCMASK2_REG		0x10
#define ADC0INTEN_REG		0x18
#define ADC0BUF0_REG		0x40 //0x40 ~ 0x5C
#define ADC0BUF1_REG		0x44
#define ADC0BUF2_REG		0x48
#define ADC0BUF3_REG		0x4C
#define ADC0BUF4_REG		0x50
#define ADC0BUF5_REG		0x54
#define ADC0BUF6_REG		0x58
#define ADC0BUF7_REG		0x5C
#define ADC0BUF_SEL(x)		(ADC0BUF0_REG+(0x4*(x)))
#define ADC0CONA_REG		0x80
#define ADC0STATUS_REG		0x84
#define ADC0CFG_REG			0x88
#define ADC0CFG2_REG		0x8C
#define ADC0DMASK_REG		0x90
#define ADC0EVTSEL_REG		0x94
#define ADC0EVTCNTV_REG		0x98
#define ADC0EIRQSTAT_REG	0x9C
#define ADC1CON_REG			0x100
#define ADC1DATA_REG		0x104
#define ADC1DMASK2_REG		0x110
#define ADC1EVTSEL2_REG		0x114
#define ADC1INTEN_REG		0x118
#define ADC1BUF0_REG		0x140 //0x140 ~ 0x15C
#define ADC1BUF1_REG		0x144
#define ADC1BUF2_REG		0x148
#define ADC1BUF3_REG		0x14C
#define ADC1BUF4_REG		0x150
#define ADC1BUF5_REG		0x154
#define ADC1BUF6_REG		0x158
#define ADC1BUF7_REG		0x15C
#define ADC1BUF8_REG		0x160
#define ADC1BUF9_REG		0x164
#define ADC1BUF_SEL(x)		(ADC1BUF0_REG+(0x4*(x)))
#define ADC1CONA_REG		0x180
#define ADC1STATUS_REG		0x184
#define ADC1CFG_REG			0x188
#define ADC1CFG2_REG		0x18C
#define ADC1DMASK_REG		0x190
#define ADC1EVTSEL_REG		0x194
#define ADC1EVTCNTV_REG		0x198
#define ADC1EIRQSTAT_REG	0x19C

#define ADCCON_ASEL(x)		(((x)&0xF)<<0)
#define ADCCON_STBY			(1<<4)
#define ADCCON_ENABLE		(0<<4)
#define ADCCFG_CLKDIV_MASK  0xFFFFFFFF
#define ADCCFG_CLKDIV(x)    (((x)&ADCCFG_CLKDIV_MASK)<<12)

#define ADCCFG_SM			(1<<0)
#define ADCCFG_APD			(0<<1)
#define ADCCFG_R8			(0<<2)
#define ADCCFG_IRQE			(0<<3)
#define ADCCFG_FIFOTH		(0<<4)
#define ADCCFG_FIFON(x)		(((x)&0x3)<<4)
#define ADCCFG_NEOC			(0<<7)
#define ADCCFG_DLYSTC		(4<<8)
#define ADCCFG_DMAEN		(0<<20)
#define ADCBUF_FWRB			(1<<19)
#define ADCBUF_NRND			(1<<23)



#define ADCCFG2_EIRQEN		(0<<0)
#define ADCCFG2_RCLR		(0<<13)
#define ADCCFG2_RBCLR		(0<<14)
#define ADCCFG2_STPCK		(0<<15)
#define ADCCFG2_STPCK_EN	(1<<15)
#define ADCCFG2_ASMEN		(0<<16)
#define ADCCFG2_ASMEN_EN	(1<<16)
#define ADCCFG2_ECKSEL		(0<<17)
#define ADCCFG2_PSCALE		(4<<20)


#define ADCCON_12BIT_RES	(1<<17)
#define ADCCON_E_FLG		(1<<16)
#define ADCCON_PS_EN		(1<<15)
#define ADCCON_PS_VAL_MASK	0xFF
#define ADCCON_PS_VAL(x)	(((x)&ADCCON_PS_VAL_MASK)<<7)

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

#define ADCDLY_DELAY_MASK	0xFFFF
#define ADCDLY_DELAY(x)		((x)&ADCDLY_DELAY_MASK)

#define ADCDAT0_UPDN		(1<<15)

#define ADCDAT1_UPDN		(1<<15)

#define ADCUPDN_UP		(1<<1)
#define ADCUPDN_DOWN		(1<<0)

#define ADCINTEOC_CLR		(1<<0)
#define ADCINTWKU_CLR		(1<<0)

#define MAX_CH0_ADC_CHANNEL 8
#define MAX_CH1_ADC_CHANNEL 10//8//12//16 MAX CHANNEL : 12, FOR AUTO_SCAN TEST : 8

#define TCC_MT_ADC_CHANNEL(_channel) {             \
	.type = IIO_VOLTAGE,                            \
	.indexed = 1,                                   \
	.channel = _channel,                            \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),   \
	.datasheet_name = "tcc_mt_adc_channel"#_channel,\
}

#endif
