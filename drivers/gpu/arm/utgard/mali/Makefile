#
# Copyright (C) 2013-2017 Telechips Inc.
# Copyright (C) 2010-2017 ARM Limited. All rights reserved.
# 
# This program is free software and is provided to you under the terms of the GNU General Public License version 2
# as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
# 
# A copy of the licence is included with the program, and can also be obtained from Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

# The Makefile sets up "arch" based on the CONFIG, creates the version info
# string and the __malidrv_build_info.c file, and then call the Linux build
# system to actually build the driver. After that point the Kbuild file takes
# over.

# set up defaults if not defined by the user
ARCH = arm
BUILD = release
TARGET_PLATFORM = tcc-m400

OSKOS=linux
FILES_PREFIX=


# Include the mapping between TARGET_PLATFORM and KDIR + MALI_PLATFORM
-include MALI_CONFIGURATION
export KDIR ?= $(KDIR-$(TARGET_PLATFORM))
export MALI_PLATFORM ?= $(MALI_PLATFORM-$(TARGET_PLATFORM))

ifneq ($(TARGET_PLATFORM),)
ifeq ($(MALI_PLATFORM),)
$(error "Invalid TARGET_PLATFORM: $(TARGET_PLATFORM)")
endif
endif

# validate lookup result
ifeq ($(KDIR),)
$(error No KDIR found for platform $(TARGET_PLATFORM))
endif

ifeq ($(CONFIG_MALI400_UMP),y)
UMP_SYMVERS_FILE ?= ../ump/Module.symvers
KBUILD_EXTRA_SYMBOLS = $(realpath $(UMP_SYMVERS_FILE))
$(warning $(KBUILD_EXTRA_SYMBOLS))
endif

# Define host system directory
KDIR-$(shell uname -m):=/lib/modules/$(shell uname -r)/build

include $(KDIR)/.config


# report detected/selected settings
ifdef ARM_INTERNAL_BUILD
$(warning TARGET_PLATFORM $(TARGET_PLATFORM))
$(warning KDIR $(KDIR))
$(warning MALI_PLATFORM $(MALI_PLATFORM))
endif


ifeq ($(USING_PROFILING),1)
ifeq ($(CONFIG_TRACEPOINTS),)
$(warning CONFIG_TRACEPOINTS required for profiling)
else
export CONFIG_MALI400_PROFILING=y
export EXTRA_DEFINES += -DCONFIG_MALI400_PROFILING=1
ifeq ($(USING_INTERNAL_PROFILING),1)
export CONFIG_MALI400_INTERNAL_PROFILING=y
export EXTRA_DEFINES += -DCONFIG_MALI400_INTERNAL_PROFILING=1
endif
ifeq ($(MALI_HEATMAPS_ENABLED),1)
export MALI_HEATMAPS_ENABLED=y
export EXTRA_DEFINES += -DCONFIG_MALI400_HEATMAPS_ENABLED
endif
endif
endif

ifeq ($(MALI_DMA_BUF_MAP_ON_ATTACH),1)
export CONFIG_MALI_DMA_BUF_MAP_ON_ATTACH=y
export EXTRA_DEFINES += -DCONFIG_MALI_DMA_BUF_MAP_ON_ATTACH
endif

ifeq ($(MALI_SHARED_INTERRUPTS),1)
export CONFIG_MALI_SHARED_INTERRUPTS=y
export EXTRA_DEFINES += -DCONFIG_MALI_SHARED_INTERRUPTS
endif

ifeq ($(USING_DVFS),1)
export CONFIG_MALI_DVFS=y
export EXTRA_DEFINES += -DCONFIG_MALI_DVFS
endif

ifeq ($(USING_DMA_BUF_FENCE),1)
export CONFIG_MALI_DMA_BUF_FENCE=y
export EXTRA_DEFINES += -DCONFIG_MALI_DMA_BUF_FENCE
endif

ifeq ($(MALI_PMU_PARALLEL_POWER_UP),1)
export CONFIG_MALI_PMU_PARALLEL_POWER_UP=y
export EXTRA_DEFINES += -DCONFIG_MALI_PMU_PARALLEL_POWER_UP
endif

ifdef CONFIG_OF
ifeq ($(USING_DT),1)
export CONFIG_MALI_DT=y
export EXTRA_DEFINES += -DCONFIG_MALI_DT
endif
endif

ifeq ($(USING_DEVFREQ), 1)
ifdef CONFIG_PM_DEVFREQ
export CONFIG_MALI_DEVFREQ=y
export EXTRA_DEFINES += -DCONFIG_MALI_DEVFREQ=1
else
$(warning "You want to support DEVFREQ but kernel didn't support DEVFREQ.")
endif
endif

ifneq ($(BUILD),release)
# Debug
export CONFIG_MALI400_DEBUG=y
else
# Release
ifeq ($(MALI_QUIET),1)
export CONFIG_MALI_QUIET=y
export EXTRA_DEFINES += -DCONFIG_MALI_QUIET
endif
endif

ifeq ($(MALI_SKIP_JOBS),1)
EXTRA_DEFINES += -DPROFILING_SKIP_PP_JOBS=1 -DPROFILING_SKIP_GP_JOBS=1
endif

ifeq ($(MALI_MEM_SWAP_TRACKING),1)
EXTRA_DEFINES += -DMALI_MEM_SWAP_TRACKING=1
endif

all: $(UMP_SYMVERS_FILE)
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) modules
	@rm $(FILES_PREFIX)__malidrv_build_info.c $(FILES_PREFIX)__malidrv_build_info.o

clean:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) clean

kernelrelease:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) kernelrelease

export CONFIG KBUILD_EXTRA_SYMBOLS
