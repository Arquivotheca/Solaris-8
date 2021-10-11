/*
 * Copyright (c) 1992-1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_XBOXIMPL_H
#define	_SYS_XBOXIMPL_H

#pragma ident	"@(#)xboximpl.h	1.7	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * useful debug stuff during bringup
 */
#ifdef DEBUG
#define	XBOX_DEBUG
#endif

#ifdef XBOX_DEBUG
#define	XPRINTF		if (xdebug) xprintf
#define	DEBUGGING	(xdebug > 1)
#define	XPRINTF_DEBUG	if (xdebug > 1) xprintf
#else
#define	DEBUGGING	(0)
#define	XPRINTF_DEBUG	if (0) xprintf
#define	XPRINTF		if (0) xprintf
#endif /* XBOX_DEBUG */

#define	XBOX_TIMEOUT	10000
#define	XBOX_DO_ERRLOG_ENABLE	1
#define	NO_PKT		0x7fff0000

/*
 * driver status, available to user via ioctl
 */
struct xac_soft_status {
	uchar_t		xac_write0_key;
	uchar_t		xac_uadm;
	uchar_t		xac_intr_number; /* index into intr properties list */
	uchar_t		xac_action_on_error;
	ulong_t		xac_driver_state;
};

#define	xac_state   xac_soft_status.xac_driver_state
#define	xac_inumber xac_soft_status.xac_intr_number


/*
 * action on error values
 */
#define	ACTION_PANIC	0
#define	ACTION_CONT	1

/*
 * state bits
 */
#define	XAC_STATE_OPEN		0x0001
#define	XAC_STATE_ALIVE		0x0010
#define	XAC_STATE_TRANSPARENT	0x0040

#ifdef _KERNEL
/*
 * xac_info:	this structure provides all xadapter info:
 *		the errstat pointer points to the next available packet
 *		the xac pointer points to hardware register space
 */

struct xbox_state {
	/*
	 * info we make available to application programs
	 */
	struct xac_soft_status	   xac_soft_status;

	/*
	 * The error packet which the XBox will DMA at us
	 */
	struct xc_errs		  *xac_epkt;
	struct xc_errs		  *xbc_epkt;

	uint_t			   xac_epkt_dma_addr;
	uint_t			   xbc_epkt_dma_addr;

	/*
	 * safe place for storing the error packet
	 */
	struct xc_errs		  xac_saved_epkt;
	struct xc_errs		  xbc_saved_epkt;

	/*
	 * Parent node (XAdapter Controller)
	 * [these registers invisible in transparent mode]
	 */
	struct xc		   xac_xc;

	/*
	 * The 'write0' key pre-shifted 24 bits.
	 */
	uint_t			   xac_write0_key24;

	/*
	 * keep dev info and instance around
	 */
	dev_info_t		  *xac_dev_info;
	int			   xac_instance;


	/*
	 * save the ctl0 register values
	 */
	uint_t			   xac_ctl0;
	uint_t			   xbc_ctl0;

	/*
	 * This mutex needs to be held when changing critical
	 * XBox state e.g. when performing the 'flush-by-self-test'
	 * operation.
	 * The cv is used for signalling a waiting thread in the event
	 * of an error
	 */
	kmutex_t		   xac_sync_mutex;
	kcondvar_t		   xac_cv;

	/*
	 * The XBox is capable of generating any SBus interrupt level
	 * - inumber is equivalent to the SBus level - 1, the iblkc
	 * is used for mutex initialization and for removing the interrupt
	 */
	ddi_iblock_cookie_t	   xac_iblkc;

	/*
	 * Used to synchronize the XBox error packets into memory.
	 */
	ddi_dma_handle_t	   xac_dhandle;

};

#define	XAC_MUTEX	&xac->xac_sync_mutex
#define	XAC_CV		&xac->xac_cv

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XBOXIMPL_H */
