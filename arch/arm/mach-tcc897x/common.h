#ifndef __MACH_TCC897X_COMMON_H
#define __MACH_TCC897X_COMMON_H

extern struct smp_operations tcc_smp_ops;
extern void __init early_print(const char *str, ...);
extern void __init tcc_map_common_io(void);

extern void __init tcc_init_early(void);
extern void __init tcc_mem_reserve(void);
extern void __init tcc_irq_dt_init(void);

extern void tcc897x_restart(char mode, const char *cmd);
#endif
