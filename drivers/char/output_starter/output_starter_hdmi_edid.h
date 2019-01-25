/****************************************************************************
output_starter

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef __HEMI_EDID_H__
#define __HEMI_EDID_H__


int edid_get_scdc_present(void);
int edid_get_lts_340mcs_scramble(void);
int edid_get_hdmi20(void);
int edid_is_sink_vizio(void);
int edid_get_optimal_settings(struct hdmi_tx_dev *dev, int *hdmi_mode, int *vic, encoding_t *encoding, int optimal_phase);

#endif /* __HEMI_EDID_H__ */
