/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sad.c	1.37	99/08/23 SMI"

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * STREAMS Administrative Driver
 *
 * Currently only handles autopush and module name verification.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/pcb.h>
#include <sys/user.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/conf.h>
#include <sys/sad.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/cmn_err.h>

static struct module_info sad_minfo = {
	0x7361, "sad", 0, INFPSZ, 0, 0
};

static int sadopen(queue_t *, dev_t *, int, int, cred_t *);
static int sadclose(queue_t *, int, cred_t *);
static int sadwput(queue_t *qp, mblk_t *mp);

static struct qinit sad_rinit = {
	NULL, NULL, sadopen, sadclose, NULL, &sad_minfo, NULL
};

static struct qinit sad_winit = {
	sadwput, NULL, NULL, NULL, NULL, &sad_minfo, NULL
};

struct streamtab sadinfo = {
	&sad_rinit, &sad_winit, NULL, NULL
};

static int sad_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int sad_identify(dev_info_t *devi);
static int sad_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static dev_info_t *sad_dip;		/* private copy of devinfo pointer */
static struct autopush *strpfreep;	/* autopush freelist */

#define	SAD_CONF_FLAG	(D_NEW | D_MTPERQ | D_MP)
	DDI_DEFINE_STREAM_OPS(sad_ops, sad_identify, nulldev,	\
			sad_attach, nodev, nodev,		\
			sad_info, SAD_CONF_FLAG, &sadinfo);



static struct autopush *ap_alloc(), *ap_hfind();
static void ap_hadd(), ap_hrmv();
static void apush_ioctl(), apush_iocdata(), nak_ioctl(), ack_ioctl();
static void vml_ioctl(), vml_iocdata();
static int valid_list();
static int valid_major(major_t);
void sadinit();

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>

extern struct dev_ops sad_ops;
extern kmutex_t sad_lock;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Streams Administrative driver'sad'",
	&sad_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
sad_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "sad") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}

/* ARGSUSED */
static int
sad_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (ddi_create_minor_node(devi, "user", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	if (ddi_create_minor_node(devi, "admin", S_IFCHR,
	    1, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	sad_dip = devi;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
sad_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (sad_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *) sad_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}


/*
 * sadinit() -
 * Initialize autopush freelist.
 */
void
sadinit()
{
	struct autopush *ap;
	int i;

	/*
	 * build the autpush freelist.
	 */
	strpfreep = autopush;
	ap = autopush;
	for (i = 1; i < nautopush; i++) {
		ap->ap_nextp = &autopush[i];
		ap->ap_flags = APFREE;
		ap = ap->ap_nextp;
	}
	ap->ap_nextp = NULL;
	ap->ap_flags = APFREE;
}

/*
 * sadopen() -
 * Allocate a sad device.  Only one
 * open at a time allowed per device.
 */
/* ARGSUSED */
static int
sadopen(
	queue_t *qp,	/* pointer to read queue */
	dev_t *devp,	/* major/minor device of stream */
	int flag,	/* file open flags */
	int sflag,	/* stream open flags */
	cred_t *credp)	/* user credentials */
{
	int i;

	if (sflag)		/* no longer called from clone driver */
		return (EINVAL);

	/*
	 * Both USRMIN and ADMMIN are clone interfaces.
	 */
	for (i = 0; i < sadcnt; i++)
		if (saddev[i].sa_qp == NULL)
			break;
	if (i >= sadcnt)		/* no such device */
		return (ENXIO);

	switch (getminor(*devp)) {
	case USRMIN:			/* mere mortal */
		saddev[i].sa_flags = 0;
		break;

	case ADMMIN:			/* priviledged user */
		saddev[i].sa_flags = SADPRIV;
		break;

	default:
		return (EINVAL);
	}

	saddev[i].sa_qp = qp;
	qp->q_ptr = (caddr_t)&saddev[i];
	WR(qp)->q_ptr = (caddr_t)&saddev[i];

	/*
	 * NOTE: should the ADMMIN or USRMIN minors change
	 * then so should the offset of 2 below
	 * Both USRMIN and ADMMIN are clone interfaces and
	 * therefore their minor numbers (0 and 1) are reserved.
	 */
	*devp = makedevice(getemajor(*devp), i + 2);
	qprocson(qp);
	return (0);
}

/*
 * sadclose() -
 * Clean up the data structures.
 */
/* ARGSUSED */
static int
sadclose(
	queue_t *qp,	/* pointer to read queue */
	int flag,	/* file open flags */
	cred_t *credp)	/* user credentials */
{
	struct saddev *sadp;

	qprocsoff(qp);
	sadp = (struct saddev *)qp->q_ptr;
	sadp->sa_qp = NULL;
	sadp->sa_addr = NULL;
	qp->q_ptr = NULL;
	WR(qp)->q_ptr = NULL;
	return (0);
}

/*
 * sadwput() -
 * Write side put procedure.
 */
static int
sadwput(
	queue_t *qp,	/* pointer to write queue */
	mblk_t *mp)	/* message pointer */
{
	struct iocblk *iocp;

	switch (mp->b_datap->db_type) {
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			*mp->b_rptr &= ~FLUSHW;
			qreply(qp, mp);
		} else
			freemsg(mp);
		break;

	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (SAD_CMD(iocp->ioc_cmd)) {
		case SAD_CMD(SAD_SAP):
		case SAD_CMD(SAD_GAP):
			apush_ioctl(qp, mp);
			break;

		case SAD_VML:
			vml_ioctl(qp, mp);
			break;

		default:
			nak_ioctl(qp, mp, EINVAL);
			break;
		}
		break;

	case M_IOCDATA:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (SAD_CMD(iocp->ioc_cmd)) {
		case SAD_CMD(SAD_SAP):
		case SAD_CMD(SAD_GAP):
			apush_iocdata(qp, mp);
			break;

		case SAD_VML:
			vml_iocdata(qp, mp);
			break;

		default:
			cmn_err(CE_WARN,
			    "sadwput: invalid ioc_cmd in case M_IOCDATA: %d",
			    iocp->ioc_cmd);
			freemsg(mp);
			break;
		}
		break;

	default:
		freemsg(mp);
		break;
	} /* switch (db_type) */
	return (0);
}

/*
 * ack_ioctl() -
 * Send an M_IOCACK message in the opposite
 * direction from qp.
 */
static void
ack_ioctl(
	queue_t *qp,	/* queue pointer */
	mblk_t *mp,	/* message block to use */
	int count,	/* number of bytes to copyout */
	int rval,	/* return value for icotl */
	int errno)	/* error number to return */
{
	struct iocblk *iocp;

	iocp = (struct iocblk *)mp->b_rptr;
	iocp->ioc_count = count;
	iocp->ioc_rval = rval;
	iocp->ioc_error = errno;
	mp->b_datap->db_type = M_IOCACK;
	qreply(qp, mp);
}

/*
 * nak_ioctl() -
 * Send an M_IOCNAK message in the opposite
 * direction from qp.
 */
static void
nak_ioctl(
	queue_t *qp,	/* queue pointer */
	mblk_t *mp,	/* message block to use */
	int errno)	/* error number to return */
{
	struct iocblk *iocp;

	iocp = (struct iocblk *)mp->b_rptr;
	iocp->ioc_error = errno;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}
	mp->b_datap->db_type = M_IOCNAK;
	qreply(qp, mp);
}

/*
 * apush_ioctl() -
 * Handle the M_IOCTL messages associated with
 * the autopush feature.
 */
static void
apush_ioctl(
	queue_t *qp,	/* pointer to write queue */
	mblk_t *mp)	/* message pointer */
{
	struct iocblk *iocp;
	struct copyreq *cqp;
	struct saddev *sadp;

	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_count != TRANSPARENT) {
		nak_ioctl(qp, mp, EINVAL);
		return;
	}
	if (SAD_VER(iocp->ioc_cmd) > AP_VERSION) {
		nak_ioctl(qp, mp, EINVAL);
		return;
	}

	sadp = (struct saddev *)qp->q_ptr;
	switch (SAD_CMD(iocp->ioc_cmd)) {
	case SAD_CMD(SAD_SAP):
		if (!(sadp->sa_flags & SADPRIV)) {
			nak_ioctl(qp, mp, EPERM);
			break;
		}
		/* FALLTHRU */

	case SAD_CMD(SAD_GAP):
		cqp = (struct copyreq *)mp->b_rptr;
		cqp->cq_addr = (caddr_t)*(intptr_t *)mp->b_cont->b_rptr;
		sadp->sa_addr = cqp->cq_addr;
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
		if (SAD_VER(iocp->ioc_cmd) == 1)
			cqp->cq_size = STRAPUSH_V1_LEN;
		else
			cqp->cq_size = STRAPUSH_V0_LEN;
		cqp->cq_flag = 0;
		cqp->cq_private = (mblk_t *)GETSTRUCT;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		qreply(qp, mp);
		break;

	default:
		ASSERT(0);
		nak_ioctl(qp, mp, EINVAL);
		break;
	} /* switch (ioc_cmd) */
}

/*
 * apush_iocdata() -
 * Handle the M_IOCDATA messages associated with
 * the autopush feature.
 */
static void
apush_iocdata(
	queue_t *qp,	/* pointer to write queue */
	mblk_t *mp)	/* message pointer */
{
	int i, ret;
	struct copyreq *cqp;
	struct copyresp *csp;
	struct strapush *sap;
	struct autopush *ap;
	struct saddev *sadp;
	int tmp_list[MAXAPUSH];

	csp = (struct copyresp *)mp->b_rptr;
	cqp = (struct copyreq *)mp->b_rptr;
	if (csp->cp_rval) {	/* if there was an error */
		freemsg(mp);
		return;
	}
	if (mp->b_cont)
		/* sap needed only if mp->b_cont is set */
		sap = (struct strapush *)mp->b_cont->b_rptr;
	switch (SAD_CMD(csp->cp_cmd)) {
	case SAD_CMD(SAD_SAP):
		switch ((long)csp->cp_private) {
		case GETSTRUCT:
			switch (sap->sap_cmd) {
			case SAP_ONE:
			case SAP_RANGE:
			case SAP_ALL:
				if ((sap->sap_npush == 0) ||
				    (sap->sap_npush > MAXAPUSH) ||
				    (sap->sap_npush > nstrpush)) {

					/* invalid number of modules to push */

					nak_ioctl(qp, mp, EINVAL);
					break;
				}
				if (ret = valid_major(sap->sap_major)) {
					nak_ioctl(qp, mp, ret);
					break;
				}
				if ((sap->sap_cmd == SAP_RANGE) &&
				    (sap->sap_lastminor <= sap->sap_minor)) {

					/* bad range */

					nak_ioctl(qp, mp, ERANGE);
					break;
				}
				if (!valid_list(sap)) {

					/* bad module name */

					nak_ioctl(qp, mp, EINVAL);
					break;
				}
				for (i = 0; i < sap->sap_npush; i++)
					tmp_list[i] =
					    findmod(sap->sap_list[i]);
				mutex_enter(&sad_lock);
				if (ap_hfind(sap->sap_major, sap->sap_minor,
				    sap->sap_lastminor, sap->sap_cmd)) {
					mutex_exit(&sad_lock);

					/* already configured */

					nak_ioctl(qp, mp, EEXIST);
					break;
				}
				if ((ap = ap_alloc()) == NULL) {
					mutex_exit(&sad_lock);

					/* no autopush structures - EAGAIN? */

					nak_ioctl(qp, mp, ENOSR);
					break;
				}
				ap->ap_cnt++;
				ap->ap_common = sap->sap_common;
				if (SAD_VER(csp->cp_cmd) > 0)
					ap->ap_anchor = sap->sap_anchor;
				else
					ap->ap_anchor = 0;
				for (i = 0; i < ap->ap_npush; i++)
					ap->ap_list[i] = tmp_list[i];
				ap_hadd(ap);
				mutex_exit(&sad_lock);
				ack_ioctl(qp, mp, 0, 0, 0);
				for (i = 0; i < ap->ap_npush; i++) {
					(void) fmod_release(ap->ap_list[i]);
				}
				break;

			case SAP_CLEAR:
				if (ret = valid_major(sap->sap_major)) {
					nak_ioctl(qp, mp, ret);
					break;
				}
				mutex_enter(&sad_lock);
				if ((ap = ap_hfind(sap->sap_major,
				    sap->sap_minor, sap->sap_lastminor,
				    sap->sap_cmd)) == NULL) {
					mutex_exit(&sad_lock);

					/* not configured */

					nak_ioctl(qp, mp, ENODEV);
					break;
				}
				if ((ap->ap_type == SAP_RANGE) &&
				    (sap->sap_minor != ap->ap_minor)) {
					mutex_exit(&sad_lock);

					/* starting minors do not match */

					nak_ioctl(qp, mp, ERANGE);
					break;
				}
				if ((ap->ap_type == SAP_ALL) &&
				    (sap->sap_minor != 0)) {
					mutex_exit(&sad_lock);

					/* SAP_ALL must have minor == 0 */

					nak_ioctl(qp, mp, EINVAL);
					break;
				}
				ap_hrmv(ap);
				if (--(ap->ap_cnt) <= 0)
					ap_free(ap);
				mutex_exit(&sad_lock);
				ack_ioctl(qp, mp, 0, 0, 0);
				break;

			default:
				nak_ioctl(qp, mp, EINVAL);
				break;
			} /* switch (sap_cmd) */
			break;

		default:
			cmn_err(CE_WARN,
			    "apush_iocdata: cp_private bad in SAD_SAP: %p",
			    (void *)csp->cp_private);
			freemsg(mp);
			break;
		} /* switch (cp_private) */
		break;

	case SAD_CMD(SAD_GAP):
		switch ((long)csp->cp_private) {

		case GETSTRUCT: {
			if (ret = valid_major(sap->sap_major)) {
				nak_ioctl(qp, mp, ret);
				break;
			}
			mutex_enter(&sad_lock);
			if ((ap = ap_hfind(sap->sap_major, sap->sap_minor,
			    sap->sap_lastminor, SAP_ONE)) == NULL) {
				mutex_exit(&sad_lock);

				/* not configured */

				nak_ioctl(qp, mp, ENODEV);
				break;
			}

			sap->sap_common = ap->ap_common;
			if (SAD_VER(csp->cp_cmd) > 0)
				sap->sap_anchor = ap->ap_anchor;
			for (i = 0; i < ap->ap_npush; i++)
				(void) strcpy(sap->sap_list[i],
				    fmodsw[ap->ap_list[i]].f_name);
			for (; i < MAXAPUSH; i++)
				bzero(sap->sap_list[i], FMNAMESZ + 1);
			mutex_exit(&sad_lock);
			mp->b_datap->db_type = M_COPYOUT;
			mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
			cqp->cq_private = (mblk_t *)GETRESULT;
			sadp = (struct saddev *)qp->q_ptr;
			cqp->cq_addr = sadp->sa_addr;
			if (SAD_VER(csp->cp_cmd) == 1)
				cqp->cq_size = STRAPUSH_V1_LEN;
			else
				cqp->cq_size = STRAPUSH_V0_LEN;
			cqp->cq_flag = 0;
			qreply(qp, mp);
			break;
			}
		case GETRESULT:
			ack_ioctl(qp, mp, 0, 0, 0);
			break;

		default:
			cmn_err(CE_WARN,
			    "apush_iocdata: cp_private bad case SAD_GAP: %p",
			    (void *)csp->cp_private);
			freemsg(mp);
			break;
		} /* switch (cp_private) */
		break;

	default:	/* can't happen */
		ASSERT(0);
		freemsg(mp);
		break;
	} /* switch (cp_cmd) */
}

/*
 * ap_alloc() -
 * Allocate an autopush structure.
 */
static struct autopush *
ap_alloc(void)
{
	struct autopush *ap;

	ASSERT(MUTEX_HELD(&sad_lock));
	if (strpfreep == NULL)
		return (NULL);
	ap = strpfreep;
	if (ap->ap_flags != APFREE)
		cmn_err(CE_PANIC, "ap_alloc: autopush struct not free: %d",
		    ap->ap_flags);
	strpfreep = strpfreep->ap_nextp;
	ap->ap_nextp = NULL;
	ap->ap_flags = APUSED;
	return (ap);
}

/*
 * ap_free() -
 * Give an autopush structure back to the freelist.
 */
void
ap_free(struct autopush *ap)
{
	ASSERT(MUTEX_HELD(&sad_lock));
	if (!(ap->ap_flags & APUSED))
		cmn_err(CE_PANIC, "ap_free: autopush struct not used: %d",
		    ap->ap_flags);
	if (ap->ap_flags & APHASH)
		cmn_err(CE_PANIC, "ap_free: autopush struct not hashed: %d",
		    ap->ap_flags);
	ap->ap_flags = APFREE;
	ap->ap_nextp = strpfreep;
	strpfreep = ap;
}

/*
 * ap_hadd() -
 * Add an autopush structure to the hash list.
 */
static void
ap_hadd(struct autopush *ap)
{
	ASSERT(MUTEX_HELD(&sad_lock));
	if (!(ap->ap_flags & APUSED))
		cmn_err(CE_PANIC, "ap_hadd: autopush struct not used: %d",
		    ap->ap_flags);
	if (ap->ap_flags & APHASH)
		cmn_err(CE_PANIC, "ap_hadd: autopush struct not hashed: %d",
		    ap->ap_flags);
	ap->ap_nextp = strphash(ap->ap_major);
	strphash(ap->ap_major) = ap;
	ap->ap_flags |= APHASH;
}

/*
 * ap_hrmv() -
 * Remove an autopush structure from the hash list.
 */
static void
ap_hrmv(struct autopush *ap)
{
	struct autopush *hap;
	struct autopush *prevp = NULL;

	ASSERT(MUTEX_HELD(&sad_lock));
	if (!(ap->ap_flags & APUSED))
		cmn_err(CE_PANIC, "ap_hrmv: autopush struct not used: %d",
		    ap->ap_flags);
	if (!(ap->ap_flags & APHASH))
		cmn_err(CE_PANIC, "ap_hrmv: autopush struct not hashed: %d",
		    ap->ap_flags);

	hap = strphash(ap->ap_major);
	while (hap) {
		if (ap == hap) {
			hap->ap_flags &= ~APHASH;
			if (prevp)
				prevp->ap_nextp = hap->ap_nextp;
			else
				strphash(ap->ap_major) = hap->ap_nextp;
			return;
		} /* if */
		prevp = hap;
		hap = hap->ap_nextp;
	} /* while */
}

/*
 * ap_hfind() -
 * Look for an autopush structure in the hash list
 * based on major, minor, lastminor, and command.
 */
static struct autopush *
ap_hfind(
	major_t maj,	/* major device number */
	minor_t minor,	/* minor device number */
	minor_t last,	/* last minor device number (SAP_RANGE only) */
	uint_t cmd)	/* who is asking */
{
	struct autopush *ap;

	ASSERT(MUTEX_HELD(&sad_lock));
	ap = strphash(maj);
	while (ap) {
		if (ap->ap_major == maj) {
			if (cmd == SAP_ALL)
				break;
			switch (ap->ap_type) {
			case SAP_ALL:
				break;

			case SAP_ONE:
				if (ap->ap_minor == minor)
					break;
				if ((cmd == SAP_RANGE) &&
				    (ap->ap_minor >= minor) &&
				    (ap->ap_minor <= last))
					break;
				ap = ap->ap_nextp;
				continue;

			case SAP_RANGE:
				if ((cmd == SAP_RANGE) &&
				    (((minor >= ap->ap_minor) &&
				    (minor <= ap->ap_lastminor)) ||
				    ((ap->ap_minor >= minor) &&
				    (ap->ap_minor <= last))))
					break;
				if ((minor >= ap->ap_minor) &&
				    (minor <= ap->ap_lastminor))
					break;
				ap = ap->ap_nextp;
				continue;

			default:
				ASSERT(0);
				break;
			}
			break;
		}
		ap = ap->ap_nextp;
	}
	return (ap);
}

/*
 * valid_list() -
 * Step through the list of modules to autopush and
 * validate their names.  This will load modules
 * with findmod() as needed. Return 1 if the list is
 * valid and 0 if it is not.
 */
static int
valid_list(struct strapush *sap)
{
	int i, idx;

	for (i = 0; i < sap->sap_npush; i++)
		if ((idx = findmod(sap->sap_list[i])) == -1)
			return (0);
		else
			(void) fmod_release(idx);
	return (1);
}

/*
 * vml_ioctl() -
 * Handle the M_IOCTL message associated with a request
 * to validate a module list.
 */
static void
vml_ioctl(
	queue_t *qp,	/* pointer to write queue */
	mblk_t *mp)	/* message pointer */
{
	struct iocblk *iocp;
	struct copyreq *cqp;

	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_count != TRANSPARENT) {
		nak_ioctl(qp, mp, EINVAL);
		return;
	}
	ASSERT(iocp->ioc_cmd == SAD_VML);
	cqp = (struct copyreq *)mp->b_rptr;
	cqp->cq_addr = (caddr_t)*(intptr_t *)mp->b_cont->b_rptr;
	freemsg(mp->b_cont);
	mp->b_cont = NULL;
	cqp->cq_size = SIZEOF_STRUCT(str_list, iocp->ioc_flag);
	cqp->cq_flag = 0;
	cqp->cq_private = (mblk_t *)GETSTRUCT;
	mp->b_datap->db_type = M_COPYIN;
	mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
	qreply(qp, mp);
}

/*
 * vml_iocdata() -
 * Handle the M_IOCDATA messages associated with
 * a request to validate a module list.
 */
static void
vml_iocdata(
	queue_t *qp,	/* pointer to write queue */
	mblk_t *mp)	/* message pointer */
{
	long i;
	int idx;
	int	nmods;
	struct copyreq *cqp;
	struct copyresp *csp;
	struct str_mlist *lp;
	STRUCT_HANDLE(str_list, slp);
	struct saddev *sadp;

	csp = (struct copyresp *)mp->b_rptr;
	cqp = (struct copyreq *)mp->b_rptr;
	if (csp->cp_rval) {	/* if there was an error */
		freemsg(mp);
		return;
	}

	ASSERT(csp->cp_cmd == SAD_VML);
	sadp = (struct saddev *)qp->q_ptr;
	switch ((long)csp->cp_private) {
	case GETSTRUCT:
		STRUCT_SET_HANDLE(slp, csp->cp_flag,
			(struct str_list *)mp->b_cont->b_rptr);
		nmods = STRUCT_FGET(slp, sl_nmods);
		if (nmods <= 0) {
			nak_ioctl(qp, mp, EINVAL);
			break;
		}
		sadp->sa_addr = (caddr_t)nmods;

		cqp->cq_addr = (caddr_t)STRUCT_FGETP(slp, sl_modlist);
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
		cqp->cq_size = (uintptr_t)sadp->sa_addr
				* sizeof (struct str_mlist);
		cqp->cq_flag = 0;
		cqp->cq_private = (mblk_t *)GETLIST;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		qreply(qp, mp);
		break;

	case GETLIST:
		lp = (struct str_mlist *)mp->b_cont->b_rptr;
		for (i = 0; i < (long)sadp->sa_addr; i++, lp++) {
			if ((idx = findmod(lp->l_name)) == -1) {
				ack_ioctl(qp, mp, 0, 1, 0);
				return;
			}
			(void) fmod_release(idx);
		}
		ack_ioctl(qp, mp, 0, 0, 0);
		break;

	default:
		cmn_err(CE_WARN, "vml_iocdata: invalid cp_private value: %p",
		    (void *)csp->cp_private);
		freemsg(mp);
		break;
	} /* switch (cp_private) */
}

/*
 * Validate a major number and also verify if
 * it is a STREAMS device.
 * Return values: 0 if a valid STREAMS dev
 *		  error code otherwise
 */
static int
valid_major(major_t major)
{
	struct dev_ops *ops = NULL;
	int ret = ENOSTR;	/* default is not a STREAMS device */

	if (etoimajor(major) == -1)
		return (EINVAL);

	/*
	 * attempt to load and attach the driver 'major'.
	 * If the driver fails to attach to any instances, then just load
	 * the driver to verify that it is a STREAMS driver.
	 */
	if ((ops = ddi_hold_installed_driver(major)) == NULL) {
		if ((ops = mod_hold_dev_by_major(major)) == NULL) {
			return (EINVAL);
		}
	}

	if ((ops->devo_cb_ops) && (STREAMSTAB(major)))
		ret = 0;		/* This is a STREAMS driver */

	ddi_rele_driver(major);

	return (ret);
}
