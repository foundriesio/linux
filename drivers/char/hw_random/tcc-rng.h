#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
#define TEE_TRNG
#elif defined(CONFIG_ARCH_TCC805X)
#define HSM_TRNG
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
	arm_smccc_smc(SIP_CRYPTO_TRNG, TRNG_GET_WORD_VALUE, 0, 0, 0, 0, 0, 0, &res); \
	rnd_word[0] = (u32) res.a0; \
	rnd_word_cnt = MAX_ENR_CNT; \
}

#elif defined(HSM_TRNG)

#include <linux/dma-mapping.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#include "../tcc_hsm/tcc805x/tcc_hsm_cmd.h"

#define MAX_ENR_CNT (8)

static dma_addr_t phy_rnd_word = 0;
static u32 *rnd_word;

#define trng_run() \
{ \
	tcc_hsm_cmd_get_rand(MBOX_DEV_HSM, REQ_HSM_GET_RNG, phy_rnd_word, MAX_ENR_CNT << 2); \
	rnd_word_cnt = MAX_ENR_CNT; \
}

#elif defined(HSB_TRNG)

#include <linux/clk.h>

#define MAX_ENR_CNT (6)

typedef	struct {
	volatile u32 nRNG_IMR;       /* 0x000 */
	volatile u32 nRNG_ISR;       /* 0x004 */
	volatile u32 nRNG_ICR;       /* 0x008 */
	volatile u32 nTRNG_CFG;      /* 0x00C */
	volatile u32 nTRNG_VALID;    /* 0x010 */
	volatile u32 nEHR_DATA0;     /* 0x014 */
	volatile u32 nEHR_DATA1;     /* 0x018 */
	volatile u32 nEHR_DATA2;     /* 0x01C */
	volatile u32 nEHR_DATA3;     /* 0x020 */
	volatile u32 nEHR_DATA4;     /* 0x024 */
	volatile u32 nEHR_DATA5;     /* 0x028 */
	volatile u32 nRND_SRC_EN;    /* 0x02C */
	volatile u32 nSAMPLE_CNT1;   /* 0x030 */
	volatile u32 nAUTO_STATIC;   /* 0x034 */
	volatile u32 nTRNG_DBG_CTRL; /* 0x038 */
	volatile u32 nTRNG_SW_RST;   /* 0x040 */
	volatile u32 _empty1[29];    /* 0x044 ~ 0x0B7 */
	volatile u32 nTRNG_BUSY;     /* 0x0B8 */
	volatile u32 nRST_BITS_CNT;  /* 0x0BC */
	volatile u32 _empty2[8];     /* 0x0C0 ~ 0x0DF */
	volatile u32 nRST_BIST_CNT0; /* 0x0E0 */
	volatile u32 nRST_BIST_CNT1; /* 0x0E4 */
	volatile u32 nRST_BIST_CNT2; /* 0x0E8 */
} TRNG;

static struct clk *trng_clk;
static void __iomem *trng_regs;

static u32 rnd_word[MAX_ENR_CNT];

#define trng_run() \
{ \
	volatile u32 nReg; \
	TRNG *HwTRNG = (TRNG*)trng_regs; \
 \
	HwTRNG->nTRNG_SW_RST = 1; \
	HwTRNG->nRNG_IMR = 0xE; \
	HwTRNG->nTRNG_CFG = 2; \
	HwTRNG->nSAMPLE_CNT1 = 0x51; \
	HwTRNG->nTRNG_DBG_CTRL = 0xF; \
	HwTRNG->nRND_SRC_EN = 1; \
 \
	nReg = HwTRNG->nTRNG_VALID; \
	while((nReg&3)==0) \
	{ \
		nReg = HwTRNG->nRNG_ISR; \
	} \
	HwTRNG->nRNG_ICR = 0xFFFFFFFF; \
	rnd_word[0] = HwTRNG->nEHR_DATA0; \
	rnd_word[1] = HwTRNG->nEHR_DATA1; \
	rnd_word[2] = HwTRNG->nEHR_DATA2; \
	rnd_word[3] = HwTRNG->nEHR_DATA3; \
	rnd_word[4] = HwTRNG->nEHR_DATA4; \
	rnd_word[5] = HwTRNG->nEHR_DATA5; \
	rnd_word_cnt = MAX_ENR_CNT; \
 \
	nReg = HwTRNG->nRNG_ISR; \
	HwTRNG->nRND_SRC_EN = 0; \
}

#endif
