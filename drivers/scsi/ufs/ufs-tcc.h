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

#ifndef UFS_TCC_H_
#define UFS_TCC_H_

#define MAX_NO_OF_RX_LANES      2
#define MAX_NO_OF_TX_LANES      2
//typedef enum { G1 = 1, G2, G3, G4 } gear_t;
//typedef enum { RATE_A = 1, RATE_B } hs_series_t;
//typedef enum {FAST=1, SLOW, FAST_AUTO=4, SLOW_AUTO, UNCHANGED=7} pwrmode_t;

//---------------------------------------------------------
//Macro Definition
//---------------------------------------------------------
#define reg_setb(x, y) ((x) |= ((1U) << (y)))
#define reg_clrb(x, y) ((x) &= ~((1U) << (y)))
#define reg_rdb(x, y)  (((0U) == ((x) & ((1U)<<(y)))) ? (0U) : (1U))

//------------------------------------------------------
// HCI Reigster Definition
//------------------------------------------------------
#define UFS_UNIPRO_BASE    0x1D270000U
#define UFS_MPHY_BASE      0x1D274000U
#define UFS_HCI_BASE       0x1D280000U
#define UFS_FMP_BASE       0x1D290000U

#define HCI_CAP                                (0x0000U)
#define HCI_VER                                (0x0008U)
#define HCI_HCPID                              (0x0010U)
#define HCI_HCMID                              (0x0014U)
#define HCI_AHIT                               (0x0018U)
#define HCI_IS                                 (0x0020U)
#define HCI_IE                                 (0x0024U)
#define HCI_HCS                                (0x0030U)
#define HCI_HCE                                (0x0034U)
#define HCI_UECPA                              (0x0038U)
#define HCI_UECDL                              (0x003cU)
#define HCI_UECN                               (0x0040U)
#define HCI_UECT                               (0x0044U)
#define HCI_UECDME                             (0x0048U)
#define HCI_UTRIACR                            (0x004cU)
#define HCI_UTRLBA                             (0x0050U)
#define HCI_UTRLBAU                            (0x0054U)
#define HCI_UTRLDBR                            (0x0058U)
#define HCI_UTRLCLR                            (0x005cU)
#define HCI_UTRLRSR                            (0x0060U)
#define HCI_UTRLCNR                            (0x0064U)
#define HCI_UTMRLBA                            (0x0070U)
#define HCI_UTMRLBAU                           (0x0074U)
#define HCI_UTMRLDBR                           (0x0078U)
#define HCI_UTMRLCLR                           (0x007cU)
#define HCI_UTMRLRSR                           (0x0080U)
#define HCI_UICCMDR                            (0x0090U)
#define HCI_UICCMDARG1                         (0x0094U)
#define HCI_UICCMDARG2                         (0x0098U)
#define HCI_UICCMDARG3                         (0x009cU)
#define HCI_CCAP                               (0x0100U)
#define HCI_CRYPTOCAP_0                        (0x0104U)
#define HCI_CRYPTOCAP_1                        (0x0108U)
#define HCI_CRYPTOCFG_CRYPTOKEY_0              (0x0400U)
#define HCI_CRYPTOCFG_CRYPTOKEY_1              (0x0404U)
#define HCI_CRYPTOCFG_CRYPTOKEY_2              (0x0408U)
#define HCI_CRYPTOCFG_CRYPTOKEY_3              (0x040cU)
#define HCI_CRYPTOCFG_CRYPTOKEY_4              (0x0410U)
#define HCI_CRYPTOCFG_CRYPTOKEY_5              (0x0414U)
#define HCI_CRYPTOCFG_CRYPTOKEY_6              (0x0418U)
#define HCI_CRYPTOCFG_CRYPTOKEY_7              (0x041cU)
#define HCI_CRYPTOCFG_CRYPTOKEY_8              (0x0420U)
#define HCI_CRYPTOCFG_CRYPTOKEY_9              (0x0424U)
#define HCI_CRYPTOCFG_CRYPTOKEY_A              (0x0428U)
#define HCI_CRYPTOCFG_CRYPTOKEY_B              (0x042cU)
#define HCI_CRYPTOCFG_CRYPTOKEY_C              (0x0430U)
#define HCI_CRYPTOCFG_CRYPTOKEY_D              (0x0434U)
#define HCI_CRYPTOCFG_CRYPTOKEY_E              (0x0438U)
#define HCI_CRYPTOCFG_CRYPTOKEY_F              (0x043cU)
#define HCI_CRYPTOCFG__CFGE_CAPIDX_DUSIZE      (0x0440U)
#define HCI_CRYPTOCFG__VSB                     (0x0444U)
#define HCI_TXPRDT_ENTRY_SIZE                  (0x1100U)
#define HCI_RXPRDT_ENTRY_SIZE                  (0x1104U)
#define HCI_TO_CNT_VAL_1US                     (0x110cU)
#define HCI_INVALID_UPIU_CTRL                  (0x1110U)
#define HCI_INVALID_UPIU_BADDR                 (0x1114U)
#define HCI_INVALID_UPIU_UBADDR                (0x1118U)
#define HCI_INVALID_UTMR_OFFSET_ADDR           (0x111cU)
#define HCI_INVALID_UTR_OFFSET_ADDR            (0x1120U)
#define HCI_INVALID_DIN_OFFSET_ADDR            (0x1124U)
#define HCI_VENDOR_SPECIFIC_IS                 (0x1138U)
#define HCI_VENDOR_SPECIFIC_IE                 (0x113cU)
#define HCI_UTRL_NEXUS_TYPE                    (0x1140U)
#define HCI_UTMRL_NEXUS_TYPE                   (0x1144U)
#define HCI_SW_RST                             (0x1150U)
#define HCI_UFS_LINK_VERSION                   (0x1154U)
#define HCI_RS_FMPIU_MATCH_ERROR_CODE          (0x115cU)
#define HCI_DATA_REORDER                       (0x1160U)
#define HCI_AXIDMA_RWDATA_BURST_LEN            (0x116cU)
#define HCI_GPIO_OUT                           (0x1170U)
#define HCI_WRITE_DMA_CTRL                     (0x1174U)
#define HCI_DFES_ERROR_EN_PA_LAYER             (0x1178U)
#define HCI_DFES_ERROR_EN_DL_LAYER             (0x117cU)
#define HCI_DFES_ERROR_EN_N_LAYER              (0x1180U)
#define HCI_DFES_ERROR_EN_T_LAYER              (0x1184U)
#define HCI_DFES_ERROR_EN_DME_LAYER            (0x1188U)
#define HCI_UFSHCI_V2P1_CTRL                   (0x118cU)
#define HCI_REQ_HOLD                           (0x11acU)
#define HCI_CLKSTOP_CTRL                       (0x11b0U)
#define HCI_MISC                               (0x11b4U)
#define HCI_PRDT_HIT_RATIO                     (0x11c4U)
#define HCI_DMA0_MONITOR_STATE                 (0x11c8U)
#define HCI_DMA0_MONITOR_CNT                   (0x11ccU)
#define HCI_DMA1_MONITOR_STATE                 (0x11d0U)
#define HCI_DMA1_MONITOR_CNT                   (0x11d4U)
#define HCI_AXI_DMA_IF_CTRL                    (0x11f8U)
#define HCI_UFS_ACG_DISABLE                    (0x11fcU)
#define HCI_IOP_CG_DISABLE                     (0x1200U)
#define HCI_UFS_UDPD                           (0x1204U)
#define HCI_MPHY_REFCLK_SEL                    (0x1208U)
#define HCI_SMU_ABORT_MATCH_INFO               (0x120cU)
#define HCI_CPORT_LOG_CTRL                     (0x1210U)
#define HCI_CPORT_LOG_CFG                      (0x1214U)
#define HCI_DBR_DUPLICATION_INFO               (0x1220U)
#define HCI_DBR_TIMER_CONFIG                   (0x1240U)
#define HCI_DBR_TIMER_ENABLE                   (0x1244U)
#define HCI_DBR_TIMER_STATUS                   (0x1248U)
#define HCI_UTRL_DBR_3_0_TIMER_EXPIRED_VALUE   (0x1250U)
#define HCI_UTRL_DBR_7_4_TIMER_EXPIRED_VALUE   (0x1254U)
#define HCI_UTRL_DBR_11_8_TIMER_EXPIRED_VALUE  (0x1258U)
#define HCI_UTRL_DBR_15_12_TIMER_EXPIRED_VALUE (0x125cU)
#define HCI_UTMRL_DBR_3_0_TIMER_EXPIRED_VALUE  (0x1260U)
#define HCI_CPORT_TX_LOG_FIFO0_0               (0x8000U)
#define HCI_CPORT_TX_LOG_FIFO7_7               (0x80fcU)
#define HCI_CPORT_RX_LOG_FIFO0_0               (0x8100U)
#define HCI_CPORT_RX_LOG_FIFO7_7               (0x81fcU)
#define HCI_UTRL0_0_LOG                        (0x8200U)
#define HCI_UTRL15_7_LOG                       (0x83fcU)
#define HCI_UTR_UCD0_0_LOG                     (0x8400U)
#define HCI_UTR_UCD15_7_LOG                    (0x85fcU)
#define HCI_UTMRL0_0_LOG                       (0x8600U)
#define HCI_UTMRL3_B_LOG                       (0x86bcU)
#define HCI_CPORT_LOG_PTR                      (0x8800U)

//-----------------------------------------------------------
// UNIPRO Reigster Definition
//-----------------------------------------------------------
#define UP_N_STD_REGION_N_DEVICEID                  (0x5000U)
#define UP_N_STD_REGION_N_DEVICEID_VALID            (0x5004U)
#define UP_N_STD_REGION_N_TC0TXMAXSDUSIZE           (0x5080U)
#define UP_N_STD_REGION_N_TC1TXMAXSDUSIZE           (0x5084U)
#define UP_DL_STD_REGION_DL_TXPREEMPTIONCAP         (0x4000U)
#define UP_DL_STD_REGION_DL_TC0TXMAXSDUSIZE         (0x4004U)
#define UP_DL_STD_REGION_DL_TC0RXINITCREDITVAL      (0x4008U)
#define UP_DL_STD_REGION_DL_TC1TXMAXSDUSIZE         (0x400cU)
#define UP_DL_STD_REGION_DL_TC1RXINITCREDITVAL      (0x4010U)
#define UP_DL_STD_REGION_DL_TC0TXBUFFERSIZE         (0x4014U)
#define UP_DL_STD_REGION_DL_TC1TXBUFFERSIZE         (0x4018U)
#define UP_DL_STD_REGION_DL_TC0TXFCTHRESHOLD        (0x4100U)
#define UP_DL_STD_REGION_DL_FC0PROTECTIONTIMEOUTVAL (0x4104U)
#define UP_DL_STD_REGION_DL_TC0REPLAYTIMEOUTVAL     (0x4108U)
#define UP_DL_STD_REGION_DL_AFC0REQTIMEOUTVAL       (0x410cU)
#define UP_DL_STD_REGION_DL_AFC0CREDITTHRESHOLD     (0x4110U)
#define UP_DL_STD_REGION_DL_TC0OUTACKTHRESHOLD      (0x4114U)
#define UP_DL_STD_REGION_DL_PEERTC0PRESENT          (0x4118U)
#define UP_DL_STD_REGION_DL_PEERTC0RXINITCREDITVAL  (0x411cU)
#define UP_DL_STD_REGION_DL_TC1TXFCTHRESHOLD        (0x4180U)
#define UP_DL_STD_REGION_DL_FC1PROTECTIONTIMEOUTVAL (0x4184U)
#define UP_DL_STD_REGION_DL_TC1REPLAYTIMEOUTVAL     (0x4188U)
#define UP_DL_STD_REGION_DL_AFC1REQTIMEOUTVAL       (0x418cU)
#define UP_DL_STD_REGION_DL_AFC1CREDITTHRESHOLD     (0x4190U)
#define UP_DL_STD_REGION_DL_TC1OUTACKTHRESHOLD      (0x4194U)
#define UP_DL_STD_REGION_DL_PEERTC1PRESENT          (0x4198U)
#define UP_DL_STD_REGION_DL_PEERTC1RXINITCREDITVAL  (0x419cU)

//-------------------------------
// FMP Reigster Definition
//-------------------------------
#define FMP_FMPRCTRL     (0x0000U)
#define FMP_FMPRSTAT     (0x0008U)
#define FMP_FMPRSECURITY (0x0010U)
#define FMP_FMPVERSION   (0x001cU)
#define FMP_FMPDEK0      (0x0020U)
#define FMP_FMPDEK1      (0x0024U)
#define FMP_FMPDEK2      (0x0028U)
#define FMP_FMPDEK3      (0x002cU)
#define FMP_FMPDEK4      (0x0030U)
#define FMP_FMPDEK5      (0x0034U)
#define FMP_FMPDEK6      (0x0038U)
#define FMP_FMPDEK7      (0x003cU)
#define FMP_FMPDTK0      (0x0040U)
#define FMP_FMPDTK1      (0x0044U)
#define FMP_FMPDTK2      (0x0048U)
#define FMP_FMPDTK3      (0x004cU)
#define FMP_FMPDTK4      (0x0050U)
#define FMP_FMPDTK5      (0x0054U)
#define FMP_FMPDTK6      (0x0058U)
#define FMP_FMPDTK7      (0x005cU)
#define FMP_FMPWCTRL     (0x0100U)
#define FMP_FMPWSTAT     (0x0108U)
#define FMP_FMPFEKM0     (0x0120U)
#define FMP_FMPFEKM1     (0x0124U)
#define FMP_FMPFEKM2     (0x0128U)
#define FMP_FMPFEKM3     (0x012cU)
#define FMP_FMPFEKM4     (0x0130U)
#define FMP_FMPFEKM5     (0x0134U)
#define FMP_FMPFEKM6     (0x0138U)
#define FMP_FMPFEKM7     (0x013cU)
#define FMP_FMPFTKM0     (0x0140U)
#define FMP_FMPFTKM1     (0x0144U)
#define FMP_FMPFTKM2     (0x0148U)
#define FMP_FMPFTKM3     (0x014cU)
#define FMP_FMPFTKM4     (0x0150U)
#define FMP_FMPFTKM5     (0x0154U)
#define FMP_FMPFTKM6     (0x0158U)
#define FMP_FMPFTKM7     (0x015cU)
#define FMP_FMPSBEGIN0   (0x0200U)
#define FMP_FMPSEND0     (0x0204U)
#define FMP_FMPSLUN0     (0x0208U)
#define FMP_FMPSCTRL0    (0x020cU)
#define FMP_FMPSBEGIN1   (0x0210U)
#define FMP_FMPSEND1     (0x0214U)
#define FMP_FMPSLUN1     (0x0218U)
#define FMP_FMPSCTRL1    (0x021cU)
#define FMP_FMPSBEGIN2   (0x0220U)
#define FMP_FMPSEND2     (0x0224U)
#define FMP_FMPSLUN2     (0x0228U)
#define FMP_FMPSCTRL2    (0x022cU)
#define FMP_FMPSBEGIN3   (0x0230U)
#define FMP_FMPSEND3     (0x0234U)
#define FMP_FMPSLUN3     (0x0238U)
#define FMP_FMPSCTRL3    (0x023cU)
#define FMP_FMPSBEGIN4   (0x0240U)
#define FMP_FMPSEND4     (0x0244U)
#define FMP_FMPSLUN4     (0x0248U)
#define FMP_FMPSCTRL4    (0x024cU)
#define FMP_FMPSBEGIN5   (0x0250U)
#define FMP_FMPSEND5     (0x0254U)
#define FMP_FMPSLUN5     (0x0258U)
#define FMP_FMPSCTRL5    (0x025cU)
#define FMP_FMPSBEGIN6   (0x0260U)
#define FMP_FMPSEND6     (0x0264U)
#define FMP_FMPSLUN6     (0x0268U)
#define FMP_FMPSCTRL6    (0x026cU)
#define FMP_FMPSBEGIN7   (0x0270U)
#define FMP_FMPSEND7     (0x0274U)
#define FMP_FMPSLUN7     (0x0278U)
#define FMP_FMPSCTRL7    (0x027cU)

//-------------------------------
// PMA Reigster Definition
//-------------------------------
#define PMA_CMN_REG00   (0x0000U)
#define PMA_CMN_REG01   (0x0004U)
#define PMA_CMN_REG02   (0x0008U)
#define PMA_CMN_REG03   (0x000cU)
#define PMA_CMN_REG04   (0x0010U)
#define PMA_CMN_REG05   (0x0014U)
#define PMA_CMN_REG06   (0x0018U)
#define PMA_CMN_REG07   (0x001cU)
#define PMA_CMN_REG08   (0x0020U)
#define PMA_CMN_REG09   (0x0024U)
#define PMA_CMN_REG0A   (0x0028U)
#define PMA_CMN_REG0B   (0x002cU)
#define PMA_CMN_REG0C   (0x0030U)
#define PMA_CMN_REG0D   (0x0034U)
#define PMA_CMN_REG0E   (0x0038U)
#define PMA_CMN_REG0F   (0x003cU)
#define PMA_CMN_REG10   (0x0040U)
#define PMA_CMN_REG11   (0x0044U)
#define PMA_CMN_REG12   (0x0048U)
#define PMA_CMN_REG13   (0x004cU)
#define PMA_CMN_REG14   (0x0050U)
#define PMA_CMN_REG15   (0x0054U)
#define PMA_CMN_REG16   (0x0058U)
#define PMA_CMN_REG17   (0x005cU)
#define PMA_CMN_REG18   (0x0060U)
#define PMA_CMN_REG19   (0x0064U)
#define PMA_CMN_REG1E   (0x0078U)
#define PMA_CMN_REG1F   (0x007cU)
#define PMA_TRSV0_REG00 (0x00c0U)
#define PMA_TRSV0_REG01 (0x00c4U)
#define PMA_TRSV0_REG02 (0x00c8U)
#define PMA_TRSV0_REG03 (0x00ccU)
#define PMA_TRSV0_REG04 (0x00d0U)
#define PMA_TRSV0_REG05 (0x00d4U)
#define PMA_TRSV0_REG06 (0x00d8U)
#define PMA_TRSV0_REG07 (0x00dcU)
#define PMA_TRSV0_REG08 (0x00e0U)
#define PMA_TRSV0_REG09 (0x00e4U)
#define PMA_TRSV0_REG0A (0x00e8U)
#define PMA_TRSV0_REG0B (0x00ecU)
#define PMA_TRSV0_REG0C (0x00f0U)
#define PMA_TRSV0_REG0D (0x00f4U)
#define PMA_TRSV0_REG0E (0x00f8U)
#define PMA_TRSV0_REG0F (0x00fcU)
#define PMA_TRSV0_REG10 (0x0100U)
#define PMA_TRSV0_REG11 (0x0104U)
#define PMA_TRSV0_REG12 (0x0108U)
#define PMA_TRSV0_REG13 (0x010cU)
#define PMA_TRSV0_REG14 (0x0110U)
#define PMA_TRSV0_REG15 (0x0114U)
#define PMA_TRSV0_REG16 (0x0118U)
#define PMA_TRSV0_REG17 (0x011cU)
#define PMA_TRSV0_REG18 (0x0120U)
#define PMA_TRSV0_REG19 (0x0124U)
#define PMA_TRSV0_REG1A (0x0128U)
#define PMA_TRSV0_REG1B (0x012cU)
#define PMA_TRSV0_REG1C (0x0130U)
#define PMA_TRSV0_REG1D (0x0134U)
#define PMA_TRSV0_REG1E (0x0138U)
#define PMA_TRSV0_REG1F (0x013cU)
#define PMA_TRSV0_REG20 (0x0140U)
#define PMA_TRSV0_REG21 (0x0144U)
#define PMA_TRSV0_REG22 (0x0148U)
#define PMA_TRSV0_REG23 (0x014cU)
#define PMA_TRSV0_REG24 (0x0150U)
#define PMA_TRSV0_REG25 (0x0154U)
#define PMA_TRSV0_REG26 (0x0158U)
#define PMA_TRSV0_REG27 (0x015cU)
#define PMA_TRSV0_REG28 (0x0160U)
#define PMA_TRSV0_REG29 (0x0164U)
#define PMA_TRSV0_REG2A (0x0168U)
#define PMA_TRSV0_REG2B (0x016cU)
#define PMA_TRSV0_REG2C (0x0170U)
#define PMA_TRSV0_REG2D (0x0174U)
#define PMA_TRSV0_REG2E (0x0178U)
#define PMA_TRSV0_REG2F (0x017cU)
#define PMA_TRSV0_REG3D (0x01b4U)
#define PMA_TRSV0_REG3E (0x01b8U)
#define PMA_TRSV0_REG3F (0x01bcU)
#define PMA_TRSV1_REG00 (0x01c0U)
#define PMA_TRSV1_REG01 (0x01c4U)
#define PMA_TRSV1_REG02 (0x01c8U)
#define PMA_TRSV1_REG03 (0x01ccU)
#define PMA_TRSV1_REG04 (0x01d0U)
#define PMA_TRSV1_REG05 (0x01d4U)
#define PMA_TRSV1_REG06 (0x01d8U)
#define PMA_TRSV1_REG07 (0x01dcU)
#define PMA_TRSV1_REG08 (0x01e0U)
#define PMA_TRSV1_REG09 (0x01e4U)
#define PMA_TRSV1_REG0A (0x01e8U)
#define PMA_TRSV1_REG0B (0x01ecU)
#define PMA_TRSV1_REG0C (0x01f0U)
#define PMA_TRSV1_REG0D (0x01f4U)
#define PMA_TRSV1_REG0E (0x01f8U)
#define PMA_TRSV1_REG0F (0x01fcU)
#define PMA_TRSV1_REG10 (0x0200U)
#define PMA_TRSV1_REG11 (0x0204U)
#define PMA_TRSV1_REG12 (0x0208U)
#define PMA_TRSV1_REG13 (0x020cU)
#define PMA_TRSV1_REG14 (0x0210U)
#define PMA_TRSV1_REG15 (0x0214U)
#define PMA_TRSV1_REG16 (0x0218U)
#define PMA_TRSV1_REG17 (0x021cU)
#define PMA_TRSV1_REG18 (0x0220U)
#define PMA_TRSV1_REG19 (0x0224U)
#define PMA_TRSV1_REG1A (0x0228U)
#define PMA_TRSV1_REG1B (0x022cU)
#define PMA_TRSV1_REG1C (0x0230U)
#define PMA_TRSV1_REG1D (0x0234U)
#define PMA_TRSV1_REG1E (0x0238U)
#define PMA_TRSV1_REG1F (0x023cU)
#define PMA_TRSV1_REG20 (0x0240U)
#define PMA_TRSV1_REG21 (0x0244U)
#define PMA_TRSV1_REG22 (0x0248U)
#define PMA_TRSV1_REG23 (0x024cU)
#define PMA_TRSV1_REG24 (0x0250U)
#define PMA_TRSV1_REG25 (0x0254U)
#define PMA_TRSV1_REG26 (0x0258U)
#define PMA_TRSV1_REG27 (0x025cU)
#define PMA_TRSV1_REG28 (0x0260U)
#define PMA_TRSV1_REG29 (0x0264U)
#define PMA_TRSV1_REG2A (0x0268U)
#define PMA_TRSV1_REG2B (0x026cU)
#define PMA_TRSV1_REG2C (0x0270U)
#define PMA_TRSV1_REG2D (0x0274U)
#define PMA_TRSV1_REG2E (0x0278U)
#define PMA_TRSV1_REG2F (0x027cU)
#define PMA_TRSV1_REG3D (0x02b4U)
#define PMA_TRSV1_REG3E (0x02b8U)
#define PMA_TRSV1_REG3F (0x02bcU)

//--------------------------------------------------------
//Ufs Specific Defines
//--------------------------------------------------------
#define TX_HSMODE_Capability                     0x0001U
#define TX_HSGEAR_Capability                     0x0002U
#define TX_PWMG0_Capability                      0x0003U
#define TX_PWMGEAR_Capability                    0x0004U
#define TX_Amplitude_Capability                  0x0005U
#define TX_ExternalSYNC_Capability               0x0006U
#define TX_HS_Unterminated_LINE_Drive_Capability 0x0007U
#define TX_LS_Terminated_LINE_Drive_Capability   0x0008U
#define TX_Min_SLEEP_NoConfig_Time_Capability    0x0009U
#define TX_Min_STALL_NoConfig_Time_Capability    0x000AU
#define TX_Min_SAVE_Config_Time_Capability       0x000BU
#define TX_REF_CLOCK_SHARED_Capability           0x000CU
#define TX_Hibern8Time_Capability                0x000FU
#define TX_Advanced_Granularity_Capability       0x0010U
#define TX_Advanced_Hibern8Time_Capability       0x0011U
//#define TX_MODE                                  0x0021U
#define TX_HSRATE_Series                         0x0022U
//#define TX_HSGEAR                                0x0023U
//#define TX_PWMGEAR                               0x0024U
#define TX_Amplitude                             0x0025U
#define TX_HS_SlewRate                           0x0026U
#define TX_SYNC_Source                           0x0027U
//#define TX_HS_SYNC_LENGTH                        0x0028U
//#define TX_HS_PREPARE_LENGTH                     0x0029U
//#define TX_LS_PREPARE_LENGTH                     0x002AU
#define TX_HIBERN8_Control                       0x002BU
#define TX_LCC_Enable                            0x002CU
#define TX_PWM_BURST_Closure_Extension           0x002DU
#define TX_BYPASS_8B10B_Enable                   0x002EU
//#define TX_DRIVER_POLARITY                       0x002FU
#define TX_HS_Unterminated_LINE_Drive_Enable     0x0030U
#define TX_LS_Terminated_LINE_Drive_Enable       0x0031U
#define TX_LCC_Sequencer                         0x0032U
//#define RX_MODE                                  0x00A1U
#define RX_HSRATE_Series                         0x00A2U
//#define RX_HSGEAR                                0x00A3U
//#define RX_PWMGEAR                               0x00A4U
#define RX_LS_Terminated_Enable                  0x00A5U
#define RX_HS_Unterminated_Enable                0x00A6U
#define RX_Enter_HIBERN8                         0x00A7U
#define RX_BYPASS_8B10B_Enable                   0x00A8U
#define TX_FSM_State                             0x0041U
#define RX_FSM_State                             0x00C1U
#define OMC_TYPE_Capability                      0x00D1U
#define MC_HSMODE_Capability                     0x00D2U
#define MC_HSGEAR_Capability                     0x00D3U
#define MC_HS_START_TIME_Var_Capability          0x00D4U
#define MC_HS_START_TIME_Range_Capability        0x00D5U
#define MC_RX_SA_Capability                      0x00D6U
#define MC_RX_LA_Capability                      0x00D7U
#define MC_LS_PREPARE_LENGTH                     0x00D8U
#define MC_PWMG0_Capability                      0x00D9U
#define MC_PWMGEAR_Capability                    0x00DAU
#define MC_LS_Terminated_Capability              0x00DBU
#define MC_HS_Unterminated_Capability            0x00DCU
#define MC_LS_Terminated_LINE_Drive_Capability   0x00DDU
#define MC_HS_Unterminated_LINE_Drive_Capability 0x00DEU
#define MC_MFG_ID_Part1                          0x00DFU
#define MC_MFG_ID_Part2                          0x00E0U
#define MC_PHY_MajorMinor_Release_Capability     0x00E1U
#define MC_PHY_Editorial_Release_Capability      0x00E2U
#define MC_Ext_Vendor_Info_Part1                 0x00E3U
#define MC_Ext_Vendor_Info_Part2                 0x00E4U
#define MC_Ext_Vendor_Info_Part3                 0x00E5U
#define MC_Ext_Vendor_Info_Part4                 0x00E6U
#define MC_Output_Amplitude                      0x0061U
#define MC_HS_Unterminated_Enable                0x0062U
#define MC_LS_Terminated_Enable                  0x0063U
#define MC_HS_Unterminated_LINE_Drive_Enable     0x0064U
#define MC_LS_Terminated_LINE_Drive_Enable       0x0065U
#define RX_HSMODE_Capability                     0x0081U
#define RX_HSGEAR_Capability                     0x0082U
#define RX_PWMG0_Capability                      0x0083U
#define RX_PWMGEAR_Capability                    0x0084U
#define RX_HS_Unterminated_Capability            0x0085U
#define RX_LS_Terminated_Capability              0x0086U
#define RX_Min_SLEEP_NoConfig_Time_Capability    0x0087U
#define RX_Min_STALL_NoConfig_Time_Capability    0x0088U
#define RX_Min_SAVE_Config_Time_Capability       0x0089U
#define RX_REF_CLOCK_SHARED_Capability           0x008AU
#define RX_HS_G1_SYNC_LENGTH_Capability          0x008BU
#define RX_HS_G1_PREPARE_LENGTH_Capability       0x008CU
#define RX_LS_PREPARE_LENGTH_Capability          0x008DU
#define RX_PWM_Burst_Closure_Length_Capability   0x008EU
#define RX_Min_ActivateTime_Capability           0x008FU
#define RX_Hibern8Time_Capability                0x0092U
#define RX_PWM_G6_G7_SYNC_LENGTH_Capability      0x0093U
#define RX_HS_G2_SYNC_LENGTH_Capability          0x0094U
#define RX_HS_G3_SYNC_LENGTH_Capability          0x0095U
#define RX_HS_G2_PREPARE_LENGTH_Capability       0x0096U
#define RX_HS_G3_PREPARE_LENGTH_Capability       0x0097U
#define RX_Advanced_Granularity_Capability       0x0098U
#define RX_Advanced_Hibern8Time_Capability       0x0099U
#define RX_Advanced_Min_ActivateTime_Capability  0x009AU
#define L0_TX_Capability                         0x0101U
#define L0_TX_OptCapability1                     0x0102U
#define L0_TX_OptCapability2                     0x0103U
#define L0_TX_OptCapability3                     0x0104U
#define L0_OMC_WO                                0x0105U
#define L0_RX_Capability                         0x0106U
#define L0_RX_OptCapability1                     0x0107U
#define L0_RX_OptCapability2                     0x0108U
#define L0_RX_OptCapability3                     0x0109U
#define L0_RX_OptCapability4                     0x010AU
#define L0_RX_OptCapability5                     0x010BU
#define L0_OMC_READ_DATA0                        0x010CU
#define L0_OMC_READ_DATA1                        0x010DU
#define L0_OMC_READ_DATA2                        0x010EU
#define L0_OMC_READ_DATA3                        0x010FU
#define L1_TX_Capability                         0x0111U
#define L1_TX_OptCapability1                     0x0112U
#define L1_TX_OptCapability2                     0x0113U
#define L1_TX_OptCapability3                     0x0114U
#define L1_OMC_WO                                0x0115U
#define L1_RX_Capability                         0x0116U
#define L1_RX_OptCapability1                     0x0117U
#define L1_RX_OptCapability2                     0x0118U
#define L1_RX_OptCapability3                     0x0119U
#define L1_RX_OptCapability4                     0x011AU
#define L1_RX_OptCapability5                     0x011BU
#define L1_OMC_READ_DATA0                        0x011CU
#define L1_OMC_READ_DATA1                        0x011DU
#define L1_OMC_READ_DATA2                        0x011EU
#define L1_OMC_READ_DATA3                        0x011FU
#define L2_TX_Capability                         0x0121U
#define L2_TX_OptCapability1                     0x0122U
#define L2_TX_OptCapability2                     0x0123U
#define L2_TX_OptCapability3                     0x0124U
#define L2_OMC_WO                                0x0125U
#define L2_RX_Capability                         0x0126U
#define L2_RX_OptCapability1                     0x0127U
#define L2_RX_OptCapability2                     0x0128U
#define L2_RX_OptCapability3                     0x0129U
#define L2_RX_OptCapability4                     0x012AU
#define L2_RX_OptCapability5                     0x012BU
#define L2_OMC_READ_DATA0                        0x012CU
#define L2_OMC_READ_DATA1                        0x012DU
#define L2_OMC_READ_DATA2                        0x012EU
#define L2_OMC_READ_DATA3                        0x012FU
#define L3_TX_Capability                         0x0131U
#define L3_TX_OptCapability1                     0x0132U
#define L3_TX_OptCapability2                     0x0133U
#define L3_TX_OptCapability3                     0x0134U
#define L3_OMC_WO                                0x0135U
#define L3_RX_Capability                         0x0136U
#define L3_RX_OptCapability1                     0x0137U
#define L3_RX_OptCapability2                     0x0138U
#define L3_RX_OptCapability3                     0x0139U
#define L3_RX_OptCapability4                     0x013AU
#define L3_RX_OptCapability5                     0x013BU
#define L3_OMC_READ_DATA0                        0x013CU
#define L3_OMC_READ_DATA1                        0x013DU
#define L3_OMC_READ_DATA2                        0x013EU
#define L3_OMC_READ_DATA3                        0x013FU
#define Selector                                 0x0160U
#define MAP_RX_LANE0                             0x0170U
#define MAP_RX_LANE1                             0x0171U
#define MAP_RX_LANE2                             0x0172U
#define MAP_RX_LANE3                             0x0173U
#define MAP_TX_LANE0                             0x0174U
#define MAP_TX_LANE1                             0x0175U
#define MAP_TX_LANE2                             0x0176U
#define MAP_TX_LANE3                             0x0177U
#define PWRMODE_MODE_ERR                         0x0180U
#define PWRMODE_SERIE_ERR                        0x0181U
#define PWRMODE_GEAR_ERR                         0x0182U
#define PWRMODE_HIBERNATE_ERR                    0x0183U
#define PWRMODE_TERMINATION_ERR                  0x0184U
#define PWRMODE_CTRL                             0x0185U
#define L0RXPowerMode                            0x018AU
#define L1RXPowerMode                            0x018BU
#define L2RXPowerMode                            0x018CU
#define L3RXPowerMode                            0x018DU
#define PA_PHY_Type                              0x1500U
#define PA_AvailTxDataLanes                      0x1520U
#define PA_AvailRxDataLanes                      0x1540U
#define PA_MinRxTrailingClocks                   0x1543U
#define PA_TxHsG1SyncLength                      0x1552U
#define PA_TxHsG1PrepareLength                   0x1553U
#define PA_TxHsG2SyncLength                      0x1554U
#define PA_TxHsG2PrepareLength                   0x1555U
#define PA_TxHsG3SyncLength                      0x1556U
#define PA_TxHsG3PrepareLength                   0x1557U
#define PA_TxMk2Extension                        0x155AU
#define PA_PeerScrambling                        0x155BU
#define PA_TxSkip                                0x155CU
#define PA_TxSkipPeriod                          0x155DU
#define PA_Local_TX_LCC_Enable                   0x155EU
#define PA_Peer_TX_LCC_Enable                    0x155FU
#define PA_ActiveTxDataLanes                     0x1560U
#define PA_ConnectedTxDataLanes                  0x1561U
#define PA_TxTrailingClocks                      0x1564U
#define PA_TxPWRStatus                           0x1567U
#define PA_TxGear                                0x1568U
#define PA_TxTermination                         0x1569U
#define PA_HSSeries                              0x156AU
#define PA_PWRMode                               0x1571U
#define PA_ActiveRxDataLanes                     0x1580U
#define PA_ConnectedRxDataLanes                  0x1581U
#define PA_RxPWRStatus                           0x1582U
#define PA_RxGear                                0x1583U
#define PA_RxTermination                         0x1584U
#define PA_Scrambling                            0x1585U
#define PA_MaxRxPWMGear                          0x1586U
#define PA_MaxRxHSGear                           0x1587U
#define PA_PACPReqTimeout                        0x1590U
#define PA_PACPReqEoBTimeout                     0x1591U
#define PA_RemoteVerInfo                         0x15A0U
#define PA_LogicalLaneMap                        0x15A1U
#define PA_SleepNoConfigTime                     0x15A2U
#define PA_StallNoConfigTime                     0x15A3U
#define PA_SaveConfigTime                        0x15A4U
#define PA_RxHSUnterminationCapability           0x15A5U
#define PA_RxLSTerminationCapability             0x15A6U
#define PA_Hibern8Time                           0x15A7U
#define PA_TActivate                             0x15A8U
#define PA_LocalVerInfo                          0x15A9U
#define PA_Granularity                           0x15AAU
#define PA_MK2ExtensionGuardBand                 0x15ABU
#define PA_PWRModeUserData0                      0x15B0U
#define PA_PWRModeUserData1                      0x15B1U
#define PA_PWRModeUserData2                      0x15B2U
#define PA_PWRModeUserData3                      0x15B3U
#define PA_PWRModeUserData4                      0x15B4U
#define PA_PWRModeUserData5                      0x15B5U
#define PA_PWRModeUserData6                      0x15B6U
#define PA_PWRModeUserData7                      0x15B7U
#define PA_PWRModeUserData8                      0x15B8U
#define PA_PWRModeUserData9                      0x15B9U
#define PA_PWRModeUserData10                     0x15BAU
#define PA_PWRModeUserData11                     0x15BBU
#define PA_PACPFrameCount                        0x15C0U
#define PA_PACPErrorCount                        0x15C1U
#define PA_PHYTestControl                        0x15C2U
#define DL_TxPreemptionCap                       0x2000U
#define DL_TC0TxMaxSDUSize                       0x2001U
#define DL_TC0RxInitCreditVal                    0x2002U
#define DL_TC1TxMaxSDUSize                       0x2003U
#define DL_TC1RxInitCreditVal                    0x2004U
#define DL_TC0TxBufferSize                       0x2005U
#define DL_TC1TxBufferSize                       0x2006U
#define DL_TC0TXFCThreshold                      0x2040U
#define DL_FC0ProtectionTimeOutVal               0x2041U
#define DL_TC0ReplayTimeOutVal                   0x2042U
#define DL_AFC0ReqTimeOutVal                     0x2043U
#define DL_AFC0CreditThreshold                   0x2044U
#define DL_TC0OutAckThreshold                    0x2045U
#define DL_PeerTC0Present                        0x2046U
#define DL_PeerTC0RxInitCreditVal                0x2047U
#define DL_TC1TXFCThreshold                      0x2060U
#define DL_FC1ProtectionTimeOutVal               0x2061U
#define DL_TC1ReplayTimeOutVal                   0x2062U
#define DL_AFC1ReqTimeOutVal                     0x2063U
#define DL_AFC1CreditThreshold                   0x2064U
#define DL_TC1OutAckThreshold                    0x2065U
#define DL_PeerTC1Present                        0x2066U
#define DL_PeerTC1RxInitCreditVal                0x2067U
#define N_DeviceID                               0x3000U
#define N_DeviceID_valid                         0x3001U
#define N_TC0TxMaxSDUSize                        0x3020U
#define N_TC1TxMaxSDUSize                        0x3021U
#define T_NumCPorts                              0x4000U
#define T_NumTestFeatures                        0x4001U
#define T_ConnectionState                        0x4020U
#define T_PeerDeviceID                           0x4021U
#define T_PeerCPortID                            0x4022U
#define T_TrafficClass                           0x4023U
#define T_ProtocolID                             0x4024U
#define T_CPortFlags                             0x4025U
#define T_TxTokenValue                           0x4026U
#define T_RxTokenValue                           0x4027U
#define T_LocalBufferSpace                       0x4028U
#define T_PeerBufferSpace                        0x4029U
#define T_CreditsToSend                          0x402AU
#define T_CPortMode                              0x402BU
#define T_TC0TxMaxSDUSize                        0x4060U
#define T_TC1TxMaxSDUSize                        0x4061U
#define T_TstCPortID                             0x4080U
#define T_TstSrcOn                               0x4081U
#define T_TstSrcPattern                          0x4082U
#define T_TstSrcIncrement                        0x4083U
#define T_TstSrcMessageSize                      0x4084U
#define T_TstSrcMessageCount                     0x4085U
#define T_TstSrcInterMessageGap                  0x4086U
#define DME_LocalFC0ProtectionTimeOutVal         0xD041U
#define DME_LocalTC0ReplayTimeOutVal             0xD042U
#define DME_LocalAFC0ReqTimeOutVal               0xD043U
#define DME_LocalFC1ProtectionTimeOutVal         0xD044U
#define DME_LocalTC1ReplayTimeOutVal             0xD045U
#define DME_LocalAFC1ReqTimeOutVal               0xD046U
#define DL_FC1ProtTimeOutVal                     0x2061U
#define DBG_PA_OPTION_SUITE                      0x9564U
#define DBG_CLK_PERIOD                           0x9514U

//---------------------------------------------
// Define Storage Bus Configuration
//---------------------------------------------
#define SBUS_CONFIG_SWRESETN			0x18U
#define SBUS_CONFIG_SWRESETN_UFS_HCI	0x400U
#define SBUS_CONFIG_SWRESETN_UFS_PHY	0x1000U
#define SBUS_TEMP						0x54U
//------------------------------------------------------
// Define Internal Attributes
//------------------------------------------------------
#define PA_TX_SKIP_VAL                      0x00000001U
#define PA_DBG_CLK_PERIOD                   0x00003850U
#define PA_DBG_AUTOMODE_THLD                0x000038d8U
#define PA_DBG_OPTION_SUITE                 0x00003990U
#define DL_FC0_PROTECTION_TIMEOUT_VAL_VALUE 0x00001FFFU
#define DL_FC1_PROTECTION_TIMEOUT_VAL_VALUE 0x00001FFFU
#define DME_LINKSTARTUP_REQ                 0x00000016U
#define N_DEVICE_ID_VAL                     0x00000000U
#define N_DEVICE_ID_VALID_VAL               0x00000001U
#define T_CONNECTION_STATE_OFF_VAL          0x00000000U
#define T_CONNECTION_STATE_ON_VAL           0x00000001U
#define T_PEER_DEVICE_ID_VAL                0x00000001U
#define DME_LINKSTARTUP_CMD                 0x00000016U
#define T_DBG_CPORT_OPTION_SUITE            0x00006890U
#define T_DBG_OPTION_SUITE_VAL              0x0000000DU
#define TX_LINERESET_PVALUE_MSB             0x000000abU
#define TX_LINERESET_PVALUE_LSB             0x000000acU
#define RX_LINERESET_VALUE_MSB              0x0000001cU
#define RX_LINERESET_VALUE_LSB              0x0000001dU
#define RX_OVERSAMPLING_ENABLE              0x00000076U
#define TX_LANE_0                           0x00000000U
#define TX_LANE_1                           0x00000001U
#define RX_LANE_0                           0x00000004U
#define RX_LANE_1                           0x00000005U
#define DBG_OV_TM                           0x00000200U

//--------------------------------------------
// HCI Specific Initialization Defines
//--------------------------------------------
//UTP Task Management Request List Base Address
#define CONV_UTMRLBA_LOW_VAL      0x7D303800U
//UTP Transfer Request List Base Address
#define CONV_UTMRLBA_HIGH_VAL     0x00000000U
#define UTMRLBA_LOW_VAL           0x1D303800U
#define UTMRLBA_HIGH_VAL          0x00000000U
#define CONV_UTRLBA_LOW_VAL       0x7D300000U
#define CONV_UTRLBA_HIGH_VAL      0x00000000U
#define UTRLBA_LOW_VAL            0x1D300000U
#define UTRLBA_HIGH_VAL           0x00000000U
//UTP  Command  Descriptor  Base  Address
#define CONV_UCDBA_LOW_VAL        0x7D300800U
#define CONV_UCDBAU_HIGH_VAL      0x00000000U
#define UCDBA_LOW_VAL             0x1D300800U
#define UCDBAU_HIGH_VAL           0x00000000U
//Data  Base  Address
#define CONV_DBA_LOW_VAL          0x7D301000U
#define CONV_DBAU_HIGH_VAL        0x00000000U
#define DBA_LOW_VAL               0x1D301000U
#define DBAU_HIGH_VAL             0x00000000U
#define DBA_OFFSET                0x00000000U
//UTP Command Descriptor
#define UCD_OFFSET                0x00000000U
//Physical Region Description Table
#define PRDT_OFFSET               0x00000200U
#define RESP_OFFSET               0x00000100U
#define RESP_LENGTH               0x00000040U
#define UTRL_OFFSET               0x00000020U
#define UTMRL_OFFSET              0x00000050U
#define AXI_ADDR_WIDTH            0x35U
#define UTRD_DOORBELL_SIZE        0x16U
#define UTMRD_DOORBELL_SIZE       0x4U
#define UTRIACR_VAL               0x0100060aU
#define ENABLE                    0x00000001U
#define DISABLE                   0x00000000U
#define TXPRDT_ENTRY_SIZE_VAL     0xFFFFFFFFU
#define RXPRDT_ENTRY_SIZE_VAL     0xFFFFFFFFU
#define TO_CNT_VAL_1US_VAL        0x00000064U
#define MISC_VAL                  0x000000f0U
#define DEFAULT_PAGE_CODE_VALUE   0x0U
#define SUPPORTED_VPD             0x0U
#define MODE_PAGE_VPD             0x87U
#define DEFAULT_ALLOCATION_LENGTH 0x24U
#define DEFAULT_CONTROL_VALUE     0x0U
#define VIP_LOGICAL_BLOCK_SIZE    0x00001000U
#define CONTROL                   0x0AU
#define READ_WRITE_ERROR_RECOVERY 0x01U
#define CACHING                   0x08U
#define ALL_PAGES                 0x3FU
#define DEFAULT_SUBPAGE_CODE      0x0U

//------------------------------------
// UFS Command Specific Defines
//------------------------------------
#define NOP_OUT                  0x00U
#define COMMAND                  0x01U
#define DATA_OUT                 0x02U
#define TASK_MANAGEMENT_REQUEST  0x04U
#define QUERY_REQUEST            0x16U
#define NOP_IN                   0x20U
#define RESPONSE                 0x21U
#define DATA_IN                  0x22U
#define TASK_MANAGEMENT_RESPONSE 0x24U
#define READY_TO_TRANSFER        0x31U
#define QUERY_RESPONSE           0x36U
#define REJECT                   0x3fU
#define LUN_MIN_VALUE            0x00U
#define LUN_MAX_VALUE            0x07U
#define SCSI                     0x00U
#define UFS_SPECIFIC             0x01U
#define VENDOR_SPECIFIC_MIN      0x08U
#define VENDOR_SPECIFIC_MAX      0x0fU
#define RPMB_LUN                 0xc4U
#define P_ABORT_TASK             0x01U
#define P_ABORT_TASK_SET         0x02U
#define P_CLEAR_TASK_SET         0x04U
#define P_LUN_RESET              0x08U
#define P_QUERY_TASK             0x80U
#define P_QUERY_TASK_SET         0x81U
#define INVALID_UPIU             0xEEU
#define I_MANUFACTURER_NAME      0x11U
#define I_PRODUCT_NAME           0x22U
#define I_OEM_ID                 0x44U
#define I_SERIAL_NUMBER          0x33U
#define I_PRODUCT_REVISION_LEVEL 0xAAU
#define DEBUG_REG_START          (UFS_HCI_BASE + 0x8000U)
#define DEBUG_REG_START_UTMRL    (UFS_HCI_BASE + 0x8600U)
#define STANDARD_READ_QUERY      0x01U
#define STANDARD_WRITE_QUERY     0x81U
#define QUERY_READ_FLAG          0x05U
#define QUERY_SET_FLAG           0x06U
#define QUERY_CLEAR_FLAG         0x07U
#define QUERY_TOGGLE_FLAG        0x08U
//IRAM Base Address
#define DBA_IRAM_LOW_VAL         0x1D020000U
#define DBAU_IRAM_HIGH_VAL       0x00000000U
//RAM Base Address +0x6000_0000 (remapped)
#define CONV_DBA_IRAM_LOW_VAL    0x7D020000U
#define CONV_DBAU_IRAM_HIGH_VAL  0x00000000U

struct ufs_tcc_host {
	struct ufs_hba *hba;
	void __iomem *ufs_sys_ctrl;
	void __iomem *ufs_reg_hci;
	void __iomem *ufs_reg_unipro;
	void __iomem *ufs_reg_mphy;
	void __iomem *ufs_reg_sbus_config;
	void __iomem *ufs_reg_fmp;
	void __iomem *ufs_reg_sec;
	struct reset_control *rst;
	struct reset_control *assert;
	uint64_t caps;

	int avail_ln_rx;
	int avail_ln_tx;

	u32 busthrtl_backup;

	bool in_suspend;

	struct ufs_pa_layer_attr dev_req_params;
};

#define ufs_hci_writel(host, val, reg) \
	writel((val), (host)->ufs_reg_hci + (reg))
#define ufs_hci_readl(host, reg) readl((host)->ufs_reg_hci + (reg))
#define ufs_hci_set_bits(host, mask, reg) \
	ufs_hci_writel(\
			(host), ((mask) | (ufs_hci_readl((host), (reg)))),\
			(reg))
#define ufs_hci_clr_bits(host, mask, reg) \
	ufs_hci_writel((host), \
			((~(mask)) & (ufs_hci_readl((host), (reg)))), (reg))

#define ufs_unipro_writel(host, val, reg) \
	writel((val), (host)->ufs_reg_unipro + (reg))
#define ufs_unipro_readl(host, reg) readl((host)->ufs_reg_unipro + (reg))
#define ufs_unipro_set_bits(host, mask, reg) \
	ufs_unipro_writel( \
		(host), ((mask) | (ufs_unipro_readl((host), (reg)))), (reg))
#define ufs_unipro_clr_bits(host, mask, reg) \
	ufs_unipro_writel((host), \
			((~(mask)) & (ufs_unipro_readl((host), (reg)))), \
			(reg))

#define ufs_fmp_writel(host, val, reg) \
	writel((val), (host)->ufs_reg_fmp + (reg))
#define ufs_fmp_readl(host, reg) readl((host)->ufs_reg_fmp + (reg))
#define ufs_fmp_set_bits(host, mask, reg) \
	ufs_fmp_writel(\
		(host), ((mask) | (ufs_fmp_readl((host), (reg)))), (reg))
#define ufs_fmp_clr_bits(host, mask, reg) \
	ufs_fmp_writel((host), \
			((~(mask)) & (ufs_fmp_readl((host), (reg)))), (reg))

#define ufs_mphy_writel(host, val, reg) \
	writel((val), (host)->ufs_reg_mphy + (reg))
#define ufs_mphy_readl(host, reg) readl((host)->ufs_reg_mphy + (reg))
#define ufs_mphy_set_bits(host, mask, reg) \
	ufs_mphy_writel( \
		(host), ((mask) | (ufs_mphy_readl((host), (reg)))), (reg))
#define ufs_mphy_clr_bits(host, mask, reg) \
	ufs_mphy_writel((host), \
		((~(mask)) & (ufs_mphy_readl((host), (reg)))), (reg))

#define ufs_sbus_config_writel(host, val, reg) \
	writel((val), (host)->ufs_reg_sbus_config + (reg))
#define ufs_sbus_config_readl(host, reg) readl((host)->ufs_reg_sbus_config \
		+ (reg))
#define ufs_sbus_config_set_bits(host, mask, reg) \
	ufs_sbus_config_writel(\
		(host), ((mask) | (ufs_sbus_config_readl((host), (reg)))),\
		(reg))
#define ufs_sbus_config_clr_bits(host, mask, reg) \
	ufs_sbus_config_writel((host), \
			((~(mask)) & (ufs_sbus_config_readl((host), (reg)))), \
			(reg))

#define ufs_sec_writel(host, val, reg) \
	writel((val), (host)->ufs_reg_sec + (reg))
#define ufs_sec_readl(host, reg) readl((host)->ufs_reg_sec + (reg))
#define ufs_sec_set_bits(host, mask, reg) \
	ufs_sec_writel(\
		(host), ((mask) | (ufs_sec_readl((host), (reg)))), (reg))
#define ufs_sec_clr_bits(host, mask, reg) \
	ufs_sec_writel((host),\
			((~(mask)) & (ufs_sec_readl((host), (reg)))), \
			(reg))

#endif /* UFS_TCC_H_ */
