/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
#include <linux/module.h>
#include <sound/tcc/params/tcc_mbox_audio_pcm_params.h>

struct asrc_pcm_volume_t g_asrc_pcm_playback_volume[ASRC_PCM_PLAYBACK_DEVICE_SIZE] = {
    {ASRC_PCM_VALUE_VOLUME_MODE_SMOOTHING, 0,0},    // {range : -79 ~ 30, default 0}
    {ASRC_PCM_VALUE_VOLUME_MODE_SMOOTHING, 0,0},    // {range : -79 ~ 30, default 0}
    {ASRC_PCM_VALUE_VOLUME_MODE_SMOOTHING, 0,0},    // {range : -79 ~ 30, default 0}
};
EXPORT_SYMBOL(g_asrc_pcm_playback_volume);

struct asrc_pcm_volume_t g_asrc_pcm_capture_volume[ASRC_PCM_CAPTURE_DEVICE_SIZE] = {
    {ASRC_PCM_VALUE_VOLUME_MODE_SMOOTHING, 0,0},    // {range : -79 ~ 30, default 0}
};
EXPORT_SYMBOL(g_asrc_pcm_capture_volume);

struct asrc_pcm_fader_t g_asrc_pcm_fader = {0, 0};    // {balance, fade ; range : -100 ~ 100}
EXPORT_SYMBOL(g_asrc_pcm_fader);

int tcc_mbox_audio_pcm_is_available_device(unsigned int card_device)
{
    const unsigned short card = (card_device >> 4) & 0x0F;
	const unsigned short device = card_device & 0x0F;

    if (card != ASRC_PCM_CARD) {
		return 0;
    }

	if (device >= (ASRC_PCM_PLAYBACK_DEVICE_SIZE + ASRC_PCM_CAPTURE_DEVICE_SIZE)) {
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL(tcc_mbox_audio_pcm_is_available_device);

int tcc_mbox_audio_pcm_is_playback_device(unsigned int card_device)
{
    const unsigned short device = card_device & 0x0F;

    if (device < ASRC_PCM_PLAYBACK_DEVICE_SIZE) {
		return 1;
    } else {
        return 0;
    }
}
EXPORT_SYMBOL(tcc_mbox_audio_pcm_is_playback_device);

