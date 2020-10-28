// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the ST ISP1763 chip
 *
 * Copyright 2014 Laurent Pinchart
 * Copyright 2007 Sebastian Siewior
 * Copyright 2013 Richard Retanubu
 *
 */

#ifndef _ISP1763_CORE_H_
#define _ISP1763_CORE_H_

#include <linux/ioport.h>

#include "isp1763-hcd.h"

struct device;
struct gpio_desc;

/*
 * Device flags that can vary from board to board.  All of these
 * indicate the most "atypical" case, so that a devflags of 0 is
 * a sane default configuration.
 */
#define ISP1763_FLAG_BUS_WIDTH_8	0x00000002 /* 8-bit data bus width */
#define ISP1763_FLAG_OTG_EN		0x00000004 /* Port 1 supports OTG */
#define ISP1763_FLAG_DACK_POL_HIGH	0x00000010 /* DACK active high */
#define ISP1763_FLAG_DREQ_POL_HIGH	0x00000020 /* DREQ active high */
#define ISP1763_FLAG_INTR_POL_HIGH	0x00000080 /* Interrupt polarity high */
#define ISP1763_FLAG_INTR_EDGE_TRIG	0x00000100 /* Interrupt edge triggered */

struct isp1763_device {
	struct device *dev;

	void __iomem *regs;
	unsigned int devflags;
	struct gpio_desc *rst_gpio;

	struct isp1763_hcd hcd;
};

int isp1763_register(struct resource *mem, int irq, unsigned long irqflags,
		     struct device *dev, unsigned int devflags);
void isp1763_unregister(struct device *dev);

static inline u16 isp1763_read16(void __iomem *base, u16 reg)
{
	return readw(base + reg);
}

static inline void isp1763_write16(void __iomem *base, u16 reg, u16 val)
{
	writew(val, base + reg);
}

#endif
