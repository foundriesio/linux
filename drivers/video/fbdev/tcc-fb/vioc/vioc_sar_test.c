/*
 * linux/arch/arm/mach-tcc899x/vioc_pixel_mapper.c
 * Author:  <linux@telechips.com>
 * Created: Jan 20, 2018
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
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
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <asm/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_sar.h>
#include <video/tcc/vioc_disp.h>

#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#define SAR_TEST_FUNC

#ifdef SAR_TEST_FUNC
#include <video/tcc/vioc_sar.h>

#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>



#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_global.h>
#include <linux/delay.h>

typedef struct {
    unsigned int nRDMA;
    unsigned int nMixer;
    unsigned int nWDMA;
}TS_Scaler_path;


typedef struct {
    unsigned int width;
    unsigned int height;
}TS_Scaler_size;

TS_Scaler_path Sar_test_Path[] = 
{
	{7, 1, 1}, 
	{10, 2, 2},
	{11, 2, 2},
	{12, 3, 3},
	{13, 3, 3},	
	{16, 5, 6},	
	{17, 6, 8}
};

TS_Scaler_size Sar_dest_size[] = {
	{720, 480},		
	{1920, 1080},
	{1280, 720},		
	{640, 480},		
	{800, 480}
};

TS_Scaler_size Sar_source_size[] = {
	{720, 480},		
	{1920, 1080},
	{1280, 720},		
};

void wdma_sar_off(void)
{
	int plug_err;
 // sar plug out
     plug_err = VIOC_CONFIG_PlugOut(VIOC_SAR0);

 // sar reset
 	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_CLEAR);

	if(plug_err < 0)
	     plug_err = VIOC_CONFIG_PlugOut(VIOC_SAR0);

	if(plug_err < 0)
	    pr_info("[INF][SAR] %s SAR  plug out error %d \n", __func__, plug_err);

}
#define INCLUDE_SAR
#define INCLUDE_SCALER
static int __init test_sar_wdma_api(unsigned int loop)
{
	unsigned int wdma_n, rdma_n, wmix_n, scaler_n;
	unsigned int dest_width, dest_height, width, height = 0;
	unsigned int Src0, Src1, Src2, Dest0, Dest1, Dest2;
	unsigned int mSarPath_TotalCnt = 0, mSarSize_TotalCnt = 0, mSarSRCSize_TotalCnt = 0,i = 0;
	int err_plug;
	void __iomem *wdma_reg;
	void __iomem *rdma_reg;
	void __iomem *wmix_reg, *scaler_reg;


	mSarPath_TotalCnt = sizeof(Sar_test_Path)/sizeof(TS_Scaler_path);
	mSarSize_TotalCnt = sizeof(Sar_dest_size)/sizeof(TS_Scaler_size);
	mSarSRCSize_TotalCnt = sizeof(Sar_source_size)/sizeof(TS_Scaler_size);
	
	i = loop % mSarPath_TotalCnt;
	pr_info("[INF][SAR] %s   loop:%d,   i : %d ", __func__, loop, i);

	rdma_n = Sar_test_Path[i].nRDMA;
	wmix_n = Sar_test_Path[i].nMixer;
	wdma_n =  Sar_test_Path[i].nWDMA;
	scaler_n = 0;


	pr_info("[INF][SAR] LOOP:%d  i:%d  rdma:%d wmixer:%d wdma:%d  scaler:%d \n",loop, i , rdma_n, wmix_n, wdma_n, scaler_n);

	err_plug = VIOC_CONFIG_PlugOut(VIOC_SCALER + scaler_n); // plugin position in scaler
	if(err_plug < 0)
		 pr_info("[INF][SAR] scaler:%d   plug out error :%d \n",scaler_n, err_plug);

#if 1
 	VIOC_CONFIG_SWReset(VIOC_WDMA + wdma_n, VIOC_CONFIG_RESET);
 	VIOC_CONFIG_SWReset(VIOC_WMIX + wmix_n, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_SCALER + scaler_n, VIOC_CONFIG_RESET);
 	VIOC_CONFIG_SWReset(VIOC_RDMA + rdma_n, VIOC_CONFIG_RESET);

	VIOC_CONFIG_SWReset(VIOC_RDMA + rdma_n, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(VIOC_SCALER + scaler_n, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(VIOC_WMIX + wmix_n, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(VIOC_WDMA + wdma_n, VIOC_CONFIG_CLEAR);	
#endif//

	scaler_reg = VIOC_SC_GetAddress(VIOC_SCALER + scaler_n);
	wdma_reg = VIOC_WDMA_GetAddress(VIOC_WDMA + wdma_n);
	rdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA + rdma_n);
	wmix_reg = VIOC_WMIX_GetAddress(VIOC_WMIX + wmix_n);

	VIOC_RDMA_SetImageDisable(rdma_reg);
	VIOC_WDMA_SetImageDisable(wdma_reg);
	
	if(err_plug < 0)
	{
		err_plug = VIOC_CONFIG_PlugOut(VIOC_SCALER + scaler_n); // plugin position in scaler
		if(err_plug < 0)
			 pr_err("[ERR][SAR] **  scaler:%d   plug out error :%d \n",scaler_n, err_plug);
	}

#ifdef INCLUDE_SCALER
	width = 1920;
	height = 1080;
	
#else
	dest_width = width = 720;
	dest_height = height = 480;
#endif//

#if 1
	i = loop % mSarSRCSize_TotalCnt;
	width = Sar_source_size[i].width;
	height = Sar_source_size[i].height;

	i = loop % mSarSize_TotalCnt;
	dest_width = Sar_dest_size[i].width;
	dest_height = Sar_dest_size[i].height;
#endif//

	
	Src0 = 0x30000000 ;
	Src1 = Src0 + (width * height);
	Src2 = Src1 + (width * height)/2;

	Dest0 = 0x42300000 + (i * 0x1000000);
	Dest1 = Dest0 + (width * height);
	Dest2 = Dest1 + (width * height)/2;


	if(rdma_n ==  10)
		VIOC_CONFIG_WMIXPath(VIOC_RDMA + 11, 1);
	if((rdma_n ==  11) || (rdma_n == 12))
		VIOC_CONFIG_WMIXPath(VIOC_RDMA + rdma_n, 0);
	else if(rdma_n == 13)
		VIOC_CONFIG_WMIXPath(VIOC_RDMA + 12, 1);
	else
		VIOC_CONFIG_WMIXPath(VIOC_RDMA + rdma_n, 1);

	// WMXIER 
	VIOC_WMIX_SetSize(wmix_reg, dest_width, dest_height);
	VIOC_WMIX_SetUpdate(wmix_reg);

#ifdef INCLUDE_SAR
	//sar in setting.
	VIOC_SAR_Set(width, height);

	//SAR IF Register config
     VIOC_CONFIG_PlugIn(VIOC_SAR0, VIOC_RDMA + rdma_n);
#endif//


#ifdef INCLUDE_SCALER
	VIOC_CONFIG_PlugIn(VIOC_SCALER + scaler_n, VIOC_RDMA + rdma_n); // plugin position in scaler
	VIOC_SC_SetDstSize(scaler_reg, dest_width, dest_height);
	VIOC_SC_SetOutSize(scaler_reg, dest_width, dest_height);
	VIOC_SC_SetOutPosition(scaler_reg, 0, 0);

	if((width == dest_width) && (height == dest_height))
		VIOC_SC_SetBypass(scaler_reg, 1);
	else
		VIOC_SC_SetBypass(scaler_reg, 0);

	VIOC_SC_SetUpdate(scaler_reg);	
#endif//

	
	// RDMA setting...
	VIOC_RDMA_SetImageFormat	(rdma_reg, VIOC_IMG_FMT_ARGB8888);
	VIOC_RDMA_SetImageSize(rdma_reg, width, height);

	VIOC_RDMA_SetImageBase(rdma_reg, Src0, Src1, Src2);
	VIOC_RDMA_SetImageOffset	(rdma_reg, (1920 * 4), (1920 * 4));
	VIOC_RDMA_SetImageR2YEnable(rdma_reg, 1);

	VIOC_RDMA_SetDataFormat(rdma_reg, 0, 0);
	VIOC_RDMA_SetImageEnable(rdma_reg);
	VIOC_RDMA_SetImageUpdate	(rdma_reg);

#ifdef INCLUDE_SAR
// sar setting.
	VIOC_SAR_TurnOn((SAR_LOW), width, height);
//	VIOC_SAR_TurnOn((i % SAR_MAX), width, height);
	VIOC_SARIF_SetNI(1);
      VIOC_SARIF_SetTimingParamSimple(width, height);
	VIOC_SARIF_SetFormat(8);

	VIOC_SARIF_SetSize (width, height);
	VIOC_SARIF_TurnOn();
	pr_info("[INF][SAR] VIOC SARIF TurnOn ~~ \n");
#endif//

	// WDMA setting...
	VIOC_WDMA_SetIreqStatus(wdma_reg, 0x1FF);
	VIOC_WDMA_SetImageFormat (wdma_reg, VIOC_IMG_FMT_ARGB8888);
	#if 1
	VIOC_WDMA_SetDataFormat(wdma_reg, 0, 0);
	#else
	VIOC_WDMA_SetImagePackingMode(wdma_reg, 5);
	#endif//

	VIOC_WDMA_SetImageSize(wdma_reg, dest_width, dest_height);
	VIOC_WDMA_SetImageY2REnable(wdma_reg, 1);
	VIOC_WDMA_SetImageBase(wdma_reg, Dest0, Dest1, Dest2);
	VIOC_WDMA_SetImageOffset (wdma_reg, dest_width*4, dest_width*4);
	VIOC_WDMA_SetImageEnable (wdma_reg, 0);

    //VIOC_DISP_SetTimingParamSimple(HwVIOC_DISP0, width, height);
    //VIOC_DISP_SetSize (HwVIOC_DISP0, width, height);
   //((VIOC_DISP *)(HwVIOC_DISP0))->uCTRL.bReg.PXDW = 8; 
    //((VIOC_DISP *)(HwVIOC_DISP0))->uCTRL.bReg.Y2R = 1; 	
	//((VIOC_DISP *)(HwVIOC_DISP0))->uCTRL.bReg.PXDW = 0x17; 	



	volatile void __iomem *UIrdma_reg;
	
	UIrdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA+3);
	VIOC_RDMA_SetImageFormat	(UIrdma_reg, VIOC_IMG_FMT_ARGB8888);

	if(dest_width > 1280)
		VIOC_RDMA_SetImageSize(UIrdma_reg, 1280, 720);
	else
		VIOC_RDMA_SetImageSize(UIrdma_reg, dest_width, dest_height);
	
	VIOC_RDMA_SetImageBase(UIrdma_reg, Dest0, Dest1, Dest2);
	VIOC_RDMA_SetImageOffset(UIrdma_reg, (dest_width * 4), (dest_width * 4));
	VIOC_RDMA_SetImageEnable(UIrdma_reg);
	VIOC_RDMA_SetImageUpdate(UIrdma_reg);

	{
		unsigned int i, status;

		for(i = 0; i < 200; i++ )
		{
			VIOC_WDMA_GetStatus(wdma_reg, &status);
			if((WDMAIRQSTS_EOFR_MASK & status ) || (WDMAIRQSTS_EOFF_MASK & status )) {
				if (!(WDMAIRQSTS_EOFR_MASK & status )) 	{
					msleep(100);					
					VIOC_WDMA_GetStatus(wdma_reg, &status);				
					pr_info("[INF][SAR] ~~~~~status :0x%08x  ~~~~\n", status);
				}
				break;
			}
			msleep(100);
		}
		VIOC_WDMA_GetStatus(wdma_reg, &status);	
		pr_info("[INF][SAR] ~~~~~status :0x%08x  end loop :%d   == %d x %d ~~%d x %d ~~~~\n", status, i, width, height, dest_width, dest_height);
	}

	VIOC_RDMA_SetImageDisable(rdma_reg);
	VIOC_WDMA_SetImageDisable(wdma_reg);

#if 1
 	VIOC_CONFIG_SWReset(VIOC_WDMA + wdma_n, VIOC_CONFIG_RESET);
 	VIOC_CONFIG_SWReset(VIOC_WMIX + wmix_n, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_SCALER + scaler_n, VIOC_CONFIG_RESET);
 	VIOC_CONFIG_SWReset(VIOC_RDMA + rdma_n, VIOC_CONFIG_RESET);

	VIOC_CONFIG_SWReset(VIOC_RDMA + rdma_n, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(VIOC_SCALER + scaler_n, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(VIOC_WMIX + wmix_n, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(VIOC_WDMA + wdma_n, VIOC_CONFIG_CLEAR);	
#endif//
	
}


static int __init test_wdma(void)
{
	unsigned int i = 0;
	
	for(i = 0; i < 20; i ++)
		printk("\n \n");
	VIOC_SAR_POWER_ONOFF(1);

 	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_CLEAR);
	msleep(100);
	i = 0;
	while (1) {
		pr_info("[INF][SAR]          start test_sar_wdma_api call loop:%d  ################### \n", i);

		test_sar_wdma_api(i);
		i++;

		msleep(2000);
		wdma_sar_off();
		printk("\n");
	}

	return 0;
}

static int __init test_disp_api(unsigned int Nrdma_n)
{
	int ret = 0;
	unsigned int wdma_n, rdma_n, wmix_n, scaler_n;
	unsigned int disp_width, disp_height, width, height = 0;
	unsigned int Src0, Src1, Src2, Dest0, Dest1, Dest2;
	
	volatile void __iomem *wdma_reg;
	volatile void __iomem *rdma_reg, *UIrdma_reg;
	volatile void __iomem *wmix_reg;
	volatile void __iomem *scaler_reg;
	volatile void __iomem *disp_reg;
	unsigned int disp_status = 0;

	rdma_n = Nrdma_n;
	wmix_n = 0;
	wdma_n =  6;
	scaler_n = 0;
	
	wdma_reg = VIOC_WDMA_GetAddress(VIOC_WDMA + wdma_n);
	rdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA + rdma_n);
	UIrdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA);

	wmix_reg = VIOC_WMIX_GetAddress(VIOC_WMIX + wmix_n);
	scaler_reg = VIOC_SC_GetAddress(VIOC_SCALER + scaler_n);
	disp_reg = VIOC_DISP_GetAddress(VIOC_DISP);


	VIOC_DISP_SetY2R(disp_reg, 1);
	VIOC_RDMA_SetImageR2YEnable(UIrdma_reg, 1);
	VIOC_RDMA_SetImageUpdate(UIrdma_reg);

	msleep(100);
#if 0
	width = 2176;
	height = 2160;
	width = 1920;
	height = 1080;
	width = 1280;
	height = 720;
#endif//

	width = 1280;
	height = 720;

	disp_width = 320;
	disp_height = 240;

	Src0 = 0x2c800000;
	Src1 = Src0 + (width * height);
	Src2 = Src1 + (width * height)/2;

	Dest0 = 0x42300000;
	Dest1 = Dest0 + (width * height);
	Dest2 = Dest1 + (width * height)/2;

	// RDMA setting...
	VIOC_RDMA_SetImageFormat	(rdma_reg, VIOC_IMG_FMT_ARGB8888);
	VIOC_RDMA_SetImageR2YEnable(rdma_reg, 1);
	VIOC_RDMA_SetImageY2REnable(rdma_reg, 0);
	VIOC_RDMA_SetImageSize(rdma_reg, width, height);
	VIOC_RDMA_SetImageBase(rdma_reg, Src0, Src1, Src2);
	VIOC_RDMA_SetImageOffset	(rdma_reg, (1920 * 4), (1920 * 4));
	VIOC_RDMA_SetDataFormat(rdma_reg, 0, 0);
#if 0
	printk("scaler setting \n");
	VIOC_CONFIG_PlugIn(VIOC_SCALER + scaler_n, VIOC_RDMA + rdma_n); // plugin position in scaler
	VIOC_SC_SetDstSize(scaler_reg, disp_width, disp_height);
	VIOC_SC_SetOutSize(scaler_reg, disp_width, disp_height);
	VIOC_SC_SetBypass(scaler_reg, 0);
	VIOC_SC_SetOutPosition(scaler_reg, 0, 0);
	VIOC_SC_SetUpdate(scaler_reg);	
#endif//
	VIOC_WMIX_SetPosition(wmix_reg, rdma_n, 0, 0);
	VIOC_WMIX_SetUpdate(wmix_reg);

// commnet jung
	VIOC_RDMA_SetImageEnable(rdma_reg);
	VIOC_RDMA_SetImageUpdate	(rdma_reg);

	msleep(20);

    //SAR IF Register config
    VIOC_CONFIG_PlugIn(VIOC_SAR0, VIOC_RDMA + rdma_n);

#if 0
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_RESET);
	msleep(1);
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_CLEAR);
#endif//
//	VIOC_RDMA_SetImageDisableNW(UIrdma_reg);
//	VIOC_DISP_TurnOff(disp_reg);

	msleep(1);

// WMXIER 
//	VIOC_WMIX_SetSize 			(wmix_reg, width, height);
//	VIOC_WMIX_SetUpdate 		(wmix_reg);


	//sar in setting.
	VIOC_SAR_Set(width, height);

	// sar setting.
	VIOC_SAR_TurnOn(SAR_LOW, width, height);

	VIOC_SARIF_SetNI(1);
      VIOC_SARIF_SetTimingParamSimple(width, height);
	VIOC_SARIF_SetFormat(8);
	VIOC_SARIF_SetSize (width, height);
	VIOC_SARIF_TurnOn();

	pr_info("[INF][SAR] Setting End %d ~~ Sar dma: %08lx UI: %08lx ~~\n",Nrdma_n, __raw_readl(rdma_reg),__raw_readl(UIrdma_reg));
	
	msleep(2000);

	VIOC_DISP_SetStatus(disp_reg, 0x3F);


	do{
		VIOC_DISP_GetStatus(disp_reg, &disp_status);
	}	while(!(disp_status & 0x4));

	VIOC_RDMA_SetImageDisableNW(rdma_reg);

	ret = VIOC_CONFIG_PlugOut(VIOC_SAR0);
	if(ret != 0) {
		pr_info("[INF][SAR] PLUT OUT Sar error:%d ~~~~~~~ \n", ret);
//		while(1);
	}
	
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_RESET);
	msleep(1);
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_CLEAR);
	msleep(26);

	pr_info("[INF][SAR] off DMA: %d ~~ Sar dma: %08lx UI: %08lx ~~0x%08x\n",Nrdma_n, __raw_readl(rdma_reg),__raw_readl(UIrdma_reg), disp_status);

}


static int __init test_disp(void)
{
	volatile unsigned int rdma_n, i , j;

	VIOC_SAR_POWER_ONOFF(1);
 	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_RESET);
	msleep(1);
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_CLEAR);
 
while(1){

	
	for(j = 0; j < 10; j++)
	{
		for(i = 0; i < 30 ; i++)
		{
			rdma_n = i % 4;
			if(rdma_n == 0)
				continue;

			pr_info("[INF][SAR] loop :%d rdma_n:%d \n", j * i , rdma_n);
		//	test_disp_api(rdma_n);
			test_disp_api(1);

		}
		msleep(10);
		pr_info("\x1b[1;38m[INF][SAR] ~~~~~~------J = %d------ ~~~~~~~  \x1b[0m \n", j);
	}

	pr_info("\x1b[1;38m[INF][SAR] ~~~~~~wdma ~~~~~~~  \x1b[0m \n");

	test_sar_wdma_api(0);
	msleep(100);

	VIOC_CONFIG_PlugOut(VIOC_SAR0);
	msleep(100);
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_RESET);
	msleep(1);
	VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_CLEAR);
	msleep(50);

	pr_info("\x1b[1;38m[INF][SAR] ~~~~~~display ~~~~~~~  \x1b[0m \n");

	VIOC_SAR_POWER_ONOFF(1);
	for(i = 0; i < 30 ; i++)
	{
		rdma_n = i % 4;
		if(rdma_n == 0)
			continue;
		test_disp_api(rdma_n);

	}
	VIOC_SAR_POWER_ONOFF(0);
}
	pr_info("\x1b[1;38m[INF][SAR] ~~~~~~test end ~~~~~~~  \x1b[0m \n");
	
	while(1);
}

#endif //SAR_TEST_FUNC

#ifdef SAR_TEST_FUNC
module_init(test_wdma);
#endif//
