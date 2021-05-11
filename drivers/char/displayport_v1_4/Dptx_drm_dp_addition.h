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

/*******************************************************************************


*   Modified by Telechips Inc.


*   Modified date :


*   Description :


*******************************************************************************/


#ifndef __DRM_DP_ADDITIONS_H__
#define __DRM_DP_ADDITIONS_H__

#include <asm/div64.h>
#include <linux/kernel.h>
#include <linux/bits.h>
#include <linux/math64.h>


/*
 *		Helper additions - drm_dp_helper.h
 */


/*
 * The following aren't defined in kernel headers 
 */

#define DP_LINK_STATUS_SIZE	  			6

#define DP_BRANCH_OUI_HEADER_SIZE		0xc
#define DP_RECEIVER_CAP_SIZE			0xf
#define DP_DSC_RECEIVER_CAP_SIZE        0xf
#define EDP_PSR_RECEIVER_CAP_SIZE		2
#define EDP_DISPLAY_CTL_CAP_SIZE		3

#define DP_LINK_BW_8_1					0x1e
#define DP_TRAINING_PATTERN_4			7
#define DP_TPS4_SUPPORTED				BIT( 7 )


#define DP_PSR2_WITH_Y_COORD_IS_SUPPORTED  3      /* eDP 1.4a */


#define DP_DSC_SUPPORT                      0x060   /* DP 1.4 */
#define DP_DSC_DECOMPRESSION_IS_SUPPORTED  (1 << 0)

#define DP_DSC_REV                          0x061
#define DP_DSC_MAJOR_MASK                  (0xf << 0)
#define DP_DSC_MINOR_MASK                  (0xf << 4)
#define DP_DSC_MAJOR_SHIFT                 0
#define DP_DSC_MINOR_SHIFT                 4

#define DP_DSC_RC_BUF_BLK_SIZE              0x062
#define DP_DSC_RC_BUF_BLK_SIZE_1           0x0
#define DP_DSC_RC_BUF_BLK_SIZE_4           0x1
#define DP_DSC_RC_BUF_BLK_SIZE_16          0x2
#define DP_DSC_RC_BUF_BLK_SIZE_64          0x3

#define DP_DSC_RC_BUF_SIZE                  0x063

#define DP_DSC_SLICE_CAP_1                  0x064
#define DP_DSC_1_PER_DP_DSC_SINK           (1 << 0)
#define DP_DSC_2_PER_DP_DSC_SINK           (1 << 1)
#define DP_DSC_4_PER_DP_DSC_SINK           (1 << 3)
#define DP_DSC_6_PER_DP_DSC_SINK           (1 << 4)
#define DP_DSC_8_PER_DP_DSC_SINK           (1 << 5)
#define DP_DSC_10_PER_DP_DSC_SINK          (1 << 6)
#define DP_DSC_12_PER_DP_DSC_SINK          (1 << 7)

#define DP_DSC_LINE_BUF_BIT_DEPTH           0x065
#define DP_DSC_LINE_BUF_BIT_DEPTH_MASK     (0xf << 0)
#define DP_DSC_LINE_BUF_BIT_DEPTH_9        0x0
#define DP_DSC_LINE_BUF_BIT_DEPTH_10       0x1
#define DP_DSC_LINE_BUF_BIT_DEPTH_11       0x2
#define DP_DSC_LINE_BUF_BIT_DEPTH_12       0x3
#define DP_DSC_LINE_BUF_BIT_DEPTH_13       0x4
#define DP_DSC_LINE_BUF_BIT_DEPTH_14       0x5
#define DP_DSC_LINE_BUF_BIT_DEPTH_15       0x6
#define DP_DSC_LINE_BUF_BIT_DEPTH_16       0x7
#define DP_DSC_LINE_BUF_BIT_DEPTH_8        0x8

#define DP_DSC_BLK_PREDICTION_SUPPORT       0x066
#define DP_DSC_BLK_PREDICTION_IS_SUPPORTED (1 << 0)

#define DP_DSC_MAX_BITS_PER_PIXEL_LOW       0x067   /* eDP 1.4 */

#define DP_DSC_MAX_BITS_PER_PIXEL_HI        0x068   /* eDP 1.4 */

#define DP_DSC_DEC_COLOR_FORMAT_CAP         0x069
#define DP_DSC_RGB                         (1 << 0)
#define DP_DSC_YCbCr444                    (1 << 1)
#define DP_DSC_YCbCr422_Simple             (1 << 2)
#define DP_DSC_YCbCr422_Native             (1 << 3)
#define DP_DSC_YCbCr420_Native             (1 << 4)

#define DP_DSC_DEC_COLOR_DEPTH_CAP          0x06A
#define DP_DSC_8_BPC                       (1 << 1)
#define DP_DSC_10_BPC                      (1 << 2)
#define DP_DSC_12_BPC                      (1 << 3)

#define DP_DSC_PEAK_THROUGHPUT              0x06B
#define DP_DSC_THROUGHPUT_MODE_0_MASK      (0xf << 0)
#define DP_DSC_THROUGHPUT_MODE_0_SHIFT     0
#define DP_DSC_THROUGHPUT_MODE_0_340       (1 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_400       (2 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_450       (3 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_500       (4 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_550       (5 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_600       (6 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_650       (7 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_700       (8 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_750       (9 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_800       (10 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_850       (11 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_900       (12 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_950       (13 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_1000      (14 << 0)
#define DP_DSC_THROUGHPUT_MODE_1_MASK      (0xf << 4)
#define DP_DSC_THROUGHPUT_MODE_1_SHIFT     4
#define DP_DSC_THROUGHPUT_MODE_1_340       (1 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_400       (2 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_450       (3 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_500       (4 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_550       (5 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_600       (6 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_650       (7 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_700       (8 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_750       (9 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_800       (10 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_850       (11 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_900       (12 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_950       (13 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_1000      (14 << 4)

#define DP_DSC_MAX_SLICE_WIDTH              0x06C

#define DP_DSC_SLICE_CAP_2                  0x06D
#define DP_DSC_16_PER_DP_DSC_SINK          (1 << 0)
#define DP_DSC_20_PER_DP_DSC_SINK          (1 << 1)
#define DP_DSC_24_PER_DP_DSC_SINK          (1 << 2)

#define DP_DSC_BITS_PER_PIXEL_INC           0x06F
#define DP_DSC_BITS_PER_PIXEL_1_16         0x0
#define DP_DSC_BITS_PER_PIXEL_1_8          0x1
#define DP_DSC_BITS_PER_PIXEL_1_4          0x2
#define DP_DSC_BITS_PER_PIXEL_1_2          0x3
#define DP_DSC_BITS_PER_PIXEL_1            0x4

#define DP_PSR_SUPPORT                      0x070   /* XXX 1.2? */
#define DP_PSR_IS_SUPPORTED                1
#define DP_PSR2_IS_SUPPORTED              2       /* eDP 1.4 */

#define DP_PSR_CAPS                         0x071   /* XXX 1.2? */
#define DP_PSR_NO_TRAIN_ON_EXIT            1
#define DP_PSR_SETUP_TIME_330              (0 << 1)
#define DP_PSR_SETUP_TIME_275              (1 << 1)
#define DP_PSR_SETUP_TIME_220              (2 << 1)
#define DP_PSR_SETUP_TIME_165              (3 << 1)
#define DP_PSR_SETUP_TIME_110              (4 << 1)
#define DP_PSR_SETUP_TIME_55               (5 << 1)
#define DP_PSR_SETUP_TIME_0                (6 << 1)
#define DP_PSR_NO_TRAIN_ON_EXIT            1
#define DP_PSR_SETUP_TIME_330              (0 << 1)
#define DP_PSR_SETUP_TIME_275              (1 << 1)
#define DP_PSR_SETUP_TIME_220              (2 << 1)
#define DP_PSR_SETUP_TIME_165              (3 << 1)
#define DP_PSR_SETUP_TIME_110              (4 << 1)
#define DP_PSR_SETUP_TIME_55               (5 << 1)
#define DP_PSR_SETUP_TIME_0                (6 << 1)
#define DP_PSR_SETUP_TIME_MASK             (7 << 1)
#define DP_PSR_SETUP_TIME_SHIFT            1
#define DP_PSR2_SU_Y_COORDINATE_REQUIRED   (1 << 4)  /* eDP 1.4a */
#define DP_PSR2_SU_GRANULARITY_REQUIRED    (1 << 5)  /* eDP 1.4b */

#define DP_TEST_LINK_AUDIO_PATTERN		BIT(5)
#define DP_TEST_H_TOTAL_MSB                     0x222
#define DP_TEST_H_TOTAL_LSB			0x223
#define DP_TEST_V_TOTAL_MSB                     0x224
#define DP_TEST_V_TOTAL_LSB			0x225
#define DP_TEST_H_START_MSB			0x226
#define DP_TEST_H_START_LSB			0x227
#define DP_TEST_V_START_MSB			0x228
#define DP_TEST_V_START_LSB			0x229
#define DP_TEST_H_SYNC_WIDTH_MSB		0x22A
#define DP_TEST_H_SYNC_WIDTH_LSB		0x22B
#define DP_TEST_V_SYNC_WIDTH_MSB		0x22C
#define DP_TEST_V_SYNC_WIDTH_LSB		0x22D
#define DP_TEST_H_WIDTH_MSB			0x22E
#define DP_TEST_H_WIDTH_LSB			0x22F
#define DP_TEST_V_WIDTH_MSB			0x230
#define DP_TEST_V_WIDTH_LSB			0x231
#define DP_TEST_PHY_PATTERN			0x248
#define DP_TEST_PATTERN_NONE			0x0
#define DP_TEST_PATTERN_COLOR_RAMPS		0x1
#define DP_TEST_PATTERN_BW_VERITCAL_LINES	0x2
#define DP_TEST_PATTERN_COLOR_SQUARE		0x3

#define DP_TEST_80BIT_CUSTOM_PATTERN_0		0x250
#define DP_TEST_80BIT_CUSTOM_PATTERN_1		0x251
#define DP_TEST_80BIT_CUSTOM_PATTERN_2		0x252
#define DP_TEST_80BIT_CUSTOM_PATTERN_3		0x253
#define DP_TEST_80BIT_CUSTOM_PATTERN_4		0x254
#define DP_TEST_80BIT_CUSTOM_PATTERN_5		0x255
#define DP_TEST_80BIT_CUSTOM_PATTERN_6		0x256
#define DP_TEST_80BIT_CUSTOM_PATTERN_7		0x257
#define DP_TEST_80BIT_CUSTOM_PATTERN_8		0x258
#define DP_TEST_80BIT_CUSTOM_PATTERN_9		0x259

#define DP_TEST_PHY_PATTERN_SEL_MASK		GENMASK(2, 0)
#define DP_TEST_PHY_PATTERN_NONE		0x0
#define DP_TEST_PHY_PATTERN_D10			0x1
#define DP_TEST_PHY_PATTERN_SEMC		0x2
#define DP_TEST_PHY_PATTERN_PRBS7		0x3
#define DP_TEST_PHY_PATTERN_CUSTOM		0x4
#define DP_TEST_PHY_PATTERN_CP2520_1		0x5
#define DP_TEST_PHY_PATTERN_CP2520_2		0x6
#define DP_TEST_PHY_PATTERN_CP2520_3_TPS4	0x7

#define DP_TEST_MISC				0x232

#define DP_TEST_COLOR_FORMAT_MASK		GENMASK(2, 1)
#define DP_TEST_BIT_DEPTH_MASK                  GENMASK(7, 5)
#define DP_TEST_BIT_DEPTH_SHIFT			5
#define DP_TEST_BIT_DEPTH_6			0x0
#define DP_TEST_BIT_DEPTH_8			0x1
#define DP_TEST_BIT_DEPTH_10			0x2
#define DP_TEST_BIT_DEPTH_12			0x3
#define DP_TEST_BIT_DEPTH_16			0x4

#define DP_TEST_DYNAMIC_RANGE_SHIFT             3
#define DP_TEST_DYNAMIC_RANGE_MASK		BIT(3)
#define DP_TEST_YCBCR_COEFF_SHIFT		4
#define DP_TEST_YCBCR_COEFF_MASK		BIT(4)

#define DP_TEST_DYNAMIC_RANGE_VESA		0x0
#define DP_TEST_DYNAMIC_RANGE_CEA               0x1
#define DP_TEST_COLOR_FORMAT_RGB	        0x0
#define DP_TEST_COLOR_FORMAT_YCBCR422           0x2
#define DP_TEST_COLOR_FORMAT_YCBCR444		0x4
#define DP_TEST_YCBCR_COEFF_ITU601		0x0
#define DP_TEST_YCBCR_COEFF_ITU709		0x1

#define	DP_TEST_AUDIO_MODE			0x271
#define DP_TEST_AUDIO_SAMPLING_RATE_MASK	GENMASK(3, 0)
#define DP_TEST_AUDIO_CH_COUNT_SHIFT		4
#define DP_TEST_AUDIO_CH_COUNT_MASK		GENMASK(7, 4)

#define DP_TEST_AUDIO_SAMPLING_RATE_32		0x0
#define DP_TEST_AUDIO_SAMPLING_RATE_44_1	0x1
#define DP_TEST_AUDIO_SAMPLING_RATE_48		0x2
#define DP_TEST_AUDIO_SAMPLING_RATE_88_2	0x3
#define DP_TEST_AUDIO_SAMPLING_RATE_96		0x4
#define DP_TEST_AUDIO_SAMPLING_RATE_176_4	0x5
#define DP_TEST_AUDIO_SAMPLING_RATE_192		0x6

#define DP_TEST_AUDIO_CHANNEL1			0x0
#define DP_TEST_AUDIO_CHANNEL2			0x1
#define DP_TEST_AUDIO_CHANNEL3			0x2
#define DP_TEST_AUDIO_CHANNEL4			0x3
#define DP_TEST_AUDIO_CHANNEL5			0x4
#define DP_TEST_AUDIO_CHANNEL6			0x5
#define DP_TEST_AUDIO_CHANNEL7			0x6
#define DP_TEST_AUDIO_CHANNEL8			0x7

#define DP_EDP_14a                         0x04    /* eDP 1.4a */
#define DP_EDP_14b                         0x05    /* eDP 1.4b */

#define DP_FEC_CONFIGURATION			0x120
#define DP_FEC_READY				(1 << 0)

#define DP_EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT BIT(7)


/**
 * struct drm_dp_aux_msg - DisplayPort AUX channel transaction
 * @address: address of the (first) register to access
 * @request: contains the type of transaction (see DP_AUX_* macros)
 * @reply: upon completion, contains the reply type of the transaction
 * @buffer: pointer to a transmission or reception buffer
 * @size: size of @buffer
 */
struct drm_dp_aux_msg 
{
	unsigned int		address;
	u_int8_t			request;
	u_int8_t			reply;
	void				*buffer;
	size_t				size;
};


/*
 *		Fixed additions - drm_fixed.h
 */
#define DRM_FIXED_POINT				32
#define DRM_FIXED_ONE				(1ULL << DRM_FIXED_POINT)
#define DRM_FIXED_DECIMAL_MASK		(DRM_FIXED_ONE - 1)
#define DRM_FIXED_DIGITS_MASK		(~DRM_FIXED_DECIMAL_MASK)
#define DRM_FIXED_EPSILON			1LL
#define DRM_FIXED_ALMOST_ONE		(DRM_FIXED_ONE - DRM_FIXED_EPSILON)

#define dfixed_const(A) (u32)(((A) << 12))/*  + ((B + 0.000122)*4096)) */
#define dfixed_const_half(A) (u32)(((A) << 12) + 2048)
#define dfixed_const_666(A) (u32)(((A) << 12) + 2731)
#define dfixed_const_8(A) (u32)(((A) << 12) + 3277)
#define dfixed_mul(A, B) ((u64)((u64)(A).full * (B).full + 2048) >> 12)
#define dfixed_init(A) { .full = dfixed_const((A)) }
#define dfixed_init_half(A) { .full = dfixed_const_half((A)) }
#define dfixed_trunc(A) ((A).full >> 12)
#define dfixed_frac(A) ((A).full & ((1 << 12) - 1))

typedef union dfixed {
	u32 full;
} fixed20_12;

static inline u32 dfixed_ceil(fixed20_12 A)
{
	u32 non_frac = dfixed_trunc(A);

	if (A.full > dfixed_const(non_frac))
		return dfixed_const(non_frac + 1);
	else
		return dfixed_const(non_frac);
}

static inline u_int32_t dfixed_div(fixed20_12 A, fixed20_12 B)
{
	u_int64_t tmp = ((u_int64_t)A.full << 13);

	do_div(tmp, B.full);
	tmp += 1;
	tmp /= 2;
	return lower_32_bits(tmp);
}

#if 0
/**
 * div_u64_rem - unsigned 64bit divide with 32bit divisor with remainder
 *
 * This is commonly provided by 32bit archs to provide an optimized 64bit
 * divide.
 */
static inline u_int64_t div_u64_rem( u_int64_t dividend, u_int32_t divisor, u_int32_t *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

/**
 * div_s64_rem - signed 64bit divide with 32bit divisor with remainder
 */
static inline int64_t div_s64_rem(int64_t dividend, int32_t divisor, int32_t *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

/**
 * div64_u64_rem - unsigned 64bit divide with 64bit divisor and remainder
 */
static inline u_int64_t div64_u64_rem(u_int64_t dividend, u_int64_t divisor, u_int64_t *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

/**
 * div64_u64 - unsigned 64bit divide with 64bit divisor
 */
static inline u_int64_t div64_u64(u_int64_t dividend, u_int64_t divisor)
{
	return dividend / divisor;
}

/**
 * div64_s64 - signed 64bit divide with 64bit divisor
 */
static inline int64_t div64_s64(int64_t dividend, int64_t divisor)
{
	return dividend / divisor;
}

#endif

static inline int64_t drm_int2fixp(int a)
{
	return ((int64_t)a) << DRM_FIXED_POINT;
}

static inline int drm_fixp2int(int64_t a)
{
	return ((int64_t)a) >> DRM_FIXED_POINT;
}

static inline int drm_fixp2int_ceil(int64_t a)
{
	if (a > 0)
		return drm_fixp2int(a + DRM_FIXED_ALMOST_ONE);
	else
		return drm_fixp2int(a - DRM_FIXED_ALMOST_ONE);
}

static inline unsigned drm_fixp_msbset(int64_t a)
{
	unsigned shift, sign = (a >> 63) & 1;
	for (shift = 62; shift > 0; --shift)
		if (((a >> shift) & 1) != sign)
			return shift;
	return 0;
}

static inline int64_t drm_fixp_mul(int64_t a, int64_t b)
{
	unsigned shift = drm_fixp_msbset(a) + drm_fixp_msbset(b);
	int64_t result;
	if (shift > 61) {
		shift = shift - 61;
		a >>= (shift >> 1) + (shift & 1);
		b >>= shift >> 1;
	} else
		shift = 0;
	result = a * b;
	if (shift > DRM_FIXED_POINT)
		return result << (shift - DRM_FIXED_POINT);
	if (shift < DRM_FIXED_POINT)
		return result >> (DRM_FIXED_POINT - shift);
	return result;
}

static inline int64_t drm_fixp_div(int64_t a, int64_t b)
{
	unsigned shift = 62 - drm_fixp_msbset(a);
	int64_t result;
	a <<= shift;
	if (shift < DRM_FIXED_POINT)
		b >>= (DRM_FIXED_POINT - shift);
	result = div64_s64(a, b);
	if (shift > DRM_FIXED_POINT)
		return result >> (shift - DRM_FIXED_POINT);
	return result;
}

static inline int64_t drm_fixp_from_fraction(int64_t a, int64_t b)
{
	int64_t		res;
	bool	a_neg = a < 0;
	bool	b_neg = b < 0;
	u_int64_t		a_abs = a_neg ? -a : a;
	u_int64_t		b_abs = b_neg ? -b : b;
	u_int64_t		rem;
	
	/* determine integer part */
	u_int64_t res_abs  = div64_u64_rem( a_abs, b_abs, &rem );
	{	/* determine fractional part */
		u_int32_t i = DRM_FIXED_POINT;
		do {
			rem <<= 1;
			res_abs <<= 1;
			if( rem >= b_abs ) 
			{
				res_abs |= 1;
				rem -= b_abs;
			}
		} while( --i != 0 );
	}
	/* round up LSB */
	{
		u_int64_t summand = (rem << 1) >= b_abs;
		res_abs += summand;
	}
	
	res = (int64_t) res_abs;
	
	if( a_neg ^ b_neg )
	{
		res = -res;
	}
	
	return res;
}

static inline int64_t drm_fixp_exp(int64_t x)
{
	int64_t tolerance = div64_s64(DRM_FIXED_ONE, 1000000);
	int64_t sum = DRM_FIXED_ONE, term, y = x;
	u_int64_t count = 1;
	if (x < 0)
		y = -1 * x;
	term = y;
	while (term >= tolerance) {
		sum = sum + term;
		count = count + 1;
		term = drm_fixp_mul(term, div64_s64(y, count));
	}
	if (x < 0)
		sum = drm_fixp_div(DRM_FIXED_ONE, sum);
	return sum;
}




/*
 *		Mst additions - drm_dp_mst_helper.h
 */
 
/* this covers ENUM_RESOURCES, POWER_DOWN_PHY, POWER_UP_PHY */
struct drm_dp_connection_status_notify {
	u_int8_t	 guid[16];
	u_int8_t	 port_number;
	bool legacy_device_plug_status;
	bool displayport_device_plug_status;
	bool message_capability_status;
	bool input_port;
	u_int8_t	 peer_device_type;
};

struct drm_dp_port_number_req {
	u_int8_t port_number;
};

struct drm_dp_resource_status_notify {
	u_int8_t port_number;
	u_int8_t guid[16];
	u_int16_t available_pbn;
};

struct drm_dp_query_payload {
	u_int8_t port_number;
	u_int8_t vcpi;
};

#define DRM_DP_MAX_SDP_STREAMS 16
struct drm_dp_allocate_payload {
	u_int8_t port_number;
	u_int8_t number_sdp_streams;
	u_int8_t vcpi;
	u_int16_t pbn;
	u_int8_t sdp_stream_sink[DRM_DP_MAX_SDP_STREAMS];
};

struct drm_dp_remote_dpcd_read {
	u_int8_t port_number;
	u_int32_t dpcd_address;
	u_int8_t num_bytes;
};

struct drm_dp_remote_dpcd_write {
	u_int8_t port_number;
	u_int32_t dpcd_address;
	u_int8_t num_bytes;
	u_int8_t *bytes;
};

#define DP_REMOTE_I2C_READ_MAX_TRANSACTIONS 4
struct drm_dp_remote_i2c_read {
	u_int8_t num_transactions;
	u_int8_t port_number;
	struct {
		u_int8_t i2c_dev_id;
		u_int8_t num_bytes;
		u_int8_t *bytes;
		u_int8_t no_stop_bit;
		u_int8_t i2c_transaction_delay;
	} transactions[DP_REMOTE_I2C_READ_MAX_TRANSACTIONS];
	u_int8_t read_i2c_device_id;
	u_int8_t num_bytes_read;
};

struct drm_dp_remote_i2c_write {
	u_int8_t port_number;
	u_int8_t write_i2c_device_id;
	u_int8_t num_bytes;
	u_int8_t *bytes;
};

struct drm_dp_sideband_msg_req_body 
{
	u_int8_t req_type;
	union ack_req 
	{
		struct drm_dp_connection_status_notify		conn_stat;
		struct drm_dp_port_number_req				port_num;
		struct drm_dp_resource_status_notify		resource_stat;
		struct drm_dp_query_payload					query_payload;
		struct drm_dp_allocate_payload				allocate_payload;
		struct drm_dp_remote_dpcd_read				dpcd_read;
		struct drm_dp_remote_dpcd_write				dpcd_write;
		struct drm_dp_remote_i2c_read				i2c_read;
		struct drm_dp_remote_i2c_write				i2c_write;
	} u;
};


/* sideband msg header - not bit struct */
struct drm_dp_sideband_msg_hdr {
	u_int8_t lct;
	u_int8_t lcr;
	u_int8_t rad[8];
	bool broadcast;
	bool path_msg;
	u_int8_t msg_len;
	bool somt;
	bool eomt;
	bool seqno;
};

struct drm_dp_sideband_msg_rx {
	u_int8_t chunk[48];
	u_int8_t msg[256];
	u_int8_t curchunk_len;
	u_int8_t curchunk_idx; /* chunk we are parsing now */
	u_int8_t curchunk_hdrlen;
	u_int8_t curlen; /* total length of the msg */
	bool have_somt;
	bool have_eomt;
	struct drm_dp_sideband_msg_hdr initial_hdr;
};


struct drm_dp_nak_reply {
	u_int8_t guid[16];
	u_int8_t reason;
	u_int8_t nak_data;
};

struct drm_dp_link_address_ack_reply {
	u_int8_t guid[16];
	u_int8_t nports;
	struct drm_dp_link_addr_reply_port {
		bool input_port;
		u_int8_t peer_device_type;
		u_int8_t port_number;
		bool mcs;
		bool ddps;
		bool legacy_device_plug_status;
		u_int8_t dpcd_revision;
		u_int8_t peer_guid[16];
		u_int8_t num_sdp_streams;
		u_int8_t num_sdp_stream_sinks;
	} ports[16];
};

struct drm_dp_port_number_rep {
	u_int8_t port_number;
};

struct drm_dp_enum_path_resources_ack_reply {
	u_int8_t port_number;
	u_int16_t full_payload_bw_number;
	u_int16_t avail_payload_bw_number;
};

struct drm_dp_allocate_payload_ack_reply {
	u_int8_t port_number;
	u_int8_t vcpi;
	u_int16_t allocated_pbn;
};

struct drm_dp_query_payload_ack_reply {
	u_int8_t port_number;
	u_int8_t allocated_pbn;
};

struct drm_dp_remote_dpcd_read_ack_reply {
	u_int8_t port_number;
	u_int8_t num_bytes;
	u_int8_t bytes[255];
};

struct drm_dp_remote_dpcd_write_ack_reply {
	u_int8_t port_number;
};

struct drm_dp_remote_dpcd_write_nak_reply {
	u_int8_t port_number;
	u_int8_t reason;
	u_int8_t bytes_written_before_failure;
};

struct drm_dp_remote_i2c_read_ack_reply {
	u_int8_t port_number;
	u_int8_t num_bytes;
	u_int8_t bytes[255];
};

struct drm_dp_remote_i2c_read_nak_reply {
	u_int8_t port_number;
	u_int8_t nak_reason;
	u_int8_t i2c_nak_transaction;
};

struct drm_dp_remote_i2c_write_ack_reply {
	u_int8_t port_number;
};

struct drm_dp_sideband_msg_reply_body {
	u_int8_t reply_type;
	u_int8_t req_type;
	union ack_replies {
		struct drm_dp_nak_reply nak;
		struct drm_dp_link_address_ack_reply link_addr;
		struct drm_dp_port_number_rep port_number;
		struct drm_dp_enum_path_resources_ack_reply path_resources;
		struct drm_dp_allocate_payload_ack_reply allocate_payload;
		struct drm_dp_query_payload_ack_reply query_payload;
		struct drm_dp_remote_dpcd_read_ack_reply remote_dpcd_read_ack;
		struct drm_dp_remote_dpcd_write_ack_reply remote_dpcd_write_ack;
		struct drm_dp_remote_dpcd_write_nak_reply remote_dpcd_write_nack;
		struct drm_dp_remote_i2c_read_ack_reply remote_i2c_read_ack;
		struct drm_dp_remote_i2c_read_nak_reply remote_i2c_read_nack;
		struct drm_dp_remote_i2c_write_ack_reply remote_i2c_write_ack;
	} u;
};


u8   Drm_Addition_Get_Lane_Status( const u8 link_status[DP_LINK_STATUS_SIZE],	int iLane_Index );
bool Drm_Addition_Get_Clock_Recovery_Status( const u_int8_t link_status[DP_LINK_STATUS_SIZE],			      int iNumOfLanes );
bool Drm_Addition_Get_Channel_EQ_Status( const u_int8_t link_status[DP_LINK_STATUS_SIZE],			  int iNumOfLanes );
int  Drm_Addition_Calculate_PBN_mode( int clock, int bpp );
int32_t Drm_Addition_Parse_Sideband_Link_Address( struct drm_dp_sideband_msg_rx *raw, 					           struct drm_dp_sideband_msg_reply_body *repmsg );
void Drm_Addition_Encode_Sideband_Msg_Hdr( struct drm_dp_sideband_msg_hdr *hdr,    				   u_int8_t *buf, int *len );
int32_t Drm_Addition_Decode_Sideband_Msg_Hdr( struct drm_dp_sideband_msg_hdr *hdr, 					   u_int8_t *buf, int buflen, u_int8_t *hdrlen );
void Drm_Addition_Encode_SideBand_Msg_CRC( u_int8_t *msg, u_int8_t len );
void Drm_Addition_Parse_Sideband_Connection_Status_Notify( struct drm_dp_sideband_msg_rx *raw,							         struct drm_dp_sideband_msg_req_body *msg );

bool Drm_dp_tps3_supported( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] );
bool Drm_dp_tps4_supported( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] );
u8   Drm_dp_max_lane_count( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] );
bool Drm_dp_enhanced_frame_cap( const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE] );


#endif
