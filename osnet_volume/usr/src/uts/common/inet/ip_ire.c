/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip_ire.c	1.71	99/10/18 SMI"

/*
 * This file contains routines that manipulate Internet Routing Entries (IREs).
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/dlpi.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/ip_ndp.h>
#include <inet/arp.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>

#include <net/pfkeyv2.h>
#include <inet/ipsec_info.h>
#include <inet/sadb.h>
#include <sys/kmem.h>

/*
 * Synchronization notes:
 *
 * The fields of the ire_t struct are protected in the following way :
 *
 * ire_next/ire_ptpn
 *
 *	- bucket lock of the respective tables (cache or forwarding tables).
 *
 * ire_fp_mp
 * ire_dlureq_mp
 *
 *	- ire_lock protects multiple threads updating ire_fp_mp
 *	  simultaneously. Otherwise no locks are used while accessing
 *	  (both read/write) both the fields.
 *
 * ire_mp, ire_rfq, ire_stq, ire_u *except* ire_gateway_addr[v6], ire_mask,
 * ire_type, ire_create_time, ire_masklen, ire_ipversion, ire_flags, ire_ipif,
 * ire_ihandle, ire_phandle, ire_nce, ire_bucket
 *
 *	- Set in ire_create_v4/v6 and never changes after that. Thus,
 *	  we don't need a lock whenever these fields are accessed.
 *
 *	- ire_bucket and ire_masklen (also set in ire_create) is set in
 *        ire_add_v4/ire_add_v6 before inserting in the bucket and never
 *        changes after that. Thus we don't need a lock whenever these
 *	  fields are accessed.
 *
 * ire_gateway_addr_v4[v6]
 *
 *	- ire_gateway_addr_v4[v6] is set during ire_create and later modified
 *	  by rts_setgwr[v6]. As ire_gateway_addr is a uint32_t, updates to
 *	  it assumed to be atomic and hence the other parts of the code
 *	  does not use any locks. ire_gateway_addr_v6 updates are not atomic
 *	  and hence any access to it uses ire_lock to get/set the right value.
 *
 * ire_ident, ire_refcnt
 *
 *	- Updated atomically using atomic_add_32
 *
 * ire_ssthresh, ire_rtt_sd, ire_rtt, ire_ib_pkt_count, ire_ob_pkt_count
 *
 *	- Assumes that 32 bit writes are atomic. No locks. ire_lock is
 *	  used to serialize updates to ire_ssthresh, ire_rtt_sd, ire_rtt.
 *
 * ire_max_frag, ire_frag_flag
 *
 *	- ire_lock is used to set/read both of them together.
 *
 * ire_tire_mark
 *
 *	- Set in ire_create and updated in ire_expire, which is called
 *	  by only one function namely ip_trash_timer_expire. Thus only
 *	  one function updates and examines the value.
 *
 * ire_marks
 *	- used only during deletes and bucket lock protects this.
 *
 * ire_ipsec_options_size/ire_ll_hdr_length
 *
 *	- Place holder for returning the information to the upper layers
 *	  when IRE_DB_REQ comes down.
 *
 * ip_ire_default_count protected by the bucket lock of
 * ip_forwarding_table[0][0].
 *
 * ipv6_ire_default_count is protected by the bucket lock of
 * ip_forwarding_table_v6[0][0].
 *
 * ip_ire_default_index/ipv6_ire_default_index is not protected as it
 * is just a hint at which default gateway to use. There is nothing
 * wrong in using the same gateway for two different connections.
 *
 * As we always hold the bucket locks in all the places while accessing
 * the above values, it is natural to use them for protecting them.
 *
 * We have a separate cache table and forwarding table for V4 and V6.
 * Cache table (ip_cache_table/ip_cache_table_v6) is a pointer to an
 * array of irb_t structure and forwarding table (ip_forwarding_table/
 * ip_forwarding_table_v6) is an array of pointers to array of irb_t
 * structure. ip_forwarding_table[_v6] is allocated dynamically in
 * ire_add_v4/v6. ire_ft_init_lock is used to serialize multiple threads
 * initializing the same bucket. Once a bucket is initialized, it is never
 * de-alloacted. This assumption enables us to access ip_forwarding_table[i]
 * or ip_forwarding_table_v6[i] without any locks.
 *
 * Each irb_t - ire bucket structure has a lock to protect
 * a bucket and the ires residing in the bucket have a back pointer to
 * the bucket structure. It also has a reference count for the number
 * of threads walking the bucket - irb_refcnt which is bumped up
 * using the macro IRB_REFHOLD macro. The flags irb_flags can be
 * set to IRE_MARK_CONDEMNED indicating that there are some ires
 * in this bucket that are marked with IRE_MARK_CONDEMNED and the
 * last thread to leave the bucket should delete the ires. Usually
 * this is done by the IRB_REFRELE macro which is used to decrement
 * the reference count on a bucket.
 *
 * IRE_REFHOLD/IRE_REFRELE macros operate on the ire which increments/
 * decrements the reference count, ire_refcnt, atomically on the ire.
 * ire_refcnt is modified only using this macro. Operations on the IRE
 * could be described as follows :
 *
 * CREATE an ire with reference count initialized to 1.
 *
 * ADDITION of an ire holds the bucket lock, checks for duplicates
 * and then adds the ire. ire_add_v4/ire_add_v6 returns the ire after
 * bumping up once more i.e the reference count is 2. This is to avoid
 * an extra lookup in the functions calling ire_add which wants to
 * work with the ire after adding.
 *
 * LOOKUP of an ire bumps up the reference count using IRE_REFHOLD
 * macro. It is valid to bump up the referece count of the IRE,
 * after the lookup has returned an ire. Following are the lookup
 * functions that return an HELD ire :
 *
 * ire_lookup_local[_v6], ire_ctable_lookup[_v6], ire_ftable_lookup[_v6],
 * ire_cache_lookup[_v6], ire_lookup_loop_multi[_v6], ire_route_lookup[_v6],
 * ipif_t_ire[_v6].
 *
 * DELETION of an ire holds the bucket lock, removes it from the list
 * and then decrements the reference count for having removed from the list
 * by using the IRE_REFRELE macro. If some other thread has looked up
 * the ire, the reference count would have been bumped up and hence
 * this ire will not be freed once deleted. It will be freed once the
 * reference count drops to zero.
 *
 * Add and Delete acquires the bucket lock as RW_WRITER, while all the
 * lookups acquire the bucket lock as RW_READER.
 *
 * NOTE : The only functions that does the IRE_REFRELE when an ire is
 *	  passed as an argument are :
 *
 *	  1) ip_wput_ire : This is because it IRE_REFHOLD/RELEs the
 *			   broadcast ires it looks up internally within
 *			   the function. Currently, for simplicity it does
 *			   not differentiate the one that is passed in and
 *			   the ones it looks up internally. It always
 *			   IRE_REFRELEs.
 *	  2) ire_send
 *	     ire_send_v6 : As ire_send calls ip_wput_ire and other functions
 *			   that take ire as an argument, it has to selectively
 *			   IRE_REFRELE the ire. To maintain symmetry,
 *			   ire_send_v6 does the same.
 *
 * Otherwise, the general rule is to do the IRE_REFRELE in the function
 * that is passing the ire as an argument.
 */

static irb_t *ip_forwarding_table[IP_MASK_TABLE_SIZE];
/* This is dynamically allocated in ip_ire_init */
static irb_t *ip_cache_table;
uint32_t	ire_handle = 1;
/*
 * ire_ft_init_lock is used while initializing ip_forwarding_table
 * dynamically in ire_add.
 */
kmutex_t	ire_ft_init_lock;
kmutex_t ire_handle_lock;	/* Protects ire_handle */
struct	kmem_cache	*ire_cache;

ire_stats_t ire_stats_v4;	/* V4 ire statistics */
ire_stats_t ire_stats_v6;	/* V6 ire statistics */

/* Zero iulp_t for initialization. */
const iulp_t	ire_uinfo_null = { 0 };

static ire_t	*ire_add_v4(ire_t *ire);
static void	ire_delete_v4(ire_t *ire);
static void	ire_report_ftable(ire_t *ire, char *mp);
static void	ire_report_ctable(ire_t *ire, char *mp);
static void	ire_walk_ipvers(pfv_t func, char *arg, uchar_t vers);
static void	ire_walk_tables(pfv_t func, char *arg,
		    size_t ftbl_sz, size_t htbl_sz, irb_t **ipftbl,
		    size_t ctbl_sz, irb_t *ipctbl);
static void	ire_walk_ill_ipvers(ill_t *ill, pfv_t func, char *arg,
		    uchar_t vers);
static	void	ire_walk_ill_tables(ill_t *ill, pfv_t func, char *arg,
		    size_t ftbl_sz, size_t htbl_sz, irb_t **ipftbl,
		    size_t ctbl_sz, irb_t *ipctbl);
static void	ire_delete_host_redirects(ipaddr_t gateway);
static boolean_t ire_match_args(ire_t *ire, ipaddr_t addr, ipaddr_t mask,
		    ipaddr_t gateway, int type, ipif_t *ipif, queue_t *wrq,
		    uint32_t ihandle, int match_flags);

/*
 * To avoid bloating the code, we call this function instead of
 * using the macro IRE_REFRELE. Use macro only in performance
 * critical paths.
 */
void
ire_refrele(ire_t *ire)
{
	IRE_REFRELE(ire);
}

/*
 * kmem_cache_alloc constructor for IRE in kma space.
 * Note that when ire_mp is set the IRE is stored in that mblk and
 * not in this cache.
 */
/* ARGSUSED */
int
ip_ire_constructor(void *buf, void *cdrarg, int kmflags)
{
	return (0);
}

/* ARGSUSED1 */
void
ip_ire_destructor(void *buf, void *cdrarg)
{
	ire_t	*ire = buf;

	ASSERT(ire->ire_fp_mp == NULL);
	ASSERT(ire->ire_dlureq_mp == NULL);
}

/*
 * Reclaim callback for ire cache. Called by kmem allocator.
 * To run in the IP's perimeter, we schedule it in the next
 * tick as a qtimeout(), by passing in zero ticks.
 */
/*ARGSUSED*/
void
ip_ire_reclaim(void *cdrarg)
{
	mutex_enter(&ip_mi_lock);
	if (ill_ire_gc != NULL)
		ip_ire_reclaim_id = qtimeout(ill_ire_gc->ill_rq,
		    ip_trash_ire_reclaim, ill_ire_gc->ill_rq, 0);
	mutex_exit(&ip_mi_lock);
}

/*
 * This function is associated with the IP_IOC_IRE_ADVISE_NO_REPLY
 * IOCTL.  It is used by TCP (or other ULPs) to supply revised information
 * for an existing CACHED IRE.
 */
/* ARGSUSED */
int
ip_ire_advise(queue_t *q, mblk_t *mp)
{
	uchar_t	*addr_ucp;
	ipic_t	*ipic;
	ire_t	*ire;

	ipic = (ipic_t *)mp->b_rptr;
	switch (ipic->ipic_addr_length) {
	case IP_ADDR_LEN: {
		ipaddr_t	addr;

		if (!(addr_ucp = mi_offset_param(mp, ipic->ipic_addr_offset,
		    ipic->ipic_addr_length)))
			return (EINVAL);
		if (!OK_32PTR(addr_ucp))
			return (EINVAL);
		/* Extract the destination address. */
		addr = *(ipaddr_t *)addr_ucp;
		/* Find the corresponding IRE. */
		ire = ire_cache_lookup(addr);
		break;
	}
	case IPV6_ADDR_LEN: {
		in6_addr_t	v6addr;

		if (!(addr_ucp = mi_offset_param(mp, ipic->ipic_addr_offset,
		    ipic->ipic_addr_length)))
			return (EINVAL);
		if (!OK_32PTR(addr_ucp))
			return (EINVAL);
		/* Extract the destination address. */
		v6addr = *(in6_addr_t *)addr_ucp;
		/* Find the corresponding IRE. */
		ire = ire_cache_lookup_v6(&v6addr);
		break;
	}
	default:
		return (EINVAL);
	}

	if (ire == NULL)
		return (ENOENT);
	/*
	 * Update the round trip time estimate and/or the max frag size
	 * and/or the slow start threshold.
	 *
	 * We serialize multiple advises using ire_lock.
	 */
	mutex_enter(&ire->ire_lock);
	if (ipic->ipic_rtt) {
		/*
		 * If there is no old cached values, initialize them
		 * conservatively.  Set them to be (1.5 * new value).
		 */
		if (ire->ire_uinfo.iulp_rtt != 0) {
			ire->ire_uinfo.iulp_rtt = (ire->ire_uinfo.iulp_rtt +
			    ipic->ipic_rtt) >> 1;
		} else {
			ire->ire_uinfo.iulp_rtt = ipic->ipic_rtt +
			    (ipic->ipic_rtt >> 1);
		}
		if (ire->ire_uinfo.iulp_rtt_sd != 0) {
			ire->ire_uinfo.iulp_rtt_sd =
			    (ire->ire_uinfo.iulp_rtt_sd +
			    ipic->ipic_rtt_sd) >> 1;
		} else {
			ire->ire_uinfo.iulp_rtt_sd = ipic->ipic_rtt_sd +
			    (ipic->ipic_rtt_sd >> 1);
		}
	}
	if (ipic->ipic_max_frag)
		ire->ire_max_frag = ipic->ipic_max_frag;
	if (ipic->ipic_ssthresh != 0) {
		if (ire->ire_uinfo.iulp_ssthresh != 0)
			ire->ire_uinfo.iulp_ssthresh =
			    (ipic->ipic_ssthresh +
			    ire->ire_uinfo.iulp_ssthresh) >> 1;
		else
			ire->ire_uinfo.iulp_ssthresh = ipic->ipic_ssthresh;
	}
	mutex_exit(&ire->ire_lock);
	ire_refrele(ire);
	return (0);
}

/*
 * This function is associated with the IP_IOC_IRE_DELETE[_NO_REPLY]
 * IOCTL[s].  The NO_REPLY form is used by TCP to delete a route IRE
 * for a host that is not responding.  This will force an attempt to
 * establish a new route, if available.  Management processes may want
 * to use the version that generates a reply.
 *
 * This function does not support IPv6 since Neighbor Unreachability Detection
 * means that negative advise like this is useless.
 */
/* ARGSUSED */
int
ip_ire_delete(queue_t *q, mblk_t *mp)
{
	uchar_t	*addr_ucp;
	ipaddr_t	addr;
	ire_t	*ire;
	ipid_t	*ipid;
	boolean_t routing_sock_info = B_FALSE;	/* Sent info? */

	ipid = (ipid_t *)mp->b_rptr;

	/* Only actions on IRE_CACHEs are acceptable at present. */
	if (ipid->ipid_ire_type != IRE_CACHE)
		return (EINVAL);

	addr_ucp = mi_offset_param(mp, ipid->ipid_addr_offset,
		ipid->ipid_addr_length);
	if (addr_ucp == NULL || !OK_32PTR(addr_ucp))
		return (EINVAL);
	switch (ipid->ipid_addr_length) {
	case IP_ADDR_LEN:
		/* addr_ucp points at IP addr */
		break;
	case sizeof (sin_t): {
		sin_t	*sin;
		/*
		 * got complete (sockaddr) address - increment addr_ucp to point
		 * at the ip_addr field.
		 */
		sin = (sin_t *)addr_ucp;
		addr_ucp = (uchar_t *)&sin->sin_addr.s_addr;
		break;
	}
	default:
		return (EINVAL);
	}
	/* Extract the destination address. */
	bcopy(addr_ucp, &addr, IP_ADDR_LEN);

	/* Try to find the CACHED IRE. */
	ire = ire_cache_lookup(addr);

	/* Nail it. */
	if (ire) {
		/* Allow delete only on CACHE entries */
		if (ire->ire_type != IRE_CACHE) {
			ire_refrele(ire);
			return (EINVAL);
		}

		/*
		 * Verify that the IRE has been around for a while.
		 * This is to protect against transport protocols
		 * that are too eager in sending delete messages.
		 */
		if (hrestime.tv_sec <
		    ire->ire_create_time + ip_ignore_delete_time) {
			ire_refrele(ire);
			return (EINVAL);
		}
		/*
		 * Now we have a potentially dead cache entry. We need
		 * to remove it.
		 * If this cache entry is generated from a default route,
		 * search the default list and mark it dead and some
		 * background process will try to activate it.
		 */
		if ((ire->ire_gateway_addr != 0) && (ire->ire_cmask == 0)) {
			/*
			 * Make sure that we pick a different
			 * IRE_DEFAULT next time.
			 * The ip_ire_default_count tracks the number of
			 * IRE_DEFAULT entries. However, the
			 * ip_forwarding_table[0] also contains
			 * interface routes thus the count can be zero.
			 */
			ire_t *gw_ire;
			irb_t *irb_ptr;
			irb_t *irb;

			if (((irb_ptr = ip_forwarding_table[0]) != NULL) &&
			    (irb = &irb_ptr[0])->irb_ire != NULL &&
			    ip_ire_default_count != 0) {
				uint_t index;

				/*
				 * We grab it as writer just to serialize
				 * multiple threads trying to bump up
				 * ip_ire_default_index.
				 */
				rw_enter(&irb->irb_lock, RW_WRITER);
				if ((gw_ire = irb->irb_ire) == NULL) {
					rw_exit(&irb->irb_lock);
					goto done;
				}
				index = ip_ire_default_index %
				    ip_ire_default_count;
				while (index-- && gw_ire->ire_next != NULL)
					gw_ire = gw_ire->ire_next;

				/* Skip past the potentially bad gateway */
				if (ire->ire_gateway_addr ==
				    gw_ire->ire_gateway_addr)
					ip_ire_default_index++;

				rw_exit(&irb->irb_lock);
		    }
		}
done:
		/* report the bad route to routing sockets */
		ip_rts_change(RTM_LOSING, ire->ire_addr, ire->ire_gateway_addr,
		    ire->ire_mask, ire->ire_src_addr, 0, 0, 0,
		    (RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFA));
		routing_sock_info = B_TRUE;
		ire_delete(ire);
		ire_refrele(ire);
	}
	/* Also look for an IRE_HOST_REDIRECT and remove it if present */
	ire = ire_route_lookup(addr, 0, 0, IRE_HOST_REDIRECT, NULL, NULL, NULL,
	    MATCH_IRE_TYPE);

	/* Nail it. */
	if (ire) {
		if (!routing_sock_info) {
			ip_rts_change(RTM_LOSING, ire->ire_addr,
			    ire->ire_gateway_addr, ire->ire_mask,
			    ire->ire_src_addr, 0, 0, 0,
			    (RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFA));
		}
		ire_delete(ire);
		ire_refrele(ire);
	}
	return (0);
}

/*
 * Named Dispatch routine to produce a formatted report on all IREs.
 * This report is accessed by using the ndd utility to "get" ND variable
 * "ipv4_ire_status".
 */
/* ARGSUSED */
int
ip_ire_report(queue_t *q, mblk_t *mp, caddr_t arg)
{
	(void) mi_mpprintf(mp,
	    "IRE      " MI_COL_HDRPAD_STR
	/*   01234567[89ABCDEF] */
	    "rfq      " MI_COL_HDRPAD_STR
	/*   01234567[89ABCDEF] */
	    "stq      " MI_COL_HDRPAD_STR
	/*   01234567[89ABCDEF] */
	    "addr            mask            "
	/*   123.123.123.123 123.123.123.123 */
	    "src             gateway         mxfrg rtt   rtt_sd ssthresh ref "
	/*   123.123.123.123 123.123.123.123 12345 12345 123456 12345678 123 */
	    "rtomax tstamp_ok wscale_ok ecn_ok pmtud_ok sack sendpipe "
	/*   123456 123456789 123456789 123456 12345678 1234 12345678 */
	    "recvpipe in/out/forward type");
	/*   12345678 in/out/forward xxxxxxxxxx */
	ire_walk_v4(ire_report_ftable, (char *)mp);
	ire_walk_v4(ire_report_ctable, (char *)mp);
	return (0);
}

/* ire_walk routine invoked for ip_ire_report for each IRE. */
static void
ire_report_ftable(ire_t *ire, char *mp)
{
	char	buf1[16];
	char	buf2[16];
	char	buf3[16];
	char	buf4[16];
	uint_t	fo_pkt_count;
	uint_t	ib_pkt_count;
	int	ref;

	if (ire->ire_type & IRE_CACHETABLE)
	    return;

	/* Number of active references of this ire */
	ref = ire->ire_refcnt;
	/* "inbound" to a non local address is a forward */
	ib_pkt_count = ire->ire_ib_pkt_count;
	fo_pkt_count = 0;
	if (!(ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))) {
		fo_pkt_count = ib_pkt_count;
		ib_pkt_count = 0;
	}
	(void) mi_mpprintf((mblk_t *)mp,
	    MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR
	    "%s %s %s %s %05d %05ld %06ld %08d %03d %06d %09d %09d %06d %08d "
	    "%04d %08d %08d %d/%d/%d %s",
	    (void *)ire, (void *)ire->ire_rfq, (void *)ire->ire_stq,
	    ip_dot_addr(ire->ire_addr, buf1), ip_dot_addr(ire->ire_mask, buf2),
	    ip_dot_addr(ire->ire_src_addr, buf3),
	    ip_dot_addr(ire->ire_gateway_addr, buf4),
	    ire->ire_max_frag, ire->ire_uinfo.iulp_rtt,
	    ire->ire_uinfo.iulp_rtt_sd,
	    ire->ire_uinfo.iulp_ssthresh, ref,
	    ire->ire_uinfo.iulp_rtomax,
	    (ire->ire_uinfo.iulp_tstamp_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_wscale_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_ecn_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_pmtud_ok ? 1: 0),
	    ire->ire_uinfo.iulp_sack,
	    ire->ire_uinfo.iulp_spipe, ire->ire_uinfo.iulp_rpipe,
	    ib_pkt_count, ire->ire_ob_pkt_count, fo_pkt_count,
	    ip_nv_lookup(ire_nv_tbl, (int)ire->ire_type));
}

/* ire_walk routine invoked for ip_ire_report for each cached IRE. */
static void
ire_report_ctable(ire_t *ire, char *mp)
{
	char	buf1[16];
	char	buf2[16];
	char	buf3[16];
	char	buf4[16];
	uint_t	fo_pkt_count;
	uint_t	ib_pkt_count;
	int	ref;

	if ((ire->ire_type & IRE_CACHETABLE) == 0)
	    return;
	/* Number of active references of this ire */
	ref = ire->ire_refcnt;
	/* "inbound" to a non local address is a forward */
	ib_pkt_count = ire->ire_ib_pkt_count;
	fo_pkt_count = 0;
	if (!(ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))) {
		fo_pkt_count = ib_pkt_count;
		ib_pkt_count = 0;
	}
	(void) mi_mpprintf((mblk_t *)mp,
	    MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR
	    "%s %s %s %s %05d %05ld %06ld %08d %03d %06d %09d %09d %06d %08d "
	    "%04d %08d %08d %d/%d/%d %s",
	    (void *)ire, (void *)ire->ire_rfq, (void *)ire->ire_stq,
	    ip_dot_addr(ire->ire_addr, buf1), ip_dot_addr(ire->ire_mask, buf2),
	    ip_dot_addr(ire->ire_src_addr, buf3),
	    ip_dot_addr(ire->ire_gateway_addr, buf4),
	    ire->ire_max_frag, ire->ire_uinfo.iulp_rtt,
	    ire->ire_uinfo.iulp_rtt_sd, ire->ire_uinfo.iulp_ssthresh, ref,
	    ire->ire_uinfo.iulp_rtomax,
	    (ire->ire_uinfo.iulp_tstamp_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_wscale_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_ecn_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_pmtud_ok ? 1: 0),
	    ire->ire_uinfo.iulp_sack,
	    ire->ire_uinfo.iulp_spipe, ire->ire_uinfo.iulp_rpipe,
	    ib_pkt_count, ire->ire_ob_pkt_count, fo_pkt_count,
	    ip_nv_lookup(ire_nv_tbl, (int)ire->ire_type));
}

/*
 * ip_ire_req is called by ip_wput when an IRE_DB_REQ_TYPE message is handed
 * down from the Upper Level Protocol to request a copy of the IRE (to check
 * its type or to extract information like round-trip time estimates or the
 * MTU.)
 * The address is assumed to be in the ire_addr field. If no IRE is found
 * an IRE is returned with ire_type being zero.
 * Note that the upper lavel protocol has to check for broadcast
 * (IRE_BROADCAST) and multicast (CLASSD(addr)).
 * If there is a b_cont the resulting IRE_DB_TYPE mblk is placed at the
 * end of the returned message.
 *
 * TCP sends down a message of this type with a connection request packet
 * chained on. UDP and ICMP send it down to verify that a route exists for
 * the destination address when they get connected.
 */
void
ip_ire_req(queue_t *q, mblk_t *mp)
{
	ire_t	*inire;
	ire_t	*ire;
	mblk_t	*mp1;
	ire_t	*sire = NULL;

	if ((mp->b_wptr - mp->b_rptr) < sizeof (ire_t) ||
	    !OK_32PTR(mp->b_rptr)) {
		freemsg(mp);
		return;
	}
	inire = (ire_t *)mp->b_rptr;
	/*
	 * Got it, now take our best shot at an IRE.
	 */
	if (inire->ire_ipversion == IPV6_VERSION) {
		ire = ire_route_lookup_v6(&inire->ire_addr_v6, 0, 0, 0,
		    NULL, &sire, NULL,
		    (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT));
	} else {
		ASSERT(inire->ire_ipversion == IPV4_VERSION);
		ire = ire_route_lookup(inire->ire_addr, 0, 0, 0, NULL, &sire,
		    NULL, (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT));
	}

	/*
	 * We prevent returning IRES with source address INADDR_ANY
	 * as these were temporarily created for sending packets
	 * from endpoints that have IPC_UNSPEC_SRC set.
	 */
	if (ire == NULL ||
	    (ire->ire_ipversion == IPV4_VERSION &&
	    ire->ire_src_addr == INADDR_ANY) ||
	    (ire->ire_ipversion == IPV6_VERSION &&
	    IN6_IS_ADDR_UNSPECIFIED(&ire->ire_src_addr_v6))) {
		inire->ire_type = 0;
	} else {
		bcopy(ire, inire, sizeof (ire_t));
		/* Copy the route metrics from the parent. */
		if (sire != NULL) {
			bcopy(&(sire->ire_uinfo), &(inire->ire_uinfo),
			    sizeof (iulp_t));
		}

		/*
		 * As we don't lookup global policy here, we may not
		 * pass the right size if per-socket policy is not
		 * present. For these cases, path mtu discovery will
		 * do the right thing.
		 */
		inire->ire_ipsec_options_size =
		    ipc_ipsec_length((ipc_t *)q->q_ptr);

		/* Pass the latest setting of the ip_path_mtu_discovery */
		inire->ire_frag_flag = (ip_path_mtu_discovery) ? IPH_DF : 0;
	}
	if (ire != NULL)
		ire_refrele(ire);
	if (sire != NULL)
		ire_refrele(sire);
	mp->b_wptr = &mp->b_rptr[sizeof (ire_t)];
	mp->b_datap->db_type = IRE_DB_TYPE;

	/* Put the IRE_DB_TYPE mblk last in the chain */
	mp1 = mp->b_cont;
	if (mp1 != NULL) {
		mp->b_cont = NULL;
		linkb(mp1, mp);
		mp = mp1;
	}
	qreply(q, mp);
}

/*
 * Add an IRE (IRE_DB_TYPE mblk) for the address to the packet.
 * If no ire is found add an IRE_DB_TYPE with ire_type = 0.
 * If there are allocation problems return without it being added.
 * Used for TCP SYN packets.
 */
void
ip_ire_append(mblk_t *mp, ipaddr_t addr)
{
	ire_t	*inire;
	ire_t	*ire;
	mblk_t	*mp1;
	ire_t	*sire = NULL;

	mp1 = allocb(sizeof (ire_t), BPRI_HI);
	if (mp1 == NULL)
		return;

	mp1->b_datap->db_type = IRE_DB_TYPE;
	mp1->b_wptr += sizeof (ire_t);
	inire = (ire_t *)mp1->b_rptr;

	/*
	 * Take our best shot at an IRE.
	 */
	ire = ire_route_lookup(addr, 0, 0, 0, NULL, &sire, NULL,
	    (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT));
	/*
	 * We prevent returning IRES with source address INADDR_ANY
	 * as these were temporarily created for sending packets
	 * from endpoints that have IPC_UNSPEC_SRC set.
	 */
	if (ire == NULL || ire->ire_src_addr == INADDR_ANY) {
		inire->ire_ipversion = IPV4_VERSION;
		inire->ire_type = 0;
	} else {
		bcopy(ire, inire, sizeof (ire_t));

		if (sire != NULL) {
			bcopy(&(sire->ire_uinfo), &(inire->ire_uinfo),
			    sizeof (iulp_t));
		}

		/*
		 * Pass the latest setting of the ip_path_mtu_discovery.
		 * The caller sets the ire_options_size as it has all the
		 * SA information of the incoming SYN.
		 */
		inire->ire_frag_flag = (ip_path_mtu_discovery) ? IPH_DF : 0;
		if (inire->ire_ipif != NULL) {
			ill_t	*ill = inire->ire_ipif->ipif_ill;

			if ((ill != NULL) &&
			    (ill->ill_ick.ick_magic == ICK_M_CTL_MAGIC))
				mp1->b_ick_flag = ICK_VALID;
			inire->ire_ipif = NULL;
		}
	}
	if (ire != NULL)
		IRE_REFRELE(ire);
	if (sire != NULL)
		IRE_REFRELE(sire);
	linkb(mp, mp1);
}

/*
 * Send a packet using the specified IRE.
 * If ire_src_addr_v6 is all zero then discard the IRE after
 * the packet has been sent.
 */
void
ire_send(queue_t *q, mblk_t *pkt, ire_t *ire)
{
	mblk_t *mp;
	mblk_t *ipsec_mp;
	boolean_t secure;

	ASSERT(ire->ire_ipversion == IPV4_VERSION);
	if (pkt->b_datap->db_type == M_CTL) {
		ipsec_mp = pkt;
		pkt = pkt->b_cont;
		secure = B_TRUE;
	} else {
		ipsec_mp = pkt;
		secure = B_FALSE;
	}

	/* If the packet originated externally then */
	if (pkt->b_prev) {
		ire_refrele(ire);
		q = (queue_t *)pkt->b_prev;
		pkt->b_prev = NULL;
		mp = allocb(0, BPRI_HI);
		if (mp == NULL) {
			pkt->b_next = NULL;
			freemsg(ipsec_mp);
			return;
		}
		mp->b_datap->db_type = M_BREAK;
		/*
		 * This packet has not gone through IPSEC processing
		 * and hence we should not have any IPSEC message
		 * prepended.
		 */
		ASSERT(ipsec_mp == pkt);
		mp->b_cont = ipsec_mp;
		put(q, mp);
	} else if (pkt->b_next) {
		/* Packets from multicast router */
		pkt->b_next = NULL;
		/*
		 * We never get the IPSEC_OUT while forwarding the
		 * packet for multicast router.
		 */
		ASSERT(ipsec_mp == pkt);
		ip_rput_forward(ire, (ipha_t *)pkt->b_rptr, ipsec_mp);
		ire_refrele(ire);
	} else {
		/* Locally originated packets */
		boolean_t inaddr_any = B_FALSE;
		ipha_t *ipha = (ipha_t *)pkt->b_rptr;

		/*
		 * We need to do an ire_delete below for which
		 * we need to make sure that the IRE will be
		 * around even after calling ip_wput_ire -
		 * which does ire_refrele. Otherwise somebody
		 * could potentially delete this ire and hence
		 * free this ire and we will be calling ire_delete
		 * on a freed ire below.
		 */
		if (ire->ire_src_addr == INADDR_ANY) {
			inaddr_any = B_TRUE;
			IRE_REFHOLD(ire);
		}
		/*
		 * If we were resolving a router we can not use the
		 * routers IRE for sending the packet (since it would
		 * violate the uniqness of the IP idents) thus we
		 * make another pass through ip_wput to create the IRE_CACHE
		 * for the destination.
		 */
		if (ipha->ipha_dst != ire->ire_addr) {
			ire_refrele(ire);	/* Held in ire_add */
			ip_wput(q, ipsec_mp);
		} else {
			if (secure) {
				ipsec_out_t *oi;
				oi = (ipsec_out_t *)ipsec_mp->b_rptr;
				if (oi->ipsec_out_proc_begin) {
					/*
					 * This is the case where
					 * ip_wput_ipsec_out could not find
					 * the IRE and recreated a new one.
					 * As ip_wput_ipsec_out does ire
					 * lookups, ire_refrele for the extra
					 * bump in ire_add.
					 */
					ire_refrele(ire);
					ip_wput_ipsec_out(q, ipsec_mp);
				} else {
					/*
					 * IRE_REFRELE will be done in
					 * ip_wput_ire.
					 */
					ip_wput_ire(q, ipsec_mp, ire, NULL);
				}
			} else {
				/*
				 * IRE_REFRELE will be done in ip_wput_ire.
				 */
				ip_wput_ire(q, ipsec_mp, ire, NULL);
			}
		}
		/*
		 * Special code to support sending a single packet with
		 * ipc_unspec_src using an IRE which has no source address.
		 * The IRE is deleted here after sending the packet to avoid
		 * having other code trip on it. But before we delete the
		 * ire, somebody could have looked up this ire.
		 * We prevent returning/using this IRE by the upper layers
		 * by making checks to NULL source address in other places
		 * like e.g ip_ire_append, ip_ire_req and ip_bind_connected.
		 * Though, this does not completely prevent other threads
		 * from using this ire, this should not cause any problems.
		 *
		 * NOTE : We use inaddr_any instead of using ire_src_addr
		 * because for the normal case i.e !inaddr_any, ire_refrele
		 * above could have potentially freed the ire.
		 */
		if (inaddr_any) {
			ip1dbg(("ire_send: delete IRE\n"));
			ire_delete(ire);
			ire_refrele(ire);	/* Held above */
		}
	}
}

/*
 * Send a packet using the specified IRE.
 * If ire_src_addr_v6 is all zero then discard the IRE after
 * the packet has been sent.
 */
void
ire_send_v6(queue_t *q, mblk_t *pkt, ire_t *ire)
{
	ASSERT(ire->ire_ipversion == IPV6_VERSION);

	/* If the packet originated externally then */
	if (pkt->b_prev) {
		/*
		 * Extract the queue from b_prev (set in ip_rput_data_v6)
		 * Unlike IPv4 there is no need for a prepended M_BREAK
		 * since ip_rput_data_v6 does not process options
		 * before finding an IRE.
		 */
		q = (queue_t *)pkt->b_prev;
		pkt->b_prev = NULL;
		put(q, pkt);
	} else if (pkt->b_next) {
		/* Packets from multicast router */
		pkt->b_next = NULL;
		/*
		 * XXX TODO IPv6.
		 */
		freemsg(pkt);
#ifdef XXX
		ip_rput_forward(ire, (ipha_t *)pkt->b_rptr, pkt);
#endif
	} else {
		/*
		 * Send packets through ip_wput_v6 so that any ip6_info header
		 * can be processed again.
		 */
		ip_wput_v6(q, pkt);
		/*
		 * Special code to support sending a single packet with
		 * ipc_unspec_src using an IRE which has no source address.
		 * The IRE is deleted here after sending the packet to avoid
		 * having other code trip on it. But before we delete the
		 * ire, somebody could have looked up this ire.
		 * We prevent returning/using this IRE by the upper layers
		 * by making checks to NULL source address in other places
		 * like e.g ip_ire_append_v6, ip_ire_req and
		 * ip_bind_connected_v6. Though, this does not completely
		 * prevent other threads from using this ire, this should
		 * not cause any problems.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ire->ire_src_addr_v6)) {
			ip1dbg(("ire_send_v6: delete IRE\n"));
			ire_delete(ire);
		}
	}
	ire_refrele(ire);	/* Held in ire_add */
}

/*
 * ire_add_then_send is called when a new IRE has been created in order to
 * route an outgoing packet.  Typically, it is called from ip_wput when
 * a response comes back down from a resolver.  We add the IRE, and then
 * run the packet through ip_wput or ip_rput, as appropriate.
 */
void
ire_add_then_send(queue_t *q, mblk_t *mp)
{
	mblk_t	*pkt;
	ire_t	*ire = (ire_t *)mp->b_rptr;

	/*
	 * We are handed a message chain of the form:
	 *	IRE_MBLK-->packet
	 * Unhook the packet from the IRE.
	 */
	pkt = mp->b_cont;
	mp->b_cont = NULL;
	ire = ire_add(ire);
	if (ire == NULL) {
		pkt->b_prev = NULL;
		pkt->b_next = NULL;
		freemsg(pkt);
		return;
	}
	if (pkt == NULL) {
		ire_refrele(ire);	/* Held in ire_add_v4/v6 */
		return;
	}
	if (ire->ire_ipversion == IPV6_VERSION) {
		ire_send_v6(q, pkt, ire);
	} else {
		ire_send(q, pkt, ire);
	}
}

/*
 * ire_create is called to allocate and initialize a new IRE.
 *
 * NOTE : This is called as writer sometimes though not required
 * by this function.
 */
ire_t *
ire_create(uchar_t *addr, uchar_t *mask, uchar_t *src_addr, uchar_t *gateway,
    uint_t max_frag, mblk_t *fp_mp, queue_t *rfq, queue_t *stq,
    ushort_t type, mblk_t *dlureq_mp, ipif_t *ipif, ipaddr_t cmask,
    uint32_t phandle, uint32_t ihandle, uint32_t flags, const iulp_t *ulp_info)
{
	static ire_t	ire_null;
	ire_t	*ire;
	mblk_t	*mp;

	if (fp_mp != NULL) {
		/*
		 * We can't dupb() here as multiple threads could be
		 * calling dupb on the same mp which is incorrect.
		 * First dupb() should be called only by one thread.
		 */
		fp_mp = copyb(fp_mp);
		if (fp_mp == NULL)
			return (NULL);
	}

	if (dlureq_mp != NULL) {
		/*
		 * We can't dupb() here as multiple threads could be
		 * calling dupb on the same mp which is incorrect.
		 * First dupb() should be called only by one thread.
		 */
		dlureq_mp = copyb(dlureq_mp);
		if (dlureq_mp == NULL) {
			if (fp_mp != NULL)
				freeb(fp_mp);
			return (NULL);
		}
	}

	/*
	 * Check that IRE_IF_RESOLVER and IRE_IF_NORESOLVER have a
	 * dlureq_mp which is the ill_resolver_mp for IRE_IF_RESOLVER
	 * and DL_UNITDATA_REQ for IRE_IF_NORESOLVER.
	 */
	if ((type & IRE_INTERFACE) &&
	    dlureq_mp == NULL) {
		ASSERT(fp_mp == NULL);
		ip0dbg(("ire_create: no dlureq_mp\n"));
		return (NULL);
	}

	/* Allocate the new IRE. */
	mp = allocb(sizeof (ire_t), BPRI_MED);
	if (mp == NULL) {
		if (fp_mp != NULL)
			freeb(fp_mp);
		if (dlureq_mp != NULL)
			freeb(dlureq_mp);
		return (NULL);
	}

	BUMP_IRE_STATS(ire_stats_v4, ire_stats_alloced);

	ire = (ire_t *)mp->b_rptr;
	mp->b_wptr = (uchar_t *)&ire[1];

	/* Start clean. */
	*ire = ire_null;

	ire->ire_mp = mp;
	mp->b_datap->db_type = IRE_DB_TYPE;

	bcopy(addr, &ire->ire_addr, IP_ADDR_LEN);
	if (src_addr)
		bcopy(src_addr, &ire->ire_src_addr, IP_ADDR_LEN);
	if (mask) {
		bcopy(mask, &ire->ire_mask, IP_ADDR_LEN);
		ire->ire_masklen = ip_mask_to_index(ire->ire_mask);
	}
	if (gateway) {
		bcopy(gateway, &ire->ire_gateway_addr, IP_ADDR_LEN);
	}
	if (type == IRE_CACHE)
		ire->ire_cmask = cmask;

	return (ire_create_common(ire, max_frag, fp_mp, rfq,
	    stq, type, dlureq_mp, ipif, phandle, ihandle, flags,
	    IPV4_VERSION, ulp_info));
}

/*
 * Common to IPv4 and IPv6
 */
ire_t *
ire_create_common(ire_t *ire, uint_t max_frag, mblk_t *fp_mp,
    queue_t *rfq, queue_t *stq, ushort_t type,
    mblk_t *dlureq_mp, ipif_t *ipif, uint32_t phandle, uint32_t ihandle,
    uint32_t flags, ushort_t ipversion, const iulp_t *ulp_info)
{
	ire->ire_max_frag = max_frag;
	ire->ire_frag_flag = (ip_path_mtu_discovery) ? IPH_DF : 0;

	ASSERT(fp_mp == NULL || fp_mp->b_datap->db_type == M_DATA);

	ire->ire_fp_mp = fp_mp;
	ire->ire_dlureq_mp = dlureq_mp;
	ire->ire_stq = stq;
	ire->ire_rfq = rfq;
	ire->ire_type = type;
	ire->ire_flags = RTF_UP | flags;
	ire->ire_ident = TICK_TO_MSEC(lbolt);
	bcopy(ulp_info, &ire->ire_uinfo, sizeof (iulp_t));

	ire->ire_tire_mark = ire->ire_ob_pkt_count + ire->ire_ib_pkt_count;
	ire->ire_create_time = (uint32_t)hrestime.tv_sec;

	/*
	 * If this IRE is an IRE_CACHE, inherit the handles from the
	 * parent IREs. For others in the forwarding table, assign appropriate
	 * new ones.
	 *
	 * The mutex protecting ire_handle is because ire_create is not always
	 * called as a writer.
	 */
	if (ire->ire_type & IRE_OFFSUBNET) {
		mutex_enter(&ire_handle_lock);
		ire->ire_phandle = (uint32_t)ire_handle++;
		mutex_exit(&ire_handle_lock);
	} else if (ire->ire_type & IRE_INTERFACE) {
		mutex_enter(&ire_handle_lock);
		ire->ire_ihandle = (uint32_t)ire_handle++;
		mutex_exit(&ire_handle_lock);
	} else if (ire->ire_type == IRE_CACHE) {
		ire->ire_phandle = phandle;
		ire->ire_ihandle = ihandle;
	}
	ire->ire_ipif = ipif;
	ire->ire_ipversion = ipversion;
	ire->ire_refcnt = 1;
	mutex_init(&ire->ire_lock, NULL, MUTEX_DEFAULT, NULL);
	return (ire);
}

/*
 * This routine is called repeatedly by ipif_up to create broadcast IREs.
 * It is passed a pointer to a slot in an IRE pointer array into which to
 * place the pointer to the new IRE, if indeed we create one.  If the
 * IRE corresponding to the address passed in would be a duplicate of an
 * existing one, we don't create the new one.  irep is incremented before
 * return only if we do create a new IRE.  (Always called as writer.)
 *
 * Note that with the "match_flags" parameter, we can match on either
 * a particular logical interface (MATCH_IRE_IPIF) or for all logical
 * interfaces for a given physical interface (MATCH_IRE_ILL).  Currently,
 * we only create broadcast ire's on a per physical interface basis. If
 * someone is going to be mucking with logical interfaces, it is important
 * to call "ipif_check_bcast_ires()" to make sure that any change to a
 * logical interface will not cause critical broadcast IRE's to be deleted.
 */
ire_t **
ire_create_bcast(ipif_t *ipif, ipaddr_t  addr, ire_t **irep, int match_flags)
{
	ire_t *ire;

	/*
	 * No broadcast IREs for the LOOPBACK interface
	 * or others such as point to point.
	 */
	if (!(ipif->ipif_flags & IFF_BROADCAST))
		return (irep);

	/* If this would be a duplicate, don't bother. */
	if ((ire = ire_ctable_lookup(addr, 0, IRE_BROADCAST, ipif, NULL,
	    match_flags)) != NULL) {
		ire_refrele(ire);
		return (irep);
	}

	if (!(ipif->ipif_flags & IFF_NOXMIT)) {
		*irep++ = ire_create(
		    (uchar_t *)&addr,			/* dest addr */
		    (uchar_t *)&ip_g_all_ones,		/* mask */
		    (uchar_t *)&ipif->ipif_src_addr,	/* source addr */
		    NULL,				/* no gateway */
		    ipif->ipif_mtu,			/* max frag */
		    NULL,				/* fast path header */
		    ipif->ipif_rq,			/* recv-from queue */
		    ipif->ipif_wq,			/* send-to queue */
		    IRE_BROADCAST,
		    ipif->ipif_bcast_mp,		/* xmit header */
		    ipif,
		    0,
		    0,
		    0,
		    0,
		    &ire_uinfo_null);
	}
	/*
	 * Create a loopback IRE for the broadcast address.
	 * Note: ire_add() will blow away duplicates thus there is no need
	 * to check for existing entries.
	 */
	*irep++ = ire_create(
		(uchar_t *)&addr,		 /* dest address */
		(uchar_t *)&ip_g_all_ones,	 /* mask */
		(uchar_t *)&ipif->ipif_src_addr, /* source address */
		NULL,				 /* no gateway */
		IP_LOOPBACK_MTU,		 /* max frag size */
		NULL,				 /* Fast Path header */
		ipif->ipif_rq,			 /* recv-from queue */
		NULL,				 /* no send-to queue */
		IRE_BROADCAST,			 /* Needed for fanout in wput */
		NULL,
		ipif,
		0,
		0,
		0,
		0,
		&ire_uinfo_null);
	return (irep);
}

/*
 * ire_walk routine to delete or update any IRE_CACHE that might contain
 * stale information.
 * The flags state which entries to delete or update.
 * Garbage collection is done separately using kmem alloc callbacks to
 * ip_trash_ire_reclaim.
 * Used for both IPv4 and IPv6. However, IPv6 only uses FLUSH_MTU_TIME
 * since other stale information is cleaned up using NUD.
 */
void
ire_expire(ire_t *ire, char *arg)
{
	int flush_flags = (int)arg;

	if ((flush_flags & FLUSH_REDIRECT_TIME) &&
	    ire->ire_type == IRE_HOST_REDIRECT) {
		/* Make sure we delete the corresponding IRE_CACHE */
		ip1dbg(("ire_expire: all redirects\n"));
		ire_delete(ire);
		return;
	}
	if (ire->ire_type != IRE_CACHE)
		return;

	if (flush_flags & FLUSH_ARP_TIME) {
		/*
		 * Remove all IRE_CACHE.
		 * Verify that create time is more than
		 * ip_ire_arp_interval milliseconds ago.
		 */
		if (((uint32_t)hrestime.tv_sec - ire->ire_create_time) *
		    MILLISEC > ip_ire_arp_interval) {
			ip1dbg(("ire_expire: all IRE_CACHE\n"));
			ire_delete(ire);
			return;
		}
	}

	if (ip_path_mtu_discovery && (flush_flags & FLUSH_MTU_TIME) &&
	    (ire->ire_ipif != NULL)) {
		/* Increase pmtu if it is less than the interface mtu */
		mutex_enter(&ire->ire_lock);
		ire->ire_max_frag = ire->ire_ipif->ipif_mtu;
		ire->ire_frag_flag = IPH_DF;
		mutex_exit(&ire->ire_lock);
	}
}

/*
 * Do fast path probing if necessary.
 */
static void
ire_fastpath(ire_t *ire)
{
	ill_t	*ill;

	if (ire->ire_fp_mp != NULL || ire->ire_dlureq_mp == NULL) {
		/*
		 * Already contains fastpath info or
		 * doesn't have DL_UNITDATA_REQ header
		 */
		return;
	}
	ill = ire_to_ill(ire);
	if (ill == NULL)
		return;
	ill_fastpath_probe(ill, ire->ire_dlureq_mp);
}

/*
 * Update all IRE's that are not in fastpath mode and
 * have an ll_dlureq_mp that matches mp. mp->b_cont contains
 * the fastpath header.
 */
void
ire_fastpath_update(ire_t *ire, char  *arg)
{
	mblk_t 	*mp, *fp_mp;
	uchar_t 	*up, *up2;
	ptrdiff_t	cmplen;

	if (!(ire->ire_type & (IRE_CACHE | IRE_BROADCAST)))
		return;

	/*
	 * Already contains fastpath info or doesn't have
	 * DL_UNITDATA_REQ header
	 */
	if (ire->ire_fp_mp != NULL || ire->ire_dlureq_mp == NULL)
		return;

	ip2dbg(("ire_fastpath_update: trying\n"));
	mp = (mblk_t *)arg;
	up = mp->b_rptr;
	cmplen = mp->b_wptr - up;
	/* Serialize multiple fast path updates */
	mutex_enter(&ire->ire_lock);
	up2 = ire->ire_dlureq_mp->b_rptr;
	ASSERT(cmplen >= 0);
	if (ire->ire_dlureq_mp->b_wptr - up2 != cmplen ||
	    bcmp(up, up2, cmplen) != 0) {
		mutex_exit(&ire->ire_lock);
		return;
	}
	/* Matched - install mp as the ire_fp_mp */
	ip1dbg(("ire_fastpath_update: match\n"));
	fp_mp = dupb(mp->b_cont);
	if (fp_mp) {
		/*
		 * We checked ire_fp_mp above. Check it again with the
		 * lock. Update fp_mp only if it has not been done
		 * already.
		 */
		if (ire->ire_fp_mp == NULL) {
			/*
			 * ire_ll_hdr_length is just an optimization to
			 * store the length. It is used to return the
			 * fast path header length to the upper layers.
			 */
			ire->ire_fp_mp = fp_mp;
			ire->ire_ll_hdr_length =
			    (uint_t)(fp_mp->b_wptr - fp_mp->b_rptr);
		} else {
			freeb(fp_mp);
		}
	}
	mutex_exit(&ire->ire_lock);
}

/*
 * ire_lookup_loop_multi: Find an IRE_INTERFACE for the group.
 * Allows different routes for multicast addresses
 * in the unicast routing table (akin to 224.0.0.0 but could be more specific)
 * which point at different interfaces. This is used when IP_MULTICAST_IF
 * isn't specified (when sending) and when IP_ADD_MEMBERSHIP doesn't
 * specify the interface to join on.
 *
 * Supports IP_BOUND_IF by following the ipif/ill when recursing.
 */
ire_t *
ire_lookup_loop_multi(ipaddr_t group)
{
	ire_t	*ire;
	ipif_t	*ipif = NULL;
	int	match_flags = MATCH_IRE_TYPE;
	ipaddr_t gw_addr;

	ire = ire_ftable_lookup(group, 0, 0, 0, NULL, NULL, NULL, 0,
		MATCH_IRE_DEFAULT);
	while (ire) {
		/* Make sure we follow ire_ipif */
		if (ire->ire_ipif != NULL) {
			ipif = ire->ire_ipif;
			match_flags |= MATCH_IRE_ILL;
		} else {
			ipif = NULL;
			match_flags &= ~MATCH_IRE_ILL;
		}
		switch (ire->ire_type) {
		case IRE_DEFAULT:
		case IRE_PREFIX:
		case IRE_HOST:
			gw_addr = ire->ire_gateway_addr;
			ire_refrele(ire);
			ire = ire_ftable_lookup(gw_addr, 0, 0,
			    IRE_INTERFACE, ipif, NULL, NULL, 0, match_flags);
			break;
		case IRE_IF_NORESOLVER:
		case IRE_IF_RESOLVER:
			return (ire);
		default:
			ire_refrele(ire);
			return (NULL);
		}
	}
	return (NULL);
}

/*
 * Return any local address.  We use this to target ourselves
 * when the src address was specified as 'default'.
 * Preference for IRE_LOCAL entries.
 */
ire_t *
ire_lookup_local(void)
{
	ire_t	*ire;
	irb_t	*irb;
	ire_t	*maybe = NULL;
	int i;

	for (i = 0; i < IP_CACHE_TABLE_SIZE;  i++) {
		irb = &ip_cache_table[i];
		if (irb->irb_ire == NULL)
			continue;
		rw_enter(&irb->irb_lock, RW_READER);
		for (ire = irb->irb_ire; ire != NULL; ire = ire->ire_next) {
			if (ire->ire_marks & IRE_MARK_CONDEMNED)
				continue;
			switch (ire->ire_type) {
			case IRE_LOOPBACK:
				if (maybe == NULL) {
					IRE_REFHOLD(ire);
					maybe = ire;
				}
				break;
			case IRE_LOCAL:
				if (maybe != NULL) {
					ire_refrele(maybe);
				}
				IRE_REFHOLD(ire);
				rw_exit(&irb->irb_lock);
				return (ire);
			}
		}
		rw_exit(&irb->irb_lock);
	}
	return (maybe);
}

/* ire_walk routine to sum all the packets for IREs that match */
void
ire_pkt_count(ire_t *ire, char *ippc_arg)
{
	ippc_t	*ippc = (ippc_t *)ippc_arg;

	if (ire->ire_src_addr == ippc->ippc_addr) {
		/* "inbound" to a non local address is a forward */
		if (ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))
			ippc->ippc_ib_pkt_count += ire->ire_ib_pkt_count;
		else
			ippc->ippc_fo_pkt_count += ire->ire_ib_pkt_count;
		ippc->ippc_ob_pkt_count += ire->ire_ob_pkt_count;
	}
}

/*
 * If the specified IRE is associated with a particular ILL, return
 * that ILL pointer (May be called as writer.).
 */
ill_t *
ire_to_ill(ire_t *ire)
{
	if (ire != NULL && ire->ire_ipif != NULL)
		return (ire->ire_ipif->ipif_ill);
	return (NULL);
}

/* Arrange to call the specified function for every IRE in the world. */
void
ire_walk(pfv_t func, char *arg)
{
	ire_walk_ipvers(func, arg, 0);
}

void
ire_walk_v4(pfv_t func, char *arg)
{
	ire_walk_ipvers(func, arg, IPV4_VERSION);
}

void
ire_walk_v6(pfv_t func, char *arg)
{
	ire_walk_ipvers(func, arg, IPV6_VERSION);
}

/*
 * Walk a particular version. version == 0 means both v4 and v6.
 */
static void
ire_walk_ipvers(pfv_t func, char *arg, uchar_t vers)
{
	if (vers != IPV6_VERSION) {
		ire_walk_tables(func, arg, IP_MASK_TABLE_SIZE,
		    IP_FTABLE_HASH_SIZE, ip_forwarding_table,
		    IP_CACHE_TABLE_SIZE, ip_cache_table);
	}
	if (vers != IPV4_VERSION) {
		ire_walk_tables(func, arg, IP6_MASK_TABLE_SIZE,
		    IP6_FTABLE_HASH_SIZE, ip_forwarding_table_v6,
		    IP6_CACHE_TABLE_SIZE, ip_cache_table_v6);
	}
}

/*
 * Walk the ftable and the ctable.
 */
static void
ire_walk_tables(pfv_t func, char *arg,
    size_t ftbl_sz, size_t htbl_sz, irb_t **ipftbl,
    size_t ctbl_sz, irb_t *ipctbl)
{
	irb_t	*irb_ptr;
	irb_t	*irb;
	ire_t	*ire;
	int i, j;

	for (i = (ftbl_sz - 1);  i >= 0; i--) {
		if ((irb_ptr = ipftbl[i]) == NULL)
			continue;
		for (j = 0; j < htbl_sz; j++) {
			irb = &irb_ptr[j];
			if (irb->irb_ire == NULL)
				continue;
			IRB_REFHOLD(irb);
			for (ire = irb->irb_ire; ire != NULL;
			    ire = ire->ire_next) {
				(*func)(ire, arg);
			}
			IRB_REFRELE(irb);
		}
	}

	for (i = 0; i < ctbl_sz;  i++) {
		irb = &ipctbl[i];
		if (irb->irb_ire == NULL)
			continue;
		IRB_REFHOLD(irb);
		for (ire = irb->irb_ire; ire != NULL; ire = ire->ire_next) {
			(*func)(ire, arg);
		}
		IRB_REFRELE(irb);
	}
}

/*
 * Arrange to call the specified
 * function for every IRE that matches the ill.
 */
void
ire_walk_ill(ill_t *ill, pfv_t func, char *arg)
{
	ire_walk_ill_ipvers(ill, func, arg, 0);
}

void
ire_walk_ill_v4(ill_t *ill, pfv_t func, char *arg)
{
	ire_walk_ill_ipvers(ill, func, arg, IPV4_VERSION);
}

void
ire_walk_ill_v6(ill_t *ill, pfv_t func, char *arg)
{
	ire_walk_ill_ipvers(ill, func, arg, IPV6_VERSION);
}

/*
 * Walk a particular ill and version. version == 0 means both v4 and v6.
 */
static void
ire_walk_ill_ipvers(ill_t *ill, pfv_t func, char *arg, uchar_t vers)
{
	if (vers != IPV6_VERSION) {
		ire_walk_ill_tables(ill, func, arg, IP_MASK_TABLE_SIZE,
		    IP_FTABLE_HASH_SIZE, ip_forwarding_table,
		    IP_CACHE_TABLE_SIZE, ip_cache_table);
	}
	if (vers != IPV4_VERSION) {
		ire_walk_ill_tables(ill, func, arg, IP6_MASK_TABLE_SIZE,
		    IP6_FTABLE_HASH_SIZE, ip_forwarding_table_v6,
		    IP6_CACHE_TABLE_SIZE, ip_cache_table_v6);
	}
}

/*
 * Walk the ftable and the ctable entries that match the ill.
 */
static void
ire_walk_ill_tables(ill_t *ill, pfv_t func, char *arg,
    size_t ftbl_sz, size_t htbl_sz, irb_t **ipftbl,
    size_t ctbl_sz, irb_t *ipctbl)
{
	irb_t	*irb_ptr;
	irb_t	*irb;
	ire_t	*ire;
	int i, j;

	for (i = (ftbl_sz - 1);  i >= 0; i--) {
		if ((irb_ptr = ipftbl[i]) == NULL)
			continue;
		for (j = 0; j < htbl_sz; j++) {
			irb = &irb_ptr[j];
			if (irb->irb_ire == NULL)
				continue;
			IRB_REFHOLD(irb);
			for (ire = irb->irb_ire; ire != NULL;
			    ire = ire->ire_next) {
				if (ire->ire_ipif != NULL &&
				    ire->ire_ipif->ipif_ill == ill) {
					(*func)(ire, arg);
				}
			}
			IRB_REFRELE(irb);
		}
	}

	for (i = 0; i < ctbl_sz;  i++) {
		irb = &ipctbl[i];
		if (irb->irb_ire == NULL)
			continue;
		IRB_REFHOLD(irb);
		for (ire = irb->irb_ire; ire != NULL; ire = ire->ire_next) {
			if (ire->ire_ipif != NULL &&
			    ire->ire_ipif->ipif_ill == ill) {
				(*func)(ire, arg);
			}
		}
		IRB_REFRELE(irb);
	}
}

/*
 * This function takes a mask and returns
 * number of bits set in the mask. If no
 * bit is set it returns 0.
 * Assumes a contigious mask.
 */
int
ip_mask_to_index(ipaddr_t mask)
{
	int i;

	mask = ntohl(mask);
	/* Search for the first one from the right */
	for (i = 0; i < IP_ABITS; i++) {
	    if (mask & (1 << i))
		return (IP_ABITS - i);
	}
	return (0);
}

/*
 * Convert length for a mask to the mask.
 */
ipaddr_t
ip_index_to_mask(uint_t masklen)
{
	return (htonl(IP_HOST_MASK << (IP_ABITS - masklen)));
}

/*
 * Add a fully initialized IRE to an appropriate
 * table based on ire_type.
 */
ire_t *
ire_add(ire_t *ire)
{
	ire_t	*ire1;

	/* get ready for the day when original ire is not created as mblk */
	if (ire->ire_mp != NULL) {
		/* Copy the ire to a kmem_alloc'ed area */
		ire1 = kmem_cache_alloc(ire_cache, KM_NOSLEEP);
		if (ire1 == NULL) {
			ip1dbg(("ire_add: alloc failed\n"));
			ire_delete(ire);
			return (NULL);
		}
		*ire1 = *ire;
		ire1->ire_mp = NULL;
		freeb(ire->ire_mp);
		ire = ire1;
	}

	if (ire->ire_ipversion == IPV6_VERSION)
		return (ire_add_v6(ire));
	else
		return (ire_add_v4(ire));
}

/*
 * Add a fully initialized IRE to an appropriate
 * table based on ire_type.
 *
 * The forward table contains IRE_PREFIX/IRE_HOST/IRE_HOST_REDIRECT
 * IRE_IF_RESOLVER/IRE_IF_NORESOLVER and IRE_DEFAULT.
 *
 * The cache table contains IRE_BROADCAST/IRE_LOCAL/IRE_LOOPBACK
 * and IRE_CACHE.
 *
 * NOTE : This function is called as writer though not required
 * by this function.
 */
static ire_t *
ire_add_v4(ire_t *ire)
{
	ire_t	*ire1;
	int	mask_table_index;
	irb_t	*irb_ptr;
	ire_t	**irep;
	int	flags;
	ire_t	*pire = NULL;

	ASSERT(ire->ire_ipversion == IPV4_VERSION);
	ASSERT(ire->ire_mp == NULL); /* Calls should go through ire_add */

	/* Find the appropriate list head. */
	switch (ire->ire_type) {
	case IRE_HOST:
	case IRE_HOST_REDIRECT:
		ire->ire_mask = IP_HOST_MASK;
		ire->ire_masklen = IP_ABITS;
		ire->ire_src_addr = 0;
		break;
	case IRE_CACHE:
	case IRE_BROADCAST:
	case IRE_LOCAL:
	case IRE_LOOPBACK:
		ire->ire_mask = IP_HOST_MASK;
		ire->ire_masklen = IP_ABITS;
		break;
	case IRE_PREFIX:
		ire->ire_src_addr = 0;
		break;
	case IRE_DEFAULT:
		ire->ire_src_addr = 0;
		break;
	case IRE_IF_RESOLVER:
	case IRE_IF_NORESOLVER:
		break;
	default:
		printf("ire_add_v4: ire %p has unrecognized IRE type (%d)\n",
		    (void *)ire, ire->ire_type);
		ire_delete(ire);
		return (NULL);
	}

	/* Make sure the address is properly masked. */
	ire->ire_addr &= ire->ire_mask;

	if ((ire->ire_type & IRE_CACHETABLE) == 0) {
		/* IRE goes into Forward Table */
		mask_table_index = ire->ire_masklen;
		if ((ip_forwarding_table[mask_table_index]) == NULL) {
			irb_t *ptr;
			int i;

			ptr = (irb_t *)mi_zalloc((IP_FTABLE_HASH_SIZE *
			    sizeof (irb_t)));
			if (ptr == NULL) {
				ire_delete(ire);
				return (NULL);
			}
			for (i = 0; i < IP_FTABLE_HASH_SIZE; i++) {
				rw_init(&ptr[i].irb_lock, NULL,
				    RW_DEFAULT, NULL);
			}
			mutex_enter(&ire_ft_init_lock);
			if (ip_forwarding_table[mask_table_index] == NULL) {
				ip_forwarding_table[mask_table_index] = ptr;
				mutex_exit(&ire_ft_init_lock);
			} else {
				/*
				 * Some other thread won the race in
				 * initializing the forwarding table at the
				 * same index.
				 */
				mutex_exit(&ire_ft_init_lock);
				for (i = 0; i < IP_FTABLE_HASH_SIZE; i++) {
					rw_destroy(&ptr[i].irb_lock);
				}
				mi_free(ptr);
			}
		}
		irb_ptr = &(ip_forwarding_table[mask_table_index][
		    IRE_ADDR_HASH(ire->ire_addr, IP_FTABLE_HASH_SIZE)]);
	} else {
		irb_ptr = &(ip_cache_table[IRE_ADDR_HASH(ire->ire_addr,
		    IP_CACHE_TABLE_SIZE)]);
	}
	/*
	 * ip_newroute/ip_newroute_ipif are unable to prevent the deletion
	 * of the interface route while adding an IRE_CACHE for an on-link
	 * destination in the IRE_IF_RESOLVER case, since the ire has to
	 * go to ARP and return. We can't do a REFHOLD on the
	 * associated interface ire for fear of ARP freeing the message.
	 * Here we look up the interface ire in the forwarding table and
	 * make sure that the interface route has not been deleted.
	 */
	if (ire->ire_type == IRE_CACHE && ire->ire_gateway_addr == 0 &&
	    ire->ire_ipif->ipif_ill->ill_net_type == IRE_IF_RESOLVER) {
		if (CLASSD(ire->ire_addr)) {
			pire = ipif_to_ire(ire->ire_ipif);
			if (pire == NULL) {
				ire_delete(ire);
				return (NULL);
			} else if (pire->ire_ihandle != ire->ire_ihandle) {
				ire_refrele(pire);
				ire_delete(ire);
				return (NULL);
			}
		} else {
			pire = ire_ihandle_lookup_onlink(ire);
			if (pire == NULL) {
				ire_delete(ire);
				return (NULL);
			}
		}
		/* Prevent pire from getting deleted */
		IRB_REFHOLD(pire->ire_bucket);
		/* Has it been removed already ? */
		if (pire->ire_marks & IRE_MARK_CONDEMNED) {
			IRB_REFRELE(pire->ire_bucket);
			ire_refrele(pire);
			ire_delete(ire);
			return (NULL);
		}
	}

	flags = (MATCH_IRE_MASK | MATCH_IRE_TYPE | MATCH_IRE_GW);
	if (ire->ire_ipif != NULL)
		flags |= (MATCH_IRE_IPIF | MATCH_IRE_WQ);

	/*
	 * Atomically check for duplicate and insert in the table.
	 */
	rw_enter(&irb_ptr->irb_lock, RW_WRITER);
	for (ire1 = irb_ptr->irb_ire; ire1 != NULL; ire1 = ire1->ire_next) {
		if (ire1->ire_marks & IRE_MARK_CONDEMNED)
			continue;
		if (ire_match_args(ire1, ire->ire_addr, ire->ire_mask,
		    ire->ire_gateway_addr, ire->ire_type, ire->ire_ipif,
		    ire->ire_stq, 0, flags)) {
			/*
			 * Return the old ire after doing a REFHOLD.
			 * As most of the callers continue to use the IRE
			 * after adding, we return a held ire. This will
			 * avoid a lookup in the caller again. If the callers
			 * don't want to use it, they need to do a REFRELE.
			 */
			IRE_REFHOLD(ire1);
			rw_exit(&irb_ptr->irb_lock);
			ire_delete(ire);
			if (pire != NULL) {
				/*
				 * Assert that it is not removed from the
				 * list yet.
				 */
				ASSERT(pire->ire_ptpn != NULL);
				IRB_REFRELE(pire->ire_bucket);
				ire_refrele(pire);
			}
			return (ire1);
		}
	}

	/*
	 * Make it easy for ip_wput_ire() to hit multiple targets by grouping
	 * identical addresses together on the hash chain. Find the first entry
	 * that matches ire_addr. *irep will be null if no match.
	 */
	irep = (ire_t **)irb_ptr;
	while ((ire1 = *irep) != NULL && ire->ire_addr != ire1->ire_addr)
		irep = &ire1->ire_next;
	if (*irep != NULL) {
		/*
		 * Find the last ire which matches ire_addr.
		 * Needed to do tail insertion among entries with the same
		 * ire_addr.
		 */
		while (ire->ire_addr == ire1->ire_addr) {
			irep = &ire1->ire_next;
			ire1 = *irep;
			if (ire1 == NULL)
				break;
		}
	}

	if (ire->ire_type == IRE_DEFAULT) {
		/*
		 * We keep a count of default gateways which is used when
		 * assigning them as routes.
		 */
		ip_ire_default_count++;
		ASSERT(ip_ire_default_count != 0); /* Wraparound */
	}
	/* Insert at *irep */
	ire1 = *irep;
	if (ire1 != NULL)
		ire1->ire_ptpn = &ire->ire_next;
	ire->ire_next = ire1;
	/* Link the new one in. */
	ire->ire_ptpn = irep;

	/*
	 * ire_walk routines de-reference ire_next without holding
	 * a lock. Before we point to the new ire, we want to make
	 * sure the store that sets the ire_next of the new ire
	 * reaches global visibility, so that ire_walk routines
	 * don't see a truncated list of ires i.e if the ire_next
	 * of the new ire gets set after we do "*irep = ire" due
	 * to re-ordering, the ire_walk thread will see a NULL
	 * once it accesses the ire_next of the new ire.
	 * membar_producer() makes sure that the following store
	 * happens *after* all of the above stores.
	 */
	membar_producer();
	*irep = ire;
	ire->ire_bucket = irb_ptr;
	/*
	 * We return a bumped up IRE above. Keep it symmetrical
	 * so that the callers will always have to release. This
	 * helps the callers of this function because they continue
	 * to use the IRE after adding and hence they don't have to
	 * lookup again after we return the IRE.
	 *
	 * NOTE : We don't have to use atomics as this is appearing
	 * in the list for the first time and no one else can bump
	 * up the reference count on this yet.
	 */
	ire->ire_refcnt++;
	BUMP_IRE_STATS(ire_stats_v4, ire_stats_inserted);
	rw_exit(&irb_ptr->irb_lock);

	if (pire != NULL) {
		/* Assert that it is not removed from the list yet */
		ASSERT(pire->ire_ptpn != NULL);
		IRB_REFRELE(pire->ire_bucket);
		ire_refrele(pire);
	}

	if (ire->ire_type != IRE_CACHE)
		ire_flush_cache_v4(ire, IRE_FLUSH_ADD);
	/*
	 * We had to delay the fast path probe until the ire is inserted
	 * in the list. Otherwise the fast path ack won't find the ire in
	 * the table.
	 */
	if (ire->ire_type == IRE_CACHE || ire->ire_type == IRE_BROADCAST)
		ire_fastpath(ire);
	return (ire);
}

/*
 * Search for all HOST REDIRECT routes that are
 * pointing at the specified gateway and
 * delete them. This routine is called only
 * when a default gateway is going away.
 */
static void
ire_delete_host_redirects(ipaddr_t gateway)
{
	irb_t *irb_ptr;
	irb_t *irb;
	ire_t *ire;
	int i;

	/* get the hash table for HOST routes */
	irb_ptr = ip_forwarding_table[(IP_MASK_TABLE_SIZE - 1)];
	if (irb_ptr == NULL)
		return;
	for (i = 0; (i < IP_FTABLE_HASH_SIZE); i++) {
		irb = &irb_ptr[i];
		IRB_REFHOLD(irb);
		for (ire = irb->irb_ire; ire != NULL; ire = ire->ire_next) {
			if (ire->ire_type != IRE_HOST_REDIRECT)
				continue;
			if (ire->ire_gateway_addr == gateway) {
				ire_delete(ire);
			}
		}
		IRB_REFRELE(irb);
	}
}

/*
 * IRB_REFRELE is the only caller of the function. ire_unlink calls to
 * do the final cleanup for this ire.
 */
void
ire_cleanup(ire_t *ire)
{
	ire_t *ire_next;

	ASSERT(ire != NULL);

	while (ire != NULL) {
		ire_next = ire->ire_next;
		if (ire->ire_ipversion == IPV4_VERSION) {
			ire_delete_v4(ire);
			BUMP_IRE_STATS(ire_stats_v4, ire_stats_deleted);
		} else {
			ASSERT(ire->ire_ipversion == IPV6_VERSION);
			ire_delete_v6(ire);
			BUMP_IRE_STATS(ire_stats_v6, ire_stats_deleted);
		}
		/*
		 * Now it's really out of the list. Before doing the
		 * REFRELE, set ire_next to NULL as ire_inactive asserts
		 * so.
		 */
		ire->ire_next = NULL;
		ire_refrele(ire);
		ire = ire_next;
	}
}

/*
 * IRB_REFRELE is the only caller of the function. It calls to unlink
 * all the CONDEMNED ires from this bucket.
 */
ire_t *
ire_unlink(irb_t *irb)
{
	ire_t *ire;
	ire_t *ire1;
	ire_t **ptpn;
	ire_t *ire_list = NULL;

	ASSERT(RW_WRITE_HELD(&irb->irb_lock));
	ASSERT(irb->irb_refcnt == 0);
	ASSERT(irb->irb_marks & IRE_MARK_CONDEMNED);
	ASSERT(irb->irb_ire != NULL);

	for (ire = irb->irb_ire; ire != NULL; ire = ire1) {
		ire1 = ire->ire_next;
		if (ire->ire_marks & IRE_MARK_CONDEMNED) {
			ptpn = ire->ire_ptpn;
			ire1 = ire->ire_next;
			if (ire1)
				ire1->ire_ptpn = ptpn;
			*ptpn = ire1;
			ire->ire_ptpn = NULL;
			ire->ire_next = NULL;
			/*
			 * We need to call ire_delete_v4 or ire_delete_v6
			 * to clean up the cache or the redirects pointing at
			 * the default gateway. We need to drop the lock
			 * as ire_flush_cache/ire_delete_host_redircts require
			 * so. But we can't drop the lock, as ire_unlink needs
			 * to atomically remove the ires from the list.
			 * So, create a temporary list of CONDEMNED ires
			 * for doing ire_delete_v4/ire_delete_v6 operations
			 * later on.
			 */
			ire->ire_next = ire_list;
			ire_list = ire;
		}
	}
	ASSERT(irb->irb_refcnt == 0);
	irb->irb_marks &= ~IRE_MARK_CONDEMNED;
	ASSERT(ire_list != NULL);
	return (ire_list);
}

/*
 * Delete the specified IRE.
 */
void
ire_delete(ire_t *ire)
{
	ire_t	*ire1;
	ire_t	**ptpn;
	irb_t *irb;

	/*
	 * It was never inserted in the list. Should call REFRELE
	 * to free this IRE.
	 */
	if ((irb = ire->ire_bucket) == NULL) {
		ire_refrele(ire);
		return;
	}

	rw_enter(&irb->irb_lock, RW_WRITER);
	if (ire->ire_ptpn == NULL) {
		/*
		 * Some other thread has removed us from the list.
		 * It should have done the REFRELE for us.
		 */
		rw_exit(&irb->irb_lock);
		return;
	}
	if (irb->irb_refcnt != 0) {
		/*
		 * The last thread to leave this bucket will
		 * delete this ire.
		 */
		ire->ire_marks |= IRE_MARK_CONDEMNED;
		irb->irb_marks |= IRE_MARK_CONDEMNED;
		rw_exit(&irb->irb_lock);
		return;
	}
	/*
	 * Normally to delete an ire, we walk the bucket. While we
	 * walk the bucket, we normally bump up irb_refcnt and hence
	 * we return from above where we mark CONDEMNED and the ire
	 * gets deleted from ire_unlink. This case is where somebody
	 * knows the ire e.g by doing a lookup, and wants to delete the
	 * IRE. irb_refcnt would be 0 in this case if nobody is walking
	 * the bucket.
	 */
	ptpn = ire->ire_ptpn;
	ire1 = ire->ire_next;
	if (ire1 != NULL)
		ire1->ire_ptpn = ptpn;
	ASSERT(ptpn != NULL);
	*ptpn = ire1;
	ire->ire_ptpn = NULL;
	ire->ire_next = NULL;
	if (ire->ire_ipversion == IPV6_VERSION) {
		BUMP_IRE_STATS(ire_stats_v6, ire_stats_deleted);
	} else {
		BUMP_IRE_STATS(ire_stats_v4, ire_stats_deleted);
	}
	/*
	 * ip_wput/ip_wput_v6 checks this flag to see whether
	 * it should still use the cached ire or not.
	 */
	ire->ire_marks |= IRE_MARK_CONDEMNED;
	rw_exit(&irb->irb_lock);

	if (ire->ire_ipversion == IPV6_VERSION) {
		ire_delete_v6(ire);
	} else {
		ire_delete_v4(ire);
	}
	/*
	 * We removed it from the list. Decrement the
	 * reference count.
	 */
	ire_refrele(ire);
}

/*
 * Delete the specified IRE.
 * All calls should use ire_delete().
 * Sometimes called as writer though not required by this function.
 *
 * NOTE : This function is called only if the ire was added
 * in the list.
 */
static void
ire_delete_v4(ire_t *ire)
{
	ASSERT(ire->ire_refcnt >= 1);
	ASSERT(ire->ire_ipversion == IPV4_VERSION);

	if (ire->ire_type != IRE_CACHE)
		ire_flush_cache_v4(ire, IRE_FLUSH_DELETE);
	if (ire->ire_type == IRE_DEFAULT) {
		/*
		 * The entry has already been ire_add()ed.
		 * Adjust accounting
		 */
		ASSERT(ip_ire_default_count != 0);
		rw_enter(&ire->ire_bucket->irb_lock, RW_WRITER);
		ip_ire_default_count--;
		rw_exit(&ire->ire_bucket->irb_lock);
		/*
		 * when a default gateway is going away
		 * delete all the host redirects pointing at that
		 * gateway.
		 */
		ire_delete_host_redirects(ire->ire_gateway_addr);
	}
}

/*
 * IRE_REFRELE/ire_refrele are the only caller of the function. It calls
 * to free the ire when the reference count goes to zero.
 */
void
ire_inactive(ire_t *ire)
{
	ipif_t	*ipif;
	mblk_t *mp;
	nce_t	*nce;

	ASSERT(ire->ire_refcnt == 0);
	ASSERT(ire->ire_ptpn == NULL);
	ASSERT(ire->ire_next == NULL);

	if ((nce = ire->ire_nce) != NULL) {
		/* Only V6 IRE_CACHE type has an nce */
		ASSERT(ire->ire_type == IRE_CACHE);
		ASSERT(ire->ire_ipversion == IPV6_VERSION);
		NCE_REFRELE(nce);
		ire->ire_nce = NULL;
	}
	/* Remember the global statistics from the dying */
	if (ire->ire_ipif != NULL) {
		ipif = ire->ire_ipif;
		/* "inbound" to a non local address is a forward */
		if (ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))
			ipif->ipif_ib_pkt_count += ire->ire_ib_pkt_count;
		else
			ipif->ipif_fo_pkt_count += ire->ire_ib_pkt_count;
		ipif->ipif_ob_pkt_count += ire->ire_ob_pkt_count;
	}

	/* Free the xmit header, and the IRE itself. */
	if ((mp = ire->ire_dlureq_mp) != NULL) {
		freeb(mp);
		ire->ire_dlureq_mp = NULL;
	}

	if ((mp = ire->ire_fp_mp) != NULL) {
		freeb(mp);
		ire->ire_fp_mp = NULL;
	}

	mutex_destroy(&ire->ire_lock);
	if (ire->ire_ipversion == IPV6_VERSION) {
		BUMP_IRE_STATS(ire_stats_v6, ire_stats_freed);
	} else {
		BUMP_IRE_STATS(ire_stats_v4, ire_stats_freed);
	}
	if (ire->ire_mp != NULL) {
		/* Still in an mblk */
		freeb(ire->ire_mp);
	} else {
		/* Has been allocated out of the cache */
		kmem_cache_free(ire_cache, ire);
	}
}

/*
 * ire_walk routine to delete all IRE_CACHE/IRE_HOST_REDIRECT entries
 * that have a given gateway address.
 */
void
ire_delete_cache_gw(ire_t *ire, char *cp)
{
	ipaddr_t	gw_addr;

	if (!(ire->ire_type & (IRE_CACHE|IRE_HOST_REDIRECT)))
		return;

	bcopy(cp, &gw_addr, sizeof (gw_addr));
	if (ire->ire_gateway_addr == gw_addr) {
		ip1dbg(("ire_delete_cache_gw: deleted 0x%x type %d to 0x%x\n",
			(int)ntohl(ire->ire_addr), ire->ire_type,
			(int)ntohl(ire->ire_gateway_addr)));
		ire_delete(ire);
	}
}

/*
 * Remove all IRE_CACHE entries that match
 * the ire specified.  (Sometimes called
 * as writer though not required by this function.)
 *
 * The flag argument indicates if the
 * flush request is due to addition
 * of new route (IRE_FLUSH_ADD) or deletion of old
 * route (IRE_FLUSH_DELETE).
 *
 * This routine takes only the IREs from the forwarding
 * table and flushes the corresponding entries from
 * the cache table.
 *
 * When flushing due to the deletion of an old route, it
 * just checks the cache handles (ire_phandle and ire_ihandle) and
 * deletes the ones that match.
 *
 * When flushing due to the creation of a new route, it checks
 * if a cache entry's address matches the one in the IRE and
 * that the cache entry's parent has a less specific mask than the
 * one in IRE.
 */
void
ire_flush_cache_v4(ire_t *ire, int flag)
{
	int i;
	ire_t *cire;
	irb_t *irb;

	if (ire->ire_type & IRE_CACHETABLE)
	    return;

	/*
	 * If a default is just created, there is no point
	 * in going through the cache, as there will not be any
	 * cached ires.
	 */
	if (ire->ire_type == IRE_DEFAULT && flag == IRE_FLUSH_ADD)
		return;
	if (flag == IRE_FLUSH_ADD) {
		/*
		 * This selective flush is due to the addition of
		 * new IRE.
		 */
		for (i = 0; i < IP_CACHE_TABLE_SIZE; i++) {
			irb = &ip_cache_table[i];
			if ((cire = irb->irb_ire) == NULL)
				continue;
			IRB_REFHOLD(irb);
			for (cire = irb->irb_ire; cire != NULL;
			    cire = cire->ire_next) {
				if (cire->ire_type != IRE_CACHE)
					continue;
				if (((cire->ire_addr & ire->ire_mask) !=
				    (ire->ire_addr & ire->ire_mask)) ||
				    (ip_mask_to_index(cire->ire_cmask) >
				    ire->ire_masklen))
					continue;
				ire_delete(cire);
			}
			IRB_REFRELE(irb);
		}
	} else {
		/*
		 * delete the cache entries based on
		 * handle in the IRE as this IRE is
		 * being deleted/changed.
		 */
		for (i = 0; i < IP_CACHE_TABLE_SIZE; i++) {
			irb = &ip_cache_table[i];
			if ((cire = irb->irb_ire) == NULL)
				continue;
			IRB_REFHOLD(irb);
			for (cire = irb->irb_ire; cire != NULL;
			    cire = cire->ire_next) {
				if (cire->ire_type != IRE_CACHE)
					continue;
				if ((cire->ire_phandle == 0 ||
				    cire->ire_phandle != ire->ire_phandle) &&
				    (cire->ire_ihandle == 0 ||
				    cire->ire_ihandle != ire->ire_ihandle))
					continue;
				ire_delete(cire);
			}
			IRB_REFRELE(irb);
		}
	}
}

/*
 * Matches the arguments passed with the values in the ire.
 *
 * Note: for match types that match using "ipif" passed in, ipif
 * must be checked for non-NULL before calling this routine.
 */
static boolean_t
ire_match_args(ire_t *ire, ipaddr_t addr, ipaddr_t mask, ipaddr_t gateway,
    int type, ipif_t *ipif, queue_t *wrq, uint32_t ihandle, int match_flags)
{
	ASSERT(ire->ire_ipversion == IPV4_VERSION);
	ASSERT((ire->ire_addr & ~ire->ire_mask) == 0);
	ASSERT((!(match_flags & MATCH_IRE_ILL)) ||
		(ipif != NULL && !ipif->ipif_isv6));

	if ((ire->ire_addr == (addr & mask)) &&
	    ((!(match_flags & MATCH_IRE_GW)) ||
		(ire->ire_gateway_addr == gateway)) &&
	    ((!(match_flags & MATCH_IRE_TYPE)) ||
		(ire->ire_type & type)) &&
	    ((!(match_flags & MATCH_IRE_SRC)) ||
		(ire->ire_src_addr == ipif->ipif_src_addr)) &&
	    ((!(match_flags & MATCH_IRE_WQ)) ||
		(ire->ire_stq == wrq)) &&
	    ((!(match_flags & MATCH_IRE_IPIF)) ||
		(ire->ire_ipif == ipif)) &&
	    ((!(match_flags & MATCH_IRE_ILL)) ||
		(ire->ire_ipif != NULL &&
		(ire->ire_ipif->ipif_ill == ipif->ipif_ill))) &&
	    ((!(match_flags & MATCH_IRE_IHANDLE)) ||
		(ire->ire_ihandle == ihandle))) {
		/* We found the matched IRE */
		return (B_TRUE);
	}
	return (B_FALSE);
}


/*
 * Lookup for a route in all the tables
 */
ire_t *
ire_route_lookup(ipaddr_t addr, ipaddr_t mask, ipaddr_t gateway,
    int type, ipif_t *ipif, ire_t **pire, queue_t *wrq, int flags)
{
	ire_t *ire = NULL;

	/*
	 * ire_match_args() will dereference ipif MATCH_IRE_SRC or
	 * MATCH_IRE_ILL is set.
	 */
	if ((flags & (MATCH_IRE_SRC | MATCH_IRE_ILL)) &&
	    (ipif == NULL))
		return (NULL);

	/*
	 * might be asking for a cache lookup,
	 * This is not best way to lookup cache,
	 * user should call ire_cache_lookup directly.
	 *
	 * If MATCH_IRE_TYPE was set, first lookup in the cache table and then
	 * in the forwarding table, if the applicable type flags were set.
	 */
	if ((flags & MATCH_IRE_TYPE) == 0 || (type & IRE_CACHETABLE) != 0) {
		ire = ire_ctable_lookup(addr, gateway, type, ipif, wrq, flags);
		if (ire != NULL)
			return (ire);
	}
	if ((flags & MATCH_IRE_TYPE) == 0 || (type & IRE_FORWARDTABLE) != 0) {
		ire = ire_ftable_lookup(addr, mask, gateway, type, ipif, pire,
		    wrq, 0, flags);
	}
	return (ire);
}

/*
 * Lookup a route in forwarding table.
 * specific lookup is indicated by passing the
 * required parameters and indicating the
 * match required in flag field.
 *
 * Looking for default route can be done in three ways
 * 1) pass mask as 0 and set MATCH_IRE_MASK in flags field
 *    along with other matches.
 * 2) pass type as IRE_DEFAULT and set MATCH_IRE_TYPE in flags
 *    field along with other matches.
 * 3) if the destination and mask are passed as zeros.
 *
 * A request to return a default route if no route
 * is found, can be specified by setting MATCH_IRE_DEFAULT
 * in flags.
 *
 * It does not support recursion more than one level. It
 * will do recursive lookup only when the lookup maps to
 * a prefix or default route and MATCH_IRE_RECURSIVE flag is passed.
 *
 * If the routing table is setup to allow more than one level
 * of recursion, the cleaning up cache table will not work resulting
 * in invalid routing.
 *
 * Supports IP_BOUND_IF by following the ipif/ill when recursing.
 *
 * NOTE : When this function returns NULL, pire has already been released.
 *	  pire is valid only when this function successfully returns an
 *	  ire.
 */
ire_t *
ire_ftable_lookup(ipaddr_t addr, ipaddr_t mask, ipaddr_t gateway,
    int type, ipif_t *ipif, ire_t **pire, queue_t *wrq, uint32_t ihandle,
    int flags)
{
	irb_t *irb_ptr;
	ire_t *ire = NULL;
	int i;
	ipaddr_t gw_addr;

	ASSERT(ipif == NULL || !ipif->ipif_isv6);

	/*
	 * When we return NULL from this function, we should make
	 * sure that *pire is NULL so that the callers will not
	 * wrongly REFRELE the pire.
	 */
	if (pire != NULL)
		*pire = NULL;
	/*
	 * ire_match_args() will dereference ipif MATCH_IRE_SRC or
	 * MATCH_IRE_ILL is set.
	 */
	if ((flags & (MATCH_IRE_SRC | MATCH_IRE_ILL)) &&
	    (ipif == NULL))
		return (NULL);

	/*
	 * If the mask is known, the lookup
	 * is simple, if the mask is not known
	 * we need to search.
	 */
	if (flags & MATCH_IRE_MASK) {
		uint_t masklen;

		masklen = ip_mask_to_index(mask);
		if (ip_forwarding_table[masklen] == NULL)
			return (NULL);
		irb_ptr = &(ip_forwarding_table[masklen][
		    IRE_ADDR_HASH(addr & mask, IP_FTABLE_HASH_SIZE)]);
		rw_enter(&irb_ptr->irb_lock, RW_READER);
		for (ire = irb_ptr->irb_ire; ire != NULL;
		    ire = ire->ire_next) {
			if (ire->ire_marks & IRE_MARK_CONDEMNED)
				continue;
			if (ire_match_args(ire, addr, mask, gateway, type, ipif,
			    wrq, ihandle, flags))
				goto found_ire;
		}
		rw_exit(&irb_ptr->irb_lock);
	} else {
		/*
		 * In this case we don't know the mask, we need to
		 * search the table assuming different mask sizes.
		 * we start with 32 bit mask, we don't allow default here.
		 */
		for (i = (IP_MASK_TABLE_SIZE - 1); i > 0; i--) {
			ipaddr_t tmpmask;

			if ((ip_forwarding_table[i]) == NULL)
				continue;
			tmpmask = ip_index_to_mask(i);
			irb_ptr = &ip_forwarding_table[i][
			    IRE_ADDR_HASH(addr & tmpmask,
			    IP_FTABLE_HASH_SIZE)];
			rw_enter(&irb_ptr->irb_lock, RW_READER);
			for (ire = irb_ptr->irb_ire; ire != NULL;
			    ire = ire->ire_next) {
				if (ire->ire_marks & IRE_MARK_CONDEMNED)
					continue;
				if (ire_match_args(ire, addr, ire->ire_mask,
				    gateway, type, ipif, wrq, ihandle, flags))
					goto found_ire;
			}
			rw_exit(&irb_ptr->irb_lock);
		}
	}
	/*
	 * We come here if no route has yet been found.
	 *
	 * Handle the case where default route is
	 * requested by specifying type as one of the possible
	 * types for that can have a zero mask (IRE_DEFAULT and IRE_INTERFACE).
	 *
	 * If MATCH_IRE_MASK is specified, then the appropriate default route
	 * would have been found above if it exists so it isn't looked up here.
	 * If MATCH_IRE_DEFAULT was also specified, then a default route will be
	 * searched for later.
	 */
	if ((flags & (MATCH_IRE_TYPE | MATCH_IRE_MASK)) == MATCH_IRE_TYPE &&
	    (type & (IRE_DEFAULT | IRE_INTERFACE))) {
		if ((ip_forwarding_table[0])) {
			/* addr & mask is zero for defaults */
			irb_ptr = &ip_forwarding_table[0][
			    IRE_ADDR_HASH(0, IP_FTABLE_HASH_SIZE)];
			rw_enter(&irb_ptr->irb_lock, RW_READER);
			for (ire = irb_ptr->irb_ire; ire != NULL;
			    ire = ire->ire_next) {
				if (ire->ire_marks & IRE_MARK_CONDEMNED)
					continue;
				if (ire_match_args(ire, addr, (ipaddr_t)0,
				    gateway, type, ipif, wrq, ihandle, flags))
					goto found_ire;
			}
			rw_exit(&irb_ptr->irb_lock);
		}
	}
	/*
	 * we come here only if no route is found.
	 * see if the default route can be used which is allowed
	 * only if the default matching criteria is specified.
	 * The ip_ire_default_count tracks the number of IRE_DEFAULT
	 * entries. However, the ip_forwarding_table[0] also contains
	 * interface routes thus the count can be zero.
	 */
	if ((flags & (MATCH_IRE_DEFAULT | MATCH_IRE_MASK)) ==
	    MATCH_IRE_DEFAULT) {
		uint_t	g_cnt;
		uint_t  g_index;
		uint_t	index;

		if (ip_forwarding_table[0] == NULL)
			return (NULL);
		irb_ptr = &(ip_forwarding_table[0])[0];

		rw_enter(&irb_ptr->irb_lock, RW_READER);
		ire = irb_ptr->irb_ire;
		if (ire ==  NULL) {
			rw_exit(&irb_ptr->irb_lock);
			return (NULL);
		}

		/*
		 * Store the index, since it can be changed by other threads.
		 */
		if (ip_ire_default_count != 0)
			g_index = ip_ire_default_index++;

		/*
		 * Round-robin the default routers list looking for a
		 * route that matches the passed in parameters.
		 */
		for (g_cnt = ip_ire_default_count; g_cnt != 0; g_cnt--) {
			index = g_index % ip_ire_default_count;
			g_index++;
			while (ire->ire_next && index--) {
				ire = ire->ire_next;
			}
			if (ire->ire_marks & IRE_MARK_CONDEMNED)
				continue;
			if (ire_match_args(ire, addr, (ipaddr_t)0,
			    gateway, type, ipif, wrq, ihandle, flags))
				goto found_ire;
		}
		/*
		 * If there are no default routes, we return the ire
		 * that was in ip_forwarding_table[0][0]. This is
		 * needed to support proxy arping, where we have a
		 * default interface route to arp for all destinations
		 * that are off-link.
		 */
		if (ire != NULL)
			goto found_ire;

		rw_exit(&irb_ptr->irb_lock);
	}
	if (ire == NULL) {
		return (NULL);
	}
found_ire:
	if ((flags & MATCH_IRE_RJ_BHOLE) &&
	    (ire->ire_flags & (RTF_BLACKHOLE | RTF_REJECT))) {
		ASSERT((ire->ire_marks & IRE_MARK_CONDEMNED) == 0);
		IRE_REFHOLD(ire);
		rw_exit(&irb_ptr->irb_lock);
		return (ire);
	}
	/*
	 * At this point, IRE that was found must be an IRE_FORWARDTABLE
	 * type.  If this is a recursive lookup and an IRE_INTERFACE type was
	 * found, return that.  If it was some other IRE_FORWARDTABLE type of
	 * IRE (one of the prefix types), then it is necessary to fill in the
	 * parent IRE pointed to by pire, and then lookup the gateway address of
	 * the parent.  For backwards compatiblity, if this lookup returns an
	 * IRE other than a IRE_CACHETABLE or IRE_INTERFACE, then one more level
	 * of lookup is done.
	 */
	if (flags & MATCH_IRE_RECURSIVE) {
		ipif_t	*gw_ipif;
		int match_flags = MATCH_IRE_DSTONLY;

		ire_t *save_ire;

		ASSERT((ire->ire_marks & IRE_MARK_CONDEMNED) == 0);
		IRE_REFHOLD(ire);
		rw_exit(&irb_ptr->irb_lock);
		if (ire->ire_type & IRE_INTERFACE)
			return (ire);
		if (pire != NULL)
			*pire = ire;
		/*
		 * If we can't find an IRE_INTERFACE or the caller has not
		 * asked for pire, we need to REFRELE the save_ire.
		 */
		save_ire = ire;

		if (ire->ire_ipif != NULL)
			match_flags |= MATCH_IRE_ILL;

		ire = ire_route_lookup(ire->ire_gateway_addr, 0, 0, 0,
		    ire->ire_ipif, NULL, wrq, match_flags);
		if (ire == NULL) {
			ire_refrele(save_ire);
			if (pire != NULL)
				*pire = NULL;
			return (NULL);
		}
		if (ire->ire_type & (IRE_CACHETABLE | IRE_INTERFACE)) {
			/*
			 * If the caller did not ask for pire, release
			 * it now.
			 */
			if (pire == NULL) {
				ire_refrele(save_ire);
			}
			return (ire);
		}
		match_flags |= MATCH_IRE_TYPE;
		gw_addr = ire->ire_gateway_addr;
		gw_ipif = ire->ire_ipif;
		ire_refrele(ire);
		ire = ire_route_lookup(gw_addr, 0, 0,
		    (IRE_CACHETABLE | IRE_INTERFACE), gw_ipif, NULL, wrq,
		    match_flags);
		if (ire == NULL) {
			ire_refrele(save_ire);
			if (pire != NULL)
				*pire = NULL;
			return (NULL);
		} else if (pire == NULL) {
			/*
			 * If the caller did not ask for pire, release
			 * it now.
			 */
			ire_refrele(save_ire);
		}
		return (ire);
	}
	ASSERT(pire == NULL || *pire == NULL);
	ASSERT((ire->ire_marks & IRE_MARK_CONDEMNED) == 0);
	IRE_REFHOLD(ire);
	rw_exit(&irb_ptr->irb_lock);
	return (ire);
}

/*
 * Looks up cache table for a route.
 * specific lookup can be indicated by
 * passing the MATCH_* flags and the
 * necessary parameters.
 */
ire_t *
ire_ctable_lookup(ipaddr_t addr, ipaddr_t gateway, int type, ipif_t *ipif,
    queue_t *wrq, int flags)
{
	irb_t *irb_ptr;
	ire_t *ire;

	/*
	 * ire_match_args() will dereference ipif MATCH_IRE_SRC or
	 * MATCH_IRE_ILL is set.
	 */
	if ((flags & (MATCH_IRE_SRC | MATCH_IRE_ILL)) &&
	    (ipif == NULL))
		return (NULL);

	irb_ptr = &ip_cache_table[IRE_ADDR_HASH(addr, IP_CACHE_TABLE_SIZE)];
	rw_enter(&irb_ptr->irb_lock, RW_READER);
	for (ire = irb_ptr->irb_ire; ire != NULL; ire = ire->ire_next) {
		if (ire->ire_marks & IRE_MARK_CONDEMNED)
			continue;
		ASSERT(ire->ire_mask == IP_HOST_MASK);
		if (ire_match_args(ire, addr, ire->ire_mask, gateway, type,
		    ipif, wrq, 0, flags)) {
			IRE_REFHOLD(ire);
			rw_exit(&irb_ptr->irb_lock);
			return (ire);
		}
	}
	rw_exit(&irb_ptr->irb_lock);
	return (NULL);
}

/*
 * Lookup cache
 */
ire_t *
ire_cache_lookup(ipaddr_t addr)
{
	irb_t *irb_ptr;
	ire_t *ire;

	irb_ptr = &ip_cache_table[IRE_ADDR_HASH(addr, IP_CACHE_TABLE_SIZE)];
	rw_enter(&irb_ptr->irb_lock, RW_READER);
	for (ire = irb_ptr->irb_ire; ire != NULL; ire = ire->ire_next) {
		if (ire->ire_marks & IRE_MARK_CONDEMNED)
			continue;
		if (ire->ire_addr == addr) {
			IRE_REFHOLD(ire);
			rw_exit(&irb_ptr->irb_lock);
			return (ire);
		}
	}
	rw_exit(&irb_ptr->irb_lock);
	return (NULL);
}

/*
 * Locate the interface ire that is tied to the cache ire 'cire' via
 * cire->ire_ihandle.
 *
 * We are trying to create the cache ire for an offlink destn based
 * on the cache ire of the gateway in 'cire'. 'pire' is the prefix ire
 * as found by ip_newroute(). We are called from ip_newroute() in
 * the IRE_CACHE case.
 */
ire_t *
ire_ihandle_lookup_offlink(ire_t *cire, ire_t *pire)
{
	ire_t	*ire;
	int	match_flags;
	ipaddr_t gw_addr;
	ipif_t	*gw_ipif;

	ASSERT(cire != NULL && pire != NULL);

	match_flags =  MATCH_IRE_TYPE | MATCH_IRE_IHANDLE | MATCH_IRE_MASK;
	if (pire->ire_ipif != NULL)
		match_flags |= MATCH_IRE_ILL;
	/*
	 * We know that the mask of the interface ire equals cire->ire_cmask.
	 * (When ip_newroute() created 'cire' for the gateway it set its
	 * cmask from the interface ire's mask)
	 */
	ire = ire_ftable_lookup(cire->ire_addr, cire->ire_cmask, 0,
	    IRE_INTERFACE, pire->ire_ipif, NULL, NULL, cire->ire_ihandle,
	    match_flags);
	if (ire != NULL)
		return (ire);
	/*
	 * If we didn't find an interface ire above, we can't declare failure.
	 * For backwards compatibility, we need to support prefix routes
	 * pointing to next hop gateways that are not on-link.
	 *
	 * Assume we are trying to ping some offlink destn, and we have the
	 * routing table below.
	 *
	 * Eg.	default	- gw1		<--- pire	(line 1)
	 *	gw1	- gw2				(line 2)
	 *	gw2	- hme0				(line 3)
	 *
	 * If we already have a cache ire for gw1 in 'cire', the
	 * ire_ftable_lookup above would have failed, since there is no
	 * interface ire to reach gw1. We will fallthru below.
	 *
	 * Here we duplicate the steps that ire_ftable_lookup() did in
	 * getting 'cire' from 'pire', in the MATCH_IRE_RECURSIVE case.
	 * The differences are the following
	 * i.   We want the interface ire only, so we call ire_ftable_lookup()
	 *	instead of ire_route_lookup()
	 * ii.  We look for only prefix routes in the 1st call below.
	 * ii.  We want to match on the ihandle in the 2nd call below.
	 */
	match_flags =  MATCH_IRE_TYPE;
	if (pire->ire_ipif != NULL)
		match_flags |= MATCH_IRE_ILL;
	ire = ire_ftable_lookup(pire->ire_gateway_addr, 0, 0, IRE_OFFSUBNET,
	    pire->ire_ipif, NULL, NULL, 0, match_flags);
	if (ire == NULL)
		return (NULL);
	/*
	 * At this point 'ire' corresponds to the entry shown in line 2.
	 * gw_addr is 'gw2' in the example above.
	 */
	gw_addr = ire->ire_gateway_addr;
	gw_ipif = ire->ire_ipif;
	ire_refrele(ire);

	match_flags |= MATCH_IRE_IHANDLE;
	ire = ire_ftable_lookup(gw_addr, 0, 0, IRE_INTERFACE,
	    gw_ipif, NULL, NULL, cire->ire_ihandle, match_flags);
	return (ire);
}

/*
 * Locate the interface ire that is tied to the cache ire 'cire' via
 * cire->ire_ihandle.
 *
 * We are trying to create the cache ire for an onlink destn. or
 * gateway in 'cire'. We are called from ire_add_v4() in the IRE_IF_RESOLVER
 * case, after the ire has come back from ARP.
 */
ire_t *
ire_ihandle_lookup_onlink(ire_t *cire)
{
	ire_t	*ire;
	int	match_flags;
	int	i;
	int	j;
	irb_t	*irb_ptr;

	ASSERT(cire != NULL);

	match_flags =  MATCH_IRE_TYPE | MATCH_IRE_IHANDLE | MATCH_IRE_MASK;
	/*
	 * We know that the mask of the interface ire equals cire->ire_cmask.
	 * (When ip_newroute() created 'cire' for an on-link destn. it set its
	 * cmask from the interface ire's mask)
	 */
	ire = ire_ftable_lookup(cire->ire_addr, cire->ire_cmask, 0,
	    IRE_INTERFACE, NULL, NULL, NULL, cire->ire_ihandle, match_flags);
	if (ire != NULL)
		return (ire);
	/*
	 * If we didn't find an interface ire above, we can't declare failure.
	 * For backwards compatibility, we need to support prefix routes
	 * pointing to next hop gateways that are not on-link.
	 *
	 * In the resolver/noresolver case, ip_newroute() thinks it is creating
	 * the cache ire for an onlink destination in 'cire'. But 'cire' is
	 * not actually onlink, because ire_ftable_lookup() cheated it, by
	 * doing ire_route_lookup() twice and returning an interface ire.
	 *
	 * Eg. default	-	gw1			(line 1)
	 *	gw1	-	gw2			(line 2)
	 *	gw2	-	hme0			(line 3)
	 *
	 * In the above example, ip_newroute() tried to create the cache ire
	 * 'cire' for gw1, based on the interface route in line 3. The
	 * ire_ftable_lookup() above fails, because there is no interface route
	 * to reach gw1. (it is gw2). We fall thru below.
	 *
	 * Do a brute force search based on the ihandle in a subset of the
	 * forwarding tables, corresponding to cire->ire_cmask. Otherwise
	 * things become very complex, since we don't have 'pire' in this
	 * case. (Also note that this method is not possible in the offlink
	 * case because we don't know the mask)
	 */
	i = ip_mask_to_index(cire->ire_cmask);
	if ((ip_forwarding_table[i]) == NULL)
		return (NULL);
	for (j = 0; j < IP_FTABLE_HASH_SIZE; j++) {
		irb_ptr = &ip_forwarding_table[i][j];
		rw_enter(&irb_ptr->irb_lock, RW_READER);
		for (ire = irb_ptr->irb_ire; ire != NULL;
		    ire = ire->ire_next) {
			if (ire->ire_marks & IRE_MARK_CONDEMNED)
				continue;
			if ((ire->ire_type & IRE_INTERFACE) &&
			    (ire->ire_ihandle == cire->ire_ihandle)) {
				IRE_REFHOLD(ire);
				rw_exit(&irb_ptr->irb_lock);
				return (ire);
			}
		}
		rw_exit(&irb_ptr->irb_lock);
	}
	return (NULL);
}

/*
 * Return the IRE_LOOPBACK, IRE_IF_RESOLVER or IRE_IF_NORESOLVER
 * ire associated with the specified ipif.
 *
 * This might occasionally be called when IFF_UP is not set since
 * the IP_MULTICAST_IF as well as creating interface routes
 * allows specifying a down ipif (ipif_lookup* match ipifs that are down).
 *
 * Note that if IFF_NOLOCAL, IFF_NOXMIT, or IFF_DEPRECATED is set on the ipif
 * this routine might return NULL.
 */
ire_t *
ipif_to_ire(ipif_t *ipif)
{
	ire_t	*ire;

	ASSERT(!ipif->ipif_isv6);
	if (ipif->ipif_ire_type == IRE_LOOPBACK) {
		ire = ire_ctable_lookup(ipif->ipif_lcl_addr, 0, IRE_LOOPBACK,
		    ipif, NULL, (MATCH_IRE_TYPE | MATCH_IRE_IPIF));
	} else if (ipif->ipif_flags & IFF_POINTOPOINT) {
		/* In this case we need to lookup destination address. */
		ire = ire_ftable_lookup(ipif->ipif_pp_dst_addr, IP_HOST_MASK, 0,
		    IRE_INTERFACE, ipif, NULL, NULL, 0,
		    (MATCH_IRE_TYPE | MATCH_IRE_IPIF | MATCH_IRE_MASK));
	} else {
		ire = ire_ftable_lookup(ipif->ipif_subnet,
		    ipif->ipif_net_mask, 0, IRE_INTERFACE, ipif, NULL, NULL, 0,
		    (MATCH_IRE_TYPE | MATCH_IRE_IPIF | MATCH_IRE_MASK));
	}
	return (ire);
}

/*
 * ire_walk function.
 * Count the number of IRE_CACHE entries in different categories.
 */
void
ire_cache_count(ire_t *ire, char *arg)
{
	ire_cache_count_t *icc = (ire_cache_count_t *)arg;

	if (ire->ire_type != IRE_CACHE)
		return;

	icc->icc_total++;

	if (ire->ire_ipversion == IPV6_VERSION) {
		mutex_enter(&ire->ire_lock);
		if (IN6_IS_ADDR_UNSPECIFIED(&ire->ire_gateway_addr_v6)) {
			mutex_exit(&ire->ire_lock);
			icc->icc_onlink++;
			return;
		}
		mutex_exit(&ire->ire_lock);
	} else {
		if (ire->ire_gateway_addr == 0) {
			icc->icc_onlink++;
			return;
		}
	}

	ASSERT(ire->ire_ipif != NULL);
	if (ire->ire_max_frag < ire->ire_ipif->ipif_mtu)
		icc->icc_pmtu++;
	else if (ire->ire_tire_mark != ire->ire_ob_pkt_count +
	    ire->ire_ib_pkt_count)
		icc->icc_offlink++;
	else
		icc->icc_unused++;
}

/*
 * ire_walk function called by ip_trash_ire_reclaim().
 * Free a fraction of the IRE_CACHE cache entries. The fractions are
 * different for different categories of IRE_CACHE entries.
 * A fraction of zero means to not free any in that category.
 * Use the hash bucket id plus lbolt as a random number. Thus if the fraction
 * is N then every Nth hash bucket chain will be freed.
 */
void
ire_cache_reclaim(ire_t *ire, char *arg)
{
	ire_cache_reclaim_t *icr = (ire_cache_reclaim_t *)arg;
	uint_t rand;

	if (ire->ire_type != IRE_CACHE)
		return;

	if (ire->ire_ipversion == IPV6_VERSION) {
		rand = (uint_t)lbolt +
		    IRE_ADDR_HASH_V6(ire->ire_addr_v6, IP6_CACHE_TABLE_SIZE);
		mutex_enter(&ire->ire_lock);
		if (IN6_IS_ADDR_UNSPECIFIED(&ire->ire_gateway_addr_v6)) {
			mutex_exit(&ire->ire_lock);
			if (icr->icr_onlink != 0 &&
			    (rand/icr->icr_onlink)*icr->icr_onlink == rand) {
				ire_delete(ire);
				return;
			}
			goto done;
		}
		mutex_exit(&ire->ire_lock);
	} else {
		rand = (uint_t)lbolt +
		    IRE_ADDR_HASH(ire->ire_addr, IP_CACHE_TABLE_SIZE);
		if (ire->ire_gateway_addr == 0) {
			if (icr->icr_onlink != 0 &&
			    (rand/icr->icr_onlink)*icr->icr_onlink == rand) {
				ire_delete(ire);
				return;
			}
			goto done;
		}
	}
	/* Not onlink IRE */
	ASSERT(ire->ire_ipif != NULL);
	if (ire->ire_max_frag < ire->ire_ipif->ipif_mtu) {
		/* Use ptmu fraction */
		if (icr->icr_pmtu != 0 &&
		    (rand/icr->icr_pmtu)*icr->icr_pmtu == rand) {
			ire_delete(ire);
			return;
		}
	} else if (ire->ire_tire_mark != ire->ire_ob_pkt_count +
	    ire->ire_ib_pkt_count) {
		/* Use offlink fraction */
		if (icr->icr_offlink != 0 &&
		    (rand/icr->icr_offlink)*icr->icr_offlink == rand) {
			ire_delete(ire);
			return;
		}
	} else {
		/* Use unused fraction */
		if (icr->icr_unused != 0 &&
		    (rand/icr->icr_unused)*icr->icr_unused == rand) {
			ire_delete(ire);
			return;
		}
	}
done:
	/*
	 * Update tire_mark so that those that haven't been used since this
	 * reclaim will be considered unused next time we reclaim.
	 */
	ire->ire_tire_mark = ire->ire_ob_pkt_count + ire->ire_ib_pkt_count;
}

void
ip_ire_init()
{
	int i;

	mutex_init(&ire_ft_init_lock, NULL, MUTEX_DEFAULT, 0);
	mutex_init(&ire_handle_lock, NULL, MUTEX_DEFAULT, NULL);

	ip_cache_table = (irb_t *)kmem_zalloc(IP_CACHE_TABLE_SIZE *
	    sizeof (irb_t), KM_SLEEP);

	for (i = 0; i < IP_CACHE_TABLE_SIZE; i++) {
		rw_init(&ip_cache_table[i].irb_lock, NULL,
		    RW_DEFAULT, NULL);
	}

	ip_cache_table_v6 = (irb_t *)kmem_zalloc(IP6_CACHE_TABLE_SIZE *
	    sizeof (irb_t), KM_SLEEP);

	for (i = 0; i < IP6_CACHE_TABLE_SIZE; i++) {
		rw_init(&ip_cache_table_v6[i].irb_lock, NULL,
		    RW_DEFAULT, NULL);
	}
	/*
	 * Create ire caches, ire_reclaim()
	 * will give IRE_CACHE back to system when needed.
	 * This needs to be done here before anything else, since
	 * ire_add() expects the cache to be created.
	 */
	ire_cache = kmem_cache_create("ire_cache",
		sizeof (ire_t), 0, ip_ire_constructor,
		ip_ire_destructor, ip_ire_reclaim, NULL, NULL, 0);
}

void
ip_ire_fini()
{
	int i;

	mutex_destroy(&ire_ft_init_lock);
	mutex_destroy(&ire_handle_lock);

	for (i = 0; i < IP_CACHE_TABLE_SIZE; i++) {
		rw_destroy(&ip_cache_table[i].irb_lock);
	}
	kmem_free(ip_cache_table, IP_CACHE_TABLE_SIZE * sizeof (irb_t));

	for (i = 0; i < IP6_CACHE_TABLE_SIZE; i++) {
		rw_destroy(&ip_cache_table_v6[i].irb_lock);
	}
	kmem_free(ip_cache_table_v6, IP6_CACHE_TABLE_SIZE * sizeof (irb_t));

	kmem_cache_destroy(ire_cache);
}
