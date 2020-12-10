// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_etc.h"
#include <linux/io.h>
//#include <asm/io.h>
#include <linux/delay.h>
#include "vpu_comm.h"
#include "vpu_buffer.h"

#define vpu_writel writel
#define vpu_readl readl

#ifdef CONFIG_VPU_TIME_MEASUREMENT
int vetc_GetTimediff_ms(struct timeval time1, struct timeval time2)
{
	int time_diff_ms = 0;

	time_diff_ms = (time2.tv_sec - time1.tv_sec) * 1000;
	time_diff_ms += (time2.tv_usec - time1.tv_usec) / 1000;

	return time_diff_ms;
}
EXPORT_SYMBOL(vetc_GetTimediff_ms);
#endif

unsigned int vetc_reg_read(void *base_addr, unsigned int offset)
{
	return vpu_readl(base_addr + offset);
}
EXPORT_SYMBOL(vetc_reg_read);

void vetc_reg_write(void *base_addr, unsigned int offset, unsigned int data)
{
	vpu_writel(data, base_addr + offset);
}
EXPORT_SYMBOL(vetc_reg_write);

#undef DEBUG_DUMP
void vetc_dump_reg_all(char *base_addr, unsigned char *str)
{
#ifdef DEBUG_DUMP
	unsigned int i = 0;

	V_DBG(VPU_DBG_REG_DUMP, "%s", str);
	while (i < 0x200) {
		V_DBG(VPU_DBG_REG_DUMP,
			"0x%8p : 0x%8x 0x%8x 0x%8x 0x%8x",
			base_addr + i,
			vetc_reg_read(base_addr, i),
			vetc_reg_read(base_addr, i+0x4),
			vetc_reg_read(base_addr, i+0x8),
			vetc_reg_read(base_addr, i+0xC));
		i += 0x10;
	}
#endif
}
EXPORT_SYMBOL(vetc_dump_reg_all);

void vetc_reg_init(char *base_addr)
{
	unsigned int i = 0;

	while (i < 0x200) {
		vpu_writel(0x00, (base_addr + i));
		vpu_writel(0x00, (base_addr + i + 0x4));
		vpu_writel(0x00, (base_addr + i + 0x8));
		vpu_writel(0x00, (base_addr + i + 0xC));
		i += 0x10;
	}

// To confirm if value are initialized!!
#ifdef DEBUG_DUMP
	while (i < 0x200) {
		V_DBG(VPU_DBG_REG_DUMP,
			"0x%8x : 0x%8x 0x%8x 0x%8x 0x%8x",
				base_addr + i,
				vetc_reg_read(base_addr, i),
				vetc_reg_read(base_addr, i+0x4),
				vetc_reg_read(base_addr, i+0x8),
				vetc_reg_read(base_addr, i+0xC));
		i += 0x10;
	}
#endif
}
EXPORT_SYMBOL(vetc_reg_init);

void *vetc_ioremap(phys_addr_t phy_addr, unsigned int size)
{
	return ioremap_nocache(phy_addr, size);
}
EXPORT_SYMBOL(vetc_ioremap);

void vetc_iounmap(void *virt_addr)
{
	iounmap((void __iomem *)virt_addr);
}
EXPORT_SYMBOL(vetc_iounmap);

void *vetc_memcpy(void *dest, const void *src, unsigned int count,
		  unsigned int type)
{
	void *ret = NULL;

	if (_vmem_is_cma_allocated_virt_region(src, count) > 0)
		type = 0;

	if (type == 1)
		memcpy_fromio(dest, src, count);
	else if (type == 2)
		memcpy_toio(dest, src, count);
	else
		ret = memcpy(dest, src, count);

	return ret;
}
EXPORT_SYMBOL(vetc_memcpy);

void vetc_memset(void *ptr, int value, unsigned int num, unsigned int type)
{
	if (_vmem_is_cma_allocated_virt_region(ptr, num) > 0)
		type = 0;

	if (type == 1)
		memset_io(ptr, value, num);
	else
		memset(ptr, value, num);
}
EXPORT_SYMBOL(vetc_memset);

void vetc_usleep(unsigned int min, unsigned int max)
{
	usleep_range((unsigned long)min, (unsigned long)max);
}
EXPORT_SYMBOL(vetc_usleep);
