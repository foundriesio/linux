EXTRA_CFLAGS += \
		-Iinclude/drm
kirin-drm-y :=  kirin_drm_drv.o \
		kirin_drm_dsi.o \

obj-$(CONFIG_DRM_HISI_KIRIN) += kirin-drm.o \
				kirin/kirin_drm_ade.o \
				kirin/dw_drm_dsi.o 

obj-$(CONFIG_DRM_HISI_KIRIN960) +=  kirin-drm.o \
				kirin960/dw_drm_dsi.o \
				kirin960/kirin_drm_dpe_utils.o \
				kirin960/kirin_drm_overlay_utils.o \
				kirin960/kirin_drm_dss.o \

