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
#ifndef __OF_VIOC_SC_H__
#define	__OF_VIOC_SC_H__



struct vioc_sc_data {
	unsigned int		bypass;
	unsigned int		dst_width;
	unsigned int		dst_height;
	unsigned int		out_x;
	unsigned int		out_y;
	unsigned int		out_width;
	unsigned int		out_height;
};

struct vioc_sc_device {
	struct vioc_intr_type	*intr;
	void __iomem		*reg;
	void __iomem		*rst_reg;
	int			id;
	int			irq;
	int			path;
	int			type;
	struct vioc_sc_data	data;
};

extern int vioc_sc_set_path(struct vioc_sc_device *sc, int en);
extern void vioc_sc_set_image(struct vioc_sc_device *sc, unsigned int en);
extern void vioc_sc_swreset(struct vioc_sc_device *sc, int reset);
extern int get_count_vioc_sc(struct device *dev);
extern struct vioc_sc_device *devm_vioc_sc_get(struct device *dev, int index);

#endif
