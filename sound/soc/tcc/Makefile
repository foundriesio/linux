obj-$(CONFIG_TCC_MULTI_MAILBOX_AUDIO) += amd/
obj-$(CONFIG_SND_SOC_TCC) += tcc_adma_pcm.o tcc_i2s.o tcc_spdif.o tcc_cdif.o
obj-$(CONFIG_SND_SOC_TCC_AUDIO_CHMUX) += tcc_audio_chmux.o

obj-y		+= asrc/

ifneq ($(CONFIG_TCC_MULTI_MAILBOX_AUDIO),)
ccflags-y += -Isound/soc/tcc/amd/include \
				-Isound/soc/tcc/amd/include/params \
				-Isound/soc/tcc/amd/include/utils
endif
obj-$(CONFIG_SND_SOC_TCC_ASRC_M2M_PCM) += tcc_asrc_m2m_pcm.o

obj-$(CONFIG_SND_SOC_TCC_CARD) += tcc-snd-card.o
obj-$(CONFIG_SND_SOC_TCC_VIR_AUDIO) += virtual/