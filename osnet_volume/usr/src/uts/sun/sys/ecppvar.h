/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ECPPVAR_H
#define	_SYS_ECPPVAR_H

#pragma ident	"@(#)ecppvar.h	2.21	99/10/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct ecppunit {
	int instance;
	dev_info_t *dip;	/* device information */
	struct config_reg *c_reg; /* configuration register */
	struct info_reg *i_reg; /* info registers */
	struct fifo_reg *f_reg; /* fifo register */
	struct cheerio_dma_reg *dmac; /* ebus dmac registers */
	ddi_acc_handle_t c_handle;
	ddi_acc_handle_t i_handle;
	ddi_acc_handle_t f_handle;
	ddi_acc_handle_t d_handle;
	/* for DMA devices */
	ddi_dma_attr_t attr;		/* DMA attribute characterics */
	ddi_dma_handle_t dma_handle;	/* active DMA mapping for this unit */
	ddi_dma_cookie_t dma_cookie;
	uint_t dma_cookie_count;
	/* For PIO transfers */
	caddr_t next_byte;
	caddr_t last_byte;
	/* For Streams */
	boolean_t oflag;		/* ecpp is already open */
	boolean_t e_busy;		/* ecpp busy flag */
	queue_t		*readq;		/* pointer to readq */
	queue_t		*writeq;	/* pointer to writeq */
	mblk_t	*msg;			/* current message block */
	timeout_id_t timeout_id;	/* io transfers timer */
	timeout_id_t fifo_timer_id;	/* drain SuperIO FIFO */
	uchar_t about_to_untimeout;	/* timeout lock */
	/*  CPR support */
	boolean_t suspended;		/* TRUE if driver suspended */
	/* 1284 */
	int    current_mode;		/* 1284 mode */
	uchar_t current_phase;		/* 1284 ECP phase */
	uchar_t backchannel;		/* backchannel mode supported */
	uchar_t error_status;		/* port error status */
	struct ecpp_transfer_parms xfer_parms;
	struct ecpp_regs regs;		/* control/status registers */
	uint8_t saved_dsr;		/* store the dsr returned from TESTIO */
	boolean_t timeout_error;	/* store the timeout for GETERR */
	uchar_t port;			/* xfer type: dma/pio/tfifo */
	ddi_iblock_cookie_t ecpp_trap_cookie; /* interrupt cookie */
	kmutex_t umutex;	/* lock for this structure */
	kcondvar_t pport_cv;		/* cv for changing port type */
	kcondvar_t transfer_cv;		/* cv for close/flush	*/
	uchar_t terminate;		/* flag to indicate closing */
	uchar_t need_idle_state;	/* return to idle state requested */
	uchar_t no_more_fifo_timers;	/* continue waiting for FIFO drain */
	uint32_t ecpp_drain_counter;	/* allows fifo to drain */
	uchar_t wsrv_timer;		/* timer status */
	timeout_id_t wsrv_timer_id;
	uchar_t about_to_untimeout_wsrvt; /* indicates cancelled timeout */
	uchar_t dma_cancelled;		/* flushed while dma'ing */
	caddr_t ioblock;		/* pio/dma transfer block */
	uchar_t io_mode;		/* transfer mode: pio/dma */
	uchar_t init_seq;		/* centronics init seq */
	uint32_t wsrv_retry;		/* delay (ms) before next wsrv */
	uint32_t wait_for_busy;		/* wait for BUSY to deassert */
	uint32_t data_setup_time;	/* pio centronics handshake */
	uint32_t strobe_pulse_width;	/* pio centronics handshake */
	uchar_t fast_centronics;	/* DMA/PIO centronics */
	uchar_t fast_compat;		/* DMA/PIO 1284 compatible mode */
};

/* current_phase values */

#define	ECPP_PHASE_INIT		0x00	/* initialization */
#define	ECPP_PHASE_NEGO		0x01	/* negotiation */
#define	ECPP_PHASE_TERM		0x02	/* termination */
#define	ECPP_PHASE_PO		0x03	/* power-on */

#define	ECPP_PHASE_C_FWD_DMA	0x10	/* cntrx/compat fwd dma xfer */
#define	ECPP_PHASE_C_FWD_PIO	0x11	/* cntrx/compat fwd PIO xfer */
#define	ECPP_PHASE_C_IDLE	0x12	/* cntrx/compat idle */

#define	ECPP_PHASE_NIBT_REVDATA	0x20	/* nibble/byte reverse data */
#define	ECPP_PHASE_NIBT_AVAIL	0x21	/* nibble/byte reverse data available */
#define	ECPP_PHASE_NIBT_NAVAIL	0x22	/* nibble/byte reverse data not avail */
#define	ECPP_PHASE_NIBT_REVIDLE	0x22	/* nibble/byte reverse idle */
#define	ECPP_PHASE_NIBT_REVINTR	0x23	/* nibble/byte reverse interrupt */

#define	ECPP_PHASE_ECP_SETUP	0x30	/* ecp setup */
#define	ECPP_PHASE_ECP_FWD_XFER	0x31	/* ecp forward transfer */
#define	ECPP_PHASE_ECP_FWD_IDLE	0x32	/* ecp forward idle */
#define	ECPP_PHASE_ECP_FWD_REV	0x33	/* ecp forward to reverse */
#define	ECPP_PHASE_ECP_REV_XFER	0x34	/* ecp reverse transfer */
#define	ECPP_PHASE_ECP_REV_IDLE	0x35	/* ecp reverse idle */
#define	ECPP_PHASE_ECP_REV_FWD	0x36	/* ecp reverse to forward */

#define	FAILURE_PHASE		0x80
#define	UNDEFINED_PHASE		0x81

/* ecpp return values */
#define	SUCCESS		1
#define	FAILURE		2

/* ecpp states */
#define	ECPP_IDLE	1 /* No ongoing transfers */
#define	ECPP_BUSY	2 /* Ongoing transfers on the cable */
#define	ECPP_DATA	3 /* Not used */
#define	ECPP_ERR	4 /* Bad status in Centronics mode */
#define	ECPP_FLUSH	5 /* Currently flushing the q */

#define	TRUE		1
#define	FALSE		0

/* message type */
#define	ECPP_BACKCHANNEL	0x45

/* port error_status values */
#define	ECPP_NO_1284_ERR	0x0
#define	ECPP_1284_ERR		0x1

/* transfer modes */
#define	ECPP_DMA		0x1
#define	ECPP_PIO		0x2

/* tuneable timing defaults */
#define	CENTRONICS_RETRY	750	/* 750 milliseconds */
#define	WAIT_FOR_BUSY		1000	/* 1000 microseconds */
#define	SUSPEND_TOUT		10	/* # seconds before suspend fails */

/* Centronics hanshaking defaults */
#define	DATA_SETUP_TIME		2	/* 2 uSec Data Setup Time (2x min) */
#define	STROBE_PULSE_WIDTH	2	/* 2 uSec Strobe Pulse (2x min) */



/* Macros for superio programming */
#define	PP_PUTB(x, y, z)  	ddi_put8(x, y, z)
#define	PP_GETB(x, y)		ddi_get8(x, y)

/* Macros for DMAC programing */
#define	OR_SET_BYTE_R(handle, addr, val) \
{		\
	uint8_t tmpval;					\
	tmpval = ddi_get8(handle, (uint8_t *)addr);	\
	tmpval |= val;					\
	ddi_put8(handle, (uint8_t *)addr, tmpval);	\
}

#define	OR_SET_LONG_R(handle, addr, val) \
{		\
	uint32_t tmpval;				\
	tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	tmpval |= val;					\
	ddi_put32(handle, (uint32_t *)addr, tmpval);	\
}

#define	AND_SET_BYTE_R(handle, addr, val) \
{		\
	uint8_t tmpval;					\
	tmpval = ddi_get8(handle, (uint8_t *)addr);	\
	tmpval &= val; 					\
	ddi_put8(handle, (uint8_t *)addr, tmpval);	\
}

#define	AND_SET_LONG_R(handle, addr, val) \
{		\
	uint32_t tmpval;				\
	tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	tmpval &= val; 					\
	ddi_put32(handle, (uint32_t *)addr, tmpval);	\
}

#define	NOR_SET_LONG_R(handle, addr, val, mask) \
{		\
	uint32_t tmpval;				\
	tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	tmpval &= ~(mask);				\
	tmpval |= val;					\
	ddi_put32(handle, (uint32_t *)addr, tmpval);	\
}


/* Debugging */
#if defined(POSTRACE)

#ifndef NPOSTRACE
#define	NPOSTRACE 1024
#endif

struct postrace {
	int count;
	int function;		/* address of function */
	int trace_action;	/* descriptive 4 characters */
	int object;		/* object operated on */
};

/*
 * For debugging, allocate space for the trace buffer
 */

extern struct postrace postrace_buffer[];
extern struct postrace *postrace_ptr;
extern int postrace_count;

#define	PTRACEINIT() {				\
	if (postrace_ptr == NULL)		\
		postrace_ptr = postrace_buffer; \
	}

#define	LOCK_TRACE()	(uint_t)ddi_enter_critical()
#define	UNLOCK_TRACE(x)	ddi_exit_critical((uint_t)x)

#define	PTRACE(func, act, obj) {		\
	int __s = LOCK_TRACE();			\
	int *_p = &postrace_ptr->count;	\
	*_p++ = ++postrace_count;		\
	*_p++ = (int)(func);			\
	*_p++ = (int)(act);			\
	*_p++ = (int)(obj);			\
	if ((struct postrace *)(void *)_p >= &postrace_buffer[NPOSTRACE])\
		postrace_ptr = postrace_buffer; \
	else					\
		postrace_ptr = (struct postrace *)(void *)_p; \
	UNLOCK_TRACE(__s);			\
	}

#else	/* !POSTRACE */

/* If no tracing, define no-ops */
#define	PTRACEINIT()
#define	PTRACE(a, b, c)

#endif	/* !POSTRACE */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ECPPVAR_H */
