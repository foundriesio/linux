/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 * Copyright (C) 2016 Toradex AG.
 *
 * Taken from the 4.1.15 kernel release for i.MX7 by Freescale.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vf610_sema4.h>

static struct vf610_sema4_mutex_device *vf610_sema4;

struct vf610_sema4_mutex *
vf610_sema4_mutex_create(u32 dev_num, u32 mutex_num)
{
	struct vf610_sema4_mutex *mutex_ptr = NULL;

	if (!vf610_sema4)
		return ERR_PTR(-ENODEV);

	if (mutex_num >= SEMA4_NUM_GATES || dev_num >= SEMA4_NUM_DEVICES)
		return ERR_PTR(-EINVAL);

	if (vf610_sema4->cpine_val & (1 < mutex_num)) {
		pr_err("Error: requiring a allocated sema4.\n");
		pr_err("mutex_num %d cpine_val 0x%08x.\n",
				mutex_num, vf610_sema4->cpine_val);
	}

	mutex_ptr = kzalloc(sizeof(*mutex_ptr), GFP_KERNEL);
	if (!mutex_ptr)
		return ERR_PTR(-ENOMEM);

	vf610_sema4->mutex_ptr[mutex_num] = mutex_ptr;
	vf610_sema4->alloced |= 1 < mutex_num;
	vf610_sema4->cpine_val |= idx_sema4[mutex_num];
	writew(vf610_sema4->cpine_val, vf610_sema4->ioaddr + SEMA4_CP0INE);

	mutex_ptr->valid = CORE_MUTEX_VALID;
	mutex_ptr->gate_num = mutex_num;
	init_waitqueue_head(&mutex_ptr->wait_q);

	return mutex_ptr;
}
EXPORT_SYMBOL(vf610_sema4_mutex_create);

int vf610_sema4_mutex_destroy(struct vf610_sema4_mutex *mutex_ptr)
{
	u32 mutex_num;

	if ((mutex_ptr == NULL) || (mutex_ptr->valid != CORE_MUTEX_VALID))
		return -EINVAL;

	mutex_num = mutex_ptr->gate_num;
	if ((vf610_sema4->cpine_val & idx_sema4[mutex_num]) == 0) {
		pr_err("Error: trying to destroy a un-allocated sema4.\n");
		pr_err("mutex_num %d cpine_val 0x%08x.\n",
				mutex_num, vf610_sema4->cpine_val);
	}
	vf610_sema4->mutex_ptr[mutex_num] = NULL;
	vf610_sema4->alloced &= ~(1 << mutex_num);
	vf610_sema4->cpine_val &= ~(idx_sema4[mutex_num]);
	writew(vf610_sema4->cpine_val, vf610_sema4->ioaddr + SEMA4_CP0INE);

	kfree(mutex_ptr);

	return 0;
}
EXPORT_SYMBOL(vf610_sema4_mutex_destroy);

int _vf610_sema4_mutex_lock(struct vf610_sema4_mutex *mutex_ptr)
{
	int ret = 0, i = 0;

	if ((mutex_ptr == NULL) || (mutex_ptr->valid != CORE_MUTEX_VALID))
		return -EINVAL;

	i = mutex_ptr->gate_num;
	mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
	mutex_ptr->gate_val &= SEMA4_GATE_MASK;
	/* Check to see if this core already own it */
	if (mutex_ptr->gate_val == SEMA4_A5_LOCK) {
		/* return -EBUSY, invoker should be in sleep, and re-lock ag */
		pr_err("%s -> %s %d already locked, wait! num %d val %d.\n",
				__FILE__, __func__, __LINE__,
				i, mutex_ptr->gate_val);
		ret = -EBUSY;
		goto out;
	} else {
		/* try to lock the mutex */
		mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
		mutex_ptr->gate_val &= (~SEMA4_GATE_MASK);
		mutex_ptr->gate_val |= SEMA4_A5_LOCK;
		writeb(mutex_ptr->gate_val, vf610_sema4->ioaddr + i);
		mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
		mutex_ptr->gate_val &= SEMA4_GATE_MASK;
		/* double check the mutex is locked, otherwise, return -EBUSY */
		if (mutex_ptr->gate_val != SEMA4_A5_LOCK) {
			pr_debug("wait-locked num %d val %d.\n",
					i, mutex_ptr->gate_val);
			ret = -EBUSY;
		}
	}
out:
	return ret;
}

int vf610_sema4_mutex_trylock(struct vf610_sema4_mutex *mutex_ptr)
{
	int ret = 0;

	ret = _vf610_sema4_mutex_lock(mutex_ptr);
	if (ret == 0)
		return SEMA4_A5_LOCK;
	else
		return ret;
}
EXPORT_SYMBOL(vf610_sema4_mutex_trylock);

int vf610_sema4_mutex_lock(struct vf610_sema4_mutex *mutex_ptr)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&vf610_sema4->lock, flags);
	ret = _vf610_sema4_mutex_lock(mutex_ptr);
	spin_unlock_irqrestore(&vf610_sema4->lock, flags);
	while (-EBUSY == ret) {
		spin_lock_irqsave(&vf610_sema4->lock, flags);
		ret = _vf610_sema4_mutex_lock(mutex_ptr);
		spin_unlock_irqrestore(&vf610_sema4->lock, flags);
		if (ret == 0)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(vf610_sema4_mutex_lock);

int vf610_sema4_mutex_unlock(struct vf610_sema4_mutex *mutex_ptr)
{
	int ret = 0, i = 0;

	if ((mutex_ptr == NULL) || (mutex_ptr->valid != CORE_MUTEX_VALID))
		return -EINVAL;

	i = mutex_ptr->gate_num;
	mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
	mutex_ptr->gate_val &= SEMA4_GATE_MASK;
	/* make sure it is locked by this core */
	if (mutex_ptr->gate_val != SEMA4_A5_LOCK) {
		pr_err("%d Trying to unlock an unlock mutex.\n", __LINE__);
		ret = -EINVAL;
		goto out;
	}
	/* unlock it */
	mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
	mutex_ptr->gate_val &= (~SEMA4_GATE_MASK);
	writeb(mutex_ptr->gate_val | SEMA4_UNLOCK, vf610_sema4->ioaddr + i);
	mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
	mutex_ptr->gate_val &= SEMA4_GATE_MASK;
	/* make sure it is locked by this core */
	if (mutex_ptr->gate_val == SEMA4_A5_LOCK)
		pr_err("%d ERROR, failed to unlock the mutex.\n", __LINE__);

out:
	return ret;
}
EXPORT_SYMBOL(vf610_sema4_mutex_unlock);

static irqreturn_t vf610_sema4_isr(int irq, void *dev_id)
{
	int i;
	struct vf610_sema4_mutex *mutex_ptr;
	unsigned int mask;
	struct vf610_sema4_mutex_device *vf610_sema4 = dev_id;

	vf610_sema4->cpntf_val = readw(vf610_sema4->ioaddr + SEMA4_CP0NTF);
	for (i = 0; i < SEMA4_NUM_GATES; i++) {
		mask = idx_sema4[i];
		if ((vf610_sema4->cpntf_val) & mask) {
			mutex_ptr = vf610_sema4->mutex_ptr[i];
			/*
			 * An interrupt is pending on this mutex, the only way
			 * to clear it is to lock it (either by this core or
			 * another).
			 */
			mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
			mutex_ptr->gate_val &= (~SEMA4_GATE_MASK);
			mutex_ptr->gate_val |= SEMA4_A5_LOCK;
			writeb(mutex_ptr->gate_val, vf610_sema4->ioaddr + i);
			mutex_ptr->gate_val = readb(vf610_sema4->ioaddr + i);
			mutex_ptr->gate_val &= SEMA4_GATE_MASK;
			if (mutex_ptr->gate_val == SEMA4_A5_LOCK) {
				/*
				 * wake up the wait queue, whatever there
				 * are wait task or not.
				 * NOTE: check gate is locted or not in
				 * sema4_lock func by wait task.
				 */
				mutex_ptr->gate_val =
					readb(vf610_sema4->ioaddr + i);
				mutex_ptr->gate_val &= (~SEMA4_GATE_MASK);
				mutex_ptr->gate_val |= SEMA4_UNLOCK;

				writeb(mutex_ptr->gate_val,
						vf610_sema4->ioaddr + i);
				wake_up(&mutex_ptr->wait_q);
			} else {
				pr_debug("can't lock gate%d %s retry!\n", i,
						mutex_ptr->gate_val ?
						"locked by m4" : "");
			}
		}
	}

	return IRQ_HANDLED;
}

static const struct of_device_id vf610_sema4_dt_ids[] = {
	{ .compatible = "fsl,vf610-sema4", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vf610_sema4_dt_ids);

static int vf610_sema4_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	vf610_sema4 = devm_kzalloc(&pdev->dev, sizeof(*vf610_sema4), GFP_KERNEL);
	if (!vf610_sema4)
		return -ENOMEM;

	vf610_sema4->dev = &pdev->dev;
	vf610_sema4->cpine_val = 0;
	spin_lock_init(&vf610_sema4->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res)) {
		dev_err(&pdev->dev, "unable to get vf610 sema4 resource 0\n");
		ret = -ENODEV;
		goto err;
	}

	vf610_sema4->ioaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(vf610_sema4->ioaddr)) {
		ret = PTR_ERR(vf610_sema4->ioaddr);
		goto err;
	}

	vf610_sema4->irq = platform_get_irq(pdev, 0);
	if (!vf610_sema4->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		ret = -ENODEV;
		goto err;
	}

	ret = devm_request_irq(&pdev->dev, vf610_sema4->irq, vf610_sema4_isr,
				IRQF_SHARED, "vf610-sema4", vf610_sema4);
	if (ret) {
		dev_err(&pdev->dev, "failed to request vf610 sema4 irq\n");
		ret = -ENODEV;
		goto err;
	}

	platform_set_drvdata(pdev, vf610_sema4);

err:
	return ret;
}

static int vf610_sema4_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vf610_sema4_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "vf610-sema4",
		   .of_match_table = vf610_sema4_dt_ids,
		   },
	.probe = vf610_sema4_probe,
	.remove = vf610_sema4_remove,
};

module_platform_driver(vf610_sema4_driver);

MODULE_DESCRIPTION("VF610 SEMA4 driver");
MODULE_LICENSE("GPL");
