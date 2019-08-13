#
# Telechips SOC drivers
#

ifeq ($(CONFIG_ARCH_TCC897X),y)
else
obj-y	+= suspend.o
endif

obj-y	+= timer.o

obj-$(CONFIG_PMAP)		+= pmap.o
obj-$(CONFIG_PMAP_TCC897X)	+= pmap-tcc897x.o

#obj-$(CONFIG_ARCH_TCC802X)	+= pm-tcc802x.o
#obj-$(CONFIG_ARCH_TCC898X)	+= pm-tcc898x.o
obj-$(CONFIG_ARCH_TCC899X)	+= pm-tcc899x.o
obj-$(CONFIG_ARCH_TCC803X)	+= pm-tcc803x.o
obj-$(CONFIG_ARCH_TCC901X)	+= pm-tcc901x.o
#obj-$(CONFIG_TCC_MICOM)		+= micom_devices.o

obj-$(CONFIG_ARCH_TCC899X)	+= chip_info.o
obj-$(CONFIG_ARCH_TCC803X)	+= chip_info.o
obj-$(CONFIG_ARCH_TCC901X)	+= chip_info.o
