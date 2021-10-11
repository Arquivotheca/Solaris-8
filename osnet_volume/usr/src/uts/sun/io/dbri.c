/*
 * Copyright (c) 1992, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dbri.c	1.50	98/07/06 SMI"

/*
 * Sun DBRI Device-Independent Audio interface routines
 */

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/ioccom.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/file.h>
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
 * Local function prototypes
 */
static void dbri_close(aud_stream_t *);
static void dbri_flushas(aud_stream_t *);
static aud_return_t dbri_ioctl(aud_stream_t *, queue_t *, mblk_t *);
static aud_return_t dbri_mproto(aud_stream_t *, mblk_t *);
static uint_t dbri_setflag(aud_stream_t *, enum aud_opflag, uint_t);
static void dbri_update_smpl_count(dbri_stream_t *);
static aud_return_t dbri_setinfo(aud_stream_t *, mblk_t *, int *);
static void dbri_queuecmd(aud_stream_t *, aud_cmd_t *);
static void dbri_txqcmd(aud_stream_t *, aud_cmd_t *);
static void dbri_rxqcmd(aud_stream_t *, aud_cmd_t *);
static boolean_t dbri_loopback(dbri_stream_t *,
    isdn_loopback_request_t *, int);
static aud_stream_t *dbri_minortoas(dbri_unit_t *, int);


/*
 * Global variables
 */
aud_ops_t dbri_audio_ops = {
	dbri_close, dbri_ioctl, dbri_mproto, dbri_start, dbri_stop,
	dbri_setflag, dbri_setinfo, dbri_queuecmd, dbri_flushas
};


/*
 * dbri_open - Set device structure ptr and call generic routine.
 */
/*ARGSUSED*/
int
dbri_open(queue_t *q, dev_t *devp, int oflag, int sflag, cred_t *credp)
{
	dbri_unit_t *unitp;
	aud_stream_t *as;
	aud_stream_t *tas;
	int instance;
	int i;
	int error;

	instance = MINORUNIT(*devp);
	ATRACE(dbri_open, 'inst', instance);

	unitp = (dbri_unit_t *)ddi_get_soft_state(Dbri_state, instance);
	if (unitp == NULL)
		return (ENODEV);

	LOCK_UNIT(unitp);

	Dbri_keepcnt++;		/* Module in use... */

	if (unitp->openinhibit) {
		error = ENXIO;
		goto failed;
	}

	/*
	 * If there are still open streams and initchip is B_FALSE this
	 * means that we have done a reset of DBRI. Do not allow any more
	 * streams to open the device until the last of the original
	 * streams finally closes the device.
	 */
	if (unitp->initialized == B_FALSE) {
		boolean_t busy = B_FALSE;

		for (i = 0; i < Dbri_nstreams; i++) {
			as = DChanToAs(unitp, i);
			if (DsDisabled(AsToDs(as)))
				continue;

			if (as->info.open) {
				busy = B_TRUE;
				break;
			}
		}
		if (busy == B_TRUE) {
			error = EBUSY;
			goto failed;
		}
	}

	ATRACE(dbri_open, 'minr', getminor(*devp));

	if (getminor(*devp) == 0) {
		/*
		 * Because the system temporarily uses the streamhead
		 * corresponding to major,0 during a clone open, and
		 * because audio_open can sleep, audio drivers are not
		 * allowed to use major,0 as a valid device number.
		 *
		 * A sleeping clone open and a non-close of major,0
		 * can mess up the reference counts on vnodes/snodes.
		 */
		ATRACE(dbri_open, 'min0', 0);
		error = ENODEV;
		goto failed;
	}

	/*
	 * Check for legal device.
	 */
	if ((as = dbri_minortoas(unitp, MINORDEV(*devp))) == NULL) {
		ATRACE(dbri_open, '!as ', MINORDEV(*devp));
		error = ENODEV;
		goto failed;
	}

	/*
	 * "as" is not the output_as if the aud_stream is a read-only
	 * data device.
	 */
	if (ISDATASTREAM(as) && ((oflag & (FREAD|FWRITE)) == FREAD)) {
		*devp = dbrimakedev(getmajor(*devp), instance,
		    as->input_as->info.minordev);
		if ((as = dbri_minortoas(unitp, MINORDEV(*devp))) == NULL) {
			ATRACE(dbri_open, '!as2', MINORDEV(*devp));
			error = ENODEV;
			goto failed;
		}
	}

	/*
	 * If this is a datastream, then we self-clone it
	 */
	if (ISDATASTREAM(as)) {
		*devp = dbrimakeclonedev(getmajor(*devp), instance,
		    as->info.minordev);
		sflag = CLONEOPEN;
	}

	/*
	 * audio_open now returns the play or record "as" we're interested in
	 * but if a device is opened r/w then the play "as" is returned.
	 */
	error = audio_open(as, q, devp, oflag, sflag);
	if (error != 0) {
		ATRACE(dbri_open, 'WHY?', as);
		goto failed;
	}

	ASSERT(as->writeq != NULL);

	/*
	 * Must call audio_close now if anything fails ...
	 */

	/*
	 * Connect the hardware for a data stream
	 */
	if (ISDATASTREAM(as)) {
		dbri_stream_t *input;
		dbri_stream_t *output;

		/*
		 * Set input and output pointer per r/w flags.
		 * If the channel does not have a format set, use the
		 * channel's default format.
		 */
		if (oflag & FREAD) {
			input = AsToDs(as->input_as);
			if (!dbri_set_default_format(input)) {
				ATRACE(dbri_open, 'WHY?', as);
				UNLOCK_UNIT(unitp);
				(void) audio_close(as->writeq, 0, NULL);
				Dbri_keepcnt--;
				return (ENODEV);
			}
		} else {
			input = NULL;
		}

		if (oflag & FWRITE) {
			output = AsToDs(as->output_as);
			if (!dbri_set_default_format(output)) {
				ATRACE(dbri_open, 'WHY?', as);
				UNLOCK_UNIT(unitp);
				(void) audio_close(as->writeq, 0, NULL);
				Dbri_keepcnt--;
				return (ENODEV);
			}
		} else {
			output = NULL;
		}

		/*
		 * Setup the hardware and connect an IO channel to the stream.
		 */
		if (!dbri_stream_connection_create(AsToDs(as), oflag,
		    &error)) {
			ATRACE(dbri_open, '!out', as);
			UNLOCK_UNIT(unitp);
			(void) audio_close(as->writeq, 0, NULL);
			return (error);
		}

		/*
		 * Clear the counters for the dbri_streams
		 */
		if (output != NULL) {
			bzero((caddr_t)&output->iostats,
			    sizeof (output->iostats));
		}
		if (input != NULL) {
			bzero((caddr_t)&input->iostats,
			    sizeof (input->iostats));
		}

		if (input != NULL)
			audio_process_input(DsToAs(input));

	} else {
		switch (as->info.minordev) {
		case DBRI_MINOR_TE_D_TRACE:
			if (DChanToAs(unitp, DBRI_TE_D_OUT)->traceq != NULL) {
				UNLOCK_UNIT(unitp);
				Dbri_keepcnt--;
				return (EBUSY);
			}
			tas = DChanToAs(unitp, DBRI_TE_D_OUT);
			tas->input_as->traceq = as->readq;
			tas->output_as->traceq = as->readq;
			tas->control_as->traceq = as->readq;
			break;

		case DBRI_MINOR_NT_D_TRACE:
			if (DChanToAs(unitp, DBRI_NT_D_OUT)->traceq != NULL) {
				UNLOCK_UNIT(unitp);
				Dbri_keepcnt--;
				return (EBUSY);
			}
			tas = DChanToAs(unitp, DBRI_NT_D_OUT);
			tas->input_as->traceq = as->readq;
			tas->output_as->traceq = as->readq;
			tas->control_as->traceq = as->readq;
			break;

		default:
			break;
		}
	}

	ASSERT(q->q_ptr != NULL);

	ATRACE(dbri_open, '  as', as);

	UNLOCK_UNIT(unitp);
	return (0);

failed:
	Dbri_keepcnt--;

	ATRACE(dbri_open, 'oerr', error);
	UNLOCK_UNIT(unitp);
	return (error);
} /* dbri_open */


/*
 * dbri_close - DD close routine.  Called from DI with mutex held.
 */
static void
dbri_close(aud_stream_t *as)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	aud_stream_t *tas;

	ASSERT_ASLOCKED(as);

	/*
	 * Only data streams need be disconnected from the hardware
	 */
	if (ISDATASTREAM(as)) {
		/*
		 * If we are closing the input side of the stream, then
		 * reset the input buffer_size to the default value.
		 *
		 * XXX - Verify the following test for the above
		 * conditions.
		 */

		/*
		 * Reset private state
		 */
		AsToDs(as)->samples = 0;
		AsToDs(as)->audio_uflow = B_FALSE;
		AsToDs(as)->last_flow_error = B_FALSE; /* XXX - DBRI bug#3034 */
		timerclear(&AsToDs(as)->last_smpl_update);

		/*
		 * XXX - ISDN stats are cleared on open... perhaps
		 * we should make that the rule?
		 */
		bzero((caddr_t)&AsToDs(as)->d_stats,
		    sizeof (AsToDs(as)->d_stats));

		/*
		 * Disassociate the stream from the hardware.
		 */
		(void) dbri_stream_connection_destroy(AsToDs(as));

	} else {
		switch (as->info.minordev) {
		case DBRI_MINOR_TE_D_TRACE:
			tas = DChanToAs(unitp, DBRI_TE_D_OUT);
			tas->input_as->traceq = NULL;
			tas->output_as->traceq = NULL;
			tas->control_as->traceq = NULL;
			break;

		case DBRI_MINOR_NT_D_TRACE:
			tas = DChanToAs(unitp, DBRI_NT_D_OUT);
			tas->input_as->traceq = NULL;
			tas->output_as->traceq = NULL;
			tas->control_as->traceq = NULL;
			break;

		default:
			break;
		}
	}

	ATRACE(dbri_close, 'CLOS', as);
	Dbri_keepcnt--;
} /* dbri_close */


/*
 * Process ioctls not already handled by the generic audio handler.
 */
static aud_return_t
dbri_ioctl(aud_stream_t *as, queue_t *q, mblk_t *mp)
{
	dbri_unit_t *unitp = AsToUnitp(as);
	dbri_stream_t *ds;
	aud_return_t change;
	int error;
	long state;
	int cmd;

	ASSERT_ASLOCKED(as);

	/*
	 * Default is no change
	 */
	change = AUDRETURN_NOCHANGE;
	error = 0;

	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		{
		struct iocblk *iocp;

		iocp = (struct iocblk *)(void *)mp->b_rptr;
		cmd = iocp->ioc_cmd;
		state = 0;	/* initial state */
		break;
		}

	case M_IOCDATA:
		{
		struct copyresp *csp;

		csp = (struct copyresp *)(void *)mp->b_rptr;
		if (csp->cp_rval != 0) { /* error, just return */
			freemsg(mp);
			goto done;
		}
		cmd = csp->cp_cmd;
		state = (long)csp->cp_private;
		break;
		}

	default:
		state = (long)0;
		cmd = 0;
	}

	ATRACE(dbri_ioctl, 'ioct', cmd);
	ds = AsToDs(as);

	switch (cmd) {
	case AUDIO_GETDEV:
	{
		caddr_t uaddr;
		audio_device_t *devtp;

		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;
		freemsg(mp->b_cont);
		mp->b_cont = allocb(sizeof (audio_device_t), BPRI_MED);
		if (mp->b_cont == NULL) {
			audio_ack(q, mp, ENOSR);
			goto done;
		}

		devtp = (audio_device_t *)(void *)mp->b_cont->b_rptr;
		mp->b_cont->b_wptr += sizeof (audio_device_t);
		(void) strcpy(devtp->name, DBRI_DEV_NAME);
		devtp->version[0] = unitp->version;
		devtp->version[1] = '\0';
		(void) strcpy(devtp->config, ds->config);

		audio_copyout(q, mp, uaddr, sizeof (audio_device_t));
		break;
	}

	case ISDN_PH_ACTIVATE_REQ: {
		dbri_bri_status_t *bs;

		if ((as != DChanToAs(unitp, DBRI_TEMGT)) &&
		    (as != DChanToAs(unitp, DBRI_NTMGT))) {
			audio_ack(q, mp, EINVAL);
			break;
		}

		if (!ISHWCONNECTED(as->output_as)) {
			ATRACE(dbri_ioctl, '!hwc', as->output_as);
			audio_ack(q, mp, ENXIO);
			break;
		}

		if (AsToDs(as)->ctep->isdntype == ISDN_TYPE_TE) {
			bs = DBRI_TE_STATUS_P(unitp);
		} else if (AsToDs(as)->ctep->isdntype == ISDN_TYPE_NT) {
			bs = DBRI_NT_STATUS_P(unitp);
		} else {
			ATRACE(dbri_ioctl, '!bri', AsToDs(as)->ctep->isdntype);
			audio_ack(q, mp, EINVAL);
			break;
		}

		if (bs->i_var.power != ISDN_PARAM_POWER_ON) {
			ATRACE(dbri_ioctl, '!pow', AsToDs(as)->ctep->isdntype);
			audio_ack(q, mp, ENXIO);
			break;
		}

		switch (AsToDs(as)->ctep->isdntype) {
		case ISDN_TYPE_TE:
			dbri_te_ph_activate_req(as); /* Activate TE */
			audio_ack(q, mp, 0);
			break;

		case ISDN_TYPE_NT:
			dbri_nt_ph_activate_req(as); /* Activate NT */
			audio_ack(q, mp, 0);
			break;

		default:
			audio_ack(q, mp, EINVAL);
			break;
		}

		break;
	}

	case ISDN_MPH_DEACTIVATE_REQ: {
		dbri_bri_status_t *bs;

		if (as == DChanToAs(unitp, DBRI_NTMGT)) {
			if (!ISHWCONNECTED(as->output_as)) {
				audio_ack(q, mp, EINVAL);
				break;
			}

			bs = DBRI_NT_STATUS_P(unitp);
			if (bs->i_var.power != ISDN_PARAM_POWER_ON) {
				audio_ack(q, mp, ENXIO);
				break;
			}
			dbri_nt_mph_deactivate_req(unitp);
			audio_ack(q, mp, 0);
		} else {
			/*
			 * There is no such thing as PH_DEACTIVATE_REQ
			 * for TE's.
			 */
			audio_ack(q, mp, EINVAL);
		}
		break;
	}

	case ISDN_ACTIVATION_STATUS: /* get ISDN status */
	{
		dbri_bri_status_t *bs;
		isdn_activation_status_t *is;

		switch (state) {
		case 0:
			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_activation_status_t));
			return (AUDRETURN_NOCHANGE);

		default:
			is = (isdn_activation_status_t *)(void *)
			    mp->b_cont->b_rptr;

			if (is->type == ISDN_TYPE_SELF)
				is->type = AsToDs(as)->ctep->isdntype;

			switch (is->type) {
			case ISDN_TYPE_TE:
				bs = DBRI_TE_STATUS_P(unitp);
				break;

			case ISDN_TYPE_NT:
				bs = DBRI_NT_STATUS_P(unitp);
				break;

			default:
				audio_ack(q, mp, EINVAL);
				goto done;
			} /* switch on ISDN status type */

			is->activation = bs->i_info.activation;

			audio_copyout(q, mp, (caddr_t)state,
			    sizeof (isdn_activation_status_t));
			break;
		} /* switch on ioctl state */
		break;
	}

	case ISDN_CHANNEL_STATUS: /* get per-channel ISDN status */
	{
		isdn_channel_info_t *ici;
		dbri_chan_t dchan;
		dbri_stream_t *tds;

		switch (state) {
		case 0:
			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_channel_info_t));
			return (AUDRETURN_NOCHANGE);

		default:
			ici = (isdn_channel_info_t *)(void *)mp->b_cont->b_rptr;

			if (ici->channel == ISDN_CHAN_SELF)
				ici->channel = AsToDs(as)->ctep->ichan;

			if (!dbri_isowner(AsToDs(as), ici->channel)) {
				audio_ack(q, mp, EPERM);
				goto done;
			}

			dchan = dbri_itod_chan(ici->channel,
			    DBRI_DIRECTION_OUT);
			if (dchan == DBRI_CHAN_NONE) {
				audio_ack(q, mp, EINVAL);
				goto done;
			}
			tds = DChanToDs(unitp, dchan);

			if (DsToAs(tds)->info.open) {
				if (DsToAs(tds)->info.pause)
					ici->iostate = ISDN_IO_STOPPED;
				else
					ici->iostate = ISDN_IO_READY;
			} else {
				ici->iostate = ISDN_IO_UNKNOWN;
			}

			ici->transmit.packets =
			    AsToDs(DsToAs(tds)->output_as)->iostats.packets;
			ici->transmit.octets =
			    AsToDs(DsToAs(tds)->output_as)->iostats.octets;
			ici->transmit.errors =
			    AsToDs(DsToAs(tds)->output_as)->iostats.errors;
			ici->receive.packets =
			    AsToDs(DsToAs(tds)->input_as)->iostats.packets;
			ici->receive.octets =
			    AsToDs(DsToAs(tds)->input_as)->iostats.octets;
			ici->receive.errors =
			    AsToDs(DsToAs(tds)->input_as)->iostats.errors;

			audio_copyout(q, mp, (caddr_t)state,
			    sizeof (isdn_channel_info_t));
			break;

		} /* switch on ioctl state */
		break;
	}

	case ISDN_INTERFACE_STATUS:	/* get per-interface ISDN status */
	{
		isdn_interface_info_t *iii;
		dbri_bri_status_t *bs;

		switch (state) {
		case 0:
			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_interface_info_t));
			return (AUDRETURN_NOCHANGE);

		default:
			iii = (isdn_interface_info_t *)(void *)
			    mp->b_cont->b_rptr;

			if (iii->interface == ISDN_TYPE_SELF)
				iii->interface = AsToDs(as)->ctep->isdntype;

			switch (iii->interface) {
			case ISDN_TYPE_TE:
				bs = DBRI_TE_STATUS_P(unitp);
				break;

			case ISDN_TYPE_NT:
				bs = DBRI_NT_STATUS_P(unitp);
				break;

			default:
				audio_ack(q, mp, EINVAL);
				goto done;
			}

			iii->activation = bs->i_info.activation;
			iii->ph_ai = bs->i_info.ph_ai;
			iii->ph_di = bs->i_info.ph_di;
			iii->mph_ai = bs->i_info.mph_ai;
			iii->mph_di = bs->i_info.mph_di;
			iii->mph_ei1 = bs->i_info.mph_ei1;
			iii->mph_ei2 = bs->i_info.mph_ei2;
			iii->mph_ii_c = bs->i_info.mph_ii_c;
			iii->mph_ii_d = bs->i_info.mph_ii_d;

			audio_copyout(q, mp, (caddr_t)state,
			    sizeof (isdn_interface_info_t));
			break;

		} /* switch on ioctl state */
		break;
	}

	case ISDN_SET_LOOPBACK:
	case ISDN_RESET_LOOPBACK:
	{
		isdn_loopback_request_t *lr;
		int flag;

		/*
		 * Restrict access to mgt and d channels only
		 */
		switch (AsToDs(as)->ctep->ichan) {
		case ISDN_CHAN_TE_D:
		case ISDN_CHAN_TE_MGT:
		case ISDN_CHAN_NT_D:
		case ISDN_CHAN_NT_MGT:
			break;

		default:
			audio_ack(q, mp, EINVAL);
			goto done;
		}

		switch (state) {
		case 0:
			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_loopback_request_t));
			return (AUDRETURN_NOCHANGE);

		default:
			lr = (isdn_loopback_request_t *)(void *)
			    mp->b_cont->b_rptr;

			flag = (cmd == ISDN_SET_LOOPBACK) ? 0 : 1;

			if (!dbri_loopback(ds, lr, flag))
				audio_ack(q, mp, EINVAL);
			else
				audio_ack(q, mp, 0);
			break;
		} /* switch on ioctl state */
		break;
	}

	case ISDN_PARAM_SET:
	case ISDN_PARAM_GET:
	{
		isdn_param_t *iphp;

		switch (state) {
		case 0:
			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_param_t));
			return (AUDRETURN_NOCHANGE);

		default:
			iphp = (isdn_param_t *)(void *)mp->b_cont->b_rptr;
			if (cmd == ISDN_PARAM_SET) {
				if (!dbri_isdn_set_param(as, iphp, &change,
				    &error)) {
					audio_ack(q, mp, error);
				} else {
					audio_ack(q, mp, 0);
				}
			} else {
				if (!dbri_isdn_get_param(as, iphp, &change,
				    &error)) {
					audio_ack(q, mp, error);
				} else {
					audio_copyout(q, mp, (caddr_t)state,
					    sizeof (isdn_param_t));
				}
			}
			return (change);
		} /* switch on ioctl state */
	}

	case ISDN_GET_FORMAT:
	case ISDN_SET_FORMAT:
	{
		isdn_format_req_t *ifrp;

		switch (state) {
		case 0:
			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_format_req_t));
			return (AUDRETURN_NOCHANGE);

		default:
			ifrp = (isdn_format_req_t *)(void *)mp->b_cont->b_rptr;
			if (cmd == ISDN_SET_FORMAT) {
				if (!dbri_isdn_set_format(ds, ifrp,
				    &change, &error)) {
					audio_ack(q, mp, error);
					break;
				}

			} else {
				if (!dbri_isdn_get_format(AsToDs(as), ifrp,
				    &error)) {
					audio_ack(q, mp, error);
					break;
				}
			}

			audio_copyout(q, mp, (caddr_t)state,
			    sizeof (isdn_format_req_t));
			return (change);
		} /* switch on ioctl state */
		return (change);
	}

	case ISDN_SET_CHANNEL:
	{
		isdn_conn_req_t	*icrp;

		switch (state) {
		case 0:
			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_conn_req_t));
			return (AUDRETURN_NOCHANGE);

		default:
			icrp = (isdn_conn_req_t *)(void *)mp->b_cont->b_rptr;
			switch (icrp->dir) {
			case ISDN_PATH_NOCHANGE:
				audio_ack(q, mp, 0);
				break;

			case ISDN_PATH_DISCONNECT:
				if (!dbri_isdn_connection_destroy(AsToDs(as),
				    icrp, &change, &error))
					audio_ack(q, mp, error);
				else
					audio_ack(q, mp, 0);
				break;

			case ISDN_PATH_ONEWAY:
			case ISDN_PATH_TWOWAY:
				if (!dbri_isdn_connection_create(AsToDs(as),
				    icrp, &change, &error))
					audio_ack(q, mp, error);
				else
					audio_ack(q, mp, 0);
				break;

			default:
				audio_ack(q, mp, EINVAL);
				break;
			} /* switch on path type */

			return (change);
		} /* switch on ioctl state */
	}

	case ISDN_GET_CONFIG:
	{
		isdn_conn_tab_t	*ictp;

		switch (state) {
		case 0:
			ATRACE(dbri_ioctl, 'gcci', 1);
			/*
			 * Save address where we need to copyout the
			 * conn_tab structure in the final step.
			 */
			as->dioctl.priv = (ulong_t)
			    *(caddr_t *)(void *)mp->b_cont->b_rptr;

			audio_copyin(q, mp,
			    *(caddr_t *)(void *)mp->b_cont->b_rptr,
			    sizeof (isdn_conn_tab_t));
			return (AUDRETURN_NOCHANGE);

		default:
			ATRACE(dbri_ioctl, 'gcco', 2);
			ictp = (isdn_conn_tab_t *)(void *)mp->b_cont->b_rptr;

			if (!dbri_isdn_get_config(AsToDs(as), ictp))
				audio_ack(q, mp, EINVAL);

			as->dioctl.mp = mp;

			return (AUDRETURN_NOCHANGE);

		case 1:		/* first copyout done, new perform second */
			ATRACE(dbri_ioctl, 'gcco', 3);
			freemsg(mp);
			mp = as->dioctl.mp;
			as->dioctl.mp = NULL; /* XXX */

			audio_copyout(q, mp, (caddr_t)as->dioctl.priv,
			    sizeof (isdn_conn_tab_t));
			return (AUDRETURN_NOCHANGE);
		} /* switch on ioctl state */
	}

	default:
		audio_ack(q, mp, EINVAL);
		break;
	} /* switch on ioctl type */

done:
	return (change);
} /* dbri_ioctl */


/*
 * dbri_mproto - handle synchronous M_PROTO messages
 */
/*ARGSUSED*/
static aud_return_t
dbri_mproto(aud_stream_t *as, mblk_t *mp)
{
	if (mp != NULL)
		freemsg(mp);
	return (AUDRETURN_NOCHANGE);
} /* dbri_mproto */


/*
 * dbri_start - used to start/resume reads or writes for an entire list.
 *
 * NOTE: This is not called to CONTINUE lists from dbri_queuecmd.
 */
void
dbri_start(aud_stream_t *as)
{
	dbri_chip_cmd_t cmd;
	dbri_pipe_t *pipep;
	dbri_unit_t *unitp;
	dbri_cmd_t *dcp;

	ASSERT_ASLOCKED(as);
	ATRACE(dbri_start, '  as', as);

	pipep = AsToDs(as)->pipep;

	/*
	 * Check for invalid pipe before continuing
	 */
	if (pipep == NULL) {
		ATRACE(dbri_start, 'BADP', 0);
		return;
	}
	ATRACE(dbri_start, 'pipe', PIPENO(pipep));

	/*
	 * If there are no commands or the stream is paused, go back
	 */
	if ((AsToDs(as)->cmdptr == NULL) || as->info.pause) {
		ATRACE(dbri_start, 'NOPA', AsToDs(as)->cmdptr);
		return;
	}
	ATRACE(dbri_start, '[go]', PIPENO(AsToDs(as)->pipep));

	unitp = AsToUnitp(as);
	pipep->sdp &= ~DBRI_SDP_CMDMASK; /* kill off any cmd bits set */
	/* XXX - potentially kill of CLR bit */
	pipep->sdp |= (DBRI_SDP_PTR | DBRI_SDP_CLR);

	/*
	 * Scan list for first non-complete message descriptor.
	 *
	 * Note: the first thing on the chain could be a done, skip, etc.
	 *
	 * Note: This is necessary for the transmit direction
	 * because when paused, we end up with 'done' descriptors
	 * and the next dbri_start will have to skip over them.
	 */
	for (dcp = AsToDs(as)->cmdptr; dcp != NULL; dcp = dcp->nextio) {
		if (dcp->cmd.skip || dcp->cmd.done)
			continue;
		if (!ISXMITDIRECTION(as)) {
			/* XXX - this sync necessary? */
			(void) DBRI_SYNC_MD(unitp, dcp->md,
			    DDI_DMA_SYNC_FORCPU);
			if (dcp->rxmd.com)
				continue;
		}
		break;
	}

	if (dcp == NULL) {
		ATRACE(dbri_start, 'bore', 0);
		return;
	}

	cmd.opcode = pipep->sdp;
	cmd.arg[0] = (caddr32_t)DBRI_IOPBIOADDR(unitp, &dcp->txmd);
	dbri_command(unitp, cmd); /* issue SDP command */
	ATRACE(dbri_start, 'sdp ', cmd.arg[0]);

	cmd.opcode = DBRI_CMD_PAUSE; /* PAUSE command */
	dbri_command(unitp, cmd);

	as->info.active = B_TRUE;
	uniqtime(&AsToDs(as)->last_smpl_update);

} /* dbri_start */


/*
 * dbri_stop - stop reads or writes. Simply issues a SDP command with a
 * NULL pointer causing data to stop at the end of the current buffer.
 */
void
dbri_stop(aud_stream_t *as)
{
	dbri_pipe_t	*pipep;
	dbri_unit_t	*unitp;
	uint_t		add_samples, orig_cnt;	/* in "sample" units */
	uint_t		smpl_frame_size;	/* bytes per sample */
	dbri_cmd_t	*dcp;
	dbri_stream_t	*ds;

	ASSERT_ASLOCKED(as);

	ATRACE(dbri_stop, '  as', as);
	pipep = AsToDs(as)->pipep;

	/*
	 * Check for invalid pipe before continuing
	 */
	if (pipep == NULL) {
		ATRACE(dbri_stop, 'BADP', 0);
		return;
	}
	ATRACE(dbri_stop, 'pipe', PIPENO(pipep));

	/*
	 * Return if nothing to stop
	 */
	if (AsToDs(as)->cmdptr == NULL)
		return;

	unitp = AsToUnitp(as);

	if (pipep->sdp == 0) {
		ATRACE(dbri_stop, 'nada', PIPENO(AsToDs(as)->pipep));
		return;
	}
	ATRACE(dbri_stop, 'STOP', PIPENO(AsToDs(as)->pipep));

	pipep->sdp &= ~DBRI_SDP_CMDMASK; /* kill off any command bits set */
	pipep->sdp |= (DBRI_SDP_PTR | DBRI_SDP_CLR);


	/*
	 * Issue an SDP command with NULL pointer
	 */
	{
		dbri_chip_cmd_t	cmd;

		cmd.opcode = pipep->sdp;
		cmd.arg[0] = 0;
		dbri_command(unitp, cmd);
	}

	/* XXX - life would be wonderful if we could do a cv_wait() here */

	/*
	 * If the stream isn't active, or if it's not transparent,
	 * don't update the sample count; Otherwise, modify the
	 * descriptor appropriately to fake a 'partial' buffer
	 */
	if (!as->info.active)
		return;

	as->info.active = B_FALSE;	/* XXX - maybe not yet */

	if (AsToDs(as)->pipep->mode != DBRI_MODE_TRANSPARENT)
		return;

	ds = AsToDs(as);
	ds->as.info.samples = ds->samples;
	dbri_update_smpl_count(ds);
	add_samples = ds->as.info.samples - ds->samples;
	smpl_frame_size = (ds->as.info.precision * ds->as.info.channels) / 8;

#define	DBRI_FIFO_SIZE	(88) /* bytes, 22 words */
	if (!ISXMITDIRECTION(as)) {
		/*
		 * The DMA burst may not have completed so there really
		 * aren't that many more samples in the buffer...
		 * to prevent returning bogus data, we simply subtract
		 * some number of samples off the end.
		 */
		if (add_samples * smpl_frame_size < DBRI_FIFO_SIZE) {
			return;
		}
		add_samples -= DBRI_FIFO_SIZE / smpl_frame_size;
		ds->as.info.samples -= DBRI_FIFO_SIZE / smpl_frame_size;
	}
	ds->samples = ds->as.info.samples;

	/*
	 * Now make the descriptor match what we've just told the user
	 */
	for (dcp = ds->cmdptr; dcp != NULL; dcp = dcp->nextio) {
		if (!dcp->cmd.done)
			break;
	}

	/*
	 * Very bad juju if dcp is NULL here, but check anyway
	 */
	if (dcp == NULL)
		return;

	ATRACE(dbri_stop, ' add', add_samples);
	if (ISXMITDIRECTION(as)) {
		/*
		 * The calculations/timing could be off such that by the
		 * we would be past the current buffer - don't do this
		 */
		orig_cnt = dcp->txmd.cnt / smpl_frame_size;
		if (add_samples >= orig_cnt)
			add_samples = orig_cnt - 1;

		dcp->txmd.bufp += (add_samples * smpl_frame_size);
		dcp->txmd.cnt -= (add_samples * smpl_frame_size);
		(void) DBRI_SYNC_MD(unitp, dcp->md, DDI_DMA_SYNC_FORDEV);
	} else {
		orig_cnt = dcp->rxmd.bcnt / smpl_frame_size;
		if (add_samples >= orig_cnt)
			add_samples = orig_cnt - 1;

		/* XXX - set rxmd.com and/or rxmd.eof ? */
		dcp->rxmd.cnt = (add_samples * smpl_frame_size);
		dcp->cmd.data += (add_samples * smpl_frame_size);
		/*
		 * No sync needed as CPU is both writer and reader - DBRI
		 * doesn't see this change
		 */
	}

} /* dbri_stop */


/*
 * dbri_setflag - Get or set a particular flag value
 */
static uint_t
dbri_setflag(aud_stream_t *as, enum aud_opflag op, uint_t val)
{
	ASSERT_ASLOCKED(as);

	switch (op) {
	case AUD_ERRORRESET:
		val = AsToDs(as)->audio_uflow;
		AsToDs(as)->audio_uflow = B_FALSE;
		/* NB: Do not reset ds->last_flow_error here */
		break;

	/* GET only */
	case AUD_ACTIVE:
		/*
		 * XXX - need to use status here like OVRN/UNDR and if
		 * not paused. Actually, a new routine.
		 */
		val = as->info.active;
		break;
	}

	return (val);
} /* dbri_setflag */

/*
 * dbri_update_smpl_count - update the "user program visible" sample
 * count by guesstimating where DBRI actually is right now in the buffer.
 */
static void
dbri_update_smpl_count(dbri_stream_t *ds)
{
	struct timeval now;
	clock_t usecs_btwn_smples, usecs_gone_by;

#define	USECS_PER_SEC	1000000

	if (ds->pipep == NULL || ds->pipep->mode != DBRI_MODE_TRANSPARENT)
		return;

	uniqtime(&now);

	/*
	 * Unfortunately, at a sample rate of 5512 with 8192
	 * size buffers, it's possible for "tv_sec" to increase
	 * by more than one - must check for 2; optimize a bit too
	 */
	if (now.tv_sec == ds->last_smpl_update.tv_sec) {
		usecs_gone_by =
		    now.tv_usec - ds->last_smpl_update.tv_usec;
	} else if ((now.tv_sec - ds->last_smpl_update.tv_sec) == 1) {
		usecs_gone_by = now.tv_usec +
		    (USECS_PER_SEC - ds->last_smpl_update.tv_usec);
	} else {
		usecs_gone_by = USECS_PER_SEC + now.tv_usec +
		    (USECS_PER_SEC - ds->last_smpl_update.tv_usec);
	}
	usecs_btwn_smples = USECS_PER_SEC / ds->as.info.sample_rate;
	ds->as.info.samples += (usecs_gone_by / usecs_btwn_smples);

	return;
#undef	USECS_PER_SEC
} /* dbri_update_smpl_count */


/*
 * dbri_setinfo - Get or set device-specific information in the audio
 * state structure. Returns B_TRUE if there is an error.
 */
static aud_return_t
dbri_setinfo(aud_stream_t *as, mblk_t *mp, int *error)
{
	dbri_stream_t *ds_out;
	dbri_stream_t *ds_in;
	dbri_stream_t *ds_as;
	dbri_unit_t *unitp;
	audio_info_t *ip;
	int set_config;
	uint_t sample_rate;
	uint_t channels;
	uint_t precision;
	uint_t encoding;
	uint_t gain;
	uint_t old;
	uchar_t balance;

	ASSERT_ASLOCKED(as);

	unitp = AsToUnitp(as);

	ds_out = AsToDs(as->output_as);
	ds_in = AsToDs(as->input_as);
	ds_as = AsToDs(as);

	set_config = B_FALSE;

	/*
	 * Set device-specific info into device-independent structure
	 */

	/*
	 * For dss - don't let sample count go backwards. This can
	 * happen on output at the end of a sequence between the
	 * EOL interrupt and the last XCMP. After the EOL we are
	 * no longer active, but the device specific sample count
	 * has not been updated from the XCMP yet. Repeat sample counts
	 * if this is the case. For some reason, as yet unidentifed,
	 * there can be many outstanding ioctls (from one application)
	 * processed *between* these two interrupts ..... go figure ....
	 */
	old = ds_out->as.info.samples;
	ds_out->as.info.samples = ds_out->samples;
	if (ds_out->as.info.active) {
		dbri_update_smpl_count(ds_out);
	}
	if ((ds_out->as.info.samples < old) &&
	    (ds_out->as.info.samples != 0) &&
	    (old != 0)) {
		ds_out->as.info.samples = old;
	}

	ds_in->as.info.samples = ds_in->samples;
	if (ds_in->as.info.active) {
		dbri_update_smpl_count(ds_in);
	}

	/*
	 * If getinfo, 'mp' is NULL...we're done
	 */
	if (mp == NULL)
		return (AUDRETURN_NOCHANGE); /* no error */

	ip = (audio_info_t *)(void *)mp->b_cont->b_rptr;

	if (ds_out == DChanToDs(unitp, DBRI_AUDIO_OUT)) {
		if ((Modify(ip->play.gain)) || (Modifyc(ip->play.balance))) {
			if (Modify(ip->play.gain))
				gain = ip->play.gain;
			else
				gain = ds_out->as.info.gain;

			if (Modifyc(ip->play.balance))
				balance = ip->play.balance;
			else
				balance = ds_out->as.info.balance;

			ds_out->as.info.gain = dbri_play_gain(unitp, gain,
			    balance);
			ds_out->as.info.balance = balance;
		}

		if (Modifyc(ip->output_muted)) {
			unitp->distate.output_muted = dbri_output_muted(unitp,
			    ip->output_muted);
		}

		if (Modify(ip->play.port)) {
			ds_out->as.info.port = dbri_outport(unitp,
			    ip->play.port);
		}
	}

	if (ds_in == DChanToDs(unitp, DBRI_AUDIO_IN)) {
		if ((Modify(ip->record.gain)) ||
		    (Modifyc(ip->record.balance))) {

			if (Modify(ip->record.gain))
				gain = ip->record.gain;
			else
				gain = ds_in->as.info.gain;
			if (Modifyc(ip->record.balance))
				balance = ip->record.balance;
			else
				balance = ds_in->as.info.balance;

			ds_in->as.info.gain = dbri_record_gain(unitp, gain,
			    balance);
			ds_in->as.info.balance = balance;
		}

		if (Modify(ip->monitor_gain)) {
			dbri_pipe_t *pipep;
			int old_monitor_gain;

			pipep = &unitp->ptab[DBRI_PIPE_CHI];
			old_monitor_gain = unitp->distate.monitor_gain;

			unitp->distate.monitor_gain = dbri_monitor_gain(unitp,
			    ip->monitor_gain);
			if (old_monitor_gain != unitp->distate.monitor_gain) {
				if (old_monitor_gain == 0) {
					(void) pm_busy_component(unitp->dip, 2);
					(void) ddi_dev_is_needed(unitp->dip, 2,
					    1);
				} else if (unitp->distate.monitor_gain == 0 &&
				    pipep->refcnt <= 1) {
					(void) pm_idle_component(unitp->dip, 2);
				}
			}
		}

		if (Modify(ip->record.port)) {
			switch (ip->record.port) {
			case AUDIO_MICROPHONE:
			case AUDIO_LINE_IN:
				ds_in->as.info.port = dbri_inport(unitp,
				    ip->record.port);
				break;
			default:
				*error = EINVAL;
				break;
			}
		}
	}

	/*
	 * Set the sample counters atomically, returning the old values.
	 */
	if (ds_out->as.info.open) {
		ds_out->as.info.samples = ds_out->samples;
		if (Modify(ip->play.samples))
			ds_out->samples = ip->play.samples;
	}

	if (ds_in->as.info.open) {
		ds_in->as.info.samples = ds_in->samples;
		if (Modify(ip->record.samples))
			ds_in->samples = ip->record.samples;
	}

	/*
	 * First, get the right values to use.
	 *
	 * XXX - As an optimization, if all new values match existing ones,
	 * we should just return.
	 */
	if (Modify(ip->play.sample_rate))
		sample_rate = ip->play.sample_rate;
	else if (Modify(ip->record.sample_rate))
		sample_rate = ip->record.sample_rate;
	else
		sample_rate = ds_as->as.info.sample_rate;

	if (Modify(ip->play.channels))
		channels = ip->play.channels;
	else if (Modify(ip->record.channels))
		channels = ip->record.channels;
	else
		channels = ds_as->as.info.channels;

	if (Modify(ip->play.precision))
		precision = ip->play.precision;
	else if (Modify(ip->record.precision))
		precision = ip->record.precision;
	else
		precision = ds_as->as.info.precision;

	if (Modify(ip->play.encoding))
		encoding = ip->play.encoding;
	else if (Modify(ip->record.encoding))
		encoding = ip->record.encoding;
	else
		encoding = ds_as->as.info.encoding;

	/*
	 * Now, if setting the same existing format, or not touching
	 * the "big four" parameters, don't do anything
	 */
	if ((sample_rate == ds_as->as.info.sample_rate) &&
	    (channels == ds_as->as.info.channels) &&
	    (precision == ds_as->as.info.precision) &&
	    (encoding == ds_as->as.info.encoding)) {

		set_config = B_FALSE;

	} else if (Modify(ip->play.sample_rate) ||
	    Modify(ip->play.precision) ||
	    Modify(ip->play.channels) ||
	    Modify(ip->play.encoding)) {
		/*
		 * So, a process wants to modify the play format - check
		 * and see if another process has it open for recording.
		 * If not, see if the new requested config is possible.
		 * Can't make changes on the control device, however.
		 */
		if (ds_in->as.info.open &&
		    ds_out->as.info.open &&
		    (ds_in->as.readq != ds_out->as.readq)) {
			*error = EBUSY;
		} else if ((ds_as != ds_in) && (ds_as != ds_out)) {
			*error = EINVAL;
		} else if (ds_out != DChanToDs(unitp, DBRI_AUDIO_OUT)) {
			/*
			 * If this isn't the proper channel then we'll
			 * try to reconfigure the codec for, say, an
			 * ISDN channel.  Prevent this!
			 */
			*error = EINVAL;
		} else {
			set_config = B_TRUE;
			*error = mmcodec_check_audio_config(unitp,
			    sample_rate, channels, precision, encoding);
		}

	} else if (Modify(ip->record.sample_rate) ||
	    Modify(ip->record.precision) ||
	    Modify(ip->record.channels) ||
	    Modify(ip->record.encoding)) {
		/*
		 * A process wants to modify the recording format - check
		 * to see if someone else is playing. If not, see if the
		 * new requested config is possible
		 */
		if (ds_in->as.info.open &&
		    ds_out->as.info.open &&
		    (ds_in->as.readq != ds_out->as.readq)) {
			*error = EBUSY;
		} else if ((ds_as != ds_in) && (ds_as != ds_out)) {
			*error = EINVAL;
		} else if (ds_in != DChanToDs(unitp, DBRI_AUDIO_IN)) {
			/*
			 * If this isn't the proper channel then we'll
			 * try to reconfigure the codec for, say, an
			 * ISDN channel.  Prevent this!
			 */
			*error = EINVAL;
		} else {
			set_config = B_TRUE;
			*error = mmcodec_check_audio_config(unitp,
			    sample_rate, channels, precision, encoding);
		}
	}

	/*
	 * Check buffer_size here since we tweak it depending on the
	 * sample size so buffers contain integral numbers of samples.
	 *
	 * For hardware performance reasons what is actually desired is a
	 * non-zero multiple of a nice DMA burst size.  Since a burst
	 * size will always be a multiple of a sample size, the burst
	 * size criteria also meets the sample size criteria.
	 */
	if (Modify(ip->record.buffer_size)) {
		if (((ds_as != ds_in) &&
		    (ds_as == ds_out) && !(ds_out->as.openflag & FREAD)) ||
		    (ip->record.buffer_size > DBRI_MAX_BSIZE) ||
		    (ip->record.buffer_size <= 0)) {
			if (ip->record.buffer_size !=
			    ds_in->as.info.buffer_size) {
				*error = EINVAL;
			}
		} else {
			int is;

			is = ip->record.buffer_size -
			    (ip->record.buffer_size % DBRI_BSIZE_MODULO);
			if (is < DBRI_BSIZE_MODULO)
				is = DBRI_BSIZE_MODULO;
			ip->record.buffer_size = is;
			ds_in->as.info.buffer_size = is;
		}
	}

	if (!*error && set_config) {
		aud_stream_t *control_as = DChanToAs(unitp, DBRI_AUDIOCTL);

		/*
		 * Update the "real" info structure (and others) accordingly
		 */
		ip->play.sample_rate = ip->record.sample_rate = sample_rate;
		ip->play.channels = ip->record.channels = channels;
		ip->play.precision = ip->record.precision = precision;
		ip->play.encoding = ip->record.encoding = encoding;

		/*
		 * Update the current configuration of the audio device passed
		 * to user programs
		 */
		control_as->info.sample_rate = sample_rate;
		control_as->info.channels = channels;
		control_as->info.precision = precision;
		control_as->info.encoding = encoding;

		ds_in->as.info.sample_rate = sample_rate;
		ds_in->as.info.channels = channels;
		ds_in->as.info.precision = precision;
		ds_in->as.info.encoding = encoding;

		ds_out->as.info.sample_rate = sample_rate;
		ds_out->as.info.channels = channels;
		ds_out->as.info.precision = precision;
		ds_out->as.info.encoding = encoding;

		mmcodec_set_audio_config(ds_in);
		mmcodec_setup_ctrl_mode(unitp);

		/*
		 * !error means that the change was okayed... but
		 * mmcodec_set_audio_config twiddles with this value, so
		 * if the user specified a new value, insert if here.
		 */
		if (Modify(ip->record.buffer_size)) {
			ds_in->as.info.buffer_size = ip->record.buffer_size;
		}

		/*
		 * Don't ack the ioctl until it's actually done; save the
		 * mp on this stream and ack when it finishes.
		 */
		return (AUDRETURN_DELAYED);
	}

	dbri_config_queue(AsToDs(as));

	return (AUDRETURN_CHANGE);
}


/*
 * This routine is called whenever a new packet is added to the cmd chain.
 * It ties in the chip specific message descriptors then tells the chip to go.
 */
static void
dbri_queuecmd(aud_stream_t *as, aud_cmd_t *cmdp)
{
	dbri_pipe_t	*pipep;

	ASSERT_ASLOCKED(as);
	ATRACE(dbri_queuecmd, '  as', as);

	/*
	 * Check for invalid pipe before continuing
	 */
	pipep = AsToDs(as)->pipep;
	if (pipep == NULL) {
		ATRACE(dbri_queuecmd, 'BADP', PIPENO(pipep));
		return;
	}
	ATRACE(dbri_queuecmd, 'pipe', PIPENO(pipep));

	if (as == as->output_as)
		dbri_txqcmd(as, cmdp);
	else if (as == as->input_as)
		dbri_rxqcmd(as, cmdp);

} /* dbri_queuecmd */


/*
 * dbri_rxqcmd - Fill in a receive md's in the dbri command structure
 */
static void
dbri_rxqcmd(aud_stream_t *as, aud_cmd_t *cmdp)
{
	dbri_cmd_t *dcp;
	dbri_unit_t *unitp = AsToUnitp(as);
	boolean_t need_start = B_FALSE;
	int e;			/* DDI/DKI error status */

	ATRACE(dbri_rxqcmd, ' cmd', cmdp);
	for (dcp = (dbri_cmd_t *)(void *)cmdp;
	    dcp != NULL;
	    dcp = (dbri_cmd_t *)(void *)dcp->cmd.next) {

		ATRACE(dbri_rxqcmd, ' dcp', dcp);
		ASSERT((dcp->cmd.enddata - dcp->cmd.data) >=
		    as->info.buffer_size);

		/*
		 * Initialize bit fields and pointers
		 */
		dcp->nextio = NULL;
		dcp->words[0] = 0; /* clear out all bits */
		dcp->rxmd.bufp = 0; /* buffer pointer */
		dcp->rxmd.fp = NULL; /* forward md pointer */
		dcp->words[3] = 0; /* clear out all bits */
		dcp->cmd.tracehdr.type = ISDN_PH_DATA_IN;

		/*
		 * Setup for DMA transfers to the buffer from the device
		 */
		ATRACE(dbri_rxqcmd, 'insz', as->info.buffer_size);
		e = ddi_dma_addr_setup(unitp->dip, (struct as *)0,
		    (caddr_t)dcp->cmd.data, as->info.buffer_size,
		    DDI_DMA_READ, DDI_DMA_DONTWAIT, 0,
		    &dbri_dma_limits, &dcp->buf_dma_handle);
		if (e != DDI_DMA_MAPPED) {
			ATRACE(dbri_rxqcmd, '!dma', e);
			/* XXX - mark command as skip or something */
			return;
		}

		e = ddi_dma_htoc(dcp->buf_dma_handle, 0, &dcp->buf_dma_cookie);
		if (e != DDI_SUCCESS) {
			/*
			 * XXX - mark command as skip or something
			 *
			 * XXX - the sync flag below may be expensive and
			 * we are only throwing the buffer away.
			 */
			if (dcp->buf_dma_handle) {
				(void) ddi_dma_free(dcp->buf_dma_handle);
				dcp->buf_dma_handle = 0;
			}
			ATRACE(dbri_rxqcmd, '!h2c', e);
			return;
		}

		dcp->rxmd.bufp = dcp->buf_dma_cookie.dmac_address;
		dcp->rxmd.bcnt = dcp->buf_dma_cookie.dmac_size;

		(void) DBRI_SYNC_BUF(dcp, dcp->rxmd.cnt, DDI_DMA_SYNC_FORDEV);

		dcp->rxmd.fint = 1; /* Frame interrupt */

		(void) DBRI_SYNC_MD(unitp, dcp->md, DDI_DMA_SYNC_FORDEV);

		if (AsToDs(as)->cmdptr == NULL) {
			need_start = B_TRUE;
			ATRACE(dbri_rxqcmd, 'frst', PIPENO(AsToDs(as)->pipep));
			AsToDs(as)->cmdptr = dcp;
			AsToDs(as)->cmdlast = dcp;
		} else {
			ATRACE(dbri_rxqcmd, 'next', PIPENO(AsToDs(as)->pipep));
			AsToDs(as)->cmdlast->rxmd.fp =
			    DBRI_IOPBIOADDR(unitp, &dcp->rxmd);
			(void) DBRI_SYNC_MD(unitp, AsToDs(as)->cmdlast->md,
			    DDI_DMA_SYNC_FORDEV);

			/*
			 * The dbri_cmd_t is now visible to DBRI.  Append
			 * to end of IO list.
			 */
			AsToDs(as)->cmdlast->nextio = dcp;
			AsToDs(as)->cmdlast = dcp;
		}
	}

	/*
	 * Done start I/O if the stream is paused.  Only issue an SDP if
	 * this is the first on the list.
	 */
	if ((!as->info.pause) && (need_start))
		dbri_start(as);	/* issues SDP with new chain */

} /* dbri_rxqcmd */


/*
 * dbri_txqcmd - Fill in a transmit md's in the dbri command structure.
 *
 * Currently chucks packets when D-channels are not activated.
 */
static void
dbri_txqcmd(aud_stream_t *as, aud_cmd_t *cmdp)
{
	dbri_unit_t *unitp;
	dbri_pipe_t *pipep;
	dbri_cmd_t *dcp;
	dbri_chip_cmd_t	cmd;
	struct {
		dbri_cmd_t *head; /* first fragment in packet */
		dbri_cmd_t *tail; /* last fragment in packet */
	} packet;
	int activated;		/* Serial interface state */
	int length;
	enum {NOTSET, IDLE, CONTINUE, START} action;
	int e;
	int samplesize;

	action = NOTSET;
	packet.head = NULL;
	packet.tail = NULL;

	unitp = AsToUnitp(as);

	/*
	 * Each fragment must be a multiple of the sample size in length
	 */
	samplesize = as->info.precision * as->info.channels;

	/*
	 * Determine the state of the serial interface
	 */
	pipep = AsToDs(as)->pipep;
	activated = dbri_chan_activated(AsToDs(as), pipep->sink.dchan,
	    pipep->source.dchan);

	/*
	 * Process a chain of one or more fragments making a single
	 * packet.
	 *
	 * audio_process_output hides zero length fragments that are
	 * part of a non-zero length packet.
	 */
	for (dcp = (dbri_cmd_t *)(void *)cmdp;
	    dcp != NULL;
	    dcp = (dbri_cmd_t *)(void *)dcp->cmd.next) {

		dcp->nextio = NULL;
		dcp->cmd.tracehdr.type = ISDN_PH_DATA_RQ;

		if (dcp->cmd.done || dcp->cmd.skip || !activated) {
			ATRACE(dbri_txqcmd, 'DISC', dcp);
			dcp->cmd.done = B_TRUE;
			dcp->cmd.skip = B_TRUE;

			dcp->words[0] = 0; /* Clear bit fields */
			dcp->txmd.bufp = NULL; /* data */
			dcp->txmd.fp = NULL; /* forward md pointer */
			dcp->txmd.status = 0; /* status */
		} else {
			ATRACE(dbri_txqcmd, 'dcp ', dcp);
			length = dcp->cmd.enddata - dcp->cmd.data;
			ATRACE(dbri_txqcmd, 'len ', length);
			ASSERT(length <= DBRI_MAX_DMASIZE);

			/*
			 * For audio streams, If the fragment is not a multiple
			 * of the sample size, shut down the stream.
			 */
			if ((AsToDs(as)->ctep->dchan == DBRI_AUDIO_OUT) &&
			    (((length * 8) % samplesize) != 0)) {
				ATRACE(dbri_txqcmd, 'ESMP', length);
				dbri_error_msg(AsToDs(as), M_ERROR);
				return;
			}

			/*
			 * This fragment is part of a packet to transmit.
			 */
			dcp->words[0] = 0; /* Clear out bit fields */
			dcp->txmd.fp = NULL; /* forward md pointer */
			dcp->txmd.status = 0; /* status */
			dcp->txmd.idl = 0;
			dcp->txmd.fcnt = 0;

			/*
			 * Setup for DMA transfers to the buffer from the device
			 */
			e = ddi_dma_addr_setup(unitp->dip, (struct as *)0,
			    (caddr_t)dcp->cmd.data, length,
			    DDI_DMA_WRITE, DDI_DMA_DONTWAIT, 0,
			    &dbri_dma_limits, &dcp->buf_dma_handle);
			if (e != 0) {
				ATRACE(dbri_txqcmd, 'Edas', e);
				dcp->cmd.skip = B_TRUE;
				dcp->cmd.done = B_TRUE;
				return;
			}

			e = ddi_dma_htoc(dcp->buf_dma_handle, 0,
			    &dcp->buf_dma_cookie);
			if (e != 0) {
				if (dcp->buf_dma_handle) {
					(void) ddi_dma_free(
					    dcp->buf_dma_handle);
					dcp->buf_dma_handle = 0;
				}
				ATRACE(dbri_txqcmd, 'Edhc', e);
				dcp->cmd.skip = B_TRUE;
				dcp->cmd.done = B_TRUE;
				return;
			}

			dcp->txmd.bufp = dcp->buf_dma_cookie.dmac_address;
			dcp->txmd.cnt = dcp->buf_dma_cookie.dmac_size;
			(void) DBRI_SYNC_BUF(dcp, dcp->txmd.cnt,
			    DDI_DMA_SYNC_FORDEV);

			ATRACE(dbri_txqcmd, 'frag',
			    dcp->buf_dma_cookie.dmac_size);

			ASSERT(dcp->cmd.lastfragment != NULL);

			if (&dcp->cmd == dcp->cmd.lastfragment) {
				dcp->txmd.eof = 1;	/* Tag last pkt */
				dcp->txmd.fint = 1;	/* Frame intr */
			}

			/*
			 * Link the fragments together
			 */
			if (packet.head == NULL) {
				packet.head = dcp;
				packet.tail = dcp;
				packet.tail->txmd.fp = 0;
			} else {
				packet.tail->nextio = dcp;
				packet.tail->txmd.fp =
				    DBRI_IOPBIOADDR(unitp, &dcp->txmd);
				packet.tail = dcp;
			}
			(void) DBRI_SYNC_MD(unitp, dcp->md,
			    DDI_DMA_SYNC_FORDEV);
		}
	}

	if (packet.head) {
		if (ISTEDCHANNEL(as)) {
			/*
			 * CCITT Fascicle III.8 - Rec. I.430, p180, 6.1.4
			 *
			 * The priority is set on the first packet fragment.
			 * Always negotiate access.
			 */
			packet.head->txmd.idl = 1;

			/*
			 * Use the correct priority for this SAPI.
			 * SAPI 0, call control, uses Class 1.
			 * All other SAPIs get Class 2.
			 */
			if ((packet.head->cmd.data[0] & 0xfc) == 0)
				packet.head->txmd.fcnt = DBRI_D_CLASS_1;
			else
				packet.head->txmd.fcnt = DBRI_D_CLASS_2;

			(void) DBRI_SYNC_MD(unitp, packet.head->md,
			    DDI_DMA_SYNC_FORDEV);
		} else {
			ASSERT(packet.tail && packet.tail->txmd.eof);
			/*
			 * The "fcnt" field only takes effect in fragments
			 * with eof set and HDLC mode.
			 *
			 * For HDLC mode, send idles between frames.
			 */
			packet.tail->txmd.idl = 1;
			packet.tail->txmd.fcnt = 1;

			(void) DBRI_SYNC_MD(unitp, packet.tail->md,
			    DDI_DMA_SYNC_FORDEV);
		}

		if (packet.tail->md2 != NULL) {
			uchar_t *buf;

			switch (as->info.encoding) {
			case AUDIO_ENCODING_ULAW:
				buf = unitp->zerobuf->ulaw;
				break;

			case AUDIO_ENCODING_ALAW:
				buf = unitp->zerobuf->alaw;
				break;

			default:
			case AUDIO_ENCODING_LINEAR:
				buf = unitp->zerobuf->linear;
				break;
			}

			packet.tail->words2[0] = 0; /* Clear out bit fields */
			packet.tail->txmd2.status = 0;
			packet.tail->txmd2.idl = 0;
			packet.tail->txmd2.bufp =
			    DBRI_IOPBIOADDR(unitp, buf);
			packet.tail->txmd2.cnt = 4;
			packet.tail->txmd2.fp = NULL; /* forward md pointer */
			packet.tail->txmd2.eof = 1;
			packet.tail->txmd2.fint = 1;

			packet.tail->txmd.fp =
			    DBRI_IOPBIOADDR(unitp, &packet.tail->txmd2);
		}

		if (AsToDs(as)->cmdptr == NULL) {
			ATRACE(dbri_txqcmd, 'FRST', packet.head);
			AsToDs(as)->cmdptr = packet.head;
			AsToDs(as)->cmdlast = packet.tail;
			action = START;
		} else {
			ATRACE(dbri_txqcmd, 'APND', packet.head);
			ATRACE(dbri_txqcmd, '(to)', AsToDs(as)->cmdlast);
			AsToDs(as)->cmdlast->nextio = packet.head;
			AsToDs(as)->cmdlast->txmd.fp = DBRI_IOPBIOADDR(unitp,
			    &packet.head->txmd);
			(void) DBRI_SYNC_MD(unitp, AsToDs(as)->cmdlast->md,
			    DDI_DMA_SYNC_FORDEV);

			if (AsToDs(as)->cmdlast->md2 != NULL) {
				AsToDs(as)->cmdlast->txmd2.fp =
				    DBRI_IOPBIOADDR(unitp, &packet.head->txmd);
				(void) DBRI_SYNC_MD(unitp,
				    AsToDs(as)->cmdlast->md2,
				    DDI_DMA_SYNC_FORDEV);
			}

			AsToDs(as)->cmdlast = packet.tail;
			action = CONTINUE;
		}
	} else {
		ATRACE(dbri_txqcmd, 'nada', PIPENO(AsToDs(as)->pipep));
		action = IDLE;
	}

	if (as->info.pause)	/* don't start IO if paused */
		action = IDLE;

	/* Give md's to chip to start or continue I/O */
	switch (action) {
	case CONTINUE:
		ATRACE(dbri_txqcmd, 'cdp ', PIPENO(AsToDs(as)->pipep));
		cmd.opcode = DBRI_CMD_CDP | PIPENO(AsToDs(as)->pipep);
		dbri_command(unitp, cmd); /* issue CDP command */

		if (!as->info.active) {
			as->info.active = B_TRUE;
			uniqtime(&AsToDs(as)->last_smpl_update);
		}
		break;

	case START:
		dbri_start(as);		/* issues SDP with new chain */
		break;

	case IDLE:
		/*
		 * Ensure that garbage is collected
		 */
		audio_gc_output(as);
		break;

	case NOTSET:
		/* XXX - error! */
		cmn_err(CE_WARN, "dbri: txqcmd with no action given");
		break;
	}

} /* dbri_txqcmd */


/*
 * dbri_flushas - Flush the device's notion of queued commands.
 * AUD_STOP() mush be called before this routine.
 */
static void
dbri_flushas(aud_stream_t *as)
{
	dbri_cmd_t	*dcp;

	ASSERT_ASLOCKED(as);
	ATRACE(dbri_flushas, 'flsh', as);

	/*
	 * XXX - This delay is bogus and needs to be reworked but makes
	 * things a little safer for now. It's purpose is to insure that
	 * all SBus accesses by the chip are complete *BEFORE* freeing up
	 * memory.
	 */
	drv_usecwait(250);

	for (dcp = AsToDs(as)->cmdptr; dcp != NULL; dcp = dcp->nextio) {
		if (dcp->buf_dma_handle) {
			(void) ddi_dma_free(dcp->buf_dma_handle);
			dcp->buf_dma_handle = 0;
		}
	}

	AsToDs(as)->cmdptr = NULL;
	AsToDs(as)->cmdlast = NULL;

} /* dbri_flushas */


/*
 * dbri_loopback - Set up a loopback (flag = 0) or clear loopback
 * (flag = 1).
 */
static boolean_t
dbri_loopback(dbri_stream_t *ds, isdn_loopback_request_t *lr, int flag)
{
	dbri_bri_status_t *bs;
	dbri_unit_t *unitp;
	dbri_chip_cmd_t	cmd;
	int lb_bits;		/* loopback bits for NT or TE channel */

	unitp = DsToUnitp(ds);
	ASSERT_UNITLOCKED(unitp);

	lb_bits = 0;

	/*
	 * Check for a request that specifies channels we are not
	 * supporting.
	 */
	if (lr->channels &
	    ~(ISDN_LOOPBACK_D | ISDN_LOOPBACK_B1 | ISDN_LOOPBACK_B2))
		return (B_FALSE);

	/*
	 * Make a mask for the proper loopback bits in the NT/TE command
	 */
	if (lr->channels & (int)ISDN_LOOPBACK_D)
		lb_bits |= DBRI_NTE_LLB_D;
	if (lr->channels & (int)ISDN_LOOPBACK_B1)
		lb_bits |= DBRI_NTE_LLB_B1;
	if (lr->channels & (int)ISDN_LOOPBACK_B2)
		lb_bits |= DBRI_NTE_LLB_B2;

	switch (lr->type) {
	case ISDN_LOOPBACK_LOCAL:
		/* Bits are already okay, don't move them */
		break;

	case ISDN_LOOPBACK_REMOTE:
		/* Shift bits over 3 positions to get Remote mask */
		lb_bits <<= 3;
		break;

	default:
		return (B_FALSE);
	}

	/*
	 * Build a DBRI TE or NT command and execute it.
	 */
	switch (ds->ctep->isdntype) {
	case ISDN_TYPE_TE:
		bs = DBRI_TE_STATUS_P(unitp);
		break;

	case ISDN_TYPE_NT:
		bs = DBRI_NT_STATUS_P(unitp);
		break;

	default:
		return (B_FALSE);
	}

	if (flag)
		bs->ntte_cmd &= ~(lb_bits);
	else
		bs->ntte_cmd |= lb_bits;

	cmd.opcode = bs->ntte_cmd;
	dbri_command(unitp, cmd);

	return (B_TRUE);
} /* dbri_loopback */


/*
 * dbri_minortoas - Search channel table for a match on the minor device
 * number then grab the audio stream from that channel.
 */
static aud_stream_t *
dbri_minortoas(dbri_unit_t *unitp, int minordev)
{
	dbri_stream_t *ds = DChanToDs(unitp, 0);

	ASSERT_UNITLOCKED(unitp);

	while (ds < &unitp->ioc[Dbri_nstreams]) {
		if (!DsDisabled(ds)) {
			if (ds->ctep->minordev == minordev)
				return (DsToAs(ds));
		}
		++ds;
	}
	return (NULL);
} /* dbri_minortoas */
