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
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <include/hdmi_ioctls.h>
#include <scdc/scdc.h>
#include <scdc/scrambling.h>
#include <core/fc_video.h>
#include <core/main_controller.h>

#define SCRAMBLING_WAIT_TIME_MS 1500

/*!
 * @brief               scrambling.
 * @details             This function is used to control hdmi scramble
 * @date                2016-10-06
 * @version             1.0
 * @author              Daoying Jin
 * @param[in]           dev     hdmi driver handle.
 * @param[in]           enable  turn on/off to hdmi scramble.
                                If enable is set "0" then hdmi link disable scramble.
                                If enable is set "1" then hdmi link enable scramble.
 * @return              none
 */
void scrambling(struct hdmi_tx_dev *dev, u8 enable){
	if (enable == 1) {
		/* Enable/Disable Scrambling */
		scrambling_Enable(dev, 1);
	} else {
		/* Enable/Disable Scrambling */
		scrambling_Enable(dev, 0);
	}
}

/*!
 * @brief               scrambling_Enable.
 * @details             This function is used to control transper scrambled video.
 * @date                2016-10-06
 * @version             1.0
 * @author              Daoying Jin
 * @param[in]           dev     hdmi driver handle.
 * @param[in]           bit     turn on/off to transper scrambled video.
                                If bit is set "0" then hdmi link transper normal video.
                                If bit is set "1" then hdmi link transper scrambled video.
 * @return              none
 */
void scrambling_Enable(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, FC_SCRAMBLER_CTRL, FC_SCRAMBLER_CTRL_SCRAMBLER_ON_MASK, bit);
}

/*!
 * @brief               scrambling_get_wait_time
 * @details             This function is used to get wait time before check scramble status of sink.
 * @date                2016-10-06
 * @version             1.0
 * @author              Daoying Jin
 * @return              Return the wait time ms before check scramble status of sink.
 * @retval              0 is no wait
 */
int scrambling_get_wait_time(void)
{
        return SCRAMBLING_WAIT_TIME_MS;
}

int is_scrambling_enabled(struct hdmi_tx_dev *dev)
{
        return hdmi_dev_read_mask(dev, FC_SCRAMBLER_CTRL, FC_SCRAMBLER_CTRL_SCRAMBLER_ON_MASK);
}

