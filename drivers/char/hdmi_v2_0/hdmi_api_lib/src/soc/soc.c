/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        phy.c
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
#include <include/hdmi_ioctls.h>
#include <soc/soc.h>

hdmi_soc_features current_soc_features = {
#if defined(CONFIG_ARCH_TCC803X)
        .max_tmds_mhz = 340,
        .support_feature_1 = 0,
#elif defined(CONFIG_ARCH_TCC898X)
        .max_tmds_mhz = 594,
        .support_feature_1 = SOC_FEATURE_DOLBYVISION | SOC_FEATURE_HDR |
                                SOC_FEATURE_30_36_BIT | SOC_FEATURE_YCBCR420,
#elif defined(CONFIG_ARCH_TCC899X)
                .max_tmds_mhz = 594,
                .support_feature_1 = SOC_FEATURE_DV_TO_DISP | SOC_FEATURE_DOLBYVISION |
                                SOC_FEATURE_HDR | SOC_FEATURE_30_36_BIT | SOC_FEATURE_YCBCR420,
#endif
};

int hdmi_get_soc_features(struct hdmi_tx_dev *dev, hdmi_soc_features *soc_features)
{
        if(dev != NULL) {
                if(soc_features != NULL) {
                        /* [03:--] : HPD interrupt model
                                                00 - gpio
                                                01 - hdi link */
                        if(dev->hotplug_irq < 0) {
                                /* soc support hdmi link version hpd */
                                current_soc_features.support_feature_1 |= SOC_FEATURE_HPD_LINK_MODE;
                        } else {
                                /* soc support gpio version hpd */
                                current_soc_features.support_feature_1 &= ~SOC_FEATURE_HPD_LINK_MODE;
                        }
                        memcpy(soc_features, &current_soc_features, sizeof(hdmi_soc_features));
                }
        }
        return 0;
}


int hdmi_get_board_features(struct hdmi_tx_dev *dev, hdmi_board_features *board_features)
{
        if(dev != NULL) {
                if(board_features != NULL) {
                        if(dev->parent_dev == NULL ||
                                of_property_read_u32(dev->parent_dev->of_node, "fixd_video_id_code", &board_features->support_feature_1) < 0) {
                                board_features->support_feature_1 = 0;
                        }
                }
        }
        return 0;
}


