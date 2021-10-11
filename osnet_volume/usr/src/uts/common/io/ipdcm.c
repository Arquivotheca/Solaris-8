
#pragma ident	"@(#)ipdcm.c	1.25	99/03/01 SMI"

/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/strlog.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>
#include <sys/kstat.h>
#include <sys/stat.h>
#include <sys/strsun.h>
#include <sys/socket.h>
#include <sys/ethernet.h>
#include <netinet/in.h>
#include <sys/dlpi.h>
#include <sys/tihdr.h>
#ifdef ISERE_TREE
#include <ipd_ioctl.h>
#include <ipd_sys.h>
#include <ipd_extern.h>
#else
#include <sys/ipd_ioctl.h>
#include <sys/ipd_sys.h>
#include <sys/ipd_extern.h>
#endif

static struct ipd_str	*find_all(struct ipd_str *, struct ipd_softc *, int);
static struct ipd_str	*find_promisc(struct ipd_str *, struct ipd_softc *,
				int);
extern void miocack(queue_t *, mblk_t *, int, int);
extern void miocnak(queue_t *, mblk_t *, int, int);
extern void merror(queue_t *, mblk_t *, int);
static int ipd_xmitpkt(queue_t *, mblk_t *, struct ipd_addr_tbl *);

static int		ipd_uwput(queue_t *, mblk_t *);
static int		ipd_uwsrv(queue_t *);
static int		ipd_lwsrv(queue_t *);
static int		ipd_lrput(queue_t *, mblk_t *);
static int		ipd_lrsrv(queue_t *);
static int		ipd_proto(queue_t *, mblk_t *);
static void		ipd_ioctl(queue_t *, mblk_t *);
static void		ipd_timer(void *);
static void		ipd_flush_timers(void);
static struct ipd_addr_tbl	*ipd_add_entry(caddr_t, int,
				struct ipd_addr_tbl **);
static void		ipd_remove_entry(struct ipd_addr_tbl **,
				struct ipd_addr_tbl *);

static int		ipd_open(queue_t *, dev_t *p, int, int, cred_t *);
static int		ipd_close(queue_t *);
static void		ipd_strdetach(struct ipd_str *);
static void		ipd_sendup(struct ipd_softc *, mblk_t *,
				struct ipd_str *(*)());
static void		round_robin(struct ipd_addr_tbl **,
				struct ipd_addr_tbl *);
static int		ipdcminfo(dev_info_t *, ddi_info_cmd_t, void *,
				void **);
static int		ipdcmidentify(dev_info_t *);
static int		ipdcmattach(dev_info_t *, ddi_attach_cmd_t);
static int		ipdcmdetach(dev_info_t *, ddi_detach_cmd_t);
static void		ipdpriminfoinit(void);
static void		free_ifs(struct ipd_softc **);
static void		cmioctl(queue_t *, mblk_t *);

#ifdef IPD_DEBUG_MSGS
static void		print_ifs(struct ipd_softc *);
#endif /* IPD_DEBUG_MSGS */

static int		ipdcmopen(queue_t *, dev_t *p, int, int, cred_t *);
static int		ipdcmclose(queue_t *);
static int		ipdcm_wput(queue_t *, mblk_t *);
static void		ipd_uninit(void);

/*
 * IP/Dialup service: connection manager interface and common
 * point-to-point/point-to-multipoint interface code.
 *
 * Please refer to the ISERE architecture document for more
 * details.
 */

static struct module_info	ipdcmminfo = {
	0,		/* mi_idnum */
	IPDCM,		/* mi_idname */
	0,		/* mi_minpsz */
	INFPSZ,		/* mi_maxpsz */
	16384,		/* mi_hiwat */
	128		/* mi_lowat */
};

static struct qinit	ipdcm_rinit = {
	NULL,		/* qi_putp */
	NULL,		/* qi_srvp */
	ipdcmopen,	/* qi_qopen */
	ipdcmclose,	/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipdcmminfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	ipdcm_winit = {
	ipdcm_wput,	/* qi_putp */
	NULL,		/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipdcmminfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct streamtab ipdcmtab = {
	&ipdcm_rinit,	/* st_rdinit */
	&ipdcm_winit,	/* st_wrinit */
	NULL,		/* st_muxrinit */
	NULL		/* st_muxwrinit */
};



DDI_DEFINE_STREAM_OPS(ipdcm_ops, \
	ipdcmidentify, nulldev, ipdcmattach, ipdcmdetach, nodev, \
	ipdcminfo, D_NEW | D_MP, &ipdcmtab);


/*
 * point-to-multipoint interface streams interface
 */
static struct module_info ipd_minfo = {
	0,		/* mi_idnum */
	IPD_MTP_NAME,	/* mi_idname */
	0,		/* mi_minpsz */
	INFPSZ,		/* mi_maxpsz */
	2000,		/* mi_hiwat */
	128		/* mi_lowat */
};

static struct qinit	ipd_urinit = {
	NULL,		/* qi_putp */
	NULL,		/* qi_srvp */
	ipd_open,	/* qi_qopen */
	ipd_close,	/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipd_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	ipd_uwinit = {
	ipd_uwput,	/* qi_putp */
	ipd_uwsrv,	/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipd_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	ipd_lrinit = {
	ipd_lrput,	/* qi_putp */
	ipd_lrsrv,	/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipd_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	ipd_lwinit = {
	NULL,		/* qi_putp */
	ipd_lwsrv,	/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipd_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

struct streamtab	ipd_tab = {
	&ipd_urinit,	/* st_rdinit */
	&ipd_uwinit,	/* st_wrinit */
	&ipd_lrinit,	/* st_muxrinit */
	&ipd_lwinit	/* st_muxwrinit */
};

/*
 * DL_INFO_ACK template for point-to-multipoint interface.
 */
static	dl_info_ack_t ipd_mtp_infoack = {
	DL_INFO_ACK,			/* dl_primitive */
	IPD_MTU,			/* dl_max_sdu */
	0,				/* dl_min_sdu */
	IPD_MTP_ADDRL,			/* dl_addr_length */
	/*
	 * snoop et. al. don't know about DL_OTHER so this entry
	 * was changed to DL_ETHER so ethernet tracing/snooping
	 * facilities will work with IPdialup.
	 */
	DL_ETHER,			/* dl_mac_type */
	0,				/* dl_reserved */
	0,				/* dl_current_state */
	IPD_SAPL,			/* dl_sap_length */
	DL_CLDLS,			/* dl_service_mode */
	0,				/* dl_qos_length */
	0,				/* dl_qos_offset */
	0,				/* dl_range_length */
	0,				/* dl_range_offset */
	DL_STYLE2,			/* dl_provider_style */
	sizeof (dl_info_ack_t),		/* dl_addr_offset */
	DL_VERSION_2,			/* dl_version */
	0,				/* dl_brdcst_addr_length */
	0,				/* dl_brdcst_addr_offset */
	0				/* dl_growth */
};

/*
 * point-to-point interface streams interface
 */

static struct module_info ipdptp_minfo = {
	0,		/* mi_idnum */
	IPD_PTP_NAME,	/* mi_idname */
	0,		/* mi_minpsz */
	INFPSZ,		/* mi_maxpsz */
	2000,		/* mi_hiwat */
	128		/* mi_lowat */
};

static struct qinit	ipdptp_urinit = {
	NULL,		/* qi_putp */
	NULL,		/* qi_srvp */
	ipd_open,	/* qi_qopen */
	ipd_close,	/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipdptp_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	ipdptp_uwinit = {
	ipd_uwput,	/* qi_putp */
	ipd_uwsrv,	/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipdptp_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	ipdptp_lrinit = {
	ipd_lrput,	/* qi_putp */
	ipd_lrsrv,	/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipdptp_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct qinit	ipdptp_lwinit = {
	NULL,		/* qi_putp */
	ipd_lwsrv,	/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&ipdptp_minfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

struct streamtab	ipdptp_tab = {
	&ipdptp_urinit, /* st_rdinit */
	&ipdptp_uwinit, /* st_wrinit */
	&ipdptp_lrinit, /* st_muxrinit */
	&ipdptp_lwinit	/* st_muxwrinit */
};


/*
 * DL_INFO_ACK template for point-to-point interface.
 */
static	dl_info_ack_t ipd_ptp_infoack = {
	DL_INFO_ACK,			/* dl_primitive */
	IPD_MTU,			/* dl_max_sdu */
	0,				/* dl_min_sdu */
	IPD_PTP_ADDRL,			/* dl_addr_length */
	/*
	 * snoop et. al. don't know about DL_OTHER so this entry
	 * was changed to DL_ETHER so ethernet tracing/snooping
	 * facilities will work with IPdialup.
	 */
	DL_ETHER,			/* dl_mac_type */
	0,				/* dl_reserved */
	0,				/* dl_current_state */
	IPD_SAPL,			/* dl_sap_length */
	DL_CLDLS,			/* dl_service_mode */
	0,				/* dl_qos_length */
	0,				/* dl_qos_offset */
	0,				/* dl_range_length */
	0,				/* dl_range_offset */
	DL_STYLE2,			/* dl_provider_style */
	sizeof (dl_info_ack_t),		/* dl_addr_offset */
	DL_VERSION_2,			/* dl_version */
	0,				/* dl_brdcst_addr_length */
	0,				/* dl_brdcst_addr_offset */
	0				/* dl_growth */
};

/*
 * ipdpriminfo[] is used to do some initial DLPI primitive checks
 * before calling the routine to interpret each specific primitive.
 */
static struct ipdpriminfo	*ipdpriminfo = NULL;


/*
 * IP/Dialup list of IP interfaces
 */
struct ipd_softc	*ipd_ifs = NULL;


/*
 * IP/Dialup list of upper streams
 */
struct ipd_str		*ipd_strup = NULL;


/*
 * maximum number of IP interfaces allowed
 */
static int ipd_maxifs = IPD_MAXIFS;

/*
 * currently configured limit to multipoint interfaces
 */
static int ipd_nmtp   = IPD_MAXIFS;

/*
 * currently configured limit to point-to-point interfaces
 */
static int ipd_nptp   = IPD_MAXIFS;

/*
 * DDI device information for connection manager node
 */
static dev_info_t	*cmdip = NULL;

/*
 * DDI device information for multipoint node
 */
dev_info_t		*ipd_mtp_dip = NULL;

/*
 * DDI device information for point-to-point node
 */
dev_info_t		*ipd_ptp_dip = NULL;

/*
 * connection timeout cookie
 */
static timeout_id_t	cmtimer = 0;

int			ipd_debug = 0;

/*
 * IP/dialup mutex. The MT model used here is very simple.  A single mutex
 * is acquired by all open/close and service procedures when manipulating
 * global data structures.  This mutex may be held across putnext because
 * the put procedures never manipulate global information and so are not
 * locked.
 */
static kmutex_t		ipd_mutex;

#ifndef BUILD_STATIC

/*
 * connection manager wrapper
 */
#include <sys/modctl.h>

extern	struct mod_ops	mod_driverops;

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops, "IP/Dialup v1.9", &ipdcm_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

static struct ipdcm_minor_info ipdcm_minor[IPD_MAXIPDCMS];

_init(void)
{
	return (mod_install(&modlinkage));
}

_fini(void)
{
	/*
	 * Make sure the timer is cancelled before unloading.
	 */
	if (cmtimer != 0) {
		(void) untimeout(cmtimer);
		cmtimer = 0;
	}

	return (mod_remove(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
#endif BUILD_STATIC

/*
 * ipd_init()
 *
 * Initialise IP/Dialup
 *
 * Must be called at least once before using IP/Dialup
 *
 * Returns: none
 */
void
ipd_init(void)
{
	/*
	 *  one-off initialisations
	 */
	if (ipdpriminfo == NULL) {
		ipdpriminfoinit();
		mutex_init(&ipd_mutex, NULL, MUTEX_DEFAULT, NULL);
	}
}

/*
 * ipd_uninit()
 *
 * Prepare IP/Dialup for unload
 *
 * Returns: none
 */
static void
ipd_uninit(void)
{
	free_ifs(&ipd_ifs);

	if (ipdpriminfo) {
		mutex_destroy(&ipd_mutex);
		kmem_free(ipdpriminfo, sizeof (struct ipdpriminfo) *
								DL_MAXPRIM);
		ipdpriminfo = NULL;
	}

}

/*
 * Identify driver.
 */
static int
ipdcmidentify(dev_info_t *dip)
{
	IPD_DDIDEBUG("ipdcmidentify: called\n");

	if (strcmp(ddi_get_name(dip), IPDCM) == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}


/*
 * ipdcmattach()
 *
 * Attach this device to the system
 */
static int
ipdcmattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	IPD_DDIDEBUG("ipdcmattach: called\n");

	ipd_init();

	if (ddi_create_minor_node(dip, IPDCM, S_IFCHR, IPD_MINOR,
		NULL, CLONE_DEV) == DDI_FAILURE) {

		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}

	cmdip = dip;

	return (DDI_SUCCESS);
}

/*
 * ipdcmdetach()
 *
 * Detach an interface to the system
 */
static int
ipdcmdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	IPD_DDIDEBUG("ipdcmdetach: called\n");

	ddi_remove_minor_node(dip, NULL);

	cmdip = NULL;

	ipd_uninit();

	return (DDI_SUCCESS);
}

/*
 * ipdcminfo()
 *
 * DDI info entry point - translate "dev_t" to a pointer to the
 * associated "dev_info_t".
 *
 */
/* ARGSUSED */
static int
ipdcminfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t		dev = (dev_t)arg;
	minor_t		instance;
	int		rc;

	IPD_DDIDEBUG1("ipdcminfo: command %d\n", infocmd);

	instance = getminor(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:

		if (cmdip) {
			*result = (void *)cmdip;
			rc = DDI_SUCCESS;
		} else {
			rc = DDI_FAILURE;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		rc = DDI_SUCCESS;
		break;

	default:
		rc = DDI_FAILURE;
		break;
	}
	return (rc);
}


/*
 * ipdcmopen()
 *
 * 5.x STREAMS driver open for connection manager interface
 *
 * Returns: 0 on success, error code otherwise
 */

/*ARGSUSED*/
static int
ipdcmopen(queue_t *rq, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	struct ipdcm_minor_info *ipdcm_data;
	dev_t  device;

	if (rq->q_ptr)
		return (EBUSY);
	if (sflag == CLONEOPEN) {
		for (device = 0; device < IPD_MAXIPDCMS; device++)
			if (ipdcm_minor[device].rq == NULL)
				break;
	} else {
		device = getminor(*devp);
	}

/* CTE fix for bugid 4067655, sgypsy 4/29/98 */
	if (device >= IPD_MAXIPDCMS)
		return (ENXIO);

	ipdcm_data = &ipdcm_minor[device];
	ipdcm_data->rq = rq;
	ipdcm_data->registree = 0;
	rq->q_ptr = (char *)ipdcm_data;
	WR(rq)->q_ptr = (char *)ipdcm_data;

	qprocson(rq);

	if (cmtimer == 0) {
		cmtimer = timeout(ipd_timer, NULL, hz);
	}

	return (0);
}

/*
 * 5.x STREAMS close function for connection manager interface
 */

/*ARGSUSED*/
static int
ipdcmclose(queue_t *rq)
{
	struct ipd_softc *ifp;
	struct ipdcm_minor_info *ipdcm_data;
	int device;

	qprocsoff(rq);

	mutex_enter(&ipd_mutex);

	ipdcm_data = (struct ipdcm_minor_info *)rq->q_ptr;
	ipdcm_data->rq = NULL;
	ipdcm_data->registree = 0;
	rq->q_ptr = NULL;
	WR(rq)->q_ptr = NULL;

/*
 * Set all references for this device being the connection manager to
 * an interface to be NULL.
 */
	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {
		if (ifp->to_cnxmgr == rq)
			ifp -> to_cnxmgr = NULL;
	}

/*
 * Check if there are still any opened devices.	 If not, we want to
 * stop the timer.
 */
	for (device = 0; device < IPD_MAXIPDCMS; device++)
		if (ipdcm_minor[device].rq != NULL)
			break;

	if (device >= IPD_MAXIPDCMS && cmtimer) {
		(void) untimeout(cmtimer);
		cmtimer = 0;
		ipd_flush_timers();
	}
	mutex_exit(&ipd_mutex);

	return (0);
}

/*
 * ipdcm_wput()
 *
 * connection manager write side put function.	This is the one exception to
 * the general MT model as it does acquire the ipd_mutex for processing
 * ioctls().
 * Returns: 0
 */
static int
ipdcm_wput(queue_t *q, mblk_t *mp)
{
	switch (MTYPE(mp)) {

	case M_FLUSH:

		/*
		 * do standard streams flush processing
		 */
		if (*mp->b_rptr & FLUSHW) {
			IPD_DEBUG("ipdcmw: flushing write queue\n");
			flushq(q, FLUSHDATA);	/* XXX is this necessary? */
		}

		if (*mp->b_rptr & FLUSHR) {
			IPD_DEBUG("ipdcmw: flushing read queue\n");
			flushq(RD(q), FLUSHDATA);
			*mp->b_rptr &= ~FLUSHW;
			qreply(q, mp);
		} else
			freemsg(mp);


		break;

	case M_IOCTL:

		/*
		 * process ioctl()s on the connection manager stream
		 */

		cmioctl(q, mp);
		break;

	default:
		/*
		 * dump everything else
		 */
		freemsg(mp);
	}

	return (0);
}



/*
 * cmioctl()
 *
 * process ioctls on the streams attached to the connection manager
 *
 * Returns: M_IOCACK or M_IOCNAK to upstream
 */

static void
cmioctl(queue_t *q, mblk_t *mp)
{

	int			iftype, ifunit, found, rc;
	struct iocblk		*iocp = (struct iocblk *)mp->b_rptr;
	union ipd_messages	*reqp;
	struct ipd_softc	*ifp;
	struct sockaddr_in	*sin;
	struct ipd_addr_tbl	*ap;
	struct ipdcm_minor_info *ipdcm_data;

	if (iocp->ioc_count < sizeof (uint_t)) {
		goto iocnak;
	}

	reqp = (union ipd_messages *)mp->b_cont->b_rptr;

	rc = EINVAL;

	switch (reqp->msg) {

	/*
	 * set the maximum number of each type of interface
	 */
	case IPD_MAKE_IF:

		/*
		 * Sanity checks
		 */

		if (iocp->ioc_count != sizeof (ipd_make_if_t)) {
			goto iocnak;
		}

		iftype = reqp->make_if.iftype;
		ifunit = reqp->make_if.ifunit;

		if (ifunit <= 0 || ifunit > ipd_maxifs) {
			goto iocnak;
		}

		switch (iftype) {

		case IPD_PTP:
			ipd_nptp = ifunit;
			break;

		case IPD_MTP:
			ipd_nmtp = ifunit;
			break;

		default:
			rc = ENXIO;
			goto iocnak;
		}

		miocack(q, mp, 0, 0);
		break;


	/*
	 * set the maximum inactivity time for a destination
	 */
	case IPD_SET_TIM:

		/*
		 * Sanity checks
		 */
		if (iocp->ioc_count != sizeof (ipd_set_tim_t)) {
			goto iocnak;
		}

		iftype = reqp->set_tim.iftype;
		ifunit = reqp->set_tim.ifunit;
		sin = (struct sockaddr_in *)&reqp->set_tim.sa;
		found = 0;

		mutex_enter(&ipd_mutex);

		for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {

			if (ifp->if_type == iftype && ifp->if_unit == ifunit) {
				for (ap = ifp->if_conn; ap; ap = ap->next) {

						ap->addr_timeout = ap->timeout =
							reqp->set_tim.timeout;
						found = 1;
				}
				break;
			}
		}

		mutex_exit(&ipd_mutex);

		if (!found) {
			rc = ENOENT;
			goto iocnak;
		}
		miocack(q, mp, 0, 0);
		break;


	/*
	 * blacklist a destination - refuse to ask for connections for it
	 * for a specified period of time.
	 */
	case IPD_BLACKLIST:

		/*
		 * Sanity checks
		 */

		if (iocp->ioc_count != sizeof (ipd_blacklist_t)) {
			goto iocnak;
		}

		iftype = reqp->blacklist.iftype;
		ifunit = reqp->blacklist.ifunit;
		sin = (struct sockaddr_in *)&reqp->blacklist.sa;

		mutex_enter(&ipd_mutex);

		for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {

			if (ifp->if_type == iftype && ifp->if_unit == ifunit) {

				for (ap = ifp->if_conn; ap; ap = ap->next) {

					if (ap->rq) {
						/*
						 * can't blacklist this!
						 */
						mutex_exit(&ipd_mutex);
						rc = EEXIST;
						goto iocnak;
					}
					if (EQUAL(&ap->dst, &sin->sin_addr)) {
						break;
					}
				}
				break;
			}
		}

		if (ifp == NULL) {

			/*
			 * unknown interface
			 */
			mutex_exit(&ipd_mutex);
			rc = ENODEV;
			goto iocnak;
		}

		/*
		 * Entry not found? Get a new one to mark this destination as
		 * blacklisted
		 */
		if (ap == NULL) {

			ap = ipd_add_entry((caddr_t)&sin->sin_addr,
			    iftype == IPD_PTP? 0 : IPD_MTP_ADDRL,
								&ifp->if_conn);
		}

		/*
		 * memory allocation failure?
		 */
		if (ap == NULL) {
			mutex_exit(&ipd_mutex);
			rc = EAGAIN;
			goto iocnak;
		}

		IPD_DEBUG1("IPD_BLACKLIST: blacklisting for %d seconds\n",
				ap->timeout);
		ap->ifp		= ifp;
		ap->timeout	= reqp->blacklist.blacklisttime;

		mutex_exit(&ipd_mutex);

		miocack(q, mp, 0, 0);
		break;

	case IPD_REGISTER:
		if (iocp->ioc_count != sizeof (ipd_register_t)) {
			goto iocnak;
		}

		mutex_enter(&ipd_mutex);

		iftype = reqp->regist.iftype;
		ifunit = reqp->regist.ifunit;

		for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {
			if (ifp->if_type == iftype && ifp->if_unit == ifunit)
				break;
		}
		if (ifp == NULL) {

			/*
			 * unknown interface
			 */
			mutex_exit(&ipd_mutex);
			rc = ENODEV;
			goto iocnak;
		}
		ifp->to_cnxmgr = RD(q);

		ipdcm_data = q->q_ptr;
		ipdcm_data -> registree = 1;

		mutex_exit(&ipd_mutex);

		miocack(q, mp, 0, 0);
		break;

	case IPD_UNREGISTER:
		if (iocp->ioc_count != sizeof (ipd_unregister_t)) {
			goto iocnak;
		}

		mutex_enter(&ipd_mutex);

		iftype = reqp->unregist.iftype;
		ifunit = reqp->unregist.ifunit;
		for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {
			if (ifp->if_type == iftype && ifp->if_unit == ifunit)
				break;
		}
		if (ifp == NULL) {

			/*
			 * unknown interface
			 */
			mutex_exit(&ipd_mutex);
			rc = ENODEV;
			goto iocnak;
		}
		ifp->to_cnxmgr = NULL;

		ipdcm_data = q->q_ptr;
		ipdcm_data->registree = 0;

		mutex_exit(&ipd_mutex);

		miocack(q, mp, 0, 0);
		break;
	default:
		/*
		 * unrecognised ioctl
		 */

		rc = ENOSYS;
iocnak:
		miocnak(q, mp, 0, rc);
	}
}


/*
 * ipd_kstat_updater()
 *
 * update kstat interface statistics
 *
 * Returns: 0 on success, error code otherwise
 */
static int
ipd_kstat_updater(kstat_t *ksp, int rw)
{
	struct ipd_softc	*ifp;
	struct ipd_stat	*ipdsp;

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	}

	ifp = (struct ipd_softc *)ksp->ks_private;
	ipdsp = (struct ipd_stat *)ksp->ks_data;
	ipdsp->ipd_ipackets.value.ul	= ifp->if_ipackets;
	ipdsp->ipd_ierrors.value.ul	= ifp->if_ierrors;
	ipdsp->ipd_opackets.value.ul	= ifp->if_opackets;
	ipdsp->ipd_oerrors.value.ul	= ifp->if_oerrors;
	ipdsp->ipd_nocanput.value.ul	= ifp->if_nocanput;
	ipdsp->ipd_allocbfail.value.ul	= ifp->if_allocbfail;

	return (0);
}

/*
 * ipd_initstats()
 *
 * make interface statistics available to kernel
 *
 * Returns: none.
 */
static void
ipd_initstats(struct ipd_softc *ifp)
{
	kstat_t		*ksp;
	struct ipd_stat	*ipdsp;

	if ((ksp = kstat_create(ifp->if_name, ifp->if_unit,
	    NULL, "net", KSTAT_TYPE_NAMED,
	    sizeof (struct ipd_stat) / sizeof (kstat_named_t), 0)) == NULL) {

		IPD_DDIDEBUG2("ipd_initstats: add_kstat for %s failed\n",
					ifp->if_name, ifp->if_unit);
		return;
	}

	ipdsp = (struct ipd_stat *)(ksp->ks_data);

	kstat_named_init(&ipdsp->ipd_ipackets,	"ipackets",
		KSTAT_DATA_UINT32);
	kstat_named_init(&ipdsp->ipd_ierrors,	"ierrors",
		KSTAT_DATA_UINT32);
	kstat_named_init(&ipdsp->ipd_opackets,	"opackets",
		KSTAT_DATA_UINT32);
	kstat_named_init(&ipdsp->ipd_oerrors,	"oerrors",
		KSTAT_DATA_UINT32);
	kstat_named_init(&ipdsp->ipd_nocanput,	"nocanput",
		KSTAT_DATA_UINT32);
	kstat_named_init(&ipdsp->ipd_allocbfail, "allocbfail",
		KSTAT_DATA_UINT32);

	ksp->ks_update = ipd_kstat_updater;
	ksp->ks_private = (void *)ifp;
	ifp->if_stats = ksp;

	kstat_install(ksp);

	IPD_DDIDEBUG2("kstats for %s%d added\n", ifp->if_name, ifp->if_unit);
}

/*
 * ipd_endstats()
 *
 * remove interface statistics from the kernel
 *
 * Returns: none
 */
static void
ipd_endstats(struct ipd_softc *ifp)
{
	if (ifp->if_stats) {

		IPD_DDIDEBUG2("kstats for %s%d removed\n", ifp->if_name,
								ifp->if_unit);
		kstat_delete(ifp->if_stats);
	}

}


/*
 * ipd_tellcm()
 *
 * send a message to the connection manager
 *
 * Returns: none, M_ERRORs the connection manager stream on error
 */
static void
ipd_tellcm(struct ipd_addr_tbl *ap, enum ipd_msgs type, uint_t param)
{
	mblk_t			*mp, *np;
	union ipd_messages	*dp;
	struct sockaddr_in	*sin;
	struct ipdcm_minor_info *ipdcm_data;
	int			i;

	ASSERT(ap->ifp);

	/*
	 * create a M_PROTO containing a struct ipd_msg and send
	 * to the connection manager
	 */

	mp = allocb(sizeof (union ipd_messages), BPRI_HI);
	if (mp == (mblk_t *)NULL) {
		IPD_DEBUG("ipd_tellcm: allocb failed\n");
		ap->ifp->if_allocbfail++;
		/*
		 * Must pass the write side queue pointer since merror()
		 * does a qreply(), sending the msg. upstream as desired
		 * see bugid 4193690 (esc #517949)
		 */
		merror(WR(ap->ifp->to_cnxmgr), NULL, ENOBUFS);
		return;
	}

	MTYPE(mp) = M_PROTO;

	/*
	 * get a handle on the data block attached to the message
	 */
	dp = (union ipd_messages *)mp->b_rptr;


	/*
	 * use the fact that all the messages have a common header
	 */
	bzero((void *)dp, sizeof (union ipd_messages));

	dp->con_dis.msg	   = type;
	dp->con_dis.iftype = ap->ifp->if_type;
	dp->con_dis.ifunit = ap->ifp->if_unit;

	sin = (struct sockaddr_in *)&dp->con_dis.sa;

	if (ap->ifp->if_type == IPD_MTP) {
		sin->sin_family = AF_INET;
	} else {
		sin->sin_family = AF_UNSPEC;
	}
	(void) bcopy((caddr_t)&ap->dst, (caddr_t)&sin->sin_addr,
								IPD_MTP_ADDRL);

	switch (type) {

	case	IPD_CON_REQ:
	case	IPD_DIS_REQ:
		mp->b_wptr += sizeof (ipd_con_dis_t);
		break;

	case	IPD_DIS_IND:
		dp->dis_ind.reason = param;
		mp->b_wptr += sizeof (ipd_dis_ind_t);
		break;

	case	IPD_ERR_IND:
		dp->err_ind.error = param;
		mp->b_wptr += sizeof (ipd_err_ind_t);
		break;
	}

	if (ap->ifp->to_cnxmgr != NULL) {
		putnext(ap->ifp->to_cnxmgr, mp);
	} else {
		for (i = 0; i < IPD_MAXIPDCMS; i++) {
			ipdcm_data = &ipdcm_minor[i];
			if (ipdcm_data->rq && !ipdcm_data->registree) {
				np = copymsg(mp);
				putnext(ipdcm_data->rq, np);
			}
		}
		freemsg(mp);
	}
}


/*
 * free_ifs()
 *
 * remove an interface list
 *
 * Returns: none.
 */
static void
free_ifs(struct ipd_softc **pifs)
{
	struct ipd_softc	*nif;

	mutex_enter(&ipd_mutex);

	while (*pifs) {
		nif = (*pifs)->if_next;
		ipd_endstats(*pifs);
		kmem_free(*pifs, sizeof (**pifs));
		*pifs = nif;
	}

	mutex_exit(&ipd_mutex);
}

#ifdef IPD_DEBUG_MSGS
/*
 * print_ifs()
 *
 * print the interface list.  Should be called with the ipd_mutex held.
 *
 * Returns: none.
 */
static void
print_ifs(struct ipd_softc *ifs)
{
	struct ipd_addr_tbl		*ap;

	while (ifs) {
		printf("interface: %s%d\n",
			ifs->if_name, ifs->if_unit);

		for (ap = ifs->if_conn; ap; ap = ap->next) {
			printf("ap=%p, rq=%p, timeout=%d, addr_timeout=%d\n",
				ap, ap->rq, ap->timeout, ap->addr_timeout);
			if (ap->rq) {
				printf("qsize(rq)=%d, qsize(wq)=%d\n",
					qsize(ap->rq), qsize(WR(ap->rq)));
			}
		}

		ifs = ifs->if_next;
	}
}

#endif /* IPD_DEBUG_MSGS */

/*
 * process_interface()
 *
 * process one connection.  Should be called with the ipd_mutex held.
 *
 * Returns: None
 */
static void
process_interface(struct ipd_softc *tbl)
{
	struct ipd_addr_tbl	*ap, *nap = NULL;

	for (ap = tbl->if_conn; ap; ap = nap) {

		nap = ap->next;
		if (ap->timeout < 1) {

			/*
			 * -1 = static; 0 = infinite timeout
			 * (which is different from static as
			 * the connection was brought up by
			 * network traffic rather than the
			 * connection manager)
			 */
			continue;
		}


		/*
		 * this connection has been active, reset its timeout
		 */
		if (ap->rq && ap->act_count > 0) {
			ap->act_count = 0;
			ap->timeout = ap->addr_timeout;
			continue;
		}

		/*
		 * this connection has been inactive since last tick,
		 * bump its count down, and carry on if it still has
		 * some time to go
		 */
		if (--ap->timeout > 0)
			continue;

		/*
		 * timeout has occurred
		 */

		if (ap->rq == (queue_t *)NULL) {

			/*
			 * no connection existed
			 */
			IPD_DEBUG("ipd_timer: no answer from cnx manager\n");
			ipd_remove_entry(&tbl->if_conn, ap);
			continue;
		}

		/*
		 * timeout occurred, connection exists, talk to the cm
		 */

		IPD_DEBUG1("ipd_timer: connection %p idle\n", ap->rq);
		ipd_tellcm(ap, IPD_DIS_REQ, 0);

		/*
		 * set a finite timeout for a response from the cm
		 */
		ap->timeout = IPD_HOLDING_TIME;
	}
}




/*
 * ipd_timer()
 *
 * scan the current connection list, trying to find those which have become
 * inactive.
 *
 * Returns: none
 */
/* ARGSUSED */
static void
ipd_timer(void *arg)
{
	struct ipd_softc	*ifp;

	mutex_enter(&ipd_mutex);

	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {

		/*
		 * check for connections which
		 * have timed out on interfaces
		 */

		process_interface(ifp);
	}

	cmtimer = timeout(ipd_timer, NULL, hz);

	mutex_exit(&ipd_mutex);
}

/*
 * ipd_flush_timers()
 *
 * flush un-expired timers - used when connection manager goes away.
 * Must be called with the ipd_mutex held.
 *
 * Returns: none
 */

static void
ipd_flush_timers(void)
{
	struct ipd_softc	*ifp;
	struct ipd_addr_tbl	*ap, *nap = NULL;


	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {

		/*
		 * check for unexpired timers
		 */

		for (ap = ifp->if_conn; ap; ap = nap) {

			nap = ap->next;

			if (ap->rq == (queue_t *)NULL) {
				IPD_DEBUG("ipd_flush_timers: removing timer\n");
				ipd_remove_entry(&ifp->if_conn, ap);
			}
		}
	}
}


/*
 * ipd_add_entry()
 *
 * allocate a entry in the address resolution table.  Should be called with
 * the ipd_mutex held.
 *
 * Returns: pointer to a new table entry on success, NULL on error
 */

static struct ipd_addr_tbl *
ipd_add_entry(caddr_t addr, int addr_len, struct ipd_addr_tbl **root)
{
	struct		ipd_addr_tbl	*ap;


	IPD_DEBUG("ipd_add_entry: allocating table entry\n");

	ASSERT(addr_len <= 14); /* max space allowed in sockaddr */

	ap = (struct ipd_addr_tbl *)
		kmem_zalloc(sizeof (struct ipd_addr_tbl), KM_NOSLEEP);

	if (ap == NULL) {
		return (NULL);
	}

	ap->next = *root;
	ap->prev = NULL;
	if (*root != NULL)
		(*root)->prev = ap;
	*root = ap;

	if (addr_len != 0) {
		(void) bcopy(addr, (caddr_t)&ap->dst, addr_len);
	}

	IPD_DEBUG1("ipd_add_entry: returning %p\n", ap);
	return (ap);
}


/*
 * ipd_remove_entry()
 *
 * remove an entry from the address resolution table.  Should be called with
 * the ipd_mutex held.
 *
 * Returns: none
 */

static void
ipd_remove_entry(struct ipd_addr_tbl **root, struct ipd_addr_tbl *item)
{
	mblk_t		*nmp;

	IPD_DEBUG1("ipd_remove_entry: removing table entry %p\n", item);

	/*
	 * remove address entry from the list
	 */
	if (item->next != NULL)
		item->next->prev = item->prev;

	if (item->prev != NULL)
		item->prev->next = item->next;
	else
		*root = item->next;

	while (item->pkt) {
		nmp = item->pkt->b_next;
		item->pkt->b_next = NULL;
		freemsg(item->pkt);
		item->pkt = nmp;
	}

	if (item->rq) {
		item->rq->q_ptr = NULL;
		WR(item->rq)->q_ptr = NULL;
	}

	(void) kmem_free(item, sizeof (struct ipd_addr_tbl));

}



/*ARGSUSED*/
/*
 * ipd_open()
 *
 * Common mtp/ptp open routine for IP/Dialup
 *
 * Returns: none.
 */
static int
ipd_open(queue_t *rq, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	struct ipd_str		*stp;
	struct ipd_str		**prevstp;
	minor_t			minordev;

	IPD_DEBUG1("ipd_open: opening stream %p\n", rq);

	ASSERT(rq);

	mutex_enter(&ipd_mutex);

	switch (sflag) {

	case CLONEOPEN:

		/*
		 * allocate the first unused minor device number
		 */

		prevstp = &ipd_strup;
		minordev = 0;
		for (; (stp = *prevstp) != 0; prevstp = &stp->st_next) {
			if (minordev < stp->st_minor)
				break;
			minordev++;
		}

		*devp = makedevice(getmajor(*devp), minordev);
		break;

	default:
		minordev = getminor(*devp);
		break;

	case MODOPEN:
		mutex_exit(&ipd_mutex);
		return (EINVAL);
	}


	if (rq->q_ptr) {
		mutex_exit(&ipd_mutex);
		return (0);
	}

	if ((stp = GETSTRUCT(struct ipd_str, 1)) == NULL) {
		mutex_exit(&ipd_mutex);
		return (ENOMEM);
	}

	stp->st_minor = minordev;
	stp->st_rq = rq;
	stp->st_state = DL_UNATTACHED;
	stp->st_sap = 0;
	stp->st_ifp = NULL;
	stp->st_type = DRIVER_IS(*devp, IPD_MTP_NAME) ? IPD_MTP : IPD_PTP;
	stp->st_raw = 0;

	/*
	 * Link new entry into the list of active entries.
	 */
	stp->st_next = *prevstp;
	*prevstp = stp;

	rq->q_ptr = WR(rq)->q_ptr = (char *)stp;
	mutex_exit(&ipd_mutex);

	qprocson(rq);

	return (0);
}

/*
 * ipd_close()
 *
 * common mtp/ptp close routine for IP/Dialup
 *
 * Returns: 0
 */

static int
ipd_close(queue_t *rq)
{
	struct ipd_str *stp;
	struct ipd_str **prevstp;

	ASSERT(rq);
	ASSERT(rq->q_ptr);

	IPD_DEBUG1("ipd_close: closing stream %p\n", rq);

	qprocsoff(rq);

	mutex_enter(&ipd_mutex);

	stp = (struct ipd_str *)rq->q_ptr;

	ipd_strdetach(stp);

	/*
	 * Unlink the per-stream entry from the active list and free it.
	 */
	for (prevstp = &ipd_strup; (stp = *prevstp) != 0;
		prevstp = &stp->st_next) {
		if (stp == (struct ipd_str *)rq->q_ptr)
			break;
	}

	ASSERT(stp);
	*prevstp = stp->st_next;

	kmem_free((char *)stp, sizeof (struct ipd_str));

	rq->q_ptr = WR(rq)->q_ptr = NULL;

	mutex_exit(&ipd_mutex);

	return (0);
}

/*
 * ipd_uwput()
 *
 * Upper write-side put procedure
 *
 * Returns: 0
 */
static int
ipd_uwput(queue_t *wq, mblk_t *mp)
{
	ASSERT(wq);
	ASSERT(mp);

	switch (DB_TYPE(mp)) {

		case M_IOCTL:
		case M_PROTO:
		case M_PCPROTO:
			(void) putq(wq, mp);
			break;

		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW) {
				flushq(wq, FLUSHALL);
				*mp->b_rptr &= ~FLUSHW;
			}
			if (*mp->b_rptr & FLUSHR) {
				flushq(RD(wq), FLUSHALL);
				qreply(wq, mp);
			}
			else
				freemsg(mp);
			break;

		default:
			freemsg(mp);
			break;
	}
	return (0);
}

/*
 * ipd_uwsrv()
 *
 * Upper write-side service procedure
 *
 * Returns: 0
 */
static int
ipd_uwsrv(queue_t *wq)
{
	mblk_t		 *mp;

	while (mp = getq(wq)) {

		switch (DB_TYPE(mp)) {

			case M_PROTO:
			case M_PCPROTO:

				if (ipd_proto(wq, mp)) {
					(void) putbq(wq, mp);
					return (0);
				}
				break;

			case M_IOCTL:
				ipd_ioctl(wq, mp);
				break;

			default:
				freemsg(mp);
				break;
		}
	}
	return (0);
}


/*
 * ipd_lwsrv()
 *
 * Lower write-side service procedure.	Used for back enabling upper
 * streams only.
 *
 * Returns: 0
 */
/* ARGSUSED */
static int
ipd_lwsrv(queue_t *wq)
{
	struct ipd_str		*stp;

	IPD_FLDEBUG1("ipd_lwsrv: rq=%p\n", RD(wq));

	mutex_enter(&ipd_mutex);

	for (stp = ipd_strup; stp; stp = stp->st_next) {
		IPD_FLDEBUG1("ipd_lwsrv: re-enabling rq %p\n", stp->st_rq);
		qenable(stp->st_rq);
		IPD_FLDEBUG1("ipd_lwsrv: re-enabling wq %p\n", WR(stp->st_rq));
		qenable(WR(stp->st_rq));
	}

	mutex_exit(&ipd_mutex);
	return (0);
}


/*
 * ipd_lrput()
 *
 * Lower read-side put procedure
 *
 * Returns: 0
 */
static int
ipd_lrput(queue_t *rq, mblk_t *mp)
{
	struct ipd_addr_tbl	*ap;
	t_uscalar_t		*prim;
	struct T_discon_ind	*tdisind;
	dl_disconnect_ind_t	*dldisind;

	if ((ap = (struct ipd_addr_tbl *)rq->q_ptr) == NULL) {

		/*
		 * we are not attached to an interface?
		 */
		IPD_DEBUG("ipd_lrput: dumping data - no upper stream\n");
		freemsg(mp);
		return (0);
	}

	ASSERT(ap->ifp);

	switch (MTYPE(mp)) {

	case	M_DATA:

		(void) putq(rq, mp);
		break;

	case	M_PROTO:

		IPD_DEBUG("ipd_lrput: M_PROTO in\n");

		/*
		 * try to guess if the driver underneath is signalling a
		 * disconnect of some form
		 */
		if (mp->b_wptr - mp->b_rptr < sizeof (prim)) {
			IPD_DEBUG("ipd_lrput: bad primsize\n");
			freemsg(mp);
			break;
		}
		prim = (t_uscalar_t *)mp->b_rptr;

		/*
		 * this is a bit naughty - normally this put procedure
		 * should not lock, but I need to call ipd_tellcm to
		 * send a message to the connection manager and this needs
		 * to be locked.   It should be impossible for the same thread
		 * to re-enter though.
		 */
		mutex_enter(&ipd_mutex);

		switch (*prim) {

		case DL_DISCONNECT_IND:

			if (mp->b_wptr - mp->b_rptr !=	sizeof (*dldisind)) {
				IPD_DEBUG("ipd_lrput: bad DL_DISCONNECT_IND\n");
				break;
			}
			dldisind = (dl_disconnect_ind_t *)mp->b_rptr;
			noenable(rq);
			ipd_tellcm(ap, IPD_DIS_IND, dldisind->dl_reason);
			freemsg(mp);
			break;

		case T_DISCON_IND:

			if (mp->b_wptr - mp->b_rptr <  sizeof (*tdisind)) {
				IPD_DEBUG("ipd_lrput: bad T_DISCON_IND\n");
				break;
			}
			noenable(rq);
			tdisind = (struct T_discon_ind *)mp->b_rptr;
			ipd_tellcm(ap, IPD_DIS_IND, tdisind->DISCON_reason);
			freemsg(mp);
			break;

		default:
			(void) putq(rq, mp);

		}

		mutex_exit(&ipd_mutex);

		break;

	case	M_ERROR:
		IPD_DEBUG1("ipd_lrput: M_ERROR (0x%x)\n", *mp->b_rptr);
		ipd_tellcm(ap, IPD_ERR_IND, *mp->b_rptr);
		freemsg(mp);
		break;

	default:
		IPD_DEBUG1("ipd_lrput: freeing message type 0x%x\n", MTYPE(mp));
		freemsg(mp);
	}

	return (0);
}


/*
 * ipd_lrsrv()
 *
 * Lower read-side service procedure
 *
 * Returns: 0
 */
static int
ipd_lrsrv(queue_t *rq)
{
	struct ipd_addr_tbl	*ap;
	struct ipd_softc	*ifp;
	mblk_t			*mp;
	t_uscalar_t		*prim;
	struct T_discon_ind	*tdisind;
	dl_disconnect_ind_t	*dldisind;

	IPD_DATADBG1("ipd_lrsrv: rq=%p\n", rq);

	mutex_enter(&ipd_mutex);

	ap = (struct ipd_addr_tbl *)rq->q_ptr;

	ifp = ap->ifp;

	while (mp = getq(rq)) {

	switch (MTYPE(mp)) {
	case	M_PROTO:

		IPD_DEBUG("ipd_lrsrv: M_PROTO in\n");

		/*
		 * try to guess if the driver underneath is signalling a
		 * disconnect of some form
		 */
		if (mp->b_wptr - mp->b_rptr < sizeof (prim)) {
			IPD_DEBUG("ipd_lrput: bad primsize\n");
			freemsg(mp);
			break;
		}
		prim = (t_uscalar_t *)mp->b_rptr;

		/*
		 * this is a bit naughty - normally this put procedure
		 * should not lock, but I need to call ipd_tellcm to
		 * send a message to the connection manager and this needs
		 * to be locked.   It should be impossible for the same thread
		 * to re-enter though.
		 */

		switch (*prim) {

		case DL_DISCONNECT_IND:

			if (mp->b_wptr - mp->b_rptr !=	sizeof (*dldisind)) {
				IPD_DEBUG("ipd_lrput: bad DL_DISCONNECT_IND\n");
				break;
			}
			dldisind = (dl_disconnect_ind_t *)mp->b_rptr;
			noenable(rq);
			ipd_tellcm(ap, IPD_DIS_IND, dldisind->dl_reason);
			freemsg(mp);
			break;

		case T_DISCON_IND:

			if (mp->b_wptr - mp->b_rptr <  sizeof (*tdisind)) {
				IPD_DEBUG("ipd_lrput: bad T_DISCON_IND\n");
				break;
			}
			noenable(rq);
			tdisind = (struct T_discon_ind *)mp->b_rptr;
			ipd_tellcm(ap, IPD_DIS_IND, tdisind->DISCON_reason);
			freemsg(mp);
			break;

		default:
			freemsg(mp);

		}


		break;


		/*
		 * bump activity counts up
		 */
	case M_DATA:
		IPD_DEBUG("ipd_lrsrv: M_DATA in\n");

		ap->act_count++;
		ap->act_time = hrestime;
		ipd_sendup(ifp, mp, find_all);
		ifp->if_ipackets++;
		break;
	}
	}

	mutex_exit(&ipd_mutex);
	return (0);
}



/*
 * ipd_proto()
 *
 * handle DLPI proto messages
 *
 * Returns: 0 when it is ok to process the next message, error otherwise
 */
static int
ipd_proto(queue_t *wq, mblk_t *mp)
{
	union	DL_primitives	*dlp;
	struct ipdpriminfo	*pip;
	struct ipd_str		*stp;
	int			rc, len;
	t_uscalar_t		prim;

	/*
	 * Get primitive type.
	 */
	if ((len = MBLKL(mp)) < sizeof (t_uscalar_t)) {
		merror(wq, mp, EPROTO);
		return (0);
	}
	dlp = (union DL_primitives *)mp->b_rptr;
	prim = dlp->dl_primitive;

	/*
	 * Check supported primitive.
	 */
	if (prim > DL_MAXPRIM) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_BADPRIM, 0);
		return (0);
	}

	pip = &ipdpriminfo[prim];

	if (pip->pi_funcp == NULL) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_NOTSUPPORTED, 0);
		return (0);
	}

	/*
	 * Check minimum length.
	 */
	if (len < pip->pi_minlen) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_BADPRIM, 0);
		return (0);
	}

	mutex_enter(&ipd_mutex);

	stp = (struct ipd_str *)wq->q_ptr;

	/*
	 * Check valid state.
	 */
	if ((pip->pi_state != 0) &&
		(stp->st_state != pip->pi_state)) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_OUTSTATE, 0);
		mutex_exit(&ipd_mutex);
		return (0);
	}

	/*
	 * Call primitive-specific routine
	 */
	rc =  (*pip->pi_funcp)(stp, wq, mp);

	mutex_exit(&ipd_mutex);

	return (rc);
}

/*
 * ipd_ioctl()
 *
 * handle ioctl messages
 */
static void
ipd_ioctl(queue_t *wq, mblk_t *mp)
{
	struct iocblk		*iocp = (struct iocblk *)mp->b_rptr;
	struct ipd_str		*stp;
	struct linkblk		*lp;
	struct ipd_addr_tbl	*ap = (struct ipd_addr_tbl *)NULL;
	int			err = EINVAL;
	mblk_t			*nmp;
	struct ipd_softc	*ifp;

	switch (iocp->ioc_cmd) {

	case DLIOCRAW:		/* raw M_DATA mode */

		mutex_enter(&ipd_mutex);

		stp = (struct ipd_str *)wq->q_ptr;

		stp->st_raw = 1;
#ifdef IPD_DEBUG_MSGS
		if (ipd_debug & IPD_DDI) {
			print_ifs(ipd_ifs);
		}
#endif /* IPD_DEBUG_MSGS */

		mutex_exit(&ipd_mutex);

		miocack(wq, mp, 0, 0);
		break;

	case	I_LINK:

		IPD_DEBUG1("ipd_ioctl: I_LINK on q=%p\n", RD(wq));

		mutex_enter(&ipd_mutex);

		stp = (struct ipd_str *)wq->q_ptr;

		/*
		 * cannot link a stream under because no interface attached
		 */
		ifp = stp->st_ifp;
		if (ifp == NULL) {
			mutex_exit(&ipd_mutex);
			err = ENOLINK;
			goto miocnak;
		}

		/*
		 * try to find a waiting connection for this stream
		 */
		for (ap = ifp->if_conn; ap; ap = ap->next) {
			if (ap->rq) {
				continue;
			}
			if (EQUAL(&ap->dst, &stp->dst)) {
				break;
			}
		}

		/*
		 * Not found? So get a new lower stream entry
		 */
		if (ap == NULL) {
			IPD_DEBUG("I_LINK: allocating new entry\n");
			ap = ipd_add_entry((caddr_t)&stp->dst, stp->st_type ==
				IPD_PTP? 0 : IPD_MTP_ADDRL, &ifp->if_conn);
		}

		if (ap == (struct ipd_addr_tbl *)NULL) {
			mutex_exit(&ipd_mutex);
			err = EAGAIN;
			goto miocnak;
		}

		/*
		 * and stuff details of this stream into it
		 */
		lp		= (struct linkblk *)mp->b_cont->b_rptr;
		ap->rq		= RD(lp->l_qbot);
		ap->mux_id	= lp->l_index;
		ap->timeout	= ap->addr_timeout = -1; /* default static */
		ap->ifp		= ifp;

		/*
		 * and stuff the entry into the stream
		 */
		ap->rq->q_ptr = WR(ap->rq)->q_ptr = (void *)ap;

		/*
		 * send any packets that were queued
		 */
		while (ap->pkt) {
			IPD_DEBUG("ipd_ioctl: sending queued packet\n");
			nmp = ap->pkt;
			ap->pkt = nmp->b_next;
			nmp->b_next = NULL;
			if (ipd_xmitpkt(WR(ap->rq), nmp, ap)) {
				freemsg(nmp);
			}
		}

		mutex_exit(&ipd_mutex);

		/*
		 * re-enable just in case we had a message which
		 * was blocked before the I_LINK completed
		 */
		enableok(ap->rq);
		enableok(WR(ap->rq));

		miocack(wq, mp, 0, 0);
		break;

	case	I_UNLINK:

		IPD_DEBUG1("ipd_ioctl: I_UNLINK on q=%p\n", RD(wq));

		mutex_enter(&ipd_mutex);

		stp = (struct ipd_str *)wq->q_ptr;

		/*
		 * cannot unlink a stream under because no interface attached
		 */
		ifp = stp->st_ifp;

		if (ifp == NULL) {
			mutex_exit(&ipd_mutex);
			err = ENOLINK;
			goto miocnak;
		}

		lp = (struct linkblk *)mp->b_cont->b_rptr;

		/*
		 * find the corresponding stream
		 */
		for (ap = ifp->if_conn; ap; ap = ap->next) {
			if (ap->mux_id == lp->l_index) {
				break;
			}
		}

		if (ap == NULL) {
			mutex_exit(&ipd_mutex);
			err = ENODEV;
			goto miocnak;
		}

		ASSERT(ap->rq);

		ipd_remove_entry(&ifp->if_conn, ap);
		miocack(wq, mp, 0, 0);

		/*
		 * re-enable upper streams in case one was waiting to be
		 * back-enabled by the unlinked stream
		 */
		for (stp = ipd_strup; stp; stp = stp->st_next) {
			qenable(stp->st_rq);
			qenable(WR(stp->st_rq));
		}

		mutex_exit(&ipd_mutex);
		break;

miocnak:
	default:
		miocnak(wq, mp, 0, err);
	}
}



/*
 * ipd_dlattachreq()
 *
 * perform DLPI attach request
 *
 * Returns: response sent upstream
 */
static int
ipd_dlattachreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	union	DL_primitives	*dlp;
	int			ppa;
	struct ipd_softc	*ifp;

	dlp = (union DL_primitives *)mp->b_rptr;
	ppa = dlp->attach_req.dl_ppa;

	IPD_DLPI3("ipd_dlattachreq: stp=%p, ppa=%d state=%d\n", stp,
			ppa, stp->st_state);


	/*
	 * valid PPA (unit number)?  This is measured against the max configured
	 * for each interface type.
	 */
	if (ppa >= (stp->st_type == IPD_MTP ? ipd_nmtp : ipd_nptp)) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_BADPPA, 0);
		return (0);
	}

	/*
	 * try to find if this is the first reference to the interface
	 * If so, we will create the interface.	 Otherwise we will use the
	 * existing interface.
	 */
	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {

		if (ifp->if_type != stp->st_type) {
			continue;
		}
		if (ppa == ifp->if_unit) {
			break;
		}
	}

	if (ifp == NULL) {

		/*
		 * time to allocate a new interface
		 */
		ifp = (struct ipd_softc *)kmem_zalloc(sizeof (*ifp),
								KM_NOSLEEP);

		if (ifp == NULL) {
			dlerrorack(wq, mp, dlp->dl_primitive, DL_BADPPA, 0);
			return (0);
		}

		ifp->if_type = stp->st_type;
		ifp->if_unit = ppa;

		switch (ifp->if_type) {

		case IPD_MTP:
			ifp->if_name = IPD_MTP_NAME;
			ifp->if_dip = ipd_mtp_dip;
			break;

		case IPD_PTP:
			ifp->if_name = IPD_PTP_NAME;
			ifp->if_dip = ipd_ptp_dip;
			break;
		}

		/*
		 * make interface statistics available to the kernel
		 */
		ipd_initstats(ifp);

		/*
		 * insert new interface in list
		 */
		if (ipd_ifs) {
			ifp->if_next = ipd_ifs;
			ipd_ifs = ifp;
		} else {
			ipd_ifs = ifp;
		}
	}

	/*
	 * Point to attached device and update our state.
	 */
	stp->st_ifp = ifp;
	stp->st_state = DL_UNBOUND;

	/*
	 * Return DL_OK_ACK response.
	 */
	dlokack(wq, mp, DL_ATTACH_REQ);

	return (0);
}

/*
 * ipd_strdetach()
 *
 * detach this stream from an interface
 *
 * Returns: none
 */
static void
ipd_strdetach(struct ipd_str *stp)
{

	stp->st_ifp = NULL;
	stp->st_state = DL_UNATTACHED;
}

/*
 * ipd_dldetachreq()
 *
 * perform DLPI detach request
 *
 * Returns: response sent upstream
 */
static int
ipd_dldetachreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	IPD_DLPI2("ipd_dldetachreq: stp=%p, state=%d\n", stp, stp->st_state);

	ipd_strdetach(stp);

	dlokack(wq, mp, DL_DETACH_REQ);

	IPD_DLPI2("ipd_dldetachreq: done. stp=%p, state=%d\n", stp,
								stp->st_state);
	return (0);
}


/*
 * ipd_dlbindreq()
 *
 * perform DLPI bind request
 *
 * Returns: response sent upstream
 */
static int
ipd_dlbindreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	union DL_primitives	*dlp;
	struct ipd_softc	*ifp;
	t_uscalar_t		sap;

	dlp = (union DL_primitives *)mp->b_rptr;
	ifp = stp->st_ifp;
	sap = dlp->bind_req.dl_sap;

	IPD_DLPI3("ipd_dlbindreq: stp=%p, state=%d sap=%d\n",
					stp, stp->st_state, sap);

#if 0 /* jupiter fcs bug */
	if (dlp->bind_req.dl_service_mode != DL_CLDLS) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_UNSUPPORTED, 0);
		return (0);
	}
#endif

	ASSERT(ifp);

#if 0
	/*
	 * even with dl_sap_length of 0 (IPD_SAPL), IP insists on sending
	 * down a sap.	How do you say you have no sap?	 sap=0x0000?
	 */
	if (sap > IPD_SAPL) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_BADSAP, 0);
		return (0);
	}
#endif

	/*
	 * Save SAP value for this Stream and change state.
	 */
	stp->st_sap = sap;
	stp->st_state = DL_IDLE;

	/*
	 * Return DL_BIND_ACK response.
	 */
	dlbindack(wq, mp, 0, NULL, IPD_SAPL, 0, 0);

	IPD_DLPI2("ipd_dlbindreq: done stp=%p, state=%d\n", stp, stp->st_state);

	return (0);
}

/*
 * ipd_dlunbindreq()
 *
 * perform DLPI unbind request
 *
 * Returns: response sent upstream
 */
static int
ipd_dlunbindreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	IPD_DLPI2("ipd_dlunbindreq: stp=%p, state=%d\n", stp, stp->st_state);

	/*
	 * the DLPI spec says to flush messages on unbind
	 */
	flushq(wq, FLUSHALL);
	flushq(RD(wq), FLUSHALL);

	/*
	 * Change state.
	 */
	stp->st_state = DL_UNBOUND;

	/*
	 * Return DL_OK_ACK response.
	 */
	dlokack(wq, mp, DL_UNBIND_REQ);

	IPD_DLPI2("ipd_dlunbindreq: done. stp=%p, state=%d\n", stp,
								stp->st_state);

	return (0);
}

/*
 * ipd_dlinforeq()
 *
 * perform DLPI info request
 *
 * Returns: response sent upstream
 */
static int
ipd_dlinforeq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	dl_info_ack_t	*dlip;
	int		size, addr_size;

	IPD_DLPI2("ipd_dlinforeq: stp=%p, state=%d\n", stp, stp->st_state);

	/*
	 * Exchange current msg for a DL_INFO_ACK.
	 */
	addr_size = stp->st_type == IPD_PTP? IPD_PTP_ADDRL : IPD_MTP_ADDRL;
	size = sizeof (dl_info_ack_t) + addr_size;
	if ((mp = mexchange(wq, mp, size, M_PCPROTO, DL_INFO_ACK)) == NULL) {
		return (0);
	}

	/*
	 * Fill in the DL_INFO_ACK fields and reply.
	 */
	dlip = (dl_info_ack_t *)mp->b_rptr;

	*dlip = stp->st_type == IPD_MTP? ipd_mtp_infoack : ipd_ptp_infoack;
	dlip->dl_current_state = stp->st_state;

	if (addr_size) {
		if (stp->dst) {
			bcopy((caddr_t)&stp->dst, (caddr_t)mp->b_rptr + size,
								addr_size);
		} else {
			bzero((caddr_t)mp->b_rptr+size, addr_size);
		}
	}

	qreply(wq, mp);

	return (0);
}

/*
 * ipd_dlunitdatareq()
 *
 * Send an IP unit datagram
 *
 * Returns: 0 if processing can continue, non-zero if in flow control
 */
static int
ipd_dlunitdatareq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	struct ipd_addr_tbl	*ap, *p;
	struct ipd_softc	*ifp = stp->st_ifp;
	int			addr_len, off, len, i;
	mblk_t			*nmp;
	ipaddr_t		dst;
	dl_unitdata_req_t	*dludp;


	IPD_DLPI2("ipd_dlunitdatareq: stp=%p, state=%d\n", stp, stp->st_state);

	/*
	 * no interface attached?
	 */

	if (ifp == NULL) {
		dluderrorind(wq, mp, NULL, 0, DL_SYSERR, ENETDOWN);
		return (0);
	}

	dludp = (dl_unitdata_req_t *)mp->b_rptr;

	off = dludp->dl_dest_addr_offset;
	len = dludp->dl_dest_addr_length;

	/*
	 * Validate destination address format.
	 */
	addr_len = stp->st_type == IPD_MTP? IPD_MTP_ADDRL : IPD_PTP_ADDRL;
	if (!MBLKIN(mp, off, len) || (len != addr_len)) {
		dluderrorind(wq, mp, mp->b_rptr + off, len, DL_BADADDR, 0);
		return (0);
	}

	/*
	 * Error if no M_DATA follows.
	 */
	if (mp->b_cont == NULL) {
		dluderrorind(wq, mp, mp->b_rptr + off, len, DL_BADDATA, 0);
		return (0);
	}

	/*
	 * checks complete, now to work...
	 */

	if (len == 0) {

		/*
		 * Point-to-point Interface
		 */

		/*
		 * select first connection regardless of destination addr
		 */
		ap = ifp->if_conn;

	} else {

		/*
		 * Point-to-Multipoint Interface
		 */

		ASSERT(len == IPD_MTP_ADDRL);

		(void) bcopy((caddr_t)dludp+off, (caddr_t)&dst, len);

		/*
		 * Find the circuit connected to the destination address.
		 *
		 * Use the last destination in the list and then rotate list.
		 *
		 * XXX this really should be implemented as a hash or similar
		 * Not too much of a problem for basic rate ISDN's
		 * two B-channels but less than best for primary rate
		 * interfaces with many B-channels
		 */
		ap = NULL;
		for (p = ifp->if_conn; p; p = p->next) {

			/*
			 * try to match the destination address
			 */
			if (p->dst == dst) {
				ap = p;
			}
		}
	}

	/*
	 * rotate the connection table to multiplex one IP destination over
	 * several connections...
	 */
	round_robin(&ifp->if_conn, ap);


	/*
	 * if we found a destination which matched, and it has a connection
	 * send the packet and go
	 */
	if (ap && ap->rq) {

		return (ipd_xmitpkt(WR(ap->rq), mp, ap));

	}

	/*
	 * At this point, we used to check for a connection manager, now
	 * the connection manager may be registered for this connection
	 * or an unregistered cnx may service the request.  In either case
	 * we'll call ipd_tellcm to send a request.  If no cnx responds
	 * timeout will handle the error on the interface.
	 */


	/*
	 * ap may or may not be NULL depending on whether we have already
	 * hit the connection manager for a new connection
	 */

	if (ap == NULL) {

		/*
		 * no request has been made to the connection manager,
		 * so do it now.  Add the connection tentatively to list
		 */

		ap = ipd_add_entry((caddr_t)&dst, len, &ifp->if_conn);

		if (ap == NULL) {
			/*
			 * Memory allocation failure?
			 */

			IPD_DEBUG2("ipd_dlunitdatareq: %s%d allocate failed\n",
					ifp->if_name, ifp->if_unit);
			dluderrorind(wq, mp, NULL, 0, DL_SYSERR, ENOMEM);
			return (0);
		}

		/*
		 * hold the packet for the specified time
		 */
		ap->ifp = ifp;
		ap->addr_timeout = ap->timeout = IPD_HOLDING_TIME;

		/*
		 * request a new connection from the connection manager
		 */
		ipd_tellcm(ap, IPD_CON_REQ, 0);
	}

	/*
	 * if ap wasn't	 NULL - the connection request has already been made
	 */

	/*
	 * queue the packet up to a maximum of IPD_MAX_PKTS
	 */

	ASSERT(mp->b_next == NULL);

	if (!ap->pkt) {
		ap->pkt = mp;
	} else {
		nmp = ap->pkt;
		i = 1;
		while (nmp->b_next != NULL) {
			nmp = nmp->b_next;
			i++;
		}
		if (i < IPD_MAX_PKTS) {
			nmp->b_next = mp;
		} else {
			/*
			 * packet queue overflow - drop the packet silently
			 */
			IPD_DEBUG("ipd_dlunitdatareq: holding q overflow\n");
			freemsg(mp);
		}
	}
	return (0);
}



/*
 * ipd_xmitpkt()
 *
 * remove the DLPI header and then transmit a packet on a lower stream
 *
 * Returns: 0 on success, error if transmission was not possible
 */
static int
ipd_xmitpkt(queue_t *wq, mblk_t *mp, struct ipd_addr_tbl *ap)
{
	mblk_t		*nmp;

	ASSERT(ap);

	if (canputnext(wq)) {

		IPD_DATADBG1("ipd_xmitpkt: wq=%p\n", wq);

		/*
		 * note there has been activity on this interface
		 */
		ap->act_count++;
		ap->act_time = hrestime;

		/*
		 * discard the DLPI header and send the packet
		 */
		nmp = mp->b_cont; mp->b_cont = NULL; freemsg(mp);

		/*
		 * copy the packet up to snoop streams
		 */
		if (mp = dupmsg(nmp)) {
			ipd_sendup(ap->ifp, mp, find_promisc);
		} else {
			ap->ifp->if_allocbfail++;
		}

		putnext(wq, nmp);

		ap->ifp->if_opackets++;

	} else {
		/*
		 * in flow control, signal the sender to stop
		 */
		IPD_FLDEBUG1("ipd_xmitpkt: wq=%p is in flow control\n", wq);

		ap->ifp->if_nocanput++;
		return (ENOSPC);
	}
	return (0);
}


/*
 * ipd_dlpromisconreq()
 *
 * Make this interface stream promiscuous.  Needed for snoop support
 *
 * Returns: DL_OK_ACK upstream
 */
static int
ipd_dlpromisconreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	stp->st_all = 1;
	dlokack(wq, mp, DL_PROMISCON_REQ);
	return (0);
}


/*
 * ipd_dlpromiscoffreq()
 *
 * Make this interface stream non-promiscuous
 *
 * Returns: DL_OK_ACK upstream
 */
static int
ipd_dlpromiscoffreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	stp->st_all = 0;
	dlokack(wq, mp, DL_PROMISCOFF_REQ);
	return (0);
}


/*
 * ipd_dlphyreq()
 *
 * Return this interface's address.  Doesn't return anything useful, but keeps
 * ifconfig happy
 *
 * Returns: DL_PHYS_ADDR_ACK upstream
 */
/*ARGSUSED*/
static int
ipd_dlphyreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	static struct ether_addr	addr = {0};

	dlphysaddrack(wq, mp, (char *)&addr, ETHERADDRL);
	return (0);
}


/*
 * ipd_dlsetphyreq()
 *
 * Set this interface's address.  Not really a physical address,
 * but a logical address.
 *
 * Returns: DL_OK_ACK upstream
 */
static int
ipd_dlsetphyreq(struct ipd_str *stp, queue_t *wq, mblk_t *mp)
{
	dl_set_phys_addr_req_t	*dlsetphy;
	int			off, len;
	struct sockaddr_in	sin;

	dlsetphy = (dl_set_phys_addr_req_t *)mp->b_rptr;
	off = dlsetphy->dl_addr_offset;
	len = dlsetphy->dl_addr_length;

	if (!MBLKIN(mp, off, len)) {

		dlerrorack(wq, mp, DL_SET_PHYS_ADDR_REQ, DL_BADADDR, 0);
		return (0);
	}
	if (len) {
		(void) bcopy((caddr_t)dlsetphy+off, (caddr_t)&sin, len);
		(void) bcopy((caddr_t)&sin.sin_addr, (caddr_t)&stp->dst,
						sizeof (stp->dst));
	}

	dlokack(wq, mp, DL_SET_PHYS_ADDR_REQ);
	return (0);
}


/*
 * round_robin()
 *
 * move the entry specified to the head of the address resolution table
 *
 * Returns: none
 */

static void
round_robin(struct ipd_addr_tbl **root, struct ipd_addr_tbl *item)
{
	if (item == NULL) {
		return;
	}

	/*
	 * remove address entry from the list
	 */
	if (item->next != NULL)
		item->next->prev = item->prev;

	if (item->prev != NULL)
		item->prev->next = item->next;
	else
		*root = item->next;

	if (*root) {

		item->next = *root;
		item->prev = NULL;
		(*root)->prev = item;
		*root = item;

	} else {
		*root = item;
		item->next = item->prev = NULL;
	}

}


/*
 * Return TRUE if the given type matches the given sap.
 */
static int
sapmatch(int sap, int type)
{
	if (sap == type)
		return (1);

	if (sap == 0)
		return (1);

	if ((sap < ETHERMTU) && (type < ETHERMTU))
		return (1);

	return (0);
}

/*
 * Test upstream destination sap and interface match.
 */
static struct ipd_str *
find_all(struct ipd_str *stp, struct ipd_softc *ifp, int type)
{
	for (; stp; stp = stp->st_next)
		if ((stp->st_ifp == ifp) &&
				sapmatch(stp->st_sap, type) &&
				stp->st_state == DL_IDLE) {
			return (stp);
		}

	return (NULL);
}


/*
 * Test upstream destination sap and interface match and stream is promiscuous.
 */
static struct ipd_str *
find_promisc(struct ipd_str *stp, struct ipd_softc *ifp, int type)
{
	for (; stp; stp = stp->st_next)
		if (stp->st_all &&
				(stp->st_ifp == ifp) &&
				sapmatch(stp->st_sap, type) &&
				stp->st_state == DL_IDLE) {
			return (stp);
		}

	return (NULL);
}



/*
 * Prefix msg with a DL_UNITDATA_IND mblk and return the new msg.
 */
static mblk_t *
ipd_addudind(struct ipd_softc *ifp, mblk_t *mp)
{
	dl_unitdata_ind_t	*dludindp;
	mblk_t			*nmp;
	size_t			size;

	/*
	 * Allocate an M_PROTO mblk for the DL_UNITDATA_IND.
	 */
	size = sizeof (dl_unitdata_ind_t);
	if ((nmp = allocb(size, BPRI_LO)) == NULL) {
		freemsg(mp);
		ifp->if_allocbfail++;
		return (NULL);
	}
	DB_TYPE(nmp) = M_PROTO;
	nmp->b_wptr = nmp->b_datap->db_lim;
	nmp->b_rptr = nmp->b_wptr - size;

	/*
	 * Construct a DL_UNITDATA_IND primitive.
	 */
	dludindp = (dl_unitdata_ind_t *)nmp->b_rptr;
	dludindp->dl_primitive = DL_UNITDATA_IND;
	dludindp->dl_dest_addr_length = 0;
	dludindp->dl_dest_addr_offset = sizeof (dl_unitdata_ind_t);
	dludindp->dl_src_addr_length = 0;
	dludindp->dl_src_addr_offset = sizeof (dl_unitdata_ind_t);
	dludindp->dl_group_address = 0;

	/*
	 * Link the M_PROTO and M_DATA together.
	 */
	nmp->b_cont = mp;
	return (nmp);
}

/*
 * add_etherheader()
 *
 * Add an empty ethernet/IP header for tracing programs such as
 * snoop.
 *
 * Returns: updated message pointer on success, frees message and returns
 * NULL on error.
 */
static mblk_t *
add_etherhdr(struct ipd_softc *ifp, mblk_t *mp, int type)
{
	mblk_t		*hp;

	hp = allocb(sizeof (struct ether_header), BPRI_HI);
	if (hp == NULL) {
		freemsg(mp);
		ifp->if_allocbfail++;
		return (NULL);
	}
	hp->b_wptr += sizeof (struct ether_header);
	bzero((caddr_t)hp->b_rptr, sizeof (struct ether_header));
	((struct ether_header *)hp->b_rptr)->ether_type = htons((short)type);
	hp->b_cont = mp;
	return (hp);
}



/*
 * ipd_sendup()
 *
 * Send packet upstream to any stream which matches acceptfunc crtieria.
 */
static void
ipd_sendup(struct ipd_softc *ifp, mblk_t *mp, struct ipd_str *(*acceptfunc)())
{
	int			type;
	struct ipd_str		*stp, *nstp;
	mblk_t			*nmp;

	type = ETHERTYPE_IP;

	if ((stp = (*acceptfunc)(ipd_strup, ifp, type)) == NULL) {
		freemsg(mp);
		return;
	}

	for (; nstp = (*acceptfunc)(stp->st_next, ifp, type); stp = nstp) {

		if (canputnext(stp->st_rq)) {
			if (nmp = dupmsg(mp)) {
				if (stp->st_raw) {
					if ((nmp =
						add_etherhdr(ifp, nmp, type))) {
						putnext(stp->st_rq, nmp);
					}
				} else {
					if ((nmp =
						ipd_addudind(ifp, nmp))) {
						putnext(stp->st_rq, nmp);
					}
				}
			IPD_DATADBG1("ipd_sendup: sent to q %p\n", stp->st_rq);
			}
		} else {
			IPD_FLDEBUG1("ipd_sendup: dropped data q %p\n",
								stp->st_rq);
			ifp->if_nocanput++;
		}
	}

	if (canputnext(stp->st_rq)) {
		if (stp->st_raw) {
			if ((mp = add_etherhdr(ifp, mp, type))) {
				putnext(stp->st_rq, mp);
			}
		} else {
			if ((mp = ipd_addudind(ifp, mp))) {
				putnext(stp->st_rq, mp);
			}
		}
		IPD_DATADBG1("ipd_sendup: sent data to q %p\n", stp->st_rq);
	} else {
		IPD_FLDEBUG1("ipd_sendup: dropped data to q %p\n", stp->st_rq);
		ifp->if_nocanput++;
	}
}

/*
 * Initialize ipdpriminfo[].
 */
static void
ipdpriminfoinit(void)
{
	if ((ipdpriminfo = GETSTRUCT(struct ipdpriminfo, DL_MAXPRIM)) == NULL)
		panic("ipdpriminfoinit:	 kmem_alloc failed");

	ipdpriminfo[DL_ATTACH_REQ].pi_minlen = sizeof (dl_attach_req_t);
	ipdpriminfo[DL_ATTACH_REQ].pi_state = DL_UNATTACHED;
	ipdpriminfo[DL_ATTACH_REQ].pi_funcp = ipd_dlattachreq;

	ipdpriminfo[DL_DETACH_REQ].pi_minlen = sizeof (dl_detach_req_t);
	ipdpriminfo[DL_DETACH_REQ].pi_state = DL_UNBOUND;
	ipdpriminfo[DL_DETACH_REQ].pi_funcp = ipd_dldetachreq;

	ipdpriminfo[DL_BIND_REQ].pi_minlen = sizeof (dl_bind_req_t);
	ipdpriminfo[DL_BIND_REQ].pi_state = DL_UNBOUND;
	ipdpriminfo[DL_BIND_REQ].pi_funcp = ipd_dlbindreq;

	ipdpriminfo[DL_UNBIND_REQ].pi_minlen = sizeof (dl_unbind_req_t);
	ipdpriminfo[DL_UNBIND_REQ].pi_state = DL_IDLE;
	ipdpriminfo[DL_UNBIND_REQ].pi_funcp = ipd_dlunbindreq;

	ipdpriminfo[DL_INFO_REQ].pi_minlen = sizeof (dl_info_req_t);
	ipdpriminfo[DL_INFO_REQ].pi_state = 0;	/* special handling */
	ipdpriminfo[DL_INFO_REQ].pi_funcp = ipd_dlinforeq;

	ipdpriminfo[DL_UNITDATA_REQ].pi_minlen = sizeof (dl_unitdata_req_t);
	ipdpriminfo[DL_UNITDATA_REQ].pi_state = DL_IDLE;
	ipdpriminfo[DL_UNITDATA_REQ].pi_funcp = ipd_dlunitdatareq;

	ipdpriminfo[DL_PROMISCON_REQ].pi_minlen = sizeof (dl_promiscon_req_t);
	ipdpriminfo[DL_PROMISCON_REQ].pi_state = DL_UNBOUND;
	ipdpriminfo[DL_PROMISCON_REQ].pi_funcp = ipd_dlpromisconreq;

	ipdpriminfo[DL_PROMISCOFF_REQ].pi_minlen = sizeof (dl_promiscoff_req_t);
	ipdpriminfo[DL_PROMISCOFF_REQ].pi_state = 0; /* special handling */
	ipdpriminfo[DL_PROMISCOFF_REQ].pi_funcp = ipd_dlpromiscoffreq;

	ipdpriminfo[DL_PHYS_ADDR_REQ].pi_minlen = sizeof (dl_phys_addr_req_t);
	ipdpriminfo[DL_PHYS_ADDR_REQ].pi_state = 0; /* special handling */
	ipdpriminfo[DL_PHYS_ADDR_REQ].pi_funcp = ipd_dlphyreq;

	ipdpriminfo[DL_SET_PHYS_ADDR_REQ].pi_minlen =
		sizeof (dl_set_phys_addr_req_t) + sizeof (struct sockaddr);
	ipdpriminfo[DL_SET_PHYS_ADDR_REQ].pi_state = DL_UNBOUND;
	ipdpriminfo[DL_SET_PHYS_ADDR_REQ].pi_funcp = ipd_dlsetphyreq;
}
