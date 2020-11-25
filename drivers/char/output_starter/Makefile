#
# Makefile for the kernel character device drivers.
#

EXTRA_CFLAGS += -I$(srctree)/drivers/char/output_starter \
 		-I$(srctree)/drivers/video/fbdev/tcc-fb

ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
EXTRA_CFLAGS += -I$(srctree)/drivers/char/hdmi_v2_0 \
 		-I$(srctree)/drivers/char/hdmi_v2_0/hdmi_api_lib/include
endif

ifeq ($(CONFIG_TCC_HDMI_DRIVER_V1_4),y)
EXTRA_CFLAGS += -I$(srctree)/drivers/char/hdmi_v1_4 \
 		-I$(srctree)/drivers/char/hdmi_v1_4/include
endif

obj-$(CONFIG_TCC_OUTPUT_STARTER)	+= tcc_output_starter.o
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
obj-$(CONFIG_TCC_OUTPUT_STARTER)	+= output_starter_hdmi.o
obj-$(CONFIG_TCC_OUTPUT_STARTER)	+= output_starter_hdmi_edid.o
endif
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V1_4),y)
obj-$(CONFIG_TCC_OUTPUT_STARTER)	+= output_starter_hdmi_v1_4.o
endif

