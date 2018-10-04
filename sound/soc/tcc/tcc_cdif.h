#ifndef _TCC_CDIF_H
#define _TCC_CDIF_H

#include <asm/io.h>
#include "tcc_audio_hw.h"

typedef enum {
	TCC_CDIF_BCLK_32FS_MODE = 32,
	TCC_CDIF_BCLK_48FS_MODE = 48,
	TCC_CDIF_BCLK_64FS_MODE = 64,
} TCC_CDIF_BCLK_FS_MODE;

typedef enum {
	TCC_CDIF_INTERFACE_IIS = 0,
	TCC_CDIF_INTERFACE_RIGHT_JUSTIFED = 1,
} TCC_CDIF_INTERFACE;

struct cdif_reg_t {
	uint32_t cicr;
};

#if 0 //DEBUG
#define cdif_writel(v,c)		({printk("<ASoC> CDIF_REG(0x%08x) = 0x%08x\n", c, v); writel(v,c); })
#else
#define cdif_writel(v,c)		writel(v,c)
#endif

static inline void tcc_cdif_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_CDIF_CICR_OFFSET);

	value &= ~CICR_CDIF_ENABLE_Msk;
	if (enable) {
		value |= CICR_CDIF_ENABLE;
	} else {
		value |= CICR_CDIF_DISABLE;
	}

	cdif_writel(value, base_addr + TCC_CDIF_CICR_OFFSET);
}

static inline void tcc_cdif_set_fs_mode(void __iomem *base_addr, TCC_CDIF_BCLK_FS_MODE fs_mode)
{
	uint32_t value = readl(base_addr + TCC_CDIF_CICR_OFFSET);

	value &= ~CICR_CDIF_BCLK_SEL_Msk;
	value |= (fs_mode == TCC_CDIF_BCLK_32FS_MODE) ? CICR_CDIF_BCLK_SEL_32FS : 
			 (fs_mode == TCC_CDIF_BCLK_48FS_MODE) ? CICR_CDIF_BCLK_SEL_48FS : CICR_CDIF_BCLK_SEL_64FS;

	cdif_writel(value, base_addr + TCC_CDIF_CICR_OFFSET);
}

static inline void tcc_cdif_set_interface(void __iomem *base_addr, TCC_CDIF_INTERFACE interface)
{
	uint32_t value = readl(base_addr + TCC_CDIF_CICR_OFFSET);

	value &= ~CICR_CDIF_INTERFACE_MODE_Msk;
	value |= (interface == TCC_CDIF_INTERFACE_RIGHT_JUSTIFED) ? CICR_CDIF_INTERFACE_RJUSTIFIED : CICR_CDIF_INTERFACE_IIS;

	cdif_writel(value, base_addr + TCC_CDIF_CICR_OFFSET);
}

static inline void tcc_cdif_set_bitclk_polarity(void __iomem *base_addr, bool positive)
{
	uint32_t value = readl(base_addr + TCC_CDIF_CICR_OFFSET);

	value &= ~CICR_CDIF_BCLK_POLARITY_Msk;
	if (positive) {
		value |= CICR_CDIF_BCLK_POSITIVE;
	} else {
		value |= CICR_CDIF_BCLK_NEGATIVE;
	}

	cdif_writel(value, base_addr + TCC_CDIF_CICR_OFFSET);
}

static inline void tcc_cdif_reg_backup(void __iomem *base_addr, struct cdif_reg_t *regs)
{
	regs->cicr = readl(base_addr + TCC_CDIF_CICR_OFFSET);
}

static inline void tcc_cdif_reg_restore(void __iomem *base_addr, struct cdif_reg_t *regs)
{
	cdif_writel(regs->cicr, base_addr + TCC_CDIF_CICR_OFFSET);
}

#endif /*_TCC_CDIF_H */
