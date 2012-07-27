/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This file is based on mcfqspi.h
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SPI_MVF_H_
#define SPI_MVF_H_

struct spi_mvf_chip {
	u8 mode;
	u8 bits_per_word;
	u16 void_write_data;
	/* Only used in master mode */
	u8 dbr;		/* Double baud rate */
	u8 pbr;		/* Baud rate prescaler */
	u8 br;		/* Baud rate scaler */
	u8 pcssck;	/* PCS to SCK delay prescaler */
	u8 pasc;	/* After SCK delay prescaler */
	u8 pdt;		/* Delay after transfer prescaler */
	u8 cssck;	/* PCS to SCK delay scaler */
	u8 asc;		/* After SCK delay scaler */
	u8 dt;		/* Delay after transfer scaler */
};

struct spi_mvf_master {
	u16 bus_num;
	int *chipselect;
	int num_chipselect;
	void (*cs_control)(u8 cs, u8 command);
};


#define	SPI_MCR			0x00

#define SPI_TCR			0x08

#define SPI_CTAR(x)		(0x0c + (x * 4))
#define SPI_CTAR_FMSZ(x)	(((x) & 0x0000000f) << 27)
#define SPI_CTAR_CPOL(x)	((x) << 26)
#define SPI_CTAR_CPHA(x)	((x) << 25)
#define SPI_CTAR_PCSSCR(x)	(((x) & 0x00000003) << 22)
#define SPI_CTAR_PASC(x)	(((x) & 0x00000003) << 20)
#define SPI_CTAR_PDT(x)		(((x) & 0x00000003) << 18)
#define SPI_CTAR_PBR(x)		(((x) & 0x00000003) << 16)
#define SPI_CTAR_CSSCK(x)	(((x) & 0x0000000f) << 12)
#define SPI_CTAR_ASC(x)		(((x) & 0x0000000f) << 8)
#define SPI_CTAR_DT(x)		(((x) & 0x0000000f) << 4)
#define SPI_CTAR_BR(x)		((x) & 0x0000000f)

#define SPI_CTAR0_SLAVE		0x0c

#define SPI_SR			0x2c
#define SPI_SR_EOQF		0x10000000

#define SPI_RSER		0x30
#define SPI_RSER_EOQFE		0x10000000

#define SPI_PUSHR		0x34
#define SPI_PUSHR_CONT		(1 << 31)
#define SPI_PUSHR_CTAS(x)	(((x) & 0x00000007) << 28)
#define SPI_PUSHR_EOQ		(1 << 27)
#define SPI_PUSHR_CTCNT		(1 << 26)
#define SPI_PUSHR_PCS(x)	(((x) & 0x0000003f) << 16)
#define SPI_PUSHR_TXDATA(x)	((x) & 0x0000ffff)

#define SPI_PUSHR_SLAVE		0x34

#define SPI_POPR		0x38
#define SPI_POPR_RXDATA(x)	((x) & 0x0000ffff)

#define SPI_TXFR0		0x3c
#define SPI_TXFR1		0x40
#define SPI_TXFR2		0x44
#define SPI_TXFR3		0x48
#define SPI_RXFR0		0x7c
#define SPI_RXFR1		0x80
#define SPI_RXFR2		0x84
#define SPI_RXFR3		0x88


#define SPI_FRAME_BITS		SPI_CTAR_FMSZ(0xf)
#define SPI_FRAME_BITS_16	SPI_CTAR_FMSZ(0xf)
#define SPI_FRAME_BITS_8	SPI_CTAR_FMSZ(0xf)

#define SPI_CS_INIT		0x01
#define SPI_CS_ASSERT		0x02
#define SPI_CS_DROP		0x04

#endif /* SPI_MVF_H_ */
