/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA

@note Tab size is 8
****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <hdmi_1_4_video.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi.h>

static struct {
        unsigned int refresh_rate;
        dtd_t dtd;
} tcc_detailed_timing_table[] = {
        /**
		     mCode mPixelClock (Hz *1000)  mHImageSize		  mVBlanking	       mVSyncPolarity
		     |           mInterlaced             mHSyncOffset	     |   mVBorder      |
		     |       |       | mHactive	      |     |  mHSyncPulseWidth  |  mVImageSize|
		     |  mPixelRepetitionInput  mHBorder     |	 |  mHSyncPolarity  |  mVSyncOffset
		     |  |    |       |   | mHBlanking |     |	 |  | mVActive   |  |  |   mVSyncPulseWidth */
	//original value is 25.00175MHz
	{60000 , {   1, 0, 25200 , 0,  640,  160, 0,  4,   16,	96, 0,	480, 45, 0, 3, 10,  2, 0}},
	//original value is 25.17MHz
	{59940 , {   1, 0, 25175 , 0,  640,  160, 0,  4,   16,	96, 0,	480, 45, 0, 3, 10,  2, 0}},

	{60000 , {   2, 0, 27027 , 0,  720,  138, 0,  4,   16,	62, 0,	480, 45, 0, 3,	9,  6, 0}},
	{59940 , {   2, 0, 27000 , 0,  720,  138, 0,  4,   16,	62, 0,	480, 45, 0, 3,	9,  6, 0}},

	{60000 , {   3, 0, 27027 , 0,  720,  138, 0, 16,   16,	62, 0,	480, 45, 0, 9,	9,  6, 0}},
	{59940 , {   3, 0, 27000 , 0,  720,  138, 0, 16,   16,	62, 0,	480, 45, 0, 9,	9,  6, 0}},

	{60000 , {   4, 0, 74250 , 0, 1280,  370, 0, 16,  110,	40, 1,	720, 30, 0, 9,	5,  5, 1}},
	//original value is 74175824.175824175824175824175824
	{59940 , {   4, 0, 74175 , 0, 1280,  370, 0, 16,  110,	40, 1,	720, 30, 0, 9,	5,  5, 1}},

	{60000 , {   5, 0, 74250 , 1, 1920,  280, 0, 16,   88,	44, 1,	540, 22, 0, 9,	2,  5, 1}},
	//original value is 74175824.175824175824175824175824
	{59940 , {   5, 0, 74175 , 1, 1920,  280, 0, 16,   88,	44, 1,	540, 22, 0, 9,	2,  5, 1}},

	{60000 , {   6, 1, 27027 , 1, 1440,  276, 0,  4,   38, 124, 0,	240, 22, 0, 3,	4,  3, 0}},
	{59940 , {   6, 1, 27000 , 1, 1440,  276, 0,  4,   38, 124, 0,	240, 22, 0, 3,	4,  3, 0}},

	{60000 , {   7, 1, 27027 , 1, 1440,  276, 0, 16,   38, 124, 0,	240, 22, 0, 9,	4,  3, 0}},
	{59940 , {   7, 1, 27000 , 1, 1440,  276, 0, 16,   38, 124, 0,	240, 22, 0, 9,	4,  3, 0}},

	{60054 , {   8, 1, 27000 , 0, 1440,  276, 0,  4,   38, 124, 0,	240, 22, 0, 3,	4,  3, 0}},
	{59826 , {   8, 1, 27000 , 0, 1440,  276, 0,  4,   38, 124, 0,	240, 23, 0, 3,	5,  3, 0}},

	{60054 , {   9, 1, 27000 , 0, 1440,  276, 0, 16,   38, 124, 0,	240, 22, 0, 9,	4,  3, 0}},
	{59826 , {   9, 1, 27000 , 0, 1440,  276, 0, 16,   38, 124, 0,	240, 23, 0, 9,	5,  3, 0}},

	{60000 , {  10, 0, 54054 , 1, 2880,  552, 0,  4,   76, 248, 0,	240, 22, 0, 3,	4,  3, 0}},
	{59940 , {  10, 0, 54000 , 1, 2880,  552, 0,  4,   76, 248, 0,	240, 22, 0, 3,	4,  3, 0}},

	{60000 , {  11, 0, 54054 , 1, 2880,  552, 0, 16,   76, 248, 0,	240, 22, 0, 9,	4,  3, 0}},
	{59940 , {  11, 0, 54000 , 1, 2880,  552, 0, 16,   76, 248, 0,	240, 22, 0, 9,	4,  3, 0}},

	{60054 , {  12, 0, 54000 , 0, 2880,  552, 0,  4,   76, 248, 0,	240, 22, 0, 3,	4,  3, 0}},
	{59826 , {  12, 0, 54000 , 0, 2880,  552, 0,  4,   76, 248, 0,	240, 23, 0, 3,	5,  3, 0}},

	{60054 , {  13, 0, 54000 , 0, 2880,  552, 0, 16,   76, 248, 0,	240, 22, 0, 9,	4,  3, 0}},
	{59826 , {  13, 0, 54000 , 0, 2880,  552, 0, 16,   76, 248, 0,	240, 23, 0, 9,	5,  3, 0}},

	{60000 , {  14, 0, 54054 , 0, 1440,  276, 0,  4,   32, 124, 0,	480, 45, 0, 3,	9,  6, 0}},
	{59940 , {  14, 0, 54000 , 0, 1440,  276, 0,  4,   32, 124, 0,	480, 45, 0, 3,	9,  6, 0}},

	{60000 , {  15, 0, 54054 , 0, 1440,  276, 0, 16,   32, 124, 0,	480, 45, 0, 9,	9,  6, 0}},
	{59940 , {  15, 0, 54000 , 0, 1440,  276, 0, 16,   32, 124, 0,	480, 45, 0, 9,	9,  6, 0}},

	{60000 , {  16, 0, 148500, 0, 1920,  280, 0, 16,   88,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},
	//original value is 148351.64835164835164835164835165
	{59940 , {  16, 0, 148350, 0, 1920,  280, 0, 16,   88,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},

	{50000 , {  17, 0, 27000 , 0,  720,  144, 0,  4,   12,	64, 0,	576, 49, 0, 3,	5,  5, 0}},

	{50000 , {  18, 0, 27000 , 0,  720,  144, 0, 16,   12,	64, 0,	576, 49, 0, 9,	5,  5, 0}},

	{50000 , {  19, 0, 74250 , 0, 1280,  700, 0, 16,  440,	40, 1,	720, 30, 0, 9,	5,  5, 1}},

	/* Not Support - lewc(mHSyncOffset) is 9bit only.
	{50000 , {  20, 0, 74250 , 1, 1920,  720, 0, 16,  528,	44, 1,	540, 22, 0, 9,	2,  5, 1}},
	*/
	{50000 , {  21, 1, 27000 , 1, 1440,  288, 0,  4,   24, 126, 0,	288, 24, 0, 3,	2,  3, 0}},

	{50000 , {  22, 1, 27000 , 1, 1440,  288, 0, 16,   24, 126, 0,	288, 24, 0, 9,	2,  3, 0}},

	{50080 , {  23, 1, 27000 , 0, 1440,  288, 0,  4,   24, 126, 0,	288, 24, 0, 3,	2,  3, 0}},
	{49920 , {  23, 1, 27000 , 0, 1440,  288, 0,  4,   24, 126, 0,	288, 25, 0, 3,	3,  3, 0}},
	{49761 , {  23, 1, 27000 , 0, 1440,  288, 0,  4,   24, 126, 0,	288, 26, 0, 3,	4,  3, 0}},

	{50080 , {  24, 1, 27000 , 0, 1440,  288, 0, 16,   24, 126, 0,	288, 24, 0, 9,	2,  3, 0}},
	{49920 , {  24, 1, 27000 , 0, 1440,  288, 0, 16,   24, 126, 0,	288, 25, 0, 9,	3,  3, 0}},
	{49761 , {  24, 1, 27000 , 0, 1440,  288, 0, 16,   24, 126, 0,	288, 26, 0, 9,	4,  3, 0}},

	{50000 , {  25, 0, 54000 , 1, 2880,  576, 0,  4,   48, 252, 0,	288, 24, 0, 3,	2,  3, 0}},

	{50000 , {  26, 0, 54000 , 1, 2880,  576, 0, 16,   48, 252, 0,	288, 24, 0, 9,	2,  3, 0}},

	{50080 , {  27, 0, 54000 , 0, 2880,  576, 0,  4,   48, 252, 0,	288, 24, 0, 3,	2,  3, 0}},
	{49920 , {  27, 0, 54000 , 0, 2880,  576, 0,  4,   48, 252, 0,	288, 25, 0, 3,	3,  3, 0}},
	{49761 , {  27, 0, 54000 , 0, 2880,  576, 0,  4,   48, 252, 0,	288, 26, 0, 3,	4,  3, 0}},

	{50000 , {  28, 0, 54000 , 0, 2880,  576, 0, 16,   48, 252, 0,	288, 24, 0, 9,	2,  3, 0}},
	{49920 , {  28, 0, 54000 , 0, 2880,  576, 0, 16,   48, 252, 0,	288, 25, 0, 9,	3,  3, 0}},
	{49761 , {  28, 0, 54000 , 0, 2880,  576, 0, 16,   48, 252, 0,	288, 24, 0, 9,	4,  3, 0}},

	{50000 , {  29, 0, 54000 , 0, 1440,  288, 0,  4,   24, 128, 0,	576, 49, 0, 3,	5,  5, 0}},

	{50000 , {  30, 0, 54000 , 0, 1440,  288, 0, 16,   24, 128, 0,	576, 49, 0, 9,	5,  5, 0}},

	/* Not Support - lewc(mHSyncOffset) is 9bit only.
	{50000 , {  31, 0, 148500, 0, 1920,  720, 0, 16,  528,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},

	{24000 , {  32, 0, 74250 , 0, 1920,  830, 0, 16,  638,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},
	//original value is 74175824.175824175824175824175824
	{23980 , {  32, 0, 74175 , 0, 1920,  830, 0, 16,  638,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},

	{25000 , {  33, 0, 74250 , 0, 1920,  720, 0, 16,  528,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},
	*/
	{30000 , {  34, 0, 74250 , 0, 1920,  280, 0, 16,   88,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},
	//original value is 74175824.175824175824175824175824
	{29970 , {  34, 0, 74175 , 0, 1920,  280, 0, 16,   88,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},

	{60000 , {  35, 0, 108108, 0, 2880,  552, 0,  4,   64, 248, 0,	480, 45, 0, 3,	9,  6, 0}},
	{59940 , {  35, 0, 108000, 0, 2880,  552, 0,  4,   64, 248, 0,	480, 45, 0, 3,	9,  6, 0}},

	{60000 , {  36, 0, 108108, 0, 2880,  552, 0, 16,   64, 248, 0,	480, 45, 0, 9,	9,  6, 0}},
	{59940 , {  36, 0, 108000, 0, 2880,  552, 0, 16,   64, 248, 0,	480, 45, 0, 9,	9,  6, 0}},

	{50000 , {  37, 0, 108000, 0, 2880,  576, 0,  4,   48, 256, 0,	576, 49, 0, 3,	5,  5, 0}},
	{50000 , {  38, 0, 108000, 0, 2880,  576, 0, 16,   48, 256, 0,	576, 49, 0, 9,	5,  5, 0}},

	{50000 , {  39, 0, 72000 , 1, 1920,  384, 0, 16,   32, 168, 1,	540, 85, 0, 9, 23,  5, 0}},

	/* Not Support - lewc(mHSyncOffset) is 9bit only.
	{100000, {  40, 0, 148500, 1, 1920,  720, 0, 16,  528,	44, 1,	540, 22, 0, 9,	2,  5, 1}},
	*/
	{100000, {  41, 0, 148500, 0, 1280,  700, 0, 16,  440,	40, 1,	720, 30, 0, 9,	5,  5, 1}},

	{100000, {  42, 0, 54000 , 0,  720,  144, 0,  4,   12,	64, 0,	576, 49, 0, 3,	5,  5, 0}},

	{100000, {  43, 0, 54000 , 0,  720,  144, 0, 16,   12,	64, 0,	576, 49, 0, 9,	5,  5, 0}},

	{100000, {  44, 1, 54000 , 1, 1440,  288, 0,  4,   24, 126, 0,	288, 24, 0, 3,	2,  3, 0}},

	{100000, {  45, 1, 54000 , 1, 1440,  288, 0, 16,   24, 126, 0,	288, 24, 0, 9,	2,  3, 0}},

	{120000, {  46, 0, 148500, 1, 1920,  280, 0, 16,   88,	44, 1,	540, 22, 0, 9,	2,  5, 1}},
	//original value is 148351.64835164835164835164835165
	{119880, {  46, 0, 148350, 1, 1920,  280, 0, 16,   88,	44, 1,	540, 22, 0, 9,	2,  5, 1}},

	{120000, {  47, 0, 148500, 0, 1280,  370, 0, 16,  110,	40, 1,	720, 30, 0, 9,	5,  5, 1}},
	//original value is 148351.64835164835164835164835165
	{119880, {  47, 0, 148350, 0, 1280,  370, 0, 16,  110,	40, 1,	720, 30, 0, 9,	5,  5, 1}},

	{120000, {  48, 0, 54054 , 0,  720,  138, 0,  4,   16,	62, 0,	480, 45, 0, 3,	9,  6, 0}},
	{119880, {  48, 0, 54000 , 0,  720,  138, 0,  4,   16,	62, 0,	480, 45, 0, 3,	9,  6, 0}},

	{120000, {  49, 0, 54054 , 0,  720,  138, 0, 16,   16,	62, 0,	480, 45, 0, 9,	9,  6, 0}},
	{119880, {  49, 0, 54000 , 0,  720,  138, 0, 16,   16,	62, 0,	480, 45, 0, 9,	9,  6, 0}},

	{120000, {  50, 1, 54054 , 1, 1440,  276, 0,  4,   38, 124, 0,	240, 22, 0, 3,	4,  3, 0}},
	{119880, {  50, 1, 54000 , 1, 1440,  276, 0,  4,   38, 124, 0,	240, 22, 0, 3,	4,  3, 0}},

	{120000, {  51, 1, 54054 , 1, 1440,  276, 0, 16,   38, 124, 0,	240, 22, 0, 9,	4,  3, 0}},
	{119880, {  51, 1, 54000 , 1, 1440,  276, 0, 16,   38, 124, 0,	240, 22, 0, 9,	4,  3, 0}},

	{200000, {  52, 0, 108000, 0,  720,  144, 0,  4,   12,	64, 0,	576, 49, 0, 3,	5,  5, 0}},

	{200000, {  53, 0, 108000, 0,  720,  144, 0, 16,   12,	64, 0,	576, 49, 0, 9,	5,  5, 0}},

	{200000, {  54, 1, 108000, 1, 1440,  288, 0,  4,   24, 126, 0,	288, 24, 0, 3,	2,  3, 0}},

	{200000, {  55, 1, 108000, 1, 1440,  288, 0, 16,   24, 126, 0,	288, 24, 0, 9,	2,  3, 0}},

	{240000, {  56, 0, 108108, 0,  720,  138, 0,  4,   16,	62, 0,	480, 45, 0, 3,	9,  6, 0}},
	{239760, {  56, 0, 108000, 0,  720,  138, 0,  4,   16,	62, 0,	480, 45, 0, 3,	9,  6, 0}},

	{240000, {  57, 0, 108108, 0,  720,  138, 0, 16,   16,	62, 0,	480, 45, 0, 9,	9,  6, 0}},
	{239760, {  57, 0, 108000, 0,  720,  138, 0, 16,   16,	62, 0,	480, 45, 0, 9,	9,  6, 0}},

	{240000, {  58, 1, 108108, 1, 1440,  276, 0,  4,   38, 124, 0,	240, 22, 0, 3,	4,  3, 0}},
	{239760, {  58, 1, 108000, 1, 1440,  276, 0,  4,   38, 124, 0,	240, 22, 0, 3,	4,  3, 0}},

	{240000, {  59, 1, 108108, 1, 1440,  276, 0, 16,   38, 124, 0,	240, 22, 0, 9,	4,  3, 0}},
	{239760, {  59, 1, 108000, 1, 1440,  276, 0, 16,   38, 124, 0,	240, 22, 0, 9,	4,  3, 0}},

	/* Not Support - lewc(mHSyncOffset) is 9bit only.
	{24000 , {  60, 0, 59400 , 0, 1280, 2020, 0, 16, 1760,	40, 1,	720, 30, 0, 9,	5,  5, 1}},
	//original value is 59340.659340659340659340659340659
	{23980 , {  60, 0, 59340 , 0, 1280, 2020, 0, 16, 1760,	40, 1,	720, 30, 0, 9,	5,  5, 1}},

	{25000 , {  61, 0, 74250 , 0, 1280, 2680, 0, 16, 2420,	40, 1,	720, 30, 0, 9,	5,  5, 1}},

	{30000 , {  62, 0, 74250 , 0, 1280, 2020, 0, 16, 1760,	40, 1,	720, 30, 0, 9,	5,  5, 1}},
	//original value is 74175824.175824175824175824175824
	{29970 , {  62, 0, 74175 , 0, 1280, 2020, 0, 16, 1760,	40, 1,	720, 30, 0, 9,	5,  5, 1}},
	*/

	{120000, {  63, 0, 297000, 0, 1920,  280, 0, 16,   88,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},
	{119880, {  63, 0, 296700, 0, 1920,  280, 0, 16,   88,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},

	/* Not Support - lewc(mHSyncOffset) is 9bit only.
	{100000, {  64, 0, 297000, 0, 1920,  720, 0, 16,  528,	44, 1, 1080, 45, 0, 9,	4,  5, 1}},

	{24000 , {  93, 0, 297000, 0, 3840, 1660, 0, 16, 1276,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	//original value is 296703.2967032967032967032967033
	{23980 , {  93, 0, 296700, 0, 3840, 1660, 0, 16, 1276,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},

	{25000 , {  94, 0, 297000, 0, 3840, 1440, 0, 16, 1056,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	*/

	{30000 , {  95, 0, 297000, 0, 3840,  560, 0, 16,  176,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	//original value is 296703.2967032967032967032967033
	{29970 , {  95, 0, 296700, 0, 3840,  560, 0, 16,  176,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},

	/* Not Support - lewc(mHSyncOffset) is 9bit only.
	{24000 , {  98, 0, 297000, 0, 4096, 1404, 0, 16, 1020,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	//original value is 296703.2967032967032967032967033
	{23980 , {  98, 0, 296700, 0, 4096, 1404, 0, 16, 1020,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	{25000 , {  99, 0, 297000, 0, 4096, 1184, 0, 16,  968,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	*/

	{30000 , { 100, 0, 297000, 0, 4096,  304, 0, 16,   88,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	//original value is 296703.2967032967032967032967033
	{29970 , { 100, 0, 296700, 0, 4096,  304, 0, 16,   88,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},

	/* Not Support - lewc(mHSyncOffset) is 9bit only.
	{30000 , { 103, 0, 297000, 0, 3840, 1660, 0, 16, 1276,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	//original value is 296703.2967032967032967032967033
	{29970 , { 103, 0, 296700, 0, 3840, 1660, 0, 16, 1276,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},

	{30000 , { 104, 0, 297000, 0, 3840, 1440, 0, 16, 1056,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	*/
	{30000 , { 105, 0, 297000, 0, 3840,  560, 0, 16,  176,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},
	//original value is 296703.2967032967032967032967033
	{29970 , { 105, 0, 296700, 0, 3840,  560, 0, 16,  176,	88, 1, 2160, 90, 0, 9,	8, 10, 1}},


        /**
		     mCode mPixelClock (Hz *1000)  mHImageSize		  mVBlanking	       mVSyncPolarity
		     |           mInterlaced             mHSyncOffset	     |   mVBorder      |
		     |       |       | mHactive	      |     |  mHSyncPulseWidth  |  mVImageSize|
		     |  mPixelRepetitionInput  mHBorder     |	 |  mHSyncPolarity  |  mVSyncOffset
		     |  |    |       |   | mHBlanking |     |	 |  | mVActive   |  |  |   mVSyncPulseWidth */

	{60000 , {1024, 0,  88200, 0, 1920,   64, 0, 16,   28,	 8, 1,	720, 21, 0, 9,	9,  3, 1}},

	{0     , {  0,	0,	0, 0,	 0,    0, 0,  0,    0,	 0, 0,	  0,  0, 0, 0,	0,  0, 0}},
};


/**
 * @short Get the DTD structure that contains the video parameters
 * @param[in] code VIC code to search for
 * @param[in] refreshRate
 * @return returns a pointer to the DTD structure or NULL if not supported.
 * If refreshRate=0 then the first (default) parameters are returned for the VIC code.
 */
static dtd_t * get_detailed_timing_pointer(unsigned int code, unsigned int refreshRate)
{
	dtd_t *detailed_timing = NULL;
	int i, arrary_size = ARRAY_SIZE(tcc_detailed_timing_table);

	for(i = 0; i < arrary_size && tcc_detailed_timing_table[i].dtd.mCode != 0; i++){
		if(tcc_detailed_timing_table[i].dtd.mCode == code){
			if(!refreshRate){
				detailed_timing =  &tcc_detailed_timing_table[i].dtd;
				break;
			}
			if(refreshRate == tcc_detailed_timing_table[i].refresh_rate){
				detailed_timing =  &tcc_detailed_timing_table[i].dtd;
				break;
			}
		}
	}
	return detailed_timing;
}

int hdmi_dtd_fill(dtd_t * target_detailed_timing, unsigned int code, unsigned int refreshRate)
{
	int ret = -1;
        dtd_t * detailed_timing = NULL;

	do {
		if(target_detailed_timing == NULL) {
			pr_err("[ERROR][HDMI_V14]%s target_detailed_timing is NULL\r\n", __func__);
			break;
		}
		detailed_timing = get_detailed_timing_pointer(code, refreshRate);
		if(detailed_timing == NULL){
			pr_err("[ERROR][HDMI_V14]%s failed to get detailed timing pointer\r\n", __func__);
			break;
		}

		memcpy(target_detailed_timing, detailed_timing, sizeof(dtd_t));
		ret = 0;
	}while(0);

	return ret;
}

unsigned int hdmi_dtd_get_refresh_rate(dtd_t *dtd){
        int i;
        for(i = 0; tcc_detailed_timing_table[i].dtd.mCode != 0; i++){
                if(tcc_detailed_timing_table[i].dtd.mCode == dtd->mCode){
                        if(dtd->mPixelClock == 0 || tcc_detailed_timing_table[i].dtd.mPixelClock == dtd->mPixelClock)
                                return tcc_detailed_timing_table[i].refresh_rate;
                }
        }
        return 0;
}

unsigned int hdmi_dtd_get_vactive(videoParams_t *videoParam)
{
        unsigned int vactive = 0;

        do {
		if(videoParam == NULL) {
			pr_err("[ERROR][HDMI_V14]%s videoParam is NULL\r\n", __func__);
			break;
		}
                vactive = videoParam->mDtd.mVActive;
		if(videoParam->mHdmiVideoFormat == 2 && videoParam->m3dStructure == 0) {
                        /* Check HDMI 3D-Frame Packing */
                        if(videoParam->mDtd.mInterlaced) {
                                vactive = (vactive << 2) + (3*videoParam->mDtd.mVBlanking+2);
                        } else {
                                vactive = (vactive << 1) + videoParam->mDtd.mVBlanking;
                        }
		} else if(videoParam->mDtd.mInterlaced) {
                        vactive <<= 1;
                }
        } while(0);
        return vactive;
}

int hdmi_dtd_get_display_param(videoParams_t *videoParam,
			struct lcdc_timimg_parms_t *lcdc_timimg_parms)
{
	int ret = -1;
	int interlaced_mode;
        unsigned int vactive = 0;
	unsigned short mHActive, mHBlanking, mHSyncOffset, mHSyncPulseWidth, temp_val;


	dtd_t *detailed_timing = NULL;
        do {
		if(videoParam == NULL) {
			pr_err("[ERROR][HDMI_V14]%s videoParam is NULL\r\n", __func__);
			break;
		}

		detailed_timing = &videoParam->mDtd;
		if(detailed_timing == NULL) {
			pr_err("[ERROR][HDMI_V14]%s detailed_timing is NULL\r\n", __func__);
			break;
		}

		if(lcdc_timimg_parms == NULL) {
			pr_err("[ERROR][HDMI_V14]%s lcdc_timimg_parms is NULL\r\n", __func__);
			break;
		}
		vactive = hdmi_dtd_get_vactive(videoParam);

		mHActive = detailed_timing->mHActive;
		mHBlanking = detailed_timing->mHBlanking;
		mHSyncOffset = detailed_timing->mHSyncOffset;
		mHSyncPulseWidth = detailed_timing->mHSyncPulseWidth;

		// Timing
		memset(lcdc_timimg_parms, 0, sizeof(struct lcdc_timimg_parms_t));
		lcdc_timimg_parms->iv = detailed_timing->mVSyncPolarity?0:1;  /** 0 for Active low, 1 active high */
		lcdc_timimg_parms->ih = detailed_timing->mHSyncPolarity?0:1;  /** 0 for Active low, 1 active high */
		lcdc_timimg_parms->dp = detailed_timing->mPixelRepetitionInput;

		/* 3d data frame packing is transmitted as a progressive format */
		interlaced_mode = detailed_timing->mInterlaced;

		if(interlaced_mode)
			lcdc_timimg_parms->tv = 1;
		else
			lcdc_timimg_parms->ni = 1;

		lcdc_timimg_parms->tft = 0;
		lcdc_timimg_parms->lpw = (mHSyncPulseWidth>0)?(mHSyncPulseWidth-1):0;
		lcdc_timimg_parms->lpc = (mHActive>0)?(mHActive-1):0;
		temp_val = (mHBlanking - (mHSyncOffset+mHSyncPulseWidth));
		lcdc_timimg_parms->lswc = (temp_val>0)?temp_val-1:0;
		lcdc_timimg_parms->lewc = (mHSyncOffset>0)?(mHSyncOffset-1):0;

		if(interlaced_mode){
			temp_val = detailed_timing->mVSyncPulseWidth << 1;
			lcdc_timimg_parms->fpw = (temp_val>0)?(temp_val-1):0;
			temp_val = ((detailed_timing->mVBlanking - (detailed_timing->mVSyncOffset + detailed_timing->mVSyncPulseWidth)) << 1);
			lcdc_timimg_parms->fswc = (temp_val>0)?(temp_val-1):0;
			lcdc_timimg_parms->fewc = (detailed_timing->mVSyncOffset << 1);
			lcdc_timimg_parms->fswc2 = lcdc_timimg_parms->fswc+1;
			lcdc_timimg_parms->fewc2 = (lcdc_timimg_parms->fewc>0)?(lcdc_timimg_parms->fewc-1):0;
			if(detailed_timing->mCode == 39) {
				/* 1920x1080@50i 1250 vtotal */
				lcdc_timimg_parms->fewc -= 2;
			}
		}
		else {
			lcdc_timimg_parms->fpw = (detailed_timing->mVSyncPulseWidth>0)?(detailed_timing->mVSyncPulseWidth-1):0;
			temp_val = (detailed_timing->mVBlanking - (detailed_timing->mVSyncOffset + detailed_timing->mVSyncPulseWidth));
			lcdc_timimg_parms->fswc = (temp_val>0)?(temp_val-1):0;
			lcdc_timimg_parms->fewc = (detailed_timing->mVSyncOffset>0)?(detailed_timing->mVSyncOffset-1):0;
			lcdc_timimg_parms->fswc2 = lcdc_timimg_parms->fswc;
			lcdc_timimg_parms->fewc2 = lcdc_timimg_parms->fewc;
		}

		// Check 3D Frame Packing
	        if(videoParam->mHdmiVideoFormat == 2 && videoParam->m3dStructure == 0) {
	                lcdc_timimg_parms->framepacking = 1;
	        }
	        else if(videoParam->mHdmiVideoFormat == 2 && (videoParam->m3dStructure == 6 || videoParam->m3dStructure == 8)) {
	                // SBS is 2, TAB is 3
	                lcdc_timimg_parms->framepacking = (videoParam->m3dStructure == 8)?2:3;
	        }else {
	                lcdc_timimg_parms->framepacking = 0;
	        }

		/* Common Timing Parameters */
		lcdc_timimg_parms->flc = (vactive>0)?(vactive-1):0;
		lcdc_timimg_parms->fpw2 = lcdc_timimg_parms->fpw;
		lcdc_timimg_parms->flc2 = lcdc_timimg_parms->flc;
		ret = 0;
	} while(0);
	return ret;
}

int hdmi_dtb_get_extra_data(videoParams_t *videoParam,
						struct tcc_fb_extra_data *tcc_fb_extra_data)
{
	int ret = -1;
	do {
		if(videoParam == NULL) {
			pr_err("[ERROR][HDMI_V14]%s videoParam is NULL\r\n", __func__);
			break;
		}

		if(tcc_fb_extra_data == NULL) {
			pr_err("[ERROR][HDMI_V14]%s tcc_fb_extra_data is NULL\r\n", __func__);
			break;
		}


		memset(tcc_fb_extra_data, 0, sizeof(struct tcc_fb_extra_data));
		switch(videoParam->mEncodingOut) {
			case YCC444:
				// YCC444
				tcc_fb_extra_data->pxdw = 12;
				tcc_fb_extra_data->swapbf = 4;
				break;

			case YCC422:
				// YCC422
				tcc_fb_extra_data->pxdw = 8;
				tcc_fb_extra_data->swapbf = 0;
				break;

			case RGB:
			default:
				tcc_fb_extra_data->pxdw = 12;
				tcc_fb_extra_data->swapbf = 0;
				break;
		}

		  // R2Y
        	if(videoParam->mEncodingOut != RGB) {
	                tcc_fb_extra_data->r2y = 1;
	                switch(videoParam->mColorimetry)
	                {
	                                case ITU601:
	                                default:
	                                        tcc_fb_extra_data->r2ymd = 0;
	                                        break;
	                                case ITU709:
	                                        tcc_fb_extra_data->r2ymd = 2;
	                                        break;
	                                case EXTENDED_COLORIMETRY:
	                                        switch(videoParam->mExtColorimetry) {
	                                                case XV_YCC601:
	                                                case S_YCC601:
	                                                case ADOBE_YCC601:
	                                                default:
	                                                        tcc_fb_extra_data->r2ymd = 0;
	                                                        break;
	                                                case XV_YCC709:
	                                                        tcc_fb_extra_data->r2ymd = 2;
	                                                        break;
	                                                case BT2020YCCBCR:
	                                                case BT2020YCBCR:
	                                                        tcc_fb_extra_data->r2ymd = 4;
	                                                        break;
	                                        }
	                                        break;
	                }
	        }
	        else {
	                tcc_fb_extra_data->r2y = 0;
	        }
		ret = 0;
	}
	while(0);
	return ret;
}


