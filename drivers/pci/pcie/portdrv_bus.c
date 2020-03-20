#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pm.h>

#include "portdrv.h"

extern int pcie_port_bus_match(struct device *dev, struct device_driver *drv);

struct bus_type pcie_port_bus_type = {
	.name		= "pci_express",
	.match		= pcie_port_bus_match,
};
EXPORT_SYMBOL_GPL(pcie_port_bus_type);
