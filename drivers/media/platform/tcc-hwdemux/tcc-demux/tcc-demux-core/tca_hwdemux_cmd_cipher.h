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

#ifndef _TCA_HWDEMUX_CIPHER_H_
#define _TCA_HWDEMUX_CIPHER_H_

#include "tcc_cipher_ioctl.h"


#ifdef __cplusplus
extern "C" {
#endif


/*****************************************************************************
*
* Defines
*
******************************************************************************/
/*	HWDEMUX Cipher cmd	*/
/************************************************************************************************
100. QUEUE_HWDEMUX_CIPHER_SET_ALGORITHM
*************************************************************************************************
uMBOX_TX0***************************************************************************************
MSB 																						  LSB
|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
|              CMD    			|   		DMXID			|  	Opeartioon Mode		|  		Algorithm			|
uMBOX_TX1***************************************************************************************
MSB 																						  LSB
|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
|        Key length  			|   		Des Mode		|  		Multi2 round		|  			PRT			|

uMBOX_TX0 : Bit[31:24] - HWDEMUX_Cipher_CMD
			Bit[23:16] - DMXID (Demux ID)
			Bit[15:08] - Operation Mode (ex, CBC,ECB, ....)
			Bit[07:00] - Algorithm (ex, AES, DES, ..)
uMBOX_TX1 : Bit[31:24] - Key length (For AES 128("0"),192("1"),256("2,3") bit)			
			Bit[23:16] - Des Mode (For DES singel, double,..)
			Bit[15:08] - Multi2 round
			Bit[07:00] - PRT(Select the parity bit location of the key in DES (¡°1=MSB is a parity bit.¡±, ¡°0=LSB is a parity bit.¡±)
uMBOX_TX2 : Not Used
uMBOX_TX3 : Not Used
uMBOX_TX4 : Not Used
uMBOX_TX5 : Not Used
uMBOX_TX6 : Not Used
uMBOX_TX7 : Not Used
*/

/************************************************************************************************
101. QUEUE_HWDEMUX_CIPHER_SET_KEY
*************************************************************************************************
uMBOX_TX0***************************************************************************************
MSB 																						  LSB
|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
|              CMD    			|   		DMXID			|  	Key length			|  Multi2 Key option		|
uMBOX_TX1***************************************************************************************
MSB 																						  LSB
|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
|        	Index	  		|   					  			Not Used									|

uMBOX_TX0 : Bit[31:24] - HWDEMUXCipher_CMD
			Bit[23:16] - DMXID (Demux ID)
			Bit[15:08] - Key length
			Bit[07:00] - Multi2 Key option (data key or system key)
uMBOX_TX1 : Bit[31:24] - command index ( Upper 4bit: total index, lower 4bit: current index) (Max Key Size = 256 bit)
			If the key size is over 6*4bytes, next (following command) is needed. total=2, index 1 and index 2
uMBOX_TX2 : key data
uMBOX_TX3 : Key data
uMBOX_TX4 : Key data
uMBOX_TX5 : Key data
uMBOX_TX6 : Key data
uMBOX_TX7 : Key data

//////////////////////////////////////////////////////////////////////////////////////////////////
If the key size is over 8bytes, next (following command) is needed. total=2, index 1 and index 2

uMBOX_TX0 : Bit[31:24] - HWDEMUXCipher_CMD
			Bit[23:16] - DMXID (Demux ID)
			Bit[15:08] - Key length
			Bit[07:00] - Multi2 Key option (data key or system key)
uMBOX_TX1 : Bit[31:24] - command index ( Upper 4bit: total index, lower 4bit: current index) (Max Key Size = 256 bit)
			following command index is 2.
uMBOX_TX2 : key data
uMBOX_TX3 : Key data
uMBOX_TX4 : Key data
uMBOX_TX5 : Key data
uMBOX_TX6 : Key data
uMBOX_TX7 : Key data
*/

/************************************************************************************************
102. QUEUE_HWDEMUX_CIPHER_SET_IV
*************************************************************************************************
uMBOX_TX0***************************************************************************************
MSB 																						  LSB
|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
|              CMD    			|   		DMXID			|  	Init Vector length		|  	Init vector option		|

uMBOX_TX0 : Bit[31:24] - HWDEMUXCipher_CMD
			Bit[23:16] - DMXID (Demux ID)
			Bit[15:08] - Init Vector length
			Bit[07:00] - Init vector option (Init Vector & for residual IV)
uMBOX_TX1 : IV data
uMBOX_TX2 : IV data
uMBOX_TX3 : IV data
uMBOX_TX4 : IV data
uMBOX_TX5 : not used
uMBOX_TX6 : not used
uMBOX_TX7 : not used

*/

/************************************************************************************************
103. QUEUE_HWDEMUX_CIPHER_RUN
*************************************************************************************************
uMBOX_TX0***************************************************************************************
MSB 																						  LSB
|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
|              CMD    			|   		DMXID			|  		option			|	CM Cipher Option		|

uMBOX_TX0 : Bit[31:24] - HWDEMUXCipher_CMD
			Bit[23:16] - DMXID (Demux ID)
			Bit[15:08] - execute option ("0:decryption", "1:encryption")
			Bit[07:00] - Using HWDEMUX Cipher Option ("0: not Used, 1: used")
uMBOX_TX1 : Not Used 
uMBOX_TX2 : Not Used
uMBOX_TX3 : Not Used
uMBOX_TX4 : Not Used
uMBOX_TX5 : Not Used
uMBOX_TX6 : Not Used
uMBOX_TX7 : Not Used
*/
/*
typedef enum
{
    HWDEMUX_CIPHER_SET_ALGORITHM = 0xC0,
    HWDEMUX_CIPHER_SET_KEY,
    HWDEMUX_CIPHER_SET_IV,
    HWDEMUX_CIPHER_EXECUTE,
}HWDEMUXCipher_CMD;
*/

/*****************************************************************************
*
* APIs
*
******************************************************************************/
extern int HWDEMUXCIPHERCMD_Set_Algorithm(stHWDEMUXCIPHER_ALGORITHM *pARG, unsigned int uiTimeOut);
extern int HWDEMUXCIPHERCMD_Set_Key(stHWDEMUXCIPHER_KEY *pARG, unsigned int uiTotalIndex, unsigned int uiCurrentIndex, unsigned int uiTimeOut);
extern int HWDEMUXCIPHERCMD_Set_IV(stHWDEMUXCIPHER_VECTOR *pARG, unsigned int uiTimeOut);
extern int HWDEMUXCIPHERCMD_Cipher_Run(stHWDEMUXCIPHER_EXECUTE *pARG, unsigned int uiTimeOut);
#ifdef __cplusplus
}
#endif

#endif  /* #ifndef _TCA_HWDEMUX_CIPHER_H_  */

