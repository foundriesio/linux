#ifndef _TCC_ASRC_DAI_H_
#define _TCC_ASRC_DAI_H_

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <sound/soc.h>

int tcc_asrc_dai_drvinit(struct platform_device *pdev);

#define TCC_ASRC_CLKID_PERI_DAI_RATE	(0x0)
#define TCC_ASRC_CLKID_PERI_DAI_FORMAT	(0x1)
#define TCC_ASRC_CLKID_PERI_DAI			(0x2)

#endif //_TCC_ASRC_DAI_H_
