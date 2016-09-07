#
# Makefile for the linux kernel.
#

ccflags-$(CONFIG_ARCH_MULTIPLATFORM) := -I$(srctree)/arch/arm/mach-rzn1/include

obj-$(CONFIG_ARCH_RZN1)		+= setup-rzn1.o sysctrl-rzn1.o rzn1-gpioirq.o
obj-$(CONFIG_SMP) += platsmp.o
