/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        audio.c
*  \brief       HDMI Audio controller driver
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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <asm/mach-types.h>
#include <linux/uaccess.h>


#include <asm/io.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#include <regs-hdmi.h>
#include <hdmi_1_4_audio.h>

#define SRC_VERSION 		"4.14_1.0.3" /* Driver version number */

#define AUDIO_DEBUG 0
#if AUDIO_DEBUG
#define DPRINTK(args...)    printk(args)
#else
#define DPRINTK(args...)
#endif

#define HDMI_IOCTL_DEBUG 0
#if HDMI_IOCTL_DEBUG 
#define io_debug(...) pr_info(__VA_ARGS__)
#else
#define io_debug(...)
#endif


#define SPDIF_DONE          0
#define SPDIF_ERROR         1

#ifndef XTIN_CLK_RATE
#define XTIN_CLK_RATE 32768
#endif

#ifndef PERI_SPDIF3
#define PERI_SPDIF3 27
#endif

#ifndef PERI_ADAI0
#define PERI_ADAI0 28
#endif

/**
 * @struct spdif_struct
 *
 * This struct is used in SPDIF interrupt handler.
 */
struct spdif_struct {
    /** State of SPDIF receiver */
    int state;
    /** Coding type of input audio stream */
    int codingtype;
};


struct tcc_hdmi_audio_dev {
        int open_cnt;
        struct device *pdev;

        /* HDMI Clock */
        struct clk *pclk;
        struct clk *hclk;
        struct clk *spidf_pclk;

        struct miscdevice *misc;

        int audio_irq;
        
        unsigned int suspend;
        unsigned int runtime_suspend;

        /** Register interface */
        volatile void __iomem   *hdmi_ctrl_io;
        volatile void __iomem   *hdmi_spdif_io;
        volatile void __iomem   *hdmi_i2s_io;
        volatile unsigned long  status;

        struct HDMIAudioParameter audio_params;
        unsigned int coding_type;
        unsigned int lpcm_word_length;
        struct spdif_struct spdif_struct;
};

extern int tcc_ckc_set_hdmi_audio_src(unsigned int src_id);

static unsigned int hdmi_audio_reg_read(struct tcc_hdmi_audio_dev *dev, unsigned int offset)
{
        unsigned int val = 0;
        volatile void __iomem *hdmi_io = NULL;
        
        if(offset & 0x3){
                return val;
        }
       
        if(dev != NULL) {
                if(offset >= HDMIDP_PHYREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_I2SREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_i2s_io;
                        offset -= HDMIDP_I2SREG(0);
                } else if(offset >= HDMIDP_SPDIFREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_spdif_io;
                        offset -= HDMIDP_SPDIFREG(0);
                } else if(offset >= HDMIDP_AESREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMIREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMI_SSREG(0)) { 
                        hdmi_io = (volatile void __iomem *)dev->hdmi_ctrl_io;
                        offset -= HDMIDP_HDMI_SSREG(0);
                }
        }

        if(hdmi_io == NULL) {
                pr_err("%s hdmi_io is NULL at offset(0x%x)\r\n", __func__, offset);
        } else {
                //pr_info(" >> Read (%p)\r\n", (void*)(hdmi_io + offset));
                val = ioread32((void*)(hdmi_io + offset));
        }
        return val;
}

static void hdmi_audio_reg_write(struct tcc_hdmi_audio_dev *dev, unsigned int data, unsigned int offset)
{
        volatile void __iomem *hdmi_io = NULL;

        if(offset & 0x3){
                return;
        }
       
        if(dev != NULL) {
                if(offset >= HDMIDP_PHYREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_I2SREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_i2s_io;
                        offset -= HDMIDP_I2SREG(0);
                } else if(offset >= HDMIDP_SPDIFREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_spdif_io;
                        offset -= HDMIDP_SPDIFREG(0);
                } else if(offset >= HDMIDP_AESREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMIREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMI_SSREG(0)) { 
                        hdmi_io = (volatile void __iomem *)dev->hdmi_ctrl_io;
                        offset -= HDMIDP_HDMI_SSREG(0);
                }
        }
        if(hdmi_io == NULL) {
                pr_err("%s hdmi_io is NULL at offset(0x%x)\r\n", __func__, offset);
        } else {
                //pr_info(" >> Write(%p) = 0x%x\r\n", (void*)(hdmi_io + offset), data);
                iowrite32(data, (void*)(hdmi_io + offset));
        }
}



/**
 * @brief SPDIF interrupt handler.
 *
 * Handle interrupt requests from SPDIF receiver. @n
 * When it succeeds in synchronizing audio stream, handler -@n
 * starts to send audio stream to HDMI H/W;Otherwise, when fail to @n
 * synchronize audio stream, handler restarts to synchronize audio stream.
 */

static irqreturn_t audio_spdif_handler(int irq, void *dev_id)
{
        unsigned int flag, status;
        struct tcc_hdmi_audio_dev *dev =  (struct tcc_hdmi_audio_dev *)dev_id;

        if(dev != NULL) {
                flag = hdmi_audio_reg_read(dev, HDMI_SS_INTC_FLAG);

                if (flag & (1<<HDMI_IRQ_SPDIF)) {
                        // check spdif status
                        status = hdmi_audio_reg_read(dev, HDMI_SS_SPDIF_IRQ_STATUS);

                        #if AUDIO_DEBUG
                        if (status & SPDIF_CLK_RECOVERY_FAIL_MASK) {
                                pr_info("SPDIFKERN_INFO_CLK_RECOVERY_FAIL_MASK\n");
                        }

                        if (status & SPDIF_STATUS_RECOVERED_MASK) {
                                pr_info("SPDIF_STATUS_RECOVERED_MASK\n");
                        }

                        if (status & SPDIF_PREAMBLE_DETECTED_MASK) {
                                pr_info("SPDIF_PREAMBLE_DETECTED_MASK\n");
                        }

                        if (status & SPDIF_HEADER_NOT_DETECTED_MASK) {
                                pr_info("SPDIF_HEADER_NOT_DETECTED_MASK\n");
                        }

                        if (status & SPDIF_HEADER_DETECTED_MASK) {
                                pr_info("SPDIF_HEADER_DETECTED_MASK\n");
                        }

                        if (status & SPDIF_PAPD_NOT_DETECTED_MASK) {
                                pr_info("SPDIF_PAPD_NOT_DETECTED_MASK\n");
                        }

                        if (status & SPDIF_ABNORMAL_PD_MASK) {
                                pr_info("SPDIF_ABNORMAL_PD_MASK\n");
                        }

                        if (status & SPDIF_BUFFER_OVERFLOW_MASK) {
                                pr_info("SPDIF_BUFFER_OVERFLOW_MASK\n");
                        }
                        #endif
                        // clear pending bit
                        hdmi_audio_reg_write(dev, status,HDMI_SS_SPDIF_IRQ_STATUS);

                        switch(dev->spdif_struct.codingtype) { 
                                case SPDIF_NLPCM:
                                        // for NLPCM
                                        switch(status) {
                                                // normal
                                                case SPDIF_STATUS_RECOVERED_MASK:
                                                case SPDIF_HEADER_DETECTED_MASK:
                                                        {
                                                            DPRINTK(KERN_INFO "Normal Procedure\n");
                                                        }
                                                        break;
                                                case (SPDIF_HEADER_DETECTED_MASK | SPDIF_STATUS_RECOVERED_MASK):
                                                        {
                                                                unsigned int val = hdmi_audio_reg_read(dev, HDMI_SS_SPDIF_IRQ_MASK);
                                                                val &= ~(SPDIF_HEADER_DETECTED_MASK | SPDIF_STATUS_RECOVERED_MASK);
                                                                // mask this interrupt. because this will happen continuously even if in normal state
                                                                hdmi_audio_reg_write(dev, val, HDMI_SS_SPDIF_IRQ_MASK);
                                                                DPRINTK(KERN_INFO "Succeed! Audio Start\n");
                                                                hdmi_audio_reg_write(dev, SPDIF_RUNNING, HDMI_SS_SPDIF_OP_CTRL); // 0b11 // run
                                                        }
                                                        break;
                                                // error state
                                                default :
                                                        {
                                                            DPRINTK(KERN_INFO "Fail! Audio Restart\n");
                                                            hdmi_audio_reg_write(dev, SPDIF_SIGNAL_RESET, HDMI_SS_SPDIF_OP_CTRL); // software reset
                                                            hdmi_audio_reg_write(dev, 0xFF, HDMI_SS_SPDIF_IRQ_MASK); // enable all interrupts
                                                            hdmi_audio_reg_write(dev, SPDIF_SIGNAL_DETECT, HDMI_SS_SPDIF_OP_CTRL); // detect signal
                                                        }
                                                        break;
                                        }
                                        break;
                                case SPDIF_LPCM:
                                        // for LPCM
                                        switch(status)
                                        {
                                        // normal
                                        case SPDIF_STATUS_RECOVERED_MASK:
                                // TODO: check if it is bug or not even if SPDIF_HEADER_NOT_DETECTED_MASK occurs, it works well.
                                        case (SPDIF_STATUS_RECOVERED_MASK | SPDIF_HEADER_NOT_DETECTED_MASK):
                                        {
                                            hdmi_audio_reg_write(dev, hdmi_audio_reg_read(dev, HDMI_SS_SPDIF_IRQ_MASK) & ~(SPDIF_STATUS_RECOVERED_MASK | SPDIF_HEADER_NOT_DETECTED_MASK), HDMI_SS_SPDIF_IRQ_MASK);
                                            DPRINTK(KERN_INFO "Succeed! Audio Start\n");
                                            hdmi_audio_reg_write(dev, SPDIF_RUNNING, HDMI_SS_SPDIF_OP_CTRL); // 0b11 // run
                                            break;
                                        }
                                        // error state
                                        default :
                                        {
                                            // initialize history
                                            DPRINTK(KERN_INFO "Fail! Audio Restart\n");
                                            hdmi_audio_reg_write(dev, SPDIF_SIGNAL_RESET, HDMI_SS_SPDIF_OP_CTRL); // software reset
                                            hdmi_audio_reg_write(dev, SPDIF_BUFFER_OVERFLOW_MASK | SPDIF_PREAMBLE_DETECTED_MASK | SPDIF_STATUS_RECOVERED_MASK | SPDIF_CLK_RECOVERY_FAIL_MASK, HDMI_SS_SPDIF_IRQ_MASK); // enable all interrupts
                                            hdmi_audio_reg_write(dev, SPDIF_SIGNAL_DETECT, HDMI_SS_SPDIF_OP_CTRL); // detect signal
                                        }
                                        }
                                        break;
                                default:
                                        DPRINTK(KERN_INFO "Not Correct SPDIF CODING TYPE\n");
                                        break;
                        }
                } else {
                        return IRQ_NONE;
                }
        }

        return IRQ_HANDLED;
}





/**
 * Set audio input port.
 *
 * @param   port    [in]    Audio input port.
 * @return  If argument is invalid, return 0;Otherwise, return 1.
 */

static int setAudioInputPort(struct tcc_hdmi_audio_dev *dev, enum HDMIAudioPort port)
{
        int ret = -1;
        unsigned int reg;

        if(dev != NULL) {
                switch (port) {
                        case I2S_PORT:
                                {
                                        /* disable SPDIF INT */
                                        
                                        reg = hdmi_audio_reg_read(dev, HDMI_SS_INTC_CON);
                                        reg &= ~(1<<HDMI_IRQ_SPDIF);
                                        
                                        hdmi_audio_reg_write(dev, reg, HDMI_SS_INTC_CON);

                                        // disable DSD
                                        hdmi_audio_reg_write(dev, I2S_DSD_CON_DISABLE, HDMI_SS_I2S_DSD_CON);

                                        // I2S control
                                        hdmi_audio_reg_write(dev, I2S_CON_SC_POL_FALLING | I2S_CON_CH_POL_LOW, HDMI_SS_I2S_CON_1);

                                        // I2S MUX Control
                                        hdmi_audio_reg_write(dev, I2S_IN_MUX_ENABLE | I2S_IN_MUX_CUV_ENABLE | I2S_IN_MUX_SELECT_I2S | I2S_IN_MUX_IN_ENABLE, HDMI_SS_I2S_IN_MUX_CON);

                                        // enable all channels
                                        hdmi_audio_reg_write(dev, I2S_MUX_CH_ALL_ENABLE , HDMI_SS_I2S_MUX_CH);

                                        // enable CUV from right and left channel
                                        hdmi_audio_reg_write(dev, I2S_MUX_CUV_LEFT_ENABLE| I2S_MUX_CUV_RIGHT_ENABLE , HDMI_SS_I2S_MUX_CUV);
                                        ret = 0;
                                }
                                break;
                        case SPDIF_PORT:
                                {
                                        // set mux control
                                        hdmi_audio_reg_write(dev, I2S_IN_MUX_SELECT_SPDIF | I2S_IN_MUX_ENABLE, HDMI_SS_I2S_IN_MUX_CON);

                                        // enable all channels
                                        hdmi_audio_reg_write(dev, I2S_MUX_CH_ALL_ENABLE , HDMI_SS_I2S_MUX_CH);

                                        // enable CUV from right and left channel
                                        hdmi_audio_reg_write(dev, I2S_MUX_CUV_LEFT_ENABLE| I2S_MUX_CUV_RIGHT_ENABLE , HDMI_SS_I2S_MUX_CUV);

                                        hdmi_audio_reg_write(dev, 0, HDMI_SS_SPDIF_CLK_CTRL); // enable clock???
                                        hdmi_audio_reg_write(dev, SPDIF_CLK_CTRL_ENABLE, HDMI_SS_SPDIF_CLK_CTRL);

                                        hdmi_audio_reg_write(dev, SPDIF_CONFIG_1_NOISE_FILTER_3_SAMPLES | SPDIF_CONFIG_1_UVCP_ENABLE | SPDIF_CONFIG_1_WORD_LENGTH_MANUAL | SPDIF_CONFIG_1_ALIGN_32BIT | SPDIF_CONFIG_1_HDMI_2_BURST, HDMI_SS_SPDIF_CONFIG_1);

                                        // max 24bits
                                        hdmi_audio_reg_write(dev, 0x0b, HDMI_SS_SPDIF_USER_VALUE_1);

                                        if (dev->spdif_struct.codingtype == SPDIF_NLPCM) {
                                                hdmi_audio_reg_write(dev, 0xFF,HDMI_SS_SPDIF_IRQ_MASK); // enable all
                                        } else {
                                                hdmi_audio_reg_write(dev, SPDIF_BUFFER_OVERFLOW_MASK | SPDIF_PREAMBLE_DETECTED_MASK | SPDIF_STATUS_RECOVERED_MASK | SPDIF_CLK_RECOVERY_FAIL_MASK, HDMI_SS_SPDIF_IRQ_MASK);
                                        }
                                        // enable SPDIF INT
                                        reg = hdmi_audio_reg_read(dev, HDMI_SS_INTC_CON);
                                        reg |= (1<<HDMI_IRQ_SPDIF) | (1<<HDMI_IRQ_GLOBAL);
                                        hdmi_audio_reg_write(dev, reg, HDMI_SS_INTC_CON);
                        
                                        // start to detect signal
                                        hdmi_audio_reg_write(dev, SPDIF_SIGNAL_DETECT, HDMI_SS_SPDIF_OP_CTRL);
                                        ret = 0;
                                }
                                break;
                        case DSD_PORT:
                                {
                                        // enable all channels
                                        hdmi_audio_reg_write(dev, I2S_MUX_CH_0_LEFT_ENABLE | I2S_MUX_CH_0_RIGHT_ENABLE |
                                        I2S_MUX_CH_1_LEFT_ENABLE | I2S_MUX_CH_1_RIGHT_ENABLE |
                                        I2S_MUX_CH_2_LEFT_ENABLE | I2S_MUX_CH_2_RIGHT_ENABLE , HDMI_SS_I2S_MUX_CH);

                                        reg = ~(I2S_MUX_CUV_LEFT_ENABLE| I2S_MUX_CUV_RIGHT_ENABLE);
                                        
                                        // enable CUV from right and left channel
                                        hdmi_audio_reg_write(dev, reg, HDMI_SS_I2S_MUX_CUV);

                                        // set mux control
                                        hdmi_audio_reg_write(dev, I2S_IN_MUX_ENABLE | I2S_IN_MUX_SELECT_DSD | I2S_IN_MUX_IN_ENABLE, HDMI_SS_I2S_IN_MUX_CON);
                                        ret = 0;
                                }
                                break;
                        default:
                                break;
                }
        }
        return ret;
}

static unsigned int io_ckc_get_dai_clock(unsigned int freq)
{
        unsigned int clock = freq * 256;
        
        switch (freq) {
                case 44100: 
                        clock = (44100 * 256);
                case 22000: 
                        clock = (22050 * 256);
                case 11000: 
                        clock = (11025 * 256);
                default:
                        break;
        }
        return clock;
}

static void tcc_hdmi_audio_set_clock(struct tcc_hdmi_audio_dev *dev, unsigned int output, unsigned int clock_rate)
{
        unsigned int clk_rate;
        DPRINTK(KERN_WARNING "[%s], clock_rate[%u]\n", __func__, clock_rate);

        if(dev != NULL) {
                switch(output) {
                        case I2S_PORT:
                                if(dev->pclk != NULL) {
                                        clk_disable_unprepare(dev->pclk);
                                        clk_set_rate(dev->pclk, XTIN_CLK_RATE);
                                        clk_rate = io_ckc_get_dai_clock(clock_rate) * 2;   /* set 512xfs for HDMI */
                                        tcc_ckc_set_hdmi_audio_src(PERI_ADAI0); 
                                        clk_set_rate(dev->pclk, clk_rate);
                                        clk_prepare_enable(dev->pclk);
                                        //pr_info("%s audio clock %dHz\r\n", __func__, clk_get_rate(dev->pclk));
                                }
                                break;
                                
                        case SPDIF_PORT:
                                clk_rate = io_ckc_get_dai_clock(clock_rate) * 2;   /* set 512xfs for HDMI */

                                if(dev->spidf_pclk != NULL) {
                                        clk_set_rate(dev->spidf_pclk, clk_rate);
                                }
                                
                                //To Do : SPDIF Clock Setting...
                                if(dev->pclk != NULL) {
                                        clk_disable_unprepare(dev->pclk);
                                        clk_set_rate(dev->pclk, XTIN_CLK_RATE);
                                        tcc_ckc_set_hdmi_audio_src(PERI_SPDIF3);		
                                        clk_set_rate(dev->pclk, clk_rate);
                                        clk_prepare_enable(dev->pclk);
                                        //pr_info("%s audio clock %dHz\r\n", __func__, clk_get_rate(dev->pclk));
                                }
                                break;
                }
        }
}


/**
 * Set sampling frequency in I2S receiver.
 *
 * @param   freq    [in]   Sampling frequency.
 * @return  If argument is invalid, return 0;Otherwise, return 1.
 */
static int setCUVSampleFreq(struct tcc_hdmi_audio_dev *dev, enum SamplingFreq freq)
{
        int ret = -1;
        unsigned int reg;
        unsigned int audio_freq = 0;
        DPRINTK(KERN_INFO "%s freq:%d \n", __FUNCTION__, freq);

        if(dev != NULL) {
                reg = hdmi_audio_reg_read(dev, HDMI_SS_I2S_CH_ST_3);
                reg &= ~I2S_CH_ST_3_SF_MASK;

                switch (freq){
                        case SF_32KHZ:
                                reg |= I2S_CH_ST_3_SF_32KHZ;
                                audio_freq = 32000;
                                break;
                        case SF_44KHZ:
                                reg |= I2S_CH_ST_3_SF_44KHZ;
                                audio_freq = 44100;
                                break;
                        case SF_88KHZ:
                                reg |= I2S_CH_ST_3_SF_88KHZ;
                                audio_freq = 88200;
                                break;
                        case SF_176KHZ:
                                reg |= I2S_CH_ST_3_SF_176KHZ;
                                audio_freq = 176000;
                                break;
                        case SF_48KHZ:
                                reg |= I2S_CH_ST_3_SF_48KHZ;
                                audio_freq = 48000;
                                break;
                        case SF_96KHZ:
                                reg |= I2S_CH_ST_3_SF_96KHZ;
                                audio_freq = 96000;
                                break;
                        case SF_192KHZ:
                                reg |= I2S_CH_ST_3_SF_192KHZ;
                                audio_freq = 192000;
                                break;
                        default:
                                break;
                }

                if( audio_freq > 0 ) {
                        tcc_hdmi_audio_set_clock(dev, I2S_PORT, audio_freq);
                        msleep(10);
                        hdmi_audio_reg_write(dev, reg, HDMI_SS_I2S_CH_ST_3);
                        ret = 0;
                }
        }
        return ret;
}

/**
 * Set sampling frequency in SPDIF receiver.
 *
 * @param   freq    [in]   Sampling frequency.
 * @return  If argument is invalid, return 0;Otherwise, return 1.
 */
static int setSPDIFSampleFreq(struct tcc_hdmi_audio_dev *dev, enum SamplingFreq freq)
{
        int ret = 1;
        unsigned int audio_freq = 0;

        DPRINTK(KERN_INFO "%s freq:%d \n", __FUNCTION__, freq);

        switch (freq) {
                case SF_32KHZ:
                        audio_freq = 32000;
                        break;
                case SF_44KHZ:
                        audio_freq = 44100;
                        break;
                case SF_88KHZ:
                        audio_freq = 88200;
                        break;
                case SF_176KHZ:
                        audio_freq = 176000;
                        break;
                case SF_48KHZ:
                        audio_freq = 48000;
                        break;
                case SF_96KHZ:
                        audio_freq = 96000;
                        break;
                case SF_192KHZ:
                        audio_freq = 192000;
                        break;
                default:
                        break;
        }

        if( audio_freq > 0 && dev != NULL ) {
                tcc_hdmi_audio_set_clock(dev, SPDIF_PORT, audio_freq);
                ret = 0;
        }

        return ret;
}

/**
 * Set coding type in I2S receiver.
 *
 * @param   coding    [in]   Coding type.
 * @return  If argument is invalid, return 0;Otherwise, return 1.
 */
static int setCUVCodingType(struct tcc_hdmi_audio_dev *dev, enum CUVAudioCoding coding)
{
        int ret = -1;
        unsigned int reg;

        if(dev != NULL) {
                reg = hdmi_audio_reg_read(dev, HDMI_SS_I2S_CH_ST_0) & ~I2S_CH_ST_0_TYPE_MASK;

                switch (coding)
                {
                        case CUV_LPCM:
                                reg |= I2S_CH_ST_0_TYPE_LPCM;
                                ret = 0;
                                break;

                        case CUV_NLPCM:
                                reg |= I2S_CH_ST_0_TYPE_NLPCM;
                                ret = 0;
                                break;

                        default:
                                break;
                };
                if(ret < 0) {       
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
                        hdmi_audio_reg_write(dev, reg, HDMI_SS_I2S_CH_ST_0);
                }
        }

        return ret;
}

/**
 * Set the number of channels in I2S receiver.
 *
 * @param   num     [in]   Number of channels.
 * @return  If argument is invalid, return 0;Otherwise, return 1.
 */
static int setCUVChannelNum(struct tcc_hdmi_audio_dev *dev, enum CUVChannelNumber num)
{
    int ret = -1;
    unsigned int reg;

    if(dev != NULL) {
                reg = hdmi_audio_reg_read(dev, HDMI_SS_I2S_CH_ST_2);
                reg &= ~I2S_CH_ST_2_CHANNEL_MASK;

                switch (num){
                        case CUV_CH_UNDEFINED:
                                reg |= I2S_CH_ST_2_CH_UNDEFINED;
                                ret = 0;
                                break;

                        case CUV_CH_01:
                                reg |= I2S_CH_ST_2_CH_01;
                                ret = 0;
                                break;

                        case CUV_CH_02:
                                reg |= I2S_CH_ST_2_CH_02;
                                ret = 0;
                                break;

                        case CUV_CH_03:
                                reg |= I2S_CH_ST_2_CH_03;
                                ret = 0;
                                break;

                        case CUV_CH_04:
                                reg |= I2S_CH_ST_2_CH_04;
                                ret = 0;
                                break;

                        case CUV_CH_05:
                                reg |= I2S_CH_ST_2_CH_05;
                                ret = 0;
                                break;

                        case CUV_CH_06:
                                reg |= I2S_CH_ST_2_CH_06;
                                ret = 0;
                                break;

                        case CUV_CH_07:
                                reg |= I2S_CH_ST_2_CH_07;
                                ret = 0;
                                break;

                        case CUV_CH_08:
                                reg |= I2S_CH_ST_2_CH_08;
                                ret = 0;
                                break;

                        case CUV_CH_09:
                                reg |= I2S_CH_ST_2_CH_09;
                                ret = 0;
                                break;

                        case CUV_CH_10:
                                reg |= I2S_CH_ST_2_CH_10;
                                ret = 0;
                                break;

                        case CUV_CH_11:
                                reg |= I2S_CH_ST_2_CH_11;
                                ret = 0;
                                break;

                        case CUV_CH_12:
                                reg |= I2S_CH_ST_2_CH_12;
                                ret = 0;
                                break;

                        case CUV_CH_13:
                                reg |= I2S_CH_ST_2_CH_13;
                                ret = 0;
                                break;

                        case CUV_CH_14:
                                reg |= I2S_CH_ST_2_CH_14;
                                ret = 0;
                                break;

                        case CUV_CH_15:
                                reg |= I2S_CH_ST_2_CH_15;
                                ret = 0;
                                break;

                        default:
                                break;
                }
                if(ret < 0) {       
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
                        hdmi_audio_reg_write(dev, reg, HDMI_SS_I2S_CH_ST_2);
                }
        }
        return ret;
}

/**
 * Set word length in I2S receiver.
 *
 * @param   length    [in]   Word length.
 * @return  If argument is invalid, return 0;Otherwise, return 1.
 */
static int setCUVWordLength(struct tcc_hdmi_audio_dev *dev, enum CUVWordLength length)
{
        int ret = -1;
        unsigned int reg;

        if(dev != NULL) {
                reg = hdmi_audio_reg_read(dev, HDMI_SS_I2S_CH_ST_4);
                reg &= ~I2S_CH_ST_4_WL_MASK;

                switch (length) {
                        case CUV_WL_20_NOT_DEFINED:
                                reg |= I2S_CH_ST_4_WL_20_NOT_DEFINED;
                                ret = 0;
                                break;

                        case CUV_WL_20_16:
                                reg |= I2S_CH_ST_4_WL_20_16;
                                ret = 0;
                                break;

                        case CUV_WL_20_18:
                                reg |= I2S_CH_ST_4_WL_20_18;
                                ret = 0;
                                break;

                        case CUV_WL_20_19:
                                reg |= I2S_CH_ST_4_WL_20_19;
                                ret = 0;
                                break;

                        case CUV_WL_20_20:
                                reg |= I2S_CH_ST_4_WL_20_20;
                                ret = 0;
                                break;

                        case CUV_WL_20_17:
                                reg |= I2S_CH_ST_4_WL_20_17;
                                ret = 0;
                                break;

                        case CUV_WL_24_NOT_DEFINED:
                                reg |= I2S_CH_ST_4_WL_24_NOT_DEFINED;
                                ret = 0;
                                break;

                        case CUV_WL_24_20:
                                reg |= I2S_CH_ST_4_WL_24_20;
                                ret = 0;
                                break;

                        case CUV_WL_24_22:
                                reg |= I2S_CH_ST_4_WL_24_22;
                                ret = 0;
                                break;

                        case CUV_WL_24_23:
                                reg |= I2S_CH_ST_4_WL_24_23;
                                ret = 0;
                                break;

                        case CUV_WL_24_24:
                                reg |= I2S_CH_ST_4_WL_24_24;
                                ret = 0;
                                break;

                        case CUV_WL_24_21:
                                reg |= I2S_CH_ST_4_WL_24_21;
                                ret = 0;
                                break;

                        default:
                                break;
                }
                if(ret < 0) {       
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                } else {
                        hdmi_audio_reg_write(dev, reg, HDMI_SS_I2S_CH_ST_4);
                }
        }

        return ret;
}

/**
 * Set I2S audio paramters in I2S receiver.
 *
 * @param   i2s     [in]   I2S audio paramters.
 * @return  If argument is invalid, return 0;Otherwise, return 1.
 */
static int setI2SParameter(struct tcc_hdmi_audio_dev *dev, struct I2SParameter i2s)
{
        int ret = -1;   
        unsigned int reg;

        do {
                // bit per channel
                switch(i2s.bpc) {
                        case I2S_BPC_16:
                                reg = I2S_CON_DATA_NUM_16;
                                ret = 0;
                                break;

                        case I2S_BPC_20:
                                reg = I2S_CON_DATA_NUM_20;
                                ret = 0;
                                break;

                        case I2S_BPC_24:
                                reg = I2S_CON_DATA_NUM_24;
                                ret = 0;
                                break;
                        default:
                                break;
                }
                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                // LR clock
                switch(i2s.clk) {
                        case I2S_32FS:
                                reg = (I2S_CON_BIT_CH_32 | I2S_CON_DATA_NUM_16);
                                ret = 0;
                                break;

                        case I2S_48FS:
                                reg |= I2S_CON_BIT_CH_48;
                                ret = 0;
                                break;

                        case I2S_64FS:
                                reg |= I2S_CON_BIT_CH_64;
                                ret = 0;
                                break;
                        default:
                                ret = -1;
                                break;
                }
                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                // format
                switch(i2s.format) {
                        case I2S_BASIC:
                                reg |= I2S_CON_I2S_MODE_BASIC;
                                ret = 0;
                                break;

                        case I2S_LEFT_JUSTIFIED:
                                reg |= I2S_CON_I2S_MODE_LEFT_JUSTIFIED;
                                ret = 0;
                                break;

                        case I2S_RIGHT_JUSTIFIED:
                                reg |= I2S_CON_I2S_MODE_RIGHT_JUSTIFIED;
                                ret = 0;
                                break;
                        default:
                                ret = -1;
                                break;
                }
                if(ret < 0) {
                        pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                        break;
                }
                hdmi_audio_reg_write(dev, reg , HDMI_SS_I2S_CON_2);
        }while(0);

    return ret;

}

static int audio_open(struct inode *inode, struct file *file)
{
        int ret = -1;
        struct miscdevice *misc = (struct miscdevice *)(file!=NULL)?file->private_data:NULL;
        struct tcc_hdmi_audio_dev *dev = (struct tcc_hdmi_audio_dev *)(misc!=NULL)?dev_get_drvdata(misc->parent):NULL;

        if(dev != NULL) {
                file->private_data = dev;  
                if(++dev->open_cnt == 1) {
                        pr_info("%s clock enable\r\n", __func__);
                        if(dev->pclk != NULL)
                                clk_prepare_enable(dev->pclk);
                        if(dev->hclk != NULL)
                                clk_prepare_enable(dev->hclk);
                }
                pr_info("%s (%d)\n", __func__, dev->open_cnt);
                ret = 0;
        }
        return ret;
}


static int audio_release(struct inode *inode, struct file *file)
{
        int ret = -1;
        struct tcc_hdmi_audio_dev *dev = (struct tcc_hdmi_audio_dev *)(file!=NULL)?file->private_data:NULL;

        if(dev != NULL) {
                file->private_data = dev; 

                if(dev->open_cnt > 0) {
                        if(--dev->open_cnt == 0) {
                                pr_info("%s clock disable\r\n", __func__);
                                if(dev->pclk != NULL)
                                        clk_disable_unprepare(dev->pclk);
                                if(dev->hclk != NULL)
                                        clk_disable_unprepare(dev->hclk);
                        }
                }
                pr_info("%s (%d)\n", __func__, dev->open_cnt);
                ret = 0;
        }         
        return ret;
}

static ssize_t audio_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

static ssize_t audio_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

static long  audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        long ret = -EINVAL;
        struct tcc_hdmi_audio_dev *dev = NULL;  
        
        if(file != NULL) {
                dev = (struct tcc_hdmi_audio_dev *)file->private_data;
        }
        
        if(dev != NULL) {
                switch (cmd) {
                        case AUDIO_IOC_SET_AUDIOINPUT:
                                {
                                        int val;
                                        io_debug("AUDIO: ioctl(AUDIO_IOC_SET_AUDIOINPUT)\n");

                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        if (setAudioInputPort(dev, val) < 0) {
                                                pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        dev->audio_params.inputPort = val;
                                        ret = 0;
                        }
                        break;
                        
                        case AUDIO_IOC_GET_AUDIOINPUT:
                                {
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_GET_AUDIOINPUT)\n");
                                        if(copy_to_user((void __user *)arg, &dev->audio_params.inputPort, sizeof(enum HDMIAudioPort))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case AUDIO_IOC_SET_I2S_CUV_SET_SAMPLEFREQ:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_I2S_CUV_SET_SAMPLEFREQ)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        if (setCUVSampleFreq(dev, val) < 0) {
                                                pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        dev->audio_params.sampleFreq = val;
                                        ret = 0;
                                }
                                break;
                                
                        case AUDIO_IOC_GET_I2S_CUV_SET_SAMPLEFREQ:
                                {
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_GET_I2S_CUV_SET_SAMPLEFREQ)\n");
                                        if(copy_to_user((void __user *)arg, &dev->audio_params.sampleFreq, sizeof(enum SamplingFreq ))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case AUDIO_IOC_SET_I2S_CUV_SET_CODINGTYPE:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_I2S_CUV_SET_CODINGTYPE)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        if (setCUVCodingType(dev, val) < 0) {
                                                pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        dev->coding_type = val;
                                        ret = 0;
                                }
                                break;
                                
                        case AUDIO_IOC_GET_I2S_CUV_SET_CODINGTYPE:
                                {
                                        //          int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_GET_I2S_CUV_SET_CODINGTYPE)\n");

                                        if(copy_to_user((void __user *)arg, &dev->coding_type, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;


                        case AUDIO_IOC_SET_AUDIOFORMATCODE_INFO:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_AUDIOFORMATCODE_INFO)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        dev->audio_params.formatCode = val;
                                        ret = 0;
                                }
                                break;

                        case AUDIO_IOC_GET_AUDIOFORMATCODE_INFO:
                                {
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_GET_AUDIOFORMATCODE_INFO)\n");
                                        if(copy_to_user((void __user *)arg, &dev->audio_params.formatCode, sizeof(enum AudioFormat))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;

                                }
                                break;

                        case AUDIO_IOC_SET_I2S_CUV_SET_CHANNELNUMBER:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_I2S_CUV_SET_CHANNELNUMBER)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        
                                        if (setCUVChannelNum(dev, val) < 0) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        dev->audio_params.channelNum = val;
                                        ret = 0;
                                }
                                break;
                                
                        case AUDIO_IOC_GET_I2S_CUV_SET_CHANNELNUMBER:
                                {
                                        //          int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_GET_I2S_CUV_SET_CHANNELNUMBER)\n");
                                        if(copy_to_user((void __user *)arg, &dev->audio_params.channelNum, sizeof(enum ChannelNum))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case AUDIO_IOC_SET_I2S_CUV_SET_WORDLENGTH:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_I2S_CUV_SET_WORDLENGTH)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        
                                        if (setCUVWordLength(dev, val) < 0) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case AUDIO_IOC_SET_I2S_CUV_SET_WORDLENGTH_INFO:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_I2S_CUV_SET_WORDLENGTH)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        dev->lpcm_word_length = val;
                                        ret = 0;      
                                }
                                break;
                                
                        case AUDIO_IOC_GET_I2S_CUV_SET_WORDLENGTH_INFO:
                                {
                                        //          int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_GET_I2S_CUV_SET_WORDLENGTH)\n");
                                        if(copy_to_user((void __user *)arg, &dev->lpcm_word_length, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;

                        case AUDIO_IOC_SET_SPDIF_SET_CODINGTYPE:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_SPDIF_SET_CODINGTYPE)\n");

                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        switch(val) {
                                                case SPDIF_NLPCM:
                                                        dev->spdif_struct.codingtype = SPDIF_NLPCM;
                                                        ret = 0;
                                                        break;
                                                case SPDIF_LPCM:
                                                        dev->spdif_struct.codingtype = SPDIF_LPCM;
                                                        ret = 0;
                                                        break;
                                                default:
                                                        dev->spdif_struct.codingtype = -1;
                                                        break;
                                        }
                                }
                                break;
                                
                        case AUDIO_IOC_SET_SPDIF_SET_SAMPLEFREQ:
                                {
                                        int val;
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_SPDIF_SET_SAMPLEFREQ)\n");
                                        if(copy_from_user(&val, (void __user *)arg, sizeof(int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                       

                                        if (setSPDIFSampleFreq(dev, val) < 0) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;
                                
                        case AUDIO_IOC_SET_I2S_PARAMETER:
                                {
                                        struct I2SParameter i2s;
                                        
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_SET_I2S_PARAMETER)\n");
                                        if(copy_from_user(&i2s, (void __user *)arg, sizeof(struct I2SParameter))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        
                                        ret = setI2SParameter(dev, i2s);
                                        if(ret < 0) {
                                                pr_err("%s invalid param at line(%d)\r\n", __func__, __LINE__);
                                        } else {
                                                memcpy(&dev->audio_params.i2sParam, &i2s, sizeof(struct I2SParameter));
                                        }
                                }
                                break;

                        case AUDIO_IOC_GET_I2S_PARAMETER:
                                {
                                        //          struct I2SParameter i2s;

                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_GET_I2S_PARAMETER)\n");
                                        if(copy_to_user((void __user *)arg, &dev->audio_params.i2sParam, sizeof(struct I2SParameter))) {
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
                                }
                                break;
                                

                        case AUDIO_IOC_UPDATE_I2S_CUV:
                                {
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_UPDATE_I2S_CUV)\n");
                                        hdmi_audio_reg_write(dev, 0x01, HDMI_SS_I2S_CH_ST_CON);
                                        ret = 0;
                                }
                                break;
                                
                        case AUDIO_IOC_RESET_I2S_CUV:
                                {
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_RESET_I2S_CUV)\n");

                                        hdmi_audio_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_0);
                                        hdmi_audio_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_1);
                                        hdmi_audio_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_2);
                                        hdmi_audio_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_3);
                                        hdmi_audio_reg_write(dev, 0x00, HDMI_SS_I2S_CH_ST_4);
                                        // update CUV
                                        hdmi_audio_reg_write(dev, 0x01, HDMI_SS_I2S_CH_ST_CON);
                                        ret = 0;
                                }
                                break;
                                
                        case AUDIO_IOC_ENABLE_I2S_CLK_CON:
                                {
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_ENABLE_I2S_CLK_CON)\n");

                                        // disable audio
                                        hdmi_audio_reg_write(dev, I2S_CLK_CON_ENABLE,HDMI_SS_I2S_CLK_CON);
                                        ret = 0;
                                }               
                                break;

                        case AUDIO_IOC_DISABLE_I2S_CLK_CON:
                                {
                                        io_debug( "AUDIO: ioctl(AUDIO_IOC_DISABLE_I2S_CLK_CON)\n");

                                        // disable audio
                                        hdmi_audio_reg_write(dev, I2S_CLK_CON_DISABLE,HDMI_SS_I2S_CLK_CON);
                                        ret = 0;
                                }
                                break;
                                
                        default:
                            break;
                }
        }
        return ret;
}

static const struct file_operations audio_fops =
{
    .owner          = THIS_MODULE,
    .open           = audio_open,
    .release        = audio_release,
    .read           = audio_read,
    .write          = audio_write,
    .unlocked_ioctl = audio_ioctl,
};

#ifdef CONFIG_PM
static int hdmi_audio_suspend(struct device *dev)
{
        struct tcc_hdmi_audio_dev *tcc_hdmi_audio_dev = (struct tcc_hdmi_audio_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
                
        if(tcc_hdmi_audio_dev != NULL) {    
                if(tcc_hdmi_audio_dev->runtime_suspend) {
                        pr_info("hdmi audio runtime suspend\r\n");
                } else {
                        pr_info("hdmi audio suspend\r\n");
                }
                if(!tcc_hdmi_audio_dev->suspend) {
                        tcc_hdmi_audio_dev->suspend = 1 ; 
                }                
        }        
        return 0;
}

static int hdmi_audio_resume(struct device *dev)
{
        struct tcc_hdmi_audio_dev *tcc_hdmi_audio_dev = (struct tcc_hdmi_audio_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
                
        if(tcc_hdmi_audio_dev != NULL) {    
                if(tcc_hdmi_audio_dev->runtime_suspend) {
                        pr_info("hdmi audio runtime suspend\r\n");
                } else {
                        pr_info("hdmi audio suspend\r\n");
                }
                if(tcc_hdmi_audio_dev->suspend  && !tcc_hdmi_audio_dev->runtime_suspend) {
                        tcc_hdmi_audio_dev->suspend = 0; 
                }
        }
        return 0;
}

static int hdmi_audio_runtime_suspend(struct device *dev)
{
        struct tcc_hdmi_audio_dev *hdmi_tx_dev = (struct tcc_hdmi_audio_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        if(hdmi_tx_dev != NULL) {
                hdmi_tx_dev->runtime_suspend = 0;
                hdmi_audio_resume(dev);
        }
        return 0;
}

static int hdmi_audio_runtime_resume(struct device *dev)
{
        struct tcc_hdmi_audio_dev *hdmi_tx_dev = (struct tcc_hdmi_audio_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        if(hdmi_tx_dev != NULL) {
                hdmi_tx_dev->runtime_suspend = 1;
                hdmi_audio_suspend(dev);
        }
        return 0;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops hdmi_audio_pm_ops = {
        .runtime_suspend = hdmi_audio_runtime_suspend,
        .runtime_resume = hdmi_audio_runtime_resume,
        .suspend = hdmi_audio_suspend,
        .resume = hdmi_audio_resume,
};
#endif

static int audio_remove(struct platform_device *pdev)
{
        unsigned int reg;
        struct tcc_hdmi_audio_dev *dev = NULL;
                                
        if(pdev != NULL) {
                dev = (struct tcc_hdmi_audio_dev *)dev_get_drvdata(pdev->dev.parent);

                if(dev != NULL) {
                        // disable SPDIF INT
                        reg = hdmi_audio_reg_read(dev, HDMI_SS_INTC_CON);
                        hdmi_audio_reg_write(dev, reg & ~(1<<HDMI_IRQ_SPDIF), HDMI_SS_INTC_CON);
                        devm_free_irq(dev->pdev, dev->audio_irq, &dev);
                        if(dev->misc != NULL) {
                                misc_deregister(dev->misc);
                                devm_kfree(dev->pdev, dev->misc);
                        }
                        devm_kfree(dev->pdev, dev);
                }
        }

        return 0;
}

static int audio_probe(struct platform_device *pdev)
{
        int ret =  -ENODEV;
        
        struct device_node *np;
        
        struct tcc_hdmi_audio_dev *dev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hdmi_audio_dev), GFP_KERNEL);
        do {
                if (dev == NULL) {
                	ret = -ENOMEM;
                	break;
                }
                dev->pdev = &pdev->dev;
                
                dev->hclk = of_clk_get(pdev->dev.of_node, 1);
                dev->pclk = of_clk_get(pdev->dev.of_node, 0);

                if (IS_ERR_OR_NULL(dev->hclk)) {
                        pr_err("HDMI: failed to get hdmi hclk\n");
                        dev->hclk = NULL;
                        break;
                }

                if (IS_ERR_OR_NULL(dev->pclk)) {
                        pr_err("HDMI: failed to get hdmi pclk\n");
                        dev->pclk = NULL;
                        break;
                }

                np = of_parse_phandle(pdev->dev.of_node,"spdif-block", 0);
                if (np) 
                {
                        dev->spidf_pclk = of_clk_get(np, 0);
                        if (IS_ERR_OR_NULL(dev->spidf_pclk)){
                                pr_err("HDMI: failed to get hdmi spdif pclk\n");
                                dev->spidf_pclk  = NULL;
                                break;
                        }
                }

                if(dev->pclk  != NULL)
                        clk_prepare_enable(dev->pclk);

                if(dev->hclk != NULL)
                        clk_prepare_enable(dev->hclk);

                dev->hdmi_ctrl_io = of_iomap(pdev->dev.of_node, 0);
                if(dev->hdmi_ctrl_io == NULL){
                        pr_err("%s:Unable to map hdmi ctrl resource at line(%d)\n", __func__, __LINE__);
                        break;
                }
                dev->hdmi_spdif_io = of_iomap(pdev->dev.of_node, 1);
                if(dev->hdmi_spdif_io == NULL){
                        pr_err("%s:Unable to map hdmi ctrl resource at line(%d)\n", __func__, __LINE__);
                        break;
                }
                dev->hdmi_i2s_io = of_iomap(pdev->dev.of_node, 2);
                if(dev->hdmi_i2s_io == NULL){
                        pr_err("%s:Unable to map hdmi ctrl resource at line(%d)\n", __func__, __LINE__);
                        break;
                }

                dev->audio_irq = of_irq_to_resource(pdev->dev.of_node, 0, NULL);
                if (dev->audio_irq < 0) {
                        pr_err("%s failed get irq for hdmi audio\r\n", __func__);
                        break;
                }

                dev->misc = devm_kzalloc(&pdev->dev, sizeof(struct miscdevice), GFP_KERNEL);
                if(dev->misc == NULL) {
                        pr_err("%s:Unable to createe hdmi misc at line(%d)\n", __func__, __LINE__);
                        ret = -ENOMEM;
                        break;
                }
                dev->misc->minor = MISC_DYNAMIC_MINOR;
                dev->misc->name = "audio";
                dev->misc->fops = &audio_fops;
                dev->misc->parent = dev->pdev;
                ret = misc_register(dev->misc);

                if(ret < 0) {
                        pr_err("%s failed misc_register for hdmi audio\r\n", __func__);
                        break;
                }
                pr_info("%s:HDMI Audio driver %s\n", __func__, SRC_VERSION);
                
                dev_set_drvdata(dev->pdev, dev);

                dev->spdif_struct.state = -1;
                dev->spdif_struct.codingtype = -1;

                ret = devm_request_irq(dev->pdev, dev->audio_irq, audio_spdif_handler, IRQF_SHARED, "hdmi-spdif", dev);
                if(ret < 0) {
                        pr_err("%s failed request interrupt for hotplug\r\n", __func__);
                }

                if(dev->pclk != NULL) {
                        clk_disable_unprepare(dev->pclk);
                }

                if(dev->hclk != NULL) {
                        clk_disable_unprepare(dev->hclk);
                }
        } while(0);

        if(ret < 0) {
                audio_remove(pdev);
        }
        
        return ret;
}



static struct of_device_id audio_of_match[] = {
	{ .compatible = "telechips,tcc897x-hdmi-audio" },
	{}
};
MODULE_DEVICE_TABLE(of, audio_of_match);

static struct platform_driver tcc_hdmi_audio = {
	.probe	= audio_probe,
	.remove	= audio_remove,
	.driver	= {
		.name	= "tcc_hdmi_audio",
		.owner	= THIS_MODULE,
		#ifdef CONFIG_PM
		.pm = &hdmi_audio_pm_ops,
                #endif
		.of_match_table = of_match_ptr(audio_of_match),
        },
};

static __init int audio_init(void)
{
        return platform_driver_register(&tcc_hdmi_audio);
}

static __exit void audio_exit(void)
{
        platform_driver_unregister(&tcc_hdmi_audio);
}

module_init(audio_init);
module_exit(audio_exit);
MODULE_DESCRIPTION("TCCxxx hdmi audio driver");
MODULE_LICENSE("GPL");

