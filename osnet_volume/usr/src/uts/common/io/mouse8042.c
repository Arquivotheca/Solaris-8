/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mouse8042.c	1.36	99/05/04 SMI"

/*
 * PS/2 type Mouse Module - Streams
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/termio.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strtty.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/stat.h>
#include <sys/cmn_err.h>
#include <sys/sunddi.h>

#include <sys/promif.h>
#include <sys/cred.h>

#include <sys/i8042.h>
#include <sys/note.h>

#define	DRIVER_NAME(dip)	ddi_major_to_name(ddi_name_to_major(	\
					ddi_get_name(dip)))

#ifdef	DEBUG
#define	MOUSE8042_DEBUG
#endif

/*
 *
 * Local Static Data
 *
 */

/*
 * We only support one instance.  Yes, it's theoretically possible to
 * plug in more than one, but it's not worth the implementation cost.
 *
 * The introduction of USB keyboards might make it worth reassessing
 * this decision, as they might free up the keyboard port for a second
 * PS/2 style mouse.
 */
static dev_info_t *mouse8042_dip;

struct mouse_state {
	queue_t	*rqp;
	queue_t	*wqp;
	ddi_iblock_cookie_t	iblock_cookie;
	ddi_acc_handle_t	handle;
	uint8_t			*addr;
	kmutex_t		mutex;
};

#if	defined(MOUSE8042_DEBUG)
int mouse8042_debug = 0;
int mouse8042_debug_minimal = 0;
#endif

static uint_t mouse8042_intr(caddr_t arg);
static int mouse8042_open(queue_t *q, dev_t *devp, int flag, int sflag,
		cred_t *cred_p);
static int mouse8042_close(queue_t *q, int flag, cred_t *cred_p);
static int mouse8042_wput(queue_t *q, mblk_t *mp);

static int mouse8042_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd,
		void *arg, void **result);
static int mouse8042_attach(dev_info_t *dev, ddi_attach_cmd_t cmd);
static int mouse8042_detach(dev_info_t *dev, ddi_detach_cmd_t cmd);


/*
 * Streams module info.
 */
#define	MODULE_NAME	"mouse8042"

static struct module_info	mouse8042_minfo = {
	23,		/* Module ID number */
	MODULE_NAME,
	0, INFPSZ,	/* minimum & maximum packet sizes */
	256, 128	/* hi and low water marks */
};

static struct qinit mouse8042_rinit = {
	NULL,		/* put */
	NULL,		/* service */
	mouse8042_open,
	mouse8042_close,
	NULL,		/* admin */
	&mouse8042_minfo,
	NULL		/* statistics */
};

static struct qinit mouse8042_winit = {
	mouse8042_wput,	/* put */
	NULL,		/* service */
	NULL,		/* open */
	NULL,		/* close */
	NULL,		/* admin */
	&mouse8042_minfo,
	NULL		/* statistics */
};

static struct streamtab mouse8042_strinfo = {
	&mouse8042_rinit,
	&mouse8042_winit,
	NULL,		/* muxrinit */
	NULL,		/* muxwinit */
};

/*
 * Local Function Declarations
 */

static struct cb_ops	mouse8042_cb_ops = {
	nodev,			/* open */
	nodev,			/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	&mouse8042_strinfo,	/* streamtab  */
	D_MP | D_NEW
};


static struct dev_ops	mouse8042_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	mouse8042_getinfo,	/* getinfo */
	nulldev,		/* identify */
	nulldev,		/* probe */
	mouse8042_attach,	/* attach */
	mouse8042_detach,	/* detach */
	nodev,			/* reset */
	&mouse8042_cb_ops,	/* driver operations */
	(struct bus_ops *)0	/* bus operations */
};

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a driver */
	"PS/2 Mouse 1.36, 99/05/04",
	&mouse8042_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

/*
 * This is the driver initialization routine.
 */
int
_init()
{
	int	rv;

	rv = mod_install(&modlinkage);
	return (rv);
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
mouse8042_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	char	*propname, *propval;
	struct mouse_state *state;
	static ddi_device_acc_attr_t attr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};
	int rc;

#ifdef MOUSE8042_DEBUG
	if (mouse8042_debug) {
		cmn_err(CE_CONT, MODULE_NAME "_attach entry\n");
	}
#endif

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	if (mouse8042_dip != NULL)
		return (DDI_FAILURE);

	/* allocate and initialize state structure */
	state = kmem_zalloc(sizeof (struct mouse_state), KM_SLEEP);
	ddi_set_driver_private(dip, (caddr_t)state);

	rc = ddi_create_minor_node(dip, "l", S_IFCHR, ddi_get_instance(dip),
	    DDI_NT_MOUSE, NULL);
	if (rc != DDI_SUCCESS) {
#if	defined(MOUSE8042_DEBUG)
		cmn_err(CE_CONT,
		    MODULE_NAME "_attach: ddi_create_minor_node failed\n");
#endif
		goto fail_1;
	}

	rc = ddi_regs_map_setup(dip, 0, (caddr_t *)&state->addr,
		(offset_t)0, (offset_t)0, &attr, &state->handle);
	if (rc != DDI_SUCCESS) {
#if	defined(MOUSE8042_DEBUG)
		cmn_err(CE_WARN, MODULE_NAME "_attach:  can't map registers");
#endif
		goto fail_2;
	}

	rc = ddi_get_iblock_cookie(dip, 0, &state->iblock_cookie);
	if (rc != DDI_SUCCESS) {
#if	defined(MOUSE8042_DEBUG)
		cmn_err(CE_WARN,
		    MODULE_NAME "_attach:  Can't get iblock cookie");
#endif
		goto fail_3;
	}

	mutex_init(&state->mutex, NULL, MUTEX_DRIVER, state->iblock_cookie);

	rc = ddi_add_intr(dip, 0,
		(ddi_iblock_cookie_t *)NULL, (ddi_idevice_cookie_t *)NULL,
		mouse8042_intr, (caddr_t)state);
	if (rc != DDI_SUCCESS) {
#if	defined(MOUSE8042_DEBUG)
		cmn_err(CE_WARN, MODULE_NAME "_attach: cannot add interrupt");
#endif
		goto fail_3;
	}

	/*
	 * streams-module-to-push is used by some SPARC versions of
	 * consconfig.
	 */
	propname = "streams-module-to-push";
	if (! ddi_prop_exists(DDI_DEV_T_ANY, dip,
			DDI_PROP_DONTPASS | DDI_PROP_NOTPROM, propname)) {
		propval = "vuidps2";
		if (ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
		    propname, propval, strlen(propval) + 1) !=
		    DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN, MODULE_NAME "_attach: "
			    "Can't create property \"%s\"=\"%s\"\n",
			    propname, propval);
		}
	}

	mouse8042_dip = dip;

	/* Now that we're attached, announce our presence to the world. */
	ddi_report_dev(dip);
#if	defined(MOUSE8042_DEBUG)
	cmn_err(CE_CONT, "?%s #%d: version %s, compiled on %s, %s\n",
			DRIVER_NAME(dip), ddi_get_instance(dip),
			"1.36 (99/05/04)", __DATE__, __TIME__);
#endif
	return (DDI_SUCCESS);

fail_3:
	ddi_regs_map_free(&state->handle);

fail_2:
	ddi_remove_minor_node(dip, NULL);

fail_1:
	return (rc);
}

/*ARGSUSED*/
static int
mouse8042_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct mouse_state *state;

	state = (struct mouse_state *)ddi_get_driver_private(dip);

	switch (cmd) {

	case DDI_DETACH:
		ddi_remove_intr(dip, 0, state->iblock_cookie);
		mouse8042_dip = NULL;
		mutex_destroy(&state->mutex);
		ddi_prop_remove_all(dip);
		ddi_remove_minor_node(dip, NULL);
		kmem_free((void *)state, sizeof (struct mouse_state));
		return (DDI_SUCCESS);

	default:
#ifdef MOUSE8042_DEBUG
		if (mouse8042_debug) {
			cmn_err(CE_CONT,
			    "mouse8042_detach: cmd = %d unknown\n", cmd);
		}
#endif
		return (DDI_FAILURE);
	}
}


/* ARGSUSED */
static int
mouse8042_getinfo(
    dev_info_t *dip,
    ddi_info_cmd_t infocmd,
    void *arg,
    void **result)
{
	dev_t dev = (dev_t)arg;

#ifdef MOUSE8042_DEBUG
	if (mouse8042_debug)
		cmn_err(CE_CONT, "mouse8042_getinfo: call\n");
#endif
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (mouse8042_dip == NULL)
			return (DDI_FAILURE);

		*result = (void *)mouse8042_dip;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor(dev);
		break;
	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
mouse8042_open(
	queue_t	*q,
	dev_t	*devp,
	int	flag,
	int	sflag,
	cred_t	*cred_p)
{
	struct mouse_state *state;

	if (mouse8042_dip == NULL)
		return (ENXIO);

	state = (struct mouse_state *)ddi_get_driver_private(mouse8042_dip);

#ifdef MOUSE8042_DEBUG
	if (mouse8042_debug)
		cmn_err(CE_CONT, "mouse8042_open:entered\n");
#endif

	if (q->q_ptr != NULL)
		return (0);

	mutex_enter(&state->mutex);

	q->q_ptr = (caddr_t)state;
	WR(q)->q_ptr = (caddr_t)state;
	state->rqp = q;
	state->wqp = WR(q);

	qprocson(q);

	mutex_exit(&state->mutex);

	return (0);
}


/*ARGSUSED*/
static int
mouse8042_close(queue_t *q, int flag, cred_t *cred_p)
{
	struct mouse_state *state;

	state = (struct mouse_state *)q->q_ptr;

#ifdef MOUSE8042_DEBUG
	if (mouse8042_debug)
		cmn_err(CE_CONT, "mouse8042_close:entered\n");
#endif

	mutex_enter(&state->mutex);

	qprocsoff(q);

	q->q_ptr = NULL;
	WR(q)->q_ptr = NULL;
	state->rqp = NULL;
	state->wqp = NULL;

	mutex_exit(&state->mutex);

	return (0);
}

static void
mouse8042_iocnack(
    queue_t *qp,
    mblk_t *mp,
    struct iocblk *iocp,
    int error,
    int rval)
{
	mp->b_datap->db_type = M_IOCNAK;
	iocp->ioc_rval = rval;
	iocp->ioc_error = error;
	qreply(qp, mp);
}

static int
mouse8042_wput(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocbp;
	mblk_t *bp;
	mblk_t *next;
	struct mouse_state *state;

	state = (struct mouse_state *)q->q_ptr;

#ifdef MOUSE8042_DEBUG
	if (mouse8042_debug)
		cmn_err(CE_CONT, "mouse8042_wput:entered\n");
#endif
	iocbp = (struct iocblk *)mp->b_rptr;
	switch (mp->b_datap->db_type) {
	case M_FLUSH:
#ifdef MOUSE8042_DEBUG
		if (mouse8042_debug)
			cmn_err(CE_CONT, "mouse8042_wput:M_FLUSH\n");
#endif

		if (*mp->b_rptr & FLUSHW)
			flushq(q, FLUSHDATA);
		qreply(q, mp);
		break;
	case M_IOCTL:
#ifdef MOUSE8042_DEBUG
		if (mouse8042_debug)
			cmn_err(CE_CONT, "mouse8042_wput:M_IOCTL\n");
#endif
		mouse8042_iocnack(q, mp, iocbp, EINVAL, 0);
		break;
	case M_IOCDATA:
#ifdef MOUSE8042_DEBUG
		if (mouse8042_debug)
			cmn_err(CE_CONT, "mouse8042_wput:M_IOCDATA\n");
#endif
		mouse8042_iocnack(q, mp, iocbp, EINVAL, 0);
		break;
	case M_DATA:
		bp = mp;
		do {
			while (bp->b_rptr < bp->b_wptr) {
#if	defined(MOUSE8042_DEBUG)
				if (mouse8042_debug) {
					cmn_err(CE_CONT,
					    "mouse8042:  send %2x\n",
					    *bp->b_rptr);
				}
				if (mouse8042_debug_minimal) {
					cmn_err(CE_CONT, ">a:%2x ",
					    *bp->b_rptr);
				}
#endif
				ddi_put8(state->handle,
					state->addr + I8042_INT_OUTPUT_DATA,
					*bp->b_rptr++);
			}
			next = bp->b_cont;
			freeb(bp);
		} while ((bp = next) != NULL);
		break;
	default:
		freemsg(mp);
		break;
	}
#ifdef MOUSE8042_DEBUG
	if (mouse8042_debug)
		cmn_err(CE_CONT, "mouse8042_wput:leaving\n");
#endif
	return (0);	/* ignored */
}

static uint_t
mouse8042_intr(caddr_t arg)
{
	unsigned char    mdata;
	mblk_t *mp;
	struct mouse_state *state = (struct mouse_state *)arg;
	int rc;

	mutex_enter(&state->mutex);

#if	defined(MOUSE8042_DEBUG)
	if (mouse8042_debug)
		cmn_err(CE_CONT, "mouse8042_intr()\n");
#endif
	rc = DDI_INTR_UNCLAIMED;

	for (;;) {

		if (ddi_get8(state->handle,
			    state->addr + I8042_INT_INPUT_AVAIL) == 0) {
			break;
		}

		mdata = ddi_get8(state->handle,
				state->addr + I8042_INT_INPUT_DATA);

#if	defined(MOUSE8042_DEBUG)
		if (mouse8042_debug)
			cmn_err(CE_CONT, "mouse8042_intr:  got %2x\n", mdata);
		if (mouse8042_debug_minimal)
			cmn_err(CE_CONT, "<A:%2x ", mdata);
#endif

		rc = DDI_INTR_CLAIMED;

		if (state->rqp != NULL && (mp = allocb(1, BPRI_MED))) {
			*mp->b_wptr++ = mdata;
			putnext(state->rqp, mp);
		}
	}
#ifdef MOUSE8042_DEBUG
	if (mouse8042_debug)
		cmn_err(CE_CONT, "mouse8042_intr() ok\n");
#endif
	mutex_exit(&state->mutex);

	return (rc);
}
