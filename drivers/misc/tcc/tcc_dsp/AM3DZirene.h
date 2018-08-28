/**  @file
* Copyright (c) 2009-2015 AM3D A/S.
*
* AM3DZirene.h
*
* \brief Zirene Interface.
*
* COPYRIGHT:
* Copyright (c) AM3D A/S. All Rights Reserved.
* AM3D products contain certain trade secrets and confidential information and proprietary information of AM3D. Use, reproduction, 
* disclosure and distribution by any means are prohibited, except pursuant to a written license from AM3D.
* The user of this AM3D software acknowledges and agrees that this software is the sole property of AM3D, that it only must be 
* used to what it is indented to and that it in all aspects shall be handled as Confidential Information. The user also acknowledges
* and agrees that he/she has the proper rights to handle this Confidential Information.
*/

#ifndef _AM3DZIRENE_H_
#define _AM3DZIRENE_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include "AM3DOSAL.h"

#define	AM3D_FALSE	0 /**< Defines false condition (0) */
#define AM3D_TRUE	1 /**< Defines true condtion (1) */


/** ZireneHandle is acquired using the Zirene_Init() function and is used in subsequent Zirene function calls. */
#ifdef __FILE__
#define ZIRENE_DECLARE_HANDLE(name, handlename) struct name; typedef struct name* handlename
ZIRENE_DECLARE_HANDLE(Zirene, ZireneHandle);
#else
typedef void* ZireneHandle; // For doxygen only so that it looks right in the API documentation.
#endif

/** NULL value definition used for null pointers */
#ifndef NULL
#define NULL    0
#endif

/**
* The eZireneStatus enum is used as Zirene return type.  
*/
typedef enum 
{
    ZIRENE_SUCCESS                          = 0,    /**< Successful function execution */
    ZIRENE_FAILURE_GENERIC                  = -128, /**< Generic error in the function execution */
    ZIRENE_FAILURE_PARAMETER_OUT_OF_RANGE   = -129, /**< Applied parameter is outside the allowed range */
    ZIRENE_UNKNOWN_PARAMETER                = -131, /**< The value passed for the parameter argument in Zirene_SetParameter() function is not recognized */
    ZIRENE_OUT_OF_MEMORY                    = -133, /**< Returned by Zirene_Init() if memory allocation fails */
    ZIRENE_UNSUPPORTED                      = -134, /**< Returned by Zirene_SetParameter() if Active State is used as parameter */
    ZIRENE_WARNING_UNSUPPORTED_SAMPLE_RATE  = -256  /**< warning in the function execution: unsupported sample rate selected*/
} eZireneStatus;

/**
* The eZireneSampleRate enum is used in Zirene_Init() and Zirene_SetSampleRate() to set the current sample rate. 
*/
typedef enum
{
    ZIRENE_FS_8000 = 8000,		/**<   8000 Hz sample rate */
    ZIRENE_FS_11025 = 11025,	/**<  11025 Hz sample rate */
    ZIRENE_FS_12000 = 12000,	/**<  12000 Hz sample rate */
    ZIRENE_FS_16000 = 16000,	/**<  16000 Hz sample rate */
    ZIRENE_FS_22050 = 22050,	/**<  22050 Hz sample rate */
    ZIRENE_FS_24000 = 24000,	/**<  24000 Hz sample rate */
    ZIRENE_FS_32000 = 32000,	/**<  32000 Hz sample rate */
    ZIRENE_FS_44100 = 44100,	/**<  44100 Hz sample rate */
    ZIRENE_FS_48000 = 48000 	/**<  48000 Hz sample rate */
} eZireneSampleRate;

/**
* The eZireneInputConfig enum is used in Zirene_SetIOConfig() to set the input channel configuration. 
* Each element in the enum provides two types of information: The number of channels and the ordering of the channels. 
*/
typedef enum 
{
    ZIRENE_IN_MONO,            /**< One mono channel */
    ZIRENE_IN_STEREO,          /**< Two channels ordered this way: 0 (left), 1 (right) */
    ZIRENE_IN_STANDARD_5P1,    /**< Six channels ordered this way: 0 (front left),  1 (front right),  2 (front center), 3 (LFE), 4 (back left) 5 (back right) */
    ZIRENE_IN_STANDARD_7P1,    /**< Eight channels ordered this way: 0 (front left),  1 (front right),  2 (front center), 3 (LFE), 4 (back left) 5 (back right) , 6 (side left), 7 (side right) */
    ZIRENE_IN_STANDARD_5P1_CO1 /**< Six channels ordered this way: 0 (front left),  1 (front center),  2 (front right), 3 (back left), 4 (back right) 5 (LFE) */
} eZireneInputConfig;

/**
* The eZireneOutputConfig enum is used in Zirene_SetIOConfig() to set the playback system. 
* Each element in the enum provides three types of informations: The number of channels, the ordering of the channels, and the type of playback system (headphone or loudspeakers). 
*/
typedef enum 
{
    ZIRENE_OUT_STEREO_LS,       /**< Playback system is stereo loudspeakers, there are two channels out ordered this way: Channel order: 0 (left), 1 (right) */
    ZIRENE_OUT_STANDARD_4P0_LS  /**< Playback system is four loudspeakers placed around the listener, the channels are ordered this way: 0 (left front), 1 (right front), 2 (left back), 3 (right back) */
} eZireneOutputConfig;

/**
* The eZirene_Effect enum is used in calls to Zirene_SetParameter() and Zirene_GetParameter() and is used in the pZireneChangedCallback callback.
*/
typedef enum
{
    ZIRENE_POWERBASS = 0,           /**< Specifices that a parameter is for PowerBass.                */
    ZIRENE_LEVELALIGNMENT = 1,      /**< Specifices that a parameter is for Level Alignment.          */
    ZIRENE_TREBLEENHANCEMENT = 2,   /**< Specifices that a parameter is for Treble Enhancement.       */
    ZIRENE_TRANSDUCEREQ = 3,        /**< Specifices that a parameter is for Transducer EQ.            */
    ZIRENE_SURROUND = 4,            /**< Specifices that a parameter is for Surround.                 */
    ZIRENE_GRAPHICEQ = 5,           /**< Specifices that a parameter is for Graphic EQ.               */
    ZIRENE_TONECONTROL = 6,         /**< Specifices that a parameter is for Tone Control.             */
    ZIRENE_HIGHPASSFILTER = 7,      /**< Specifices that a parameter is for High Pass Filter.         */
    ZIRENE_SWEETSPOT = 8,           /**< Specifices that a parameter is for Sweet Spot.               */
    ZIRENE_PARAMETRICEQ = 9         /**< Specifices that a parameter is for Parametric EQ.            */
} eZirene_Effect;

/** 
* Returns a nonzero handle on success. The handle must be used in subsequent calls to Zirene.
* The function performs memory allocation for all internal algorithm data structures and buffers. All effect modules are initialized to a default parameter set. 
* See Zirene_SetSampleRate() for supported sample rates. All unsupported sample rates force the processing into a global bypass state as described in  
* Zirene_SetSampleRate(), where sound is passed through but with no effects being applied. Use Zirene_IsGloballyBypassed() to get global processing bypass state.
* @param [in] iSampleRateInHz is the input and output sample rate Zirene is initialized with. 
* It is possible to change this later on using Zirene_SetSampleRate() but input and output sample rate is always the same.
* All sound enhancement effects are disabled by default. The default input configuration is ZIRENE_IN_STEREO and the output 
* configuration is ZIRENE_OUT_STEREO_LS. The default number of bits per sample is 16.
* @return handle to new Zirene instance on success, ::NULL otherwise.
* @note When the Zirene instance is no longer needed, you should call Zirene_DeInit() to destroy the Zirene instance, and free any allocated memory.
*/
eZireneStatus Zirene_Init(ZireneHandle* zireneHandle, eZireneSampleRate iSampleRateInHz);

/** 
* De-initialize Zirene. This destroys the Zirene instance identified by the handle. This releases resources allocated in Zirene_Init(), e.g. all allocated memory for Zirene's internal algorithm variables, buffers etc.
* @param [in] handle is the Zirene handle that identifies the Zirene instance that should be de-initialized.
* @note After calling this function, the handle becomes invalid, and must subsequently NOT be used to call any Zirene functions. Be sure to stop any processing threads, etc. that use this Zirene instance, before calling this function.
*/
void Zirene_DeInit(ZireneHandle handle);

/**
* Processing function. Processes a block of interleaved audio data and applies the enabled sound enhancement effects.
* Number of input and output channels must comply with what has been specified using Zirene_SetIOConfig().
* Any number of processing frame sizes can be used from 1 and upwards, however for best resource performance it is advised to use 64 and upwards. 
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] pInput should be a pointer to interleaved input data.
* @param[out] pOutput should be a pointer a buffer into which the interleaved output data will be written.
* @param[in] iNumberOfFrames the number of frames to process. A frame corresponds to one sample from all input channels.  
* Inplace processing is supported.
* @note For 24 bits processing, it is expected that the samples are stored in the lower 24 bits of an AM3D_INT32 type and
* that the 8 most significant bits are zero see ::Zirene_SetNumberOfBitsPerSample().
* @note When processing interleaved audio data in-place and the number of output channels is larger than the number of
* input channels the output from Zirene will be distorted/corrupt if a blocksize larger than 512 is used.
*/
void Zirene_ProcessInterleaved(ZireneHandle handle, void* pInput, void* pOutput, AM3D_INT32 iNumberOfFrames);

/**
* Processing function. Processes a block of non-interleaved audio data and applies the enabled sound enhancement effects. 
* Number of input and output channels must comply with what has been specified using Zirene_SetIOConfig().
* Any number of processing frame sizes can be used from 1 and upwards, however for best resource performance it is advised to use 64 and upwards. 
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] ppInput should be a pointer to an array of input channel pointers.
* @param[out] ppOutput should be a pointer to an array of output channel pointers.
* @param[in] iNumberOfFrames is the number of frames to process. A frame corresponds to one sample from all input channels.  
* Inplace processing is supported (when number of input channels equals number of output channels).
* @note For 24 bits processing, it is expected that the samples are stored in the lower 24 bits of an AM3D_INT32 type and
* that the 8 most significant bits are zero.
*/
void Zirene_ProcessNonInterleaved(ZireneHandle handle, void** ppInput, void** ppOutput, AM3D_INT32 iNumberOfFrames);

/** 
* Sets the channel configuration for the audio input and audio output passed to Zirene_ProcessInterleaved() / 
* Zirene_ProcessNonInterleaved(). 
* Furthermore Zirene internally relies on this information to be correct in order to automatically adjust some effect settings.
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] eInputConfig contains the information about the input data.
* @param[in] eOutputConfig sets the playback system.
* @return ::eZireneStatus value. 
*/
eZireneStatus Zirene_SetIOConfig(ZireneHandle handle, eZireneInputConfig eInputConfig, eZireneOutputConfig eOutputConfig);

/** 
* Sets the sample rate. The output sample rate is the same as the sample rate of the input. The supported sample rates are 
* 8, 11.025, 12, 16, 22.05, 24, 32, 44.1 and 48 kHz. Sample rates of 96 kHz and 192 kHz are supported as a build option, however these are 
* internally realized by downmixing to 48 kHz and upsampling back again on the output. Unsupported sample rates force the processing 
* into a global bypass state, where sound is passed through Zirene, but without effects being applied. Use Zirene_IsGloballyBypassed()
* to get global bypass state.
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] eSampleRateInHz should be of type ::eZireneSampleRate. Specifies the sample rate in Hz for the input and output data.
* @return ::eZireneStatus value. ZIRENE_WARNING_UNSUPPORTED_SAMPLE_RATE is returned if sample rate is not supported and the processing is forced to bypass state.
* @note Calling this function also clears internal delay-lines (hence no need to call Zirene_Discontinuity).
*/
eZireneStatus Zirene_SetSampleRate(ZireneHandle handle, eZireneSampleRate eSampleRateInHz);

/** 
* Sets the number of bits per sample for both input and output.  
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in]  iNumberOfBitsPerSample sets the number of bits per sample for input and output.
* * The following values are supported: 16 (default) and 24
* @note Expected data format
* iNumberOfBitsPerSample                  Format (s = sign bit, d = data bit)
*         16                  signed 16 bit integer data type Q15 [s.ddddddddddddddd]
*         24                  signed 32 bit integer data type Q23 [sssssssss.ddddddddddddddddddddddd]
* @return ::eZireneStatus value.  
*/
eZireneStatus Zirene_SetNumberOfBitsPerSample(ZireneHandle handle, AM3D_INT32 iNumberOfBitsPerSample); 

/** 
* Sets a global bypass state valid for all effect modules. When Zirene is globally bypassed all effect modules are skipped regardless of
* their enable state and audio is passed directly to output. For multi-channel input down-mixing will occur. Setting of the global bypass state overrules 
* all effect modules active state in the following sense: 
* if global bypass state is set to AM3D_TRUE all effect modules will be inactive.
* if global bypass state is set to AM3D_FALSE all effect modules will be active if possible.
* There is only one exception to this rule. The bypass state can be overruled and set to AM3D_TRUE if an unsupported sample-rate is set in a 
* call to Zirene_SetSampleRate(). Setting of bypass state does not change enable state on effect modules. 
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] iGlobalBypassState sets global bypass state to bypassed (::AM3D_TRUE) or not bypassed (::AM3D_FALSE).  
*/
void Zirene_GlobalBypass(ZireneHandle handle, AM3D_INT32 iGlobalBypassState);

/** 
* Returns the state of the sample-rate dependent global bypass feature.
* @param[in] handle is a handle to an initialized Zirene instance.  
* @return ::AM3D_FALSE if NOT in bypass state, ::AM3D_TRUE if in bypass state  
*/
AM3D_INT32 Zirene_IsGloballyBypassed(ZireneHandle handle);

/*** Zirene_SetGlobalBypassGain supported range and default value. Default value is set if GlobalBypass enabled without
* specifically setting the GlobalBypassGain */
#define ZIRENE_GLOBAL_BYPASS_GAIN_DB_MIN         -15        /**< Global bypass gain, minimum allowed value  [dB] */
#define ZIRENE_GLOBAL_BYPASS_GAIN_DB_MAX           0        /**< Global bypass gain, maximum allowed value  [dB] */
#define ZIRENE_GLOBAL_BYPASS_GAIN_DB_DEFAULT       0        /**< Global bypass gain, default value  [dB] */
/** 
* Sets the gain applied when ::Zirene_GlobalBypass is set to ::AM3D_TRUE.
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] iGlobalBypassGainInDb The gain value applied in dB. Allowed range is ::ZIRENE_GLOBAL_BYPASS_GAIN_DB_MIN to ::ZIRENE_GLOBAL_BYPASS_GAIN_DB_MAX.
* @return ::eZireneStatus value. 
* @note ::ZIRENE_GLOBAL_BYPASS_GAIN_DB_DEFAULT is applied if this function is not called. Normally this is set to 0 dB.
*/
eZireneStatus Zirene_SetGlobalBypassGain(ZireneHandle handle, AM3D_INT32 iGlobalBypassGainInDb); 

/** 
* Informs Zirene that there is a discontinuity in input data. A discontinuity in input data typically produces a click
* when resuming Zirene processing if nothing is done to prevent such a click. 
* When calling this function Zirene will reset internal buffers and variables.
* None of the parameters exposed in the Zirene interface are changed. 
* @param[in] handle is a handle to an initialized Zirene instance.  
*/
void Zirene_Discontinuity(ZireneHandle handle);

/** 
* Returns the Zirene library version. The header file version (see top of file) should match with the first part of the returned version string. 
* @return The Zirene library version as a string.
*/
const AM3D_CHAR* Zirene_GetVersionString(void);


/**
* List of output channels known by Zirene.  
*/
#define ZIRENE_SPEAKER_FRONT_LEFT	0x1  /**< Front left loudspeaker. */
#define ZIRENE_SPEAKER_FRONT_RIGHT  0x2  /**< Front right loudspeaker. */
#define ZIRENE_SPEAKER_BACK_LEFT    0x10 /**< Back left loudspeaker. */
#define ZIRENE_SPEAKER_BACK_RIGHT   0x20 /**< Back right loudspeaker. */
#define ZIRENE_SPEAKER_ALL_CHANNELS (ZIRENE_SPEAKER_FRONT_LEFT | ZIRENE_SPEAKER_FRONT_RIGHT |\
                                     ZIRENE_SPEAKER_BACK_LEFT | ZIRENE_SPEAKER_BACK_RIGHT)     /**< All loudspeakers. */

/** Please refer to the basic functional specification document for detailed informations about the effect(s) and how to tune parameters.
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] eEffect is the effect on which a parameter is going to be changed, see ::eZirene_Effect for possible choices of parameter.
* @param[in] eParm specifies which parameter to set, see eZirene_[effect]_Parameters.
* @param[in] iValue specifies the value of the parameter to set.
* @return ::ZIRENE_SUCCESS on success, ::ZIRENE_EFFECT_NOT_VALID_FOR_INSTANCE if Zirene instance has not been initialized/configured using the effect specified
* in eEffect argument. 
*/
eZireneStatus Zirene_SetParameter(ZireneHandle handle, eZirene_Effect eEffect, AM3D_INT32 eParm, AM3D_INT32 eChannelMask, AM3D_INT32 iValue);

/** Please refer to the basic functional specification document for detailed informations about the effect(s) and how to tune parameters.
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] eEffect is the effect on which a parameter is going to be read, see ::eZirene_Effect for possible choices of parameter.
* @param[in] eParm specifies which parameter to get, see eZirene_[effect]_Parameters.
* @param[in] iValue contains the value of read parameter.
* @return ::ZIRENE_SUCCESS on success, ::ZIRENE_EFFECT_NOT_VALID_FOR_INSTANCE if Zirene instance has not been initialized/configured using the effect specified
* in eEffect argument. 
*/
eZireneStatus Zirene_GetParameter(ZireneHandle handle, eZirene_Effect eEffect, AM3D_INT32 eParm, AM3D_INT32 eChannelMask, AM3D_INT32* iValue);


/*---------------------------------------------------------------------------------------------------------------------
	 Surround Sound
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_SURROUND_REAR_DELAY_IN_MS_MIN            0 /**< Minimum supported rear delay in milliseconds */
#define ZIRENE_SURROUND_REAR_DELAY_IN_MS_MAX           30 /**< Maximum supported rear delay in milliseconds */
#define ZIRENE_SURROUND_REAR_DELAY_IN_MS_DEFAULT       15 /**< Default rear delay in milliseconds           */


/**
* The enum contains the list of audio channel combinations supported by the Surround Sound effect.  
*/
typedef enum {
    ZIRENE_SS_ALL_CHANNELS = ZIRENE_SPEAKER_ALL_CHANNELS
} eZireneSurroundSoundSupportedChannelCombinations;

/**
* The enum contains the list of parameters supported by Surround.  
*/
typedef enum 
{
    /** Enables or disables the Surround effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). 
     */
    ZIRENE_SURROUND_ENABLE_IN_ONOFF,
    /** Used to request the "Active State" for Surround. <br>
      *  It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() 
      * @note This parameter states if the Enable State for Surround has been overruled internally. <br>
      *       If the Active State equals ::AM3D_FALSE the Enable State has been overruled. Otherwise it has not. <br>
      *       The Enable State is overruled if number of output channels is not 4. */
    ZIRENE_SURROUND_ACTIVESTATE_IN_ONOFF,
    /** Set Delay in ms for rear channels. */
    ZIRENE_SURROUND_REAR_DELAY_IN_MS
} eZirene_SurroundSound_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 Transducer EQ
  ---------------------------------------------------------------------------------------------------------------------*/
/**
* The enum contains the list of audio channel combinations supported by the TrebleEnhancement effect in this Zirene release.  
*/
typedef enum {
    ZIRENE_TEQ_FRONT_LEFT_RIGHT = (ZIRENE_SPEAKER_FRONT_LEFT | ZIRENE_SPEAKER_FRONT_RIGHT), 
    ZIRENE_TEQ_BACK_LEFT_RIGHT  = (ZIRENE_SPEAKER_BACK_LEFT  | ZIRENE_SPEAKER_BACK_RIGHT)
} eZireneTransducerEqSupportedChannelCombinations;


/*---- Data structures ----*/

/** \brief An equalization filter structure for one specific sample rate. */
/** In the call to Zirene_TransducerEq_AddEqFilter(), Zirene saves a copy of the data pointed to by the pointers psFIRCoeffsAndShiftFactor
and psIIRCoeffsAndShiftFactors. The data pointed to by these pointers is not used by Zirene after the call to Zirene_TransducerEq_AddEqFilter(). */
typedef struct
{
    /** The sample rate in Hz which the filter was designed for. */
    AM3D_INT32 iSampleRateInHz; 
    /** Bitmask specifying which channels the filter applies to.
     * - 0x0000: all channels
     * - 0x0001: front left
     * - 0x0002: front right
     *
     * To specify that the filter applies to front left and front right only, one would use the bitmask: 0x0001 | 0x0002.
     */
    AM3D_INT32 iChannelMask;
    AM3D_INT32 iOutputGainQ10; /**< After filtering the output will be multiplied by this factor and divided by 2^10. */
    /**< @note Shall be set to 1024, since other output gains is not supported in current version. */
    AM3D_INT32 iNumberOfFIRCoeffs; /**< Number of FIR filter taps NOT including shift factor. */ 
    /** Pointer to an array of FIR filter coefficients and a closing shift factor (must be the last entry in the array). 
      * The shift factor specifies the fixed-point representation of the coefficients e.g. a shift factor of 15 means that
      * the coefficients are represented in Q15.
      */
    AM3D_INT16* psFIRCoeffsAndShiftFactor; 
    AM3D_INT32 iNumberOfIIRFilters; /**< Number of IIR filters sections. */
    /** Pointer to an array of IIR coefficients for all IIR sections.
      * An IIR section consists of three b coefficients, two a coefficients, and a shift factor. 
      * Example with two IIR sections: {b0, b1, b2, a1, a2, shiftFactor, b0, b1, b2, a1, a2, shiftFactor}. 
      */
    AM3D_INT16* psIIRCoeffsAndShiftFactors; 
} tZireneEqFilter;


/*---- Parameters ----*/

/**
* The enum contains the list of parameters supported by Transducer EQ.  
*/
typedef enum 
{
    /** Enables or disables the Transducer EQ effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). 
     * If enabled and current sample rate (as set in call to ::Zirene_SetSampleRate) 
     * does not match with iSampleRateInHz member of ::tZireneEqFilter struct for any filters in the current selected Transducer EQ section (as set in call to ::Zirene_TransducerEq_SelectEqSectionByIndex)
     * the closest matching filter will be used. The closest matching filter is the filter which iSampleRateInHz member is nearest to the current sample rate.
     */
    ZIRENE_TRANSDUCEREQ_ENABLE_IN_ONOFF,
    /** Used to request the "Active State" for Transducer EQ. <br>
      *  It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() 
      * @note This parameter states if the Enable State for Transducer EQ has been overruled internally.<br>
      *       If the Active State equals ::AM3D_FALSE the Enable State has been overruled. Otherwise it has not. <br>
      *       The Enable State is overruled if no valid filters are available. */
    ZIRENE_TRANSDUCEREQ_ACTIVESTATE_IN_ONOFF
} eZirene_TransducerEq_Parameters;

/*---- Functions ----*/

/** 
* Defines a Transducer EQ section, for instance a section could be defined as "Internal loudspeaker". The number of sections should match the number of supported transducers. 
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] iChannelMask is a bitmask describing on which output channels the effect setting should be applied.
* @param[out] piSectionIndex is set to a unique section index. The index corresponds to the order in which the sections have been created.
* @return ::ZIRENE_SUCCESS on success, ::ZIRENE_FAILURE_GENERIC if unable to add section. 
*/
eZireneStatus Zirene_TransducerEq_AddEqSection(ZireneHandle handle, eZireneTransducerEqSupportedChannelCombinations iChannelMask, AM3D_INT32* piSectionIndex);

/** 
* Adds a Trandsducer EQ filter to the section with iSectionIndex. The number of filters should match the number of different transducers and supported sample rates. 
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] iChannelMask is a bitmask describing on which output channels the effect setting should be applied.
* @param[in] iSectionIndex is the unique index of the section which was returned by Zirene_TransducerEq_AddEqSection().
* @param[in] pEqFilter is a pointer to the filter structure that should be added. The structure should be allocated and
* setup by the caller and should remain valid during the lifetime of the Zirene engine, e.g. the caller cannot
* free/re-use the structure before de-initializing Zirene. 
* @return ::ZIRENE_SUCCESS if successful ::ZIRENE_FAILURE_GENERIC if not. 
*/    
eZireneStatus Zirene_TransducerEq_AddEqFilter(ZireneHandle handle, eZireneTransducerEqSupportedChannelCombinations iChannelMask, AM3D_INT32 iSectionIndex, tZireneEqFilter* pEqFilter);

/** 
* Selects the wanted Transducer EQ section for processing. This is the function that should be used to set the Transducer EQ when switching between different transducers. 
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] iChannelMask is a bitmask describing on which output channels the effect setting should be applied.
* @param[in] iSectionIndex is the unique section index that should be selected.
* @return ::ZIRENE_SUCCESS if successful ::ZIRENE_FAILURE_GENERIC if not. 
*/
eZireneStatus Zirene_TransducerEq_SelectEqSectionByIndex(ZireneHandle handle, eZireneTransducerEqSupportedChannelCombinations iChannelMask, AM3D_INT32 iSectionIndex);

/** 
* Convenient function for adding multiple Transducer EQ filters, all belonging to the same section, to Zirene in one function call. The purpose of the function is
* to support easy handling of Transducer EQ filters created by AM3D Filter Design Tool (FDT). When multiple filters are created using FDT the filters can be added 
* to Zirene using this function.
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] iChannelMask is a bitmask describing on which output channels the effect setting should be applied.
* @param[in] EqFilterArray is a zero-terminated array of TZireneEqFilter references.
* @param[out] piSectionIndex is set to a unique section index. The index corresponds to the order in which the sections have been created.
* @return ::ZIRENE_SUCCESS if successful ::ZIRENE_FAILURE_GENERIC if not. 
*/
eZireneStatus Zirene_TransducerEq_CreateEqFilterSectionAndAddFilters(ZireneHandle handle, eZireneTransducerEqSupportedChannelCombinations iChannelMask, tZireneEqFilter** EqFilterArray, AM3D_INT32* piSectionIndex);

/** Returns the current number of Transducer EQ sections which corresponds to the number of different transducer systems (for instance MX300 headphones, MX301 headphones, JB40 loudspeakers).
* @param[in] handle is a handle to an initialized Zirene instance.  
* @param[in] iChannelMask is a bitmask describing on which output channels the effect setting should be applied.
* @param[out]  piNumberOfEqs returns the number of eqs that have been added.  
* @return ::ZIRENE_SUCCESS if successful ::ZIRENE_FAILURE_GENERIC if not. 
*/
eZireneStatus Zirene_TransducerEq_GetNumberOfEqSections(ZireneHandle handle, eZireneTransducerEqSupportedChannelCombinations iChannelMask, AM3D_INT32* piNumberOfEqs);

/** Clears the list of transducer eq sections in Zirene. 
* @param[in] handle is a handle to an initialized Zirene instance.
* @param[in] iChannelMask is a bitmask describing on which output channels the effect setting should be applied.
* @return ::ZIRENE_SUCCESS if successful ::ZIRENE_FAILURE_GENERIC if not. 
*/
eZireneStatus Zirene_TransducerEq_ClearEqList(ZireneHandle handle, eZireneTransducerEqSupportedChannelCombinations iChannelMask);


/*---------------------------------------------------------------------------------------------------------------------
	 SweetSpot
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_SWEETSPOT_DISTANCE_TO_LOUDSPEAKER_IN_CM_MIN       1    /**< SweetSpot, minimum allowed distance [cm] */
#define ZIRENE_SWEETSPOT_DISTANCE_TO_LOUDSPEAKER_IN_CM_MAX       500  /**< SweetSpot, maximum allowed distance [cm] */
#define ZIRENE_SWEETSPOT_DISTANCE_TO_LOUDSPEAKER_IN_CM_DEFAULT   ZIRENE_SWEETSPOT_DISTANCE_TO_LOUDSPEAKER_IN_CM_MIN /**< SweetSpot, default value when parameter is not specifically set [cm] */
#define ZIRENE_SWEETSPOT_GAIN_IN_DB_MIN                         -6  /**< SweetSpot, maximum allowed attenuation [dB] */
#define ZIRENE_SWEETSPOT_GAIN_IN_DB_MAX                          0  /**< SweetSpot, minimum allowed attenuation [dB] */
#define ZIRENE_SWEETSPOT_GAIN_IN_DB_DEFAULT                      ZIRENE_SWEETSPOT_GAIN_IN_DB_MAX /**< SweetSpot, default value when parameter is not specifically set [dB] */


/**
* The enum contains the list of audio channel combinations supported by the SweetSpot effect.  
* @note ::ZIRENE_SW_ALL_CHANNELS must be used for parameters ::ZIRENE_SWEETSPOT_ENABLE_IN_ONOFF and ::ZIRENE_SWEETSPOT_ACTIVESTATE_IN_ONOFF. <br> 
* Parameters ::ZIRENE_SWEETSPOT_DISTANCE_TO_LOUDSPEAKER_IN_CM and ::ZIRENE_SWEETSPOT_GAIN_IN_DB can only be used with ::ZIRENE_SW_FRONT_LEFT, ::ZIRENE_SW_FRONT_RIGHT
* ::ZIRENE_SW_BACK_LEFT or ::ZIRENE_SW_BACK_RIGHT.
*/

typedef enum {
    ZIRENE_SW_ALL_CHANNELS = ZIRENE_SPEAKER_ALL_CHANNELS,
    ZIRENE_SW_FRONT_LEFT   = ZIRENE_SPEAKER_FRONT_LEFT, 
    ZIRENE_SW_FRONT_RIGHT  = ZIRENE_SPEAKER_FRONT_RIGHT, 
    ZIRENE_SW_BACK_LEFT    = ZIRENE_SPEAKER_BACK_LEFT,
    ZIRENE_SW_BACK_RIGHT   = ZIRENE_SPEAKER_BACK_RIGHT
} eZireneSweetSpotSupportedChannelCombinations;

/**
* The enum contains the list of parameters supported by SweetSpot.  
*/
typedef enum 
{
    /** Enables or disables the SweetSpot effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). 
     */
    ZIRENE_SWEETSPOT_ENABLE_IN_ONOFF,
    /** Used to request the "Active State" for SweetSpot. <br>
      * It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() 
      * @note This parameter states if the Enable State for SweetSpot has been overruled internally. <br>
      *     If the Active State equals ::AM3D_FALSE the Enable State has been overruled. Otherwise it has not. <br> */
    ZIRENE_SWEETSPOT_ACTIVESTATE_IN_ONOFF,
    /** Sets the distance from the listeners position to each loudspeaker in centimeters. Shall be set for all loudspeakers. */
    ZIRENE_SWEETSPOT_DISTANCE_TO_LOUDSPEAKER_IN_CM,
    /** Controls channel gain for each channel
      * The gain is -6dB to 0dB in steps of 1dB.  Shall be set individually for all loudspeakers. */
    ZIRENE_SWEETSPOT_GAIN_IN_DB
} eZirene_SweetSpot_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 TrebleEnhancement
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_TREBLEENHANCEMENT_HEADROOM_GAIN_IN_DB_MIN                   -12  /**< Treble Enhancement, minimum allowed headroom gain [dB] */
#define ZIRENE_TREBLEENHANCEMENT_HEADROOM_GAIN_IN_DB_MAX	                 0  /**< Treble Enhancement, maximum allowed headroom gain [dB] */
#define ZIRENE_TREBLEENHANCEMENT_HEADROOM_GAIN_IN_DB_DEFAULT               ZIRENE_TREBLEENHANCEMENT_HEADROOM_GAIN_IN_DB_MAX /**< Treble Enhancement, default value when parameter is not specifically set [dB] */
#define ZIRENE_TREBLEENHANCEMENT_MAX_GAIN_IN_DB_MIN                          0  /**< Treble Enhancement, minimum allowed value for maximum gain parameter [dB] */
#define ZIRENE_TREBLEENHANCEMENT_MAX_GAIN_IN_DB_MAX                         30  /**< Treble Enhancement, maximum allowed value for maximum gain parameter [dB] */
#define ZIRENE_TREBLEENHANCEMENT_MAX_GAIN_IN_DB_DEFAULT                     12  /**< Treble Enhancement, default value when parameter is not specifically set [dB] */
#define ZIRENE_TREBLEENHANCEMENT_HIGHPASS_CUTOFF_FREQUENCY_IN_HZ_MIN      2000  /**< Treble Enhancement high-pass filter cutoff frequency, minimum allowed value  [Hz] */
#define ZIRENE_TREBLEENHANCEMENT_HIGHPASS_CUTOFF_FREQUENCY_IN_HZ_MAX     20000  /**< Treble Enhancement high-pass filter cutoff frequency, maximum allowed value  [Hz] */
#define ZIRENE_TREBLEENHANCEMENT_HIGHPASS_CUTOFF_FREQUENCY_IN_HZ_DEFAULT  2000  /**< Treble Enhancement, default value when parameter is not specifically set [Hz] */


/**
* The enum contains the list of audio channel combinations supported by the TrebleEnhancement effect in this Zirene release.  
*/
typedef enum {
    ZIRENE_TE_ALL_CHANNELS     = ZIRENE_SPEAKER_ALL_CHANNELS,
    ZIRENE_TE_FRONT_LEFT_RIGHT = (ZIRENE_SPEAKER_FRONT_LEFT | ZIRENE_SPEAKER_FRONT_RIGHT), 
    ZIRENE_TE_BACK_LEFT_RIGHT  = (ZIRENE_SPEAKER_BACK_LEFT  | ZIRENE_SPEAKER_BACK_RIGHT)
} eZireneTrebleEnhancementSupportedChannelCombinations;

/**
* The enum contains the supported options for steepness (slope) of the high-frequency response of Treble Enhancement.  
*/
typedef enum {
    ZIRENE_TREBLEENHANCEMENT_SLOPE_MODERATE = 0,  /**< Moderate slope */
    ZIRENE_TREBLEENHANCEMENT_SLOPE_STEEP          /**< Steep slope */
} eZireneTrebleEnhancement_Slope;
#define ZIRENE_TREBLEENHANCEMENT_SLOPE_DEFAULT	ZIRENE_TREBLEENHANCEMENT_SLOPE_STEEP	/**< Treble Enhancement, default slope setting */


/**
* The enum contains the list of parameters supported by TrebleEnhancement.  
*/
typedef enum 
{
    /** Enables or disables the TrebleEnhancement effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). */
    ZIRENE_TREBLEENHANCEMENT_ENABLE_IN_ONOFF = 0,
    /** Used to request the "Active State" for Treble Enhancement.<br>
      *  It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() 
      * @note This parameter states if the Enable State for Treble Enhancement has been overruled internally.<br>
      *       If the Active State equals ::AM3D_FALSE the Enable State has been overruled. Otherwise it has not.*/
    ZIRENE_TREBLEENHANCEMENT_ACTIVESTATE_IN_ONOFF,
    /** TrebleEnhancement boosts audio above this frequency.\n
     * iValue range is from ::ZIRENE_TREBLEENHANCEMENT_HIGHPASS_CUTOFF_FREQUENCY_IN_HZ_MIN <br>
     * to ::ZIRENE_TREBLEENHANCEMENT_HIGHPASS_CUTOFF_FREQUENCY_IN_HZ_MAX in steps of 1 [Hz]. 
	 * @note The Active State for Treble Enhancement will become false (::AM3D_FALSE),<br>
     *       if the Highpass Cutoff Frequency is higher than SampleRate/2 set using e.g. Zirene_SetSampleRate().<br>
     *       This disabling is acoustic property based and is reflected in the output from Zirene_GetParameter(). */
    ZIRENE_TREBLEENHANCEMENT_HIGHPASS_CUTOFF_FREQUENCY_IN_HZ,
    /** Treble Enhancement defines the slope of the highpass filter.\n
     * iValue supports ::ZIRENE_TREBLEENHANCEMENT_SLOPE_MODERATE, ::ZIRENE_TREBLEENHANCEMENT_SLOPE_STEEP. */
    ZIRENE_TREBLEENHANCEMENT_SLOPE_IN_INDEX,
    /** Sets the TrebleEnhancement headroom gain in dB.\n
     * iValue range is from ::ZIRENE_TREBLEENHANCEMENT_HEADROOM_GAIN_IN_DB_MIN to ::ZIRENE_TREBLEENHANCEMENT_HEADROOM_GAIN_IN_DB_MAX in steps of 1 [dB].
     * Please note the most high frequency enhancement is achieved with the most negative value. */
    ZIRENE_TREBLEENHANCEMENT_HEADROOM_GAIN_IN_DB,
    /** Sets the TrebleEnhancement maximum gain in dB.\n
     * iValue range is from ::ZIRENE_TREBLEENHANCEMENT_MAX_GAIN_IN_DB_MIN to ::ZIRENE_TREBLEENHANCEMENT_MAX_GAIN_IN_DB_MAX in steps of 1 [dB]. */
    ZIRENE_TREBLEENHANCEMENT_MAX_GAIN_IN_DB
} eZirene_TrebleEnhancement_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 LevelAlignment
  ---------------------------------------------------------------------------------------------------------------------*/
/**
* The enum contains the list of audio channel combinations supported by the Level Alignment effect in this Zirene release.  
*/
typedef enum {
    ZIRENE_LA_ALL_CHANNELS = ZIRENE_SPEAKER_ALL_CHANNELS
} eZireneLevelAlignmentSupportedChannelCombinations;

/**
* The enum contains the list of supported options for controlling mode parameter used in LevelAlignment.  
*/
typedef enum {
    ZIRENE_LEVELALIGNMENT_MODE_MUSIC = 0,  /**< Music mode */
    ZIRENE_LEVELALIGNMENT_MODE_MOVIE       /**< Movie mode */
} eLevelAlignment_Mode;


/**
* The enum contains the list of parameters supported by LevelAlignment.  
*/
typedef enum 
{
    /** Enables or disables the LevelAlignment effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). */
    ZIRENE_LEVELALIGNMENT_ENABLE_IN_ONOFF = 0,
    /** Used to request the "Active State" for LevelAlignment.<br>
     * It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() */
    ZIRENE_LEVELALIGNMENT_ACTIVESTATE_IN_ONOFF,
    /** Sets the mode parameter on LevelAlignment.\n
     * iValue supports ::ZIRENE_LEVELALIGNMENT_MODE_MUSIC and ::ZIRENE_LEVELALIGNMENT_MODE_MOVIE.
     * The default mode is ::ZIRENE_LEVELALIGNMENT_MODE_MUSIC. The intended use of ::ZIRENE_LEVELALIGNMENT_MODE_MOVIE is when LevelAlignment is used
      for movie typically together with voice enhancement. */
    ZIRENE_LEVELALIGNMENT_MODE_IN_NA
} eZirene_LevelAlignment_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 PowerBass
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_POWERBASS_HEADROOM_GAIN_IN_DB_MIN                -12  /**< PowerBass, minimum allowed headroom gain [dB] */
#define ZIRENE_POWERBASS_HEADROOM_GAIN_IN_DB_MAX                  0  /**< PowerBass, maximum allowed headroom gain [dB] */
#define ZIRENE_POWERBASS_HEADROOM_GAIN_IN_DB_DEFAULT             ZIRENE_POWERBASS_HEADROOM_GAIN_IN_DB_MAX /**< PowerBass, default value when parameter is not specifically set [dB] */
#define ZIRENE_POWERBASS_MAX_GAIN_IN_DB_MIN                       0  /**< PowerBass, minimum value for max gain parameter [dB] */
#define ZIRENE_POWERBASS_MAX_GAIN_IN_DB_MAX                      24  /**< PowerBass, maximum value for max gain parameter [dB] */
#define ZIRENE_POWERBASS_MAX_GAIN_IN_DB_DEFAULT                  12  /**< PowerBass, default value when parameter is not specifically set [dB] */
#define ZIRENE_POWERBASS_UPPER_CUTOFF_FREQUENCY_IN_HZ_MIN        50  /**< PowerBass low-pass filter cutoff frequency, minimum allowed value [Hz] */
#define ZIRENE_POWERBASS_UPPER_CUTOFF_FREQUENCY_IN_HZ_MAX       300  /**< PowerBass low-pass filter cutoff frequency, maximum allowed value [Hz] */
#define ZIRENE_POWERBASS_UPPER_CUTOFF_FREQUENCY_IN_HZ_DEFAULT   200  /**< PowerBass, default value when parameter is not specifically set [Hz] */

/**
* The enum contains the list of audio channel combinations supported by the PowerBass effect in this Zirene release.  
*/
typedef enum {
    ZIRENE_PB_ALL_CHANNELS     = ZIRENE_SPEAKER_ALL_CHANNELS,
    ZIRENE_PB_FRONT_LEFT_RIGHT = (ZIRENE_SPEAKER_FRONT_LEFT | ZIRENE_SPEAKER_FRONT_RIGHT), 
    ZIRENE_PB_BACK_LEFT_RIGHT  = (ZIRENE_SPEAKER_BACK_LEFT  | ZIRENE_SPEAKER_BACK_RIGHT)
} eZirenePowerBassSupportedChannelCombinations;


/**
* The enum contains the list of parameters supported by PowerBass.  
*/
typedef enum 
{
    /** Enables or disables the PowerBass effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). */
    ZIRENE_POWERBASS_ENABLE_IN_ONOFF = 0,
    /** Used to request the "Active State" for PowerBass.<br>
     * It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() */
    ZIRENE_POWERBASS_ACTIVESTATE_IN_ONOFF,
    /** PowerBass boosts audio below this frequency.\n
     * iValue range is from ::ZIRENE_POWERBASS_UPPER_CUTOFF_FREQUENCY_IN_HZ_MIN to ::ZIRENE_POWERBASS_UPPER_CUTOFF_FREQUENCY_IN_HZ_MAX in steps of 1 [Hz]. */
    ZIRENE_POWERBASS_UPPER_CUTOFF_FREQUENCY_IN_HZ,
    /** Sets the PowerBass headroom gain in dB. \n
     * iValue range is from ::ZIRENE_POWERBASS_HEADROOM_GAIN_IN_DB_MIN to ::ZIRENE_POWERBASS_HEADROOM_GAIN_IN_DB_MAX in steps of 1 [dB].
     * Please note the most bass is achieved with the most negative value. */
    ZIRENE_POWERBASS_HEADROOM_GAIN_IN_DB,
    /** Sets the PowerBass level/boost in dB.\n
     * iValue range is from ::ZIRENE_POWERBASS_MAX_GAIN_IN_DB_MIN to ::ZIRENE_POWERBASS_MAX_GAIN_IN_DB_MAX in steps of 1 [dB]. */
    ZIRENE_POWERBASS_MAX_GAIN_IN_DB
} eZirene_PowerBass_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 High Pass Filter
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_HIGHPASSFILTER_LOWER_CUTOFF_FREQUENCY_IN_HZ_MIN          15   /**< High-pass filter cutoff frequency, minimum allowed value  [Hz] */
#define ZIRENE_HIGHPASSFILTER_LOWER_CUTOFF_FREQUENCY_IN_HZ_MAX        1500   /**< High-pass filter cutoff frequency, maximum allowed value  [Hz] */
#define ZIRENE_HIGHPASSFILTER_LOWER_CUTOFF_FREQUENCY_IN_HZ_DEFAULT     100   /**< High-pass filter cutoff frequency, default value when parameter is not specifically set [Hz] */

/**
* The enum contains the list of audio channel combinations supported by the Highpass Filter effect in this Zirene release.  
*/
typedef enum {
    ZIRENE_HPF_ALL_CHANNELS     = ZIRENE_SPEAKER_ALL_CHANNELS,
    ZIRENE_HPF_FRONT_LEFT_RIGHT = (ZIRENE_SPEAKER_FRONT_LEFT | ZIRENE_SPEAKER_FRONT_RIGHT), 
    ZIRENE_HPF_BACK_LEFT_RIGHT  = (ZIRENE_SPEAKER_BACK_LEFT  | ZIRENE_SPEAKER_BACK_RIGHT)
} eZireneHighpassFilterSupportedChannelCombinations;

/**
* The enum contains the list of parameters supported by HighPassFilter.  
*/
typedef enum 
{
    /** Enables or disables the HighPassFilter effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). */
    ZIRENE_HIGHPASSFILTER_ENABLE_IN_ONOFF = 0,
    /** Used to request the "Active State" for High Pass Filter.<br>
     * It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() */
    ZIRENE_HIGHPASSFILTER_ACTIVESTATE_IN_ONOFF,
    /** Zirene does not reproduce audio content below this frequency.\n
     * iValue range is from ::ZIRENE_HIGHPASSFILTER_LOWER_CUTOFF_FREQUENCY_IN_HZ_MIN to ::ZIRENE_HIGHPASSFILTER_LOWER_CUTOFF_FREQUENCY_IN_HZ_MAX in steps of 1 [Hz]. */
    ZIRENE_HIGHPASSFILTER_LOWER_CUTOFF_FREQUENCY_IN_HZ,
} eZirene_HighPassFilter_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 Graphic EQ
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_GRAPHICEQ_BAND1_LOW_BASS_GAIN_IN_DB_MIN              -6 /**< Minimum supported gain for the "Low Bass" band in dB */
#define ZIRENE_GRAPHICEQ_BAND1_LOW_BASS_GAIN_IN_DB_MAX              +6 /**< Maxmum supported gain for the "Low Bass" band in dB*/
#define ZIRENE_GRAPHICEQ_BAND1_LOW_BASS_GAIN_IN_DB_DEFAULT           0 /**< Default gain for for the "Low Bass" band */
#define ZIRENE_GRAPHICEQ_BAND2_HIGH_BASS_GAIN_IN_DB_MIN             -6 /**< Minimum supported gain for the "High Bass" band in dB */
#define ZIRENE_GRAPHICEQ_BAND2_HIGH_BASS_GAIN_IN_DB_MAX             +6 /**< Maxmum supported gain for the "High Bass" band in dB*/
#define ZIRENE_GRAPHICEQ_BAND2_HIGH_BASS_GAIN_IN_DB_DEFAULT          0 /**< Default gain for the "High Bass" band */
#define ZIRENE_GRAPHICEQ_BAND3_MID_GAIN_IN_DB_MIN                   -6 /**< Minimum supported gain for the "Mid" band in dB */
#define ZIRENE_GRAPHICEQ_BAND3_MID_GAIN_IN_DB_MAX                   +6 /**< Maxmum supported gain for the "Mid" band in dB*/
#define ZIRENE_GRAPHICEQ_BAND3_MID_GAIN_IN_DB_DEFAULT                0 /**< Default gain for the "Mid" band */
#define ZIRENE_GRAPHICEQ_BAND4_LOW_TREBLE_GAIN_IN_DB_MIN            -6 /**< Minimum supported gain for the "Low Treble" band in dB */
#define ZIRENE_GRAPHICEQ_BAND4_LOW_TREBLE_GAIN_IN_DB_MAX            +6 /**< Maxmum supported gain for the "Low Treble" band in dB*/
#define ZIRENE_GRAPHICEQ_BAND4_LOW_TREBLE_GAIN_IN_DB_DEFAULT         0 /**< Default gain for the "Low Treble" band */
#define ZIRENE_GRAPHICEQ_BAND5_HIGH_TREBLE_GAIN_IN_DB_MIN           -6 /**< Minimum supported gain for the "High Treble" band in dB */
#define ZIRENE_GRAPHICEQ_BAND5_HIGH_TREBLE_GAIN_IN_DB_MAX           +6 /**< Maxmum supported gain for the "High Treble" band in dB*/
#define ZIRENE_GRAPHICEQ_BAND5_HIGH_TREBLE_GAIN_IN_DB_DEFAULT        0 /**< Default gain for the "High Treble" band */


/**
* The enum contains the list of audio channel combinations supported by the Graphic EQ effect.  
*/
typedef enum {
    ZIRENE_GEQ_ALL_CHANNELS = ZIRENE_SPEAKER_ALL_CHANNELS
} eZireneGraphicEqSupportedChannelCombinations;

/**
* The enum contains the list of parameters supported by GraphicEQ.  
*/
typedef enum 
{
    /** Enables or disables the GraphicEQ effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). */
    ZIRENE_GRAPHICEQ_ENABLE_IN_ONOFF = 0,
    /** Used to request the "Active State" for Graphic EQ.<br>
     * It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() */
    ZIRENE_GRAPHICEQ_ACTIVESTATE_IN_ONOFF,
    /** Sets gain for the low bass frequency range (20 Hz - 80 Hz).\n
     * iValue range is from ::ZIRENE_GRAPHICEQ_BAND1_LOW_BASS_GAIN_IN_DB_MIN to ::ZIRENE_GRAPHICEQ_BAND1_LOW_BASS_GAIN_IN_DB_MAX. */
    ZIRENE_GRAPHICEQ_BAND1_LOW_BASS_GAIN_IN_DB,
    /** Sets gain for the high bass frequency range (60 Hz - 250 Hz).\n
     * iValue range is from ::ZIRENE_GRAPHICEQ_BAND2_HIGH_BASS_GAIN_IN_DB_MIN to ::ZIRENE_GRAPHICEQ_BAND2_HIGH_BASS_GAIN_IN_DB_MAX. */
    ZIRENE_GRAPHICEQ_BAND2_HIGH_BASS_GAIN_IN_DB,
    /** Sets gain for the mid frequency range (250 Hz - 1000 Hz).\n
     * iValue range is from ::ZIRENE_GRAPHICEQ_BAND3_MID_GAIN_IN_DB_MIN to ::ZIRENE_GRAPHICEQ_BAND3_MID_GAIN_IN_DB_MAX. */
    ZIRENE_GRAPHICEQ_BAND3_MID_GAIN_IN_DB,
    /** Sets gain for the low treble frequency range (1000 Hz - 4000 Hz).\n
     * iValue range is from ::ZIRENE_GRAPHICEQ_BAND4_LOW_TREBLE_GAIN_IN_DB_MIN to ::ZIRENE_GRAPHICEQ_BAND4_LOW_TREBLE_GAIN_IN_DB_MAX. */
    ZIRENE_GRAPHICEQ_BAND4_LOW_TREBLE_GAIN_IN_DB,
    /** Sets gain for the high treble frequency range (4000 Hz - 16000 Hz).\n
     * iValue range is from ::ZIRENE_GRAPHICEQ_BAND5_HIGH_TREBLE_GAIN_IN_DB_MIN to ::ZIRENE_GRAPHICEQ_BAND5_HIGH_TREBLE_GAIN_IN_DB_MAX. */
    ZIRENE_GRAPHICEQ_BAND5_HIGH_TREBLE_GAIN_IN_DB
} eZirene_GraphicEQ_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 Tone Control
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_TONECONTROL_BASS_GAIN_IN_DB_MIN           -6 /**< Minimum supported gain in bass section in dB   */
#define ZIRENE_TONECONTROL_BASS_GAIN_IN_DB_MAX           +6 /**< Maximum supported gain in bass section in dB   */
#define ZIRENE_TONECONTROL_BASS_GAIN_IN_DB_DEFAULT        0 /**< Default gain for bass section                  */
#define ZIRENE_TONECONTROL_MIDDLE_GAIN_IN_DB_MIN         -6 /**< Minimum supported gain in middle section in dB */
#define ZIRENE_TONECONTROL_MIDDLE_GAIN_IN_DB_MAX         +6 /**< Maximum supported gain in middle section in dB */
#define ZIRENE_TONECONTROL_MIDDLE_GAIN_IN_DB_DEFAULT      0 /**< Default gain for middle section                */
#define ZIRENE_TONECONTROL_TREBLE_GAIN_IN_DB_MIN         -6 /**< Minimum supported gain in treble section in dB */
#define ZIRENE_TONECONTROL_TREBLE_GAIN_IN_DB_MAX         +6 /**< Maximum supported gain in treble section in dB */
#define ZIRENE_TONECONTROL_TREBLE_GAIN_IN_DB_DEFAULT      0 /**< Default gain for treble section                */
#define ZIRENE_TONECONTROL_BASS_QFACTOR_MIN             410 /**< Minimum supported quality factor in bass section expressed in Q10 fixed-point format (0.4)   */
#define ZIRENE_TONECONTROL_BASS_QFACTOR_MAX            4096 /**< Maximum supported quality factor in bass section expressed in Q10 fixed-point format (4.0)   */
#define ZIRENE_TONECONTROL_BASS_QFACTOR_DEFAULT        1024 /**< Default quality factor for bass section expressed in Q10 fixed-point format (1.0)            */
#define ZIRENE_TONECONTROL_MIDDLE_QFACTOR_MIN           410 /**< Minimum supported quality factor in middle section expressed in Q10 fixed-point format (0.4) */
#define ZIRENE_TONECONTROL_MIDDLE_QFACTOR_MAX          4096 /**< Maximum supported quality factor in middle section expressed in Q10 fixed-point format (4.0) */
#define ZIRENE_TONECONTROL_MIDDLE_QFACTOR_DEFAULT      1024 /**< Default quality factor for middle section expressed in Q10 fixed-point format (1.0)          */
#define ZIRENE_TONECONTROL_TREBLE_QFACTOR_MIN           410 /**< Minimum supported quality factor in treble section expressed in Q10 fixed-point format (0.4) */
#define ZIRENE_TONECONTROL_TREBLE_QFACTOR_MAX          4096 /**< Maximum supported quality factor in treble section expressed in Q10 fixed-point format (4.0) */
#define ZIRENE_TONECONTROL_TREBLE_QFACTOR_DEFAULT      1024 /**< Default quality factor for treble section expressed in Q10 fixed-point format (1.0)          */
#define ZIRENE_TONECONTROL_BASS_CENTER_FREQUENCY_IN_HZ_MIN          60 /**< Minimum supported center frequency in Hz for Bass section */
#define ZIRENE_TONECONTROL_BASS_CENTER_FREQUENCY_IN_HZ_MAX         599 /**< Maximum supported center frequency in Hz for Bass section */
#define ZIRENE_TONECONTROL_BASS_CENTER_FREQUENCY_IN_HZ_DEFAULT     100 /**< Default center frequency in Hz for Bass section           */
#define ZIRENE_TONECONTROL_MIDDLE_CENTER_FREQUENCY_IN_HZ_MIN       600 /**< Minimum supported center frequency in Hz for Middle section */
#define ZIRENE_TONECONTROL_MIDDLE_CENTER_FREQUENCY_IN_HZ_MAX      4999 /**< Maximum supported center frequency in Hz for Middle section */
#define ZIRENE_TONECONTROL_MIDDLE_CENTER_FREQUENCY_IN_HZ_DEFAULT  2000 /**< Default center frequency in Hz for Middle section           */
#define ZIRENE_TONECONTROL_TREBLE_CENTER_FREQUENCY_IN_HZ_MIN      5000 /**< Minimum supported center frequency in Hz for Treble section */
#define ZIRENE_TONECONTROL_TREBLE_CENTER_FREQUENCY_IN_HZ_MAX     20000 /**< Maximum supported center frequency in Hz for Treble section */
#define ZIRENE_TONECONTROL_TREBLE_CENTER_FREQUENCY_IN_HZ_DEFAULT 10000 /**< Default center frequency in Hz for Treble section */

/**
* The enum contains the list of audio channel combinations supported by the Tone Control effect.
*/
typedef enum {
    ZIRENE_TC_ALL_CHANNELS = ZIRENE_SPEAKER_ALL_CHANNELS
} eZireneToneControlSupportedChannelCombinations;

/**
* The enum contains the list of parameters supported by Tone Control.  
*/
typedef enum 
{
    /** Enables or disables the ToneControl effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). */
    ZIRENE_TONECONTROL_ENABLE_IN_ONOFF = 0,
    /** Used to request the "Active State" for Tone Control.<br>
     * It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() */
    ZIRENE_TONECONTROL_ACTIVESTATE_IN_ONOFF,
    /** Sets center frequency in Hz for the bass section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_BASS_CENTER_FREQUENCY_IN_HZ_MIN to ::ZIRENE_TONECONTROL_BASS_CENTER_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_TONECONTROL_BASS_CENTER_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the bass section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_BASS_GAIN_IN_DB_MIN to ::ZIRENE_TONECONTROL_BASS_GAIN_IN_DB_MAX. */
    ZIRENE_TONECONTROL_BASS_GAIN_IN_DB,
    /** Sets quality factor for the bass section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_BASS_QFACTOR_MIN to ::ZIRENE_TONECONTROL_BASS_QFACTOR_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_TONECONTROL_BASS_QFACTOR,
    /** Sets center frequency in Hz for the middle section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_MIDDLE_CENTER_FREQUENCY_IN_HZ_MIN to ::ZIRENE_TONECONTROL_MIDDLE_CENTER_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_TONECONTROL_MIDDLE_CENTER_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the middle section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_MIDDLE_GAIN_IN_DB_MIN to ::ZIRENE_TONECONTROL_MIDDLE_GAIN_IN_DB_MAX. */
    ZIRENE_TONECONTROL_MIDDLE_GAIN_IN_DB,
    /** Sets quality factor for the middle section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_MIDDLE_QFACTOR_MIN to ::ZIRENE_TONECONTROL_MIDDLE_QFACTOR_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_TONECONTROL_MIDDLE_QFACTOR,
    /** Sets center frequency in Hz for the treble section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_TREBLE_CENTER_FREQUENCY_IN_HZ_MIN to ::ZIRENE_TONECONTROL_TREBLE_CENTER_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_TONECONTROL_TREBLE_CENTER_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the treble section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_TREBLE_GAIN_IN_DB_MIN to ::ZIRENE_TONECONTROL_TREBLE_GAIN_IN_DB_MAX. */
    ZIRENE_TONECONTROL_TREBLE_GAIN_IN_DB,
    /** Sets quality factor for the treble section.\n
     * iValue range is from ::ZIRENE_TONECONTROL_TREBLE_QFACTOR_MIN to ::ZIRENE_TONECONTROL_TREBLE_QFACTOR_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_TONECONTROL_TREBLE_QFACTOR
} eZirene_ToneControl_Parameters;


/*---------------------------------------------------------------------------------------------------------------------
	 Parametric EQ
  ---------------------------------------------------------------------------------------------------------------------*/
#define ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MIN                      80  /**< Minimum supported value for Peaking section Center Frequency in Hz */
#define ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MAX                   14000  /**< Maximum supported value for Peaking section Center Frequency in Hz */
#define ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_DEFAULT                1000  /**< Default value for Peaking section Center Frequency in Hz           */
#define ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MIN                                (-12) /**< Minimum supported value for Peaking section Gain in dB             */
#define ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MAX                                  12  /**< Maximum supported value for Peaking section Gain in dB             */
#define ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_DEFAULT                               0  /**< Default value for Peaking section Gain in dB                       */
#define ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MIN                           512  /**< Minimum supported value for Peaking section Q-factor as Q10        */
#define ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MAX                          4096  /**< Maximum supported value for Peaking section Q-factor as Q10        */
#define ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_DEFAULT                      1024  /**< Default value for Peaking section Q-factor as Q10                  */

#define ZIRENE_PARAMETRICEQ_LFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MIN                150  /**< Minimum supported value for LF Shelving section Center Frequency in Hz */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MAX               5000  /**< Maximum supported value for LF Shelving section Center Frequency in Hz */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_DEFAULT           1000  /**< Default value for LF Shelving section Center Frequency in Hz           */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_GAIN_IN_DB_MIN                             (-12) /**< Minimum supported value for LF Shelving section Gain in dB             */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_GAIN_IN_DB_MAX                               12  /**< Maximum supported value for LF Shelving section Gain in dB             */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_GAIN_IN_DB_DEFAULT                            0  /**< Default value for LF Shelving section Gain in dB                       */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_QFACTORQ10_IN_NA_MIN                        512  /**< Minimum supported value for LF Shelving section Q-factor as Q10        */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_QFACTORQ10_IN_NA_MAX                       4096  /**< Maximum supported value for LF Shelving section Q-factor as Q10        */
#define ZIRENE_PARAMETRICEQ_LFSHELVING_QFACTORQ10_IN_NA_DEFAULT                   1024  /**< Default value for LF Shelving section Q-factor as Q10                  */

#define ZIRENE_PARAMETRICEQ_HFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MIN               2000  /**< Minimum supported value for HF Shelving section Center Frequency in Hz */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MAX              15000  /**< Maximum supported value for HF Shelving section Center Frequency in Hz */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_DEFAULT           9000  /**< Default value for HF Shelving section Center Frequency in Hz           */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_GAIN_IN_DB_MIN                             (-12) /**< Minimum supported value for HF Shelving section Gain in dB             */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_GAIN_IN_DB_MAX                               12  /**< Maximum supported value for HF Shelving section Gain in dB             */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_GAIN_IN_DB_DEFAULT                            0  /**< Default value for HF Shelving section Gain in dB                       */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_QFACTORQ10_IN_NA_MIN                        512  /**< Minimum supported value for HF Shelving section Q-factor as Q10        */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_QFACTORQ10_IN_NA_MAX                       4096  /**< Maximum supported value for HF Shelving section Q-factor as Q10        */
#define ZIRENE_PARAMETRICEQ_HFSHELVING_QFACTORQ10_IN_NA_DEFAULT                   1024  /**< Default value for HF Shelving section Q-factor as Q10                  */


/**
* The enum contains the list of audio channel combinations supported by the Parametric EQ effect.
*/
typedef enum {
    ZIRENE_PEQ_ALL_CHANNELS = ZIRENE_SPEAKER_ALL_CHANNELS
} eZireneParametricEqSupportedChannelCombinations;

/**
* The enum contains the list of parameters supported by Parametric EQ.  
*/
typedef enum 
{
    /** Enables or disables the Parametric EQ effect. iValue sets the effect to enabled (::AM3D_TRUE) or disabled (::AM3D_FALSE). 
     * Default the effect is disabled (::AM3D_FALSE). */
    ZIRENE_PARAMETRICEQ_ENABLE_IN_ONOFF = 0,
    /** Used to request the "Active State" for Parametric EQ.<br>
     * It is only possible to Get the value of Active State parameter, an error (::ZIRENE_UNSUPPORTED) is returned if parameter is used in Zirene_SetParameter() */
    ZIRENE_PARAMETRICEQ_ACTIVESTATE_IN_ONOFF,
    /** Sets center frequency in Hz for the first peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING1_CENTER_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the first peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING1_GAIN_IN_DB,
    /** Sets quality factor for the first peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_PARAMETRICEQ_PEAKING1_QFACTORQ10_IN_NA,
    /** Sets center frequency in Hz for the second peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING2_CENTER_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the second peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING2_GAIN_IN_DB,
    /** Sets quality factor for the second peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_PARAMETRICEQ_PEAKING2_QFACTORQ10_IN_NA,
    /** Sets center frequency in Hz for the third peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING3_CENTER_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the third peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING3_GAIN_IN_DB,
    /** Sets quality factor for the third peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_PARAMETRICEQ_PEAKING3_QFACTORQ10_IN_NA,
    /** Sets center frequency in Hz for the fourth peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_CENTER_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING4_CENTER_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the fourth peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_GAIN_IN_DB_MAX. */
    ZIRENE_PARAMETRICEQ_PEAKING4_GAIN_IN_DB,
    /** Sets quality factor for the fourth peaking section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MIN to ::ZIRENE_PARAMETRICEQ_PEAKING_QFACTORQ10_IN_NA_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_PARAMETRICEQ_PEAKING4_QFACTORQ10_IN_NA,
    /** Sets cutoff frequency in Hz for the low-frequency shelving section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_LFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MIN to ::ZIRENE_PARAMETRICEQ_LFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_PARAMETRICEQ_LFSHELVING_MIDPOINT_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the low-frequency shelving section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_LFSHELVING_GAIN_IN_DB_MIN to ::ZIRENE_PARAMETRICEQ_LFSHELVING_GAIN_IN_DB_MAX. */
    ZIRENE_PARAMETRICEQ_LFSHELVING_GAIN_IN_DB,
    /** Sets quality factor for the low-frequency shelving section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_LFSHELVING_QFACTORQ10_IN_NA_MIN to ::ZIRENE_PARAMETRICEQ_LFSHELVING_QFACTORQ10_IN_NA_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_PARAMETRICEQ_LFSHELVING_QFACTORQ10_IN_NA,
    /** Sets cutoff frequency in Hz for the high-frequency shelving section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_HFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MIN to ::ZIRENE_PARAMETRICEQ_HFSHELVING_MIDPOINT_FREQUENCY_IN_HZ_MAX. */
    ZIRENE_PARAMETRICEQ_HFSHELVING_MIDPOINT_FREQUENCY_IN_HZ,
    /** Sets gain in dB for the high-frequency shelving section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_HFSHELVING_GAIN_IN_DB_MIN to ::ZIRENE_PARAMETRICEQ_HFSHELVING_GAIN_IN_DB_MAX. */
    ZIRENE_PARAMETRICEQ_HFSHELVING_GAIN_IN_DB,
    /** Sets quality factor for the high-frequency shelving section.\n
     * iValue range is from ::ZIRENE_PARAMETRICEQ_HFSHELVING_QFACTORQ10_IN_NA_MIN to ::ZIRENE_PARAMETRICEQ_HFSHELVING_QFACTORQ10_IN_NA_MAX. 
     * @note The quality factor is expressed in Q10 fixed-point format. A quality factor of 1 is then expressed as 1024 in Q10 format. */
    ZIRENE_PARAMETRICEQ_HFSHELVING_QFACTORQ10_IN_NA
} eZirene_ParametricEq_Parameters;


#ifdef __cplusplus
}
#endif 

#endif
