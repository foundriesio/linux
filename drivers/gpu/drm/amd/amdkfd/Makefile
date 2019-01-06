#
# Makefile for Heterogenous System Architecture support for AMD GPU devices
#

ccflags-y := -Idrivers/gpu/drm/amd/include/  \
		-Idrivers/gpu/drm/amd/include/asic_reg

amdkfd-y	:= kfd_module.o kfd_device.o kfd_chardev.o kfd_topology.o \
		kfd_pasid.o kfd_doorbell.o kfd_flat_memory.o \
		kfd_process.o kfd_queue.o kfd_mqd_manager.o \
		kfd_mqd_manager_cik.o kfd_mqd_manager_vi.o \
		kfd_mqd_manager_v9.o \
		kfd_kernel_queue.o kfd_kernel_queue_cik.o \
		kfd_kernel_queue_vi.o kfd_kernel_queue_v9.o \
		kfd_packet_manager.o kfd_process_queue_manager.o \
		kfd_device_queue_manager.o kfd_device_queue_manager_cik.o \
		kfd_device_queue_manager_vi.o kfd_device_queue_manager_v9.o \
		kfd_interrupt.o kfd_events.o cik_event_interrupt.o \
		kfd_int_process_v9.o kfd_dbgdev.o kfd_dbgmgr.o kfd_crat.o

ifneq ($(CONFIG_AMD_IOMMU_V2),)
amdkfd-y += kfd_iommu.o
endif

amdkfd-$(CONFIG_DEBUG_FS) += kfd_debugfs.o

obj-$(CONFIG_HSA_AMD)	+= amdkfd.o
