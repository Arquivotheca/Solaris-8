/*
 * Copyright (c) 1990,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LANCE_H
#define	_SYS_LANCE_H

#pragma ident	"@(#)lance.h	1.8	99/10/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Declarations and definitions specific to the Am7990
 * LANCE Ethernet Local Area Network Controller.
 */

/*
 * The LANCE chip saves address pins by accessing
 * several registers with one address pin by first writing the
 * register address to an internal address register, then reading
 * or writing a data register.  To use this safely, care must be
 * taken that the driver isn't reentered between the writing
 * of the address register and the access to the data register,
 * lest the reentered code try to touch the registers and
 * screw up the sequence.
 *
 * There are 4 registers accessible with this scheme, CSR0, CSR1,
 * CSR2, and CSR3.  In normal operation, only CSR0 can be or
 * needs to be accessed, so the driver normally leaves a 0 in the
 * Register Address Port, allowing CSR0 to be accessed simply by
 * accessing the Register Data Port.  During the initialization
 * sequence, when the other CSRs need to be accessed, the appropriate
 * CSR address is written into the address port, and afterwards
 * the 0 is put back in the address port.
 */

struct lanceregs {
	volatile uint16_t	lance_rdp;	/* Register Data Port */

	/*
	 * Strictly speaking, the following definition is
	 *
	 *	uint16_t	: 14,
	 *		rap	:  2;
	 *
	 * but compilers will often generate byte accesses on
	 * machines where the lance chip needs to be accessed
	 * as a halfword (e.g., 4/60,3/80), so we'll declare
	 * it as a plain halfword here.
	 *
	 */
	volatile uint16_t	lance_rap;	/* Register Address Port */
};
#define	lance_csr lance_rdp

#define	LANCE_CSR0		0
#define	LANCE_CSR1		1
#define	LANCE_CSR2		2
#define	LANCE_CSR3		3

/*
 * Control and status bits for CSR0.
 * These behave somewhat strangely, but the net effect is that
 * bit masks may be written to the register which affect only
 * those functions for which there is a one bit in the mask.
 * The exception is the interrupt enable, which must be explicitly
 * set to the correct value in each mask that is used.
 *
 * RO - Read Only, writing has no effect
 * RC - Read, Clear.  Writing 1 clears, writing 0 has no effect
 * RW - Read, Write.
 * W1 - Write with 1 only.  Writing 1 sets, writing 0 has no effect.
 *	Reading gives unpredictable data but doesn't hurt anything.
 * RW1 - Read, Write with 1 only.  Writing 1 sets, writing 0 has no effect.
 */

#define	LANCE_ERR	0x8000		/* RO BABL | CERR | MISS | MERR */
#define	LANCE_BABL	0x4000		/* RC transmitted too many bits */
#define	LANCE_CERR	0x2000		/* RC No Heartbeat */
#define	LANCE_MISS	0x1000		/* RC Missed an incoming packet */
#define	LANCE_MERR	0x0800		/* RC Memory Error; no acknowledge */
#define	LANCE_RINT	0x0400		/* RC Received packet Interrupt */
#define	LANCE_TINT	0x0200		/* RC Transmitted packet Interrupt */
#define	LANCE_IDON	0x0100		/* RC Initialization Done */
#define	LANCE_INTR	0x0080		/* RO BABL|MISS|MERR|RINT|TINT|IDON */
#define	LANCE_INEA	0x0040		/* RW Interrupt Enable */

#define	LANCE_RXON	0x0020		/* RO Receiver On */
#define	LANCE_TXON	0x0010		/* RO Transmitter On */
#define	LANCE_TDMD	0x0008		/* W1 Transmit Demand (send it now) */
#define	LANCE_STOP	0x0004		/* RW1 Stop */
#define	LANCE_STRT	0x0002		/* RW1 Start */
#define	LANCE_INIT	0x0001		/* RW1 Initialize */

/*
 * CSR1 is the low 16 bits of the address of the initialization block
 * CSR2 is the high 8 bits of the address of the initialization block
 *	the high 8 bits of the register must be 0
 * CSR3 mode bits:
 */
#define	LANCE_BSWP	0x4	/* Byte Swap (on for 68000 byte order) */
#define	LANCE_ACON	0x2	/* ALE Control (on for active low ALE) */
#define	LANCE_BCON	0x1	/* Byte Control (see the manual) */

/* The address contained in this structure must be longword aligned */
struct lancering {		/* Descriptor Ring Pointer */
	uint16_t lr_laddr;	/* Low 16 bits of ring address */
	uint8_t	lr_len	: 3;	/* Binary exponent of no. of ring entries */
	uint8_t		: 5;	/* Reserved */
	uint8_t	lr_haddr;	/* High 8 bits of ring address */
};

/*
 * Initialization Block.  This structure is constructed in memory,
 * and it's address is written into the chip during initialization.
 * The chip then fetches it's initialization info from the structure.
 */
struct lance_init_block {
	/* In the normal mode, these 16 bits are all 0 */
	uint16_t	ib_prom	: 1;	/* Promiscuous Mode */
	uint16_t		: 8;	/* Reserved */
	uint16_t	ib_intl	: 1;	/* Internal Loopback */
	uint16_t	ib_drty	: 1;	/* Disable Retry */
	uint16_t	ib_coll	: 1;	/* Force Collision */
	uint16_t	ib_dtcr	: 1;	/* Disable Transmit CRC */
	uint16_t	ib_loop	: 1;	/* Loopback */
	uint16_t	ib_dtx	: 1;	/* Disable Transmitter */
	uint16_t	ib_drx	: 1;	/* Disable Receiver */

	/*
	 * The bytes must be swapped within the word, so that, for example,
	 * the address: 8:0:20:1:25:5a is written in the order
	 *		0 8 1 20 5a 25
	 */
	uint8_t		ib_padr[6];

	uint16_t	ib_ladrf[4];	/* Multicast logical address filter */

	struct	lancering ib_rdrp;	/* Receive Descriptor Ring Pointer */
	struct	lancering ib_tdrp;	/* Transmit Descriptor Ring Pointer */
};

struct	lmd {			/* Message Descriptor */
	uint16_t lmd_ladr;	/* Low Order 16 Address Bits */
	uint8_t	lmd_flags;
	uint8_t	lmd_hadr: 8;	/* High Order 8 Address Bits */
	uint16_t lmd_bcnt;	/* Buffer Byte Count (maximum length) */
	uint16_t lmd_mcnt;	/* Message Byte Count (actual length) */
};
#define	lmd_flags3 lmd_mcnt	/* for Transmit message descriptor */

/* Bits common to both rmds and tmds */
#define	LMD_OWN		0x80	/* Chip owns the descriptor */
#define	LMD_ERR		0x40	/* Error occurred */
#define	LMD_STP		0x02	/* Start of Packet */
#define	LMD_ENP		0x01	/* End of Packet */

/* Bits in rmd flags */
#define	RMD_FRAM	0x20	/* Framing error */
#define	RMD_OFLO	0x10	/* Internal Silo Overflowed. Valid if !ENP */
#define	RMD_CRC		0x08	/* CRC Error */
#define	RMD_BUFF	0x04	/* Didn't have a buffer for the packet */
/* bits in tmd flags */
#define	TMD_RES		0x20	/* Reserved, lance writes this with a zero */
#define	TMD_MORE	0x10	/* More than one retry was needed */
#define	TMD_ONE  	0x08	/* Exactly One Retry, valid only if !LCOL */
#define	TMD_DEF  	0x04	/* Deferred (net was initially busy) */

/* Bits for lmd_errflags */
#define	TMD_BUFF 0x8000		/* Buffer Error (imples underflow too) */
#define	TMD_UFLO 0x4000		/* Underflow Error */
#define	TMD_LCOL 0x1000		/* Late Collision */
#define	TMD_LCAR 0x0800		/* Loss of Carrier */
#define	TMD_RTRY 0x0400		/* More than 16 Retry's */
#define	TMD_TDR	 0x003f		/* Time Domain Reflectometry counter mask */

/* Handy combo errflags */
#define	TMD_ANYERROR	(TMD_BUFF|TMD_UFLO|TMD_LCOL|TMD_LCAR|TMD_RTRY)

#define	LANCEALIGN	(8)
#define	LANCEALIGNMASK	(0x3);
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LANCE_H */
