// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*
* NOTE: Tab size is 8
*/
#include "../include/hdmi_cec.h"
#include "cec_access.h"
#include "cec_reg.h"

static unsigned char cec_lowestBitSet(unsigned char x)
{
        unsigned char result = 0;

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

int cec_dev_write(struct cec_device *cec_dev, unsigned int offset,
        unsigned int data)
{
        int write_rel = -1;
        volatile void __iomem *cec_io;

        do {
                if(cec_dev == NULL) {
                        break;
                }
                if((offset & IO_IH_MASK) == IO_IH_MASK) {
                        offset &= ~IO_IH_MASK;
                        cec_io = cec_dev->cec_irq_io;
                } else {
                        cec_io = cec_dev->cec_core_io;
                }

                if(offset & 0x3){
                        break;
                }
                if(cec_io == NULL) {
                        break;
                }
                iowrite32(data, (void*)(cec_io + offset));
                write_rel = 0;
        }while(0);

        return write_rel;
}
EXPORT_SYMBOL(cec_dev_write);

int cec_dev_read(struct cec_device *cec_dev, unsigned int offset)
{
        int read_val = -1;
        volatile void __iomem *cec_io;

        do {
                if(cec_dev == NULL) {
                        break;
                }
                if((offset & IO_IH_MASK) == IO_IH_MASK) {
                        offset &= ~IO_IH_MASK;
                        cec_io = cec_dev->cec_irq_io;
                } else {
                        cec_io = cec_dev->cec_core_io;
                }

                if(offset & 0x3){
                        break;
                }

                if(cec_io == NULL) {
                        break;
                }
                read_val = ioread32((void*)(cec_io + offset));
        }while(0);

        return read_val;
}
EXPORT_SYMBOL(cec_dev_read);

void cec_dev_write_mask(struct cec_device *cec_dev, unsigned int addr,
        unsigned char mask, unsigned char data)
{
        unsigned char temp = 0;
        unsigned char shift = cec_lowestBitSet(mask);

        if(cec_dev != NULL) {
                temp = cec_dev_read(cec_dev, addr);
                temp &= ~(mask);
                temp |= (mask & data << shift);
                cec_dev_write(cec_dev, addr, temp);
        }
}
EXPORT_SYMBOL(cec_dev_write_mask);

unsigned int cec_dev_read_mask(struct cec_device *cec_dev, unsigned int addr,
        unsigned char mask)
{
        unsigned read_val = 0;
        unsigned char shift = cec_lowestBitSet(mask);
        if(cec_dev != NULL) {
                read_val =  ((cec_dev_read(cec_dev, addr) & mask) >> shift);
        }
        return read_val;
}
EXPORT_SYMBOL(cec_dev_read_mask);

int cec_dev_sel_write(struct cec_device *cec_dev, unsigned int data)
{
        volatile void __iomem *cec_clk_sel =
                (volatile void __iomem *)(cec_dev!=NULL)?cec_dev->cec_clk_sel:NULL;
        if(cec_clk_sel != NULL) {
                iowrite32(data, (void*)cec_clk_sel);
        }
        return 0;
}
EXPORT_SYMBOL(cec_dev_sel_write);


int cec_dev_sel_read(struct cec_device *cec_dev)
{
        int read_val = -1;
        volatile void __iomem *cec_clk_sel =
                (volatile void __iomem *)(cec_dev!=NULL)?cec_dev->cec_clk_sel:NULL;
        if(cec_clk_sel != NULL) {
                read_val = ioread32((void*)cec_clk_sel);
        }
        return read_val;
}
EXPORT_SYMBOL(cec_dev_sel_read);
