/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STCREG_H
#define	_SYS_STCREG_H

#pragma ident	"@(#)stcreg.h	1.15	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Register definitions for SBus 8-port serial communications card
 * using the Cirrus Logic CD-180 octal UART including, at no extra
 * cost, the Paradise PPC-2 parallel printer controller
 */

/*
 * STC memory map
 *
 *		    read	write
 *    1000 (4K)	+-------------------------------+ (top 4 bytes of page used for
 *		|	PPC - parallel		| PPC-2 interrupt acknowledge
 *		|	printer controller	| control and status)
 *    0e00(3.5K)+-------------------------------+
 *		|	*IACK space		|
 *		|	(CD-180 regs)		|
 *    0c00 (3K) +-------------------------------+
 *		|				|
 *		|	CD180 regs		|
 *		|	(octal UART)		|
 *		|				|
 *    0800 (2K) +---------------+---------------+
 *		|		|		|
 *		|		| DTR		|
 *		|		| latch		|
 *		| BOOT		|		|
 *    0400 (1K) + PROM		+---------------+
 *		|		|		|
 *		|		| (N/A)  	|
 *		|		|		|
 *		|		|		|
 *    0000 (0K) +---------------+---------------+
 *
 * due to expansion box requirements, the page is actually mapped by the driver
 * at 64K bytes above address 0 in the SBus slot, which is fine, since the
 * card's registers are mirrored every 4K throughout the slot address space
 */

/*
 * define some offsets from the base address of the board for the various
 * device spaces on the board since everything's in 1 page (4K), it
 * doesn't make sense to have multiple register space mappings
 */
#define	DTR_OFFSET	0x0400	/* offset of DTR latch from base */
#define	CD180_OFFSET	0x0800	/* offset of CD180 registers from base */
#define	IACK_OFFSET	0x0c00	/* offset of CD180 regs in *IACK space */
#define	PPC_OFFSET	0x0e00	/* offset of PPC registers from base */

/*
 * DTR latch register map
 *
 * The DTR output lines for each port are controlled by an 8-bit
 * transparent latch (DTR latch); each port's DTR line occupies
 * one byte of address space.  Writing a 1 to a port's DTR line
 * latch address raises DTR; writing a 0 lowers it.  Writing any
 * other value than 0 or 1 causes a system crash due to excessive
 * bus loading (see SBus loading and capacitance spec).
 *
 * we don't need a struct, since we have a copy of the address of
 * each line's DTR latch location in the line struct (see stcvar.h)
 */

/*
 * CD-180 octal UART register map
 *
 * There are two sets of registers, global registers signified by
 * address line A6=1, and channel registers signified by A6=0.
 *
 * The global registers can always be accessed directly; the channel
 * registers can be accessed after writing the desired channel number
 * into the Channel Access Register (CAR).
 *
 * Note that certain global registers can not be accessed unless the
 * chip thinks it's in an interrupt routine; these are the RDR, RCSR,
 * TDR and the EOIR registers (see p. 26 of the CD-180 data sheet) and
 * are marked with a (V) in the definition below.
 *
 * During the interrupt service routine, the CD-180 wants to put a
 * vector on the bus (the contents of the GIVR register); it will do
 * this when it sees a read (*RD asserted) qualified with an interrupt
 * acknowledge (*IACK asserted) signal.  To assert *IACK, all the CD-180
 * registers are duplicated in the "*IACK space"; an access to this space
 * will qualify the *RD or *WR signal with *IACK.
 */

#define	FILLER(n, x)	uchar_t	f##n[x]	/* for those annoying pet stains */

struct stcregs_t {
	/* channel registers - use CAR to specify channel */
	FILLER(0, 1);
	volatile uchar_t ccr;	/* CCR - Channel Command Register */
	volatile uchar_t ier;	/* IER - Interrupt Enable Register */
	volatile uchar_t cor1;	/* COR1 - Channel Option Register 1 */
	volatile uchar_t cor2;	/* COR2 - Channel Option Register 2 */
	volatile uchar_t cor3;	/* COR3 - Channel Option Register 3 */
	volatile uchar_t ccsr;	/* CCSR - Channel Control Status Register */
	volatile uchar_t rdcr;	/* RDCR - Receive Data Count Register */
	FILLER(1, 1);
	volatile uchar_t schr1;	/* SCHR1 - Special Character Register 1 */
	volatile uchar_t schr2;	/* SCHR2 - Special Character Register 2 */
	volatile uchar_t schr3;	/* SCHR3 - Special Character Register 3 */
	volatile uchar_t schr4;	/* SCHR4 - Special Character Register 4 */
	FILLER(2, 3);
	volatile uchar_t mcor1;	/* MCOR1 - Modem Change Option Register 1 */
	volatile uchar_t mcor2;	/* MCOR2 - Modem Change Option Register 2 */
	volatile uchar_t mcr;	/* MCR - Modem Change Register */
	FILLER(3, 5);
	volatile uchar_t rtpr;	/* RTPR - Receive Timeout Period Register */
	FILLER(4, 15);
	volatile uchar_t msvr;	/* MSVR - Modem Signal Value Register */
	FILLER(5, 8);
	volatile uchar_t rbprh;	/* RBPRH - Rcv Baud Rate Period Register High */
	volatile uchar_t rbprl;	/* RBPRL - Rcv Baud Rate Period Register Low */
	FILLER(6, 6);
	volatile uchar_t tbprh;	/* TBPRH - Xmit Baud Rate Period Register Hi */
	volatile uchar_t tbprl;	/* TBPRL - Xmit Baud Rate Period Register Low */

	/* global registers - (V) means only available in interrupt routine */
	FILLER(7, 5);
	volatile uchar_t givr;	/* GIVR - Global Interrupt Vector Register */
	volatile uchar_t gicr;	/* GICR - Global Interruping Channel Register */
	FILLER(8, 31);
	volatile uchar_t pilr1;	/* PILR1 - Priority Interrupt Level Reg 1 */
	volatile uchar_t pilr2;	/* PILR2 - Priority Interrupt Level Reg 2 */
	volatile uchar_t pilr3;	/* PILR3 - Priority Interrupt Level Reg 3 */
	volatile uchar_t car;	/* CAR - Channel Access Register */
	FILLER(9, 6);
	volatile uchar_t gfrcr;	/* GFRCR - Global Firmware Revision Register */
	FILLER(10, 4);
	volatile uchar_t pprh;	/* PPRH - Prescaler Period Register High */
	volatile uchar_t pprl;	/* PPRL - Prescaler Period Register Low */
	FILLER(11, 6);
	volatile uchar_t rdr;	/* RDR - Receiver Data Register (V) */
	FILLER(12, 1);
	volatile uchar_t rcsr;	/* RCSR - Receiver Character Status Reg (V) */
	volatile uchar_t tdr;	/* TDR - Transmit Data Register (V) */
	FILLER(13, 3);
	volatile uchar_t eoir;	/* EOIR - End of Interrupt Register (V) */
};

/*
 * CD-180 registers in *IACK space.  Since accesses to this space are only made
 * during the first part of the interrupt handler to determine if a particular
 * type of interrupt occured, and the CD-180 can only generate 4 distinct types
 * of interrupts (although Group 3 interrupts, receive data available and
 * receive exception interrupt share the same level), we only need to have
 * mappings to 3 offsets in the CD-180 address space; when accessing these
 * offsets, the value on the address bus (A0...A6) will be compared by the
 * CD-180 to it's 3 PILR registers to determine what type of interrupt we are
 * trying to service, and the CD-180 will return the value of the GIVR in the
 * read.
 *
 * To make all this work requires a leap of faith, and also make sure that
 * the following CD-180 interrupt-related registers are programmed with the
 * following values (from stcvar.h):
 *			pilr1 - 0x80	(PILR1)
 *			pilr2 - 0x81	(PILR2)
 *			pilr3 - 0x82	(PILR3)
 * Note that the high bit is set; this is per the CD-180 spec, pp. 24 and 25.
 */
struct stciack_t {
	volatile uchar_t pilr1;	/* group 1 (modem signal change) interrupts */
	volatile uchar_t pilr2;	/* group 2 (transmit data) interrupts */
	volatile uchar_t pilr3;	/* group 3 (receive data/exception) intr */
};

/*
 * PPC-2 parallel printer controller register map
 *
 * The PPC-2 has 4 registers; a r/w data register that reflects the current
 * state of the 8 data lines, a r/o status register that reflects the input
 * signals like PAPER OUT, BUSY, ACK, etc..., a w/o control register that
 * controls things like bidirectional operation, interrupt enable and STROBE,
 * and a n/a wierd register.
 * The PAL on the stc is designed so that when a PPC-2 interrupt occurs (due to
 * the ACK signal pulsing), the first access to the PPC-2 address space will
 * clear the interrupt and return the data at the corresponding FCode PROM
 * location.  In this revision of the board, the driver expects to read the
 * value 0x0a from the "ipstat" address if an interrupt has occured, otherwise
 * a value with the bits specified in PPC_INTMASK set.
 * Don't remove the "pwierd" member, and if you change the PPC-2 offset in the
 * address map, make sure you adjust the parameters to the FILLER() macro also.
 */
struct ppcregs_t {
	/* for input */
	volatile uchar_t in_pdata;	/* data I/O register */
	volatile uchar_t in_pstat;	/* status register */
	volatile uchar_t in_pcon;	/* control register */
	volatile uchar_t in_pwierd;	/* wierd register */
	/* for output */
	volatile uchar_t pdata;		/* data I/O register */
	volatile uchar_t pstat;		/* status register */
	volatile uchar_t pcon;		/* control register */
	volatile uchar_t pwierd;	/* wierd register */
	FILLER(14, (0xffc-PPC_OFFSET-8)); /* should be last 4 bytes of page */
	volatile uchar_t ipdata; /* mirror of data register in iack space */
	volatile uchar_t ipstat; /* mirror of status register in iack space */
	volatile uchar_t ipcon;	/* mirror of control register in iack space */
	volatile uchar_t ipwierd; /* mirror of wierd register in iack space */
};

#ifdef	__cplusplus
}
#endif

#endif	/* !_SYS_STCREG_H */
