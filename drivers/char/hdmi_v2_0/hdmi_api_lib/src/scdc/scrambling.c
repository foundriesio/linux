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

