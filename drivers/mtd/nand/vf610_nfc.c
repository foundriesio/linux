/*
 * Copyright 2009-2015 Freescale Semiconductor, Inc. and others
 *
 * Description: MPC5125, VF610, MCF54418 and Kinetis K70 Nand driver.
 * Jason ported to M54418TWR and MVFA5 (VF610).
 * Authors: Stefan Agner <stefan.agner@toradex.com>
 *          Bill Pringlemeir <bpringlemeir@nbsps.com>
 *          Shaohui Xie <b21989@freescale.com>
 *          Jason Jin <Jason.jin@freescale.com>
 *
 * Based on original driver mpc5121_nfc.c.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Limitations:
 * - Untested on MPC5125 and M54418.
 * - DMA not used.
 * - 2K pages or less.
 * - Only 2K page w. 64+OOB and hardware ECC.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_mtd.h>

#define	DRV_NAME		"vf610_nfc"

/* Register Offsets */
#define NFC_FLASH_CMD1			0x3F00
#define NFC_FLASH_CMD2			0x3F04
#define NFC_COL_ADDR			0x3F08
#define NFC_ROW_ADDR			0x3F0c
#define NFC_ROW_ADDR_INC		0x3F14
#define NFC_FLASH_STATUS1		0x3F18
#define NFC_FLASH_STATUS2		0x3F1c
#define NFC_CACHE_SWAP			0x3F28
#define NFC_SECTOR_SIZE			0x3F2c
#define NFC_FLASH_CONFIG		0x3F30
#define NFC_IRQ_STATUS			0x3F38

/* Addresses for NFC MAIN RAM BUFFER areas */
#define NFC_MAIN_AREA(n)		((n) *  0x1000)

#define PAGE_2K				0x0800
#define OOB_64				0x0040

/*
 * NFC_CMD2[CODE] values. See section:
 *  - 31.4.7 Flash Command Code Description, Vybrid manual
 *  - 23.8.6 Flash Command Sequencer, MPC5125 manual
 *
 * Briefly these are bitmasks of controller cycles.
 */
#define READ_PAGE_CMD_CODE		0x7EE0
#define READ_ONFI_PARAM_CMD_CODE	0x4860
#define PROGRAM_PAGE_CMD_CODE		0x7FC0
#define ERASE_CMD_CODE			0x4EC0
#define READ_ID_CMD_CODE		0x4804
#define RESET_CMD_CODE			0x4040
#define STATUS_READ_CMD_CODE		0x4068

/* NFC ECC mode define */
#define ECC_BYPASS			0
#define ECC_45_BYTE			6
#define ECC_60_BYTE			7

/*** Register Mask and bit definitions */

/* NFC_FLASH_CMD1 Field */
#define CMD_BYTE2_MASK				0xFF000000
#define CMD_BYTE2_SHIFT				24

/* NFC_FLASH_CM2 Field */
#define CMD_BYTE1_MASK				0xFF000000
#define CMD_BYTE1_SHIFT				24
#define CMD_CODE_MASK				0x00FFFF00
#define CMD_CODE_SHIFT				8
#define BUFNO_MASK				0x00000006
#define BUFNO_SHIFT				1
#define START_BIT				(1<<0)

/* NFC_COL_ADDR Field */
#define COL_ADDR_MASK				0x0000FFFF
#define COL_ADDR_SHIFT				0

/* NFC_ROW_ADDR Field */
#define ROW_ADDR_MASK				0x00FFFFFF
#define ROW_ADDR_SHIFT				0
#define ROW_ADDR_CHIP_SEL_RB_MASK		0xF0000000
#define ROW_ADDR_CHIP_SEL_RB_SHIFT		28
#define ROW_ADDR_CHIP_SEL_MASK			0x0F000000
#define ROW_ADDR_CHIP_SEL_SHIFT			24

/* NFC_FLASH_STATUS2 Field */
#define STATUS_BYTE1_MASK			0x000000FF

/* NFC_FLASH_CONFIG Field */
#define CONFIG_ECC_SRAM_ADDR_MASK		0x7FC00000
#define CONFIG_ECC_SRAM_ADDR_SHIFT		22
#define CONFIG_ECC_SRAM_REQ_BIT			(1<<21)
#define CONFIG_DMA_REQ_BIT			(1<<20)
#define CONFIG_ECC_MODE_MASK			0x000E0000
#define CONFIG_ECC_MODE_SHIFT			17
#define CONFIG_FAST_FLASH_BIT			(1<<16)
#define CONFIG_16BIT				(1<<7)
#define CONFIG_BOOT_MODE_BIT			(1<<6)
#define CONFIG_ADDR_AUTO_INCR_BIT		(1<<5)
#define CONFIG_BUFNO_AUTO_INCR_BIT		(1<<4)
#define CONFIG_PAGE_CNT_MASK			0xF
#define CONFIG_PAGE_CNT_SHIFT			0

/* NFC_IRQ_STATUS Field */
#define IDLE_IRQ_BIT				(1<<29)
#define IDLE_EN_BIT				(1<<20)
#define CMD_DONE_CLEAR_BIT			(1<<18)
#define IDLE_CLEAR_BIT				(1<<17)

#define NFC_TIMEOUT		(HZ)

/* ECC status placed at end of buffers. */
#define ECC_SRAM_ADDR	((PAGE_2K+256-8) >> 3)
#define ECC_STATUS_MASK	0x80
#define ECC_ERR_COUNT	0x3F

/*
 * ECC status is stored at NFC_CFG[ECCADD] +4 for little-endian
 * and +7 for big-endian SoCs.
 */
#ifdef CONFIG_SOC_VF610
#define ECC_OFFSET	4
#else
#define ECC_OFFSET	7
#endif

struct vf610_nfc_config {
	bool hardware_ecc;
	int ecc_strength;
	int ecc_step_size;
	int width;
	int flash_bbt;
	u32 ecc_mode;
};

struct vf610_nfc {
	struct mtd_info mtd;
	struct nand_chip chip;
	struct device *dev;
	void __iomem *regs;
	wait_queue_head_t irq_waitq;
	uint buf_offset;
	int page_sz;
	/* Status and ID are in alternate locations. */
	int alt_buf;
#define ALT_BUF_ID   1
#define ALT_BUF_STAT 2
#define ALT_BUF_ONFI 3
	struct clk *clk;

	struct vf610_nfc_config cfg;
};

#define mtd_to_nfc(_mtd) container_of(_mtd, struct vf610_nfc, mtd)

static struct nand_ecclayout vf610_nfc_ecc45 = {
	.eccbytes = 45,
	.eccpos = {19, 20, 21, 22, 23,
		   24, 25, 26, 27, 28, 29, 30, 31,
		   32, 33, 34, 35, 36, 37, 38, 39,
		   40, 41, 42, 43, 44, 45, 46, 47,
		   48, 49, 50, 51, 52, 53, 54, 55,
		   56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {
		{.offset = 2,
		 .length = 17} }
};

static struct nand_ecclayout vf610_nfc_ecc60 = {
	.eccbytes = 60,
	.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11,
		   12, 13, 14, 15, 16, 17, 18, 19,
		   20, 21, 22, 23, 24, 25, 26, 27,
		   28, 29, 30, 31, 32, 33, 34, 35,
		   36, 37, 38, 39, 40, 41, 42, 43,
		   44, 45, 46, 47, 48, 49, 50, 51,
		   52, 53, 54, 55, 56, 57, 58, 59,
		   60, 61, 62, 63 },
	.oobfree = {
		{.offset = 2,
		 .length = 2} }
};

static inline u32 vf610_nfc_read(struct vf610_nfc *nfc, uint reg)
{
	return readl(nfc->regs + reg);
}

static inline void vf610_nfc_write(struct vf610_nfc *nfc, uint reg, u32 val)
{
	writel(val, nfc->regs + reg);
}

static inline void vf610_nfc_set(struct vf610_nfc *nfc, uint reg, u32 bits)
{
	vf610_nfc_write(nfc, reg, vf610_nfc_read(nfc, reg) | bits);
}

static inline void vf610_nfc_clear(struct vf610_nfc *nfc, uint reg, u32 bits)
{
	vf610_nfc_write(nfc, reg, vf610_nfc_read(nfc, reg) & ~bits);
}

static inline void vf610_nfc_set_field(struct vf610_nfc *nfc, u32 reg,
				       u32 mask, u32 shift, u32 val)
{
	vf610_nfc_write(nfc, reg,
			(vf610_nfc_read(nfc, reg) & (~mask)) | val << shift);
}

static inline void vf610_nfc_memcpy(void *dst, const void *src, size_t n)
{
	/*
	 * Use this accessor for the interal SRAM buffers. On ARM we can
	 * treat the SRAM buffer as if its memory, hence use memcpy
	 */
	memcpy(dst, src, n);
}

/* Clear flags for upcoming command */
static inline void vf610_nfc_clear_status(struct vf610_nfc *nfc)
{
	void __iomem *reg = nfc->regs + NFC_IRQ_STATUS;
	u32 tmp = __raw_readl(reg);

	tmp |= CMD_DONE_CLEAR_BIT | IDLE_CLEAR_BIT;
	__raw_writel(tmp, reg);
}

static inline void vf610_nfc_done(struct vf610_nfc *nfc)
{
	int rv;

	/*
	 * Barrier is needed after this write. This write need
	 * to be done before reading the next register the first
	 * time.
	 * vf610_nfc_set implicates such a barrier by using writel
	 * to write to the register.
	 */
	vf610_nfc_set(nfc, NFC_IRQ_STATUS, IDLE_EN_BIT);
	vf610_nfc_set(nfc, NFC_FLASH_CMD2, START_BIT);

	if (!(vf610_nfc_read(nfc, NFC_IRQ_STATUS) & IDLE_IRQ_BIT)) {
		rv = wait_event_timeout(nfc->irq_waitq,
			(vf610_nfc_read(nfc, NFC_IRQ_STATUS) & IDLE_IRQ_BIT),
					NFC_TIMEOUT);
		if (!rv)
			dev_warn(nfc->dev, "Timeout while waiting for BUSY.\n");
	}
	vf610_nfc_clear_status(nfc);
}

static u8 vf610_nfc_get_id(struct vf610_nfc *nfc, int col)
{
	u32 flash_id;

	if (col < 4) {
		flash_id = vf610_nfc_read(nfc, NFC_FLASH_STATUS1);
		return (flash_id >> (3-col)*8) & 0xff;
	} else {
		flash_id = vf610_nfc_read(nfc, NFC_FLASH_STATUS2);
		return flash_id >> 24;
	}
}

static u8 vf610_nfc_get_status(struct vf610_nfc *nfc)
{
	return vf610_nfc_read(nfc, NFC_FLASH_STATUS2) & STATUS_BYTE1_MASK;
}

static void vf610_nfc_send_command(struct vf610_nfc *nfc, u32 cmd_byte1,
				   u32 cmd_code)
{
	void __iomem *reg = nfc->regs + NFC_FLASH_CMD2;
	u32 tmp;

	vf610_nfc_clear_status(nfc);

	tmp = __raw_readl(reg);
	tmp &= ~(CMD_BYTE1_MASK | CMD_CODE_MASK | BUFNO_MASK);
	tmp |= cmd_byte1 << CMD_BYTE1_SHIFT;
	tmp |= cmd_code << CMD_CODE_SHIFT;
	__raw_writel(tmp, reg);
}

static void vf610_nfc_send_commands(struct vf610_nfc *nfc, u32 cmd_byte1,
				    u32 cmd_byte2, u32 cmd_code)
{
	void __iomem *reg = nfc->regs + NFC_FLASH_CMD1;
	u32 tmp;

	vf610_nfc_send_command(nfc, cmd_byte1, cmd_code);

	tmp = __raw_readl(reg);
	tmp &= ~CMD_BYTE2_MASK;
	tmp |= cmd_byte2 << CMD_BYTE2_SHIFT;
	__raw_writel(tmp, reg);
}

static irqreturn_t vf610_nfc_irq(int irq, void *data)
{
	struct mtd_info *mtd = data;
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);

	vf610_nfc_clear(nfc, NFC_IRQ_STATUS, IDLE_EN_BIT);
	wake_up(&nfc->irq_waitq);

	return IRQ_HANDLED;
}

static void vf610_nfc_addr_cycle(struct vf610_nfc *nfc, int column, int page)
{
	if (column != -1) {
		if (nfc->chip.options & NAND_BUSWIDTH_16)
			column = column / 2;
		vf610_nfc_set_field(nfc, NFC_COL_ADDR, COL_ADDR_MASK,
				    COL_ADDR_SHIFT, column);
	}
	if (page != -1)
		vf610_nfc_set_field(nfc, NFC_ROW_ADDR, ROW_ADDR_MASK,
				    ROW_ADDR_SHIFT, page);
}

static inline void vf610_nfc_ecc_mode(struct vf610_nfc *nfc, int ecc_mode)
{
	vf610_nfc_set_field(nfc, NFC_FLASH_CONFIG,
			    CONFIG_ECC_MODE_MASK,
			    CONFIG_ECC_MODE_SHIFT, ecc_mode);
}

static inline void vf610_nfc_transfer_size(void __iomem *regbase, int size)
{
	__raw_writel(size, regbase + NFC_SECTOR_SIZE);
}

static void vf610_nfc_command(struct mtd_info *mtd, unsigned command,
			      int column, int page)
{
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);
	int page_sz = nfc->chip.options & NAND_BUSWIDTH_16 ? 1 : 0;

	nfc->buf_offset = max(column, 0);
	nfc->alt_buf = 0;

	switch (command) {
	case NAND_CMD_SEQIN:
		/* Use valid column/page from preread... */
		vf610_nfc_addr_cycle(nfc, column, page);
		/*
		 * SEQIN => data => PAGEPROG sequence is done by the controller
		 * hence we do not need to issue the command here...
		 */
		return;
	case NAND_CMD_PAGEPROG:
		page_sz += mtd->writesize + mtd->oobsize;
		vf610_nfc_transfer_size(nfc->regs, page_sz);
		vf610_nfc_send_commands(nfc, NAND_CMD_SEQIN,
					command, PROGRAM_PAGE_CMD_CODE);
		vf610_nfc_ecc_mode(nfc, nfc->cfg.ecc_mode);
		break;

	case NAND_CMD_RESET:
		vf610_nfc_transfer_size(nfc->regs, 0);
		vf610_nfc_send_command(nfc, command, RESET_CMD_CODE);
		break;

	case NAND_CMD_READOOB:
		page_sz += mtd->oobsize;
		column = mtd->writesize;
		vf610_nfc_transfer_size(nfc->regs, page_sz);
		vf610_nfc_send_commands(nfc, NAND_CMD_READ0,
					NAND_CMD_READSTART, READ_PAGE_CMD_CODE);
		vf610_nfc_addr_cycle(nfc, column, page);

		/* Disable ECC when reading OOB only */
		vf610_nfc_ecc_mode(nfc, ECC_BYPASS);
		break;

	case NAND_CMD_READ0:
		page_sz += mtd->writesize + mtd->oobsize;
		column = 0;
		vf610_nfc_transfer_size(nfc->regs, page_sz);
		vf610_nfc_send_commands(nfc, NAND_CMD_READ0,
					NAND_CMD_READSTART, READ_PAGE_CMD_CODE);
		vf610_nfc_addr_cycle(nfc, column, page);
		vf610_nfc_ecc_mode(nfc, nfc->cfg.ecc_mode);
		break;

	case NAND_CMD_PARAM:
		nfc->alt_buf = ALT_BUF_ONFI;
		vf610_nfc_transfer_size(nfc->regs, 768);
		vf610_nfc_send_command(nfc, NAND_CMD_PARAM, READ_ONFI_PARAM_CMD_CODE);
		vf610_nfc_set_field(nfc, NFC_ROW_ADDR, ROW_ADDR_MASK,
				    ROW_ADDR_SHIFT, column);
		vf610_nfc_ecc_mode(nfc, ECC_BYPASS);
		break;

	case NAND_CMD_ERASE1:
		vf610_nfc_transfer_size(nfc->regs, 0);
		vf610_nfc_send_commands(nfc, command,
					NAND_CMD_ERASE2, ERASE_CMD_CODE);
		vf610_nfc_addr_cycle(nfc, column, page);
		break;

	case NAND_CMD_READID:
		nfc->alt_buf = ALT_BUF_ID;
		nfc->buf_offset = 0;
		vf610_nfc_transfer_size(nfc->regs, 0);
		vf610_nfc_send_command(nfc, command, READ_ID_CMD_CODE);
		vf610_nfc_set_field(nfc, NFC_ROW_ADDR, ROW_ADDR_MASK,
				    ROW_ADDR_SHIFT, column);
		break;

	case NAND_CMD_STATUS:
		nfc->alt_buf = ALT_BUF_STAT;
		vf610_nfc_transfer_size(nfc->regs, 0);
		vf610_nfc_send_command(nfc, command, STATUS_READ_CMD_CODE);
		break;
	default:
		return;
	}

	vf610_nfc_done(nfc);
}

static void vf610_nfc_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);
	uint c = nfc->buf_offset;

	/* Alternate buffers are only supported through read_byte */
	WARN_ON(nfc->alt_buf);

	vf610_nfc_memcpy(buf, nfc->regs + NFC_MAIN_AREA(0) + c, len);

	nfc->buf_offset += len;
}

static void vf610_nfc_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);
	uint c = nfc->buf_offset;
	uint l;

	l = min_t(uint, len, mtd->writesize + mtd->oobsize - c);
	nfc->buf_offset += l;
	vf610_nfc_memcpy(nfc->regs + NFC_MAIN_AREA(0) + c, buf, l);
}

static uint8_t vf610_nfc_read_byte(struct mtd_info *mtd)
{
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);
	u8 tmp;
	uint c = nfc->buf_offset;

	switch (nfc->alt_buf) {
	case ALT_BUF_ID:
		tmp = vf610_nfc_get_id(nfc, c);
		break;
	case ALT_BUF_STAT:
		tmp = vf610_nfc_get_status(nfc);
		break;
	case ALT_BUF_ONFI:
		/* Reverse byte since the controller uses big endianness */
		c = nfc->buf_offset % 4;
		c = nfc->buf_offset - c + (3 - c);
		tmp = *((u8 *)(nfc->regs + NFC_MAIN_AREA(0) + c));
		break;
	default:
		tmp = *((u8 *)(nfc->regs + NFC_MAIN_AREA(0) + c));
		break;
	}
	nfc->buf_offset++;
	return tmp;
}

static u16 vf610_nfc_read_word(struct mtd_info *mtd)
{
	u16 tmp;

	vf610_nfc_read_buf(mtd, (u_char *)&tmp, sizeof(tmp));
	return tmp;
}

/* If not provided, upper layers apply a fixed delay. */
static int vf610_nfc_dev_ready(struct mtd_info *mtd)
{
	/* NFC handles R/B internally; always ready.  */
	return 1;
}

/*
 * This function supports Vybrid only (MPC5125 would have full RB and four CS)
 */
static void vf610_nfc_select_chip(struct mtd_info *mtd, int chip)
{
#ifdef CONFIG_SOC_VF610
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);
	u32 tmp = vf610_nfc_read(nfc, NFC_ROW_ADDR);

	tmp &= ~(ROW_ADDR_CHIP_SEL_RB_MASK | ROW_ADDR_CHIP_SEL_MASK);
	tmp |= 1 << ROW_ADDR_CHIP_SEL_RB_SHIFT;

	if (chip == 0)
		tmp |= 1 << ROW_ADDR_CHIP_SEL_SHIFT;
	else if (chip == 1)
		tmp |= 2 << ROW_ADDR_CHIP_SEL_SHIFT;

	vf610_nfc_write(nfc, NFC_ROW_ADDR, tmp);
#endif
}

/* Count the number of 0's in buff up to max_bits */
static inline int count_written_bits(uint8_t *buff, int size, int max_bits)
{
	uint32_t *buff32 = (uint32_t *)buff;
	int k, written_bits = 0;

	for (k = 0; k < (size / 4); k++) {
		written_bits += hweight32(~buff32[k]);
		if (written_bits > max_bits)
			break;
	}

	return written_bits;
}

static inline int vf610_nfc_correct_data(struct mtd_info *mtd, uint8_t *dat,
					 uint8_t *read_ecc, uint8_t *calc_ecc)
{
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);
	u8 ecc_status;
	u8 ecc_count;
	int flip;

	ecc_status = __raw_readb(nfc->regs + ECC_SRAM_ADDR * 8 + ECC_OFFSET);
	ecc_count = ecc_status & ECC_ERR_COUNT;
	if (!(ecc_status & ECC_STATUS_MASK))
		return ecc_count;

	/*
	 * On an erased page, bit count should be zero or at least
	 * less then half of the ECC strength
	 */
	flip = count_written_bits(dat, nfc->chip.ecc.size, ecc_count);

	if (flip > ecc_count && flip > (nfc->chip.ecc.strength / 2))
		return -1;

	/* Erased page. */
	memset(dat, 0xff, nfc->chip.ecc.size);
	return 0;
}

static int vf610_nfc_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				   u_char *ecc_code)
{
	return 0;
}

static void vf610_nfc_enable_hwecc(struct mtd_info *mtd, int mode)
{
}

#ifdef CONFIG_OF_MTD
static const struct of_device_id vf610_nfc_dt_ids[] = {
	{ .compatible = "fsl,vf610-nfc" },
	{ .compatible = "fsl,mpc5125-nfc" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vf610_nfc_dt_ids);

static int vf610_nfc_probe_dt(struct device *dev, struct vf610_nfc_config *cfg)
{
	struct device_node *np = dev->of_node;
	int buswidth;

	if (!np)
		return 1;

	if (of_get_nand_ecc_mode(np) >= NAND_ECC_HW)
		cfg->hardware_ecc = 1;

	cfg->flash_bbt = of_get_nand_on_flash_bbt(np);
	cfg->ecc_strength = of_get_nand_ecc_strength(np);
	if (cfg->ecc_strength < 0)
		cfg->ecc_strength = 0;

	cfg->ecc_step_size = of_get_nand_ecc_step_size(np);
	if (cfg->ecc_step_size < 0)
		cfg->ecc_step_size = 0;

	buswidth = of_get_nand_bus_width(np);
	if (buswidth < 0)
		return buswidth;

	cfg->width = buswidth;

	return 0;
}
#else
static int vf610_nfc_probe_dt(struct device *dev, struct vf610_nfc_config *cfg)
{
	return 0;
}
#endif

static int vf610_nfc_init_controller(struct vf610_nfc *nfc)
{
	struct vf610_nfc_config *cfg = &nfc->cfg;

	if (cfg->width == 16)
		vf610_nfc_set(nfc, NFC_FLASH_CONFIG, CONFIG_16BIT);
	else
		vf610_nfc_clear(nfc, NFC_FLASH_CONFIG, CONFIG_16BIT);

	/* Set configuration register. */
	vf610_nfc_clear(nfc, NFC_FLASH_CONFIG, CONFIG_ADDR_AUTO_INCR_BIT);
	vf610_nfc_clear(nfc, NFC_FLASH_CONFIG, CONFIG_BUFNO_AUTO_INCR_BIT);
	vf610_nfc_clear(nfc, NFC_FLASH_CONFIG, CONFIG_BOOT_MODE_BIT);
	vf610_nfc_clear(nfc, NFC_FLASH_CONFIG, CONFIG_DMA_REQ_BIT);
	vf610_nfc_set(nfc, NFC_FLASH_CONFIG, CONFIG_FAST_FLASH_BIT);

	/* PAGE_CNT = 1 */
	vf610_nfc_set_field(nfc, NFC_FLASH_CONFIG, CONFIG_PAGE_CNT_MASK,
			    CONFIG_PAGE_CNT_SHIFT, 1);

	if (cfg->hardware_ecc) {
		/* Set ECC_STATUS offset */
		vf610_nfc_set_field(nfc, NFC_FLASH_CONFIG,
				    CONFIG_ECC_SRAM_ADDR_MASK,
				    CONFIG_ECC_SRAM_ADDR_SHIFT, ECC_SRAM_ADDR);

		/* Enable ECC_STATUS */
		vf610_nfc_set(nfc, NFC_FLASH_CONFIG, CONFIG_ECC_SRAM_REQ_BIT);
	}

	return 0;
}

static int vf610_nfc_probe(struct platform_device *pdev)
{
	struct vf610_nfc *nfc;
	struct resource *res;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	struct vf610_nfc_config *cfg;
	int err = 0;
	int irq;

	nfc = devm_kzalloc(&pdev->dev, sizeof(*nfc), GFP_KERNEL);
	if (!nfc)
		return -ENOMEM;

	cfg = &nfc->cfg;

	nfc->dev = &pdev->dev;
	mtd = &nfc->mtd;
	chip = &nfc->chip;

	mtd->priv = chip;
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = nfc->dev;
	mtd->name = DRV_NAME;

	err = vf610_nfc_probe_dt(nfc->dev, cfg);
	if (err)
		return -ENODEV;

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	nfc->regs = devm_ioremap_resource(nfc->dev, res);
	if (IS_ERR(nfc->regs))
		return PTR_ERR(nfc->regs);

	nfc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(nfc->clk))
		return PTR_ERR(nfc->clk);

	err = clk_prepare_enable(nfc->clk);
	if (err) {
		dev_err(nfc->dev, "Unable to enable clock!\n");
		return err;
	}

	if (cfg->width == 16)
		chip->options |= NAND_BUSWIDTH_16;
	else
		chip->options &= ~NAND_BUSWIDTH_16;

	chip->dev_ready = vf610_nfc_dev_ready;
	chip->cmdfunc = vf610_nfc_command;
	chip->read_byte = vf610_nfc_read_byte;
	chip->read_word = vf610_nfc_read_word;
	chip->read_buf = vf610_nfc_read_buf;
	chip->write_buf = vf610_nfc_write_buf;
	chip->select_chip = vf610_nfc_select_chip;

	/* Bad block options. */
	if (cfg->flash_bbt)
		chip->bbt_options = NAND_BBT_USE_FLASH | NAND_BBT_NO_OOB |
				    NAND_BBT_CREATE;

	init_waitqueue_head(&nfc->irq_waitq);

	err = devm_request_irq(nfc->dev, irq, vf610_nfc_irq, 0, DRV_NAME, mtd);
	if (err) {
		dev_err(nfc->dev, "Error requesting IRQ!\n");
		goto error;
	}

	vf610_nfc_init_controller(nfc);

	/* first scan to find the device and get the page size */
	if (nand_scan_ident(mtd, 1, NULL)) {
		err = -ENXIO;
		goto error;
	}

	chip->ecc.mode = NAND_ECC_SOFT; /* default */

	/* Single buffer only, max 256 OOB minus ECC status */
	if (mtd->writesize + mtd->oobsize > PAGE_2K + 256 - 8) {
		dev_err(nfc->dev, "Unsupported flash page size\n");
		err = -ENXIO;
		goto error;
	}

	if (cfg->hardware_ecc) {
		if (mtd->writesize != PAGE_2K && mtd->oobsize < 64) {
			dev_err(nfc->dev, "Unsupported flash with hwecc\n");
			err = -ENXIO;
			goto error;
		}

		if (cfg->ecc_step_size != mtd->writesize) {
			dev_err(nfc->dev, "Step size needs to be page size\n");
			err = -ENXIO;
			goto error;
		}

		if (cfg->ecc_strength == 32) {
			cfg->ecc_mode = ECC_60_BYTE;
			chip->ecc.bytes = 60;
			chip->ecc.layout = &vf610_nfc_ecc60;
		} else if (cfg->ecc_strength == 24) {
			cfg->ecc_mode = ECC_45_BYTE;
			chip->ecc.bytes = 45;
			chip->ecc.layout = &vf610_nfc_ecc45;
		} else {
			dev_err(nfc->dev, "Unsupported ECC strength\n");
			err = -ENXIO;
			goto error;
		}

		/* propagate ecc.layout to mtd_info */
		mtd->ecclayout = chip->ecc.layout;
		chip->ecc.calculate = vf610_nfc_calculate_ecc;
		chip->ecc.hwctl = vf610_nfc_enable_hwecc;
		chip->ecc.correct = vf610_nfc_correct_data;
		chip->ecc.mode = NAND_ECC_HW;

		chip->ecc.size = PAGE_2K;
		chip->ecc.strength = cfg->ecc_strength;
	}

	/* second phase scan */
	if (nand_scan_tail(mtd)) {
		err = -ENXIO;
		goto error;
	}

	/* Register device in MTD */
	mtd_device_parse_register(mtd, NULL,
		&(struct mtd_part_parser_data){
			.of_node = pdev->dev.of_node,
		},
		NULL, 0);

	platform_set_drvdata(pdev, mtd);

	return 0;

error:
	clk_disable_unprepare(nfc->clk);
	return err;
}

static int vf610_nfc_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);

	nand_release(mtd);
	clk_disable_unprepare(nfc->clk);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int vf610_nfc_suspend(struct device *dev)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);

	clk_disable_unprepare(nfc->clk);
	return 0;
}

static int vf610_nfc_resume(struct device *dev)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct vf610_nfc *nfc = mtd_to_nfc(mtd);

	pinctrl_pm_select_default_state(dev);

	clk_prepare_enable(nfc->clk);

	vf610_nfc_init_controller(nfc);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(vf610_nfc_pm_ops, vf610_nfc_suspend, vf610_nfc_resume);

static struct platform_driver vf610_nfc_driver = {
	.driver		= {
		.name	= DRV_NAME,
		.of_match_table = vf610_nfc_dt_ids,
		.pm	= &vf610_nfc_pm_ops,
	},
	.probe		= vf610_nfc_probe,
	.remove		= vf610_nfc_remove,
};

module_platform_driver(vf610_nfc_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale VF610/MPC5125 NFC MTD NAND driver");
MODULE_LICENSE("GPL");
