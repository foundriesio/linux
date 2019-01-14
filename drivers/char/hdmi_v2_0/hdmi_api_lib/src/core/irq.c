/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <core/interrupt/interrupt_reg.h>
#include <core/irq.h>
#include <bsp/i2cm.h>
#include <phy/phy.h>

typedef struct irq_vector {
        hdmi_irq_sources_t source;
        unsigned int stat_reg;
        unsigned int mute_reg;
}irq_vector_t;

static irq_vector_t irq_vec[] = {
                {AUDIO_PACKETS,         IH_FC_STAT0,            IH_MUTE_FC_STAT0},
                {OTHER_PACKETS,         IH_FC_STAT1,            IH_MUTE_FC_STAT1},
                {PACKETS_OVERFLOW,      IH_FC_STAT2,            IH_MUTE_FC_STAT2},
                {AUDIO_SAMPLER,         IH_AS_STAT0,            IH_MUTE_AS_STAT0},
                {PHY,                           IH_PHY_STAT0,           IH_MUTE_PHY_STAT0},
                {I2C_DDC,                       IH_I2CM_STAT0,          IH_MUTE_I2CM_STAT0},
                {CEC,                           IH_CEC_STAT0,           IH_MUTE_CEC_STAT0},
                {VIDEO_PACKETIZER,      IH_VP_STAT0,            IH_MUTE_VP_STAT0},
                {I2C_PHY,                       IH_I2CMPHY_STAT0,   IH_MUTE_I2CMPHY_STAT0},
                {AUDIO_DMA,             IH_AHBDMAAUD_STAT0, IH_MUTE_AHBDMAAUD_STAT0},
                {0, 0, 0},
};

#if 0
// This Api is deprecated.
static int hdmi_find_irq_enable_entry(struct hdmi_tx_dev *dev)
{
        struct list_head* next;
        struct drv_enable_entry *irq_enable_entry = NULL;

        list_for_each(next, &dev->irq_enable_entry.list) {
            irq_enable_entry = list_entry(next, struct drv_enable_entry, list);
            if(dev->verbose >= VERBOSE_IO)
                printk(" Remain IRQ ID=%d\r\n", irq_enable_entry->id);
            break;
        }
        //printk("----\r\n");
        return (irq_enable_entry != NULL)?1:0;

}

static struct drv_enable_entry* hdmi_find_irq_enable_entry_by_id(struct hdmi_tx_dev *dev, unsigned int id)
{
    struct list_head* next;
    struct drv_enable_entry *irq_enable_entry;

    list_for_each(next, &dev->irq_enable_entry.list) {
        irq_enable_entry = list_entry(next, struct drv_enable_entry, list);
        if(irq_enable_entry->id == id) break;
        irq_enable_entry = NULL;
    }

    return irq_enable_entry;
}


static int hdmi_add_irq_enable_entry(struct hdmi_tx_dev *dev, unsigned int id)
{
        int result = -1;

        struct drv_enable_entry *irq_enable_entry = hdmi_find_irq_enable_entry_by_id(dev, id);

        if(irq_enable_entry) {
                // alread existed.
                //printk(" hdmi_add_irq_enable_entry %d is already enabled\r\n", id);
                result = 0;
        }
        else {
                irq_enable_entry = kzalloc(sizeof(struct drv_enable_entry), GFP_ATOMIC);

                if(!irq_enable_entry) {
                        printk("out of memory\r\n");
                        result = -1;
                }
                else {
                        irq_enable_entry->id = id;
                        list_add_tail(&irq_enable_entry->list, &dev->irq_enable_entry.list);
                        result = 0;
                }
        }

        return result;
}

static int hdmi_remove_irq_enable_entry(struct hdmi_tx_dev *dev, unsigned int id)
{
        int result = -1;

        struct drv_enable_entry *irq_enable_entry;

        irq_enable_entry = hdmi_find_irq_enable_entry_by_id(dev, id);

        if(irq_enable_entry) {
                list_del(&irq_enable_entry->list);
                kfree(irq_enable_entry);
                result = 0;
        }
        else {
                // already remove..!
                //printk(" hdmi_remove_irq_enable_entry %d is already enabled\r\n", id);
                result = 0;
        }

        return result;
}
#endif

int hdmi_irq_read_stat(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source, u8 *stat)
{
        int i;
        for(i = 0; irq_vec[i].source != 0; i++){
                if(irq_vec[i].source == irq_source){
                        *stat = hdmi_dev_read(dev, irq_vec[i].stat_reg);
                        LOGGER(SNPS_DEBUG, "IRQ read state: irq[%d] stat[%d]", irq_source, *stat);
                        return TRUE;
                }
        }
        LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
        *stat = 0;
        return FALSE;
}

/*******************************************************************
 * Clear IRQ miscellaneous
 */
int hdmi_irq_clear_source(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source)
{
        int i;

        for(i = 0; irq_vec[i].source != 0; i++){
                if(irq_vec[i].source == irq_source){
                        LOGGER(SNPS_DEBUG, "IRQ write clear: irq[%d] mask[%d]", irq_source, 0xff);
                        hdmi_dev_write(dev, irq_vec[i].stat_reg,  0xff);
                        return TRUE;
                }
        }
        LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
        return FALSE;
}

int hdmi_irq_clear_bit(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source, u8 bit_mask)
{
        int i;

        for(i = 0; irq_vec[i].source != 0; i++){
                if(irq_vec[i].source == irq_source){
                        LOGGER(SNPS_DEBUG, "IRQ write clear bit: irq[%d] bitmask[%d]", irq_source, bit_mask);
                        hdmi_dev_write_mask(dev, irq_vec[i].stat_reg, bit_mask, 1);
                        return TRUE;
                }
        }
        LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
        return FALSE;
}

/*******************************************************************
 * Mute IRQ miscellaneous
 */
int hdmi_irq_mute_source(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source)
{
        int i;

        for(i = 0; irq_vec[i].source != 0; i++){
                if(irq_vec[i].source == irq_source){
                        LOGGER(SNPS_DEBUG, "IRQ write mute: irq[%d] mask[%d]", irq_source, 0xff);
                        hdmi_dev_write(dev, irq_vec[i].mute_reg,  0xff);
                        return TRUE;
                }
        }
        LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
        return FALSE;
}

static int irq_mask_bit(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source, u8 bit_mask)
{
        int i;

        for(i = 0; irq_vec[i].source != 0; i++){
                if(irq_vec[i].source == irq_source){
                        LOGGER(SNPS_DEBUG, "IRQ mask bit: irq[%d] bit_mask[%d]", irq_source, bit_mask);
                        hdmi_dev_write_mask(dev, irq_vec[i].mute_reg, bit_mask, 1);
                        return TRUE;
                }
        }
        LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
        return FALSE;
}

static int irq_unmask_bit(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source, u8 bit_mask)
{
        int i;

        for(i = 0; irq_vec[i].source != 0; i++){
                if(irq_vec[i].source == irq_source){
                        LOGGER(SNPS_DEBUG, "IRQ unmask bit: irq[%d] bit_mask[%d]", irq_source, bit_mask);
                        hdmi_dev_write_mask(dev, irq_vec[i].mute_reg, bit_mask, 0);
                        return TRUE;
                }
        }
        LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
        return FALSE;
}

/*******************************************************************
 *
 */

int hdmi_irq_mute(struct hdmi_tx_dev *dev)
{
        hdmi_dev_write(dev, IH_MUTE,  0x3);
        return 0;
}

void hdmi_irq_unmute(struct hdmi_tx_dev *dev)
{
        hdmi_dev_write(dev, IH_MUTE,  0x0);
}

void hdmi_irq_clear_all(struct hdmi_tx_dev *dev)
{
        hdmi_irq_clear_source(dev, AUDIO_PACKETS);
        hdmi_irq_clear_source(dev, OTHER_PACKETS);
        hdmi_irq_clear_source(dev, PACKETS_OVERFLOW);
        hdmi_irq_clear_source(dev, AUDIO_SAMPLER);
        hdmi_irq_clear_source(dev, PHY);
        hdmi_irq_clear_source(dev, I2C_DDC);
        hdmi_irq_clear_source(dev, CEC);
        hdmi_irq_clear_source(dev, VIDEO_PACKETIZER);
        hdmi_irq_clear_source(dev, I2C_PHY);
        hdmi_irq_clear_source(dev, AUDIO_DMA);
}

void hdmi_irq_mask_all(struct hdmi_tx_dev *dev)
{
        hdmi_irq_mute(dev);
        hdmi_irq_mute_source(dev, AUDIO_PACKETS);
        hdmi_irq_mute_source(dev, OTHER_PACKETS);
        hdmi_irq_mute_source(dev, PACKETS_OVERFLOW);
        hdmi_irq_mute_source(dev, AUDIO_SAMPLER);
        hdmi_irq_mute_source(dev, PHY);
        hdmi_irq_mute_source(dev, I2C_DDC);
        hdmi_irq_mute_source(dev, CEC);
        hdmi_irq_mute_source(dev, VIDEO_PACKETIZER);
        hdmi_irq_mute_source(dev, I2C_PHY);
        hdmi_irq_mute_source(dev, AUDIO_DMA);
}

void hdmi_irq_scdc_read_request(struct hdmi_tx_dev *dev, int enable)
{
        if(enable)
                irq_unmask_bit(dev, I2C_DDC, IH_MUTE_I2CM_STAT0_SCDC_READREQ_MASK);
        else
                irq_mask_bit(dev, I2C_DDC, IH_MUTE_I2CM_STAT0_SCDC_READREQ_MASK);
}

/*
void un_mask_i2c_interrupt(struct hdmi_tx_dev *dev)
{
        irq_clear_source(dev, I2C_DDC);

        // scdc_readreq
        irq_unmask_bit(dev, I2C_DDC, IH_MUTE_I2CM_STAT0_I2CMASTERERROR_MASK);
        // I2Cmasterdone
        irq_unmask_bit(dev, I2C_DDC, IH_MUTE_I2CM_STAT0_I2CMASTERDONE_MASK);
        // I2Cmastererror
        irq_unmask_bit(dev, I2C_DDC, IH_MUTE_I2CM_STAT0_SCDC_READREQ_MASK);
}
*/

static void hdmi_irq_hpd_sense_enable(struct hdmi_tx_dev *dev)
{
        // Un-mask the HPD sense
        irq_unmask_bit(dev, PHY, IH_MUTE_PHY_STAT0_HPD_MASK);

        // Un-mask the PHY_mask register - hpd bit
        hdmi_phy_interrupt_enable(dev, PHY_MASK0_TX_PHY_LOCK_MASK|
                                        PHY_MASK0_RX_SENSE_0_MASK|
                                        PHY_MASK0_RX_SENSE_1_MASK|
                                        PHY_MASK0_RX_SENSE_2_MASK|
                                        PHY_MASK0_RX_SENSE_3_MASK);
}

static void hdmi_irq_hpd_sense_disable(struct hdmi_tx_dev *dev)
{
        // mask the HPD sense
        irq_mask_bit(dev, PHY, IH_MUTE_PHY_STAT0_HPD_MASK);

        // mask the PHY_mask register - hpd bit
        hdmi_phy_interrupt_enable(dev, PHY_MASK0_HPD_MASK);
}


u32 hdmi_read_interrupt_decode(struct hdmi_tx_dev *dev)
{
        return (hdmi_dev_read(dev, IH_DECODE) & 0xFF);
}

int hdmi_hpd_enable(struct hdmi_tx_dev *dev)
{
        if(dev != NULL) {
                /*
                 * You can use hpd interrupts of the hdmi link only if you do not use gpio interrupts */
                if(dev->hotplug_irq < 0) {
                        if(dev->hotplug_irq_enable) {
                                // Setting HPD Polarity
                                hdmi_phy_hot_plug_detected(dev);

                                // Enable HPD Interrupt
                                hdmi_irq_hpd_sense_enable(dev);
                                msleep_interruptible(10); // This delay for 10ms is necessary to catch hpd interrupt.!!
                        }
                        else {
                                // Disable HPD Interrupt
                                hdmi_irq_hpd_sense_disable(dev);
                        }
                } /* else {
                        pr_warning("%s skip..\r\nbecause, hdmi uses gpio interrupt to detect the hotplug \r\n", __func__);
                } */
        }
        return 0;
}

