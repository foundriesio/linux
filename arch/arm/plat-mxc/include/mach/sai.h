#ifndef __MACH_SAI_H
#define __MACH_SAI_H

struct mvf_sai_platform_data {
	unsigned int flags;
#define MVF_SAI_DMA            (1 << 0)
#define MVF_SAI_USE_AC97       (1 << 1)
#define MVF_SAI_NET            (1 << 2)
#define MVF_SAI_TRA_SYN        (1 << 3)
#define MVF_SAI_REC_SYN        (1 << 4)
#define MVF_SAI_USE_I2S_SLAVE  (1 << 5)
};

#endif /* __MACH_SAI_H */
