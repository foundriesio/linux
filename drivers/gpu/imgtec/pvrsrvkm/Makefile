ifeq ($(CONFIG_ANDROID),y)
    ifeq ($(CONFIG_ARM64),y)
        include $(srctree)/drivers/gpu/imgtec/config_kernel_android64.mk
        ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_android64.h
    else
        include $(srctree)/drivers/gpu/imgtec/config_kernel_android32.mk
        ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_android32.h
    endif
else
    ifeq ($(CONFIG_POWERVR_DC_FBDEV),y)
        ifeq ($(CONFIG_ARM64),y)
            include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_fb.mk
            ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_fb.h
        else
            include $(srctree)/drivers/gpu/imgtec/config_kernel_linux32_fb.mk
            ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux32_fb.h
	endif
    else
        ifeq ($(CONFIG_ARM64),y)
            include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_drm.mk
            ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_drm.h
        else
            include $(srctree)/drivers/gpu/imgtec/config_kernel_linux32_drm.mk
            ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux32_drm.h
	endif    
    endif
endif

ccflags-y += -include include/generated/uapi/linux/version.h \
 -I$(srctree)/$(src)/$(PVR_ARCH)/hwdefs/km \
 -I$(srctree)/$(src)/$(PVR_ARCH)/hwdefs \
 -I$(srctree)/$(src)/$(PVR_ARCH) \
 -I$(srctree)/$(src)/.. \
 -I$(srctree)/$(src)
ccflags-$(CONFIG_X86) += -mno-soft-float

tmp := $(addprefix -I,$(wildcard $(srctree)/$(src)/$(PVR_ARCH)/generated/*))
ccflags-y += $(tmp)

ccflags-$(CONFIG_POWERVR_TC:m=y) += -I$(srctree)/drivers/gpu/imgtec/tc
ccflags-$(CONFIG_POWERVR_RK3368) += -I$(srctree)/drivers/gpu/imgtec/rk3368
ccflags-$(CONFIG_POWERVR_MT8173) += -I$(srctree)/drivers/gpu/imgtec/mt8173
ccflags-$(CONFIG_POWERVR_SUNXI) += -I$(srctree)/drivers/gpu/imgtec/sunxi
ccflags-$(CONFIG_POWERVR_TCC9XTP) += -I$(srctree)/drivers/gpu/imgtec/tcc_9xtp

ifeq ($(DISPLAY_CONTROLLER), dc_fbdev)
obj-$(CONFIG_POWERVR_DC_FBDEV) += dc_fbdev.o
dc_fbdev-y += dc_fbdev_tcc.o
endif

obj-$(CONFIG_POWERVR_ROGUE) += pvrsrvkm.o

pvrsrvkm-y += allocmem.o
pvrsrvkm-y += cache_km.o
pvrsrvkm-y += connection_server.o
pvrsrvkm-y += devicemem.o
pvrsrvkm-y += devicemem_heapcfg.o
pvrsrvkm-y += devicemem_history_server.o
pvrsrvkm-y += osmmap_stub.o
pvrsrvkm-y += devicemem_server.o
pvrsrvkm-y += devicemem_utils.o
pvrsrvkm-y += event.o
pvrsrvkm-y += fwload.o
pvrsrvkm-y += handle.o
ifeq ($(PVR_HANDLE_BACKEND),generic)
pvrsrvkm-y += handle_generic.o
else
ifeq ($(PVR_HANDLE_BACKEND),idr)
pvrsrvkm-y += handle_idr.o
endif
endif
pvrsrvkm-y += di_server.o
pvrsrvkm-y += hash.o
pvrsrvkm-y += htbserver.o
pvrsrvkm-y += htbuffer.o
pvrsrvkm-y += htb_debug.o
pvrsrvkm-y += info_page_km.o
pvrsrvkm-y += km_apphint.o
pvrsrvkm-y += lists.o
pvrsrvkm-y += mem_utils.o
pvrsrvkm-y += mmu_common.o
pvrsrvkm-y += module_common.o
pvrsrvkm-y += osconnection_server.o
pvrsrvkm-y += osfunc.o
pvrsrvkm-y += physheap.o
pvrsrvkm-y += physmem.o
pvrsrvkm-y += physmem_lma.o
pvrsrvkm-y += physmem_osmem_linux.o
pvrsrvkm-y += physmem_hostmem.o
pvrsrvkm-y += physmem_test.o
pvrsrvkm-y += pmr.o
pvrsrvkm-y += pmr_os.o
pvrsrvkm-y += power.o
pvrsrvkm-y += process_stats.o
pvrsrvkm-y += pvr_bridge_k.o
pvrsrvkm-y += pvr_debug.o
pvrsrvkm-y += pvr_notifier.o
pvrsrvkm-y += pvrsrv.o
pvrsrvkm-y += pvrsrv_bridge_init.o
pvrsrvkm-y += pvrsrv_error.o
pvrsrvkm-y += pvrsrv_pool.o
pvrsrvkm-y += ra.o
pvrsrvkm-y += rgxfwdbg.o
pvrsrvkm-y += rgxfwtrace_strings.o
pvrsrvkm-y += rgxshader.o
pvrsrvkm-y += rgxtimecorr.o
pvrsrvkm-y += srvcore.o
pvrsrvkm-y += sync.o
pvrsrvkm-y += sync_checkpoint.o
pvrsrvkm-y += sync_server.o
pvrsrvkm-y += tlclient.o
pvrsrvkm-y += tlintern.o
pvrsrvkm-y += tlserver.o
pvrsrvkm-y += tlstream.o
pvrsrvkm-y += uniq_key_splay_tree.o
pvrsrvkm-y += vmm_pvz_server.o
pvrsrvkm-y += vmm_pvz_client.o
pvrsrvkm-y += vz_vmm_pvz.o
pvrsrvkm-y += vz_vmm_vm.o

pvrsrvkm-y += $(PVR_ARCH)/rgxbvnc.o
pvrsrvkm-y += $(PVR_ARCH)/rgxccb.o
pvrsrvkm-y += $(PVR_ARCH)/rgxcompute.o
pvrsrvkm-y += $(PVR_ARCH)/rgxdebug.o
pvrsrvkm-y += $(PVR_ARCH)/rgxfwutils.o
pvrsrvkm-y += $(PVR_ARCH)/rgxhwperf.o
pvrsrvkm-y += $(PVR_ARCH)/rgxinit.o
pvrsrvkm-y += $(PVR_ARCH)/rgxkicksync.o
pvrsrvkm-y += $(PVR_ARCH)/rgxmem.o
pvrsrvkm-y += $(PVR_ARCH)/rgxmmuinit.o
pvrsrvkm-y += $(PVR_ARCH)/rgxmulticore.o
pvrsrvkm-y += $(PVR_ARCH)/rgxpower.o
pvrsrvkm-y += $(PVR_ARCH)/rgxregconfig.o
pvrsrvkm-y += $(PVR_ARCH)/rgxsignals.o
pvrsrvkm-y += $(PVR_ARCH)/rgxstartstop.o
pvrsrvkm-y += $(PVR_ARCH)/rgxsyncutils.o
pvrsrvkm-y += $(PVR_ARCH)/rgxta3d.o
pvrsrvkm-y += $(PVR_ARCH)/rgxtdmtransfer.o
pvrsrvkm-y += $(PVR_ARCH)/rgxutils.o
pvrsrvkm-y += $(PVR_ARCH)/vmm_type_stub.o

ifeq ($(PVR_ARCH),rogue)
pvrsrvkm-y += $(PVR_ARCH)/rgxbreakpoint.o
pvrsrvkm-y += $(PVR_ARCH)/rgxmipsmmuinit.o
pvrsrvkm-y += $(PVR_ARCH)/rgxtransfer.o
pvrsrvkm-y += $(PVR_ARCH)/rgx_hwperf_table.o
endif

# Kernel srvinit
pvrsrvkm-y += $(PVR_ARCH)/rgxfwimageutils.o
pvrsrvkm-y += $(PVR_ARCH)/rgxlayer_impl.o
pvrsrvkm-y += $(PVR_ARCH)/rgxsrvinit.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/dpdump_bridge/client_pdump_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/dpdumpctrl_bridge/client_pdumpctrl_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/drgxpdump_bridge/client_rgxpdump_bridge.o

pvrsrvkm-y += ../pvr_platform_drv.o

pvrsrvkm-$(CONFIG_DRM) += ../pvr_drm.o

pvrsrvkm-$(CONFIG_DMA_SHARED_BUFFER) += physmem_dmabuf.o

# Platform specific
pvrsrvkm-$(CONFIG_POWERVR_TC:m=y) += ../tc/sysconfig.o
pvrsrvkm-$(CONFIG_POWERVR_TC:m=y) += $(PVR_ARCH)/dma_support.o

pvrsrvkm-$(CONFIG_POWERVR_RK3368) += ../rk3368/rk3368.o

pvrsrvkm-$(CONFIG_POWERVR_MT8173) += ../mt8173/mt8173.o

# RI strings
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_RESOURCE_INFO) += ri_server.o
ccflags-$(CONFIG_POWERVR_ROGUE_RESOURCE_INFO) += -DPVRSRV_ENABLE_GPU_MEMORY_INFO=1

# Device memory history
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_DEVICEMEM_HISTORY) += devicemem_history_server.o

# GPU tracing in systrace
# TCC
ifeq ($(CONFIG_ANDROID),y)
pvrsrvkm-$(CONFIG_FTRACE) += pvr_gputrace.o
else
pvrsrvkm-y += pvr_gputrace.o
endif

# Event tracing
pvrsrvkm-$(CONFIG_EVENT_TRACING) += trace_events.o

# Debug info interface
pvrsrvkm-$(CONFIG_DEBUG_FS) += pvr_debugfs.o
pvrsrvkm-$(CONFIG_PROC_FS) += pvr_procfs.o

# arch-specific wrapper functions
pvrsrvkm-$(CONFIG_X86) += osfunc_x86.o
pvrsrvkm-$(CONFIG_ARM) += osfunc_arm.o
pvrsrvkm-$(CONFIG_ARM64) += osfunc_arm64.o
pvrsrvkm-$(CONFIG_METAG) += osfunc_metag.o
pvrsrvkm-$(CONFIG_MIPS) += osfunc_mips.o

# PDump
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += dbgdriv.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += dbgdriv_handle.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += devicemem_pdump.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += devicememx_pdump.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += hostfunc.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += ioctl.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += main.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += pdump.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += pdump_server.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += pdump_mmu.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += pdump_physmem.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += srvinit_pdump.o
ccflags-$(CONFIG_POWERVR_ROGUE_PDUMP) += -DPDUMP=1

ifeq ($(SUPPORT_DISPLAY_CLASS),1)
pvrsrvkm-y += \
 dc_server.o \
 scp.o
endif


# Linux synchronisation  ????
ifeq ($(SUPPORT_BUFFER_SYNC),1)
 pvrsrvkm-y += \
  ../pvr_buffer_sync.o \
  ../pvr_fence.o
endif

# Android native synchronisation
ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)                                                                                                   
ifeq ($(SUPPORT_DMA_FENCE),1)
 pvrsrvkm-y += \
  ../pvr_sync_file.o \
  ../pvr_counting_timeline.o \
  ../pvr_sw_fence.o \
  ../pvr_fence.o
else
 pvrsrvkm-y += ../pvr_sync2.o
endif
else
ifeq ($(SUPPORT_FALLBACK_FENCE_SYNC),1)
 pvrsrvkm-y += \
 ../sync_fallback_server.o \
 ossecure_export.o \
 $(PVR_ARCH)/generated/syncfallback_bridge/server_syncfallback_bridge.o
endif
endif

ifeq ($(SUPPORT_LINUX_DVFS),1)
 pvrsrvkm-y += pvr_dvfs_device.o
endif

ifeq ($(PVRSRV_ENABLE_PVR_ION_STATS),1)
 pvrsrvkm-y += pvr_ion_stats.o
endif

# Generated bridge code
pvrsrvkm-y += $(PVR_ARCH)/generated/devicememhistory_bridge/client_devicememhistory_direct_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/devicememhistory_bridge/server_devicememhistory_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/mm_bridge/server_mm_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/mm_bridge/client_mm_direct_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/cmm_bridge/server_cmm_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxta3d_bridge/server_rgxta3d_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxcmp_bridge/server_rgxcmp_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxsignals_bridge/server_rgxsignals_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/srvcore_bridge/server_srvcore_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/sync_bridge/server_sync_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/sync_bridge/client_sync_direct_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/cache_bridge/client_cache_direct_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/cache_bridge/server_cache_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/pvrtl_bridge/server_pvrtl_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/pvrtl_bridge/client_pvrtl_direct_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxfwdbg_bridge/server_rgxfwdbg_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxhwperf_bridge/server_rgxhwperf_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxregconfig_bridge/server_rgxregconfig_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/htbuffer_bridge/server_htbuffer_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/htbuffer_bridge/client_htbuffer_direct_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxkicksync_bridge/server_rgxkicksync_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxtq2_bridge/server_rgxtq2_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/synctracking_bridge/client_synctracking_direct_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/synctracking_bridge/server_synctracking_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_RESOURCE_INFO) += $(PVR_ARCH)/generated/ri_bridge/server_ri_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_RESOURCE_INFO) += $(PVR_ARCH)/generated/ri_bridge/client_ri_direct_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_DEVICEMEM_HISTORY) += $(PVR_ARCH)/generated/devicememhistory_bridge/server_devicememhistory_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_DEVICEMEM_HISTORY) += $(PVR_ARCH)/generated/devicememhistory_bridge/client_devicememhistory_direct_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/pdumpmm_bridge/server_pdumpmm_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/pdumpmm_bridge/client_pdumpmm_direct_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/pdump_bridge/server_pdump_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/pdumpctrl_bridge/client_pdumpctrl_direct_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/pdumpctrl_bridge/server_pdumpctrl_bridge.o
pvrsrvkm-$(CONFIG_POWERVR_ROGUE_PDUMP) += $(PVR_ARCH)/generated/rgxpdump_bridge/server_rgxpdump_bridge.o
pvrsrvkm-$(CONFIG_DMA_SHARED_BUFFER) += $(PVR_ARCH)/generated/dmabuf_bridge/server_dmabuf_bridge.o

ifeq ($(PVR_ARCH),rogue)
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxtq_bridge/server_rgxtq_bridge.o
pvrsrvkm-y += $(PVR_ARCH)/generated/rgxbreakpoint_bridge/server_rgxbreakpoint_bridge.o
endif
ifeq ($(SUPPORT_DISPLAY_CLASS),1)
pvrsrvkm-y += $(PVR_ARCH)/generated/dc_bridge/server_dc_bridge.o
endif
