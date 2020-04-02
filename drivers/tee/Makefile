# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_TEE) += tee.o
tee-objs += tee_core.o
tee-objs += tee_shm.o
tee-objs += tee_shm_pool.o
ifeq ($(CONFIG_ARCH_TCC),y)
tee-objs += tee_lib_api.o
tee-objs += tee_trace.o
endif
obj-$(CONFIG_OPTEE) += optee/
