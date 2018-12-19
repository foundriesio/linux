/*
 * video_params.h
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
 /**
 * @file
 * Define video parameters structure and functions to manipulate the
 * information held by the structure.
 * Video encoding and colorimetry are also defined here
 */

#ifndef __HDMI_VIDEOPARAMS_H_
#define __HDMI_VIDEOPARAMS_H_

int hdmi_dtd_fill(dtd_t * dtd, u32 code, u32 refreshRate);

unsigned int hdmi_dtd_get_refresh_rate(dtd_t *dtd);


int videoParams_GetCeaVicCode(int hdmi_vic_code);

int videoParams_GetHdmiVicCode(int cea_code);

/**
 * This method should be called before setting any parameters
 * to put the state of the strucutre to default
 * @param params pointer to the video parameters structure
 */
void video_params_reset(struct hdmi_tx_dev *dev,videoParams_t * params);

/**
 * @param params pointer to the video parameters structure
 * @return the custom csc coefficients A
 */
u16 *videoParams_GetCscA(struct hdmi_tx_dev *dev, videoParams_t * params);

void videoParams_SetCscA(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value[4]);

/**
 * @param params pointer to the video parameters structure
 * @return the custom csc coefficients B
 */
u16 *videoParams_GetCscB(struct hdmi_tx_dev *dev, videoParams_t * params);

void videoParams_SetCscB(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value[4]);

/**
 * @param params pointer to the video parameters structure
 * @return the custom csc coefficients C
 */
u16 *videoParams_GetCscC(struct hdmi_tx_dev *dev, videoParams_t * params);

void videoParams_SetCscC(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value[4]);

void videoParams_SetCscScale(struct hdmi_tx_dev *dev, videoParams_t * params, u16 value);

/**
 * @param params pointer to the video parameters structure
 * @return Video PixelClock in [0.01 MHz]
 */
u32 videoParams_GetPixelClock(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * @param params pointer to the video parameters structure
 * @return TMDS Clock in [0.01 MHz]
 */
u16 videoParams_GetTmdsClock(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * @param params pointer to the video parameters structure
 * @return Ration clock x 100 (struct hdmi_tx_dev *dev, should be multiplied by x 0.01 afterwards)
 */
unsigned videoParams_GetRatioClock(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * @param params pointer to the video parameters structure
 * @return TRUE if csc is needed
 */
int videoParams_IsColorSpaceConversion(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * @param params pointer to the video parameters structure
 * @return TRUE if color space decimation is needed
 */
int videoParams_IsColorSpaceDecimation(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * @param params pointer to the video parameters structure
 * @return TRUE if if video is interpolated
 */
int videoParams_IsColorSpaceInterpolation(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * @param params pointer to the video parameters structure
 * @return TRUE if if video has pixel repetition
 */
int videoParams_IsPixelRepetition(struct hdmi_tx_dev *dev, videoParams_t * params);

void videoParams_UpdateCscCoefficients(struct hdmi_tx_dev *dev, videoParams_t * params);

u8 videoParams_IsLimitedToYcc420(struct hdmi_tx_dev *dev, videoParams_t * params);

char * getEncodingString(encoding_t encoding);

#endif				/* VIDEOPARAMS_H_ */
