/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        regs-cec.h
*  \brief       HDMI CEC controller driver
*  \details   
*               Important!
*               The default tab size of this source code is setted with 8.
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not 
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source 
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular 
purpose or non-infringement of any patent, copyright or other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source code or the use in the 
source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure 
Agreement between Telechips and Company. 
*/
#ifndef __TCC_CEC_REG_H__
#define __TCC_CEC_REG_H__
#define HDMIDP_CECREG(x)        (0x50000 + (x))

//@{
/**
 * @name CEC config/status registers
 */
#define CEC_STATUS_0            HDMIDP_CECREG(0x0000)
#define CEC_STATUS_1            HDMIDP_CECREG(0x0004)
#define CEC_STATUS_2            HDMIDP_CECREG(0x0008)
#define CEC_STATUS_3            HDMIDP_CECREG(0x000C)
#define CEC_IRQ_MASK            HDMIDP_CECREG(0x0010)
#define CEC_IRQ_CLEAR           HDMIDP_CECREG(0x0014)
#define CEC_LOGIC_ADDR          HDMIDP_CECREG(0x0020)
#define CEC_DIVISOR_0           HDMIDP_CECREG(0x0030)
#define CEC_DIVISOR_1           HDMIDP_CECREG(0x0034)
#define CEC_DIVISOR_2           HDMIDP_CECREG(0x0038)
#define CEC_DIVISOR_3           HDMIDP_CECREG(0x003C)
//@}

//@{
/**
 * @name CEC Tx related registers
 */
#define CEC_TX_CTRL             HDMIDP_CECREG(0x0040)
#define CEC_TX_BYTES            HDMIDP_CECREG(0x0044)
#define CEC_TX_STAT0            HDMIDP_CECREG(0x0060)
#define CEC_TX_STAT1            HDMIDP_CECREG(0x0064)
#define CEC_TX_BUFF0            HDMIDP_CECREG(0x0080)
#define CEC_TX_BUFF1            HDMIDP_CECREG(0x0084)
#define CEC_TX_BUFF2            HDMIDP_CECREG(0x0088)
#define CEC_TX_BUFF3            HDMIDP_CECREG(0x008C)
#define CEC_TX_BUFF4            HDMIDP_CECREG(0x0090)
#define CEC_TX_BUFF5            HDMIDP_CECREG(0x0094)
#define CEC_TX_BUFF6            HDMIDP_CECREG(0x0098)
#define CEC_TX_BUFF7            HDMIDP_CECREG(0x009C)
#define CEC_TX_BUFF8            HDMIDP_CECREG(0x00A0)
#define CEC_TX_BUFF9            HDMIDP_CECREG(0x00A4)
#define CEC_TX_BUFF10           HDMIDP_CECREG(0x00A8)
#define CEC_TX_BUFF11           HDMIDP_CECREG(0x00AC)
#define CEC_TX_BUFF12           HDMIDP_CECREG(0x00B0)
#define CEC_TX_BUFF13           HDMIDP_CECREG(0x00B4)
#define CEC_TX_BUFF14           HDMIDP_CECREG(0x00B8)
#define CEC_TX_BUFF15           HDMIDP_CECREG(0x00BC)
        //@}

//@{
/**
 * @name CEC Rx related registers
 */
#define CEC_RX_CTRL             HDMIDP_CECREG(0x00C0)
#define CEC_RX_CTRL1            HDMIDP_CECREG(0x00C4)
#define CEC_RX_STAT0            HDMIDP_CECREG(0x00E0)
#define CEC_RX_STAT1            HDMIDP_CECREG(0x00E4)
#define CEC_RX_BUFF0            HDMIDP_CECREG(0x0100)
#define CEC_RX_BUFF1            HDMIDP_CECREG(0x0104)
#define CEC_RX_BUFF2            HDMIDP_CECREG(0x0108)
#define CEC_RX_BUFF3            HDMIDP_CECREG(0x010C)
#define CEC_RX_BUFF4            HDMIDP_CECREG(0x0110)
#define CEC_RX_BUFF5            HDMIDP_CECREG(0x0114)
#define CEC_RX_BUFF6            HDMIDP_CECREG(0x0118)
#define CEC_RX_BUFF7            HDMIDP_CECREG(0x011C)
#define CEC_RX_BUFF8            HDMIDP_CECREG(0x0120)
#define CEC_RX_BUFF9            HDMIDP_CECREG(0x0124)
#define CEC_RX_BUFF10           HDMIDP_CECREG(0x0128)
#define CEC_RX_BUFF11           HDMIDP_CECREG(0x012C)
#define CEC_RX_BUFF12           HDMIDP_CECREG(0x0130)
#define CEC_RX_BUFF13           HDMIDP_CECREG(0x0134)
#define CEC_RX_BUFF14           HDMIDP_CECREG(0x0138)
#define CEC_RX_BUFF15           HDMIDP_CECREG(0x013C)
//@}

#define CEC_RX_FILTER_CTRL      HDMIDP_CECREG(0x0180)
#define CEC_RX_FILTER_TH        HDMIDP_CECREG(0x0184)

//@{
/**
 * @name Bit values
 */
#define CEC_STATUS_TX_RUNNING           (1<<0)
#define CEC_STATUS_TX_TRANSFERRING      (1<<1)
#define CEC_STATUS_TX_DONE              (1<<2)
#define CEC_STATUS_TX_ERROR             (1<<3)
#define CEC_STATUS_TX_BYTES             (0xFF<<8)
#define CEC_STATUS_RX_RUNNING           (1<<16)
#define CEC_STATUS_RX_RECEIVING         (1<<17)
#define CEC_STATUS_RX_DONE              (1<<18)
#define CEC_STATUS_RX_ERROR             (1<<19)
#define CEC_STATUS_RX_BCAST             (1<<20)
#define CEC_STATUS_RX_BYTES             (0xFF<<24)

#define CEC_IRQ_TX_DONE                 (1<<0)
#define CEC_IRQ_TX_ERROR                (1<<1)
#define CEC_IRQ_RX_DONE                 (1<<4)
#define CEC_IRQ_RX_ERROR                (1<<5)

#define CEC_TX_CTRL_START               (1<<0)
#define CEC_TX_CTRL_BCAST               (1<<1)
#define CEC_TX_CTRL_RETRY               (0x04<<4)
#define CEC_TX_CTRL_RESET               (1<<7)

#define CEC_RX_CTRL_ENABLE              (1<<0)
#define CEC_RX_CTRL_RESET               (1<<7)

#define CEC_RX_CTRL_CHK_SAMPLING_ERROR	(1<<6)
#define CEC_RX_CTRL_CHK_LOW_TIME_ERROR	(1<<5)
#define CEC_RX_CTRL_CHK_START_BIT_ERROR	(1<<4)

#define CEC_FILTER_EN			(1<<0)

//@}

/** CEC Rx buffer size */
#define CEC_RX_BUFF_SIZE                16
/** CEC Tx buffer size */
#define CEC_TX_BUFF_SIZE                16

#define CEC_LOGIC_ADDR_MASK             0x0F

#endif /* __TCC_CEC_REG_H__ */
