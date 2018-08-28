/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 * Seung-Woo Kim <sw0312.kim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * Based on drivers/media/video/s5p-tv/hdmi_drv.c
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/hdmi.h>
#include <linux/component.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <drm/tcc_drm.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_crtc.h"

#include <linux/gpio.h>


#include <hdmi_includes.h>
#include <irq_handlers.h>
#include <proc_fs.h>
#include <hdmi_ioctls.h>
#include <hdmi_api_lib/include/core/irq.h>
#include <hdmi_api_lib/include/api/api.h>
#include <hdmi_api_lib/include/scdc/scdc.h>
#include <hdmi_api_lib/include/scdc/scrambling.h>
#include <hdmi_api_lib/include/core/main_controller.h>

#include <include/video_params.h>
#include <include/audio_params.h>


#define get_hdmi_display(dev)	platform_get_drvdata(to_platform_device(dev))
#define ctx_from_connector(c)	container_of(c, struct hdmi_tx_dev, connector)

#define HOTPLUG_DEBOUNCE_MS		1100

/* AVI header and aspect ratio */
#define HDMI_AVI_VERSION	0x02
#define HDMI_AVI_LENGTH		0x0D

/* AUI header info */
#define HDMI_AUI_VERSION	0x01
#define HDMI_AUI_LENGTH	0x0A
#define AVI_SAME_AS_PIC_ASPECT_RATIO 0x8
#define AVI_4_3_CENTER_RATIO	0x9
#define AVI_16_9_CENTER_RATIO	0xa



//Frame Composer Input Video Configuration and HDCP Keepout Register
#define FC_INVIDCONF  0x00004000
//Frame Composer Input Video HActive Pixels Register 0
#define FC_INHACTIV0  0x00004004
//Frame Composer Input Video HActive Pixels Register 1
#define FC_INHACTIV1  0x00004008
//Frame Composer Input Video HBlank Pixels Register 0
#define FC_INHBLANK0  0x0000400C
//Frame Composer Input Video HBlank Pixels Register 1
#define FC_INHBLANK1  0x00004010
//Frame Composer Input Video VActive Pixels Register 0
#define FC_INVACTIV0  0x00004014
//Frame Composer Input Video VActive Pixels Register 1
#define FC_INVACTIV1  0x00004018
//Frame Composer Input Video VBlank Pixels Register
#define FC_INVBLANK  0x0000401C
//Frame Composer Input Video HSync Front Porch Register 0
#define FC_HSYNCINDELAY0  0x00004020
//Frame Composer Input Video HSync Front Porch Register 1
#define FC_HSYNCINDELAY1  0x00004024
//Frame Composer Input Video HSync Width Register 0
#define FC_HSYNCINWIDTH0  0x00004028
//Frame Composer Input Video HSync Width Register 1
#define FC_HSYNCINWIDTH1  0x0000402C
//Frame Composer Input Video VSync Front Porch Register
#define FC_VSYNCINDELAY  0x00004030
//Frame Composer Input Video VSync Width Register
#define FC_VSYNCINWIDTH  0x00004034

//Frame Composer Pixel Repetition Configuration Register
#define FC_PRCONF  0x00004380

//Frame Composer Control Period Duration Register
#define FC_CTRLDUR  0x00004044
//Frame Composer Extended Control Period Duration Register
#define FC_EXCTRLDUR  0x00004048
//Frame Composer Extended Control Period Maximum Spacing Register
#define FC_EXCTRLSPAC  0x0000404C
//Frame Composer Channel 0 Non-Preamble Data Register
#define FC_CH0PREAM  0x00004050
//Frame Composer Channel 1 Non-Preamble Data Register
#define FC_CH1PREAM  0x00004054
//Frame Composer Channel 2 Non-Preamble Data Register
#define FC_CH2PREAM  0x00004058


// External APIs
extern struct edid *tcc_drm_get_edid(struct drm_connector *connector);
int mixer_check_mode(struct drm_display_mode *mode);

extern void dwc_hdmi_core_power_on(struct hdmi_tx_dev *hdmi_tx_dev);
extern void dwc_hdmi_core_power_off(struct hdmi_tx_dev *hdmi_tx_dev);

// Temporal
extern int hdmi_api_Configure_0(struct hdmi_tx_dev *dev, videoParams_t * video, audioParams_t * audio, productParams_t * product, hdcpParams_t * hdcp);
extern int hdmi_api_Configure_1(struct hdmi_tx_dev *dev, videoParams_t * video, audioParams_t * audio, productParams_t * product, hdcpParams_t * hdcp);

// HDMI Access API
static int _hdmi_dev_write(struct hdmi_tx_dev *hdmi_dev, uint32_t offset, uint32_t data){
        volatile void __iomem *dwc_hdmi_tx_core_io = (volatile void __iomem *)hdmi_dev->dwc_hdmi_tx_core_io;
        if(offset & 0x3){
                return -1;
        }
        iowrite32(data, (void*)(dwc_hdmi_tx_core_io + offset));
        return 0;
}

static int _hdmi_dev_read(struct hdmi_tx_dev *hdmi_dev, uint32_t offset){
        volatile void __iomem *dwc_hdmi_tx_core_io = (volatile void __iomem *)hdmi_dev->dwc_hdmi_tx_core_io;
        if(offset & 0x3){
                return -1;
        }
        return ioread32((void*)(dwc_hdmi_tx_core_io + offset));
}
#if 0
static void hdmi_regs_dump(struct hdmi_tx_dev *hdmi_tx_dev, char *prefix)
{
	// Nothing
}

static void hdmi_reg_infoframe(struct hdmi_tx_dev *hdmi_tx_dev, union hdmi_infoframe *infoframe)
{
	u32 mod;
	u32 vic;

	if(hdmi_tx_dev->videoParam.mHdmi) {
		return ;
	}

#if 0
	switch (infoframe->any.type) {
	case HDMI_INFOFRAME_TYPE_AVI:
		//Version

		hdmi_reg_writeb(hdata, HDMI_AVI_CON, HDMI_AVI_CON_EVERY_VSYNC);
		hdmi_reg_writeb(hdata, HDMI_AVI_HEADER0, infoframe->any.type);
		hdmi_reg_writeb(hdata, HDMI_AVI_HEADER1,
						infoframe->any.version);
		hdmi_reg_writeb(hdata, HDMI_AVI_HEADER2, infoframe->any.length);
		hdr_sum = infoframe->any.type + infoframe->any.version +
			infoframe->any.length;

		/* Output format zero hardcoded ,RGB YBCR selection */
		hdmi_reg_writeb(hdata, HDMI_AVI_BYTE(1), 0 << 5 |
						AVI_ACTIVE_FORMAT_VALID |
						AVI_UNDERSCANNED_DISPLAY_VALID);

		/*
		 * Set the aspect ratio as per the mode, mentioned in
		 * Table 9 AVI InfoFrame Data Byte 2 of CEA-861-D Standard
		 */
		switch (hdata->mode_conf.aspect_ratio) {
		case HDMI_PICTURE_ASPECT_4_3:
			hdmi_reg_writeb(hdata, HDMI_AVI_BYTE(2),
							hdata->mode_conf.aspect_ratio |
							AVI_4_3_CENTER_RATIO);
			break;
		case HDMI_PICTURE_ASPECT_16_9:
			hdmi_reg_writeb(hdata, HDMI_AVI_BYTE(2),
							hdata->mode_conf.aspect_ratio |
							AVI_16_9_CENTER_RATIO);
			break;
		case HDMI_PICTURE_ASPECT_NONE:
		default:
			hdmi_reg_writeb(hdata, HDMI_AVI_BYTE(2),
							hdata->mode_conf.aspect_ratio |
							AVI_SAME_AS_PIC_ASPECT_RATIO);
			break;
		}

		vic = hdata->mode_conf.cea_video_id;
		hdmi_reg_writeb(hdata, HDMI_AVI_BYTE(4), vic);

		chksum = hdmi_chksum(hdata, HDMI_AVI_BYTE(1),
							 infoframe->any.length, hdr_sum);
		DRM_DEBUG_KMS("AVI checksum = 0x%x\n", chksum);
		hdmi_reg_writeb(hdata, HDMI_AVI_CHECK_SUM, chksum);
		break;

	case HDMI_INFOFRAME_TYPE_AUDIO:
		hdmi_reg_writeb(hdata, HDMI_AUI_CON, 0x02);
		hdmi_reg_writeb(hdata, HDMI_AUI_HEADER0, infoframe->any.type);
		hdmi_reg_writeb(hdata, HDMI_AUI_HEADER1,
						infoframe->any.version);
		hdmi_reg_writeb(hdata, HDMI_AUI_HEADER2, infoframe->any.length);
		hdr_sum = infoframe->any.type + infoframe->any.version +
			infoframe->any.length;
		chksum = hdmi_chksum(hdata, HDMI_AUI_BYTE(1),
							 infoframe->any.length, hdr_sum);
		DRM_DEBUG_KMS("AUI checksum = 0x%x\n", chksum);
		hdmi_reg_writeb(hdata, HDMI_AUI_CHECK_SUM, chksum);
		break;
	default:
		break;
	}
#endif
}
#endif /* Not Used */

static enum drm_connector_status hdmi_detect(struct drm_connector *connector, bool force)
{
	struct hdmi_tx_dev *hdmi_tx_dev = ctx_from_connector(connector);

	pr_info("%s hdmi_detect hpd=%d\r\n", __func__, hdmi_tx_dev->hpd);
	return hdmi_tx_dev->hpd ? connector_status_connected :connector_status_disconnected;
}

static void hdmi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static struct drm_connector_funcs hdmi_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = hdmi_detect,
	.destroy = hdmi_connector_destroy,
};

static int hdmi_get_modes(struct drm_connector *connector)
{
	struct edid *edid;
	struct hdmi_tx_dev *hdmi_tx_dev = ctx_from_connector(connector);

	// Enable DRM_MODE_DPMS_ON
	dwc_hdmi_core_power_on(hdmi_tx_dev);
	edid = tcc_drm_get_edid(connector);
	if (!edid)
		return -ENODEV;

	hdmi_tx_dev->videoParam.mHdmi = drm_detect_hdmi_monitor(edid)?HDMI:DVI;
	DRM_DEBUG_KMS("%s : width[%d] x height[%d]\n",
				  ((hdmi_tx_dev->videoParam.mHdmi==HDMI)? "hdmi monitor" : "dvi monitor"),
				  edid->width_cm, edid->height_cm);

	drm_mode_connector_update_edid_property(connector, edid);

	return drm_add_edid_modes(connector, edid);
}

#if 0
static int hdmi_find_phy_conf(struct hdmi_tx_dev *hdmi_tx_dev, u32 pixel_clock)
{
#if 1
	return 1;
#else
	int i;

	for (i = 0; i < dev->phy_conf_count; i++)
		if (dev->phy_confs[i].pixel_clock == pixel_clock)
			return i;

	DRM_DEBUG_KMS("Could not find phy config for %d\n", pixel_clock);
	return -EINVAL;
#endif
}
#endif /* Not Used */

static int hdmi_mode_valid(struct drm_connector *connector,
						   struct drm_display_mode *mode)
{
	/*struct hdmi_tx_dev *hdmi_tx_dev = ctx_from_connector(connector);*/
	int ret = mixer_check_mode(mode);
	if (ret < 0) {
                ret = MODE_BAD;
	} else {
                ret = MODE_OK;
	}
        DRM_DEBUG_KMS(" %dx%d%s@%dHz pclk(%dKHz) %s\r\n",
                mode->hdisplay, mode->vdisplay, (mode->flags & DRM_MODE_FLAG_INTERLACE) ? "I":"P",
                mode->vrefresh, mode->clock, (ret==MODE_OK)?"MODE_SUPPORT":"");

	return ret;
}

static struct drm_encoder *hdmi_best_encoder(struct drm_connector *connector)
{
	struct hdmi_tx_dev *hdmi_tx_dev = ctx_from_connector(connector);
	DRM_DEBUG_KMS("%s\n", __FILE__);
	return hdmi_tx_dev->encoder;
}

static struct drm_connector_helper_funcs hdmi_connector_helper_funcs = {
	.get_modes = hdmi_get_modes,
	.mode_valid = hdmi_mode_valid,
	.best_encoder = hdmi_best_encoder,
};

static int hdmi_create_connector(struct tcc_drm_display *display,
								 struct drm_encoder *encoder)
{
	struct hdmi_tx_dev *hdmi_tx_dev = display->device_context;
	struct drm_connector *connector = &hdmi_tx_dev->connector;
	int ret;

	hdmi_tx_dev->encoder = encoder;
	connector->interlace_allowed = true;
	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(hdmi_tx_dev->drm_dev, connector,
							 &hdmi_connector_funcs, DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		DRM_ERROR("Failed to initialize connector with drm\n");
		return ret;
	}

	drm_connector_helper_add(connector, &hdmi_connector_helper_funcs);
	drm_connector_register(connector);
	drm_mode_connector_attach_encoder(connector, encoder);

	return 0;
}

static void hdmi_mode_fixup(struct tcc_drm_display *display,
							struct drm_connector *connector,
							const struct drm_display_mode *mode,
							struct drm_display_mode *adjusted_mode)
{
	struct drm_display_mode *m;
	int mode_ok;

	drm_mode_set_crtcinfo(adjusted_mode, CRTC_INTERLACE_HALVE_V);

	mode_ok = hdmi_mode_valid(connector, adjusted_mode);


	DRM_DEBUG_KMS("first mode_ok = %d\r\n", mode_ok);

	/* just return if user desired mode exists. */
	if (mode_ok == MODE_OK)
		return;

	/*
	 * otherwise, find the most suitable mode among modes and change it
	 * to adjusted_mode.
	 */
	list_for_each_entry(m, &connector->modes, head) {
		mode_ok = hdmi_mode_valid(connector, m);

		if (mode_ok == MODE_OK) {
			DRM_INFO("desired mode doesn't exist so\n");
			DRM_INFO("use the most suitable mode among modes.\n");

			DRM_DEBUG_KMS("Adjusted Mode: [%d]x[%d] [%d]Hz\n",
						  m->hdisplay, m->vdisplay, m->vrefresh);

			drm_mode_copy(adjusted_mode, m);
			break;
		}
	}
}

#if 0
static void hdmi_set_acr(u32 freq, u8 *acr)
{
	u32 n, cts;

	switch (freq) {
	case 32000:
		n = 4096;
		cts = 27000;
		break;
	case 44100:
		n = 6272;
		cts = 30000;
		break;
	case 88200:
		n = 12544;
		cts = 30000;
		break;
	case 176400:
		n = 25088;
		cts = 30000;
		break;
	case 48000:
		n = 6144;
		cts = 27000;
		break;
	case 96000:
		n = 12288;
		cts = 27000;
		break;
	case 192000:
		n = 24576;
		cts = 27000;
		break;
	default:
		n = 0;
		cts = 0;
		break;
	}

	acr[1] = cts >> 16;
	acr[2] = cts >> 8 & 0xff;
	acr[3] = cts & 0xff;

	acr[4] = n >> 16;
	acr[5] = n >> 8 & 0xff;
	acr[6] = n & 0xff;
}
#endif /* Not Used */

#if 0
static void hdmi_reg_acr(struct hdmi_tx_dev *hdmi_tx_dev, u8 *acr)
{
	hdmi_reg_writeb(hdata, HDMI_ACR_N0, acr[6]);
	hdmi_reg_writeb(hdata, HDMI_ACR_N1, acr[5]);
	hdmi_reg_writeb(hdata, HDMI_ACR_N2, acr[4]);
	hdmi_reg_writeb(hdata, HDMI_ACR_MCTS0, acr[3]);
	hdmi_reg_writeb(hdata, HDMI_ACR_MCTS1, acr[2]);
	hdmi_reg_writeb(hdata, HDMI_ACR_MCTS2, acr[1]);
	hdmi_reg_writeb(hdata, HDMI_ACR_CTS0, acr[3]);
	hdmi_reg_writeb(hdata, HDMI_ACR_CTS1, acr[2]);
	hdmi_reg_writeb(hdata, HDMI_ACR_CTS2, acr[1]);

	if (hdata->type == HDMI_TYPE13)
		hdmi_reg_writeb(hdata, HDMI_V13_ACR_CON, 4);
	else
		hdmi_reg_writeb(hdata, HDMI_ACR_CON, 4);
}
#endif /* Not Used */

#if 0
static void hdmi_audio_init(struct hdmi_tx_dev *hdmi_tx_dev)
{
	u32 sample_rate, bits_per_sample;
	u32 data_num, bit_ch, sample_frq;
	u32 val;
	u8 acr[7];

	sample_rate = 44100;
	bits_per_sample = 16;

	switch (bits_per_sample) {
	case 20:
		data_num = 2;
		bit_ch  = 1;
		break;
	case 24:
		data_num = 3;
		bit_ch  = 1;
		break;
	default:
		data_num = 1;
		bit_ch  = 0;
		break;
	}

	hdmi_set_acr(sample_rate, acr);
	hdmi_reg_acr(hdata, acr);

	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CON, HDMI_I2S_IN_DISABLE
					| HDMI_I2S_AUD_I2S | HDMI_I2S_CUV_I2S_ENABLE
					| HDMI_I2S_MUX_ENABLE);

	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CH, HDMI_I2S_CH0_EN
					| HDMI_I2S_CH1_EN | HDMI_I2S_CH2_EN);

	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CUV, HDMI_I2S_CUV_RL_EN);

	sample_frq = (sample_rate == 44100) ? 0 :
		(sample_rate == 48000) ? 2 :
		(sample_rate == 32000) ? 3 :
		(sample_rate == 96000) ? 0xa : 0x0;

	hdmi_reg_writeb(hdata, HDMI_I2S_CLK_CON, HDMI_I2S_CLK_DIS);
	hdmi_reg_writeb(hdata, HDMI_I2S_CLK_CON, HDMI_I2S_CLK_EN);

	val = hdmi_reg_read(hdata, HDMI_I2S_DSD_CON) | 0x01;
	hdmi_reg_writeb(hdata, HDMI_I2S_DSD_CON, val);

	/* Configuration I2S input ports. Configure I2S_PIN_SEL_0~4 */
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_0, HDMI_I2S_SEL_SCLK(5)
					| HDMI_I2S_SEL_LRCK(6));
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_1, HDMI_I2S_SEL_SDATA1(1)
					| HDMI_I2S_SEL_SDATA2(4));
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_2, HDMI_I2S_SEL_SDATA3(1)
					| HDMI_I2S_SEL_SDATA2(2));
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_3, HDMI_I2S_SEL_DSD(0));

	/* I2S_CON_1 & 2 */
	hdmi_reg_writeb(hdata, HDMI_I2S_CON_1, HDMI_I2S_SCLK_FALLING_EDGE
					| HDMI_I2S_L_CH_LOW_POL);
	hdmi_reg_writeb(hdata, HDMI_I2S_CON_2, HDMI_I2S_MSB_FIRST_MODE
					| HDMI_I2S_SET_BIT_CH(bit_ch)
					| HDMI_I2S_SET_SDATA_BIT(data_num)
					| HDMI_I2S_BASIC_FORMAT);

	/* Configure register related to CUV information */
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_0, HDMI_I2S_CH_STATUS_MODE_0
					| HDMI_I2S_2AUD_CH_WITHOUT_PREEMPH
					| HDMI_I2S_COPYRIGHT
					| HDMI_I2S_LINEAR_PCM
					| HDMI_I2S_CONSUMER_FORMAT);
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_1, HDMI_I2S_CD_PLAYER);
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_2, HDMI_I2S_SET_SOURCE_NUM(0));
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_3, HDMI_I2S_CLK_ACCUR_LEVEL_2
					| HDMI_I2S_SET_SMP_FREQ(sample_frq));
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_4,
					HDMI_I2S_ORG_SMP_FREQ_44_1
					| HDMI_I2S_WORD_LEN_MAX24_24BITS
					| HDMI_I2S_WORD_LEN_MAX_24BITS);

	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_CON, HDMI_I2S_CH_STATUS_RELOAD);
}
#endif /* Not Used */

#if 0
static void hdmi_audio_control(struct hdmi_tx_dev *hdmi_tx_dev, bool onoff)
{
	if (hdata->dvi_mode)
		return;

	hdmi_reg_writeb(hdata, HDMI_AUI_CON, onoff ? 2 : 0);
	hdmi_reg_writemask(hdata, HDMI_CON_0, onoff ?
					   HDMI_ASP_EN : HDMI_ASP_DIS, HDMI_ASP_MASK);
}
#endif /* Not Used */

#if 0
static void hdmi_start(struct hdmi_tx_dev *hdmi_tx_dev, bool start)
{
	u32 val = start ? HDMI_TG_EN : 0;

	if (hdata->drm_display_mode.flags & DRM_MODE_FLAG_INTERLACE)
		val |= HDMI_FIELD_EN;

	hdmi_reg_writemask(hdata, HDMI_CON_0, val, HDMI_EN);
	hdmi_reg_writemask(hdata, HDMI_TG_CMD, val, HDMI_TG_EN | HDMI_FIELD_EN);
}
#endif /* Not Used */

#if 0
static void hdmi_conf_init(struct hdmi_tx_dev *hdmi_tx_dev)
{
	union hdmi_infoframe infoframe;

	/* disable HPD interrupts from HDMI IP block, use GPIO instead */
	hdmi_reg_writemask(hdata, HDMI_INTC_CON, 0, HDMI_INTC_EN_GLOBAL |
					   HDMI_INTC_EN_HPD_PLUG | HDMI_INTC_EN_HPD_UNPLUG);

	/* choose HDMI mode */
	hdmi_reg_writemask(hdata, HDMI_MODE_SEL,
					   HDMI_MODE_HDMI_EN, HDMI_MODE_MASK);
	/* Apply Video preable and Guard band in HDMI mode only */
	hdmi_reg_writeb(hdata, HDMI_CON_2, 0);
	/* disable bluescreen */
	hdmi_reg_writemask(hdata, HDMI_CON_0, 0, HDMI_BLUE_SCR_EN);

	if (hdata->dvi_mode) {
		/* choose DVI mode */
		hdmi_reg_writemask(hdata, HDMI_MODE_SEL,
						   HDMI_MODE_DVI_EN, HDMI_MODE_MASK);
		hdmi_reg_writeb(hdata, HDMI_CON_2,
						HDMI_VID_PREAMBLE_DIS | HDMI_GUARD_BAND_DIS);
	}

	if (hdata->type == HDMI_TYPE13) {
		/* choose bluescreen (fecal) color */
		hdmi_reg_writeb(hdata, HDMI_V13_BLUE_SCREEN_0, 0x12);
		hdmi_reg_writeb(hdata, HDMI_V13_BLUE_SCREEN_1, 0x34);
		hdmi_reg_writeb(hdata, HDMI_V13_BLUE_SCREEN_2, 0x56);

		/* enable AVI packet every vsync, fixes purple line problem */
		hdmi_reg_writeb(hdata, HDMI_V13_AVI_CON, 0x02);
		/* force RGB, look to CEA-861-D, table 7 for more detail */
		hdmi_reg_writeb(hdata, HDMI_V13_AVI_BYTE(0), 0 << 5);
		hdmi_reg_writemask(hdata, HDMI_CON_1, 0x10 << 5, 0x11 << 5);

		hdmi_reg_writeb(hdata, HDMI_V13_SPD_CON, 0x02);
		hdmi_reg_writeb(hdata, HDMI_V13_AUI_CON, 0x02);
		hdmi_reg_writeb(hdata, HDMI_V13_ACR_CON, 0x04);
	} else {
		infoframe.any.type = HDMI_INFOFRAME_TYPE_AVI;
		infoframe.any.version = HDMI_AVI_VERSION;
		infoframe.any.length = HDMI_AVI_LENGTH;
		hdmi_reg_infoframe(hdata, &infoframe);

		infoframe.any.type = HDMI_INFOFRAME_TYPE_AUDIO;
		infoframe.any.version = HDMI_AUI_VERSION;
		infoframe.any.length = HDMI_AUI_LENGTH;
		hdmi_reg_infoframe(hdata, &infoframe);

		/* enable AVI packet every vsync, fixes purple line problem */
		hdmi_reg_writemask(hdata, HDMI_CON_1, 2, 3 << 5);
	}
}

static void hdmi_mode_apply(struct hdmi_tx_dev *hdmi_tx_dev)
{

}

static void hdmi_conf_apply(struct hdmi_tx_dev *hdmi_tx_dev)
{
#if 0
	hdmiphy_conf_reset(hdata);
	hdmiphy_conf_apply(hdata);

	mutex_lock(&hdata->hdmi_mutex);
	hdmi_start(hdata, false);
	hdmi_conf_init(hdata);
	mutex_unlock(&hdata->hdmi_mutex);

	hdmi_audio_init(hdata);

	/* setting core registers */
	hdmi_mode_apply(hdata);
	hdmi_audio_control(hdata, true);

	hdmi_regs_dump(hdata, "start");
#endif
}
#endif /* Not Used */

static void hdmi_mode_set(struct tcc_drm_display *display, struct drm_display_mode *mode)
{
	struct hdmi_tx_dev *hdmi_tx_dev = display->device_context;
	DRM_DEBUG_KMS("xres=%d, yres=%d, refresh=%d, intl=%s\n",
				  mode->hdisplay, mode->vdisplay,
				  mode->vrefresh, (mode->flags & DRM_MODE_FLAG_INTERLACE) ?
				  "INTERLACED" : "PROGERESSIVE");

	/* preserve mode information for later use. */
	drm_mode_copy(&hdmi_tx_dev->drm_display_mode, mode);


#if 0
	struct hdmi_tg_regs *tg = &hdata->mode_conf.conf.v14_conf.tg;
	struct hdmi_v14_core_regs *core = &hdata->mode_conf.conf.v14_conf.core;

	hdata->mode_conf.cea_video_id =
		drm_match_cea_mode((struct drm_display_mode *)m);
	hdata->mode_conf.pixel_clock = m->clock * 1000;
	hdata->mode_conf.aspect_ratio = m->picture_aspect_ratio;

	hdmi_set_reg(core->h_blank, 2, m->htotal - m->hdisplay);
	hdmi_set_reg(core->v_line, 2, m->vtotal);
	hdmi_set_reg(core->h_line, 2, m->htotal);
	hdmi_set_reg(core->hsync_pol, 1,
				 (m->flags & DRM_MODE_FLAG_NHSYNC)  ? 1 : 0);
	hdmi_set_reg(core->vsync_pol, 1,
				 (m->flags & DRM_MODE_FLAG_NVSYNC) ? 1 : 0);
	hdmi_set_reg(core->int_pro_mode, 1,
				 (m->flags & DRM_MODE_FLAG_INTERLACE) ? 1 : 0);

	/*
	 * Quirk requirement for exynos 5 HDMI IP design,
	 * 2 pixels less than the actual calculation for hsync_start
	 * and end.
	 */

	/* Following values & calculations differ for different type of modes */
	if (m->flags & DRM_MODE_FLAG_INTERLACE) {
		/* Interlaced Mode */
		hdmi_set_reg(core->v_sync_line_bef_2, 2,
					 (m->vsync_end - m->vdisplay) / 2);
		hdmi_set_reg(core->v_sync_line_bef_1, 2,
					 (m->vsync_start - m->vdisplay) / 2);
		hdmi_set_reg(core->v2_blank, 2, m->vtotal / 2);
		hdmi_set_reg(core->v1_blank, 2, (m->vtotal - m->vdisplay) / 2);
		hdmi_set_reg(core->v_blank_f0, 2, m->vtotal - m->vdisplay / 2);
		hdmi_set_reg(core->v_blank_f1, 2, m->vtotal);
		hdmi_set_reg(core->v_sync_line_aft_2, 2, (m->vtotal / 2) + 7);
		hdmi_set_reg(core->v_sync_line_aft_1, 2, (m->vtotal / 2) + 2);
		hdmi_set_reg(core->v_sync_line_aft_pxl_2, 2,
					 (m->htotal / 2) + (m->hsync_start - m->hdisplay));
		hdmi_set_reg(core->v_sync_line_aft_pxl_1, 2,
					 (m->htotal / 2) + (m->hsync_start - m->hdisplay));
		hdmi_set_reg(tg->vact_st, 2, (m->vtotal - m->vdisplay) / 2);
		hdmi_set_reg(tg->vact_sz, 2, m->vdisplay / 2);
		hdmi_set_reg(tg->vact_st2, 2, m->vtotal - m->vdisplay / 2);
		hdmi_set_reg(tg->vsync2, 2, (m->vtotal / 2) + 1);
		hdmi_set_reg(tg->vsync_bot_hdmi, 2, (m->vtotal / 2) + 1);
		hdmi_set_reg(tg->field_bot_hdmi, 2, (m->vtotal / 2) + 1);
		hdmi_set_reg(tg->vact_st3, 2, 0x0);
		hdmi_set_reg(tg->vact_st4, 2, 0x0);
	} else {
		/* Progressive Mode */
		hdmi_set_reg(core->v_sync_line_bef_2, 2,
					 m->vsync_end - m->vdisplay);
		hdmi_set_reg(core->v_sync_line_bef_1, 2,
					 m->vsync_start - m->vdisplay);
		hdmi_set_reg(core->v2_blank, 2, m->vtotal);
		hdmi_set_reg(core->v1_blank, 2, m->vtotal - m->vdisplay);
		hdmi_set_reg(core->v_blank_f0, 2, 0xffff);
		hdmi_set_reg(core->v_blank_f1, 2, 0xffff);
		hdmi_set_reg(core->v_sync_line_aft_2, 2, 0xffff);
		hdmi_set_reg(core->v_sync_line_aft_1, 2, 0xffff);
		hdmi_set_reg(core->v_sync_line_aft_pxl_2, 2, 0xffff);
		hdmi_set_reg(core->v_sync_line_aft_pxl_1, 2, 0xffff);
		hdmi_set_reg(tg->vact_st, 2, m->vtotal - m->vdisplay);
		hdmi_set_reg(tg->vact_sz, 2, m->vdisplay);
		hdmi_set_reg(tg->vact_st2, 2, 0x248); /* Reset value */
		hdmi_set_reg(tg->vact_st3, 2, 0x47b); /* Reset value */
		hdmi_set_reg(tg->vact_st4, 2, 0x6ae); /* Reset value */
		hdmi_set_reg(tg->vsync2, 2, 0x233); /* Reset value */
		hdmi_set_reg(tg->vsync_bot_hdmi, 2, 0x233); /* Reset value */
		hdmi_set_reg(tg->field_bot_hdmi, 2, 0x233); /* Reset value */
	}

	/* Following values & calculations are same irrespective of mode type */
	hdmi_set_reg(core->h_sync_start, 2, m->hsync_start - m->hdisplay - 2);
	hdmi_set_reg(core->h_sync_end, 2, m->hsync_end - m->hdisplay - 2);
	hdmi_set_reg(core->vact_space_1, 2, 0xffff);
	hdmi_set_reg(core->vact_space_2, 2, 0xffff);
	hdmi_set_reg(core->vact_space_3, 2, 0xffff);
	hdmi_set_reg(core->vact_space_4, 2, 0xffff);
	hdmi_set_reg(core->vact_space_5, 2, 0xffff);
	hdmi_set_reg(core->vact_space_6, 2, 0xffff);
	hdmi_set_reg(core->v_blank_f2, 2, 0xffff);
	hdmi_set_reg(core->v_blank_f3, 2, 0xffff);
	hdmi_set_reg(core->v_blank_f4, 2, 0xffff);
	hdmi_set_reg(core->v_blank_f5, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_3, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_4, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_5, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_6, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_pxl_3, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_pxl_4, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_pxl_5, 2, 0xffff);
	hdmi_set_reg(core->v_sync_line_aft_pxl_6, 2, 0xffff);

	/* Timing generator registers */
	hdmi_set_reg(tg->cmd, 1, 0x0);
	hdmi_set_reg(tg->h_fsz, 2, m->htotal);
	hdmi_set_reg(tg->hact_st, 2, m->htotal - m->hdisplay);
	hdmi_set_reg(tg->hact_sz, 2, m->hdisplay);
	hdmi_set_reg(tg->v_fsz, 2, m->vtotal);
	hdmi_set_reg(tg->vsync, 2, 0x1);
	hdmi_set_reg(tg->field_chg, 2, 0x233); /* Reset value */
	hdmi_set_reg(tg->vsync_top_hdmi, 2, 0x1); /* Reset value */
	hdmi_set_reg(tg->field_top_hdmi, 2, 0x1); /* Reset value */
	hdmi_set_reg(tg->tg_3d, 1, 0x0);
#endif
}

void make_product_packet(videoParams_t *videoParams, productParams_t *productParams)
{
	memset(productParams, 0, sizeof(productParams_t));
	productParams->mVendorName[0] = 'T';
	productParams->mVendorName[1] = 'C';
	productParams->mVendorName[2] = 'C';
	productParams->mVendorNameLength = 3;

	productParams->mProductName[0] = 'T';
	productParams->mProductName[1] = 'C';
	productParams->mProductName[2] = 'C';
	productParams->mProductName[3] = '8';
	productParams->mProductName[4] = '9';
	productParams->mProductName[5] = '8';
	productParams->mProductName[6] = 'X';
	productParams->mProductNameLength = 7;

	// fc_vsdsize is 27
	productParams->mVendorPayloadLength = 0;

	if(0) {
		// Only HDMI2.0 3D Mode..!!
		productParams->mOUI = 0x00D85DC4;
		productParams->mVendorPayload[0] = 0x1; // version 1
	}
	else {
		productParams->mOUI = 0x00030C00;
		productParams->mVendorPayloadLength = 4;
		if((videoParams->mDtd.mCode >= 93 && videoParams->mDtd.mCode <= 95) || videoParams->mDtd.mCode == 98)
		{
			productParams->mVendorPayload[0] = 1 << 5;
			productParams->mVendorPayloadLength  = 5;
			switch(videoParams->mDtd.mCode)
			{
			case 93: // 3840x2160p24Hz
				productParams->mVendorPayload[1] = 3;
				break;
			case 94: // 3840x2160p25Hz
				productParams->mVendorPayload[1] = 2;
				break;
			case 95: // 3840x2160p30Hz
				productParams->mVendorPayload[1] = 1;
				break;
			case 98: // 4096x2160p24Hz
				productParams->mVendorPayload[1] = 4;
				break;
			}
		}else if (videoParams->mHdmiVideoFormat == 2) {
			u8 struct_3d = videoParams->m3dStructure;
			productParams->mVendorPayloadLength = 6;
			// frame packing || tab || sbs
			if ((struct_3d == 0) || (struct_3d == 6) || (struct_3d == 8)) {
				/*u32 oui;*/
				/*u8 data[3];*/
				if(0) {
					// Futher Used
					productParams->mVendorPayload[0] = 0x1; // version 1
					productParams->mVendorPayload[1] = 0x1; // 3D valid
					productParams->mVendorPayload[2] = ((struct_3d & 0xF) << 4) | (videoParams->m3dExtData & 0xF);
					productParams->mOUI = 0x01D85DC4;
				}
				else {
					productParams->mVendorPayload[0] = videoParams->mHdmiVideoFormat << 5;
					productParams->mVendorPayload[1] = struct_3d << 4;
					productParams->mVendorPayload[2] = videoParams->m3dExtData << 4;
					productParams->mOUI = 0x00030C00;
				}
			}
		}
	}
}


static int fc_video_config_timing(struct hdmi_tx_dev *hdmi_tx_dev)
{
        int ret = -1;
        unsigned char reg_val;
        unsigned int usr_data;

        struct drm_display_mode *drm_display_mode  = &hdmi_tx_dev->drm_display_mode;

        if(IS_ERR_OR_NULL(hdmi_tx_dev)) {
                goto dev_is_null;
        }

        // Update FC_INVDCONF
        reg_val = _hdmi_dev_read(hdmi_tx_dev, FC_INVIDCONF);
        reg_val &= ~((1 << 6)|(1 << 5)|(1 << 3)|(1 << 1)|(1 << 0));
        reg_val |= (((drm_display_mode->flags & DRM_MODE_FLAG_PVSYNC)?1:0) << 6);
        reg_val |= (((drm_display_mode->flags & DRM_MODE_FLAG_PHSYNC)?1:0) << 5);
        reg_val |= (1/*de_in_polarity*/ << 5);
        reg_val |= ((hdmi_tx_dev->videoParam.mHdmi?1:0 /* 1: HDMI; 0: DVI */) << 3);
        reg_val |= (((drm_display_mode->flags & DRM_MODE_FLAG_3D_FRAME_PACKING)?0:(drm_display_mode->flags & DRM_MODE_FLAG_INTERLACE)?1:0) << 1);
        reg_val |= ((drm_display_mode->flags & DRM_MODE_FLAG_3D_FRAME_PACKING)?0:(drm_display_mode->flags & DRM_MODE_FLAG_INTERLACE)?1:0);
        _hdmi_dev_write(hdmi_tx_dev, FC_INVIDCONF, reg_val);

        // Update Vactive
        usr_data = drm_display_mode->crtc_vdisplay;
        reg_val = (unsigned char)(usr_data & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, FC_INVACTIV0, reg_val);
        reg_val = (unsigned char)((usr_data >> 8) & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, FC_INVACTIV1, reg_val);

        // Update Hactive
        /*
        if(video->mEncodingOut == YCC420){
                LOGGER(SNPS_WARN, "Encoding configured to YCC 420\r\n");
                reg_val = (unsigned char)((drm_display_mode->crtc_hdisplay >> 1) & 0xFF);
                _hdmi_dev_write(hdmi_tx_dev, (FC_INHACTIV0), (u8) (value));
                reg_val = (unsigned char)((drm_display_mode->crtc_vdisplay >> 9) & 0xFF);
	        _hdmi_dev_write(hdmi_tx_dev, FC_INHACTIV1, reg_val);

                fc_video_HBlank(dev, dtd->mHBlanking/2);
                fc_video_HSyncPulseWidth(dev, dtd->mHSyncPulseWidth/2);
                fc_video_HSyncEdgeDelay(dev, dtd->mHSyncOffset/2);
        } else {
        */
        // Update Hactive
        usr_data = (drm_display_mode->crtc_hdisplay >> 0);
        // usr_data = (drm_display_mode->crtc_hdisplay >> 1); // YCC420
        reg_val = (unsigned char)(usr_data & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, (FC_INHACTIV0), reg_val);
        reg_val = (unsigned char)((usr_data >> 8) & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, FC_INHACTIV1, reg_val);

        // Update Hblank
        usr_data = (drm_display_mode->crtc_htotal - drm_display_mode->crtc_hdisplay);
        reg_val = (unsigned char)(usr_data & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, FC_INHBLANK0, reg_val);
        reg_val = (unsigned char)((usr_data >> 8) & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, FC_INHBLANK1, reg_val);

        // Update Hsync Plus Witdh
        usr_data = (drm_display_mode->crtc_hsync_end - drm_display_mode->crtc_hsync_start);
        reg_val = (unsigned char)(usr_data & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, (FC_HSYNCINWIDTH0), reg_val);
        reg_val = (unsigned char)((usr_data >> 8) & 0xFF);
	_hdmi_dev_write(hdmi_tx_dev, FC_HSYNCINWIDTH1, reg_val);

        // Update Hsync Offset
        usr_data = drm_display_mode->crtc_hsync_start-drm_display_mode->crtc_hdisplay;
        reg_val = (unsigned char)(usr_data & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, (FC_HSYNCINDELAY0), reg_val);
        reg_val = (unsigned char)((usr_data >> 8) & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, FC_HSYNCINDELAY1, reg_val);

        // Update Vblank
        usr_data = (drm_display_mode->crtc_vtotal - drm_display_mode->crtc_vdisplay);
        reg_val = (unsigned char)(usr_data & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, (FC_INVBLANK), reg_val);

        // Update VSync Offset
        usr_data = drm_display_mode->crtc_vsync_start - drm_display_mode->crtc_vdisplay;
        reg_val = (unsigned char)(usr_data & 0xFF);
        _hdmi_dev_write(hdmi_tx_dev, (FC_VSYNCINDELAY), reg_val);

        // Update Vsync Plus Width
        usr_data = drm_display_mode->crtc_vsync_end - drm_display_mode->crtc_vsync_start;
        reg_val = (unsigned char)(usr_data & 0x3F);
        _hdmi_dev_write(hdmi_tx_dev, FC_VSYNCINWIDTH, reg_val);

        reg_val = _hdmi_dev_read(hdmi_tx_dev, FC_PRCONF);
        reg_val &= ~(0xF << 4);
        reg_val |= (((drm_display_mode->flags & DRM_MODE_FLAG_DBLCLK)?2:1) << 4);
        _hdmi_dev_write(hdmi_tx_dev, FC_PRCONF, reg_val);
        ret = 0;

dev_is_null:
        return ret;
}


static void fc_video_config_default(struct hdmi_tx_dev *hdmi_tx_dev)
{
        _hdmi_dev_write(hdmi_tx_dev, FC_CTRLDUR, 12);
        _hdmi_dev_write(hdmi_tx_dev, FC_EXCTRLDUR, 32);
        /* spacing < 256^2 * config / tmdsClock, spacing <= 50ms
        * worst case: tmdsClock == 25MHz => config <= 19
        */
        _hdmi_dev_write(hdmi_tx_dev, FC_EXCTRLSPAC, 1);

        _hdmi_dev_write(hdmi_tx_dev, FC_CH0PREAM, 11);
        _hdmi_dev_write(hdmi_tx_dev, FC_CH1PREAM, 22);
        _hdmi_dev_write(hdmi_tx_dev, FC_CH2PREAM, 33);
}

static void hdmi_commit(struct tcc_drm_display *display)
{
        int vic, hdmi_mode;
	struct hdmi_tx_dev *hdmi_tx_dev = display->device_context;
	struct drm_display_mode *drm_display_mode  = &hdmi_tx_dev->drm_display_mode;

        vic = drm_match_cea_mode(drm_display_mode);

	hdmi_mode = hdmi_tx_dev->videoParam.mHdmi;
	// Reset Parameters
	video_params_reset(hdmi_tx_dev, &hdmi_tx_dev->videoParam);
	audio_reset(hdmi_tx_dev, &hdmi_tx_dev->audioParam);

	DRM_DEBUG_KMS("vic=%d, xres=%d, yres=%d, refresh=%d, intl=%s\n",
				  vic,
				  drm_display_mode->hdisplay, drm_display_mode->vdisplay,
				  drm_display_mode->vrefresh, (drm_display_mode->flags & DRM_MODE_FLAG_INTERLACE) ?
				  "INTERLACED" : "PROGERESSIVE");
	hdmi_tx_dev->videoParam.mDtd.mCode = vic;
        hdmi_tx_dev->videoParam.mDtd.mPixelRepetitionInput = 0;

        /** In units of 1MHz */
        hdmi_tx_dev->videoParam.mDtd.mPixelClock = drm_display_mode->crtc_clock;
        /** 1 for interlaced, 0 progressive */
        hdmi_tx_dev->videoParam.mDtd.mInterlaced = (drm_display_mode->flags & DRM_MODE_FLAG_INTERLACE)?1:0;
        hdmi_tx_dev->videoParam.mDtd.mHActive = drm_display_mode->crtc_hdisplay;
        hdmi_tx_dev->videoParam.mDtd.mHBlanking = (drm_display_mode->crtc_htotal - drm_display_mode->crtc_hdisplay);
        hdmi_tx_dev->videoParam.mDtd.mHBorder = 0;
        hdmi_tx_dev->videoParam.mDtd.mHImageSize = (drm_display_mode->picture_aspect_ratio==HDMI_PICTURE_ASPECT_16_9)?16:4;
        hdmi_tx_dev->videoParam.mDtd.mHSyncOffset = drm_display_mode->crtc_hsync_start-drm_display_mode->crtc_hdisplay;
        hdmi_tx_dev->videoParam.mDtd.mHSyncPulseWidth = (drm_display_mode->crtc_hsync_end - drm_display_mode->crtc_hsync_start);

        /** 0 for Active low, 1 active high */
        hdmi_tx_dev->videoParam.mDtd.mHSyncPolarity = (drm_display_mode->flags & DRM_MODE_FLAG_PHSYNC)?1:0;
        hdmi_tx_dev->videoParam.mDtd.mVActive = drm_display_mode->crtc_vdisplay;
        hdmi_tx_dev->videoParam.mDtd.mVBlanking = (drm_display_mode->crtc_vtotal - drm_display_mode->crtc_vdisplay);
        hdmi_tx_dev->videoParam.mDtd.mVBorder = 0;
        hdmi_tx_dev->videoParam.mDtd.mVImageSize = (drm_display_mode->picture_aspect_ratio==HDMI_PICTURE_ASPECT_16_9)?9:3;
        hdmi_tx_dev->videoParam.mDtd.mVSyncOffset = drm_display_mode->crtc_vsync_start - drm_display_mode->crtc_vdisplay; // 1080
        hdmi_tx_dev->videoParam.mDtd.mVSyncPulseWidth = (drm_display_mode->crtc_vsync_end - drm_display_mode->crtc_vsync_start);

        /** 0 for Active low, 1 active high */
        hdmi_tx_dev->videoParam.mDtd.mVSyncPolarity = (drm_display_mode->flags & DRM_MODE_FLAG_PVSYNC)?1:0;
	hdmi_tx_dev->videoParam.mEncodingIn = RGB;
	hdmi_tx_dev->videoParam.mEncodingOut = RGB;

	hdmi_tx_dev->videoParam.mHdmi = hdmi_mode;

	hdmi_tx_dev->hdmi_tx_ctrl.data_enable_polarity = 1;
	hdmi_tx_dev->hdmi_tx_ctrl.csc_on = 1;
	hdmi_tx_dev->hdmi_tx_ctrl.audio_on = 1;


	make_product_packet(&hdmi_tx_dev->videoParam, &hdmi_tx_dev->productParam);

	//hdmi_tx_dev->hdmi_tx_ctrl.pixel_clock = api_data.videoParam.mDtd.mPixelClock;
	mutex_lock(&hdmi_tx_dev->hdmi_mutex);
	if(!test_bit(HDMI_TX_STATUS_SUSPEND_L0, &hdmi_tx_dev->status)) {
		if(test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_tx_dev->status)) {
			hdmi_api_Configure_0(hdmi_tx_dev, &hdmi_tx_dev->videoParam, &hdmi_tx_dev->audioParam, &hdmi_tx_dev->productParam, &hdmi_tx_dev->hdcpParam);
                        fc_video_config_timing(hdmi_tx_dev);
                        fc_video_config_default(hdmi_tx_dev);
                        hdmi_api_Configure_1(hdmi_tx_dev, &hdmi_tx_dev->videoParam, &hdmi_tx_dev->audioParam, &hdmi_tx_dev->productParam, &hdmi_tx_dev->hdcpParam);
		} else {
			pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
		}
	} else {
		pr_err("## Failed to hdmi_api_Configure at HDMI_TX_STATUS_SUSPEND_L0\r\n");
	}

	mdelay(90);
	DRM_DEBUG_KMS("disable avmute\r\n");
	hdmi_api_avmute(hdmi_tx_dev, 0);

	mutex_unlock(&hdmi_tx_dev->hdmi_mutex);
}

static void hdmi_poweron(struct tcc_drm_display *display)
{
	struct hdmi_tx_dev *hdmi_tx_dev = display->device_context;

	mutex_lock(&hdmi_tx_dev->hdmi_mutex);
	if (test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_tx_dev->status)) {
		pr_err("%s already power on\r\n", __func__);
		mutex_unlock(&hdmi_tx_dev->hdmi_mutex);
		return;
	}
	mutex_unlock(&hdmi_tx_dev->hdmi_mutex);

	pm_runtime_get_sync(hdmi_tx_dev->parent_dev);

	dwc_hdmi_core_power_on(hdmi_tx_dev);

	hdmi_commit(display);
}

static void hdmi_poweroff(struct tcc_drm_display *display)
{

	struct hdmi_tx_dev *hdmi_tx_dev = display->device_context;

	mutex_lock(&hdmi_tx_dev->hdmi_mutex);
	if (!test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_tx_dev->status)) {
		goto out;
	}
	mutex_unlock(&hdmi_tx_dev->hdmi_mutex);

	// diable hdmi
	dwc_hdmi_core_power_off(hdmi_tx_dev);

	pm_runtime_put_sync(hdmi_tx_dev->parent_dev);

	mutex_lock(&hdmi_tx_dev->hdmi_mutex);

out:
	mutex_unlock(&hdmi_tx_dev->hdmi_mutex);
}

static void hdmi_dpms(struct tcc_drm_display *display, int mode)
{
	struct hdmi_tx_dev *hdmi_tx_dev = display->device_context;
	struct drm_encoder *encoder = hdmi_tx_dev->encoder;
	struct drm_crtc *crtc = encoder->crtc;
	const struct drm_crtc_helper_funcs *funcs = NULL;

	DRM_DEBUG_KMS("mode: %d\n", mode);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		hdmi_poweron(display);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		/*
		 * The SFRs of VP and Mixer are updated by Vertical Sync of
		 * Timing generator which is a part of HDMI so the sequence
		 * to disable TV Subsystem should be as following,
		 *	VP -> Mixer -> HDMI
		 *
		 * Below codes will try to disable Mixer and VP(if used)
		 * prior to disabling HDMI.
		 */
		if (crtc)
			funcs = crtc->helper_private;
		if (funcs && funcs->dpms)
			(*funcs->dpms)(crtc, mode);

		hdmi_poweroff(display);
		break;
	default:
		DRM_DEBUG_KMS("unknown dpms mode: %d\n", mode);
		break;
	}
}

void hdmi_prepare(struct tcc_drm_display *display)
{
	DRM_DEBUG_KMS(" \r\n");
}

void hdmi_disable(struct tcc_drm_display *display)
{
	struct hdmi_tx_dev *hdmi_tx_dev = display->device_context;

	DRM_DEBUG_KMS(" \r\n");
	mutex_lock(&hdmi_tx_dev->hdmi_mutex);
	if(test_bit(HDMI_TX_STATUS_POWER_ON, &hdmi_tx_dev->status)) {
		// Disable TMDS Clock Ratio and Scramble before disable tmds clock.
		scdc_tmds_bit_clock_ratio_enable_flag(hdmi_tx_dev, 0);
		scrambling(hdmi_tx_dev, 0);
		mdelay(50);
		mc_disable_all_clocks(hdmi_tx_dev);
		clear_bit(HDMI_TX_STATUS_OUTPUT_ON, &hdmi_tx_dev->status);
	} else {
		printk(" HDMI is not powred <%d>\r\n", __LINE__);
	}
	mutex_unlock(&hdmi_tx_dev->hdmi_mutex);
}


static struct tcc_drm_display_ops hdmi_display_ops = {
	.create_connector = hdmi_create_connector,
	.mode_fixup	= hdmi_mode_fixup,
	.mode_set	= hdmi_mode_set,
	.dpms		= hdmi_dpms,
	.commit		= hdmi_commit,
	.prepare        = hdmi_prepare,
	.disable        = hdmi_disable,
};

static struct tcc_drm_display hdmi_display = {
	.type = TCC_DISPLAY_TYPE_HDMI,
	.ops = &hdmi_display_ops,
};

#if 0
static void hdmi_hotplug_work_func(struct work_struct *work)
{
	struct hdmi_tx_dev *hdmi_tx_dev;

	hdmi_tx_dev = container_of(work, struct hdmi_tx_dev, hotplug_handler);

	// Disable HPD Interrupt
	DRM_DEBUG_KMS("disable hpd irq\r\n");
	//hdmi_tx_dev->hpd_enable = 0;
	//hdmi_hpd_enable(hdmi_tx_dev);

	drm_helper_hpd_irq_event(hdmi_tx_dev->drm_dev);

	// Enable HPD Interrupt
	DRM_DEBUG_KMS("enable hpd irq\r\n");
	//hdmi_tx_dev->hpd_enable = 1;
	//hdmi_hpd_enable(hdmi_tx_dev);
}
#endif

static void hdmi_enable_hotplug_irq(struct hdmi_tx_dev *hdmi_tx_dev)
{
	int prev_hpd = hdmi_tx_dev->hpd;
	hdmi_tx_dev->hpd = gpio_get_value(hdmi_tx_dev->hpd_gpio);
	DRM_DEBUG_KMS("HPD=%d ",hdmi_tx_dev->hpd);

	irq_set_irq_type(hdmi_tx_dev->hpd_irq, hdmi_tx_dev->hpd?IRQF_TRIGGER_FALLING:IRQF_TRIGGER_RISING);
	enable_irq(hdmi_tx_dev->hpd_irq);

	if(prev_hpd != hdmi_tx_dev->hpd)
		schedule_work(&hdmi_tx_dev->hotplug_handler);
}

static void hdmi_hotplug_work_func(struct work_struct *work)
{
	/*int hpd = 0;*/
	int /*prev_hpd,*/current_hpd;

	struct hdmi_tx_dev *hdmi_tx_dev;

	hdmi_tx_dev = container_of(work, struct hdmi_tx_dev, hotplug_handler);
	DRM_DEBUG_KMS(" \r\n");
	disable_irq(hdmi_tx_dev->hpd_irq);
	current_hpd = gpio_get_value(hdmi_tx_dev->hpd_gpio);
#if 0
	if(!current_hpd) {
		prev_hpd = current_hpd;
		for(i=0;i<10;i++) {
			current_hpd = gpio_get_value(hdmi_tx_dev->hpd_gpio);
			usleep_range(10000, 10000);
			if(prev_hpd && !current_hpd) {
				i = -1;
			}
		}
	}
#endif

	mutex_lock(&hdmi_tx_dev->hdmi_mutex);
	hdmi_tx_dev->hpd = current_hpd;
	mutex_unlock(&hdmi_tx_dev->hdmi_mutex);

	if (hdmi_tx_dev->drm_dev) {
		drm_helper_hpd_irq_event(hdmi_tx_dev->drm_dev);
	}
	hdmi_enable_hotplug_irq(hdmi_tx_dev);
}

static irqreturn_t hdmi_hpd_irq_handler(int irq, void *arg)
{
	struct hdmi_tx_dev *hdmi_tx_dev = arg;

	DRM_DEBUG_KMS("hdmi_hpd_irq_handler\r\n");
	schedule_work(&hdmi_tx_dev->hotplug_handler);


	return IRQ_HANDLED;
}


static int hdmi_bind(struct device *dev, struct device *master, void *data)
{
	int ret = -1;
	struct drm_device *drm_dev = data;
	struct hdmi_tx_dev *hdmi_tx_dev;

	hdmi_tx_dev = hdmi_display.device_context;
	hdmi_tx_dev->drm_dev = drm_dev;

	ret = tcc_drm_create_enc_conn(drm_dev, &hdmi_display);
	if(ret < 0) {
		goto end_process;
	}

	hdmi_enable_hotplug_irq(hdmi_tx_dev);

#if 0
	hdmi_tx_dev->hpd_enable = 1;
	hdmi_hpd_enable(hdmi_tx_dev);
#endif

end_process:
	return ret;
}

static void hdmi_unbind(struct device *dev, struct device *master, void *data)
{
	struct hdmi_tx_dev *hdmi_tx_dev = hdmi_display.device_context;

	cancel_work_sync(&hdmi_tx_dev->hotplug_handler);
}

static const struct component_ops hdmi_component_ops = {
	.bind	= hdmi_bind,
	.unbind = hdmi_unbind,
};

/**
 * @short Map memory blocks
 * @param[in,out] main HDMI TX structure
 * @param[in] Device node pointer
 * @return Return -ENOMEM if one of the blocks is not mapped and 0 if all
 * blocks are mapped successful.
 */
static int __init of_parse_hdmi_dt(struct hdmi_tx_dev *hdmi_tx_dev, struct device_node *node){
	int ret = 0;
	unsigned int of_loop;

	struct device_node *ddibus_np, *ckc_np;

	// Map DWC HDMI TX Core
	hdmi_tx_dev->dwc_hdmi_tx_core_io = of_iomap(node, PROTO_HDMI_TX_CORE);
	if(!hdmi_tx_dev->dwc_hdmi_tx_core_io){
		pr_err("%s:Unable to map resource\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;

	}

	// Map HDCP
	hdmi_tx_dev->hdcp_io = of_iomap(node, PROTO_HDMI_TX_HDCP);       // HDCP_ADDRESS,
	if(!hdmi_tx_dev->hdcp_io){
		pr_err("%s:Unable to map hdcp_base_addr resource\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	// Map HDMI TX PHY interface
	hdmi_tx_dev->hdmi_tx_phy_if_io = of_iomap(node, PROTO_HDMI_TX_PHY); // TXPHY_IF_ADDRESS,
	if(!hdmi_tx_dev->hdmi_tx_phy_if_io){
		pr_err("%s:Unable to map hdmi_tx_phy_if_base_addr resource\n",
			   FUNC_NAME);
		ret = -ENODEV;
		goto end_process;

	}

	// HPD GPIO
	hdmi_tx_dev->hpd_gpio = of_get_gpio(node, 0);
	if (hdmi_tx_dev->hpd_gpio < 0) {
		DRM_ERROR("Could not parse the hpd gpio.\r\n");
		ret = -ENODEV;
		goto end_process;
	}

	DRM_DEBUG_KMS("HPD GPIO=%d\r\n", hdmi_tx_dev->hpd_gpio);
	ret = gpio_request(hdmi_tx_dev->hpd_gpio, dev_name(hdmi_tx_dev->parent_dev));
	if (ret) {
		DRM_ERROR("Could not request the hpd gpio\r\n");
		ret = -ENODEV;
		goto end_process;
	}
	gpio_direction_input(hdmi_tx_dev->hpd_gpio);

	hdmi_tx_dev->hpd_irq = gpio_to_irq(hdmi_tx_dev->hpd_gpio);
	if (hdmi_tx_dev->hpd_irq < 0) {
		pr_err("%s:Unable to get irq number\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;

	}
	DRM_DEBUG_KMS("HPD IRQ=%d\r\n", hdmi_tx_dev->hpd_irq);

	// Find DDI_BUS Node
	ddibus_np = of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
	if(!ddibus_np) {
		pr_err("%s:Unable to map ddibus resource\n",
			   FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	// Map DDI_Bus interface
	hdmi_tx_dev->ddibus_io = of_iomap(ddibus_np, 0);
	if(!hdmi_tx_dev->ddibus_io){
		pr_err("%s:Unable to map ddibus_io base address resource\n",
			   FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	ckc_np = of_find_compatible_node(NULL, NULL, "telechips,ckc");
	if(!ckc_np) {
		pr_err("%s:Unable to map ckc resource\n",
			   FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	hdmi_tx_dev->io_bus = of_iomap(ckc_np, 5);
	if(!hdmi_tx_dev->io_bus){
		pr_err("%s:Unable to map io_bus base address resource\n",
			   FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	// Parse Interrupt
	for(of_loop = HDMI_IRQ_TX_CORE; of_loop < HDMI_IRQ_TX_MAX; of_loop++) {
		hdmi_tx_dev->irq[of_loop] = of_irq_to_resource(node, of_loop, NULL);
	}

	// Parse Clock
	for(of_loop = HDMI_CLK_INDEX_PHY27M; of_loop < HDMI_CLK_INDEX_MAX; of_loop++) {
		hdmi_tx_dev->clk[of_loop] = of_clk_get(node, of_loop);
		if (IS_ERR(hdmi_tx_dev->clk[of_loop])) {
			pr_err("%s:Unable to map hdmi clock (%d)\n",
				   FUNC_NAME, of_loop);
			ret = -ENODEV;
			goto end_process;
		}
	}

	// Parse Clock Freq
	ret = of_property_read_u32(node, "clock-frequency", &hdmi_tx_dev->clk_freq_pclk);
	if(ret < 0) {
		pr_err("%s:Unable to map hdmi pclk frequency\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	ret = of_property_read_u32(node, "audio_src_offset", &hdmi_tx_dev->audio_offset);
	if(ret < 0) {
		pr_err("%s:Unable to map audio source offset\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	of_property_read_u32(node, "hdmi_phy_type", &hdmi_tx_dev->hdmi_phy_type);
	if(hdmi_tx_dev->hdmi_phy_type >= hdmi_phy_type_max) {
		printk("%s: Invalide hdmi phy type. force set hdmi phy type to 8980 \r\n", FUNC_NAME);
		hdmi_tx_dev->hdmi_phy_type = 0;
	}

	// HDMI Display Device
	of_property_read_u32(node, "DisplayDevice", &hdmi_tx_dev->display_device);

	// Update the verbose level
	of_property_read_u32(node, "verbose", &hdmi_tx_dev->verbose);
	if(hdmi_tx_dev->verbose >= VERBOSE_BASIC)
		pr_info("%s: verbose level is %d\n", FUNC_NAME, hdmi_tx_dev->verbose);

end_process:

	return ret;
}


/**
 * @short Map memory blocks
 * @param[in,out] main HDMI TX structure
 * @param[in] Device node pointer
 * @return Return -ENOMEM if one of the blocks is not mapped and 0 if all
 * blocks are mapped successful.
 */
static int __init of_parse_i2c_mapping(struct device_node *node){
	int ret = 0;
	struct device_node *i2c_np;
	volatile void __iomem   *hdmi_i2c_io = NULL;
	unsigned int i2c_mapping_reg, i2c_mapping_mask, i2c_mapping_val;
	volatile unsigned int val;

	// Map HDMI I2C Mapping interface
	i2c_np = of_parse_phandle(node, "hdmi_i2c_mapping", 0);
	if (!i2c_np) {
		pr_err("%s:Unable to map i2c_mapping node\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	// Map HDMI TX PHY interface
	hdmi_i2c_io = of_iomap(i2c_np, 1); // I2C Port Config,
	if(!hdmi_i2c_io){
		pr_err("%s:Unable to map i2c mapping resource\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}
	ret = of_property_read_u32_index(node, "hdmi_i2c_mapping_val", 0, &i2c_mapping_reg);
	if(ret < 0) {
		pr_err("%s:Unable to map i2c_mapping_reg\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	ret = of_property_read_u32_index(node, "hdmi_i2c_mapping_val", 1, &i2c_mapping_mask);
	if(ret < 0) {
		pr_err("%s:Unable to map i2c_mapping_mask\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	ret = of_property_read_u32_index(node, "hdmi_i2c_mapping_val", 2, &i2c_mapping_val);
	if(ret < 0) {
		pr_err("%s:Unable to map i2c_mapping_val\n", FUNC_NAME);
		ret = -ENODEV;
		goto end_process;
	}

	//pr_info("of_parse_i2c_mapping reg=0x%x, mask=0x%x, val=0x%x\r\n", i2c_mapping_reg, i2c_mapping_mask, i2c_mapping_val);
	val = ioread32((void*)(hdmi_i2c_io + i2c_mapping_reg));
	val &= ~i2c_mapping_mask;
	val |= i2c_mapping_val;
	iowrite32(val, (void*)(hdmi_i2c_io + i2c_mapping_reg));

end_process:

	if(hdmi_i2c_io)
		iounmap(hdmi_i2c_io);

	return ret;
}

static int __init hdmi_probe(struct platform_device *pdev)
{
	int ret;

	struct hdmi_tx_dev *hdmi_tx_dev = NULL;

	pr_info("****************************************\n");
	pr_info("%s:DRM HDMI module\n", FUNC_NAME);
	pr_info("****************************************\n");

	ret = tcc_drm_component_add(&pdev->dev, TCC_DEVICE_TYPE_CONNECTOR, hdmi_display.type);
	if (ret) {
		pr_err("%s failed tcc_drm_component_add\r\n", FUNC_NAME);
		return ret;
	}

	if (!pdev->dev.of_node) {
		ret = -ENODEV;
		goto err_del_component;
	}

	hdmi_tx_dev = devm_kzalloc(&pdev->dev, sizeof(struct hdmi_tx_dev), GFP_KERNEL);
	if (!hdmi_tx_dev) {
		ret = -ENOMEM;
		goto err_del_component;
	}

	platform_set_drvdata(pdev, &hdmi_display);

	// Update the device node
	hdmi_tx_dev->parent_dev = &pdev->dev;
	hdmi_tx_dev->device_name = "TCC_HDMI";

	spin_lock_init(&hdmi_tx_dev->slock);
	spin_lock_init(&hdmi_tx_dev->slock_power);

	mutex_init(&hdmi_tx_dev->hdmi_mutex);

	// Alloc HDMI Parameters
	hdmi_tx_dev->drmParm = (void*)devm_kzalloc(hdmi_tx_dev->parent_dev, sizeof(DRM_Packet_t), GFP_KERNEL);

	// wait queue of poll
	init_waitqueue_head(&hdmi_tx_dev->poll_wq);

	// Map memory blocks
	if(of_parse_hdmi_dt(hdmi_tx_dev, pdev->dev.of_node) < 0){
		pr_info("%s:Map memory blocks failed\n", FUNC_NAME);
		ret = -ENOMEM;
		goto free_mem;
	}

	if(of_parse_i2c_mapping(pdev->dev.of_node) < 0){
		pr_info("%s:Map i2c mapping failed\n", FUNC_NAME);
		ret = -ENOMEM;
		goto free_mem;
	}

	INIT_WORK(&hdmi_tx_dev->hotplug_handler, hdmi_hotplug_work_func);
	ret = devm_request_threaded_irq(hdmi_tx_dev->parent_dev, hdmi_tx_dev->hpd_irq, NULL,
									hdmi_hpd_irq_handler, IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_ONESHOT, "hdmi_hpd", hdmi_tx_dev);
	if (ret) {
		DRM_ERROR("failed to register hdmi interrupt\n");
		goto free_mem;
	}
	disable_irq(hdmi_tx_dev->hpd_irq);

	pm_runtime_enable(hdmi_tx_dev->parent_dev);
	hdmi_display.device_context = hdmi_tx_dev;

	ret = component_add(hdmi_tx_dev->parent_dev, &hdmi_component_ops);
	if (ret)
		goto err_disable_pm_runtime;

	// Initialize Interrupt
	dwc_init_interrupts(hdmi_tx_dev);

	// Enable HDMI
	//dwc_hdmi_core_power_on(hdmi_tx_dev);

	return ret;

err_disable_pm_runtime:
	pm_runtime_disable(hdmi_tx_dev->parent_dev);

err_del_component:
	tcc_drm_component_del(hdmi_tx_dev->parent_dev, TCC_DEVICE_TYPE_CONNECTOR);

free_mem:
#if 0
	if(dev->drmParm) {
		devm_kfree(&pdev->dev, dev->drmParm);
		dev->drmParm = NULL;
	}
#endif


	return ret;
}

static int hdmi_remove(struct platform_device *pdev)
{
	struct hdmi_tx_dev *hdmi_tx_dev = hdmi_display.device_context;

	pr_info("**************************************\n");
	pr_info("%s:Removing HDMI module\n", FUNC_NAME);
	pr_info("**************************************\n");

	pm_runtime_disable(hdmi_tx_dev->parent_dev);

	pr_info("%s:Remove proc file system\n", FUNC_NAME);
	//proc_interface_remove(hdmi_tx_dev);

	dwc_deinit_interrupts(hdmi_tx_dev);

	// Deregister HDMI Misc driver.
	//dwc_hdmi_misc_deregister(hdmi_tx_dev);

	tcc_drm_component_del(hdmi_tx_dev->parent_dev, TCC_DEVICE_TYPE_CONNECTOR);

	return 0;
}


/**
 * @short of_device_id structure
 */
static const struct of_device_id dw_hdmi_tx[] = {
	{ .compatible = "telechips,dw-hdmi-tx" },
};
MODULE_DEVICE_TABLE(of, dw_hdmi_tx);


struct platform_driver hdmi_driver = {
	.probe		= hdmi_probe,
	.remove		= hdmi_remove,
	.driver		= {
		.name = "dw-hdmi-tx",
		.owner	= THIS_MODULE,
		.of_match_table = dw_hdmi_tx,
	},
};

