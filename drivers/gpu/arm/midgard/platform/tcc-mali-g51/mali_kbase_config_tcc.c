/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2017 Telechips Inc.

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

#include <linux/ioport.h>
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#ifdef CONFIG_MALI_MIDGARD_DVFS
#include "mali_kbase_pm_internal.h"
#endif

static int clk_maliG51_status = 0;

#ifndef CONFIG_OF
static kbase_io_resources io_resources = {
       .job_irq_number = 24,
       .mmu_irq_number = 25,
       .gpu_irq_number = 26,
       .io_memory_region = {
                            .start = 0x10000000,
                            .end = 0x10000000 + (4096 * 4) - 1}
};
#endif

#ifdef CONFIG_MALI_MIDGARD_DVFS
struct tcc_gpu_dvfs_table_t {
	unsigned int gpu_freq;
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int uVol;
};

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
static struct tcc_gpu_dvfs_table_t gpu_dvfs_table[] = {
       	/* Hz    			uV */
	{ 440000000,   30,  0, 650000 },	
	{ 530000000,   40, 30, 700000 },	
	{ 620000000,   70, 40, 750000 },	
	{ 700000000,  100, 70, 800000 },	
};
#endif
#define MALI_DVFS_STEPS ARRAY_SIZE(gpu_dvfs_table)
static unsigned int maliDvfsCurrentStep = MALI_DVFS_STEPS-1;

__inline static unsigned int decideNextStatus(unsigned int utilisation)
{
	int i;
	unsigned int currStep = 0;
	for (i=0; i<MALI_DVFS_STEPS; i++)
	{
		if(utilisation <= gpu_dvfs_table[i].up_threshold && utilisation > gpu_dvfs_table[i].down_threshold)
			currStep = i;
	}
	return currStep;
}

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation,
	u32 util_gl_share, u32 util_cl_share[2])
{
	unsigned int nextStep = 0;
	unsigned int currStep = 0;
	
	currStep = maliDvfsCurrentStep;
	nextStep = decideNextStatus(utilisation);
	if(currStep != nextStep) {
		if (kbdev->clock) 
		{
			#if defined(CONFIG_REGULATOR)
printk("################################## %s %d\n", __func__, __LINE__);

				if(kbdev->regulator)
					regulator_set_voltage(kbdev->regulator, gpu_dvfs_table[nextStep].uVol, gpu_dvfs_table[nextStep].uVol);
			#endif
			clk_set_rate(kbdev->clock, gpu_dvfs_table[nextStep].gpu_freq);
			maliDvfsCurrentStep = nextStep;		
//			if(kbdev->clock && kbdev->regulator)
//				printk("%s. setting %d, utilisation:%d,  curstep:%d, nextstep:%d, getrate:%d, getvoltage:%d\n", __func__, gpu_dvfs_table[nextStep].gpu_freq, utilisation, currStep, nextStep, clk_get_rate(kbdev->clock), regulator_get_voltage(kbdev->regulator));
			return 1;
		}
		else
		{
			dev_dbg(kbdev->dev, "%s. Please check kbdev->clock\n", __func__);
			return 0;
		}
	}
		
}
#endif

int kbase_platform_clk_on(struct kbase_device *kbdev)
{
	int err = 0;
	if (!kbdev) 
 		return -ENODEV; 
	if(!kbdev->clocks[0])
 		return -ENODEV; 
		
	if (clk_maliG51_status == 1) 
		return 0; 
	err = clk_prepare_enable(kbdev->clocks[0]);
	if(err)
		dev_err(kbdev->dev, "Failed to prepare and enable clock (%d)\n", err);
	else		
		clk_maliG51_status = 1; 
	return err; 
}

int kbase_platform_clk_off(struct kbase_device *kbdev)
{
	if (!kbdev) 
 		return -ENODEV; 
	if(!kbdev->clocks[0])
 		return -ENODEV; 
	
	if (clk_maliG51_status == 0) 
		return 0; 
	clk_disable_unprepare(kbdev->clocks[0]);
	clk_maliG51_status = 0; 
	return 0; 
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int ret;

	dev_dbg(kbdev->dev, "pm_callback_power_on %p\n", (void *)kbdev->dev->pm_domain);

	ret = pm_runtime_get_sync(kbdev->dev);
	pm_runtime_mark_last_busy(kbdev->dev);

	dev_dbg(kbdev->dev, "pm_runtime_get returned %d\n", ret);

	return 1;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "pm_callback_power_off\n");
	pm_runtime_mark_last_busy(kbdev->dev);
	pm_runtime_put_autosuspend(kbdev->dev);
}

#ifdef KBASE_PM_RUNTIME
int kbase_device_runtime_init(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "kbase_device_runtime_init\n");
	pm_runtime_set_autosuspend_delay(kbdev->dev, 1000);
	pm_runtime_use_autosuspend(kbdev->dev);
	pm_runtime_enable(kbdev->dev);

#ifdef CONFIG_MALI_MIDGARD_DEBUG_SYS
	{
		int err = kbase_platform_create_sysfs_file(kbdev->dev);
		if (err)
			return err;
	}
#endif				/* CONFIG_MALI_MIDGARD_DEBUG_SYS */
	return 0;
}

void kbase_device_runtime_disable(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "kbase_device_runtime_disable\n");
	pm_runtime_disable(kbdev->dev);
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	dev_dbg(kbdev->dev, "%s clk_maliG51_status:%d\n", __func__, clk_maliG51_status);
	kbase_platform_clk_on(kbdev);
	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{	
	dev_dbg(kbdev->dev, "%s clk_maliG51_status:%d\n", __func__, clk_maliG51_status);
	kbase_platform_clk_off(kbdev);
}
#endif

static void pm_callback_resume(struct kbase_device *kbdev)
{
	int ret = pm_callback_runtime_on(kbdev);

	WARN_ON(ret);
}

static void pm_callback_suspend(struct kbase_device *kbdev)
{
	pm_callback_runtime_off(kbdev);
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = kbase_device_runtime_init,
	.power_runtime_term_callback = kbase_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
#else				/* KBASE_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* KBASE_PM_RUNTIME */
};


static struct kbase_platform_config tcc_platform_config = {
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &tcc_platform_config;
}

int kbase_platform_early_init(void)
{
	return 0;
}
