ifeq ($(CONFIG_TCC803X_CA7S),y)
zreladdr-y     += 0x80008000
params_phys-y  += 0x80000100
initrd_phys-y  += 0x80800000
else
zreladdr-y	+= 0x20008000
params_phys-y	+= 0x20000100
initrd_phys-y	+= 0x20800000
endif
