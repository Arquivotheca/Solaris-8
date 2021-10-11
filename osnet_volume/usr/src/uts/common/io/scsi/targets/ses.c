/*
 * Enclosure Services Device target driver
 *
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)ses.c	1.34	99/09/07 SMI"

#include <sys/modctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/scsi/scsi.h>
#include <sys/scsi/generic/status.h>
#include <sys/scsi/targets/sesio.h>
#include <sys/scsi/targets/ses.h>



/*
 * Power management defines (should be in a common include file?)
 */
#define	PM_HARDWARE_STATE_PROP		"pm-hardware-state"
#define	PM_NEEDS_SUSPEND_RESUME		"needs-suspend-resume"


/*
 * Eventually these have to go away.
 */
#define	DEPRECATED_INTERFACES	1


/*
 * Global Driver Data
 */
int ses_io_time = SES_IO_TIME;

static int ses_retry_count = SES_RETRY_COUNT * SES_RETRY_MULTIPLIER;

#ifdef	DEBUG
int ses_debug = 0;
#else	/* DEBUG */
#define	ses_debug	0
#endif	/* DEBUG */


/*
 * External Enclosure Functions
 */
extern int ses_softc_init(ses_softc_t *, int);
extern int ses_init_enc(ses_softc_t *);
extern int ses_get_encstat(ses_softc_t *, int);
extern int ses_set_encstat(ses_softc_t *, uchar_t, int);
extern int ses_get_objstat(ses_softc_t *, ses_objarg *, int);
extern int ses_set_objstat(ses_softc_t *, ses_objarg *, int);

extern int safte_softc_init(ses_softc_t *, int);
extern int safte_init_enc(ses_softc_t *);
extern int safte_get_encstat(ses_softc_t *, int);
extern int safte_set_encstat(ses_softc_t *, uchar_t, int);
extern int safte_get_objstat(ses_softc_t *, ses_objarg *, int);
extern int safte_set_objstat(ses_softc_t *, ses_objarg *, int);

extern int sen_softc_init(ses_softc_t *, int);
extern int sen_init_enc(ses_softc_t *);
extern int sen_get_encstat(ses_softc_t *, int);
extern int sen_set_encstat(ses_softc_t *, uchar_t, int);
extern int sen_get_objstat(ses_softc_t *, ses_objarg *, int);
extern int sen_set_objstat(ses_softc_t *, ses_objarg *, int);

/*
 * Local Function prototypes
 */
static int ses_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int ses_identify(dev_info_t *);
static int ses_probe(dev_info_t *);
static int ses_attach(dev_info_t *, ddi_attach_cmd_t);
static int ses_detach(dev_info_t *, ddi_detach_cmd_t);

static int is_enc_dev(ses_softc_t *, struct scsi_inquiry *, int, enctyp *);
static int ses_doattach(dev_info_t *dip);

static int  ses_open(dev_t *, int, int, cred_t *);
static int  ses_close(dev_t, int, int, cred_t *);
static int  ses_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static encvec vecs[3] = {
{
	ses_softc_init, ses_init_enc, ses_get_encstat,
	ses_set_encstat, ses_get_objstat, ses_set_objstat
},
{
	safte_softc_init, safte_init_enc, safte_get_encstat,
	safte_set_encstat, safte_get_objstat, safte_set_objstat,
},
{
	sen_softc_init, sen_init_enc, sen_get_encstat,
	sen_set_encstat, sen_get_objstat, sen_set_objstat
}
};


/*
 * Local Functions
 */
#ifdef	DEPRECATED_INTERFACES
static int sen_compat(ses_softc_t *, intptr_t, int, int);
#endif /* DEPRECATED_INTERFACES */

static int ses_start(struct buf *bp);
static int ses_decode_sense(struct scsi_pkt *pkt, int *err);

static void ses_get_pkt(struct buf *bp, int (*func)(opaque_t));
static void ses_callback(struct scsi_pkt *pkt);
static void ses_restart(void *arg);


/*
 * Local Static Data
 */
#ifndef	D_HOTPLUG
#define	D_HOTPLUG	0
#endif /* D_HOTPLUG */

static struct cb_ops ses_cb_ops = {
	ses_open,			/* open */
	ses_close,			/* close */
	nodev,				/* strategy */
	nodev,				/* print */
	nodev,				/* dump */
	nodev,				/* read */
	nodev,				/* write */
	ses_ioctl,			/* ioctl */
	nodev,				/* devmap */
	nodev,				/* mmap */
	nodev,				/* segmap */
	nochpoll,			/* poll */
	ddi_prop_op,			/* cb_prop_op */
	0,				/* streamtab  */
#if	!defined(CB_REV)
	D_MP | D_NEW | D_HOTPLUG	/* Driver compatibility flag */
#else	/* !defined(CB_REV) */
	D_MP | D_NEW | D_HOTPLUG,	/* Driver compatibility flag */
	CB_REV,				/* cb_ops version number */
	nodev,				/* aread */
	nodev				/* awrite */
#endif	/* !defined(CB_REV) */
};

static struct dev_ops ses_dev_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ses_info,		/* info */
	ses_identify,		/* identify */
	ses_probe,		/* probe */
	ses_attach,		/* attach */
	ses_detach,		/* detach */
	nodev,			/* reset */
	&ses_cb_ops,		/* driver operations */
	(struct bus_ops *)NULL,	/* bus operations */
	NULL			/* power */
};

static void *estate  = NULL;
static const char *Snm = "ses";
static const char *Str = "%s\n";
static const char *efl = "copyin/copyout EFAULT @ line %d";
static const char *fail_msg = "%stransport failed: reason '%s': %s";



/*
 * autoconfiguration routines.
 */
char _depends_on[] = "misc/scsi";

static struct modldrv modldrv = {
	&mod_driverops, "SCSI Enclosure Services Driver", &ses_dev_ops
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};


int
_init(void)
{
	int status;
	status = ddi_soft_state_init(&estate, sizeof (ses_softc_t), 0);
	if (status == 0) {
		if ((status = mod_install(&modlinkage)) != 0) {
			ddi_soft_state_fini(&estate);
		}
	}
	return (status);
}

int
_fini(void)
{
	int status;
	if ((status = mod_remove(&modlinkage)) != 0) {
		return (status);
	}
	ddi_soft_state_fini(&estate);
	return (status);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
ses_identify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "ses") == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}

static int
ses_probe(dev_info_t *dip)
{
	if (dip == NULL)
		return (DDI_PROBE_FAILURE);
	/*
	 * XXX: Breakage from the x86 folks.
	 */
	if (strcmp(ddi_get_name(ddi_get_parent(dip)), "ata") == 0) {
		return (DDI_PROBE_FAILURE);
	}
	/* SES_LOG(NULL, SES_CE_DEBUG1, "ses_probe: OK"); */
	return (DDI_PROBE_SUCCESS);
}

static int
ses_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int inst, err;
	ses_softc_t *ssc;

	inst = ddi_get_instance(dip);
	switch (cmd) {
	case DDI_ATTACH:
		SES_LOG(NULL, SES_CE_DEBUG9, "ses_attach: DDI_ATTACH ses%d",
		    inst);

		err = ses_doattach(dip);

		if (err == DDI_FAILURE) {
			return (DDI_FAILURE);
		}
		SES_LOG(NULL, SES_CE_DEBUG4,
		    "ses_attach: DDI_ATTACH OK ses%d", inst);
		break;

	case DDI_RESUME:
		if ((ssc = ddi_get_soft_state(estate, inst)) == NULL) {
			return (DDI_FAILURE);
		}
		SES_LOG(ssc, SES_CE_DEBUG1, "ses_attach: DDI_ATTACH ses%d",
		    inst);
		ssc->ses_suspended = 0;
		break;

	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

static int
is_enc_dev(ses_softc_t *ssc, struct scsi_inquiry *inqp, int iqlen, enctyp *ep)
{
	uchar_t dt = (inqp->inq_dtype & DTYPE_MASK);
	uchar_t *iqd = (uchar_t *)inqp;

	if (dt == DTYPE_ESI) {
		if (strncmp(inqp->inq_vid, SEN_ID, SEN_ID_LEN) == 0) {
			SES_LOG(ssc, SES_CE_DEBUG3, "SEN device found");
			*ep = SEN_TYPE;
		} else if (inqp->inq_rdf > RDF_SCSI2) {
			SES_LOG(ssc, SES_CE_DEBUG3, "SES device found");
			*ep = SES_TYPE;
		} else {
			SES_LOG(ssc, SES_CE_DEBUG3, "Pre-SCSI3 SES device");
			*ep = SES_TYPE;
		}
		return (1);
	}
	if ((iqd[6] & 0x40) && inqp->inq_rdf >= RDF_SCSI2) {
		/*
		 * PassThrough Device.
		 */
		*ep = SES_TYPE;
		SES_LOG(ssc, SES_CE_DEBUG3, "Passthru SES device");
		return (1);
	}

	if (iqlen < 47) {
		SES_LOG(ssc, CE_NOTE,
		    "INQUIRY data too short to determine SAF-TE");
		return (0);
	}
	if (strncmp((char *)&iqd[44], "SAF-TE", 4) == 0) {
		*ep = SAFT_TYPE;
		SES_LOG(ssc, SES_CE_DEBUG3, "SAF-TE device found");
		return (1);
	}
	return (0);
}


/*
 * Attach ses device.
 *
 * XXX:  Power management is NOT supported.  A token framework
 *       is provided that will need to be extended assuming we have
 *       ses devices we can power down.  Currently, we don't have any.
 */
static int
ses_doattach(dev_info_t *dip)
{
	int inst, err;
	Scsidevp devp;
	ses_softc_t *ssc;
	enctyp etyp;

	inst = ddi_get_instance(dip);
	/*
	 * Workaround for bug #4154979- for some reason we can
	 * be called with identical instance numbers but for
	 * different dev_info_t-s- all but one are bogus.
	 *
	 * Bad Dog! No Biscuit!
	 *
	 * A quick workaround might be to call ddi_soft_state_zalloc
	 * unconditionally, as the implementation fails these calls
	 * if there's an item already allocated. A more reasonable
	 * and longer term change is to move the allocation past
	 * the probe for the device's existence as most of these
	 * 'bogus' calls are for nonexistent devices.
	 */

	devp  = (Scsidevp) ddi_get_driver_private(dip);
	devp->sd_dev = dip;

	/*
	 * Determine whether the { i, t, l } we're called
	 * to start is an enclosure services device.
	 */

	/*
	 * Call the scsi_probe routine to see whether
	 * we actually have an Enclosure Services device at
	 * this address.
	 */

	switch (err = scsi_probe(devp, SLEEP_FUNC)) {
	case SCSIPROBE_EXISTS:
		if (is_enc_dev(NULL, devp->sd_inq, SUN_INQSIZE, &etyp))
			break;
		/* FALLTHROUGH */
	case SCSIPROBE_NORESP:
		scsi_unprobe(devp);
		return (DDI_FAILURE);
	/* case SCSIPROBE_BUSY: */
	/* case SCSIPROBE_NONCCS: */
	/* case SCSIPROBE_NOMEM: */
	/* case SCSIPROBE_FAILURE: */
	default:
		SES_LOG(NULL, SES_CE_DEBUG9,
		    "ses_doattach: probe error %d", err);
		scsi_unprobe(devp);
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(estate, inst) != DDI_SUCCESS) {
		scsi_unprobe(devp);
		SES_LOG(NULL, CE_NOTE, "ses%d: softalloc fails", inst);
		return (DDI_FAILURE);
	}
	ssc = ddi_get_soft_state(estate, inst);
	if (ssc == NULL) {
		scsi_unprobe(devp);
		SES_LOG(NULL, CE_NOTE, "ses%d: get_soft_state fails", inst);
		return (DDI_FAILURE);
	}
	devp->sd_private = (opaque_t)ssc;
	ssc->ses_devp = devp;
	err = ddi_create_minor_node(dip, "0", S_IFCHR, inst, DDI_PSEUDO, NULL);
	if (err == DDI_FAILURE) {
		ddi_remove_minor_node(dip, NULL);
		SES_LOG(ssc, CE_NOTE, "minor node creation failed");
		ddi_soft_state_free(estate, inst);
		scsi_unprobe(devp);
		return (DDI_FAILURE);
	}

	ssc->ses_type = etyp;
	ssc->ses_vec = vecs[etyp];

	/* Call SoftC Init Routine A bit later... */

	ssc->ses_rqbp = scsi_alloc_consistent_buf(SES_ROUTE(ssc),
	    NULL, SENSE_LENGTH, B_READ, SLEEP_FUNC, NULL);
	if (ssc->ses_rqbp != NULL) {
		ssc->ses_rqpkt = scsi_init_pkt(SES_ROUTE(ssc), NULL,
		    ssc->ses_rqbp, CDB_GROUP0, 1, 0, PKT_CONSISTENT,
		    SLEEP_FUNC, NULL);
	}
	if (ssc->ses_rqbp == NULL || ssc->ses_rqpkt == NULL) {
		ddi_remove_minor_node(dip, NULL);
		SES_LOG(ssc, CE_NOTE, "scsi_init_pkt of rqbuf failed");
		if (ssc->ses_rqbp != NULL) {
			scsi_free_consistent_buf(ssc->ses_rqbp);
			ssc->ses_rqbp = NULL;
		}
		ddi_soft_state_free(estate, inst);
		scsi_unprobe(devp);
		return (DDI_FAILURE);
	}
	ssc->ses_rqpkt->pkt_private = (opaque_t)ssc;
	ssc->ses_rqpkt->pkt_address = *(SES_ROUTE(ssc));
	ssc->ses_rqpkt->pkt_comp = ses_callback;
	ssc->ses_rqpkt->pkt_time = ses_io_time;
	ssc->ses_rqpkt->pkt_flags = FLAG_NOPARITY|FLAG_NODISCON|FLAG_SENSING;
	ssc->ses_rqpkt->pkt_cdbp[0] = SCMD_REQUEST_SENSE;
	ssc->ses_rqpkt->pkt_cdbp[1] = 0;
	ssc->ses_rqpkt->pkt_cdbp[2] = 0;
	ssc->ses_rqpkt->pkt_cdbp[3] = 0;
	ssc->ses_rqpkt->pkt_cdbp[4] = SENSE_LENGTH;
	ssc->ses_rqpkt->pkt_cdbp[5] = 0;

	switch (scsi_ifgetcap(SES_ROUTE(ssc), "auto-rqsense", 1)) {
	case 1:
		/* if already set, don't reset it */
		ssc->ses_arq = 1;
		break;
	case 0:
		/* try and set it */
		ssc->ses_arq = ((scsi_ifsetcap(SES_ROUTE(ssc),
		    "auto-rqsense", 1, 1) == 1) ? 1 : 0);
		break;
	default:
		/* probably undefined, so zero it out */
		ssc->ses_arq = 0;
		break;
	}

	ssc->ses_sbufp = getrbuf(KM_SLEEP);
	cv_init(&ssc->ses_sbufcv, NULL, CV_DRIVER, NULL);

	/*
	 * If the HBA supports wide, tell it to use wide.
	 */
	if (scsi_ifgetcap(SES_ROUTE(ssc), "wide-xfer", 1) != -1) {
		int wd = ((devp->sd_inq->inq_rdf == RDF_SCSI2) &&
		    (devp->sd_inq->inq_wbus16 || devp->sd_inq->inq_wbus32))
		    ? 1 : 0;
		(void) scsi_ifsetcap(SES_ROUTE(ssc), "wide-xfer", wd, 1);
	}

	/*
	 * Now do ssc init of enclosure specifics.
	 * At the same time, check to make sure getrbuf
	 * actually succeeded.
	 */
	if (ssc->ses_sbufp == NULL || (*ssc->ses_vec.softc_init)(ssc, 1)) {
		ddi_remove_minor_node(dip, NULL);
		scsi_destroy_pkt(ssc->ses_rqpkt);
		scsi_free_consistent_buf(ssc->ses_rqbp);
		if (ssc->ses_sbufp) {
			SES_LOG(ssc, SES_CE_DEBUG3, "failed to getrbuf");
			freerbuf(ssc->ses_sbufp);
		} else {
			SES_LOG(ssc, SES_CE_DEBUG3, "failed softc init");
		}
		cv_destroy(&ssc->ses_sbufcv);
		ddi_soft_state_free(estate, inst);
		scsi_unprobe(devp);
		return (DDI_FAILURE);
	}

	/*
	 * create this property so that PM code knows we want
	 * to be suspended at PM time
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, dip,
	    DDI_PROP_CANSLEEP, PM_HARDWARE_STATE_PROP,
	    (caddr_t)PM_NEEDS_SUSPEND_RESUME,
	    strlen(PM_NEEDS_SUSPEND_RESUME) + 1);

	/* announce the existence of this device */
	ddi_report_dev(dip);
	return (DDI_SUCCESS);
}


/*
 * Detach ses device.
 *
 * XXX:  Power management is NOT supported.  A token framework
 *       is provided that will need to be extended assuming we have
 *       ses devices we can power down.  Currently, we don't have any.
 */
static int
ses_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	register ses_softc_t *ssc;
	int inst;

	switch (cmd) {
	case DDI_DETACH:
		inst = ddi_get_instance(dip);
		ssc = ddi_get_soft_state(estate, inst);
		if (ssc == NULL) {
			cmn_err(CE_NOTE,
			    "ses%d: DDI_DETACH, no softstate found", inst);
			return (DDI_FAILURE);
		}
		if (ISOPEN(ssc)) {
			return (DDI_FAILURE);
		}

#if		!defined(lint)
		/* LINTED */
		_NOTE(COMPETING_THREADS_NOW);
#endif		/* !defined(lint) */

		if (ssc->ses_vec.softc_init)
			(void) (*ssc->ses_vec.softc_init)(ssc, 0);

#if		!defined(lint)
		_NOTE(NO_COMPETING_THREADS_NOW);
#endif 		/* !defined(lint) */

		(void) scsi_ifsetcap(SES_ROUTE(ssc), "auto-rqsense", 1, 0);
		scsi_destroy_pkt(ssc->ses_rqpkt);
		scsi_free_consistent_buf(ssc->ses_rqbp);
		freerbuf(ssc->ses_sbufp);
		cv_destroy(&ssc->ses_sbufcv);
		ddi_soft_state_free(estate, inst);
		ddi_prop_remove_all(dip);
		ddi_remove_minor_node(dip, NULL);
		scsi_unprobe((Scsidevp) ddi_get_driver_private(dip));
		break;

	case DDI_SUSPEND:
		inst = ddi_get_instance(dip);
		if ((ssc = ddi_get_soft_state(estate, inst)) == NULL) {
			cmn_err(CE_NOTE,
			    "ses%d: DDI_SUSPEND, no softstate found", inst);
			return (DDI_FAILURE);
		}

		/*
		 * If driver idle, accept suspend request.
		 * If it's busy, reject it.  This keeps things simple!
		 */
		mutex_enter(SES_MUTEX);
		if (ssc->ses_sbufbsy) {
			mutex_exit(SES_MUTEX);
			return (DDI_FAILURE);
		}
		ssc->ses_suspended = 1;
		mutex_exit(SES_MUTEX);
		break;

	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
ses_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register dev_t dev;
	register ses_softc_t *ssc;
	register int inst, error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dev = (dev_t)arg;
		inst = getminor(dev);
		if ((ssc = ddi_get_soft_state(estate, inst)) == NULL) {
			return (DDI_FAILURE);
		}
		*result = (void *) ssc->ses_devp->sd_dev;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		dev = (dev_t)arg;
		inst = getminor(dev);
		*result = (void *)inst;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * Unix Entry Points
 */

/* ARGSUSED */
static int
ses_open(dev_t *dev_p, int flag, int otyp, cred_t *cred_p)
{
	ses_softc_t *ssc;

	if ((ssc = ddi_get_soft_state(estate, getminor(*dev_p))) == NULL) {
		return (ENXIO);
	}

	/*
	 * If the device is powered down, request it's activation.
	 * If it can't be activated, fail open.
	 */
	if (ssc->ses_suspended &&
	    ddi_dev_is_needed(SES_DEVINFO(ssc), 0, 1) != DDI_SUCCESS) {
		return (EIO);
	}

	mutex_enter(SES_MUTEX);
	if (otyp == OTYP_LYR)
		ssc->ses_lyropen++;
	else
		ssc->ses_oflag = 1;

	ssc->ses_present = (ssc->ses_present)? ssc->ses_present: SES_OPENING;
	mutex_exit(SES_MUTEX);
	return (EOK);
}

/*ARGSUSED*/
static int
ses_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	ses_softc_t *ssc;
	if ((ssc = ddi_get_soft_state(estate, getminor(dev))) == NULL) {
		return (ENXIO);
	}

	if (ssc->ses_suspended) {
		(void) ddi_dev_is_needed(SES_DEVINFO(ssc), 0, 1);
	}

	mutex_enter(SES_MUTEX);
	if (otyp == OTYP_LYR)
		ssc->ses_lyropen -= (ssc->ses_lyropen)? 1: 0;
	else
		ssc->ses_oflag = 0;
	mutex_exit(SES_MUTEX);
	return (0);
}


/*ARGSUSED3*/
static int
ses_ioctl(dev_t dev, int cmd, intptr_t arg, int flg, cred_t *cred_p, int *rvalp)
{
	ses_softc_t *ssc;
	ses_object k, *up;
	ses_objarg x;
	uchar_t t;
	uchar_t i;
	int rv = 0;

	if ((ssc = ddi_get_soft_state(estate, getminor(dev))) == NULL ||
	    ssc->ses_present == SES_CLOSED) {
		return (ENXIO);
	}


	switch (cmd) {
#ifdef	DEPRECATED_INTERFACES
	case SES_IOCTL_GETSTATE:
		rv = sen_compat(ssc, arg, flg, SCMD_GDIAG);
		break;

	case SES_IOCTL_SETSTATE:
		rv = sen_compat(ssc, arg, flg, SCMD_SDIAG);
		break;

	case SES_IOCTL_INQUIRY:
		rv = sen_compat(ssc, arg, flg, SCMD_INQUIRY);
		break;
#endif	/* DEPRECATED_INTERFACES */

	case SESIOC_GETNOBJ:

		if (ddi_copyout((caddr_t)&ssc->ses_nobjects,
		    (caddr_t)arg, sizeof (int), flg)) {
			rv = EFAULT;
			break;
		}
		break;

	case SESIOC_GETOBJMAP:
		up = (ses_object *) arg;
		mutex_enter(SES_MUTEX);
		for (i = 0; i != ssc->ses_nobjects; i++) {
			k.obj_id = i;
			k.subencid = ssc->ses_objmap[i].subenclosure;
			k.elem_type = ssc->ses_objmap[i].enctype;
			if (ddi_copyout((caddr_t)&k, (caddr_t)up,
			    sizeof (k), flg)) {
				rv = EFAULT;
				break;
			}
			up++;
		}
		mutex_exit(SES_MUTEX);
		break;

	case SESIOC_INIT:
		rv = (*ssc->ses_vec.init_enc)(ssc);
		break;

	case SESIOC_GETENCSTAT:
		if ((ssc->ses_encstat & ENCI_SVALID) == 0) {
			rv = (*ssc->ses_vec.get_encstat)(ssc, KM_SLEEP);
			if (rv) {
				break;
			}
		}
		t = ssc->ses_encstat & 0xf;
		if (ddi_copyout((caddr_t)&t, (caddr_t)arg, sizeof (t), flg))
			rv = EFAULT;
		/*
		 * And always invalidate enclosure status on the way out.
		 */
		mutex_enter(SES_MUTEX);
		ssc->ses_encstat &= ~ENCI_SVALID;
		mutex_exit(SES_MUTEX);
		break;

	case SESIOC_SETENCSTAT:
		if (ddi_copyin((caddr_t)arg, (caddr_t)&t, sizeof (t), flg))
			rv = EFAULT;
		else
			rv = (*ssc->ses_vec.set_encstat)(ssc, t, KM_SLEEP);
		mutex_enter(SES_MUTEX);
		ssc->ses_encstat &= ~ENCI_SVALID;
		mutex_exit(SES_MUTEX);
		break;

	case SESIOC_GETOBJSTAT:
		if (ddi_copyin((caddr_t)arg, (caddr_t)&x, sizeof (x), flg)) {
			rv = EFAULT;
			break;
		}
		if (x.obj_id >= ssc->ses_nobjects) {
			rv = EINVAL;
			break;
		}
		if ((rv = (*ssc->ses_vec.get_objstat)(ssc, &x, KM_SLEEP)) != 0)
			break;
		if (ddi_copyout((caddr_t)&x, (caddr_t)arg, sizeof (x), flg))
			rv = EFAULT;
		else {
			/*
			 * Now that we no longer poll, svalid never stays true.
			 */
			mutex_enter(SES_MUTEX);
			ssc->ses_objmap[x.obj_id].svalid = 0;
			mutex_exit(SES_MUTEX);
		}
		break;

	case SESIOC_SETOBJSTAT:
		if (ddi_copyin((caddr_t)arg, (caddr_t)&x, sizeof (x), flg)) {
			rv = EFAULT;
			break;
		}
		if (x.obj_id >= ssc->ses_nobjects) {
			rv = EINVAL;
			break;
		}
		rv = (*ssc->ses_vec.set_objstat)(ssc, &x, KM_SLEEP);
		if (rv == 0) {
			mutex_enter(SES_MUTEX);
			ssc->ses_objmap[x.obj_id].svalid = 0;
			mutex_exit(SES_MUTEX);
		}
		break;

	case USCSICMD:
		rv = ses_uscsi_cmd(ssc, (Uscmd *) arg, flg, flg, flg, flg);
		break;

	default:
		rv = ENOTTY;
		break;
	}
	return (rv);
}

#ifdef	DEPRECATED_INTERFACES
/*
 * Send 'sen' compatability commands.
 */
static int
sen_compat(ses_softc_t *ssc, intptr_t a, int flg, int cmd)
{
	struct ses_ioctl sep;
	uchar_t cdb[CDB_GROUP0], rqbuf[SENSE_LENGTH];
	Uscmd local, *lp = &local;
#ifdef	_MULTI_DATAMODEL
	/*
	 * 32 bit application to 64 bit kernel call.
	 */
	struct {
		uint32_t page_size;
		uint8_t page_code;
		caddr32_t page;
	} s32;

	switch (ddi_model_convert_from(flg & FMODELS)) {
	case DDI_MODEL_ILP32:
		if (ddi_copyin((caddr_t)a, (caddr_t)&s32, sizeof (s32), flg)) {
			SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
			return (EFAULT);
		}
		sep.page_size = s32.page_size;
		sep.page_code = s32.page_code;
		sep.page = (caddr_t)s32.page;
		break;
	case DDI_MODEL_NONE:
		if (ddi_copyin((caddr_t)a, (caddr_t)&sep, sizeof (sep), flg)) {
			SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
			return (EFAULT);
		}
		break;
	default:
		SES_LOG(ssc, CE_NOTE, "Unknown model conversion flag %x",
		    flg & FMODELS);
		return (EINVAL);
	}
#else	/* _MULTI_DATAMODEL */
	if (ddi_copyin((caddr_t)a, (caddr_t)&sep, sizeof (sep), flg)) {
		return (EFAULT);
	}
#endif	/* _MULTI_DATAMODEL */

	bzero((caddr_t)lp, sizeof (*lp));
	switch (cmd) {
	case SCMD_GDIAG:
	case SCMD_SDIAG:
		/* These ioctls are only for the SEN Card */
		if (ssc->ses_type != SEN_TYPE) {
			return (ENOTTY);
		}
		if (cmd == SCMD_SDIAG) {
			lp->uscsi_flags = USCSI_WRITE;
			/*
			 * The page code should be 0 in the cdb, no
			 * matter what the page being sent to the
			 * device. As per SES specification.
			 */
			sep.page_code = 0;
		} else
			lp->uscsi_flags = USCSI_READ;
		cdb[0] = (uchar_t)cmd;
		cdb[1] = SCSI_ESI_PF;
		cdb[2] = sep.page_code & 0xFF;
		cdb[3] = (sep.page_size >> 8) & 0xff;
		cdb[4] = (sep.page_size) & 0xFF;
		cdb[5] = 0;
		break;
	case SCMD_INQUIRY:
		cdb[0] = (char)cmd;
		if (sep.page_code)
			cdb[1] = 0x01; /* EVPD Bit */
		else
			cdb[1] = 0;
		cdb[2] = sep.page_code & 0xFF;
		cdb[3] = 0;
		cdb[4] = sep.page_size & 0xFF;
		cdb[5] = 0;
		lp->uscsi_flags = USCSI_READ;
		break;
	default:
		return (EINVAL);
	}
	lp->uscsi_timeout = ses_io_time;
	lp->uscsi_cdb = (char *)cdb;
	lp->uscsi_bufaddr = sep.page;
	lp->uscsi_buflen = sep.page_size & 0xff;
	lp->uscsi_cdblen = sizeof (cdb);
	lp->uscsi_rqbuf = (char *)rqbuf;
	lp->uscsi_rqlen = sizeof (rqbuf);
	lp->uscsi_flags |= USCSI_RQENABLE;
	return (ses_uscsi_cmd(ssc, lp, FKIOCTL, FKIOCTL, 0, FKIOCTL));
}
#endif	/* DEPRECATED_INTERFACES */


/*
 * Loop on running a kernel based command
 *
 * FIXME:  This routine is not really needed.
 */
int
ses_runcmd(ses_softc_t *ssc, Uscmd *lp)
{
	int e;

	lp->uscsi_status = 0;
	e = ses_uscsi_cmd(ssc, lp, FKIOCTL, FKIOCTL, FKIOCTL, FKIOCTL);

#ifdef	not
	/*
	 * Debug:  Nice cross-check code for verifying consistant status.
	 */
	if (lp->uscsi_status) {
		if (lp->uscsi_status == STATUS_CHECK) {
			SES_LOG(ssc, CE_NOTE, "runcmd<cdb[0]="
			    "0x%x->%s ASC/ASCQ=0x%x/0x%x>",
			    lp->uscsi_cdb[0],
			    scsi_sname(lp->uscsi_rqbuf[2] & 0xf),
			    lp->uscsi_rqbuf[12] & 0xff,
			    lp->uscsi_rqbuf[13] & 0xff);
		} else {
			SES_LOG(ssc, CE_NOTE, "runcmd<cdb[0]="
			    "0x%x -> Status 0x%x", lp->uscsi_cdb[0],
			    lp->uscsi_status);
		}
	}
#endif	/* not */
	return (e);
}


/*
 * Run a scsi command.
 */
int
ses_uscsi_cmd(ses_softc_t *ssc, Uscmd *Uc, int Uf, int Cf, int Df, int Rf)
{
	Uscmd local, *lc = &local, *scmd = &ssc->ses_uscsicmd;
	int err = EOK;
	int rw, rqlen;
	struct buf *bp;

#ifdef	_MULTI_DATAMODEL
	/*
	 * 32 bit application to 64 bit kernel call.
	 */
	struct uscsi_cmd32 ucmd_32, *uc = &ucmd_32;

	switch (ddi_model_convert_from(Uf & FMODELS)) {
	case DDI_MODEL_ILP32:
		if (ddi_copyin((caddr_t)Uc, (caddr_t)uc, sizeof (*uc), Uf)) {
			SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
			return (EFAULT);
		}
		uscsi_cmd32touscsi_cmd(uc, lc);
		break;
	case DDI_MODEL_NONE:
		uc = NULL;
		if (ddi_copyin((caddr_t)Uc, (caddr_t)lc, sizeof (*lc), Uf)) {
			SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
			return (EFAULT);
		}
		break;
	default:
		SES_LOG(ssc, CE_NOTE, "Unknown model conversion flag %x",
		    Uf & FMODELS);
		return (EINVAL);
	}
#else	/* _MULTI_DATAMODEL */
	if (ddi_copyin((caddr_t)Uc, (caddr_t)lc, sizeof (*lc), Uf)) {
		SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
		return (EFAULT);
	}
#endif	/* _MULTI_DATAMODEL */

	/*
	 * Is this a request to reset the bus?
	 * If so, we need go no further.
	 */
	if (lc->uscsi_flags & (USCSI_RESET|USCSI_RESET_ALL)) {
		int flag = ((lc->uscsi_flags & USCSI_RESET_ALL)) ?
			RESET_ALL : RESET_TARGET;
		err = (scsi_reset(SES_ROUTE(ssc), flag))? 0 : EIO;
		SES_LOG(ssc, SES_CE_DEBUG1, "reset %s %s\n",
			(flag == RESET_ALL)? "all" : "target",
			(err)? "failed" : "ok");
		return (err);
	}

	/*
	 * First do some sanity checks.
	 */
	if (lc->uscsi_cdblen == 0) {
		SES_LOG(ssc, SES_CE_DEBUG1, "cblen %d", lc->uscsi_cdblen);
		return (EINVAL);
	}
	if (lc->uscsi_flags & USCSI_RQENABLE &&
	    (lc->uscsi_rqlen == 0 || lc->uscsi_rqbuf == NULL)) {
		return (EINVAL);
	}


	/*
	 * Grab local 'special' buffer
	 */
	mutex_enter(SES_MUTEX);
	while (ssc->ses_sbufbsy) {
		cv_wait(&ssc->ses_sbufcv, &ssc->ses_devp->sd_mutex);
	}
	ssc->ses_sbufbsy = 1;
	mutex_exit(SES_MUTEX);

	/*
	 * If the device is powered down, request it's activation.
	 * This check must be done after setting ses_sbufbsy!
	 */
	if (ssc->ses_suspended &&
	    ddi_dev_is_needed(SES_DEVINFO(ssc), 0, 1) != DDI_SUCCESS) {
		mutex_enter(SES_MUTEX);
		ssc->ses_sbufbsy = 0;
		mutex_exit(SES_MUTEX);
		return (EIO);
	}


	bcopy((caddr_t)lc, (caddr_t)scmd, sizeof (Uscmd));

	if (ddi_copyin(lc->uscsi_cdb, (caddr_t)&ssc->ses_srqcdb,
	    (size_t)scmd->uscsi_cdblen, Cf)) {
		mutex_enter(SES_MUTEX);
		ssc->ses_sbufbsy = 0;
		cv_signal(&ssc->ses_sbufcv);
		mutex_exit(SES_MUTEX);
		SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
		return (EFAULT);
	}
	scmd->uscsi_cdb = (char *)ssc->ses_srqcdb;


	if (scmd->uscsi_flags & USCSI_RQENABLE) {
		scmd->uscsi_rqlen = min(SENSE_LENGTH, scmd->uscsi_rqlen);
		scmd->uscsi_rqresid = scmd->uscsi_rqlen;
		scmd->uscsi_rqbuf = (char *)&ssc->ses_srqsbuf;
	} else {
		scmd->uscsi_flags &= ~USCSI_RQENABLE;
		scmd->uscsi_rqlen = 0;
		scmd->uscsi_rqresid = 0;
		scmd->uscsi_rqbuf = (char *)NULL;
	}

	/*
	 * Drive on..
	 */
	rw = (scmd->uscsi_flags & USCSI_READ) ? B_READ : B_WRITE;

	/*
	 * We're going to do actual I/O,
	 * let physio do all the right things.
	 */
	bp = ssc->ses_sbufp;
	bp->av_back = (struct buf *)NULL;
	bp->av_forw = (struct buf *)NULL;
	bp->b_back = (struct buf *)ssc;
	bp->b_edev = NODEV;

	if (scmd->uscsi_cdblen == CDB_GROUP0) {
		SES_LOG(ssc, SES_CE_DEBUG7,
		    "scsi_cmd: %x %x %x %x %x %x",
		    ssc->ses_srqcdb[0], ssc->ses_srqcdb[1],
		    ssc->ses_srqcdb[2], ssc->ses_srqcdb[3],
		    ssc->ses_srqcdb[4], ssc->ses_srqcdb[5]);
	} else {
		SES_LOG(ssc, SES_CE_DEBUG7,
		    "scsi cmd: %x %x %x %x %x %x %x %x %x %x",
		    ssc->ses_srqcdb[0], ssc->ses_srqcdb[1],
		    ssc->ses_srqcdb[2], ssc->ses_srqcdb[3],
		    ssc->ses_srqcdb[4], ssc->ses_srqcdb[5],
		    ssc->ses_srqcdb[6], ssc->ses_srqcdb[7],
		    ssc->ses_srqcdb[8], ssc->ses_srqcdb[9]);
	}

	if (scmd->uscsi_buflen) {
		struct iovec aiov;
		struct uio auio;
		struct uio *uio = &auio;

		bzero((caddr_t)&auio, sizeof (struct uio));
		bzero((caddr_t)&aiov, sizeof (struct iovec));

		aiov.iov_base = scmd->uscsi_bufaddr;
		aiov.iov_len = scmd->uscsi_buflen;
		uio->uio_iov = &aiov;
		uio->uio_iovcnt = 1;
		uio->uio_resid = aiov.iov_len;
		uio->uio_segflg =
			(Df & FKIOCTL)? UIO_SYSSPACE : UIO_USERSPACE;
		uio->uio_loffset = 0;
		uio->uio_fmode = 0;

		/*
		 * Call physio and let that do the rest.
		 * Note: we wait there until the command is complete.
		 */
		err = physio(ses_start, bp, NODEV, rw, minphys, uio);
		scmd->uscsi_resid = bp->b_resid;
	} else {
		/*
		 * Since we do not move any data in this section
		 * call ses_start directly. Mimic physio.
		 */
		bp->b_flags = B_BUSY | rw;
		bp->b_bcount = 0;
		(void) ses_start(bp);
		scmd->uscsi_resid = 0;
		err = biowait(bp);
	}


	/*
	 * Copy status and sense data to where it needs to be..
	 * If the sender of the scsi command is in user space,
	 * mask off the high bits.
	 */
	lc->uscsi_status = scmd->uscsi_status;
	if (lc->uscsi_status)
		SES_LOG(ssc, SES_CE_DEBUG5, "status %x", lc->uscsi_status);
	lc->uscsi_resid = scmd->uscsi_resid;

	rqlen = scmd->uscsi_rqlen - scmd->uscsi_rqresid;
	lc->uscsi_rqresid = scmd->uscsi_rqlen - rqlen;

	if (lc->uscsi_rqbuf && rqlen) {
		SES_LOG(ssc, SES_CE_DEBUG5, "Sense Key %x",
		    scmd->uscsi_rqbuf[2] & 0xf);
		if (ddi_copyout((caddr_t)scmd->uscsi_rqbuf,
		    (caddr_t)lc->uscsi_rqbuf, rqlen, Rf)) {
			SES_LOG(ssc, SES_CE_DEBUG2,
			    "ses_uscsi_cmd: rqbuf copy-out problem");
			SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
			err = EFAULT;
		}
	}

	/*
	 * Free up allocated resources.
	 */
	mutex_enter(SES_MUTEX);
	ssc->ses_sbufbsy = 0;
	cv_signal(&ssc->ses_sbufcv);
	mutex_exit(SES_MUTEX);

	/*
	 * Copy out changed values
	 */
#ifdef	_MULTI_DATAMODEL
	if (uc != NULL) {
		uscsi_cmdtouscsi_cmd32(lc, uc);
		if (ddi_copyout((caddr_t)uc, (caddr_t)Uc, sizeof (*uc), Uf)) {
			SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
			return (EFAULT);
		}
	} else
#endif	/* _MULTI_DATAMODEL */

	if (ddi_copyout((caddr_t)lc, (caddr_t)Uc, sizeof (*lc), Uf)) {
		SES_LOG(ssc, SES_CE_DEBUG2, efl, __LINE__);
		if (err == 0)
			err = EFAULT;
	}
	if (err)
		SES_LOG(ssc, SES_CE_DEBUG5, "ses_uscsi_cmd returning %d", err);
	return (err);
}



/*
 * Command start and done functions.
 */
static int
ses_start(struct buf *bp)
{
	ses_softc_t *ssc = (ses_softc_t *)bp->b_back;

	SES_LOG(ssc, SES_CE_DEBUG9, "ses_start");
	if (!BP_PKT(bp)) {
		/*
		 * Allocate a packet.
		 */
		ses_get_pkt(bp, SLEEP_FUNC);
		if (!BP_PKT(bp)) {
			int err;
			bp->b_resid = bp->b_bcount;
			if (geterror(bp) == 0)
				SET_BP_ERROR(bp, EIO);
			err = geterror(bp);
			biodone(bp);
			return (err);
		}
	}

	/*
	 * Initialize the transfer residue, error code, and retry count.
	 */
	bp->b_resid = 0;
	SET_BP_ERROR(bp, 0);

#if	!defined(lint)
	_NOTE(NO_COMPETING_THREADS_NOW);
#endif 	/* !defined(lint) */
	ssc->ses_retries = ses_retry_count;

#if	!defined(lint)
	/* LINTED */
	_NOTE(COMPETING_THREADS_NOW);
#endif	/* !defined(lint) */

	SES_LOG(ssc, SES_CE_DEBUG9, "ses_start -> scsi_transport");
	switch (scsi_transport(BP_PKT(bp))) {
	case TRAN_ACCEPT:
		return (0);
		/* break; */

	case TRAN_BUSY:
		SES_LOG(ssc, SES_CE_DEBUG2,
			"ses_start: TRANSPORT BUSY");
		SES_ENABLE_RESTART(SES_RESTART_TIME, BP_PKT(bp));
		return (0);
		/* break; */

	default:
		SES_LOG(ssc, SES_CE_DEBUG2, "TRANSPORT ERROR\n");
		SET_BP_ERROR(bp, EIO);
		scsi_destroy_pkt(BP_PKT(bp));
		biodone(bp);
		return (EIO);
		/* break; */
	}
}


static void
ses_get_pkt(struct buf *bp, int (*func)())
{
	ses_softc_t *ssc = (ses_softc_t *)bp->b_back;
	Uscmd *scmd = &ssc->ses_uscsicmd;
	struct scsi_pkt *pkt;
	int stat_size;

	if ((scmd->uscsi_flags & USCSI_RQENABLE) && ssc->ses_arq) {
		stat_size = sizeof (struct scsi_arq_status);
	} else {
		stat_size = 1;
	}

	if (bp->b_bcount) {
		pkt = scsi_init_pkt(SES_ROUTE(ssc), NULL, bp,
		    scmd->uscsi_cdblen, stat_size, 0, 0, func, (caddr_t)ssc);
	} else {
		pkt = scsi_init_pkt(SES_ROUTE(ssc), NULL, NULL,
		    scmd->uscsi_cdblen, stat_size, 0, 0, func, (caddr_t)ssc);
	}
	if (pkt == (struct scsi_pkt *)NULL)
		return;
	SET_BP_PKT(bp, pkt);
	bcopy((caddr_t)scmd->uscsi_cdb, (caddr_t)pkt->pkt_cdbp,
	    (int)scmd->uscsi_cdblen);
	pkt->pkt_time = scmd->uscsi_timeout;

	pkt->pkt_comp = ses_callback;
	pkt->pkt_private = (opaque_t)ssc;
}


/*
 * Restart ses command.
 */
static void
ses_restart(void *arg)
{
	struct scsi_pkt *pkt = (struct scsi_pkt *)arg;
	ses_softc_t *ssc = (ses_softc_t *)pkt->pkt_private;
	struct buf *bp = ssc->ses_sbufp;
	SES_LOG(ssc, SES_CE_DEBUG9, "ses_restart");

	ssc->ses_restart_id = NULL;

	switch (scsi_transport(pkt)) {
	case TRAN_ACCEPT:
		SES_LOG(ssc, SES_CE_DEBUG9,
		    "RESTART %d ok", ssc->ses_retries);
		return;
		/* break; */
	case TRAN_BUSY:
		SES_LOG(ssc, SES_CE_DEBUG1,
		    "RESTART %d TRANSPORT BUSY\n", ssc->ses_retries);
		if (ssc->ses_retries > SES_NO_RETRY) {
		    ssc->ses_retries -= SES_BUSY_RETRY;
		    SES_ENABLE_RESTART(SES_RESTART_TIME, pkt);
		    return;
		}
		SET_BP_ERROR(bp, EBUSY);
		break;
	default:
		SET_BP_ERROR(bp, EIO);
		break;
	}
	SES_LOG(ssc, SES_CE_DEBUG1,
	    "RESTART %d TRANSPORT FAILED\n", ssc->ses_retries);

	pkt = (struct scsi_pkt *)bp->av_back;
	scsi_destroy_pkt(pkt);
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}


/*
 * Command completion processing
 */
#define	HBA_RESET	(STAT_BUS_RESET|STAT_DEV_RESET|STAT_ABORTED)
static void
ses_callback(struct scsi_pkt *pkt)
{
	ses_softc_t *ssc = (ses_softc_t *)pkt->pkt_private;
	struct buf *bp;
	Uscmd *scmd;
	int err;
	char action;

	bp = ssc->ses_sbufp;
	scmd = &ssc->ses_uscsicmd;
	/* SES_LOG(ssc, SES_CE_DEBUG9, "ses_callback"); */

	/*
	 * Optimization: Normal completion.
	 */
	if (pkt->pkt_reason == CMD_CMPLT &&
	    !SCBP_C(pkt) &&
	    !(pkt->pkt_flags & FLAG_SENSING) &&
	    !pkt->pkt_resid) {
	    scsi_destroy_pkt(pkt);
	    biodone(bp);
	    return;
	}


	/*
	 * Abnormal completion.
	 *
	 * Assume most common error initially.
	 */
	err = EIO;
	action = COMMAND_DONE;
	if (scmd->uscsi_flags & USCSI_DIAGNOSE) {
		ssc->ses_retries = SES_NO_RETRY;
	}

CHECK_PKT:
	if (pkt->pkt_reason != CMD_CMPLT) {
		/* Process transport errors. */
		switch (pkt->pkt_reason) {
		case CMD_TIMEOUT:
			/*
			 * If the transport layer didn't clear the problem,
			 * reset the target.
			 */
			if (! (pkt->pkt_statistics & HBA_RESET)) {
			    (void) scsi_reset(&pkt->pkt_address, RESET_TARGET);
			}
			err = ETIMEDOUT;
			break;

		case CMD_INCOMPLETE:
		case CMD_UNX_BUS_FREE:
			/*
			 * No response?  If probing, give up.
			 * Otherwise, keep trying until retries exhausted.
			 * Then lockdown the driver as the device is
			 * unplugged.
			 */
			if (ssc->ses_retries <= SES_NO_RETRY &&
			    !(scmd->uscsi_flags & USCSI_DIAGNOSE)) {
				ssc->ses_present = SES_CLOSED;
			}
			/* Inhibit retries to speed probe/attach. */
			if (ssc->ses_present < SES_OPEN) {
				ssc->ses_retries = SES_NO_RETRY;
			}
			/* SES_CMD_RETRY4(ssc->ses_retries); */
			err = ENXIO;
			break;

		case CMD_DATA_OVR:
			/*
			 * XXX:	Some HBA's (e.g. Adaptec 1740 and
			 *	earlier ISP revs) report a DATA OVERRUN
			 *	error instead of a transfer residue.  So,
			 *	we convert the error and restart.
			 */
			if ((bp->b_bcount - pkt->pkt_resid) > 0) {
				SES_LOG(ssc, SES_CE_DEBUG6,
					"ignoring overrun");
				pkt->pkt_reason = CMD_CMPLT;
				err = EOK;
				goto CHECK_PKT;
			}
			ssc->ses_retries = SES_NO_RETRY;
			/* err = EIO; */
			break;

		case CMD_DMA_DERR:
			ssc->ses_retries = SES_NO_RETRY;
			err = EFAULT;
			break;

		default:
			/* err = EIO; */
			break;
		}
		if (pkt == ssc->ses_rqpkt) {
			SES_LOG(ssc, CE_WARN, fail_msg,
				"Request Sense ",
				scsi_rname(pkt->pkt_reason),
				(ssc->ses_retries > 0)?
					"retrying": "giving up");
			pkt = (struct scsi_pkt *)bp->av_back;
			action = QUE_SENSE;
		} else {
			SES_LOG(ssc, CE_WARN, fail_msg,
				"", scsi_rname(pkt->pkt_reason),
				(ssc->ses_retries > 0)?
					"retrying": "giving up");
			action = QUE_COMMAND;
		}
		/* Device exists, allow full error recovery. */
		if (ssc->ses_retries > SES_NO_RETRY) {
			ssc->ses_present = SES_OPEN;
		}


	/*
	 * Process status and sense data errors.
	 */
	} else {
		ssc->ses_present = SES_OPEN;
		action = ses_decode_sense(pkt, &err);
	}


	/*
	 * Initiate error recovery action, as needed.
	 */
	switch (action) {
	case QUE_COMMAND_NOW:
		/* SES_LOG(ssc, SES_CE_DEBUG1, "retrying cmd now"); */
		if (ssc->ses_retries > SES_NO_RETRY) {
		    ssc->ses_retries -= SES_CMD_RETRY;
		    scmd->uscsi_status = 0;
		    if (ssc->ses_arq) {
			bzero((caddr_t)pkt->pkt_scbp,
			    sizeof (struct scsi_arq_status));
		    }
		    if (scsi_transport((struct scsi_pkt *)bp->av_back)
			!= TRAN_ACCEPT) {
			SES_ENABLE_RESTART(SES_RESTART_TIME,
				(struct scsi_pkt *)bp->av_back);
		    }
		    return;
		}
		break;

	case QUE_COMMAND:
		SES_LOG(ssc, SES_CE_DEBUG1, "retrying cmd");
		if (ssc->ses_retries > SES_NO_RETRY) {
		    ssc->ses_retries -=
			(err == EBUSY)? SES_BUSY_RETRY: SES_CMD_RETRY;
		    scmd->uscsi_status = 0;
		    if (ssc->ses_arq) {
			bzero((caddr_t)pkt->pkt_scbp,
			    sizeof (struct scsi_arq_status));
		    }
		    SES_ENABLE_RESTART(
			(err == EBUSY)? SES_BUSY_TIME: SES_RESTART_TIME,
			(struct scsi_pkt *)bp->av_back);
		    return;
		}
		break;

	case QUE_SENSE:
		SES_LOG(ssc, SES_CE_DEBUG1, "retrying sense");
		if (ssc->ses_retries > SES_NO_RETRY) {
		    ssc->ses_retries -= SES_SENSE_RETRY;
		    scmd->uscsi_status = 0;
		    bzero((caddr_t)&ssc->ses_srqsbuf,
			sizeof (struct scsi_extended_sense));
		    if (scsi_transport(ssc->ses_rqpkt) != TRAN_ACCEPT) {
			SES_ENABLE_RESTART(SES_RESTART_TIME, ssc->ses_rqpkt);
		    }
		    return;
		}
		break;

	case COMMAND_DONE:
		SES_LOG(ssc, SES_CE_DEBUG4, "cmd done");
		pkt = (struct scsi_pkt *)bp->av_back;
		bp->b_resid = pkt->pkt_resid;
		if (bp->b_resid) {
		    SES_LOG(ssc, SES_CE_DEBUG6,
			"transfer residue %d(%d)",
			bp->b_bcount - bp->b_resid, bp->b_bcount);
		}
		break;
	}
	pkt = (struct scsi_pkt *)bp->av_back;
	if (err) {
		SES_LOG(ssc, SES_CE_DEBUG1, "SES: ERROR %d\n", err);
		SET_BP_ERROR(bp, err);
		bp->b_resid = bp->b_bcount;
	}
	scsi_destroy_pkt(pkt);
	biodone(bp);
}


/*
 * Check status and sense data and determine recovery.
 */
static int
ses_decode_sense(struct scsi_pkt *pkt, int *err)
{
	ses_softc_t *ssc = (ses_softc_t *)pkt->pkt_private;
	struct	scsi_extended_sense *sense =
	    (struct scsi_extended_sense *)&ssc->ses_srqsbuf;
	Uscmd *scmd = &ssc->ses_uscsicmd;
	char sense_flag = 0;
	uchar_t status = SCBP_C(pkt) & STATUS_MASK;
	char *err_action;
	char action;

	/*
	 * Process manual request sense.
	 * Copy manual request sense to sense buffer.
	 *
	 * This is done if auto request sense is not enabled.
	 * Or the auto request sense failed and the request
	 * sense needs to be retried.
	 */
	if (pkt->pkt_flags & FLAG_SENSING) {
		register struct buf *sbp = ssc->ses_rqbp;
		int amt = min(SENSE_LENGTH,
		    sbp->b_bcount - sbp->b_resid);

		bcopy(sbp->b_un.b_addr, (caddr_t)sense, amt);
		scmd->uscsi_rqresid = scmd->uscsi_rqlen - amt;
		sense_flag = 1;

	/*
	 * Process auto request sense.
	 * Copy auto request sense to sense buffer.
	 *
	 * If auto request sense failed due to transport error,
	 * retry the command.  Otherwise process the status and
	 * sense data.
	 */
	} else if (ssc->ses_arq && pkt->pkt_state & STATE_ARQ_DONE) {
		struct scsi_arq_status *arq =
			(struct scsi_arq_status *)(pkt->pkt_scbp);
		int amt = min(sizeof (arq->sts_sensedata), SENSE_LENGTH);
		uchar_t *arq_status = (uchar_t *)&arq->sts_rqpkt_status;

		if (arq->sts_rqpkt_reason != CMD_CMPLT) {
			return (QUE_COMMAND);
		}
		bcopy((caddr_t)&arq->sts_sensedata, (caddr_t)sense, amt);
		scmd->uscsi_status = status;
		scmd->uscsi_rqresid = scmd->uscsi_rqlen - amt;
		status = *arq_status & STATUS_MASK;
		pkt->pkt_state &= ~STATE_ARQ_DONE;
		sense_flag = 1;
	}


	/*
	 * Check status of REQUEST SENSE or command.
	 *
	 * If it's not successful, try retrying the original command
	 * and hope that it goes away.  If not, we'll eventually run
	 * out of retries and die.
	 */
	switch (status) {
	case STATUS_GOOD:
	case STATUS_INTERMEDIATE:
	case STATUS_MET:
		/*
		 * If the command status is ok, we're done.
		 * Otherwise, examine the request sense data.
		 */
		if (! sense_flag) {
			*err = EOK;
			return (COMMAND_DONE);
		}
		break;

	case STATUS_CHECK:
		SES_LOG(ssc, SES_CE_DEBUG3, "status decode: check");
		*err = EIO;
		return (QUE_SENSE);
		/* break; */

	case STATUS_BUSY:
		SES_LOG(ssc, SES_CE_DEBUG1, "status decode: busy");
		/* SES_CMD_RETRY2(ssc->ses_retries); */
		*err = EBUSY;
		return (QUE_COMMAND);
		/* break; */

	case STATUS_RESERVATION_CONFLICT:
		SES_LOG(ssc, SES_CE_DEBUG1, "status decode: reserved");
		*err = EACCES;
		return (COMMAND_DONE_ERROR);
		/* break; */

	case STATUS_TERMINATED:
		SES_LOG(ssc, SES_CE_DEBUG1, "status decode: terminated");
		*err = ECANCELED;
		return (COMMAND_DONE_ERROR);
		/* break; */

	default:
		SES_LOG(ssc, SES_CE_DEBUG1, "status 0x%x", status);
		*err = EIO;
		return (QUE_COMMAND);
		/* break; */
	}


	/*
	 * Check REQUEST SENSE error code.
	 *
	 * Either there's no error, a retryable error,
	 * or it's dead.  SES devices aren't very complex.
	 */
	err_action = "retrying";
	switch (sense->es_key) {
	case KEY_RECOVERABLE_ERROR:
		*err = EOK;
		err_action = "recovered";
		action = COMMAND_DONE;
		break;

	case KEY_UNIT_ATTENTION:
		/*
		 * This is common for RAID!
		 */
		/* *err = EIO; */
		SES_CMD_RETRY1(ssc->ses_retries);
		action = QUE_COMMAND_NOW;
		break;

	case KEY_NOT_READY:
	case KEY_NO_SENSE:
		/* *err = EIO; */
		action = QUE_COMMAND;
		break;

	default:
		/* *err = EIO; */
		err_action = "fatal";
		action = COMMAND_DONE_ERROR;
		break;
	}
	SES_LOG(ssc, SES_CE_DEBUG1,
		"cdb[0]= 0x%x %s,  key=0x%x, ASC/ASCQ=0x%x/0x%x",
		scmd->uscsi_cdb[0], err_action,
		sense->es_key, sense->es_add_code, sense->es_qual_code);

#ifdef 	not
	/*
	 * Dump cdb and sense data stat's for manufacturing.
	 */
	if (DEBUGGING_ERR || sd_error_level == SDERR_ALL) {
		auto buf[128];

		p = pkt->pkt_cdbp;
		if ((j = scsi_cdb_size[CDB_GROUPID(*p)]) == 0)
			j = CDB_SIZE;

		/* Print cdb */
		(void) sprintf(buf, "cmd:");
		for (i = 0; i < j; i++) {
			(void) sprintf(&buf[strlen(buf)],
			    hex, (uchar_t)*p++);
		}
		SES_LOG(ssc, SES_CE_DEBUG3, "%s", buf);

		/* Suppress trailing zero's in sense data */
		if (amt > 3) {
			p = (char *)devp->sd_sense + amt;
			for (j = amt; j > 3; j--) {
				if (*(--p))  break;
			}
		} else {
			j = amt;
		}

		/* Print sense data. */
		(void) sprintf(buf, "sense:");
		p = (char *)devp->sd_sense;
		for (i = 0; i < j; i++) {
			(void) sprintf(&buf[strlen(buf)],
			    hex, (uchar_t)*p++);
		}
		SES_LOG(ssc, SES_CE_DEBUG3, "%s", buf);
	}
#endif 	/* not */
	return (action);
}


/*ARGSUSED*/
void
ses_log(ses_softc_t *ssc, int level, const char *fmt, ...)
{
	va_list	ap;
	char buf[256];

	va_start(ap, fmt);
	(void) vsprintf(buf, fmt, ap);
	va_end(ap);

	if (ssc == (ses_softc_t *)NULL) {
		switch (level) {
		case SES_CE_DEBUG1:
			if (ses_debug > 1)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG2:
			if (ses_debug > 2)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG3:
			if (ses_debug > 3)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG4:
			if (ses_debug > 4)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG5:
			if (ses_debug > 5)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG6:
			if (ses_debug > 6)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG7:
			if (ses_debug > 7)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG8:
			if (ses_debug > 8)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case SES_CE_DEBUG9:
			if (ses_debug > 9)
				cmn_err(CE_NOTE, "%s", buf);
			break;
		case CE_NOTE:
		case CE_WARN:
		case CE_PANIC:
			cmn_err(level, "%s", buf);
			break;
		case SES_CE_DEBUG:
		default:
			cmn_err(CE_NOTE, "%s", buf);
		break;
		}
		return;
	}

	switch (level) {
	case CE_CONT:
	case CE_NOTE:
	case CE_WARN:
	case CE_PANIC:
		scsi_log(SES_DEVINFO(ssc), (char *)Snm, level, Str, buf);
		break;
	case SES_CE_DEBUG1:
		if (ses_debug > 1)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG2:
		if (ses_debug > 2)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG3:
		if (ses_debug > 3)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG4:
		if (ses_debug > 4)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG5:
		if (ses_debug > 5)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG6:
		if (ses_debug > 6)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG7:
		if (ses_debug > 7)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG8:
		if (ses_debug > 8)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG9:
		if (ses_debug > 9)
			scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG,
			    Str, buf);
		break;
	case SES_CE_DEBUG:
	default:
		scsi_log(SES_DEVINFO(ssc), (char *)Snm, SCSI_DEBUG, Str, buf);
		break;
	}
}
/*
 * mode: c
 * Local variables:
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -8
 * c-argdecl-indent: 8
 * c-label-offset: -8
 * c-continued-statement-offset: 8
 * c-continued-brace-offset: 0
 * End:
 */
