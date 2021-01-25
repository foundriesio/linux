#
# Telechips SOC drivers
#

ifeq ($(CONFIG_TCC803X_CA7S)$(CONFIG_ARCH_TCC897X),)
obj-y	+= chip_info.o
endif

obj-y	+= timer.o irq-pic.o

obj-$(CONFIG_PMAP)		+= pmap.o
obj-$(CONFIG_TCC_BOOTSTAGE)	+= bootstage.o

obj-$(CONFIG_ARCH_TCC899X)	+= pm-tcc899x.o suspend.o
obj-$(CONFIG_ARCH_TCC901X)	+= pm-tcc901x.o suspend.o
