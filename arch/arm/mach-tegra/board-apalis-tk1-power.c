/*
 * arch/arm/mach-tegra/board-apali-tk1-power.c
 *
 * Copyright (c) 2016, Toradex AG. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <mach/edp.h>
#include <mach/irqs.h>
#include <linux/edp.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/pid_thermal_gov.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/tegra-fuse.h>
#include <linux/tegra-pmc.h>

#include <asm/mach-types.h>
#include <mach/pinmux-t12.h>

#include "pm.h"
#include "dvfs.h"
#include "board.h"
#include "common.h"
#include "tegra-board-id.h"
#include "board-common.h"
#include "board-apalis-tk1.h"
#include "board-pmu-defines.h"
#include "devices.h"
#include "iomap.h"
#include "tegra_cl_dvfs.h"
#include "tegra11_soctherm.h"

static struct tegra_suspend_platform_data apalis_tk1_suspend_data = {
	.cpu_timer			= 500,
	.cpu_off_timer			= 300,
	.suspend_mode			= TEGRA_SUSPEND_LP0,
	.core_timer			= 0x157e,
	.core_off_timer			= 10,
	.corereq_high			= true,
	.sysclkreq_high			= true,
	.cpu_lp2_min_residency		= 1000,
	.min_residency_vmin_fmin	= 1000,
	.min_residency_ncpu_fast	= 8000,
	.min_residency_ncpu_slow	= 5000,
	.min_residency_mclk_stop	= 5000,
	.min_residency_crail		= 20000,
};

/************************ Apalis TK1 CL-DVFS Data *********************/
#define APALIS_TK1_DEFAULT_CVB_ALIGNMENT	10000

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param apalis_tk1_cl_dvfs_param = {
	.sample_rate = 12500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};
#endif

/* Apalis TK1: fixed 10mV steps from 700mV to 1400mV */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 700000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(void)
{
	int i;
	u32 reg_init_value = 0x1e;
	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + reg_init_value;
		pmu_cpu_vdd_map[i].reg_uV = 700000 + 10000 * i;
	}
}

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_platform_data apalis_tk1_cl_dvfs_data = {
	.dfll_clk_name	= "dfll_cpu",
	.pmu_if		= TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate	= 400000,
		.slave_addr	= 0x80,
		.reg		= 0x00,
	},
	.vdd_map	= pmu_cpu_vdd_map,
	.vdd_map_size	= PMU_CPU_VDD_MAP_SIZE,

	.cfg_param	= &apalis_tk1_cl_dvfs_param,
};

/* ToDo: Jetson TK1 is missing such dfll node */
static const struct of_device_id dfll_of_match[] = {
	{ .compatible	= "nvidia,tegra124-dfll", },
	{ },
};

static int __init apalis_tk1_cl_dvfs_init(void)
{
	struct device_node *dn = of_find_matching_node(NULL, dfll_of_match);

	/*
	 * Apalis TK1 platforms maybe used with different DT variants. Some of them
	 * include DFLL data in DT, some - not. Check DT here, and continue with
	 * platform device registration only if DT DFLL node is not present.
	 */
	if (dn) {
		bool available = of_device_is_available(dn);
		of_node_put(dn);
		if (available)
			return 0;
	}

	fill_reg_map();
	apalis_tk1_cl_dvfs_data.flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
	tegra_cl_dvfs_device.dev.platform_data = &apalis_tk1_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

int __init apalis_tk1_rail_alignment_init(void)
{
	tegra12x_vdd_cpu_align(APALIS_TK1_DEFAULT_CVB_ALIGNMENT, 0);

	return 0;
}

int __init apalis_tk1_regulator_init(void)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	apalis_tk1_cl_dvfs_init();
#endif
	tegra_pmc_pmu_interrupt_polarity(true);

	return 0;
}

int __init apalis_tk1_suspend_init(void)
{
	tegra_init_suspend(&apalis_tk1_suspend_data);

	return 0;
}

int __init apalis_tk1_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA) {
		/* CD575M UCM2 */
		if(tegra_cpu_speedo_id() == 6)
			regulator_mA = 11800;
		/* CD575MI UCM1 */
		else if (tegra_cpu_speedo_id() == 8)
			regulator_mA = 12450;
		/* CD575MI UCM2 */
		else if (tegra_cpu_speedo_id() == 7)
			regulator_mA = 11500;
		/* CD575M UCM1 default */
		else
			regulator_mA = 12500;
	}

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	/* gpu maximum current */
	regulator_mA = 11400;

	pr_info("%s: GPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_gpu_edp_limits(regulator_mA);

	return 0;
}

static struct pid_thermal_gov_params soctherm_pid_params = {
	.max_err_temp		= 9000,
	.max_err_gain		= 1000,

	.gain_p			= 1000,
	.gain_d			= 0,

	.up_compensation	= 20,
	.down_compensation	= 20,
};

static struct thermal_zone_params soctherm_tzp = {
	.governor_name		= "pid_thermal_gov",
	.governor_params	= &soctherm_pid_params,
};

static struct tegra_thermtrip_pmic_data tpdata_as3722 = {
	.reset_tegra		= 1,
	.pmu_16bit_ops		= 0,
	.controller_type	= 0,
	.pmu_i2c_addr		= 0x40,
	.i2c_controller_id	= 4,
	.poweroff_reg_addr	= 0x36,
	.poweroff_reg_data	= 0x2,
};

static struct soctherm_platform_data apalis_tk1_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable	= true,
			.passive_delay	= 1000,
			.hotspot_offset	= 10000,
			.num_trips	= 3,
			.trips = {
				{
					.cdev_type	= "tegra-shutdown",
					.trip_temp	= 105000,
					.trip_type	= THERMAL_TRIP_CRITICAL,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
				},
				{
					.cdev_type	= "tegra-heavy",
					.trip_temp	= 102000,
					.trip_type	= THERMAL_TRIP_HOT,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
				},
				{
					.cdev_type	= "cpu-balanced",
					.trip_temp	= 92000,
					.trip_type	= THERMAL_TRIP_PASSIVE,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
				},
			},
			.tzp		= &soctherm_tzp,
		},
		[THERM_GPU] = {
			.zone_enable	= true,
			.passive_delay	= 1000,
			.hotspot_offset	= 5000,
			.num_trips	= 3,
			.trips = {
				{
					.cdev_type	= "tegra-shutdown",
					.trip_temp	= 105000,
					.trip_type	= THERMAL_TRIP_CRITICAL,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
				},
				{
					.cdev_type	= "tegra-heavy",
					.trip_temp	= 99000,
					.trip_type	= THERMAL_TRIP_HOT,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
				},
				{
					.cdev_type	= "gpu-balanced",
					.trip_temp	= 89000,
					.trip_type	= THERMAL_TRIP_PASSIVE,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
				},
			},
			.tzp		= &soctherm_tzp,
		},
		[THERM_MEM] = {
			.zone_enable	= true,
			.num_trips	= 1,
			.trips = {
				{
					.cdev_type	= "tegra-shutdown",
					.trip_temp	= 105000, /* = GPU shut */
					.trip_type	= THERMAL_TRIP_CRITICAL,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
				},
			},
			.tzp		= &soctherm_tzp,
		},
		[THERM_PLL] = {
			.zone_enable	= true,
			.tzp		= &soctherm_tzp,
		},
	},
	.throttle = {
		[THROTTLE_HEAVY] = {
			.priority	= 100,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable			= true,
					.depth			= 80,
					.throttling_depth	= "heavy_throttling",
				},
				[THROTTLE_DEV_GPU] = {
					.enable			= true,
					.throttling_depth	= "heavy_throttling",
				},
			},
		},
	},
};

int __init apalis_tk1_soctherm_init(void)
{
	const int t13x_cpu_edp_temp_margin = 5000,
		  t13x_gpu_edp_temp_margin = 6000;
	int cp_rev, ft_rev;
	enum soctherm_therm_id therm_cpu = THERM_CPU;

	cp_rev = tegra_fuse_calib_base_get_cp(NULL, NULL);
	ft_rev = tegra_fuse_calib_base_get_ft(NULL, NULL);

	pr_info("FUSE: cp_rev %d ft_rev %d\n", cp_rev, ft_rev);

	/* do this only for supported CP,FT fuses */
	if ((cp_rev >= 0) && (ft_rev >= 0)) {
		tegra_platform_edp_init(apalis_tk1_soctherm_data.
					therm[therm_cpu].trips,
					&apalis_tk1_soctherm_data.
					therm[therm_cpu].num_trips,
					t13x_cpu_edp_temp_margin);
		tegra_platform_gpu_edp_init(apalis_tk1_soctherm_data.
					    therm[THERM_GPU].trips,
					    &apalis_tk1_soctherm_data.
					    therm[THERM_GPU].num_trips,
					    t13x_gpu_edp_temp_margin);
		tegra_add_cpu_vmax_trips(apalis_tk1_soctherm_data.
					 therm[therm_cpu].trips,
					 &apalis_tk1_soctherm_data.
					 therm[therm_cpu].num_trips);
		tegra_add_tgpu_trips(apalis_tk1_soctherm_data.therm[THERM_GPU].
				     trips,
				     &apalis_tk1_soctherm_data.therm[THERM_GPU].
				     num_trips);
		tegra_add_core_vmax_trips(apalis_tk1_soctherm_data.
					  therm[THERM_PLL].trips,
					  &apalis_tk1_soctherm_data.
					  therm[THERM_PLL].num_trips);
	}

	tegra_add_cpu_vmin_trips(apalis_tk1_soctherm_data.therm[therm_cpu].
				 trips,
				 &apalis_tk1_soctherm_data.therm[therm_cpu].
				 num_trips);
	tegra_add_gpu_vmin_trips(apalis_tk1_soctherm_data.therm[THERM_GPU].
				 trips,
				 &apalis_tk1_soctherm_data.therm[THERM_GPU].
				 num_trips);
	tegra_add_core_vmin_trips(apalis_tk1_soctherm_data.therm[THERM_PLL].
				  trips,
				  &apalis_tk1_soctherm_data.therm[THERM_PLL].
				  num_trips);

	tegra_add_cpu_clk_switch_trips(apalis_tk1_soctherm_data.
				       therm[THERM_CPU].trips,
				       &apalis_tk1_soctherm_data.
				       therm[THERM_CPU].num_trips);

	apalis_tk1_soctherm_data.tshut_pmu_trip_data = &tpdata_as3722;

	return tegra11_soctherm_init(&apalis_tk1_soctherm_data);
}
