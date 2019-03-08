// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <core/video_sampler.h>
#include <hdmi_api_lib/src/core/video/video_sampler_reg.h>


static void halVideoSampler_InternalDataEnableGenerator(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, TX_INVID0, TX_INVID0_INTERNAL_DE_GENERATOR_MASK, (bit ? 1 : 0));
}

static void halVideoSampler_VideoMapping(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, TX_INVID0, TX_INVID0_VIDEO_MAPPING_MASK, value);
}

static void halVideoSampler_StuffingGy(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write(dev, (TX_GYDATA0), (u8) (value >> 0));
	hdmi_dev_write(dev, (TX_GYDATA1), (u8) (value >> 8));
	hdmi_dev_write_mask(dev, TX_INSTUFFING, TX_INSTUFFING_GYDATA_STUFFING_MASK, 1);
}

static void halVideoSampler_StuffingRcr(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write(dev, (TX_RCRDATA0), (u8) (value >> 0));
	hdmi_dev_write(dev, (TX_RCRDATA1), (u8) (value >> 8));
	hdmi_dev_write_mask(dev, TX_INSTUFFING, TX_INSTUFFING_RCRDATA_STUFFING_MASK, 1);
}

static void halVideoSampler_StuffingBcb(struct hdmi_tx_dev *dev, u16 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write(dev, (TX_BCBDATA0), (u8) (value >> 0));
	hdmi_dev_write(dev, (TX_BCBDATA1), (u8) (value >> 8));
	hdmi_dev_write_mask(dev, TX_INSTUFFING, TX_INSTUFFING_BCBDATA_STUFFING_MASK, 1);
}

void video_sampler_config(struct hdmi_tx_dev *dev, u8 map_code)
{
	halVideoSampler_InternalDataEnableGenerator(dev, 0);
	halVideoSampler_VideoMapping(dev, map_code);
	halVideoSampler_StuffingGy(dev, 0);
	halVideoSampler_StuffingRcr(dev, 0);
	halVideoSampler_StuffingBcb(dev, 0);
}
