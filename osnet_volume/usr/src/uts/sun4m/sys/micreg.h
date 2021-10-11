/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MICREG_H
#define	_SYS_MICREG_H

#pragma ident	"@(#)micreg.h	1.5	96/04/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The MIC chip's register's are divided up into 2 banks of four register
 * sets. Each bank corresponds to a serial port (port A and port B). The
 * four sets in each bank are comprised of:
 *	Prom/NVRAM
 *	Staus Register set
 *	Control Register set (including transmit FIFO)
 *	SCC Register Set
 *
 * The Prom/NVRAM registers are not mapped in and are not available for use.
 * The other sets have the structure and form shown below.  The SCC is the
 * the 16C450 from National Semiconductor. For more detail see the relevent
 * chip specifications.
 */

/*
 * Status register set
 */
struct mic_stat {
	uchar_t		id : 7;		/* MIC ID */
	uchar_t		sbus_int : 1;	/* Sbus interrupt */
	uchar_t		isr;		/* interrupt source */
	uchar_t		rx_cnt;		/* Rx FIFO count */
	uchar_t		tx_cnt;		/* Tx FIFO count */
	ushort_t	: 16;
	ushort_t	tmo_cnt;	/* time out count */
	uchar_t		rx_stat;	/* Rx FIFO data status */
	uchar_t		rx_data;	/* Rx FIFO data (pops fifo stack) */
};

/*
 * Control register set
 */
struct mic_ctl {
	uchar_t 	gen_ie;		/* general interrupt enable */
	uchar_t		flw_ctl;	/* flow control */
	uchar_t		rx_water;	/* Rx fifo upper level (interrupt) */
	uchar_t		tx_water;	/* Tx fifo lower level (interrupt) */
	ushort_t	rx_timer;	/* Stale Rx data timeout counter */
	uchar_t		misc_ctl;	/* clear/disable fifo's */
	uchar_t		test_ctl;	/* test control */
	uchar_t		tx_data;	/* Tx FIFO data */
	uchar_t		: 8;
	uchar_t		ir_div;		/* divisor for IR modulation */
	uchar_t		ir_mode;	/* select IR mode */
};

/*
 * SCC register set (16C450)
 */
struct mic_scc {
	union {
		struct {
			uchar_t	data;		/* data buffer */
			uchar_t	intr_en;	/* interrupt enable */
		} normal;
		struct {
			uchar_t	dllsb;		/* divisor latch lsb */
			uchar_t	dlmsb;		/* divisor latch msb */
		} divisor;
	} overlay;
	uchar_t		: 8;
	uchar_t		lcr;		/* line control */
	uchar_t		mcr;		/* modem control */
	uchar_t		: 8;
	uchar_t		msr;		/* modem status */
};
#define	rbr	overlay.normal.data
#define	thr	overlay.normal.data
#define	dll	overlay.divisor.dllsb
#define	dlm	overlay.divisor.dlmsb
#define	ier	overlay.normal.intr_en

/*
 * MIC Status Registers
 */
/* Interrupt Source register */
#define	MIC_TXWATER_I		0x01	/* Transmitter fifo empty interrupt */
#define	MIC_RXWATER_I		0x02	/* Recieve fifo full interrupt */
#define	MIC_TIMEOUT_I		0x04	/* Revieve fifo timeout interrupt */
#define	MIC_SCC_I		0x08	/* UART Modem control interrupt */
#define	MIC_TXEMPTY_I		0x10	/* UART TX empty */

/* Receive FIFO Data Status */
#define	MIC_EMPTY		0x00	/* Rx FIFO is empty */
#define	MIC_VALID		0x01	/* Data is valid */
#define	MIC_FRAME		0x02	/* Framing error */
#define	MIC_PARITY		0x03	/* Parity error */
#define	MIC_PARFR		0x04	/* Parity and Framing error */
#define	MIC_BREAK		0x05	/* Break */
#define	MIC_OVERRUN		0x06	/* FIFO was overrun */
#define	MIC_UNDEF		0x07	/* Undefined */

/*
 * MIC Control Registers
 */
/* Interrupt Enable register */
#define	MIC_MASTER_IE		0x01	/* master interrupt enable */
#define	MIC_TXWATER_IE		0x02	/* transmit water mark enable */
#define	MIC_RXWATER_IE		0x04	/* receive water mark enable */
#define	MIC_TIMEOUT_IE		0x08	/* receive timeout enable */
#define	MIC_TXEMPTY_IE		0x10	/* transmit empty interrupt */

#define	MIC_INTR_MASK		0x1f	/* mask for all interrupts */

/* Flow Control register */
#define	MIC_RTS_AUTOEN		0x01	/* Enables automatic control of RTS */
#define	MIC_RTS_STAT		0x02	/* RTS line status */
#define	MIC_CTS_AUTOEN		0x04	/* Enables automatic control of CTS */
#define	MIC_CTS_STAT		0x08	/* CTS line status */

/* Miscellaneous Control register */
#define	MIC_SCC_PROG_MODE 	0x04	/* disable fifo during DLAB */
#define	MIC_CLR_RX 		0x02	/* clear rx fifo */
#define	MIC_CLR_TX 		0x01	/* clear tx fifo */

/* Test Control register */
#define	MIC_TMO_CNT_TST		0x04	/* timeout counter test mode */
#define	MIC_RTS_CTS_LOOP	0x02	/* internally loops RTS to CTS */
#define	MIC_TXFF_TO_RXFF	0x01	/* moves Tx data to Rx Fifo */

/* Infra Red Mode Register */
#define	MIC_IR_OFF		0x00	/* Disable IR (use port A) */
#define	MIC_IR_PULSE		0x05	/* Pulse mode (Rx and Tx) */
#define	MIC_IR_HI		0x0a	/* Hi frequency (Tx and Rx) */
#define	MIC_IR_LO		0x0f	/* Lo frequency (Tx and Rx) */

/*
 * SCC Registers
 */
/* Interrupt Enable Register */
#define	SCC_STAT_IE		0x08	/* Modem Status */

/* Line Control Register */
#define	WLS0		0x01	/* word length select bit 0 */
#define	WLS1		0x02	/* word length select bit 2 */
#define	STB2		0x04	/* number of stop bits */
#define	PEN		0x08	/* parity enable */
#define	EVENPAR		0x10	/* even parity select */
#define	STKPAR		0x20	/* stick parity select */
#define	SETBREAK 	0x40	/* break key */
#define	DLAB		0x80	/* divisor latch access bit */

#define	BITS5		0x00	/* 5 bits per char */
#define	BITS6		0x01	/* 6 bits per char */
#define	BITS7		0x02	/* 7 bits per char */
#define	BITS8		0x03	/* 8 bits per char */

/* Modem Control Register */
#define	DTR		0x01	/* Data Terminal Ready */
#define	OUT1		0x04	/* Aux output - not used */
#define	OUT2		0x08	/* turns intr to 386 on/off */
#define	SCC_LOOP	0x10	/* loopback for diagnostics */

/* Modem Status Register */
#define	DDSR		0x02	/* Delta Data Set Ready */
#define	DRI		0x04	/* Trail Edge Ring Indicator */
#define	DDCD		0x08	/* Delta Data Carrier Detect */
#define	DSR		0x20	/* Data Set Ready */
#define	RI		0x40	/* Ring Indicator */
#define	DCD		0x80	/* Data Carrier Detect */

/* baud rate definitions */
#define	ASY110		0x2ba2	/* 110 baud rate for serial console */
#define	ASY150		0x2000	/* 150 baud rate for serial console */
#define	ASY300		0x1000	/* 300 baud rate for serial console */
#define	ASY600		0x800	/* 600 baud rate for serial console */
#define	ASY1200		0x400	/* 1200 baud rate for serial console */
#define	ASY2400		0x200	/* 2400 baud rate for serial console */
#define	ASY4800		0x100	/* 4800 baud rate for serial console */
#define	ASY7200		0xab	/* 4800 baud rate for serial console */
#define	ASY9600		0x80	/* 9600 baud rate for serial console */
#define	ASY14400	0x55	/* ...You get the idea. */
#define	ASY19200	0x40
#define	ASY38400	0x20
#define	ASY57600	0x15
#define	ASY76800	0x10
#define	ASY96000	0xd
#define	ASY115200	0xb
#define	ASY153600	0x8

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MICREG_H */
