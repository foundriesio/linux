/****************************************************************************
 * Copyright (C) 2019 Telechips Inc.
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
 * this program; if not	 write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __DT_TCC901X_PM_H__
#define __DT_TCC901X_PM_H__

/* Wake up source index */
#define PMU_WKUP_TSADC_UPDOWN		0
#define PMU_WKUP_TSADC_STOP_WKU		1
#define PMU_WKUP_TSADC_WAKEUP		2
#define PMU_WKUP_RTC_WAKEUP		3
#define PMU_WKUP_nIRQOUT00		4
#define PMU_WKUP_nIRQOUT01		5
#define PMU_WKUP_nIRQOUT02		6
#define PMU_WKUP_nIRQOUT03		7
#define PMU_WKUP_GP_B11			8
#define PMU_WKUP_GP_B12			9
#define PMU_WKUP_GP_B13			10
#define PMU_WKUP_GP_B14			11
#define PMU_WKUP_GP_B15			12
#define PMU_WKUP_GP_B28			13
#define PMU_WKUP_GP_C00			14
#define PMU_WKUP_GP_C15			15
#define PMU_WKUP_GP_C18			16
#define PMU_WKUP_GP_C19			17
#define PMU_WKUP_GP_C20			18
#define PMU_WKUP_GP_C21			19
#define PMU_WKUP_GP_C22			20
#define PMU_WKUP_GP_C23			21
#define PMU_WKUP_GP_C24			22
#define PMU_WKUP_GP_C25			23
#define PMU_WKUP_GP_C27			24
#define PMU_WKUP_GP_C28			25
#define PMU_WKUP_GP_C29			26
#define PMU_WKUP_GP_D08			27
#define PMU_WKUP_GP_D09			28
#define PMU_WKUP_GP_D12			29
#define PMU_WKUP_GP_D13			30
#define PMU_WKUP_GP_D14			31
#define PMU_WKUP_GP_D20			32
#define PMU_WKUP_GP_E31			33
#define PMU_WKUP_GP_F15			34
#define PMU_WKUP_GP_F16			35
#define PMU_WKUP_GP_F17			36
#define PMU_WKUP_GP_F18			37
#define PMU_WKUP_GP_F19			38
#define PMU_WKUP_GP_F20			39
#define PMU_WKUP_GP_F21			40
#define PMU_WKUP_GP_F22			41
#define PMU_WKUP_GP_F23			42
#define PMU_WKUP_GP_F29			43
#define PMU_WKUP_GP_G05			44
#define PMU_WKUP_GP_G08			45
#define PMU_WKUP_GP_G09			46
#define PMU_WKUP_GP_G10			47
#define PMU_WKUP_GP_G11			48
#define PMU_WKUP_GP_G12			49
#define PMU_WKUP_GP_G13			50
#define PMU_WKUP_GP_G14			51
#define PMU_WKUP_GP_G16			52
#define PMU_WKUP_GP_G17			53
#define PMU_WKUP_GP_G18			54
#define PMU_WKUP_GP_G19			55
#define PMU_WKUP_GP_HDMI00		56
#define PMU_WKUP_GP_HDMI01		57
#define PMU_WKUP_GP_SD06		58
#define PMU_WKUP_GP_SD07		59
#define PMU_WKUP_GP_SD08		60
#define PMU_WKUP_GP_SD09		61
#define PMU_WKUP_GP_SD10		62
#define PMU_WKUP_REMOCON		63

/* Wake up polarity */
#define WAKEUP_ACT_LO		0
#define WAKEUP_ACT_HI		1

#endif
