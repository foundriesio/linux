/*
 * linux/drivers/video/fbdev/tcc-fb/vioc/vioc_lvds.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC LVDS 803X h/w block
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_lvds.h>

#define LVDS_PHY_VCO_RANGE_MIN		(560000000)			// 560Mhz
#define LVDS_PHY_VCO_RANGE_MAX		(1120000000)		// 1120Mhz
#define LVDS_PHY_UPSAMPLE_RATIO_MAX		(0x4)			// 0~4

/* for upsample ratio calculation
 * n = upsample ratio
 * X = 2^n (X = 1, 2, 4, 8 16)
 */
static unsigned int ref_ratio_arr[5][2] = {
	{ 0, 1},
	{ 1, 2},
	{ 2, 4},
	{ 3, 8},
	{ 4, 16}
};
static volatile void __iomem *pLVDS_reg[LVDS_PHY_PORT_MAX];

/* VIOC_LVDS_PHY_GetUpsampleRatio
 * Get upsample ratio value for Automatic FCON
 * p_port : the primary port number of lvds phy
 * s_port : the secondary port number of lvds phy
 * freq : lvds pixel clock
 */
int VIOC_LVDS_PHY_GetUpsampleRatio(unsigned int p_port, unsigned int s_port, unsigned int freq)
{
	volatile void __iomem *p_reg = VIOC_LVDS_PHY_GetAddress(p_port);
	volatile void __iomem *s_reg = VIOC_LVDS_PHY_GetAddress(s_port);
	int i = -1;
	if(p_reg) {
		unsigned int pxclk;
		if(s_reg)
			pxclk = freq/2;
		else
			pxclk = freq;

		for(i = 0; i < (sizeof(ref_ratio_arr) / sizeof(ref_ratio_arr[0])); i++) {
			if(((pxclk * 7) * ref_ratio_arr[i][1]) > LVDS_PHY_VCO_RANGE_MIN)
				goto end_func;
		}

		if(i < (sizeof(ref_ratio_arr) / sizeof(ref_ratio_arr[0]))) {
			pr_err("error in %s: can not get upsample ratio (%dMhz)\n",
				__func__, (pxclk/1000000));
			i = -1;
		}
	} else {
		pr_err("error in %s can not get hw address \n",
			__func__);
	}
end_func:
	return i;
}

/* VIOC_LVDS_PHY_GetRefCnt
 * Get Reference count value for Automatic FCON
 * p_port : the primary port number of lvds phy
 * s_port : the secondary port number of lvds phy
 * freq : lvds pixel clock
 * upsample_ratio : upsample_ratio
 * Formula : REF_CNT = ((pixel clk * 7 * (2^upsample_ratio))/20)/24*16*16
 */
unsigned int VIOC_LVDS_PHY_GetRefCnt(unsigned int p_port, unsigned int s_port, unsigned int freq, int upsample_ratio)
{
	volatile void __iomem *p_reg = VIOC_LVDS_PHY_GetAddress(p_port);
	volatile void __iomem *s_reg = VIOC_LVDS_PHY_GetAddress(s_port);

	if(p_reg) {
		unsigned int pxclk;
		if(s_reg)
			pxclk = freq/2;
		else
			pxclk = freq;

		if((upsample_ratio > LVDS_PHY_UPSAMPLE_RATIO_MAX) || (upsample_ratio < 0)) {
			pr_err("error in %s: invaild parameter (pxclk:%dMhz / upsample_ratio:%d)\n",
					__func__, (pxclk/1000000), upsample_ratio);
			return 0;
		}

		return (((pxclk * 7 * ref_ratio_arr[upsample_ratio][1])/20)/24*16*16)/1000000;
	} else {
		pr_err("error in %s can not get hw address \n", __func__);
	}

	return 0;
}

/* VIOC_LVDS_PHY_SetFormat
 * Set LVDS phy format information
 * port : the port number of lvds phy
 * balance : balanced mode enable (0-disable, 1-enable)
 * depth : color depth(0-6bit, 1-8bit)
 * format : 0-VESA, 1-JEIDA
 */
void VIOC_LVDS_PHY_SetFormat(unsigned int port, unsigned int balance,
			     unsigned int depth, unsigned int format,
				 unsigned int freq)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if (reg) {
		unsigned int value;
		value = (__raw_readl(reg + LVDS_FORMAT) &
			 ~(LVDS_FORMAT_BALANCED_EN_MASK |
			   LVDS_FORMAT_COLOR_DEPTH_MASK |
			   LVDS_FORMAT_COLOR_FORMAT_MASK |
			   LVDS_FORMAT_UPSAMPLE_RATIO_MASK));
		value |= (((balance & 0x1) << LVDS_FORMAT_BALANCED_EN_SHIFT) |
			  ((depth & 0x1) << LVDS_FORMAT_COLOR_DEPTH_SHIFT) |
			  ((format & 0x1) << LVDS_FORMAT_COLOR_FORMAT_SHIFT) |
			  ((freq & 0x7) << LVDS_FORMAT_UPSAMPLE_RATIO_SHIFT));
		__raw_writel(value, reg + LVDS_FORMAT);
	}
}

/* VIOC_LVDS_PHY_SetUserMode
 * Control lane skew and p/n swap
 * port : the port number of lvds phy
 * lane : lane type
 * skew : lane skew value
 * swap : lane p/n swap (0-Normal, 1-Swap p/n)
 */
void VIOC_LVDS_PHY_SetUserMode(unsigned int port, unsigned int lane,
			       unsigned int skew, unsigned int swap)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if (reg) {
		unsigned int value;
		switch (lane) {
		case LVDS_PHY_CLK_LANE:
			value = (__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0) &
				 ~(LVDS_USER_MODE_PHY_IF_SET0_CLK_PN_SWAP_MASK |
				   LVDS_USER_MODE_PHY_IF_SET0_CLK_LANE_SKEW_MASK));
			value |=
				(((skew & 0x7)
				  << LVDS_USER_MODE_PHY_IF_SET0_CLK_LANE_SKEW_SHIFT) |
				 ((swap & 0x1)
				  << LVDS_USER_MODE_PHY_IF_SET0_CLK_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case LVDS_PHY_DATA0_LANE:
			value = (__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0) &
				 ~(LVDS_USER_MODE_PHY_IF_SET0_DATA0_PN_SWAP_MASK |
				   LVDS_USER_MODE_PHY_IF_SET0_DATA0_LANE_SKEW_MASK));
			value |=
				(((skew & 0x7)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA0_LANE_SKEW_SHIFT) |
				 ((swap & 0x1)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA0_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case LVDS_PHY_DATA1_LANE:
			value = (__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0) &
				 ~(LVDS_USER_MODE_PHY_IF_SET0_DATA1_PN_SWAP_MASK |
				   LVDS_USER_MODE_PHY_IF_SET0_DATA1_LANE_SKEW_MASK));
			value |=
				(((skew & 0x7)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA1_LANE_SKEW_SHIFT) |
				 ((swap & 0x1)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA1_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case LVDS_PHY_DATA2_LANE:
			value = (__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0) &
				 ~(LVDS_USER_MODE_PHY_IF_SET0_DATA2_PN_SWAP_MASK |
				   LVDS_USER_MODE_PHY_IF_SET0_DATA2_LANE_SKEW_MASK));
			value |=
				(((skew & 0x7)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA2_LANE_SKEW_SHIFT) |
				 ((swap & 0x1)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA2_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case LVDS_PHY_DATA3_LANE:
			value = (__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0) &
				 ~(LVDS_USER_MODE_PHY_IF_SET0_DATA3_PN_SWAP_MASK |
				   LVDS_USER_MODE_PHY_IF_SET0_DATA3_LANE_SKEW_MASK));
			value |=
				(((skew & 0x7)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA3_LANE_SKEW_SHIFT) |
				 ((swap & 0x1)
				  << LVDS_USER_MODE_PHY_IF_SET0_DATA3_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case LVDS_PHY_LANE_MAX:
		default:
			pr_err("%s in error, invaild parameter(lane:%d)\n",
			       __func__, lane);
			break;
		}
	}
}

/* VIOC_LVDS_PHY_SetLaneSwap
 * Swap the data lanes
 * port : the port number of lvds phy
 * lane : the lane number that will be changed
 * select : the lane number that want to swap
 */
void VIOC_LVDS_PHY_SetLaneSwap(unsigned int port, unsigned int lane, unsigned int select)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);
	unsigned int value;
	if(reg) {
		switch(lane) {
		case LVDS_PHY_CLK_LANE:
		case LVDS_PHY_DATA0_LANE:
		case LVDS_PHY_DATA1_LANE:
		case LVDS_PHY_DATA2_LANE:
		case LVDS_PHY_DATA3_LANE:
			break;
		case LVDS_PHY_LANE_MAX:
		default:
			pr_err("%s in error, invaild parameter(lane:%d)\n",
			       __func__, lane);
			break;
		}
		value = (__raw_readl(reg+LVDS_USER_MODE_PHY_IF_SET1) &
			~(0x7 << (LVDS_USER_MODE_PHY_IF_SET1_SET_LANE0_SHIFT + (lane * 0x4))));
		value |= ((select & 0x7) << (LVDS_USER_MODE_PHY_IF_SET1_SET_LANE0_SHIFT + (lane*0x4)));
		__raw_writel(value, reg+LVDS_USER_MODE_PHY_IF_SET1);
	}
}

/* VIOC_LVDS_PHY_SetFifoEnableTiming
 * Select FIFO2 enable timing
 * port : the port number of lvds phy
 * cycle : FIFO enable after n clock cycle (0~3 cycle)
 */
void VIOC_LVDS_PHY_SetFifoEnableTiming(unsigned int port, unsigned int cycle)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if (reg) {
		unsigned int value;
		value = (__raw_readl(reg + LVDS_STARTUP_MODE) &
			 ~(LVDS_STARTUP_MODE_FIFO2_RD_EN_TIMING_MASK));
		value |= ((cycle & 0x3)
			  << LVDS_STARTUP_MODE_FIFO2_RD_EN_TIMING_SHIFT);
		__raw_writel(value, reg + LVDS_STARTUP_MODE);
	}
}

/* VIOC_LVDS_PHY_SetPortOption
 * Selects a port option for dual pixel mode
 * port : the port number of lvds phy
 * port_mode : the mode of this port(0-main port, 1-sub port)
 * sync_swap : swap vsync/hsync position (0-do not swap, 1-swap)
 * use_other_port : 0-normal, 1-use sync from other port
 * lane_en : lane enable (CLK, DATA0~3)
 * sync_transmit_src : sync transmit source (0-normal, 1-other port)
 */
void VIOC_LVDS_PHY_SetPortOption(unsigned int port, unsigned int port_mode,
				 unsigned int sync_swap,
				 unsigned int use_other_port,
				 unsigned int lane_en,
				 unsigned int sync_transmit_src)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if (reg) {
		unsigned int value;
		value = (__raw_readl(reg + LVDS_PORT) &
			 ~(LVDS_PORT_SET_SECONDARY_PORT_MASK |
			   LVDS_PORT_VSYNC_HSYNC_SWAP_MASK |
			   LVDS_PORT_USE_SYNC_FROM_OP_MASK |
			   LVDS_PORT_LANE_EN_MASK |
			   LVDS_PORT_SYNC_TRANSMITTED_MASK));

		value |= (((port_mode & 0x1)
			   << LVDS_PORT_SET_SECONDARY_PORT_SHIFT) |
			  ((sync_swap & 0x1)
			   << LVDS_PORT_VSYNC_HSYNC_SWAP_SHIFT) |
			  ((use_other_port & 0x1)
			   << LVDS_PORT_USE_SYNC_FROM_OP_SHIFT) |
			  ((lane_en & 0x1F) << LVDS_PORT_LANE_EN_SHIFT) |
			  ((sync_transmit_src & 0x7)
			   << LVDS_PORT_SYNC_TRANSMITTED_SHIFT));
		__raw_writel(value, reg + LVDS_PORT);
	}
}

/* VIOC_LVDS_PHY_FifoEnable
 * Set lvds phy fifo enable
 * port : the port number of lvds phy
 */
void VIOC_LVDS_PHY_FifoEnable(unsigned int port, unsigned int enable)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if (reg) {
		unsigned int value;
		value = (__raw_readl(reg + LVDS_EN) &
			 ~(LVDS_EN_FIFO2_EN_MASK | LVDS_EN_FIFO1_EN_MASK |
			   LVDS_EN_FIFO0_EN_MASK | LVDS_EN_DATA_EN_MASK));

		if (enable) {
			value |= ((0x1 << LVDS_EN_FIFO2_EN_SHIFT) |
				  (0x1 << LVDS_EN_FIFO1_EN_SHIFT) |
				  (0x1 << LVDS_EN_FIFO0_EN_SHIFT) |
				  (0x1 << LVDS_EN_DATA_EN_SHIFT));
		}
		__raw_writel(value, reg + LVDS_EN);
	}
}

/* VIOC_LVDS_PHY_SWReset
 * Control lvds phy swreset
 * port : the port number of lvds phy
 * reset : 0-release, 1-reset
 */
void VIOC_LVDS_PHY_SWReset(unsigned int port, unsigned int reset)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if (reg) {
		if (reset)
			__raw_writel(0x00000000, reg + LVDS_RESETB);
		else
			__raw_writel(0x0001FFFF, reg + LVDS_RESETB);
	}
}

/* VIOC_LVDS_PHY_ClockEnable
 * Control lvds phy clock enable
 * port : the port number of lvds phy
 * enable : 0-disable, 1-enable
 */
void VIOC_LVDS_PHY_ClockEnable(unsigned int port, unsigned int enable)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if (reg) {
		if (enable)
			__raw_writel(0x000100FF, reg + LVDS_CLK_SET);
		else
			__raw_writel(0x00000000, reg + LVDS_CLK_SET);
	}
}

/* VIOC_LVDS_PHY_SetStrobe
 * Sets LVDS strobe registers
 * port : the port number of lvds phy
 * mode : 0-Manual, 1- Auto
 * enable : 0-disable, 1-enable
 */
void VIOC_LVDS_PHY_SetStrobe(unsigned int port, unsigned int mode, unsigned int enable)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);

	if(reg) {
		unsigned int value;
		value = (__raw_readl(reg+LVDS_STB_EN) & ~(LVDS_STB_EN_STB_EN_MASK));
		value |= ((enable & 0x1) << LVDS_STB_EN_STB_EN_SHIFT);
		__raw_writel(value, reg+LVDS_STB_EN);

		value = (__raw_readl(reg+LVDS_AUTO_STB_SET) & ~(LVDS_AUTO_STB_SET_STB_AUTO_EN_MASK));
		value |= ((mode & 0x1) << LVDS_AUTO_STB_SET_STB_AUTO_EN_SHIFT);
		__raw_writel(value, reg+LVDS_AUTO_STB_SET);
	}
}

/* VIOC_LVDS_PHY_SetFcon
 * Setup LVDS PHY Manual/Automatic Coarse tunning
 * port : the port number of lvds phy
 * mode : coarse tunning method 0-manual, 1-automatic
 * loop : feedback loop mode 0-closed loop, 1-open loop
 * fcon : frequency control value
 */
void VIOC_LVDS_PHY_SetFcon(unsigned int port, unsigned int mode, unsigned int loop, unsigned int division, unsigned int fcon)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);
	unsigned int value;

	if(reg) {
		unsigned int target_th = 0x2020;		/* Alphachips Guide for FCON */
		switch(mode) {
		case LVDS_PHY_FCON_MANUAL:
			value = (__raw_readl(reg+LVDS_CTSET1) & ~(LVDS_CTSET1_MPLL_CTLCK_MASK|
						LVDS_CTSET1_MPLL_DIVN_MASK| LVDS_CTSET1_MPLL_FCON_MASK));
			value |= (((loop & 0x1) << LVDS_CTSET1_MPLL_CTLCK_SHIFT) |
					((division & 0x3) << LVDS_CTSET1_MPLL_DIVN_SHIFT) |
					((fcon & 0x3FF) << LVDS_CTSET1_MPLL_FCON_SHIFT));
			__raw_writel(value, reg+LVDS_CTSET1);         // offset: 0x094

			value = (__raw_readl(reg+LVDS_FCOPT) & ~(LVDS_FCOPT_CLK_DET_SEL_MASK |
						LVDS_FCOPT_CT_SEL_MASK));
			value |= ((mode & 0x1) << LVDS_FCOPT_CT_SEL_SHIFT);
			__raw_writel(value, reg+LVDS_FCOPT);          // offset: 0x09C
			break;
		case LVDS_PHY_FCON_AUTOMATIC:
			value = (__raw_readl(reg+LVDS_FCCNTR1) & ~(LVDS_FCCNTR1_CONTIN_TARGET_TH_MASK |
						LVDS_FCCNTR1_REF_CNT_MASK));
			value |= (((target_th & 0xFFFF) << LVDS_FCCNTR1_CONTIN_TARGET_TH_SHIFT) |
					(fcon & 0xFFFF) << LVDS_FCCNTR1_REF_CNT_SHIFT);
			__raw_writel(value, reg+LVDS_FCCNTR1);                // offset: 0x0B0

			value = (__raw_readl(reg+LVDS_CTSET1) & ~(LVDS_CTSET1_MPLL_CTLCK_MASK|
						LVDS_CTSET1_MPLL_DIVN_MASK| LVDS_CTSET1_MPLL_FCON_MASK));
			value |= (((loop & 0x1) << LVDS_CTSET1_MPLL_CTLCK_SHIFT) |		/* Shoulb be set to 'Open' loop */
					((division & 0x3) << LVDS_CTSET1_MPLL_DIVN_SHIFT));
			__raw_writel(value, reg+LVDS_CTSET1);         // offset: 0x094

			value = (__raw_readl(reg+LVDS_CTSET0) & ~(LVDS_CTSET0_ENABLE_MASK |
						LVDS_CTSET0_RUN_MASK));
			value |= (0x1 << LVDS_CTSET0_ENABLE_SHIFT);
			__raw_writel(value, reg+LVDS_CTSET0);         // offset: 0x090

			value = (__raw_readl(reg+LVDS_FCOPT) & ~(LVDS_FCOPT_CLK_DET_SEL_MASK |
						LVDS_FCOPT_CT_SEL_MASK));
			value |= (((mode & 0x1) << LVDS_FCOPT_CT_SEL_SHIFT) |
					(0x1 << LVDS_FCOPT_CLK_DET_SEL_SHIFT));
			__raw_writel(value, reg+LVDS_FCOPT);          // offset: 0x09C

			__raw_writel(0x10101010, reg+LVDS_FCCNTR0);           /* Alphachips Guide for FCON */         // offset: 0x098
			break;
		case LVDS_PHY_FCON_MAX:
		default:
			pr_err("%s in error, invaild parameter(mode: %d)\n",
					__func__, mode);
			break;
		}
	}
}

/* VIOC_LVDS_PHY_SetCFcon
 * Check fcon status and setup cfcon value
 * port : the port number of lvds phy
 * mode : fcon control mode 0-Manual, 1-Automatic
 * enable : 0-cfcon disable 1-cfcon enable
 */
void VIOC_LVDS_PHY_SetCFcon(unsigned int port, unsigned int mode, unsigned int enable)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);
	unsigned int value;

	if(reg) {
		if(mode == LVDS_PHY_FCON_AUTOMATIC) {
			unsigned int pd_fcstat, pd_fccntval1, pd_fcresval;

			pd_fcstat = (__raw_readl(reg+LVDS_FCSTAT) & (LVDS_FCST_CLK_OK_MASK|LVDS_FCST_DONE_MASK|LVDS_FCST_ERROR_MASK));
			pd_fccntval1 = __raw_readl(reg+LVDS_FCCNTVAL1);
			pd_fcresval = __raw_readl(reg+LVDS_FCRESEVAL);
			if(pd_fcstat == (LVDS_FCST_CLK_OK_MASK|LVDS_FCST_DONE_MASK))
			{
				if(enable) {
					/* Alphachips Guide For CFCON */
					__raw_writel(0x0000002C, reg+LVDS_FCCONTINSET0);              // offset: 0x0B4
					__raw_writel(0x00000960, reg+LVDS_FCCONTINSET1);              // offset: 0x0B8
					__raw_writel(0x003FF005, reg+LVDS_FCCONTINSET2);              // offset: 0x0BC
					__raw_writel(0x0000002D, reg+LVDS_FCCONTINSET0);              // offset: 0x0B4
				}
			} else {
				pr_err("%s in error, Primay Port FCON is failed (stat: 0x%08x)\n",
						__func__, pd_fcstat);
			}
		}

		/* Change loop mode to 'Closed' loop */
		value = (__raw_readl(reg+LVDS_CTSET1) & ~(LVDS_CTSET1_MPLL_CTLCK_MASK));
		value |= (0x1 << LVDS_CTSET1_MPLL_CTLCK_SHIFT);
		__raw_writel(value, reg+LVDS_CTSET1);         // offset: 0x094
	}
}

/* VIOC_LVDS_PHY_FConEnable
 * Controls FCON running enable
 * port : the port number of lvds phy
 * enable : 0-disable, 1-enable
 */
void VIOC_LVDS_PHY_FConEnable(unsigned int port, unsigned int enable)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);
	unsigned int value;

	if(reg) {
		value = (__raw_readl(reg+LVDS_CTSET0) & ~(LVDS_CTSET0_ENABLE_MASK |
					LVDS_CTSET0_RUN_MASK));
		value |= (((enable & 0x1) << LVDS_CTSET0_ENABLE_SHIFT) |
				((enable & 0x1) << LVDS_CTSET0_RUN_SHIFT));
		__raw_writel(value, reg+LVDS_CTSET0);
	}
}

/* VIOC_LVDS_PHY_Config
 * Setup LVDS PHY Strobe. (Alphachips Guide Value Only)
 * p_port : the primary port number
 * s_port : the secondary port number
 * upsample_ratio : division ratio (0: fcon automatic, 0~4: fcon manual)
 * step : the step of lvds phy configure
 */
void VIOC_LVDS_PHY_StrobeConfig(unsigned int p_port, unsigned int s_port, unsigned int upsample_ratio, unsigned int step)
{
	volatile void __iomem *p_reg = VIOC_LVDS_PHY_GetAddress(p_port);
	volatile void __iomem *s_reg = VIOC_LVDS_PHY_GetAddress(s_port);
	unsigned int value;

	if(p_reg) {
		switch(step) {
		case LVDS_PHY_INIT:
			value = (upsample_ratio & 0x7);

			__raw_writel(0x000000ff, p_reg+0x380);		// power on
			__raw_writel(0x0000001f, p_reg+0x384);		// power on
			__raw_writel(0x00000001, p_reg+0x3B4);		// power on
			__raw_writel(value, p_reg+0x3A4);		// division ratio		--- upsample_ratio b[0]
			__raw_writel(value, p_reg+0x3AC);		// division ratio		--- upsample_ratio b[0]

			if(s_reg) {
				__raw_writel(0x000000ff, s_reg+0x380);		// power on
				__raw_writel(0x0000001f, s_reg+0x384);		// power on
				__raw_writel(0x00000001, s_reg+0x3B4);		// power on
				__raw_writel(value, s_reg+0x3A4);		// division ratio		-- upsample_ratio b[0]
				__raw_writel(value, s_reg+0x3AC);		// division ratio		-- upsample_ratio b[0]
			}

			__raw_writel(0x00000001, p_reg+0x3B4);		// input clock ready

			__raw_writel(0x00000032, p_reg+0x388);
			__raw_writel(0x0000000f, p_reg+0x38C);
			__raw_writel(0x00000002, p_reg+0x390);
			__raw_writel(0x00000013, p_reg+0x394);
			__raw_writel(0x00000029, p_reg+0x398);
			__raw_writel(0x00000025, p_reg+0x39C);
			__raw_writel(0x000000F4, p_reg+0x3A0);
			__raw_writel(0x0000000C, p_reg+0x3A8);
			__raw_writel(0x00000000, p_reg+0x3B0);

			if(s_reg) {
				__raw_writel(0x00000032, s_reg+0x388);
				__raw_writel(0x0000000f, s_reg+0x38C);
				__raw_writel(0x00000002, s_reg+0x390);
				__raw_writel(0x00000013, s_reg+0x394);
				__raw_writel(0x00000029, s_reg+0x398);
				__raw_writel(0x00000025, s_reg+0x39C);
				__raw_writel(0x000000F4, s_reg+0x3A0);
				__raw_writel(0x0000000C, s_reg+0x3A8);
				__raw_writel(0x00000000, s_reg+0x3B0);
			}
			break;
		case LVDS_PHY_READY:
			__raw_writel(0x00000001, p_reg+0x3B8);
			if(s_reg)
				__raw_writel(0x00000001, s_reg+0x3B8);

			__raw_writel(0x00000079, p_reg+0x200);		// STB_LANE0 ADDR 0x0
			__raw_writel(0x0000002F, p_reg+0x204);		// STB_LANE0 ADDR 0x0
			__raw_writel(0x00000079, p_reg+0x240);		// STB_LANE1 ADDR 0x0
			__raw_writel(0x0000002F, p_reg+0x244);		// STB_LANE0 ADDR 0x0
			__raw_writel(0x00000079, p_reg+0x280);		// STB_LANE2 ADDR 0x0
			__raw_writel(0x0000002F, p_reg+0x284);		// STB_LANE0 ADDR 0x0
			__raw_writel(0x00000079, p_reg+0x2C0);		// STB_LANE3 ADDR 0x0
			__raw_writel(0x0000002F, p_reg+0x2C4);		// STB_LANE0 ADDR 0x0
			__raw_writel(0x00000079, p_reg+0x300);		// STB_LANE4 ADDR 0x0
			__raw_writel(0x0000002F, p_reg+0x304);		// STB_LANE0 ADDR 0x0

			if(s_reg) {
				__raw_writel(0x00000079, s_reg+0x200);		// STB_LANE0 ADDR 0x0
				__raw_writel(0x0000002F, s_reg+0x204);		// STB_LANE0 ADDR 0x0
				__raw_writel(0x00000079, s_reg+0x240);		// STB_LANE1 ADDR 0x0
				__raw_writel(0x0000002F, s_reg+0x244);		// STB_LANE0 ADDR 0x0
				__raw_writel(0x00000079, s_reg+0x280);		// STB_LANE2 ADDR 0x0
				__raw_writel(0x0000002F, s_reg+0x284);		// STB_LANE0 ADDR 0x0
				__raw_writel(0x00000079, s_reg+0x2C0);		// STB_LANE3 ADDR 0x0
				__raw_writel(0x0000002F, s_reg+0x2C4);		// STB_LANE0 ADDR 0x0
				__raw_writel(0x00000079, s_reg+0x300);		// STB_LANE4 ADDR 0x0
				__raw_writel(0x0000002F, s_reg+0x304);		// STB_LANE0 ADDR 0x0
			}

			if(s_reg) {
				__raw_writel(0x000000C7, p_reg+0x3B4);		// ready
				__raw_writel(0x0000002C, s_reg+0x3B4);		// ready
				__raw_writel(0x0000003C, s_reg+0x3B4);		// ready
			} else {
				__raw_writel(0x00000001, p_reg+0x3B4);		// ready
			}
			break;
		case LVDS_PHY_START:
			if(s_reg)
				__raw_writel(0x000000D7, p_reg+0x3B4);		// start
			else
				__raw_writel(0x00000011, p_reg+0x3B4);		// start
			break;
		case LVDS_PHY_CONFIG_MAX:
		default:
			pr_err("%s in error, invaild parameter(step: %d)\n",
					__func__, step);
			break;
		}
	}
}

/* VIOC_LVDS_PHY_CheckStatus
 * Check the status of lvds phy
 * p_port : the primary port number of lvds phy
 * s_port : the secondary port number of lvds phy
 */
unsigned int VIOC_LVDS_PHY_CheckStatus(unsigned int p_port, unsigned int s_port)
{
	volatile void __iomem *p_reg = VIOC_LVDS_PHY_GetAddress(p_port);
	volatile void __iomem *s_reg = VIOC_LVDS_PHY_GetAddress(s_port);
	unsigned int ret = 0, value;

	if(p_reg) {
		value = (readl(p_reg+LVDS_MONITOR_DEBUG1) &
				(LVDS_MONITOR_DEBUG1_PLL_STATUS_MASK));
		if(value & (LVDS_MONITOR_DEBUG1_LKVDETLOW_MASK |
					LVDS_MONITOR_DEBUG1_LKVDETHIGH_MASK))
			ret |= 0x1;		/* primary port status b[0]*/
	}

	if(s_reg) {
		value = (readl(s_reg+LVDS_MONITOR_DEBUG1) &
				(LVDS_MONITOR_DEBUG1_PLL_STATUS_MASK));
		if(value & (LVDS_MONITOR_DEBUG1_LKVDETLOW_MASK |
					LVDS_MONITOR_DEBUG1_LKVDETHIGH_MASK))
			ret |= 0x2;		/* secondary port status b[1] */
	}
	return ret;
}

/* VIOC_LVDS_PHY_GetRegValue
 * Read the register corresponding to 'offset'
 * port : the port number of lvds phy
 * offset : the register offset
 */
unsigned int VIOC_LVDS_PHY_GetRegValue(unsigned int port, unsigned int offset)
{
	volatile void __iomem *reg = VIOC_LVDS_PHY_GetAddress(port);
	unsigned int ret;

	ret = __raw_readl(reg+offset);
	if(offset > 0x1FF) {	/* Write Only Registers */
		while(1) {
			if(__raw_readl(reg+LVDS_AUTO_STB_DONE) & 0x1)
				break;
		}
		ret = __raw_readl(reg+LVDS_AUTO_STB_RDATA);
	}
	return ret;
}

volatile void __iomem *VIOC_LVDS_PHY_GetAddress(unsigned int port)
{
	if (port >= LVDS_PHY_PORT_MAX)
		pr_err("%s: wrong parameter - %d\n", __func__, port);

	if (pLVDS_reg[port] == NULL)
		pr_err("%s: pLVDS_reg:%p \n", __func__, pLVDS_reg[port]);

	return pLVDS_reg[port];
}

static int __init vioc_lvds_init(void)
{
	struct device_node *ViocLVDS_np;
	int i;

	ViocLVDS_np = of_find_compatible_node(NULL, NULL, "telechips,lvds_phy");
	if (ViocLVDS_np == NULL) {
		pr_info("vioc-lvdsphy: disabled\n");
	} else {
		for (i = 0; i < LVDS_PHY_PORT_MAX; i++) {
			pLVDS_reg[i] = (volatile void __iomem *)of_iomap(ViocLVDS_np, i);

			if (pLVDS_reg[i])
				pr_info("vioc-lvdsphy%d: 0x%p\n", i, pLVDS_reg[i]);
		}
	}
	return 0;
}
arch_initcall(vioc_lvds_init);
