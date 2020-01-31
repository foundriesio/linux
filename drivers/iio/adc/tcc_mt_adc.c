/****************************************************************************
 * tcc_adc.c
 * Copyright (C) 2014 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>

#ifdef CONFIG_ARCH_TCC897X
#include <mach/tcc_adc.h>
#else
#include "tcc_mt_adc.h"
#endif

#define adc_dbg(_adc, msg...) dev_dbg((_adc)->dev, msg)

#define adc_readl		__raw_readl
#define adc_writel		__raw_writel
//#define ADC_TEST

struct adc_device {
	struct device		*dev;
	struct clk		*fclk;
	struct clk		*hclk;
	struct clk		*phyclk;
	struct tcc_mt_adc_client	*cur;
	void __iomem		*regs;
	void __iomem		*pmu_regs;
	u32			is_12bit_res;
	u32			delay;
	u32			ckin;
	unsigned int *dma_addr;
	void *v_ddr;
};

extern struct device_attribute dev_attr_tccadc;

static struct mutex		lock;
static struct adc_device	*adc_dev;
static spinlock_t		adc_spin_lock;

static ADC_PGDMACTRL         pADC_DMA;
static unsigned int n=0;
static void mt_adc_pmu_power_ctrl(struct adc_device *adc, int adc_num, int pwr_on)
{
	unsigned reg_values;
	BUG_ON(!adc);
	reg_values = adc_readl(adc->pmu_regs);
	if (adc_num == 0) {
		reg_values &= ~(1<<PMU_TSADC0_STOP_SHIFT);

		if (pwr_on) {
			clk_prepare_enable(adc->phyclk);
			reg_values |= (0<<PMU_TSADC0_STOP_SHIFT);
			adc_writel(reg_values, adc->pmu_regs);
			mdelay(1);
		}
		else {
			reg_values |= (1<<PMU_TSADC0_STOP_SHIFT);
			adc_writel(reg_values, adc->pmu_regs);
			mdelay(1);
			clk_prepare_enable(adc->phyclk);
		}
	}
	else {
		reg_values &= ~(1<<PMU_TSADC0_STOP_SHIFT);

		if (pwr_on) {
			clk_prepare_enable(adc->phyclk);
			reg_values |= (0<<PMU_TSADC1_STOP_SHIFT);
			mdelay(1);
			adc_writel(reg_values, adc->pmu_regs);
		}
		else {
			reg_values |= (1<<PMU_TSADC1_STOP_SHIFT);
			adc_writel(reg_values, adc->pmu_regs);
			mdelay(1);
			clk_prepare_enable(adc->phyclk);
		}
	}
}

static void mt_adc_power_ctrl(struct adc_device *adc, int adc_num, int pwr_on)
{
	unsigned reg_values;

	if (pwr_on) {
		unsigned long clk_rate;
		int ps_val;
		mt_adc_pmu_power_ctrl(adc, 0, 1); //enable adc core0
		mt_adc_pmu_power_ctrl(adc, 1, 1); //enable adc core1

		clk_prepare_enable(adc->fclk); //enable adc clock
		clk_prepare_enable(adc->hclk); //enable adc clock(bus)

		clk_rate = clk_get_rate(adc->fclk);
		ps_val = (clk_rate+adc->ckin/2)/adc->ckin - 1;

		/* ADC Configuration*/
		if(adc_num == 0)
		{
			reg_values = adc_readl(adc->regs+ADC0CFG_REG);
			reg_values = reg_values & ADCCFG_CLKDIV(1) ;
			adc_writel(reg_values, adc->regs+ADC0CFG_REG);

			reg_values = adc_readl(adc->regs+ADC0CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);

			reg_values = adc_readl(adc->regs+ADC0MODE_REG);
			reg_values |= 1<<0x6;
			adc_writel(reg_values, adc->regs+ADC0MODE_REG); //Set ADC0(Not TSADC)

		}
		else if(adc_num == 1)
		{
			reg_values = adc_readl(adc->regs+ADC1CFG_REG);
			reg_values = reg_values & ADCCFG_CLKDIV(1) ;
			adc_writel(reg_values, adc->regs+ADC1CFG_REG);

			reg_values = adc_readl(adc->regs+ADC1CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
		}
	}
	else {
		if(adc_num == 0)
		{
		/* set stand-by mode */
			reg_values = adc_readl(adc->regs+ADC0CON_REG) | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);
		}
		else if(adc_num == 1)
		{
		/* set stand-by mode */
			reg_values = adc_readl(adc->regs+ADC1CON_REG) | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
		}

		if(adc->fclk)
			clk_disable_unprepare(adc->fclk);
		if(adc->hclk)
			clk_disable_unprepare(adc->hclk);

		mt_adc_pmu_power_ctrl(adc, 0, 0); //disable adc core0
		mt_adc_pmu_power_ctrl(adc, 1, 0); //disable adc core1
	}
}

static inline unsigned long mt_adc_read(struct adc_device *adc, int adc_num, int ch)
{
	unsigned int data=0;
	unsigned reg_values;
	unsigned int flag;
	unsigned int mask;

	if (adc_num == 0) {
		#ifdef CONFIG_TCC_MT_ADC0
		if(ch >= 2 && ch <= 9) {
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);

			/* Change Configuration*/
			mask = adc_readl(adc->regs+ADC0DMASK_REG);
			mask &= ~(0xf<<((ch-2)*4));
			adc_writel(mask, adc->regs+ADC0DMASK_REG);

			reg_values = adc_readl(adc->regs+ADC0CFG_REG);
			reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFOTH | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
			adc_writel(reg_values, adc->regs+ADC0CFG_REG);

			reg_values = adc_readl(adc->regs+ADC0CFG2_REG);
			reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK | ADCCFG2_STPCK_EN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
			adc_writel(reg_values, adc->regs+ADC0CFG2_REG);

			/*Set channel and enable*/
			reg_values = ADCCON_ASEL(ch-2) | ADCCON_ENABLE; //ASEL[2:0], SEL value is channel+2 in CH0_ADC
			adc_writel(reg_values, adc->regs+ADC0CON_REG);

			do{
				data = adc_readl(adc->regs+ADC0DATA_REG);
				flag = data & 0x1;
			}while(flag==0); //wait for valid data

			data = (data >> 1)&0xFFF;

			//reg_values = adc_readl(adc->regs+ADC0CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);
		}
		else
		{
			printk(KERN_ERR "[ERROR][ADC] %s : invalid channel number(ADC%d - %d)!\n", __func__, adc_num, ch);
		}
		#else
		printk(KERN_ERR "[ERROR][ADC] %s : ADC core0 is disabled!!\n", __func__);
		#endif
	}
	else if (adc_num == 1)
	{
		#ifdef CONFIG_TCC_MT_ADC1
		if(ch >= 0 && ch < 10) {
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
			/* Change Configuration*/
			if(ch < 8) {
				mask = adc_readl(adc->regs+ADC1DMASK_REG);
				mask &= ~(0xf<<(ch*4));
				adc_writel(mask, adc->regs+ADC1DMASK_REG);
				//printk("%s : ADC1 mask = 0x%x\n", __func__, mask);
			}
			else {
				mask = adc_readl(adc->regs+ADC1DMASK2_REG);
				mask &= ~(0xf<<((ch-8)*4));
				adc_writel(mask, adc->regs+ADC1DMASK2_REG);
				//printk("%s : ADC1 mask2 = 0x%x\n", __func__, mask);
			}

			reg_values = adc_readl(adc->regs+ADC1CFG_REG);
			reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFOTH | ADCCFG_DLYSTC | ADCCFG_CLKDIV(0) | ADCCFG_DMAEN;
			adc_writel(reg_values, adc->regs+ADC1CFG_REG);

			reg_values = adc_readl(adc->regs+ADC1CFG2_REG);
			reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
			adc_writel(reg_values, adc->regs+ADC1CFG2_REG);

			/*Set channel and enable*/
			reg_values = ADCCON_ASEL(ch) | ADCCON_ENABLE;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);

			do{
				data = adc_readl(adc->regs+ADC1DATA_REG);
				flag = data & 0x1;
			}while(flag==0); //wait for valid data

			data = (data >> 1)&0xFFF;

			//reg_values = adc_readl(adc->regs+ADC1CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
		}
		else
		{
			printk(KERN_ERR "[ERROR][ADC] %s : invalid channel number(ADC%d - %d)!\n", __func__, adc_num, ch);
		}
		#else
		printk(KERN_ERR "[ERROR][ADC] %s : ADC core1 is disabled!!\n", __func__);
		#endif
	}

	//printk("\x1b[1;33m[%s:ADC%d(%d) value = %d]\x1b[0m\n", __func__, adc_num, ch, data);

	return data;
}

unsigned long mt_adc_auto_scan(struct adc_device *adc, int adc_num, unsigned int* pbuff, unsigned int mask, unsigned int mask2)//mask2 is only for ADC core1
{
    unsigned int i;
    unsigned int read_data;
    unsigned int flag = 0;
	unsigned int mask_flag = 0;
	unsigned int timeout = 500;
	unsigned long data = 0;
	unsigned reg_values;

	if (adc_num == 0) {
		#ifdef CONFIG_TCC_MT_ADC0
		/////////////////////////////////
		////TO BEGIN AUTO-SCAN MODE!!////
		/////////////////////////////////

		reg_values = 0x00000000;
		adc_writel(reg_values, adc->regs+ADC0DMASK_REG);

		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFOTH | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
		adc_writel(reg_values, adc->regs+ADC0CFG_REG);

		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
		adc_writel(reg_values, adc->regs+ADC0CFG2_REG);

		reg_values = adc_readl(adc->regs+ADC0CON_REG);
		reg_values |= ADCCON_STBY;
		adc_writel(reg_values, adc->regs+ADC0CON_REG);

		reg_values = ADCCON_ASEL(15) | ADCCON_ENABLE;
		adc_writel(reg_values, adc->regs+ADC0CON_REG);

		do{
			data = adc_readl(adc->regs+ADC0DATA_REG);
			flag = data & 0x1;
		}while(flag==0); //wait for valid data

		data = (data >> 1)&0xFFF;
		udelay(10);

		//////////////////////////////
		////AUTO-SCAN MODE START!!////
		//////////////////////////////

		if (mask != 0x0)
		{
			adc_writel((0xFFFFFFFF&mask), adc->regs+ADC0DMASK_REG);
		}

		for(i=0;i<MAX_CH0_ADC_CHANNEL;i++)
        {
          adc_writel(0x00880000, adc->regs+ADC0BUF_SEL(i)); //disalbe round-off & force write to read data buffer(not scan mode read buffer)
        }
		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFON(3) | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
		adc_writel(reg_values, adc->regs+ADC0CFG_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC0EVTSEL_REG);

		reg_values = 0x03030303;
		adc_writel(reg_values, adc->regs+ADC0EVTCNTV_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC0EIRQSTAT_REG); //to clear IRQ STATE

		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN_EN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
		adc_writel(reg_values, adc->regs+ADC0CFG2_REG);

		if ((adc_readl(adc->regs+ADC0CFG_REG)) & ADCCFG_SM)
		{
			for(i = 0; i < MAX_CH0_ADC_CHANNEL; i++)
			{
				//check channel mask
				if(mask & (0xf<<(i*4)))
					mask_flag &= ~(1<<i);
				else
					mask_flag |= (1<<i);
			}
			//printk("%s : mask=0x%x, mask_flag=0x%x\n", __func__, mask, mask_flag);
			do{
				data = adc_readl(adc->regs+ADC0EIRQSTAT_REG);
				flag = (data & 0x00FF0000) >> 16; //check raw status for ADC0 ch 2~9
				timeout--;
				if (!timeout)
				{
					printk(KERN_ERR "[ERROR][ADC]%s : timeout! - data=0x%x, flag=0x%x\n", __func__, data, flag);
				}
				//printk("%s : data=0x%x, flag=0x%x\n", __func__, data, flag);
			}while(flag!=mask_flag && !timeout); //wait for valid data
		}

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC0EIRQSTAT_REG); //to clear IRQ STATE
		udelay(20);

	    for(i=0;i<MAX_CH0_ADC_CHANNEL;i++)
	    {
			if (!((0xf<<(i*4)) & mask)) {
	        	read_data = *((volatile unsigned int *)(adc->regs+ADC0STATUS_REG));
				memcpy((pbuff), &read_data, sizeof(unsigned int));
	        	//printk("(%d) CONV_SUCCESS : %d\n ",((((*pbuff) & 0x00070000))>>(0x10))+2, (*pbuff) & 0x00000fff);
				pbuff++;
	        	udelay(10);
			}
			else {
				//printk("%s : skip ADC0 %d channel\n", __func__, i+2);
			}
	    }
		#else
		printk(KERN_ERR "[ERROR][ADC] %s : ADC core0 is disabled!!\n", __func__);
		#endif
	}
	else if (adc_num == 1) {
		#ifdef CONFIG_TCC_MT_ADC1
		/////////////////////////////////
		////TO BEGIN AUTO-SCAN MODE!!////
		/////////////////////////////////
		reg_values = 0x00000000;
		adc_writel(reg_values, adc->regs+ADC1DMASK_REG);
		adc_writel(reg_values, adc->regs+ADC1DMASK2_REG);

		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFOTH | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
		adc_writel(reg_values, adc->regs+ADC1CFG_REG);

//		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
//		adc_writel(reg_values, adc->regs+ADC1CFG2_REG);

		reg_values = adc_readl(adc->regs+ADC1CON_REG);
		reg_values |= ADCCON_STBY;
		adc_writel(reg_values, adc->regs+ADC1CON_REG);

		reg_values |= ADCCON_ASEL(15);
		adc_writel(reg_values, adc->regs+ADC1CON_REG);

		reg_values &= ~ADCCON_STBY;
		adc_writel(reg_values, adc->regs+ADC1CON_REG);

		do{
			data = adc_readl(adc->regs+ADC1DATA_REG);
			flag = data & 0x1;
		}while(flag==0); //wait for valid data

		data = (data >> 1)&0xFFF;

		udelay(10);

		//////////////////////////////
		////AUTO-SCAN MODE START!!////
		//////////////////////////////
		if (mask != 0x0)
		{
			adc_writel((0xFFFFFFFF&mask), adc->regs+ADC1DMASK_REG);
		}
		if(mask2 != 0x0)
		{
			adc_writel((0xFFFFFFFF&mask2), adc->regs+ADC1DMASK2_REG);
		}

		for(i=0;i<MAX_CH1_ADC_CHANNEL;i++)
        {
          adc_writel(0x00880000, adc->regs+ADC1BUF_SEL(i)); //disalbe round-off & force write to read data buffer(not scan mode read buffer)
        }
		/*
		reg_values = 0xFFFFFFFF & (ADCBUF_FWRB | ADCBUF_NRND); //ADC Buff set (disalbe round-off & force write to read data buffer(not scan mode read buffer))
		for (i=0; i<MAX_CH1_ADC_CHANNEL; i++)
		{
			adc_writel(reg_values, adc->regs+ADC0BUF_SEL(i));
		}
		*/
		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFON(7) | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
		adc_writel(reg_values, adc->regs+ADC1CFG_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC1EVTSEL_REG);  //ADC1 ch0~7
		adc_writel(reg_values, adc->regs+ADC1EVTSEL2_REG); //ADC1 ch8~9

		reg_values = 0x03030303;
		adc_writel(reg_values, adc->regs+ADC1EVTCNTV_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC1EIRQSTAT_REG); //to clear IRQ STATE

		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN_EN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
		adc_writel(reg_values, adc->regs+ADC1CFG2_REG);

		if ((adc_readl(adc->regs+ADC1CFG_REG)) & ADCCFG_SM)
		{
			for(i=0; i<MAX_CH1_ADC_CHANNEL; i++)
			{
				//check channel mask
				if (i<8) {
					if(mask & (0xf<<(i*4)))
						mask_flag &= ~(1<<i);
					else
						mask_flag |= (1<<i);
				}
				else {
					if(mask2 & (0xf<<((i-8)*4)))
						mask_flag &= ~(1<<i);
					else
						mask_flag |= (1<<i);
				}
			}
			//printk("%s : mask=0x%x, ,mask2=0x%x, mask_flag=0x%x\n", __func__, mask, mask2, mask_flag);
			do{
				data = adc_readl(adc->regs+ADC1EIRQSTAT_REG);
				flag = (data & 0x03FF0000) >> 16; //check raw status for ADC1 ch0~9
				timeout--;
				if (!timeout)
				{
					printk(KERN_ERR "[ERROR][ADC] %s : timeout! - data=0x%x, flag=0x%x\n", __func__, data, flag);
				}
				//printk("%s : data=0x%x, flag=0x%x\n", __func__,data, flag);
			}while(flag!=mask_flag && timeout); //wait for valid data
		}

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC1EIRQSTAT_REG);
		udelay(20);

	    for(i=0;i<MAX_CH1_ADC_CHANNEL;i++)
	    {
			if (i < 8) {
				if (!((0xf<<(i*4)) & mask)) {
					read_data = *((volatile unsigned int *)(adc->regs+ADC1STATUS_REG));
					memcpy((pbuff), &read_data, sizeof(unsigned int));
					//printk("(%d) CONV_SUCCESS : %d\n ",((((*pbuff) & 0x00070000))>>(0xF)), (*pbuff) & 0x00000fff);
					pbuff++;
					udelay(10);
				}
				else {
					//printk("%s : skip ADC1 %d channel\n", __func__, i);
				}
			}
			else {
				if (!((0xf<<((i-8)*4)) & mask2)) {
					read_data = *((volatile unsigned int *)(adc->regs+ADC1STATUS_REG));
					memcpy((pbuff), &read_data, sizeof(unsigned int));
					//printk("(%d) CONV_SUCCESS : %d\n ",((((*pbuff) & 0x00070000))>>(0xF)), (*pbuff) & 0x00000fff);
					pbuff++;
					udelay(10);
				}
				else {
					//printk("%s : skip ADC1 %d channel\n", __func__, i);
				}
			}
	    }
		#else
		printk(KERN_ERR "[ERROR][ADC] %s : ADC core1 is disabled!!\n", __func__);
		#endif

	}
	return 0;
}

#ifdef USE_GDMA
void GdmaEnable (void)
{
	pADC_DMA->CHCTRL0 |= 0x1;
}

void mt_adc_dma_config(
    uint nSrcAddr,
    uint nDstAddr,
    uint ADC_CH )
{
	unsigned int uCHCTRL;
       #ifdef USE_DMA_IRQ
       register_int_handler((41+32), &IRQHandlerDMA, NULL);
       printk(KERN_DEBUG "[DEBUG][ADC] Open IOB DMA END\n");
       #endif

       pADC_DMA->ST_SADR0            = nSrcAddr;
       pADC_DMA->ST_DADR0            = nDstAddr;
       pADC_DMA->SPARAM0			 = 0;
       pADC_DMA->DPARAM0			 = 0xfffff004; //@15.07.10, need to check the data according to address (o) => In this example, the address does not increase the reason why the nDParam is '0'. But the data are changed every cycles
       pADC_DMA->HCOUNT0			 = 1;

       if      (ADC_CH ==0) pADC_DMA->EXTREQ0  = 1<<8;
       else if (ADC_CH ==1) pADC_DMA->EXTREQ0  = 1<<9;
       else                 printk(KERN_ERR "[ERROR][ADC] DMA REQ_SEL FAIL!!!");

	   uCHCTRL = (1<<0xf) | //continuous transfer, start address doesn't reset
	 			 (1<<0xb) | //lock
	 			 (1<<0xa) | //bst
	   			 (3<<0x8) |
	   			 (0<<0x6) |
	   			 (3<<0x4) | //DMA_32BIT;
			     (1<<0x3) | //check the effect of writting '1' in this register(o) => DMA Done Flag register is cleared by writting '1' in this field
	   			 (1<<0x1) | //repeat mode
		#ifdef USE_DMA_IRQ
				 (1<<0x2) | //ien
		#endif
				 0;
       pADC_DMA->CHCTRL0 = uCHCTRL;
}

void  ADC_DmaFlagWait(uint nCH)
{
    uint flag;

    do {
        flag = pADC_DMA->CHCTRL0 & (1<<3);
    } while (flag==0);
    pADC_DMA->CHCTRL0 |= flag;

    printk(KERN_DEBUG "[DEBUG][ADC] DMA Transfer finish ...!!! \n");
}

#ifdef USE_GDMA
void ADC_GdmaEnable (void)
{
        pADC_DMA->CHCTRL |= (1<<0);
}
#endif

unsigned long mt_adc_auto_scan_dma(struct adc_device *adc, int adc_num, unsigned int* pbuff, unsigned mask, unsigned mask2)
{
    unsigned int i;
    unsigned int read_data;
    unsigned int flag = 0;

	unsigned long data = 0;
	unsigned reg_values;

	if (adc_num == 0) {
		#ifdef CONFIG_TCC_MT_ADC0
		/////////////////////////////////
		////TO BEGIN AUTO-SCAN MODE!!////
		/////////////////////////////////
		ADC_GdmaEnable();
		reg_values = 0x00000000;
		adc_writel(reg_values, adc->regs+ADC0DMASK_REG);

		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE_EN | ADCCFG_FIFON(7) | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) |ADCCFG_DMAEN_EN;	
		adc_writel(reg_values, adc->regs+ADC0CFG_REG);

//		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK | ADCCFG2_ASMEN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
//		adc_writel(reg_values, adc->regs+ADC0CFG2_REG);

		reg_values = adc_readl(adc->regs+ADC0CON_REG);
		reg_values |= ADCCON_STBY;
		adc_writel(reg_values, adc->regs+ADC0CON_REG);

		reg_values = ADCCON_ASEL(15) | ADCCON_ENABLE;
		adc_writel(reg_values, adc->regs+ADC0CON_REG);

		do{
			data = adc_readl(adc->regs+ADC0DATA_REG);
			flag = data & 0x1;
		}while(flag==0); //wait for valid data

		data = (data >> 1)&0xFFF;
		udelay(10);

		//////////////////////////////
		////AUTO-SCAN MODE START!!////
		//////////////////////////////

		adc_writel(0x00000000, adc->regs+ADC0DMASK_REG);
		//GdmaEnable(0);
		for(i=0;i<MAX_CH0_ADC_CHANNEL;i++)
		{
			adc_writel(0x00880000, adc->regs+ADC0BUF_SEL(i)); //disalbe round-off & force write to read data buffer(not scan mode read buffer)
		}

		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFON(3) | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
		adc_writel(reg_values, adc->regs+ADC0CFG_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC0EVTSEL_REG);

		reg_values = 0x03030303;
		adc_writel(reg_values, adc->regs+ADC0EVTCNTV_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC0EIRQSTAT_REG); //to clear IRQ STATE

		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN_EN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
		adc_writel(reg_values, adc->regs+ADC0CFG2_REG);

		if ((adc_readl(adc->regs+ADC0CON_REG)) & ADCCFG_SM)
		{
			do{
				data = adc_readl(adc->regs+ADC0EIRQSTAT_REG);
				flag = (data & 0x00FF0000) >> 4;
			}while(flag!=0xFF); //wait for valid data
		}

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC0EIRQSTAT_REG);
		udelay(10);

	    for(i=0;i<MAX_CH0_ADC_CHANNEL;i++)
	    {
	        read_data = *((volatile unsigned int *)(adc->regs+ADC0BUF_SEL(i)));
			memcpy((pbuff+i), &read_data, sizeof(unsigned int));
	        //printk(" CONV_SUCCESS : %d\n ", read_data & 0x00000fff);
	        udelay(10);
	    }
		#else
		printk(KERN_ERR "[ERROR][ADC] %s : ADC core0 is disabled!!\n", __func__);
		#endif
	}
	else if (adc_num == 1) {
		#ifdef CONFIG_TCC_MT_ADC1
		/////////////////////////////////
		////TO BEGIN AUTO-SCAN MODE!!////
		/////////////////////////////////
		reg_values = 0x00000000;
		adc_writel(reg_values, adc->regs+ADC1DMASK_REG);
		adc_writel(reg_values, adc->regs+ADC1DMASK2_REG);

		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFOTH | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
		adc_writel(reg_values, adc->regs+ADC1CFG_REG);

//		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK | ADCCFG2_ASMEN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
//		adc_writel(reg_values, adc->regs+ADC1CFG2_REG);

		reg_values = adc_readl(adc->regs+ADC1CON_REG);
		reg_values |= ADCCON_STBY;
		adc_writel(reg_values, adc->regs+ADC1CON_REG);

		reg_values = ADCCON_ASEL(15) | ADCCON_ENABLE;
		adc_writel(reg_values, adc->regs+ADC1CON_REG);

		do{
			data = adc_readl(adc->regs+ADC1DATA_REG);
			flag = data & 0x1;
		}while(flag==0); //wait for valid data

		data = (data >> 1)&0xFFF;
		udelay(10);

		//////////////////////////////
		////AUTO-SCAN MODE START!!////
		//////////////////////////////
		for(i=0;i<MAX_CH1_ADC_CHANNEL;i++)
        {
          adc_writel(0x00880000, adc->regs+ADC1BUF_SEL(i)); //disalbe round-off & force write to read data buffer(not scan mode read buffer)
        }
		/*
		reg_values = 0xFFFFFFFF & (ADCBUF_FWRB | ADCBUF_NRND); //ADC Buff set (disalbe round-off & force write to read data buffer(not scan mode read buffer))
		for (i=0; i<MAX_CH1_ADC_CHANNEL; i++)
		{
			adc_writel(reg_values, adc->regs+ADC0BUF_SEL(i));
		}
		*/
		reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFON(7) | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
		adc_writel(reg_values, adc->regs+ADC1CFG_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC1EVTSEL_REG);  //ADC1 ch0~7
		adc_writel(reg_values, adc->regs+ADC1EVTSEL2_REG); //ADC1 ch8~9

		reg_values = 0x03030303;
		adc_writel(reg_values, adc->regs+ADC1EVTCNTV_REG);

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC1EIRQSTAT_REG); //to clear IRQ STATE

		reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN_EN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
		adc_writel(reg_values, adc->regs+ADC1CFG2_REG);

		do{
			data = adc_readl(adc->regs+ADC1EIRQSTAT_REG);
			flag = (data & 0x00FF0000) >> 4;
		}while(flag!=0xFF); //wait for valid data

		reg_values = 0xFFFFFFFF;
		adc_writel(reg_values, adc->regs+ADC1EIRQSTAT_REG);

	    for(i=0;i<MAX_CH1_ADC_CHANNEL;i++)
	    {
	        read_data = *((volatile unsigned int *)(adc->regs+ADC1BUF_SEL(i)));
			memcpy((pbuff+i), &read_data, sizeof(unsigned int));
	        //printk(" CONV_SUCCESS : %d\n ", *(pbuff+i) & 0x00000fff);
			udelay(10);
	    }
		#else
		printk(KERN_ERR "[ERROR][ADC] %s : ADC core1 is disabled!!\n", __func__);
		#endif
	}
	return 0;
}
#endif

unsigned long tcc_mt_adc_getdata(struct tcc_mt_adc_client *client)
{
	struct adc_device *adc = adc_dev;
	BUG_ON(!client);

	if (!adc) {
		printk(KERN_ERR "[ERROR][ADC] %s: failed to find adc device\n", __func__);
		return -EINVAL;
	}

	spin_lock(&adc_spin_lock);
	if (client->channel < 10)
		client->data = mt_adc_read(adc, client->adc_num, client->channel);
	else
		mt_adc_auto_scan(adc, client->adc_num, client->read_buff, client->mask, client->mask2);
	spin_unlock(&adc_spin_lock);

	return client->data;
}
EXPORT_SYMBOL_GPL(tcc_mt_adc_getdata);

struct tcc_mt_adc_client *tcc_mt_adc_register(struct device *dev, int adc_num, int ch, unsigned int mask, unsigned int mask2, unsigned int* pbuff)
{
	struct tcc_mt_adc_client *client;

	BUG_ON(!dev);
	//printk("\x1b[1;33m[%s:%d]\x1b[0m\n", __func__, __LINE__);

	client = devm_kzalloc(dev, sizeof(struct tcc_mt_adc_client), GFP_KERNEL);
	if (!client) {
		 dev_err(dev, "[ERROR][ADC] no memory for adc client\n");
		 return NULL;
	}
	//printk("%s : ch = %d, mask = 0x%x, mask2 = 0x%x\n", __func__, ch, mask, mask2);
	client->dev = dev;
	client->adc_num = adc_num;
	client->channel = ch;
	client->mask = mask;
	client->mask2 = mask2;
	client->read_buff = pbuff;
	return client;
}
EXPORT_SYMBOL_GPL(tcc_mt_adc_register);

void tcc_mt_adc_release(struct tcc_mt_adc_client *client)
{
	if (client) {
		devm_kfree(client->dev, client);
		client = NULL;
	}
}
EXPORT_SYMBOL_GPL(tcc_mt_adc_release);

spinlock_t *get_mt_adc_spinlock(void)
{
	return &adc_spin_lock;
}
EXPORT_SYMBOL_GPL(get_mt_adc_spinlock);

static int tcc_mt_adc_probe(struct platform_device *pdev)
{
	struct adc_device *adc;
	u32 clk_rate;
	int ret = -ENODEV;
	int err;
	dev_dbg(&pdev->dev, "[DEBUG][ADC] [%s]\n", __func__);
	n++;
	adc = devm_kzalloc(&pdev->dev, sizeof(struct adc_device), GFP_KERNEL);
	if (adc == NULL) {
		dev_err(&pdev->dev, "[ERROR][ADC] failed to allocate adc_device\n");
		return -ENOMEM;
	}
	adc->dev = &pdev->dev;

	/* get adc/pmu register addresses */
	adc->regs = of_iomap(pdev->dev.of_node, 0);
	if (!adc->regs) {
		dev_err(&pdev->dev, "[ERROR][ADC] failed to get adc registers\n");
		ret = -ENXIO;
		goto err_get_reg_addrs;
	}
	adc->pmu_regs = of_iomap(pdev->dev.of_node, 1);
	if (!adc->pmu_regs) {
		dev_err(&pdev->dev, "[ERROR][ADC] failed to get pmu registers\n");
		ret = -ENXIO;
		goto err_get_reg_addrs;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &clk_rate);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][ADC] Can not get clock frequency value\n");
		goto err_get_property;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "ckin-frequency", &adc->ckin);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][ADC] Can not get adc ckin value\n");
		goto err_get_property;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "adc-delay", &adc->delay);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][ADC] Can not get ADC Dealy value\n");
		goto err_get_property;
	}
	//adc->is_12bit_res = of_property_read_bool(pdev->dev.of_node, "adc-12bit-resolution");

	/* get peri/io_hclk/pmu_ipisol clocks */
	adc->fclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(adc->fclk))
		goto err_get_property;
	else
		clk_set_rate(adc->fclk, (unsigned long)clk_rate);
	adc->hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(adc->hclk))
		goto err_fclk;
	adc->phyclk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR(adc->phyclk))
		goto err_hclk;

	platform_set_drvdata(pdev, adc);
/*
	pADC_DMA   = (volatile ADC_PGDMACTRL )tcc_p2v( HwGDMA_BASE );
	adc->dma_addr = devm_kzalloc(&pdev->dev,sizeof(adc->dma_addr), GFP_KERNEL);
	adc->v_ddr = dma_alloc_writecombine(&pdev->dev, sizeof(unsigned int), adc->dma_addr, GFP_KERNEL);
	printk("mt_adc : alloc DMA buffer @0x%p(Phy=0x%p), size:%d\n",
		adc->v_ddr, adc->dma_addr, sizeof(unsigned int));
	mt_adc_dma_config((unsigned int)adc->regs+ADC0STATUS_REG, (unsigned int)adc->v_ddr, 0);
*/
	mutex_init(&lock);
	spin_lock_init(&adc_spin_lock);
	#ifdef CONFIG_TCC_MT_ADC0
	mt_adc_power_ctrl(adc, 0, 1);
	#endif
	#ifdef CONFIG_TCC_MT_ADC1
	mt_adc_power_ctrl(adc, 1, 1);
	#endif
	ret = device_create_file(&pdev->dev, &dev_attr_tccadc);
	if(ret)
		goto err_phyclk;

	dev_info(&pdev->dev, "[INFO][ADC] attached driver\n");
	adc_dev = adc;
/*
	err = of_platform_populate(pdev->dev.of_node, NULL, NULL, adc->dev);
	if (err < 0 )
	{
		printk("\x1b[1;31m[%s:%d]\x1b[0m\n", __func__, err);
		return err;
	}
*/
#ifdef ADC_TEST
	if (1) {
		unsigned int i;
		unsigned int tmp_buff[10];
		unsigned int * tmp_prt = &tmp_buff[0];
		printk(KERN_DEBUG "[DEBUG][ADC][%s:%d Test!!!(%p)]\n", __func__, __LINE__, tmp_prt);

		while (1)
		{
			memset(tmp_buff, 0x0, (sizeof(unsigned int)*MAX_CH0_ADC_CHANNEL));

			mt_adc_auto_scan(adc, 0, tmp_prt, 0xf0f0f0f0, 0x0);
			for (i = 0; i < MAX_CH0_ADC_CHANNEL; i++){
				printk(KERN_DEBUG "[DEBUG][ADC] %s : %d channel : %d\n", __func__, i+2, (*(tmp_prt+i)) & 0x00000fff);
			}
			mdelay(1);

			for (i = 0; i < MAX_CH0_ADC_CHANNEL; i++){
				printk(KERN_DEBUG "[DEBUG][ADC] ADC0 ch%d : %d\n", i+2, mt_adc_read(adc, 0, i+2));
			}
			mdelay(1);

			memset(tmp_buff, 0x0, (sizeof(unsigned int)*MAX_CH1_ADC_CHANNEL));

			mt_adc_auto_scan(adc, 1, tmp_prt, 0x0F0F0F0F, 0x0F);
			for (i = 0; i < MAX_CH1_ADC_CHANNEL; i++){
				printk(KERN_DEBUG "[DEBUG][ADC] %s : %d channel : %d\n", __func__, i, (*(tmp_prt+i)) & 0x00000fff);
			}
			mdelay(1);

			for (i=0; i<MAX_CH1_ADC_CHANNEL; i++)
				printk(KERN_DEBUG "[DEBUG][ADC] ADC0 ch%d : %d\n", i, mt_adc_read(adc, 1, i));

			printk(KERN_DEBUG "[DEBUG][ADC] ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
			mdelay(500);
		}
	}
	printk(KERN_DEBUG "[DEBUG][ADC][%s:%d]\n", __func__, __LINE__);
#endif
	return 0;
err_phyclk:
	clk_put(adc->phyclk);
err_hclk:
	clk_put(adc->hclk);
err_fclk:
	clk_put(adc->fclk);

err_get_property:

err_get_reg_addrs:
	devm_kfree(&pdev->dev, adc);
	return ret;
}

static int tcc_mt_adc_remove(struct platform_device *pdev)
{
	struct adc_device *adc = platform_get_drvdata(pdev);
	dma_free_writecombine(&pdev->dev, sizeof(unsigned int), adc->v_ddr, (dma_addr_t)adc->dma_addr);
	#ifdef CONFIG_TCC_MT_ADC0
	mt_adc_power_ctrl(adc, 0, 0);
	#endif
	#ifdef CONFIG_TCC_MT_ADC1
	mt_adc_power_ctrl(adc, 1, 0);
	devm_kfree(&pdev->dev, adc);
	#endif
	return 0;
}

static int tcc_mt_adc_suspend(struct device *dev)
{
	struct adc_device *adc = dev_get_drvdata(dev);
	#ifdef CONFIG_TCC_MT_ADC0
	mt_adc_power_ctrl(adc, 0, 0);
	#endif
	#ifdef CONFIG_TCC_MT_ADC1
	mt_adc_power_ctrl(adc, 1, 0);
	#endif
	return 0;
}

static int tcc_mt_adc_resume(struct device *dev)
{
	struct adc_device *adc = dev_get_drvdata(dev);
	#ifdef CONFIG_TCC_MT_ADC0
	mt_adc_power_ctrl(adc, 0, 1);
	#endif
	#ifdef CONFIG_TCC_MT_ADC1
	mt_adc_power_ctrl(adc, 1, 1);
	#endif
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt_adc_pm_ops, tcc_mt_adc_suspend, tcc_mt_adc_resume);

static ssize_t tcc_adc_show(struct device *_dev,
                             struct device_attribute *attr, char *buf)
{
       return sprintf(buf, "[DEBUG][ADC] Input adc channel number!\n");
}

static ssize_t tcc_adc_store(struct device *_dev,
                              struct device_attribute *attr,
                                  const char *buf, size_t count)
{
       uint32_t ch = simple_strtoul(buf, NULL, 10);
       uint32_t core=0;
       unsigned long data = 0;
       if(ch >= 10) {
	       core = 1;
	       ch = ch - 10;
       }
       else{
	       core = 0;
       }
       printk(KERN_INFO "[INFO][ADC] core = %d / Ch = %d\n", core, ch);
       spin_lock(&adc_spin_lock);
       data = mt_adc_read(adc_dev, core ,ch);
       spin_unlock(&adc_spin_lock);
       printk(KERN_INFO "[INFO][ADC] Get ADC %d : value = 0x%x\n", ch, data);

       return count;
}
DEVICE_ATTR(tccadc, S_IRUGO | S_IWUSR, tcc_adc_show, tcc_adc_store);

#ifdef CONFIG_OF
static const struct of_device_id tcc_mt_adc_dt_ids[] = {
	{.compatible = "telechips,mtadc",},
	{}
};
#else
#define tcc_adc_dt_ids NULL
#endif

static struct platform_driver tcc_mt_adc_driver = {
	.driver	= {
		.name	= "tcc-mt-adc",
		.owner	= THIS_MODULE,
		.pm	= &mt_adc_pm_ops,
		.of_match_table	= of_match_ptr(tcc_mt_adc_dt_ids),
	},
	.probe	= tcc_mt_adc_probe,
	.remove	= tcc_mt_adc_remove,
};

//module_platform_driver(tcc_adc_driver);

static int  tcc_mt_adc_init(void)
{
	//printk("\x1b[1;33m[%s:%d]\x1b[0m\n", __func__, __LINE__);
	platform_driver_register(&tcc_mt_adc_driver);
}
arch_initcall(tcc_mt_adc_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Corporation");

