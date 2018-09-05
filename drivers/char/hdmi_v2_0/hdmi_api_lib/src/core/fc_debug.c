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

#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <include/hdmi_ioctls.h>

#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <core/fc_debug.h>


static void fc_force_audio(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_DBGFORCE, FC_DBGFORCE_FORCEAUDIO_MASK, bit);
}

void fc_force_video(struct hdmi_tx_dev *dev, u8 bit)
{
        unsigned int dbg_val[3];
        videoParams_t * videoParams;
        do {
                if(dev == NULL) {
                        break;
                }
                videoParams = dev->videoParam;

                if(videoParams == NULL) {
                        break;
                }

                if(bit) {
                        /* Fill Black */
                        switch(videoParams->mEncodingOut) {
                                case RGB:
                                        dbg_val[0] = 0x00; /* B or Cb */
                                        dbg_val[1] = 0x00; /* G or Y */
                                        dbg_val[2] = 0x00; /* R or Cr */
                                        //pr_info("\r\n%s RGB\r\n", __func__);
                                default:
                                        break;
                                case YCC444:
                                case YCC422:
                                        dbg_val[0] = 0x80; /* B or Cb */
                                        dbg_val[1] = 0x00; /* G or Y */
                                        dbg_val[2] = 0x80; /* R or Cr */
                                        //pr_info("\r\n%s YCC444 or YCC422\r\n", __func__);
                                        break;
                                case YCC420:
                                        dbg_val[0] = 0x80; /* B or Cb */
                                        dbg_val[1] = 0x00; /* G or Y */
                                        dbg_val[2] = 0x00; /* R or Cr */
                                        //pr_info("\r\n%s YCC420\r\n", __func__);
                                        break;
                        }
                }
                hdmi_dev_write(dev, FC_DBGTMDS0, dbg_val[0]);
                hdmi_dev_write(dev, FC_DBGTMDS1, dbg_val[1]);
                hdmi_dev_write(dev, FC_DBGTMDS2, dbg_val[2]);
        	hdmi_dev_write_mask(dev, FC_DBGFORCE, FC_DBGFORCE_FORCEVIDEO_MASK, (bit!=0)?1:0);
        }while(0);
}

void fc_force_output(struct hdmi_tx_dev *dev, int enable)
{
	fc_force_audio(dev, 0);
	fc_force_video(dev, (u8)enable);
}
