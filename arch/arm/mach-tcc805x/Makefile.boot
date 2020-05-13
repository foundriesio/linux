ifeq ($(CONFIG_TCC805X_CA53Q), y)
zreladdr-y	+= 0x70008000
params_phys-y	+= 0x70000100
initrd_phys-y	+= 0x70800000
else
zreladdr-y	+= 0x20008000
params_phys-y	+= 0x20000100
initrd_phys-y	+= 0x20800000
endif
