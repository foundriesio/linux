obj-$(CONFIG_POWERVR_ADF_FBDEV) += powervr_adf_fbdev.o
powervr_adf_fbdev-y += adf_common.o adf_fbdev.o
ifeq ($(CONFIG_ANDROID),y)
    ifeq ($(CONFIG_ARM64),y)
        ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_android64.h
    else
        ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_android32.h
    endif
else
    ifeq ($(CONFIG_ARM64),y)
        ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64.h
    else
        ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux32.h
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
