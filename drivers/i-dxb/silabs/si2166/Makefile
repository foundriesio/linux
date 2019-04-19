# 
# SiLabs SI2166 device
#
idxb-silabs-si2166-objs := 					\
	module/SiLabs_SAT_Tuner_API.o			\
	module/SiLabs_Unicable_API.o			\
	module/A8304_L1_API.o					\
	module/SiLabs_L1_RF_AV2018_API.o		\
	module/Silabs_L0_Connection.o			\
	module/Si2183_L1_Commands.o				\
	module/Si2183_L1_Properties.o			\
	module/Si2183_L1_API.o					\
	module/Si2183_L2_API.o					\
	module/SiLabs_API_L3_Wrapper.o			\
	../../telechips/tcc_power_silabs.o		\
	../../telechips/tcc_i2c.o				\
	../../telechips/tcc_gpio.o

ifeq ($(CONFIG_iDxB_SILABS_SI2166_LinuxFE),y)
idxb-silabs-si2166-objs += si2166.o
endif

ifeq ($(CONFIG_iDxB_SILABS_SI2166_Device),y)
idxb-silabs-si2166-objs += isi2166.o
endif

obj-$(CONFIG_iDxB_SILABS_SI2166_Device) += idxb-silabs-si2166.o

obj-$(CONFIG_iDxB_SILABS_SI2166_LinuxFE) += idxb-silabs-si2166.o


ccflags-y := 											\
	-I$(srctree)/drivers/i-dxb/silabs/si2166/module		\
	-I$(srctree)/drivers/i-dxb/silabs/si2166/patches	\
	-I$(srctree)/drivers/i-dxb/telechips	\
	-I$(srctree)/drivers/media/dvb-core	\
	-I$(srctree)/drivers/media/platform/tcc-hwdemux/hwdmx-drv	


ccflags-y += 				\
	-DSILABS_SUPERSET		\
	-DSATELLITE_FRONT_END	\
	-DSi2183_B60_COMPATIBLE	\
	-DSAT_TUNER_SILABS		\
	-DSAT_TUNER_AV2018		\
	-DDEMOD_DVB_S_S2_DSS	\
	-DUNICABLE_COMPATIBLE	\
	-DA8304_COMPATIBLE		\
	-DNO_FLOATS_ALLOWED		\
	-DTCC_KERNEL_MODULE
