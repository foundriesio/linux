/*
 * Copyright (C) Telechips
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
#ifndef __TCC_FB_ADDRESS__

#ifndef __TCC_VIOCMG_H__
#define __TCC_VIOCMG_H__

#include "viocmg_types.h"

// Caller ID
#define VIOCMG_CALLERID_FB              0x10000001
#define VIOCMG_CALLERID_SCALER_0        0x10000008
#define VIOCMG_CALLERID_VOUT            0x10000010
#define VIOCMG_CALLERID_OVERLAY         0x10000012
#define VIOCMG_CALLERID_FB_INTERFACE    0x10000014        
#define VIOCMG_CALLERID_FB_OUTPUT       0x10000016
#define VIOCMG_CALLERID_IOCTL           0x10000020


#define VIOCMG_CALLERID_API             0x20000001        
#define VIOCMG_CALLERID_REAR_CAM        0xF0000001


#define VIOCMG_CALLERID_NONE            0xFFFFFFFF


#define VIOCMG_REAR_MODE_STOP           0
#define VIOCMG_REAR_MODE_PREPARE        1
#define VIOCMG_REAR_MODE_RUN            2

// Common API
extern struct viocmg_soc *viocmg_get_soc(void);

extern unsigned int viocmg_get_main_display_id(void);
extern unsigned int viocmg_get_main_display_port(void);
extern unsigned int viocmg_get_main_display_ovp(void);

// Feature API
extern unsigned int viocmg_is_feature_rear_cam_enable(void);
extern unsigned int viocmg_is_feature_rear_cam_use_viqe(void);
extern unsigned int viocmg_is_feature_rear_cam_use_parking_line(void);


// Lock API
int viocmg_lock_viqe(unsigned int caller_id);
void viocmg_free_viqe(unsigned int caller_id);



// WMIX
extern int  viocmg_set_wmix_position(unsigned int caller_id, unsigned int wmix_id, unsigned int wmix_channel, unsigned int nX, unsigned int nY);
extern void viocmg_get_wmix_position(unsigned int caller_id, unsigned int wmix_id, unsigned int wmix_channel, unsigned int *nX, unsigned int *nY);
extern int  viocmg_set_wmix_ovp(unsigned int caller_id, unsigned int wmix_id, unsigned int ovp);
extern void viocmg_get_wmix_ovp(unsigned int wmix_id, unsigned int *ovp);
extern int  viocmg_set_wmix_chromakey_enable(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int enable);
extern void viocmg_get_wmix_chromakey_enable(unsigned int wmix_id, unsigned int layer, unsigned int *enable);
extern int  viocmg_set_wmix_chromakey(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int enable, unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB, unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB);
extern int  viocmg_set_wmix_overlayalphavaluecontrol(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int region, unsigned int acon0, unsigned int acon1 );
extern int  viocmg_set_wmix_overlayalphacolorcontrol(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int region, unsigned int ccon0, unsigned int ccon1);
extern int  viocmg_set_wmix_overlayalpharopmode(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int opmode );
extern int  viocmg_set_wmix_overlayalphaselection(unsigned int caller_id, unsigned int wmix_id, unsigned int layer,unsigned int asel );
extern int  viocmg_set_wmix_overlayalphavalue(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int alpha0, unsigned int alpha1 );


// Rear cam API
extern unsigned int     viocmg_get_rear_cam_ovp(void);

extern void             viocmg_save_wmix_default(void);
extern void             viocmg_set_rear_cam_mode(unsigned int caller_id, unsigned int mode);
extern unsigned int     viocmg_get_rear_cam_mode(void);
extern unsigned int     viocmg_get_rear_cam_vin_rdma_id(void);
extern unsigned int     viocmg_get_rear_cam_vin_id(void);
extern unsigned int     viocmg_get_rear_cam_scaler_id(void);
extern unsigned int     viocmg_get_rear_cam_vin_wmix_id(void);
extern unsigned int     viocmg_get_rear_cam_vin_wdma_id(void);

extern unsigned int     viocmg_get_rear_cam_display_rdma_id(void);
extern void             viocmg_set_rear_cam_suspend(unsigned int suspend);
extern void             viocmg_set_rear_cam_gearstatus_api(void* api);
extern void             viocmg_set_rear_cam_dmastatus_api(void* api);


#endif // __TCC_VIOCMG_H__
