# 
# Maxlinear mxl532c device
#
idxb-maxlinear-mxl532c-objs := 					\
	mxl532c.o \
	demod_MxL532C.o \
	module/MxLWare_FW_Download.o \
	module/MxLWare_HYDRA_BERTApi.o \
	module/MxLWare_HYDRA_ChanBondApi.o \
	module/MxLWare_HYDRA_CommonApi.o \
	module/MxLWare_HYDRA_DemodTunerApi.o \
	module/MxLWare_HYDRA_DeviceApi.o \
	module/MxLWare_HYDRA_DiseqcFskApi.o \
	module/MxLWare_HYDRA_PhyCtrl.o \
	module/MxLWare_HYDRA_TsMuxCtrlApi.o \
	module/sample_oem/MxLWare_HYDRA_OEM_Drv.o \
	../../telechips/tcc_gpio.o 

#	d0mxl532c.o \
#	mxl532c_drv.o \
#	mxl532c_init.o \

obj-$(CONFIG_iDxB_MAXLINEAR_MXL532C) += idxb-maxlinear-mxl532c.o

ccflags-y := 											\
	-I$(srctree)/drivers/media/dvb-core \
	-I$(srctree)/drivers/i-dxb/maxlinear/mxl532c	\
	-I$(srctree)/drivers/i-dxb/maxlinear/mxl532c/module	\
	-I$(srctree)/drivers/i-dxb/maxlinear/mxl532c/module/sample_oem	\
	-I$(srctree)/drivers/i-dxb/telechips \
	-I$(srctree)/drivers/media/platform/tcc-hwdemux/hwdmx-drv

ccflags-y += 				\
	-DTCC_KERNEL_MODULE 
