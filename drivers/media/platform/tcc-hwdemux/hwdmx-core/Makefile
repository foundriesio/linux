obj-$(CONFIG_PLATFORM_HWDEMUX) += tcc_hwdmx.o

ifeq ($(CONFIG_TCC_SEC_IPC),y)
tcc_hwdmx-objs += hwdmx_sec_cmd.o
else
tcc_hwdmx-objs += hwdmx_cmd.o
endif
tcc_hwdmx-objs += \
	hwdmx_core.o \
	tcc_hwdemux_tsif_rx.o \
	TSDEMUX_sys.o
obj-$(CONFIG_HWDEMUX_TEST) += hwdmx_test.o
