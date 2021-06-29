/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
*/

/*
* Modified by Telechips Inc.
*/

#include <linux/slab.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"

//#define EDID_DATA_DUMP

#define EDID_I2C_OVER_AUX_ADDR				0x50
#define EDID_I2C_OVER_AUX_SEGMENT_ADDR		0x30

#define EDID_MAX_EXTRA_BLK					3
#define EDID_EXT_BLK_FIELD					126

#define DETAIL_TIMING_DESC_1_PCLK_L			55
#define DETAIL_TIMING_DESC_1_PCLK_M			54
#define DETAIL_TIMING_DESC_2_PCLK_L			73
#define DETAIL_TIMING_DESC_2_PCLK_M			72
#define DETAIL_TIMING_DESC_3_PCLK_L			90
#define DETAIL_TIMING_DESC_3_PCLK_M			91
#define DETAIL_TIMING_DESC_4_PCLK_L			109
#define DETAIL_TIMING_DESC_4_PCLK_M			108

#define PRINT_BUF_SIZE						DPTX_EDID_BUFLEN

/* EDID Audio Data Block */
#define AUDIO_TAG			1
#define VIDEO_TAG			2

#define EDID_TAG_MASK		GENMASK(7, 5)
#define EDID_TAG_SHIFT		5
#define EDID_SIZE_MASK		GENMASK(4, 0)
#define EDID_SIZE_SHIFT		0

/* Established timing blocks */
#define ET1_800x600_60hz		BIT(0)
#define ET1_800x600_56hz		BIT(1)
#define ET1_640x480_75hz		BIT(2)
#define ET1_640x480_72hz		BIT(3)
#define ET1_640x480_67hz		BIT(4)
#define ET1_640x480_60hz		BIT(5)
#define ET1_720x400_88hz		BIT(6)
#define ET1_720x400_70hz		BIT(7)

#define ET2_1280x1024_75hz		BIT(0)
#define ET2_1024x768_75hz		BIT(1)
#define ET2_1024x768_70hz		BIT(2)
#define ET2_1024x768_60hz		BIT(3)
#define ET2_1024x768_87hz		BIT(4)
#define ET2_832x624_75hz		BIT(5)
#define ET2_800x600_75hz		BIT(6)
#define ET2_800x600_72hz		BIT(7)
#define ET3_1152x870_75hz		BIT(7)


#if defined(EDID_DATA_DUMP)
static void dptx_edid_Print_U8_Buf(u8 *pucBuf, u32 uiStart_RegOffset, u32 uiLength)
{
	int			iOffset;
	char		acStr[PRINT_BUF_SIZE];
	int			iNumOfWritten = 0;

	iNumOfWritten += snprintf(&acStr[iNumOfWritten], PRINT_BUF_SIZE - iNumOfWritten, "\n");

	for (iOffset = 0; iOffset < uiLength; iOffset++) {
		if (!(iOffset % 16)) {
			iNumOfWritten += snprintf(&acStr[iNumOfWritten], PRINT_BUF_SIZE - iNumOfWritten, "\n%02x:", (uiStart_RegOffset + iOffset));

			if (iNumOfWritten >= PRINT_BUF_SIZE)
				break;
		}

		iNumOfWritten += snprintf(&acStr[iNumOfWritten], PRINT_BUF_SIZE - iNumOfWritten, " %02x", pucBuf[iOffset]);

		if (iNumOfWritten >= PRINT_BUF_SIZE)
			break;
	}

	dptx_info("%s", acStr);
}
#endif


static int32_t dptx_edid_read_block_from_I2cOverAux(struct Dptx_Params *pstDptx, u8 ucBlockNum)
{
	uint8_t	ucRetry = 0;
	u8 ucOffset, ucSegment;
	u8 *pucBuffer_Copied;
	int iRetVal;

again:
	ucOffset  = ((ucBlockNum % 2) * DPTX_ONE_EDID_BLK_LEN);
	ucSegment = (ucBlockNum >> 1);

	iRetVal = Dptx_Aux_Write_Bytes_To_I2C(pstDptx, EDID_I2C_OVER_AUX_SEGMENT_ADDR, &ucSegment, 1);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Aux_Write_Bytes_To_I2C(pstDptx, EDID_I2C_OVER_AUX_ADDR, &ucOffset, 1);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Aux_Read_Bytes_From_I2C(pstDptx, EDID_I2C_OVER_AUX_ADDR, &pstDptx->pucEdidBuf[ucBlockNum * DPTX_ONE_EDID_BLK_LEN], DPTX_ONE_EDID_BLK_LEN);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
		if (ucRetry == 0) {
			ucRetry = 1;
			goto again;
		} else {
			return iRetVal;
		}
	}

	iRetVal = Dptx_Aux_Write_AddressOnly_To_I2C(pstDptx, EDID_I2C_OVER_AUX_ADDR);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	if (ucBlockNum == 0) {
		iRetVal = Dptx_Edid_Verify_BaseBlk_Data(pstDptx->pucEdidBuf);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;
	}

	if (ucBlockNum > 0)
		pucBuffer_Copied = &pstDptx->pucEdidBuf[ucBlockNum * DPTX_ONE_EDID_BLK_LEN];
	else
		pucBuffer_Copied = pstDptx->pucEdidBuf;

#if defined(EDID_DATA_DUMP)
	dptx_edid_Print_U8_Buf(pucBuffer_Copied, 0, DPTX_ONE_EDID_BLK_LEN);
#endif

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Edid_Verify_BaseBlk_Data(u8 *pucEDID_Buf)
{
	const u8 aucHeader[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
	int	iCompCmp;
	int	iElements;
	u32	uiEdid_CheckSum = 0;

	if (pucEDID_Buf == NULL) {
		dptx_err("EDID Buffer ptr is NULL");
		return DPTX_RETURN_EINVAL;
	}

	iCompCmp = memcmp(pucEDID_Buf, aucHeader, sizeof(aucHeader));
	if (iCompCmp) {
		dptx_err("Invalid EDID header(0x%x, 0x%x, 0x%x, 0x%x, 0x%x)",
					pucEDID_Buf[0],
					pucEDID_Buf[1],
					pucEDID_Buf[2],
					pucEDID_Buf[3],
					pucEDID_Buf[4]);

		return DPTX_RETURN_ENODEV;
	}

	for (iElements = 0; iElements < DPTX_ONE_EDID_BLK_LEN; iElements++)
		uiEdid_CheckSum += pucEDID_Buf[iElements];

	if (uiEdid_CheckSum & 0xFF) {
		dptx_err("Invalid EDID checksum( 0x%x )", uiEdid_CheckSum);
		return DPTX_RETURN_ENODEV;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Edid_Read_EDID_I2C_Over_Aux(struct Dptx_Params *pstDptx)
{
	u8	ucExt_Blocks, ucBlk_Index;
	int32_t iRetVal;

	memset(pstDptx->pucEdidBuf, 0, DPTX_EDID_BUFLEN);

	iRetVal = dptx_edid_read_block_from_I2cOverAux(pstDptx, 0);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		if (iRetVal == DPTX_RETURN_I2C_OVER_AUX_NO_ACK)
			dptx_info("Sink doesn't support EDID data on I2C Over Aux");

		return iRetVal;
	}

	dptx_info("The number of num_ext_blocks = %d ",
				pstDptx->pucEdidBuf[EDID_EXT_BLK_FIELD]);

	ucExt_Blocks = pstDptx->pucEdidBuf[EDID_EXT_BLK_FIELD];
	if (ucExt_Blocks == 0)
		return DPTX_RETURN_NO_ERROR;

	if (ucExt_Blocks > EDID_MAX_EXTRA_BLK) {
		dptx_warn("Ext blocks are larger than Max %d->down to %d",
					(u32)EDID_MAX_EXTRA_BLK,
					(u32)EDID_MAX_EXTRA_BLK);
		ucExt_Blocks = EDID_MAX_EXTRA_BLK;
	}

	for (ucBlk_Index = 1; ucBlk_Index <= ucExt_Blocks; ucBlk_Index++) {
		iRetVal = dptx_edid_read_block_from_I2cOverAux(pstDptx, ucBlk_Index);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Edid_Read_EDID_Over_Sideband_Msg(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u8 ucExt_Blocks;
	int32_t iRetVal;

	memset(pstDptx->pucEdidBuf, 0, DPTX_EDID_BUFLEN);

	iRetVal = Dptx_Ext_Remote_I2C_Read(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

#if defined(EDID_DATA_DUMP)
	dptx_edid_Print_U8_Buf(pstDptx->pucEdidBuf, 0, DPTX_ONE_EDID_BLK_LEN);
#endif

	ucExt_Blocks = pstDptx->pucEdidBuf[EDID_EXT_BLK_FIELD];
	if (ucExt_Blocks > 0) {

#if defined(EDID_DATA_DUMP)
		dptx_edid_Print_U8_Buf(&pstDptx->pucEdidBuf[DPTX_ONE_EDID_BLK_LEN], 0, DPTX_ONE_EDID_BLK_LEN);
#endif

	}

	return DPTX_RETURN_NO_ERROR;
}


