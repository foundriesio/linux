/* linux/arch/arm/mach-tcc893x/tcc_lut_ioctl.h
 *
 * Copyright (C) 2008, 2009 Telechips, Inc.
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
#ifndef _TCC_VIOC_LUT_IOCTL_H_
#define _TCC_VIOC_LUT_IOCTL_H_

#include "../../generated/autoconf.h"
//#include <linux/ioctl.h>

#define CONFIG_LUT_SUPPORT_CSC_PRESET_SET

#define LUT_IOC_MAGIC		'l'

enum {
	LUT_DEV0 = 0,
	LUT_DEV1 = 1,
	LUT_DEV2 = 2,
	LUT_COMP0 = 3,
	LUT_COMP1 = 4,
	LUT_COMP2 = 5,
	LUT_COMP3 = 6,
	LUT_DEV3 = 7,
};

enum {
	LUT_CSC_PRESET_SDR_TO_HDR10 = 0,
	LUT_CSC_PRESET_HDR10_TO_SDR,
	LUT_CSC_PRESET_SDR_TO_HLG,
	LUT_CSC_PRESET_HLG_TO_SDR,
	LUT_CSC_PRESET_MAX
};

enum {
	LUT_CSC_PRESET_BT709_TO_BT2020 = 0,
	LUT_CSC_PRESET_BT2020_TO_BT709,
	LUT_CSC_PRESET_BTXXX_MAX
};

enum {
	LUT_CSC_COEFF11 = 0,
	LUT_CSC_COEFF12,
	LUT_CSC_COEFF13,
	LUT_CSC_COEFF21,
	LUT_CSC_COEFF22,
	LUT_CSC_COEFF23,
	LUT_CSC_COEFF31,
	LUT_CSC_COEFF32,
	LUT_CSC_COEFF33,
	LUT_CSC_COEFF_MAX
};

struct VIOC_LUT_VALUE_SET {
#if defined(CONFIG_ARCH_TCC803X) ||\
		defined(CONFIG_ARCH_TCC897X) ||\
			defined(CONFIG_ARCH_TCC805X)
	unsigned int Gamma[256];
	//vioc componet : RGB ,  disp component : BGR
#else
	// 10bit RGB
	unsigned int Gamma[1024];
	//vioc componet : RGB ,  disp component : BGR
#endif				//
	unsigned int lut_number;
	//enum VIOC_LUT_NUM
};

/**
 * Structure that interfaces with the IOCTL of the tcc_lut driver.
 * This is an extension of VIOC_LUT_VALUE_SET, It has the additional
 * variable to provides information of lut_size
 */
struct VIOC_LUT_VALUE_SET_EX {
	/** lookup table size */
	unsigned int lut_size;
	/** lookup table id */
	unsigned int lut_number;
	/** lookup parameter bit[ :0] table 0: rgb-table, 1: y-table */
	unsigned int param;
	/** lookup table */
	unsigned int Gamma[1024];
};

struct VIOC_LUT_PLUG_IN_SET {
	unsigned int enable;
	unsigned int lut_number;	//enum VIOC_LUT_NUM
	unsigned int lut_plug_in_ch;	//ex :VIOC_LUT_RDMA_00
};

struct VIOC_LUT_ONOFF_SET {
	unsigned int lut_onoff;
	unsigned int lut_number;	//enum VIOC_LUT_NUM
};

struct VIOC_LUT_SET_Type {
	unsigned int lut_value;
	int sin_value;
	int cos_value;

	int saturation;
};

struct VIOC_LUT_CSC_PRESET_SET {
	unsigned int lut_number;
	unsigned int preset_id;
	unsigned int enable;
};

struct VIOC_LUT_CSC_COEFF {
	unsigned short csc_coeff_1[3];
	unsigned short csc_coeff_2[3];
	unsigned short csc_coeff_3[3];
};

struct VIOC_LUT_MIX_CONFIG {
	int r2y_sel;
	int bypass;
};

struct VIOC_LUT_UPDATE_PEND {
	unsigned int lut_number;

	/** lookup parameter bit[ :0] table 0: rgb-table, 1: y-table */
	unsigned int param;
	int update_pend;
};

#define TCC_LUT_SET\
	_IOW(LUT_IOC_MAGIC, 2, struct VIOC_LUT_VALUE_SET)
	//hw vioc lut set
#define TCC_LUT_PLUG_IN\
	_IOW(LUT_IOC_MAGIC, 3, struct VIOC_LUT_PLUG_IN_SET)
#define TCC_LUT_ONOFF\
	_IOW(LUT_IOC_MAGIC, 4, struct VIOC_LUT_ONOFF_SET)

#define TCC_LUT_PLUG_IN_INFO\
	_IOWR(LUT_IOC_MAGIC, 10, struct VIOC_LUT_PLUG_IN_SET)
#define TCC_LUT_CSC_PRESET_SET\
	_IOW(LUT_IOC_MAGIC, 11, struct VIOC_LUT_CSC_PRESET_SET)

#define TCC_LUT_GET_DEPTH\
	_IOR(LUT_IOC_MAGIC, 20, unsigned int)
#define TCC_LUT_SET_EX\
	_IOW(LUT_IOC_MAGIC, 21, struct VIOC_LUT_VALUE_SET_EX)

#define TCC_LUT_SET_CSC_COEFF\
	_IOW(LUT_IOC_MAGIC, 22, struct VIOC_LUT_CSC_COEFF)
#define TCC_LUT_SET_MIX_CONIG\
	_IOW(LUT_IOC_MAGIC, 24, struct VIOC_LUT_MIX_CONFIG)
#define TCC_LUT_GET_UPDATE_PEND\
	_IOW(LUT_IOC_MAGIC, 25, struct VIOC_LUT_UPDATE_PEND)

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT			(0x8-1)
#define BIT_0			3
#define GET_ADDR_YUV42X_spY(Base_addr)\
	(((((unsigned int)Base_addr) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y)\
	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y)\
	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y)\
	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#endif

#endif //
