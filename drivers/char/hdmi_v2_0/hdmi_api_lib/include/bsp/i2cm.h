/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        extenddisplay.cpp
*  \brief       HDMI TX controller driver
*  \details
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular
purpose or non-infringement of any patent, copyright or other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source code or the use in the
source code.
This source code is provided subject to the terms of a Mutual Non-Disclosure
Agreement between Telechips and Company.
*/
#ifndef __HDMI_I2CM_H__
#define __HDMI_I2CM_H__


#define I2CDDC_TIMEOUT 400
#define I2C_MIN_FS_SCL_HIGH_TIME        61 //63 //75
#define I2C_MIN_FS_SCL_LOW_TIME         132 //137 //163
//Fixed DDC timing issues in HF1-55
#define I2C_MIN_SS_SCL_HIGH_TIME        5480 //4680 //5480 //5500 //4592 //4737 //5625
#define I2C_MIN_SS_SCL_LOW_TIME         6440 //5200 //6088 //6100 //5102 //5263 //6250
#define HDMI_DDC_SDA_HOLD	        0x50

void hdmi_i2cddc_reset(struct hdmi_tx_dev *dev);
void hdmi_i2cddc_bus_clear(struct hdmi_tx_dev *dev);

/** I2C clock configuration
 *
 * @param sfrClock external clock supplied to controller
 * @param value of standard speed low time counter (refer to HDMITXCTRL databook)
 * @param value of standard speed high time counter (refer to HDMITXCTRL databook)
 * @param value of fast speed low time counter (refer to HDMITXCTRL databook)
 * @param value of fast speed high time counter (refer to HDMITXCTRL databook)
 */
void hdmi_i2cddc_clk_config(struct hdmi_tx_dev *dev, u16 sfrClock, u16 ss_low_ckl, u16 ss_high_ckl, u16 fs_low_ckl, u16 fs_high_ckl);

/** Set the speed mode (standard/fast mode)
 *
 * @param fast mode selection, 0 standard - 1 fast
 */
void hdmi_i2cddc_fast_mode(struct hdmi_tx_dev *dev, u8 fast);

/** Set the sda holdtime
 *
 * @param sda holdtime
 */
void hdmi_i2cddc_sda_hold(struct hdmi_tx_dev *dev, int hold);

/** Enable disable interrupts.
 *
 * @param mask to enable or disable the masking (u32 baseAddr, true to mask,
 * ie true to stop seeing the interrupt).
 */
void hdmi_i2cddc_mask_interrupts(struct hdmi_tx_dev *dev, u8 mask);

/** Read from extended addresses, E-DDC.
 *
 * @param i2cAddr i2c device address to read data
 * @param addr base address of the module registers
 * @param segment segment to read from
 * @param pointer in segment to read
 * @param value pointer to data read
 * @returns 0 if ok and error in other cases
 */
int hdmi_ddc_read(struct hdmi_tx_dev *dev, u8 i2cAddr, u8 segment, u8 pointer, u8 addr, u8 len, u8 * data);

/** Write from extended addresses, E-DDC.
 *
 * @param i2cAddr i2c device address to read data
 * @param addr base address of the module registers
 * @param len lenght to write
 * @param data pointer to data write
 * @returns 0 if ok and error in other cases
 */
int hdmi_ddc_write(struct hdmi_tx_dev *dev, u8 i2cAddr, u8 addr, u8 len, u8 * data);

int hdmi_ddc_check(struct hdmi_tx_dev *dev, int addr, int len);





#endif				/* __HDMI_I2CM_H__ */
