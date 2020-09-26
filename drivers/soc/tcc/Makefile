#
# Telechips SOC drivers
#

ifeq ($(CONFIG_ARCH_TCC897X),y)
#none
else
ifeq ($(CONFIG_ARCH_TCC805X),y)
obj-y	+= chip_info.o
else
obj-y	+= suspend.o chip_info.o
endif
endif

obj-y	+= timer.o irq-pic.o

obj-$(CONFIG_PMAP)		+= pmap.o
obj-$(CONFIG_TCC_BOOTSTAGE)	+= bootstage.o

obj-$(CONFIG_ARCH_TCC899X)	+= pm-tcc899x.o
obj-$(CONFIG_ARCH_TCC803X)	+= pm-tcc803x.o
obj-$(CONFIG_ARCH_TCC901X)	+= pm-tcc901x.o
