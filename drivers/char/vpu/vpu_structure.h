
#ifndef _STRUCTURES_VIDEO_H_
#define _STRUCTURES_VIDEO_H_

#include "vpu_type.h"

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#define HwVIDEOBUSCONFIG_BASE                   (0x75100000)
#else //CONFIG_ARCH_TCC898X , CONFIG_ARCH_TCC802X
	#if defined(CONFIG_ARCH_TCC896X)
	#define HwVIDEOBUSCONFIG_BASE               (0x15200000)
	#else
	#define HwVIDEOBUSCONFIG_BASE               (0x15100000)
	#endif
#endif

#endif /* _STRUCTURES_VIDEO_H_ */

