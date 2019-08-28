#
# Makefile for the kernel character device drivers.
#
EXTRA_CFLAGS += -I$(srctree)/drivers/char/hdmi_v2_0 \
 		-I$(srctree)/drivers/char/hdmi_v2_0/hdmi_api_lib/include

obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/irq.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/phy/phy.o
ifdef CONFIG_ARCH_TCC898X
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/phy/phy_898x.o
endif
ifdef CONFIG_ARCH_TCC899X
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/phy/phy_899x.o
endif
ifdef CONFIG_ARCH_TCC803X
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/phy/phy_803x.o
endif
ifdef CONFIG_ARCH_TCC901X
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/phy/phy_901x.o
endif

obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/bsp/i2cm.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/main_controller.o
#obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/api/api.o

obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_video.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/video_packetizer.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/edid/dtd.o

obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/video_params.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/video_sampler.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_avi.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_gamut.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_debug.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/csc.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/video.o

obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/audio.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/audio_i2s.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/audio_spdif.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/audio_params.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/audio_multistream.o

obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_audio.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_audio_info.o

obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_packets.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_acp.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_isrc.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_spd.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/fc_vsd.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/product.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/core/packets.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/scdc/scdc.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/scdc/scrambling.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/api/api.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/soc/soc.o


# HDCP
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/hdcp/hdcp.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/hdcp/hdcp_verify.o
obj-$(CONFIG_TCC_HDMI_DRIVER_V2_0)	+=  src/identification/identification.o

