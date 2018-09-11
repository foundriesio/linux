/*
 * Copyright (c) 2011 Telechips, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
	Not finished block "GMAC"
*/

#ifndef _STRUCTURES_HSIO_H_
#define _STRUCTURES_HSIO_H_


/*******************************************************************************
*
*    TCC893x DataSheet PART 6 HSIO BUS
*
********************************************************************************/

/************************************************************************
*   3. Ethernet MAC (GMAC)                       (Base Addr = 0x71701000)
*************************************************************************/

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

/************************************************************************
*   4. USB 2.0 Host                  (Base Addr = 0x71200000, 0x71300000)
*************************************************************************/

typedef struct _USB_EHCI {
    volatile TCC_DEF32BIT_TYPE  HCCAPBASE;          // 0x000  R    0x01000010   // Capability Register
    volatile TCC_DEF32BIT_TYPE  HCSPARAMS;          // 0x004  R    0x00001116
    volatile TCC_DEF32BIT_TYPE  HCCPARAMS;          // 0x008  R/W  0x0000A010
    volatile TCC_DEF32BIT_TYPE  USBCMD;             // 0x010  R/W  0x00080000 or 0x00080B00 // Operational Registers
    volatile TCC_DEF32BIT_TYPE  USBSTS;             // 0x014  R/W  0x00001000
    volatile TCC_DEF32BIT_TYPE  USBINTR;            // 0x018  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  FRINDEX;            // 0x01C  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  CTRLDSSEGMENT;      // 0x020  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  PERIODICLISTBASE;   // 0x024  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  ASYNCLISTADDR;      // 0x028  R/W  0x00000000
    unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  CONFIGFLAG;         // 0x050  R/W  0x00000000   // Auxiliary Power Well Registers
    volatile TCC_DEF32BIT_TYPE  PORTSC[15];         // 0x054~ R/W  0x00002000
    volatile TCC_DEF32BIT_TYPE  INSNREG[6];         // 0x090~ R/W  0x00000000, 0x00100010, 0x00000100,  // Synopsys-Specific Registers
} USB_EHCI, *PUSB_EHCI;

typedef struct _USB_OHCI {
    volatile TCC_DEF32BIT_TYPE  HcRevision;         // 0x000  R    0x00000010   // Control and status registers
    volatile TCC_DEF32BIT_TYPE  HcControl;          // 0x004  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcCommandStatus;    // 0x008  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcInterruptStatus;  // 0x00C  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcInterruptEnable;  // 0x010  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcInterruptDisable; // 0x014  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcHCCA;             // 0x018  R/W  0x00000000   // Memory pointer registers
    volatile TCC_DEF32BIT_TYPE  HcPeriodCurrentED;  // 0x01C  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcControlHeadED;    // 0x020  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcControlCurrentED; // 0x024  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcBulkHeadED;       // 0x028  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcBulkCurrentED;    // 0x02C  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcDoneHead;         // 0x030  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcRminterval;       // 0x034  R/W  0x00002EDF   // Frame counter registers
    volatile TCC_DEF32BIT_TYPE  HcFmRemaining;      // 0x038  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcFmNumber;         // 0x03C  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcPeriodStart;      // 0x040  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcLSThreshold;      // 0x044  R/W  0x00000628
    volatile TCC_DEF32BIT_TYPE  HcRhDescriptorA;    // 0x048  R/W  0x02001202   // Root hub registers
    volatile TCC_DEF32BIT_TYPE  HcRhDescriptorB;    // 0x04C  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcRhStatus;         // 0x050  R/W  0x00000000
    volatile TCC_DEF32BIT_TYPE  HcRhPortStatus1;    // 0x054  R/W  0x00000100
    volatile TCC_DEF32BIT_TYPE  HcRhPortStatus2;    // 0x058  R/W  0x00000100
} USB_OHCI, *PUSB_OHCI;

#if 1
/************************************************************************
*   4.5. USB 3.0 Host 
*************************************************************************/
typedef struct _USB3_XHCI {
    volatile TCC_DEF32BIT_TYPE  CAPLENGTH;			// 0x0000
    volatile TCC_DEF32BIT_TYPE  HCSPARAMS1;			// 0x0004
    volatile TCC_DEF32BIT_TYPE  HCSPARAMS2;			// 0x0008
    volatile TCC_DEF32BIT_TYPE  HCSPARAMS3;			// 0x000C
    volatile TCC_DEF32BIT_TYPE  HCCPARAMS;			// 0x0010
    volatile TCC_DEF32BIT_TYPE  DBOFF;				// 0x0014
    volatile TCC_DEF32BIT_TYPE  RTSOFF;				// 0x0018
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  USBCMD;				// 0x0020
    volatile TCC_DEF32BIT_TYPE  USBSTS;				// 0x0024
    volatile TCC_DEF32BIT_TYPE  USB_PAGE_SIZE;		// 0x0028
    unsigned :32;
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  DNCTRL;				// 0x0034
    volatile TCC_DEF32BIT_TYPE  CRCR1;				// 0x0038
    volatile TCC_DEF32BIT_TYPE  CRCR2;				// 0x003C
    volatile TCC_DEF32BIT_TYPE  DCBAAP1;				// 0x0050
    volatile TCC_DEF32BIT_TYPE  DCBAAP2;				// 0x0054
    volatile TCC_DEF32BIT_TYPE  CONFIG;				// 0x0058
    volatile TCC_DEF32BIT_TYPE  reserved5C_41C[241];
    volatile TCC_DEF32BIT_TYPE  PORTSC1;				// 0x0420
    volatile TCC_DEF32BIT_TYPE  PORTPMSC_20;   		// 0x0424
    volatile TCC_DEF32BIT_TYPE  PORTLI1;				// 0x0428
    volatile TCC_DEF32BIT_TYPE  PORTHLPMC_20; 		// 0x042C
    volatile TCC_DEF32BIT_TYPE  PORTSC2;				// 0x0430
    volatile TCC_DEF32BIT_TYPE  PORTPMSC_SS; 		// 0x0434
    volatile TCC_DEF32BIT_TYPE  PORTLI2;				// 0x0438
    volatile TCC_DEF32BIT_TYPE  PORTHLPMC_SS;  		// 0x043C 
    volatile TCC_DEF32BIT_TYPE  MFINDEX;			// 0x0440
    unsigned :32;
    unsigned :32;
    unsigned :32;
    unsigned :32;
    unsigned :32;
    unsigned :32;
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  IMAN;				// 0x0460
    volatile TCC_DEF32BIT_TYPE  IMOD;				// 0x0464
    volatile TCC_DEF32BIT_TYPE  ERSTSZ;				// 0x0468
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  ERSTBA1;				// 0x0470
    volatile TCC_DEF32BIT_TYPE  ERSTBA2;				// 0x0474
    volatile TCC_DEF32BIT_TYPE  ERDP1;				// 0x0478
    volatile TCC_DEF32BIT_TYPE  ERDP2;				// 0x047C
    volatile TCC_DEF32BIT_TYPE  DB;					// 0x0480
} USB3_XHCI, *PUSB3_XHCI;

typedef struct _USB3_GLB {
    volatile TCC_DEF32BIT_TYPE  GSBUSCFG0;  		//     0xC100      0x00000001   Global SoC Bus Configuration Register 0
    volatile TCC_DEF32BIT_TYPE  GSBUSCFG1;  		//     0xC104      0x00000300   Global SoC Bus Configuration Register 1
    volatile TCC_DEF32BIT_TYPE  GTXTHRCFG;  		//     0xC108 0x0          Global Tx Threshold Control Register
    volatile TCC_DEF32BIT_TYPE  GRXTHRCFG;  		//     0xC10C 0x0          Global Rx Threshold Control Register
    volatile TCC_DEF32BIT_TYPE  GCTL; 				//		0xC110      0x30C12030  Global Core Control Register
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  GSTS; 				//		0xC118      0x3E800002   Global Status Register
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  GSNPSID ; 			//		0xC120      0x5533202A   Global Synopsys ID Register
    volatile TCC_DEF32BIT_TYPE  GGPIO; 				//	 	0xC124 0x0          Global General Purpose I/O Register
    volatile TCC_DEF32BIT_TYPE  GUID; 				//		0xC128      0x12345678   Global User ID Register
    volatile TCC_DEF32BIT_TYPE  GUCTL; 				//		0xC12C     0x7FC08010  Global User Control Register
    volatile TCC_DEF32BIT_TYPE  GBUSERRADDR;  		//     0xC130 0x0          Global SoC Bus Error Address Register
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  GPRTBIMAP;			//      0xC138 0x0          Global Port - SS USB Instance Mapping Register
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS0;			//    0xC140      0x2020400A  Global Hardware Parameters Register 0
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS1;			//    0xC144      0x0060C93B  Global Hardware Parameters Register 1
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS2;			//    0xC148      0x12345678   Global Hardware Parameters Register 2
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS3;			//    0xC14C      0x04108085   Global Hardware Parameters Register 3
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS4;			//    0xC150      0x48822004   Global Hardware Parameters Register 4
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS5;			//    0xC154      0x04202088   Global Hardware Parameters Register 5
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS6;			//    0xC158      0x07888020   Global Hardware Parameters Register 6
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS7; 		//    0xC15C     0x030804CE  Global Hardware Parameters Register 7
    volatile TCC_DEF32BIT_TYPE  GDBGFIFOSPACE; 		//    0xC160     
    volatile TCC_DEF32BIT_TYPE  GDBGLTSSM; 			//    0xC164     
    volatile TCC_DEF32BIT_TYPE  GDBGLNMCC; 			//    0xC168     
    volatile TCC_DEF32BIT_TYPE  GDBGBMU; 			//    0xC16C     
    volatile TCC_DEF32BIT_TYPE  GDBGLSPMUX; 		//    0xC170    
    volatile TCC_DEF32BIT_TYPE  GDBGLSP; 			//    0xC174    
    volatile TCC_DEF32BIT_TYPE  GDBGEPINFO0; 		//    0xC178    
    volatile TCC_DEF32BIT_TYPE  GDBGEPINFO1; 		//    0xC17c 
    volatile TCC_DEF32BIT_TYPE  GPRTBIMAP_HS;      	//	  0xC180 0x0          Global Port - HS USB Instance Mapping Register
    unsigned :32;
	volatile TCC_DEF32BIT_TYPE  GPRTBIMAP_FS;       //	  0xC188 0x0          Global Port - FS USB Instance Mapping Register
	volatile TCC_DEF32BIT_TYPE  reservedC18C[29];			
    volatile TCC_DEF32BIT_TYPE  GUSB2PHYCFG;		//    0xC200      0x00002400   Global USB2 PHY Configuration Register
    volatile TCC_DEF32BIT_TYPE 	reservedC204[47];
	volatile TCC_DEF32BIT_TYPE  GUSB3PIPECTL;		//	  0xC2C0
    volatile TCC_DEF32BIT_TYPE 	reservedC2C4[15];
    volatile TCC_DEF32BIT_TYPE  GTXFIFOSIZ_0;     	//  0xC300      0x00000042   Global Transmit FIFO Size Register 0
    volatile TCC_DEF32BIT_TYPE  GTXFIFOSIZ_1;    	//  0xC304      0x00420184   Global Transmit FIFO Size Register 1
    volatile TCC_DEF32BIT_TYPE  GTXFIFOSIZ_2;     	//  0xC308      0x01C60184  Global Transmit FIFO Size Register 2
    volatile TCC_DEF32BIT_TYPE  GTXFIFOSIZ_3;     	// 	0xC30C      0x034A0184  Global Transmit FIFO Size Register 3
    volatile TCC_DEF32BIT_TYPE 	reservedC310[28];
    volatile TCC_DEF32BIT_TYPE  GRXFIFOSIZ_0;     	//	0xC380      0x00000185   Global Receive FIFO Size Register 0
    volatile TCC_DEF32BIT_TYPE  GRXFIFOSIZ_1;     	//	0xC384      0x01850000   Global Receive FIFO Size Register 1
    volatile TCC_DEF32BIT_TYPE  GRXFIFOSIZ_2;     	// 	0xC38C      0x01850000   Global Receive FIFO Size Register 2
    volatile TCC_DEF32BIT_TYPE  GEVNTADR_L;     	//	0xC400 0x0          Global Event Buffer Address Register L
    volatile TCC_DEF32BIT_TYPE  GEVNTADR_H;         //	0xC404 0x0          Global Event Buffer Address Register H
    volatile TCC_DEF32BIT_TYPE  GEVNTSIZ;			//	0xC408 0x0          Global Event Buffer Size Register
    volatile TCC_DEF32BIT_TYPE  GEVNTCOUNT;        	//	0xC40C 0x0          Global Event Buffer Count Register
    volatile TCC_DEF32BIT_TYPE 	reservedC410[124];
    volatile TCC_DEF32BIT_TYPE  GHWPARAMS8;        	//	0xC600      0x00000708   Global Hardware Parameters Register 8
}USB3_GLB, *PUSB3_GLB;
#endif
/************************************************************************
*   5. SECURITY (Cipher)                         (Base Addr = 0x71EC0000)
*************************************************************************/

typedef struct {
    unsigned SELECT         :2;
    unsigned ENC            :1;
    unsigned PRT            :1;
    unsigned OPMODE         :3;
    unsigned DESMODE        :2;
    unsigned KEYLEN         :2;
    unsigned KEYLD          :1;
    unsigned IVLD           :1;
    unsigned BCLR           :1;
    unsigned RCLR           :1;
    unsigned WCLR           :1;
} CIPHER_CTRL_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_CTRL_IDX_TYPE    bREG;
} CIPHER_CTRL_TYPE;

typedef struct {
    unsigned ADDR           :32;
} CIPHER_BASE_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_BASE_IDX_TYPE    bREG;
} CIPHER_BASE_TYPE;

typedef struct {
    unsigned SIZE           :13;
    unsigned                :3;
    unsigned COUNT          :13;
    unsigned                :3;
} CIPHER_PACKET_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_PACKET_IDX_TYPE  bREG;
} CIPHER_PACKET_TYPE;

typedef struct {
    unsigned EN             :1;
    unsigned                :1;
    unsigned PCLK           :1;
    unsigned                :11;
    unsigned RXAM           :2;
    unsigned TXAM           :2;
    unsigned                :10;
    unsigned END            :1;
    unsigned                :2;
    unsigned DE             :1;
} CIPHER_DMACTR_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_DMACTR_IDX_TYPE  bREG;
} CIPHER_DMACTR_TYPE;

typedef struct {
    unsigned TXCNT          :13;
    unsigned                :3;
    unsigned RXCNT          :13;
    unsigned                :3;
} CIPHER_DMASTS_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_DMASTS_IDX_TYPE  bREG;
} CIPHER_DMASTS_TYPE;

typedef struct {
    unsigned IRQPCNT        :13;
    unsigned                :3;
    unsigned IEP            :1;
    unsigned IED            :1;
    unsigned                :2;
    unsigned IRQS           :1;
    unsigned                :7;
    unsigned ISP            :1;
    unsigned ISD            :1;
    unsigned                :2;
} CIPHER_IRQCTR_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_IRQCTR_IDX_TYPE  bREG;
} CIPHER_IRQCTR_TYPE;

typedef struct {
    unsigned BLKNUM         :32;
} CIPHER_BLKNUM_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_BLKNUM_IDX_TYPE  bREG;
} CIPHER_BLKNUM_TYPE;

typedef struct {
    unsigned ROUND          :16;
    unsigned                :16;
} CIPHER_ROUND_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_ROUND_IDX_TYPE   bREG;
} CIPHER_ROUND_TYPE;

typedef struct {
    unsigned KEY            :32;
} CIPHER_KEY_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_KEY_IDX_TYPE     bREG;
} CIPHER_KEY_TYPE;

typedef struct {
    unsigned IV             :32;
} CIPHER_IV_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    CIPHER_IV_IDX_TYPE      bREG;
} CIPHER_IV_TYPE;

typedef struct _CIPHER{
    volatile CIPHER_CTRL_TYPE   CTRL;           // 0x000  R/W  0x00000000   Cipher control regsiter
    volatile CIPHER_BASE_TYPE   TXBASE;         // 0x004  R/W  0x00000000   TX base address register
    volatile CIPHER_BASE_TYPE   RXBASE;         // 0x008  R/W  0x00000000   RX base address register
    volatile CIPHER_PACKET_TYPE PACKET;         // 0x00C  R/W  0x00000000   Packet register
    volatile CIPHER_DMACTR_TYPE DMACTR;         // 0x010  R/W  0x00000000   DMA control register
    volatile CIPHER_DMASTS_TYPE DMASTR;         // 0x014  RO   -            DMA status register
    volatile CIPHER_IRQCTR_TYPE IRQCTR;         // 0x018  R/W  0x00000000   Interrupt control register
    volatile CIPHER_BLKNUM_TYPE BLKNUM;         // 0x01C  RO   -            Block count register
    volatile CIPHER_ROUND_TYPE  ROUND;          // 0x020  R/W  0x00000000   Round register
    unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    volatile CIPHER_KEY_TYPE    KEY0;           // 0x040  R/W  0x00000000   Key0 register
    volatile CIPHER_KEY_TYPE    KEY1;           // 0x044  R/W  0x00000000   Key1 register
    volatile CIPHER_KEY_TYPE    KEY2;           // 0x048  R/W  0x00000000   Key2 register
    volatile CIPHER_KEY_TYPE    KEY3;           // 0x04C  R/W  0x00000000   Key3 register
    volatile CIPHER_KEY_TYPE    KEY4;           // 0x050  R/W  0x00000000   Key4 register
    volatile CIPHER_KEY_TYPE    KEY5;           // 0x054  R/W  0x00000000   Key5 register
    volatile CIPHER_KEY_TYPE    KEY6;           // 0x058  R/W  0x00000000   Key6 register
    volatile CIPHER_KEY_TYPE    KEY7;           // 0x05C  R/W  0x00000000   Key7 register
#if !defined(CONFIG_CHIP_TCC8935S) && !defined(CONFIG_CHIP_TCC8933S) && !defined(CONFIG_CHIP_TCC8937S)
    volatile CIPHER_KEY_TYPE    KEY8;           // 0x60  R/W  0x00000000 	Key8 register
    volatile CIPHER_KEY_TYPE    KEY9;           // 0x64  R/W  0x00000000 	Key9 register
    volatile CIPHER_IV_TYPE     IV0;            // 0x068  R/W  0x00000000   Initial vector0 register
    volatile CIPHER_IV_TYPE     IV1;            // 0x06C  R/W  0x00000000   Initial vector1 register
    volatile CIPHER_IV_TYPE     IV2;            // 0x070  R/W  0x00000000   Initial vector2 register
    volatile CIPHER_IV_TYPE     IV3;            // 0x074  R/W  0x00000000   Initial vector3 register
#else
    volatile CIPHER_IV_TYPE     IV0;            // 0x060  R/W  0x00000000   Initial vector0 register
    volatile CIPHER_IV_TYPE     IV1;            // 0x064  R/W  0x00000000   Initial vector1 register
    volatile CIPHER_IV_TYPE     IV2;            // 0x068  R/W  0x00000000   Initial vector2 register
    volatile CIPHER_IV_TYPE     IV3;            // 0x06c  R/W  0x00000000   Initial vector3 register
#endif
} CIPHER, *PCIPHER;


/************************************************************************
*   6. DMAX                                      (Base Addr = 0x71EB0000)
*************************************************************************/

typedef struct {
    unsigned EN             :1;
    unsigned TYPE           :1;
    unsigned                :6;
    unsigned ROPT           :1;
    unsigned RTYPE          :1;
    unsigned                :5;
    unsigned WB             :1;
    unsigned REN            :1;
    unsigned RPAUSE         :3;
    unsigned RWAIT          :3;
    unsigned IEN            :1;
    unsigned CH             :5;
    unsigned                :3;
} DMAX_CTRL_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    DMAX_CTRL_IDX_TYPE      bREG;
} DMAX_CTRL_TYPE;

typedef struct {
    unsigned ADDR           :32;
} DMAX_BASE_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    DMAX_BASE_IDX_TYPE      bREG;
} DMAX_BASE_TYPE;

typedef struct {
    unsigned SIZE           :24;
    unsigned                :7;
    unsigned CIPHER	    :1;
} DMAX_SIZE_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    DMAX_SIZE_IDX_TYPE      bREG;
} DMAX_SIZE_TYPE;

typedef struct {
    unsigned NEMPTY         :8;
    unsigned                :21;
    unsigned IDLE           :1;
    unsigned EMP            :1;
    unsigned FULL           :1;
} DMAX_FIFO0_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    DMAX_FIFO0_IDX_TYPE     bREG;
} DMAX_FIFO0_TYPE;

typedef struct {
    unsigned FIFO           :32;
} DMAX_FIFO_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    DMAX_FIFO_IDX_TYPE      bREG;
} DMAX_FIFO_TYPE;

typedef struct {
    unsigned INT            :32;
} DMAX_INT_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    DMAX_INT_IDX_TYPE       bREG;
} DMAX_INT_TYPE;

typedef struct _DMAX{
    volatile DMAX_CTRL_TYPE     CTRL;           // 0x000  R/W  0x00000000   Control Register
    volatile DMAX_BASE_TYPE     SBASE;          // 0x004  R/W  0x00000000   Source Base Address Register
    volatile DMAX_BASE_TYPE     DBASE;          // 0x008  R/W  0x00000000   Destination Base Address Register
    volatile DMAX_SIZE_TYPE     SIZE;           // 0x00C  R/W  0x00000000   Transfer Byte Size Register
    volatile DMAX_FIFO0_TYPE    FIFO0;          // 0x010  R/W  0x00000000   FIFO Status 0 Register
    volatile DMAX_FIFO_TYPE     FIFO1;          // 0x014  R/W  0x00000000   FIFO Status 1 Register
    volatile DMAX_FIFO_TYPE     FIFO2;          // 0x018  R/W  0x00000000   FIFO Status 2 Register
    volatile DMAX_FIFO_TYPE     FIFO3;          // 0x01C  R/W  0x00000000   FIFO Status 3 Register
    volatile DMAX_INT_TYPE      INTMSK;         // 0x020  R/W  0x00000000   Interrupt Mask Register
    volatile DMAX_INT_TYPE      INTSTS;         // 0x024  R/W  0x00000000   Interrupt Status Register
} DMAX, *PDMAX;


/************************************************************************
*   7. HSIO BUS Configuration Registers          (Base Addr = 0x71EA0000)
*************************************************************************/

typedef struct {
    unsigned USB20OTG       :1;
    unsigned GMAC_G         :1;
    unsigned GMAC           :1;
    unsigned                :3;
    unsigned US20H          :1;
    unsigned                :5;
    unsigned SEC_CTRL       :1;
    unsigned                :3;
    unsigned HSOCFG         :1;
    unsigned                :1;
    unsigned CIPHER         :1;
    unsigned                :13;
} HSIO_HCLK_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    HSIO_HCLK_IDX_TYPE      bREG;
} HSIO_HCLK_TYPE;

typedef struct {
    unsigned FGS            :2;
    unsigned FG             :2;
    unsigned                :28;
} USB20H_BCFG_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    USB20H_BCFG_IDX_TYPE    bREG;
} USB20H_BCFG_TYPE;

typedef struct {
    unsigned FSEL           :3;
    unsigned                :1;
    unsigned RCS            :2;
    unsigned                :14;
    unsigned PHYVLD_EN      :1;
    unsigned PHYVLD         :1;
    unsigned                :2;
    unsigned SIDDQ          :1;
    unsigned                :6;
    unsigned POR        	:1;
} USB20H_PCFG0_IDX_TYPE;

typedef union {
    unsigned long            nREG;
    USB20H_PCFG0_IDX_TYPE    bREG;
} USB20H_PCFG0_TYPE;

typedef struct {
    unsigned TXVRT			:4;
    unsigned CDT 			:3;
    unsigned TXPT			:1;
    unsigned TXAT			:2;
    unsigned TXRISET		:2;
    unsigned TXREST			:2;
    unsigned TXHSXVT		:2;
    unsigned OTGT			:3;
    unsigned 				:1;
    unsigned SQRXT			:4;
    unsigned TXFSLST		:4;
    unsigned 				:4;
} USB20H_PCFG1_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    USB20H_PCFG1_IDX_TYPE   bREG;
} USB20H_PCFG1_TYPE;

typedef struct {
    unsigned UARTEN			:1;
    unsigned UARTTXDP		:1;
    unsigned 				:14;
    unsigned DCDEN			:1;
    unsigned VDATDETEN		:1;
    unsigned VDATSRCEN		:1;
    unsigned CHRGSEL		:1;
    unsigned CHGDET			:1;
    unsigned 				:3;
    unsigned ACAEN			:1;
    unsigned RIDC			:1;
    unsigned RIDB			:1;
    unsigned RIDA			:1;
    unsigned RIDFLOAT		:1;
    unsigned RIDGND			:1;
    unsigned 				:2;
} USB20H_PCFG2_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    USB20H_PCFG2_IDX_TYPE   bREG;
} USB20H_PCFG2_TYPE;

typedef struct {
    unsigned UTMI_SWCTRL	:1;
    unsigned XCVRSEL		:2;
    unsigned OPMODE			:2;
    unsigned TERMSEL		:1;
    unsigned DMPD			:1;
    unsigned DPPD			:1;
    unsigned DISC			:1;
    unsigned DISC_EN		:1;
    unsigned LINESTATE		:2;
    unsigned TDOUT			:4;
    unsigned TDI			:8;
    unsigned TAD			:4;
    unsigned TCK			:1;
    unsigned TDOSEL			:1;
    unsigned 				:2;
} USB20H_PCFG3_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    USB20H_PCFG3_IDX_TYPE   bREG;
} USB20H_PCFG3_TYPE;

typedef struct {
    unsigned 				:26;;
    unsigned IRQ_VBUSOFF	:1;
    unsigned IRQ_VBUSON		:1;
    unsigned IRQ_PHYVALID	:1;
    unsigned IRQ_VBUSEN 	:1;
    unsigned IRQ_PHYVALIDEN	:1;
    unsigned IRQ_CLR		:1;
} USB20H_PCFG4_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    USB20H_PCFG4_IDX_TYPE   bREG;
} USB20H_PCFG4_TYPE;

typedef struct {
   unsigned 		        :7;
    unsigned PRT_PWR        :1;
    unsigned                :24;
} USB20H_STS_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    USB20H_STS_IDX_TYPE     bREG;
} USB20H_STS_TYPE;

typedef struct {
    unsigned FLADJ_HOST     :6;
    unsigned                :4;
    unsigned FLADJ_PORT     :6;
    unsigned                :16;
} USB20H_LCFG0_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    USB20H_LCFG0_IDX_TYPE   bREG;
} USB20H_LCFG0_TYPE;

#ifdef CONFIG_ARCH_TCC897X		/* 015.05.29 */
/*******************************************************************************
 *   14. USB 2.0 OTG Controller                          (Base Addr = 0x76A00000)
 ********************************************************************************/

typedef struct _USB20OTG{
	// Core Global CSR Map
	volatile unsigned int       GOTGCTL;            // 0x000  R/W   OTG Control and Status Register
	volatile unsigned int       GOTGINT;            // 0x004        OTG Interrupt Register
	volatile unsigned int       GAHBCFG;            // 0x008        Core AHB Configuration Register
	volatile unsigned int       GUSBCFG;            // 0x00C        Core USB Configuration register
	volatile unsigned int       GRSTCTL;            // 0x010        Core Reset Register
	volatile unsigned int       GINTSTS;            // 0x014        Core Interrupt Register
	volatile unsigned int       GINTMSK;            // 0x018        Core Interrupt Mask Register
	volatile unsigned int       GRXSTSR;            // 0x01C        Receive Status Debug Read register(Read Only)
	volatile unsigned int       GRXSTSP;            // 0x020        Receive Status Read /Pop register(Read Only)
	volatile unsigned int       GRXFSIZ;            // 0x024        Receive FIFO Size Register
	volatile unsigned int       GNPTXFSIZ;          // 0x028        Non-periodic Transmit FIFO Size register
	volatile unsigned int       GNPTXSTS;           // 0x02C        Non-periodic Transmit FIFO/Queue Status register (Read Only)
	volatile unsigned int       NOTDEFINE0[3];      // 0x030~       Reserved
	volatile unsigned int       GUID;               // 0x03C        User ID Register
	volatile unsigned int       NOTDEFINE1;         // 0x040        Reserved
	volatile unsigned int       GHWCFG1;            // 0x044        User HW Config1 Register(Read Only)
	volatile unsigned int       GHWCFG2;            // 0x048        User HW Config2 Register(Read Only)
	volatile unsigned int       GHWCFG3;            // 0x04C        User HW Config3 Register(Read Only)
	volatile unsigned int       GHWCFG4;            // 0x050        User HW Config4 Register(Read Only)
	volatile unsigned int       NOTDEFINE2[43];     // 0x054~       Reserved
	volatile unsigned int       HPTXFSIZ;           // 0x100        Host Periodic Transmit FIFO Size Register
	volatile unsigned int       DIEPTXFn[15];       // 0x104~       Device IN Endpoint Transmit FIFO Size register
	volatile unsigned int       NOTDEFINE3[176];    // 0x140~       Reserved
	// Host Mode CSR Map
	volatile unsigned int       HCFG;               // 0x400        Host Configuration Register
	volatile unsigned int       HFIR;               // 0x404        Host Frame Interval Register
	volatile unsigned int       HFNUM;              // 0x408        Host Frame Number/Frame Time Remaining register
	volatile unsigned int       NOTDEFINE4;         // 0x40C        Reserved
	volatile unsigned int       HPTXSTS;            // 0x410        Host Periodic Transmit FIFO/Queue Status Register
	volatile unsigned int       HAINT;              // 0x414        Host All Channels Interrupt Register
	volatile unsigned int       HAINTMSK;           // 0x418        Host All Channels Interrupt Mask register
	volatile unsigned int       NOTDEFINE5[9];      // 0x41C~       Not Used
	volatile unsigned int       HPRT;               // 0x440        Host Port Control and Status register
	volatile unsigned int       NOTDEFINE6[47];     // 0x444~       Reserved
	volatile unsigned int       HCCHARn;            // 0x500        Host Channel 0 Characteristics Register
	volatile unsigned int       HCSPLTn;            // 0x504        Host Channel 0 Split Control Register
	volatile unsigned int       HCINTn;             // 0x508        Host Channel 0 Interrupt Register
	volatile unsigned int       HCINTMSKn;          // 0x50C        Host Channel 0 Interrupt Mask Register
	volatile unsigned int       HCTSIZn;            // 0x510        Host Channel 0 Transfer Size Register
	volatile unsigned int       HCDMAn;             // 0x514        Host Channel 0 DMA Address Register
    volatile unsigned int       NOTDEFINE7[2];      // 0x518~       Reserved
    volatile unsigned int       HCH1[8];            // 0x520~       Host Channel 1 Registers
    volatile unsigned int       HCH2[8];            // 0x540~       Host Channel 2 Registers
    volatile unsigned int       HCH3[8];            // 0x560~       Host Channel 3 Registers
    volatile unsigned int       HCH4[8];            // 0x580~       Host Channel 4 Registers
    volatile unsigned int       HCH5[8];            // 0x5A0~       Host Channel 5 Registers
    volatile unsigned int       HCH6[8];            // 0x5C0~       Host Channel 6 Registers
    volatile unsigned int       HCH7[8];            // 0x5E0~       Host Channel 7 Registers
    volatile unsigned int       HCH8[8];            // 0x600~       Host Channel 8 Registers
    volatile unsigned int       HCH9[8];            // 0x620~       Host Channel 9 Registers
    volatile unsigned int       HCH10[8];           // 0x640~       Host Channel 10 Registers
    volatile unsigned int       HCH11[8];           // 0x660~       Host Channel 11 Registers
    volatile unsigned int       HCH12[8];           // 0x680~       Host Channel 12 Registers
    volatile unsigned int       HCH13[8];           // 0x6A0~       Host Channel 13 Registers
    volatile unsigned int       HCH14[8];           // 0x6C0~       Host Channel 14 Registers
    volatile unsigned int       HCH15[8];           // 0x6E0~       Host Channel 15 Registers
    volatile unsigned int       NOTDEFINE8[64];     // 0x700~       Reserved
// Device Mode CSR Map
    volatile unsigned int       DCFG;               // 0x800        Device Configuration Register
    volatile unsigned int       DCTL;               // 0x804        Device Control Register
    volatile unsigned int       DSTS;               // 0x808        Device Status Register (Read Only)
    volatile unsigned int       NOTDEFINE9;         // 0x80C        Reserved
    volatile unsigned int       DIEPMSK;            // 0x810        Device IN Endpoint Common Interrupt Mask Register
    volatile unsigned int       DOEPMSK;            // 0x814        Device OUT Endpoint Common Interrupt Mask register
    volatile unsigned int       DAINT;              // 0x818        Device All Endpoints Interrupt Register
    volatile unsigned int       DAINTMSK;           // 0x81C        Device All Endpoints Interrupt Mask Register
    volatile unsigned int       NOTDEFINE10[2];     // 0x820~       Reserved
    volatile unsigned int       DVBUSDIS;           // 0x828        Device VBUS Discharge Time Register
    volatile unsigned int       DVBUSPULSE;         // 0x82C        Device VBUS Pulsing Time register
    volatile unsigned int       DTHRCTL;            // 0x830        Device Threshold Control register
    volatile unsigned int       DIEPEMPMSK;         // 0x834        Device IN Endpoint FIFO Empty Interrupt Mask register
    volatile unsigned int       NOTDEFINE11[50];    // 0x838~       Reserved

    volatile unsigned int       DIEPCTL0;           // 0x900        Device Control IN Endpoint 0 Control Register
    volatile unsigned int       NOTDEFINE12;        // 0x904        Reserved
    volatile unsigned int       DIEPINT0;           // 0x908        Device IN Endpoint 0 Interrupt Register
    volatile unsigned int       NOTDEFINE13;        // 0x90C        Reserved
    volatile unsigned int       DIEPTSIZ0;          // 0x910        Device IN Endpoint 0 Transfer Size register
    volatile unsigned int       DIEPDMA0;           // 0x914        Device IN Endpoint 0 DMA Address Register
    volatile unsigned int       DTXFSTS0;           // 0x918        Device IN Endpoint Transmit FIFO Status Register
    volatile unsigned int       NOTDEFINE14;        // 0x91C        Reserved

    volatile unsigned int       DEVINENDPT[15][8];  // 0x920~       Device IN Endpoint 1~15 Registers
    volatile unsigned int       DOEPCTL0;           // 0xB00        Device Control OUT Endpoint 0 Control register
    volatile unsigned int       NOTDEFINE15;        // 0xB04        Reserved
    volatile unsigned int       DOEPINT0;           // 0xB08        Device OUT Endpoint 0 Interrupt Register
    volatile unsigned int       NOTDEFINE16;        // 0xB0C        Reserved
    volatile unsigned int       DOEPTSIZ0;          // 0xB10        Device OUT Endpoint 0 Transfer Size Register
    volatile unsigned int       DOEPDMA0;           // 0xB14        Device OUT Endpoint 0 DMA Address register
    volatile unsigned int       NOTDEFINE17[2];     // 0xB18~       Reserved
    volatile unsigned int       DEVOUTENDPT[15][8]; // 0xB20~       Device OUT Endpoint 1~15 Registers
    volatile unsigned int       NOTDEFINE18[64];    // 0xD00~       Reserved
    // Power and Clock Gating CSR Map
    volatile unsigned int       PCGCR;              // 0xE00        Power and Clock Gating Control Register
    volatile unsigned int       NOTDEFINE19[127];   // 0xE04~       Reserved
    // Data FIFO(DFIFO) Access Register Map
    volatile unsigned int       DFIFOENDPT[16][1024];   // 0x1000~      Device IN Endpoint 0~16/Host Out Channel 0~16: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT0[1024];  // 0x1000~      Device IN Endpoint 0/Host Out Channel 0: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT1[1024];  // 0x2000~      Device IN Endpoint 1/Host Out Channel 1: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT2[1024];  // 0x3000~      Device IN Endpoint 2/Host Out Channel 2: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT3[1024];  // 0x4000~      Device IN Endpoint 3/Host Out Channel 3: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT4[1024];  // 0x5000~      Device IN Endpoint 4/Host Out Channel 4: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT5[1024];  // 0x6000~      Device IN Endpoint 5/Host Out Channel 5: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT6[1024];  // 0x7000~      Device IN Endpoint 6/Host Out Channel 6: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT7[1024];  // 0x8000~      Device IN Endpoint 7/Host Out Channel 7: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT8[1024];  // 0x9000~      Device IN Endpoint 8/Host Out Channel 8: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT9[1024];  // 0xA000~      Device IN Endpoint 9/Host Out Channel 9: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT10[1024]; // 0xB000~      Device IN Endpoint 10/Host Out Channel 10: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT11[1024]; // 0xC000~      Device IN Endpoint 11/Host Out Channel 11: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT12[1024]; // 0xD000~      Device IN Endpoint 12/Host Out Channel 12: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT13[1024]; // 0xE000~      Device IN Endpoint 13/Host Out Channel 13: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT14[1024]; // 0xF000~      Device IN Endpoint 14/Host Out Channel 14: DFIFO Write/Read Access
    //volatile unsigned int       DFIFOENDPT15[1024]; // 0x10000~     Device IN Endpoint 15/Host Out Channel 15: DFIFO Write/Read Access
} USB20OTG, *PUSB20OTG;

typedef struct _USBOTG{
    // Core Global CSR Map
    volatile unsigned int       GOTGCTL;            // 0x000  R/W   OTG Control and Status Register
    volatile unsigned int       GOTGINT;            // 0x004        OTG Interrupt Register
    volatile unsigned int       GAHBCFG;            // 0x008        Core AHB Configuration Register
    volatile unsigned int       GUSBCFG;            // 0x00C        Core USB Configuration register
    volatile unsigned int       GRSTCTL;            // 0x010        Core Reset Register
    volatile unsigned int       GINTSTS;            // 0x014        Core Interrupt Register
    volatile unsigned int       GINTMSK;            // 0x018        Core Interrupt Mask Register
    volatile unsigned int       GRXSTSR;            // 0x01C        Receive Status Debug Read register(Read Only)
    volatile unsigned int       GRXSTSP;            // 0x020        Receive Status Read /Pop register(Read Only)
    volatile unsigned int       GRXFSIZ;            // 0x024        Receive FIFO Size Register
    volatile unsigned int       GNPTXFSIZ;          // 0x028        Non-periodic Transmit FIFO Size register
    volatile unsigned int       GNPTXSTS;           // 0x02C        Non-periodic Transmit FIFO/Queue Status register (Read Only)
    volatile unsigned int       NOTDEFINE0[3];      // 0x030~       Reserved
    volatile unsigned int       GUID;               // 0x03C        User ID Register
    volatile unsigned int       NOTDEFINE1;         // 0x040        Reserved
    volatile unsigned int       GHWCFG1;            // 0x044        User HW Config1 Register(Read Only)
    volatile unsigned int       GHWCFG2;            // 0x048        User HW Config2 Register(Read Only)
    volatile unsigned int       GHWCFG3;            // 0x04C        User HW Config3 Register(Read Only)
    volatile unsigned int       GHWCFG4;            // 0x050        User HW Config4 Register(Read Only)
    volatile unsigned int       NOTDEFINE2[43];     // 0x054~       Reserved
    volatile unsigned int       HPTXFSIZ;           // 0x100        Host Periodic Transmit FIFO Size Register
    volatile unsigned int       DIEPTXFn[15];       // 0x104~       Device IN Endpoint Transmit FIFO Size register
    volatile unsigned int       NOTDEFINE3[176];    // 0x140~       Reserved
    // Host Mode CSR Map
    volatile unsigned int       HCFG;               // 0x400        Host Configuration Register
    volatile unsigned int       HFIR;               // 0x404        Host Frame Interval Register
    volatile unsigned int       HFNUM;              // 0x408        Host Frame Number/Frame Time Remaining register
    volatile unsigned int       NOTDEFINE4;         // 0x40C        Reserved
    volatile unsigned int       HPTXSTS;            // 0x410        Host Periodic Transmit FIFO/Queue Status Register
    volatile unsigned int       HAINT;              // 0x414        Host All Channels Interrupt Register
    volatile unsigned int       HAINTMSK;           // 0x418        Host All Channels Interrupt Mask register
    volatile unsigned int       NOTDEFINE5[9];      // 0x41C~       Not Used
    volatile unsigned int       HPRT;               // 0x440        Host Port Control and Status register
    volatile unsigned int       NOTDEFINE6[47];     // 0x444~       Reserved
    volatile unsigned int       HCCHARn;            // 0x500        Host Channel 0 Characteristics Register
    volatile unsigned int       HCSPLTn;            // 0x504        Host Channel 0 Split Control Register
    volatile unsigned int       HCINTn;             // 0x508        Host Channel 0 Interrupt Register
    volatile unsigned int       HCINTMSKn;          // 0x50C        Host Channel 0 Interrupt Mask Register
    volatile unsigned int       HCTSIZn;            // 0x510        Host Channel 0 Transfer Size Register
    volatile unsigned int       HCDMAn;             // 0x514        Host Channel 0 DMA Address Register
    volatile unsigned int       NOTDEFINE7[2];      // 0x518~       Reserved
    volatile unsigned int       HCH1[8];            // 0x520~       Host Channel 1 Registers
    volatile unsigned int       HCH2[8];            // 0x540~       Host Channel 2 Registers
    volatile unsigned int       HCH3[8];            // 0x560~       Host Channel 3 Registers
    volatile unsigned int       HCH4[8];            // 0x580~       Host Channel 4 Registers
    volatile unsigned int       HCH5[8];            // 0x5A0~       Host Channel 5 Registers
    volatile unsigned int       HCH6[8];            // 0x5C0~       Host Channel 6 Registers
    volatile unsigned int       HCH7[8];            // 0x5E0~       Host Channel 7 Registers
    volatile unsigned int       HCH8[8];            // 0x600~       Host Channel 8 Registers
    volatile unsigned int       HCH9[8];            // 0x620~       Host Channel 9 Registers
    volatile unsigned int       HCH10[8];           // 0x640~       Host Channel 10 Registers
    volatile unsigned int       HCH11[8];           // 0x660~       Host Channel 11 Registers
    volatile unsigned int       HCH12[8];           // 0x680~       Host Channel 12 Registers
    volatile unsigned int       HCH13[8];           // 0x6A0~       Host Channel 13 Registers
    volatile unsigned int       HCH14[8];           // 0x6C0~       Host Channel 14 Registers
    volatile unsigned int       HCH15[8];           // 0x6E0~       Host Channel 15 Registers
    volatile unsigned int       NOTDEFINE8[64];     // 0x700~       Reserved
    // Device Mode CSR Map
    volatile unsigned int       DCFG;               // 0x800        Device Configuration Register
    volatile unsigned int       DCTL;               // 0x804        Device Control Register
    volatile unsigned int       DSTS;               // 0x808        Device Status Register (Read Only)
    volatile unsigned int       NOTDEFINE9;         // 0x80C        Reserved
    volatile unsigned int       DIEPMSK;            // 0x810        Device IN Endpoint Common Interrupt Mask Register
    volatile unsigned int       DOEPMSK;            // 0x814        Device OUT Endpoint Common Interrupt Mask register
    volatile unsigned int       DAINT;              // 0x818        Device All Endpoints Interrupt Register
    volatile unsigned int       DAINTMSK;           // 0x81C        Device All Endpoints Interrupt Mask Register
    volatile unsigned int       NOTDEFINE10[2];     // 0x820~       Reserved
    volatile unsigned int       DVBUSDIS;           // 0x828        Device VBUS Discharge Time Register
    volatile unsigned int       DVBUSPULSE;         // 0x82C        Device VBUS Pulsing Time register
    volatile unsigned int       DTHRCTL;            // 0x830        Device Threshold Control register
    volatile unsigned int       DIEPEMPMSK;         // 0x834        Device IN Endpoint FIFO Empty Interrupt Mask register
    volatile unsigned int       NOTDEFINE11[50];    // 0x838~       Reserved

    //volatile unsigned int     DIEPCTL0;           // 0x900        Device Control IN Endpoint 0 Control Register
    //volatile unsigned int     NOTDEFINE12;        // 0x904        Reserved
    //volatile unsigned int     DIEPINT0;           // 0x908        Device IN Endpoint 0 Interrupt Register
    //volatile unsigned int     NOTDEFINE13;        // 0x90C        Reserved
    //volatile unsigned int     DIEPTSIZ0;          // 0x910        Device IN Endpoint 0 Transfer Size register
    //volatile unsigned int     DIEPDMA0;           // 0x914        Device IN Endpoint 0 DMA Address Register
    //volatile unsigned int     DTXFSTS0;           // 0x918        Device IN Endpoint Transmit FIFO Status Register
    //volatile unsigned int     NOTDEFINE14;        // 0x91C        Reserved

    volatile unsigned int       DEVINENDPT[16][8];  // 0x900~       Device IN Endpoint 1~15 Registers

    //volatile unsigned int     DOEPCTL0;           // 0xB00        Device Control OUT Endpoint 0 Control register
    //volatile unsigned int     NOTDEFINE15;        // 0xB04        Reserved
    //volatile unsigned int     DOEPINT0;           // 0xB08        Device OUT Endpoint 0 Interrupt Register
    //volatile unsigned int     NOTDEFINE16;        // 0xB0C        Reserved
    //volatile unsigned int     DOEPTSIZ0;          // 0xB10        Device OUT Endpoint 0 Transfer Size Register
    //volatile unsigned int     DOEPDMA0;           // 0xB14        Device OUT Endpoint 0 DMA Address register
    //volatile unsigned int     NOTDEFINE17[2];     // 0xB18~       Reserved

    volatile unsigned int       DEVOUTENDPT[16][8]; // 0xB00~       Device OUT Endpoint 1~15 Registers
    volatile unsigned int       NOTDEFINE18[64];    // 0xD00~       Reserved

    // Power and Clock Gating CSR Map
    volatile unsigned int       PCGCR;              // 0xE00        Power and Clock Gating Control Register
    volatile unsigned int       NOTDEFINE19[127];   // 0xE04~       Reserved
    // Data FIFO(DFIFO) Access Register Map
    volatile unsigned int       DFIFOENDPT[16][1024];   // 0x1000~      Device IN Endpoint 0~16/Host Out Channel 0~16: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT0[1024];  // 0x1000~      Device IN Endpoint 0/Host Out Channel 0: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT1[1024];  // 0x2000~      Device IN Endpoint 1/Host Out Channel 1: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT2[1024];  // 0x3000~      Device IN Endpoint 2/Host Out Channel 2: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT3[1024];  // 0x4000~      Device IN Endpoint 3/Host Out Channel 3: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT4[1024];  // 0x5000~      Device IN Endpoint 4/Host Out Channel 4: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT5[1024];  // 0x6000~      Device IN Endpoint 5/Host Out Channel 5: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT6[1024];  // 0x7000~      Device IN Endpoint 6/Host Out Channel 6: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT7[1024];  // 0x8000~      Device IN Endpoint 7/Host Out Channel 7: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT8[1024];  // 0x9000~      Device IN Endpoint 8/Host Out Channel 8: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT9[1024];  // 0xA000~      Device IN Endpoint 9/Host Out Channel 9: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT10[1024]; // 0xB000~      Device IN Endpoint 10/Host Out Channel 10: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT11[1024]; // 0xC000~      Device IN Endpoint 11/Host Out Channel 11: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT12[1024]; // 0xD000~      Device IN Endpoint 12/Host Out Channel 12: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT13[1024]; // 0xE000~      Device IN Endpoint 13/Host Out Channel 13: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT14[1024]; // 0xF000~      Device IN Endpoint 14/Host Out Channel 14: DFIFO Write/Read Access
    //volatile unsigned int     DFIFOENDPT15[1024]; // 0x10000~     Device IN Endpoint 15/Host Out Channel 15: DFIFO Write/Read Access
} USBOTG, *PUSBOTG;

typedef struct _USBPHYCFG
{
    volatile unsigned int       PCFG0;              // 0x100  R/W    USB PHY Configuration Register0
    volatile unsigned int       PCFG1;              // 0x104  R/W    USB PHY Configuration Register1
    volatile unsigned int       PCFG2;              // 0x108  R/W    USB PHY Configuration Register2
    volatile unsigned int       PCFG3;              // 0x10C  R/W    USB PHY Configuration Register3
    volatile unsigned int       PCFG4;              // 0x110  R/W    USB PHY Configuration Register3
    volatile unsigned int       LSTS;               // 0x114  R/W    USB PHY Configuration Register3
    volatile unsigned int       LCFG0;              // 0x118  R/W    USB PHY Configuration Register3
} USBPHYCFG, *PUSBPHYCFG;
#else

typedef struct _USBPHYCFG
{
    volatile unsigned int       U30_PCFG0;              // 0x0A4  R/W    USB PHY Configuration Register0
    volatile unsigned int       U30_PCFG1;              // 0x0A8  R/W    USB PHY Configuration Register1
    volatile unsigned int       U30_PCFG2;              // 0x0AC  R/W    USB PHY Configuration Register2
    volatile unsigned int       U30_PCFG3;              // 0x0B0  R/W    USB PHY Configuration Register3
    volatile unsigned int       U30_PCFG4;              // 0x0B4  R/W    USB PHY Configuration Register4
	volatile unsigned int 		U30_PFLT;				// 0x0B8  R/W 	 USB PHY Filter Configuration Register
	volatile unsigned int 		U30_PINT;				// 0x0BC  R/W 	 USB PHY Interrupt Register
	volatile unsigned int       U30_LCFG;               // 0x0BC  R/W    USB 3.0 LINK Controller Configuration Register Set 0 
	volatile unsigned int 		U30_PCR0;			// 0xC4  0x0  USB 3.0 PHY Parameter Control Register Set 0 
	volatile unsigned int   	U30_PCR1;			// 0xC8  0x0  USB 3.0 PHY Parameter Control Register Set 1 
	volatile unsigned int		U30_PCR2;			// 0xCC  0x0  USB 3.0 PHY Parameter Control Register Set 2 
	volatile unsigned int 		U30_SWUTMI;			// 0xD0  0x0  USB 3.0 UTMI Software Control Register
} USBPHYCFG, *PUSBPHYCFG;
#endif /* CONFIG_ARCH_TCC897X */

typedef struct {
    unsigned                :6;
    unsigned A2X_USB20H     :2;
    unsigned                :10;
    unsigned BWC            :1;
    unsigned BWU            :1;
    unsigned                :12;
} HSIO_CFG_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    HSIO_CFG_IDX_TYPE       bREG;
} HSIO_CFG_TYPE;

typedef struct {
    unsigned RXCLK_SEL      :2;
    unsigned                :2;
    unsigned RXDIV          :6;
    unsigned                :5;
    unsigned RR             :1;
    unsigned TXCLK_SEL      :2;
    unsigned                :2;
    unsigned TXDIV          :6;
    unsigned                :5;
    unsigned TR             :1;
} HSIO_ETHERCFG0_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    HSIO_ETHERCFG0_IDX_TYPE bREG;
} HSIO_ETHERCFG0_TYPE;

typedef struct {
    unsigned                :16;
    unsigned TCO            :1;
    unsigned FCTRL          :1;
    unsigned PHY_INFSEL     :3;
    unsigned                :9;
    unsigned CC             :1;
    unsigned CE             :1;
} HSIO_ETHERCFG1_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    HSIO_ETHERCFG1_IDX_TYPE bREG;
} HSIO_ETHERCFG1_TYPE;

typedef struct _HSIOBUSCFG{
    volatile HSIO_HCLK_TYPE         PWDN;           // 0x000  R/W  0xFFFFFFFF   Module Clock Mask Register
    volatile HSIO_HCLK_TYPE         SWRESET;        // 0x004  R/W  0xFFFFFFFF   Module Software Reset Register
    unsigned :32; unsigned :32;
    volatile USB20H_BCFG_TYPE       USB20H_BCFG;    // 0x010  R/W  0x00000000    
    volatile USB20H_PCFG0_TYPE      USB20H_PCFG0;   // 0x014  R/W  0x00000000    
    volatile USB20H_PCFG1_TYPE      USB20H_PCFG1;   // 0x018  R/W  0x00000000
    volatile USB20H_PCFG2_TYPE      USB20H_PCFG2;   // 0x01C  R/W  0x00000000
    volatile USB20H_STS_TYPE        USB20H_STS;     // 0x020  R/W  0x00000000
    unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    unsigned :32; unsigned :32; unsigned :32; unsigned :32;
    volatile HSIO_CFG_TYPE          HSIO_CFG;       // 0x050  R/W  0x03FF0001
    unsigned :32; unsigned :32; unsigned :32;
    volatile HSIO_ETHERCFG0_TYPE    GMAC0_CFG0;     // 0x060  R/W  0x00000000
    volatile HSIO_ETHERCFG1_TYPE    GMAC0_CFG1;     // 0x064  R/W  0x00000000
    volatile HSIO_ETHERCFG0_TYPE    GMAC1_CFG0;     // 0x068  R/W  0x00000000
    volatile HSIO_ETHERCFG1_TYPE    GMAC1_CFG1;     // 0x06C  R/W  0x00000000
} HSIOBUSCFG, *PHSIOBUSCFG;

typedef struct _USB20PHYCFG{
    volatile USB20H_BCFG_TYPE       USB20H0_BCFG;    // 0x00  R/W  0x00000000    
    volatile USB20H_PCFG0_TYPE      USB20H0_PCFG0;   // 0x04  R/W  0x00000000    
    volatile USB20H_PCFG1_TYPE      USB20H0_PCFG1;   // 0x08  R/W  0x00000000
    volatile USB20H_PCFG2_TYPE      USB20H0_PCFG2;   // 0x0C  R/W  0x00000000
    volatile USB20H_PCFG3_TYPE      USB20H0_PCFG3;   // 0x010  R/W  0x00000000
    volatile USB20H_PCFG4_TYPE      USB20H0_PCFG4;   // 0x014  R/W  0x00000000
    volatile USB20H_STS_TYPE        USB20H0_STS;     // 0x014  R/W  0x00000000
    volatile USB20H_LCFG0_TYPE      USB20H0_LCFG0;    // 0x018  R/W  0x00000000
} USB20PHYCFG, *PUSB20PHYCFG;

#endif /* _STRUCTURES_HSIO_H_ */
