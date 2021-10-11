/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SCSI_TARGETS_PLN_CTLR_H
#define	_SYS_SCSI_TARGETS_PLN_CTLR_H

#pragma ident	"@(#)pln_ctlr.h	1.9	98/10/11 SMI"

/*
 * Pluto (Sparc Storage Array) controller target driver
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Compile options to turn on debugging code
 */
#ifdef	DEBUG
#define	PLN_CTLR_DEBUG
#endif	/* DEBUG */

#if	defined(_KERNEL) || defined(_KMEMUSER)

/*
 * Local definitions, for clarity of code
 */
#define	NEW_STATE(state)					\
		pctlr->pln_last_state = pctlr->pln_state;	\
		pctlr->pln_state = (state)


#define	PLN_CTLR_SCSI_DEVP	(pctlr->pln_scsi_devicep)
#define	PLN_CTLR_MUTEX		(&(PLN_CTLR_SCSI_DEVP)->sd_mutex)
#define	ROUTE			(&(PLN_CTLR_SCSI_DEVP)->sd_address)

#define	SCBP(pkt)		((struct scsi_status *)(pkt)->pkt_scbp)
#define	SCBP_C(pkt)		((*(pkt)->pkt_scbp) & STATUS_MASK)
#define	CDBP(pkt)		((union scsi_cdb *)(pkt)->pkt_cdbp)
#define	BP_PKT(bp)		((struct scsi_pkt *)bp->av_back)

#define	PLN_CTLR_MINOR(i, p)	(((i) << 2) | (p))
#define	PLN_CTLR_INST(d)	(getminor((d)) >> 2)
#define	PLN_CTLR_NODE(d)	(getminor((d)) & 3)
#define	PLN_CTLR_NODE_CTLR	0
#define	PLN_CTLR_NODE_DEVCTL	1
#define	PLN_CTLR_NODE_SCSI	2

#ifdef PLN_CTLR_DEBUG
#define	PLN_CTLR_CE_DEBUG	((1 << 8) | CE_CONT)
#define	PLN_CTLR_CE_DEBUG1	((2 << 8) | CE_CONT)
#define	PLN_CTLR_CE_DEBUG2	((3 << 8) | CE_CONT)
#define	PLN_CTLR_CE_DEBUG3	((4 << 8) | CE_CONT)
#define	PLN_CTLR_LOG		if (pln_ctlr_debug) pln_ctlr_log
#define	PLN_CTLR_DEBUG_ENTER	if (pln_ctlr_debug) debug_enter
#else
#define	PLN_CTLR_CE_DEBUG	0
#define	PLN_CTLR_CE_DEBUG1	0
#define	PLN_CTLR_CE_DEBUG2	0
#define	PLN_CTLR_CE_DEBUG3	0
#define	PLN_CTLR_LOG		if (0) pln_ctlr_log
#define	PLN_CTLR_DEBUG_ENTER	if (0) (void) debug_enter
#endif	/* PLN_CTLR_DEBUG */

/*
 * Private info for scsi targets.
 *
 * Pointed to by the un_private pointer
 * of one of the SCSI_DEVICE structures.
 */
typedef struct pln_controller {
	struct	buf	*pln_sbufp;		/* for use in special io */
	struct scsi_device *pln_scsi_devicep;	/* back pointer to */
						/* SCSI_DEVICE */
	kcondvar_t	pln_sbuf_cv;		/* conditional variable */
						/* for sbufp */
	int		pln_sbuf_busy;		/* Wait Variable */
	struct diskhd	pln_utab;		/* for queuing */
	int		pln_retry_count;	/* retry count */
	int		pln_state;		/* current state */
	int		pln_last_state;		/* last state */
	timeout_id_t	pln_ctlr_restart_id;	/* timeout id for restarts */
} pln_controller_t;

_NOTE(MUTEX_PROTECTS_DATA(scsi_device::sd_mutex, pln_controller::pln_state))
_NOTE(MUTEX_PROTECTS_DATA(scsi_device::sd_mutex,
		pln_controller::pln_last_state))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_controller::pln_sbufp))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_controller::pln_scsi_devicep))
#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

/*
 * Driver states
 */

#define	PLN_CTLR_STATE_ATTACHED		0
#define	PLN_CTLR_STATE_CLOSED		1
#define	PLN_CTLR_STATE_OPENING		2
#define	PLN_CTLR_STATE_OPEN		3
#define	PLN_CTLR_STATE_SENSING		4
#define	PLN_CTLR_STATE_RWAIT		5
#define	PLN_CTLR_STATE_DETACHING	6

/*
 * Parameters
 */

/*
 * Timeout value for internally-generated operations
 */

#define	PLN_CTLR_IO_TIME	35

/*
 * Wait time before retries for commands returning Busy Status
 */

#define	PLN_CTLR_BSY_TIMEOUT		(drv_usectohz(5 * 1000000))

/*
 * Number of times we'll retry a normal operation.
 *
 * This includes retries due to transport failure
 * (need to distinguish between Target and Transport failure)
 */

#define	PLN_CTLR_RETRY_COUNT		4

/*
 * pln_ctlr_intr action codes
 */

#define	COMMAND_DONE		0
#define	COMMAND_DONE_ERROR	1
#define	QUE_COMMAND		2
#define	QUE_SENSE		3
#define	JUST_RETURN		4

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_TARGETS_PLN_CTLR_H */
