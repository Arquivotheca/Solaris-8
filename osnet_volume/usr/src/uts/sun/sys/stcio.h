/*
 * Copyright (c) 1990-1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STCIO_H
#define	_SYS_STCIO_H

#pragma ident	"@(#)stcio.h	1.24	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for users of stc devices.
 */

/*
 * Minor device number encoding:
 *
 *	o p u u | u l l l
 *
 *	o - set if this device is an outgoing serial line
 *	p - set if this is a parallel port device
 *	u - device unit number
 *	l - device line number
 *	    if this is the parallel port line, 'p' should be 1 and
 *		'lll' should be all 0's
 *	    if this is the control line, both 'p' and 'lll' should be set
 *		to all 1's
 */

/*
 *
 *	STC_SREGS - sets on-board register values
 *	STC_GREGS - get register values
 *	STC_SPPC - set parallel port parameters (until close())
 *	STC_GPPC - get parallel port parameters (valid until changed or close())
 *
 * #define	STC_DCONTROL	_IOWR('s', 254, struct stc_defaults_t)
 * #define	STC_SDEFAULTS	_IOW('s', 251, struct stc_defaults_t)
 * #define	STC_GDEFAULTS	_IOR('s', 250, struct stc_defaults_t)
 * #define	STC_SPPC	_IOW('s', 252, struct ppc_params_t)
 * #define	STC_GPPC	_IOR('s', 253, struct ppc_params_t)
 * #define	STC_GSTATS	_IOWR('s', 260, struct stc_stats_t)
 */
#define	sIOC		('s'<<8)
#define	STC_SREGS	(sIOC|251)
#define	STC_GREGS	(sIOC|252)
#define	STC_SPPC	(sIOC|253)
#define	STC_GPPC	(sIOC|254)
#define	STC_GSTATS	(sIOC|255)

/*
 * we define this here so that it can be exported to users using the
 * serial ioctl()'s to manipulate line default parameters; if you
 * change this, the driver must be recompiled
 */
#define	STC_SILOSIZE	1024	/* size of (soft) rx silo in line struct */

/*
 * macro that returns 1 if parameter is out of range, used for range checking
 * on valued parameters passed to driver via stc_defaults_t and ppc_params_t
 */
#define	FUNKY(v, min, max)	((v < min) || (v > max))

/*
 * fields of stc_default_t structure
 *
 * flags for the serial lines (line->flags)
 */
#define	DTR_ASSERT	0x0001		/* assert DTR on open */
#define	SOFT_CARR	0x0002		/* ignore CD input on open */
#define	STC_DTRCLOSE	0x0004		/* use zs style DTR close() if clear */
#define	STC_CFLOWFLUSH	0x0008		/* flush in close() for flow control */
#define	STC_CFLOWMSG	0x0010		/* close() message for flow control */
#define	STC_INSTANTFLOW	0x0020		/* when flow control off, enable xmtr */
#define	STC_DTRFORCE	0x0040		/* force DTR always on */

/*
 * parameters for the serial lines. min and max values are listed.
 * if you put a strange value in, the ioctl will return an EINVAL and
 * no defaults will be changed
 */
#define	MIN_DRAIN_SIZE	4		/* min STREAMS size in drainsilo() */
#define	MAX_DRAIN_SIZE	1024		/* max STREAMS size in drainsilo() */
#define	MIN_HIWATER	2		/* min hiwater mark for Rx silo */
#define	MAX_HIWATER	(STC_SILOSIZE-2) /* max hiwater mark for Rx silo */
#define	MIN_LOWWATER	2		/* min lowwater mark for Rx silo */
#define	MAX_LOWWATER	((stc_defaults->stc_hiwater)-2) /* max lowwater mark */
#define	MIN_RTPR	1		/* min Rx timeout regtister value */
#define	MAX_RTPR	255		/* max Rx timeout regtister value */
#define	MIN_RX_FIFO	1		/* min value for Rx FIFO threshold */
#define	MAX_RX_FIFO	8		/* max value for Rx FIFO threshold */

/*
 * flags for the parallel line (line->flags)
 * (these also are returned in the "state" field of the ppc_param struct when
 * you do a STC_GPPC ioctl(); setting them on a STC_SPPC ioctl() has no effect)
 *
 * if PP_MSG is clear, you'll get a console message when a particular error is
 * first detected, and another console message when that error has cleared
 * itself.  if PP_MSG * is set, you'll get a console error message approx
 * every 60 seconds for a particular * error condition until that error
 * condition clears itself
 *
 * if PP_SIGNAL is set, you'll get a PP_SIGTYPE signal whenever we detect a
 * printer error that you haven't masked off
 *
 * you get 2 versions of the interface status - the "soft" version, i.e. what
 * the driver thinks the current status is qualified with the signals that
 * you've masked off, and the "real" version which is read from the pins
 * at the time the STC_GPPC ioctl() is done (no masking on the "real"
 * version).  the latter appears in the "state" field shifted left
 * by PP_SHIFT
 */
#define	PP_SHIFT	8		/* "real" interface status left shift */
#define	PP_PAPER_OUT	0x00001		/* honour/return PAPER OUT */
#define	PP_ERROR	0x00002		/* honour/return ERROR */
#define	PP_BUSY		0x00004		/* honour/return BUSY */
#define	PP_SELECT	0x00008		/* honour/return SELECT */
#define	PP_MSG		0x20000		/* print message on error scan */
#define	PP_SIGNAL	0x10000		/* send signal if printer error */
#define	PP_SIGTYPE	SIGURG		/* signal to send on printer error */

/*
 * some parameter ranges for the ppc defaults.  min and max values are listed.
 * if you put a strange value in, the ioctl will return an EINVAL and
 * no defaults will be changed
 */
#define	MIN_STBWIDTH	1		/* min ppc STROBE width, in uS */
#define	MAX_STBWIDTH	30		/* max ppc STROBE width, in uS */
#define	MIN_DATASETUP	0		/* min data setup time, in uS */
#define	MAX_DATASETUP	30		/* max data setup time, in uS */
#define	MIN_ACKTIMEOUT	5		/* min ACK timeout, in seconds */
#define	MAX_ACKTIMEOUT	7200		/* max ACK timeout, in seconds */
#define	MIN_ERRTIMEOUT	1		/* min ERROR timeout, in seconds */
#define	MAX_ERRTIMEOUT	480		/* max ERROR timeout, in seconds */
#define	NO_BUSYTIMEOUT	0		/* set this for infinite BUSY timeout */
#define	MIN_BUSYTIMEOUT	0		/* min BUSY timeout, in seconds */
#define	MAX_BUSYTIMEOUT	7200		/* max BUSY timeout, in seconds */

/*
 * the structure that gets passed back and forth to deal with the defaults
 */
struct stc_defaults_t {
	int		flags;		/* things like soft carrier, etc... */

	/* serial port Rx handler parameters */
	int		drain_size;	/* size of buffer in stc_drainsilo() */
	int		stc_hiwater;	/* high water in CHECK_RTS() macro */
	int		stc_lowwater;	/* low water in CHECK_RTS() macro */
	int		rtpr;		/* inter-character receive timer */
	int		rx_fifo_thld;	/* cd-180 RxFIFO threshold */
	struct termios	termios;	/* baud rates, parity, etc... */

	/* parallel line things */
	int		strobe_w;	/* strobe width, in uS */
	int		data_setup;	/* data setup time, in uS */
	int		ack_timeout;	/* ACK timeout, in secs */
	int		error_timeout;	/* PAPER OUT, etc, timeout in seconds */
	int		busy_timeout;	/* BUSY timeout, in seconds */

	/* for the control device */
	int		line_no;	/* line number to operate on */
	int		op;		/* operation */

	/* for diagnostic access to registers */
	uchar_t		reg_offset;	/* register offset to read/write */
	uchar_t		reg_data;	/* register data to read/write */
};

/*
 * op field return values for STC_GDEFAULTS, STC_DCONTROL(STC_CDEFGET)
 * and STC_DCONTROL(STC_SPARAM_GET)
 */
#define	STC_SERIAL	0x01		/* this is a serial line */
#define	STC_PARALLEL	0x02		/* this is a parallel line */
#define	STC_CNTRL	0x04		/* this is the control line */

/*
 * the structure that gets passed to the GREGS and SREGS ioctls
*/
struct stc_diagregs_t {
	int	reg_flag;	/* which register set */
	uchar_t	reg_offset;	/* register offset to read/write */
	uchar_t	reg_data;	/* register data to read/write */
};

/*
 * the reg flag for tha above structure
 */
#define	STC_IOREG	0x1
#define	STC_PPCREG	0x0

/*
 * the op parameters, only written for the control device per board
 * used only with the STC_DCONTROL ioctl() and then only if the
 * device is the board control device (read the man page)
 *
 * STC_GDEFAULTS will return the type of line it's connected to
 * in the op field
 *
 * STC_DCONTROL(STC_CDEFGET) and STC_DCONTROL(STC_SPARAM_GET) will
 * return the type of line specified by the line_no field in
 * the op field
 *
 * STC_DCONTROL(STC_SPARAM_SET) and STC_DCONTROL(STC_SPARAM_GET) will
 * return an STC_NOTOPEN_ERR error if the referenced line is not open
 *
 * Note on STC_CFLUSH: set the line # that you want to flush in
 * the "line_no" field of the "stc_defaults_t" struct that you
 * pass to STC_DCONTROL; if you want to flush the printer, set
 * the line number to 64 or use the STC_LP_SETLINE() macro
 *
 * STC_REGIOR, STC_REGIOW, STC_PPCREGR and STC_PPCREGW are designed
 * mostly for diagnostic use - don't try them unless you know what
 * you're doing; you can cause all sorts of problems like enabling
 * interrupts when they shouldn't be, resetting the cd180 and/or the
 * PPC and generally wreaking havoc with the whole system
 * to get register offsets for these, include <sbusdev/stcreg.h>
 * for the diagnostic ops, specify the line number to operate on
 * as 0 (unless you want the passed line number to be loaded into
 * the cd180's CAR (channel address register) before each cd180
 * register access; if so, OR in STC_SETCAR to the op field)
 */
#define	STC_CDEFSET	0x00000001	/* set another line's defaults */
#define	STC_CDEFGET	0x00000002	/* get another line's defaults */
#define	STC_SPARAM_SET	0x00000004	/* set serial port parameters now */
#define	STC_SPARAM_GET	0x00000008	/* get current serial port parameters */
#define	STC_REGIOR	0x00000100	/* read cd180 register */
#define	STC_REGIOW	0x00000200	/* write cd180 register */
#define	STC_PPCREGR	0x00000400	/* read PPC register */
#define	STC_PPCREGW	0x00000800	/* write PPC register */
#define	STC_CFLUSH	0x00008000	/* flush a line's write queue */
#define	STC_SETCAR	0x10000000	/* set cd180 car (diag use) */

#define	STC_LP_SETLINE(line)	(((line) & 0x3f) | 64)
#define	STC_NOTOPEN_ERR	ESRCH		/* return if line is not open for use */

/*
 * the parameter structure for the parallel port
 */
struct ppc_params_t {
	int		flags;		/* same as above */
	int		state;		/* status of the printer interface */
	int		strobe_w;	/* strobe width, in uS */
	int		data_setup;	/* data setup time, in uS */
	int		ack_timeout;	/* ACK timeout, in secs */
	int		error_timeout;	/* PAPER OUT, etc, timeout in seconds */
	int		busy_timeout;	/* BUSY timeout, in seconds */
};

/*
 * the stc_stats_t struct is used for statistics gathering and monitoring
 *	driver performance of the serial lines (statistics gathering is
 *	not supported on the parallel line)
 */
struct stc_stats_t {
	int			line_no;	/* line number to operate on */
	int			cmd;		/* command (see flags below) */
	int			qpunt;		/* punting in stc_drainsilo() */
	int			drain_timer;	/* posted a drainsilo timer */
	int			no_canput;	/* drainsilo canput() failed */
	int			no_rcv_drain;	/* call to drainsilo() failed */
	int			stc_drain;	/* STC_DRAIN set on this line */
	int			stc_break;	/* BREAK requested */
	int			stc_sbreak;	/* start BREAK requested */
	int			stc_ebreak;	/* end BREAK requested */
	int			set_modem;	/* set modem control lines */
	int			get_modem;	/* get modem control lines */
	int			ioc_error;	/* bad ioctl() */
	int			set_params;	/* call to stc_param() */
	int			no_start;	/* already in stc_start() */
	int			xmit_int;	/* transmit interrupts */
	int			rcv_int;	/* receive interrupts */
	int			rcvex_int;	/* rcv exception interrupts */
	int			modem_int;	/* modem change interrupts */
	int			xmit_cc;	/* characters transmitted */
	int			rcv_cc;		/* characters received */
	int			break_cnt;	/* BREAKs received */
	int			bufcall;	/* times no STREAMS buffer */
	int			canwait;	/* pending timer in drainsilo */
	int			nqfretry;	/* number of queue retries */
};

/*
 * flags in stc_stats_t.cmd field
 */
#define	STAT_SET		0x0002		/* set line parameters */
#define	STAT_CLEAR		0x0001		/* clear line statistics */
#define	STAT_GET		0x0000		/* get line statistics */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_STCIO_H */
