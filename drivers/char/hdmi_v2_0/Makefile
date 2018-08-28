#
# Makefile for the kernel character device drivers.
#
EXTRA_CFLAGS += -I$(srctree)/drivers/char/hdmi_v2_0 \
 		-I$(srctree)/drivers/char/hdmi_v2_0/hdmi_api_lib/include
 		
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0) +=\
	hdmi_api_lib/ \
	hdmi_access.o \
	hdmi_tx.o \
	hdmi_misc.o \
	hdmi_api.o \
	irq_handlers.o \
	proc_fs.o \
	hdmi_drm.o
