/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF    	*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any  	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SUDEV_H
#define	_SYS_SUDEV_H

#pragma ident	"@(#)sudev.h	1.7	98/01/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/tty.h>
#include <sys/ksynch.h>
#include <sys/dditypes.h>

/*
 * Definitions for INS8250 / 16550  chips
 */

/* defined as offsets from the data register */
#define	DAT		0 	/* receive/transmit data */
#define	ICR		1  	/* interrupt control register */
#define	ISR		2   	/* interrupt status register */
#define	LCR		3   	/* line control register */
#define	MCR		4   	/* modem control register */
#define	LSR		5   	/* line status register */
#define	MSR		6   	/* modem status register */
#define	DLL		0   	/* divisor latch (lsb) */
#define	DLH		1   	/* divisor latch (msb) */
#define	FIFOR		ISR	/* FIFO register for 16550 */

#ifdef SU_DRIVER
#define	OUTB(ignore, offset, value) ddi_put8(asy->asy_handle, \
					asy->asy_ioaddr+offset, value)
#define	INB(ignore, offset) \
	ddi_get8(asy->asy_handle, asy->asy_ioaddr+offset)
#else SU_DRIVER
#define	INB(a, off)		(inb((int)(a)+(off)))
#define	OUTB(a, off, val)	(outb((int)(a)+(off), (char)(val)))
#endif SU_DRIVER


/*
 * INTEL 8210-A/B & 16450/16550 Registers Structure.
 */

/* Line Control Register */
#define	WLS0		0x01	/* word length select bit 0 */
#define	WLS1		0x02	/* word length select bit 2 */
#define	STB		0x04	/* number of stop bits */
#define	PEN		0x08	/* parity enable */
#define	EPS		0x10	/* even parity select */
#define	SETBREAK 	0x40	/* break key */
#define	DLAB		0x80	/* divisor latch access bit */
#define	RXLEN   	0x03   	/* # of data bits per received/xmitted char */
#define	STOP1   	0x00
#define	STOP2   	0x04
#define	PAREN   	0x08
#define	PAREVN  	0x10
#define	PARMARK 	0x20
#define	SNDBRK  	0x40


#define	BITS5		0x00	/* 5 bits per char */
#define	BITS6		0x01	/* 6 bits per char */
#define	BITS7		0x02	/* 7 bits per char */
#define	BITS8		0x03	/* 8 bits per char */

/* baud rate definitions */
#define	ASY110		1047	/* 110 baud rate for serial console */
#define	ASY150		768	/* 150 baud rate for serial console */
#define	ASY300		384	/* 300 baud rate for serial console */
#define	ASY600		192	/* 600 baud rate for serial console */
#define	ASY1200		96	/* 1200 baud rate for serial console */
#define	ASY2400		48	/* 2400 baud rate for serial console */
#define	ASY4800		24	/* 4800 baud rate for serial console */
#define	ASY9600		12	/* 9600 baud rate for serial console */

/* Line Status Register */
#define	RCA		0x01	/* data ready */
#define	OVRRUN		0x02	/* overrun error */
#define	PARERR		0x04	/* parity error */
#define	FRMERR		0x08	/* framing error */
#define	BRKDET  	0x10	/* a break has arrived */
#define	XHRE		0x20	/* tx hold reg is now empty */
#define	XSRE		0x40	/* tx shift reg is now empty */
#define	RFBE		0x80	/* rx FIFO Buffer error */

/* Interrupt Id Regisger */
#define	MSTATUS		0x00	/* modem status changed */
#define	NOINTERRUPT	0x01	/* no interrupt pending */
#define	TxRDY		0x02	/* Transmitter Holding Register Empty */
#define	RxRDY		0x04	/* Receiver Data Available */
#define	FFTMOUT 	0x0c	/* FIFO timeout - 16550AF */
#define	RSTATUS 	0x06	/* Receiver Line Status */

/* Interrupt Enable Register */
#define	RIEN		0x01	/* Received Data Ready */
#define	TIEN		0x02	/* Tx Hold Register Empty */
#define	SIEN		0x04	/* Receiver Line Status */
#define	MIEN		0x08	/* Modem Status */

/* Modem Control Register */
#define	DTR		0x01	/* Data Terminal Ready */
#define	RTS		0x02	/* Request To Send */
#define	OUT1		0x04	/* Aux output - not used */
#define	OUT2		0x08	/* turns intr to 386 on/off */
#define	ASY_LOOP	0x10	/* loopback for diagnostics */

/* Modem Status Register */
#define	DCTS		0x01	/* Delta Clear To Send */
#define	DDSR		0x02	/* Delta Data Set Ready */
#define	DRI		0x04	/* Trail Edge Ring Indicator */
#define	DDCD		0x08	/* Delta Data Carrier Detect */
#define	CTS		0x10	/* Clear To Send */
#define	DSR		0x20	/* Data Set Ready */
#define	RI		0x40	/* Ring Indicator */
#define	DCD		0x80	/* Data Carrier Detect */

#define	DELTAS(x)	((x)&(DCTS|DDSR|DRI|DDCD))
#define	STATES(x)	((x)(CTS|DSR|RI|DCD))

/* flags for FCR (FIFO Control register) */
#define	FIFO_OFF	0x00	/* fifo disabled */
#define	FIFO_ON		0x01	/* fifo enabled */
#define	FIFOEN		0x8f	/* fifo enabled, w/ 8 byte trigger */
#define	FIFORCLR	0x8b	/* Clear receiver FIFO only */

#define	FIFORXFLSH	0x02	/* flush receiver FIFO */
#define	FIFOTXFLSH	0x04	/* flush transmitter FIFO */
#define	FIFODMA		0x08	/* DMA mode 1 */
#define	FIFO_TRIG_1	0x00	/* 1 byte trigger level */
#define	FIFO_TRIG_4	0x40	/* 4 byte trigger level */
#define	FIFO_TRIG_8	0x80	/* 8 byte trigger level */
#define	FIFO_TRIG_14	0xC0	/* 14 byte trigger level */

/*
 * Defines for ioctl calls (VP/ix)
 */

#define	AIOC		('A'<<8)
#define	AIOCINTTYPE	(AIOC|60)	/* set interrupt type */
#define	AIOCDOSMODE	(AIOC|61)	/* set DOS mode */
#define	AIOCNONDOSMODE	(AIOC|62)	/* reset DOS mode */
#define	AIOCSERIALOUT	(AIOC|63)	/* serial device data write */
#define	AIOCSERIALIN	(AIOC|64)	/* serial device data read */
#define	AIOCSETSS	(AIOC|65)	/* set start/stop chars */
#define	AIOCINFO	(AIOC|66)	/* tell usr what device we are */

/* Ioctl alternate names used by VP/ix */
#define	VPC_SERIAL_DOS		AIOCDOSMODE
#define	VPC_SERIAL_NONDOS	AIOCNONDOSMODE
#define	VPC_SERIAL_INFO		AIOCINFO
#define	VPC_SERIAL_OUT		AIOCSERIALOUT
#define	VPC_SERIAL_IN		AIOCSERIALIN

/* Serial in/out requests */
#define	SO_DIVLLSB	1
#define	SO_DIVLMSB	2
#define	SO_LCR		3
#define	SO_MCR		4
#define	SI_MSR		1
#define	SIO_MASK(elem)		(1<<((elem)-1))

#define	OVERRUN		040000
#define	FRERROR		020000
#define	PERROR		010000
#define	S_ERRORS	(PERROR|OVERRUN|FRERROR)

/*
 * Ring buffer and async line management definitions.
 */
#define	RINGBITS	10		/* # of bits in ring ptrs */
#define	RINGSIZE	(1<<RINGBITS)   /* size of ring */
#define	RINGMASK	(RINGSIZE-1)
#define	RINGFRAC	8		/* fraction of ring to force flush */

#define	RING_INIT(ap)  ((ap)->async_rput = (ap)->async_rget = 0)
#define	RING_CNT(ap)   (((ap)->async_rput - (ap)->async_rget) & RINGMASK)
#define	RING_FRAC(ap)  ((int)RING_CNT(ap) >= (int)(RINGSIZE/RINGFRAC))
#define	RING_POK(ap, n) ((int)RING_CNT(ap) < (int)(RINGSIZE-(n)))
#define	RING_PUT(ap, c) \
	((ap)->async_ring[(ap)->async_rput++ & RINGMASK] =  (uchar_t)(c))
#define	RING_UNPUT(ap) ((ap)->async_rput--)
#define	RING_GOK(ap, n) ((int)RING_CNT(ap) >= (int)(n))
#define	RING_GET(ap)   ((ap)->async_ring[(ap)->async_rget++ & RINGMASK])
#define	RING_EAT(ap, n) ((ap)->async_rget += (n))
#define	RING_MARK(ap, c, s) \
	((ap)->async_ring[(ap)->async_rput++ & RINGMASK] = ((uchar_t)(c)|(s)))
#define	RING_UNMARK(ap) \
	((ap)->async_ring[((ap)->async_rget) & RINGMASK] &= ~S_ERRORS)
#define	RING_ERR(ap, c) \
	((ap)->async_ring[((ap)->async_rget) & RINGMASK] & (c))

/*
 * Hardware channel common data. One structure per port.
 * Each of the fields in this structure is required to be protected by a
 * mutex lock at the highest priority at which it can be altered.
 * The asy_flags, and asy_next fields can be altered by interrupt
 * handling code that must be protected by the mutex whose handle is
 * stored in asy_excl_hi.  All others can be protected by the asy_excl
 * mutex, which is lower priority and adaptive.
 */

struct asycom {
	int		asy_flags;	/* random flags  */
					/* protected by asy_excl_hi lock */
	uint_t		asy_hwtype;	/* HW type: ASY82510, etc. */
	uint_t		asy_use_fifo;	/* HW FIFO use it or not ?? */
	uint_t		asy_fifo_buf;	/* With FIFO = 16, otherwise = 1 */
	uchar_t		*asy_ioaddr;	/* i/o address of ASY port */
	uint_t		asy_vect;	/* IRQ number */
	boolean_t	suspended;	/* TRUE if driver suspended */
	caddr_t		asy_priv;	/* protocol private data */
	dev_info_t	*asy_dip;	/* dev_info */
	long		asy_unit;	/* which port */
	ddi_iblock_cookie_t asy_iblock;
	kmutex_t	*asy_excl;	/* asy adaptive mutex */
	kmutex_t	*asy_excl_hi;	/* asy spinlock mutex */
	ddi_acc_handle_t asy_handle;    /* ddi_get/put handle */
};

/*
 * Asychronous protocol private data structure for ASY.
 * Each of the fields in the structure is required to be protected by
 * the lower priority lock except the fields that are set only at
 * base level but cleared (with out lock) at interrupt level.
 */

struct asyncline {
	int		async_flags;	/* random flags */
	kcondvar_t	async_flags_cv; /* condition variable for flags */
	dev_t		async_dev;	/* device major/minor numbers */
	mblk_t		*async_xmitblk;	/* transmit: active msg block */
	struct asycom	*async_common;	/* device common data */
	tty_common_t 	async_ttycommon; /* tty driver common data */
	bufcall_id_t	async_wbufcid;	/* id for pending write-side bufcall */
	timeout_id_t	async_polltid;	/* softint poll timeout id */

	/*
	 * The following fields are protected by the asy_excl_hi lock.
	 * Some, such as async_flowc, are set only at the base level and
	 * cleared (without the lock) only by the interrupt level.
	 */
	uchar_t		*async_optr;	/* output pointer */
	int		async_ocnt;	/* output count */
	ushort_t	async_rput;	/* producing pointer for input */
	ushort_t	async_rget;	/* consuming pointer for input */
	uchar_t		async_flowc;	/* flow control char to send */

	/*
	 * Each character stuffed into the ring has two bytes associated
	 * with it.  The first byte is used to indicate special conditions
	 * and the second byte is the actual data.  The ring buffer
	 * needs to be defined as ushort_t to accomodate this.
	 */
	ushort_t	async_ring[RINGSIZE];

	short		async_break;	/* break count */

	union {
		struct {
			uchar_t _hw;	/* overrun (hw) */
			uchar_t _sw;	/* overrun (sw) */
		} _a;
		ushort_t uover_overrun;
	} async_uover;
#define	async_overrun		async_uover._a.uover_overrun
#define	async_hw_overrun	async_uover._a._hw
#define	async_sw_overrun	async_uover._a._sw
	short		async_ext;	/* modem status change count */
	short		async_work;	/* work to do flag */
};

/* definitions for async_flags field */
#define	ASYNC_EXCL_OPEN	 0x10000000	/* exclusive open */
#define	ASYNC_WOPEN	 0x00000001	/* waiting for open to complete */
#define	ASYNC_ISOPEN	 0x00000002	/* open is complete */
#define	ASYNC_OUT	 0x00000004	/* line being used for dialout */
#define	ASYNC_CARR_ON	 0x00000008	/* carrier on last time we looked */
#define	ASYNC_STOPPED	 0x00000010	/* output is stopped */
#define	ASYNC_DELAY	 0x00000020	/* waiting for delay to finish */
#define	ASYNC_BREAK	 0x00000040	/* waiting for break to finish */
#define	ASYNC_BUSY	 0x00000080	/* waiting for transmission to finish */
#define	ASYNC_DRAINING	 0x00000100	/* waiting for output to drain */
#define	ASYNC_SERVICEIMM 0x00000200	/* queue soft interrupt as soon as */
#define	ASYNC_HW_IN_FLOW 0x00000400	/* input flow control in effect */
#define	ASYNC_HW_OUT_FLW 0x00000800	/* output flow control in effect */

/* asy_hwtype definitions */
#define	ASY82510	0x1
#define	ASY16550AF	0x2
#define	ASY8250		0x3		/* 8250 or 16450 or 16550 */

/* definitions for asy_flags field */
#define	ASY_NEEDSOFT	0x00000001
#define	ASY_DOINGSOFT	0x00000002

/*
 * OUTLINE defines the high-order flag bit in the minor device number that
 * controls use of a tty line for dialin and dialout simultaneously.
 */
#define	OUTLINE		(1 << (NBITSMINOR32 - 1))
#define	UNIT(x)		(getminor(x) & ~OUTLINE)

/*
 * ASYSETSOFT macro to pend a soft interrupt if one isn't already pending.
 */

extern kmutex_t	asy_soft_lock;		/* ptr to lock for asysoftpend */
extern int asysoftpend;			/* secondary interrupt pending */

#define	ASYSETSOFT(asy)	{		\
	mutex_enter(&asy_soft_lock);	\
	asy->asy_flags |= ASY_NEEDSOFT;	\
	if (!asysoftpend) {		\
		asysoftpend = 1;	\
		mutex_exit(&asy_soft_lock);\
		ddi_trigger_softintr(asy_softintr_id);\
	}				\
	else				\
		mutex_exit(&asy_soft_lock);\
}

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_SUDEV_H */
