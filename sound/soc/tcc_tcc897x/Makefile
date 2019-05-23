# TCC Platform Support
#ifdef CONFIG_SND_SOC_TCC_PCM_V10
#snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-v10.o tcc/tca_tcchwcontrol.o
#else #for TCC898x, TCC802x
#ifdef CONFIG_ARCH_TCC898X
#snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-v20.o tcc/tca_tcchwcontrol.o
#else #for TCC802x
#snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-v20.o tcc/tca_tcchwcontrol.o
#endif
#endif

ifdef CONFIG_ARCH_TCC897X
snd-soc-tcc-pcm-objs := tcc-adma.o tcc-pcm-v10.o tcc/tca_tcchwcontrol.o
endif

ifdef CONFIG_ARCH_TCC897X
snd-soc-tcc-i2s-objs := tcc-i2s.o
snd-soc-tcc-spdif-objs := tcc-spdif.o
snd-soc-tcc-cdif-objs := tcc-cdif.o
else #for TCC898x, TCC802x
snd-soc-tcc-i2s-objs := tcc-i2s.o
snd-soc-tcc-spdif-objs := tcc-spdif.o
snd-soc-tcc-cdif-objs := tcc-cdif.o
endif
obj-$(CONFIG_SND_TCC_AUDIO_DSP) += tcc_board_dsp.o tcc-dsp-api.o tcc-i2s-dsp.o tcc-pcm-dsp.o
obj-$(CONFIG_SND_SOC_TCC) += snd-soc-tcc-pcm.o snd-soc-tcc-i2s.o snd-soc-tcc-spdif.o snd-soc-tcc-cdif.o

# TCC Machine Support
snd-soc-tcc-board-wm8731-objs := tcc_board_wm8731.o
snd-soc-tcc-board-wm8581-objs := tcc_board_wm8581.o
snd-soc-tcc-board-wm8524-objs := tcc_board_wm8524.o

obj-$(CONFIG_SND_SOC_TCC_BOARD_WM8524) += snd-soc-tcc-board-wm8524.o
obj-$(CONFIG_SND_SOC_TCC_BOARD_WM8731) += snd-soc-tcc-board-wm8731.o
obj-$(CONFIG_SND_SOC_TCC_BOARD_WM8581) += snd-soc-tcc-board-wm8581.o

snd-soc-tcc8970-board-stb-objs := tcc8970_board_stb.o
snd-soc-tcc8971-board-lcn10-objs := tcc8971_board_lcn_10.o
snd-soc-tcc8971-board-lcn20-objs := tcc8971_board_lcn_20.o
snd-soc-tcc8971-board-lcn30-objs := tcc8971_board_lcn_30.o
snd-soc-tcc5700-board-hd-dmb-objs := tcc5700_board_hd-dmb.o
snd-soc-tcc8021-board-evm-objs := tcc8021_board_evm.o
snd-soc-tcc8021-board-evm20-objs := tcc8021_board_evm_20.o

obj-$(CONFIG_SND_SOC_TCC8970_BOARD_STB) += snd-soc-tcc8970-board-stb.o
obj-$(CONFIG_SND_SOC_TCC8971_BOARD_LCN1) += snd-soc-tcc8971-board-lcn10.o
obj-$(CONFIG_SND_SOC_TCC8971_BOARD_LCN2) += snd-soc-tcc8971-board-lcn20.o
obj-$(CONFIG_SND_SOC_TCC8971_BOARD_LCN3) += snd-soc-tcc8971-board-lcn30.o
obj-$(CONFIG_SND_SOC_TCC5700_BOARD_HD_DMB) += snd-soc-tcc5700-board-hd-dmb.o
obj-$(CONFIG_SND_SOC_TCC8021_BOARD_EVM) += snd-soc-tcc8021-board-evm.o
obj-$(CONFIG_SND_SOC_TCC8021_BOARD_EVM2) += snd-soc-tcc8021-board-evm20.o

obj-y		+= asrc/
