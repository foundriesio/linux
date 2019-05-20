#
# Makefile for the kernel character device drivers.
#
ifeq ($(CONFIG_ARCH_TCC803X),)
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0) += hdmi_cec.o hdmi_cec_misc.o cec_proc_fs.o hdmi_cec_lib/
endif

ifeq ($(CONFIG_ARCH_TCC803X),)
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
obj-$(CONFIG_CEC_SUPPORT_KEYS)	+= tcc_cec_interface.o
endif
endif

