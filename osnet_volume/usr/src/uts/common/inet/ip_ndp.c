/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ip_ndp.c 1.7	99/11/17 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/strlog.h>
#include <sys/dlpi.h>
#include <sys/sockio.h>
#include <sys/tiuser.h>
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/ip.h>
#include <inet/ip_multi.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip6.h>
#include <inet/ip_ndp.h>

/*
 * Function names with nce_ prefix are static while function
 * names with ndp_ prefix are used by rest of the IP.
 */

static	boolean_t nce_cmp_ll_addr(nce_t *nce, char *new_ll_addr,
    uint32_t ll_addr_len);
static	void	nce_fastpath(nce_t *nce);
static	void	nce_ire_delete(nce_t *nce);
static	void	nce_ire_delete1(ire_t *ire, char *nce_arg);
static	void 	nce_set_ll(nce_t *nce, uchar_t *ll_addr);
static	nce_t	*nce_lookup_addr(ill_t *ill, const in6_addr_t *addr);
static	nce_t	*nce_lookup_mapping(ill_t *ill, const in6_addr_t *addr);
static	void	nce_make_mapping(nce_t *nce, uchar_t *addrpos,
    uchar_t *addr);
static	int	nce_set_multicast(ipif_t *ipif, const in6_addr_t *addr);
static	void	nce_queue_mp(nce_t *nce, mblk_t *mp);
static	void	nce_report1(nce_t *nce, uchar_t *mp_arg);
static	void	nce_resolv_failed(nce_t *nce);
static	mblk_t	*nce_udreq_alloc(ill_t *ill);
static	void	nce_update(nce_t *nce, uint16_t new_state,
    uchar_t *new_ll_addr);
static	uint32_t	nce_solicit(nce_t *nce, mblk_t *mp);
static	void	nce_xmit(ill_t *ill, uint32_t operation, uchar_t *haddr,
    const in6_addr_t *sender, const in6_addr_t *target, int flag);

/* NDP Cache Entry Hash Table */
#define	NCE_TABLE_SIZE	256
static	nce_t	*nce_hash_tbl[NCE_TABLE_SIZE];
static	nce_t	*nce_mask_entries;	/* mask not all ones */
static	int	ndp_g_walker = 0;	/* # of active thread */
					/* walking nce hash list */
/* ndp_g_walker_cleanup will be true, when deletion have to be defered */
static	boolean_t	ndp_g_walker_cleanup = B_FALSE;

#ifdef _BIG_ENDIAN
#define	IN6_IS_ADDR_MC_SOLICITEDNODE(addr) \
	((((addr)->s6_addr32[0] & 0xff020000) == 0xff020000) && \
	((addr)->s6_addr32[1] == 0x0) && \
	((addr)->s6_addr32[2] == 0x00000001) && \
	((addr)->s6_addr32[3] & 0xff000000) == 0xff000000)
#else	/* _BIG_ENDIAN */
#define	IN6_IS_ADDR_MC_SOLICITEDNODE(addr) \
	((((addr)->s6_addr32[0] & 0x000002ff) == 0x000002ff) && \
	((addr)->s6_addr32[1] == 0x0) && \
	((addr)->s6_addr32[2] == 0x01000000) && \
	((addr)->s6_addr32[3] & 0x000000ff) == 0x000000ff)
#endif

#define	NCE_HASH_PTR(addr) \
	(&(nce_hash_tbl[NCE_ADDR_HASH_V6(addr, NCE_TABLE_SIZE)]))
/*
 * NDP Cache Entry creation routine.
 * Mapped entries will never do NUD .
 * This routine must always be called with ndp_g_lock held.
 * Prior to return, nce_refcnt is incremented and ndp_g_lock is released.
 */
int
ndp_add(ill_t *ill, uchar_t *hw_addr, const in6_addr_t *addr,
    const in6_addr_t *mask, const in6_addr_t *extract_mask,
    uint32_t hw_extract_start, uint32_t flags, uint16_t state,
    nce_t **newnce)
{
static	nce_t		nce_nil;
	nce_t		*nce;
	mblk_t		*mp;
	mblk_t		*template;
	nce_t		**ncep;
	int		err = 0;

	ASSERT(MUTEX_HELD(&ndp_g_lock));
	ASSERT(ill != NULL);
	if (IN6_IS_ADDR_UNSPECIFIED(addr)) {
		ip0dbg(("ndp_add: no addr\n"));
		err = EINVAL;
		goto done;
	}
	if ((flags & ~NCE_EXTERNAL_FLAGS_MASK)) {
		ip0dbg(("ndp_add: flags = %x\n", (int)flags));
		return (EINVAL);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(extract_mask) &&
	    (flags & NCE_F_MAPPING)) {
		ip0dbg(("ndp_add: extract mask zero for mapping"));
		err = EINVAL;
		goto done;
	}
	/*
	 * Allocate the mblk to hold the nce.
	 *
	 * XXX This can come out of a separate cache - nce_cache.
	 * We don't need the mp anymore as there are no more
	 * "become_exclusives"
	 */
	mp = allocb(sizeof (nce_t), BPRI_MED);
	if (mp == NULL) {
		err = ENOMEM;
		goto done;
	}
	nce = (nce_t *)mp->b_rptr;
	mp->b_wptr = (uchar_t *)&nce[1];
	*nce = nce_nil;

	/*
	 * This one holds link layer address
	 */
	if (ill->ill_net_type == IRE_IF_RESOLVER) {
		template = nce_udreq_alloc(ill);
	} else {
		ASSERT((ill->ill_net_type == IRE_IF_NORESOLVER));
		ASSERT((ill->ill_resolver_mp != NULL));
		template = copyb(ill->ill_resolver_mp);
	}
	if (template == NULL) {
		freeb(mp);
		err = ENOMEM;
		goto done;
	}
	nce->nce_timer_mp = mi_timer_alloc(sizeof (ipt_t));
	if (nce->nce_timer_mp == NULL) {
		ip1dbg(("nd_ce_create: alloc of nce failed \n"));
		freeb(mp);
		freeb(template);
		err = ENOMEM;
		goto done;
	} else {
		ipt_t *timer = (ipt_t *)nce->nce_timer_mp->b_rptr;

		timer->func = (pfv_t)ndp_timer;
		timer->arg = (uchar_t *)nce;
	}
	nce->nce_ill = ill;
	nce->nce_flags = flags;
	nce->nce_state = state;
	nce->nce_pcnt = ND_MAX_UNICAST_SOLICIT;
	nce->nce_rcnt = ill->ill_xmit_count;
	nce->nce_addr = *addr;
	nce->nce_mask = *mask;
	nce->nce_extract_mask = *extract_mask;
	nce->nce_ll_extract_start = hw_extract_start;
	nce->nce_fp_mp = NULL;
	nce->nce_res_mp = template;
	if (state == ND_REACHABLE)
		nce->nce_last = TICK_TO_MSEC(lbolt64);
	else
		nce->nce_last = 0;
	nce->nce_qd_mp = NULL;
	nce->nce_mp = mp;
	if (hw_addr != NULL)
		nce_set_ll(nce, hw_addr);
	/* This one is for nce getting created */
	nce->nce_refcnt = 1;
	mutex_init(&nce->nce_lock, NULL, MUTEX_DEFAULT, NULL);
	if (nce->nce_flags & NCE_F_MAPPING) {
		ASSERT(IN6_IS_ADDR_MULTICAST(addr));
		ASSERT(!IN6_IS_ADDR_UNSPECIFIED(&nce->nce_mask));
		ASSERT(!IN6_IS_ADDR_UNSPECIFIED(&nce->nce_extract_mask));
		ncep = &nce_mask_entries;
	} else {
		ncep = ((nce_t **)NCE_HASH_PTR(*addr));
	}
	if ((nce->nce_next = *ncep) != NULL)
		nce->nce_next->nce_ptpn = &nce->nce_next;
	*ncep = nce;
	nce->nce_ptpn = ncep;
	*newnce = nce;
	/* This one is for nce being used by an active thread */
	NCE_REFHOLD(*newnce);
done:
	return (err);
}

int
ndp_lookup_then_add(ill_t *ill, uchar_t *hw_addr, const in6_addr_t *addr,
    const in6_addr_t *mask, const in6_addr_t *extract_mask,
    uint32_t hw_extract_start, uint32_t flags, uint16_t state,
    nce_t **newnce)
{
	int	err = 0;
	nce_t	*nce;

	mutex_enter(&ndp_g_lock);
	nce = nce_lookup_addr(ill, addr);
	if (nce == NULL) {
		err = ndp_add(ill,
		    hw_addr,
		    addr,
		    mask,
		    extract_mask,
		    hw_extract_start,
		    flags,
		    state,
		    newnce);
	} else {
		*newnce = nce;
		err = EEXIST;
	}
	mutex_exit(&ndp_g_lock);
	return (err);
}

/*
 * Remove all the CONDEMNED nces from the appropriate hash table.
 * We create a private list of NCEs, these may have ires pointing
 * to them, so the list will be passed through to clean up dependent
 * ires and only then we can do NCE_REFRELE which can make NCE inactive.
 */
static void
nce_remove(nce_t *nce, nce_t **free_nce_list)
{
	nce_t *nce1;
	nce_t **ptpn;

	ASSERT(MUTEX_HELD(&ndp_g_lock));
	ASSERT(ndp_g_walker == 0);
	for (; nce; nce = nce1) {
		nce1 = nce->nce_next;
		if (nce->nce_flags & NCE_F_CONDEMNED) {
			/* Cancel retransmit timer just in case */
			mutex_enter(&nce->nce_lock);
			mi_timer_stop(nce->nce_timer_mp);
			mutex_exit(&nce->nce_lock);
			ptpn = nce->nce_ptpn;
			nce1 = nce->nce_next;
			if (nce1 != NULL)
				nce1->nce_ptpn = ptpn;
			*ptpn = nce1;
			nce->nce_ptpn = NULL;
			nce->nce_next = NULL;
			nce->nce_next = *free_nce_list;
			*free_nce_list = nce;
		}
	}
}

void
ndp_delete(nce_t *nce)
{
	nce_t	**ptpn;
	nce_t	*nce1;

	ASSERT(nce->nce_refcnt >= 2);
	/* Get out of the hash list. */
	mutex_enter(&ndp_g_lock);
	if (nce->nce_ptpn == NULL) {
		mutex_exit(&ndp_g_lock);
		/* Some other thread is doing the delete */
		return;
	}
	if (ndp_g_walker > 0) {
		/*
		 * Can't unlink and delete - just mark as
		 * condemned for the walker to clean up
		 */
		ndp_g_walker_cleanup = B_TRUE;
		mutex_enter(&nce->nce_lock);
		nce->nce_flags |= NCE_F_CONDEMNED;
		mutex_exit(&nce->nce_lock);
		mutex_exit(&ndp_g_lock);
		return;
	}
	ASSERT(!(nce->nce_flags & NCE_F_CONDEMNED));
	/* Cancel retransmit timer */
	mutex_enter(&nce->nce_lock);
	mi_timer_stop(nce->nce_timer_mp);
	mutex_exit(&nce->nce_lock);
	ptpn = nce->nce_ptpn;
	nce1 = nce->nce_next;
	if (nce1 != NULL)
		nce1->nce_ptpn = ptpn;
	*ptpn = nce1;
	nce->nce_ptpn = NULL;
	nce->nce_next = NULL;
	mutex_exit(&ndp_g_lock);
	nce_ire_delete(nce);
}

void
ndp_inactive(nce_t *nce)
{
	mblk_t		**mpp;

	ASSERT(nce->nce_refcnt == 0);

	ASSERT(MUTEX_HELD(&nce->nce_lock));
	/* Free all nce allocated messages */
	mpp = &nce->nce_first_mp_to_free;
	do {
		while (*mpp != NULL) {
			mblk_t  *mp;

			mp = *mpp;
			*mpp = mp->b_next;
			mp->b_next = NULL;
			mp->b_prev = NULL;
			freemsg(mp);
		}
	} while (mpp++ != &nce->nce_last_mp_to_free);
	ASSERT(nce->nce_timer_mp != NULL);
	mi_timer_free(nce->nce_timer_mp);
	nce->nce_timer_mp = NULL;
	mutex_destroy(&nce->nce_lock);
	freeb(nce->nce_mp);
}

/*
 * ndp_walk routine.  Delete the nce if it is associated with the ill
 * that is going away.  Always called as a writer.
 */
void
ndp_delete_per_ill(nce_t *nce, uchar_t *arg)
{
	if ((nce != NULL) && nce->nce_ill == (ill_t *)arg) {
		ndp_delete(nce);
	}
}

/*
 * Walk a list of to be inactive NCEs and blow away all the ires.
 */
static void
nce_ire_delete_list(nce_t *nce)
{
	nce_t *nce_next;

	ASSERT(nce != NULL);
	while (nce != NULL) {
		nce_next = nce->nce_next;
		nce->nce_next = NULL;
		ire_walk_ill_v6(nce->nce_ill, nce_ire_delete1, (char *)nce);
		NCE_REFRELE(nce);
		nce = nce_next;
	}
}

/*
 * Delete an ire when the nce goes away.
 */
/* ARGSUSED */
static void
nce_ire_delete(nce_t *nce)
{
	ire_walk_ill_v6(nce->nce_ill, nce_ire_delete1, (char *)nce);
	NCE_REFRELE(nce);
}

/*
 * ire_walk routine used to delete every IRE that shares this nce
 */
static void
nce_ire_delete1(ire_t *ire, char *nce_arg)
{
	nce_t	*nce = (nce_t *)nce_arg;

	if (ire->ire_nce == nce)
		ire_delete(ire);
}

/*
 * Cache entry lookup.  Try to find an nce matching the parameters passed.
 * If one is found, the refcnt on the nce will be incremented.
 */
nce_t *
ndp_lookup(ill_t *ill, const in6_addr_t *addr)
{
	nce_t	*nce;

	mutex_enter(&ndp_g_lock);
	nce = nce_lookup_addr(ill, addr);
	if (nce == NULL)
		nce = nce_lookup_mapping(ill, addr);
	mutex_exit(&ndp_g_lock);
	return (nce);
}

/*
 * Cache entry lookup.  Try to find an nce matching the parameters passed.
 * Look only for exact entries (no mappings).  If an nce is found, increment
 * the hold count on that nce.
 */
static nce_t *
nce_lookup_addr(ill_t *ill, const in6_addr_t *addr)
{
	nce_t	*nce;

	ASSERT(ill != NULL);
	ASSERT(MUTEX_HELD(&ndp_g_lock));
	if (IN6_IS_ADDR_UNSPECIFIED(addr))
		return (NULL);
	nce = *((nce_t **)NCE_HASH_PTR(*addr));
	for (; nce != NULL; nce = nce->nce_next) {
		if (nce->nce_ill == ill) {
			if (IN6_ARE_ADDR_EQUAL(&nce->nce_addr, addr) &&
			    IN6_ARE_ADDR_EQUAL(&nce->nce_mask,
			    &ipv6_all_ones)) {
				NCE_REFHOLD(nce);
				break;
			}
		}
	}
	return (nce);
}

/*
 * Cache entry lookup.  Try to find an nce matching the parameters passed.
 * Look only for mappings.
 */
static nce_t *
nce_lookup_mapping(ill_t *ill, const in6_addr_t *addr)
{
	nce_t	*nce;

	ASSERT(ill != NULL);
	ASSERT(MUTEX_HELD(&ndp_g_lock));
	if (!IN6_IS_ADDR_MULTICAST(addr))
		return (NULL);
	nce = nce_mask_entries;
	for (; nce != NULL; nce = nce->nce_next)
		if (nce->nce_ill == ill &&
		    (V6_MASK_EQ(*addr, nce->nce_mask, nce->nce_addr))) {
			NCE_REFHOLD(nce);
			break;
		}
	return (nce);
}

/*
 * Process passed in parameters either from an incoming packet or via
 * user ioctl.
 */
void
ndp_process(nce_t *nce, uchar_t *hw_addr, uint32_t flag, boolean_t is_adv)
{
	ill_t	*ill = nce->nce_ill;
	uint32_t hw_addr_len = ill->ill_phys_addr_length;
	mblk_t	*mp;
	boolean_t ll_updated = B_FALSE;
	boolean_t ll_changed;

	/*
	 * No updates of link layer address or the neighbor state is
	 * allowed, when the cache is in NONUD state.  This still
	 * allows for responding to reachability solicitation.
	 */
	mutex_enter(&nce->nce_lock);
	if (nce->nce_state == ND_INCOMPLETE) {
		if (hw_addr == NULL) {
			mutex_exit(&nce->nce_lock);
			return;
		}
		nce_set_ll(nce, hw_addr);
		/*
		 * Update nce state and send the queued packets
		 * back to ip this time ire will be added.
		 */
		if (flag & ND_NA_FLAG_SOLICITED) {
			nce_update(nce, ND_REACHABLE, NULL);
		} else {
			nce_update(nce, ND_STALE, NULL);
		}
		mutex_exit(&nce->nce_lock);
		nce_fastpath(nce);
		mutex_enter(&nce->nce_lock);
		mp = nce->nce_qd_mp;
		nce->nce_qd_mp = NULL;
		mutex_exit(&nce->nce_lock);
		while (mp != NULL) {
			mblk_t *nxt_mp;
			queue_t *fwdq;

			nxt_mp = mp->b_next;
			mp->b_next = NULL;
			fwdq = (queue_t *)mp->b_prev;
			mp->b_prev = NULL;
			/*
			 * Send a forwarded packet back into ip_rput_v6
			 * just as in ire_send_v6().
			 * Extract the queue from b_prev (set in
			 * ip_rput_data_v6).
			 */
			if (fwdq != NULL) {
				/*
				 * Forwarded packets hop count will
				 * get decremented in ip_rput_data_v6
				 */
				put(RD(fwdq), mp);
			} else {
				/*
				 * Send locally originated packets back
				 * into * ip_wput_v6.
				 */
				put(WR(ill->ill_rq), mp);
			}
			mp = nxt_mp;
		}
		return;
	}
	ll_changed = nce_cmp_ll_addr(nce, (char *)hw_addr, hw_addr_len);
	if (!is_adv) {
		/* If this is a SOLICITATION request only */
		if (ll_changed)
			nce_update(nce, ND_STALE, hw_addr);
		mutex_exit(&nce->nce_lock);
		return;
	}
	if (!(flag & ND_NA_FLAG_OVERRIDE) && ll_changed) {
		/* If in any other state than REACHABLE, ignore */
		if (nce->nce_state == ND_REACHABLE) {
			nce_update(nce, ND_STALE, NULL);
		}
		mutex_exit(&nce->nce_lock);
		return;
	}
	if ((flag & ND_NA_FLAG_OVERRIDE) ||
	    (!(flag & ND_NA_FLAG_OVERRIDE) && !ll_changed) ||
	    (hw_addr == NULL)) {
		if (ll_changed) {
			nce_update(nce, ND_UNCHANGED, hw_addr);
			ll_updated = B_TRUE;
		}
		if (flag & ND_NA_FLAG_SOLICITED) {
			nce_update(nce, ND_REACHABLE, NULL);
		} else {
			if (ll_updated) {
				nce_update(nce, ND_STALE, NULL);
			}
		}
		mutex_exit(&nce->nce_lock);
		if (!(flag & ND_NA_FLAG_ROUTER) && (nce->nce_flags &
		    NCE_F_ISROUTER)) {
			/*
			 * Router turned to host, just remove the entry.
			 */
			ndp_delete(nce);
		}
	} else {
		mutex_exit(&nce->nce_lock);
	}
}

/*
 * Pass arg1 to the pfi supplied, along with each nce in existence.
 * ndp_walk() places a REFHOLD on the nce and drops the lock when
 * walking the hash list.
 */
void
ndp_walk(ill_t *ill, pfi_t pfi, uchar_t *arg1)
{

	nce_t	*nce;
	nce_t	*nce1;
	nce_t	**ncep;
	nce_t	*free_nce_list = NULL;

	mutex_enter(&ndp_g_lock);
	ndp_g_walker++;	/* Prevent ndp_delete from unlink and free of NCE */
	mutex_exit(&ndp_g_lock);
	for (ncep = nce_hash_tbl; ncep < A_END(nce_hash_tbl); ncep++) {
		for (nce = *ncep; nce; nce = nce1) {
			nce1 = nce->nce_next;
			if (ill == NULL || nce->nce_ill == ill) {
				/*
				 * Strictly speaking, the REFHOLD here is not
				 * necessary.  We do it to be consistent with
				 * the notion of a bump up refcount per
				 * active thread.
				 */
				NCE_REFHOLD(nce);
				(*pfi)(nce, arg1);
				NCE_REFRELE(nce);
			}
		}
	}
	for (nce = nce_mask_entries; nce; nce = nce1) {
		nce1 = nce->nce_next;
		if (ill == NULL || nce->nce_ill == ill) {
			/*
			 * Strictly speaking, the REFHOLD here is not
			 * necessary.  We do it to be consistent with
			 * the notion of a bump up refcount per
			 * active thread.
			 */
			NCE_REFHOLD(nce);
			(*pfi)(nce, arg1);
			NCE_REFRELE(nce);
		}
	}
	mutex_enter(&ndp_g_lock);
	ndp_g_walker--;
	/*
	 * While NCE's are removed from global list they are placed
	 * in a private list, to be passed to nce_ire_delete_list().
	 * The reason is, there may be ires pointing to this nce
	 * which needs to cleaned up.
	 */
	if (ndp_g_walker_cleanup && ndp_g_walker == 0) {
		/* Time to delete condemned entries */
		for (ncep = nce_hash_tbl; ncep < A_END(nce_hash_tbl); ncep++) {
			nce = *ncep;
			if (nce != NULL) {
				nce_remove(nce, &free_nce_list);
			}
		}
		nce = nce_mask_entries;
		if (nce != NULL) {
			nce_remove(nce, &free_nce_list);
		}
	}
	ndp_g_walker_cleanup = B_FALSE;
	mutex_exit(&ndp_g_lock);

	if (free_nce_list != NULL) {
		nce_ire_delete_list(free_nce_list);
	}
}

/*
 * Process resolve requests.  Handles both mapped entries
 * as well as cases that needs to be send out on the wire.
 * Lookup a NCE for a given IRE.  Regardless of whether one exists
 * or one is created, we defer making ire point to nce until the
 * ire is actually added at which point the nce_refcnt on the nce is
 * incremented.  This is done primarily to have symmetry between ire_add()
 * and ire_delete() which decrements the nce_refcnt, when an ire is deleted.
 */
int
ndp_resolver(ipif_t *ipif, const in6_addr_t *dst, mblk_t *mp)
{
	ill_t		*ill = ipif->ipif_ill;
	nce_t		*nce;
	int		err = 0;
	uint32_t	ms;

	ASSERT(ill != NULL);
	if (IN6_IS_ADDR_MULTICAST(dst)) {
		err = nce_set_multicast(ipif, dst);
		return (err);
	}
	err = ndp_lookup_then_add(ill,
	    NULL,	/* No hardware address */
	    dst,
	    &ipv6_all_ones,
	    &ipv6_all_zeros,
	    0,
	    ipif->ipif_flags & IFF_NONUD ? NCE_F_NONUD : 0,
	    ND_INCOMPLETE,
	    &nce);

	switch (err) {
	case 0:
		/*
		 * New cache entry was created. Make sure that the state
		 * is not ND_INCOMPLETE. It can be in some other state
		 * even before we send out the solicitation as we could
		 * get un-solicited advertisements.
		 */
		mutex_enter(&nce->nce_lock);
		if (nce->nce_state != ND_INCOMPLETE) {
			mutex_exit(&nce->nce_lock);
			NCE_REFRELE(nce);
			return (0);
		}
		ms = nce_solicit(nce, mp);
		if (ms == 0) {
			/* The caller will free mp */
			mutex_exit(&nce->nce_lock);
			ndp_delete(nce);
			NCE_REFRELE(nce);
			return (EBUSY);
		}
		mi_timer(ill->ill_rq, nce->nce_timer_mp, (clock_t)ms);
		mutex_exit(&nce->nce_lock);
		NCE_REFRELE(nce);
		return (EINPROGRESS);
	case EEXIST:
		/* Resolution in progress just queue the packet */
		mutex_enter(&nce->nce_lock);
		if (nce->nce_state == ND_INCOMPLETE) {
			nce_queue_mp(nce, mp);
			mutex_exit(&nce->nce_lock);
			err = EINPROGRESS;
		} else {
			mutex_exit(&nce->nce_lock);
			/*
			 * Any other state implies we have
			 * a nce but IRE needs to be added ...
			 */
			err = 0;
		}
		NCE_REFRELE(nce);
		break;
	default:
		ip1dbg(("ndp_resolver: Can't create NCE %d\n", err));
		break;
	}
	return (err);
}

/*
 * When there is no resolver, the link layer template is passed in
 * the IRE.
 * Lookup a NCE for a given IRE.  Regardless of whether one exists
 * or one is created, we defer making ire point to nce until the
 * ire is actually added at which point the nce_refcnt on the nce is
 * incremented.  This is done primarily to have symmetry between ire_add()
 * and ire_delete() which decrements the nce_refcnt, when an ire is deleted.
 */
int
ndp_noresolver(ipif_t *ipif, const in6_addr_t *dst)
{
	ill_t		*ill = ipif->ipif_ill;
	nce_t		*nce;
	int		err = 0;
	int		nonud;

	ASSERT(ill != NULL);
	if (IN6_IS_ADDR_MULTICAST(dst)) {
		err = nce_set_multicast(ipif, dst);
		return (err);
	}

	nonud = (ipif->ipif_flags & IFF_NONUD);
	err = ndp_lookup_then_add(ill,
	    NULL,	/* hardware address */
	    dst,
	    &ipv6_all_ones,
	    &ipv6_all_zeros,
	    0,
	    nonud ? NCE_F_NONUD : 0,
	    ND_REACHABLE,
	    &nce);

	switch (err) {
	case 0:
		/*
		 * Cache entry with a proper resolver cookie was
		 * created.
		 */
		nce_fastpath(nce);
		NCE_REFRELE(nce);
		break;
	case EEXIST:
		err = 0;
		NCE_REFRELE(nce);
		break;
	default:
		ip1dbg(("ndp_noresolve: Can't create NCE %d\n", err));
		break;
	}
	return (err);
}

/*
 * For each interface an entry is added for the unspecified multicast group.
 * Here that mapping is used to form the multicast cache entry for a particular
 * multicast destination.
 */
static int
nce_set_multicast(ipif_t *ipif, const in6_addr_t *dst)
{
	ill_t		*ill = ipif->ipif_ill;
	nce_t		*mnce;	/* Multicast mapping entry */
	nce_t		*nce;
	uchar_t		*hw_addr = NULL;
	int		err = 0;

	ASSERT(ill != NULL);
	ASSERT(!(IN6_IS_ADDR_UNSPECIFIED(dst)));

	mutex_enter(&ndp_g_lock);
	nce = nce_lookup_addr(ill, dst);
	if (nce != NULL) {
		mutex_exit(&ndp_g_lock);
		NCE_REFRELE(nce);
		return (0);
	}
	/* No entry, now lookup for a mapping this should never fail */
	mnce = nce_lookup_mapping(ill, dst);
	if (mnce == NULL) {
		/* Something broken for the interface. */
		mutex_exit(&ndp_g_lock);
		return (ESRCH);
	}
	ASSERT(mnce->nce_flags & NCE_F_MAPPING);
	if (ill->ill_net_type == IRE_IF_RESOLVER) {
		/*
		 * For IRE_IF_RESOLVER a hardware mapping can be
		 * generated, for IRE_IF_NORESOLVER, resolution cookie
		 * in the ill is copied in ndp_add().
		 */
		hw_addr = kmem_alloc(ill->ill_phys_addr_length, KM_NOSLEEP);
		if (hw_addr == NULL) {
			mutex_exit(&ndp_g_lock);
			NCE_REFRELE(mnce);
			return (ENOMEM);
		}
		nce_make_mapping(mnce, hw_addr, (uchar_t *)dst);
	}
	NCE_REFRELE(mnce);
	/*
	 * IRE_IF_NORESOLVER type simply copies the resolution
	 * cookie passed in.  So no hw_addr is needed.
	 */
	err = ndp_add(ill,
	    hw_addr,
	    dst,
	    &ipv6_all_ones,
	    &ipv6_all_zeros,
	    0,
	    NCE_F_NONUD,
	    ND_REACHABLE,
	    &nce);
	mutex_exit(&ndp_g_lock);
	if (hw_addr != NULL)
		kmem_free(hw_addr, ill->ill_phys_addr_length);
	if (err != 0) {
		ip1dbg(("ndp_resolve: create failed" "%d\n", err));
		return (err);
	}
	nce_fastpath(nce);
	NCE_REFRELE(nce);
	return (0);
}

/*
 * Return the link layer address, and any flags of a nce.
 */
int
ndp_query(ill_t *ill, struct lif_nd_req *lnr)
{
	nce_t		*nce;
	in6_addr_t	*addr;
	sin6_t		*sin6;

	ASSERT(ill != NULL);
	sin6 = (sin6_t *)&lnr->lnr_addr;
	addr =  &sin6->sin6_addr;

	nce = ndp_lookup(ill, addr);
	if (nce == NULL)
		return (ESRCH);
	/* If in INCOMPLETE state, no link layer address is avaialbe  yet */
	if (nce->nce_state == ND_INCOMPLETE)
		goto done;
	lnr->lnr_hdw_len = ill->ill_phys_addr_length;
	ASSERT(NCE_LL_ADDR_OFFSET(ill) + ill->ill_phys_addr_length <=
	    sizeof (lnr->lnr_hdw_addr));
	bcopy(nce->nce_res_mp->b_rptr + NCE_LL_ADDR_OFFSET(ill),
	    (uchar_t *)&lnr->lnr_hdw_addr, ill->ill_phys_addr_length);
	if (nce->nce_flags & NCE_F_ISROUTER)
		lnr->lnr_flags = NDF_ISROUTER_ON;
	if (nce->nce_flags & NCE_F_PROXY)
		lnr->lnr_flags |= NDF_PROXY_ON;
	if (nce->nce_flags & NCE_F_ANYCAST)
		lnr->lnr_flags |= NDF_ANYCAST_ON;
done:
	NCE_REFRELE(nce);
	return (0);
}

/*
 * Send Enable/Disable multicast reqs to driver.
 */
int
ndp_mcastreq(ill_t *ill, const in6_addr_t *addr, uint32_t hw_addr_len,
    uint32_t hw_addr_offset, mblk_t *mp)
{
	nce_t		*nce;
	uchar_t		*hw_addr;
	uchar_t		*cp;

	ASSERT(ill != NULL);
	ASSERT(ill->ill_net_type == IRE_IF_RESOLVER);
	hw_addr = mi_offset_paramc(mp, hw_addr_offset, hw_addr_len);
	if (hw_addr == NULL ||
	    !IN6_IS_ADDR_MULTICAST(addr)) {
		freemsg(mp);
		return (EINVAL);
	}
	mutex_enter(&ndp_g_lock);
	nce = nce_lookup_mapping(ill, addr);
	if (nce == NULL) {
		mutex_exit(&ndp_g_lock);
		freemsg(mp);
		return (ESRCH);
	}
	mutex_exit(&ndp_g_lock);
	ASSERT(hw_addr_len >= ill->ill_phys_addr_length);
	cp = (uchar_t *)mp->b_rptr;
	/*
	 * Update dl_addr_length and dl_addr_offset for primitives that
	 * have physical addresses as opposed to full saps
	 */
	switch (((union DL_primitives *)mp->b_rptr)->dl_primitive) {
	case DL_ENABMULTI_REQ: {
		dl_enabmulti_req_t *dl = (dl_enabmulti_req_t *)cp;

		/*
		 * Remove the sap from the DL address either at the end or
		 * in front of the physical address.
		 */
		if (ill->ill_sap_length < 0)
			dl->dl_addr_length += ill->ill_sap_length;
		else {
			dl->dl_addr_offset += ill->ill_sap_length;
			hw_addr += ill->ill_sap_length;
			dl->dl_addr_length -= ill->ill_sap_length;
		}
		/* Track the state if this is the first enabmulti */
		if (ill->ill_dlpi_multicast_state == IDMS_UNKNOWN)
			ill->ill_dlpi_multicast_state = IDMS_INPROGRESS;
		ip1dbg(("ndp_mcastreq: ENABMULTI\n"));
		break;
	}
	case DL_DISABMULTI_REQ: {
		dl_disabmulti_req_t *dl = (dl_disabmulti_req_t *)cp;

		/*
		 * Remove the sap from the DL address either at the end or
		 * in front of the physical address.
		 */
		if (ill->ill_sap_length < 0)
			dl->dl_addr_length += ill->ill_sap_length;
		else {
			dl->dl_addr_offset += ill->ill_sap_length;
			hw_addr += ill->ill_sap_length;
			dl->dl_addr_length -= ill->ill_sap_length;
		}
		ip1dbg(("ndp_mcastreq: DISABMULTI\n"));
		break;
	}
	default:
		NCE_REFRELE(nce);
		ip1dbg(("ndp_mcastreq: default\n"));
		return (EINVAL);
	}
	nce_make_mapping(nce, hw_addr, (uchar_t *)addr);
	NCE_REFRELE(nce);
	putnext(ill->ill_wq, mp);
	return (0);
}

/*
 * Send a neighbor solicitation.
 * Returns number of milliseconds after which we should either rexmit or abort.
 * Return of zero means we should abort.
 * The caller holds the nce_lock to protect nce_qd_mp and nce_rcnt.
 *
 * NOTE : This routine drops nce_lock (and later reacquires it) when sending
 * the packet.
 */
uint32_t
nce_solicit(nce_t *nce, mblk_t *mp)
{
	ill_t		*ill;
	ip6_t		*ip6h;
	in6_addr_t	src;
	in6_addr_t	dst;
	ipif_t		*ipif;
	ip6i_t		*ip6i;

	ASSERT(MUTEX_HELD(&nce->nce_lock));
	ill = nce->nce_ill;
	ASSERT(ill != NULL);

	/*
	 * If we can't send a soliciation now, we should at least
	 * retry in the future.
	 */
	if (!canput(ill->ill_wq)) {
		return (ill->ill_reachable_retrans_time);
	}
	if (nce->nce_rcnt == 0) {
		return (0);
	}

	if (mp == NULL) {
		ASSERT(nce->nce_qd_mp != NULL);
		mp = nce->nce_qd_mp;
	} else {
		nce_queue_mp(nce, mp);
	}
	ip6h = (ip6_t *)mp->b_rptr;
	if (ip6h->ip6_nxt == IPPROTO_RAW) {
		/*
		 * This message should have been pulled up already in
		 * ip_wput_v6. We can't do pullups here because the message
		 * could be from the nce_qd_mp which could have b_next/b_prev
		 * non-NULL.
		 */
		ip6i = (ip6i_t *)ip6h;
		ASSERT((mp->b_wptr - (uchar_t *)ip6i) >=
			    sizeof (ip6i_t) + IPV6_HDR_LEN);
		ip6h = (ip6_t *)(mp->b_rptr + sizeof (ip6i_t));
	}
	src = ip6h->ip6_src;
	/*
	 * If the src of outgoing packet is one of the assigned interface
	 * addresses use it, otherwise let ip pick a source.
	 */
	for (ipif = ill->ill_ipif; ipif != NULL; ipif = ipif->ipif_next)
		if (IN6_ARE_ADDR_EQUAL(&src, &ipif->ipif_v6lcl_addr))
			break;
	if (ipif == NULL)
		src = ipv6_all_zeros;
	dst = nce->nce_addr;
	nce->nce_rcnt--;
	mutex_exit(&nce->nce_lock);
	nce_xmit(ill, ND_NEIGHBOR_SOLICIT, ill->ill_hw_addr, &src, &dst, 0);
	mutex_enter(&nce->nce_lock);
	return (ill->ill_reachable_retrans_time);
}

void
ndp_input_solicit(ill_t *ill, mblk_t *mp)
{
	nd_neighbor_solicit_t *ns;
	uint32_t	hlen = ill->ill_phys_addr_length;
	uchar_t		*haddr = NULL;
	icmp6_t		*icmp_nd;
	ip6_t		*ip6h;
	nce_t		*our_nce = NULL;
	in6_addr_t	target;
	in6_addr_t	src;
	int		len;
	int		flag = 0;
	nd_opt_hdr_t	*opt = NULL;
	boolean_t	bad_solicit = B_FALSE;
	mib2_ipv6IfIcmpEntry_t	*mib = ill->ill_icmp6_mib;

	ip6h = (ip6_t *)mp->b_rptr;
	icmp_nd = (icmp6_t *)(mp->b_rptr + IPV6_HDR_LEN);
	len = mp->b_wptr - mp->b_rptr - IPV6_HDR_LEN;
	src = ip6h->ip6_src;
	ns = (nd_neighbor_solicit_t *)icmp_nd;
	target = ns->nd_ns_target;
	if (IN6_IS_ADDR_MULTICAST(&target)) {
		if (ip_debug > 2) {
			/* ip1dbg */
			pr_addr_dbg("ndp_input_solicit: Target is"
			    " multicast! %s\n", AF_INET6, &target);
		}
		bad_solicit = B_TRUE;
		goto done;
	}
	if (len > sizeof (nd_neighbor_solicit_t)) {
		/* Options present */
		opt = (nd_opt_hdr_t *)&ns[1];
		len -= sizeof (nd_neighbor_solicit_t);
		if (!ndp_verify_optlen(opt, len)) {
			ip1dbg(("ndp_input_solicit: Bad opt len\n"));
			bad_solicit = B_TRUE;
			goto done;
		}
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&src)) {
		/* Check to see if this is a valid DAD solicitation */
		if (!IN6_IS_ADDR_MC_SOLICITEDNODE(&ip6h->ip6_dst)) {
			if (ip_debug > 2) {
				/* ip1dbg */
				pr_addr_dbg("ndp_input_solicit: IPv6 "
				    "Destination is not solicited node "
				    "multicast %s\n", AF_INET6,
				    &ip6h->ip6_dst);
			}
			bad_solicit = B_TRUE;
			goto done;
		}
	}

	our_nce = ndp_lookup(ill, &target);
	/*
	 * If this is a valid Solicitation, a permanent
	 * entry should exist in the cache
	 */
	if (our_nce == NULL ||
	    !(our_nce->nce_flags & NCE_F_PERMANENT)) {
		ip1dbg(("ndp_input_solicit: Wrong target in NS?!"
		    "ifname=%s ", ill->ill_name));
		if (ip_debug > 2) {
			/* ip1dbg */
			pr_addr_dbg(" dst %s\n", AF_INET6, &target);
		}
		bad_solicit = B_TRUE;
		goto done;
	}

	/* At this point we should have a verified NS per spec */
	if (opt != NULL) {
		opt = ndp_get_option(opt, len, ND_OPT_SOURCE_LINKADDR);
		if (opt != NULL) {
			/*
			 * No source link layer address option should
			 * be present in a valid DAD request.
			 */
			if (IN6_IS_ADDR_UNSPECIFIED(&src)) {
				ip1dbg(("ndp_input_solicit: source link-layer "
				    "address option present with an "
				    "unspecified source. \n"));
				bad_solicit = B_TRUE;
				goto done;
			}
			haddr = (uchar_t *)&opt[1];
			if (hlen > opt->nd_opt_len * 8 ||
			    hlen == 0) {
				bad_solicit = B_TRUE;
				goto done;
			}
		}
	}
	/* Set override flag, it will be reset later if need be. */
	flag |= NDP_ORIDE;
	if (!IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst)) {
		flag |= NDP_UNICAST;
	}

	/*
	 * Create/update the entry for the soliciting node.
	 * or respond to outstanding queries, don't if
	 * the source is unspecified address.
	 */
	if (!IN6_IS_ADDR_UNSPECIFIED(&src)) {
		int	err = 0;
		nce_t	*nnce;

		err = ndp_lookup_then_add(ill,
		    haddr,
		    &src,	/* Soliciting nodes address */
		    &ipv6_all_ones,
		    &ipv6_all_zeros,
		    0,
		    0,
		    ND_STALE,
		    &nnce);
		switch (err) {
		case 0:
			/* done with this entry */
			NCE_REFRELE(nnce);
			break;
		case EEXIST:
			/*
			 * B_FALSE indicates this is not an
			 * an advertisement.
			 */
			ndp_process(nnce, haddr, 0, B_FALSE);
			NCE_REFRELE(nnce);
			break;
		default:
			ip1dbg(("ndp_input_solicit: Can't create NCE %d\n",
			    err));
			goto done;
		}
		flag |= NDP_SOLICITED;
	} else {
		/*
		 * This is a DAD req, multicast the advertisement
		 * to the all-nodes address.
		 */
		src = ipv6_all_hosts_mcast;
	}
	if (our_nce->nce_flags & NCE_F_ISROUTER)
		flag |= NDP_ISROUTER;
	if (our_nce->nce_flags & NCE_F_PROXY)
		flag &= ~NDP_ORIDE;
	/* Response to a solicitation */
	nce_xmit(ill,
	    ND_NEIGHBOR_ADVERT,
	    ill->ill_hw_addr,
	    &target,	/* Source and target of the advertisement pkt */
	    &src,	/* IP Destination (source of original pkt) */
	    flag);
done:
	if (bad_solicit)
		BUMP_MIB(mib->ipv6IfIcmpInBadNeighborSolicitations);
	if (our_nce != NULL)
		NCE_REFRELE(our_nce);
}

void
ndp_input_advert(ill_t *ill, mblk_t *mp)
{
	nd_neighbor_advert_t *na;
	uint32_t	hlen = ill->ill_phys_addr_length;
	uchar_t		*haddr = NULL;
	icmp6_t		*icmp_nd;
	ip6_t		*ip6h;
	nce_t		*dst_nce = NULL;
	in6_addr_t	target;
	nd_opt_hdr_t	*opt = NULL;
	int		len;
	boolean_t	bad_advert = B_FALSE;
	mib2_ipv6IfIcmpEntry_t	*mib = ill->ill_icmp6_mib;

	ip6h = (ip6_t *)mp->b_rptr;
	icmp_nd = (icmp6_t *)(mp->b_rptr + IPV6_HDR_LEN);
	len = mp->b_wptr - mp->b_rptr - IPV6_HDR_LEN;
	na = (nd_neighbor_advert_t *)icmp_nd;
	if (IN6_IS_ADDR_MULTICAST(&ip6h->ip6_dst) &&
	    (na->nd_na_flags_reserved & ND_NA_FLAG_SOLICITED)) {
		ip1dbg(("ndp_input_advert: Target is multicast but the "
		    "solicited flag is not zero\n"));
		bad_advert = B_TRUE;
		goto done;
	}
	target = na->nd_na_target;
	if (IN6_IS_ADDR_MULTICAST(&target)) {
		ip1dbg(("ndp_input_advert: Target is multicast!\n"));
		bad_advert = B_TRUE;
		goto done;
	}
	if (len > sizeof (nd_neighbor_advert_t)) {
		opt = (nd_opt_hdr_t *)&na[1];
		if (!ndp_verify_optlen(opt,
		    len - sizeof (nd_neighbor_advert_t))) {
			bad_advert = B_TRUE;
			goto done;
		}
		/* At this point we have a verified NA per spec */
		len -= sizeof (nd_neighbor_advert_t);
		opt = ndp_get_option(opt, len, ND_OPT_TARGET_LINKADDR);
		if (opt != NULL) {
			haddr = (uchar_t *)&opt[1];
			if (hlen > opt->nd_opt_len * 8 ||
			    hlen == 0) {
				bad_advert = B_TRUE;
				goto done;
			}
		}
	}

	/* Look to see if this is something we asked for */
	dst_nce = ndp_lookup(ill, &target);
	/*
	 *  If we don't have an nce, we did not ask for this.
	 *  just drop it.
	 */
	if (dst_nce == NULL)
		goto done;

	if (na->nd_na_flags_reserved & ND_NA_FLAG_ROUTER)
		dst_nce->nce_flags |= NCE_F_ISROUTER;
	/* B_TRUE indicates this an advertisement */
	ndp_process(dst_nce, haddr,
		na->nd_na_flags_reserved, B_TRUE);
	NCE_REFRELE(dst_nce);
done:
	if (bad_advert)
		BUMP_MIB(mib->ipv6IfIcmpInBadNeighborAdvertisements);
}

/*
 * Process NDP neighbor solicitation/advertisement messages.
 * The checksum has already checked o.k before reaching here.
 */
void
ndp_input(ill_t *ill, mblk_t *mp)
{
	icmp6_t		*icmp_nd;
	ip6_t		*ip6h;
	int		len;
	mib2_ipv6IfIcmpEntry_t	*mib = ill->ill_icmp6_mib;


	if (!pullupmsg(mp, -1)) {
		ip1dbg(("ndp_input: pullupmsg failed\n"));
		BUMP_MIB(ill->ill_ip6_mib->ipv6InDiscards);
		goto done;
	}
	ip6h = (ip6_t *)mp->b_rptr;
	if (ip6h->ip6_hops != IPV6_MAX_HOPS) {
		ip1dbg(("ndp_input: hoplimit != IPV6_MAX_HOPS\n"));
		BUMP_MIB(mib->ipv6IfIcmpBadHoplimit);
		goto done;
	}
	/*
	 * NDP does not accept any extension headers between the
	 * IP header and the ICMP header since e.g. a routing
	 * header could be dangerous.
	 * This assumes that any AH or ESP headers are removed
	 * by ip prior to passing the packet to ndp_input.
	 */
	if (ip6h->ip6_nxt != IPPROTO_ICMPV6) {
		ip1dbg(("ndp_input: Wrong next header 0x%x\n",
		    ip6h->ip6_nxt));
		BUMP_MIB(mib->ipv6IfIcmpInErrors);
		goto done;
	}
	icmp_nd = (icmp6_t *)(mp->b_rptr + IPV6_HDR_LEN);
	ASSERT(icmp_nd->icmp6_type == ND_NEIGHBOR_SOLICIT ||
	    icmp_nd->icmp6_type == ND_NEIGHBOR_ADVERT);
	if (icmp_nd->icmp6_code != 0) {
		ip1dbg(("ndp_input: icmp6 code != 0 \n"));
		BUMP_MIB(mib->ipv6IfIcmpInErrors);
		goto done;
	}
	len = mp->b_wptr - mp->b_rptr - IPV6_HDR_LEN;
	/*
	 * Make sure packet length is large enough for either
	 * a NS or a NA icmp packet.
	 */
	if (len <  sizeof (struct icmp6_hdr) + sizeof (struct in6_addr)) {
		ip1dbg(("ndp_input: packet too short\n"));
		BUMP_MIB(mib->ipv6IfIcmpInErrors);
		goto done;
	}
	if (icmp_nd->icmp6_type == ND_NEIGHBOR_SOLICIT) {
		ndp_input_solicit(ill, mp);
	} else {
		ndp_input_advert(ill, mp);
	}
done:
	freemsg(mp);
}

/*
 * nce_xmit is called to form and transmit a ND solicitation or
 * advertisement ICMP packet.
 */
static void
nce_xmit(ill_t *ill, uint32_t operation, uchar_t *haddr,
    const in6_addr_t *sender, const in6_addr_t *target, int flag)
{
	uint32_t	len;
	icmp6_t 	*icmp6;
	mblk_t		*mp;
	ip6_t		*ip6h;
	nd_opt_hdr_t	*opt;
	uint_t		plen;

	/* Don't include any link layer address option if none passed in */
	if (haddr != NULL)
		plen = (sizeof (nd_opt_hdr_t) +
		    ill->ill_phys_addr_length + 7)/8;
	else
		plen = 0;
	/*
	 * To play it safe always include link layer address option.
	 * Solicitation and advertisement headers have the same length.
	 */
	len = IPV6_HDR_LEN + sizeof (nd_neighbor_advert_t) + plen * 8;
	mp = allocb(len,  BPRI_LO);
	if (mp == NULL)
		return;
	bzero((char *)mp->b_rptr, len);
	mp->b_wptr = mp->b_rptr + len;
	ip6h = (ip6_t *)mp->b_rptr;
	ip6h->ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6h->ip6_plen = htons(len - IPV6_HDR_LEN);
	ip6h->ip6_nxt = IPPROTO_ICMPV6;
	ip6h->ip6_hops = IPV6_MAX_HOPS;
	ip6h->ip6_dst = *target;
	icmp6 = (icmp6_t *)&ip6h[1];

	opt = (nd_opt_hdr_t *)(mp->b_rptr + IPV6_HDR_LEN +
	    sizeof (nd_neighbor_advert_t));
	if (operation == ND_NEIGHBOR_SOLICIT) {
		nd_neighbor_solicit_t *ns = (nd_neighbor_solicit_t *)icmp6;

		if (plen != 0)
			opt->nd_opt_type = ND_OPT_SOURCE_LINKADDR;
		ip6h->ip6_src = *sender;
		ns->nd_ns_target = *target;
		if (!(flag & NDP_UNICAST)) {
			/* Form multicast address of the target */
			ip6h->ip6_dst = ipv6_solicited_node_mcast;
			ip6h->ip6_dst.s6_addr32[3] |=
			    ns->nd_ns_target.s6_addr32[3];
		}
	} else {
		nd_neighbor_advert_t *na = (nd_neighbor_advert_t *)icmp6;

		if (plen != 0)
			opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
		ip6h->ip6_src = *sender;
		na->nd_na_target = *sender;
		if (flag & NDP_ISROUTER)
			na->nd_na_flags_reserved |= ND_NA_FLAG_ROUTER;
		if (flag & NDP_SOLICITED)
			na->nd_na_flags_reserved |= ND_NA_FLAG_SOLICITED;
		if (flag & NDP_ORIDE)
			na->nd_na_flags_reserved |= ND_NA_FLAG_OVERRIDE;

	}
	if (plen != 0) {
		/* Fill in link layer address and option len */
		opt->nd_opt_len = (uint8_t)plen;
		bcopy(haddr, &opt[1], ill->ill_phys_addr_length);
	}
	icmp6->icmp6_type = operation;
	icmp6->icmp6_code = 0;
	/*
	 * Prepare for checksum by putting icmp length in the icmp
	 * checksum field. The checksum is calculated in ip_wput_v6.
	 */
	icmp6->icmp6_cksum = ip6h->ip6_plen;

	if (canput(ill->ill_wq))
		put(ill->ill_wq, mp);
	else
		freemsg(mp);
}

/*
 * Make a link layer address (does not include the SAP) from an nce.
 * To form the link layer address, use the last four bytes of ipv6
 * address passed in and the fixed offset stored in nce.
 */
static void
nce_make_mapping(nce_t *nce, uchar_t *addrpos, uchar_t *addr)
{
	uchar_t *mask, *to;
	ill_t	*ill = nce->nce_ill;
	int 	len;

	if (ill->ill_net_type == IRE_IF_NORESOLVER)
		return;
	ASSERT(nce->nce_res_mp != NULL);
	ASSERT(ill->ill_net_type == IRE_IF_RESOLVER);
	ASSERT(nce->nce_flags & NCE_F_MAPPING);
	ASSERT(!IN6_IS_ADDR_UNSPECIFIED(&nce->nce_extract_mask));
	ASSERT(addr != NULL);
	bcopy(nce->nce_res_mp->b_rptr + NCE_LL_ADDR_OFFSET(ill),
	    addrpos, ill->ill_phys_addr_length);
	len = MIN((int)ill->ill_phys_addr_length
		    - nce->nce_ll_extract_start, IPV6_ADDR_LEN);
	mask = (uchar_t *)&nce->nce_extract_mask;
	mask += (IPV6_ADDR_LEN - len);
	addr += (IPV6_ADDR_LEN - len);
	to = addrpos + nce->nce_ll_extract_start;
	while (len-- > 0)
		*to++ |= *mask++ & *addr++;
}

/*
 * Pass a cache report back out via NDD.
 */
/* ARGSUSED */
int
ndp_report(queue_t *q, mblk_t *mp, caddr_t	arg)
{
	(void) mi_mpprintf(mp, "ifname      hardware addr    flags"
			"     proto addr/mask");
	ndp_walk(NULL, (pfi_t)nce_report1, (uchar_t *)mp);
	return (0);
}

/*
 * Add a single line to the NDP Cache Entry Report.
 * To do:  This always prints 6 bytes of address.
 */
static void
nce_report1(nce_t *nce, uchar_t *mp_arg)
{
	ill_t		*ill = nce->nce_ill;
	uchar_t		local_buf[INET6_ADDRSTRLEN];
	uchar_t		flags_buf[10];
	uint32_t	flags = nce->nce_flags;
	mblk_t		*mp = (mblk_t *)mp_arg;
	uchar_t		*h;
	uchar_t		*m = flags_buf;
	in6_addr_t	v6addr;

	ASSERT(ill != NULL);
	h = nce->nce_res_mp->b_rptr + NCE_LL_ADDR_OFFSET(ill);
	v6addr = nce->nce_mask;
	if (flags & NCE_F_PERMANENT)
		*m++ = 'P';
	if (flags & NCE_F_ISROUTER)
		*m++ = 'R';
	if (flags & NCE_F_MAPPING)
		*m++ = 'M';
	*m = '\0';

	if (ill->ill_net_type == IRE_IF_RESOLVER) {
		(void) mi_mpprintf(mp,
		    "%8s %02x:%02x:%02x:%02x:%02x:%02x %5s %s/%d",
		    ill->ill_name,
		    h[0] & 0xFF, h[1] & 0xFF, h[2] & 0xFF, h[3] & 0xFF,
		    h[4] & 0xFF, h[5] & 0xFF,
		    (uchar_t *)&flags_buf,
		    inet_ntop(AF_INET6, (char *)&nce->nce_addr,
			(char *)local_buf, sizeof (local_buf)),
		    ip_mask_to_index_v6(&v6addr));
	} else {
		(void) mi_mpprintf(mp,
		    "%8s %9s %5s %s/%d",
		    ill->ill_name,
		    "None",
		    (uchar_t *)&flags_buf,
		    inet_ntop(AF_INET6, (char *)&nce->nce_addr,
			(char *)local_buf, sizeof (local_buf)),
			ip_mask_to_index_v6(&v6addr));
	}
}

mblk_t *
nce_udreq_alloc(ill_t *ill)
{
	mblk_t	*template_mp = NULL;
	dl_unitdata_req_t *dlur;
	int	sap_length;

	sap_length = ill->ill_sap_length;
	template_mp = ip_dlpi_alloc(sizeof (dl_unitdata_req_t) +
	    ill->ill_phys_addr_length + ABS(sap_length), DL_UNITDATA_REQ);
	if (template_mp == NULL)
		return (NULL);

	dlur = (dl_unitdata_req_t *)template_mp->b_rptr;
	dlur->dl_priority.dl_min = 0;
	dlur->dl_priority.dl_max = 0;
	dlur->dl_dest_addr_length = ABS(sap_length) + ill->ill_phys_addr_length;
	dlur->dl_dest_addr_offset = sizeof (dl_unitdata_req_t);

	/* Copy in the SAP value. */
	ASSERT((uchar_t *)dlur + NCE_LL_SAP_OFFSET(ill) <=
	    template_mp->b_wptr);
	bcopy(&ill->ill_sap + sizeof (ill->ill_sap) - ABS(sap_length),
	    (char *)dlur + NCE_LL_SAP_OFFSET(ill), ABS(sap_length));

	return (template_mp);
}

/*
 * NDP retransmit timer.
 * This timer goes off when:
 * a. It is time to retransmit NS for resolver.
 * b. It is time to send reachability probes.
 */
void
ndp_timer(void *arg)
{
	nce_t		*nce = arg;
	ill_t		*ill = nce->nce_ill;
	uint32_t	ms;
	char		addrbuf[INET6_ADDRSTRLEN];

	ASSERT(nce != NULL);
	/*
	 * Check the reachability state first.
	 */
	mutex_enter(&nce->nce_lock);
	/* Since nce_lock is already held, this is same as doing a REFHOLD */
	nce->nce_refcnt++;
	switch (nce->nce_state) {
	case ND_DELAY:
		nce->nce_state = ND_PROBE;
		mutex_exit(&nce->nce_lock);
		nce_xmit(ill, ND_NEIGHBOR_SOLICIT, ill->ill_hw_addr,
		    &ipv6_all_zeros, &nce->nce_addr, NDP_UNICAST);
		if (ip_debug > 3) {
			/* ip2dbg */
			pr_addr_dbg("ndp_timer: state for %s changed "
			    "to PROBE \n", AF_INET6, &nce->nce_addr);
		}
		mutex_enter(&nce->nce_lock);
		mi_timer(ill->ill_rq, nce->nce_timer_mp,
		    ill->ill_reachable_retrans_time);
		mutex_exit(&nce->nce_lock);
		NCE_REFRELE(nce);
		return;
	case ND_PROBE:
		/* must be retransmit timer */
		nce->nce_pcnt--;
		ASSERT(nce->nce_pcnt < ND_MAX_UNICAST_SOLICIT &&
		    nce->nce_pcnt >= -1);
		if (nce->nce_pcnt == 0) {
			/* Wait RetransTimer, before deleting the entry */
			ip2dbg(("ndp_timer: pcount=%x dst  %s\n",
			    nce->nce_pcnt, inet_ntop(AF_INET6,
			    &nce->nce_addr, addrbuf, sizeof (addrbuf))));
			mi_timer(ill->ill_rq, nce->nce_timer_mp,
			    ill->ill_reachable_retrans_time);
			mutex_exit(&nce->nce_lock);
		} else {
			mutex_exit(&nce->nce_lock);
			nce_xmit(ill, ND_NEIGHBOR_SOLICIT, ill->ill_hw_addr,
			    &ipv6_all_zeros, &nce->nce_addr, NDP_UNICAST);
			mutex_enter(&nce->nce_lock);
			if (nce->nce_pcnt > 0) {
				ip2dbg(("ndp_timer: pcount=%x dst %s\n",
				    nce->nce_pcnt, inet_ntop(AF_INET6,
				    &nce->nce_addr,
				    addrbuf, sizeof (addrbuf))));
				mi_timer(ill->ill_rq, nce->nce_timer_mp,
				    ill->ill_reachable_retrans_time);
				mutex_exit(&nce->nce_lock);
			} else {
				/* No hope, delete the ire */
				nce->nce_state = ND_UNREACHABLE;
				mutex_exit(&nce->nce_lock);
				if (ip_debug > 2) {
					/* ip1dbg */
					pr_addr_dbg("ndp_timer: Delete IRE for"
					    " dst %s\n", AF_INET6,
					    &nce->nce_addr);
				}
				ndp_delete(nce);
			}
		}
		NCE_REFRELE(nce);
		return;
	case ND_INCOMPLETE:
		/*
		 * Must be resolvers retransmit timer.
		 */
		if (nce->nce_qd_mp != NULL) {
			ms = nce_solicit(nce, NULL);
			if (ms == 0) {
				mutex_exit(&nce->nce_lock);
				if (nce->nce_state != ND_REACHABLE) {
					nce_resolv_failed(nce);
					ndp_delete(nce);
				}
			} else {
				mi_timer(ill->ill_rq, nce->nce_timer_mp,
				    (clock_t)ms);
				mutex_exit(&nce->nce_lock);
			}
			NCE_REFRELE(nce);
			return;
		}
		/* FALLTHRU */
	default:
		mutex_exit(&nce->nce_lock);
		NCE_REFRELE(nce);
		break;
	}
}

/*
 * Set a link layer address from the ll_addr passed in.
 * Copy SAP from ill.
 */
static void
nce_set_ll(nce_t *nce, uchar_t *ll_addr)
{
	ill_t	*ill = nce->nce_ill;
	int 	abs_sap_length;
	uchar_t	*woffset;

	ASSERT(ll_addr != NULL);
	/* Always called before fast_path_probe */
	if (nce->nce_fp_mp != NULL)
		return;
	if (ill->ill_sap_length != 0) {
		/*
		 * Copy the SAP type specified in the
		 * request into the xmit template.
		 */
		abs_sap_length = ABS(ill->ill_sap_length);
		bcopy((char *)&ill->ill_sap + sizeof (ill->ill_sap) -
		    abs_sap_length,
		    nce->nce_res_mp->b_rptr + NCE_LL_SAP_OFFSET(ill),
		    abs_sap_length);
	}
	ASSERT(nce->nce_res_mp->b_wptr - nce->nce_res_mp->b_rptr >=
	    NCE_LL_ADDR_OFFSET(ill) + ill->ill_phys_addr_length);
	woffset = nce->nce_res_mp->b_rptr + NCE_LL_ADDR_OFFSET(ill);
	bcopy(ll_addr, woffset, ill->ill_phys_addr_length);
}

static boolean_t
nce_cmp_ll_addr(nce_t *nce, char *ll_addr, uint32_t ll_addr_len)
{
	ill_t	*ill = nce->nce_ill;
	uchar_t	*ll_offset;

	ASSERT(nce->nce_res_mp != NULL);
	if (ll_addr == NULL)
		return (B_FALSE);
	ll_offset = nce->nce_res_mp->b_rptr + NCE_LL_ADDR_OFFSET(ill);
	if (bcmp(ll_addr, (char *)ll_offset, ll_addr_len) != 0)
		return (B_TRUE);
	return (B_FALSE);
}

/*
 * Updates the link layer address or the reachability state of
 * a cache entry.  Reset probe counter if needed.
 */
static void
nce_update(nce_t *nce, uint16_t new_state, uchar_t *new_ll_addr)
{
	ill_t	*ill = nce->nce_ill;

	ASSERT(MUTEX_HELD(&nce->nce_lock));
	/*
	 * If this interface does not do NUD, there is no point
	 * in allowing an update to the cache entry.  Although
	 * we will respond to NS.
	 * The only time we accept an update for a resolver when
	 * NUD is turned off is when it has just been created.
	 * Non-Resolvers will always be created as REACHABLE.
	 */
	if (new_state != ND_UNCHANGED) {
		if ((nce->nce_flags & NCE_F_NONUD) &&
		    (nce->nce_state != ND_INCOMPLETE))
			return;
		ASSERT((int16_t)new_state >= ND_STATE_VALID_MIN);
		ASSERT((int16_t)new_state <= ND_STATE_VALID_MAX);
		mi_timer_stop(nce->nce_timer_mp);
		if (new_state == ND_REACHABLE)
			nce->nce_last = TICK_TO_MSEC(lbolt64);
		else {
			nce->nce_last = TICK_TO_MSEC(lbolt64) -
			    (uint64_t)ill->ill_reachable_time;
		}
		nce->nce_state = new_state;
		nce->nce_pcnt = ND_MAX_UNICAST_SOLICIT;
	}
	/*
	 * In case of fast path we need to free the the fastpath
	 * M_DATA and do another probe.  Otherwise we can just
	 * overwrite the DL_UNITDATA_REQ data, noting we'll lose
	 * whatever packets that happens to be transmitting at the time.
	 */
	if (new_ll_addr != NULL) {
		ASSERT(nce->nce_res_mp->b_rptr + NCE_LL_ADDR_OFFSET(ill) +
		    ill->ill_phys_addr_length <= nce->nce_res_mp->b_wptr);
		bcopy(new_ll_addr, nce->nce_res_mp->b_rptr +
		    NCE_LL_ADDR_OFFSET(ill), ill->ill_phys_addr_length);
		if (nce->nce_fp_mp != NULL) {
			freemsg(nce->nce_fp_mp);
			nce->nce_fp_mp = NULL;
			mutex_exit(&nce->nce_lock);
			nce_fastpath(nce);
			mutex_enter(&nce->nce_lock);
			return;
		}
	}
	ASSERT(MUTEX_HELD(&nce->nce_lock));
}

static void
nce_queue_mp(nce_t *nce, mblk_t *mp)
{
	uint_t	count = 0;
	mblk_t  **mpp;

	ASSERT(MUTEX_HELD(&nce->nce_lock));
	for (mpp = &nce->nce_qd_mp; *mpp != NULL;
	    mpp = &(*mpp)->b_next) {
		if (++count >
		    nce->nce_ill->ill_max_buf) {
			mblk_t *tmp = nce->nce_qd_mp->b_next;

			nce->nce_qd_mp->b_next = NULL;
			nce->nce_qd_mp->b_prev = NULL;
			freemsg(nce->nce_qd_mp);
			ip1dbg(("nce_queue_mp: pkt dropped\n"));
			nce->nce_qd_mp = tmp;
		}
	}
	/* put this on the list */
	*mpp = mp;
}

/*
 * Called when address resolution failed due to a timeout.
 * Send an ICMP unreachable in response to all queued packets.
 */
static void
nce_resolv_failed(nce_t *nce)
{
	mblk_t	*mp, *nxt_mp;
	char	buf[INET6_ADDRSTRLEN];
	ip6_t *ip6h;

	ip1dbg(("nce_resolv_failed: dst %s\n",
	    inet_ntop(AF_INET6, (char *)&nce->nce_addr, buf, sizeof (buf))));
	mutex_enter(&nce->nce_lock);
	mp = nce->nce_qd_mp;
	nce->nce_qd_mp = NULL;
	mutex_exit(&nce->nce_lock);
	while (mp != NULL) {
		nxt_mp = mp->b_next;
		mp->b_next = NULL;
		mp->b_prev = NULL;
		ip6h = (ip6_t *)mp->b_rptr;
		if (ip6h->ip6_nxt == IPPROTO_RAW) {
			ip6i_t *ip6i;
			/*
			 * This message should have been pulled up already
			 * in ip_wput_v6. ip_hdr_complete_v6 assumes that
			 * the header is pulled up.
			 */
			ip6i = (ip6i_t *)ip6h;
			ASSERT((mp->b_wptr - (uchar_t *)ip6i) >=
			    sizeof (ip6i_t) + IPV6_HDR_LEN);
			mp->b_rptr += sizeof (ip6i_t);
		}
		/*
		 * Ignore failure since icmp_unreachable_v6 will silently
		 * drop packets with an unspecified source address.
		 */
		(void) ip_hdr_complete_v6((ip6_t *)mp->b_rptr);
		icmp_unreachable_v6(nce->nce_ill->ill_wq, mp,
		    ICMP6_DST_UNREACH_ADDR, B_FALSE, B_FALSE);
		mp = nxt_mp;
	}
}

/*
 * Called by SIOCSNDP* ioctl to add/change an nce entry
 * and the corresponding attributes.
 * Disallow states other than ND_REACHABLE or ND_STALE.
 */
int
ndp_sioc_update(ill_t *ill, lif_nd_req_t *lnr)
{
	sin6_t		*sin6;
	in6_addr_t	*addr;
	nce_t		*nce;
	int		err;
	uint16_t	new_flags = 0;
	uint16_t	old_flags = 0;
	int		inflags = lnr->lnr_flags;

	if ((lnr->lnr_state_create != ND_REACHABLE) &&
	    (lnr->lnr_state_create != ND_STALE))
		return (EINVAL);

	sin6 = (sin6_t *)&lnr->lnr_addr;
	addr = &sin6->sin6_addr;

	mutex_enter(&ndp_g_lock);
	/* We know it can not be mapping so just look in the hash table */
	nce = nce_lookup_addr(ill, addr);
	if (nce != NULL)
		new_flags = nce->nce_flags;

	switch (inflags & (NDF_ISROUTER_ON|NDF_ISROUTER_OFF)) {
	case NDF_ISROUTER_ON:
		new_flags |= NCE_F_ISROUTER;
		break;
	case NDF_ISROUTER_OFF:
		new_flags &= ~NCE_F_ISROUTER;
		break;
	case (NDF_ISROUTER_OFF|NDF_ISROUTER_ON):
		mutex_exit(&ndp_g_lock);
		if (nce != NULL)
			NCE_REFRELE(nce);
		return (EINVAL);
	}

	switch (inflags & (NDF_ANYCAST_ON|NDF_ANYCAST_OFF)) {
	case NDF_ANYCAST_ON:
		new_flags |= NCE_F_ANYCAST;
		break;
	case NDF_ANYCAST_OFF:
		new_flags &= ~NCE_F_ANYCAST;
		break;
	case (NDF_ANYCAST_OFF|NDF_ANYCAST_ON):
		mutex_exit(&ndp_g_lock);
		if (nce != NULL)
			NCE_REFRELE(nce);
		return (EINVAL);
	}

	switch (inflags & (NDF_PROXY_ON|NDF_PROXY_OFF)) {
	case NDF_PROXY_ON:
		new_flags |= NCE_F_PROXY;
		break;
	case NDF_PROXY_OFF:
		new_flags &= ~NCE_F_PROXY;
		break;
	case (NDF_PROXY_OFF|NDF_PROXY_ON):
		mutex_exit(&ndp_g_lock);
		if (nce != NULL)
			NCE_REFRELE(nce);
		return (EINVAL);
	}

	if (nce == NULL) {
		err = ndp_add(ill,
		    (uchar_t *)lnr->lnr_hdw_addr,
		    addr,
		    &ipv6_all_ones,
		    &ipv6_all_zeros,
		    0,
		    new_flags,
		    lnr->lnr_state_create,
		    &nce);
		if (err != 0) {
			mutex_exit(&ndp_g_lock);
			ip1dbg(("ndp_sioc_update: Can't create NCE %d\n", err));
			return (err);
		}
	}
	old_flags = nce->nce_flags;
	if (old_flags & NCE_F_ISROUTER && !(new_flags & NCE_F_ISROUTER)) {
		/*
		 * Router turned to host, delete all ires.
		 * XXX Just delete the entry, but we need to add too.
		 */
		nce->nce_flags &= ~NCE_F_ISROUTER;
		mutex_exit(&ndp_g_lock);
		ndp_delete(nce);
		NCE_REFRELE(nce);
		return (0);
	}
	mutex_exit(&ndp_g_lock);

	mutex_enter(&nce->nce_lock);
	nce->nce_flags = new_flags;
	mutex_exit(&nce->nce_lock);
	/*
	 * Note that we ignore the state at this point, which
	 * should be either STALE or REACHABLE.  Instead we let
	 * the link layer address passed in to determine the state
	 * much like incoming packets.
	 */
	ndp_process(nce, (uchar_t *)lnr->lnr_hdw_addr, 0, B_FALSE);
	NCE_REFRELE(nce);
	return (0);
}

/*
 * If the device driver supports it, we make nce_fp_mp to have
 * an M_DATA prepend.  Otherwise nce_fp_mp will be null.
 * The caller insures there is hold on nce for this function.
 * Note that since ill_fastpath_probe() copies the mblk there is
 * no need for the hold beyond this function.
 */
static void
nce_fastpath(nce_t *nce)
{
	ill_t	*ill = nce->nce_ill;

	ASSERT(ill != NULL);
	if (nce->nce_fp_mp != NULL) {
		/* Already contains fastpath info */
		return;
	}
	if (nce->nce_res_mp != NULL)
		ill_fastpath_probe(ill, nce->nce_res_mp);
}

/*
 * Update all NCE's that are not in fastpath mode and
 * have an nce_fp_mp that matches mp. mp->b_cont contains
 * the fastpath header.
 */
void
ndp_fastpath_update(nce_t *nce, char  *arg)
{
	mblk_t 	*mp, *fp_mp;
	uchar_t	*mp_rptr, *ud_mp_rptr;
	mblk_t	*ud_mp = nce->nce_res_mp;
	ptrdiff_t	cmplen;

	if (nce->nce_flags & NCE_F_MAPPING)
		return;
	if ((nce->nce_fp_mp != NULL) || (ud_mp == NULL))
		return;

	ip2dbg(("ndp_fastpath_update: trying\n"));
	mp = (mblk_t *)arg;
	mp_rptr = mp->b_rptr;
	cmplen = mp->b_wptr - mp_rptr;
	ASSERT(cmplen >= 0);
	ud_mp_rptr = ud_mp->b_rptr;
	mutex_enter(&nce->nce_lock);
	if (ud_mp->b_wptr - ud_mp_rptr != cmplen ||
	    bcmp((char *)mp_rptr, (char *)ud_mp_rptr, cmplen) != 0) {
		mutex_exit(&nce->nce_lock);
		return;
	}
	/* Matched - install mp as the fastpath mp */
	ip1dbg(("ndp_fastpath_update: match\n"));
	fp_mp = dupb(mp->b_cont);
	if (fp_mp != NULL) {
		nce->nce_fp_mp = fp_mp;
	}
	mutex_exit(&nce->nce_lock);
}

/*
 * Return a pointer to a given option in the packet.
 * Assumes that option part of the packet have already been validated.
 */
nd_opt_hdr_t *
ndp_get_option(nd_opt_hdr_t *opt, int optlen, int opt_type)
{
	while (optlen > 0) {
		if (opt->nd_opt_type == opt_type)
			return (opt);
		optlen -= 8 * opt->nd_opt_len;
		opt = (struct nd_opt_hdr *)((char *)opt + 8 * opt->nd_opt_len);
	}
	return (NULL);
}

/*
 * Verify all option lengths present are > 0, also check to see
 * if the option lengths and packet length are consistent.
 */
boolean_t
ndp_verify_optlen(nd_opt_hdr_t *opt, int optlen)
{
	ASSERT(opt != NULL);
	while (optlen > 0) {
		if (opt->nd_opt_len == 0)
			return (B_FALSE);
		optlen -= 8 * opt->nd_opt_len;
		if (optlen < 0)
			return (B_FALSE);
		opt = (struct nd_opt_hdr *)((char *)opt + 8 * opt->nd_opt_len);
	}
	return (B_TRUE);
}

/*
 * ndp_walk function.
 * Free a fraction of the NCE cache entries.
 * A fraction of zero means to not free any in that category.
 */
void
ndp_cache_reclaim(nce_t *nce, char *arg)
{
	nce_cache_reclaim_t *ncr = (nce_cache_reclaim_t *)arg;
	uint_t	rand;

	if (nce->nce_flags & NCE_F_PERMANENT)
		return;

	rand = (uint_t)lbolt +
	    NCE_ADDR_HASH_V6(nce->nce_addr, NCE_TABLE_SIZE);
	if (ncr->ncr_host != 0 &&
	    (rand/ncr->ncr_host)*ncr->ncr_host == rand) {
		ndp_delete(nce);
		return;
	}
}

/*
 * ndp_walk function.
 * Count the number of NCEs that can be deleted.
 * These would be hosts but not routers.
 */
void
ndp_cache_count(nce_t *nce, char *arg)
{
	ncc_cache_count_t *ncc = (ncc_cache_count_t *)arg;

	if (nce->nce_flags & NCE_F_PERMANENT)
		return;

	ncc->ncc_total++;
	if (!(nce->nce_flags & NCE_F_ISROUTER))
		ncc->ncc_host++;
}
