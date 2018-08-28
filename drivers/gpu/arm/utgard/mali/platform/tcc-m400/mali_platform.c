/*
 * Copyright (C) 2015 Telechips Inc.
 * Copyright (C) 2010, 2012-2014 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for:
 * - Realview Versatile platforms with ARM11 Mpcore and virtex 5.
 * - Versatile Express platforms with ARM Cortex-A9 and virtex 6.
 */
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include "mali_executor.h"
#ifndef CONFIG_MALI_DT
#include "arch/config.h"
#endif

#ifndef CONFIG_MALI_OS_MEMORY
#include <plat/pmap.h>
#endif

#if defined(CONFIG_GPU_BUS_SCALING) || defined(CONFIG_MALI_DVFS)
extern mali_bool init_mali_dvfs_staus(void);
extern void deinit_mali_dvfs_staus(void);
extern mali_bool mali_dvfs_handler(u32 setting);
#endif

#if defined(CONFIG_GPU_BUS_SCALING)
#include "arm_core_scaling.h"
void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);
static int mali_core_scaling_enable = 1;
#endif

#if defined(CONFIG_MALI_DVFS)
void mali_report_gpu_clock_info( struct mali_gpu_clock **data);
int mali_gpu_set_clock_step(int setting_clock_step);
int mali_gpu_get_clock_step(void);
extern u32 mali_dvfs_getCurStep(void);
struct mali_gpu_clock tcc_mali_clock_items;

#if defined(CONFIG_ARCH_TCC898X)
struct mali_gpu_clk_item mali_dvfs[] = {
                /* Hz                   uV */
        /*step 0*/{355, 800000},
        /*step 1*/{395, 850000},
        /*step 2*/{434, 900000},
        /*step 3*/{467, 950000},
        /*step 4*/{500, 1000000},
};
#elif defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
struct mali_gpu_clk_item mali_dvfs[] = {
              	/* MHz    uV */
	/*step 0*/{446, 900000}, 
	/*step 1*/{486, 950000},
	/*step 2*/{525, 1000000}, 
};
#elif defined(CONFIG_ARCH_TCC893X)
struct mali_gpu_clk_item mali_dvfs[] = {
	/*step 0*/{180, 1000000}, 
	/*step 1*/{227, 1050000},
	/*step 2*/{274, 1100000}, 
	/*step 3*/{316, 1150000},
	/*step 4*/{333, 1175000}, 
};
#elif defined(CONFIG_ARCH_TCC892X)
struct mali_gpu_clk_item mali_dvfs[] = {
	/*step 0*/{184, 1000000}, 
	/*step 1*/{224, 1050000},
	/*step 2*/{264, 1100000}, 
	/*step 3*/{317, 1150000},
	/*step 4*/{370, 1200000}, 
	/*step 5*/{414, 1250000},
	/*step 6*/{458, 1300000}, 
};
#endif
#endif


#if defined(CONFIG_CLOCK_TABLE)
#include <mach/clocktable.h>
static struct func_clktbl_t *mali_clktbl = NULL;
#endif
static struct clk *mali_clk = NULL;

typedef enum	{
	MALI_CLK_NONE=0,
	MALI_CLK_ENABLED,
	MALI_CLK_DISABLED
} t_mali_clk_type;

static t_mali_clk_type mali_clk_enable = MALI_CLK_NONE;

#ifndef CONFIG_MALI_DT
static void mali_platform_device_release(struct device *device);

static struct resource mali_gpu_resources[] =
{
#if defined(CONFIG_ARCH_TCC893X)
	MALI_GPU_RESOURCES_MALI400_MP2(0x70000000, _IRQ_3DGP_, _IRQ_3DGPMMU_, _IRQ_3DPP0_, _IRQ_3DPP0MMU_, _IRQ_3DPP1_, _IRQ_3DPP1MMU_)
#else
	MALI_GPU_RESOURCES_MALI400_MP1(0x70000000, _IRQ_3DGP_, _IRQ_3DGPMMU_, _IRQ_3DPP0_, _IRQ_3DPP0MMU_)

#endif
};
#endif

static struct mali_gpu_device_data mali_gpu_data =
{
#ifndef CONFIG_MALI_DT
	.pmu_switch_delay = 0xFF, /* do not have to be this high on FPGA, but it is good for testing to have a delay */
	.max_job_runtime = 6000, /* 6 seconds */

/* Mali OS memory limit */
#ifdef CONFIG_MALI_OS_MEMORY
	.shared_mem_size = CONFIG_MALI_MEMORY_SIZE * 1024UL * 1024UL,
#else	
	.dedicated_mem_start = NULL,
	.dedicated_mem_size = 0,
#endif
#endif


#if defined(CONFIG_GPU_BUS_SCALING)
	.control_interval = 300, /* 300ms */
	.utilization_callback = mali_gpu_utilization_callback,
#endif	
#if defined(CONFIG_MALI_DVFS)
	.get_clock_info = mali_report_gpu_clock_info,
	.get_freq = mali_gpu_get_clock_step,
	.set_freq = mali_gpu_set_clock_step,
#endif
#ifndef CONFIG_MALI_DT
/* PMU power domain configuration */
	.pmu_domain_config = {0x1, 0x2, 0x4, 0, 0, 0, 0, 0, 0, 0x1, 0, 0},
#endif
};

#ifndef CONFIG_MALI_DT
static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.dev.release = mali_platform_device_release,
	.dev.dma_mask = &mali_gpu_device.dev.coherent_dma_mask,
	.dev.coherent_dma_mask = DMA_BIT_MASK(32),

	.dev.platform_data = &mali_gpu_data,
};
#endif

_mali_osk_errcode_t mali_platform_power_mode_change(mali_bool power_on)
{
	if(power_on == MALI_TRUE)
	{
		if(mali_clk_enable != MALI_CLK_ENABLED)
		{
			MALI_DEBUG_PRINT(2, ("mali_platform_power_mode_change() clk_enable\n"));
			if (mali_clk == NULL)
				mali_clk = clk_get(NULL, "mali_clk");	
			clk_enable(mali_clk);
#if defined(CONFIG_CLOCK_TABLE)
		if (mali_clktbl == NULL) {
			mali_clktbl = clocktable_get("mali_clktbl");
			if (IS_ERR(mali_clktbl))
				mali_clktbl = NULL;
		}
		if (mali_clktbl)
			clocktable_ctrl(mali_clktbl, 0, CLKTBL_ENABLE);
#endif
			mali_clk_enable = MALI_CLK_ENABLED;
		}
	}

	else
	{
		if( mali_clk_enable == MALI_CLK_ENABLED)
		{
			MALI_DEBUG_PRINT(2, ("mali_platform_power_mode_change() clk_disable\n"));
			if (mali_clk == NULL)
				mali_clk = clk_get(NULL, "mali_clk");	
#if defined(CONFIG_CLOCK_TABLE)
		if (mali_clktbl == NULL) {
			mali_clktbl = clocktable_get("mali_clktbl");
			if (IS_ERR(mali_clktbl))
				mali_clktbl = NULL;
		}
		if (mali_clktbl)
			clocktable_ctrl(mali_clktbl, 0, CLKTBL_DISABLE);
#endif
			clk_disable(mali_clk);
			mali_clk_enable = MALI_CLK_DISABLED;
		}

	}
    MALI_SUCCESS;
}

#ifndef CONFIG_MALI_DT
int mali_platform_device_register(void)
{
	int err = -1;
	int num_pp_cores = 0;

	printk("%s\n", __func__);
	MALI_DEBUG_PRINT(2, ("mali_platform_device_register() called\n"));

#if defined(CONFIG_ARCH_TCC893X)
	num_pp_cores = 2;
#else
	num_pp_cores = 1;
#endif

#ifndef CONFIG_MALI_OS_MEMORY
	pmap_t pmap_mali_reserved;
	pmap_get_info("mali_reserved", &pmap_mali_reserved);
	unsigned int test_addr;
	unsigned int test_size;

	mali_gpu_data.dedicated_mem_start = pmap_mali_reserved.base;
	mali_gpu_data.dedicated_mem_size= pmap_mali_reserved.size;
	printk("%s Requesting MALI dedicated memory: 0x%08x, size: %u\n", __func__, mali_gpu_data.dedicated_mem_start, mali_gpu_data.dedicated_mem_size);
	/* Ask the OS if we can use the specified physical memory */
	if (NULL == request_mem_region(mali_gpu_data.dedicated_mem_start, mali_gpu_data.dedicated_mem_size, "MALI Memory"))
	{
		printk("Failed to request memory region (0x%08X - 0x%08X). \n", mali_gpu_data.dedicated_mem_start, mali_gpu_data.dedicated_mem_start + mali_gpu_data.dedicated_mem_start, mali_gpu_data.dedicated_mem_size-1);
	}
#endif	

	mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources);
	mali_gpu_device.resource = mali_gpu_resources;
	/* Register the platform device */
	err = platform_device_register(&mali_gpu_device);
	if (0 == err) {
#if defined(CONFIG_GPU_BUS_SCALING) || defined(CONFIG_MALI_DVFS)
	if(!init_mali_dvfs_staus())
		MALI_DEBUG_PRINT(1, ("mali_platform_init failed\n"));        
#endif		
		mali_platform_power_mode_change(MALI_TRUE);
		
#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 1000);
		pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
		pm_runtime_enable(&(mali_gpu_device.dev));
#endif
		MALI_DEBUG_ASSERT(0 < num_pp_cores);
#if defined(CONFIG_GPU_BUS_SCALING)
	mali_core_scaling_init(num_pp_cores);
#endif
		return 0;
	}

	return err;
}

void mali_platform_device_unregister(void)
{
	printk("%s\n", __func__);
	MALI_DEBUG_PRINT(2, ("mali_platform_device_unregister() called\n"));

#if defined(CONFIG_GPU_BUS_SCALING)
	mali_core_scaling_term();
#endif
#if defined(CONFIG_GPU_BUS_SCALING) || defined(CONFIG_MALI_DVFS)
	deinit_mali_dvfs_staus();
#endif
	platform_device_unregister(&mali_gpu_device);

	platform_device_put(&mali_gpu_device);

}

static void mali_platform_device_release(struct device *device)
{
	printk("%s\n", __func__);
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

#else /* CONFIG_MALI_DT */
int mali_platform_device_init(struct platform_device *device)
{
	int num_pp_cores;
	int err = -1;

#if defined(CONFIG_ARCH_TCC893X)
	num_pp_cores = 2;
#else
	num_pp_cores = 1;
#endif

	err = platform_device_add_data(device, &mali_gpu_data, sizeof(mali_gpu_data));
	if (0 == err) {
#if defined(CONFIG_GPU_BUS_SCALING) || defined(CONFIG_MALI_DVFS)
	if(!init_mali_dvfs_staus())
		MALI_DEBUG_PRINT(1, ("mali_platform_init failed\n"));        
#endif	
		mali_platform_power_mode_change(MALI_TRUE);
		
#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(device->dev), 1000);
		pm_runtime_use_autosuspend(&(device->dev));
#endif
		pm_runtime_enable(&(device->dev));
#endif
		MALI_DEBUG_ASSERT(0 < num_pp_cores);
#if defined(CONFIG_GPU_BUS_SCALING)
		mali_core_scaling_init(num_pp_cores);
#endif
	}

	return err;
}

int mali_platform_device_deinit(struct platform_device *device)
{
	MALI_IGNORE(device);

	MALI_DEBUG_PRINT(4, ("mali_platform_device_deinit() called\n"));
#if defined(CONFIG_GPU_BUS_SCALING)
	mali_core_scaling_term();
#endif
#if defined(CONFIG_GPU_BUS_SCALING) || defined(CONFIG_MALI_DVFS)
	deinit_mali_dvfs_staus();
#endif
	return 0;
}

#endif /* CONFIG_MALI_DT */
#if defined(CONFIG_GPU_BUS_SCALING)

static int param_set_core_scaling(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);

	if (1 == mali_core_scaling_enable) {
		mali_core_scaling_sync(mali_executor_get_num_cores_enabled());
	}
	return ret;
}

static struct kernel_param_ops param_ops_core_scaling = {
	.set = param_set_core_scaling,
	.get = param_get_int,
};

module_param_cb(mali_core_scaling_enable, &param_ops_core_scaling, &mali_core_scaling_enable, 0644);
MODULE_PARM_DESC(mali_core_scaling_enable, "1 means to enable core scaling policy, 0 means to disable core scaling policy");


void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
	if (1 == mali_core_scaling_enable) {
		mali_core_scaling_update(data);
	}
	if(mali_clk_enable == MALI_CLK_ENABLED)
	{
		MALI_DEBUG_PRINT(1,( "data->utilization_gpu:%d\n", data->utilization_gpu)); 
		if(!mali_dvfs_handler(data->utilization_gpu))
				MALI_DEBUG_PRINT(1,( "error on mali dvfs status in utilization\n")); 
	}
}
#endif
#if defined(CONFIG_MALI_DVFS)

void mali_report_gpu_clock_info( struct mali_gpu_clock **data)
{
	tcc_mali_clock_items.num_of_steps = sizeof(mali_dvfs) / sizeof(struct mali_gpu_clk_item);
	tcc_mali_clock_items.item = mali_dvfs;
	*data = &tcc_mali_clock_items;
}

int mali_gpu_set_clock_step(int setting_clock_step)
{
	if( mali_clk_enable == MALI_CLK_ENABLED){
		MALI_DEBUG_PRINT(1,( "mali_gpu_set_clock_step setting_clock_step:%d\n", setting_clock_step)); 
		if(!mali_dvfs_handler(setting_clock_step))
			MALI_DEBUG_PRINT(1,( "error on mali dvfs status in utilization\n")); 

		return MALI_TRUE;
	} else {
		return MALI_FALSE;
	}
}

int mali_gpu_get_clock_step(void)
{
	return mali_dvfs_getCurStep();
}
#endif
