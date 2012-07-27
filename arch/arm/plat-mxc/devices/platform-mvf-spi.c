/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <mach/hardware.h>
#include <mach/devices-common.h>

#define mvf_spi_data_entry_single(soc, type, _devid, _id, _size)	\
	{								\
		.devid = _devid,					\
		.id = _id,						\
		.iobase = soc ## _ ## type ## _id ## _BASE_ADDR,	\
		.iosize = _size,					\
		.irq = soc ## _INT_ ## type ## _id,			\
	}

#define mvf_spi_data_entry(soc, type, devid, id, size)			\
	[id] = mvf_spi_data_entry_single(soc, type, devid, id, size)

#ifdef CONFIG_SOC_MVFA5
const struct imx_spi_imx_data mvf_dspi_data[] __initconst = {
#define mvf_dspi_data_entry(_id)					\
	mvf_spi_data_entry(MVF, DSPI, "mvf-dspi", _id, SZ_4K)
	mvf_dspi_data_entry(0),
	mvf_dspi_data_entry(1),
	mvf_dspi_data_entry(2),
	mvf_dspi_data_entry(3),
};
#endif /* ifdef CONFIG_SOC_MVF */

struct platform_device *__init mvf_add_spi_mvf(
		const struct imx_spi_imx_data *data,
		const struct spi_mvf_master *pdata)
{
	struct resource res[] = {
		{
			.start = data->iobase,
			.end = data->iobase + data->iosize - 1,
			.flags = IORESOURCE_MEM,
		}, {
			.start = data->irq,
			.end = data->irq,
			.flags = IORESOURCE_IRQ,
		},
	};
	return imx_add_platform_device(data->devid, data->id,
			res, ARRAY_SIZE(res), pdata, sizeof(*pdata));
}
