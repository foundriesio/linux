/*
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2013
 * Description: LINUX H/W DEMUX FUNCTIONS
 *
 *   Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _TCC_CIPHER_IOCTL_H_
#define _TCC_CIPHER_IOCTL_H_

/* DMA Control Register */
#define HwCIPHER_DMACTR_CH(x) ((x)*Hw24)
#define HwCIPHER_DMACTR_CH_Mask (Hw28 + Hw27 + Hw26 + Hw25 + Hw24)
#define HwCIPHER_DMACTR_CH0 HwCIPHER_DMACTR_CH(0)
#define HwCIPHER_DMACTR_CH1 HwCIPHER_DMACTR_CH(1)
#define HwCIPHER_DMACTR_CH2 HwCIPHER_DMACTR_CH(2)
#define HwCIPHER_DMACTR_CH3 HwCIPHER_DMACTR_CH(3)
#define HwCIPHER_DMACTR_CH4 HwCIPHER_DMACTR_CH(4)
#define HwCIPHER_DMACTR_CH5 HwCIPHER_DMACTR_CH(5)
#define HwCIPHER_DMACTR_CH6 HwCIPHER_DMACTR_CH(6)
#define HwCIPHER_DMACTR_CH7 HwCIPHER_DMACTR_CH(7)
#define HwCIPHER_DMACTR_CH8 HwCIPHER_DMACTR_CH(8)
#define HwCIPHER_DMACTR_CH9 HwCIPHER_DMACTR_CH(9)
#define HwCIPHER_DMACTR_CH10 HwCIPHER_DMACTR_CH(10)
#define HwCIPHER_DMACTR_CH11 HwCIPHER_DMACTR_CH(11)
#define HwCIPHER_DMACTR_CH12 HwCIPHER_DMACTR_CH(12)
#define HwCIPHER_DMACTR_CH13 HwCIPHER_DMACTR_CH(13)
#define HwCIPHER_DMACTR_CH14 HwCIPHER_DMACTR_CH(14)
#define HwCIPHER_DMACTR_CH15 HwCIPHER_DMACTR_CH(15)
#define HwCIPHER_DMACTR_InterruptEnable Hw23
#define HwCIPHER_DMACTR_WaitCycle(x) ((x)*Hw20)
#define HwCIPHER_DMACTR_WaitCycle_Mask (Hw22 + Hw21 + Hw20)
#define HwCIPHER_DMACTR_PauseTransfer(x) ((x)*Hw20)
#define HwCIPHER_DMACTR_PauseTransfer_Mask (Hw19 + Hw18 + Hw17)
#define HwCIPHER_DMACTR_RateConEnable Hw16
#define HwCIPHER_DMACTR_WriteBufferable Hw15
#define HwCIPHER_DMACTR_Enable Hw0

/*FIFO Status 0 Register*/
#define HwCIPHER_FIFO0_FULL Hw31
#define HwCIPHER_FIFO0_EMP Hw30
#define HwCIPHER_FIFO0_IDLE Hw29
#define HwCIPHER_FIFO0_NumberofEmpty(x) ((x)*Hw0)
#define HwCIPHER_FIFO0_NumberofEmpty_Mask (Hw7 - Hw0)

/* Cipher Control Register */
#define HwCIPHER_CCTRL_IDLE Hw15
#define HwCIPHER_CCTRL_KEY Hw8
#define HwCIPHER_CCTRL_ENDIAN(x) ((x)*Hw4)
#define HwCIPHER_CCTRL_ENDIAN_MASK (Hw7 - Hw4)
#define HwCIPHER_CCTRL_Algorithm(x) ((x)*Hw0)
#define HwCIPHER_CCTRL_Algorithm_Mask (Hw3 + Hw2 + Hw1 + Hw0)
#define HwCIPHER_CCTRL_Algorithm_BYPASS HwCIPHER_CCTRL_Algorithm(0)
#define HwCIPHER_CCTRL_Algorithm_AES HwCIPHER_CCTRL_Algorithm(1)
#define HwCIPHER_CCTRL_Algorithm_DES HwCIPHER_CCTRL_Algorithm(2)
#define HwCIPHER_CCTRL_Algorithm_MULTI2 HwCIPHER_CCTRL_Algorithm(3)
#define HwCIPHER_CCTRL_Algorithm_CSA2 HwCIPHER_CCTRL_Algorithm(4)
#define HwCIPHER_CCTRL_Algorithm_CSA3 HwCIPHER_CCTRL_Algorithm(5)
#define HwCIPHER_CCTRL_Algorithm_HASH HwCIPHER_CCTRL_Algorithm(6)

/*AES, DES, Multi2 Register (ADM) */
#define HwCIPHER_ADM_KEYLD Hw31
#define HwCIPHER_ADM_IVLD Hw30
#define HwCIPHER_ADM_ENC Hw27
#define HwCIPHER_ADM_OperationMode(x) ((x)*Hw24)
#define HwCIPHER_ADM_OperationMode_Mask (Hw26 + Hw25 + Hw24)
#define HwCIPHER_ADM_OperationMode_ECB HwCIPHER_ADM_OperationMode(0)
#define HwCIPHER_ADM_OperationMode_CBC HwCIPHER_ADM_OperationMode(1)
#define HwCIPHER_ADM_OperationMode_CFB HwCIPHER_ADM_OperationMode(2)
#define HwCIPHER_ADM_OperationMode_OFB HwCIPHER_ADM_OperationMode(3)
#define HwCIPHER_ADM_OperationMode_CTR HwCIPHER_ADM_OperationMode(4)
#define HwCIPHER_ADM_ParityBit Hw6
#define HwCIPHER_ADM_DESMode(x) ((x)*Hw4)
#define HwCIPHER_ADM_DESMode_Mask (Hw5 + Hw4)
#define HwCIPHER_ADM_DESMode_SingleDES HwCIPHER_ADM_DESMode(0)
#define HwCIPHER_ADM_DESMode_DoubleDES HwCIPHER_ADM_DESMode(1)
#define HwCIPHER_ADM_DESMode_TripleDES2 HwCIPHER_ADM_DESMode(2)
#define HwCIPHER_ADM_DESMode_TripleDES3 HwCIPHER_ADM_DESMode(3)
#define HwCIPHER_ADM_KeyLength(x) ((x)*Hw0)
#define HwCIPHER_ADM_KeyLength_Mask (Hw1 + Hw0)
#define HwCIPHER_ADM_keyLength_128 HwCIPHER_ADM_KeyLength(0)
#define HwCIPHER_ADM_KeyLength_192 HwCIPHER_ADM_KeyLength(1)
#define HwCIPHER_ADM_KeyLength_256 HwCIPHER_ADM_KeyLength(2)

/*Hash Register*/
#define HwCIPHER_Hash_Mode(x) ((x)*Hw0)
#define HwCIPHER_Hash_Mode_Mask (Hw2 + Hw1 + Hw0)
#define HwCIPHER_Hash_Mode_SHA1 HwCIPHER_Hash_Mode(0)
#define HwCIPHER_Hash_Mode_SHA224 HwCIPHER_Hash_Mode(1)
#define HwCIPHER_Hash_Mode_SHA256 HwCIPHER_Hash_Mode(2)
#define HwCIPHER_Hash_Mode_SHA384 HwCIPHER_Hash_Mode(3)
#define HwCIPHER_Hash_Mode_SHA512 HwCIPHER_Hash_Mode(4)
#define HwCIPHER_Hash_Mode_SHA512_224 HwCIPHER_Hash_Mode(5)
#define HwCIPHER_Hash_Mode_SHA512_256 HwCIPHER_Hash_Mode(6)

/* The Key Length in AES */
enum {
	TCC_CIPHER_KEYLEN_128 = 0,
	TCC_CIPHER_KEYLEN_192,
	TCC_CIPHER_KEYLEN_256,
	TCC_CIPHER_KEYLEN_256_1,
	TCC_CIPHER_KEYLEN_MAX
};

/* The Mode in DES */
enum {
	TCC_CIPHER_DESMODE_SINGLE = 0,
	TCC_CIPHER_DESMODE_DOUBLE,
	TCC_CIPHER_DESMODE_TRIPLE2,
	TCC_CIPHER_DESMODE_TRIPLE3,
	TCC_CIPHER_DESMODE_MAX
};

/* The Operation Mode */
enum {
	TCC_CIPHER_OPMODE_ECB = 0,
	TCC_CIPHER_OPMODE_CBC,
	TCC_CIPHER_OPMODE_CFB,
	TCC_CIPHER_OPMODE_OFB,
	TCC_CIPHER_OPMODE_CTR,
	TCC_CIPHER_OPMODE_CTR_1,
	TCC_CIPHER_OPMODE_CTR_2,
	TCC_CIPHER_OPMODE_CTR_3,
	TCC_CIPHER_OPMODE_MAX
};

/* The Algorithm of the Encryption */
enum {
	TCC_CIPHER_ALGORITM_BYPASS = 0,	// Use Like DMA
	TCC_CIPHER_ALGORITM_AES = 1,
	TCC_CIPHER_ALGORITM_DES,
	TCC_CIPHER_ALGORITM_MULTI2,
	TCC_CIPHER_ALGORITM_CSA2,
	TCC_CIPHER_ALGORITM_CSA3,
	TCC_CIPHER_ALGORITM_HASH,
	TCC_CIPHER_ALGORITM_MAX
};

/* The Key Select Mode */
enum {
	TCC_CIPHER_KEY_NORMAL = 0,
	TCC_CIPHER_KEY_FROM_OTP,
	TCC_CIPHER_KEY_MAX
};

/* The Base Address */
enum {
	TCC_CIPHER_BASEADDR_TX = 0,
	TCC_CIPHER_BASEADDR_RX,
	TCC_CIPHER_BASEADDR_MAX
};

/* The Key Option for Multi2 */
enum {
	TCC_CIPHER_KEY_MULTI2_DATA = 0,
	TCC_CIPHER_KEY_MULTI2_SYSTEM,
	TCC_CIPHER_KEY_MULTI2_MAX
};

/* CIPHER IOCTL Command */
enum {
	TCC_CIPHER_IOCTL_SET_ALGORITHM = 0x100,
	TCC_CIPHER_IOCTL_SET_KEY,
	TCC_CIPHER_IOCTL_SET_VECTOR,
	TCC_CIPHER_IOCTL_ENCRYPT,
	TCC_CIPHER_IOCTL_DECRYPT,
	TCC_CIPHER_IOCTL_MAX,
};

/*DMA Control Channel */
enum CIPHER_DCTRL_CH_TYPE {
	CH0 = 0x00000,
	CH1 = 0x00001,
	CH2 = 0x00010,
	CH3 = 0x00011,
	CH4 = 0x00100,
	CH5 = 0x00101,
	CH6 = 0x00110,
	CH7 = 0x00111,
	CH8 = 0x01000,
	CH9 = 0x01001,
	CH10 = 0x01010,
	CH11 = 0x01011,
	CH12 = 0x01100,
	CH13 = 0x01101,
	CH14 = 0x01110,
	CH15 = 0x01111,
};

/*HASH Mode */
enum {
	MD5 = 0,
	SHA_1,
	SHA_224,
	SHA_256,
	SHA_384,
	SHA_512,
	SHA_512_224,
	SHA_512_256
};

/* DMA TYPE :  Not used in TCC896x & TCC897x*/
enum {
	TCC_CIPHER_DMA_INTERNAL = 0,
	TCC_CIPHER_DMA_DMAX,
	TCC_CIPHER_DMA_MAX
};

struct stCIPHER_ALGORITHM {
	unsigned int uDmaMode;
	unsigned int uOperationMode;
	unsigned int uAlgorithm;
	unsigned int uArgument1;
	unsigned int uArgument2;
};

struct stCIPHER_KEY {
	unsigned char *pucData;
	unsigned int uLength;
	unsigned int uOption;
};

struct stCIPHER_VECTOR {
	unsigned char *pucData;
	unsigned int uLength;
};

struct stCIPHER_ENCRYPTION {
	unsigned char *pucSrcAddr;
	unsigned char *pucDstAddr;
	unsigned int uLength;
};

struct stCIPHER_DECRYPTION {
	unsigned char *pucSrcAddr;
	unsigned char *pucDstAddr;
	unsigned int uLength;
};

#if 1				// for HWDEMUX Cipher
struct stHWDEMUXCIPHER_ALGORITHM {
	unsigned int uDemuxId;
	unsigned int uOperationMode;
	unsigned int uAlgorithm;
	unsigned int uResidual;
	unsigned int uSmsg;
};

struct stHWDEMUXCIPHER_KEY {
	unsigned int uDemuxId;
	unsigned char *pucData;
	unsigned int uLength;
	unsigned int uKeyType;
	unsigned int uKeyMode;
};

struct stHWDEMUXCIPHER_VECTOR {
	unsigned int uDemuxId;
	unsigned char *pucData;
	unsigned int uLength;
	unsigned int uOption;// 0:init vector, 1:residual init vector
};

struct stHWDEMUXCIPHER_EXECUTE {
	unsigned int uDemuxId;
	unsigned int uExecuteOption;// 0:decryption, 1:encryption
	unsigned int uCWsel;
	unsigned int uKLIdx;
	unsigned int uKeyMode;
};

struct stHWDEMUXCIPHER_ENCRYPTION {
	unsigned int uDemuxId;
	unsigned char *pucSrcAddr;
	unsigned char *pucDstAddr;
	unsigned int uLength;
};

struct stHWDEMUXCIPHER_SENDDATA {
	unsigned int uDemuxId;
	unsigned char *pucSrcAddr;
	unsigned char *pucDstAddr;
	unsigned int uLength;
};

struct stHWDEMUXCIPHER_DECRYPTION {
	unsigned int uDemuxId;
	unsigned char *pucSrcAddr;
	unsigned char *pucDstAddr;
	unsigned int uLength;
};
#endif

#endif
