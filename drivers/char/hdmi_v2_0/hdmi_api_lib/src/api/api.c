// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare HDMI driver
 */
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <include/hdmi_ioctls.h>
#include <include/video_params.h>
#include <include/hdmi_drm.h>
#include <include/hdmi_misc.h>

#include <api/api.h>
#include <core/main_controller.h>
#include <core/video.h>
#include <core/audio.h>
#include <core/packets.h>
#include <core/irq.h>

#include <core/product.h>

#include <core/fc_debug.h>
#include <core/fc_video.h>
#include <core/interrupt/interrupt_reg.h>
#include <phy/phy.h>

#include <phy/phy_reg.h>
#include <hdmi_api_lib/src/core/main_controller/main_controller_reg.h>
#include <scdc/scdc.h>
#include <scdc/scrambling.h>
#include <hdcp/hdcp.h>

#ifdef CONFIG_TCC_HDCP2_CORE_DRIVER
#include <host_lib_driver/host_lib_driver_linux_kernel.h>
#endif

static void hdmi_api_wait_phylock(struct hdmi_tx_dev *dev)
{
	volatile unsigned int val = 0;
	int loop, phy_lock = 0;

	hdmi_phy_enable_hpd_sense(dev);
	// PLL_LOCK
	dwc_hdmi_phy_clear_status_ready(dev);
	for (loop = 10, val = 0; (loop > 0); loop--) {
		val = dwc_hdmi_phy_status_ready(dev);
		if (val) {
			phy_lock |= (1 << 0);
			break;
		}
		mdelay(10);
	}
	if (!val)
		goto END_PROCESS;

	// PHY_CLK_READY
	dwc_hdmi_phy_clear_status_clock_ready(dev);
	for (loop = 10, val = 0; (loop > 0); loop--) {
		val = dwc_hdmi_phy_status_clock_ready(dev);
		if (val) {
			phy_lock |= (1 << 1);
			break;
		}
		mdelay(10);
	}
	if (!val)
		goto END_PROCESS;

	//  PHY_READY
	dwc_hdmi_phy_clear_status_pll_lock(dev);
	for (loop = 10, val = 0; (loop > 0); loop--) {
		val = dwc_hdmi_phy_status_pll_lock(dev);
		if (val) {
			phy_lock |= (1 << 2);
			break;
		}
		mdelay(10);
	}
	if (!val)
		goto END_PROCESS;

END_PROCESS:
	pr_info("[INFO][HDMI_V20]HDMI_LOCK=0x%x\r\n", phy_lock);

}

int hdmi_api_Configure(struct hdmi_tx_dev *dev)
{
	int ret = -1;
	int mc_timeout = 100;
	unsigned int mc_reg_val;

	videoParams_t *video;
	audioParams_t *audio;
	productParams_t *product;


	if (dev == NULL) {
		pr_err("[ERROR][HDMI_V20]%s dev is NULL\r\n", __func__);
		goto out;
	}
	video = (videoParams_t *)dev->videoParam;
	audio = (audioParams_t *)dev->audioParam;
	product = (productParams_t *)dev->productParam;

	if (video == NULL || product == NULL) {
		pr_err(
			"[ERROR][HDMI_V20]%s param: video_%p, product_%p \r\n",
			__func__, video, product);
		goto out;
	}

	/* Suspend status */
	if (test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
		pr_err(
			"[ERROR][HDMI_V20]%s hdmi link was suspended \r\n",
			__func__);
		goto out;
	}

	/* Power status */
	if (!test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status))
		pr_err(
			"[ERROR][HDMI_V20]%s HDMI is not powred <%d>\r\n",
			__func__, __LINE__);
	// Reset HDMI
	dwc_hdmi_hw_reset(dev, 1);
	dwc_hdmi_hw_reset(dev, 0);

	/* Initialize scdc status */
	dev->prev_scdc_status = (unsigned char)-1;

	mc_disable_all_clocks(dev);
	if (dev->hdmi_tx_ctrl.sink_need_hdcp_keepout == 1) {
		/*
		 * In general, HDCP Keepout is set to 1 when TMDS character
		 * rate is higher than 340 MHz or when HDCP is enabled.
		 * If HDCP Keepout is set to 1 then the control period
		 * configuration is changed in order to supports scramble and
		 * HDCP encryption. But some SINK needs this packet
		 * configuration always even if HDMI ouput is not scrambled or
		 * HDCP is not enabled.
		 */
		fc_video_hdcp_keepout(dev, 1);
		pr_info("[INFO][HDMI_V20]NOTIFY: HDCP_Keepout\r\n");
	} else {
		dwc_hdmi_set_hdcp_keepout(dev);
	}

	hdmi_api_avmute(dev, TRUE);

	ret = video_Configure(dev, video);
	if (ret < 0) {
		pr_err(
			"[ERROR][HDMI_V20]%s:Could not configure video",
			__func__);
		goto out;
	}

	/* Setting audio_on through hdmi mode */
	dev->hdmi_tx_ctrl.audio_on = (video->mHdmi == DVI)?0:1;

	if (audio == NULL) {
		pr_info(
			"[INFO][HDMI_V20]%s there is no audio packet\r\n",
			__func__);
	} else {
		audio_Initialize(dev);
		ret = audio_Configure(dev, audio);
		if (ret == FALSE) {
			pr_err(
				"[ERROR][HDMI_V20]%s Audio not configured\r\n",
				__func__);
			ret = -1;
			goto out;
		}
	}

	/*
	 * Source Product Description
	 * If the vendor and product names are not set, the kernel uses
	 * predefined names.
	 */
	if (
		product->mVendorNameLength == 0 &&
			product->mProductNameLength == 0) {
		strncpy(
			product->mVendorName,
			dev->vendor_name, sizeof(product->mVendorName));
		product->mVendorNameLength = strlen(product->mVendorName);
		if (product->mVendorNameLength > 8)
			product->mVendorNameLength = 8;

		strncpy(
			product->mProductName,
			dev->product_description,
			sizeof(product->mProductName));
		product->mProductNameLength = strlen(product->mProductName);
		if (product->mProductNameLength > 16)
			product->mProductNameLength = 16;

		product->mSourceType = dev->source_information;
	}

	// Packets
	if (packets_Configure(dev, video, product) < 0) {
		pr_err(
			"[ERROR][HDMI_V20]%s Could not configure packets\r\n",
			__func__);
		goto out;
	}

	mc_enable_all_clocks(dev);

	/* Configure the hdmi phy */
	if (dwc_hdmi_phy_config(dev, video) < 0) {
		pr_err(
			"[ERROR][HDMI_V20]%s Cann't settig HDMI PHY\r\n",
			__func__);
		ret = -1;
		goto out;
	}
	hdmi_api_wait_phylock(dev);

	/* Reset the main controller */
	hdmi_dev_write(dev, MC_SWRSTZREQ, 0);

	/* wait main controller to resume */
	do {
		usleep_range(10, 20);
		mc_reg_val = hdmi_dev_read(dev, MC_SWRSTZREQ);
	} while (mc_timeout-- && mc_reg_val != 0xDF);
	if (mc_timeout < 1) {
		pr_info(
			"[INFO][HDMI_V20]%s main controller timeout \r\n",
			__func__);
	}

	fc_video_VSyncPulseWidth(dev, video->mDtd.mVSyncPulseWidth);

	hdmi_hpd_enable(dev);

	set_bit(HDMI_TX_STATUS_OUTPUT_ON, &dev->status);

	ret = 0;
out:
	return ret;
}

int hdmi_api_Disable(struct hdmi_tx_dev *dev)
{
	int ret = -1;

	if (dev == NULL) {
		pr_err("[ERROR][HDMI_V20]%s dev is NULL\r\n", __func__);
		goto out;
	}

	ret = 0;

	/* Suspend status */
	if (test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
		pr_err(
			"[ERROR][HDMI_V20]%s hdmi link was suspended \r\n",
			__func__);
		goto out;
	}

	/* Power status */
	if (!test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
		pr_err(
			"[ERROR][HDMI_V20]%s HDMI is not powred <%d>\r\n",
			__func__, __LINE__);
		goto out;
	}

	/* Disable HDMI PHY clock */
	dwc_hdmi_phy_standby(dev);
	clear_bit(HDMI_TX_STATUS_OUTPUT_ON, &dev->status);

	/* Clear VSIF update function */
	clear_bit(HDMI_TX_VSIF_UPDATE_FOR_HDR_10P, &dev->status);

	/* HDCP */
	hdcp_statusinit(dev);
out:
	return ret;
}

#ifdef CONFIG_TCC_HDCP2_CORE_DRIVER
static uint32_t en_mask;
int hdmi_api_avmute_core(struct hdmi_tx_dev *dev, int enable, uint8_t caller)
{
	int ret = 0;

	if (caller > 1) {
		ret = -EINVAL;
		goto out;
	}

	if (enable)
		en_mask |= (1<<caller);
	else
		en_mask &= ~(1<<caller);

	if (dev == NULL) {
		pr_err("[ERROR][HDMI_V20]%s dev is NULL\r\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	/* Suspend status */
	if (test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
		pr_err(
			"[ERROR][HDMI_V20]%s hdmi link was suspended \r\n",
			__func__);
		ret = -EPERM;
		goto out;
	}

	/* Power status */
	if (!test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
		pr_err(
			"[ERROR][HDMI_V20]%s HDMI is not powred <%d>\r\n",
			__func__, __LINE__);
		ret = -EPERM;
		goto out;
	}

	if (enable)
		dwc_hdcp_avmute(1);

	packets_AvMute(dev, en_mask ? 1 : 0);

	if (!enable)
		dwc_hdcp_avmute(0);

out:
	return ret;
}

void hdmi_api_avmute(struct hdmi_tx_dev *dev, int enable)
{
	hdmi_api_avmute_core(dev, enable, 0);
}
#else /* CONFIG_TCC_HDCP2_CORE_DRIVER */
void hdmi_api_avmute(struct hdmi_tx_dev *dev, int enable)
{
	if (dev == NULL) {
		pr_err("[ERROR][HDMI_V20]%s dev is NULL\r\n", __func__);
		return;
	}

	/* Suspend status */
	if (test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
		pr_err(
			"[ERROR][HDMI_V20]%s hdmi link was suspended \r\n",
			__func__);
		return;
	}

	/* Power status */
	if (!test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
		pr_err(
			"[ERROR][HDMI_V20]%s HDMI is not powred <%d>\r\n",
			__func__, __LINE__);
		return;
	}

	packets_AvMute(dev, enable);
}
#endif /* CONFIG_TCC_HDCP2_CORE_DRIVER */
EXPORT_SYMBOL(hdmi_api_avmute);

