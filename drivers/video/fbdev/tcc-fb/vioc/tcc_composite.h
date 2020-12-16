/*
 * Copyright (C) Telechips, Inc.
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
#ifndef	_TCC_COMPOSITE_H_
#define	_TCC_COMPOSITE_H_


extern struct tcc_dp_device *tca_fb_get_displayType(TCC_OUTPUT_TYPE check_type);
extern void tca_scale_display_update(struct tcc_dp_device *pdp_data,
				     struct tcc_lcdc_image_update *ImageInfo);
extern void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo,
					  int specific_pclk);
extern void tca_vioc_displayblock_powerOff(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_disable(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_ctrl_set(unsigned int outDevice,
					   struct tcc_dp_device *pDisplayInfo,
					   stLTIMING *pstTiming,
					   stLCDCTR *pstCtrl);
extern void tca_fb_attach_start(struct tccfb_info *info);
extern int tca_fb_attach_stop(struct tccfb_info *info);

#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
extern int hdmi_get_hotplug_status(void);
#elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_AUTO)
#error "display dual auto mode needs hdmi v2.0"
#endif

extern char fb_power_state;


#endif /*_TCC_COMPOSITE_H_*/