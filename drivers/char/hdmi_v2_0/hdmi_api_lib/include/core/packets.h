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
#ifndef PACKETS_H_
#define PACKETS_H_

/**
 * Initialize the packets package. Reset local variables.
 * @param dev Device structure
 * @return TRUE when successful
 */
int packets_Initialize(struct hdmi_tx_dev *dev);

int vendor_Configure(struct hdmi_tx_dev *dev, productParams_t *productParams);

/**
 * Configure Source Product Description, Vendor Specific and Auxiliary
 * Video InfoFrames.
 * @param dev Device structure
 * @param video  Video Parameters to set up AVI InfoFrame (and all
 * other video parameters)
 * @param prod Description of Vendor and Product to set up Vendor
 * Specific InfoFrame and Source Product Description InfoFrame
 * @return TRUE when successful
 */
int packets_Configure(struct hdmi_tx_dev *dev, videoParams_t * video,
		      productParams_t * prod);

/**
 * Configure Audio Content Protection packets.
 * @param type Content protection type (see HDMI1.3a Section 9.3)
 * @param fields  ACP Type Dependent Fields
 * @param length of the ACP fields
 * @param autoSend Send Packets Automatically
 */
void packets_AudioContentProtection(struct hdmi_tx_dev *dev, u8 type, const u8 * fields,
				    u8 length, u8 autoSend);

/**
 * Configure ISRC 1 & 2 Packets
 * @param dev Device structure
 * @param initStatus Initial status which the packets are sent with (usually starting position)
 * @param codes ISRC codes array
 * @param length of the ISRC codes array
 * @param autoSend Send ISRC Automatically
 * @note Automatic sending does not change status automatically, it does the insertion of the packets in the data
 * islands.
 */
void packets_IsrcPackets(struct hdmi_tx_dev *dev, u8 initStatus, const u8 * codes,
			 u8 length, u8 autoSend);

/**
 * Send/stop sending AV Mute in the General Control Packet
 * @param dev Device structure
 * @param enable (TRUE) /disable (FALSE) the AV Mute
 */
void packets_AvMute(struct hdmi_tx_dev *dev, u8 enable);

/**
 * Set ISRC status that is changing during play back depending on position (see HDMI 1.3a Section 8.8)
 * @param dev Device structure
 * @param status the ISRC status code according to position of track
 */
void packets_IsrcStatus(struct hdmi_tx_dev *dev, u8 status);


/**
 * Stop sending ACP packets when in auto send mode
 * @param dev Device structure
 */
void packets_StopSendAcp(struct hdmi_tx_dev *dev);

/**
 * Stop sending ISRC 1 & 2 packets when in auto send mode (ISRC 2 packets cannot be send without ISRC 1)
 * @param dev Device structure
 */
void packets_StopSendIsrc1(struct hdmi_tx_dev *dev);

/**
 * Stop sending ISRC 2 packets when in auto send mode
 * @param dev Device structure
 */
void packets_StopSendIsrc2(struct hdmi_tx_dev *dev);

/**
 * Stop sending Source Product Description InfoFrame packets when in auto send mode
 * @param dev Device structure
 */
void packets_StopSendSpd(struct hdmi_tx_dev *dev);

/**
 * Stop sending Vendor Specific InfoFrame packets when in auto send mode
 * @param dev Device structure
 */
void packets_StopSendVsd(struct hdmi_tx_dev *dev);

/**
 * Disable all metadata packets from being sent automatically. (ISRC 1& 2, ACP, VSD and SPD)
 * @param dev Device structure
 */
void packets_DisableAllPackets(struct hdmi_tx_dev *dev);

/**
 * Configure Vendor Specific InfoFrames.
 * @param dev Device structure
 * @param oui Vendor Organisational Unique Identifier 24 bit IEEE
 * Registration Identifier
 * @param payload Vendor Specific Info Payload
 * @param length of the payload array
 * @param autoSend Start send Vendor Specific InfoFrame automatically
 */
int packets_VendorSpecificInfoFrame(struct hdmi_tx_dev *dev, u32 oui, const u8 * payload, u8 length, u8 autoSend);

/**
 * Configure Colorimetry packets
 * @param dev Device structure
 * @param video Video information structure
 */
void packets_colorimetry_config(struct hdmi_tx_dev *dev, videoParams_t * video);


#endif	/* PACKETS_H_ */
