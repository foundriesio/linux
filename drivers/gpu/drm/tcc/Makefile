#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm -Idrivers/gpu/drm/tcc
tccdrm-y := tcc_drm_drv.o tcc_drm_crtc.o tcc_drm_fbdev.o \
		tcc_drm_fb.o tcc_drm_gem.o tcc_drm_core.o \
		tcc_drm_plane.o

tccdrm-$(CONFIG_DRM_TCC_LCD)	+= tcc_drm_lcd.o
tccdrm-$(CONFIG_DRM_TCC_EXT)	+= tcc_drm_ext.o
tccdrm-$(CONFIG_DRM_TCC_THIRD)	+= tcc_drm_third.o
tccdrm-$(CONFIG_DRM_TCC_DPI)	+= tcc_drm_dpi.o
tccdrm-$(CONFIG_DRM_TCC_MIXER)	+= tcc_mixer.o
tccdrm-$(CONFIG_DRM_TCC_HDMI)	+= tcc_drm_hdmi.o
tccdrm-$(CONFIG_DRM_TCC_VIDI)	+= tcc_drm_vidi.o
tccdrm-$(CONFIG_DRM_TCC_IPP)	+= tcc_drm_ipp.o
tccdrm-$(CONFIG_DRM_TCC_VIOC)	+= tcc_drm_vioc.o

obj-$(CONFIG_DRM_TCC)		+= tccdrm.o
