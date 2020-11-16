// SPDX-License-Identifier: GPL-2.0
/*
 * TCC ION device driver
 *
 * Copyright (c) 2013 - 2020 Telechips, Inc.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "../../uapi/ion.h"
#include "../ion.h"
#include <soc/tcc/pmap.h>

struct ion_platform_data {
	int nr;
	struct ion_platform_heap *heaps;
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap
						 *heap_data);
#ifdef CONFIG_ION_CARVEOUT_CAM_HEAP
struct ion_heap *ion_carveout_cam_heap_create(struct ion_platform_heap
						     *heap_data);
#endif
struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap
					      *heap_data);

static struct ion_platform_data *tcc_ion_parse_dt(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *child;
	struct device_node *mem_np;
	struct ion_platform_data *pdata;
	struct resource res;
	int num_heaps;
	int index;
	int id, type;
	int ret;

	num_heaps = 0;
	for_each_child_of_node(node, child)
		++num_heaps;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct ion_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->heaps = devm_kzalloc(&pdev->dev,
				    num_heaps *
				    sizeof(struct ion_platform_heap),
				    GFP_KERNEL);
	if (!pdata->heaps)
		return ERR_PTR(-ENOMEM);

	index = 0;
	for_each_child_of_node(node, child) {
		struct ion_platform_heap *heap = &pdata->heaps[index++];

		ret = of_property_read_u32_index(child, "reg", 0, &id);
		if (ret)
			return ERR_PTR(ret);

		ret = of_property_read_u32_index(child, "reg", 1, &type);
		if (ret)
			return ERR_PTR(ret);

		heap->id = id;
		heap->type = type;

		ret = of_property_read_string(child,
					      "telechips,ion-heap-name",
					      &heap->name);

		mem_np = of_parse_phandle(child, "memory-region",  0);
		ret = of_address_to_resource(mem_np, 0, &res);
		of_node_put(mem_np);
		if (ret) {
			dev_info(&pdev->dev, "registered %s: id %d type 0x%x\n",
				 heap->name, id, type);
		} else {
			heap->base = res.start;
			heap->size = resource_size(&res);
			dev_info(&pdev->dev, "registered %s: id %d type 0x%x %pr\n",
				 heap->name, id, type, &res);
		}
	}

	pdata->nr = num_heaps;

	return pdata;
}

struct ion_heap *ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch (heap_data->type) {
#ifdef CONFIG_ION_CARVEOUT_HEAP
	case ION_HEAP_TYPE_CARVEOUT:
		heap = ion_carveout_heap_create(heap_data);
		break;
#endif
#ifdef CONFIG_ION_CARVEOUT_CAM_HEAP
	case ION_HEAP_TYPE_CARVEOUT_CAM:
		heap = ion_carveout_cam_heap_create(heap_data);
		break;
#endif
#ifdef CONFIG_ION_CHUNK_HEAP
	case ION_HEAP_TYPE_CHUNK:
		heap = ion_chunk_heap_create(heap_data);
		break;
#endif
	default:
		pr_err("%s: Invalid heap type %d\n", __func__, heap_data->type);
		return ERR_PTR(-EINVAL);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %pa size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       &heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	return heap;
}

int tcc_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata;
	struct ion_heap **heaps;
	int num_heaps;
	int i;

	if (pdev->dev.of_node) {
		pdata = tcc_ion_parse_dt(pdev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}

	num_heaps = pdata->nr;

	heaps = devm_kcalloc(&pdev->dev, pdata->nr, sizeof(struct ion_heap *),
			     GFP_KERNEL);
	if (!heaps)
		return PTR_ERR(heaps);

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i]))
			return PTR_ERR(heaps[i]);

		ion_device_add_heap(heaps[i]);
	}

	dev_info(&pdev->dev, "initialized");

	return 0;
}

int tcc_ion_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_ion_of_match[] = {
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

static int __init tcc_ion_init(void)
{
	return platform_driver_register(&tcc_ion_driver);
}

subsys_initcall(tcc_ion_init);
