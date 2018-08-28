/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#ifndef _TCC_SCALER_COMMON_H_
#define _TCC_SCALER_COMMON_H_

#define TCC_SCALER_PATH 						0x303

#define TCC_SCALER_FILTER2D 					0x304

struct tc_scaler_filter2D_set {
	unsigned int filter2d_need;
	unsigned int filter2d_num;
	unsigned int filter2d_level;
	unsigned int filter2d_pos;
};

struct tc_scaler_path_set {
	unsigned int rdma_num;
	unsigned int wmixer_num;
	unsigned int wdma_num;
	unsigned int scaler_num;
	unsigned int scaler_pos; // 0 : rdma -> sc , 1 : sc -> wdma
	unsigned int mixer_bypass;
	unsigned int afbc_dec_num;
	unsigned int afbc_dec_need;

	unsigned int filter2d_need;
	unsigned int filter2d_num;
	unsigned int filter2d_level;
	unsigned int filter2d_pos;

	unsigned int sar_need;
	unsigned int sar_level;
	
	// number : -1  not use 
	int pixel_mapper_n;

};

#endif

