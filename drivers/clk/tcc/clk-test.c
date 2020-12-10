// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clocksource.h>

#include <linux/fs.h>
#include <linux/device.h>
#include <linux/major.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>

#include <linux/clk/tcc.h>
#if defined(CONFIG_ARCH_TCC898X)
#define TCC898X_CKC_DRIVER
#include "clk-tcc898x.h"
#else
#error
#endif

static char *common_var;

static struct tcc_ckc_ops *ckc_ops;

static ssize_t gpu_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count);
static ssize_t gpu_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf);
static ssize_t g2d_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count);
static ssize_t g2d_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf);
static ssize_t ddi_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count);
static ssize_t ddi_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf);
static ssize_t cpu1_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count);
static ssize_t cpu1_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf);
static ssize_t vbus_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count);
static ssize_t vbus_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf);
static ssize_t coda_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count);
static ssize_t coda_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf);
static ssize_t hevc_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count);
static ssize_t hevc_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf);
static ssize_t vp9_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count);
static ssize_t vp9_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf);
static ssize_t jpeg_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count);
static ssize_t jpeg_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf);
static ssize_t cm_power_store(struct class *cls, struct class_attribute *attr,
			      const char *buf, size_t count);
static ssize_t cm_power_show(struct class *cls, struct class_attribute *attr,
			     char *buf);
static ssize_t hsio_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count);
static ssize_t hsio_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf);

static struct class_attribute clktest_class_attrs[] = {
	__ATTR(gpu_power, 0644, gpu_power_show, gpu_power_store),
	__ATTR(g2d_power, 0644, g2d_power_show, g2d_power_store),
	__ATTR(ddi_power, 0644, ddi_power_show, ddi_power_store),
	__ATTR(cpu1_power, 0644, cpu1_power_show,
	       cpu1_power_store),
	__ATTR(vbus_power, 0644, vbus_power_show,
	       vbus_power_store),
	__ATTR(coda_power, 0644, coda_power_show,
	       coda_power_store),
	__ATTR(hevc_power, 0644, hevc_power_show,
	       hevc_power_store),
	__ATTR(vp9_power, 0644, vp9_power_show, vp9_power_store),
	__ATTR(jpeg_power, 0644, jpeg_power_show,
	       jpeg_power_store),
	__ATTR(cm_power, 0644, cm_power_show, cm_power_store),
	__ATTR(hsio_power, 0644, hsio_power_show,
	       hsio_power_store),
	__ATTR_NULL
};

static struct class clktest_class = {
	.name = "tcc_clk",
	.owner = THIS_MODULE,
	.class_attrs = clktest_class_attrs
};

static int clktest_init(void)
{
	char string[] = "nothing";

	common_var = kmalloc_array(strlen(string), sizeof(char), GFP_KERNEL);
	class_register(&clktest_class);
	snprintf(common_var, sizeof(char) * strlen(string) + 1, "%s", string);
	return 0;
}

static int tcc_clkctrl_enable(unsigned int id)
{
	if (ckc_ops->ckc_pmu_pwdn)
		ckc_ops->ckc_pmu_pwdn(id, false);
	if (ckc_ops->ckc_swreset)
		ckc_ops->ckc_swreset(id, false);
	if (ckc_ops->ckc_clkctrl_enable)
		ckc_ops->ckc_clkctrl_enable(id);

	return 0;
}

static void tcc_clkctrl_disable(unsigned int id)
{
	if (ckc_ops->ckc_clkctrl_disable)
		ckc_ops->ckc_clkctrl_disable(id);
	if (ckc_ops->ckc_swreset)
		ckc_ops->ckc_swreset(id, true);
	if (ckc_ops->ckc_pmu_pwdn)
		ckc_ops->ckc_pmu_pwdn(id, true);
}

static int tcc_vpubus_enable(unsigned int id)
{
	if (ckc_ops->ckc_vpubus_pwdn)
		ckc_ops->ckc_vpubus_pwdn(id, false);
	if (ckc_ops->ckc_vpubus_swreset)
		ckc_ops->ckc_vpubus_swreset(id, false);
	return 0;
}

static void tcc_vpubus_disable(unsigned int id)
{
	if (ckc_ops->ckc_vpubus_swreset)
		ckc_ops->ckc_vpubus_swreset(id, true);
	if (ckc_ops->ckc_vpubus_pwdn)
		ckc_ops->ckc_vpubus_pwdn(id, true);
}

static int tcc_hsiobus_enable(unsigned int id)
{
	if (ckc_ops->ckc_hsiobus_pwdn)
		ckc_ops->ckc_hsiobus_pwdn(id, false);
	if (ckc_ops->ckc_hsiobus_swreset)
		ckc_ops->ckc_hsiobus_swreset(id, false);

	return 0;
}

static void tcc_hsiobus_disable(unsigned int id)
{
	if (ckc_ops->ckc_hsiobus_swreset)
		ckc_ops->ckc_hsiobus_swreset(id, true);
	if (ckc_ops->ckc_hsiobus_pwdn)
		ckc_ops->ckc_hsiobus_pwdn(id, true);
}

static ssize_t gpu_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf)
{
	pr_info("GPU Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_GPU));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t gpu_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("GPU Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_clkctrl_enable(FBUS_GPU);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_clkctrl_disable(FBUS_GPU);
	}

	pr_info("GPU Power Control done\n");

	return count;
}

static ssize_t g2d_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf)
{
	pr_info("G2D Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_G2D));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t g2d_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("G2D Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_clkctrl_enable(FBUS_G2D);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_clkctrl_disable(FBUS_G2D);
	}

	pr_info("G2D Power Control done\n");

	return count;
}

static ssize_t ddi_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf)
{
	pr_info("G2D Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_DDI));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t ddi_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("DDI Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_clkctrl_enable(FBUS_DDI);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_clkctrl_disable(FBUS_DDI);
	}

	pr_info("DDI Power Control done\n");

	return count;
}

static ssize_t cpu1_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	pr_info("CPU1 Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_CPU1));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t cpu1_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("CPU1 Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_clkctrl_enable(FBUS_CPU1);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_clkctrl_disable(FBUS_CPU1);
	}

	pr_info("CPU1 Power Control done\n");

	return count;
}

static ssize_t vbus_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	pr_info("VBUS ALL Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_VBUS));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t vbus_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("VBUS ALL Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_clkctrl_enable(FBUS_VBUS);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_clkctrl_disable(FBUS_VBUS);
	}

	pr_info("VBUS Power Control done\n");

	return count;
}

static ssize_t coda_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	pr_info("Video CODA Power = %d\n",
	       ckc_ops->ckc_is_vpubus_pwdn(VIDEOBUS_CODA_CORE));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t coda_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("Video CODA Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		tcc_clkctrl_enable(FBUS_CODA);
		tcc_vpubus_enable(VIDEOBUS_CODA_CORE);
		tcc_vpubus_enable(VIDEOBUS_CODA_BUS);
	} else if (data == 0) {
		tcc_vpubus_disable(VIDEOBUS_CODA_CORE);
		tcc_vpubus_disable(VIDEOBUS_CODA_BUS);
		tcc_clkctrl_disable(FBUS_CODA);
	}

	pr_info("Video CODA Power Control done\n");

	return count;
}

static ssize_t hevc_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	pr_info("Video HEVC Power = %d\n",
	       ckc_ops->ckc_is_vpubus_pwdn(VIDEOBUS_CODA_CORE));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t hevc_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("Video HEVC Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		tcc_clkctrl_enable(FBUS_CHEVC);
		tcc_clkctrl_enable(FBUS_VHEVC);
		tcc_clkctrl_enable(FBUS_BHEVC);
		tcc_vpubus_enable(VIDEOBUS_HEVC_CORE);
		tcc_vpubus_enable(VIDEOBUS_HEVC_BUS);
	} else if (data == 0) {
		tcc_vpubus_disable(VIDEOBUS_HEVC_CORE);
		tcc_vpubus_disable(VIDEOBUS_HEVC_BUS);
		tcc_clkctrl_disable(FBUS_VHEVC);
		tcc_clkctrl_disable(FBUS_BHEVC);
		tcc_clkctrl_disable(FBUS_CHEVC);
	}

	pr_info("Video HEVC Power Control done\n");

	return count;
}

static ssize_t vp9_power_show(struct class *cls, struct class_attribute *attr,
			      char *buf)
{
	pr_info("Video VP9 Power = %d\n",
	       ckc_ops->ckc_is_vpubus_pwdn(VIDEOBUS_VP9));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t vp9_power_store(struct class *cls, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("Video VP9 Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_vpubus_enable(VIDEOBUS_VP9);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_vpubus_disable(VIDEOBUS_VP9);
	}

	pr_info("Video VP9 Power Control done\n");

	return count;
}

static ssize_t jpeg_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	pr_info("Video JPEG Power = %d\n",
	       ckc_ops->ckc_is_vpubus_pwdn(VIDEOBUS_JENC));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t jpeg_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("Video JPEG Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_vpubus_enable(VIDEOBUS_JENC);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_vpubus_disable(VIDEOBUS_JENC);
	}

	pr_info("Video JPEG Power Control done\n");

	return count;
}

static ssize_t cm_power_show(struct class *cls, struct class_attribute *attr,
			     char *buf)
{
	pr_info("CM BUS Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_CMBUS));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t cm_power_store(struct class *cls, struct class_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("CM BUS Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		/* data equals 1 */
		tcc_clkctrl_enable(FBUS_CMBUS);
	} else if (data == 0) {
		/* data equals 0 */
		tcc_clkctrl_disable(FBUS_CMBUS);
	}

	pr_info("CM BUS Power Control done\n");

	return count;
}

static ssize_t hsio_power_show(struct class *cls, struct class_attribute *attr,
			       char *buf)
{
	pr_info("HSIO BUS Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_HSIO));
	pr_info("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t hsio_power_store(struct class *cls, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data;
	int error;

	pr_info("HSIO BUS Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == 1) {
		tcc_clkctrl_enable(FBUS_HSIO);
		tcc_hsiobus_enable(HSIOBUS_TRNG);
		tcc_hsiobus_enable(HSIOBUS_GMAC);
		tcc_hsiobus_enable(HSIOBUS_CIPHER);
		tcc_hsiobus_enable(HSIOBUS_USB20H);
		tcc_hsiobus_enable(HSIOBUS_DWC_OTG);
	} else if (data == 0) {
		tcc_hsiobus_disable(HSIOBUS_TRNG);
		tcc_hsiobus_disable(HSIOBUS_GMAC);
		tcc_hsiobus_disable(HSIOBUS_CIPHER);
		tcc_hsiobus_disable(HSIOBUS_USB20H);
		tcc_hsiobus_disable(HSIOBUS_DWC_OTG);
		tcc_clkctrl_disable(FBUS_HSIO);
	}

	pr_info("HSIO BUS Power Control done\n");

	return count;
}

static void clktest_exit(void)
{
	class_unregister(&clktest_class);
	pr_info("In hello_exit function\n");
}

void tcc_ckctest_set_ops(struct tcc_ckc_ops *ops)
{
	ckc_ops = ops;
}

module_init(clktest_init);
module_exit(clktest_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Inc.");
