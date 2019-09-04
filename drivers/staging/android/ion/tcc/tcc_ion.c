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
#include "../../uapi/ion.h"
#include "../ion.h"
#include <soc/tcc/pmap.h>

struct ion_platform_data {
	int nr;
	struct ion_platform_heap *heaps;
};

struct ion_mapper *tcc_user_mapper;
int num_heaps;
struct ion_heap **heaps;
static pmap_t pmap_ump_reserved;
#ifdef CONFIG_ION_CARVEOUT_CAM_HEAP
static pmap_t pmap_ion_carveout_cam;
#endif


extern struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data);
#ifdef CONFIG_ION_CARVEOUT_CAM_HEAP
extern struct ion_heap *ion_carveout_cam_heap_create(struct ion_platform_heap *heap_data);
#endif
extern struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap *heap_data);

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

		if(heap->type == ION_HEAP_TYPE_CARVEOUT)
		{
			if(0 > pmap_get_info("ump_reserved", &pmap_ump_reserved)){
				printk("%s-%d : ump_reserved allocation is failed.\n", __func__, __LINE__);
				return ERR_PTR(-ENOMEM);
			}
			printk("@@@@@@@@@@@@@@@@@@@@ %s - 0x%x - 0x%x - %d - %d - %d\n",
						pmap_ump_reserved.name, pmap_ump_reserved.base, pmap_ump_reserved.size,
						pmap_ump_reserved.groups, pmap_ump_reserved.rc, pmap_ump_reserved.flags);

			heap->base = pmap_ump_reserved.base;
			heap->size = pmap_ump_reserved.size;
			printk("ump_reserved base:0x%x 0x%x\n", heap->base, pmap_ump_reserved.base);
	 	}
#ifdef CONFIG_ION_CARVEOUT_CAM_HEAP
		else if(heap->type == ION_HEAP_TYPE_CARVEOUT_CAM)
		{
			if(0 > pmap_get_info("ion_carveout_cam", &pmap_ion_carveout_cam)){
				printk("%s-%d : ion_carveout_cam allocation is failed.\n", __func__, __LINE__);
				return ERR_PTR(-ENOMEM);
			}
			heap->base = pmap_ion_carveout_cam.base;
			heap->size = pmap_ion_carveout_cam.size;
			printk("ion_carveout_cam base:0x%x\n", heap->base);
	 	}
#endif		
		++index;
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
		pr_err("%s: Invalid heap type %d\n", __func__,
		       heap_data->type);
		return ERR_PTR(-EINVAL);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %lu size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	return heap;
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

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(heaps[i]);
	}
	return 0;
err:
	kfree(heaps);
out:
	return err;
}

int tcc_ion_remove(struct platform_device *pdev)
{
	if(pmap_ump_reserved.base) {
		pmap_release_info("ump_reserved");
		pmap_ump_reserved.base = 0;
	}
#ifdef CONFIG_ION_CARVEOUT_CAM_HEAP
	if(pmap_ion_carveout_cam.base) {
        pmap_release_info("ion_carveout_cam");
		pmap_ion_carveout_cam.base = 0;
	}
#endif
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
	printk("ion_init\n");
	return platform_driver_register(&tcc_ion_driver);
}

subsys_initcall(ion_init);
