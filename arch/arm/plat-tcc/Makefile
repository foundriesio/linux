# "Telechips Platform Common Modules"

ifeq ($(CONFIG_ARCH_TCC897X),y)
obj-$(CONFIG_ARCH_TCC)	+= cpu.o io.o nand.o proc_rotation.o timer.o
obj-$(CONFIG_ARCH_TCC)	+= poweroff.o suspend.o pmu_wakeup.o suspend_asm.o
obj-$(CONFIG_ARCH_TCC)	+= lcd_panel.o
obj-$(CONFIG_CPU_IDLE)	+= cpuidle.o
endif
ifeq ($(CONFIG_ARCH_TCC570X),y)
obj-$(CONFIG_ARCH_TCC)	+= io.o nand.o proc_rotation.o
obj-$(CONFIG_ARCH_TCC)	+= lcd_panel.o
endif
obj-y	+= dummy.o
