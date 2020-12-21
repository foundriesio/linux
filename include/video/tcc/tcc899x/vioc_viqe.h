/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __VIOC_VIQE_H__
#define	__VIOC_VIQE_H__

#ifndef ON
#define ON 1
#endif
#ifndef OFF
#define OFF 0
#endif

#define	NORMAL_MODE 0  // normal mode
#define	DUPLI_MODE  1  // duplicate mode
#define	SKIP_MODE   2  // skip mode

typedef enum {
	VIOC_VIQE_DEINTL_MODE_BYPASS = 0,
	VIOC_VIQE_DEINTL_MODE_2D,
	VIOC_VIQE_DEINTL_MODE_3D,
	VIOC_VIQE_DEINTL_S
} VIOC_VIQE_DEINTL_MODE;

typedef enum {
	VIOC_VIQE_FMT_YUV420 = 0,
	VIOC_VIQE_FMT_YUV422
} VIOC_VIQE_FMT_TYPE;

/*
 * Register offset
 */
#define VCTRL				(0x000)
#define VSIZE				(0x004)
#define VTIMEGEN			(0x008)
#define VINT				(0x00C)
#define VMISC				(0x010)
#define DI_BASE0			(0x080)
#define DI_BASE1			(0x084)
#define DI_BASE2			(0x088)
#define DI_BASE3			(0x08C)
#define DI_SIZE				(0x090)
#define DI_OFFS				(0x094)
#define DI_CTRL				(0x098)
#define DI_BASE0A			(0x0A0)
#define DI_BASE1A			(0x0A4)
#define DI_BASE2A			(0x0A8)
#define DI_BASE3A			(0x0AC)
#define DI_BASE0B			(0x0B0)
#define DI_BASE1B			(0x0B4)
#define DI_BASE2B			(0x0B8)
#define DI_BASE3B			(0x0BC)
#define DI_BASE0C			(0x0C0)
#define DI_BASE1C			(0x0C4)
#define DI_BASE2C			(0x0C8)
#define DI_BASE3C			(0x0CC)
#define DI_CUR_BASE0		(0x0D0)
#define DI_CUR_BASE1		(0x0D4)
#define DI_CUR_BASE2		(0x0D8)
#define DI_CUR_BASE3		(0x0DC)
#define DI_CUR_WADDR		(0x0E0)
#define DI_CUR_RADDR		(0x0E4)
#define DI_DEC0_MISC		(0x100)
#define DI_DEC0_SIZE		(0x104)
#define DI_DEC0_STS			(0x108)
#define DI_DEC0_CTRL		(0x10C)
#define DI_DEC1_MISC		(0x120)
#define DI_DEC1_SIZE		(0x124)
#define DI_DEC1_STS			(0x128)
#define DI_DEC1_CTRL		(0x12C)
#define DI_DEC2_MISC		(0x140)
#define DI_DEC2_SIZE		(0x144)
#define DI_DEC2_STS			(0x148)
#define DI_DEC2_CTRL		(0x14C)
#define DI_COM0_MISC		(0x160)
#define DI_COM0_NU			(0x164)
#define DI_COM0_STS			(0x168)
#define DI_COM0_CTRL		(0x16C)
#define DI_COM0_AC			(0x170)
#define DI_CTRL2			(0x280)
#define DI_ENGINE0			(0x284)
#define DI_ENGINE1			(0x288)
#define DI_ENGINE2			(0x28C)
#define DI_ENGINE3			(0x290)
#define DI_ENGINE4			(0x294)
#define PD_THRES0			(0x298)
#define PD_THRES1			(0x29C)
#define PD_JUDDER			(0x2A0)
#define PD_JUDDER_M			(0x2A4)
#define DI_MISCC			(0x2A8)
#define DI_STATUS			(0x2AC)
#define PD_STSTUS			(0x2B0)
#define DI_REGION0			(0x2B4)
#define DI_REGION1			(0x2B8)
#define DI_INT				(0x2BC)
#define DI_PD_SAW			(0x2E0)
#define DI_CSIZE			(0x2E4)
#define DI_FMT				(0x2E8)

/*
 * VIQE Control Register
 */
#define VCTRL_CGPMD_SHIFT		(26) // Clock Gate Disable for De-nosier
#define VCTRL_CGDND_SHIFT		(25) // Clock Gate Disable for De-nosier (data sheet error)
#define VCTRL_CGDID_SHIFT		(24) // Clock Gate Disable for De-interlacer
#define VCTRL_CFGUPD_SHIFT		(21) // Set Register
#define VCTRL_UPD_SHIFT			(20) // Update Method
#define VCTRL_FCF_SHIFT			(18) // Format Converter Buffer Flush
#define VCTRL_FCDUF_SHIFT		(17) // Format Converter Disable Using FMT
#define VCTRL_FCD_SHIFT			(16) // Format Converter Disable
#define VCTRL_NHINTPL_SHIFT		(8)  // Not-Horizontal Interpolation
#define VCTRL_DIEN_SHIFT		(4)  // De-interlacer enable
#define VCTRL_DNEN_SHIFT		(3)  // SHOULD BE 0
#define VCTRL_GMEN_SHIFT		(2)  // SHOULD BE 0
#define VCTRL_HIEN_SHIFT		(1)  // SHOULD BE 0
#define VCTRL_HILUT_SHIFT		(0)  // SHOULD BE 0

#define VCTRL_CGPMD_MASK		(0x1 << VCTRL_CGPMD_SHIFT)
#define VCTRL_CGDND_MASK		(0x1 << VCTRL_CGDND_SHIFT)
#define VCTRL_CGDID_MASK		(0x1 << VCTRL_CGDID_SHIFT)
#define VCTRL_CFGUPD_MASK		(0x1 << VCTRL_CFGUPD_SHIFT)
#define VCTRL_UPD_MASK			(0x1 << VCTRL_UPD_SHIFT)
#define VCTRL_FCF_MASK			(0x1 << VCTRL_FCF_SHIFT)
#define VCTRL_FCDUF_MASK		(0x1 << VCTRL_FCDUF_SHIFT)
#define VCTRL_FCD_MASK			(0x1 << VCTRL_FCD_SHIFT)
#define VCTRL_NHINTPL_MASK		(0x1 << VCTRL_NHINTPL_SHIFT)
#define VCTRL_DIEN_MASK			(0x1 << VCTRL_DIEN_SHIFT)
#define VCTRL_DNEN_MASK			(0x1 << VCTRL_DNEN_SHIFT)
#define VCTRL_GMEN_MASK			(0x1 << VCTRL_GMEN_SHIFT)
#define VCTRL_HIEN_MASK			(0x1 << VCTRL_HIEN_SHIFT)
#define VCTRL_HILUT_MASK		(0x1 << VCTRL_HILUT_SHIFT)

/*
 * VIQE Size Register
 */
#define VSIZE_HEIGHT_SHIFT		(16) // Input Image Height by pixel
#define VSIZE_WIDTH_SHIFT		(0)  // Input Image Width by pixel

#define VSIZE_HEIGHT_MASK		(0x7FF << VSIZE_HEIGHT_SHIFT)
#define VSIZE_WIDTH_MASK		(0x7FF << VSIZE_WIDTH_SHIFT)

/*
 * VIQE Time Generator Register
 */
#define VTIMEGEN_Y2RMD_SHIFT		(9) // Y2R Converter Mode
#define VTIMEGEN_Y2REN_SHIFT		(8) // Y2R Converter Enable
#define VTIMEGEN_H2H_SHIFT			(0) // the pixel count between the ending pixel of the line and the first pixel of the next line

#define VTIMEGEN_Y2RMD_MASK			(0x3 << VTIMEGEN_Y2RMD_SHIFT)
#define VTIMEGEN_Y2REN_MASK			(0x1 << VTIMEGEN_Y2REN_SHIFT)
#define VTIMEGEN_H2H_MASK			(0xFF << VTIMEGEN_H2H_SHIFT)

/*
 * VIQE Interrupt Register
 */
#define VINT_MPMI_SHIFT			(10) // SHOULD BE 0
#define VINT_MDNI_SHIFT			(9)  // SHOULD BE 0
#define VINT_MDII_SHIFT			(8)  // De-interlacer Interrupt enable
#define VINT_PMI_SHIFT			(2)  // SHOULD BE 0
#define VINT_DNI_SHIFT			(1)  // SHOULD BE 0
#define VINT_DII_SHIFT			(0)  // De-interlacer Interrupt status

#define VINT_MPMI_MASK			(0x1 << VINT_MPMI_SHIFT)
#define VINT_MDNI_MASK			(0x1 << VINT_MDNI_SHIFT)
#define VINT_MDII_MASK			(0x1 << VINT_MDII_SHIFT)
#define VINT_PMI_MASK			(0x1 << VINT_PMI_SHIFT)
#define VINT_DNI_MASK			(0x1 << VINT_DNI_SHIFT)
#define VINT_DII_MASK			(0x1 << VINT_DII_SHIFT)

/*
 * VIQE Miscellaneous Register
 */
#define VMISC_GED_SHIFT				(3) // Don't generate EOF Signal
#define VMISC_SDDU_SHIFT			(2) // Don't use Stream Info. for de-interlacer
#define VMISC_TSDU_SHIFT			(1) // Don't use Size Info.
#define VMISC_GENDU_SHIFT			(0) // Don't use Global ENable

#define VMISC_GED_MASK				(0x1 << VMISC_GED_SHIFT)
#define VMISC_SDDU_MASK				(0x1 << VMISC_SDDU_SHIFT)
#define VMISC_TSDU_MASK				(0x1 << VMISC_TSDU_SHIFT)
#define VMISC_GENDU_MASK			(0x1 << VMISC_GENDU_SHIFT)

/*
 * De-interlacer Base n Register
 */
#define DI_BASE_BASE_SHIFT			(0)

#define DI_BASE_BASE_MASK			(0xFFFFFFFF << DI_BASE_BASE_SHIFT)

/*
 * De-interlacer Size Register
 */
#define DI_SIZE_HEIGHT_SHIFT		(16) // Input Image Height by pixel
#define DI_SIZE_WIDTH_SHIFT			(0)  // Input Image Width by pixel

#define DI_SIZE_HEIGHT_MASK			(0x7FF << DI_SIZE_HEIGHT_SHIFT)
#define DI_SIZE_WIDTH_MASK			(0x7FF << DI_SIZE_WIDTH_SHIFT)

/*
 * De-interlacer Offset Register
 */
#define DI_OFFS_OFFS1_SHIFT			(16) // Address offset in chrominance by pixel
#define DI_OFFS_OFFS0_SHIFT			(0)  // Address offset in luminance by pixel

#define DI_OFFS_OFFS1_MASK			(0xFFFF << DI_OFFS_OFFS1_SHIFT)
#define DI_OFFS_OFFS0_MASK			(0xFFFF << DI_OFFS_OFFS0_SHIFT)

/*
 * De-interlacer Control Register
 */
#define DI_CTRL_H2H_SHIFT			(24) // the pixel count between the ending pixel of the line and the first pixel of the next line
#define DI_CTRL_FRD_SHIFT			(18) // Monitoring Frame Rate
#define DI_CTRL_CFGUPD_SHIFT		(17) // Set Register
#define DI_CTRL_EN_SHIFT			(16) // De-interlacer DMA Enable
#define DI_CTRL_FRMNUM_SHIFT		(8)  // Operation Frame Number
#define DI_CTRL_UVINTPL_SHIFT		(2)  // FOR DEBUG
#define DI_CTRL_SDDU_SHIFT			(1)  // Don't use Stream Info. for de-interlacer
#define DI_CTRL_TSDU_SHIFT			(0)  // Don't use Size Info. for de-interlacer

#define DI_CTRL_H2H_MASK			(0xFF << DI_CTRL_H2H_SHIFT)
#define DI_CTRL_FRD_MASK			(0x1 << DI_CTRL_FRD_SHIFT)
#define DI_CTRL_CFGUPD_MASK			(0x1 << DI_CTRL_CFGUPD_SHIFT)
#define DI_CTRL_EN_MASK				(0x1 << DI_CTRL_EN_SHIFT)
#define DI_CTRL_FRMNUM_MASK			(0x3 << DI_CTRL_FRMNUM_SHIFT)
#define DI_CTRL_UVINTPL_MASK		(0x1 << DI_CTRL_UVINTPL_SHIFT)
#define DI_CTRL_SDDU_MASK			(0x1 << DI_CTRL_SDDU_SHIFT)
#define DI_CTRL_TSDU_MASK			(0x1 << DI_CTRL_TSDU_SHIFT)

/*
 * De-interlacer Decomp. n MISC. Register
 */
#define DI_DEC_MISC_FMT_SHIFT			(12) // Deinterlacer decompressor format
#define DI_DEC_MISC_DEC_DIV_SHIFT		(8)  // The divisor value of stream decompressor
#define DI_DEC_MISC_SF_SHIFT			(3)  // Set to change the stream size to maximum value
#define DI_DEC_MISC_ECR_SHIFT			(2)  // SHOULD BE 1
#define DI_DEC_MISC_FLUSH_SHIFT			(1)  // The status of frame decompressor flush
#define DI_DEC_MISC_DE_SHIFT			(0)  // Detect EndStream

#define DI_DEC_MISC_FMT_MASK			(0xF << DI_DEC_MISC_FMT_SHIFT)
#define DI_DEC_MISC_DEC_DIV_MASK		(0x3 << DI_DEC_MISC_DEC_DIV_SHIFT)
#define DI_DEC_MISC_SF_MASK				(0x1 << DI_DEC_MISC_SF_SHIFT)
#define DI_DEC_MISC_ECR_MASK			(0x1 << DI_DEC_MISC_ECR_SHIFT)
#define DI_DEC_MISC_FLUSH_MASK			(0x1 << DI_DEC_MISC_FLUSH_SHIFT)
#define DI_DEC_MISC_DE_MASK				(0x1 << DI_DEC_MISC_DE_SHIFT)

/*
 * De-interlacer Decomp. n Size Register
 */
#define DI_DEC_SIZE_HEIGHT_SHIFT	(16)			// Image Height divided by 2
#define DI_DEC_SIZE_WIDTH_SHIFT		(0)				// Image Width

#define DI_DEC_SIZE_HEIGHT_MASK		(0x7FF << DI_DEC_SIZE_HEIGHT_SHIFT)
#define DI_DEC_SIZE_WIDTH_MASK		(0x7FF << DI_DEC_SIZE_WIDTH_SHIFT)

/*
 * De-interlacer Decomp. n Status Register
 */
#define DI_DEC_STS_DEC_STS_SHIFT				(28) // Status of decoder core
#define DI_DEC_STS_DEC_ER_SHIFT					(27) // Error of decoder core
#define DI_DEC_STS_HR_ER_SHIFT					(26) // Error of decoding header
#define DI_DEC_STS_EOFO_SHIFT					(25) // Output EOF of frame decompressor
#define DI_DEC_STS_EOFI_SHIFT					(24) // Input EOF of VIOC
#define DI_DEC_STS_EMPTY_SHIFT					(20) // Empty Status in Buffer
#define DI_DEC_STS_FULL_SHIFT					(16) // Full Status in buffer
#define DI_DEC_STS_WIDTH_DWALIGN_SHIFT			(0)

#define DI_DEC_STS_DEC_STS_MASK (0xF << DI_DEC_STS_DEC_STS_SHIFT)
#define DI_DEC_STS_DEC_ER_MASK  (0x1 << DI_DEC_STS_DEC_ER_SHIFT)
#define DI_DEC_STS_HR_ER_MASK   (0x1 << DI_DEC_STS_HR_ER_SHIFT)
#define DI_DEC_STS_EOFO_MASK    (0x1 << DI_DEC_STS_EOFO_SHIFT)
#define DI_DEC_STS_EOFI_MASK    (0x1 << DI_DEC_STS_EOFI_SHIFT)
#define DI_DEC_STS_EMPTY_MASK   (0xF << DI_DEC_STS_EMPTY_SHIFT)
#define DI_DEC_STS_FULL_MASK    (0xF << DI_DEC_STS_FULL_SHIFT)
#define DI_DEC_STS_WIDTH_DWALIGN_MASK \
	(0xFFFF << DI_DEC_STS_WIDTH_DWALIGN_SHIFT)

/*
 * De-intelracer Decomp. n Contrl Register
 */
#define DI_DEC_CTRL_EN_SHIFT			(31) // frame decompressor enable status
#define DI_DEC_CTRL_STS_SHIFT			(16) // the status of frame decompressor
#define DI_DEC_CTRL_HEADER_EN_SHIFT		(9)  // Check stream header error
#define DI_DEC_CTRL_ER_CK_SHIFT			(8)  // Check decoder core error
#define DI_DEC_CTRL_SELECT_SHIFT		(0)  // NOT USED

#define DI_DEC_CTRL_EN_MASK        (0x1 << DI_DEC_CTRL_EN_SHIFT)
#define DI_DEC_CTRL_STS_MASK       (0xF << DI_DEC_CTRL_STS_SHIFT)
#define DI_DEC_CTRL_HEADER_EN_MASK (0x1 << DI_DEC_CTRL_HEADER_EN_SHIFT)
#define DI_DEC_CTRL_ER_CK_MASK     (0x1 << DI_DEC_CTRL_ER_CK_SHIFT)
#define DI_DEC_CTRL_SELECT_MASK    (0xFF << DI_DEC_CTRL_SELECT_SHIFT)

/*
 * De-interlacer Comp. 0 MISC. Register
 */
#define DI_COM0_MISC_FMT_SHIFT			(12) // Frame compressor format
#define DI_COM0_MISC_ENC_DIV_SHIFT		(8)  // The divisor value of stream compressor
#define DI_COM0_MISC_SF_SHIFT			(3)  // Stream size is set to maximum value
#define DI_COM0_MISC_FLUSH_SHIFT		(1)  // frame compressor is flushed
#define DI_COM0_MISC_DE_SHIFT			(0)  // NOT USED

#define DI_COM0_MISC_FMT_MASK     (0xF << DI_COM0_MISC_FMT_SHIFT)
#define DI_COM0_MISC_ENC_DIV_MASK (0x3 << DI_COM0_MISC_ENC_DIV_SHIFT)
#define DI_COM0_MISC_SF_MASK      (0x1 << DI_COM0_MISC_SF_SHIFT)
#define DI_COM0_MISC_FLUSH_MASK   (0x1 << DI_COM0_MISC_FLUSH_SHIFT)
#define DI_COM0_MISC_DE_MASK      (0x1 << DI_COM0_MISC_DE_SHIFT)

/*
 * De-intelracer Comp. 0 Status Register
 */
#define DI_COM0_STS_EOFO_SHIFT			(25) // Output EOF of frame compressor
#define DI_COM0_STS_EOFI_SHIFT			(24) // Input EOF of VIOC
#define DI_COM0_STS_EMPTY_SHIFT			(20) // Empty Status in Buffer
#define DI_COM0_STS_FULL_SHIFT			(16) // Full Status in Buffer

#define DI_COM0_STS_EOFO_MASK			(0x1 << DI_COM0_STS_EOFO_SHIFT)
#define DI_COM0_STS_EOFI_MASK			(0x1 << DI_COM0_STS_EOFI_SHIFT)
#define DI_COM0_STS_EMPTY_MASK			(0xF << DI_COM0_STS_EMPTY_SHIFT)
#define DI_COM0_STS_FULL_MASK			(0xF << DI_COM0_STS_FULL_SHIFT)

/*
 * De-intelracer Comp. 0 Control Register
 */
#define DI_COM0_CTRL_EN_SHIFT			(31) // frame compressor enable status
#define DI_COM0_CTRL_STS_SHIFT			(16) // the status of frame compressor
#define DI_COM0_CTRL_SELECT_SHIFT		(0)  // NOT USED

#define DI_COM0_CTRL_EN_MASK     (0x1 << DI_COM0_CTRL_EN_SHIFT)
#define DI_COM0_CTRL_STS_MASK    (0xF << DI_COM0_CTRL_STS_SHIFT)
#define DI_COM0_CTRL_SELECT_MASK (0xFF << DI_COM0_CTRL_SELECT_SHIFT)

/*
 * De-interlacer Comp. 0 AC_Length Register
 */
#define DI_COM0_AC_K2_AC_SHIFT			(16) // K2 AC Length of frame encoder core
#define DI_COM0_AC_K1_AC_SHIFT			(8)  // K1 AC Length of frame encoder core
#define DI_COM0_AC_K0_AC_SHIFT			(0)  // K0 AC Length of frame encoder core

#define DI_COM0_AC_K2_AC_MASK			(0x3F << DI_COM0_AC_K2_AC_SHIFT)
#define DI_COM0_AC_K1_AC_MASK			(0x3F << DI_COM0_AC_K1_AC_SHIFT)
#define DI_COM0_AC_K0_AC_MASK			(0x3F << DI_COM0_AC_K0_AC_SHIFT)

/*
 * De-interlacer Control Register #2
 */
#define DI_CTRL2_CLRIR_SHIFT  (31) // Clear Internal
#define DI_CTRL2_FLSFF_SHIFT  (30) // Flush synchronizer FIFO
#define DI_CTRL2_BYPASS_SHIFT (29) // Bypass Register
#define DI_CTRL2_PDCLFI_SHIFT (15) // Clear Internal Frame Index
#define DI_CTRL2_PDPF_SHIFT   (11) // Use Prevention-flag in Pulldown Detector
#define DI_CTRL2_PDJUD_SHIFT  (9)  // Judder Detection in Pulldown Detector
#define DI_CTRL2_PDEN_SHIFT   (8)  // Pulldown Detector
#define DI_CTRL2_YDM_SHIFT    (7)  // YD mode
#define DI_CTRL2_GTHSJ_SHIFT  (6)  // Generate Jaggy Checker Threshold
#define DI_CTRL2_MRSP_SHIFT   (5)  // Pixel Output Range Enable in Spatial Processing
#define DI_CTRL2_MRTM_SHIFT   (4)  // Pixel Output Range Enable in Temporal Processing
#define DI_CTRL2_JT_SHIFT     (2)  // Temporal Jaggycheck
#define DI_CTRL2_JSP_SHIFT    (1)  // Temporal Jaggycheck
#define DI_CTRL2_JS_SHIFT     (0)  // Spatio Jaggycheck

#define DI_CTRL2_CLRIR_MASK  (0x1 << DI_CTRL2_CLRIR_SHIFT)
#define DI_CTRL2_FLSFF_MASK  (0x1 << DI_CTRL2_FLSFF_SHIFT)
#define DI_CTRL2_BYPASS_MASK (0x1 << DI_CTRL2_BYPASS_SHIFT)
#define DI_CTRL2_PDCLFI_MASK (0x1 << DI_CTRL2_PDCLFI_SHIFT)
#define DI_CTRL2_PDPF_MASK   (0x1 << DI_CTRL2_PDPF_SHIFT)
#define DI_CTRL2_PDJUD_MASK  (0x1 << DI_CTRL2_PDJUD_SHIFT)
#define DI_CTRL2_PDEN_MASK   (0x1 << DI_CTRL2_PDEN_SHIFT)
#define DI_CTRL2_YDM_MASK    (0x1 << DI_CTRL2_YDM_SHIFT)
#define DI_CTRL2_GTHSJ_MASK  (0x1 << DI_CTRL2_GTHSJ_SHIFT)
#define DI_CTRL2_MRSP_MASK   (0x1 << DI_CTRL2_MRSP_SHIFT)
#define DI_CTRL2_MRTM_MASK   (0x1 << DI_CTRL2_MRTM_SHIFT)
#define DI_CTRL2_JT_MASK     (0x1 << DI_CTRL2_JT_SHIFT)
#define DI_CTRL2_JSP_MASK    (0x1 << DI_CTRL2_JSP_SHIFT)
#define DI_CTRL2_JS_MASK     (0x1 << DI_CTRL2_JS_SHIFT)

/*
 * De-interlacer Engine 0 Register
 */
#define DI_ENGINE0_DMTSADC_SHIFT (24) // SAD Threshold in Motion Detection for Chrominance
#define DI_ENGINE0_DMTPXLC_SHIFT (16) // Pixel Threshold in Motion Detection for Chrominance
#define DI_ENGINE0_DMTPXL_SHIFT  (8)  // Pixel Threshold in Motion Detection for Luminance
#define DI_ENGINE0_DMTSAD_SHIFT  (0)  // SAD Threshold in Motion Detection for Luminance

#define DI_ENGINE0_DMTSADC_MASK (0xFF << DI_ENGINE0_DMTSADC_SHIFT)
#define DI_ENGINE0_DMTPXLC_MASK (0xFF << DI_ENGINE0_DMTPXLC_SHIFT)
#define DI_ENGINE0_DMTPXL_MASK  (0xFF << DI_ENGINE0_DMTPXL_SHIFT)
#define DI_ENGINE0_DMTSAD_MASK  (0xFF << DI_ENGINE0_DMTSAD_SHIFT)

/*
 * De-interlacer Engine 1 Register
 */
#define DI_ENGINE1_THSJMAX_SHIFT (24) // Maximum Threshold in Jaggy Threshold Generator
#define DI_ENGINE1_THSJMIN_SHIFT (16) // Minimum Threshold in Jaggy Threshold Generator
#define DI_ENGINE1_LRT_SHIFT     (14) // Horizontal Reference Type
#define DI_ENGINE1_LRL_SHIFT     (9)  // Horizontal Reference Length
#define DI_ENGINE1_LRD_SHIFT     (8)  // Horizontal Reference Disable
#define DI_ENGINE1_DIRLT_SHIFT   (4)  // Spatial Edge Direction Detector Length Type
#define DI_ENGINE1_GLD_SHIFT     (3)  // Small Angle Detection Disable
#define DI_ENGINE1_JDH_SHIFT     (2)  // Jaggy Detection Half Divider

#define DI_ENGINE1_THSJMAX_MASK (0xFF << DI_ENGINE1_THSJMAX_SHIFT)
#define DI_ENGINE1_THSJMIN_MASK (0xFF << DI_ENGINE1_THSJMIN_SHIFT)
#define DI_ENGINE1_LRT_MASK     (0x3 << DI_ENGINE1_LRT_SHIFT)
#define DI_ENGINE1_LRL_MASK     (0x1F << DI_ENGINE1_LRL_SHIFT)
#define DI_ENGINE1_LRD_MASK     (0x1 << DI_ENGINE1_LRD_SHIFT)
#define DI_ENGINE1_DIRLT_MASK   (0xF << DI_ENGINE1_DIRLT_SHIFT)
#define DI_ENGINE1_GLD_MASK     (0x1 << DI_ENGINE1_GLD_SHIFT)
#define DI_ENGINE1_JDH_MASK     (0x1 << DI_ENGINE1_JDH_SHIFT)

/*
 * De-interlacer Engine 2 Register
 */
#define DI_ENGINE2_A_THS_SHIFT  (20) // Threshold of Early determination
#define DI_ENGINE2_E_THS_SHIFT  (8)  // Threshold of Adaptive Edge determination
#define DI_ENGINE2_EP2_SHIFT    (7)  // Early determination Suppress Algorithm 2 Disable
#define DI_ENGINE2_EP1_SHIFT    (6)  // Early determination Suppress Algorithm 1 Disable
#define DI_ENGINE2_OPPD_SHIFT   (2)  // Edge Adaptive Direction determination Disable
#define DI_ENGINE2_EARLYD_SHIFT (1)  // Early determination Disable
#define DI_ENGINE2_ADAPD_SHIFT  (0)  // Adaptive Edge determination Disable

#define DI_ENGINE2_A_THS_MASK  (0xFFF << DI_ENGINE2_A_THS_SHIFT)
#define DI_ENGINE2_E_THS_MASK  (0xFFF << DI_ENGINE2_E_THS_SHIFT)
#define DI_ENGINE2_EP2_MASK	   (0x1 << DI_ENGINE2_EP2_SHIFT)
#define DI_ENGINE2_EP1_MASK	   (0x1 << DI_ENGINE2_EP1_SHIFT)
#define DI_ENGINE2_OPPD_MASK   (0x1 << DI_ENGINE2_OPPD_SHIFT)
#define DI_ENGINE2_EARLYD_MASK (0x1 << DI_ENGINE2_EARLYD_SHIFT)
#define DI_ENGINE2_ADAPD_MASK  (0x1 << DI_ENGINE2_ADAPD_SHIFT)

/*
 * De-interlacer Engine 3 Register
 */
#define DI_ENGINE3_STTHW_SHIFT (20) // Threshold Weight Parameter of Stationary Detection
#define DI_ENGINE3_STTHM_SHIFT (8)  // Threshold Multiplier Parameter of Stationary Detection
#define DI_ENGINE3_STTHD_SHIFT (0)  // Adaptive Stationary Checker Disable

#define DI_ENGINE3_STTHW_MASK (0xFFF << DI_ENGINE3_STTHW_SHIFT)
#define DI_ENGINE3_STTHM_MASK (0xFFF << DI_ENGINE3_STTHM_SHIFT)
#define DI_ENGINE3_STTHD_MASK (0x1 << DI_ENGINE3_STTHD_SHIFT)

/*
 * De-interlacer Engine 4 Register
 */
#define DI_ENGINE4_VARLG_SHIFT  (28) // Variance small Stationary range
#define DI_ENGINE4_VARR1_SHIFT  (24) // Variance dynamic Threshold ratio 1
#define DI_ENGINE4_VARR0_SHIFT  (20) // Variance dynamic Threshold ratio 0
#define DI_ENGINE4_DYNVAR_SHIFT (16) // Derivative Variance Minimum Threshold
#define DI_ENGINE4_HPFVAR_SHIFT (12) // Derivative Variance Threshold for HPF output
#define DI_ENGINE4_HPFFLD_SHIFT (8)  // HPF Threshold of Field Difference
#define DI_ENGINE4_HPFFRM_SHIFT (4)  // HPF Threshold of Frame Difference
#define DI_ENGINE4_DMDC_SHIFT   (1)  // Stationary Checker Mode in Chrominance
#define DI_ENGINE4_DMDL_SHIFT   (0)  // Stationary Checker Mode in Luminance

#define DI_ENGINE4_VARLG_MASK  (0xF << DI_ENGINE4_VARLG_SHIFT)
#define DI_ENGINE4_VARR1_MASK  (0xF << DI_ENGINE4_VARR1_SHIFT)
#define DI_ENGINE4_VARR0_MASK  (0xF << DI_ENGINE4_VARR0_SHIFT)
#define DI_ENGINE4_DYNVAR_MASK (0xF << DI_ENGINE4_DYNVAR_SHIFT)
#define DI_ENGINE4_HPFVAR_MASK (0xF << DI_ENGINE4_HPFVAR_SHIFT)
#define DI_ENGINE4_HPFFLD_MASK (0xF << DI_ENGINE4_HPFFLD_SHIFT)
#define DI_ENGINE4_HPFFRM_MASK (0xF << DI_ENGINE4_HPFFRM_SHIFT)
#define DI_ENGINE4_DMDC_MASK   (0x1 << DI_ENGINE4_DMDC_SHIFT)
#define DI_ENGINE4_DMDL_MASK   (0x1 << DI_ENGINE4_DMDL_SHIFT)

/*
 * Pulldown Detector Threshold 0 Register
 */
#define PD_THRES0_ZP_SHIFT      (24) // Avoidance of zero difference
#define PD_THRES0_CNTSCO_SHIFT  (20) // Threshold for counter value of Pulldown Prevention-Flag
#define PD_THRES0_CO_SHIFT      (16) // Value of Pulldown Detection type
#define PD_THRES0_UVALDIS_SHIFT (15) // Not Use User-defined Threshold Value in Prevention-Flag
#define PD_THRES0_WEIGHT_SHIFT  (10) // Weight Value for Threshold of Pull Prevention-Flag
#define PD_THRES0_CNTS_SHIFT    (0)  // Threshold for counter value of Pulldown checker

#define PD_THRES0_ZP_MASK      (0x1 << PD_THRES0_ZP_SHIFT)
#define PD_THRES0_CNTSCO_MASK  (0xF << PD_THRES0_CNTSCO_SHIFT)
#define PD_THRES0_CO_MASK      (0x3 << PD_THRES0_CO_SHIFT)
#define PD_THRES0_UVALDIS_MASK (0x1 << PD_THRES0_UVALDIS_SHIFT)
#define PD_THRES0_WEIGHT_MASK  (0x1F << PD_THRES0_WEIGHT_SHIFT)
#define PD_THRES0_CNTS_MASK    (0x3FF << PD_THRES0_CNTS_SHIFT)

/*
 * Pulldown Detector Threshold 1 Register
 */
#define PD_THRES1_THRES2_SHIFT (16) // Threshold for pixel difference value of third level
#define PD_THRES1_THRES1_SHIFT (8)  // Threshold for pixel difference value of second level
#define PD_THRES1_THRES0_SHIFT (0)  // Threshold for pixel difference value of first level

#define PD_THRES1_THRES2_MASK (0xFF << PD_THRES1_THRES2_SHIFT)
#define PD_THRES1_THRES1_MASK (0xFF << PD_THRES1_THRES1_SHIFT)
#define PD_THRES1_THRES0_MASK (0xFF << PD_THRES1_THRES0_SHIFT)

/*
 * Pulldown Detector Judder Register
 */
#define PD_JUDDER_THSJDMAX_SHIFT (24) // Threshold maximum boundary for Pulldown Judder Detector
#define PD_JUDDER_THSJDMIN_SHIFT (16) // Threshold Minimum boundary for Pulldown Judder Detector
#define PD_JUDDER_HORLINE_SHIFT  (12) // Horizontal Margin for Judder Elimination Processing
#define PD_JUDDER_DNLINE_SHIFT   (8)  // Downward Margin for Judder Elimination Processor
#define PD_JUDDER_CNTS_SHIFT     (0)  // Threshold for counter of Judder pixels

#define PD_JUDDER_THSJDMAX_MASK (0xFF << PD_JUDDER_THSJDMAX_SHIFT)
#define PD_JUDDER_THSJDMIN_MASK (0xFF << PD_JUDDER_THSJDMIN_SHIFT)
#define PD_JUDDER_HORLINE_MASK  (0xF << PD_JUDDER_HORLINE_SHIFT)
#define PD_JUDDER_DNLINE_MASK   (0xF << PD_JUDDER_DNLINE_SHIFT)
#define PD_JUDDER_CNTS_MASK     (0xFF << PD_JUDDER_CNTS_SHIFT)

/*
 * Pulldown Detector Judder Misc. Register
 */
#define PD_JUDDER_M_MULDMT_SHIFT  (17) // Multiplier Stationary Checker Threshold
#define PD_JUDDER_M_NOJDS_SHIFT   (16) // Prevention count Stationary pixels
#define PD_JUDDER_M_THSJDS2_SHIFT (8)  // Threshold Pulldown Judder for Two Buffers mode
#define PD_JUDDER_M_JDH_SHIFT     (0)  // Judder Detection Half Divider

#define PD_JUDDER_M_MULDMT_MASK  (0x7F << PD_JUDDER_M_MULDMT_SHIFT)
#define PD_JUDDER_M_NOJDS_MASK   (0x1 << PD_JUDDER_M_NOJDS_SHIFT)
#define PD_JUDDER_M_THSJDS2_MASK (0xFF << PD_JUDDER_M_THSJDS2_SHIFT)
#define PD_JUDDER_M_JDH_MASK     (0x1 << PD_JUDDER_M_JDH_SHIFT)

/*
 * De-interlacer Status Register
 */
#define DI_STATUS_POS_Y_SHIFT		(19) // Current Vertical Position
#define DI_STATUS_POS_X_SHIFT		(8)  // Current Horizontal Position
#define DI_STATUS_BUSY_SHIFT		(0)  // De-interlacer Busy

#define DI_STATUS_POS_Y_MASK		(0x7FF << DI_STATUS_POS_Y_SHIFT)
#define DI_STATUS_POS_X_MASK		(0x7FF << DI_STATUS_POS_X_SHIFT)
#define DI_STATUS_BUSY_MASK			(0x1 << DI_STATUS_BUSY_SHIFT)

/*
 * Pulldown Detector Status Register
 */
#define PD_STATUS_MPOFF_SHIFT		(8) // Minimum Position Offfset
#define PD_STATUS_CO_SHIFT			(4) // Value of Pulldown Detection type
#define PD_STATUS_STATE_SHIFT		(0) // Current FSM State

#define PD_STATUS_MPOFF_MASK		(0x7 << PD_STATUS_MPOFF_SHIFT)
#define PD_STATUS_CO_MASK			(0x3 << PD_STATUS_CO_SHIFT)
#define PD_STATUS_STATE_MASK		(0xF << PD_STATUS_STATE_SHIFT)

/*
 * De-interlacer Region 0 Register
 */
#define DI_REGION0_EN_SHIFT			(31) // Region Selection Enable
#define DI_REGION0_XEND_SHIFT		(16) // Last Horizontal Position of Region Selection
#define DI_REGION0_XSTART_SHIFT		(0)  // First Horizontal Position of Region Selection

#define DI_REGION0_EN_MASK			(0x1 << DI_REGION0_EN_SHIFT)
#define DI_REGION0_XEND_MASK		(0x3FF << DI_REGION0_XEND_SHIFT)
#define DI_REGION0_XSTART_MASK		(0x3FF << DI_REGION0_XSTART_SHIFT)

/*
 * De-interlacer Region 1 Register
 */
#define DI_REGION1_YEND_SHIFT		(16) // Last Vertical Position of Region Selection
#define DI_REGION1_YSTART_SHIFT		(0)  // First Vertical Position of Region Selection

#define DI_REGION1_YEND_MASK		(0x3FF << DI_REGION1_YEND_SHIFT)
#define DI_REGION1_YSTART_MASK		(0x3FF << DI_REGION1_YSTART_SHIFT)

/*
 * De-interlacer Interrupt Register
 */
#define DI_INT_MINT_SHIFT		(16) // Interrupt enable
#define DI_INT_INT_SHIFT		(0)  // clear Interrupt

#define DI_INT_MINT_MASK		(0x1 << DI_INT_MINT_SHIFT)
#define DI_INT_INT_MASK			(0x1 << DI_INT_INT_SHIFT)

/*
 * De-interlace Pulldown SAW Register
 */
#define DI_PD_SAW_SAWEN_SHIFT		(31)
#define DI_PD_SAW_SAW_DUR_SHIFT		(16)
#define DI_PD_SAW_FRM_THR_SHIFT		(8)
#define DI_PD_SAW_FLD_THR_SHIFT		(0)

#define DI_PD_SAW_SAWEN_MASK		(0x1 << DI_PD_SAW_SAWEN_SHIFT)
#define DI_PD_SAW_SAW_DUR_MASK		(0xFF << DI_PD_SAW_SAW_DUR_SHIFT)
#define DI_PD_SAW_FRM_THR_MASK		(0xFF << DI_PD_SAW_FRM_THR_SHIFT)
#define DI_PD_SAW_FLD_THR_MASK		(0xFF << DI_PD_SAW_FLD_THR_SHIFT)

/*
 * De-interlacer Core Size Register
 */
#define DI_CSIZE_HEIGHT_SHIFT (16) // Input Image Width by pixel
#define DI_CSIZE_WIDTH_SHIFT  (0)  // Input Image Height by pixel

#define DI_CSIZE_HEIGHT_MASK		(0x7FF << DI_CSIZE_HEIGHT_SHIFT)
#define DI_CSIZE_WIDTH_MASK			(0x7FF << DI_CSIZE_WIDTH_SHIFT)

/*
 * De-interlacer Format Register
 */
#define DI_FMT_TFCD_SHIFT (31) // SHOULD BE 0
#define DI_FMT_TSDU_SHIFT (16) // Don't use Size Info.
#define DI_FMT_F422_SHIFT (0)  // De-interlacer format selection

#define DI_FMT_TFCD_MASK		(0x1 << DI_FMT_TFCD_SHIFT)
#define DI_FMT_TSDU_MASK		(0x1 << DI_FMT_TSDU_SHIFT)
#define DI_FMT_F422_MASK		(0x1 << DI_FMT_F422_SHIFT)

/* Interface APIs */
extern void VIOC_VIQE_InitDeintlCoreTemporal(
	volatile void __iomem *reg);
extern void VIOC_VIQE_SetImageSize(volatile void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIQE_SetImageY2RMode(volatile void __iomem *reg,
	unsigned int y2r_mode);
extern void VIOC_VIQE_SetImageY2REnable(volatile void __iomem *reg,
	unsigned int enable);
extern void VIOC_VIQE_SetControlMisc(volatile void __iomem *reg,
	unsigned int no_hor_intpl, unsigned int fmt_conv_disable,
	unsigned int fmt_conv_disable_using_fmt, unsigned int update_disable,
	unsigned int cfgupd, unsigned int h2h);
extern void VIOC_VIQE_SetControlDontUse(volatile void __iomem *reg,
	unsigned int global_en_dont_use, unsigned int top_size_dont_use,
	unsigned int stream_deintl_info_dont_use);
extern void VIOC_VIQE_SetControlClockGate(volatile void __iomem *reg,
	unsigned int deintl_dis, unsigned int d3d_dis, unsigned int pm_dis);
extern void VIOC_VIQE_SetControlEnable(volatile void __iomem *reg,
	unsigned int his_cdf_or_lut_en, unsigned int his_en,
	unsigned int gamut_en, unsigned int denoise3d_en,
	unsigned int deintl_en);
extern void VIOC_VIQE_SetControlMode(volatile void __iomem *reg,
	unsigned int his_cdf_or_lut_en, unsigned int his_en,
	unsigned int gamut_en, unsigned int denoise3d_en,
	unsigned int deintl_en);
extern void VIOC_VIQE_SetControlRegister(volatile void __iomem *reg,
	unsigned int width, unsigned int height, unsigned int fmt);

extern void VIOC_VIQE_SetDeintlBase(volatile void __iomem *reg,
	unsigned int frmnum, unsigned int base0, unsigned int base1,
	unsigned int base2, unsigned int base3);
extern void VIOC_VIQE_SwapDeintlBase(volatile void __iomem *reg, int mode);
extern void VIOC_VIQE_SetDeintlSize(volatile void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIQE_SetDeintlMisc(volatile void __iomem *reg,
	unsigned int uvintpl, unsigned int cfgupd, unsigned int dma_enable,
	unsigned int h2h, unsigned int top_size_dont_use);
extern void VIOC_VIQE_SetDeintlControl(volatile void __iomem *reg,
	unsigned int fmt, unsigned int eof_control_ready,
	unsigned int dec_divisor, unsigned int ac_k0_limit,
	unsigned int ac_k1_limit, unsigned int ac_k2_limit);
extern void VIOC_VIQE_SetDeintlFMT(volatile void __iomem *reg, int enable);
extern void VIOC_VIQE_SetDeintlMode(volatile void __iomem *reg,
	VIOC_VIQE_DEINTL_MODE mode);
extern void VIOC_VIQE_SetDeintlRegion(volatile void __iomem *reg,
	int region_enable, int region_idx_x_start,
	int region_idx_x_end, int region_idx_y_start, int region_idx_y_end);
extern void VIOC_VIQE_SetDeintlCore(volatile void __iomem *reg,
	unsigned int width, unsigned int height,
	VIOC_VIQE_FMT_TYPE fmt, unsigned int bypass,
	unsigned int top_size_dont_use);
extern void VIOC_VIQE_SetDeintlRegister(volatile void __iomem *reg,
	unsigned int fmt, unsigned int top_size_dont_use,
	unsigned int width, unsigned int height,
	VIOC_VIQE_DEINTL_MODE mode, unsigned int base0,
	unsigned int base1, unsigned int base2, unsigned int base3);
extern void VIOC_VIQE_SetDeintlJudderCnt(volatile void __iomem *reg,
	unsigned int cnt);
extern void VIOC_VIQE_InitDeintlCoreVinMode(volatile void __iomem *reg);
extern void VIOC_VIQE_IgnoreDecError(volatile void __iomem *reg,
	int sf, int er_ck, int hrer_en);
extern void VIOC_VIQE_DUMP(volatile void __iomem *reg, unsigned int vioc_id);
extern volatile void __iomem *VIOC_VIQE_GetAddress(unsigned int vioc_id);

#endif //__VIOC_VIQE_H__
