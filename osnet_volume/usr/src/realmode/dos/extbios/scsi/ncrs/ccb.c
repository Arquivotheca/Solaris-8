/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: NCR 710/810 EISA SCSI HBA       (ccb.c)
 *
#pragma ident	"@(#)ccb.c	1.1	97/07/21 SMI"
 *
 */

/*
 * Routines in this file are based on equivalent routines in the Solaris
 * NCR driver.  Some have been simplified for use in a single-threaded
 * environment.
 */
/* #define DEBUG */

#ifdef DEBUG
    #pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
#endif


#include <types.h>
#include "ncr.h"


void
ncr_queue_ccb(	ncr_t	*ncrp,
		npt_t	*nptp,
		unchar	rqst_type)
{
	NDBG1(("ncr_queue_ccb: okay\n"));

#ifdef SOLARIS
        /* add this ccb to the target's work queue */
        nccbp->nc_type = rqst_type;
        ncr_waitq_add(nptp, nccbp);
#endif

	nptp->nt_type = rqst_type;
	
	/* if this target isn't active stick it on the hba's work queue */
	if (nptp->nt_state == NPT_STATE_DONE) {
		NDBG1(("ncr_queue_ccb: done\n"));
		ncr_queue_target(ncrp, nptp);
	}

	return;
}

#ifdef SOLARIS
bool_t
ncr_send_dev_reset(	struct scsi_address	*ap,
			ncr_t			*ncrp,
			npt_t			*nptp )
{
	struct scsi_pkt	*pktp;
	nccb_t		*nccbp;
	bool_t	 	 rc;

	if ((nptp = ADDR2NCRUNITP(ap)) == NULL) {
		NDBG1(("ncr_send_dev_reset: nptp null\n"));
		/* shouldn't happen */
		return (FALSE);
	}

	/* allocate a ccb */
	if ((pktp = ncr_pktalloc(ap, 0, 0, 0, NULL, NULL)) == NULL) {
		NDBG30(("ncr_send_dev_reset: alloc\n"));
		return (FALSE);
	}
	nccbp = PKTP2NCCBP(pktp);

	/* add it to the target's and hba's work queues */
	/* maybe it should be added to the front of the target's */
	/* queue and the target should be placed first on the */
	/* hba's queue ??? */
	ncr_queue_ccb(ncrp, nptp, nccbp, NRQ_DEV_RESET);

	/* wait for the request to finish */
	ncr_pollret(ncrp, nccbp);
	rc = (pktp->pkt_reason == CMD_CMPLT);
	ncr_pktfree(pktp);
	return (rc);
}

bool_t
ncr_abort_ccb(	struct	scsi_address	*ap,
		ncr_t			*ncrp,
		npt_t			*nptp )
{
	struct scsi_pkt	*pktp;
	nccb_t		*nccbp;
	bool_t	 	 rc;

	NDBG30(("ncr_abort_ccb: state=%d\n", nptp->nt_state));
	/* allocate a ccb */
	if ((pktp = ncr_pktalloc(ap, 0, 0, 0, NULL, NULL)) == NULL) {
		NDBG30(("ncr_send_dev_reset: alloc\n"));
		return (FALSE);
	}
	nccbp = PKTP2NCCBP(pktp);
	ncr_queue_ccb(ncrp, nptp, nccbp, NRQ_ABORT);
	ncr_pollret(ncrp, nccbp);
	rc = (pktp->pkt_reason == CMD_CMPLT);
	ncr_pktfree(pktp);

	NDBG30(("ncr_abort_ccb: done\n"));
	return (rc);
}


#endif

/* 
 * Called from ncr_pollret when an interrupt is pending on the
 * HBA, or from the interrupt service routine ncr_intr.
 * Read status back from your HBA, determining why the interrupt 
 * happened.  If it's because of a completed command, update the
 * command packet that relates (you'll have some HBA-specific
 * information about tying the interrupt to the command, but it
 * will lead you back to the scsi_pkt that caused the command
 * processing and then the interrupt). 
 * If the command has completed normally, 
 *  1. set the SCSI status byte into *pktp->pkt_scbp
 *  2. set pktp->pkt_reason to an appropriate CMD_ value
 *  3. set pktp->pkt_resid to the amount of data not transferred 
 *  4. set pktp->pkt_state's bits appropriately, according to the
 *     information you now have; things like bus arbitration,
 *     selection, command sent, data transfer, status back, ARQ
 *     done
 */
void
ncr_chkstatus(	ncr_t	*ncrp,
		npt_t	*nptp,
#ifdef SOLARIS
		nccb_t	*nccbp )
#endif
		struct scsi_pkt *pktp )
{
#ifdef SOLARIS
	struct scsi_pkt *pktp = NCCBP2PKTP(nccbp);
#endif


	NDBG17(("ncr_chkstatus: ioaddr=%d\n", ncrp->n_ioaddr));

	/* The active logical unit just completed an operation, pass
	 * the status back to the requestor.
	 */
	if (nptp->nt_goterror) {
		/* interpret the error status */
		NDBG17(("ncr_chkstatus: got error\n"));
		NCR_CHECK_ERROR(ncrp, nptp, pktp);

	} else {
		NDBG17(("ncr_chkstatus: okay\n"));
#ifdef SOLARIS
		*pktp->pkt_scbp  = nptp->nt_statbuf[0];
#endif
		pktp->pkt_reason = CMD_CMPLT;

#ifdef SOLARIS
		/* get residual byte count from the S/G DMA list */
		if (nptp->nt_savedp.nd_left != 0)
			pktp->pkt_resid  = ncr_sg_residual(ncrp, nptp);
		else
			pktp->pkt_resid  = 0;
#endif	
		pktp->pkt_resid = 0;
		pktp->pkt_state = (STATE_XFERRED_DATA | STATE_GOT_BUS
				| STATE_GOT_TARGET | STATE_SENT_CMD
				| STATE_GOT_STATUS);
	}

	NDBG26(("ncr_chkstatus: pktp=0x%x resid=%d\n", pktp, pktp->pkt_resid));
	return;
}

		
/* 
 * Utility routine.  Poll for status of a command sent to HBA 
 * without interrupts (a FLAG_NOINTR command).
 */

void
ncr_pollret(	ncr_t	*ncrp,
#ifdef SOLARIS
		nccb_t	*poll_nccbp )
#else
		struct scsi_pkt *poll_pktp )
#endif
{
#ifdef SOLARIS
	nccb_t		 *nccbp;
	nccb_t		 *nccb_headp = NULL;
	nccb_t		**nccb_tailpp = &nccb_headp;
#endif
	bool_t		  got_it = FALSE;

#ifdef SOLARIS
	NDBG17(("ncr_pollret: nccbp=0x%x\n", poll_nccbp));
#else
	NDBG17(("ncr_pollret: pkt=0x%x\n", poll_pktp));
#endif

	/*
 	 * Wait, using drv_usecwait(), long enough for the command to
	 * reasonably return from the target if the target isn't
	 * "dead".  A polled command may well be sent from scsi_poll, and
	 * there are retries built in to scsi_poll if the transport
	 * accepted the packet (TRAN_ACCEPT).  scsi_poll waits 1 second
	 * and retries the transport up to scsi_poll_busycnt times
	 * (currently 60) if
	 * 1. pkt_reason is CMD_INCOMPLETE and pkt_state is 0, or
	 * 2. pkt_reason is CMD_CMPLT and *pkt_scbp has STATUS_BUSY
 	 */
	while (!got_it) {
		if (ncr_wait_intr(ncrp) == FALSE) {
			poll_pktp->pkt_reason = CMD_INCOMPLETE;
			poll_pktp->pkt_state = 0;
			NDBG17(("ncr_pollret: wait_intr=FALSE\n"));
			break;
		}
		else
			got_it = TRUE;

#ifdef SOLARIS
		/* requeue all completed packets on my local list
		 * until my polled packet returns
		 */
		while ((nccbp = ncr_doneq_rm(ncrp)) != NULL) {
			/* if it's my packet, re-queue the rest and return */
			if (poll_nccbp == nccbp) {
				NDBG17(("ncr_pollret: okay\n"));
				got_it = TRUE;
				continue;
			}

			/* requeue the other packets on my local list */
			/* keep list in fifo order */
			*nccb_tailpp = nccbp;
			nccb_tailpp = &nccbp->nc_linkp;
			NDBG17(("ncr_pollret: loop\n"));
		}
#endif
	}

	NDBG17(("ncr_pollret: break\n"));

	if (!got_it) {
		NDBG17(("ncr_pollret: gotit\n"));
		/* this isn't supposed to happen, the hba must be wedged */
		NDBG17(("ncr_pollret: command incomplete\n"));
#ifdef SOLARIS
		if (poll_nccbp->nc_queued == FALSE) {
#endif

			NDBG17(("ncr_pollret: not on waitq\n"));

			/* it must be the active request */
			/* reset the bus and flush all ccb's */
			NCR_BUS_RESET(ncrp);

			/* set all targets on this hba to renegotiate syncio */
#ifdef SOLARIS
			ncr_syncio_reset(ncrp, NULL);
#endif

			/* try brute force to un-wedge the hba */
			ncr_flush_hba(ncrp, TRUE, CMD_RESET, STATE_GOT_BUS
					  , STAT_BUS_RESET);
			ncrp->n_state = NSTATE_IDLE;
#ifdef SOLARIS
			cmn_err(CE_WARN, "ncrs: bus reset: instance=%d\n",
				ddi_get_instance(ncrp->n_dip));
		} else {
			npt_t	*nptp = PKT2NCRUNITP(NCCBP2PKTP(poll_nccbp));

			/* find and remove it from the waitq */
			NDBG17(("ncr_pollret: delete from waitq\n"));
			ncr_waitq_delete(nptp, poll_nccbp);
		}
		NCCBP2PKTP(poll_nccbp)->pkt_reason = CMD_INCOMPLETE;
		NCCBP2PKTP(poll_nccbp)->pkt_state  = 0;
#else
		poll_pktp->pkt_reason = CMD_INCOMPLETE;
		poll_pktp->pkt_state = 0;
#endif
	}

#ifdef SOLARIS
	/* check for other completed packets that have been queued */
	if (nccb_headp) {
		NDBG17(("ncr_pollret: nccb loop\n"));
		mutex_exit(&ncrp->n_mutex);
		while ((nccbp = nccb_headp) != NULL) {
			nccb_headp = nccbp->nc_linkp;
			scsi_run_cbthread(ncrp->n_cbthdl, NCCBP2SCMDP(nccbp));
		}
		mutex_enter(&ncrp->n_mutex);
	}
#endif
	NDBG17(("ncr_pollret: done\n"));
	return;
}


void
ncr_flush_lun(	ncr_t	*ncrp, 
		npt_t	*nptp,
		bool_t	 flush_all,
		u_char	 pkt_reason,
		u_long	 pkt_state,
		u_long	 pkt_statistics )
{
#ifdef SOLARIS
	nccb_t	*nccbp;
#endif

	NDBG17(("ncr_flush_lun: %d\n", flush_all));

	/* post the completion status, and put the ccb on the
	/* doneq to schedule the call of the completion function */
#ifdef SOLARIS
	nccbp = nptp->nt_nccbp;
	nptp->nt_nccbp = NULL;
#endif
	nptp->nt_pktp = NULL;
	ncr_set_done(ncrp, nptp, nptp->nt_pktp, pkt_reason, pkt_state,
			pkt_statistics);

#ifdef SOLARIS
	if (flush_all) {
		/* flush all the queued ccb's and then mark the target idle */
		NDBG17(("ncr_flush_lun: loop "));
		while ((nccbp = ncr_waitq_rm(nptp)) != NULL) {
			NDBG17(("#"));
			ncr_set_done(ncrp, nptp, nccbp, pkt_reason, pkt_state
					 , pkt_statistics);
		}
		nptp->nt_state = NPT_STATE_DONE;
		NDBG17(("\nncr_flush_lun: flush all done\n"));
		return;
	}
	if (nptp->nt_waitq != NULL) {
		/* requeue this target on the hba's work queue */
		NDBG17(("ncr_flush_lun: waitq not null\n"));
		nptp->nt_state = NPT_STATE_QUEUED;
		ncr_addbq(ncrp, nptp);
	}
#endif

	nptp->nt_state = NPT_STATE_DONE;
	NDBG17(("\nncr_flush_lun: done\n"));
	return;
}

#ifdef SOLARIS
/*
 * Flush all the disconnected requests for all the LUNs on
 * the specified target device.
 */
void
ncr_flush_target(	ncr_t	*ncrp, 
			ushort	 target,
			bool_t	 flush_all,
			u_char	 pkt_reason,
			u_long	 pkt_state,
			u_long	 pkt_statistics )
{
	npt_t	*nptp;
	ushort	 lun;

	/* completed Bus Device Reset, clean up disconnected LUNs */
	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		nptp = NTL2UNITP(ncrp, target, lun);
		if (nptp == NULL) {
			NDBG17(("ncr_flush_target: null\n"));
			continue;
		}
		if (nptp->nt_state != NPT_STATE_DISCONNECTED) {
			NDBG17(("ncr_flush_target: no disco\n"));
			continue;
		}
		ncr_flush_lun(ncrp, nptp, flush_all, pkt_reason, pkt_state
				  , pkt_statistics);
		ncrp->n_disc_num--;
	}
	NDBG17(("ncr_flush_target: done\n"));
	return;
}
#endif

/*
 * Called after a SCSI Bus Reset to find and flush all the outstanding
 * scsi requests for any devices which disconnected and haven't
 * yet reconnected. Also called to flush everything if we're resetting
 * the driver.
 */
void
ncr_flush_hba(	ncr_t	*ncrp, 
		bool_t	 flush_all,
		u_char	 pkt_reason,
		u_long	 pkt_state,
		u_long	 pkt_statistics )
{
	npt_t	**nptpp;
	npt_t	*nptp;
	ushort	cnt;
	
	NDBG17(("ncr_flush_hba: 0x%x %d\n", ncrp, flush_all));

	if (flush_all) {
		NDBG17(("ncr_flush_hba: all\n"));

		/* first flush the currently active request if any */
		if ((nptp = ncrp->n_current) != NULL
		&&  nptp->nt_state == NPT_STATE_ACTIVE) {
			NDBG17(("ncr_flush_hba: current nptp=0x%x\n", nptp));
			ncrp->n_current = NULL;
			ncr_flush_lun(ncrp, nptp, flush_all, pkt_reason
					  , pkt_state, pkt_statistics);
		}

#ifdef SOLARIS
		/* next, all the queued devices waiting for this hba */
		while ((nptp = ncr_rmq(ncrp)) != NULL) {
			NDBG17(("ncr_flush_hba: queued nptp=0x%x\n", nptp));
			ncr_flush_lun(ncrp, nptp, flush_all, pkt_reason
					  , pkt_state, pkt_statistics);
		}
#endif

	}

#ifdef SOLARIS
	/* finally (or just) flush all the disconnected target,luns */
	nptpp = &ncrp->n_pt[0];
	for (cnt = 0; cnt < sizeof ncrp->n_pt / sizeof ncrp->n_pt[0]; cnt++) {
		/* skip it if device is not in use */
		if ((nptp = *nptpp++) == NULL) {
			NDBG17(("ncr_flush_hba: null\n"));
			continue;
		}
		/* skip it if it's the wrong state */
		if (nptp->nt_state != NPT_STATE_DISCONNECTED) {
			NDBG17(("ncr_flush_hba: no disco\n"));
			continue;
		}
		ncr_flush_lun(ncrp, nptp, flush_all, pkt_reason, pkt_state
				  , pkt_statistics);
		ncrp->n_disc_num--;
	}
#endif

#ifdef NCR_DEBUG
if (ncrp->n_disc_num != 0) {
    NDBG17(("ncr_flush_hba: n_disc_num=%d\n",ncrp->n_disc_num));
    debug_enter("\nn_disc_num invalid\n");
}
#endif
	NDBG17(("ncr_flush_hba: done\n"));
	return;
}


void
ncr_set_done(	ncr_t	*ncrp,
		npt_t	*nptp,
		struct scsi_pkt	*pktp,
		u_char	 pkt_reason,
		u_long	 pkt_state,
		u_long	 pkt_statistics )
{
#ifdef SOLARIS
	struct scsi_pkt	*pktp;

	NDBG17(("ncr_set_done: ncrp=0x%x nptp=0x%x nccbp=0x%x %d\n", ncrp
				, nptp, nccbp, pkt_reason));
	pktp = NCCBP2PKTP(nccbp);
#endif
	pktp->pkt_reason = pkt_reason;
	pktp->pkt_state |= pkt_state;
	pktp->pkt_statistics |= pkt_statistics;
#ifdef SOLARIS
	ncr_doneq_add(ncrp, nccbp);
#endif
	return;
}
