/*
 * vioc_v_dv.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
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
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/delay.h>

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_dv_cfg.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_ddicfg.h>

static struct device_node *pVDV_np;
static struct device_node *pVEDR_np;
static struct device_node *pDNG_np;
static volatile void __iomem *pVDV_reg[EDR_MAX];
static volatile void __iomem *pVEDR_reg[VEDR_MAX];
static volatile void __iomem *pDNG_reg;

#if defined(CONFIG_TCC_DV_IN)
#define DV_CLK_CTRL
#ifdef DV_CLK_CTRL
struct clk *peri_lcd0_clk = NULL;
#endif
#endif

static int debug = 0;
#define dprintk(msg...)	\
	if (debug) {	\
		printk("[DBG][V_DV] " msg);	\
	}

static volatile void __iomem *get_v_dv_reg(volatile void __iomem *pRDMA)
{
	int nRdma = EDR_MAX - 1;

	if (!VIOC_CONFIG_DV_GET_EDR_PATH())
		return NULL;

	while (nRdma >= 0) {
		if (pRDMA == VIOC_RDMA_GetAddress(nRdma))
			break;
		nRdma--;
	}

	if (nRdma < 0)
		return NULL;
	else
		return VIOC_DV_GetAddress((DV_DISP_TYPE)nRdma);
}

static char convert_v_dv_PixelFmt(char rdma_format)
{
	// There is no need to configure proper format because DV receives the
	// data by sinaling base. should confirm this. perhaps there might be
	// any setting on DV block.
	return VIOC_PXDW_FMT_24_RGB888;
}

void VIOC_V_DV_SetInterruptEnable(volatile void __iomem *reg,
				  unsigned int nInterrupt, unsigned int en)
{
	unsigned long val;

	val = (__raw_readl(reg + INT_EN) & ~(nInterrupt));
	if (en)
		val |= nInterrupt;
	__raw_writel(val, reg + INT_EN);
}

void VIOC_V_DV_GetInterruptPending(volatile void __iomem *reg,
				   unsigned int *pPending)
{
	*pPending = (unsigned int)(__raw_readl(reg + INT_PEND));
}

void VIOC_V_DV_GetInterruptStatus(volatile void __iomem *reg,
				  unsigned int *pStatus)
{
	*pStatus = (unsigned int)(__raw_readl(reg + INT_STS));
}

void VIOC_V_DV_ClearInterrupt(volatile void __iomem *reg,
			      unsigned int nInterrupt)
{
	unsigned long val;

	val = (__raw_readl(reg + INT_CLR) & ~(nInterrupt));
	val |= nInterrupt;
	__raw_writel(val, reg + INT_CLR);
}

int VIOC_V_DV_Is_EdrRDMA(volatile void __iomem *pRDMA)
{
	if (get_v_dv_reg(pRDMA) != NULL)
		return 1;
	return 0;
}

void VIOC_V_DV_SetSize(volatile void __iomem *pDISP,
		       volatile void __iomem *pRDMA, unsigned int sx,
		       unsigned int sy, unsigned int width, unsigned int height)
{
	volatile void __iomem *pDisp_DV = NULL;

	if (pDISP)
		pDisp_DV = pDISP;
	else
		pDisp_DV = get_v_dv_reg(pRDMA);

	if (pDisp_DV && VIOC_CONFIG_DV_GET_EDR_PATH()) {
		dprintk("%s-%d\n", __func__, __LINE__);

		VIOC_DISP_SetPosition(pDisp_DV, sx, sy);
		VIOC_DISP_SetSize(pDisp_DV, width, height);
	}
}

void VIOC_V_DV_SetPosition(volatile void __iomem *pDISP,
		       volatile void __iomem *pRDMA, unsigned int sx,
		       unsigned int sy)
{
	volatile void __iomem *pDisp_DV = NULL;

	if (pDISP)
		pDisp_DV = pDISP;
	else
		pDisp_DV = get_v_dv_reg(pRDMA);

	if (pDisp_DV && VIOC_CONFIG_DV_GET_EDR_PATH()) {
		dprintk("%s-%d\n", __func__, __LINE__);

		VIOC_DISP_SetPosition(pDisp_DV, sx, sy);
	}
}

void VIOC_V_DV_SetPXDW(volatile void __iomem *pDISP,
		       volatile void __iomem *pRDMA, unsigned int pixel_fmt)
{
	volatile void __iomem *pDisp_DV = NULL;
	unsigned int PXDW = 0;

	if (pDISP) {
		pDisp_DV = pDISP;
		PXDW = pixel_fmt;
	} else {
		pDisp_DV = get_v_dv_reg(pRDMA);
		PXDW = convert_v_dv_PixelFmt(pixel_fmt);
	}

	if (pDisp_DV && VIOC_CONFIG_DV_GET_EDR_PATH()) {
		dprintk("%s-%d\n", __func__, __LINE__);

		VIOC_DISP_SetPXDW(pDisp_DV, PXDW);
	}
}

void VIOC_V_DV_SetBGColor(volatile void __iomem *pDISP,
			  volatile void __iomem *pRDMA, unsigned int R_y,
			  unsigned int G_u, unsigned int B_v,
			  unsigned int alpha)
{
	volatile void __iomem *pDisp_DV = NULL;

	if (pDISP) {
		pDisp_DV = pDISP;
	} else {
		pDisp_DV = get_v_dv_reg(pRDMA);
	}

	if (pDisp_DV && VIOC_CONFIG_DV_GET_EDR_PATH()) {
		dprintk("%s-%d\n", __func__, __LINE__);

		VIOC_DISP_SetBGColor(pDisp_DV, R_y, G_u, B_v, alpha);
	}
}

static int _Set_BG_Color(volatile void __iomem *pDISP_DV)
{
	int off = 0;

	if (pDISP_DV == VIOC_DV_GetAddress(/*EDR_OSD1*/RDMA_FB1) ||
	    pDISP_DV == VIOC_DV_GetAddress(/*EDR_OSD3*/RDMA_FB)) {
		VIOC_V_DV_SetBGColor(pDISP_DV, NULL, 0, 0, 0, 0); // Black RGB
		off = 1;
	} else {
		VIOC_V_DV_SetBGColor(pDISP_DV, NULL, 0, 0x1FF, 0x1FF,
				     0x0); // Black YUV
		off = 0;
	}

	return off;
}

static unsigned int VIOC_V_DV_Get_TurnOn(volatile void __iomem *reg)
{
	return (__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK) ? 1 : 0;
}

void VIOC_V_DV_Turnon(volatile void __iomem *pDISP,
		      volatile void __iomem *pRDMA)
{
	volatile volatile void __iomem *pDisp_DV = NULL;

	if (pDISP) {
		pDisp_DV = pDISP;
	} else {
		pDisp_DV = get_v_dv_reg(pRDMA);
	}

	if (pDisp_DV && VIOC_CONFIG_DV_GET_EDR_PATH()) {
		unsigned long val;

		if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD1*/RDMA_FB1)) {
			if (!pRDMA)
				pRDMA = VIOC_RDMA_GetAddress(/*EDR_OSD1*/RDMA_FB1);
			VIOC_RDMA_SetImageRGBSwapMode(pRDMA, 0x4); // BRG
		} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD3*/RDMA_FB)) {
			if (!pRDMA)
				pRDMA = VIOC_RDMA_GetAddress(/*EDR_OSD3*/RDMA_FB);
			VIOC_RDMA_SetImageRGBSwapMode(pRDMA, 0x4); // BRG
		}
		_Set_BG_Color(pDisp_DV);

	#if defined(DOLBY_VISION_CHECK_SEQUENCE)
		if(!VIOC_V_DV_Get_TurnOn(pDisp_DV))
		{
			if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD1*/RDMA_FB1)) {
				dprintk_dv_sequence("### ====> %d Stream I/F On \n", RDMA_FB1);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD3*/RDMA_FB)) {
				dprintk_dv_sequence("### ====> %d Stream I/F On \n", RDMA_FB);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_BL*/RDMA_VIDEO)) {
				dprintk_dv_sequence("### ====> %d Stream I/F On \n", RDMA_VIDEO);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_EL*/RDMA_VIDEO_SUB)) {
				dprintk_dv_sequence("### ====> %d Stream I/F On \n", RDMA_VIDEO_SUB);
			}
		}
	#endif

		val = (__raw_readl(pDisp_DV + DCTRL) & ~(DCTRL_SRST_MASK));
		val |= ((0x1 << DCTRL_SRST_SHIFT) | ((0x1 << DCTRL_LEN_SHIFT)));
		__raw_writel(val, pDisp_DV + DCTRL);
	}
}

void VIOC_V_DV_Turnoff(volatile void __iomem *pDISP,
		       volatile void __iomem *pRDMA)
{
	volatile void __iomem *pDisp_DV = NULL;

	if (pDISP)
		pDisp_DV = pDISP;
	else
		pDisp_DV = get_v_dv_reg(pRDMA);

	if (pDisp_DV && VIOC_CONFIG_DV_GET_EDR_PATH())
	{
	#if defined(DOLBY_VISION_CHECK_SEQUENCE)
		if(VIOC_V_DV_Get_TurnOn(pDisp_DV))
		{
			if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD1*/RDMA_FB1)) {
				dprintk_dv_sequence("### ====> %d Stream I/F Off \n", RDMA_FB1);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD3*/RDMA_FB)) {
				dprintk_dv_sequence("### ====> %d Stream I/F Off \n", RDMA_FB);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD3*/RDMA_VIDEO)) {
				dprintk_dv_sequence("### ====> %d Stream I/F Off \n", RDMA_VIDEO);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD3*/RDMA_VIDEO_SUB)) {
				dprintk_dv_sequence("### ====> %d Stream I/F Off \n", RDMA_VIDEO_SUB);
			}
		}
	#endif

		//if (0 < _Set_BG_Color(pDisp_DV))
			VIOC_DISP_TurnOff(pDisp_DV);
	}
}

void VIOC_V_DV_All_Turnoff()
{
	int ret = 0;
	int nRdma = EDR_MAX - 1;
	int bDisp_On[EDR_MAX] = {
		0,
	};
	unsigned int enabled = 0;
	volatile void __iomem *pDisp_DV = NULL;
	volatile void __iomem *pRdma_DV = NULL;


// Very Important Sequence for DV_IN use case!!!
// ON  :: EDR Path -> Streaming IF -> V_DV -> RDMA -> DISP -> DV_IN -> WDMA
// OFF :: WDMA -> RDMA -> Streaming IF -> V_DV -> EDR Path -> DV_IN and reset -> DISP and reset

	if (!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	dprintk_dv_sequence("### All Stream I/F  =>  RDMA Off \n");

	while (nRdma >= 0) {
		unsigned long val;
		pRdma_DV = VIOC_RDMA_GetAddress(nRdma);
		pDisp_DV = get_v_dv_reg(pRdma_DV);

		bDisp_On[nRdma] = VIOC_V_DV_Get_TurnOn(pDisp_DV);
		val = (__raw_readl(pDisp_DV + DSTATUS) &
		       ~(VIOC_DISP_IREQ_DD_MASK));
		val |= (VIOC_DISP_IREQ_DD_MASK);
		__raw_writel(val, pDisp_DV + DSTATUS); // clear DD status!!

	#if defined(DOLBY_VISION_CHECK_SEQUENCE)
		if(VIOC_V_DV_Get_TurnOn(pDisp_DV))
		{
			if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD1*/RDMA_FB1)) {
				dprintk_dv_sequence("### ======> %d Stream I/F Off \n", RDMA_FB1);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_OSD3*/RDMA_FB)) {
				dprintk_dv_sequence("### ======> %d Stream I/F Off \n", RDMA_FB);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_BL*/RDMA_VIDEO)) {
				dprintk_dv_sequence("### ======> %d Stream I/F Off \n", RDMA_VIDEO);
			} else if (pDisp_DV == VIOC_DV_GetAddress(/*EDR_EL*/RDMA_VIDEO_SUB)) {
				dprintk_dv_sequence("### ======> %d Stream I/F Off \n", RDMA_VIDEO_SUB);
			}
		}
	#endif

		VIOC_DISP_TurnOff(pDisp_DV);
		nRdma--;
	}

	nRdma = EDR_MAX - 1;
	while (nRdma >= 0) {
		if (bDisp_On[nRdma]) {
			pRdma_DV = VIOC_RDMA_GetAddress(nRdma);
			pDisp_DV = get_v_dv_reg(pRdma_DV);
			dprintk_dv_sequence("### Stream I/F [%d] Off Waiting~~ \n", nRdma);
			if (0 == (ret = VIOC_DISP_Wait_DisplayDone(pDisp_DV))) {
				pr_info("[INF][V_DV] %s-%d DD Checking :: %d Stream-I/F (0x%x : 0-Timeout).\n",
				       __func__, __LINE__, nRdma, ret);
			}

			VIOC_RDMA_SetImageDisable(pRdma_DV);
		}
		nRdma--;
	}

	VIOC_V_DV_Power(0);
}

void VIOC_V_DV_Power(char on)
{
	volatile void __iomem *pDDICONFIG = VIOC_DDICONFIG_GetAddress();

	dprintk_dv_sequence("### V_DV Power %s (for path %d) sequence IN ===> \n", on ? "On" : "Off", vioc_get_path_type());
	if (on) {
		if ( DV_PATH_DIRECT & vioc_get_path_type() ) {
			/* nothing to do */
			//VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, 3, 1);
		}
	#if defined(CONFIG_TCC_DV_IN)
		else
		{
			// DV_IN :: 0x1200A128(PWDN)/0x1200A12C(SW-Reset)
		#ifdef DV_CLK_CTRL
			if ( DV_PATH_VIN_DISP & vioc_get_path_type() )
			{
				if(vioc_v_dv_check_hdmi_out())
				{
					int ret = 0;

					dprintk_dv_sequence("### V_DV : Use lcd0 clk, %d khz \n", vioc_v_dv_get_lcd0_clk_khz());
					ret = clk_set_rate(peri_lcd0_clk, vioc_v_dv_get_lcd0_clk_khz()*1000);
					if (ret) {
						pr_info("[INF][V_DV] %s-%d Clock rate change failed %d\n", __func__, __LINE__, ret);
					}
				}

				if (peri_lcd0_clk)
					clk_prepare_enable(peri_lcd0_clk);
			}
		#endif
			//VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, 3, 0);
		}
	#endif

		VIOC_DDICONFIG_SetPWDN(pDDICONFIG, DDICFG_TYPE_NG, 1);
		VIOC_DDICONFIG_SetPWDN(pDDICONFIG, DDICFG_TYPE_DV, 1);
#if 0 // specific sequence!!
		mdelay(2);
		VIOC_DDICONFIG_SetPWDN(pDDICONFIG, DDICFG_TYPE_DV, 0);
		mdelay(2);
		VIOC_V_DV_SWReset(1, 0);
		mdelay(2);
		VIOC_DDICONFIG_SetPWDN(pDDICONFIG, DDICFG_TYPE_DV, 1);
#endif
	} else {
		volatile void __iomem *reg;

		// change the display path from EDR to VIOC.
		VIOC_CONFIG_DV_SET_EDR_PATH(0);
		vioc_v_dv_set_stage(DV_OFF);

		if (DV_PATH_DIRECT & vioc_get_path_type()) {
			// NexGuard - line swap!!
			reg = VIOC_DNG_GetAddress();
			__raw_writel(0x00000000, reg + 0x34);
		}

		// disable Meta DMA.
		dprintk_dv_sequence("### V_DV Meta-DMA Off\n");
		VIOC_CONFIG_DV_Metadata_Disable();
		dprintk_dv_sequence("### V_DV NexGuard Off\n");
		VIOC_DDICONFIG_SetPWDN(pDDICONFIG, DDICFG_TYPE_NG, 0);mdelay(50);

		//vioc_v_dv_swreset(1, 1, 1);
#if 0 // specific sequence!!
		mdelay(2);
		VIOC_V_DV_SWReset(1, 1);
#else
		VIOC_V_DV_SWReset(1, 1);
		VIOC_V_DV_SWReset(1, 0);
#endif

		dprintk_dv_sequence("### V_DV-IP Off\n");
		VIOC_DDICONFIG_SetPWDN(pDDICONFIG, DDICFG_TYPE_DV, 0);

#if defined(CONFIG_TCC_DV_IN)
		if ( !(DV_PATH_DIRECT & vioc_get_path_type()) && (DV_PATH_VIN_DISP & vioc_get_path_type())) {
			// DV_IN :: 0x1200A128(PWDN)/0x1200A12C(SW-Reset)
	#ifdef DV_CLK_CTRL
			if(peri_lcd0_clk)
				clk_disable_unprepare(peri_lcd0_clk);
	#endif
		}

		if( (DV_PATH_VIN_DISP & vioc_get_path_type())
			|| (DV_PATH_DIRECT_VIN_WDMA == vioc_get_path_type())
		){
			VIOC_DISP_TurnOnOff_With_DV(VIOC_DISP_GetAddress(0), 0);

			VIOC_DV_IN_SetEnable(0);
			VIOC_CONFIG_SWReset(VIOC_DV_IN, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(VIOC_DV_IN, VIOC_CONFIG_CLEAR);
			dprintk_dv_sequence("### DV_IN S/W Reset \n");
		}
#endif
	}
}

void VIOC_V_DV_SWReset(unsigned int force, unsigned int bReset)
{
	if (VIOC_CONFIG_DV_GET_EDR_PATH() || force) {
		dprintk_dv_sequence("### V_DV Reset\n");

		if(bReset) {
			VIOC_CONFIG_SWReset(VIOC_DV_DISP0, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(VIOC_DV_DISP1, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(VIOC_DV_DISP2, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(VIOC_DV_DISP3, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(VIOC_V_DV, VIOC_CONFIG_RESET); // strange frame issues...!!!!!
		}
		else{
			VIOC_CONFIG_SWReset(VIOC_V_DV, VIOC_CONFIG_CLEAR); // strange frame issues...!!!!!
			VIOC_CONFIG_SWReset(VIOC_DV_DISP3, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(VIOC_DV_DISP2, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(VIOC_DV_DISP1, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(VIOC_DV_DISP0, VIOC_CONFIG_CLEAR);
		}
	}
}

void VIOC_V_DV_Base_Configure(int sx, int sy, int w, int h)
{
	volatile void __iomem *pReg = NULL;
	int nEDR_dma = EDR_EL;

	if (!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	dprintk("%s-%d\n", __func__, __LINE__);
	// reset
	//VIOC_V_DV_SWReset(0, 1);
	//VIOC_V_DV_SWReset(0, 0);

//	dprintk_dv_sequence("### All Stream I/F On \n");

	while (nEDR_dma >= 0) {
		pReg = VIOC_DV_GetAddress((DV_DISP_TYPE)nEDR_dma);
		VIOC_V_DV_SetSize(pReg, NULL, sx, sy, w, h);
		VIOC_V_DV_SetPXDW(pReg, NULL, VIOC_PXDW_FMT_24_RGB888);
		_Set_BG_Color(pReg);
		VIOC_V_DV_Turnon(pReg, NULL);
		nEDR_dma--;
	}

	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		unsigned int val;

		pReg = VIOC_DV_VEDR_GetAddress(VDV_CFG);
		__raw_writel((160), pReg + OSD0_DLY_H);     // default: 160
		__raw_writel((13), pReg + OSD0_DLY_V);      // default: 14
		__raw_writel((160 + 8), pReg + OSD1_DLY_H); // default: 160
		__raw_writel((14), pReg + OSD1_DLY_V);      // default: 13

		if ((w == 720) && (h == 480)) {
			val = (__raw_readl(pReg + TX_INV) & ~(TX_INV_HS_MASK));
			val |= (0x1 << TX_INV_HS_SHIFT);
			__raw_writel(val, pReg + TX_INV);
		}

		if ((DV_PATH_DIRECT & vioc_get_path_type()) && (vioc_v_dv_get_mode() != DV_LL_RGB)) {
			// NexGuard - line swap!!
			pReg = VIOC_DNG_GetAddress();
			__raw_writel(0x30000000, pReg + 0x34);
		}
	}
}

volatile void __iomem *VIOC_DNG_GetAddress(void)
{
	if (pDNG_reg == NULL)
		pr_err("[ERR][V_DV] %s \n", __func__);

	return pDNG_reg;
}

volatile void __iomem *VIOC_DV_GetAddress(DV_DISP_TYPE type)
{
	if (pVDV_reg[type] == NULL)
		pr_err("[ERR][V_DV] %s \n", __func__);

	return pVDV_reg[type];
}

void VIOC_DV_DUMP(DV_DISP_TYPE type, unsigned int size)
{
	volatile void __iomem *pReg;
	unsigned int cnt = 0;

	if (!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	dprintk("%s-%d\n", __func__, __LINE__);

	pReg = VIOC_DV_GetAddress(type);

	pr_debug("[DBG][V_DV] DISP_DV :: 0x%p \n", pReg);
	while (cnt < size) {
		pr_debug("0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
}

volatile void __iomem *VIOC_DV_VEDR_GetAddress(VEDR_TYPE type)
{
	if (pVEDR_reg[type] == NULL)
		pr_err("[ERR][V_DV] %s \n", __func__);

	return pVEDR_reg[type];
}

void VIOC_DV_VEDR_DUMP(VEDR_TYPE type, unsigned int size)
{
	volatile void __iomem *pReg;
	unsigned int cnt = 0;

	if (pVEDR_np == NULL)
		return;

	if (!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	pReg = VIOC_DV_VEDR_GetAddress(type);
	pr_debug("[DBG][V_DV] VEDR :: 0x%p \n", pReg);
	while (cnt < size) {
		pr_debug("0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
}

static int __init ddi_nexguard_init(void)
{
	pDNG_np = of_find_compatible_node(NULL, NULL, "telechips,ddi_nexguard");
	if (pDNG_np == NULL)
		pr_err("[ERR][V_DV] can not find ddi nextguard \n");

	pDNG_reg = (volatile void __iomem *)of_iomap(pDNG_np, 0);
	if (pDNG_reg)
		pr_info("[INF][V_DV] %s DDI NexGuard: %p \n", __func__, pDNG_reg);

	return 0;
}
arch_initcall(ddi_nexguard_init);

static int __init vioc_disp_dv_init(void)
{
	int type;

	pVDV_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_disp_dv");
	if (pVDV_np == NULL)
		pr_err("[ERR][V_DV] can not find vioc DISP_DV \n");

	for (type = 0; type < EDR_MAX; type++) {
		pVDV_reg[type] =
			(volatile void __iomem *)of_iomap(pVDV_np, type);
		if (pVDV_reg[type])
			pr_info("[INF][V_DV] %s disp_dv: 0x%p\n", __func__, pVDV_reg[type]);
	}

	return 0;
}
arch_initcall(vioc_disp_dv_init);

static int __init vioc_vedr_init(void)
{
	int type;

	pVEDR_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_vedr");
	if (pVEDR_np == NULL) {
		pr_info("vioc-vedr: disabled\n");
	} else {
		for (type = 0; type < VEDR_MAX; type++) {
			pVEDR_reg[type] = (volatile void __iomem *)of_iomap(pVEDR_np, type);

			if (pVEDR_reg[type])
				pr_info("vioc-vedr%d: 0x%p\n", type, pVEDR_reg[type]);
		}

#if defined(CONFIG_TCC_DV_IN)
#ifdef DV_CLK_CTRL
		peri_lcd0_clk = of_clk_get(pVEDR_np, 0);
		if (IS_ERR(peri_lcd0_clk)){
			printk("%s-%d Error:: get clock 1 \n", __func__, __LINE__);
			peri_lcd0_clk = NULL;
			return -1;
		}
#endif
#endif
	}

	return 0;
}
arch_initcall(vioc_vedr_init);
#endif
