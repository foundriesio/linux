/*
 * Copyright © 2014 Red Hat
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*******************************************************************************


*   Modified by Telechips Inc.


*   Modified date :


*   Description :


*******************************************************************************/

#include <linux/types.h>

#include "Dptx_drm_dp_addition.h"
#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"


static u8 drm_addition_get_link_status( const u8 link_status[DP_LINK_STATUS_SIZE], int iDPCD_Address )
{
	return link_status[iDPCD_Address - DP_LANE0_1_STATUS];
}

static u8 drm_addition_get_msg_data_crc4( const uint8_t *data, u8 number_of_bytes )
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = number_of_bytes * 8;
	u16 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x100) == 0x100)
			remainder ^= 0xd5;
	}

	number_of_bits = 8;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x100) != 0)
			remainder ^= 0xd5;
	}

	return remainder & 0xff;
}

/*
 *	DP_LANE0_1_STATUS -> Lane 0 and Lan 1 Status, DP_LANE2_3_STATUS -> Lane 2 and Lan 3 Status
 */
u8 Drm_Addition_Get_Lane_Status( const u8 link_status[DP_LINK_STATUS_SIZE],	int iLane_Index )
{
	int			iDPCD_Address, iDPCP_LaneX_Status_Offset;
	u8			ucDPCD_LaneX_X_Status;

	iDPCD_Address				= ( DP_LANE0_1_STATUS + ( iLane_Index >> 1 ) );	/* Lane 0 & 1 -> 00202 + 0 <-> Lane 2 & 3 -> 00202 + 1 */
	ucDPCD_LaneX_X_Status		= drm_addition_get_link_status( link_status, iDPCD_Address );
	iDPCP_LaneX_Status_Offset	= ( iLane_Index & 1 ) * 4;		/* Lane 0 & 1 -> [00202]Bit0 & Bit4, Lane 2 & 3 -> [00203]Bit0 & Bit4 */

	return ( ucDPCD_LaneX_X_Status >> iDPCP_LaneX_Status_Offset ) & 0xf;
}


bool Drm_Addition_Get_Clock_Recovery_Status( const u8 link_status[DP_LINK_STATUS_SIZE],			      int iNumOfLanes )
{
	u8			ucLaneX_Status;
	int			iLane_Index;

	for( iLane_Index = 0; iLane_Index < iNumOfLanes; iLane_Index++ ) 
	{
		ucLaneX_Status = Drm_Addition_Get_Lane_Status( link_status, iLane_Index );
		if(( ucLaneX_Status & DP_LANE_CR_DONE ) == 0 )
		{
			return false;
		}
	}
	return true;
}

bool Drm_Addition_Get_Channel_EQ_Status( const u8 link_status[DP_LINK_STATUS_SIZE],			  int iNumOfLanes )
{
	u8			ucDPCD_Lane_Align_Status, ucLaneX_Status;
	int			iLane_Index;

	ucDPCD_Lane_Align_Status = drm_addition_get_link_status( link_status, DP_LANE_ALIGN_STATUS_UPDATED );
	if( ( ucDPCD_Lane_Align_Status & DP_INTERLANE_ALIGN_DONE ) == 0 )
	{
		return false;
	}

	for( iLane_Index = 0; iLane_Index < iNumOfLanes; iLane_Index++ ) 
	{
		ucLaneX_Status = Drm_Addition_Get_Lane_Status( link_status, iLane_Index );
		if( ( ucLaneX_Status & DP_CHANNEL_EQ_BITS ) != DP_CHANNEL_EQ_BITS )	/* Check if CR, EQ or SYMBOL is not locked */
		{
			return false;
		}
	}
	return true;
}

/**
 * drm_dp_calc_pbn_mode() - Calculate the PBN for a mode.
 * @clock: dot clock for the mode
 * @bpp: bpp for the mode.
 *
 * This uses the formula in the spec to calculate the PBN value for a mode.
 */
int Drm_Addition_Calculate_PBN_mode( int clock, int bpp )
{
	fixed20_12		pix_bw;
	fixed20_12		fbpp;
	fixed20_12		result;
	fixed20_12		margin, tmp;
	u32				res;

	pix_bw.full	= dfixed_const( clock );
	fbpp.full	= dfixed_const( bpp );
	tmp.full	= dfixed_const( 8 );
	fbpp.full	= dfixed_div( fbpp, tmp );

	result.full = dfixed_mul( pix_bw, fbpp );
	margin.full = dfixed_const( 54 );
	tmp.full	= dfixed_const( 64 );
	margin.full = dfixed_div( margin, tmp );
	result.full = dfixed_div( result, margin );

	margin.full = dfixed_const( 1006 );
	tmp.full	= dfixed_const( 1000 );
	margin.full = dfixed_div( margin, tmp );
	result.full = dfixed_mul( result, margin );

	result.full = dfixed_div( result, tmp );
	result.full = dfixed_ceil( result );
	
	res			= dfixed_trunc( result );

	return res;
}

EXPORT_SYMBOL( Drm_Addition_Calculate_PBN_mode );


int32_t Drm_Addition_Parse_Sideband_Link_Address( struct drm_dp_sideband_msg_rx *raw, 					           struct drm_dp_sideband_msg_reply_body *repmsg )
{
	int idx = 1;
	int i;
	memcpy(repmsg->u.link_addr.guid, &raw->msg[idx], 16);
	idx += 16;
	repmsg->u.link_addr.nports = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	for (i = 0; i < repmsg->u.link_addr.nports; i++) {
		if (raw->msg[idx] & 0x80)
			repmsg->u.link_addr.ports[i].input_port = 1;

		repmsg->u.link_addr.ports[i].peer_device_type = (raw->msg[idx] >> 4) & 0x7;
		repmsg->u.link_addr.ports[i].port_number = (raw->msg[idx] & 0xf);

		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		repmsg->u.link_addr.ports[i].mcs = (raw->msg[idx] >> 7) & 0x1;
		repmsg->u.link_addr.ports[i].ddps = (raw->msg[idx] >> 6) & 0x1;
		if (repmsg->u.link_addr.ports[i].input_port == 0)
			repmsg->u.link_addr.ports[i].legacy_device_plug_status = (raw->msg[idx] >> 5) & 0x1;
		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		if (repmsg->u.link_addr.ports[i].input_port == 0) {
			repmsg->u.link_addr.ports[i].dpcd_revision = (raw->msg[idx]);
			idx++;
			if (idx > raw->curlen)
				goto fail_len;
			memcpy(repmsg->u.link_addr.ports[i].peer_guid, &raw->msg[idx], 16);
			idx += 16;
			if (idx > raw->curlen)
				goto fail_len;
			repmsg->u.link_addr.ports[i].num_sdp_streams = (raw->msg[idx] >> 4) & 0xf;
			repmsg->u.link_addr.ports[i].num_sdp_stream_sinks = (raw->msg[idx] & 0xf);
			idx++;

		}
		if (idx > raw->curlen)
			goto fail_len;
	}

	return DPTX_RETURN_NO_ERROR;
fail_len:
	dptx_err("link address reply parse length fail %d %d\n", idx, raw->curlen);

	return DPTX_RETURN_ENOENT;
}






static u8 drm_addition_get_msg_header_crc4( const uint8_t *data, size_t num_nibbles )
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = num_nibbles * 4;
	u8 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x10) == 0x10)
			remainder ^= 0x13;
	}

	number_of_bits = 4;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x10) != 0)
			remainder ^= 0x13;
	}

	return remainder;
}

void Drm_Addition_Encode_SideBand_Msg_CRC( u8 *msg, u8 len )
{
	u8		crc4;

	crc4 = drm_addition_get_msg_data_crc4( msg, len );
	msg[len] = crc4;
}

void Drm_Addition_Encode_Sideband_Msg_Hdr( struct drm_dp_sideband_msg_hdr *hdr,    				   u8 *buf, int *len )
{
	int idx = 0;
	int i;
	u8 crc4;
	buf[idx++] = ((hdr->lct & 0xf) << 4) | (hdr->lcr & 0xf);
	for (i = 0; i < (hdr->lct / 2); i++)
		buf[idx++] = hdr->rad[i];
	buf[idx++] = (hdr->broadcast << 7) | (hdr->path_msg << 6) |
		(hdr->msg_len & 0x3f);
	buf[idx++] = (hdr->somt << 7) | (hdr->eomt << 6) | (hdr->seqno << 4);

	crc4 = drm_addition_get_msg_header_crc4(buf, (idx * 2) - 1);
	buf[idx - 1] |= (crc4 & 0xf);
	*len = idx;
}

int32_t Drm_Addition_Decode_Sideband_Msg_Hdr( struct drm_dp_sideband_msg_hdr *hdr, 					   u8 *buf, int buflen, u8 *hdrlen )
{
	u8 crc4;
	u8 len;
	int i;
	u8 idx;
	if (buf[0] == 0)
		return DPTX_RETURN_ENOENT;
	len = 3;
	len += ((buf[0] & 0xf0) >> 4) / 2;
	if (len > buflen)
		return DPTX_RETURN_ENOENT;
	crc4 = drm_addition_get_msg_header_crc4(buf, (len * 2) - 1);

	if ((crc4 & 0xf) != (buf[len - 1] & 0xf)) {
		//DRM_DEBUG_KMS("crc4 mismatch 0x%x 0x%x\n", crc4, buf[len - 1]);
		return DPTX_RETURN_ENOENT;
	}

	hdr->lct = (buf[0] & 0xf0) >> 4;
	hdr->lcr = (buf[0] & 0xf);
	idx = 1;
	for (i = 0; i < (hdr->lct / 2); i++)
		hdr->rad[i] = buf[idx++];
	hdr->broadcast = (buf[idx] >> 7) & 0x1;
	hdr->path_msg = (buf[idx] >> 6) & 0x1;
	hdr->msg_len = buf[idx] & 0x3f;
	idx++;
	hdr->somt = (buf[idx] >> 7) & 0x1;
	hdr->eomt = (buf[idx] >> 6) & 0x1;
	hdr->seqno = (buf[idx] >> 4) & 0x1;
	idx++;
	*hdrlen = idx;

	return DPTX_RETURN_NO_ERROR;
}

void Drm_Addition_Parse_Sideband_Connection_Status_Notify( struct drm_dp_sideband_msg_rx *raw,							         struct drm_dp_sideband_msg_req_body *msg )
{
	int idx = 1;

	msg->u.conn_stat.port_number = (raw->msg[idx] & 0xf0) >> 4;
	idx++;

	memcpy(msg->u.conn_stat.guid, &raw->msg[idx], 16);
	idx += 16;

	msg->u.conn_stat.legacy_device_plug_status = (raw->msg[idx] >> 6) & 0x1;
	msg->u.conn_stat.displayport_device_plug_status = (raw->msg[idx] >> 5) & 0x1;
	msg->u.conn_stat.message_capability_status = (raw->msg[idx] >> 4) & 0x1;
	msg->u.conn_stat.input_port = (raw->msg[idx] >> 3) & 0x1;
	msg->u.conn_stat.peer_device_type = (raw->msg[idx] & 0x7);
	idx++;
}


bool Drm_dp_tps3_supported( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] )
{
	return ( dpcd[DP_DPCD_REV] >= 0x12 ) && ( dpcd[DP_MAX_LANE_COUNT] & DP_TPS3_SUPPORTED );
}

bool Drm_dp_tps4_supported( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] )
{
	return ( dpcd[DP_DPCD_REV] >= 0x14) && ( dpcd[DP_MAX_DOWNSPREAD] & DP_TPS4_SUPPORTED );
}

u8 Drm_dp_max_lane_count( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] )
{
	return ( dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK );
}

bool Drm_dp_enhanced_frame_cap( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] )
{
	return ( dpcd[DP_DPCD_REV] >= 0x11 ) &&	( dpcd[DP_MAX_LANE_COUNT] & DP_ENHANCED_FRAME_CAP );
}






