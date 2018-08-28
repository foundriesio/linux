/*
 *   FileName : vpu_etc.c
 *   Description :
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved
 *
 * This source code contains confidential information of Telechips.
 * Any unauthorized use without a written permission of Telechips including
 * not limited to re-distribution in source or binary form is strictly prohibited.
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind,
 * including without limitation, any warranty of merchantability,
 * fitness for a particular purpose or non-infringement of any patent,
 * copyright or other third party intellectual property right.
 * No warranty is made, express or implied, regarding the informations accuracy,
 * completeness, or performance.
 * In no event shall Telechips be liable for any claim, damages or other liability
 * arising from, out of or in connection with this source code or the use
 * in the source code.
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
 *
 */

#include "vpu_etc.h"
#include <asm/io.h>

#define vpu_writel writel
#define vpu_readl  readl

#ifdef CONFIG_VPU_TIME_MEASUREMENT
int vetc_GetTimediff_ms(struct timeval time1, struct timeval time2)
{
    int time_diff_ms = 0;

    time_diff_ms = (time2.tv_sec- time1.tv_sec)*1000;
    time_diff_ms += (time2.tv_usec-time1.tv_usec)/1000;

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
    vpu_writel(data, base_addr+offset);
}
EXPORT_SYMBOL(vetc_reg_write);

void vetc_dump_reg_all(char *base_addr, unsigned char *str)
{
    unsigned int j;

return; // disable temporarily
//mem r 0xF0700000 w 0x200
    j = 0;
    printk("%s \n", str);
    while(j < 0x200)
    {
        printk("0x%8p : 0x%8x 0x%8x 0x%8x 0x%8x \n", base_addr + j, vetc_reg_read(base_addr, j), vetc_reg_read(base_addr, j+0x4), vetc_reg_read(base_addr, j+0x8), vetc_reg_read(base_addr, j+0xC));
        j+=0x10;
    }
}EXPORT_SYMBOL(vetc_dump_reg_all);


void vetc_reg_init(char *base_addr)
{
    unsigned int i = 0;

    while(i < 0x200)
    {
        vpu_writel(0x00, (base_addr + i));
        vpu_writel(0x00, (base_addr + i + 0x4));
        vpu_writel(0x00, (base_addr + i + 0x8));
        vpu_writel(0x00, (base_addr + i + 0xC));
        i += 0x10;
    }

#if 0// To confirm inited value!!
    while(i < 0x200)
    {
        printk("0x%8x : 0x%8x 0x%8x 0x%8x 0x%8x \n", base_addr + i, vetc_reg_read(base_addr, i), vetc_reg_read(base_addr, i+0x4), vetc_reg_read(base_addr, i+0x8), vetc_reg_read(base_addr, i+0xC));
        i+=0x10;
    }
#endif
}
EXPORT_SYMBOL(vetc_reg_init);

void* vetc_ioremap(phys_addr_t phy_addr, unsigned int size)
{
    return ioremap_nocache(phy_addr, size);
}
EXPORT_SYMBOL(vetc_ioremap);

void vetc_iounmap(void* virt_addr)
{
    iounmap((void __iomem *)virt_addr);
}
EXPORT_SYMBOL(vetc_iounmap);

void * vetc_memcpy( void *dest, const void *src, unsigned int count, unsigned int type )
{
    if ( type == 1 )
        memcpy_fromio(dest, src, count);
    else if ( type == 2 )
        memcpy_toio(dest, src, count);
    else
        return memcpy(dest, src, count);
}
EXPORT_SYMBOL(vetc_memcpy);

void vetc_memset(void *ptr, int value, unsigned  num, unsigned int type )
{
    if ( type == 1 )
        memset_io(ptr, value, num);
    else
        memset(ptr, value, num);
}
EXPORT_SYMBOL(vetc_memset);
