/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip6_if.c	1.15	99/11/07 SMI"

/*
 * This file contains the interface control functions for IPv6.
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/kstat.h>
#include <sys/debug.h>

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/socket.h>
#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>
#include <sys/isa_defs.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <sys/strick.h>
#include <netinet/in.h>
#include <netinet/igmp_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/mib2.h>
#include <inet/arp.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/ip_multi.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>
#include <inet/ip_ndp.h>
#include <inet/ip_if.h>

#include <netinet/igmp.h>
#include <netinet/ip_mroute.h>

static	uint8_t	ipv6_g_phys_multi_addr[] =
		{0x33, 0x33, 0x00, 0x00, 0x00, 0x00};

#define	IP_PHYS_MULTI_ADDR_LENGTH	sizeof (ipv6_g_phys_multi_addr)

static in6_addr_t	ipv6_ll_template =
			{(uint32_t)V6_LINKLOCAL, 0x0, 0x0, 0x0};

#define	IP_NDP_HW_MAPPING_START		2

/*
 * ipif_lookup_group_v6
 */
ipif_t *
ipif_lookup_group_v6(const in6_addr_t *group)
{
	ire_t	*ire;
	ipif_t	*ipif;

	ire = ire_lookup_loop_multi_v6(group);
	if (ire == NULL)
		return (NULL);
	ipif = ire->ire_ipif;
	ire_refrele(ire);
	return (ipif);
}

/*
 * Look for an ipif with the specified interface address and destination.
 * The destination address is used only for matching point-to-point interfaces.
 */
ipif_t *
ipif_lookup_interface_v6(const in6_addr_t *if_addr, const in6_addr_t *dst)
{
	ipif_t	*ipif;
	ill_t	*ill;

	/*
	 * First match all the point-to-point interfaces
	 * before looking at non-point-to-point interfaces.
	 * This is done to avoid returning non-point-to-point
	 * ipif instead of unnumbered point-to-point ipif.
	 */
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		if (!ill->ill_isv6)
			continue;
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			/* Allow the ipif to be down */
			if ((ipif->ipif_flags & IFF_POINTOPOINT) &&
			    (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6lcl_addr,
			    if_addr)) &&
			    (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6pp_dst_addr,
			    dst))) {
				return (ipif);
			}
		}
	}
	/* lookup the ipif based on interface address */
	ipif = ipif_lookup_addr_v6(if_addr, NULL);
	ASSERT(ipif == NULL || ipif->ipif_isv6);
	return (ipif);
}

/*
 * Look for an ipif with the specified address. For point-point links
 * we look for matches on either the destination address and the local
 * address, but we ignore the check on the local address if IFF_UNNUMBERED
 * is set.
 * Matches on a specific ill if match_ill is set.
 */
ipif_t *
ipif_lookup_addr_v6(const in6_addr_t *addr, ill_t *match_ill)
{
	ipif_t	*ipif;
	ipif_t	*fallback;
	ill_t	*ill;

	fallback = NULL;
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		if (!ill->ill_isv6)
			continue;
		if (match_ill != NULL && ill != match_ill)
			continue;
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			/* Allow the ipif to be down */
			if (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6lcl_addr, addr) &&
			    (ipif->ipif_flags & IFF_UNNUMBERED) == 0) {
				return (ipif);
			}
			if (ipif->ipif_flags & IFF_POINTOPOINT &&
			    IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6pp_dst_addr,
			    addr)) {
				fallback = ipif;
			}
		}
	}
	return (fallback);
}

/*
 * Look for an ipif that matches the specified remote address i.e. the
 * ipif that would receive the specified packet.
 * First look for directly connected interfaces and then do a recursive
 * IRE lookup and pick the first ipif corresponding to the local address in the
 * ire.
 */
ipif_t *
ipif_lookup_remote_v6(ill_t *ill, const in6_addr_t *addr)
{
	ipif_t	*ipif;
	ire_t	*ire;

	ASSERT(ill->ill_isv6);

	if (IN6_IS_ADDR_LINKLOCAL(addr)) {
		return (ill->ill_ipif);
	}

	for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
		/* Allow the ipif to be down */
		if (ipif->ipif_flags & IFF_POINTOPOINT) {
			if (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6pp_dst_addr,
			    addr)) {
				return (ipif);
			}
			if (!(ipif->ipif_flags & IFF_UNNUMBERED) &&
			    IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6lcl_addr, addr)) {
				return (ipif);
			}
		} else if (V6_MASK_EQ(*addr, ipif->ipif_v6net_mask,
			    ipif->ipif_v6subnet)) {
				return (ipif);
		}
	}
	ire = ire_route_lookup_v6(addr, 0, 0, 0, NULL, NULL, NULL,
	    MATCH_IRE_RECURSIVE);
	if (ire != NULL) {
		ipif = ire->ire_ipif;
		ire_refrele(ire);
		if (ipif != NULL) {
			ASSERT(ipif->ipif_isv6);
			return (ipif);
		}
	}
	/* Pick the first interface */
	return (ill->ill_ipif);
}

/*
 * Perform various checks to verify that an address would make sense as a local
 * interface address.  This is currently only called when an attempt is made
 * to set a local address.
 *
 * Does not allow a v4-mapped address, an address that equals the subnet
 * anycast address, ... a multicast address, ...
 */
boolean_t
ip_local_addr_ok_v6(const in6_addr_t *addr, const in6_addr_t *subnet_mask)
{
	in6_addr_t subnet;

	if (IN6_IS_ADDR_UNSPECIFIED(addr))
		return (B_TRUE);	/* Allow all zeros */

	/*
	 * Don't allow all zeroes or host part, but allow
	 * all ones netmask.
	 */
	V6_MASK_COPY(*addr, *subnet_mask, subnet);
	if (IN6_IS_ADDR_V4MAPPED(addr) ||
	    (IN6_ARE_ADDR_EQUAL(addr, &subnet) &&
	    !IN6_ARE_ADDR_EQUAL(subnet_mask, &ipv6_all_ones)) ||
	    (IN6_IS_ADDR_V4COMPAT(addr) && CLASSD(V4_PART_OF_V6((*addr)))) ||
	    IN6_IS_ADDR_MULTICAST(addr))
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * Perform various checks to verify that an address would make sense as a
 * remote/subnet interface address.
 */
boolean_t
ip_remote_addr_ok_v6(const in6_addr_t *addr, const in6_addr_t *subnet_mask)
{
	in6_addr_t subnet;

	if (IN6_IS_ADDR_UNSPECIFIED(addr))
		return (B_TRUE);	/* Allow all zeros */

	V6_MASK_COPY(*addr, *subnet_mask, subnet);
	if (IN6_IS_ADDR_V4MAPPED(addr) ||
	    (IN6_ARE_ADDR_EQUAL(addr, &subnet) &&
	    !IN6_ARE_ADDR_EQUAL(subnet_mask, &ipv6_all_ones)) ||
	    IN6_IS_ADDR_MULTICAST(addr) ||
	    (IN6_IS_ADDR_V4COMPAT(addr) && CLASSD(V4_PART_OF_V6((*addr)))))
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * ip_rt_add_v6 is called to add an IPv6 route to the forwarding table.
 * ipif_arg is passed in to associate it with the correct interface
 * (for link-local destinations and gateways).
 */
/* ARGSUSED1 */
int
ip_rt_add_v6(const in6_addr_t *dst_addr, const in6_addr_t *mask,
    const in6_addr_t *gw_addr, int flags, ipif_t *ipif_arg, ire_t **ire_arg)
{
	ire_t	*ire;
	ire_t	*gw_ire;
	ipif_t	*ipif;
	uint_t	type;
	int	match_flags = MATCH_IRE_TYPE;

	if (ire_arg != NULL)
		*ire_arg = NULL;

	/*
	 * Prevent routes with a zero gateway from being created (since
	 * interfaces can currently be plumbed and brought up with no assigned
	 * address).
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(gw_addr))
		return (ENETUNREACH);

	/*
	 * If this is the case of RTF_HOST being set, then we set the netmask
	 * to all ones (regardless if one was supplied).
	 */
	if (flags & RTF_HOST)
		mask = &ipv6_all_ones;

	/*
	 * Get the ipif, if any, corresponding to the gw_addr
	 */
	ipif = ipif_lookup_interface_v6(gw_addr, dst_addr);

	/*
	 * GateD will attempt to create routes with a loopback interface
	 * address as the gateway and with RTF_GATEWAY set.  We allow
	 * these routes to be added, but create them as interface routes
	 * since the gateway is an interface address.
	 */
	if ((ipif != NULL) && (ipif->ipif_ire_type == IRE_LOOPBACK))
		flags &= ~RTF_GATEWAY;

	/*
	 * Traditionally, interface routes are ones where RTF_GATEWAY isn't set
	 * and the gateway address provided is one of the system's interface
	 * addresses.  By using the routing socket interface and supplying an
	 * RTA_IFP sockaddr with an interface index, an alternate method of
	 * specifying an interface route to be created is available which uses
	 * the interface index that specifies the outgoing interface rather than
	 * the address of an outgoing interface (which may not be able to
	 * uniquely identify an interface).  When coupled with the RTF_GATEWAY
	 * flag, routes can be specified which not only specify the next-hop to
	 * be used when routing to a certain prefix, but also which outgoing
	 * interface should be used.
	 *
	 * Previously, interfaces would have unique addresses assigned to them
	 * and so the address assigned to a particular interface could be used
	 * to identify a particular interface.  One exception to this was the
	 * case of an unnumbered interface (where IFF_UNNUMBERED was set).
	 *
	 * With the advent of IPv6 and its link-local addresses, this
	 * restriction was relaxed and interfaces could share addresses between
	 * themselves.  In fact, typically all of the link-local interfaces on
	 * an IPv6 node or router will have the same link-local address.  In
	 * order to differentiate between these interfaces, the use of an
	 * interface index is necessary and this index can be carried inside a
	 * RTA_IFP sockaddr (which is actually a sockaddr_dl).  One restriction
	 * of using the interface index, however, is that all of the ipif's that
	 * are part of an ill have the same index and so the RTA_IFP sockaddr
	 * cannot be used to differentiate between ipif's (or logical
	 * interfaces) that belong to the same ill (physical interface).
	 *
	 * For example, in the following case involving IPv4 interfaces and
	 * logical interfaces
	 *
	 *	192.0.2.32	255.255.255.224	192.0.2.33	U	if0
	 *	192.0.2.32	255.255.255.224	192.0.2.34	U	if0:1
	 *	192.0.2.32	255.255.255.224	192.0.2.35	U	if0:2
	 *
	 * the ipif's corresponding to each of these interface routes can be
	 * uniquely identified by the "gateway" (actually interface address).
	 *
	 * In this case involving multiple IPv6 default routes to a particular
	 * link-local gateway, the use of RTA_IFP is necessary to specify which
	 * default route is of interest:
	 *
	 *	default		fe80::123:4567:89ab:cdef	U	if0
	 *	default		fe80::123:4567:89ab:cdef	U	if1
	 */

	/* RTF_GATEWAY not set */
	if (!(flags & RTF_GATEWAY)) {
		queue_t	*stq;

		/*
		 * As the interface index specified with the RTA_IFP sockaddr is
		 * the same for all ipif's off of an ill, the matching logic
		 * below uses MATCH_IRE_ILL if such an index was specified.
		 * This means that routes sharing the same prefix when added
		 * using a RTA_IFP sockaddr must have distinct interface
		 * indices (namely, they must be on distinct ill's).
		 *
		 * On the other hand, since the gateway address will usually be
		 * different for each ipif on the system, the matching logic
		 * uses MATCH_IRE_IPIF in the case of a traditional interface
		 * route.  This means that interface routes for the same prefix
		 * can be created if they belong to distinct ipif's and if a
		 * RTA_IFP sockaddr is not present.
		 */
		if (ipif_arg != NULL) {
			ipif = ipif_arg;
			match_flags |= MATCH_IRE_ILL;
		} else {
			/*
			 * Check the ipif corresponding to the gw_addr
			 */
			if (ipif == NULL)
				return (ENETUNREACH);
			match_flags |= MATCH_IRE_IPIF;
		}

		/*
		 * We check for an existing entry at this point.
		 */
		match_flags |= MATCH_IRE_MASK;
		ire = ire_ftable_lookup_v6(dst_addr, mask, 0, IRE_INTERFACE,
		    ipif, NULL, NULL, 0, match_flags);
		if (ire != NULL) {
			ire_refrele(ire);
			return (EEXIST);
		}

		stq = (ipif->ipif_net_type == IRE_IF_RESOLVER)
		    ? ipif->ipif_rq : ipif->ipif_wq;

		/*
		 * Create a copy of the IRE_LOOPBACK, IRE_IF_NORESOLVER or
		 * IRE_IF_RESOLVER with the modified address and netmask.
		 */
		ire = ire_create_v6(
		    dst_addr,
		    mask,
		    &ipif->ipif_v6src_addr,
		    NULL,
		    ipif->ipif_mtu,
		    NULL,
		    NULL,
		    stq,
		    ipif->ipif_net_type,
		    ipif->ipif_resolver_mp,
		    ipif,
		    NULL,
		    0,
		    0,
		    flags,
		    &ire_uinfo_null);
		if (ire == NULL)
			return (ENOMEM);

		/*
		 * Some software (for example, GateD and Sun Cluster) attempts
		 * to create (what amount to) IRE_PREFIX routes with the
		 * loopback address as the gateway.  This is primarily done to
		 * set up prefixes with the RTF_REJECT flag set (for example,
		 * when generating aggregate routes.)
		 *
		 * If the IRE type (as defined by ipif->ipif_net_type) is
		 * IRE_LOOPBACK, then we map the request into a
		 * IRE_IF_NORESOLVER.
		 *
		 * Needless to say, the real IRE_LOOPBACK is NOT created by this
		 * routine, but rather using ire_create_v6() directly.
		 */
		if (ipif->ipif_net_type == IRE_LOOPBACK)
			ire->ire_type = IRE_IF_NORESOLVER;
		ire = ire_add(ire);
		if (ire != NULL)
			goto save_ire;

		/*
		 * In the result of failure, ire_add() will have already
		 * deleted the ire in question, so there is no need to
		 * do that here.  As to the return value, either
		 * ire_add() could not allocate the necessary memory or
		 * the IRE type passed down was an impossible value, so
		 * we choose to return ENOMEM.
		 */
		return (ENOMEM);
	}

	/*
	 * Get an interface IRE for the specified gateway.
	 * If we don't have an IRE_IF_NORESOLVER or IRE_IF_RESOLVER for the
	 * gateway, it is currently unreachable and we fail the request
	 * accordingly.
	 */
	ipif = ipif_arg;
	if (ipif_arg != NULL)
		match_flags |= MATCH_IRE_ILL;
	gw_ire = ire_ftable_lookup_v6(gw_addr, 0, 0, IRE_INTERFACE, ipif_arg,
	    NULL, NULL, 0, match_flags);
	if (gw_ire == NULL)
		return (ENETUNREACH);

	/*
	 * We create one of three types of IREs as a result of this request
	 * based on the netmask.  A netmask of all ones (which is automatically
	 * assumed when RTF_HOST is set) results in an IRE_HOST being created.
	 * An all zeroes netmask implies a default route so an IRE_DEFAULT is
	 * created.  Otherwise, an IRE_PREFIX route is created for the
	 * destination prefix.
	 */
	if (IN6_ARE_ADDR_EQUAL(mask, &ipv6_all_ones))
		type = IRE_HOST;
	else if (IN6_IS_ADDR_UNSPECIFIED(mask))
		type = IRE_DEFAULT;
	else
		type = IRE_PREFIX;

	/* check for a duplicate entry */
	ire = ire_ftable_lookup_v6(dst_addr, mask, gw_addr, type, ipif_arg,
	    NULL, NULL, 0, match_flags | MATCH_IRE_MASK | MATCH_IRE_GW);
	if (ire != NULL) {
		ire_refrele(gw_ire);
		ire_refrele(ire);
		return (EEXIST);
	}

	/* Create the IRE. */
	ire = ire_create_v6(
	    dst_addr,				/* dest address */
	    mask,				/* mask */
	    NULL,				/* no source address */
	    gw_addr,				/* gateway address */
	    gw_ire->ire_max_frag,
	    NULL,				/* no Fast Path header */
	    NULL,				/* no recv-from queue */
	    NULL,				/* no send-to queue */
	    (ushort_t)type,			/* IRE type */
	    NULL,
	    ipif_arg,
	    NULL,
	    0,
	    0,
	    flags,
	    &gw_ire->ire_uinfo);		/* Inherit ULP info from gw */
	ire_refrele(gw_ire);
	if (ire == NULL)
		return (ENOMEM);

	/*
	 * POLICY: should we allow an RTF_HOST with address INADDR_ANY?
	 * SUN/OS socket stuff does but do we really want to allow ::0 ?
	 */

	/* Add the new IRE. */
	ire = ire_add(ire);
	if (ire == NULL) {
		/*
		 * In the result of failure, ire_add() will have already
		 * deleted the ire in question, so there is no need to
		 * do that here.  As to the return value, either
		 * ire_add() could not allocate the necessary memory or
		 * the IRE type passed down was an impossible value, so
		 * we choose to return ENOMEM.
		 */
		return (ENOMEM);
	}

save_ire:
	if (ipif != NULL) {
		mblk_t	*save_mp;

		/*
		 * Save enough information so that we can recreate the IRE if
		 * the interface goes down and then up.  The metrics associated
		 * with the route will be saved as well when rts_setmetrics() is
		 * called after the IRE has been created.  In the case where
		 * memory cannot be allocated, none of this information will be
		 * saved.
		 */
		save_mp = allocb(sizeof (ifrt_t), BPRI_MED);
		if (save_mp != NULL) {
			ifrt_t	*ifrt;

			save_mp->b_wptr += sizeof (ifrt_t);
			ifrt = (ifrt_t *)save_mp->b_rptr;
			bzero(ifrt, sizeof (ifrt_t));
			ifrt->ifrt_type = ire->ire_type;
			ifrt->ifrt_v6addr = ire->ire_addr_v6;
			mutex_enter(&ire->ire_lock);
			ifrt->ifrt_v6gateway_addr = ire->ire_gateway_addr_v6;
			mutex_exit(&ire->ire_lock);
			ifrt->ifrt_v6mask = ire->ire_mask_v6;
			ifrt->ifrt_flags = ire->ire_flags;
			ifrt->ifrt_max_frag = ire->ire_max_frag;
			mutex_enter(&ipif->ipif_saved_ire_lock);
			save_mp->b_cont = ipif->ipif_saved_ire_mp;
			ipif->ipif_saved_ire_mp = save_mp;
			mutex_exit(&ipif->ipif_saved_ire_lock);
		}
	}
	if (ire_arg != NULL) {
		/*
		 * Store the ire that was successfully added into where ire_arg
		 * points to so that callers don't have to look it up
		 * themselves (but they are responsible for ire_refrele()ing
		 * the ire when they are finished with it).
		 */
		*ire_arg = ire;
	} else {
		ire_refrele(ire);		/* Held in ire_add */
	}
	return (0);
}

/*
 * ip_rt_delete_v6 is called to delete an IPv6 route.
 * ipif_arg is passed in to associate it with the correct interface
 * (for link-local destinations and gateways).
 */
/* ARGSUSED4 */
int
ip_rt_delete_v6(const in6_addr_t *dst_addr, const in6_addr_t *mask,
    const in6_addr_t *gw_addr, uint_t rtm_addrs, int flags, ipif_t *ipif_arg)
{
	ire_t	*ire = NULL;
	ipif_t	*ipif;
	uint_t	type;
	uint_t	match_flags = MATCH_IRE_TYPE;

	/*
	 * If this is the case of RTF_HOST being set, then we set the netmask
	 * to all ones.  Otherwise, we use the netmask if one was supplied.
	 */
	if (flags & RTF_HOST) {
		mask = &ipv6_all_ones;
		match_flags |= MATCH_IRE_MASK;
	} else if (rtm_addrs & RTA_NETMASK) {
		match_flags |= MATCH_IRE_MASK;
	}

	/*
	 * Note that RTF_GATEWAY is never set on a delete, therefore
	 * we check if the gateway address is one of our interfaces first,
	 * and fall back on RTF_GATEWAY routes.
	 *
	 * This makes it possible to delete an original
	 * IRE_IF_NORESOLVER/IRE_IF_RESOLVER - consistent with SunOS 4.1.
	 *
	 * As the interface index specified with the RTA_IFP sockaddr is the
	 * same for all ipif's off of an ill, the matching logic below uses
	 * MATCH_IRE_ILL if such an index was specified.  This means a route
	 * sharing the same prefix and interface index as the the route
	 * intended to be deleted might be deleted instead if a RTA_IFP sockaddr
	 * is specified in the request.
	 *
	 * On the other hand, since the gateway address will usually be
	 * different for each ipif on the system, the matching logic
	 * uses MATCH_IRE_IPIF in the case of a traditional interface
	 * route.  This means that interface routes for the same prefix can be
	 * uniquely identified if they belong to distinct ipif's and if a
	 * RTA_IFP sockaddr is not present.
	 *
	 * For more detail on specifying routes by gateway address and by
	 * interface index, see the comments in ip_rt_add_v6().
	 */
	ipif = ipif_lookup_interface_v6(gw_addr, dst_addr);
	if (ipif != NULL) {
		if (ipif_arg != NULL) {
			ipif = ipif_arg;
			match_flags |= MATCH_IRE_ILL;
		} else {
			match_flags |= MATCH_IRE_IPIF;
		}

		if (ipif->ipif_ire_type == IRE_LOOPBACK)
			ire = ire_ctable_lookup_v6(dst_addr, 0, IRE_LOOPBACK,
			    ipif, NULL, match_flags);
		if (ire == NULL)
			ire = ire_ftable_lookup_v6(dst_addr, mask, 0,
			    IRE_INTERFACE, ipif, NULL, NULL, 0, match_flags);
	}
	if (ire == NULL) {
		/*
		 * At this point, the gateway address is not one of our own
		 * addresses or a matching interface route was not found.  We
		 * set the IRE type to lookup based on whether
		 * this is a host route, a default route or just a prefix.
		 *
		 * If an ipif_arg was passed in, then the lookup is based on an
		 * interface index so MATCH_IRE_ILL is added to match_flags.
		 * In any case, MATCH_IRE_IPIF is cleared and MATCH_IRE_GW is
		 * set as the route being looked up is not a traditional
		 * interface route.
		 */
		match_flags &= ~MATCH_IRE_IPIF;
		match_flags |= MATCH_IRE_GW;
		if (ipif_arg != NULL)
			match_flags |= MATCH_IRE_ILL;
		if (IN6_ARE_ADDR_EQUAL(mask, &ipv6_all_ones))
			type = IRE_HOST;
		else if (IN6_IS_ADDR_UNSPECIFIED(mask))
			type = IRE_DEFAULT;
		else
			type = IRE_PREFIX;
		ire = ire_ftable_lookup_v6(dst_addr, mask, gw_addr, type,
		    ipif_arg, NULL, NULL, 0, match_flags);
		if (ire == NULL && type == IRE_HOST) {
			ire = ire_ftable_lookup_v6(dst_addr, mask, gw_addr,
			    IRE_HOST_REDIRECT, ipif_arg, NULL, NULL, 0,
			    match_flags);
		}
	}

	if (ire == NULL)
		return (ESRCH);
	ipif = ire->ire_ipif;
	if (ipif != NULL) {
		mblk_t		**mpp;
		mblk_t		*mp;
		ifrt_t		*ifrt;
		in6_addr_t	gw_addr_v6;

		/* Remove from ipif_saved_ire_mp list if it is there */
		mutex_enter(&ire->ire_lock);
		gw_addr_v6 = ire->ire_gateway_addr_v6;
		mutex_exit(&ire->ire_lock);
		mutex_enter(&ipif->ipif_saved_ire_lock);
		for (mpp = &ipif->ipif_saved_ire_mp; *mpp != NULL;
		    mpp = &(*mpp)->b_cont) {
			/*
			 * On a given ipif, the triple of address, gateway and
			 * mask is unique for each saved IRE (in the case of
			 * ordinary interface routes, the gateway address is
			 * all-zeroes).
			 */
			mp = *mpp;
			ifrt = (ifrt_t *)mp->b_rptr;
			if (IN6_ARE_ADDR_EQUAL(&ifrt->ifrt_v6addr,
			    &ire->ire_addr_v6) &&
			    IN6_ARE_ADDR_EQUAL(&ifrt->ifrt_v6gateway_addr,
			    &gw_addr_v6) &&
			    IN6_ARE_ADDR_EQUAL(&ifrt->ifrt_v6mask,
			    &ire->ire_mask_v6)) {
				*mpp = mp->b_cont;
				freeb(mp);
				break;
			}
		}
		mutex_exit(&ipif->ipif_saved_ire_lock);
	}
	ire_delete(ire);
	ire_refrele(ire);
	return (0);
}

/*
 * Look up the source address index for an address. Return 0 if not found.
 * Used to implement __sin6_src_id
 * Called by the transport modules.
 * XXX implement. Need mutex/rwlock plus separate add/delete functions.
 */
/*ARGSUSED*/
uint_t
ip_srcaddr_to_index_v6(in6_addr_t *src)
{
	return (0);
}

/*
 * Look up the source address for a given index. Return false if not found
 * Used to implement __sin6_src_id
 * Called by the transport modules.
 */
/*ARGSUSED*/
boolean_t
ip_index_to_srcaddr_v6(uint_t index, in6_addr_t *src)
{
	return (B_FALSE);
}

/*
 * Derive a token from the link layer address.
 * Knows about IEEE 802 and IEEE EUI-64 mappings.
 */
boolean_t
ill_setdefaulttoken(ill_t *ill)
{
	int i;

	if (ill->ill_phys_addr_length != 6)
		return (B_FALSE);

	switch (ill->ill_mactype) {
	case DL_FDDI:
	case DL_ETHER: {
		in6_addr_t	v6addr;
		in6_addr_t	v6mask;
		char		*addr;

		/* Form EUI 64 like address */
		addr = (char *)&v6addr.s6_addr32[2];
		bcopy((char *)ill->ill_hw_addr, addr, 3);
		addr[0] ^= 0x2;		/* Toggle Universal/Local bit */
		addr[3] = (char)0xff;
		addr[4] = (char)0xfe;
		bcopy((char *)ill->ill_hw_addr + 3, &addr[5], 3);

		(void) ip_index_to_mask_v6(IPV6_TOKEN_LEN, &v6mask);

		for (i = 0; i < 4; i++)
			v6mask.s6_addr32[i] = v6mask.s6_addr32[i] ^
			    (uint32_t)0xffffffff;

		V6_MASK_COPY(v6addr, v6mask, ill->ill_token);
		ill->ill_token_length = IPV6_TOKEN_LEN;
		return (B_TRUE);
	}
	case DL_CSMACD:
	case DL_TPB:
	case DL_TPR:
	default:
		return (B_FALSE);
	}
}

/*
 * Create a link-local address from a token.
 */
static void
ipif_get_linklocal(in6_addr_t *dest, const in6_addr_t *token)
{
	int i;

	for (i = 0; i < 4; i++) {
		dest->s6_addr32[i] =
		    token->s6_addr32[i] | ipv6_ll_template.s6_addr32[i];
	}
}

/*
 * Set a nice default address for automatic tunnels tsrc/96
 */
static void
ipif_set_tun_auto_addr(ipif_t *ipif, struct iftun_req *ta)
{
	sin6_t	sin6;
	sin_t	*sin;

	/*  upper part could be set to link local prefix  */
	if (ipif->ipif_v6src_addr.s6_addr32[3] != 0 ||
	    ipif->ipif_v6src_addr.s6_addr32[2] != 0 ||
	    ta->ifta_saddr.ss_family != AF_INET ||
	    (ipif->ipif_flags & IFF_UP) || !ipif->ipif_isv6 ||
	    (ta->ifta_flags & IFTUN_SRC) == 0)
		return;
	(void) ip_index_to_mask_v6(IPV6_ABITS - IP_ABITS,
	    &ipif->ipif_v6net_mask);
	bzero(&sin6, sizeof (sin6_t));
	sin = (sin_t *)&ta->ifta_saddr;
	V4_PART_OF_V6(sin6.sin6_addr) = sin->sin_addr.s_addr;
	sin6.sin6_family = AF_INET6;
	(void) ip_sioctl_addr(ipif, (sin_t *)&sin6, NULL, NULL);
}

/*
 * Set link local for ipif_id 0 of a configured tunnel based on the
 * tsrc or tdst parameter
 * For tunnels over IPv4 use the IPv4 address prepended with 32 zeros as
 * the token.
 * For tunnels over IPv6 use the low-order 64 bits of the "inner" IPv6 address
 * as the token for the "outer" link.
 */
void
ipif_set_tun_llink(ill_t *ill, struct iftun_req *ta)
{
	ipif_t		*ipif;
	sin_t		*sin;
	in6_addr_t	*s6addr;

	/* The first ipif must be id zero. */
	ipif = ill->ill_ipif;
	ASSERT(ipif->ipif_id == 0);

	/* no link local for automatic tunnels */
	if (!(ipif->ipif_flags & IFF_POINTOPOINT)) {
		ipif_set_tun_auto_addr(ipif, ta);
		return;
	}

	if ((ta->ifta_flags & IFTUN_DST) &&
	    IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6pp_dst_addr)) {
		sin6_t	sin6;

		ASSERT(!(ipif->ipif_flags & IFF_UP));
		bzero(&sin6, sizeof (sin6_t));
		if ((ta->ifta_saddr.ss_family == AF_INET)) {
			sin = (sin_t *)&ta->ifta_daddr;
			V4_PART_OF_V6(sin6.sin6_addr) =
			    sin->sin_addr.s_addr;
		} else {
			s6addr =
			    &((sin6_t *)&ta->ifta_daddr)->sin6_addr;
			sin6.sin6_addr.s6_addr32[3] = s6addr->s6_addr32[3];
			sin6.sin6_addr.s6_addr32[2] = s6addr->s6_addr32[2];
		}
		ipif_get_linklocal(&ipif->ipif_v6pp_dst_addr,
		    &sin6.sin6_addr);
		ipif->ipif_v6subnet = ipif->ipif_v6pp_dst_addr;
	}
	if ((ta->ifta_flags & IFTUN_SRC)) {
		/* Already set? */
		if (ill->ill_token.s6_addr32[3] != 0 ||
		    ill->ill_token.s6_addr32[2] != 0) {
			return;
		}
		ASSERT(!(ipif->ipif_flags & IFF_UP));
		ill->ill_token = ipv6_all_zeros;
		if ((ta->ifta_saddr.ss_family == AF_INET)) {
			sin = (sin_t *)&ta->ifta_saddr;
			V4_PART_OF_V6(ill->ill_token) = sin->sin_addr.s_addr;
		} else {
			s6addr = &((sin6_t *)&ta->ifta_saddr)->sin6_addr;
			ill->ill_token.s6_addr32[3] = s6addr->s6_addr32[3];
			ill->ill_token.s6_addr32[2] = s6addr->s6_addr32[2];
		}
		ill->ill_token_length = IPV6_TOKEN_LEN;
		(void) ipif_setlinklocal(ipif);
	}
}

/*
 * Generate a link-local address from the token.
 * Returns zero if ok. Non-zero if the token length is too large.
 */
int
ipif_setlinklocal(ipif_t *ipif)
{
	ill_t *ill = ipif->ipif_ill;

	if (ill->ill_token_length > IPV6_ABITS - IPV6_LL_PREFIXLEN)
		return (-1);

	ipif_get_linklocal(&ipif->ipif_v6lcl_addr, &ill->ill_token);
	(void) ip_index_to_mask_v6(IPV6_LL_PREFIXLEN, &ipif->ipif_v6net_mask);
	V6_MASK_COPY(ipif->ipif_v6lcl_addr, ipif->ipif_v6net_mask,
	    ipif->ipif_v6subnet);

	if (ipif->ipif_flags & IFF_NOLOCAL) {
		ipif->ipif_v6src_addr = ipv6_all_zeros;
	} else {
		ipif->ipif_v6src_addr = ipif->ipif_v6lcl_addr;
	}
	return (0);
}

/*
 * Get the resolver set up for a new interface address.  (Always called
 * as writer.)
 */
int
ipif_ndp_up(ipif_t *ipif, const in6_addr_t *addr)
{
	ill_t		*ill = ipif->ipif_ill;
	int		err = 0;
	in6_addr_t	v6_mcast_addr = {(uint32_t)V6_MCAST, 0, 0, 0};
	in6_addr_t	v6_mcast_mask = {(uint32_t)V6_MCAST, 0, 0, 0};
	in6_addr_t	v6_extract_mask = {0, 0, 0, 0};
	uchar_t		*phys_addr;
	nce_t		*nce = NULL;
	nce_t		*mnce = NULL;

	ip1dbg(("ipif_ndp_up(%s:%u)\n",
		ipif->ipif_ill->ill_name, ipif->ipif_id));

	if (IN6_IS_ADDR_UNSPECIFIED(addr) ||
	    (!(ill->ill_net_type & IRE_INTERFACE)))
		return (0);

	/* Clean up the existing entries before adding new ones */
	ipif_ndp_down(ipif);
	if ((ipif->ipif_flags & (IFF_UNNUMBERED|IFF_NOLOCAL)) == 0) {
		int	flags;
		uchar_t	*hw_addr = NULL;

		/* Permanent entries don't need NUD */
		flags = NCE_F_PERMANENT;
		flags |= NCE_F_NONUD;
		if (ill->ill_ipif->ipif_flags & IFF_ROUTER)
			flags |= NCE_F_ISROUTER;

		if (ipif->ipif_flags & IFF_ANYCAST)
			flags |= NCE_F_ANYCAST;

		if (ill->ill_net_type == IRE_IF_RESOLVER)
			hw_addr = ill->ill_hw_addr;
		err = ndp_lookup_then_add(ill,
		    hw_addr,
		    addr,
		    &ipv6_all_ones,
		    &ipv6_all_zeros,
		    0,
		    flags,
		    ND_REACHABLE,
		    &nce);
		switch (err) {
		case 0:
			break;
		case EEXIST:
			NCE_REFRELE(nce);
			nce = NULL;
			ip1dbg(("ipif_ndp_up: NCE already  for %s exists \n",
			    ill->ill_name));
			goto failed;
		default:
			ip1dbg(("ipif_ndp_up: NCE already  for %s exists \n",
			    ill->ill_name));
			goto failed;
		}
	}
	/*
	 * If there are multiple logical interfaces for the same stream
	 * (e.g. le0:1) we only add a multicast mapping for the primary one.
	 * Note: there will be no multicast info in arp if le0 is down
	 * but le0:1 is up.
	 */
	if (ipif->ipif_id != 0 ||
	    !(ipif->ipif_flags & IFF_MULTICAST)) {
		if (nce != NULL)
			NCE_REFRELE(nce);
		return (0);
	}

	/*
	 * Check IFF_MULTI_BCAST and possible length of physical
	 * address != 6(?) to determine if we use the mapping or the
	 * broadcast address.
	 */
	if (ipif->ipif_flags & IFF_MULTI_BCAST) {
		dl_unitdata_req_t *dlur;

		if (ill->ill_phys_addr_length > IP_MAX_HW_LEN) {
			err = E2BIG;
			goto failed;
		}
		/* Use the link-layer broadcast address for MULTI_BCAST */
		dlur = (dl_unitdata_req_t *)ill->ill_bcast_mp->b_rptr;
		if (ill->ill_sap_length < 0)
			phys_addr = (uchar_t *)dlur + dlur->dl_dest_addr_offset;
		else
			phys_addr = (uchar_t *)dlur +
			    dlur->dl_dest_addr_offset + ill->ill_sap_length;
	} else {
		/* Extract low order 32 bits from IPv6 multicast address */
		v6_extract_mask.s6_addr32[3] = 0xffffffffU;
		phys_addr = ipv6_g_phys_multi_addr;
	}
	if (ipif->ipif_flags &
	    (IFF_MULTICAST | IFF_MULTI_BCAST | IFF_BROADCAST)) {
		err = ndp_lookup_then_add(ill,
		    phys_addr,
		    &v6_mcast_addr,	/* v6 address */
		    &v6_mcast_mask,	/* v6 mask */
		    &v6_extract_mask,
		    IP_NDP_HW_MAPPING_START,
		    NCE_F_MAPPING | NCE_F_PERMANENT | NCE_F_NONUD,
		    ND_REACHABLE,
		    &mnce);
		switch (err) {
		case 0:
			NCE_REFRELE(mnce);
			break;
		case EEXIST:
			NCE_REFRELE(mnce);
			ip1dbg(("ipif_ndp_up: Mapping NCE for %s exists\n",
			    ill->ill_name));
			goto failed;
		default:
			ip1dbg(("ipif_ndp_up: Mapping NCE creation failed %s\n",
			    ill->ill_name));
			goto failed;
		}
		ip2dbg(("ipif_ndp_up: adding multicast NDP setup for %s\n",
			ill->ill_name));
	}
	ip2dbg(("ipif_ndp_up: adding multicast NDP setup for %s\n",
		ill->ill_name));

	if (nce != NULL)
		NCE_REFRELE(nce);
	return (0);

failed:
	/* If addr was added to the cache remove it */
	if (nce != NULL) {
		(void) ndp_delete(nce);
		NCE_REFRELE(nce);
	}
	return (err);
}

/* Remove all cache entries for this logical interface */
void
ipif_ndp_down(ipif_t *ipif)
{
	nce_t	*nce;

	nce = ndp_lookup(ipif->ipif_ill, &ipif->ipif_v6lcl_addr);
	if (nce != NULL) {
		ndp_delete(nce);
		NCE_REFRELE(nce);
	}
	/* Remove mapping as well if zeroth interface */
	if (ipif->ipif_id == 0) {
		ndp_walk(ipif->ipif_ill, (pfi_t)ndp_delete_per_ill,
		    (uchar_t *)ipif->ipif_ill);
	}
}

/*
 * Used when an interface comes up to recreate any extra routes on this
 * interface.
 */
void
ipif_recover_ire_v6(ipif_t *ipif)
{
	mblk_t	*mp;

	ip1dbg(("ipif_recover_ire_v6(%s:%u)", ipif->ipif_ill->ill_name,
	    ipif->ipif_id));

	ASSERT(ipif->ipif_isv6);

	mutex_enter(&ipif->ipif_saved_ire_lock);
	for (mp = ipif->ipif_saved_ire_mp; mp != NULL; mp = mp->b_cont) {
		ire_t		*ire;
		queue_t		*stq;
		ifrt_t		*ifrt;
		in6_addr_t	*src_addr;
		in6_addr_t	*gateway_addr;
		mblk_t		*resolver_mp;
		char		buf[INET6_ADDRSTRLEN];
		ushort_t	type;

		/*
		 * When the ire was initially created and then added in
		 * ip_rt_add_v6(), it was created either using
		 * ipif->ipif_net_type in the case of a traditional interface
		 * route, or as one of the IRE_OFFSUBNET types (with the
		 * exception of IRE_HOST_REDIRECT which is created by
		 * icmp_redirect_v6() and which we don't need to save or
		 * recover).  In the case where ipif->ipif_net_type was
		 * IRE_LOOPBACK, ip_rt_add_v6() will update the ire_type to
		 * IRE_IF_NORESOLVER before calling ire_add_v6() to satisfy
		 * software like GateD and Sun Cluster which creates routes
		 * using the the loopback interface's address as a gateway.
		 *
		 * As ifrt->ifrt_type reflects the already updated ire_type and
		 * since ire_create_v6() expects that IRE_IF_NORESOLVER will
		 * have a valid ire_dlureq_mp field (which doesn't make sense
		 * for a IRE_LOOPBACK), ire_create_v6() will be called in the
		 * same way here as in ip_rt_add_v6(), namely using
		 * ipif->ipif_net_type when the route looks like a traditional
		 * interface route (where ifrt->ifrt_type & IRE_INTERFACE is
		 * true) and otherwise using the saved ifrt->ifrt_type.  This
		 * means that in the case where ipif->ipif_net_type is
		 * IRE_LOOPBACK, the ire created by ire_create_v6() will be an
		 * IRE_LOOPBACK, it will then be turned into an
		 * IRE_IF_NORESOLVER and then added by ire_add_v6().
		 */
		ifrt = (ifrt_t *)mp->b_rptr;
		if (ifrt->ifrt_type & IRE_INTERFACE) {
			stq = (ipif->ipif_net_type == IRE_IF_RESOLVER)
			    ? ipif->ipif_rq : ipif->ipif_wq;
			src_addr = &ipif->ipif_v6src_addr;
			gateway_addr = NULL;
			resolver_mp = ipif->ipif_resolver_mp;
			type = ipif->ipif_net_type;
		} else {
			stq = NULL;
			src_addr = NULL;
			gateway_addr = &ifrt->ifrt_v6gateway_addr;
			resolver_mp = NULL;
			type = ifrt->ifrt_type;
		}

		/*
		 * Create a copy of the IRE with the saved address and netmask.
		 */
		ip1dbg(("ipif_recover_ire_v6: creating IRE %s (%d) for %s/%d\n",
		    ip_nv_lookup(ire_nv_tbl, ifrt->ifrt_type), ifrt->ifrt_type,
		    inet_ntop(AF_INET6, &ifrt->ifrt_v6addr, buf, sizeof (buf)),
		    ip_mask_to_index_v6(&ifrt->ifrt_v6mask)));
		ire = ire_create_v6(
		    &ifrt->ifrt_v6addr,
		    &ifrt->ifrt_v6mask,
		    src_addr,
		    gateway_addr,
		    ifrt->ifrt_max_frag,
		    NULL,
		    NULL,
		    stq,
		    type,
		    resolver_mp,
		    ipif,
		    NULL,
		    0,
		    0,
		    ifrt->ifrt_flags,
		    &ifrt->ifrt_iulp_info);
		if (ire == NULL) {
			mutex_exit(&ipif->ipif_saved_ire_lock);
			return;
		}

		/*
		 * Some software (for example, GateD and Sun Cluster) attempts
		 * to create (what amount to) IRE_PREFIX routes with the
		 * loopback address as the gateway.  This is primarily done to
		 * set up prefixes with the RTF_REJECT flag set (for example,
		 * when generating aggregate routes.)
		 *
		 * If the IRE type (as defined by ipif->ipif_net_type) is
		 * IRE_LOOPBACK, then we map the request into a
		 * IRE_IF_NORESOLVER.
		 */
		if (ipif->ipif_net_type == IRE_LOOPBACK)
			ire->ire_type = IRE_IF_NORESOLVER;
		ire = ire_add(ire);
		if (ire != NULL) {
			ire_refrele(ire);		/* Held in ire_add */
		}
	}
	mutex_exit(&ipif->ipif_saved_ire_lock);
}

/*
 * Larger scope address return larger numbers.
 * The code below uses special case code to allow a v4-compat source
 * with a v6 global destination.
 */
uint_t
ip_address_scope_v6(const in6_addr_t *addr)
{
	if (IN6_IS_ADDR_MULTICAST(addr)) {
		switch (IN6_ADDR_MC_SCOPE(addr)) {
		case 0: /* Reserved */
			return (IP6_SCOPE_GLOBAL);
		case 1: /* Node local - approximate as link-local */
		case 2: /* Link local */
			return (IP6_SCOPE_LINKLOCAL);
		case 3:
		case 4:
		case 5: /* Site local */
			return (IP6_SCOPE_SITELOCAL);
		default:
			return (IP6_SCOPE_GLOBAL);
		}
	}
	if (IN6_IS_ADDR_LINKLOCAL(addr))
		return (IP6_SCOPE_LINKLOCAL);
	if (IN6_IS_ADDR_SITELOCAL(addr))
		return (IP6_SCOPE_SITELOCAL);
	if (IN6_IS_ADDR_V4COMPAT(addr))
		return (IP6_SCOPE_V4COMPAT);
	if (IN6_IS_ADDR_V4MAPPED(addr))
		return (IP6_SCOPE_V4MAPPED);
	return (IP6_SCOPE_GLOBAL);
}

/*
 * Determine number of leading bits that are common between two addresses.
 */
static uint_t
ip_common_bits_v6(const in6_addr_t *a1, const in6_addr_t *a2)
{
	uint_t		bits;
	int		i;
	uint32_t	diff;	/* Bits that differ */

	bits = 0;
	for (i = 0; i < 4; i++) {
		if (a1->s6_addr32[i] == a2->s6_addr32[i]) {
			bits += 32;
			continue;
		}
		break;
	}
	if (bits == IPV6_ABITS)
		return (bits);

	/*
	 * Find number of leading common bits in the word which might
	 * have some common bits by searching for the first one from the left
	 * in the xor of the two addresses.
	 */
	diff = ntohl(a1->s6_addr32[i] ^ a2->s6_addr32[i]);
	for (i = 0; i < 32; i++) {
		if ((diff >> (31 - i)) & 0x1)
			return (bits + i);
	}
	/* Should never happen since we catch all ones in first loop */
#ifdef DEBUG
	cmn_err(CE_PANIC, "internal error in ip_common_bits_v6");
#endif
	return (bits + 32);
}

/*
 * Used to record information for each scope level while searching.
 */
struct ipv6_scope_info {
	ipif_t	*si_ipif;	/* Chosen best one */
	int	si_common_bits;	/* Number of common bits with destination */
	uint_t	si_num_equal;	/* Number of ipifs that equally good */
};

/*
 * Return a pointer to the best ipif with a matching scope.
 * First look for the desired scope and if none found
 * look between min and max. If Desired == Min then search from
 * low to high; otherwise search from high to low.
 * For a given scope select the ipif whose source address best matches
 * the destination (most number of leading common bits).
 * Also return the number of equally good matches.
 */
ipif_t *
ipif_lookup_scope_v6(ill_t *ill, const in6_addr_t *faddr,
    uint_t desired, uint_t min, uint_t max, uint_t on_flags, uint_t off_flags,
    uint_t *num_equal)
{
	ipif_t  *ipif;
	struct	ipv6_scope_info	scope_array[IP6_SCOPE_MAX];
	uint_t	scope;
	int	 common_bits;

	ASSERT(min <= max);
	for (scope = 0; scope < IP6_SCOPE_MAX; scope++) {
		scope_array[scope].si_ipif = NULL;
		scope_array[scope].si_common_bits = -1;	/* Uninitialized */
		scope_array[scope].si_num_equal = 0;
	}

	for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
		/* Always skip NOLOCAL and ANYCAST interfaces */
		if (ipif->ipif_flags & (IFF_NOLOCAL|IFF_ANYCAST))
			continue;
		if ((ipif->ipif_flags & on_flags) != on_flags)
			continue;
		if ((ipif->ipif_flags & off_flags) != 0)
			continue;
		scope = ip_address_scope_v6(&ipif->ipif_v6lcl_addr);
		common_bits = ip_common_bits_v6(&ipif->ipif_v6lcl_addr, faddr);
#ifdef DEBUG
		{
			char abuf1[INET6_ADDRSTRLEN];
			char abuf2[INET6_ADDRSTRLEN];

			ip1dbg(("ipif_lookup_scope_v6(%s, faddr %s) "
			    "laddr %s, common %d\n",
			    ill->ill_name,
			    inet_ntop(AF_INET6, (char *)faddr,
				abuf2, sizeof (abuf2)),
			    inet_ntop(AF_INET6,
				(char *)&ipif->ipif_v6lcl_addr,
				abuf1, sizeof (abuf1)),
			    common_bits));
		}
#endif /* DEBUG */
		if (common_bits > scope_array[scope].si_common_bits) {
			scope_array[scope].si_ipif = ipif;
			scope_array[scope].si_common_bits = common_bits;
			scope_array[scope].si_num_equal = 1;
		} else if (common_bits == scope_array[scope].si_common_bits) {
			/* Count one more choice for load balancing */
			scope_array[scope].si_num_equal++;
		}
	}
	if (desired == min) {
		for (scope = min; scope <= max; scope++) {
			if (scope_array[scope].si_ipif != NULL) {
				*num_equal = scope_array[scope].si_num_equal;
				return (scope_array[scope].si_ipif);
			}
		}
	} else {
		ASSERT(desired == max);
		for (scope = max; scope >= min; scope--) {
			if (scope_array[scope].si_ipif != NULL) {
				*num_equal = scope_array[scope].si_num_equal;
				return (scope_array[scope].si_ipif);
			}
		}
	}
	ip1dbg(("ipif_lookup_scope_v6(%s, des %d, min %d max %d on %x off %x: "
	    "not found\n",
	    ill->ill_name, desired, min, max, on_flags, off_flags));
	return (NULL);
}

/*
 * Determine the best source address given a destination address and an ill.
 * First look for a non-deprecated address of the same or greater scope.
 * Second look for a deprecated address of the same or greater scope.
 * Third look for a non-deprecated address of smaller scope.
 * Finally look for a deprecated address of smaller scope.
 *
 * For compatible addresses the preference order is compatible, global, and
 * site local. If all of them are deprecated then look for deprecated
 * addresses in the same preference order.
 *
 * Returns NULL if there is no suitable source address for the ill.
 * This only occurs when there is no valid source address for the ill.
 */
ipif_t *
ipif_select_source_v6(ill_t *ill, const in6_addr_t *faddr)
{
	uint_t scope;
	ipif_t *ipif;
	uint_t num_equal;

	scope = ip_address_scope_v6(faddr);
	ASSERT(scope != IP6_SCOPE_V4MAPPED);
	switch (scope) {
	case IP6_SCOPE_V4COMPAT:
		/* Order of preference is compat, global and site local */
		ipif = ipif_lookup_scope_v6(ill, faddr, scope,
		    IP6_SCOPE_SITELOCAL, scope, IFF_UP, IFF_DEPRECATED,
		    &num_equal);
		if (ipif == NULL) {
			/* Look for deprecated */
			ipif = ipif_lookup_scope_v6(ill, faddr, scope,
			    IP6_SCOPE_SITELOCAL, scope,
			    IFF_UP | IFF_DEPRECATED, 0, &num_equal);
		}
		break;
	default:
		/*
		 * Check for preferred addresses with equal or larger scope
		 * first. Then check for deprecated with equal or larger.
		 * Finally look for preferred and deprecated with a smaller
		 * scope.
		 */
		ipif = ipif_lookup_scope_v6(ill, faddr, scope, scope,
		    IP6_SCOPE_GLOBAL, IFF_UP, IFF_DEPRECATED, &num_equal);
		if (ipif == NULL) {
			/* Look for deprecated */
			ipif = ipif_lookup_scope_v6(ill, faddr, scope, scope,
			    IP6_SCOPE_GLOBAL, IFF_UP | IFF_DEPRECATED, 0,
			    &num_equal);
		}
		if (ipif == NULL) {
			/* Look for non-deprecated smaller scope */
			ipif = ipif_lookup_scope_v6(ill, faddr, scope,
			    IP6_SCOPE_LINKLOCAL, scope,
			    IFF_UP, IFF_DEPRECATED, &num_equal);
		}
		if (ipif == NULL) {
			/* Look for deprecated smaller scope */
			ipif = ipif_lookup_scope_v6(ill, faddr, scope,
			    IP6_SCOPE_LINKLOCAL, scope,
			    IFF_UP | IFF_DEPRECATED, 0, &num_equal);
		}
		break;
	}
#ifdef DEBUG
	if (ipif == NULL) {
		char buf1[INET6_ADDRSTRLEN];

		ip1dbg(("ipif_select_source_v6(%s, %s) -> NULL\n",
		    ill->ill_name,
		    inet_ntop(AF_INET6, faddr, buf1, sizeof (buf1))));
	} else {
		char buf1[INET6_ADDRSTRLEN];
		char buf2[INET6_ADDRSTRLEN];

		ip1dbg(("ipif_select_source_v6(%s, %s) -> %s, eq %d\n",
		    ill->ill_name,
		    inet_ntop(AF_INET6, faddr, buf1, sizeof (buf1)),
		    inet_ntop(AF_INET6, &ipif->ipif_v6lcl_addr,
		    buf2, sizeof (buf2)),
		    num_equal));
	}
#endif /* DEBUG */
	return (ipif);
}

/*
 * This old_ipif is going away.
 *
 * Determine if any other ipif's is using our address as
 * ipif_v6lcl_addr (due to those being IFF_NOLOCAL, IFF_ANYCAST, or
 * IFF_DEPRECATED).
 * Find the IRE_INTERFACE for such ipifs and recreate them
 * to use an different source address following the rules in
 * ipif_up_done_v6.
 */
void
ipif_update_other_ipifs_v6(ipif_t *old_ipif)
{
	ipif_t *ipif;
	ipif_t *nipif;
	ire_t *ire;
	queue_t *stq;
	char	buf[INET6_ADDRSTRLEN];

	ASSERT(!(old_ipif->ipif_flags & IFF_UP));

	/* Is there any work to be done? */
	if (IN6_IS_ADDR_UNSPECIFIED(&old_ipif->ipif_v6lcl_addr) ||
	    old_ipif->ipif_ill->ill_wq == NULL)
		return;

	ip1dbg(("ipif_update_other_ipifs_v6(%s, %s)\n",
	    old_ipif->ipif_ill->ill_name,
	    inet_ntop(AF_INET6, &old_ipif->ipif_v6lcl_addr,
	    buf, sizeof (buf))));

	for (ipif = old_ipif->ipif_ill->ill_ipif; ipif != NULL;
	    ipif = ipif->ipif_next) {
		if (ipif == old_ipif)
			continue;

		if (!(ipif->ipif_flags &
		    (IFF_NOLOCAL|IFF_ANYCAST|IFF_DEPRECATED))) {
			/* Can't possibly have borrowed the source from ipif */
			continue;
		}
		/*
		 * Perform the same checks as when creating the IRE_INTERFACE
		 * in ipif_up_done_v6.
		 */
		if (!(ipif->ipif_flags & IFF_UP))
			continue;
		if ((ipif->ipif_flags & IFF_NOXMIT))
			continue;
		if (IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6subnet) &&
		    IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6net_mask))
			continue;

		/*
		 * We know that ipif uses some other source for its
		 * IRE_INTERFACE. Is it using the source of this old_ipif?
		 */
		ire = ipif_to_ire_v6(ipif);
		if (ire == NULL)
			continue;
		if (!IN6_ARE_ADDR_EQUAL(&old_ipif->ipif_v6lcl_addr,
		    &ire->ire_src_addr_v6))
			continue;
		ip1dbg(("ipif_update_other_ipifs_v6: deleting IRE for src %s\n",
		    inet_ntop(AF_INET6, &ire->ire_src_addr_v6,
		    buf, sizeof (buf))));

		stq = ire->ire_stq;

		/* Remove the ire and recreate one */
		ire_delete(ire);
		ire_refrele(ire);

		/*
		 * Can't use our source address. Select a different
		 * source address for the IRE_INTERFACE.
		 */
		nipif = ipif_select_source_v6(ipif->ipif_ill,
		    &ipif->ipif_v6subnet);
		if (nipif == NULL) {
			/* Last resort - all ipif's have IFF_NOLOCAL */
			nipif = ipif;
		}
		ip1dbg(("ipif_update_other_ipifs_v6: create if IRE %d for %s\n",
		    ipif->ipif_ill->ill_net_type,
		    inet_ntop(AF_INET6, &ipif->ipif_v6subnet,
		    buf, sizeof (buf))));

		ire = ire_create_v6(
		    &ipif->ipif_v6subnet,	/* dest pref */
		    &ipif->ipif_v6net_mask,	/* mask */
		    &nipif->ipif_v6src_addr,	/* src addr */
		    NULL,			/* no gateway */
		    ipif->ipif_mtu,		/* max frag */
		    NULL,			/* no Fast path header */
		    NULL,			/* no recv from queue */
		    stq,			/* send-to queue */
		    ipif->ipif_ill->ill_net_type, /* IF_[NO]RESOLVER */
		    ipif->ipif_ill->ill_resolver_mp,	/* xmit header */
		    ipif,
		    NULL,
		    0,
		    0,
		    0,
		    &ire_uinfo_null);
		if (ire != NULL) {
			ire = ire_add(ire);
			if (ire != NULL) {
				ire_refrele(ire);	/* Held in ire_add */
			}
		}
	}
}

/*
 * Perform an attach and bind to get phys addr plus info_req for
 * the physical device.
 * q and mp represents an ioctl which will be queued waiting for
 * completion of the DLPI message exchange.
 * MUST be called on an ill queue. Can not set ipc_pending_ill for that
 * reason thus the DL_PHYS_ADDR_ACK code does not assume ill_pending_q.
 *
 * Returns EINPROGRESS when mp has been consumed by queueing it on
 * ill_pending_mp and the ioctl will complete in ip_rput.
 */
int
ill_dl_phys(ill_t *ill, ipif_t *ipif, mblk_t *mp, queue_t *q)
{
	mblk_t	*phys_mp = NULL;
	mblk_t	*info_mp = NULL;
	mblk_t	*attach_mp = NULL;
	mblk_t	*detach_mp = NULL;
	mblk_t	*bind_mp = NULL;
	mblk_t	*unbind_mp = NULL;

	ip1dbg(("ill_dl_phys(%s:%u)\n", ill->ill_name, ipif->ipif_id));
	ASSERT(ill->ill_dlpi_style_set);
	ASSERT(WR(q)->q_next != NULL);

	if (ill->ill_pending_mp != NULL)
		return (EAGAIN);

	phys_mp = ip_dlpi_alloc(sizeof (dl_phys_addr_req_t) +
	    sizeof (t_scalar_t), DL_PHYS_ADDR_REQ);
	if (phys_mp == NULL)
		goto bad;
	((dl_phys_addr_req_t *)phys_mp->b_rptr)->dl_addr_type =
	    DL_CURR_PHYS_ADDR;

	info_mp = ip_dlpi_alloc(
	    sizeof (dl_info_req_t) + sizeof (dl_info_ack_t),
	    DL_INFO_REQ);
	if (info_mp == NULL)
		goto bad;

	bind_mp = ip_dlpi_alloc(sizeof (dl_bind_req_t) + sizeof (long),
	    DL_BIND_REQ);
	if (bind_mp == NULL)
		goto bad;
	((dl_bind_req_t *)bind_mp->b_rptr)->dl_sap = ill->ill_sap;
	((dl_bind_req_t *)bind_mp->b_rptr)->dl_service_mode = DL_CLDLS;

	unbind_mp = ip_dlpi_alloc(sizeof (dl_unbind_req_t), DL_UNBIND_REQ);
	if (unbind_mp == NULL)
		goto bad;

	/* If we need to attach/detach, pre-alloc and initialize the mblks */
	if (ill->ill_needs_attach) {
		attach_mp = ip_dlpi_alloc(sizeof (dl_attach_req_t),
		    DL_ATTACH_REQ);
		if (attach_mp == NULL)
			goto bad;
		((dl_attach_req_t *)attach_mp->b_rptr)->dl_ppa = ill->ill_ppa;

		detach_mp = ip_dlpi_alloc(sizeof (dl_detach_req_t),
		    DL_DETACH_REQ);
		if (detach_mp == NULL)
			goto bad;
	}

	/*
	 * Here we are going to delay the ioctl ack until after
	 * ACKs from DL_PHYS_ADDR_REQ. So need to save the
	 * original ioctl message before sending the requests
	 */
	ASSERT(ill->ill_pending_mp == NULL && ill->ill_pending_ipif == NULL);
	ill->ill_pending_ipif = ipif;
	ill->ill_pending_mp = mp;
	ASSERT(ill->ill_pending_q == NULL);

	if (attach_mp != NULL) {
		ip1dbg(("ill_dl_phys: attach\n"));
		ill_dlpi_send(ill, attach_mp);
	}
	ill_dlpi_send(ill, bind_mp);
	ill_dlpi_send(ill, info_mp);
	ill_dlpi_send(ill, phys_mp);
	ill_dlpi_send(ill, unbind_mp);

	if (detach_mp != NULL) {
		ip1dbg(("ill_dl_phys: detach\n"));
		ill_dlpi_send(ill, detach_mp);
	}
	/*
	 * This operation will complete in ip_rput_dlpi_writer with either
	 * a DL_PHYS_ADDR_ACK or DL_ERROR_ACK.
	 */
	return (EINPROGRESS);
bad:
	if (phys_mp != NULL)
		freemsg(phys_mp);
	if (info_mp != NULL)
		freemsg(info_mp);
	if (attach_mp != NULL)
		freemsg(attach_mp);
	if (detach_mp != NULL)
		freemsg(detach_mp);
	if (bind_mp != NULL)
		freemsg(bind_mp);
	if (unbind_mp != NULL)
		freemsg(unbind_mp);
	return (ENOMEM);
}

/*
 * DLPI is up.
 * Create all the IREs associated with an interface bring up multicast.
 * Set the interface flag and finish other initialization
 * that potentially had to be differed to after DL_BIND_ACK.
 */
int
ipif_up_done_v6(ipif_t *ipif)
{
	ire_t	*ire_array[20];
	ire_t	**irep = ire_array;
	ire_t	**irep1;
	ill_t	*ill = ipif->ipif_ill;
	queue_t	*stq;
	in6_addr_t	v6addr;
	in6_addr_t	route_mask;
	ipif_t	 *src_ipif;
	int	err;
	char	buf[INET6_ADDRSTRLEN];

	ip1dbg(("ipif_up_done_v6(%s:%u)\n",
		ipif->ipif_ill->ill_name, ipif->ipif_id));

	ASSERT(ipif->ipif_isv6);

	/*
	 * Make sure ip6_forwarding is set if this interface is a router.
	 * XXX per-interface ip_forwarding.
	 */
	if ((ipif->ipif_flags & IFF_ROUTER) && !ipv6_forward) {
		ip1dbg(("ipif_up_done_v6: setting ip6_forwarding\n"));
		ipv6_forward = 1;
	}
	/*
	 * Remove any IRE_CACHE entries for this ill to make
	 * sure source address selection gets to take this new ipif
	 * into account.
	 */
	ire_walk_ill_v6(ill, ill_cache_delete, (char *)ipif->ipif_ill);

	/*
	 * Figure out which way the send-to queue should go.  Only
	 * IRE_IF_RESOLVER or IRE_IF_NORESOLVER should show up here.
	 */
	switch (ill->ill_net_type) {
	case IRE_IF_RESOLVER:
		stq = ill->ill_rq;
		break;
	case IRE_IF_NORESOLVER:
	case IRE_LOOPBACK:
		stq = ill->ill_wq;
		break;
	default:
		return (EINVAL);
	}

	if (ipif->ipif_flags & (IFF_NOLOCAL|IFF_ANYCAST|IFF_DEPRECATED)) {
		/*
		 * Can't use our source address. Select a different
		 * source address for the IRE_INTERFACE and IRE_LOCAL
		 */
		src_ipif = ipif_select_source_v6(ipif->ipif_ill,
		    &ipif->ipif_v6subnet);
		if (src_ipif == NULL)
			src_ipif = ipif;	/* Last resort */
	} else {
		src_ipif = ipif;
	}

	if (!IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6lcl_addr) &&
	    !(ipif->ipif_flags & IFF_NOLOCAL)) {
		/* Register the source address for __sin6_src_id */
		err = ip_srcid_insert(&ipif->ipif_v6lcl_addr);
		if (err != 0) {
			ip0dbg(("ipif_up_done_v6: srcid_insert %d\n", err));
			return (err);
		}
		/*
		 * If the interface address is set, create the LOCAL
		 * or LOOPBACK IRE.
		 */
		ip1dbg(("ipif_up_done_v6: creating IRE %d for %s\n",
		    ipif->ipif_ire_type,
		    inet_ntop(AF_INET6, &ipif->ipif_v6lcl_addr,
		    buf, sizeof (buf))));

		*irep++ = ire_create_v6(
		    &ipif->ipif_v6lcl_addr,		/* dest address */
		    &ipv6_all_ones,			/* mask */
		    &src_ipif->ipif_v6src_addr,		/* source address */
		    NULL,				/* no gateway */
		    IP_LOOPBACK_MTU + IPV6_HDR_LEN + 20, /* max frag size */
		    NULL,
		    ipif->ipif_rq,			/* recv-from queue */
		    NULL,				/* no send-to queue */
		    ipif->ipif_ire_type,		/* LOCAL or LOOPBACK */
		    NULL,
		    ipif,				/* interface */
		    NULL,
		    0,
		    0,
		    (ipif->ipif_flags & IFF_PRIVATE) ? RTF_PRIVATE : 0,
		    &ire_uinfo_null);
	}

	/*
	 * Set up the IRE_IF_RESOLVER or IRE_IF_NORESOLVER, as appropriate.
	 * Note that atun interfaces have an all-zero ipif_v6subnet.
	 * Thus we allow a zero subnet as long as the mask is non-zero.
	 */
	if (stq != NULL && !(ipif->ipif_flags & IFF_NOXMIT) &&
	    !(IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6subnet) &&
	    IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6net_mask))) {
		/* ipif_v6subnet is ipif_v6pp_dst_addr for pt-pt */
		v6addr = ipif->ipif_v6subnet;

		if (ipif->ipif_flags & IFF_POINTOPOINT) {
			route_mask = ipv6_all_ones;
		} else {
			route_mask = ipif->ipif_v6net_mask;
		}

		ip1dbg(("ipif_up_done_v6: creating if IRE %d for %s\n",
		    ill->ill_net_type,
		    inet_ntop(AF_INET6, &v6addr, buf, sizeof (buf))));

		*irep++ = ire_create_v6(
		    &v6addr,			/* dest pref */
		    &route_mask,		/* mask */
		    &src_ipif->ipif_v6src_addr,	/* src addr */
		    NULL,			/* no gateway */
		    ipif->ipif_mtu,		/* max frag */
		    NULL,			/* no Fast path header */
		    NULL,			/* no recv from queue */
		    stq,			/* send-to queue */
		    ill->ill_net_type,		/* IF_[NO]RESOLVER */
		    ill->ill_resolver_mp,	/* xmit header */
		    ipif,
		    NULL,
		    0,
		    0,
		    (ipif->ipif_flags & IFF_PRIVATE) ? RTF_PRIVATE : 0,
		    &ire_uinfo_null);
	}

	/* If an earlier ire_create failed, get out now */
	for (irep1 = irep; irep1 > ire_array; ) {
		irep1--;
		if (*irep1 == NULL)
			goto bad;
	}

	/*
	 * Add in all newly created IREs. We want to add before
	 * we call ifgrp_insert which wants to know whether
	 * IRE_IF_RESOLVER exists or not.
	 *
	 * NOTE : We refrele the ire though we may branch to "bad"
	 *	later on where we do ire_delete. This is okay
	 *	because nobody can delete it as we are running
	 *	exclusively.
	 */
	for (irep1 = irep; irep1 > ire_array; ) {
		irep1--;
		/* Shouldn't be adding any bcast ire's */
		ASSERT((*irep1)->ire_type != IRE_BROADCAST);
		*irep1 = ire_add(*irep1);
		if (*irep1 != NULL) {
			ire_refrele(*irep1);		/* Held in ire_add */
		}
	}

	/*
	 * If grouping interfaces and we created an IRE_INTERFACE
	 * for this ipif, insert this ipif
	 * into the appropriate interface group, or create a new one.
	 */
	if (ip_enable_group_ifs &&
	    !(IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6subnet) &&
	    IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6net_mask))) {
		if (!ifgrp_insert(ipif)) /* If ifgrp allocation failed. */
			goto bad;
	}

	/* Recover any additional IRE_IF_[NO]RESOLVER entries for this ipif */
	ipif_recover_ire_v6(ipif);

	/* If this is the loopback interface, we are done. */
	if (ipif->ipif_ire_type == IRE_LOOPBACK) {
		ipif->ipif_flags |= IFF_UP;
		ipif->ipif_ipif_up_count++;
		/* This one doesn't count towards ipif_g_count. */

		/* Join the allhosts multicast address */
		ipif_multicast_up(ipif);

		ip_rts_ifmsg(ipif);
		ip_rts_newaddrmsg(RTM_ADD, 0, ipif);
		return (0);
	}

	/* Mark it up, and increment counters. */
	ill->ill_ipif_up_count++;
	ipif->ipif_flags |= IFF_UP;
	ipif_g_count++;

	if (ipif->ipif_ipif_up_count == 1) {
		/*
		 * Need to recover all multicast memberships in the driver.
		 * This had to be deferred until we had attached.
		 */
		ill_recover_multicast(ill);
	}
	/* Join the allhosts multicast address and the solicited node MC */
	ipif_multicast_up(ipif);

	ip_rts_ifmsg(ipif);
	ip_rts_newaddrmsg(RTM_ADD, 0, ipif);
	return (0);
bad:
	/* Check here for possible removal from ifgrp. */
	if (ipif->ipif_ifgrpschednext != NULL)
	    ifgrp_delete(ipif);

	while (irep > ire_array) {
		irep--;
		if (*irep != NULL)
			ire_delete(*irep);
	}
	(void) ip_srcid_remove(&ipif->ipif_v6lcl_addr);
	return (ENOMEM);
}

/*
 * Delete an ND entry and the corresponding IRE_CACHE entry if it exists.
 */
/* ARGSUSED2 */
int
ip_siocdelndp_v6(ipif_t *ipif, lif_nd_req_t *lnr, queue_t *q)
{
	in6_addr_t	addr;
	sin6_t		*sin6;
	nce_t		*nce;

	/* Only allow for logical unit zero i.e. not on "le0:17" */
	if (ipif->ipif_id != 0)
		return (EINVAL);

	if (!ipif->ipif_isv6)
		return (EINVAL);

	if (lnr->lnr_addr.ss_family != AF_INET6)
		return (EAFNOSUPPORT);

	sin6 = (sin6_t *)&lnr->lnr_addr;
	addr = sin6->sin6_addr;
	nce = ndp_lookup(ipif->ipif_ill, &addr);
	if (nce == NULL)
		return (ESRCH);
	ndp_delete(nce);
	NCE_REFRELE(nce);
	return (0);
}

/*
 * Return nbr cache info.
 */
/* ARGSUSED */
int
ip_siocqueryndp_v6(ipif_t *ipif, lif_nd_req_t *lnr, queue_t *q, mblk_t *mp)
{
	ill_t		*ill = ipif->ipif_ill;

	/* Only allow for logical unit zero i.e. not on "le0:17" */
	if (ipif->ipif_id != 0)
		return (EINVAL);

	if (!ipif->ipif_isv6)
		return (EINVAL);

	if (lnr->lnr_addr.ss_family != AF_INET6)
		return (EAFNOSUPPORT);

	if (ill->ill_phys_addr_length > sizeof (lnr->lnr_hdw_addr))
		return (EINVAL);

	return (ndp_query(ill, lnr));
}

/*
 * Perform an update of the nd entry for the specified address.
 */
/* ARGSUSED */
int
ip_siocsetndp_v6(ipif_t *ipif, lif_nd_req_t *lnr, queue_t *q, mblk_t *mp)
{
	ill_t		*ill = ipif->ipif_ill;

	/* Only allow for logical unit zero i.e. not on "le0:17" */
	if (ipif->ipif_id != 0)
		return (EINVAL);

	if (!ipif->ipif_isv6)
		return (EINVAL);

	if (lnr->lnr_addr.ss_family != AF_INET6)
		return (EAFNOSUPPORT);

	return (ndp_sioc_update(ill, lnr));
}
