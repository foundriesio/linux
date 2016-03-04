/*
 * System Control Driver
 *
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 * Copyright (C) 2012 Linaro Ltd.
 *
 * Author: Dong Aisheng <dong.aisheng@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_MFD_SYSCON_H__
#define __LINUX_MFD_SYSCON_H__

#include <linux/err.h>

struct device_node;

#ifdef CONFIG_MFD_SYSCON
extern struct regmap *syscon_node_to_regmap(struct device_node *np);
extern struct regmap *syscon_regmap_lookup_by_compatible(const char *s);
extern struct regmap *syscon_regmap_lookup_by_pdevname(const char *s);
extern struct regmap *syscon_regmap_lookup_by_phandle(
					struct device_node *np,
					const char *property);
extern int syscon_regmap_read_from_offset(struct device_node *np,
					  const char *s,
					  unsigned int *val);
#else
static inline struct regmap *syscon_node_to_regmap(struct device_node *np)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct regmap *syscon_regmap_lookup_by_compatible(const char *s)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct regmap *syscon_regmap_lookup_by_pdevname(const char *s)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct regmap *syscon_regmap_lookup_by_phandle(
					struct device_node *np,
					const char *property)
{
	return ERR_PTR(-ENOSYS);
}

static inline int syscon_regmap_read_from_offset(struct device_node *np,
						 const char *s,
						 unsigned int *val)
{
	return ERR_PTR(-ENOSYS);
}
#endif

#endif /* __LINUX_MFD_SYSCON_H__ */
