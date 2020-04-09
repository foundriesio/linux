/*
 * Copyright (C) Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __OF_VIOC_WDMA_H__
#define	__OF_VIOC_WDMA_H__

enum {
	VIOC_WDMA_MODE_NONE = 0,
	VIOC_WDMA_MODE_RGB2YUV,
	VIOC_WDMA_MODE_YUV2RGB,
};

enum {
	VIOC_WDMA_SWAP_RGB = 0,
	VIOC_WDMA_SWAP_RBG,
	VIOC_WDMA_SWAP_GRB,
	VIOC_WDMA_SWAP_GBR,
	VIOC_WDMA_SWAP_BRG,
	VIOC_WDMA_SWAP_BGR,
	VIOC_WDMA_SWAP_MAX
};

struct vioc_wdma_mode {
	unsigned int		mode;
	unsigned int		convert;
	unsigned int		rgb_swap;
};

struct vioc_wdma_data {
	unsigned int		format;
	unsigned int		width;
	unsigned int		height;
	unsigned int		offset;
	unsigned int		base0;
	unsigned int		base1;
	unsigned int		base2;
	struct vioc_wdma_mode	mode;
	unsigned int		cont;	/* nContinuous */
};

struct vioc_wdma_device {
	struct vioc_intr_type	*intr;
	void __iomem		*reg;
	void __iomem		*rst_reg;
	int			id;
	int			irq;
	struct vioc_wdma_data	data;
};

extern void vioc_wdma_set_image(struct vioc_wdma_device *wdma, unsigned int en);
extern void vioc_wdma_swreset(struct vioc_wdma_device *wdma, int reset);
extern int get_count_vioc_wdma(struct device *dev);
extern struct vioc_wdma_device *devm_vioc_wdma_get(struct device *dev, int index);

#endif
