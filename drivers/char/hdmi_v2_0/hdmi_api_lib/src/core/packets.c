/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <include/hdmi_ioctls.h>
#include <core/packets.h>
#include <core/fc_packets.h>
#include <core/fc_gamut.h>

#include <core/fc_acp.h>

#include <core/fc_audio_info.h>
#include <core/fc_video.h>
#include <core/fc_avi.h>
#include <core/fc_isrc.h>
#include <core/fc_spd.h>
#include <core/fc_vsd.h>
#include <core/audio_multistream.h>
#include <hdmi_api_lib/src/core/main_controller/main_controller_reg.h>


#define ACP_PACKET_SIZE 	16
#define ISRC_PACKET_SIZE 	16


int packets_Initialize(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	packets_DisableAllPackets(dev);
	return TRUE;
}


int vendor_Configure(struct hdmi_tx_dev *dev, productParams_t *productParams)
{
        videoParams_t * video = NULL;
        productParams_t * prod = NULL;

        do {
                if(dev == NULL) {
                        break;
                }
                video = (videoParams_t*)dev->videoParam;
                prod = (productParams_t*)dev->productParam;

                if(video->mHdmi == DVI) {
                        pr_info("%s DVI mode do not support to send infoframe \r\n", __func__);
                        break;
                }

                if (productParams != NULL && productParams->mVendorPayloadLength > 0) {
                        if(productParams->mVendorPayloadLength > 24) {
                                pr_info("%s mVendorPayloadLength is over 24\r\n", __func__);
                                productParams->mVendorPayloadLength = 24;
                        }
                        packets_VendorSpecificInfoFrame(dev, productParams->mOUI,
                                productParams->mVendorPayload, productParams->mVendorPayloadLength, 1);
                        memcpy(prod, productParams, sizeof(productParams_t));
                }
        } while(0);

        return 0;
}

int packets_Configure(struct hdmi_tx_dev *dev, videoParams_t * video, productParams_t * prod)
{
	LOG_TRACE();

	if(video->mHdmi == DVI){
		LOGGER(SNPS_WARN, "DVI mode selected: packets not configured\r\n");
		return TRUE;
	}

	if (prod != 0) {
		fc_spd_info_t spd_data = {
                        .vName = prod->mVendorName,
		        .vLength = prod->mVendorNameLength,
		        .pName = prod->mProductName,
		        .pLength = prod->mProductNameLength,
		        .code = prod->mSourceType,
		        .autoSend = 1,
		};

		u32 oui = prod->mOUI;
		u8 *vendor_payload = prod->mVendorPayload;
		u8 payload_length = prod->mVendorPayloadLength;

		fc_spd_config(dev, &spd_data);

                if(payload_length > 0)
		        packets_VendorSpecificInfoFrame(dev, oui, vendor_payload, payload_length, 1);

	}
	else {
		LOGGER(SNPS_WARN,"No product info provided: not configured");
	}

	fc_packets_metadata_config(dev);

	// default phase 1 = true
	hdmi_dev_write_mask(dev, FC_GCP, FC_GCP_DEFAULT_PHASE_MASK, ((video->mPixelPackingDefaultPhase == 1) ? 1 : 0));

	fc_gamut_config(dev);

	fc_avi_config(dev, video);

	/** Colorimetry */
	packets_colorimetry_config(dev, video);

	return TRUE;
}

void packets_AudioContentProtection(struct hdmi_tx_dev *dev, u8 type, const u8 * fields, u8 length, u8 autoSend)
{
        u16 i;
        u8 newFields[ACP_PACKET_SIZE];

	LOG_TRACE();
	fc_packets_AutoSend(dev, 0, ACP_TX);

	fc_acp_type(dev, type);

        /* Preven buffer that will be overrun. */
        if(length > ACP_PACKET_SIZE) {
                length = ACP_PACKET_SIZE;
        }

	for (i = 0; i < length; i++) {
		newFields[i] = fields[i];
	}
	if (length < ACP_PACKET_SIZE) {
		for (i = length; i < ACP_PACKET_SIZE; i++) {
			newFields[i] = 0;	/* Padding */
		}
		length = ACP_PACKET_SIZE;
	}
	fc_acp_type_dependent_fields(dev, newFields, length);
	if (!autoSend) {
		fc_packets_ManualSend(dev, ACP_TX);
	} else {
		fc_packets_AutoSend(dev, autoSend, ACP_TX);
	}

}

#if 0
/* This API is deprecated */
void packets_IsrcPackets(struct hdmi_tx_dev *dev, u8 initStatus, const u8 * codes, u8 length, u8 autoSend)
{
	u16 i;
	u8 newCodes[ISRC_PACKET_SIZE * 2];
	LOG_TRACE();

	fc_packets_AutoSend(dev, 0, ISRC1_TX);
	fc_packets_AutoSend(dev, 0, ISRC2_TX);

	fc_isrc_status(dev, initStatus);

	for (i = 0; i < length; i++) {
		newCodes[i] = codes[i];
	}

	if (length > ISRC_PACKET_SIZE) {
		for (i = length; i < (ISRC_PACKET_SIZE * 2); i++) {
			newCodes[i] = 0;	/* Padding */
		}
		length = (ISRC_PACKET_SIZE * 2);

		fc_isrc_isrc2_codes(dev, newCodes + (ISRC_PACKET_SIZE * sizeof(u8)), length - ISRC_PACKET_SIZE);
		fc_isrc_cont(dev, 1);

		fc_packets_AutoSend(dev, autoSend, ISRC2_TX);

		if (!autoSend) {
			fc_packets_ManualSend(dev, ISRC2_TX);
		}
	}
	if (length < ISRC_PACKET_SIZE) {
		for (i = length; i < ISRC_PACKET_SIZE; i++) {
			newCodes[i] = 0;	/* Padding */
		}

		length = ISRC_PACKET_SIZE;

		fc_isrc_cont(dev, 0);
	}

	fc_isrc_isrc1_codes(dev, newCodes, length);	/* first part only */
	fc_isrc_valid(dev, 1);

	fc_packets_AutoSend(dev, autoSend, ISRC1_TX);

	if (!autoSend) {
		fc_packets_ManualSend(dev, ISRC1_TX);
	}
}
#endif

void packets_AvMute(struct hdmi_tx_dev *dev, u8 enable)
{
	LOG_TRACE1(enable);
	hdmi_dev_write_mask(dev, FC_GCP, FC_GCP_SET_AVMUTE_MASK, (enable ? 1 : 0));
	hdmi_dev_write_mask(dev, FC_GCP, FC_GCP_CLEAR_AVMUTE_MASK, (enable ? 0 : 1));
}

void packets_IsrcStatus(struct hdmi_tx_dev *dev, u8 status)
{
	LOG_TRACE();
	fc_isrc_status(dev, status);
}

void packets_StopSendAcp(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	fc_packets_AutoSend(dev, 0, ACP_TX);
}

void packets_StopSendIsrc1(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	fc_packets_AutoSend(dev, 0, ISRC1_TX);
	fc_packets_AutoSend(dev, 0, ISRC2_TX);
}

void packets_StopSendIsrc2(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	fc_isrc_cont(dev, 0);
	fc_packets_AutoSend(dev, 0, ISRC2_TX);
}

void packets_StopSendSpd(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	fc_packets_AutoSend(dev, 0, SPD_TX);
}

void packets_StopSendVsd(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	fc_packets_AutoSend(dev, 0, VSD_TX);
}

void packets_DisableAllPackets(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	fc_packets_disable_all(dev);
}

int packets_VendorSpecificInfoFrame(struct hdmi_tx_dev *dev, u32 oui, const u8 * payload, u8 length, u8 autoSend)
{
	LOG_TRACE();
	fc_packets_AutoSend(dev,  0, VSD_TX);	/* prevent sending half the info. */
	fc_vsd_vendor_OUI(dev, oui);
	if (fc_vsd_vendor_payload(dev, payload, length)) {
		return FALSE;	/* DEFINE ERROR */
	}
	if (autoSend) {
		fc_packets_AutoSend(dev, autoSend, VSD_TX);
	} else {
		fc_packets_ManualSend(dev, VSD_TX);
	}
	return TRUE;
}

u8 packets_AudioMetaDataPacket(struct hdmi_tx_dev *dev, audioMetaDataPacket_t * audioMetaDataPckt)
{
	halAudioMultistream_MetaDataPacket_Header(dev, audioMetaDataPckt);
	halAudioMultistream_MetaDataPacketBody(dev, audioMetaDataPckt);
	return TRUE;
}

void packets_colorimetry_config(struct hdmi_tx_dev *dev, videoParams_t * video)
{
	u8 gamut_metadata[28] = {0};
	int gdb_color_space = 0;

	fc_gamut_enable_tx(dev, 0);

	if(video->mColorimetry == EXTENDED_COLORIMETRY){
		if(video->mExtColorimetry == XV_YCC601){
			gdb_color_space = 1;
		}
		else if(video->mExtColorimetry == XV_YCC709){
			gdb_color_space = 2;
			LOGGER(SNPS_WARN, "xv ycc709");
		}
		else if(video->mExtColorimetry == S_YCC601){
			gdb_color_space = 3;
		}
		else if(video->mExtColorimetry == ADOBE_YCC601){
			gdb_color_space = 3;
		}
		else if(video->mExtColorimetry == ADOBE_RGB){
			gdb_color_space = 3;
		}

		if(video->mColorimetryDataBlock == TRUE){
			gamut_metadata[0] = (1 << 7) | gdb_color_space;
			fc_gamut_packet_config(dev, gamut_metadata, (sizeof(gamut_metadata) / sizeof(u8)));
		}
	}
}
