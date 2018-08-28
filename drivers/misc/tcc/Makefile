
obj-$(CONFIG_SCALER_TEST) += scaler_drv_common.o
obj-$(CONFIG_TCC_SCALER) += scaler_drv.o
obj-$(CONFIG_TCC_WMIXER) += wmixer_drv.o
obj-$(CONFIG_TCC_ATTACH) += attach_drv.o
obj-$(CONFIG_TCC_VIQE_MADI)	+= viqe_madi_drv.o
obj-$(CONFIG_VIDEO_HDMI_IN_SENSOR_EP9553T)	+= tcc_ep9553t_cec.o
obj-$(CONFIG_TCC_G2D) += tcc_grp.o

ifdef CONFIG_ARCH_TCC802X
obj-$(CONFIG_TCC_SDR)   += tcc_sdr.o tca_sdr.o
else
obj-$(CONFIG_TCC_SDR)   += tcc_sdr.o
endif
obj-y				+= tcc_dsp/

obj-$(CONFIG_VIOC_CHROMA_INTERPOLATOR) += chromainterpolator_drv.o
