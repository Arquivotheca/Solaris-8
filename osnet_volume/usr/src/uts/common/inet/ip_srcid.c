/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip_srcid.c	1.5	99/09/26 SMI"

/*
 * This is used to support the hidden __sin6_src_id in the sockaddr_in6
 * structure which is there to ensure that applications (such as UDP apps)
 * which get an address from recvfrom and use that address in a sendto
 * or connect will by default use the same source address in the "response"
 * as the destination address in the "request" they received.
 *
 * This is built using some new functions (in IP - doing their own locking
 * so they can be called from the transports) to map between integer IDs
 * and in6_addr_t.
 * The use applies to sockaddr_in6 - whether or not mapped addresses are used.
 *
 * This file contains the functions used by both IP and the transports
 * to implement __sin6_src_id.
 * The routines do their own locking since they are called from
 * the transports (to map between a source id and an address)
 * and from IP proper when IP addresses are added and removed.
 *
 * The routines handle both IPv4 and IPv6 with the IPv4 addresses represented
 * as IPv4-mapped addresses.
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/sysmacros.h>
#include <sys/strsubr.h>
#include <sys/strlog.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/xti_inet.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/atomic.h>

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/callb.h>
#include <sys/socket.h>
#include <sys/vtrace.h>
#include <sys/isa_defs.h>
#include <sys/kmem.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <net/if_dl.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/snmpcom.h>
#include <sys/strick.h>

#include <netinet/igmp_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/tcp.h>
#include <inet/ip_multi.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>
#include <inet/optcom.h>
#include <inet/ip_ndp.h>
#include <netinet/igmp.h>
#include <netinet/ip_mroute.h>

#include <netinet/igmp.h>
#include <netinet/ip_mroute.h>

#include <net/pfkeyv2.h>
#include <inet/ipsec_info.h>
#include <inet/sadb.h>
#include <sys/kmem.h>
#include <inet/ipsec_conf.h>
#include <inet/tun.h>

/* Data structure to represent addresses */
struct srcid_map {
	struct srcid_map	*sm_next;
	in6_addr_t		sm_addr;	/* Local address */
	uint_t			sm_srcid;	/* source id */
	uint_t			sm_refcnt;	/* > 1 ipif with same addr? */
};
typedef struct srcid_map srcid_map_t;

static uint_t		srcid_nextid(void);
static srcid_map_t	**srcid_lookup_addr(const in6_addr_t *addr);
static srcid_map_t	**srcid_lookup_id(uint_t id);


/*
 * ID used to assign next free one.
 * Increases by one. Once it wraps we search for an unused ID.
 */
static uint_t		ip_src_id = 1;
static boolean_t	srcid_wrapped = B_FALSE;

static srcid_map_t	*srcid_head;
krwlock_t		srcid_lock;

/*
 * Insert/add a new address to the map.
 * Returns zero if ok; otherwise errno (e.g. for memory allocation failure).
 */
int
ip_srcid_insert(const in6_addr_t *addr)
{
	srcid_map_t	**smpp;
#ifdef DEBUG
	char		abuf[INET6_ADDRSTRLEN];

	ip1dbg(("ip_srcid_insert(%s)\n",
	    inet_ntop(AF_INET6, addr, abuf, sizeof (abuf))));
#endif

	rw_enter(&srcid_lock, RW_WRITER);
	smpp = srcid_lookup_addr(addr);
	if (*smpp != NULL) {
		/* Already present - increment refcount */
		(*smpp)->sm_refcnt++;
		ASSERT((*smpp)->sm_refcnt != 0);	/* wraparound */
		rw_exit(&srcid_lock);
		return (0);
	}
	/* Insert new */
	*smpp = kmem_alloc(sizeof (srcid_map_t), KM_NOSLEEP);
	if (*smpp == NULL) {
		rw_exit(&srcid_lock);
		return (ENOMEM);
	}
	(*smpp)->sm_next = NULL;
	(*smpp)->sm_addr = *addr;
	(*smpp)->sm_srcid = srcid_nextid();
	(*smpp)->sm_refcnt = 1;

	rw_exit(&srcid_lock);
	return (0);
}

/*
 * Remove an new address from the map.
 * Returns zero if ok; otherwise errno (e.g. for nonexistent address).
 */
int
ip_srcid_remove(const in6_addr_t *addr)
{
	srcid_map_t	**smpp;
	srcid_map_t	*smp;
#ifdef DEBUG
	char		abuf[INET6_ADDRSTRLEN];

	ip1dbg(("ip_srcid_remove(%s)\n",
	    inet_ntop(AF_INET6, addr, abuf, sizeof (abuf))));
#endif

	rw_enter(&srcid_lock, RW_WRITER);
	smpp = srcid_lookup_addr(addr);
	smp = *smpp;
	if (smp == NULL) {
		/* Not preset */
		rw_exit(&srcid_lock);
		return (ENOENT);
	}

	/* Decrement refcount */
	ASSERT(smp->sm_refcnt != 0);
	smp->sm_refcnt--;
	if (smp->sm_refcnt != 0) {
		rw_exit(&srcid_lock);
		return (0);
	}
	/* Remove entry */
	*smpp = smp->sm_next;
	rw_exit(&srcid_lock);
	smp->sm_next = NULL;
	kmem_free(smp, sizeof (srcid_map_t));
	return (0);
}

/*
 * Map from an address to a source id.
 * If the address is unknown return the unknown id (zero).
 */
uint_t
ip_srcid_find_addr(const in6_addr_t *addr)
{
	srcid_map_t	**smpp;
	srcid_map_t	*smp;
	uint_t		id;

	rw_enter(&srcid_lock, RW_READER);
	smpp = srcid_lookup_addr(addr);
	smp = *smpp;
	if (smp == NULL) {
		char		abuf[INET6_ADDRSTRLEN];

		/* Not present - could be broadcast or multicast address */
		ip1dbg(("ip_srcid_find_addr: unknown %s\n",
		    inet_ntop(AF_INET6, addr, abuf, sizeof (abuf))));
		id = 0;
	} else {
		ASSERT(smp->sm_refcnt != 0);
		id = smp->sm_srcid;
	}
	rw_exit(&srcid_lock);
	return (id);
}

/*
 * Map from a source id to an address.
 * If the id is unknown return the unspecified address.
 */
void
ip_srcid_find_id(uint_t id, in6_addr_t *addr)
{
	srcid_map_t	**smpp;
	srcid_map_t	*smp;

	rw_enter(&srcid_lock, RW_READER);
	smpp = srcid_lookup_id(id);
	smp = *smpp;
	if (smp == NULL) {
		/* Not preset */
		ip1dbg(("ip_srcid_find_id: unknown %u\n", id));
		*addr = ipv6_all_zeros;
	} else {
		ASSERT(smp->sm_refcnt != 0);
		*addr = smp->sm_addr;
	}
	rw_exit(&srcid_lock);
}

/*
 * ndd report function
 */
/*ARGSUSED*/
int
ip_srcid_report(queue_t *q, mblk_t *mp, void *arg)
{
	srcid_map_t	*smp;
	char		abuf[INET6_ADDRSTRLEN];

	(void) mi_mpprintf(mp,
	    "addr                                              "
	    "id      refcnt");
	rw_enter(&srcid_lock, RW_READER);
	for (smp = srcid_head; smp != NULL; smp = smp->sm_next) {
		(void) mi_mpprintf(mp, "%46s %5u %5u",
		    inet_ntop(AF_INET6, &smp->sm_addr, abuf, sizeof (abuf)),
		    smp->sm_srcid, smp->sm_refcnt);
	}
	rw_exit(&srcid_lock);
	return (0);
}

/* Assign the next available ID */
static uint_t
srcid_nextid(void)
{
	uint_t id;
	srcid_map_t	**smpp;

	ASSERT(rw_owner(&srcid_lock) == curthread);

	if (!srcid_wrapped) {
		id = ip_src_id++;
		if (ip_src_id == 0)
			srcid_wrapped = B_TRUE;
		return (id);
	}
	/* Once it wraps we search for an unused ID. */
	for (id = 0; id < 0xffffffff; id++) {
		smpp = srcid_lookup_id(id);
		if (*smpp == NULL)
			return (id);
	}
	cmn_err(CE_PANIC, "srcid_nextid: No free identifiers!");
	/* NOTREACHED */
}

/*
 * Lookup based on address.
 * Always returns a non-null pointer.
 * If found then *ptr will be the found object.
 * Otherwise *ptr will be NULL and can be used to insert a new object.
 */
static srcid_map_t **
srcid_lookup_addr(const in6_addr_t *addr)
{
	srcid_map_t	**smpp;

	ASSERT(RW_LOCK_HELD(&srcid_lock));
	smpp = &srcid_head;
	while (*smpp != NULL) {
		if (IN6_ARE_ADDR_EQUAL(&(*smpp)->sm_addr, addr))
			return (smpp);
		smpp = &(*smpp)->sm_next;
	}
	return (smpp);
}

/*
 * Lookup based on address.
 * Always returns a non-null pointer.
 * If found then *ptr will be the found object.
 * Otherwise *ptr will be NULL and can be used to insert a new object.
 */
static srcid_map_t **
srcid_lookup_id(uint_t id)
{
	srcid_map_t	**smpp;

	ASSERT(RW_LOCK_HELD(&srcid_lock));
	smpp = &srcid_head;
	while (*smpp != NULL) {
		if ((*smpp)->sm_srcid == id)
			return (smpp);
		smpp = &(*smpp)->sm_next;
	}
	return (smpp);
}
