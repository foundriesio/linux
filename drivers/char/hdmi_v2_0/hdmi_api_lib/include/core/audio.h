/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef AUDIO_H_
#define AUDIO_H_

#include <include/hdmi_includes.h>
#include <include/audio_params.h>

/**
 * Initial set up of package and prepare it to be configured. Set audio mute to on.
 * @param baseAddr base Address of controller
 * @return TRUE if successful
 */
int audio_Initialize(struct hdmi_tx_dev *dev);

/**
 * Configure hardware modules corresponding to user requirements to start transmitting audio packets.
 * @param baseAddr base Address of controller
 * @param params: audio parameters
 * @param pixelClk: pixel clock [0.01 MHz]
 * @param ratioClk: ratio clock (TMDS / Pixel) [0.01]
 * @return TRUE if successful
 */
int audio_Configure(struct hdmi_tx_dev *dev, audioParams_t * params);

/**
 * Mute audio.
 * Stop sending audio packets
 * @param baseAddr base Address of controller
 * @param state:  1 to enable/0 to disable the mute
 * @return TRUE if successful
 */
int audio_mute(struct hdmi_tx_dev *dev, u8 state);

/**
 * Compute the value of N to be used in a clock divider to generate an
 * intermediate clock that is slower than the 128f'S clock by the factor N.
 * The N value relates the TMDS to the sampling frequency fS.
 * @param baseAddr base Address of controller
 * @param freq: audio sampling frequency [Hz]
 * @param pixelClk: pixel clock [0.01 MHz]
 * @param ratioClk: ratio clock (TMDS / Pixel) [0.01]
 * @return N
 */
u32 audio_ComputeN(struct hdmi_tx_dev *dev, u32 freq, u32 pixelClk);

/**
 * Calculate the CTS value, the denominator of the fractional relationship
 * between the TMDS clock and the audio reference clock used in ACR
 * @param baseAddr base Address of controller
 * @param freq: audio sampling frequency [Hz]
 * @param pixelClk: pixel clock [0.01 MHz]
 * @return CTS value
 */
u32 audio_ComputeCts(struct hdmi_tx_dev *dev, u32 freq, u32 pixelClk);

#endif				/* AUDIO_H_ */
