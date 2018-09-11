#ifndef __CPU_POWER_H__
#define __CPU_POWER_H__

enum {
	CPU_0 = 0,
	CPU_1,
	CPU_2,
	CPU_3
};

enum {
	CPU_PWUP = 0,
	CPU_PWDN
};

extern int tcc_cpu_pwdn(unsigned int cluster, unsigned int cpu, unsigned int pwdn);
extern int tcc_cluster_pwdn(unsigned int cluster, unsigned int pwdn);

#endif /* __CPU_POWER_H__ */
