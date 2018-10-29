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

/*---------------------------------------------------------------------------------------------------------------------
	 Memory
  ---------------------------------------------------------------------------------------------------------------------*/

/** 
* Abstraction layer for OS specific memory allocation function.
* Allocates memory.
* @note AM3D_UINT32 forces the max allocation size to 4Gb.
* @param [in] size is the number of bytes to allocate.  
*/
extern void* AM3DOSAL_Malloc(AM3D_UINT32 size);

/** 
* Abstraction layer for OS specific memory de-allocation function.
* De-allocates memory.
* @param [in] ptr Pointer to a memory block previously allocated with malloc to be deallocated.
* If a null pointer is passed as argument, no action occurs.
*/
extern void AM3DOSAL_Free(void* ptr);

/** 
* Abstraction layer for OS specific memory copy function.
* Copies memory. Source and destination memory blocks must not overlap.
* @param [out] dest Pointer to a destination memory block.
* @param [in] src Pointer to a source memory block.
* @param [in] count Number of bytes to copy.
*/
extern void* AM3DOSAL_MemCpy(void* dest, const void* src, AM3D_UINT32 count);

/** 
* Abstraction layer for OS specific memory move function.
* Moves memory. Source and destinationn memory blocks can overlap.
* @param [out] dest Pointer to a destination memory block.
* @param [in] src Pointer to a source memory block.
* @param [in] count Number of bytes to copy.
*/
extern void* AM3DOSAL_MemMove(void* dest, const void* src, AM3D_UINT32 count);

/** 
* Abstraction layer for OS specific memory copy function.
* Sets memory.
* @param [out] dest Pointer to a destination memory block.
* @param [in] c Character value to fill the memory block with.
* @param [in] count Number of bytes to fill.
*/
extern void* AM3DOSAL_MemSet(void* dest, AM3D_INT32 c, AM3D_UINT32 count);

/*---------------------------------------------------------------------------------------------------------------------
	 Thread safety
  ---------------------------------------------------------------------------------------------------------------------*/

/**
* Abstraction layer for OS specific critical section function.  
* Initializes a critical section object.
* @param[in] ppCriticalSectionObject points to a critical section object pointer.
* @return 0 on success, implementation dependent error code should be returned on failure. 
*/
extern AM3D_INT32 AM3DOSAL_CriticalSectionInit(void** ppCriticalSectionObject);

/**
* Abstraction layer for OS specific critical section function.  
* De-initializes a critical section object.
* @param [in] pCriticalSectionObject critical section object pointer.  
*/
extern void AM3DOSAL_CriticalSectionDeInit(void* pCriticalSectionObject);

/**
* Abstraction layer for OS specific critical section function.  
* Waits for ownership of the specified critical section object.
* The function should return when the calling thread is granted ownership.
* @param [in] pCriticalSectionObject critical section object pointer. 
*/
extern void AM3DOSAL_CriticalSectionEnter(void* pCriticalSectionObject);

/**
* Abstraction layer for OS specific critical section function. 
* Releases ownership of the specified critical section object.
* @param [in] pCriticalSectionObject critical section object pointer. 
*/
extern void AM3DOSAL_CriticalSectionLeave(void* pCriticalSectionObject);

/*---------------------------------------------------------------------------------------------------------------------
	 Profiling
  ---------------------------------------------------------------------------------------------------------------------*/

/**
* Abstraction layer for OS specific profilling function. 
* Returns a number e.g. clock cycle or timer value used for profiling.
*/
extern unsigned int AM3DOSAL_GetCounter(void);


#ifdef __cplusplus
}
#endif 

#endif //_AM3DOSALDYNAMIC_H_

