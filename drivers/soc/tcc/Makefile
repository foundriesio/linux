# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.

ifeq ($(CONFIG_TCC803X_CA7S)$(CONFIG_ARCH_TCC897X),)
obj-y	+= chip_info.o
endif

obj-y	+= timer.o irq-pic.o

obj-$(CONFIG_PMAP)		+= pmap.o
obj-$(CONFIG_TCC_BOOTSTAGE)	+= bootstage.o

obj-$(CONFIG_PM_TCC805X)	+= pm-tcc805x.o
