ifeq ($(CONFIG_ANDROID),y)
dtb-$(CONFIG_ARCH_TCC899X) += \
	tcc8990-android-lpd4322-v0.1.dtb \
	tcc8995-android-ott-lpd4321-v0.1.dtb \
	tcc8995-android-mini-lpd4321-v0.1.dtb \
	tcc8995-android-mini-lpd4321-v0.2.dtb \
	tcc8995-android-mini-tcm3840-lpd4321-v0.1.dtb
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-android-lpd4321_sv0.1.dtb
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-android-lpd4321_sv0.1-hdmi-ext-output.dtb \
	tcc8030-android-lpd4321_sv0.1-hdmi-ext-panel.dtb
endif
dtb-$(CONFIG_ARCH_TCC901X) += \
	tcc9010-android-lpd4321-v0.1.dtb
else
dtb-$(CONFIG_ARCH_TCC899X) += \
	tcc8990-linux-lpd4322-v0.1.dtb \
	tcc8995-linux-ott-lpd4321-v0.1.dtb \
	tcc8995-linux-mini-lpd4321-v0.1.dtb \
	tcc8995-linux-mini-lpd4321-v0.2.dtb \
	tcc8995-linux-mini-tcm3840-lpd4321-v0.1.dtb
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-linux-lpd4321_sv0.1.dtb
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-linux-lpd4321_sv0.1-hdmi-ext-output.dtb \
	tcc8030-linux-lpd4321_sv0.1-hdmi-main-output.dtb
endif
dtb-$(CONFIG_ARCH_TCC901X) += \
	tcc9010-linux-lpd4321-v0.1.dtb
endif

always         := $(dtb-y)
subdir-y       := $(dts-dirs)
clean-files    := *.dtb
