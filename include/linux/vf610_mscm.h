#ifndef __VF610_MSCM__
#define __VF610_MSCM__

#include <linux/interrupt.h>

int mscm_request_cpu2cpu_irq(unsigned int intid, irq_handler_t handler,
			const char *name, void *priv);
void mscm_free_cpu2cpu_irq(unsigned int intid, void *priv);
void mscm_trigger_cpu2cpu_irq(unsigned int intid, int cpuid);
void mscm_enable_cpu2cpu_irq(unsigned int intid);
void mscm_disable_cpu2cpu_irq(unsigned int intid);

#endif /* __VF610_MSCM__ */
