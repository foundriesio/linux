obj-$(CONFIG_PLATFORM_HWDEMUX) += tcc_hwdmx.o

ifeq ($(CONFIG_ARCH_TCC803X),y)
tcc_hwdmx-objs += hwdmx_multi_cmd.o
else
tcc_hwdmx-objs += hwdmx_cmd.o
endif
tcc_hwdmx-objs += \
	hwdmx_core.o \
	tcc_hwdemux_tsif_rx.o \
	TSDEMUX_sys.o

