/*
 * Copyright (c) 1987,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consms.c	5.42	99/09/13 SMI"

/*
 * Console mouse driver for Sun.
 * The console "zs" port is linked under us, with the "ms" module pushed
 * on top of it.
 *
 * We don't do any real multiplexing here; this device merely provides a
 * way to have "/dev/mouse" automatically have the "ms" module present.
 * Due to problems with the way the "specfs" file system works, you can't
 * use an indirect device (a "stat" on "/dev/mouse" won't get the right
 * snode, so you won't get the right time of last access), and due to
 * problems with the kernel window system code, you can't use a "cons"-like
 * driver ("/dev/mouse" won't be a streams device, even though operations
 * on it get turned into operations on the real stream).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/consdev.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

static int	consmsopen();
static int	consmsclose();
static void	consmsuwput();
static void	consmslrput();
static void	consmslwserv();

static struct module_info consmsm_info = {
	0,
	"consms",
	0,
	1024,
	2048,
	128
};

static struct qinit consmsurinit = {
	putq,
	(int (*)())NULL,
	consmsopen,
	consmsclose,
	(int (*)())NULL,
	&consmsm_info,
	NULL
};

static struct qinit consmsuwinit = {
	(int (*)())consmsuwput,
	(int (*)())NULL,
	consmsopen,
	consmsclose,
	(int (*)())NULL,
	&consmsm_info,
	NULL
};

static struct qinit consmslrinit = {
	(int (*)())consmslrput,
	(int (*)())NULL,
	(int (*)())NULL,
	(int (*)())NULL,
	(int (*)())NULL,
	&consmsm_info,
	NULL
};

static struct qinit consmslwinit = {
	putq,
	(int (*)())consmslwserv,
	(int (*)())NULL,
	(int (*)())NULL,
	(int (*)())NULL,
	&consmsm_info,
	NULL
};

static struct streamtab consms_str_info = {
	&consmsurinit,
	&consmsuwinit,
	&consmslrinit,
	&consmslwinit,
};

static void consmsioctl(queue_t *q, mblk_t *mp);

static int consms_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int consms_identify(dev_info_t *devi);
static int consms_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int consms_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int consms_power(dev_info_t *devi, int cmpt, int level);

/*
 * Module global data are protected by the per-module inner perimeter.
 */
static queue_t	*upperqueue;	/* regular keyboard queue above us */
static queue_t	*lowerqueue;	/* queue below us */

static dev_info_t *consms_dip;		/* private copy of devinfo pointer */
static uchar_t	consms_suspended;	/* True, if suspended */

static 	struct cb_ops cb_consms_ops = {
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
	&consms_str_info,	/* cb_stream */
	D_NEW|D_MP|D_MTPERMOD	/* cb_flag */
};

static struct dev_ops consms_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	consms_info,		/* devo_getinfo */
	consms_identify,	/* devo_identify */
	nulldev,		/* devo_probe */
	consms_attach,		/* devo_attach */
	consms_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&(cb_consms_ops),	/* devo_cb_ops */
	(struct bus_ops *)NULL,	/* devo_bus_ops */
	consms_power		/* devo_power */
};


/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Mouse Driver for Sun 'consms'",
	&consms_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};


/*
 * Single private "global" lock for the few rare conditions
 * we want single-threaded.
 */
static	kmutex_t	consmslock;

int
_init(void)
{
	int	error;

	mutex_init(&consmslock, NULL, MUTEX_DRIVER, NULL);
	error = mod_install(&modlinkage);
	if (error != 0) {
		mutex_destroy(&consmslock);
	}
	return (error);
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);
	mutex_destroy(&consmslock);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
consms_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "consms") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static char *pmcomp[] = {
	"NAME=mouse",
	"0=off",
	"1=on"
};

static int
consms_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "mouse", S_IFCHR,
		0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}
	consms_dip = devi;

	if (ddi_prop_update_string_array(DDI_DEV_T_NONE, devi, "pm-components",
	    &pmcomp[0], 3) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "consms: Can't create component definition "
		    "for %s%s\n", ddi_binding_name(devi),
		    ddi_get_name_addr(devi));
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
consms_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
consms_power(dev_info_t *devi, int cmpt, int level)
{
	/*
	 * This driver implements dummy power levels (that have no effect)
	 * so that the frame buffer can depend on it
	 */
	if (cmpt != 0 || 0 > level || level > 1)
		return (DDI_FAILURE);
	if (level == 0)
		consms_suspended = 1;
	else
		consms_suspended = 0;
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
consms_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (consms_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *) consms_dip;
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


/*ARGSUSED*/
static int
consmsopen(q, devp, flag, sflag, crp)
	queue_t *q;
	dev_t	*devp;
	int	flag, sflag;
	cred_t	*crp;
{
	upperqueue = q;
	qprocson(q);
	return (0);
}

/*ARGSUSED*/
static int
consmsclose(q, flag, crp)
	queue_t *q;
	int	flag;
	cred_t	*crp;
{
	qprocsoff(q);
	upperqueue = NULL;
	return (0);
}

/*
 * Put procedure for upper write queue.
 */
static void
consmsuwput(q, mp)
	register queue_t *q;
	register mblk_t *mp;
{
	switch (mp->b_datap->db_type) {

	case M_IOCTL:
		consmsioctl(q, mp);
		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq(q, FLUSHDATA);
		if (*mp->b_rptr & FLUSHR)
			flushq(RD(q), FLUSHDATA);
		if (lowerqueue != NULL)
			(void) putq(lowerqueue, mp);	/* pass it through */
		else {
			/*
			 * No lower queue; just reflect this back upstream.
			 */
			*mp->b_rptr &= ~FLUSHW;
			if (*mp->b_rptr & FLUSHR)
				qreply(q, mp);
			else
				freemsg(mp);
		}
		break;

	case M_DATA:
		if (lowerqueue == NULL)
			goto bad;
		(void) putq(lowerqueue, mp);
		break;

	default:
	bad:
		/*
		 * Pass an error message up.
		 */
		mp->b_datap->db_type = M_ERROR;
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
		}
		mp->b_rptr = mp->b_datap->db_base;
		mp->b_wptr = mp->b_rptr + sizeof (char);
		*mp->b_rptr = EINVAL;
		qreply(q, mp);
	}
}

static void
consmsioctl(q, mp)
	register queue_t *q;
	register mblk_t *mp;
{
	register struct iocblk *iocp;
	register struct linkblk *linkp;

	iocp = (struct iocblk *)mp->b_rptr;

	switch (iocp->ioc_cmd) {

	case I_LINK:	/* stupid, but permitted */
	case I_PLINK:
		if (lowerqueue != NULL) {
			iocp->ioc_error = EINVAL;	/* XXX */
			goto iocnak;
		}
		mutex_enter(&consmslock);
		linkp = (struct linkblk *)mp->b_cont->b_rptr;
		lowerqueue = linkp->l_qbot;
		mutex_exit(&consmslock);
		iocp->ioc_count = 0;
		break;

	case I_UNLINK:	/* stupid, but permitted */
	case I_PUNLINK:
		mutex_enter(&consmslock);
		linkp = (struct linkblk *)mp->b_cont->b_rptr;
		if (lowerqueue != linkp->l_qbot) {
			iocp->ioc_error = EINVAL;	/* XXX */
			mutex_exit(&consmslock);
			goto iocnak;	/* not us */
		}
		lowerqueue = NULL;
		mutex_exit(&consmslock);
		iocp->ioc_count = 0;
		break;

#ifdef notdef
	case TIOCGETD: {
		/*
		 * Pretend it's the mouse line discipline; what else
		 * could it be?
		 */
		register mblk_t *datap;

		if ((datap = allocb(sizeof (int), BPRI_MED)) == NULL) {
			iocp->ioc_erroro = ENOMEM;
			goto iocnak;
		}
		*(int *)datap->b_wptr = MOUSELDISC;
		datap->b_wptr += (sizeof (int)/sizeof *datap->b_wptr);
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
		}
		mp->b_cont = datap;
		iocp->ioc_count = sizeof (int);
		break;
	}
	case TIOCSETD:
		/*
		 * Unless they're trying to set it to the mouse line
		 * discipline, reject this call.
		 */
		if (*(int *)mp->b_cont->b_rptr != MOUSELDISC) {
			iocp->ioc_error = EINVAL;
			goto iocnak;
		}
		iocp->ioc_count = 0;
		break;
#endif notdef

	default:
		/*
		 * Pass this through, if there's something to pass it
		 * through to; otherwise, reject it.
		 */
		if (lowerqueue != NULL) {
			(void) putq(lowerqueue, mp);
			return;
		}
		iocp->ioc_error = EINVAL;
		goto iocnak;	/* nobody below us; reject it */
	}

	/*
	 * Common exit path for calls that return a positive
	 * acknowledgment with a return value of 0.
	 */
	iocp->ioc_rval = 0;
	iocp->ioc_error = 0;	/* brain rot */
	mp->b_datap->db_type = M_IOCACK;
	qreply(q, mp);
	return;

iocnak:
	iocp->ioc_rval = 0;
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);
}

/*
 * Service procedure for lower write queue.
 * Puts things on the queue below us, if it lets us.
 */
static void
consmslwserv(q)
	register queue_t *q;
{
	register mblk_t *mp;

	while (canput(q->q_next) && (mp = getq(q)) != NULL)
		putnext(q, mp);
}

/*
 * Put procedure for lower read queue.
 */
static void
consmslrput(q, mp)
	register queue_t *q;
	register mblk_t *mp;
{
	if (consms_suspended)
		(void) ddi_dev_is_needed(consms_dip, 0, 1);
	(void) pm_idle_component(consms_dip, 0);

	switch (mp->b_datap->db_type) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq(WR(q), FLUSHDATA);
		if (*mp->b_rptr & FLUSHR)
			flushq(q, FLUSHDATA);
		if (upperqueue != NULL)
			putnext(upperqueue, mp);	/* pass it through */
		else {
			/*
			 * No upper queue; just reflect this back downstream.
			 */
			*mp->b_rptr &= ~FLUSHR;
			if (*mp->b_rptr & FLUSHW)
				qreply(q, mp);
			else
				freemsg(mp);
		}
		break;

	case M_ERROR:
	case M_HANGUP:
		/* XXX - should we tell the upper queue(s) about this? */
		freemsg(mp);
		break;

	case M_DATA:
	case M_IOCACK:
	case M_IOCNAK:
		if (upperqueue != NULL)
			putnext(upperqueue, mp);
		else
			freemsg(mp);
		break;

	default:
		freemsg(mp);	/* anything useful here? */
		break;
	}
}
