// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018/2019 ARM Ltd.
 *
 * This quirk is to mask the following issues:
 * - PCIE SLVERR: config space accesses to invalid PCIe BDFs cause a bus
 *		  error (signalled as an asynchronous SError)
 * - MCFG BDF mapping: the root complex is mapped separately from the device
 *		       config space
 * - Non 32-bit accesses to config space are not supported.
 *
 * At boot time the SCP board firmware creates a discovery table with
 * the root complex' base address and the valid BDF values, discovered while
 * scanning the config space and catching the SErrors.
 * Linux responds only to the EPs listed in this table, returning NULL
 * for the rest.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/sizes.h>
#include <linux/of_pci.h>
#include <linux/of.h>
#include <linux/pci-ecam.h>
#include <linux/platform_device.h>
#include <linux/module.h>

/* Platform specific values as hardcoded in the firmware. */
#define AP_NS_SHARED_MEM_BASE	0x06000000
#define MAX_SEGMENTS		2		/* Two PCIe root complexes. */
#define BDF_TABLE_SIZE		SZ_16K

/*
 * Shared memory layout as written by the SCP upon boot time:
 *  ----
 *  Discover data header --> RC base address
 *                       \-> BDF Count
 *  Discover data        --> BDF 0...n
 *  ----
 */
struct pcie_discovery_data {
	u32 rc_base_addr;
	u32 nr_bdfs;
	u32 valid_bdfs[0];
} *pcie_discovery_data[MAX_SEGMENTS];

void __iomem *rc_remapped_addr[MAX_SEGMENTS];

/*
 * map_bus() is called before we do a config space access for a certain
 * device. We use this to check whether this device is valid, avoiding
 * config space accesses which would result in an SError otherwise.
 */
static void __iomem *pci_n1sdp_map_bus(struct pci_bus *bus, unsigned int devfn,
				       int where)
{
	struct pci_config_window *cfg = bus->sysdata;
	unsigned int devfn_shift = cfg->ops->bus_shift - 8;
	unsigned int busn = bus->number;
	unsigned int segment = bus->domain_nr;
	unsigned int bdf_addr;
	unsigned int table_count, i;
	struct pci_dev *dev;

	if (segment >= MAX_SEGMENTS ||
	    busn < cfg->busr.start || busn > cfg->busr.end)
		return NULL;

	/* The PCIe root complex has a separate config space mapping. */
	if (busn == 0 && devfn == 0)
		return rc_remapped_addr[segment] + where;

	dev = pci_get_domain_bus_and_slot(segment, busn, devfn);
	if (dev && dev->is_virtfn)
		return pci_ecam_map_bus(bus, devfn, where);

	/* Accesses beyond the vendor ID always go to existing devices. */
	if (where > 0)
		return pci_ecam_map_bus(bus, devfn, where);

	busn -= cfg->busr.start;
	bdf_addr = (busn << cfg->ops->bus_shift) + (devfn << devfn_shift);
	table_count = pcie_discovery_data[segment]->nr_bdfs;
	for (i = 0; i < table_count; i++) {
		if (bdf_addr == pcie_discovery_data[segment]->valid_bdfs[i])
			return pci_ecam_map_bus(bus, devfn, where);
	}

	return NULL;
}

static int pci_n1sdp_init(struct pci_config_window *cfg, unsigned int segment)
{
	phys_addr_t table_base;
	struct device *dev = cfg->parent;
	struct pcie_discovery_data *shared_data;
	size_t bdfs_size;

	if (segment >= MAX_SEGMENTS)
		return -ENODEV;

	table_base = AP_NS_SHARED_MEM_BASE + segment * BDF_TABLE_SIZE;

	if (!request_mem_region(table_base, BDF_TABLE_SIZE,
				"PCIe valid BDFs")) {
		dev_err(dev, "PCIe BDF shared region request failed\n");
		return -ENOMEM;
	}

	shared_data = devm_ioremap(dev,
				   table_base, BDF_TABLE_SIZE);
	if (!shared_data)
		return -ENOMEM;

	/* Copy the valid BDFs structure to allocated normal memory. */
	bdfs_size = sizeof(struct pcie_discovery_data) +
		    sizeof(u32) * shared_data->nr_bdfs;
	pcie_discovery_data[segment] = devm_kmalloc(dev, bdfs_size, GFP_KERNEL);
	if (!pcie_discovery_data[segment])
		return -ENOMEM;

	memcpy_fromio(pcie_discovery_data[segment], shared_data, bdfs_size);

	rc_remapped_addr[segment] = devm_ioremap_nocache(dev,
						shared_data->rc_base_addr,
						PCI_CFG_SPACE_EXP_SIZE);
	if (!rc_remapped_addr[segment]) {
		dev_err(dev, "Cannot remap root port base\n");
		return -ENOMEM;
	}

	devm_iounmap(dev, shared_data);

	return 0;
}

static int pci_n1sdp_pcie_init(struct pci_config_window *cfg)
{
	return pci_n1sdp_init(cfg, 0);
}

static int pci_n1sdp_ccix_init(struct pci_config_window *cfg)
{
	return pci_n1sdp_init(cfg, 1);
}

struct pci_ecam_ops pci_n1sdp_pcie_ecam_ops = {
	.bus_shift	= 20,
	.init		= pci_n1sdp_pcie_init,
	.pci_ops	= {
		.map_bus        = pci_n1sdp_map_bus,
		.read           = pci_generic_config_read32,
		.write          = pci_generic_config_write32,
	}
};

struct pci_ecam_ops pci_n1sdp_ccix_ecam_ops = {
	.bus_shift	= 20,
	.init		= pci_n1sdp_ccix_init,
	.pci_ops	= {
		.map_bus        = pci_n1sdp_map_bus,
		.read           = pci_generic_config_read32,
		.write          = pci_generic_config_write32,
	}
};

static const struct of_device_id n1sdp_pcie_of_match[] = {
	{ .compatible = "arm,n1sdp-pcie" },
	{ },
};
MODULE_DEVICE_TABLE(of, n1sdp_pcie_of_match);

static int n1sdp_pcie_probe(struct platform_device *pdev)
{
	const struct device_node *of_node = pdev->dev.of_node;
	u32 segment;

	if (of_property_read_u32(of_node, "linux,pci-domain", &segment)) {
		dev_err(&pdev->dev, "N1SDP PCI controllers require linux,pci-domain property\n");
		return -EINVAL;
	}

	switch (segment) {
	case 0:
		return pci_host_common_probe(pdev, &pci_n1sdp_pcie_ecam_ops);
	case 1:
		return pci_host_common_probe(pdev, &pci_n1sdp_ccix_ecam_ops);
	}

	dev_err(&pdev->dev, "Invalid segment number, must be smaller than %d\n",
		MAX_SEGMENTS);

	return -EINVAL;
}

static struct platform_driver n1sdp_pcie_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = n1sdp_pcie_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = n1sdp_pcie_probe,
};
builtin_platform_driver(n1sdp_pcie_driver);
