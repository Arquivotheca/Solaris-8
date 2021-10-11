/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dbri_subr.c	1.47	97/10/31 SMI"

/*
 * Sun DBRI subroutines
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
 * Reset the DBRI and MMCODEC chips to a known safe state.  Chip needs to
 * be in a fully reset state. This is currently called at attach time and
 * unload time. When called from attach the streams have just been setup
 * but nothing is connected. At UNLOAD time the chip could really be in
 * any state.
 */
void
dbri_initchip(dbri_unit_t *unitp)
{
	dbri_reg_t	*regsp = unitp->regsp;
	dbri_intq_t	*qp, *nqp;
	int		i;
	uint_t		pipe;

	ASSERT_UNITLOCKED(unitp);

	regsp->n.sts = DBRI_STS_R;		/* reset DBRI chip */
	unitp->initialized = B_FALSE;

	/*
	 * Kill off all timers
	 */
	dbri_remove_timeouts(unitp);

	/*
	 * Clear out structures.  This also intializes the command
	 * queue to all DBRI WAIT's since the WAIT opcode is 0
	 */
	bzero(unitp->intq.intq_bp,
	    DBRI_NUM_INTQS * sizeof (dbri_intq_t));
	bzero(unitp->cmdqp, sizeof (dbri_cmdq_t));

	/*
	 * Circularly chain the interrupt queues and set status pointers
	 */
	qp = unitp->intq.intq_bp;
	for (i = 0; i < (DBRI_NUM_INTQS - 1); ++i, qp++) {
		nqp = qp + 1;	/* can't do this calc in DBRI_ADDR */
		qp->nextq = (caddr32_t)DBRI_IOPBIOADDR(unitp, nqp);
	}

	/* last queue points back to first */
	qp->nextq = (caddr32_t)DBRI_IOPBIOADDR(unitp, unitp->intq.intq_bp);

	unitp->intq.curqp = unitp->intq.intq_bp;
	unitp->intq.off = 0;

	/*
	 * Now set status pointers for the command queue
	 */
	unitp->cmdqp->cmdhp = 0;
	unitp->init_cmdq = B_FALSE;

	/*
	 * Initialize the pipe table
	 */
	bzero(unitp->ptab, sizeof (unitp->ptab));
	for (pipe = 0; pipe < DBRI_MAX_PIPES; ++pipe) {
		dbri_pipe_t	*pipep = &unitp->ptab[pipe];

		pipep->ds = NULL;
		pipep->pipeno = pipe;
		pipep->mode = 0;	/* XXX ? */

		/*
		 * Software copies of DBRI commands
		 */
		pipep->sdp = 0;
		pipep->ssp = 0;
		pipep->dts.opcode.word32 = 0;
		pipep->dts.input_tsd.word32 = 0;
		pipep->dts.output_tsd.word32 = 0;

		pipep->ismonitor = B_FALSE;
		pipep->otherpipep = NULL;
		pipep->monitorpipep = NULL;

		pipep->sink.dchan = DBRI_CHAN_NONE;
		pipep->source.dchan = DBRI_CHAN_NONE;
		pipep->sink.basepipep = NULL;
		pipep->source.basepipep = NULL;

		pipep->refcnt = 0;
		pipep->allocated = B_FALSE;
	}

	for (i = 0; i < 10; i++) { /* loop up to 2.5ms */
		drv_usecwait(250); /* don't bog down bus */
		if (!(regsp->n.sts & DBRI_STS_R)) /* brk on reset done */
			break;
	}
	if (regsp->n.sts & DBRI_STS_R) {
		cmn_err(CE_WARN, "dbri: unable to reset chip");
		return;
	}

	mmcodec_reset(unitp);

	unitp->initialized = B_TRUE;

	{
		dbri_chip_cmd_t	cmd;

		cmd.opcode = DBRI_CMD_IIQ;
		cmd.arg[0] = (caddr32_t)DBRI_IOPBIOADDR(unitp,
		    unitp->intq.curqp);
		dbri_command(unitp, cmd); /* Enable interrupts */
	}

	/*
	 * Configure the size of DBRI's DMA bursts
	 */
	if (unitp->ddi_burstsizes_avail & 0x10)
		regsp->n.sts |= DBRI_STS_G;
	if (unitp->ddi_burstsizes_avail & 0x20)
		regsp->n.sts |= DBRI_STS_E;
#if 0 /* XXX - DO NOT enable 16-word bursts on current DBRI!! */
	if (unitp->ddi_burstsizes_avail & 0x40)
		regsp->n.sts |= DBRI_STS_S;
#endif
} /* dbri_initchip */


/*
 * dbri_is_dbri_audio_chan - XXX
 */
boolean_t
dbri_is_dbri_audio_chan(dbri_chan_t dchan)
{
	switch (dchan) {
	case DBRI_AUDIO_OUT:
	case DBRI_AUDIO_IN:
		return (B_TRUE);
	default:
		return (B_FALSE);
	}
} /* dbri_is_dbri_audio_chan */


/*
 * dbri_cmd_info - a table indexed by command opcode which contains
 * the length of the command in words and the mnemonic for the opcode.
 */
static struct {
	int length;
	char *name;
} dbri_cmd_info[16] = {
	{-2, "WAIT"},
	{1, "PAUSE"},
	{2, "JMP"},
	{2, "IIQ"},
	{1, "REX"},
	{2, "SDP"},
	{1, "CDP"},
	{3, "DTS"},
	{2, "SSP"},
	{1, "CHI"},
	{1, "NT"},
	{1, "TE"},
	{1, "CDEC"},
	{-1, "TEST"},	/* SBus TEST/COPY is the only 3 word test command */
	{1, "CDM"},
	{-2, "undefined"}
};


void
dbri_command(dbri_unit_t *unitp, dbri_chip_cmd_t cmd)
{
	uint_t unprocessed;
	volatile dbri_cmdq_t *cmdq;
	int command_length;
	uint_t dbri_pc;
	uint_t *reg_cmdqp;
	uint_t tmp_cmdp;

	ASSERT(DBRI_OP(cmd.opcode) < 16);

	ASSERT_UNITLOCKED(unitp);

	cmdq = unitp->cmdqp;	/* base of cmdq */

	/*
	 * Protect this routine if the chip has not initialized
	 * successfully.
	 */
	if (!unitp->initialized)
		return;

	command_length = dbri_cmd_info[DBRI_OP(cmd.opcode)].length;

#if defined(DEBUG) && defined(AUDIOTRACE)
	/*
	 * XXX - remove for production
	 */
	if (command_length == -2) {
		extern void call_debug();

		call_debug("caller tried to issue illegal command");
	}
#endif

	if (command_length == -1) {
		/*
		 * Handle any special cases here.  The only current
		 * special case is the TEST command which has either one
		 * or two arguments based on the test type.
		 */
		switch (DBRI_TST_TYPE(cmd.opcode)) {
		case DBRI_TST_SBUS:
			command_length = 3;
			break;

		default:
			command_length = 2;
			break;
		}
	}

	/*
	 * Get command queue information
	 */
	if (!unitp->init_cmdq || unitp->suspended) {
		dbri_pc = 0;	/* Not initialized yet, it will be at 0 */
	} else {
		reg_cmdqp = (uint_t *)
		    DBRI_IOPBKADDR(unitp, unitp->regsp->n.cmdqp);
		dbri_pc = (caddr32_t)(reg_cmdqp - &cmdq->cmd[0]);
		if (dbri_pc > DBRI_MAX_CMDS) {
			ATRACE(dbri_command, 'PC  ', dbri_pc);
			dbri_panic(unitp, "DBRI Reg8 bad!\n");
			return;
		}
	}

	/*
	 * Calculate space at end of queue. cmdq->cmdhp always points to WAIT
	 */
	unprocessed = (cmdq->cmdhp >= dbri_pc)
	    ? (cmdq->cmdhp - dbri_pc) :
	    (DBRI_MAX_CMDS - (dbri_pc - cmdq->cmdhp));

	/*
	 * If the command queue is almost full, DBRI has not been processing
	 * commands and this indicates a serious problem.
	 *
	 * XXX - bad test
	 */
	if (unprocessed > (DBRI_MAX_CMDS - 10 /* XXX - magic number */)) {
		cmn_err(CE_WARN, "dbri: no space in command queue");
		return;		/* XXX - may want to return error here */
	}

	/*
	 * If at end of command queue, insert a JMP command, to wrap
	 * around to head of the command queue, followed by a WAIT
	 * command.
	 */
	if ((cmdq->cmdhp + command_length) >
	    (DBRI_MAX_CMDS - DBRI_CMD_JMP_LEN)) {
		cmdq->cmd[0] = DBRI_CMD_WAIT; /* jump to WAIT */

		/*
		 * Stuff the JMP command into the current word and 1 more
		 */
		cmdq->cmd[cmdq->cmdhp + 1] = (caddr32_t)
		    DBRI_IOPBIOADDR(unitp, &cmdq->cmd[0]);
		cmdq->cmd[cmdq->cmdhp] = DBRI_CMD_JMP;

		/*
		 * Point back to head of list
		 */
		cmdq->cmdhp = 0;
	}

#if 0 /* XXX */
	cmd.opcode |= DBRI_CMDI;	/* Per-command interrupt */
#endif

	/*
	 * Write the command plus a WAIT command into the queue.  Insert
	 * commands last to first to avoid race with DBRI.
	 */
	switch (command_length) {
	case 1:
		DTRACE(dbri_command, 'opcd', cmd.opcode);
		if (DBRI_OP(cmd.opcode) == DBRI_OP(DBRI_CMD_WAIT)) {
			/* Don't advance pointer on WAIT command. */
			cmdq->cmd[cmdq->cmdhp] = cmd.opcode;
		} else {
			tmp_cmdp = cmdq->cmdhp + 1;
			cmdq->cmdhp = tmp_cmdp;
			cmdq->cmd[tmp_cmdp] = DBRI_CMD_WAIT;
			cmdq->cmd[--tmp_cmdp] = cmd.opcode;
		}
		(void) DBRI_SYNC_CMDQ(unitp, 1, DDI_DMA_SYNC_FORDEV);
		break;

	case 2:
		DTRACE(dbri_command, 'opcd', cmd.opcode);
		DTRACE(dbri_command, 'arg0', cmd.arg[0]);
		tmp_cmdp = cmdq->cmdhp + 2;
		cmdq->cmdhp = tmp_cmdp;
		cmdq->cmd[tmp_cmdp] = DBRI_CMD_WAIT;
		cmdq->cmd[--tmp_cmdp] = cmd.arg[0];
		cmdq->cmd[--tmp_cmdp] = cmd.opcode;
		(void) DBRI_SYNC_CMDQ(unitp, 2, DDI_DMA_SYNC_FORDEV);
		break;

	case 3:
		DTRACE(dbri_command, 'opcd', cmd.opcode);
		DTRACE(dbri_command, 'arg0', cmd.arg[0]);
		DTRACE(dbri_command, 'arg1', cmd.arg[1]);
		tmp_cmdp = cmdq->cmdhp + 3;
		cmdq->cmdhp = tmp_cmdp;
		cmdq->cmd[tmp_cmdp] = DBRI_CMD_WAIT;
		cmdq->cmd[--tmp_cmdp] = cmd.arg[1];
		cmdq->cmd[--tmp_cmdp] = cmd.arg[0];
		cmdq->cmd[--tmp_cmdp] = cmd.opcode;
		(void) DBRI_SYNC_CMDQ(unitp, 3, DDI_DMA_SYNC_FORDEV);
		break;

	default:
		dbri_panic(unitp, "illegal dbri_command");
		return;
	} /* switch on command size */

	if (!unitp->init_cmdq) {
		unitp->regsp->n.cmdqp = (caddr32_t)
		    DBRI_IOPBIOADDR(unitp, &cmdq->cmd[0]);
		unitp->init_cmdq = B_TRUE;
	} else {
		unitp->regsp->n.sts |= DBRI_STS_P;	/* Queue ptr valid */
	}
} /* dbri_command */


/*
 * dbri_chan_activated - Checks that the base channel is activated
 *
 * XXX - May have to be expanded to check for CHI being active.
 */
boolean_t
dbri_chan_activated(dbri_stream_t *ds, dbri_chan_t sink, dbri_chan_t source)
{
	dbri_unit_t *unitp;
	dbri_bri_status_t *bs;
	dbri_pipe_t *bpipep;

	unitp = DsToUnitp(ds);
	ASSERT_UNITLOCKED(unitp);

	/*
	 * Grab mode of channel not connected to host from table
	 */
	if ((sink != DBRI_CHAN_HOST) && (sink != DBRI_CHAN_NONE)) {
		bpipep = dbri_basepipe(unitp, sink);
	} else if ((source != DBRI_CHAN_HOST) && (source != DBRI_CHAN_NONE)) {
		bpipep = dbri_basepipe(unitp, source);
	} else {
		ATRACE(dbri_chan_activated, '!chn', 0);
		return (B_FALSE);
	}

	switch (PIPENO(bpipep)) {
	case DBRI_PIPE_TE_D_IN:
	case DBRI_PIPE_TE_D_OUT:
		bs = DBRI_TE_STATUS_P(unitp);
		if (bs->i_info.activation != ISDN_ACTIVATED) {
			ATRACE(dbri_chan_activated, 'act-', PIPENO(bpipep));
			return (B_FALSE);
		}
		return (B_TRUE);

	case DBRI_PIPE_NT_D_IN:
	case DBRI_PIPE_NT_D_OUT:
		bs = DBRI_NT_STATUS_P(unitp);
		if (bs->i_info.activation != ISDN_ACTIVATED) {
			ATRACE(dbri_chan_activated, 'act-', PIPENO(bpipep));
			return (B_FALSE);
		}
		return (B_TRUE);

	default:		/* This really means CHI */
		/*
		 * Assume that CHI is always "activated"
		 */
		return (B_TRUE);
	}
} /* dbri_chan_activated */


/*
 * dbri_keep_alive - keep alive timer routine that reads reg0 every 4
 * seconds to insure that there is activity on DBRI.  This prevents the
 * relays from bypassing the host.
 */
void
dbri_keep_alive(void *arg)
{
	dbri_unit_t *unitp = arg;
	/*LINTED this is a register we need only read*/
	uint_t tmp;

	LOCK_UNIT(unitp);


	/*
	 * Restart keep alive timer.  Don't do anything if the keepalive
	 * timer has been turned off or this device has been disabled.
	 * Don't need to check for suspended here, as openinhibit is also
	 * set when we're suspended.
	 */
	if (unitp->keep_alive_running && !unitp->openinhibit) {
		tmp = unitp->regsp->n.sts; /* dummy read to keep chip alive */

		unitp->keepalive_id = timeout(dbri_keep_alive, unitp,
		    TICKS(Keepalive_timer));
	} else {
		unitp->keepalive_id = 0;
	}

	UNLOCK_UNIT(unitp);
} /* dbri_keep_alive */


/*
 * dbri_panic - Print out a message and either cause a system panic or
 * deactivate the current DBRI unit until the next time the system is
 * booted/unitp is reloaded.
 */
void
dbri_panic(dbri_unit_t *unitp, const char *message)
{
	dbri_reg_t *regsp = unitp->regsp;
	int i;

	ASSERT_UNITLOCKED(unitp);

	/*
	 * Prevent new opens on the device
	 */
	unitp->openinhibit = B_TRUE;

	/*
	 * Reset the chip and disallow it to become an SBus master
	 */
	regsp->n.sts = DBRI_STS_R; /* reset DBRI chip */
	drv_usecwait(2);	/* XXX - For safety? */
	regsp->n.sts |= (DBRI_STS_F|DBRI_STS_D);

	if (Dbri_panic) {
		cmn_err(CE_PANIC, "dbri: %s\n", message);
		/*NOTREACHED*/
	}

	cmn_err(CE_WARN, "dbri: %s", message);

	/*
	 * Send M_ERROR messages up all streams associated with this
	 * device.
	 */
	for (i = 0; i < Dbri_nstreams; i++) {
		dbri_stream_t *ds;

		ds = AsToDs(DChanToAs(unitp, i)->output_as);
		if (DsDisabled(ds))
			continue;

		dbri_error_msg(ds, M_ERROR);
	}

	/*
	 * Device will no longer be used by this instance of the device
	 *
	 * XXX - Is this all we need to do?
	 */
}


/*
 * dbri_config_queue - Set the high and low water marks for a queue
 *
 * XXX - Needs testing, code review
 */
void
dbri_config_queue(dbri_stream_t *ds)
{
	size_t hiwater, lowater;
	size_t onesec;
	aud_stream_t *as;

	ASSERT(ds != NULL);
	ASSERT_ASLOCKED(DsToAs(ds));

	as = DsToAs(ds);

	/*
	 * Configure an output stream
	 */
	if (as == as->output_as) {
		/*
		 * If the write queue is not open, then just return
		 */
		if (as->writeq == NULL)
			return;

		onesec = (as->info.sample_rate * as->info.channels *
		    as->info.precision) / 8;

		hiwater = onesec * 3;
		hiwater = min(hiwater, 80000);
		lowater = hiwater * 2 / 3;

		/*
		 * Tweak the high and low water marks based on throughput
		 * expectations.
		 */
		freezestr(as->writeq);
		(void) strqset(as->writeq, QHIWAT, 0, hiwater);
		(void) strqset(as->writeq, QLOWAT, 0, lowater);
		unfreezestr(as->writeq);
	}
}


/*
 * dbri_error_msg - Send the specified message up the specified stream.
 *
 * Argument as always points to the write side audio stream (XXX - really?)
 */
void
dbri_error_msg(dbri_stream_t *ds, uchar_t msgtype)
{
	mblk_t *mp;

	ASSERT_ASLOCKED(DsToAs(ds));

	/*
	 * If stream is not open, simply return
	 */
	if (ds == NULL || DsToAs(ds)->readq == NULL)
		return;

	/*
	 * Initialize a message to send a SIGPOLL upstream
	 */
	if ((mp = allocb(sizeof (char), BPRI_HI)) == NULL)
		return;

	mp->b_datap->db_type = msgtype;
	*mp->b_wptr++ = SIGPOLL;

	/*
	 * Signal the specified stream
	 */

	UNLOCK_AS(DsToAs(ds));	/* XXXXXX */
	putnext(DsToAs(ds)->readq, mp);
	LOCK_AS(DsToAs(ds));	/* XXXXXX */
} /* dbri_error_msg */


/*
 * dbri_power_subr - XXX
 */
int
dbri_power_subr(dbri_stream_t *ds, void *arg)
{
	dbri_unit_t *unitp;
	dbri_bri_status_t *bs;
	uint_t power;

	unitp = DsToUnitp(ds);

	switch (ds->ctep->isdntype) {
	case ISDN_TYPE_TE:
		bs = DBRI_TE_STATUS_P(unitp);
		break;

	case ISDN_TYPE_NT:
		bs = DBRI_TE_STATUS_P(unitp);
		break;

	default:
		return (0);
	}

	power = (uint_t)arg;
	bs->i_var.power = power;

	if ((power == ISDN_PARAM_POWER_OFF) &&
	    !ISCONTROLSTREAM(DsToAs(ds))) {
		/*
		 * Send an M_HANGUP message upstream
		 */
		dbri_error_msg(ds, M_HANGUP); /* XXX */
	} else {
		dbri_error_msg(ds, M_UNHANGUP);	/* XXX */
	}
	/* XXX - For power-up case, should M_HANGUP be sent? */

	return (0);
} /* dbri_power_subr */


#define	IOCEQ(CH) (ds == AsToDs(unitp->ioc[(CH)].as.output_as))

/*
 * Turn interface "power" on or off.
 *
 * XXX - For the NT, should this routine control the relay?
 */
int
dbri_power(dbri_unit_t *unitp, isdn_interface_t bri_type, uint_t power_state)
{
	dbri_bri_status_t *bs;

	switch (bri_type) {
	case ISDN_TYPE_TE:
		bs = DBRI_TE_STATUS_P(unitp);
		break;

	case ISDN_TYPE_NT:
		bs = DBRI_NT_STATUS_P(unitp);
		break;

	default:
		return (0);
	}

	/*
	 * Check for invalid args or redundant work
	 */
	if (power_state != ISDN_PARAM_POWER_OFF &&
	    power_state != ISDN_PARAM_POWER_ON)
		return (0);

	if (bs->i_var.power == power_state)
		return (1);

	/*
	 * State has changed
	 */
	bs->i_var.power = power_state;

	ATRACE(dbri_power, 'juce', power_state);

	if (power_state == ISDN_PARAM_POWER_OFF)
		dbri_bri_down(unitp, bri_type);

	if (power_state == ISDN_PARAM_POWER_ON) {
		if (bri_type == ISDN_TYPE_NT)
			dbri_enable_nt(unitp);
		else
			dbri_enable_te(unitp);
	}

	(void) dbri_bri_func(unitp, bri_type, dbri_power_subr,
	    (void *)power_state);

	if (power_state == ISDN_PARAM_POWER_ON)
		dbri_bri_up(unitp, bri_type);

	return (1);
} /* dbri_power */

#undef IOCEQ


/*
 * An interface is being brought up or down, perform the appropriate
 * actions on an aud_stream on that interface.
 *
 * If the interface is being brought down (arg == 0), stop IO on the pipe
 * and flush all pending output.
 *
 * If the interface is coming up, reset the eol state and queue record
 * buffers.
 */
/*ARGSUSED*/
int
dbri_bri_up_down_subr(dbri_stream_t *ds, void *arg)
{
	int up = (int)arg;

	if (up) {
		if (DsToAs(ds) == DsToAs(ds)->input_as) {
			ATRACE(dbri_bri_up, 'rego', ds);
			audio_process_input(DsToAs(ds));
		}
	} else {
		ATRACE(dbri_bri_down, 'bri-', ds);
		dbri_stop(DsToAs(ds));
		audio_flush_cmdlist(DsToAs(ds));
	}
	return (0);
} /* dbri_bri_up_down_subr */


/*
 * Flush all pipes associated with a TE or NT.
 *
 * XXX - Should we flush output pipes only? Abort input pipes?
 */
void
dbri_bri_down(dbri_unit_t *unitp, isdn_interface_t bri_type)
{
	if (bri_type != ISDN_TYPE_TE && bri_type != ISDN_TYPE_NT)
		return;

	(void) dbri_bri_func(unitp, bri_type, dbri_bri_up_down_subr,
	    (void *)0);
} /* dbri_bri_down */


/*
 * Restart all receive pipes associated with a TE or NT.
 */
void
dbri_bri_up(dbri_unit_t *unitp, isdn_interface_t bri_type)
{
	ATRACE(dbri_bri_up, 'bri+', bri_type);

	if (bri_type != ISDN_TYPE_TE && bri_type != ISDN_TYPE_NT)
		return;

	(void) dbri_bri_func(unitp, bri_type, dbri_bri_up_down_subr,
	    (void *)1);
} /* dbri_bri_down */


/*
 * Run a function on all open BRI aud_streams. Return code is OR of all
 * return codes.
 */
int
dbri_bri_func(dbri_unit_t *unitp, isdn_interface_t bri_type, int (*func)(),
	void *arg)
{
	int	i;
	int	r = 0;

	if (bri_type != ISDN_TYPE_TE && bri_type != ISDN_TYPE_NT)
		return (r);

	for (i = 0; i < Dbri_nstreams; i++) {
		aud_stream_t *as;

		if (bri_type != unitp->ioc[i].ctep->isdntype)
			continue;

		as = DChanToAs(unitp, i);
		if (DsDisabled(AsToDs(as)))
			continue;

		if (!ISHWCONNECTED(as))
			continue;
		r |= func(as, arg);
	}
	return (r);
} /* dbri_bri_func */


/*
 * dbri_enable_te - XXX
 */
void
dbri_enable_te(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;

	bs = DBRI_TE_STATUS_P(unitp);

	bs->i_var.enable = B_TRUE;
	if (bs->i_var.power == ISDN_PARAM_POWER_ON)
		unitp->regsp->n.sts |= DBRI_STS_T;
} /* dbri_enable_te */


/*
 * dbri_disable_te - XXX
 */
void
dbri_disable_te(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;

	bs = DBRI_TE_STATUS_P(unitp);

	bs->i_var.enable = B_FALSE;
	unitp->regsp->n.sts &= ~DBRI_STS_T;
} /* dbri_disable_te */


/*
 * dbri_enable_nt - XXX
 */
void
dbri_enable_nt(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;

	bs = DBRI_NT_STATUS_P(unitp);

	bs->i_var.enable = B_TRUE;
	if (bs->i_var.power == ISDN_PARAM_POWER_ON)
		unitp->regsp->n.sts |= DBRI_STS_N;
} /* dbri_enable_nt */


/*
 * dbri_disable_nt - Caller needs to update bs->sbri.tss to either
 * DBRI_NTINFO0_G1 or DBRI_NTINFO0_G4.
 */
void
dbri_disable_nt(dbri_unit_t *unitp)
{
	dbri_bri_status_t *bs;

	bs = DBRI_NT_STATUS_P(unitp);

	bs->i_var.enable = B_FALSE;
	unitp->regsp->n.sts &= ~DBRI_STS_N;
} /* dbri_disable_nt */


/*
 * dbri_isdn_set_format - XXX
 */
/*ARGSUSED*/
boolean_t
dbri_isdn_set_format(dbri_stream_t *ds, isdn_format_req_t *ifrp,
	aud_return_t *changep, int *error)
{
	dbri_unit_t *unitp;
	dbri_chan_t dchan1, dchan2;
	dbri_chantab_t *ctep1, *ctep2;
	dbri_chanformat_t *dfmt1, *dfmt2;

	ASSERT(ds != NULL);

	unitp = DsToUnitp(ds);
	*changep = AUDRETURN_NOCHANGE;
	*error = 0;

	if (!dbri_isowner(ds, ifrp->channel)) {
		*error = EPERM;
		return (B_FALSE);
	}

	dchan1 = dbri_itod_chan(ifrp->channel, DBRI_DIRECTION_IN);
	dchan2 = dbri_itod_chan(ifrp->channel, DBRI_DIRECTION_OUT);
	ATRACE(dbri_isdn_set_format, 'dchn', dchan1);
	ATRACE(dbri_isdn_set_format, 'dchn', dchan2);
	if (dchan1 == DBRI_CHAN_NONE || dchan2 == DBRI_CHAN_NONE) {
		ATRACE(dbri_isdn_set_format, '!chn', 0);
		*error = EINVAL;
		return (B_FALSE);
	}

	/*
	 * Allowing a management entity to change the default format
	 * of the audio channels will break our defined audio
	 * interface (which explicitly defines the default format
	 * of audio devices.
	 *
	 * dchan1 is always a source and dchan2 is always a sink.
	 */
	if (dchan1 == DBRI_AUDIO_IN || dchan2 == DBRI_AUDIO_OUT) {
		ATRACE(dbri_isdn_set_format, 'aud!', 0);
		*error = EINVAL;
		return (B_FALSE);
	}

	ctep1 = dbri_dchan_to_ctep(dchan1);
	ctep2 = dbri_dchan_to_ctep(dchan2);
	if (ctep1 == NULL || ctep2 == NULL) {
		ATRACE(dbri_isdn_set_format, '!ctp', 0);
		*error = EINVAL;
		return (B_FALSE);
	}

	/*
	 * Check to see if the format works for both dbri channels
	 */
	dfmt1 = dbri_itod_fmt(unitp, ctep1, &ifrp->format);
	if (dfmt1 == NULL) {
		ATRACE(dbri_isdn_set_format, 'err1', 0);
		*error = EINVAL;
		return (B_FALSE);
	}
	dfmt2 = dbri_itod_fmt(unitp, ctep2, &ifrp->format);
	if (dfmt2 == NULL) {
		ATRACE(dbri_isdn_set_format, 'err2', 0);
		*error = EINVAL;
		return (B_FALSE);
	}

	DChanToDs(unitp, dchan1)->dfmt = dfmt1;
	DChanToDs(unitp, dchan2)->dfmt = dfmt2;

	/*
	 * If the channel is active, then we're already finished.
	 *
	 * XXX - This is because we do not currently change the format of
	 * open channels "on the fly".
	 */
	if (DChanToAs(unitp, dchan1)->info.open ||
	    DChanToAs(unitp, dchan2)->info.open) {
		goto done;
	}

	/* XXX - How to handle errors? */
	if (!dbri_set_format(unitp, dchan1, dfmt1)) {
		*error = EINVAL;
		return (B_FALSE);
	}
	if (!dbri_set_format(unitp, dchan2, dfmt2)) {
		*error = EINVAL;
		return (B_FALSE);
	}

done:
	*changep = AUDRETURN_CHANGE;
	return (B_TRUE);
} /* dbri_isdn_set_format */


/*
 * dbri_isdn_get_format - XXX
 */
boolean_t
dbri_isdn_get_format(dbri_stream_t *ds, isdn_format_req_t *ifrp, int *error)
{
	dbri_unit_t *unitp;
	dbri_chan_t dchan1, dchan2;
	dbri_chanformat_t *dfmt;
	dbri_stream_t *tds;

	ASSERT(ds != NULL);

	*error = 0;

	unitp = DsToUnitp(ds);

	/*
	 * Hosts have no formats...
	 */
	if (ifrp->channel == ISDN_CHAN_HOST) {
		*error = EINVAL;
		return (B_FALSE);
	}

	/*
	 * If ISDN_CHAN_SELF, figure out who we are...
	 */
	if (ifrp->channel == ISDN_CHAN_SELF) {
		/*
		 * Not valid on non-data streams
		 */
		dchan1 = AsToDs(DsToAs(ds)->input_as)->ctep->dchan;
		dchan2 = AsToDs(DsToAs(ds)->output_as)->ctep->dchan;
	} else {
		dchan1 = dbri_itod_chan(ifrp->channel, DBRI_DIRECTION_IN);
		dchan2 = dbri_itod_chan(ifrp->channel, DBRI_DIRECTION_OUT);
	}
	ATRACE(dbri_isdn_set_format, 'dchn', dchan1);
	ATRACE(dbri_isdn_set_format, 'dchn', dchan2);
	if (dchan1 == DBRI_CHAN_NONE || dchan2 == DBRI_CHAN_NONE) {
		ATRACE(dbri_isdn_set_format, '!chn', 0);
		*error = EINVAL;
		return (B_FALSE);
	}

	if (!dbri_isowner(ds, ifrp->channel)) {
		*error = EPERM;
		return (B_FALSE);
	}

	/*
	 * Non-data streams have no formats
	 */
	if (!ISDATASTREAM(DChanToAs(unitp, dchan1))) {
		*error = EINVAL;
		return (B_FALSE);
	}

	/*
	 * We'll just use the output-side format...
	 */
	tds = DChanToDs(unitp, dchan2);

	/*
	 * Use the current format if there is one, otherwise just
	 * return the default format.
	 */
	if (tds->pipep == NULL)
		dfmt = tds->dfmt;
	else
		dfmt = &tds->pipep->format;

	if (!dbri_dtoi_fmt(dfmt, &ifrp->format))
		return (B_FALSE);

	return (B_TRUE);
} /* dbri_isdn_get_format */


/*
 * dbri_isdn_get_config - XXX
 */
/*ARGSUSED*/
boolean_t
dbri_isdn_get_config(dbri_stream_t *ds, isdn_conn_tab_t *ictp)
{
	dbri_unit_t *unitp;
	int icrcount = 0;
	isdn_conn_req_t *icrp;
	uint_t pipe;
	dbri_pipe_t *pipep;
	mblk_t *mp;
	struct copyreq *cqp;

	ASSERT(ds != NULL);
	unitp = DsToUnitp(ds);

	if ((mp = allocb(sizeof (struct copyreq), BPRI_HI)) == NULL)
		return (B_FALSE);
	if ((mp->b_cont = allocb(ictp->maxpaths * sizeof (isdn_conn_req_t),
	    BPRI_HI)) == NULL) {
		freeb(mp);
		return (B_FALSE);
	}
	mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
	cqp = (struct copyreq *)(void *)mp->b_rptr;

	mp->b_cont->b_wptr += (ictp->maxpaths * sizeof (isdn_conn_req_t));
	icrp = (isdn_conn_req_t *)(void *)mp->b_cont->b_rptr;

	for (pipe = 0; pipe < DBRI_MAX_PIPES; pipe++) {
		pipep = &unitp->ptab[pipe];

		/*
		 * The user's buffer has been filled up
		 */
		if (icrcount >= ictp->maxpaths)
			break;

		if (ISPIPEINUSE(pipep)) {
			dbri_chantab_t *ctep1, *ctep2;
			int score = 0;

			ctep1 = dbri_dchan_to_ctep(pipep->source.dchan);
			if (ctep1 != NULL && ctep1->ichan != ISDN_CHAN_HOST &&
			    dbri_isowner(ds, ctep1->ichan))
				score++;
			ctep2 = dbri_dchan_to_ctep(pipep->sink.dchan);
			if (ctep2 != NULL && ctep2->ichan != ISDN_CHAN_HOST &&
			    dbri_isowner(ds, ctep2->ichan))
				score++;

			if (score) {
				if (ctep1 == NULL)
					icrp[icrcount].from = ISDN_CHAN_NONE;
				else
					icrp[icrcount].from = ctep1->ichan;

				if (ctep2 == NULL)
					icrp[icrcount].to = ISDN_CHAN_NONE;
				else
					icrp[icrcount].to = ctep2->ichan;

				if (pipep->otherpipep != NULL) {
					/*
					 * Only report this connection once
					 */
					if (PIPENO(pipep->otherpipep) <
					    PIPENO(pipep))
						continue;

					icrp[icrcount].dir = ISDN_PATH_TWOWAY;
				} else {
					icrp[icrcount].dir = ISDN_PATH_ONEWAY;
				}

				if (!dbri_dtoi_fmt(&pipep->format,
				    &icrp[icrcount].format))
					continue;

				icrp[icrcount].reserved[0] = 0;
				icrp[icrcount].reserved[1] = 0;
				icrp[icrcount].reserved[2] = 0;
				icrp[icrcount].reserved[3] = 0;

				icrcount++;
			}
		}
	}

	ictp->npaths = icrcount;

	cqp->cq_cmd = ISDN_GET_CONFIG;
	cqp->cq_id = DsToAs(ds)->dioctl.ioctl_id;
	cqp->cq_addr = (caddr_t)ictp->paths;
	cqp->cq_cr = DsToAs(ds)->dioctl.credp;
	cqp->cq_size = sizeof (isdn_conn_req_t) * icrcount;
	cqp->cq_private = (mblk_t *)1; /* Note well... */
	cqp->cq_flag = 0;
	mp->b_datap->db_type = M_COPYOUT;

	ASSERT(DsToAs(ds)->writeq != NULL);
	qreply(DsToAs(ds)->writeq, mp);

	return (B_TRUE);
} /* dbri_isdn_get_config */


/*
 * dbri_dts - Everything we always wanted the DBRI DTS command to do...
 * namely: keep track of the tsd rings for us.
 *
 * XXX - Need a routine which will dts-delete the basepipe and mark all
 * the other pipes as gone...
 */
boolean_t
dbri_dts(dbri_unit_t *unitp, dbri_pipe_t *pipep, dbri_dts_action_t action)
{
	boolean_t insert_dummydata;
	struct {
		struct {
			dbri_pipe_t *prevp, *nextp;
			dbri_pipe_t *monitorp;
			boolean_t valid;
		} in;
		struct {
			dbri_pipe_t *prevp, *nextp;
			dbri_pipe_t *monitorp; /* dummy--- not used */
			boolean_t valid;
		} out;
	} tsd;

	insert_dummydata = B_FALSE;

	tsd.in.prevp = NULL;
	tsd.in.nextp = NULL;
	tsd.in.monitorp = NULL;
	tsd.out.prevp = NULL;
	tsd.out.nextp = NULL;

	tsd.in.valid = pipep->dts.opcode.r.vi;
	tsd.out.valid = pipep->dts.opcode.r.vo;

	switch (action) {
	case DBRI_DTS_DELETE:
		ATRACE(dbri_dts, ' del', PIPENO(pipep));
		if (pipep->refcnt != 0) {
			cmn_err(CE_WARN,
			    "dbri: Deleting bogus pipe[%d].refcnt = %d",
			    PIPENO(pipep), pipep->refcnt);
			return (B_FALSE);
		}

		pipep->dts.opcode.r.id = 0;

		/*
		 * We filled these in on insertion and thus can just extract
		 * the values from the DTS command we created...
		 */
		if (tsd.in.valid) {
			tsd.in.nextp =
			    &unitp->ptab[pipep->dts.input_tsd.r.next];
			tsd.in.prevp =
			    &unitp->ptab[pipep->dts.opcode.r.oldin];
		}
		if (tsd.out.valid) {
			tsd.out.nextp =
			    &unitp->ptab[pipep->dts.output_tsd.r.next];
			tsd.out.prevp =
			    &unitp->ptab[pipep->dts.opcode.r.oldout];

			/* XXX - ugly kludge to prevent popping on SpeakerBox */
			if (pipep->sink.dchan == DBRI_AUDIO_OUT)
				insert_dummydata = B_TRUE;
		}

		/*
		 * Remove our pipe from the ring by re-arranging the next
		 * and prev pointers of the surrounding pipes.
		 */
		if (tsd.in.valid) {
			tsd.in.nextp->dts.opcode.r.oldin =
			    PIPENO(tsd.in.prevp);
			tsd.in.prevp->dts.input_tsd.r.next =
			    PIPENO(tsd.in.nextp);
		}

		if (tsd.out.valid) {
			tsd.out.nextp->dts.opcode.r.oldout =
			    PIPENO(tsd.out.prevp);
			tsd.out.prevp->dts.output_tsd.r.next =
			    PIPENO(tsd.out.nextp);
		}

		break;

	case DBRI_DTS_MODIFY:
		ATRACE(dbri_dts, ' mod', PIPENO(pipep));

		if (pipep->refcnt < 1) {
			cmn_err(CE_WARN,
			    "dbri: Modifying bogus pipe[%d].refcnt = %d",
			    PIPENO(pipep), pipep->refcnt);
			ATRACE(dbri_dts, 'bgus', PIPENO(pipep));
			return (B_FALSE);
		}

		pipep->dts.opcode.r.id = 1;

		break;

	case DBRI_DTS_INSERT:
		ATRACE(dbri_dts, ' ins', PIPENO(pipep));

		if (pipep->refcnt < 1) {
			cmn_err(CE_WARN,
			    "dbri: Inserting bogus pipe[%d].refcnt = %d",
			    PIPENO(pipep), pipep->refcnt);
			ATRACE(dbri_dts, 'bgus', PIPENO(pipep));
			return (B_FALSE);
		}

		pipep->dts.opcode.r.id = 1;

		/*
		 * If this is the base pipe or there is only a single
		 * pipe on the tsd ring, then we don't need to traverse
		 * the linked list to discover prev and nextpipe.
		 */
		if (tsd.in.valid) {
			if (!dbri_find_oldnext(unitp, pipep,
			    DBRI_DIRECTION_IN, &tsd.in.prevp, &tsd.in.nextp,
			    &tsd.in.monitorp)) {
				ATRACE(dbri_dts, 'bgus', PIPENO(pipep));
				return (B_FALSE);
			}

			if (tsd.in.monitorp == NULL) {
				tsd.in.nextp->dts.opcode.r.oldin =
				    PIPENO(pipep);
				tsd.in.prevp->dts.input_tsd.r.next =
				    PIPENO(pipep);
			} else {
				ATRACE(dbri_dts, 'monp',
				    PIPENO(tsd.in.monitorp));
				if (tsd.in.monitorp->monitorpipep != NULL) {
					/*
					 * Underlying pipe already
					 * has a monitor pipe
					 */
					return (B_FALSE);
				}

				if ((unitp->version >= 'a' &&
				    unitp->version <= 'e') &&
				    ((tsd.in.monitorp->source.dchan ==
				    DBRI_NT_B1_IN) ||
				    (tsd.in.monitorp->source.dchan ==
				    DBRI_TE_B1_IN))) {
					return (B_FALSE);
				}

				/*
				 * Cannot create serial-serial connections
				 * that need to be a monitor pipe on the
				 * source end.
				 */
				if (pipep->sink.dchan != DBRI_CHAN_HOST)
					return (B_FALSE);

				pipep->ismonitor = B_TRUE;

				pipep->monitorpipep = tsd.in.monitorp;
				pipep->monitorpipep->monitorpipep = pipep;

				pipep->monitorpipep->dts.input_tsd.r.mon =
				    PIPENO(pipep);
				pipep->monitorpipep->dts.input_tsd.r.mode =
				    DBRI_DTS_MONITOR;
			}
		}

		if (tsd.out.valid) {
			if (!dbri_find_oldnext(unitp, pipep,
			    DBRI_DIRECTION_OUT, &tsd.out.prevp, &tsd.out.nextp,
			    &tsd.out.monitorp)) {
				ATRACE(dbri_dts, 'bgso', PIPENO(pipep));
				return (B_FALSE);
			}

			tsd.out.nextp->dts.opcode.r.oldout = PIPENO(pipep);
			tsd.out.prevp->dts.output_tsd.r.next = PIPENO(pipep);
		}

		if (tsd.in.valid) {
			pipep->dts.opcode.r.oldin = PIPENO(tsd.in.prevp);
			pipep->dts.input_tsd.r.next = PIPENO(tsd.in.nextp);
		} else {
			pipep->dts.opcode.r.oldin = 0;
			pipep->dts.input_tsd.r.next = 0;
		}
		if (tsd.out.valid) {
			pipep->dts.opcode.r.oldout = PIPENO(tsd.out.prevp);
			pipep->dts.output_tsd.r.next = PIPENO(tsd.out.nextp);
		} else {
			pipep->dts.opcode.r.oldout = 0;
			pipep->dts.output_tsd.r.next = 0;
		}
		break;

	default:
		cmn_err(CE_WARN, "dbri_dts called with invalid argument");
		return (B_FALSE);
	} /* switch on dbri_dts command */

	pipep->dts.opcode.r.cmd = DBRI_OPCODE_DTS;

	ATRACE(dbri_dts, 'exec', PIPENO(pipep));

	{
		dbri_dts_cmd_t dts;
		dbri_pipe_t *npipep;
		dbri_chip_cmd_t	cmd;

		npipep = (pipep->ismonitor) ? pipep->monitorpipep : pipep;

		dts.opcode.word32 = npipep->dts.opcode.word32;
		dts.input_tsd.word32 = npipep->dts.input_tsd.word32;
		dts.output_tsd.word32 = npipep->dts.output_tsd.word32;

		if (pipep->ismonitor)
			dts.opcode.r.vo = 0;

		cmd.opcode = dts.opcode.word32;
		cmd.arg[0] = dts.input_tsd.word32;
		cmd.arg[1] = dts.output_tsd.word32;

		if (insert_dummydata)
			mmcodec_insert_dummydata(unitp);
		else
			dbri_command(unitp, cmd);

		cmd.opcode = DBRI_CMD_PAUSE;
		dbri_command(unitp, cmd);
	}

	return (B_TRUE);
} /* dbri_dts */

void
dbri_remove_timeouts(dbri_unit_t *unitp)
{
	/*
	 * Remove pending timeouts
	 */
	unitp->keep_alive_running = B_FALSE;
	if (unitp->keepalive_id) {
		(void) untimeout(unitp->keepalive_id);
		unitp->keepalive_id = 0;
	}
	if (unitp->tetimer_id) {
		(void) untimeout(unitp->tetimer_id);
		unitp->tetimer_id = 0;
	}
	if (unitp->nttimer_t101_id) {
		(void) untimeout(unitp->nttimer_t101_id);
		unitp->nttimer_t101_id = 0;
	}
	if (unitp->nttimer_t102_id) {
		(void) untimeout(unitp->nttimer_t102_id);
		unitp->nttimer_t102_id = 0;
	}
	if (unitp->mmcwatchdog_id) {
		(void) untimeout(unitp->mmcwatchdog_id);
		unitp->mmcwatchdog_id = 0;
	}
	if (unitp->mmcvolume_id) {
		(void) untimeout(unitp->mmcvolume_id);
		unitp->mmcvolume_id = 0;
	}
	if (unitp->mmcdataok_id) {
		(void) untimeout(unitp->mmcdataok_id);
		unitp->mmcdataok_id = 0;
	}
	if (unitp->mmcstartup_id) {
		(void) untimeout(unitp->mmcstartup_id);
		unitp->mmcstartup_id = 0;
	}
}
