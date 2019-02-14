/****************************************************************************
Copyright (C) 2018 Telechips Inc.

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


