# Makefile for Telechips clocks
obj-$(CONFIG_ARCH_TCC)		+= clk.o
obj-$(CONFIG_ARCH_TCC897X)	+= clk-tcc897x.o
obj-$(CONFIG_ARCH_TCC899X)	+= clk-tcc899x.o
ifeq ($(CONFIG_TCC803X_CA7S), )
obj-$(CONFIG_ARCH_TCC803X)	+= clk-tcc803x.o
else
obj-$(CONFIG_ARCH_TCC803X)	+= clk-tcc803x-a7s.o
endif
obj-$(CONFIG_ARCH_TCC901X)	+= clk-tcc901x.o
obj-$(CONFIG_ARCH_TCC805X)	+= clk-tcc805x.o
