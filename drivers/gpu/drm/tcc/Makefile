#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

tccdrm-y := tcc_drm_drv.o tcc_drm_crtc.o tcc_drm_fb.o \
	tcc_drm_gem.o tcc_drm_core.o tcc_drm_plane.o

tccdrm-$(CONFIG_DRM_FBDEV_EMULATION) += tcc_drm_fbdev.o
tccdrm-$(CONFIG_DRM_TCC_LCD)	+= tcc_drm_lcd.o
tccdrm-$(CONFIG_DRM_TCC_EXT)	+= tcc_drm_ext.o
tccdrm-$(CONFIG_DRM_TCC_THIRD)	+= tcc_drm_third.o
tccdrm-$(CONFIG_DRM_TCC_DPI)	+= tcc_drm_dpi.o
tccdrm-$(CONFIG_DRM_TCC_VIDI)	+= tcc_drm_vidi.o

obj-$(CONFIG_DRM_TCC)		+= tccdrm.o
