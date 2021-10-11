/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dbri_isr.c	1.34	97/10/31 SMI"

/*
 * Sun DBRI interrupt handling routines
 */

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/ioccom.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/isdnio.h>
#include <sys/dbriio.h>
#include <sys/dbrireg.h>
#include <sys/dbrivar.h>
#include <sys/audiodebug.h>


/*
 * Global variables
 */
#if defined(AUDIOTRACE)
int Dbri_debug_sbri = 0;	/* SBRI Activation/Deactivation messages */
#endif


/*
 * Local function prototypes
 */
static void dbri_rxintr(dbri_unit_t *, dbri_intrq_ent_t);
static void dbri_txintr(dbri_unit_t *, dbri_intrq_ent_t);
static void dbri_sbri_intr(dbri_unit_t *, dbri_intrq_ent_t);
static void dbri_txeol(dbri_stream_t *);
static void dbri_rxeol(dbri_stream_t *);
#if defined(DBRI_SWFCS)
static boolean_t dbri_checkfcs(dbri_unit_t *, dbri_cmd_t *);
#endif
#if defined(AUDIOTRACE)
static void pr_interrupt(dbri_intrq_ent_t *);
#endif


/*
 * Need to remember that interrupts can occur without any devices open as
 * they are enabled at the end of the attach routine. Only upon unload
 * will they be disabled.
 */
uint_t
dbri_intr(caddr_t arg)
{
	dbri_unit_t *unitp = (dbri_unit_t *)(void *)arg;
	uint_t intr_reg;
	dbri_intrq_ent_t intr;
	dbri_pipe_t *pipep;
	int serviced = DDI_INTR_UNCLAIMED;
	dbri_stream_t *ds;

	if ((unitp == NULL) || (unitp->regsp == NULL))
		return (serviced);

	LOCK_UNIT(unitp);
	DTRACE(dbri_intr, 'unit', unitp);

	intr_reg = unitp->regsp->n.intr; /* auto clr on read */

	if (intr_reg)
		serviced = DDI_INTR_CLAIMED;

	if (!(intr_reg & (DBRI_INTR_REQ | DBRI_INTR_LATE_ERR |
	    DBRI_INTR_BUS_GRANT_ERR | DBRI_INTR_BURST_ERR |
	    DBRI_INTR_MRR_ERR))) {
		/* No interrupt from this DBRI */
		goto done;
	}

	if (intr_reg & DBRI_INTR_LATE_ERR)  {
		/*
		 * NB - There is nothing we are going to be able to do
		 * about this one.  If the system is really
		 * hosed, we're sure to hit more serious
		 * interrupts later.  So just re-enable DBRI
		 * master mode and continue on.
		 */
		DTRACE(dbri_intr, 'LATE', intr_reg);
		cmn_err(CE_WARN, "dbri: Multiple Late Error on SBus");
		unitp->regsp->n.sts &= ~(DBRI_STS_D);
	}

	if (intr_reg & DBRI_INTR_BUS_GRANT_ERR)  {
		/*
		 * NB - we don't know where this occurred so we
		 * cannot set any error flags to let a user
		 * process know that there is a data
		 * discontinuity.  This is BAD.
		 */
		DTRACE(dbri_intr, 'BGNT', intr_reg);
		cmn_err(CE_WARN, "dbri: Lost Bus Grant on SBus");
	}

	if (intr_reg & DBRI_INTR_BURST_ERR)  {
		if (unitp->ddi_burstsizes_avail & 0x10)
			unitp->regsp->n.sts |= DBRI_STS_G;
		if (unitp->ddi_burstsizes_avail & 0x20)
			unitp->regsp->n.sts |= DBRI_STS_E;
#if 0 /* DO NOT enable 16-word bursts on current DBRI!! */
		if (unitp->ddi_burstsizes_avail & 0x40)
			unitp->regsp->n.sts |= DBRI_STS_S;
#endif
		DTRACE(dbri_intr, 'BRST', intr_reg);
	}

	if (intr_reg & DBRI_INTR_MRR_ERR) {
		unitp->regsp->n.sts &= ~(DBRI_STS_D);
		DTRACE(dbri_intr, 'MRR ', intr_reg);
#if 0
		/*
		 * When DBRI is under load it is possible
		 * for the driver to unmap a buffer before
		 * DBRI has pulled the brakes on the DMA.
		 * Ignore the messages for now until this
		 * problem is fixed.
		 */
		cmn_err(CE_WARN, "dbri: S-Bus Multiple Error ACK");
#endif
	}

	/*
	 * If we don't have to check the queue, don't go to memory.
	 */
	if (!(intr_reg & DBRI_INTR_REQ))
		goto done;

	(void) DBRI_SYNC_CURINTQ(unitp);
	intr = unitp->intq.curqp->intr[unitp->intq.off];
	while (intr.f.ibits == DBRI_INT_IBITS) {
		DTRACE(dbri_intr, 'intr', intr.word);
		/*
		 * Clear out the IBITS for the current interrupt word
		 */
		unitp->intq.curqp->intr[unitp->intq.off].f.ibits = 0;

		pipep = (intr.f.channel >= DBRI_INT_MAX_CHAN) ?
		    NULL : &unitp->ptab[intr.f.channel];

		switch (intr.f.code) {
		case DBRI_INT_BRDY: /* Receive buffer ready */
			ATRACE(dbri_intr, 'BRDY', intr.word);
			dbri_rxintr(unitp, intr);
			break;

		case DBRI_INT_MINT: /* Marked interrupt in RD/TD */
			ATRACE(dbri_intr, 'MINT', intr.word);
			break;

		case DBRI_INT_IBEG: /* Flag to idle transition */
			ATRACE(dbri_intr, 'IBEG', intr.word);
			break;

		case DBRI_INT_IEND: /* Idle to flag transition */
			ATRACE(dbri_intr, 'IEND', intr.word);
			break;

		case DBRI_INT_EOL: /* End of list */
			ATRACE(dbri_intr, 'EOL ', intr.word);

			/* Check if stream closed or invalid EOL */
			if (!ISPIPEINUSE(pipep))
				break;
			if (pipep->ds == NULL || pipep->ds->pipep == NULL)
				break;

			ds = pipep->ds;
			if (ISXMITDIRECTION(DsToAs(ds))) {
				dbri_txeol(ds);
			} else {
				dbri_rxeol(ds);
			}
			break;

		case DBRI_INT_CMDI: /* first word of command read by DBRI */
			ATRACE(dbri_intr, 'CMDI', intr.word);
			/*
			 * NB: If REX commands are ever used for
			 * synchronization, they will be processed
			 * here.
			 */
			break;

		case DBRI_INT_XCMP: /* Xmit Frame complete */
			ATRACE(dbri_intr, 'XCMP', intr.word);
			dbri_txintr(unitp, intr);
			break;

		case DBRI_INT_SBRI: /* BRI status change info */
			ATRACE(dbri_intr, 'SBRI', intr.word);
			dbri_sbri_intr(unitp, intr);
			break;

		case DBRI_INT_FXDT: /* Fixed data change */
			ATRACE(dbri_intr, 'FXDT', intr.word);
			dbri_fxdt_intr(unitp, intr);
			break;

		case DBRI_INT_CHIL: /* CHI lost frame sync */
			if (intr.f.channel == DBRI_INT_CHI_CHAN) {
				ATRACE(dbri_intr, 'CHIL', intr.word);
				dbri_chil_intr(unitp, intr);
			} else {
				ATRACE(dbri_intr, 'COLL', intr.word);

				if (!ISTEDCHANNEL(DsToAs(pipep->ds)))
					break;

				/*
				 * XXX - Code needs to be updated to
				 * 1992/02/27 DBRI Spec
				 */
				dbri_stop(DsToAs(pipep->ds));
				drv_usecwait(250 * 1000);
				dbri_start(DsToAs(pipep->ds));
			}
			break;

		case DBRI_INT_DBYT: /* Dropped byte frame slip */
			/*
			 * This can only happen on a
			 * serial-to-serial pipe.
			 *
			 * XXX - Do we need to set an error flag
			 * somewhere?
			 */
			ATRACE(dbri_intr, 'DBYT', intr.word);
			break;

		case DBRI_INT_RBYT: /* Repeated byte frame slip */
			/*
			 * This can only happen on a
			 * serial-to-serial pipe.
			 *
			 * XXX - Do we need to set an error flag
			 * somewhere?
			 */
			ATRACE(dbri_intr, 'RBYT', intr.word);
			break;

		case DBRI_INT_LINT: /* Lost interrupt */
			ATRACE(dbri_intr, 'LINT', intr.word);
			cmn_err(CE_WARN, "dbri: Lost interrupts");
			break;

		case DBRI_INT_UNDR: /* DMA underrun */
			ATRACE(dbri_intr, 'UNDR', intr.word);
			cmn_err(CE_WARN, "dbri_intr: DMA underrun");
			break;

		default:
			ATRACE(dbri_intr, '?!?!', intr.word);
			cmn_err(CE_WARN, "dbri: No interrupt code given");
			break;
		} /* switch on interrupt type */

		/*
		 * Advance intqp to point to the next available
		 * interrupt status word
		 */
		if (unitp->intq.off == (DBRI_MAX_QWORDS - 1)) {
			unitp->intq.curqp = (dbri_intq_t *)DBRI_IOPBKADDR(unitp,
			    unitp->intq.curqp->nextq);
			unitp->intq.off = 0;
		} else {
			unitp->intq.off++;
		}

		/*
		 * get the next interrupt status word
		 */
		(void) DBRI_SYNC_CURINTQ(unitp);
		intr = unitp->intq.curqp->intr[unitp->intq.off];
	} /* while there is another interrupt word */

done:
	UNLOCK_UNIT(unitp);
	return (serviced);
} /* dbri_intr */

/*
 * dbri_txeol - handle EOL interrupts for a transmit pipe
 */
static void
dbri_txeol(dbri_stream_t *ds)
{
	dbri_unit_t *unitp;

	ASSERT_ASLOCKED(DsToAs(ds));
	ATRACE(dbri_txeol, 'TEOL', PIPENO(ds->pipep));

	unitp = DsToUnitp(ds);
	if (unitp == NULL)
		return;

	/*
	 * If we get an unintended EOL, then this is an underflow
	 */
	if ((ds->pipep->mode == DBRI_MODE_TRANSPARENT) &&
	    (DsToAs(ds)->info.active))
		ds->audio_uflow = B_TRUE;

	DsToAs(ds)->info.active = B_FALSE;

} /* dbri_txeol */


/*
 * dbri_rxeol - handle EOL interrupts for a receive pipe
 */
static void
dbri_rxeol(dbri_stream_t *ds)
{
	dbri_unit_t *unitp;
	dbri_cmd_t *dcp;
	int n;
	int neof = 0;
	int ncom = 0;
	int done = 0;
	int total = 0;

	ASSERT_ASLOCKED(DsToAs(ds));
	ATRACE(dbri_rxeol, 'REOL', PIPENO(ds->pipep));

	unitp = DsToUnitp(ds);
	if (unitp == NULL)
		return;

	AsToDs(ds->as.control_as)->d_stats.recv_eol++;

	if (ds->cmdptr == NULL) {
		/*
		 * There are no receive buffers on the list.  Call
		 * process_record to try to queue some.
		 */
		ATRACE(dbri_rxeol, 'None', ds);
		if ((ds->pipep->mode == DBRI_MODE_TRANSPARENT) &&
		    (DsToAs(ds)->info.active))
			ds->audio_uflow = B_TRUE;

		DsToAs(ds)->info.active = B_FALSE;

		audio_process_input(&ds->as);
		return;
	}

	DsToAs(ds)->info.active = B_FALSE;

	/*
	 * Scan the receive buffer list for complete frames. If there are
	 * some complete frames, then normal processing will clear the
	 * EOL condition. If the list is full of completed fragments, but
	 * no completed frames, and the freelist is empty, then special
	 * action needs to be taken.
	 */
	for (dcp = ds->cmdptr, n = 0; dcp != NULL; dcp = dcp->nextio, ++n) {
		(void) DBRI_SYNC_MD(unitp, dcp->md, DDI_DMA_SYNC_FORCPU);

		if (dcp->rxmd.com)
			ncom++;
		if (dcp->rxmd.eof)
			neof++;
		if (dcp->cmd.done)
			done++;
	}

#if 0 /* Debugging  */
	cmn_err(CE_WARN, "dbri: REOL; %d frags, %d com, %d eof, %d done", n,
	    ncom, neof, done);
#endif

	if ((ncom == n) && (neof == 0) && (ds->as.cmdlist.free == NULL)) {
		/*
		 * If there is no hope of audio_process_input solving
		 * this EOL problem, then mark the entire list for
		 * discarding.
		 */

		ATRACE(dbri_rxeol, 'Bgus', PIPENO(ds->pipep));

		/*
		 * The receive list has been completely filled with a "huge"
		 * packet (huge means larger that the list.)
		 */
		for (dcp = ds->cmdptr, n = 0; dcp != NULL;
		    dcp = dcp->nextio, n++) {
			dcp->cmd.done = B_TRUE;
			dcp->cmd.skip = B_TRUE;
			total += dcp->rxmd.cnt;
			if (dcp->buf_dma_handle) {
				(void) ddi_dma_free(dcp->buf_dma_handle);
				dcp->buf_dma_handle = 0;
			}
		}
		ds->d_stats.recv_error_octets += total;

		/*
		 * There are cases when after we do the above the chip
		 * still doesn't recover and no more packets are received
		 * on the channel.  This appears to take care of that.
		 */
		dbri_stop(DsToAs(ds));
		audio_flush_cmdlist(DsToAs(ds));
		/* audio_process_input takes care of the rest */

	} else if ((ncom == 0) && (neof == 0) && (n > 0)) {
		/*
		 * It appears we got an EOL with a completely full list
		 * which can happen if we are receiving and we enter
		 * the debugger.  All of the BRDY interrupts are processed
		 * but they don't know about the EOL so they don't start
		 * any I/O.  When we get around to looking at the EOL
		 * interrupt and call audio_process_input, it doesn't
		 * have any buffers to post so it can't start the I/O.
		 * Start it here then...
		 */
		dbri_start(DsToAs(ds));
		return;
	}

	audio_process_input(DsToAs(ds));
}


/*
 * dbri_rxintr - process a single packet or a single audio buffer.
 */
static void
dbri_rxintr(dbri_unit_t *unitp, dbri_intrq_ent_t intr)
{
	uint_t status;
	dbri_pipe_t *pipep;
	dbri_cmd_t *dcp;
	struct {
		dbri_cmd_t *head;
		dbri_cmd_t *tail;
	} packet;
	aud_stream_t *as;
	dbri_stream_t *ds;
	boolean_t error = B_FALSE;
	uint_t count;
	int total = 0;
	boolean_t hdlc;
#if defined(DBRI_SWFCS)
	extern boolean_t Dbri_fcscheck;
#endif

	ASSERT_UNITLOCKED(unitp);

	pipep = &unitp->ptab[intr.f.channel];

	as = DsToAs(pipep->ds);
	if (as == NULL) {
		ATRACE(dbri_rxintr, '-na-', intr.word);
		return;
	}

	/*
	 * "Processed" commands never show up on the DBRI's IO list.
	 * Completed packets are removed from the DBRI IO list by this
	 * routine.
	 */

	ds = AsToDs(as);

	/*
	 * Just go through list to end to check for bad status
	 */
	dcp = ds->cmdptr;	/* current head of DBRI cmdlist */

	if (dcp != NULL)
		(void) DBRI_SYNC_MD(unitp, dcp->md, DDI_DMA_SYNC_FORCPU);

	if (dcp == NULL || !dcp->rxmd.com) {
		/*
		 * NB - Something's wrong since we got a BRDY but the first
		 * packet is not done.
		 */
		ATRACE(dbri_rxintr, 'SPUR', intr.f.channel);
		return;
	}

	packet.head = dcp;	/* Any new packet will start here */
	packet.tail = NULL;

#if defined(AUDIOTRACE)
	if (dcp == NULL) {
		ATRACE(dbri_rxintr, 'emty', 0);
	}
#endif

	/*
	 * Search for a valid EOF
	 */
	while (dcp != NULL) {
		(void) DBRI_SYNC_MD(unitp, dcp->md, DDI_DMA_SYNC_FORCPU);
		if (dcp->rxmd.com && dcp->rxmd.eof) {
			ATRACE(dbri_rxintr, ' eof', dcp);
			packet.tail = dcp;
			break;
		}
		ATRACE(dbri_rxintr, '!eof', dcp);
		dcp = dcp->nextio;
	}

	if (packet.tail == NULL) {
		ATRACE(dbri_rxintr, 'frag', dcp);
		return;
	}

	/*
	 * There is at least one complete packet on the receive chain.
	 * Process one packet only.
	 */

	/*
	 * Start and end of packet have been found. Mark fragments as
	 * done and set lastfragment pointer.
	 */
	for (dcp = packet.head; dcp != NULL; dcp = dcp->nextio) {
		dcp->cmd.done = B_TRUE;
		dcp->cmd.lastfragment = &packet.tail->cmd;

		/*
		 * Free DMA resources.  Not that this also implicitly
		 * calls ddi_dma_sync for us.
		 */
		if (dcp->buf_dma_handle) {
			(void) ddi_dma_free(dcp->buf_dma_handle);
			dcp->buf_dma_handle = 0;
		}

		if (dcp == packet.tail)
			break;
	}

	DTRACE(dbri_rxintr, 'RMD0', packet.tail->words[0]);
	DTRACE(dbri_rxintr, 'RMD1', packet.tail->words[1]);
	DTRACE(dbri_rxintr, 'RMD2', packet.tail->words[2]);
	DTRACE(dbri_rxintr, 'RMD3', packet.tail->words[3]);

	/*
	 * Process receive errors
	 */
	status = packet.tail->rxmd.status; /* check status */

#if defined(AUDIOTRACE)
	if (status != 0) {
		ATRACE(dbri_rxintr, 'stat', status);
	}
#endif

	hdlc = ((pipep->sdp & DBRI_SDP_MODEMASK) != DBRI_SDP_TRANSPARENT) ?
	    B_TRUE : B_FALSE;

	if (status & (DBRI_RMD_CRC | DBRI_RMD_BBC | DBRI_RMD_ABT |
	    DBRI_RMD_OVRN)) {

		if (status & DBRI_RMD_OVRN) {
			ds->audio_uflow = B_TRUE;
			ds->d_stats.dma_underflow++;
		}

		if (status & DBRI_RMD_CRC) {
			error = B_TRUE;
			ds->d_stats.crc_error++;
		}

		if (status & DBRI_RMD_BBC) {
			error = B_TRUE;
			ds->d_stats.badbyte_error++;
		}

		if (status & DBRI_RMD_ABT) {
			error = B_TRUE;
			ds->d_stats.abort_error++;
		}
	}

#if defined(DBRI_SWFCS)
	if (Dbri_fcscheck && !error && hdlc) {
		error = dbri_checkfcs(unitp, packet.head);

		if (error)
			cmn_err(CE_WARN, "dbri: packet with bad SW FCS");
	}
#endif

	/*
	 * Second pass thru data to adjust data pointers,
	 * and adjust sample count.
	 */
	total = 0;

	for (dcp = packet.head; dcp != NULL; dcp = dcp->nextio) {
		count = dcp->rxmd.cnt;

		/*
		 * When DBRI is under load it can mark zero-length
		 * buffers.  This can cause undesirable behaviour at the
		 * STREAM head, so don't pass them up.  Marking them as
		 * 'skip' causes a signal, but this should be fine since
		 * there is probably already one pending for the overflow
		 * condition.  Zero length problems are a problem for
		 * HDLC as well since there is at least supposed to be an
		 * FCS there.
		 */
		if ((count == 0) || (error))
			dcp->cmd.skip = B_TRUE;

		/*
		 * For HDLC mode, remove the 2-byte CRC at the end of the
		 * frame.
		 *
		 * XXX - Will CRC ever be given to device users?
		 */
		if (hdlc) {
			/*
			 * If HDLC mode, remove CRC from end of packet.
			 * If the last fragment contains one byte, it is
			 * from the CRC
			 */
			if (dcp->rxmd.eof)
				count -= (dcp->rxmd.cnt == 1) ? 1 : 2;
			else if (dcp->nextio->rxmd.eof &&
			    dcp->nextio->rxmd.cnt == 1)
				count -= 1;
		}

		dcp->cmd.data += count;
		total += count;

		if (dcp == packet.tail)
			break;
	}
	ASSERT(dcp != NULL);	/* above loop stays on last cmd */

	/*
	 * Update dbri stream structure command pointers.  Delete the
	 * complete packet from the DBRI IO list.
	 *
	 * The "processed" fragment doesn't show up on the DBRI IO list
	 * even though the DBRI still partially "owns" it.  This is
	 * correct behavior.
	 */
	ds->cmdptr = packet.tail->nextio;
	if (packet.tail->nextio == NULL)
		ds->cmdlast = NULL;

	/*
	 * Updata audio samples counter
	 */
	if (as->info.channels != 0 && (as->info.precision/8) != 0)
		total /= as->info.channels * (as->info.precision/8);

	if (error) {
		ATRACE(dbri_rxintr, 'ERR ', ds->cmdptr);
		ds->d_stats.recv_error_octets += total;
		ds->iostats.errors++;
	} else {
		ATRACE(dbri_rxintr, 'totl', total);
		ds->iostats.octets += total;
		ds->iostats.packets++;
	}

	if (!error) {
		if (hdlc) {
			ds->samples += 1; /* counts packets in HDLC */
		} else {
			ds->samples += total;
		}
		uniqtime(&ds->last_smpl_update);
	}

	audio_process_input(DsToAs(pipep->ds));
} /* dbri_rxintr */


/*
 * dbri_txintr - process a single packet or a single audio buffer
 */
static void
dbri_txintr(dbri_unit_t *unitp, dbri_intrq_ent_t intr)
{
	dbri_cmd_t *dcp;
	uint_t status;
	dbri_pipe_t *pipep;
	dbri_stream_t *ds;
	int octets = 0;
	dbri_cmd_t *err_dcp = NULL;
	boolean_t hdlc;
	int total;

	ASSERT_UNITLOCKED(unitp);

	pipep = &unitp->ptab[intr.f.channel];

	/* Check for a closed stream */
	if (pipep->ds == NULL) {
		ATRACE(dbri_txintr, 'BAD!', intr.word);
		return;
	}

	ds = pipep->ds;		/* Get dbri stream pointer */
	ASSERT(ds != NULL);

	dcp = ds->cmdptr;	/* head of DBRI cmdlist */

	/*
	 * Check if first command already processed.
	 */
	while ((dcp != NULL) && dcp->cmd.processed) {
		/*
		 * NB: Device routines follow the nextio list, the aud_cmd
		 * "next" pointer is irrelevent here.
		 */
		dcp = dcp->nextio;
	}

	if (dcp == NULL)
		return;

	if ((pipep->sdp & DBRI_SDP_MODEMASK) != DBRI_SDP_TRANSPARENT)
		hdlc = B_TRUE;
	else
		hdlc = B_FALSE;

	octets = 0;
	total = 0;
	while (dcp != NULL) {
		int samples;

		(void) DBRI_SYNC_MD(unitp, dcp->md, DDI_DMA_SYNC_FORCPU);
		status = dcp->txmd.status;
		if (!(status & (DBRI_TMD_TBC | DBRI_TMD_ABT)))
			break;

		dcp->cmd.done = B_TRUE;

		if ((status & DBRI_TMD_UNR) && (!hdlc)) {
			if (!ds->last_flow_error) {
				ATRACE(dbri_txintr, 'UNDR', ds);
				ds->audio_uflow = B_TRUE;
			}
		}

		switch (unitp->version) {
		case 'd':
		case 'e':
			ds->last_flow_error = (status & DBRI_TMD_UNR) ?
			    B_TRUE : B_FALSE;
			break;

		default:
			ds->last_flow_error = B_FALSE;
		}

		/*
		 * XXX1 - This conditional looks like it checks for
		 * DBRI'a' bugs where transparent buffers had abt set.
		 */
		if (((status & DBRI_TMD_ABT) &&
		    ((pipep->sdp & DBRI_SDP_MODEMASK) !=
		    DBRI_SDP_TRANSPARENT)) ||
		    (status & DBRI_TMD_UNR)) {
			if (err_dcp == NULL) /* Grab first error status */
				err_dcp = dcp;
			ATRACE(dbri_txintr, 'XXX1', status);
		}

		if (dcp->buf_dma_handle) {
			(void) ddi_dma_free(dcp->buf_dma_handle);
			dcp->buf_dma_handle = 0;
		}

		samples = dcp->txmd.cnt;
		octets += samples;

		if (!hdlc) {
			if ((DsToAs(pipep->ds)->info.channels != 0) &&
			    (DsToAs(pipep->ds)->info.precision/8 != 0)) {
				samples /= DsToAs(pipep->ds)->info.channels *
				    (DsToAs(pipep->ds)->info.precision/8);
			}
		} else {
			samples = 1; /* packets */
		}
		total += samples;

		if (dcp->txmd.eof)
			break;

		dcp = dcp->nextio;
	}

	if (dcp == NULL)
		return;

	/*
	 * If there was an error, retrieve the md where the error occurred
	 */
	if (err_dcp != NULL) {
		dcp = err_dcp;
		status = dcp->txmd.status;

		if (status & DBRI_TMD_UNR) {
			ds->d_stats.dma_underflow++;
			ds->samples += total;
		}
		if (status & DBRI_TMD_ABT)
			ds->d_stats.abort_error++;
		ds->iostats.errors++;
	} else {
		ds->iostats.packets++;
		ds->iostats.octets += octets;
		ds->samples += total; /* adjust sample count */
	}
	uniqtime(&ds->last_smpl_update);

	/*
	 * Update dbri stream structure command pointers
	 */
	ds->cmdptr = dcp;
	if (dcp == NULL)
		ds->cmdlast = NULL;

	/* EOF - release transmit buffers */
	audio_process_output(DsToAs(pipep->ds));
} /* dbri_txintr */


/*
 * dbri_sbri_intr - process a SBRI interrupt DBRI always has the say as
 * to what the current state is. However, old state must be known in
 * order to properly implement Primitives.
 */
static void
dbri_sbri_intr(dbri_unit_t *unitp, dbri_intrq_ent_t intr)
{
	aud_stream_t *as;
	dbri_pipe_t *pipep;
	dbri_bri_status_t *bs;

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_sbri_intr, 'unit', unitp);

	/*
	 * Detectible Bit Error
	 */
	if (intr.code_sbri.berr) {
		ATRACE(dbri_sbri_intr, 'BERR', intr.word);
		if (intr.f.channel == DBRI_INT_TE_CHAN)
			bs = DBRI_TE_STATUS_P(unitp);
		else if (intr.f.channel == DBRI_INT_NT_CHAN)
			bs = DBRI_NT_STATUS_P(unitp);
		else
			bs = NULL;

		if (bs != NULL)
			bs->primitives.berr++;
		/* XXX - any other action? */
	}

	/*
	 * Frame Sync Error
	 */
	if (intr.code_sbri.ferr) {
		ATRACE(dbri_sbri_intr, 'FERR', intr.word);
		if (intr.f.channel == DBRI_INT_TE_CHAN)
			bs = DBRI_TE_STATUS_P(unitp);
		else if (intr.f.channel == DBRI_INT_NT_CHAN)
			bs = DBRI_NT_STATUS_P(unitp);
		else
			bs = NULL;

		if (bs != NULL)
			bs->primitives.ferr++;
		/* XXX - any other action? */
	}

#if defined(AUDIOTRACE)
	if (Dbri_debug_sbri)
		pr_interrupt(&intr);
#endif

	if (intr.f.channel == DBRI_INT_TE_CHAN) {
		struct dbri_code_sbri old_te_sbri;

		bs = DBRI_TE_STATUS_P(unitp);

		ATRACE(dbri_sbri_intr, 'TECH', 0);

		old_te_sbri = bs->sbri;
		bs->sbri = intr.code_sbri;

		pipep = &unitp->ptab[DBRI_PIPE_TE_D_IN];

		/* Don't do anything if pipe not connected */
		if (!ISPIPEINUSE(pipep))
			return;

		ASSERT(pipep->ds != NULL);

		as = DsToAs(pipep->ds)->output_as; /* get D-channel as */

		/*
		 * If new state is not F3, then remove PH-ACTIVATEreq
		 * since it's only use, for the TE, is to get out of F3.
		 */
		if (bs->sbri.tss != DBRI_TEINFO0_F3)
			dbri_hold_f3(unitp);

		/*
		 * XXX - DBRI hangs if synchronization is lost.  Flush
		 * queues and stop I/O if this is a transition out of F7
		 * and this hack is enabled.
		 */
		if ((bs->i_var.norestart == 0) &&
		    (old_te_sbri.tss == DBRI_TEINFO3_F7) &&
		    (bs->sbri.tss != DBRI_TEINFO3_F7)) {
			ATRACE(dbri_sbri_intr, 'Thck', intr.word);
			dbri_bri_down(unitp, ISDN_TYPE_TE);
		}

		if (old_te_sbri.tss != bs->sbri.tss) {
			switch (bs->sbri.tss) {
			case DBRI_TEINFO0_F1:
				dbri_primitive_mph_ii_d(as);
#if 0
				AsToDs(as)->i_info.activation = ISDN_UNPLUGGED;
				/*
				 * F1 and F2 are not distinguishable by
				 * the unitp
				 *
				 * f3_f1	mph-ii(d)
				 * f4_f1	mph-ii(d), mph-di, ph-di
				 * f5_f1	mph-ii(d), mph-di, ph-di
				 * f6_f1	mph-ii(d), mph-di, ph-di
				 * f7_f1	mph-ii(d), mph-di, ph-di
				 * f8_f1	mph-ii(d), mph-di, ph-di
				 */
				dbri_primitive_mph_ii_d(as);
				if (old_te_sbri.tss != DBRI_TEINFO0_F3) {
					dbri_bri_down(unitp, ISDN_TYPE_TE);
					dbri_primitive_mph_di(as);
					dbri_primitive_ph_di(as);
				}
				break;
#else /* 0 */
				/*
				 * Given the implementation of this
				 * unitp and the implementation of DBRI,
				 * transitions to F1 reported by DBRI are
				 * to be interpreted as transitions to
				 * F3.
				 */
				bs->sbri.tss = DBRI_TEINFO0_F3;
				/*FALLTHROUGH*/
#endif /* !0 */

			case DBRI_TEINFO0_F3:
				bs->i_info.activation = ISDN_DEACTIVATED;
				/*
				 * f1_f3	mph-ii(c)
				 * f4_f3	mph-di, ph-di	// T3 expiry
				 * f5_f3	mph-di, ph-di	// T3 expiry
				 * f6_f3	mph-di, ph-di	// T3 expiry
				 * 			or receive I0
				 * f7_f3	mph-di, ph-di	// receive I0
				 * f8_f3	mph-di, ph-di, mph-ei2
				 */
				if (old_te_sbri.tss != DBRI_TEINFO0_F1) {
					dbri_bri_down(unitp, ISDN_TYPE_TE);
					dbri_primitive_mph_di(as);
					dbri_primitive_ph_di(as);
					if (old_te_sbri.tss ==
					    DBRI_TEINFO0_F8) {
						dbri_primitive_mph_ei2(as);
					}
				} else {
					/*EMPTY*/
					/* really from F2 */
#if 0
					/*
					 * DBRI "falsely" reports F1 when
					 * T3 expires and then unitp
					 * toggles the T bit in reg0.
					 *
					 * Since dbri_primitive_mph_ii_c
					 * should only/always happen once
					 * immediately after open, this
					 * line has been moved into the
					 * TE initialization code in
					 * dbri_setup_ntte().
					 */
					dbri_primitive_mph_ii_c(as,
					    &bs->primitives);
#endif	/* 0 */
				}
				break;

			case DBRI_TEINFO1_F4:
				bs->i_info.activation = ISDN_ACTIVATE_REQ;
				break;

			case DBRI_TEINFO0_F5:
				bs->i_info.activation = ISDN_ACTIVATE_REQ;
				break;

			case DBRI_TEINFO3_F6:
				/* AsToDs(as)->i_info.activation=no change; */
#if 0
				if (unitp->tetimer_id) {
					(void) untimeout(unitp->tetimer_id);
					unitp->tetimer_id = 0;
				}
#endif

				/*
				 * f1_f6	mph-ii(c)
				 * f7_f6	mph-ei1	// report error
				 * f8_f6	mph-ei2	// report recovery
				 * 		from error
				 * f3_f6	no-action
				 * f5_f6	no-action
				 */
				switch (old_te_sbri.tss) {
				case DBRI_TEINFO0_F1:
					/* really from F2 */
					dbri_primitive_mph_ii_c(as);
					break;

				case DBRI_TEINFO3_F7:
					dbri_primitive_mph_ei1(as);
					break;

				case DBRI_TEINFO0_F8:
					dbri_primitive_mph_ei2(as);
					break;
				}
				break;

			case DBRI_TEINFO3_F7:
				bs->i_info.activation = ISDN_ACTIVATED;

				if (unitp->tetimer_id) {
					(void) untimeout(unitp->tetimer_id);
					unitp->tetimer_id = 0;
				}

				/*
				 * f1_f7	mph-ii(c), ph-ai, mph-ai
				 * f3_f7	ph-ai, mph-ai
				 * f5_f7	ph-ai, mph-ai
				 * f6_f7	ph-ai, mph-ai, mph-ei2
				 * f8_f7	ph-ai, mph-ai, mph-ei2
				 */

				if (old_te_sbri.tss == DBRI_TEINFO0_F1) {
					/* really from F2 */
					dbri_primitive_mph_ii_c(as);
				}
				dbri_bri_up(unitp, ISDN_TYPE_TE);
				dbri_primitive_ph_ai(as);
				dbri_primitive_mph_ai(as);

				if ((old_te_sbri.tss == DBRI_TEINFO3_F6) ||
				    (old_te_sbri.tss == DBRI_TEINFO0_F8)) {
					/* recovered from error */
					dbri_primitive_mph_ei2(as);
				}
				break;

			case DBRI_TEINFO0_F8:
				/*
				 * f7_f8	mph-e11
				 * f6_f8	mph-ei1
				 */

				/* report error */
				dbri_primitive_mph_ei1(as);

				break;
			} /* switch on current state */
		}
	} else if (intr.f.channel == DBRI_INT_NT_CHAN) {
		struct dbri_code_sbri old_nt_sbri;

		ATRACE(dbri_sbri_intr, 'NTCH', 0);

		bs = DBRI_NT_STATUS_P(unitp);

		old_nt_sbri = bs->sbri;

		/*
		 * DBRI doesn't implement G4. So be careful about the
		 * pseudo G4 state.
		 */
		if (bs->sbri.tss == DBRI_NTINFO0_G4) {
#if 0 /* not used */
			if (intr.code_sbri.tss == DBRI_NTINFO_G1) {
				/*
				 * but but but... the interface is
				 * disabled!  We can't receive I1!!!.
				 *
				 * G4:I0->G1 assumes t102=0 and NT can
				 * unambiguously recognize I1.  We state
				 * that DBRI cannot unambiguously
				 * recognize I1 therefore t102 must be
				 * non-zero.  The CCITT state tables are
				 * a bit unclear.
				 */
			}
#endif
			/*
			 * Since DBRI "cannot unambiguously recognize I1"
			 * we do not look for I0 in G4.  All other states
			 * are ignored, so...  we're done.
			 *
			 * Only t2! or PH-AR will get DBRI out of G4.
			 */
			return;
		} else {
			/*
			 * We're not in G4, so DBRI really knows what's
			 * happening.
			 */
			bs->sbri = intr.code_sbri;
		}

		pipep = &unitp->ptab[DBRI_PIPE_NT_D_IN];

		/*
		 * Do not do anything if pipe not connected
		 */
		if (!ISPIPEINUSE(pipep)) {
			ATRACE(dbri_sbri_intr, 'xcon', intr.word);
			return;
		}

		ASSERT(pipep->ds != NULL);

		as = DsToAs(pipep->ds)->output_as; /* get D-channel as */

		/*
		 * According to the CCITT spec, an NT interface will
		 * only deactivate under explicit request from its upper
		 * layers. Receipt of I0 will not cause an NT to cease
		 * activation request. This behavior is different from
		 * the TE side's behavior where an I0 signal received from the
		 * NT will force the TE to deactivate and not seek
		 * reactivation.
		 */

		/* If no state change, then no actions taken */
		if (old_nt_sbri.tss != bs->sbri.tss) {
			/*
			 * XXX - DBRI hangs if synchronization is lost.
			 * Flush queues and stop I/O if this is a
			 * transition G3 and this hack is enabled.
			 */
			if ((bs->i_var.norestart == 0) &&
			    (old_nt_sbri.tss == DBRI_NTINFO4_G3) &&
			    (bs->sbri.tss != DBRI_NTINFO4_G3)) {
				ATRACE(dbri_sbri_intr, 'Nhck', intr.word);
				dbri_bri_down(unitp, ISDN_TYPE_NT);
			}

			switch (bs->sbri.tss) {
			case DBRI_NTINFO0_G1:
				/*
				 * Transitions into G1 must be forced by
				 * the driver.  Any report by DBRI while in
				 * G1 is not a real state transition.
				 */
				break;

			case DBRI_NTINFO2_G2:
				/*
				 * g1->g2	received I0
				 * g2->g2	noop
				 * g3->g2	received I0 or lost framing
				 * g4->g2	can't happen
				 */
				bs->i_info.activation = ISDN_ACTIVATE_REQ;
				if (old_nt_sbri.tss == DBRI_NTINFO0_G1) {
					/*
					 * A TE can request activation, but
					 * only the NT can request
					 * deactivation.
					 */
					dbri_exit_g1(unitp);

					if (unitp->nttimer_t101_id) {
						(void) untimeout(
						    unitp->nttimer_t101_id);
					}
					unitp->nttimer_t101_id = timeout(
					    dbri_nttimer_t101, unitp,
					    TICKS(bs->i_var.t101));
					ATRACE(dbri_sbri_intr, 't101',
					    bs->i_var.t101);
				} else if (old_nt_sbri.tss ==
				    DBRI_NTINFO4_G3) {
					dbri_primitive_mph_di(as);
					dbri_primitive_mph_ei1(as);
				}

				break;

			case DBRI_NTINFO4_G3:
				/*
				 * g1->g3	can't happen
				 * g2->g3	received I3
				 * g3->g3	can't happen
				 * g4->g3	can't happen
				 */
				bs->i_info.activation = ISDN_ACTIVATED;
				dbri_bri_up(unitp, ISDN_TYPE_NT);
				if (old_nt_sbri.tss == DBRI_NTINFO2_G2) {
					/* cancel T1 */
					if (unitp->nttimer_t101_id) {
						(void) untimeout(
						    unitp->nttimer_t101_id);
					}
					unitp->nttimer_t101_id = 0;
					/*
					 * Note 4 from the CCITT spec: As
					 * an implementation option, to
					 * avoid premature transmission
					 * of information (i.e.  INFO 4),
					 * layer 1 may not initiate the
					 * transmission of INFO 4 or send
					 * the primitives PH-ACTIVATE
					 * INDICATION and MPH-ACTIVATE
					 * INDICATION (to layer 2 and
					 * management, respectively)
					 * until a period of 100 ms has
					 * elapsed since the receipt of
					 * INFO 3.  Such a delay time
					 * should be implemented in the
					 * ET, if required.
					 *
					 * This driver does not implement
					 * the above option.
					 */
					dbri_primitive_ph_ai(as);
					dbri_primitive_mph_ai(as);
				}
				break;

#if 0
			case DBRI_NTINFO0_G4:
				bs->i_info.activation = ISDN_DEACTIVATED;
				/*
				 * NB: DBRI does not generate
				 * transitions to this state. Transition
				 * to G4 is done only through expiry of
				 * T1 or receipt of MPH-DEACTIVATEreq.
				 */
				break;
#endif
			}
		}
	} else {
#if defined(AUDIOTRACE)
		ATRACE(dbri_sbri_intr, 'bgus', intr.f.channel);
#else
		/*EMPTY*/
#endif
	}
} /* dbri_sbri_int */


#if defined(DBRI_SWFCS)
static unsigned short fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48,	0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78,
};


/*
 * dbri_checkfcs - paranoid re-checking of the FCS in a HDLC packet
 * to inform us of bugs in DBRI.  Returns B_TRUE if there is an error.
 * This code assumes the message descriptors are sync'd with memory
 * already.
 */
static boolean_t
dbri_checkfcs(dbri_unit_t *unitp, dbri_cmd_t *dcp)
{
	register int		databytes;
	register unsigned short	fcs = 0xffff; /* Initial FCS value */
	register unsigned char	*p;
	unsigned short		dbri_fcs;
	boolean_t		error = B_FALSE;

	p = dcp->cmd.data;

	/*
	 * There are five types of fragment:
	 * 1) Data only (not eof)
	 * 2) Data and complete FCS (databytes >= 2)
	 * 3) Data and 1 byte of FCS (next pkt is eof and has 1 byte)
	 * 4) One byte of FCS only (databytes == 1)
	 * 5) FCS only (databytes >= 2)
	 */
	databytes = dcp->rxmd.cnt;
	if (dcp->rxmd.eof) {
		if (databytes == 1) {
			databytes = 0;
		} else if (databytes >= 2) {
			databytes -= 2;
		} else {
			dbri_panic(unitp, "dbri: bogus receive packet type");
			return (B_TRUE);
		}
	} else if (dcp->nextio->rxmd.eof && (dcp->nextio->rxmd.cnt == 1)) {
		--databytes;
	}

	while (dcp != NULL && dcp->rxmd.com) {
		while (databytes-- > 0)
			fcs = (fcs >> 8) ^ fcstab[(fcs ^ *p++) & 0xff];

		if (dcp->rxmd.eof)
			break;

		/*
		 * Before advancing to the next fragment, check for an FCS byte
		 * in current fragment.
		 */
		if (!dcp->rxmd.eof &&
		    dcp->nextio->rxmd.eof &&
		    dcp->nextio->rxmd.cnt == 1) {
			dbri_fcs = *p++ << 8;
		}

		if (dcp->nextio == NULL)
			break;

		dcp = dcp->nextio;
		p = dcp->cmd.data;
		databytes = dcp->rxmd.cnt;

		if (dcp->rxmd.eof) {
			if (databytes == 1)
				databytes = 0;
			else if (databytes >= 2)
				databytes -= 2;
			else
				cmn_err(CE_PANIC, "dbri: bogus rx packet type");
		} else if (dcp->nextio->rxmd.eof &&
		    (dcp->nextio->rxmd.cnt == 1)) {
			--databytes;
		}
	}
	/* p is left pointing at the first FCS octet */

	/*
	 * Calculate SW FCS
	 */
	fcs ^= 0xffff;
	fcs = ((fcs & 0xff) << 8) | ((fcs >> 8) & 0xff);

	/*
	 * Retrieve HW FCS
	 */
	if (dcp->rxmd.cnt > 1) {
		dbri_fcs = *p++ << 8;
		dbri_fcs |= *p++;
	} else {
		dbri_fcs = *p++ << 8;
		if (dcp->rxmd.eof || dcp->nextio == NULL ||
		    !dcp->nextio->rxmd.eof) {
			dbri_panic(unitp, "dbri: error while getting fcs");
			return (B_TRUE);
		}
		p = dcp->nextio->cmd.data;
		dbri_fcs |= *p++;
	}

	if (fcs != dbri_fcs) {
		error = B_TRUE;	/* Discard this packet */
		cmn_err(CE_WARN, "dbri: software/hardware fcs mismatch");
	}

	return (error);
}
#endif /* DBRI_SWFCS */

#if defined(AUDIOTRACE)
static char *
pr_code(int code)
{
	static char	*codes[16] = {
		"****", "BRDY", "MINT", "IBEG",
		"IEND", "EOL", "CMDI", "****",
		"XCMP", "SBRI", "FXDT", "CHIL",
		"DBYT", "RBYT", "LINT", "UNDR",
	};

	return (codes[code]);
}


static char *
pr_channel(int ch)
{
	static char	buf[100];

	switch (ch) {
	case DBRI_INT_TE_CHAN:
		return ("TE_status");
	case DBRI_INT_NT_CHAN:
		return ("NT_status:");
	case DBRI_INT_CHI_CHAN:
		return ("CHI_status:");
	case DBRI_INT_REPORT_CHAN:
		return ("Report_Command_channel_intr_status");
	case DBRI_INT_OTHER_CHAN:
		return ("Other_status");
	default:
		(void) sprintf(buf, "Channel_%d", ch);
		return (buf);
	}
}


static void
pr_interrupt(dbri_intrq_ent_t *intr)
{
	static char	*tss[8] = {
		"G1/F1",	/* 0 */
		"**/**",	/* 1 */
		"**/F8",	/* 2 */
		"**/F3",	/* 3 */
		"**/F4",	/* 4 */
		"**/F5",	/* 5 */
		"G2/F6",	/* 6 */
		"G3/F7",	/* 7 */
	};

	(void) printf("I=%d channel=%s code=%s, field=0x%x",
	    intr->f.ibits, pr_channel((int)intr->f.channel),
	    pr_code((int)intr->f.code), (unsigned int)intr->f.field);

	if (intr->f.code != DBRI_INT_SBRI) {
		(void) printf("\n");
	} else {
		(void) printf(" %x %s\n", intr->f.field,
		    tss[intr->code_sbri.tss]);
	}
}
#endif /* AUDIOTRACE */
