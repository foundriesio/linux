/*
 * mcf_edma.c - Freescale eDMA driver for ColdFire and Vybird platform.
 *
 * Copyright 2008-2012 Freescale Semiconductor, Inc.
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ***************************************************************************
 * Changes:
 *   v0.002    29 February 2008        Andrey Butok, Freescale Semiconductor
 *             Added support of atomatic channel allocation from the
 *             channel pool.
 *   v0.001    12 February 2008                Andrey Butok
 *             Initial Release - developed on uClinux with 2.6.23 kernel.
 *             Based on coldfire_edma.c code
 *             of Yaroslav Vinogradov (Freescale Semiconductor, Inc.)
 *
 * NOTE: This driver was tested on MCF52277 platform.
 *      It should also work on other Coldfire platdorms with eDMA module.
 *
 * TBD: Try to make it more general.
 *     Try to integrate with current <asm/dma.h> <kernel/dma.c> API
 *     or use Intel DMA API
 */

#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#ifdef CONFIG_COLDFIRE
#include <asm/mcf_edma.h>
#include <asm/coldfire.h>
#endif
#ifdef CONFIG_SOC_MVFA5
#include <mach/mcf_edma.h>
#endif

/* Please add here processors that were tested with this driver */
#if !defined(CONFIG_M5227x) && !defined(CONFIG_M5445X) && \
	!defined(CONFIG_M5441X) && !defined(CONFIG_ARCH_MVF)
#error "The driver is not tested/designed for your processor!"
#endif

#define EDMA_DRIVER_VERSION	"Revision: 0.003"
#define EDMA_DRIVER_AUTHOR	"Freescale Semiconductor Inc, Andrey Butok"
#define EDMA_DRIVER_DESC	"Freescale EDMA driver."
#define EDMA_DRIVER_INFO	DRIVER_VERSION " " DRIVER_DESC
#define EDMA_DRIVER_LICENSE	"GPL"
#define EDMA_DRIVER_NAME	"mcf_edma"

#define EDMA_DEV_MINOR	(1)

#undef EDMA_DEBUG

#ifdef EDMA_DEBUG
#define DBG(fmt, args...)	printk(KERN_INFO "[%s]  " fmt, \
				__func__, ## args)
#else
#define DBG(fmt, args...)	do {} while (0)
#endif

#define ERR(format, arg...)	printk(KERN_ERR "%s:%s: " format "\n", \
				 __FILE__,  __func__ , ## arg)
#define INFO(stuff...)		printk(KERN_INFO EDMA_DRIVER_NAME \
				": " stuff)
#ifdef CONFIG_COLDFIRE
/* DMA channel pool used for atomtic channel allocation.
 * You can edit this list. First candidates are "Not used/Reserved" channels */
u8 mcf_edma_channel_pool[] = { 1,	/* Not used */
	0,			/* External DMA request */
	5,			/* UART1 Receive */
	6,			/* UART1 Transmit */
	7,			/* UART2 Receive */
	8,			/* UART2 Transmit */
#if defined(CONFIG_M5441X)
	16,
	55,
	56,
	63,
#endif
};
#elif defined(CONFIG_ARCH_MVF)
/*
 * The always-on source pool of DMAMUX
 *
 */
u8 mvf_dma_mux_pool[20] = {0,};
#endif

/*
 * Callback handler data for each TCD
 */
struct mcf_edma_isr_record {
	irqreturn_t(*irq_handler) (int, void *);	/* interrupt handler */
	void (*error_handler) (int, void *);	/* error interrupt handler */
	void *arg;		/* argument to pass back */
	int allocated;		/* busy flag */
	int slot;		/* unioned DMA MUX request slot number */
	spinlock_t *lock;	/* spin lock (optional) */
	const char *device_id;	/* dev id string, used in procfs */
};

/*
 * Device structure
 */
struct mcf_edma_dev {
	struct cdev cdev;	/* character device */
#ifdef CONFIG_ARCH_MVF
	void *dma_base_addr[2];	/* DMA0/1 memory map base address */
	void *dmamux_base_addr[4];
#endif
	struct mcf_edma_isr_record dma_interrupt_handlers[MCF_EDMA_CHANNELS];
};

/* allocated major device number */
static int mcf_edma_major;

/* device driver structure */
static struct mcf_edma_dev *mcf_edma_devp;

#ifdef CONFIG_M54455
/* PATA controller structure */
static struct {
	struct TCD *pata_tcd_va;
	dma_addr_t pata_tcd_pa;
} fsl_pata_dma_tcd;
#endif

/* device driver file operations */
const struct file_operations mcf_edma_fops = {
	.owner = THIS_MODULE,
};

/*
 * mcf_edma_isr - eDMA channel interrupt handler
 * @irq: interrupt number
 * @dev_id: argument
 */
static irqreturn_t
mcf_edma_isr(int irq, void *dev_id)
{
	int channel = -1;
	int result = IRQ_HANDLED;

#if defined(CONFIG_M5445X)
	channel = irq - MCF_EDMA_INT0_BASE;
#elif defined(CONFIG_M5441X)
	if (irq >= MCF_EDMA_INT0_BASE &&
	    irq < MCF_EDMA_INT0_BASE + MCF_EDMA_INT0_NUM)
		channel = irq - MCF_EDMA_INT0_BASE;
	else if (irq >= MCF_EDMA_INT1_BASE &&
		 irq < MCF_EDMA_INT1_BASE + MCF_EDMA_INT1_NUM)
		channel = irq - MCF_EDMA_INT1_BASE + MCF_EDMA_INT0_END;
	else if (irq == MCF_EDMA_INT2_BASE &&
		 irq < MCF_EDMA_INT2_BASE + MCF_EDMA_INT2_NUM) {
		int i;
		for (i = 0; i < MCF_EDMA_INT2_NUM; i++) {
			if ((MCF_EDMA_INTH >> 24) & (0x1 << i)) {
				channel = irq - MCF_EDMA_INT2_BASE +
					  MCF_EDMA_INT1_END + i;
				break;
			}
		}
	} else {
		ERR("Bad irq number at isr!\n");
		return result;
	}
#elif defined(CONFIG_ARCH_MVF)
	int intr, i;
	void *addr;
	if (irq == MVF_INT_DMA0_TX) {
		addr = mcf_edma_devp->dma_base_addr[0];
	} else if (irq == MVF_INT_DMA1_TX) {
		addr = mcf_edma_devp->dma_base_addr[1];
	} else {
		ERR("Bad irq number at isr!\n");
		return result;
	}
	intr = readl(addr + MCF_EDMA_INTR);
	for (i = 0; i < 32; i++)
		if (intr & (0x1 << i)) {
			channel = i;
			break;
		}

	if (irq == MVF_INT_DMA1_TX)
		channel += 32;

#endif

	DBG("\n");

	if ((mcf_edma_devp != NULL) &&
	    (mcf_edma_devp->dma_interrupt_handlers[channel].irq_handler)) {
		/* call user irq handler */
		if (mcf_edma_devp->dma_interrupt_handlers[channel].lock)
			spin_lock(mcf_edma_devp->
				  dma_interrupt_handlers[channel].lock);

		result =
		    mcf_edma_devp->dma_interrupt_handlers[channel].
		    irq_handler(channel,
				mcf_edma_devp->dma_interrupt_handlers[channel].
				arg);

		if (mcf_edma_devp->dma_interrupt_handlers[channel].lock)
			spin_unlock(mcf_edma_devp->
				    dma_interrupt_handlers[channel].lock);
	} else {
		/* no irq handler so just ack it */
		mcf_edma_confirm_interrupt_handled(channel);
		ERR(" No handler for DMA channel (%d)\n", channel);
	}
#ifdef CONFIG_ARCH_MVF
	writeb(MCF_EDMA_CINT_CINT(channel >= 32 ? channel - 32 : channel),
			addr + MCF_EDMA_CINT);
#endif
	return result;
}

/**
 * mcf_edma_error_isr - eDMA error interrupt handler
 * @irq: interrupt number
 * @dev_id: argument
 */
static irqreturn_t
mcf_edma_error_isr(int irq, void *dev_id)
{
	int i;

#if defined(CONFIG_M5445X)
	u16 err;

	err = MCF_EDMA_ERR;
	for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		if (err & (1 << i)) {
			if (mcf_edma_devp != NULL &&
			    mcf_edma_devp->dma_interrupt_handlers[i].
			    error_handler)
				mcf_edma_devp->dma_interrupt_handlers[i].
				    error_handler(i,
						  mcf_edma_devp->
						  dma_interrupt_handlers[i].
						  arg);
			else
				ERR(" DMA error on channel (%d)\n", i);
		}
	}
#elif defined(CONFIG_M5441X)
	u32 errl, errh;

	errl = MCF_EDMA_ERRL;
	errh = MCF_EDMA_ERRH;

	for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		if ((errl & (1 << i)) || (errh & (1 << (i - 32)))) {
			if (mcf_edma_devp != NULL &&
				mcf_edma_devp->dma_interrupt_handlers[i].
				error_handler)
				mcf_edma_devp->dma_interrupt_handlers[i].
					error_handler(i, mcf_edma_devp->
						dma_interrupt_handlers[i].arg);
			else
				ERR(" DMA error on channel (%d)\n", i);
		}
	}
#elif defined(CONFIG_ARCH_MVF)
	u32 err;
	if (irq == MVF_INT_DMA0_ERR) {
		err = readl(mcf_edma_devp->dma_base_addr[0] + MCF_EDMA_ERR);
		for (i = 0; i < 32; i++) {
			if (err & (1 << i)) {
				if (mcf_edma_devp != NULL &&
					mcf_edma_devp->
					dma_interrupt_handlers[i].
					error_handler)
						mcf_edma_devp->
						dma_interrupt_handlers[i].
					error_handler(i, mcf_edma_devp->
					dma_interrupt_handlers[i].arg);
				else
					ERR(" DMA error on channel (%d)\n", i);
			}
		}
	} else if (irq == MVF_INT_DMA1_ERR) {
		err = readl(mcf_edma_devp->dma_base_addr[1] + MCF_EDMA_ERR);
		for (i = 0; i < 32; i++) {
			if (err & (1 << i)) {
				if (mcf_edma_devp != NULL &&
					mcf_edma_devp->
					dma_interrupt_handlers[i + 32].
					error_handler)
						mcf_edma_devp->
						dma_interrupt_handlers[i + 32].
					error_handler(i + 32, mcf_edma_devp->
					dma_interrupt_handlers[i + 32].arg);
				else
					ERR(" DMA error on channel (%d)\n", i);
			}
		}

	}
#endif
#if defined(CONFIG_COLDFIRE)
	MCF_EDMA_CERR = MCF_EDMA_CERR_CAER;
#elif defined(CONFIG_ARCH_MVF)
	writeb(MCF_EDMA_CERR_CAER,
		mcf_edma_devp->dma_base_addr[(irq - MVF_INT_DMA0_ERR)/2] +
		MCF_EDMA_CERR);
#endif
	return IRQ_HANDLED;
}

/**
 * mcf_edma_check_done - Check if channel is finished or not
 * @channel: channel number
 * return: 0 if not done yet
 */
int
mcf_edma_check_done(int channel)
{
	if (channel < 0 || channel > MCF_EDMA_CHANNELS)
		return 1;
#if defined(CONFIG_COLDFIRE)
	return MCF_EDMA_TCD_CSR(channel) & MCF_EDMA_TCD_CSR_DONE;
#elif defined(CONFIG_ARCH_MVF)
	if (channel < 32)
		return readw(mcf_edma_devp->dma_base_addr[0] +
				MCF_EDMA_TCD_CSR(channel)) &
			MCF_EDMA_TCD_CSR_DONE;
	else
		return readw(mcf_edma_devp->dma_base_addr[1] +
				MCF_EDMA_TCD_CSR(channel - 32)) &
			MCF_EDMA_TCD_CSR_DONE;

#endif
}
EXPORT_SYMBOL(mcf_edma_check_done);

/**
 * mcf_edma_set_tcd_params - Set transfer control descriptor (TCD)
 * @channel: channel number
 * @source: source address
 * @dest: destination address
 * @attr: attributes
 * @soff: source offset
 * @nbytes: number of bytes to be transfered in minor loop
 * @slast: last source address adjustment
 * @citer: major loop count
 * @biter: beginning major loop count
 * @doff: destination offset
 * @dlast_sga: last destination address adjustment
 * @major_int: generate interrupt after each major loop
 * @disable_req: disable DMA request after major loop
 */
void
mcf_edma_set_tcd_params(int channel, u32 source, u32 dest,
			u32 attr, u32 soff, u32 nbytes, u32 slast,
			u32 citer, u32 biter, u32 doff, u32 dlast_sga,
			int major_int, int disable_req, int enable_sg)
{
	void *addr;
	unsigned short csr = 0;
	DBG("(%d)\n", channel);

	if (channel < 0 || channel > MCF_EDMA_CHANNELS)
		return;
#if defined(CONFIG_ARCH_MVF)
	if (channel >= 32) {
		addr = mcf_edma_devp->dma_base_addr[1];
		channel -= 32;
	} else
		addr = mcf_edma_devp->dma_base_addr[0];

	writel(source, addr + MCF_EDMA_TCD_SADDR(channel));
	writel(dest, addr + MCF_EDMA_TCD_DADDR(channel));
	writew(attr, addr + MCF_EDMA_TCD_ATTR(channel));
	writew(MCF_EDMA_TCD_SOFF_SOFF(soff),
			addr + MCF_EDMA_TCD_SOFF(channel));
	writel(MCF_EDMA_TCD_NBYTES_NBYTES(nbytes),
			addr + MCF_EDMA_TCD_NBYTES(channel));
	writel(MCF_EDMA_TCD_SLAST_SLAST(slast),
			addr + MCF_EDMA_TCD_SLAST(channel));
	writew(MCF_EDMA_TCD_CITER_CITER(citer),
			addr + MCF_EDMA_TCD_CITER(channel));
	writew(MCF_EDMA_TCD_BITER_BITER(biter),
			addr + MCF_EDMA_TCD_BITER(channel));
	writew(MCF_EDMA_TCD_DOFF_DOFF(doff),
			addr + MCF_EDMA_TCD_DOFF(channel));
	writel(MCF_EDMA_TCD_DLAST_SGA_DLAST_SGA(dlast_sga),
			addr + MCF_EDMA_TCD_DLAST_SGA(channel));
	writew(csr, addr + MCF_EDMA_TCD_CSR(channel));

	/* interrupt at the end of major loop */
	if (major_int)
		csr |= MCF_EDMA_TCD_CSR_INT_MAJOR;

	/* disable request at the end of major loop of transfer or not */
	if (disable_req)
		csr |= MCF_EDMA_TCD_CSR_D_REQ;

	if (enable_sg)
		csr |= MCF_EDMA_TCD_CSR_E_SG;

	writew(csr, addr + MCF_EDMA_TCD_CSR(channel));
	/* enable error interrupt */
	writeb(MCF_EDMA_SEEI_SEEI(channel), addr + MCF_EDMA_SEEI);

#elif defined(CONFIG_COLDFIRE)
	MCF_EDMA_TCD_SADDR(channel) = source;
	MCF_EDMA_TCD_DADDR(channel) = dest;
	MCF_EDMA_TCD_ATTR(channel) = attr;
	MCF_EDMA_TCD_SOFF(channel) = MCF_EDMA_TCD_SOFF_SOFF(soff);
	MCF_EDMA_TCD_NBYTES(channel) = MCF_EDMA_TCD_NBYTES_NBYTES(nbytes);
	MCF_EDMA_TCD_SLAST(channel) = MCF_EDMA_TCD_SLAST_SLAST(slast);
	MCF_EDMA_TCD_CITER(channel) = MCF_EDMA_TCD_CITER_CITER(citer);
	MCF_EDMA_TCD_BITER(channel) = MCF_EDMA_TCD_BITER_BITER(biter);
	MCF_EDMA_TCD_DOFF(channel) = MCF_EDMA_TCD_DOFF_DOFF(doff);
	MCF_EDMA_TCD_DLAST_SGA(channel) =
	    MCF_EDMA_TCD_DLAST_SGA_DLAST_SGA(dlast_sga);
	MCF_EDMA_TCD_CSR(channel) = 0x0000;

	/* interrupt at the end of major loop */
	if (major_int)
		MCF_EDMA_TCD_CSR(channel) |= MCF_EDMA_TCD_CSR_INT_MAJOR;
	else
		MCF_EDMA_TCD_CSR(channel) &= ~MCF_EDMA_TCD_CSR_INT_MAJOR;

	/* disable request at the end of major loop of transfer or not */
	if (disable_req)
		MCF_EDMA_TCD_CSR(channel) |= MCF_EDMA_TCD_CSR_D_REQ;
	else
		MCF_EDMA_TCD_CSR(channel) &= ~MCF_EDMA_TCD_CSR_D_REQ;

	/* enable error interrupt */
	MCF_EDMA_SEEI = MCF_EDMA_SEEI_SEEI(channel);
#endif
}
EXPORT_SYMBOL(mcf_edma_set_tcd_params);
#ifdef CONFIG_M54455
/**
 * mcf_edma_sg_config - config an eDMA channel to use the S/G tcd feature
 * @channel: channel number
 * @buf: the array of tcd sg
 * @nents: number of tcd sg array, the max is 256 set but can modify
 *
 * limitation:
 *	currently this function is only for PATA RX/TX on MCF54455,
 *	so eDMA init does not allocate TCD memory for other memory
 *
 * TODO:
 *	any one who need this feature shoule add his own TCD memory init
 */
void mcf_edma_sg_config(int channel, struct fsl_edma_requestbuf *buf,
			int nents)
{
	struct TCD *vtcd = (struct TCD *)fsl_pata_dma_tcd.pata_tcd_va;
	u32 ptcd = fsl_pata_dma_tcd.pata_tcd_pa;
	struct fsl_edma_requestbuf *pb = buf;
	int i;

	if (channel < MCF_EDMA_CHAN_ATA_RX || channel > MCF_EDMA_CHAN_ATA_TX) {
		printk(KERN_ERR "mcf edma sg config err, not support\n");
		return;
	}
	if (nents > MCF_EDMA_TCD_PER_CHAN) {
		printk(KERN_ERR "Too many SGs, please confirm.%d > %d\n",
				nents, MCF_EDMA_TCD_PER_CHAN);
		return;
	}

	/* build our tcd sg array */
	for (i = 0; i < nents; i++) {
		memset(vtcd, 0 , sizeof(struct TCD));
		vtcd->saddr = pb->saddr;
		vtcd->daddr = pb->daddr;
		vtcd->attr = pb->attr;
		vtcd->soff = pb->soff;
		vtcd->doff = pb->doff;
		vtcd->nbytes = pb->minor_loop;
		vtcd->citer = vtcd->biter = pb->len/pb->minor_loop;

		if (i != nents - 1) {
			vtcd->csr |= MCF_EDMA_TCD_CSR_E_SG;/* we are tcd sg */
			vtcd->dlast_sga =
				(u32)(ptcd + (i + 1)*sizeof(struct TCD));
		} else {
			/*this is the last sg, so enable the major int*/
			vtcd->csr |= MCF_EDMA_TCD_CSR_INT_MAJOR
					|MCF_EDMA_TCD_CSR_D_REQ;
		}
		pb++;
		vtcd++;
	}

	/* Now setup the firset TCD for this sg to the edma enginee */
	vtcd = fsl_pata_dma_tcd.pata_tcd_va;

	MCF_EDMA_TCD_CSR(channel) = 0x0000;
	MCF_EDMA_TCD_SADDR(channel) = vtcd->saddr;
	MCF_EDMA_TCD_DADDR(channel) = vtcd->daddr;
	MCF_EDMA_TCD_ATTR(channel) = vtcd->attr;
	MCF_EDMA_TCD_SOFF(channel) = MCF_EDMA_TCD_SOFF_SOFF(vtcd->soff);
	MCF_EDMA_TCD_NBYTES(channel) = MCF_EDMA_TCD_NBYTES_NBYTES(vtcd->nbytes);
	MCF_EDMA_TCD_SLAST(channel) = MCF_EDMA_TCD_SLAST_SLAST(vtcd->slast);
	MCF_EDMA_TCD_CITER(channel) = MCF_EDMA_TCD_CITER_CITER(vtcd->citer);
	MCF_EDMA_TCD_BITER(channel) = MCF_EDMA_TCD_BITER_BITER(vtcd->biter);
	MCF_EDMA_TCD_DOFF(channel) = MCF_EDMA_TCD_DOFF_DOFF(vtcd->doff);
	MCF_EDMA_TCD_DLAST_SGA(channel) =
	    MCF_EDMA_TCD_DLAST_SGA_DLAST_SGA(vtcd->dlast_sga);
	MCF_EDMA_TCD_CSR(channel) |= vtcd->csr;
}
EXPORT_SYMBOL(mcf_edma_sg_config);

/**
 * The zero-copy version of mcf_edma_sg_config
 * dma_dir : indicate teh addr direction
 */
void mcf_edma_sglist_config(int channel, struct scatterlist *sgl, int n_elem,
			int dma_dir, u32 addr, u32 attr,
			u32 soff, u32 doff, u32 nbytes)
{
	struct TCD *vtcd = (struct TCD *)fsl_pata_dma_tcd.pata_tcd_va;
	u32 ptcd = fsl_pata_dma_tcd.pata_tcd_pa;
	struct scatterlist *sg;
	u32 si;

	if (channel < MCF_EDMA_CHAN_ATA_RX || channel > MCF_EDMA_CHAN_ATA_TX) {
		printk(KERN_ERR "mcf edma sg config err, not support\n");
		return;
	}
	if (n_elem > MCF_EDMA_TCD_PER_CHAN) {
		printk(KERN_ERR "Too many SGs, please confirm.%d > %d\n",
				n_elem, MCF_EDMA_TCD_PER_CHAN);
		return;
	}

	/* build our tcd sg array */
	if (dma_dir == DMA_TO_DEVICE) {	/* write */
		for_each_sg(sgl, sg, n_elem, si) {
			memset(vtcd, 0 , sizeof(struct TCD));
			vtcd->saddr = sg_dma_address(sg);
			vtcd->daddr = addr;
			vtcd->attr = attr;
			vtcd->soff = soff;
			vtcd->doff = doff;
			vtcd->nbytes = nbytes;
			vtcd->citer = vtcd->biter = sg_dma_len(sg)/nbytes;

			if (si != n_elem - 1) {
				/* we are tcd sg */
				vtcd->csr |= MCF_EDMA_TCD_CSR_E_SG;
				vtcd->dlast_sga = (u32)(ptcd + (si + 1) * \
							sizeof(struct TCD));
			} else {
				/*this is the last sg, so enable the major int*/
				vtcd->csr |= MCF_EDMA_TCD_CSR_INT_MAJOR
					|MCF_EDMA_TCD_CSR_D_REQ;
			}
			vtcd++;
		}
	} else {
		for_each_sg(sgl, sg, n_elem, si) {
			memset(vtcd, 0 , sizeof(struct TCD));
			vtcd->daddr = sg_dma_address(sg);
			vtcd->saddr = addr;
			vtcd->attr = attr;
			vtcd->soff = soff;
			vtcd->doff = doff;
			vtcd->nbytes = nbytes;
			vtcd->citer = vtcd->biter = sg_dma_len(sg)/nbytes;

			if (si != n_elem - 1) {
				/* we are tcd sg */
				vtcd->csr |= MCF_EDMA_TCD_CSR_E_SG;
				vtcd->dlast_sga = (u32)(ptcd + (si + 1) * \
							sizeof(struct TCD));
			} else {
				/*this is the last sg, so enable the major int*/
				vtcd->csr |= MCF_EDMA_TCD_CSR_INT_MAJOR
					|MCF_EDMA_TCD_CSR_D_REQ;
			}
			vtcd++;
		}
	}

	/* Now setup the firset TCD for this sg to the edma enginee */
	vtcd = fsl_pata_dma_tcd.pata_tcd_va;

	MCF_EDMA_TCD_CSR(channel) = 0x0000;
	MCF_EDMA_TCD_SADDR(channel) = vtcd->saddr;
	MCF_EDMA_TCD_DADDR(channel) = vtcd->daddr;
	MCF_EDMA_TCD_ATTR(channel) = vtcd->attr;
	MCF_EDMA_TCD_SOFF(channel) = MCF_EDMA_TCD_SOFF_SOFF(vtcd->soff);
	MCF_EDMA_TCD_NBYTES(channel) = MCF_EDMA_TCD_NBYTES_NBYTES(vtcd->nbytes);
	MCF_EDMA_TCD_SLAST(channel) = MCF_EDMA_TCD_SLAST_SLAST(vtcd->slast);
	MCF_EDMA_TCD_CITER(channel) = MCF_EDMA_TCD_CITER_CITER(vtcd->citer);
	MCF_EDMA_TCD_BITER(channel) = MCF_EDMA_TCD_BITER_BITER(vtcd->biter);
	MCF_EDMA_TCD_DOFF(channel) = MCF_EDMA_TCD_DOFF_DOFF(vtcd->doff);
	MCF_EDMA_TCD_DLAST_SGA(channel) =
	    MCF_EDMA_TCD_DLAST_SGA_DLAST_SGA(vtcd->dlast_sga);

	MCF_EDMA_TCD_CSR(channel) |= vtcd->csr;
}
EXPORT_SYMBOL(mcf_edma_sglist_config);
#endif
/**
 * mcf_edma_set_tcd_params_halfirq - Set TCD AND enable half irq
 * @channel: channel number
 * @source: source address
 * @dest: destination address
 * @attr: attributes
 * @soff: source offset
 * @nbytes: number of bytes to be transfered in minor loop
 * @slast: last source address adjustment
 * @biter: beginning major loop count
 * @doff: destination offset
 * @dlast_sga: last destination address adjustment
 * @disable_req: disable DMA request after major loop
 */
void
mcf_edma_set_tcd_params_halfirq(int channel, u32 source, u32 dest,
				u32 attr, u32 soff, u32 nbytes, u32 slast,
				u32 biter, u32 doff, u32 dlast_sga,
				int disable_req, int enable_sg)
{
	void *addr;
	unsigned short csr;
	DBG("(%d)\n", channel);

	if (channel < 0 || channel > MCF_EDMA_CHANNELS)
		return;

	if (channel >= 32)
		addr = mcf_edma_devp->dma_base_addr[1];
	else
		addr = mcf_edma_devp->dma_base_addr[0];

	mcf_edma_set_tcd_params(channel, source, dest,
				attr, soff, nbytes, slast,
				biter, biter, doff, dlast_sga,
				1/*0*/, disable_req, enable_sg);

	if (biter < 2)
		printk(KERN_ERR "MCF_EDMA: Request for halfway irq denied\n");

	/* interrupt midway through major loop */
#if defined(CONFIG_ARCH_MVF)
	csr = readw(addr + MCF_EDMA_TCD_CSR(channel));
	writew(csr | MCF_EDMA_TCD_CSR_INT_HALF,
			addr + MCF_EDMA_TCD_CSR(channel));
#elif defined(CONFIG_COLDFIRE)
	MCF_EDMA_TCD_CSR(channel) |= MCF_EDMA_TCD_CSR_INT_HALF;
#endif
}
EXPORT_SYMBOL(mcf_edma_set_tcd_params_halfirq);

/**
 * mcf_edma_request_channel - Request an eDMA channel
 * @channel: channel number. In case it is equal to EDMA_CHANNEL_ANY
 *		it will be allocated a first free eDMA channel.
 * @handler: dma handler
 * @error_handler: dma error handler
 * @irq_level: irq level for the dma handler
 * @arg: argument to pass back
 * @lock: optional spinlock to hold over interrupt
 * @device_id: device id
 *
 * Returns allocatedd channel number if success or
 * a negative value if failure.
 */
int
mcf_edma_request_channel(int channel,
			 irqreturn_t(*handler) (int, void *),
			 void (*error_handler) (int, void *),
			 u8 irq_level,
			 void *arg, spinlock_t *lock, const char *device_id)
{
#ifdef CONFIG_ARCH_MVF
	int cfg_ch = 0, i;
#endif
	DBG("\n channel=%d\n", channel);

	if (mcf_edma_devp != NULL
	    && ((channel >= 0 && channel <= MCF_EDMA_SOURCES)
		|| (channel == MCF_EDMA_CHANNEL_ANY))) {
#if defined(CONFIG_COLDFIRE)
		if (channel == MCF_EDMA_CHANNEL_ANY) {
			int i;
			for (i = 0; i < sizeof(mcf_edma_channel_pool); i++) {
				if (mcf_edma_devp->dma_interrupt_handlers
				    [mcf_edma_channel_pool[i]].allocated ==
				    0) {
					channel = mcf_edma_channel_pool[i];
					break;
				}
			};
			if (channel == MCF_EDMA_CHANNEL_ANY)
				return -EBUSY;
		} else {
			if (mcf_edma_devp->dma_interrupt_handlers[channel].
			    allocated)
				return -EBUSY;
		}
#elif defined(CONFIG_ARCH_MVF)
		if (channel == MCF_EDMA_CHANNEL_ANY) {
			/* always-on source for soft request */
			/* first find a free request source */
			for (i = 0; i < 20; i++) {
				if (mvf_dma_mux_pool[i] == 0) {
					mvf_dma_mux_pool[i] = 1;
					break;
				}
			}
			if (i >= 20)
				return -EBUSY;
			channel = i >= 10 ? 64 + 54 + i : 54 + i;
		}

		if (channel >= 64) {
			/* It's a DMAMUX1,2 source
			 * first find channel from DMAMUX2 to MUX1
			 */
			for (i = 47; i >= 16; i--)
				if (mcf_edma_devp->dma_interrupt_handlers[i].
						allocated == 0) {
					cfg_ch = i;
					break;
				}

			if (i < 16)
				return -EBUSY;
		} else {
			/* DMAMUX0,3 source, find channel from MUX0 to MUX3 */
			for (i = 0; i < 16; i++) {
				if (mcf_edma_devp->dma_interrupt_handlers[i].
						allocated == 0) {
					cfg_ch = i;
					break;
				}

			}

			if (i >= 16) {
				for (i = 48; i < 64; i++) {
					if (mcf_edma_devp->
						dma_interrupt_handlers[i].
							allocated == 0) {
						cfg_ch = i;
						break;
					}
				}
			}

			if (i > 64)
				return -EBUSY;
		}

#endif
#if defined(CONFIG_COLDFIRE)

		mcf_edma_devp->dma_interrupt_handlers[channel].allocated = 1;
		mcf_edma_devp->dma_interrupt_handlers[channel].irq_handler =
		    handler;
		mcf_edma_devp->dma_interrupt_handlers[channel].error_handler =
		    error_handler;
		mcf_edma_devp->dma_interrupt_handlers[channel].arg = arg;
		mcf_edma_devp->dma_interrupt_handlers[channel].lock = lock;
		mcf_edma_devp->dma_interrupt_handlers[channel].device_id =
		    device_id;
#elif defined(CONFIG_ARCH_MVF)
		mcf_edma_devp->dma_interrupt_handlers[cfg_ch].allocated = 1;
		mcf_edma_devp->dma_interrupt_handlers[cfg_ch].irq_handler =
		    handler;
		mcf_edma_devp->dma_interrupt_handlers[cfg_ch].error_handler =
		    error_handler;
		mcf_edma_devp->dma_interrupt_handlers[cfg_ch].arg = arg;
		mcf_edma_devp->dma_interrupt_handlers[cfg_ch].lock = lock;
		mcf_edma_devp->dma_interrupt_handlers[cfg_ch].device_id =
		    device_id;

#endif
		/* Initalize interrupt controller to allow eDMA interrupts */
#if defined(CONFIG_M5445X)
		MCF_INTC0_ICR(MCF_EDMA_INT0_CHANNEL_BASE + channel) = irq_level;
		MCF_INTC0_CIMR = MCF_EDMA_INT0_CHANNEL_BASE + channel;
#elif defined(CONFIG_M5441X)
		if (channel >= 0 && channel < MCF_EDMA_INT0_END) {
			MCF_INTC0_ICR(MCF_EDMA_INT0_CHANNEL_BASE + channel) =
				irq_level;
			MCF_INTC0_CIMR = MCF_EDMA_INT0_CHANNEL_BASE + channel;
		} else if (channel >= MCF_EDMA_INT0_END &&
			   channel < MCF_EDMA_INT1_END) {
			MCF_INTC1_ICR(MCF_EDMA_INT1_CHANNEL_BASE +
				(channel - MCF_EDMA_INT0_END)) = irq_level;
			MCF_INTC1_CIMR = MCF_EDMA_INT1_CHANNEL_BASE +
				(channel - MCF_EDMA_INT0_END);
		} else if (channel >= MCF_EDMA_INT1_END &&
			   channel < MCF_EDMA_INT2_END) {
			MCF_INTC2_ICR(MCF_EDMA_INT2_CHANNEL_BASE) = irq_level;
			MCF_INTC2_CIMR = MCF_EDMA_INT2_CHANNEL_BASE;
		} else
			ERR("Bad channel number!\n");
#elif defined(CONFIG_ARCH_MVF)
		/* config the dma mux to route the source */
		if (channel >= 64)
			channel -= 64;
		writeb(0x00, mcf_edma_devp->dmamux_base_addr[cfg_ch / 16] +
				DMAMUX_CHCFG(cfg_ch % 16));

		writeb(DMAMUX_CHCFG_ENBL|DMAMUX_CHCFG_SOURCE(channel),
				mcf_edma_devp->dmamux_base_addr[cfg_ch/16] +
				DMAMUX_CHCFG(cfg_ch%16));
		mcf_edma_devp->dma_interrupt_handlers[cfg_ch].slot = channel;

		return cfg_ch;
#endif
		return channel;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(mcf_edma_request_channel);

/**
 * mcf_edma_set_callback - Update the channel callback/arg
 * @channel: channel number
 * @handler: dma handler
 * @error_handler: dma error handler
 * @arg: argument to pass back
 *
 * Returns 0 if success or a negative value if failure
 */
int
mcf_edma_set_callback(int channel,
		      irqreturn_t(*handler) (int, void *),
		      void (*error_handler) (int, void *), void *arg)
{
	DBG("\n");

	if (mcf_edma_devp != NULL && channel >= 0
	    && channel <= MCF_EDMA_CHANNELS
	    && mcf_edma_devp->dma_interrupt_handlers[channel].allocated) {
		mcf_edma_devp->dma_interrupt_handlers[channel].irq_handler =
		    handler;
		mcf_edma_devp->dma_interrupt_handlers[channel].error_handler =
		    error_handler;
		mcf_edma_devp->dma_interrupt_handlers[channel].arg = arg;
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(mcf_edma_set_callback);

/**
 * mcf_edma_free_channel - Free the edma channel
 * @channel: channel number
 * @arg: argument created with
 *
 * Returns 0 if success or a negative value if failure
 */
int
mcf_edma_free_channel(int channel, void *arg)
{
	int slot;
	DBG("\n");

	if (mcf_edma_devp != NULL && channel >= 0
	    && channel <= MCF_EDMA_CHANNELS) {
		if (mcf_edma_devp->dma_interrupt_handlers[channel].allocated) {
#if 1
			if (mcf_edma_devp->dma_interrupt_handlers[channel].
			    arg != arg)
				return -EBUSY;
#endif

			mcf_edma_devp->dma_interrupt_handlers[channel].
			    allocated = 0;
			mcf_edma_devp->dma_interrupt_handlers[channel].arg =
			    NULL;
			mcf_edma_devp->dma_interrupt_handlers[channel].
			    irq_handler = NULL;
			mcf_edma_devp->dma_interrupt_handlers[channel].
			    error_handler = NULL;
			mcf_edma_devp->dma_interrupt_handlers[channel].lock =
			    NULL;
#ifdef CONFIG_ARCH_MVF
			slot =
			mcf_edma_devp->dma_interrupt_handlers[channel].slot;
			if (slot >= 54 && slot < 64)
				mvf_dma_mux_pool[slot - 53] = 0;
			else if (slot >= 117 && slot < 128)
				mvf_dma_mux_pool[slot - 64 - 53] = 0;
#endif
		}


		/* make sure error interrupt is disabled */
#if defined(CONFIG_COLDFIRE)
		MCF_EDMA_CEEI = MCF_EDMA_CEEI_CEEI(channel);
#elif defined(CONFIG_ARCH_MVF)
		writeb(0x0, mcf_edma_devp->dmamux_base_addr[channel/16] +
				DMAMUX_CHCFG(channel%16));
		if (channel < 32)
			writeb(channel,
			mcf_edma_devp->dma_base_addr[0] + MCF_EDMA_CEEI);
		else
			writeb(channel - 32,
			mcf_edma_devp->dma_base_addr[1] + MCF_EDMA_CEEI);

#endif
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(mcf_edma_free_channel);

#ifdef CONFIG_ARCH_MVF
/* Starts eDMA transfer on specified channel
 * channel - eDMA TCD number
 */
void mcf_edma_start_transfer(int channel)
{
	void *addr;
	if (channel < 32)
		addr = mcf_edma_devp->dma_base_addr[0];
	else {
		addr = mcf_edma_devp->dma_base_addr[1];
		channel -= 32;
	}

	writeb(channel, addr + MCF_EDMA_SERQ);
	writeb(channel, addr + MCF_EDMA_SSRT);
}
EXPORT_SYMBOL(mcf_edma_start_transfer);

/* Restart eDMA transfer from halfirq
 * channel - eDMA TCD number
 */
void mcf_edma_confirm_halfirq(int channel)
{
	void *addr;
	if (channel < 32)
		addr = mcf_edma_devp->dma_base_addr[0];
	else {
		addr = mcf_edma_devp->dma_base_addr[1];
		channel -= 32;
	}

	/*MCF_EDMA_TCD_CSR(channel) = 7;*/
	writeb(channel, addr + MCF_EDMA_SSRT);
}
EXPORT_SYMBOL(mcf_edma_confirm_halfirq);

/* Starts eDMA transfer on specified channel based on peripheral request
 * channel - eDMA TCD number
 */
void  mcf_edma_enable_transfer(int channel)
{
	void *addr;
	if (channel < 32)
		addr = mcf_edma_devp->dma_base_addr[0];
	else {
		addr = mcf_edma_devp->dma_base_addr[1];
		channel -= 32;
	}

	writeb(channel, addr + MCF_EDMA_SERQ);
}
EXPORT_SYMBOL(mcf_edma_enable_transfer);

/* Stops eDMA transfer
 * channel - eDMA TCD number
 */
void mcf_edma_stop_transfer(int channel)
{
	void *addr;
	if (channel < 32)
		addr = mcf_edma_devp->dma_base_addr[0];
	else {
		addr = mcf_edma_devp->dma_base_addr[1];
		channel -= 32;
	}

	writeb(channel, addr + MCF_EDMA_CINT);
	writeb(channel, addr + MCF_EDMA_CERQ);
}
EXPORT_SYMBOL(mcf_edma_stop_transfer);

/* Confirm that interrupt has been handled
 * channel - eDMA TCD number
 */
void mcf_edma_confirm_interrupt_handled(int channel)
{
	void *addr;
	if (channel < 32)
		addr = mcf_edma_devp->dma_base_addr[0];
	else {
		addr = mcf_edma_devp->dma_base_addr[1];
		channel -= 32;
	}

	writeb(channel, addr + MCF_EDMA_CINT);
}
EXPORT_SYMBOL(mcf_edma_confirm_interrupt_handled);
#endif
/*
 * mcf_edma_cleanup - cleanup driver allocated resources
 */
static void
mcf_edma_cleanup(void)
{
	dev_t devno;
	int i;

	DBG("\n");

	/* disable all error ints */
#if defined(CONFIG_COLDFIRE)
	MCF_EDMA_CEEI = MCF_EDMA_CEEI_CAEE;
	/* free interrupts/memory */
	if (mcf_edma_devp) {
		for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		#if defined(CONFIG_M5445X)
			free_irq(MCF_EDMA_INT0_BASE + i, mcf_edma_devp);
		#elif defined(CONFIG_M5441X)
			if (i >= 0 && i < MCF_EDMA_INT0_END)
				free_irq(MCF_EDMA_INT0_BASE + i, mcf_edma_devp);
			else if (i >= MCF_EDMA_INT0_END &&
				 i <= MCF_EDMA_INT1_END)
				free_irq(MCF_EDMA_INT1_BASE +
				 (i - MCF_EDMA_INT0_END), mcf_edma_devp);
			else if (i >= MCF_EDMA_INT1_END &&
				 i < MCF_EDMA_INT2_END) {
				free_irq(MCF_EDMA_INT2_BASE, mcf_edma_devp);
				break;
			} else {
				ERR("Bad irq number!\n");
				return;
			}
		#endif
		}
	}

		free_irq(MCF_EDMA_INT0_BASE + MCF_EDMA_INT_ERR, mcf_edma_devp);
#elif defined(CONFIG_ARCH_MVF)
	if (mcf_edma_devp) {
		writeb(MCF_EDMA_CEEI_CAEE,
			mcf_edma_devp->dma_base_addr[0] + MCF_EDMA_CEEI);
		writeb(MCF_EDMA_CEEI_CAEE,
			mcf_edma_devp->dma_base_addr[1] + MCF_EDMA_CEEI);
		free_irq(MVF_INT_DMA0_TX, mcf_edma_devp);
		free_irq(MVF_INT_DMA1_TX, mcf_edma_devp);
		free_irq(MVF_INT_DMA0_ERR, mcf_edma_devp);
		free_irq(MVF_INT_DMA1_ERR, mcf_edma_devp);
	}

#endif
	cdev_del(&mcf_edma_devp->cdev);
		kfree(mcf_edma_devp);

	/* unregister character device */
	devno = MKDEV(mcf_edma_major, 0);
	unregister_chrdev_region(devno, 1);
}

/**
 * mcf_edma_dump_channel - dump a channel information
 */
void
mcf_edma_dump_channel(int channel)
{
#if defined(CONFIG_COLDFIRE)
	printk(KERN_DEBUG "EDMA Channel %d\n", channel);
	printk(KERN_DEBUG "  TCD Base     = 0x%x\n",
	       (int)&MCF_EDMA_TCD_SADDR(channel));
	printk(KERN_DEBUG "  SRCADDR      = 0x%lx\n",
	       MCF_EDMA_TCD_SADDR(channel));
	printk(KERN_DEBUG "  SRCOFF       = 0x%x\n",
	       MCF_EDMA_TCD_SOFF(channel));
	printk(KERN_DEBUG "  XFR ATTRIB   = 0x%x\n",
	       MCF_EDMA_TCD_ATTR(channel));
	printk(KERN_DEBUG "  SRCLAST      = 0x%lx\n",
	       MCF_EDMA_TCD_SLAST(channel));
	printk(KERN_DEBUG "  DSTADDR      = 0x%lx\n",
	       MCF_EDMA_TCD_DADDR(channel));
	printk(KERN_DEBUG "  MINOR BCNT   = 0x%lx\n",
	       MCF_EDMA_TCD_NBYTES(channel));
	printk(KERN_DEBUG "  CUR_LOOP_CNT = 0x%x\n",
	       MCF_EDMA_TCD_CITER(channel)&0x1ff);
	printk(KERN_DEBUG "  BEG_LOOP_CNT = 0x%x\n",
	       MCF_EDMA_TCD_BITER(channel)&0x1ff);
	printk(KERN_DEBUG "  STATUS       = 0x%x\n",
	       MCF_EDMA_TCD_CSR(channel));
#endif

}
EXPORT_SYMBOL(mcf_edma_dump_channel);

void mcf_edma_dump_tcd(void *addr , int channel)
{
#if defined(CONFIG_ARCH_MVF)
	printk(KERN_INFO "####DMA_TCD_CSR: 0x%x\n",
			readw(addr + MCF_EDMA_TCD_CSR(channel)));
	printk(KERN_INFO "####TCD_SADDR: 0x%08x\n",
			readl(addr + MCF_EDMA_TCD_SADDR(channel)));
	printk(KERN_INFO "####TCD_DADDR: 0x%08x\n",
			readl(addr + MCF_EDMA_TCD_DADDR(channel)));
	printk(KERN_INFO "####TCD_ATTR: 0x%08x\n",
			readw(addr + MCF_EDMA_TCD_ATTR(channel)));
	printk(KERN_INFO "####TCD_SOFF: 0x%08x\n",
			readw(addr + MCF_EDMA_TCD_SOFF(channel)));
	printk(KERN_INFO "####TCD_NBYTES: 0x%08x\n",
			readl(addr + MCF_EDMA_TCD_NBYTES(channel)));
	printk(KERN_INFO "####TCD_SLAST: 0x%08x\n",
			readl(addr + MCF_EDMA_TCD_SLAST(channel)));
	printk(KERN_INFO "####TCD_CITER: 0x%08x\n",
			readw(addr + MCF_EDMA_TCD_CITER(channel)));
	printk(KERN_INFO "####TCD_BITER: 0x%08x\n",
			readw(addr + MCF_EDMA_TCD_BITER(channel)));
	printk(KERN_INFO "####TCD_DOFF: 0x%08x\n",
			readw(addr + MCF_EDMA_TCD_DOFF(channel)));
	printk(KERN_INFO "####TCD_DLAST_SGA: 0x%08x\n",
			readl(addr + MCF_EDMA_TCD_DLAST_SGA(channel)));
#endif
	return;
}


#ifdef CONFIG_PROC_FS
/*
 * proc file system support
 */

#define FREE_CHANNEL "free"
#define DEVICE_UNKNOWN "device unknown"

/**
 * mcf_edma_proc_show - print out proc info
 * @m: seq_file
 * @v:
 */
static int
mcf_edma_proc_show(struct seq_file *m, void *v)
{
	int i;

	if (mcf_edma_devp == NULL)
		return 0;

	for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		if (mcf_edma_devp->dma_interrupt_handlers[i].allocated) {
			if (mcf_edma_devp->dma_interrupt_handlers[i].device_id)
				seq_printf(m, "%2d: %s\n", i,
					   mcf_edma_devp->
					   dma_interrupt_handlers[i].
					   device_id);
			else
				seq_printf(m, "%2d: %s\n", i, DEVICE_UNKNOWN);
		} else
			seq_printf(m, "%2d: %s\n", i, FREE_CHANNEL);
	}
	return 0;
}

/**
 * mcf_edma_proc_open - open the proc file
 * @inode: inode ptr
 * @file: file ptr
 */
static int
mcf_edma_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mcf_edma_proc_show, NULL);
}

static const struct file_operations mcf_edma_proc_operations = {
	.open = mcf_edma_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * mcf_edma_proc_init - initialize proc filesystem
 */
static int __init
mcf_edma_proc_init(void)
{
	struct proc_dir_entry *e;

	e = create_proc_entry("edma", 0, NULL);
	if (e)
		e->proc_fops = &mcf_edma_proc_operations;

	return 0;
}

#endif

/**
 * mcf_edma_init - eDMA module init
 */
static int __init
mcf_edma_init(void)
{
	dev_t dev;
	int result;
	int i;
#ifdef CONFIG_M54455
	u32 offset;
#endif

#if defined(CONFIG_M5441X)
	/* edma group priority, default grp0 > grp1 > grp2 > grp3 */
	u32 grp0_pri = MCF_EDMA_CR_GRP0PRI(0x00);
	u32 grp1_pri = MCF_EDMA_CR_GRP1PRI(0x01);
	u32 grp2_pri = MCF_EDMA_CR_GRP2PRI(0x02);
	u32 grp3_pri = MCF_EDMA_CR_GRP3PRI(0x03);
#endif

	DBG("Entry\n");

	/* allocate free major number */
	result =
	    alloc_chrdev_region(&dev, EDMA_DEV_MINOR, 1,
				EDMA_DRIVER_NAME);
	if (result < 0) {
		ERR("Error %d can't get major number.\n", result);
		return result;
	}
	mcf_edma_major = MAJOR(dev);

	/* allocate device driver structure */
	mcf_edma_devp = kmalloc(sizeof(struct mcf_edma_dev), GFP_DMA);
	if (!mcf_edma_devp) {
		result = -ENOMEM;
		goto fail;
	}

	mcf_edma_devp->dma_base_addr[0] = MVF_IO_ADDRESS(MVF_DMA0_BASE_ADDR);
	mcf_edma_devp->dma_base_addr[1] = MVF_IO_ADDRESS(MVF_DMA1_BASE_ADDR);
	mcf_edma_devp->dmamux_base_addr[0] =
				MVF_IO_ADDRESS(MVF_DMAMUX0_BASE_ADDR);
	mcf_edma_devp->dmamux_base_addr[1] =
				MVF_IO_ADDRESS(MVF_DMAMUX1_BASE_ADDR);
	mcf_edma_devp->dmamux_base_addr[2] =
				MVF_IO_ADDRESS(MVF_DMAMUX2_BASE_ADDR);
	mcf_edma_devp->dmamux_base_addr[3] =
				MVF_IO_ADDRESS(MVF_DMAMUX3_BASE_ADDR);

	/* init handlers (no handlers for beginning) */
	for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		mcf_edma_devp->dma_interrupt_handlers[i].irq_handler = NULL;
		mcf_edma_devp->dma_interrupt_handlers[i].error_handler = NULL;
		mcf_edma_devp->dma_interrupt_handlers[i].arg = NULL;
		mcf_edma_devp->dma_interrupt_handlers[i].allocated = 0;
		mcf_edma_devp->dma_interrupt_handlers[i].lock = NULL;
		mcf_edma_devp->dma_interrupt_handlers[i].device_id = NULL;
#if defined(CONFIG_ARCH_MVF)
		if (i < 32)
			writew(0x0000, mcf_edma_devp->dma_base_addr[0] +
					MCF_EDMA_TCD_CSR(i));
		else
			writew(0x0000, mcf_edma_devp->dma_base_addr[1] +
					MCF_EDMA_TCD_CSR(i - 32));
#elif defined(CONFIG_COLDFIRE)
		MCF_EDMA_TCD_CSR(i) = 0x0000;
#endif
	}

	/* register char device */
	cdev_init(&mcf_edma_devp->cdev, &mcf_edma_fops);
	mcf_edma_devp->cdev.owner = THIS_MODULE;
	mcf_edma_devp->cdev.ops = &mcf_edma_fops;
	result = cdev_add(&mcf_edma_devp->cdev, dev, 1);
	if (result) {
		ERR("Error %d adding coldfire-dma device.\n", result);
		result = -ENODEV;
		goto fail;
	}

	/* request/enable irq for each eDMA channel */
#if defined(CONFIG_M5445X)
	for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		result = request_irq(MCF_EDMA_INT0_BASE + i,
				     mcf_edma_isr, IRQF_DISABLED,
				     EDMA_DRIVER_NAME, mcf_edma_devp);
		if (result) {
			ERR("Cannot request irq %d\n",
			     (MCF_EDMA_INT0_BASE + i));
			result = -EBUSY;
			goto fail;
		}
	}
#elif defined(CONFIG_M5441X)
	for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		if (i >= 0 && i < MCF_EDMA_INT0_END) {
			result = request_irq(MCF_EDMA_INT0_BASE + i,
					     mcf_edma_isr, IRQF_DISABLED,
					     EDMA_DRIVER_NAME,
					     mcf_edma_devp);

			if (result) {
				ERR("Cannot request irq %d\n",
				     (MCF_EDMA_INT0_BASE + i));
				result = -EBUSY;
				goto fail;
			}
		} else if (i >= MCF_EDMA_INT0_END && i < MCF_EDMA_INT1_END) {
			result = request_irq(MCF_EDMA_INT1_BASE +
					     (i - MCF_EDMA_INT0_END),
					     mcf_edma_isr, IRQF_DISABLED,
					     EDMA_DRIVER_NAME,
					     mcf_edma_devp);

			if (result) {
				ERR("Cannot request irq %d\n",
				    (MCF_EDMA_INT1_BASE +
				    (i - MCF_EDMA_INT0_END)));
				result = -EBUSY;
				goto fail;
			}
		} else if (i >= MCF_EDMA_INT1_END && MCF_EDMA_INT2_END) {
			result = request_irq(MCF_EDMA_INT2_BASE,
					     mcf_edma_isr, IRQF_DISABLED,
					     EDMA_DRIVER_NAME,
					     mcf_edma_devp);
			if (result) {
				ERR("Cannot request irq %d\n",
				    MCF_EDMA_INT2_BASE);
				result = -EBUSY;
				goto fail;
			}
			break;
		} else {
			ERR(" Cannot request irq because of wrong number!\n");
			result = -EBUSY;
			goto fail;
		}
	}
#elif CONFIG_ARCH_MVF
	result = request_irq(MVF_INT_DMA0_TX,
			mcf_edma_isr, IRQF_DISABLED,
			EDMA_DRIVER_NAME,
			mcf_edma_devp);
	if (result) {
		ERR("Cannot request irq %d\n",
				MVF_INT_DMA0_TX);
		result = -EBUSY;
		goto fail;
	}
	result = request_irq(MVF_INT_DMA1_TX,
			mcf_edma_isr, IRQF_DISABLED,
			EDMA_DRIVER_NAME,
			mcf_edma_devp);
	if (result) {
		ERR("Cannot request irq %d\n",
				MVF_INT_DMA1_TX);
		result = -EBUSY;
		goto fail;
	}

#endif
#if defined(CONFIG_COLDFIRE)
	/* request error interrupt */
	result = request_irq(MCF_EDMA_INT0_BASE + MCF_EDMA_INT_ERR,
			     mcf_edma_error_isr, IRQF_DISABLED,
			     EDMA_DRIVER_NAME, mcf_edma_devp);
	if (result) {
		ERR("Cannot request irq %d\n",
		    (MCF_EDMA_INT0_BASE + MCF_EDMA_INT_ERR));
		result = -EBUSY;
		goto fail;
	}
#elif defined(CONFIG_ARCH_MVF)
	result = request_irq(MVF_INT_DMA0_ERR,
			     mcf_edma_error_isr, IRQF_DISABLED,
			     EDMA_DRIVER_NAME, mcf_edma_devp);
	if (result) {
		ERR("Cannot request irq %d\n",
		    (MVF_INT_DMA0_ERR));
		result = -EBUSY;
		goto fail;
	}
	result = request_irq(MVF_INT_DMA1_ERR,
			     mcf_edma_error_isr, IRQF_DISABLED,
			     EDMA_DRIVER_NAME, mcf_edma_devp);
	if (result) {
		ERR("Cannot request irq %d\n",
		    (MVF_INT_DMA1_ERR));
		result = -EBUSY;
		goto fail;
	}

#endif

#if defined(CONFIG_M5445X)
	MCF_EDMA_CR = 0;
#elif defined(CONFIG_M5441X)
	MCF_EDMA_CR = (0 | grp0_pri | grp1_pri | grp2_pri | grp3_pri);
	DBG("MCF_EDMA_CR = %lx\n", MCF_EDMA_CR);
#elif defined(CONFIG_ARCH_MVF)
#if defined(ROUNDROBIN_PRIORITY)
	/* Enable Round Robin Group Arbitration
	 *      * and Round Robin Channel Arbitration */
	edma_cr0 = readl(mcf_edma_devp->dma_base_addr[0] + MCF_EDMA_CR);
	edma_cr1 = readl(mcf_edma_devp->dma_base_addr[1] + MCF_EDMA_CR);
	writel(edma_cr0 | MCF_EDMA_CR_ERCA | MCF_EDMA_CR_ERGA,
			mcf_edma_devp->dma_base_addr[0] + MCF_EDMA_CR);
	writel(edma_cr1 | MCF_EDMA_CR_ERCA | MCF_EDMA_CR_ERGA,
			mcf_edma_devp->dma_base_addr[1] + MCF_EDMA_CR);
#else
	/* group priority: GRP0 > GRP1 > GRP2 > GRP3 */
	writel(MCF_EDMA_CR_GRP0PRI(3)|MCF_EDMA_CR_GRP1PRI(2),
			mcf_edma_devp->dma_base_addr[0] + MCF_EDMA_CR);
	writel(MCF_EDMA_CR_GRP0PRI(1)|MCF_EDMA_CR_GRP1PRI(0),
			mcf_edma_devp->dma_base_addr[1] + MCF_EDMA_CR);
#endif
#endif

#ifdef CONFIG_M54455
	fsl_pata_dma_tcd.pata_tcd_va = (struct TCD *) dma_alloc_coherent(NULL,
			MCF_EDMA_TCD_PER_CHAN + 1,
			&fsl_pata_dma_tcd.pata_tcd_pa,
			GFP_KERNEL);

	if (!fsl_pata_dma_tcd.pata_tcd_va) {
		printk(KERN_INFO "MCF eDMA alllocate tcd memeory failed\n");
		goto fail;
	}


	offset = (fsl_pata_dma_tcd.pata_tcd_pa & (sizeof(struct TCD)-1)) ;
	if (offset) {
		/*
		 * up align the addr to 32B to match the eDMA enginee require,
		 * ie. sizeof tcd boundary
		 * */
		printk(KERN_INFO "pata tcd original:pa-%x[%x]\n",
				fsl_pata_dma_tcd.pata_tcd_pa,
				(u32)fsl_pata_dma_tcd.pata_tcd_va);

		fsl_pata_dma_tcd.pata_tcd_pa += sizeof(struct TCD) - offset;
		fsl_pata_dma_tcd.pata_tcd_va += sizeof(struct TCD) - offset;

		printk(KERN_INFO "pata tcd realigned:pa-%x[%x]\n",
				fsl_pata_dma_tcd.pata_tcd_pa,
				(u32)fsl_pata_dma_tcd.pata_tcd_va);
	}
#endif
#ifdef CONFIG_PROC_FS
	mcf_edma_proc_init();
#endif

	INFO("Initialized successfully\n");
	return 0;
fail:
	mcf_edma_cleanup();
	return result;
}

/**
 * mcf_edma_exit - eDMA module exit
 */
static void __exit
mcf_edma_exit(void)
{
	mcf_edma_cleanup();
}

#ifdef CONFIG_COLDFIRE_EDMA_MODULE
module_init(mcf_edma_init);
module_exit(mcf_edma_exit);
#else
/* get us in early */
postcore_initcall(mcf_edma_init);
#endif

MODULE_DESCRIPTION(MCF_EDMA_DRIVER_INFO);
MODULE_AUTHOR(MCF_EDMA_DRIVER_AUTHOR);
MODULE_LICENSE(MCF_EDMA_DRIVER_LICENSE);
