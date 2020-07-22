override HOST_PRIMARY_ARCH := host_x86_64
override HOST_32BIT_ARCH := host_i386
override HOST_FORCE_32BIT := -m32
override HOST_ALL_ARCH := host_x86_64 host_i386
override TARGET_PRIMARY_ARCH := target_aarch64
override TARGET_SECONDARY_ARCH :=
override TARGET_ALL_ARCH := target_aarch64
override TARGET_FORCE_32BIT :=
override PVR_ARCH := volcanic
override METAG_VERSION_NEEDED := 2.8.1.0.3
override KERNELDIR := /home/B090157/Linux/ALS/tcc805x_linux_ivi/build/tcc8059-main/tmp/work-shared/tcc8059-main/kernel-source
override KERNEL_ID := 4.14.137-tcc
override PVRSRV_MODULE_BASEDIR := /lib/modules/4.14.137-tcc/extra/
override KERNEL_COMPONENTS := srvkm dc_fbdev
override KERNEL_CROSS_COMPILE := aarch64-telechips-linux-
override WINDOW_SYSTEM := nullws
override PVRSRV_MODNAME := pvrsrvkm
override PVRHMMU_MODNAME :=
override PVRSYNC_MODNAME := pvr_sync
override PVR_BUILD_DIR := tcc_9xtp_linux
ifeq ($(CONFIG_POWERVR_DEBUG),y)
override PVR_BUILD_TYPE := debug
else
override PVR_BUILD_TYPE := release
endif
override SUPPORT_RGX := 1
override DISPLAY_CONTROLLER := dc_fbdev
override PVR_SYSTEM := tcc_9xtp
override PVR_LOADER :=
ifeq ($(CONFIG_POWERVR_DEBUG),y)
override BUILD := debug
else
override BUILD := release
override DEBUGLINK := 1
endif
override SUPPORT_PHYSMEM_TEST := 1
override RGX_BNC := 27.V.254.2
ifeq ($(CONFIG_POWERVR_VZ),y)
override RGX_NUM_OS_SUPPORTED := 2
else
override RGX_NUM_OS_SUPPORTED := 1
endif
override VMM_TYPE := stub
override SUPPORT_POWMON_COMPONENT := 1
override SUPPORT_DISPLAY_CLASS := 1
override RGX_TIMECORR_CLOCK := mono
override PDVFS_COM_HOST := 1
override PDVFS_COM_AP := 2
override PDVFS_COM_PMC := 3
override PDVFS_COM_IMG_CLKDIV := 4
override PDVFS_COM := PDVFS_COM_HOST
override PVR_GPIO_MODE_GENERAL := 1
override PVR_GPIO_MODE_POWMON_PIN := 2
override PVR_GPIO_MODE := PVR_GPIO_MODE_GENERAL
override PVR_HANDLE_BACKEND := generic
override SUPPORT_DMABUF_BRIDGE := 1
override SUPPORT_FALLBACK_FENCE_SYNC := 1
