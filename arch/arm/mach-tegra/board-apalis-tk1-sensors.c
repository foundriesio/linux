/*
 * arch/arm/mach-tegra/board-apalis-tk1-sensors.c
 *
 * Copyright (c) 2016, Toradex AG.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/mpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/nct1008.h>
#include <linux/pid_thermal_gov.h>
#include <linux/tegra-fuse.h>
#include <linux/of_platform.h>
#include <mach/edp.h>
#include <mach/pinmux-t12.h>
#include <mach/pinmux.h>
#include <mach/io_dpd.h>
#include <media/camera.h>
#include <media/ar0330.h>
#include <media/ar0261.h>
#include <media/ar1335.h>
#include <media/imx135.h>
#include <media/imx179.h>
#include <media/imx185.h>
#include <media/dw9718.h>
#include <media/as364x.h>
#include <media/ov5693.h>
#include <media/ov7695.h>
#include <media/mt9m114.h>
#include <media/ad5823.h>
#include <media/max77387.h>

#include <media/ov4689.h>
#include <linux/platform_device.h>
#include <media/soc_camera.h>
#include <media/soc_camera_platform.h>
#include <media/tegra_v4l2_camera.h>
#include <linux/generic_adc_thermal.h>

#include "cpu-tegra.h"
#include "devices.h"
#include "board.h"
#include "board-common.h"
#include "board-apalis-tk1.h"
#include "tegra-board-id.h"

static struct regulator *apalis_tk1_vcmvdd;

static int apalis_tk1_get_extra_regulators(void)
{
	if (!apalis_tk1_vcmvdd) {
		apalis_tk1_vcmvdd = regulator_get(NULL, "avdd_af1_cam");
		if (WARN_ON(IS_ERR(apalis_tk1_vcmvdd))) {
			pr_err("%s: can't get regulator avdd_af1_cam: %ld\n",
			       __func__, PTR_ERR(apalis_tk1_vcmvdd));
			regulator_put(apalis_tk1_vcmvdd);
			apalis_tk1_vcmvdd = NULL;
			return -ENODEV;
		}
	}

	return 0;
}

static struct tegra_io_dpd csia_io = {
	.name			= "CSIA",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 0,
};

static struct tegra_io_dpd csib_io = {
	.name			= "CSIB",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 1,
};

static struct tegra_io_dpd csie_io = {
	.name			= "CSIE",
	.io_dpd_reg_index	= 1,
	.io_dpd_bit		= 12,
};

static int apalis_tk1_ar0330_front_power_on(struct ar0330_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd)))
		return -EFAULT;

	/* disable CSIE IOs DPD mode to turn on front camera for apalis_tk1 */
	tegra_io_dpd_disable(&csie_io);

	gpio_set_value(CAM2_PWDN, 0);

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto ar0330_front_iovdd_fail;

	usleep_range(1000, 1100);
	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ar0330_front_avdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM2_PWDN, 1);

	return 0;
 ar0330_front_avdd_fail:
	regulator_disable(pw->iovdd);

 ar0330_front_iovdd_fail:
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_ar0330_front_power_off(struct ar0330_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd))) {
		/* put CSIE IOs into DPD mode to
		 * save additional power for apalis_tk1
		 */
		tegra_io_dpd_enable(&csie_io);
		return -EFAULT;
	}

	gpio_set_value(CAM2_PWDN, 0);

	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	return 0;
}

struct ar0330_platform_data apalis_tk1_ar0330_front_data = {
	.power_on	= apalis_tk1_ar0330_front_power_on,
	.power_off	= apalis_tk1_ar0330_front_power_off,
	.dev_name	= "ar0330.1",
	.mclk_name	= "mclk2",
};

static int apalis_tk1_ar0330_power_on(struct ar0330_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd)))
		return -EFAULT;

	/* disable CSIE IOs DPD mode to turn on front camera for apalis_tk1 */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	gpio_set_value(CAM1_PWDN, 0);

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto ar0330_iovdd_fail;

	usleep_range(1000, 1100);
	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ar0330_avdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM1_PWDN, 1);

	return 0;
 ar0330_avdd_fail:
	regulator_disable(pw->iovdd);

 ar0330_iovdd_fail:
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_ar0330_power_off(struct ar0330_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd))) {
		/* put CSIE IOs into DPD mode to
		 * save additional power for apalis_tk1
		 */
		tegra_io_dpd_enable(&csia_io);
		tegra_io_dpd_enable(&csib_io);
		return -EFAULT;
	}

	gpio_set_value(CAM1_PWDN, 0);

	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	return 0;
}

struct ar0330_platform_data apalis_tk1_ar0330_data = {
	.power_on	= apalis_tk1_ar0330_power_on,
	.power_off	= apalis_tk1_ar0330_power_off,
	.dev_name	= "ar0330",
};

static int apalis_tk1_ov4689_power_on(struct ov4689_power_rail *pw)
{
	pr_info("%s: ++\n", __func__);
	/* disable CSIA/B IOs DPD mode to turn on camera for apalis_tk1 */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	gpio_set_value(TEGRA_GPIO_PBB5, 0);
	usleep_range(10, 20);
	gpio_set_value(TEGRA_GPIO_PBB5, 1);
	usleep_range(820, 1000);

	return 1;
}

static int apalis_tk1_ov4689_power_off(struct ov4689_power_rail *pw)
{
	pr_info("%s: ++\n", __func__);

	gpio_set_value(TEGRA_GPIO_PBB5, 0);

	/*
	 * put CSIA/B IOs into DPD mode to save additional power for apalis_tk1
	 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);

	return 0;
}

static int apalis_tk1_ar0261_power_on(struct ar0261_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd)))
		return -EFAULT;

	/* disable CSIE IOs DPD mode to turn on front camera for apalis_tk1 */
	tegra_io_dpd_disable(&csie_io);

	if (apalis_tk1_get_extra_regulators())
		goto apalis_tk1_ar0261_poweron_fail;

	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM_AF_PWDN, 1);

	err = regulator_enable(apalis_tk1_vcmvdd);
	if (unlikely(err))
		goto ar0261_vcm_fail;

	err = regulator_enable(pw->dvdd);
	if (unlikely(err))
		goto ar0261_dvdd_fail;

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ar0261_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto ar0261_iovdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM2_PWDN, 1);

	gpio_set_value(CAM_RSTN, 1);

	return 0;
 ar0261_iovdd_fail:
	regulator_disable(pw->dvdd);

 ar0261_dvdd_fail:
	regulator_disable(pw->avdd);

 ar0261_avdd_fail:
	regulator_disable(apalis_tk1_vcmvdd);

 ar0261_vcm_fail:
	pr_err("%s vcmvdd failed.\n", __func__);
	return -ENODEV;

 apalis_tk1_ar0261_poweron_fail:
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_ar0261_power_off(struct ar0261_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd ||
			     !apalis_tk1_vcmvdd))) {
		/* put CSIE IOs into DPD mode to
		 * save additional power for apalis_tk1
		 */
		tegra_io_dpd_enable(&csie_io);
		return -EFAULT;
	}

	gpio_set_value(CAM_RSTN, 0);

	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);
	regulator_disable(pw->avdd);
	regulator_disable(apalis_tk1_vcmvdd);
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	return 0;
}

struct ar0261_platform_data apalis_tk1_ar0261_data = {
	.power_on	= apalis_tk1_ar0261_power_on,
	.power_off	= apalis_tk1_ar0261_power_off,
	.mclk_name	= "mclk2",
};

static int apalis_tk1_imx135_get_extra_regulators(struct imx135_power_rail *pw)
{
	if (!pw->ext_reg1) {
		pw->ext_reg1 = regulator_get(NULL, "imx135_reg1");
		if (WARN_ON(IS_ERR(pw->ext_reg1))) {
			pr_err("%s: can't get regulator imx135_reg1: %ld\n",
			       __func__, PTR_ERR(pw->ext_reg1));
			pw->ext_reg1 = NULL;
			return -ENODEV;
		}
	}

	if (!pw->ext_reg2) {
		pw->ext_reg2 = regulator_get(NULL, "imx135_reg2");
		if (WARN_ON(IS_ERR(pw->ext_reg2))) {
			pr_err("%s: can't get regulator imx135_reg2: %ld\n",
			       __func__, PTR_ERR(pw->ext_reg2));
			pw->ext_reg2 = NULL;
			return -ENODEV;
		}
	}

	return 0;
}

static int apalis_tk1_imx135_power_on(struct imx135_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	/* disable CSIA/B IOs DPD mode to turn on camera for apalis_tk1 */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	if (apalis_tk1_imx135_get_extra_regulators(pw))
		goto imx135_poweron_fail;

	err = regulator_enable(pw->ext_reg1);
	if (unlikely(err))
		goto imx135_ext_reg1_fail;

	err = regulator_enable(pw->ext_reg2);
	if (unlikely(err))
		goto imx135_ext_reg2_fail;

	gpio_set_value(CAM_AF_PWDN, 1);
	gpio_set_value(CAM1_PWDN, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto imx135_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (err)
		goto imx135_iovdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM1_PWDN, 1);

	usleep_range(300, 310);

	return 1;

 imx135_iovdd_fail:
	regulator_disable(pw->avdd);

 imx135_avdd_fail:
	if (pw->ext_reg2)
		regulator_disable(pw->ext_reg2);

 imx135_ext_reg2_fail:
	if (pw->ext_reg1)
		regulator_disable(pw->ext_reg1);
	gpio_set_value(CAM_AF_PWDN, 0);

 imx135_ext_reg1_fail:
 imx135_poweron_fail:
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_imx135_power_off(struct imx135_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd))) {
		tegra_io_dpd_enable(&csia_io);
		tegra_io_dpd_enable(&csib_io);
		return -EFAULT;
	}

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);

	regulator_disable(pw->ext_reg1);
	regulator_disable(pw->ext_reg2);

	/*
	 * put CSIA/B IOs into DPD mode to save additional power for apalis_tk1
	 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	return 0;
}

static int apalis_tk1_ar1335_power_on(struct ar1335_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	/* disable CSIA/B IOs DPD mode to turn on camera for apalis_tk1 */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	gpio_set_value(CAM_RSTN, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto ar1335_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (err)
		goto ar1335_iovdd_fail;

	err = regulator_enable(pw->dvdd);
	if (err)
		goto ar1335_dvdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM_RSTN, 1);

	usleep_range(300, 310);

	return 0;

 ar1335_dvdd_fail:
	regulator_disable(pw->iovdd);

 ar1335_iovdd_fail:
	regulator_disable(pw->avdd);

 ar1335_avdd_fail:
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_ar1335_power_off(struct ar1335_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd))) {
		tegra_io_dpd_enable(&csia_io);
		tegra_io_dpd_enable(&csib_io);
		return -EFAULT;
	}

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);
	regulator_disable(pw->dvdd);

	/*
	 * put CSIA/B IOs into DPD mode to save additional power for apalis_tk1
	 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	return 0;
}

static int apalis_tk1_imx179_power_on(struct imx179_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	/* disable CSIA/B IOs DPD mode to turn on camera for apalis_tk1 */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	gpio_set_value(CAM_AF_PWDN, 1);
	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM1_PWDN, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto imx179_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (err)
		goto imx179_iovdd_fail;

	err = regulator_enable(pw->dvdd);
	if (err)
		goto imx179_dvdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM_RSTN, 1);

	usleep_range(300, 310);

	return 1;

 imx179_dvdd_fail:
	regulator_disable(pw->iovdd);

 imx179_iovdd_fail:
	regulator_disable(pw->avdd);

 imx179_avdd_fail:
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_imx179_power_off(struct imx179_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd))) {
		tegra_io_dpd_enable(&csia_io);
		tegra_io_dpd_enable(&csib_io);
		return -EFAULT;
	}

	regulator_disable(pw->dvdd);
	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);

	/*
	 * put CSIA/B IOs into DPD mode to save additional power for apalis_tk1
	 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	return 0;
}

struct ar1335_platform_data apalis_tk1_ar1335_data = {
	.flash_cap = {
		.enable		= 1,
		.edge_trig_en	= 1,
		.start_edge	= 0,
		.repeat		= 1,
		.delay_frm	= 0,
	},
	.power_on	= apalis_tk1_ar1335_power_on,
	.power_off	= apalis_tk1_ar1335_power_off,
};

static int apalis_tk1_imx185_power_on(struct imx185_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	/* disable CSIA/B IOs DPD mode to turn on camera for apalis_tk1 */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	gpio_set_value(CAM1_PWDN, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->dvdd);
	if (err)
		goto imx185_dvdd_fail;

	err = regulator_enable(pw->iovdd);
	if (err)
		goto imx185_iovdd_fail;

	err = regulator_enable(pw->avdd);
	if (err)
		goto imx185_avdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM1_PWDN, 1);

	usleep_range(300, 310);

	return 0;

 imx185_avdd_fail:
	regulator_disable(pw->iovdd);

 imx185_iovdd_fail:
	regulator_disable(pw->dvdd);

 imx185_dvdd_fail:
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_imx185_power_off(struct imx185_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd))) {
		tegra_io_dpd_enable(&csia_io);
		tegra_io_dpd_enable(&csib_io);
		return -EFAULT;
	}

	regulator_disable(pw->avdd);
	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);

	/*
	 * put CSIA/B IOs into DPD mode to save additional power for apalis_tk1
	 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	return 0;
}

struct imx135_platform_data apalis_tk1_imx135_data = {
	.flash_cap = {
		.enable		= 1,
		.edge_trig_en	= 1,
		.start_edge	= 0,
		.repeat		= 1,
		.delay_frm	= 0,
	},
	.ext_reg	= true,
	.power_on	= apalis_tk1_imx135_power_on,
	.power_off	= apalis_tk1_imx135_power_off,
};

struct imx179_platform_data apalis_tk1_imx179_data = {
	.flash_cap = {
		.enable		= 1,
		.edge_trig_en	= 1,
		.start_edge	= 0,
		.repeat		= 1,
		.delay_frm	= 0,
	},
	.power_on	= apalis_tk1_imx179_power_on,
	.power_off	= apalis_tk1_imx179_power_off,
};

struct ov4689_platform_data apalis_tk1_ov4689_data = {
	.flash_cap = {
		.enable		= 0,
		.edge_trig_en	= 1,
		.start_edge	= 0,
		.repeat		= 1,
		.delay_frm	= 0,
	},
	.power_on	= apalis_tk1_ov4689_power_on,
	.power_off	= apalis_tk1_ov4689_power_off,
};

static int apalis_tk1_dw9718_power_on(struct dw9718_power_rail *pw)
{
	int err;
	pr_info("%s\n", __func__);

	if (unlikely(!pw || !pw->vdd || !pw->vdd_i2c || !pw->vana))
		return -EFAULT;

	err = regulator_enable(pw->vdd);
	if (unlikely(err))
		goto dw9718_vdd_fail;

	err = regulator_enable(pw->vdd_i2c);
	if (unlikely(err))
		goto dw9718_i2c_fail;

	err = regulator_enable(pw->vana);
	if (unlikely(err))
		goto dw9718_ana_fail;

	usleep_range(1000, 1020);

	/* return 1 to skip the in-driver power_on sequence */
	pr_debug("%s --\n", __func__);
	return 1;

 dw9718_ana_fail:
	regulator_disable(pw->vdd_i2c);

 dw9718_i2c_fail:
	regulator_disable(pw->vdd);

 dw9718_vdd_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_dw9718_power_off(struct dw9718_power_rail *pw)
{
	pr_info("%s\n", __func__);

	if (unlikely(!pw || !pw->vdd || !pw->vdd_i2c || !pw->vana))
		return -EFAULT;

	regulator_disable(pw->vdd);
	regulator_disable(pw->vdd_i2c);
	regulator_disable(pw->vana);

	return 1;
}

static u16 dw9718_devid;
static int apalis_tk1_dw9718_detect(void *buf, size_t size)
{
	dw9718_devid = 0x9718;
	return 0;
}

static struct nvc_focus_cap dw9718_cap = {
	.settle_time	= 30,
	.slew_rate	= 0x3A200C,
	.focus_macro	= 450,
	.focus_infinity	= 200,
	.focus_hyper	= 200,
};

static struct dw9718_platform_data apalis_tk1_dw9718_data = {
	.cfg		= NVC_CFG_NODEV,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "focuser",
	.cap		= &dw9718_cap,
	.power_on	= apalis_tk1_dw9718_power_on,
	.power_off	= apalis_tk1_dw9718_power_off,
	.detect		= apalis_tk1_dw9718_detect,
};

static struct as364x_platform_data apalis_tk1_as3648_data = {
	.config = {
		.led_mask		= 3,
		.max_total_current_mA	= 1000,
		.max_peak_current_mA	= 600,
		.max_torch_current_mA	= 600,
		.vin_low_v_run_mV	= 3070,
		.strobe_type		= 1,
	},
	.pinstate = {
		.mask	= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0),
		.values	= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0)
	},
	.dev_name	= "torch",
	.type		= AS3648,
	.gpio_strobe	= CAM_FLASH_STROBE,
};

static int apalis_tk1_ov7695_power_on(struct ov7695_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd)))
		return -EFAULT;

	/* disable CSIE IOs DPD mode to turn on front camera for apalis_tk1 */
	tegra_io_dpd_disable(&csie_io);

	gpio_set_value(CAM2_PWDN, 0);
	usleep_range(1000, 1020);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ov7695_avdd_fail;
	usleep_range(300, 320);

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto ov7695_iovdd_fail;
	usleep_range(1000, 1020);

	gpio_set_value(CAM2_PWDN, 1);
	usleep_range(1000, 1020);

	return 0;

 ov7695_iovdd_fail:
	regulator_disable(pw->avdd);

 ov7695_avdd_fail:
	gpio_set_value(CAM_RSTN, 0);
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	return -ENODEV;
}

static int apalis_tk1_ov7695_power_off(struct ov7695_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd))) {
		/* put CSIE IOs into DPD mode to
		 * save additional power for apalis_tk1
		 */
		tegra_io_dpd_enable(&csie_io);
		return -EFAULT;
	}
	usleep_range(100, 120);

	gpio_set_value(CAM2_PWDN, 0);
	usleep_range(100, 120);

	regulator_disable(pw->iovdd);
	usleep_range(100, 120);

	regulator_disable(pw->avdd);

	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	return 0;
}

struct ov7695_platform_data apalis_tk1_ov7695_pdata = {
	.power_on	= apalis_tk1_ov7695_power_on,
	.power_off	= apalis_tk1_ov7695_power_off,
	.mclk_name	= "mclk2",
};

static int apalis_tk1_mt9m114_power_on(struct mt9m114_power_rail *pw)
{
	int err;
	if (unlikely(!pw || !pw->avdd || !pw->iovdd))
		return -EFAULT;

	/* disable CSIE IOs DPD mode to turn on front camera for apalis_tk1 */
	tegra_io_dpd_disable(&csie_io);

	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM2_PWDN, 1);
	usleep_range(1000, 1020);

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto mt9m114_iovdd_fail;

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto mt9m114_avdd_fail;

	usleep_range(1000, 1020);
	gpio_set_value(CAM_RSTN, 1);
	gpio_set_value(CAM2_PWDN, 0);
	usleep_range(1000, 1020);

	/* return 1 to skip the in-driver power_on swquence */
	return 1;

 mt9m114_avdd_fail:
	regulator_disable(pw->iovdd);

 mt9m114_iovdd_fail:
	gpio_set_value(CAM_RSTN, 0);
	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	return -ENODEV;
}

static int apalis_tk1_mt9m114_power_off(struct mt9m114_power_rail *pw)
{
	if (unlikely(!pw || !pw->avdd || !pw->iovdd)) {
		/* put CSIE IOs into DPD mode to
		 * save additional power for apalis_tk1
		 */
		tegra_io_dpd_enable(&csie_io);
		return -EFAULT;
	}

	usleep_range(100, 120);
	gpio_set_value(CAM_RSTN, 0);
	usleep_range(100, 120);
	regulator_disable(pw->avdd);
	usleep_range(100, 120);
	regulator_disable(pw->iovdd);

	/* put CSIE IOs into DPD mode to save additional power for apalis_tk1 */
	tegra_io_dpd_enable(&csie_io);
	return 1;
}

struct mt9m114_platform_data apalis_tk1_mt9m114_pdata = {
	.power_on	= apalis_tk1_mt9m114_power_on,
	.power_off	= apalis_tk1_mt9m114_power_off,
	.mclk_name	= "mclk2",
};

static int apalis_tk1_ov5693_power_on(struct ov5693_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->dovdd || !pw->avdd)))
		return -EFAULT;

	/* disable CSIA/B IOs DPD mode to turn on camera for apalis_tk1 */
	tegra_io_dpd_disable(&csia_io);
	tegra_io_dpd_disable(&csib_io);

	if (apalis_tk1_get_extra_regulators())
		goto ov5693_poweron_fail;

	gpio_set_value(CAM1_PWDN, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto ov5693_avdd_fail;

	err = regulator_enable(pw->dovdd);
	if (err)
		goto ov5693_iovdd_fail;

	udelay(2);
	gpio_set_value(CAM1_PWDN, 1);

	err = regulator_enable(apalis_tk1_vcmvdd);
	if (unlikely(err))
		goto ov5693_vcmvdd_fail;

	usleep_range(1000, 1110);

	return 0;

 ov5693_vcmvdd_fail:
	regulator_disable(pw->dovdd);

 ov5693_iovdd_fail:
	regulator_disable(pw->avdd);

 ov5693_avdd_fail:
	gpio_set_value(CAM1_PWDN, 0);

 ov5693_poweron_fail:
	/*
	 * put CSIA/B IOs into DPD mode to save additional power for apalis_tk1
	 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_ov5693_power_off(struct ov5693_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->dovdd || !pw->avdd))) {
		/* put CSIA/B IOs into DPD mode to
		 * save additional power for apalis_tk1
		 */
		tegra_io_dpd_enable(&csia_io);
		tegra_io_dpd_enable(&csib_io);
		return -EFAULT;
	}

	usleep_range(21, 25);
	gpio_set_value(CAM1_PWDN, 0);
	udelay(2);

	regulator_disable(apalis_tk1_vcmvdd);
	regulator_disable(pw->dovdd);
	regulator_disable(pw->avdd);

	/*
	 * put CSIA/B IOs into DPD mode to save additional power for apalis_tk1
	 */
	tegra_io_dpd_enable(&csia_io);
	tegra_io_dpd_enable(&csib_io);
	return 0;
}

static struct nvc_gpio_pdata ov5693_gpio_pdata[] = {
	{OV5693_GPIO_TYPE_PWRDN, CAM1_PWDN, true, 0,},
};

#define NV_GUID(a, b, c, d, e, f, g, h) \
	((u64) ((((a)&0xffULL) << 56ULL) | (((b)&0xffULL) << 48ULL) | \
	(((c)&0xffULL) << 40ULL) | (((d)&0xffULL) << 32ULL) | \
	(((e)&0xffULL) << 24ULL) | (((f)&0xffULL) << 16ULL) | \
	(((g)&0xffULL) << 8ULL) | (((h)&0xffULL))))

static struct nvc_imager_cap ov5693_cap = {
	.identifier		= "OV5693",
	.sensor_nvc_interface	= 3,
	.pixel_types[0]		= 0x101,
	.orientation		= 0,
	.direction		= 0,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 8000000, /* value * 1000000 */
	},
	.clock_profiles[1] = {
		.external_clock_khz	= 0,
		.clock_multiplier	= 0,
	},
	.h_sync_edge			= 0,
	.v_sync_edge			= 0,
	.mclk_on_vgp0			= 0,
	.csi_port			= 0,
	.data_lanes			= 2,
	.virtual_channel_id		= 0,
	.discontinuous_clk_mode		= 1,
	.cil_threshold_settle		= 0,
	.min_blank_time_width		= 16,
	.min_blank_time_height		= 16,
	.preferred_mode_index		= 0,
	.focuser_guid	= NV_GUID('f', '_', 'A', 'D', '5', '8', '2', '3'),
	.torch_guid	= NV_GUID('l', '_', 'N', 'V', 'C', 'A', 'M', '0'),
	.cap_version			= NVC_IMAGER_CAPABILITIES_VERSION2,
	.flash_control_enabled		= 0,
	.adjustable_flash_timing	= 0,
	.is_hdr				= 1,
};

static struct ov5693_platform_data apalis_tk1_ov5693_pdata = {
	.gpio_count	= ARRAY_SIZE(ov5693_gpio_pdata),
	.gpio		= ov5693_gpio_pdata,
	.power_on	= apalis_tk1_ov5693_power_on,
	.power_off	= apalis_tk1_ov5693_power_off,
	.dev_name	= "ov5693",
	.cap		= &ov5693_cap,
	.mclk_name	= "mclk",
	.regulators = {
		.avdd	= "avdd_ov5693",
		.dvdd	= "dvdd",
		.dovdd	= "dovdd",
	},
	.has_eeprom	= 1,
};

static struct imx185_platform_data apalis_tk1_imx185_data = {
	.power_on	= apalis_tk1_imx185_power_on,
	.power_off	= apalis_tk1_imx185_power_off,
	.mclk_name	= "mclk",
};

static int apalis_tk1_ov5693_front_power_on(struct ov5693_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->dovdd || !pw->avdd)))
		return -EFAULT;

	if (apalis_tk1_get_extra_regulators())
		goto ov5693_front_poweron_fail;

	gpio_set_value(CAM2_PWDN, 0);
	gpio_set_value(CAM_RSTN, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto ov5693_front_avdd_fail;

	err = regulator_enable(pw->dovdd);
	if (err)
		goto ov5693_front_iovdd_fail;

	udelay(2);
	gpio_set_value(CAM2_PWDN, 1);
	gpio_set_value(CAM_RSTN, 1);

	err = regulator_enable(apalis_tk1_vcmvdd);
	if (unlikely(err))
		goto ov5693_front_vcmvdd_fail;

	usleep_range(1000, 1110);

	return 0;

 ov5693_front_vcmvdd_fail:
	regulator_disable(pw->dovdd);

 ov5693_front_iovdd_fail:
	regulator_disable(pw->avdd);

 ov5693_front_avdd_fail:
	gpio_set_value(CAM2_PWDN, 0);
	gpio_set_value(CAM_RSTN, 0);

 ov5693_front_poweron_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int apalis_tk1_ov5693_front_power_off(struct ov5693_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->dovdd || !pw->avdd)))
		return -EFAULT;

	usleep_range(21, 25);
	gpio_set_value(CAM2_PWDN, 0);
	gpio_set_value(CAM_RSTN, 0);
	udelay(2);

	regulator_disable(apalis_tk1_vcmvdd);
	regulator_disable(pw->dovdd);
	regulator_disable(pw->avdd);

	return 0;
}

static struct nvc_gpio_pdata ov5693_front_gpio_pdata[] = {
	{OV5693_GPIO_TYPE_PWRDN, CAM2_PWDN, true, 0,},
	{OV5693_GPIO_TYPE_PWRDN, CAM_RSTN, true, 0,},
};

static struct nvc_imager_cap ov5693_front_cap = {
	.identifier		= "OV5693.1",
	.sensor_nvc_interface	= 4,
	.pixel_types[0]		= 0x101,
	.orientation		= 0,
	.direction		= 1,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 8000000, /* value * 1000000 */
	},
	.clock_profiles[1] = {
		.external_clock_khz	= 0,
		.clock_multiplier	= 0,
	},
	.h_sync_edge			= 0,
	.v_sync_edge			= 0,
	.mclk_on_vgp0			= 0,
	.csi_port			= 1,
	.data_lanes			= 2,
	.virtual_channel_id		= 0,
	.discontinuous_clk_mode		= 1,
	.cil_threshold_settle		= 0,
	.min_blank_time_width		= 16,
	.min_blank_time_height		= 16,
	.preferred_mode_index		= 0,
	.focuser_guid			= 0,
	.torch_guid			= 0,
	.cap_version			= NVC_IMAGER_CAPABILITIES_VERSION2,
	.flash_control_enabled		= 0,
	.adjustable_flash_timing	= 0,
	.is_hdr				= 1,
};

static struct ov5693_platform_data apalis_tk1_ov5693_front_pdata = {
	.gpio_count	= ARRAY_SIZE(ov5693_front_gpio_pdata),
	.gpio		= ov5693_front_gpio_pdata,
	.power_on	= apalis_tk1_ov5693_front_power_on,
	.power_off	= apalis_tk1_ov5693_front_power_off,
	.dev_name	= "ov5693.1",
	.mclk_name	= "mclk2",
	.cap		= &ov5693_front_cap,
	.regulators = {
		.avdd	= "vana",
		.dvdd	= "vdig",
		.dovdd	= "vif",
	},
	.has_eeprom	= 0,
};

static int apalis_tk1_ad5823_power_on(struct ad5823_platform_data *pdata)
{
	int err = 0;

	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 1);
	pdata->pwr_dev = AD5823_PWR_DEV_ON;

	return err;
}

static int apalis_tk1_ad5823_power_off(struct ad5823_platform_data *pdata)
{
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 0);
	pdata->pwr_dev = AD5823_PWR_DEV_OFF;

	return 0;
}

static struct ad5823_platform_data apalis_tk1_ad5823_pdata = {
	.gpio		= CAM_AF_PWDN,
	.power_on	= apalis_tk1_ad5823_power_on,
	.power_off	= apalis_tk1_ad5823_power_off,
};

static struct camera_data_blob apalis_tk1_camera_lut[] = {
	{"apalis_tk1_imx135_pdata", &apalis_tk1_imx135_data},
	{"apalis_tk1_imx185_pdata", &apalis_tk1_imx185_data},
	{"apalis_tk1_dw9718_pdata", &apalis_tk1_dw9718_data},
	{"apalis_tk1_ar0261_pdata", &apalis_tk1_ar0261_data},
	{"apalis_tk1_mt9m114_pdata", &apalis_tk1_mt9m114_pdata},
	{"apalis_tk1_ov5693_pdata", &apalis_tk1_ov5693_pdata},
	{"apalis_tk1_ad5823_pdata", &apalis_tk1_ad5823_pdata},
	{"apalis_tk1_as3648_pdata", &apalis_tk1_as3648_data},
	{"apalis_tk1_ov7695_pdata", &apalis_tk1_ov7695_pdata},
	{"apalis_tk1_ov5693f_pdata", &apalis_tk1_ov5693_front_pdata},
	{"apalis_tk1_ar0330_pdata", &apalis_tk1_ar0330_data},
	{"apalis_tk1_ar0330_front_pdata", &apalis_tk1_ar0330_front_data},
	{"apalis_tk1_ov4689_pdata", &apalis_tk1_ov4689_data},
	{"apalis_tk1_ar1335_pdata", &apalis_tk1_ar1335_data},
	{},
};

void __init apalis_tk1_camera_auxdata(void *data)
{
	struct of_dev_auxdata *aux_lut = data;
	while (aux_lut && aux_lut->compatible) {
		if (!strcmp(aux_lut->compatible, "nvidia,tegra124-camera")) {
			pr_info("%s: update camera lookup table.\n", __func__);
			aux_lut->platform_data = apalis_tk1_camera_lut;
		}
		aux_lut++;
	}
}

static struct pid_thermal_gov_params cpu_pid_params = {
	.max_err_temp		= 4000,
	.max_err_gain		= 1000,

	.gain_p			= 1000,
	.gain_d			= 0,

	.up_compensation	= 15,
	.down_compensation	= 15,
};

static struct thermal_zone_params cpu_tzp = {
	.governor_name		= "pid_thermal_gov",
	.governor_params	= &cpu_pid_params,
};

static struct thermal_zone_params board_tzp = {
	.governor_name = "pid_thermal_gov"
};

static struct throttle_table cpu_throttle_table[] = {
	/* CPU_THROT_LOW cannot be used by other than CPU */
	/*      CPU,    GPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
	{ {2295000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2269500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2244000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2218500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2193000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2167500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2142000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2116500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2091000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2065500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2040000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {2014500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1989000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1963500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1938000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1912500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1887000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1861500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1836000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1810500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1785000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1759500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1734000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1708500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1683000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1657500, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1632000, NO_CAP, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1606500, 790000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1581000, 776000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1555500, 762000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1530000, 749000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1504500, 735000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1479000, 721000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1453500, 707000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1428000, 693000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1402500, 679000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1377000, 666000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1351500, 652000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1326000, 638000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1300500, 624000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1275000, 610000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1249500, 596000, NO_CAP, NO_CAP, NO_CAP, NO_CAP} },
	{ {1224000, 582000, NO_CAP, NO_CAP, NO_CAP, 792000} },
	{ {1198500, 569000, NO_CAP, NO_CAP, NO_CAP, 792000} },
	{ {1173000, 555000, NO_CAP, NO_CAP, 360000, 792000} },
	{ {1147500, 541000, NO_CAP, NO_CAP, 360000, 792000} },
	{ {1122000, 527000, NO_CAP, 684000, 360000, 792000} },
	{ {1096500, 513000, 444000, 684000, 360000, 792000} },
	{ {1071000, 499000, 444000, 684000, 360000, 792000} },
	{ {1045500, 486000, 444000, 684000, 360000, 792000} },
	{ {1020000, 472000, 444000, 684000, 324000, 792000} },
	{ { 994500, 458000, 444000, 684000, 324000, 792000} },
	{ { 969000, 444000, 444000, 600000, 324000, 792000} },
	{ { 943500, 430000, 444000, 600000, 324000, 792000} },
	{ { 918000, 416000, 396000, 600000, 324000, 792000} },
	{ { 892500, 402000, 396000, 600000, 324000, 792000} },
	{ { 867000, 389000, 396000, 600000, 324000, 792000} },
	{ { 841500, 375000, 396000, 600000, 288000, 792000} },
	{ { 816000, 361000, 396000, 600000, 288000, 792000} },
	{ { 790500, 347000, 396000, 600000, 288000, 792000} },
	{ { 765000, 333000, 396000, 504000, 288000, 792000} },
	{ { 739500, 319000, 348000, 504000, 288000, 792000} },
	{ { 714000, 306000, 348000, 504000, 288000, 624000} },
	{ { 688500, 292000, 348000, 504000, 288000, 624000} },
	{ { 663000, 278000, 348000, 504000, 288000, 624000} },
	{ { 637500, 264000, 348000, 504000, 288000, 624000} },
	{ { 612000, 250000, 348000, 504000, 252000, 624000} },
	{ { 586500, 236000, 348000, 504000, 252000, 624000} },
	{ { 561000, 222000, 348000, 420000, 252000, 624000} },
	{ { 535500, 209000, 288000, 420000, 252000, 624000} },
	{ { 510000, 195000, 288000, 420000, 252000, 624000} },
	{ { 484500, 181000, 288000, 420000, 252000, 624000} },
	{ { 459000, 167000, 288000, 420000, 252000, 624000} },
	{ { 433500, 153000, 288000, 420000, 252000, 396000} },
	{ { 408000, 139000, 288000, 420000, 252000, 396000} },
	{ { 382500, 126000, 288000, 420000, 252000, 396000} },
	{ { 357000, 112000, 288000, 420000, 252000, 396000} },
	{ { 331500,  98000, 288000, 420000, 252000, 396000} },
	{ { 306000,  84000, 288000, 420000, 252000, 396000} },
	{ { 280500,  84000, 288000, 420000, 252000, 396000} },
	{ { 255000,  84000, 288000, 420000, 252000, 396000} },
	{ { 229500,  84000, 288000, 420000, 252000, 396000} },
	{ { 204000,  84000, 288000, 420000, 252000, 396000} },
};

static struct balanced_throttle cpu_throttle = {
	.throt_tab_size	= ARRAY_SIZE(cpu_throttle_table),
	.throt_tab	= cpu_throttle_table,
};

static struct throttle_table gpu_throttle_table[] = {
	/* CPU_THROT_LOW cannot be used by other than CPU */
	/*      CPU,    GPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
	{ {2295000, 782800, 480000, 756000, 384000, 924000} },
	{ {2269500, 772200, 480000, 756000, 384000, 924000} },
	{ {2244000, 761600, 480000, 756000, 384000, 924000} },
	{ {2218500, 751100, 480000, 756000, 384000, 924000} },
	{ {2193000, 740500, 480000, 756000, 384000, 924000} },
	{ {2167500, 729900, 480000, 756000, 384000, 924000} },
	{ {2142000, 719300, 480000, 756000, 384000, 924000} },
	{ {2116500, 708700, 480000, 756000, 384000, 924000} },
	{ {2091000, 698100, 480000, 756000, 384000, 924000} },
	{ {2065500, 687500, 480000, 756000, 384000, 924000} },
	{ {2040000, 676900, 480000, 756000, 384000, 924000} },
	{ {2014500, 666000, 480000, 756000, 384000, 924000} },
	{ {1989000, 656000, 480000, 756000, 384000, 924000} },
	{ {1963500, 645000, 480000, 756000, 384000, 924000} },
	{ {1938000, 635000, 480000, 756000, 384000, 924000} },
	{ {1912500, 624000, 480000, 756000, 384000, 924000} },
	{ {1887000, 613000, 480000, 756000, 384000, 924000} },
	{ {1861500, 603000, 480000, 756000, 384000, 924000} },
	{ {1836000, 592000, 480000, 756000, 384000, 924000} },
	{ {1810500, 582000, 480000, 756000, 384000, 924000} },
	{ {1785000, 571000, 480000, 756000, 384000, 924000} },
	{ {1759500, 560000, 480000, 756000, 384000, 924000} },
	{ {1734000, 550000, 480000, 756000, 384000, 924000} },
	{ {1708500, 539000, 480000, 756000, 384000, 924000} },
	{ {1683000, 529000, 480000, 756000, 384000, 924000} },
	{ {1657500, 518000, 480000, 756000, 384000, 924000} },
	{ {1632000, 508000, 480000, 756000, 384000, 924000} },
	{ {1606500, 497000, 480000, 756000, 384000, 924000} },
	{ {1581000, 486000, 480000, 756000, 384000, 924000} },
	{ {1555500, 476000, 480000, 756000, 384000, 924000} },
	{ {1530000, 465000, 480000, 756000, 384000, 924000} },
	{ {1504500, 455000, 480000, 756000, 384000, 924000} },
	{ {1479000, 444000, 480000, 756000, 384000, 924000} },
	{ {1453500, 433000, 480000, 756000, 384000, 924000} },
	{ {1428000, 423000, 480000, 756000, 384000, 924000} },
	{ {1402500, 412000, 480000, 756000, 384000, 924000} },
	{ {1377000, 402000, 480000, 756000, 384000, 924000} },
	{ {1351500, 391000, 480000, 756000, 384000, 924000} },
	{ {1326000, 380000, 480000, 756000, 384000, 924000} },
	{ {1300500, 370000, 480000, 756000, 384000, 924000} },
	{ {1275000, 359000, 480000, 756000, 384000, 924000} },
	{ {1249500, 349000, 480000, 756000, 384000, 924000} },
	{ {1224000, 338000, 480000, 756000, 384000, 792000} },
	{ {1198500, 328000, 480000, 756000, 384000, 792000} },
	{ {1173000, 317000, 480000, 756000, 360000, 792000} },
	{ {1147500, 306000, 480000, 756000, 360000, 792000} },
	{ {1122000, 296000, 480000, 684000, 360000, 792000} },
	{ {1096500, 285000, 444000, 684000, 360000, 792000} },
	{ {1071000, 275000, 444000, 684000, 360000, 792000} },
	{ {1045500, 264000, 444000, 684000, 360000, 792000} },
	{ {1020000, 253000, 444000, 684000, 324000, 792000} },
	{ { 994500, 243000, 444000, 684000, 324000, 792000} },
	{ { 969000, 232000, 444000, 600000, 324000, 792000} },
	{ { 943500, 222000, 444000, 600000, 324000, 792000} },
	{ { 918000, 211000, 396000, 600000, 324000, 792000} },
	{ { 892500, 200000, 396000, 600000, 324000, 792000} },
	{ { 867000, 190000, 396000, 600000, 324000, 792000} },
	{ { 841500, 179000, 396000, 600000, 288000, 792000} },
	{ { 816000, 169000, 396000, 600000, 288000, 792000} },
	{ { 790500, 158000, 396000, 600000, 288000, 792000} },
	{ { 765000, 148000, 396000, 504000, 288000, 792000} },
	{ { 739500, 137000, 348000, 504000, 288000, 792000} },
	{ { 714000, 126000, 348000, 504000, 288000, 624000} },
	{ { 688500, 116000, 348000, 504000, 288000, 624000} },
	{ { 663000, 105000, 348000, 504000, 288000, 624000} },
	{ { 637500,  95000, 348000, 504000, 288000, 624000} },
	{ { 612000,  84000, 348000, 504000, 252000, 624000} },
	{ { 586500,  84000, 348000, 504000, 252000, 624000} },
	{ { 561000,  84000, 348000, 420000, 252000, 624000} },
	{ { 535500,  84000, 288000, 420000, 252000, 624000} },
	{ { 510000,  84000, 288000, 420000, 252000, 624000} },
	{ { 484500,  84000, 288000, 420000, 252000, 624000} },
	{ { 459000,  84000, 288000, 420000, 252000, 624000} },
	{ { 433500,  84000, 288000, 420000, 252000, 396000} },
	{ { 408000,  84000, 288000, 420000, 252000, 396000} },
	{ { 382500,  84000, 288000, 420000, 252000, 396000} },
	{ { 357000,  84000, 288000, 420000, 252000, 396000} },
	{ { 331500,  84000, 288000, 420000, 252000, 396000} },
	{ { 306000,  84000, 288000, 420000, 252000, 396000} },
	{ { 280500,  84000, 288000, 420000, 252000, 396000} },
	{ { 255000,  84000, 288000, 420000, 252000, 396000} },
	{ { 229500,  84000, 288000, 420000, 252000, 396000} },
	{ { 204000,  84000, 288000, 420000, 252000, 396000} },
};

static struct balanced_throttle gpu_throttle = {
	.throt_tab_size	= ARRAY_SIZE(gpu_throttle_table),
	.throt_tab	= gpu_throttle_table,
};

/* throttle table that sets all clocks to approximately 50% of their max */
static struct throttle_table emergency_throttle_table[] = {
	/*      CPU,    GPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
	{ {1122000, 391000, 288000, 420000, 252000, 396000} },
};

static struct balanced_throttle emergency_throttle = {
	.throt_tab_size	= ARRAY_SIZE(emergency_throttle_table),
	.throt_tab	= emergency_throttle_table,
};

static int __init apalis_tk1_balanced_throttle_init(void)
{
	if (of_machine_is_compatible("nvidia,apalis-tk1")) {
		if (!balanced_throttle_register(&cpu_throttle, "cpu-balanced"))
			pr_err
			("balanced_throttle_register 'cpu-balanced' FAILED.\n");
		if (!balanced_throttle_register(&gpu_throttle, "gpu-balanced"))
			pr_err
			("balanced_throttle_register 'gpu-balanced' FAILED.\n");
		if (!balanced_throttle_register(&emergency_throttle,
						"emergency-balanced"))
			pr_err
		("balanced_throttle_register 'emergency-balanced' FAILED\n");
	}

	return 0;
}

late_initcall(apalis_tk1_balanced_throttle_init);

static struct nct1008_platform_data apalis_tk1_nct72_pdata = {
	.loc_name		= "tegra",
	.supported_hwrev	= true,
	.conv_rate		= 0x06, /* 4Hz conversion rate */
	.offset			= 0,
	.extended_range		= true,

	.sensors = {
		[LOC] = {
			.tzp		= &board_tzp,
			.shutdown_limit	= 120, /* C */
			.passive_delay	= 1000,
			.num_trips	= 1,
			.trips = {
				{
					.cdev_type	= "therm_est_activ",
					.trip_temp	= 40000,
					.trip_type	= THERMAL_TRIP_ACTIVE,
					.hysteresis	= 1000,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
					.mask		= 1,
				},
			},
		},
		[EXT] = {
			.tzp		= &cpu_tzp,
			.shutdown_limit = 95, /* C */
			.passive_delay	= 1000,
			.num_trips	= 2,
			.trips = {
				{
					.cdev_type	= "shutdown_warning",
					.trip_temp	= 93000,
					.trip_type	= THERMAL_TRIP_PASSIVE,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
					.mask		= 0,
				},
				{
					.cdev_type	= "cpu-balanced",
					.trip_temp	= 83000,
					.trip_type	= THERMAL_TRIP_PASSIVE,
					.upper		= THERMAL_NO_LIMIT,
					.lower		= THERMAL_NO_LIMIT,
					.hysteresis	= 1000,
					.mask		= 1,
				},
			}
		}
	}
};

static struct i2c_board_info apalis_tk1_i2c_nct72_board_info[] = {
	{
		I2C_BOARD_INFO("nct72", 0x4c),
		.platform_data	= &apalis_tk1_nct72_pdata,
		.irq		= -1,
	},
};

static int apalis_tk1_nct72_init(void)
{
	int nct72_port = TEGRA_GPIO_PI6;
	int ret = 0;
	int i;
	struct thermal_trip_info *trip_state;

	/* raise NCT's thresholds if soctherm CP,FT fuses are ok */
	if ((tegra_fuse_calib_base_get_cp(NULL, NULL) >= 0) &&
	    (tegra_fuse_calib_base_get_ft(NULL, NULL) >= 0)) {
		apalis_tk1_nct72_pdata.sensors[EXT].shutdown_limit += 20;
		for (i = 0; i < apalis_tk1_nct72_pdata.sensors[EXT].num_trips;
		     i++) {
			trip_state =
			    &apalis_tk1_nct72_pdata.sensors[EXT].trips[i];
			if (!strncmp
			    (trip_state->cdev_type, "cpu-balanced",
			     THERMAL_NAME_LENGTH)) {
				trip_state->cdev_type = "_none_";
				break;
			}
		}
	} else {
		tegra_platform_edp_init(
				apalis_tk1_nct72_pdata.sensors[EXT].trips,
				&apalis_tk1_nct72_pdata.sensors[EXT].num_trips,
				12000); /* edp temperature margin */
		tegra_add_cpu_vmax_trips(apalis_tk1_nct72_pdata.sensors[EXT].
					 trips,
					 &apalis_tk1_nct72_pdata.sensors[EXT].
					 num_trips);
		tegra_add_tgpu_trips(apalis_tk1_nct72_pdata.sensors[EXT].trips,
				     &apalis_tk1_nct72_pdata.sensors[EXT].
				     num_trips);
		tegra_add_vc_trips(apalis_tk1_nct72_pdata.sensors[EXT].trips,
				   &apalis_tk1_nct72_pdata.sensors[EXT].
				   num_trips);
		tegra_add_core_vmax_trips(apalis_tk1_nct72_pdata.sensors[EXT].
					  trips,
					  &apalis_tk1_nct72_pdata.sensors[EXT].
					  num_trips);
	}

	apalis_tk1_i2c_nct72_board_info[0].irq = gpio_to_irq(nct72_port);

	ret = gpio_request(nct72_port, "temp_alert");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(nct72_port);
	if (ret < 0) {
		pr_info("%s: calling gpio_free(nct72_port)", __func__);
		gpio_free(nct72_port);
	}

	apalis_tk1_nct72_pdata.sensors[EXT].shutdown_limit = 105;
	apalis_tk1_nct72_pdata.sensors[LOC].shutdown_limit = 100;
	i2c_register_board_info(4, apalis_tk1_i2c_nct72_board_info,
				1); /* only register device[0] */

	return ret;
}

struct ntc_thermistor_adc_table {
	int temp; /* degree C */
	int adc;
};

int __init apalis_tk1_sensors_init(void)
{
	apalis_tk1_nct72_init();

	return 0;
}
