/*
 * Copyright (C) Telechips Inc.
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
#ifndef __GRE2D_H__
#define __GRE2D_H__

#include <video/tcc/tcc_types.h>
#include "tcc_gre2d_reg.h"
#include <video/tcc/tcc_gre2d_type.h>

#define TCC_OVERLAY_MIXER_CLUT_SUPPORT

 /*------------------------------------------------------------------
GRE_2D_SetInterrupt
 graphic engine interrupt enable/ disable
-------------------------------------------------------------------*/
extern void GRE_2D_SetInterrupt(char onoff);

/*------------------------------------------------------------------
GRE_2D_SetFChAddress
 graphic engine Front End channel address 0,1,2 setting
-------------------------------------------------------------------*/
extern void GRE_2D_SetFChAddress(G2D_CHANNEL ch, unsigned int add0, unsigned int add1, unsigned int add2);

/*------------------------------------------------------------------
GRE_2D_SetFChPosition
 graphic engine Front channel position settig
 
 frameps_x, frameps_y : frame pixsel size
 poffset_x, poffset_y : pixsel offset
 imageps_x, imageps_y : imagme pixel size
 winps_x, winps_y : window pixsel offset
-------------------------------------------------------------------*/
extern void GRE_2D_SetFChPosition(G2D_CHANNEL ch, unsigned int frameps_x, unsigned int frameps_y, 
                                unsigned int poffset_x, unsigned int poffset_y, unsigned int imageps_x, unsigned int imageps_y, unsigned int winps_x, unsigned int winps_y );

/*------------------------------------------------------------------
GRE_2D_SetFChControl
 graphic engine Front channel control setting
 mode : flip, rotate
 data_form : rgb, yuv, alpha-rgb
-------------------------------------------------------------------*/
extern void GRE_2D_SetFChControl(G2D_CHANNEL ch, G2D_MABC_TYPE MABC, unsigned char LUTE, unsigned char SSUV, G2D_OP_MODE mode, G2D_ZF_TYPE ZF, G2D_FMT_CTRL data_form);

/*------------------------------------------------------------------
GRE_2D_SetFChChromaKey
 graphic engine Front channel chroma key Set
-------------------------------------------------------------------*/
extern void GRE_2D_SetFChChromaKey(G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV);

/*------------------------------------------------------------------
GRE_2D_SetFChArithmeticPar
 graphic engine Front channel Arithmetic parameter setting
-------------------------------------------------------------------*/
extern void GRE_2D_SetFChArithmeticPar(G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV);

/*------------------------------------------------------------------
GRE_2D_SetSrcCtrl
 graphic engine sorce control
-------------------------------------------------------------------*/
extern void GRE_2D_SetSrcCtrl(G2D_SRC_CTRL g2d_ctrl);

/*-------- Source Operator pattern set -------*/
/*------------------------------------------------------------------
GRE_2D_SetOperator
 graphic engine operator 0, 1 setting 
-------------------------------------------------------------------*/
extern void GRE_2D_SetOperator(G2D_OP_TYPE op_set, unsigned short alpha , unsigned char RY, unsigned char GU, unsigned char BV);

/*------------------------------------------------------------------
GRE_2D_SetOperatorCtrl
 graphic engine operator control register setting
-------------------------------------------------------------------*/
extern void GRE_2D_SetOperatorCtrl(G2D_OP_TYPE op_set, G2D_OP_ACON ACON1, G2D_OP_ACON ACON0, G2D_OP_CCON CCON1, G2D_OP_CCON CCON0, G2D_OP_ATUNE ATUNE, G2D_OP1_CHROMA CSEL,GE_ROP_TYPE op );

/*-------- BACK END CHANNEL DESTINATION SETTIG. -------*/
/*------------------------------------------------------------------
GRE_2D_SetBChAddress
graphic engine BACK End channel address 0,1,2 setting
-------------------------------------------------------------------*/
extern void GRE_2D_SetBChAddress(G2D_CHANNEL ch, unsigned int add0, unsigned int add1, unsigned int add2);

/*------------------------------------------------------------------
GRE_2D_SetBChPosition
 graphic engine BACK END channel position settig
 
 frameps_x, frameps_y : frame pixsel size
 poffset_x, poffset_y : pixsel offset
-------------------------------------------------------------------*/
extern void GRE_2D_SetBChPosition(G2D_CHANNEL ch, unsigned int frameps_x, unsigned intframeps_y, unsigned int poffset_x, unsigned int poffset_y);

/*------------------------------------------------------------------
GRE_2D_SetBChControl
 graphic engine Back END channel control setting
 mode : flip, rotate
 data_form : rgb, yuv, alpha-rgb
-------------------------------------------------------------------*/
extern void GRE_2D_SetBChControl(G2D_BCH_CTRL_TYPE *g2d_bch_ctrl);

/*------------------------------------------------------------------
GRE_2D_SetDitheringMatrix
 graphic engine Dithering Matix Setting
-------------------------------------------------------------------*/
extern void GRE_2D_SetDitheringMatrix(unsigned short *Matrix);

/*------------------------------------------------------------------
GRE_2D_Enable
 graphic engine channel enable control
-------------------------------------------------------------------*/
extern void GRE_2D_Enable(G2D_EN grp_enalbe, unsigned char int_en);

/*------------------------------------------------------------------
GRE_2D_Check
 graphic engine transger check
-------------------------------------------------------------------*/
extern unsigned int GRE_2D_Check(void);

/*------------------------------------------------------------------
GRE_2D_IntEnable
 graphic engine interrupt enable
-------------------------------------------------------------------*/
extern void GRE_2D_IntEnable(unsigned int int_en);

/*------------------------------------------------------------------
GRE_2D_IntCtrl
 graphic engine interrupt control 
 wr : write / read
 int_irq : interrupt request 
 int_flg : flag bit
-------------------------------------------------------------------*/
extern G2D_INT_TYPE GRE_2D_IntCtrl(unsigned char wr, G2D_INT_TYPE flag, unsigned char int_irq, unsigned char int_flg);

extern void GRE_2D_ClutCtrl(unsigned int ch, unsigned int index, unsigned int data);

#endif//__GRE2D_H__
