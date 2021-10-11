/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dbri_isdn.c	1.36	97/10/22 SMI"

/*
 * DBRI ISDN related routines
 */

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/ioccom.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/errno.h>
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
 * Local function prototypes
 */
static void dbri_exit_f3(dbri_unit_t *);


/*
 * dbri_setup_ntte - setup serial interface but *DO NOT* issue activate
 * command.  Also start timeout timers.
 */
void
dbri_setup_ntte(dbri_unit_t *unitp, dbri_chan_t dchan)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t cmd;
	dbri_pipe_t *pipe_in;
	dbri_pipe_t *pipe_out;
	aud_stream_t *as;

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_setup_ntte, 'dchn', dchan);

	if ((dchan == DBRI_TE_D_IN) || (dchan == DBRI_TE_D_OUT)) {
		bs = DBRI_TE_STATUS_P(unitp);

		pipe_in = &unitp->ptab[DBRI_PIPE_TE_D_OUT]; /* XXX pipe_in? */
		pipe_out = &unitp->ptab[DBRI_PIPE_TE_D_IN]; /* XXX pipe_out? */
		as = DChanToAs(unitp, DBRI_TE_D_OUT);

		/*
		 * Make sure both pipes setup before enabling interface
		 */
		if (!ISPIPEINUSE(pipe_in) || !ISPIPEINUSE(pipe_out)) {
			ATRACE(dbri_setup_ntte, '!yet', dchan);
			return;
		}

		/*
		 * Reset TE's activation state machine.
		 */
		dbri_disable_te(unitp);
		bs->sbri.tss = DBRI_TEINFO0_F1;
		bs->i_info.activation = ISDN_DEACTIVATED;

		/*
		 * Because of the DBRI implementation, it is proper and
		 * required to send MPH-INFORMATION.ind(connected) here.
		 */
		dbri_primitive_mph_ii_c(as);
		/* XXX - Do we also need PH_DI, MPH_DI? */

		/*
		 * Enable TE interface.  This is only done once initially
		 * and then is not touched again until close is called.
		 * This enables F3 as well as allows NT to activate the
		 * interface.
		 *
		 * Note: The exception is on expiry of T3. In that case,
		 * DBRI_STS_T may be toggled to force DBRI TE into F3,
		 * depending on the state of the activation state
		 * machine.
		 */
		dbri_enable_te(unitp);

		/*
		 * Setup TE command
		 */
		bs->ntte_cmd = (DBRI_CMD_TE | DBRI_NTE_IRM_STATUS |
		    DBRI_NTE_IRM_SBRI | DBRI_NTE_ABV);

		if (bs->i_var.maint == ISDN_PARAM_MAINT_OFF)
			bs->ntte_cmd &= ~DBRI_TE_QE;
		else
			bs->ntte_cmd |= DBRI_TE_QE;

		if (bs->dbri_f == B_TRUE)
			bs->ntte_cmd |= DBRI_NTE_F;
		else
			bs->ntte_cmd &= ~DBRI_NTE_F;	/* default case */

		if (bs->dbri_nbf == 2) {
			bs->ntte_cmd |= DBRI_NTE_NBF;
		} else if (bs->dbri_nbf == 3) {
			bs->ntte_cmd &= ~DBRI_NTE_F;	/* default case */
		} else {
			cmn_err(CE_WARN,
			    "dbri: TE, Incorrect NBF parameter, assume 3");
			bs->ntte_cmd &= ~DBRI_NTE_F;	/* default case */
		}

		cmd.opcode = bs->ntte_cmd;
		dbri_command(unitp, cmd);

	} else if ((dchan == DBRI_NT_D_IN) || (dchan == DBRI_NT_D_OUT)) {
		bs = DBRI_NT_STATUS_P(unitp);

		pipe_in = &unitp->ptab[DBRI_PIPE_NT_D_OUT];
		pipe_out = &unitp->ptab[DBRI_PIPE_NT_D_IN];
		as = DChanToAs(unitp, DBRI_NT_D_OUT);

		/*
		 * Make sure both pipes setup before enabling interface
		 */
		if (!ISPIPEINUSE(pipe_in) || !ISPIPEINUSE(pipe_out)) {
			ATRACE(dbri_setup_ntte, '!yet', dchan);
			return;
		}

		/*
		 * Reset NT's activation state machine.
		 */
		dbri_disable_nt(unitp);
		bs->sbri.tss = DBRI_NTINFO0_G1;
		bs->i_info.activation = ISDN_DEACTIVATED;

		/*
		 * Setup NT and start relay sanity timer.  To prevent
		 * deadlock with untimeout, we normally disable the
		 * keepalive by setting keep_alive_running to B_FALSE
		 * and letting it coast to a stop.  This will result
		 * in many timers running if someone opens and closes
		 * the nt d-channel rapidly so we don't restart one
		 * if there is already one running (it'll see the
		 * the keep_alive_running variable and perpetuate
		 * itself).  If one's not running, then we have to
		 * kickstart it.
		 */
		if (!unitp->keep_alive_running) {
			unitp->keep_alive_running = B_TRUE;
			if (unitp->keepalive_id == 0) {
				unitp->keepalive_id = timeout(dbri_keep_alive,
				    unitp, TICKS(Keepalive_timer));
			}
		}
		unitp->regsp->n.sts &= ~(DBRI_STS_F|DBRI_STS_X);

		/*
		 * Enable and configure the NT interface
		 */
		dbri_enable_nt(unitp);

		bs->ntte_cmd = DBRI_CMD_NT | DBRI_NTE_IRM_STATUS |
		    DBRI_NTE_IRM_SBRI | DBRI_NTE_ISNT | DBRI_NTE_ABV;

		if (bs->i_var.maint == ISDN_PARAM_MAINT_OFF)
			bs->ntte_cmd &= ~DBRI_NT_MFE;
		else
			bs->ntte_cmd |= DBRI_NT_MFE;

		if (bs->dbri_f == B_TRUE)
			bs->ntte_cmd |= DBRI_NTE_F;
		else
			bs->ntte_cmd &= ~DBRI_NTE_F;	/* default case */

		if (bs->dbri_nbf == 2) {
			bs->ntte_cmd |= DBRI_NTE_NBF;
		} else if (bs->dbri_nbf == 3) {
			bs->ntte_cmd &= ~DBRI_NTE_F;	/* default case */
		} else {
			cmn_err(CE_WARN,
			    "dbri: NT, incorrect NBF parameter, assume 3");
			bs->ntte_cmd &= ~DBRI_NTE_F;	/* default case */
		}

		cmd.opcode = bs->ntte_cmd;
		dbri_command(unitp, cmd);
	}
} /* dbri_setup_ntte */


/*
 * dbri_forceactivation - set the force-activation bit for the
 * given interface
 */
boolean_t
dbri_forceactivation(dbri_unit_t *unitp, isdn_interface_t britype,
	boolean_t flag)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t cmd;

	switch (britype) {
	case ISDN_TYPE_TE:
		ATRACE(dbri_forceactivation, '  TE', 1);
		bs = DBRI_TE_STATUS_P(unitp);
		break;

	case ISDN_TYPE_NT:
		ATRACE(dbri_forceactivation, '  NT', 1);
		bs = DBRI_NT_STATUS_P(unitp);
		break;

	default:
		return (0);
	}

	/*
	 * Don't execute an NTTE command when not necessary
	 */
	if (bs->fact == flag)
		return (B_TRUE);

	if (flag) {
		ATRACE(dbri_forceactivation, 'FACT', 1);
		bs->ntte_cmd |= DBRI_NTE_FACT;
	} else {
		ATRACE(dbri_forceactivation, 'FACT', 0);
		bs->ntte_cmd &= ~DBRI_NTE_FACT;
	}

	cmd.opcode = bs->ntte_cmd;
	dbri_command(unitp, cmd);

	return (B_FALSE);
} /* dbri_forceactivation */


/*
 * dbri_set_f - set/clear the "Accept an F bit which is not a bipolar
 * violation after an errored frame" bit.
 */
boolean_t
dbri_set_f(dbri_unit_t *unitp, isdn_interface_t britype,
	boolean_t flag)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t cmd;

	switch (britype) {
	case ISDN_TYPE_TE:
		ATRACE(dbri_set_f, '  TE', 1);
		bs = DBRI_TE_STATUS_P(unitp);
		break;

	case ISDN_TYPE_NT:
		ATRACE(dbri_set_f, '  NT', 1);
		bs = DBRI_NT_STATUS_P(unitp);
		break;

	default:
		return (B_FALSE);
	}

	/*
	 * Don't execute an NTTE command when not necessary
	 */
	ATRACE(dbri_set_f, 'BS_F', bs->dbri_f);
	ATRACE(dbri_set_f, 'FLAG', flag);
	if (bs->dbri_f == flag)
		return (B_TRUE);

	if (flag) {
		ATRACE(dbri_set_f, 'F_OK', 1);
		bs->ntte_cmd |= DBRI_NTE_F;
		bs->dbri_f = B_TRUE;
	} else {
		ATRACE(dbri_forceactivation, 'FNOT', 0);
		bs->ntte_cmd &= ~DBRI_NTE_F;
		bs->dbri_f = B_FALSE;
	}
	ATRACE(dbri_set_f, 'NTTE', bs->ntte_cmd);

	cmd.opcode = bs->ntte_cmd;
	dbri_command(unitp, cmd);

	return (B_FALSE);
} /* dbri_set_f */


/*
 * dbri_set_nbf - set the number of bad frames needed to loose framing to
 * either 2 or 3.
 */
boolean_t
dbri_set_nbf(dbri_unit_t *unitp, isdn_interface_t britype,
	int nbf)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t cmd;

	switch (britype) {
	case ISDN_TYPE_TE:
		ATRACE(dbri_set_nbf, '  TE', 1);
		bs = DBRI_TE_STATUS_P(unitp);
		break;

	case ISDN_TYPE_NT:
		ATRACE(dbri_set_nbf, '  NT', 1);
		bs = DBRI_NT_STATUS_P(unitp);
		break;

	default:
		return (B_FALSE);
	}

	/*
	 * Don't execute an NTTE command when not necessary
	 */
	ATRACE(dbri_set_f, 'ONBF', bs->dbri_nbf);
	ATRACE(dbri_set_f, 'NNBF', nbf);
	if (bs->dbri_nbf == nbf)
		return (B_TRUE);

	switch (nbf) {
	case 2:
		ATRACE(dbri_set_nbf, 'NBF-', 2);
		bs->ntte_cmd |= DBRI_NTE_NBF;
		bs->dbri_nbf = 2;
		break;

	case 3:
		ATRACE(dbri_set_nbf, 'NBF-', 3);
		bs->ntte_cmd &= ~DBRI_NTE_NBF;
		bs->dbri_nbf = 3;
		break;

	default:
		return (B_FALSE);
	}
	ATRACE(dbri_set_f, 'NTTE', bs->ntte_cmd);

	cmd.opcode = bs->ntte_cmd;
	dbri_command(unitp, cmd);

	return (B_TRUE);
} /* dbri_set_nbf */


/*
 * dbri_te_timer - Expiry of T3.
 */
static void
dbri_te_timer(void *arg)
{
	aud_stream_t *as = arg;
	dbri_unit_t *unitp;
	dbri_bri_status_t *bs;

	if (as == NULL) {
		ATRACE(dbri_te_timer, 'BGUS', 0);
		return;
	}

	LOCK_AS(as);
	ATRACE(dbri_te_timer, '  as', as);

	/* Don't do anything if H/W not connected */
	if (!ISHWCONNECTED(as)) {
		ATRACE(dbri_te_timer, ' !hw', 0);
		goto done;
	}

	unitp = AsToUnitp(as);
	bs = DBRI_TE_STATUS_P(unitp);

	switch (bs->sbri.tss) {
	case DBRI_TEINFO3_F6:
		if (bs->i_var.asmb == ISDN_PARAM_TE_ASMB_CTS2) {
			ATRACE(dbri_te_timer, 'f6c2', 0);
			/*
			 * XXX - France Telecom wants us to remain in F5
			 * after T3!
			 */
			dbri_primitive_mph_di(as);
			dbri_primitive_ph_di(as);
			goto done;
		}

		/*
		 * CCITT says to go to state F3 and send PH_DI and MPH_DI
		 */
		ATRACE(dbri_te_timer, 'f6ct', 0);
		break;

	case DBRI_TEINFO0_F1:	/* CCITT: "impossible", it happens in DBRI */
	case DBRI_TEINFO0_F3:	/* no effect */
	case DBRI_TEINFO3_F7:	/* no effect */
		ATRACE(dbri_te_timer, 'f1f3', 0);
		goto done;	/* Sending data */

	case DBRI_TEINFO0_F8:	/* blue book says "no effect", FT disagrees */
		/*
		 * In CTS2, if activation has not been achieved since
		 * F3->F6 or F3->F4, then go to F3 and emit PH-DI and
		 * MPH-DI.
		 *
		 * In I.430 and in CTS2 when activation has been
		 * achieved, then stay in F8.
		 */
		if ((bs->i_var.asmb != ISDN_PARAM_TE_ASMB_CTS2) ||
		    (bs->i_info.activation == ISDN_ACTIVATED)) {
			ATRACE(dbri_te_timer, 'f8ct', 0);
			goto done;
		}

		/*
		 * go to state F3
		 */
		ATRACE(dbri_te_timer, 'f8c2', 0);
		break;

	default:
		ATRACE(dbri_te_timer, 'othr', 0);
		break;
	}

	bs->sbri.tss = DBRI_TEINFO0_F3;
	bs->i_info.activation = ISDN_DEACTIVATED;

	/*
	 * Force DBRI's TE state machine into F3 by resetting the TE
	 * activation bit and then setting.
	 *
	 * NB: Given DBRI's implementation, DBRI will report a transition
	 * to F1. This transition is to be interpreted as a transition to
	 * F3.
	 */
	dbri_disable_te(unitp);
	dbri_hold_f3(unitp);

	/* XXX - Stoltz bets that 2 frame times must go by */
	drv_usecwait(250);

	dbri_enable_te(unitp);

	dbri_bri_down(unitp, ISDN_TYPE_TE);
	dbri_primitive_mph_di(as);
	dbri_primitive_ph_di(as);

	ATRACE(dbri_te_timer, 'down', 0);

done:
	UNLOCK_AS(as);
} /* dbri_te_timer */


/*
 * dbri_nttimer_t101 - See Fascille III.8, Table 6/I.430
 */
void
dbri_nttimer_t101(void *arg)
{
	dbri_unit_t *unitp = arg;
	aud_stream_t *as;
	dbri_bri_status_t *bs;

	if (unitp == NULL)
		return;

	LOCK_UNIT(unitp);
	ATRACE(dbri_nttimer_t101, 'unit', unitp);

	as = DChanToAs(unitp, DBRI_NT_D_OUT);

	unitp->nttimer_t101_id = 0;
	if (!ISHWCONNECTED(as))
		goto done;

	unitp = AsToUnitp(as);
	bs = DBRI_NT_STATUS_P(unitp);

	if ((bs->sbri.tss == DBRI_NTINFO0_G1) ||
	    (bs->sbri.tss == DBRI_NTINFO4_G3) ||
	    (bs->sbri.tss == DBRI_NTINFO0_G4)) {
		/*EMPTY*/
		/* t1! in G3 cannot happen */
#if defined(AUDIOTRACE)
		if (bs->sbri.tss == DBRI_NTINFO4_G3)
			ATRACE(dbri_nttimer_t101, '?g3?', unitp);
#endif

	} else if (bs->sbri.tss == DBRI_NTINFO2_G2) {
		/*
		 * Next state is G4
		 */
		bs->i_info.activation = ISDN_DEACTIVATE_REQ;
		bs->sbri.tss = DBRI_NTINFO0_G4;	/* DBRI "G4 pseudo state */

		/*
		 * DBRI only implements G1, G2, and G3.  G4 is simulated by
		 * diasabling the NT interface.  This means that the I0->G1
		 * transition in G4 cannot be implemented by this driver
		 * and t102 is not allowed to be zero.
		 */
		dbri_disable_nt(unitp);	/* Force DBRI into its "G1" state */
		dbri_hold_g1(unitp);	/* Stop requesting activation */
		dbri_bri_down(unitp, ISDN_TYPE_NT);

		if (unitp->nttimer_t102_id)
			(void) untimeout(unitp->nttimer_t102_id);
		unitp->nttimer_t102_id = timeout(dbri_nttimer_t102,
		    unitp, TICKS(bs->i_var.t102));

		dbri_primitive_ph_di(as);

	} else {
		ATRACE(dbri_nttimer_t101, ' bad', unitp);
		ATRACE(dbri_nttimer_t101, ' tss', bs->sbri.tss);
		cmn_err(CE_WARN, "dbri: t101 state bogosity!");
	}
done:
	UNLOCK_AS(as);
} /* dbri_nttimer_t101 */


/*
 * dbri_nttimer_t102 - See Fascille III.8, Table 6/I.430
 */
void
dbri_nttimer_t102(void *arg)
{
	dbri_unit_t *unitp = arg;
	aud_stream_t *as;
	dbri_bri_status_t *bs;

	if (unitp == NULL)
		return;

	LOCK_UNIT(unitp);
	ATRACE(dbri_nttimer_t102, 'unit', unitp);

	as = DChanToAs(unitp, DBRI_NT_D_OUT);

	unitp->nttimer_t101_id = 0;
	if (!ISHWCONNECTED(as))
		goto done;

	unitp = AsToUnitp(as);
	bs = DBRI_NT_STATUS_P(unitp);

	if ((bs->sbri.tss == DBRI_NTINFO0_G1) ||
	    (bs->sbri.tss == DBRI_NTINFO2_G2) ||
	    (bs->sbri.tss == DBRI_NTINFO4_G3)) {
		/*EMPTY*/
		/* t2! is ignored */

	} else if ((bs->sbri.tss == DBRI_NTINFO0_G4)) {
		/*
		 * Next state is G1.
		 *
		 * DBRI doesn't implement G4, so it thinks that it is in G1
		 * right now because we have disabled the NT interface.
		 */
		bs->i_info.activation = ISDN_DEACTIVATED;
		bs->sbri.tss = DBRI_NTINFO0_G1;

		/*
		 * Remote TE can successfully request activation now.
		 */
		dbri_enable_nt(unitp);


	} else {
		ATRACE(dbri_nttimer_t102, ' bad', unitp);
		ATRACE(dbri_nttimer_t102, ' tss', bs->sbri.tss);
		cmn_err(CE_WARN, "dbri: t102 state bogosity!");
	}

done:
	UNLOCK_AS(as);
} /* dbri_nttimer_t102 */


/*
 * dbri_te_unplug - adjusts the state of the isdnstate variable to
 * reflect an unplugged condition as well as conditionally signals the
 * user program and disables the "permit-activation" bit if this has not
 * been previously done.
 */
void
dbri_te_unplug(dbri_unit_t *unitp)
{
	dbri_pipe_t *pipep;
	dbri_bri_status_t *bs;

	ATRACE(dbri_te_unplug, 'unit', unitp);

	pipep = &unitp->ptab[DBRI_PIPE_TE_D_IN];
	bs = DBRI_TE_STATUS_P(unitp);

	/*
	 * XXX - Need to have semaphore valid variable to check if
	 * structure is valid.
	 */
	if (pipep->ds == NULL)
		return;

	if (unitp->tetimer_id) {
		(void) untimeout(unitp->tetimer_id);
		unitp->tetimer_id = 0;
	}

	if (bs->i_info.activation != ISDN_UNPLUGGED) {
		/*
		 * Only signal upper layer when there is a change from
		 * active to not active.
		 *
		 * XXX - send deactivation message
		 */
		/*EMPTY*/
	}
	/* XXX - should be per-interface and not per-stream information */
	bs->i_info.activation = ISDN_UNPLUGGED;

	dbri_hold_f3(unitp);
} /* dbri_te_unplug */


/*
 * dbri_te_ph_activate_req - Activate TE
 */
void
dbri_te_ph_activate_req(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;
	aud_stream_t *tas;
	audtrace_hdr_t th;

	ASSERT_UNITLOCKED(unitp);
	bs = DBRI_TE_STATUS_P(unitp);

	as = as->output_as;	/* Use D-Channel */
	ATRACE(dbri_te_ph_activate_req, '  as', as);

	tas = DChanToAs(unitp, DBRI_TE_D_OUT);
	th.type = ISDN_PH_AR;
	th.seq = tas->control_as->sequence++;
	audio_trace_hdr(tas->control_as, &th);

	if ((bs->i_info.activation != ISDN_ACTIVATED) &&
	    (bs->i_info.activation != ISDN_ACTIVATE_REQ)) {
		/*
		 * Activate TE interface
		 *
		 * XXX - Should not do anything unless in F3
		 */
		dbri_exit_f3(unitp);

		bs->i_info.activation = ISDN_ACTIVATE_REQ;

		if (unitp->tetimer_id)
			(void) untimeout(unitp->tetimer_id);
		unitp->tetimer_id = timeout(dbri_te_timer, as,
		    TICKS(bs->i_var.t103));
	}
}


/*
 * Activate NT interface
 */
void
dbri_nt_ph_activate_req(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;
	audtrace_hdr_t th;
	aud_stream_t *tas;

	ASSERT_UNITLOCKED(unitp);
	bs = DBRI_NT_STATUS_P(unitp);

	as = as->output_as;	/* Use D-channel */
	ATRACE(dbri_nt_ph_activate_req, '  as', as);

	if ((bs->sbri.tss == DBRI_NTINFO2_G2) ||
	    (bs->sbri.tss == DBRI_NTINFO4_G3)) {
		/*
		 * Users of layer 1 are not supposed to request activation
		 * when layer 1 is in G2 or G3.
		 */
		ATRACE(dbri_nt_ph_activate_req, 'noop', bs->sbri.tss);
		return;
	}

	/*
	 * PH-AR is legal in G1 or G4, actions are the same exiting from
	 * either state.
	 */
	tas = DChanToAs(unitp, DBRI_NT_D_OUT);
	th.type = ISDN_PH_AR;
	th.seq = tas->control_as->sequence++;
	audio_trace_hdr(tas->control_as, &th);

	dbri_enable_nt(unitp);	/* In G4, interface is disabled */
	dbri_exit_g1(unitp);	/* Request activation */

	if (unitp->nttimer_t102_id) {
		/*
		 * Cancel deactivation timer.
		 * This is redundant with t102 handler code.
		 */
		(void) untimeout(unitp->nttimer_t102_id);
		unitp->nttimer_t102_id = 0;
	}
	if (unitp->nttimer_t101_id)
		(void) untimeout(unitp->nttimer_t101_id);
	unitp->nttimer_t101_id = timeout(dbri_nttimer_t101, unitp,
	    TICKS(bs->i_var.t101));
}


/*
 * dbri_nt_mph_deactivate_req - Deactivate NT
 */
void
dbri_nt_mph_deactivate_req(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;

	ASSERT_UNITLOCKED(unitp);
	bs = DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_nt_mph_deactivate_req, 'unit', unitp);

	if ((bs->sbri.tss == DBRI_NTINFO0_G1) ||
	    (bs->sbri.tss == DBRI_NTINFO0_G4)) {
		/*
		 * Users of layer 1 are not supposed to request deactivation
		 * when layer 1 is in G1 or G4.
		 */
		ATRACE(dbri_nt_mph_deactivate_req, 'noop', bs->sbri.tss);
		return;
	}

	/*
	 * Either pending activation or activated.  Begin
	 * deactivation sequence.
	 */
	bs->sbri.tss = DBRI_NTINFO0_G4;	/* DBRI "pseudo state */
	bs->i_info.activation = ISDN_DEACTIVATE_REQ;

	dbri_disable_nt(unitp);	/* Force DBRI into its "G1" state */
	dbri_hold_g1(unitp);	/* Stop requesting activation */

	if (unitp->nttimer_t101_id) {
		/*
		 * Cancel activation timer.
		 * This is redundant with t101 handler code.
		 */
		(void) untimeout(unitp->nttimer_t101_id);
		unitp->nttimer_t101_id = 0;
	}
	if (unitp->nttimer_t102_id)
		(void) untimeout(unitp->nttimer_t102_id);
	unitp->nttimer_t102_id = timeout(dbri_nttimer_t102, unitp,
	    TICKS(bs->i_var.t102));

	dbri_primitive_ph_di(DChanToAs(unitp, DBRI_NT_D_OUT));
}


/*
 * TE sends: ISDN_PH_AI, ISDN_PH_DI, ISDN_MPH_AI, ISDN_MPH_DI, ISDN_MPH_EI1,
 * 	ISDN_MPH_EI2, ISDN_MPH_II_C, ISDN_MPH_II_D
 * NT sends: ISDN_PH_AI, ISDN_PH_DI, ISDN_MPH_AI, ISDN_MPH_DI, MPH_EI
 */

/*
 * Send a CCITT PH or MPH indication upstream.
 */
static void
dbri_send_primitive(aud_stream_t *as, isdn_message_type_t message)
{
	dbri_stream_t *ds = AsToDs(as);
	isdn_message_t *ip;
	mblk_t *mp;
	audtrace_hdr_t th;

	ATRACE(dbri_send_primitive, '  as', as);

	if (as == NULL)
		return;

	as = as->control_as;

	if (as == NULL || !as->info.open || as->readq == NULL)
		return;

	th.seq = as->sequence++;
	th.type = message;
	audio_trace_hdr(as, &th);

	if ((mp = allocb(sizeof (isdn_message_t), BPRI_HI)) == NULL)
		return;

	mp->b_datap->db_type = M_PROTO;
	ip = (isdn_message_t *)(void *)mp->b_wptr;
	mp->b_wptr += sizeof (*ip);

	bzero((caddr_t)ip, sizeof (*ip));
	ip->magic = ISDN_PROTO_MAGIC;
	ip->type = ds->ctep->isdntype;
	ip->message = message;

	putnext(as->readq, mp);
}


/*
 * MPH-ACTIVATE Indication
 */
void
dbri_primitive_mph_ai(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_mph_ai, '  as', as);
	++bs->i_info.mph_ai;
	dbri_send_primitive(as, ISDN_MPH_AI);
}


/*
 * MPH-DEACTIVATE Indication
 */
void
dbri_primitive_mph_di(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_mph_di, '  as', as);
	++bs->i_info.mph_di;
	dbri_send_primitive(as, ISDN_MPH_DI);
}


void
dbri_primitive_mph_ei1(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_mph_ei1, '  as', as);
	++bs->i_info.mph_ei1;
	dbri_send_primitive(as, ISDN_MPH_EI1);
}


void
dbri_primitive_mph_ei2(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_mph_ei2, '  as', as);
	++bs->i_info.mph_ei2;
	dbri_send_primitive(as, ISDN_MPH_EI2);
}


void
dbri_primitive_mph_ii_c(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_mph_ii_c, '  as', as);
	++bs->i_info.mph_ii_c;
	dbri_send_primitive(as, ISDN_MPH_II_C);
}


void
dbri_primitive_mph_ii_d(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_mph_ii_d, '  as', as);
	++bs->i_info.mph_ii_d;
	dbri_send_primitive(as, ISDN_MPH_II_D);
}


void
dbri_primitive_ph_ai(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_ph_ai, '  as', as);
	++bs->i_info.ph_ai;
	dbri_send_primitive(as, ISDN_PH_AI);
}


void
dbri_primitive_ph_di(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_bri_status_t *bs;

	bs = (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) ?
	    DBRI_TE_STATUS_P(unitp) : DBRI_NT_STATUS_P(unitp);

	ATRACE(dbri_primitive_ph_di, '  as', as);
	++bs->i_info.ph_di;
	dbri_send_primitive(as, ISDN_PH_DI);
}


/*
 * dbri_hold_f3 - Do not exit F3 unless I2 or I4 is received.
 */
void
dbri_hold_f3(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t	cmd;

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_hold_f3, 'unit', unitp);

	bs = DBRI_TE_STATUS_P(unitp);

	if (!(bs->ntte_cmd & DBRI_NTE_ACT))
		return;

	bs->ntte_cmd &= ~(DBRI_NTE_ACT|DBRI_NTE_IRM_STATUS);

	cmd.opcode = bs->ntte_cmd;
	dbri_command(unitp, cmd);

	/*
	 * XXX - Must ensure that the command has been processed
	 */
}


/*
 * dbri_exit_f3 - Request activation.
 */
static void
dbri_exit_f3(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t	cmd;

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_exit_f3, 'unit', unitp);

	bs = DBRI_TE_STATUS_P(unitp);

	if (bs->ntte_cmd & DBRI_NTE_ACT)
		return;

	bs->ntte_cmd |= DBRI_NTE_ACT;
	bs->ntte_cmd |= DBRI_NTE_IRM_STATUS;

	cmd.opcode = bs->ntte_cmd;	/* TE command */
	dbri_command(unitp, cmd);

	/*
	 * XXX - Must ensure that the command has been processed
	 */
}


/*
 * dbri_hold_g1 - Stop requesting activation.
 */
void
dbri_hold_g1(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t	cmd;

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_hold_g1, 'unit', unitp);

	bs = DBRI_NT_STATUS_P(unitp);

	if ((bs->ntte_cmd & DBRI_NTE_ACT) == 0)
		return;

	bs->ntte_cmd &= ~(DBRI_NTE_ACT|DBRI_NTE_IRM_STATUS);

	cmd.opcode = bs->ntte_cmd;
	dbri_command(unitp, cmd);

	/*
	 * XXX - Must ensure that the command has been processed
	 */
}


void
dbri_exit_g1(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;
	dbri_chip_cmd_t	cmd;

	ASSERT_UNITLOCKED(unitp);
	ATRACE(dbri_exit_g1, 'unit', unitp);

	bs = DBRI_NT_STATUS_P(unitp);

	if (bs->ntte_cmd & DBRI_NTE_ACT)
		return;

	bs->ntte_cmd |= DBRI_NTE_ACT;
	bs->ntte_cmd &= ~DBRI_NTE_IRM_STATUS;

	cmd.opcode = bs->ntte_cmd; 		/* NT command */
	dbri_command(unitp, cmd);

	/*
	 * XXX - Must ensure that the command has been processed
	 */
}


#define	IOCEQ(X)  (as == DChanToAs(unitp, (X))->output_as)
#define	IOCEQMGT(X)  (as == DChanToAs(unitp, (X))->control_as)

/*
 * dbri_isdn_set_param - handles the ISDN_PARAM_SET ioctl
 */
boolean_t
dbri_isdn_set_param(aud_stream_t *as, isdn_param_t *iphp,
	aud_return_t *changep, int *error)
{
	dbri_unit_t *unitp;
	dbri_bri_status_t *bs;
	isdn_interface_t bri_type;

	unitp = AsToUnitp(as);
	*error = 0;

	switch (iphp->tag) {
	case ISDN_PARAM_NT_T101:
		if (IOCEQ(DBRI_NT_D_OUT) || IOCEQMGT(DBRI_NTMGT)) {
			bs = DBRI_NT_STATUS_P(unitp);

			bs->i_var.t101 = iphp->value.ms;
			*changep = AUDRETURN_CHANGE;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_NT_T102:
		if (IOCEQ(DBRI_NT_D_OUT) || IOCEQMGT(DBRI_NTMGT)) {
			bs = DBRI_NT_STATUS_P(unitp);

			bs->i_var.t102 = iphp->value.ms;
			*changep = AUDRETURN_CHANGE;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_TE_T103:
		if (IOCEQ(DBRI_TE_D_OUT) || IOCEQMGT(DBRI_TEMGT)) {
			bs = DBRI_TE_STATUS_P(unitp);

			bs->i_var.t103 = iphp->value.ms;
			*changep = AUDRETURN_CHANGE;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_TE_T104:
		if (IOCEQ(DBRI_TE_D_OUT) || IOCEQMGT(DBRI_TEMGT)) {
			bs = DBRI_TE_STATUS_P(unitp);

			bs->i_var.t104 = iphp->value.ms;
			*changep = AUDRETURN_CHANGE;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_MAINT:
		if (IOCEQMGT(DBRI_TEMGT))
			bs = DBRI_TE_STATUS_P(unitp);
		else if (IOCEQMGT(DBRI_NTMGT))
			bs = DBRI_NT_STATUS_P(unitp);
		else {
			*error = EINVAL;
			return (B_FALSE);
		}

		switch (iphp->value.maint) {
		case ISDN_PARAM_MAINT_OFF:
		case ISDN_PARAM_MAINT_ECHO:
		case ISDN_PARAM_MAINT_ON:
			bs->i_var.maint = iphp->value.maint;
			*changep = AUDRETURN_CHANGE; /* XXX - need signal? */
			break;

		default:
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_ASMB:
		/*
		 * If optional behaviours are allowed for the NT side,
		 * then add more conditionals here.
		 */

		if (!IOCEQMGT(DBRI_TEMGT)) {
			*error = EINVAL;
			return (B_FALSE);
		}
		bs = DBRI_TE_STATUS_P(unitp);

		switch (iphp->value.asmb) {
		case ISDN_PARAM_TE_ASMB_CCITT88:
		case ISDN_PARAM_TE_ASMB_CTS2:
			bs->i_var.asmb = iphp->value.asmb;
			*changep = AUDRETURN_CHANGE;
			break;

		default:
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_POWER:
		if (!IOCEQMGT(DBRI_TEMGT) && !IOCEQMGT(DBRI_NTMGT)) {
			*error = EINVAL;
			return (B_FALSE);
		}

		if (IOCEQMGT(DBRI_TEMGT)) {
			bri_type = ISDN_TYPE_TE;
			bs = DBRI_TE_STATUS_P(unitp);
		} else {
			bri_type = ISDN_TYPE_NT;
			bs = DBRI_NT_STATUS_P(unitp);
		}

		switch (iphp->value.asmb) {
		case ISDN_PARAM_POWER_OFF:
		case ISDN_PARAM_POWER_ON:
			if (bs->i_var.power != iphp->value.flag) {
				(void) dbri_power(unitp, bri_type,
				    iphp->value.flag);
				*changep = AUDRETURN_CHANGE;
			}
			break;

		default:
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_PAUSE:
	{
		dbri_chan_t dchan;

		dchan = dbri_itod_chan(iphp->value.pause.channel,
		    DBRI_DIRECTION_OUT);

		switch (dchan) {
		case DBRI_NT_B1_OUT:
		case DBRI_NT_B2_OUT:
			if (!IOCEQMGT(DBRI_DBRIMGT) && !IOCEQMGT(DBRI_NTMGT))
				as = NULL;
			else
				as = DChanToAs(unitp, dchan)->output_as;
			break;

		case DBRI_TE_B1_OUT:
		case DBRI_TE_B2_OUT:
			if (!IOCEQMGT(DBRI_DBRIMGT) && !IOCEQMGT(DBRI_TEMGT))
				as = NULL;
			else
				as = DChanToAs(unitp, dchan)->output_as;
			break;

		default:
			as = NULL;
		}

		if (as == NULL) {
			*error = EINVAL;
			return (B_FALSE);
		}

		if (iphp->value.pause.paused) {
			audio_pause_record(as);
			audio_pause_play(as);
		} else {
			audio_resume_record(as);
			audio_resume_play(as);
		}
		break;
	}

	case DBRI_PARAM_FORCE_ACTIVATION:
		if (IOCEQMGT(DBRI_TEMGT)) {
			bri_type = ISDN_TYPE_TE;
		} else if (IOCEQMGT(DBRI_NTMGT)) {
			bri_type = ISDN_TYPE_NT;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}

		switch (iphp->value.flag) {
		case B_TRUE:
		case B_FALSE:
			(void) dbri_forceactivation(unitp, bri_type,
			    iphp->value.flag);
				*changep = AUDRETURN_CHANGE;
			break;

		default:
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case DBRI_PARAM_F:
		if (IOCEQMGT(DBRI_TEMGT)) {
			bri_type = ISDN_TYPE_TE;
		} else if (IOCEQMGT(DBRI_NTMGT)) {
			bri_type = ISDN_TYPE_NT;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}

		switch (iphp->value.flag) {
		case B_TRUE:
		case B_FALSE:
			(void) dbri_set_f(unitp, bri_type,
			    iphp->value.flag);
				*changep = AUDRETURN_CHANGE;
			break;

		default:
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case DBRI_PARAM_NBF:
		if (IOCEQMGT(DBRI_TEMGT)) {
			bri_type = ISDN_TYPE_TE;
		} else if (IOCEQMGT(DBRI_NTMGT)) {
			bri_type = ISDN_TYPE_NT;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}

		switch (iphp->value.count) {
		case 2:
		case 3:
			(void) dbri_set_nbf(unitp, bri_type, iphp->value.count);
			*changep = AUDRETURN_CHANGE;
			break;

		default:
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	default:
		*error = EINVAL;
		return (B_FALSE);
	} /* switch on parameter tag */

	return (B_TRUE);
}


/*
 * dbri_isdn_get_param - handles the ISDN_PARAM_GET ioctl
 */
/*ARGSUSED*/
boolean_t
dbri_isdn_get_param(aud_stream_t *as, isdn_param_t *iphp,
	aud_return_t *changep, int *error)
{
	dbri_bri_status_t *bs;
	dbri_unit_t *unitp;

	*error = 0;

	unitp = AsToUnitp(as);

	switch (iphp->tag) {
	case ISDN_PARAM_NT_T101:
		if (IOCEQ(DBRI_NT_D_OUT) || IOCEQMGT(DBRI_NTMGT)) {
			bs = DBRI_NT_STATUS_P(unitp);
			iphp->value.ms = bs->i_var.t101;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_NT_T102:
		if (IOCEQ(DBRI_NT_D_OUT) || IOCEQMGT(DBRI_NTMGT)) {
			bs = DBRI_NT_STATUS_P(unitp);
			iphp->value.ms = bs->i_var.t102;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_TE_T103:
		if (IOCEQ(DBRI_TE_D_OUT) || IOCEQMGT(DBRI_TEMGT)) {
			bs = DBRI_TE_STATUS_P(unitp);
			iphp->value.ms = bs->i_var.t103;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_TE_T104:
		if (IOCEQ(DBRI_TE_D_OUT) || IOCEQMGT(DBRI_TEMGT)) {
			bs = DBRI_TE_STATUS_P(unitp);
			iphp->value.ms = bs->i_var.t104;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_MAINT:
		if (IOCEQMGT(DBRI_TEMGT) || IOCEQMGT(DBRI_NTMGT)) {
			if (IOCEQMGT(DBRI_TEMGT))
				bs = DBRI_TE_STATUS_P(unitp);
			else
				bs = DBRI_NT_STATUS_P(unitp);
			iphp->value.maint = bs->i_var.maint;
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}
		break;

	case ISDN_PARAM_ASMB:
		if (!IOCEQMGT(DBRI_TEMGT)) {
			*error = EINVAL;
			return (B_FALSE);
		}

		bs = DBRI_TE_STATUS_P(unitp);

		iphp->value.asmb = bs->i_var.asmb;
		break;

	case ISDN_PARAM_POWER:
		if (!IOCEQMGT(DBRI_TEMGT) && !IOCEQMGT(DBRI_NTMGT)) {
			*error = EINVAL;
			return (B_FALSE);
		}

		if (IOCEQMGT(DBRI_TEMGT))
			bs = DBRI_TE_STATUS_P(unitp);
		else
			bs = DBRI_NT_STATUS_P(unitp);

		iphp->value.flag = bs->i_var.power;
		break;

	case ISDN_PARAM_PAUSE:
		{
		dbri_chan_t chan;

		chan = dbri_itod_chan(iphp->value.pause.channel,
		    DBRI_DIRECTION_OUT);

		switch (chan) {
		case DBRI_NT_B1_OUT:
		case DBRI_NT_B2_OUT:
			if (IOCEQMGT(DBRI_DBRIMGT) || IOCEQMGT(DBRI_NTMGT)) {
				as = DChanToAs(unitp, chan)->output_as;
			} else {
				*error = EINVAL;
				return (B_FALSE);
			}
			break;

		case DBRI_TE_B1_OUT:
		case DBRI_TE_B2_OUT:
			if (IOCEQMGT(DBRI_DBRIMGT) || IOCEQMGT(DBRI_TEMGT)) {
				as = DChanToAs(unitp, chan)->output_as;
			} else {
				*error = EINVAL;
				return (B_FALSE);
			}
			break;

		default:
			*error = EINVAL;
			return (B_FALSE);
		}
		}

		/* XXX */
		if (as->output_as->info.pause != as->input_as->info.pause)
			cmn_err(CE_WARN, "ISDN pause play != record");

		iphp->value.pause.paused = as->info.pause;
		break;

	case DBRI_PARAM_FORCE_ACTIVATION:
		if (IOCEQMGT(DBRI_TEMGT)) {
			bs = DBRI_TE_STATUS_P(unitp);
		} else if (IOCEQMGT(DBRI_NTMGT)) {
			bs = DBRI_NT_STATUS_P(unitp);
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}

		iphp->value.flag = bs->fact;
		break;

	case DBRI_PARAM_F:
		if (IOCEQMGT(DBRI_TEMGT)) {
			bs = DBRI_TE_STATUS_P(unitp);
		} else if (IOCEQMGT(DBRI_NTMGT)) {
			bs = DBRI_NT_STATUS_P(unitp);
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}

		iphp->value.flag = bs->dbri_f;
		break;

	case DBRI_PARAM_NBF:
		if (IOCEQMGT(DBRI_TEMGT)) {
			bs = DBRI_TE_STATUS_P(unitp);
		} else if (IOCEQMGT(DBRI_NTMGT)) {
			bs = DBRI_NT_STATUS_P(unitp);
		} else {
			*error = EINVAL;
			return (B_FALSE);
		}

		iphp->value.flag = bs->dbri_nbf;
		break;

	default:
		*error = EINVAL;
		return (B_FALSE);
	} /* switch on parameter tag */

	/*
	 * If we get here, there was not an error, the caller will
	 * perform the copyout.
	 */
	return (B_TRUE);
}

#undef IOCEQ
