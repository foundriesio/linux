#
# Makefile for the kernel character device drivers.
#
EXTRA_CFLAGS += -I$(srctree)/drivers/char/output_starter \
		-I$(srctree)/drivers/char/hdmi_v2_0 \
 		-I$(srctree)/drivers/char/hdmi_v2_0/hdmi_api_lib/include \
 		-I$(srctree)/drivers/video/fbdev/tcc-fb

obj-$(CONFIG_TCC_OUTPUT_STARTER)	+= tcc_output_starter.o
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
obj-$(CONFIG_TCC_OUTPUT_STARTER)	+= output_starter_hdmi.o
obj-$(CONFIG_TCC_OUTPUT_STARTER)	+= output_starter_hdmi_edid.o

endif

