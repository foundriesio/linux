obj-$(CONFIG_POWERVR_ADF_FBDEV) += powervr_adf_fbdev.o
powervr_adf_fbdev-y += adf_common.o adf_fbdev.o
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

ccflags-y += \
	-I$(srctree)/$(src) \
	-I$(srctree)/drivers/staging/android

obj-y += pvrsrvkm/
obj-$(CONFIG_POWERVR_TC:m=y) += tc/
obj-$(CONFIG_POWERVR_RK3368) += rk3368/
obj-$(CONFIG_POWERVR_MT8173) += mt8173/
obj-$(CONFIG_POWERVR_SUNXI) += sunxi/
obj-$(CONFIG_POWERVR_TCC9XTP) += tcc_9xtp/
