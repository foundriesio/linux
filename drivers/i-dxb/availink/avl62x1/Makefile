# 
# Availink AVL62X1 device
#
idxb-availink-avl62x1-objs := 					\
	sdk_src/AVL62X1.o \
	sdk_src/AVL62X1_API.o \
	sdk_src/AVL62X1_DVBSx.o \
	sdk_src/AVL62X1_Fw.o \
	sdk_src/AVL62X1_Internal.o \
	sdk_src/IBSP.o \
	tuner/AV201X/AV201X.o \
	tuner/AV201X/AV201X_MoreAPI.o \
	../../telechips/tcc_gpio.o

obj-$(CONFIG_iDxB_AVAILINK_AVL62X1) += idxb-availink-avl62x1.o

ccflags-y := 											\
	-I$(srctree)/drivers/media/dvb-core \
	-I$(srctree)/drivers/i-dxb/availink/avl62x1/tuner	\
	-I$(srctree)/drivers/i-dxb/availink/avl62x1/tuner/AV201X	\
	-I$(srctree)/drivers/i-dxb/availink/avl62x1/sdk_src \
	-I$(srctree)/drivers/i-dxb/telechips \
	-I$(srctree)/drivers/media/platform/tcc-hwdemux/hwdmx-drv

ccflags-y += 				\
	-DTCC_KERNEL_MODULE
