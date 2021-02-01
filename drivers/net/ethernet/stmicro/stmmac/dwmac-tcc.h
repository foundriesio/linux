/*
 * linux/driver/net/tcc_gmac/tca_gmac.h
 * 	
 * Author : Telechips <linux@telechips.com>
 * Created : June 22, 2010
 * Description : This is the driver for the Telechips MAC 10/100/1000 on-chip Ethernet controllers.  
 *               Telechips Ethernet IPs are built around a Synopsys IP Core.
 *
 * Copyright (C) 2010 Telechips
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */ 

#ifndef _TCA_GMAC_H_
#define _TCA_GMAC_H_

//#include <mach/bsp.h>
//#include <mach/gpio.h>
//#include <mach/iomap.h>

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/phy.h>
#include <linux/delay.h>

#include <linux/stmmac.h>

#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/mach-types.h>
#include <asm/system_info.h>
#endif
/* Define HSIO GMAC Structure */

typedef struct {
	unsigned VALUE          :16;
} TCC_DEF16BIT_IDX_TYPE;

typedef union {
	unsigned short          nREG;
	TCC_DEF16BIT_IDX_TYPE   bREG;
} TCC_DEF16BIT_TYPE;

typedef struct {
	unsigned VALUE          :32;
} TCC_DEF32BIT_IDX_TYPE;

typedef union {
	unsigned long           nREG;
	TCC_DEF32BIT_IDX_TYPE   bREG;
} TCC_DEF32BIT_TYPE;

typedef struct _GMACDMA{
    volatile TCC_DEF32BIT_TYPE  BMODE;          // 0x000  R/W  0x00020101   Bus mode register
    volatile TCC_DEF32BIT_TYPE  TPD;            // 0x004  R/W  0x00000000   Transmit poll demand register
    volatile TCC_DEF32BIT_TYPE  RPD;            // 0x008  R/W  0x00000000   Receive poll demand register
    volatile TCC_DEF32BIT_TYPE  RDLA;           // 0x00C  R/W  0x00000000   Receive descriptor list address register
    volatile TCC_DEF32BIT_TYPE  TDLA;           // 0x010  R/W  0x00000000   Transmit descriptor list address reigster
    volatile TCC_DEF32BIT_TYPE  STS;            // 0x014  R    -            Status register
    volatile TCC_DEF32BIT_TYPE  OPMODE;         // 0x018  R/W  0x00000000   Operation mode register
    volatile TCC_DEF32BIT_TYPE  IE;             // 0x01C  R/W  0x00000000   Interrupt enable register
    volatile TCC_DEF32BIT_TYPE  MFBOC;          // 0x020  R/C  0x00000000   Missed frame and buffer overflow counter register
    unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  CHTD;           // 0x048  R    -            Current host transmit descriptor register
    volatile TCC_DEF32BIT_TYPE  CHRD;           // 0x04C  R    -            Current host receive descriptor register
    volatile TCC_DEF32BIT_TYPE  CHTBA;          // 0x050  R    -            Current host transmit buffer address register
    volatile TCC_DEF32BIT_TYPE  CHRBA;          // 0x054  R    -            Current host receive buffer address register
} GMACDMA, *PGMACDMA;

typedef struct _GMAC{
    volatile TCC_DEF32BIT_TYPE  MACC;           // 0x000  R/W  0x00000000   MAC configuration register
    volatile TCC_DEF32BIT_TYPE  MACFF;          // 0x004  R/W  0x00000000   MAC frame filter
    volatile TCC_DEF32BIT_TYPE  HTH;            // 0x008  R/W  0x00000000   Hash table high register
    volatile TCC_DEF32BIT_TYPE  HTL;            // 0x00C  R/W  0x00000000   Hash table low register
    volatile TCC_DEF32BIT_TYPE  GMIIA;          // 0x010  R/W  0x00000000   GMII address register
    volatile TCC_DEF32BIT_TYPE  GMIID;          // 0x014  R/W  0x00000000   GMII data register
    volatile TCC_DEF32BIT_TYPE  FC;             // 0x018  R/W  0x00000000   Flow control register
    volatile TCC_DEF32BIT_TYPE  VLANT;          // 0x01C  R/W  0x00000000   VLAN tag register
    volatile TCC_DEF32BIT_TYPE  VERSION;        // 0x020  R/W  0x0000--34   Version register
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  RWFF;           // 0x028  R/W  0x00000000   Remote wake-up frame register
    volatile TCC_DEF32BIT_TYPE  PMTCS;          // 0x02C  R/W  0x00000000   PMT control and status
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  IRQS;           // 0x038  R    -            Interrupt status register
    volatile TCC_DEF32BIT_TYPE  IRQM;           // 0x03C  R/W  0x00000000   Interrupt mask register
    volatile TCC_DEF32BIT_TYPE  MACA0H;         // 0x040  R/W  0x0000FFFF   MAC address0 high register
    volatile TCC_DEF32BIT_TYPE  MACA0L;         // 0x044  R/W  0xFFFFFFFF   MAC address0 low register
    volatile TCC_DEF32BIT_TYPE  MACA1H;         // 0x048  R/W  0x0000FFFF   MAC address1 high register
    volatile TCC_DEF32BIT_TYPE  MACA1L;         // 0x04C  R/W  0xFFFFFFFF   MAC address1 low register
    volatile TCC_DEF32BIT_TYPE  MACA2H;         // 0x050  R/W  0x0000FFFF   MAC address2 high register
    volatile TCC_DEF32BIT_TYPE  MACA2L;         // 0x054  R/W  0xFFFFFFFF   MAC address2 low register
    volatile TCC_DEF32BIT_TYPE  MACA3H;         // 0x058  R/W  0x0000FFFF   MAC address3 high register
    volatile TCC_DEF32BIT_TYPE  MACA3L;         // 0x05C  R/W  0xFFFFFFFF   MAC address3 low register
    volatile TCC_DEF32BIT_TYPE  MACA4H;         // 0x060  R/W  0x0000FFFF   MAC address4 high register
    volatile TCC_DEF32BIT_TYPE  MACA4L;         // 0x064  R/W  0xFFFFFFFF   MAC address4 low register
    volatile TCC_DEF32BIT_TYPE  MACA5H;         // 0x068  R/W  0x0000FFFF   MAC address5 high register
    volatile TCC_DEF32BIT_TYPE  MACA5L;         // 0x06C  R/W  0xFFFFFFFF   MAC address5 low register
    volatile TCC_DEF32BIT_TYPE  MACA6H;         // 0x070  R/W  0x0000FFFF   MAC address6 high register
    volatile TCC_DEF32BIT_TYPE  MACA6L;         // 0x074  R/W  0xFFFFFFFF   MAC address6 low register
    volatile TCC_DEF32BIT_TYPE  MACA7H;         // 0x078  R/W  0x0000FFFF   MAC address7 high register
    volatile TCC_DEF32BIT_TYPE  MACA7L;         // 0x07C  R/W  0xFFFFFFFF   MAC address7 low register
    volatile TCC_DEF32BIT_TYPE  MACA8H;         // 0x080  R/W  0x0000FFFF   MAC address8 high register
    volatile TCC_DEF32BIT_TYPE  MACA8L;         // 0x084  R/W  0xFFFFFFFF   MAC address8 low register
    volatile TCC_DEF32BIT_TYPE  MACA9H;         // 0x088  R/W  0x0000FFFF   MAC address9 high register
    volatile TCC_DEF32BIT_TYPE  MACA9L;         // 0x08C  R/W  0xFFFFFFFF   MAC address9 low register
    volatile TCC_DEF32BIT_TYPE  MACA10H;        // 0x090  R/W  0x0000FFFF   MAC address10 high register
    volatile TCC_DEF32BIT_TYPE  MACA10L;        // 0x094  R/W  0xFFFFFFFF   MAC address10 low register
    volatile TCC_DEF32BIT_TYPE  MACA11H;        // 0x098  R/W  0x0000FFFF   MAC address11 high register
    volatile TCC_DEF32BIT_TYPE  MACA11L;        // 0x09C  R/W  0xFFFFFFFF   MAC address11 low register
    volatile TCC_DEF32BIT_TYPE  MACA12H;        // 0x0A0  R/W  0x0000FFFF   MAC address12 high register
    volatile TCC_DEF32BIT_TYPE  MACA12L;        // 0x0A4  R/W  0xFFFFFFFF   MAC address12 low register
    volatile TCC_DEF32BIT_TYPE  MACA13H;        // 0x0A8  R/W  0x0000FFFF   MAC address13 high register
    volatile TCC_DEF32BIT_TYPE  MACA13L;        // 0x0AC  R/W  0xFFFFFFFF   MAC address13 low register
    volatile TCC_DEF32BIT_TYPE  MACA14H;        // 0x0B0  R/W  0x0000FFFF   MAC address14 high register
    volatile TCC_DEF32BIT_TYPE  MACA14L;        // 0x0B4  R/W  0xFFFFFFFF   MAC address14 low register
    volatile TCC_DEF32BIT_TYPE  MACA15H;        // 0x0B8  R/W  0x0000FFFF   MAC address15 high register
    volatile TCC_DEF32BIT_TYPE  MACA15L;        // 0x0BC  R/W  0xFFFFFFFF   MAC address15 low register
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  RGMIIS;         // 0x0D8  R    -            RGMII status register
    unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  MMC_CNTRL;              // 0x100  R/W  0x00000000   MMC control establishes the operating mode of MMC
    volatile TCC_DEF32BIT_TYPE  MMC_INTR_RX;            // 0x104  R/C  -            MMC receive interrupt maintains the interrupt generated from all of the receive statistic counters.
    volatile TCC_DEF32BIT_TYPE  MMC_INTR_TX;            // 0x108  R/C  -            MMC transmit interrupt maintains the interrupt generated form all of the transmit statistic counters.
    volatile TCC_DEF32BIT_TYPE  MMC_INTR_MASK_RX;       // 0x10C  R/W  0x00000000   MMC receive interrupt mask maintains the mask for the interrupt generated form all of the receive statistic counters
    volatile TCC_DEF32BIT_TYPE  MMC_INTR_MASK_TX;       // 0x110  R/W  0x00000000   MMC transmit interrupt mask maintains the mask for the interrupt generated form all of the transmit statistic counters
    volatile TCC_DEF32BIT_TYPE  TXOCTETCOUNT_GB;        // 0x114  R    -            Number of bytes transmitted, exclusive of preamble and retried bytes, in good and bad frames.
    volatile TCC_DEF32BIT_TYPE  TXFRAMECOUNT_GB;        // 0x118  R    -            Number of good and bad frames transmitted, exclusive of retried frames.
    volatile TCC_DEF32BIT_TYPE  TXBRADCASTFRAMES_G;     // 0x11C  R    -            Number of good broadcast frames transmitted.
    volatile TCC_DEF32BIT_TYPE  TXMULTICASTFRAMES_G;    // 0x120  R    -            Number of good multicast frames transmitted.
    volatile TCC_DEF32BIT_TYPE  TX64OCTETS_GB;          // 0x124  R    -            Number of good and bad frames transmitted with length 64 bytes, exclusive of preamble and retried frames.
    volatile TCC_DEF32BIT_TYPE  TX65TO127OCTETS_GB;     // 0x128  R    -            Number of good and bad frames transmitted with length between 65 and 127 (inclusive) bytes, exclusive of preamble and retried frames.
    volatile TCC_DEF32BIT_TYPE  TX128TO255OCTETS_GB;    // 0x12C  R    -            Number of good and bad frames transmitted with length between 128 and 255 (inclusive) bytes, exclusive of preamble and retried frames.
    volatile TCC_DEF32BIT_TYPE  TX256TO511OCTETS_GB;    // 0x130  R    -            Number of good and bad frames transmitted with length between 256 and 511 (inclusive) bytes, exclusive of preamble and retried frames.
    volatile TCC_DEF32BIT_TYPE  TX512TO1023OCTETS_GB;   // 0x134  R    -            Number of good and bad frames transmitted with length between 512 and 1,023 (inclusive) bytes, exclusive of preamble and retried frames.
    volatile TCC_DEF32BIT_TYPE  TX1024TOMAXOCTETS_GB;   // 0x138  R    -            Number of good and bad frames transmitted with length between 1,024 and maxsize (inclusive) bytes, exclusive of preamble and retried frames.
    volatile TCC_DEF32BIT_TYPE  TXUNICASTFRAMES_GB;     // 0x13C  R    -            Number of good and bad unicast frames transmitted.
    volatile TCC_DEF32BIT_TYPE  TXMULTICASTFRAMES_GB;   // 0x140  R    -            Number of good and bad multicast frames transmitted.
    volatile TCC_DEF32BIT_TYPE  TXBRADCASTFRAMES_GB;    // 0x144  R    -            Number of good and bad broadcast frames transmitted.
    volatile TCC_DEF32BIT_TYPE  TXUNDERFLOWERRR;        // 0x148  R    -            Number of frames aborted due to frame underflow error.
    volatile TCC_DEF32BIT_TYPE  TXSINGLECOL_G;          // 0x14C  R    -            Number of successfully transmitted frames after a single collision in Half-duplex mode.
    volatile TCC_DEF32BIT_TYPE  TXMULTICOL_G;           // 0x150  R    -            Number of successfully transmitted frames after more than a single collision in Half-duplex mode.
    volatile TCC_DEF32BIT_TYPE  TXDEFERRED;             // 0x154  R    -            Number of successfully transmitted frames after a deferral in Half-duplex mode.
    volatile TCC_DEF32BIT_TYPE  TXLATECOL;              // 0x158  R    -            Number of frames aborted due to late collision error.
    volatile TCC_DEF32BIT_TYPE  TXEXESSCOL;             // 0x15C  R    -            Number of frames aborted due to excessive (16) collision errors.
    volatile TCC_DEF32BIT_TYPE  TXCARRIERERRR;          // 0x160  R    -            Number of frames aborted due to carrier sense error (no carrier or loss of carrier).
    volatile TCC_DEF32BIT_TYPE  TXOCTETCOUNT_G;         // 0x164  R    -            Number of bytes transmitted, exclusive of preamble, in good frames only.
    volatile TCC_DEF32BIT_TYPE  TXFRAMECOUNT_G;         // 0x168  R    -            Number of good frames transmitted.
    volatile TCC_DEF32BIT_TYPE  TXEXCESSDEF;            // 0x16C  R    -            Number of frames aborted due to excessive deferral error (deferred for more  than two max-sized frame times).
    volatile TCC_DEF32BIT_TYPE  TXPAUSEFRAMES;          // 0x170  R    -            Number of good PAUSE frames transmitted.
    volatile TCC_DEF32BIT_TYPE  TXVLANFRAMES_G;         // 0x174  R    -            Number of good VLAN frames transmitted, exclusive of retried frames.
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  RXFRAMECOUNT_GB;        // 0x180  R    -            Number of good and bad frames received.
    volatile TCC_DEF32BIT_TYPE  RXOCTETCOUNT_GB;        // 0x184  R    -            Number of bytes received, exclusive of preamble, in good and bad frames.
    volatile TCC_DEF32BIT_TYPE  RXOCTETCOUNT_G;         // 0x188  R    -            Number of bytes received, exclusive of preamble, only in good frames.
    volatile TCC_DEF32BIT_TYPE  RXBRADCASTFRAMES_G;     // 0x18C  R    -            Number of good broadcast frames received.
    volatile TCC_DEF32BIT_TYPE  RXMULTICASTFRAMES_G;    // 0x190  R    -            Number of good multicast frames received.
    volatile TCC_DEF32BIT_TYPE  RXCRCERRR;              // 0x194  R    -            Number of frames received with CRC error.
    volatile TCC_DEF32BIT_TYPE  RXALIGNMENTERRR;        // 0x198  R    -            Number of frames received with alignment (dribble) error. Valid only in 10/100 mode.
    volatile TCC_DEF32BIT_TYPE  RXRUNTERRR;             // 0x19C  R    -            Number of frames received with runt (<64 bytes and CRC error) error.
    volatile TCC_DEF32BIT_TYPE  RXJABBERERRR;           // 0x1A0  R    -            Number of giant frames received with length (including CRC) greater than 1,518 bytes (1,522 bytes for VLAN tagged) and with CRC error. If Jumbo Frame mode is enabled, then frames of length greater than 9,018 bytes (9,022 for VLAN tagged) are considered as giant frames.
    volatile TCC_DEF32BIT_TYPE  RXUNDERSIZE_G;          // 0x1A4  R    -            Number of frames received with length less than 64 bytes, without any errors.
    volatile TCC_DEF32BIT_TYPE  RXOVERSIZE_G;           // 0x1A8  R    -            Number of frames received with length greater than the maxsize (1,518 or 1,522 for VLAN tagged frames), without errors.
    volatile TCC_DEF32BIT_TYPE  RX64OCTETS_GB;          // 0x1AC  R    -            Number of good and bad frames received with length 64 bytes, exclusive of preamble.
    volatile TCC_DEF32BIT_TYPE  RX65TO127OCTETS_GB;     // 0x1B0  R    -            Number of good and bad frames received with length between 65 and 127 (inclusive) bytes, exclusive of preamble.
    volatile TCC_DEF32BIT_TYPE  RX128TO255OCTETS_GB;    // 0x1B4  R    -            Number of good and bad frames received with length between 128 and 255 (inclusive) bytes, exclusive of preamble.
    volatile TCC_DEF32BIT_TYPE  RX256TO511OCTETS_GB;    // 0x1B8  R    -            Number of good and bad frames received with length between 256 and 511 (inclusive) bytes, exclusive of preamble.
    volatile TCC_DEF32BIT_TYPE  RX512TO1023OCTETS_GB;   // 0x1BC  R    -            Number of good and bad frames received with length between 512 and 1,023 (inclusive) bytes, exclusive of preamble.
    volatile TCC_DEF32BIT_TYPE  RX1024TOMAXOCTETS_GB;   // 0x1C0  R    -            Number of good and bad frames received with length between 1,024 and maxsize (inclusive) bytes, exclusive of preamble and retried frames.
    volatile TCC_DEF32BIT_TYPE  RXUNICASTFRAMES_G;      // 0x1C4  R    -            Number of good unicast frames received.
    volatile TCC_DEF32BIT_TYPE  RXLENGTHERRR;           // 0x1C8  R    -            Number of frames received with length error (Length type field ¡Á frame size), for all frames with valid length field.
    volatile TCC_DEF32BIT_TYPE  RXOUTOFRANGETYPE;       // 0x1CC  R    -            Number of frames received with length field not equal to the valid frame size (greater than 1,500 but less than 1,536).
    volatile TCC_DEF32BIT_TYPE  RXPAUSEFRAMES;          // 0x1D0  R    -            Number of missed received frames due to FIFO overflow. This counter is not present in the GMAC-CORE configuration.
    volatile TCC_DEF32BIT_TYPE  rxfifooverflow;         // 0x1D4  R    -            Number of missed received frames due to FIFO overflow. This counter is not present in the GMAC-CORE configuration.
    volatile TCC_DEF32BIT_TYPE  rxvlanframes_gb;        // 0x1D8  R    -            Number of good and bad VLAN frames received.
    volatile TCC_DEF32BIT_TYPE  rxwatchdogerror;        // 0x1DC  R    -            Number of frames received with error due to watchdog timeout error (frames with a data load larger than 2,048 bytes).
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  mmc_ipc_intr_mask_rx;   // 0x200  R/W 0x00000000    MMC IPC Receive Checksum Offload Interrupt Mask maintains the mask for the interrupt generated from the receive IPC statistic counters.
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  mmc_ipc_intr_rx;        // 0x208  R/C 0x00000000    MMC Receive Checksum Offload Interrupt maintains the interrupt that the receive IPC statistic counters generate.
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  rxipv4_gd_frms;         // 0x210  R    -            Number of good IPv4 datagrams received with the TCP, UDP, or ICMP payload
    volatile TCC_DEF32BIT_TYPE  rxipv4_hdrerr_frms;     // 0x214  R    -            Number of IPv4 datagrams received with header (checksum, length, or version mismatch) errors
    volatile TCC_DEF32BIT_TYPE  rxipv4_nopay_frms;      // 0x218  R    -            Number of IPv4 datagram frames received that did not have a TCP, UDP, or ICMP payload processed by the Checksum engine
    volatile TCC_DEF32BIT_TYPE  rxipv4_frag_frms;       // 0x21C  R    -            Number of good IPv4 datagrams with fragmentation
    volatile TCC_DEF32BIT_TYPE  rxipv4_udsbl_frms;      // 0x220  R    -            Number of good IPv4 datagrams received that had a UDP payload with checksum disabled
    volatile TCC_DEF32BIT_TYPE  rxipv6_gd_frms;         // 0x224  R    -            Number of good IPv6 datagrams received with TCP, UDP, or ICMP payloads
    volatile TCC_DEF32BIT_TYPE  rxipv6_hdrerr_frms;     // 0x228  R    -            Number of IPv6 datagrams received with header errors (length or version mismatch)
    volatile TCC_DEF32BIT_TYPE  rxipv6_nopay_frms;      // 0x22C  R    -            Number of IPv6 datagram frames received that did not have a TCP, UDP, or ICMP payload. This includes all IPv6 datagrams with fragmentation or security extension headers
    volatile TCC_DEF32BIT_TYPE  rxudp_gd_frms;          // 0x230  R    -            Number of good IP datagrams with a good UDP payload. This counter is not updated when the rxipv4_udsbl_frms counter is incremented.
    volatile TCC_DEF32BIT_TYPE  rxudp_err_frms;         // 0x234  R    -            Number of good IP datagrams whose UDP payload has a checksum error
    volatile TCC_DEF32BIT_TYPE  rxtcp_gd_frms;          // 0x238  R    -            Number of good IP datagrams with a good TCP payload
    volatile TCC_DEF32BIT_TYPE  rxtcp_err_frms;         // 0x23C  R    -            Number of good IP datagrams whose TCP payload has a checksum error
    volatile TCC_DEF32BIT_TYPE  rxicmp_gd_frms;         // 0x240  R    -            Number of good IP datagrams with a good ICMP payload
    volatile TCC_DEF32BIT_TYPE  rxicmp_err_frms;        // 0x244  R    -            Number of good IP datagrams whose ICMP payload has a checksum error
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  rxipv4_gd_octets;       // 0x250  R    -            Number of bytes received in good IPv4 datagrams encapsulating TCP, UDP, or ICMP data. (Ethernet header, FCS, pad, or IP pad bytes are not included in this counter or in the octet counters listed below).
    volatile TCC_DEF32BIT_TYPE  rxipv4_hdrerr_octets;   // 0x254  R    -            Number of bytes received in IPv4 datagrams with header errors (checksum, length, version mismatch). The value in the Length field of IPv4 header is used to update this counter.
    volatile TCC_DEF32BIT_TYPE  rxipv4_nopay_octets;    // 0x258  R    -            Number of bytes received in IPv4 datagrams that did not have a TCP, UDP, or ICMP payload. The value in the IPv4 header¡¯s Length field is used to update this counter.
    volatile TCC_DEF32BIT_TYPE  rxipv4_frag_octets;     // 0x25C  R    -            Number of bytes received in fragmented IPv4 datagrams. The value in the IPv4 header¡¯s Length field is used to update this counter.
    volatile TCC_DEF32BIT_TYPE  rxipv4_udsbl_octets;    // 0x260  R    -            Number of bytes received in a UDP segment that had the UDP checksum disabled. This counter does not count IP Header bytes.
    volatile TCC_DEF32BIT_TYPE  rxipv6_gd_octets;       // 0x264  R    -            Number of bytes received in good IPv6 datagrams encapsulating TCP, UDP or ICMPv6 data
    volatile TCC_DEF32BIT_TYPE  rxipv6_hdrerr_octets;   // 0x268  R    -            Number of bytes received in IPv6 datagrams with header errors (length, version mismatch). The value in the IPv6 header¡¯s Length field is used to update this counter.
    volatile TCC_DEF32BIT_TYPE  rxipv6_nopay_octets;    // 0x26C  R    -            Number of bytes received in IPv6 datagrams that did not have a TCP, UDP, or ICMP payload. The value in the IPv6 header¡¯s Length field is used to update this counter.
    volatile TCC_DEF32BIT_TYPE  rxudp_gd_octets;        // 0x270  R    -            Number of bytes received in a good UDP segment. This counter (and the counters below) does not count IP header bytes.
    volatile TCC_DEF32BIT_TYPE  rxudp_err_octets;       // 0x274  R    -            Number of bytes received in a UDP segment that had checksum errors
    volatile TCC_DEF32BIT_TYPE  rxtcp_gd_octets;        // 0x278  R    -            Number of bytes received in a good TCP segment
    volatile TCC_DEF32BIT_TYPE  rxtcp_err_octets;       // 0x27C  R    -            Number of bytes received in a TCP segment with checksum errors
    volatile TCC_DEF32BIT_TYPE  rxicmp_gd_octets;       // 0x280  R    -            Number of bytes received in a good ICMP segment
    volatile TCC_DEF32BIT_TYPE  rxicmp_err_octets;      // 0x284  R    -            Number of bytes received in an ICMP segment with checksum errors
    TCC_DEF32BIT_TYPE           NOTDEF0[286];
    volatile TCC_DEF32BIT_TYPE  TSC;            // 0x700  R/W  0x00000000   Time stamp control register
    volatile TCC_DEF32BIT_TYPE  SSI;            // 0x704  R/W  0x00000000   Sub-second increment register
    volatile TCC_DEF32BIT_TYPE  TSH;            // 0x708  R    -            Time stamp high register
    volatile TCC_DEF32BIT_TYPE  TSL;            // 0x70C  R    -            Time stamp low register
    volatile TCC_DEF32BIT_TYPE  TSHU;           // 0x710  R/W  0x00000000   Time stamp high update register
    volatile TCC_DEF32BIT_TYPE  TSLU;           // 0x714  R/W  0x00000000   Time stamp low update register
    volatile TCC_DEF32BIT_TYPE  TSA;            // 0x718  R/W  0x00000000   Time stamp addend register
    volatile TCC_DEF32BIT_TYPE  TTH;            // 0x71C  R/W  0x00000000   Target time high register
    volatile TCC_DEF32BIT_TYPE  TTL;            // 0x720  R/W  0x00000000   Target time low register
    volatile TCC_DEF32BIT_TYPE  NOTDEF1[55];
    volatile TCC_DEF32BIT_TYPE  MACA16H;        // 0x800  R/W  0x0000FFFF   MAC address16 high register
    volatile TCC_DEF32BIT_TYPE  MACA16L;        // 0x804  R/W  0xFFFFFFFF   MAC address16 low register
    volatile TCC_DEF32BIT_TYPE  MACA17H;        // 0x808  R/W  0x0000FFFF   MAC address17 high register
    volatile TCC_DEF32BIT_TYPE  MACA17L;        // 0x80C  R/W  0xFFFFFFFF   MAC address17 low register
    volatile TCC_DEF32BIT_TYPE  MACA18H;        // 0x810  R/W  0x0000FFFF   MAC address18 high register
    volatile TCC_DEF32BIT_TYPE  MACA18L;        // 0x814  R/W  0xFFFFFFFF   MAC address18 low register
    volatile TCC_DEF32BIT_TYPE  MACA19H;        // 0x818  R/W  0x0000FFFF   MAC address19 high register
    volatile TCC_DEF32BIT_TYPE  MACA19L;        // 0x81C  R/W  0xFFFFFFFF   MAC address19 low register
    volatile TCC_DEF32BIT_TYPE  MACA20H;        // 0x820  R/W  0x0000FFFF   MAC address20 high register
    volatile TCC_DEF32BIT_TYPE  MACA20L;        // 0x824  R/W  0xFFFFFFFF   MAC address20 low register
    volatile TCC_DEF32BIT_TYPE  MACA21H;        // 0x828  R/W  0x0000FFFF   MAC address21 high register
    volatile TCC_DEF32BIT_TYPE  MACA21L;        // 0x82C  R/W  0xFFFFFFFF   MAC address21 low register
    volatile TCC_DEF32BIT_TYPE  MACA22H;        // 0x830  R/W  0x0000FFFF   MAC address22 high register
    volatile TCC_DEF32BIT_TYPE  MACA22L;        // 0x834  R/W  0xFFFFFFFF   MAC address22 low register
    volatile TCC_DEF32BIT_TYPE  MACA23H;        // 0x838  R/W  0x0000FFFF   MAC address23 high register
    volatile TCC_DEF32BIT_TYPE  MACA23L;        // 0x83C  R/W  0xFFFFFFFF   MAC address23 low register
    volatile TCC_DEF32BIT_TYPE  MACA24H;        // 0x840  R/W  0x0000FFFF   MAC address24 high register
    volatile TCC_DEF32BIT_TYPE  MACA24L;        // 0x844  R/W  0xFFFFFFFF   MAC address24 low register
    volatile TCC_DEF32BIT_TYPE  MACA25H;        // 0x848  R/W  0x0000FFFF   MAC address25 high register
    volatile TCC_DEF32BIT_TYPE  MACA25L;        // 0x84C  R/W  0xFFFFFFFF   MAC address25 low register
    volatile TCC_DEF32BIT_TYPE  MACA26H;        // 0x850  R/W  0x0000FFFF   MAC address26 high register
    volatile TCC_DEF32BIT_TYPE  MACA26L;        // 0x854  R/W  0xFFFFFFFF   MAC address26 low register
    volatile TCC_DEF32BIT_TYPE  MACA27H;        // 0x858  R/W  0x0000FFFF   MAC address27 high register
    volatile TCC_DEF32BIT_TYPE  MACA27L;        // 0x85C  R/W  0xFFFFFFFF   MAC address27 low register
    volatile TCC_DEF32BIT_TYPE  MACA28H;        // 0x860  R/W  0x0000FFFF   MAC address28 high register
    volatile TCC_DEF32BIT_TYPE  MACA28L;        // 0x864  R/W  0xFFFFFFFF   MAC address28 low register
    volatile TCC_DEF32BIT_TYPE  MACA29H;        // 0x868  R/W  0x0000FFFF   MAC address29 high register
    volatile TCC_DEF32BIT_TYPE  MACA29L;        // 0x86C  R/W  0xFFFFFFFF   MAC address29 low register
    volatile TCC_DEF32BIT_TYPE  MACA30H;        // 0x870  R/W  0x0000FFFF   MAC address30 high register
    volatile TCC_DEF32BIT_TYPE  MACA30L;        // 0x874  R/W  0xFFFFFFFF   MAC address30 low register
    volatile TCC_DEF32BIT_TYPE  MACA31H;        // 0x878  R/W  0x0000FFFF   MAC address31 high register
    volatile TCC_DEF32BIT_TYPE  MACA31L;        // 0x87C  R/W  0xFFFFFFFF   MAC address31 low register
} GMAC, *PGMAC;

typedef struct _GMACDLY {
	volatile union {
		unsigned bREG;
		struct {
			unsigned TXCLKIDLY	: 5;
			unsigned 			: 2;
			unsigned TXCLKIINV	: 1;
			unsigned TXCLKODLY	: 5;
			unsigned			: 2;
			unsigned TXCLKOINV	: 1;
			unsigned TXENDLY	: 5;
			unsigned			: 3;
			unsigned TXERDLY	: 5;
			unsigned			: 3;
		} nREG;
	} DLY0;
	volatile union {
		unsigned bREG;
		struct {
			unsigned TXD0DLY	: 5;
			unsigned 			: 3;
			unsigned TXD1DLY	: 5;
			unsigned			: 3;
			unsigned TXD2DLY	: 5;
			unsigned			: 3;
			unsigned TXD3DLY	: 5;
			unsigned			: 3;
		} nREG;
	} DLY1;
	volatile union {
		unsigned bREG;
		struct {
			unsigned TXD4DLY	: 5;
			unsigned     		: 3;
			unsigned TXD5DLY	: 5;
			unsigned     		: 3;
			unsigned TXD6DLY	: 5;
			unsigned     		: 3;
			unsigned TXD7DLY	: 5;
			unsigned			: 3;
		} nREG;
	} DLY2;
	volatile union {
		unsigned bREG;
		struct {
			unsigned RXCLKIDLY	: 5;
			unsigned 			: 2;
			unsigned RXCLKIINV	: 1;
			unsigned			: 8;
			unsigned RXDVDLY	: 5;
			unsigned			: 3;
			unsigned RXERDLY	: 5;
			unsigned			: 3;
		} nREG;
	} DLY3;
	volatile union {
		unsigned bREG;
		struct {
			unsigned RXD0DLY	: 5;
			unsigned     		: 3;
			unsigned RXD1DLY	: 5;
			unsigned     		: 3;
			unsigned RXD2DLY	: 5;
			unsigned     		: 3;
			unsigned RXD3DLY	: 5;
			unsigned			: 3;
		} nREG;
	} DLY4;
	volatile union {
		unsigned bREG;
		struct {
			unsigned RXD4DLY	: 5;
			unsigned     		: 3;
			unsigned RXD5DLY	: 5;
			unsigned     		: 3;
			unsigned RXD6DLY	: 5;
			unsigned     		: 3;
			unsigned RXD7DLY	: 5;
			unsigned			: 3;
		} nREG;
	} DLY5;
	volatile union {
		unsigned bREG;
		struct {
			unsigned COLDLY		: 5;
			unsigned 			: 3;
			unsigned CRSDLY		: 5;
			unsigned			: 19;
		} nREG;
	} DLY6;
} GMACDLY, *PGMACDLY;


/* Global setting */
#if defined(CONFIG_ARCH_TCC898X)
	#define TCC_PA_HSIOBUSCFG   0x111A0000
	#define TCC_PA_GPIO     0x14200000
#elif defined (CONFIG_ARCH_TCC802X)
	#define TCC_PA_HSIOBUSCFG   0x119A0000
	#define TCC_PA_GPIO     0x14200000
#elif defined (CONFIG_ARCH_TCC899X) || defined (CONFIG_ARCH_TCC803X) || defined (CONFIG_ARCH_TCC901X) || defined (CONFIG_ARCH_TCC805X)
	#define TCC_PA_HSIOBUSCFG   0x11DA0000
	#define TCC_PA_GPIO     0x14200000
#else
#endif

#define Hw31		0x80000000
#define Hw30		0x40000000
#define Hw29		0x20000000
#define Hw28		0x10000000
#define Hw27		0x08000000
#define Hw26		0x04000000
#define Hw25		0x02000000
#define Hw24		0x01000000
#define Hw23		0x00800000
#define Hw22		0x00400000
#define Hw21		0x00200000
#define Hw20		0x00100000
#define Hw19		0x00080000
#define Hw18		0x00040000
#define Hw17		0x00020000
#define Hw16		0x00010000
#define Hw15		0x00008000
#define Hw14		0x00004000
#define Hw13		0x00002000
#define Hw12		0x00001000
#define Hw11		0x00000800
#define Hw10		0x00000400
#define Hw9		0x00000200
#define Hw8		0x00000100
#define Hw7		0x00000080
#define Hw6		0x00000040
#define Hw5		0x00000020
#define Hw4		0x00000010
#define Hw3		0x00000008
#define Hw2		0x00000004
#define Hw1		0x00000002
#define Hw0		0x00000001
#define HwZERO		0x00000000

#define ENABLE		1
#define DISABLE		0

#define ON		1
#define OFF		0

#define FALSE		0
#define TRUE		1

#define BITSET(X, MASK)			((X) |= (unsigned int)(MASK))
#define BITSCLR(X, SMASK, CMASK)	((X) = ((((unsigned int)(X)) | ((unsigned int)(SMASK))) & ~((unsigned int)(CMASK))) )
#define BITCSET(X, CMASK, SMASK)	((X) = ((((unsigned int)(X)) & ~((unsigned int)(CMASK))) | ((unsigned int)(SMASK))) )
#define BITCLR(X, MASK)			((X) &= ~((unsigned int)(MASK)) )
#define BITXOR(X, MASK)			((X) ^= (unsigned int)(MASK) )
#define ISZERO(X, MASK)			(!(((unsigned int)(X))&((unsigned int)(MASK))))
#define	ISSET(X, MASK)			((unsigned long)(X)&((unsigned long)(MASK)))

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X)
#define IO_OFFSET       0xE1000000
#elif defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#define IO_OFFSET       0x81000000
#else
#endif
#define io_p2v(pa)      ((pa) + IO_OFFSET)
#define tcc_p2v(pa)     io_p2v(pa)

/* Original Code */ 
#define ECID0_OFFSET		(0x290)
#define ECID1_OFFSET		(0x294)
#define ECID2_OFFSET		(0x298)
#define ECID3_OFFSET		(0x29C)

#define GMACDLY0_OFFSET		(0x2000)
#define GMACDLY1_OFFSET		(0x2004)
#define GMACDLY2_OFFSET		(0x2008)
#define GMACDLY3_OFFSET		(0x200C)
#define GMACDLY4_OFFSET		(0x2010)
#define GMACDLY5_OFFSET		(0x2014)
#define GMACDLY6_OFFSET		(0x2018)

#define GMAC0_CFG0_OFFSET	(0x0060)
#define GMAC0_CFG1_OFFSET	(0x0064)
#define GMAC0_CFG2_OFFSET	(0x0054)
#define GMAC0_CFG3_OFFSET	(0x0058)
#define GMAC0_CFG4_OFFSET	(0x005C)

#define GMAC1_CFG0_OFFSET	(0x0068)
#define GMAC1_CFG1_OFFSET	(0x006C)
#define GMAC1_CFG2_OFFSET	(0x0070)
#define GMAC1_CFG3_OFFSET	(0x0074)
#define GMAC1_CFG4_OFFSET	(0x0078)

// GMAC CFG0 
#define CFG0_TR					((unsigned)1<<(unsigned)31)
#define CFG0_TXDIV_SHIFT		(19)
#define CFG0_TXDIV_MASK			((unsigned)0x7f<<(unsigned)CFG0_TXDIV_SHIFT)
#define CFG0_TXCLK_SEL_SHIFT	(16)
#define CFG0_TXCLK_SEL_MASK		((unsigned)0x3<<(unsigned)CFG0_TXCLK_SEL_SHIFT)
#define CFG0_RR					((unsigned)1<<(unsigned)15)
#define CFG0_RXDIV_SHIFT		(4)
#define CFG0_RXDIV_MASK			((unsigned)0x3f<<(unsigned)CFG0_RXDIV_SHIFT)
#define CFG0_RXCLK_SEL_SHIFT	(0)
#define CFG0_RXCLK_SEL_MASK		((unsigned)0x3<<(unsigned)CFG0_RXCLK_SEL_SHIFT)

// GMAC CFG1 
#define CFG1_CE					((unsigned)1<<(unsigned)31)
#define CFG1_CC					((unsigned)1<<(unsigned)30)
#define CFG1_PHY_INFSEL_SHIFT	(20)
#define CFG1_PHY_INFSEL_MASK	((unsigned int)((unsigned)0x7<<(unsigned)CFG1_PHY_INFSEL_SHIFT))
#define CFG1_FCTRL				((unsigned)1<<(unsigned)17)
#define CFG1_FCTRL_SHIFT				(17)
#define CFG1_FCTRL_MASK				(0x7<<CFG1_FCTRL_SHIFT)
#define CFG1_TCO				((unsigned)1<<(unsigned)16)

// GMAC CFG2
#define GPI_SHIFT				(0)
#define GPO_SHIFT				(4)
#define PTP_AUX_TS_TRIG_SHIFT	(8)
#define PTP_PPS_TRIG_SHIFT		(12)


struct gmac_dt_info_t {
	unsigned index;
	phy_interface_t phy_inf;
	struct clk *hsio_clk;
	struct clk *gmac_clk;
	struct clk *ptp_clk;
	struct clk *gmac_hclk;
	unsigned phy_on;
	unsigned phy_rst;
	u32 txclk_i_dly;
	u32 txclk_i_inv;
	u32 txclk_o_dly;
	u32 txclk_o_inv;
	u32 txen_dly;
	u32 txer_dly;
	u32 txd0_dly;
	u32 txd1_dly;
	u32 txd2_dly;
	u32 txd3_dly;
	u32 txd4_dly;
	u32 txd5_dly;
	u32 txd6_dly;
	u32 txd7_dly;
	u32 rxclk_i_dly;
	u32 rxclk_i_inv;
	u32 rxdv_dly;
	u32 rxer_dly;
	u32 rxd0_dly;
	u32 rxd1_dly;
	u32 rxd2_dly;
	u32 rxd3_dly;
	u32 rxd4_dly;
	u32 rxd5_dly;
	u32 rxd6_dly;
	u32 rxd7_dly;
	u32 crs_dly;
	u32 col_dly;
};

int dwmac_tcc_init(struct device_node *np, struct gmac_dt_info_t *dt_info);
// void tca_gmac_clk_enable(struct gmac_dt_info_t *dt_info);
void dwmac_tcc_clk_enable(struct plat_stmmacenet_data *plat);
void tca_gmac_clk_disable(struct gmac_dt_info_t *dt_info);
unsigned long tca_gmac_get_hsio_clk(struct gmac_dt_info_t *dt_info);
phy_interface_t tca_gmac_get_phy_interface(struct gmac_dt_info_t *dt_info);
void tca_gmac_phy_pwr_on(struct gmac_dt_info_t *dt_info);
void tca_gmac_phy_pwr_off(struct gmac_dt_info_t *dt_info);
void dwmac_tcc_phy_reset(struct gmac_dt_info_t *dt_info);
void dwmac_tcc_tunning_timing(struct gmac_dt_info_t *dt_info, void __iomem *ioaddr);
void dwmac_tcc_portinit(struct gmac_dt_info_t *dt_info, void __iomem *ioaddr);
void IO_UTIL_ReadECID (unsigned ecid[]);
int tca_get_mac_addr_from_ecid(unsigned char *mac_addr);

#endif /*_TCA_GMAC_H_*/
