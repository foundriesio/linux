obj-$(CONFIG_LCD_FLD0800)  += lcd_FLD0800.o
#obj-$(CONFIG_LCD_ED090NA) 	+= lcd_ED090NA.o
obj-$(CONFIG_LCD_AT070TN93) += lcd_at070tn93.o
obj-$(CONFIG_LCD_CVBS)  += lcd_cvbs.o
obj-$(CONFIG_LCD_CCIR656)  += lcd_ccir656.o
obj-$(CONFIG_LCD_TM123XDHP90) += lcd_TM123XDHP90.o
obj-$(CONFIG_LCDlcd_PVLBJT020) += lcd_PVLBJT020.o
obj-$(CONFIG_DRM_TCC) += lcd_drm_dummy.o
obj-$(CONFIG_FB_TCC_OVERLAY)	+= tcc_overlay.o
ifdef CONFIG_ANDROID
ccflags-y += -Idrivers/staging/android
obj-$(CONFIG_TCC_VIOC_CONTROLLER)	 += tcc_displayblock.o
endif
obj-$(CONFIG_LCD_HDMI1920X720_ADV7613)	+= hdmi_1920x720_adv7613.o
obj-$(CONFIG_LCD_HDMI1920X720_PVLBJT_020) += hdmi_1920x720_pvlbjt_020_01.o
obj-$(CONFIG_LCD_HDMI640X480TU)	+= hdmi_640x480tu.o
obj-$(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT) += tcc_vsync.o
obj-$(CONFIG_VIOC_CORERESET_SUPPORT) += tcc_fb_cr.o
obj-$(CONFIG_FB_VIOC) += tcc_vioc_fb.o
obj-$(CONFIG_FB_NEW) += panel/ fb.o
obj-$(CONFIG_TCC_VIOC_CONTROLLER)	 += tcc_vioc_interface.o tca_display_config.o vioc/ g2d/
obj-$(CONFIG_VIOC_DOLBY_VISION_EDR) += tcc_vdv_interface.o
obj-$(CONFIG_LCD_HDMI1920X720)	+= hdmi_1920x720.o
obj-$(CONFIG_LCD_HDMI1280X720)	+= hdmi_1280x720.o
obj-$(CONFIG_LCD_HDMI1920X1080)	+= hdmi_1920x1080.o
obj-$(CONFIG_LCD_HDMI4K)		+= hdmi_4k.o
obj-$(CONFIG_TCC_VIDEO_DISPLAY_DEINTERLACE_MODE)	+= viqe.o viqe/
ccflags-y += -Idrivers/video/fbdev/tcc-fb/viqe
#obj-$(CONFIG_HIBERNATION)	+= tcc_qb_fb.o

obj-$(CONFIG_TCC_MIPI)	      += tcc_mipi_dsi.o
obj-$(CONFIG_TCC_CCFB) 	+= tcc_ccfb.o