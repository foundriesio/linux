# SPDX-License-Identifier: GPL-2.0
ifneq (,$(filter y,$(CONFIG_FB_PANEL_LVDS_TCC)))
ccflags-y += -I$(srctree)/drivers/video/fbdev/tcc-fb/panel
obj-y += panel_helper.o
endif

obj-$(CONFIG_FB_PANEL_LVDS_TCC) += panel-lvds-tcc.o
