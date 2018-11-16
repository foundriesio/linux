/**  @file
* Copyright (c) 2009-2011 AM3D A/S.
*
* AM3DOSAL.h
*
* \brief AM3D OS Abstraction Layer.
*
* Defines a set of functions  used by AM3D, which can be changed by the end user to
* OS optimal implementations. The AM3D OSAL files should be compiled and linked into the
* target project by the end user.
*
* COPYRIGHT:
* Copyright (c) AM3D A/S. All Rights Reserved.
* AM3D products contain certain trade secrets and confidential information and proprietary information of AM3D. Use, reproduction, 
* disclosure and distribution by any means are prohibited, except pursuant to a written license from AM3D.
* The user of this AM3D software acknowledges and agrees that this software is the sole property of AM3D, that it only must be 
* used to what it is indented to and that it in all aspects shall be handled as Confidential Information. The user also acknowledges
* and agrees that he/she has the proper rights to handle this Confidential Information.
*/

#ifndef _AM3DOSALDYNAMIC_H_
#define _AM3DOSALDYNAMIC_H_

#ifdef __cplusplus
extern "C" {
#endif 

/*---------------------------------------------------------------------------------------------------------------------
	 Type defines
  ---------------------------------------------------------------------------------------------------------------------*/

/* 
* AM3D types. As the length of some data types vary from platform to platform AM3D data types have been defined to avoid confusion.
* For instance int might be 32 bits on some platforms but 16 bits on others. The underlying library has been compiled using these 
* definitions.
*/
typedef signed int AM3D_INT32; /**< Signed 32 bit integer */
typedef signed short AM3D_INT16; /**< Signed 16 bit integer */
typedef char AM3D_CHAR; /**< Char type used for characters and strings */
typedef unsigned int AM3D_UINT32; /**< Unsigned 32 bit integer */
//Data types below are for AM3D internal use
typedef signed long long int AM3D_INT64; /**< Signed 64 bit integer */
typedef signed char AM3D_INT8; /**< Signed 8 bit integer */
typedef unsigned long long AM3D_UINT64; /**< Unsigned 64 bit integer */
typedef unsigned short AM3D_UINT16; /**< Unsigned 16 bit integer */
typedef unsigned char AM3D_UINT8; /**< Unsigned 8 bit integer */
typedef short AM3D_BOOL; /**< Boolean type */

#ifdef __cplusplus
}
#endif 

#endif //_AM3DOSALDYNAMIC_H_

