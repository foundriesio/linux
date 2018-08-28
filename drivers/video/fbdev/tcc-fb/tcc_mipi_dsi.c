/*
 * drivers/video/tcc/tcc_mipi_dsi.c
 *
 * Copyright (c) 2013, Telechips Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <mach/tcc_fb.h>

#include <mach/bsp.h>
#include <mach/vioc_outcfg.h>

#define GRAY_COLOR             "\033[1;30m"
#define NORMAL_COLOR           "\033[0m"
#define RED_COLOR              "\033[1;31m"
#define GREEN_COLOR            "\033[1;32m" 

#define MAGENTA_COLOR          "\033[1;35m"
#define YELLOW_COLOR           "\033[1;33m"
#define BLUE_COLOR             "\033[1;34m"
#define DBLUE_COLOR            "\033[0;34m"
#define WHITE_COLOR            "\033[1;37m"
#define COLORERR_COLOR         "\033[1;37;41m"
#define COLORWRN_COLOR         "\033[0;31m"
#define BROWN_COLOR            "\033[0;40;33m"
#define CYAN_COLOR             "\033[0;40;36m"
#define LIGHTGRAY_COLOR        "\033[1;40;37m"
#define BRIGHTRED_COLOR        "\033[0;40;31m"
#define BRIGHTBLUE_COLOR       "\033[0;40;34m"
#define BRIGHTMAGENTA_COLOR    "\033[0;40;35m"
#define BRIGHTCYAN_COLOR       "\033[0;40;36m" 

struct tcc_db_dsi_data {
	struct lcd_panel *db;
	struct tcc_db_platform_data *mipi_platform_data;
	struct clk *mipi_dsi_clk;
	struct mutex lock;
	bool enabled;
};

static int tcc_db_dsi_init(struct lcd_panel *db)
{
	struct tcc_db_dsi_data *dsi;
	struct clk *dsi_clk = NULL;
	int err = 0;

	dsi = kzalloc(sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;
#if 0 // clock
	dsi_clk = clk_get(0, "dsib");

	if (IS_ERR_OR_NULL(dsi_clk)) {
		err = -EBUSY;
		goto err_clk_put;
	}
#endif//
	mutex_init(&dsi->lock);

	dsi->enabled = true;
	dsi->db = db;
	dsi->mipi_dsi_clk = dsi_clk;
	dsi->mipi_platform_data = get_tcc_db_platform_data();
	
	dsi->mipi_platform_data->set_power(db, TCC_PWR_INIT);
	
	tcc_db_set_outdata(db, (void *)dsi);

	return 0;
	
err_clk_put:
	clk_put(dsi_clk);

err_free_dsi:
	kfree(dsi);

	return err;
}

static void tcc_db_dsi_destroy(struct lcd_panel *db)
{
	struct tcc_db_dsi_data *dsi = (struct tcc_db_dsi_data *)tcc_db_get_outdata(db);

	printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);

	mutex_lock(&dsi->lock);

	clk_put(dsi->mipi_dsi_clk);

	mutex_unlock(&dsi->lock);

	mutex_destroy(&dsi->lock);

	kfree(dsi);
}


static void tcc_db_dsi_enable(struct lcd_panel *db)
{
	struct tcc_db_dsi_data *dsi = tcc_db_get_outdata(db);
	int err;
	u32 val;

	printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);

	mutex_lock(&dsi->lock);

	if (dsi->enabled)	{
		printk(KERN_ERR
		       "ERROR %s: mipi dsi enabled:\n", __func__);
		goto fail;
	}
	dsi->mipi_platform_data->set_power(db, TCC_PWR_ON);
	if(dsi->mipi_dsi_clk)
		clk_enable(dsi->mipi_dsi_clk);	
{
	
	#define sim_debug(x) printk("sim %s \n", x)
	#define sim_value(x) 	printk("sim  0x%x \n", x)
	volatile unsigned int cnt;
	volatile PDDICONFIG	pDDICfg 	= (DDICONFIG *)tcc_p2v(HwDDI_CONFIG_BASE);
	
	volatile MIPI_DSI     	*pHwMIPI_DSI = (MIPI_DSI *)tcc_p2v(HwMIPI_DSI);
#if 1

	// MIPI PHY Reset
	pDDICfg->uMIPICFG.bReg.RESETN = 0; // MIPI PHY Reset
	for(cnt=0;cnt<0x10;cnt++);
	pDDICfg->uMIPICFG.bReg.RESETN = 1;

	
    // MIPI PHY PLL Setting
	pHwMIPI_DSI->PLLTMR      = 0x0800;
	#define TCC420
	//#define TCC320
	//#define TCC154

	#ifdef TCC420
	pHwMIPI_DSI->PHYACCHR     = (4<<5) | (1<<14); 

	pHwMIPI_DSI->PLLCTRL     = (0x0<<28)     // HSzeroCtl
				                        | (0x7<<24)     // FreqBand
				                        | (0x1<<23)     // PllEn
				                        | (0x0<<20)     // PREPRCtl
				                        | (  2<<13)     // P
				                        | (70<< 4)     // M
				                        | (  1<< 1);    // S

	#elif defined(TCC320)
	pHwMIPI_DSI->PHYACCHR     = (3<<5) | (1<<14); 

	pHwMIPI_DSI->PLLCTRL     = (0x0<<28)     // HSzeroCtl
				                        | (0x5<<24)     // FreqBand
				                        | (0x1<<23)     // PllEn
				                        | (0x0<<20)     // PREPRCtl
				                        | (  3<<13)     // P
				                        | (77<< 4)     // M
				                        | (  1<< 1);    // S

	#elif defined(TCC154)
	pHwMIPI_DSI->PHYACCHR     = (3<<5) | (1<<14); 

	pHwMIPI_DSI->PLLCTRL     = (0x0<<28)     // HSzeroCtl
				                        | (0x5<<24)     // FreqBand
				                        | (0x1<<23)     // PllEn
				                        | (0x0<<20)     // PREPRCtl
				                        | (  3<<13)     // P
				                        | (77<< 4)     // M
				                        | (  2<< 1);    // S

	#else
		#error not define PMS frequency
	#endif//

	pHwMIPI_DSI->ESCMODE     = (0x0<<21)     // STOPstate_cnt
	                    | (0x1<<20)     // ForceStopstate
	                    | (0x0<<16)     // ForceBta
	                    | (0x0<< 7)     // CmdLpdt
	                    | (0x0<< 6)     // TxLpdt
	                    | (0x0<< 4)     // TxTriggerRst
	                    | (0x0<< 3)     // TxUlpsDat
	                    | (0x0<< 2)     // TxUlpsExit
	                    | (0x0<< 1)     // TxUlpsClk
	                    | (0x0<< 0);    // TxUlpsClkExit

		udelay(200);

    pHwMIPI_DSI->ESCMODE     = (0x0<<21)     // STOPstate_cnt
                            | (0x0<<20)     // ForceStopstate
                            | (0x0<<16)     // ForceBta
                            | (0x0<< 7)     // CmdLpdt
                            | (0x0<< 6)     // TxLpdt
                            | (0x0<< 4)     // TxTriggerRst
                            | (0x0<< 3)     // TxUlpsDat
                            | (0x0<< 2)     // TxUlpsExit
                            | (0x0<< 1)     // TxUlpsClk
                            | (0x0<< 0);    // TxUlpsClkExit
		udelay(200);

    pHwMIPI_DSI->CLKCTRL     = (0x1<<31)     // TxRequestHsClk
                            | (0x1<<28)     // EscClkEn
                            | (0x0<<27)     // PLLBypass
                            | (0x0<<25)     // ByteClkSrc
                            | (0x1<<24)     // ByteClkEn
                            | (0x1<<23)     // LaneEscClkEn[4] - Data lane 3
                            | (0x1<<22)     // LaneEscClkEn[3] - Data lane 2
                            | (0x1<<21)     // LaneEscClkEn[2] - Data lane 1
                            | (0x1<<20)     // LaneEscClkEn[1] - Data lane 0
                            | (0x1<<19)     // LaneEscClkEn[0] - Clock lane
                            | (0x1<< 0);    // EscPrescaler
		udelay(200);
		printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);
    pHwMIPI_DSI->MDRESOL     = (0x1<<31)     // MainStandby
                            | (  (db->yres) <<16)     // MainVResol
                            | (  (db->xres)<< 0);    // MainHResol
		printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);

    pHwMIPI_DSI->MVPORCH     = (0x0<<28)     // CmdAllow
                            | (  ((db->fewc1) + 1)<<16)     // StableVfp
                            | (  ((db->fswc1) + 1)<< 0);    // MainVbp
		printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);

    pHwMIPI_DSI->MHPORCH     = (  ((db->lewc) + 1)<<16)     // MainHfp
                            | (  ((db->lswc) + 1)<< 0);    // MainHbp

    // MBLANK ?
		printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);
    pHwMIPI_DSI->MSYNC       = (  ((db->fpw1) + 1)<<22)     // MainVsa
                            | (  ((db->lpw) + 1)<< 0);    // MainHsa

    pHwMIPI_DSI->CONFIG      = (0x0<<28)     // EoT_r03
                            | (0x0<<27)     // SyncInform
                            | (0x1<<26)     // BurstMode
                            | (0x1<<25)     // VideoMode
                            | (0x0<<24)     // AutoMode
                            | (0x0<<23)     // HseMode
                            | (0x0<<22)     // HfpMode
                            | (0x0<<21)     // HbpMode
                            | (0x0<<20)     // HsaMode
                            | (0x0<<18)     // MainVc
                            | (0x1<<16)     // SubVc
                            | (0x7<<12)     // MainPixFormat
                            | (0x7<< 8)     // SubPixFormat
                            | (0x3<< 5)     // NumOfDataLane
                            | (0x1<< 4)     // LaneEn[4] - data lane 3 enabler
                            | (0x1<< 3)     // LaneEn[3] - data lane 2 enabler
                            | (0x1<< 2)     // LaneEn[2] - data lane 1 enabler
                            | (0x1<< 1)     // LaneEn[1] - data lane 0 enabler
                            | (0x1<< 0);    // LaneEn[0] - clock lane enabler


    //pHwMIPI_DSI->ESCMODE &= ~(0x1<<20); // ForceStopstate
    pHwMIPI_DSI->ESCMODE = 0;
		printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);
    sim_debug("wait for NOT Stopstate");
    while ( ((pHwMIPI_DSI->STATUS>>8)&0x1) != 0x0 ); // wait for StopstateClk = 0
    //while ( ((pHwMIPI_DSI->STATUS>>0)&0xf) != 0x0 ); // wait for StopstateDat = 0
    sim_value(pHwMIPI_DSI->STATUS);
		printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);

    // enter ulps mode
    pHwMIPI_DSI->ESCMODE     = (0x0<<21)     // STOPstate_cnt
                            | (0x0<<20)     // ForceStopstate
                            | (0x0<<16)     // ForceBta
                            | (0x0<< 7)     // CmdLpdt
                            | (0x0<< 6)     // TxLpdt
                            | (0x0<< 4)     // TxTriggerRst
                            | (0x1<< 3)     // TxUlpsDat
                            | (0x0<< 2)     // TxUlpsExit
                            | (0x1<< 1)     // TxUlpsClk
                            | (0x0<< 0);    // TxUlpsClkExit

    sim_debug("wait for ULPS");
    while ( ((pHwMIPI_DSI->STATUS>>9)&0x1) != 0x1 ); // wait for UlpsClk = 1
    while ( ((pHwMIPI_DSI->STATUS>>4)&0xf) != 0xf ); // wait for UlpsDat = 0xf
    sim_value(pHwMIPI_DSI->STATUS);
    // exit ulps mode
    pHwMIPI_DSI->ESCMODE     = (0x0<<21)     // STOPstate_cnt
                            | (0x0<<20)     // ForceStopstate
                            | (0x0<<16)     // ForceBta
                            | (0x0<< 7)     // CmdLpdt
                            | (0x0<< 6)     // TxLpdt
                            | (0x0<< 4)     // TxTriggerRst
                            | (0x1<< 3)     // TxUlpsDat
                            | (0x1<< 2)     // TxUlpsExit
                            | (0x1<< 1)     // TxUlpsClk
                            | (0x1<< 0);    // TxUlpsClkExit
		udelay(200);
    sim_debug("wait for NOT ULPS");
    while ( ((pHwMIPI_DSI->STATUS>>9)&0x1) != 0x0 ); // wait for UlpsClk = 0
    while ( ((pHwMIPI_DSI->STATUS>>4)&0xf) != 0x0 ); // wait for UlpsDat = 0
    sim_value(pHwMIPI_DSI->STATUS);
		udelay(200);

    pHwMIPI_DSI->ESCMODE     = (0x0<<21)     // STOPstate_cnt
                            | (0x0<<20)     // ForceStopstate
                            | (0x0<<16)     // ForceBta
                            | (0x0<< 7)     // CmdLpdt
                            | (0x0<< 6)     // TxLpdt
                            | (0x0<< 4)     // TxTriggerRst
                            | (0x1<< 3)     // TxUlpsDat
                            | (0x0<< 2)     // TxUlpsExit
                            | (0x1<< 1)     // TxUlpsClk
                            | (0x0<< 0);    // TxUlpsClkExit
    // wait 1ms?
	mdelay(1);
    pHwMIPI_DSI->ESCMODE     = (0x0<<21)     // STOPstate_cnt
                            | (0x0<<20)     // ForceStopstate
                            | (0x0<<16)     // ForceBta
                            | (0x0<< 7)     // CmdLpdt
                            | (0x0<< 6)     // TxLpdt
                            | (0x0<< 4)     // TxTriggerRst
                            | (0x0<< 3)     // TxUlpsDat
                            | (0x0<< 2)     // TxUlpsExit
                            | (0x0<< 1)     // TxUlpsClk
                            | (0x0<< 0);    // TxUlpsClkExit

#endif//		
}


dsi->enabled = true;



fail:
	mutex_unlock(&dsi->lock);
}


static void tcc_db_dsi_disable(struct lcd_panel *db)
{
	int err;
	struct tcc_db_dsi_data *dsi = tcc_db_get_outdata(db);

	printk("%s %s: LINE:%d  %s\n",COLORWRN_COLOR,  __func__ , __LINE__, NORMAL_COLOR);

	mutex_lock(&dsi->lock);

	if (!dsi->enabled)	{
		printk(KERN_ERR
		       "ERROR %s: mipi dsi disabled:\n", __func__);
		goto fail;
	}
	
	{
	volatile PDDICONFIG	pDDICfg 	= (DDICONFIG *)tcc_p2v(HwDDI_CONFIG_BASE);

	volatile MIPI_DSI     	*pHwMIPI_DSI = (MIPI_DSI *)tcc_p2v(HwMIPI_DSI);

	printk(" %s: LINE:%d \n",  __func__ , __LINE__);

	pHwMIPI_DSI->CLKCTRL     = (0x1<<31)     // TxRequestHsClk
	                        | (0x0<<28)     // EscClkEn
	                        | (0x0<<27)     // PLLBypass
	                        | (0x0<<25)     // ByteClkSrc
	                        | (0x0<<24)     // ByteClkEn
	                        | (0x0<<23)     // LaneEscClkEn[4] - Data lane 3
	                        | (0x0<<22)     // LaneEscClkEn[3] - Data lane 2
	                        | (0x0<<21)     // LaneEscClkEn[2] - Data lane 1
	                        | (0x0<<20)     // LaneEscClkEn[1] - Data lane 0
	                        | (0x0<<19)     // LaneEscClkEn[0] - Clock lane
	                        | (0x1<< 0);    // EscPrescaler

	}
	printk(" %s: LINE:%d \n",  __func__ , __LINE__);

	if(dsi->mipi_dsi_clk)
		clk_disable(dsi->mipi_dsi_clk);	
	printk(" %s: LINE:%d \n",  __func__ , __LINE__);

	dsi->mipi_platform_data->set_power(db, TCC_PWR_OFF);
	printk(" %s: LINE:%d \n",  __func__ , __LINE__);

	dsi->enabled = false;	

fail:
	mutex_unlock(&dsi->lock);
}


struct tcc_db_out_ops tcc_db_dsi_ops = {
	.init = tcc_db_dsi_init,
	.destroy = tcc_db_dsi_destroy,
	.enable = tcc_db_dsi_enable,
	.disable = tcc_db_dsi_disable,
	.destroy = tcc_db_dsi_destroy,
};
