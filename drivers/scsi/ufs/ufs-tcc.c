/*
 * Telechips UFS Driver
 *
 * Copyright (C) 2020- Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA V
 */

#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "unipro.h"
#include "ufs-tcc.h"
#include "ufshci.h"

#define UNIPRO_PCLK_PERIOD_NS 6
#define CALCULATED_VALUE 0x4E20

static unsigned int debug;

static int ufs_tcc_get_resource(struct ufs_tcc_host *host)
{
	struct resource *mem_res;
	struct device *dev = host->hba->dev;
	struct platform_device *pdev = to_platform_device(dev);

	mem_res =
		platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "ufs-unipro");
	host->ufs_reg_unipro = devm_ioremap_resource(dev, mem_res);
	dev_dbg(dev,
		"%s:%d mem_res = %08x\n",
		__func__, __LINE__, host->ufs_reg_unipro);
	if (host->ufs_reg_unipro == NULL) {
		dev_err(dev,
		"cannot ioremap for ufs unipro register\n");
		return -ENOMEM;
	}

	mem_res =
		platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "ufs-mphy");
	host->ufs_reg_mphy = devm_ioremap_resource(dev, mem_res);
	dev_dbg(dev,
		"%s:%d mem_res = %08x\n",
		__func__, __LINE__, host->ufs_reg_mphy);

	if (host->ufs_reg_mphy == NULL) {
		dev_err(dev, "cannot ioremap for ufs mphy register\n");
		return -ENOMEM;
	}

	mem_res =
		platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "ufs-sbus-config");
	host->ufs_reg_sbus_config =
		devm_ioremap_resource(dev, mem_res);
	dev_dbg(dev,
		"%s:%d mem_res = %08x\n",
		__func__, __LINE__, host->ufs_reg_sbus_config);
	if (host->ufs_reg_sbus_config == NULL) {
		dev_err(dev,
		"cannot ioremap for ufs sbus config register\n");
		return -ENOMEM;
	}

	mem_res =
		platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "ufs-fmp");
	host->ufs_reg_fmp = devm_ioremap_resource(dev, mem_res);
	dev_dbg(dev,
		"%s:%d mem_Res = %08x\n",
		__func__, __LINE__, host->ufs_reg_fmp);
	if (host->ufs_reg_fmp == NULL) {
		dev_err(dev, "cannot ioremap for ufs fmp register\n");
		return -ENOMEM;
	}

	mem_res =
		platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "ufs-sec");
	host->ufs_reg_sec = devm_ioremap_resource(dev, mem_res);
	dev_dbg(dev,
		"%s:%d mem_Res = %08x\n",
		__func__, __LINE__, host->ufs_reg_sec);
	if (host->ufs_reg_sec == NULL) {
		dev_err(dev, "cannot ioremap for ufs fmp register\n");
		return -ENOMEM;
	}
	return 0;
}

static void ufs_tcc_set_pm_lvl(struct ufs_hba *hba)
{
	hba->rpm_lvl = UFS_PM_LVL_1;
	hba->spm_lvl = UFS_PM_LVL_3;
}

static void encryption_setting(struct ufs_hba *hba)
{
	// Step#1: In-Line Encryption Sequence
	ufshcd_writel(hba, 0x0, HCI_CRYPTOCFG__CFGE_CAPIDX_DUSIZE);

	ufshcd_writel(hba, 0x00000003, HCI_CRYPTOCFG_CRYPTOKEY_0);
	ufshcd_writel(hba, 0x0000000F, HCI_CRYPTOCFG_CRYPTOKEY_1);
	ufshcd_writel(hba, 0x00000030, HCI_CRYPTOCFG_CRYPTOKEY_2);
	ufshcd_writel(hba, 0x000000F0, HCI_CRYPTOCFG_CRYPTOKEY_3);
	ufshcd_writel(hba, 0x00000300, HCI_CRYPTOCFG_CRYPTOKEY_4);
	ufshcd_writel(hba, 0x00000F00, HCI_CRYPTOCFG_CRYPTOKEY_5);
	ufshcd_writel(hba, 0x00003000, HCI_CRYPTOCFG_CRYPTOKEY_6);
	ufshcd_writel(hba, 0x0000F000, HCI_CRYPTOCFG_CRYPTOKEY_7);
	ufshcd_writel(hba, 0x00030000, HCI_CRYPTOCFG_CRYPTOKEY_8);
	ufshcd_writel(hba, 0x000F0000, HCI_CRYPTOCFG_CRYPTOKEY_9);
	ufshcd_writel(hba, 0x00300000, HCI_CRYPTOCFG_CRYPTOKEY_A);
	ufshcd_writel(hba, 0x00F00000, HCI_CRYPTOCFG_CRYPTOKEY_B);
	ufshcd_writel(hba, 0x03000000, HCI_CRYPTOCFG_CRYPTOKEY_C);
	ufshcd_writel(hba, 0x0F000000, HCI_CRYPTOCFG_CRYPTOKEY_D);
	ufshcd_writel(hba, 0x30000000, HCI_CRYPTOCFG_CRYPTOKEY_E);
	ufshcd_writel(hba, 0xF0000000, HCI_CRYPTOCFG_CRYPTOKEY_F);
}

static int tcc_ufs_smu_setting(struct ufs_hba *hba)
{
	unsigned int smu_bypass = 1;
	unsigned int smu_index  = 0;
	unsigned int desc_type  = 0;
	unsigned int tid, sw, sr, nsw, nsr, ufk, enc, valid;
	unsigned int protbytzpc, select_inline_enc,
		fmp_on, unmap_disable, nskeyreg,
		nssmu, nsuser, use_otp_mask;

	unsigned int fmp_bsector;
	unsigned int fmp_esector;
	unsigned int fmp_lun;
	unsigned int fmp_ctrl;
	unsigned int fmp_security;

	struct ufs_tcc_host *host = ufshcd_get_variant(hba);

	encryption_setting(hba);

	tid   = 0;
	sw    = 1;
	sr    = 1;
	nsw   = 1;
	nsr   = 1;
	ufk   = 1;
	enc   = 0;
	valid = 1;

	fmp_bsector = 0x0;
	fmp_esector = 0xFFFFFFFF;
	fmp_lun     = 0x7;
	fmp_ctrl =
		(tid   << 8U) | //tid
		(sw    << 7U) | //sw
		(sr    << 6U) | //sr
		(nsw   << 5U) | //nsw
		(nsr   << 4U) | //nsr
		(ufk   << 3U) | //ufk
		(enc   << 1U) | //enc
		(valid << 0U) ; //valid

	ufs_fmp_writel(host, 0x00000001, FMP_FMPDEK0);
	ufs_fmp_writel(host, 0x00000010, FMP_FMPDEK1);
	ufs_fmp_writel(host, 0x00000100, FMP_FMPDEK2);
	ufs_fmp_writel(host, 0x00001000, FMP_FMPDEK3);
	ufs_fmp_writel(host, 0x00010000, FMP_FMPDEK4);
	ufs_fmp_writel(host, 0x00100000, FMP_FMPDEK5);
	ufs_fmp_writel(host, 0x01000000, FMP_FMPDEK6);
	ufs_fmp_writel(host, 0x10000000, FMP_FMPDEK7);

	ufs_fmp_writel(host, 0x00000001, FMP_FMPDTK0);
	ufs_fmp_writel(host, 0x00000010, FMP_FMPDTK1);
	ufs_fmp_writel(host, 0x00000100, FMP_FMPDTK2);
	ufs_fmp_writel(host, 0x00001000, FMP_FMPDTK3);
	ufs_fmp_writel(host, 0x00010000, FMP_FMPDTK4);
	ufs_fmp_writel(host, 0x00100000, FMP_FMPDTK5);
	ufs_fmp_writel(host, 0x01000000, FMP_FMPDTK6);
	ufs_fmp_writel(host, 0x10000000, FMP_FMPDTK7);

	ufs_fmp_writel(host, 0x00000001, FMP_FMPFEKM0);
	ufs_fmp_writel(host, 0x00000010, FMP_FMPFEKM1);
	ufs_fmp_writel(host, 0x00000100, FMP_FMPFEKM2);
	ufs_fmp_writel(host, 0x00001000, FMP_FMPFEKM3);
	ufs_fmp_writel(host, 0x00010000, FMP_FMPFEKM4);
	ufs_fmp_writel(host, 0x00100000, FMP_FMPFEKM5);
	ufs_fmp_writel(host, 0x01000000, FMP_FMPFEKM6);
	ufs_fmp_writel(host, 0x10000000, FMP_FMPFEKM7);

	ufs_fmp_writel(host, 0x00000001, FMP_FMPFTKM0);
	ufs_fmp_writel(host, 0x00000010, FMP_FMPFTKM1);
	ufs_fmp_writel(host, 0x00000100, FMP_FMPFTKM2);
	ufs_fmp_writel(host, 0x00001000, FMP_FMPFTKM3);
	ufs_fmp_writel(host, 0x00010000, FMP_FMPFTKM4);
	ufs_fmp_writel(host, 0x00100000, FMP_FMPFTKM5);
	ufs_fmp_writel(host, 0x01000000, FMP_FMPFTKM6);
	ufs_fmp_writel(host, 0x10000000, FMP_FMPFTKM7);

	ufs_fmp_writel(host, 0x00000001, FMP_FMPSCTRL0);
	ufs_fmp_writel(host, 0x00000010, FMP_FMPSCTRL1);
	ufs_fmp_writel(host, 0x00000100, FMP_FMPSCTRL2);
	ufs_fmp_writel(host, 0x00001000, FMP_FMPSCTRL3);
	ufs_fmp_writel(host, 0x00010000, FMP_FMPSCTRL4);
	ufs_fmp_writel(host, 0x00100000, FMP_FMPSCTRL5);
	ufs_fmp_writel(host, 0x01000000, FMP_FMPSCTRL6);
	ufs_fmp_writel(host, 0x10000000, FMP_FMPSCTRL7);

	switch (smu_index) {
	case 0:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN0);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND0);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN0);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL0);
		break;
	case 1:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN1);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND1);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN1);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL1);
		break;
	case 2:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN2);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND2);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN2);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL2);
		break;
	case 3:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN3);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND3);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN3);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL3);
		break;
	case 4:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN4);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND4);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN4);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL4);
		break;
	case 5:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN5);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND5);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN5);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL5);
		break;

	case 6:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN6);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND6);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN6);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL6);
		break;

	case 7:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN7);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND7);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN7);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL7);
		break;

	default:
		ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN7);
		ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND7);
		ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN7);
		ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL7);
		break;
	}

	// --------------------------------------------
	// Write to FMPSECURITY
	// --------------------------------------------
	protbytzpc        = 0x0;
	select_inline_enc = 0x1;
	fmp_on            = 0x1;
	unmap_disable     = 0x1;
	nskeyreg          = 0x1;
	nssmu             = 0x1;
	nsuser            = 0x1;
	use_otp_mask      = 0x0;

	fmp_security = (protbytzpc         << 31U) |
		(select_inline_enc  << 30U) |
		(fmp_on             << 29U) |
		(unmap_disable      << 28U) |
		((unsigned int)0x3F	             << 22U) |
		(desc_type          << 19U) |
		((unsigned int)0x5                << 16U) |
		(nskeyreg           << 15U) |
		(nssmu              << 14U) |
		(nsuser             << 13U) |
		(use_otp_mask       << 12U) |
		((unsigned int)0x5                <<  9U) |
		((unsigned int)0x5                <<  6U) |
		((unsigned int)0x5                <<  3U) |
		((unsigned int)0x5                <<  0U);


	ufs_fmp_writel(host, fmp_security, FMP_FMPRSECURITY);
	return 0;
}

static void ufs_tcc_pre_init(struct ufs_hba *hba)
{
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);

	ufs_sbus_config_clr_bits(host, SBUS_CONFIG_SWRESETN_UFS_HCI |
		SBUS_CONFIG_SWRESETN_UFS_PHY, SBUS_CONFIG_SWRESETN);
	mdelay(1);
	ufs_sbus_config_set_bits(host, SBUS_CONFIG_SWRESETN_UFS_HCI |
		SBUS_CONFIG_SWRESETN_UFS_PHY, SBUS_CONFIG_SWRESETN);
	ufshcd_writel(hba, 0x1, HCI_MPHY_REFCLK_SEL);
	ufshcd_writel(hba, 0x0, HCI_CLKSTOP_CTRL);
	ufshcd_writel(hba, 0x1, HCI_GPIO_OUT);
	ufshcd_writel(hba, 0x1, HCI_UFS_ACG_DISABLE);
}

static void ufs_tcc_post_init(struct ufs_hba *hba)
{
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);
	unsigned int data = 0;

	data = ufshcd_readl(hba, REG_CONTROLLER_ENABLE);
	while (data != 0x1U) {
		data = ufshcd_readl(hba, REG_CONTROLLER_ENABLE);
	}

	data = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
	while (reg_rdb((data), (3U)) != 1U) {
		data = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
	}

	ufshcd_writel(hba, 0x7FFF, REG_INTERRUPT_ENABLE);


	ufs_unipro_writel(host,
		UNIPRO_PCLK_PERIOD_NS, PA_DBG_CLK_PERIOD);
	ufs_unipro_writel(host,
		CALCULATED_VALUE, PA_DBG_AUTOMODE_THLD);
	ufs_unipro_writel(host,
		0x2E820183, PA_DBG_OPTION_SUITE);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)PA_Local_TX_LCC_Enable,
		0x0U), 0x0U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)N_DeviceID,
		0x0U), N_DEVICE_ID_VAL);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)N_DeviceID_valid,
		0x0U), N_DEVICE_ID_VALID_VAL);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)T_ConnectionState,
		0x0U), T_CONNECTION_STATE_OFF_VAL);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)T_PeerDeviceID,
		0x0U), T_PEER_DEVICE_ID_VAL);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)T_ConnectionState,
		0x0U), T_CONNECTION_STATE_ON_VAL);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x0200,
		0x0U), 0x3fU);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x8f,
		TX_LANE_0), 0x0U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x8f,
		RX_LANE_1), 0x0U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x0f,
		RX_LANE_0), 0x0U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x0f,
		RX_LANE_1), 0x0U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x21,
		RX_LANE_0), 0x0U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x21,
		RX_LANE_1), 0x0U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x22,
		RX_LANE_0), 0x0U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x22,
		RX_LANE_1), 0x0U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x5c,
		RX_LANE_0), 0x38U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x5c,
		RX_LANE_1), 0x38U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x62,
		RX_LANE_0), 0x97U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x62,
		RX_LANE_1), 0x97U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x63,
		RX_LANE_0), 0x70U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x63,
		RX_LANE_1), 0x70U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x65,
		RX_LANE_0), 0x1U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x65,
		RX_LANE_1), 0x1U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x69,
		RX_LANE_0), 0x1U);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x69,
		RX_LANE_1), 0x1U);

	ufshcd_dme_set(hba,
		UIC_ARG_MIB_SEL((unsigned int)0x200,
		0x0U), 0x0U);

	ufshcd_writel(hba, 0xA, HCI_DATA_REORDER);

}
static int ufs_tcc_hce_enable_notify(struct ufs_hba *hba,
				enum ufs_notify_change_status status)
{
	int ret = 0;

	switch (status) {
	case PRE_CHANGE:
		ufs_tcc_pre_init(hba);
		break;
	case POST_CHANGE:
		ufs_tcc_post_init(hba);
		break;
	default:
		break;
	}

	return 0;
}

static int ufs_tcc_link_startup_post_change(struct ufs_hba *hba)
{
	uint32_t  rd_data;
	uint32_t  cport_log_en = 0;
	uint32_t  wlu_enable = 0;
	uint32_t  wlu_burst_len = 3;
	uint32_t  hci_buffering_enable = 0;
	uint32_t  axidma_rwdataburstlen = 0;
	uint32_t  no_of_beat_burst = 7;
	int res = 0;
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);

	ufshcd_writel(hba, UTRIACR_VAL, HCI_UTRIACR);
	ufshcd_writel(hba, UTMRLBA_LOW_VAL, HCI_UTMRLBA);
	ufshcd_writel(hba, UTMRLBA_HIGH_VAL, HCI_UTMRLBAU);

	ufshcd_writel(hba, UTRLBA_LOW_VAL, HCI_UTRLBA);
	ufshcd_writel(hba, UTRLBA_HIGH_VAL, HCI_UTRLBAU);

	ufshcd_writel(hba, 0xC, HCI_TXPRDT_ENTRY_SIZE);
	ufshcd_writel(hba, 0xC, HCI_RXPRDT_ENTRY_SIZE);

	ufshcd_writel(hba, 0x1, HCI_UTMRLRSR);
	ufshcd_writel(hba, 0x1, HCI_UTRLRSR);

	axidma_rwdataburstlen = no_of_beat_burst;
	axidma_rwdataburstlen = (axidma_rwdataburstlen & 0xFFFFFFF0);

	tcc_ufs_smu_setting(hba);

	return res;
}

static int ufs_tcc_link_startup_notify(struct ufs_hba *hba,
		enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		err = ufs_tcc_link_startup_post_change(hba);
		break;
	default:
		break;
	}

	return err;
}


/**
 * ufs_telechips_init
 * @hba: host controller instance
 */
static int ufs_tcc_init(struct ufs_hba *hba)
{
	int err;
	struct device *dev = hba->dev;
	struct ufs_tcc_host *host;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (host == NULL) {
		return -ENOMEM;
	}
	hba->quirks |= 0x00000080U;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	host->rst = devm_reset_control_get(dev, "rst");
	host->assert = devm_reset_control_get(dev, "assert");

	ufs_tcc_set_pm_lvl(hba);

	err = ufs_tcc_get_resource(host);
	if (err != 0) {
		ufshcd_set_variant(hba, NULL);
		return err;
	}

	return 0;
}
static int ufs_tcc_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	ufshcd_writel(hba, 0x0, HCI_HCE);
	if (ufshcd_is_runtime_pm(pm_op))
		return 0;
	if (host->in_suspend) {
		WARN_ON(1);
		return 0;
	}
	host->in_suspend = true;
	return ret;
}
static int ufs_tcc_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	ufs_sbus_config_clr_bits(host,
		SBUS_CONFIG_SWRESETN_UFS_HCI | SBUS_CONFIG_SWRESETN_UFS_PHY,
		SBUS_CONFIG_SWRESETN);
	mdelay(1);
	ufs_sbus_config_set_bits(host,
		SBUS_CONFIG_SWRESETN_UFS_HCI | SBUS_CONFIG_SWRESETN_UFS_PHY,
		SBUS_CONFIG_SWRESETN);
	ufshcd_writel(hba, 0x1, HCI_MPHY_REFCLK_SEL);
	host->in_suspend = false;
	return ret;
}
static struct ufs_hba_variant_ops ufs_hba_tcc_vops = {
	.name = "tcc",
	.init = ufs_tcc_init,
	.link_startup_notify = ufs_tcc_link_startup_notify,
	.hce_enable_notify = ufs_tcc_hce_enable_notify,
	//.pwr_change_notify = ufs_tcc_pwr_change_notify,
	.suspend = ufs_tcc_suspend,
	.resume = ufs_tcc_resume,
};

static int ufs_tcc_probe(struct platform_device *pdev)
{
	return ufshcd_pltfrm_init(pdev, &ufs_hba_tcc_vops);
}

static int ufs_tcc_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	ufshcd_remove(hba);
	return 0;
}

static const struct of_device_id ufs_tcc_of_match[] = {
	{ .compatible = "telechips,tcc-ufs" },
	{},
};

static const struct dev_pm_ops ufs_tcc_pm_ops = {
	.suspend        = ufshcd_pltfrm_suspend,
	.resume         = ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume  = ufshcd_pltfrm_runtime_resume,
	.runtime_idle    = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_tcc_pltform = {
	.probe  = ufs_tcc_probe,
	.remove = ufs_tcc_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver = {
		.name   = "ufshcd-tcc",
		.pm     = &ufs_tcc_pm_ops,
		.of_match_table = of_match_ptr(ufs_tcc_of_match),
	},
};
module_platform_driver(ufs_tcc_pltform);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ufshcd-tcc");
MODULE_DESCRIPTION("Telechips UFS Driver");
