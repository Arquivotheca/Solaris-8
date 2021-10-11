/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_QEC_H
#define	_SYS_QEC_H

#pragma ident	"@(#)qec.h	1.15	98/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Declarations and definitions specific to the global registers of the
 * Quad Ethernet Controller (QEC) chip.
 *
 * The QEC chip is an SBus ASIC designed by Sun Microsystems.
 * The QEC functions as an SBus master/slave controller
 * and local bus manager in one of two modes:
 *
 *  - MACE mode controls up to 4 Am79C940 MACE 10 Mbit/s ethernet chips.
 *  - Big-MAC mode controls a single 10/100 Mbit/s ethernet chip.
 *
 * Theory of operation: (MACE mode)
 * The QEC supports 4 per-channel and 1 global register sets.
 * Slave access to the MACE registers are supported.
 * The QEC acts as an intermediary between transmit and receive
 * buffers in host main memory and the MACE chip by issuing
 * SBus DVMA requests to read and write host memory to access
 * transmit/receive descriptors and transmit/receive buffers.
 * Buffer data is moved between the host main memory and local
 * 64K of SRAM to create a fully-buffered networking interface
 * very robust to SBus latency.  The QEC manages SBus (slave)
 * and MACE accesses to local memory.  Upon frame transmission/
 * reception completion, the QEC reads appropriate MACE registers
 * updating internal QEC registers.  SBus interrupts are both
 * MACE and QEC sourced and fully maskable.
 *
 * Theory of operation: (BigMAC mode)
 * The QEC supports 1 100Mbps Ethernet interface with 1 channel and
 * 1 global register sets.
 * Slave access to the BigMAC registers are supported.
 * Buffer data is moved between the host main memory and local
 * 64K of SRAM to create a fully-buffered networking interface
 * very robust to SBus latency.  The QEC manages SBus (slave)
 * and BigMAC accesses to local memory.  Upon frame transmission/
 * reception completion, the QEC reads appropriate BigMAC registers
 * updating internal QEC registers.  SBus interrupts are both
 * MACE and QEC sourced and fully maskable.
 *
 * Refer to the QEC Programmers Model for further information.
 */

/*
 * QEC Global register set.
 */
struct	qec_global {
	uint_t	control;	/* global control register (RW) */
	uint_t 	status;		/* global status register (R) */
	uint_t	packetsize;	/* global packet size register (RW) */
	uint_t	memsize;	/* global local memory size register (RW) */
	uint_t	rxsize;		/* global rx partition register (RW) */
	uint_t	txsize;		/* global tx partition register (RW) */
};

/*
 * QEC Global registers Bit Masks.
 * XXX add encoding for parity, arb, and burst later.
 */
#define	QECG_CONTROL_MODE	(0xf0000000)	/* mode mask */
#define	QECG_CONTROL_MACE	(0x40000000)	/* MACE mode */
#define	QECG_CONTROL_BMAC	(0x10000000)	/* BigMAC mode */
#define	QECG_CONTROL_PARITY	(0x00000020)	/* sbus parity enable/disable */
#define	QECG_CONTROL_ARB	(0x00000018)	/* bus arbitration control */
#define	QECG_CONTROL_BURST64	(0x00000004)	/* sbus max burst size 64 */
#define	QECG_CONTROL_BURST32	(0x00000002)	/* sbus max burst size 32 */
#define	QECG_CONTROL_BURST16	(0x00000000)	/* sbus max burst size 16 */
#define	QECG_CONTROL_RST	(0x00000001)	/* global reset */

#define	QECG_STATUS_TINT	(0x00000008)	/* TINT in BM mode */
#define	QECG_STATUS_RINT	(0x00000004)	/* RINT in BM mode */
#define	QECG_STATUS_BMINT	(0x00000002)	/* BM INT in BM mode */
#define	QECG_STATUS_QECERR	(0x00000001)	/* QEC ERR in BM mode */

#define	QECG_PKTSIZE_2K		(0x00)
#define	QECG_PKTSIZE_4K		(0x01)
#define	QECG_PKTSIZE_6K		(0x10)
#define	QECG_PKTSIZE_8K		(0x11)

/*
 * Pick out channel i bits from global status value.
 */
#define	QECCHANBITS(v, i)	((v >> (i << 2)) & 0xf)

/*
 * QEC Per-Channel register set (MACE mode).
 */
struct	qecm_chan {
	uint_t	control;	/* per channel control register (RW) */
	uint_t	status;		/* per channel status register (R) */
	uint_t	rxring;		/* rx descriptor ring base pointer (RW) */
	uint_t	txring;		/* tx descriptor ring base pointer (RW) */
	uint_t	rintm;		/* receive interrupt mask register (RW) */
	uint_t	tintm;		/* transmit interrupt mask register (RW) */
	uint_t	qecerrm;	/* QEC error mask register (RW) */
	uint_t	macerrm;	/* MACE error mask register (RW) */
	uint_t	lmrxwrite;	/* local memory rx write pointer (RW) */
	uint_t	lmrxread;	/* local memory rx read pointer (RW) */
	uint_t	lmtxwrite;	/* local memory tx write pointer (RW) */
	uint_t	lmtxread;	/* local memory tx read pointer (RW) */
	uint_t	coll;		/* collision error counter (RW) */
	uint_t	pifs;		/* programmable inter-frame space (RW) */
};

/*
 * QEC Per-Channel Register Bit Masks.
 */
#define	QECM_CONTROL_DRCV	(0x04)	/* receive disable */
#define	QECM_CONTROL_RST	(0x02)	/* channel reset */
#define	QECM_CONTROL_TDMD	(0x01)	/* channel transmit demand "go" */

#define	QECM_STATUS_EXDER	(0x10000000)	/* excessive defer */
#define	QECM_STATUS_LCAR	(0x08000000)	/* loss of carrier */
#define	QECM_STATUS_RTRY	(0x04000000)	/* >16 retries */
#define	QECM_STATUS_LCOL	(0x02000000)	/* late collision on transmit */
#define	QECM_STATUS_UFLO	(0x01000000)	/* MACE FIFO underflow */
#define	QECM_STATUS_JAB		(0x00800000)	/* MACE jabber error */
#define	QECM_STATUS_BABL	(0x00400000)	/* MACE >1518 babble error */
#define	QECM_STATUS_TINT	(0x00200000)	/* transmit interrupt */
#define	QECM_STATUS_COLCO	(0x00100000)	/* counter overflow */
#define	QECM_STATUS_TMDER	(0x00080000)	/* chained tx desc. error */
#define	QECM_STATUS_TXLATERR	(0x00040000)	/* sbus tx late error */
#define	QECM_STATUS_TXPARERR	(0x00020000)	/* sbus tx parity error */
#define	QECM_STATUS_TXERRACK	(0x00010000)	/* sbus tx error ack */
#define	QECM_STATUS_RVCCO	(0x00001000)	/* rx coll counter overflow */
#define	QECM_STATUS_RPCO	(0x00000800)	/* runt counter overflow */
#define	QECM_STATUS_MPCO	(0x00000400)	/* missed counter overflow */
#define	QECM_STATUS_OFLO	(0x00000200)	/* FIFO rx overflow */
#define	QECM_STATUS_CLSN	(0x00000100)	/* late collision */
#define	QECM_STATUS_FMC		(0x00000080)	/* fram counter overflow */
#define	QECM_STATUS_CRC		(0x00000040)	/* crc error counter overflow */
#define	QECM_STATUS_RINT	(0x00000020)	/* receive interrupt */
#define	QECM_STATUS_DROP	(0x00000010)	/* rx packet dropped */
#define	QECM_STATUS_BUFF	(0x00000008)	/* data buffer too small */
#define	QECM_STATUS_RXLATERR	(0x00000004)	/* sbus rx late error */
#define	QECM_STATUS_RXPARERR	(0x00000002)	/* sbus rx parity error */
#define	QECM_STATUS_RXERRACK	(0x00000001)	/* sbus rx error ack */

#define	QECM_STATUS_OTHER	(0x1fdf3fdf)	/* all except TINT, RINT */

#define	QECM_QECERRM_COLLM	(0x00100000)	/* coll counter overflow mask */
#define	QECM_QECERRM_TMDERM	(0x00080000)	/* tx descriptor error mask */
#define	QECM_QECERRM_TXLATERRM	(0x00040000)	/* sbus tx late error mask */
#define	QECM_QECERRM_TXPARERRM	(0x00020000)	/* sbus tx parity error mask */
#define	QECM_QECERRM_TXERRACKM	(0x00010000)	/* sbus tx error ack mask */
#define	QECM_QECERRM_DROPM	(0x00000010)	/* lmem rx packet drop mask */
#define	QECM_QECERRM_BUFFM	(0x00000008)	/* rx buffer error mask */
#define	QECM_QECERRM_RXLATERRM	(0x00000004)	/* sbus rx late error mask */
#define	QECM_QECERRM_RXPARERRM	(0x00000002)	/* sbus rx parity error mask */
#define	QECM_QECERRM_RXERRACKM	(0x00000001)	/* sbus rx error ack mask */

#define	QECM_MACERRM_EXDERM	(0x10000000)	/* excessive defer mask */
#define	QECM_MACERRM_LCARM	(0x08000000)	/* loss of carrier mask */
#define	QECM_MACERRM_RTRYM	(0x04000000)	/* >16 retries mask */
#define	QECM_MACERRM_LCOLM	(0x02000000)	/* late collision error mask */
#define	QECM_MACERRM_UFLOM	(0x01000000)	/* overflow error mask */
#define	QECM_MACERRM_JABM	(0x00800000)	/* jabber mask */
#define	QECM_MACERRM_BABLM	(0x00400000)	/* babble mask */
#define	QECM_MACERRM_OFLOM	(0x00000800)	/* overflow error mask */
#define	QECM_MACERRM_RVCCOM	(0x00000400)	/* rx collision overflow mask */
#define	QECM_MACERRM_RPCOM	(0x00000200)	/* runt packet overflow mask */
#define	QECM_MACERRM_MPCOM	(0x00000100)	/* missed overflow mask */

#define	QECM_PIFS_ENABLE	(0x00000020)	/* Throttle enable */
#define	QECM_PIFS_MANUAL	(0x00000010)	/* Throttle manual mode */
#define	QECM_PIFS_1920		(0x0000000f)	/* # of SBus wait */
#define	QECM_PIFS_1792		(0x0000000e)	/* # of SBus wait */
#define	QECM_PIFS_1664		(0x0000000d)	/* # of SBus wait */
#define	QECM_PIFS_1536		(0x0000000c)	/* # of SBus wait */
#define	QECM_PIFS_1408		(0x0000000b)	/* # of SBus wait */
#define	QECM_PIFS_1280		(0x0000000a)	/* # of SBus wait */
#define	QECM_PIFS_1152		(0x00000009)	/* # of SBus wait */
#define	QECM_PIFS_1024		(0x00000008)	/* # of SBus wait */
#define	QECM_PIFS_896		(0x00000007)	/* # of SBus wait */
#define	QECM_PIFS_768		(0x00000006)	/* # of SBus wait */
#define	QECM_PIFS_640		(0x00000005)	/* # of SBus wait */
#define	QECM_PIFS_512		(0x00000004)	/* # of SBus wait */
#define	QECM_PIFS_384		(0x00000003)	/* # of SBus wait */
#define	QECM_PIFS_256		(0x00000002)	/* # of SBus wait */
#define	QECM_PIFS_128		(0x00000001)	/* # of SBus wait */
#define	QECM_PIFS_2048		(0x00000000)	/* # of SBus wait */

/*
 * QEC Per-Channel register set (BigMAC mode).
 */
struct	qecb_chan {
	uint_t	control;	/* channel control register (RW) */
	uint_t	status;		/* channel status register (R) */
	uint_t	rxring;		/* rx descriptor ring base pointer (RW) */
	uint_t	txring;		/* tx descriptor ring base pointer (RW) */
	uint_t	rintm;		/* receive interrupt mask register (RW) */
	uint_t	tintm;		/* transmit interrupt mask register (RW) */
	uint_t	qecerrm;	/* QEC error mask register (RW) */
	uint_t	bmacerrm;	/* BigMAC error mask register (RW) */
	uint_t	lmrxwrite;	/* local memory rx write pointer (RW) */
	uint_t	lmrxread;	/* local memory rx read pointer (RW) */
	uint_t	lmtxwrite;	/* local memory tx write pointer (RW) */
	uint_t	lmtxread;	/* local memory tx read pointer (RW) */
	uint_t	coll;		/* XXX collision error counter (RW) */
};

/*
 * QEC Channel Register Bit Masks. (BigMAC mode)
 */
#define	QECB_CONTROL_TDMD	(0x01)	/* channel transmit demand "go" */

#define	QECB_STATUS_MACE	(0x80000000)	/* bmac error interrupt */
#define	QECB_STATUS_TINT	(0x00200000)	/* transmit interrupt */
#define	QECB_STATUS_TMDER	(0x00080000)	/* chained tx desc. error */
#define	QECB_STATUS_TXLATERR	(0x00040000)	/* sbus tx late error */
#define	QECB_STATUS_TXPARERR	(0x00020000)	/* sbus tx parity error */
#define	QECB_STATUS_TXERRACK	(0x00010000)	/* sbus tx error ack */
#define	QECB_STATUS_RINT	(0x00000020)	/* receive interrupt */
#define	QECB_STATUS_DROP	(0x00000010)	/* rx packet dropped */
#define	QECB_STATUS_BUFF	(0x00000008)	/* data buffer too small */
#define	QECB_STATUS_RXLATERR	(0x00000004)	/* sbus rx late error */
#define	QECB_STATUS_RXPARERR	(0x00000002)	/* sbus rx parity error */
#define	QECB_STATUS_RXERRACK	(0x00000001)	/* sbus rx error ack */

#define	QECB_STATUS_ERR		(0x800f001f)	/* all except TINT, RINT */
#define	QECB_STATUS_INTR	(0x802f003f)	/* all interrupt conditions */
#define	QECB_STATUS_QEC		(0x000f001f)	/* QEC interrupt conditions */

#define	QECB_QECERRM_TMDERM	(0x00080000)	/* tx descriptor error mask */
#define	QECB_QECERRM_TXLATERRM	(0x00040000)	/* sbus tx late error mask */
#define	QECB_QECERRM_TXPARERRM	(0x00020000)	/* sbus tx parity error mask */
#define	QECB_QECERRM_TXERRACKM	(0x00010000)	/* sbus tx error ack mask */
#define	QECB_QECERRM_DROPM	(0x00000010)	/* lmem rx packet drop mask */
#define	QECB_QECERRM_BUFFM	(0x00000008)	/* rx buffer error mask */
#define	QECB_QECERRM_RXLATERRM	(0x00000004)	/* sbus rx late error mask */
#define	QECB_QECERRM_RXPARERRM	(0x00000002)	/* sbus rx parity error mask */
#define	QECB_QECERRM_RXERRACKM	(0x00000001)	/* sbus rx error ack mask */

/*
 *	Definitions for the BigMAC Error Mask Register
 */
#define	QECB_BMACERRM_EXDERM	(0x00000001)    /* BigMAC error mask */

/*
 * QEC Rx/Tx Descriptor.
 * Must be aligned on 8-byte boundary.
 */
struct	qmd {
	uint_t qmd_flags;			/* OWN, SOP, EOP, size/length */
	uint_t qmd_addr;			/* buffer address */
};

/* flags */
#define	QMD_OWN		(0x80000000)		/* "own" bit */
#define	QMD_SOP 	(0x40000000)		/* Tx start of packet */
#define	QMD_EOP		(0x20000000)		/* Tx end of packet */
#define	QMD_INUSE	(0x10000000)		/* qmd being updated */
#define	QMD_MACE_BUFLEN	(0x7ff);		/* Tx/Rx pkt length - MACE */
#define	QMD_BMAC_BUFLEN	(0x1fff);		/* Tx/Rx pkt length - BMAC */

#define	QEC_QMDMAX	(256)			/* # Tx/Rx ring entries */

/*
 * Special alignment required for the message descriptors.  This will
 * increase iopb usage but reduce the number of gates in the QEC.
 */
#define	QEC_QMDALIGN	(2048)

/*
 * Definition for the time required to wait after a software
 * reset has been issued.
 */
#ifdef	MPSAS
#define	QECMAXRSTDELAY	(1000000)
#else
#define	QECMAXRSTDELAY	(100)
#endif	/* MPSAS */
#define	QECPERIOD	10	/* period to wait */
#define	QECWAITPERIOD	QECPERIOD

#define	QECDELAY(c, n) \
	{ \
		register int N = n / QECWAITPERIOD; \
		while (--N > 0) { \
			if (c) \
				break; \
			drv_usecwait(QECWAITPERIOD); \
		} \
	}

/*
 * QED information.  The QEC driver and its child (BigMac or QE)
 * communicate thru this data structure.  The QEC driver passes the
 * interrupt cookie to the child so the child can initialize its
 * mutexes.  The child registers its interrupt handler and the
 * parameter so that the QEC can call it.
 *
 * The QEC driver exports the pointer to it and the child imports the
 * pointer to it by using the QEC's private pointer.  An alternative
 * is for the QEC to create a property which is a pointer to this
 * data structure.
 */
struct	qec_soft	{
	uint_t	qs_nchan;			/* # of channels per card */
	uint_t	qs_memsize;			/* local mem size (in bytes) */
	void	(**qs_intr_func)();		/* child interrupt handler */
	void	**qs_intr_arg;			/* input argument array */
	uint_t	(*qs_reset_func)();		/* global reset function */
	void	*qs_reset_arg;			/* reset argument */
	int	(*qs_init_func)();		/* qec init function */
	dev_info_t	*qs_init_arg;		/* qec init argument */
	ddi_iblock_cookie_t	qs_cookie;	/* interrupt cookie */
	volatile	struct	qec_global	*qs_globregp;
						/* qec global regs */
	int	qe_intr_flag;			/* this flag is set in */
						/* qeinit().  bug 1204247 */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_QEC_H */
