/*
 * linux/include/video/tcc/vioc_fifo.h
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
#ifndef __VIOC_EXTRA_H__
#define	__VIOC_EXTRA_H__

/*
 * Register offset
 */
#define OPL_SIZE		(0x10)
#define OPL_BLANK		(0x08)

/*
 * Output Channel Status Register 
 */
#define OPL_FK_SHIFT			(13)
#define OPL_PR_SHIFT			(12)
#define OPL_PR_NoHDCP_SHIFT		(11)
#define PR_SUB_SHIFT			(9)
#define PR_MAIN_SHIFT			(8)
#define PR_COMPONENT_SHIFT		(2)
#define PR_COMPOSITE_SHIFT		(1)
#define PR_HDMI_SHIFT			(0)

#define OPL_FK_MASK				(0x1 << OPL_FK_SHIFT)
#define OPL_PR_MASK				(0x1 << OPL_PR_SHIFT)
#define OPL_PR_NoHDCP_MASK		(0x1 << OPL_PR_NoHDCP_SHIFT)
#define PR_SUB_MASK				(0x1 << PR_SUB_SHIFT)
#define PR_MAIN_MASK			(0x1 << PR_MAIN_SHIFT)
#define PR_COMPONENT_MASK		(0x1 << PR_COMPONENT_SHIFT)
#define PR_COMPOSITE_MASK		(0x1 << PR_COMPOSITE_SHIFT)
#define PR_HDMI_MASK			(0x1 << PR_HDMI_SHIFT)

#define PR_CH_STATUS_MASK		(PR_SUB_MASK|PR_MAIN_MASK|PR_COMPONENT_MASK|PR_COMPOSITE_MASK|PR_HDMI_MASK)

/*
 * Output Channel Size Register
 */
#define CH_SIZE_HEIGHT_SHIFT		(16)
#define CH_SIZE_WIDTH_SHIFT			(0)

#define CH_SIZE_HEIGHT_MASK			(0xFFFF << CH_SIZE_HEIGHT_SHIFT)
#define CH_SIZE_WIDTH_MASK			(0xFFFF << CH_SIZE_WIDTH_SHIFT)

extern void VIOC_OUTCFG_set_disp_size(unsigned int width, unsigned int height);
extern unsigned int VIOC_OUTCFG_get_disp_size(void);
extern int VIOC_OUTCFG_dispStatus(int hdmi, int composite, int component, int sub_off, int main_off);
extern int PlayReady_main_off(void);
extern int PlayReady_sub_off(void);
extern void OPL_PlayReady_NoHDCP_set(unsigned long val);
extern unsigned long OPL_PlayReady_NoHDCP_get(void);
extern void OPL_PlayReady_set(unsigned long ch);
extern unsigned long OPL_PlayReady_get(void);
extern void OPL_FixedKey_set(unsigned long ch);
extern unsigned long OPL_FixedKey_get(void);
#endif
