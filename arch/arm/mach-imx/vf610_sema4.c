#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sched.h>

#include <linux/mm.h>
#include <linux/version.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include "hardware.h"
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <asm/io.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/vf610_sema4.h>
// ************************************ Local Data *************************************************

#define NUM_GATES 16
#define MASK_FROM_GATE(gate_num) ((u32)(1 << (NUM_GATES*2 - 1 - idx[gate_num])))

#define THIS_CORE (0)
#define LOCK_VALUE (THIS_CORE + 1)

#define SEMA4_CP0INE (0x40)
#define SEMA4_CP0NTF (0x80)

static MVF_SEMA4* gates[NUM_GATES];
static void __iomem *sema4_base;

// account for the way the bits are set / returned in CP0INE and CP0NTF
static const int idx[16] = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12};

// debugfs
#define DEBUGFS_DIR "mvf_sema4"
static struct dentry *debugfs_dir;

// ************************************ Interrupt handler *************************************************

static irqreturn_t sema4_irq_handler(int irq, void *dev_id)
{
	int gate_num;

	u32 cp0ntf = readl(sema4_base + SEMA4_CP0NTF);

	for(gate_num=0; gate_num<NUM_GATES; gate_num++)
	{
		// interrupt from this gate?
		if(cp0ntf & MASK_FROM_GATE(gate_num))
		{
			// grab the gate to stop the interrupts
			writeb(LOCK_VALUE, sema4_base + gate_num);

			// make sure there's a gate assigned
			if(gates[gate_num])
			{
				//if(gates[gate_num]->use_interrupts) {
					// wake up whoever was aiting
					wake_up_interruptible(&(gates[gate_num]->wait_queue));
					// bump stats
					gates[gate_num]->interrupts++;
				//}
			}
		}
	}

	return IRQ_HANDLED;
}

// ************************************ Utility functions *************************************************

int mvf_sema4_assign(int gate_num, MVF_SEMA4** sema4_p)
{
	u32 cp0ine;
	unsigned long irq_flags;
	char debugfs_gatedir_name[4];
	struct dentry *debugfs_gate_dir;

	if((gate_num < 0) || (gate_num >= NUM_GATES))
		return -EINVAL;

	if(gates[gate_num])
		return -EBUSY;

	*sema4_p = (MVF_SEMA4 *)kmalloc(sizeof(MVF_SEMA4), GFP_KERNEL);
	if(*sema4_p == NULL)
		return -ENOMEM;
	memset(*sema4_p, 0, sizeof(MVF_SEMA4));

	gates[gate_num] = *sema4_p;
	(*sema4_p)->gate_num = gate_num;

	init_waitqueue_head(&((*sema4_p)->wait_queue));
	local_irq_save(irq_flags);
	cp0ine = readl(sema4_base + SEMA4_CP0INE);
	cp0ine |= MASK_FROM_GATE(gate_num);
	writel(cp0ine, sema4_base + SEMA4_CP0INE);
	local_irq_restore(irq_flags);

	// debugfs
	sprintf(debugfs_gatedir_name, "%d", gate_num);
	debugfs_gate_dir = debugfs_create_dir(debugfs_gatedir_name, debugfs_dir);
	debugfs_create_u32("attempts", S_IRUGO | S_IWUGO, debugfs_gate_dir, &(*sema4_p)->attempts);
	debugfs_create_u32("interrupts", S_IRUGO | S_IWUGO, debugfs_gate_dir, &(*sema4_p)->interrupts);
	debugfs_create_u32("failures", S_IRUGO | S_IWUGO, debugfs_gate_dir, &(*sema4_p)->failures);
	debugfs_create_u64("total_latency_us", S_IRUGO | S_IWUGO, debugfs_gate_dir, &(*sema4_p)->total_latency_us);
	debugfs_create_u32("worst_latency_us", S_IRUGO | S_IWUGO, debugfs_gate_dir, &(*sema4_p)->worst_latency_us);

	return 0;
}
EXPORT_SYMBOL(mvf_sema4_assign);

int mvf_sema4_deassign(MVF_SEMA4 *sema4)
{
	u32 cp0ine;
	unsigned long irq_flags;

	int gate_num;
	if(!sema4)
		return -EINVAL;
	gate_num = sema4->gate_num;

	local_irq_save(irq_flags);
	cp0ine = readl(sema4_base + SEMA4_CP0INE);
	cp0ine &= ~MASK_FROM_GATE(gate_num);
	writel(cp0ine, sema4_base + SEMA4_CP0INE);
	local_irq_restore(irq_flags);

	kfree(sema4);
	gates[gate_num] = NULL;

	return 0;
}
EXPORT_SYMBOL(mvf_sema4_deassign);

static long delta_time(struct timeval *start) {

	struct timeval now;
	long now_us, start_us;

	do_gettimeofday(&now);

	now_us = (now.tv_sec * 1000000) + now.tv_usec;
	start_us = (start->tv_sec * 1000000) + start->tv_usec;

	return now_us > start_us ? now_us - start_us : 0;
}

static void add_latency_stat(MVF_SEMA4 *sema4) {

	long latency = delta_time(&sema4->request_time);

	sema4->total_latency_us += latency;
	sema4->worst_latency_us = sema4->worst_latency_us < latency ? latency : sema4->worst_latency_us;
}

int mvf_sema4_lock(MVF_SEMA4 *sema4, unsigned int timeout_us, bool use_interrupts)
{
	int retval;
	int gate_num;
	if(!sema4)
		return -EINVAL;
	gate_num = sema4->gate_num;

	// bump stats
	gates[gate_num]->attempts++;
	do_gettimeofday(&gates[gate_num]->request_time);

	// try to grab it
	writeb(LOCK_VALUE, sema4_base + gate_num);
	if(readb(sema4_base + gate_num) == LOCK_VALUE) {
		add_latency_stat(gates[gate_num]);
		return 0;
	}

	// no timeout, fail
	if(!timeout_us) {
		gates[gate_num]->failures++;
		return -EBUSY;
	}

	// spin lock?
	if(!use_interrupts) {
		while(readb(sema4_base + gate_num) != LOCK_VALUE) {

			if((timeout_us != 0xffffffff) && (delta_time(&gates[gate_num]->request_time) > timeout_us)) {
				gates[gate_num]->failures++;
				return -EBUSY;
			}

			writeb(LOCK_VALUE, sema4_base + gate_num);
		}
		add_latency_stat(gates[gate_num]);
		return 0;
	}

	// wait forever?
	if(timeout_us == 0xffffffff)
	{
		if(wait_event_interruptible(sema4->wait_queue, (readb(sema4_base + gate_num) == LOCK_VALUE))) {
			gates[gate_num]->failures++;
			return -ERESTARTSYS;
		}
	}
	else
	{
		// return: 0 = timeout, >0 = woke up with that many jiffies left, <0 = error
		retval = wait_event_interruptible_timeout(sema4->wait_queue,
							(readb(sema4_base + gate_num) == LOCK_VALUE),
							usecs_to_jiffies(timeout_us));
		if(retval == 0) {
			gates[gate_num]->failures++;
			return -ETIME;
		}
		else if(retval < 0) {
			gates[gate_num]->failures++;
			return retval;
		}
	}

	add_latency_stat(gates[gate_num]);
	return 0;
}
EXPORT_SYMBOL(mvf_sema4_lock);

int mvf_sema4_unlock(MVF_SEMA4 *sema4)
{
	if(!sema4)
		return -EINVAL;

	// unlock it
	writeb(0, sema4_base + sema4->gate_num);

	return 0;
}
EXPORT_SYMBOL(mvf_sema4_unlock);

// return 0 on success (meaning it is set to us)
int mvf_sema4_test(MVF_SEMA4 *sema4)
{
	if(!sema4)
		return -EINVAL;

	return (readb(sema4_base + sema4->gate_num)) == LOCK_VALUE ? 0 : 1;
}
EXPORT_SYMBOL(mvf_sema4_test);

static int vf610_sema4_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *iores;
	int irq, ret, i;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sema4_base = devm_ioremap_resource(dev, iores);
	if (IS_ERR(sema4_base))
		return PTR_ERR(sema4_base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq specified\n");
		return irq;
	}

	ret = devm_request_irq(dev, irq, sema4_irq_handler, 0, "SEMA4", NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to claim irq %u\n", irq);
		return ret;
	}

	// clear the gates table
	for (i = 0; i < NUM_GATES; i++)
		gates[i] = NULL;

	writel_relaxed(0, sema4_base + SEMA4_CP0INE);

	// debugfs
	debugfs_dir = debugfs_create_dir(DEBUGFS_DIR, NULL);

	return 0;
}

static const struct of_device_id vf610_sema4_dt_ids[] = {
	{ .compatible = "fsl,vf610-sema4" },
	{ /* sentinel */ }
};

static struct platform_driver vf610_sema4_compat_driver = {
	.driver		= {
		.name	= "vf610-sema4",
		.owner	= THIS_MODULE,
		.of_match_table = vf610_sema4_dt_ids,
	},
	.probe		= vf610_sema4_probe,
};

static int __init vf610_sema4_compat_init(void)
{
	return platform_driver_register(&vf610_sema4_compat_driver);
}
device_initcall(vf610_sema4_compat_init);

MODULE_DESCRIPTION("Freescale SEMA4 driver");
MODULE_LICENSE("GPL v2");
