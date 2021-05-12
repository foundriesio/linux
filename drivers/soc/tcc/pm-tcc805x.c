// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "tcc: pm: " fmt

#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/regmap.h>
#include <linux/mfd/da9062/core.h>
#include <linux/regulator/consumer.h>

struct pmic {
	struct regmap *da9062_regmap;
	struct regmap *da9131_regmap[2];
};

static struct pmic pmic;

static s32 pmic_ctrl_str_mode(u32 enter)
{
	struct regmap __maybe_unused *map;
	s32 __maybe_unused ret;

#if defined(CONFIG_REGULATOR_DA9062)
	map = pmic.da9062_regmap;
	if (map == NULL)
		return 0;

	/*
	 * Keep BUCK1/LDO2 on in power-down for STR support.
	 * Must be reset back to default config on wake-up.
	 */
	ret = regmap_update_bits(map, DA9062AA_BUCK1_CONT,
				 DA9062AA_BUCK1_CONF_MASK,
				 enter << DA9062AA_BUCK1_CONF_SHIFT);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(map, DA9062AA_LDO2_CONT,
				 DA9062AA_LDO2_CONF_MASK,
				 enter << DA9062AA_LDO2_CONF_SHIFT);
	if (ret < 0)
		return ret;
#endif

	return 0;
}

s32 pmic_sw_workaround(void)
{
	struct regmap __maybe_unused *map;
	s32 __maybe_unused i;
	s32 __maybe_unused ret;

#if defined(CONFIG_PM_TCC805X_DA9062_SW_WORKAROUND)
	map = pmic.da9062_regmap;
	if (map == NULL)
		return 0;

	/*
	 * S/W Workaround for power sequence issue (DA9062)
	 * - Delay CORE0P8 power off timing for 16.4 msec
	 * - Unmask SYS_EN IRQ to get wake-up signal
	 */
	ret = regmap_update_bits(map, DA9062AA_WAIT, 0xff, 0x96);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(map, DA9062AA_ID_32_31, 0xff, 0x03);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(map, DA9062AA_IRQ_MASK_C, 0x10, 0x00);
	if (ret < 0)
		return ret;
#endif
#if defined(CONFIG_PM_TCC805X_DA9131_SW_WORKAROUND)
	/*
	 * S/W Workaround for power sequence issue (DA9131 OTP-42/43 v1)
	 * - Adjust voltage slew rate correctly
	 * - Enable GPIO1/2 pull-up/pull-down
	 */
	for (i = 0; i < 2; i++) {
		map = pmic.da9131_regmap[i];
		if (map == NULL)
			return 0;

		ret = regmap_update_bits(map, 0x20, 0xff, 0x49);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(map, 0x21, 0xff, 0x49);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(map, 0x28, 0xff, 0x49);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(map, 0x29, 0xff, 0x49);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(map, 0x13, 0xff, 0x08);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(map, 0x15, 0xff, 0x08);
		if (ret < 0)
			return ret;
	}
#endif

	return 0;
}

static s32 tcc_pm_get_pmic(struct pmic *pmic)
{
	struct regulator __maybe_unused *regulator;
	bool __maybe_unused err;

	pmic->da9062_regmap = NULL;
	pmic->da9131_regmap[0] = NULL;
	pmic->da9131_regmap[1] = NULL;

#if defined(CONFIG_REGULATOR_DA9062)
	regulator = regulator_get_exclusive(NULL, "memq_1p1");
	err = IS_ERR(regulator);
	if (!err) {
		pmic->da9062_regmap = regulator_get_regmap(regulator);
		regulator_put(regulator);
		err = IS_ERR(pmic->da9062_regmap);
		if (err) {
			pmic->da9062_regmap = NULL;
			return -ENOENT;
		}
	}
#endif
#if defined(CONFIG_PM_TCC805X_DA9131_SW_WORKAROUND)
	/* D0 */
	regulator = regulator_get_exclusive(NULL, "core_0p8");
	err = IS_ERR(regulator);
	if (!err) {
		pmic->da9131_regmap[0] = regulator_get_regmap(regulator);
		regulator_put(regulator);
		err = IS_ERR(pmic->da9131_regmap[0]);
		if (err) {
			pmic->da9131_regmap[0] = NULL;
			return 0;
		}
	}

	/* D2 */
	regulator = regulator_get_exclusive(NULL, "cpu_1p0");
	err = IS_ERR(regulator);
	if (!err) {
		pmic->da9131_regmap[1] = regulator_get_regmap(regulator);
		regulator_put(regulator);
		err = IS_ERR(pmic->da9131_regmap[1]);
		if (err) {
			pmic->da9131_regmap[1] = NULL;
			return 0;
		}
	}
#endif

	return 0;
}

static int pm_notifier_call(struct notifier_block *nb, unsigned long action,
			    void *data)
{
	u32 enter_suspend;
	s32 ret = 0;

	if ((action != PM_SUSPEND_PREPARE) && (action != PM_POST_SUSPEND))
		return NOTIFY_DONE;

	enter_suspend = (action == PM_SUSPEND_PREPARE) ? 1 : 0;
	ret = pmic_ctrl_str_mode(enter_suspend);
	if (ret < 0)
		pr_err("Failed to control PMIC (err: %d)\n", ret);

	ret = pmic_sw_workaround();
	if (ret < 0)
		pr_err("Failed to adopt PMIC S/W workaround (err: %d)\n", ret);

	return (ret < 0) ? NOTIFY_BAD : NOTIFY_OK;
}

static struct notifier_block pm_notifier_block = {
	.notifier_call = &pm_notifier_call,
};

static int tcc_pm_init(void)
{
	s32 ret;

	/* Get PMIC regmap for handling on power events */
	ret = tcc_pm_get_pmic(&pmic);
	if (ret != 0) {
		pr_err("Failed to get DA9062 regmap (err: %d)\n", ret);
		goto fail;
	}

	/* Adopt S/W workaround for Dialog PMIC */
	ret = pmic_sw_workaround();
	if (ret != 0) {
		pr_err("Failed to adopt PMIC S/W workaround (err: %d)\n", ret);
		goto fail;
	}

	/* Initialize PM notifier for handling power events */
	ret = register_pm_notifier(&pm_notifier_block);
	if (ret != 0) {
		pr_err("Failed to register notifier (err: %d)\n", ret);
		goto fail;
	}

	return 0;

fail:
	return ret;
}

late_initcall(tcc_pm_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC805x power management driver");
