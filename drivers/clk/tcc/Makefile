# Makefile for Telechips clocks
obj-$(CONFIG_ARCH_TCC)		+= clk.o
obj-$(CONFIG_ARCH_TCC898X)	+= clk-test.o
obj-$(CONFIG_ARCH_TCC898X)	+= clk-tcc898x.o
obj-$(CONFIG_ARCH_TCC899X)	+= clk-tcc899x.o
obj-$(CONFIG_ARCH_TCC802X)	+= clk-tcc802x.o
obj-$(CONFIG_ARCH_TCC802X)	+= clk-tcc802x-test.o
ifeq ($(CONFIG_TCC803X_CA7S), )
obj-$(CONFIG_ARCH_TCC803X)	+= clk-tcc803x.o
else
obj-$(CONFIG_ARCH_TCC803X)	+= clk-tcc803x-a7s.o
endif
