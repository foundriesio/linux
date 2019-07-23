/* linux/include/linux/tcc_cipher.h
 *
 * Copyright (C) 2009, 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/**
 * @file tcc_cipher.h
 * @brief cipher driver header file
 * @defgroup cipher_drv Cipher Driver
 */
#ifndef _TCC_CIPHER_H_
#define _TCC_CIPHER_H_

#define TCCCIPHER_DEVICE_NAME			"tcc_cipher"

/**
 * @ingroup cipher_drv
 * The Key Length in AES */
typedef enum {
	TCC_CIPHER_KEYLEN_128 = 0,
	TCC_CIPHER_KEYLEN_192,
	TCC_CIPHER_KEYLEN_256,
	TCC_CIPHER_KEYLEN_256_1,
	TCC_CIPHER_KEYLEN_MAX
} eCipherKEYLEN;

/**
 * @ingroup cipher_drv
 * The Mode in DES */
typedef enum {
	TCC_CIPHER_DESMODE_SINGLE = 0,
	TCC_CIPHER_DESMODE_DOUBLE,
	TCC_CIPHER_DESMODE_TRIPLE2,
	TCC_CIPHER_DESMODE_TRIPLE3,
	TCC_CIPHER_DESMODE_MAX
} eCipherDESMODE;

/**
 * @ingroup cipher_drv
 * The Operation Mode */
typedef enum {
	TCC_CIPHER_OPMODE_ECB = 0,
	TCC_CIPHER_OPMODE_CBC,
	TCC_CIPHER_OPMODE_CFB,
	TCC_CIPHER_OPMODE_OFB,
	TCC_CIPHER_OPMODE_CTR,
	TCC_CIPHER_OPMODE_CTR_1,
	TCC_CIPHER_OPMODE_CTR_2,
	TCC_CIPHER_OPMODE_CTR_3,
	TCC_CIPHER_OPMODE_MAX
} eCipherOPMODE;

/**
 * @ingroup cipher_drv
 * The Cipher Algorithm */
typedef enum {
	TCC_CIPHER_ALGORITHM_BYPASS = 0,	//Use Like DMA
	TCC_CIPHER_ALGORITHM_AES = 1,
	TCC_CIPHER_ALGORITHM_DES,
	TCC_CIPHER_ALGORITHM_MULTI2,
	TCC_CIPHER_ALGORITHM_CSA2,
	TCC_CIPHER_ALGORITHM_CSA3,
	TCC_CIPHER_ALGORITHM_HASH,
	TCC_CIPHER_ALGORITHM_MAX
} eCipherALGORITHM;

/**
 * @ingroup cipher_drv
 * The Key Option for Multi2 */
typedef enum {
	TCC_CIPHER_KEY_MULTI2_DATA = 0,
	TCC_CIPHER_KEY_MULTI2_SYSTEM,
	TCC_CIPHER_KEY_MULTI2_MAX
} eCipherMULTI2;

/**
 * @ingroup cipher_drv
 * HASH Mode */
typedef enum {
	TCC_CIPHER_HASH_MD5 = 0,
	TCC_CIPHER_HASH_SHA1 ,
	TCC_CIPHER_HASH_SHA224,
	TCC_CIPHER_HASH_SHA256,
	TCC_CIPHER_HASH_SHA384,
	TCC_CIPHER_HASH_SHA512,
	TCC_CIPHER_HASH_SHA512_224,
	TCC_CIPHER_HASH_SHA512_256
} eCipherHASH;

/**
 * @ingroup cipher_drv
 * Crypto Mode */
typedef enum {                 
    TCC_CIPHER_DECRYPT = 0,    
    TCC_CIPHER_ENCRYPT,        
    TCC_CIPHER_MODE_MAX        
} eCipherMODE;                 

/**
 * @ingroup cipher_drv
 * CIPHER IOCTL Command */
typedef enum {
	TCC_CIPHER_IOCTL_ALGORITHM = 0x100,
	TCC_CIPHER_IOCTL_KEY,
	TCC_CIPHER_IOCTL_IV,
	TCC_CIPHER_IOCTL_SET,
	TCC_CIPHER_IOCTL_RUN,
	TCC_CIPHER_IOCTL_MAX,
} eCipherIOCTL;

/**
 * @ingroup cipher_drv
 * Structure contains the configuration for algorithm
 */
typedef struct {
	unsigned int	opMode;		/**< Operation Mode, refer to eCipherOPMODE*/
	unsigned int	algorithm;	/**< Algorithm, refer to eCipherALGORITHM*/
	unsigned int	arg1;		/**< Argument for algorithm\n
                                     in AES, refer to #eCipherKEYLEN\n
                                     in DES, refer to #eCipherDESMODE\n
                                     in MULTI2, round\n
                                     in CSA2, round\n
                                     in CSA3, round\n
                                     in HASH, refer to #eCipherHASH*/
	unsigned int	arg2;		/**< Argument for #algorithm\n
                                     in HASH, round*/
} tCIPHER_ALGORITHM;

/**
 * @ingroup cipher_drv
 * Structure contains the configuration for key
 */
typedef struct {
	unsigned char	key[40];	/**< Key*/
	unsigned int	len;		/**< Key length*/
	unsigned int	option;		/**< option for MULTI2, refer to #eCipherMULTI2*/
} tCIPHER_KEY;

/**
 * @ingroup cipher_drv
 * Structure contains the configuration for initial vector
 */
typedef struct {
	unsigned char	iv[16];		/**< Initial vector*/
	unsigned int	len;		/**< Initial vector length*/
} tCIPHER_IV;

/**
 * @ingroup cipher_drv
 * Structure contains the configuration to establish the cipher
 */
typedef struct {
	unsigned int	keyLoad;	/**< Load the key or not*/
	unsigned int	ivLoad;		/**< Load the initial vector or not*/
	unsigned int	enc;		/**< Decide encrypt or decrypt*/
} tCIPHER_SET;

/**
 * @ingroup cipher_drv
 * Structure contains the configuration for execute the cipher
 */
typedef struct {
	unsigned char*	srcAddr;	/**< Source address*/
	unsigned char*	dstAddr;	/**< Destination address*/
	unsigned int	len;		/**< Source length*/
	unsigned int 	isPhy;		/**< It is flag for verify source/destination address is physical*/
} tCIPHER_RUN;

#ifdef __KERNEL__

/**
 * @ingroup cipher_drv
 * @brief Open the cipher driver
 *
 * @parma[in] inode A pointer of inode
 * @parma[in] filp A pointer of file
 *
 * @return 0 Success
 * @return else Fail
 */
int tcc_cipher_open( struct inode *inode, struct file *filp );

/**
 * @ingroup cipher_drv
 * @brief Release the cipher driver
 *
 * @parma[in] inode A pointer of inode
 * @parma[in] file A pinter of file
 *
 * @return 0 Success
 * @return else Fail
 */
int tcc_cipher_release( struct inode *inode, struct file *file );

/**
 * @ingroup cipher_drv
 * @brief Set the initail vector
 *
 * @parma[in] iv Initial vector pointer
 * @parma[in] len Initial vector length
 */
void tcc_cipher_iv( unsigned char *iv, unsigned int len );

/**
 * @ingroup cipher_drv
 * @brief Set the Key
 *
 * @parma[in] key Key pointer
 * @parma[in] len Key length
 * @parma[in] option Key option\n
 *                   It is used in MULTI2 algorithm for verify DATA key or not.
 */
void tcc_cipher_key( unsigned char *key, unsigned int len, unsigned int option );

/**
 * @ingroup cipher_drv
 * @brief Set the algorithm
 *
 * @parma[in] opmode Operation Mode, refer to eCipherOPMODE
 * @parma[in] algorithm Algorithm, refer to eCipherALGORITHM
 * @parma[in] arg1 Argument for algorithm\n
 *                 in AES, refer to #eCipherKEYLEN\n
 *                 in DES, refer to #eCipherDESMODE\n
 *                 in MULTI2, round\n
 *                 in CSA2, round\n
 *                 in CSA3, round\n
 *                 in HASH, refer to #eCipherHASH\n
 * @parma[in] arg2 Argument for algorithm\n
 *                 in HASH, round
 */
void tcc_cipher_algorithm( unsigned int opmode, unsigned int algorithm, unsigned int arg1, unsigned int arg2 );

/**
 * @ingroup cipher_drv
 * @brief Set the command FIFO parameters
 *
 * @parma[in] keyLoad load the key or not\n
 *                    0 : do not load the key\n
 *                    else : load the key
 * @parma[in] ivLoad load the initial vector or not\n
 *                   0 : do not load the initial vector\n
 *                   else : load the initial vector
 * @parma[in] enc Decide encrypt or decrypt
 */
void tcc_cipher_set( unsigned int keyLoad, unsigned int ivLoad, unsigned int enc );

/**
 * @ingroup cipher_drv
 * @brief Execute the cipher operation
 *
 * @parma[in] srcAddr Source address
 * @parma[in] dstAddr Destination address
 * @parma[in] len Source length
 * @parma[in] isPhy It is flag for verify source/destination address is physical
 *
 * @return 0 Success
 * @return else Fail
 */
int tcc_cipher_run( unsigned char *srcAddr, unsigned char *dstAddr, unsigned int len, unsigned int isPhy);
#endif

#endif
