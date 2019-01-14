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
#include <include/hdmi_ioctls.h>
#include <include/video_params.h>

#include <core/irq.h>
#include <core/main_controller.h>


#include <bsp/i2cm.h>
#include <phy/phy.h>
#include <phy/phy_private.h>


/*************************************************************
 * Internal functions
 */
static void phy_gen2_en_hpd_rx_sense(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, PHY_CONF0, PHY_CONF0_ENHPDRXSENSE_MASK, (bit ? 1 : 0));
}


/*************************************************************
 * External functions
 *************************************************************/

void hdmi_phy_gen2_tx_power_on(struct hdmi_tx_dev *dev, u8 bit)
{
        hdmi_dev_write_mask(dev, PHY_CONF0, PHY_CONF0_TXPWRON_MASK, (bit ? 1 : 0));
}

int hdmi_phy_enable_hpd_sense(struct hdmi_tx_dev *dev)
{
	phy_gen2_en_hpd_rx_sense(dev, 0);
	return TRUE;
}

int hdmi_phy_disable_hpd_sense(struct hdmi_tx_dev *dev)
{
	phy_gen2_en_hpd_rx_sense(dev, 1);
	return TRUE;
}

int hdmi_phy_hot_plug_detected(struct hdmi_tx_dev *dev)
{
	/* MASK         STATUS          POLARITY        INTERRUPT        HPD
	 *   0             0                 0               1             0
	 *   0             1                 0               0             1
	 *   0             0                 1               0             0
	 *   0             1                 1               1             1
	 *   1             x                 x               0             x
	 */

	int hpd_polarity = hdmi_dev_read_mask(dev, PHY_POL0, PHY_POL0_HPD_MASK);
	int hpd = hdmi_dev_read_mask(dev, PHY_STAT0, PHY_STAT0_HPD_MASK);

	// Mask interrupt
	hdmi_phy_interrupt_mask(dev, PHY_MASK0_HPD_MASK);

	if (hpd_polarity == hpd) {
		hdmi_dev_write_mask(dev, PHY_POL0, PHY_POL0_HPD_MASK, !hpd_polarity);

		// Un-mask interrupts
		hdmi_phy_interrupt_unmask(dev, PHY_MASK0_HPD_MASK);

		return hpd_polarity;
	}

	// Un-mask interrupts
	hdmi_phy_interrupt_unmask(dev, PHY_MASK0_HPD_MASK);

	return !hpd_polarity;
}

int hdmi_phy_interrupt_enable(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write(dev, PHY_MASK0, value);
	return TRUE;
}

int hdmi_phy_phase_lock_loop_state(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, (PHY_STAT0), PHY_STAT0_TX_PHY_LOCK_MASK);
}

void hdmi_phy_interrupt_mask(struct hdmi_tx_dev *dev, u8 mask)
{
	// Mask will determine which bits will be enabled
	hdmi_dev_write_mask(dev, PHY_MASK0, mask, 0xff);
}

void hdmi_phy_interrupt_unmask(struct hdmi_tx_dev *dev, u8 mask)
{
	// Mask will determine which bits will be enabled
	hdmi_dev_write_mask(dev, PHY_MASK0, mask, 0x0);
}

u8 hdmi_phy_hot_plug_state(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, (PHY_STAT0), PHY_STAT0_HPD_MASK);
}

u8 hdmi_phy_get_rx_sense_status(struct hdmi_tx_dev *dev)
{
	dev->rxsense = hdmi_dev_read_mask(dev, PHY_STAT0, PHY_STAT0_RX_SENSE_0_MASK);
	return dev->rxsense;
}

unsigned int hdmi_phy_get_actual_tmds_bit_ratio(struct hdmi_tx_dev *dev, unsigned int pixelclock, color_depth_t color_depth)
{
        unsigned int tmds_clock = 0;

        if(pixelclock) {
                pixelclock /= 1000;
                switch(color_depth)
                {
                        case COLOR_DEPTH_8:
                                tmds_clock = pixelclock;
                                break;

                        case COLOR_DEPTH_10:
                                // 1.25x
                                tmds_clock = (pixelclock * 125)/100;
                                break;
                        case COLOR_DEPTH_12:
                                // 1.5x
                                tmds_clock = (pixelclock * 15)/10;
                                break;
                        default:
                                // noghing.
                                break;
                }

                if(tmds_clock > 0) {
                        tmds_clock *= 1000;
                }
        }

        //printf("\r\n >>actual tmds clock is %dHz\r\n", tmds_clock);
        return tmds_clock;
}

unsigned int hdmi_phy_get_actual_tmds_bit_ratio_by_videoparam(struct hdmi_tx_dev *dev, videoParams_t *videoParams)
{
        color_depth_t color_depth;
        unsigned int pixel_clock, tmds_clock = 0;

        if(videoParams != NULL) {
                color_depth = (videoParams->mEncodingOut == YCC422)?COLOR_DEPTH_8:videoParams->mColorResolution;
                pixel_clock = videoParams_GetPixelClock(dev, videoParams);
                tmds_clock = hdmi_phy_get_actual_tmds_bit_ratio(dev, pixel_clock, color_depth);
        }
        return tmds_clock;
}


int dwc_hdmi_phy_config(struct hdmi_tx_dev *dev, videoParams_t * videoParams)
{
        int ret = -1;
        color_depth_t color_depth;
        unsigned int pixel_clock, tmds_clock;

        if(dev != NULL && videoParams != NULL) {
                color_depth = (videoParams->mEncodingOut == YCC422)?COLOR_DEPTH_8:videoParams->mColorResolution;
                pixel_clock = videoParams_GetPixelClock(dev, videoParams);
                tmds_clock = hdmi_phy_get_actual_tmds_bit_ratio(dev, pixel_clock, color_depth);
                tcc_hdmi_phy_config(dev, pixel_clock, tmds_clock, color_depth, videoParams->mScdcPresent, videoParams->mScrambling);
                ret = 0;
        }

        return ret;
}

void dwc_hdmi_phy_mask(struct hdmi_tx_dev *dev, int mask)
{
        tcc_hdmi_phy_mask(dev, mask);
}

void dwc_hdmi_phy_dump(struct hdmi_tx_dev *dev)
{
        tcc_hdmi_phy_dump(dev);
}

void dwc_hdmi_phy_standby(struct hdmi_tx_dev *dev)
{
        tcc_hdmi_phy_standby(dev);
}

void dwc_hdmi_phy_clear_status_ready(struct hdmi_tx_dev *dev)
{
        tcc_hdmi_phy_clear_status_ready(dev);
}

int dwc_hdmi_phy_status_ready(struct hdmi_tx_dev *dev)
{
        return tcc_hdmi_phy_status_ready(dev);
}

void dwc_hdmi_phy_clear_status_clock_ready(struct hdmi_tx_dev *dev)
{
        tcc_hdmi_phy_clear_status_clock_ready(dev);
}

int dwc_hdmi_phy_status_clock_ready(struct hdmi_tx_dev *dev)
{
        return tcc_hdmi_phy_status_clock_ready(dev);
}

void dwc_hdmi_phy_clear_status_pll_lock(struct hdmi_tx_dev *dev)
{
        tcc_hdmi_phy_clear_status_pll_lock(dev);
}

int dwc_hdmi_phy_status_pll_lock(struct hdmi_tx_dev *dev)
{
        return tcc_hdmi_phy_status_pll_lock(dev);
}

int dwc_hdmi_phy_use_peri_clock(struct hdmi_tx_dev *dev)
{
       return tcc_hdmi_phy_use_peri_clock(dev);
}
#if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
int dwc_hdmi_proc_write_phy_regs(struct hdmi_tx_dev *dev, char *phy_regs_buf)
{
        return  tcc_hdmi_proc_write_phy_regs(dev, phy_regs_buf);
}

ssize_t dwc_hdmi_proc_read_phy_regs(struct hdmi_tx_dev *dev, char *phy_regs_buf, ssize_t regs_buf_size)
{
        return tcc_hdmi_proc_read_phy_regs(dev, phy_regs_buf, regs_buf_size);
}
#endif

