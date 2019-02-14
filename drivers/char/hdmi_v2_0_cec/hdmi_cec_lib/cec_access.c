// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#include "../include/hdmi_cec.h"
#include "cec_access.h"
#include "cec_reg.h"

static u8 cec_lowestBitSet(u8 x)
{
        u8 result = 0;

        // x=0 is not properly handled by while-loop
        if (x == 0)
                return 0;

        while ((x & 1) == 0)
        {
                x >>= 1;
                result++;
        }

        return result;
}

int cec_dev_write(struct cec_device *cec_dev, uint32_t offset, uint32_t data){
        volatile void __iomem *cec_core_io = (volatile void __iomem *)cec_dev->cec_core_io;
        if(offset & 0x3){
                return -1;
        }
        iowrite32(data, (void*)(cec_core_io + offset));
        return 0;
}
EXPORT_SYMBOL(cec_dev_write);


int cec_dev_read(struct cec_device *cec_dev, uint32_t offset){
        volatile void __iomem *cec_core_io = (volatile void __iomem *)cec_dev->cec_core_io;
        if(offset & 0x3){
                return -1;
        }
        return ioread32((void*)(cec_core_io + offset));
}
EXPORT_SYMBOL(cec_dev_read);


void cec_dev_write_mask(struct cec_device *cec_dev, u32 addr, u8 mask, u8 data){
        u8 temp = 0;
        u8 shift = cec_lowestBitSet(mask);

        temp = cec_dev_read(cec_dev, addr);
        temp &= ~(mask);
        temp |= (mask & data << shift);
        cec_dev_write(cec_dev, addr, temp);
}
EXPORT_SYMBOL(cec_dev_write_mask);


u32 cec_dev_read_mask(struct cec_device *cec_dev, u32 addr, u8 mask){
        u8 shift = cec_lowestBitSet(mask);
        return ((cec_dev_read(cec_dev, addr) & mask) >> shift);
}
EXPORT_SYMBOL(cec_dev_read_mask);

int cec_dev_sel_write(struct cec_device *cec_dev, uint32_t offset, uint32_t data){
        volatile void __iomem *cec_clk_sel = (volatile void __iomem *)cec_dev->cec_clk_sel;

        if(offset & 0x3){
                return -1;
        }
        iowrite32(data, (void*)(cec_clk_sel + offset));
        return 0;
}
EXPORT_SYMBOL(cec_dev_sel_write);


int cec_dev_sel_read(struct cec_device *cec_dev, uint32_t offset){
        volatile void __iomem *cec_clk_sel = (volatile void __iomem *)cec_dev->cec_clk_sel;
        if(offset & 0x3){
                return -1;
        }
        return ioread32((void*)(cec_clk_sel + offset));
}
EXPORT_SYMBOL(cec_dev_sel_read);


void cec_dev_sel_write_mask(struct cec_device *cec_dev, u32 addr, u8 mask, u8 data){
        u8 temp = 0;
        u8 shift = cec_lowestBitSet(mask);

        temp = cec_dev_sel_read(cec_dev, addr);
        temp &= ~(mask);
        temp |= (mask & data << shift);
        cec_dev_sel_write(cec_dev, addr, temp);
}
EXPORT_SYMBOL(cec_dev_sel_write_mask);


u32 cec_dev_sel_read_mask(struct cec_device *cec_dev, u32 addr, u8 mask){
        u8 shift = cec_lowestBitSet(mask);
        return ((cec_dev_sel_read(cec_dev, addr) & mask) >> shift);
}
EXPORT_SYMBOL(cec_dev_sel_read_mask);