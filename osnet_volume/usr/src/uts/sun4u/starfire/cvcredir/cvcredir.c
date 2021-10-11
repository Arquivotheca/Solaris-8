/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cvcredir.c	1.9	98/11/25 SMI"

/*
 * MT STREAMS Virtual Console Redirection Device Driver
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/debug.h>
#include <sys/thread.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/tty.h>
#include <sys/cvc.h>
#include <sys/conf.h>
#include <sys/modctl.h>


/*
 * Routine to to register/unregister our queue for console output and pass
 * redirected data to the console.  The cvc driver will do a putnext using
 * our queue, so we will not see the redirected console data.
 */
extern int	cvc_redir(mblk_t *);
extern int	cvc_register(queue_t *);
extern int	cvc_unregister(queue_t *);

static int	cvcr_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	cvcr_identify(dev_info_t *);
static int	cvcr_attach(dev_info_t *, ddi_attach_cmd_t);
static int	cvcr_detach(dev_info_t *, ddi_detach_cmd_t);
static int	cvcr_wput(queue_t *, mblk_t *);
static int	cvcr_open(queue_t *, dev_t *, int, int, cred_t *);
static int	cvcr_close(queue_t *, int, cred_t *);
static void	cvcr_ioctl(queue_t *, mblk_t *);
static void	cvcr_ack(mblk_t *, mblk_t *, uint);

static dev_info_t	*cvcr_dip;
static int		cvcr_suspend = 0;

static struct module_info minfo = {
	1314,		/* mi_idnum Bad luck number +1  ;-) */
	"cvcredir",	/* mi_idname */
	0,		/* mi_minpsz */
	INFPSZ,		/* mi_maxpsz */
	2048,		/* mi_hiwat */
	2048		/* mi_lowat */
};

static struct qinit	cvcr_rinit = {
	NULL,		/* qi_putp */
	NULL,		/* qi_srvp */
	cvcr_open,	/* qi_qopen */
	cvcr_close,	/* qi_qclose */
	NULL,		/* qi_qadmin */
	&minfo,		/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	cvcr_winit = {
	cvcr_wput,	/* qi_putp */
	NULL,		/* qi_srvp */
	cvcr_open,	/* qi_qopen */
	cvcr_close,	/* qi_qclose */
	NULL,		/* qi_qadmin */
	&minfo,		/* qi_minfo */
	NULL		/* qi_mstat */
};

struct streamtab	cvcrinfo = {
	&cvcr_rinit,	/* st_rdinit */
	&cvcr_winit,	/* st_wrinit */
	NULL,		/* st_muxrinit */
	NULL		/* st_muxwrinit */
};

DDI_DEFINE_STREAM_OPS(cvcrops, cvcr_identify, nulldev, cvcr_attach, \
			cvcr_detach, nodev, cvcr_info, (D_NEW|D_MTPERQ|D_MP), \
			&cvcrinfo);

extern nodev(), nulldev();
extern struct mod_ops mod_driverops;

char _depends_on[] = "drv/cvc";

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"CVC redirect driver 'cvcredir' v1.9",
	&cvcrops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};


_init()
{
	return (mod_install(&modlinkage));
}
_fini()
{
	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

static int
cvcr_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "cvcredir") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/*ARGSUSED*/
static int
cvcr_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
#ifdef lint
	cvcr_suspend = cvcr_suspend;
#endif
	if (cmd == DDI_RESUME) {
		cvcr_suspend = 0;
	} else {
		if (ddi_create_minor_node(devi, "cvcredir", S_IFCHR,
		    0, NULL, NULL) == DDI_FAILURE) {
			ddi_remove_minor_node(devi, NULL);
			return (-1);
		}
		cvcr_dip = devi;
	}
	return (DDI_SUCCESS);
}

static int
cvcr_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd == DDI_SUSPEND) {
		cvcr_suspend = 1;
	} else {
		if (cmd != DDI_DETACH) {
			return (DDI_FAILURE);
		}
		ddi_remove_minor_node(dip, NULL);
	}
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
cvcr_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (cvcr_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *)cvcr_dip;
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

/* ARGSUSED */
static int
cvcr_open(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *cred)
{
	WR(q)->q_ptr = q->q_ptr = (char *)2;
	/*
	 * call into the cvc driver to register our queue.  cvc will use
	 * our queue to send console output data upstream (our stream)to
	 * cvcd which has us open and is reading console data.
	 */
	if (cvc_register(RD(q)) == -1) {
		cmn_err(CE_WARN, "cvcr_open: cvc_register failed for q = 0x%x",
			q);
	}
	return (0);
}

/* ARGSUSED */
static int
cvcr_close(queue_t *q, int flag, cred_t *cred)
{
	/*
	 * call into the cvc driver to un-register our queue.  cvc will
	 * no longer use our queue to send console output data upstream.
	 */
	cvc_unregister(RD(q));
	WR(q)->q_ptr = q->q_ptr = NULL;
	return (0);
}

static int
cvcr_wput(queue_t *q, mblk_t *mp)
{
	/*
	 * Handle BREAK key for debugger and TIOCSWINSZ.
	 */
	if (mp->b_datap->db_type == M_IOCTL) {
		cvcr_ioctl(q, mp);
		return (0);
	}
	/*
	 * Call into the cvc driver to put console input data on
	 * its upstream queue to be picked up by the console driver.
	 */
	if (!cvc_redir(mp))
		freemsg(mp);
	return (0);
}

static void
cvcr_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;

	if (iocp->ioc_cmd == CVC_BREAK) {
		abort_sequence_enter((char *)NULL);
		cvcr_ack(mp, NULL, 0);
	} else if (iocp->ioc_cmd == CVC_DISCONNECT ||
		iocp->ioc_cmd == TIOCSWINSZ) {
		/*
		 * Generate a SIGHUP or SIGWINCH to the console.  Note in this
		 * case cvc_redir does not free up mp, so we can reuse for
		 * IOCACK.
		 */
		(void) cvc_redir(mp);
		cvcr_ack(mp, NULL, 0);
	} else {
		iocp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
	}
	qreply(q, mp);
}

static void
cvcr_ack(mblk_t *mp, mblk_t *dp, uint size)
{
	register struct iocblk  *iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_count = size;
	iocp->ioc_error = 0;
	iocp->ioc_rval = 0;
	if (mp->b_cont != NULL)
		freeb(mp->b_cont);
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else
		mp->b_cont = NULL;
}
