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

#ifndef __UFS_TCC_H_
#define __UFS_TCC_H_

#define  MAX_NO_OF_RX_LANES      2
#define  MAX_NO_OF_TX_LANES      2
typedef enum {G1=1, G2, G3, G4} gear_t;
typedef enum {RATE_A=1, RATE_B} hs_series_t;
//typedef enum {FAST=1, SLOW, FAST_AUTO=4, SLOW_AUTO, UNCHANGED=7} pwrmode_t;

//----------------------------------------------------------------------------------------------------------------------------
//Macro Definition
//----------------------------------------------------------------------------------------------------------------------------
#define reg_setb(x,y) (x|=(1<<y))
#define reg_clrb(x,y) (x&=~(1<<y))
#define reg_rdb(x,y)  ((0==(x&(1<<y)))?0:1)

//----------------------------------------------------------------------------------------------------------------------------
// HCI Reigster Definition
//----------------------------------------------------------------------------------------------------------------------------
#define  UFS_UNIPRO_BASE    0x1D270000
#define  UFS_MPHY_BASE      0x1D274000
#define  UFS_HCI_BASE       0x1D280000
#define  UFS_FMP_BASE       0x1D290000

#define  HCI_CAP                                                         (0x0000)
#define  HCI_VER                                                         (0x0008)
#define  HCI_HCPID                                                       (0x0010)
#define  HCI_HCMID                                                       (0x0014)
#define  HCI_AHIT                                                        (0x0018)
#define  HCI_IS                                                          (0x0020)
#define  HCI_IE                                                          (0x0024)
#define  HCI_HCS                                                         (0x0030)
#define  HCI_HCE                                                         (0x0034)
#define  HCI_UECPA                                                       (0x0038)
#define  HCI_UECDL                                                       (0x003c)
#define  HCI_UECN                                                        (0x0040)
#define  HCI_UECT                                                        (0x0044)
#define  HCI_UECDME                                                      (0x0048)
#define  HCI_UTRIACR                                                     (0x004c)
#define  HCI_UTRLBA                                                      (0x0050)
#define  HCI_UTRLBAU                                                     (0x0054)
#define  HCI_UTRLDBR                                                     (0x0058)
#define  HCI_UTRLCLR                                                     (0x005c)
#define  HCI_UTRLRSR                                                     (0x0060)
#define  HCI_UTRLCNR                                                     (0x0064)
#define  HCI_UTMRLBA                                                     (0x0070)
#define  HCI_UTMRLBAU                                                    (0x0074)
#define  HCI_UTMRLDBR                                                    (0x0078)
#define  HCI_UTMRLCLR                                                    (0x007c)
#define  HCI_UTMRLRSR                                                    (0x0080)
#define  HCI_UICCMDR                                                     (0x0090)
#define  HCI_UICCMDARG1                                                  (0x0094)
#define  HCI_UICCMDARG2                                                  (0x0098)
#define  HCI_UICCMDARG3                                                  (0x009c)
#define  HCI_CCAP                                                        (0x0100)
#define  HCI_CRYPTOCAP_0                                                 (0x0104)
#define  HCI_CRYPTOCAP_1                                                 (0x0108)
#define  HCI_CRYPTOCFG_CRYPTOKEY_0                                       (0x0400)
#define  HCI_CRYPTOCFG_CRYPTOKEY_1                                       (0x0404)
#define  HCI_CRYPTOCFG_CRYPTOKEY_2                                       (0x0408)
#define  HCI_CRYPTOCFG_CRYPTOKEY_3                                       (0x040c)
#define  HCI_CRYPTOCFG_CRYPTOKEY_4                                       (0x0410)
#define  HCI_CRYPTOCFG_CRYPTOKEY_5                                       (0x0414)
#define  HCI_CRYPTOCFG_CRYPTOKEY_6                                       (0x0418)
#define  HCI_CRYPTOCFG_CRYPTOKEY_7                                       (0x041c)
#define  HCI_CRYPTOCFG_CRYPTOKEY_8                                       (0x0420)
#define  HCI_CRYPTOCFG_CRYPTOKEY_9                                       (0x0424)
#define  HCI_CRYPTOCFG_CRYPTOKEY_A                                       (0x0428)
#define  HCI_CRYPTOCFG_CRYPTOKEY_B                                       (0x042c)
#define  HCI_CRYPTOCFG_CRYPTOKEY_C                                       (0x0430)
#define  HCI_CRYPTOCFG_CRYPTOKEY_D                                       (0x0434)
#define  HCI_CRYPTOCFG_CRYPTOKEY_E                                       (0x0438)
#define  HCI_CRYPTOCFG_CRYPTOKEY_F                                       (0x043c)
#define  HCI_CRYPTOCFG__CFGE_CAPIDX_DUSIZE                               (0x0440)
#define  HCI_CRYPTOCFG__VSB                                              (0x0444)
#define  HCI_TXPRDT_ENTRY_SIZE                                           (0x1100)
#define  HCI_RXPRDT_ENTRY_SIZE                                           (0x1104)
#define  HCI_TO_CNT_VAL_1US                                              (0x110c)
#define  HCI_INVALID_UPIU_CTRL                                           (0x1110)
#define  HCI_INVALID_UPIU_BADDR                                          (0x1114)
#define  HCI_INVALID_UPIU_UBADDR                                         (0x1118)
#define  HCI_INVALID_UTMR_OFFSET_ADDR                                    (0x111c)
#define  HCI_INVALID_UTR_OFFSET_ADDR                                     (0x1120)
#define  HCI_INVALID_DIN_OFFSET_ADDR                                     (0x1124)
#define  HCI_VENDOR_SPECIFIC_IS                                          (0x1138)
#define  HCI_VENDOR_SPECIFIC_IE                                          (0x113c)
#define  HCI_UTRL_NEXUS_TYPE                                             (0x1140)
#define  HCI_UTMRL_NEXUS_TYPE                                            (0x1144)
#define  HCI_SW_RST                                                      (0x1150)
#define  HCI_UFS_LINK_VERSION                                            (0x1154)
#define  HCI_RS_FMPIU_MATCH_ERROR_CODE                                   (0x115c)
#define  HCI_DATA_REORDER                                                (0x1160)
#define  HCI_AXIDMA_RWDATA_BURST_LEN                                     (0x116c)
#define  HCI_GPIO_OUT                                                    (0x1170)
#define  HCI_WRITE_DMA_CTRL                                              (0x1174)
#define  HCI_DFES_ERROR_EN_PA_LAYER                                      (0x1178)
#define  HCI_DFES_ERROR_EN_DL_LAYER                                      (0x117c)
#define  HCI_DFES_ERROR_EN_N_LAYER                                       (0x1180)
#define  HCI_DFES_ERROR_EN_T_LAYER                                       (0x1184)
#define  HCI_DFES_ERROR_EN_DME_LAYER                                     (0x1188)
#define  HCI_UFSHCI_V2P1_CTRL                                            (0x118c)
#define  HCI_REQ_HOLD                                                    (0x11ac)
#define  HCI_CLKSTOP_CTRL                                                (0x11b0)
#define  HCI_MISC                                                        (0x11b4)
#define  HCI_PRDT_HIT_RATIO                                              (0x11c4)
#define  HCI_DMA0_MONITOR_STATE                                          (0x11c8)
#define  HCI_DMA0_MONITOR_CNT                                            (0x11cc)
#define  HCI_DMA1_MONITOR_STATE                                          (0x11d0)
#define  HCI_DMA1_MONITOR_CNT                                            (0x11d4)
#define  HCI_AXI_DMA_IF_CTRL                                             (0x11f8)
#define  HCI_UFS_ACG_DISABLE                                             (0x11fc)
#define  HCI_IOP_CG_DISABLE                                              (0x1200)
#define  HCI_UFS_UDPD                                                    (0x1204)
#define  HCI_MPHY_REFCLK_SEL                                             (0x1208)
#define  HCI_SMU_ABORT_MATCH_INFO                                        (0x120c)
#define  HCI_CPORT_LOG_CTRL                                              (0x1210)
#define  HCI_CPORT_LOG_CFG                                               (0x1214)
#define  HCI_DBR_DUPLICATION_INFO                                        (0x1220)
#define  HCI_DBR_TIMER_CONFIG                                            (0x1240)
#define  HCI_DBR_TIMER_ENABLE                                            (0x1244)
#define  HCI_DBR_TIMER_STATUS                                            (0x1248)
#define  HCI_UTRL_DBR_3_0_TIMER_EXPIRED_VALUE                            (0x1250)
#define  HCI_UTRL_DBR_7_4_TIMER_EXPIRED_VALUE                            (0x1254)
#define  HCI_UTRL_DBR_11_8_TIMER_EXPIRED_VALUE                           (0x1258)
#define  HCI_UTRL_DBR_15_12_TIMER_EXPIRED_VALUE                          (0x125c)
#define  HCI_UTMRL_DBR_3_0_TIMER_EXPIRED_VALUE                           (0x1260)
#define  HCI_CPORT_TX_LOG_FIFO0_0                                        (0x8000)
#define  HCI_CPORT_TX_LOG_FIFO7_7                                        (0x80fc)
#define  HCI_CPORT_RX_LOG_FIFO0_0                                        (0x8100)
#define  HCI_CPORT_RX_LOG_FIFO7_7                                        (0x81fc)
#define  HCI_UTRL0_0_LOG                                                 (0x8200)
#define  HCI_UTRL15_7_LOG                                                (0x83fc)
#define  HCI_UTR_UCD0_0_LOG                                              (0x8400)
#define  HCI_UTR_UCD15_7_LOG                                             (0x85fc)
#define  HCI_UTMRL0_0_LOG                                                (0x8600)
#define  HCI_UTMRL3_B_LOG                                                (0x86bc)
#define  HCI_CPORT_LOG_PTR                                               (0x8800)

//----------------------------------------------------------------------------------------------------------------------------
// UNIPRO Reigster Definition
//----------------------------------------------------------------------------------------------------------------------------
#define  UP_N_STD_REGION_N_DEVICEID                                     (0x5000)
#define  UP_N_STD_REGION_N_DEVICEID_VALID                               (0x5004)
#define  UP_N_STD_REGION_N_TC0TXMAXSDUSIZE                              (0x5080)
#define  UP_N_STD_REGION_N_TC1TXMAXSDUSIZE                              (0x5084)
#define  UP_DL_STD_REGION_DL_TXPREEMPTIONCAP                            (0x4000)
#define  UP_DL_STD_REGION_DL_TC0TXMAXSDUSIZE                            (0x4004)
#define  UP_DL_STD_REGION_DL_TC0RXINITCREDITVAL                         (0x4008)
#define  UP_DL_STD_REGION_DL_TC1TXMAXSDUSIZE                            (0x400c)
#define  UP_DL_STD_REGION_DL_TC1RXINITCREDITVAL                         (0x4010)
#define  UP_DL_STD_REGION_DL_TC0TXBUFFERSIZE                            (0x4014)
#define  UP_DL_STD_REGION_DL_TC1TXBUFFERSIZE                            (0x4018)
#define  UP_DL_STD_REGION_DL_TC0TXFCTHRESHOLD                           (0x4100)
#define  UP_DL_STD_REGION_DL_FC0PROTECTIONTIMEOUTVAL                    (0x4104)
#define  UP_DL_STD_REGION_DL_TC0REPLAYTIMEOUTVAL                        (0x4108)
#define  UP_DL_STD_REGION_DL_AFC0REQTIMEOUTVAL                          (0x410c)
#define  UP_DL_STD_REGION_DL_AFC0CREDITTHRESHOLD                        (0x4110)
#define  UP_DL_STD_REGION_DL_TC0OUTACKTHRESHOLD                         (0x4114)
#define  UP_DL_STD_REGION_DL_PEERTC0PRESENT                             (0x4118)
#define  UP_DL_STD_REGION_DL_PEERTC0RXINITCREDITVAL                     (0x411c)
#define  UP_DL_STD_REGION_DL_TC1TXFCTHRESHOLD                           (0x4180)
#define  UP_DL_STD_REGION_DL_FC1PROTECTIONTIMEOUTVAL                    (0x4184)
#define  UP_DL_STD_REGION_DL_TC1REPLAYTIMEOUTVAL                        (0x4188)
#define  UP_DL_STD_REGION_DL_AFC1REQTIMEOUTVAL                          (0x418c)
#define  UP_DL_STD_REGION_DL_AFC1CREDITTHRESHOLD                        (0x4190)
#define  UP_DL_STD_REGION_DL_TC1OUTACKTHRESHOLD                         (0x4194)
#define  UP_DL_STD_REGION_DL_PEERTC1PRESENT                             (0x4198)
#define  UP_DL_STD_REGION_DL_PEERTC1RXINITCREDITVAL                     (0x419c)
#define  UP_DME_USER_REGION_DME_POWERON_REQ                             (0x7890)
#define  UP_DME_USER_REGION_DME_POWERON_CNF_RESULT                      (0x7894)
#define  UP_DME_USER_REGION_DME_POWEROFF_REQ                            (0x7810)
#define  UP_DME_USER_REGION_DME_POWEROFF_CNF_RESULT                     (0x7814)
#define  UP_DME_USER_REGION_DME_RESET_REQ                               (0x7820)
#define  UP_DME_USER_REGION_DME_RESET_REQ_LEVEL                         (0x7824)
#define  UP_DME_USER_REGION_DME_ENABLE_REQ                              (0x7830)
#define  UP_DME_USER_REGION_DME_ENABLE_CNF_RESULT                       (0x7834)
#define  UP_DME_USER_REGION_DME_ENDPOINTRESET_REQ                       (0x7840)
#define  UP_DME_USER_REGION_DME_ENDPOINTRESET_CNF_RESULT                (0x7844)
#define  UP_DME_USER_REGION_DME_LINKSTARTUP_REQ                         (0x7850)
#define  UP_DME_USER_REGION_DME_LINKSTARTUP_CNF_RESULT                  (0x7854)
#define  UP_DME_USER_REGION_DME_HIBERNATE_ENTER_REQ                     (0x7860)
#define  UP_DME_USER_REGION_DME_HIBERNATE_ENTER_CNF_RESULT              (0x7864)
#define  UP_DME_USER_REGION_DME_HIBERNATE_ENTER_IND_RESULT              (0x7868)
#define  UP_DME_USER_REGION_DME_HIBERNATE_EXIT_REQ                      (0x7870)
#define  UP_DME_USER_REGION_DME_HIBERNATE_EXIT_CNF_RESULT               (0x7874)
#define  UP_DME_USER_REGION_DME_HIBERNATE_EXIT_IND_RESULT               (0x7878)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ                           (0x7880)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_POWERMODE                 (0x7884)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_LOCALL2TIMER0             (0x7888)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_LOCALL2TIMER1             (0x788c)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_LOCALL2TIMER2             (0x7890)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_LOCALL2TIMER3             (0x7894)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_LOCALL2TIMER4             (0x7898)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_LOCALL2TIMER5             (0x789c)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_REMOTEL2TIMER0            (0x78b8)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_REMOTEL2TIMER1            (0x78bc)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_REMOTEL2TIMER2            (0x78c0)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_REMOTEL2TIMER3            (0x78c4)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_REMOTEL2TIMER4            (0x78c8)
#define  UP_DME_USER_REGION_DME_POWERMODE_REQ_REMOTEL2TIMER5            (0x78cc)
#define  UP_DME_USER_REGION_DME_POWERMODE_CNF_RESULT                    (0x78e8)
#define  UP_DME_USER_REGION_DME_POWERMODE_IND_RESULT                    (0x78ec)
#define  UP_DME_USER_REGION_DME_TEST_MODE_REQ                           (0x7900)
#define  UP_DME_USER_REGION_DME_TEST_MODE_CNF_RESULT                    (0x7904)
#define  UP_DME_USER_REGION_DME_DEEPSTALL_ENTER_REQ                     (0x7910)
#define  UP_DME_USER_REGION_DME_GETSET_CONTROL                          (0x7a00)
#define  UP_DME_USER_REGION_DME_GETSET_ADDR                             (0x7a04)
#define  UP_DME_USER_REGION_DME_GETSET_WDATA                            (0x7a08)
#define  UP_DME_USER_REGION_DME_GETSET_RDATA                            (0x7a0c)
#define  UP_DME_USER_REGION_DME_GETSET_RESULT                           (0x7a10)
#define  UP_DME_USER_REGION_DME_PEER_GETSET_CONTROL                     (0x7a20)
#define  UP_DME_USER_REGION_DME_PEER_GETSET_ADDR                        (0x7a24)
#define  UP_DME_USER_REGION_DME_PEER_GETSET_WDATA                       (0x7a28)
#define  UP_DME_USER_REGION_DME_PEER_GETSET_RDATA                       (0x7a2c)
#define  UP_DME_USER_REGION_DME_PEER_GETSET_RESULT                      (0x7a30)
#define  UP_DME_USER_REGION_DME_INTR_STATUS_LSB                         (0x7b00)
#define  UP_DME_USER_REGION_DME_INTR_STATUS_MSB                         (0x7b04)
#define  UP_DME_USER_REGION_DME_INTR_ENABLE_LSB                         (0x7b10)
#define  UP_DME_USER_REGION_DME_INTR_ENABLE_MSB                         (0x7b14)
#define  UP_DME_USER_REGION_DME_INTR_ERROR_CODE                         (0x7b20)
#define  UP_DME_USER_REGION_DME_DISCARD_CPORT_ID                        (0x7b24)
#define  UP_DME_USER_REGION_DME_DL_FRAME_IND                            (0x7b30)
#define  UP_DME_USER_REGION_DME_DBG_OPTION_SUITE                        (0x7c00)
#define  UP_DME_USER_REGION_DME_DBG_CTRL_FSM                            (0x7d00)
#define  UP_DME_USER_REGION_DME_DBG_FORCE_CTRL_FSM                      (0x7d04)
#define  UP_DME_USER_REGION_DME_DBG_FREEZE_CTRL_FSM                     (0x7d08)
#define  UP_DME_USER_REGION_DME_DBG_STEP_CTRL_FSM                       (0x7d0c)
#define  UP_DME_USER_REGION_DME_DBG_HALT_CTRL_FSM                       (0x7d10)
#define  UP_DME_USER_REGION_DME_DBG_FLAG_STATUS                         (0x7d14)
#define  UP_DME_USER_REGION_DME_DBG_LINKCFG_FSM                         (0x7d18)
#define  UP_DME_USER_REGION_DME_DBG_LINKCFG_FSM_HALT_CTRL               (0x7d1c)
#define  UP_DME_USER_REGION_DME_DBG_LINKCFG_FSM_FORCE                   (0x7d20)
#define  UP_DME_STD_REGION_DME_DDBL1_REVISION                           (0x7000)
#define  UP_DME_STD_REGION_DME_DDBL1_LEVEL                              (0x7004)
#define  UP_DME_STD_REGION_DME_DDBL1_DEVICECLASS                        (0x7008)
#define  UP_DME_STD_REGION_DME_DDBL1_MANUFACTURERID                     (0x700c)
#define  UP_DME_STD_REGION_DME_DDBL1_PRODUCTID                          (0x7010)
#define  UP_DME_STD_REGION_DME_DDBL1_LENGTH                             (0x7014)
#define  UP_N_USER_REGION_N_DBG_L3S                                     (0x5800)
#define  UP_N_USER_REGION_N_DBG_ERROR_IRQ_MASK                          (0x5840)
#define  UP_N_USER_REGION_N_DBG_ERROR_IRQ_FLAG                          (0x5844)
#define  UP_T_USER_REGION_T_DBG_CTRL_FSM                                (0x6800)
#define  UP_T_USER_REGION_T_DBG_CMN_OPTION_SUITE                        (0x6804)
#define  UP_T_USER_REGION_T_DBG_CPORT_L4S                               (0x6880)
#define  UP_T_USER_REGION_T_DBG_CPORT_T_MTU                             (0x6884)
#define  UP_T_USER_REGION_T_DBG_CPORT_TX_ENDIAN                         (0x6888)
#define  UP_T_USER_REGION_T_DBG_CPORT_RX_ENDIAN                         (0x688c)
#define  UP_T_USER_REGION_T_DBG_CPORT_OPTION_SUITE                      (0x6890)
#define  UP_T_USER_REGION_T_DBG_CPORT_ERROR_IRQ_MASK                    (0x6894)
#define  UP_T_USER_REGION_T_DBG_CPORT_ERROR_IRQ_FLAG                    (0x6898)
#define  UP_T_USER_REGION_T_DBG_CPORT_TX_FSM                            (0x68a0)
#define  UP_T_USER_REGION_T_DBG_CPORT_RX_FSM                            (0x68a4)
#define  UP_PA_USER_REGION_PA_DBG_TX_INFO_RECEIVED                      (0x3804)
#define  UP_PA_USER_REGION_PA_DBG_RX_INFO_RECEIVED                      (0x380c)
#define  UP_PA_USER_REGION_PA_DBG_RECEIVED_TRG0_NUM                     (0x3840)
#define  UP_PA_USER_REGION_PA_DBG_RECEIVED_TRG1_NUM                     (0x3844)
#define  UP_PA_USER_REGION_PA_DBG_RECEIVED_TRG2_NUM                     (0x3848)
#define  UP_PA_USER_REGION_PA_DBG_LINK_STARTUP_TIMER_THLD               (0x384c)
#define  UP_PA_USER_REGION_PA_DBG_CLK_PERIOD                            (0x3850)
#define  UP_PA_USER_REGION_PA_DBG_TXPHY_CFGUPDT                         (0x3860)
#define  UP_PA_USER_REGION_PA_DBG_RXPHY_CFGUPDT                         (0x3864)
#define  UP_PA_USER_REGION_PA_DBG_SEND_TRG0_NUM                         (0x3880)
#define  UP_PA_USER_REGION_PA_DBG_SEND_TRG1_NUM                         (0x3884)
#define  UP_PA_USER_REGION_PA_DBG_SEND_TRG2_NUM                         (0x3888)
#define  UP_PA_USER_REGION_PA_DBG_INLNCFG                               (0x3890)
#define  UP_PA_USER_REGION_PA_DBG_FORCE_FSM_START                       (0x3898)
#define  UP_PA_USER_REGION_PA_DBG_MODE                                  (0x38a4)
#define  UP_PA_USER_REGION_PA_DBG_RAWCAP                                (0x38d0)
#define  UP_PA_USER_REGION_PA_DBG_IGNORE_CAP                            (0x38d4)
#define  UP_PA_USER_REGION_PA_DBG_AUTOMODE_THLD                         (0x38d8)
#define  UP_PA_USER_REGION_PA_DBG_SKIP_RESET_PHY                        (0x38e4)
#define  UP_PA_USER_REGION_PA_DBG_SKIP_LINE_RESET                       (0x3904)
#define  UP_PA_USER_REGION_PA_DBG_SEND_NO_DATA_DURING_DEEP_SYNC         (0x3908)
#define  UP_PA_USER_REGION_PA_DBG_LINE_RESET_REQ                        (0x390c)
#define  UP_PA_USER_REGION_PA_DBG_CMNPHY_CFGUPDT                        (0x3910)
#define  UP_PA_USER_REGION_PA_DBG_MIB_CAP_SEL                           (0x3918)
#define  UP_PA_USER_REGION_PA_DBG_RESUME_HIBERNATE                      (0x3940)
#define  UP_PA_USER_REGION_PA_DBG_PHY_RESET_DELAY                       (0x3950)
#define  UP_PA_USER_REGION_PA_DBG_RESET_DURATION                        (0x3954)
#define  UP_PA_USER_REGION_PA_DBG_PA_ERROR_FLAGS                        (0x3958)
#define  UP_PA_USER_REGION_PA_DBG_PA_ERROR_IRQ_MASK                     (0x395c)
#define  UP_PA_USER_REGION_PA_DBG_LINE_RESET_THLD                       (0x3984)
#define  UP_PA_USER_REGION_PA_DBG_LOCAL_PHY_ACTIVATE_TIME               (0x3988)
#define  UP_PA_USER_REGION_PA_DBG_PEERWAKE_TIMEOUT                      (0x398c)
#define  UP_PA_USER_REGION_PA_DBG_OPTION_SUITE_LSB                      (0x3990)
#define  UP_PA_USER_REGION_PA_DBG_OPTION_SUITE_DYNAMIC                  (0x3994)
#define  UP_PA_USER_REGION_PA_DBG_LOCAL_TX_CONNECTED_LANE_MASK          (0x3998)
#define  UP_PA_USER_REGION_PA_DBG_PEER_TX_CONNECTED_LANE_MASK           (0x399c)
#define  UP_PA_USER_REGION_PA_DBG_RECV_PEER_TX_LANE_NUMBER              (0x39a0)
#define  UP_PA_USER_REGION_PA_DBG_FSM_POWER_CTRL                        (0x39a4)
#define  UP_PA_USER_REGION_PA_DBG_OPTION_SUITE_MSB                      (0x39a8)
#define  UP_PA_USER_REGION_PA_DBG_FSM_POWER_EXPECTED_STATE              (0x39ac)
#define  UP_PA_USER_REGION_PA_DBG_FSM_POWER_EXPECTED_NEXT_STATE         (0x39b0)
#define  UP_PA_USER_REGION_PA_DBG_EXCEPTION_FLAGS                       (0x39c0)
#define  UP_PA_USER_REGION_PA_DBG_EXCEPTION_IRQ_MASK                    (0x39c4)
#define  UP_PA_USER_REGION_PA_DBG_DESKEW_NUM_FILLED                     (0x39c8)
#define  UP_PA_USER_REGION_PA_DBG_START_FSM_BREAK                       (0x39cc)
#define  UP_PA_USER_REGION_PA_DBG_PACP_SELECTOR                         (0x39d0)
#define  UP_PA_USER_REGION_PA_DBG_PACP_PARAMETER0                       (0x39d4)
#define  UP_PA_USER_REGION_PA_DBG_PACP_PARAMETER2                       (0x39d8)
#define  UP_PA_USER_REGION_PA_DBG_PACP_PARAMETER4                       (0x39dc)
#define  UP_PA_USER_REGION_PA_DBG_PACP_PARAMETER6                       (0x39e0)
#define  UP_PA_USER_REGION_PA_DBG_PACP_PARAMETER8                       (0x39e4)
#define  UP_PA_USER_REGION_PA_DBG_PACP_PARAMETER10                      (0x3a00)
#define  UP_PA_USER_REGION_PA_DBG_PACP_PARAMETER12                      (0x3a04)
#define  UP_PA_USER_REGION_PA_DBG_PA_RX_INFO_CONTROL                    (0x3a08)
#define  UP_PA_USER_REGION_PA_DBG_RX_PWM_BURST_CLOSURE_LENGTH           (0x3a40)
#define  UP_PA_USER_REGION_PA_DBG_RX_LS_PREPARE_LENGTH                  (0x3a44)
#define  UP_PA_USER_REGION_PA_DBG_RX_PWM_G6G7_SYNC_LENGTH               (0x3a48)
#define  UP_PA_USER_REGION_PA_DBG_USERDATA_VALID                        (0x3a4c)
#define  UP_PA_USER_REGION_PA_DBG_START_FSM_HALT                        (0x3a50)
#define  UP_PA_USER_REGION_PA_DBG_FSM_START                             (0x3a54)
#define  UP_PA_USER_REGION_PA_DBG_FSM_BURST_TXLANE                      (0x3a58)
#define  UP_PA_USER_REGION_PA_DBG_FSM_POWER                             (0x3a5c)
#define  UP_COMPONENT_REGION_COMP_VERSION                               (0x0000)
#define  UP_COMPONENT_REGION_COMP_INFO                                  (0x0004)
#define  UP_COMPONENT_REGION_COMP_RESET                                 (0x0010)
#define  UP_COMPONENT_REGION_COMP_OPTION_SUITE                          (0x0020)
#define  UP_COMPONENT_REGION_COMP_REMAP_CTRL                            (0x0030)
#define  UP_COMPONENT_REGION_COMP_REMAP_BASE                            (0x0034)
#define  UP_COMPONENT_REGION_COMP_AXI_AUX_FIELD                         (0x0040)
#define  UP_COMPONENT_REGION_COMP_DBG_DTB_MUX_SEL                       (0x0050)
#define  UP_PA_STD_REGION_PA_PHY_TYPE                                   (0x3000)
#define  UP_PA_STD_REGION_PA_AVAILTXDATALANES                           (0x3080)
#define  UP_PA_STD_REGION_PA_AVAILRXDATALANES                           (0x3100)
#define  UP_PA_STD_REGION_PA_MINRXTRAILINGCLOCKS                        (0x310c)
#define  UP_PA_STD_REGION_PA_TXHSG1SYNCLENGTH                           (0x3148)
#define  UP_PA_STD_REGION_PA_TXHSG1PREPARELENGTH                        (0x314c)
#define  UP_PA_STD_REGION_PA_TXHSG2SYNCLENGTH                           (0x3150)
#define  UP_PA_STD_REGION_PA_TXHSG2PREPARELENGTH                        (0x3154)
#define  UP_PA_STD_REGION_PA_TXHSG3SYNCLENGTH                           (0x3158)
#define  UP_PA_STD_REGION_PA_TXHSG3PREPARELENGTH                        (0x315c)
#define  UP_PA_STD_REGION_PA_TXMK2EXTENSION                             (0x3168)
#define  UP_PA_STD_REGION_PA_PEERSCRAMBLING                             (0x316c)
#define  UP_PA_STD_REGION_PA_TXSKIP                                     (0x3170)
#define  UP_PA_STD_REGION_PA_TXSKIPPERIOD                               (0x3174)
#define  UP_PA_STD_REGION_PA_LOCAL_TX_LCC_ENABLE                        (0x3178)
#define  UP_PA_STD_REGION_PA_PEER_TX_LCC_ENABLE                         (0x317c)
#define  UP_PA_STD_REGION_PA_ACTIVETXDATALANES                          (0x3180)
#define  UP_PA_STD_REGION_PA_CONNECTEDTXDATALANES                       (0x3184)
#define  UP_PA_STD_REGION_PA_TXTRAILINGCLOCKS                           (0x3190)
#define  UP_PA_STD_REGION_PA_TXPWRSTATUS                                (0x319c)
#define  UP_PA_STD_REGION_PA_TXGEAR                                     (0x31a0)
#define  UP_PA_STD_REGION_PA_TXTERMINATION                              (0x31a4)
#define  UP_PA_STD_REGION_PA_HSSERIES                                   (0x31a8)
#define  UP_PA_STD_REGION_PA_PWRMODE                                    (0x31c4)
#define  UP_PA_STD_REGION_PA_ACTIVERXDATALANES                          (0x3200)
#define  UP_PA_STD_REGION_PA_CONNECTEDRXDATALANES                       (0x3204)
#define  UP_PA_STD_REGION_PA_RXPWRSTATUS                                (0x3208)
#define  UP_PA_STD_REGION_PA_RXGEAR                                     (0x320c)
#define  UP_PA_STD_REGION_PA_RXTERMINATION                              (0x3210)
#define  UP_PA_STD_REGION_PA_SCRAMBLING                                 (0x3214)
#define  UP_PA_STD_REGION_PA_MAXRXPWMGEAR                               (0x3218)
#define  UP_PA_STD_REGION_PA_MAXRXHSGEAR                                (0x321c)
#define  UP_PA_STD_REGION_PA_PACPREQTIMEOUT                             (0x3240)
#define  UP_PA_STD_REGION_PA_PACPREQEOBTIMEOUT                          (0x3244)
#define  UP_PA_STD_REGION_PA_REMOTEVERINFO                              (0x3280)
#define  UP_PA_STD_REGION_PA_LOGICALLANEMAP                             (0x3284)
#define  UP_PA_STD_REGION_PA_SLEEPNOCONFIGTIME                          (0x3288)
#define  UP_PA_STD_REGION_PA_STALLNOCONFIGTIME                          (0x328c)
#define  UP_PA_STD_REGION_PA_SAVECONFIGTIME                             (0x3290)
#define  UP_PA_STD_REGION_PA_RXHSUNTERMINATIONCAPABILITY                (0x3294)
#define  UP_PA_STD_REGION_PA_RXLSTERMINATIONCAPABILITY                  (0x3298)
#define  UP_PA_STD_REGION_PA_HIBERN8TIME                                (0x329c)
#define  UP_PA_STD_REGION_PA_TACTIVATE                                  (0x32a0)
#define  UP_PA_STD_REGION_PA_LOCALVERINFO                               (0x32a4)
#define  UP_PA_STD_REGION_PA_GRANULARITY                                (0x32a8)
#define  UP_PA_STD_REGION_PA_MK2EXTENSIONGUARDBAND                      (0x32ac)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA0                           (0x32c0)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA1                           (0x32c4)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA2                           (0x32c8)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA3                           (0x32cc)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA4                           (0x32d0)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA5                           (0x32d4)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA6                           (0x32d8)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA7                           (0x32dc)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA8                           (0x32e0)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA9                           (0x32e4)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA10                          (0x32e8)
#define  UP_PA_STD_REGION_PA_PWRMODEUSERDATA11                          (0x32ec)
#define  UP_PA_STD_REGION_PA_PACPFRAMECOUNT                             (0x3300)
#define  UP_PA_STD_REGION_PA_PACPERRORCOUNT                             (0x3304)
#define  UP_PA_STD_REGION_PA_PHYTESTCONTROL                             (0x3308)
#define  UP_BUS_MONITOR_REGION_BUS_EXT_ACCESS_RESULT                    (0x0200)
#define  UP_BUS_MONITOR_REGION_BUS_REM_ACCESS_CHANNEL                   (0x0210)
#define  UP_BUS_MONITOR_REGION_BUS_REM_ACCESS_ADDR                      (0x0214)
#define  UP_BUS_MONITOR_REGION_BUS_REM_ACCESS_USER                      (0x0218)
#define  UP_BUS_MONITOR_REGION_BUS_REM_ACCESS_WDATA                     (0x021c)
#define  UP_BUS_MONITOR_REGION_BUS_TRANS_CAPTURE_CONFIG                 (0x0220)
#define  UP_BUS_MONITOR_REGION_BUS_TRANS_CAPTURE_WDATA                  (0x0224)
#define  UP_T_STD_REGION_T_NUM_CPORTS                                   (0x6000)
#define  UP_T_STD_REGION_T_NUMTESTFEATURES                              (0x6004)
#define  UP_T_STD_REGION_T_CONNECTIONSTATE                              (0x6080)
#define  UP_T_STD_REGION_T_PEERDEVICEID                                 (0x6084)
#define  UP_T_STD_REGION_T_PEERCPORTID                                  (0x6088)
#define  UP_T_STD_REGION_T_TRAFFICCLASS                                 (0x608c)
#define  UP_T_STD_REGION_T_PROTOCOLID                                   (0x6090)
#define  UP_T_STD_REGION_T_CPORTFLAGS                                   (0x6094)
#define  UP_T_STD_REGION_T_TXTOKENVALUE                                 (0x6098)
#define  UP_T_STD_REGION_T_RXTOKENVALUE                                 (0x609c)
#define  UP_T_STD_REGION_T_LOCALBUFFERSPACE                             (0x60a0)
#define  UP_T_STD_REGION_T_PEERBUFFERSPACE                              (0x60a4)
#define  UP_T_STD_REGION_T_CREDITSTOSEND                                (0x60a8)
#define  UP_T_STD_REGION_T_CPORTMODE                                    (0x60ac)
#define  UP_T_STD_REGION_T_TC0TXMAXSDUSIZE                              (0x6180)
#define  UP_T_STD_REGION_T_TC1TXMAXSDUSIZE                              (0x6184)
#define  UP_T_STD_REGION_T_TSTCPORTID                                   (0x6200)
#define  UP_T_STD_REGION_T_TSTSRCON                                     (0x6204)
#define  UP_T_STD_REGION_T_TSTSRCPATTERN                                (0x6208)
#define  UP_T_STD_REGION_T_TSTSRCINCREMENT                              (0x620c)
#define  UP_T_STD_REGION_T_TSTSRCMESSAGESIZE                            (0x6210)
#define  UP_T_STD_REGION_T_TSTSRCMESSAGECOUNT                           (0x6214)
#define  UP_T_STD_REGION_T_TSTSRCINTERMESSAGEGAP                        (0x6218)
#define  UP_T_STD_REGION_T_TSTDSTON                                     (0x6284)
#define  UP_T_STD_REGION_T_TSTDSTERRORDETECTIONENABLE                   (0x6288)
#define  UP_T_STD_REGION_T_TSTDSTPATTERN                                (0x628c)
#define  UP_T_STD_REGION_T_TSTDSTINCREMENT                              (0x6290)
#define  UP_T_STD_REGION_T_TSTDSTMESSAGECOUNT                           (0x6294)
#define  UP_T_STD_REGION_T_TSTDSTMESSAGEOFFSET                          (0x6298)
#define  UP_T_STD_REGION_T_TSTDSTMESSAGESIZE                            (0x629c)
#define  UP_T_STD_REGION_T_TSTDSTFCCREDITS                              (0x62a0)
#define  UP_T_STD_REGION_T_TSTDSTINTERFCTOKENGAP                        (0x62a4)
#define  UP_T_STD_REGION_T_TSTDSTINITIALFCCREDITS                       (0x62a8)
#define  UP_T_STD_REGION_T_TSTDSTERRORCODE                              (0x62ac)
#define  UP_DL_USER_REGION_DL_DBG_FSM_STATE                             (0x4800)
#define  UP_DL_USER_REGION_DL_DBG_FSM_FORCE                             (0x4804)
#define  UP_DL_USER_REGION_DL_DBG_FSM_STEP_CTRL                         (0x4808)
#define  UP_DL_USER_REGION_DL_DBG_FSM_HALT                              (0x480c)
#define  UP_DL_USER_REGION_DL_DBG_MODE                                  (0x4810)
#define  UP_DL_USER_REGION_DL_DBG_STATUS                                (0x4814)
#define  UP_DL_USER_REGION_DL_DBG_DL_RX_INFO_CTRL                       (0x4818)
#define  UP_DL_USER_REGION_DL_DBG_OPTION_SUITE                          (0x481c)
#define  UP_DL_USER_REGION_DL_DBG_ERROR_FLAGS                           (0x4840)
#define  UP_DL_USER_REGION_DL_DBG_ERROR_IRQ_MASK                        (0x4844)
#define  UP_DL_USER_REGION_DL_DBG_PA_INIT_RETRY_COUNT                   (0x4848)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_AFC_FRAME_COUNT                (0x4880)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_AFC_FRAME_COUNT                (0x4884)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_AFC_SEQ_NUM                    (0x4888)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_AFC_SEQ_NUM                    (0x488c)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_DATA_FRAME_COUNT               (0x4890)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_DATA_FRAME_COUNT               (0x4894)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_DATA_SEQ_NUM                   (0x4898)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_DATA_SEQ_NUM                   (0x489c)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_DATA_SEQ_NUM_READY             (0x48a0)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_DATA_SEQ_NUM_ACKED             (0x48a4)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_REPLAYED_DATA_FRAME_COUNT      (0x48a8)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_REPLAYED_DATA_SEQ_NUM          (0x48ac)
#define  UP_DL_USER_REGION_DL_DBG_TC1_TX_AFC_FRAME_COUNT                (0x4900)
#define  UP_DL_USER_REGION_DL_DBG_TC1_RX_AFC_FRAME_COUNT                (0x4904)
#define  UP_DL_USER_REGION_DL_DBG_TC1_TX_AFC_SEQ_NUM                    (0x4908)
#define  UP_DL_USER_REGION_DL_DBG_TC1_RX_AFC_SEQ_NUM                    (0x490c)
#define  UP_DL_USER_REGION_DL_DBG_TC0_CREDIT_R_BYTE                     (0x4c00)
#define  UP_DL_USER_REGION_DL_DBG_TC0_CREDIT_U_BYTE                     (0x4c04)
#define  UP_DL_USER_REGION_DL_DBG_TC0_CREDIT_A_BYTE                     (0x4c08)
#define  UP_DL_USER_REGION_DL_DBG_TC0_CREDIT_S_BYTE                     (0x4c0c)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFF_INFO_ADDR                 (0x4c10)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFF_INFO_DATA                 (0x4c14)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_CURR_PLD_WPTR           (0x4c40)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_CURR_PLD_RPTR_BASE      (0x4c44)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_PLD_WPTR                (0x4c48)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_PLD_RPTR_BASE           (0x4c4c)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_CURR_WSEQ_NUM           (0x4c50)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_CURR_RSEQ_NUM           (0x4c54)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_CURR_RSEQ_NUM_TO_REPLAY (0x4c58)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_WSEQ_NUM                (0x4c5c)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_RSEQ_NUM                (0x4c60)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_RSEQ_NUM_TO_REPLAY      (0x4c64)
#define  UP_DL_USER_REGION_DL_DBG_TC0_TX_BUFFER_SEQ_NUM_FORCE           (0x4c68)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_BUFFER_CURR_RPTR               (0x4c80)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_BUFFER_CURR_WPTR               (0x4c84)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_BUFFER_CURR_WPTR_CONFIRMED     (0x4c88)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_BUFFER_RPTR                    (0x4c8c)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_BUFFER_RPTR_FORCE              (0x4c90)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_BUFFER_WPTR                    (0x4c94)
#define  UP_DL_USER_REGION_DL_DBG_TC0_RX_BUFFER_WPTR_HOLD               (0x4c98)

//----------------------------------------------------------------------------------------------------------------------------
// FMP Reigster Definition
//----------------------------------------------------------------------------------------------------------------------------
#define  FMP_FMPRCTRL                                                   (0x0000)
#define  FMP_FMPRSTAT                                                   (0x0008)
#define  FMP_FMPRSECURITY                                               (0x0010)
#define  FMP_FMPVERSION                                                 (0x001c)
#define  FMP_FMPDEK0                                                    (0x0020)
#define  FMP_FMPDEK1                                                    (0x0024)
#define  FMP_FMPDEK2                                                    (0x0028)
#define  FMP_FMPDEK3                                                    (0x002c)
#define  FMP_FMPDEK4                                                    (0x0030)
#define  FMP_FMPDEK5                                                    (0x0034)
#define  FMP_FMPDEK6                                                    (0x0038)
#define  FMP_FMPDEK7                                                    (0x003c)
#define  FMP_FMPDTK0                                                    (0x0040)
#define  FMP_FMPDTK1                                                    (0x0044)
#define  FMP_FMPDTK2                                                    (0x0048)
#define  FMP_FMPDTK3                                                    (0x004c)
#define  FMP_FMPDTK4                                                    (0x0050)
#define  FMP_FMPDTK5                                                    (0x0054)
#define  FMP_FMPDTK6                                                    (0x0058)
#define  FMP_FMPDTK7                                                    (0x005c)
#define  FMP_FMPWCTRL                                                   (0x0100)
#define  FMP_FMPWSTAT                                                   (0x0108)
#define  FMP_FMPFEKM0                                                   (0x0120)
#define  FMP_FMPFEKM1                                                   (0x0124)
#define  FMP_FMPFEKM2                                                   (0x0128)
#define  FMP_FMPFEKM3                                                   (0x012c)
#define  FMP_FMPFEKM4                                                   (0x0130)
#define  FMP_FMPFEKM5                                                   (0x0134)
#define  FMP_FMPFEKM6                                                   (0x0138)
#define  FMP_FMPFEKM7                                                   (0x013c)
#define  FMP_FMPFTKM0                                                   (0x0140)
#define  FMP_FMPFTKM1                                                   (0x0144)
#define  FMP_FMPFTKM2                                                   (0x0148)
#define  FMP_FMPFTKM3                                                   (0x014c)
#define  FMP_FMPFTKM4                                                   (0x0150)
#define  FMP_FMPFTKM5                                                   (0x0154)
#define  FMP_FMPFTKM6                                                   (0x0158)
#define  FMP_FMPFTKM7                                                   (0x015c)
#define  FMP_FMPSBEGIN0                                                 (0x0200)
#define  FMP_FMPSEND0                                                   (0x0204)
#define  FMP_FMPSLUN0                                                   (0x0208)
#define  FMP_FMPSCTRL0                                                  (0x020c)
#define  FMP_FMPSBEGIN1                                                 (0x0210)
#define  FMP_FMPSEND1                                                   (0x0214)
#define  FMP_FMPSLUN1                                                   (0x0218)
#define  FMP_FMPSCTRL1                                                  (0x021c)
#define  FMP_FMPSBEGIN2                                                 (0x0220)
#define  FMP_FMPSEND2                                                   (0x0224)
#define  FMP_FMPSLUN2                                                   (0x0228)
#define  FMP_FMPSCTRL2                                                  (0x022c)
#define  FMP_FMPSBEGIN3                                                 (0x0230)
#define  FMP_FMPSEND3                                                   (0x0234)
#define  FMP_FMPSLUN3                                                   (0x0238)
#define  FMP_FMPSCTRL3                                                  (0x023c)
#define  FMP_FMPSBEGIN4                                                 (0x0240)
#define  FMP_FMPSEND4                                                   (0x0244)
#define  FMP_FMPSLUN4                                                   (0x0248)
#define  FMP_FMPSCTRL4                                                  (0x024c)
#define  FMP_FMPSBEGIN5                                                 (0x0250)
#define  FMP_FMPSEND5                                                   (0x0254)
#define  FMP_FMPSLUN5                                                   (0x0258)
#define  FMP_FMPSCTRL5                                                  (0x025c)
#define  FMP_FMPSBEGIN6                                                 (0x0260)
#define  FMP_FMPSEND6                                                   (0x0264)
#define  FMP_FMPSLUN6                                                   (0x0268)
#define  FMP_FMPSCTRL6                                                  (0x026c)
#define  FMP_FMPSBEGIN7                                                 (0x0270)
#define  FMP_FMPSEND7                                                   (0x0274)
#define  FMP_FMPSLUN7                                                   (0x0278)
#define  FMP_FMPSCTRL7                                                  (0x027c)

//----------------------------------------------------------------------------------------------------------------------------
// PMA Reigster Definition
//----------------------------------------------------------------------------------------------------------------------------
#define  PMA_CMN_REG00                                                   (0x0000)
#define  PMA_CMN_REG01                                                   (0x0004)
#define  PMA_CMN_REG02                                                   (0x0008)
#define  PMA_CMN_REG03                                                   (0x000c)
#define  PMA_CMN_REG04                                                   (0x0010)
#define  PMA_CMN_REG05                                                   (0x0014)
#define  PMA_CMN_REG06                                                   (0x0018)
#define  PMA_CMN_REG07                                                   (0x001c)
#define  PMA_CMN_REG08                                                   (0x0020)
#define  PMA_CMN_REG09                                                   (0x0024)
#define  PMA_CMN_REG0A                                                   (0x0028)
#define  PMA_CMN_REG0B                                                   (0x002c)
#define  PMA_CMN_REG0C                                                   (0x0030)
#define  PMA_CMN_REG0D                                                   (0x0034)
#define  PMA_CMN_REG0E                                                   (0x0038)
#define  PMA_CMN_REG0F                                                   (0x003c)
#define  PMA_CMN_REG10                                                   (0x0040)
#define  PMA_CMN_REG11                                                   (0x0044)
#define  PMA_CMN_REG12                                                   (0x0048)
#define  PMA_CMN_REG13                                                   (0x004c)
#define  PMA_CMN_REG14                                                   (0x0050)
#define  PMA_CMN_REG15                                                   (0x0054)
#define  PMA_CMN_REG16                                                   (0x0058)
#define  PMA_CMN_REG17                                                   (0x005c)
#define  PMA_CMN_REG18                                                   (0x0060)
#define  PMA_CMN_REG19                                                   (0x0064)
#define  PMA_CMN_REG1E                                                   (0x0078)
#define  PMA_CMN_REG1F                                                   (0x007c)
#define  PMA_TRSV0_REG00                                                 (0x00c0)
#define  PMA_TRSV0_REG01                                                 (0x00c4)
#define  PMA_TRSV0_REG02                                                 (0x00c8)
#define  PMA_TRSV0_REG03                                                 (0x00cc)
#define  PMA_TRSV0_REG04                                                 (0x00d0)
#define  PMA_TRSV0_REG05                                                 (0x00d4)
#define  PMA_TRSV0_REG06                                                 (0x00d8)
#define  PMA_TRSV0_REG07                                                 (0x00dc)
#define  PMA_TRSV0_REG08                                                 (0x00e0)
#define  PMA_TRSV0_REG09                                                 (0x00e4)
#define  PMA_TRSV0_REG0A                                                 (0x00e8)
#define  PMA_TRSV0_REG0B                                                 (0x00ec)
#define  PMA_TRSV0_REG0C                                                 (0x00f0)
#define  PMA_TRSV0_REG0D                                                 (0x00f4)
#define  PMA_TRSV0_REG0E                                                 (0x00f8)
#define  PMA_TRSV0_REG0F                                                 (0x00fc)
#define  PMA_TRSV0_REG10                                                 (0x0100)
#define  PMA_TRSV0_REG11                                                 (0x0104)
#define  PMA_TRSV0_REG12                                                 (0x0108)
#define  PMA_TRSV0_REG13                                                 (0x010c)
#define  PMA_TRSV0_REG14                                                 (0x0110)
#define  PMA_TRSV0_REG15                                                 (0x0114)
#define  PMA_TRSV0_REG16                                                 (0x0118)
#define  PMA_TRSV0_REG17                                                 (0x011c)
#define  PMA_TRSV0_REG18                                                 (0x0120)
#define  PMA_TRSV0_REG19                                                 (0x0124)
#define  PMA_TRSV0_REG1A                                                 (0x0128)
#define  PMA_TRSV0_REG1B                                                 (0x012c)
#define  PMA_TRSV0_REG1C                                                 (0x0130)
#define  PMA_TRSV0_REG1D                                                 (0x0134)
#define  PMA_TRSV0_REG1E                                                 (0x0138)
#define  PMA_TRSV0_REG1F                                                 (0x013c)
#define  PMA_TRSV0_REG20                                                 (0x0140)
#define  PMA_TRSV0_REG21                                                 (0x0144)
#define  PMA_TRSV0_REG22                                                 (0x0148)
#define  PMA_TRSV0_REG23                                                 (0x014c)
#define  PMA_TRSV0_REG24                                                 (0x0150)
#define  PMA_TRSV0_REG25                                                 (0x0154)
#define  PMA_TRSV0_REG26                                                 (0x0158)
#define  PMA_TRSV0_REG27                                                 (0x015c)
#define  PMA_TRSV0_REG28                                                 (0x0160)
#define  PMA_TRSV0_REG29                                                 (0x0164)
#define  PMA_TRSV0_REG2A                                                 (0x0168)
#define  PMA_TRSV0_REG2B                                                 (0x016c)
#define  PMA_TRSV0_REG2C                                                 (0x0170)
#define  PMA_TRSV0_REG2D                                                 (0x0174)
#define  PMA_TRSV0_REG2E                                                 (0x0178)
#define  PMA_TRSV0_REG2F                                                 (0x017c)
#define  PMA_TRSV0_REG3D                                                 (0x01b4)
#define  PMA_TRSV0_REG3E                                                 (0x01b8)
#define  PMA_TRSV0_REG3F                                                 (0x01bc)
#define  PMA_TRSV1_REG00                                                 (0x01c0)
#define  PMA_TRSV1_REG01                                                 (0x01c4)
#define  PMA_TRSV1_REG02                                                 (0x01c8)
#define  PMA_TRSV1_REG03                                                 (0x01cc)
#define  PMA_TRSV1_REG04                                                 (0x01d0)
#define  PMA_TRSV1_REG05                                                 (0x01d4)
#define  PMA_TRSV1_REG06                                                 (0x01d8)
#define  PMA_TRSV1_REG07                                                 (0x01dc)
#define  PMA_TRSV1_REG08                                                 (0x01e0)
#define  PMA_TRSV1_REG09                                                 (0x01e4)
#define  PMA_TRSV1_REG0A                                                 (0x01e8)
#define  PMA_TRSV1_REG0B                                                 (0x01ec)
#define  PMA_TRSV1_REG0C                                                 (0x01f0)
#define  PMA_TRSV1_REG0D                                                 (0x01f4)
#define  PMA_TRSV1_REG0E                                                 (0x01f8)
#define  PMA_TRSV1_REG0F                                                 (0x01fc)
#define  PMA_TRSV1_REG10                                                 (0x0200)
#define  PMA_TRSV1_REG11                                                 (0x0204)
#define  PMA_TRSV1_REG12                                                 (0x0208)
#define  PMA_TRSV1_REG13                                                 (0x020c)
#define  PMA_TRSV1_REG14                                                 (0x0210)
#define  PMA_TRSV1_REG15                                                 (0x0214)
#define  PMA_TRSV1_REG16                                                 (0x0218)
#define  PMA_TRSV1_REG17                                                 (0x021c)
#define  PMA_TRSV1_REG18                                                 (0x0220)
#define  PMA_TRSV1_REG19                                                 (0x0224)
#define  PMA_TRSV1_REG1A                                                 (0x0228)
#define  PMA_TRSV1_REG1B                                                 (0x022c)
#define  PMA_TRSV1_REG1C                                                 (0x0230)
#define  PMA_TRSV1_REG1D                                                 (0x0234)
#define  PMA_TRSV1_REG1E                                                 (0x0238)
#define  PMA_TRSV1_REG1F                                                 (0x023c)
#define  PMA_TRSV1_REG20                                                 (0x0240)
#define  PMA_TRSV1_REG21                                                 (0x0244)
#define  PMA_TRSV1_REG22                                                 (0x0248)
#define  PMA_TRSV1_REG23                                                 (0x024c)
#define  PMA_TRSV1_REG24                                                 (0x0250)
#define  PMA_TRSV1_REG25                                                 (0x0254)
#define  PMA_TRSV1_REG26                                                 (0x0258)
#define  PMA_TRSV1_REG27                                                 (0x025c)
#define  PMA_TRSV1_REG28                                                 (0x0260)
#define  PMA_TRSV1_REG29                                                 (0x0264)
#define  PMA_TRSV1_REG2A                                                 (0x0268)
#define  PMA_TRSV1_REG2B                                                 (0x026c)
#define  PMA_TRSV1_REG2C                                                 (0x0270)
#define  PMA_TRSV1_REG2D                                                 (0x0274)
#define  PMA_TRSV1_REG2E                                                 (0x0278)
#define  PMA_TRSV1_REG2F                                                 (0x027c)
#define  PMA_TRSV1_REG3D                                                 (0x02b4)
#define  PMA_TRSV1_REG3E                                                 (0x02b8)
#define  PMA_TRSV1_REG3F                                                 (0x02bc)

//----------------------------------------------------------------------------------------------------------------------------
//Ufs Specific Defines
//----------------------------------------------------------------------------------------------------------------------------
#define  TX_HSMODE_Capability                                            0x0001
#define  TX_HSGEAR_Capability                                            0x0002
#define  TX_PWMG0_Capability                                             0x0003
#define  TX_PWMGEAR_Capability                                           0x0004
#define  TX_Amplitude_Capability                                         0x0005
#define  TX_ExternalSYNC_Capability                                      0x0006
#define  TX_HS_Unterminated_LINE_Drive_Capability                        0x0007
#define  TX_LS_Terminated_LINE_Drive_Capability                          0x0008
#define  TX_Min_SLEEP_NoConfig_Time_Capability                           0x0009
#define  TX_Min_STALL_NoConfig_Time_Capability                           0x000A
#define  TX_Min_SAVE_Config_Time_Capability                              0x000B
#define  TX_REF_CLOCK_SHARED_Capability                                  0x000C
#define  TX_Hibern8Time_Capability                                       0x000F
#define  TX_Advanced_Granularity_Capability                              0x0010
#define  TX_Advanced_Hibern8Time_Capability                              0x0011
#define  TX_MODE                                                         0x0021 
#define  TX_HSRATE_Series                                                0x0022
#define  TX_HSGEAR                                                       0x0023
#define  TX_PWMGEAR                                                      0x0024
#define  TX_Amplitude                                                    0x0025
#define  TX_HS_SlewRate                                                  0x0026
#define  TX_SYNC_Source                                                  0x0027
#define  TX_HS_SYNC_LENGTH                                               0x0028
#define  TX_HS_PREPARE_LENGTH                                            0x0029
#define  TX_LS_PREPARE_LENGTH                                            0x002A
#define  TX_HIBERN8_Control                                              0x002B
#define  TX_LCC_Enable                                                   0x002C
#define  TX_PWM_BURST_Closure_Extension                                  0x002D
#define  TX_BYPASS_8B10B_Enable                                          0x002E
#define  TX_DRIVER_POLARITY                                              0x002F
#define  TX_HS_Unterminated_LINE_Drive_Enable                            0x0030
#define  TX_LS_Terminated_LINE_Drive_Enable                              0x0031
#define  TX_LCC_Sequencer                                                0x0032
#define  RX_MODE                                                         0x00A1 
#define  RX_HSRATE_Series                                                0x00A2 
#define  RX_HSGEAR                                                       0x00A3
#define  RX_PWMGEAR                                                      0x00A4
#define  RX_LS_Terminated_Enable                                         0x00A5
#define  RX_HS_Unterminated_Enable                                       0x00A6
#define  RX_Enter_HIBERN8                                                0x00A7
#define  RX_BYPASS_8B10B_Enable                                          0x00A8
#define  TX_FSM_State                                                    0x0041
#define  RX_FSM_State                                                    0x00C1
#define  OMC_TYPE_Capability                                             0x00D1
#define  MC_HSMODE_Capability                                            0x00D2
#define  MC_HSGEAR_Capability                                            0x00D3
#define  MC_HS_START_TIME_Var_Capability                                 0x00D4
#define  MC_HS_START_TIME_Range_Capability                               0x00D5
#define  MC_RX_SA_Capability                                             0x00D6
#define  MC_RX_LA_Capability                                             0x00D7
#define  MC_LS_PREPARE_LENGTH                                            0x00D8
#define  MC_PWMG0_Capability                                             0x00D9
#define  MC_PWMGEAR_Capability                                           0x00DA
#define  MC_LS_Terminated_Capability                                     0x00DB
#define  MC_HS_Unterminated_Capability                                   0x00DC
#define  MC_LS_Terminated_LINE_Drive_Capability                          0x00DD
#define  MC_HS_Unterminated_LINE_Drive_Capability                        0x00DE
#define  MC_MFG_ID_Part1                                                 0x00DF
#define  MC_MFG_ID_Part2                                                 0x00E0
#define  MC_PHY_MajorMinor_Release_Capability                            0x00E1
#define  MC_PHY_Editorial_Release_Capability                             0x00E2
#define  MC_Ext_Vendor_Info_Part1                                        0x00E3
#define  MC_Ext_Vendor_Info_Part2                                        0x00E4
#define  MC_Ext_Vendor_Info_Part3                                        0x00E5
#define  MC_Ext_Vendor_Info_Part4                                        0x00E6
#define  MC_Output_Amplitude                                             0x0061
#define  MC_HS_Unterminated_Enable                                       0x0062
#define  MC_LS_Terminated_Enable                                         0x0063
#define  MC_HS_Unterminated_LINE_Drive_Enable                            0x0064
#define  MC_LS_Terminated_LINE_Drive_Enable                              0x0065
#define  RX_HSMODE_Capability                                            0x0081 
#define  RX_HSGEAR_Capability                                            0x0082
#define  RX_PWMG0_Capability                                             0x0083
#define  RX_PWMGEAR_Capability                                           0x0084
#define  RX_HS_Unterminated_Capability                                   0x0085
#define  RX_LS_Terminated_Capability                                     0x0086
#define  RX_Min_SLEEP_NoConfig_Time_Capability                           0x0087
#define  RX_Min_STALL_NoConfig_Time_Capability                           0x0088
#define  RX_Min_SAVE_Config_Time_Capability                              0x0089
#define  RX_REF_CLOCK_SHARED_Capability                                  0x008A
#define  RX_HS_G1_SYNC_LENGTH_Capability                                 0x008B
#define  RX_HS_G1_PREPARE_LENGTH_Capability                              0x008C
#define  RX_LS_PREPARE_LENGTH_Capability                                 0x008D
#define  RX_PWM_Burst_Closure_Length_Capability                          0x008E
#define  RX_Min_ActivateTime_Capability                                  0x008F
#define  RX_Hibern8Time_Capability                                       0x0092
#define  RX_PWM_G6_G7_SYNC_LENGTH_Capability                             0x0093
#define  RX_HS_G2_SYNC_LENGTH_Capability                                 0x0094
#define  RX_HS_G3_SYNC_LENGTH_Capability                                 0x0095
#define  RX_HS_G2_PREPARE_LENGTH_Capability                              0x0096
#define  RX_HS_G3_PREPARE_LENGTH_Capability                              0x0097
#define  RX_Advanced_Granularity_Capability                              0x0098
#define  RX_Advanced_Hibern8Time_Capability                              0x0099
#define  RX_Advanced_Min_ActivateTime_Capability                         0x009A
#define  L0_TX_Capability                                                0x0101 
#define  L0_TX_OptCapability1                                            0x0102
#define  L0_TX_OptCapability2                                            0x0103
#define  L0_TX_OptCapability3                                            0x0104
#define  L0_OMC_WO                                                       0x0105
#define  L0_RX_Capability                                                0x0106
#define  L0_RX_OptCapability1                                            0x0107
#define  L0_RX_OptCapability2                                            0x0108
#define  L0_RX_OptCapability3                                            0x0109
#define  L0_RX_OptCapability4                                            0x010A
#define  L0_RX_OptCapability5                                            0x010B
#define  L0_OMC_READ_DATA0                                               0x010C
#define  L0_OMC_READ_DATA1                                               0x010D
#define  L0_OMC_READ_DATA2                                               0x010E
#define  L0_OMC_READ_DATA3                                               0x010F
#define  L1_TX_Capability                                                0x0111
#define  L1_TX_OptCapability1                                            0x0112
#define  L1_TX_OptCapability2                                            0x0113
#define  L1_TX_OptCapability3                                            0x0114
#define  L1_OMC_WO                                                       0x0115
#define  L1_RX_Capability                                                0x0116
#define  L1_RX_OptCapability1                                            0x0117
#define  L1_RX_OptCapability2                                            0x0118
#define  L1_RX_OptCapability3                                            0x0119
#define  L1_RX_OptCapability4                                            0x011A
#define  L1_RX_OptCapability5                                            0x011B
#define  L1_OMC_READ_DATA0                                               0x011C
#define  L1_OMC_READ_DATA1                                               0x011D
#define  L1_OMC_READ_DATA2                                               0x011E
#define  L1_OMC_READ_DATA3                                               0x011F
#define  L2_TX_Capability                                                0x0121
#define  L2_TX_OptCapability1                                            0x0122
#define  L2_TX_OptCapability2                                            0x0123
#define  L2_TX_OptCapability3                                            0x0124
#define  L2_OMC_WO                                                       0x0125
#define  L2_RX_Capability                                                0x0126
#define  L2_RX_OptCapability1                                            0x0127
#define  L2_RX_OptCapability2                                            0x0128
#define  L2_RX_OptCapability3                                            0x0129
#define  L2_RX_OptCapability4                                            0x012A
#define  L2_RX_OptCapability5                                            0x012B
#define  L2_OMC_READ_DATA0                                               0x012C
#define  L2_OMC_READ_DATA1                                               0x012D
#define  L2_OMC_READ_DATA2                                               0x012E
#define  L2_OMC_READ_DATA3                                               0x012F
#define  L3_TX_Capability                                                0x0131
#define  L3_TX_OptCapability1                                            0x0132
#define  L3_TX_OptCapability2                                            0x0133
#define  L3_TX_OptCapability3                                            0x0134
#define  L3_OMC_WO                                                       0x0135
#define  L3_RX_Capability                                                0x0136
#define  L3_RX_OptCapability1                                            0x0137 
#define  L3_RX_OptCapability2                                            0x0138
#define  L3_RX_OptCapability3                                            0x0139
#define  L3_RX_OptCapability4                                            0x013A
#define  L3_RX_OptCapability5                                            0x013B
#define  L3_OMC_READ_DATA0                                               0x013C
#define  L3_OMC_READ_DATA1                                               0x013D
#define  L3_OMC_READ_DATA2                                               0x013E
#define  L3_OMC_READ_DATA3                                               0x013F
#define  Selector                                                        0x0160
#define  MAP_RX_LANE0                                                    0x0170
#define  MAP_RX_LANE1                                                    0x0171
#define  MAP_RX_LANE2                                                    0x0172
#define  MAP_RX_LANE3                                                    0x0173
#define  MAP_TX_LANE0                                                    0x0174
#define  MAP_TX_LANE1                                                    0x0175
#define  MAP_TX_LANE2                                                    0x0176
#define  MAP_TX_LANE3                                                    0x0177
#define  PWRMODE_MODE_ERR                                                0x0180
#define  PWRMODE_SERIE_ERR                                               0x0181
#define  PWRMODE_GEAR_ERR                                                0x0182
#define  PWRMODE_HIBERNATE_ERR                                           0x0183
#define  PWRMODE_TERMINATION_ERR                                         0x0184
#define  PWRMODE_CTRL                                                    0x0185
#define  L0RXPowerMode                                                   0x018A
#define  L1RXPowerMode                                                   0x018B
#define  L2RXPowerMode                                                   0x018C
#define  L3RXPowerMode                                                   0x018D
#define  PA_PHY_Type                                                     0x1500
#define  PA_AvailTxDataLanes                                             0x1520
#define  PA_AvailRxDataLanes                                             0x1540
#define  PA_MinRxTrailingClocks                                          0x1543
#define  PA_TxHsG1SyncLength                                             0x1552
#define  PA_TxHsG1PrepareLength                                          0x1553
#define  PA_TxHsG2SyncLength                                             0x1554
#define  PA_TxHsG2PrepareLength                                          0x1555
#define  PA_TxHsG3SyncLength                                             0x1556
#define  PA_TxHsG3PrepareLength                                          0x1557
#define  PA_TxMk2Extension                                               0x155A
#define  PA_PeerScrambling                                               0x155B
#define  PA_TxSkip                                                       0x155C
#define  PA_TxSkipPeriod                                                 0x155D
#define  PA_Local_TX_LCC_Enable                                          0x155E
#define  PA_Peer_TX_LCC_Enable                                           0x155F
#define  PA_ActiveTxDataLanes                                            0x1560
#define  PA_ConnectedTxDataLanes                                         0x1561
#define  PA_TxTrailingClocks                                             0x1564
#define  PA_TxPWRStatus                                                  0x1567
#define  PA_TxGear                                                       0x1568
#define  PA_TxTermination                                                0x1569
#define  PA_HSSeries                                                     0x156A
#define  PA_PWRMode                                                      0x1571
#define  PA_ActiveRxDataLanes                                            0x1580
#define  PA_ConnectedRxDataLanes                                         0x1581
#define  PA_RxPWRStatus                                                  0x1582
#define  PA_RxGear                                                       0x1583
#define  PA_RxTermination                                                0x1584
#define  PA_Scrambling                                                   0x1585
#define  PA_MaxRxPWMGear                                                 0x1586
#define  PA_MaxRxHSGear                                                  0x1587
#define  PA_PACPReqTimeout                                               0x1590
#define  PA_PACPReqEoBTimeout                                            0x1591
#define  PA_RemoteVerInfo                                                0x15A0
#define  PA_LogicalLaneMap                                               0x15A1
#define  PA_SleepNoConfigTime                                            0x15A2
#define  PA_StallNoConfigTime                                            0x15A3
#define  PA_SaveConfigTime                                               0x15A4
#define  PA_RxHSUnterminationCapability                                  0x15A5
#define  PA_RxLSTerminationCapability                                    0x15A6
#define  PA_Hibern8Time                                                  0x15A7
#define  PA_TActivate                                                    0x15A8
#define  PA_LocalVerInfo                                                 0x15A9
#define  PA_Granularity                                                  0x15AA
#define  PA_MK2ExtensionGuardBand                                        0x15AB
#define  PA_PWRModeUserData0                                             0x15B0
#define  PA_PWRModeUserData1                                             0x15B1
#define  PA_PWRModeUserData2                                             0x15B2
#define  PA_PWRModeUserData3                                             0x15B3
#define  PA_PWRModeUserData4                                             0x15B4
#define  PA_PWRModeUserData5                                             0x15B5
#define  PA_PWRModeUserData6                                             0x15B6
#define  PA_PWRModeUserData7                                             0x15B7
#define  PA_PWRModeUserData8                                             0x15B8
#define  PA_PWRModeUserData9                                             0x15B9
#define  PA_PWRModeUserData10                                            0x15BA
#define  PA_PWRModeUserData11                                            0x15BB
#define  PA_PACPFrameCount                                               0x15C0
#define  PA_PACPErrorCount                                               0x15C1
#define  PA_PHYTestControl                                               0x15C2
#define  DL_TxPreemptionCap                                              0x2000
#define  DL_TC0TxMaxSDUSize                                              0x2001
#define  DL_TC0RxInitCreditVal                                           0x2002
#define  DL_TC1TxMaxSDUSize                                              0x2003
#define  DL_TC1RxInitCreditVal                                           0x2004
#define  DL_TC0TxBufferSize                                              0x2005
#define  DL_TC1TxBufferSize                                              0x2006
#define  DL_TC0TXFCThreshold                                             0x2040
#define  DL_FC0ProtectionTimeOutVal                                      0x2041
#define  DL_TC0ReplayTimeOutVal                                          0x2042
#define  DL_AFC0ReqTimeOutVal                                            0x2043
#define  DL_AFC0CreditThreshold                                          0x2044
#define  DL_TC0OutAckThreshold                                           0x2045
#define  DL_PeerTC0Present                                               0x2046
#define  DL_PeerTC0RxInitCreditVal                                       0x2047
#define  DL_TC1TXFCThreshold                                             0x2060
#define  DL_FC1ProtectionTimeOutVal                                      0x2061
#define  DL_TC1ReplayTimeOutVal                                          0x2062
#define  DL_AFC1ReqTimeOutVal                                            0x2063
#define  DL_AFC1CreditThreshold                                          0x2064
#define  DL_TC1OutAckThreshold                                           0x2065
#define  DL_PeerTC1Present                                               0x2066
#define  DL_PeerTC1RxInitCreditVal                                       0x2067
#define  N_DeviceID                                                      0x3000
#define  N_DeviceID_valid                                                0x3001
#define  N_TC0TxMaxSDUSize                                               0x3020
#define  N_TC1TxMaxSDUSize                                               0x3021
#define  T_NumCPorts                                                     0x4000
#define  T_NumTestFeatures                                               0x4001
#define  T_ConnectionState                                               0x4020
#define  T_PeerDeviceID                                                  0x4021
#define  T_PeerCPortID                                                   0x4022
#define  T_TrafficClass                                                  0x4023
#define  T_ProtocolID                                                    0x4024
#define  T_CPortFlags                                                    0x4025
#define  T_TxTokenValue                                                  0x4026
#define  T_RxTokenValue                                                  0x4027
#define  T_LocalBufferSpace                                              0x4028
#define  T_PeerBufferSpace                                               0x4029
#define  T_CreditsToSend                                                 0x402A
#define  T_CPortMode                                                     0x402B
#define  T_TC0TxMaxSDUSize                                               0x4060
#define  T_TC1TxMaxSDUSize                                               0x4061
#define  T_TstCPortID                                                    0x4080
#define  T_TstSrcOn                                                      0x4081
#define  T_TstSrcPattern                                                 0x4082
#define  T_TstSrcIncrement                                               0x4083
#define  T_TstSrcMessageSize                                             0x4084
#define  T_TstSrcMessageCount                                            0x4085
#define  T_TstSrcInterMessageGap                                         0x4086
#define  DME_LocalFC0ProtectionTimeOutVal                                0xD041
#define  DME_LocalTC0ReplayTimeOutVal                                    0xD042
#define  DME_LocalAFC0ReqTimeOutVal                                      0xD043
#define  DME_LocalFC1ProtectionTimeOutVal                                0xD044
#define  DME_LocalTC1ReplayTimeOutVal                                    0xD045
#define  DME_LocalAFC1ReqTimeOutVal                                      0xD046
#define  DL_FC1ProtTimeOutVal                                            0x2061
#define  DBG_PA_OPTION_SUITE                                             0x9564
#define  DBG_CLK_PERIOD                                                  0x9514

//----------------------------------------------------------------------------------------------------------------------------
// Define Internal Attributes
//----------------------------------------------------------------------------------------------------------------------------
#define  PA_TX_SKIP_VAL                                                  0x00000001
#define  PA_DBG_CLK_PERIOD                                               0x00003850
#define  PA_DBG_AUTOMODE_THLD                                            0x000038d8
#define  PA_DBG_OPTION_SUITE                                             0x00003990
#define  DL_FC0_PROTECTION_TIMEOUT_VAL_VALUE                             0x00001FFF
#define  DL_FC1_PROTECTION_TIMEOUT_VAL_VALUE                             0x00001FFF
#define  DME_LINKSTARTUP_REQ                                             0x00000016
#define  N_DEVICE_ID_VAL                                                 0x00000000
#define  N_DEVICE_ID_VALID_VAL                                           0x00000001
#define  T_CONNECTION_STATE_OFF_VAL                                      0x00000000
#define  T_CONNECTION_STATE_ON_VAL                                       0x00000001
#define  T_PEER_DEVICE_ID_VAL                                            0x00000001
#define  DME_LINKSTARTUP_CMD                                             0x00000016
#define  T_DBG_CPORT_OPTION_SUITE                                        0x00006890
#define  T_DBG_OPTION_SUITE_VAL                                          0x0000000D
#define  TX_LINERESET_PVALUE_MSB                                         0x000000ab
#define  TX_LINERESET_PVALUE_LSB                                         0x000000ac
#define  RX_LINERESET_VALUE_MSB                                          0x0000001c
#define  RX_LINERESET_VALUE_LSB                                          0x0000001d
#define  RX_OVERSAMPLING_ENABLE                                          0x00000076
#define  TX_LANE_0                                                       0x00000000
#define  TX_LANE_1                                                       0x00000001
#define  RX_LANE_0                                                       0x00000004
#define  RX_LANE_1                                                       0x00000005
#define  DBG_OV_TM                                                       0x00000200

//----------------------------------------------------------------------------------------------------------------------------
// HCI Specific Initialization Defines
//----------------------------------------------------------------------------------------------------------------------------
#define  CONV_UTMRLBA_LOW_VAL                                            0x7D303800 //UTP Task Management Request List Base Address 
#define  CONV_UTMRLBA_HIGH_VAL                                           0x00000000 //UTP Transfer Request List Base Address
#define  UTMRLBA_LOW_VAL                                                 0x1D303800
#define  UTMRLBA_HIGH_VAL                                                0x00000000
#define  CONV_UTRLBA_LOW_VAL                                             0x7D300000
#define  CONV_UTRLBA_HIGH_VAL                                            0x00000000
#define  UTRLBA_LOW_VAL                                                  0x1D300000
#define  UTRLBA_HIGH_VAL                                                 0x00000000
#define  CONV_UCDBA_LOW_VAL                                              0x7D300800 //UTP  Command  Descriptor  Base  Address
#define  CONV_UCDBAU_HIGH_VAL                                            0x00000000
#define  UCDBA_LOW_VAL                                                   0x1D300800
#define  UCDBAU_HIGH_VAL                                                 0x00000000
#define  CONV_DBA_LOW_VAL                                                0x7D301000 //Data  Base  Address
#define  CONV_DBAU_HIGH_VAL                                              0x00000000
#define  DBA_LOW_VAL                                                     0x1D301000
#define  DBAU_HIGH_VAL                                                   0x00000000
#define  DBA_OFFSET                                                      0x00000000
#define  UCD_OFFSET                                                      0x00000000 //UTP Command Descriptor 
#define  PRDT_OFFSET                                                     0x00000200 //Physical Region Description Table
#define  RESP_OFFSET                                                     0x00000100
#define  RESP_LENGTH                                                     0x00000040
#define  UTRL_OFFSET                                                     0x00000020
#define  UTMRL_OFFSET                                                    0x00000050
#define  AXI_ADDR_WIDTH                                                  0x35
#define  UTRD_DOORBELL_SIZE                                              0x16
#define  UTMRD_DOORBELL_SIZE                                             0x4
#define  UTRIACR_VAL                                                     0x0100060a
#define  ENABLE                                                          0x00000001
#define  DISABLE                                                         0x00000000
#define  TXPRDT_ENTRY_SIZE_VAL                                           0xFFFFFFFF
#define  RXPRDT_ENTRY_SIZE_VAL                                           0xFFFFFFFF
#define  TO_CNT_VAL_1US_VAL                                              0x00000064
#define  MISC_VAL                                                        0x000000f0
#define  DEFAULT_PAGE_CODE_VALUE                                         0x0
#define  SUPPORTED_VPD                                                   0x0
#define  MODE_PAGE_VPD                                                   0x87
#define  DEFAULT_ALLOCATION_LENGTH                                       0x24
#define  DEFAULT_CONTROL_VALUE                                           0x0
#define  VIP_LOGICAL_BLOCK_SIZE                                          0x00001000
#define  CONTROL                                                         0x0A
#define  READ_WRITE_ERROR_RECOVERY                                       0x01
#define  CACHING                                                         0x08
#define  ALL_PAGES                                                       0x3F
#define  DEFAULT_SUBPAGE_CODE                                            0x0

//----------------------------------------------------------------------------------------------------------------------------
// UFS Command Specific Defines
//----------------------------------------------------------------------------------------------------------------------------
#define  NOP_OUT                                                         0x00
#define  COMMAND                                                         0x01
#define  DATA_OUT                                                        0x02
#define  TASK_MANAGEMENT_REQUEST                                         0x04
#define  QUERY_REQUEST                                                   0x16
#define  NOP_IN                                                          0x20
#define  RESPONSE                                                        0x21
#define  DATA_IN                                                         0x22
#define  TASK_MANAGEMENT_RESPONSE                                        0x24
#define  READY_TO_TRANSFER                                               0x31
#define  QUERY_RESPONSE                                                  0x36
#define  REJECT                                                          0x3f
#define  LUN_MIN_VALUE                                                   0x00
#define  LUN_MAX_VALUE                                                   0x07
#define  SCSI                                                            0x00
#define  UFS_SPECIFIC                                                    0x01
#define  VENDOR_SPECIFIC_MIN                                             0x08
#define  VENDOR_SPECIFIC_MAX                                             0x0f
#define  MAC_AUTHENTICATION_KEY                                          0xABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEFABCDBCDECDEF
#define  RPMB_LUN                                                        0xc4
#define  P_ABORT_TASK                                                    0x01
#define  P_ABORT_TASK_SET                                                0x02
#define  P_CLEAR_TASK_SET                                                0x04
#define  P_LUN_RESET                                                     0x08
#define  P_QUERY_TASK                                                    0x80
#define  P_QUERY_TASK_SET                                                0x81
#define  INVALID_UPIU                                                    0xEE
#define  I_MANUFACTURER_NAME                                             0x11
#define  I_PRODUCT_NAME                                                  0x22
#define  I_OEM_ID                                                        0x44
#define  I_SERIAL_NUMBER                                                 0x33
#define  I_PRODUCT_REVISION_LEVEL                                        0xAA
#define  DEBUG_REG_START                                                 (UFS_HCI_BASE + 0x8000)
#define  DEBUG_REG_START_UTMRL                                           (UFS_HCI_BASE + 0x8600)
#define  STANDARD_READ_QUERY                                             0x01
#define  STANDARD_WRITE_QUERY                                            0x81
#define  QUERY_READ_FLAG                                                 0x05
#define  QUERY_SET_FLAG                                                  0x06
#define  QUERY_CLEAR_FLAG                                                0x07
#define  QUERY_TOGGLE_FLAG                                               0x08
#define  DBA_IRAM_LOW_VAL                                                0x1D020000 // IRAM Base Address
#define  DBAU_IRAM_HIGH_VAL                                              0x00000000
#define  CONV_DBA_IRAM_LOW_VAL                                           0x7D020000 //  RAM Base Address +0x6000_0000 (remapped)
#define  CONV_DBAU_IRAM_HIGH_VAL                                         0x00000000

 struct ufs_tcc_host {
        struct ufs_hba *hba;
        void __iomem *ufs_sys_ctrl;
        void __iomem *ufs_reg_hci;
        void __iomem *ufs_reg_unipro;
        void __iomem *ufs_reg_fmp;
        void __iomem *ufs_reg_mphy;
        struct reset_control    *rst;
        struct reset_control    *assert;
        uint64_t caps;

        int avail_ln_rx;
        int avail_ln_tx;

        u32 busthrtl_backup;
        u32 reset_gpio;

        bool in_suspend;

        struct ufs_pa_layer_attr dev_req_params;
 };

 #define ufs_hci_writel(host, val, reg)                                    \
        writel((val), (host)->ufs_reg_hci + (reg))
 #define ufs_hci_readl(host, reg) readl((host)->ufs_reg_hci + (reg))
 #define ufs_hci_set_bits(host, mask, reg)                                 \
        ufs_hci_writel(                                                   \
                (host), ((mask) | (ufs_hci_readl((host), (reg)))), (reg))
 #define ufs_hci_clr_bits(host, mask, reg)                                 \
       ufs_hci_writel((host),                                            \
                           ((~(mask)) & (ufs_hci_readl((host), (reg)))), \
                           (reg))

 #define ufs_unipro_writel(host, val, reg)                                    \
        writel((val), (host)->ufs_reg_unipro + (reg))
 #define ufs_unipro_readl(host, reg) readl((host)->ufs_reg_unipro + (reg))
 #define ufs_unipro_set_bits(host, mask, reg)                                 \
        ufs_unipro_writel(                                                   \
                (host), ((mask) | (ufs_unipro_readl((host), (reg)))), (reg))
 #define ufs_unipro_clr_bits(host, mask, reg)                                 \
       ufs_unipro_writel((host),                                            \
                           ((~(mask)) & (ufs_unipro_readl((host), (reg)))), \
                           (reg))
						   
 #define ufs_fmp_writel(host, val, reg)                                    \
        writel((val), (host)->ufs_reg_fmp + (reg))
 #define ufs_fmp_readl(host, reg) readl((host)->ufs_reg_fmp + (reg))
 #define ufs_fmp_set_bits(host, mask, reg)                                 \
        ufs_fmp_writel(                                                   \
                (host), ((mask) | (ufs_fmp_readl((host), (reg)))), (reg))
 #define ufs_fmp_clr_bits(host, mask, reg)                                 \
       ufs_fmp_writel((host),                                            \
                           ((~(mask)) & (ufs_fmp_readl((host), (reg)))), \
                           (reg))

 #define ufs_mphy_writel(host, val, reg)                                    \
        writel((val), (host)->ufs_reg_mphy + (reg))
 #define ufs_mphy_readl(host, reg) readl((host)->ufs_reg_mphy + (reg))
 #define ufs_mphy_set_bits(host, mask, reg)                                 \
        ufs_mphy_writel(                                                   \
                (host), ((mask) | (ufs_mphy_readl((host), (reg)))), (reg))
 #define ufs_mphy_clr_bits(host, mask, reg)                                 \
       ufs_mphy_writel((host),                                            \
                           ((~(mask)) & (ufs_mphy_readl((host), (reg)))), \
                           (reg))
#endif /* __UFS_TCC_H_ */
