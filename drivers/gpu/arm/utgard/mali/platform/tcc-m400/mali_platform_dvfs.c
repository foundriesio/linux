/*
 * Copyright (C) 2013-2015 Telechips Inc.
 * Copyright (C) 2010-2013 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mali_kernel_common.h"
#include "mali_osk.h"
#include <linux/mali/mali_utgard.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <asm/io.h>
//#include "mali_device_pause_resume.h"
#include <linux/workqueue.h>

#if defined(CONFIG_CLOCK_TABLE)
#include <mach/clocktable.h>
static struct func_clktbl_t *mali_clktbl = NULL;
#else
static struct clk *gpu_clk = NULL;
static unsigned int gpu_max_rate;
#endif
#if defined(CONFIG_MALI_DVFS)
struct regulator	*vdd_gv;
#endif
#define GPU_THRESHOLD(x)	((unsigned int)((x*255)/100))

struct tcc_gpu_dvfs_table_t {
	unsigned int gpu_freq;
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int uVol;
};

#if defined(CONFIG_ARCH_TCC898X)
static struct tcc_gpu_dvfs_table_t gpu_dvfs_table[] = {
                /* Hz                   uV */
        { 355000000, 60, 40, 800000 },  // Core 0.80V
        { 395000000, 60, 40, 850000 },  // Core 0.85V
        { 434000000, 60, 40, 900000 },  // Core 0.90V
        { 467000000, 60, 40, 950000 },  // Core 0.95V
        { 500000000, 60, 40, 1000000 }, // Core 1.00V
};
#elif defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
static struct tcc_gpu_dvfs_table_t gpu_dvfs_table[] = {
          	/* Hz    		uV */
	{ 446250000, 60, 40, 900000 },	// Core 0.90V
	{ 485625000, 60, 40, 950000 },	// Core 0.95V
	{ 525000000, 60, 40, 1000000 },	// Core 1.00V
};
#elif defined(CONFIG_ARCH_TCC893X)
static struct tcc_gpu_dvfs_table_t gpu_dvfs_table[] = {
	{ 180979, 60, 40 },	// Core 1.00V
	{ 227548, 60, 40 },	// Core 1.05V
	{ 274117, 60, 40 },	// Core 1.10V
	{ 316029, 60, 40 },	// Core 1.15V
	{ 333334, 60, 40 },	// Core 1.175V
};
#elif defined(CONFIG_ARCH_TCC892X)
static struct tcc_gpu_dvfs_table_t gpu_dvfs_table[] = {
#if (1)	// normal
	{ 184000, 50, 30 },	// Core 1.00V
	{ 224140, 60, 40 },	// Core 1.05V
	{ 264290, 70, 50 },	// Core 1.10V
	{ 317140, 80, 60 },	// Core 1.15V
	{ 370000, 85, 70 },	// Core 1.20V
	{ 414400, 90, 80 },	// Core 1.25V
	{ 458800, 95, 85 },	// Core 1.30V
#else
#if (1)	// for reducing current
	#if (0)
	{ 184000, 85, 75 },	// Core 1.00V
	{ 224140, 85, 75 },	// Core 1.05V
	{ 264290, 85, 75 },	// Core 1.10V
	{ 317140, 85, 75 },	// Core 1.15V
	{ 370000, 90, 80 },	// Core 1.20V
	{ 414400, 95, 85 },	// Core 1.25V
	{ 458800, 95, 90 },	// Core 1.30V
	#else
	{ 184000, 95, 85 },	// Core 1.00V
	{ 224140, 95, 85 },	// Core 1.05V
	{ 264290, 95, 85 },	// Core 1.10V
	{ 317140, 95, 85 },	// Core 1.15V
	{ 370000, 95, 85 },	// Core 1.20V
	{ 414400, 95, 85 },	// Core 1.25V
	{ 458800, 95, 90 },	// Core 1.30V
	#endif
#else	// for performance
	{ 184000, 60, 40 },	// Core 1.00V
	{ 224140, 60, 40 },	// Core 1.05V
	{ 264290, 60, 40 },	// Core 1.10V
	{ 317140, 60, 40 },	// Core 1.15V
	{ 370000, 60, 40 },	// Core 1.20V
	{ 414400, 60, 40 },	// Core 1.25V
	{ 458800, 60, 40 },	// Core 1.30V
#endif
#endif
};
#else
	#error
#endif
#define MALI_DVFS_STEPS ARRAY_SIZE(gpu_dvfs_table)

static struct workqueue_struct *mali_dvfs_wq = 0;
static unsigned int maliDvfsCurrentStep;
static unsigned int maliDvfsNextStep;
static u32 mali_dvfs_utilization = 255;

static void mali_dvfs_work_handler(struct work_struct *w);
static DECLARE_WORK(mali_dvfs_work, mali_dvfs_work_handler);

__inline static unsigned int decideNextStatus(unsigned int utilization, unsigned int currStep)
{
	if (currStep >= MALI_DVFS_STEPS)
		return currStep;

    if(utilization > GPU_THRESHOLD(gpu_dvfs_table[currStep].up_threshold)) {
	    currStep++;
		if (currStep >= (MALI_DVFS_STEPS-1))
			currStep = MALI_DVFS_STEPS-1;
	}
    else if(utilization < GPU_THRESHOLD(gpu_dvfs_table[currStep].down_threshold)) {
		if (currStep > 0)
			currStep--;
	}

	return currStep;
}

static void mali_dvfs_work_handler(struct work_struct *w)
{
	unsigned int nextStep = 0;
	unsigned int currStep = 0;
	mali_bool boostup = MALI_FALSE;
	int ret = 0;

	/*decide next step*/
	currStep = maliDvfsCurrentStep;
#if defined(CONFIG_GPU_BUS_SCALING)
	nextStep = decideNextStatus(mali_dvfs_utilization, currStep);
#elif defined(CONFIG_MALI_DVFS)  && defined(CONFIG_REGULATOR)
	nextStep = maliDvfsNextStep;
	if(vdd_gv == NULL)	
	{
		char *buf = "vdd_gv";
		vdd_gv = regulator_get(NULL, buf);
		if (IS_ERR(vdd_gv))
			vdd_gv = NULL;
	}
#endif
	/*if next status is same with current status, don't change anything*/
	if(currStep != nextStep) {
		/*check if boost up or not*/
		if(nextStep > currStep)
			boostup = 1;

#if defined(CONFIG_CLOCK_TABLE)
		if (mali_clktbl == NULL) {
			mali_clktbl = clocktable_get("mali_clktbl");
			if (IS_ERR(mali_clktbl))
				mali_clktbl = NULL;
		}
		if (mali_clktbl)
		{
			MALI_DEBUG_PRINT(1,( "mali_dvfs_work_handler nextStep:%d\n", nextStep)); 
			clocktable_ctrl(mali_clktbl, nextStep, CLKTBL_ENABLE);
		}
#else
		if (gpu_clk) {
			unsigned int new_rate;
			if (nextStep >= MALI_DVFS_STEPS)
				nextStep = MALI_DVFS_STEPS-1;
			mali_dev_pause();
#if defined(CONFIG_MALI_DVFS) && defined(CONFIG_REGULATOR)
			if(vdd_gv)
			{
				ret = regulator_set_voltage(vdd_gv, gpu_dvfs_table[nextStep].uVol, gpu_dvfs_table[nextStep].uVol);
			}
			else
				printk("%s vdd_gv null\n", __func__);
#endif

			new_rate = (gpu_dvfs_table[nextStep].gpu_freq > gpu_max_rate) ? (gpu_max_rate+100) : gpu_dvfs_table[nextStep].gpu_freq;
			clk_set_rate(gpu_clk, new_rate);
			mali_dev_resume();
		}
#endif
	    maliDvfsCurrentStep = nextStep;
	}
}

mali_bool init_mali_dvfs_staus(void)
{
	int step =0;
	if (!mali_dvfs_wq)
		mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");
#if !defined(CONFIG_CLOCK_TABLE)
	if (gpu_clk == NULL) {
		gpu_clk = clk_get(NULL, "mali_clk");
		if (IS_ERR(gpu_clk))
			gpu_clk = NULL;
		else {
			step = MALI_DVFS_STEPS-1;
			gpu_max_rate = clk_get_rate(gpu_clk);
			//clk_set_rate(gpu_clk, gpu_dvfs_table[step].gpu_freq*1000);
		}
	}
#endif
#if defined(CONFIG_MALI_DVFS)
	step = MALI_DVFS_STEPS-1;
#endif
	MALI_DEBUG_PRINT(1,( "init_mali_dvfs_staus maliDvfsCurrentStep:%d\n", step)); 
	maliDvfsCurrentStep = step;
	return MALI_TRUE;
}

void deinit_mali_dvfs_staus(void)
{
#if defined(CONFIG_MALI_DVFS)  && defined(CONFIG_REGULATOR)
	if(vdd_gv)	
		regulator_put(vdd_gv);
#endif
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);
	mali_dvfs_wq = NULL;
}

mali_bool mali_dvfs_handler(u32 setting)
{
#if defined(CONFIG_GPU_BUS_SCALING)
	mali_dvfs_utilization = setting;	//setting : utilization
#elif defined(CONFIG_MALI_DVFS)
	maliDvfsNextStep = setting;		//setting : next step
#endif
	if (mali_dvfs_wq)
		queue_work_on(0, mali_dvfs_wq,&mali_dvfs_work);
	return MALI_TRUE;
}

u32 mali_dvfs_getCurStep(void)
{
	return maliDvfsCurrentStep;
}
