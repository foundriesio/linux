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
#include "../../include/hdcp/hdcp.h"
#include "../../include/hdcp/hdcp_reg.h"

#include "../../include/hdcp/hdcp_14.h"
#include "../../include/hdcp/hdcp_verify.h"
#include "../../include/identification/identification.h"
#include "../../include/phy/phy.h"
#include "../../../include/hdmi_log.h"

#define KSV_LEN 5 // KSV value size

/* HDCP Interrupt fields */
#define INT_KSV_ACCESS (A_APIINTSTAT_KSVACCESSINT_MASK)
#define INT_KSV_SHA1 (A_APIINTSTAT_KSVSHA1CALCINT_MASK)
#define INT_KSV_SHA1_DONE (A_APIINTSTAT_KSVSHA1CALCDONEINT_MASK)
#define INT_HDCP_FAIL (A_APIINTSTAT_HDCP_FAILED_MASK)
#define INT_HDCP_ENGAGED (A_APIINTSTAT_HDCP_ENGAGED_MASK)

void _setDeviceMode(struct hdmi_tx_dev *dev, video_mode_t mode)
{
	u8 set_mode = (mode == HDMI ? 1 : 0); // 1 - HDMI : 0 - DVI
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_HDMIDVI_MASK, set_mode);
}

void _EnableFeature11(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_EN11FEATURE_MASK, bit);
}

void hdcp_rxdetect(struct hdmi_tx_dev *dev, u8 enable)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_RXDETECT_MASK, enable);
}

void _EnableAvmute(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_AVMUTE_MASK, bit);
}

void _RiCheck(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_SYNCRICHECK_MASK, bit);
}

void _BypassEncryption(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_BYPENCRYPTION_MASK, bit);
}

void _EnableI2cFastMode(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_I2CFASTMODE_MASK, bit);
}

void _EnhancedLinkVerification(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_ELVENA_MASK, bit);
}

void hdcp_sw_reset(struct hdmi_tx_dev *dev)
{
	// Software reset signal, active by writing a zero and auto cleared to 1 in the following cycle
	hdmi_dev_write_mask(dev, A_HDCPCFG1, A_HDCPCFG1_SWRESET_MASK, 0);
}

void _DisableEncryption(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG1, A_HDCPCFG1_ENCRYPTIONDISABLE_MASK, bit);
}

void _EncodingPacketHeader(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG1, A_HDCPCFG1_PH2UPSHFTENC_MASK, bit);
}

void _DisableKsvListCheck(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_HDCPCFG1, A_HDCPCFG1_DISSHA1CHECK_MASK, bit);
}

u8 _HdcpEngaged(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, A_HDCPOBS0, A_HDCPOBS0_HDCPENGAGED_MASK);
}

u8 _AuthenticationState(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, A_HDCPOBS0, A_HDCPOBS0_SUBSTATEA_MASK | A_HDCPOBS0_STATEA_MASK);
}

u8 _CipherState(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, A_HDCPOBS2, A_HDCPOBS2_STATEE_MASK);
}

u8 _RevocationState(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, (A_HDCPOBS1), A_HDCPOBS1_STATER_MASK);
}

u8 _OessState(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, (A_HDCPOBS1), A_HDCPOBS1_STATEOEG_MASK);
}

u8 _EessState(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read_mask(dev, (A_HDCPOBS2), A_HDCPOBS2_STATEEEG_MASK);
}

u8 _DebugInfo(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, A_HDCPOBS3);
}

void _InterruptClear(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write(dev, (A_APIINTCLR), value);
}

u8 _InterruptStatus(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, A_APIINTSTAT);
}

void _InterruptMask(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write(dev, (A_APIINTMSK), value);
}

u8 _InterruptMaskStatus(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, A_APIINTMSK);
}

void _HSyncPolarity(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_VIDPOLCFG, A_VIDPOLCFG_HSYNCPOL_MASK, bit);
}

void _VSyncPolarity(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_VIDPOLCFG, A_VIDPOLCFG_VSYNCPOL_MASK, bit);
}

void _DataEnablePolarity(struct hdmi_tx_dev *dev, u8 bit)
{
	hdmi_dev_write_mask(dev, A_VIDPOLCFG, A_VIDPOLCFG_DATAENPOL_MASK, bit);
}

void _UnencryptedVideoColor(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write_mask(dev, A_VIDPOLCFG, A_VIDPOLCFG_UNENCRYPTCONF_MASK, value);
}

void _OessWindowSize(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write(dev, (A_OESSWCFG), value);
}

u16 _CoreVersion(struct hdmi_tx_dev *dev)
{
	u16 version = 0;
	version = hdmi_dev_read(dev, A_COREVERLSB);
	version |= hdmi_dev_read(dev, A_COREVERMSB) << 8;
	return version;
}

u8 _ControllerVersion(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, A_HDCPCFG0);
}

void _MemoryAccessRequest(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE();
	hdmi_dev_write_mask(dev, A_KSVMEMCTRL, A_KSVMEMCTRL_KSVMEMREQUEST_MASK, bit);
}

u8 _MemoryAccessGranted(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	return (u8)((hdmi_dev_read(dev, A_KSVMEMCTRL) & A_KSVMEMCTRL_KSVMEMACCESS_MASK) >> 1);
}

void _UpdateKsvListState(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, A_KSVMEMCTRL, A_KSVMEMCTRL_SHA1FAIL_MASK, bit);
	hdmi_dev_write_mask(dev, A_KSVMEMCTRL, A_KSVMEMCTRL_KSVCTRLUPD_MASK, 1);
	hdmi_dev_write_mask(dev, A_KSVMEMCTRL, A_KSVMEMCTRL_KSVCTRLUPD_MASK, 0);
}

u8 _ksv_sha1_status(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	return (u8)((hdmi_dev_read(dev, A_KSVMEMCTRL) & A_KSVMEMCTRL_KSVSHA1STATUS_MASK) >> 4);
}

u16 _BStatusRead(struct hdmi_tx_dev *dev)
{
	u16 bstatus = 0;

	bstatus = hdmi_dev_read(dev, HDCP_BSTATUS);
	bstatus |= hdmi_dev_read(dev, HDCP_BSTATUS + ADDR_JUMP) << 8;
	return bstatus;
}

void _M0Read(struct hdmi_tx_dev *dev, u8 *data)
{
	u8 i = 0;
	for (i = 0; i < HDCP_M0_SIZE; i++) {
		data[i] = hdmi_dev_read(dev, HDCP_M0 + (i * ADDR_JUMP));
	}
}

void _SHA1VHRead(struct hdmi_tx_dev *dev, u8 *data)
{
	u8 i = 0;
	for (i = 0; i < HDCP_VH_SIZE; i++) {
		data[i] = hdmi_dev_read(dev, HDCP_VH + (i * ADDR_JUMP));
	}
}

void _RevocListWrite(struct hdmi_tx_dev *dev, u16 addr, u8 data)
{
	LOG_TRACE2(addr, data);
	hdmi_dev_write(dev, HDCP_REVOC_LIST + addr, data);
}

void _AnWrite(struct hdmi_tx_dev *dev, u8 *data)
{
	short i = 0;
	LOG_TRACE();
	if (data != 0) {
		LOG_TRACE1(data[0]);
		for (i = 0; i <= (HDCPREG_AN7 - HDCPREG_AN0); i++) {
			hdmi_dev_write(dev, (HDCPREG_AN0 + (i * ADDR_JUMP)), data[i]);
		}
		hdmi_dev_write_mask(dev, HDCPREG_ANCONF, HDCPREG_ANCONF_OANBYPASS_MASK, 1);
	} else {
		hdmi_dev_write_mask(dev, HDCPREG_ANCONF, HDCPREG_ANCONF_OANBYPASS_MASK, 0);
	}
}

u8 _BksvRead(struct hdmi_tx_dev *dev, u8 *bksv)
{
	short i = 0;
	if (bksv != 0) {
		LOG_TRACE1(bksv[0]);
		for (i = 0; i <= (HDCPREG_BKSV4 - HDCPREG_BKSV0); i++) {
			bksv[i] = hdmi_dev_read(dev, HDCPREG_BKSV0 + (i * 4));
		}
		return i;
	} else {
		return 0;
	}
}

u8 _HDCP22CtrlRegReset(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write(dev, HDCP22REG_CTRL, 0);

	return 0;
}

u8 _HDCP22RegStatusRead(struct hdmi_tx_dev *dev)
{
	return (u8)(hdmi_dev_read(dev, HDCP22REG_STS));
}

u8 _HDCP22RegMuteRead(struct hdmi_tx_dev *dev)
{
	return (u8)(hdmi_dev_read(dev, HDCP22REG_MUTE));
}

u8 _HDCP22RegMask(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write(dev, (HDCP22REG_MASK), value);
	return 0;
}
EXPORT_SYMBOL(_HDCP22RegMask);

u8 _HDCP22RegMute(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write(dev, (HDCP22REG_MUTE), value);
	return 0;
}
EXPORT_SYMBOL(_HDCP22RegMute);

u8 _HDCP22RegStat(struct hdmi_tx_dev *dev, u8 value)
{
	hdmi_dev_write(dev, (HDCP22REG_STAT), value);
	return 0;
}

u8 _HDCP22RegReadStat(struct hdmi_tx_dev *dev)
{
	return (u8)(hdmi_dev_read(dev, HDCP22REG_STAT));
}

u8 _HDCP22RegMaskRead(struct hdmi_tx_dev *dev)
{
	return (u8)(hdmi_dev_read(dev, HDCP22REG_MASK));
}

void hdcp_statusinit(struct hdmi_tx_dev *dev)
{
	dev->hdcp_status = HDCP_IDLE;
}

u8 hdcp1p4SwitchSet(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HDCP22_SWITCH_LCK_MASK, 0);
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HDCP22_OVR_EN_MASK, 1);
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HDCP22_OVR_VAL_MASK, 0);
	return 0;
}
EXPORT_SYMBOL(hdcp1p4SwitchSet);

u8 hdcp2p2SwitchSet(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HPD_OVR_EN_MASK, 1);
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HPD_OVR_VAL_MASK, 1);
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HDCP22_SWITCH_LCK_MASK, 0);
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HDCP22_OVR_EN_MASK, 1);
	hdmi_dev_write_mask(dev, HDCP22REG_CTRL, HDCP22REG_CTRL_HDCP22_OVR_VAL_MASK, 1);
	return 0;
}
EXPORT_SYMBOL(hdcp2p2SwitchSet);

int hdcpAuthGetStatus(struct hdmi_tx_dev *dev)
{
	return dev->hdcp_status;
}
EXPORT_SYMBOL(hdcpAuthGetStatus);

int hdcp1p4Configure(struct hdmi_tx_dev *dev, hdcpParams_t * hdcp)
{
	hdcp_rxdetect(dev, 0);
	_DataEnablePolarity(dev, dev->hdmi_tx_ctrl.data_enable_polarity);
	_DisableEncryption(dev, 1);

	video_mode_t mode = hdcp->iHdmiMode;
	u8 hsPol = hdcp->uHSPol;
	u8 vsPol = hdcp->uVSPol;
	static int hdcp_2p2 = 0;

	if (dev->hdmi_tx_ctrl.hdcp_on == 0) {
		LOGGER(SNPS_WARN, "HDCP is not active");
		return TRUE;
	}

	// Before configure HDCP we should configure the internal parameters
	hdcp->maxDevices = 128;
	hdcp->mI2cFastMode = 0;

	// 1 - To determine if the controller supports HDCP
	if (id_product_type(dev) != 0xC1) {
		LOGGER(SNPS_ERROR, "Controller does not supports HDCP");
		return FALSE;
	}

	// 2 - To determine the HDCP version of the transmitter
	if (id_hdcp22_support(dev) == FALSE) {
		LOGGER(SNPS_DEBUG, "HDCP 2.2 SNPS is not present (HDCP 1.4 support only)");
		hdcp_2p2 = 0;
	} else {
		LOGGER(
			SNPS_DEBUG, "HDCP 2.2 SNPS is present (both HDCP 1.4 and HDCP 2.2 versions supported)");
		hdcp_2p2 = 1;
	}

	// 3 - Select DVI or HDMI mode
	LOGGER(SNPS_DEBUG, "Set HDCP %s", mode == HDMI ? "HDMI" : "DVI");
	hdmi_dev_write_mask(dev, A_HDCPCFG0, A_HDCPCFG0_HDMIDVI_MASK, (mode == HDMI) ? 1 : 0);

	// 4 - Set the Data enable, Hsync, and VSync polarity
	_HSyncPolarity(dev, (hsPol > 0) ? 1 : 0);
	_VSyncPolarity(dev, (vsPol > 0) ? 1 : 0);
	_DataEnablePolarity(dev, (dev->hdmi_tx_ctrl.data_enable_polarity > 0) ? 1 : 0);

	// a. If HDCP2Version is 0x00, write “0” to mc_opctrl.h22s_ovr_val (this selects HDCP 1.4)
	hdmi_dev_write_mask(dev, 0x1000C, 1 << 5, 0x0);

	return TRUE;
}
EXPORT_SYMBOL(hdcp1p4Configure);

void hdcp1p4Start(struct hdmi_tx_dev *dev, hdcpParams_t *hdcp)
{
	_EnableFeature11(dev, (hdcp->mEnable11Feature > 0) ? 1 : 0);
	_RiCheck(dev, (hdcp->mRiCheck > 0) ? 1 : 0);
	_EnableI2cFastMode(dev, (hdcp->mI2cFastMode > 0) ? 1 : 0);
	_EnhancedLinkVerification(dev, (hdcp->mEnhancedLinkVerification > 0) ? 1 : 0);

	/* fixed */
	_EnableAvmute(dev, FALSE);
	_UnencryptedVideoColor(dev, 0x00);
	_EncodingPacketHeader(dev, TRUE);

	// 9 - Set encryption
	_OessWindowSize(dev, 64);
	_BypassEncryption(dev, FALSE);
	_DisableEncryption(dev, TRUE);

	// 10 - Reset the HDCP 1.4 engine
	hdcp_sw_reset(dev);

	// 11 - Configure Device Private Keys (required when DWC_HDMI_HDCP_DPK_ROMLESS configuration
	//	parameter is set to True [1]), which is illustrated in Figure 3-7 on page 82. For required
	//  memory contents, refer to the “HDCP DPK 56-bit Memory Mapping” table in Chapter 2 of the
	//  DesignWare HDMI Transmitter Controller Databook.

	// 12 - Enable encryption
	if (hdcp->bypass)
		hdcp_rxdetect(dev, 0);
	else
		hdcp_rxdetect(dev, 1);

	LOGGER(SNPS_DEBUG, "HDCP enable interrupts");
	_InterruptClear(
		dev,
		A_APIINTCLR_HDCP_FAILED_MASK | A_APIINTCLR_HDCP_ENGAGED_MASK
			| A_APIINTSTAT_KSVSHA1CALCDONEINT_MASK);
	/* enable KSV list SHA1 verification interrupt */
	_InterruptMask(
		dev,
		(~(A_APIINTMSK_HDCP_FAILED_MASK | A_APIINTMSK_HDCP_ENGAGED_MASK
		   | A_APIINTSTAT_KSVSHA1CALCDONEINT_MASK))
			& _InterruptMaskStatus(dev));
}
EXPORT_SYMBOL(hdcp1p4Start);

void hdcp1p4Stop(struct hdmi_tx_dev *dev)
{
	hdcp_disable_encryption(dev, TRUE);
	hdcp_rxdetect(dev, 0);
	_InterruptMask(dev, 0xFF);
	hdcp_interrupt_clear(dev, 0xFF);
}
EXPORT_SYMBOL(hdcp1p4Stop);

void hdcp2p2Stop(struct hdmi_tx_dev *dev)
{
	u8 interrupt_status = 0;

	// interrupt clear
	_HDCP22RegStat(dev, interrupt_status);

	interrupt_status = _HDCP22RegReadStat(dev);

	if (interrupt_status == 0) {
		dev->hdcp_status = HDCP_IDLE;
		_HDCP22RegMask(dev, 0x3F);
		_HDCP22RegMute(dev, 0x3F);
		_HDCP22RegStat(dev, 0x3F);
		// printk("HDCP_IDLE - interrupt_status : %d \n", interrupt_status);
	}
}
EXPORT_SYMBOL(hdcp2p2Stop);

u8 hdcp22_event_handler(struct hdmi_tx_dev *dev, int *param)
{
	u8 interrupt_status = 0;
	int valid = HDCP_IDLE;
	interrupt_status = _HDCP22RegReadStat(dev);

	if (interrupt_status == 0) {
		LOGGER(SNPS_TRACE, "HDCP_IDLE");
		dev->hdcp_status = HDCP_IDLE;
		printk(KERN_INFO "HDCP_IDLE - interrupt_status : %d \n", interrupt_status);
		return HDCP_IDLE;
	}
	// interrupt clear
	_HDCP22RegStat(dev, interrupt_status);

	if (interrupt_status & HDCP22REG_STAT_ST_HDCP2_CAPABLE_MASK) {
		dev->hdcp_status = HDCP2_CAPABLE;
		printk(KERN_INFO "%s:%s\n", __func__, "HDCP22REG_STAT_ST_HDCP2_CAPABLE");
	}
	if (interrupt_status & HDCP22REG_STAT_ST_HDCP2_NOT_CAPABLE_MASK) {
		dev->hdcp_status = HDCP2_NOT_CAPABLE;
		printk(KERN_INFO "%s:%s\n", __func__, "HDCP22REG_STAT_ST_HDCP2_NOT_CAPABLE");
	}
	if (interrupt_status & HDCP22REG_STAT_ST_HDCP_AUTHENTICATION_LOST_MASK) {
		dev->hdcp_status = HDCP2_AUTHENTICATION_LOST;
		printk("%s:%s\n", __func__, "HDCP22REG_STAT_ST_HDCP_AUTHENTICATION_LOST");
		if (dev->hotplug_status == 0 || hdmi_phy_get_rx_sense_status(dev) == 0) {
			printk(KERN_INFO
				"%s:%s, hpd : %d, rxsense : %d\n", __func__, "HDCP_IDLE", dev->hotplug_status,
				dev->rxsense);
			dev->hdcp_status = HDCP_IDLE;
		}
	}
	if (interrupt_status & HDCP22REG_STAT_ST_HDCP_AUTHENTICATED_MASK) {
		dev->hdcp_status = HDCP2_AUTHENTICATED;
		printk(KERN_INFO "%s:%s\n", __func__, "HDCP22REG_STAT_ST_HDCP_AUTHENTICATED");
	}
	if (interrupt_status & HDCP22REG_STAT_ST_HDCP_AUTHENTICATION_FAIL_MASK) {
		dev->hdcp_status = HDCP2_AUTHENTICATION_FAIL;
		printk(KERN_INFO "%s:%s\n", __func__, "HDCP22REG_STAT_ST_HDCP_AUTHENTICATION_FAIL");
		if (dev->hotplug_status == 0 || hdmi_phy_get_rx_sense_status(dev) == 0) {
			printk(KERN_INFO
				"%s:%s, hpd : %d, rxsense : %d\n", __func__, "HDCP_IDLE", dev->hotplug_status,
				dev->rxsense);
			dev->hdcp_status = HDCP_IDLE;
		}
	}
	if (interrupt_status & HDCP22REG_STAT_ST_HDCP_DECRYPTED_CHG_MASK) {
		printk(KERN_INFO "%s:%s\n", __func__, "HDCP22REG_STAT_ST_HDCP_DECRYPTED_CHG");
		dev->hdcp_status = HDCP2_DECRYPTED_CHG;
	}

	_HDCP22RegMask(dev, ~interrupt_status & _HDCP22RegMaskRead(dev));
	_HDCP22RegMute(dev, ~interrupt_status & _HDCP22RegMuteRead(dev));
	return valid;
}

u8 hdcp_event_handler(struct hdmi_tx_dev *dev, int *param)
{
	u8 interrupt_status = 0;
	int valid = HDCP_IDLE;

	LOG_TRACE();
	interrupt_status = hdcp_interrupt_status(dev);

	if (interrupt_status == 0) {
		LOGGER(SNPS_TRACE, "HDCP_IDLE");
		printk(KERN_INFO "HDCP_IDLE - interrupt_status : %d \n", interrupt_status);
		dev->hdcp_status = HDCP_IDLE;
		return HDCP_IDLE;
	}

	hdcp_interrupt_clear(dev, interrupt_status);

	if (dev->hotplug_status == 0 || hdmi_phy_get_rx_sense_status(dev) == 0) {
		printk(KERN_INFO
			"%s:%s, hpd : %d, rxsense : %d\n", __func__, "HDCP_IDLE", dev->hotplug_status,
			dev->rxsense);
		dev->hdcp_status = HDCP_IDLE;
	} else {
		if (interrupt_status & INT_KSV_SHA1_DONE) {
			printk(KERN_INFO "INT_KSV_SHA1_DONE - Status %d\n", _ksv_sha1_status(dev));
			dev->hdcp_status = HDCP_KSV_LIST_READY;
			valid = HDCP_KSV_LIST_READY;
		}

		if ((interrupt_status & INT_HDCP_FAIL) != 0) {
			*param = 0;
			if (hdmi_dev_read_mask(dev, A_HDCPCFG1, A_HDCPCFG1_ENCRYPTIONDISABLE_MASK) == 0) {
				_DisableEncryption(dev, TRUE);
			}
			printk(KERN_INFO "HDCP 1.4 Authentication process - HDCP_FAILED \n");
			dev->hdcp_status = HDCP_FAILED;
			valid = HDCP_FAILED;
		}

		if ((interrupt_status & INT_HDCP_ENGAGED) != 0) {
			*param = 1;
			if (hdmi_dev_read_mask(dev, A_HDCPCFG1, A_HDCPCFG1_ENCRYPTIONDISABLE_MASK) == 1) {
				_DisableEncryption(dev, FALSE);
			}
			printk(KERN_INFO "HDCP 1.4 Authentication process - HDCP_ENGAGED \n");
			dev->hdcp_status = HDCP_ENGAGED;
			valid = HDCP_ENGAGED;
		}

#if 0
	if ((state & A_APIINTSTAT_KEEPOUTERRORINT_MASK) != 0)
		LOGGER(SNPS_ERROR,"keep out error interrupt");

	if ((state & A_APIINTSTAT_LOSTARBITRATION_MASK) != 0)
		LOGGER(SNPS_ERROR,"lost arbitration error interrupt");
		valid = HDCP_IDLE;

	if ((state & A_APIINTSTAT_I2CNACK_MASK) != 0)
		LOGGER(SNPS_ERROR,"i2c nack error interrupt");
		valid = HDCP_IDLE;

#endif
	}
	_InterruptMask(dev, ~interrupt_status & _InterruptMaskStatus(dev));
	return valid;
}

void hdcp_av_mute(struct hdmi_tx_dev *dev, int enable)
{
/*
	if (dev->hdmi_tx_ctrl.hdcp_on == 1) {
		_EnableAvmute(dev, (enable == TRUE) ? 1 : 0);
	} else if (dev->hdmi_tx_ctrl.hdcp_on == 2) {
		hdmi_dev_write_mask(dev, HDCP22REG_CTRL1, HDCP22REG_CTRL1_HDCP22_AVMUTE_OVR_EN_MASK, 1);
		hdmi_dev_write_mask(
			dev, HDCP22REG_CTRL1, HDCP22REG_CTRL1_HDCP22_AVMUTE_OVR_VAL_MASK, enable);
	}
*/
}

void hdcp_disable_encryption(struct hdmi_tx_dev *dev, int disable)
{
	LOG_TRACE1(disable);
	_DisableEncryption(dev, (disable == TRUE) ? 1 : 0);
}
EXPORT_SYMBOL(hdcp_disable_encryption);

u8 hdcp_interrupt_status(struct hdmi_tx_dev *dev)
{
	return _InterruptStatus(dev);
}

int hdcp_interrupt_clear(struct hdmi_tx_dev *dev, u8 value)
{
	_InterruptClear(dev, value);
	return TRUE;
}
