/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * This product and related documentation are protected by copyright
 * and distributed under licenses restricting their use, copying,
 * distribution and decompilation.  No part of this product may be
 * reproduced in any form by any means without prior written
 * authorization by Sun and its licensors, if any."
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 * Government is subject to restrictions as set forth in subparagraph
 * (c) (1) (ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 * NASA FAR Supplement.
 */

#pragma ident	"@(#)logindmux.c	1.19	99/07/30 SMI"

/*
 * Description: logindmux.c
 *
 * The logindmux driver is used with login modules (like telnet/rlogin).
 * This is a 1x1 cloning mux and two of these muxes are used. The lower link
 * of one of the muxes receives input from net and the lower link of the
 * other mux receives input from pseudo terminal subsystem.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/ptms.h>
#include <sys/logindmux.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/strsubr.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/telioctl.h>
#include <sys/rlioctl.h>
#include <sys/strlog.h>
#include <sys/termios.h>
#include <sys/tihdr.h>

static int logdmuxopen(queue_t *, dev_t *, int, int, cred_t *);
static int logdmuxclose(queue_t *, int, cred_t *);
static int logdmuxursrv(queue_t *);
static int logdmuxuwput(queue_t *, mblk_t *);
static int logdmuxlrput(queue_t *, mblk_t *);
static int logdmuxlrsrv(queue_t *);
static int logdmuxlwsrv(queue_t *);
static int logdmuxuwsrv(queue_t *);

static void logdmuxlink(queue_t *, mblk_t *);
static void logdmuxunlink(queue_t *, mblk_t *);
static void recover(queue_t *, mblk_t *, size_t);
static void flushq_dataonly(queue_t *);

#define	LOGDMX_ID	107
#define	SIMWAIT		(1*hz)

static struct module_info logdmuxm_info = {
	LOGDMX_ID,
	"logindmux",
	0,
	256,
	512,
	256
};

static struct qinit logdmuxurinit = {
	NULL,
	logdmuxursrv,
	logdmuxopen,
	logdmuxclose,
	NULL,
	&logdmuxm_info,
	NULL
};

static struct qinit logdmuxuwinit = {
	logdmuxuwput,
	logdmuxuwsrv,
	NULL,
	NULL,
	NULL,
	&logdmuxm_info,
	NULL
};

static struct qinit logdmuxlrinit = {
	logdmuxlrput,
	logdmuxlrsrv,
	NULL,
	NULL,
	NULL,
	&logdmuxm_info,
	NULL
};

static struct qinit logdmuxlwinit = {
	NULL,
	logdmuxlwsrv,
	NULL,
	NULL,
	NULL,
	&logdmuxm_info,
	NULL
};

struct streamtab logdmuxinfo = {
	&logdmuxurinit,
	&logdmuxuwinit,
	&logdmuxlrinit,
	&logdmuxlwinit
};


static int logdmux_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int logdmux_identify(dev_info_t *);
static int logdmux_attach(dev_info_t *, ddi_attach_cmd_t);
static int logdmux_detach(dev_info_t *, ddi_detach_cmd_t);
static dev_info_t *logdmux_dip;

static	struct	tmx	*telstrup = NULL;
static 	void	logdmux_sndnak(queue_t *, mblk_t *, int);
static	void	logdmux_sndack(queue_t *, mblk_t *, mblk_t *, size_t);

#ifdef DEBUG
int	logdmuxdebug = 1;
#endif

#define	D_SD_FLAGS	(D_NEW | D_MP | D_MTOUTPERIM | D_MTOCEXCL | D_MTPERQ)

static struct cb_ops cb_logdmux_ops = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&logdmuxinfo,		/* cb_stream */
	(int)D_SD_FLAGS		/* cb_flag */
};

static struct dev_ops logdmux_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	logdmux_info,		/* devo_getinfo */
	logdmux_identify,	/* devo_identify */
	nulldev,		/* devo_probe */
	logdmux_attach,		/* devo_attach */
	logdmux_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_logdmux_ops,	/* devo_cb_ops */
	NULL,			/* devo_bus_ops */
	NULL			/* devo_power */
};

/*
 * State for multiplexor queues	(XXX.sparker: Make this *one* structure!)
 */

struct tmx {
	unsigned	state; 		/* Driver state value */
	queue_t		*rdq;		/* Upper read queue */
	struct tmxl	*tmxl; 		/* Lower link for mux */
	struct	tmx	*nexttp; 	/* Next queue pointer */
	struct	tmx	*pair; 		/* Q for clone mux used in logind */
	minor_t		dev0;		/* minor device number */
	minor_t		dev1;		/* minor device number for clone */
	int		flag;		/* set 1 for M_DATA/M_FLUSH from ptm */
	bufcall_id_t	wbufcid;	/* needed for recovery */
	bufcall_id_t	rbufcid;	/* needed for recovery */
	timeout_id_t	wtimoutid;	/* needed for recovery */
	timeout_id_t	rtimoutid;	/* needed for recovery */
};

struct tmxl {
	int		muxid;		/* id of link */
	unsigned	ltype;		/* persistent or non-persistent link */
	queue_t		*muxq;		/* link to lower write queue of mux */
	struct tmx	*tmx;   	/* upper tmx queue of mux */
	queue_t		*peerq; 	/* lower peerq of mux pair */
};

/*
 * This is the loadable module wrapper.
 */
extern struct mod_ops mod_driverops;

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module.  This one is a driver */
	" LOGIND MUX Driver",
	&logdmux_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
logdmux_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "logindmux") != 0)
		return (DDI_NOT_IDENTIFIED);
	return (DDI_IDENTIFIED);
}

/*ARGSUSED*/
static int
logdmux_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (ddi_create_minor_node(devi, "logindmux", S_IFCHR,
	    0, NULL, CLONE_DEV) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}
	logdmux_dip = devi;
	return (DDI_SUCCESS);
}

static int
logdmux_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
logdmux_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (logdmux_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *) logdmux_dip;
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
 * Logindmux open routine
 */
/*ARGSUSED*/
static int
logdmuxopen(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *crp)
{
	struct tmx *tp;
	struct tmx **prevtp;
	minor_t	minordev;

	if (sflag != CLONEOPEN)
		return (EINVAL);

	minordev = 0;

	for (prevtp = &telstrup; (tp = *prevtp) != NULL;
		prevtp = &tp->nexttp) {
		if (minordev < tp->dev0)
			break;
		minordev++;
	}
	*devp = makedevice(getmajor(*devp), minordev);

	tp = (struct tmx *)kmem_zalloc(sizeof (*tp), KM_SLEEP);
	tp->state |= TMXOPEN;
	tp->rdq = q;
	tp->tmxl = (struct tmxl *)kmem_zalloc(sizeof (struct tmxl), KM_SLEEP);
	tp->dev0 = minordev;
	tp->dev1 = 0;
	tp->flag = 0;
	tp->nexttp = *prevtp;
	*prevtp = tp;
	q->q_ptr = tp;
	WR(q)->q_ptr = tp;
	qprocson(q);
	return (0);
}
/*
 * Logindmux close routine gets called when telnet connection is closed
 */
/*ARGSUSED*/
static int
logdmuxclose(queue_t *q, int flag, cred_t *crp)
{

	mblk_t		*mp;
	struct	tmx	*tp, *tmxp;
	struct	tmx	**prevtp;
	struct	tmx	*tmxpeerp = 0;

	tmxp = (struct tmx *)q->q_ptr;

	/*
	 * Flush any write side data  downstream
	 */
	while ((mp = getq(WR(q))) != NULL) {
		if ((tmxp->tmxl) && (tmxp->tmxl->muxq))
			putnext(tmxp->tmxl->muxq, mp);
		else
			freemsg(mp);
	}

	qprocsoff(q);
	if (tmxp->wbufcid) {
		qunbufcall(q, tmxp->wbufcid);
		tmxp->wbufcid = 0;
	}
	if (tmxp->rbufcid) {
		qunbufcall(q, tmxp->rbufcid);
		tmxp->rbufcid = 0;
	}
	if (tmxp->rtimoutid) {
		(void) quntimeout(q, tmxp->rtimoutid);
		tmxp->rtimoutid = 0;
	}
	if (tmxp->wtimoutid) {
		(void) quntimeout(q, tmxp->wtimoutid);
		tmxp->wtimoutid = 0;
	}

	for (prevtp = &telstrup; (tp = *prevtp) != NULL;
	    prevtp = &tp->nexttp) {
		if (tp == tmxp)
			break;
	}
	*prevtp = tp->nexttp;

	tmxp->state &= ~TMXOPEN;
	tmxp->rdq = NULL;

	/*
	 * Sometimes tmxp->tmxl isn't NULL'ed by logdmuxunlink by the time we
	 * get here.  Just in case we clean it up.  One day, when we're sure
	 * this can't happen, it should become ASSERT(tmxp->tmxl == NULL);
	 */
	if (tmxp->tmxl) {
		if (tmxp->tmxl->peerq)
			tmxpeerp = tmxp->pair;
		tmxp->tmxl->peerq = 0;

		tmxp->tmxl->muxq = 0;
		tmxp->tmxl->muxid = 0;
		if (tmxpeerp && tmxpeerp->tmxl)
			tmxpeerp->tmxl->peerq = 0;
		tmxp->tmxl->tmx = 0;

		kmem_free((char *)tmxp->tmxl, sizeof (struct tmxl));
		tmxp->tmxl = NULL;
	}
	kmem_free((char *)tmxp, sizeof (struct tmx));
	q->q_ptr = NULL;
	WR(q)->q_ptr = NULL;

	return (0);
}

/*
 * Upper read service routine
 */
static int
logdmuxursrv(queue_t *q)
{
	struct tmx *tmx;

	tmx = (struct tmx *)(q->q_ptr);

	if (tmx->tmxl->muxq)
		qenable(RD(tmx->tmxl->muxq));
	return (0);
}

/*
 * This routine gets called when telnet daemon sends data or ioctl messages
 * to upper mux queue.
 */
static int
logdmuxuwput(queue_t *q, mblk_t *mp)
{
	queue_t *qp;
	mblk_t	*newmp;
	struct iocblk *ioc;
	minor_t	minor;
	STRUCT_HANDLE(protocol_arg, protoh);
	struct tmx	*saveq0, *saveq1, *tp;
	int	qexchange;

	saveq0 = (struct tmx *)q->q_ptr;

	switch (mp->b_datap->db_type) {

	case M_IOCTL:
		if ((mp->b_wptr - mp->b_rptr) != sizeof (*ioc)) {
			logdmux_sndnak(q, mp, EINVAL);
			break;
		}
		ioc = (struct iocblk *)mp->b_rptr;
		switch (ioc->ioc_cmd) {

		/*
		 * This is a special ioctl call sent to reenable
		 * telnetp queues and also reinsert the data read
		 * from stream head at the time telnetp was pushed.
		 */
		case TEL_IOC_ENABLE:
		case RL_IOC_ENABLE:
		case TEL_IOC_MODE:
		case TEL_IOC_GETBLK:
			if (ioc->ioc_count == TRANSPARENT) {
				logdmux_sndnak(q, mp, EINVAL);
				break;
			}
			if (saveq0->tmxl == NULL ||
			    saveq0->tmxl->muxq == NULL) {
				logdmux_sndnak(q, mp, EINVAL);
				break;
			}
			qp = saveq0->tmxl->muxq;
			putnext(qp, mp);
			break;

		/*
		 * This is a special ioctl which exchanges q info
		 * of the two pairs, connected to netf and ptmx.
		 */
		case LOGDMX_IOC_QEXCHANGE:
			if (ioc->ioc_count == TRANSPARENT) {
				logdmux_sndnak(q, mp, EINVAL);
				break;
			}
			STRUCT_SET_HANDLE(protoh, ioc->ioc_flag,
			    (struct protocol_arg *)mp->b_cont->b_rptr);
#ifdef _SYSCALL32_IMPL
			if ((ioc->ioc_flag & DATAMODEL_MASK) ==
			    DATAMODEL_ILP32) {
				minor = getminor(expldev(
				    STRUCT_FGET(protoh, dev)));
			} else
#endif
			{
				minor = getminor(STRUCT_FGET(protoh, dev));
			}
			((struct tmx *)(q->q_ptr))->dev1 = minor;
			qexchange = STRUCT_FGET(protoh, flag);

			for (tp = telstrup; tp; tp = tp->nexttp) {
				if (tp->dev0 == minor) {
					((struct tmx *)q->q_ptr)->pair = tp;
					break;
				}
			}

			saveq1 = ((struct tmx *)(q->q_ptr))->pair;
			if (saveq1 == NULL) {
				logdmux_sndnak(q, mp, EINVAL);
				break;
			}
			if (qexchange) {
				saveq0->tmxl->peerq = saveq1->tmxl->muxq;
				saveq1->tmxl->peerq = saveq0->tmxl->muxq;
				saveq0->flag = 1;
			}
			logdmux_sndack(q, mp, NULL, 0);
			break;

		case I_LINK:
			if ((mp->b_cont->b_wptr - mp->b_cont->b_rptr)
			    != sizeof (struct linkblk)) {
				logdmux_sndnak(q, mp, EINVAL);
				break;
			}
			qwriter(q, mp, logdmuxlink, PERIM_OUTER);
			break;

		case I_UNLINK:
			if ((mp->b_cont->b_wptr - mp->b_cont->b_rptr) !=
			    sizeof (struct linkblk)) {
				logdmux_sndnak(q, mp, EINVAL);
				break;
			}
			qwriter(q, mp, logdmuxunlink, PERIM_OUTER);
			break;

		default:
			if (!(saveq0->tmxl) || !(saveq0->tmxl->muxq)) {
				logdmux_sndnak(q, mp, EINVAL);
				break;
			}
			qp = saveq0->tmxl->muxq;

			if (!canputnext(qp)) {
				(void) putq(q, mp);
				return (0);
			}
			putnext(qp, mp);
		}

		break;

	case M_DATA:
		if (!(saveq0->flag)) {
			if ((newmp = allocb(sizeof (char), BPRI_MED)) == NULL) {
				recover(q, mp, msgdsize(mp));
				return (0);
			}
			newmp->b_datap->db_type = M_CTL;
			newmp->b_wptr = newmp->b_rptr + 1;
			*(newmp->b_rptr) = M_CTL_MAGIC_NUMBER;
			newmp->b_cont = mp;
			mp = newmp;
		}
		/* FALLTHRU */

	case M_PROTO:
	case M_PCPROTO:
		if (!(saveq0->tmxl) || !(saveq0->tmxl->muxq)) {
			mp->b_datap->db_type = M_ERROR;
			*mp->b_rptr = EINVAL;
			qreply(q, mp);
			break;
		}
		qp = saveq0->tmxl->muxq;
		if (!canputnext(qp) && (mp->b_datap->db_type < QPCTL)) {
			(void) putq(q, mp);
			return (0);
		}
		putnext(qp, mp);
		break;

	case M_FLUSH:
		if (!(saveq0->tmxl) || !(saveq0->tmxl->muxq)) {
			freemsg(mp);
			break;
		}
		if (*mp->b_rptr & FLUSHW)
			flushq(q, FLUSHALL);
		qp = saveq0->tmxl->muxq;
		mp->b_flag |= MSGMARK;
		putnext(qp, mp);
		break;

	default:
#ifdef DEBUG
		if (logdmuxdebug)
			debug_enter(NULL);
#else
		(void) strlog(LOGDMX_ID, -1, 0, SL_ERROR,
		    "logdmuxuwput: obtained 0x%x msg type\n",
		    mp->b_datap->db_type);
#endif
		freemsg(mp);
	}
	return (0);
}

/*
 * Upper write service routine
 */
static int
logdmuxuwsrv(queue_t *q)
{
	mblk_t  *mp, *newmp;
	queue_t	*qp;
	struct tmx	*saveq0;


	saveq0 = (struct tmx *)q->q_ptr;

	while ((mp = getq(q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case M_DATA:
			if (!(saveq0->flag)) {
				if ((newmp = allocb(sizeof (char), BPRI_MED)) ==
				    NULL) {
					recover(q, mp, msgdsize(mp));
					return (0);
				}
				newmp->b_datap->db_type = M_CTL;
				newmp->b_wptr = newmp->b_rptr + 1;
				*(newmp->b_rptr) = M_CTL_MAGIC_NUMBER;
				newmp->b_cont = mp;
				mp = newmp;
			}
			/* FALLTHRU */

		case M_CTL:
		case M_PROTO:
			if (!(saveq0->tmxl) || !(saveq0->tmxl->muxq)) {
				mp->b_datap->db_type = M_ERROR;
				*mp->b_rptr = EIO;
				qreply(q, mp);
				break;
			}
			qp = saveq0->tmxl->muxq;
			if (!canputnext(qp)) {
				(void) putbq(q, mp);
				return (0);
			}
			putnext(qp, mp);
			break;


		default:
#ifdef DEBUG
			if (logdmuxdebug)
				debug_enter(NULL);
#else
			(void) strlog(LOGDMX_ID, -1, 0, SL_ERROR,
			    "logdmuxuwsrv: got 0x%x msg type\n",
			    mp->b_datap->db_type);
#endif
		}
	}
	return (0);
}

/*
 * Logindmux lower put routine detects from which of the two lower queues
 * the data needs to be read from and writes it out to its partner queue.
 * For protocol, it detects M_CTL and sends its data to the daemon. Also,
 * for ioctl and other types of messages, it lets the daemon handle it.
 */
static int
logdmuxlrput(queue_t *q, mblk_t *mp)
{
	mblk_t	*savemp;
	queue_t *qp;
	struct	iocblk	*ioc;
	struct tmxl	*tmxl;
	int flushr;

	tmxl = (struct tmxl *)((WR(q))->q_ptr);

	if ((tmxl == NULL) || (tmxl->muxq == NULL) || (tmxl->peerq == NULL)) {
		freemsg(mp);
		return (0);
	}

	if ((tmxl->tmx == NULL) || (tmxl->tmx->state != TMXOPEN)) {
		freemsg(mp);
		return (0);
	}

	if ((q->q_first) && (mp->b_datap->db_type < QPCTL)) {
		(void) putq(q, mp);
		return (0);
	}

	switch (mp->b_datap->db_type) {

	case M_IOCTL:
		ioc = (struct iocblk *)mp->b_rptr;
		switch (ioc->ioc_cmd) {

		case TIOCSWINSZ:
		case TCSETAF:
		case TCSETSF:
		case TCSETA:
		case TCSETAW:
		case TCSETS:
		case TCSETSW:
		case TCSBRK:
		case TIOCSTI:
			qp = tmxl->peerq;
			break;

		default:
#ifdef DEBUG
			if (logdmuxdebug)
				debug_enter(NULL);
#else
			(void) strlog(LOGDMX_ID, -1, 0, SL_ERROR,
			    "logdmuxlrput: got ioctl type 0x%x\n",
			    ioc->ioc_cmd);
#endif
			freemsg(mp);
			return (0);
		}

		break;

	case M_DATA:
	case M_HANGUP:
		qp = tmxl->peerq;
		break;

	case M_CTL:
		qp = tmxl->tmx->rdq;
		if (!canputnext(qp)) {
			(void) putq(q, mp);
			return (0);
		}
		if (((mp->b_wptr - mp->b_rptr) == 1) &&
		    (*(mp->b_rptr) == M_CTL_MAGIC_NUMBER)) {
			savemp = mp->b_cont;
			freeb(mp);
			mp = savemp;
		}
		putnext(qp, mp);
		return (0);

	case M_IOCACK:
	case M_IOCNAK:
	case M_PROTO:
	case M_PCPROTO:
	case M_PCSIG:
	case M_SETOPTS:
		qp = tmxl->tmx->rdq;
		break;

	case M_ERROR:
		if (tmxl->tmx->flag) {
			/*
			 * This error is from ptm.  We could tell TCP to
			 * shutdown the connection, but it's easier to just
			 * wait for the daemon to get SIGCHLD and close from
			 * above.
			 */
			freemsg(mp);
			return (0);
		}
		/*
		 * This is from TCP.  Don't really know why we'ld
		 * get this, but we have a pretty good idea what
		 * to do:  Send M_HANGUP to the pty.
		 */
		mp->b_datap->db_type = M_HANGUP;
		mp->b_wptr = mp->b_rptr;
		qp = tmxl->peerq;
		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq_dataonly(q);
		}
		if (mp->b_flag & MSGMARK) {
			/*
			 * This M_FLUSH has been marked by the module
			 * below as intended for the upper queue,
			 * not the peer queue.
			 */
			qp = tmxl->tmx->rdq;
			mp->b_flag &= ~MSGMARK;
		} else {
			/*
			 * Wrap this M_FLUSH through the mux.
			 * The FLUSHR and FLUSHW bits must be
			 * reversed.
			 */
			qp = tmxl->peerq;
			flushr = *mp->b_rptr & FLUSHR;
			if (*mp->b_rptr & FLUSHW) {
				*mp->b_rptr |= FLUSHR;
			} else {
				*mp->b_rptr &= ~FLUSHR;
			}
			if (flushr & FLUSHR) {
				*mp->b_rptr |= FLUSHW;
			} else {
				*mp->b_rptr &= ~FLUSHW;
			}
		}
		break;

	case M_START:
	case M_STOP:
	case M_STARTI:
	case M_STOPI:
		freemsg(mp);
		return (0);

	default:
#ifdef DEBUG
		if (logdmuxdebug)
			debug_enter(NULL);
#else
		(void) strlog(LOGDMX_ID, -1, 0, SL_ERROR,
		    "logdmuxlrput: obtained 0x%x msg type\n",
		    mp->b_datap->db_type);
#endif
		freemsg(mp);
		return (0);
	}
	if (!canputnext(qp) && (mp->b_datap->db_type < QPCTL)) {
		(void) putq(q, mp);
		return (0);
	}
	putnext(qp, mp);
	return (0);
}



/*
 * Lower read service routine
 */
static int
logdmuxlrsrv(queue_t *q)
{
	mblk_t  *mp, *savemp;
	queue_t *qp;
	struct iocblk *ioc;
	struct	tmxl	*tmxl;

	tmxl = (struct tmxl *)((WR(q))->q_ptr);

	while ((mp = getq(q)) != NULL) {

		if ((tmxl == NULL) || (tmxl->muxq == NULL) ||
		    (tmxl->peerq == NULL)) {
			freemsg(mp);
			continue;
		}

		if ((tmxl->tmx == NULL) ||
		    (tmxl->tmx->state != TMXOPEN)) {
			freemsg(mp);
			continue;
		}

		switch (mp->b_datap->db_type) {

		case M_IOCTL:
			ioc = (struct iocblk *)mp->b_rptr;

			switch (ioc->ioc_cmd) {

			case TIOCSWINSZ:
			case TCSETAF:
			case TCSETSF:
			case TCSETA:
			case TCSETAW:
			case TCSETS:
			case TCSETSW:
			case TCSBRK:
			case TIOCSTI:
				qp = tmxl->peerq;
				break;

			default:
#ifdef DEBUG
				if (logdmuxdebug)
					debug_enter(NULL);
#else
				(void) strlog(LOGDMX_ID, -1, 0, SL_ERROR,
				    "logdmuxlrsrv: got ioctl"
				    " type 0x%x\n", ioc->ioc_cmd);
#endif
				freemsg(mp);
				continue;
			}
			break;

		case M_DATA:
		case M_HANGUP:
			qp = tmxl->peerq;
			break;

		case M_CTL:
			qp = tmxl->tmx->rdq;
			if (!canputnext(qp)) {
				(void) putbq(q, mp);
				return (0);
			}
			if (((mp->b_wptr - mp->b_rptr) == 1) &&
				(*(mp->b_rptr) == M_CTL_MAGIC_NUMBER)) {
				savemp = mp->b_cont;
				freeb(mp);
				mp = savemp;
			}
			putnext(qp, mp);
			continue;

		case M_PROTO:
		case M_SETOPTS:
			qp = tmxl->tmx->rdq;
			break;

		default:
#ifdef DEBUG
			if (logdmuxdebug)
				debug_enter(NULL);
#else
			(void) strlog(LOGDMX_ID, -1, 0, SL_ERROR,
			    "logdmuxlrsrv: obtained 0x%x msg type\n",
			    mp->b_datap->db_type);
#endif
			freemsg(mp);
			continue;
		}
		ASSERT(mp->b_datap->db_type < QPCTL);
		if (!canputnext(qp)) {
			(void) putbq(q, mp);
			return (0);
		}
		putnext(qp, mp);
	}
	return (0);
}

/*
 * Lower side write service procedure.  No messages are ever placed on
 * the write queue here, this just back-enables all of the upper side
 * write service procedures.
 */
static int
logdmuxlwsrv(queue_t *q)
{
	struct tmxl	*tmxl;

	tmxl = (struct tmxl *)(q->q_ptr);

	/*
	 * Qenable upper write queue and find out which lower
	 * queue needs to be restarted with flow control.
	 * Qenable the partner queue so canputnext will
	 * succeed on next call to logdmuxlrput.
	 */

	if (tmxl && tmxl->tmx)
		qenable(WR(tmxl->tmx->rdq));

	if (tmxl && tmxl->peerq)
		qenable(RD(tmxl->peerq));

	return (0);
}

/*
 * This routine does I_LINK operation.
 * The routine gets called from qwriter().
 */

static void
logdmuxlink(queue_t *q, mblk_t *mp)
{
	struct	linkblk *lp;
	struct	tmx	*tmxp;

	tmxp = (struct tmx *)q->q_ptr;
	lp = (struct linkblk *)mp->b_cont->b_rptr;

	tmxp->tmxl->muxq = lp->l_qbot;
	tmxp->tmxl->muxid = lp->l_index;
	tmxp->tmxl->tmx = tmxp;
	(tmxp->tmxl->muxq)->q_ptr = tmxp->tmxl;

	logdmux_sndack(q, mp, NULL, 0);
	qenable(q);
}

/*
 * This routine does I_UNLINK operation.
 * This routine gets called from qwriter().
 */
static void
logdmuxunlink(queue_t *q, mblk_t *mp)
{
	struct	tmx	*tmxp;
	struct	tmx	*tmxpeerp = 0;

	tmxp = (struct tmx *)q->q_ptr;

	if (tmxp->tmxl->peerq)
		tmxpeerp = tmxp->pair;
	tmxp->tmxl->peerq = 0;

	(tmxp->tmxl->muxq)->q_ptr = 0;

	/*
	 * enable lower read q just in case it is blocked on peer flow
	 * control, because from here on out there won't be a peer thump
	 * to restart.
	 */
	qenable(RD(tmxp->tmxl->muxq));
	tmxp->tmxl->muxq = 0;
	tmxp->tmxl->muxid = 0;
	if (tmxpeerp && tmxpeerp->tmxl)
		tmxpeerp->tmxl->peerq = 0;
	tmxp->tmxl->tmx = 0;

	kmem_free((char *)tmxp->tmxl, sizeof (struct tmxl));
	tmxp->tmxl = NULL;

	logdmux_sndack(q, mp, NULL, 0);
	qenable(q);
}

/*
 * Send a negative acknowledgement for the ioctl denoted by mp through the
 * queue q, specifying the error code err.
 * This routine could be a macro or in-lined, except that space is more
 * critical than time in error cases.
 */
static void
logdmux_sndnak(queue_t *q, mblk_t *mp, int err)
{
	struct iocblk  *iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCNAK;
	iocp->ioc_count = 0;
	iocp->ioc_error = err;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}
	qreply(q, mp);
}

/*
 * Convert the M_IOCTL or M_IOCDATA mesage denoted by mp into an M_IOCACK.
 * Free any data associated with the message and replace it with dp if dp is
 * non-NULL, adjusting dp's write pointer to match size.
 */
static void
logdmux_sndack(queue_t *q, mblk_t *mp, mblk_t *dp, size_t size)
{
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_count = size;
	iocp->ioc_error = 0;
	iocp->ioc_rval = 0;
	if (mp->b_cont != NULL)
		freemsg(mp->b_cont);
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else {
		mp->b_cont = NULL;
	}
	qreply(q, mp);
}

static void
logdmux_timer(void *arg)
{
	queue_t *q = arg;
	struct	tmx	*saveq = (struct tmx *)q->q_ptr;

	ASSERT(saveq);

	if (q->q_flag & QREADR) {
		ASSERT(saveq->rtimoutid);
		saveq->rtimoutid = 0;
	} else {
		ASSERT(saveq->wtimoutid);
		saveq->wtimoutid = 0;
	}
	enableok(q);
	qenable(q);
}

static void
logdmux_buffer(void *arg)
{
	queue_t *q = arg;
	struct	tmx	*saveq = (struct tmx *)q->q_ptr;

	ASSERT(saveq);

	if (q->q_flag & QREADR) {
		ASSERT(saveq->rbufcid);
		saveq->rbufcid = 0;
	} else {
		ASSERT(saveq->wbufcid);
		saveq->wbufcid = 0;
	}
	enableok(q);
	qenable(q);
}

static void
recover(queue_t *q, mblk_t *mp, size_t size)
{
	timeout_id_t	tid;
	bufcall_id_t	bid;
	struct	tmx	*saveq = (struct tmx *)q->q_ptr;

	/*
	 * Avoid re-enabling the queue.
	 */
	ASSERT(mp->b_datap->db_type < QPCTL);
	ASSERT(WR(q)->q_next == NULL); /* Called from upper queue only */
	noenable(q);
	(void) putbq(q, mp);

	/*
	 * Make sure there is at most one outstanding request per queue.
	 */
	if (q->q_flag & QREADR) {
		if (saveq->rtimoutid || saveq->rbufcid)
			return;
	} else {
		if (saveq->wtimoutid || saveq->wbufcid)
			return;
	}
	if (!(bid = qbufcall(RD(q), size, BPRI_MED, logdmux_buffer, q))) {
		tid = qtimeout(RD(q), logdmux_timer, q, SIMWAIT);
		if (q->q_flag & QREADR)
			saveq->rtimoutid = tid;
		else
			saveq->wtimoutid = tid;
	} else	{
		if (q->q_flag & QREADR)
			saveq->rbufcid = bid;
		else
			saveq->wbufcid = bid;
	}
}

static void
flushq_dataonly(queue_t *q)
{
	mblk_t *mp, *nmp;

	/*
	 * Since we are already in the perimeter, and we are not a put-shared
	 * perimeter, we don't need to freeze the stream or anything to
	 * be insured of exclusivity.
	 */
	mp = q->q_first;
	while (mp) {
		if (mp->b_datap->db_type == M_DATA) {
			nmp = mp->b_next;
			rmvq(q, mp);
			freemsg(mp);
			mp = nmp;
		} else {
			mp = mp->b_next;
		}
	}
}
