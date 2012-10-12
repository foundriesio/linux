/*
 * l2switch.c
 *
 * Sub-architcture dependant initialization code for the Freescale
 * 5441X and Vybrid L2 Switch module.
 *
 * Copyright 2010-2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/dma-mapping.h>

#include <asm/traps.h>
#include <asm/sizes.h>

#include <mach/clock.h>
#include <mach/fsl_l2_switch.h>
#include <mach/hardware.h>
#include <mach/devices-common.h>


#define ENET_PALR	0x0e4
#define ENET_PAUR	0x0e8

static unsigned char default_mac[ETH_ALEN] = {
	0x00, 0x04, 0x9F, 0x00, 0xB3, 0x49};

/* FEC mac registers set by bootloader */
static int switch_get_mac_addr(unsigned char *mac)
{
	unsigned int value;

	value = readl(MVF_IO_ADDRESS(MVF_FEC_BASE_ADDR) + ENET_PALR);
	mac[2] = value & 0xff;
	mac[3] = (value >> 8) & 0xff;
	mac[4] = (value >> 16) & 0xff;
	mac[5] = (value >> 24) & 0xff;
	value = readl(MVF_IO_ADDRESS(MVF_FEC_BASE_ADDR) + ENET_PAUR);
	mac[0] = (value >> 16) & 0xff;
	mac[1] = (value >> 24) & 0xff;

	return 0;
}

struct platform_device *__init mvf_init_switch(
		struct switch_platform_data pdata)

{
	struct resource res[] = {
		{
			.start  = MVF_L2SWITCH_BASE_ADDR,
			.end    = MVF_L2SWITCH_BASE_ADDR + SZ_4K - 1 ,
			.flags  = IORESOURCE_MEM,
		},
		{
			.start  = MVF_L2SWITCH_MACBASE_ADDR,
			.end    = MVF_L2SWITCH_MACBASE_ADDR + SZ_16K - 1,
			.flags  = IORESOURCE_MEM,
		},
		{
			.start  = MVF_MAC0_BASE_ADDR,
			.end    = MVF_MAC0_BASE_ADDR + SZ_8K - 1,
			.flags  = IORESOURCE_MEM,
		},
		{
			.start  = MVF_INT_ENET_SWITCH,
			.end    = MVF_INT_ENET_SWITCH,
			.flags  = IORESOURCE_IRQ,
		},
	};

	switch_get_mac_addr(pdata.mac);
	if (!is_valid_ether_addr(pdata.mac))
		memcpy(pdata.mac, default_mac, ETH_ALEN);

	return imx_add_platform_device_dmamask("switch", 0,
			res, ARRAY_SIZE(res),
			&pdata, sizeof(pdata), DMA_BIT_MASK(32));
}
