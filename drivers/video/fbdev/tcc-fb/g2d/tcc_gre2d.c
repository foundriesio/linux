/*
 * Copyright (C) 2008-2019 Telechips Inc.
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

#include <video/tcc/tcc_gre2d.h>

struct device_node *pGre2D_np;
volatile void __iomem *pGre2D_reg = NULL;

extern volatile void __iomem* GRE_2D_GetAddress(void);

void GRE_2D_SetInterrupt(char onoff)
{
	#if 0
	PPIC pHwPIC;
	pHwPIC  = (volatile PPIC)tcc_p2v(HwPIC_BASE);

	if(onoff)
	{
		BITSET(pHwPIC->CLR1,  HwINT1_G2D);
		BITCLR(pHwPIC->POL1,  HwINT1_G2D);
		BITSET(pHwPIC->SEL1,  HwINT1_G2D);
		BITSET(pHwPIC->IEN1,  HwINT1_G2D);
		BITSET(pHwPIC->MODE1,  HwINT1_G2D);
	}
	else
	{
		BITCLR(pHwPIC->IEN1,  HwINT1_G2D);
	}
	#endif
}

/*------------------------------------------------------------------
GRE_2D_SetFChAddress
 graphic engine Front End channel address 0,1,2 setting
-------------------------------------------------------------------*/
void GRE_2D_SetFChAddress(G2D_CHANNEL ch, unsigned int add0, unsigned int add1, unsigned int add2)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();

	switch(ch)
    {
        case FCH0_CH:
			//BITCSET(pHwOVERLAYMIXER->FCH0_SADDR0.nREG, 0xFFFFFFFF, add0);
			//BITCSET(pHwOVERLAYMIXER->FCH0_SADDR1.nREG, 0xFFFFFFFF, add1);
			//BITCSET(pHwOVERLAYMIXER->FCH0_SADDR2.nREG, 0xFFFFFFFF, add2);
			__raw_writel(add0, reg + 0x00);
			__raw_writel(add1, reg + 0x04);
			__raw_writel(add2, reg + 0x08);
			break;

        case FCH1_CH:
			//BITCSET(pHwOVERLAYMIXER->FCH1_SADDR0.nREG, 0xFFFFFFFF, add0);
			//BITCSET(pHwOVERLAYMIXER->FCH1_SADDR1.nREG, 0xFFFFFFFF, add1);
			//BITCSET(pHwOVERLAYMIXER->FCH1_SADDR2.nREG, 0xFFFFFFFF, add2);
			__raw_writel(add0, reg + 0x20);
			__raw_writel(add1, reg + 0x24);
			__raw_writel(add2, reg + 0x28);
            break;

        default:
			break;
    }
}

/*------------------------------------------------------------------
GRE_2D_SetFChPosition
 graphic engine Front channel position settig

 frameps_x, frameps_y : frame pixsel size
 poffset_x, poffset_y : pixsel offset
 imageps_x, imageps_y : imagme pixel size
 winps_x, winps_y : window pixsel offset
-------------------------------------------------------------------*/
void GRE_2D_SetFChPosition(G2D_CHANNEL ch, unsigned int  frameps_x, unsigned int  frameps_y,
                                unsigned int  poffset_x, unsigned int  poffset_y, unsigned int  imageps_x, unsigned int  imageps_y, unsigned int  winps_x, unsigned int  winps_y )
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	switch(ch)
    {
        case FCH0_CH:
            //BITCSET(pHwOVERLAYMIXER->FCH0_SFSIZE.nREG,0x0FFF0FFF,((frameps_y<<16) | frameps_x)); // pHwOVERLAYMIXER->FCH0_SFSIZE
            //BITCSET(pHwOVERLAYMIXER->FCH0_SOFF.nREG,0x0FFF0FFF,((poffset_y<<16) | poffset_x));
            //BITCSET(pHwOVERLAYMIXER->FCH0_SISIZE.nREG,0x0FFF0FFF,((imageps_y<<16) | imageps_x));
            //BITCSET(pHwOVERLAYMIXER->FCH0_WOFF.nREG,0x0FFF0FFF,((winps_y<<16) | winps_x));

			value = (__raw_readl(reg + 0x0c) & ~(0x0FFF0FFF));
			value |= ((frameps_y<<16) | frameps_x);
			__raw_writel(value, reg + 0x0c);

			value = (__raw_readl(reg + 0x10) & ~(0x0FFF0FFF));
			value |= ((poffset_y<<16) | poffset_x);
			__raw_writel(value, reg + 0x10);

			value = (__raw_readl(reg + 0x14) & ~(0x0FFF0FFF));
			value |= ((imageps_y<<16) | imageps_x);
			__raw_writel(value, reg + 0x14);

			value = (__raw_readl(reg + 0x18) & ~(0x0FFF0FFF));
			value |= ((winps_y<<16) | winps_x);
			__raw_writel(value, reg + 0x18);
            break;

        case FCH1_CH:
            //BITCSET(pHwOVERLAYMIXER->FCH1_SFSIZE.nREG,0x0FFF0FFF,((frameps_y<<16) | frameps_x)); // pHwOVERLAYMIXER->FCH0_SFSIZE
            //BITCSET(pHwOVERLAYMIXER->FCH1_SOFF.nREG,0x0FFF0FFF,((poffset_y<<16) | poffset_x));
            //BITCSET(pHwOVERLAYMIXER->FCH1_SISIZE.nREG,0x0FFF0FFF,((imageps_y<<16) | imageps_x));
            //BITCSET(pHwOVERLAYMIXER->FCH1_WOFF.nREG,0x0FFF0FFF,((winps_y<<16) | winps_x));

			value = (__raw_readl(reg + 0x2c) & ~(0x0FFF0FFF));
			value |= ((frameps_y<<16) | frameps_x);
			__raw_writel(value, reg + 0x2c);

			value = (__raw_readl(reg + 0x30) & ~(0x0FFF0FFF));
			value |= ((poffset_y<<16) | poffset_x);
			__raw_writel(value, reg + 0x30);

			value = (__raw_readl(reg + 0x34) & ~(0x0FFF0FFF));
			value |= ((imageps_y<<16) | imageps_x);
			__raw_writel(value, reg + 0x34);

			value = (__raw_readl(reg + 0x38) & ~(0x0FFF0FFF));
			value |= ((winps_y<<16) | winps_x);
			__raw_writel(value, reg + 0x38);
            break;

        default:
            break;
    }
}

/*------------------------------------------------------------------
GRE_2D_SetFChControl
 graphic engine Front channel control setting
 mode : flip, rotate
 data_form : rgb, yuv, alpha-rgb
-------------------------------------------------------------------*/
void GRE_2D_SetFChControl(G2D_CHANNEL ch, G2D_MABC_TYPE MABC, unsigned char LUTE, unsigned char SSUV, G2D_OP_MODE mode, G2D_ZF_TYPE ZF, G2D_FMT_CTRL data_form)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	switch(ch)
	{
		case FCH0_CH:
			//BITCSET(pHwOVERLAYMIXER->FCH0_SCTRL.nREG, 0x0F00FFFF, (LUTE<<12)|(SSUV<<11)|((mode<<8)&HwGE_FCHO_OPMODE)
			//						    |(ZF<<5)| (data_form.format& HwGE_FCHO_SDFRM)
			//							|((data_form.data_swap<<24) & HwGE_FCH_SSB));

			value = (__raw_readl(reg + 0x1c) & ~(0x0F00FFFF));
			value |= ((LUTE<<12)|(SSUV<<11)|((mode<<8)&HwGE_FCHO_OPMODE)
									    |(ZF<<5)| (data_form.format& HwGE_FCHO_SDFRM)
										|((data_form.data_swap<<24) & HwGE_FCH_SSB));
			__raw_writel(value, reg + 0x1c);
			break;

		case FCH1_CH:
			//BITCSET(pHwOVERLAYMIXER->FCH1_SCTRL.nREG, 0x0F00FFFF,  (LUTE<<12)|(SSUV<<11)|((mode<<8)&HwGE_FCHO_OPMODE)
			//						    |(ZF<<5)| (data_form.format& HwGE_FCHO_SDFRM)
			//							|((data_form.data_swap<<24) & HwGE_FCH_SSB));

			value = (__raw_readl(reg + 0x3c) & ~(0x0F00FFFF));
			value |= ((LUTE<<12)|(SSUV<<11)|((mode<<8)&HwGE_FCHO_OPMODE)
									    |(ZF<<5)| (data_form.format& HwGE_FCHO_SDFRM)
										|((data_form.data_swap<<24) & HwGE_FCH_SSB));
			__raw_writel(value, reg + 0x3c);
			break;

		default:
			break;
	}

}

/*------------------------------------------------------------------
GRE_2D_SetFChChromaKey
 graphic engine Front channel chroma key Set
 -------------------------------------------------------------------*/
void GRE_2D_SetFChChromaKey(G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	switch(ch)
    {
        case FCH0_CH:
            //BITCSET(pHwOVERLAYMIXER->S0_CHROMA.nREG,0x00FFFFFF, (((RY<<16)&0xFF0000) | ((GU<<8)&0xFF00)| (BV&0xFF))); // pHwOVERLAYMIXER->S0_CHROMA
            value = (__raw_readl(reg + 0x80) & ~(0x00FFFFFF));
			value |= (((RY<<16)&0xFF0000) | ((GU<<8)&0xFF00)| (BV&0xFF));
			__raw_writel(value, reg + 0x80);
            break;

        case FCH1_CH:
            break;

        default:
                break;
    }
}

/*------------------------------------------------------------------
GRE_2D_SetFChArithmeticPar
 graphic engine Front channel Arithmetic parameter setting
-------------------------------------------------------------------*/
void GRE_2D_SetFChArithmeticPar(G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	switch(ch)
	{
	    case FCH0_CH:
            //BITCSET(pHwOVERLAYMIXER->S0_PAR.nREG,0x00FFFFFF, (((RY<<16)&0xFF0000) | ((GU<<8)&0xFF00)| (BV&0xFF))); // pHwOVERLAYMIXER->S0_PAR
            value = (__raw_readl(reg + 0x84) & ~(0x00FFFFFF));
			value |= (((RY<<16)&0xFF0000) | ((GU<<8)&0xFF00)| (BV&0xFF));
			__raw_writel(value, reg + 0x84);
            break;

	    case FCH1_CH:
            break;

	    default:
            break;
	}
}

/*------------------------------------------------------------------
GRE_2D_SetSrcCtrl
 graphic engine sorce control
-------------------------------------------------------------------*/
void GRE_2D_SetSrcCtrl(G2D_SRC_CTRL g2d_ctrl)
{
    unsigned int sf_ctrl_reg = 0, sa_ctrl_reg = 0;
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

// source YUV to RGB converter enable 	sf_ctrl
    sf_ctrl_reg |= (((g2d_ctrl.src0_y2r.src_y2r <<24) & Hw2D_SFCTRL_S0_Y2REN) |
                    ((g2d_ctrl.src1_y2r.src_y2r <<25) & Hw2D_SFCTRL_S1_Y2REN) | ((g2d_ctrl.src2_y2r.src_y2r <<26) & Hw2D_SFCTRL_S2_Y2REN));

// source YUV to RGB coverter type	sf_ctrl
    sf_ctrl_reg |= (((g2d_ctrl.src0_y2r.src_y2r_type<<16) & Hw2D_SFCTRL_S0_Y2RMODE) |
                    ((g2d_ctrl.src1_y2r.src_y2r_type <<18) & Hw2D_SFCTRL_S1_Y2RMODE) | ((g2d_ctrl.src2_y2r.src_y2r_type <<20) & Hw2D_SFCTRL_S2_Y2RMODE));

// source select  sf_ctrl
    sf_ctrl_reg |= ((g2d_ctrl.src_sel_0) & Hw2D_SFCTRL_S0_SEL) |((g2d_ctrl.src_sel_1<<2) & Hw2D_SFCTRL_S1_SEL)
    		| ((g2d_ctrl.src_sel_2<<4) & Hw2D_SFCTRL_S2_SEL) | ((g2d_ctrl.src_sel_3<<6) & Hw2D_SFCTRL_S3_SEL) ;

// source arithmetic mode 	sa_ctrl
    sa_ctrl_reg |= (((g2d_ctrl.src0_arith ) & Hw2D_SACTRL_S0_ARITHMODE) |
                    ((g2d_ctrl.src1_arith<<4) & Hw2D_SACTRL_S1_ARITHMODE) | ((g2d_ctrl.src2_arith<<8) & Hw2D_SACTRL_S2_ARITHMODE));
// source chroma key enable : for arithmetic	sa_ctrl
    sa_ctrl_reg |= (((g2d_ctrl.src0_chroma_en<<16) & Hw2D_SACTRL_S0_CHROMAEN) |
                    ((g2d_ctrl.src1_chroma_en<<17) & Hw2D_SACTRL_S1_CHROMAEN) | ((g2d_ctrl.src2_chroma_en<<18) & Hw2D_SACTRL_S2_CHROMAEN));

	//BITCSET(pHwOVERLAYMIXER->SF_CTRL.nREG, 0x0FFFFFFF, sf_ctrl_reg);
	//BITCSET(pHwOVERLAYMIXER->SA_CTRL.nREG, 0x0FFFFFFF, sa_ctrl_reg);

	value = sf_ctrl_reg;
	__raw_writel(value, reg + 0xa0);

	value = sa_ctrl_reg;
	__raw_writel(value, reg + 0xa4);

}

/* -------- Source Operator pattern set ------- */
/*------------------------------------------------------------------
GRE_2D_SetOperator
 graphic engine operator 0, 1 setting
-------------------------------------------------------------------*/
void GRE_2D_SetOperator(G2D_OP_TYPE op_set, unsigned short alpha , unsigned char RY, unsigned char GU, unsigned char BV)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	switch(op_set)
    {
		case OP_0:
			//BITCSET(pHwOVERLAYMIXER->OP0_PAT.nREG, 0x00FFFFFF,((RY<<16)&HwGE_PAT_RY) | ((GU<<8)&HwGE_PAT_GU)| (BV&HwGE_PAT_BV));   // pHwOVERLAYMIXER->OP0_PAT
      		//BITCSET(pHwOVERLAYMIXER->OP0_ALPHA.nREG,0x0000FFFF, ( (alpha)& HwGE_ALPHA) );

			value = (__raw_readl(reg + 0xc0) & ~(0x00FFFFFF));
			value |= (((RY<<16)&HwGE_PAT_RY) | ((GU<<8)&HwGE_PAT_GU)| (BV&HwGE_PAT_BV));
			__raw_writel(value, reg + 0xc0);

			value = (__raw_readl(reg + 0xb0) & ~(0x0000FFFF));
			value |= ( (alpha)& HwGE_ALPHA);
			__raw_writel(value, reg + 0xb0);
            break;

        default:
            break;
    }

}

/*------------------------------------------------------------------
GRE_2D_SetOperatorCtrl
 graphic engine operator control register setting
-------------------------------------------------------------------*/
void GRE_2D_SetOperatorCtrl(G2D_OP_TYPE op_set, G2D_OP_ACON ACON1, G2D_OP_ACON ACON0, G2D_OP_CCON CCON1, G2D_OP_CCON CCON0, G2D_OP_ATUNE ATUNE, G2D_OP1_CHROMA CSEL,GE_ROP_TYPE op )
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	switch(op_set){
		case OP_0:
		//BITCSET(pHwOVERLAYMIXER->OP0_CTRL.nREG, 0xFFFFFFFF, (   ( (ACON1<<28)&HwGE_OP_CTRL_ACON1)| ((ACON0<<24)&HwGE_OP_CTRL_ACON0)
  		//											     | ( (CCON1<<21)&HwGE_OP_CTRL_CCON1)| ((CCON0<<16)&HwGE_OP_CTRL_CCON0)
  		//											     | ( (ATUNE<<12)&HwGE_OP_CTRL_ATUNE)| ((CSEL<<8)&HwGE_OP_CTRL_CSEL)
  		//											     | (op & HwGE_OP_CTRL_OPMODE)));

		value = (__raw_readl(reg + 0xd0) & ~(0xFFFFFFFF));
		value |= (   ( (ACON1<<28)&HwGE_OP_CTRL_ACON1)| ((ACON0<<24)&HwGE_OP_CTRL_ACON0)
  													     | ( (CCON1<<21)&HwGE_OP_CTRL_CCON1)| ((CCON0<<16)&HwGE_OP_CTRL_CCON0)
  													     | ( (ATUNE<<12)&HwGE_OP_CTRL_ATUNE)| ((CSEL<<8)&HwGE_OP_CTRL_CSEL)
  													     | (op & HwGE_OP_CTRL_OPMODE));
		__raw_writel(value, reg + 0xd0);
		break;

		default:
			break;

	}
}

/*-------- BACK END CHANNEL DESTINATION SETTIG. -------*/
/*------------------------------------------------------------------
GRE_2D_SetBChAddress
  graphic engine BACK End channel address 0,1,2 setting
-------------------------------------------------------------------*/
void GRE_2D_SetBChAddress(G2D_CHANNEL ch, unsigned int add0, unsigned int add1, unsigned int add2)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();

    if(ch == DEST_CH)
    {
		//BITCSET(pHwOVERLAYMIXER->BCH_DADDR0.nREG, 0xFFFFFFFF, add0); // pHwOVERLAYMIXER->BCH_DADDR0
		//BITCSET(pHwOVERLAYMIXER->BCH_DADDR1.nREG, 0xFFFFFFFF, add1); // pHwOVERLAYMIXER->BCH_DADDR0
		//BITCSET(pHwOVERLAYMIXER->BCH_DADDR2.nREG, 0xFFFFFFFF, add2); // pHwOVERLAYMIXER->BCH_DADDR0

		__raw_writel(add0, reg + 0xe0);
		__raw_writel(add1, reg + 0xe4);
		__raw_writel(add2, reg + 0xe8);
    }
}

/*------------------------------------------------------------------
GRE_2D_SetBChPosition
 graphic engine BACK END channel position settig

 frameps_x, frameps_y : frame pixsel size
 poffset_x, poffset_y : pixsel offset
-------------------------------------------------------------------*/
void GRE_2D_SetBChPosition(G2D_CHANNEL ch, unsigned int  frameps_x, unsigned int  frameps_y, unsigned int  poffset_x, unsigned int  poffset_y)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

    if(ch == DEST_CH)
    {
		//BITCSET(pHwOVERLAYMIXER->BCH_DFSIZE.nREG, 0x0FFF0FFF, ((frameps_y<<16) | frameps_x)); // pHwOVERLAYMIXER->BCH_DFSIZE
		//BITCSET(pHwOVERLAYMIXER->BCH_DOFF.nREG, 0x0FFF0FFF, ((poffset_y<<16) | poffset_x)); // pHwOVERLAYMIXER->BCH_DOFF

		value = (__raw_readl(reg + 0xec) & ~(0x0FFF0FFF));
		value |= ((frameps_y<<16) | frameps_x);
		__raw_writel(value, reg + 0xec);

		value = (__raw_readl(reg + 0xf0) & ~(0x0FFF0FFF));
		value |= ((poffset_y<<16) | poffset_x);
		__raw_writel(value, reg + 0xf0);
    }
}

/*------------------------------------------------------------------
GRE_2D_SetBChControl
 graphic engine Back END channel control setting
 ysel, xsel:
 converter_en : format converter enable RGB -> YUV
 converter_mode : format converter mode RGB -> YUV
 opmode : flip, rotate
 data_form : rgb, yuv, alpha-rgb
-------------------------------------------------------------------*/
void GRE_2D_SetBChControl(G2D_BCH_CTRL_TYPE *g2d_bch_ctrl)
{
	unsigned int BCH_ctrl_reg = 0;

	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();

	BCH_ctrl_reg |= ((g2d_bch_ctrl->MABC<<21) & HwGE_BCH_DCTRL_MABC);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->ysel<<18) & HwGE_BCH_DCTRL_YSEL);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->xsel<<16) & HwGE_BCH_DCTRL_XSEL);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->converter_en<<15) & HwGE_BCH_DCTRL_CEN);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->converter_mode<<13) & HwGE_BCH_DCTRL_CMODE);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->DSUV<<11) & HwGE_BCH_DCTRL_DSUV);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->opmode<<8) & HwGE_BCH_DCTRL_OPMODE);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->dithering_type<<6) & HwGE_BCH_DCTRL_DOP);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->dithering_en<<5) & HwGE_BCH_DCTRL_DEN);
	BCH_ctrl_reg |= (g2d_bch_ctrl->data_form.format & HwGE_BCH_DCTRL_DDFRM);
	BCH_ctrl_reg |= ((g2d_bch_ctrl->data_form.data_swap<<24) & HwGE_DCH_SSB);

	//BITCSET(pHwOVERLAYMIXER->BCH_DCTRL.nREG, 0xFFFFFFFFF, BCH_ctrl_reg); // pHwOVERLAYMIXER->BCH_DCTRL
	__raw_writel(BCH_ctrl_reg, reg + 0xf4);
}

void GRE_2D_SetDitheringMatrix(unsigned short *Matrix)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	//BITCSET(pHwOVERLAYMIXER->BCH_DDMAT0.nREG, 0x1F1F1F1F, Matrix[0] | (Matrix[1]<<8) | (Matrix[2]<<16) | (Matrix[3]<<24)); // pHwOVERLAYMIXER->BCH_DDMAT0
	//BITCSET(pHwOVERLAYMIXER->BCH_DDMAT1.nREG, 0x1F1F1F1F, Matrix[4] | (Matrix[5]<<8) | (Matrix[6]<<16) | (Matrix[7]<<24));
	//BITCSET(pHwOVERLAYMIXER->BCH_DDMAT2.nREG, 0x1F1F1F1F, Matrix[8] | (Matrix[9]<<8) | (Matrix[10]<<16) | (Matrix[11]<<24));
	//BITCSET(pHwOVERLAYMIXER->BCH_DDMAT3.nREG, 0x1F1F1F1F, Matrix[12] | (Matrix[13]<<8) | (Matrix[14]<<16) | (Matrix[15]<<24));

	value = (__raw_readl(reg + 0x100) & ~(0x1F1F1F1F));
	value |= (Matrix[0] | (Matrix[1]<<8) | (Matrix[2]<<16) | (Matrix[3]<<24));
	__raw_writel(value, reg + 0x100);

	value = (__raw_readl(reg + 0x104) & ~(0x1F1F1F1F));
	value |= (Matrix[4] | (Matrix[5]<<8) | (Matrix[6]<<16) | (Matrix[7]<<24));
	__raw_writel(value, reg + 0x104);

	value = (__raw_readl(reg + 0x108) & ~(0x1F1F1F1F));
	value |= (Matrix[8] | (Matrix[9]<<8) | (Matrix[10]<<16) | (Matrix[11]<<24));
	__raw_writel(value, reg + 0x108);

	value = (__raw_readl(reg + 0x10c) & ~(0x1F1F1F1F));
	value |= (Matrix[12] | (Matrix[13]<<8) | (Matrix[14]<<16) | (Matrix[15]<<24));
	__raw_writel(value, reg + 0x10c);
}

/*------------------------------------------------------------------
GRE_2D_Enable
 graphic engine channel enable control
-------------------------------------------------------------------*/
void GRE_2D_Enable(G2D_EN grp_enalbe, unsigned char int_en)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	//BITCSET( pHwOVERLAYMIXER->OM_CTRL.nREG, (HwGE_GE_CTRL_EN|HwGE_GE_INT_EN), ((int_en<<16)|grp_enalbe));

	value = (__raw_readl(reg + 0x110) & ~(HwGE_GE_CTRL_EN|HwGE_GE_INT_EN));
	value |= ((int_en<<16)|grp_enalbe);
	__raw_writel(value, reg + 0x110);
}

/*------------------------------------------------------------------
GRE_2D_Check
 graphic engine transger check
-------------------------------------------------------------------*/
unsigned int GRE_2D_Check(void)
{
	//unsigned int contrReg = 0;
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();

	return __raw_readl(reg + 0x110);
}

/*------------------------------------------------------------------
GRE_2D_IntEnable
 graphic engine interrupt enable
-------------------------------------------------------------------*/
void GRE_2D_IntEnable(unsigned int int_en)
{
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	//BITCSET( pHwOVERLAYMIXER->OM_IREQ.nREG, 0xFFFFFFFF, int_en);
	value = (__raw_readl(reg + 0x114) & ~(0xFFFFFFFF));
	value |= int_en;
	__raw_writel(value, reg + 0x114);
}

/*------------------------------------------------------------------
GRE_2D_IntCtrl
 graphic engine interrupt control
 wr : write / read
 int_irq : interrupt request
 int_flg : flag bit
-------------------------------------------------------------------*/
G2D_INT_TYPE GRE_2D_IntCtrl(unsigned char wr, G2D_INT_TYPE flag, unsigned char int_irq, unsigned char int_flg)
{
	G2D_INT_TYPE ret_v = 0;
	//POVERLAYMIXER pHwOVERLAYMIXER;
	//pHwOVERLAYMIXER  = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);
	volatile void __iomem *reg = GRE_2D_GetAddress();
	unsigned int value = 0x00;

	if(wr) {
        if(flag & G2D_INT_R_IRQ){
            //BITCSET( pHwOVERLAYMIXER->OM_IREQ.nREG, HwGE_GE_IREQ_IRQ, (int_irq ? HwGE_GE_IREQ_IRQ : 0));
			value = (__raw_readl(reg + 0x114) & ~(HwGE_GE_IREQ_IRQ));
			value |= (int_irq ? HwGE_GE_IREQ_IRQ : 0);
			__raw_writel(value, reg + 0x114);
        }

        if(flag & G2D_INT_R_FLG){
            //BITCSET( pHwOVERLAYMIXER->OM_IREQ.nREG, HwGE_GE_IREQ_FLG, (int_flg ? HwGE_GE_IREQ_FLG : 0));
			value = (__raw_readl(reg + 0x114) & ~(HwGE_GE_IREQ_FLG));
			value |= (int_flg ? HwGE_GE_IREQ_FLG : 0);
			__raw_writel(value, reg + 0x114);
        }
    }
    else {
		//if(ISSET(pHwOVERLAYMIXER->OM_IREQ.nREG, HwGE_GE_IREQ_IRQ)){
		//	ret_v |= G2D_INT_R_IRQ;
		//}

		//if(ISSET(pHwOVERLAYMIXER->OM_IREQ.nREG, HwGE_GE_IREQ_FLG)){ // pHwOVERLAYMIXER->OM_IREQ
		//	ret_v |= G2D_INT_R_FLG;
		//}

		value = __raw_readl(reg + 0x114);
		if( value & HwGE_GE_IREQ_IRQ ){
			ret_v |= G2D_INT_R_IRQ;
		}

		if( value & HwGE_GE_IREQ_FLG ){ // pHwOVERLAYMIXER->OM_IREQ
			ret_v |= G2D_INT_R_FLG;
		}
	}

    return ret_v;
}

#if defined(TCC_OVERLAY_MIXER_CLUT_SUPPORT)
void GRE_2D_ClutCtrl(unsigned int ch, unsigned int index, unsigned int data)
{
	//OVERLAYMIXER *p;
	//unsigned int *pClut;
	volatile void __iomem *reg = GRE_2D_GetAddress();

	if(index > 256)
	{
		pr_err("[ERR][G2D] Invalid index value(%d)\n",__func__, __LINE__, index);
		return;
	}

	// p = (volatile POVERLAYMIXER)tcc_p2v(HwOVERLAYMIXER_BASE);

	if(ch == 0){
		//pClut = (unsigned int*)&p->FCH0_LUT[0];
		//pClut[index] = data;
		__raw_writel(data, reg + 0x400 + (index*0x4));
	}
	else if(ch == 1){
		//pClut = (unsigned int*)&p->FCH1_LUT[0];
		//pClut[index] = data;
		__raw_writel(data, reg + 0x800 + (index*0x4));
	}
	else if(ch == 2){
		//pClut = (unsigned int*)&p->FCH2_LUT[0];
		//pClut[index] = data;
	}
	else {
		pr_err("[ERR][G2D] [%s:%d] invalid ch for clut\n", __func__, __LINE__);
	}
}
#endif /* TCC_OVERLAY_MIXER_CLUT_SUPPORT */

volatile void __iomem* GRE_2D_GetAddress(void)
{
	if(pGre2D_reg == NULL){
		pr_err("[ERR][G2D] %s \n", __func__);
		return NULL;
	}

	return pGre2D_reg;
}

void GRE_2D_DUMP(void)
{
	volatile void __iomem *pReg;
	unsigned int cnt = 0;

	if(pGre2D_np == NULL )
		return;

	pReg = GRE_2D_GetAddress();

	pr_info("[INF][G2D] GRE_2D :: 0%p \n", pReg);
	while(cnt < 0x100)
	{
		pr_debug("[DBG][G2D] %p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg+cnt, __raw_readl(pReg+cnt), __raw_readl(pReg+cnt+0x4), __raw_readl(pReg+cnt+0x8), __raw_readl(pReg+cnt+0xC));
		cnt += 0x10;
	}
}

static int __init gre2d_init(void)
{
	pGre2D_np = of_find_compatible_node(NULL, NULL, "telechips,graphic.2d");
	if(pGre2D_np == NULL)
		pr_info("[INF][G2D] vioc-g2d: disabled\n");

	pGre2D_reg = of_iomap(pGre2D_np, 0);
	if(pGre2D_reg)
		pr_info("[INF][G2D] vioc-g2d: 0x%p\n", pGre2D_reg);
/*
	else {
		struct resource res;
		int rc;

		rc = of_address_to_resource(pGre2D_np, 0, &res);
		pr_info("[INF][G2D] %s GRE_2D: 0x%x ~ 0x%x \n", __func__, res.start, res.end);
	}
*/

return 0;
}
arch_initcall(gre2d_init);

