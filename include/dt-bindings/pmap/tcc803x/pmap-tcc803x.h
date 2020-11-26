// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2020 Telechips Inc.
 *
 * This file will be removed after all the file related to pmap is seperated according to the new pmap strategy.
 */
#ifndef DT_BINDINGS_PMAP_TCC803X_H
#define DT_BINDINGS_PMAP_TCC803X_H

#include "../../../generated/autoconf.h"

//#if defined(CONFIG_ARCH_TCC803X)

#if defined(CONFIG_ANDROID)
#include "pmap-tcc803x-android-ivi.h"
#else
#include "pmap-tcc803x-linux-ivi.h"
#endif

//#endif//defined(CONFIG_ARCH_TCC803X)

#endif//DT_BINDINGS_PMAP_TCC803X_H
