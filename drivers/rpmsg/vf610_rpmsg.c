/*
 * Copyright (C) 2016 Toradex AG
 *
 * Derived from the downstream iMX7 rpmsg implementation by Freescale.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/vf610_mscm.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/vf610_sema4.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>

struct vf610_rpmsg_vproc {
	struct virtio_device vdev;
	unsigned int vring[2];
	char *rproc_name;
	struct mutex lock;
	struct delayed_work rpmsg_work;
	struct virtqueue *vq[2];
	int base_vq_id;
	int num_of_vqs;
};

#define MSCM_CPU_M4		1

/*
 * For now, allocate 256 buffers of 512 bytes for each side. each buffer
 * will then have 16B for the msg header and 496B for the payload.
 * This will require a total space of 256KB for the buffers themselves, and
 * 3 pages for every vring (the size of the vring depends on the number of
 * buffers it supports).
 */
#define RPMSG_NUM_BUFS		(32)
#define RPMSG_BUF_SIZE		(512)
#define RPMSG_BUFS_SPACE	(RPMSG_NUM_BUFS * RPMSG_BUF_SIZE)

/*
 * The alignment between the consumer and producer parts of the vring.
 * Note: this is part of the "wire" protocol. If you change this, you need
 * to update your BIOS image as well
 */
#define RPMSG_VRING_ALIGN	(4096)

/* With 256 buffers, our vring will occupy 3 pages */
#define RPMSG_RING_SIZE	((DIV_ROUND_UP(vring_size(RPMSG_NUM_BUFS / 2, \
				RPMSG_VRING_ALIGN), PAGE_SIZE)) * PAGE_SIZE)

#define to_vf610_rpdev(vd) container_of(vd, struct vf610_rpmsg_vproc, vdev)

struct vf610_rpmsg_vq_info {
	__u16 num;	/* number of entries in the virtio_ring */
	__u16 vq_id;	/* a globaly unique index of this virtqueue */
	void *addr;	/* address where we mapped the virtio ring */
	struct vf610_rpmsg_vproc *rpdev;
};

static struct vf610_sema4_mutex *rpmsg_mutex;

static u64 vf610_rpmsg_get_features(struct virtio_device *vdev)
{
	return 1 << VIRTIO_RPMSG_F_NS;
}

static int vf610_rpmsg_finalize_features(struct virtio_device *vdev)
{
	/* Give virtio_ring a chance to accept features */
	vring_transport_features(vdev);

	return 0;
}

/* kick the remote processor */
static bool vf610_rpmsg_notify(struct virtqueue *vq)
{
	struct vf610_rpmsg_vq_info *rpvq = vq->priv;

	mutex_lock(&rpvq->rpdev->lock);

	mscm_trigger_cpu2cpu_irq(rpvq->vq_id, MSCM_CPU_M4);

	mutex_unlock(&rpvq->rpdev->lock);

	return true;
}

static void rpmsg_work_handler(struct work_struct *work)
{
	struct vf610_rpmsg_vproc *rpdev = container_of(work,
				struct vf610_rpmsg_vproc, rpmsg_work.work);

	/* Process incoming buffers on all our vrings */
	vring_interrupt(0, rpdev->vq[0]);
	vring_interrupt(1, rpdev->vq[1]);
}

static irqreturn_t cpu_to_cpu_irq_handler(int irq, void *p)
{
	struct vf610_rpmsg_vproc *rpdev = (struct vf610_rpmsg_vproc *)p;

	schedule_delayed_work(&rpdev->rpmsg_work, 0);

	return IRQ_HANDLED;
}

static struct virtqueue *rp_find_vq(struct virtio_device *vdev,
				    unsigned index,
				    void (*callback)(struct virtqueue *vq),
				    const char *name)
{
	struct vf610_rpmsg_vproc *rpdev = to_vf610_rpdev(vdev);
	struct vf610_rpmsg_vq_info *rpvq;
	struct virtqueue *vq;
	int err;

	rpvq = kmalloc(sizeof(*rpvq), GFP_KERNEL);
	if (!rpvq)
		return ERR_PTR(-ENOMEM);

	/* ioremap'ing normal memory, so we cast away sparse's complaints */
	rpvq->addr = (__force void *) ioremap_nocache(rpdev->vring[index],
							RPMSG_RING_SIZE);
	if (!rpvq->addr) {
		err = -ENOMEM;
		goto free_rpvq;
	}

	memset(rpvq->addr, 0, RPMSG_RING_SIZE);

	pr_debug("vring%d: phys 0x%x, virt 0x%x\n", index, rpdev->vring[index],
					(unsigned int) rpvq->addr);

	vq = vring_new_virtqueue(index, RPMSG_NUM_BUFS, RPMSG_VRING_ALIGN,
			vdev, true, rpvq->addr, vf610_rpmsg_notify, callback,
			name);
	if (!vq) {
		pr_err("vring_new_virtqueue failed\n");
		err = -ENOMEM;
		goto unmap_vring;
	}

	rpdev->vq[index] = vq;
	vq->priv = rpvq;
	/* system-wide unique id for this virtqueue */
	rpvq->vq_id = rpdev->base_vq_id + index;
	rpvq->rpdev = rpdev;
	mutex_init(&rpdev->lock);

	return vq;

unmap_vring:
	/* iounmap normal memory, so make sparse happy */
	iounmap((__force void __iomem *) rpvq->addr);
free_rpvq:
	kfree(rpvq);
	return ERR_PTR(err);
}

static void vf610_rpmsg_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;

	list_for_each_entry_safe(vq, n, &vdev->vqs, list) {
		struct vf610_rpmsg_vq_info *rpvq = vq->priv;
		iounmap(rpvq->addr);
		vring_del_virtqueue(vq);
		kfree(rpvq);
	}
}

static int vf610_rpmsg_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		       struct virtqueue *vqs[],
		       vq_callback_t *callbacks[],
		       const char *names[])
{
	struct vf610_rpmsg_vproc *rpdev = to_vf610_rpdev(vdev);
	int i, err;

	/* we maintain two virtqueues per remote processor (for RX and TX) */
	if (nvqs != 2)
		return -EINVAL;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = rp_find_vq(vdev, i, callbacks[i], names[i]);
		if (IS_ERR(vqs[i])) {
			err = PTR_ERR(vqs[i]);
			goto error;
		}
	}

	rpdev->num_of_vqs = nvqs;

	return 0;

error:
	vf610_rpmsg_del_vqs(vdev);
	return err;
}

static void vf610_rpmsg_reset(struct virtio_device *vdev)
{
	dev_dbg(&vdev->dev, "reset !\n");
}

static u8 vf610_rpmsg_get_status(struct virtio_device *vdev)
{
	return 0;
}

static void vf610_rpmsg_set_status(struct virtio_device *vdev, u8 status)
{
	dev_dbg(&vdev->dev, "%s new status: %d\n", __func__, status);
}

static void vf610_rpmsg_vproc_release(struct device *dev)
{
	/* this handler is provided so driver core doesn't yell at us */
}

static struct virtio_config_ops vf610_rpmsg_config_ops = {
	.get_features	= vf610_rpmsg_get_features,
	.finalize_features = vf610_rpmsg_finalize_features,
	.find_vqs	= vf610_rpmsg_find_vqs,
	.del_vqs	= vf610_rpmsg_del_vqs,
	.reset		= vf610_rpmsg_reset,
	.set_status	= vf610_rpmsg_set_status,
	.get_status	= vf610_rpmsg_get_status,
};

static struct vf610_rpmsg_vproc vf610_rpmsg_vprocs = {
	.vdev.id.device	= VIRTIO_ID_RPMSG,
	.vdev.config	= &vf610_rpmsg_config_ops,
	.rproc_name	= "vf610_m4",
	.base_vq_id	= 0,
};

static const struct of_device_id vf610_rpmsg_dt_ids[] = {
	{ .compatible = "fsl,vf610-rpmsg", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vf610_rpmsg_dt_ids);

static int vf610_rpmsg_probe(struct platform_device *pdev)
{
	int i;
	int ret = 0;
	struct vf610_rpmsg_vproc *rpdev = &vf610_rpmsg_vprocs;

	INIT_DELAYED_WORK(&rpdev->rpmsg_work, rpmsg_work_handler);

	rpdev->vring[0] = 0x3f070000;
	rpdev->vring[1] = 0x3f074000;

	rpdev->vdev.dev.parent = &pdev->dev;
	rpdev->vdev.dev.release = vf610_rpmsg_vproc_release;

	ret = register_virtio_device(&rpdev->vdev);
	if (ret) {
		pr_err("%s failed to register rpdev: %d\n",
				__func__, ret);
		return ret;
	}

	for (i = 0; i < 2; i++) {
		ret = mscm_request_cpu2cpu_irq(i, cpu_to_cpu_irq_handler,
				(const char *)rpdev->rproc_name, rpdev);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to register CPU2CPU interrupt\n");
			unregister_virtio_device(&rpdev->vdev);
			return ret;
		}
	}

	return 0;
}

static int vf610_rpmsg_remove(struct platform_device *pdev)
{
	struct vf610_rpmsg_vproc *rpdev = &vf610_rpmsg_vprocs;
	int i;

	for (i = 0; i < 2; i++)
		mscm_free_cpu2cpu_irq(i, NULL);

	unregister_virtio_device(&rpdev->vdev);

	return 0;
}

static struct platform_driver vf610_rpmsg_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "vf610-rpmsg",
		   .of_match_table = vf610_rpmsg_dt_ids,
		   },
	.probe = vf610_rpmsg_probe,
	.remove = vf610_rpmsg_remove,
};

static int __init vf610_rpmsg_init(void)
{
	int ret;

	rpmsg_mutex = vf610_sema4_mutex_create(0, 0);
	if (IS_ERR(rpmsg_mutex)) {
		pr_err("vf610 rpmsg unable to create mutex\n");
		return PTR_ERR(rpmsg_mutex);
	}

	vf610_sema4_mutex_lock(rpmsg_mutex);

	ret = platform_driver_register(&vf610_rpmsg_driver);
	if (ret)
		pr_err("Unable to initialize rpmsg driver\n");
	else
		pr_info("vf610 rpmsg driver is registered.\n");

	vf610_sema4_mutex_unlock(rpmsg_mutex);

	return ret;
}

static void __exit vf610_rpmsg_exit(void)
{
	if (rpmsg_mutex)
		vf610_sema4_mutex_destroy(rpmsg_mutex);

	pr_info("vf610 rpmsg driver is unregistered.\n");
	platform_driver_unregister(&vf610_rpmsg_driver);
}
module_exit(vf610_rpmsg_exit);
module_init(vf610_rpmsg_init);

MODULE_AUTHOR("Sanchayan Maity <sanchayan.maity@toradex.com>");
MODULE_DESCRIPTION("vf610 remote processor messaging virtio device");
MODULE_LICENSE("GPL v2");
