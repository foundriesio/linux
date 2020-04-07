obj-$(CONFIG_TCC_SMP) += tsmp_drv.o

CFLAGS_tsmp_drv.o :=            \
    -DDEVICE_NAME=\"tcc-smp\"   \
    -DDEVICE_INSTANCES=2

EXTRA_CFLAGS := \
	-Idrivers/media/platform/tcc-hwdemux/hwdmx-drv/ \
	-Idrivers/media/dvb-core

