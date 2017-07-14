/*
 *
 * (C) COPYRIGHT 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/scmi_protocol.h>
#include <linux/version.h>

static int scmi_gpu_opps_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct platform_device *gpu_pdev;
	int err;

	np = of_find_node_by_name(NULL, "gpu");
	if (!np) {
		dev_err(&pdev->dev, "Failed to find DT entry for Mali\n");
		return -EFAULT;
	}

	gpu_pdev = of_find_device_by_node(np);
	if (!gpu_pdev) {
		dev_err(&pdev->dev, "Failed to find device for Mali\n");
		of_node_put(np);
		return -EFAULT;
	}

	of_node_put(np);

	return err;
}


static struct platform_driver scmi_gpu_opp_driver = {
	.driver = {
		.name = "scmi_gpu_opp",
	},
	.probe = scmi_gpu_opps_probe,
};
module_platform_driver(scmi_gpu_opp_driver);

MODULE_AUTHOR("Amit Kachhap <amit.kachhap@arm.com>");
MODULE_DESCRIPTION("ARM SCMI GPU opp driver");
MODULE_LICENSE("GPL v2");
