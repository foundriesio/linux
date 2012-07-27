/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <mach/hardware.h>
#include <mach/devices-common.h>

#define mvf_sai_data_entry_single(soc, id, size)			\
	{								\
		.iobase = soc ## _SAI ## id ## _BASE_ADDR,		\
		.iosize = size,						\
		.irq = soc ## _INT_SAI ## id,				\
		.dmatx0 = DMA_MUX03_SAI ## id ## _TX,			\
		.dmarx0 = DMA_MUX03_SAI ## id ## _RX,			\
	}

const struct mvf_sai_data mvfa5_sai_data[] __initconst = {
	mvf_sai_data_entry_single(MVF, 0, SZ_4K),
	mvf_sai_data_entry_single(MVF, 1, SZ_4K),
	mvf_sai_data_entry_single(MVF, 2, SZ_4K),
};

struct platform_device *__init mvf_add_sai(
		int id,
		const struct mvf_sai_data *data,
		const struct mvf_sai_platform_data *pdata)
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
#define DMARES(_name) {							\
	.name = #_name,							\
	.start = data->dma ## _name,					\
	.end = data->dma ## _name,					\
	.flags = IORESOURCE_DMA,					\
}
		DMARES(tx0),
		DMARES(rx0),
	};

	return imx_add_platform_device("mvf-sai", data->id,
			res, ARRAY_SIZE(res),
			pdata, sizeof(*pdata));
}
