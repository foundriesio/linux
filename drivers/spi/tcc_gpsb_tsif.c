// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/poll.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <asm/dma.h>
#include <linux/dmaengine.h>

#include <linux/spi/tcc_tsif.h>
#include "tca_spi.h"

#include <linux/cdev.h>

#define tcc_tsif_writel	__raw_writel
#define tcc_tsif_readl	__raw_readl

/* tcc_tsif_devices global variables *****************************************/
struct class *tsif_class;
EXPORT_SYMBOL(tsif_class);

uint32_t tsif_major_num;
EXPORT_SYMBOL(tsif_major_num);

int32_t gpsb_tsif_num;
EXPORT_SYMBOL(gpsb_tsif_num);

int32_t *pTsif_mode;
EXPORT_SYMBOL(pTsif_mode);

static struct cdev tsif_device_cdev;
#define MAX_SUPPORT_TSIF_DEVICE 8
static int32_t tsif_mode[MAX_SUPPORT_TSIF_DEVICE] = {0, };

/* tcc_tsif_devices global variables *****************************************/

/* define USE_STATIC_DMA_BUFFER */
#define iTotalDriverHandle 2
#define ALLOC_DMA_SIZE 0x100000

static struct tea_dma_buf *g_static_dma;

static struct clk *gpsb_hclk[iTotalDriverHandle];
static struct clk *gpsb_pclk[iTotalDriverHandle];
struct fo {
	int32_t minor;
};
static struct fo fodevs[iTotalDriverHandle];
static struct fo *fodp;

#define	MAX_PCR_CNT 2
#define MAX_GPSB_PORT_NUM 0x3F

struct tca_spi_pri_handle {
	wait_queue_head_t wait_q;
	struct mutex mutex;
	int32_t open_cnt;
#ifdef TCC_USE_GFB_PORT
	uint32_t gpsb_port[4];
#else
	u32 gpsb_port;
#endif
	u32 gpsb_channel;
	void __iomem *reg_base;
	resource_size_t phy_reg_base;
	void __iomem *port_reg;
	void __iomem *pid_reg;
	void __iomem *ac_reg;
	u32 drv_major_num;
	u32 pcr_pid[MAX_PCR_CNT];
	u32 bus_num;
	u32 irq_no;
	u32 is_suspend;  /* 0:not in suspend, 1:in suspend */
	u32 packet_read_count;
	const char *name;

	/* GDMA Configuration */
	/* DMA-engine specific */
	struct tcc_spi_dma	dma;
	uint32_t gdma_use;
	struct device *dev;

	/* SRAM Specific */
	uint32_t sram_use;
	uint32_t sram_addr;
	uint32_t sram_size;

	/* Use for normal slave not broadcast */
	bool normal_slave;
};

static struct tca_spi_port_config port_cfg[iTotalDriverHandle];
static struct tca_spi_handle tsif_handle[iTotalDriverHandle];
static struct tca_spi_pri_handle tsif_pri[iTotalDriverHandle];

static int32_t tcc_gpsb_tsif_init(int32_t id);
static void tcc_gpsb_tsif_deinit(int32_t id);

static int32_t tcc_gpsb_tsif_probe(struct platform_device *pdev);
static int32_t tcc_gpsb_tsif_remove(struct platform_device *pdev);
static int32_t tcc_gpsb_tsif_open(struct inode *inode, struct file *filp);
static int32_t tcc_gpsb_tsif_release(struct inode *inode, struct file *filp);
static long tcc_gpsb_tsif_ioctl(struct file *filp, uint32_t cmd,
				ulong arg);
static ssize_t tcc_gpsb_tsif_read(struct file *filp, char *buf, size_t len,
				loff_t *ppos);
static ssize_t tcc_gpsb_tsif_write(struct file *filp, const char *buf,
					size_t len, loff_t *ppos);
static uint32_t tcc_gpsb_tsif_poll(struct file *filp,
					struct poll_table_struct *wait);

static const struct file_operations tcc_gpsb_tsif_fops = {
	.owner          = THIS_MODULE,
	.read           = tcc_gpsb_tsif_read,
	.write          = tcc_gpsb_tsif_write,
	.unlocked_ioctl = tcc_gpsb_tsif_ioctl,
	.compat_ioctl	= tcc_gpsb_tsif_ioctl,
	.open           = tcc_gpsb_tsif_open,
	.release        = tcc_gpsb_tsif_release,
	.poll           = tcc_gpsb_tsif_poll,
};

/* External Decoder: Send packet to external kernel ts demuxer */
struct tsdemux_extern {
	/* 0:don't use external demuxer, 1:use external decoder */
	int32_t is_active;
	int32_t index;
	int32_t call_decoder_index;
	int32_t (*tsdemux_decoder)(char *p1, int32_t p1_size,
			char *p2, int32_t p2_size, int32_t devid);
};

static struct tsdemux_extern tsdemux_extern_handle[iTotalDriverHandle];
static int32_t tsif_get_readable_cnt(struct tca_spi_handle *H);

static ssize_t show_port(struct device *dev, struct device_attribute *attr,
			char *buf)
{

#ifdef TCC_USE_GFB_PORT
	return sprintf(buf, "SDO: %d, SDI: %d, SCLK: %d, SFRM: %d\n",
			tsif_pri[0].gpsb_port[0], tsif_pri[0].gpsb_port[1],
			tsif_pri[0].gpsb_port[2], tsif_pri[0].gpsb_port[3]);
#else
	return sprintf(buf, "gpsb port : %d\n", tsif_pri[0].gpsb_port);
#endif

}
static ssize_t store_port(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
#ifdef TCC_USE_GFB_PORT
	pr_warn("[WARN][SPI] Does not support set port due to using GFB port\n");
	return (ssize_t)count;
#else
	int32_t ret;
	ulong port;

	ret = kstrtoul(buf, 16, &port);
	if ((ret != 0) || (port > (ulong)MAX_GPSB_PORT_NUM)) {
		pr_err("[ERROR][SPI] tcc-tsif: invalid port!\n");
		return -EINVAL;
	}

	tsif_pri[0].gpsb_port = (u32)port;
	return (ssize_t)count;
#endif
}
static DEVICE_ATTR(tcc_port, 0600, show_port, store_port);

#ifdef TCC_DMA_ENGINE
static bool tcc_tsif_dma_filter(struct dma_chan *chan, void *pdata)
{
	struct tca_spi_handle *tspi = pdata;
	struct device *dma_dev;

	if (tspi == NULL) {
		pr_err("[ERROR][SPI] [%s:%d] tspi is NULL!!\n",
				__func__, __LINE__);
		return (bool)false;
	}

	dma_dev = tspi->dma.dma_dev;
	if (dma_dev != chan->device->dev) {
		pr_err("[ERROR][SPI] [%s:%d] dma_dev(%p) != dev(%p)\n",
				__func__, __LINE__,
				dma_dev, chan->device->dev);
		return (bool)false;
	}

	chan->private = dma_dev;
	return (bool)true;
}

static void tcc_tsif_stop_dma(struct tca_spi_handle *tspi)
{
	if (tspi->dma.chan_rx != NULL) {
		(void)dmaengine_terminate_all(tspi->dma.chan_rx);
	}
}

static void tcc_tsif_release_dma(struct tca_spi_handle *tspi)
{
	if (tspi->dma.chan_rx != NULL) {
		dma_release_channel(tspi->dma.chan_rx);
		tspi->dma.chan_rx = NULL;
	}
}

static int32_t tcc_tsif_dma_submit(struct tca_spi_handle *tspi);
static void tcc_tsif_dma_rx_callback(void *data)
{
	struct tca_spi_handle *tspi = (struct tca_spi_handle *)data;
	struct tca_spi_pri_handle *tpri =
		(struct tca_spi_pri_handle *)tspi->private_data;

	tspi->cur_q_pos += (s32)tspi->dma_intr_packet_cnt;
	if (tspi->cur_q_pos >= (s32)tspi->dma_total_packet_cnt) {
		tspi->cur_q_pos = 0;
	}
	wake_up(&(tpri->wait_q));

	(void)tcc_tsif_dma_submit(tspi);
}

static void tcc_tsif_dma_slave_config_addr_width(
		struct dma_slave_config *slave_config, u8 bytes_per_word)
{
	/* Set WSIZE */
	slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;

	if (bytes_per_word == 4U) {
		slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	} else if (bytes_per_word == 2U) {
		slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else {
		slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	}
}

static int32_t tcc_tsif_dma_slave_config(struct tca_spi_handle *tspi,
					struct dma_slave_config *slave_config,
					u8 bytes_per_word)
{
	int32_t ret;

	if (tspi->dma.chan_rx == NULL) {
		pr_err("[ERROR][SPI] [%s:%d] RX DMA channel is NULL!!\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	/* Set BSIZE */
	slave_config->dst_maxburst = SLV_GDMA_BSIZE;
	slave_config->src_maxburst = SLV_GDMA_BSIZE;

	/* Set Address to GPSB_PORT */
	slave_config->src_addr = tspi->phy_reg_base;

	/* Set Rx Channel */
	slave_config->direction = DMA_DEV_TO_MEM;
	tcc_tsif_dma_slave_config_addr_width(slave_config, bytes_per_word);
	ret = dmaengine_slave_config(tspi->dma.chan_rx, slave_config);
	if (ret != 0) {
		pr_err("[ERROR][SPI] [%s:%d] Failed to configrue rx dma channel.\n",
				__func__, __LINE__);
		ret = -EINVAL;
	}

	return ret;
}

#define TCC_SLV_DMA_SG_LEN	1
static int32_t tcc_tsif_dma_submit(struct tca_spi_handle *tspi)
{
	struct dma_chan *rxchan = tspi->dma.chan_rx;
	struct dma_async_tx_descriptor *rxdesc;
	struct dma_slave_config slave_config;
	dma_cookie_t cookie;
	u32 len, bits_per_word, bytes_per_word;
	u32 reg_val;
	int32_t ret;

	if (rxchan == NULL) {
		pr_err("[ERROR][SPI] [%s:%d] rxchan(%p) is NULL\n",
				__func__, __LINE__, rxchan);
		return -ENODEV;
	}

	reg_val = tcc_tsif_readl(tspi->regs + TCC_GPSB_MODE);
	bits_per_word = (u32)(((reg_val >> 8U) & 0x1FUL) + 1UL);
	bytes_per_word = bits_per_word >> 3U;

	if ((UINT_MAX / (u32)SPI_GDMA_PACKET_SIZE) < tspi->dma_intr_packet_cnt) {
		pr_err("[ERROR][SPI] %s: dma_intr_packet(%d) is too much\n",
				__func__, tspi->dma_intr_packet_cnt);
		return -EINVAL;
	}
	len = (tspi->dma_intr_packet_cnt * (u32)SPI_GDMA_PACKET_SIZE);
	len = len / (u32)SLV_GDMA_BSIZE;
	if ((UINT_MAX / len) < bytes_per_word) {
		pr_err("[ERROR][SPI] %s: len(%d) is too long\n",
				__func__, len);
	}
	len = len * bytes_per_word;

	/* Prepare the RX dma transfer */
	sg_init_table(&tspi->dma.sgrx, TCC_SLV_DMA_SG_LEN);
	sg_dma_len(&tspi->dma.sgrx) = len; /* Set HOP Count */
	sg_dma_address(&tspi->dma.sgrx) = tspi->rx_dma.dma_addr;

	/* Config dma slave */
	ret = tcc_tsif_dma_slave_config(tspi, &slave_config,
			(u8)bytes_per_word);
	if (ret != 0) {
		pr_err("[ERROR][SPI] [%s:%d] Slave config failed.\n",
				__func__, __LINE__);
		return -ENOMEM;
	}

	/* Send scatterlist for RX */
	rxdesc = dmaengine_prep_slave_sg(rxchan, &tspi->dma.sgrx,
			TCC_SLV_DMA_SG_LEN,
			DMA_DEV_TO_MEM,
			(u32)DMA_PREP_INTERRUPT | (u32)DMA_CTRL_ACK);
	if (rxdesc == NULL) {
		pr_err("[ERROR][SPI] [%s:%d] Preparing RX DMA Desc.\n",
				__func__, __LINE__);
		goto err_dma;
	}

	rxdesc->callback = tcc_tsif_dma_rx_callback;
	rxdesc->callback_param = tspi;

	/* Enable GPSB Interrupt */
	reg_val = tcc_tsif_readl(tspi->regs + TCC_GPSB_INTEN);
	reg_val &= (u32)(~(BIT(25) | BIT(24)));
	if (bytes_per_word == 4U) {
		reg_val |= (u32)(BIT(25) | BIT(24));
	}
	if (bytes_per_word == 2U) {
		reg_val |= (u32)BIT(24);
	}
	tcc_tsif_writel(reg_val, tspi->regs + TCC_GPSB_INTEN);

	/* Submit desctriptors */
	cookie = dmaengine_submit(rxdesc);
	if (dma_submit_error(cookie) != 0) {
		pr_err("[ERROR][SPI] [%s:%d] RX Desc. submitting error! (cookie: %X)\n",
				__func__, __LINE__, (u32)cookie);
		goto err_dma;
	}

	/* Issue pendings */
	dma_async_issue_pending(rxchan);

	return 0;
err_dma:
	tcc_tsif_stop_dma(tspi);
	return -ENOMEM;
}

static int32_t tcc_tsif_dma_configuration(struct platform_device *pdev,
					struct tca_spi_handle *tspi)
{
	struct dma_slave_config slave_config;
	struct device *dev = &pdev->dev;
	int32_t ret;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	tspi->dma.chan_tx = NULL;

	tspi->dma.chan_rx = dma_request_slave_channel_compat(mask,
							tcc_tsif_dma_filter,
							&tspi->dma, dev, "rx");
	if (tspi->dma.chan_rx == NULL) {
		pr_err("[ERROR][SPI] [%s:%d] DMA RX channel request Error!\n",
				__func__, __LINE__);
		ret = -EBUSY;
		goto error;
	}

	ret = tcc_tsif_dma_slave_config(tspi, &slave_config, SLV_GDMA_WSIZE);
	if (ret != 0) {
		goto error;
	}
	pr_debug("[DEBUG][SPI] Using %s (RX) for DMA trnasfers\n",
			dma_chan_name(tspi->dma.chan_rx));

	return 0;

error:
	tcc_tsif_release_dma(tspi);
	return ret;
}
#else
static void tcc_tsif_stop_dma(struct tca_spi_handle *tspi)
{
}
static void tcc_tsif_release_dma(struct tca_spi_handle *tspi)
{
}
static int32_t tcc_tsif_dma_submit(struct tca_spi_handle *tspi)
{
	return -EPERM;
}
static int32_t tcc_tsif_dma_configuration(struct platform_device *pdev,
					struct tca_spi_handle *tspi)
{
	return -EPERM;
}
#endif /* TCC_DMA_ENGINE */

#ifdef CONFIG_OF
static int32_t tcc_gpsb_tsif_parse_dt(struct device_node *np,
				struct tca_spi_port_config *pgpios)
{
	int32_t ret;
	u32 utmp_val;

	pgpios->name = np->name;
	ret = of_property_read_u32(np, "gpsb-id", &utmp_val);
	if (ret != 0) {
		pr_err("[ERROR][SPI] Cannot find gpsb-id property\n");
		return -EINVAL;
	}
	pgpios->gpsb_id = (s32)utmp_val;

#ifdef TCC_USE_GFB_PORT
	/* Port array order : [0]SDI [1]SCLK [2]SFRM [3]SDO */
	of_property_read_u32_array(np, "gpsb-port", pgpios->gpsb_port,
		(size_t)of_property_count_elems_of_size(np, "gpsb-port",
							sizeof(u32)));

	pr_info("[INFO][SPI] SDI: %d, SCLK: %d, SFRM: %d, SDO: %d\n",
			pgpios->gpsb_port[0], pgpios->gpsb_port[1],
			pgpios->gpsb_port[2], pgpios->gpsb_port[3]);
#else
	ret = of_property_read_u32(np, "gpsb-port", &pgpios->gpsb_port);
	if (ret != 0) {
		pr_err("[ERROR][SPI] Cannot find gpsb-port property\n");
		return -EINVAL;
	}
#endif

	return ret;
}

#else
static int32_t tcc_gpsb_tsif_parse_dt(struct device_node *np,
				struct tca_spi_port_config *pgpios)
{
	return 0;
}
#endif /* CONFIG_OF */

static int32_t tcc_gpsb_tsif_probe(struct platform_device *pdev)
{
	int32_t ret;
	int32_t ret1, ret2, i;
	int32_t irq;
	int32_t id, rx_id;
	uint32_t ac_val[2] = {0};
	struct resource *regs;
	struct tca_spi_port_config tmpcfg;
	struct device *tmp_dev;

	/* global variables init */
	pTsif_mode = &tsif_mode[0];

#ifdef USE_STATIC_DMA_BUFFER
	if (g_static_dma == NULL) {
		g_static_dma = kmalloc(sizeof(struct tea_dma_buf), GFP_KERNEL);
		if (g_static_dma != NULL) {
			g_static_dma->buf_size = ALLOC_DMA_SIZE;
			g_static_dma->v_addr = dma_alloc_writecombine(0,
					(size_t)g_static_dma->buf_size,
					&g_static_dma->dma_addr,
					GFP_KERNEL);
			dev_dbg(&pdev->dev,
					"[DEBUG][SPI] tcc-tsif : dma buffer alloc 0x%p(Phy=0x%p), size:%d\n",
					g_static_dma->v_addr,
					(void *)g_static_dma->dma_addr,
					g_static_dma->buf_size);
			if (g_static_dma->v_addr == NULL) {
				kfree(g_static_dma);
				g_static_dma = NULL;
			}
		}
	}
#endif

	if (pdev->dev.of_node == NULL) {
		return -EINVAL;
	}
	/* GPSB Register */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (regs == NULL) {
		dev_err(&pdev->dev,
				"[ERROR][SPI] Found SPI TSIF with no register addr. Check %s setup!\n",
				dev_name(&pdev->dev));
		return -ENXIO;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev,
				"[ERROR][SPI] Cannot get irq number\n");
		return -ENXIO;
	}

#ifdef CONFIG_OF
	ret = tcc_gpsb_tsif_parse_dt(pdev->dev.of_node, &tmpcfg);
	if (ret != 0) {
		pr_err("[ERROR][SPI] Failed to parse DTB\n");
		return -EINVAL;
	}

	ret = (s32)of_property_read_bool(pdev->dev.of_node, "normal-slave");
	if (ret != 0) {
		tsif_pri[tmpcfg.gpsb_id].normal_slave = (bool)1;
		dev_dbg(&pdev->dev,
				"[DEBUG][SPI] [%s:%d] TSIF [%d] Normal Slave mode.\n",
				__func__, __LINE__, tmpcfg.gpsb_id);
	} else {
		tsif_pri[tmpcfg.gpsb_id].normal_slave = (bool)0;
		dev_dbg(&pdev->dev,
				"[DEBUG][SPI] [%s:%d] TSIF [%d] TSIF mode.\n",
				__func__, __LINE__, tmpcfg.gpsb_id);
	}
	/* USE SRAM */
	ret1 = of_property_read_u32(pdev->dev.of_node, "sram-addr",
			&tsif_pri[tmpcfg.gpsb_id].sram_addr);
	ret2 = of_property_read_u32(pdev->dev.of_node, "sram-size",
			&tsif_pri[tmpcfg.gpsb_id].sram_size);
	if ((ret1 != 0) || (ret2 != 0)) {
		tsif_pri[tmpcfg.gpsb_id].sram_use = 0;
	} else {
		tsif_pri[tmpcfg.gpsb_id].sram_use = 1;
	}
#endif

	id = tmpcfg.gpsb_id;
	(void)memset(&tsdemux_extern_handle[id], 0x0,
			sizeof(struct tsdemux_extern));
	(void)memcpy(&port_cfg[id], &tmpcfg,
			sizeof(struct tca_spi_port_config));
	mutex_init(&(tsif_pri[id].mutex));

	pdev->id = id;
	port_cfg[id].name = regs->name + 1;

	for (i = 0; i < MAX_PCR_CNT; i++) {
		tsif_pri[id].pcr_pid[i] = 0xFFFF;
	}
	tsif_pri[id].drv_major_num = tsif_major_num;
	tsif_pri[id].bus_num = (u32)id;
	tsif_pri[id].irq_no = (u32)irq;
	tsif_pri[id].reg_base = devm_ioremap_resource(&pdev->dev, regs);
	tsif_pri[id].phy_reg_base = regs->start;
	tsif_pri[id].port_reg = of_iomap(pdev->dev.of_node, 1);
	tsif_pri[id].pid_reg = of_iomap(pdev->dev.of_node, 2);
	tsif_pri[id].ac_reg = of_iomap(pdev->dev.of_node, 3);
	tsif_pri[id].name = port_cfg[id].name;
	tsif_pri[id].dev = &pdev->dev;

	/* Access Control configuration */
	if (tsif_pri[id].ac_reg != NULL) {

		if (of_property_read_u32_array(pdev->dev.of_node,
					"access-control0", ac_val, 2) == 0) {
			dev_dbg(&pdev->dev, "[DEBUG][SPI] access-control0 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0],
				tsif_pri[id].ac_reg + TCC_GPSB_AC0_START);
			writel(ac_val[1],
				tsif_pri[id].ac_reg + TCC_GPSB_AC0_LIMIT);
		}
		if (of_property_read_u32_array(pdev->dev.of_node,
					"access-control1", ac_val, 2) == 0) {
			dev_dbg(&pdev->dev, "[DEBUG][SPI] access-control1 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0],
				tsif_pri[id].ac_reg + TCC_GPSB_AC1_START);
			writel(ac_val[1],
				tsif_pri[id].ac_reg + TCC_GPSB_AC1_LIMIT);
		}
		if (of_property_read_u32_array(pdev->dev.of_node,
					"access-control2", ac_val, 2) == 0) {
			dev_dbg(&pdev->dev, "[DEBUG][SPI] access-control2 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0],
				tsif_pri[id].ac_reg + TCC_GPSB_AC2_START);
			writel(ac_val[1],
				tsif_pri[id].ac_reg + TCC_GPSB_AC2_LIMIT);
		}
		if (of_property_read_u32_array(pdev->dev.of_node,
					"access-control3", ac_val, 2) == 0) {
			dev_dbg(&pdev->dev, "[DEBUG][SPI] access-control3 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0],
				tsif_pri[id].ac_reg + TCC_GPSB_AC3_START);
			writel(ac_val[1],
				tsif_pri[id].ac_reg + TCC_GPSB_AC3_LIMIT);
		}
	}

	/* Does it use GDMA? */
	ret = sscanf(tsif_pri[id].name, "gpsb%d", &rx_id);
	if (ret != 1) {
		return -EINVAL;
	}
	if (rx_id < 3) {
		tsif_pri[id].gdma_use = 0;
	} else {
		tsif_pri[id].gdma_use = 1;
	}

#ifdef TCC_USE_GFB_PORT
	for (i = 0; i < 4; i++)
		tsif_pri[id].gpsb_port[i] = port_cfg[id].gpsb_port[i];
#else
	tsif_pri[id].gpsb_port = port_cfg[id].gpsb_port;
#endif

	/* GDMA Register (if does not use GDMA Address is NULL) */
	if (tsif_pri[id].gdma_use != (u32)0) {
		dev_dbg(&pdev->dev, "[DEBUG][SPI] [%s:%d] GPSB id: %d (GDMA)\n",
				__func__, __LINE__, id);
#ifdef TCC_DMA_ENGINE
		ret = tcc_tsif_dma_configuration(pdev, &tsif_handle[id]);
#else
		ret = -ENOMEM;
#endif
		if (ret != 0) {
			dev_err(&pdev->dev,
					"[ERROR][SPI] [%s:%d] tcc dma engine configration fail!!\n",
					__func__, __LINE__);
			return -ENXIO;
		}
	} else {
		dev_dbg(&pdev->dev,
				"[DEBUG][SPI] [%s:%d] GPSB id: %d (Dedicated DMA)\n",
				__func__, __LINE__, id);
	}

	dev_info(&pdev->dev,
			"[INFO][SPI] Telechips TSIF ID: %d, 0x%08x, 0x%08x, %s, flags: %lu\n",
			id, (u32)(regs->start), (u32)(regs->end), regs->name,
			regs->flags);
#ifdef TCC_USE_GFB_PORT
	dev_dbg(&pdev->dev,
			"[DEBUG][SPI] [%s:%d]%d, %d, %p, %s, ret = %d\n",
			__func__, __LINE__, pdev->id, irq,
			tsif_pri[id].reg_base, port_cfg[id].name, ret);
#else
	dev_dbg(&pdev->dev,
			"[DEBUG][SPI] [%s:%d]%d, %d, %p, %d, %s, ret = %d\n",
			__func__, __LINE__, pdev->id, irq,
			tsif_pri[id].reg_base, port_cfg[id].gpsb_port,
			port_cfg[id].name, ret);
#endif

	tmp_dev = device_create(tsif_class, NULL,
			MKDEV(tsif_pri[id].drv_major_num, id), NULL,
			TSIF_DEV_NAMES, id);
	ret = (s32)IS_ERR(tmp_dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][SPI] Failed to create device\n");
		return -EINVAL;
	}

	gpsb_tsif_num++;
	tsif_mode[id] = TSIF_MODE_GPSB;

	gpsb_hclk[id] = of_clk_get(pdev->dev.of_node, 1);
	gpsb_pclk[id] = of_clk_get(pdev->dev.of_node, 0);

	platform_set_drvdata(pdev, &tsif_handle[id]);

	ret = device_create_file(&pdev->dev, &dev_attr_tcc_port);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][SPI] Failed to create device file\n");
		return -EINVAL;
	}

	return 0;
}

static int32_t tcc_gpsb_tsif_remove(struct platform_device *pdev)
{
	uint32_t id;

	if (pdev->id < 0) {
		return -EINVAL;
	}
	id = (u32)pdev->id;

	if ((gpsb_pclk[id] != NULL) && (gpsb_hclk[id] != NULL)) {
		clk_disable_unprepare(gpsb_pclk[id]);
		clk_disable_unprepare(gpsb_hclk[id]);
		clk_put(gpsb_pclk[id]);
		clk_put(gpsb_hclk[id]);
		gpsb_pclk[id] = NULL;
		gpsb_hclk[id] = NULL;
	}

	tsif_mode[id] = 0;
	gpsb_tsif_num--;
	device_destroy(tsif_class, MKDEV(tsif_major_num, id));

	if (g_static_dma != NULL) {
		dma_free_writecombine(NULL, (size_t)g_static_dma->buf_size,
				g_static_dma->v_addr, g_static_dma->dma_addr);
		kfree(g_static_dma);
		g_static_dma = NULL;
	}

	tcc_tsif_release_dma(&tsif_handle[id]);

	device_remove_file(&pdev->dev, &dev_attr_tcc_port);

	return 0;
}

static void tea_free_dma_linux(struct tea_dma_buf *tdma, struct device *dev,
				int32_t id)
{
	if (tsif_pri[id].sram_use != 0U) {
		iounmap(tdma->v_addr);
		return;
	}

	if (g_static_dma != NULL) {
		return;
	}
	if (tdma != NULL) {
		if (tdma->v_addr != NULL) {
			dma_free_writecombine(dev, (size_t)tdma->buf_size,
					tdma->v_addr, tdma->dma_addr);
		}
		(void)memset(tdma, 0, sizeof(struct tea_dma_buf));
	}
}

static int32_t tea_alloc_dma_linux(struct tea_dma_buf *tdma, uint32_t size,
				struct device *dev, int32_t id)
{
	int32_t ret = -1;

	if (tsif_pri[id].sram_use != 0U) {
		tdma->buf_size = (s32)tsif_pri[id].sram_size;
		tdma->dma_addr = tsif_pri[id].sram_addr;
		tdma->v_addr = (void *)ioremap_wc(tdma->dma_addr,
						(u32)tdma->buf_size);
		pr_debug("[DEBUG][SPI] tcc_tsif: alloc DMA buffer(SRAM) 0x%p(Phy=0x%p), size:0x%X\n",
				tdma->v_addr,
				(void *)tdma->dma_addr,
				tdma->buf_size);
		ret = (tdma->v_addr != NULL) ? 0 : 1;
		return ret;
	}

	if (g_static_dma != NULL) {
		tdma->buf_size = g_static_dma->buf_size;
		tdma->v_addr = g_static_dma->v_addr;
		tdma->dma_addr = g_static_dma->dma_addr;
		return 0;
	}

	if (tdma != NULL) {
		tea_free_dma_linux(tdma, dev, id);
		tdma->buf_size = (s32)size;
		tdma->v_addr = dma_alloc_writecombine(dev, (u32)tdma->buf_size,
				&tdma->dma_addr, GFP_KERNEL);
		pr_debug("[DEBUG][SPI] tcc_tsif: alloc DMA buffer 0x%p(Phy=0x%p), size:%d\n",
				tdma->v_addr,
				(void *)tdma->dma_addr,
				tdma->buf_size);
		ret = (tdma->v_addr != NULL) ? 0 : 1;
	}
	return ret;
}

static irqreturn_t tcc_gpsb_tsif_dma_handler(int32_t irq, void *dev_id)
{
	struct tca_spi_handle *tspi = (struct tca_spi_handle *)dev_id;
	struct tca_spi_pri_handle *tpri =
		(struct tca_spi_pri_handle *)tspi->private_data;
	void __iomem *gpsb_pcfg_reg = (void __iomem *)tspi->port_regs;
	ulong dma_done_reg;
	int32_t id = tspi->id;
	int32_t irq_idx = tspi->gpsb_channel;
	int32_t ret;
	u32 uret;

	ret = tca_spi_is_use_gdma(tspi);
	if (ret != 0) {
		return IRQ_HANDLED;
	}
	/* if use dedicated DMA */
	/* Check GPSB Core IRQ */
	uret = (u32)2UL << ((u32)irq_idx << 1);
	uret = __raw_readl(gpsb_pcfg_reg + 0xC) & uret;
	if (uret == 0UL) {
		return IRQ_NONE;
	}
	dma_done_reg = tcc_tsif_readl(tspi->regs + TCC_GPSB_DMAICR);
	if ((dma_done_reg & (BIT(28) | BIT(29))) == (u32)0) {
		return IRQ_HANDLED;
	}
	TCC_GPSB_BITSET(tspi->regs + TCC_GPSB_DMAICR, BIT(29) | BIT(28));
	uret = tcc_tsif_readl(tspi->regs + TCC_GPSB_DMASTR) >> 17U;
	tspi->cur_q_pos = (s32)uret;

	if (tpri->open_cnt <= 0) {
		return IRQ_HANDLED;
	}
	if ((tsdemux_extern_handle[id].is_active != 0) &&
			(tsdemux_extern_handle[id].tsdemux_decoder != NULL)) {
		if (tspi->cur_q_pos == tsif_handle[id].q_pos) {
			return IRQ_HANDLED;
		}
		tsdemux_extern_handle[id].index++;
		if (tsdemux_extern_handle[id].index >=
				tsdemux_extern_handle[id].call_decoder_index) {
			char *p1, *p2 = NULL;
			int32_t p1_size, p2_size = 0;

			if (tspi->cur_q_pos > tsif_handle[id].q_pos) {
				p1 = (char *)tsif_handle[id].rx_dma.v_addr +
						(tsif_handle[id].q_pos *
						 TSIF_PACKET_SIZE);
				p1_size = (tspi->cur_q_pos -
						tsif_handle[id].q_pos) *
						TSIF_PACKET_SIZE;
			} else {
				p1 = (char *)tsif_handle[id].rx_dma.v_addr +
						(tsif_handle[id].q_pos *
						 TSIF_PACKET_SIZE);
				ret = (s32)tspi->dma_total_packet_cnt;
				p1_size = (ret - tsif_handle[id].q_pos) *
					TSIF_PACKET_SIZE;

				p2 = (char *)tsif_handle[id].rx_dma.v_addr;
				p2_size = tspi->cur_q_pos * TSIF_PACKET_SIZE;
			}
			ret = tsdemux_extern_handle[id].tsdemux_decoder(
					p1, p1_size, p2, p2_size, id);
			if (ret == 0) {
				tsif_handle[id].q_pos = tspi->cur_q_pos;
				tsdemux_extern_handle[id].index = 0;
			}
		}
	}

	/* Check read count & wake_up wait_q */
	if (tsdemux_extern_handle[id].tsdemux_decoder == NULL) {
		ret = tsif_get_readable_cnt(tspi);
		uret = (u32)ret;
		if (uret >= tpri->packet_read_count) {
			wake_up(&(tpri->wait_q));
		}
	}

	return IRQ_HANDLED;
}

static int32_t tsif_get_readable_cnt(struct tca_spi_handle *H)
{
	int32_t ret;

	if (H != NULL) {
		int32_t dma_pos = H->cur_q_pos;
		int32_t q_pos = H->q_pos;
		int32_t readable_cnt = 0;

		if (dma_pos > q_pos) {
			readable_cnt = dma_pos - q_pos;
		} else if (dma_pos < q_pos) {
			ret = (s32)H->dma_total_packet_cnt;
			readable_cnt = ret - q_pos;
			readable_cnt += dma_pos;
		} else {
			return readable_cnt;
		}
		return readable_cnt;
	}

	return 0;
}

static ssize_t tcc_gpsb_tsif_read(struct file *filp, char *buf, size_t len,
				loff_t *ppos)
{
	int32_t readable_cnt, copy_cnt;
	int32_t copy_byte;
	int32_t id = ((struct fo *)filp->private_data)->minor;
	int32_t packet_size;
	int32_t ret, ret2;
	u32 uret;

	ret = tca_spi_is_use_gdma(&tsif_handle[id]);
	if (ret == 0) {
		/* if use dedicated DMA */
		packet_size = TSIF_PACKET_SIZE;
		ret = tca_spi_is_normal_slave(&tsif_handle[id]);
		if (ret != 0) {
			packet_size = NORMAL_PACKET_SIZE;
		}
	} else {
		packet_size = SPI_GDMA_PACKET_SIZE;
	}

	readable_cnt = tsif_get_readable_cnt(&tsif_handle[id]);

	if (readable_cnt > 0) {
		ret = tca_spi_is_use_gdma(&tsif_handle[id]);
		ret2 = tca_spi_is_normal_slave(&tsif_handle[id]);
		if ((ret == 0) && (ret2 == 0)) {
			/* if use dedicated DMA */
			ret = (s32)len;
			ret = ret/packet_size;
			uret = (u32)ret;
			if (tsif_pri[id].packet_read_count != uret) {
				pr_debug("[DEBUG][SPI] set packet_read_count=%d\n",
						(s32)ret);
			}
			tsif_pri[id].packet_read_count = uret;
		}

		copy_byte = readable_cnt * packet_size;
		ret = (s32)len;
		if (copy_byte > ret) {
			copy_byte = ret;
		}
		copy_byte -= copy_byte % packet_size;
		copy_cnt = copy_byte / packet_size;
		ret = (s32)tsif_handle[id].dma_intr_packet_cnt;
		copy_cnt -= copy_cnt % ret;
		copy_byte = copy_cnt * packet_size;

		ret = (s32)tsif_handle[id].dma_intr_packet_cnt;
		if (copy_cnt >= ret) {
			int32_t offset = tsif_handle[id].q_pos * packet_size;

			/* When does not use repeat mode,
			 * rx_dma buf address offset should be zero.
			 */
			ret = tca_spi_is_use_gdma(&tsif_handle[id]);
			if (ret != 0) {
				offset = 0;
			}
			ret = (s32)tsif_handle[id].dma_total_packet_cnt;
			ret = ret - tsif_handle[id].q_pos;
			if (copy_cnt > ret) {
				int32_t first_copy_byte;
				int32_t first_copy_cnt;
				int32_t second_copy_byte;

				ret = (s32)tsif_handle[id].dma_total_packet_cnt;
				ret = ret - tsif_handle[id].q_pos;
				first_copy_byte = ret * packet_size;
				first_copy_cnt = first_copy_byte / packet_size;
				uret = (u32)copy_to_user(buf,
					tsif_handle[id].rx_dma.v_addr + offset,
					(u32)first_copy_byte);
				if (uret != 0UL) {
					return -EFAULT;
				}
				second_copy_byte = (copy_cnt - first_copy_cnt) *
					packet_size;
				uret = (u32)copy_to_user(buf + first_copy_byte,
						tsif_handle[id].rx_dma.v_addr,
						(u32)second_copy_byte);
				if (uret != 0UL) {
					return -EFAULT;
				}
				tsif_handle[id].q_pos =
					copy_cnt - first_copy_cnt;
			} else {
				uret = (u32)copy_to_user(buf,
					tsif_handle[id].rx_dma.v_addr + offset,
					(u32)copy_byte);
				if (uret != 0UL) {
					return -EFAULT;
				}
				tsif_handle[id].q_pos += copy_cnt;
				ret = (s32)tsif_handle[id].dma_total_packet_cnt;
				if (tsif_handle[id].q_pos >= ret) {
					tsif_handle[id].q_pos = 0;
				}
			}
			return copy_byte;
		}
	}
	return 0;
}

static ssize_t tcc_gpsb_tsif_write(struct file *filp, const char *buf,
				size_t len, loff_t *ppos)
{
	/* int32_t id = ((struct fo *)filp->private_data)->minor. */
	return 0;
}

static uint32_t tcc_gpsb_tsif_poll(struct file *filp,
					struct poll_table_struct *wait)
{
	int32_t id = ((struct fo *)filp->private_data)->minor;
	int32_t ret;
	u32 uret;

	ret = tsif_get_readable_cnt(&tsif_handle[id]);
	uret = (u32)ret;
	if (uret >= tsif_pri[id].packet_read_count) {
		return ((u32)POLLIN | (u32)POLLRDNORM);
	}

	poll_wait(filp, &(tsif_pri[id].wait_q), wait);
	ret = tsif_get_readable_cnt(&tsif_handle[id]);
	uret = (u32)ret;
	if (uret >= tsif_pri[id].packet_read_count) {
		return	((u32)POLLIN | (u32)POLLRDNORM);
	}

	return 0;
}

static ssize_t tcc_gpsb_tsif_copy_from_user(void *dest, void *src,
					size_t copy_size)
{
	int32_t ret;
	u32 uret;

	uret = (u32)copy_from_user(dest, src, copy_size);
	ret = (s32)uret;

	return ret;
}

static ssize_t tcc_gpsb_tsif_copy_to_user(void *dest, void *src,
					size_t copy_size)
{
	int32_t ret;
	u32 uret;

	uret = (u32)copy_to_user(dest, src, copy_size);
	ret = (s32)uret;

	return ret;
}

static long tcc_gpsb_tsif_ioctl(struct file *filp, uint32_t cmd,
				ulong arg)
{
	int32_t ret, ret2;
	u32 mode;
	int32_t id = ((struct fo *)filp->private_data)->minor;
	int32_t packet_size;

	ret = tca_spi_is_use_gdma(&tsif_handle[id]);
	if (ret == 0) {
		packet_size = TSIF_PACKET_SIZE;
		ret = tca_spi_is_normal_slave(&tsif_handle[id]);
		if (ret != 0) {
			packet_size = NORMAL_PACKET_SIZE;
		}
	} else {
		packet_size = SPI_GDMA_PACKET_SIZE;
	}

	switch (cmd) {
	case IOCTL_TSIF_DMA_START:
	{
		struct tcc_tsif_param param;

		ret = (s32)tcc_gpsb_tsif_copy_from_user((void *)&param,
				(void *)arg, sizeof(struct tcc_tsif_param));
		if (ret != 0) {
			pr_err("[ERROR][SPI] cannot copy from user in IOCTL_TSIF_DMA_START\n");
			return -EFAULT;
		}

		ret = (s32)param.ts_total_packet_cnt;
		ret = packet_size * ret;
		ret2 = (s32)param.ts_total_packet_cnt;
		if ((ret > tsif_handle[id].dma_total_size) || (ret2 <= 0)) {
			pr_warn("[WARN][SPI] so big ts_total_packet_cnt[%d:%d]\n",
					ret, tsif_handle[id].dma_total_size);
			ret = (s32)param.ts_intr_packet_cnt;
			ret = packet_size * ret;
			ret = tsif_handle[id].dma_total_size / ret;
			param.ts_total_packet_cnt = (u32)ret;
		}

		/* Max packet is 0x1fff(13bit) */
		if (param.ts_total_packet_cnt > 0x1fffUL) {
			param.ts_total_packet_cnt = 0x1fffU;
		}
		tcc_tsif_stop_dma(&tsif_handle[id]);
		tsif_handle[id].dma_stop(&tsif_handle[id]);

		tsif_handle[id].dma_mode = (s32)param.dma_mode;
		if (tsif_handle[id].dma_mode == 0) {
			tsif_handle[id].set_dma_addr(&tsif_handle[id]);
			tsif_handle[id].set_mpegts_pidmode(&tsif_handle[id], 0);
		}

		tsif_handle[id].dma_total_packet_cnt =
			param.ts_total_packet_cnt;
		tsif_handle[id].dma_intr_packet_cnt = param.ts_intr_packet_cnt;

		if (tsif_handle[id].dma_total_packet_cnt ==
				tsif_handle[id].dma_intr_packet_cnt) {
			pr_warn("[WARN][SPI] ## Warning! total_packet_cnt[%d] and intr_packet_cnt[%d] is same!! ## --> intr_packet_cnt is set to [%d] ##\n",
				tsif_handle[id].dma_total_packet_cnt,
				tsif_handle[id].dma_intr_packet_cnt,
				tsif_handle[id].dma_total_packet_cnt /
				(u32)TCC_GPSB_TSIF_DEF_INTR_INTERVAL);
			tsif_handle[id].dma_intr_packet_cnt =
				tsif_handle[id].dma_total_packet_cnt /
				(u32)TCC_GPSB_TSIF_DEF_INTR_INTERVAL;
		}

		tsif_pri[id].packet_read_count =
			tsif_handle[id].dma_intr_packet_cnt;

		tsif_handle[id].clear_fifo_packet(&tsif_handle[id]);
		tsif_handle[id].q_pos = 0;
		tsif_handle[id].cur_q_pos = 0;
		ret = tca_spi_is_use_gdma(&tsif_handle[id]);
		if (ret != 0) { /* if use GDMA */
			ret = tcc_tsif_dma_submit(&tsif_handle[id]);
		}
		/* Set clocking mode and data direction */
		mode = (u32)param.mode;
		if ((mode & (u32)SPI_CS_HIGH) != 0U) {
			TCC_GPSB_BITSET(tsif_handle[id].regs + TCC_GPSB_MODE,
					(BIT(20) | BIT(19)));
		} else {
			TCC_GPSB_BITCLR(tsif_handle[id].regs + TCC_GPSB_MODE,
					(BIT(20) | BIT(19)));
		}
		if ((mode & (u32)SPI_LSB_FIRST) != 0U) {
			TCC_GPSB_BITSET(tsif_handle[id].regs + TCC_GPSB_MODE,
					BIT(7));
		} else {
			TCC_GPSB_BITCLR(tsif_handle[id].regs + TCC_GPSB_MODE,
					BIT(7));
		}
		if (((mode & (u32)SPI_CPHA) == 0U) &&
				((mode & (u32)SPI_CPOL) == 0U)) {
			TCC_GPSB_BITCLR(tsif_handle[id].regs + TCC_GPSB_MODE,
					BIT(16));
		} else if (((mode & (u32)SPI_CPHA) != 0U) &&
				((mode & (u32)SPI_CPOL) != 0U)) {
			TCC_GPSB_BITCLR(tsif_handle[id].regs + TCC_GPSB_MODE,
					BIT(16));
		} else {
			TCC_GPSB_BITSET(tsif_handle[id].regs + TCC_GPSB_MODE,
					BIT(16));
		}
		TCC_GPSB_BITCLR(tsif_handle[id].regs + TCC_GPSB_MODE,
				(BIT(17) | BIT(18)));

		tsif_handle[id].set_packet_cnt(&tsif_handle[id], packet_size);
		tsif_handle[id].dma_start(&tsif_handle[id]);

		break;
	}
	case IOCTL_TSIF_DMA_STOP:
	{
		tcc_tsif_stop_dma(&tsif_handle[id]);
		tsif_handle[id].dma_stop(&tsif_handle[id]);
		break;
	}
	case IOCTL_TSIF_GET_MAX_DMA_SIZE:
	{
		struct tcc_tsif_param param;

		ret = tsif_handle[id].dma_total_size / TSIF_PACKET_SIZE;
		param.ts_total_packet_cnt = (u32)ret;
		param.ts_intr_packet_cnt = 1;
		ret = (s32)tcc_gpsb_tsif_copy_to_user((void *)arg,
				(void *)&param, sizeof(struct tcc_tsif_param));
		if (ret != 0) {
			pr_err("[ERROR][SPI] cannot copy to user in IOCTL_TSIF_GET_MAX_DMA_SIZE\n");
			return -EFAULT;
		}

		break;
	}
	case IOCTL_TSIF_SET_PID:
	{
		struct tcc_tsif_pid_param param;

		ret = (s32)tcc_gpsb_tsif_copy_from_user((void *)&param,
				(void *)arg, sizeof(struct tcc_tsif_pid_param));
		if (ret != 0) {
			pr_err("[ERROR][SPI] cannot copy from user in IOCTL_TSIF_SET_PID\n");
			return -EFAULT;
		}
		ret = tca_spi_register_pids(&tsif_handle[id], param.pid_data,
				param.valid_data_cnt);

		break;
	}
	case IOCTL_TSIF_DXB_POWER:
		/* NOTE:
		 * the power control moves to tcc_dxb_control driver.
		 */
		break;
	case IOCTL_TSIF_RESET:
		break;
	default:
		pr_err("[ERROR][SPI] tsif: unrecognized ioctl (0x%X)\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int32_t tcc_gpsb_tsif_init(int32_t id)
{
	int32_t ret, packet_size;
	struct device *dev;
	int32_t slave_flag;

	dev = tsif_pri[id].dev;

	if (tsif_pri[id].normal_slave) {
		slave_flag = (s32)TCC_GPSB_SLAVE_NORMAL;
	} else {
		slave_flag = (s32)TCC_GPSB_SLAVE_TSIF;
	}
	ret = tca_spi_init(&tsif_handle[id],
				tsif_pri[id].reg_base,
				(u32)tsif_pri[id].phy_reg_base,
				tsif_pri[id].port_reg,
				tsif_pri[id].pid_reg,
				(s32)tsif_pri[id].irq_no,
				tea_alloc_dma_linux,
				tea_free_dma_linux,
				ALLOC_DMA_SIZE,
				(s32)tsif_pri[id].bus_num,
				slave_flag,
				&port_cfg[id],
				tsif_pri[id].name,
				dev);
	if (ret != 0) {
		pr_err("[ERROR][SPI] %s: tca_spi_init error !!!!!\n", __func__);
		ret = -EBUSY;
		goto err_spi;
	}

	tsif_handle[id].private_data = (void *)&tsif_pri[id];
	init_waitqueue_head(&(tsif_pri[id].wait_q));
	tsif_handle[id].clear_fifo_packet(&tsif_handle[id]);
	tsif_handle[id].dma_stop(&tsif_handle[id]);

	ret = tca_spi_is_use_gdma(&tsif_handle[id]);
	if (ret == 0) {
		/* if use dedicated DMA */
		packet_size = TSIF_PACKET_SIZE;
		ret = tca_spi_is_normal_slave(&tsif_handle[id]);
		if (ret != 0) {
			packet_size = NORMAL_PACKET_SIZE;
		}
	} else {
		/* if use GDMA */
		packet_size = SPI_GDMA_PACKET_SIZE;
	}

	ret = tsif_handle[id].dma_total_size / packet_size;
	tsif_handle[id].dma_total_packet_cnt = (u32)ret;
	tsif_handle[id].dma_intr_packet_cnt = 1;

	tsif_handle[id].hw_init(&tsif_handle[id]);

	ret = tca_spi_is_use_gdma(&tsif_handle[id]);
	if (ret == 0) {
		/* if use dedicated DMA */
		ret = request_irq((u32)tsif_handle[id].irq,
				tcc_gpsb_tsif_dma_handler, IRQF_SHARED,
				tsif_pri[id].name, (void *)&tsif_handle[id]);
	}

	if (ret != 0) {
		goto err_irq;
	}
	tsif_handle[id].set_packet_cnt(&tsif_handle[id], packet_size);

	return 0;

err_irq:

	ret = tca_spi_is_use_gdma(&tsif_handle[id]);
	if (ret == 0) {
		/* if use dedicated DMA */
		free_irq((u32)tsif_handle[id].irq, (void *)&tsif_handle[id]);
	}

err_spi:
	tca_spi_clean(&tsif_handle[id]);
	return ret;
}

static void tcc_gpsb_tsif_deinit(int32_t id)
{
	int32_t ret;

	ret = tca_spi_is_use_gdma(&tsif_handle[id]);
	if (ret == 0) {
		/* if use dedicated DMA */
		free_irq((u32)tsif_handle[id].irq, (void *)&tsif_handle[id]);
	}
	tca_spi_clean(&tsif_handle[id]);
}

static int32_t tcc_gpsb_tsif_open(struct inode *inode, struct file *filp)
{
	int32_t i;
	int32_t minor_number = 0;
	int32_t ret, ret2;
	struct pinctrl *pinctrl;

	if (inode != NULL) {
		minor_number = (s32)iminor(inode);
		fodp = &fodevs[minor_number];
		fodp->minor = minor_number;
	}

	if (filp != NULL) {
		filp->f_op = &tcc_gpsb_tsif_fops;
		filp->private_data = (void *)fodp;
	}

	if (tsif_pri[minor_number].open_cnt == 0) {
		tsif_pri[minor_number].open_cnt++;
	} else {
		return -EBUSY;
	}
	ret = (s32)IS_ERR(gpsb_pclk[minor_number]);
	ret2 = (s32)IS_ERR(gpsb_hclk[minor_number]);
	if ((ret != 0) || (ret2 != 0)) {
		pr_err("[ERROR][SPI] TSIF#%d: failed to get gpsb clock\n",
				minor_number);
		return -EINVAL;
	}

	ret = clk_prepare_enable(gpsb_pclk[minor_number]);
	if (ret != 0) {
		pr_err("[ERROR][SPI] Failed to enable pclk\n");
		return -EINVAL;
	}
	ret = clk_prepare_enable(gpsb_hclk[minor_number]);
	if (ret != 0) {
		pr_err("[ERROR][SPI] Failed to enable hclk\n");
		return -EINVAL;
	}

	pinctrl = pinctrl_get_select(tsif_pri[minor_number].dev, "active");
	ret = (s32)IS_ERR(pinctrl);
	if (ret != 0) {
		pr_err("[ERROR][SPI] %s : pinctrl active error[0x%p]\n",
				__func__, pinctrl);
	}

	mutex_lock(&(tsif_pri[minor_number].mutex));

	for (i = 0; i < MAX_PCR_CNT; i++) {
		tsif_pri[minor_number].pcr_pid[i] = 0xFFFF;
	}
	ret = tcc_gpsb_tsif_init(minor_number);
	if (ret != 0) {
		pr_err("[ERROR][SPI] %s : tcc_gpsb_tsif_init failed(%d)\n",
				__func__, ret);
		clk_disable_unprepare(gpsb_pclk[minor_number]);
		clk_disable_unprepare(gpsb_hclk[minor_number]);
		pinctrl = pinctrl_get_select(
				tsif_pri[minor_number].dev, "idle");
		ret2 = (s32)IS_ERR(pinctrl);
		if (ret2 != 0) {
			pr_err("[ERROR][SPI] %s : pinctrl idle error[0x%p]\n",
					__func__, pinctrl);
		} else {
			pinctrl_put(pinctrl);
		}
	}

	mutex_unlock(&(tsif_pri[minor_number].mutex));

	return ret;
}

static int32_t tcc_gpsb_tsif_release(struct inode *inode, struct file *filp)
{
	int32_t minor_number = 0;
	struct pinctrl *pinctrl;
	int32_t ret;

	if (inode != NULL) {
		minor_number = (s32)iminor(inode);
	}
	if (tsif_pri[minor_number].open_cnt > 0) {
		tsif_pri[minor_number].open_cnt--;
	}
	if (tsif_pri[minor_number].open_cnt == 0) {
		mutex_lock(&(tsif_pri[minor_number].mutex));
		tsif_handle[minor_number].dma_stop(&tsif_handle[minor_number]);
		tcc_gpsb_tsif_deinit(minor_number);
		mutex_unlock(&(tsif_pri[minor_number].mutex));

		if ((gpsb_pclk[minor_number] != NULL) &&
				(gpsb_hclk[minor_number] != NULL)) {
			clk_disable_unprepare(gpsb_pclk[minor_number]);
			clk_disable_unprepare(gpsb_hclk[minor_number]);
		}

		pinctrl = pinctrl_get_select(
				tsif_pri[minor_number].dev, "idle");
		ret = (s32)IS_ERR(pinctrl);
		if (ret != 0) {
			pr_err("[ERROR][SPI] %s : pinctrl idle error[0x%p]\n",
					__func__, pinctrl);
		} else {
			pinctrl_put(pinctrl);
		}
	}

	return 0;
}

/* tcc_tsif_devices */
#ifdef CONFIG_OF
static const struct of_device_id tcc_gpsb_tsif_of_match[7] = {
	{ .compatible = "telechips,tcc89xx-tsif" },
	{ .compatible = "telechips,tcc805x-tsif" },
	{ .compatible = "telechips,tcc803x-tsif" },
	{ .compatible = "telechips,tcc897x-tsif" },
	{ .compatible = "telechips,tcc899x-tsif" },
	{ .compatible = "telechips,tcc901x-tsif" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_gpsb_tsif_of_match);
#endif

static struct platform_driver tcc_gpsb_tsif_driver = {
	.probe	= tcc_gpsb_tsif_probe,
	.remove = tcc_gpsb_tsif_remove,
	.driver = {
		.name	= "tcc-gpsb-tsif",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_gpsb_tsif_of_match),
#endif
	},
};

static void __exit tsif_exit(void)
{
	class_destroy(tsif_class);
	cdev_del(&tsif_device_cdev);
	unregister_chrdev_region(MKDEV(tsif_major_num, 0UL), TSIF_MINOR_NUMBER);

	platform_driver_unregister(&tcc_gpsb_tsif_driver);
}

static int32_t tcc_tsif_open(struct inode *inode, struct file *filp)
{
	int32_t error_check_flag = 0;
	u32 minor_num;

	minor_num = MINOR(inode->i_rdev);
	if (minor_num >= (u32)MAX_SUPPORT_TSIF_DEVICE) {
		return -EBADF;
	}
	switch (tsif_mode[minor_num]) {
	case TSIF_MODE_GPSB:
		filp->f_op = &tcc_gpsb_tsif_fops;
		break;
	default:
		error_check_flag = 1;
		break;
	}

	if (error_check_flag == 1) {
		return -ENXIO;
	}
	return filp->f_op->open(inode, filp);
}

static const struct file_operations tcc_tsif_fops = {
	.owner	= THIS_MODULE,
	.open	= tcc_tsif_open,
};

static int32_t __init tsif_init(void)
{
	int32_t ret;
	dev_t dev;

	ret = alloc_chrdev_region(&dev, 0, TSIF_MINOR_NUMBER, "TSIF");
	if (ret != 0) {
		pr_err("[ERROR][SPI] tsif: failed to alloc character device\n");
		return ret;
	}
	tsif_major_num = MAJOR(dev);

	cdev_init(&tsif_device_cdev, &tcc_tsif_fops);
	ret = cdev_add(&tsif_device_cdev, dev, TSIF_MINOR_NUMBER);
	if (ret != 0) {
		pr_err("[ERROR][SPI] tsif: unable register character device\n");
		goto tsif_init_error;
	}

	tsif_class = class_create(THIS_MODULE, "tsif");
	ret = (s32)IS_ERR(tsif_class);
	if (ret != 0) {
		ret = (s32)PTR_ERR(tsif_class);
		goto tsif_init_error;
	}

	ret = platform_driver_register(&tcc_gpsb_tsif_driver);
	if (ret != 0) {
		pr_err("[ERROR][SPI] tsif: unable register platform driver\n");
		return ret;
	}

	return 0;

tsif_init_error:
	cdev_del(&tsif_device_cdev);
	unregister_chrdev_region(dev, TSIF_MINOR_NUMBER);
	return ret;
}

module_init(tsif_init);
module_exit(tsif_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips TSIF(GPSB Slave, Block TSIF) driver");
MODULE_LICENSE("GPL");

