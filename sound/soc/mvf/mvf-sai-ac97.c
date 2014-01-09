/*
 * sound/soc/mvf/mvf-ac97.c
 *
 * Copyright (C) 2013 Toradex, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <mach/gpio.h>
#include <mach/iomux-mvf.h>
#include <mach/mcf_edma.h>

#include <sound/ac97_codec.h>
#include <sound/initval.h>
#include <sound/core.h>
#include <sound/soc.h>

#include "mvf-sai.h"

#define DRIVER_NAME			"mvf-sai-ac97"
#define EDMA_PRIO_HIGH			6
#define MVF_SAI_AC97_DMABUF_SIZE	(13 * 4)

/* read states */
enum read_state {
	RD_IDLE = 0,
	RD_INIT,	/* command initiated via mvf_sai_ac97_read() call,
			   rd_address valid */
	RD_WAIT,	/* command sent, waiting for reply,
			   rd_address still valid */
	RD_REP_RCV,	/* reply received */
};

/* write states */
enum write_state {
	WR_IDLE = 0,
	WR_INIT,	/* command initiated via mvf_sai_ac97_write() call,
			   rd_address and rd_data valid */
	WR_CMD_SENT,	/* command sent,
			   slot subsequently to be cleared again */
	WR_CMD_CLR,	/* slot cleared */
};

/* AC97 controller */
struct mvf_sai_ac97_info {
	struct platform_device *pdev;
	struct tegra_audio_platform_data *pdata;
	phys_addr_t ac97_phys;
	void __iomem *ac97_base;
	void __iomem *sai0_base;
	struct clk *clk;
	struct snd_card *card;
	struct imx_pcm_dma_params dma_params_rx;
	struct imx_pcm_dma_params dma_params_tx;
	struct snd_dma_buffer rx_buf;
	struct snd_dma_buffer tx_buf;
	int rx_tcd_chan;
	int tx_tcd_chan;
	struct mutex lock;
	enum read_state rd_state;
	unsigned int rd_address;
	unsigned int rd_data;
	enum write_state wr_state;
	unsigned int wr_address;
	unsigned int wr_data;
};

static u64 mvf_pcm_dmamask = DMA_BIT_MASK(32);

/* TODO: not just global */
static struct mvf_sai_ac97_info *info;

/* software AC97 register read access */
static unsigned short mvf_sai_ac97_read(struct snd_ac97 *ac97,
					unsigned short reg)
{
	u32 val;
	int timeout = 100;

	mutex_lock(&info->lock);

	if (info->rd_state != RD_IDLE)
		dev_warn(&info->pdev->dev, "read state machine was %d instead "
				"of RD_IDLE\n", info->rd_state);
	if (info->wr_state != WR_IDLE)
		dev_warn(&info->pdev->dev, "write state machine was %d instead "
				"of WR_IDLE\n", info->wr_state);

	/* Slot 1: Command Address Port
	Bit(19) Read/Write command (1=read, 0=write)
	Bit(18:12) Control Register Index (64 16-bit locations,
			addressed on even byte boundaries)
	Bit(11:0) Reserved (Stuffed with 0’s) */
	info->rd_address = (1 << 19) | /* read */
			(reg << 12);
	info->wr_address = info->rd_address;

	info->rd_state = RD_INIT;
	info->wr_state = WR_INIT;

	/* Slot 2: Status Data Port
	The status data port delivers 16-bit control register read data.
	Bit(19:4) Control Register Read Data (Completed with 0’s if tagged
			“invalid” by AC‘97) */
	do {
		mdelay(1);
		val = (info->rd_data & 0xfffff) >> 4;
	} while ((info->rd_state != RD_REP_RCV) && --timeout);

	info->rd_state = RD_IDLE;

	if (!timeout) {
		dev_warn(&info->pdev->dev,
				"timeout reading register %x\n", reg);
		mutex_unlock(&info->lock);
		return -ETIMEDOUT;
	}

	mutex_unlock(&info->lock);

	pr_debug("%s: 0x%02x 0x%04x\n", __func__, reg, val);

	return val;
}

/* software AC97 register write access */
static void mvf_sai_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
			     unsigned short val)
{
	int timeout = 100;

	pr_debug("%s: 0x%02x 0x%04x\n", __func__, reg, val);

	if (info->wr_state != WR_IDLE)
		dev_warn(&info->pdev->dev, "write state machine was %d instead "
				"of WR_IDLE\n", info->wr_state);

	mutex_lock(&info->lock);

	/* Slot 1: Command Address Port
	Bit(19) Read/Write command (1=read, 0=write)
	Bit(18:12) Control Register Index (64 16-bit locations,
			addressed on even byte boundaries)
	Bit(11:0) Reserved (Stuffed with 0’s) */
	info->wr_address = reg << 12;

	/* Slot 2: Command Data Port
	The command data port is used to deliver 16-bit control register write
			data in the event that the current command
	port operation is a write cycle. (as indicated by Slot 1, bit 19)
	Bit(19:4) Control Register Write Data (Completed with 0’s if current
			operation is a read)
	Bit(3:0) Reserved (Completed with 0’s) */
	info->wr_data = (val << 4);

	info->wr_state = WR_INIT;

	while ((info->wr_state != WR_CMD_CLR) && --timeout)
		mdelay(1);

	info->wr_state = WR_IDLE;

	if (!timeout)
		dev_warn(&info->pdev->dev, "timeout writing register %x\n", reg);

	mutex_unlock(&info->lock);
}

struct snd_ac97_bus_ops soc_ac97_ops = {
	.read	= mvf_sai_ac97_read,
	.write	= mvf_sai_ac97_write,
};
EXPORT_SYMBOL_GPL(soc_ac97_ops);

static struct snd_ac97_bus_ops mvf_sai_ac97_ops = {
	.read	= mvf_sai_ac97_read,
	.write	= mvf_sai_ac97_write,
};

static struct snd_ac97 *mvf_sai_ac97_ac97;

static __initdata iomux_v3_cfg_t ac97_pinmux[] = {
	/* SAI2: AC97/Touchscreen */
	MVF600_PAD4_PTA11_WM9715L_PENDOWN,	/* carefull also used for JTAG
						   JTMS/SWDIO */
	MVF600_PAD6_PTA16_SAI2_TX_BCLK,		/* AC97_BIT_CLK */
	MVF600_PAD8_PTA18_SAI2_TX_DATA,		/* AC97_SDATA_OUT */
	MVF600_PAD9_PTA19_SAI2_TX_SYNC,		/* AC97_SYNC */
	MVF600_PAD12_PTA22_SAI2_RX_DATA,	/* AC97_SDATA_IN */
	MVF600_PAD13_PTA23_WM9715L_RESET,
	MVF600_PAD24_PTB2_WM9715L_GENIRQ,
	MVF600_PAD40_PTB18_CKO1,		/* AC97_MCLK fed back in from
						   camera clock pin */
	MVF600_PAD93_PTB23_SAI0_TX_BCLK,	/* AC97_MCLK */
};

static irqreturn_t mvf_sai_ac97_dma_irq(int channel, void *data)
{
	if (channel == info->rx_tcd_chan) {
		if ((info->rd_state == RD_WAIT) &&
		    ((*((unsigned int *)(info->rx_buf.area + 4)) & (3 << (13 +
		     4))) == (3 << (13 + 4))) && /* valid slot 1 & 2 */
		    ((*((unsigned int *)(info->rx_buf.area + 8)) | (1 << 19)) ==
		     info->rd_address)) {
			info->rd_data =	*((unsigned int *)(info->rx_buf.area
					+ 12));
			info->rd_state = RD_REP_RCV;
		}

		mcf_edma_confirm_interrupt_handled(DMA_MUX_SAI2_RX);
	} else if (channel == info->tx_tcd_chan) {
		if (info->wr_state == WR_INIT) {
			/* Slot 0: TAG
			Bit 15 Codec Ready
			Bit 14:3 Slot Valid (Which of slot 1 to slot 12 contain
					valid data)
			Bit 2:0 Zero */
			*((unsigned int *)(info->tx_buf.area + 0)) =
					(1 << (15 + 4)) | /* valid frame */
					(1 << (14 + 4)) | /* slot 1 valid */
					(((info->wr_address & (1 << 19))?0:1) <<
					 (13 + 4)); /* slot 2 valid */

			/* Slot 1: Command Address Port
			Bit(19) Read/Write command (1=read, 0=write)
			Bit(18:12) Control Register Index (64 16-bit locations,
					addressed on even byte boundaries)
			Bit(11:0) Reserved (Stuffed with 0’s) */
			*((unsigned int *)(info->tx_buf.area + 4)) =
					info->wr_address;

			if (!(info->wr_address & (1 << 19))) {
				/* Slot 2: Command Data Port
				The command data port is used to deliver 16-bit
				control register write data in the event that
				the current command port operation is a write
				cycle. (as indicated by Slot 1, bit 19)
				Bit(19:4) Control Register Write Data (Completed
						with 0’s if current operation is
						a read)
				Bit(3:0) Reserved (Completed with 0’s) */
				*((unsigned int *)(info->tx_buf.area + 8)) =
						info->wr_data;
				info->wr_state = WR_CMD_CLR;
			} else {
				info->rd_state = RD_WAIT;
				info->wr_state = WR_IDLE;
			}
		}

		mcf_edma_confirm_interrupt_handled(DMA_MUX_SAI2_TX);
	}

	return IRQ_HANDLED;
}

static int __devinit mvf_sai_ac97_dev_probe(struct platform_device *pdev)
{
	struct snd_ac97_bus *ac97_bus;
	struct snd_ac97_template ac97_template;
	int err = 0;
	int gpio_status;
	int i;
	unsigned int rcsr;
	unsigned int reg;
	struct resource *res;
	unsigned int tcsr;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto failed_alloc;
	}
	dev_set_drvdata(&pdev->dev, info);
	info->pdev = pdev;

	mutex_init(&info->lock);

/*
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		ret = -ENODEV;
		goto failed_irq;
	}
	info->irq = res->start;
*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource!\n");
		err = -ENODEV;
		goto failed_mem;
	}

	if (!request_mem_region(res->start, resource_size(res), DRV_NAME)) {
		dev_err(&pdev->dev, "request_mem_region failed\n");
		err = -EBUSY;
		goto failed_region;
	}

	info->ac97_phys = res->start;
	info->ac97_base = ioremap(res->start, resource_size(res));
	if (!info->ac97_base) {
		dev_err(&pdev->dev, "cannot remap iomem!\n");
		err = -ENOMEM;
		goto failed_map;
	}

	/* SAI0_TX_BCLK used as AC97 master clock */
	info->sai0_base = ioremap(MVF_SAI0_BASE_ADDR, resource_size(res));
	if (!info->sai0_base) {
		dev_err(&pdev->dev, "cannot remap iomem!\n");
		err = -ENOMEM;
		goto failed_map2;
	}

	/* configure AC97 master clock */
	reg = readl(info->sai0_base + SAI_TCR2);
	reg &= ~SAI_TCR2_SYNC_MASK;	/* asynchronous aka independent
					   operation */
	reg &= ~SAI_TCR2_BCS;		/* bit clock not swapped */
	reg &= ~SAI_TCR2_MSEL_MASK;
	reg |= SAI_TCR2_MSEL_MCLK1;	/* Clock selected by
					   CCM_CSCMR1[SAIn_CLK_SEL] */
	reg |= SAI_TCR2_BCD_MSTR;	/* Bitclock is generated internally
					   (master mode) */
	reg &= ~SAI_TCR2_DIV_MASK;	/* Divides down the audio master */
	reg |= SAI_TCR2_DIV(2);		/* clock by 6 to generate the bitclock
					   when configured for an internal
					   bitclock (master). */
	writel(reg, info->sai0_base + SAI_TCR2);

	/* enable AC97 master clock */
	reg = readl(info->sai0_base + SAI_TCSR);
	writel(reg | SAI_TCSR_BCE, info->sai0_base + SAI_TCSR);

	info->clk = clk_get(&pdev->dev, "sai_clk");
	if (IS_ERR(info->clk)) {
		err = PTR_ERR(info->clk);
		dev_err(&pdev->dev, "Cannot get the clock: %d\n",
			err);
		goto failed_clk;
	}
	clk_enable(info->clk);

	err = snd_card_create(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
			      THIS_MODULE, 0, &info->card);
	if (err < 0)
		goto failed_create;

	info->card->dev = &pdev->dev;
	strncpy(info->card->driver, pdev->dev.driver->name, sizeof(info->card->driver));

	/* AC'97 controller required to drive and keep SYNC and SDATA_OUT low
	   to avoid wolfson entering test mode */
#define GPIO_AC97_SDATAOUT	8
	gpio_status = gpio_request(GPIO_AC97_SDATAOUT, "WOLFSON_SDATAOUT");
	if (gpio_status < 0) {
		pr_info("WOLFSON_SDATAOUT request GPIO FAILED\n");
		WARN_ON(1);
	}
	gpio_status = gpio_direction_output(GPIO_AC97_SDATAOUT, 0);
	if (gpio_status < 0) {
		pr_info("WOLFSON_SDATAOU request GPIO DIRECTION FAILED\n");
		WARN_ON(1);
	}
	udelay(2);

	/* do wolfson hard reset */
#define GPIO_AC97_nRESET	13
	gpio_status = gpio_request(GPIO_AC97_nRESET, "WOLFSON_RESET");
	if (gpio_status < 0) {
		pr_info("WOLFSON_RESET request GPIO FAILED\n");
		WARN_ON(1);
	}

	/* Wolfson initially powered up disabled due to ADCIRQ aka PWRUP
	   strapping pin being held high.
	   WM9715L awakes from sleep mode on warm reset of AC-Link
	   (according to the AC’97 specification). */

	/* do wolfson warm reset by toggling SYNC */
#define GPIO_AC97_SYNC	9
	gpio_status = gpio_request(GPIO_AC97_SYNC, "WOLFSON_SYNC");
	if (gpio_status < 0) {
		pr_info("WOLFSON_SYNC request GPIO FAILED\n");
		WARN_ON(1);
	}
	gpio_status = gpio_direction_output(GPIO_AC97_SYNC, 0);
	if (gpio_status < 0) {
		pr_info("WOLFSON_SYNC request GPIO DIRECTION FAILED\n");
		WARN_ON(1);
	}
	udelay(2);
	gpio_status = gpio_direction_output(GPIO_AC97_nRESET, 0);
	if (gpio_status < 0) {
		pr_info("WOLFSON_RESET request GPIO DIRECTION FAILED\n");
		WARN_ON(1);
	}
	udelay(2);
	gpio_set_value(GPIO_AC97_nRESET, 1);
	udelay(2);
	gpio_set_value(GPIO_AC97_SYNC, 1);
	udelay(2);
	gpio_set_value(GPIO_AC97_SYNC, 0);
	udelay(2);
	gpio_free(GPIO_AC97_SYNC);

	mxc_iomux_v3_setup_multiple_pads(ac97_pinmux, ARRAY_SIZE(ac97_pinmux));

	/* clear transmit/receive configuration/status registers */
	writel(0x0, info->ac97_base + SAI_TCSR);
	writel(0x0, info->ac97_base + SAI_RCSR);

	info->dma_params_tx.dma_addr = res->start + SAI_TDR;
	info->dma_params_rx.dma_addr = res->start + SAI_RDR;

	/* 32 deep FIFOs */
	info->dma_params_tx.burstsize = 16;
	info->dma_params_rx.burstsize = 16;

	res = platform_get_resource_byname(pdev, IORESOURCE_DMA, "tx0");
	if (res)
		info->dma_params_tx.dma = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_DMA, "rx0");
	if (res)
		info->dma_params_rx.dma = res->start;

	platform_set_drvdata(pdev, info);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &mvf_pcm_dmamask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	/* pre allocate DMA buffers */
	size_t size = MVF_SAI_AC97_DMABUF_SIZE;

	info->tx_buf.dev.type = SNDRV_DMA_TYPE_DEV;
	info->tx_buf.dev.dev = &pdev->dev;
	info->tx_buf.private_data = NULL;
	info->tx_buf.area = dma_alloc_writecombine(&pdev->dev, size,
					   &info->tx_buf.addr, GFP_KERNEL);
	if (!info->tx_buf.area) {
		err = -ENOMEM;
		goto failed_tx_buf;
	}
	info->tx_buf.bytes = size;

	info->rx_buf.dev.type = SNDRV_DMA_TYPE_DEV;
	info->rx_buf.dev.dev = &pdev->dev;
	info->rx_buf.private_data = NULL;
	info->rx_buf.area = dma_alloc_writecombine(&pdev->dev, size,
					   &info->rx_buf.addr, GFP_KERNEL);
	if (!info->rx_buf.area) {
		err = -ENOMEM;
		goto failed_rx_buf;
	}
	info->rx_buf.bytes = size;

	memset(info->tx_buf.area, 0, MVF_SAI_AC97_DMABUF_SIZE);
	memset(info->rx_buf.area, 0, MVF_SAI_AC97_DMABUF_SIZE);

	/* 1. Configuration of SAI clock mode */

	/* Issue software reset and FIFO reset for Transmitter and Receiver
	   sections before starting configuration. */
	reg = readl(info->ac97_base + SAI_TCSR);
	writel(reg | SAI_TCSR_SR, info->ac97_base + SAI_TCSR);	/* Issue
							software reset */
	udelay(2);
	writel(reg & ~SAI_TCSR_SR, info->ac97_base + SAI_TCSR);	/* Release
							software reset */
	writel(reg | SAI_TCSR_FR, info->ac97_base + SAI_TCSR);	/* FIFO reset */
	reg = readl(info->ac97_base + SAI_RCSR);
	writel(reg | SAI_RCSR_SR, info->ac97_base + SAI_RCSR);	/* Issue
							software reset */
	udelay(2);
	writel(reg & ~SAI_RCSR_SR, info->ac97_base + SAI_RCSR);	/* Release
							software reset */
	writel(reg | SAI_RCSR_FR, info->ac97_base + SAI_RCSR);	/* FIFO reset */

	/* Configure FIFO watermark. FIFO watermark is used as an indicator for
	   DMA trigger when read or write data from/to FIFOs. */
	/* Watermark level for all enabled transmit channels of one SAI module.
	 */
	writel(info->dma_params_tx.burstsize, info->ac97_base + SAI_TCR1);
	writel(info->dma_params_rx.burstsize, info->ac97_base + SAI_RCR1);

	/* Configure the clocking mode, bitclock polarity, direction, and
	   divider. Clocking mode defines synchronous or asynchronous operation
	   for SAI module. Bitclock polarity configures polarity of the
	   bitclock. Bitclock direction configures direction of the bitclock.
	   Bus master has bitclock generated externally, slave has bitclock
	   generated internally */
	reg = readl(info->ac97_base + SAI_TCR2);
	reg &= ~SAI_TCR2_SYNC_MASK;	/* The transmitter must be configured
				for asynchronous operation and the receiver for
				synchronous operation. */
	reg &= ~SAI_TCR2_BCS;	/* bit clock not swapped */
	reg &= ~SAI_TCR2_BCP;	/* Bitclock is active high (drive outputs on
				rising edge and sample inputs on falling edge */
	reg &= ~SAI_TCR2_BCD_MSTR;	/* Bitclock is generated externally
					   (Slave mode) */
	writel(reg, info->ac97_base + SAI_TCR2);
	reg = readl(info->ac97_base + SAI_RCR2);
	reg &= ~SAI_RCR2_SYNC_MASK;	/* The transmitter must be configured
				for asynchronous operation and the receiver */
	reg |= SAI_RCR2_SYNC;	/* for synchronous operation. */
	reg &= ~SAI_RCR2_BCS;	/* bit clock not swapped */
	reg &= ~SAI_RCR2_BCP;	/* Bitclock is active high (drive outputs on
				rising edge and sample inputs on falling edge */
	reg &= ~SAI_RCR2_BCD_MSTR;	/* Bitclock is generated externally
					   (Slave mode) */
	writel(reg, info->ac97_base + SAI_RCR2);

	/* Configure frame size, frame sync width, MSB first, frame sync early,
	   polarity, and direction
	   Frame size – configures the number of words in each frame. AC97
	   requires 13 words per frame.
	   Frame sync width – configures the length of the frame sync in number
	   of bitclock. The sync width cannot be longer than the first word of
	   the frame. AC97 requires frame sync asserted for first word. */
	reg = readl(info->ac97_base + SAI_TCR4);
	reg &= ~SAI_TCR4_FRSZ_MASK;	/* Configures number of words in each */
	reg |= SAI_TCR4_FRSZ(12);	/* frame. The value written should be
			one less than the number of words in the frame. */
	reg &= ~SAI_TCR4_SYWD_MASK;	/* Configures length of the frame */
	reg |= SAI_TCR4_SYWD(15);	/* sync. The value written should be one
					   less than the number of bitclocks.
				AC97 - 16 bits transmitted in first word. */
	reg |= SAI_TCR4_MF;	/* MSB is transmitted first */
	reg |= SAI_TCR4_FSE;	/* Frame sync asserted one bit before the first
				   bit of the frame */
	reg &= ~SAI_TCR4_FSP;	/* A new AC-link input frame begins with a low
				   to high transition of SYNC.
				   Frame sync is active high */
	reg |= SAI_TCR4_FSD_MSTR;	/* Frame sync is generated internally
					   (Master mode) */
	writel(reg, info->ac97_base + SAI_TCR4);
	reg = readl(info->ac97_base + SAI_RCR4);
	reg &= ~SAI_RCR4_FRSZ_MASK;	/* Configures number of words in each */
	reg |= SAI_RCR4_FRSZ(12);	/* frame. The value written should be
			one less than the number of words in the frame. */
	reg &= ~SAI_RCR4_SYWD_MASK;	/* Configures length of the frame */
	reg |= SAI_RCR4_SYWD(15);	/* sync. The value written should be one
					   less than the number of bitclocks.
				AC97 - 16 bits transmitted in first word. */
	reg |= SAI_RCR4_MF;	/* MSB is transmitted first */
	reg |= SAI_RCR4_FSE;	/* Frame sync asserted one bit before the first
				   bit of the frame */
	reg &= ~SAI_RCR4_FSP;	/* A new AC-link input frame begins with a low
				   to high transition of SYNC.
				   Frame sync is active high */
	reg |= SAI_RCR4_FSD_MSTR;	/* Frame sync is generated internally
					   (Master mode) */
	writel(reg, info->ac97_base + SAI_RCR4);

	/* Configure the Word 0 and next word sizes.
	   W0W – defines number of bits in the first word in each frame.
	   WNW – defines number of bits in each word for each word except the
	   first in the frame. */
	reg = readl(info->ac97_base + SAI_TCR5);
	reg &= ~SAI_TCR5_W0W_MASK;	/* Number of bits in first word in */
	reg |= SAI_TCR5_W0W(15);	/* each frame. AC97 – 16-bit word is
					   transmitted. */
	reg &= ~SAI_TCR5_WNW_MASK;	/* Number of bits in each word in */
	reg |= SAI_TCR5_WNW(19);	/* each frame. AC97 – 20-bit word is
					   transmitted. */
	reg &= ~SAI_TCR5_FBT_MASK;	/* Configures the bit index for the
					   first bit transmitted for each word
					   in the frame. */
	reg |= SAI_TCR5_FBT(19);	/* The value written must be greater
					   than or equal to the word width when
					   configured for MSB First. */
	writel(reg, info->ac97_base + SAI_TCR5);
	reg = readl(info->ac97_base + SAI_RCR5);
	reg &= ~SAI_RCR5_W0W_MASK;	/* Number of bits in first word in */
	reg |= SAI_RCR5_W0W(15);	/* each frame. AC97 – 16-bit word is
					   transmitted. */
	reg &= ~SAI_RCR5_WNW_MASK;	/* Number of bits in each word in */
	reg |= SAI_RCR5_WNW(19);	/* each frame. AC97 – 20-bit word is
					   transmitted. */
	reg &= ~SAI_RCR5_FBT_MASK;	/* Configures the bit index for the
					   first bit transmitted for each word
					   in the frame. */
	reg |= SAI_RCR5_FBT(19);	/* The value written must be greater
					   than or equal to the word width when
					   configured for MSB First. */
	writel(reg, info->ac97_base + SAI_RCR5);

	/* Clear the Transmit and Receive Mask registers. */
	writel(0, info->ac97_base + SAI_TMR);	/* Enable or mask word N in the
						   frame. */
	writel(0, info->ac97_base + SAI_RMR);	/* Enable or mask word N in the
						   frame. */

	/**
	 * mcf_edma_request_channel - Request an eDMA channel
	 * @channel: channel number. In case it is equal to EDMA_CHANNEL_ANY
	 *              it will be allocated a first free eDMA channel.
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
	err = mcf_edma_request_channel(DMA_MUX_SAI2_TX, mvf_sai_ac97_dma_irq,
			NULL, EDMA_PRIO_HIGH, NULL, NULL, DRIVER_NAME);
	if (err < 0)
		goto failed_request_tx_dma;
	info->tx_tcd_chan = err;

	err = mcf_edma_request_channel(DMA_MUX_SAI2_RX, mvf_sai_ac97_dma_irq,
			NULL, EDMA_PRIO_HIGH, NULL, NULL, DRIVER_NAME);
	if (err < 0)
		goto failed_request_rx_dma;
	info->rx_tcd_chan = err;

	/* Setup transfer control descriptor (TCD)
	 *   channel - descriptor number
	 *   source  - source address
	 *   dest    - destination address
	 *   attr    - attributes
	 *   soff    - source offset
	 *   nbytes  - number of bytes to be transfered in minor loop
	 *   slast   - last source address adjustment
	 *   citer   - major loop count
	 *   biter   - begining minor loop count
	 *   doff    - destination offset
	 *   dlast_sga - last destination address adjustment
	 *   major_int - generate interrupt after each major loop
	 *   disable_req - disable DMA request after major loop
	 *   enable_sg - enable scatter/gather processing
	 */
	mcf_edma_set_tcd_params(info->tx_tcd_chan, info->tx_buf.addr,
			info->dma_params_tx.dma_addr,
			MCF_EDMA_TCD_ATTR_SSIZE_32BIT |
			MCF_EDMA_TCD_ATTR_DSIZE_32BIT, 4, 4, -13 * 4,
			13, 13, 0, 0, 1, 0, 0);
	mcf_edma_set_tcd_params(info->rx_tcd_chan, info->dma_params_rx.dma_addr,
			info->rx_buf.addr,
			MCF_EDMA_TCD_ATTR_SSIZE_32BIT |
			MCF_EDMA_TCD_ATTR_DSIZE_32BIT, 0, 4, 0,
			13, 13, 4, -13 * 4, 1, 0, 0);

	reg = readl(info->ac97_base + SAI_TCR3);
	reg |= SAI_TCR3_TCE;	/* Enables a data channel for a transmit
				   operation. */
	writel(reg, info->ac97_base + SAI_TCR3);
	reg = readl(info->ac97_base + SAI_RCR3);
	reg |= SAI_RCR3_RCE;	/* Enables a data channel for a receive
				   operation. */
	writel(reg, info->ac97_base + SAI_RCR3);

	mcf_edma_start_transfer(info->tx_tcd_chan);
	mcf_edma_start_transfer(info->rx_tcd_chan);

	tcsr = readl(info->ac97_base + SAI_TCSR);
	rcsr = readl(info->ac97_base + SAI_RCSR);

	/* enable transmit DMA */
	tcsr |= SAI_TCSR_FRDE;

	/* enable receive DMA */
	rcsr |= SAI_RCSR_FRDE;

	/* enable transmit/receive */
	tcsr |= SAI_TCSR_TE;
	rcsr |= SAI_RCSR_RE;

	info->rd_state = RD_IDLE;
	info->wr_state = WR_IDLE;

	/* In synchronous mode, receiver is enabled only when both transmitter
	   and receiver are enabled. It is recommended that transmitter is
	   enabled last and disabled first. */
	writel(rcsr, info->ac97_base + SAI_RCSR);
	writel(tcsr, info->ac97_base + SAI_TCSR);

	err = snd_ac97_bus(info->card, 0, &mvf_sai_ac97_ops, NULL, &ac97_bus);
	if (err)
		goto failed_bus;
	memset(&ac97_template, 0, sizeof(ac97_template));
	err = snd_ac97_mixer(ac97_bus, &ac97_template, &mvf_sai_ac97_ac97);
	if (err)
		goto failed_mixer;

	snprintf(info->card->shortname, sizeof(info->card->shortname), "%s",
		 snd_ac97_get_short_name(mvf_sai_ac97_ac97));
	snprintf(info->card->longname, sizeof(info->card->longname), "%s (%s)",
		 pdev->dev.driver->name, info->card->mixername);

	snd_card_set_dev(info->card, &pdev->dev);
	err = snd_card_register(info->card);
	if (err == 0) {
		platform_set_drvdata(pdev, info->card);
		return 0;
	}

failed:
	if (info->card)
		snd_card_disconnect(info->card);
failed_mixer:
failed_bus:
	/* disable transmit/receive and respective DMAs */
        tcsr = readl(info->ac97_base + SAI_TCSR);
        rcsr = readl(info->ac97_base + SAI_RCSR);
        tcsr &= ~SAI_TCSR_FRDE;
        rcsr &= ~SAI_RCSR_FRDE;
        tcsr &= ~SAI_TCSR_TE;
        rcsr &= ~SAI_RCSR_RE;
        writel(rcsr, info->ac97_base + SAI_RCSR);
        writel(tcsr, info->ac97_base + SAI_TCSR);

	mcf_edma_free_channel(DMA_MUX_SAI2_RX, NULL);
failed_request_rx_dma:
	mcf_edma_free_channel(DMA_MUX_SAI2_TX, NULL);
failed_request_tx_dma:
	dma_free_writecombine(&pdev->dev, MVF_SAI_AC97_DMABUF_SIZE,
			info->rx_buf.area, info->rx_buf.addr);
failed_rx_buf:
	dma_free_writecombine(&pdev->dev, MVF_SAI_AC97_DMABUF_SIZE,
			info->tx_buf.area, info->tx_buf.addr);
failed_tx_buf:
//iomux
	snd_card_free(info->card);
failed_create:
	clk_disable(info->clk);
	clk_put(info->clk);

	/* disable AC97 master clock */
	reg = readl(info->sai0_base + SAI_TCSR);
	writel(reg & ~SAI_TCSR_BCE, info->sai0_base + SAI_TCSR);
failed_clk:
	iounmap((volatile void *)MVF_SAI0_BASE_ADDR);
failed_map2:
	iounmap(info->ac97_base);
failed_map:
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) release_mem_region(res->start, resource_size(res));
failed_region:
failed_mem:
//	free_irq(info->irq, NULL);
failed_irq:
	kfree(info);
failed_alloc:
	return err;
}

static int __devexit mvf_sai_ac97_dev_remove(struct platform_device *pdev)
{
	unsigned int reg;
	unsigned int rcsr;
	struct resource *res;
	unsigned int tcsr;

	snd_card_disconnect(info->card);

	/* disable transmit/receive and respective DMAs */
        tcsr = readl(info->ac97_base + SAI_TCSR);
        rcsr = readl(info->ac97_base + SAI_RCSR);
        tcsr &= ~SAI_TCSR_FRDE;
        rcsr &= ~SAI_RCSR_FRDE;
        tcsr &= ~SAI_TCSR_TE;
        rcsr &= ~SAI_RCSR_RE;
        writel(rcsr, info->ac97_base + SAI_RCSR);
        writel(tcsr, info->ac97_base + SAI_TCSR);

	mcf_edma_free_channel(DMA_MUX_SAI2_RX, NULL);
	mcf_edma_free_channel(DMA_MUX_SAI2_TX, NULL);
	dma_free_writecombine(&pdev->dev, MVF_SAI_AC97_DMABUF_SIZE, info->rx_buf.area, info->rx_buf.addr);
	dma_free_writecombine(&pdev->dev, MVF_SAI_AC97_DMABUF_SIZE, info->tx_buf.area, info->tx_buf.addr);
//iomux
	snd_card_free(info->card);
	clk_disable(info->clk);
	clk_put(info->clk);

	/* disable AC97 master clock */
	reg = readl(info->sai0_base + SAI_TCSR);
	writel(reg & ~SAI_TCSR_BCE, info->sai0_base + SAI_TCSR);

	iounmap((volatile void *)MVF_SAI0_BASE_ADDR);
	iounmap(info->ac97_base);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) release_mem_region(res->start, resource_size(res));
//	free_irq(info->irq, NULL);
	kfree(info);

	return 0;
}

static struct platform_driver mvf_sai_ac97_driver = {
	.probe	= mvf_sai_ac97_dev_probe,
	.remove	= __devexit_p(mvf_sai_ac97_dev_remove),
	.driver	= {
		.name	= "mvf-sai",
		.owner	= THIS_MODULE,
	},
};

static int __init mvf_sai_ac97_init(void)
{
	return platform_driver_register(&mvf_sai_ac97_driver);
}
module_init(mvf_sai_ac97_init);

static void __exit mvf_sai_ac97_exit(void)
{
	platform_driver_unregister(&mvf_sai_ac97_driver);
}
module_exit(mvf_sai_ac97_exit);

MODULE_AUTHOR("Marcel Ziswiler");
MODULE_DESCRIPTION("Software AC97 driver for the SAI of the Vybrid");
MODULE_LICENSE("GPL");
