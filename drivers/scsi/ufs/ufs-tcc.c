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

#define UNIPRO_PCLK_PERIOD_NS 6 //145 MHz < PCLK_Freq <= 170 MHz
#define CALCULATED_VALUE 0x4E20 //UNIPRO_PCLK_PERIOD_NS * 6 = 120,000

//#define USE_POLL
#ifdef USE_POLL                
int ufs_check_link_error(void);
volatile uint  uic_linkup_done;
#endif
unsigned int debug;

#ifdef USE_POLL                                                                           
int ufs_check_link_error(struct ufs_tcc_host *host)
{                                                                                         
    unsigned int rd_data;
    int res = 0;
                                                                                          
    //rd_data = HCI_IS;                                                                     
	rd_data = ufs_sys_ctrl_readl(host,HCI_IS);

    if((uic_linkup_done == 1) && (reg_rdb(rd_data,2) == 1))                               
    {                                                                                     
        printk(KERN_DEBUG "UE: UIC Error");
        HCI_IS = 1<<2;
                                                                                          
        //Check Error register                                                            
        //rd_data = HCI_UECPA;                                                              
		rd_data = ufs_sys_ctrl_readl(host, HCI_UECPA);
        if(reg_rdb(rd_data,31) == 1)                                                      
        {                                                                                 
            printk(KERN_DEBUG " UECPA : UIC PHY Adapter Layer Error (0x%x)", (0x1f & rd_data));  
        }                                                                                 
                                                                                          
        //rd_data = HCI_UECDL;
		rd_data = ufs_sys_ctrl_readl(host, HCI_UECDL);
        if(reg_rdb(rd_data,31) == 1)                                                      
        {                                                                                 
            printk(KERN_DEBUG " UECDL : UIC Data Link Layer Error (0x%x)", (0x7fff & rd_data));  
        }                                                                                 

        //rd_data = HCI_UECN;                                                               
		rd_data = ufs_sys_ctrl_readl(host, HCI_UECN);
        if(reg_rdb(rd_data,31) == 1)
        {
            printk(KERN_DEBUG " UECN : UIC network layer error (0x%x)", (0x7 & rd_data));        
        }
                                                                                          
        //rd_data = HCI_UECT;                                                               
		rd_data = ufs_sys_ctrl_readl(host, HCI_UECT);
        if(reg_rdb(rd_data,31) == 1)
        {                                                                                 
            printk(KERN_DEBUG " UECT : UIC transport layer error (0x%x)", (0x7f & rd_data));     
        }
                                                                                          
        //rd_data = HCI_UECDME;                                                             
		rd_data = ufs_sys_ctrl_readl(host, HCI_UECDME);
        if(reg_rdb(rd_data,31) == 1)
        {                                                                                 
            printk(KERN_DEBUG " UECDME : UIC DME error (0x%x)", (0x1 & rd_data));                
        }                                                                                 
        res = -1;                                                                         
    }                                                                                     
                                                                                          
    return res;                                                                           
}
#endif                                                                                    

static int ufs_tcc_get_resource(struct ufs_tcc_host *host)
{
       struct resource *mem_res;
       struct device *dev = host->hba->dev;
       struct platform_device *pdev = to_platform_device(dev);

       /* get resource of ufs sys ctrl */
	   #if 0
       mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
       host->ufs_sys_ctrl = devm_ioremap_resource(dev, mem_res);
       if (!host->ufs_sys_ctrl) {
               dev_err(dev, "cannot ioremap for ufs sys ctrl register\n");
               return -ENOMEM;
       }
	   #endif

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-hci");
       host->ufs_reg_hci = devm_ioremap_resource(dev, mem_res);
       if (!host->ufs_reg_hci) {
               dev_err(dev, "cannot ioremap for ufs hci register\n");
               return -ENOMEM;
       }

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-unipro");
       host->ufs_reg_unipro = devm_ioremap_resource(dev, mem_res);
       if (!host->ufs_reg_unipro) {
               dev_err(dev, "cannot ioremap for ufs unipro register\n");
               return -ENOMEM;
       }

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-fmp");
       host->ufs_reg_fmp = devm_ioremap_resource(dev, mem_res);
       if (!host->ufs_reg_fmp) {
               dev_err(dev, "cannot ioremap for ufs fmp register\n");
               return -ENOMEM;
       }

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-mphy");
       host->ufs_reg_mphy = devm_ioremap_resource(dev, mem_res);
       if (!host->ufs_reg_mphy) {
               dev_err(dev, "cannot ioremap for ufs mphy register\n");
               return -ENOMEM;
       }

       return 0;
}

static void ufs_tcc_set_pm_lvl(struct ufs_hba *hba)
{
       hba->rpm_lvl = UFS_PM_LVL_1;
       hba->spm_lvl = UFS_PM_LVL_3;
}

static void ufs_tcc_populate_dt(struct device *dev,
                                  struct ufs_tcc_host *host)
{
       struct device_node *np = dev->of_node;
       int ret;

       if (!np) {
               dev_err(dev, "can not find device node\n");
               return;
       }
#if 0
       if (of_find_property(np, "ufs-tcc-use-rate-B", NULL))
               host->caps |= USE_RATE_B;

       if (of_find_property(np, "ufs-tcc-broken-fastauto", NULL))
               host->caps |= BROKEN_FASTAUTO;

       if (of_find_property(np, "ufs-tcc-use-one-line", NULL))
               host->caps |= USE_ONE_LANE;

       if (of_find_property(np, "ufs-tcc-use-HS-GEAR3", NULL))
               host->caps |= USE_HS_GEAR3;

       if (of_find_property(np, "ufs-tcc-use-HS-GEAR2", NULL))
               host->caps |= USE_HS_GEAR2;

       if (of_find_property(np, "ufs-tcc-use-HS-GEAR1", NULL))
               host->caps |= USE_HS_GEAR1;

       if (of_find_property(np, "ufs-tcc-broken-clk-gate-bypass", NULL))
               host->caps |= BROKEN_CLK_GATE_BYPASS;
#endif
       host->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
       if (gpio_is_valid(host->reset_gpio)) {
               ret = devm_gpio_request_one(dev, host->reset_gpio,
                                           GPIOF_DIR_OUT, "tcc_ufs_reset");
               if (ret < 0)
                       dev_err(dev, "could not acquire gpio (err=%d)\n", ret);
       }
}

static void ufs_tcc_clk_init(struct ufs_hba *hba)
{
	//struct ufs_tcc_host *host = ufshcd_get_variant(hba);

	//ufs_sys_ctrl_clr_bits(host, BIT_SYSCTRL_REF_CLOCK_EN, PHY_CLK_CTRL);
	//if (ufs_sys_ctrl_readl(host, PHY_CLK_CTRL) & BIT_SYSCTRL_REF_CLOCK_EN)
	//	mdelay(1);
	/* use abb clk */
	//ufs_sys_ctrl_clr_bits(host, BIT_UFS_REFCLK_SRC_SEl, UFS_SYSCTRL);
	//ufs_sys_ctrl_clr_bits(host, BIT_UFS_REFCLK_ISO_EN, PHY_ISO_EN);
	/* open mphy ref clk */
	//ufs_sys_ctrl_set_bits(host, BIT_SYSCTRL_REF_CLOCK_EN, PHY_CLK_CTRL);
}

static int ufs_tcc_link_startup_pre_change(struct ufs_hba *hba)
{
	uint32_t  rd_data;
	uint32_t  cport_log_en = 0;
	uint32_t  wlu_enable = 0;
	uint32_t  wlu_burst_len = 3;
	uint32_t  hci_buffering_enable = 0;
	uint32_t  axidma_rwdataburstlen = 0;
	uint32_t  no_of_beat_burst = 7;
	//uint32_t  otp_data=0;
	//int  ext_clk_sel = 0; //Internal Clock default
	int res = 0;
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Release refclk for device
	//---------------------------------------------------------------------------------------------------------------------------------------
	//HCI_CLKSTOP_CTRL = 0x0; // Disable
	ufs_hci_writel(host, 0x0, HCI_CLKSTOP_CTRL);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Release Device reset
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Release device reset");
	//HCI_GPIO_OUT = 0x1;
	ufs_hci_writel(host, 0x1, HCI_GPIO_OUT);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// AUTO CLOCK GATING DISABLE
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Disable HWACG Feature of UFS");
	//HCI_UFS_ACG_DISABLE = 0x1;
	ufs_hci_writel(host, 0x1, HCI_UFS_ACG_DISABLE);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// ENABLE HCI
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::ENABLE HCI");
	//HCI_HCE = 0x1;
	ufs_hci_writel(host, 0x1, HCI_HCE);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Read HCI VERSION
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::HCI VERSION DETAILS");
	//rd_data = HCI_UFS_LINK_VERSION;
	rd_data = ufs_hci_readl(host, HCI_UFS_LINK_VERSION);
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::UFS_LINK MAJOR VERSION = 0x%x",rd_data);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Check if HCI is Enabled
	//---------------------------------------------------------------------------------------------------------------------------------------
	//rd_data = HCI_HCE; //Host Controller Enable
	rd_data = ufs_hci_readl(host, HCI_HCE);

	while(rd_data != 0x1){
		//rd_data = HCI_HCE;
		rd_data = ufs_hci_readl(host, HCI_HCE);
	}
	if (res < 0)
		goto done;

	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::HCI_HCE Status = 0x%x",rd_data);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Check if UIC is ready to accept Configuration Commands
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) {
		printk(KERN_DEBUG "BASIC_LINK_UP::Check if UIC is ready to accept Configuration Commands");
		printk(KERN_DEBUG "BASIC_LINK_UP::Reading value of Hcs. UCRDY Should be 1 before proceeding with UIC COMMAND");
	}
	//rd_data = HCI_HCS;  //Host Controller Status
	rd_data = ufs_hci_readl(host, HCI_HCS);

	while(reg_rdb(rd_data,3) != 1){ // Check UIC COMMAND Ready (UCRDY)
		//rd_data = HCI_HCS;
		rd_data = ufs_hci_readl(host, HCI_HCS);
	}
	if (res < 0)
		goto done;

	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::HCI_HCS Status = 0x%x",rd_data);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// ENABLE CPORT LOG FEATURE
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(cport_log_en) {
		//HCI_CPORT_LOG_CTRL = 1;
		ufs_hci_writel(host, 0x1, HCI_CPORT_LOG_CTRL);
		//HCI_CPORT_LOG_CFG  = 0;
		ufs_hci_writel(host, 0x0, HCI_CPORT_LOG_CTRL);
	}

	//HCI_IE = 0x7FFF;
	ufs_hci_writel(host, 0x7FFF, HCI_IE);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some MPHY registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some MPHY registers");
	///dme_set_req(DBG_OV_TM, 0x40, 0x0, 0x0); //the DME_SET command to set UIC attributes
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(DBG_OV_TM, 0x0), 0x40);

	//    dme_set_req(0x9a, 0x0f, RX_LANE_0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x009a, RX_LANE_0), 0xf);
	//    dme_set_req(0x9a, 0x0f, RX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x009a, RX_LANE_1), 0xf);
	///dme_set_req(0x93, 0xff, RX_LANE_0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0093, RX_LANE_0), 0xff);
	//dme_set_req(0x93, 0xff, RX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0093, RX_LANE_1), 0xff);
	//dme_set_req(0x99, 0xff, RX_LANE_0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0099, RX_LANE_0), 0xff);
	//dme_set_req(0x99, 0xff, RX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0099, RX_LANE_1), 0xff);
	//dme_set_req(0x8F, 0x3A, TX_LANE_0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x008F, TX_LANE_0), 0x3a);
	//dme_set_req(0x8F, 0x3A, TX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x008F, TX_LANE_1), 0x3a);

	//dme_set_req(TX_LINERESET_PVALUE_LSB, 0x50, TX_LANE_0, 0x0);
	//dme_set_req(TX_LINERESET_PVALUE_LSB, 0x50, TX_LANE_1, 0x0);
	//dme_set_req(TX_LINERESET_PVALUE_MSB, 0x0,  TX_LANE_0, 0x0);
	//dme_set_req(TX_LINERESET_PVALUE_MSB, 0x0,  TX_LANE_1, 0x0);
	//dme_set_req(RX_LINERESET_VALUE_LSB,  0x50, RX_LANE_0, 0x0);
	//dme_set_req(RX_LINERESET_VALUE_LSB,  0x50, RX_LANE_1, 0x0);
	//dme_set_req(RX_LINERESET_VALUE_MSB,  0x0,  RX_LANE_0, 0x0);
	//dme_set_req(RX_LINERESET_VALUE_MSB,  0x0,  RX_LANE_1, 0x0);
	//dme_set_req(RX_OVERSAMPLING_ENABLE,  0x4,  RX_LANE_0, 0x0); //UFS post-sim error
	//dme_set_req(RX_OVERSAMPLING_ENABLE,  0x4,  RX_LANE_1, 0x0);

	//dme_set_req(DBG_OV_TM, 0x0, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(DBG_OV_TM, 0x0), 0x0);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some PA MIB registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some PA MIB registers");

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some DL MIB registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some DL MIB registers");

	//dme_set_req(DL_FC0ProtectionTimeOutVal, DL_FC0_PROTECTION_TIMEOUT_VAL_VALUE, 0x0, 0x0);
	//dme_set_req(DL_FC1ProtectionTimeOutVal, DL_FC1_PROTECTION_TIMEOUT_VAL_VALUE, 0x0, 0x0);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of
	//---------------------------------------------------------------------------------------------------------------------------------------
	// Configuration regarding real time specification
	//dme_set_req(PA_DBG_CLK_PERIOD, UNIPRO_PCLK_PERIOD_NS, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_DBG_CLK_PERIOD, 0x0), UNIPRO_PCLK_PERIOD_NS);
	//dme_set_req(PA_DBG_AUTOMODE_THLD, CALCULATED_VALUE, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_DBG_AUTOMODE_THLD, 0x0), CALCULATED_VALUE);
	//dme_set_req(PA_DBG_OPTION_SUITE, 0x2E820183, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_DBG_OPTION_SUITE, 0x0), 0x2E820183);

	// Configuration regarding operation mode
	//dme_set_req(PA_Local_TX_LCC_Enable, 0x0, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_Local_TX_LCC_Enable, 0x0), 0x0);
	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some NL MIB registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some NL MIB registers");

	//dme_set_req(N_DeviceID, N_DEVICE_ID_VAL, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(N_DeviceID, 0x0), N_DEVICE_ID_VAL);
	//dme_set_req(N_DeviceID_valid, N_DEVICE_ID_VALID_VAL, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(N_DeviceID_valid, 0x0), N_DEVICE_ID_VALID_VAL);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some TL MIB registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some TL MIB registers");

	//dme_set_req(T_ConnectionState, T_CONNECTION_STATE_OFF_VAL, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_ConnectionState, 0x0), T_CONNECTION_STATE_OFF_VAL);
	//dme_set_req(T_PeerDeviceID,    T_PEER_DEVICE_ID_VAL, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_PeerDeviceID, 0x0), T_PEER_DEVICE_ID_VAL);
	//dme_set_req(T_ConnectionState, T_CONNECTION_STATE_ON_VAL, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_ConnectionState, 0x0), T_CONNECTION_STATE_ON_VAL);

	//dme_set_req(0x200, 0x40, 0x0, 0x0); // ov_tm on
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x40);
	if(debug) printk(KERN_DEBUG "Setting to reduce CDR Lock M-PHY PCS debugmode = 1\n");
	//dme_set_req(0x62, 0x97, 0x4, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x62, 0x4), 0x97);
	if(debug) printk(KERN_DEBUG "M-PHY PCS RX0\n");
	//dme_set_req(0x63, 0x70, 0x4, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x63, 0x4), 0x70);
	if(debug) printk(KERN_DEBUG "M-PHY PCS RX0 sync_mask_cnt = 6,000\n");
	//dme_set_req(0x62, 0x97, 0x5, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x62, 0x5), 0x97);
	if(debug) printk(KERN_DEBUG "M-PHY PCS RX1\n");
	//dme_set_req(0x63, 0x70, 0x5, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x63, 0x5), 0x70);
	if(debug) printk(KERN_DEBUG "M-PHY PCS RX1 sync_mask_cnt = 6,000 \n");
	//dme_set_req(0x21, 0x2, 0x0, 0x0); // M-PHY PCS Initial Setting
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x21, 0x0), 0x02);
	if(debug) printk(KERN_DEBUG "M-PHY PCS TX1\n");
	//dme_set_req(0x200, 0x0, 0x0, 0x0); // ov_tm on
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x0);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Set the data byte reordering.CPORT_BYTE_REORDERING must be 1 when connected to Samsung or Toshiba UFS Device
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Set the data byte reordering.CPORT_BYTE_REORDERING must be 1 when connected to Samsung or Toshiba UFS Device");

	// HCI_DATA_REORDER = 0xA;
	ufs_hci_writel(host, 0x0, HCI_DATA_REORDER);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Disable  FmpSecurity.21st bit has been initialized zero for LHOTSE project.
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Disable  FmpSecurity");
	//FMP_FMPRSECURITY = 0xDFC2E492;
	ufs_hci_writel(host, 0xDFC2E492, FMP_FMPRSECURITY);

	//---------------------------------------------------------------------------------------------------------------------------------------
	//  Now Initialize interrupt enable register to report link start up completion and then send link start up cmd
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Now Initialize interrupt enable register to report link start up completion and then send link start up cmd");

	//HCI_UICCMDR = DME_LINKSTARTUP_REQ;
	ufs_hci_writel(host, DME_LINKSTARTUP_REQ, HCI_UICCMDR);

	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Dme_Link_StartUpCmd sent");

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Link Start Up cmd sent. Now checking proper registers to see if Link start up has been successful
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) {
		printk(KERN_DEBUG "BASIC_LINK_UP::Link Start Up cmd sent. Now checking proper registers to see if Link start up has been successful");
		printk(KERN_DEBUG "BASIC_LINK_UP::Now waiting for Is.uccs and Hcs.dp bits to go high. This will indicate that Link Start Up has Completed Successfully");
	}

	//wait_and_clear_uccs();

	//rd_data = HCI_HCS;
	rd_data = ufs_hci_readl(host, HCI_HCS);

	while(reg_rdb(rd_data,0) != 1) {
		//rd_data = HCI_HCS;
		rd_data = ufs_hci_readl(host, HCI_HCS);
	}

	if (res < 0)
		goto done;

	if(debug) {
		printk(KERN_DEBUG "BASIC_LINK_UP::Done waiting for Hcs.dp bits to go high");
		printk(KERN_DEBUG "BASIC_LINK_UP::Device has been Found during LINKSTARTUP");
		printk(KERN_DEBUG "BASIC_LINK_UP::linkup_done event emitted..");
	}

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Link Start Up Complete. Now initializing UTP transfer management request list base address.
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Link Start Up Complete. Now initializing UTP transfer management request list base address");

	//HCI_UTRIACR  = UTRIACR_VAL;
	ufs_hci_writel(host, UTRIACR_VAL, HCI_UTRIACR);
	//HCI_UTMRLBA  = UTMRLBA_LOW_VAL;
	ufs_hci_writel(host, UTMRLBA_LOW_VAL, HCI_UTMRLBA);
	//HCI_UTMRLBAU = UTMRLBA_HIGH_VAL;
	ufs_hci_writel(host, UTMRLBA_HIGH_VAL, HCI_UTMRLBAU);

	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Link Start Up Complete. Now initializing UTP transfer management request list base address");

	//HCI_UTRIACR  = UTRIACR_VAL;
	ufs_hci_writel(host, UTRIACR_VAL, HCI_UTRIACR);
	//HCI_UTMRLBA  = UTMRLBA_LOW_VAL;
	ufs_hci_writel(host, UTMRLBA_LOW_VAL, HCI_UTMRLBA);
	//HCI_UTMRLBAU = UTMRLBA_HIGH_VAL;
	ufs_hci_writel(host, UTMRLBA_HIGH_VAL, HCI_UTMRLBAU);

	//---------------------------------------------------------------------------------------------------------------------------------------
	//Allocate and initialize UTP transfer request list
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Allocate and initialize UTP transfer request list");

	//HCI_UTRLBA  = UTRLBA_LOW_VAL;
	ufs_hci_writel(host, UTRLBA_LOW_VAL, HCI_UTRLBA);
	//HCI_UTRLBAU = UTRLBA_HIGH_VAL;
	ufs_hci_writel(host, UTRLBA_HIGH_VAL, HCI_UTRLBAU);

	//---------------------------------------------------------------------------------------------------------------------------------------
	//Extra internal register settings
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Extra internal register settings");

	//HCI_TXPRDT_ENTRY_SIZE = TXPRDT_ENTRY_SIZE_VAL;
	ufs_hci_writel(host, TXPRDT_ENTRY_SIZE_VAL, HCI_TXPRDT_ENTRY_SIZE);
	//HCI_RXPRDT_ENTRY_SIZE = RXPRDT_ENTRY_SIZE_VAL;
	ufs_hci_writel(host, RXPRDT_ENTRY_SIZE_VAL, HCI_RXPRDT_ENTRY_SIZE);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Enable the run stop request list registers. Doorbell regsisters now enabled.
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Enable the run stop request list registers. Doorbell regsisters now enabled");

	//HCI_UTMRLRSR = 0x1;
	ufs_hci_writel(host, 0x1, HCI_UTMRLRSR);
	//HCI_UTRLRSR  = 0x1;
	ufs_hci_writel(host, 0x1, HCI_UTRLRSR);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Write Line Unique Feature Enable/Disable
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(wlu_enable == 1) {
		if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Write Line Unique Feature Enable");
		axidma_rwdataburstlen = (1u<<31) | (wlu_burst_len<<27) | 0x3;
	} else {
		axidma_rwdataburstlen = (0u<<31) | (0u<<27) | no_of_beat_burst;
	}

	if(hci_buffering_enable == 1) {
		if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Hci Buffering Feature Enable");
		axidma_rwdataburstlen = (axidma_rwdataburstlen & 0xFFFFFFF0) | no_of_beat_burst;
	} else {
		axidma_rwdataburstlen = (axidma_rwdataburstlen & 0xFFFFFFF0) | 0x0;
	}

	if((hci_buffering_enable == 1) || (wlu_enable == 1)) {
		//HCI_AXIDMA_RWDATA_BURST_LEN = axidma_rwdataburstlen;
		ufs_hci_writel(host, axidma_rwdataburstlen, HCI_AXIDMA_RWDATA_BURST_LEN);
	}

#ifdef USE_POLL
	//clearInterrupt (ignore in LinkUp State)
	//rd_data = HCI_IS;
	rd_data = ufs_hci_readl(host, HCI_IS);
	if(reg_rdb(rd_data,2) == 1)  //Polling status change
	{
		//HCI_IS = 1<<2;           // clear UIC Error(ignore in LinkUp State
		ufs_hci_writel(host, 1u<<2, HCI_IS);
	}
	if(reg_rdb(rd_data,10) == 1) //Polling status change
	{
		//HCI_IS = 1<<10;          // Clear UIC Command Completion Status
		ufs_hci_writel(host, 1u<<10, HCI_IS);
	}

	uic_linkup_done = 1;
#endif
done :

	return res;
}
static int ufs_tcc_link_startup_post_change(struct ufs_hba *hba)
{
	//struct ufs_tcc_host *host = ufshcd_get_variant(hba);

	return 0;
}

static int ufs_tcc_link_startup_notify(struct ufs_hba *hba,
		enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
		case PRE_CHANGE:
			err = ufs_tcc_link_startup_pre_change(hba);
			break;
		case POST_CHANGE:
			err = ufs_tcc_link_startup_post_change(hba);
			break;
		default:
			break;
	}

	return err;
}

//static void ufs_tcc_soc_init(struct ufs_hba *hba)
//{
     //  struct ufs_tcc_host *host = ufshcd_get_variant(hba);
      // u32 reg;
//}

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
	if (!host) {
		dev_err(dev, "%s: no memory for telechips ufs host\n", __func__);
		return -ENOMEM;
	}

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	host->rst = devm_reset_control_get(dev, "rst");
	host->assert = devm_reset_control_get(dev, "assert");

	ufs_tcc_set_pm_lvl(hba);

	ufs_tcc_populate_dt(dev, host);

	err = ufs_tcc_get_resource(host);
	if (err) {
		ufshcd_set_variant(hba, NULL);
		return err;
	}

	ufs_tcc_clk_init(hba);

	//ufs_tcc_soc_init(hba);

	return 0;
}

static struct ufs_hba_variant_ops ufs_hba_tcc_vops = {
	.name = "tcc",
	.init = ufs_tcc_init,
	.link_startup_notify = ufs_tcc_link_startup_notify,
	//.pwr_change_notify = ufs_tcc_pwr_change_notify,
//	.suspend = ufs_tcc_suspend,
//	.resume = ufs_tcc_resume,
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
