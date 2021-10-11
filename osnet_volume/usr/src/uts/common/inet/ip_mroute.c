/*
 * Copyright (c) 1995, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip_mroute.c	1.43	99/10/21 SMI"

/*
 * Procedures for the kernel part of DVMRP,
 * a Distance-Vector Multicast Routing Protocol.
 * (See RFC-1075)
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1995
 *
 * MROUTING 3.5
 */

/*
 * TODO
 * - function pointer field in vif, void *vif_sendit()
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

#include <sys/param.h>
#include <sys/socket.h>
#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/sockio.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/if_dl.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/mib2.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/snmpcom.h>

#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <netinet/udp.h>
#include <netinet/ip_mroute.h>
#include <inet/ip_multi.h>
#include <inet/ip_ire.h>
#include <inet/ip_if.h>

#include <netinet/pim.h>

/*
 * MT design:
 *	All accesses through ip_mrouter_*() are single-threaded because
 *      IP gets a writers lock before ip_mrouter_* is called. In these cases
 *      locks aren't needed. Any activity in the data path, e.g., ip_mforward,
 *      or tbf functions requires mutexes when they modify IP data structures.
 */

/*
 * Locks:
 * 	Locked data structures and their locks are:
 *	Mfctable -- protected by kcache_lock, global.
 *	Each vif's tbf -- protected by vif.v_tbf->tbf_lock, per vif.
 * 	Last_encap_src and last_encap_vif -
 *	protected by last_encap_lock, global.
 *
 *	The statistics are currently not protected by a lock,
 * 	causing the stats be be approximate, not exact.
 */

/*
 * Globals
 * All but ip_g_mrouter and ip_mrtproto could be static,
 * except for netstat or debugging purposes.
 */
queue_t		*ip_g_mrouter	= NULL;
int		ip_mrtproto	= IGMP_DVMRP;	/* for netstat only */
struct mrtstat	mrtstat;	/* Stats for netstat */

#define	NO_VIF	MAXVIFS 	/* from mrouted, no route for src */

/*
 * Timeouts:
 * 	Upcall timeouts - BSD uses boolean_t mfc->expire and
 *	nexpire[MFCTBLSIZE], the number of times expire has been called.
 *	SunOS 5.x uses mfc->timeout for each mfc.
 *	Some Unixes are limited in the number of simultaneous timeouts
 * 	that can be run, SunOS 5.x does not have this restriction.
 */

/*
 * In BSD, EXPIRE_TIMEOUT is how often expire_upcalls() is called and
 * UPCALL_EXPIRE is the nmber of timeouts before a particular upcall
 * expires. Thus the time till expiration is EXPIRE_TIMEOUT * UPCALL_EXPIRE
 */
#define		EXPIRE_TIMEOUT	(hz/4)	/* 4x / second	*/
#define		UPCALL_EXPIRE	6	/* number of timeouts	*/

/*
 * Hash function for a source, group entry
 */
#define	MFCHASH(a, g) MFCHASHMOD(((a) >> 20) ^ ((a) >> 10) ^ (a) ^ \
	((g) >> 20) ^ ((g) >> 10) ^ (g))

/*
 * mfctable:
 * Includes all mfcs, including waiting upcalls.
 * Multiple mfcs per bucket.
 */
static struct mfc	*mfctable[MFCTBLSIZ];	/* kernel routing table	*/
static kmutex_t		kcache_lock;		/* Protects mfctable	*/

/*
 * Define the token bucket filter structures.
 * tbftable -> each vif has one of these for storing info.
 */
struct tbf 		tbftable[MAXVIFS];
#define			TBF_REPROCESS	(hz / 100)	/* 100x /second	*/

/* Identify PIM packet that came on a Register interface */
#define	PIM_REGISTER_MARKER	0xffffffff

/* Function declarations */
static int	add_mfc(struct mfcctl *);
static int	add_vif(struct vifctl *);
static int	del_mfc(struct mfcctl *);
static int	del_vif(vifi_t *);
static void	encap_send(ipha_t *, mblk_t *, struct vif *, ipaddr_t);
static void	expire_upcalls(void *);
static void	fill_route(struct mfc *, struct mfcctl *);
static int	get_assert(uchar_t *);
static int	get_lsg_cnt(struct sioc_lsg_req *);
static int	get_sg_cnt(struct sioc_sg_req *);
static int	get_version(uchar_t *);
static int	get_vif_cnt(struct sioc_vif_req *);
static int	ip_mdq(mblk_t *, ipha_t *, ill_t *,
		    ipaddr_t, struct mfc *);
static int	ip_mrouter_init(queue_t *, uchar_t *, int);
static void	phyint_send(ipha_t *, mblk_t *, struct vif *, ipaddr_t);
static int	register_mforward(queue_t *, mblk_t *);
static void	register_send(ipha_t *, mblk_t *, struct vif *, ipaddr_t);
static int	set_assert(int *);

/*
 * Token Bucket Filter functions
 */
static int  priority(struct vif *, ipha_t *);
static void tbf_control(struct vif *, mblk_t *, ipha_t *);
static int  tbf_dq_sel(struct vif *, ipha_t *);
static void tbf_process_q(struct vif *);
static void tbf_queue(struct vif *, mblk_t *);
static void tbf_reprocess_q(void *);
static void tbf_send_packet(struct vif *, mblk_t *);
static void tbf_update_tokens(struct vif *);

/*
 * Encapsulation packets
 */

#define	ENCAP_TTL	64

/* prototype IP hdr for encapsulated packets */
static ipha_t multicast_encap_iphdr = {
	IP_SIMPLE_HDR_VERSION,
	0,				/* tos */
	sizeof (ipha_t),		/* total length */
	0,				/* id */
	0,				/* frag offset */
	ENCAP_TTL, IPPROTO_ENCAP,
	0,				/* checksum */
};

/*
 * Private variables.
 */
static int		saved_ip_g_forward = -1;
static vifi_t		numvifs = 0;
static struct vif	viftable[MAXVIFS+1];	/* Index needs to accomodate */
/* the value of NO_VIF, which */
/* is MAXVIFS. */

/*
 * One-back cache used to locate a tunnel's vif,
 * given a datagram's src ip address.
 */
static ipaddr_t		last_encap_src;
static struct vif	*last_encap_vif;
static kmutex_t		last_encap_lock;	/* Protects the above */

/*
 * Whether or not special PIM assert processing is enabled.
 */
static int pim_assert;
static vifi_t reg_vif_num = ALL_VIFS; 	/* Index to Register vif */

/*
 * Rate limit for assert notification messages, in nsec.
 */
#define	ASSERT_MSG_TIME		3000000000


/*
 * MFCFIND:
 * Find a route for a given origin IP address and multicast group address.
 * Skip entries with pending upcalls.
 * Type of service parameter to be added in the future!
 */
#define	MFCFIND(o, g, rt) { \
	struct mfc *_mb_rt = mfctable[MFCHASH(o, g)]; \
	rt = NULL; \
	while (_mb_rt) { \
		if ((_mb_rt->mfc_origin.s_addr == o) && \
		    (_mb_rt->mfc_mcastgrp.s_addr == g) && \
		    (_mb_rt->mfc_rte == NULL)) { \
			rt = _mb_rt; \
			break; \
		} \
	_mb_rt = _mb_rt->mfc_next; \
	} \
}

/*
 * BSD uses timeval with sec and usec. In SunOS 5.x uniqtime() and gethrtime()
 * are inefficient. We use gethrestime() which returns a timespec_t with
 * sec and nsec, the resolution is machine dependent.
 * The following 2 macros have been changed to use nsec instead of usec.
 */
/*
 * Macros to compute elapsed time efficiently.
 * Borrowed from Van Jacobson's scheduling code.
 * Delta should be a hrtime_t.
 */
#define	TV_DELTA(a, b, delta) { \
	int xxs; \
 \
	delta = (a).tv_nsec - (b).tv_nsec; \
	if ((xxs = (a).tv_sec - (b).tv_sec) != 0) { \
		switch (xxs) { \
		case 2: \
		    delta += 1000000000; \
		    /*FALLTHROUGH*/ \
		case 1: \
		    delta += 1000000000; \
		    break; \
		default: \
		    delta += (1000000000 * xxs); \
		} \
	} \
}

#define	TV_LT(a, b) (((a).tv_nsec < (b).tv_nsec && \
	(a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

#ifdef UPCALL_TIMING
static void collate(timespec_t *);
#endif /* UPCALL_TIMING */

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
int
ip_mrouter_set(int cmd, queue_t *q, int checkonly, uchar_t *data,
    int datalen)
{
	if (cmd != MRT_INIT && q != ip_g_mrouter)
		return (EACCES);

	if (checkonly) {
		/*
		 * do not do operation, just pretend to - new T_CHECK
		 * Note: Even routines further on can probably fail but
		 * this T_CHECK stuff is only to please XTI so it not
		 * necessary to be perfect.
		 */
		switch (cmd) {
		case MRT_INIT:
		case MRT_DONE:
		case MRT_ADD_VIF:
		case MRT_DEL_VIF:
		case MRT_ADD_MFC:
		case MRT_DEL_MFC:
		case MRT_ASSERT:
		    return (0);
		default:
		    return (EOPNOTSUPP);
		}
	}
	switch (cmd) {
	case MRT_INIT:	return (ip_mrouter_init(q, data, datalen));
	case MRT_DONE:	return (ip_mrouter_done());
	case MRT_ADD_VIF:  return (add_vif((struct vifctl *)data));
	case MRT_DEL_VIF:  return (del_vif((vifi_t *)data));
	case MRT_ADD_MFC:  return (add_mfc((struct mfcctl *)data));
	case MRT_DEL_MFC:  return (del_mfc((struct mfcctl *)data));
	case MRT_ASSERT:   return (set_assert((int *)data));
	default:	   return (EOPNOTSUPP);
	}
}

/*
 * Handle MRT getsockopt commands
 */
int
ip_mrouter_get(int cmd, queue_t *q, uchar_t *data)
{
	if (q != ip_g_mrouter)
		return (EACCES);

	switch (cmd) {
	case MRT_VERSION:	return (get_version((uchar_t *)data));
	case MRT_ASSERT:	return (get_assert((uchar_t *)data));
	default:		return (EOPNOTSUPP);
	}
}

/*
 * Handle ioctl commands to obtain information from the cache.
 * Called with shared access to IP. These are read_only ioctls.
 */
int
mrt_ioctl(int cmd, intptr_t data)
{
	switch (cmd) {
	case (SIOCGETVIFCNT):
		return (get_vif_cnt((struct sioc_vif_req *)data));
	case (SIOCGETSGCNT):
		return (get_sg_cnt((struct sioc_sg_req *)data));
	case (SIOCGETLSGCNT):
		return (get_lsg_cnt((struct sioc_lsg_req *)data));
	default:
		return (EINVAL);
	}
}

/*
 * Returns the packet, byte, rpf-failure count for the source, group provided.
 */
static int
get_sg_cnt(struct sioc_sg_req *req)
{
	struct mfc *rt;

	/* no lock */
	MFCFIND(req->src.s_addr, req->grp.s_addr, rt);

	if (rt != NULL) {
		req->pktcnt   = rt->mfc_pkt_cnt;
		req->bytecnt  = rt->mfc_byte_cnt;
		req->wrong_if = rt->mfc_wrong_if;
	} else
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffffU;

	return (0);
}

/*
 * Returns the packet, byte, rpf-failure count for the source, group provided.
 * Uses larger counters and IPv6 addresses.
 */
/* ARGSUSED XXX until implemented */
static int
get_lsg_cnt(struct sioc_lsg_req *req)
{
	/* XXX TODO SIOCGETLSGCNT */
	return (ENXIO);
}

/*
 * Returns the input and output packet and byte counts on the vif provided.
 */
static int
get_vif_cnt(struct sioc_vif_req *req)
{
	vifi_t vifi = req->vifi;

	if (vifi >= numvifs)
		return (EINVAL);

	req->icount = viftable[vifi].v_pkt_in;
	req->ocount = viftable[vifi].v_pkt_out;
	req->ibytes = viftable[vifi].v_bytes_in;
	req->obytes = viftable[vifi].v_bytes_out;

	return (0);
}

static int
get_version(uchar_t *data)
{
	int *v = (int *)data;

	*v = 0x0305;	/* XXX !!!! */

	return (0);
}

/*
 * Set PIM assert processing global.
 */
static int
set_assert(int *i)
{
	if ((*i != 1) && (*i != 0))
		return (EINVAL);

	pim_assert = *i;

	return (0);
}

/*
 * Get PIM assert processing global.
 */
static int
get_assert(uchar_t *data)
{
	int *i = (int *)data;

	*i = pim_assert;

	return (0);
}

/*
 * Enable multicast routing.
 */
static int
ip_mrouter_init(queue_t *q, uchar_t *data, int datalen)
{
	ipc_t	*ipc = (ipc_t *)q->q_ptr;
	int		*v;

	if (data == NULL || (datalen != sizeof (int)))
		return (ENOPROTOOPT);

	v = (int *)data;
	if (*v != 1)
		return (ENOPROTOOPT);

	if (ip_g_mrouter != NULL)
		return (EADDRINUSE);

	ip_g_mrouter = q;
	ipc->ipc_multi_router = 1;

	mutex_init(&kcache_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&last_encap_lock, NULL, MUTEX_DEFAULT, NULL);

	bzero(mfctable, sizeof (mfctable));
	mrtstat.mrts_vifctlSize = sizeof (struct vifctl);
	mrtstat.mrts_mfcctlSize = sizeof (struct mfcctl);

	pim_assert = 0;

	/* In order for tunnels to work we have to turn ip_g_forward on */
	if (!WE_ARE_FORWARDING) {
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(q, 1, SL_TRACE,
			    "ip_mrouter_init: turning on forwarding");
		}
		saved_ip_g_forward = ip_g_forward;
		ip_g_forward = IP_FORWARD_ALWAYS;
	}

	return (0);
}

/*
 * Disable multicast routing.
 * Didn't use global timeout_val (BSD version), instead check the mfctable.
 */
int
ip_mrouter_done(void)
{
	ipc_t		*ipc = (ipc_t *)ip_g_mrouter->q_ptr;
	vifi_t 		vifi;
	struct mfc		*mfc_rt;
	struct rtdetq	*rte0;
	int			i;

	if (saved_ip_g_forward != -1) {
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "ip_mrouter_done: turning off forwarding");
		}
		ip_g_forward = saved_ip_g_forward;
		saved_ip_g_forward = -1;
	}

	/*
	 * Always clear cache when vifs change.
	 * No need to get last_encap_lock since we are running as a writer.
	 */
	last_encap_src = 0;
	last_encap_vif = NULL;
	ipc->ipc_multi_router = 0;

	/*
	 * For each phyint in use,
	 * disable promiscuous reception of all IP multicasts.
	 */
	for (vifi = 0; vifi < numvifs; vifi++) {
		/* lcl_addr.s_addr is not 0 if it has been initialized	*/
		/* and therefore the ipif is not null.	*/
		if (viftable[vifi].v_lcl_addr.s_addr != 0) {
			struct vif 	*vifp = viftable + vifi;
			ipif_t 	*ipif = vifp->v_ipif;
			struct tbf 	*t = vifp->v_tbf;
			mblk_t	*mp0;

			if (vifp->v_timeout_id != 0)
				(void) quntimeout(ipif->ipif_rq,
				    vifp->v_timeout_id);

			/*
			 * Free packets queued at the interface
			 */
			while (t->tbf_q != NULL) {
				mp0 = t->tbf_q;
				t->tbf_q = t->tbf_q->b_next;
				if (ip_mrtdebug > 1) {
					(void) mi_strlog(ip_g_mrouter,
					    1, SL_TRACE,
					    "ip_mrouter_done: free q'd pkt\n");
				}
				mp0->b_prev = mp0->b_next = NULL;
				freemsg(mp0);
			}

			mutex_destroy(&vifp->v_tbf->tbf_lock);
			/* phyint */
			if (!viftable[vifi].v_flags &
			    (VIFF_TUNNEL | VIFF_REGISTER))
				(void) ip_delmulti(INADDR_ANY, ipif);
		}
	}

	bzero(tbftable, sizeof (tbftable));
	bzero(viftable, sizeof (viftable));
	numvifs = 0;
	pim_assert = 0;
	reg_vif_num = ALL_VIFS;

	/*
	 * Free upcall msgs.
	 * Go through mfctable and stop any outstanding upcall
	 * timeouts remaining on mfcs.
	 */
	for (i = 0; i < MFCTBLSIZ; i++) {
		mfc_rt = mfctable[i];
		while (mfc_rt) {
			/* Free upcalls */
			if (mfc_rt->mfc_rte != NULL) {

				if (mfc_rt->mfc_timeout_id != 0) {
					(void) quntimeout(ip_g_mrouter,
					    mfc_rt->mfc_timeout_id);

					while (mfc_rt->mfc_rte !=
					    NULL) {
						rte0 = mfc_rt->mfc_rte;
						mfc_rt->mfc_rte =
						    rte0->rte_next;
						freemsg(rte0->mp);
						mi_free((char *)rte0);
					}
				}
			}
			mfc_rt = mfc_rt->mfc_next;
		}
	}
	mutex_destroy(&kcache_lock);
	mutex_destroy(&last_encap_lock);

	/*
	 * Free all multicast forwarding cache entries
	 */
	for (i = 0; i < MFCTBLSIZ; i++) {
		struct mfc *mfc, *next_mfc;

		for (mfc = mfctable[i]; mfc; mfc = next_mfc) {
			ASSERT(mfc->mfc_rte == NULL);
			next_mfc = mfc->mfc_next;
			mi_free((char *)mfc);
		}
	}
	bzero(mfctable, sizeof (mfctable));

	ip_g_mrouter = NULL;
	return (0);
}

/*
 * Add a vif to the vif table.
 */
static int
add_vif(struct vifctl *vifcp)
{
	struct vif	*vifp = viftable + vifcp->vifc_vifi;
	ipif_t		*ipif;
	int		error;
	struct tbf	*v_tbf = tbftable + vifcp->vifc_vifi;

	if (vifcp->vifc_vifi >= MAXVIFS)
		return (EINVAL);

	/* Viftable entry should be 0 */
	if (vifp->v_lcl_addr.s_addr != 0)
		return (EADDRINUSE);

	/* Incoming vif should not be 0 */
	if (vifcp->vifc_lcl_addr.s_addr == 0)
		return (EINVAL);

	/* Find the interface with the local address */
	ipif = ipif_lookup_addr((ipaddr_t)vifcp->vifc_lcl_addr.s_addr, NULL);
	if (ipif == NULL)
		return (EADDRNOTAVAIL);

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "add_vif: src 0x%x enter",
		    vifcp->vifc_lcl_addr.s_addr);
	}

	/*
	 * Always clear cache when vifs change.
	 * Needed to ensure that src isn't left over from before vif was added.
	 * No need to get last_encap_lock, since we are running as a writer.
	 */
	last_encap_src = 0;
	last_encap_vif = NULL;

	if (vifcp->vifc_flags & VIFF_TUNNEL) {
		if ((vifcp->vifc_flags & VIFF_SRCRT) != 0) {
			cmn_err(CE_WARN,
			    "add_vif: source route tunnels not supported\n");
			return (EOPNOTSUPP);
		}
		vifp->v_rmt_addr  = vifcp->vifc_rmt_addr;

	} else {
		/* Phyint or Register vif */
		if (vifcp->vifc_flags & VIFF_REGISTER) {
			/*
			 * Note: Since all IPPROTO_IP level options (including
			 * MRT_ADD_VIF) are done exclusively via
			 * ip_optmgmt_writer(), a lock is not necessary to
			 * protect reg_vif_num.
			 */
			if (reg_vif_num == ALL_VIFS)
				reg_vif_num = vifcp->vifc_vifi;
			else
				return (EADDRINUSE);
		}

		/* Make sure the interface supports multicast */
		if ((ipif->ipif_flags & IFF_MULTICAST) == 0) {
			return (EOPNOTSUPP);
		}
		/* Enable promiscuous reception of all IP mcasts from the if */
		error = ip_addmulti(INADDR_ANY, ipif);
		if (error) {
			return (error);
		}
	}
	/* Define parameters for the tbf structure */
	vifp->v_tbf = v_tbf;
	gethrestime(&vifp->v_tbf->tbf_last_pkt_t);
	vifp->v_tbf->tbf_n_tok = 0;
	vifp->v_tbf->tbf_q_len = 0;
	vifp->v_tbf->tbf_max_q_len = MAXQSIZE;
	vifp->v_tbf->tbf_q = vifp->v_tbf->tbf_t = NULL;

	vifp->v_flags = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	vifp->v_lcl_addr = vifcp->vifc_lcl_addr;
	vifp->v_ipif = ipif;
	/* Scaling up here, allows division by 1024 in critical code.	*/
	vifp->v_rate_limit = vifcp->vifc_rate_limit * (1024/1000);
	vifp->v_timeout_id = 0;
	/* initialize per vif pkt counters */
	vifp->v_pkt_in = 0;
	vifp->v_pkt_out = 0;
	vifp->v_bytes_in = 0;
	vifp->v_bytes_out = 0;
	mutex_init(&vifp->v_tbf->tbf_lock, NULL, MUTEX_DEFAULT, NULL);

	/* Adjust numvifs up, if the vifi is higher than numvifs */
	if (numvifs <= vifcp->vifc_vifi)
		numvifs = vifcp->vifc_vifi + 1;

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "add_vif: #%d, lcladdr %x, %s %x, thresh %x, rate %d",
		    vifcp->vifc_vifi,
		    ntohl(vifcp->vifc_lcl_addr.s_addr),
		    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
		    ntohl(vifcp->vifc_rmt_addr.s_addr),
		    vifcp->vifc_threshold, vifcp->vifc_rate_limit);
	}
	return (0);
}

/* Delete a vif from the vif table. */
static int
del_vif(vifi_t *vifip)
{
	struct vif 	*vifp = viftable + *vifip;
	vifi_t 	vifi;
	struct tbf	*t = vifp->v_tbf;
	mblk_t	*mp0;

	if (*vifip >= numvifs)
		return (EINVAL);

	/* Not initialized */
	if (vifp->v_lcl_addr.s_addr == 0)
		return (EADDRNOTAVAIL);

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "del_vif: src 0x%x\n", vifp->v_lcl_addr.s_addr);
	}

	if (vifp->v_timeout_id != 0) {
		(void) quntimeout(vifp->v_ipif->ipif_rq, vifp->v_timeout_id);
		vifp->v_timeout_id = 0;
	}

	/* Phyint only */
	if (!(vifp->v_flags & (VIFF_TUNNEL | VIFF_REGISTER))) {
		ipif_t *ipif = vifp->v_ipif;
		if (ipif) {
			(void) ip_delmulti(INADDR_ANY, ipif);
		}
	}

	/*
	 * Free packets queued at the interface.
	 * Mrouted takes care of cleaning up mfcs - makes calls to del_mfc.
	 */
	while (t->tbf_q != NULL) {
		mp0 = t->tbf_q;
		t->tbf_q = t->tbf_q->b_next;
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "del_vif: free q'd pkt vif %d\n", (int)*vifip);
		}
		mp0->b_prev = mp0->b_next = NULL;
		freemsg(mp0);
	}

	/*
	 * Always clear cache when vifs change.
	 * No need to get last_encap_lock since we are running as a writer.
	 */
	if (vifp == last_encap_vif) {
		last_encap_vif = NULL;
		last_encap_src = 0;
	}
	mutex_destroy(&t->tbf_lock);

	bzero(vifp->v_tbf, sizeof (*(vifp->v_tbf)));
	bzero(vifp, sizeof (*vifp));

	/* Adjust numvifs down */
	for (vifi = numvifs; vifi != 0; vifi--) /* vifi is unsigned */
		if (viftable[vifi - 1].v_lcl_addr.s_addr != 0) break;
	numvifs = vifi;

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "del_vif: end vif %d, (int)numvifs %d", *vifip, numvifs);
	}

	return (0);
}

/*
 * Add an mfc entry.
 */
static int
add_mfc(struct mfcctl *mfccp)
{
	struct mfc *rt;
	uint_t hash;
	struct rtdetq *rte;
	ushort_t nstl;
	int i;

	/*
	 * The value of vifi is NO_VIF (==MAXVIFS) if Mrouted
	 * did not have a real route for pkt.
	 * We want this pkt without rt installed in the mfctable to prevent
	 * multiiple tries, so go ahead and put it in mfctable, it will
	 * be discarded later in ip_mdq() because the child is NULL.
	 */

	/* Error checking, out of bounds? */
	if (mfccp->mfcc_parent > MAXVIFS) {
		ip0dbg(("ADD_MFC: mfcc_parent out of range %d",
		    (int)mfccp->mfcc_parent));
		return (EINVAL);
	}

	if ((mfccp->mfcc_parent != NO_VIF) &&
	    (viftable[mfccp->mfcc_parent].v_ipif == NULL)) {
		ip0dbg(("ADD_MFC: NULL ipif for parent vif %d\n",
		    (int)mfccp->mfcc_parent));
		return (EINVAL);
	}

	MFCFIND(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr, rt);

	/* If an entry already exists, just update the fields */
	if (rt) {
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "add_mfc: update o %x grp %x parent %x",
			    ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);
		}
		rt->mfc_parent = mfccp->mfcc_parent;

		for (i = 0; i < (int)numvifs; i++)
			rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
		return (0);
	}

	/*
	 * Find the entry for which the upcall was made and update.
	 */
	hash = MFCHASH(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);
	for (rt = mfctable[hash], nstl = 0; rt; rt = rt->mfc_next) {
		if ((rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr) &&
		    (rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr) &&
		    (rt->mfc_rte != NULL)) {
			if (nstl++ != 0)
				cmn_err(CE_WARN,
				    "add_mfc: %s hash %d, o %x g %x p %x",
				    "multiple kernel entries",
				    hash, ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent);

			if (ip_mrtdebug > 1) {
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "add_mfc: hash %d o %x g %x p %x",
				    hash, ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent);
			}
			fill_route(rt, mfccp);

			/*
			 * Prevent cleanup of cache entry.
			 * Timer starts in ip_mforward.
			 */
			if (rt->mfc_timeout_id != 0) {
				(void) quntimeout(ip_g_mrouter,
				    rt->mfc_timeout_id);
				rt->mfc_timeout_id = 0;
			}

			/*
			 * Send all pkts that are queued waiting for the upcall.
			 * ip_mdq param tun set to 0 -
			 * the return value of ip_mdq() isn't used here,
			 * so value we send doesn't matter.
			 */
			while ((rte = rt->mfc_rte) != NULL) {
				(void) ip_mdq(rte->mp, (ipha_t *)
				    rte->mp->b_rptr, rte->ill, 0, rt);
				rt->mfc_rte = rte->rte_next;
				freemsg(rte->mp);
#ifdef UPCALL_TIMING
				collate(&(rte->t));
#endif /* UPCALL_TIMING */
				mi_free((char *)rte);
			}
		}
	}

	/*
	 * It is possible that an entry is being inserted without an upcall
	 */
	if (nstl == 0) {
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "add_mfc: no upcall h %d o %x g %x p %x",
			    hash, ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);
		}
		for (rt = mfctable[hash]; rt; rt = rt->mfc_next) {

			if ((rt->mfc_origin.s_addr ==
			    mfccp->mfcc_origin.s_addr) &&
			    (rt->mfc_mcastgrp.s_addr ==
				mfccp->mfcc_mcastgrp.s_addr)) {
				fill_route(rt, mfccp);
				break;
			}
		}

		/* No upcall, so make a new entry into mfctable */
		if (rt == NULL) {
			rt = (struct mfc *)mi_zalloc(sizeof (struct mfc));
			if (rt == NULL) {
				ip1dbg(("add_mfc: out of memory\n"));
				return (ENOBUFS);
			}

			/* Insert new entry at head of hash chain */
			fill_route(rt, mfccp);

			/* Link into table */
			rt->mfc_next   = mfctable[hash];
			mfctable[hash] = rt;
		}
	}
	return (0);
}

/*
 * Fills in mfc structure from mrouted mfcctl.
 */
static void
fill_route(struct mfc *rt, struct mfcctl *mfccp)
{
	int i;

	rt->mfc_origin		= mfccp->mfcc_origin;
	rt->mfc_mcastgrp	= mfccp->mfcc_mcastgrp;
	rt->mfc_parent		= mfccp->mfcc_parent;
	for (i = 0; i < (int)numvifs; i++) {
		rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
	}
	/* Initialize pkt counters per src-grp */
	rt->mfc_pkt_cnt	= 0;
	rt->mfc_byte_cnt	= 0;
	rt->mfc_wrong_if	= 0;
	rt->mfc_last_assert.tv_sec = rt->mfc_last_assert.tv_nsec = 0;

}

#ifdef UPCALL_TIMING
/*
 * Collects delay statistics on the upcalls.
 * Index is the time-delay for the upcall and entry is the number
 * of times that delay has occured. Delay is truncated at 50 max.
 * BSD measures in usec, SUNOS 5.x measures in nsec.
 * A histogram appears in netstat -Ms if UPCALL_TIMING is defined.
 */
static void
collate(timespec_t *t)
{
	hrtime_t	d;
	timespec_t 	tp;
	hrtime_t 	delta;

	gethrestime(&tp);

	if (TV_LT(*t, tp)) {
			TV_DELTA(tp, *t, delta);

			d = delta >> 20;
			if (d > 50)
				d = 50;

			++mrtstat.upcall_data[d];
	}
}
#endif /* UPCALL_TIMING */

/*
 * Delete an mfc entry.
 */
static int
del_mfc(struct mfcctl *mfccp)
{
	struct in_addr	origin;
	struct in_addr	mcastgrp;
	struct mfc 		*rt;
	struct mfc 		*prev_mfc_rt;
	uint_t			hash;

	origin = mfccp->mfcc_origin;
	mcastgrp = mfccp->mfcc_mcastgrp;
	hash = MFCHASH(origin.s_addr, mcastgrp.s_addr);

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "del_mfc: o %x g %x",
		    ntohl(origin.s_addr),
		    ntohl(mcastgrp.s_addr));
	}

	/* Find mfc in mfctable, finds only entries without upcalls */
	for (prev_mfc_rt = rt = mfctable[hash]; rt;
	    prev_mfc_rt = rt, rt = rt->mfc_next) {
		if (origin.s_addr == rt->mfc_origin.s_addr &&
		    mcastgrp.s_addr == rt->mfc_mcastgrp.s_addr &&
		    rt->mfc_rte == NULL)
			break;
	}

	/*
	 * Return if there was an upcall (mfc_rte != NULL,
	 * or rt not in mfctable.
	 */
	if (rt == NULL)
		return (EADDRNOTAVAIL);

	/* error checking */
	if (rt->mfc_timeout_id != 0) {
		ip0dbg(("del_mfc: TIMEOUT NOT 0, rte not null"));
		(void) quntimeout(ip_g_mrouter, rt->mfc_timeout_id);
		rt->mfc_timeout_id = 0;
	}

	ASSERT(rt->mfc_rte == NULL);

	/* Unlink from mfctable */
	if (prev_mfc_rt != rt) {	/* moved past head of list */
		prev_mfc_rt->mfc_next = rt->mfc_next;
	} else			/* delete head of list, it is in the table */
		mfctable[hash] = rt->mfc_next;

	/* Free mfc */
	mi_free((char *)rt);

	return (0);
}

#define	TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by ipha has arrived on (or is about to be sent to) the interface
 * pointed to by "ill", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a -1 value tells the caller (IP)
 * to discard it.
 *
 * Unlike BSD, SunOS 5.x needs to return to IP info about
 * whether pkt came in thru a tunnel, so it can be discarded, unless
 * it's IGMP. In BSD, the ifp is bogus for tunnels, so pkt won't try
 * to be delivered.
 * Return values are 0 - pkt is okay and phyint
 *		    -1 - pkt is malformed and to be tossed
 *                   1 - pkt came in on tunnel
 */
int
ip_mforward(ill_t *ill, ipha_t *ipha, mblk_t *mp)
{
	struct mfc 	*rt;
	ipaddr_t	src, dst, tunnel_src = 0;
	static int	srctun = 0;
	vifi_t		vifi;
	boolean_t	pim_reg_packet = B_FALSE;

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "ip_mforward: RECV ipha_src %x, ipha_dst %x, ill %s",
		    ntohl(ipha->ipha_src), ntohl(ipha->ipha_dst),
		    ill->ill_name);
	}

	dst = ipha->ipha_dst;
	if ((uint32_t)mp->b_prev == PIM_REGISTER_MARKER)
		pim_reg_packet = B_TRUE;
	else
		tunnel_src = (ipaddr_t)mp->b_prev;

	/*
	 * Don't forward a packet with time-to-live of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (CLASSD(dst) && (ipha->ipha_ttl <= 1 ||
			(ipaddr_t)ntohl(dst) <= INADDR_MAX_LOCAL_GROUP)) {
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "ip_mforward: not forwarded ttl %d,"
			    " dst 0x%x ill %s",
			    ipha->ipha_ttl, ntohl(dst), ill->ill_name);
		}
		mp->b_prev = NULL;
		if (tunnel_src != 0)
			return (1);
		else
			return (0);
	}

	if ((tunnel_src != 0) || pim_reg_packet) {
		/*
		 * Packet arrived over an encapsulated tunnel or via a PIM
		 * register message. Both ip_mroute_decap() and pim_input()
		 * encode information in mp->b_prev.
		 */
		mp->b_prev = NULL;
		if (ip_mrtdebug > 1) {
			if (tunnel_src != 0) {
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "ip_mforward: ill %s arrived via ENCAP TUN",
				    ill->ill_name);
			} else if (pim_reg_packet) {
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "ip_mforward: ill %s arrived via"
				    "  REGISTER VIF",
				    ill->ill_name);
			}
		}
	} else if ((ipha->ipha_version_and_hdr_length & 0xf) <
	    (uint_t)(IP_SIMPLE_HDR_LENGTH + TUNNEL_LEN) >> 2 ||
	    ((uchar_t *)(ipha + 1))[1] != IPOPT_LSRR) {
		/* Packet arrived via a physical interface. */
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "ip_mforward: ill %s arrived via PHYINT",
			    ill->ill_name);
		}

	} else {
		/*
		 * Packet arrived through a SRCRT tunnel.
		 * Source-route tunnels are no longer supported.
		 * Error message printed every 1000 times.
		 */
		if ((srctun++ % 1000) == 0) {
			cmn_err(CE_WARN,
			    "ip_mforward: received source-routed pkt from %x",
			    ntohl(ipha->ipha_src));
		}
		return (-1);
	}

	mrtstat.mrts_fwd_in++;
	src = ipha->ipha_src;

	/* Find route in cache, return NULL if not there or upcalls q'ed. */

	/*
	 * Lock the mfctable against changes made by ip_mforward.
	 * Note that only add_mfc and del_mfc can remove entries and
	 * they run with exclusive access to IP. So we do not need to
	 * guard against the rt being deleted, so release lock after reading.
	 */
	mutex_enter(&kcache_lock);
	MFCFIND(src, dst, rt);
	mutex_exit(&kcache_lock);

	/* Entry exists, so forward if necessary */
	if (rt != NULL) {
		mrtstat.mrts_mfc_hits++;
		if (pim_reg_packet) {
			ASSERT(reg_vif_num != ALL_VIFS);
			return (ip_mdq(mp, ipha,
			    viftable[reg_vif_num].v_ipif->ipif_ill, 0, rt));
		} else {
			return (ip_mdq(mp, ipha, ill, tunnel_src, rt));
		}

		/*
		 * Don't forward if we don't have a cache entry.  Mrouted will
		 * always provide a cache entry in response to an upcall.
		 */
	} else {
		/*
		 * If we don't have a route for packet's origin, make a copy
		 * of the packet and send message to routing daemon.
		 */
		struct mfc	*mfc_rt	 = NULL;
		mblk_t		*mp0	 = NULL;
		mblk_t		*mp_copy = NULL;
		struct rtdetq	*rte	 = NULL;
		struct rtdetq	*rte_m, *rte1, *prev_rte;
		uint_t		hash;
		int		npkts;
		boolean_t	new_mfc = B_FALSE;
#ifdef UPCALL_TIMING
		timespec_t tp;

		gethrestime(&tp);
#endif /* UPCALL_TIMING */
		mrtstat.mrts_mfc_misses++;
		/* BSD uses mrts_no_route++ */
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "ip_mforward: no rte ill %s src %x g %x misses %d",
			    ill->ill_name, ntohl(src), ntohl(dst),
			    (int)mrtstat.mrts_mfc_misses);
		}
		/*
		 * The order of the following code differs from the BSD code.
		 * Pre-mc3.5, the BSD code was incorrect and SunOS 5.x
		 * code works, so SunOS 5.x wasn't changed to conform to the
		 * BSD version.
		 */

		/* Lock mfctable */
		mutex_enter(&kcache_lock);

		/* Is there an upcall waiting for this packet? */
		hash = MFCHASH(src, dst);
		for (mfc_rt = mfctable[hash]; mfc_rt;
		    mfc_rt = mfc_rt->mfc_next) {
			if (ip_mrtdebug > 1) {
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "ip_mforward: MFCTAB hash %d o 0x%x"
				    " g 0x%x\n",
				    hash, ntohl(mfc_rt->mfc_origin.s_addr),
				    ntohl(mfc_rt->mfc_mcastgrp.s_addr));
			}
			/* There is an upcall */
			if ((src == mfc_rt->mfc_origin.s_addr) &&
			    (dst == mfc_rt->mfc_mcastgrp.s_addr) &&
			    (mfc_rt->mfc_rte != NULL))
				break;
		}
		/* No upcall, so make a new entry into mfctable */
		if (mfc_rt == NULL) {
			mfc_rt = (struct mfc *)mi_zalloc(sizeof (struct mfc));
			if (mfc_rt == NULL) {
				mrtstat.mrts_fwd_drop++;
				ip1dbg(("ip_mforward: out of memory "
				    "for mfc, mfc_rt\n"));
				goto error_return;
			} else
				new_mfc = B_TRUE;
			/* Get resources */
			/* TODO could copy header and dup rest */
			mp_copy = copymsg(mp);
			if (mp_copy == NULL) {
				mrtstat.mrts_fwd_drop++;
				ip1dbg(("ip_mforward: out of memory for "
				    "mblk, mp_copy\n"));
				goto error_return;
			}
		}
		/* Get resources for rte, whether first rte or not first. */
		/* Add this packet into rtdetq */
		rte = (struct rtdetq *)mi_zalloc(sizeof (struct rtdetq));
		if (rte == NULL) {
			mrtstat.mrts_fwd_drop++;
			ip1dbg(("ip_mforward: out of memory for"
			    " rtdetq, rte\n"));
			goto error_return;
		}
		rte->rte_next = NULL;

		mp0 = copymsg(mp);
		if (mp0 == NULL) {
			mrtstat.mrts_fwd_drop++;
			ip1dbg(("ip_mforward: out of memory for mblk, mp0\n"));
			goto error_return;
		}
		rte->mp		= mp0;
		if (pim_reg_packet) {
			ASSERT(reg_vif_num != ALL_VIFS);
			rte->ill = viftable[reg_vif_num].v_ipif->ipif_ill;
		} else {
			rte->ill = ill;
		}
#ifdef UPCALL_TIMING
		rte->t		= tp;
#endif UPCALL_TIMING
		rte->rte_next	= NULL;

		/*
		 * Determine if upcall q (rtdetq) has overflowed.
		 * mfc_rt->mfc_rte is null by mi_zalloc
		 * if it is the first message.
		 */
		for (rte_m = mfc_rt->mfc_rte, npkts = 0; rte_m;
		    rte_m = rte_m->rte_next)
			npkts++;
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "ip_mforward: upcalls %d\n", npkts);
		}
		if (npkts > MAX_UPQ) {
			mrtstat.mrts_upq_ovflw++;
			goto error_return;
		}

		if (npkts == 0) {	/* first upcall */
			int i = 0;
			/*
			 * Now finish installing the new mfc! Now that we have
			 * resources!  Insert new entry at head of hash chain.
			 * Use src and dst which are ipaddr_t's.
			 */
			mfc_rt->mfc_origin.s_addr = src;
			mfc_rt->mfc_mcastgrp.s_addr = dst;

			for (i = 0; i < (int)numvifs; i++)
				mfc_rt->mfc_ttls[i] = 0;
			mfc_rt->mfc_parent = ALL_VIFS;

			/* Link into table */
			if (ip_mrtdebug > 1) {
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "ip_mforward: NEW MFCTAB hash %d o 0x%x "
				    "g 0x%x\n", hash,
				    ntohl(mfc_rt->mfc_origin.s_addr),
				    ntohl(mfc_rt->mfc_mcastgrp.s_addr));
			}
			mfc_rt->mfc_next = mfctable[hash];
			mfctable[hash] = mfc_rt;
			mfc_rt->mfc_rte = NULL;
		}

		/* Link in the upcall */
		/* First upcall */
		if (mfc_rt->mfc_rte == NULL)
			mfc_rt->mfc_rte = rte;
		else {
			/* not the first upcall */
			prev_rte = mfc_rt->mfc_rte;
			for (rte1 = mfc_rt->mfc_rte->rte_next; rte1;
			    prev_rte = rte1, rte1 = rte1->rte_next);
			prev_rte->rte_next = rte;
		}

		/*
		 * No upcalls waiting, this is first one, so send a message to
		 * routing daemon to install a route into kernel table.
		 */
		if (npkts == 0) {
			struct igmpmsg	*im;
			/* ipha_protocol is 0, for upcall */
			im = (struct igmpmsg *)mp_copy->b_rptr;
			im->im_msgtype	= IGMPMSG_NOCACHE;
			im->im_mbz = 0;
			if (pim_reg_packet) {
				im->im_vif = reg_vif_num;
			} else {
				for (vifi = 0; vifi < numvifs; vifi++) {
					if (viftable[vifi].v_ipif == NULL)
						continue;
					if (viftable[vifi].v_ipif->ipif_ill ==
					    ill) {
						im->im_vif = vifi;
						break;
					}
				}
				ASSERT(vifi < numvifs);
			}

			mrtstat.mrts_upcalls++;
			/* Timer to discard upcalls if mrouted is too slow */
			mfc_rt->mfc_timeout_id =
			    qtimeout(ip_g_mrouter, expire_upcalls,
				mfc_rt, EXPIRE_TIMEOUT * UPCALL_EXPIRE);
			mutex_exit(&kcache_lock);
			putnext(RD(ip_g_mrouter), mp_copy);

		} else {
			mutex_exit(&kcache_lock);
			freemsg(mp_copy);
		}

		if (tunnel_src != 0)
			return (1);
		else
			return (0);
	error_return:
		if (mfc_rt != NULL && (new_mfc == B_TRUE))
			mi_free((char *)mfc_rt);
		if (rte != NULL)
			mi_free((char *)rte);
		if (mp_copy != NULL)
			freemsg(mp_copy);
		if (mp0 != NULL)
			freemsg(mp0);
		mutex_exit(&kcache_lock);
		return (-1);
	}
}

/*
 * Clean up the mfctable cache entry if upcall is not serviced.
 * SunOS 5.x has timeout per mfc, unlike BSD which has one timer.
 */
static void
expire_upcalls(void *arg)
{
	struct mfc *mfc_rt = arg;
	uint_t hash;
	struct mfc *prev_mfc, *mfc0;
	struct rtdetq *rte0;

	hash = MFCHASH(mfc_rt->mfc_origin.s_addr, mfc_rt->mfc_mcastgrp.s_addr);
	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "expire_upcalls: hash %d s %x g %x",
		    hash, ntohl(mfc_rt->mfc_origin.s_addr),
		    ntohl(mfc_rt->mfc_mcastgrp.s_addr));
	}
	mrtstat.mrts_cache_cleanups++;
	mfc_rt->mfc_timeout_id = 0;

	/* Determine entry to be cleaned up in cache table. */
	mutex_enter(&kcache_lock);
	for (prev_mfc = mfc0 = mfctable[hash]; mfc0;
	    prev_mfc = mfc0, mfc0 = mfc0->mfc_next)
		if (mfc0 == mfc_rt)
			break;

	/* del_mfc takes care of gone mfcs */
	ASSERT(prev_mfc != NULL);
	ASSERT(mfc0 != NULL);

	/*
	 * Drop all queued upcall packets.
	 * Free the mbuf with the pkt, if, timing info.
	 */
	while ((rte0 = mfc_rt->mfc_rte) != NULL) {
		mfc_rt->mfc_rte = rte0->rte_next;
		freemsg(rte0->mp);
		mi_free((char *)rte0);
	}

	/*
	 * Delete the entry from the cache
	 */
	if (prev_mfc != mfc0) {		/* if moved past head of list */
		prev_mfc->mfc_next = mfc0->mfc_next;
	} else			/* delete head of list, it's in table */
		mfctable[hash] = mfc0->mfc_next;

	mi_free((char *)mfc0);
	mutex_exit(&kcache_lock);
}

/*
 * Packet forwarding routine once entry in the cache is made.
 */
static int
ip_mdq(mblk_t *mp, ipha_t *ipha, ill_t *ill, ipaddr_t tunnel_src,
    struct mfc *rt)
{
	vifi_t vifi;
	struct vif *vifp;
	ipaddr_t dst = ipha->ipha_dst;
	size_t  plen = msgdsize(mp);

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "ip_mdq: SEND src %x, ipha_dst %x, ill %s",
		    ntohl(ipha->ipha_src), ntohl(ipha->ipha_dst),
		    ill->ill_name);
	}

	/* Macro to send packet on vif */
#define	MC_SEND(ipha, mp, vifp, dst) { \
	if ((vifp)->v_flags & VIFF_TUNNEL) \
		encap_send((ipha), (mp), (vifp), (dst)); \
	else if ((vifp)->v_flags & VIFF_REGISTER) \
		register_send((ipha), (mp), (vifp), (dst)); \
	else \
		phyint_send((ipha), (mp), (vifp), (dst)); \
}

	vifi = rt->mfc_parent;

	/*
	 * The value of vifi is MAXVIFS if the pkt had no parent, i.e.,
	 * Mrouted had no route.
	 * We wanted the route installed in the mfctable to prevent multiple
	 * tries, so it passed add_mfc(), but is discarded here. The v_ipif is
	 * NULL so we don't want to check the ill. Still needed as of Mrouted
	 * 3.6.
	 */
	if (vifi == NO_VIF) {
		ip1dbg(("ip_mdq: no route for origin ill %s, vifi is NO_VIF\n",
		    ill->ill_name));
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "ip_mdq: vifi is NO_VIF ill = %s", ill->ill_name);
		}
		return (-1);	/* drop pkt */
	}
	/*
	 * The MFC entries are not cleaned up when an ipif goes
	 * away thus this code has to guard against an MFC referencing
	 * an ipif that has been closed. Note: reset_mrt_vif_ipif
	 * sets the v_ipif to NULL when the ipif disappears.
	 */
	if (viftable[vifi].v_ipif == NULL) {
		cmn_err(CE_WARN, "ip_mdq: closed parent vif. "
			"Parent %d, ill %s", (int)vifi, ill->ill_name);
		return (-1);	/* drop pkt */
	}

	if (vifi >= numvifs) {
		cmn_err(CE_WARN, "ip_mdq: illegal vifi %d numvifs "
		    "%d ill %s viftable ill %s\n",
		    (int)vifi, (int)numvifs, ill->ill_name,
		    viftable[vifi].v_ipif->ipif_ill->ill_name);
		return (-1);
	}
	/*
	 * Don't forward if it didn't arrive from the parent vif for its
	 * origin.
	 */
	if ((viftable[vifi].v_ipif->ipif_ill != ill) ||
			(viftable[vifi].v_rmt_addr.s_addr != tunnel_src)) {
		/* Came in the wrong interface */
		ip1dbg(("ip_mdq: arrived wrong if, vifi %d "
			"numvifs %d ill %s viftable ill %s\n",
			(int)vifi, (int)numvifs, ill->ill_name,
			viftable[vifi].v_ipif->ipif_ill->ill_name));
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "ip_mdq: arrived wrong if, vifi %d ill "
			    "%s viftable ill %s\n",
			    (int)vifi, ill->ill_name,
			    viftable[vifi].v_ipif->ipif_ill->ill_name);
		}
		mrtstat.mrts_wrong_if++;
		rt->mfc_wrong_if++;

		/*
		 * If we are doing PIM assert processing and we are forwarding
		 * packets on this interface, and it is a broadcast medium
		 * interface (and not a tunnel), send a message to the routing.
		 *
		 * We use the first ipif on the list, since it's all we have.
		 * Chances are the ipif_flags are the same for ipifs on the ill.
		 */
		if (pim_assert && rt->mfc_ttls[vifi] > 0 &&
		    (ill->ill_ipif->ipif_flags & IFF_BROADCAST) &&
		    !(viftable[vifi].v_flags & VIFF_TUNNEL)) {
			mblk_t		*mp_copy;
			struct igmpmsg	*im;

			/* TODO could copy header and dup rest */
			mp_copy = copymsg(mp);
			if (mp_copy == NULL) {
				mrtstat.mrts_fwd_drop++;
				ip1dbg(("ip_mdq: out of memory "
				    "for mblk, mp_copy\n"));
				return (-1);
			}

			im = (struct igmpmsg *)mp_copy->b_rptr;
			im->im_msgtype = IGMPMSG_WRONGVIF;
			im->im_mbz = 0;
			im->im_vif = (ushort_t)vifi;
			putnext(RD(ip_g_mrouter), mp_copy);
		}
		if (tunnel_src != 0)
			return (1);
		else
			return (0);
	}
	/*
	 * If I sourced this packet, it counts as output, else it was input.
	 * No locks, therefore approximate statistics.
	 */
	if (ipha->ipha_src == viftable[vifi].v_lcl_addr.s_addr) {
		viftable[vifi].v_pkt_out++;
		viftable[vifi].v_bytes_out += plen;
	} else {
		viftable[vifi].v_pkt_in++;
		viftable[vifi].v_bytes_in += plen;
	}
	rt->mfc_pkt_cnt++;
	rt->mfc_byte_cnt += plen;
	/*
	 * For each vif, decide if a copy of the packet should be forwarded.
	 * Forward if:
	 *		- the vif threshold ttl is non-zero AND
	 *		- the pkt ttl exceeds the vif's threshold
	 * A non-zero mfc_ttl indicates that the vif is part of
	 * the output set for the mfc entry.
	 */
	for (vifp = viftable, vifi = 0; vifi < numvifs; vifp++, vifi++) {
		if ((rt->mfc_ttls[vifi] > 0) &&
		    (ipha->ipha_ttl > rt->mfc_ttls[vifi])) {
			/*
			 * The MFC entries are not cleaned up when an ipif goes
			 * away thus this code has to guard against an MFC
			 * referencing an ipif that has been closed.
			 * Note: reset_mrt_vif_ipif
			 * sets the v_ipif to NULL when the ipif disappears.
			 */
			if (vifp->v_ipif == NULL) {
				cmn_err(CE_WARN, "ip_mdq: closed vif. "
					"Vif %d, ill %s",
					(int)vifi, ill->ill_name);
				continue;
			}
			vifp->v_pkt_out++;
			vifp->v_bytes_out += plen;
			MC_SEND(ipha, mp, vifp, dst);
			mrtstat.mrts_fwd_out++;
		}
	}
	if (tunnel_src != 0)
		return (1);
	else
		return (0);
}

/*
 * Send the packet on physical interface.
 * Caller assumes can continue to use mp on return.
 */
/* ARGSUSED */
static void
phyint_send(ipha_t *ipha, mblk_t *mp, struct vif *vifp, ipaddr_t dst)
{
	mblk_t 	*mp_copy;

	/* Make a new reference to the packet */
	mp_copy = copymsg(mp);	/* TODO could copy header and dup rest */
	if (mp_copy == NULL) {
		mrtstat.mrts_fwd_drop++;
		ip1dbg(("phyint_send: out of memory for mblk, mp_copy\n"));
		return;
	}
	if (vifp->v_rate_limit <= 0)
		tbf_send_packet(vifp, mp_copy);
	else  {
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "phyint_send: tbf_contr rate %d "
			    "vifp 0x%x mp 0x%x dst 0x%x",
			    vifp->v_rate_limit, vifp, mp, dst);
		}
		tbf_control(vifp, mp_copy, (ipha_t *)mp_copy->b_rptr);
	}
}

/*
 * Send the whole packet for REGISTER encapsulation to PIM daemon
 * Caller assumes it can continue to use mp on return.
 */
/* ARGSUSED */
static void
register_send(ipha_t *ipha, mblk_t *mp, struct vif *vifp, ipaddr_t dst)
{
	struct igmpmsg	*im;
	mblk_t		*mp_copy;
	ipha_t		*ipha_copy;

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "register_send: src %x, dst %x\n",
		    ntohl(ipha->ipha_src), ntohl(ipha->ipha_dst));
	}

	/*
	 * Copy the old packet & pullup its IP header into the new mblk_t so we
	 * can modify it.  Try to fill the new mblk_t since if we don't the
	 * ethernet driver will.
	 */
	mp_copy = allocb(sizeof (struct igmpmsg) + sizeof (ipha_t), BPRI_MED);
	bzero(mp_copy, sizeof (struct igmpmsg) + sizeof (ipha_t));
	if (mp_copy == NULL) {
		++mrtstat.mrts_pim_nomemory;
		if (ip_mrtdebug > 3) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "register_send: allocb failure.");
		}
		return;
	}

	/*
	 * Bump write pointer to account for igmpmsg being added.
	 */
	mp_copy->b_wptr = mp_copy->b_rptr + sizeof (struct igmpmsg);

	/*
	 * Chain packet to new mblk_t.
	 */
	if ((mp_copy->b_cont = copymsg(mp)) == NULL) {
		++mrtstat.mrts_pim_nomemory;
		if (ip_mrtdebug > 3) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "register_send: copymsg failure.");
		}
		freeb(mp_copy);
		return;
	}

	/*
	 * icmp_rput() asserts that IP version field is set to an
	 * appropriate version. Hence, the struct igmpmsg that this really
	 * becomes, needs to have the correct IP version field.
	 */
	ipha_copy = (ipha_t *)mp_copy->b_rptr;
	*ipha_copy = multicast_encap_iphdr;

	/*
	 * The kernel uses the struct igmpmsg header to encode the messages to
	 * the multicast routing daemon. Fill in the fields in the header
	 * starting with the message type which is IGMPMSG_WHOLEPKT
	 */
	im = (struct igmpmsg *)mp_copy->b_rptr;
	im->im_msgtype = IGMPMSG_WHOLEPKT;
	im->im_src.s_addr = ipha->ipha_src;
	im->im_dst.s_addr = ipha->ipha_dst;

	/*
	 * Must Be Zero. This is because the struct igmpmsg is really an IP
	 * header with renamed fields and the multicast routing daemon uses
	 * an ipha_protocol (aka im_mbz) of 0 to distinguish these messages.
	 */
	im->im_mbz = 0;

	++mrtstat.mrts_upcalls;
	if (!canputnext(RD(ip_g_mrouter))) {
		++mrtstat.mrts_pim_regsend_drops;
		if (ip_mrtdebug > 3) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "register_send: register upcall failure.");
		}
		freemsg(mp_copy);
	} else {
		putnext(RD(ip_g_mrouter), mp_copy);
	}
}

/*
 * pim_validate_cksum handles verification of the checksum in the
 * pim header.  For PIM Register packets, the checksum is calculated
 * across the PIM header only.  For all other packets, the checksum
 * is for the PIM header and remainder of the packet.
 *
 * returns: B_TRUE, if checksum is okay.
 *          B_FALSE, if checksum is not valid.
 */
static boolean_t
pim_validate_cksum(mblk_t *mp, ipha_t *ip, struct pim *pimp)
{
	mblk_t *mp_dup;

	if ((mp_dup = dupmsg(mp)) == NULL)
		return (B_FALSE);

	mp_dup->b_rptr += IPH_HDR_LENGTH(ip);
	if (pimp->pim_type == PIM_REGISTER)
		mp_dup->b_wptr = mp_dup->b_rptr + PIM_MINLEN;
	if (IP_CSUM(mp_dup, 0, 0)) {
		freemsg(mp_dup);
		return (B_FALSE);
	}
	freemsg(mp_dup);
	return (B_TRUE);
}

/*
 * int
 * pim_input(queue_t *, mblk_t *) - Process PIM protocol packets.
 *	IP Protocol 103. Register messages are decapsulated and sent
 *	onto multicast forwarding.
 */
int
pim_input(queue_t *q, mblk_t *mp)
{
	ipha_t		*eip, *ip;
	int		iplen, pimlen, iphlen;
	struct pim	*pimp;	/* pointer to a pim struct */
	uint32_t	*reghdr;

	/*
	 * Pullup the msg for PIM protocol processing.
	 */
	if (pullupmsg(mp, -1) == 0) {
		++mrtstat.mrts_pim_nomemory;
		freemsg(mp);
		return (-1);
	}

	ip = (ipha_t *)mp->b_rptr;
	iplen = ip->ipha_length;
	iphlen = IPH_HDR_LENGTH(ip);
	pimlen = ntohs(iplen) - iphlen;

	/*
	 * Validate lengths
	 */
	if (pimlen < PIM_MINLEN) {
		++mrtstat.mrts_pim_malformed;
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "pim_input: length not at least minlen");
		}
		freemsg(mp);
		return (-1);
	}

	/*
	 * Point to the PIM header.
	 */
	pimp = (struct pim *)((caddr_t)ip + iphlen);

	/*
	 * Check the version number.
	 */
	if (pimp->pim_vers != PIM_VERSION) {
		++mrtstat.mrts_pim_badversion;
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "pim_input: unknown version of PIM");
		}
		freemsg(mp);
		return (-1);
	}

	/*
	 * Validate the checksum
	 */
	if (!pim_validate_cksum(mp, ip, pimp)) {
		++mrtstat.mrts_pim_rcv_badcsum;
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "pim_input: invalid checksum");
		}
		freemsg(mp);
		return (-1);
	}

	if (pimp->pim_type != PIM_REGISTER)
		return (0);

	reghdr = (uint32_t *)(pimp + 1);
	eip = (ipha_t *)(reghdr + 1);

	/*
	 * check if the inner packet is destined to mcast group
	 */
	if (!CLASSD(eip->ipha_dst)) {
		++mrtstat.mrts_pim_badregisters;
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "pim_input: Inner pkt not mcast .. !");
		}
		freemsg(mp);
		return (-1);
	}
	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "register from %x, to %x, len %d",
		    ntohl(eip->ipha_src),
		    ntohl(eip->ipha_dst),
		    ntohs(eip->ipha_length));
	}
	/*
	 * If the null register bit is not set, decapsulate
	 * the packet before forwarding it.
	 */
	if (!(ntohl(*reghdr) & PIM_NULL_REGISTER)) {
		mblk_t *mp_copy;

		/* Copy the message */
		if ((mp_copy = copymsg(mp)) == NULL) {
			++mrtstat.mrts_pim_nomemory;
			freemsg(mp);
			return (-1);
		}

		/*
		 * Decapsulate the packet and give it to
		 * register_mforward.
		 */
		mp_copy->b_rptr += iphlen + sizeof (pim_t) +
		    sizeof (*reghdr);
		if (register_mforward(q, mp_copy) != 0) {
			freemsg(mp);
			return (-1);
		}
	}

	/*
	 * Pass all valid PIM packets up to any process(es) listening on a raw
	 * PIM socket. For Solaris it is done right after pim_input() is
	 * called.
	 */
	return (0);
}

/*
 * PIM sparse mode hook.  Called by pim_input after decapsulating
 * the packet. Loop back the packet, as if we have received it.
 * In pim_input() we have to check if the destination is a multicast address.
 */
/* ARGSUSED */
static int
register_mforward(queue_t *q, mblk_t *mp)
{
	ASSERT(reg_vif_num <= numvifs);

	if (ip_mrtdebug > 3) {
		ipha_t *ipha;

		ipha = (ipha_t *)mp->b_rptr;
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "register_mforward: src %x, dst %x\n",
		    ntohl(ipha->ipha_src), ntohl(ipha->ipha_dst));
	}
	/*
	 * Need to pass in to ip_mforward() the information that the
	 * packet has arrived on the register_vif. We use the solution that
	 * ip_mroute_decap() employs: use mp->b_prev to pass some information
	 * to ip_mforward(). Nonzero value means the packet has arrived on a
	 * tunnel (ip_mroute_decap() puts the address of the other side of the
	 * tunnel there.) This is safe since ip_rput() either frees the packet
	 * or passes it to ip_mforward(). We use
	 * PIM_REGISTER_MARKER = 0xffffffff to indicate the has arrived on the
	 * register vif. If in the future we have more than one register vifs,
	 * then this will need re-examination.
	 */
	mp->b_prev = (mblk_t *)PIM_REGISTER_MARKER;
	++mrtstat.mrts_pim_regforwards;
	ip_rput(q, mp);
	return (0);
}

/*
 * Send an encapsulated packet.
 * Caller assumes can continue to use mp when routine returns.
 */
/* ARGSUSED */
static void
encap_send(ipha_t *ipha, mblk_t *mp, struct vif *vifp, ipaddr_t dst)
{
	mblk_t 	*mp_copy;
	ipha_t 	*ipha_copy;
	size_t	len;
	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "encap_send: vif %ld enter", (ptrdiff_t)(vifp - viftable));
	}
	len = ntohs(ipha->ipha_length);

	/*
	 * Copy the old packet & pullup it's IP header into the
	 * new mbuf so we can modify it.  Try to fill the new
	 * mbuf since if we don't the ethernet driver will.
	 */
	mp_copy = allocb(32 + sizeof (multicast_encap_iphdr), BPRI_MED);
	if (mp_copy == NULL)
		return;
	mp_copy->b_rptr += 32;
	mp_copy->b_wptr = mp_copy->b_rptr + sizeof (multicast_encap_iphdr);
	if ((mp_copy->b_cont = copymsg(mp)) == NULL) {
		freeb(mp_copy);
		return;
	}

	/*
	 * Fill in the encapsulating IP header.
	 * Remote tunnel dst in rmt_addr, from add_vif().
	 */
	ipha_copy = (ipha_t *)mp_copy->b_rptr;
	*ipha_copy = multicast_encap_iphdr;
	ASSERT((len + sizeof (ipha_t)) <= IP_MAXPACKET);
	ipha_copy->ipha_length = htons(len + sizeof (ipha_t));
	ipha_copy->ipha_src = vifp->v_lcl_addr.s_addr;
	ipha_copy->ipha_dst = vifp->v_rmt_addr.s_addr;
	ASSERT(ipha_copy->ipha_ident == 0);

	/* Turn the encapsulated IP header back into a valid one. */
	ipha = (ipha_t *)mp_copy->b_cont->b_rptr;
	ipha->ipha_ttl--;
	ipha->ipha_hdr_checksum = 0;
	ipha->ipha_hdr_checksum = ip_csum_hdr(ipha);

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "encap_send: group 0x%x", ntohl(ipha->ipha_dst));
	}
	if (vifp->v_rate_limit <= 0)
		tbf_send_packet(vifp, mp_copy);
	else
		/* ipha is from the original header */
		tbf_control(vifp, mp_copy, ipha);
}

/*
 * De-encapsulate a packet and feed it back through IP input.
 * This routine is called whenever IP gets a packet with prototype
 * IPPROTO_ENCAP and a local destination address.
 */
void
ip_mroute_decap(queue_t *q, mblk_t *mp)
{
	ipha_t		*ipha = (ipha_t *)mp->b_rptr;
	ipha_t		*ipha_encap;
	int		hlen = IPH_HDR_LENGTH(ipha);
	ipaddr_t	src;
	struct vif	*vifp;

	/*
	 * Dump the packet if it's not to a multicast destination or if
	 * we don't have an encapsulating tunnel with the source.
	 * Note:  This code assumes that the remote site IP address
	 * uniquely identifies the tunnel (i.e., that this site has
	 * at most one tunnel with the remote site).
	 */
	ipha_encap = (ipha_t *)((char *)ipha + hlen);
	if (!CLASSD(ipha_encap->ipha_dst)) {
		mrtstat.mrts_bad_tunnel++;
		ip1dbg(("ip_mroute_decap: bad tunnel\n"));
		freemsg(mp);
		return;
	}
	src = (ipaddr_t)ipha->ipha_src;
	mutex_enter(&last_encap_lock);
	if (src != last_encap_src) {
		struct vif *vife;

		vifp = viftable;
		vife = vifp + numvifs;
		last_encap_src = src;
		last_encap_vif = 0;
		for (; vifp < vife; ++vifp)
			if (vifp->v_rmt_addr.s_addr == src) {
				if (vifp->v_flags & VIFF_TUNNEL)
					last_encap_vif = vifp;
				if (ip_mrtdebug > 1) {
					(void) mi_strlog(ip_g_mrouter,
					    1, SL_TRACE,
					    "ip_mroute_decap: good tun "
					    "vif %ld with %x",
					    (ptrdiff_t)(vifp - viftable),
					    ntohl(src));
				}
				break;
			}
	}
	if ((vifp = last_encap_vif) == 0) {
		mutex_exit(&last_encap_lock);
		mrtstat.mrts_bad_tunnel++;
		freemsg(mp);
		ip1dbg(("ip_mroute_decap: vif %ld no tunnel with %x\n",
		    (ptrdiff_t)(vifp - viftable), ntohl(src)));
		return;
	}
	mutex_exit(&last_encap_lock);

	/*
	 * Need to pass in the tunnel source to ip_mforward (so that it can
	 * verify that the packet arrived over the correct vif.)  We use b_prev
	 * to pass this information. This is safe since the ip_rput either
	 * frees the packet or passes it to ip_mforward.
	 */
	mp->b_prev = (mblk_t *)src;
	mp->b_rptr += hlen;
	/* Feed back into ip_rput as an M_DATA. */
	ip_rput(q, mp);
}

/*
 * Remove all records with v_ipif == ipif.  Called when an interface goes away
 * (stream closed).  Called as writer.
 */
void
reset_mrt_vif_ipif(ipif_t *ipif)
{
	vifi_t vifi, tmp_vifi;

	/* Can't check vifi >= 0 since vifi_t is unsigned! */
	for (vifi = numvifs; vifi != 0; vifi--) {
		tmp_vifi = vifi - 1;
		if (viftable[tmp_vifi].v_ipif == ipif) {
			(void) del_vif(&tmp_vifi);
		}
	}
}

/* Remove pending upcall msgs when ill goes away.  Called by ill_delete.  */
void
reset_mrt_ill(ill_t *ill)
{
	struct mfc		*rt;
	struct rtdetq	*rte;
	int			i;

	for (i = 0; i < MFCTBLSIZ; i++) {
		if ((rt = mfctable[i]) != NULL) {
			if (ip_mrtdebug > 1) {
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "reset_mrt_ill: mfctable [%d]", i);
			}
			while (rt != NULL) {
				while ((rte = rt->mfc_rte) != NULL) {
					if (rte->ill == ill) {
						if (ip_mrtdebug > 1) {
							(void) mi_strlog(
							    ip_g_mrouter,
							    1, SL_TRACE,
							    "reset_mrt_ill: "
							    "ill 0x%p", ill);
						}
						rt->mfc_rte = rte->rte_next;
						freemsg(rte->mp);
						mi_free((char *)rte);
					}
				}
				rt = rt->mfc_next;
			}
		}
	}
}

/*
 * Token bucket filter module.
 * The ipha is for mcastgrp destination for phyint and encap.
 */
static void
tbf_control(struct vif *vifp, mblk_t *mp, ipha_t *ipha)
{
	size_t 	p_len =  msgdsize(mp);
	struct tbf	*t    = vifp->v_tbf;
	timeout_id_t id = 0;

	/* Drop if packet is too large */
	if (p_len > MAX_BKT_SIZE) {
		mrtstat.mrts_pkt2large++;
		freemsg(mp);
		return;
	}
	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "tbf_ctrl: SEND vif %ld, qlen %d, ipha_dst 0x%x",
		    (ptrdiff_t)(vifp - viftable), t->tbf_q_len,
		    ntohl(ipha->ipha_dst));
	}

	mutex_enter(&t->tbf_lock);

	tbf_update_tokens(vifp);

	/*
	 * If there are enough tokens,
	 * and the queue is empty, send this packet out.
	 */
	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "tbf_control: vif %d, TOKENS  %d, pkt len  %d, qlen  %d",
		    (ptrdiff_t)(vifp - viftable), t->tbf_n_tok, p_len,
		    t->tbf_q_len);
	}
	/* No packets are queued */
	if (t->tbf_q_len == 0) {
		/* queue empty, send packet if enough tokens */
		if (p_len <= t->tbf_n_tok) {
			t->tbf_n_tok -= p_len;
			mutex_exit(&t->tbf_lock);
			tbf_send_packet(vifp, mp);
			return;
		} else {
			/* Queue packet and timeout till later */
			tbf_queue(vifp, mp);
			ASSERT(vifp->v_timeout_id == 0);
			vifp->v_timeout_id = qtimeout(vifp->v_ipif->ipif_rq,
			    tbf_reprocess_q, vifp, TBF_REPROCESS);
		}
	} else if (t->tbf_q_len < t->tbf_max_q_len) {
		/* Finite queue length, so queue pkts and process queue */
		tbf_queue(vifp, mp);
		tbf_process_q(vifp);
	} else {
		/* Check that we have UDP header with IP header */
		size_t hdr_length = IPH_HDR_LENGTH(ipha) +
					sizeof (struct udphdr);

		if ((mp->b_wptr - mp->b_rptr) < hdr_length) {
			if (!pullupmsg(mp, hdr_length)) {
				freemsg(mp);
				ip1dbg(("tbf_ctl: couldn't pullup udp hdr, "
				    "vif %ld src 0x%x dst 0x%x\n",
				    (ptrdiff_t)(vifp - viftable),
				    ntohl(ipha->ipha_src),
				    ntohl(ipha->ipha_dst)));
				mutex_exit(&vifp->v_tbf->tbf_lock);
				return;
			} else
				/* Have to reassign ipha after pullupmsg */
				ipha = (ipha_t *)mp->b_rptr;
		}
		/*
		 * Queue length too much,
		 * try to selectively dq, or queue and process
		 */
		if (!tbf_dq_sel(vifp, ipha)) {
			mrtstat.mrts_q_overflow++;
			freemsg(mp);
		} else {
			tbf_queue(vifp, mp);
			tbf_process_q(vifp);
		}
	}
	if (t->tbf_q_len == 0) {
		id = vifp->v_timeout_id;
		vifp->v_timeout_id = 0;
	}
	mutex_exit(&vifp->v_tbf->tbf_lock);
	if (id != 0)
		(void) quntimeout(ip_g_mrouter, id);
}

/*
 * Adds a packet to the tbf queue at the interface.
 * The ipha is for mcastgrp destination for phyint and encap.
 */
static void
tbf_queue(struct vif *vifp, mblk_t *mp)
{
	struct tbf	*t = vifp->v_tbf;

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "tbf_queue: vif %ld", (ptrdiff_t)(vifp - viftable));
	}
	ASSERT(MUTEX_HELD(&t->tbf_lock));

	if (t->tbf_t == NULL) {
		/* Queue was empty */
		t->tbf_q = mp;
	} else {
		/* Insert at tail */
		t->tbf_t->b_next = mp;
	}
	/* set new tail pointer */
	t->tbf_t = mp;

	mp->b_next = mp->b_prev = NULL;

	t->tbf_q_len++;
}

/*
 * Process the queue at the vif interface.
 * Drops the tbf_lock when sending packets.
 *
 * NOTE : The caller should quntimeout if the queue length is 0.
 */
static void
tbf_process_q(struct vif *vifp)
{
	mblk_t	*mp;
	struct tbf	*t = vifp->v_tbf;
	size_t	len;

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "tbf_process_q 1: vif %ld qlen = %d",
		    (ptrdiff_t)(vifp - viftable), t->tbf_q_len);
	}

	/*
	 * Loop through the queue at the interface and send
	 * as many packets as possible.
	 */
	ASSERT(MUTEX_HELD(&t->tbf_lock));

	while (t->tbf_q_len > 0) {
		mp = t->tbf_q;
		len = (size_t)msgdsize(mp); /* length of ip pkt */

		/* Determine if the packet can be sent */
		if (len <= t->tbf_n_tok) {
			/*
			 * If so, reduce no. of tokens, dequeue the packet,
			 * send the packet.
			 */
			t->tbf_n_tok -= len;

			t->tbf_q = mp->b_next;
			if (--t->tbf_q_len == 0) {
				t->tbf_t = NULL;
			}
			mp->b_next = NULL;
			/* Exit mutex before sending packet, then re-enter */
			mutex_exit(&t->tbf_lock);
			tbf_send_packet(vifp, mp);
			mutex_enter(&t->tbf_lock);
		} else
			break;
	}
}

/* Called at tbf timeout to update tokens, process q and reset timer.  */
static void
tbf_reprocess_q(void *arg)
{
	struct vif *vifp = arg;

	mutex_enter(&vifp->v_tbf->tbf_lock);
	vifp->v_timeout_id = 0;
	tbf_update_tokens(vifp);

	tbf_process_q(vifp);

	if (vifp->v_tbf->tbf_q_len > 0) {
		vifp->v_timeout_id = qtimeout(vifp->v_ipif->ipif_rq,
		    tbf_reprocess_q, vifp, TBF_REPROCESS);
	}
	mutex_exit(&vifp->v_tbf->tbf_lock);

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "tbf_reprcess_q: vif %ld timeout id = %d",
		    (ptrdiff_t)(vifp - viftable), vifp->v_timeout_id);
	}
}

/*
 * Function that will selectively discard a member of the tbf queue,
 * based on the precedence value and the priority.
 *
 * NOTE : The caller should quntimeout if the queue length is 0.
 */
static int
tbf_dq_sel(struct vif *vifp, ipha_t *ipha)
{
	uint_t		p;
	struct tbf		*t = vifp->v_tbf;
	mblk_t		**np;
	mblk_t		*last, *mp;

	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "dq_sel: vif %ld dst 0x%x",
		    (ptrdiff_t)(vifp - viftable), ntohl(ipha->ipha_dst));
	}

	ASSERT(MUTEX_HELD(&t->tbf_lock));
	p = priority(vifp, ipha);

	np = &t->tbf_q;
	last = NULL;
	while ((mp = *np) != NULL) {
		if (p > (priority(vifp, (ipha_t *)mp->b_rptr))) {
			*np = mp->b_next;
			/* If removing the last packet, fix the tail pointer */
			if (mp == t->tbf_t)
				t->tbf_t = last;
			mp->b_prev = mp->b_next = NULL;
			freemsg(mp);
			/*
			 * It's impossible for the queue to be empty, but
			 * we check anyway.
			 */
			if (--t->tbf_q_len == 0) {
				t->tbf_t = NULL;
			}
			mrtstat.mrts_drop_sel++;
			return (1);
		}
		np = &mp->b_next;
		last = mp;
	}
	return (0);
}

/* Sends packet, 2 cases - encap tunnel, phyint.  */
static void
tbf_send_packet(struct vif *vifp, mblk_t *mp)
{
	ipif_t  *ipif;

	/* If encap tunnel options */
	if (vifp->v_flags & VIFF_TUNNEL)  {
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "tbf_send_pkt: ENCAP tunnel vif %ld",
			    (ptrdiff_t)(vifp - viftable));
		}

		/*
		 * Feed into ip_wput which will set the ident field and
		 * checksum the encapsulating header.
		 * BSD gets the cached route vifp->v_route from ip_output()
		 * to speed up route table lookups. Not necessary in SunOS 5.x.
		 */
		put(vifp->v_ipif->ipif_wq, mp);
		return;

		/* phyint */
	} else {
		/* Need to loop back to members on the outgoing interface. */
		ipha_t  *ipha;
		ipaddr_t    dst;
		ipha  = (ipha_t *)mp->b_rptr;
		dst  = ipha->ipha_dst;
		ipif = vifp->v_ipif;

		if (ilm_lookup_ipif(ipif, dst) != NULL) {
			/*
			 * The packet is not yet reassembled, thus we need to
			 * pass it to ip_rput_local for checksum verification
			 * and reassembly (and fanout the user stream).
			 */
			mblk_t 	*mp_loop;
			ire_t	*ire;

			if (ip_mrtdebug > 1) {
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "tbf_send_pkt: loopback vif %ld",
				    (ptrdiff_t)(vifp - viftable));
			}
			mp_loop = copymsg(mp);
			ire = ire_ctable_lookup(~0, 0, IRE_BROADCAST, NULL,
			    NULL, MATCH_IRE_TYPE);

			if (mp_loop != NULL && ire != NULL) {
				ip_rput_local(ipif->ipif_rq, mp_loop,
				    (ipha_t *)mp_loop->b_rptr, ire, 0);
			} else {
				/* Either copymsg failed or no ire */
				(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
				    "tbf_send_pkt: mp_loop 0x%p, ire 0x%p "
				    "vif %ld\n", mp_loop, ire,
				    (ptrdiff_t)(vifp - viftable));
			}
			if (ire != NULL)
				ire_refrele(ire);
		}
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "tbf_send_pkt: phyint forward  vif %ld dst = 0x%x",
			    (ptrdiff_t)(vifp - viftable), ntohl(dst));
		}
		ip_rput_forward_multicast(dst, mp, ipif);
	}
}

/*
 * Determine the current time and then the elapsed time (between the last time
 * and time now).  Update the no. of tokens in the bucket.
 */
static void
tbf_update_tokens(struct vif *vifp)
{
	timespec_t	tp;
	hrtime_t	tm;
	struct tbf	*t = vifp->v_tbf;

	ASSERT(MUTEX_HELD(&t->tbf_lock));

	/* Time in secs and nsecs, rate limit in kbits/sec */
	gethrestime(&tp);

	/*LINTED*/
	TV_DELTA(tp, t->tbf_last_pkt_t, tm);

	/*
	 * This formula is actually
	 * "time in seconds" * "bytes/second".  Scaled for nsec.
	 * (tm/1000000000) * (v_rate_limit * 1000 * (1000/1024) /8)
	 *
	 * The (1000/1024) was introduced in add_vif to optimize
	 * this divide into a shift.
	 */
	t->tbf_n_tok += (tm/1000) * vifp->v_rate_limit / 1024 / 8;
	t->tbf_last_pkt_t = tp;

	if (t->tbf_n_tok > MAX_BKT_SIZE)
		t->tbf_n_tok = MAX_BKT_SIZE;
	if (ip_mrtdebug > 1) {
		(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
		    "tbf_update_tok: tm %d  tok %d vif %ld",
		    tm, t->tbf_n_tok, (ptrdiff_t)(vifp - viftable));
	}
}

/*
 * Priority currently is based on port nos.
 * Different forwarding mechanisms have different ways
 * of obtaining the port no. Hence, the vif must be
 * given along with the packet itself.
 *
 */
static int
priority(struct vif *vifp, ipha_t *ipha)
{
	int prio;

	/* Temporary hack; may add general packet classifier some day */

	ASSERT(MUTEX_HELD(&vifp->v_tbf->tbf_lock));

	/*
	 * The UDP port space is divided up into four priority ranges:
	 * [0, 16384)	: unclassified - lowest priority
	 * [16384, 32768)	: audio - highest priority
	 * [32768, 49152)	: whiteboard - medium priority
	 * [49152, 65536)	: video - low priority
	 */

	if (ipha->ipha_protocol == IPPROTO_UDP) {
		struct udphdr *udp =
		    (struct udphdr *)((char *)ipha + IPH_HDR_LENGTH(ipha));
		switch (ntohs(udp->uh_dport) & 0xc000) {
		case 0x4000:
			prio = 70;
			break;
		case 0x8000:
			prio = 60;
			break;
		case 0xc000:
			prio = 55;
			break;
		default:
			prio = 50;
			break;
		}
		if (ip_mrtdebug > 1) {
			(void) mi_strlog(ip_g_mrouter, 1, SL_TRACE,
			    "priority: port %x prio %d\n",
			    ntohs(udp->uh_dport), prio);
		}
	} else
		prio = 50;  /* default priority */
	return (prio);
}

/*
 * End of token bucket filter modifications
 */



/*
 * Produces data for netstat -M.
 */
int
ip_mroute_stats(mblk_t *mp)
{
	mrtstat.mrts_vifctlSize = sizeof (struct vifctl);
	mrtstat.mrts_mfcctlSize = sizeof (struct mfcctl);
	if (!snmp_append_data(mp, (char *)&mrtstat, sizeof (mrtstat))) {
		ip0dbg(("ip_mroute_stats: failed %ld bytes\n",
		    (size_t)sizeof (mrtstat)));
		return (0);
	}
	return (1);
}

/*
 * Sends info for SNMP's MIB.
 */
int
ip_mroute_vif(mblk_t *mp)
{
	struct vifctl 	vi;
	vifi_t		vifi;

	for (vifi = 0; vifi < numvifs; vifi++) {
		if (viftable[vifi].v_lcl_addr.s_addr == 0)
			continue;
		vi.vifc_vifi = vifi;
		vi.vifc_flags = viftable[vifi].v_flags;
		vi.vifc_threshold = viftable[vifi].v_threshold;
		vi.vifc_rate_limit	= viftable[vifi].v_rate_limit;
		vi.vifc_lcl_addr	= viftable[vifi].v_lcl_addr;
		vi.vifc_rmt_addr	= viftable[vifi].v_rmt_addr;
		vi.vifc_pkt_in		= viftable[vifi].v_pkt_in;
		vi.vifc_pkt_out		= viftable[vifi].v_pkt_out;

		if (!snmp_append_data(mp, (char *)&vi, sizeof (vi))) {
			ip0dbg(("ip_mroute_vif: failed %ld bytes\n",
			    (size_t)sizeof (vi)));
			return (0);
		}
	}
	return (1);
}

/*
 * Called by ip_snmp_get to send up multicast routing table.
 */
int
ip_mroute_mrt(mblk_t *mp)
{
	int			i, j;
	struct mfc		*rt;
	struct mfcctl	mfcc;

	/* Loop over all hash buckets and their chains */
	for (i = 0; i < MFCTBLSIZ; i++) {
		for (rt = mfctable[i]; rt; rt = rt->mfc_next) {
			if (rt->mfc_rte != NULL)
				break;
			mfcc.mfcc_origin = rt->mfc_origin;
			mfcc.mfcc_mcastgrp = rt->mfc_mcastgrp;
			mfcc.mfcc_parent = rt->mfc_parent;
			mfcc.mfcc_pkt_cnt = rt->mfc_pkt_cnt;
			for (j = 0; j < (int)numvifs; j++)
				mfcc.mfcc_ttls[j] = rt->mfc_ttls[j];
			for (j = (int)numvifs; j < MAXVIFS; j++)
				mfcc.mfcc_ttls[j] = 0;

			if (!snmp_append_data(mp, (char *)&mfcc,
			    sizeof (mfcc))) {
				ip0dbg(("ip_mroute_mrt: failed %ld bytes\n",
				    (size_t)sizeof (mfcc)));
				return (0);
			}
		}
	}
	return (1);
}
