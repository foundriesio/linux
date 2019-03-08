// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#include "include/hdmi_includes.h"

static u8 hdmi_lowestBitSet(u8 x)
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

int hdmi_dev_write(struct hdmi_tx_dev *hdmi_dev, uint32_t offset, uint32_t data){
        volatile void __iomem *dwc_hdmi_tx_core_io = (volatile void __iomem *)hdmi_dev->dwc_hdmi_tx_core_io;
        if(offset & 0x3){
                return -1;
        }
        iowrite32(data, (void*)(dwc_hdmi_tx_core_io + offset));
        return 0;
}
EXPORT_SYMBOL(hdmi_dev_write);


unsigned int hdmi_dev_read(struct hdmi_tx_dev *hdmi_dev, uint32_t offset){
        volatile void __iomem *dwc_hdmi_tx_core_io = (volatile void __iomem *)hdmi_dev->dwc_hdmi_tx_core_io;
        if(offset & 0x3){
                return 0;
        }
        return ioread32((void*)(dwc_hdmi_tx_core_io + offset));
}
EXPORT_SYMBOL(hdmi_dev_read);


void hdmi_dev_write_mask(struct hdmi_tx_dev *hdmi_dev, u32 addr, u8 mask, u8 data){
        u8 temp = 0;
        u8 shift = hdmi_lowestBitSet(mask);

        temp = hdmi_dev_read(hdmi_dev, addr);
        temp &= ~(mask);
        temp |= (mask & data << shift);
        hdmi_dev_write(hdmi_dev, addr, temp);
}
EXPORT_SYMBOL(hdmi_dev_write_mask);


u32 hdmi_dev_read_mask(struct hdmi_tx_dev *hdmi_dev, u32 addr, u8 mask){
        u8 shift = hdmi_lowestBitSet(mask);
        return ((hdmi_dev_read(hdmi_dev, addr) & mask) >> shift);
}
EXPORT_SYMBOL(hdmi_dev_read_mask);



