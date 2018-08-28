/* soc/tcc/pmap.h
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PLAT_PMAP_H
#define __PLAT_PMAP_H

#define TCC_PMAP_NAME_LEN	16

#define PMAP_FLAG_SECURED	(1 << 1)
#define PMAP_FLAG_SHARED	(1 << 2)

typedef struct {
	char name[TCC_PMAP_NAME_LEN];
	__u32 base;
	__u32 size;
	__u32 groups;
	unsigned int flags;
} pmap_t;


int pmap_get_info(const char *name, pmap_t *mem);
int pmap_check_region(__u32 base, __u32 size);

#endif  /* __PLAT_PMAP_H */
