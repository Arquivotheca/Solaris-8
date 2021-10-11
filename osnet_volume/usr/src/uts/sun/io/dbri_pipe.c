/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dbri_pipe.c	1.15	97/03/14 SMI"

/*
 * Sun DBRI pipe/channel connection routines
 */

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/ioccom.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sysmacros.h>	/* for MIN */

#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/isdnio.h>
#include <sys/dbriio.h>
#include <sys/dbrireg.h>
#include <sys/dbrivar.h>
#include <sys/audiodebug.h>


/*
 * REMIND: exodus 1992/09/26 - context required in dbri_basepipe_init
 *
 * Something will have to be done about this sometime.  Right now
 * an ioctl that creates a connection will hang the STREAMS service
 * thread--- no other service routines in the kernel will be able
 * to execute!
 */
/*
 * REMIND: exodus 1992/10/10 - audio chan check not good enough?
 *
 * dbri_is_dbri_audio_chan only checks for AUDIO_IN and AUDIO_OUT.  Other
 * pipes can be connected to the audio device.  The check should probably
 * be something like "basepipe(pipep) == CHI basepipe.
 */
/*
 * REMIND: exodus YYYY/XX/XX - find_oldnext should deal with logical cycles
 *
 * The callers of find_oldnext routine should pass the "logical" cycle
 * and this routine should patch the value to the correct "physical"
 * value once its location on this ring has been found.
 */


/*
 * Local function prototypes
 */
static int dbri_tsd_overlap(uint_t, uint_t, uint_t, uint_t);


/*
 * Local macros
 */
#define	XPIPENO(z) (((z->refcnt & 0xffff) << 16)|PIPENO(z))


/*
 * dbri_basepipe_init - Setup the base pipe for this channel
 */
boolean_t
dbri_basepipe_init(dbri_unit_t	*unitp, dbri_chan_t dchan, int *error)
{
	dbri_pipe_t *bpipep = NULL;
	dbri_pipe_t *pipep = NULL;

	ATRACE(dbri_basepipe_init, 'dchn', dchan);
	*error = 0;

	/*
	 * There are no basepipes for host channels.
	 */
	if (dchan == DBRI_CHAN_HOST)
		return (B_TRUE);

	{
		dbri_chantab_t *ctep;

		ctep = dbri_dchan_to_ctep(dchan);
		if (ctep == NULL || ctep->bpipe == DBRI_BAD_PIPE) {
			*error = EINVAL;
			return (B_FALSE);
		}
		bpipep = &unitp->ptab[ctep->bpipe];
		ATRACE(dbri_basepipe_init, 'bppe', PIPENO(bpipep));
	}


	switch (PIPENO(bpipep)) {
	case DBRI_PIPE_CHI:

		/*
		 * This call is going to wait until the MMCODEC is
		 * configured properly.
		 */
		if (!dbri_mmcodec_connect(unitp, dchan)) {
			*error = ENODEV;
			return (B_FALSE);
		}
		ATRACE(dbri_basepipe_init, ' aud', PIPENO(bpipep));
		/* Audio is now in use */
		(void) pm_busy_component(unitp->dip, 2);
		(void) ddi_dev_is_needed(unitp->dip, 2, 1);
		return (B_TRUE);

	case DBRI_PIPE_TE_D_IN:
	case DBRI_PIPE_TE_D_OUT:
	case DBRI_PIPE_NT_D_OUT:
	case DBRI_PIPE_NT_D_IN:
		if (!ISPIPEINUSE(bpipep)) {
			dbri_chantab_t	*bctep;
			dbri_conn_oneway_t dcr;

			(void) pm_busy_component(unitp->dip, 1);
			(void) ddi_dev_is_needed(unitp->dip, 1, 1);

			bctep = dbri_dchan_to_ctep(
			    dbri_base_p_to_ch(PIPENO(bpipep)));

			/*
			 * Setup base pipe "by hand".
			 */
			if (bctep->dir == DBRI_DIRECTION_IN) {
				dcr.sink.dchan = DBRI_CHAN_NONE;
				dcr.source.dchan = bctep->dchan;
			} else {
				dcr.sink.dchan = bctep->dchan;
				dcr.source.dchan = DBRI_CHAN_NONE;
			}

			dcr.format = *bctep->default_format;

			pipep = dbri_pipe_allocate(unitp, &dcr);
			if (pipep == NULL) {
				ATRACE(dbri_basepipe_init, 'Ibad',
				    PIPENO(bpipep));
				*error = EINVAL;
				return (B_FALSE);
			}
			ASSERT(pipep == bpipep);

			dbri_setup_sdp(unitp, pipep, bctep);
			dbri_setup_dts(unitp, pipep, bctep);

			{
				dbri_chip_cmd_t	cmd;

				cmd.opcode = pipep->sdp;
				cmd.arg[0] = 0;	/* NULL dvma address */
				dbri_command(unitp, cmd);
			}
			{
				dbri_chip_cmd_t	cmd;

				cmd.opcode = DBRI_CMD_PAUSE;
				dbri_command(unitp, cmd);
			}
		} /* if !ISPIPEINUSE(bpipep) */

		/*
		 * This also bumps the reference count
		 */
		if (!dbri_pipe_insert(unitp, bpipep)) {
			dbri_pipe_free(pipep);
			*error = ENXIO;
			return (B_FALSE);
		}

		break;

	default:
		cmn_err(CE_WARN,
		    "dbri: can't initialize unknown base pipe");
		ATRACE(dbri_basepipe_init, 'bgus', PIPENO(bpipep));
		*error = EINVAL;
		return (B_FALSE);
	}

	ATRACE(dbri_basepipe_init, ' :^)', PIPENO(bpipep));
	return (B_TRUE);
} /* dbri_basepipe_init */


/*
 * dbri_basepipe_fini - inverse function to dbri_basepipe_init
 */
boolean_t
dbri_basepipe_fini(dbri_unit_t *unitp, dbri_chan_t dchan)
{
	dbri_pipe_t *bpipep;

	ATRACE(dbri_basepipe_fini, 'dchn', dchan);

	/*
	 * There are no basepipes for host channels.
	 */
	if (dchan == DBRI_CHAN_HOST)
		return (B_TRUE);

	{
		dbri_chantab_t *ctep;

		ctep = dbri_dchan_to_ctep(dchan);
		if (ctep == NULL || ctep->bpipe == DBRI_BAD_PIPE) {
			return (B_FALSE);
		}
		bpipep = &unitp->ptab[ctep->bpipe];
		ATRACE(dbri_basepipe_fini, 'bppe', PIPENO(bpipep));
	}

	switch (PIPENO(bpipep)) {
	case DBRI_PIPE_CHI:
		if (!ISPIPEINUSE(bpipep)) {
			cmn_err(CE_WARN,
			    "dbri_bp_fini: CHI base pipe not DTS'ed\n");
		}

		ATRACE(dbri_basepipe_fini, ' aud', PIPENO(bpipep));

		dbri_mmcodec_disconnect(unitp, dchan);
		if (bpipep->refcnt <= 1 && unitp->distate.monitor_gain == 0) {
			/*
			 * Done with audio: enable power management
			 */
			(void) pm_idle_component(unitp->dip, 2);
		}
		break;

	case DBRI_PIPE_TE_D_IN:
	case DBRI_PIPE_TE_D_OUT:
	case DBRI_PIPE_NT_D_OUT:
	case DBRI_PIPE_NT_D_IN:
		/*
		 * dbri_pipe_remove checks the reference count
		 * so just call it to decrement the reference
		 * count and to actually remove it if it is not
		 * needed anymore.
		 */
		if (!dbri_pipe_remove(unitp, bpipep)) {
			cmn_err(CE_PANIC, "basepipe remove failed");
		}
		if (!ISPIPEINUSE(&unitp->ptab[DBRI_PIPE_TE_D_IN]) &&
		    !ISPIPEINUSE(&unitp->ptab[DBRI_PIPE_TE_D_OUT]) &&
		    !ISPIPEINUSE(&unitp->ptab[DBRI_PIPE_NT_D_OUT]) &&
		    !ISPIPEINUSE(&unitp->ptab[DBRI_PIPE_NT_D_IN]))
			(void) pm_idle_component(unitp->dip, 1);

		break;

	default:
		cmn_err(CE_WARN,
		    "dbri: can't clean up unknown base pipe");
		ATRACE(dbri_basepipe_fini, 'bgus', PIPENO(bpipep));
		return (B_FALSE);
	}

	ATRACE(dbri_basepipe_fini, ' :^)', PIPENO(bpipep));
	return (B_TRUE);
}


/*
 * dbri_pipe_insert - insert a pipe into an interfaces timeslot ring.
 *
 * dbri_dts will automatically make this pipe a monitor pipe if necessary
 * (since we do not have the information to do this ourselves), however
 * dbri_pipe_remove *will* have the information to detect monitor pipes
 * and is responsible for dealing with them itself (ie, re-inserting the
 * monitor pipe if the underlying pipe is deleted).
 *
 * XXX NB: this routine doesn't check to see if it is inserting a pipe
 * twice via the reference count since basepipe inserts can already have
 * a positive reference count.
 */
boolean_t
dbri_pipe_insert(dbri_unit_t *unitp, dbri_pipe_t *pipep)
{
	int refcnt;

	ASSERT_UNITLOCKED(unitp);

	ASSERT(pipep != NULL);
	ASSERT(pipep->refcnt >= 0);
	refcnt = pipep->refcnt;
	pipep->refcnt++;
	ATRACE(dbri_pipe_insert, 'ref+', XPIPENO(pipep));

	/*
	 * Pipe is already inserted, just return
	 */
	if (refcnt)
		return (B_TRUE);

	if (!dbri_dts(unitp, pipep, DBRI_DTS_INSERT)) {
		--pipep->refcnt;
		ATRACE(dbri_pipe_insert, 'ref-', XPIPENO(pipep));
		return (B_FALSE);
	}

	return (B_TRUE);
} /* dbri_pipe_insert */


/*
 * dbri_pipe_remove - remove a pipe from the timeslot rings of any
 * interfaces it is connected to.  This routine also handles monitor
 * pipes in a semi-intelligent manner--- it will make the monitor
 * pipe the real pipe when we delete the real pipe.
 */
boolean_t
dbri_pipe_remove(dbri_unit_t *unitp, dbri_pipe_t *pipep)
{
	dbri_pipe_t *monitorpipep;

	ASSERT_UNITLOCKED(unitp);
	monitorpipep = NULL;

	/*
	 * We don't require a non-zero reference count (the insertion
	 * may have never occured or it may have failed), but if the
	 * reference count is non-zero, decrement it.
	 */
	ASSERT(pipep->refcnt >= 0);
	if (pipep->refcnt > 0) {
		pipep->refcnt--;
		ASSERT(pipep->refcnt >= 0);
		ATRACE(dbri_pipe_remove, 'ref-', XPIPENO(pipep));
	} else {
		/*
		 * We only want to remove the pipe on a refcnt>0 ->
		 * refcnt==0 transition; return if the refcnt is
		 * already zero since the pipe has already been
		 * removed.
		 */
		return (B_TRUE);
	}

	if (pipep->refcnt != 0)
		return (B_TRUE);

	if (pipep->monitorpipep == NULL) {
		/*
		 * Normal pipe removal...
		 */
		ATRACE(dbri_pipe_remove, 'pip1', XPIPENO(pipep));
		if (!dbri_dts(unitp, pipep, DBRI_DTS_DELETE)) {
			cmn_err(CE_WARN,
			    "dbri_pipe_remove: deleting time slot failed");
			return (B_FALSE);
		}
	} else {
		/*
		 * Monitor or Monitored pipe removal
		 */
		if (!pipep->ismonitor) {
			ATRACE(dbri_pipe_remove, 'pip2', XPIPENO(pipep));
			/*
			 * If this pipe is being monitored, then delete
			 * the monitor pipe, delete this pipe, and then
			 * reinsert the monitor pipe.
			 */
			monitorpipep = pipep->monitorpipep;
			ATRACE(dbri_pipe_remove, 'pip3', XPIPENO(monitorpipep));

			pipep->monitorpipep = NULL;
			monitorpipep->ismonitor = B_FALSE;
			monitorpipep->monitorpipep = NULL;

			if (monitorpipep->ds != NULL)
				dbri_stop(DsToAs(monitorpipep->ds));

			if (!dbri_dts(unitp, pipep, DBRI_DTS_DELETE)) {
				cmn_err(CE_WARN,
	"dbri_pipe_remove: deleting underlying time slot failed");
				return (B_FALSE);
			}
		} else {
			ATRACE(dbri_pipe_remove, 'pip4', XPIPENO(pipep));

			/*
			 * If this is a monitor pipe then remove
			 * it by re-issuing the DTS command for
			 * the underlying pipe
			 * (pipep->monitorpipep)(arcane knowledge
			 * not really in the DBRI hardware spec).
			 */
			ASSERT(pipep->monitorpipep != NULL);
			ATRACE(dbri_pipe_remove, 'pip5', XPIPENO(pipep));

			pipep->monitorpipep->dts.input_tsd.r.mode =
			    DBRI_DTS_SINGLE;
			if (!dbri_dts(unitp, pipep->monitorpipep,
			    DBRI_DTS_MODIFY)) {
				cmn_err(CE_WARN,
	"dbri_pipe_remove: removing monitor pipe failed");
				return (B_FALSE);
			}

			pipep->monitorpipep->monitorpipep = NULL;
			pipep->monitorpipep->ismonitor = B_FALSE;
			pipep->monitorpipep = NULL;
			pipep->ismonitor = B_FALSE;
		}
	}

	pipep->ds = NULL;
	dbri_pipe_free(pipep);

	/*
	 * Reinsert the monitor pipe if we deleted it
	 */
	if (monitorpipep != NULL) {
		ATRACE(dbri_pipe_remove, 'mdts', XPIPENO(monitorpipep));

		if (!dbri_dts(unitp, monitorpipep,
		    DBRI_DTS_INSERT)) {
			cmn_err(CE_WARN,
		    "dbri_pipe_remove: monitor dbri_dts 2 failed");
			return (B_FALSE);
		}
		if (monitorpipep->ds != NULL) {
			dbri_start(DsToAs(monitorpipep->ds));
			/* audio_process_record? */
		}
	}

	return (B_TRUE);
} /* dbri_pipe_remove */


/*
 * dbri_find_oldnext - finds the oldpipe, nextpipe, and monitor pipes for
 * the given timeslot. (ie determines where in a linked list of timeslots
 * a new pipe should go.
 */
boolean_t
dbri_find_oldnext(dbri_unit_t *unitp,
	dbri_pipe_t *pipep, int dir,
	dbri_pipe_t **oldpipep, dbri_pipe_t **nextpipep,
	dbri_pipe_t **monpipep)
{
	union dbri_dts_tsd ttsd;
	int minvalue = INT_MAX; /* larger than any cycle */
	dbri_pipe_t *bpipep;
	dbri_pipe_t *tpipep;
	dbri_pipe_t *minpipep;
	uint_t cycle, length;
	int counter;

	ASSERT_UNITLOCKED(unitp);

	*monpipep = NULL;
	*oldpipep = NULL;
	*nextpipep = NULL;

	/*
	 * Extract the length and cycle information from the
	 * pipe depending on which direction we are setting
	 * up.
	 */
	switch (dir) {
	case DBRI_DIRECTION_IN:
		cycle = pipep->source.cycle;
		length = pipep->source.length;
		bpipep = pipep->source.basepipep;
		break;

	case DBRI_DIRECTION_OUT:
		cycle = pipep->sink.cycle;
		length = pipep->sink.length;
		bpipep = pipep->sink.basepipep;
		break;

	default:
		return (B_FALSE);
	} /* switch on direction */

	ASSERT(bpipep != NULL);

	/*
	 * If this will be the only tsd on the ring, the answers are easy
	 */
	if (pipep == bpipep) {
		*nextpipep = pipep;
		*oldpipep = pipep;
		return (B_TRUE);
	}

	minpipep = tpipep = bpipep;

	ATRACE(dbri_find_oldnext, 'pipe', PIPENO(bpipep));

	/*
	 * Skip over pipe 16 since it doesn't have a len or cycle
	 */
	if (PIPENO(bpipep) == DBRI_PIPE_CHI) {
		if (dir == DBRI_DIRECTION_IN)
			ttsd.word32 = bpipep->dts.input_tsd.word32;
		else
			ttsd.word32 = bpipep->dts.output_tsd.word32;

		ATRACE(dbri_find_oldnext, 'CHI ', 0);
		ATRACE(dbri_find_oldnext, 'pipe', ttsd.chi.next);

		tpipep = &unitp->ptab[ttsd.chi.next];

		/*
		 * CHI transmit pipes can't start at zero, they must
		 * start at the length of the frame so that they actually
		 * *start* at zero - receive pipes can start at zero.
		 * Having a cycle 128 imply the beginning of the 128 bit
		 * frame confuses the list management logic.
		 *
		 * Notice the receive side will go through the regular code.
		 */
		if (cycle >= CHI_DATA_MODE_LEN) {
			*oldpipep = bpipep;
			*nextpipep = tpipep;

			return (B_TRUE);

		} else if (ttsd.r.cycle >= CHI_DATA_MODE_LEN) {
			*oldpipep = tpipep;
			*nextpipep = &unitp->ptab[ttsd.r.next];

			return (B_TRUE);
		}
	}

	/*
	 * This first loop looks for potential monitor pipes, overlaps in
	 * timeslots and finds the minimum bit offset value in the
	 * circular linked list.
	 */
	counter = 0;
	do {
		if (dir == DBRI_DIRECTION_IN)
			ttsd.word32 = tpipep->dts.input_tsd.word32;
		else
			ttsd.word32 = tpipep->dts.output_tsd.word32;

		/*
		 * Check if we timeslots match..perhaps monitor pipe
		 * candidate
		 *
		 * NB: for CHI, len and cycle are not there, but are
		 * reserved fields of zero; this works since they won't
		 * match
		 */
		if ((ttsd.r.len == length) && (ttsd.r.cycle == cycle)) {
			/* Monitor pipes must tap off of an input pipe */
			if (dir == DBRI_DIRECTION_OUT) {
				ATRACE(dbri_find_oldnext, '!mon', 0);
				return (B_FALSE);
			}

			*oldpipep = &unitp->ptab[tpipep->dts.opcode.r.oldin];
			*nextpipep = &unitp->ptab[ttsd.r.next];
			*monpipep = tpipep;

			return (B_TRUE);
		}

		/*
		 * Check for illegal overlapping offsets and cycles
		 *
		 * This test means that an open ISDN H-channel will lock
		 * out the corresponding B2 channel and visa versa.
		 * It is very obscure to have the test nested this far
		 * down in the code and maybe we will want overlapping
		 * channels later?
		 *
		 * Hmmm...
		 * This test might also mean that it would be easy to
		 * implement left and right audio channels as separate
		 * devices without screwing up anything else. We would have
		 * fix the fixed pipe management code in dbri_mmcodec.c
		 */
		if (dbri_tsd_overlap(cycle, (cycle+length), ttsd.r.cycle,
		    (ttsd.r.cycle + ttsd.r.len))) {
			ATRACE(dbri_find_oldnext, 'over', -1);
			return (B_FALSE);
		}

		/*
		 * Get the minimum cycle bit offset in the list
		 */
		if ((uint_t)ttsd.r.cycle < (uint_t)minvalue) {
			minvalue = ttsd.r.cycle;
			minpipep = tpipep;
		}
		tpipep = &unitp->ptab[ttsd.r.next];
	} while (tpipep != bpipep && ++counter < 32);

	if (counter >= 32) {
		cmn_err(CE_WARN, "dbri_find_oldnext: counter exceeded!\n");
		ATRACE(dbri_find_oldnext, 'cntr', -1);
		return (B_FALSE);
	}

	/*
	 * Find spot in list to insert before.  Note that we are starting
	 * with the minimum offset element in list.
	 */
	tpipep = minpipep;
	do {
		ttsd.word32 = (dir == DBRI_DIRECTION_IN) ?
		    tpipep->dts.input_tsd.word32 :
		    tpipep->dts.output_tsd.word32;

		if ((uint_t)cycle < (uint_t)ttsd.r.cycle)
			break;

		/* advance to next pipe in list */
		tpipep = &unitp->ptab[ttsd.r.next];

		if ((PIPENO(bpipep) == DBRI_PIPE_CHI) &&
		    (PIPENO(tpipep) == DBRI_PIPE_CHI))
			break;

	} while (tpipep != minpipep);

	/*
	 * Now set oldpipe and nextpipe variables
	 */
	*nextpipep = tpipep;
	*oldpipep = &unitp->ptab[(dir == DBRI_DIRECTION_IN) ?
	    tpipep->dts.opcode.r.oldin : tpipep->dts.opcode.r.oldout];

	return (B_TRUE);
} /* dbri_find_oldnext */


/*
 * dbri_pipe_allocate - Get an available free pipe.  This routine
 * allocates the appropriate type of pipe {long or short} based on the
 * data channel.  Note that if the channel is of an unknown type then a
 * short pipe is automatically allocated. This could be a problem in the
 * future when unknown pipes are setup by the user to the host.
 *
 * NB: every pipep will have it's basepipe pointer in pipep->basepipep.
 */
dbri_pipe_t *
dbri_pipe_allocate(dbri_unit_t *unitp, dbri_conn_oneway_t *dcrp)
{
	uint_t pipe, maxpipe;
	dbri_pipe_t *pipep, *bpipep, *bpipep1, *bpipep2;
	dbri_chan_t dchan1, dchan2, dchan;
	int pipetype;

	ASSERT_UNITLOCKED(unitp);

	dchan1 = dcrp->sink.dchan;
	dchan2 = dcrp->source.dchan;

	ATRACE(dbri_pipe_allocate, 'dchn', dchan1);
	ATRACE(dbri_pipe_allocate, 'dchn', dchan2);

	pipep = bpipep = bpipep1 = bpipep2 = NULL;

	/*
	 * We don't support connections between two "special" channels,
	 * so assume that if there isn't a HOST endpoint, then the
	 * connection is serial-serial and will not require a special
	 * pipe.
	 */
	dchan = DBRI_CHAN_NONE;
	if (dchan1 != DBRI_CHAN_HOST && dchan1 != DBRI_CHAN_NONE) {
		dchan = dchan1;
		bpipep1 = dbri_basepipe(unitp, dchan1);
		bpipep = bpipep1;
	}
	if (dchan2 != DBRI_CHAN_HOST && dchan2 != DBRI_CHAN_NONE) {
		dchan = dchan2;
		bpipep2 = dbri_basepipe(unitp, dchan2);
		bpipep = bpipep2;
	}

	/*
	 * D-channels are required to use predetermined pipes--- they are
	 * their own basepipes.
	 */
	if ((dchan == DBRI_TE_D_OUT) ||
	    (dchan == DBRI_TE_D_IN) ||
	    (dchan == DBRI_NT_D_OUT) ||
	    (dchan == DBRI_NT_D_IN)) {

		if (!ISPIPEINUSE(bpipep)) {
			ATRACE(dbri_pipe_allocate, '!use', PIPENO(bpipep));
			pipep = bpipep;
			goto done;
		}

		ATRACE(dbri_pipe_allocate, 'bppe', PIPENO(bpipep));
		return (bpipep);
	}

	/*
	 * Non-predetermined pipes
	 */
	if (dchan1 != DBRI_CHAN_HOST && dchan2 != DBRI_CHAN_HOST) {
		if (dcrp->format.length > 32) /* hardware limitation */
			pipetype = DBRI_LONGPIPE;
		else
			pipetype = DBRI_SHORTPIPE;
	} else if (dchan1 == DBRI_CHAN_FIXED || dchan2 == DBRI_CHAN_FIXED) {
		pipetype = DBRI_SHORTPIPE;
	} else {
		pipetype = DBRI_LONGPIPE;
	}

	if (pipetype == DBRI_LONGPIPE) {
		pipe = DBRI_PIPE_LONGFREE;
		maxpipe = DBRI_PIPE_CHI;
	} else {
		pipe = DBRI_PIPE_SHORTFREE;
		maxpipe = DBRI_MAX_PIPES;
	}

	/*
	 * Search the list of pipes looking for one not in use.
	 */
	for (; pipe < maxpipe; pipe++) {
		pipep = &unitp->ptab[pipe];
		if (!pipep->allocated)
			break; /* free pipe available */
	}
	if (pipe == maxpipe)
		return (NULL);	/* no free pipes */

done:
	/*
	 * Initialize the SDP, SSP, and DTS commands with their opcodes
	 * and their pipe number.
	 */
	pipep->sdp = DBRI_CMD_SDP | PIPENO(pipep);
	pipep->ssp = DBRI_CMD_SSP | DBRI_SSP_PIPE(PIPENO(pipep));
	pipep->dts.opcode.word32 = 0; /* clear out all state */
	pipep->dts.opcode.r.cmd = DBRI_OPCODE_DTS;
	pipep->dts.opcode.r.pipe = PIPENO(pipep);
	pipep->dts.input_tsd.word32 = 0;
	pipep->dts.output_tsd.word32 = 0;

	pipep->refcnt = 0;
	pipep->allocated = B_TRUE;
	pipep->sink.dchan = dcrp->sink.dchan;
	pipep->sink.basepipep = bpipep1;
	pipep->source.dchan = dcrp->source.dchan;
	pipep->source.basepipep = bpipep2;
	pipep->otherpipep = NULL;
	pipep->ismonitor = B_FALSE;
	pipep->monitorpipep = NULL;

	/*
	 * Copy the format into the pipe.  The pipe's mode must be
	 * changed to SERIAL if neither end is connected to the host.
	 *
	 * NB - The conditional will have to change when we support
	 * SERIAL-FIXED connections since neither end of that type of
	 * connection is "host".
	 */
	pipep->format = dcrp->format;

	ATRACE(dbri_pipe_allocate, 'pipe', pipe);
	return (pipep);
} /* dbri_pipe_allocate */


/*
 * dbri_pipe_free - return a pipe to the free pipe pool
 */
void
dbri_pipe_free(dbri_pipe_t *pipep)
{
	ATRACE(dbri_pipe_free, 'free', PIPENO(pipep));

#if defined(DEBUG) /* XXX - Remove later */
	if (pipep->ds != NULL) {
		cmn_err(CE_WARN,
		    "dbri: pipe being freed contains ds pointer...");
	}
	if (pipep->monitorpipep != NULL) {
		cmn_err(CE_WARN,
		    "dbri: pipe being freed contains monitor pointer...");
	}
	if (pipep->refcnt != 0) {
		cmn_err(CE_WARN, "dbri: pipe[%d].refcnt = %d", PIPENO(pipep),
		    pipep->refcnt);
	}
	if (!pipep->allocated) {
		cmn_err(CE_WARN, "dbri: pipe being freed (%d) is free...",
		    PIPENO(pipep));
	}
#endif

	pipep->sink.basepipep = NULL;
	pipep->source.basepipep = NULL;
	pipep->otherpipep = NULL;
	pipep->allocated = B_FALSE;
	pipep->refcnt = 0;
	pipep->sink.dchan = DBRI_CHAN_NONE;
	pipep->source.dchan = DBRI_CHAN_NONE;

	bzero((caddr_t)&pipep->format, sizeof (pipep->format));

} /* dbri_pipe_free */


/*
 * dbri_findshortpipe - searches for a short pipe in use that matches
 * channels requested
 */
dbri_pipe_t *
dbri_findshortpipe(dbri_unit_t *unitp, dbri_chan_t source, dbri_chan_t sink)
{
	dbri_pipe_t *pipep;
	uint_t pipe;

	ASSERT_UNITLOCKED(unitp);

	/*
	 * search short pipe list for matching connection
	 */
	for (pipe = DBRI_PIPE_CHI; pipe < DBRI_MAX_PIPES; pipe++) {
		pipep = &unitp->ptab[pipe];
		if (!ISPIPEINUSE(pipep))
			continue;
		if (sink == pipep->sink.dchan &&
		    source == pipep->source.dchan) {
			break;	/* found a match */
		}
	}
	if (pipe >= DBRI_MAX_PIPES)
		return (NULL);	/* cannot find connection */

	if (!ISPIPEINUSE(pipep))
		return (NULL);

	return (pipep);
} /* dbri_findshortpipe */


/*
 * dbri_tsd_overlap - check if anywhere the values of a1 thru a2
 * inclusive lie within the values of x1 thru x2 inclusive.
 */
static int
dbri_tsd_overlap(uint_t a1, uint_t a2, uint_t x1, uint_t x2)
{
	return (((a1 < x1) && (a2 > x1)) || ((a1 > x1) && (a1 < x2)));
}


/*
 * dbri_setup_dts - Setup in-core version of DTS command.
 */
void
dbri_setup_dts(dbri_unit_t *unitp, dbri_pipe_t *pipep, dbri_chantab_t *ctep)
{
	dbri_chanformat_t *dfmt;
	uint_t cycle;	/* XXX - should be taken from fmt */
	uint_t length;	/* XXX - should be taken from fmt */
	uint_t mode;

	ASSERT(unitp != NULL);
	ASSERT(ctep != NULL);
	ASSERT(pipep != NULL);

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_setup_dts, 'pipe', PIPENO(pipep));

	dfmt = &pipep->format;

	/*
	 * These fields are not found in the Channel table for audio
	 * data, get them from the current audio configuration
	 * information.
	 *
	 * XXX - different methods for audio vs. ISDN, not good.
	 */
	if (dbri_is_dbri_audio_chan(ctep->dchan)) {
		/*
		 * SBI time slots can "float".
		 *
		 * XXX - Using static formats now.
		 *
		 * XXX - The format for this channel should already be
		 * setup for the current SBI configuration.
		 */
		cycle = get_aud_pipe_cycle(unitp, pipep, ctep->dchan);

		/*
		 * XXX - length always depends on format and should
		 * already be available from fmt->length.
		 */
		length = get_aud_pipe_length(unitp, pipep, ctep->dchan);
	} else {
		cycle = ctep->cycle;
		length = dfmt->length;
	}
	ATRACE(dbri_setup_dts, 'leng', length);
	ATRACE(dbri_setup_dts, 'cycl', cycle);

	/*
	 * dbri_sdp will change this if necessary to make a monitor pipe
	 */
	mode = DBRI_DTS_SINGLE;

	/*
	 * Setup dts command words
	 *
	 * Note that for serial to serial connections 1/2 of the dts
	 * command can already been setup. This is why we don't zero out
	 * anything. Upon release of tsd everything is cleared.
	 */
	pipep->dts.opcode.r.cmd = DBRI_OPCODE_DTS;
	pipep->dts.opcode.r.pipe = PIPENO(pipep);
	pipep->dts.opcode.r.id = DBRI_DTS_ID;

	ATRACE(dbri_setup_dts, 'dchn', ctep->dchan);

	if (ctep->dir == DBRI_DIRECTION_IN) {
		ATRACE(dbri_setup_dts, 'dchn', pipep->source.dchan);
		pipep->dts.opcode.r.vi = DBRI_DTS_VI;
		ASSERT(pipep->source.basepipep != NULL);
		pipep->dts.opcode.r.oldin = PIPENO(pipep->source.basepipep);

		pipep->dts.input_tsd.r.len = length;
		pipep->dts.input_tsd.r.cycle = cycle;
		pipep->dts.input_tsd.r.mode = mode;

		pipep->source.cycle = cycle;
		pipep->source.length = length;
	} else {
		ATRACE(dbri_setup_dts, 'dchn', pipep->sink.dchan);
		pipep->dts.opcode.r.vo = DBRI_DTS_VO;
		ASSERT(pipep->sink.basepipep != NULL);
		pipep->dts.opcode.r.oldout = PIPENO(pipep->sink.basepipep);

		pipep->dts.output_tsd.r.len = length;
		pipep->dts.output_tsd.r.cycle = cycle;
		pipep->dts.output_tsd.r.mode = mode;

		pipep->sink.cycle = cycle;
		pipep->sink.length = length;
	}

} /* dbri_setup_dts */


/*
 * dbri_setup_sdp - sets up the in memory copy of the sdp and te/nt
 * commands
 */
/*ARGSUSED*/
void
dbri_setup_sdp(dbri_unit_t *unitp, dbri_pipe_t *pipep, dbri_chantab_t *ctep)
{
	dbri_chanformat_t *dfmt;

	ASSERT(unitp != NULL);
	ASSERT_UNITLOCKED(unitp);
	ASSERT(ctep != NULL);
	ASSERT(pipep != NULL);
	ATRACE(dbri_setup_sdp, 'pipe', PIPENO(pipep));

	dfmt = &pipep->format;

	if (pipep->source.dchan != DBRI_CHAN_HOST &&
	    pipep->sink.dchan != DBRI_CHAN_HOST) {
		switch (PIPENO(pipep)) {
		case DBRI_PIPE_TE_D_OUT:
		case DBRI_PIPE_TE_D_IN:
		case DBRI_PIPE_NT_D_OUT:
		case DBRI_PIPE_NT_D_IN:
			break;

		default:
			dfmt->mode = DBRI_MODE_SERIAL;
			break;
		}
	}

	/*
	 * Setup the SDP command.
	 *
	 * dfmt->mode will be one of HDLC, HDLC_D, or TRANSPARENT.
	 *
	 * XXX5 - Don't enable UNDR interrupt reporting
	 */
	ATRACE(dbri_setup_sdp, 'mode', dfmt->mode);
	if (dfmt->mode != DBRI_MODE_SERIAL) {
		pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_EOL | dfmt->mode |
		    DBRI_SDP_PTR | DBRI_SDP_CLR | PIPENO(pipep);
	} else {
		/* serial-serial doesn't use PTR */
		pipep->sdp = DBRI_CMD_SDP | DBRI_SDP_EOL | dfmt->mode |
		    DBRI_SDP_CLR | PIPENO(pipep);
	}

	/*
	 * Detect collisions on TE D-channel
	 */
	if (dfmt->mode == DBRI_MODE_HDLC_D)
		pipep->sdp |= DBRI_SDP_COLL;

	/*
	 * CHI and B-channels in transparent mode have MSB bit-ordering
	 */
	if (dfmt->mode == DBRI_MODE_TRANSPARENT)
		pipep->sdp |= DBRI_SDP_B;

	if ((ctep->dir == DBRI_DIRECTION_OUT) &&
	    (dfmt->mode != DBRI_MODE_SERIAL))
		pipep->sdp |= DBRI_SDP_D; /* set transmit direction */

	ATRACE(dbri_setup_sdp, ' sdp', pipep->sdp);

} /* dbri_setup_sdp */
