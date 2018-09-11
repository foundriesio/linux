
#include <asm/io.h>
#include <mach/iomap.h>
#include <mach/sram_map.h>
#include <mach/cpu_power.h>

#define PMU_SYSRST	0x010

int tcc_cpu_pwdn(unsigned int cluster, unsigned int cpu, unsigned int pwdn)
{
	void __iomem *rst_reg = IOMEM(io_p2v(TCC_PA_PMU + PMU_SYSRST));
	unsigned rst_mask;

	if (cluster != 0)
		return -1;

	switch (cpu) {
	case CPU_0:
		rst_mask = 1<<16;
		break;
	case CPU_1:
		rst_mask = 1<<15;
		break;
	case CPU_2:
		rst_mask = 1<<14;
		break;
	case CPU_3:
		rst_mask = 1<<13;
		break;
	default:
		return -1;
	}

	if (pwdn) {
		/* reset: reset mode */
		writel_relaxed(readl_relaxed(rst_reg) & ~rst_mask, rst_reg);

		/* cpu power down */
	}
	else {
		/* cpu power up */

		/* reset: normal mode */
		writel_relaxed(readl_relaxed(rst_reg) | rst_mask, rst_reg);
	}

	return 0;
}

int tcc_cluster_pwdn(unsigned int cluster, unsigned int pwdn)
{
	return 0;
}

