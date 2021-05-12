/*
 * Universal Flash Storage Host controller Platform bus based glue driver
 *
 * This code is based on drivers/scsi/ufs/ufshcd-pltfrm.c
 * Copyright (C) 2011-2013 Samsung India Software Operations
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 */

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"

#define UFSHCD_DEFAULT_LANES_PER_DIRECTION		2
static u8 conf0_desc_buf[0x90];
static u8 conf1_desc_buf[0x90];
static u8 conf2_desc_buf[0x90];
static u8 conf3_desc_buf[0x90];
static int is_read;

static ssize_t conf_write_to_storage_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1)) {

		if (is_read == 0) {
			dev_err(dev, "Need read operation!\n");
			goto out;
		}

		ufshcd_write_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				0, 0, conf0_desc_buf, hba->desc_size.conf_desc);
		ufshcd_write_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				1, 0, conf1_desc_buf, hba->desc_size.conf_desc);
		ufshcd_write_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				2, 0, conf2_desc_buf, hba->desc_size.conf_desc);
		ufshcd_write_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				3, 0, conf3_desc_buf, hba->desc_size.conf_desc);
	} else {
		dev_err(dev, "example : echo 1 > conf_write_to_storage\n");
		goto out;
	}

out:
	return count;
}
static DEVICE_ATTR_WO(conf_write_to_storage);

static ssize_t conf_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i;
	int *tmp;
	int offset = 0;

	offset += sprintf(buf,
		"====Configuration Desc Header (IDX=0)====\n");
	offset += sprintf(buf + offset,
		"bLength = 0x%x\n", conf0_desc_buf[0]);
	offset += sprintf(buf + offset,
		"bDescriptorIDN = 0x%x\n", conf0_desc_buf[1]);
	offset += sprintf(buf + offset,
		"bConfDescContinue = 0x%x\n", conf0_desc_buf[2]);
	offset += sprintf(buf + offset,
		"bBootEnable = 0x%x\n", conf0_desc_buf[3]);
	offset += sprintf(buf + offset,
		"bDescrAccessEn = 0x%x\n", conf0_desc_buf[4]);
	offset += sprintf(buf + offset,
		"bInitPowerMode = 0x%x\n", conf0_desc_buf[5]);
	offset += sprintf(buf + offset,
		"bHighPriorityLUN = 0x%x\n", conf0_desc_buf[6]);
	offset += sprintf(buf + offset,
		"bSecureRemovalType = 0x%x\n", conf0_desc_buf[7]);
	offset += sprintf(buf + offset,
		"bInitActiveICCLevel = 0x%x\n\n", conf0_desc_buf[8]);

	for (i = 0; i < 8; i++) {
		offset += sprintf(buf + offset,
			"====Unit Descriptor LUN : %d====\n", i);
		offset += sprintf(buf + offset,
			"bLUEnable = 0x%x\n", conf0_desc_buf[0x10 + 0x10*i]);
		offset += sprintf(buf + offset,
			"bMemoryType = 0x%x\n",
			conf0_desc_buf[0x10 + 0x10*i + 0x3]);
		tmp = (int *)&conf0_desc_buf[0x10 + 0x10*i + 0x4];
		offset += sprintf(buf + offset,
			"bNumAllocUnits = 0x%x\n", be32_to_cpu(*tmp));
		offset += sprintf(buf + offset,
			"bProvisiongType = 0x%x\n\n",
			conf0_desc_buf[0x10 + 0x10*i + 0xA]);
	}

	offset += sprintf(buf + offset,
		"====Configuration Desc Header (IDX=1)====\n");
	offset += sprintf(buf + offset,
		"bLength = 0x%x\n", conf1_desc_buf[0]);
	offset += sprintf(buf + offset,
		"bDescriptorIDN = 0x%x\n\n", conf1_desc_buf[1]);

	for (i = 0; i < 8; i++) {
		offset += sprintf(buf + offset,
			"====Unit Descriptor LUN : %d====\n", i + 0x8);
		offset += sprintf(buf + offset,
			"bLUEnable = 0x%x\n", conf1_desc_buf[0x10 + 0x10*i]);
		offset += sprintf(buf + offset,
			"bMemoryType = 0x%x\n",
			conf1_desc_buf[0x10 + 0x10*i + 0x3]);
		tmp = (int *)&conf1_desc_buf[0x10 + 0x10*i + 0x4];
		offset += sprintf(buf + offset,
			"bNumAllocUnits = 0x%x\n",
			be32_to_cpu(*tmp));
		offset += sprintf(buf + offset,
			"bProvisiongType = 0x%x\n\n",
			conf1_desc_buf[0x10 + 0x10*i + 0xA]);
	}

	offset += sprintf(buf + offset,
		"====Configuration Desc Header (IDX=2)====\n");
	offset += sprintf(buf + offset,
		"bLength = 0x%x\n", conf1_desc_buf[0]);
	offset += sprintf(buf + offset,
		"bDescriptorIDN = 0x%x\n\n", conf1_desc_buf[1]);

	for (i = 0; i < 8; i++) {
		offset += sprintf(buf + offset,
			"====Unit Descriptor LUN : %d====\n", i + 0x10);
		offset += sprintf(buf + offset,
			"bLUEnable = 0x%x\n", conf2_desc_buf[0x10 + 0x10*i]);
		offset += sprintf(buf + offset,
			"bMemoryType = 0x%x\n",
			conf2_desc_buf[0x10 + 0x10*i + 0x3]);
		tmp = (int *)&conf2_desc_buf[0x10 + 0x10*i + 0x4];
		offset += sprintf(buf + offset,
			"bNumAllocUnits = 0x%x\n", be32_to_cpu(*tmp));
		offset += sprintf(buf + offset,
			"bProvisiongType = 0x%x\n\n",
			conf2_desc_buf[0x10 + 0x10*i + 0xA]);
	}

	offset += sprintf(buf + offset,
		"====Configuration Desc Header (IDX=3)====\n");
	offset += sprintf(buf + offset,
		"bLength = 0x%x\n", conf1_desc_buf[0]);
	offset += sprintf(buf + offset,
		"bDescriptorIDN = 0x%x\n\n", conf1_desc_buf[1]);

	for (i = 0; i < 8; i++) {
		offset += sprintf(buf + offset,
		"====Unit Descriptor LUN : %d====\n", i + 0x18);
		offset += sprintf(buf + offset,
		"bLUEnable = 0x%x\n", conf3_desc_buf[0x10 + 0x10*i]);
		offset += sprintf(buf + offset,
		"bMemoryType = 0x%x\n", conf3_desc_buf[0x10 + 0x10*i + 0x3]);
		tmp = (int *)&conf3_desc_buf[0x10 + 0x10*i + 0x4];
		offset += sprintf(buf + offset,
		"bNumAllocUnits = 0x%x\n", be32_to_cpu(*tmp));
		offset += sprintf(buf + offset,
		"bProvisiongType = 0x%x\n\n",
		conf3_desc_buf[0x10 + 0x10*i + 0xA]);
	}

	offset += sprintf(buf + offset, "The End\n");
	return offset;
}

static ssize_t conf_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	char *ptr;
	char tmp_str[100];
	char *tmp_str2;
	uint32_t lun, bLUEnable, bMemoryType, dNumAllocUnits, bProvisioningType;
	uint32_t *tmp_val;
	int ret = 0;

	pr_info("How To Use : # echo LUN,bLUEnable,bMemoryType,dNumAllocUnits,bProvisioningType > conf\n"
		);
	pr_info("# echo 0,1,0,65535,3 > conf\n");
	pr_info(" LUN = 0 ~ 31\n");
	pr_info(" bLUEnable = 0, 1\n");
	pr_info(" bMemoryType = 0 (Normal Memory), 1 (System Code Memory), 2 (Non-Persistent Memory), 3 (Enhanced Memory 1), 4 (Enhanced Memory 2), 5 (Enhanced Memory 3), 6 (Enhanced Memory 4)\n"
		);
	pr_info(" dNumAllocUnits (bytes)  = The value shall be calculated considering the capacity adjustment factor of the selected memory type\n"
		);
	pr_info(" bProvisioningType = 0 (Thin Provisioning is disabled), 2 (Thin Provisioning is enabled and TPRZ = 0), 3 (This Provisioning is enabled and TPRZ = 1)\n"
		);

	strcpy(tmp_str, buf);

	tmp_str2 = tmp_str;
	ptr = strsep(&tmp_str2, ",");
	ret = kstrtoul(ptr, 0, (ulong *)&lun);
	ptr = strsep(&tmp_str2, ",");
	ret = kstrtoul(ptr, 0, (ulong *)&bLUEnable);
	ptr = strsep(&tmp_str2, ",");
	ret = kstrtoul(ptr, 0, (ulong *)&bMemoryType);
	ptr = strsep(&tmp_str2, ",");
	ret = kstrtoul(ptr, 0, (ulong *)&dNumAllocUnits);
	ptr = strsep(&tmp_str2, ",");
	ret = kstrtoul(ptr, 0, (ulong *)&bProvisioningType);

	if (lun >= 0 && lun < 8) {
		conf0_desc_buf[0x10 + 0x10*lun] = bLUEnable;
		conf0_desc_buf[0x10 + 0x10*lun + 0x3] = bMemoryType;
		tmp_val = (uint32_t *)&conf0_desc_buf[0x10 + 0x10*lun + 0x4];
		*tmp_val = cpu_to_be32(dNumAllocUnits);
		conf0_desc_buf[0x10 + 0x10*lun + 0xA] = bProvisioningType;
	} else if (lun >= 8 && lun < 16) {
		lun -= 8;
		conf1_desc_buf[0x10 + 0x10*lun] = bLUEnable;
		conf1_desc_buf[0x10 + 0x10*lun + 0x3] = bMemoryType;
		tmp_val = (uint32_t *)&conf1_desc_buf[0x10 + 0x10*lun + 0x4];
		*tmp_val = cpu_to_be32(dNumAllocUnits);
		conf1_desc_buf[0x10 + 0x10*lun + 0xA] = bProvisioningType;
	} else if (lun >= 16 && lun < 24) {
		lun -= 16;
		conf2_desc_buf[0x10 + 0x10*lun] = bLUEnable;
		conf2_desc_buf[0x10 + 0x10*lun + 0x3] = bMemoryType;
		tmp_val = (uint32_t *)&conf2_desc_buf[0x10 + 0x10*lun + 0x4];
		*tmp_val = cpu_to_be32(dNumAllocUnits);
		conf2_desc_buf[0x10 + 0x10*lun + 0xA] = bProvisioningType;
	} else if (lun >= 24 && lun < 32) {
		lun -= 24;
		conf3_desc_buf[0x10 + 0x10*lun] = bLUEnable;
		conf3_desc_buf[0x10 + 0x10*lun + 0x3] = bMemoryType;
		tmp_val = (uint32_t *)&conf3_desc_buf[0x10 + 0x10*lun + 0x4];
		*tmp_val = cpu_to_be32(dNumAllocUnits);
		conf3_desc_buf[0x10 + 0x10*lun + 0xA] = bProvisioningType;
	}

	return count;
}

static DEVICE_ATTR(conf, 0644,
		   conf_show, conf_store);

static ssize_t conf_read_from_storage_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1)) {
		is_read = 1;
		ufshcd_read_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				0, 0, conf0_desc_buf, hba->desc_size.conf_desc);
		ufshcd_read_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				1, 0, conf1_desc_buf, hba->desc_size.conf_desc);
		ufshcd_read_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				2, 0, conf2_desc_buf, hba->desc_size.conf_desc);
		ufshcd_read_desc_param(hba, QUERY_DESC_IDN_CONFIGURATION,
				3, 0, conf3_desc_buf, hba->desc_size.conf_desc);

		dev_info(dev, "read complete!\n");
	} else {
		dev_err(dev, "example : echo 1 > conf_read_from_storage\n");
	}

	return count;
}

static DEVICE_ATTR_WO(conf_read_from_storage);

static ssize_t geometry_desc_show(struct device *dev,
			struct device_attribute *attr,
			 char *buf)
{
	char str[1024] = { 0 };
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long long *qTotal;
	unsigned long *dSeg;

	u8 desc_buf[hba->desc_size.geom_desc];

	ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY,
				  0, 0, desc_buf, hba->desc_size.geom_desc);

	qTotal = (unsigned long long *)&desc_buf[GEOMETRY_DESC_PARAM_DEV_CAP];
	dSeg = (unsigned long *)&desc_buf[GEOMETRY_DESC_PARAM_SEG_SIZE];
	sprintf(str,
		"qTotalRawDeviceCapacity = 0x%llx (bytes)\ndSegmentSize = 0x%x\nbAllocationUnitSize = 0x%x\n",
		be64_to_cpu(*qTotal) << 9, be32_to_cpu(*dSeg),
		desc_buf[GEOMETRY_DESC_PARAM_ALLOC_UNIT_SIZE]);
	return sprintf(buf, "%s", str);
}

static DEVICE_ATTR_RO(geometry_desc);

static int ufshcd_parse_clock_info(struct ufs_hba *hba)
{
	int ret = 0;
	int cnt;
	int i;
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	char *name;
	u32 *clkfreq = NULL;
	struct ufs_clk_info *clki;
	int len = 0;
	size_t sz = 0;

	if (!np)
		goto out;

	cnt = of_property_count_strings(np, "clock-names");
	if (!cnt || (cnt == -EINVAL)) {
		dev_info(dev, "%s: Unable to find clocks, assuming enabled\n",
				__func__);
	} else if (cnt < 0) {
		dev_err(dev, "%s: count clock strings failed, err %d\n",
				__func__, cnt);
		ret = cnt;
	}

	if (cnt <= 0)
		goto out;

	if (!of_get_property(np, "freq-table-hz", &len)) {
		dev_info(dev, "freq-table-hz property not specified\n");
		goto out;
	}

	if (len <= 0)
		goto out;

	sz = len / sizeof(*clkfreq);
	if (sz != 2 * cnt) {
		dev_err(dev, "%s len mismatch\n", "freq-table-hz");
		ret = -EINVAL;
		goto out;
	}

	clkfreq = devm_kzalloc(dev, sz * sizeof(*clkfreq),
			GFP_KERNEL);
	if (!clkfreq) {
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32_array(np, "freq-table-hz",
			clkfreq, sz);
	if (ret && (ret != -EINVAL)) {
		dev_err(dev, "%s: error reading array %d\n",
				"freq-table-hz", ret);
		return ret;
	}

	for (i = 0; i < sz; i += 2) {
		ret = of_property_read_string_index(np,
				"clock-names", i/2, (const char **)&name);
		if (ret)
			goto out;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki) {
			ret = -ENOMEM;
			goto out;
		}

		clki->min_freq = clkfreq[i];
		clki->max_freq = clkfreq[i+1];
		clki->name = kstrdup(name, GFP_KERNEL);
		dev_dbg(dev, "%s: min %u max %u name %s\n", "freq-table-hz",
				clki->min_freq, clki->max_freq, clki->name);
		list_add_tail(&clki->list, &hba->clk_list_head);
	}
out:
	return ret;
}

#define MAX_PROP_SIZE 32
static int ufshcd_populate_vreg(struct device *dev, const char *name,
		struct ufs_vreg **out_vreg)
{
	int ret = 0;
	char prop_name[MAX_PROP_SIZE];
	struct ufs_vreg *vreg = NULL;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "%s: non DT initialization\n", __func__);
		goto out;
	}

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "%s: Unable to find %s regulator, assuming enabled\n",
				__func__, prop_name);
		goto out;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	vreg->name = kstrdup(name, GFP_KERNEL);

	/* if fixed regulator no need further initialization */
	snprintf(prop_name, MAX_PROP_SIZE, "%s-fixed-regulator", name);
	if (of_property_read_bool(np, prop_name))
		goto out;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-max-microamp", name);
	ret = of_property_read_u32(np, prop_name, &vreg->max_uA);
	if (ret) {
		dev_err(dev, "%s: unable to find %s err %d\n",
				__func__, prop_name, ret);
		goto out;
	}

	vreg->min_uA = 0;
	if (!strcmp(name, "vcc")) {
		if (of_property_read_bool(np, "vcc-supply-1p8")) {
			vreg->min_uV = UFS_VREG_VCC_1P8_MIN_UV;
			vreg->max_uV = UFS_VREG_VCC_1P8_MAX_UV;
		} else {
			vreg->min_uV = UFS_VREG_VCC_MIN_UV;
			vreg->max_uV = UFS_VREG_VCC_MAX_UV;
		}
	} else if (!strcmp(name, "vccq")) {
		vreg->min_uV = UFS_VREG_VCCQ_MIN_UV;
		vreg->max_uV = UFS_VREG_VCCQ_MAX_UV;
	} else if (!strcmp(name, "vccq2")) {
		vreg->min_uV = UFS_VREG_VCCQ2_MIN_UV;
		vreg->max_uV = UFS_VREG_VCCQ2_MAX_UV;
	}

	goto out;

out:
	if (!ret)
		*out_vreg = vreg;
	return ret;
}

/**
 * ufshcd_parse_regulator_info - get regulator info from device tree
 * @hba: per adapter instance
 *
 * Get regulator info from device tree for vcc, vccq, vccq2 power supplies.
 * If any of the supplies are not defined it is assumed that they are always-on
 * and hence return zero. If the property is defined but parsing is failed
 * then return corresponding error.
 */
static int ufshcd_parse_regulator_info(struct ufs_hba *hba)
{
	int err;
	struct device *dev = hba->dev;
	struct ufs_vreg_info *info = &hba->vreg_info;

	err = ufshcd_populate_vreg(dev, "vdd-hba", &info->vdd_hba);
	if (err)
		goto out;

	err = ufshcd_populate_vreg(dev, "vcc", &info->vcc);
	if (err)
		goto out;

	err = ufshcd_populate_vreg(dev, "vccq", &info->vccq);
	if (err)
		goto out;

	err = ufshcd_populate_vreg(dev, "vccq2", &info->vccq2);
out:
	return err;
}

#ifdef CONFIG_PM
/**
 * ufshcd_pltfrm_suspend - suspend power management function
 * @dev: pointer to device handle
 *
 * Returns 0 if successful
 * Returns non-zero otherwise
 */
int ufshcd_pltfrm_suspend(struct device *dev)
{
	return ufshcd_system_suspend(dev_get_drvdata(dev));
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_suspend);

/**
 * ufshcd_pltfrm_resume - resume power management function
 * @dev: pointer to device handle
 *
 * Returns 0 if successful
 * Returns non-zero otherwise
 */
int ufshcd_pltfrm_resume(struct device *dev)
{
	return ufshcd_system_resume(dev_get_drvdata(dev));
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_resume);

int ufshcd_pltfrm_runtime_suspend(struct device *dev)
{
	return ufshcd_runtime_suspend(dev_get_drvdata(dev));
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_runtime_suspend);

int ufshcd_pltfrm_runtime_resume(struct device *dev)
{
	return ufshcd_runtime_resume(dev_get_drvdata(dev));
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_runtime_resume);

int ufshcd_pltfrm_runtime_idle(struct device *dev)
{
	return ufshcd_runtime_idle(dev_get_drvdata(dev));
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_runtime_idle);

#endif /* CONFIG_PM */

void ufshcd_pltfrm_shutdown(struct platform_device *pdev)
{
	ufshcd_shutdown((struct ufs_hba *)platform_get_drvdata(pdev));
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_shutdown);

static void ufshcd_init_lanes_per_dir(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	int ret;

	ret = of_property_read_u32(dev->of_node, "lanes-per-direction",
		&hba->lanes_per_direction);
	if (ret) {
		dev_dbg(hba->dev,
			"%s: failed to read lanes-per-direction, ret=%d\n",
			__func__, ret);
		hba->lanes_per_direction = UFSHCD_DEFAULT_LANES_PER_DIRECTION;
	}
}

/**
 * ufshcd_pltfrm_init - probe routine of the driver
 * @pdev: pointer to Platform device handle
 * @vops: pointer to variant ops
 *
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_pltfrm_init(struct platform_device *pdev,
		       struct ufs_hba_variant_ops *vops)
{
	struct ufs_hba *hba;
	void __iomem *mmio_base;
	struct resource *mem_res;
	int irq, err;
	struct device *dev = &pdev->dev;
	int ret;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mmio_base = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR(mmio_base)) {
		err = PTR_ERR(mmio_base);
		goto out;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "IRQ resource not available\n");
		err = -ENODEV;
		goto out;
	}

	err = ufshcd_alloc_host(dev, &hba);
	if (err) {
		dev_err(&pdev->dev, "Allocation failed\n");
		goto out;
	}

	hba->vops = vops;

	err = ufshcd_parse_clock_info(hba);
	if (err) {
		dev_err(&pdev->dev, "%s: clock parse failed %d\n",
				__func__, err);
		goto dealloc_host;
	}
	err = ufshcd_parse_regulator_info(hba);
	if (err) {
		dev_err(&pdev->dev, "%s: regulator init failed %d\n",
				__func__, err);
		goto dealloc_host;
	}

	ufshcd_init_lanes_per_dir(hba);

	err = ufshcd_init(hba, mmio_base, irq);
	if (err) {
		dev_err(dev, "Initialization failed\n");
		goto dealloc_host;
	}

	platform_set_drvdata(pdev, hba);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = device_create_file(&pdev->dev, &dev_attr_geometry_desc);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][UFS] failed to create geometry_desc\n");
		goto dealloc_host;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_conf_read_from_storage);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][UFS] failed to create conf_read_from_storage\n");
		goto dealloc_host;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_conf_write_to_storage);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][UFS] failed to create conf_write_to_storage\n");
		goto dealloc_host;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_conf);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][UFS] failed to create conf\n");
		goto dealloc_host;
	}

	return 0;

dealloc_host:
	ufshcd_dealloc_host(hba);
out:
	return err;
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_init);

int ufshcd_pltfrm_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_conf_read_from_storage);
	device_remove_file(&pdev->dev, &dev_attr_conf_write_to_storage);
	device_remove_file(&pdev->dev, &dev_attr_geometry_desc);
	device_remove_file(&pdev->dev, &dev_attr_conf);

	return 0;
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_remove);

MODULE_AUTHOR("Santosh Yaragnavi <santosh.sy@samsung.com>");
MODULE_AUTHOR("Vinayak Holikatti <h.vinayak@samsung.com>");
MODULE_DESCRIPTION("UFS host controller Platform bus based glue driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(UFSHCD_DRIVER_VERSION);
