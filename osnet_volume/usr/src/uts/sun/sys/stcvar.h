/*
 * Copyright (c) 1990-1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STCVAR_H
#define	_SYS_STCVAR_H

#pragma ident	"@(#)stcvar.h	1.63	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Private data structures for SBus 8-port serial communications card
 * using the Cirrus Logic CD-180 octal UART plus an 8-bit parallel printer
 * port using the Paradise PPC-2
 *
 * various device things
 */
#define	STC_UNIT(dev)		(((getminor(dev) & 0x038)>>3)&0x07)
#define	STC_LINE(dev)		(getminor(dev) & 0x07)
#define	STC_OUTLINE(dev)	(getminor(dev) & 0x80)
#define	STC_LP_LINE(dev)	(getminor(dev) & 0x40)
#define	STC_CONTROL_LINE(dev)	((getminor(dev) & 0x40) &&	\
					((getminor(dev)&7) == 7))

#define	N_STC			4	/* hint to ddi_soft_state_init() */

/*
 * driver ID string
 */
#define	STC_DRIVERID	"V2.1"

/*
 * device name stuff used in stc_attach() and stc_detach() to
 *	automatically create the device nodes
 */
#define	STC_DONODE(u, l)	((u<<3) | ((l&7) | 0x80))	/* dial-out */
#define	STC_DINODE(u, l)	((u<<3) | (l&7))		/* dial-in  */
#define	STC_PPCNODE(u)		((u<<3) | 0x40)
#define	STC_STCNODE(u)		((u<<3) | 0x47)
#define	STC_PPCNAME		"stclp"
#define	STC_STCNAME		"stc"
#define	STC_NT_PARALLEL		"ddi_parallel"	/* until real DDI_NT_PARALLEL */
#define	STC_SERIAL_PROP_ID	"stc_"
#define	STC_PARALLEL_PROP_ID	"stc_p"

#define	STC_MAXPROPLEN		20

/*
 * various properties returned from the FCode PROM - NOTE: These MUST jive
 *	with whatever's in the PROM
 * references to V1.1, "new" and "STC_FCS2_REV" all refer to the same PROM
 * the revision levels are all 4-byte ints, and we get a pointer to
 *	them from getlongprop()
 */
#define	STC_NAME	"SUNW,spif"	/* name in V1.1 FCode PROM on board */
#define	STC_REV_PROP	"revlev"	/* dev entry for board revision level */
#define	OSC_REV_PROP	"verosc"	/* dev entry for oscillator rev level */
#define	OSC_10		2		/* oscillator revision for 10Mhz */
#define	OSC_9		1		/* oscillator revision for 9....Mhz */
#define	NUM_INTS	2		/* number of interrupt levels */
#define	NUM_REGS	1		/* number of interrupt levels */
#define	STC_FCS2_REV	5		/* revision level with V1.1 PROM */
#define	PPC_INTMASK	0x0a		/* bits always to be 1 from pstat */

/*
 * the driver uses a two-level interrupt scheme; the serial port hardware
 *	interrupts on SBus level 7, but the driver schedules a softint on
 *	a lower level to deal with the STREAMS processing
 * the parallel port hardware interrupts on SBus level 4
 */
#define	STC_SOFT_PREF	DDI_SOFTINT_MED	/* soft interrupt level */
#define	STC_INT_NUM	0		/* first interrupt property */
#define	PPC_INT_NUM	1		/* second interrupt property */

/*
 * some debugging stuff
 */
#define	STC_DEBUG_DEFLEVEL	1	/* was 10 */
#define	STC_TRACK		5
#define	STC_DEBUG		9
#define	STC_TESTING		3

/*
 * default parsing routine return values
 */

#define	STC_EXTRACT_FAILURE	-1
#define	STC_BOOLEAN		0
#define	STC_VALUED		1

/*
 * some general sizing and enumeration constants
 * the silo sizes are the same for both the cd180 and the ppc, and are
 *	located in stcio.h as STC_SILOSIZE
 * the TXBUF sizes determine how many characters stc_start() will try to
 *	stuff into the soft tx buffers in the line struct.  you should leave
 *	STC_TXBUFSIZE at 8 since the interrupt handler for the cd180 doesn't
 *	know what to do if it gets larger.
 * PPC_TXBUFSIZE is made a little larger since the ppc interrupt handler
 *	is smart enough to take data from the soft tx buffer if there is any
 *	both STC_TXBUFSIZE and PPC_TXBUFSIZE must be <= to LINE_TXBUFSIZE
 */
#define	STC_LINES	8	/* number of serial lines per board */
#define	PPC_LINES	1	/* number of parallel lines per board */
#define	STC_TXBUFSIZE	8	/* size of tx buffer use in stc_start() */
#define	STC_HIWATER	1010	/* when to disable RTS with flow control */
#define	STC_LOWWATER	512	/* when to enable RTS with flow control */
#define	LINE_TXBUFSIZE	16	/* size of (soft) tx buffer in line struct */
#define	PPC_TXBUFSIZE	LINE_TXBUFSIZE	/* size of tx buffer in stc_start() */
#define	STC_DRAIN_BSIZE	64	/* STREAMS buffer size in stc_drainsilo() */
#define	STC_RTPR	18	/* RTPR, chosen for good interactive response */
#define	RX_FIFO_SIZE	4	/* Rx bytes to pool in cd-180 before Rx int */

/*
 * timeout and timing parameters
 *
 * serial lines
 *	NQFRETRY and QFRETRYTIME are used in stc_drainsilo()
 */
#define	STC_TIMEOUT	(hz*15)	/* timeout in stc_close() for serial lines */
#define	NQFRETRY	10	/* number of tries to put data in rcv queue */
#define	QFRETRYTIME	(hz*4)	/* time to wait between above trys */

/*
 * parallel line
 *	many of these timing parameters can be changed; see the "stcio.h"
 *	header file for the appropriate structures and ioctl() definitions
 *
 * the following five defines are used in "stcconf.h" and are the default
 *	values for the parallel port
 */
#define	PPC_STROBE_W	2	/* strobe width, in uS */
#define	PPC_DATA_SETUP	2	/* delay from data available to STROBE, in uS */
#define	PPC_ACK_TIMEOUT	60	/* time to wait for ACK, in seconds */
#define	PPC_ERR_TIMEOUT	5	/* time between error checks in seconds */
#define	PPC_BSY_TIMEOUT	10	/* time to wait for BUSY, in seconds */

/*
 * these are constants used to convert seconds<->ticks when setting user
 *	supplied timing default values
 */
#define	PPC_ACK_POLL	5	/* each poll to ppc_acktimeout() in seconds */
#define	PPC_ERRMSG_TIME	40	/* time to print err msgs in ppc_stat() */
#define	BUSY_LOOP_C	200000	/* times to loop waiting for busy to go away */
#define	BUSY_COUNT_C	10	/* times to timeout for busy to go away */
#define	NOBUSY_POLLTIME	5	/* time to check for no BUSY timeout, in sec */

/*
 * UNTIMEOUT() macro to make sure we're not trying untimeout a bogus timeout
 */
#define	UNTIMEOUT(utt) {		\
	if (utt) {			\
	    (void) untimeout(utt);	\
	    utt = 0;			\
	}				\
}

/*
 * -------------------- cd180 stuff --------------------
 *
 * define the PILR register values - it's best not to change these unless
 * you really know what you're doing; see stcreg.h for a description of
 * what these mean
 */
#define	PILR1		0x80	/* group 1 */
#define	PILR2		0x81	/* group 2 */
#define	PILR3		0x82	/* group 3 */

/*
 * prescaler period; this affects recieve timeouts and breaks (we only
 * use it for break timing)
 *
 * BREAK time in mS = ((PPRH<<8) + PPRL) * DELAY * (1/(clock*1000))
 * DELAY and clock are from embedded XMIT constants
 */
#define	PPRH		0x0f0
#define	PPRL		0x00
#define	BRK_DELAY	0x28	/* about 250mS */

/*
 * these values come back from the GIVR when we read from the *IACK space
 * during the interrupt handler (stc_poll()); see pp. 23 of the CD-180 spec
 */
#define	MODEM_CHANGE	1	/* group 1 */
#define	XMIT_DATA	2	/* group 2 */
#define	RCV_DATA	3	/* group 3 */
#define	RCV_EXP		7	/* group 3 */

/*
 * misc stuff
 */
#define	INIT_ALL	-1	/* cd180_init() to reset chip and init all */

/*
 * magic bits for the various CD-180 registers
 *
 * IER
 */
#define	TX_RDY		0x04	/* transmitter fifo empty */
#define	TX_MPTY		0x02	/* xmtr fifo, shift reg and holding reg empty */
#define	RX_DATA		0x10	/* receive data */
#define	RX_RET		0x01	/* receive exception timeout */
#define	CD_INT		0x40	/* CD changed state */

/*
 * RCSR
 */
#define	RX_TIMEOUT	0x80	/* timeout occured */
#define	RX_BREAK	0x08	/* BREAK detected */
#define	RX_PARITY	0x04	/* parity error */
#define	RX_FRAMING	0x02	/* framing error */
#define	RX_OVERRUN	0x01	/* overrun error */

/*
 * MCR
 */
#define	CD_CHANGE	0x40	/* CD changed state */

/*
 * MSVR
 */
#define	CD_ON		0x40	/* CD is asserted */
#define	CTS_ON		0x20	/* CTS is asserted */
#define	DSR_ON		0x80	/* DSR is asserted */
#define	RTS_ON		0x01	/* assert RTS */

/*
 * CCSR
 */
#define	RX_ENA		0x80	/* receiver is enabled */
#define	TX_ENA		0x08	/* transmitter is enabled */
#define	TX_FLOFF	0x04	/* Tx waiting on flow control */
#define	TX_FLON		0x02	/* remote asked Tx to start transmitting */

/*
 * CCR
 */
#define	CHAN_CTL	0x10
#define	XMTR_DIS	0x04
#define	XMTR_ENA	0x08
#define	RCVR_DIS	0x01
#define	RCVR_ENA	0x02
#define	COR_CHANGED	0x40
#define	COR1_CHANGED	0x02
#define	COR2_CHANGED	0x04
#define	COR3_CHANGED	0x08
#define	SEND_SPEC1	0x21	/* send special char 1 (XON) command */
#define	SEND_SPEC2	0x22	/* send special char 2 (XOFF) command */
#define	CD180_RESET_ALL	0x81	/* reset all channels command */
#define	RESET_CHAN	0x80	/* reset individual channel (from CAR) */

/*
 * the SET_CCR()/SET_COR() macros are used because we must wait until the CCR
 * is 0 before writing another command to it
 */
#define	SET_CCR(ccr, value)	{					\
				    int i = 0;				\
					ccr = value;			\
					while (ccr)			\
					    if (i++ > 1000000) {	\
						cmn_err(CE_CONT,	\
						    "SET_CCR: CCR timeout\n"); \
						break;			\
					    }				\
				}

#define	SET_COR(ccr, cor, num, value)	{ cor = value; 			\
					    SET_CCR(ccr, (COR_CHANGED | num)); }

/*
 * baud rate conversion macros, table from pp. 10 of CD-180 spec sheet
 */
#define	HI_BAUD(stc, baud)	((stc->stc_baud[baud&0x0f]>>8)&0x0ff)
#define	LO_BAUD(stc, baud)	(stc->stc_baud[baud&0x0f]&0x0ff)

/*
 *
 * COR1
 */
#define	EVEN_P		0x00
#define	ODD_P		0x80
#define	NO_P		0x00
#define	FORCE_P		0x20
#define	NORMAL_P	0x40
#define	IGNORE_P	0x10
#define	USE_P		0x00
#define	STOP_1		0x00
#define	STOP_15		0x04
#define	STOP_2		0x08
#define	CHAR_8		0x03
#define	CHAR_7		0x02
#define	CHAR_6		0x01
#define	CHAR_5		0x00

/*
 * COR2
 */
#define	FLOW_RTSCTS	0x02	/* use RTS/CTS h/w flow control */
#define	FLOW_IXON	0x40	/* transmitter uses XON/XOFF flow control */
#define	FLOW_IXANY	0x80	/* any char restarts transmitter after XOFF */
#define	EMBED_XMIT	0x20	/* enable embedded xmit commands for BREAK */
#define	EMBED_XMIT_LEN	7	/* length of embedded xmit command for BREAK */

/*
 * COR3
 */
#define	XON_1		0x00
#define	XON_13		0x80
#define	XOFF_2		0x00
#define	XOFF_24		0x40
#define	FLOW_EXP	0x00
#define	NO_FLOW_EXP	0x20
#define	NO_SCHR_DET	0x00
#define	SCHR_DET	0x10
#define	RX_FIFO_MASK	0x0f

/*
 * MCOR1 - specify 1->0 transition detection for modem control
 *		line interrupts
 */
#define	CD_ZD		0x40	/* detect 1->0 transition on CD */

/*
 * MCOR2 - specify 0->1 transition detection for modem control
 *		line interrupts
 */
#define	CD_OD		0x40	/* detect 0->1 transition on CD */

/*
 * DTR latch values - these are opposite of what we see on the
 * connector
 */
#define	DTR_OFF		0x01	/* drop DTR */
#define	DTR_ON		0x00	/* assert DTR */

/*
 * macro to set/clear DTR - we use this so that we can keep track of
 * the DTR pin to report back to the user in an ioctl() if they ask
 * for it; unfortunately, we can't read back the status of the
 * DTR latch on the board (ungh!)
 */
#define	DTR_SET(L, S) {							\
	if (L->dtr) {							\
		if (L->flags & STC_DTRFORCE) {				\
			*(L->dtr) = DTR_ON;				\
			L->dtr_shadow = (uchar_t)DTR_ON;		\
		} else {						\
			*(L->dtr) = S;					\
			L->dtr_shadow = (uchar_t)S;			\
		}							\
	} else {							\
		cmn_err(CE_CONT,					\
			"stc%d: line %d NULL pointer in DTR_SET(0x%p, %d)\n", \
			L->unit_no, L->line_no, (void *)L, S);		\
	}								\
}

/*
 * and a macro to get the shadow state of the line's DTR pin
 */
#define	DTR_GET(L)	(((L->dtr_shadow)&DTR_OFF)?0:1)

/*
 * define driver defaults for all the serial lines; these can be manipulated
 * via the STC_SDEFAULTS/STC_GDEFAULTS ioctl()'s; see "stcio.h"
 */
#define	SDFLAGS		(DTR_ASSERT|SOFT_CARR)	/* use zs style DTR on close */
#define	CFLAGS		(CS8|CREAD|HUPCL)	/* UNIX line flags in t_cflag */
#define	COR1		(EVEN_P | NO_P | IGNORE_P | STOP_1 | CHAR_8)
#define	COR2		0x00
#define	COR3		(XON_1 | XOFF_2 | FLOW_EXP | NO_SCHR_DET | RX_FIFO_SIZE)
#define	SCHR1		0x11	/* single Xon or first 1/2 of Xon sequence */
#define	SCHR2		0x13	/* single Xoff or first 1/2 of Xoff sequence */
#define	SCHR3		0x00	/* second 1/2 of Xon sequence */
#define	SCHR4		0x00	/* second 1/2 of Xoff sequence */
#define	MCOR1		CD_ZD	/* detect 1->0 modem control line transitions */
#define	MCOR2		CD_OD	/* detect 0->1 modem control line transitions */
#define	RX_BAUD		B9600	/* default rcvr baud rate from sys/ttydev.h */
#define	TX_BAUD		B9600	/* default xmtr baud rate from sys/ttydev.h */

/*
 * the soft receive silo handling macro for the cd180 and ppc read side
 * interrupt handlers all the bytes we get from the cd180 and the ppc get
 * put into a soft silo before being handed off to streams
 *
 * handle the RTS line if we're using CTS/RTS flow control; in the SunOS 4.X
 * version of this code, we had an array of stc_unit_t structs that we could
 * dereferance via the unit_no 	member of the stc_line_t struct; in the SunOS
 * 5.X version of this code, we don't, so I added a pointer in each stc_line_t
 * struct to the stc_unit_t struct for that line FLUSHSILO(line) is used to
 * flush the silo in case there's an error
 *
 * if we're using CTS/RTS, then enable RTS so that data can come in again
 */
#define	CHECK_RTS(line) { \
    uchar_t car; \
	if (line->stc_ttycommon.t_cflag & CRTSCTS) { \
	    car = line->stc->regs->car; \
	    line->stc->regs->car = line->line_no; \
	    if (line->stc_sscnt < line->stc_lowwater) \
		line->stc->regs->msvr |= RTS_ON; \
	    else if (line->stc_sscnt > line->stc_hiwater) \
		line->stc->regs->msvr &= ~RTS_ON; \
	    line->stc->regs->car = car; \
	} \
}

#define	FLUSHSILO(line) { \
	line->stc_source = line->stc_sink = line->stc_ssilo; \
	line->stc_sscnt = 0; \
	CHECK_RTS(line); \
}

#define	PUTSILO(zelineo, ch) {					\
	if (zelineo->stc_sscnt < STC_SILOSIZE) {			\
		++zelineo->stc_sscnt;					\
		if (zelineo->stc_source == &zelineo->stc_ssilo[STC_SILOSIZE]) \
			zelineo->stc_source = zelineo->stc_ssilo;	\
		*zelineo->stc_source++ = (unsigned char)ch;		\
		CHECK_RTS(zelineo);					\
	} else {							\
		FLUSHSILO(zelineo);					\
		cmn_err(CE_CONT,					\
			"stc_putsilo: unit %d line %d soft silo overflow\n", \
			zelineo->unit_no, zelineo->line_no);		\
	}								\
}

/*
 * -------------------- ppc stuff --------------------
 *
 * bits in pstat, the status register, along with macros to read the various
 * signals qualified with a flags field (fl) that usually comes from the
 * line->flags field of the line struct - this lets the user specify which
 * signals they want to observe.  the macros do the right thing if their
 * signal is specified to be ignored
 * these bits are all r/o
 */
#define	PPC_ERROR_B		0x08	/* ERRN bit */
#define	PPC_ERROR(pstat, fl)	(!(pstat & PPC_ERROR_B) && (fl & PP_ERROR))
#define	PPC_SELECT_IN_B		0x10	/* SCLT bit, driven by the printer */
#define	PPC_SELECT(pstat, fl)	((pstat & PPC_SELECT_IN_B) || !(fl & PP_SELECT))
#define	PPC_PAPER_OUT_B		0x20	/* PAPER bit */
#define	PPC_PAPER_OUT(pstat, fl)	((pstat & PPC_PAPER_OUT_B) && \
				(fl & PP_PAPER_OUT))
#define	PPC_ACK_B		0x40	/* ACKN bit, from the printer */
#define	PPC_BUSY_B		0x80	/* BUSY bit */
#define	PPC_BUSY(pstat, fl)	(!(pstat & PPC_BUSY_B) && (fl & PP_BUSY))

/*
 * bits in pcon, the control register.  these are all r/w
 */
#define	PPC_STROBE	0x01	/* STROBE bit, high means drop STROBE */
#define	PPC_AFX		0x02	/* auto form-feed */
#define	PPC_INIT	0x04	/* ININ bit, high means enable printer */
#define	PPC_SLCT	0x08	/* SLC bit, high means select printer */
#define	PPC_IRQE	0x10	/* IRQE bit, high means enable ppc interrupts */
#define	PPC_OUTPUT	0x20	/* direction control, high means ppc is out */

/*
 * initial settings of the control register - you probably don't want to
 * change these
 *	PCON_INIT - this is put into pcon in stc_init()
 *	PCON_OPEN_INIT - this is stuffed into pcon in stc_open() via
 *		default_param.pcon
 */
#define	PCON_INIT	(PPC_OUTPUT | PPC_INIT | PPC_SLCT)
#define	PCON_OPEN_INIT	(PPC_IRQE | PPC_OUTPUT | PPC_INIT | PPC_SLCT)

/*
 * default ppc interface lines to monitor
 * default is NOT to send a PP_SIGTYPE signal if we detect an error
 * and to print error messages only once on the console
 * (these bits are defined in stcio.h)
 */
#define	PDFLAGS		(PP_PAPER_OUT | PP_BUSY | PP_SELECT | PP_ERROR)

/*
 * macro to diddle with the STROBE line; pcon should be the ppc's control
 * register and strobe_w is the width of the strobe pulse, in uS
 * pcon_s is the soft copy of pcon from the line struct
 * the driver default strobe width is in PPC_STROBE_W, defined above
 */
#define	STROBE(pcon, strobe_w, pcon_s) { \
	pcon_s |= PPC_STROBE; \
	pcon = pcon_s; \
	DELAY(strobe_w); \
	pcon_s &= ~PPC_STROBE; \
	pcon = pcon_s; \
		}

/*
 * macro that returns !0 if it's time to print an error message in ppc_stat()
 */
#define	DO_PPC_ERRMSG(line)	((line->flags & PP_MSG) && !(line->error_cnt))

/*
 * -------------------- general board stuff --------------------
 *
 * Per line default parameter structure
 *
 * Note that we can establish default values for the line parameters
 * on a per-line basis, rather than a per-board or per-driver basis.
 * This lets us set up line defaults when we modload the driver rather
 * than at compile time.
 */
struct stc_lineparam_t {
	struct stc_defaults_t	sd;	/* defaults struct that user sees */
	/* ppc-specific stuff */
	uchar_t		pcon;		/* ppc's pcon register default value */
	int		ack_maxloops;	/* used in ppc_acktimeout() */
	/* cd-180 specific stuff */
	uchar_t		cor1;		/* parity/stop bits/char length */
	uchar_t		cor2;		/* flow control/loopback/embed xmit */
	uchar_t		cor3;		/* s/w flow cntl/schar detect/Rx fifo */
	uchar_t		schr1;		/* SCHR1- Special Character Reg 1 */
	uchar_t		schr2;		/* SCHR2- Special Character Reg 2 */
	uchar_t		schr3;		/* SCHR3- Special Character Reg 3 */
	uchar_t		schr4;		/* SCHR4- Special Character Reg 4 */
	uchar_t		mcor1;		/* modem pins to monitor */
	uchar_t		mcor2;		/* modem pins to monitor */
	uchar_t		rbprh;		/* RBPRH-Rcv Baud Rate Period Reg Hi */
	uchar_t		rbprl;		/* RBPRL-Rcv Baud Rate Period Reg Lo */
	uchar_t		tbprh;		/* TBPRH-Xmit Baud Rate Period Reg Hi */
	uchar_t		tbprl;		/* TBPRL-Xmit Baud Rate Period Reg Lo */
	uchar_t		rtpr;		/* inter-character receive timer */
};

/*
 * Per line structure
 * there is one of these for each serial line plus one more for
 * the ppc.
 */
struct stc_line_t {
	/* stuff common to both the cd180 and the ppc */
	unsigned		state;		/* various state flags */
	unsigned		flags;		/* default mode flags */
	tty_common_t		stc_ttycommon;	/* common tty driver stuff */
	unsigned		unit_no;	/* for printf's and things */
	unsigned		line_no;	/* ditto... */
	dev_t			*dev;		/* to fix concurrent open */
	timeout_id_t		stc_timeout_id;		/* timeout id */
	timeout_id_t		stc_draintimeout_id;	/* timeout id */
	timeout_id_t		ppc_acktimeout_id;	/* timeout id */
	timeout_id_t		ppc_out_id;		/* timeout id */
	struct stc_unit_t	*stc;		/* do this for PUTSILO */
	kcondvar_t		*cvp;		/* used for cv_wait_sig() */
	kmutex_t		*line_mutex;	/* stc_line_t mutex */
	/* cd180 speific stuff */
	uchar_t			*dtr;		/* DTR latch reg, LSB only */
	uchar_t			dtr_shadow;	/* shadow of DTR latch */
	uchar_t			stc_flowc;	/* flow control character */
	int			stc_txbufsize;	/* size of (soft) Tx buffer */
	uchar_t			stc_txbuf[LINE_TXBUFSIZE]; /* soft tx buffer */
	int			stc_txcount;	/* chars in stc_txbuf */
	int			stc_silosize;	/* size of rx silo */
	int			stc_sscnt;	/* silo count */
	uchar_t			*stc_source;	/* silo source */
	uchar_t			*stc_sink;	/* silo sink */
	uchar_t			stc_ssilo[STC_SILOSIZE];	/* soft silo */
	int			stc_qfcnt;	/* queue full retry count */
	bufcall_id_t		stc_wbufcid;	/* id of write-side bufcall */

	/* stuff that affects  the reception of data */
	int			drain_size;	/* STREAMs size in drainsilo */
	int			stc_hiwater;	/* CHECK_RTS() macro hiwater */
	int			stc_lowwater;	/* CHECK_RTS() macro lowater */
	int			rx_fifo_thld;	/* cd-180 RxFIFO threshold */
	struct stc_lineparam_t	default_param;	/* default line parameters */
	/* ppc specific stuff */
	int			strobe_w;	/* strobe width, in uS */
	int			data_setup;	/* data setup time, in uS */
	int			ack_timeout;	/* ACK timeout, in secs */
	int			ack_loops;	/* ppc_acktimeout() loops  */
	int			ack_maxloops;	/* max loop value for above */
	uchar_t			pcon_s;		/* soft copy of control reg */
	uchar_t			*stc_txbufp;	/* ptr into soft tx buffer */
	int			pstate;		/* current printer state */
	int			error_timeout;	/* ERROR timeout, in seconds */
	int			error_cnt;	/* index for msgs/signals */
	int			busy_timeout;	/* BUSY timeout, in seconds */
	int			busy_cnt;	/* to do a BUSY timeout */
	struct	stc_stats_t	stc_stats;	/* for STC_GSTATS ioctl() */
};

/*
 * flags in stc_line_t.state field
 */
#define	STC_WOPEN	0x00000001	/* waiting for open to complete */
#define	STC_ISOPEN	0x00000002	/* open is complete */
#define	STC_OUT		0x00000004	/* line being used for dialout */
#define	STC_CARR_ON	0x00000008	/* carrier on last time we looked */
#define	STC_XCLUDE	0x00000010	/* device is open for exclusive use */
#define	STC_STOPPED	0x00000020	/* output is stopped */
#define	STC_DELAY	0x00000040	/* waiting for delay to finish */
#define	STC_BREAK	0x00000080	/* waiting for break to finish */
#define	STC_BUSY	0x00000100	/* waiting for transmission to end */
#define	STC_FLUSH	0x00000200	/* flushing output being transmitted */
#define	STC_OPEN_INH	0x00000400	/* don't allow opens on this port */
#define	STC_WCLOSE	0x00000800	/* wakeup from close() in open() */
#define	STC_XWAIT	0x00001000	/* waiting for xmtr to drain */
#define	STC_IXOFF	0x00002000	/* using s/w receive flow control */
#define	STC_CANWAIT	0x00004000	/* waiting in stc_drainsilo() */
#define	STC_CONTROL	0x00008000	/* control line */
#define	STC_SBREAK	0x00010000	/* start BREAK */
#define	STC_EBREAK	0x00020000	/* end BREAK */
#define	STC_ISROOT	0x00040000	/* root open()ed this line */
#define	STC_STARTED	0x00080000	/* this line is already started */
/* flags used with stc_softint() */
#define	STC_TXWORK	0x00100000	/* line has some Tx work to do */
#define	STC_RXWORK	0x00200000	/* line has some Rx work to do */
#define	STC_CVBROADCAST	0x00400000	/* do a hi level intr cv_broadcast() */
#define	STC_UNTIMEOUT	0x00800000	/* do a hi level intr untimeout() */
#define	STC_MHANGUP	0x01000000	/* send an M_HANGUP downstream */
#define	STC_MUNHANGUP	0x02000000	/* send an M_UNHANGUP downstream */
#define	STC_MBREAK	0x04000000	/* send an M_BREAK downstream */
#define	STC_INSOFTINT	0x08000000	/* processing this line in softint */
#define	STC_LP		0x10000000	/* this is the printer port */
#define	STC_ACKWAIT	0x40000000	/* waiting in ACK for error to clear */
#define	STC_DRAIN	0x80000000	/* flushing output in stc_start() */

/*
 * flags in stc_line_t.flags field are in stcio.h
 */

/*
 * Per board (controller) structure
 * don't try to kmem_zalloc() the line[] array struct on each open;
 * it will get very messy
 */
struct stc_unit_t {
	kmutex_t		*stc_mutex;	/* protects cd-180 registers */
	ddi_iblock_cookie_t	stc_blk_cookie;	/* cd180 interrupt cookie */
	ddi_idevice_cookie_t	stc_dev_cookie;

	kmutex_t		*ppc_mutex;	/* protects ppc registers */
	ddi_iblock_cookie_t	ppc_blk_cookie;	/* ppc interrupt cookie */
	ddi_idevice_cookie_t	ppc_dev_cookie;

	ddi_iblock_cookie_t	soft_blk_cookie; /* soft interrupt cookie */
	ddi_idevice_cookie_t	soft_dev_cookie;
	ddi_softintr_t		softint_id;

	unsigned		flags;		/* various board-level flags */
	struct stc_line_t	line[STC_LINES]; /* one per (serial) line */
	struct stc_line_t	ppc_line;	/* only one PPC per board */
	struct stc_line_t	control_line;	/* stc control device */
	caddr_t			prom;		/* PROM, base board addr */
	struct stcregs_t	*regs;		/* CD-180 registers */
	struct stciack_t	*iack;		/* CD-180 PILRx in *IACK */
	struct ppcregs_t	*ppc;		/* PPC registers */
	unsigned short		*stc_baud;	/* pointer to baud rate tbl */
	dev_info_t		*dip;		/* Device dev_info_t */
};

/*
 * flags in stc_unit_t.flags field
 */
#define	STC_SOFTINTROK	0x00000001	/* added to interrupt chain */
#define	STC_CD180INTROK	0x00000002	/* cd-180 added to interrupt chain */
#define	STC_PPCINTROK	0x00000004	/* ppc added to interrupt chain */
#define	STC_REGOK	0x00000008	/* good register mappings */
#define	STC_ATTACHOK	0x00000010	/* made it through stc_attach() OK */

/*
 * the state struct for transparent ioctl()s
 */
struct stc_state_t {
	int	state;
	caddr_t	addr;
};

/*
 * state for transparent ioctl()'s used in stc_state_t
 */
#define	STC_COPYIN	1
#define	STC_COPYOUT	2

/*
 * ioctl debugging stuff
 */
#ifdef	DEBUG_STC

struct ioc_txt_t {
	char	*name;
	int	ioc_cmd;
};

struct ioc_txt_t ioc_txt[] = {
	{ "TCSBRK",		TCSBRK },
	{ "TCSETSW",		TCSETSW },
	{ "TCSETSF",		TCSETSF },
	{ "TCSETAW",		TCSETAW },
	{ "TCSETAF",		TCSETAF },
	{ "TIOCSBRK",		TIOCSBRK },
	{ "TIOCCBRK",		TIOCCBRK },
	{ "TCGETA",		TCGETA },
	{ "TCSETA",		TCSETA },
	{ "TCSETAW",		TCSETAW },
	{ "TCSETAF",		TCSETAF },
	{ "TCXONC",		TCXONC },
	{ "TCFLSH",		TCFLSH },
	{ "TIOCKBON",		TIOCKBON },
	{ "TIOCKBOF",		TIOCKBOF },
	{ "TIOCMGET",		TIOCMGET },
	{ "TIOCMBIC",		TIOCMBIC },
	{ "TIOCMBIS",		TIOCMBIS },
	{ "TIOCMSET",		TIOCMSET },
	{ "KBENABLED",		KBENABLED },
	{ "TCDSET",		TCDSET },
	{ "RTS_TOG",		RTS_TOG },
	{ "TIOCGWINSZ",		TIOCGWINSZ },
	{ "TIOCSWINSZ",		TIOCSWINSZ },
	{ "TIOCGSOFTCAR",	TIOCGSOFTCAR },
	{ "TIOCSSOFTCAR",	TIOCSSOFTCAR },
	{ "TCGETS",		TCGETS },
	{ "TCSETS",		TCSETS },
	{ "TCSANOW",		TCSANOW },
	{ "TCSADRAIN",		TCSADRAIN },
	{ "TCSAFLUSH",		TCSAFLUSH },
	{ "STGET",		STGET },
	{ "STSET",		STSET },
	{ "STTHROW",		STTHROW },
	{ "STWLINE",		STWLINE },
	{ "STTSV",		STTSV },
	{ "TCGETX",		TCGETX },
	{ "TCSETX",		TCSETX },
	{ "TCSETXW",		TCSETXW },
	{ "TCSETXF",		TCSETXF },
	{ (char *)NULL,		0 },
};
#endif	/* DEBUG_STC */

#ifdef	__cplusplus
}
#endif

#endif	/* !_SYS_STCVAR_H */
