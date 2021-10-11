/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip6_ire.c	1.8	99/10/18 SMI"

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
#include <inet/arp.h>
#include <inet/ip_ndp.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>

irb_t *ip_forwarding_table_v6[IP6_MASK_TABLE_SIZE];
/* This is dynamically allocated in ip_ire_init */
irb_t *ip_cache_table_v6;

static	void	ire_report_ftable_v6(ire_t *ire, char *mp);
static	void	ire_report_ctable_v6(ire_t *ire, char *mp);
static boolean_t ire_match_args_v6(ire_t *ire, const in6_addr_t *addr,
    const in6_addr_t *mask, const in6_addr_t *gateway, int type, ipif_t *ipif,
    queue_t *wrq, uint32_t ihandle, int match_flags);

/*
 * Named Dispatch routine to produce a formatted report on all IREs.
 * This report is accessed by using the ndd utility to "get" ND variable
 * "ip_ire_status_v6".
 */
/* ARGSUSED */
int
ip_ire_report_v6(queue_t *q, mblk_t *mp, caddr_t arg)
{
	(void) mi_mpprintf(mp,
	    "IRE      rfq      stq      mxfrg rtt   rtt_sd ssthresh ref "
	    "rtomax tstamp_ok wscale_ok ecn_ok pmtud_ok sack sendpipe recvpipe "
	    "in/out/forward type    addr         mask         "
	    "src             gateway");
	/*
	 *   01234567 01234567 01234567 12345 12345  12345   12345678 123
	 *   123456 123456789 123456789 123456 12345678 1234 12345678 12345678
	 *   in/out/forward xxxxxxxxxx
	 *   xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
	 *   xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
	 *   xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
	 *   xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
	 */
	ire_walk_v6(ire_report_ftable_v6, (char *)mp);
	ire_walk_v6(ire_report_ctable_v6, (char *)mp);
	return (0);
}

/*
 * ire_walk routine invoked for ip_ire_report_v6 for each IRE.
 */
static void
ire_report_ftable_v6(ire_t *ire, char *mp)
{
	char	buf1[INET6_ADDRSTRLEN];
	char	buf2[INET6_ADDRSTRLEN];
	char	buf3[INET6_ADDRSTRLEN];
	char	buf4[INET6_ADDRSTRLEN];
	uint_t	fo_pkt_count;
	uint_t	ib_pkt_count;
	int	ref;
	in6_addr_t gw_addr_v6;

	ASSERT(ire->ire_ipversion == IPV6_VERSION);
	if (ire->ire_type & IRE_CACHETABLE)
	    return;

	/* Number of active references of this ire */
	ref = ire->ire_refcnt;
	/* "inbound" to a non local address is a forward */
	ib_pkt_count = ire->ire_ib_pkt_count;
	fo_pkt_count = 0;
	ASSERT(!(ire->ire_type & IRE_BROADCAST));
	if (!(ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))) {
		fo_pkt_count = ib_pkt_count;
		ib_pkt_count = 0;
	}

	mutex_enter(&ire->ire_lock);
	gw_addr_v6 = ire->ire_gateway_addr_v6;
	mutex_exit(&ire->ire_lock);

	(void) mi_mpprintf((mblk_t *)mp,
	    MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR
	    "%05d %05ld %06ld %08d %03d %06d %09d %09d %06d %08d "
	    "%04d %08d %08d %d/%d/%d %s\n\t%s\n\t%s\n\t%s\n\t%s",
	    (void *)ire, (void *)ire->ire_rfq, (void *)ire->ire_stq,
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
	    ip_nv_lookup(ire_nv_tbl, (int)ire->ire_type),
	    inet_ntop(AF_INET6, &ire->ire_addr_v6, buf1, sizeof (buf1)),
	    inet_ntop(AF_INET6, &ire->ire_mask_v6, buf2, sizeof (buf2)),
	    inet_ntop(AF_INET6, &ire->ire_src_addr_v6, buf3, sizeof (buf3)),
	    inet_ntop(AF_INET6, &gw_addr_v6, buf4, sizeof (buf4)));
}

/* ire_walk routine invoked for ip_ire_report_v6 for each IRE. */
static void
ire_report_ctable_v6(ire_t *ire, char *mp)
{
	char	buf1[INET6_ADDRSTRLEN];
	char	buf2[INET6_ADDRSTRLEN];
	char	buf3[INET6_ADDRSTRLEN];
	char	buf4[INET6_ADDRSTRLEN];
	uint_t	fo_pkt_count;
	uint_t	ib_pkt_count;
	int	ref;
	in6_addr_t gw_addr_v6;

	if ((ire->ire_type & IRE_CACHETABLE) == 0)
		return;
	/* Number of active references of this ire */
	ref = ire->ire_refcnt;
	/* "inbound" to a non local address is a forward */
	ib_pkt_count = ire->ire_ib_pkt_count;
	fo_pkt_count = 0;
	ASSERT(!(ire->ire_type & IRE_BROADCAST));
	if (ire->ire_type & IRE_LOCAL) {
		fo_pkt_count = ib_pkt_count;
		ib_pkt_count = 0;
	}

	mutex_enter(&ire->ire_lock);
	gw_addr_v6 = ire->ire_gateway_addr_v6;
	mutex_exit(&ire->ire_lock);

	(void) mi_mpprintf((mblk_t *)mp,
	    MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR
	    "%05d %05ld %06ld %08d %03d %06d %09d %09d %06d %08d "
	    "%04d %08d %08d %d/%d/%d %s\n\t%s\n\t%s\n\t%s\n\t%s",
	    (void *)ire, (void *)ire->ire_rfq, (void *)ire->ire_stq,
	    ire->ire_max_frag, ire->ire_uinfo.iulp_rtt,
	    ire->ire_uinfo.iulp_rtt_sd, ire->ire_uinfo.iulp_ssthresh, ref,
	    ire->ire_uinfo.iulp_rtomax,
	    (ire->ire_uinfo.iulp_tstamp_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_wscale_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_ecn_ok ? 1: 0),
	    (ire->ire_uinfo.iulp_pmtud_ok ? 1: 0),
	    ire->ire_uinfo.iulp_sack,
	    ire->ire_uinfo.iulp_spipe, ire->ire_uinfo.iulp_rpipe,
	    ib_pkt_count, ire->ire_ob_pkt_count,
	    fo_pkt_count, ip_nv_lookup(ire_nv_tbl, (int)ire->ire_type),
	    inet_ntop(AF_INET6, &ire->ire_addr_v6, buf1, sizeof (buf1)),
	    inet_ntop(AF_INET6, &ire->ire_mask_v6, buf2, sizeof (buf2)),
	    inet_ntop(AF_INET6, &ire->ire_src_addr_v6, buf3, sizeof (buf3)),
	    inet_ntop(AF_INET6, &gw_addr_v6, buf4, sizeof (buf4)));
}

/*
 * Add an IRE (IRE_DB_TYPE mblk) for the address to the packet.
 * If no ire is found add an IRE_DB_TYPE with ire_type = 0.
 * If there are allocation problems return without it being added.
 * Used for TCP SYN packets.
 */
void
ip_ire_append_v6(mblk_t *mp, const in6_addr_t *addr)
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
	 *  Take our best shot at an IRE.
	 */
	ire = ire_route_lookup_v6(addr, NULL, NULL, 0, NULL, &sire,
	    NULL, (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT));

	/*
	 * We prevent returning IRES with source address INADDR_ANY
	 * as these were temporarily created for sending packets
	 * from endpoints that have IPC_UNSPEC_SRC set.
	 */
	if (ire == NULL ||
	    IN6_IS_ADDR_UNSPECIFIED(&ire->ire_src_addr_v6)) {
		inire->ire_ipversion = IPV6_VERSION;
		inire->ire_type = 0;
	} else {
		bcopy(ire, inire, sizeof (ire_t));
		if (sire != NULL) {
			bcopy(&(sire->ire_uinfo), &(inire->ire_uinfo),
			    sizeof (iulp_t));
		}
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
 * ire_create_v6 is called to allocate and initialize a new IRE.
 *
 * NOTE : This is called as writer sometimes though not required
 * by this function.
 */
ire_t *
ire_create_v6(const in6_addr_t *v6addr, const in6_addr_t *v6mask,
    const in6_addr_t *v6src_addr, const in6_addr_t *v6gateway, uint_t max_frag,
    mblk_t *fp_mp, queue_t *rfq, queue_t *stq, ushort_t type,
    mblk_t *dlureq_mp, ipif_t *ipif, const in6_addr_t *v6cmask,
    uint32_t phandle, uint32_t ihandle, uint_t flags, const iulp_t *ulp_info)
{
	static	ire_t	ire_null;
	ire_t	*ire;
	mblk_t	*mp;

	ASSERT(!IN6_IS_ADDR_V4MAPPED(v6addr));

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

	/* Allocate the new IRE. */
	mp = allocb(sizeof (ire_t), BPRI_MED);
	if (mp == NULL) {
		if (fp_mp != NULL)
			freeb(fp_mp);
		if (dlureq_mp != NULL)
			freeb(dlureq_mp);
		return (NULL);
	}

	BUMP_IRE_STATS(ire_stats_v6, ire_stats_alloced);

	ire = (ire_t *)mp->b_rptr;
	mp->b_wptr = (uchar_t *)&ire[1];

	/* Start clean. */
	*ire = ire_null;

	/*
	 * Initialize the atomic ident field, using a possibly environment-
	 * specific macro.
	 */
	ire->ire_mp = mp;
	mp->b_datap->db_type = IRE_DB_TYPE;

	ire->ire_addr_v6 = *v6addr;

	if (v6src_addr)
		ire->ire_src_addr_v6 = *v6src_addr;
	if (v6mask) {
		ire->ire_mask_v6 = *v6mask;
		ire->ire_masklen = ip_mask_to_index_v6(&ire->ire_mask_v6);
	}
	if (v6gateway)
		ire->ire_gateway_addr_v6 = *v6gateway;
	if (type == IRE_CACHE && v6cmask != NULL)
		ire->ire_cmask_v6 = *v6cmask;

	return (ire_create_common(ire, max_frag, fp_mp, rfq,
	    stq, type, dlureq_mp, ipif, phandle, ihandle, flags,
	    IPV6_VERSION, ulp_info));
}

/*
 * ire_lookup_loop_multi_v6: Find an IRE_INTERFACE for the group.
 * Allows different routes for multicast addresses
 * in the unicast routing table (akin to FF::0/8 but could be more specific)
 * which point at different interfaces. This is used when IPV6_MULTICAST_IF
 * isn't specified (when sending) and when IPV6_JOIN_GROUP doesn't
 * specify the interface to join on.
 *
 * Supports link-local addresses by following the ipif/ill when recursing.
 */
ire_t *
ire_lookup_loop_multi_v6(const in6_addr_t *group)
{
	ire_t	*ire;
	ipif_t	*ipif = NULL;
	int	match_flags = MATCH_IRE_TYPE;
	in6_addr_t gw_addr_v6;

	ire = ire_ftable_lookup_v6(group, 0, 0, 0, NULL, NULL, NULL, 0,
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
			mutex_enter(&ire->ire_lock);
			gw_addr_v6 = ire->ire_gateway_addr_v6;
			mutex_exit(&ire->ire_lock);
			ire_refrele(ire);
			ire = ire_ftable_lookup_v6(&gw_addr_v6, 0, 0,
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
ire_lookup_local_v6(void)
{
	ire_t	*ire;
	irb_t	*irb;
	ire_t	*maybe = NULL;
	int i;

	for (i = 0; i < IP6_CACHE_TABLE_SIZE;  i++) {
		irb = &ip_cache_table_v6[i];
		if (irb->irb_ire == NULL)
			continue;
		rw_enter(&irb->irb_lock, RW_READER);
		for (ire = irb->irb_ire; ire; ire = ire->ire_next) {
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
ire_pkt_count_v6(ire_t *ire, char *ippc_arg)
{
	ippc_t	*ippc = (ippc_t *)ippc_arg;

	if (IN6_ARE_ADDR_EQUAL(&ire->ire_src_addr_v6, &ippc->ippc_v6addr)) {
		ASSERT(!(ire->ire_type & IRE_BROADCAST));
		/* "inbound" to a non local address is a forward */
		if (ire->ire_type & IRE_LOCAL)
			ippc->ippc_ib_pkt_count += ire->ire_ib_pkt_count;
		else
			ippc->ippc_fo_pkt_count += ire->ire_ib_pkt_count;
		ippc->ippc_ob_pkt_count += ire->ire_ob_pkt_count;
	}
}

/*
 * This function takes a mask and returns
 * number of bits set in the mask. If no
 * bit is set it returns 0.
 * Assumes a contigious mask.
 */
int
ip_mask_to_index_v6(const in6_addr_t *v6mask)
{
	uint8_t		bits;
	uint32_t	mask;
	int		i;

	if (v6mask->s6_addr32[3] == 0xffffffff) /* check for all ones */
		return (IPV6_ABITS);

	/* Find number of words with 32 ones */
	bits = 0;
	for (i = 0; i < 4; i++) {
		if (v6mask->s6_addr32[i] == 0xffffffff) {
			bits += 32;
			continue;
		}
		break;
	}

	ASSERT(bits != IPV6_ABITS);
	/*
	 * Find number of bits in the last word by searching
	 * for the first one from the right
	 */
	mask = ntohl(v6mask->s6_addr32[i]);
	for (i = 0; i < 32; i++) {
	    if (mask & (1 << i))
		return (bits + 32 - i);
	}
	return (bits);
}

/*
 * Convert length for a mask to the mask.
 * Returns the argument bitmask.
 */
in6_addr_t *
ip_index_to_mask_v6(uint_t masklen, in6_addr_t *bitmask)
{
	uint32_t *ptr;

	ASSERT(masklen <= IPV6_ABITS);
	*bitmask = ipv6_all_zeros;

	ptr = (uint32_t *)bitmask;
	while (masklen > 32) {
		*ptr++ = 0xffffffffU;
		masklen -= 32;
	}
	*ptr = htonl(0xffffffffU << (32 - masklen));
	return (bitmask);
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
ire_t *
ire_add_v6(ire_t *ire)
{
	ire_t	*ire1;
	int	mask_table_index;
	irb_t	*irb_ptr;
	ire_t	**irep;
	int	flags;

	ASSERT(ire->ire_ipversion == IPV6_VERSION);
	ASSERT(ire->ire_mp == NULL); /* Calls should go through ire_add */
	ASSERT(ire->ire_nce == NULL);

	/* Find the appropriate list head. */
	switch (ire->ire_type) {
	case IRE_HOST:
	case IRE_HOST_REDIRECT:
		ire->ire_mask_v6 = ipv6_all_ones;
		ire->ire_masklen = IPV6_ABITS;
		ire->ire_src_addr_v6 = ipv6_all_zeros;
		break;
	case IRE_CACHE:
	case IRE_LOCAL:
	case IRE_LOOPBACK:
		ire->ire_mask_v6 = ipv6_all_ones;
		ire->ire_masklen = IPV6_ABITS;
		break;
	case IRE_PREFIX:
		ire->ire_src_addr_v6 = ipv6_all_zeros;
		break;
	case IRE_DEFAULT:
		ire->ire_src_addr_v6 = ipv6_all_zeros;
		break;
	case IRE_IF_RESOLVER:
	case IRE_IF_NORESOLVER:
		break;
	default:
		printf("ire_add_v6: ire %p has unrecognized IRE type (%d)\n",
		    (void *)ire, ire->ire_type);
		ire_delete(ire);
		return (NULL);
	}

	/* Make sure the address is properly masked. */
	V6_MASK_COPY(ire->ire_addr_v6, ire->ire_mask_v6, ire->ire_addr_v6);

	if ((ire->ire_type & IRE_CACHETABLE) == 0) {
		/* IRE goes into Forward Table */
		mask_table_index = ip_mask_to_index_v6(&ire->ire_mask_v6);
		if ((ip_forwarding_table_v6[mask_table_index]) == NULL) {
			irb_t *ptr;
			int i;

			ptr = (irb_t *)mi_zalloc((IP6_FTABLE_HASH_SIZE *
			    sizeof (irb_t)));
			if (ptr == NULL) {
				ire_delete(ire);
				return (NULL);
			}
			for (i = 0; i < IP6_FTABLE_HASH_SIZE; i++) {
				rw_init(&ptr[i].irb_lock, NULL,
				    RW_DEFAULT, NULL);
			}
			mutex_enter(&ire_ft_init_lock);
			if (ip_forwarding_table_v6[mask_table_index] == NULL) {
				ip_forwarding_table_v6[mask_table_index] = ptr;
				mutex_exit(&ire_ft_init_lock);
			} else {
				/*
				 * Some other thread won the race in
				 * initializing the forwarding table at the
				 * same index.
				 */
				mutex_exit(&ire_ft_init_lock);
				for (i = 0; i < IP6_FTABLE_HASH_SIZE; i++) {
					rw_destroy(&ptr[i].irb_lock);
				}
				mi_free(ptr);
			}
		}
		irb_ptr = &(ip_forwarding_table_v6[mask_table_index][
		    IRE_ADDR_MASK_HASH_V6(ire->ire_addr_v6, ire->ire_mask_v6,
		    IP6_FTABLE_HASH_SIZE)]);
	} else {
		irb_ptr = &(ip_cache_table_v6[IRE_ADDR_HASH_V6(
		    ire->ire_addr_v6, IP6_CACHE_TABLE_SIZE)]);
	}

	flags = (MATCH_IRE_MASK | MATCH_IRE_TYPE | MATCH_IRE_GW);
	if (ire->ire_ipif != NULL)
		flags |= MATCH_IRE_IPIF;

	/*
	 * Atomically check for duplicate and insert in the table.
	 */
	rw_enter(&irb_ptr->irb_lock, RW_WRITER);
	for (ire1 = irb_ptr->irb_ire; ire1 != NULL; ire1 = ire1->ire_next) {
		if (ire1->ire_marks & IRE_MARK_CONDEMNED)
			continue;
		if (ire_match_args_v6(ire1, &ire->ire_addr_v6,
		    &ire->ire_mask_v6, &ire->ire_gateway_addr_v6,
		    ire->ire_type, ire->ire_ipif, ire->ire_stq, 0, flags)) {
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
			return (ire1);
		}
	}
	if (ire->ire_type == IRE_CACHE) {
		in6_addr_t gw_addr_v6;
		ill_t	*ill = ire_to_ill(ire);
		char	buf[INET6_ADDRSTRLEN];
		nce_t	*nce;

		/*
		 * All IRE_CACHE types must have a nce.  If this is
		 * not the case the entry will not be added. We need
		 * to make sure that if somebody deletes the nce
		 * after we looked up, they will find this ire and
		 * delete the ire. To delete this ire one needs the
		 * bucket lock which we are still holding here. So,
		 * even if the nce gets deleted after we looked up,
		 * this ire  will get deleted.
		 *
		 * NOTE : Don't need the ire_lock for accessing
		 * ire_gateway_addr_v6 as it is appearing first
		 * time on the list and rts_setgwr_v6 could not
		 * be changing this.
		 */
		gw_addr_v6 = ire->ire_gateway_addr_v6;
		if (IN6_IS_ADDR_UNSPECIFIED(&gw_addr_v6)) {
			nce = ndp_lookup(ill, &ire->ire_addr_v6);
		} else {
			nce = ndp_lookup(ill, &gw_addr_v6);
		}
		if (nce == NULL || nce->nce_state == ND_INCOMPLETE) {
			rw_exit(&irb_ptr->irb_lock);
			ip1dbg(("ire_add_v6: No nce for dst %s \n",
			    inet_ntop(AF_INET6, &ire->ire_addr_v6,
			    buf, sizeof (buf))));
			ire_delete(ire);
			if (nce != NULL)
				NCE_REFRELE(nce);
			return (NULL);
		} else {
			ASSERT(nce->nce_state != ND_UNREACHABLE);
			ire->ire_nce = nce;
		}
	}
	/*
	 * Find the first entry that matches ire_addr - provides
	 * tail insertion. *irep will be null if no match.
	 */
	irep = (ire_t **)irb_ptr;
	while ((ire1 = *irep) != NULL &&
	    !IN6_ARE_ADDR_EQUAL(&ire->ire_addr_v6, &ire1->ire_addr_v6))
		irep = &ire1->ire_next;
	ASSERT(!(ire->ire_type & IRE_BROADCAST));

	if (ire->ire_type == IRE_DEFAULT) {
		/*
		 * We keep a count of default gateways which is used when
		 * assigning them as routes.
		 */
		ipv6_ire_default_count++;
		ASSERT(ipv6_ire_default_count != 0); /* Wraparound */
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
	BUMP_IRE_STATS(ire_stats_v6, ire_stats_inserted);
	rw_exit(&irb_ptr->irb_lock);
	if (ire->ire_type != IRE_CACHE)
		ire_flush_cache_v6(ire, IRE_FLUSH_ADD);
	return (ire);
}

/*
 * Search for all HOST REDIRECT routes that are
 * pointing at the specified gateway and
 * delete them. This routine is called only
 * when a default gateway is going away.
 */
void
ire_delete_host_redirects_v6(const in6_addr_t *gateway)
{
	irb_t *irb_ptr;
	irb_t *irb;
	ire_t *ire;
	in6_addr_t gw_addr_v6;
	int i;

	/* get the hash table for HOST routes */
	irb_ptr = ip_forwarding_table_v6[(IP6_MASK_TABLE_SIZE - 1)];
	if (irb_ptr == NULL)
		return;
	for (i = 0; (i < IP6_FTABLE_HASH_SIZE); i++) {
		irb = &irb_ptr[i];
		IRB_REFHOLD(irb);
		for (ire = irb->irb_ire; ire != NULL; ire = ire->ire_next) {
			if (ire->ire_type != IRE_HOST_REDIRECT)
				continue;
			mutex_enter(&ire->ire_lock);
			gw_addr_v6 = ire->ire_gateway_addr_v6;
			mutex_exit(&ire->ire_lock);
			if (IN6_ARE_ADDR_EQUAL(&gw_addr_v6, gateway))
				ire_delete(ire);
		}
		IRB_REFRELE(irb);
	}
}

/*
 * Delete the specified IRE.
 * All calls should use ire_delete().
 * Sometimes called as writer though not required by this function.
 *
 * NOTE : This function is called only if the ire was added
 * in the list.
 */
void
ire_delete_v6(ire_t *ire)
{
	in6_addr_t gw_addr_v6;

	ASSERT(ire->ire_refcnt >= 1);
	ASSERT(ire->ire_ipversion == IPV6_VERSION);

	if (ire->ire_type != IRE_CACHE)
		ire_flush_cache_v6(ire, IRE_FLUSH_DELETE);
	if (ire->ire_type == IRE_DEFAULT) {
		/*
		 * The entry has already been ire_add()ed.
		 * Adjust accounting.
		 */
		ASSERT(ipv6_ire_default_count != 0);
		rw_enter(&ire->ire_bucket->irb_lock, RW_WRITER);
		ipv6_ire_default_count--;
		rw_exit(&ire->ire_bucket->irb_lock);
		/*
		 * when a default gateway is going away
		 * delete all the host redirects pointing at that
		 * gateway.
		 */
		mutex_enter(&ire->ire_lock);
		gw_addr_v6 = ire->ire_gateway_addr_v6;
		mutex_exit(&ire->ire_lock);
		ire_delete_host_redirects_v6(&gw_addr_v6);
	}
}

/*
 * ire_walk routine to delete all IRE_CACHE/IRE_HOST_REDIRECT entries
 * that have a given gateway address.
 */
void
ire_delete_cache_gw_v6(ire_t *ire, char *addr)
{
	in6_addr_t	*gw_addr = (in6_addr_t *)addr;
	char		buf1[INET6_ADDRSTRLEN];
	char		buf2[INET6_ADDRSTRLEN];
	in6_addr_t	ire_gw_addr_v6;

	if (!(ire->ire_type & (IRE_CACHE|IRE_HOST_REDIRECT)))
		return;

	mutex_enter(&ire->ire_lock);
	ire_gw_addr_v6 = ire->ire_gateway_addr_v6;
	mutex_exit(&ire->ire_lock);

	if (IN6_ARE_ADDR_EQUAL(&ire_gw_addr_v6, gw_addr)) {
		ip1dbg(("ire_delete_cache_gw_v6: deleted %s type %d to %s\n",
		    inet_ntop(AF_INET6, &ire->ire_src_addr_v6,
		    buf1, sizeof (buf1)),
		    ire->ire_type,
		    inet_ntop(AF_INET6, &ire_gw_addr_v6,
		    buf2, sizeof (buf2))));
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
ire_flush_cache_v6(ire_t *ire, int flag)
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
		 * This selective flush is
		 * due to the addition of
		 * new IRE.
		 */
		for (i = 0; i < IP6_CACHE_TABLE_SIZE; i++) {
			irb = &ip_cache_table_v6[i];
			if ((cire = irb->irb_ire) == NULL)
				continue;
			IRB_REFHOLD(irb);
			for (cire = irb->irb_ire; cire != NULL;
			    cire = cire->ire_next) {
				if (cire->ire_type != IRE_CACHE)
					continue;
				if (!V6_MASK_EQ_2(cire->ire_addr_v6,
				    ire->ire_mask_v6, ire->ire_addr_v6) ||
				    (ip_mask_to_index_v6(&cire->ire_cmask_v6) >
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
		for (i = 0; i < IP6_CACHE_TABLE_SIZE; i++) {
			irb = &ip_cache_table_v6[i];
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
ire_match_args_v6(ire_t *ire, const in6_addr_t *addr, const in6_addr_t *mask,
    const in6_addr_t *gateway, int type, ipif_t *ipif, queue_t *wrq,
    uint32_t ihandle, int match_flags)
{
	in6_addr_t masked_addr;
	in6_addr_t gw_addr_v6;

	ASSERT(ire->ire_ipversion == IPV6_VERSION);
	ASSERT(addr != NULL);
	ASSERT(mask != NULL);
	ASSERT((!(match_flags & MATCH_IRE_GW)) || gateway != NULL);
	ASSERT((!(match_flags & MATCH_IRE_ILL)) ||
		(ipif != NULL && ipif->ipif_isv6));

	if (match_flags & MATCH_IRE_GW) {
		mutex_enter(&ire->ire_lock);
		gw_addr_v6 = ire->ire_gateway_addr_v6;
		mutex_exit(&ire->ire_lock);
	}

	/* Only used for IRE_BROADCAST */
	ASSERT(!(match_flags & MATCH_IRE_WQ));
	/* No ire_addr_v6 bits set past the mask */
	ASSERT(V6_MASK_EQ(ire->ire_addr_v6, ire->ire_mask_v6,
	    ire->ire_addr_v6));
	V6_MASK_COPY(*addr, *mask, masked_addr);

	if (V6_MASK_EQ(*addr, *mask, ire->ire_addr_v6) &&
	    ((!(match_flags & MATCH_IRE_GW)) ||
		IN6_ARE_ADDR_EQUAL(&gw_addr_v6, gateway)) &&
	    ((!(match_flags & MATCH_IRE_TYPE)) ||
		(ire->ire_type & type)) &&
	    ((!(match_flags & MATCH_IRE_SRC)) ||
		IN6_ARE_ADDR_EQUAL(&ire->ire_src_addr_v6,
		&ipif->ipif_v6src_addr)) &&
	    ((!(match_flags & MATCH_IRE_IPIF)) ||
		(ire->ire_ipif == ipif)) &&
	    ((!(match_flags & MATCH_IRE_WQ)) ||
		(ire->ire_stq == wrq)) &&
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
ire_route_lookup_v6(const in6_addr_t *addr, const in6_addr_t *mask,
    const in6_addr_t *gateway, int type, ipif_t *ipif, ire_t **pire,
    queue_t *wrq, int flags)
{
	ire_t *ire = NULL;

	/*
	 * ire_match_args_v6() will dereference ipif MATCH_IRE_SRC or
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
		ire = ire_ctable_lookup_v6(addr, gateway, type, ipif, wrq,
		    flags);
		if (ire != NULL)
			return (ire);
	}
	if ((flags & MATCH_IRE_TYPE) == 0 || (type & IRE_FORWARDTABLE) != 0) {
		ire = ire_ftable_lookup_v6(addr, mask, gateway, type, ipif,
		    pire, wrq, 0, flags);
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
 * 1) pass mask as ipv6_all_zeros and set MATCH_IRE_MASK in flags field
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
 * Supports link-local addresses by following the ipif/ill when recursing.
 *
 * NOTE : When this function returns NULL, pire has already been released.
 *	  pire is valid only when this function successfully returns an
 *	  ire.
 */
ire_t *
ire_ftable_lookup_v6(const in6_addr_t *addr, const in6_addr_t *mask,
    const in6_addr_t *gateway, int type, ipif_t *ipif, ire_t **pire,
    queue_t *wrq, uint32_t ihandle, int flags)
{
	irb_t *irb_ptr;
	ire_t	*rire;
	ire_t *ire = NULL;
	ire_t	*saved_ire;
	nce_t	*nce;
	int i;
	in6_addr_t gw_addr_v6;

	ASSERT(addr != NULL);
	ASSERT((!(flags & MATCH_IRE_MASK)) || mask != NULL);
	ASSERT((!(flags & MATCH_IRE_GW)) || gateway != NULL);
	ASSERT(ipif == NULL || ipif->ipif_isv6);

	/*
	 * When we return NULL from this function, we should make
	 * sure that *pire is NULL so that the callers will not
	 * wrongly REFRELE the pire.
	 */
	if (pire != NULL)
		*pire = NULL;
	/*
	 * ire_match_args_v6() will dereference ipif MATCH_IRE_SRC or
	 * MATCH_IRE_ILL is set.
	 */
	if ((flags & (MATCH_IRE_SRC | MATCH_IRE_ILL)) &&
	    (ipif == NULL)) {
		return (NULL);
	}

	/*
	 * If the mask is known, the lookup
	 * is simple, if the mask is not known
	 * we need to search.
	 */
	if (flags & MATCH_IRE_MASK) {
		uint_t masklen;

		masklen = ip_mask_to_index_v6(mask);
		if (ip_forwarding_table_v6[masklen] == NULL)
			return (NULL);
		irb_ptr = &(ip_forwarding_table_v6[masklen][
		    IRE_ADDR_MASK_HASH_V6(*addr, *mask, IP6_FTABLE_HASH_SIZE)]);
		rw_enter(&irb_ptr->irb_lock, RW_READER);
		for (ire = irb_ptr->irb_ire; ire != NULL;
		    ire = ire->ire_next) {
			if (ire->ire_marks & IRE_MARK_CONDEMNED)
				continue;
			if (ire_match_args_v6(ire, addr, mask, gateway, type,
			    ipif, wrq, ihandle, flags))
				goto found_ire;
		}
		rw_exit(&irb_ptr->irb_lock);
	} else {
		/*
		 * In this case we don't know the mask, we need to
		 * search the table assuming different mask sizes.
		 * we start with 128 bit mask, we don't allow default here.
		 */
		for (i = (IP6_MASK_TABLE_SIZE - 1); i > 0; i--) {
			in6_addr_t tmpmask;

			if ((ip_forwarding_table_v6[i]) == NULL)
				continue;
			(void) ip_index_to_mask_v6(i, &tmpmask);
			irb_ptr = &ip_forwarding_table_v6[i][
			    IRE_ADDR_MASK_HASH_V6(*addr, tmpmask,
			    IP6_FTABLE_HASH_SIZE)];
			rw_enter(&irb_ptr->irb_lock, RW_READER);
			for (ire = irb_ptr->irb_ire; ire != NULL;
			    ire = ire->ire_next) {
				if (ire->ire_marks & IRE_MARK_CONDEMNED)
					continue;
				if (ire_match_args_v6(ire, addr,
				    &ire->ire_mask_v6, gateway, type, ipif,
				    wrq, ihandle, flags))
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
	saved_ire = NULL;
	if ((flags & (MATCH_IRE_TYPE | MATCH_IRE_MASK)) == MATCH_IRE_TYPE &&
	    (type & (IRE_DEFAULT | IRE_INTERFACE))) {
		if (ip_forwarding_table_v6[0] != NULL) {
			/* addr & mask is zero for defaults */
			irb_ptr = &ip_forwarding_table_v6[0][
			    IRE_ADDR_HASH_V6(ipv6_all_zeros,
			    IP6_FTABLE_HASH_SIZE)];
			rw_enter(&irb_ptr->irb_lock, RW_READER);
			for (ire = irb_ptr->irb_ire; ire != NULL;
			    ire = ire->ire_next) {

				if (ire->ire_marks & IRE_MARK_CONDEMNED)
					continue;

				if (ire_match_args_v6(ire, addr,
				    &ipv6_all_zeros, gateway, type, ipif,
				    wrq, ihandle, flags)) {

					/*
					 * We have something to work with,
					 * if we can find a resolved/reachable
					 * entry, we use that instead, otherwise
					 * we'll just use what we got.
					 */
					if (saved_ire == NULL)
						saved_ire = ire;
					mutex_enter(&ire->ire_lock);
					gw_addr_v6 = ire->ire_gateway_addr_v6;
					mutex_exit(&ire->ire_lock);
					rire = ire_ctable_lookup_v6(
					    &gw_addr_v6,
					    NULL, 0, ire->ire_ipif, NULL,
					    MATCH_IRE_ILL);
					if (rire != NULL) {
						nce = rire->ire_nce;
						if (nce != NULL &&
						    NCE_ISREACHABLE(nce) &&
						    nce->nce_flags &
						    NCE_F_ISROUTER) {
							ire_refrele(rire);
							goto found_ire;
						} else {
							ire_refrele(rire);
						}
					}
				}
			}
			if (saved_ire != NULL) {
				pr_addr_dbg("ire_ftable_lookup_v6: found"
				    " unreachable or non-router gateway %s\n",
				    AF_INET6, &ire->ire_gateway_addr_v6);
				ire = saved_ire;
				goto found_ire;
			}
			rw_exit(&irb_ptr->irb_lock);
		}
	}
	/*
	 * we come here only if no route is found.
	 * see if the default route can be used which is allowed
	 * only if the default matching criteria is specified.
	 * The ipv6_ire_default_count tracks the number of IRE_DEFAULT
	 * entries. However, the ip_forwarding_table_v6[0] also contains
	 * interface routes thus the count can be zero.
	 */
	if ((flags & (MATCH_IRE_DEFAULT | MATCH_IRE_MASK)) ==
	    MATCH_IRE_DEFAULT) {
		uint_t 	g_cnt;
		uint_t	g_index;
		uint_t	index;

		if (ip_forwarding_table_v6[0] == NULL)
			return (NULL);
		irb_ptr = &(ip_forwarding_table_v6[0])[0];

		rw_enter(&irb_ptr->irb_lock, RW_READER);
		ire = irb_ptr->irb_ire;
		if (ire ==  NULL) {
			rw_exit(&irb_ptr->irb_lock);
			return (NULL);
		}

		/*
		 * Store the index, since it can be changed by other threads.
		 */
		if (ipv6_ire_default_count != 0)
			g_index = ipv6_ire_default_index++;

		/*
		 * Round-robin the default routers list looking for a
		 * neighbor that matches the passed in params and
		 * is reachable.  If none found, just return a route
		 * from the default router list if it exists.
		 */
		for (g_cnt = ipv6_ire_default_count; g_cnt != 0; g_cnt--) {
			index = g_index % ipv6_ire_default_count;
			g_index++;
			while (ire->ire_next && index--)
				ire = ire->ire_next;

			if (ire->ire_marks & IRE_MARK_CONDEMNED)
				continue;

			if (saved_ire == NULL)
				saved_ire = ire;
			if (ire_match_args_v6(ire, addr,
			    &ipv6_all_zeros, gateway, type, ipif,
			    wrq, ihandle, flags)) {
				/* See if there is a resolved cache entry */
				mutex_enter(&ire->ire_lock);
				gw_addr_v6 = ire->ire_gateway_addr_v6;
				mutex_exit(&ire->ire_lock);
				rire = ire_ctable_lookup_v6(
				    &gw_addr_v6, NULL, 0,
				    ire->ire_ipif, NULL, MATCH_IRE_ILL);
				if (rire != NULL) {
					nce = rire->ire_nce;
					if (nce != NULL &&
					    NCE_ISREACHABLE(nce) &&
					    nce->nce_flags & NCE_F_ISROUTER) {
						ire_refrele(rire);
						goto found_ire;
					} else if (nce != NULL &&
					    !(nce->nce_flags &
					    NCE_F_ISROUTER)) {
						/*
						 * Make sure we don't use
						 * this ire
						 */
						if (saved_ire == ire)
							saved_ire = NULL;
					}
					ire_refrele(rire);
				}
			}
		}
		if (saved_ire == NULL) {
			/*
			 * If there are no default routes, we return the ire
			 * that was in ip_forwarding_table_v6[0][0].
			 * in.ndpd adds such a route when it does not find
			 * any default routes. Thus, we will send out NS
			 * packets for all off-link destinations.
			 */
			if (ire != NULL)
				goto found_ire;
			rw_exit(&irb_ptr->irb_lock);
			ip1dbg(("ire_ftable_lookup_v6: returning NULL ire"));
			return (NULL);
		}
		ire = saved_ire;
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
	 * or IRE_CACHETABLE type.  If this is a recursive lookup and an
	 * IRE_INTERFACE type was found, return that.  If it was some other
	 * IRE_FORWARDTABLE type of IRE (one of the prefix types), then it
	 * is necessary to fill in the  parent IRE pointed to by pire, and
	 * then lookup the gateway address of  the parent.  For backwards
	 * compatiblity, if this lookup returns an
	 * IRE other than a IRE_CACHETABLE or IRE_INTERFACE, then one more level
	 * of lookup is done.
	 */
	if (flags & MATCH_IRE_RECURSIVE) {
		ipif_t	*gw_ipif;
		int match_flags = MATCH_IRE_DSTONLY;

		ASSERT((ire->ire_marks & IRE_MARK_CONDEMNED) == 0);
		IRE_REFHOLD(ire);
		rw_exit(&irb_ptr->irb_lock);
		if (ire->ire_type & IRE_INTERFACE)
			return (ire);
		if (ire->ire_ipif != NULL)
			match_flags |= MATCH_IRE_ILL;
		if (pire != NULL)
			*pire = ire;
		/*
		 * If we can't find an IRE_INTERFACE or the caller has not
		 * asked for pire, we need to REFRELE the saved_ire.
		 */
		saved_ire = ire;

		mutex_enter(&ire->ire_lock);
		gw_addr_v6 = ire->ire_gateway_addr_v6;
		mutex_exit(&ire->ire_lock);

		ire = ire_route_lookup_v6(&gw_addr_v6, NULL,
		    NULL, 0, ire->ire_ipif, NULL, wrq, match_flags);
		if (ire == NULL) {
			ire_refrele(saved_ire);
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
				ire_refrele(saved_ire);
			}
			return (ire);
		}
		match_flags |= MATCH_IRE_TYPE;
		mutex_enter(&ire->ire_lock);
		gw_addr_v6 = ire->ire_gateway_addr_v6;
		mutex_exit(&ire->ire_lock);
		gw_ipif = ire->ire_ipif;
		ire_refrele(ire);
		ire = ire_route_lookup_v6(&gw_addr_v6, NULL, NULL,
		    (IRE_CACHETABLE | IRE_INTERFACE), gw_ipif, NULL, wrq,
		    match_flags);
		if (ire == NULL) {
			ire_refrele(saved_ire);
			if (pire != NULL)
				*pire = NULL;
			return (NULL);
		} else if (pire == NULL) {
			/*
			 * If the caller did not ask for pire, release
			 * it now.
			 */
			ire_refrele(saved_ire);
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
ire_ctable_lookup_v6(const in6_addr_t *addr, const in6_addr_t *gateway,
    int type, ipif_t *ipif, queue_t *wrq, int flags)
{
	ire_t *ire;
	irb_t *irb_ptr;
	ASSERT(addr != NULL);
	ASSERT((!(flags & MATCH_IRE_GW)) || gateway != NULL);

	/*
	 * ire_match_args_v6() will dereference ipif MATCH_IRE_SRC or
	 * MATCH_IRE_ILL is set.
	 */
	if ((flags & (MATCH_IRE_SRC |  MATCH_IRE_ILL)) &&
	    (ipif == NULL))
		return (NULL);

	irb_ptr = &ip_cache_table_v6[IRE_ADDR_HASH_V6(*addr,
	    IP6_CACHE_TABLE_SIZE)];
	rw_enter(&irb_ptr->irb_lock, RW_READER);
	for (ire = irb_ptr->irb_ire; ire; ire = ire->ire_next) {
		if (ire->ire_marks & IRE_MARK_CONDEMNED)
			continue;
		ASSERT(IN6_ARE_ADDR_EQUAL(&ire->ire_mask_v6, &ipv6_all_ones));
		if (ire_match_args_v6(ire, addr, &ire->ire_mask_v6, gateway,
		    type, ipif, wrq, 0, flags)) {
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
ire_cache_lookup_v6(const in6_addr_t *addr)
{
	irb_t *irb_ptr;
	ire_t *ire;

	irb_ptr = &ip_cache_table_v6[IRE_ADDR_HASH_V6(*addr,
	    IP6_CACHE_TABLE_SIZE)];
	rw_enter(&irb_ptr->irb_lock, RW_READER);
	for (ire = irb_ptr->irb_ire; ire; ire = ire->ire_next) {
		if (ire->ire_marks & IRE_MARK_CONDEMNED)
			continue;
		if (IN6_ARE_ADDR_EQUAL(&ire->ire_addr_v6, addr)) {
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
 * as found by ip_newroute_v6(). We are called from ip_newroute_v6() in
 * the IRE_CACHE case.
 */
ire_t *
ire_ihandle_lookup_offlink_v6(ire_t *cire, ire_t *pire)
{
	ire_t	*ire;
	int	match_flags;
	in6_addr_t	gw_addr;
	ipif_t		*gw_ipif;

	ASSERT(cire != NULL && pire != NULL);

	match_flags =  MATCH_IRE_TYPE | MATCH_IRE_IHANDLE | MATCH_IRE_MASK;
	if (pire->ire_ipif != NULL)
		match_flags |= MATCH_IRE_ILL;
	/*
	 * We know that the mask of the interface ire equals cire->ire_cmask.
	 * (When ip_newroute_v6() created 'cire' for an on-link destn. it set
	 * its cmask from the interface ire's mask)
	 */
	ire = ire_ftable_lookup_v6(&cire->ire_addr_v6, &cire->ire_cmask_v6, 0,
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
	 * ire_ftable_lookup_v6 above would have failed, since there is no
	 * interface ire to reach gw1. We will fallthru below.
	 *
	 * Here we duplicate the steps that ire_ftable_lookup_v6() did in
	 * getting 'cire' from 'pire', in the MATCH_IRE_RECURSIVE case.
	 * The differences are the following
	 * i.   We want the interface ire only, so we call
	 *	ire_ftable_lookup_v6() instead of ire_route_lookup_v6()
	 * ii.  We look for only prefix routes in the 1st call below.
	 * ii.  We want to match on the ihandle in the 2nd call below.
	 */
	match_flags =  MATCH_IRE_TYPE;
	if (pire->ire_ipif != NULL)
		match_flags |= MATCH_IRE_ILL;

	mutex_enter(&pire->ire_lock);
	gw_addr = pire->ire_gateway_addr_v6;
	mutex_exit(&pire->ire_lock);
	ire = ire_ftable_lookup_v6(&gw_addr, 0, 0, IRE_OFFSUBNET,
	    pire->ire_ipif, NULL, NULL, 0, match_flags);
	if (ire == NULL)
		return (NULL);
	/*
	 * At this point 'ire' corresponds to the entry shown in line 2.
	 * gw_addr is 'gw2' in the example above.
	 */
	mutex_enter(&ire->ire_lock);
	gw_addr = ire->ire_gateway_addr_v6;
	mutex_exit(&ire->ire_lock);
	gw_ipif = ire->ire_ipif;
	ire_refrele(ire);

	match_flags |= MATCH_IRE_IHANDLE;
	ire = ire_ftable_lookup_v6(&gw_addr, 0, 0, IRE_INTERFACE,
	    gw_ipif, NULL, NULL, cire->ire_ihandle, match_flags);
	return (ire);
}

/*
 * Return the IRE_LOOPBACK, IRE_IF_RESOLVER or IRE_IF_NORESOLVER
 * ire associated with the specified ipif.
 *
 * This might occasionally be called when IFF_UP is not set since
 * the IPV6_MULTICAST_IF as well as creating interface routes
 * allows specifying a down ipif (ipif_lookup* match ipifs that are down).
 *
 * Note that if IFF_NOLOCAL, IFF_NOXMIT, or IFF_DEPRECATED is set on the ipif
 * this routine might return NULL.
 * (Sometimes called as writer though not required by this function.)
 */
ire_t *
ipif_to_ire_v6(ipif_t *ipif)
{
	ire_t	*ire;

	ASSERT(ipif->ipif_isv6);
	if (ipif->ipif_ire_type == IRE_LOOPBACK) {
		ire = ire_ctable_lookup_v6(&ipif->ipif_v6lcl_addr, NULL,
		    IRE_LOOPBACK, ipif, NULL,
		    (MATCH_IRE_TYPE | MATCH_IRE_IPIF));
	} else if (ipif->ipif_flags & IFF_POINTOPOINT) {
		/* In this case we need to lookup destination address. */
		ire = ire_ftable_lookup_v6(&ipif->ipif_v6pp_dst_addr,
		    &ipv6_all_ones, NULL, IRE_INTERFACE, ipif, NULL, NULL, 0,
		    (MATCH_IRE_TYPE | MATCH_IRE_IPIF | MATCH_IRE_MASK));
	} else {
		ire = ire_ftable_lookup_v6(&ipif->ipif_v6subnet,
		    &ipif->ipif_v6net_mask, NULL, IRE_INTERFACE, ipif, NULL,
		    NULL, 0, (MATCH_IRE_TYPE | MATCH_IRE_IPIF |
		    MATCH_IRE_MASK));
	}
	return (ire);
}
