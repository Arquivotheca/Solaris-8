/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Tunnel driver
 * This module acts like a driver/DLPI provider as viewed from the top
 * and a stream head/TPI user from the bottom
 * Implements the logic for IP (IPv4 or IPv6) encapsulation
 * within IP (IPv4 or IPv6)
 */

#pragma ident	"@(#)tun.c	1.8	99/10/06 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ethernet.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/vtrace.h>
#include <sys/isa_defs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <netinet/in.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/snmpcom.h>

#include <netinet/igmp_var.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <net/if_dl.h>
#include <inet/ip_if.h>
#include <sys/strsun.h>
#include <inet/tun.h>


#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/stat.h>

static void	tun_cancel_rec_evs(queue_t *, eventid_t *);
static void	tun_bufcall_handler(void *);
static boolean_t tun_icmp_message_v4(queue_t *, ipha_t *, icmph_t *, mblk_t *);
static boolean_t tun_icmp_too_big_v4(queue_t *, ipha_t *, uint16_t, mblk_t *);
static boolean_t	tun_icmp_message_v6(queue_t *, ip6_t *, icmp6_t *,
			    uint8_t, mblk_t *);
static boolean_t	tun_icmp_too_big_v6(queue_t *, ip6_t *, uint32_t,
			    uint8_t, mblk_t *);
static int	tun_sendokack(queue_t *, mblk_t *, t_uscalar_t);
static int	tun_fastpath(queue_t *q, mblk_t *mp);
static int	tun_ioctl(queue_t *, mblk_t  *);
static void	tun_timeout_handler(void *);
static int	tun_rproc(queue_t *, mblk_t  *);
static int	tun_wproc_mdata(queue_t *q, mblk_t *mp);
static int	tun_wproc(queue_t *, mblk_t  *);
static int	tun_rdata_v4(queue_t *, mblk_t  *);
static int	tun_rdata_v6(queue_t *, mblk_t  *);
static int	tun_send_sec_req(queue_t *, mblk_t *);
static mblk_t	*tun_realloc_mblk(queue_t *, mblk_t *, size_t, mblk_t *,
		    boolean_t);
static void	tun_recover(queue_t *, mblk_t *, size_t);
static void	tun_rem_list(tun_t *);
static void	tun_rput_icmp_err_v4(queue_t *, mblk_t  *);
static void	icmp_ricmp_err_v4_v4(queue_t *, mblk_t *, icmph_t *);
static void	icmp_ricmp_err_v6_v4(queue_t *, mblk_t *, icmph_t *);
static void	tun_rput_icmp_err_v6(queue_t *, mblk_t  *);
static int	tun_rput_tpi(queue_t *, mblk_t  *);
static int	tun_send_bind_req(queue_t *, mblk_t *);
static void	tun_statinit(tun_stats_t *, char *);
static int	tun_stat_kstat_update(kstat_t *ksp, int rw);
static int	tun_wdata_v4(queue_t *, mblk_t  *);
static int	tun_wdata_v6(queue_t *, mblk_t  *);
static char	*tun_who(queue_t *, char *);
static int	tun_wput_dlpi(queue_t *, mblk_t  *);
static int	tun_wputnext_v6(queue_t *, mblk_t *);
static int	tun_wputnext_v4(queue_t *, mblk_t *);

/* module's defined constants, globals and data structures */

#define	SUPPORTED	1
#define	NOT_SUPPORTED	0
#define	LOWER_V6	NOT_SUPPORTED
#define	UPPER_V6	SUPPORTED
#define	LOWER_V4	SUPPORTED
#define	UPPER_V4	SUPPORTED

#define	IP	"ip"
#define	IP6	"ip6"
static major_t	IP_MAJ;
#if (LOWER_V6 == SUPPORTED)
static major_t	IP6_MAJ;
#endif

#define	TUN_LINK_EXTRA_OFF	32

#define	IPV6V4_DEF_TTL		60
#define	IPV6V4_DEF_ENCAP	60

#define	TUN_WHO_BUF		60


#ifdef DEBUG
#define	TUN_DBGERR	0x1
#define	TUN_DBG1	0x2
#define	TUN_DBG2	0x4
#define	TUN_DBG3	0x8
int	tundebug = TUN_DBGERR;	/* debug flag */
#define	TUNDEBUG(level, what) if (tundebug & (level)) (void) printf what
#else
#define	TUNDEBUG(level, what)
#endif
#define	TUN_RECOVER_WAIT		(1*hz)


/* canned DL_INFO_ACK  - adjusted based on tunnel type */
dl_info_ack_t infoack = {
	DL_INFO_ACK,	/* dl_primitive */
	4196,		/* dl_max_sdu */
	0,		/* dl_min_sdu */
	0,		/* dl_addr_length */
	DL_OTHER,	/* dl_mac_type */	/* XXX DL_IP someday */
	0,		/* dl_reserved */
	DL_UNATTACHED,	/* dl_current_state */
	0,		/* dl_sap_length */
	DL_CLDLS,	/* dl_service_mode */
	0,		/* dl_qos_length */
	0,		/* dl_qos_offset */
	0,		/* dl_qos_range_length */
	0,		/* dl_qos_range_offset */
	DL_STYLE2,	/* dl_provider_style */
	0,		/* dl_addr_offset */
	DL_VERSION_2,	/* dl_version */
	0,		/* dl_brdcast_addr_length */
	0,		/* dl_brdcst_addr_offset */
	0		/* dl_grow */
};

/*
 * canned DL_BIND_ACK - IP doesn't use any of this info.
 */
dl_bind_ack_t bindack = {
	DL_BIND_ACK,	/* dl_primitive */
	0,		/* dl_sap */
	0,		/* dl_addr_length */
	0,		/* dl_addr_offset */
	0,		/* dl_max_conind */
	0		/* dl_xidtest_flg */
};

/*
 * Linked list of tunnels.
 */

#define	TUN_PPA_SZ	64
#define	TUN_LIST_HASH(ppa)	((ppa) % TUN_PPA_SZ)

/*
 * protects global data structures such as tun_ppa_list
 * also protects tun_t at ts_next and *ts_atp
 * should be acquired before ts_lock
 */
static kmutex_t		tun_global_lock;
static tun_stats_t	*tun_ppa_list[TUN_PPA_SZ];
static tun_stats_t	*tun_add_stat(queue_t *);
static boolean_t tun_do_fastpath = B_TRUE;

/* streams linkages */
static struct module_info info = {
	TUN_MODID,	/* module id number */
	TUN_NAME,	/* module name */
	1,		/* min packet size accepted */
	INFPSZ,		/* max packet size accepted */
	65536,		/* hi-water mark */
	1024		/* lo-water mark */
};

static struct qinit tunrinit = {
	(pfi_t)tun_rput,	/* read side put procedure */
	(pfi_t)tun_rsrv,	/* read side service procedure */
	tun_open,		/* open procedure */
	tun_close,		/* close procedure */
	NULL,			/* for future use */
	&info,			/* module information structure */
	NULL			/* module statistics structure */
};

static struct qinit tunwinit = {
	(pfi_t)tun_wput,	/* write side put procedure */
	(pfi_t)tun_wsrv,	/* write side service procedure */
	NULL,
	NULL,
	NULL,
	&info,
	NULL
};

struct streamtab tuninfo = {
	&tunrinit,		/* read side queue init */
	&tunwinit,		/* write side queue init */
	NULL,			/* mux read side init */
	NULL			/* mux write side init */
};

static struct fmodsw tun_fmodsw = {
	TUN_NAME,
	&tuninfo,
	(D_NEW | D_MP | D_MTQPAIR | D_MTPUTSHARED)
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"configured tunneling module",
	&tun_fmodsw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *) &modlstrmod,
	NULL
};

int
_init(void)
{
	int	rc;

	IP_MAJ = ddi_name_to_major(IP);
#if (LOWER_V6 == SUPPORTED)
	IP6_MAJ = ddi_name_to_major(IP6);
#endif
	rc = mod_install(&modlinkage);
	if (rc == 0) {
		mutex_init(&tun_global_lock, NULL, MUTEX_DEFAULT, NULL);
	}
	return (rc);
}

int
_fini(void)
{
	int	rc;

	rc = mod_remove(&modlinkage);
	if (rc == 0) {
		mutex_destroy(&tun_global_lock);
	}
	return (rc);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * this module is meant to be pushed on an instance of IP and
 * have an instance of IP pushed on top of it.
 */

/* ARGSUSED */
int
tun_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	tun_t	*atp;

	if (q->q_ptr) {
		/* re-open of an already open instance */
		return (0);
	}

	if (sflag != MODOPEN) {
		return (EINVAL);
	}

	TUNDEBUG(TUN_DBG1, ("tun_open\n"));

	/* allocate per-instance structure */
	atp = kmem_zalloc(sizeof (tun_t), KM_SLEEP);

	atp->tun_state = DL_UNATTACHED;
	atp->tun_dev = *devp;

	/*
	 * Based on the lower version of IP, initialize stuff that
	 * won't change
	 */
	if (getmajor(*devp) == IP_MAJ) {
		ipha_t *ipha;

		atp->tun_flags = TUN_L_V4;
		/* XXX IPv4 */
		atp->tun_hop_limit = IPV6V4_DEF_TTL;

		/*
		 * If an interface with a larger MTU is brought up
		 * after the tunnel, we do not take advantage of its
		 * mtu
		 */
		atp->tun_mtu = MAX(IPV6_MIN_MTU, ip_max_mtu - sizeof (ipha_t));
		ipha = &atp->tun_ipha;
		ipha->ipha_version_and_hdr_length = IP_SIMPLE_HDR_VERSION;
		ipha->ipha_type_of_service = 0;
		ipha->ipha_ident = 0;		/* to be filled in by IP */
		ipha->ipha_fragment_offset_and_flags = htons(IPH_DF);
		ipha->ipha_ttl = atp->tun_hop_limit;
		ipha->ipha_hdr_checksum = 0;	/* to be filled in by IP */
#if (LOWER_V6 == SUPPORTED)
	} else if (getmajor(*devp) == IP6_MAJ) {
		atp->tun_flags = TUN_L_V6;
		atp->tun_hop_limit = IPV6_DEFAULT_HOPS;
		atp->tun_encap_lim = IPV6_DEF_ENCAP;
		/*
		 * If an interface with a larger MTU is brought up
		 * after the tunnel, we do not take advantage of its
		 * mtu
		 */
		atp->tun_mtu = MAX(IPV6_MIN_MTU, ip_max_mtu - sizeof (ip6_t));
		atp->tun_ip6h.ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
		atp->tun_ip6h.ip6_hops = IPV6_DEFAULT_HOPS;
#endif
	} else {
		kmem_free(atp, sizeof (tun_t));
		return (ENXIO);
	}

	atp->tun_extra_offset = TUN_LINK_EXTRA_OFF;
	mutex_init(&atp->tun_lock, NULL, MUTEX_DEFAULT, NULL);

	if (q->q_qinfo->qi_minfo->mi_idnum == ATUN_MODID) {
		atp->tun_flags |= TUN_AUTOMATIC;
	}

	q->q_ptr = WR(q)->q_ptr = atp;
	qprocson(q);
	return (0);
}

/* ARGSUSED */
int
tun_close(queue_t *q, int flag, cred_t *cred_p)
{
	tun_t *atp = (tun_t *)q->q_ptr;

	ASSERT(atp != NULL);

	/* Cancel outstanding qtimeouts() or qbufcalls() */
	tun_cancel_rec_evs(q, &atp->tun_events);

	qprocsoff(q);

	if (atp->tun_stats != NULL)
		tun_rem_list(atp);

	mutex_destroy(&atp->tun_lock);

	/* free per-instance struct  */
	kmem_free(atp, sizeof (tun_t));

	q->q_ptr = WR(q)->q_ptr = NULL;

	return (0);
}


/*
 * Cancel bufcall and timer requests
 * Don't need to hold lock. protected by perimeter
 */
static void
tun_cancel_rec_evs(queue_t *q, eventid_t *evs)
{
	if (evs->ev_rbufcid != 0) {
		qunbufcall(RD(q), evs->ev_rbufcid);
		evs->ev_rbufcid = 0;
	}
	if (evs->ev_wbufcid != 0) {
		qunbufcall(WR(q), evs->ev_wbufcid);
		evs->ev_wbufcid = 0;
	}
	if (evs->ev_rtimoutid != 0) {
		(void) quntimeout(RD(q), evs->ev_rtimoutid);
		evs->ev_rtimoutid = 0;
	}
	if (evs->ev_wtimoutid != 0) {
		(void) quntimeout(WR(q), evs->ev_wtimoutid);
		evs->ev_wtimoutid = 0;
	}
}

/*
 * Called by bufcall() when memory becomes available
 * Don't need to hold lock. protected by perimeter
 */
static void
tun_bufcall_handler(void *arg)
{
	queue_t		*q = arg;
	tun_t		*atp = (tun_t *)q->q_ptr;
	eventid_t	*evs;

	ASSERT(atp);

	evs = &atp->tun_events;
	if ((q->q_flag & QREADR) != 0) {
		ASSERT(evs->ev_rbufcid);
		evs->ev_rbufcid = 0;
	} else {
		ASSERT(evs->ev_wbufcid);
		evs->ev_wbufcid = 0;
	}
	enableok(q);
	qenable(q);
}

/*
 * Called by timeout (if we couldn't do a bufcall)
 * Don't need to hold lock. protected by perimeter
 */
static void
tun_timeout_handler(void *arg)
{
	queue_t		*q = arg;
	tun_t		*atp = (tun_t *)q->q_ptr;
	eventid_t	*evs;

	ASSERT(atp);
	evs = &atp->tun_events;

	if (q->q_flag & QREADR) {
		ASSERT(evs->ev_rtimoutid);
		evs->ev_rtimoutid = 0;
	} else {
		ASSERT(evs->ev_wtimoutid);
		evs->ev_wtimoutid = 0;
	}
	enableok(q);
	qenable(q);
}

/*
 * This routine is called when a message buffer can not
 * be allocated.  M_PCPROT message are converted to M_PROTO, but
 * other than that, the mblk passed in must not be a high
 * priority message (putting a hight priority message back on
 * the queue is a bad idea)
 * Side effect: the queue is disabled
 * (timeout or bufcall handler will re-enable the queue)
 * tun_cancel_rec_evs() must be called in close to cancel all
 * outstanding requests.
 */
static void
tun_recover(queue_t *q, mblk_t *mp, size_t size)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	timeout_id_t	tid;
	bufcall_id_t	bid;
	eventid_t	*evs = &atp->tun_events;

	/*
	 * To avoid re-enabling the queue, change the high priority
	 * M_PCPROTO message to a M_PROTO before putting it on the queue
	 */
	if (mp != NULL && mp->b_datap->db_type == M_PCPROTO)
		mp->b_datap->db_type = M_PROTO;

	ASSERT(mp->b_datap->db_type < QPCTL);

	(void) putbq(q, mp);

	/*
	 * Make sure there is at most one outstanding request per queue.
	 */
	if (q->q_flag & QREADR) {
		if (evs->ev_rtimoutid || evs->ev_rbufcid)
			return;
	} else {
		if (evs->ev_wtimoutid || evs->ev_wbufcid)
			return;
	}

	noenable(q);
	/*
	 * locking is needed here because this routine may be called
	 * with two puts() running
	 */
	mutex_enter(&atp->tun_lock);
	if (!(bid = qbufcall(q, size, BPRI_MED, tun_bufcall_handler, q))) {
		tid = qtimeout(q, tun_timeout_handler, q, TUN_RECOVER_WAIT);
		if (q->q_flag & QREADR)
			evs->ev_rtimoutid = tid;
		else
			evs->ev_wtimoutid = tid;
	} else	{
		if (q->q_flag & QREADR)
			evs->ev_rbufcid = bid;
		else
			evs->ev_wbufcid = bid;
	}
	mutex_exit(&atp->tun_lock);
}

/*
 * note: this routine will adjust the b_rptr and b_wptr of the
 * mblk.  Returns an mblk able to hold the requested size or
 * NULL if allocation failed. If copy is true, original
 * contents, if any, will be copied to new mblk
 */
static mblk_t *
tun_realloc_mblk(queue_t *q, mblk_t *mp, size_t size, mblk_t *orig_mp,
    boolean_t copy)
{
	/*
	 * If we are passed in an mblk.. check to make sure that
	 * it is big enough and we are the only users of the mblk
	 * If not, then try and allocate one
	 */
	if (mp == NULL || mp->b_datap->db_lim - mp->b_datap->db_base < size ||
	    mp->b_datap->db_ref > 1) {
		size_t	asize;
		mblk_t *newmp;

		/* allocate at least as much as we had -- don't shrink */
		if (mp != NULL) {
			asize = MAX(size,
			    mp->b_datap->db_lim - mp->b_datap->db_base);
		} else {
			asize = size;
		}
		newmp = allocb(asize, BPRI_HI);

		if (newmp == NULL) {
			/*
			 * Reschedule the mblk via bufcall or timeout
			 */
			tun_recover(q, orig_mp, asize);
			TUNDEBUG(TUN_DBG1,
			    ("tun_realloc_mblk: couldn't allocate"
			    " dl_ok_ack mblk\n"));
			return (NULL);
		}
		if (mp != NULL) {
			if (copy)
				bcopy(mp->b_rptr, newmp->b_rptr,
				    mp->b_wptr - mp->b_rptr);
			newmp->b_datap->db_type = mp->b_datap->db_type;
			freemsg(mp);
		}
		mp = newmp;
	} else {
		if (mp->b_rptr != mp->b_datap->db_base) {
			if (copy)
				bcopy(mp->b_rptr, mp->b_datap->db_base,
				    mp->b_wptr - mp->b_rptr);
			mp->b_rptr = mp->b_datap->db_base;
		}
	}
	mp->b_wptr = mp->b_rptr + size;
	return (mp);
}


/* send a DL_OK_ACK back upstream */
static int
tun_sendokack(queue_t *q, mblk_t *mp, t_uscalar_t prim)
{
	dl_ok_ack_t *dlok;

	if ((mp = tun_realloc_mblk(q, mp, sizeof (dl_ok_ack_t), mp,
	    B_FALSE)) == NULL) {
		return (ENOMEM);
	}
	dlok = (dl_ok_ack_t *)mp->b_rptr;
	dlok->dl_primitive = DL_OK_ACK;
	dlok->dl_correct_primitive = prim;
	mp->b_datap->db_type = M_PCPROTO;
	qreply(q, mp);
	return (0);
}

/* send a DL_ERROR_ACK back upstream */
static int
senderrack(queue_t *q, mblk_t *mp, t_uscalar_t prim, t_uscalar_t dl_err,
    t_uscalar_t errno)
{
	dl_error_ack_t *dl_err_ack;

	if ((mp = tun_realloc_mblk(q, mp, sizeof (dl_error_ack_t), mp,
	    B_FALSE)) == NULL) {
		return (ENOMEM);
	}

	dl_err_ack = (dl_error_ack_t *)mp->b_rptr;
	dl_err_ack->dl_error_primitive =  prim;
	dl_err_ack->dl_primitive = DL_ERROR_ACK;
	dl_err_ack->dl_errno = dl_err;
	dl_err_ack->dl_unix_errno = errno;
	mp->b_datap->db_type = M_PCPROTO;
	qreply(q, mp);
	return (0);
}

/*
 * Search tun_ppa_list for occurrence of tun_ppa, same lower stream,
 * and same type (automatic vs configured)
 * If none is found, insert this tun entry and create a new kstat for
 * the entry.
 * This is needed so that multiple tunnels with the same interface
 * name (e.g. ip.tun0 under IPv4 and ip.tun0 under IPv6) can share the
 * same kstats. (they share the same tun_stat and kstat)
 * Don't need to hold tun_lock if we are coming is as qwriter()
 */
static tun_stats_t *
tun_add_stat(queue_t *q)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	tun_stats_t	*tun_list;
	tun_stats_t	*tun_stat;
	t_uscalar_t	ppa = atp->tun_ppa;
	uint_t	lower = atp->tun_flags & TUN_LOWER_MASK;
	uint_t	tun_type = (atp->tun_flags & TUN_AUTOMATIC);
	uint_t index = TUN_LIST_HASH(ppa);

	ASSERT(atp->tun_stats == NULL);

	ASSERT(atp->tun_next == NULL);
	/*
	 * it's ok to grab global lock while holding tun_lock/perimeter
	 */
	mutex_enter(&tun_global_lock);

	/*
	 * walk through list of tun_stats looking for a match of
	 * ppa, same lower stream and same tunnel type (automatic
	 * or configured
	 * There shouldn't be all that many tunnels, so a sequential
	 * search should be fine
	 * XXX - this may change if tunnels get ever get created on the fly
	 */
	for (tun_list = tun_ppa_list[index]; tun_list;
	    tun_list = tun_list->ts_next) {
		if (tun_list->ts_ppa == ppa &&
		    tun_list->ts_lower == lower &&
		    tun_list->ts_type == tun_type) {
			TUNDEBUG(TUN_DBG1,
			    ("tun_add_stat: tun 0x%p Found ppa %d tun_stats"
			    " 0x%p\n", (void *)atp, ppa, (void *)tun_list));
			mutex_enter(&tun_list->ts_lock);
			mutex_exit(&tun_global_lock);
			ASSERT(tun_list->ts_refcnt > 0);
			tun_list->ts_refcnt++;
			ASSERT(atp->tun_next == NULL);
			ASSERT(atp != tun_list->ts_atp);
			/*
			 * add this tunnel instance to head of list
			 * of tunnels referencing this kstat structure
			 */
			atp->tun_next = tun_list->ts_atp;
			tun_list->ts_atp = atp;
			atp->tun_stats = tun_list;
			mutex_exit(&tun_list->ts_lock);
			return (tun_list);
		}
	}

	/* didn't find one, allocate a new one */

	tun_stat = kmem_zalloc(sizeof (tun_stats_t), KM_NOSLEEP);
	if (tun_stat != NULL) {
		mutex_init(&tun_stat->ts_lock, NULL, MUTEX_DEFAULT,
		    NULL);
		TUNDEBUG(TUN_DBG1,
		    ("tun_add_stat: New ppa %d tun_stat 0x%p\n",
		    ppa, (void *)tun_stat));
		tun_stat->ts_refcnt = 1;
		tun_stat->ts_lower = lower;
		tun_stat->ts_type = tun_type;
		tun_stat->ts_ppa = ppa;
		tun_stat->ts_next = tun_ppa_list[index];
		tun_ppa_list[index] = tun_stat;
		tun_stat->ts_atp = atp;
		atp->tun_next = NULL;
		atp->tun_stats = tun_stat;
		mutex_exit(&tun_global_lock);
		tun_statinit(tun_stat, q->q_qinfo->qi_minfo->mi_idname);
	} else {
		mutex_exit(&tun_global_lock);
	}
	return (tun_stat);
}

/*
 * remove tun from tun_ppa_list
 * called either holding tun_lock or in perimeter
 */
static void
tun_rem_list(tun_t *atp)
{
	uint_t index = TUN_LIST_HASH(atp->tun_ppa);
	tun_stats_t	*tun_stat = atp->tun_stats;
	tun_stats_t	**tun_list;
	tun_t		**at_list;

	if (tun_stat == NULL)
		return;

	ASSERT(atp->tun_ppa == tun_stat->ts_ppa);
	mutex_enter(&tun_global_lock);
	mutex_enter(&tun_stat->ts_lock);
	atp->tun_stats = NULL;
	tun_stat->ts_refcnt--;

	/*
	 * If this is the last instance, delete the tun_stat
	 */
	if (tun_stat->ts_refcnt == 0) {
		kstat_t		*tksp;

		TUNDEBUG(TUN_DBG1,
		    ("tun_rem_list: tun 0x%p Last ref ppa %d tun_stat 0x%p\n",
		    (void *)atp, tun_stat->ts_ppa, (void *)tun_stat));

		ASSERT(atp->tun_next == NULL);
		for (tun_list = &tun_ppa_list[index]; *tun_list;
		    tun_list = &(*tun_list)->ts_next) {
			if (tun_stat == *tun_list) {
				*tun_list = tun_stat->ts_next;
				tun_stat->ts_next = NULL;
				break;
			}
		}
		mutex_exit(&tun_global_lock);
		tksp = tun_stat->ts_ksp;
		tun_stat->ts_ksp = NULL;
		mutex_exit(&tun_stat->ts_lock);
		kstat_delete(tksp);
		mutex_destroy(&tun_stat->ts_lock);
		kmem_free(tun_stat, sizeof (tun_stats_t));
		return;
	}
	mutex_exit(&tun_global_lock);

	TUNDEBUG(TUN_DBG1,
	    ("tun_rem_list: tun 0x%p Removing ref ppa %d tun_stat 0x%p\n",
	    (void *)atp, tun_stat->ts_ppa, (void *)tun_stat));

	ASSERT(tun_stat->ts_atp->tun_next != NULL);

	/*
	 * remove tunnel instance from list of tunnels referencing
	 * this kstat.  List should be short, so we just search
	 * sequentially
	 */
	for (at_list = &tun_stat->ts_atp; *at_list;
	    at_list = &(*at_list)->tun_next) {
		if (atp == *at_list) {
			*at_list = atp->tun_next;
			atp->tun_next = NULL;
			break;
		}
	}
	ASSERT(tun_stat->ts_atp != NULL);
	ASSERT(atp->tun_next == NULL);
	mutex_exit(&tun_stat->ts_lock);
}

/*
 * handle all non-unitdata DLPI requests from above
 * called as qwriter()
 */
static void
tun_wput_dlpi_other(queue_t *q, mblk_t *mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	uint_t	lvers;
	t_uscalar_t prim = *((t_uscalar_t *)mp->b_rptr);
	t_uscalar_t dl_err = DL_UNSUPPORTED;
	t_uscalar_t dl_errno = 0;
	char buf1[TUN_WHO_BUF];

	switch (prim) {
	case DL_INFO_REQ: {
		dl_info_ack_t *dinfo;

		TUNDEBUG(TUN_DBG1, ("tun_wput_dlpi_other: got DL_INFO_REQ\n"));

		if ((mp = tun_realloc_mblk(q, mp, sizeof (dl_info_ack_t), mp,
		    B_FALSE)) == NULL) {
			return;
		}
		mp->b_datap->db_type = M_PCPROTO;

		/* send DL_INFO_ACK back up */
		dinfo = (dl_info_ack_t *)mp->b_rptr;

		*dinfo = infoack;
		dinfo->dl_current_state = atp->tun_state;
		dinfo->dl_max_sdu = atp->tun_mtu;

		/*
		 * We set the address length to non-zero so that
		 * automatic tunnels will not have multicast or
		 * point to point set.
		 * Someday IPv6 needs to support multicast over automatic
		 * tunnels
		 */
		if (atp->tun_flags & TUN_AUTOMATIC) {
			/*
			 * set length to size of ip address so that
			 * ip_newroute will generate dl_unitdata_req for
			 * us with gateway or dest filed in. (i.e.
			 * might as well have ip do something useful)
			 */
			ASSERT((atp->tun_flags & TUN_U_V4) == 0);
			dinfo->dl_addr_length = IPV6_ADDR_LEN;
		} else {
			dinfo->dl_addr_length = 0;
		}
		qreply(q, mp);
		return;
	}

	case DL_ATTACH_REQ: {
		dl_attach_req_t *dla;

		TUNDEBUG(TUN_DBG1, ("tun_wput_dlpi_other: "
		    "got DL_ATTACH_REQ\n"));

		if ((mp = tun_realloc_mblk(q, mp, sizeof (dl_ok_ack_t), mp,
		    B_TRUE)) == NULL) {
			return;
		}

		dla = (dl_attach_req_t *)mp->b_rptr;

		if (atp->tun_state != DL_UNATTACHED) {
			dl_err = DL_OUTSTATE;
			TUNDEBUG(TUN_DBGERR, ("tun_wput_dlpi_other: "
			    "DL_ATTACH_REQ state not DL_UNATTACHED (0x%x)\n",
			    atp->tun_state));
			break;
		}
		atp->tun_ppa = dla->dl_ppa;

		/*
		 * get (possibly shared) kstat structure
		 */
		if (tun_add_stat(q) == NULL) {
			ASSERT(atp->tun_stats == NULL);
			dl_err = DL_SYSERR;
			dl_errno = ENOMEM;
			break;
		}
		atp->tun_state = DL_UNBOUND;

		(void) tun_sendokack(q, mp, prim);
		return;
	}

	case DL_DETACH_REQ:

		TUNDEBUG(TUN_DBG1, ("tun_wput_dlpi_other: "
		    "got DL_DETACH_REQ\n"));

		if ((mp = tun_realloc_mblk(q, mp, sizeof (dl_ok_ack_t), mp,
		    B_FALSE)) == NULL) {
			return;
		}

		if (atp->tun_state != DL_UNBOUND) {
			dl_err = DL_OUTSTATE;
			TUNDEBUG(TUN_DBGERR, ("tun_wput_dlpi_other: "
			    "DL_DETACH_REQ state not DL_UNBOUND (0x%x)\n",
			    atp->tun_state));
			break;
		}
		atp->tun_state = DL_UNATTACHED;

		/*
		 * don't need to hold tun_lock
		 * since this is really a single thread operation
		 * for this instance
		 */
		if (atp->tun_stats) {
			tun_rem_list(atp);
			TUNDEBUG(TUN_DBG1,
			    ("tun_wput_dlpi_other: deleting kstat"));
		}
		(void) tun_sendokack(q, mp, prim);
		return;

	case DL_BIND_REQ: {
		dl_bind_req_t *bind_req;
		t_uscalar_t dl_sap = 0;

		TUNDEBUG(TUN_DBG1, ("tun_wput_dlpi_other: got DL_BIND_REQ\n"));

		if (atp->tun_state != DL_UNBOUND) {
			dl_err = DL_OUTSTATE;
			TUNDEBUG(TUN_DBGERR, ("tun_wput_dlpi_other: "
			    "DL_BIND_REQ state not DL_UNBOUND (0x%x)\n",
			    atp->tun_state));
			break;
		}

		atp->tun_state = DL_IDLE;

		bind_req = (dl_bind_req_t *)mp->b_rptr;

		dl_sap = bind_req->dl_sap;
		ASSERT(bind_req->dl_sap == IP_DL_SAP ||
		    bind_req->dl_sap == IP6_DL_SAP);

		lvers = atp->tun_flags & TUN_LOWER_MASK;

		if (dl_sap == IP_DL_SAP) {
			if ((atp->tun_flags & TUN_U_V6) != 0) {
				dl_err = DL_BOUND;
				TUNDEBUG(TUN_DBGERR, ("tun_wput_dlpi_other: "
				    "DL_BIND_REQ upper TUN_U_V6"
				    " (0x%x)\n",
				    atp->tun_flags));
				break;
			}

			atp->tun_flags |= TUN_U_V4;
			if (lvers == TUN_L_V4) {
				atp->tun_ipha.ipha_protocol = IPPROTO_ENCAP;
			} else {
				ASSERT(lvers == TUN_L_V6);
				atp->tun_ip6h.ip6_nxt = IPPROTO_ENCAP;
			}
		} else if (dl_sap == IP6_DL_SAP) {
			if ((atp->tun_flags & TUN_U_V4) != 0) {
				dl_err = DL_BOUND;
				TUNDEBUG(TUN_DBGERR, ("tun_wput_dlpi_other: "
				    "DL_BIND_REQ upper TUN_U_V4 (0x%x)\n",
				    atp->tun_flags));
				break;
			}
			atp->tun_flags |= TUN_U_V6;
			if (lvers == TUN_L_V4) {
				atp->tun_ipha.ipha_protocol = IPPROTO_IPV6;
			} else {
				ASSERT(lvers == TUN_L_V6);
				atp->tun_ip6h.ip6_nxt = IPPROTO_IPV6;
			}
		} else {
			atp->tun_state = DL_UNBOUND;
			break;
		}

		/*
		 * Send DL_BIND_ACK, which is the same size as the
		 * request, so we can re-use the mblk.
		 */

		*(dl_bind_ack_t *)mp->b_rptr = bindack;
		((dl_bind_ack_t *)mp->b_rptr)->dl_sap = dl_sap;
		mp->b_datap->db_type = M_PCPROTO;
		qreply(q, mp);
		return;
	}
	case DL_UNBIND_REQ:

		TUNDEBUG(TUN_DBG1, ("tun_wput_dlpi_other: "
		    "got DL_UNBIND_REQ\n"));

		if ((mp = tun_realloc_mblk(q, mp, sizeof (dl_ok_ack_t), mp,
		    B_FALSE)) == NULL) {
			return;
		}

		if (atp->tun_state != DL_IDLE) {
			dl_err = DL_OUTSTATE;
			TUNDEBUG(TUN_DBGERR, ("tun_wput_dlpi_other: "
			    "DL_UNBIND_REQ state not DL_IDLE (0x%x)\n",
			    atp->tun_state));
			break;
		}
		atp->tun_state = DL_UNBOUND;
		/* Send a DL_OK_ACK. */
		(void) tun_sendokack(q, mp, prim);
		return;

	case DL_PHYS_ADDR_REQ: {
		dl_phys_addr_ack_t *dpa;

		TUNDEBUG(TUN_DBG1, ("tun_wput_dlpi_other: "
		    "got DL_PHYS_ADDR_REQ\n"));

		if ((mp = tun_realloc_mblk(q, mp, sizeof (dl_phys_addr_ack_t),
		    mp, B_FALSE)) == NULL) {
			return;
		}

		dpa = (dl_phys_addr_ack_t *)mp->b_rptr;

		dpa->dl_primitive = DL_PHYS_ADDR_ACK;

		/*
		 * dl_addr_length must match info ack
		 */
		if (atp->tun_flags & TUN_AUTOMATIC) {
			if ((atp->tun_flags & TUN_U_V4) != 0) {
				dpa->dl_addr_length = IP_ADDR_LEN;
			} else {
				dpa->dl_addr_length = IPV6_ADDR_LEN;
			}
		} else {
			dpa->dl_addr_length = 0;
		}

		dpa->dl_addr_offset = 0;
		mp->b_datap->db_type = M_PCPROTO;
		qreply(q, mp);
		return;
	}
	default:
		/* DL_UNSUPPORTED */
		printf("tun_wput_dlpi_other: "
		    "unsupported DLPI message type: %d\n", prim);
		break;
	}

	TUNDEBUG(TUN_DBGERR,
	    ("tun_wput_dlpi_other: %s dl_error_ack prim 0x%x dl_err %d"
	    " dl_errno %d\n", tun_who(q, buf1), prim, dl_err,
	    dl_errno));
	(void) senderrack(q, mp, prim, dl_err, dl_errno);
}

/*
 * handle all DLPI requests from above
 */
static int
tun_wput_dlpi(queue_t *q, mblk_t *mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	mblk_t	*mp1;
	int	error = 0;
	t_uscalar_t prim = *((t_uscalar_t *)mp->b_rptr);

	switch (prim) {
	case DL_UNITDATA_REQ:
		if (atp->tun_state != DL_IDLE) {
			break;
		}
		if (!canputnext(q)) {
			mutex_enter(&atp->tun_lock);
			atp->tun_xmtretry++;
			mutex_exit(&atp->tun_lock);
			(void) putbq(q, mp);
			return (ENOMEM); /* to get service proc to stop */
		}
		/* we don't use any of the data in the DLPI header */
		mp1 = mp->b_cont;
		freeb(mp);
		if (mp1 == NULL) {
			break;
		}
		switch (atp->tun_flags & TUN_UPPER_MASK) {
		case TUN_U_V4:
			error = tun_wdata_v4(q, mp1);
			break;
		case TUN_U_V6:
			error = tun_wdata_v6(q, mp1);
			break;
		default:
			mutex_enter(&atp->tun_lock);
			atp->tun_OutErrors++;
			mutex_exit(&atp->tun_lock);
			ASSERT((atp->tun_flags & TUN_UPPER_MASK) != TUN_U_V4 ||
			    (atp->tun_flags & TUN_UPPER_MASK) != TUN_U_V6);
			break;
		}
		break;

	default:
		qwriter(q, mp, tun_wput_dlpi_other, PERIM_INNER);
		break;
	}
	return (error);
}

/*
 * set the tunnel parameters
 * called as qwriter
 */
static void
tun_sparam(queue_t *q, mblk_t *mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	struct	iocblk   *iocp = (struct iocblk *)(mp->b_rptr);
	struct iftun_req	*ta;
	mblk_t	*mp1;
	int	uerr = 0;
	uint_t	lvers;
	sin_t	*sin;

	/* don't allow changes after dl_bind_req */
	if (atp->tun_state  == DL_IDLE) {
		uerr = EAGAIN;
		goto nak;
	}

	mp1 = mp->b_cont;
	if (mp1 == NULL) {
		uerr = EPROTO;
		goto nak;
	}

	mp1 = mp1->b_cont;
	if (mp1 == NULL) {
		uerr = EPROTO;
		goto nak;
	}
	if (mp1->b_wptr - mp1->b_rptr != sizeof (struct iftun_req)) {
		uerr = EPROTO;
		goto nak;
	}
	if (atp->tun_iocmp) {
		uerr = EBUSY;
		goto nak;
	}

	lvers = atp->tun_flags & TUN_LOWER_MASK;

	ta = (struct iftun_req *)mp1->b_rptr;

	/*
	 * Check version number for parsing the security settings.
	 */
	if (ta->ifta_vers != IFTUN_VERSION) {
		uerr = EINVAL;
		goto nak;
	}

	/*
	 * Upper layer will give us a v4/v6 indicator, in case we don't know
	 * already.
	 */
	if ((atp->tun_flags & TUN_UPPER_MASK) == 0) {
		if (ta->ifta_flags & 0x80000000) {
			atp->tun_flags |= TUN_U_V6;
		} else {
			atp->tun_flags |= TUN_U_V4;
		}
	}

	if (ta->ifta_flags & IFTUN_SRC) {
		switch (ta->ifta_saddr.ss_family) {
		case AF_INET:
			sin = (sin_t *)&ta->ifta_saddr;
			if (lvers != TUN_L_V4) {
				uerr = EINVAL;
				goto nak;
			}
			if ((sin->sin_addr.s_addr == INADDR_ANY) ||
			    (sin->sin_addr.s_addr == 0xffffffff) ||
			    CLASSD(sin->sin_addr.s_addr)) {
				uerr = EADDRNOTAVAIL;
				goto nak;
			}
			atp->tun_ipha.ipha_src = sin->sin_addr.s_addr;
			IN6_IPADDR_TO_V4MAPPED(sin->sin_addr.s_addr,
			    &atp->tun_laddr);
			break;
		case AF_INET6:
			if (lvers != TUN_L_V6) {
				uerr = EINVAL;
				goto nak;
			}
			/* XXX need to do sanity check here */
			atp->tun_laddr =
			    ((sin6_t *)&ta->ifta_saddr)->sin6_addr;
			atp->tun_ip6h.ip6_src = atp->tun_laddr;
			break;
		default:
			uerr = EAFNOSUPPORT;
			goto nak;
		}

		/*
		 * If I reach here, then I didn't bail, the src address
		 * was good.
		 */
		atp->tun_flags |= TUN_SRC;
	}
	if (ta->ifta_flags & IFTUN_DST) {
		if (atp->tun_flags & TUN_AUTOMATIC) {
			uerr = EINVAL;
			goto nak;
		}
		if (ta->ifta_saddr.ss_family == AF_INET) {
			sin = (sin_t *)&ta->ifta_daddr;
			if (lvers != TUN_L_V4) {
				uerr = EINVAL;
				goto nak;
			}
			if ((sin->sin_addr.s_addr == 0) ||
			    (sin->sin_addr.s_addr == 0xffffffff) ||
			    CLASSD(sin->sin_addr.s_addr)) {
				uerr = EADDRNOTAVAIL;
				goto nak;
			}
			atp->tun_ipha.ipha_dst = sin->sin_addr.s_addr;
			IN6_IPADDR_TO_V4MAPPED(sin->sin_addr.s_addr,
			    &atp->tun_faddr);
		} else if (ta->ifta_saddr.ss_family == AF_INET6) {
			if (lvers != TUN_L_V6) {
				uerr = EINVAL;
				goto nak;
			}
			atp->tun_faddr =
			    ((sin6_t *)&ta->ifta_saddr)->sin6_addr;
			atp->tun_ip6h.ip6_dst = atp->tun_faddr;
		} else {
			uerr = EAFNOSUPPORT;
			goto nak;
		}

		/*
		 * If I reach here, then I didn't bail, the dst address
		 * was good.
		 */
		atp->tun_flags |= TUN_DST;
	}
#if 0
		/* XXX future */
	if (ta->ifta_flags & TUN_HOP_LIM) {
		atp->tun_hop_limit = ta->ta_hop_limit;
		/* XXX do we really need this flag */
		atp->tun_flags |= TUN_HOP_LIM;
		if (lvers == TUN_L_V4) {
			atp->tun_ipha.ipha_ttl = atp->tun_hop_limit;
		} else {
			atp->tun_ip6h.ip6_hops = atp->tun_hop_limit;
		}
	}
#endif
	if (ta->ifta_flags & IFTUN_SECURITY) {
		ipsec_req_t *ipsr;

		/* Can't set security properties for automatic tunnels. */
		if (atp->tun_flags & TUN_AUTOMATIC) {
			uerr = EINVAL;
			goto nak;
		}

		/*
		 * The version number checked out, so just cast
		 * iftr_secinfo to an ipsr.
		 *
		 * Also pay attention to the iftr_secinfo.
		 */

		ipsr = (ipsec_req_t *)(&ta->ifta_secinfo);
		atp->tun_secinfo = *ipsr;
		atp->tun_flags |= TUN_SECURITY;
		/*
		 * Do setting of security options after T_BIND_ACK
		 * happens.  If there is no T_BIND_ACK, however,
		 * see below.
		 */
	}

	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_error = 0;

	/*
	 * Send a T_BIND_REQ if and only if a tsrc/tdst change was requested
	 * _AND_ tsrc is turned on _AND_ the tunnel either has tdst turned on
	 * or is an automatic tunnel.
	 */
	if ((ta->ifta_flags & (IFTUN_SRC | IFTUN_DST)) != 0 &&
	    (atp->tun_flags & TUN_SRC) != 0 &&
	    (atp->tun_flags & (TUN_DST | TUN_AUTOMATIC)) != 0) {
		atp->tun_iocmp = mp;
		uerr = tun_send_bind_req(q, mp);
		if (uerr == 0) {
			/* qreply() done by T_BIND_ACK processing */
			return;
		} else {
			atp->tun_iocmp = NULL;
			goto nak;
		}
	} else if (ta->ifta_flags & IFTUN_SECURITY) {
		/*
		 * If just a change of security settings, do it now!
		 * Either ifta_flags or tun_flags will do, but ASSERT
		 * that both are turned on.
		 */
		ASSERT(atp->tun_flags & TUN_SECURITY);
		atp->tun_iocmp = mp;
		uerr = tun_send_sec_req(q, mp);
		if (uerr == 0) {
			/* qreply() done by T_OPTMGMT_REQ processing */
			return;
		} else {
			atp->tun_iocmp = NULL;
			goto nak;
		}
	}
	qreply(q, mp);
	return;
nak:
	iocp->ioc_error = uerr;
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);
}

/*
 * Processes SIOCs to setup a configured tunnel.
 * M_IOCDATA->M_COPY->DATA
 */
static int
tun_ioctl(queue_t *q, mblk_t *mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	struct	iocblk   *iocp = (struct iocblk *)(mp->b_rptr);
	struct iftun_req	*ta;
	mblk_t	*mp1;
	int	reterr = 0;
	int	uerr = 0;
	uint_t	lvers;
	sin_t	*sin;
	sin6_t	*sin6;

	lvers = atp->tun_flags & TUN_LOWER_MASK;

	switch (iocp->ioc_cmd) {
	case SIOCSTUNPARAM:
		qwriter(q, mp, tun_sparam, PERIM_INNER);
		return (0);
	case SIOCGTUNPARAM:
		mp1 = mp->b_cont;
		if (mp1 == NULL) {
			uerr = EPROTO;
			goto nak;
		}
		mp1 = mp1->b_cont;
		if (mp1 == NULL) {
			uerr = EPROTO;
			goto nak;
		}
		if (mp1->b_wptr - mp1->b_rptr != sizeof (struct iftun_req)) {
			uerr = EPROTO;
			goto nak;
		}
		/*
		 * don't need to hold any locks. Can only be
		 * changed by qwriter
		 */
		ta = (struct iftun_req *)mp1->b_rptr;
		ta->ifta_flags = 0;

		/*
		 * Unlike tun_sparam(), the version number for security
		 * parameters is ignored, since we're filling it in!
		 */
		ta->ifta_vers = IFTUN_VERSION;

		/* in case we are pushed under something unsupported */
		switch (atp->tun_flags & TUN_UPPER_MASK) {
		case TUN_U_V4:
			ta->ifta_upper = IFTAP_IPV4;
			break;
		case TUN_U_V6:
			ta->ifta_upper = IFTAP_IPV6;
			break;
		default:
			ta->ifta_upper = 0;
			break;
		}
		/*
		 * Copy in security information.
		 *
		 * If we revise IFTUN_VERSION, this will become revision-
		 * dependent.
		 */
		if (atp->tun_flags & TUN_SECURITY) {
			ipsec_req_t *ipsr;

			ta->ifta_flags |= IFTUN_SECURITY;
			ipsr = (ipsec_req_t *)(&ta->ifta_secinfo);
			*ipsr = atp->tun_secinfo;
		}

		/* lower must be IPv4 or IPv6, otherwise open fails */
		if (lvers == TUN_L_V4) {
			sin = (sin_t *)&ta->ifta_saddr;
			ta->ifta_lower = IFTAP_IPV4;
			bzero(sin, sizeof (sin_t));
			sin->sin_family = AF_INET;
			if (atp->tun_flags & TUN_SRC) {
				IN6_V4MAPPED_TO_IPADDR(&atp->tun_laddr,
				    sin->sin_addr.s_addr);
				ta->ifta_flags |= IFTUN_SRC;
			} else {
				sin->sin_addr.s_addr = 0;
			}

			sin = (sin_t *)&ta->ifta_daddr;
			bzero(sin, sizeof (sin_t));
			sin->sin_family = AF_INET;
			if (atp->tun_flags & TUN_DST) {
				IN6_V4MAPPED_TO_IPADDR(&atp->tun_faddr,
				    sin->sin_addr.s_addr);
				ta->ifta_flags |= IFTUN_DST;
			} else {
				sin->sin_addr.s_addr = 0;
			}
		} else {
			ASSERT(lvers == TUN_L_V6);

			ta->ifta_lower = IFTAP_IPV6;
			sin6 = (sin6_t *)&ta->ifta_saddr;
			bzero(sin6, sizeof (sin6_t));
			sin6->sin6_family = AF_INET6;
			if (atp->tun_flags & TUN_SRC) {
				sin6->sin6_addr = atp->tun_laddr;
				ta->ifta_flags |= IFTUN_SRC;
			} else {
				V6_SET_ZERO(sin6->sin6_addr);
			}

			sin6 = (sin6_t *)&ta->ifta_daddr;
			bzero(sin6, sizeof (sin6_t));
			sin6->sin6_family = AF_INET6;

			if (atp->tun_flags & TUN_DST) {
				ta->ifta_flags |= IFTUN_DST;
				sin6->sin6_addr = atp->tun_faddr;
			} else {
				V6_SET_ZERO(sin6->sin6_addr);
			}
		}
		break;
	case DL_IOC_HDR_INFO:
		mp1 = mp->b_cont;
		if (mp1 == NULL) {
			uerr = EPROTO;
			goto nak;
		}
		uerr = tun_fastpath(q, mp1);
		if (uerr != 0)
			goto nak;
		break;
	/*
	 * We are module that thinks it's a driver so nak anything
	 * we don't understand
	 */
	default:
		uerr = EINVAL;
		goto nak;
	}
	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_error = 0;
	qreply(q, mp);
	return (reterr);
nak:
	iocp->ioc_error = uerr;
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);
	return (reterr);
}

/*
 * mp contains dl_unitdata_req_t part of the messsage
 * allocate mblk for fast path.
 * XXX - fix IP so that db_base and rptr can be different
 */
static int
tun_fastpath(queue_t *q, mblk_t *mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	dl_unitdata_req_t *dlu = (dl_unitdata_req_t *)mp->b_rptr;
	mblk_t *nmp;

	if (!tun_do_fastpath || atp->tun_state != DL_IDLE ||
	    mp->b_wptr - mp->b_rptr < sizeof (dl_unitdata_req_t) ||
	    dlu->dl_primitive != DL_UNITDATA_REQ) {
		return (EINVAL);
	}
	switch (atp->tun_flags & TUN_LOWER_MASK) {
	case TUN_L_V4:
		nmp = allocb(sizeof (ipha_t) + atp->tun_extra_offset, BPRI_HI);
		if (nmp == NULL) {
			return (ENOMEM);
		}
		mp->b_cont = nmp;
		nmp->b_rptr += atp->tun_extra_offset;
		nmp->b_wptr = nmp->b_rptr + sizeof (ipha_t);
		*(ipha_t *)(nmp->b_rptr) = atp->tun_ipha;
		nmp->b_rptr = nmp->b_datap->db_base;
		break;
	case TUN_L_V6:
	default:
		return (EPFNOSUPPORT);
	}
	atp->tun_flags |= TUN_FASTPATH;

	return (0);
}



/*
 *  write side service procedure
 */
void
tun_wsrv(queue_t *q)
{
	mblk_t	*mp;
	tun_t	*atp = (tun_t *)q->q_ptr;

	while (mp = getq(q)) {
		/* out of memory or canputnext failed */
		if (tun_wproc(q, mp) == ENOMEM) {
			break;
		}
		/*
		 * If we called qwriter, then the only way we
		 * can tell if we ran out of memory is to check if
		 * any events have been scheduled
		 */
		if (atp->tun_events.ev_wtimoutid != 0 &&
		    atp->tun_events.ev_wbufcid != 0) {
			break;
		}
	}
}


/* write side put procedure */
void
tun_wput(queue_t *q, mblk_t *mp)
{
	/* note: q_first is 'protected' by perimeter */
	if (q->q_first != NULL) {
		(void) putq(q, mp);
	} else {
		(void) tun_wproc(q, mp);
	}
}

/*
 * called from write side put or service procedure to process
 * messages
 */
static int
tun_wproc(queue_t *q, mblk_t *mp)
{
	int		error = 0;

	switch (mp->b_datap->db_type) {
	case M_DATA:
		error = tun_wproc_mdata(q, mp);
		break;

	case M_PROTO:
	case M_PCPROTO:
		/* its a DLPI message */
		error = tun_wput_dlpi(q, mp);
		break;

	case M_IOCDATA:
	case M_IOCTL:
		/* Data to be copied out arrives from ip as M_IOCDATA */
		error = tun_ioctl(q, mp);
		break;

	/* we are a module pretending to be a driver.. turn around flush */

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW) {
			flushq(q, FLUSHALL);
			*mp->b_rptr &= ~FLUSHW;
		}
		if (*mp->b_rptr & FLUSHR)
			flushq(RD(q), FLUSHALL);
		qreply(q, mp);
		break;

	/*
	 * we are a module pretending to be a driver.. so just free message
	 * we don't understand
	 */
	default: {
		char buf[TUN_WHO_BUF];

		TUNDEBUG(TUN_DBGERR,
		    ("tun_wproc: %s got unknown mblk type %d\n",
		    tun_who(q, buf), mp->b_datap->db_type));
		freemsg(mp);
		break;
	}

	}
	return (error);
}

/*
 * handle fast path M_DATA message
 */
static int
tun_wproc_mdata(queue_t *q, mblk_t *mp)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	int		error = 0;

	ASSERT(atp->tun_flags & TUN_FASTPATH);

	/* XXX - assert only good for IPv6 over IPv4 */
	ASSERT(mp->b_wptr - mp->b_rptr >= atp->tun_extra_offset +
	    sizeof (ipha_t));

	if (!canputnext(q)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_xmtretry++;
		mutex_exit(&atp->tun_lock);
		(void) putbq(q, mp);
		return (ENOMEM);	/* get service procedure to stop */
	}

	if (atp->tun_flags & TUN_AUTOMATIC) {
		int iph_hdr_length;
		/*
		 * get rid of fastpath header. let tun_wdata*
		 * fill in real thing
		 */

		iph_hdr_length = IPH_HDR_LENGTH((ipha_t *)(mp->b_rptr +
		    atp->tun_extra_offset));
		if (mp->b_wptr - mp->b_rptr < iph_hdr_length +
		    atp->tun_extra_offset + sizeof (ip6_t)) {
			if (!pullupmsg(mp, iph_hdr_length +
			    atp->tun_extra_offset + sizeof (ip6_t))) {
				TUNDEBUG(TUN_DBGERR,
				    ("tun_rdata_v4:  message too short for IPv6"
				    " header\n"));
				mutex_enter(&atp->tun_lock);
				atp->tun_InErrors++;
				atp->tun_InDiscard++;
				mutex_exit(&atp->tun_lock);
				freemsg(mp);
				return (0);
			}
		}
		mp->b_rptr += atp->tun_extra_offset + iph_hdr_length;

		ASSERT((atp->tun_flags & TUN_UPPER_MASK) == TUN_U_V6);
		error = tun_wdata_v6(q, mp);
		return (error);
	}

	switch (atp->tun_flags & TUN_UPPER_MASK) {
	case TUN_U_V4:
		error = tun_wputnext_v4(q, mp);
		break;
	case TUN_U_V6:
		error = tun_wputnext_v6(q, mp);
		break;
	default:
		mutex_enter(&atp->tun_lock);
		atp->tun_OutErrors++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		error = EINVAL;
	}
	return (error);
}

static int
tun_send_sec_req(queue_t *q, mblk_t *orig_mp)
{
	tun_t *atp = (tun_t *)q->q_ptr;
	mblk_t *optmp;
	struct T_optmgmt_req *omr;
	struct opthdr *oh;

	optmp = tun_realloc_mblk(q, NULL, sizeof (*omr) + sizeof (*oh) +
	    sizeof (atp->tun_secinfo), orig_mp, B_FALSE);
	if (optmp == NULL)
		return (ENOMEM);

	optmp->b_wptr += sizeof (*omr) + sizeof (*oh) +
	    sizeof (atp->tun_secinfo);
	optmp->b_datap->db_type = M_PROTO;
	omr = (struct T_optmgmt_req *)optmp->b_rptr;
	oh = (struct opthdr *)(omr + 1);
	/*
	 * XXX Which TPI version am I?  Make sure we stay on top of things
	 * w.r.t. option management.
	 */
	omr->PRIM_type = T_SVR4_OPTMGMT_REQ;
	omr->MGMT_flags = T_NEGOTIATE;
	omr->OPT_offset = sizeof (*omr);
	omr->OPT_length = sizeof (*oh) + sizeof (atp->tun_secinfo);

	oh->level = IPPROTO_IP;
	oh->name = IP_SEC_OPT;
	oh->len = sizeof (atp->tun_secinfo);

	mutex_enter(&atp->tun_lock);
	*((ipsec_req_t *)(oh + 1)) = atp->tun_secinfo;
	mutex_exit(&atp->tun_lock);
	putnext(WR(q), optmp);
	return (0);
}

/*
 * Process TPI messages responses comming up the read side
 */

/* ARGSUSED */
int
tun_rput_tpi(queue_t *q, mblk_t *mp)
{
	tun_t *atp = (tun_t *)q->q_ptr;
	t_uscalar_t prim = *((t_uscalar_t *)mp->b_rptr);
	mblk_t *iocmp;

	switch (prim) {
	case T_OPTMGMT_ACK:
		mutex_enter(&atp->tun_lock);
		iocmp = atp->tun_iocmp;
		atp->tun_iocmp = NULL;
		if (atp->tun_secinfo.ipsr_esp_req == 0 &&
		    atp->tun_secinfo.ipsr_ah_req == 0)
			atp->tun_flags &= ~TUN_SECURITY;
		mutex_exit(&atp->tun_lock);
		ASSERT(iocmp != NULL);
		putnext(q, iocmp);
		freemsg(mp);
		break;
	case T_BIND_ACK: {
		ire_t	*ire;

		TUNDEBUG(TUN_DBG1, ("tun_rput_tpi: got a T_BIND_ACK\n"));
		mutex_enter(&atp->tun_lock);

		/*
		 * XXX This first assert may fail if this path gets re-
		 * executed because of tun_recover() being invoked.
		 */
		ASSERT((atp->tun_flags & TUN_BIND_SENT) != 0);
		ASSERT(atp->tun_iocmp != NULL);
		ire = (ire_t *)mp->b_cont->b_rptr;
		atp->tun_extra_offset =
		    MAX(ire->ire_ll_hdr_length, TUN_LINK_EXTRA_OFF);

		/*
		 * Automatic tunnels only require source to be set
		 * Configured tunnels require both
		 */
		if ((atp->tun_flags & TUN_SRC) &&
		    (atp->tun_flags & (TUN_DST | TUN_AUTOMATIC))) {
			atp->tun_flags |= TUN_BOUND;
		}

		atp->tun_flags &= ~TUN_BIND_SENT;

		iocmp = atp->tun_iocmp;
		/*
		 * If we have security information to send, do it after
		 * the T_BIND_ACK has been received.  Don't bother ACK-ing
		 * the ioctl until after this has been done.
		 *
		 * XXX One small nit about here is that if the user didn't set
		 * IFTUN_SECURITY, then this handler will reset the security
		 * levels anyway.
		 */

		if (atp->tun_flags & TUN_SECURITY) {
			int err;
			struct iocblk *iocp;

			/* Exit the mutex for tun_send_sec_req(). */
			mutex_exit(&atp->tun_lock);
			err = tun_send_sec_req(q, mp);
			if (err != 0) {
				/* Re-enter the mutex. */
				mutex_enter(&atp->tun_lock);
				atp->tun_flags &= ~TUN_SECURITY;
				iocp = (struct iocblk *)iocmp->b_rptr;
				iocp->ioc_error = err;
			} else {
				/* Let the OPTMGMT_ACK handler deal with it. */
				break;	/* Out of this case. */
			}
		}

		/*
		 * Ack the ioctl
		 */
		atp->tun_iocmp = NULL;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		putnext(q, iocmp);
		break;
	}
	case T_ERROR_ACK: {
		struct T_error_ack *terr = (struct T_error_ack *)mp->b_rptr;
		switch (terr->ERROR_prim) {
		case T_SVR4_OPTMGMT_REQ: {
			struct iocblk *iocp;

			mutex_enter(&atp->tun_lock);
			/* XXX Should we should unbind too? */
			atp->tun_flags &= ~TUN_SECURITY;
			iocmp = atp->tun_iocmp;
			atp->tun_iocmp = NULL;
			mutex_exit(&atp->tun_lock);
			iocp = (struct iocblk *)(iocmp->b_rptr);
			/* XXX Does OPTMGMT generate TLI_errors? */
			if (terr->UNIX_error == 0)
				iocp->ioc_error = EINVAL;
			else iocp->ioc_error = terr->UNIX_error;
			putnext(q, iocmp);
			freemsg(mp);
			return (0);
		}
		case T_BIND_REQ: {
			struct iftun_req	*ta;
			mblk_t *mp1;
			struct iocblk	*iocp;

			mutex_enter(&atp->tun_lock);
			atp->tun_flags &= ~(TUN_BOUND | TUN_BIND_SENT);
			iocmp = atp->tun_iocmp;
			atp->tun_iocmp = NULL;
			mutex_exit(&atp->tun_lock);
			iocp = (struct iocblk *)(iocmp->b_rptr);

			mp1 = iocmp->b_cont;
			if (mp1 != NULL)
				mp1 = mp1->b_cont;
			if (mp1 != NULL) {
				ta = (struct iftun_req *)mp1->b_rptr;
				if (ta->ifta_flags & IFTUN_SRC) {
					atp->tun_flags &= ~TUN_SRC;
				}
				if (ta->ifta_flags & IFTUN_DST) {
					atp->tun_flags &= ~TUN_DST;
				}
			}
			switch (terr->TLI_error) {
			default:
				iocp->ioc_error = EINVAL;
				break;
			case TSYSERR:
				iocp->ioc_error = terr->UNIX_error;
				break;
			case TBADADDR:
				iocp->ioc_error = EADDRNOTAVAIL;
				break;
			}
			putnext(q, iocmp);
			freemsg(mp);
			return (0);
		}
		default: {
			char buf[TUN_WHO_BUF];

			TUNDEBUG(TUN_DBGERR,
			    ("tun_rput_tpi: %s got an unkown TPI Error "
			    "message: %d\n",
			    tun_who(q, buf), terr->ERROR_prim));
			freemsg(mp);
			break;
		}
		}
		break;
	}

	case T_OK_ACK:
		freemsg(mp);
		break;

	/* act like a stream head and eat all up comming tpi messages */
	default: {
		char buf[TUN_WHO_BUF];

		TUNDEBUG(TUN_DBGERR,
		    ("tun_rput_tpi: %s got an unkown TPI message: %d\n",
		    tun_who(q, buf), prim));
		freemsg(mp);
		break;
	}
	}
	return (0);
}

/*
 * handle tunnel over IPv6
 * XXX currently don't support this
 */
/* ARGSUSED */
static int
tun_rdata_v6(queue_t *q, mblk_t *mp)
{
	cmn_err(CE_PANIC, "Tunneling not supported over IPv6..");
	return (0);
}

/*
 * Handle tunnels over IPv4
 * XXX - we don't do any locking here. The worst that
 * can happen is we drop the packet or don't record stats quite right
 * what's the worst that can happen if the header stuff changes?
 */
static int
tun_rdata_v4(queue_t *q, mblk_t *mp)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	ipha_t		*iph, *inner_iph;
	ip6_t		*ip6h;
	size_t		hdrlen;
	mblk_t		*mp1;
	boolean_t	inner_v4;
	ipaddr_t	v4src;
	ipaddr_t	v4dst;
	in6_addr_t	v4mapped_src;
	in6_addr_t	v4mapped_dst;
#ifdef DEBUG
	char		buf1[INET6_ADDRSTRLEN];
	char		buf2[INET6_ADDRSTRLEN];
	char		buf[TUN_WHO_BUF];
#endif

	/* need at least an IP header */
	ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (ipha_t));

	if (atp->tun_state != DL_IDLE) {
		mutex_enter(&atp->tun_lock);
		atp->tun_InErrors++;
		atp->tun_HCInUcastPkts++;	/* need to count packet */
		mutex_exit(&atp->tun_lock);
		goto drop;
	}

	if (!canputnext(q)) {
		TUNDEBUG(TUN_DBG1,
		    ("tun_rdata_v4: flow controlled\n"));
		ASSERT(mp->b_datap->db_type < QPCTL);
		mutex_enter(&atp->tun_lock);
		atp->tun_nocanput++;
		mutex_exit(&atp->tun_lock);
		(void) putbq(q, mp);
		return (ENOMEM);	/* to stop service procedure */
	}

	iph = (ipha_t *)mp->b_rptr;

	hdrlen = IPH_HDR_LENGTH(iph);
	/* check IP version number */
	ASSERT(IPH_HDR_VERSION(iph) == IPV4_VERSION);

	ASSERT(hdrlen >= sizeof (ipha_t));
	ASSERT(hdrlen <= (mp->b_wptr - mp->b_rptr));

	v4src = iph->ipha_src;
	v4dst = iph->ipha_dst;
	IN6_IPADDR_TO_V4MAPPED(v4src, &v4mapped_src);
	IN6_IPADDR_TO_V4MAPPED(v4dst, &v4mapped_dst);
	inner_v4 = (iph->ipha_protocol == IPPROTO_ENCAP);

	/* shave off the IPv4 header */
	if ((mp->b_wptr - mp->b_rptr) == hdrlen) {
		TUNDEBUG(TUN_DBG1,
		    ("tun_rdata_v4: new path hdrlen= %lu\n", hdrlen));
		mp1 = mp->b_cont;
		freeb(mp);
		mp = mp1;
		if (mp == NULL) {
			TUNDEBUG(TUN_DBGERR,
			    ("tun_rdata_v4: b_cont null, no data\n"));
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			return (0);
		}
	} else {
		mp->b_rptr += hdrlen;
	}

	if ((mp->b_wptr - mp->b_rptr) <
	    (inner_v4 ? sizeof (ipha_t) : sizeof (ip6_t))) {
		if (!pullupmsg(mp,
		    (inner_v4 ? sizeof (ipha_t) : sizeof (ip6_t)))) {
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			atp->tun_InDiscard++;
			mutex_exit(&atp->tun_lock);
			goto drop;
		}
	}

	if (inner_v4) {
		/* IPv4 in IPv4 */
		inner_iph = (ipha_t *)mp->b_rptr;
		ASSERT(IPH_HDR_VERSION(inner_iph) == IPV4_VERSION);
		ASSERT(IN6_ARE_ADDR_EQUAL(&v4mapped_dst, &atp->tun_laddr) &&
		    IN6_ARE_ADDR_EQUAL(&v4mapped_src, &atp->tun_faddr));
		mutex_enter(&atp->tun_lock);
		if (CLASSD(inner_iph->ipha_dst)) {
			atp->tun_HCInMulticastPkts++;
		} else {
			atp->tun_HCInUcastPkts++;
		}
		mutex_exit(&atp->tun_lock);
	} else {
		/* IPv6 in IPv4 */
		ip6h = (ip6_t *)mp->b_rptr;
		ASSERT(IPH_HDR_VERSION(ip6h) == IPV6_VERSION);

		ASSERT(IN6_ARE_ADDR_EQUAL(&v4mapped_dst, &atp->tun_laddr));
		mutex_enter(&atp->tun_lock);
		if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst)) {
			atp->tun_HCInMulticastPkts++;
		} else {
			atp->tun_HCInUcastPkts++;
		}
		mutex_exit(&atp->tun_lock);

		if ((atp->tun_flags & TUN_AUTOMATIC) != 0) {
			dl_unitdata_ind_t *dludindp;

			/*
			 *  make sure IPv4 destination makes sense
			 */
			if (v4dst == INADDR_ANY || CLASSD(v4dst)) {
#ifdef DEBUG
				TUNDEBUG(TUN_DBGERR,
				    ("tun_rdata_v4: %s tun: invalid IPv4 dest "
					"(%s)\n",
					tun_who(q, buf),
					inet_ntop(AF_INET, &v4dst,
					    buf1, sizeof (buf1))));
#endif
				mutex_enter(&atp->tun_lock);
				atp->tun_InErrors++;
				mutex_exit(&atp->tun_lock);
				goto drop;
			}

			/*
			 * send packet up as DL_UNITDATA_IND so that it won't
			 * be forwarded
			 */

			mp1 = allocb(sizeof (dl_unitdata_ind_t), BPRI_HI);
			if (mp1 == NULL) {
				TUNDEBUG(TUN_DBGERR,
				    ("tun_rdata_v4: allocb failed\n"));
				mutex_enter(&atp->tun_lock);
				atp->tun_InDiscard++;
				atp->tun_allocbfail++;
				mutex_exit(&atp->tun_lock);
				goto drop;
			}
			mp1->b_cont = mp;
			mp = mp1;
			/*
			 * create dl_unitdata_ind with group address set so
			 * we don't forward
			 */
			mp->b_wptr = mp->b_rptr + sizeof (dl_unitdata_ind_t);
			mp->b_datap->db_type = M_PROTO;
			dludindp = (dl_unitdata_ind_t *)mp->b_rptr;
			dludindp->dl_primitive = DL_UNITDATA_IND;
			dludindp->dl_dest_addr_length = 0;
			dludindp->dl_dest_addr_offset = 0;
			dludindp->dl_src_addr_length = 0;
			dludindp->dl_src_addr_offset = 0;
			dludindp->dl_group_address = 1;

			/*
			 * this might happen if we are in the middle of
			 * re-binding
			 */
		} else if (!IN6_ARE_ADDR_EQUAL(&v4mapped_src,
		    &atp->tun_faddr)) {
			/*
			 * Configured Tunnel packet source should match our
			 * destination
			 * XXX should this be debug ?
			 * Lower IP should ensure that this is true
			 */
#ifdef DEBUG
			TUNDEBUG(TUN_DBGERR,
			    ("tun_rdata_v4: %s src (%s) != tun_faddr (%s)\n",
				tun_who(q, buf),
				inet_ntop(AF_INET6, &v4mapped_src,
				    buf1, sizeof (buf1)),
				inet_ntop(AF_INET6, &atp->tun_faddr,
				    buf2, sizeof (buf2))));
#endif
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			goto drop;
		}
	}

	mutex_enter(&atp->tun_lock);
	atp->tun_HCInOctets += msgdsize(mp);
	mutex_exit(&atp->tun_lock);
	putnext(q, mp);
	return (0);
drop:
	freemsg(mp);
	return (0);
}

/* ARGSUSED */
static void
tun_rput_icmp_err_v6(queue_t *q, mblk_t *mp)
{
	cmn_err(CE_PANIC, "V6 Tunnels not supported");
	freemsg(mp);
}

/*
 * icmp from lower IPv4
 * Process ICMP messages from IPv4. Pass them to the appropriate
 * lower processing function.
 */
static void
tun_rput_icmp_err_v4(queue_t *q, mblk_t *mp)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	ipha_t		*ipha;
	icmph_t		*icmph;
	int		iph_hdr_length;
	int		inner_len;

	/* XXX - should we trust this or do the same logic as IP */
	if (!canputnext(q)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_nocanput++;
		atp->tun_InDiscard++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return;
	}

	/* need to force to M_DATA so that pullupmsg works */
	mp->b_datap->db_type = M_DATA;
	ipha = (ipha_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(ipha);

	/* inner ip header length dependent on what's above us */
	inner_len = ((atp->tun_flags & TUN_U_V4) != 0) ?  sizeof (ipha_t) :
	    sizeof (ip6_t);

	/*
	 * Check length - minimum packet is inner (encapsulated) IP
	 * header in an IPv4 in ICMP in IPv4 AND 8 bytes of transport
	 * (chances are the 2 * outer ip_hdr_length will get ip header
	 * encapsulated in icmp)
	 */
	if ((mp->b_wptr - mp->b_rptr) < iph_hdr_length * 2 + sizeof (icmph_t)
	    + inner_len + 8) {
		/*
		 * try and pull all the headers up
		 * Note: because we don't support any options over a IPv4
		 * tunnel, there shouldn't be any options
		 */
		if (!pullupmsg(mp, iph_hdr_length * 2 + sizeof (icmph_t) +
		    inner_len + 8)) {

			char buf[TUN_WHO_BUF];

			TUNDEBUG(TUN_DBGERR,
			    ("tun_rput_icmp_err_v4: %s packet too short\n",
			    tun_who(q, buf)));
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
			return;
		}
		ipha = (ipha_t *)mp->b_rptr;
	}

	mp->b_datap->db_type = M_CTL;
	icmph = (icmph_t *)(&mp->b_rptr[iph_hdr_length]);

	switch (atp->tun_flags & TUN_UPPER_MASK) {
	case TUN_U_V6:
		icmp_ricmp_err_v6_v4(q, mp, icmph);
		break;
	case TUN_U_V4:
		icmp_ricmp_err_v4_v4(q, mp, icmph);
		break;
	default:
		mutex_enter(&atp->tun_lock);
		atp->tun_InErrors++;
		mutex_exit(&atp->tun_lock);
		ASSERT(0);
		freemsg(mp);
	}
}

/*
 * Process ICMP message from IPv4 encapsulating an IPv4 packet
 * If this message contains path mtu information, cut out the
 * encapsulation from the icmp data.  If there is still useful
 * information in the icmp data pass it upstream (packaged correctly for
 * the upper layer IP)
 */
static void
icmp_ricmp_err_v4_v4(queue_t *q, mblk_t *mp, icmph_t *icmph)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	ipha_t		*outer_ipha, *inner_ipha;
	int		outer_hlen;
	int		mtu = 0;
	icmph_t		icmp;
	uint8_t		type;
	uint8_t		code;
#ifdef DEBUG
	char		buf_who[TUN_WHO_BUF];
#endif
	char		buf1[INET_ADDRSTRLEN];
	char		buf2[INET_ADDRSTRLEN];

	outer_ipha = (ipha_t *)&icmph[1];

	/*
	 * check length again - encapsulating IPv4 packet can be longer
	 * than minimum if it has options.
	 */
	ASSERT((uchar_t *)&outer_ipha[1] <= mp->b_wptr);

	outer_hlen = IPH_HDR_LENGTH(outer_ipha);

	/*
	 * Make sure that IPv4 header and encapsulated ip6 header and
	 * 8 bytes of transport are all pulled up
	 */
	if ((mp->b_wptr - mp->b_rptr) < outer_hlen + sizeof (ipha_t) + 8) {
		/*
		 * try and pull all the headers up
		 * note: we probably don't get here at present because
		 * we don't support IPv4 options over tunnels and caller
		 * already did a pullup
		 */
		if (!pullupmsg(mp, outer_hlen + sizeof (ipha_t) + 8)) {
			TUNDEBUG(TUN_DBGERR,
			    ("tun_rput_icmp_err_v4: %s packet too short\n",
			    tun_who(q, buf_who)));
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
			return;
		}
		outer_ipha = (ipha_t *)mp->b_rptr;
	}
	inner_ipha = (ipha_t *)((uint8_t *)outer_ipha + outer_hlen);

	/*
	 * again check length again - encapsulating IPv4 packet can be longer
	 * than minimum if it has options.
	 */
	ASSERT((uchar_t *)&inner_ipha[1] <= mp->b_wptr);

	type = icmph->icmph_type;
	code = icmph->icmph_code;

	/* new packet will contain all of old packet */

	mp->b_rptr = (uchar_t *)inner_ipha;

	switch (type) {
	case ICMP_DEST_UNREACHABLE:
		switch (code) {
		case ICMP_FRAGMENTATION_NEEDED:
			mtu = ntohs(icmph->icmph_du_mtu);
			if (icmph->icmph_du_zero != 0 && mtu <= 68) {
				TUNDEBUG(TUN_DBGERR,
				    ("tun_rput_icmp_err_v4:"
					" invalid icmp mtu\n"));
				mutex_enter(&atp->tun_lock);
				atp->tun_InErrors++;
				mutex_exit(&atp->tun_lock);
				freemsg(mp);
				return;
			}
			mtu -= sizeof (ipha_t);
			if (mtu < 68)
				mtu = 68;
			if (!tun_icmp_too_big_v4(q, inner_ipha, mtu, mp)) {
				mutex_enter(&atp->tun_lock);
				atp->tun_InDiscard++;
				atp->tun_allocbfail++;
				mutex_exit(&atp->tun_lock);
			}
			return;
		case ICMP_PROTOCOL_UNREACHABLE:
			/*
			 * XXX may need way to throttle messages
			 * XXX should we do this for automatic or
			 * just configured tunnels ?
			 */
			(void) strlog(q->q_qinfo->qi_minfo->mi_idnum,
			    atp->tun_ppa, 1,
			    SL_ERROR | SL_WARN,
			    "%s.%s%d: Protocol unreachable. "
			    "Misconfigured tunnel? source %s"
			    " destination %s\n",
			    (atp->tun_flags & TUN_LOWER_MASK) ==
			    TUN_L_V4 ? "ip" : "ip6",
			    TUN_NAME, atp->tun_ppa,
			    inet_ntop(AF_INET, &outer_ipha->ipha_dst,
			    buf1, sizeof (buf1)),
			    inet_ntop(AF_INET, &outer_ipha->ipha_src,
			    buf2, sizeof (buf2)));
			/* FALLTHRU */
		case ICMP_NET_UNREACHABLE:
		case ICMP_HOST_UNREACHABLE:
		case ICMP_DEST_NET_UNKNOWN:
		case ICMP_DEST_HOST_UNKNOWN:
		case ICMP_SRC_HOST_ISOLATED:
		case ICMP_SOURCE_ROUTE_FAILED:
		case ICMP_DEST_NET_UNREACH_TOS:
		case ICMP_DEST_HOST_UNREACH_TOS:
			icmp.icmph_type = ICMP_DEST_UNREACHABLE;
			/* XXX HOST or NET unreachable? */
			icmp.icmph_code = ICMP_NET_UNREACHABLE;
			icmp.icmph_rd_gateway = (ipaddr_t)0;
			break;
		case ICMP_DEST_NET_UNREACH_ADMIN:
		case ICMP_DEST_HOST_UNREACH_ADMIN:
			icmp.icmph_type = ICMP_DEST_UNREACHABLE;
			icmp.icmph_code = ICMP_DEST_NET_UNREACH_ADMIN;
			icmp.icmph_rd_gateway = (ipaddr_t)0;
			break;
		default:
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
			return;
		}
		break;
	case ICMP_TIME_EXCEEDED:
		icmp.icmph_type = ICMP_TIME_EXCEEDED;
		icmp.icmph_code = code;
		icmp.icmph_rd_gateway = (ipaddr_t)0;
		break;
	case ICMP_PARAM_PROBLEM:
		icmp.icmph_type = ICMP_PARAM_PROBLEM;
		if (icmph->icmph_pp_ptr < (uchar_t *)inner_ipha - mp->b_rptr) {
			TUNDEBUG(TUN_DBGERR,
			    ("tun_rput_icmp_err_v4:"
			    " ICMP_PARAM_PROBLEM too short\n"));
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
			return;
		}
		icmp.icmph_pp_ptr = htonl(icmph->icmph_pp_ptr -
		    ((uchar_t *)inner_ipha - mp->b_rptr) + sizeof (ipha_t) +
		    sizeof (icmph_t));
		break;
	default:
		mutex_enter(&atp->tun_lock);
		atp->tun_InErrors++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return;
	}
	if (!tun_icmp_message_v4(q, inner_ipha, &icmp, mp)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_InDiscard++;
		atp->tun_allocbfail++;
		mutex_exit(&atp->tun_lock);
	}
}

/*
 * Process ICMP message from IPv4 encapsulating an IPv6 packet
 * If this message contains path mtu information, cut out the
 * encapsulation from the icmp data.  If there is still useful
 * information in the icmp data pass it upstream (packaged correctly for
 * the upper layer IP)
 */
static void
icmp_ricmp_err_v6_v4(queue_t *q, mblk_t *mp, icmph_t *icmph)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	ip6_t		*ip6h;
	ipha_t		*ipha;
	int		iph_hdr_length;
	int		mtu = 0;
	icmp6_t		icmp6;
	uint8_t		type;
	uint8_t		code;
	uint8_t		hoplim;
#ifdef DEBUG
	char		buf_who[TUN_WHO_BUF];
#endif
	char		buf1[INET_ADDRSTRLEN];
	char		buf2[INET_ADDRSTRLEN];

	ipha = (ipha_t *)&icmph[1];

	/*
	 * check length again - encapsulating IPv4 packet can be longer
	 * than minimum if it has options.
	 */
	ASSERT((uchar_t *)&ipha[1] <= mp->b_wptr);

	iph_hdr_length = IPH_HDR_LENGTH(ipha);

	/*
	 * Make sure that IPv4 header and encapsulated ip6 header and
	 * 8 bytes of transport are all pulled up
	 */
	if ((mp->b_wptr - mp->b_rptr) < iph_hdr_length + sizeof (ip6_t) + 8) {
		/*
		 * try and pull all the headers up
		 * note: we probably don't get here at present because
		 * we don't support IPv4 options over tunnels and caller
		 * already did a pullup
		 */
		if (!pullupmsg(mp, iph_hdr_length + sizeof (ip6_t) + 8)) {
			TUNDEBUG(TUN_DBGERR,
			    ("tun_rput_icmp_err_v4: %s packet too short\n",
			    tun_who(q, buf_who)));
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
			return;
		}
		ipha = (ipha_t *)mp->b_rptr;
	}
	ip6h = (ip6_t *)((uint8_t *)ipha + iph_hdr_length);

	/*
	 * again check length again - encapsulating IPv4 packet can be longer
	 * than minimum if it has options.
	 */
	ASSERT((uchar_t *)&ip6h[1] <= mp->b_wptr);

	type = icmph->icmph_type;
	code = icmph->icmph_code;
	hoplim = ipha->ipha_ttl;

	/* new packet will contain all of old packet */

	mp->b_rptr = (uchar_t *)ip6h;

	switch (type) {
	case ICMP_DEST_UNREACHABLE:
		switch (code) {
		case ICMP_FRAGMENTATION_NEEDED:
			mtu = ntohs(icmph->icmph_du_mtu);
			if (icmph->icmph_du_zero != 0 && mtu <= 68) {
				TUNDEBUG(TUN_DBGERR,
				    ("tun_rput_icmp_err_v4:"
				    " invalid icmp mtu\n"));
				mutex_enter(&atp->tun_lock);
				atp->tun_InErrors++;
				mutex_exit(&atp->tun_lock);
				freemsg(mp);
				return;
			}
			mtu -= sizeof (ipha_t);
			if (mtu < IPV6_MIN_MTU) {
				/*
				 * can't set to less than IPv6 min..
				 * set to min and allow IPv4 to
				 * fragment
				 */
				mtu = IPV6_MIN_MTU;
			}
			if (!tun_icmp_too_big_v6(q, ip6h, mtu, hoplim, mp)) {
				mutex_enter(&atp->tun_lock);
				atp->tun_InDiscard++;
				atp->tun_allocbfail++;
				mutex_exit(&atp->tun_lock);
			}
			return;

		case ICMP_PROTOCOL_UNREACHABLE: {
			/*
			 * XXX may need way to throttle messages
			 * XXX should we do this for automatic or
			 * just configured tunnels ?
			 */
			(void) strlog(q->q_qinfo->qi_minfo->mi_idnum,
			    atp->tun_ppa, 1,
			    SL_ERROR | SL_WARN,
			    "%s.%s%d: Protocol unreachable. "
			    "Misconfigured tunnel? source %s"
			    " destination %s\n",
			    (atp->tun_flags & TUN_LOWER_MASK) ==
			    TUN_L_V4 ? "ip" : "ip6",
			    TUN_NAME, atp->tun_ppa,
			    inet_ntop(AF_INET, &ipha->ipha_dst,
			    buf1, sizeof (buf1)),
			    inet_ntop(AF_INET, &ipha->ipha_src,
			    buf2, sizeof (buf2)));
			icmp6.icmp6_type = ICMP6_DST_UNREACH;
			icmp6.icmp6_code = ICMP6_DST_UNREACH_ADDR;
			icmp6.icmp6_data32[0] = 0;
			break;
		}
		case ICMP_PORT_UNREACHABLE:
			icmp6.icmp6_type = ICMP6_DST_UNREACH;
			icmp6.icmp6_code = ICMP6_DST_UNREACH_NOPORT;
			icmp6.icmp6_data32[0] = 0;
			break;
		case ICMP_NET_UNREACHABLE:
		case ICMP_HOST_UNREACHABLE:
		case ICMP_DEST_NET_UNKNOWN:
		case ICMP_DEST_HOST_UNKNOWN:
		case ICMP_SRC_HOST_ISOLATED:
		case ICMP_DEST_NET_UNREACH_TOS:
		case ICMP_DEST_HOST_UNREACH_TOS:
			icmp6.icmp6_type = ICMP6_DST_UNREACH;
			icmp6.icmp6_code = ICMP6_DST_UNREACH_NOROUTE;
			icmp6.icmp6_data32[0] = 0;
			break;
		case ICMP_DEST_NET_UNREACH_ADMIN:
		case ICMP_DEST_HOST_UNREACH_ADMIN:
			icmp6.icmp6_type = ICMP6_DST_UNREACH;
			icmp6.icmp6_code = ICMP6_DST_UNREACH_ADMIN;
			icmp6.icmp6_data32[0] = 0;
			break;

		case ICMP_SOURCE_ROUTE_FAILED:
			icmp6.icmp6_type = ICMP6_DST_UNREACH;
			icmp6.icmp6_code =
			    ICMP6_DST_UNREACH_NOTNEIGHBOR;
			icmp6.icmp6_data32[0] = 0;
			break;
		default:
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
			return;
		}
		break;
	case ICMP_TIME_EXCEEDED:
		icmp6.icmp6_type = ICMP6_TIME_EXCEEDED;
		icmp6.icmp6_code = code;
		icmp6.icmp6_data32[0] = 0;
		break;
	case ICMP_PARAM_PROBLEM:
		icmp6.icmp6_type = ICMP6_PARAM_PROB;
		if (icmph->icmph_pp_ptr < (uchar_t *)ip6h - mp->b_rptr) {
			TUNDEBUG(TUN_DBGERR,
			    ("tun_rput_icmp_err_v4:"
			    " ICMP_PARAM_PROBLEM too short\n"));
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
			return;
		}
		icmp6.icmp6_pptr = htonl(
		    icmph->icmph_pp_ptr - ((uchar_t *)ip6h - mp->b_rptr)
		    + sizeof (ip6_t) + sizeof (icmp6_t));
		break;

	default:
		mutex_enter(&atp->tun_lock);
		atp->tun_InErrors++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return;
	}
	if (!tun_icmp_message_v6(q, ip6h, &icmp6, hoplim, mp)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_InDiscard++;
		atp->tun_allocbfail++;
		mutex_exit(&atp->tun_lock);
	}
}

/*
 * Rewhack the packet for the upper IP.
 */
static boolean_t
tun_icmp_too_big_v4(queue_t *q, ipha_t *ipha, uint16_t mtu, mblk_t *mp)
{
	icmph_t icmp;

	icmp.icmph_type = ICMP_DEST_UNREACHABLE;
	icmp.icmph_code = ICMP_FRAGMENTATION_NEEDED;
	ASSERT(mtu >= 68);
	icmp.icmph_du_zero = 0;
	icmp.icmph_du_mtu = htons(mtu);
	return (tun_icmp_message_v4(q, ipha, &icmp, mp));
}

/*
 * Send an ICMP6_PACKET_TOO_BIG message
 */
static boolean_t
tun_icmp_too_big_v6(queue_t *q, ip6_t *ip6ha, uint32_t mtu, uint8_t hoplim,
    mblk_t *mp)
{
	icmp6_t	icmp6;

	icmp6.icmp6_type = ICMP6_PACKET_TOO_BIG;
	icmp6.icmp6_code = 0;
	ASSERT(mtu >= IPV6_MIN_MTU);
	icmp6.icmp6_mtu = htonl(mtu);
	return (tun_icmp_message_v6(q, ip6ha, &icmp6, hoplim, mp));
}

/*
 * Send an icmp message up an IPv4 stream.
 */
static boolean_t
tun_icmp_message_v4(queue_t *q, ipha_t *ipha, icmph_t *icmp, mblk_t *mp)
{
	ssize_t plen, nsize;
	mblk_t *send_mp;
	tun_t *atp = (tun_t *)q->q_ptr;
	ipha_t *nipha;
	icmph_t *nicmp;

	plen = mp->b_wptr - mp->b_rptr;
	nsize = sizeof (ipha_t) + sizeof (icmph_t) + plen;

	if ((send_mp = allocb(nsize, BPRI_HI)) == NULL) {
		mutex_enter(&atp->tun_lock);
		atp->tun_InDiscard++;
		atp->tun_allocbfail++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return (B_FALSE);
	}
	send_mp->b_wptr =  send_mp->b_rptr + nsize;

	nipha = (ipha_t *)send_mp->b_rptr;
	nicmp = (icmph_t *)(nipha + 1);
	*nicmp = *icmp;
	nicmp->icmph_checksum = 0;
	nipha->ipha_version_and_hdr_length = IP_SIMPLE_HDR_VERSION;
	nipha->ipha_type_of_service = 0;
	nipha->ipha_length = htons(nsize);
	nipha->ipha_ident = ipha->ipha_ident;
	nipha->ipha_fragment_offset_and_flags = htons(IPH_DF);
	nipha->ipha_ttl = ipha->ipha_ttl;
	nipha->ipha_protocol = IPPROTO_ICMP;
	nipha->ipha_src = ipha->ipha_dst;
	nipha->ipha_dst = ipha->ipha_src;
	nipha->ipha_hdr_checksum = 0;
	nipha->ipha_hdr_checksum = ip_csum_hdr(nipha);
	bcopy(ipha, &nicmp[1], plen);
	if (mp->b_cont != NULL) {
		send_mp->b_cont = mp->b_cont;
		mp->b_cont = NULL;
		plen += msgdsize(send_mp->b_cont);
	}
	freeb(mp);
	ASSERT(send_mp->b_rptr == send_mp->b_datap->db_base);
	nicmp->icmph_checksum = IP_CSUM(send_mp, sizeof (ipha_t), 0);

	/* let ip know we are an icmp message */
	mutex_enter(&atp->tun_lock);
	atp->tun_HCInOctets += plen + sizeof (icmp6_t);
	mutex_exit(&atp->tun_lock);
	putnext(q, send_mp);
	return (B_TRUE);
}

/*
 * Send an icmp message up an IPv6 stream.
 */
static boolean_t
tun_icmp_message_v6(queue_t *q, ip6_t *ip6h, icmp6_t *icmp6, uint8_t hoplim,
    mblk_t *mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	mblk_t		*send_mp;
	ssize_t		nsize;
	icmp6_t		*nicmp6;
	ip6_t		*nip6h;
	uint16_t	*up;
	uint32_t	sum;
	ssize_t		plen;

	plen = mp->b_wptr - mp->b_rptr;
	nsize = sizeof (ip6_t) + sizeof (icmp6_t) + plen;

	if ((send_mp = allocb(nsize, BPRI_HI)) == NULL) {
		mutex_enter(&atp->tun_lock);
		atp->tun_InDiscard++;
		atp->tun_allocbfail++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return (B_FALSE);
	}
	send_mp->b_wptr =  send_mp->b_rptr + nsize;

	nip6h = (ip6_t *)send_mp->b_rptr;
	nicmp6 = (icmp6_t *)&nip6h[1];
	*nicmp6 = *icmp6;
	nip6h->ip6_vcf = ip6h->ip6_vcf;
	nip6h->ip6_plen = ip6h->ip6_plen;
	nip6h->ip6_hops = hoplim;
	nip6h->ip6_nxt = IPPROTO_ICMPV6;
	nip6h->ip6_src = ip6h->ip6_dst;
	nip6h->ip6_dst = ip6h->ip6_src;
	/* copy of ipv6 header into icmp6 message */
	bcopy(ip6h, &nicmp6[1], plen);
	/* add in the rest of the packet if any */
	if (mp->b_cont) {
		send_mp->b_cont = mp->b_cont;
		mp->b_cont = NULL;
		plen += msgdsize(send_mp->b_cont);
	}
	freeb(mp);
	nip6h->ip6_plen = htons(plen + sizeof (icmp6_t));
	nicmp6->icmp6_cksum = 0;
	up = (uint16_t *)&nip6h->ip6_src;
	sum = htons(IPPROTO_ICMPV6 +
	    ntohs(nip6h->ip6_plen)) +
	    up[0] + up[1] + up[2] + up[3] +
	    up[4] + up[5] + up[6] + up[7] +
	    up[8] + up[9] + up[10] + up[11] +
	    up[12] + up[13] + up[14] + up[15];
	sum = (sum & 0xffff) + (sum >> 16);
	nicmp6->icmp6_cksum = IP_CSUM(send_mp, IPV6_HDR_LEN, sum);

	/* let ip know we are an icmp message */
	mutex_enter(&atp->tun_lock);
	atp->tun_HCInOctets += plen + sizeof (icmp6_t);
	mutex_exit(&atp->tun_lock);
	send_mp->b_datap->db_type = M_DATA;
	putnext(q, send_mp);
	return (B_TRUE);
}

/*
 * Read side service routine.
 */
void
tun_rsrv(queue_t *q)
{
	mblk_t  *mp;
	tun_t	*atp = (tun_t *)q->q_ptr;

	while (mp = getq(q)) {
		if (tun_rproc(q, mp) == ENOMEM) {
			break;
		}
		/*
		 * If we called qwriter, then the only way we
		 * can tell if we ran out of memory is to check if
		 * any events have been scheduled
		 */
		if (atp->tun_events.ev_rtimoutid != 0 &&
		    atp->tun_events.ev_rbufcid != 0) {
			break;
		}
	}
}

/*
 * Read side put procedure
 */
void
tun_rput(queue_t *q, mblk_t *mp)
{
	/* note: q_first is 'protected' by perimeter */
	if (q->q_first != NULL) {
		(void) putq(q, mp);
	} else {
		(void) tun_rproc(q, mp);
	}
}

/*
 * Process read side messages
 */
static int
tun_rproc(queue_t *q, mblk_t *mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	uint_t	lvers;
	int	error = 0;
	char	buf[TUN_WHO_BUF];

	/* no lock needed, won't ever change */
	lvers = atp->tun_flags & TUN_LOWER_MASK;

	switch (mp->b_datap->db_type) {
	case M_DATA:

		if (lvers == TUN_L_V4) {
			error = tun_rdata_v4(q, mp);
		} else if (lvers == TUN_L_V6) {
			error = tun_rdata_v6(q, mp);
		} else {

			TUNDEBUG(TUN_DBGERR,
			    ("tun_rproc: %s no lower version\n",
			    tun_who(q, buf)));
			mutex_enter(&atp->tun_lock);
			atp->tun_InErrors++;
			mutex_exit(&atp->tun_lock);
			freemsg(mp);
		}
		if (error) {
			/* only record non flow control problems */
			if (error != EBUSY) {
				TUNDEBUG(TUN_DBGERR,
				    ("tun_rproc: %s error"
				    " encounterd %d\n", tun_who(q, buf),
				    error));
			}
		}
		break;

	case M_PROTO:
	case M_PCPROTO:
		/* its a TPI message */
		error = tun_rput_tpi(q, mp);
		break;

	case M_CTL:
		/* its an ICMP error message from IP */

		mutex_enter(&atp->tun_lock);
		atp->tun_HCInUcastPkts++;
		mutex_exit(&atp->tun_lock);
		if (lvers == TUN_L_V4) {
			tun_rput_icmp_err_v4(q, mp);
		} else if (lvers == TUN_L_V6) {
			tun_rput_icmp_err_v6(q, mp);
		} else {
			freemsg(mp);
		}
		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq(q, FLUSHALL);
			*mp->b_rptr &= ~FLUSHR;
		}
		/* we're pretending to be a stream head */
		if (*mp->b_rptr & FLUSHW) {
			qreply(q, mp);
		} else {
			freemsg(mp);
		}
		break;
	default:
		TUNDEBUG(TUN_DBGERR,
		    ("tun_rproc: %s got unknown mblk type %d\n",
		    tun_who(q, buf), mp->b_datap->db_type));
		freemsg(mp);
		break;
	}
	return (error);
}


/*
 * Handle Upper IPv4
 */
static int
tun_wdata_v4(queue_t *q, mblk_t *mp)
{
	ipha_t *outer_ipha, *inner_ipha;
	tun_t *atp = (tun_t *)q->q_ptr;
	mblk_t *newmp;

	ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (ipha_t));

	inner_ipha = (ipha_t *)mp->b_rptr;

	/*
	 * increment mib counters and pass message off to ip
	 * note: we must always increment packet counters, but
	 * only increment byte counter if we actually send packet
	 */

	mutex_enter(&atp->tun_lock);
	if (CLASSD(inner_ipha->ipha_dst)) {
		atp->tun_HCOutMulticastPkts++;
	} else {
		atp->tun_HCOutUcastPkts++;
	}
	mutex_exit(&atp->tun_lock);

	if (atp->tun_state != DL_IDLE || !(atp->tun_flags & TUN_BOUND)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_OutErrors++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return (0);
	}

	switch (atp->tun_flags & TUN_LOWER_MASK) {
	case TUN_L_V4:
		if (inner_ipha->ipha_dst == atp->tun_ipha.ipha_dst) {
			/*
			 * Watch out!  There is potential for an infinite loop.
			 * If IP sent a packet with destination address equal
			 * to the tunnel's destination address, we'll hit
			 * an infinite routing loop, where the packet will keep
			 * going through here.
			 *
			 * In the long term, perhaps IP should be somewhat
			 * intelligent about this.  Until then, nip this in
			 * the bud.
			 */
			TUNDEBUG(TUN_DBGERR,
			    ("tun_wdata: inner dst == tunnel dst.\n"));
			atp->tun_OutErrors++;
			freemsg(mp);
			return (0);
		}

		/* room for IPv4 header? */
		if ((mp->b_rptr - mp->b_datap->db_base) < sizeof (ipha_t)) {
			/* no */

			newmp = allocb(sizeof (ipha_t) + atp->tun_extra_offset,
			    BPRI_HI);
			if (newmp == NULL) {
				mutex_enter(&atp->tun_lock);
				atp->tun_OutDiscard++;
				atp->tun_allocbfail++;
				mutex_exit(&atp->tun_lock);
				freemsg(mp);
				return (0);
			}
			newmp->b_cont = mp;
			mp = newmp;
			mp->b_wptr = mp->b_datap->db_lim;
			mp->b_rptr = mp->b_wptr - sizeof (ipha_t);
		} else {
			/* yes */
			mp->b_rptr -= sizeof (ipha_t);
		}
		outer_ipha = (ipha_t *)mp->b_rptr;

		/*
		 * copy template header into packet IPv4 header
		 * Q: Do I need to hold the mutex?
		 */
		mutex_enter(&atp->tun_lock);
		*outer_ipha = atp->tun_ipha;
		mutex_exit(&atp->tun_lock);

		outer_ipha->ipha_length = htons(ntohs(inner_ipha->ipha_length)
		    + sizeof (ipha_t));
		break;
	default:
		/* LINTED */
		ASSERT(0 && "not supported");
		mutex_enter(&atp->tun_lock);
		atp->tun_OutErrors++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return (0);
	}

	mutex_enter(&atp->tun_lock);
	atp->tun_HCOutOctets += msgdsize(mp);
	mutex_exit(&atp->tun_lock);
	putnext(q, mp);
	return (0);
}

/*
 * put M_DATA fastpath upper IPv4
 * Assumes canput is possible
 *
 * XXX Hopefully this works.
 */
static int
tun_wputnext_v4(queue_t *q, mblk_t *mp)
{
	tun_t *atp = (tun_t *)q->q_ptr;
	ipha_t *inner_ipha, *outer_ipha;

	mp->b_rptr += atp->tun_extra_offset;
	if ((atp->tun_flags & TUN_L_V4) != 0) {
		uint_t	hdrlen;

		outer_ipha = (ipha_t *)mp->b_rptr;
		hdrlen = IPH_HDR_LENGTH(outer_ipha);

		if (mp->b_wptr - mp->b_rptr < hdrlen + sizeof (ipha_t)) {
			if (!pullupmsg(mp, hdrlen + sizeof (ipha_t))) {
				mutex_enter(&atp->tun_lock);
				atp->tun_OutErrors++;
				mutex_exit(&atp->tun_lock);
				freemsg(mp);
				return (0);	/* silently fail */
			}
			outer_ipha = (ipha_t *)mp->b_rptr;
		}

		inner_ipha = (ipha_t *)((uint8_t *)outer_ipha + hdrlen);
		outer_ipha->ipha_length = htons(ntohs(inner_ipha->ipha_length) +
		    sizeof (ipha_t));

		if (inner_ipha->ipha_dst == outer_ipha->ipha_dst) {
			/*
			 * Infinite loop check.  See the TUN_L_V4 case in
			 * tun_wdata_v4() for details.
			 */
			TUNDEBUG(TUN_DBGERR,
			    ("tun_wdata: inner dst == tunnel dst.\n"));
			atp->tun_OutErrors++;
			freemsg(mp);
			return (EINVAL);
		}
	} else {
		/* XXX can't get here yet - force assert */
		ASSERT((atp->tun_flags & TUN_L_V4) != 0);
		freemsg(mp);
		return (EINVAL);
	}

	if (inner_ipha->ipha_dst == atp->tun_ipha.ipha_dst) {
		/*
		 * Watch out!  There is potential for an infinite loop.
		 * If IP sent a packet with destination address equal
		 * to the tunnel's destination address, we'll hit
		 * an infinite routing loop, where the packet will keep
		 * going through here.
		 *
		 * In the long term, perhaps IP should be somewhat
		 * intelligent about this.  Until then, nip this in
		 * the bud.
		 */
		TUNDEBUG(TUN_DBGERR,
		    ("tun_wdata: inner dst == tunnel dst.\n"));
		atp->tun_OutErrors++;
		freemsg(mp);
		return (EINVAL);
	}

	/*
	 * increment mib counters and pass message off to ip
	 * note: we must always increment packet counters, but
	 * only increment byte counter if we actually send packet
	 */

	mutex_enter(&atp->tun_lock);
	if (CLASSD(inner_ipha->ipha_dst)) {
		atp->tun_HCOutMulticastPkts++;
	} else {
		atp->tun_HCOutUcastPkts++;
	}
	mutex_exit(&atp->tun_lock);

	if (!(atp->tun_flags & TUN_BOUND)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_OutErrors++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return (EINVAL);
	}

	mutex_enter(&atp->tun_lock);
	atp->tun_HCOutOctets += msgsize(mp);
	mutex_exit(&atp->tun_lock);

	/* send the packet down the transport stream to IPv4 */
	putnext(q, mp);
	return (0);
}

/*
 * put M_DATA fastpath upper IPv6
 * Assumes canput is possible
 */
static int
tun_wputnext_v6(queue_t *q, mblk_t *mp)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	ip6_t		*ip6h;

	/*
	 * fastpath reserves a bit more then we can use.
	 * get rid of hardware bits.. ip below us will fill it in
	 */
	mp->b_rptr += atp->tun_extra_offset;
	if ((atp->tun_flags & TUN_L_V4) != 0) {
		uint_t	hdrlen;
		ipha_t	*ipha;

		ipha = (ipha_t *)mp->b_rptr;
		hdrlen = IPH_HDR_LENGTH(ipha);

		if (mp->b_wptr - mp->b_rptr < hdrlen + sizeof (ip6_t)) {
			if (!pullupmsg(mp, hdrlen + sizeof (ip6_t))) {
				mutex_enter(&atp->tun_lock);
				atp->tun_OutErrors++;
				mutex_exit(&atp->tun_lock);
				freemsg(mp);
				return (0);	/* silently fail */
			}
			ipha = (ipha_t *)mp->b_rptr;
		}

		ip6h = (ip6_t *)((uint8_t *)ipha + hdrlen);
		/*
		 * if we are less than the minimum IPv6 mtu size, then
		 * allow IPv4 to fragment the packet
		 */
		if (ntohs(ip6h->ip6_plen) + IPV6_HDR_LEN <= IPV6_MIN_MTU) {
			ipha->ipha_fragment_offset_and_flags = 0;
		} else {
			ipha->ipha_fragment_offset_and_flags = htons(IPH_DF);
		}
		ipha->ipha_length = htons(ntohs(ip6h->ip6_plen) +
		    (uint16_t)sizeof (ip6_t) + (uint16_t)sizeof (ipha_t));

	} else {
		/* XXX can't get here yet - force assert */
		ASSERT((atp->tun_flags & TUN_L_V4) != 0);
		ip6h = (ip6_t *)mp->b_rptr;
		/* XXX - massage any IPv6 header things here */
		ip6h = &ip6h[1];
		freemsg(mp);
		return (EINVAL);
	}

	/*
	 * increment mib counters and pass message off to ip
	 * note: we must always increment packet counters, but
	 * only increment byte counter if we actually send packet
	 */

	mutex_enter(&atp->tun_lock);
	if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst)) {
		atp->tun_HCOutMulticastPkts++;
	} else {
		atp->tun_HCOutUcastPkts++;
	}
	mutex_exit(&atp->tun_lock);

	if (!(atp->tun_flags & TUN_BOUND)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_OutErrors++;
		mutex_exit(&atp->tun_lock);
		freemsg(mp);
		return (EINVAL);
	}

	mutex_enter(&atp->tun_lock);
	atp->tun_HCOutOctets += msgsize(mp);
	mutex_exit(&atp->tun_lock);

	/* send the packet down the transport stream to IPv4 */
	putnext(q, mp);
	return (0);
}

/*
 * Handle Upper IPv6 write side data
 * Note: all lower tunnels must have a source
 * This routine assumes that a canput has already been done on the
 * stream.
 * XXX we currently don't support a lower tunnel of V6
 * XXX currently, no locking.. this may change
 */
static int
tun_wdata_v6(queue_t *q, mblk_t *mp)
{
	tun_t		*atp = (tun_t *)q->q_ptr;
	ipha_t		*ipha;
	ip6_t		*ip6h;
	mblk_t		*newmp;
	ipaddr_t	v4addr;
#ifdef DEBUG
	char		buf1[INET_ADDRSTRLEN];
	char		buf2[INET_ADDRSTRLEN];
#endif
	char		buf[TUN_WHO_BUF];

	ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (ip6_t));

	ip6h = (ip6_t *)mp->b_rptr;

	/*
	 * increment mib counters and pass message off to ip
	 * note: we must always increment packet counters, but
	 * only increment byte counter if we actually send packet
	 */

	mutex_enter(&atp->tun_lock);
	if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst)) {
		atp->tun_HCOutMulticastPkts++;
	} else {
		atp->tun_HCOutUcastPkts++;
	}
	mutex_exit(&atp->tun_lock);

	if (atp->tun_state != DL_IDLE || !(atp->tun_flags & TUN_BOUND)) {
		mutex_enter(&atp->tun_lock);
		atp->tun_OutErrors++;
		mutex_exit(&atp->tun_lock);
		goto drop;
	}

	/* check version  */

	ASSERT((ip6h->ip6_vcf & IPV6_VERS_AND_FLOW_MASK) ==
	    IPV6_DEFAULT_VERS_AND_FLOW);

	switch (atp->tun_flags & TUN_LOWER_MASK) {
	case TUN_L_V4:
		/* room for IPv4 header? */
		if ((mp->b_rptr - mp->b_datap->db_base) < sizeof (ipha_t)) {
			/* no */

			newmp = allocb(sizeof (ipha_t) + atp->tun_extra_offset,
			    BPRI_HI);
			if (newmp == NULL) {
				mutex_enter(&atp->tun_lock);
				atp->tun_OutDiscard++;
				atp->tun_allocbfail++;
				mutex_exit(&atp->tun_lock);
				goto drop;
			}
			newmp->b_cont = mp;
			mp = newmp;
			mp->b_wptr = mp->b_datap->db_lim;
			mp->b_rptr = mp->b_wptr - sizeof (ipha_t);
		} else {
			/* yes */
			mp->b_rptr -= sizeof (ipha_t);
		}
		ipha = (ipha_t *)mp->b_rptr;

		/*
		 * copy template header into packet IPv4 header
		 * for configured tunnels everything should be
		 * in template.
		 * Automatic tunnels need the dest set from
		 * incoming ipv6 packet
		 */
		*ipha = atp->tun_ipha;

		/* XXX don't support tun_laddr of 0 */
		ASSERT(IN6_IS_ADDR_V4MAPPED(&atp->tun_laddr));

		/* Is this an automatic tunnel ? */
		if ((atp->tun_flags & TUN_AUTOMATIC) != 0) {
			/*
			 * Process packets for automatic tunneling
			 */
			IN6_V4MAPPED_TO_IPADDR(&atp->tun_laddr,
			    ipha->ipha_src);

			/*
			 * destination address must be compatible address
			 * and cannot be multicast
			 */
			if (!IN6_IS_ADDR_V4COMPAT(&ip6h->ip6_dst)) {
#ifdef DEBUG
				TUNDEBUG(TUN_DBGERR,
				    ("tun_wdata_v6: %s dest is not IPv4: %s\n",
				    tun_who(q, buf),
				    inet_ntop(AF_INET6, &ip6h->ip6_dst,
				    buf1, sizeof (buf1))));
#endif
				mutex_enter(&atp->tun_lock);
				atp->tun_OutErrors++;
				mutex_exit(&atp->tun_lock);
				goto drop;
			}
			IN6_V4MAPPED_TO_IPADDR(&ip6h->ip6_dst, v4addr);
			if (CLASSD(v4addr)) {
#ifdef DEBUG
				TUNDEBUG(TUN_DBGERR,
				    ("tun_wdata_v6: %s Multicast dst not"
				    " allowed : %s\n",
				    tun_who(q, buf),
				    inet_ntop(AF_INET6, &ip6h->ip6_src,
				    buf2, sizeof (buf2))));
#endif
				mutex_enter(&atp->tun_lock);
				atp->tun_OutErrors++;
				mutex_exit(&atp->tun_lock);
				goto drop;
			}
			ipha->ipha_dst = v4addr;
		}
		/*
		 * If IPv4 mtu is less than the minimum IPv6 mtu size, then
		 * allow IPv4 to fragment the packet.
		 * This works because if our IPv6 length is less than
		 * min IPv6 mtu, IPv4 might have to fragment anyway
		 * and we really can't handle an message too big icmp
		 * error.  If the packet is greater them min IPv6 mtu,
		 * then a message too big icmp error will cause the
		 * IPv6 to shrink its packets
		 */
		if (ntohs(ip6h->ip6_plen) + IPV6_HDR_LEN <= IPV6_MIN_MTU) {
			ipha->ipha_fragment_offset_and_flags = 0;
		} else {
			ipha->ipha_fragment_offset_and_flags = htons(IPH_DF);
		}
		ipha->ipha_length = htons(ntohs(ip6h->ip6_plen) +
		    (uint16_t)sizeof (ip6_t) + (uint16_t)sizeof (ipha_t));
#ifdef DEBUG
		TUNDEBUG(TUN_DBG3,
		    ("tun_wdata_ipv6: %s sending IPv4 packet src %s dest %s\n",
		    tun_who(q, buf),
		    inet_ntop(AF_INET, &ipha->ipha_src, buf1, sizeof (buf1)),
		    inet_ntop(AF_INET, &ipha->ipha_dst,
			buf2, sizeof (buf2))));
#endif
		break;
	case TUN_L_V6:
	default:
		/* LINTED */
		ASSERT(0 && "not supported");
		mutex_enter(&atp->tun_lock);
		atp->tun_OutErrors++;
		mutex_exit(&atp->tun_lock);
		goto drop;
	}

	mutex_enter(&atp->tun_lock);
	atp->tun_HCOutOctets += msgdsize(mp);
	mutex_exit(&atp->tun_lock);

	/* send the packet down the transport stream to IP */
	putnext(q, mp);
	return (0);
drop:
	freemsg(mp);
	return (0);
}

/*
 * T_BIND to lower stream.
 */
static int
tun_send_bind_req(queue_t *q, mblk_t *orig_mp)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	mblk_t	*mp;
	struct	T_bind_req *tbr;
	int	err = 0;
	size_t	size;
	uint_t	lvers;
	char	*cp;

	if ((atp->tun_flags & TUN_SRC) == 0) {
		return (EINVAL);
	}

	lvers = atp->tun_flags & TUN_LOWER_MASK;

	if (lvers == TUN_L_V4) {
		if (atp->tun_flags & TUN_SRC) {
			ASSERT(!(IN6_IS_ADDR_UNSPECIFIED(&atp->tun_laddr)));
			if (atp->tun_flags & TUN_DST) {
				ASSERT(!(IN6_IS_ADDR_UNSPECIFIED(
				    &atp->tun_faddr)));
				size = sizeof (ipa_conn_t);
			} else {
				size = sizeof (sin_t);
			}
		} else {
			return (EINVAL);
		}
	} else {	/* lower is V6 */
#if (LOWER_V6 == SUPPORTED)
		if (atp->tun_flags & TUN_SRC) {
			ASSERT(!(IN6_IS_ADDR_UNSPECIFIED(&atp->tun_laddr)));
			if (atp->tun_flags & TUN_DST) {
				ASSERT(!(IN6_IS_ADDR_UNSPECIFIED(
				    &atp->tun_faddr)));
				size = sizeof (ipa6_conn_t);
			} else {
				size = sizeof (sin6_t);
			}
		} else {
			return (EINVAL);
		}
#else
		return (EINVAL);
#endif
	}

	/* allocate an mblk */
	if ((mp = tun_realloc_mblk(q, NULL, size + sizeof (struct T_bind_req) +
	    1, orig_mp, B_FALSE)) == NULL) {
		TUNDEBUG(TUN_DBGERR,
		    ("tun_send_bind_req: couldn't allocate mblk\n"));
		return (ENOMEM);
	}
	if ((mp->b_cont = tun_realloc_mblk(q, NULL, sizeof (ire_t), orig_mp,
	    B_FALSE)) == NULL) {
		TUNDEBUG(TUN_DBGERR,
		    ("tun_send_bind_req: couldn't allocate mblk\n"));
		freeb(mp);
		return (ENOMEM);
	}
	mp->b_cont->b_datap->db_type = IRE_DB_REQ_TYPE;
	tbr = (struct T_bind_req *)mp->b_rptr;
	tbr->CONIND_number = 0;
	tbr->PRIM_type = T_BIND_REQ;
	tbr->ADDR_length = size;
	tbr->ADDR_offset = sizeof (struct T_bind_req);
	cp = (char *)&tbr[1];
	if (lvers == TUN_L_V4) {

		/* Send a T_BIND_REQ down to IP to bind to IPPROTO_IPV6 */

		/* Source is always required */
		ASSERT((atp->tun_flags & TUN_SRC) &&
		    !IN6_IS_ADDR_UNSPECIFIED(&atp->tun_laddr));

		if (!(atp->tun_flags & TUN_DST) ||
		    IN6_IS_ADDR_UNSPECIFIED(&atp->tun_faddr)) {
			sin_t		*sin;

			sin = (sin_t *)cp;
			bzero(sin, sizeof (sin_t));
			IN6_V4MAPPED_TO_IPADDR(&atp->tun_laddr,
			    sin->sin_addr.s_addr);
			sin->sin_port = 0;
		} else {
			ipa_conn_t	*ipa;

			if (!IN6_IS_ADDR_V4MAPPED(&atp->tun_faddr)) {
				err = EINVAL;
				goto error;
			}
			ipa = (ipa_conn_t *)cp;
			bzero(ipa, sizeof (ipa_conn_t));
			IN6_V4MAPPED_TO_IPADDR(&atp->tun_laddr, ipa->ac_laddr);
			IN6_V4MAPPED_TO_IPADDR(&atp->tun_faddr, ipa->ac_faddr);
			ipa->ac_fport = 0;
			ipa->ac_lport = 0;
		}
		if ((atp->tun_flags & TUN_UPPER_MASK) == TUN_U_V6)
			*(cp + size) = (uchar_t)IPPROTO_IPV6;
		else
			*(cp + size) = (uchar_t)IPPROTO_ENCAP;
	} else {
		/* IPv6 not supported */
		err = EINVAL;
		goto error;
	}
	mp->b_datap->db_type = M_PCPROTO;

	atp->tun_flags |= TUN_BIND_SENT;
	putnext(WR(q), mp);
	return (0);
error:
	freemsg(mp);
	return (err);
}

/*
 * Update kstats
 */
static int
tun_stat_kstat_update(kstat_t *ksp, int rw)
{
	tun_t *tunp;
	tun_stats_t *tstats;
	struct tunstat *tunsp;

	tstats = (tun_stats_t *)ksp->ks_private;
	mutex_enter(&tstats->ts_lock);
	tunsp = (struct tunstat *)ksp->ks_data;

	/* Initialize kstat, but only the first one */
	if (rw == KSTAT_WRITE) {
		if (tstats->ts_refcnt > 1) {
			mutex_exit(&tstats->ts_lock);
			return (EACCES);
		}
		tunp = tstats->ts_atp;

		/*
		 * MIB II kstat variables
		 */
		tunp->tun_nocanput	= tunsp->tuns_nocanput.value.ui32;
		tunp->tun_xmtretry	= tunsp->tuns_xmtretry.value.ui32;
		tunp->tun_allocbfail	= tunsp->tuns_allocbfail.value.ui32;
		tunp->tun_InDiscard	= tunsp->tuns_InDiscard.value.ui32;
		tunp->tun_InErrors	= tunsp->tuns_InErrors.value.ui32;
		tunp->tun_OutDiscard	= tunsp->tuns_OutDiscard.value.ui32;
		tunp->tun_OutErrors	= tunsp->tuns_OutErrors.value.ui32;

		tunp->tun_HCInOctets	= tunsp->tuns_HCInOctets.value.ui64;
		tunp->tun_HCInUcastPkts	= tunsp->tuns_HCInUcastPkts.value.ui64;
		tunp->tun_HCInMulticastPkts =
		    tunsp->tuns_HCInMulticastPkts.value.ui64;
		tunp->tun_HCOutOctets	= tunsp->tuns_HCOutOctets.value.ui64;
		tunp->tun_HCOutUcastPkts =
		    tunsp->tuns_HCOutUcastPkts.value.ui64;
		tunp->tun_HCOutMulticastPkts =
		    tunsp->tuns_HCOutMulticastPkts.value.ui64;
		mutex_exit(&tstats->ts_lock);
		return (0);
	}
	/*
	 * update kstats.. fist zero them all out, then
	 * walk through all the interfaces that share kstat and
	 * add in the new stats
	 */
	tunsp->tuns_nocanput.value.ui32 = 0;
	tunsp->tuns_xmtretry.value.ui32 = 0;
	tunsp->tuns_allocbfail.value.ui32 = 0;
	tunsp->tuns_InDiscard.value.ui32 = 0;
	tunsp->tuns_InErrors.value.ui32 = 0;
	tunsp->tuns_OutDiscard.value.ui32 = 0;
	tunsp->tuns_OutErrors.value.ui32 = 0;
	tunsp->tuns_HCInOctets.value.ui64 = 0;
	tunsp->tuns_HCInUcastPkts.value.ui64 = 0;
	tunsp->tuns_HCInMulticastPkts.value.ui64 = 0;
	tunsp->tuns_HCOutOctets.value.ui64 = 0;
	tunsp->tuns_HCOutUcastPkts.value.ui64 = 0;
	tunsp->tuns_HCOutMulticastPkts.value.ui64 = 0;

	for (tunp = tstats->ts_atp; tunp; tunp = tunp->tun_next) {
		tunsp->tuns_nocanput.value.ui32 += tunp->tun_nocanput;
		tunsp->tuns_xmtretry.value.ui32 += tunp->tun_xmtretry;
		tunsp->tuns_allocbfail.value.ui32 += tunp->tun_allocbfail;
		tunsp->tuns_InDiscard.value.ui32 += tunp->tun_InDiscard;
		tunsp->tuns_InErrors.value.ui32 += tunp->tun_InErrors;
		tunsp->tuns_OutDiscard.value.ui32 += tunp->tun_OutDiscard;
		tunsp->tuns_OutErrors.value.ui32 += tunp->tun_OutErrors;

		tunsp->tuns_HCInOctets.value.ui64 += tunp->tun_HCInOctets;
		tunsp->tuns_HCInUcastPkts.value.ui64 += tunp->tun_HCInUcastPkts;
		tunsp->tuns_HCInMulticastPkts.value.ui64 +=
		    tunp->tun_HCInMulticastPkts;
		tunsp->tuns_HCOutOctets.value.ui64 += tunp->tun_HCOutOctets;
		tunsp->tuns_HCOutUcastPkts.value.ui64 +=
		    tunp->tun_HCOutUcastPkts;
		tunsp->tuns_HCOutMulticastPkts.value.ui64 +=
		    tunp->tun_HCOutMulticastPkts;
	}
	tunsp->tuns_xmtbytes.value.ui32 =
	    tunsp->tuns_HCOutOctets.value.ui64 & 0xffffffff;
	tunsp->tuns_rcvbytes.value.ui32 =
	    tunsp->tuns_HCInOctets.value.ui64 & 0xffffffff;
	tunsp->tuns_opackets.value.ui32 =
	    tunsp->tuns_HCOutUcastPkts.value.ui64 & 0xffffffff;
	tunsp->tuns_ipackets.value.ui32 =
	    tunsp->tuns_HCInUcastPkts.value.ui64 & 0xffffffff;
	tunsp->tuns_multixmt.value.ui32 =
	    tunsp->tuns_HCOutMulticastPkts.value.ui64 & 0xffffffff;
	tunsp->tuns_multircv.value.ui32 =
	    tunsp->tuns_HCInMulticastPkts.value.ui64 & 0xffffffff;
	mutex_exit(&tstats->ts_lock);
	return (0);
}

/*
 * Initialize kstats
 */
static void
tun_statinit(tun_stats_t *tun_stat, char *modname)
{
	kstat_t	*ksp;
	struct tunstat *tunsp;
	char buf[32];
	char *mod_buf;

	/*
	 * create kstat name based on lower ip and ppa
	 */
	if (tun_stat->ts_lower == TUN_L_V4) {
		mod_buf = "ip";
	} else {
		mod_buf = "ip6";
	}
	(void) sprintf(buf, "%s.%s%d", mod_buf, modname, tun_stat->ts_ppa);
	TUNDEBUG(TUN_DBG1,
		("tunstatinit: Creating kstat %s\n", buf));
	if ((ksp = kstat_create(mod_buf, tun_stat->ts_ppa, buf, "net",
	    KSTAT_TYPE_NAMED, sizeof (struct tunstat) / sizeof (kstat_named_t),
	    KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_CONT, "tun: kstat_create failed tun%d",
		    tun_stat->ts_ppa);
		return;
	}
	mutex_enter(&tun_stat->ts_lock);
	tun_stat->ts_ksp = ksp;
	mutex_exit(&tun_stat->ts_lock);
	tunsp = (struct tunstat *)(ksp->ks_data);
	kstat_named_init(&tunsp->tuns_ipackets, "ipackets", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_opackets, "opackets", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_InErrors, "ierrors", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_OutErrors, "oerrors", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_xmtbytes, "obytes", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_rcvbytes, "rbytes", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_multixmt, "multixmt", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_multircv,	"multircv", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_nocanput, "blocked", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_xmtretry, "xmtretry", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_InDiscard, "norcvbuf", KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_OutDiscard, "noxmtbuF",
	    KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_allocbfail, "allocbfail",
	    KSTAT_DATA_UINT32);
	kstat_named_init(&tunsp->tuns_HCOutUcastPkts, "opackets64",
	    KSTAT_DATA_UINT64);
	kstat_named_init(&tunsp->tuns_HCInUcastPkts, "ipackets64",
	    KSTAT_DATA_UINT64);
	kstat_named_init(&tunsp->tuns_HCOutMulticastPkts, "multixmt64",
	    KSTAT_DATA_UINT64);
	kstat_named_init(&tunsp->tuns_HCInMulticastPkts, "multircv64",
	    KSTAT_DATA_UINT64);
	kstat_named_init(&tunsp->tuns_HCOutOctets, "obytes64",
	    KSTAT_DATA_UINT64);
	kstat_named_init(&tunsp->tuns_HCInOctets, "rbytes64",
	    KSTAT_DATA_UINT64);

	ksp->ks_update = tun_stat_kstat_update;
	ksp->ks_private = (void *) tun_stat;
	kstat_install(ksp);
}

/*
 * Debug routine to print out tunnel name
 */
static char *
tun_who(queue_t *q, char *buf)
{
	tun_t	*atp = (tun_t *)q->q_ptr;
	char ppa_buf[20];

	if (buf == NULL)
		return ("tun_who: no buf");

	if (atp->tun_state != DL_UNATTACHED) {
		(void) sprintf(ppa_buf, "%d", atp->tun_ppa);
	} else {
		(void) sprintf(ppa_buf, "<not attached>");
	}

	(void) sprintf(buf, "%s.%s%s (%s)",
	    (atp->tun_flags & TUN_LOWER_MASK) == TUN_L_V4 ? "ip" :
	    (atp->tun_flags & TUN_LOWER_MASK) == TUN_L_V6 ? "ip6" : "<unknown>",
	    q->q_qinfo->qi_minfo->mi_idname,
	    ppa_buf,
	    (atp->tun_flags & TUN_UPPER_MASK) == TUN_U_V4 ? "inet" :
	    (atp->tun_flags & TUN_UPPER_MASK) == TUN_U_V6 ? "inet6" :
	    "<unknown af>");
	return (buf);
}
