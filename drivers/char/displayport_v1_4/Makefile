#
# Makefile for the kernel character device drivers.
#
EXTRA_CFLAGS += -I$(srctree)/drivers/char/displayport_v1_4 \
				  -I$(srctree)/drivers/char/displayport_v1_4/include

ifeq ($(CONFIG_DRM_TCC),y)
EXTRA_CFLAGS += -I$(srctree)/drivers/gpu/drm/tcc
endif

obj-$(CONFIG_TCC_DP_DRIVER_V1_4) +=\
	Dptx_serdes.o \
	Dptx.o \
	Dptx_api.o \
	Dptx_platform.o \
	Dptx_reg.o \
	Dptx_aux.o \
	Dptx_avgen.o \
	Dptx_core.o \
	Dptx_intr.o \
	Dptx_link.o \
	Dptx_edid.o \
	Dptx_extension.o \
	Dptx_hdcp.o \
	Dptx_drm_dp_addition.o