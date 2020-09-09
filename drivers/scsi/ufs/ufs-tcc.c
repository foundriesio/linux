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

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-unipro");
       host->ufs_reg_unipro = devm_ioremap_resource(dev, mem_res);
		printk("%s:%d mem_res = %08x\n", __func__, __LINE__, host->ufs_reg_unipro);
       if (!host->ufs_reg_unipro) {
               dev_err(dev, "cannot ioremap for ufs unipro register\n");
               return -ENOMEM;
       }

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-mphy");
       host->ufs_reg_mphy = devm_ioremap_resource(dev, mem_res);
		printk("%s:%d mem_res = %08x\n", __func__, __LINE__, host->ufs_reg_mphy);
       if (!host->ufs_reg_mphy) {
               dev_err(dev, "cannot ioremap for ufs mphy register\n");
               return -ENOMEM;
       }

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-sbus-config");
       host->ufs_reg_sbus_config = devm_ioremap_resource(dev, mem_res);
		printk("%s:%d mem_res = %08x\n", __func__, __LINE__, host->ufs_reg_sbus_config);
       if (!host->ufs_reg_sbus_config) {
               dev_err(dev, "cannot ioremap for ufs sbus config register\n");
               return -ENOMEM;
       }

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-fmp");
       host->ufs_reg_fmp = devm_ioremap_resource(dev, mem_res);
		printk("%s:%d mem_Res = %08x\n", __func__, __LINE__, host->ufs_reg_fmp);
       if (!host->ufs_reg_fmp) {
               dev_err(dev, "cannot ioremap for ufs fmp register\n");
               return -ENOMEM;
       }

       mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs-sec");
       host->ufs_reg_sec = devm_ioremap_resource(dev, mem_res);
		printk("%s:%d mem_Res = %08x\n", __func__, __LINE__, host->ufs_reg_sec);
       if (!host->ufs_reg_sec) {
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

void encryption_setting(struct ufs_hba *hba)
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

int tcc_ufs_smu_setting(struct ufs_hba *hba)
{
	unsigned int smu_bypass = 1;
	unsigned int smu_index  = 0;
	unsigned int desc_type  = 0;
	unsigned int tid, sw, sr, nsw, nsr, ufk, enc, valid;
	unsigned int protbytzpc, select_inline_enc, fmp_on, unmap_disable, nskeyreg, nssmu, nsuser, use_otp_mask;

	unsigned int fmp_bsector;
	unsigned int fmp_esector;
	unsigned int fmp_lun;
	unsigned int fmp_ctrl;
	unsigned int fmp_security;


	struct ufs_tcc_host *host = ufshcd_get_variant(hba);


	encryption_setting(hba);

	tid   = 0;
	sw    = (smu_bypass)? 1 : 1;
	sr    = (smu_bypass)? 1 : 1;
	nsw   = (smu_bypass)? 1 : 1;
	nsr   = (smu_bypass)? 1 : 1;
	ufk   = (smu_bypass)? 1 : 1;
	enc   = (smu_bypass)? 0 : 1;
	valid = (smu_bypass)? ((smu_index==0)? 1 : 0) : 1;

	fmp_bsector = 0x0;
	fmp_esector = 0xFFFFFFFF;
	fmp_lun     = 0x7;
	fmp_ctrl = (0x0   << 9) |
		(tid   << 8) | //tid
		(sw    << 7) | //sw
		(sr    << 6) | //sr
		(nsw   << 5) | //nsw
		(nsr   << 4) | //nsr
		(ufk   << 3) | //ufk
		(0x0   << 2) | //
		(enc   << 1) | //enc
		(valid << 0) ; //valid

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

	switch(smu_index) {
		case 0:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN0);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND0);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN0);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL0); break;}
		case 1:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN1);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND1);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN1);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL1); break;}
		case 2:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN2);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND2);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN2);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL2); break;}
		case 3:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN3);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND3);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN3);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL3); break;}
		case 4:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN4);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND4);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN4);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL4); break;}
		case 5:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN5);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND5);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN5);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL5); break;}

		case 6:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN6);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND6);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN6);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL6); break;}

		case 7:  {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN7);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND7);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN7);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL7); break;}

		default: {
					 ufs_fmp_writel(host, fmp_bsector, FMP_FMPSBEGIN7);
					 ufs_fmp_writel(host, fmp_esector, FMP_FMPSEND7);
					 ufs_fmp_writel(host, fmp_lun, FMP_FMPSLUN7);
					 ufs_fmp_writel(host, fmp_ctrl, FMP_FMPSCTRL7); break;}
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

	fmp_security = (protbytzpc         << 31) |
		(select_inline_enc  << 30) |
		(fmp_on             << 29) |
		(unmap_disable      << 28) |
		(0x3F               << 22) |
		(desc_type          << 19) |
		(0x5                << 16) |
		(nskeyreg           << 15) |
		(nssmu              << 14) |
		(nsuser             << 13) |
		(use_otp_mask       << 12) |
		(0x5                <<  9) |
		(0x5                <<  6) |
		(0x5                <<  3) |
		(0x5                <<  0);


	ufs_fmp_writel(host, fmp_security, FMP_FMPRSECURITY);
	return 0;
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

static void ufs_tcc_pre_init(struct ufs_hba *hba)
{
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);

	printk("%s:%d\n", __func__, __LINE__);
	//ufs_sbus_config_clr_bits(host, SBUS_CONFIG_SWRESETN_UFS_HCI | SBUS_CONFIG_SWRESETN_UFS_PHY, SBUS_CONFIG_SWRESETN);
	msleep(10);
	//ufs_sbus_config_set_bits(host, SBUS_CONFIG_SWRESETN_UFS_HCI | SBUS_CONFIG_SWRESETN_UFS_PHY, SBUS_CONFIG_SWRESETN);
	ufs_sbus_config_writel(host , 0x1, SBUS_TEMP);
	msleep(10);
	ufshcd_writel(hba, 0x1, HCI_MPHY_REFCLK_SEL);
	ufshcd_writel(hba, 0x0, HCI_CLKSTOP_CTRL);
	ufshcd_writel(hba, 0x1, HCI_GPIO_OUT);
	ufshcd_writel(hba, 0x1, HCI_UFS_ACG_DISABLE);
	//printk("AAA\n");
	//ufs_sec_writel(host, 0x2, 0xc);
	//printk("BBB\n");
}

static void ufs_tcc_post_init(struct ufs_hba *hba)
{
	struct ufs_tcc_host *host = ufshcd_get_variant(hba);
	unsigned int data = 0;

	printk("%s:%d\n", __func__, __LINE__);
	data = ufshcd_readl(hba,REG_CONTROLLER_ENABLE);
	while (data != 0x1)
	{
		data = ufshcd_readl(hba,REG_CONTROLLER_ENABLE);
	}

	data = ufshcd_readl(hba,REG_CONTROLLER_STATUS);
	while (reg_rdb(data,3) != 1)
	{
		data = ufshcd_readl(hba,REG_CONTROLLER_STATUS);
	}

	ufshcd_writel(hba, 0x7FFF,REG_INTERRUPT_ENABLE);


	ufs_unipro_writel(host, UNIPRO_PCLK_PERIOD_NS, PA_DBG_CLK_PERIOD);
	ufs_unipro_writel(host, CALCULATED_VALUE, PA_DBG_AUTOMODE_THLD);
	ufs_unipro_writel(host, 0x2E820183, PA_DBG_OPTION_SUITE);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_Local_TX_LCC_Enable, 0x0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(N_DeviceID, 0x0), N_DEVICE_ID_VAL);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(N_DeviceID_valid, 0x0), N_DEVICE_ID_VALID_VAL);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_ConnectionState, 0x0), T_CONNECTION_STATE_OFF_VAL);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_PeerDeviceID, 0x0), T_PEER_DEVICE_ID_VAL);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(T_ConnectionState, 0x0), T_CONNECTION_STATE_ON_VAL);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0200, 0x0), 0x3f);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x8f, TX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x8f, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0f, RX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0f, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x21, RX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x21, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x22, RX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x22, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x5c, RX_LANE_0), 0x38);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x5c, RX_LANE_1), 0x38);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x62, RX_LANE_0), 0x97);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x62, RX_LANE_1), 0x97);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x63, RX_LANE_0), 0x70);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x63, RX_LANE_1), 0x70);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x65, RX_LANE_0), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x65, RX_LANE_1), 0x1);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x69, RX_LANE_0), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x69, RX_LANE_1), 0x1);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x0);

	ufshcd_writel(hba, 0xA, HCI_DATA_REORDER);
	//ufs_fmp_clr_bits(host, 0x80000000, FMP_FMPRSECURITY);
	//ufs_fmp_writel(host, 0xDFC2E492, FMP_FMPRSECURITY);
	//ufs_fmp_readl(host, FMP_FMPRSECURITY);

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

#if 0
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
	ufs_sbus_config_clr_bits(host, SBUS_CONFIG_SWRESETN_UFS_HCI | SBUS_CONFIG_SWRESETN_UFS_PHY, SBUS_CONFIG_SWRESETN);
	msleep(10);
	ufs_sbus_config_set_bits(host, SBUS_CONFIG_SWRESETN_UFS_HCI | SBUS_CONFIG_SWRESETN_UFS_PHY, SBUS_CONFIG_SWRESETN);

	ufshcd_writel(hba, 0x1, HCI_MPHY_REFCLK_SEL);
	//---------------------------------------------------------------------------------------------------------------------------------------
	// Release refclk for device
	//---------------------------------------------------------------------------------------------------------------------------------------
	//HCI_CLKSTOP_CTRL = 0x0; // Disable
	//ufs_hci_writel(host, 0x0, HCI_CLKSTOP_CTRL);
	ufshcd_writel(hba, 0x0, HCI_CLKSTOP_CTRL);
	//---------------------------------------------------------------------------------------------------------------------------------------
	// Release Device reset
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Release device reset");
	//HCI_GPIO_OUT = 0x1;
	//ufs_hci_writel(host, 0x1, HCI_GPIO_OUT);
	ufshcd_writel(hba, 0x1, HCI_GPIO_OUT);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// AUTO CLOCK GATING DISABLE
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Disable HWACG Feature of UFS");
	//HCI_UFS_ACG_DISABLE = 0x1;
	//ufs_hci_writel(host, 0x1, HCI_UFS_ACG_DISABLE);
	ufshcd_writel(hba, 0x1, HCI_UFS_ACG_DISABLE);
	//---------------------------------------------------------------------------------------------------------------------------------------
	// ENABLE HCI
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::ENABLE HCI");
	//HCI_HCE = 0x1;
	//ufs_hci_writel(host, 0x1, HCI_HCE);
	ufshcd_writel(hba, 0x1, HCI_HCE);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Read HCI VERSION
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::HCI VERSION DETAILS");
	//rd_data = HCI_UFS_LINK_VERSION;
	//rd_data = ufs_hci_readl(host, HCI_UFS_LINK_VERSION);
	rd_data = ufshcd_readl(hba, HCI_UFS_LINK_VERSION);
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::UFS_LINK MAJOR VERSION = 0x%x",rd_data);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Check if HCI is Enabled
	//---------------------------------------------------------------------------------------------------------------------------------------
	//rd_data = HCI_HCE; //Host Controller Enable
	//rd_data = ufs_hci_readl(host, HCI_HCE);
	rd_data = ufshcd_readl(hba, HCI_HCE);
	while(rd_data != 0x1){
		//rd_data = HCI_HCE;
		//rd_data = ufs_hci_readl(host, HCI_HCE);
		rd_data = ufshcd_readl(hba, HCI_HCE);
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
	//rd_data = ufs_hci_readl(host, HCI_HCS);
	rd_data = ufshcd_readl(hba, HCI_HCS);
	while(reg_rdb(rd_data,3) != 1){ // Check UIC COMMAND Ready (UCRDY)
		//rd_data = HCI_HCS;
		//rd_data = ufs_hci_readl(host, HCI_HCS);
		rd_data = ufshcd_readl(hba, HCI_HCS);
		msleep(500);
	}
	if (res < 0)
		goto done;

	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::HCI_HCS Status = 0x%x",rd_data);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// ENABLE CPORT LOG FEATURE
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(cport_log_en) {
		//HCI_CPORT_LOG_CTRL = 1;
		//ufs_hci_writel(host, 0x1, HCI_CPORT_LOG_CTRL);
		ufshcd_writel(hba, 0x1, HCI_CPORT_LOG_CTRL);
		//HCI_CPORT_LOG_CFG  = 0;
		//ufs_hci_writel(host, 0x0, HCI_CPORT_LOG_CTRL);
		ufshcd_writel(hba, 0x0, HCI_CPORT_LOG_CFG);
	}

	//HCI_IE = 0x7FFF;
	//ufs_hci_writel(host, 0x7FFF, HCI_IE);
	ufshcd_writel(hba, 0x7FFF, HCI_IE);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some MPHY registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some MPHY registers");
	///dme_set_req(DBG_OV_TM, 0x40, 0x0, 0x0); //the DME_SET command to set UIC attributes
/*
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
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_LINERESET_PVALUE_LSB, TX_LANE_0), 0x50);
	//dme_set_req(TX_LINERESET_PVALUE_LSB, 0x50, TX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_LINERESET_PVALUE_LSB, TX_LANE_1), 0x50);
	//dme_set_req(TX_LINERESET_PVALUE_MSB, 0x0,  TX_LANE_0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_LINERESET_PVALUE_MSB, TX_LANE_0), 0x0);
	//dme_set_req(TX_LINERESET_PVALUE_MSB, 0x0,  TX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_LINERESET_PVALUE_MSB, TX_LANE_1), 0x0);
	//dme_set_req(RX_LINERESET_VALUE_LSB,  0x50, RX_LANE_0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LINERESET_VALUE_LSB, RX_LANE_0), 0x50);
	//dme_set_req(RX_LINERESET_VALUE_LSB,  0x50, RX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LINERESET_VALUE_LSB, RX_LANE_1), 0x50);
	//dme_set_req(RX_LINERESET_VALUE_MSB,  0x0,  RX_LANE_0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LINERESET_VALUE_MSB, RX_LANE_0), 0x0);
	//dme_set_req(RX_LINERESET_VALUE_MSB,  0x0,  RX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LINERESET_VALUE_MSB, RX_LANE_1), 0x0);
	//dme_set_req(RX_OVERSAMPLING_ENABLE,  0x4,  RX_LANE_0, 0x0); //UFS post-sim error
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_OVERSAMPLING_ENABLE, RX_LANE_0), 0x4);
	//dme_set_req(RX_OVERSAMPLING_ENABLE,  0x4,  RX_LANE_1, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_OVERSAMPLING_ENABLE, RX_LANE_1), 0x4);

	//dme_set_req(DBG_OV_TM, 0x0, 0x0, 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(DBG_OV_TM, 0x0), 0x0);
*/
	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some PA MIB registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some PA MIB registers");

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of some DL MIB registers
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Initial Set of some DL MIB registers");

	//dme_set_req(DL_FC0ProtectionTimeOutVal, DL_FC0_PROTECTION_TIMEOUT_VAL_VALUE, 0x0, 0x0);
	//ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(DL_FC0ProtectionTimeOutVal, 0x0), DL_FC0_PROTECTION_TIMEOUT_VAL_VALUE);
	//dme_set_req(DL_FC1ProtectionTimeOutVal, DL_FC1_PROTECTION_TIMEOUT_VAL_VALUE, 0x0, 0x0);
	//ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(DL_FC1ProtectionTimeOutVal, 0x0), DL_FC1_PROTECTION_TIMEOUT_VAL_VALUE);
	//---------------------------------------------------------------------------------------------------------------------------------------
	// Initial Set of
	//---------------------------------------------------------------------------------------------------------------------------------------
	// Configuration regarding real time specification
	//dme_set_req(PA_DBG_CLK_PERIOD, UNIPRO_PCLK_PERIOD_NS, 0x0, 0x0);
	//ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_DBG_CLK_PERIOD, 0x0), UNIPRO_PCLK_PERIOD_NS);
	ufs_unipro_writel(host, UNIPRO_PCLK_PERIOD_NS, PA_DBG_CLK_PERIOD);
	//dme_set_req(PA_DBG_AUTOMODE_THLD, CALCULATED_VALUE, 0x0, 0x0);
	//ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_DBG_AUTOMODE_THLD, 0x0), CALCULATED_VALUE);
	ufs_unipro_writel(host, CALCULATED_VALUE, PA_DBG_AUTOMODE_THLD);
	//dme_set_req(PA_DBG_OPTION_SUITE, 0x2E820183, 0x0, 0x0);
	//ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(PA_DBG_OPTION_SUITE, 0x0), 0x2E820183);
	ufs_unipro_writel(host, 0x2E820183, PA_DBG_OPTION_SUITE);

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

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0200, 0x0), 0x3f);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x8f, TX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x8f, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0f, RX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x0f, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x21, RX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x21, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x22, RX_LANE_0), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x22, RX_LANE_1), 0x0);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x5c, RX_LANE_0), 0x38);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x5c, RX_LANE_1), 0x38);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x62, RX_LANE_0), 0x97);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x62, RX_LANE_1), 0x97);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x63, RX_LANE_0), 0x70);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x63, RX_LANE_1), 0x70);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x65, RX_LANE_0), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x65, RX_LANE_1), 0x1);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x69, RX_LANE_0), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x69, RX_LANE_1), 0x1);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x0);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Set the data byte reordering.CPORT_BYTE_REORDERING must be 1 when connected to Samsung or Toshiba UFS Device
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Set the data byte reordering.CPORT_BYTE_REORDERING must be 1 when connected to Samsung or Toshiba UFS Device");

	// HCI_DATA_REORDER = 0xA;
	//ufs_hci_writel(host, 0x0, HCI_DATA_REORDER);
	ufshcd_writel(hba, 0xA, HCI_DATA_REORDER);

	//---------------------------------------------------------------------------------------------------------------------------------------
	// Disable  FmpSecurity.21st bit has been initialized zero for LHOTSE project.
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Disable  FmpSecurity");
	//FMP_FMPRSECURITY = 0xDFC2E492;
	//ufs_hci_writel(host, 0xDFC2E492, FMP_FMPRSECURITY);
	ufshcd_writel(hba, 0xDFC2E492, FMP_FMPRSECURITY);


/**********************
	//---------------------------------------------------------------------------------------------------------------------------------------
	//  Now Initialize interrupt enable register to report link start up completion and then send link start up cmd
	//---------------------------------------------------------------------------------------------------------------------------------------
	if(debug) printk(KERN_DEBUG "BASIC_LINK_UP::Now Initialize interrupt enable register to report link start up completion and then send link start up cmd");

	//HCI_UICCMDR = DME_LINKSTARTUP_REQ;
	//ufs_hci_writel(host, DME_LINKSTARTUP_REQ, HCI_UICCMDR);
	ufshcd_writel(hba, DME_LINKSTARTUP_REQ, HCI_UICCMDR);
printk("%s:%d\n", __func__, __LINE__);

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
	//rd_data = ufs_hci_readl(host, HCI_HCS);
	rd_data = ufshcd_readl(hba, HCI_HCS);

	while(reg_rdb(rd_data,0) != 1) {
		//rd_data = HCI_HCS;
		//rd_data = ufs_hci_readl(host, HCI_HCS);
		rd_data = ufshcd_readl(hba, HCI_HCS);
		msleep(200);
	}
printk("%s:%d\n", __func__, __LINE__);

	if (res < 0)
		goto done;

	if(debug) {
		printk(KERN_DEBUG "BASIC_LINK_UP::Done waiting for Hcs.dp bits to go high");
		printk(KERN_DEBUG "BASIC_LINK_UP::Device has been Found during LINKSTARTUP");
		printk(KERN_DEBUG "BASIC_LINK_UP::linkup_done event emitted..");
	}
***************************/






	//---------------------------------------------------------------------------------------------------------------------------------------
	// Link Start Up Complete. Now initializing UTP transfer management request list base address.
	//---------------------------------------------------------------------------------------------------------------------------------------

#ifdef USE_POLL
	//clearInterrupt (ignore in LinkUp State)
	//rd_data = HCI_IS;
	//rd_data = ufs_hci_readl(host, HCI_IS);
	rd_data = ufshcd_readl(hba, HCI_IS);
	if(reg_rdb(rd_data,2) == 1)  //Polling status change
	{
		//HCI_IS = 1<<2;           // clear UIC Error(ignore in LinkUp State
		//ufs_hci_writel(host, 1u<<2, HCI_IS);
		ufshcd_writel(hba, 1u<<2, HCI_IS);
	}
	if(reg_rdb(rd_data,10) == 1) //Polling status change
	{
		//HCI_IS = 1<<10;          // Clear UIC Command Completion Status
		//ufs_hci_writel(host, 1u<<10, HCI_IS);
		ufshcd_writel(hba, 1u<<10, HCI_IS);
	}

	uic_linkup_done = 1;
#endif
done :

	return res;
}
#endif

static int ufs_tcc_link_startup_post_change(struct ufs_hba *hba)
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

	printk("%s:%d\n", __func__, __LINE__);
#if 0
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x40);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x35, RX_LANE_0), 0x05);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x35, RX_LANE_1), 0x05);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x41, RX_LANE_0), 0x02);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x41, RX_LANE_1), 0x02);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x42, RX_LANE_0), 0xAC);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x42, RX_LANE_1), 0xAC);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x73, RX_LANE_0), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x73, RX_LANE_1), 0x01);

	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x200, 0x0), 0x0);
#endif

	ufshcd_writel(hba, UTRIACR_VAL, HCI_UTRIACR);
	ufshcd_writel(hba, UTMRLBA_LOW_VAL, HCI_UTMRLBA);
	ufshcd_writel(hba, UTMRLBA_HIGH_VAL, HCI_UTMRLBAU);

	ufshcd_writel(hba, UTRLBA_LOW_VAL, HCI_UTRLBA);
	ufshcd_writel(hba, UTRLBA_HIGH_VAL, HCI_UTRLBAU);

	ufshcd_writel(hba, /*TXPRDT_ENTRY_SIZE_VAL*/ 0xC, HCI_TXPRDT_ENTRY_SIZE);
	ufshcd_writel(hba, /*RXPRDT_ENTRY_SIZE_VAL*/ 0xC, HCI_RXPRDT_ENTRY_SIZE);

	ufshcd_writel(hba, 0x1, HCI_UTMRLRSR);
	ufshcd_writel(hba, 0x1, HCI_UTRLRSR);

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
		ufshcd_writel(hba, axidma_rwdataburstlen, HCI_AXIDMA_RWDATA_BURST_LEN);
	}

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
	hba->quirks |= UFSHCD_QUIRK_PRDT_BYTE_GRAN;
			//UFSHCI_QUIRK_SKIP_INTR_AGGR |
			//UFSHCD_QUIRK_UNRESET_INTR_AGGR |
			//UFSHCD_QUIRK_BROKEN_REQ_LIST_CLR |
			//UFSHCD_QUIRK_GET_UPMCRS_DIRECT |
			//UFSHCD_QUIRK_GET_GENERRCODE_DIRECT;

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
	.hce_enable_notify = ufs_tcc_hce_enable_notify,
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
