// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2020 Telechips Inc.
 *
 * This file will be removed after all the file related to pmap is seperated according to the new pmap strategy.
 */
#ifndef DT_BINDINGS_PMAP_TCC805X_H
#define DT_BINDINGS_PMAP_TCC805X_H

#include "../../../generated/autoconf.h"

//#if defined(CONFIG_ARCH_TCC805X)

#if defined(CONFIG_ANDROID)
#include "pmap-tcc805x-android-ivi.h"
#elif defined(CONFIG_TCC805X_CLUSTER)
#include "pmap-tcc805x-linux-cluster.h"
//#elif defined(CONFIG_TCC805X_DVRS)
//#include "pmap-tcc805x-linux-dvrs.h"
#else
#include "pmap-tcc805x-linux-ivi.h"
#endif

//#endif//defined(CONFIG_ARCH_TCC805X)

#endif//DT_BINDINGS_PMAP_TCC805X_H
