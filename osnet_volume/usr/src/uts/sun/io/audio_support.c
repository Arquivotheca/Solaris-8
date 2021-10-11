/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_support.c	1.7	99/10/19 SMI"

/*
 * Audio Support Module
 *
 * This module is use by Audio Drivers that use the new audio driver
 * architecture. It provides common services to get and set data
 * structures used by Audio Drivers and Audio Personality Modules.
 * It provides the home for the audio_state_t structures, one per
 * audio device.
 *
 * Audio Drivers set their qinit structutres to the open, close, put
 * and service routines in this module. Then this module determines
 * which Audio Personality Module to call to implement the read, write,
 * and ioctl semantics.
 */

#include <sys/audio_impl.h>
#include <sys/modctl.h>		/* modldrv */
#include <sys/debug.h>		/* ASSERT() */
#include <sys/kmem.h>		/* kmem_zalloc(), etc. */
#include <sys/ksynch.h>		/* mutex_init(), etc. */
#include <sys/stat.h>		/* S_IFCHR */
#include <sys/errno.h>		/* ENXIO, etc. */
#include <sys/stropts.h>	/* I_PLINK, I_UNPLINK */
#include <sys/modctl.h>		/* mod_miscops */
#include <sys/ddi.h>		/* getminor(), etc. */
#include <sys/sunddi.h>		/* nochpoll, ddi_prop_op, etc. */

#ifdef DEBUG
/*
 * Global audio tracing variables
 */
audio_trace_buf_t audio_trace_buffer[AUDIO_TRACE_BUFFER_SIZE];
kmutex_t audio_tb_lock;
size_t audio_tb_siz = AUDIO_TRACE_BUFFER_SIZE;
int audio_tb_pos = 0;
uint_t audio_tb_seq = 0;
#endif

/*
 * Global hidden variables.
 */
static kmutex_t inst_lock;		/* mutex to protect instance list */
static void *inst_statep;		/* anchor for soft state structures */
static int audio_reserved;		/* number of devices, /dev/audio etc. */
static int instances = 0;		/* the number of attached instances */
static int minors_per_inst;		/* number of minor #s per dev inst. */
static int sup_chs;			/* the # of supported chs/instance */

static audio_device_t audio_device_info = {
	AUDIO_NAME,
	AUDIO_VERSION,
	AUDIO_CONFIGURATION
};

/*
 * Local Support Routine Prototypes For Audio Support Module
 */
static audio_state_t *audio_sup_bind_dev(dev_t);
static void audio_sup_free_instance(dev_info_t *);
static audio_state_t *audio_sup_reg_instance(dev_info_t *);
static int audio_sup_wiocdata(queue_t *, mblk_t *, audio_ch_t *);
static int audio_sup_wioctl(queue_t *, mblk_t *, audio_ch_t *);

/*
 * Module Linkage Structures
 */
/* Linkage structure for loadable drivers */
static struct modlmisc audio_modlmisc = {
	&mod_miscops,		/* drv_modops */
	AUDIO_MOD_NAME,		/* drv_linkinfo */
};

static struct modlinkage audio_modlinkage =
{
	MODREV_1,		/* ml_rev */
	(void*)&audio_modlmisc,	/* ml_linkage */
	NULL			/* NULL terminates the list */
};

/*
 *  Loadable Module Configuration Entry Points
 */

/*
 * _init()
 *
 * Description:
 *	Driver initialization, called when driver is first loaded.
 *
 *	Because audiosup may be called by various drivers we cannot use
 *	ddi_get_instance() because the dev_info_t may always have minor
 *	number zero. Therefore we allocate soft state structures for each
 *	instance, but don't remove them until we detach the audiosup module.
 *	The inst_lock is used when we need to muck with these structures.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	ddi_soft_state_init() status, see ddi_soft_state_init(9f), or
 *	mod_install() status, see mod_install(9f)
 */
int
_init(void)
{
	int	error;

#ifdef DEBUG
	mutex_init(&audio_tb_lock, NULL, MUTEX_DRIVER, NULL);
#endif

	ATRACE("in audiosup _init()", 0);

	/* set up for the soft state */
	if ((error = ddi_soft_state_init(&inst_statep,
	    sizeof (audio_inst_info_t), 0)) != DDI_SUCCESS) {
		ATRACE("audiosup ddi_soft_state_init() failed", inst_statep);
#ifdef DEBUG
		mutex_destroy(&audio_tb_lock);
#endif
		return (error);
	}

	/* standard linkage call */
	if ((error = mod_install(&audio_modlinkage)) != DDI_SUCCESS) {
		ATRACE_32("audiosup _init() error 1", error);
		ddi_soft_state_fini(&inst_statep);
#ifdef DEBUG
		mutex_destroy(&audio_tb_lock);
#endif
		return (error);
	}

	/* CAUTION - From this point on nothing can fail */

	/* intialize the instance list lock */
	mutex_init(&inst_lock, NULL, MUTEX_DRIVER, NULL);

	ATRACE("audiosup _init() successful", 0);

	return (error);

}	/* _init() */

/*
 * _fini()
 *
 * Description
 *	Module de-initialization, called when driver is to be unloaded.
 *
 *	Free resources that were allocated in _init(). We also get rid of the
 *	soft state structures. The number of soft state structures may grow,
 *	but they never shrink. This lets us manage instances from different
 *	devices.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	mod_remove() status, see mod_remove(9f)
 */
int
_fini(void)
{
	int	error;

	ATRACE("in audiosup _fini()", 0);

	if ((error = mod_remove(&audio_modlinkage)) != DDI_SUCCESS) {
		ATRACE_32("audiosup _fini() mod_remove failed", error);
		return (error);
	}

	/* CAUTION - From this point on nothing can fail */

	/* free the instance state structures */
	while (instances) {
		ddi_soft_state_free(inst_statep, --instances);
		ATRACE_32("audiosup _fini() freeing inst structure", instances);
	};

	/* now free the soft state internal structures */
	ddi_soft_state_fini(&inst_statep);

	/* free instance list lock */
	mutex_destroy(&inst_lock);

	ATRACE_32("audiosup _fini() successful", error);

#ifdef DEBUG
	mutex_destroy(&audio_tb_lock);
#endif

	return (error);

}	/* _fini() */

/*
 * _info()
 *
 * Description:
 *	Module information, returns infomation about the driver.
 *
 * Arguments:
 *	modinfo	*modinfop	Pointer to an opaque modinfo structure
 *
 * Returns:
 *	mod_info() status, see mod_info(9f)
 */
int
_info(struct modinfo *modinfop)
{
	int		rc;

	rc = mod_info(&audio_modlinkage, modinfop);

	ATRACE_32("audiosup _info() returning", rc);

	return (rc);

}	/* _info() */

/*
 * Public Audio Device Independent Driver Entry Points
 *
 * Standard Driver Entry Points
 */

/*
 * audio_sup_attach()
 *
 * Description:
 *	This routine initializes structures used for an instance of the
 *	audio device. It doesn't really do too much.
 *
 * Arguments:
 *	dev_info_t	*dip		Ptr to the device's dev_info structure
 *	ddi_attach_cmd_t cmd		Attach command
 *
 * Returns:
 *	>= 0				Instance number for the device
 *	AUDIO_FAILURE			Attach failed
 */
int
audio_sup_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	audio_state_t	*statep;		/* instance state pointer */
	audio_ch_t	*chptr;			/* channel pointer */
	int		i;

	ATRACE("in audio_sup_attach()", dip);

	switch (cmd) {
	case DDI_ATTACH:
		ATRACE_32("audio_sup_attach() good command", cmd);
		break;
	default:
		ATRACE_32("audio_sup_attach() bad command", cmd);
		return (AUDIO_FAILURE);
	}

	/* get the properties from the .conf file */
	if ((audio_reserved = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_CANSLEEP,
	    "audio_reserved", AUDIO_NUM_DEVS)) == AUDIO_NUM_DEVS) {
		ATRACE_32("audio_sup_attach() "
		    "setting the number of audio devices", AUDIO_NUM_DEVS);
#ifdef DEBUG
	} else {
		ATRACE_32("audio_sup_attach() "
		    "setting the number of audio devices from .conf file",
		    audio_reserved);
#else
		/*EMPTY*/
#endif
	}

	if ((minors_per_inst = ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_CANSLEEP, "minors_per_inst",
	    AUDIO_MINOR_PER_INST)) == AUDIO_MINOR_PER_INST) {
		ATRACE_32("audio_sup_attach() "
		    "setting the number of minors per instance",
		    AUDIO_MINOR_PER_INST);
#ifdef DEBUG
	} else {
		ATRACE_32("audio_sup_attach() "
		    "setting the number of minors per instance from .conf",
		    minors_per_inst);
#else
		/*EMPTY*/
#endif
	}
	sup_chs = minors_per_inst - audio_reserved;
	ATRACE_32("audio_sup_attach() supported channels per device", sup_chs);

	/* sanity check */
	if (sup_chs < AUDIO_MIN_CLONE_CHS) {
		ATRACE_32("audio_sup_attach() supported channels too small",
		    sup_chs);
		cmn_err(CE_NOTE, "audiosup: attach() "
		    "the number of clone channels, %d, is too small", sup_chs);
		return (AUDIO_FAILURE);
	} else if (sup_chs > AUDIO_CLONE_CHANLIM) {
		ATRACE_32("audio_sup_attach() supported channels too large",
		    sup_chs);
		cmn_err(CE_NOTE, "audiosup: attach() "
		    "the number of clone channels, %d, is too large", sup_chs);
		return (AUDIO_FAILURE);
	}

	/* register and get device instance number */
	if ((statep = audio_sup_reg_instance(dip)) == NULL) {
		ATRACE("audio_sup_attach() audio_sup_reg_instance() failed", 0);
		return (AUDIO_FAILURE);
	}

	/*
	 * WARNING: From here on all error returns must worry about the
	 *	instance state structures.
	 *
	 * Initialize the instance mutex and condition variables. Used to
	 * allocate channels.
	 */
	mutex_init(&statep->as_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&statep->as_cv, NULL, CV_DRIVER, NULL);

	/* initialize other state information */
	statep->as_dev_instance = ddi_get_instance(dip);
	statep->as_max_chs = sup_chs;

	/* initialze the channel structures */
	ATRACE("audio_sup_attach() # sup channels", sup_chs);
	for (i = 0, chptr = &statep->as_channels[0]; i < sup_chs;
								i++, chptr++) {
		/* most everything is zero, do it quickly */
		bzero(chptr, sizeof (*chptr));

		/* now do the non-zero members */
		chptr->ch_statep =		statep;
		chptr->ch_dev =			AUDIO_NO_DEV;
		chptr->ch_info.ch_number =	i;
		chptr->ch_info.dev_type =	UNDEFINED;

		mutex_init(&chptr->ch_lock, NULL, MUTEX_DRIVER, NULL);
		mutex_init(&chptr->ch_msg_lock, NULL, MUTEX_DRIVER, NULL);
		cv_init(&chptr->ch_cv, NULL, CV_DRIVER, NULL);
	}

	ATRACE_32("audio_sup_attach() returning", statep->as_dev_instance);
	return (statep->as_dev_instance);

}	/* audio_sup_attach() */

/*
 * audio_sup_close()
 *
 * Description:
 *	This routine is called when the kernel wants to close an Audio Driver
 *	channel. It figures out what kind of device it is and calls the
 *	appropriate Audio Personality Module.
 *
 * Arguments:
 *	queue_t		*q	Pointer to the read queue
 *	int		flag	Open flags
 *	cred_t		*credp	Pointer to the user's credential struct.
 *
 * Returns:
 *	0			Successfully closed the device
 *	errno			Error number for failed close
 */
int
audio_sup_close(queue_t *q, int flag, cred_t *credp)
{
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;

	ATRACE("in audio_sup_close()", q);

	ASSERT(chptr->ch_apm_infop);
	ASSERT(chptr->ch_apm_infop->apm_close);
	ATRACE("audio_sup_close() chptr->ch_apm_infop", chptr->ch_apm_infop);

	return ((*chptr->ch_apm_infop->apm_close)(q, flag, credp));

}	/* audio_sup_open() */

/*
 * audio_sup_detach()
 *
 * Description:
 *	This routine de-initializes structures used for an instance of the
 *	of an audio device. It doesn't really do too much.
 *
 * Arguments:
 *	dev_info_t	*dip		Ptr to the device's dev_info structure
 *	ddi_detach_cmd_t cmd		Detach command
 *
 * Returns:
 *	AUDIO_SUCCESS			Detach successful
 *	AUDIO_FAILURE			Detach failed
 */
int
audio_sup_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	audio_state_t	*statep;		/* instance state pointer */
	audio_ch_t	*chptr;			/* channel pointer */
	int		i;
	int		num_chs;

	ATRACE("in audio_sup_detach()", dip);

	switch (cmd) {
	case DDI_DETACH:
		ATRACE_32("audio_sup_detach() good command", cmd);
		break;
	default:
		ATRACE_32("audio_sup_detach() bad command", cmd);
		return (AUDIO_FAILURE);
	}

	/*
	 * WARNING: From here on all error returns must worry about the
	 *	instance state structures.
	 */

	/* get the state pointer for this instance */
	if ((statep = audio_sup_get_state(dip, NODEV)) == NULL) {
		ATRACE("audio_sup_detach() audio_sup_get_state() failed", 0);
		return (AUDIO_FAILURE);
	}

	/* de-initialize the channel structures */
	num_chs = statep->as_max_chs;
	for (i = 0, chptr = &statep->as_channels[0]; i < num_chs;
								i++, chptr++) {

		/* all we have to worry about is locks and cond. vars */
		mutex_destroy(&chptr->ch_lock);
		mutex_destroy(&chptr->ch_msg_lock);
		cv_destroy(&chptr->ch_cv);
	}

	/* free state mutext and condition variables */
	mutex_destroy(&statep->as_lock);
	cv_destroy(&statep->as_cv);

	/* remove the dip from the instance list */
	ATRACE("audio_sup_detach() freeing instance", dip);
	audio_sup_free_instance(dip);

	ATRACE("audio_sup_detach() returning", 0);
	return (AUDIO_SUCCESS);

}	/* audio_sup_detach() */

/*
 * audio_sup_open()
 *
 * Description:
 *	This routine is called when the kernel wants to open an Audio Driver
 *	channel. It figures out what kind of device it is and calls the
 *	appropriate Audio Personality Module.
 *
 * Arguments:
 *	queue_t		*q	Pointer to the read queue
 *	dev_t		*devp	Pointer to the device
 *	int		flag	Open flags
 *	int		sflag	STREAMS flags
 *	cred_t		*credp	Pointer to the user's credential struct.
 *
 * Returns:
 *	0			Successfully opened the device
 *	errno			Error number for failed open
 */
int
audio_sup_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	audio_state_t		*statep;
	audio_apm_info_t	*apm_infop;
	audio_ch_t		*chptr;
	audio_device_type_e	type;
	int			rc;

	ATRACE("in audio_sup_open()", q);

	/* get the state structure */
	if ((statep = audio_sup_get_state(NULL, *devp)) == NULL) {
		ATRACE_32("audio_sup_open() audio_sup_get_state() failed", 0);
		return (ENXIO);
	}
	ATRACE("audio_sup_open() statep", statep);

	/* get the device type */
	type = audio_sup_get_ch_type(*devp, NULL);
	ATRACE_32("audio_sup_open() type", type);

	/* get the APM info structure */
	if ((apm_infop = audio_sup_get_apm_info(statep, type)) == NULL) {
		ATRACE_32("audio_sup_open() audio_sup_get_apm_info() failed",
		    type);
		return (ENXIO);
	}
	ATRACE("audio_sup_open() apm_infop", apm_infop);

	ASSERT(apm_infop->apm_open);

	rc = (*apm_infop->apm_open)(q, devp, flag, sflag, credp);

	if (rc == AUDIO_SUCCESS) {
		/* open was successful, make sure we've got got routines */
		chptr = (audio_ch_t *)q->q_ptr;
		if (chptr->ch_wput == NULL || chptr->ch_wsvc == NULL ||
		    chptr->ch_rput == NULL || chptr->ch_rsvc == NULL) {
			ATRACE("audio_sup_open() bad open", chptr);
			/* close the device */
			(*apm_infop->apm_close)(q, flag, credp);
			rc = EIO;
		}
	}

	return (rc);

}	/* audio_sup_open() */

/*
 * audio_sup_rput()
 *
 * Description:
 *	Make sure we have a valid function pointer. If we do we make the call.
 *
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *      mblk_t		*mp	Ptr to the msg block being passed to the queue
 *
 * Returns:
 *      0			Always returns 0
 */
int
audio_sup_rput(queue_t *q, mblk_t *mp)
{
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;
	int			rc;

	ATRACE("in audio_sup_rput()", chptr);

	if (chptr->ch_rput == NULL) {
		ATRACE("audio_sup_rput() bad ch_rput", chptr->ch_rput);
		mp->b_datap->db_type = M_ERROR;
		mp->b_rptr = mp->b_datap->db_base;
		*(int *)mp->b_rptr = EINVAL;
		mp->b_wptr = mp->b_rptr + sizeof (uchar_t *);
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = 0;
		}
		qreply(q, mp);
		return (0);
	}

	ATRACE("audio_sup_rput() calling ch_rput()", chptr);

	rc = chptr->ch_rput(q, mp);

	ATRACE_32("audio_sup_rput() ch_rput() returned", rc);

	return (rc);

}	/* audio_sup_rput() */

/*
 * audio_sup_rsvc()
 *
 * Description:
 *	Make sure we have a valid function pointer. If we do we make the call.
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *
 * Returns:
 *      0			Always returns 0
 */
int
audio_sup_rsvc(queue_t *q)
{
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;
	int			rc;

	ATRACE("in audio_sup_rsvc()", chptr);

	if (chptr->ch_rsvc == NULL) {
		ATRACE("audio_sup_rsvc() bad ch_rsvc", chptr->ch_rsvc);
		return (0);
	}

	ATRACE("audio_sup_rsvc() calling ch_rsvc()", chptr);

	rc = chptr->ch_rsvc(q);

	ATRACE_32("audio_sup_rsvc() ch_rsvc() returned", rc);

	return (rc);

}	/* audio_sup_rsvc() */

/*
 * audio_sup_wput()
 *
 * Description:
 *	Make sure we have a valid function pointer. If we do we make the call.
 *
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *      mblk_t		*mp	Ptr to the msg block being passed to the queue
 *
 * Returns:
 *      0			Always returns 0
 */
int
audio_sup_wput(queue_t *q, mblk_t *mp)
{
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;
	audio_i_state_t		*cmd;
	struct iocblk		*iocbp;
	struct copyresp		*csp;
	int			rc = 0;

	ATRACE("in audio_sup_wput()", chptr);

	/*
	 * Make sure we've got a channel pointer. Something is terribly
	 * wrong if we don't.
	 */
	if (chptr == NULL || chptr->ch_wput == NULL) {
		cmn_err(CE_NOTE, "audiosup: wput() bad pointer");
		ATRACE("am_wput() possible bad chptr", q);
		ATRACE("audio_sup_wput() possible bad ch_wput", chptr->ch_wput);
		mp->b_datap->db_type = M_ERROR;
		mp->b_rptr = mp->b_datap->db_base;
		*(int *)mp->b_rptr = EINVAL;
		mp->b_wptr = mp->b_rptr + sizeof (int *);
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = 0;
		}
		qreply(q, mp);
		return (0);
	}

	/* pick off the audio support ioctls, there aren't very many */
	ATRACE_32("audio_sup_wput() type", mp->b_datap->db_type);
	switch (mp->b_datap->db_type) {

	case M_IOCTL:
		ATRACE("audio_sup_wput() IOCTL ", chptr);
		iocbp = (struct iocblk *)mp->b_rptr;	/* ptr to ioctl info */

		switch (iocbp->ioc_cmd) {
		case AUDIO_GET_CH_NUMBER:
		case AUDIO_GET_CH_TYPE:
		case AUDIO_GET_NUM_CHS:
		case AUDIO_GET_AD_DEV:
		case AUDIO_GET_APM_DEV:
		case AUDIO_GET_AS_DEV:
			ATRACE_32("audio_sup_wput() "
			    "IOCTL calling audio_sup_wioctl()",
			    iocbp->ioc_cmd);
			rc = audio_sup_wioctl(q, mp, chptr);
			break;
		default:
			ATRACE("audio_sup_wput() IOCTL calling ch_wput()",
				chptr);
			if (chptr->ch_wput == NULL) {
				ATRACE("audio_sup_wput() IOCTL: bad ch_wput",
				    chptr->ch_wput);
				mp->b_datap->db_type = M_ERROR;
				mp->b_rptr = mp->b_datap->db_base;
				*(int *)mp->b_rptr = EINVAL;
				mp->b_wptr = mp->b_rptr + sizeof (uchar_t *);
				if (mp->b_cont) {
					freemsg(mp->b_cont);
					mp->b_cont = 0;
				}
				qreply(q, mp);
				return (0);
			}
			rc = chptr->ch_wput(q, mp);
		}
		break;
	case M_IOCDATA:
		ATRACE("audio_sup_wput() IOCDATA ", chptr);
		iocbp = (struct iocblk *)mp->b_rptr;	/* ptr to ioctl info */
		csp = (struct copyresp *)mp->b_rptr;	/* copy response ptr */
		cmd = (audio_i_state_t *)csp->cp_private; /* get state info */

		switch (cmd->command) {
		case COPY_OUT_CH_NUMBER:
		case COPY_OUT_CH_TYPE:
		case COPY_OUT_NUM_CHS:
		case COPY_OUT_AD_DEV:
		case COPY_OUT_APM_DEV:
		case COPY_OUT_AS_DEV:
			ATRACE_32("audio_sup_wput() "
			    "IOCDATA calling audio_sup_wiocdata()",
			    cmd->command);
			rc = audio_sup_wiocdata(q, mp, chptr);
			break;
		default:
			ATRACE("audio_sup_wput() IOCDATA calling ch_wput()",
			    chptr);
			if (chptr->ch_wput == NULL) {
				ATRACE("audio_sup_wput() IODATA: bad ch_wput",
				    chptr->ch_wput);
				mp->b_datap->db_type = M_ERROR;
				mp->b_rptr = mp->b_datap->db_base;
				*(int *)mp->b_rptr = EINVAL;
				mp->b_wptr = mp->b_rptr + sizeof (uchar_t *);
				if (mp->b_cont) {
					freemsg(mp->b_cont);
					mp->b_cont = 0;
				}
				qreply(q, mp);
				return (0);
			}
			rc = chptr->ch_wput(q, mp);
		}
		break;
	default:
		ATRACE("audio_sup_wput() calling ch_wput()", chptr);
		if (chptr->ch_wput == NULL) {
			ATRACE("audio_sup_wput() DEFAULT: bad ch_wput",
			    chptr->ch_wput);
			mp->b_datap->db_type = M_ERROR;
			mp->b_rptr = mp->b_datap->db_base;
			*(int *)mp->b_rptr = EINVAL;
			mp->b_wptr = mp->b_rptr + sizeof (uchar_t *);
			if (mp->b_cont) {
				freemsg(mp->b_cont);
				mp->b_cont = 0;
			}
			qreply(q, mp);
			return (0);
		}
		rc = chptr->ch_wput(q, mp);
	}

	ATRACE_32("audio_sup_wput() ch_wput() returned", rc);

	return (rc);

}	/* audio_sup_wput() */

/*
 * audio_sup_wsvc()
 *
 * Description:
 *	Make sure we have a valid function pointer. If we do we make the call.
 *
 * Arguments:
 *      queue_t		*q	Pointer to a queue
 *
 * Returns:
 *      0			Always returns 0
 */
int
audio_sup_wsvc(queue_t *q)
{
	audio_ch_t		*chptr = (audio_ch_t *)q->q_ptr;
	int			rc;

	ATRACE("in audio_sup_wsvc()", chptr);

	if (chptr->ch_wsvc == NULL) {
		ATRACE("audio_sup_wsvc() bad ch_wsvc", chptr->ch_wsvc);
		return (0);
	}

	ATRACE("audio_sup_wsvc() calling ch_wsvc()", chptr);

	rc = chptr->ch_wsvc(q);

	ATRACE_32("audio_sup_wsvc() ch_wsvc() returned", rc);

	return (rc);

}	/* audio_sup_wsvc() */

/*
 * Public Audio Personality Module Support Routines
 *
 * Channel Routines
 */

/*
 * audio_sup_free_ch()
 *
 * Description:
 *	This routine returns a channel structure to the device instance's
 *	pool of channel structures. All the various pointers must be freed
 *	and NULLed before this routine is called. It then does a cv_broadcast()
 *	to wake up any cv_wait()/cv_wait_sig() calls which might be waiting
 *	for a channel to be freed.
 *
 * Arguments:
 *	audio_ch_t	*chptr	The channel structure to free
 *
 * Returns:
 *	AUDIO_SUCCESS		No error
 *	AUDIO_FAILURE		One of the pointers is not set to NULL or
 *				chptr is not valid
 */
int
audio_sup_free_ch(audio_ch_t *chptr)
{
	audio_state_t		*statep = chptr->ch_statep;
	audio_apm_info_t	*apm_infop = chptr->ch_apm_infop;

	ATRACE("in audio_sup_free_ch()", chptr);

	ASSERT(chptr->ch_statep == statep);
	ASSERT(apm_infop != NULL);

	ASSERT(chptr->ch_msg_cnt == 0);

	ATRACE("audio_sup_free_ch() chptr", chptr);
	ATRACE("audio_sup_free_ch() ch_private", chptr->ch_private);
	ATRACE("audio_sup_free_ch() ch_info.info", chptr->ch_info.info);

	/* first we check the args & ptrs to make sure they are valid */
	if (chptr == NULL) {
		ATRACE("audio_sup_free_ch() chptr == NULL", chptr);
		return (AUDIO_FAILURE);
	}
	if (chptr->ch_private) {
		ATRACE("audio_sup_free_ch() chptr->ch_private != NULL",
		    chptr->ch_private);
		return (AUDIO_FAILURE);
	}
	if (chptr->ch_info.info) {
		ATRACE("audio_sup_free_ch() chptr->ch_info.info != NULL",
		    chptr->ch_info.info);
		return (AUDIO_FAILURE);
	}

	/* free the message list */
	ATRACE("audio_sup_free_ch() freeing saved messages", chptr);
	audio_sup_flush_msgs(chptr);

	/* update the channel counts */
	mutex_enter(apm_infop->apm_swlock);

	if (chptr->ch_dir & AUDIO_RECORD) {
		(*apm_infop->apm_in_chs)--;
	}
	if (chptr->ch_dir & AUDIO_PLAY) {
		(*apm_infop->apm_out_chs)--;
	}
	(*apm_infop->apm_chs)--;

	mutex_exit(apm_infop->apm_swlock);

	/* finally, clear the channel structure and make available for reuse */
	mutex_enter(&statep->as_lock);

	statep->as_ch_inuse--;

	ATRACE("audio_sup_free_ch() resetting channel info", chptr);

	/* remove the pointers to the channel structure, if present */
	if (chptr->ch_qptr) {
		chptr->ch_qptr->q_ptr = NULL;
		WR(chptr->ch_qptr)->q_ptr = NULL;
	}

	chptr->ch_qptr =		NULL;
	chptr->ch_msgs =		NULL;
	chptr->ch_msg_cnt =		0;
	chptr->ch_apm_infop =		NULL;
	chptr->ch_wput =		NULL;
	chptr->ch_wsvc =		NULL;
	chptr->ch_rput =		NULL;
	chptr->ch_rsvc =		NULL;
	chptr->ch_dir =			0;
	chptr->ch_flags =		0;
	chptr->ch_dev =			AUDIO_NO_DEV;
	chptr->ch_info.pid =		0;
	chptr->ch_info.dev_type =	UNDEFINED;

	/* the channel is freed, so send the broadcast */
	cv_broadcast(&statep->as_cv);

	mutex_exit(&statep->as_lock);

	ATRACE("audio_sup_free_ch() returning success", statep);

	return (AUDIO_SUCCESS);

}	/* audio_sup_free_ch() */

/*
 * Device Independent Driver Registration Routines
 */

/*
 * audio_sup_register_apm()
 *
 * Description:
 *	Register the Audio Personality Module with this instance of the
 *	Audio Driver. This provides a place to store state information
 *	for the APM.
 *
 *	We only allow one instance of a APM present for each instance of
 *	an Audio Driver.
 *
 *	NOTE: Instance and type are mandatory.
 *
 *	NOTE: It is okay for memory allocation to sleep.
 *
 * Arguments:
 *	audio_state_t	*statep		device state structure
 *	audio_device_type_e type	APM type
 *	int (*apm_open)(queue_t *, dev_t *, int, int, cred_t *)
 *					the APM's open() routine
 *	int (*apm_close)(queue_t *, int, cred_t *)
 *					the APM's close() routine
 *	kmutex_t	*swlock		APM structure state lock
 *	void		*private	APM private data
 *	void		*info		APM info structure
 *	void		*state		APM state structure
 *	int		*max_chs	Ptr to max ch supported by APM
 *	int		*max_play_chs	Ptr to max supported play channels
 *	int		*max_record_chs	Ptr to max supported record channels
 *	int		*chs		Ptr to current chs opened
 *	int		*play_chs	Ptr to current play channels open
 *	int		*record_chs	Ptr to current record channels open
 *	audio_device_t	*dev_info	APM device info
 *
 * Returns:
 *	valid pointer			The audio_apm_info structure registered
 *	NULL				Couldn't register the APM
 */
audio_apm_info_t *
audio_sup_register_apm(audio_state_t *statep, audio_device_type_e type,
	int (*apm_open)(queue_t *, dev_t *, int, int, cred_t *),
	int (*apm_close)(queue_t *, int, cred_t *),
	kmutex_t *swlock, void *private, void *info, void *state,
	int *max_chs, int *max_play_chs, int *max_record_chs,
	int *chs, int *play_chs, int *record_chs, audio_device_t *dev_info)
{
	audio_apm_info_t	*apm_infop;

	ATRACE_32("in audio_sup_register_apm()", type);

	/* we must have an open() and close() routine, as well as the locks */
	if (swlock == NULL || apm_open == NULL || apm_close == NULL) {
		ATRACE("audio_sup_register_apm() apm_open()", apm_open);
		ATRACE("audio_sup_register_apm() apm_close()", apm_close);
		return (NULL);
	}

	/* first make sure we haven't already registered this type before */
	mutex_enter(&statep->as_lock);

	for (apm_infop = statep->as_apm_info_list; apm_infop != NULL;
	    apm_infop = apm_infop->apm_next) {
		if (apm_infop->apm_type == type) {
			mutex_exit(&statep->as_lock);
			ATRACE("audio_sup_register_apm() "
			    "duplicate diaudio type", 0);
			return (NULL);
		}
	}

	ATRACE("audio_sup_register_apm() not a dupliacte, ok to continue", 0);

	apm_infop = kmem_zalloc(sizeof (*apm_infop), KM_SLEEP);

	cv_init(&apm_infop->apm_cv, NULL, CV_DRIVER, NULL);

	/* there are only so many channels per instance, enforce this */
	if (*max_chs > statep->as_max_chs) {
		*max_chs = statep->as_max_chs;
	}
	if (*max_record_chs > statep->as_max_chs) {
		*max_record_chs = statep->as_max_chs;
	}
	if (*max_play_chs > statep->as_max_chs) {
		*max_play_chs = statep->as_max_chs;
	}

	apm_infop->apm_swlock =		swlock;
	apm_infop->apm_open =		apm_open;
	apm_infop->apm_close =		apm_close;
	apm_infop->apm_info =		dev_info;
	apm_infop->apm_type =		type;
	apm_infop->apm_private =	private;
	apm_infop->apm_ad_infop =	info;
	apm_infop->apm_ad_state =	state;
	apm_infop->apm_max_chs =	max_chs;
	apm_infop->apm_max_out_chs =	max_play_chs;
	apm_infop->apm_max_in_chs =	max_record_chs;
	apm_infop->apm_chs =		chs;
	apm_infop->apm_out_chs =	play_chs;
	apm_infop->apm_in_chs =		record_chs;

	/* put at the head of the list */
	apm_infop->apm_next = statep->as_apm_info_list;
	if (apm_infop->apm_next) {
		ASSERT(apm_infop->apm_next->apm_previous == NULL);
		apm_infop->apm_next->apm_previous = apm_infop;
	}
	statep->as_apm_info_list = apm_infop;

	mutex_exit(&statep->as_lock);

	ATRACE("audio_sup_register_apm() returning successful", apm_infop);

	return (apm_infop);

}	/* audio_sup_register_apm() */

/*
 * audio_sup_unregister_apm()
 *
 * Description:
 *	Unregister the Audio Personality Module from this instance of the
 *	Audio Driver.
 *
 * Arguments:
 *	audio_state_t		*statep		device state structure
 *	audio_device_type_e	type;		APM type
 *
 * Returns:
 *	AUDIO_SUCCESS			Successfully registered the APM
 *	AUDIO_FAILURE			Couldn't unregister the APM
 */
int
audio_sup_unregister_apm(audio_state_t *statep, audio_device_type_e type)
{
	audio_apm_info_t	*apm_infop;

	ATRACE_32("in audio_sup_unregister_apm()", type);

	/* first make sure we have already registered this type before */
	mutex_enter(&statep->as_lock);

	for (apm_infop = statep->as_apm_info_list; apm_infop != NULL;
	    apm_infop = apm_infop->apm_next) {
		if (apm_infop->apm_type == type) {
			ATRACE("audio_sup_unregister_apm() "
			    "APM type registered", apm_infop);
			break;
		}
	}

	if (apm_infop == NULL) {
		mutex_exit(&statep->as_lock);
		ATRACE("audio_sup_unregister_apm() "
		    "APM type not registered", apm_infop);
		return (AUDIO_FAILURE);
	}

	if (apm_infop->apm_private) {
		mutex_exit(&statep->as_lock);
		ATRACE("audio_sup_unregister_apm() "
		    "private data not cleared", apm_infop->apm_private);
		return (AUDIO_FAILURE);
	}
	ATRACE("audio_sup_unregister_apm() ok to unreregister", apm_infop);

	/* remove the struct from the list */
	if (apm_infop == statep->as_apm_info_list) {	/* 1st/only on list */
		ASSERT(apm_infop->apm_previous == NULL);

		if (apm_infop->apm_next) {
			statep->as_apm_info_list = apm_infop->apm_next;
			statep->as_apm_info_list->apm_previous = NULL;
		} else {
			statep->as_apm_info_list = NULL;
		}
	} else if (apm_infop->apm_next == NULL) {	/* end of list */
		ASSERT(apm_infop != statep->as_apm_info_list);
		ASSERT(apm_infop->apm_previous != NULL);

		apm_infop->apm_previous->apm_next = NULL;
	} else {					/* middle of list */
		ASSERT(apm_infop != statep->as_apm_info_list);
		ASSERT(apm_infop->apm_next != NULL);
		ASSERT(apm_infop->apm_previous != NULL);

		apm_infop->apm_previous->apm_next = apm_infop->apm_next;
		apm_infop->apm_next->apm_previous = apm_infop->apm_previous;
	}

	mutex_exit(&statep->as_lock);

	cv_destroy(&apm_infop->apm_cv);

	ATRACE("audio_sup_unregister_apm() freeing apm_infop", apm_infop);
	kmem_free(apm_infop, sizeof (*apm_infop));

	ATRACE("audio_sup_unregister_apm() done", 0);

	return (AUDIO_SUCCESS);

}	/* audio_sup_unregister_apm() */

/*
 * Message Handling Routines
 */

/*
 * audio_sup_flush_msgs()
 *
 * Description:
 *	Flush all the messages queued up for a channel. We remain locked at
 *	all times so no one else can sneak in and grab a message.
 *
 * Arguments:
 *	audio_ch_t	*chptr	Pointer to the channel structure
 *
 * Returns:
 *	void
 */
void
audio_sup_flush_msgs(audio_ch_t *chptr)
{
	audio_msg_t	*tmp;
#ifdef DEBUG
	int		count = 0;
#endif

	ATRACE("in audio_sup_flush_msgs()", chptr);

	mutex_enter(&chptr->ch_msg_lock);

	while ((tmp = chptr->ch_msgs) != 0) {
		ATRACE("audio_sup_flush_msgs() freeing message", tmp);

#ifdef DEBUG
		count++;
#endif

		/* set up for next loop */
		chptr->ch_msgs = tmp->msg_next;

		ATRACE("audio_sup_flush_msgs() freeing msg", tmp);
		audio_sup_free_msg(tmp);
	}

	ASSERT(count == chptr->ch_msg_cnt);

	chptr->ch_msgs = 0;
	chptr->ch_msg_cnt = 0;

	mutex_exit(&chptr->ch_msg_lock);

	ATRACE("audio_sup_flush_msgs() finished", chptr);

}	/* audio_sup_flush_msgs() */

/*
 * audio_sup_free_msg()
 *
 * Description:
 *	Free the audio message.
 *
 *	NOTE: The message must already be off the list, so there isn't
 *		a need to lock the message list.
 *
 * Arguments:
 *	audio_msg_t	*msg		The message to free
 *
 * Returns:
 *	void
 */
void
audio_sup_free_msg(audio_msg_t *msg)
{
	ATRACE("in audio_sup_free_msg()", msg);

	if (msg == NULL) {
		ATRACE("audio_sup_free_msg() nothing to free", msg);
		return;
	}

	ATRACE("audio_sup_free_msg() msgs differ, orig", msg->msg_orig);
	ATRACE("audio_sup_free_msg() msgs differ, proc", msg->msg_proc);
	if (msg->msg_orig) {
		ATRACE("audio_sup_free_msg() "
		    "freeing original STREAMS message", msg->msg_orig);
		freemsg(msg->msg_orig);
	}
	if (msg->msg_proc) {
		ATRACE("audio_sup_free_msg() "
		    "freeing processed data buffer", msg->msg_proc);
		kmem_free(msg->msg_proc, msg->msg_psize);
	}

	ATRACE("audio_sup_free_msgs() freeing msg", msg);
	kmem_free(msg, sizeof (*msg));

	ATRACE("audio_sup_free_msg() done", 0);

}	/* audio_sup_free_msg() */

/*
 * audio_sup_get_msg()
 *
 * Description:
 *	Get the oldest STREAMS message off the channel's message list, which
 *	would be the first message.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel structure
 *
 * Returns:
 *	Valid audio_msg_t pointer	The message
 *	NULL				No messages available
 */
audio_msg_t *
audio_sup_get_msg(audio_ch_t *chptr)
{
	audio_msg_t	*tmp;

	ATRACE("in audio_sup_get_msg()", chptr);

	mutex_enter(&chptr->ch_msg_lock);

	tmp = chptr->ch_msgs;

	if (tmp) {
		/* set up for next time */
		chptr->ch_msgs = tmp->msg_next;

		chptr->ch_msg_cnt--;

		ASSERT(chptr->ch_msg_cnt >= 0);

		mutex_exit(&chptr->ch_msg_lock);

		ATRACE("audio_sup_get_msg() found a message to return", tmp);

		return (tmp);
	} else {
		ASSERT(chptr->ch_msg_cnt == 0);

		mutex_exit(&chptr->ch_msg_lock);

		ATRACE("audio_sup_get_msg() NO message found", chptr);

		return (NULL);
	}

}	/* audio_sup_get_msg() */

/*
 * audio_sup_get_msg_cnt()
 *
 * Description:
 *	Get the number of messages currently queued on the message list.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel structure
 *
 * Returns:
 *	>= 0				The number of queued messages
 */
int
audio_sup_get_msg_cnt(audio_ch_t *chptr)
{
	int		tmp;

	ATRACE("in audio_sup_get_msg_cnt()", chptr);

	ASSERT(chptr->ch_msg_cnt >= 0);

	tmp = chptr->ch_msg_cnt;

	ATRACE("audio_sup_get_msg_cnt() returning", tmp);

	return (tmp);

}	/* audio_sup_get_msg_cnt() */

/*
 * audio_sup_get_msg_size()
 *
 * Description:
 *	Get the number of bytes stored in messages that are currently
 *	queued on the message list. We only look at the original message,
 *	not the processed messages.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel structure
 *
 * Returns:
 *	>= 0				The number of bytes queued
 */
int
audio_sup_get_msg_size(audio_ch_t *chptr)
{
	audio_msg_t	*msgp;
	int		tmp = 0;

	ATRACE("in audio_sup_get_msg_size()", chptr);

	/* lock the structure */
	mutex_enter(&chptr->ch_msg_lock);

	ASSERT(chptr->ch_msg_cnt >= 0);

	msgp = chptr->ch_msgs;
	while (msgp != 0) {
		tmp += msgp->msg_orig->b_wptr - msgp->msg_orig->b_rptr;
		msgp = msgp->msg_next;
	}

	mutex_exit(&chptr->ch_msg_lock);

	ATRACE("audio_sup_get_msg_size() returning", tmp);

	return (tmp);

}	/* audio_sup_get_msg_size() */

/*
 * audio_sup_putback_msg()
 *
 * Description:
 *	Put the message back onto the list. It will be the first to be
 *	removed the next time audio_sup_get_msg() is called.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel structure
 *	audio_msg_t	*msg		The message to put back on the list
 *
 * Returns:
 *	void
 */
void
audio_sup_putback_msg(audio_ch_t *chptr, audio_msg_t *msg)
{
	ATRACE("in audio_sup_putback_msg()", chptr);

	if (msg == 0) {
		ATRACE("audio_sup_putback_msg() bad message pointer", msg);
		return;
	}

	ATRACE("audio_sup_putback_msg() putting msg back", msg);

	/* lock the message list */
	mutex_enter(&chptr->ch_msg_lock);

	msg->msg_next = chptr->ch_msgs;

	chptr->ch_msgs = msg;

	chptr->ch_msg_cnt++;

	ASSERT(chptr->ch_msg_cnt >= 1);

	mutex_exit(&chptr->ch_msg_lock);

	ATRACE("audio_sup_putback_msg() done", chptr);

}	/* audio_sup_putback_msg() */

/*
 * audio_sup_save_msg()
 *
 * Description:
 *	Save a STREAMS message on the channel's message list. New messages
 *	are placed at the end of the list.
 *
 *	CAUTION: This routine may be called from interrupt context, so memory
 *		allocation cannot sleep.
 *
 * Arguments:
 *	audio_ch_t	*chptr		Pointer to the channel structure
 *	mblk_t		*msg_orig	Pointer to the original message to save
 *	void		*msg_proc	Pointer to the processed buffer to save
 *	void		*msg_ptr	Pointer to the end of data in the buffer
 *	size_t		msg_psize	Size of the processed buffer
 *
 * Returns:
 *	AUDIO_SUCCESS		The message was successfully saved
 *	AUDIO_FAILURE		The message was not successfully saved
 */
int
audio_sup_save_msg(audio_ch_t *chptr, mblk_t *msg_orig, void *msg_proc,
	void *msg_eptr, size_t msg_psize)
{
	register audio_msg_t	*new;
	register audio_msg_t	*tmp;
#ifdef DEBUG
	int			count = 1;	/* while loop assumes > 0 msg */
#endif

	ATRACE("in audio_sup_save_msg()", chptr);

	/* first we allocate an audio_msg_t structure (zeros out next field) */
	if ((new = kmem_zalloc(sizeof (*new), KM_NOSLEEP)) == NULL) {
		ATRACE("audio_sup_save_msg() kmem_zalloc() failed", 0);
		return (AUDIO_FAILURE);
	}

	if (msg_orig) {
		new->msg_orig = msg_orig;	/* orig data from app */
		new->msg_optr = msg_orig->b_rptr; /* working pointer */
	}
	if (msg_proc) {
		new->msg_proc = msg_proc;	/* the processed data */
		new->msg_psize = msg_psize;	/* the size of the buffer */
		new->msg_eptr = msg_eptr;	/* ptr to end of good data */
		new->msg_pptr = msg_proc;	/* working pointer */
	}

	/* now we save the message */
	mutex_enter(&chptr->ch_msg_lock);

	/* see if this is the first message */
	if (chptr->ch_msgs == 0) {		/* it is */
		ASSERT(chptr->ch_msg_cnt == 0);
		chptr->ch_msgs = new;		/* next is already set to 0 */
		chptr->ch_msg_cnt = 1;
		mutex_exit(&chptr->ch_msg_lock);
		ATRACE("audio_sup_save_msg() first message", new);
		return (AUDIO_SUCCESS);
	}

	/* walk the list to the end */
	tmp = chptr->ch_msgs;
	while (tmp->msg_next != NULL) {
#ifdef DEBUG
		count++;
#endif
		tmp = tmp->msg_next;
	}

	ATRACE("audio_sup_save_msg() count", count);

	ASSERT(count == chptr->ch_msg_cnt);

	ATRACE("audio_sup_save_msg() saving message", new);

	tmp->msg_next = new;

	chptr->ch_msg_cnt++;

	ASSERT(chptr->ch_msg_cnt >= 1);

	mutex_exit(&chptr->ch_msg_lock);

	return (AUDIO_SUCCESS);

}	/* audio_sup_save_msg() */

/*
 * Device Instance Routines
 */

/*
 * audio_sup_get_dev_instance()
 *
 * Description:
 *	Get the device instance within a specific device driver. Similar
 *	to ddi_get_instance(), but with different arguments.
 *
 * Arguments:
 *	dev_t		dev	The device we are getting the instance of
 *	queue_t		*q	Pointer to a queue structure
 *
 * Returns:
 *	The instance
 */
int
audio_sup_get_dev_instance(dev_t dev, queue_t *q)
{
	/* first do the easy one */
	if (q) {
		ATRACE_32("audio_sup_get_dev_instance() Q returning",
		    ((audio_ch_t *)q->q_ptr)->ch_statep->as_dev_instance);

		return (((audio_ch_t *)q->q_ptr)->ch_statep->as_dev_instance);
	} else {
		ATRACE_32("audio_sup_get_dev_instance() returning",
		    (getminor((dev_t)dev) / minors_per_inst));

		return (getminor((dev_t)dev) / minors_per_inst);
	}

}	/* audio_sup_get_dev_instance() */

/*
 * Minor <--> Channel Routines
 */

/*
 * audio_sup_ch_to_minor()
 *
 * Description:
 *	Return the minor number of the channel.
 *
 * Arguments:
 *	audio_state_t	*statep		The device state structure
 *	int		channel		The device channel
 *
 * Returns:
 *	>= 0			The minor number of the channel
 */
int
audio_sup_ch_to_minor(audio_state_t *statep, int channel)
{
	ATRACE("in audio_sup_ch_to_minor(): statep", statep);
	ATRACE_32("audio_sup_ch_to_minor(): channel #", channel);

	ATRACE_32("audio_sup_ch_to_minor() returning",
	    ((statep->as_dev_instance * minors_per_inst) +
	    channel + audio_reserved));

	return ((statep->as_dev_instance * minors_per_inst) +
	    channel + audio_reserved);

}	/* audio_sup_ch_to_minor() */

/*
 * audio_sup_get_max_chs()
 *
 * Description:
 *	Get the maximum number of supported channels per instance.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	> 0			The number of minor numbers per instance
 */
int
audio_sup_get_max_chs(void)
{
	ATRACE_32("in audio_sup_get_max_chs() returning",
	    sup_chs);

	return (sup_chs);

}	/* audio_sup_get_max_chs() */

/*
 * audio_sup_get_minors_per_inst()
 *
 * Description:
 *	Get the number of minor numbers allowed per instance.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	> 0			The number of minor numbers per instance
 */
int
audio_sup_get_minors_per_inst(void)
{
	ATRACE_32("in audio_sup_get_minors_per_inst() returning",
	    minors_per_inst);

	return (minors_per_inst);

}	/* audio_sup_get_minors_per_inst() */

/*
 * audio_sup_getminor()
 *
 * Description:
 *	Normally a macro would be used to figure out the minor number. But
 *	we don't want the Audio Driver using the Audio Support Module's
 *	macros which might change. So we provide a call that will let us
 *	change what we are doing later on if we wish.
 *
 * Arguments:
 *	dev_t			dev	The device we are getting the type of
 *	audio_device_type_e	type	The device type we want the minor # of
 *
 * Returns:
 *	The device type
 *	AUDIO_FAILURE		Unrecognized audio device
 */
int
audio_sup_getminor(dev_t dev, audio_device_type_e type)
{
	ATRACE_32("in audio_sup_getminor()", dev);

	if (dev == NODEV) {
		int	minor;

		ASSERT(type);
		ATRACE_32("audio_sup_getminor() type", type);

		switch (type) {
		case AUDIO:	minor = AUDIO_MINOR_AUDIO;	break;
		case AUDIOCTL:	minor = AUDIO_MINOR_AUDIOCTL;	break;
		case WTABLE:	minor = AUDIO_MINOR_WAVE_TABLE;	break;
		case MIDI:	minor = AUDIO_MINOR_MIDI_PORT;	break;
		case ATIME:	minor = AUDIO_MINOR_TIME;	break;
		case USER1:	minor = AUDIO_MINOR_USER1;	break;
		case USER2:	minor = AUDIO_MINOR_USER2;	break;
		case USER3:	minor = AUDIO_MINOR_USER3;	break;
		default:	minor = AUDIO_FAILURE;		break;
		}

		ATRACE_32("audio_sup_getminor() returning minor", minor);

		return (minor);
	}

	ATRACE_32("audio_sup_getminor() returning",
	    (getminor(dev) % minors_per_inst));

	return ((getminor(dev) % minors_per_inst));

}	/* audio_sup_getminor() */

/*
 * audio_sup_minor_to_ch()
 *
 * Description:
 *	Convert a minor number to a channel number.
 *
 * Arguments:
 *	minor_t		minor	The minor number to convert
 *
 * Returns:
 *	>= 0			The channel number
 */
int
audio_sup_minor_to_ch(minor_t minor)
{
	ATRACE_32("in audio_sup_minor_to_ch()", minor);
	ATRACE_32("audio_sup_minor_to_ch() returning",
	    ((minor % minors_per_inst) - audio_reserved));

	return ((minor % minors_per_inst) - audio_reserved);

}	/* audio_sup_minor_to_ch() */

/*
 * State structure routines
 */

/*
 * audio_sup_get_state()
 *
 * Description:
 *	Get the state pointer for the audio device. We use one of two
 *	arguments to determine the device. The dip argument may be set to
 *	NULL, or the dev argument may be set to NODEV. Doing so forces the
 *	use of the other argument.
 *
 * Arguments:
 *	dev_info_t	*dip	dev_info_t ponter for the device
 *	dev_t		dev	Device name
 *
 * Returns:
 *	Valid pointer			Pointer to the state
 *	NULL				State pointer not found
 */
audio_state_t *
audio_sup_get_state(dev_info_t *dip, dev_t dev)
{
	audio_inst_info_t	*instp;
	audio_state_t		*statep;
	int			i;

	ATRACE("in audio_sup_get_state(), dip", dip);
	ATRACE("audio_sup_get_state() dev", dev);

	ASSERT(dip != NULL || dev != NODEV);

	/* normalize the device name, if given */
	if (dev != NODEV) {
		dev = makedevice(getmajor(dev),
		    (getminor(dev) / minors_per_inst) * minors_per_inst);
	}

	/* okay, now we've got to do it the hard way; protect the inst. list */

	/* see if we use the devinfo pointer, dev may or may not == NULL */
	if (dip) {
		ATRACE("audio_sup_get_state() dip valid", dip);
		mutex_enter(&inst_lock);
		for (i = 0; i < instances; i++) {
			if ((instp = ddi_get_soft_state(inst_statep, i)) ==
			    NULL) {
				cmn_err(CE_NOTE, "audiosup: get_state() "
				    "bad pointer, get state failed");
				break;
			}
			if (instp->dip == dip) {
				/* verify, if given */
				if (dev != NODEV && instp->dev != dev) {
				    mutex_exit(&inst_lock);
				    ATRACE("audio_sup_get_state() "
					"device name doesn't match, dev", dev);
				    ATRACE("audio_sup_get_state() "
					"instp->dev", instp->dev);
				    return (NULL);
				}
				mutex_exit(&inst_lock);
				statep = &instp->state;
				ATRACE("audio_sup_get_state() "
				    "found dip match, returning", statep);
				return (statep);
			}
		}
		mutex_exit(&inst_lock);
		ATRACE("audio_sup_get_state() no dip match", dip);
		return (NULL);
	} else if (dev != NODEV) {
		/* now try the device name, dip must == NULL */
		ATRACE("audio_sup_get_state() dev valid", dev);

		return (audio_sup_bind_dev(dev));
	}

	ATRACE("audio_sup_get_state() NULL arguments", 0);
	cmn_err(CE_NOTE, "audiosup: get_state() NULL arguments");
	return (NULL);

}	/* audio_sup_get_state() */

/*
 * Miscellaneous Routines
 */

/*
 * audio_sup_get_ch_type()
 *
 * Description:
 *	Normally a macro would be used to figure out the minor number. But
 *	we don't want the Audio Driver using the Audio Support Module's
 *	macros which might change. So we provide a call that will let use
 *	change what we are doing later on if we wish. The dev argument may
 *	be set to NODEV, or the q argument may be set to NULL. Doing so
 *	forces the routine to use the other argument.
 *
 * Arguments:
 *	dev_t		dev	The device we are getting the type of
 *	queue_t		*q	Pointer to the channel's STREAMS queue
 *
 * Returns:
 *	The device type
 *	AUDIO_FAILURE		Couldn't get the state structure, so failed
 */
audio_device_type_e
audio_sup_get_ch_type(dev_t dev, queue_t *q)
{
	audio_device_type_e	type;
	minor_t			minor;

	ATRACE("in audio_sup_get_ch_type()", dev);
	ATRACE("audio_sup_get_ch_type() q", q);

	ASSERT(dev != NODEV || q != NULL);

	if (dev == NODEV) {
		ASSERT(q);
		/* we've got a queue pointer instead, so return the type */
		ATRACE_32("audio_sup_get_ch_type() q --> type",
		    ((audio_ch_t *)q->q_ptr)->ch_info.dev_type);
		return (((audio_ch_t *)q->q_ptr)->ch_info.dev_type);
	}

	/* this could be one of the reserved, or an allocated channel */
	minor = getminor(dev) % minors_per_inst;
	if (minor < audio_reserved) {
		ATRACE_32("audio_sup_get_ch_type() reserved minor", minor);

		switch (minor) {
		case AUDIO_MINOR_AUDIO:		type = AUDIO;		break;
		case AUDIO_MINOR_AUDIOCTL:	type = AUDIOCTL;	break;
		case AUDIO_MINOR_WAVE_TABLE:	type = WTABLE;		break;
		case AUDIO_MINOR_MIDI_PORT:	type = MIDI;		break;
		case AUDIO_MINOR_TIME:		type = TIME;		break;
		case AUDIO_MINOR_USER1:		type = USER1;		break;
		case AUDIO_MINOR_USER2:		type = USER2;		break;
		case AUDIO_MINOR_USER3:		type = USER3;		break;
		default:			type = UNDEFINED;	break;
		}

		ATRACE_32("audio_sup_get_ch_type() reserved, returning", type);

		return (type);
	} else {
		audio_state_t	*statep;
		audio_ch_t	*chptr;

		ATRACE_32("audio_sup_get_ch_type() allocated channel", minor);

		if ((statep = audio_sup_get_state(NULL, dev)) == NULL) {
			ATRACE("audio_sup_get_ch_type() "
			    "audio_sup_get_state() failed", 0);
			return ((audio_device_type_e)AUDIO_FAILURE);
		}

		ATRACE("audio_sup_get_ch_type() statep", statep);

		chptr = &statep->as_channels[audio_sup_minor_to_ch(minor)];

		ATRACE("audio_sup_get_ch_type() chptr", chptr);

		ATRACE_32("audio_sup_get_ch_type() returning type",
		    chptr->ch_info.dev_type);

		return (chptr->ch_info.dev_type);
	}

}	/* audio_sup_get_ch_type() */

/*
 * audio_sup_get_channel_number()
 *
 * Description:
 *	Get the channel number for the audio queue.
 *
 * Arguments:
 *	queue_t		*q	Pointer to a queue structure
 *
 * Returns:
 *	channel number		The channel number for the audio queue.
 */
int
audio_sup_get_channel_number(queue_t *q)
{
	ATRACE("in audio_sup_get_channel_number()", q);

	ATRACE("audio_sup_get_channel_number() returning",
	    ((audio_ch_t *)q->q_ptr)->ch_info.ch_number);
	return (((audio_ch_t *)q->q_ptr)->ch_info.ch_number);

}	/* audio_sup_get_channel_number() */

/*
 * audio_sup_get_apm_info()
 *
 * Description:
 *	Get the audio_apm_info structure for the audio instance and
 *	type passed in.
 *
 *	NOTE: Since the apm_info list is created when the driver is
 *		attached it should never change during normal operation
 *		of the audio device. Therefore we don't need to lock
 *		the list while we traverse it.
 *
 * Arguments:
 *	audio_state_t		*statep		device state structure
 *	audio_device_type_e	type		APM type
 *
 * Returns:
 *	valid pointer		Ptr to the returned audio_apm_info struct
 *	NULL			audio_apm_info struct not found
 */
audio_apm_info_t *
audio_sup_get_apm_info(audio_state_t *statep, audio_device_type_e type)
{
	audio_apm_info_t	*apm_infop;

	ATRACE("in audio_sup_get_apm_info()", statep);

	/* sanity check */
	if (type == UNDEFINED) {
		ATRACE("audio_sup_get_apm_info() returning NULL (fail)", 0);
		return (NULL);
	}

	apm_infop = statep->as_apm_info_list;

	while (apm_infop != NULL) {
		if (apm_infop->apm_type == type) {
			ATRACE_32("audio_sup_get_apm_info() found type", type);
			break;
		}
		apm_infop = apm_infop->apm_next;
	}

	/* make sure we got a stucture */
	if (apm_infop == NULL) {
		ATRACE_32("audio_sup_get_apm_info() didn't find type", type);
		return (NULL);
	}

	ATRACE("audio_sup_get_apm_info() returning", apm_infop);

	return (apm_infop);

}	/* audio_sup_get_apm_info() */

/*
 * audio_sup_get_info()
 *
 * Description:
 *	Get the info structure for the audio queue.
 *
 * Arguments:
 *	queue_t		*q	Pointer to a queue structure
 *
 * Returns:
 *	valid pointer		Ptr to the returned audio_apm_info struct
*/
void *
audio_sup_get_info(queue_t *q)
{
	ATRACE("in audio_sup_get_info()", q);

	ATRACE("audio_sup_get_info() returning",
	    ((audio_ch_t *)q->q_ptr)->ch_info.info);

	return (((audio_ch_t *)q->q_ptr)->ch_info.info);

}	/* audio_sup_get_info() */

/*
 * Private Support Routines For The Audio Support Module
 */

/*
 * audio_sup_bind_dev()
 *
 * Description:
 *	This routine is used to associate the device number with the dev_info_t
 *	pointer. It returns the device's state structure when it is done
 *	binding. It is called by any routine that has a dev_t for an argument,
 *	but needs a dev_info_t.
 *
 *	NOTE: The dev_info_t pointer is set in audio_sup_reg_instance().
 *
 *	NOTE: We convert the minor device so that it points to the first
 *		minor device for the Audio Driver instance of that device.
 *		That way it doesn't matter which minor number is used to
 *		find the Audio Driver instance.
 *
 * Arguments:
 *	dev_t		dev	The deivce number
 *
 * Returns:
 *	valid pointer		Pointer to the state structure
 *	NULL			Bind failed
 */
static audio_state_t *
audio_sup_bind_dev(dev_t dev)
{
	dev_info_t		*my_dip;
	audio_inst_info_t	*instp;
	audio_state_t		*statep;
	int			i;

	ATRACE("in audio_sup_bind_dev()", dev);

	/* get the dip for this device */
	if ((my_dip = dev_get_dev_info(dev, 0)) == 0) {
		/* dev_get_dev_info() failed so don't call ddi_rele_driver() */
		ATRACE("audio_sup_bind_dev() dev_get_dev_info() failed", dev);
		cmn_err(CE_WARN, "audiosup: bind_dev() dev_get_dev_info() "
		    "failed, run drvconfig");
		return (NULL);
	}
	ATRACE("audio_sup_bind_dev() dev_get_dev_info() my_dip", my_dip);

	/* dev_get_dev_info() locks down dev */
	ddi_rele_driver(getmajor(dev));

	/* normalize the device number */
	dev = makedevice(getmajor(dev),
	    (getminor(dev) / minors_per_inst) * minors_per_inst);

	/* protect the instance list */
	mutex_enter(&inst_lock);

	/* look for the dev_info_t, which we had better already have */
	for (i = 0; i < instances; i++) {
		if ((instp = ddi_get_soft_state(inst_statep, i)) == NULL) {
			mutex_exit(&inst_lock);
			return (NULL);
		}

		if (instp->dip == my_dip) {
			if (instp->dev != NODEV && instp->dev != dev) {
			    mutex_exit(&inst_lock);
			    ATRACE("audio_sup_bind_dev() "
				"instance list corruption", inst_statep);
			    cmn_err(CE_NOTE, "audiosup: bind_dev() "
				"instance list corruption");

			    return (NULL);
			}

			instp->dev = dev;
			mutex_exit(&inst_lock);
			statep = &instp->state;
			ATRACE("audio_sup_bind_dev() bound", statep);
			return (statep);
		}
	}

	mutex_exit(&inst_lock);

	ATRACE("audio_sup_bind_dev() instance list corruption", inst_statep);

	cmn_err(CE_NOTE,
	    "audiosup: bind_dev() instance list corruption, dip not found");

	return (NULL);

}	/* audio_sup_bind_dev() */

/*
 * audio_sup_free_instance()
 *
 * Description:
 *	This routine is used to clear entries in the instance list that
 *	were filled in by audio_sup_reg_instance(). audio_sup_reg_instance()
 *	will grow the list as needed. However, we don't ever shrink the
 *	list. That way the instance number is the list index.
 *
 * Arguments:
 *	dev_info_t	*dip		dev_info_t pointer for the device
 *
 * Returns:
 *	void
 */
static void
audio_sup_free_instance(dev_info_t *dip)
{
	audio_inst_info_t	*instp;
	int			flag = 0;
	int			i;

	ATRACE("in audio_sup_free_instance()", dip);

	/* protect the instance list */
	mutex_enter(&inst_lock);

	/* we search the whole list looking for the entry and dups */
	for (i = 0; i < instances; i++) {
		if ((instp = ddi_get_soft_state(inst_statep, i)) == NULL) {
			mutex_exit(&inst_lock);
			cmn_err(CE_NOTE, "audiosup: free() "
			    "bad pointer, get state failed");
			return;
		}
		if (instp->dip == dip) {
			ATRACE_32("audio_sup_free_instance() found match", i);
			bzero(&instp->state, sizeof (instp->state));
			instp->dip = NULL;
			instp->dev = NODEV;
			flag++;
		}
	}

	if (flag == 1) {
		ATRACE("audio_sup_free_instance() flag = 1", 1);
		goto done;		/* the high runner case */
	} else if (flag == 0) {
		ATRACE("audio_sup_free_instance() flag = 0", 0);
		cmn_err(CE_NOTE,
		    "audiosup: free() couldn't find dip to free");
	} else {
		ATRACE_32("audio_sup_free_instance() flag > 1", flag);
		cmn_err(CE_NOTE,
		    "audiosup: free() duplicate entries freed");
	}

done:
	mutex_exit(&inst_lock);

	ATRACE("audio_sup_free_instance() done", 0);

}	/* audio_sup_free_instance() */

/*
 * audio_sup_reg_instance()
 *
 * Description:
 *	Add an entry in the instance list. The whole list is scanned to
 *	make sure we don't ever get a duplicate entry. The list will grow
 *	over time, but when an instance is deallocated the list doesn't
 *	shrink. That way the audio instance number can be the list index.
 *
 * Arguments:
 *	dev_info_t	*dip	dev_info_t ponter for the device, what we use
 *				to find the instance
 *
 * Returns:
 *	Valid pointer		Valid instance
 *	NULL			Isntance already registered
 */
static audio_state_t *
audio_sup_reg_instance(dev_info_t *dip)
{
	audio_state_t		*statep;
	audio_inst_info_t	*instp;
	int			i;

	ATRACE("in audio_sup_reg_instance()", dip);

	/* protect the instance list */
	mutex_enter(&inst_lock);

	/* first make sure this "dip" hasn't already been registered */
	for (i = 0; i < instances; i++) {
		if ((instp = ddi_get_soft_state(inst_statep, i)) == NULL) {
			ATRACE_32("audio_sup_reg_instance() "
			    "bogus pointer, get state #1 failed", 0);
			cmn_err(CE_NOTE, "audiosup: reg() "
			    "bad pointer, get state #1 failed");
			break;
		}
		if (instp->dip == dip) {
			mutex_exit(&inst_lock);
			ATRACE_32("audio_sup_reg_instance() "
			    "instance already registered", i);
			cmn_err(CE_NOTE, "audiosup: reg() "
			    "instance %d already registered", i);
			return (NULL);
		}
	}

	/* "dip" not registered, so see if we have an empty slot */
	ATRACE("audio_sup_reg_instance() dip not registerd", dip);
	for (i = 0; i < instances; i++) {
		if ((instp = ddi_get_soft_state(inst_statep, i)) == NULL) {
			ATRACE_32("audio_sup_reg_instance() "
			    "bogus pointer, get state #2 failed", 0);
			cmn_err(CE_NOTE, "audiosup: reg() "
			    "bad pointer, get state #2 failed");
			break;
		}
		if (instp->dip == NULL) {	/* yes! an empty slot */
			/* allocate a zeroed out state structure */
			mutex_exit(&inst_lock);
			statep = &instp->state;
			instp->dip = dip;
			bzero(statep, sizeof (*statep));	/* zero it */
			ATRACE_32(
			    "audio_sup_reg_instance() reused empty slot", i);
			return (statep);
		}

	}

	ASSERT(i == instances);

	/* no empty slot, so grow the list */
	ATRACE_32("audio_sup_reg_instance() grow list", instances);
	if (ddi_soft_state_zalloc(inst_statep, instances) == DDI_FAILURE) {
		mutex_exit(&inst_lock);
		ATRACE_32("audio_sup_reg_instance() "
		    "ddi_soft_state_zalloc() failed", 0);
		cmn_err(CE_NOTE, "audiosup: reg() "
		    "couldn't allocate new instance structure");
		return (NULL);
	}

	/* allocate a zeroed out state structure, and increment instances */
	if ((instp = ddi_get_soft_state(inst_statep, instances++)) == NULL) {
		mutex_exit(&inst_lock);
		ATRACE_32("audio_sup_reg_instance() "
		    "ddi_get_soft_state() failed", 0);
		cmn_err(CE_NOTE, "audiosup: reg() "
		    "couldn't get soft state structure");
		return (NULL);
	}

	/* now complete the registration, using the new list entry */
	statep = &instp->state;
	instp->dip = dip;
	instp->dev = NODEV;
	bzero(statep, sizeof (*statep));	/* zero it */

	ATRACE("audio_sup_reg_instance() inst_statep", inst_statep);
	ATRACE("audio_sup_reg_instance() instp", instp);
	ATRACE("audio_sup_reg_instance() statep", statep);

	mutex_exit(&inst_lock);

	ATRACE("audio_sup_reg_instance() returning", statep);

	return (statep);

}	/* audio_sup_reg_instance() */

/*
 * audio_sup_wiocdata()
 *
 * Description:
 *	This routine is called by audio_sup_wput() to process the IOCDATA
 *	messages that belong to the Audio Support Module's routines.
 *
 *	We only support transparent ioctls.
 *
 *	WARNING: Don't forget to free the mblk_t struct used to hold private
 *		data. The ack: and nack: jump points take care of this.
 *
 *	WARNING: Don't free the private mblk_t structure if the command is
 *		going to call qreply(). This frees the private date that will
 *		be needed for the next M_IOCDATA message.
 *
 * Arguments:
 *	queue_t		*q	Pointer to the STREAMS queue
 *	mblk_t		*mp	Pointer to the message block
 *	audio_ch_t	*chptr	Pointer to this channel's state information
 *
 * Returns:
 *	0			Always returns a 0, becomes on return for
 *				audio_sup_wput()
 */
static int
audio_sup_wiocdata(queue_t *q, mblk_t *mp, audio_ch_t *chptr)
{
	audio_state_t		*statep = chptr->ch_statep;
	struct iocblk		*iocbp;
	struct copyreq		*cqp;
	struct copyresp		*csp;
	audio_i_state_t		*cmd;
	int			error;

	ATRACE("in audio_sup_wiocdata()", chptr);

	iocbp = (struct iocblk *)mp->b_rptr;	/* pointer to ioctl info */
	csp = (struct copyresp *)mp->b_rptr;	/* set up copy response ptr */
	cmd = (audio_i_state_t *)csp->cp_private;	/* get state info */
	cqp =  (struct copyreq *)mp->b_rptr;	/* set up copy requeset ptr */

	/* make sure this is a transparent ioctl and we have good pointers */
	if (statep == 0 || chptr == 0) {
		ATRACE("audio_sup_wiocdata() no statep or chptr", statep);
		error = EINVAL;
		goto nack;
	}

	/* make sure we've got a good return value */
	if (csp->cp_rval) {
		ATRACE("audio_sup_wiocdata() bad return value", csp->cp_rval);
		error = EINVAL;
		goto nack;
	}

	/* find the command */
	ATRACE_32("audio_sup_wiocdata() command", cmd->command);
	switch (cmd->command) {

	case COPY_OUT_CH_NUMBER:	/* AUDIO_GET_CH_NUMBER */
		ATRACE("audio_sup_wiocdata() COPY_OUT_CH_NUMBER", chptr);
		if (csp->cp_cmd != AUDIO_GET_CH_NUMBER) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_OUT_CH_TYPE:		/* AUDIO_GET_CH_TYPE */
		ATRACE("audio_sup_wiocdata() COPY_OUT_CH_TYPE", chptr);
		if (csp->cp_cmd != AUDIO_GET_CH_TYPE) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_OUT_NUM_CHS:		/* AUDIO_GET_NUM_CHS */
		ATRACE("audio_sup_wiocdata() COPY_OUT_CHANNELS", chptr);
		if (csp->cp_cmd != AUDIO_GET_NUM_CHS) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_OUT_AD_DEV:		/* AUDIO_GET_AD_DEV */
		ATRACE("audio_sup_wiocdata() COPY_OUT_AD_DEV", chptr);
		if (csp->cp_cmd != AUDIO_GET_AD_DEV) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_OUT_APM_DEV:		/* AUDIO_GET_APM_DEV */
		ATRACE("audio_sup_wiocdata() COPY_OUT_APM_DEV", chptr);
		if (csp->cp_cmd != AUDIO_GET_APM_DEV) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	case COPY_OUT_AS_DEV:		/* AUDIO_GET_AS_DEV */
		ATRACE("audio_sup_wiocdata() COPY_OUT_AS_DEV", chptr);
		if (csp->cp_cmd != AUDIO_GET_AS_DEV) {
			error = EINVAL;
			goto nack;
		}
		goto ack;

	default:
		ATRACE("audio_sup_wiocdata() default", chptr);
		error = EINVAL;
		goto nack;
	}

ack:
	ATRACE("audio_sup_wiocdata() ack", chptr);

	if (csp->cp_private) {
		ATRACE("audio_sup_wiocdata() freeing csp->cp_private",
		    csp->cp_private);
		kmem_free(csp->cp_private, sizeof (audio_i_state_t));
		csp->cp_private = NULL;
	}
	if (cqp->cq_private) {
		ATRACE("audio_sup_wiocdata() freeing cqp->cq_private",
		    cqp->cq_private);
		kmem_free(cqp->cq_private, sizeof (audio_i_state_t));
		cqp->cq_private = NULL;
	}
	iocbp->ioc_rval = 0;
	iocbp->ioc_count = 0;
	iocbp->ioc_error = 0;		/* just in case */
	mp->b_wptr = mp->b_rptr + sizeof (*iocbp);
	mp->b_datap->db_type = M_IOCACK;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = 0;
	}
	qreply(q, mp);

	ATRACE("audio_sup_wiocdata() returning success", chptr);

	return (0);

nack:
	ATRACE("audio_sup_wiocdata() nack", chptr);

	if (csp->cp_private) {
		ATRACE("audio_sup_wiocdata() freeing csp->cp_private #2",
		    csp->cp_private);
		kmem_free(csp->cp_private, sizeof (audio_i_state_t));
		csp->cp_private = NULL;
	}
	if (cqp->cq_private) {
		ATRACE("audio_sup_wiocdata() freeing cqp->cq_private #2",
		    cqp->cq_private);
		kmem_free(cqp->cq_private, sizeof (audio_i_state_t));
		cqp->cq_private = NULL;
	}
	iocbp->ioc_rval = -1;
	iocbp->ioc_count = 0;
	iocbp->ioc_error = error;
	mp->b_wptr = mp->b_rptr + sizeof (*iocbp);
	mp->b_datap->db_type = M_IOCNAK;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = 0;
	}
	qreply(q, mp);

	ATRACE("audio_sup_wiocdata() returning success", chptr);

	return (0);

}	/* audio_sup_wiocdata() */

/*
 * audio_sup_wioctl()
 *
 * Description:
 *	This routine is called by audio_sup_wput() to process all M_IOCTL
 *	messages that the Audio Support Module provides.
 *
 *	We only support transparent ioctls. Since this is a driver we
 *	nack unrecognized ioctls.
 *
 *	The following ioctls are supported:
 *		AUDIO_GET_CH_NUMBER
 *		AUDIO_GET_CH_TYPE
 *		AUDIO_GET_NUM_CHS
 *		AUDIO_GET_AD_DEV
 *		AUDIO_GET_APM_DEV
 *		AUDIO_GET_AS_DEV
 *		unknown		nack back up the queue
 *
 *	CAUTION: This routine is called from interrupt context, so memory
 *		allocation cannot sleep.
 *
 *	WARNING: There cannot be any locks owned by calling rutines.
 *
 * Arguments:
 *	queue_t		*q	Pointer to the STREAMS queue
 *	mblk_t		*mp	Pointer to the message block
 *	audio_ch_t	*chptr	Pointer to this channel's state information
 *
 * Returns:
 *	0			Always returns a 0, becomes a return for
 *				audio_sup_wput()
 */
static int
audio_sup_wioctl(queue_t *q, mblk_t *mp, audio_ch_t *chptr)
{
	audio_state_t		*statep = chptr->ch_statep;
	audio_device_t		*devp;
	struct iocblk		*iocbp;
	struct copyreq		*cqp;
	audio_i_state_t		*state = NULL;
	audio_device_type_e	type = chptr->ch_info.dev_type;
	int			error;

	ATRACE("in audio_sup_wioctl()", chptr);

	ASSERT(!mutex_owned(&statep->as_lock));

	iocbp = (struct iocblk *)mp->b_rptr;	/* pointer to ioctl info */
	cqp = (struct copyreq *)mp->b_rptr;	/* set up copyreq ptr */

	/* make sure we have good pointers */
	if (statep == NULL || chptr == NULL) {
		ATRACE("audio_sup_wioctl() no statep or chptr", statep);
		error = EINVAL;
		goto nack;
	}

	/* make sure this is a transparent ioctl */
	if (iocbp->ioc_count != TRANSPARENT) {
		ATRACE_32("audio_sup_wioctl() not TRANSPARENT",
			iocbp->ioc_count);
		error = EINVAL;
		goto nack;
	}

	/* get a buffer for private data */
	if ((state = kmem_alloc(sizeof (*state), KM_NOSLEEP)) == NULL) {
		ATRACE("audio_sup_wioctl() state kmem_alloc() failed", 0);
		error = ENOMEM;
		goto nack;
	}

	ATRACE_32("audio_sup_wioctl() command", iocbp->ioc_cmd);
	switch (iocbp->ioc_cmd) {

	case AUDIO_GET_CH_NUMBER:
		ATRACE("audio_sup_wioctl() AUDIO_GET_CH_NUMBER", chptr);

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_CH_NUMBER;	/* M_IOCDATA command */
			/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (int);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* only an int, so reuse the data block without checks */
		*(int *)mp->b_cont->b_rptr = chptr->ch_info.ch_number;
		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (int);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("audio_sup_wioctl() AUDIO_GET_CH_NUMBER returning",
			chptr);

		return (0);

		/* end AUDIO_GET_CH_NUMBER */

	case AUDIO_GET_CH_TYPE:
		ATRACE("audio_sup_wioctl() AUDIO_GET_CH_TYPE", chptr);

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_CH_TYPE;	/* M_IOCDATA command */
			/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (int);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* only an int, so reuse the data block without checks */
		*(int *)mp->b_cont->b_rptr = type;
		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (AUDIO);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("audio_sup_wioctl() AUDIO_GET_CH_TYPE returning", chptr);

		return (0);

		/* end AUDIO_GET_CH_TYPE */

	case AUDIO_GET_NUM_CHS:
		ATRACE("audio_sup_wioctl() AUDIO_GET_NUM_CHS", chptr);

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_NUM_CHS;	/* M_IOCDATA command */
			/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (int);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* only an int, so reuse the data block without checks */
		*(int *)mp->b_cont->b_rptr = *chptr->ch_apm_infop->apm_max_chs;
		mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof (int);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("audio_sup_wioctl() AUDIO_GET_NUM_CHS returning",
		    chptr);

		return (0);

		/* end AUDIO_GET_NUM_CHS */

	case AUDIO_GET_AD_DEV:
		ATRACE("audio_sup_wioctl() AUDIO_GET_AD_DEV", chptr);

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_AD_DEV;	/* M_IOCDATA command */
			/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (*chptr->ch_dev_info);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* put the data in the buffer, but try to reuse it first */
		if ((mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) < sizeof (audio_device_t)) {
			freemsg(mp->b_cont);
			mp->b_cont = (struct msgb *)allocb(
			    sizeof (*chptr->ch_dev_info), BPRI_MED);
			if (mp->b_cont == NULL) {
				error = EAGAIN;
				goto nack;
			}
		}

		/*
		 * We don't bother to lock the state structure because this
		 * is static data.
		 */

		devp = (audio_device_t *)mp->b_cont->b_rptr;

		bcopy(chptr->ch_dev_info, devp, sizeof (*chptr->ch_dev_info));

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (*chptr->ch_dev_info);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("audio_sup_wioctl() AUDIO_GET_AD_DEV returning", chptr);

		return (0);

	case AUDIO_GET_APM_DEV:
		ATRACE("audio_sup_wioctl() AUDIO_GET_APM_DEV", chptr);

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_APM_DEV;	/* M_IOCDATA command */
			/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (*chptr->ch_dev_info);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* put the data in the buffer, but try to reuse it first */
		if ((mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) <
		    sizeof (*chptr->ch_dev_info)) {
			freemsg(mp->b_cont);
			mp->b_cont = (struct msgb *)allocb(
				sizeof (*chptr->ch_dev_info), BPRI_MED);
			if (mp->b_cont == NULL) {
				error = EAGAIN;
				goto nack;
			}
		}

		/*
		 * We don't bother to lock the state structure because this
		 * is static data.
		 */

		devp = (audio_device_t *)mp->b_cont->b_rptr;

		bcopy(chptr->ch_apm_infop->apm_info, devp,
		    sizeof (*chptr->ch_dev_info));

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (*chptr->ch_dev_info);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("audio_sup_wioctl() AUDIO_GET_APM_DEV returning", chptr);

		return (0);

	case AUDIO_GET_AS_DEV:
		ATRACE("audio_sup_wioctl() AUDIO_GET_AS_DEV", chptr);

		/* save state for M_IOCDATA processing */
		state->command = COPY_OUT_AS_DEV;	/* M_IOCDATA command */
			/* user space addr */
		state->address = (caddr_t)(*(caddr_t *)mp->b_cont->b_rptr);

		cqp->cq_private = (mblk_t *)state;
		cqp->cq_flag = 0;
		cqp->cq_size = sizeof (*chptr->ch_dev_info);
		cqp->cq_addr = state->address;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_wptr = mp->b_rptr + sizeof (*cqp);

		/* put the data in the buffer, but try to reuse it first */
		if ((mp->b_cont->b_datap->db_lim -
		    mp->b_cont->b_datap->db_base) <
		    sizeof (*chptr->ch_dev_info)) {
			freemsg(mp->b_cont);
			mp->b_cont = (struct msgb *)allocb(
			    sizeof (*chptr->ch_dev_info), BPRI_MED);
			if (mp->b_cont == NULL) {
				error = EAGAIN;
				goto nack;
			}
		}

		/*
		 * We don't bother to lock the state structure because this
		 * is static data.
		 */

		devp = (audio_device_t *)mp->b_cont->b_rptr;

		bcopy(&audio_device_info, devp, sizeof (*chptr->ch_dev_info));

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (*chptr->ch_dev_info);

		/* send the copy out request */
		qreply(q, mp);

		ATRACE("audio_sup_wioctl() AUDIO_GET_AS_DEV returning", chptr);

		return (0);

	default:	/* this should never happen */
		ATRACE_32("audio_sup_wioctl() default", iocbp->ioc_cmd);
		error = EINVAL;
		goto nack;
	}

ack:
	ATRACE("audio_sup_wioctl() ack", chptr);

	if (state) {		/* free allocated state memory */
		ATRACE("audio_sup_wioctl() ack freeing state", state);
		kmem_free(state, sizeof (audio_i_state_t));
	}

	iocbp->ioc_rval = 0;
	iocbp->ioc_error = 0;		/* just in case */
	iocbp->ioc_count = 0;
	mp->b_datap->db_type = M_IOCACK;
	qreply(q, mp);

	ATRACE("audio_sup_wioctl() returning successful", chptr);

	return (0);

nack:
	ATRACE("audio_sup_wioctl() nack", chptr);

	if (state) {		/* free allocated state memory */
		ATRACE("audio_sup_wioctl() nack freeing state", state);
		kmem_free(state, sizeof (audio_i_state_t));
	}

	iocbp->ioc_rval = -1;
	iocbp->ioc_error = error;
	iocbp->ioc_count = 0;
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);

	ATRACE("audio_sup_wioctl() returning failure", chptr);

	return (0);

}	/* audio_sup_wioctl() */
