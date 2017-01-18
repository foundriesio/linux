/*
 * Copyright 2016 Toradex AG
 * Dominik Sliwa <dominik.sliwa@toradex.com>
 *
 * based on an driver for MC13xxx by:
 * Copyright 2009-2010 Pengutronix
 * Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/apalis-tk1-k20.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "apalis-tk1-k20-ezp.h"

static const struct spi_device_id apalis_tk1_k20_device_ids[] = {
	{
		.name = "apalis-tk1-k20",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(spi, apalis_tk1_k20_device_ids);

static const struct of_device_id apalis_tk1_k20_dt_ids[] = {
	{
		.compatible = "toradex,apalis-tk1-k20",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, apalis_tk1_k20_dt_ids);

static const struct regmap_config apalis_tk1_k20_regmap_spi_config = {
	.reg_bits = 8,
	.pad_bits = 0,
	.val_bits = 8,

	.max_register = APALIS_TK1_K20_NUMREGS,

	.cache_type = REGCACHE_NONE,

	.use_single_rw = 1,
};

static int apalis_tk1_k20_spi_read(void *context, const void *reg,
		size_t reg_size, void *val, size_t val_size)
{
	unsigned char w[3] = {APALIS_TK1_K20_READ_INST,
			*((unsigned char *) reg), 0};
	unsigned char r[3];
	unsigned char *p = val;
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);
	struct spi_transfer t = {
		.tx_buf = w,
		.rx_buf = r,
		.len = 3,
	};
	struct spi_message m;
	int ret;

	spi->mode = SPI_MODE_1;
	if (val_size != 1 || reg_size != 1)
		return -ENOTSUPP;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spi_sync(spi, &m);
	spi_message_init(&m);
	t.len = 3;
	spi_message_add_tail(&t, &m);
	ret = spi_sync(spi, &m);

	dev_vdbg(dev, "Apalis TK1 K20 MFD SPI read reg 0x%X: 0x%X 0x%X\n",
			*((unsigned char *) reg), r[1], r[2]);

	if (r[1] == TK1_K20_SENTINEL)
		memcpy(p, &r[2], 1);
	else
		return -EIO;

	return ret;
}

static int apalis_tk1_k20_spi_write(void *context, const void *data,
		size_t count)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);
	uint8_t out_data[3];

	spi->mode = SPI_MODE_1;

	if (count != 2) {
		dev_err(dev, "Apalis TK1 K20 MFD write count = %d\n", count);
		return -ENOTSUPP;
	}
	out_data[0] = APALIS_TK1_K20_WRITE_INST;
	out_data[1] = ((uint8_t *)data)[0];
	out_data[2] = ((uint8_t *)data)[1];
	dev_vdbg(dev, "Apalis TK1 K20 MFD SPI write 0x%X 0x%X\n", out_data[1],
			out_data[2]);

	return spi_write(spi, out_data, 3);
}

static struct regmap_bus regmap_apalis_tk1_k20_bus = {
	.write = apalis_tk1_k20_spi_write,
	.read = apalis_tk1_k20_spi_read,
};

void apalis_tk1_k20_lock(struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	if (!mutex_trylock(&apalis_tk1_k20->lock)) {
		dev_dbg(apalis_tk1_k20->dev, "wait for %s from %ps\n",
				__func__, __builtin_return_address(0));

		mutex_lock(&apalis_tk1_k20->lock);
	}
	dev_dbg(apalis_tk1_k20->dev, "%s from %ps\n",
			__func__, __builtin_return_address(0));
}
EXPORT_SYMBOL(apalis_tk1_k20_lock);

void apalis_tk1_k20_unlock(struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	dev_dbg(apalis_tk1_k20->dev, "%s from %ps\n",
			__func__, __builtin_return_address(0));
	mutex_unlock(&apalis_tk1_k20->lock);
}
EXPORT_SYMBOL(apalis_tk1_k20_unlock);

int apalis_tk1_k20_reg_read(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		unsigned int offset, u32 *val)
{
	int ret;

	ret = regmap_read(apalis_tk1_k20->regmap, offset, val);
	dev_vdbg(apalis_tk1_k20->dev, "[0x%02x] -> 0x%06x\n", offset, *val);

	return ret;
}
EXPORT_SYMBOL(apalis_tk1_k20_reg_read);

int apalis_tk1_k20_reg_write(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		unsigned int offset, u32 val)
{
	dev_vdbg(apalis_tk1_k20->dev, "[0x%02x] <- 0x%06x\n", offset, val);

	if (val >= BIT(24))
		return -EINVAL;

	return regmap_write(apalis_tk1_k20->regmap, offset, val);
}
EXPORT_SYMBOL(apalis_tk1_k20_reg_write);

int apalis_tk1_k20_reg_read_bulk(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		unsigned int offset,
		uint8_t *val, size_t size)
{
	int ret;

	if (size > APALIS_TK1_K20_MAX_BULK)
		return -EINVAL;

	ret = regmap_bulk_read(apalis_tk1_k20->regmap, offset, val, size);
	dev_vdbg(apalis_tk1_k20->dev, "bulk read %d bytes from [0x%02x]\n",
			size, offset);

	return ret;
}
EXPORT_SYMBOL(apalis_tk1_k20_reg_read_bulk);

int apalis_tk1_k20_reg_write_bulk(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		unsigned int offset, uint8_t *buf, size_t size)
{
	dev_vdbg(apalis_tk1_k20->dev, "bulk write %d bytes to [0x%02x]\n",
			(unsigned int)size, offset);

	if (size > APALIS_TK1_K20_MAX_BULK)
		return -EINVAL;

	return regmap_bulk_write(apalis_tk1_k20->regmap, offset, buf, size);
}
EXPORT_SYMBOL(apalis_tk1_k20_reg_write_bulk);

int apalis_tk1_k20_reg_rmw(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		unsigned int offset, u32 mask, u32 val)
{
	dev_vdbg(apalis_tk1_k20->dev, "[0x%02x] <- 0x%06x (mask: 0x%06x)\n",
			offset, val, mask);

	return regmap_update_bits(apalis_tk1_k20->regmap, offset, mask, val);
}
EXPORT_SYMBOL(apalis_tk1_k20_reg_rmw);

int apalis_tk1_k20_irq_mask(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		int irq)
{
	int virq = regmap_irq_get_virq(apalis_tk1_k20->irq_data, irq);

	disable_irq_nosync(virq);

	return 0;
}
EXPORT_SYMBOL(apalis_tk1_k20_irq_mask);

int apalis_tk1_k20_irq_unmask(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		int irq)
{
	int virq = regmap_irq_get_virq(apalis_tk1_k20->irq_data, irq);

	enable_irq(virq);

	return 0;
}
EXPORT_SYMBOL(apalis_tk1_k20_irq_unmask);

int apalis_tk1_k20_irq_status(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		int irq, int *enabled, int *pending)
{
	int ret;
	unsigned int offmask = APALIS_TK1_K20_MSQREG;
	unsigned int offstat = APALIS_TK1_K20_IRQREG;
	u32 irqbit = 1 << irq;

	if (irq < 0 || irq >= ARRAY_SIZE(apalis_tk1_k20->irqs))
		return -EINVAL;

	if (enabled) {
		u32 mask;

		ret = apalis_tk1_k20_reg_read(apalis_tk1_k20, offmask, &mask);
		if (ret)
			return ret;

		*enabled = mask & irqbit;
	}

	if (pending) {
		u32 stat;

		ret = apalis_tk1_k20_reg_read(apalis_tk1_k20, offstat, &stat);
		if (ret)
			return ret;

		*pending = stat & irqbit;
	}

	return 0;
}
EXPORT_SYMBOL(apalis_tk1_k20_irq_status);

int apalis_tk1_k20_irq_request(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		int irq, irq_handler_t handler, const char *name, void *dev)
{
	int virq = regmap_irq_get_virq(apalis_tk1_k20->irq_data, irq);

	return devm_request_threaded_irq(apalis_tk1_k20->dev, virq, NULL,
					 handler, IRQF_ONESHOT, name, dev);
}
EXPORT_SYMBOL(apalis_tk1_k20_irq_request);

int apalis_tk1_k20_irq_free(struct apalis_tk1_k20_regmap *apalis_tk1_k20,
		int irq, void *dev)
{
	int virq = regmap_irq_get_virq(apalis_tk1_k20->irq_data, irq);

	devm_free_irq(apalis_tk1_k20->dev, virq, dev);

	return 0;
}
EXPORT_SYMBOL(apalis_tk1_k20_irq_free);

static int apalis_tk1_k20_add_subdevice_pdata(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20, const char *name,
		void *pdata, size_t pdata_size)
{
	struct mfd_cell cell = {
		.platform_data = pdata,
		.pdata_size = pdata_size,
	};

	cell.name = kmemdup(name, strlen(name) + 1, GFP_KERNEL);
	if (!cell.name)
		return -ENOMEM;

	return mfd_add_devices(apalis_tk1_k20->dev, -1, &cell, 1, NULL, 0,
			       regmap_irq_get_domain(apalis_tk1_k20->irq_data));
}

static int apalis_tk1_k20_add_subdevice(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20, const char *name)
{
	return apalis_tk1_k20_add_subdevice_pdata(apalis_tk1_k20, name, NULL,
						  0);
}

static void apalis_tk1_k20_reset_chip(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	udelay(10);
	gpio_set_value(apalis_tk1_k20->reset_gpio, 0);
	udelay(10);
	gpio_set_value(apalis_tk1_k20->reset_gpio, 1);
	udelay(10);
}

#ifdef CONFIG_APALIS_TK1_K20_EZP
static int apalis_tk1_k20_read_ezport(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20, uint8_t command,
		int addr, int count, uint8_t *buffer)
{
	uint8_t w[4 + APALIS_TK1_K20_EZP_MAX_DATA];
	uint8_t r[4 + APALIS_TK1_K20_EZP_MAX_DATA];
	uint8_t *out;
	struct spi_message m;
	struct spi_device *spi = to_spi_device(apalis_tk1_k20->dev);
	struct spi_transfer t = {
		.tx_buf = w,
		.rx_buf = r,
		.speed_hz = APALIS_TK1_K20_EZP_MAX_SPEED,
	};
	int ret;

	spi->mode = SPI_MODE_0;
	if (count > APALIS_TK1_K20_EZP_MAX_DATA)
		return -ENOSPC;

	memset(w, 0, 4 + count);

	switch (command) {
	case APALIS_TK1_K20_EZP_READ:
	case APALIS_TK1_K20_EZP_FREAD:
		t.len = 4 + count;
		w[1] = (addr & 0xFF0000) >> 16;
		w[2] = (addr & 0xFF00) >> 8;
		w[3] = (addr & 0xFC);
		out = &r[4];
		break;
	case APALIS_TK1_K20_EZP_RDSR:
	case APALIS_TK1_K20_EZP_FRDFCOOB:
		t.len = 1 + count;
		out = &r[1];
		break;
	default:
		return -EINVAL;
	}
	w[0] = command;

	gpio_set_value(apalis_tk1_k20->ezpcs_gpio, 0);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spi_sync(spi, &m);
	gpio_set_value(apalis_tk1_k20->ezpcs_gpio, 1);
	if (ret != 0)
		return ret;

	memcpy(buffer, out, count);

	return 0;
}

static int apalis_tk1_k20_write_ezport(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20, uint8_t command,
		int addr, int count, const uint8_t *buffer)
{
	uint8_t w[4 + APALIS_TK1_K20_EZP_MAX_DATA];
	uint8_t r[4 + APALIS_TK1_K20_EZP_MAX_DATA];
	struct spi_message m;
	struct spi_device *spi = to_spi_device(apalis_tk1_k20->dev);
	struct spi_transfer t = {
		.tx_buf = w,
		.rx_buf = r,
		.speed_hz = APALIS_TK1_K20_EZP_MAX_SPEED,
	};
	int ret;

	spi->mode = SPI_MODE_0;
	if (count > APALIS_TK1_K20_EZP_MAX_DATA)
		return -ENOSPC;

	switch (command) {
	case APALIS_TK1_K20_EZP_SP:
	case APALIS_TK1_K20_EZP_SE:
		t.len = 4 + count;
		w[1] = (addr & 0xFF0000) >> 16;
		w[2] = (addr & 0xFF00) >> 8;
		w[3] = (addr & 0xF8);
		memcpy(&w[4], buffer, count);
		break;
	case APALIS_TK1_K20_EZP_WREN:
	case APALIS_TK1_K20_EZP_WRDI:
	case APALIS_TK1_K20_EZP_BE:
	case APALIS_TK1_K20_EZP_RESET:
	case APALIS_TK1_K20_EZP_WRFCCOB:
		t.len = 1 + count;
		memcpy(&w[1], buffer, count);
		break;
	default:
		return -EINVAL;
	}
	w[0] = command;

	gpio_set_value(apalis_tk1_k20->ezpcs_gpio, 0);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spi_sync(spi, &m);
	gpio_set_value(apalis_tk1_k20->ezpcs_gpio, 1);

	return ret;
}

static int apalis_tk1_k20_set_wren_ezport(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	uint8_t buffer;

	if (apalis_tk1_k20_write_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_WREN,
			0, 0, NULL) < 0)
		return -EIO;
	if (apalis_tk1_k20_read_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_RDSR,
			0, 1, &buffer) < 0)
		return -EIO;
	if ((buffer & APALIS_TK1_K20_EZP_STA_WEN))
		return 0;

	/* If it failed try one last time */
	if (apalis_tk1_k20_write_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_WREN,
			0, 0, NULL) < 0)
		return -EIO;
	if (apalis_tk1_k20_read_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_RDSR,
			0, 1, &buffer) < 0)
		return -EIO;
	if ((buffer & APALIS_TK1_K20_EZP_STA_WEN))
		return 0;

	return -EIO;

}

static int apalis_tk1_k20_enter_ezport(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	uint8_t status = 0x00;
	uint8_t buffer;
	gpio_set_value(apalis_tk1_k20->ezpcs_gpio, 0);
	apalis_tk1_k20_reset_chip(apalis_tk1_k20);
	msleep(30);
	gpio_set_value(apalis_tk1_k20->ezpcs_gpio, 1);
	if (apalis_tk1_k20_read_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_RDSR,
			0, 1, &buffer) < 0)
		goto bad;
	status = buffer;
	if (apalis_tk1_k20_write_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_WREN,
			0, 0, NULL) < 0)
		goto bad;
	if (apalis_tk1_k20_read_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_RDSR,
			0, 1, &buffer) < 0)
		goto bad;
	if ((buffer & APALIS_TK1_K20_EZP_STA_WEN) && buffer != status)
		return 0;

bad:
	dev_err(apalis_tk1_k20->dev, "Error entering EZ Port mode.\n");
	return -EIO;
}

static int apalis_tk1_k20_erase_chip_ezport(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	uint8_t buffer[16];
	int i;
	if (apalis_tk1_k20_set_wren_ezport(apalis_tk1_k20))
		goto bad;
	if (apalis_tk1_k20_write_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_BE,
			0, 0, NULL) < 0)
		goto bad;

	if (apalis_tk1_k20_read_ezport(apalis_tk1_k20, APALIS_TK1_K20_EZP_RDSR,
			0, 1, buffer) < 0)
		goto bad;

	i = 0;
	while (buffer[0] & APALIS_TK1_K20_EZP_STA_WIP) {
		msleep(20);
		if ((apalis_tk1_k20_read_ezport(apalis_tk1_k20,
		     APALIS_TK1_K20_EZP_RDSR, 0, 1, buffer) < 0) || (i > 50))
			goto bad;
		i++;
	}

	return 0;

bad:
	dev_err(apalis_tk1_k20->dev, "Error erasing the chip.\n");
	return -EIO;
}

static int apalis_tk1_k20_flash_chip_ezport(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	uint8_t buffer;
	const uint8_t *fw_chunk;
	int i, j, transfer_size;

	for (i = 0; i < fw_entry->size;) {
		if (apalis_tk1_k20_set_wren_ezport(apalis_tk1_k20))
			goto bad;

		fw_chunk = fw_entry->data + i;
		transfer_size = (i + APALIS_TK1_K20_EZP_WRITE_SIZE <
				 fw_entry->size) ? APALIS_TK1_K20_EZP_WRITE_SIZE
				 : (fw_entry->size - i - 1);
		dev_vdbg(apalis_tk1_k20->dev,
			 "Apalis TK1 K20 MFD transfer_size = %d addr = 0x%X\n",
				 transfer_size, i);
		if (apalis_tk1_k20_write_ezport(apalis_tk1_k20,
				APALIS_TK1_K20_EZP_SP, i,
			transfer_size, fw_chunk) < 0)
			goto bad;

		if (apalis_tk1_k20_read_ezport(apalis_tk1_k20,
				APALIS_TK1_K20_EZP_RDSR, 0, 1, &buffer) < 0)
			goto bad;

		j = 0;
		while (buffer & APALIS_TK1_K20_EZP_STA_WIP) {
			udelay(100);
			if ((apalis_tk1_k20_read_ezport(apalis_tk1_k20,
					APALIS_TK1_K20_EZP_RDSR, 0, 1,
					&buffer) < 0) || (j > 1000))
				goto bad;
			j++;
		}
		i += APALIS_TK1_K20_EZP_WRITE_SIZE;
	}

	return 0;

bad:
	dev_err(apalis_tk1_k20->dev, "Error writing to the chip.\n");
	return -EIO;
}

static uint8_t apalis_tk1_k20_fw_ezport_status(void)
{
	return fw_entry->data[APALIS_TK1_K20_FW_FOPT_ADDR] &
			APALIS_TK1_K20_FOPT_EZP_ENA;
}

static uint8_t apalis_tk1_k20_get_fw_revision(void)
{
	if (fw_entry->size > APALIS_TK1_K20_FW_VER_ADDR)
		return fw_entry->data[APALIS_TK1_K20_FW_VER_ADDR];
	return 0;
}
#endif /* CONFIG_APALIS_TK1_K20_EZP */


#ifdef CONFIG_OF
static int apalis_tk1_k20_probe_flags_dt(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	struct device_node *np = apalis_tk1_k20->dev->of_node;

	if (!np)
		return -ENODEV;

	if (of_property_read_bool(np, "toradex,apalis-tk1-k20-uses-adc"))
		apalis_tk1_k20->flags |= APALIS_TK1_K20_USES_ADC;

	if (of_property_read_bool(np, "toradex,apalis-tk1-k20-uses-tsc"))
		apalis_tk1_k20->flags |= APALIS_TK1_K20_USES_TSC;

	if (of_property_read_bool(np, "toradex,apalis-tk1-k20-uses-can"))
		apalis_tk1_k20->flags |= APALIS_TK1_K20_USES_CAN;

	if (of_property_read_bool(np, "toradex,apalis-tk1-k20-uses-gpio"))
		apalis_tk1_k20->flags |= APALIS_TK1_K20_USES_GPIO;

	return 0;
}

static int apalis_tk1_k20_probe_gpios_dt(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	struct device_node *np = apalis_tk1_k20->dev->of_node;

	if (!np)
		return -ENODEV;

	apalis_tk1_k20->reset_gpio = of_get_named_gpio(np, "rst-gpio", 0);
	if (apalis_tk1_k20->reset_gpio < 0)
		return apalis_tk1_k20->reset_gpio;
	gpio_request(apalis_tk1_k20->reset_gpio, "apalis-tk1-k20-reset");
	gpio_direction_output(apalis_tk1_k20->reset_gpio, 1);

	apalis_tk1_k20->ezpcs_gpio = of_get_named_gpio(np, "ezport-cs-gpio", 0);
	if (apalis_tk1_k20->ezpcs_gpio < 0)
		return apalis_tk1_k20->ezpcs_gpio;
	gpio_request(apalis_tk1_k20->ezpcs_gpio, "apalis-tk1-k20-ezpcs");
	gpio_direction_output(apalis_tk1_k20->ezpcs_gpio, 1);

	apalis_tk1_k20->appcs_gpio = of_get_named_gpio(np, "spi-cs-gpio", 0);
	if (apalis_tk1_k20->appcs_gpio < 0)
		return apalis_tk1_k20->appcs_gpio;
	gpio_request(apalis_tk1_k20->appcs_gpio, "apalis-tk1-k20-appcs");
	gpio_direction_output(apalis_tk1_k20->appcs_gpio, 1);

	apalis_tk1_k20->int2_gpio = of_get_named_gpio(np, "int2-gpio", 0);
	if (apalis_tk1_k20->int2_gpio < 0)
		return apalis_tk1_k20->int2_gpio;
	gpio_request(apalis_tk1_k20->int2_gpio, "apalis-tk1-k20-int2");
	gpio_direction_output(apalis_tk1_k20->int2_gpio, 1);

	return 0;
}
#else
static inline int apalis_tk1_k20_probe_flags_dt(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	return -ENODEV;
}
static inline int apalis_tk1_k20_probe_gpios_dt(
		struct apalis_tk1_k20_regmap *apalis_tk1_k20)
{
	return -ENODEV;
}
#endif

int apalis_tk1_k20_dev_init(struct device *dev)
{
	struct apalis_tk1_k20_platform_data *pdata = dev_get_platdata(dev);
	struct apalis_tk1_k20_regmap *apalis_tk1_k20 = dev_get_drvdata(dev);
	uint32_t revision = 0x00;
	int ret, i;
	int erase_only = 0;

	apalis_tk1_k20->dev = dev;

	ret = apalis_tk1_k20_probe_gpios_dt(apalis_tk1_k20);
	if ((ret < 0) && pdata) {
		if (pdata) {
			apalis_tk1_k20->ezpcs_gpio = pdata->ezpcs_gpio;
			apalis_tk1_k20->reset_gpio = pdata->reset_gpio;
			apalis_tk1_k20->appcs_gpio = pdata->appcs_gpio;
			apalis_tk1_k20->int2_gpio = pdata->int2_gpio;
		} else {
			dev_err(dev, "Error claiming GPIOs\n");
			ret = -EINVAL;
			goto bad;
		}
	}

	apalis_tk1_k20_reset_chip(apalis_tk1_k20);
	msleep(100);
	gpio_set_value(apalis_tk1_k20->appcs_gpio, 0);
	ret = apalis_tk1_k20_reg_read(apalis_tk1_k20, APALIS_TK1_K20_REVREG,
			&revision);

#ifdef CONFIG_APALIS_TK1_K20_EZP
	if ((request_firmware(&fw_entry, "apalis-tk1-k20.bin", dev) < 0)
		&& (revision != APALIS_TK1_K20_FW_VER)) {
			dev_err(apalis_tk1_k20->dev,
				"Unsupported firmware version %d.%d and no local"
				" firmware file available.\n",
				(revision & 0xF0 >> 8),
				(revision & 0x0F));
			ret = -ENOTSUPP;
			goto bad;
	}

	if (fw_entry->size == 1)
		erase_only = 1;

	if ((apalis_tk1_k20_get_fw_revision() != APALIS_TK1_K20_FW_VER) &&
		(revision != APALIS_TK1_K20_FW_VER) && !erase_only) {
		dev_err(apalis_tk1_k20->dev,
			"Unsupported firmware version in both the device as well"
			" as the local firmware file.\n");
		release_firmware(fw_entry);
		ret = -ENOTSUPP;
		goto bad;
	}

	if ((revision != APALIS_TK1_K20_FW_VER) && !erase_only &&
			(!apalis_tk1_k20_fw_ezport_status())) {
		dev_err(apalis_tk1_k20->dev,
			"Unsupported firmware version in the device and the "
			"local firmware file disables the EZ Port.\n");
		release_firmware(fw_entry);
		ret = -ENOTSUPP;
		goto bad;
	}

	if ((revision != APALIS_TK1_K20_FW_VER) || erase_only) {
		if (apalis_tk1_k20_enter_ezport(apalis_tk1_k20) < 0) {
			dev_err(apalis_tk1_k20->dev,
				"Problem entering EZ port mode.\n");
			release_firmware(fw_entry);
			ret = -EIO;
			goto bad;
		}
		if (apalis_tk1_k20_erase_chip_ezport(apalis_tk1_k20) < 0) {
			dev_err(apalis_tk1_k20->dev,
				"Problem erasing the chip.\n");
			release_firmware(fw_entry);
			ret = -EIO;
			goto bad;
		}
		if (erase_only) {
			dev_err(apalis_tk1_k20->dev,
				"Chip fully erased.\n");
			release_firmware(fw_entry);
			ret = -EIO;
			goto bad;
		}
		if (apalis_tk1_k20_flash_chip_ezport(apalis_tk1_k20) < 0) {
			dev_err(apalis_tk1_k20->dev,
				"Problem flashing new firmware.\n");
			release_firmware(fw_entry);
			ret = -EIO;
			goto bad;
		}
	}
	release_firmware(fw_entry);

	gpio_set_value(apalis_tk1_k20->appcs_gpio, 1);
	apalis_tk1_k20_reset_chip(apalis_tk1_k20);
	msleep(100);
	gpio_set_value(apalis_tk1_k20->appcs_gpio, 0);

	ret = apalis_tk1_k20_reg_read(apalis_tk1_k20, APALIS_TK1_K20_REVREG,
			&revision);
#endif /* CONFIG_APALIS_TK1_K20_EZP */

	if (ret) {
		dev_err(apalis_tk1_k20->dev, "Device is not answering.\n");
		goto bad;
	}

	if (revision != APALIS_TK1_K20_FW_VER) {
		dev_err(apalis_tk1_k20->dev,
			"Unsupported firmware version %d.%d.\n",
			((revision & 0xF0) >> 8), (revision & 0x0F));
		ret = -ENOTSUPP;
		goto bad;
	}

	for (i = 0; i < ARRAY_SIZE(apalis_tk1_k20->irqs); i++) {
		apalis_tk1_k20->irqs[i].reg_offset = i /
						     APALIS_TK1_K20_IRQ_PER_REG;
		apalis_tk1_k20->irqs[i].mask = BIT(i %
						   APALIS_TK1_K20_IRQ_PER_REG);
	}

	apalis_tk1_k20->irq_chip.name		= dev_name(dev);
	apalis_tk1_k20->irq_chip.status_base	= APALIS_TK1_K20_IRQREG;
	apalis_tk1_k20->irq_chip.mask_base	= APALIS_TK1_K20_MSQREG;
	apalis_tk1_k20->irq_chip.ack_base	= APALIS_TK1_K20_IRQREG;
	apalis_tk1_k20->irq_chip.irq_reg_stride	= 0;
	apalis_tk1_k20->irq_chip.num_regs	= APALIS_TK1_K20_IRQ_REG_CNT;
	apalis_tk1_k20->irq_chip.irqs		= apalis_tk1_k20->irqs;
	apalis_tk1_k20->irq_chip.num_irqs = ARRAY_SIZE(apalis_tk1_k20->irqs);

	ret = regmap_add_irq_chip(apalis_tk1_k20->regmap, apalis_tk1_k20->irq,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING |
			IRQF_TRIGGER_RISING, 0, &apalis_tk1_k20->irq_chip,
			&apalis_tk1_k20->irq_data);
	if (ret)
		goto bad;

	mutex_init(&apalis_tk1_k20->lock);

	if (apalis_tk1_k20_probe_flags_dt(apalis_tk1_k20) < 0 && pdata)
		apalis_tk1_k20->flags = pdata->flags;

	if (pdata) {
		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_TSC)
			apalis_tk1_k20_add_subdevice_pdata(apalis_tk1_k20,
					"apalis-tk1-k20-ts",
					&pdata->touch, sizeof(pdata->touch));

		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_ADC)
			apalis_tk1_k20_add_subdevice_pdata(apalis_tk1_k20,
					"apalis-tk1-k20-adc",
					&pdata->adc, sizeof(pdata->adc));

		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_CAN) {
			pdata->can0.id = 0;
			pdata->can1.id = 1;
			apalis_tk1_k20_add_subdevice_pdata(apalis_tk1_k20,
					"apalis-tk1-k20-can",
					&pdata->can0, sizeof(pdata->can0));
			apalis_tk1_k20_add_subdevice_pdata(apalis_tk1_k20,
					"apalis-tk1-k20-can",
					&pdata->can1, sizeof(pdata->can1));
		}
		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_GPIO)
			apalis_tk1_k20_add_subdevice_pdata(apalis_tk1_k20,
					"apalis-tk1-k20-gpio",
					&pdata->gpio, sizeof(pdata->gpio));
	} else {
		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_TSC)
			apalis_tk1_k20_add_subdevice(apalis_tk1_k20,
					"apalis-tk1-k20-ts");

		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_ADC)
			apalis_tk1_k20_add_subdevice(apalis_tk1_k20,
					"apalis-tk1-k20-adc");

		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_CAN) {
			apalis_tk1_k20_add_subdevice(apalis_tk1_k20,
					"apalis-tk1-k20-can");
		}

		if (apalis_tk1_k20->flags & APALIS_TK1_K20_USES_GPIO)
			apalis_tk1_k20_add_subdevice(apalis_tk1_k20,
					"apalis-tk1-k20-gpio");
	}

	dev_info(apalis_tk1_k20->dev, "Apalis TK1 K20 MFD driver.\n"
			"Firmware version %d.%d.\n", FW_MAJOR, FW_MINOR);

	return 0;

bad:
	if (apalis_tk1_k20->ezpcs_gpio >= 0)
		gpio_free(apalis_tk1_k20->ezpcs_gpio);
	if (apalis_tk1_k20->reset_gpio >= 0)
		gpio_free(apalis_tk1_k20->reset_gpio);
	if (apalis_tk1_k20->appcs_gpio >= 0)
		gpio_free(apalis_tk1_k20->appcs_gpio);
	if (apalis_tk1_k20->int2_gpio >= 0)
		gpio_free(apalis_tk1_k20->int2_gpio);

	return ret;
}


static int apalis_tk1_k20_spi_probe(struct spi_device *spi)
{
	struct apalis_tk1_k20_regmap *apalis_tk1_k20;
	int ret;

	apalis_tk1_k20 = devm_kzalloc(&spi->dev, sizeof(*apalis_tk1_k20),
			GFP_KERNEL);
	if (!apalis_tk1_k20)
		return -ENOMEM;

	dev_set_drvdata(&spi->dev, apalis_tk1_k20);

	spi->mode = SPI_MODE_1;

	apalis_tk1_k20->irq = spi->irq;

	spi->max_speed_hz = (spi->max_speed_hz > APALIS_TK1_K20_MAX_SPI_SPEED) ?
			APALIS_TK1_K20_MAX_SPI_SPEED : spi->max_speed_hz;

	ret = spi_setup(spi);
	if (ret)
		return ret;

	apalis_tk1_k20->regmap = devm_regmap_init(&spi->dev,
			&regmap_apalis_tk1_k20_bus, &spi->dev,
			&apalis_tk1_k20_regmap_spi_config);
	if (IS_ERR(apalis_tk1_k20->regmap)) {
		ret = PTR_ERR(apalis_tk1_k20->regmap);
		dev_err(&spi->dev, "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	return apalis_tk1_k20_dev_init(&spi->dev);
}

static int apalis_tk1_k20_spi_remove(struct spi_device *spi)
{
	struct apalis_tk1_k20_regmap *apalis_tk1_k20 =
			dev_get_drvdata(&spi->dev);

	if (apalis_tk1_k20->ezpcs_gpio >= 0)
		gpio_free(apalis_tk1_k20->ezpcs_gpio);
	if (apalis_tk1_k20->reset_gpio >= 0)
		gpio_free(apalis_tk1_k20->reset_gpio);
	if (apalis_tk1_k20->appcs_gpio >= 0)
		gpio_free(apalis_tk1_k20->appcs_gpio);
	if (apalis_tk1_k20->int2_gpio >= 0)
		gpio_free(apalis_tk1_k20->int2_gpio);

	mfd_remove_devices(&spi->dev);
	regmap_del_irq_chip(apalis_tk1_k20->irq, apalis_tk1_k20->irq_data);
	mutex_destroy(&apalis_tk1_k20->lock);

	return 0;
}

static struct spi_driver apalis_tk1_k20_spi_driver = {
	.id_table = apalis_tk1_k20_device_ids,
	.driver = {
		.name = "apalis-tk1-k20",
		.of_match_table = apalis_tk1_k20_dt_ids,
	},
	.probe = apalis_tk1_k20_spi_probe,
	.remove = apalis_tk1_k20_spi_remove,
};

static int __init apalis_tk1_k20_init(void)
{
	return spi_register_driver(&apalis_tk1_k20_spi_driver);
}
subsys_initcall(apalis_tk1_k20_init);

static void __exit apalis_tk1_k20_exit(void)
{
	spi_unregister_driver(&apalis_tk1_k20_spi_driver);
}
module_exit(apalis_tk1_k20_exit);

MODULE_DESCRIPTION("MFD driver for Kinetis MK20DN512 MCU on Apalis TK1");
MODULE_AUTHOR("Dominik Sliwa <dominik.sliwa@toradex.com>");
MODULE_LICENSE("GPL v2");
