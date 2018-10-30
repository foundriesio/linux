/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi.c
*  \brief       HDMI controller driver
*  \details   
*               Important!
*               The default tab size of this source code is setted with 8.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_PM
#include <linux/pm_runtime.h>
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_video.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi.h>
#include <regs-hdmi.h>
#include <linux/clk/tcc.h>
#include <linux/clocksource.h>
#include <asm/bitops.h> // bit macros

#define SRC_VERSION     "4.14_1.0.4"
/**
 * If 'SIMPLAYHD' is 1, check Ri of 127th and 128th frame -@n
 * on 3rd authentication. And also check if Ri of 127th frame is -@n
 * different from that of 128th frame. if 'SIMPLAYHD' is 0, check only Ri -@n
 * of 128th frame.
 */
#define HDMI_DEBUG 	0
#define HDMI_DEBUG_TIME 0

#if HDMI_DEBUG
#define dprintk(args...)   { printk( "hdmi1.4: " args); }
#else
#define dprintk(args...)
#endif

#define HDMI_IOCTL_DEBUG 0
#if HDMI_IOCTL_DEBUG 
#define io_debug(...) pr_info(__VA_ARGS__)
#else
#define io_debug(...)
#endif

#if HDMI_DEBUG_TIME
unsigned long jstart, jend;
unsigned long ji2cstart, ji2cend;
#endif

/* Expoprt Interface API */
static struct tcc_hdmi_dev *api_dev = NULL;


static unsigned int ddi_reg_read(struct tcc_hdmi_dev *dev, unsigned int offset)
{
        unsigned int val = 0;
        volatile void __iomem *ddibus_io = dev->ddibus_io;;
        
        if(offset & 0x3){
                return val;
        }
        
        if(ddibus_io != NULL) {
                val = ioread32((void*)(ddibus_io + offset));
        }
        return val;
}

static void ddi_reg_write(struct tcc_hdmi_dev *dev, unsigned int data, unsigned int offset)
{
        volatile void __iomem *ddibus_io = dev->ddibus_io;;
        
        if(offset & 0x3){
                return;
        }
        
        if(ddibus_io != NULL) {
                iowrite32(data, (void*)(ddibus_io + offset));
        }
}


static unsigned int hdmi_reg_read(struct tcc_hdmi_dev *dev, unsigned int offset)
{
        unsigned int val = 0;
        volatile void __iomem *hdmi_io = NULL;
        
        if(offset & 0x3){
                return val;
        }
       
        if(dev != NULL) {
                if(offset >= HDMIDP_PHYREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_phy_io;
                        offset -= HDMIDP_PHYREG(0);
                } else if(offset >= HDMIDP_I2SREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_SPDIFREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_AESREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMIREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_core_io;
                        offset -= HDMIDP_HDMIREG(0);
                } else if(offset >= HDMIDP_HDMI_SSREG(0)) { 
                        hdmi_io = (volatile void __iomem *)dev->hdmi_ctrl_io;
                        offset -= HDMIDP_HDMI_SSREG(0);
                }
        }
        if(hdmi_io == NULL) {
                pr_err("%s offset(0x%x) is out of range\r\n", __func__, offset);
        } else {
                //pr_info(" >> Read (%p)\r\n", (void*)(hdmi_io + offset));
                val = ioread32((void*)(hdmi_io + offset));
        }
        return val;
}

static void hdmi_reg_write(struct tcc_hdmi_dev *dev, unsigned int data, unsigned int offset)
{
        volatile void __iomem *hdmi_io = NULL;

        if(offset & 0x3){
                return;
        }
       
        if(dev != NULL) {
                if(offset >= HDMIDP_PHYREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_phy_io;
                        offset -= HDMIDP_PHYREG(0);
                } else if(offset >= HDMIDP_I2SREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_SPDIFREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_AESREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMIREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_core_io;
                        offset -= HDMIDP_HDMIREG(0);
                } else if(offset >= HDMIDP_HDMI_SSREG(0)) { 
                        hdmi_io = (volatile void __iomem *)dev->hdmi_ctrl_io;
                        offset -= HDMIDP_HDMI_SSREG(0);
                }
        }
        if(hdmi_io == NULL) {
                pr_err("%s offset(0x%x) is out of range\r\n", __func__, offset);
        } else {
                //pr_info(" >> Write(%p) = 0x%x\r\n", (void*)(hdmi_io + offset), data);
                iowrite32(data, (void*)(hdmi_io + offset));
        }
}


//! Structure for video timing parameters
static const struct device_video_params HDMIVideoParams[] =
{
	{ 800 , 160 , 525 , 45, 16  , 96 , 1, 10, 2, 1, 1 , 1 , 0, 0, },	// v640x480p_60Hz
	{ 858 , 138 , 525 , 45, 16  , 62 , 1, 9 , 6, 1, 2 , 3 , 0, 0, },	// v720x480p_60Hz
	{ 1650, 370 , 750 , 30, 110 , 40 , 0, 5 , 5, 0, 4 , 4 , 0, 0, },	// v1280x720p_60Hz_3D
	{ 1650, 370 , 750 , 30, 110 , 40 , 0, 5 , 5, 0, 4 , 4 , 0, 0, },	// v1280x720p_60Hz
	{ 2200, 280 , 1125, 22, 88  , 44 , 0, 2 , 5, 0, 5 , 5 , 1, 0, },	// v1920x1080i_60H
	{ 1716, 276 , 525 , 22, 38  , 124, 1, 4 , 3, 1, 6 , 7 , 1, 1, },	// v720x480i_60Hz
	{ 1716, 276 , 262 , 22, 38  , 124, 1, 4 , 3, 1, 8 , 9 , 0, 1, },	// v720x240p_60Hz
	{ 3432, 552 , 525 , 22, 76  , 248, 1, 4 , 3, 1, 10, 11, 1, 1, },	// v2880x480i_60Hz
	{ 3432, 552 , 262 , 22, 76  , 248, 1, 4 , 3, 1, 12, 13, 0, 1, },	// v2880x240p_60Hz
	{ 1716, 276 , 525 , 45, 32  , 124, 1, 9 , 6, 1, 14, 15, 0, 1, },	// v1440x480p_60Hz
	{ 2200, 280 , 1125, 45, 88  , 44 , 0, 4 , 5, 0, 16, 16, 0, 0, },	// [10] v1920x1080p_60H
	{ 864 , 144 , 625 , 49, 12  , 64 , 1, 5 , 5, 1, 17, 18, 0, 0, },	// v720x576p_50Hz
	{ 1980, 700 , 750 , 30, 440 , 40 , 0, 5 , 5, 0, 19, 19, 0, 0, },	// v1280x720p_50Hz
	{ 2640, 720 , 1125, 22, 528 , 44 , 0, 2 , 5, 0, 20, 20, 1, 0, },	// v1920x1080i_50H
	{ 1728, 288 , 625 , 24, 24  , 126, 1, 2 , 3, 1, 21, 22, 1, 1, },	// v720x576i_50Hz
	{ 1728, 288 , 312 , 24, 24  , 126, 1, 2 , 3, 1, 23, 24, 0, 1, },	// v720x288p_50Hz
	{ 3456, 576 , 625 , 24, 48  , 252, 1, 2 , 3, 1, 25, 26, 1, 1, },	// v2880x576i_50Hz
	{ 3456, 576 , 312 , 24, 48  , 252, 1, 2 , 3, 1, 27, 28, 0, 1, },	// v2880x288p_50Hz
	{ 1728, 288 , 625 , 49, 24  , 128, 1, 5 , 5, 1, 29, 30, 0, 1, },	// v1440x576p_50Hz
	{ 2640, 720 , 1125, 45, 528 , 44 , 0, 4 , 5, 0, 31, 31, 0, 0, },	// v1920x1080p_50Hz
	{ 2750, 830 , 1125, 45, 638 , 44 , 0, 4 , 5, 0, 32, 32, 0, 0, },	// [20] v1920x1080p_23.976Hz
	{ 2750, 830 , 1125, 45, 638 , 44 , 0, 4 , 5, 0, 32, 32, 0, 0, },	// v1920x1080p_24Hz_3D
	{ 2750, 830 , 1125, 45, 638 , 44 , 0, 4 , 5, 0, 32, 32, 0, 0, },	// v1920x1080p_24Hz
	{ 2640, 720 , 1125, 45, 528 , 44 , 0, 4 , 5, 0, 33, 33, 0, 0, },	// v1920x1080p_25Hz
	{ 2200, 280 , 1125, 45, 88  , 44 , 0, 4 , 5, 0, 34, 34, 0, 0, },	// v1920x1080p_30Hz

	{ 3432, 552 , 525 , 45, 64  , 248, 1, 9 , 6, 1, 35, 36, 0, 1, },	// [25] v2880x480p_60Hz
	{ 3456, 576 , 625 , 49, 48  , 256, 1, 5 , 5, 1, 37, 38, 0, 1, },	// v2880x576p_50Hz
	{ 2304, 384 , 1250, 85, 32  , 168, 0, 23, 5, 1, 39, 39, 1, 0, },	// v1920x1080i_50Hz(1250)
	{ 2640, 720 , 1125, 22, 528 , 44 , 0, 2 , 5, 0, 40, 40, 1, 0, },	// v1920x1080i_100Hz
	{ 1980, 700 , 750 , 30, 440 , 40 , 0, 5 , 5, 0, 41, 41, 0, 0, },	// v1280x720p_100Hz
	{ 864 , 144 , 625 , 49, 12  , 64 , 1, 5 , 5, 1, 42, 43, 0, 0, },	// [30] v720x576p_100Hz
	{ 1728, 288 , 625 , 24, 24  , 126, 1, 2 , 3, 1, 44, 45, 1, 1, },	// v720x576i_100Hz
	{ 2200, 280 , 1125, 22, 88  , 44 , 0, 2 , 5, 0, 46, 46, 1, 0, },	// v1920x1080i_120Hz
	{ 1650, 370 , 750 , 30, 110 , 40 , 0, 5 , 5, 0, 47, 47, 0, 0, },	// v1280x720p_120Hz
	{ 858 , 138 , 525 , 54, 16  , 62 , 1, 9 , 6, 1, 48, 49, 0, 0, },	// v720x480p_120Hz
	{ 1716, 276 , 525 , 22, 38  , 124, 1, 4 , 3, 1, 50, 51, 1, 1, },	// v720x480i_120Hz
	{ 864 , 144 , 625 , 49, 12  , 64 , 1, 5 , 5, 1, 52, 53, 0, 0, },	// v720x576p_200Hz
	{ 1728, 288 , 625 , 24, 24  , 126, 1, 2 , 3, 1, 54, 55, 1, 1, },	// v720x576i_200Hz
	{ 858 , 138 , 525 , 45, 16  , 62 , 1, 9 , 6, 1, 56, 57, 0, 0, },	// v720x480p_240Hz
	{ 1716, 276 , 525 , 22, 38  , 124, 1, 4 , 3, 1, 58, 59, 1, 1, },	// v720x480i_240Hz
/*	
	{ 3300, 2020, 750 , 30, 1760, 40 , 0, 5 , 5, 0, 60, 60, 0, 0, },	// v1280x720p24Hz
	{ 3960, 2680, 750 , 30, 2420, 40 , 0, 5 , 5, 0, 61, 61, 0, 0, },        // v1280x720p25Hz
	{ 3300, 2020, 750 , 30, 1760, 40 , 0, 5 , 5, 0, 62, 62, 0, 0, },	// v1280x720p30Hz
	{ 2200, 280 , 1125, 45, 88  , 44 , 0, 4 , 5, 0, 63, 63, 0, 0, },	// v1920x1080p120Hz
	{ 2640, 720 , 1125, 45, 528 , 44 , 0, 4 , 5, 0, 64, 64, 0, 0, },	// v1920x1080p100Hz
	{ 4400, 560 , 2250, 90, 176 , 88 , 0, 8 , 10, 0, 1, 1, 0, 0, },	    	// v4Kx2K_30Hz
*/
	{ 2200, 280 , 1125, 45, 88  , 44 , 0, 4 , 5, 0, 34, 34, 0, 0, },	// [40] v1920x1080p_29.976Hz	
       { 2080, 160 , 780,   60, 60  , 20 , 0, 2 , 1, 0,   0, 0,  0, 0, }        // v1920x720p_60H

};


/**
 * N value of ACR packet.@n
 * 4096  is the N value for 32 KHz sampling frequency @n
 * 6272  is the N value for 44.1 KHz sampling frequency @n
 * 12544 is the N value for 88.2 KHz sampling frequency @n
 * 25088 is the N value for 176.4 KHz sampling frequency @n
 * 6144  is the N value for 48 KHz sampling frequency @n
 * 12288 is the N value for 96 KHz sampling frequency @n
 * 24576 is the N value for 192 KHz sampling frequency @n
 */
static const unsigned int ACR_N_params[] =
{
    4096,
    6272,
    12544,
    25088,
    6144,
    12288,
    24576
};


/**
 * PHY register setting values for each Pixel clock and Bit depth (8, 10, 12 bit).
 * @see  Setting values are came from LN28LPP_HDMI_v1p4_TX_PHY_DataSheet_REV1.2.pdf document.
 */

// TCC896X & TCC897x HDMI PHY Setting
static const unsigned char hdmi_phy_config[][3][32] =
{
	//  0: 25.200Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x52,0x3F,0x55,0x40,0x01,0x00,0xC8,0x82,0xC8,0xBD,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x01,0x84,0x05,0x02,0x24,0x86,0x54,0xF4,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0x52,0x69,0x75,0x57,0x01,0x00,0xC8,0x82,0xC8,0x3B,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x01,0x84,0x05,0x02,0x24,0x86,0x54,0xC3,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0x52,0x3F,0x35,0x63,0x01,0x00,0xC8,0x82,0xC8,0xBD,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xA3,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//  1: 25.175Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x1F,0x50,0x40,0x20,0x1E,0xC8,0x81,0xE8,0xBD,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xF4,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x27,0x51,0x55,0x40,0x2B,0xC8,0x81,0xE8,0xEC,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xC3,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x1F,0x30,0x63,0x20,0x1E,0xC8,0x81,0xE8,0xBD,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xA3,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//  2: 27.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x22,0x51,0x40,0x08,0xFC,0xE0,0x98,0xE8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xE4,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x2A,0x52,0x55,0x08,0x03,0xC8,0x86,0xE8,0xFD,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xB6,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x33,0x54,0x65,0x08,0xFA,0xC8,0x85,0xE8,0x30,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x98,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//  3: 27.027Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x2D,0x72,0x40,0x64,0x12,0xC8,0x43,0xE8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xE3,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x38,0x74,0x57,0x50,0x31,0xC1,0x80,0xC8,0x52,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xB6,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD4,0x87,0x31,0x63,0x64,0x1B,0xE0,0x19,0xE8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x98,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//  4: 54.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x51,0x2D,0x35,0x40,0x01,0x00,0xC8,0x82,0xC8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xE4,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x38,0x35,0x53,0x08,0x04,0xC8,0x88,0xE8,0x52,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xB6,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x22,0x15,0x61,0x08,0xFC,0xC8,0x82,0xC8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x98,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	// 5: 54.054Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x2D,0x32,0x40,0x64,0x12,0xC8,0x43,0xE8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xE3,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD2,0x70,0x34,0x53,0x50,0x31,0xC8,0x80,0xC8,0x52,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xB6,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD4,0x87,0x11,0x61,0x64,0x1B,0xE0,0x19,0xE8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x98,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	//74.250Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x1F,0x10,0x40,0x40,0xF8,0xC8,0x81,0xE8,0xBA,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xA5,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x27,0x11,0x51,0x40,0xD6,0xC8,0x81,0xE8,0xE8,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x84,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x2E,0x12,0x61,0x40,0x34,0xC8,0x82,0xE8,0x16,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xB9,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	//74.176Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x1F,0x10,0x40,0x5B,0xEF,0xC8,0x81,0xE8,0xB9,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xA6,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x27,0x14,0x51,0x5B,0xA7,0xC8,0x84,0xE8,0xE8,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x85,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD2,0x5D,0x12,0x61,0x5B,0xCD,0xD0,0x43,0xE8,0x16,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xBA,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	//148.500Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x1F,0x00,0x40,0x40,0xF8,0xC8,0x81,0xE8,0xBA,0xD8,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x4B,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x27,0x01,0x50,0x40,0xD6,0xC8,0x81,0xE8,0xE8,0xD8,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x09,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x2E,0x02,0x60,0x40,0x34,0xC8,0x82,0xE8,0x16,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xDD,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	//148.352Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x1F,0x00,0x40,0x5B,0xEF,0xC8,0x81,0xE8,0xB9,0xD8,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x4B,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x27,0x04,0x50,0x5B,0xA7,0xC8,0x84,0xE8,0xE8,0xD8,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x09,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD2,0x5D,0x02,0x60,0x5B,0xCD,0xD0,0x43,0xE8,0x16,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xDD,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	// 10: 108.108Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x2D,0x12,0x40,0x64,0x12,0xC8,0x43,0xE8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xC7,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD2,0x70,0x14,0x51,0x50,0x31,0xC8,0x80,0xC8,0x52,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x6C,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD4,0x87,0x01,0x60,0x64,0x1B,0xE0,0x19,0xE8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x2F,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	//72.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x51,0x1E,0x15,0x40,0x01,0x00,0xC8,0x82,0xC8,0xB4,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xAB,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0x52,0x4B,0x15,0x51,0x01,0x00,0xC8,0x82,0xC8,0xE1,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x89,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0x51,0x2D,0x15,0x61,0x01,0x00,0xC8,0x82,0xC8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xC7,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	//25.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x2A,0x72,0x40,0x3C,0xD8,0xC8,0x86,0xE8,0xFA,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xF6,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x27,0x51,0x55,0x40,0x08,0xC8,0x81,0xE8,0xEA,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xC5,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x1F,0x30,0x63,0x40,0x20,0xC8,0x81,0xE8,0xBC,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xA4,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//65.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x36,0x34,0x40,0x0C,0x04,0xC8,0x82,0xE8,0x45,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xBD,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x22,0x11,0x51,0x30,0xF2,0xC8,0x86,0xE8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x97,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x29,0x12,0x61,0x40,0xD0,0xC8,0x87,0xE8,0xF4,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x7E,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	//108.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x51,0x2D,0x15,0x40,0x01,0x00,0xC8,0x82,0xC8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xC7,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x38,0x14,0x51,0x08,0x04,0xC8,0x80,0xC8,0x52,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x6C,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x22,0x01,0x60,0x08,0xFC,0xC8,0x86,0xE8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x2F,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	// 15: 162.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x54,0x87,0x05,0x40,0x01,0x00,0xC8,0x82,0xC8,0xCB,0xD8,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x2F,0x25,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x2A,0x02,0x50,0x40,0x18,0xC8,0x86,0xE8,0xFD,0xD8,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xF3,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x33,0x04,0x60,0x40,0xD0,0xC8,0x85,0xE8,0x30,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xCA,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	//45.000Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x52,0x4B,0x35,0x40,0x01,0x00,0xC8,0x82,0xC8,0xE1,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x11,0x25,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x2F,0x32,0x53,0x40,0xF0,0xC8,0x81,0xE8,0x19,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xDA,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x38,0x34,0x63,0x40,0x20,0xC8,0x80,0xC8,0x52,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xB6,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	//44.955Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x25,0x31,0x40,0x5B,0x54,0xC8,0x83,0xE8,0xE1,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x11,0x25,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD2,0x5D,0x32,0x53,0x5B,0x3B,0xD0,0x83,0xE8,0x19,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xDB,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD2,0x70,0x35,0x63,0x5B,0x23,0xC8,0x88,0xE8,0x51,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xB6,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	//297.000Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x3E,0x05,0x40,0x40,0xE0,0xC8,0x42,0xE8,0x73,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xA5,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3E,0x05,0x40,0x40,0xE0,0xC8,0x42,0xE8,0x73,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xA5,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3E,0x05,0x40,0x40,0xE0,0xC8,0x42,0xE8,0x73,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xA5,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	//296.703Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x3E,0x05,0x40,0x5B,0xDE,0xC8,0x82,0xE8,0x73,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xA6,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3E,0x05,0x40,0x5B,0xDE,0xC8,0x82,0xE8,0x73,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xA6,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3E,0x05,0x40,0x5B,0xDE,0xC8,0x82,0xE8,0x73,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0xA6,0x24,0x03,0x00,0x00,0x01,0x80,0x90},
	},
	
	// 20: 59.400Mhz
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x52,0x63,0x35,0x40,0x01,0x00,0xC8,0x82,0xC8,0x29,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xCF,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x1F,0x10,0x51,0x40,0xF8,0xC8,0x81,0xE8,0xBA,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xA5,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x25,0x11,0x61,0x40,0x10,0xC8,0x83,0xE8,0xDF,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x8A,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	//36.000Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0x51,0x2D,0x55,0x40,0x40,0x00,0xC8,0x02,0xC8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xAB,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0x51,0x2D,0x55,0x40,0x40,0x00,0xC8,0x02,0xC8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xAB,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0x51,0x2D,0x55,0x40,0x40,0x00,0xC8,0x02,0xC8,0x0E,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0xAB,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//40.000Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x21,0x31,0x40,0x3C,0x28,0xC8,0x87,0xE8,0xC8,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x9A,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x21,0x31,0x40,0x3C,0x28,0xC8,0x87,0xE8,0xC8,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x9A,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x21,0x31,0x40,0x3C,0x28,0xC8,0x87,0xE8,0xC8,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x9A,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//71.000Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x3B,0x35,0x40,0x0C,0x04,0xC8,0x85,0xE8,0x63,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x57,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3B,0x35,0x40,0x0C,0x04,0xC8,0x85,0xE8,0x63,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x57,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3B,0x35,0x40,0x0C,0x04,0xC8,0x85,0xE8,0x63,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x57,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	//83.500Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x23,0x11,0x40,0x0C,0xFB,0xC8,0x85,0xE8,0xD1,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x4A,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x23,0x11,0x40,0x0C,0xFB,0xC8,0x85,0xE8,0xD1,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x4A,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x23,0x11,0x40,0x0C,0xFB,0xC8,0x85,0xE8,0xD1,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x4A,0x24,0x00,0x00,0x00,0x01,0x80,0x90},
	},
	
	// 25: 106.500Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x2C,0x12,0x40,0x0C,0x09,0xC8,0x84,0xE8,0x0A,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x73,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x2C,0x12,0x40,0x0C,0x09,0xC8,0x84,0xE8,0x0A,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x73,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x2C,0x12,0x40,0x0C,0x09,0xC8,0x84,0xE8,0x0A,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x73,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	//122.500Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x33,0x14,0x40,0x0C,0x01,0xC8,0x85,0xE8,0x32,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x64,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x33,0x14,0x40,0x0C,0x01,0xC8,0x85,0xE8,0x32,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x64,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x33,0x14,0x40,0x0C,0x01,0xC8,0x85,0xE8,0x32,0xD9,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x64,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
	
	// 27: 146.250Mhz
	//We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
	{
	   // TMDS Data Amplitude[4:0] : (Reg40 Bit<3:0>, REG3C Bit<7>), TMDS Clock Amplitude[3:0] : (Reg5C Bit<7:3>), TMDS Pre-emphasis Control : (Reg40 Bit<7:4>)
	   //0x04,0x08,0x0C,0x10,0x14.0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x80
		{0xD1,0x3D,0x15,0x40,0x18,0xFD,0xC8,0x83,0xE8,0x6E,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x54,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3D,0x15,0x40,0x18,0xFD,0xC8,0x83,0xE8,0x6E,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x54,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
		{0xD1,0x3D,0x15,0x40,0x18,0xFD,0xC8,0x83,0xE8,0x6E,0xD9,0x45,0xA0,0xAC,0x30,0x0e,0x80,0x09,0x84,0x05,0x02,0x24,0xe4,0x54,0x54,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
	},
        // 28: 97.34Mhz 
        //  change to lcdc clock        nPCLKCTRL.sel = PCLKCTRL_SEL_HDMITMDS;
        //We only support 8bit mode, 10bit and 12bit is same setting with 8bit. If you set 10bit and 12bit, there is no meaning.
        {
               {0xD1,0x29,0x12,0x61,0x40,0xD0,0xC8,0x87,0xE8,0xF4,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x7E,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
               {0xD1,0x29,0x12,0x61,0x40,0xD0,0xC8,0x87,0xE8,0xF4,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x7E,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
               {0xD1,0x29,0x12,0x61,0x40,0xD0,0xC8,0x87,0xE8,0xF4,0xD8,0x45,0xA0,0xAC,0x30,0x08,0x80,0x09,0x84,0x05,0x02,0x24,0x86,0x54,0x7E,0x24,0x01,0x00,0x00,0x01,0x80,0x90},
        }
};







// onoff : 1 - power down
static void tcc_ddi_pwdn_hdmi(struct tcc_hdmi_dev *dev, char onoff)
{
        unsigned int val;

        if(dev != NULL) {
                // HDMI Power-on
                val = ddi_reg_read(dev, DDICFG_PWDN);
                if(onoff) {
                	val &= ~PWDN_HDMI;
                } else {
                	val |= PWDN_HDMI;
                }
                ddi_reg_write(dev, val, DDICFG_PWDN);	
        }
}

// onoff : 1 -reset
static void tcc_ddi_swreset_hdmi(struct tcc_hdmi_dev *dev, char onoff)
{
        unsigned int val;
                
        if(dev != NULL) {
                // HDMI Power-on
                val = ddi_reg_read(dev, DDICFG_PWDN);
                if(onoff) {
                        val &= ~SWRESET_HDMI;
                } else {
                        val |= SWRESET_HDMI;
                }
                ddi_reg_write(dev, val, DDICFG_PWDN);   
        }
}

// onoff : 1 -power on
static void tcc_ddi_hdmi_ctrl(struct tcc_hdmi_dev *dev, unsigned int index, char onoff)
{
	unsigned int val;
        unsigned int offset = 0x1000;
        if(dev != NULL) {
        	val = ddi_reg_read(dev, DDICFG_HDMICTRL);
        	
         	switch(index) {
                        case HDMICTRL_RESET_HDMI:
                              if(onoff) {
                                        val &= ~HDMICTRL_RESET_HDMI;
                                } else {
                                        val |= HDMICTRL_RESET_HDMI;
                                }
                                offset = DDICFG_HDMICTRL;
                		break;

        		case HDMICTRL_RESET_SPDIF:
                                if(onoff) {
                                        val &= ~HDMICTRL_RESET_SPDIF;
                                } else {
                                        val |= HDMICTRL_RESET_SPDIF;
                                }
                                offset = DDICFG_HDMICTRL;
                                break;
        		
        		case HDMICTRL_RESET_TMDS:
                                if(onoff) {
                                        val &= ~HDMICTRL_RESET_TMDS;
                                } else {
                                        val |= HDMICTRL_RESET_TMDS;
                                }
                                offset = DDICFG_HDMICTRL;
                                break;
        		
        		case HDMICTRL_HDMI_ENABLE:
                                if(onoff) {
                                        val |= HDMICTRL_HDMI_ENABLE;
                                } else {
                                        val &= ~HDMICTRL_HDMI_ENABLE;
                                }
                                offset = DDICFG_HDMICTRL;
                                break;
        	}
                
                if(offset < 0x1000) {
                        ddi_reg_write(dev, val, DDICFG_HDMICTRL);   
                }
        }
}

/**
 * hdmi_phy_reset
 */
static void hdmi_phy_reset(struct tcc_hdmi_dev *dev)
{
        unsigned int val;

        if(dev != NULL) {
                // HDMI PHY Reset
                val = ddi_reg_read(dev, DDICFG_HDMICTRL);

                val |= HDMICTRL_HDMI_ENABLE;
                val &= ~HDMICTRL_RESET_SPDIF;
                val &= ~HDMICTRL_RESET_TMDS;
                val |= HDMICTRL_RESET_HDMI;

                ddi_reg_write(dev, val, DDICFG_HDMICTRL);

                tcc_ddi_swreset_hdmi(dev, 1);
                udelay(1);
                tcc_ddi_swreset_hdmi(dev, 0);

                udelay(100);        // min 25us wait is needed for resetting phy.               

                val = ddi_reg_read(dev, DDICFG_HDMICTRL);

                val |= HDMICTRL_HDMI_ENABLE;
                val &= ~HDMICTRL_RESET_SPDIF;
                val &= ~HDMICTRL_RESET_TMDS;
                val &= ~HDMICTRL_RESET_HDMI;

                ddi_reg_write(dev, val, DDICFG_HDMICTRL);

                val = ddi_reg_read(dev, DDICFG_HDMICTRL);
                #if defined(CONFIG_HDMI_CLK_USE_XIN_24MHZ)
                val |= (1 << 9);
                val |= (1 << 8);
                #else
                val &= ~(3 << 8);
                #endif /* CONFIG_HDMI_CLK_USE_XIN_24MHZ */
                
                // This option must be enable at TCC897x
                // Internal Clock Setting
                val |= (1 << 11);
                // In TCC897x HDMI Link, Encoding option is must disabled.
                val |= (0 << 14);
                ddi_reg_write(dev, val, DDICFG_HDMICTRL);

                udelay(1000);
        }
}


/**
 * Set checksum in SPD InfoFrame Packet. @n
 * Calculate a checksum and set it in packet.
 */
static void hdmi_spd_update_checksum(struct tcc_hdmi_dev *dev)
{
        unsigned char index, checksum = SPD_HEADER;

        if(dev != NULL) {
                for (index = 0; index < SPD_PACKET_BYTE_LENGTH; index++) {
                        checksum += hdmi_reg_read(dev, HDMI_SPD_DATA1 + 4*index);
                }
                checksum = ~checksum +1;
                hdmi_reg_write(dev, checksum, HDMI_SPD_CHECK_SUM);
        }
}

/**
 * Set checksum in Audio InfoFrame Packet. @n
 * Calculate a checksum and set it in packet.
 */
static void hdmi_aui_update_checksum(struct tcc_hdmi_dev *dev)
{
        unsigned char index, checksum;

        hdmi_reg_write(dev, AUI_HEADER_BYTE0, HDMI_AUI_HEADER0);
        hdmi_reg_write(dev, AUI_HEADER_BYTE1, HDMI_AUI_HEADER1);
        hdmi_reg_write(dev, AUI_HEADER_BYTE2, HDMI_AUI_HEADER2);

        checksum = AUI_HEADER;
        if(dev != NULL) {
                for (index = 0; index < AUI_PACKET_BYTE_LENGTH; index++)
                {
                        checksum += hdmi_reg_read(dev, HDMI_AUI_BYTE1 + 4*index);
                }
                checksum = ~checksum +1;
                hdmi_reg_write(dev, checksum, HDMI_AUI_CHECK_SUM);
        }
}


/**
 * Set checksum in AVI InfoFrame Packet. @n
 * Calculate a checksum and set it in packet.
 */
static void hdmi_avi_update_checksum(struct tcc_hdmi_dev *dev)
{
        unsigned char index, checksum;

        checksum = AVI_HEADER;

        if(dev != NULL) {
                hdmi_reg_write(dev, AVI_HEADER_BYTE0, HDMI_AVI_HEADER0);
                hdmi_reg_write(dev, AVI_HEADER_BYTE1, HDMI_AVI_HEADER1);
                hdmi_reg_write(dev, AVI_HEADER_BYTE2, HDMI_AVI_HEADER2);

                for (index = 0; index < AVI_PACKET_BYTE_LENGTH; index++)
                {
                        checksum += hdmi_reg_read(dev, HDMI_AVI_BYTE1 + 4*index);
                }
                checksum = ~checksum +1;
                hdmi_reg_write(dev, checksum, HDMI_AVI_CHECK_SUM);
        }
}

/**
 * Set color space in HDMI H/W. @n
 * @param   space   [in] Color space
 * @return  If argument is invalid, return -1;Otherwise return 0.
 */
static int hdmi_set_color_space(struct tcc_hdmi_dev *dev, enum ColorSpace space)
{
        int ret = -1;
        unsigned int val_con0, val_avi_byte1;
        //VIOC_DISP *pDispBase;
	//unsigned int iPXWD=12, iR2YMD=0, iR2Y=0, iSWAP=0;

        if(dev != NULL) {
                val_con0 = hdmi_reg_read(dev, HDMI_CON_0);
                val_avi_byte1 = hdmi_reg_read(dev, HDMI_AVI_BYTE1);
                
                // clear fields
                val_con0 &= ~HDMI_YCBCR422_ENABLE;
                val_avi_byte1 &= ~(AVI_CS_Y422|AVI_CS_Y444);

                switch(space) {
                        case HDMI_CS_YCBCR422:
                                // set video input interface
                                val_con0 |= HDMI_YCBCR422_ENABLE;
                                val_avi_byte1 |= AVI_CS_Y422;
                                ret = 0;
                                break;
                                /*
                                	iPXWD = 8;
                                	iR2YMD = 3;
                                	iR2Y = 1;
                                	iSWAP =0;
                                */
                        case HDMI_CS_YCBCR444:
                                val_avi_byte1 |= AVI_CS_Y444;
                                ret = 0;
                                break;
                                /*      
                                        iPXWD = 12;
                        		iR2YMD = 3;
                        		iR2Y = 1;
                        		iSWAP = 4;
                                */
                        case HDMI_CS_RGB:
                                ret = 0;
                                break;
                                /*
                                        iPXWD = 12;
                        		iR2YMD = 0;
                        		iR2Y = 0;
                        		iSWAP = 0;
                                */      
                        default:
                                break;
                }

                hdmi_reg_write(dev, val_con0, HDMI_CON_0);
                hdmi_reg_write(dev, val_avi_byte1, HDMI_AVI_BYTE1);       

                /*
                VIOC_DISP_SetPXDW(pDispBase,  iPXWD);
                VIOC_DISP_SetR2YMD(pDispBase, iR2YMD);
                VIOC_DISP_SetR2Y(pDispBase, iR2Y);
                #if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
                VIOC_DISP_SetR2Y(pDispBase, 0);
                VIOC_DISP_SetY2R(pDispBase, iR2Y?0:1);
                #endif
                VIOC_DISP_SetSWAP(pDispBase, iSWAP);
                */
        }
        return ret;
}


#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
static int hdmi_set_yuv420_color_space(struct tcc_hdmi_dev *dev)
{
        unsigned int val_con0,val_avi_byte1;

        if(dev != NULL) {
                val_con0 = hdmi_reg_read(dev, HDMI_CON_0);
                val_avi_byte1 = hdmi_reg_read(dev, HDMI_AVI_BYTE1);
                
                // clear fields
                val_avi_byte1 &= ~AVI_CS_Y444;
                
                // set video input interface
                val_con0 |= HDMI_YCBCR422_ENABLE;
                val_avi_byte1 |= AVI_CS_Y422;
                
                hdmi_reg_write(dev, val_con0, HDMI_CON_0);
                hdmi_reg_write(dev, val_avi_byte1, HDMI_AVI_BYTE1);
        }
        return 0;
}
#endif

/**
 * Set color depth.@n
 * @param   depth   [in] Color depth of input vieo stream
 * @return  If argument is invalid, return 0;Otherwise return 1.
 */
static int hdmi_set_color_depth(struct tcc_hdmi_dev *dev, enum ColorDepth depth)
{
	int ret = -1;

        if(dev != NULL) {
        	switch (depth)
        	{
        		case HDMI_CD_36:
        			// set GCP CD
        			hdmi_reg_write(dev, GCP_CD_36BPP,HDMI_GCP_BYTE2);
        			// set DC_CTRL
        			hdmi_reg_write(dev, HDMI_DC_CTL_12,HDMI_DC_CONTROL);
                                ret = 0;
        			break;
        		case HDMI_CD_30:
        			// set GCP CD
        			hdmi_reg_write(dev, GCP_CD_30BPP,HDMI_GCP_BYTE2);
        			// set DC_CTRL
        			hdmi_reg_write(dev, HDMI_DC_CTL_10,HDMI_DC_CONTROL);
                                ret = 0;
        			break;
        		case HDMI_CD_24:
        			// set GCP CD
        			hdmi_reg_write(dev, GCP_CD_24BPP,HDMI_GCP_BYTE2);
        			// set DC_CTRL
        			hdmi_reg_write(dev, HDMI_DC_CTL_8,HDMI_DC_CONTROL);
                                ret = 0;
        			break;
        		default:
        			break;
        	}
        }
	return ret;
}

/**
 * Set Phy Freq.@n
 * @param   freq   [in] freq of phy
 * @return  If argument is invalid, return 0;Otherwise return 1.
 */
static int hdmi_set_phy_freq(struct tcc_hdmi_dev *dev, enum PHYFreq freq)
{
        int phy_loop, ret = -1;
        unsigned char *phy_buffer = NULL;
        int phy_depth_index, phy_reg_count;

        do {
                if(dev == NULL) {
                        pr_err("%s device is NULL at line(%d)\r\n", __func__, __LINE__);
                        break;
                }
                
                phy_depth_index = dev->phy_depth;

                if(freq >= PHY_FREQ_MAX) {
                        pr_err("%s out of range at line(%d)\r\n", __func__, __LINE__);
                        break;
                }
        
                if(phy_depth_index > 2) {
                        pr_err("%s out of range at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                pr_info("%s freq=%d, depth=%d\r\n", __func__, freq, phy_depth_index);
        
                phy_reg_count = (sizeof(hdmi_phy_config[freq][phy_depth_index]) / sizeof(hdmi_phy_config[freq][phy_depth_index][0])) -2;
                if(phy_reg_count < 0) {
                        pr_err("%s out of range at line(%d)\r\n", __func__, __LINE__);
                        break;
                }
                
                phy_buffer = (unsigned char *) hdmi_phy_config[freq][phy_depth_index];

        	#if (HDMI_DEBUG)
        	{
        		printk("%s : freq = %d, phy_depth = %d, phy_reg_count = %d\n",__func__,(int)freq, phy_depth_index, phy_reg_count);

        		for(phy_loop = 0; phy_loop < phy_reg_count; phy_loop++)
        		{
        			printk("0x%02x ", phy_buffer[phy_loop]);
        		}
        		
        		printk("\n");
        	}
        	#endif

                // Clear Top:MODE_SET_DONE.
                // If you set the PHY setting, you must clear Mode_Set_Done.
        	hdmi_reg_write(dev, 0x00, HDMIDP_PHYREG(0x7C));

        	for(phy_loop = 0; phy_loop < phy_reg_count; phy_loop++) {
        		hdmi_reg_write(dev, phy_buffer[phy_loop], HDMIDP_PHYREG((phy_loop+1) << 2));
        	}

        	hdmi_reg_write(dev, 0x90, HDMIDP_PHYREG(0x8C));

        	// Set Top:MODE_SET_DONE.
        	//If you apply the PHY Setting, you must set MODE_SET_DONE>
        	hdmi_reg_write(dev, 0x80, HDMIDP_PHYREG(0x7C));

        	msleep(16);

        	#if (HDMI_DEBUG)
        	{
                        unsigned int val;
        		for(phy_loop = 0; phy_loop < phy_reg_count; phy_loop++) {
        			val = hdmi_reg_read(dev, HDMIDP_PHYREG((phy_loop+1) << 2));
        			printk("0x%02x ", val);
        		}
        		
        		printk("\n");
        	}
        	#endif
                ret = 0;
        } while(0);
	return ret;
}

static int hdmi_set_phy_pwdn(struct tcc_hdmi_dev *dev, unsigned char enable)
{
        if(dev != NULL) {
        	if(enable) {
        	        // Clear Top:MODE_SET_DONE.
        		// If you set the PHY setting, you must clear Mode_Set_Done.
        		hdmi_reg_write(dev, 0x00, HDMIDP_PHYREG(0x7C));

        		//TX Power down(Reg74) Bit<6:4>,Bit<2:0> : refer LN28LPP_HDMI_v1p4_TX_PHY_DataSheet_REV1.2.pdf 
        		// Bit<6> : PLL PD(PLL & Bias Block Power Down)
        		// Bit<5> : TX_CLKSER_PD (Clock Serializer Power Down)
        		// Bit<4> : TX_CLKDRV_PD (TMDS Clock Driver Power Down)
        		// Bit<2> : TX_DRV_PD (TMDS Data Driver Power Down)
        		// Bit<1> : TX_SER_PD (TMDS Data Serializer Power Down)
        		// Bit<0> : TX_CLK_PD (TMDS Internal Clock Buffer / Divider Power Down)
        		hdmi_reg_write(dev, 0x36, HDMIDP_PHYREG(0x74));	

        		// Set Top:MODE_SET_DONE.
        		//If you apply the PHY Setting, you must set MODE_SET_DONE>
        		hdmi_reg_write(dev, 0x80, HDMIDP_PHYREG(0x7C));

        		#if (HDMI_DEBUG)
        		{
        			unsigned int reg = hdmi_reg_read(dev, HDMIDP_PHYREG(0x74));
        			printk("Reg74  = 0x%02x \n", reg);
        		}
        		#endif
        	}
        }

	return 0;
}

/**
 * Set VIC field in AVI Packet.@n
 * Set HDMI VSI packet in case of 3D video mode or HDMI VIC mode.
 *
 * @param  pVideo       [in] Video parameter
 * @return  If argument is invalid, return 0; Otherwise return 1.
 */
static int hdmi_set_aux_data(struct tcc_hdmi_dev *dev, struct HDMIVideoParameter* pVideo)
{
        int ret = -1;
        enum HDMI3DVideoStructure mode = pVideo->hdmi_3d_format;
        unsigned int reg;
        dprintk(KERN_ERR "%s()\n",__FUNCTION__);

    // common for all except HDMI_VIC_FORMAT
        // set AVI packet with VIC
        if (pVideo->pixelAspectRatio == HDMI_PIXEL_RATIO_16_9) {
                hdmi_reg_write(dev, HDMIVideoParams[pVideo->resolution].AVI_VIC_16_9,HDMI_AVI_BYTE4);
        } else {
                hdmi_reg_write(dev, HDMIVideoParams[pVideo->resolution].AVI_VIC,HDMI_AVI_BYTE4);
        }

        if (mode == HDMI_2D_VIDEO_FORMAT) {
                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_VSI_CON);

                // set pixel repetition
                reg = hdmi_reg_read(dev, HDMI_CON_1);
                if ( HDMIVideoParams[pVideo->resolution].repetition )
                {
                        // set pixel repetition
                        hdmi_reg_write(dev, reg|HDMICON1_DOUBLE_PIXEL_REPETITION,HDMI_CON_1);
                        // set avi packet
                        hdmi_reg_write(dev, AVI_PIXEL_REPETITION_DOUBLE,HDMI_AVI_BYTE5);
                }
                else
                {
                        // clear pixel repetition
                        hdmi_reg_write(dev, reg & ~(1<<1|1<<0),HDMI_CON_1);
                        // set avi packet
                        hdmi_reg_write(dev, 0x00,HDMI_AVI_BYTE5);
                }

                ret = 0;
        }
        else
        {
                // common for all 3D mode
                hdmi_reg_write(dev, 0x81,HDMI_VSI_HEADER0);
                hdmi_reg_write(dev, 0x01,HDMI_VSI_HEADER1);
                hdmi_reg_write(dev, 0x05,HDMI_VSI_HEADER2);

                hdmi_reg_write(dev, 0x03,HDMI_VSI_DATA01);
                hdmi_reg_write(dev, 0x0C,HDMI_VSI_DATA02);
                hdmi_reg_write(dev, 0x00,HDMI_VSI_DATA03);

                switch (mode) {
                        case HDMI_3D_FP_FORMAT:
                                hdmi_reg_write(dev, 0x2a,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x00,HDMI_VSI_DATA05);
                                ret = 0;
                                break;
                        case HDMI_3D_SSH_FORMAT:
                                hdmi_reg_write(dev, 0x06,HDMI_VSI_HEADER2);
                                hdmi_reg_write(dev, 0x99,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x80,HDMI_VSI_DATA05);
                                hdmi_reg_write(dev, 0x10,HDMI_VSI_DATA06);
                                ret = 0;
                                break;
                        case HDMI_3D_TB_FORMAT:
                                hdmi_reg_write(dev, 0xCA,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x60,HDMI_VSI_DATA05);
                                ret = 0;
                                break;
                        case HDMI_3D_FA_FORMAT:
                                hdmi_reg_write(dev, 0x1A,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x10,HDMI_VSI_DATA05);
                                ret = 0;
                                break;
                        case HDMI_3D_LA_FORMAT:
                                hdmi_reg_write(dev, 0x0A,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x20,HDMI_VSI_DATA05);
                                ret = 0;
                                break;
                        case HDMI_3D_SSF_FORMAT:
                                hdmi_reg_write(dev, 0x09,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x03,HDMI_VSI_DATA05);
                                ret = 0;
                                break;
                        case HDMI_3D_LD_FORMAT:
                                hdmi_reg_write(dev, 0xEA,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA05);
                                ret = 0;
                                break;
                        case HDMI_3D_LDGFX_FORMAT:
                                hdmi_reg_write(dev, 0xE9,HDMI_VSI_DATA00);
                                hdmi_reg_write(dev, 0x40,HDMI_VSI_DATA04);
                                hdmi_reg_write(dev, 0x05,HDMI_VSI_DATA05);
                                ret = 0;
                                break;
                        case HDMI_VIC_FORMAT:
                                {
                                        unsigned int vic = HDMIVideoParams[pVideo->resolution].AVI_VIC;
                                        hdmi_reg_write(dev, 0x20, HDMI_VSI_DATA04);
                                        hdmi_reg_write(dev, vic, HDMI_VSI_DATA05);
                                        hdmi_reg_write(dev, 0x4A - vic, HDMI_VSI_DATA00);
                                        // clear VIC in AVI
                                        hdmi_reg_write(dev, 0x00,HDMI_AVI_BYTE4);
                                        ret = 0;
                                }
                                break;
                        default:
                                break;
                } // switch

                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
                        hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC,HDMI_VSI_CON);

                        // set pixel repetition
                        reg = hdmi_reg_read(dev, HDMI_CON_1);
                        if ( HDMIVideoParams[pVideo->resolution].repetition ) {
                                // set pixel repetition
                                hdmi_reg_write(dev, reg|HDMICON1_DOUBLE_PIXEL_REPETITION,HDMI_CON_1);
                                // set avi packet
                                hdmi_reg_write(dev, AVI_PIXEL_REPETITION_DOUBLE,HDMI_AVI_BYTE5);
                        } else {
                                // clear pixel repetition
                                hdmi_reg_write(dev, reg & ~(1<<1|1<<0),HDMI_CON_1);
                                // set avi packet
                                hdmi_reg_write(dev, 0x00,HDMI_AVI_BYTE5);
                        }
                }
        }
        return ret;
}

/**
 * Set video registers as 2D video structure
 *
 * @param  format       [in] Video format
 * @return  1
 */
static int hdmi_set_2D_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        // basic video parametres
        unsigned int temp;
        if(dev != NULL) {
                // HBlank
                hdmi_reg_write(dev, HDMIVideoParams[format].HBlank & 0xFF, HDMI_H_BLANK_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].HBlank>>8) & 0xFF, HDMI_H_BLANK_1 );
                // V1 Blank
                hdmi_reg_write(dev, HDMIVideoParams[format].VBlank & 0xFF, HDMI_V1_BLANK_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].VBlank>>8) & 0xFF, HDMI_V1_BLANK_1 );

                // HTotal
                hdmi_reg_write(dev, HDMIVideoParams[format].HTotal & 0xFF, HDMI_H_LINE_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].HTotal>>8) & 0xFF, HDMI_H_LINE_1 );

                // VTotal
                hdmi_reg_write(dev, HDMIVideoParams[format].VTotal & 0xFF, HDMI_V_LINE_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].VTotal>>8) & 0xFF, HDMI_V_LINE_1 );

                // H POL
                hdmi_reg_write(dev, HDMIVideoParams[format].HPol, HDMI_HSYNC_POL );

                // V POL
                hdmi_reg_write(dev, HDMIVideoParams[format].VPol, HDMI_VSYNC_POL );

                // HSYNC Front
                hdmi_reg_write(dev, (HDMIVideoParams[format].HFront-2) & 0xFF, HDMI_H_SYNC_START_0 );
                hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2)>>8) & 0xFF, HDMI_H_SYNC_START_1 );

                // HSYNC End
                hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync) & 0xFF
                                                                        , HDMI_H_SYNC_END_0 );
                hdmi_reg_write(dev, (((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync)>>8) & 0xFF
                                                                        , HDMI_H_SYNC_END_1 );


                // VSYNC Front
                hdmi_reg_write(dev, HDMIVideoParams[format].VFront & 0xFF, HDMI_V_SYNC_LINE_BEF_1_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].VFront>>8) & 0xFF, HDMI_V_SYNC_LINE_BEF_1_1 );

                // VSYNC End
                hdmi_reg_write(dev, (HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_0 );
                hdmi_reg_write(dev, ((HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync)>>8) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_1 );


                if ( HDMIVideoParams[format].interlaced )
                {
                        // for interlace
                        hdmi_reg_write(dev, 0x1, HDMI_INT_PRO_MODE );

                        if ( format == v1920x1080i_50Hz_1250 ) // V TOP and V BOT are same
                        {
                                // V2 BLANK
                                temp = HDMIVideoParams[format].VTotal/2;
                                // V TOP and V BOT are same
                                hdmi_reg_write(dev, temp & 0xFF, HDMI_V2_BLANK_0 );
                                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V2_BLANK_1 );

                                // VBLANK_F0
                                hdmi_reg_write(dev, (temp + HDMIVideoParams[format].VBlank)&0xFF, HDMI_V_BLANK_F0_0 );
                                hdmi_reg_write(dev, ((temp + HDMIVideoParams[format].VBlank)>>8)&0xFF, HDMI_V_BLANK_F0_1 );

                                // VSYNC_LINE_AFT1
                                temp = temp + HDMIVideoParams[format].VFront - 1;
                                hdmi_reg_write(dev, temp & 0xFF , HDMI_V_SYNC_LINE_AFT_1_0);
                                hdmi_reg_write(dev, (temp>>8 & 0xFF), HDMI_V_SYNC_LINE_AFT_1_1);

                                // VSYNC_LINE_AFT2
                                temp = temp + HDMIVideoParams[format].VSync;
                                hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_2_0);
                                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_2_1);
                        }
                        else // V TOP and V BOT are not same
                        {
                                // V2 BLANK
                                temp = (HDMIVideoParams[format].VTotal - HDMIVideoParams[format].VBlank*2 - 1)/2 
                                                + HDMIVideoParams[format].VBlank;
                                hdmi_reg_write(dev, temp&0xFF, HDMI_V2_BLANK_0 );
                                hdmi_reg_write(dev, (temp>>8)&0xFF, HDMI_V2_BLANK_1 );

                                // VBLANK_F0
                                hdmi_reg_write(dev, ((HDMIVideoParams[format].VTotal + HDMIVideoParams[format].VBlank*2 + 1)/2) 
                                                                        & 0xFF, HDMI_V_BLANK_F0_0);
                                hdmi_reg_write(dev, (((HDMIVideoParams[format].VTotal + HDMIVideoParams[format].VBlank*2 + 1)/2)>>8) 
                                                                        & 0xFF, HDMI_V_BLANK_F0_1);

                                // VSYNC_LINE_AFT1
                                temp = temp + HDMIVideoParams[format].VFront;
                                hdmi_reg_write(dev, temp & 0xFF , HDMI_V_SYNC_LINE_AFT_1_0);
                                hdmi_reg_write(dev, (temp>>8 & 0xFF), HDMI_V_SYNC_LINE_AFT_1_1);

                                // VSYNC_LINE_AFT2
                                temp = temp + HDMIVideoParams[format].VSync;
                                hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_2_0);
                                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_2_1);
                        }



                        // VBLANK_F1
                        hdmi_reg_write(dev, HDMIVideoParams[format].VTotal & 0xFF, HDMI_V_BLANK_F1_0);
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VTotal>>8) & 0xFF, HDMI_V_BLANK_F1_1);

                        temp = HDMIVideoParams[format].HTotal/2 + HDMIVideoParams[format].HFront;

                        // VSYNC_LINE_AFT_PXL_1
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_1 );
                        // VSYNC_LINE_AFT_PXL_2
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_2_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_2_1 );
                }
                else
                {
                        // for progressive
                        hdmi_reg_write(dev, 0x0, HDMI_INT_PRO_MODE );

                        // V2 BLANK, same as V total
                        hdmi_reg_write(dev, HDMIVideoParams[format].VTotal & 0xFF, HDMI_V2_BLANK_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VTotal>>8) & 0xFF, HDMI_V2_BLANK_1 );
                }
        }
        
        return 0;
}

/**
 * Set video registers as 3D Frame-Packing video structure
 *
 * @param  format       [in] Video format
 * @return  1
 */
int hdmi_set_3D_FP_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        // basic video parametres
        unsigned int temp;

        dprintk("%s()\n",__FUNCTION__);

        if(dev != NULL) {
                // HBlank
                hdmi_reg_write(dev, HDMIVideoParams[format].HBlank & 0xFF, HDMI_H_BLANK_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].HBlank>>8) & 0xFF, HDMI_H_BLANK_1 );

                // V1 Blank
                hdmi_reg_write(dev, HDMIVideoParams[format].VBlank & 0xFF, HDMI_V1_BLANK_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].VBlank>>8) & 0xFF, HDMI_V1_BLANK_1 );

                // HTotal
                hdmi_reg_write(dev, HDMIVideoParams[format].HTotal & 0xFF, HDMI_H_LINE_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].HTotal>>8) & 0xFF, HDMI_H_LINE_1 );

                temp = HDMIVideoParams[format].VTotal*2;

                // VTotal
                hdmi_reg_write(dev, temp & 0xFF, HDMI_V_LINE_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_LINE_1 );

                // V2 BLANK
                hdmi_reg_write(dev, temp & 0xFF, HDMI_V2_BLANK_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V2_BLANK_1 );

                // H POL
                hdmi_reg_write(dev, HDMIVideoParams[format].HPol, HDMI_HSYNC_POL );

                // V POL
                hdmi_reg_write(dev, HDMIVideoParams[format].VPol, HDMI_VSYNC_POL );

                // HSYNC Front
                hdmi_reg_write(dev, (HDMIVideoParams[format].HFront-2) & 0xFF, HDMI_H_SYNC_START_0 );
                hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2)>>8) & 0xFF, HDMI_H_SYNC_START_1 );

                // HSYNC End
                hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync) & 0xFF
                                                                        , HDMI_H_SYNC_END_0 );
                hdmi_reg_write(dev, (((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync)>>8) & 0xFF
                                                                        , HDMI_H_SYNC_END_1 );

                // VSYNC Front
                hdmi_reg_write(dev, HDMIVideoParams[format].VFront & 0xFF, HDMI_V_SYNC_LINE_BEF_1_0 );
                hdmi_reg_write(dev, (HDMIVideoParams[format].VFront>>8) & 0xFF, HDMI_V_SYNC_LINE_BEF_1_1 );

                // VSYNC End
                hdmi_reg_write(dev, (HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_0 );
                hdmi_reg_write(dev, ((HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync)>>8) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_1 );

                if ( HDMIVideoParams[format].interlaced )
                {
                        // for interlace
                        hdmi_reg_write(dev, 0x1, HDMI_INT_PRO_MODE );

                        if ( format == v1920x1080i_50Hz_1250 ) // V TOP and V BOT are same
                        {
                                temp = HDMIVideoParams[format].VTotal/2;
                                // VACT_SPACE1
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE1_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE1_1);

                                // VACT_SPACE2
                                temp += HDMIVideoParams[format].VBlank;
                                hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE2_0);
                                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE2_1);

                                // VACT_SPACE5
                                temp = (HDMIVideoParams[format].VTotal*3)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE5_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE5_1);

                                // VACT_SPACE6
                                temp = (HDMIVideoParams[format].VTotal*3 + HDMIVideoParams[format].VBlank*2)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE6_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE6_1);

                        }
                        else // V TOP and V BOT are not same
                        {
                                temp = (HDMIVideoParams[format].VTotal - HDMIVideoParams[format].VBlank*2 - 1)/2 
                                                + HDMIVideoParams[format].VBlank;

                                // VACT_SPACE1
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE1_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE1_1);

                                // VACT_SPACE2
                                temp = (HDMIVideoParams[format].VTotal + HDMIVideoParams[format].VBlank*2 + 1)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE2_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE2_1);

                                // VACT_SPACE5
                                temp = (HDMIVideoParams[format].VTotal*3 - 1)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE5_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE5_1);

                                // VACT_SPACE6
                                temp = (HDMIVideoParams[format].VTotal*3 + HDMIVideoParams[format].VBlank*2 + 1)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE6_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE6_1);
                        }

                        // VACT_SPACE3
                        temp = HDMIVideoParams[format].VTotal;
                        hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE3_0);
                        hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE3_1);

                        // VACT_SPACE4
                        temp = HDMIVideoParams[format].VTotal + HDMIVideoParams[format].VBlank;
                        hdmi_reg_write(dev, (temp) & 0xFF, HDMI_VACT_SPACE4_0);
                        hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_VACT_SPACE4_1);
                }
                else
                {
                        // for progressive
                        hdmi_reg_write(dev, 0x0, HDMI_INT_PRO_MODE );

                        temp = HDMIVideoParams[format].VTotal;
                        // VACT_SPACE1
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE1_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE1_1);

                        // VACT_SPACE2
                        temp += HDMIVideoParams[format].VBlank;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE2_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE2_1);
                }
        }
        return 0;
}

/**
 * Set video registers as 3D Side-by-Side video structure
 *
 * @param  format       [in] Video format
 * @return 1
 */
int hdmi_set_3D_SSH_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        return hdmi_set_2D_video(dev, format);
}

/**
 * Set video registers as 3D Top-And-Bottom video structure
 *
 * @param  format       [in] Video format
 * @return  1
 */
int hdmi_set_3D_TB_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        return hdmi_set_2D_video(dev, format);
}

/**
 * Set video registers as 3D Field Alternative video structure
 *
 * @param  format       [in] Video format
 * @return  If format is progressive, return 0; Otherwise return 1.
 */
int hdmi_set_3D_FA_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        int ret = -1;
        // basic video parametres
        unsigned int temp;

        // only for interlaced
        if ( HDMIVideoParams[format].interlaced )
        {
                if(dev != NULL) {
                        // for interlaced
                        hdmi_reg_write(dev, 0x1, HDMI_INT_PRO_MODE );

                        // HBlank
                        hdmi_reg_write(dev, HDMIVideoParams[format].HBlank & 0xFF, HDMI_H_BLANK_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HBlank>>8) & 0xFF, HDMI_H_BLANK_1 );

                        // V1 Blank
                        hdmi_reg_write(dev, HDMIVideoParams[format].VBlank & 0xFF, HDMI_V1_BLANK_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VBlank>>8) & 0xFF, HDMI_V1_BLANK_1 );

                        // HTotal
                        hdmi_reg_write(dev, HDMIVideoParams[format].HTotal & 0xFF, HDMI_H_LINE_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HTotal>>8) & 0xFF, HDMI_H_LINE_1 );

                        temp = HDMIVideoParams[format].VTotal*2;

                        // VTotal
                        hdmi_reg_write(dev, HDMIVideoParams[format].VTotal & 0xFF, HDMI_V_LINE_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VTotal>>8) & 0xFF, HDMI_V_LINE_1 );

                        // H POL
                        hdmi_reg_write(dev, HDMIVideoParams[format].HPol, HDMI_HSYNC_POL );

                        // V POL
                        hdmi_reg_write(dev, HDMIVideoParams[format].VPol, HDMI_VSYNC_POL );

                        // HSYNC Front
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HFront-2) & 0xFF, HDMI_H_SYNC_START_0 );
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2)>>8) & 0xFF, HDMI_H_SYNC_START_1 );

                        // HSYNC End
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync) & 0xFF
                                                                        , HDMI_H_SYNC_END_0 );
                        hdmi_reg_write(dev, (((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync)>>8) & 0xFF
                                                                        , HDMI_H_SYNC_END_1 );

                        // VSYNC Front
                        hdmi_reg_write(dev, HDMIVideoParams[format].VFront & 0xFF, HDMI_V_SYNC_LINE_BEF_1_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VFront>>8) & 0xFF, HDMI_V_SYNC_LINE_BEF_1_1 );

                        // VSYNC End
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_0 );
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync)>>8) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_1 );

                        if ( format == v1920x1080i_50Hz_1250 ) // V TOP and V BOT are same
                        {
                                temp = HDMIVideoParams[format].VTotal/2;
                                //V BLANK2
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V2_BLANK_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V2_BLANK_1);

                                // VBLANK_F0
                                temp += HDMIVideoParams[format].VBlank;
                                hdmi_reg_write(dev, temp & 0xFF, HDMI_V_BLANK_F0_0);
                                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_BLANK_F0_1);

                                // VBLANK_F3 == VACT_SPACE5
                                temp = HDMIVideoParams[format].VTotal - HDMIVideoParams[format].VBlank/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F3_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F3_1);

                                // VBLANK_F4 = VACT_SPACE6
                                temp = (HDMIVideoParams[format].VTotal*3 + HDMIVideoParams[format].VBlank*2)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F4_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F4_1);

                                // VSYNC_LINE_AFT1
                                temp += HDMIVideoParams[format].VFront-1;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_SYNC_LINE_AFT_1_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_SYNC_LINE_AFT_1_1);

                                // VSYNC_LINE_AFT2
                                temp += HDMIVideoParams[format].VSync;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_SYNC_LINE_AFT_2_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_SYNC_LINE_AFT_2_1);

                        }
                        else // V TOP and V BOT are not same
                        {
                                temp = (HDMIVideoParams[format].VTotal - HDMIVideoParams[format].VBlank*2 - 1)/2 
                                        + HDMIVideoParams[format].VBlank;

                                //V BLANK2
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V2_BLANK_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V2_BLANK_1);

                                // VBLANK_F0
                                temp = (HDMIVideoParams[format].VTotal + HDMIVideoParams[format].VBlank*2 + 1)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F0_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F0_1);

                                // VBLANK_F3 == VACT5
                                temp = (HDMIVideoParams[format].VTotal - (HDMIVideoParams[format].VBlank+1))/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F3_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F3_1);

                                // VBLANK_F4 = VACT_SPACE6
                                temp = (HDMIVideoParams[format].VTotal*3 + HDMIVideoParams[format].VBlank*2 + 1)/2;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F4_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F4_1);

                                // VSYNC_LINE_AFT1
                                temp += HDMIVideoParams[format].VFront;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_SYNC_LINE_AFT_1_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_SYNC_LINE_AFT_1_1);

                                // VSYNC_LINE_AFT2
                                temp += HDMIVideoParams[format].VSync;
                                hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_SYNC_LINE_AFT_2_0);
                                hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_SYNC_LINE_AFT_2_1);
                        }

                        // VBLANK_F1
                        temp = HDMIVideoParams[format].VTotal;
                        hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F1_0);
                        hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F1_1);


                        temp = HDMIVideoParams[format].HTotal/2 + HDMIVideoParams[format].HFront;
                        // VSYNC_LINE_AFT_PXL_1
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_1 );
                        // VSYNC_LINE_AFT_PXL_2
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_1 );

                        // VBLANK_F2 == VACT4
                        temp = HDMIVideoParams[format].VTotal + HDMIVideoParams[format].VBlank;
                        hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F2_0);
                        hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F2_1);

                        // VBLANK_F5
                        temp = HDMIVideoParams[format].VTotal*2;
                        hdmi_reg_write(dev, (temp) & 0xFF, HDMI_V_BLANK_F5_0);
                        hdmi_reg_write(dev, (temp>>8)  & 0xFF, HDMI_V_BLANK_F5_1);
                        ret = 0;
                }
        }
        else // progressive mode
        {
                // not available
        }

        return ret;
}

/**
 * Set video registers as 3D Line Alternative video structure
 *
 * @param  format       [in] Video format
 * @return  If format is interlaced, return 0; Otherwise return 1.
 */
int hdmi_set_3D_LA_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        int ret = -1;
        
        // only for progressive
        if ( HDMIVideoParams[format].interlaced )
        {
                // interlaced mode
        }
        else // progressive mode
        {
                unsigned int temp;

                if(dev != NULL) {
                        // HBlank
                        temp = HDMIVideoParams[format].HBlank;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_H_BLANK_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_H_BLANK_1 );

                        // V1Blank
                        temp = (unsigned int)HDMIVideoParams[format].VBlank*2;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V1_BLANK_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V1_BLANK_1 );

                        // V2Blank
                        temp = HDMIVideoParams[format].VTotal*2;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V2_BLANK_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V2_BLANK_1 );

                        // VTotal
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V_LINE_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_LINE_1 );

                        // Htotal
                        hdmi_reg_write(dev, HDMIVideoParams[format].HTotal & 0xFF, HDMI_H_LINE_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HTotal>>8) & 0xFF, HDMI_H_LINE_1 );

                        // H POL
                        hdmi_reg_write(dev, HDMIVideoParams[format].HPol, HDMI_HSYNC_POL );

                        // V POL
                        hdmi_reg_write(dev, HDMIVideoParams[format].VPol, HDMI_VSYNC_POL );

                        // HSYNC Front
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HFront-2) & 0xFF, HDMI_H_SYNC_START_0 );
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2)>>8) & 0xFF, HDMI_H_SYNC_START_1 );

                        // HSYNC End
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync) & 0xFF
                                                                        , HDMI_H_SYNC_END_0 );
                        hdmi_reg_write(dev, (((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync)>>8) & 0xFF
                                                                        , HDMI_H_SYNC_END_1 );

                        // VSYNC Front
                        temp = HDMIVideoParams[format].VFront*2;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_BEF_1_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_BEF_1_1 );

                        // VSYNC End
                        temp += HDMIVideoParams[format].VSync*2;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_BEF_2_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_BEF_2_1 );
                        ret = 0;
                }
        }

        return ret;
}

/**
 * Set video registers as Side-by-Side Full video structure
 *
 * @param  format       [in] Video format
 * @return  1
 */
int hdmi_set_3D_SSF_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        unsigned int temp;

        if(dev != NULL) {
                // same with 2D but H is twice longer than 2D
                hdmi_set_2D_video(dev, format);

                // H
                // HBlank
                temp = HDMIVideoParams[format].HBlank*2;
                hdmi_reg_write(dev, temp & 0xFF, HDMI_H_BLANK_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_H_BLANK_1 );

                // Htotal
                temp = HDMIVideoParams[format].HTotal*2;
                hdmi_reg_write(dev, temp & 0xFF, HDMI_H_LINE_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_H_LINE_1 );

                // HSync Start
                temp = HDMIVideoParams[format].HFront*2-2;
                hdmi_reg_write(dev, temp & 0xFF, HDMI_H_SYNC_START_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_H_SYNC_START_1 );

                // HSYNC End
                temp += HDMIVideoParams[format].HSync*2;
                hdmi_reg_write(dev, temp & 0xFF, HDMI_H_SYNC_END_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_H_SYNC_END_1 );

                temp = (HDMIVideoParams[format].HTotal/2 + HDMIVideoParams[format].HFront)*2;
                // VSYNC_LINE_AFT_PXL_1
                hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_1 );

                // VSYNC_LINE_AFT_PXL_2
                hdmi_reg_write(dev, temp & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_0 );
                hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V_SYNC_LINE_AFT_PXL_1_1 );
        }
        return 0;
}

/**
 * Set video registers as L + Depth video structure
 *
 * @param  format       [in] Video format
 * @return  If format is interlaced, return 0; Otherwise return 1.
 */
int hdmi_set_3D_LD_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        int ret = -1;
        // same with 3D FP but only for prog
        // only for progressive
        if ( HDMIVideoParams[format].interlaced ) {
                // interlaced mode
        } else  {
                // progressive mode        
                if(dev != NULL) {
                        ret =  hdmi_set_3D_FP_video(dev, format);
                }
        }

        return ret;
}


/**
 * Set video registers as L + Depth + Graphics + Graphics-Depth video structure
 *
 * @param  format       [in] Video format
 * @return  If format is interlaced, return 0; Otherwise return 1.
 */
int hdmi_set_3D_LDGFX_video(struct tcc_hdmi_dev *dev, enum VideoFormat format)
{
        int ret = -1;
        // similar to 3D LD but V is twice longer than LD

        // basic video parametres
        unsigned int temp;

        if ( HDMIVideoParams[format].interlaced ) {
        } else {
                if(dev != NULL) {
                        // HBlank
                        hdmi_reg_write(dev, HDMIVideoParams[format].HBlank & 0xFF, HDMI_H_BLANK_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HBlank>>8) & 0xFF, HDMI_H_BLANK_1 );

                        // V1 Blank
                        hdmi_reg_write(dev, HDMIVideoParams[format].VBlank & 0xFF, HDMI_V1_BLANK_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VBlank>>8) & 0xFF, HDMI_V1_BLANK_1 );

                        // HTotal
                        hdmi_reg_write(dev, HDMIVideoParams[format].HTotal & 0xFF, HDMI_H_LINE_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HTotal>>8) & 0xFF, HDMI_H_LINE_1 );

                        temp = HDMIVideoParams[format].VTotal*4;

                        // VTotal
                        hdmi_reg_write(dev, HDMIVideoParams[format].VTotal & 0xFF, HDMI_V_LINE_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VTotal>>8) & 0xFF, HDMI_V_LINE_1 );

                        // V2 BLANK
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_V2_BLANK_0 );
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_V2_BLANK_1 );

                        // H POL
                        hdmi_reg_write(dev, HDMIVideoParams[format].HPol, HDMI_HSYNC_POL );

                        // V POL
                        hdmi_reg_write(dev, HDMIVideoParams[format].VPol, HDMI_VSYNC_POL );

                        // HSYNC Front
                        hdmi_reg_write(dev, (HDMIVideoParams[format].HFront-2) & 0xFF, HDMI_H_SYNC_START_0 );
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2)>>8) & 0xFF, HDMI_H_SYNC_START_1 );

                        // HSYNC End
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync) & 0xFF
                                                                        , HDMI_H_SYNC_END_0 );
                        hdmi_reg_write(dev, (((HDMIVideoParams[format].HFront-2) + HDMIVideoParams[format].HSync)>>8) & 0xFF
                                                                        , HDMI_H_SYNC_END_1 );

                        // VSYNC Front
                        hdmi_reg_write(dev, HDMIVideoParams[format].VFront & 0xFF, HDMI_V_SYNC_LINE_BEF_1_0 );
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VFront>>8) & 0xFF, HDMI_V_SYNC_LINE_BEF_1_1 );

                        // VSYNC End
                        hdmi_reg_write(dev, (HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_0 );
                        hdmi_reg_write(dev, ((HDMIVideoParams[format].VFront + HDMIVideoParams[format].VSync)>>8) & 0xFF
                                                                        , HDMI_V_SYNC_LINE_BEF_2_1 );

                        // for progressive
                        hdmi_reg_write(dev, 0x0, HDMI_INT_PRO_MODE );

                        temp = HDMIVideoParams[format].VTotal;
                        // VACT_SPACE1
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE1_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE1_1);

                        // VACT_SPACE2
                        temp += HDMIVideoParams[format].VBlank;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE2_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE2_1);

                        temp = HDMIVideoParams[format].VTotal*2;
                        // VACT_SPACE3
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE3_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE3_1);

                        // VACT_SPACE4
                        temp += HDMIVideoParams[format].VBlank;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE4_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE4_1);

                        temp = HDMIVideoParams[format].VTotal*3;
                        // VACT_SPACE5
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE5_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE5_1);

                        // VACT_SPACE6
                        temp += HDMIVideoParams[format].VBlank;
                        hdmi_reg_write(dev, temp & 0xFF, HDMI_VACT_SPACE6_0);
                        hdmi_reg_write(dev, (temp>>8) & 0xFF, HDMI_VACT_SPACE6_1);
                        ret = 0;
                }
        }

        return ret;
}


/**
 * Initialize HDMI video registers. @n
 */
static void hdmi_set_default_value(struct tcc_hdmi_dev *dev)
{
        unsigned int offset;
        
        if(dev != NULL) {
                // HBLANK
                hdmi_reg_write(dev, 0x00, HDMI_H_BLANK_0);
                hdmi_reg_write(dev, 0x00, HDMI_H_BLANK_1);

                // VBLANK1,2
                hdmi_reg_write(dev, 0x00, HDMI_V1_BLANK_0);
                hdmi_reg_write(dev, 0x00, HDMI_V1_BLANK_1);

                hdmi_reg_write(dev, 0x00, HDMI_V2_BLANK_0);
                hdmi_reg_write(dev, 0x00, HDMI_V2_BLANK_1);

                // H_LINE
                hdmi_reg_write(dev, 0x00, HDMI_H_LINE_0);
                hdmi_reg_write(dev, 0x00, HDMI_H_LINE_1);

                // V_LINE
                hdmi_reg_write(dev, 0x00, HDMI_V_LINE_0);
                hdmi_reg_write(dev, 0x00, HDMI_V_LINE_1);
                
                // set default value(0xff) from HDMI_V_BLANK_F0_0 to HDMI_VACT_SPACE6_1
                for ( offset = HDMI_V_BLANK_F0_0; offset <= HDMI_VACT_SPACE6_1; offset+=4) {
                        hdmi_reg_write(dev, 0xFF, offset);
                }
        }
}


/**
 * Set video timing parameters.@n
 * @param   pVideo   [in] pointer to Video timing parameters
 * @return  If argument is invalid, return 0; Otherwise return 1.
 */
static int hdmi_set_video_mode(struct tcc_hdmi_dev *dev, struct HDMIVideoParameter* pVideo)
{
        int ret = -1;
	dprintk("%s : pVideo->hdmi_3d_format = %d, pVideo->resolution = %d\n", __func__, pVideo->hdmi_3d_format, pVideo->resolution);

        if(dev != NULL) {
        	// set default values first
        	hdmi_set_default_value(dev);

        	// set video registers
        	switch (pVideo->hdmi_3d_format)
        	{
        		case HDMI_2D_VIDEO_FORMAT:
        		{
        			if (hdmi_set_2D_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_FP_FORMAT:
        		{
        			if (hdmi_set_3D_FP_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_SSH_FORMAT:
        		{
        			if (hdmi_set_3D_SSH_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_TB_FORMAT:
        		{
        			if (hdmi_set_3D_TB_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_FA_FORMAT:
        		{
        			if (hdmi_set_3D_FA_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_LA_FORMAT:
        		{
        			if (hdmi_set_3D_LA_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_SSF_FORMAT:
        		{
        			if (hdmi_set_3D_SSF_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_LD_FORMAT:
        		{
        			if (hdmi_set_3D_LD_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		case HDMI_3D_LDGFX_FORMAT:
        		{
        			if (hdmi_set_3D_LDGFX_video(dev, pVideo->resolution) < 0)
        				return 0;
        			break;
        		}
        		default:
        			return 0;
        	}
                
                // Set Video Auxilary packet
                ret = hdmi_set_aux_data(dev, pVideo);
        }
	return ret;
}

/**
 * Set pixel limitation.
 * @param   limit   [in] Pixel limitation.
* @return  If argument is invalid, return 0;Otherwise return 1.
 */
static int hdmi_set_pixel_limit(struct tcc_hdmi_dev *dev, enum PixelLimit limit)
{
	int ret = 1;
	unsigned int reg, aviQQ;

        if(dev != NULL) {
        	// clear field
        	reg = hdmi_reg_read(dev, HDMI_CON_1);
        	reg &= ~HDMICON1_LIMIT_MASK;

        	aviQQ = hdmi_reg_read(dev, HDMI_AVI_BYTE3);
        	aviQQ &= ~AVI_QUANTIZATION_MASK;

        	switch (limit) // full
        	{
        		case HDMI_FULL_RANGE:
        			aviQQ |= AVI_QUANTIZATION_FULL;
                                ret = 0;
        			break;
        		case HDMI_RGB_LIMIT_RANGE:
        			reg |= HDMICON1_RGB_LIMIT;
        			aviQQ |= AVI_QUANTIZATION_LIMITED;
                                ret = 0;
        			break;
        		case HDMI_YCBCR_LIMIT_RANGE:
        			reg |= HDMICON1_YCBCR_LIMIT;
        			aviQQ |= AVI_QUANTIZATION_LIMITED;
                                ret = 0;
        			break;
        		default:
                                pr_err("%s invlaid param line(%d)\r\n", __func__, __LINE__);
        			break;
        	}
                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
                	// set pixel repetition
                	hdmi_reg_write(dev, reg,HDMI_CON_1);
                        
                	// set avi packet body
                	hdmi_reg_write(dev, aviQQ,HDMI_AVI_BYTE3);
                }
        }

	return ret;
}

/**
 * Set pixel aspect ratio information in AVI InfoFrame
 * @param   ratio   [in] Pixel Aspect Ratio
 * @return  If argument is invalid, return 0;Otherwise return 1.
 */
static int hdmi_set_pixel_aspect_ratio(struct tcc_hdmi_dev *dev, enum PixelAspectRatio ratio)
{
	int ret = -1;
	unsigned int reg = AVI_FORMAT_ASPECT_AS_PICTURE;

        if(dev != NULL) {
        	switch (ratio)
        	{
        		case HDMI_PIXEL_RATIO_AS_PICTURE:
                                ret = 0;
        			break;
        		case HDMI_PIXEL_RATIO_16_9:
        			reg |= AVI_PICTURE_ASPECT_RATIO_16_9;
                                ret = 0;
        			break;
        		case HDMI_PIXEL_RATIO_4_3:
        			reg |= AVI_PICTURE_ASPECT_RATIO_4_3;
                                ret = 0;
        			break;
        		default:
        			break;
        	}
                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
	                hdmi_reg_write(dev, reg,HDMI_AVI_BYTE2);
                }
        }
	return ret;
}

/**
 * Set colorimetry information in AVI InfoFrame
 * @param   colorimetry   [in] colorimetry
 * @return  If argument is invalid, return 0; Otherwise return 1.
 */
static int hdmi_set_colorimetry(struct tcc_hdmi_dev *dev, enum HDMIColorimetry colorimetry)
{
	int ret = -1;
	unsigned int avi2,avi3;

        if(dev != NULL) {
        	avi2 = hdmi_reg_read(dev, HDMI_AVI_BYTE2);
        	avi3 = hdmi_reg_read(dev, HDMI_AVI_BYTE3);

        	avi2 &= ~AVI_COLORIMETRY_MASK;
        	avi3 &= ~AVI_COLORIMETRY_EXT_MASK;

        	switch (colorimetry)
        	{
        		case HDMI_COLORIMETRY_NO_DATA:
                                ret = 0;
        			break;

        		case HDMI_COLORIMETRY_ITU601:
        			avi2 |= AVI_COLORIMETRY_ITU601;
                                ret = 0;
        			break;

        		case HDMI_COLORIMETRY_ITU709:
        			avi2 |= AVI_COLORIMETRY_ITU709;
                                ret = 0;
        			break;

        		case HDMI_COLORIMETRY_EXTENDED_xvYCC601:
        			avi2 |= AVI_COLORIMETRY_EXTENDED;
        			avi3 |= AVI_COLORIMETRY_EXT_xvYCC601;
                                ret = 0;
        			break;

        		case HDMI_COLORIMETRY_EXTENDED_xvYCC709:
        			avi2 |= AVI_COLORIMETRY_EXTENDED;
        			avi3 |= AVI_COLORIMETRY_EXT_xvYCC709;
                                ret = 0;
        			break;

        		default:
        			break;
        	}

                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
        	        hdmi_reg_write(dev, avi2,HDMI_AVI_BYTE2);
        	        hdmi_reg_write(dev, avi3,HDMI_AVI_BYTE3);
                }
        }
	return ret;
}


/**
 * Set HDMI/DVI mode
 * @param   mode   [in] HDMI/DVI mode
 * @return  If argument is invalid, return 0;Otherwise return 1.
 */
static int hdmi_set_hdmimode(struct tcc_hdmi_dev *dev, int mode)
{
	int ret = -1;

        if(dev != NULL) {
        	switch(mode){
        		case HDMI:
                	        hdmi_reg_write(dev, HDMI_MODE_SEL_HDMI,HDMI_MODE_SEL);
                	        hdmi_reg_write(dev, HDMICON2_HDMI,HDMI_CON_2);
                                ret = 0;
        			break;
        		case DVI:
                	        hdmi_reg_write(dev, HDMI_MODE_SEL_DVI,HDMI_MODE_SEL);
                	        hdmi_reg_write(dev, HDMICON2_DVI,HDMI_CON_2);
                                ret = 0;
        			break;
        		default:
        			break;
        	}
        }

	return ret;
}

/**
 * Set Audio Clock Recovery and Audio Infoframe packet -@n
 * based on sampling frequency.
 * @param   freq   [in] Sampling frequency
 * @return  If argument is invalid, return 0;Otherwise return 1.
 */
int hdmi_set_audio_sample_freq(struct tcc_hdmi_dev *dev, enum SamplingFreq freq)
{
        
        int ret = -1;
	unsigned int n, reg;

        do {
                if(dev == NULL) 
                        break;
                
        	// check param
        	if ( freq > sizeof(ACR_N_params)/sizeof(unsigned int) || freq < 0 )
        		break;
                

        	// set ACR packet
        	// set N value
        	n = ACR_N_params[freq];
        	reg = n & 0xff;
        	hdmi_reg_write(dev, 0,HDMI_ACR_N0);
        	hdmi_reg_write(dev, reg, HDMI_ACR_N0);
        	reg = (n>>8) & 0xff;
        	hdmi_reg_write(dev, reg, HDMI_ACR_N1);
        	reg = (n>>16) & 0xff;
        	hdmi_reg_write(dev, reg, HDMI_ACR_N2);

        	// set AUI packet
        	reg = hdmi_reg_read(dev, HDMI_AUI_BYTE2) & ~HDMI_AUI_SF_MASK;

        	switch (freq) {
        		case SF_32KHZ:
        			reg |= HDMI_AUI_SF_SF_32KHZ;
                                ret = 0;
        			break;

        		case SF_44KHZ:
        			reg |= HDMI_AUI_SF_SF_44KHZ;
                                ret = 0;
        			break;

        		case SF_88KHZ:
        			reg |= HDMI_AUI_SF_SF_88KHZ;
                                ret = 0;
        			break;

        		case SF_176KHZ:
        			reg |= HDMI_AUI_SF_SF_176KHZ;
                                ret = 0;
        			break;

        		case SF_48KHZ:
        			reg |= HDMI_AUI_SF_SF_48KHZ;
                                ret = 0;
        			break;

        		case SF_96KHZ:
        			reg |= HDMI_AUI_SF_SF_96KHZ;
                                ret = 0;
        			break;

        		case SF_192KHZ:
        			reg |= HDMI_AUI_SF_SF_192KHZ;
                                ret = 0;
        			break;

        		default:
        			break;
        	}
                
                if(ret < 0) {
                        break;
                }
        	hdmi_reg_write(dev, reg, HDMI_AUI_BYTE2);
        }while(0);

	return ret;
}

/**
 * Set HDMI audio output packet type.
 * @param   packet   [in] Audio packet type
 * @return  If argument is invalid, return 0;Otherwise return 1.
 */
static int hdmi_set_audio_packet_type(struct tcc_hdmi_dev *dev, enum HDMIASPType packet)
{
        int ret = -1;
        unsigned int reg, regb;

        if(dev != NULL) {
                reg = hdmi_reg_read(dev, HDMI_ASP_CON);
                reg &= ~ASP_TYPE_MASK;

                switch (packet){
                        case HDMI_ASP:
                                reg |= ASP_LPCM_TYPE;
                                ret = 0;
                                break;
                        case HDMI_DSD:
                                reg |= ASP_DSD_TYPE;
                                ret = 0;
                                break;
                        case HDMI_HBR:
                                regb = hdmi_reg_read(dev, HDMI_SS_I2S_CH_ST_3) & ~I2S_CH_ST_3_SF_MASK;
                                regb |= I2S_CH_ST_3_SF_768KHZ;
                                hdmi_reg_write(dev, regb, HDMI_SS_I2S_CH_ST_3);
                                reg |= ASP_HBR_TYPE;
                                ret = 0;
                                break;
                        case HDMI_DST:
                                reg |= ASP_DST_TYPE;
                                ret = 0;
                                break;
                        default:
                                break;
                }
                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
                        hdmi_reg_write(dev, reg,HDMI_ASP_CON);
                }
        }
        return ret;
}

/**
 * Set layout and sample present fields in Audio Sample Packet -@n
 * and channel number field in Audio InfoFrame packet.
 * @param   channel   [in]  Number of channels
 * @return  If argument is invalid, return 0;Otherwise return 1.
 */
static int hdmi_set_audio_number_of_channels(struct tcc_hdmi_dev *dev, enum ChannelNum channel)
{
	int ret = -1;
        unsigned int reg_asp_con, reg_byte1, reg_byte4;
        
        if(dev != NULL) {
        	reg_asp_con = hdmi_reg_read(dev, HDMI_ASP_CON);
        	// clear field
        	reg_asp_con &= ~(ASP_MODE_MASK|ASP_SP_MASK);

        	// clear field
        	hdmi_reg_write(dev, 0x00, HDMI_ASP_SP_FLAT);

        	// celar field
        	reg_byte4 = 0;

        	// set layout & SP_PRESENT on ASP_CON
        	// set AUI Packet
        	switch (channel)
        	{
        		case CH_2:
        			reg_asp_con |= (ASP_LAYOUT_0|ASP_SP_0);
                                reg_byte1 = AUI_CC_2CH;
                                ret = 0;
        			break;
        		case CH_3:
        			reg_asp_con |= (ASP_LAYOUT_1|ASP_SP_0|ASP_SP_1);
                                reg_byte1 = AUI_CC_3CH;
                                ret = 0;
        			break;
        		case CH_4:
        			reg_asp_con |= (ASP_LAYOUT_1|ASP_SP_0|ASP_SP_1);
                                reg_byte1 = AUI_CC_4CH;
                                ret = 0;
        			break;
        		case CH_5:
        			reg_byte4 = 0x0A;
        			reg_asp_con |= (ASP_LAYOUT_1|ASP_SP_0|ASP_SP_1|ASP_SP_2);
                                reg_byte1 = AUI_CC_5CH;
                                ret = 0;
        			break;
        		case CH_6:
        			reg_byte4 = 0x0A;
        			reg_asp_con |= (ASP_LAYOUT_1|ASP_SP_0|ASP_SP_1|ASP_SP_2);
                                reg_byte1 = AUI_CC_6CH;
                                ret = 0;
        			break;
        		case CH_7:
        			reg_byte4 = 0x12;
        			reg_asp_con |= (ASP_LAYOUT_1|ASP_SP_0|ASP_SP_1|ASP_SP_2|ASP_SP_3);
                                reg_byte1 = AUI_CC_7CH;
                                ret = 0;
        			break;
        		case CH_8:
        			reg_byte4 = 0x12;
        			reg_asp_con |= (ASP_LAYOUT_1|ASP_SP_0|ASP_SP_1|ASP_SP_2|ASP_SP_3);
                                reg_byte1 = AUI_CC_8CH;
                                ret = 0;
        			break;
        		default:
        			break;
        	}
                if(ret < 0) {       
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
                        hdmi_reg_write(dev, reg_byte1, HDMI_AUI_BYTE1);
        	        hdmi_reg_write(dev, reg_byte4, HDMI_AUI_BYTE4);
        	        hdmi_reg_write(dev, reg_asp_con, HDMI_ASP_CON);
                }
        }
        return ret;
}

static int hdmi_set_spd_infoframe(struct tcc_hdmi_dev *dev, struct HDMIVideoFormatCtrl VideoFormatCtrl)
{
        if(dev != NULL) {
        	//SPD INFOFRAME PACKET HEADER
        	hdmi_reg_write(dev, SPD_PACKET_TYPE,HDMI_SPD_HEADER0);
        	hdmi_reg_write(dev, SPD_PACKET_VERSION,HDMI_SPD_HEADER1);
        	hdmi_reg_write(dev, SPD_PACKET_BYTE_LENGTH,HDMI_SPD_HEADER2);

        	//SPD INFOFRAME PACKET CONTENTS
        	hdmi_reg_write(dev, SPD_PACKET_ID0,HDMI_SPD_DATA1);
        	hdmi_reg_write(dev, SPD_PACKET_ID1,HDMI_SPD_DATA2);
        	hdmi_reg_write(dev, SPD_PACKET_ID2,HDMI_SPD_DATA3);

        	switch(VideoFormatCtrl.video_format)
        	{
        		case HDMI_2D:
        			hdmi_reg_write(dev, (SPD_HDMI_VIDEO_FORMAT_NONE << 5),HDMI_SPD_DATA4);
        			break;
        		case HDMI_VIC:
        			hdmi_reg_write(dev, (SPD_HDMI_VIDEO_FORMAT_VIC << 5),HDMI_SPD_DATA4);
        			break;
        		case HDMI_3D:
        			hdmi_reg_write(dev, (SPD_HDMI_VIDEO_FORMAT_3D << 5),HDMI_SPD_DATA4);
        			break;
        		default:
        			break;
        	}

        	if(VideoFormatCtrl.video_format == HDMI_3D)
        	{
        		switch(VideoFormatCtrl.structure_3D)
        		{
        			case FRAME_PACKING:
        				hdmi_reg_write(dev, (SPD_3D_STRUCT_FRAME_PACKING << 4),HDMI_SPD_DATA5);
        				break;
        			case TOP_AND_BOTTOM:
        				hdmi_reg_write(dev, (SPD_3D_STRUCT_TOP_AND_BOTTOM << 4),HDMI_SPD_DATA5);
        				break;
        			case SIDE_BY_SIDE:
        				hdmi_reg_write(dev, (SPD_3D_STRUCT_SIDE_BY_SIDE << 4),HDMI_SPD_DATA5);
        				break;
        		}

        		if(VideoFormatCtrl.ext_data_3D)
        			hdmi_reg_write(dev, VideoFormatCtrl.ext_data_3D << 4,HDMI_SPD_DATA5);
        	}
        	else
        	{
        		hdmi_reg_write(dev, 0,HDMI_SPD_DATA5);
        		hdmi_reg_write(dev, 0,HDMI_SPD_DATA6);
        		hdmi_reg_write(dev, 0,HDMI_SPD_DATA7);
        	}
        	
        	hdmi_spd_update_checksum(dev);
        }

	return 0;
}

static int hdmi_get_power_status(struct tcc_hdmi_dev *dev)
{
        int power_status = 0;
        if(dev != NULL) {
                power_status = dev->power_status;
        }
        return power_status;
}

static int hdmi_get_VBlank_internal(struct tcc_hdmi_dev *dev)
{
        unsigned int VBlankValue = 0;
        if(dev != NULL) {
                VBlankValue = hdmi_reg_read(dev, HDMI_V1_BLANK_0) & 0xFF;
                VBlankValue |= ((hdmi_reg_read(dev, HDMI_V1_BLANK_1) & 0xFF) << 8);
        }

        return VBlankValue;
}

/**
 * Enable/disable Blue-Screen.
 *
 * @param  enable       [in] 0 to stop sending, 1 to start sending.
 */
static void hdmi_enable_bluescreen(struct tcc_hdmi_dev *dev, unsigned char enable)
{
        unsigned int reg;

        if(dev != NULL) {
                reg = hdmi_reg_read(dev, HDMI_CON_0);

                dprintk(KERN_INFO "%s enable = %d\n", __func__, enable);
                
                if (enable) {
                        reg |= HDMI_BLUE_SCR_ENABLE;
                } else {
                        reg &= ~HDMI_BLUE_SCR_ENABLE;
                }
                hdmi_reg_write(dev, reg,HDMI_CON_0);
        }
}

static void hdmi_start_internal(struct tcc_hdmi_dev *dev) 
{
        unsigned int reg, mode;

        if(dev != NULL) {
                if(!dev->hdmi_started) {
                        pr_info("%s \r\n", __func__);
                        
                        // check HDMI mode
                        mode = hdmi_reg_read(dev, HDMI_MODE_SEL) & HDMI_MODE_SEL_HDMI;
                        reg = hdmi_reg_read(dev, HDMI_CON_0);

                        // enable external vido gen.
                        hdmi_reg_write(dev, HDMI_EXTERNAL_VIDEO,HDMI_VIDEO_PATTERN_GEN);

                        if (mode) {
                                // HDMI
                                
                                // enable AVI packet: mandatory
                                // update avi packet checksum
                                hdmi_avi_update_checksum(dev);
                                // enable avi packet
                                hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC, HDMI_AVI_CON);

                                // check if audio is enable
                                if (hdmi_reg_read(dev, HDMI_ACR_CON))
                                {
                                        // enable aui packet
                                        hdmi_aui_update_checksum(dev);
                                        hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC, HDMI_AUI_CON);
                                        reg |= HDMI_ASP_ENABLE;
                                }

                                // check if it is deep color mode or not
                                if (hdmi_reg_read(dev, HDMI_DC_CONTROL))
                                {
                                        hdmi_reg_write(dev, GCP_TRANSMIT_EVERY_VSYNC, HDMI_GCP_CON);
                                }
                                else
                                {
                                        // disable GCP
                                        hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_GCP_CON);
                                }

                                // for checking
                                udelay(200);

                                // enable hdmi

                                //-------------------------------------------------------------------------------------------
                                // In TCC897x HDMI Link, Encoding option is must disabled.
                                //-------------------------------------------------------------------------------------------
                                hdmi_reg_write(dev, reg|HDMI_SYS_ENABLE/*|HDMI_ENCODING_OPTION_ENABLE*/, HDMI_CON_0);
                                //-------------------------------------------------------------------------------------------
                        }
                        else // DVI
                        {
                                // disable all packet
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_AVI_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_AUI_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_GCP_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_GAMUT_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_ACP_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_ISRC_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_MPG_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_SPD_CON);
                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_ACR_CON);

                                // enable hdmi without audio
                                reg &= ~HDMI_ASP_ENABLE;

                                //-------------------------------------------------------------------------------------------
                                // In TCC897x HDMI Link, Encoding option is must disabled.
                                //-------------------------------------------------------------------------------------------
                                hdmi_reg_write(dev, reg|HDMI_SYS_ENABLE/*|HDMI_ENCODING_OPTION_ENABLE*/, HDMI_CON_0);
                                //-------------------------------------------------------------------------------------------
                        }

                        hdmi_enable_bluescreen(dev, 0);

                        reg = hdmi_reg_read(dev, HDMI_AVI_BYTE4);
                        pr_info("\r\n VIDEO-%s VIC[%d] %s - %dBIT\r\n", (mode==0)?"DVO":"HDMI", reg, 
                                (dev->video_params.colorSpace==0)?"RGB":(dev->video_params.colorSpace==1)?"YCbCr444":(dev->video_params.colorSpace==2)?"YCbCr422":"UNKNOWN", 
                                24+(dev->video_params.colorDepth*8));
                }
                dev->hdmi_started = 1;
        }

}

static void hdmi_stop_internal(struct tcc_hdmi_dev *dev)
{
        unsigned int reg;

        if(dev != NULL) {
                pr_info("%s \r\n", __func__);
                if(dev->hdmi_started) {
                        reg = hdmi_reg_read(dev, HDMI_CON_0);
                        hdmi_reg_write(dev, reg & ~HDMI_SYS_ENABLE,HDMI_CON_0);
                        hdmi_set_phy_pwdn(dev, 1);
                }
                dev->hdmi_started = 0;
        }
}


/**
 * If 2CH Audio Stream, fill only one Subpacket in Audio Sample Packet.
 *
 * @param  enable	[in] 0 to stop sending, 1 to start sending.
 * @return 1
 */
static int hdmi_fill_one_subpacket(struct tcc_hdmi_dev *dev, unsigned char enable)
{
        if(dev != NULL) {
        	if (enable) {
        		hdmi_reg_write(dev, HDMI_OLD_IP_VER,HDMI_IP_VER);
        	} else {
        		hdmi_reg_write(dev, HDMI_NEW_IP_VER,HDMI_IP_VER);
        	}
        }
	return 0;
}

static void tcc_hdmi_power_on(struct tcc_hdmi_dev *dev)
{
        unsigned int  val;
        
        if(dev != NULL) {
                if(++dev->power_enable_count == 1) {
                        if(!dev->suspend) {
                                #if defined(CONFIG_REGULATOR)
                                /*
                                if(dev->vdd_hdmi != NULL) {
                                        ret = regulator_enable(dev->vdd_hdmi);
                                        if (ret)
                                                pr_err("failed to enable hdmi regulator: %d\n", ret);
                                }
                                */
                                #endif

                                if(dev->pclk != NULL)
                                        clk_prepare_enable(dev->pclk);
                                if(dev->hclk != NULL)
                                        clk_prepare_enable(dev->hclk);

                                udelay(100);

                                if(dev->pclk != NULL)
                                        clk_set_rate(dev->pclk, HDMI_LINK_CLK_FREQ);
                                
                                if(dev->ipclk != NULL) {
                                        clk_prepare_enable(dev->ipclk);
                                        clk_set_rate(dev->ipclk, HDMI_PCLK_FREQ);
                                }

                                // HDMI Power-on
                                tcc_ddi_pwdn_hdmi(dev, 0);

                                val = ddi_reg_read(dev, DDICFG_PWDN);
                                ddi_reg_write(dev, val & ~(0xf << 8), DDICFG_PWDN);

                                hdmi_phy_reset(dev);

                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_HDMI_ENABLE, 1);

                                // disable HDCP INT
                                val = hdmi_reg_read(dev, HDMI_SS_INTC_CON);
                                hdmi_reg_write(dev, val & ~(1<<HDMI_IRQ_HDCP), HDMI_SS_INTC_CON);

                                // disable SPDIF INT
                                val = hdmi_reg_read(dev, HDMI_SS_INTC_CON);
                                hdmi_reg_write(dev, val & ~(1<<HDMI_IRQ_SPDIF), HDMI_SS_INTC_CON);
                                dprintk(KERN_INFO "%s End\n", __FUNCTION__);
                        }
                        dev->power_status = 1;
                }
        }
}

static void tcc_hdmi_power_off(struct tcc_hdmi_dev *dev)
{
        if(dev != NULL) {
                if(dev->power_enable_count == 1) {
                        dev->power_status = 0;
                        dev->power_enable_count = 0;
                        if(!dev->suspend) {
                                hdmi_stop_internal(dev);
                                
                                dprintk(KERN_INFO "%s\n", __FUNCTION__);
                                // HDMI PHY Reset
                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_HDMI, 0);
                                udelay(1);
                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_HDMI, 1);

                                // HDMI SPDIF Reset
                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_SPDIF, 0);
                                udelay(1);
                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_SPDIF, 1);

                                // HDMI TMDS Reset
                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_TMDS, 0);
                                udelay(1);
                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_RESET_TMDS, 1);

                                // swreset DDI_BUS HDMI
                                tcc_ddi_swreset_hdmi(dev, 1);
                                udelay(1);
                                tcc_ddi_swreset_hdmi(dev, 0);

                                // disable DDI_BUS HDMI CLK
                                tcc_ddi_hdmi_ctrl(dev, HDMICTRL_HDMI_ENABLE, 0);

                                // ddi hdmi power down
                                tcc_ddi_pwdn_hdmi(dev, 1);

                                // enable HDMI PHY Power-off
                                if(dev->ipclk != NULL)
                                        clk_disable_unprepare(dev->ipclk); // power down
                                
                                // gpio power on
                                udelay(100);

                                if(dev->pclk != NULL)
                                        clk_disable_unprepare(dev->pclk);
                                if(dev->hclk != NULL)
                                        clk_disable_unprepare(dev->hclk);
                                
                                #if defined(CONFIG_REGULATOR)
                                /*
                                if(dev->vdd_hdmi != NULL) {
                                        regulator_disable(dev->vdd_hdmi);
                                }
                                */
                                #endif
                        }
                        memset(&dev->video_params, 0, sizeof(struct HDMIVideoParameter));
                } else if(dev->power_enable_count > 1) {
                        dev->power_enable_count--;
                }
        }
}


#ifdef CONFIG_PM
static int hdmi_suspend(struct device *dev)
{
        int suspend_power_enable_count;
        struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
                
        if(hdmi_tx_dev != NULL) {    
                if(hdmi_tx_dev->runtime_suspend) {
                        pr_info("hdmi runtime suspend\r\n");
                } else {
                        pr_info("hdmi suspend\r\n");
                }
                if(!hdmi_tx_dev->suspend) {
                        if(hdmi_tx_dev->power_enable_count > 1) {
                                hdmi_stop_internal(hdmi_tx_dev);
                                suspend_power_enable_count = hdmi_tx_dev->power_enable_count;
                                hdmi_tx_dev->power_enable_count = 1;
                                tcc_hdmi_power_off(hdmi_tx_dev);
                                hdmi_tx_dev->power_enable_count = suspend_power_enable_count;
                        }
                        hdmi_tx_dev->suspend = 1 ; 
                        hdmi_tx_dev->os_suspend = 1;
                }                
        }        
        return 0;
}

static int hdmi_resume(struct device *dev)
{
        int suspend_power_enable_count;
        struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
                
        if(hdmi_tx_dev != NULL) {    
                if(hdmi_tx_dev->runtime_suspend) {
                        pr_info("hdmi runtime suspend\r\n");
                } else {
                        pr_info("hdmi suspend\r\n");
                }
                if(hdmi_tx_dev->suspend  && !hdmi_tx_dev->runtime_suspend) {
                        hdmi_tx_dev->suspend = 0; 
                        if(hdmi_tx_dev->power_enable_count > 0) {
                                suspend_power_enable_count = hdmi_tx_dev->power_enable_count;
                                hdmi_tx_dev->power_enable_count = 0;
                                tcc_hdmi_power_on(hdmi_tx_dev);
                                hdmi_tx_dev->power_enable_count = suspend_power_enable_count;
                        }
                }
        }        
        return 0;
}


static int hdmi_runtime_suspend(struct device *dev)
{
        struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        if(hdmi_tx_dev != NULL) {
                hdmi_tx_dev->runtime_suspend = 1;
                hdmi_suspend(dev);
        }
        return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
        struct tcc_hdmi_dev *hdmi_tx_dev = (struct tcc_hdmi_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        if(hdmi_tx_dev != NULL) {
                hdmi_tx_dev->runtime_suspend = 0;
                hdmi_resume(dev);
        }
        return 0;
}

#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume = hdmi_runtime_resume,
	.suspend = hdmi_suspend,
	.resume = hdmi_resume,
};
#endif

static int hdmi_parse_dt(struct tcc_hdmi_dev *dev)
{
        int ret = -ENODEV;
        struct device_node *np, *ddibus_np;

        do {
                if(dev == NULL) {
                        break;
                }
                np = dev->pdev->of_node;
                
                /* Register Mapping */
                dev->hdmi_ctrl_io = of_iomap(np, 0);
                if(dev->hdmi_ctrl_io == NULL){
                        pr_err("%s:Unable to map hdmi ctrl resource\n", __func__);
                        break;
                }
                dev->hdmi_core_io = of_iomap(np, 1);
                if(dev->hdmi_core_io == NULL){
                        pr_err("%s:Unable to map hdmi core resource\n", __func__);
                        break;
                }

                dev->hdmi_phy_io = of_iomap(np, 2);
                if(dev->hdmi_phy_io == NULL){
                        pr_err("%s:Unable to map hdmi core resource\n", __func__);
                        break;
                }

                //pr_info("%s %p %p %p\r\n", __func__, dev->hdmi_ctrl_io, dev->hdmi_core_io, dev->hdmi_phy_io);
                
                ddibus_np = of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
                if(ddibus_np == NULL) {
                        pr_err("%s:Unable to map ddibus resource\n", __func__);
                        break;
                }
                
                // Map DDI_Bus interface                
                dev->ddibus_io = of_iomap(ddibus_np, 0);
                if(dev->ddibus_io == NULL){
                        pr_err("%s:Unable to map ddibus resource\n", __func__);
                        break;
                }

                ret = 0;
        }while(0);
        return ret;
}


#if defined(CONFIG_PLATFORM_AVN)
static void send_hdmi_output_event(struct work_struct *work)
{
        char *u_events[2];
        char u_event_name[16];
        struct tcc_hdmi_dev *dev = container_of(work, struct tcc_hdmi_dev, hdmi_output_event_work);
        if(dev != NULL) {
                snprintf(u_event_name, sizeof(u_event_name), "SWITCH_STATE=%d", test_bit(HDMI_SWITCH_STATUS_ON, &dev->status)?1:0);
                u_events[0] = u_event_name;
                u_events[1] = NULL;
                pr_info("%s u_event(%s)\r\n", __func__, u_event_name);
                kobject_uevent_env(&dev->pdev->kobj, KOBJ_CHANGE, u_events);
        }
}
#endif


/**
 *      tccfb_blank
 *      @blank_mode: the blank mode we want.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *      Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *      blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *      video mode which doesn't support it. Implements VESA suspend
 *      and powerdown modes on hardware that supports disabling hsync/vsync:
 *      blank_mode == 2: suspend vsync
 *      blank_mode == 3: suspend hsync
 *      blank_mode == 4: powerdown
 *
 *      Returns negative errno on error, or zero on success.
 *
 */
static int hdmi_blank(struct tcc_hdmi_dev *dev, int blank_mode)
{
        int ret = -EINVAL;
        struct device *pdev = NULL;

        pr_info("%s : blank(mode=%d)\n",__func__, blank_mode);

        if(dev != NULL) {
                pdev = dev->pdev;
        }
        
        if(pdev != NULL) {
                #ifdef CONFIG_PM
                switch(blank_mode) {
                        case FB_BLANK_POWERDOWN:
                        case FB_BLANK_NORMAL:
                                pm_runtime_put_sync(pdev);
                                ret = 0;
                                break;
                        case FB_BLANK_UNBLANK:
                                if(pdev->power.usage_count.counter == 1) {
                                /* 
                                 * usage_count = 1 ( resume ), blank_mode = 0 ( FB_BLANK_UNBLANK ) means that 
                                 * this driver is stable state when booting. don't call runtime_suspend or resume state  */
                                } else {
                	                pm_runtime_get_sync(dev->pdev);
                                }
                                ret = 0;
                                break;
                        case FB_BLANK_HSYNC_SUSPEND:
                        case FB_BLANK_VSYNC_SUSPEND:
                                ret = 0;
                                break;
                        default:
                                ret = -EINVAL;
                                break;
                }
                #endif
        }
        return ret;
}

static int hdmi_open(struct inode *inode, struct file *file)
{
        int ret = -1;
        struct miscdevice *misc = (struct miscdevice *)(file!=NULL)?file->private_data:NULL;
        struct tcc_hdmi_dev *dev = (struct tcc_hdmi_dev *)(misc!=NULL)?dev_get_drvdata(misc->parent):NULL;


        if(dev != NULL) {
                file->private_data = dev;  
        
                if(dev != NULL) {
                        dev->open_cnt++;
                        dprintk("%s (%d)\n", __func__, dev->open_cnt);
                }
                ret = 0;
        }

        return ret;
}

static int hdmi_release(struct inode *inode, struct file *file)
{
        int ret = -1;
        struct tcc_hdmi_dev *dev = (struct tcc_hdmi_dev *)(file!=NULL)?file->private_data:NULL;
        if(dev != NULL) {
                if(dev->open_cnt > 0) {
                        dev->open_cnt--;
                        dprintk("%s (%d)\n", __func__, dev->open_cnt);
                }
                ret = 0;
        }        
        return ret;
}

static ssize_t hdmi_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

static ssize_t hdmi_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

static unsigned int hdmi_poll(struct file *file, poll_table *wait)
{
        return 0;
}

static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        long ret = -EINVAL;
        struct tcc_hdmi_dev *dev = (struct tcc_hdmi_dev *)(file!=NULL)?file->private_data:NULL;

        if(dev != NULL) {                      
                switch (cmd) {
                        case HDMI_IOC_GET_PWR_STATUS:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_GET_PWR_STATUS : %d )\n", dev->power_status);
                                        if(copy_to_user((void __user *)arg, &dev->power_status, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;
                        
                        case HDMI_IOC_SET_PWR_CONTROL:
                                {
                                        
                                        unsigned int cmd;
                                        if(copy_from_user(&cmd, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_PWR_CONTROL :  %d )\n", cmd);
                                        switch(cmd) {
                                                case 0:
                                                        tcc_hdmi_power_off(dev);
                                                        break;
                                                case 1:
                                                        tcc_hdmi_power_on(dev);
                                                        break;
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_SET_PWR_V5_CONTROL:
                                {
                                        
                                        unsigned int cmd;
                                        if(copy_from_user(&cmd, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_PWR_V5_CONTROL :  %d )\n", cmd);
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_SUSPEND_STATUS:
                                {
                                        if (dev->os_suspend) {
                                                if(copy_to_user((void __user *)arg, &dev->os_suspend, sizeof(unsigned int))) {
                                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                        break;
                                                }
                                                dev->os_suspend = 0;
                                        } else {
                                                if(copy_to_user((void __user *)arg, &dev->os_suspend, sizeof(unsigned int))) {
                                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                        break;
                                                }
                                        }
                                        ret = 0;
                                }
                                break;
                        case HDMI_IOC_SET_COLORSPACE:
                                {
                                        int space;

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_COLORSPACE)\n");
                                        if(copy_from_user(&space, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_color_space(dev, space) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_COLORSPACE) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }                                        
                                        dev->video_params.colorSpace = space;
                                        ret = 0;
                                }
                                break;
                        case HDMI_IOC_SET_COLORDEPTH:
                                {
                                        int depth;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_COLORDEPTH)\n");
                                        if(copy_from_user(&depth, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_color_depth(dev, depth) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_COLORDEPTH) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }  
                                        dev->video_params.colorDepth = depth;
                                        ret = 0;
                                }
                                break;
                        case HDMI_IOC_SET_HDMIMODE:
                                {
                                        int mode;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_HDMIMODE)\n");
                                        if(copy_from_user(&mode, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_hdmimode(dev, mode) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_HDMIMODE) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }  
                                        dev->video_params.mode = mode;
                                        ret = 0;
                                }
                                break;
                        case HDMI_IOC_SET_VIDEOMODE:
                                {
                                        struct HDMIVideoParameter video_mode;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_VIDEOMODE)\n");
                                        if(copy_from_user(&video_mode, (void __user *)arg, sizeof(struct HDMIVideoParameter))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_video_mode(dev, &video_mode) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_VIDEOMODE) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }  
                                }
                                break;
                                        
                        case HDMI_IOC_SET_VIDEOFORMAT_INFO:
                                {
                                        enum VideoFormat video_format;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_VIDEOFORMAT_INFO)\n");
                                        if(copy_from_user(&video_format, (void __user *)arg, sizeof(enum VideoFormat))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        dev->video_params.resolution = video_format;
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_GET_VIDEOCONFIG:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_GET_VIDEOCONFIG)\n");
                                        if(copy_to_user((void __user *)arg, &dev->video_params, sizeof(struct HDMIVideoParameter))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_HDMISTART_STATUS:
                                {
                                        if(copy_to_user((void __user *)arg, &dev->hdmi_started, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_LCDC_TIMING:
                                {
                                        /* Nothing to do */
                                }
                                break;

                        case HDMI_IOC_SET_BLUESCREEN:
                                {
                                        unsigned char val;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_BLUESCREEN)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(unsigned char))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        hdmi_enable_bluescreen(dev, (val > 0)?1:0);
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_SET_PIXEL_LIMIT:
                                {
                                        int val;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_PIXEL_LIMIT)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_pixel_limit(dev, val) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_PIXEL_LIMIT) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }  
                                        dev->color_range = val;
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_PIXEL_LIMIT:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_GET_PIXEL_LIMIT)\n");
                                        if(copy_to_user((void __user *)arg, &dev->color_range, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_PIXEL_ASPECT_RATIO:
                                {
                                        int val;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_PIXEL_ASPECT_RATIO)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_pixel_aspect_ratio(dev, val) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_PIXEL_ASPECT_RATIO) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        dev->video_params.pixelAspectRatio = val;
                                        ret  = 0;
                                }
                                break;
                                
                        case HDMI_IOC_SET_COLORIMETRY:
                                {
                                        int val;

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_COLORIMETRY)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_colorimetry(dev, val) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_COLORIMETRY) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        dev->video_params.colorimetry = val;
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_SET_AVMUTE:
                                {
                                        unsigned char val;
                                        unsigned int reg;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_AVMUTE)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(unsigned char))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        reg = hdmi_reg_read(dev, HDMI_MODE_SEL);
                                                        if (reg & HDMI_MODE_SEL_HDMI) {
                                                                if (val) {
                                                                        // set AV Mute
                                                                        hdmi_reg_write(dev, GCP_AVMUTE_ON,HDMI_GCP_BYTE1);
                                                                        hdmi_reg_write(dev, GCP_TRANSMIT_EVERY_VSYNC,HDMI_GCP_CON);
                                                                } else {
                                                                        // clear AV Mute
                                                                        hdmi_reg_write(dev, GCP_AVMUTE_OFF, HDMI_GCP_BYTE1);
                                                                        hdmi_reg_write(dev, GCP_TRANSMIT_EVERY_VSYNC,HDMI_GCP_CON);
                                                                }
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_SET_AUDIOPACKETTYPE:
                                {
                                        int val;

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_AUDIOPACKETTYPE)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_audio_packet_type(dev, val) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_AUDIOPACKETTYPE) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        dev->audio_packet = val;
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_AUDIOPACKETTYPE:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_GET_AUDIOPACKETTYPE)\n");

                                        if(copy_to_user((void __user *)arg, &dev->audio_packet, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_SET_AUDIOSAMPLEFREQ:
                                {
                                        int val;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_AUDIOSAMPLEFREQ)\n");

                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_audio_sample_freq(dev, val) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_AUDIOSAMPLEFREQ) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        dev->audio_sample_freq = val;
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_AUDIOSAMPLEFREQ:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_GET_AUDIOSAMPLEFREQ)\n");
                                        if(copy_to_user((void __user *)arg, &dev->audio_sample_freq, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_AUDIOCHANNEL:
                                {
                                        int val;

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_AUDIOCHANNEL)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_audio_number_of_channels(dev, val) < 0) {
                                                                pr_err("HDMI: ioctl(HDMI_IOC_SET_AUDIOCHANNEL) \r\n");
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        dev->audio_channel = val;
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_AUDIOCHANNEL:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_GET_AUDIOCHANNEL)\n");
                                        if(copy_to_user((void __user *)arg, &dev->audio_channel, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_SPEAKER_ALLOCATION:
                                {
                                        unsigned int val;

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_SPEAKER_ALLOCATION)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        hdmi_reg_write(dev, val,HDMI_AUI_BYTE4);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_SPEAKER_ALLOCATION:
                                {
                                        unsigned int val;

                                        io_debug("HDMI: ioctl(HDMI_IOC_GET_SPEAKER_ALLOCATION)\n");

                                        val = hdmi_reg_read(dev, HDMI_AUI_BYTE4);

                                        if(copy_to_user((void __user *)arg, &val, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_GET_PHYREADY:
                                {
                                        unsigned char phy_status = hdmi_reg_read(dev, HDMI_SS_PHY_STATUS_0);
                                        if(copy_to_user((void __user *)arg, &phy_status, sizeof(unsigned char))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_PHYRESET:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_PHYRESET)\n");
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        hdmi_phy_reset(dev);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_HPD_SWITCH_STATUS:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_HPD_SWITCH_STATUS)\n");
                                        ret = 0;
                                        break;
                                }

                        case HDMI_IOC_START_HDMI:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_START_HDMI)\n");

                                        #if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
                                        if(dev->video_params.mode == HDMI)
                                                hdmi_set_yuv420_color_space(dev);
                                        #endif
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        hdmi_start_internal(dev);
                                                        #if defined(CONFIG_PLATFORM_AVN)       
                                                        set_bit(HDMI_SWITCH_STATUS_ON, &dev->status);
                                                        schedule_work(&dev->hdmi_output_event_work);
                                                        #endif
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        
                                        ret = 0;
                                }
                                break;
                        
                        case HDMI_IOC_STOP_HDMI:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_STOP_HDMI)\n");

                                        #if defined(CONFIG_PLATFORM_AVN)
                                        clear_bit(HDMI_SWITCH_STATUS_ON, &dev->status);
                                        schedule_work(&dev->hdmi_output_event_work);
                                        #endif
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        hdmi_stop_internal(dev);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_SET_AUDIO_ENABLE:
                                {
                                        unsigned char enable;
                                        unsigned int reg, mode;

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_AUDIO_ENABLE)\n");

                                        if(copy_from_user(&enable, (void __user *)arg, sizeof(unsigned char))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        // check HDMI mode
                                                        mode = hdmi_reg_read(dev, HDMI_MODE_SEL) & HDMI_MODE_SEL_HDMI;

                                                        reg = hdmi_reg_read(dev, HDMI_CON_0);
                                                        
                                                        // enable audio output
                                                        if ( enable && mode ) {
                                                                hdmi_aui_update_checksum(dev);
                                                                hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC,HDMI_AUI_CON);
                                                                //  hdmi_reg_write(dev, TRANSMIT_ONCE,HDMI_AUI_CON);
                                                                hdmi_reg_write(dev, ACR_MEASURED_CTS_MODE,HDMI_ACR_CON);
                                                                hdmi_reg_write(dev, reg|HDMI_ASP_ENABLE,HDMI_CON_0);

                                                                io_debug("HDMI: ioctl(HDMI_IOC_SET_AUDIO_ENABLE) : enable\n");
                                                        } else {
                                                                // disable encryption
                                                                hdmi_reg_write(dev, reg& ~HDMI_ASP_ENABLE,HDMI_CON_0);
                                                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_AUI_CON);
                                                                hdmi_reg_write(dev, DO_NOT_TRANSMIT,HDMI_ACR_CON);
                                                                io_debug("HDMI: ioctl(HDMI_IOC_SET_AUDIO_ENABLE) : disable\n");
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_RESET_AUISAMPLEFREQ:
                                {
                                        unsigned int reg;
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        reg = hdmi_reg_read(dev, HDMI_AUI_BYTE2);
                                                        reg &= ~HDMI_AUI_SF_MASK;
                                                        hdmi_reg_write(dev, reg, HDMI_AUI_BYTE2);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_VIDEO_FORMAT_CONTROL:
                                {
                                        struct HDMIVideoFormatCtrl video_format_ctrl;
                                        io_debug("HDMI: ioctl(HDMI_IOC_VIDEO_FORMAT_CONTROL)\n");
                                        if(copy_from_user(&video_format_ctrl, (void __user *)arg, sizeof(struct HDMIVideoFormatCtrl))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        hdmi_set_spd_infoframe(dev, video_format_ctrl );
                                                        hdmi_reg_write(dev, TRANSMIT_EVERY_VSYNC,HDMI_SPD_CON);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_VIDEO_SOURCE:
                                {
                                        int mode;

                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_VIDEO_SOURCE)\n");
                                        if(copy_from_user(&mode, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if (mode == HDMI_SOURCE_EXTERNAL) {
                                                                hdmi_reg_write(dev, HDMI_EXTERNAL_VIDEO, HDMI_VIDEO_PATTERN_GEN);

                                                        } else {
                                                                hdmi_reg_write(dev, HDMI_INTERNAL_VIDEO, HDMI_VIDEO_PATTERN_GEN);
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_DEBUG_HDMI_CORE:
                                {
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_DEBUG_HDMI_CORE_VIDEO:
                                {
                                        ret = 0;
                                }
                                break;
                                
                        case HDMI_IOC_FILL_ONE_SUBPACKET:
                                {
                                        unsigned char enable;

                                        io_debug("HDMI: ioctl(HDMI_IOC_FILL_ONE_SUBPACKET)");
                                        if(copy_from_user(&enable, (void __user *)arg, sizeof(unsigned char ))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        hdmi_fill_one_subpacket(dev, enable);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_PHY_FREQ:
                                {
                                        enum PHYFreq phy_freq;
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_PHY_FREQ)\n");
                                        if(copy_from_user(&phy_freq, (void __user *)arg, sizeof(enum PHYFreq))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        if ( hdmi_set_phy_freq(dev, phy_freq) < 0 ){
                                                                pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                                                                break;
                                                        }
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_PHY_INDEX:
                                {
                                        io_debug("HDMI: ioctl(HDMI_IOC_SET_PHY_INDEX)\n");
                                        if(copy_from_user(&dev->phy_depth, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        if(dev->phy_depth < 0 || dev->phy_depth > 2) {
                                                pr_err("%s phy depth is out of range at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_SET_DEBUG:
                                {
                                        struct hdmi_dbg dbg;
                                        io_debug("HDMI_IOC_SET_DEBUG\n");
                                        if(copy_from_user(&dbg, (void __user *)arg, sizeof(struct hdmi_dbg))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }          
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        //hdmi_reg_write(dev, dbg.value, regs_core + dbg.offset);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                        ret = 0;
                                }
                                break;

                        case HDMI_IOC_BLANK:
                                {
                                        unsigned int cmd;
                                        if(copy_from_user(&cmd, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        } 
                                        if(!dev->suspend) {
                                                if(dev->power_status) {
                                                        ret = hdmi_blank(dev, cmd);
                                                } else {
                                                        pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                                                }
                                        } else {
                                                pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                                        }
                                }
                                break;
                                
                        default:
                                break;
                }
        }

        return ret;
}

static const struct file_operations hdmi_fops =
{
        .owner          = THIS_MODULE,
        .open           = hdmi_open,
        .release        = hdmi_release,
        .read           = hdmi_read,
        .write          = hdmi_write,
        .unlocked_ioctl = hdmi_ioctl,
        .poll           = hdmi_poll,
};

static int hdmi_remove(struct platform_device *pdev)
{    

        struct tcc_hdmi_dev *dev = NULL;
                        
        if(pdev != NULL) {
                dev = (struct tcc_hdmi_dev *)dev_get_drvdata(pdev->dev.parent);
        
                if(dev != NULL) {
                        /* Deinit Interrupt */
                        if(dev->misc != NULL) {
                                misc_deregister(dev->misc);
                                devm_kfree(dev->pdev, dev->misc);
                        }
                }
                devm_kfree(dev->pdev, dev);

                #ifdef CONFIG_PM
                pm_runtime_disable(dev->pdev);
                #endif
        }        
        return 0;
        
}

static int hdmi_probe(struct platform_device *pdev)
{
        int ret =  -ENODEV;

        struct tcc_hdmi_dev *dev;

        pr_info("****************************************\n");
        pr_info("%s:HDMI driver %s\n", __func__, SRC_VERSION);
        pr_info("****************************************\n");
        
        dev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hdmi_dev), GFP_KERNEL);
        do {
                if (dev == NULL) {
                	ret = -ENOMEM;
                	break;
                }

                dev->pdev = &pdev->dev;

                if(hdmi_parse_dt(dev) < 0) {
                        break;
                }

        	dev->pclk = of_clk_get(pdev->dev.of_node, 0);
        	dev->hclk = of_clk_get(pdev->dev.of_node, 1);
                dev->ipclk = of_clk_get(pdev->dev.of_node, 2);
                
        	if (IS_ERR_OR_NULL(dev->pclk)) {
        		pr_err("HDMI: failed to get hdmi pclock\n");
        		dev->pclk = NULL;
                        break;
        	}

        	if (IS_ERR_OR_NULL(dev->hclk)) {
        		pr_err("HDMI: failed to get hdmi hclock\n");
        		dev->hclk = NULL;
                        break;
                }

                if (IS_ERR_OR_NULL(dev->ipclk)) {
        		pr_err("HDMI: failed to get hdmi ipclock\n");
        		dev->hclk = NULL;
                        break;
        	}

                clk_prepare_enable(dev->pclk);
                clk_prepare_enable(dev->hclk);

                /** uevent work for dual display */
                #if defined(CONFIG_PLATFORM_AVN)
                INIT_WORK(&dev->hdmi_output_event_work, send_hdmi_output_event);
                #endif

                dev->misc = devm_kzalloc(&pdev->dev, sizeof(struct miscdevice), GFP_KERNEL);
                if(dev->misc == NULL) {
                        ret = -ENOMEM;
                        break;
                }

                dev->misc->minor = MISC_DYNAMIC_MINOR;
                dev->misc->name = "hdmi";
                dev->misc->fops = &hdmi_fops;
                dev->misc->parent = dev->pdev;
                ret = misc_register(dev->misc);

                if(ret < 0) {
                        break;
                }

                dev_set_drvdata(dev->pdev, dev);

                clk_disable_unprepare(dev->pclk);
                clk_disable_unprepare(dev->hclk);

                #if defined(CONFIG_REGULATOR)
                /*
                dev->vdd_v5p0 = regulator_get(dev->pdev, "dev->vdd_v5p0");
                if (IS_ERR(dev->vdd_v5p0)) {
                        pr_warning("clock_table: failed to obtain dev->vdd_v5p0\n");
                        dev->vdd_v5p0 = NULL;
                }

                dev->vdd_hdmi = regulator_get(dev->pdev, "dev->vdd_hdmi");
                if (IS_ERR(dev->vdd_hdmi)) {
                        pr_warning("clock_table: failed to obtain dev->vdd_hdmi\n");
                        dev->vdd_hdmi = NULL;
                }
                */
                #endif

                #ifdef CONFIG_PM
                pm_runtime_set_active(dev->pdev);	
                pm_runtime_enable(dev->pdev);  
                pm_runtime_get_noresume(dev->pdev);  //increase usage_count 
                #endif

                api_dev = dev;
        } while(0);

        if(ret < 0) {
                if(dev != NULL) {
                        devm_kfree(dev->pdev, dev);
                };
        }
	return ret;
}

static struct of_device_id hdmi_of_match[] = {
        { .compatible = "telechips,tcc897x-hdmi" },
        {}
};
MODULE_DEVICE_TABLE(of, hdmi_of_match);

static struct platform_driver tcc_hdmi = {
	.probe	= hdmi_probe,
	.remove	= hdmi_remove,
	.driver	= {
                .name = "tcc_hdmi",
		.owner = THIS_MODULE,
                #ifdef CONFIG_PM
		.pm = &hdmi_pm_ops,
                #endif
		.of_match_table = of_match_ptr(hdmi_of_match),
	},
};

static __init int hdmi_init(void)
{	
	return platform_driver_register(&tcc_hdmi);
}

static __exit void hdmi_exit(void)
{
	platform_driver_unregister(&tcc_hdmi);
}

/* Export HDMI APIS */
int hdmi_api_get_power_status(void)
{
        return hdmi_get_power_status(api_dev);
}

EXPORT_SYMBOL(hdmi_api_get_power_status);
int hdmi_get_VBlank(void)
{
        int blank = 0;
        if(api_dev != NULL) {
                if(!api_dev->suspend) {
                        if(api_dev->power_status) {
                                blank = hdmi_get_VBlank_internal(api_dev);
                        } else {
                                pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
        return blank;
}
EXPORT_SYMBOL(hdmi_get_VBlank);

void hdmi_start(void)
{        
        if(api_dev != NULL) {
                if(!api_dev->suspend) {
                        if(api_dev->power_status) {
                                hdmi_start_internal(api_dev);
                        } else {
                                pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
}
EXPORT_SYMBOL(hdmi_start);

void hdmi_stop(void)
{
        if(api_dev != NULL) {
                if(!api_dev->suspend) {
                        if(api_dev->power_status) {
                                hdmi_stop_internal(api_dev);
                        } else {
                                pr_err("%s hdmi is power down at line(%d)\r\n", __func__, __LINE__);
                        }
                } else {
                        pr_err("%s hdmi was in suspend at line(%d)\r\n", __func__, __LINE__);
                }
        }
}
EXPORT_SYMBOL(hdmi_stop);


module_init(hdmi_init);
module_exit(hdmi_exit);
MODULE_DESCRIPTION("TCCxxx hdmi driver");
MODULE_LICENSE("GPL");
