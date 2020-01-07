/****************************************************************************
 * One line to give the program's name and a brief idea of what it does.
 * Copyright (C) 2013 Telechips Inc.
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
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 * ****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
#include <mach/bsp.h>
#include <mach/io.h>
#include <mach/tca_lcdc.h>
#include <mach/vioc_outcfg.h>
#include <mach/vioc_rdma.h>
#include <mach/vioc_wdma.h>
#include <mach/vioc_wmix.h>
#include <mach/vioc_disp.h>
#include <mach/vioc_global.h>
#include <mach/vioc_vin.h>
#include <mach/vioc_viqe.h>
#include <mach/vioc_config.h>
#include <mach/vioc_api.h>
#include <mach/tcc_fb.h>
#include <mach/tccfb_ioctrl.h>
#include <mach/vioc_scaler.h>
#else
#include <video/tcc/tcc_types.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_scaler.h>
#endif
#include <soc/tcc/pmap.h>
#include <video/tcc/viocmg.h>

#define VIOMG_DEBUG 		0
#define dprintk(msg...) 	if(VIOMG_DEBUG) { printk("VIOCMG: " msg); }
#define ALIGNED_BUFF(buf, mul) ( ( (unsigned int)buf + (mul-1) ) & ~(mul-1) )


#ifndef CONFIG_VOUT_KEEP_VIDEO_LAYER
extern void tcc_vout_disable_hwlayer( unsigned int type );
#endif

static struct viocmg_soc *viocmg = NULL;


struct viocmg_soc *viocmg_get_soc(void)
{
    if(viocmg == NULL) {
        printk(" \r\n"
               " Error.. VIOCMG is NULL..!!!\r\n");
    }
    return viocmg;
}
EXPORT_SYMBOL_GPL(viocmg_get_soc);

unsigned int viocmg_get_main_display_id(void)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();

    return viocmg->viocmg_dt_info->main_display_id;
}
EXPORT_SYMBOL_GPL(viocmg_get_main_display_id);

unsigned int viocmg_get_main_display_port(void)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();

    return viocmg->viocmg_dt_info->main_display_port;
}
EXPORT_SYMBOL_GPL(viocmg_get_main_display_port);

unsigned int viocmg_get_main_display_ovp(void)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();

    return viocmg->viocmg_dt_info->main_display_ovp;
}
EXPORT_SYMBOL_GPL(viocmg_get_main_display_ovp);


unsigned int viocmg_is_feature_rear_cam_enable(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->feature_rear_cam_enable;
}
EXPORT_SYMBOL_GPL(viocmg_is_feature_rear_cam_enable);

unsigned int viocmg_is_feature_rear_cam_use_viqe(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->feature_rear_cam_use_viqe;
}
EXPORT_SYMBOL_GPL(viocmg_is_feature_rear_cam_use_viqe);

unsigned int viocmg_is_feature_rear_cam_use_parking_line(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->feature_rear_cam_use_parking_line;
}
EXPORT_SYMBOL_GPL(viocmg_is_feature_rear_cam_use_parking_line);


/*
 * viocmg_lock_viqe
 *  lock viqe component 
 *   0: success 
 *   -1: false.. viqe is used by another. or rear camera mode..!
 */
int viocmg_lock_viqe(unsigned int caller_id)
{
	int result = -1;

	struct viocmg_soc *viocmg = viocmg_get_soc();
	VIOC_IREQ_CONFIG * pVIOCConfig = (VIOC_IREQ_CONFIG *)viocmg->reg.camera_config;

	mutex_lock(&viocmg->mutex_viqe);

	if(caller_id != VIOCMG_CALLERID_REAR_CAM && viocmg->viocmg_dt_info->feature_rear_cam_use_viqe && viocmg->viocmg_dt_info->rear_cam_mode > 0 ) {
		dprintk("warring rear camera using viqe\r\n");
		goto END_PROCESS;
	}

	if(viocmg->components.viqe.owner != VIOCMG_CALLERID_NONE && viocmg->components.viqe.owner != caller_id) {
		dprintk("caller[0x%x] warring viqe is already locked.!\r\n", caller_id);
		goto END_PROCESS;
	}

	viocmg->components.viqe.owner = caller_id;
	dprintk("caller[0x%x] lock viqe\r\n", caller_id);

	if(caller_id == VIOCMG_CALLERID_REAR_CAM) {
		VIOC_PlugInOutCheck vioc_plug_in;

		#ifndef CONFIG_VOUT_KEEP_VIDEO_LAYER
		tcc_vout_disable_hwlayer(0);
		#endif

		VIOC_CONFIG_Device_PlugState(VIOC_VIQE, &vioc_plug_in);

		if((vioc_plug_in.connect_device != VIOC_VIQE_VIN_00) \
		&& (vioc_plug_in.connect_device != VIOC_VIQE_VIN_03) \
		&& (vioc_plug_in.connect_statue != VIOC_PATH_DISCONNECTED)) {
			VIOC_CONFIG_PlugOut(VIOC_VIQE);
			VIOC_CONFIG_SWReset(pVIOCConfig, VIOC_CONFIG_VIQE, viocmg->viocmg_dt_info->rear_cam_cam_config, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pVIOCConfig, VIOC_CONFIG_VIQE, viocmg->viocmg_dt_info->rear_cam_cam_config, VIOC_CONFIG_CLEAR);
		}
	}
	result = 0;

END_PROCESS:
	mutex_unlock(&viocmg->mutex_viqe);
	return result;
}
EXPORT_SYMBOL_GPL(viocmg_lock_viqe);


/*
 * viocmg_free_viqe
 *  unlock viqe component 
 *   0: success 
 *   -1: false.. viqe is used by another. 
 */
void viocmg_free_viqe(unsigned int caller_id)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
	VIOC_IREQ_CONFIG * pVIOCConfig = (VIOC_IREQ_CONFIG *)viocmg->reg.camera_config;

        mutex_lock(&viocmg->mutex_viqe);
        if(viocmg->components.viqe.owner == caller_id) {
                viocmg->components.viqe.owner = VIOCMG_CALLERID_NONE;
                complete(&viocmg->components.viqe.completion);
                dprintk("caller[0x%x] free viqe component\r\n", caller_id);
        }

        mutex_unlock(&viocmg->mutex_viqe);
}
EXPORT_SYMBOL_GPL(viocmg_free_viqe);



static int tcc_viocmg_suspend(struct platform_device *pdev, pm_message_t state)
{
        struct viocmg_soc *viocmg = platform_get_drvdata(pdev); 

        viocmg->suspend = 1;
        return 0;
}

static int tcc_viocmg_resume(struct platform_device *pdev)
{
        struct viocmg_soc *viocmg = platform_get_drvdata(pdev); 

        if(viocmg->suspend) {
                viocmg->suspend = 0;
        }

        return 0;
}

static void tcc_viocmg_dump_all(struct platform_device *pdev)
{
        struct viocmg_soc *viocmg = platform_get_drvdata(pdev);

        printk("viocmg_dump\r\n");
        printk("main_display_id = %d\r\n", viocmg->viocmg_dt_info->main_display_id);
        printk("main_display_port = %d\r\n", viocmg->viocmg_dt_info->main_display_port);
        printk("main_display_ovp = %d\r\n", viocmg->viocmg_dt_info->main_display_ovp);
        
        printk("feature_rear_view_enable = %d\r\n", viocmg->viocmg_dt_info->feature_rear_cam_enable);
        printk("feature_rear_view_use_viqe = %d\r\n", viocmg->viocmg_dt_info->feature_rear_cam_use_viqe);
        printk("feature_rear_view_viqe_mode = %d\r\n", viocmg->viocmg_dt_info->feature_rear_cam_viqe_mode);
        printk("feature_rear_view_use_parking_line = %d\r\n", viocmg->viocmg_dt_info->feature_rear_cam_use_parking_line);
        
        printk("rear_cam_cifport = %d\r\n", viocmg->viocmg_dt_info->rear_cam_cifport);
        printk("rear_cam_vin_vin = %d\r\n", viocmg->viocmg_dt_info->rear_cam_vin_vin);   
        printk("rear_cam_vin_rdma = %d\r\n", viocmg->viocmg_dt_info->rear_cam_vin_rdma);
        printk("rear_cam_vin_wmix = %d\r\n", viocmg->viocmg_dt_info->rear_cam_vin_wmix);        
        printk("rear_cam_vin_wdma = %d\r\n", viocmg->viocmg_dt_info->rear_cam_vin_wdma);
        printk("rear_cam_vin_scaler = %d\r\n", viocmg->viocmg_dt_info->rear_cam_vin_scaler);
        printk("rear_cam_display_rdma = %d\r\n", viocmg->viocmg_dt_info->rear_cam_display_rdma);
        
        printk("rear_cam_mode = %d\r\n", viocmg->viocmg_dt_info->rear_cam_mode);
        printk("rear_cam_ovp = %d\r\n", viocmg->viocmg_dt_info->rear_cam_ovp);

        #if 0
        printk("rear_cam_preview_x = %d\r\n", viocmg->viocmg_dt_info->rear_cam_preview_x);
        printk("rear_cam_preview_y = %d\r\n", viocmg->viocmg_dt_info->rear_cam_preview_y);
        printk("rear_cam_preview_width = %d\r\n", viocmg->viocmg_dt_info->rear_cam_preview_width);
        printk("rear_cam_preview_height = %d\r\n", viocmg->viocmg_dt_info->rear_cam_preview_height);
        printk("rear_cam_preview_format = %d\r\n", viocmg->viocmg_dt_info->rear_cam_preview_format);
        
        printk("rear_cam_preview_additional_width = %d\r\n", viocmg->viocmg_dt_info->rear_cam_preview_additional_width);
        printk("rear_cam_preview_additional_height = %d\r\n", viocmg->viocmg_dt_info->rear_cam_preview_additional_height);
        
        printk("rear_cam_parking_line_x = %d\r\n", viocmg->viocmg_dt_info->rear_cam_parking_line_x);
        printk("rear_cam_parking_line_y = %d\r\n", viocmg->viocmg_dt_info->rear_cam_parking_line_y);
        printk("rear_cam_parking_line_width = %d\r\n", viocmg->viocmg_dt_info->rear_cam_parking_line_width);
        printk("rear_cam_parking_line_height = %d\r\n", viocmg->viocmg_dt_info->rear_cam_parking_line_height);
        printk("rear_cam_parking_line_format = %d\r\n", viocmg->viocmg_dt_info->rear_cam_parking_line_format);
        #endif

}

static int tcc_viocmg_parse_fbdisplay(struct platform_device *pdev)
{
        int result = -1;
        unsigned int val;
        struct device_node *np = pdev->dev.of_node;
        struct device_node *np_fbdisplay;

        struct viocmg_soc *viocmg = platform_get_drvdata(pdev);
        
        np_fbdisplay = of_parse_phandle(np, "telechips,viocmg-fbdisplay", 0);
        if(np_fbdisplay < 1) {
                pr_err("count not find telechips,viocmg-fbdisplay\r\n");
                result = -ENODEV;
                goto END_PROCESS;                
        }

        // main_display_ovp
        if (of_property_read_u32(np_fbdisplay, "telechips,fbdisplay_num", &val)){
                pr_err( "could not find  telechips,fbdisplay_nubmer\n");
                result = -ENODEV;
                goto END_PROCESS;
        }

        viocmg->viocmg_dt_info->main_display_id = val;


        // fbdisplay_port
        if (of_property_read_u32(np_fbdisplay, "telechips,fbdisplay_port", &val)){
                pr_err( "could not find telechips,fbdisplay_port\n");
                viocmg->viocmg_dt_info->main_display_port = 0xFFFFFFFF;
        }
        else  {        
                viocmg->viocmg_dt_info->main_display_port = val;
        }

        // main_display_ovp
        if (of_property_read_u32(np_fbdisplay, "telechips,overlay_priority", &val)){
                pr_err( "could not find  telechips,overlay_priority\n");
                result = -ENODEV;
                goto END_PROCESS;
        }
        
        viocmg->viocmg_dt_info->main_display_ovp = val;

        result = 0;
END_PROCESS:
        return result;
}

static int tcc_viocmg_parse_camera_common(struct platform_device *pdev)
{
        int result = -1;
        unsigned int val;
        struct device_node *np = pdev->dev.of_node;
        struct device_node *np_camera, *np_phandle;

        struct viocmg_soc *viocmg = platform_get_drvdata(pdev);
        
        np_camera = of_parse_phandle(np, "telechips,viocmg-camera", 0);
        if(np_camera < 1) {
                pr_err("count not find telechips,viocmg-fbdisplay\r\n");
                result = -ENODEV;
                goto END_PROCESS;                
        }
        
        result = of_property_read_u32_index(np_camera,"camera_port",1,&val);
        if(result) {
                pr_err( "could not find index from camera_port\n");
                goto END_PROCESS;
        }
        viocmg->viocmg_dt_info->rear_cam_cifport = val;
        
        result = of_property_read_u32_index(np_camera,"camera_wmixer",1,&val);
        if(result) {
                pr_err( "could not find index from camera_wmixer\n");
                goto END_PROCESS;
        }

        viocmg->viocmg_dt_info->rear_cam_vin_wmix = val;

        result = of_property_read_u32_index(np_camera,"camera_wdma",1,&val);
        if(result) {
                pr_err( "could not find index from camera_wdma\n");
                goto END_PROCESS;
        }
        viocmg->viocmg_dt_info->rear_cam_vin_wdma = val;

        result = of_property_read_u32_index(np_camera,"camera_videoin",2,&val);
        if(result) {
                pr_err( "could not find index from camera_videoin\n");
                goto END_PROCESS;
        }
        viocmg->viocmg_dt_info->rear_cam_vin_vin = val;

        result = of_property_read_u32_index(np_camera,"camera_pgl",1,&val);
        if(result) {
                pr_err( "could not find index from camera_pgl\n");
                goto END_PROCESS;
        }
        viocmg->viocmg_dt_info->rear_cam_vin_rdma = val;
        
        result = of_property_read_u32_index(np_camera,"camera_scaler",1,&val);
        if(result) {
                pr_err( "could not find index from camera_scaler\n");
                goto END_PROCESS;
        }

        viocmg->viocmg_dt_info->rear_cam_vin_scaler = val;
                   
	result = of_property_read_u32_index(np_camera,"camera_config",1,&val);
	if(result) {
		pr_err( "could not find index from camera_scaler\n");
		goto END_PROCESS;
	}

	viocmg->viocmg_dt_info->rear_cam_cam_config = val;

END_PROCESS:
        return result;
}



static int tcc_viocmg_parse_viocmg_common(struct platform_device *pdev)
{
        int result = -1;
        unsigned int val;
        struct device_node *np = pdev->dev.of_node;

        struct viocmg_soc *viocmg = platform_get_drvdata(pdev);

        result = of_property_read_u32_index(np,"telechips,disp_rdma",1,&val);
        if(result) { pr_err("cann't parse telechips,disp_rdma\r\n"); goto END_PROCESS; }
        viocmg->viocmg_dt_info->rear_cam_display_rdma = val;

        result = of_property_read_u32(np,"telechips,rear_cam_enable",&val);
        if(result) { pr_err("cann't parse telechips,rear_cam_enable\r\n"); goto END_PROCESS; }
        viocmg->viocmg_dt_info->feature_rear_cam_enable = val;

        result = of_property_read_u32(np,"telechips,rear_cam_use_viqe",&val);
        if(result) { pr_err("cann't parse telechips,rear_cam_use_viqe\r\n"); goto END_PROCESS; }
        viocmg->viocmg_dt_info->feature_rear_cam_use_viqe = val;

        result = of_property_read_u32(np,"telechips,rear_cam_viqe_mode",&val);
        if(result) { pr_err("cann't parse telechips,rear_cam_viqe_mode\r\n"); goto END_PROCESS; }
        viocmg->viocmg_dt_info->feature_rear_cam_viqe_mode = val;

        result = of_property_read_u32(np,"telechips,rear_cam_use_parking_line",&val);
        if(result) { pr_err("cann't parse telechips,rear_cam_use_parking_line\r\n"); goto END_PROCESS; }
        viocmg->viocmg_dt_info->feature_rear_cam_use_parking_line = val;

        result = of_property_read_u32(np,"telechips,rear_cam_mode",&val);
        if(result) { pr_err("cann't parse telechips,rear_cam_mode\r\n"); goto END_PROCESS; }
        viocmg->viocmg_dt_info->rear_cam_mode = val;

        result = of_property_read_u32(np,"telechips,rear_cam_ovp",&val);
        if(result) { pr_err("cann't parse telechips,rear_cam_ovp\r\n"); goto END_PROCESS; }
        printk("rear_cam_ovp = %d\r\n", val);
        viocmg->viocmg_dt_info->rear_cam_ovp = val;

END_PROCESS:
        return result;

}



static int tcc_viocmg_parse_hw_reg(struct platform_device *pdev)
{
        int result = -1;
        unsigned int val;
        struct device_node *np = pdev->dev.of_node;
        struct device_node *np_phandle;

        struct viocmg_soc *viocmg = platform_get_drvdata(pdev);
        
        np_phandle = of_parse_phandle(np, "telechips,wmixer", 0);
        if(np_phandle < 1) {
                pr_err("count not find telechips,wmixer\r\n");
                result = -ENODEV;
                goto END_PROCESS;                
        }

        viocmg->reg.display_wmix = of_iomap(np_phandle, viocmg->viocmg_dt_info->main_display_id);
	// Camera Config
	np_phandle = of_parse_phandle(np, "telechips,config", 0);
	if(np_phandle < 1) {
		pr_err("count not find telechips,config\r\n");
		result = -ENODEV;
		goto END_PROCESS;
	}
	viocmg->reg.camera_config = of_iomap(np_phandle, viocmg->viocmg_dt_info->rear_cam_cam_config);

END_PROCESS:
        return result;
}




static int tcc_viocmg_parse_dt(struct platform_device *pdev)
{
        int result;

        unsigned int val, vioc_addr;

        struct device_node *np = pdev->dev.of_node;
        struct viocmg_soc *viocmg = platform_get_drvdata(pdev);
        struct device_node *np_fbdisplay,*np_camera, *np_temp;

        tcc_viocmg_parse_fbdisplay(pdev);
        tcc_viocmg_parse_camera_common(pdev);
        tcc_viocmg_parse_viocmg_common(pdev);
        tcc_viocmg_parse_hw_reg(pdev);
        #if 0
        np_fbdisplay = of_parse_phandle(np, "telechips,viocmg-fbdisplay", 0);
        if(np_fbdisplay < 0) {
                pr_err("count not find telechips,viocmg-fbdisplay\r\n");
                result = -ENODEV;
                goto END_PROCESS;                
        }

        np_camera = of_parse_phandle(np, "telechips,viocmg-camera", 0);
        if(np_camera < 0) {
                pr_err("count not find telechips,viocmg-camera\r\n");
                result = -ENODEV;
                goto END_PROCESS;                
        }

        // fbdisplay display device id. 
        if (of_property_read_u32(np_fbdisplay, "telechips,fbdisplay_num", &val)){
                pr_err( "could not find  telechips,fbdisplay_nubmer\n");
                result = -ENODEV;
                goto END_PROCESS;
        }

        dprintk("telechips,fbdisplay_num = %d\r\n", val);

        if (of_property_read_u32(np_fbdisplay, "telechips,overlay_priority", &val)){
                pr_err( "could not find  telechips,overlay_priority\n");
                result = -ENODEV;
                goto END_PROCESS;
        }
        
        dprintk("telechips,overlay_priority = %d\r\n", val);

        // fbdisplay display device port. 
        if (of_property_read_u32(np_fbdisplay, "telechips,fbdisplay_port", &val)){
                val = 0xFFFFFFFF;
                pr_err("fbdisplay_port is not support..!!\r\n");
        }

        dprintk("telechips,fbdisplay_port = %d\r\n", val);
        #endif

        
        #if 0
        // Display RDMA 
        np_temp = of_parse_phandle(np, "telechips,disp_rdma", 0);
        if(!np_temp) {
                result = -ENODEV;
                pr_err( "could not find telechips,disp_rdma\n");
                goto END_PROCESS;
        }
        
        of_property_read_u32_index(np_camera,"telechips,disp_rdma", 1, &val);
        vioc_addr = (unsigned int)of_iomap(np_temp, val);           
        printk("disp_rdma = %d\r\n", val);
        
        // VIN
        np_temp = of_parse_phandle(np_camera, "camera_videoin", 0);
        if(!np_temp) {
                result = -ENODEV;
                pr_err( "could not find camera_videoin\n");
                goto END_PROCESS;
        }
        
        of_property_read_u32_index(np_camera,"camera_videoin", 1, &val);
        vioc_addr = (unsigned int)of_iomap(np_temp, val);     
        dprintk("camera_videoin = %d\r\n", val);
        viocmg->viocmg_dt_info->rear_cam_vin_vin = val;
        
        
        // VIQE
        np_temp = of_parse_phandle(np_camera, "camera_viqe", 0);
        if(!np_temp) {
                result = -ENODEV;
                pr_err( "could not find camera_viqe\n");
                goto END_PROCESS;
        }
        
        of_property_read_u32_index(np_camera,"camera_viqe", 1, &val);
        vioc_addr = (unsigned int)of_iomap(np_temp, val);
        dprintk("camera_viqe = %d\r\n", val);
        

        // SCALER
        np_temp = of_parse_phandle(np_camera, "camera_scaler", 0);
        if(!np_temp) {
                result = -ENODEV;
                pr_err( "could not find camera_scaler\n");
                goto END_PROCESS;
        }
        
        of_property_read_u32_index(np_camera,"camera_scaler", 1, &val);
        vioc_addr = (unsigned int)of_iomap(np_temp, val);
        dprintk("camera_scaler = %d\r\n", val);

        // wmixer
        np_temp = of_parse_phandle(np_camera, "camera_wmixer", 0);
        if(!np_temp) {
                result = -ENODEV;
                pr_err( "could not find camera_wmixer\n");
                goto END_PROCESS;
        }
        
        of_property_read_u32_index(np_camera,"camera_wmixer", 1, &val);
        vioc_addr = (unsigned int)of_iomap(np_temp, val);
        dprintk("camera_wmixer = %d\r\n", val);

        // WDMA
        np_temp = of_parse_phandle(np_camera, "camera_wdma", 0);
        if(!np_temp) {
                result = -ENODEV;
                pr_err( "could not find camera_wdma\n");
                goto END_PROCESS;
        }
        
        of_property_read_u32_index(np_camera,"camera_wdma", 1, &val);
        vioc_addr = (unsigned int)of_iomap(np_temp, val);
        
        // VIOC CONFIG
        np_temp = of_parse_phandle(np_camera, "camera_config", 0);
        if(!np_temp) {
                result = -ENODEV;
                pr_err( "could not find camera_config\n");
                goto END_PROCESS;
        }

        
        of_property_read_u32_index(np_camera,"camera_config", 1, &val);
        vioc_addr = (unsigned int)of_iomap(np_temp, val);

        #endif

        
        tcc_viocmg_dump_all(pdev);
        
        result = 0;
        
END_PROCESS:


        return result;
        
}


static int tcc_viocmg_probe(struct platform_device *pdev)
{
        int result = -1;
        pmap_t pmem_vioc;

        dev_dbg(&pdev->dev, "<%s>\n", __func__);

        printk("tcc_viocmg_probe\r\n");

        viocmg = devm_kzalloc(&pdev->dev, sizeof(struct viocmg_soc),GFP_KERNEL);
        if (!viocmg) {
                printk("viocmg: out of memory\r\n");
                result = -ENOMEM;
                goto END_PROCESS;
        }

        viocmg->viocmg_dt_info = (struct viocmg_dt_info *)devm_kzalloc(&pdev->dev, sizeof(struct viocmg_dt_info),GFP_KERNEL);
        
        if(!viocmg->viocmg_dt_info) {
                result = -ENOMEM;
                goto END_PROCESS;
        }   

        platform_set_drvdata(pdev, viocmg);
        
        result = tcc_viocmg_parse_dt(pdev);
        if(result < 0) {
                printk("Cann't parse viocmg..!!!\r\n");
                goto END_PROCESS;
        }

        viocmg->dev = &pdev->dev;

        spin_lock_init(&viocmg->int_reg_spin_lock);
        spin_lock_init(&viocmg->rdy_reg_spin_lock);
        
        mutex_init(&viocmg->mutex_lock);
        mutex_init(&viocmg->mutex_wmix);
        mutex_init(&viocmg->mutex_viqe);

        // linked list
        INIT_LIST_HEAD(&viocmg->wmix_enable_lists.chromakey.list);
        INIT_LIST_HEAD(&viocmg->wmix_enable_lists.alphablend.list);

        viocmg->components.deints.owner = VIOCMG_CALLERID_NONE;
        init_completion(&viocmg->components.deints.completion);
        viocmg->components.scaler[0].owner = VIOCMG_CALLERID_NONE;
        init_completion(&viocmg->components.scaler[0].completion);
        viocmg->components.scaler[1].owner = VIOCMG_CALLERID_NONE;
        init_completion(&viocmg->components.scaler[1].completion);
        viocmg->components.scaler[2].owner = VIOCMG_CALLERID_NONE;
        init_completion(&viocmg->components.scaler[2].completion);
        viocmg->components.scaler[3].owner = VIOCMG_CALLERID_NONE;
        init_completion(&viocmg->components.scaler[3].completion);
        viocmg->components.viqe.owner = VIOCMG_CALLERID_NONE;
        init_completion(&viocmg->components.viqe.completion);

        if(viocmg->viocmg_dt_info->feature_rear_cam_enable) {
                dprintk(" >> Dump viocmg rear_cam info\r\n"
                        " rear_cam_vin_rdma=%d\r\n"
                        " rear_cam_vin_vin=%d\r\n"
                        " rear_cam_vin_wmix=%d\r\n"
                        " rear_cam_vin_wdma=%d\r\n"
                        " rear_cam_display_rdma=%d\r\n"
                        " rear_cam_ovp=%d\r\n",
                        viocmg->viocmg_dt_info->rear_cam_vin_rdma,
                        viocmg->viocmg_dt_info->rear_cam_vin_vin,
                        viocmg->viocmg_dt_info->rear_cam_vin_wmix,
                        viocmg->viocmg_dt_info->rear_cam_vin_wdma,
                        viocmg->viocmg_dt_info->rear_cam_display_rdma,
                        viocmg->viocmg_dt_info->rear_cam_ovp);
        }
        
        result = 0;


END_PROCESS:


        return result;

}


int tcc_viocmg_remove(struct platform_device *pdev)
{
        struct viocmg_soc *viocmg = platform_get_drvdata(pdev); 

        if(viocmg->viocmg_dt_info)
                iounmap(viocmg->viocmg_dt_info);
        viocmg->viocmg_dt_info = NULL;
        return 0;
}



#ifdef CONFIG_OF
static struct of_device_id viocmg_of_match[] = {
        {.compatible = "telechips,viocmg-drv"},
};
MODULE_DEVICE_TABLE(of,viocmg_of_match);
#endif


static struct platform_driver tcc_viocmg_driver = {
        .probe      = tcc_viocmg_probe,
        .remove     = tcc_viocmg_remove,
        .suspend    = tcc_viocmg_suspend,
        .resume     = tcc_viocmg_resume,
        .driver = {
                .name       = "tcc_viocmg",
                .owner  = THIS_MODULE,
                #ifdef CONFIG_OF
                .of_match_table = of_match_ptr(viocmg_of_match),
                #endif

        },
};

#ifndef CONFIG_OF
struct platform_device tcc89XX_viocmg_device = {
        .name = "tcc_viocmg",
        .id = 0,
};
#endif

int32_t __init tcc_viocmg_init(void)
{
        int32_t ret;
    
        #ifndef CONFIG_OF
        ret = platform_device_register(&tcc89XX_viocmg_device);
        #endif

        dprintk("tcc_viocmg_init\r\n");
        ret = platform_driver_register(&tcc_viocmg_driver);
        return 0;
}

postcore_initcall(tcc_viocmg_init);

static void __exit tcc_viocmg_uninit(void)
{
    platform_driver_unregister(&tcc_viocmg_driver);
}

module_exit(tcc_viocmg_uninit);

