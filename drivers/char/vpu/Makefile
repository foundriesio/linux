
obj-$(CONFIG_TCC_VPU_DRV) += vpu_lib/

obj-$(CONFIG_TCC_VPU_DRV) += vpu.o

ifdef CONFIG_SUPPORT_TCC_JPU
obj-$(CONFIG_TCC_VPU_DRV) += jpu_lib/
vpu-y += jpu_mgr.o
vpu-y += jpu_mgr_sys.o

obj-$(CONFIG_TCC_VPU_DRV) += jpu_dev.o
jpu_dev-y += jpu_devices.o
endif

ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
obj-$(CONFIG_TCC_VPU_DRV) += vp9_lib/
vpu-y += vp9_mgr.o
vpu-y += vp9_mgr_sys.o

obj-$(CONFIG_TCC_VPU_DRV) += vp9_dev.o
vp9_dev-y += vp9_devices.o
endif

ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
obj-$(CONFIG_TCC_VPU_DRV) += hevc_lib/
vpu-y += hevc_mgr.o
vpu-y += hevc_mgr_sys.o

obj-$(CONFIG_TCC_VPU_DRV) += hevc_dev.o
hevc_dev-y += hevc_devices.o
endif

ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
obj-$(CONFIG_TCC_VPU_DRV) += vpu_4k_d2_lib/
vpu-y += vpu_4k_d2_mgr.o
vpu-y += vpu_4k_d2_mgr_sys.o

obj-$(CONFIG_TCC_VPU_DRV) += vpu_4k_d2_dev.o
vpu_4k_d2_dev-y += vpu_4k_d2_devices.o
endif

ifdef CONFIG_ARCH_TCC899X
vpu-y += smp_driver.o	###__FXXX__
endif

ifdef CONFIG_ARCH_TCC901X
vpu-y += smp_driver.o	###__FXXX__
endif

vpu-y += vpu_mgr.o vpu_buffer.o vpu_mem.o vpu_etc.o
vpu-y += vpu_mgr_sys.o
vpu-y += vpu_devices.o
vpu-y += vpu_dec.o
vpu-y += vpu_enc.o

