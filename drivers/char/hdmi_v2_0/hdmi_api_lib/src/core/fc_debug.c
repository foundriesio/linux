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

#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <core/fc_debug.h>

#if 0
static void fc_force_audio(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_DBGFORCE, FC_DBGFORCE_FORCEAUDIO_MASK, bit);
}

static void fc_force_video(struct hdmi_tx_dev *dev, u8 bit)
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
                        /* Fill Black with 8-bit only */
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
#endif

void fc_force_output(struct hdmi_tx_dev *dev, int enable)
{
        /* Nothing To Do */
}

