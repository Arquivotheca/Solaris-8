/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)dbri_connect.c	1.13	99/05/21 SMI"

/*
 * Sun DBRI pipe/channel connection routines
 */

/*
 * REMIND: exodus 1992/09/23 - marking busy and monitor connections
 * For serial-serial connections we should un-mark the input channels
 * (only) after we have set up the connection (and signal the open
 * condition variable) so a read-only open of the stream will succeed.
 * Otherwise monitor pipes become tedious.  There's also the problem
 * of opening a stream read-only or read-write then creating a
 * serial-serial connection to it from a mgt device.  Perhaps the input
 * channels should only be marked busy when no more connections can
 * be made to them?  (ie, for output: one connection, for input: two
 * connections).
 */

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/ioccom.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/file.h>
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
 * Local function prototypes
 */
extern boolean_t dbri_connection_create(dbri_conn_request_t *);
extern boolean_t dbri_mark_channels_busy(dbri_unit_t *,
	dbri_conn_request_t *, boolean_t);
extern void dbri_pipe_configure(dbri_unit_t *, dbri_conn_oneway_t *);
extern boolean_t dbri_connection_cleanup(dbri_unit_t *, dbri_conn_oneway_t *);
extern boolean_t dbri_isdn_connection_setup(dbri_unit_t *, isdn_chan_t,
	isdn_chan_t, isdn_format_t *, dbri_conn_oneway_t *);
extern boolean_t dbri_mark_channel_busy(dbri_unit_t *, dbri_chan_t,
	dbri_chanformat_t *, boolean_t, boolean_t);


/*
 * dbri_connection_create - create the connection described in the dbri
 * connection structure.
 *
 * The goals of this function are:
 * 	1) Perform sanity checks early on
 * 	2) Have a point at which failures can no longer occur
 * 	3) Maintain audio semantics (sleep in open) on the streams
 *	   and channels which are connected
 * 	4) atomic resource allocation for the entire connection
 *
 * The problem with having a simple routine to set up one unidirectional
 * connection is that when setting up a bidirectional connection, the
 * resources for the second "half" may become allocated after we have
 * already setup the first "half".  Setting up one or two connections at
 * a time solves this problem since we allocate all resources at one
 * time.  Setting the busy flags in the dbri_stream's associated with the
 * channels we are connecting allows us to know in advance if the
 * connection is likely to succeed (before we allocate pipes, etc.) and
 * it also prevents other asynchronous (from this connection_create's
 * point of view) connections from taking away our resources before we're
 * finished.
 *
 * This routine is supposed to operate solely on dbri-specific data
 * structures.  The caller should have already converted any generic isdn
 * data structures into the dbri format.
 */
boolean_t
dbri_connection_create(dbri_conn_request_t *dcrp)
{
	dbri_unit_t *unitp;
	dbri_conn_oneway_t *dcrp1, *dcrp2;
	boolean_t basepipes_initialized = B_FALSE;
	int error = 0;

	ASSERT(dcrp != NULL);

	unitp = DsToUnitp(dcrp->ds);

	dcrp->errno = 0;
	dcrp1 = &dcrp->oneway;
	dcrp2 = &dcrp->otherway;

	if (!dcrp->oneway_valid && !dcrp->otherway_valid) {
		ATRACE(dbri_connection_create, 'NOOP', 0);
		dcrp->errno = EINVAL;
		return (B_FALSE);
	}

	/*
	 * Mark the dbri channels that we are going to use as busy.  Ben
	 * doesn't like this, but to maintain audio semantics (sleep in
	 * open if a channel is busy), this step must be performed.  This
	 * also guarantees that once the channels and pipes are
	 * allocated, we will not run into resource conflicts with
	 * concurrent connection requests made on other management
	 * channels or via device opens.
	 */
	if (!dbri_mark_channels_busy(unitp, dcrp, B_TRUE)) {
		ATRACE(dbri_connection_create, '!mrk', 0);
		dcrp->errno = EBUSY;
		return (B_FALSE);
	}

	/*
	 * Allocate pipes to make the connections with
	 */
	if (dcrp->oneway_valid) {
		dcrp1->pipep = dbri_pipe_allocate(unitp, dcrp1);
		if (dcrp1->pipep == NULL) {
			ATRACE(dbri_connection_create, '!pp1', 0);
			(void) dbri_mark_channels_busy(unitp, dcrp, B_FALSE);
			dcrp->errno = EBUSY;
			return (B_FALSE);
		}

		/*
		 * Pipe's ds points to a source or sink of data
		 * only--- a serial-serial pipe has no STREAM
		 * attached.
		 */
		if (dcrp1->source.dchan == DBRI_CHAN_HOST)
			dcrp1->pipep->ds = dcrp1->ds;
		else if (dcrp1->sink.dchan == DBRI_CHAN_HOST)
			dcrp1->pipep->ds = dcrp1->ds;
		else
			dcrp1->pipep->ds = NULL; /* no data stream */

		/*
		 * data streams point to their pipes
		 */
		if (dcrp1->ds != NULL)
			dcrp1->ds->pipep = dcrp1->pipep;
	}
	if (dcrp->otherway_valid) {
		dcrp2->pipep = dbri_pipe_allocate(unitp, dcrp2);
		if (dcrp2->pipep == NULL) {
			ATRACE(dbri_connection_create, '!pp2', 0);
			if (dcrp->oneway_valid)
				dbri_pipe_free(dcrp1->pipep);
			(void) dbri_mark_channels_busy(unitp, dcrp, B_FALSE);
			dcrp->errno = EINVAL;
			return (B_FALSE);
		}

		/*
		 * Pipe's ds points to a source or sink of data
		 * only--- a serial-serial pipe has no STREAM
		 * attached.
		 */
		if (dcrp2->source.dchan == DBRI_CHAN_HOST)
			dcrp2->pipep->ds = dcrp2->ds;
		else if (dcrp2->sink.dchan == DBRI_CHAN_HOST)
			dcrp2->pipep->ds = dcrp2->ds;
		else
			dcrp2->pipep->ds = NULL; /* no data stream */

		/*
		 * data streams point to their pipes
		 */
		if (dcrp2->ds != NULL)
			dcrp2->ds->pipep = dcrp2->pipep;
	}

	if (dcrp->oneway_valid) {
		dbri_chi_status_t *cs = DBRI_CHI_STATUS_P(unitp);

		if (dbri_is_dbri_audio_chan(dcrp1->source.dchan) ||
		    dbri_is_dbri_audio_chan(dcrp1->sink.dchan)) {
			if (dcrp1->pipep->ds != NULL)
				cs->chi_dbri_clock = DBRI_CLOCK_EXT;
			else
				cs->chi_dbri_clock = DBRI_CLOCK_DBRI;
		}
	} else if (dcrp->otherway_valid) {
		dbri_chi_status_t *cs = DBRI_CHI_STATUS_P(unitp);

		/*
		 * Only need to check this side if the other side
		 * was not valid (thus the "else if").
		 */

		if (dbri_is_dbri_audio_chan(dcrp2->source.dchan) ||
		    dbri_is_dbri_audio_chan(dcrp2->sink.dchan)) {
			if (dcrp2->pipep->ds != NULL)
				cs->chi_dbri_clock = DBRI_CLOCK_EXT;
			else
				cs->chi_dbri_clock = DBRI_CLOCK_DBRI;
		}
	}

	/*
	 * Two-way connections have pipes that reference each other
	 */
	if (dcrp->oneway_valid && dcrp->otherway_valid) {
		dcrp1->pipep->otherpipep = dcrp2->pipep;
		dcrp2->pipep->otherpipep = dcrp1->pipep;
	}

	/*
	 * *** NOTE ***
	 * Now that we have allocated pipes, if any failures occur after
	 * this point, exit via "goto failed;" so cleanup can occur.
	 */

	/*
	 * We are now ready to initialize the base pipes.  The only
	 * remaining failure point is the CHI initialization failing.
	 *
	 * If all goes well, the only remaining initialization to
	 * complete is inserting the pipes into the timeslot rings of
	 * DBRI.
	 */

/*
 * REMIND: exodus 1992/09/23 - connection_create and cv_wait
 * This routine currently is assuming user context.  To get this all
 * working, this means that we are going to hang the STREAMS service
 * thread for a short amount of time if a connection to CHI is made via
 * an ioctl.  Later we will want to do this differently--- perhaps create
 * our own thread that processes dbri_conn_request_t structures in a
 * first-come first-server fashion.  It makes the connection code much
 * easier to assume user context exists, so having our own thread would
 * be much the same.
 *
 * Triggering a soft interrupt and having the interrupt handler perform
 * the connection tasks may be an easy way to get enough context to
 * cv_wait in.
 */
	if (dcrp->oneway_valid) {
		if (!dbri_basepipe_init(unitp, dcrp1->source.dchan, &error)) {
			ATRACE(dbri_connection_create, 'bppi', 1);
			goto failed;
		}
		if (!dbri_basepipe_init(unitp, dcrp1->sink.dchan, &error)) {
			ATRACE(dbri_connection_create, 'bppi', 2);
			(void) dbri_basepipe_fini(unitp, dcrp1->source.dchan);
			goto failed;
		}
	}
	if (dcrp->otherway_valid) {
		if (!dbri_basepipe_init(unitp, dcrp2->source.dchan, &error)) {
			ATRACE(dbri_connection_create, 'bppi', 3);
			if (dcrp->oneway_valid) {
				(void) dbri_basepipe_fini(unitp,
				    dcrp1->source.dchan);
				(void) dbri_basepipe_fini(unitp,
				    dcrp1->sink.dchan);
			}
			goto failed;
		}
		if (!dbri_basepipe_init(unitp, dcrp2->sink.dchan, &error)) {
			ATRACE(dbri_connection_create, 'bppi', 4);
			if (dcrp->oneway_valid) {
				(void) dbri_basepipe_fini(unitp,
				    dcrp1->source.dchan);
				(void) dbri_basepipe_fini(unitp,
				    dcrp1->sink.dchan);
			}
			(void) dbri_basepipe_fini(unitp, dcrp2->source.dchan);
			goto failed;
		}
	}
	basepipes_initialized = B_TRUE;

	/*
	 * Setup software state in the pipe structures.  This will
	 * fill in all applicable fields in the pipe structure
	 * and issue the initial SDP command to clear out the pipe
	 * and set its modes.
	 */
	if (dcrp->oneway_valid)
		dbri_pipe_configure(unitp, dcrp1);
	if (dcrp->otherway_valid)
		dbri_pipe_configure(unitp, dcrp2);

	/*
	 * Clear pipes and initialize DMA engine
	 */
	if (dcrp->oneway_valid) {
		dbri_chip_cmd_t	cmd;

		cmd.opcode = dcrp1->pipep->sdp;
		cmd.arg[0] = 0; /* NULL md list address */
		dbri_command(unitp, cmd);

		cmd.opcode = DBRI_CMD_PAUSE;
		dbri_command(unitp, cmd);
	}
	if (dcrp->otherway_valid) {
		dbri_chip_cmd_t	cmd;

		cmd.opcode = dcrp2->pipep->sdp;
		cmd.arg[0] = 0; /* NULL md list address */
		dbri_command(unitp, cmd);

		cmd.opcode = DBRI_CMD_PAUSE;
		dbri_command(unitp, cmd);
	}

	/*
	 * Add the pipes to the descriptor rings now
	 */
	if (dcrp->oneway_valid) {
		if (!dbri_pipe_insert(unitp, dcrp1->pipep)) {
			ATRACE(dbri_connection_create, 'pins', 1);
			error = ENXIO;
			goto failed;
		}
	}
	if (dcrp->otherway_valid) {
		if (!dbri_pipe_insert(unitp, dcrp2->pipep)) {
			if (dcrp->oneway_valid)
				(void) dbri_pipe_remove(unitp, dcrp1->pipep);
			ATRACE(dbri_connection_create, 'pins', 2);
			error = ENXIO;
			goto failed;
		}
	}

	/*
	 * Perform any per-pipe initialization here.
	 */
	if (dcrp->oneway_valid) {
		switch (dcrp1->sink.dchan) {
		case DBRI_TE_D_IN:
		case DBRI_TE_D_OUT:
		case DBRI_NT_D_IN:
		case DBRI_NT_D_OUT:
			dbri_setup_ntte(unitp, dcrp1->sink.dchan);
			break;

		default:
			break;
		}

		switch (dcrp1->source.dchan) {
		case DBRI_TE_D_IN:
		case DBRI_TE_D_OUT:
		case DBRI_NT_D_IN:
		case DBRI_NT_D_OUT:
			dbri_setup_ntte(unitp, dcrp1->source.dchan);
			break;

		default:
			break;
		}
	}
	if (dcrp->otherway_valid) {
		switch (dcrp2->sink.dchan) {
		case DBRI_TE_D_IN:
		case DBRI_TE_D_OUT:
		case DBRI_NT_D_IN:
		case DBRI_NT_D_OUT:
			dbri_setup_ntte(unitp, dcrp2->sink.dchan);
			break;

		default:
			break;
		}

		switch (dcrp1->source.dchan) {
		case DBRI_TE_D_IN:
		case DBRI_TE_D_OUT:
		case DBRI_NT_D_IN:
		case DBRI_NT_D_OUT:
			dbri_setup_ntte(unitp, dcrp2->source.dchan);
			break;

		default:
			break;
		}
	}

	/*
	 * If any serial-serial connections are made, then mark the
	 * input channels as *not* busy so they can be opened as
	 * monitor pipes...
	 */
	if (dcrp->oneway_valid) {
		if (dcrp1->sink.dchan != DBRI_CHAN_HOST &&
		    dcrp1->source.dchan != DBRI_CHAN_HOST) {
			(void) dbri_mark_channel_busy(unitp,
			    dcrp1->source.dchan, &dcrp1->format,
			    B_FALSE, B_FALSE);
		}
	}
	if (dcrp->otherway_valid) {
		if (dcrp2->sink.dchan != DBRI_CHAN_HOST &&
		    dcrp2->source.dchan != DBRI_CHAN_HOST) {
			(void) dbri_mark_channel_busy(unitp,
			    dcrp2->source.dchan, &dcrp2->format,
			    B_FALSE, B_FALSE);
		}
	}

	/*
	 * FINISHED!
	 */
	return (B_TRUE);

failed:
	/*
	 * Coming here implies everything was okay up until the
	 * basepipe initialization, so tear down everything
	 * we set up.
	 */
	if (dcrp->oneway_valid) {
		dcrp1->pipep->otherpipep = NULL;
		dcrp1->pipep->ds = NULL;
		dbri_pipe_free(dcrp1->pipep);
		dcrp1->pipep = NULL;
		if (!basepipes_initialized) {
			if (dcrp1->ds != NULL)
				dcrp1->ds->pipep = NULL;
		}
	}
	if (dcrp->otherway_valid) {
		dcrp2->pipep->otherpipep = NULL;
		dcrp2->pipep->ds = NULL;
		dbri_pipe_free(dcrp2->pipep);
		dcrp2->pipep = NULL;
		if (!basepipes_initialized) {
			if (dcrp2->ds != NULL)
				dcrp2->ds->pipep = NULL;
		}
	}

	if (basepipes_initialized) {
		if (dcrp->oneway_valid) {
			(void) dbri_basepipe_fini(unitp, dcrp1->source.dchan);
			(void) dbri_basepipe_fini(unitp, dcrp1->sink.dchan);
		}
		if (dcrp->otherway_valid) {
			(void) dbri_basepipe_fini(unitp, dcrp2->source.dchan);
			(void) dbri_basepipe_fini(unitp, dcrp2->sink.dchan);
		}
	}

	(void) dbri_mark_channels_busy(unitp, dcrp, B_FALSE);

	dcrp->errno = error;
	return (B_FALSE);
} /* dbri_connection_create */


/*
 * dbri_connection_destroy - XXX
 */
boolean_t
dbri_connection_destroy(dbri_unit_t *unitp, dbri_conn_request_t *dcrp)
{
	dbri_conn_oneway_t *dcrp1, *dcrp2;

	ASSERT(dcrp != NULL);

	dcrp1 = &dcrp->oneway;
	dcrp2 = &dcrp->otherway;

	/*
	 * Cleanup channel
	 */
	if (dcrp->oneway_valid)
		(void) dbri_connection_cleanup(unitp, dcrp1);
	if (dcrp->otherway_valid)
		(void) dbri_connection_cleanup(unitp, dcrp2);

	/*
	 * Now that we no longer are using any pipes, allow the
	 * basepipes to be de-initialized.
	 */
	if (dcrp->oneway_valid) {
		(void) dbri_basepipe_fini(unitp, dcrp1->source.dchan);
		(void) dbri_basepipe_fini(unitp, dcrp1->sink.dchan);
	}
	if (dcrp->otherway_valid) {
		(void) dbri_basepipe_fini(unitp, dcrp2->source.dchan);
		(void) dbri_basepipe_fini(unitp, dcrp2->sink.dchan);
	}

	/*
	 * Now we can free the "resources" we've tied up...
	 * We've already torn down everything--- if this fails there's
	 * not much we can do about it!
	 */
	(void) dbri_mark_channels_busy(unitp, dcrp, B_FALSE);

	/*
	 * XXX - What else to cleanup here?
	 */

	return (B_TRUE);
} /* dbri_connection_destroy */


/*
 * dbri_stream_connection_create - Connect an audio stream to its
 * associated hardware.  This routine is ONLY called for DATASTREAMS.
 * The parameters to this subroutine are aways valid but the requested
 * actions might not be possible.
 */
boolean_t
dbri_stream_connection_create(dbri_stream_t *ds, int oflag, int *error)
{
	dbri_unit_t *unitp;
	dbri_conn_request_t dcr;
	boolean_t wantread, wantwrite;

	unitp = DsToUnitp(ds);

	ASSERT_UNITLOCKED(unitp);
	ASSERT(ds != NULL);
	ASSERT(ISDATASTREAM(DsToAs(ds)));

	ATRACE(dbri_stream_connection_create, '  ds', ds);
	ATRACE(dbri_stream_connection_create, 'flag', oflag);
	ATRACE(dbri_stream_connection_create, 'dchn', ds->ctep->dchan);

	wantread = oflag & FREAD;
	wantwrite = oflag & FWRITE;

	/*
	 * Fill in "global" connection information
	 */
	dcr.has_context = B_TRUE;
	dcr.ds = ds;
	dcr.already_busy = B_TRUE;
	dcr.oneway_valid = B_FALSE;
	dcr.otherway_valid = B_FALSE;

	/*
	 * Fill in information about the connection (each direction).
	 */
	if (wantwrite) {
		dcr.oneway.ds = AsToDs(DsToAs(ds)->output_as);
		dcr.oneway_valid = B_TRUE;
		dcr.oneway.sink.dchan = dcr.oneway.ds->ctep->dchan;
		dcr.oneway.sink.ctep = dcr.oneway.ds->ctep;
		dcr.oneway.source.dchan = DBRI_CHAN_HOST;
		if (!dbri_set_format(unitp, dcr.oneway.sink.dchan, NULL)) {
			*error = EINVAL;
			return (B_FALSE);
		}
		dcr.oneway.format = *dcr.oneway.ds->dfmt;
	}
	if (wantread) {
		dcr.otherway.ds = AsToDs(DsToAs(ds)->input_as);
		dcr.otherway_valid = B_TRUE;
		dcr.otherway.sink.dchan = DBRI_CHAN_HOST;
		dcr.otherway.source.dchan = dcr.otherway.ds->ctep->dchan;
		dcr.otherway.source.ctep = dcr.otherway.ds->ctep;
		if (!dbri_set_format(unitp, dcr.otherway.source.dchan, NULL)) {
			*error = EINVAL;
			return (B_FALSE);
		}

		/*
		 * Both directions will have the same format, so don't
		 * change it if it's already set...
		 */
		dcr.otherway.format = *dcr.otherway.ds->dfmt;
	}

	if (!dbri_connection_create(&dcr)) {
		*error = dcr.errno;
		return (B_FALSE);
	}

	/*
	 * Configure STREAMS queue parameters for this channel
	 */
	if (wantwrite)
		dbri_config_queue(dcr.oneway.ds);
	if (wantread)
		dbri_config_queue(dcr.otherway.ds);

	return (B_TRUE);
} /* dbri_stream_connection_create */


/*
 * dbri_stream_connection_destroy - disconnect a stream from the
 * hardware.  This is not intended to be called by the setchannel ioctl,
 * only by open.
 *
 * NB: This routine assumes that STOP has already been called (DI open
 * does this for us).
 */
boolean_t
dbri_stream_connection_destroy(dbri_stream_t *ds)
{
	dbri_unit_t *unitp;
	dbri_conn_request_t dcr;
	boolean_t wantread, wantwrite;

	unitp = DsToUnitp(ds);

	ASSERT_UNITLOCKED(unitp);
	ASSERT(ds != NULL);
	ASSERT(ISDATASTREAM(DsToAs(ds)));

	ATRACE(dbri_stream_connection_destroy, '  ds', ds);
	ATRACE(dbri_stream_connection_destroy, 'dchn', ds->ctep->dchan);

	/*
	 * Close is a little different than open... the DI close
	 * routine calls the DD close routine twice if the stream
	 * is open read/write--- once for each logical stream.
	 */
	wantread = (DsToAs(ds) == DsToAs(ds)->input_as);
	wantwrite = (DsToAs(ds) == DsToAs(ds)->output_as);

	ASSERT(wantread || wantwrite);

	/*
	 * Fill in "global" connection information
	 */
	dcr.has_context = B_TRUE;
	dcr.ds = ds;
	dcr.already_busy = B_TRUE;
	dcr.oneway_valid = B_FALSE;
	dcr.otherway_valid = B_FALSE;

	/*
	 * Fill in information about the connection (each direction).
	 *
	 * If the stream associated with the connection has no pipe
	 * associated with it, we assume it was never properly setup.
	 */
	if (wantwrite) {
		dcr.oneway.ds = AsToDs(DsToAs(ds)->output_as);
		dcr.oneway.sink.dchan = dcr.oneway.ds->ctep->dchan;
		dcr.oneway.source.dchan = DBRI_CHAN_HOST;
		dcr.oneway.pipep = dcr.oneway.ds->pipep;
		if (dcr.oneway.pipep != NULL) {
			dcr.oneway.format = dcr.oneway.pipep->format;
			dcr.oneway_valid = B_TRUE;
		}
	}
	if (wantread) {
		dcr.otherway.ds = AsToDs(DsToAs(ds)->input_as);
		dcr.otherway.sink.dchan = DBRI_CHAN_HOST;
		dcr.otherway.source.dchan = dcr.otherway.ds->ctep->dchan;
		dcr.otherway.pipep = dcr.otherway.ds->pipep;
		if (dcr.otherway.pipep != NULL) {
			dcr.otherway.format = dcr.otherway.pipep->format;
			dcr.otherway_valid = B_TRUE;
		}
	}

	if (!dbri_connection_destroy(unitp, &dcr))
		return (B_FALSE);

	return (B_TRUE);
} /* dbri_stream_connection_destroy */


/*
 * dbri_isdn_connection_create - XXX
 *
 * The isdnio man page says that a connection made with
 * dir=ISDN_PATH_TWOWAY has both directions deleted with a
 * single ISDN_SET_CHANNEL(ISDN_PATH_DISCONNECT) ioctl.
 * The driver is supposed to remember which connections where
 * "TWOWAY" and which were "ONEWAY" and delete them appropriately.
 * So...
 */
boolean_t
dbri_isdn_connection_create(dbri_stream_t *ds, isdn_conn_req_t *icrp,
	aud_return_t *changep, int *error)
{
	dbri_unit_t *unitp;
	dbri_conn_request_t dcr;

	ASSERT(ds != NULL);
	unitp = DsToUnitp(ds);
	ASSERT_UNITLOCKED(unitp);

	*changep = AUDRETURN_NOCHANGE;
	*error = 0;

	/*
	 * Check permissions--- the controlling dbri_stream must own
	 * both ends of the connection for the connection to be
	 * created.
	 */
	if (!dbri_isowner(ds, icrp->from) || !dbri_isowner(ds, icrp->to)) {
		*error = EPERM;
		return (B_FALSE);
	}

	/*
	 * Fill in "global" connection information
	 */
	dcr.has_context = B_FALSE; /* called only from ioctl's... */
	dcr.ds = ds;		/* controlling ds */
	dcr.already_busy = B_FALSE;
	dcr.oneway_valid = B_FALSE;
	dcr.otherway_valid = B_FALSE;

	if (!dbri_isdn_connection_setup(unitp, icrp->from, icrp->to,
	    &icrp->format, &dcr.oneway)) {
		*error = EINVAL;
		return (B_FALSE);
	}
	dcr.oneway_valid = B_TRUE;

	if (icrp->dir == ISDN_PATH_TWOWAY) {
		if (!dbri_isdn_connection_setup(unitp, icrp->to, icrp->from,
		    &icrp->format, &dcr.otherway)) {
			*error = EINVAL;
			return (B_FALSE);
		}
		dcr.otherway_valid = B_TRUE;
	}

	/*
	 * Now actually do the work...
	 */
	if (!dbri_connection_create(&dcr)) {
		ATRACE(dbri_isdn_connection_create, '!dcr', 0);
		*error = dcr.errno;
		return (B_FALSE);
	}

	return (B_TRUE);
} /* dbri_isdn_connection_create */


/*
 * dbri_isdn_connection_destroy - XXX
 *
 * The isdnio man page says that a connection made with
 * dir=ISDN_PATH_TWOWAY has both directions deleted with a
 * single ISDN_SET_CHANNEL(ISDN_PATH_DISCONNECT) ioctl.
 * The driver is supposed to remember which connections where
 * "TWOWAY" and which were "ONEWAY" and delete them appropriately.
 * So...
 */
boolean_t
dbri_isdn_connection_destroy(dbri_stream_t *ds, isdn_conn_req_t *icrp,
	aud_return_t *changep, int *error)
{
	dbri_unit_t *unitp;
	dbri_conn_request_t dcr;
	dbri_chan_t dchan1, dchan2;

	ASSERT(ds != NULL);
	unitp = DsToUnitp(ds);
	ASSERT_UNITLOCKED(unitp);

	*changep = AUDRETURN_NOCHANGE;
	*error = 0;

	/*
	 * Fill in "global" connection information
	 */
	dcr.has_context = B_FALSE; /* called only from ioctl's... */
	dcr.ds = ds;		/* controlling ds */
	dcr.already_busy = B_FALSE;
	dcr.oneway_valid = B_FALSE;
	dcr.otherway_valid = B_FALSE;

	/*
	 * Convert ISDN channels to DBRI channels
	 */
	dchan1 = dbri_itod_chan(icrp->from, DBRI_DIRECTION_IN);
	dchan2 = dbri_itod_chan(icrp->to, DBRI_DIRECTION_OUT);
	ATRACE(dbri_isdn_connection_destroy, 'dchn', dchan1);
	ATRACE(dbri_isdn_connection_destroy, 'dchn', dchan2);
	if (dchan1 == DBRI_CHAN_NONE || dchan2 == DBRI_CHAN_NONE) {
		ATRACE(dbri_isdn_connection_destroy, '!chn', 0);
		*error = EINVAL;
		return (B_FALSE);
	}

	/* XXX - need to use find_short_pipe? */
	dcr.oneway.pipep = dbri_findshortpipe(unitp, dchan1, dchan2);
	if (dcr.oneway.pipep == NULL) {
		ATRACE(dbri_isdn_connection_destroy, '!pip', 0);
		*error = EINVAL;
		return (B_FALSE);
	}
	dcr.oneway.format = dcr.oneway.pipep->format;
	dcr.oneway.ds = NULL;	/* no data stream */
	dcr.oneway.source.dchan = dchan1;
	dcr.oneway.sink.dchan = dchan2;
	dcr.oneway_valid = B_TRUE;

	if (dcr.oneway.pipep->otherpipep != NULL) {
		/*
		 * Convert ISDN channels to DBRI channels
		 */
		dchan1 = dbri_itod_chan(icrp->to, DBRI_DIRECTION_IN);
		dchan2 = dbri_itod_chan(icrp->from, DBRI_DIRECTION_OUT);
		ATRACE(dbri_isdn_connection_destroy, 'dchn', dchan1);
		ATRACE(dbri_isdn_connection_destroy, 'dchn', dchan2);
		if (dchan1 == DBRI_CHAN_NONE || dchan2 == DBRI_CHAN_NONE) {
			ATRACE(dbri_isdn_connection_destroy, '!chn', 1);
			*error = EINVAL;
			return (B_FALSE);
		}

		dcr.otherway.pipep = dbri_findshortpipe(unitp, dchan1,
		    dchan2);
		if (dcr.otherway.pipep == NULL) {
			ATRACE(dbri_isdn_connection_destroy, '!pip', 1);
			*error = EINVAL;
			return (B_FALSE);
		}
		ASSERT(dcr.otherway.pipep == dcr.oneway.pipep->otherpipep);
		ASSERT(dcr.oneway.pipep == dcr.otherway.pipep->otherpipep);

		dcr.otherway.format = dcr.otherway.pipep->format;
		dcr.otherway.ds = NULL;	/* no data stream */
		dcr.otherway.source.dchan = dchan1;
		dcr.otherway.sink.dchan = dchan2;
		dcr.otherway_valid = B_TRUE;
	}

	/*
	 * Now actually do the work...
	 */
	if (!dbri_connection_destroy(unitp, &dcr)) {
		ATRACE(dbri_isdn_connection_destroy, '!dcr', 0);
		*error = EINVAL;
		return (B_FALSE);
	}

	return (B_TRUE);
} /* dbri_isdn_connection_destroy */


/*
 * dbri_set_format - A ds requests that channel be configured according
 * to the supplied format.  The ds is not necessarily connected to the
 * channel.
 *
 * If a format is supplied, use it.  Otherwise, use ds->dfmt.
 */
boolean_t
dbri_set_format(dbri_unit_t *unitp, dbri_chan_t dchan,
	dbri_chanformat_t *dfmt)
{
	dbri_stream_t *ds;	/* Target dbri_stream */

	ATRACE(dbri_set_format, 'dchn', dchan);

	if (dchan == DBRI_CHAN_NONE || dchan == DBRI_CHAN_HOST ||
	    dchan == DBRI_CHAN_FIXED) {
		ATRACE(dbri_set_format, 'BGUS', 0);
		return (B_FALSE);
	}

	ds = DChanToDs(unitp, dchan);

	if (dfmt == NULL) {
		if ((ds->dfmt == NULL) && !dbri_set_default_format(ds)) {
			ATRACE(dbri_set_format, '!set', 0);
			return (B_FALSE);
		}
		dfmt = ds->dfmt;
	} else {
		/*
		 * This has the useful side-effect of making the format
		 * of a serial<->serial connection the default format
		 * for any monitor pipe connections created afterwards.
		 */
		ds->dfmt = dfmt;
	}
	ATRACE(dbri_set_format, 'dfmt', dfmt);

	/*
	 * Apply the format to the aud_stream_t
	 */
/*
 * REMIND: stoltz 1992/08/XX - Need isdn to dbri format converter
 * This should be a call somewhere in dbri_conf.c to translate the
 * generic isdn format into a dbri format appropriate for the channel.
 */
	/*
	 * In the case of MMCODEC, this is the first of
	 * many places where the as's format is set.
	 */
	DsToAs(ds)->info.sample_rate = dfmt->lsamplerate;
	DsToAs(ds)->info.channels = dfmt->channels;
	DsToAs(ds)->info.precision = (int)dfmt->llength / (int)dfmt->channels;
	DsToAs(ds)->info.encoding = dfmt->encoding;
	DsToAs(ds)->mode = (dfmt->mode == DBRI_MODE_TRANSPARENT)
	    ? AUDMODE_AUDIO : AUDMODE_HDLC;

	ATRACE(dbri_set_format, 'len ', dfmt->length);
	ATRACE(dbri_set_format, 'samp', dfmt->samplerate);
	ATRACE(dbri_set_format, 'chan', dfmt->channels);

	return (B_TRUE);
} /* dbri_set_format */


/*
 * dbri_mark_channel_busy - if the argument is TRUE, then mark the
 * channel as busy.  If the physical channel spans multiple logical
 * channels (B1 configured as an H channel spans B1 and B2), then also
 * mark the associated channels.  If already_busy is true, then we don't
 * have to mark the primary channel as busy but we will have to mark
 * the secondary channel (B2 if B1 is an H channel) as busy (if it
 * already is, then fail).
 */
boolean_t
dbri_mark_channel_busy(dbri_unit_t *unitp, dbri_chan_t dchan,
	dbri_chanformat_t *dfmt, boolean_t arg, boolean_t already_busy)
{
	dbri_chan_t dchan2;
	dbri_stream_t *tds, *tds2;	/* target dbri_streams */

	ATRACE(dbri_mark_channel_busy, 'dchn', dchan);
	if (dchan == DBRI_CHAN_HOST)
		return (B_TRUE);

	tds = DChanToDs(unitp, dchan);

	ATRACE(dbri_mark_channel_busy, 'open', DsToAs(tds)->info.open);

	dchan2 = DBRI_CHAN_NONE;

	if (arg == B_TRUE) {
		if (DsToAs(tds)->info.open && !already_busy) {
			ATRACE(dbri_mark_channel_busy, 'opn!', 0);
			return (B_FALSE);
		}

		/*
		 * Currently the only special case here is a B1 channel
		 * configured as an H-channel.  In this case, also mark the
		 * adjacent B2 channel as busy.
		 */
		switch (dchan) {
		case DBRI_TE_B1_IN:
			if (dfmt->length > 8)
				dchan2 = DBRI_TE_B2_IN;
			break;

		case DBRI_TE_B1_OUT:
			if (dfmt->length > 8)
				dchan2 = DBRI_TE_B2_OUT;
			break;

		case DBRI_NT_B1_IN:
			if (dfmt->length > 8)
				dchan2 = DBRI_NT_B2_IN;
			break;

		case DBRI_NT_B1_OUT:
			if (dfmt->length > 8)
				dchan2 = DBRI_NT_B2_OUT;
			break;

		default:
			break;
		} /* switch on dbri channel */

		if (dchan2 != DBRI_CHAN_NONE) {
			ATRACE(dbri_mark_channel_busy, 'othr', 0);
			ATRACE(dbri_mark_channel_busy, 'dchn', dchan2);
			tds2 = DChanToDs(unitp, dchan2);

			/*
			 * It's always an error if the adjacent channel is
			 * busy.
			 */
			if (DsToAs(tds2)->info.open) {
				if (!already_busy)
					DsToAs(tds)->info.open = B_FALSE;
				ATRACE(dbri_mark_channel_busy, 'opn!', 0);
				return (B_FALSE);
			}

			/*
			 * Mark the adjacent channel as busy.
			 */
			DsToAs(tds2)->info.open = B_TRUE;
		}

		/*
		 * Mark the channel as busy
		 */
		DsToAs(tds)->info.open = B_TRUE;

		return (B_TRUE);
	} else {
		/*
		 * Currently the only special case here is a B1 channel
		 * configured as an H-channel.  In this case, also mark the
		 * adjacent B2 channel as busy.
		 */
		switch (dchan) {
		case DBRI_TE_B1_IN:
			if (dfmt->length > 8)
				dchan2 = DBRI_TE_B2_IN;
			break;

		case DBRI_TE_B1_OUT:
			if (dfmt->length > 8)
				dchan2 = DBRI_TE_B2_OUT;
			break;

		case DBRI_NT_B1_IN:
			if (dfmt->length > 8)
				dchan2 = DBRI_NT_B2_IN;
			break;

		case DBRI_NT_B1_OUT:
			if (dfmt->length > 8)
				dchan2 = DBRI_NT_B2_OUT;
			break;

		default:
			dchan2 = DBRI_CHAN_NONE;
			break;
		} /* switch on dbri channel */

		if (dchan2 != DBRI_CHAN_NONE) {
			tds2 = DChanToDs(unitp, dchan2);

			/*
			 * Mark the adjacent channel as not busy.
			 */
			DsToAs(tds2)->info.open = B_FALSE;
			cv_signal(&DsToAs(tds2)->control_as->cv);
		}

		/*
		 * Mark the channel as not busy
		 */
		if (!already_busy) {
			DsToAs(tds)->info.open = B_FALSE;
			cv_signal(&DsToAs(tds)->control_as->cv);
		}

		/*
		 * Notify the controlling entity that there has
		 * been a state change.
		 */
		audio_sendsig(DsToAs(tds), AUDIO_SENDSIG_ALL);

		return (B_TRUE);
	}
}


/*
 * dbri_mark_channels_busy - if the argument is TRUE, then mark all
 * channels associated with the connection request as "busy".  This
 * will fail if any are already busy.  If the argument is FALSE, then
 * mark the channels as not "busy".
 *
 * NB: this will mark a B2 channel as busy if the B1 channel being
 * used will use the B2 channel because it is 16-bits long.
 */
boolean_t
dbri_mark_channels_busy(dbri_unit_t *unitp, dbri_conn_request_t *dcrp,
	boolean_t arg)
{
	dbri_conn_oneway_t *dcrp1, *dcrp2;

	ASSERT(unitp != NULL);
	ASSERT(dcrp != NULL);

	dcrp1 = &dcrp->oneway;
	dcrp2 = &dcrp->otherway;

	/*
	 * Mark the channels we are using as busy.
	 */
	if (dcrp->oneway_valid) {
		if (!dbri_mark_channel_busy(unitp, dcrp1->source.dchan,
		    &dcrp1->format, arg, dcrp->already_busy)) {
			return (B_FALSE);
		}
		if (!dbri_mark_channel_busy(unitp, dcrp1->sink.dchan,
		    &dcrp1->format, arg, dcrp->already_busy)) {
			(void) dbri_mark_channel_busy(unitp,
			    dcrp1->source.dchan, &dcrp1->format, B_FALSE,
			    dcrp->already_busy);
			return (B_FALSE);
		}
	}
	if (dcrp->otherway_valid) {
		if (!dbri_mark_channel_busy(unitp, dcrp2->source.dchan,
		    &dcrp2->format, arg, dcrp->already_busy)) {
			if (dcrp->oneway_valid) {
				(void) dbri_mark_channel_busy(unitp,
				    dcrp1->source.dchan, &dcrp1->format,
				    B_FALSE, dcrp->already_busy);
				(void) dbri_mark_channel_busy(unitp,
				    dcrp1->sink.dchan, &dcrp1->format,
				    B_FALSE, dcrp->already_busy);
			}
			return (B_FALSE);
		}
		if (!dbri_mark_channel_busy(unitp, dcrp2->sink.dchan,
		    &dcrp2->format, arg, dcrp->already_busy)) {
			if (dcrp->oneway_valid) {
				(void) dbri_mark_channel_busy(unitp,
				    dcrp1->source.dchan, &dcrp1->format,
				    B_FALSE, dcrp->already_busy);
				(void) dbri_mark_channel_busy(unitp,
				    dcrp1->sink.dchan, &dcrp1->format,
				    B_FALSE, dcrp->already_busy);
			}
			(void) dbri_mark_channel_busy(unitp,
			    dcrp2->source.dchan,
			    &dcrp1->format, B_FALSE, dcrp->already_busy);
			return (B_FALSE);
		}
	}

	return (B_TRUE);
}


/*
 * dbri_pipe_configure - Given a unidirectional dbri connection
 * structure, call the necessary routines to set up the SDP and DTS
 * commands as well as issue the initial SDP to clear the pipe.
 */
void
dbri_pipe_configure(dbri_unit_t *unitp, dbri_conn_oneway_t *dcrp)
{
	ASSERT(dcrp != NULL);
	ASSERT(dcrp->pipep != NULL);

	/*
	 * In the event that this is not a serial-to-serial connection,
	 * we should use the information from the serial side of the
	 * connection.
	 */
	if (dcrp->sink.dchan == DBRI_CHAN_HOST) {
		dbri_setup_sdp(unitp, dcrp->pipep, dcrp->source.ctep);
		if (!ISPIPEINUSE(dcrp->pipep))
			dbri_setup_dts(unitp, dcrp->pipep, dcrp->source.ctep);
	} else {
		dbri_setup_sdp(unitp, dcrp->pipep, dcrp->sink.ctep);
		dbri_setup_dts(unitp, dcrp->pipep, dcrp->sink.ctep);
		/*
		 * SERIAL-SERIAL requires two dbri_setup_dts calls
		 */
		if (dcrp->source.dchan != DBRI_CHAN_HOST &&
		    !ISPIPEINUSE(dcrp->pipep)) {
			dbri_setup_dts(unitp, dcrp->pipep, dcrp->source.ctep);
		}
	}
}


/*
 * dbri_connection_cleanup - XXX
 */
boolean_t
dbri_connection_cleanup(dbri_unit_t *unitp, dbri_conn_oneway_t *dcrp)
{
	dbri_bri_status_t *bs;

	ASSERT(dcrp != NULL);
	ASSERT(dcrp->pipep != NULL);

	ASSERT_UNITLOCKED(unitp);

	ATRACE(dbri_connection_cleanup, 'pipe', PIPENO(dcrp->pipep));

	/*
	 * if this is a base pipe disable the serial interface
	 */
	switch (PIPENO(dcrp->pipep)) {
	case DBRI_PIPE_TE_D_IN:
	case DBRI_PIPE_TE_D_OUT:
		bs = DBRI_TE_STATUS_P(unitp);

		dbri_te_unplug(unitp);
		drv_usecwait(250); /* XXX - delay for nice exit */

		if (unitp->initialized)	/* Disable interface */
			dbri_disable_te(unitp);

		break;

	case DBRI_PIPE_NT_D_IN:
	case DBRI_PIPE_NT_D_OUT:
		bs = DBRI_NT_STATUS_P(unitp);

		/*
		 * Disable NT serial interface
		 */
		dbri_disable_nt(unitp);
		bs->sbri.tss = DBRI_NTINFO0_G1;
		dbri_hold_g1(unitp);
/*
 * REMIND: stoltz YYYY/XX/XX - should relay control be in dbri_disable_nt?
 */
		unitp->regsp->n.sts |= DBRI_STS_F;
		bs->sbri.tss = DBRI_NTINFO0_G1;
		bs->i_info.activation = ISDN_DEACTIVATED;

		/*
		 * If NT D-channels not in use then kill alive timer and
		 * switch in relays.  We don't have to call untimeout
		 * (which would introduce the possibilty of deadlock)
		 * since we can just reset unitp->keep_alive_running.
		 */
		unitp->keep_alive_running = B_FALSE;
		break;

	default:
		break;
	} /* switch on pipe */

	/*
	 * Remove the pipe from the timeslot rings
	 */
	(void) dbri_pipe_remove(unitp, dcrp->pipep);

	if (dcrp->ds != NULL) {
		dcrp->ds->pipep = NULL;
		DsToAs(dcrp->ds)->mode = AUDMODE_NONE;
	}

	return (B_TRUE);
} /* dbri_connection_cleanup */


/*
 * dbri_isdn_connection_setup - XXX
 */
boolean_t
dbri_isdn_connection_setup(dbri_unit_t *unitp, isdn_chan_t source,
	isdn_chan_t sink, isdn_format_t *ifp, dbri_conn_oneway_t *dcrp)
{
	dbri_chan_t dchan1, dchan2;
	dbri_chantab_t *ctep1, *ctep2;
	dbri_chanformat_t *dfmt;

	/*
	 * Convert ISDN channels to DBRI channels
	 */
	dchan1 = dbri_itod_chan(source, DBRI_DIRECTION_IN);
	dchan2 = dbri_itod_chan(sink, DBRI_DIRECTION_OUT);
	ATRACE(dbri_isdn_connection_setup, 'dchn', dchan1);
	ATRACE(dbri_isdn_connection_setup, 'dchn', dchan2);
	if (dchan1 == DBRI_CHAN_NONE || dchan2 == DBRI_CHAN_NONE) {
		ATRACE(dbri_isdn_connection_setup, '!chn', 0);
		return (B_FALSE);
	}

	ctep1 = dbri_dchan_to_ctep(dchan1);
	ctep2 = dbri_dchan_to_ctep(dchan2);
	if (ctep1 == NULL || ctep2 == NULL) {
		ATRACE(dbri_isdn_connection_setup, '!ctp', 0);
		return (B_FALSE);
	}

	dfmt = dbri_itod_fmt(unitp, ctep1, ifp);
	if (dfmt == NULL || !dbri_set_format(unitp, dchan1, dfmt)) {
		ATRACE(dbri_isdn_connection_setup, '!fmt', 0);
		return (B_FALSE);
	}
	dfmt = dbri_itod_fmt(unitp, ctep2, ifp);
	if (dfmt == NULL || !dbri_set_format(unitp, dchan2, dfmt)) {
		ATRACE(dbri_isdn_connection_setup, '!fmt', 1);
		return (B_FALSE);
	}

	dcrp->ds = NULL;	/* no data stream */
	dcrp->source.dchan = dchan1;
	dcrp->sink.dchan = dchan2;
	dcrp->source.ctep = ctep1;
	dcrp->sink.ctep = ctep2;
	dcrp->format = *dfmt;

	return (B_TRUE);
} /* dbri_isdn_connection_setup */
