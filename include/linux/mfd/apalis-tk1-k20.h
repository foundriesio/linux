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
#include <linux/mfd/apalis-tk1-k20-api.h>

#define APALIS_TK1_MAX_RETRY_CNT		4

#define APALIS_TK1_K20_MAX_SPI_SPEED		6120000

struct apalis_tk1_k20_regmap {
	struct regmap *regmap;

	struct device *dev;

	struct regmap_irq irqs[APALIS_TK1_K20_IRQ_REG_CNT * APALIS_TK1_K20_IRQ_PER_REG];
	struct regmap_irq_chip irq_chip;
	struct regmap_irq_chip_data *irq_data;
	int can0_irq;
	int can1_irq;

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
