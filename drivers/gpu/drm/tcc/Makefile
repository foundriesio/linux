#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.
EXTRA_CFLAGS += -I$(srctree)/drivers/gpu/drm/tcc
ifeq ($(CONFIG_TCC_DP_DRIVER_V1_4),y)
EXTRA_CFLAGS += -I$(srctree)/drivers/char/displayport_v1_4
endif
ifeq ($(CONFIG_TCC_SCREEN_SHARE),y)
EXTRA_CFLAGS += -I$(srctree)/drivers/char -Idrivers/clk/tcc
endif

tccdrm-y := tcc_drm_drv.o tcc_drm_crtc.o tcc_drm_fb.o \
	tcc_drm_gem.o tcc_drm_plane.o

tccdrm-$(CONFIG_DRM_FBDEV_EMULATION) += tcc_drm_fbdev.o
tccdrm-$(CONFIG_DRM_TCC_DPI) += tcc_drm_dpi.o
ifneq (,$(filter y,$(CONFIG_DRM_TCC_LCD) $(CONFIG_DRM_TCC_EXT) $(CONFIG_DRM_TCC_THIRD) $(CONFIG_DRM_TCC_FOURTH)))
tccdrm-y += tcc_drm_lcd_base.o
endif
tccdrm-$(CONFIG_DRM_TCC_LCD) += tcc_drm_lcd_primary.o
tccdrm-$(CONFIG_DRM_TCC_EXT) += tcc_drm_lcd_ext.o
tccdrm-$(CONFIG_DRM_TCC_THIRD) += tcc_drm_lcd_third.o
tccdrm-$(CONFIG_DRM_TCC_FOURTH) += tcc_drm_lcd_fourth.o
tccdrm-$(CONFIG_DRM_TCC_SCREEN_SHARE) += tcc_drm_screen_share.o

obj-$(CONFIG_DRM_TCC)		+= tccdrm.o tcc_drm_address.o
