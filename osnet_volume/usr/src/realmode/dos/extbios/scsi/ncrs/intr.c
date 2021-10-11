/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: NCR 710/810 EISA SCSI HBA       (intr.c)
 *
#pragma ident	"@(#)intr.c	1.6	97/07/21 SMI"
 *
 */

/*
 * Routines in this file are based on equivalent routines in the Solaris
 * NCR 710/810 driver file interrupt.c.  Some have been simplified for use in
 * a single-threaded environment.  The filename was changed because it
 * is not a valid DOS name.
 */
/* #define DEBUG */

#ifdef DEBUG
    #pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
#endif


#include <types.h>
#include "ncr.h"

#define ncr_addbq(ncrp, nptp)	(ncrp)->n_forwp = (ncrp)->n_backp = (nptp);
#define ncr_rmq(ncrp, nptp)	(ncrp)->n_forwp = (ncrp)->n_backp = 0;

extern ulong inl();

/* Utility routine; check result of SCSI command (the SCSI status byte). */

static void
ncr_process_intr(	ncr_t	*ncrp,
			unchar	 istat )
{
	ulong		 action = 0;

	NDBG16(("ncr_process_intr: ioaddr=0x%x: istat=0x%x\n",
		ncrp->n_ioaddr, istat));

#ifdef SOLARIS
	/* Always clear sigp bit if it might still be set */
	if (ncrp->n_state == NSTATE_WAIT_RESEL)
		NCR_RESET_SIGP(ncrp);
#endif

	/* Analyze DMA interrupts */
	if (istat & NB_ISTAT_DIP)
		action |= NCR_DMA_STATUS(ncrp);

	/* Analyze SCSI errors and check for phase mismatch */
	if (istat & NB_ISTAT_SIP)
		action |= NCR_SCSI_STATUS(ncrp);

	/* if no errors, no action, just restart the HBA */
	if (action != 0) {
		action = ncr_decide(ncrp, action);
	}

	/* restart the current, or start a new, queue item */
	ncr_restart_hba(ncrp, action);

	return;
}


/*
 * Some event or combination of events has occurred. Decide which
 * one takes precedence and do the appropiate HBA function and then
 * the appropiate end of request function.
 */
static ulong
ncr_decide(	ncr_t	*ncrp,
		ulong	 action )
{
	npt_t	*nptp;

	/*
	 * If multiple errors occurred do the action for
	 * the most severe error.
	 */

	nptp = ncrp->n_current;

	if (action & NACTION_CHK_INTCODE) {
		action = ncr_check_intcode(ncrp, nptp, action);
	}

#ifdef SOLARIS
	/* if sync i/o negotiation in progress, determine new syncio state */
	if (ncrp->n_state == NSTATE_ACTIVE
	&& (action & (NACTION_DO_BUS_RESET | NACTION_GOT_BUS_RESET))
	== 0) {
		if (NSYNCSTATE(ncrp, nptp) == NSYNC_SDTR_SENT
		||  NSYNCSTATE(ncrp, nptp) == NSYNC_SDTR_RCVD) {
			action = ncr_syncio_decide(ncrp, nptp, action);
		}
	}
#endif

	if (action & NACTION_GOT_BUS_RESET) {
		/* clear all requests waiting for reconnection */
		ncr_flush_hba(ncrp, FALSE, CMD_RESET, STATE_GOT_BUS
				  , STAT_BUS_RESET);
	}

	if (action & NACTION_SIOP_REINIT) {
		NCR_RESET(ncrp);
		NCR_INIT(ncrp);
#ifdef SOLARIS
		NCR_ENABLE_INTR(ncrp);
#endif
		/* the reset clears the byte counters so can't do save */
		action &= ~NACTION_SAVE_BCNT;
#ifdef SOLARIS
		cmn_err(CE_WARN, "ncrs: HBA reset: instance=%d\n",
			ddi_get_instance(ncrp->n_dip));
#endif
	}

	if (action & NACTION_SIOP_HALT) {
		NCR_HALT(ncrp);
#ifdef SOLARIS
		cmn_err(CE_WARN, "ncrs: HBA halt: instance=%d\n",
			ddi_get_instance(ncrp->n_dip));
#endif
	}

	if (action & NACTION_DO_BUS_RESET) {
		NCR_BUS_RESET(ncrp);
		/* clear invalid actions if any */
		action &= NACTION_DONE | NACTION_ERR | NACTION_DO_BUS_RESET
					| NACTION_BUS_FREE;
		/* clear all requests waiting for reconnection */
		ncr_flush_hba(ncrp, FALSE, CMD_RESET, STATE_GOT_BUS
				  , STAT_BUS_RESET);
#ifdef SOLARIS
		/* set all targets on this hba to renegotiate syncio */
		ncr_syncio_reset(ncrp, NULL);
		cmn_err(CE_WARN, "ncrs: bus reset: instance=%d\n",
			ddi_get_instance(ncrp->n_dip));
#endif
	}

#ifdef SOLARIS
	if (action & NACTION_SAVE_BCNT) {
		/* Save the state of the data transfer scatter/gather for
		 * possible later reselect/reconnect.
		 */
		if (!NCR_SAVE_BYTE_COUNT(ncrp, nptp)) {
			/* if this isn't an interrupt during a S/G dma */
			/* then the target changed phase when it shouldn't */
			NDBG1(("ncr_decide: phase mismatch: nptp=0x%x\n"
				, nptp));
		}
	}
#endif

	/*
	 * Check to see if the current request has completed.
	 * If the HBA isn't active it can't be done, we're probably
	 * just waiting for reselection and now need to reconnect to
	 * a new target.
	 */
	if (ncrp->n_state == NSTATE_ACTIVE) {
		action = ncr_ccb_decide(ncrp, nptp, action);
	}
	return (action);
}

static ulong
ncr_ccb_decide(	ncr_t	*ncrp,
		npt_t	*nptp,
		ulong	 action )
{
#ifdef SOLARIS
	nccb_t	*nccbp;
#endif
	struct scsi_pkt *pkt;

	if (action & NACTION_ERR) {
		/* error completion, save all the errors seen for later */	
		nptp->nt_goterror = TRUE;

	} else if ((action & NACTION_DONE) == 0) {
		/* the target's state hasn't changed */
		return (action);
	}

	/* detach this target from the hba */
	ncrp->n_current = NULL;
	ncrp->n_state = NSTATE_IDLE;

#ifdef SOLARIS
	/* if this target has more requests then requeue it fifo order */
	if (nptp->nt_waitq != NULL) {
		nptp->nt_state = NPT_STATE_QUEUED;
		ncr_addbq(ncrp, nptp);
	} else {
		nptp->nt_state = NPT_STATE_DONE;
	}
#endif
	nptp->nt_state = NPT_STATE_DONE;

	/* if no active request then just return */
	if ((pkt = nptp->nt_pktp) == NULL)
		return (action);

	/* decouple the request from the target */
	/* nptp->nt_pktp = NULL*/;

	/* post the completion status into the scsi packet */
	ncr_chkstatus(ncrp, nptp, pkt);

#ifdef SOLARIS
	/* add the completed request to end of the done queue */
	ncr_doneq_add(ncrp, nccbp);
#endif
	ncr_rmq(ncrp, nptp);
	return (action);

}

bool_t
ncr_wait_intr( ncr_t *ncrp  )
{
	ushort	cnt;
	unchar	istat;

	istat = NCR_GET_ISTAT(ncrp);

	/* keep trying for at least 25 seconds */
	for (cnt = 0; cnt < 50000; cnt += 1) {
		/* loop 50,000 times but wait at least 500 microseconds */
		/* each time around the loop */
		if (istat & (NB_ISTAT_DIP | NB_ISTAT_SIP)) {
			NDBG17(("ncr_wait_intr: istat=0x%x\n", istat));
			/* process this interrupt */
			ncr_process_intr(ncrp, istat);
			if (ncrp->n_state == NSTATE_IDLE)
				return (TRUE);
		}
		drv_usecwait(500);
		istat = NCR_GET_ISTAT(ncrp);
	}
	return (FALSE);
}

#ifdef SOLARIS
/*  
 * Autovector interrupt entry point.  Passed to ddi_add_intr() in 
 * ncr_attach().
 */

u_int
ncr_intr( caddr_t arg )
{
	ncr_t	*ncrp;
	nccb_t	*nccbp;
	unchar	 istat;


	ncrp = NCR_BLKP(arg);

	mutex_enter(&ncrp->n_mutex);

	istat = NCR_GET_ISTAT(ncrp);
	if ((istat & (NB_ISTAT_DIP | NB_ISTAT_SIP)) == 0) {
		/* interrupt status not true */
		mutex_exit(&ncrp->n_mutex);
		return (DDI_INTR_UNCLAIMED);
	}

	for (;;) {
		/* clear the next interrupt status from the hardware */
		ncr_process_intr(ncrp, istat);

		/* run the completion routines of all the completed commands */
		while ((nccbp = ncr_doneq_rm(ncrp)) != NULL) {
#if 0
			struct scsi_pkt *pktp;

			mutex_exit(&ncrp->n_mutex);
			pktp = NCCBP2PKTP(nccbp);
			(*pktp->pkt_comp)(pktp);
			mutex_enter(&ncrp->n_mutex);
#else
			/* run this command's completion routine */
			mutex_exit(&ncrp->n_mutex);
			scsi_run_cbthread(ncrp->n_cbthdl, NCCBP2SCMDP(nccbp));
			mutex_enter(&ncrp->n_mutex);
#endif
		}

		/* get the next hardware interrupt status */
		istat = NCR_GET_ISTAT(ncrp);
		if ((istat & (NB_ISTAT_DIP | NB_ISTAT_SIP)) == 0)
			break;
	}

	NDBG17(("ncr_intr complete\n"));
	mutex_exit(&ncrp->n_mutex);
	return (DDI_INTR_CLAIMED); 
}
#endif

static int
ncr_setup_npt(	ncr_t	*ncrp,
		npt_t	*nptp )
#ifdef SOLARIS
		nccb_t	*nccbp )
#endif
{
#ifdef SOLARIS
	struct scsi_pkt	*pktp = NCCBP2PKTP(nccbp);
#endif

	NDBG17(("ncr_setup_npt: nptp=0x%x\n", nptp));

#ifdef SOLARIS
	nptp->nt_type = nccbp->nc_type;
#endif
	nptp->nt_goterror = FALSE;
	nptp->nt_dma_status = 0;
	nptp->nt_scsi_status0 = 0;
	nptp->nt_scsi_status1 = 0;

	nptp->nt_statbuf[0] = 0;
	nptp->nt_errmsgbuf[0] = MSG_NOP;

	switch (nptp->nt_type) {
	case NRQ_NORMAL_CMD:
		NDBG17(("ncr_setup_npt: normal\n"));
		/* reset the msg out length for single identify byte */
		nptp->nt_sendmsg.count = 1;

		/* target disconnect is not permitted */
		nptp->nt_identify[0] = MSG_IDENTIFY | nptp->nt_lun;

		nptp->nt_savedp.nd_left = 1;
		nptp->nt_curdp = nptp->nt_savedp;
		
#ifdef SOLARIS
		/* assume target disconnect is permitted for this lun */
		nptp->nt_identify[0] = MSG_IDENTIFY | INI_CAN_DISCON
						    | nptp->nt_lun;

		/* check to see if disconnect is really not allowed */
		if ((ncrp->n_nodisconnect[nptp->nt_target] == TRUE)
		||  ((pktp->pkt_flags & (FLAG_NOINTR | FLAG_NODISCON)) != 0))
			nptp->nt_identify[0] &= ~INI_CAN_DISCON;

		/* copy the command into the target's DSA structure */
		nptp->nt_cdb = nccbp->nc_cdb;
		nptp->nt_cmd.count = nccbp->nc_cdblen;

		/* setup the Scatter/Gather DMA list for this request */
		ncr_sg_setup(ncrp, nptp, nccbp);

		/*******************************/

		/* SCSI-2 spec section 6.6.21, do SDTR for these commands */
		if (nptp->nt_cdb.scc_cmd == SCMD_INQUIRY
		||  nptp->nt_cdb.scc_cmd == SCMD_REQUEST_SENSE) {
			NDBG31(("ncr_setup_npt: inq or reqsen\n"));
			ncr_syncio_reset(ncrp, nptp);
		}

		if (NSYNCSTATE(ncrp, nptp) == NSYNC_SDTR_NOTDONE) {
			NDBG31(("ncr_setup_npt: try syncio\n"));
			/* haven't yet tried syncio on this target */
			ncr_syncio_msg_init(ncrp, nptp);
			ncr_syncio_state(ncrp, nptp, NSYNC_SDTR_SENT, 0, 0);
		} else {
			nptp->nt_sendmsg.count = 1;
		}
#endif

#if defined(__ppc)
		NDBG31(("cdb cmd size: %d\tphysaddr: 0x%x\n",
		    nptp->nt_cmd.count, nptp->nt_cmd.address));
		NDBG31(("active request ccbp: 0x%x\n", nptp->nt_nccbp));
		NDBG31(("table indirect physaddr: 0x%x\n",
		    nptp->nt_dsa_physaddr));
		NDBG31(("current S/G pointers: 0x%x\n", nptp->nt_curdp));
#endif
		return (NSS_STARTUP);

	case NRQ_DEV_RESET:
		NDBG31(("ncr_setup_npt: bus device reset\n"));
		/* reset the msg out length for single message byte */
		nptp->nt_sendmsg.count = 1;
		nptp->nt_identify[0] = MSG_DEVICE_RESET;

		/* no command buffer */
		nptp->nt_cmd.count = 0;

#ifdef SOLARIS
		ncr_syncio_reset(ncrp, nptp);
#endif
		return (NSS_BUS_DEV_RESET);

#ifdef SOLARIS
	case NRQ_ABORT:
		NDBG31(("ncr_setup_npt: abort\n"));
		/* reset the msg out length for two single */
		/* byte messages */
		nptp->nt_sendmsg.count = 2;
		nptp->nt_identify[0] = MSG_IDENTIFY
				     | (nptp->nt_lun & ~INI_CAN_DISCON);
		nptp->nt_abortmsg = MSG_ABORT;

		/* no command buffer */
		nptp->nt_cmd.count = 0;
		return (NSS_BUS_DEV_RESET);

	default:
		cmn_err(CE_PANIC, "ncrs: invalid queue entry nccbp=0x%x\n"
				, nccbp);
#endif
	}
}



/*
 * start a fresh request from the top of the device queue
 */
static void
ncr_start_next(	ncr_t *ncrp )
{
	npt_t	*nptp;
#ifdef SOLARIS
	nccb_t	*nccbp;
#endif
	int	 script_type;

	NDBG31(("ncr_start_next: ncrp=0x%x\n", ncrp));

	if ((nptp = ncrp->n_forwp) == NULL) {
		ncrp->n_state = NSTATE_IDLE;
		return;
	}

#ifdef SOLARIS
	if ((nptp = ncr_rmq(ncrp)) == NULL) {
		/* no devs waiting for the hba, wait for disco-ed dev */
		ncr_wait_for_reselect(ncrp, 0);
		return;
	}
	if ((nccbp = ncr_waitq_rm(nptp)) == NULL) {
		/* the request queue is empty, wait for disconnected devs */
		ncr_wait_for_reselect(ncrp, 0);
		return;
	}
#endif

	/* attach this target to the hba and make it active */
	ncrp->n_current = nptp;

#ifdef SOLARIS
	nptp->nt_nccbp = nccbp;
#endif
	ncrp->n_state = NSTATE_ACTIVE;
	nptp->nt_state = NPT_STATE_ACTIVE;

#ifdef SOLARIS
	script_type = ncr_setup_npt(ncrp, nptp, nccbp);
#endif
	script_type = ncr_setup_npt(ncrp, nptp);

	NCR_SETUP_SCRIPT(ncrp, nptp);
	NCR_START_SCRIPT(ncrp, script_type);
	return;

}

#ifdef SOLARIS
static void
ncr_wait_for_reselect(	ncr_t	*ncrp,
			ulong	 action )
{
	npt_t	*nptp;


	NDBG31(("ncr_wait_for_reselect: ncrp=0x%x 0x%x\n", ncrp, action));

	nptp = ncrp->n_hbap;
	ncrp->n_current = nptp;
	ncrp->n_state = NSTATE_WAIT_RESEL;
	nptp->nt_state = NPT_STATE_WAIT;
	nptp->nt_errmsgbuf[0] = MSG_NOP;


	action &= NACTION_ABORT | NACTION_MSG_REJECT | NACTION_MSG_PARITY
			| NACTION_INITIATOR_ERROR;

	if (action == 0 && ncrp->n_disc_num != 0) {
		/* wait for any disconnected targets */
		NCR_SETUP_SCRIPT(ncrp, nptp);
		NCR_START_SCRIPT(ncrp, NSS_WAIT4RESELECT);
		NDBG19(("ncr_wait_for_reselect: WAIT\n"));
		return;
	}

	if (action & NACTION_ABORT) {
		/* abort an invalid reconnect */
		nptp->nt_errmsgbuf[0] = MSG_ABORT;
		NCR_START_SCRIPT(ncrp, NSS_ABORT);
		return;
	}

	if (action & NACTION_MSG_REJECT) {
		/* target sent me bad identify msg, send msg reject */
		nptp->nt_errmsgbuf[0] = MSG_REJECT;
		NCR_START_SCRIPT(ncrp, NSS_ERR_MSG);
		NDBG19(("ncr_wait_for_reselect: Message Reject\n"));
		return;
	}

	if (action & NACTION_MSG_PARITY) {
		/* got a parity error during message in phase */
		nptp->nt_errmsgbuf[0] = MSG_MSG_PARITY;
		NCR_START_SCRIPT(ncrp, NSS_ERR_MSG);
		NDBG19(("ncr_wait_for_reselect: Message Parity Error\n"));
		return;
	}

	if (action & NACTION_INITIATOR_ERROR) {
		/* catchall for other errors */
		nptp->nt_errmsgbuf[0] = MSG_INITIATOR_ERROR;
		NCR_START_SCRIPT(ncrp, NSS_ERR_MSG);
		NDBG19(("ncr_wait_for_reselect: Initiator Detected Error\n"));
		return;
	}

	/* no disconnected targets go idle */
	ncrp->n_current = NULL;
	ncrp->n_state = NSTATE_IDLE;
	NDBG19(("ncr_wait_for_reselect: IDLE\n"));
	return;
}
#endif

/*
 * How the hba continues depends on whether sync i/o 
 * negotiation was in progress and if so how far along.
 * Or there might be an error message to be sent out.
 */
static void
ncr_restart_current(	ncr_t	*ncrp,
			ulong	 action )
{
	npt_t	*nptp = ncrp->n_current;

	NDBG19(("ncr_restart_current: ncrp=0x%x 0x%x\n", ncrp, action));

	if (nptp == NULL) {
		/* the current request just finished, do the next one */
		ncr_start_next(ncrp);
		return;
	}

	/* Determine how to get the device at the top of the queue restarted */
	nptp->nt_errmsgbuf[0] = MSG_NOP;
	switch (nptp->nt_state) {
	case NPT_STATE_ACTIVE:
		NDBG19(("ncr_restart_current: active\n"));
		action &= NACTION_ACK | NACTION_SDTR_OUT
			| NACTION_MSG_REJECT | NACTION_MSG_PARITY
			| NACTION_INITIATOR_ERROR | NACTION_GOT_MSGREJ;
		if (action == 0) {
			/* continue the script on the currently active target */
			NCR_START_SCRIPT(ncrp, NSS_CONTINUE);
			break;
		}

		if (action & NACTION_ACK) {
			/* just ack the last byte and continue */
			NCR_START_SCRIPT(ncrp, NSS_CLEAR_ACK);
			break;
		}
#ifdef SOLARIS
		if (action & NACTION_SDTR_OUT) {
			/* send my SDTR message */
			NCR_START_SCRIPT(ncrp, NSS_SYNC_OUT);
			break;
		}
#endif

		if (action & NACTION_MSG_REJECT) {
			/* target sent me bad msg, send msg reject */
			nptp->nt_errmsgbuf[0] = MSG_REJECT;
			NCR_START_SCRIPT(ncrp, NSS_ERR_MSG);
			break;
		}

		
		if (action & NACTION_GOT_MSGREJ) {
			/* got msg reject from target */
			nptp->nt_goterror = TRUE;
			/* ack the last byte and continue */
			NCR_START_SCRIPT(ncrp, NSS_CLEAR_ACK);
		}

		if (action & NACTION_MSG_PARITY) {
			/* got a parity error during message in phase */
			nptp->nt_errmsgbuf[0] = MSG_MSG_PARITY;
			NCR_START_SCRIPT(ncrp, NSS_ERR_MSG);
			break;
		}

		if (action & NACTION_INITIATOR_ERROR) {
			/* catchall for other errors */
			nptp->nt_errmsgbuf[0] = MSG_INITIATOR_ERROR;
			NCR_START_SCRIPT(ncrp, NSS_ERR_MSG);
		}
		break;

#ifdef SOLARIS
	case NPT_STATE_DISCONNECTED:
		NDBG19(("ncr_restart_current: disconnected\n"));
		/* a target wants to reconnect so make
		 * it the currently active target 
		 */
		NCR_SETUP_SCRIPT(ncrp, nptp);
		NCR_START_SCRIPT(ncrp, NSS_CLEAR_ACK);
		break;

#endif
	default:
#ifdef SOLARIS
		cmn_err(CE_WARN, "ncr_restart_current: invalid state %d\n"
				, nptp->nt_state);
#endif
		printf("ncr_restart_current: invalid state %d\n", 
			nptp->nt_state);
		return;
	}
	nptp->nt_state = NPT_STATE_ACTIVE;
	NDBG19(("ncr_restart_current: okay\n"));
	return;
}

static void
ncr_restart_hba(	ncr_t	*ncrp,
			ulong	 action )
{

	NDBG19(("ncr_restart_hba: ncrp=0x%x\n", ncrp));

	/*
	 * run the target at the front of the queue unless we're
	 * just waiting for a reconnect. In which case just use
	 * the first target's data structure since it's handy.
	 */
	switch (ncrp->n_state) {
	case NSTATE_ACTIVE:
		NDBG19(("ncr_restart_hba: ACTIVE\n"));
		ncr_restart_current(ncrp, action);
		break;

	case NSTATE_IDLE:
		NDBG19(("ncr_restart_hba: IDLE\n"));
		/* start whatever's on the top of the queue */
		ncr_start_next(ncrp);
		break;

#ifdef SOLARIS
	case NSTATE_WAIT_RESEL:
		NDBG19(("ncr_restart_hba: WAIT\n"));
		ncr_wait_for_reselect(ncrp, action);
		break;
#endif
	}
	return;
}

void
ncr_queue_target(	ncr_t	*ncrp,
			npt_t	*nptp )
{
	unchar	istat;

	NDBG1(("ncr_queue_target\n"));

	/* add this target to the end of the hba's queue */
	nptp->nt_state = NPT_STATE_QUEUED;
	ncr_addbq(ncrp, nptp);

	switch (ncrp->n_state) {
	case NSTATE_IDLE:
		/* the device is idle, start first queue entry now */
		ncr_restart_hba(ncrp, 0);
		break;
	case NSTATE_ACTIVE:
		/* queue the target and return without doing anything */
		break;
#ifdef SOLARIS
	case NSTATE_WAIT_RESEL:
		/* If we're waiting for a reselection of a disconnected target
		 * then set the Signal Process bit in the ISTAT register
		 * and return. The interrupt routine handles restarting
		 * the queue
		 */
		NCR_SET_SIGP(ncrp);
		break;
#endif
	}
	return;
}

#ifdef SOLARIS
static npt_t *
ncr_get_target( ncr_t *ncrp )
{
	unchar	target;
	unchar	lun;

	NDBG1(("\nGET TARGET\n"));

	/* get the LUN from the IDENTIFY message byte */
	lun = ncrp->n_hbap->nt_msginbuf[0];
	if (IS_IDENTIFY_MSG(lun) == FALSE)
		return (NULL);
	lun = lun & MSG_LUNRTN;

	/* get the target from the HBA's id register */
	if (NCR_GET_TARGET(ncrp, &target))
		return (NTL2UNITP(ncrp, target, lun));

	return (NULL);
}

#endif

static ulong
ncr_check_intcode(	ncr_t	*ncrp,
			npt_t	*nptp,
			ulong	 action )
{
	npt_t	*re_nptp;
	int	 target;
	int	 lun;
	ulong	 intcode;
	char	*errmsg;

	if (action & (NACTION_GOT_BUS_RESET | NACTION_DO_BUS_RESET
			| NACTION_SIOP_HALT | NACTION_SIOP_REINIT
			| NACTION_BUS_FREE | NACTION_DONE | NACTION_ERR)) {
		return (action);
	}
	/* SCRIPT interrupt instruction */
	/* Get the interrupt vector number */
	intcode = NCR_GET_INTCODE(ncrp);

	NDBG1(("ncr_check_intcode=0x%lx\n", intcode));

	switch (intcode) {
	default:
		break;

	case NINT_OK:
		return (NACTION_DONE | action);

	case NINT_SDP_MSG:
		/* Save Data Pointers msg */
		NDBG1(("\n\nintcode SDP\n\n"));
#ifdef SOLARIS
		nptp->nt_savedp = nptp->nt_curdp;
		return (NACTION_ACK | action);
#else
		break;
#endif

	case NINT_DISC:
		NDBG1(("\n\nintcode DISC\n\n"));
#ifdef SOLARIS
		/* detach this target from the hba */
		ncrp->n_current = NULL;

		/* increment count of disconnected targets */
		ncrp->n_disc_num++;	

		/* adjust the target's and hba's states */
		nptp->nt_state = NPT_STATE_DISCONNECTED;
		ncrp->n_state = NSTATE_IDLE;
		return (action);
#else
		break;
#endif

	case NINT_RP_MSG:
		/* Restore Data Pointers */
		NDBG1(("\n\nintcode RP\n\n"));
#ifdef SOLARIS
		nptp->nt_curdp = nptp->nt_savedp;
		return (NACTION_ACK | action);
#else
		break;
#endif

	case NINT_RESEL:
		/* reselected by a disconnected target */
		NDBG1(("\n\nintcode RESEL\n\n"));
#ifdef SOLARIS
		/* One of two situations:
		 *
		 */
		switch (ncrp->n_state) {
		case NSTATE_ACTIVE:
			/* Reselection during select. Leave the request I */
			/* was trying to activate on the top of the queue. */
			nptp->nt_state = NPT_STATE_QUEUED;
			ncr_waitq_add_lifo(nptp, nptp->nt_nccbp);
			nptp->nt_nccbp = NULL;
			ncr_addfq(ncrp, nptp);

			/* make it look like we're waiting for a reconnect */
			nptp = ncrp->n_hbap;
			ncrp->n_current = nptp;
			ncrp->n_state = NSTATE_WAIT_RESEL;
			nptp->nt_state = NPT_STATE_WAIT;
			break;

		case NSTATE_WAIT_RESEL:
			/* Target reselected while hba was waiting. */
			/* nptp points to the HBA which is never really */
			/* queued. */
			break;

		default:
			/* should never happen */
			NDBG1(("\n\nintcode RESEL botched\n\n"));
			return (NACTION_DO_BUS_RESET | action);
		}


		/* Get target structure of device that wants to reconnect */
		if ((re_nptp = ncr_get_target(ncrp)) == NULL) {
			/* invalid reselection */
			return (NACTION_DO_BUS_RESET | action);
		}

		if (re_nptp->nt_state != NPT_STATE_DISCONNECTED) {
			/* send out ABORT message using hba's npt struct */
			return (NACTION_ABORT | action);
		}

		nptp = re_nptp;

		/* implicit restore data pointers */
		nptp->nt_curdp = nptp->nt_savedp;

		/* one less outstanding disconnected target */
		ncrp->n_disc_num--;

		/* make this target the currently active one */
		ncrp->n_current= nptp;
		ncrp->n_state = NSTATE_ACTIVE;
		return (NACTION_ACK | action);
#else
		break;
#endif

	case NINT_SIGPROC:
		/* Give up waiting, start up another target */
#ifdef SOLARIS
		if (ncrp->n_state != NSTATE_WAIT_RESEL) {
			/* big trouble, bus reset time ? */
			return (NACTION_DO_BUS_RESET | NACTION_ERR | action);
		}

		NDBG31(("%s", (NCR_GET_ISTAT(ncrp) & NB_ISTAT_CON)
				? "ncrs: connected after sigproc\n"
				: ""));
		ncrp->n_state = NSTATE_IDLE;
		return (action);
#else
		break;
#endif

	case NINT_SDTR:
		if (ncrp->n_state != NSTATE_ACTIVE) {
			/* reset the bus */
			return (NACTION_DO_BUS_RESET);

		}
		switch (ncrp->n_syncstate) {
		default:
			/* reset the bus */
			NDBG31(("\n\nintcode SDTR state botch\n\n"));
			return (NACTION_DO_BUS_RESET);

		case NSYNC_SDTR_REJECT:
			/* just ignore it, since we're not doing sync i/o */
			NDBG31(("\n\nintcode SDTR reject\n\n"));
			return (NACTION_ACK | action);

#ifdef SOLARIS
		/* XXXX shouldn't have the following cases */
		case NSYNC_SDTR_DONE:
			/* target wants to renegotiate */
			NDBG31(("\n\nintcode SDTR done, renegotiate\n\n"));
			ncr_syncio_reset(ncrp, nptp);
			NSYNCSTATE(ncrp, nptp) = NSYNC_SDTR_RCVD;
			break;

		case NSYNC_SDTR_NOTDONE:
			/* target initiated negotiation */
			NDBG31(("\n\nintcode SDTR notdone\n\n"));
			ncr_syncio_reset(ncrp, nptp);
			NSYNCSTATE(ncrp, nptp) = NSYNC_SDTR_RCVD;
			break;

		case NSYNC_SDTR_SENT:
			/* target responded to my negotiation */
			NDBG31(("\n\nintcode SDTR sent\n\n"));
			break;
#endif
		}
		return (NACTION_SDTR | action);

	case NINT_WDTR:
		if (ncrp->n_state != NSTATE_ACTIVE) {
			/* reset the bus */
			return (NACTION_DO_BUS_RESET);
		}

		/* just ignore it, since we're not doing wide i/o */
		NDBG31(("\n\nintcode WDTR reject\n\n"));
		return (NACTION_ACK | action);

	case NINT_SDTR_REJECT:
#ifdef SOLARIS
		/* set all LUNs on this target to async i/o */
		NDBG31(("\n\nintcode SDTR_REJECT \n\n"));
		ncr_syncio_state(ncrp, nptp, NSYNC_SDTR_DONE, 0, 0);
		return (NACTION_ACK | action);
#else
		break;
#endif

	case NINT_MSGREJ:
		if (ncrp->n_state != NSTATE_ACTIVE) {
			/* reset the bus */
			return (NACTION_DO_BUS_RESET);
		}

#ifdef SOLARIS
		if (NSYNCSTATE(ncrp, nptp) == NSYNC_SDTR_SENT) {
			/* the target can't handle sync i/o */
			ncr_syncio_state(ncrp, nptp, NSYNC_SDTR_DONE, 0, 0);
			return (NACTION_ACK | action);
		}
#endif
		/* some devices respond to an invalid LUN with a MSG_REJECT */
		/* e.g. the SUN CDROM PN:5995-1929-05 */
		return (NACTION_GOT_MSGREJ);

	case NINT_DEV_RESET:
		if (nptp->nt_type != NRQ_DEV_RESET) {
			NDBG30(("ncr_intcode: abort completed\n"));
			return (NACTION_DONE | action);
		}

#ifdef SOLARIS
		/* clear disco-ed requests for all the LUNs on this device */
		ncr_flush_target(ncrp, nptp->nt_target, FALSE, CMD_RESET
				     , STATE_GOT_BUS, STAT_BUS_RESET);

		/* clear sync i/o state on this target */
		ncr_syncio_reset(ncrp, nptp);
#endif
		NDBG30(("ncr_intcode: bus device reset completed\n"));
		return (NACTION_DONE | action);
	}

	/* All of the interrupt codes handled above are the of
	 * the "expected" non-error type. The following interrupt
	 * codes are for unusual errors detected by the SCRIPT.
	 * For now treat all of them the same, mark the request
	 * as failed due to an error and then reset the SCSI bus.
	 */
    handle_error:

	/*
	 * Some of these errors should be getting BUS DEVICE RESET
	 * rather than bus_reset.
	 */
	switch (intcode) {
	case NINT_SELECTED:
		errmsg = "got selected\n";
		break;
	case NINT_UNS_MSG:
		errmsg = "got an unsupported message ";
		break;
	case NINT_ILI_PHASE:
		errmsg = "got incorrect phase ";
		break;
	case NINT_UNS_EXTMSG:
		errmsg = "got unsupported extended message ";
		break;
	case NINT_MSGIN:
		errmsg = "Message-In was expected ";
		break;
	case NINT_MSGREJ:
		errmsg = "got unexpected Message Reject ";
		break;
	case NINT_REJECT:
		errmsg = "unable to send Message Reject ";
		break;
	case NINT_TOOMUCHDATA:
		errmsg = "got too much data to/from target ";
		break;

	default:
#ifdef SOLARIS
		cmn_err(CE_WARN, "ncrs: invalid intcode=%x\n", intcode);
#endif
		errmsg = "invalid intcode";
		break;
	}

    bus_reset:
#ifdef SOLARIS
	cmn_err(CE_WARN, "ncrs: Resetting scsi bus, %s(%d,%d)\n"
			, errmsg, nptp->nt_target, nptp->nt_lun);
#endif
	printf("ncrs: Resetting scsi bus, %s(%d,%d, 0x%x)\n"
			, errmsg, nptp->nt_target, nptp->nt_lun, intcode);
	return (NACTION_DO_BUS_RESET | NACTION_ERR);
}

/*
 * Figure out the recovery for a parity interrupt or a SGE interrupt.
 */
ulong
ncr_parity_check( unchar phase )
{
	NDBG1(("ncr_parity_check: 0x%x\n", phase));

	switch (phase) {
	case NSOP_MSGIN:
		return (NACTION_MSG_PARITY);
	case NSOP_MSGOUT:
		return (NACTION_DO_BUS_RESET | NACTION_ERR);
	case NSOP_COMMAND:
	case NSOP_STATUS:
		return (NACTION_INITIATOR_ERROR);
	case NSOP_DATAIN:
	case NSOP_DATAOUT:
		return (NACTION_SAVE_BCNT | NACTION_INITIATOR_ERROR);
	}
}
