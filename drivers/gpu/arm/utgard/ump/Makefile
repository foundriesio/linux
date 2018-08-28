#
# Copyright (C) 2013-2017 Telechips Inc.
# Copyright (C) 2010-2012, 2014, 2017 ARM Limited. All rights reserved.
# 
# This program is free software and is provided to you under the terms of the GNU General Public License version 2
# as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
# 
# A copy of the licence is included with the program, and can also be obtained from Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

# For each arch check: CROSS_COMPILE , KDIR , CFLAGS += -DARCH

export ARCH ?= arm
BUILD = release

# Define host system directory
KDIR-$(shell uname -m):=/lib/modules/$(shell uname -r)/build

# look up KDIR based om CPU selection
KDIR ?= $(KDIR-$(CPU))

export CONFIG

ifeq ($(BUILD),debug)
export CONFIG_UMP_DEBUG := y
else
export CONFIG_UMP_DEBUG := n
endif

ifeq ($(KDIR),)
$(error No KDIR found for platform $(CPU))
endif

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

kernelrelease:
	$(MAKE) -C $(KDIR) kernelrelease

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	$(MAKE) -C $(KDIR) M=$(CURDIR)/../mali clean
