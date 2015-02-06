/*
 * Copyright (C) 2015 Linaro.
 * Leo Yan <leo.yan@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HISI_ACPU_CPUFREQ__
#define __HISI_ACPU_CPUFREQ__

extern unsigned int hisi_acpu_get_freq(void);
extern void hisi_acpu_set_freq(unsigned int freq);
extern int hisi_acpu_set_max_freq(unsigned int max_freq, unsigned int flag);

#endif /* __HISI_ACPU_CPUFREQ__ */
