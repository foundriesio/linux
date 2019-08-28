/*
 * IR driver for remote controller : tca_remocon.c
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#if !defined(CONFIG_ARM64_TCC_BUILD)  
#include <asm/mach-types.h>
#else
#endif

#include <asm/io.h>

#include "tcc_remocon.h"
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of_address.h>
#include <linux/arm-smccc.h>

#if defined (CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	#define SIP_REMOCON_CFG	0x82004000
#endif

extern void __iomem *remote_base;
extern void __iomem *remote_cfg_base;

static unsigned int rmcfg;

extern unsigned long rmt_clk_rate;

void tca_remocon_reg_dump(void)
{
	// for checking all of remote ctrl register.

	printk("==============================================\n");
	printk("CMD	   0x%08x\n", readl(remote_base + REMOTE_CMD_OFFSET));
	printk("INPOL   0x%08x\n", readl(remote_base + REMOTE_INPOL_OFFSET));
	printk("STA	   0x%08x\n", readl(remote_base + REMOTE_STA_OFFSET));
	printk("FSR    0x%08x\n", readl(remote_base + REMOTE_FSR_OFFSET));
	printk("BDD	   0x%08x\n", readl(remote_base + REMOTE_BDD_OFFSET));
	printk("BDR0    0x%08x\n", readl(remote_base + REMOTE_BDR0_OFFSET));
	printk("BDR1    0x%08x\n", readl(remote_base + REMOTE_BDR1_OFFSET));
	printk("SD	   0x%08x\n", readl(remote_base + REMOTE_SD_OFFSET));
	printk("DBD0    0x%08x\n", readl(remote_base + REMOTE_DBD0_OFFSET));
	printk("DBD1    0x%08x\n", readl(remote_base + REMOTE_DBD1_OFFSET));
	printk("RBD	   0x%08x\n", readl(remote_base + REMOTE_RBD_OFFSET));
	printk("PBD00   0x%08x\n", readl(remote_base + REMOTE_PBD0_L_OFFSET));
	printk("PBD01   0x%08x\n", readl(remote_base + REMOTE_PBD0_H_OFFSET));
	printk("PBD10   0x%08x\n", readl(remote_base + REMOTE_PBD1_L_OFFSET));
	printk("PBD10   0x%08x\n", readl(remote_base + REMOTE_PBD1_H_OFFSET));
#if defined(CONFIG_ARCH_TCC896X)||defined(CONFIG_ARCH_TCC897x) || defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	printk("PBD20   0x%08x\n", readl(remote_base + REMOTE_PBD2_L_OFFSET));
	printk("PBD21   0x%08x\n", readl(remote_base + REMOTE_PBD2_H_OFFSET));
	printk("PBD30   0x%08x\n", readl(remote_base + REMOTE_PBD3_L_OFFSET));
	printk("PBD31   0x%08x\n", readl(remote_base + REMOTE_PBD3_H_OFFSET));
	printk("RKD0   0x%08x\n", readl(remote_base + REMOTE_RKD0_OFFSET));
	printk("RKD1   0x%08x\n", readl(remote_base + REMOTE_RKD1_OFFSET));
	printk("PBM0   0x%08x\n", readl(remote_base + REMOTE_PBM0_OFFSET));
	printk("PBM1   0x%08x\n", readl(remote_base + REMOTE_PBM1_OFFSET));
#endif
	printk("CLKDIV  0x%08x\n", readl(remote_base + REMOTE_CLKDIV_OFFSET));
	printk("==============================================\n");

}



void tcc_remocon_rmcfg(unsigned int value)
{
	// This function set RMISEL of PMU REMOCON register to set working H/W port and wakeup source.

	//RMSEL : Top reset
	//RMTE	: Disable
	//RMDIV	: Remocon Clock Divisor 2
	
	int port=0;
	struct arm_smccc_res res;

	switch(value)
	{
		case 001 : port = 1; break;
		case 010 : port = 2; break;
		case 011 : port = 3; break;
		case 100 : port = 4; break;
		case 101 : port = 5; break;
		case 110 : port = 6; break;
		default : break;
	}

	arm_smccc_smc(SIP_REMOCON_CFG, port, 0, 0, 0, 0, 0, 0, &res);
	if(res.a0 == 0)
		return;

	rmcfg = value;

	writel(readl(remote_cfg_base) | Hw6 , remote_cfg_base);
	writel(readl(remote_cfg_base) & ~Hw5, remote_cfg_base);
	writel(readl(remote_cfg_base) | Hw4 , remote_cfg_base);
	writel(readl(remote_cfg_base) & ~Hw3, remote_cfg_base);

	switch(value)
	{
		case 001 : //RMISEL is 001b
			writel(readl(remote_cfg_base)|(0x1) , remote_cfg_base);
			break;
		case 010 : //RMISEL is 010b
			writel(readl(remote_cfg_base)|(0x2) , remote_cfg_base);
			break;
		case 011 : //RMISEL is 011b
				writel(readl(remote_cfg_base)|(0x3) , remote_cfg_base);
			break;
		case 100 : //RMISEL is 100b
				writel(readl(remote_cfg_base)|(0x4) , remote_cfg_base);
			break;
		case 101 : //RMISEL is 101b
			writel(readl(remote_cfg_base)|(0x5) , remote_cfg_base);
				break;
		case 110 : //RMISEL is 110b
			writel(readl(remote_cfg_base)|(0x6) , remote_cfg_base);
				break;
		default :
			break;

	}
	return;
}

void tca_remocon_suspend(void)
{

	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw25, remote_base + REMOTE_BDD_OFFSET); //MISM
	writel(readl(remote_base + REMOTE_BDD_OFFSET) & ~Hw23, remote_base + REMOTE_BDD_OFFSET); //MDSC
}

void tca_remocon_resume(void)
{
#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
#else
	tcc_remocon_rmcfg(rmcfg);
#endif
}
//======================================================
// REMOCON set PBD(power button data).
//======================================================
void tca_remocon_set_pbd(void)
{
	long long power_code = 0;

	writel((START_MAX_VALUE<<16)|(START_MIN_VALUE), remote_base + REMOTE_SD_OFFSET);
	writel((LOW_MAX_VALUE<<16)|(LOW_MIN_VALUE), remote_base + REMOTE_DBD0_OFFSET);
#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
	writel((REPEAT_MAX_VALUE<<16)|(REPEAT_MIN_VALUE), remote_base + REMOTE_DBD1_OFFSET);
#else
	writel((HIGH_MAX_VALUE<<16)|(HIGH_MIN_VALUE), remote_base + REMOTE_DBD1_OFFSET);
#endif
	writel((REPEAT_MAX_VALUE<<16)|(REPEAT_MIN_VALUE), remote_base + REMOTE_RBD_OFFSET);

	power_code = (((long long)SCAN_POWER<<16)|(long long)REMOCON_ID)<<1;

#if defined(CONFIG_VERIZON)
	writel(((power_code & 0xFFFFFFFF)>>16), remote_base + REMOTE_PBD0_L_OFFSET);
	writel(((power_code>>32) & 0xFFFFFFFF), remote_base + REMOTE_PBD0_H_OFFSET);
#else
	writel(((power_code & 0xFFFFFFFF)), remote_base + REMOTE_PBD0_L_OFFSET);
	writel(((power_code>>32) & 0xFFFFFFFF), remote_base + REMOTE_PBD0_H_OFFSET);
#endif
	writel(0xFFFFFFFF, remote_base + REMOTE_PBD1_L_OFFSET);
	writel(0xFFFFFFFF, remote_base + REMOTE_PBD1_H_OFFSET);

	writel(0xFFFFFFFF, remote_base + REMOTE_PBD2_L_OFFSET);
	writel(0xFFFFFFFF, remote_base + REMOTE_PBD2_H_OFFSET);

	writel(0xFFFFFFFF, remote_base + REMOTE_PBD3_L_OFFSET);
	writel(0xFFFFFFFF, remote_base + REMOTE_PBD3_H_OFFSET);

	writel(0x00000000, remote_base + REMOTE_PBM0_OFFSET);
	writel(0x00000000, remote_base + REMOTE_PBM1_OFFSET);

}

//======================================================
// REMOCON initialize
//======================================================
void    RemoconCommandOpen (void)
{
	writel(readl(remote_base + REMOTE_CMD_OFFSET) & ~Hw0, remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_CMD_OFFSET) | Hw1 | Hw2, remote_base + REMOTE_CMD_OFFSET);

#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_XTIN_CLOCK)
	writel(readl(remote_base + REMOTE_CMD_OFFSET)|(0x1E<<3) , remote_base + REMOTE_CMD_OFFSET);
#else
	writel(readl(remote_base + REMOTE_CMD_OFFSET)|(IR_FIFO_READ_COUNT<<3) , remote_base + REMOTE_CMD_OFFSET);
#endif
	writel(readl(remote_base + REMOTE_CMD_OFFSET) | Hw12, remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_CMD_OFFSET) | Hw13, remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_CMD_OFFSET) | Hw14, remote_base + REMOTE_CMD_OFFSET);
}

//======================================================
// configure address
//======================================================
void    RemoconConfigure (void)
{
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) | Hw0, remote_base + REMOTE_INPOL_OFFSET);

#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) | Hw14, remote_base + REMOTE_INPOL_OFFSET);
#endif
#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) & ~Hw8, remote_base + REMOTE_INPOL_OFFSET);
#else
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) | Hw8, remote_base + REMOTE_INPOL_OFFSET);
#endif
  
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) | Hw9 | Hw10 | Hw13 , remote_base + REMOTE_INPOL_OFFSET);
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) & ~Hw11 & ~Hw12 , remote_base + REMOTE_INPOL_OFFSET);
	
	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw0 | Hw1 | Hw5 , remote_base + REMOTE_BDD_OFFSET);

#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw26 , remote_base + REMOTE_BDD_OFFSET);
  #endif
#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
	writel(readl(remote_base + REMOTE_BDD_OFFSET) & ~Hw23 , remote_base + REMOTE_BDD_OFFSET);
#else
	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw23 , remote_base + REMOTE_BDD_OFFSET);
#endif
  #if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	writel(readl(remote_base + REMOTE_BDD_OFFSET) & ~Hw25 , remote_base + REMOTE_BDD_OFFSET);
  #endif
	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw24 , remote_base + REMOTE_BDD_OFFSET);

}

//======================================================
// REMOCON STATUS : irq enable and fifo overflow (active low)
//======================================================
void    RemoconStatus (void)
{
	writel(readl(remote_base + REMOTE_STA_OFFSET) & ~(Hw12-Hw0) , remote_base + REMOTE_STA_OFFSET);
	writel(readl(remote_base + REMOTE_STA_OFFSET) | Hw12 , remote_base + REMOTE_STA_OFFSET);
}

//======================================================
// REMOCON DIVIDE enable & ir clk & end count setting
// (end count use remocon clk)
//======================================================
void    RemoconDivide (int state)
{
	unsigned int	uiclock;
	unsigned int	uidiv = 0;
	unsigned long remote_clock_rate;

	remote_clock_rate = rmt_clk_rate;
	uiclock           = state == 1? 24*1000*1000:(remote_clock_rate);
	uidiv             = uiclock/32768;

	//printk("##[%s] , rmt_clk_rate : %d ,remote clock_rate : %d \n", __func__ , rmt_clk_rate, remote_clock_rate);
	//printk("+++++++++++++++++++++++++++++++++++++[%d][%d]\n", uiclock, uidiv);

	writel(readl(remote_base + REMOTE_CLKDIV_OFFSET) | (0xC8), remote_base + REMOTE_CLKDIV_OFFSET);
	writel(readl(remote_base + REMOTE_CLKDIV_OFFSET) | (uidiv<<14), remote_base + REMOTE_CLKDIV_OFFSET);
	writel(readl(remote_base + REMOTE_CLKDIV_OFFSET) | Hw14 , remote_base + REMOTE_CLKDIV_OFFSET);
}

//======================================================
// REMOCON diable 
//======================================================
void    DisableRemocon (void)
{
	writel(readl(remote_base + REMOTE_CMD_OFFSET) | Hw0 , remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_CMD_OFFSET) & ~Hw1 & ~Hw2 & ~Hw13 & ~Hw14 , remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_BDD_OFFSET) & Hw1 , remote_base + REMOTE_BDD_OFFSET);
}

//======================================================
// REMOCON functions
//======================================================
void    RemoconInit (int state)
{
	RemoconDivide(state);
	RemoconIntClear();
}

void    RemoconIntClear ()
{
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) | Hw0 , remote_base + REMOTE_INPOL_OFFSET);
#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) & ~Hw8 , remote_base + REMOTE_INPOL_OFFSET);
#else
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) | Hw8 , remote_base + REMOTE_INPOL_OFFSET);
#endif
	
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) | Hw9 | Hw10 | Hw13 , remote_base + REMOTE_INPOL_OFFSET);
	writel(readl(remote_base + REMOTE_INPOL_OFFSET) & ~Hw11 & ~Hw12 , remote_base + REMOTE_INPOL_OFFSET);

	writel(readl(remote_base + REMOTE_CMD_OFFSET) & ~Hw13 & ~Hw2 , remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_CMD_OFFSET) | Hw0 , remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_BDD_OFFSET) & ~Hw1 , remote_base + REMOTE_BDD_OFFSET);

#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
#else
	writel(readl(remote_base + REMOTE_STA_OFFSET) & ~(Hw12-Hw0) , remote_base + REMOTE_STA_OFFSET);
#endif
	writel(readl(remote_base + REMOTE_CMD_OFFSET) | Hw2 | Hw13 , remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_CMD_OFFSET) & ~Hw0 , remote_base + REMOTE_CMD_OFFSET);
	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw1 , remote_base + REMOTE_BDD_OFFSET);
#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_XTIN_CLOCK)
	writel(readl(remote_base + REMOTE_STA_OFFSET) | (0xFFF), remote_base + REMOTE_STA_OFFSET);
#else
	writel(readl(remote_base + REMOTE_STA_OFFSET) | (0x1FE), remote_base + REMOTE_STA_OFFSET);
#endif

	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw0 | Hw5, remote_base + REMOTE_BDD_OFFSET);
  
#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITH_XTIN) || defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
	writel(readl(remote_base + REMOTE_BDD_OFFSET) & ~Hw23, remote_base + REMOTE_BDD_OFFSET);
#else
	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw23, remote_base + REMOTE_BDD_OFFSET);
#endif
	writel(readl(remote_base + REMOTE_BDD_OFFSET) | Hw24, remote_base + REMOTE_BDD_OFFSET);
}
