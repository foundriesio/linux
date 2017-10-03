/*
 * Copyright 2016-2017 Toradex AG
 * Dominik Sliwa <dominik.sliwa@toradex.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */

#ifndef __LINUX_MFD_APALIS_TK1_K20_H
#define __LINUX_MFD_APALIS_TK1_K20_H

#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/regmap.h>

/* Commands and registers used in SPI communication */

/* Commands*/
#define APALIS_TK1_K20_READ_INST		0x0F
#define APALIS_TK1_K20_WRITE_INST		0xF0
#define APALIS_TK1_K20_BULK_WRITE_INST		0x3C
#define APALIS_TK1_K20_BULK_READ_INST		0xC3

#define APALIS_TK1_K20_MAX_BULK			(32)

/* General registers*/
#define APALIS_TK1_K20_STAREG			0x00 /* general status register RO */
#define APALIS_TK1_K20_REVREG			0x01 /* FW revision register RO*/
#define APALIS_TK1_K20_IRQREG			0x02 /* IRQ status RW(write of 1 will reset the bit) */
#define APALIS_TK1_K20_CTRREG			0x03 /* general control register RW */
#define APALIS_TK1_K20_MSQREG			0x04 /* IRQ mask register RW */

/* 0x05-0x0F Reserved */

/* CAN Registers */
#define APALIS_TK1_K20_CANREG			0x10 /* CAN0 control & status register RW */
#define APALIS_TK1_K20_CANERR			0x11 /* CAN0 error register RW */
#define APALIS_TK1_K20_CAN_BAUD_REG		0x12 /* CAN0 baud set register RW */
#define APALIS_TK1_K20_CAN_BIT_1		0x13 /* CAN0 bit timing register 1 RW */
#define APALIS_TK1_K20_CAN_BIT_2		0x14 /* CAN0 bit timing register 2 RW */
#define APALIS_TK1_K20_CAN_IN_BUF_CNT		0x15 /* CAN0 IN received data count RO */
#define APALIS_TK1_K20_CAN_IN_BUF		0x16 /* CAN0 IN RO */
/* buffer size is 13 bytes */
#define APALIS_TK1_K20_CAN_IN_BUF_END		0x22 /* CAN0 IN RO */
#define APALIS_TK1_K20_CAN_OUT_BUF_CNT		0x23 /* CAN0 OUT data Count WO */
#define APALIS_TK1_K20_CAN_OUT_BUF		0x26 /* CAN0 OUT WO */
/* buffer size is 13 bytes */
#define APALIS_TK1_K20_CAN_OUT_BUF_END		(APALIS_TK1_K20_CAN_OUT_BUF + 13 - 1)/* CAN OUT BUF END */
#define APALIS_TK1_K20_CAN_DEV_OFFSET(x)	(x ? 0x30 : 0)

/* 0x33-0x3F Reserved */
/* 0x40-0x62 CAN1 registers same layout as CAN0*/
/* 0x63-0x6F Reserved */

/* ADC Registers */
#define APALIS_TK1_K20_ADCREG			0x70 /* ADC control & status register RW */
#define APALIS_TK1_K20_ADC_CH0L			0x71 /* ADC Channel 0 LSB RO */
#define APALIS_TK1_K20_ADC_CH0H			0x72 /* ADC Channel 0 MSB RO */
#define APALIS_TK1_K20_ADC_CH1L			0x73 /* ADC Channel 1 LSB RO */
#define APALIS_TK1_K20_ADC_CH1H			0x74 /* ADC Channel 1 MSB RO */
#define APALIS_TK1_K20_ADC_CH2L			0x75 /* ADC Channel 2 LSB RO */
#define APALIS_TK1_K20_ADC_CH2H			0x76 /* ADC Channel 2 MSB RO */
#define APALIS_TK1_K20_ADC_CH3L			0x77 /* ADC Channel 3 LSB RO */
#define APALIS_TK1_K20_ADC_CH3H			0x78 /* ADC Channel 3 MSB RO */
/* Bulk read of LSB register can be use to read entire 16-bit in one command
 * Bulk read of APALIS_TK1_K20_ADC_CH0L register can be use to read all
 * ADC channels in one command
 */

/* 0x79-0x7F reserved */

/* TSC Register */
#define APALIS_TK1_K20_TSCREG			0x80 /* TSC control & status register RW */
#define APALIS_TK1_K20_TSC_XML			0x81 /* TSC X- data LSB RO */
#define APALIS_TK1_K20_TSC_XMH			0x82 /* TSC X- data MSB RO */
#define APALIS_TK1_K20_TSC_XPL			0x83 /* TSC X+ data LSB RO */
#define APALIS_TK1_K20_TSC_XPH			0x84 /* TSC X+ data MSB RO */
#define APALIS_TK1_K20_TSC_YML			0x85 /* TSC Y- data LSB RO */
#define APALIS_TK1_K20_TSC_YMH			0x86 /* TSC Y- data MSB RO */
#define APALIS_TK1_K20_TSC_YPL			0x87 /* TSC Y+ data LSB RO */
#define APALIS_TK1_K20_TSC_YPH			0x88 /* TSC Y+ data MSB RO */
/* Bulk read of LSB register can be use to read entire 16-bit in one command */
#define APALIS_TK1_K20_TSC_ENA			BIT(0)
#define APALIS_TK1_K20_TSC_ENA_MASK		BIT(0)

/* 0x89-0x8F Reserved */

/* GPIO Registers */
#define APALIS_TK1_K20_GPIOREG			0x90 /* GPIO control & status register RW */
#define APALIS_TK1_K20_GPIO_NO			0x91 /* currently configured GPIO RW */
#define APALIS_TK1_K20_GPIO_STA			0x92 /* Status register for the APALIS_TK1_K20_GPIO_NO GPIO RW */
/* MSB | 0 ... 0 | VALUE | Output-1 / Input-0 | LSB  */
#define APALIS_TK1_K20_GPIO_STA_OE		BIT(0)
#define APALIS_TK1_K20_GPIO_STA_VAL		BIT(1)

/* 0x93-0xFD Reserved */
#define APALIS_TK1_K20_RET_REQ			0xFE
/* 0xFF Reserved */

/* Interrupt flags */
#define APALIS_TK1_K20_GEN_IRQ			0
#define APALIS_TK1_K20_CAN0_IRQ			1
#define APALIS_TK1_K20_CAN1_IRQ			2
#define APALIS_TK1_K20_ADC_IRQ			3
#define APALIS_TK1_K20_TSC_IRQ			4
#define APALIS_TK1_K20_GPIO_IRQ			5

#define APALIS_TK1_K20_FW_VER			0x0A

#define FW_MINOR (APALIS_TK1_K20_FW_VER & 0x0F)
#define FW_MAJOR ((APALIS_TK1_K20_FW_VER & 0xF0) >> 4)

#define TK1_K20_SENTINEL			0x55
#define TK1_K20_INVAL				0xAA

#define APALIS_TK1_K20_NUMREGS			0x3f
#define APALIS_TK1_K20_IRQ_REG_CNT		1
#define APALIS_TK1_K20_IRQ_PER_REG		8

#define APALIS_TK1_CAN_CLK_UNIT			6250

#define APALIS_TK1_K20_MAX_SPI_SPEED		6000000

struct apalis_tk1_k20_regmap {
	struct regmap *regmap;

	struct device *dev;

	struct regmap_irq irqs[APALIS_TK1_K20_IRQ_REG_CNT * APALIS_TK1_K20_IRQ_PER_REG];
	struct regmap_irq_chip irq_chip;
	struct regmap_irq_chip_data *irq_data;

	struct mutex lock;
	int irq;
	int flags;

	int ezpcs_gpio;
	int reset_gpio;
	int appcs_gpio;
	int int2_gpio;
};

void apalis_tk1_k20_lock(struct apalis_tk1_k20_regmap *apalis_tk1_k20);
void apalis_tk1_k20_unlock(struct apalis_tk1_k20_regmap *apalis_tk1_k20);

int apalis_tk1_k20_reg_read(struct apalis_tk1_k20_regmap *apalis_tk1_k20, unsigned int offset, u32 *val);
int apalis_tk1_k20_reg_write(struct apalis_tk1_k20_regmap *apalis_tk1_k20, unsigned int offset, u32 val);
int apalis_tk1_k20_reg_read_bulk(struct apalis_tk1_k20_regmap *apalis_tk1_k20, unsigned int offset,
		uint8_t *val, size_t size);
int apalis_tk1_k20_reg_write_bulk(struct apalis_tk1_k20_regmap *apalis_tk1_k20, unsigned int offset,
		uint8_t *val, size_t size);
int apalis_tk1_k20_reg_rmw(struct apalis_tk1_k20_regmap *apalis_tk1_k20, unsigned int offset,
		u32 mask, u32 val);

int apalis_tk1_k20_irq_mask(struct apalis_tk1_k20_regmap *apalis_tk1_k20, int irq);
int apalis_tk1_k20_irq_unmask(struct apalis_tk1_k20_regmap *apalis_tk1_k20, int irq);
int apalis_tk1_k20_irq_request(struct apalis_tk1_k20_regmap *apalis_tk1_k20, int irq,
		irq_handler_t handler, const char *name, void *dev);
int apalis_tk1_k20_irq_free(struct apalis_tk1_k20_regmap *apalis_tk1_k20, int irq, void *dev);

int apalis_tk1_k20_irq_status(struct apalis_tk1_k20_regmap *apalis_tk1_k20, int irq,
		int *enabled, int *pending);

int apalis_tk1_k20_get_flags(struct apalis_tk1_k20_regmap *apalis_tk1_k20);

struct apalis_tk1_k20_can_platform_data {
	uint8_t id;
	u16 status;
};

struct apalis_tk1_k20_tsc_platform_data {
	u16 status;
};

struct apalis_tk1_k20_adc_platform_data {
	u16 status;
};

struct apalis_tk1_k20_gpio_platform_data {
	u16 status;
};

#define APALIS_TK1_K20_USES_TSC		BIT(0)
#define APALIS_TK1_K20_USES_ADC		BIT(1)
#define APALIS_TK1_K20_USES_CAN		BIT(2)
#define APALIS_TK1_K20_USES_GPIO	BIT(3)

struct apalis_tk1_k20_platform_data {
	unsigned int flags;

	struct apalis_tk1_k20_tsc_platform_data touch;
	struct apalis_tk1_k20_adc_platform_data adc;
	struct apalis_tk1_k20_can_platform_data can0;
	struct apalis_tk1_k20_can_platform_data can1;
	struct apalis_tk1_k20_gpio_platform_data gpio;

	int ezpcs_gpio;
	int reset_gpio;
	int appcs_gpio;
	int int2_gpio;
};

#define APALIS_TK1_K20_ADC_CHANNELS	4
#define APALIS_TK1_K20_ADC_BITS		16
#define APALIS_TK1_K20_VADC_MILI	3300

enum apalis_tk1_k20_adc_id {
	APALIS_TK1_K20_ADC1,
	APALIS_TK1_K20_ADC2,
	APALIS_TK1_K20_ADC3,
	APALIS_TK1_K20_ADC4
};

#endif /* ifndef __LINUX_MFD_APALIS_TK1_K20_H */
