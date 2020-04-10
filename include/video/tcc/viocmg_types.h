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
#ifndef __TCC_VIOCMG_TYPES_H__
#define __TCC_VIOCMG_TYPES_H__

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
#include <mach/vioc_wmix.h>
#else
#include "vioc_wmix.h"
#endif

struct rearcam_path_info
{
    unsigned int vin;
    unsigned int vin_rdma;
    unsigned int vin_wmix;
    unsigned int vin_wdma;
    unsigned int vin_scaler;
    unsigned int vin_deints; // 0: none, 1: deints, 2: viqe
    unsigned int vin_ovp;

    unsigned int display_rdma;
    unsigned int display_wmix;
    unsigned int display_ovp;
};

#define REARCAM_BK_WMIX_DISPLAY 0
#define REARCAM_BK_WMIX_VIN     1
#define REARCAM_BK_WMIX_MAX     2

struct rearcam_backup_components
{
    void __iomem *wmix[REARCAM_BK_WMIX_MAX];  // 0: display, 1: vin
};

// PMAP INFO
struct viocmg_dt_info
{
        // fbdisplay
        unsigned int main_display_id;
        unsigned int main_display_port;
        unsigned int main_display_ovp;  // default ovp

        // feature!
        unsigned int feature_rear_cam_enable;
        unsigned int feature_rear_cam_use_viqe;
        unsigned int feature_rear_cam_viqe_mode;      //0: VIOC_VIQE_DEINTL_MODE_BYPASS, 1: VIOC_VIQE_DEINTL_MODE_2D 2: VIOC_VIQE_DEINTL_MODE_3D
        unsigned int feature_rear_cam_use_parking_line;

        // camera hw
        unsigned int rear_cam_cifport;
        unsigned int rear_cam_vin_vin;
        unsigned int rear_cam_vin_rdma;
        unsigned int rear_cam_vin_wmix;
        unsigned int rear_cam_vin_wdma;
        unsigned int rear_cam_vin_scaler;
        unsigned int rear_cam_display_rdma;
        unsigned int rear_cam_gear_port;
	unsigned int rear_cam_cam_config;

        // rear cam 
        unsigned int rear_cam_mode; //0: normal mode, 1: rear cam mode   further use
        unsigned int rear_cam_ovp;

        #if 0
        // rear cam preview
        unsigned int rear_cam_preview_x;
        unsigned int rear_cam_preview_y;
        unsigned int rear_cam_preview_width;
        unsigned int rear_cam_preview_height;
        unsigned int rear_cam_preview_format;

        unsigned int rear_cam_preview_additional_width;
        unsigned int rear_cam_preview_additional_height;

        // rear cam parking line
        unsigned int rear_cam_parking_line_x;
        unsigned int rear_cam_parking_line_y;
        unsigned int rear_cam_parking_line_width;
        unsigned int rear_cam_parking_line_height;
        unsigned int rear_cam_parking_line_format;
        #endif

};


// VIOCMG 

struct viocmg_component_info
{
    unsigned int owner;
    unsigned int patchselection;
    struct completion completion;
};



struct wmix_enable_entry
{
    struct list_head list;
    unsigned int id;
    unsigned int layer;
    unsigned int caller_id;
};

struct viocmg_wmix_enable_lists
{
    struct wmix_enable_entry chromakey;
    struct wmix_enable_entry alphablend;    
};


struct viocmg_components
{
    struct viocmg_component_info scaler[4];
    struct viocmg_component_info viqe;
    struct viocmg_component_info deints;
};

struct viocmg_hw_reg
{
    // reg
    void __iomem *rdma;
    void __iomem *wdma;
    ///void __iomem *wmix;
    unsigned int display_wmix;
	unsigned int camera_config;
};



struct viocmg_soc {
		// suspend..
        unsigned int suspend;
        unsigned int reardrv_suspend;
        unsigned int (*reardrv_get_gearstatus)(void);
        unsigned int (*reardrv_get_dmastatus)(void);

        struct viocmg_dt_info *viocmg_dt_info;

        // irq

        // reg
        struct viocmg_hw_reg reg;

        struct device *dev;

        // use count


        // sync
        struct mutex mutex_lock;
        struct mutex mutex_wmix;

        spinlock_t viqe_spin_lock;
        spinlock_t int_reg_spin_lock;
        spinlock_t rdy_reg_spin_lock;


        // component 

        struct viocmg_components components;
        struct viocmg_wmix_enable_lists wmix_enable_lists;



        // rearcam
        struct rearcam_backup_components rear_cam_backup_components;

};


#endif // __TCC_VIOCMG_TYPES_H__
