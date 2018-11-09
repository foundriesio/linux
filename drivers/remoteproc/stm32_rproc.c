// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics 2018 - All Rights Reserved
 * Authors: Ludovic Barre <ludovic.barre@st.com> for STMicroelectronics.
 *          Fabien Dessenne <fabien.dessenne@st.com> for STMicroelectronics.
 */

#include <linux/arm-smccc.h>
#include <linux/interrupt.h>
#include <linux/mailbox_client.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>
#include <linux/reset.h>

#include "remoteproc_internal.h"

#define HOLD_BOOT		0
#define RELEASE_BOOT		1

#define MBOX_NB_VQ		2
#define MBOX_NB_MBX		3

#define STM32_SMC_RCC		0x82001000
#define STM32_SMC_REG_WRITE	0x1

#define STM32_MBX_VQ0		"vq0"
#define STM32_MBX_VQ1		"vq1"
#define STM32_MBX_SHUTDOWN	"init_shdn"

struct stm32_syscon {
	struct regmap *map;
	u32 reg;
	u32 mask;
};

struct stm32_rproc_mem {
	void __iomem *cpu_addr;
	phys_addr_t bus_addr;
	u32 dev_addr;
	size_t size;
};

struct stm32_mbox {
	const unsigned char name[10];
	struct mbox_chan *chan;
	struct mbox_client client;
	int vq_id;
};

struct stm32_rproc {
	struct reset_control *rst;
	struct stm32_syscon hold_boot;
	struct stm32_syscon pdds;
	struct stm32_rproc_mem ram[2];
	struct stm32_mbox mb[MBOX_NB_MBX];
	bool secured_soc;
	u32 rsc_addr;
	u32 rsc_len;
};

static int stm32_rproc_elf_load_segments(struct rproc *rproc,
					 const struct firmware *fw)
{
	if (!rproc->early_boot)
		return rproc_elf_load_segments(rproc, fw);

	return 0;
}

static int stm32_rproc_mbox_idx(struct rproc *rproc, const unsigned char *name)
{
	struct stm32_rproc *ddata = rproc->priv;
	int i;

	for (i = 0; i < ARRAY_SIZE(ddata->mb); i++) {
		if (!strncmp(ddata->mb[i].name, name, strlen(name)))
			return i;
	}
	dev_err(&rproc->dev, "mailbox %s not found\n", name);

	return -EINVAL;
}

static int stm32_rproc_elf_load_rsc_table(struct rproc *rproc,
					  const struct firmware *fw)
{
	int status;
	struct resource_table *table = NULL;
	struct stm32_rproc *ddata = rproc->priv;
	size_t tablesz = 0;

	if (!rproc->early_boot) {
		status = rproc_elf_load_rsc_table(rproc, fw);
		if (status)
			goto no_rsc_table;

		return 0;
	}

	if (ddata->rsc_addr) {
		tablesz = ddata->rsc_len;
		table = (struct resource_table *)
				rproc_da_to_va(rproc, (u64)ddata->rsc_addr,
					       ddata->rsc_len);
		rproc->cached_table = kmemdup(table, tablesz, GFP_KERNEL);
		if (!rproc->cached_table)
			return -ENOMEM;

		rproc->table_ptr = rproc->cached_table;
		rproc->table_sz = tablesz;
		return 0;
	}

	rproc->cached_table = NULL;
	rproc->table_ptr = NULL;
	rproc->table_sz = 0;

no_rsc_table:
	dev_warn(&rproc->dev, "not resource table found for this firmware\n");
	return 0;
}

static struct resource_table *
stm32_rproc_elf_find_loaded_rsc_table(struct rproc *rproc,
				      const struct firmware *fw)
{
	struct stm32_rproc *ddata = rproc->priv;

	if (!rproc->early_boot)
		return rproc_elf_find_loaded_rsc_table(rproc, fw);

	if (ddata->rsc_addr)
		return (struct resource_table *)
				rproc_da_to_va(rproc, (u64)ddata->rsc_addr,
					       ddata->rsc_len);

	return NULL;
}

static int stm32_rproc_elf_sanity_check(struct rproc *rproc,
					const struct firmware *fw)
{
	if (!rproc->early_boot)
		return rproc_elf_sanity_check(rproc, fw);

	return 0;
}

static u32 stm32_rproc_elf_get_boot_addr(struct rproc *rproc,
					 const struct firmware *fw)
{
	if (!rproc->early_boot)
		return rproc_elf_get_boot_addr(rproc, fw);

	return 0;
}

static irqreturn_t stm32_rproc_wdg(int irq, void *data)
{
	struct rproc *rproc = data;

	rproc_report_crash(rproc, RPROC_WATCHDOG);

	return IRQ_HANDLED;
}

static void stm32_rproc_mb_callback(struct mbox_client *cl, void *data)
{
	struct rproc *rproc = dev_get_drvdata(cl->dev);
	struct stm32_mbox *mb = container_of(cl, struct stm32_mbox, client);

	if (rproc_vq_interrupt(rproc, mb->vq_id) == IRQ_NONE)
		dev_dbg(&rproc->dev, "no message found in vq%d\n", mb->vq_id);
}

static void stm32_rproc_free_mbox(struct rproc *rproc)
{
	struct stm32_rproc *ddata = rproc->priv;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ddata->mb); i++) {
		if (ddata->mb[i].chan)
			mbox_free_channel(ddata->mb[i].chan);
		ddata->mb[i].chan = NULL;
	}
}

static const struct stm32_mbox stm32_rproc_mbox[MBOX_NB_MBX] = {
	{
		.name = STM32_MBX_VQ0,
		.vq_id = 0,
		.client = {
			.rx_callback = stm32_rproc_mb_callback,
			.tx_block = false,
		},
	},
	{
		.name = STM32_MBX_VQ1,
		.vq_id = 1,
		.client = {
			.rx_callback = stm32_rproc_mb_callback,
			.tx_block = false,
		},
	},
	{
		.name = STM32_MBX_SHUTDOWN,
		.vq_id = -1,
		.client = {
			.tx_block = true,
			.tx_done = NULL,
			.tx_tout = 500, /* 500 ms time out */
		},
	}
};

static void stm32_rproc_request_mbox(struct rproc *rproc)
{
	struct stm32_rproc *ddata = rproc->priv;
	struct device *dev = &rproc->dev;
	unsigned int i;
	const unsigned char *name;
	struct mbox_client *cl;

	/* Initialise mailbox structure table */
	memcpy(ddata->mb, stm32_rproc_mbox, sizeof(stm32_rproc_mbox));

	for (i = 0; i < MBOX_NB_MBX; i++) {
		name = ddata->mb[i].name;

		cl = &ddata->mb[i].client;
		cl->dev = dev->parent;

		ddata->mb[i].chan = mbox_request_channel_byname(cl, name);
		if (IS_ERR(ddata->mb[i].chan)) {
			dev_warn(dev, "cannot get %s mbox\n", name);
			ddata->mb[i].chan = NULL;
		}
	}
}

static int stm32_rproc_set_hold_boot(struct rproc *rproc, bool hold)
{
	struct stm32_rproc *ddata = rproc->priv;
	struct stm32_syscon hold_boot = ddata->hold_boot;
	struct arm_smccc_res smc_res;
	int val, err;

	val = hold ? HOLD_BOOT : RELEASE_BOOT;

	if (ddata->secured_soc) {
		arm_smccc_smc(STM32_SMC_RCC, STM32_SMC_REG_WRITE,
			      hold_boot.reg, val, 0, 0, 0, 0, &smc_res);
		err = smc_res.a0;
	} else {
		err = regmap_update_bits(hold_boot.map, hold_boot.reg,
					 hold_boot.mask, val);
	}

	if (err)
		dev_err(&rproc->dev, "failed to set hold boot\n");

	return err;
}

static int stm32_rproc_start(struct rproc *rproc)
{
	int err;

	/*
	 * If M4 previously started by bootloader, just guarantee holdboot
	 *  is set to catch any crash.
	 */
	if (!rproc->early_boot) {
		err = stm32_rproc_set_hold_boot(rproc, false);
		if (err)
			return err;
	}

	return stm32_rproc_set_hold_boot(rproc, true);
}

static int stm32_rproc_stop(struct rproc *rproc)
{
	struct stm32_rproc *ddata = rproc->priv;
	int err, dummy_data, idx;

	/* request shutdown of the remote processor */
	if (rproc->state != RPROC_OFFLINE) {
		idx = stm32_rproc_mbox_idx(rproc, STM32_MBX_SHUTDOWN);
		if (idx >= 0 && ddata->mb[idx].chan) {
			/* a dummy data is sent to allow to block on transmit */
			err = mbox_send_message(ddata->mb[idx].chan,
						&dummy_data);
			if (err < 0)
				dev_warn(&rproc->dev, "warning: remote FW shutdown without ack\n");
		}
	}

	err = stm32_rproc_set_hold_boot(rproc, true);
	if (err)
		return err;

	err = reset_control_assert(ddata->rst);
	if (err) {
		dev_err(&rproc->dev, "failed to assert the reset\n");
		return err;
	}

	/* to allow platform Standby power mode, set remote proc Deep Sleep */
	if (ddata->pdds.map) {
		err = regmap_update_bits(ddata->pdds.map, ddata->pdds.reg,
					 ddata->pdds.mask, 1);
		if (err) {
			dev_err(&rproc->dev, "failed to set pdds\n");
			return err;
		}
	}

	/* Reset early_boot state as we stop the co-processor */
	rproc->early_boot = false;

	return 0;
}

static void stm32_rproc_kick(struct rproc *rproc, int vqid)
{
	struct stm32_rproc *ddata = rproc->priv;
	unsigned int i;
	int err;

	if (WARN_ON(vqid >= MBOX_NB_VQ))
		return;

	for (i = 0; i < MBOX_NB_MBX; i++) {
		if (vqid != ddata->mb[i].vq_id)
			continue;
		if (!ddata->mb[i].chan)
			return;
		err = mbox_send_message(ddata->mb[i].chan, (void *)vqid);
		if (err < 0)
			dev_err(&rproc->dev, "%s: failed (%s, err:%d)\n",
				__func__, ddata->mb[i].name, err);
		return;
	}
}

static void *stm32_rproc_da_to_va(struct rproc *rproc, u64 da, int len)
{
	struct stm32_rproc *ddata = rproc->priv;
	void *va = NULL;
	u32 offset;
	unsigned int i;

	for (i = 0; i < 2; i++) {
		if (da >= ddata->ram[i].dev_addr && da + len <=
		    ddata->ram[i].dev_addr + ddata->ram[i].size) {
			offset = da - ddata->ram[i].dev_addr;
			/* __force to make sparse happy with type conversion */
			va = (__force void *)(ddata->ram[i].cpu_addr + offset);
			break;
		}
	}

	return va;
}

static struct rproc_ops st_rproc_ops = {
	.start		= stm32_rproc_start,
	.stop		= stm32_rproc_stop,
	.kick		= stm32_rproc_kick,
	.da_to_va	= stm32_rproc_da_to_va,
	.load = stm32_rproc_elf_load_segments,
	.parse_fw = stm32_rproc_elf_load_rsc_table,
	.find_loaded_rsc_table = stm32_rproc_elf_find_loaded_rsc_table,
	.sanity_check = stm32_rproc_elf_sanity_check,
	.get_boot_addr = stm32_rproc_elf_get_boot_addr,
};

static const struct of_device_id stm32_rproc_match[] = {
	{ .compatible = "st,stm32mp1-rproc" },
	{},
};
MODULE_DEVICE_TABLE(of, stm32_rproc_match);

static int stm32_rproc_get_syscon(struct device_node *np, const char *prop,
				  struct stm32_syscon *syscon)
{
	int err = 0;

	syscon->map = syscon_regmap_lookup_by_phandle(np, prop);
	if (IS_ERR(syscon->map)) {
		err = PTR_ERR(syscon->map);
		syscon->map = NULL;
		goto out;
	}

	err = of_property_read_u32_index(np, prop, 1, &syscon->reg);
	if (err)
		goto out;

	err = of_property_read_u32_index(np, prop, 2, &syscon->mask);

out:
	return err;
}

static int stm32_rproc_parse_dt(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct stm32_rproc *ddata = rproc->priv;
	struct resource *res;
	struct stm32_syscon tz;
	unsigned int tzen, i = 0;
	int err, irq;

	irq = platform_get_irq_byname(pdev, "wdg");
	if (irq > 0) {
		err = devm_request_irq(dev, irq, stm32_rproc_wdg, 0,
				       dev_name(dev), rproc);
		if (err) {
			dev_err(dev, "failed to request wdg irq\n");
			return err;
		}

		dev_info(dev, "wdg irq registered\n");
	}

	ddata->rst = devm_reset_control_get(dev, "mcu_rst");
	if (IS_ERR(ddata->rst)) {
		dev_err(dev, "failed to get mcu reset\n");
		return PTR_ERR(ddata->rst);
	}

	/*
	 * if platform is secured the hold boot bit must be written by
	 * smc call and read normally.
	 * if not secure the hold boot bit could be read/write normally
	 */
	err = stm32_rproc_get_syscon(np, "st,syscfg-tz", &tz);
	if (err) {
		dev_err(dev, "failed to get tz syscfg\n");
		return err;
	}

	err = regmap_read(tz.map, tz.reg, &tzen);
	if (err) {
		dev_err(&rproc->dev, "failed to read tzen\n");
		return err;
	}
	ddata->secured_soc = tzen & tz.mask;

	err = stm32_rproc_get_syscon(np, "st,syscfg-holdboot",
				     &ddata->hold_boot);
	if (err) {
		dev_err(dev, "failed to get hold boot\n");
		return err;
	}

	err = stm32_rproc_get_syscon(np, "st,syscfg-pdds", &ddata->pdds);
	if (err)
		dev_warn(dev, "failed to get pdds\n");

	while ((res = platform_get_resource(pdev, IORESOURCE_MEM, i))) {
		ddata->ram[i].cpu_addr = devm_ioremap_resource(dev, res);
		if (IS_ERR(ddata->ram[i].cpu_addr))
			return err;

		ddata->ram[i].bus_addr = res->start;
		ddata->ram[i].size = resource_size(res);

		/*
		 * the m4 has retram at address 0 in its view (DA)
		 * so for retram DA=0x0 PA=bus_addr else DA=PA=bus_addr
		 */
		if (i == 0)
			ddata->ram[i].dev_addr = 0x0;
		else
			ddata->ram[i].dev_addr = ddata->ram[i].bus_addr;

		i++;
	}

	rproc->auto_boot = of_property_read_bool(np, "auto_boot");
	rproc->recovery_disabled = !of_property_read_bool(np, "recovery");

	if (of_property_read_bool(np, "early-booted")) {
		rproc->early_boot = true;

		err = of_property_read_u32(np, "rsc-address", &ddata->rsc_addr);
		if (!err) {
			err = of_property_read_u32(np, "rsc-size",
						   &ddata->rsc_len);

			if (err) {
				dev_err(dev, "resource table size required as address defined\n");
				return err;
			}
		}
	}

	of_reserved_mem_device_init(dev);

	return 0;
}

static int stm32_rproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stm32_rproc *ddata;
	struct device_node *np = dev->of_node;
	struct rproc *rproc;
	int ret;

	rproc = rproc_alloc(dev, np->name, &st_rproc_ops, NULL, sizeof(*ddata));
	if (!rproc)
		return -ENOMEM;

	rproc->has_iommu = false;
	ddata = rproc->priv;

	platform_set_drvdata(pdev, rproc);

	ret = stm32_rproc_parse_dt(pdev);
	if (ret)
		goto free_rproc;

	if (!rproc->early_boot) {
		ret = stm32_rproc_stop(rproc);
		if (ret)
			goto free_mem;
	}

	stm32_rproc_request_mbox(rproc);

	ret = rproc_add(rproc);
	if (ret)
		goto free_mb;

	return 0;

free_mb:
	stm32_rproc_free_mbox(rproc);
free_mem:
	of_reserved_mem_device_release(dev);
free_rproc:
	rproc_free(rproc);
	return ret;
}

static int stm32_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	if (atomic_read(&rproc->power) > 0)
		dev_warn(dev, "Releasing rproc while firmware running!\n");

	rproc_del(rproc);
	stm32_rproc_free_mbox(rproc);
	of_reserved_mem_device_release(dev);
	rproc_free(rproc);

	return 0;
}

static struct platform_driver stm32_rproc_driver = {
	.probe = stm32_rproc_probe,
	.remove = stm32_rproc_remove,
	.driver = {
		.name = "stm32-rproc",
		.of_match_table = of_match_ptr(stm32_rproc_match),
	},
};
module_platform_driver(stm32_rproc_driver);

MODULE_DESCRIPTION("STM32 Remote Processor Control Driver");
MODULE_AUTHOR("Ludovic Barre <ludovic.barre@st.com>");
MODULE_LICENSE("GPL v2");

