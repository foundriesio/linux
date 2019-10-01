// SPDX-License-Identifier: GPL-2.0
/*
 * RAM block device on reserved regions
 *
 * Copyright (C) 2019 Telechips Inc.
 */

/*#define DEBUG*/

#define pr_fmt(fmt)	"ramblk: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/slab.h>
#include <linux/idr.h>

#define RAMBLK_DRV_NAME	"ramblk"

#define SECTOR_SHIFT	9
#define SECTOR_SIZE	(1 >> SECTOR_SHIFT)

struct ramblk {
	struct device *dev;
	void __iomem *base;

	spinlock_t lock;
	struct request_queue *queue;

	struct gendisk *gd;
	int dev_id;
	phys_addr_t phys;
	size_t size;
};

static int ramblk_major;
static DEFINE_IDA(ramblk_id_ida);

static void ramblk_request(struct request_queue *q)
{
	struct ramblk *ramblk = q->queuedata;
	struct request *req;

	dev_dbg(ramblk->dev, "%s: q=%p\n", __func__, q);

	req = blk_fetch_request(q);
	while (req) {
		void *buffer;

		buffer  = bio_data(req->bio);

		dev_dbg(ramblk->dev, "%s: %s: req=%p, pos=%lld, sz=%d, data=%p\n",
				__func__, rq_data_dir(req) == WRITE ? "wr" : "rd", req,
				blk_rq_pos(req), blk_rq_cur_bytes(req), buffer);

		switch (rq_data_dir(req)) {
		case WRITE:
			memcpy(ramblk->base + blk_rq_pos(req) * 512, buffer,
					blk_rq_cur_bytes(req));
			break;
		case READ:
			memcpy(buffer, ramblk->base + blk_rq_pos(req) * 512,
					blk_rq_cur_bytes(req));
			break;
		}
		if (!__blk_end_request_cur(req, BLK_STS_OK))
			req = blk_fetch_request(q);
	}
}

static int ramblk_open(struct block_device *bdev, fmode_t mode)
{
	struct ramblk *ramblk = bdev->bd_disk->private_data;

	(void) get_device(ramblk->dev);
	return 0;
}

static void ramblk_release(struct gendisk *disk, fmode_t mode)
{
	struct ramblk *ramblk = disk->private_data;

	put_device(ramblk->dev);
}

static int ramblk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->heads = 1 << 6;
	geo->sectors = 1 << 5;
	geo->cylinders = get_capacity(bdev->bd_disk) >> 11;
	return 0;
}

static int ramblk_ioctl(struct block_device *bdev, fmode_t mode,
		unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static int ramblk_compat_ioctl(strcut block_device *bdev, fmode_t mode,
		unsigned int cmd, unsigned long arg)
{
	return ramblk_ioctl(bdev, mode, cmd, arg);
}
#endif

static const struct block_device_operations ramblk_fops = {
	.owner = THIS_MODULE,
	.open = ramblk_open,
	.release = ramblk_release,
	.getgeo = ramblk_getgeo,
	.ioctl = ramblk_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ramblk_compat_ioctl,
#endif
};

static int ramblk_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct ramblk *ramblk;
	struct gendisk *gd;
	struct resource res;
	int ret;

	ramblk = kzalloc(sizeof(struct ramblk), GFP_KERNEL);
	if (!ramblk)
		return -ENOMEM;

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(&pdev->dev, "Unable to get memory region\n");
		goto out;
	}
	of_node_put(node);

	ramblk->phys = res.start;
	ramblk->size = resource_size(&res);
	ramblk->base = devm_ioremap_wc(&pdev->dev, ramblk->phys, ramblk->size);
	if (!ramblk->base) {
		dev_err(&pdev->dev, "Unabled to map memory region %pa - %pa\n",
				&res.start, &res.end);
		ret = -ENOMEM;
		goto out;
	}
	dev_dbg(&pdev->dev, "memory mapped region %pa - %pa\n",
			&res.start, &res.end);

	spin_lock_init(&ramblk->lock);

	ramblk->queue = blk_init_queue(ramblk_request, &ramblk->lock);
	if (!ramblk->queue) {
		ret = -ENOMEM;
		goto out;
	}
	ramblk->queue->queuedata = ramblk;
	blk_queue_logical_block_size(ramblk->queue, SECTOR_SIZE);
	blk_queue_bounce_limit(ramblk->queue, BLK_BOUNCE_HIGH);

	gd = alloc_disk(1);
	if (!gd) {
		dev_err(&pdev->dev, "Unable to allocate disk\n");
		ret = -ENOMEM;
		goto out;
	}

	ramblk_major = register_blkdev(UNNAMED_MAJOR, RAMBLK_DRV_NAME);
	if (ramblk_major <= 0) {
		dev_err(&pdev->dev, "Unabled to get major number\n");
		ret = -EBUSY;
		goto out;
	}
	ramblk->dev_id = ida_simple_get(&ramblk_id_ida, 0, 1 << MINORBITS,
			GFP_KERNEL);
	if (ramblk->dev_id < 0) {
		ret = -ENOMEM;
		goto out;
	}

	snprintf(gd->disk_name, sizeof(gd->disk_name), "rd%d",
			ramblk->dev_id);
	gd->major = ramblk_major;
	gd->fops = &ramblk_fops;
	gd->queue = ramblk->queue;
	gd->private_data = ramblk;

	set_capacity(gd, ramblk->size >> SECTOR_SHIFT);

	ramblk->dev = &pdev->dev;
	ramblk->gd = gd;

	add_disk(ramblk->gd);

	dev_info(&pdev->dev, "registered %d\n", ramblk_major);

	return 0;
out:
	kfree(ramblk);
	unregister_blkdev(ramblk_major, RAMBLK_DRV_NAME);
	return ret;
}

static int ramblk_remove(struct platform_device *pdev)
{
	unregister_blkdev(ramblk_major, RAMBLK_DRV_NAME);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ramblk_of_match[] = {
	{ .compatible = "telechips,ramblk", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ramblk_of_match);
#else
#define ramblk_of_match	NULL
#endif

static struct platform_driver ramblk_driver = {
	.probe	= ramblk_probe,
	.remove	= ramblk_remove,
	.driver	= {
		.name	= RAMBLK_DRV_NAME,
		.of_match_table = ramblk_of_match,
	},
};

static int __init ramblk_init(void)
{
	return platform_driver_register(&ramblk_driver);
}
module_init(ramblk_init);

static void __exit ramblk_exit(void)
{
	platform_driver_unregister(&ramblk_driver);
}
module_exit(ramblk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("RAM Block Device");
