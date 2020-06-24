/****************************************************************************
linux/driver/gpu/ion/tcc/tcc_ion.c
Description: TCC ION device driver

Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/module.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "../../uapi/ion_4.9.h"
#include "../ion_priv.h"
#include <soc/tcc/pmap.h>

struct ion_device *idev;
struct ion_mapper *tcc_user_mapper;
int num_heaps;
struct ion_heap **heaps;

static struct ion_platform_data *tcc_ion_parse_dt(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *child;
	struct ion_platform_data *pdata;
	int index = 0;
	u32 val;
	int ret;

	num_heaps = 0;
	for_each_child_of_node(node, child)
		++num_heaps;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct ion_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->heaps = devm_kzalloc(&pdev->dev,
			num_heaps * sizeof(struct ion_platform_heap),
			GFP_KERNEL);
	if (!pdata->heaps)
		return ERR_PTR(-ENOMEM);
 
	for_each_child_of_node(node, child) {
		struct ion_platform_heap *heap = &pdata->heaps[index];

		ret = of_property_read_u32(child, "reg", &val);
		if (ret) {
			return ERR_PTR(ret);
		}
		heap->id = index;
		heap->type = val;

		ret = of_property_read_string(child,
				"telechips,ion-heap-name", &heap->name);

		printk("%s heap id:%d, type : 0x%x\n", __func__, heap->id, heap->type);
		if(heap->type == ION_HEAP_TYPE_CARVEOUT)
		{
			pmap_t pmap_ump_reserved;
			pmap_get_info("ump_reserved", &pmap_ump_reserved);

			heap->base = pmap_ump_reserved.base;
			heap->size = pmap_ump_reserved.size;
			printk("base:0x%x\n", heap->base);
	 	}

		++index;
	}



	pdata->nr = num_heaps;

	return pdata;
}

int tcc_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata;
	int err = 0;
	int i;

	if (pdev->dev.of_node) {
		pdata = tcc_ion_parse_dt(pdev);
		if (IS_ERR(pdata)) {
			err = PTR_ERR(pdata);
			goto out;
		}
	} else {
		pdata = pdev->dev.platform_data;
	}

	num_heaps = pdata->nr;

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	idev = ion_device_create(NULL);
	if (IS_ERR_OR_NULL(idev)) {
		kfree(heaps);
		return PTR_ERR(idev);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(idev, heaps[i]);
	}
	platform_set_drvdata(pdev, idev);
	return 0;
err:
	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			ion_heap_destroy(heaps[i]);
	}
	kfree(heaps);
out:
	return err;
}

int tcc_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id tcc_ion_of_match[] = {
	{ .compatible = "telechips,tcc-ion" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_ion_of_match);
#endif

static struct platform_driver tcc_ion_driver = {
	.probe = tcc_ion_probe,
	.remove = tcc_ion_remove,
	.driver = {
		.name = "ion-tcc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_ion_of_match),
	},
};

static int __init ion_init(void)
{
	printk("ion_4.9_init\n");
	return platform_driver_register(&tcc_ion_driver);
}

subsys_initcall(ion_init);
