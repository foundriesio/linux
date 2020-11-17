ifeq ($(CONFIG_ANDROID),y)
dtb-$(CONFIG_ARCH_TCC899X) += \
	tcc8990-android-lpd4322-v0.1.dtb \
	tcc8995-android-ott-lpd4321-v0.1.dtb \
	tcc8995-android-mini-lpd4321-v0.1.dtb \
	tcc8995-android-mini-lpd4321-v0.2.dtb \
	tcc8995-android-mini-tcm3840-lpd4321-v0.1.dtb
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-android-lpd4321_sv0.1.dtb \
	tcc8030-android-lpd4321_sv0.1_for_t-sound.dtb \
	tcc803xp-android-lpd4321_sv0.1.dtb
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-android-lpd4321_sv0.1-hdmi-ext-output.dtb \
	tcc8030-android-lpd4321_sv0.1-hdmi-ext-panel.dtb
endif
dtb-$(CONFIG_ARCH_TCC901X) += \
	tcc9010-android-lpd4321-v0.1.dtb
dtb-$(CONFIG_ARCH_TCC805X) += \
	tcc8050-android-lpd4x322_sv0.1.dtb \
	tcc8053-android-lpd4x322_sv0.1.dtb \
	tcc8059-android-lpd4x321_sv0.1.dtb \
	tcc8060-android-board-xxx.dtb \
	tcc8050-android-ems-lpd4x322_sv0.1.dtb
else
dtb-$(CONFIG_ARCH_TCC899X) += \
	tcc8990-linux-lpd4322-v0.1.dtb \
	tcc8995-linux-ott-lpd4321-v0.1.dtb \
	tcc8995-linux-mini-lpd4321-v0.1.dtb \
	tcc8995-linux-mini-lpd4321-v0.2.dtb \
	tcc8995-linux-mini-tcm3840-lpd4321-v0.1.dtb
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc803xp-linux-lpd4321_sv0.1.dtb \
	tcc8030-linux-lpd4321_sv0.1.dtb \
	tcc8030-linux-lpd4321_sv0.1_for_t-sound.dtb
ifeq ($(CONFIG_TCC_HDMI_DRIVER_V2_0),y)
dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-linux-lpd4321_sv0.1-hdmi-ext-output.dtb \
	tcc8030-linux-lpd4321_sv0.1-hdmi-main-output.dtb
endif
dtb-$(CONFIG_ARCH_TCC901X) += \
	tcc9010-linux-lpd4321-v0.1.dtb

ifeq ($(CONFIG_TCC805X_CA53Q),y)
dtb-$(CONFIG_ARCH_TCC805X) += \
	tcc8050-linux-subcore-a53q.dtb \
	tcc8053-linux-subcore-a53q.dtb \
	tcc8059-linux-subcore-a53q.dtb \
	tcc8050-linux-ems-subcore-a53q.dtb
else
dtb-$(CONFIG_ARCH_TCC805X) += \
	tcc8050-linux-lpd4x322_sv0.1.dtb \
	tcc8053-linux-lpd4x322_sv0.1.dtb \
	tcc8059-linux-lpd4x321_sv0.1.dtb \
	tcc8060-linux-board-xxx.dtb \
	tcc8050-linux-ems-lpd4x322_sv0.1.dtb
endif
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtb-y += \
	tcc8031-soc.dtb

dtbo-y += \
	overlay-tcc8030-android-lpd4321_sv0.1.dtbo \
	overlay-tcc8030-android-lpd4321_sv0.1-hdmi-ext-output.dtbo \
	overlay-tcc8030-android-lpd4321_sv0.1-hdmi-ext-panel.dtbo \
	overlay-tcc803xp-android-lpd4321_sv0.1.dtbo

dtb-y += \
	tcc8050-soc.dtb \
	tcc8059-soc.dtb

dtbo-y += \
	overlay-tcc8050-android-lpd4x322_sv0.1.dtbo \
	overlay-tcc8053-android-lpd4x322_sv0.1.dtbo \
	overlay-tcc8059-android-lpd4x321_sv0.1.dtbo \
	overlay-tcc8060-android-board-xxx.dtbo \
	overlay-tcc8050-android-ems-lpd4x322_sv0.1.dtbo
endif

always         := $(dtb-y)
subdir-y       := $(dts-dirs)
clean-files    := *.dtb *.dtbo

targets += $(dtbo-y)
always  += $(dtbo-y)
