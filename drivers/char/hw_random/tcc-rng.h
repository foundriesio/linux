// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#if defined(CONFIG_ARCH_TCC899X)
#define TEE_TRNG
#elif defined(CONFIG_ARCH_TCC805X)
#define HSM_TRNG
#elif defined(CONFIG_ARCH_TCC803X)
#if defined(CONFIG_TCC_HSM)
#define HSM_TRNG
#else
#define TEE_TRNG
#endif
#else
#define HSB_TRNG
#endif

#if defined(TEE_TRNG)

#include <linux/arm-smccc.h>

#define SIP_CRYPTO_TRNG 0x8200D000

#define TRNG_GET_WORD_VALUE 1
#define TRNG_GET_BYTE_VALUE 0

#define MAX_ENR_CNT (1)

static u32 rnd_word[MAX_ENR_CNT];

#define trng_run() \
{ \
	struct arm_smccc_res res; \
	\
	arm_smccc_smc(SIP_CRYPTO_TRNG, TRNG_GET_WORD_VALUE, 0, 0, 0, 0, 0, 0, \
		      &res); \
	rnd_word[0] = (u32) res.a0; \
	rnd_word_cnt = MAX_ENR_CNT; \
}

#elif defined(HSM_TRNG)

#include <linux/dma-mapping.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#if defined(CONFIG_ARCH_TCC803X)
#include "../tcc_hsm/tcc803x/tcc_hsm_sp_cmd.h"
#define MAX_ENR_CNT (4)
#else
#include "../tcc_hsm/tcc805x/tcc_hsm_cmd.h"
#define MAX_ENR_CNT (8)
#endif

static dma_addr_t phy_rnd_word;
static u32 *rnd_word;

#if defined(CONFIG_ARCH_TCC803X)

#define trng_run() \
{ \
	tcc_hsm_sp_cmd_get_rand(MBOX_DEV_M4, (char*)rnd_word, \
				 MAX_ENR_CNT << 2); \
	rnd_word_cnt = MAX_ENR_CNT; \
}

#else

#define trng_run() \
{ \
	tcc_hsm_cmd_get_rand(MBOX_DEV_HSM, REQ_HSM_GET_RNG, phy_rnd_word, \
				 MAX_ENR_CNT << 2); \
	rnd_word_cnt = MAX_ENR_CNT; \
}

#endif

#elif defined(HSB_TRNG)

#include <linux/io.h>
#include <linux/clk.h>

#define MAX_ENR_CNT (6)

#define TRNG_IMR (0x000)
#define TRNG_ISR (0x004)
#define TRNG_ICR (0x008)
#define TRNG_CFG (0x00C)
#define TRNG_VALID (0x010)
#define TRNG_EHR_DATA0 (0x014)
#define TRNG_EHR_DATA1 (0x018)
#define TRNG_EHR_DATA2 (0x01C)
#define TRNG_EHR_DATA3 (0x020)
#define TRNG_EHR_DATA4 (0x024)
#define TRNG_EHR_DATA5 (0x028)
#define TRNG_RND_SRC_EN (0x02C)
#define TRNG_SAMPLE_CNT1 (0x030)
#define TRNG_AUTOCORR_STATISTIC (0x034)
#define TRNG_DBG_CTRL (0x038)
#define TRNG_SW_RST (0x040)
#define TRNG_BUSY (0x0B8)
#define TRNG_RST_BITS_CNT (0x0BC)
#define TRNG_RST_BIST_CNT0 (0x0E0)
#define TRNG_RST_BIST_CNT1 (0x0E4)
#define TRNG_RST_BIST_CNT2 (0x0E8)

static struct clk *trng_clk;
static void __iomem *trng_regs;

static u32 rnd_word[MAX_ENR_CNT];

#define trng_run() \
{ \
	u32 nReg; \
	\
	writel_relaxed(1, trng_regs + TRNG_SW_RST); \
	writel_relaxed(0xE, trng_regs + TRNG_IMR); \
	writel_relaxed(2, trng_regs + TRNG_CFG); \
	writel_relaxed(0x51, trng_regs + TRNG_SAMPLE_CNT1); \
	writel_relaxed(0xF, trng_regs + TRNG_DBG_CTRL); \
	writel_relaxed(1, trng_regs + TRNG_RND_SRC_EN); \
	\
	nReg = readl_relaxed(trng_regs + TRNG_VALID); \
	while ((nReg&3) == 0) { \
		nReg = readl_relaxed(trng_regs + TRNG_ISR); \
	} \
	writel_relaxed(0xFFFFFFFF, trng_regs + TRNG_ICR); \
	rnd_word[0] = readl_relaxed(trng_regs + TRNG_EHR_DATA0); \
	rnd_word[1] = readl_relaxed(trng_regs + TRNG_EHR_DATA1); \
	rnd_word[2] = readl_relaxed(trng_regs + TRNG_EHR_DATA2); \
	rnd_word[3] = readl_relaxed(trng_regs + TRNG_EHR_DATA3); \
	rnd_word[4] = readl_relaxed(trng_regs + TRNG_EHR_DATA4); \
	rnd_word[5] = readl_relaxed(trng_regs + TRNG_EHR_DATA5); \
	rnd_word_cnt = MAX_ENR_CNT; \
	\
	nReg = readl_relaxed(trng_regs + TRNG_ISR); \
	writel_relaxed(0, trng_regs + TRNG_RND_SRC_EN); \
}

#endif
