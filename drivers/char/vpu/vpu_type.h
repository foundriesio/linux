
#ifndef _VPU_TYPE_H_
#define _VPU_TYPE_H_

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC570X)
#define VPU_D6
#else
#define VPU_C7
#endif
#define JPU_C6

//#define VBUS_CLK_ALWAYS_ON
#define VBUS_CODA_CORE_CLK_CTRL

#endif /* _VPU_TYPE_H_ */

