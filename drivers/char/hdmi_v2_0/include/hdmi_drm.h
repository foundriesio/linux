/****************************************************************************
hdmi_drm.h

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef __HDMI_DRM_H__
#define __HDMI_DRM_H__

#define DRM_BIT_OFFSET	8
int hdmi_clear_drm(struct hdmi_tx_dev *dev);
int hdmi_apply_drm(struct hdmi_tx_dev *dev);
int hdmi_update_drm_configure(struct hdmi_tx_dev *dev, DRM_Packet_t * drmparm);

#endif
