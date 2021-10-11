/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip_multi.c	1.42	99/12/02 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

#include <sys/param.h>
#include <sys/socket.h>
#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip_ndp.h>
#include <inet/ip_multi.h>

#include <netinet/igmp.h>

static int	ilm_numentries_v6(ill_t *ill, const in6_addr_t *group);
static ilm_t	*ilm_add_v6(ipif_t *ipif, const in6_addr_t *group);
static void	ilm_delete(ilm_t *ilm);
static int	ip_ll_addmulti_v6(ipif_t *ipif, const in6_addr_t *group);
static int	ip_ll_delmulti_v6(ipif_t *ipif, const in6_addr_t *group);
static int	ip_join_allmulti(ipif_t *ipif);
static int	ip_leave_allmulti(ipif_t *ipif);
static ilg_t	*ilg_lookup_ipif(ipc_t *ipc, ipaddr_t group,
		    ipif_t *ipif);
static int	ilg_add(ipc_t *ipc, ipaddr_t group, ipif_t *ipif);
static int	ilg_add_v6(ipc_t *ipc, const in6_addr_t *group, ill_t *ill);
static int	ilg_delete(ipc_t *ipc, ilg_t *ilg);
static mblk_t	*ill_create_dl(ill_t *ill, uint32_t dl_primitive,
		    uint32_t length, uint32_t *addr_lenp, uint32_t *addr_offp);
static mblk_t	*ill_create_squery(ill_t *ill, ipaddr_t ipaddr,
		    uint32_t addrlen, uint32_t addroff, mblk_t *mp_tail);

/*
 * INADDR_ANY means all multicast addresses. This is only used
 * by the multicast router.
 * INADDR_ANY is stored as IPv6 unspecified addr.
 */
int
ip_addmulti(group, ipif)
	ipaddr_t	group;
	ipif_t	*ipif;
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t 	*ilm;
	in6_addr_t v6group;

	ip1dbg(("ip_addmulti: 0x%x on %s\n", ntohl(group), ill->ill_name));
	if (!CLASSD(group) && group != INADDR_ANY)
		return (EINVAL);

	/*
	 * INADDR_ANY is represented as the IPv6 unspecifed addr.
	 */
	if (group == INADDR_ANY)
		v6group = ipv6_all_zeros;
	else
		IN6_IPADDR_TO_V4MAPPED(group, &v6group);

	/*
	 * Look for a match on the ipif.
	 * (IP_ADD_MEMBERSHIP specifies an ipif using an IP address).
	 */
	ilm = ilm_lookup_ipif(ipif, group);
	if (ilm != NULL) {
		ilm->ilm_refcnt++;
		ip1dbg(("ip_addmulti: already there\n"));
		return (0);
	}

	ilm = ilm_add_v6(ipif, &v6group);
	if (ilm == NULL)
		return (ENOMEM);

	if (group == INADDR_ANY) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if more then one we should not tell the driver to join
		 * this time
		 */
		if (ilm_numentries_v6(ill, &v6group) > 1)
			return (0);
		return (ip_join_allmulti(ipif));
	}
	if ((ipif->ipif_flags & IFF_LOOPBACK) == 0)
		igmp_joingroup(ilm);

	/*
	 * Check how many ipif's that have members in this group -
	 * if more then one we should not tell the driver to join
	 * this time
	 */
	if (ilm_numentries_v6(ill, &v6group) > 1)
		return (0);
	return (ip_ll_addmulti_v6(ipif, &v6group));
}

/*
 * The unspecified address means all multicast addresses.
 * This is only used by the multicast router.
 */
int
ip_addmulti_v6(const in6_addr_t *v6group, ipif_t *ipif)
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm;
	char	groupbuf[INET6_ADDRSTRLEN];

	ip1dbg(("ip_addmulti_v6: %s on %s\n", inet_ntop(AF_INET6,
	    (void *)v6group, groupbuf, sizeof (groupbuf)), ill->ill_name));

	if (!IN6_IS_ADDR_MULTICAST(v6group) &&
	    !IN6_IS_ADDR_UNSPECIFIED(v6group))
	    return (EINVAL);

	/*
	 * Look for a match on the ill.
	 * (IPV6_JOIN_GROUP specifies an ill using an ifindex).
	 */
	if (ilm = ilm_lookup_ill_v6(ill, v6group)) {
		ilm->ilm_refcnt++;
		ip1dbg(("ip_addmulti_v6: already there\n"));
		return (0);
	}

	ilm = ilm_add_v6(ipif, v6group);
	if (ilm == NULL)
		return (ENOMEM);

	if ((ipif->ipif_flags & IFF_LOOPBACK) == 0) {
		mld_joingroup(ilm);
	}

	/*
	 * Check how many ipif's that have members in this group -
	 * if more then one we should not tell the driver to join
	 * this time
	 */
	if (ilm_numentries_v6(ill, v6group) > 1)
		return (0);
	return (ip_ll_addmulti_v6(ipif, v6group));
}

static int
ip_ll_addmulti_v6(ipif_t *ipif, const in6_addr_t *v6groupp)
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	char	group_buf[INET6_ADDRSTRLEN];
	char	group_buf2[INET6_ADDRSTRLEN];
	uint32_t	addrlen, addroff;

	ip1dbg(("ip_ll_addmulti_v6: %s on %s (%s)\n",
	    inet_ntop(AF_INET6, v6groupp, group_buf, sizeof (group_buf)),
	    ill->ill_name,
	    inet_ntop(AF_INET6, &ipif->ipif_v6lcl_addr, group_buf2,
	    sizeof (group_buf2))));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		ip1dbg(("ip_ll_addmulti_v6: not resolver\n"));
		return (0);	/* Must be IRE_IF_NORESOLVER */
	}

	if (ipif->ipif_flags & IFF_MULTI_BCAST) {
		ip1dbg(("ip_ll_addmulti_v6: MULTI_BCAST\n"));
		return (0);
	}
	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		ip1dbg(("ip_ll_addmulti_v6: nobody up\n"));
		return (0);
	}

	/*
	 * Create a AR_ENTRY_SQUERY message with a dl_enabmulti_req tacked
	 * on.
	 */
	mp = ill_create_dl(ill, DL_ENABMULTI_REQ, sizeof (dl_enabmulti_req_t),
	    &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);
	if (IN6_IS_ADDR_V4MAPPED(v6groupp)) {
		ipaddr_t v4group;

		IN6_V4MAPPED_TO_IPADDR(v6groupp, v4group);
		mp = ill_create_squery(ill, v4group, addrlen, addroff, mp);
		if (!mp)
			return (ENOMEM);
		ip1dbg(("ip_ll_addmulti_v6 (v4): putnext 0x%x on %s\n",
		    (int)ntohl(v4group), ill->ill_name));
		putnext(ill->ill_rq, mp);
	} else {
		ip1dbg(("ip_ll_addmulti_v6: ndp_squery_mp %s on %s\n",
		    inet_ntop(AF_INET6, v6groupp, group_buf,
		    sizeof (group_buf)),
		    ill->ill_name));
		return (ndp_mcastreq(ill, v6groupp, addrlen, addroff, mp));
	}
	return (0);
}

/*
 * INADDR_ANY means all multicast addresses. This is only used
 * by the multicast router.
 * INADDR_ANY is stored as the IPv6 unspecifed addr.
 */
int
ip_delmulti(ipaddr_t group, ipif_t *ipif)
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t *ilm;
	in6_addr_t v6group;

	ip1dbg(("ip_delmulti: 0x%x on %s\n", ntohl(group), ill->ill_name));
	if (!CLASSD(group) && group != INADDR_ANY)
		return (EINVAL);

	/*
	 * INADDR_ANY is represented as the IPv6 unspecifed addr.
	 */
	if (group == INADDR_ANY)
		v6group = ipv6_all_zeros;
	else
		IN6_IPADDR_TO_V4MAPPED(group, &v6group);

	/*
	 * Look for a match on the ipif.
	 * (IP_DROP_MEMBERSHIP specifies an ipif using an IP address).
	 */
	ilm = ilm_lookup_ipif(ipif, group);
	if (ilm == NULL) {
		return (ENOENT);
	}
	ilm->ilm_refcnt--;
	if (ilm->ilm_refcnt > 0) {
		ip1dbg(("ip_delmulti: still %d left\n", ilm->ilm_refcnt));
		return (0);
	}

	if (group == INADDR_ANY) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if there are still some left then don't tell the driver
		 * to drop it.
		 */
		ilm_delete(ilm);
		if (ilm_numentries_v6(ill, &v6group) != 0)
			return (0);
		return (ip_leave_allmulti(ipif));
	}

	if ((ipif->ipif_flags & IFF_LOOPBACK) == 0)
		igmp_leavegroup(ilm);
	ilm_delete(ilm);
	/*
	 * Check how many ipif's that have members in this group -
	 * if there are still some left then don't tell the driver
	 * to drop it.
	 */
	if (ilm_numentries_v6(ill, &v6group) != 0)
		return (0);
	return (ip_ll_delmulti_v6(ipif, &v6group));
}

/*
 * The unspecified address means all multicast addresses.
 * This is only used by the multicast router.
 */
int
ip_delmulti_v6(const in6_addr_t *v6group, ipif_t *ipif)
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t *ilm;
	char	group_buf[INET6_ADDRSTRLEN];

	ip1dbg(("ip_delmulti_v6: %s on %s\n",
	    inet_ntop(AF_INET6, v6group, group_buf, sizeof (group_buf)),
	    ill->ill_name));

	if (!IN6_IS_ADDR_MULTICAST(v6group) &&
	    !IN6_IS_ADDR_UNSPECIFIED(v6group))
		return (EINVAL);

	/*
	 * Look for a match on the ill.
	 * (IPV6_LEAVE_GROUP specifies an ill using an ifindex).
	 */
	ilm = ilm_lookup_ill_v6(ill, v6group);
	if (ilm == NULL)
		return (ENOENT);

	ilm->ilm_refcnt--;
	if (ilm->ilm_refcnt > 0) {
		ip1dbg(("ip_delmulti_v6: still %d left\n", ilm->ilm_refcnt));
		return (0);
	}

	if (IN6_IS_ADDR_UNSPECIFIED(v6group)) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if there are still some left then don't tell the driver
		 * to drop it.
		 */
		ilm_delete(ilm);
		if (ilm_numentries_v6(ill, v6group) != 0)
			return (0);
		return (ip_leave_allmulti(ipif));
	}

	if ((ipif->ipif_flags & IFF_LOOPBACK) == 0) {
		mld_leavegroup(ilm);
	}
	ilm_delete(ilm);
	/*
	 * Check how many ipif's that have members in this group -
	 * if there are still some left then don't tell the driver
	 * to drop it.
	 */
	if (ilm_numentries_v6(ill, v6group) != 0)
		return (0);
	return (ip_ll_delmulti_v6(ipif, v6group));
}

static int
ip_ll_delmulti_v6(ipif_t *ipif, const in6_addr_t *v6groupp)
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	char	group_buf1[INET6_ADDRSTRLEN];
	char	group_buf2[INET6_ADDRSTRLEN];
	uint32_t	addrlen, addroff;

	ip1dbg(("ip_ll_delmulti_v6: %s on %s (%s)\n",
	    inet_ntop(AF_INET6, v6groupp, group_buf1, sizeof (group_buf1)),
	    ill->ill_name,
	    inet_ntop(AF_INET6, &ipif->ipif_v6lcl_addr, group_buf2,
	    sizeof (group_buf2))));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		return (0);	/* Must be IRE_IF_NORESOLVER */
	}
	if (ipif->ipif_flags & IFF_MULTI_BCAST) {
		ip1dbg(("ip_ll_delmulti: MULTI_BCAST\n"));
		return (0);
	}
	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		ip1dbg(("ip_ll_delmulti: nobody up\n"));
		return (0);
	}

	/*
	 * Create a AR_ENTRY_SQUERY message with a dl_disabmulti_req tacked
	 * on.
	 */
	mp = ill_create_dl(ill, DL_DISABMULTI_REQ,
	    sizeof (dl_disabmulti_req_t), &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);

	if (IN6_IS_ADDR_V4MAPPED(v6groupp)) {
		ipaddr_t v4group;

		IN6_V4MAPPED_TO_IPADDR(v6groupp, v4group);
		mp = ill_create_squery(ill, v4group, addrlen, addroff, mp);
		if (!mp)
			return (ENOMEM);
		ip1dbg(("ip_ll_delmulti_v6 (v4): putnext %s on %s\n",
		    inet_ntop(AF_INET6, v6groupp, group_buf1,
		    sizeof (group_buf1)),
		    ill->ill_name));
		putnext(ill->ill_rq, mp);
	} else {
		ip1dbg(("ip_ll_delmulti_v6 (v4): ndp_squery_mp %s on %s\n",
		    inet_ntop(AF_INET6, v6groupp, group_buf2,
		    sizeof (group_buf2)),
		    ill->ill_name));
		return (ndp_mcastreq(ill, v6groupp, addrlen, addroff, mp));
	}
	return (0);
}

/*
 * Make the driver pass up all multicast packets
 */
static int
ip_join_allmulti(ipif_t *ipif)
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	uint32_t	addrlen, addroff;
	char	addrbuf[INET6_ADDRSTRLEN];

	ip1dbg(("ip_join_allmulti: on %s addr %s\n", ill->ill_name,
	    inet_ntop(AF_INET6, &ipif->ipif_v6lcl_addr, addrbuf,
	    sizeof (addrbuf))));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		ip1dbg(("ip_join_allmulti: not resolver\n"));
		return (0);	/* Must be IRE_IF_NORESOLVER */
	}
	if (ipif->ipif_flags & IFF_MULTI_BCAST)
		return (0);

	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		return (0);
	}
	/* Create a dl_promiscon_req message */
	mp = ill_create_dl(ill, DL_PROMISCON_REQ, sizeof (dl_promiscon_req_t),
	    &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);
	/*
	 * send this directly to the DLPI provider - not through the resolver
	 */
	putnext(ill->ill_wq, mp);
	return (0);
}

/*
 * Make the driver stop passing up all multicast packets
 */
static int
ip_leave_allmulti(ipif_t *ipif)
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	uint32_t	addrlen, addroff;
	char    addrbuf[INET6_ADDRSTRLEN];

	ip1dbg(("ip_leave_allmulti: on %s addr %s\n", ill->ill_name,
	    inet_ntop(AF_INET6, &ipif->ipif_v6lcl_addr, addrbuf,
	    sizeof (addrbuf))));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		ip1dbg(("ip_leave_allmulti: not resolver\n"));

		return (0);	/* Must be IRE_IF_NORESOLVER */
	}
	if (ipif->ipif_flags & IFF_MULTI_BCAST)
		return (0);

	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		return (0);
	}
	/* Create a dl_promiscoff_req message */
	mp = ill_create_dl(ill, DL_PROMISCOFF_REQ,
	    sizeof (dl_promiscoff_req_t), &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);
	/*
	 * send this directly to the DLPI provider - not through the resolver
	 */
	putnext(ill->ill_wq, mp);
	return (0);
}

/*
 * Copy mp_orig and pass it in as a local message.
 */
void
ip_multicast_loopback(queue_t *q, ill_t *ill, mblk_t *mp_orig)
{
	mblk_t	*mp;
	mblk_t	*ipsec_mp;

	ip1dbg(("ip_multicast_loopback\n"));
	/* TODO this could use dup'ed messages except for the IP header. */
	mp = copymsg(mp_orig);
	if (mp != NULL) {
		if (mp->b_datap->db_type == M_CTL) {
			ipsec_mp = mp;
			mp = mp->b_cont;
		} else {
			ipsec_mp = mp;
		}
		ip_wput_local(q, ill, (ipha_t *)mp->b_rptr, ipsec_mp,
		    IRE_BROADCAST);
	} else {
		ip1dbg(("ip_multicast_loopback: copymsg failed: "
		    "base 0x%p, limit 0x%p, read 0x%p, write 0x%p\n",
		    (void *)mp_orig->b_datap->db_base,
		    (void *)mp_orig->b_datap->db_lim,
		    (void *)mp_orig->b_rptr, (void *)mp_orig->b_wptr));
	}
}

static area_t	ip_aresq_template = {
	AR_ENTRY_SQUERY,		/* cmd */
	sizeof (area_t)+IP_ADDR_LEN,	/* name offset */
	sizeof (area_t),	/* name len (filled by ill_arp_alloc) */
	IP_ARP_PROTO_TYPE,		/* protocol, from arps perspective */
	sizeof (area_t),			/* proto addr offset */
	IP_ADDR_LEN,			/* proto addr_length */
	0,				/* proto mask offset */
	/* Rest is initialized when used */
	0,				/* flags */
	0,				/* hw addr offset */
	0,				/* hw addr length */
};

static mblk_t *
ill_create_squery(ill_t *ill, ipaddr_t ipaddr, uint32_t addrlen,
    uint32_t addroff, mblk_t *mp_tail)
{
	mblk_t	*mp;
	area_t	*area;

	mp = ill_arp_alloc(ill, (uchar_t *)&ip_aresq_template, ipaddr);
	if (!mp) {
		freemsg(mp_tail);
		return (NULL);
	}
	area = (area_t *)mp->b_rptr;
	area->area_hw_addr_length = addrlen;
	area->area_hw_addr_offset = mp->b_wptr - mp->b_rptr + addroff;

	mp->b_cont = mp_tail;
	return (mp);
}

/*
 * Create a dlpi message with room for phys+sap. When we come back in
 * ip_wput_ctl() we will strip the sap for those primitives which
 * only need a physical address.
 */
static mblk_t *
ill_create_dl(ill_t *ill, uint32_t dl_primitive, uint32_t length,
    uint32_t *addr_lenp, uint32_t *addr_offp)
{
	mblk_t	*mp;
	uint32_t	hw_addr_length;
	char		*cp;
	uint32_t	offset;
	uint32_t 	size;

	*addr_lenp = *addr_offp = 0;

	hw_addr_length = ill->ill_phys_addr_length;
	if (!hw_addr_length) {
		ip0dbg(("ip_create_dl: hw addr length = 0\n"));
		return (NULL);
	}
	hw_addr_length += ((ill->ill_sap_length > 0) ? ill->ill_sap_length :
	    -ill->ill_sap_length);

	size = length;
	switch (dl_primitive) {
	case DL_ENABMULTI_REQ:
	case DL_DISABMULTI_REQ:
	case DL_UNITDATA_REQ:
		size += hw_addr_length;
		break;
	case DL_PROMISCON_REQ:
	case DL_PROMISCOFF_REQ:
		break;
	default:
		return (NULL);
	}
	mp = allocb(size, BPRI_HI);
	if (!mp)
		return (NULL);
	mp->b_wptr += size;
	mp->b_datap->db_type = M_PROTO;

	cp = (char *)mp->b_rptr;
	offset = length;

	switch (dl_primitive) {
	case DL_ENABMULTI_REQ: {
		dl_enabmulti_req_t *dl = (dl_enabmulti_req_t *)cp;

		dl->dl_primitive = dl_primitive;
		dl->dl_addr_offset = offset;
		dl->dl_addr_length = *addr_lenp = hw_addr_length;
		*addr_offp = offset;
		break;
	}
	case DL_DISABMULTI_REQ: {
		dl_disabmulti_req_t *dl = (dl_disabmulti_req_t *)cp;

		dl->dl_primitive = dl_primitive;
		dl->dl_addr_offset = offset;
		dl->dl_addr_length = *addr_lenp = hw_addr_length;
		*addr_offp = offset;
		break;
	}
	case DL_UNITDATA_REQ: {
		dl_unitdata_req_t *dl = (dl_unitdata_req_t *)cp;

		dl->dl_primitive = dl_primitive;
		dl->dl_dest_addr_offset = offset;
		dl->dl_dest_addr_length = *addr_lenp = hw_addr_length;
		*addr_offp = offset;
		break;
	}
	case DL_PROMISCON_REQ:
	case DL_PROMISCOFF_REQ: {
		dl_promiscon_req_t *dl = (dl_promiscon_req_t *)cp;

		dl->dl_primitive = dl_primitive;
		dl->dl_level = DL_PROMISC_MULTI;
		*addr_lenp = *addr_offp = 0;
		break;
	}
	}
	ip1dbg(("ill_create_dl: addr_len %d, addr_off %d\n",
		*addr_lenp, *addr_offp));
	return (mp);
}

void
ip_wput_ctl(queue_t *q, mblk_t *mp_orig)
{
	ill_t	*ill = (ill_t *)q->q_ptr;
	mblk_t	*mp = mp_orig;
	area_t	*area;
	uint8_t	*cp;

	ip1dbg(("ip_wput_ctl\n"));
	/* Check that we have a AR_ENTRY_SQUERY with a tacked on mblk */
	if ((mp->b_wptr - mp->b_rptr) < sizeof (area_t) ||
	    mp->b_cont == NULL) {
		putnext(q, mp);
		return;
	}
	area = (area_t *)mp->b_rptr;
	if (area->area_cmd != AR_ENTRY_SQUERY) {
		putnext(q, mp);
		return;
	}
	mp = mp->b_cont;
	cp = (uint8_t *)mp->b_rptr;
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
			dl->dl_addr_length -= ill->ill_sap_length;
		}
		/* Track the state if this is the first enabmulti */
		if (ill->ill_dlpi_multicast_state == IDMS_UNKNOWN)
			ill->ill_dlpi_multicast_state = IDMS_INPROGRESS;
		ip1dbg(("ip_wput_ctl: ENABMULTI\n"));
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
			dl->dl_addr_length -= ill->ill_sap_length;
		}
		ip1dbg(("ip_wput_ctl: DISABMULTI\n"));
		break;
	}
	default:
		ip1dbg(("ip_wput_ctl: default\n"));
		break;
	}
	freeb(mp_orig);
	putnext(q, mp);
}

/*
 * Redo the DLPI part of joining a group after the driver has been DL_DETACHed.
 * The driver looses knowledge of groups on detach.
 * Also handles groups that are joined when an interface is down.
 * Called from ipif_up_done* after the DL_ATTACH has completed.
 */
void
ill_recover_multicast(ill_t *ill)
{
	ilm_t	*ilm;
	char    addrbuf[INET6_ADDRSTRLEN];

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if more then one we make sure that this entry is first
		 * in the list.
		 */
		if (ilm_numentries_v6(ill, &ilm->ilm_v6addr) > 1 &&
		    ilm_lookup_ill_v6(ill, &ilm->ilm_v6addr) != ilm)
			continue;
		ip1dbg(("ill_recover_multicast: %s\n",
		    inet_ntop(AF_INET6, &ilm->ilm_v6addr, addrbuf,
		    sizeof (addrbuf))));
		if (IN6_IS_ADDR_UNSPECIFIED(&ilm->ilm_v6addr)) {
			(void) ip_join_allmulti(ill->ill_ipif);
		} else {
			(void) ip_ll_addmulti_v6(ill->ill_ipif,
			    &ilm->ilm_v6addr);
		}
	}
}

/* Find an ilm for matching the ill */
ilm_t *
ilm_lookup_ill(ill_t *ill, ipaddr_t group)
{
	in6_addr_t	v6group;

	/*
	 * INADDR_ANY is represented as the IPv6 unspecifed addr.
	 */
	if (group == INADDR_ANY)
		v6group = ipv6_all_zeros;
	else
		IN6_IPADDR_TO_V4MAPPED(group, &v6group);

	return (ilm_lookup_ill_v6(ill, &v6group));
}

/* Find an ilm for matching the ill */
ilm_t *
ilm_lookup_ill_v6(ill_t *ill, const in6_addr_t *v6group)
{
	ilm_t	*ilm;

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
		if (IN6_ARE_ADDR_EQUAL(&ilm->ilm_v6addr, v6group))
			return (ilm);
	}
	return (NULL);
}

/*
 * Found an ilm for the ipif. Only needed for IPv4 which does
 * ipif specific socket options.
 */
ilm_t *
ilm_lookup_ipif(ipif_t *ipif, ipaddr_t group)
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm;
	in6_addr_t	v6group;

	/*
	 * INADDR_ANY is represented as the IPv6 unspecifed addr.
	 */
	if (group == INADDR_ANY)
		v6group = ipv6_all_zeros;
	else
		IN6_IPADDR_TO_V4MAPPED(group, &v6group);

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
		if (ilm->ilm_ipif == ipif &&
		    IN6_ARE_ADDR_EQUAL(&ilm->ilm_v6addr, &v6group))
			return (ilm);
	}
	return (NULL);
}

/*
 * How many members on this ill?
 */
static int
ilm_numentries_v6(ill_t *ill, const in6_addr_t *v6group)
{
	ilm_t	*ilm;
	int i = 0;

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
		if (IN6_ARE_ADDR_EQUAL(&ilm->ilm_v6addr, v6group)) {
			i++;
		}
	}
	return (i);
}

#define	GETSTRUCT(structure, number)	\
	((structure *)mi_zalloc(sizeof (structure) * (number)))

/* Caller guarantees that the group is not already on the list */
static ilm_t *
ilm_add_v6(ipif_t *ipif, const in6_addr_t *v6group)
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm;

	ilm = GETSTRUCT(ilm_t, 1);
	if (ilm == NULL)
		return (NULL);
	ilm->ilm_v6addr = *v6group;
	ilm->ilm_refcnt = 1;
	ilm->ilm_ipif = ipif;
	ilm->ilm_next = ill->ill_ilm;
	ill->ill_ilm = ilm;
	return (ilm);
}

/*
 * Unlink ilm and free it.
 */
static void
ilm_delete(ilm_t *ilm)
{
	ipif_t	*ipif = ilm->ilm_ipif;
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	**ilmp;

	for (ilmp = &ill->ill_ilm; *ilmp; ilmp = &(*ilmp)->ilm_next) {
		if (*ilmp == ilm) {
			*ilmp = ilm->ilm_next;
			mi_free((char *)ilm);
			return;
		}
	}
	cmn_err(CE_PANIC, "ip: ilm_delete not found");
}

/* Free all ilms for this ipif */
void
ilm_free(ipif_t *ipif)
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm, *next_ilm;

	for (ilm = ill->ill_ilm; ilm; ilm = next_ilm) {
		next_ilm = ilm->ilm_next;
		if (ilm->ilm_ipif == ipif)
			ilm_delete(ilm);
	}
}

/*
 * Handle the IP_ADD_MEMBERSHIP optmgmt.
 */
int
ip_opt_add_group(ipc_t *ipc, boolean_t checkonly, ipaddr_t group,
    ipaddr_t ifaddr)
{
	ipif_t	*ipif;

	if (!CLASSD(group))
		return (EINVAL);

	if (ifaddr == 0)
		ipif = ipif_lookup_group(group);
	else
		ipif = ipif_lookup_addr(ifaddr, NULL);

	if (ipif == NULL) {
		ip1dbg((
		    "ip_opt_add_group: no ipif for group 0x%x, ifaddr 0x%x\n",
			(int)ntohl(group), (int)ntohl(ifaddr)));
		return (EADDRNOTAVAIL);
	}
	if (checkonly) {
		/*
		 * do not do operation, just pretend to - new T_CHECK
		 * semantics. The error return case above if encountered
		 * considered a good enough "check" here.
		 */
		return (0);
	}
	return (ilg_add(ipc, group, ipif));
}

/*
 * Handle the IPV6_JOIN_GROUP optmgmt.
 * Verifies that there is a source address of appropriate scope for
 * the group.
 * Handle IPv4-mapped IPv6 multicast addresses by associating them
 * with the link-local ipif.
 */
int
ip_opt_add_group_v6(ipc_t *ipc, boolean_t checkonly, const in6_addr_t *v6group,
    int ifindex)
{
	ill_t *ill;
	ipif_t	*ipif;
	char v6group_buf[INET6_ADDRSTRLEN];
	ipaddr_t v4group;
	boolean_t isv6;

	if (IN6_IS_ADDR_V4MAPPED(v6group)) {
		/* Do the IPv4 thing */
		IN6_V4MAPPED_TO_IPADDR(v6group, v4group);
		if (!CLASSD(v4group))
			return (EINVAL);
		isv6 = B_FALSE;
	} else {
		if (!IN6_IS_ADDR_MULTICAST(v6group))
			return (EINVAL);
		isv6 = B_TRUE;
	}

	if (ifindex == 0) {
		if (isv6)
			ipif = ipif_lookup_group_v6(v6group);
		else
			ipif = ipif_lookup_group(v4group);
		if (ipif == NULL) {
			ip1dbg((
			    "ip_opt_add_group:_v6 no ill for group %s\n",
			    inet_ntop(AF_INET6, v6group,
			    v6group_buf, sizeof (v6group_buf))));
			return (EADDRNOTAVAIL);
		}
		ill = ipif->ipif_ill;
	} else {
		if (isv6)
			ill = ill_lookup_on_ifindex(ifindex, B_TRUE);
		else
			ill = ill_lookup_on_ifindex(ifindex, B_FALSE);
		if (ill == NULL) {
			ip1dbg((
			    "ip_opt_add_group:_v6 no ill for ifindex %d\n",
			    ifindex));
			return (ENXIO);
		}
	}
	if (checkonly) {
		/*
		 * do not do operation, just pretend to - new T_CHECK
		 * semantics. The error return case above if encountered
		 * considered a good enough "check" here.
		 */
		return (0);
	}
	if (!isv6) {
		/* Pick the first ipif on the ill */
		ipif = ill->ill_ipif;
		return (ilg_add(ipc, v4group, ipif));
	} else {
		return (ilg_add_v6(ipc, v6group, ill));
	}
}

/*
 * Handle the IP_DROP_MEMBERSHIP optmgmt.
 */
int
ip_opt_delete_group(ipc_t *ipc, boolean_t checkonly, ipaddr_t group,
    ipaddr_t ifaddr)
{
	ipif_t	*ipif;

	if (!CLASSD(group))
		return (EINVAL);

	if (ifaddr == 0)
		ipif = ipif_lookup_group(group);
	else
		ipif = ipif_lookup_addr(ifaddr, NULL);


	/*
	 * If the ipif has gone away we can't leave the group.
	 * Once the ipif is unplumbed things will be cleaned up.
	 */
	if (ipif == NULL) {
		ip1dbg(("ip_opt_delete_group: no ipif for group "
		    "0x%x, ifaddr 0x%x\n",
		    (int)ntohl(group), (int)ntohl(ifaddr)));
		return (EADDRNOTAVAIL);
	}
	if (checkonly) {
		/*
		 * do not do operation, just pretend to - new T_CHECK
		 * semantics. The error return case above if encountered
		 * considered a good enough "check" here.
		 */
		return (0);
	}
	return (ilg_delete(ipc, ilg_lookup_ipif(ipc, group, ipif)));
}

/*
 * Handle the IPV6_LEAVE_GROUP optmgmt.
 * Handle IPv4-mapped IPv6 multicast addresses by associating them
 * with the link-local ipif.
 */
int
ip_opt_delete_group_v6(ipc_t *ipc, boolean_t checkonly,
    const in6_addr_t *v6group, int ifindex)
{
	ill_t *ill;
	ipif_t	*ipif;
	char v6group_buf[INET6_ADDRSTRLEN];
	ipaddr_t v4group;
	boolean_t isv6;

	if (IN6_IS_ADDR_V4MAPPED(v6group)) {
		/* Do the IPv4 thing */
		IN6_V4MAPPED_TO_IPADDR(v6group, v4group);
		if (!CLASSD(v4group))
			return (EINVAL);
		isv6 = B_FALSE;
	} else {
		if (!IN6_IS_ADDR_MULTICAST(v6group))
			return (EINVAL);
		isv6 = B_TRUE;
	}

	if (ifindex == 0) {
		if (isv6)
			ipif = ipif_lookup_group_v6(v6group);
		else
			ipif = ipif_lookup_group(v4group);
		/*
		 * If the ipif has gone away we can't leave the group.
		 * Once the ill is unplumbed things will be cleaned up.
		 */
		if (ipif == NULL) {
			ip1dbg(("ip_opt_delete_group_v6: no ipif for group "
			    "%s\n",
			    inet_ntop(AF_INET6, v6group,
			    v6group_buf, sizeof (v6group_buf))));
			return (EADDRNOTAVAIL);
		}
		ill = ipif->ipif_ill;
	} else {
		if (isv6)
			ill = ill_lookup_on_ifindex(ifindex, B_TRUE);
		else
			ill = ill_lookup_on_ifindex(ifindex, B_FALSE);
		/*
		 * If the ill has gone away we can't leave the group.
		 * Once the ill is unplumbed things will be cleaned up.
		 */
		if (ill == NULL) {
			ip1dbg(("ip_opt_delete_group_v6: no ipif for "
			    "ifindex %d\n",
			    ifindex));
			return (ENXIO);
		}
	}
	if (checkonly) {
		/*
		 * do not do operation, just pretend to - new T_CHECK
		 * semantics. The error return case above if encountered
		 * considered a good enough "check" here.
		 */
		return (0);
	}
	if (!isv6) {
		/* Pick the first ipif on the ill */
		ipif = ill->ill_ipif;
		return (ilg_delete(ipc, ilg_lookup_ipif(ipc, v4group, ipif)));
	} else {
		return (ilg_delete(ipc, ilg_lookup_ill_v6(ipc, v6group, ill)));
	}
}

/*
 * Group mgmt for upper ipc that passes things down
 * to the interface multicast list (and DLPI)
 * These routines can handle new style options that specify an interface name
 * as opposed to an interface address (needed for general handling of
 * unnumbered interfaces.)
 */

#define	ILG_ALLOC_CHUNK	16

/*
 * Add a group to an upper ipc group data structure and pass things down
 * to the interface multicast list (and DLPI)
 */
static int
ilg_add(ipc_t *ipc, ipaddr_t group, ipif_t *ipif)
{
	int	error;

	if (!(ipif->ipif_flags & IFF_MULTICAST))
		return (EADDRNOTAVAIL);

	if (ilg_lookup_ipif(ipc, group, ipif) != NULL)
		return (EADDRINUSE);

	if (error = ip_addmulti(group, ipif))
		return (error);
	if (!ipc->ipc_ilg) {
		/* Allocate first chunk */
		ipc->ipc_ilg = GETSTRUCT(ilg_t, ILG_ALLOC_CHUNK);
		if (ipc->ipc_ilg == NULL) {
			(void) ip_delmulti(group, ipif);
			return (ENOMEM);
		}
		ipc->ipc_ilg_allocated = ILG_ALLOC_CHUNK;
		ipc->ipc_ilg_inuse = 0;
	}
	if (ipc->ipc_ilg_inuse >= ipc->ipc_ilg_allocated) {
		/* Allocate next larger chunk and copy old into new */
		ilg_t	*new;

		new = GETSTRUCT(ilg_t,
		    ipc->ipc_ilg_allocated + ILG_ALLOC_CHUNK);
		if (new == NULL) {
			(void) ip_delmulti(group, ipif);
			return (ENOMEM);
		}
		bcopy(ipc->ipc_ilg, new,
		    sizeof (ilg_t) * ipc->ipc_ilg_allocated);
		mi_free((char *)ipc->ipc_ilg);
		ipc->ipc_ilg = new;
		ipc->ipc_ilg_allocated += ILG_ALLOC_CHUNK;
	}
	if (group == INADDR_ANY) {
		ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_v6group =
		    ipv6_all_zeros;
	} else {
		IN6_IPADDR_TO_V4MAPPED(group,
		    &ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_v6group);
	}
	ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_ipif = ipif;
	ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_ill = ipif->ipif_ill;
	ipc->ipc_ilg_inuse++;
	return (0);
}

static int
ilg_add_v6(ipc_t *ipc, const in6_addr_t *v6group, ill_t *ill)
{
	int	error;
	ipif_t	*ipif = ill->ill_ipif;

	if (!(ipif->ipif_flags & IFF_MULTICAST))
		return (EADDRNOTAVAIL);

	if (ilg_lookup_ill_v6(ipc, v6group, ill) != NULL)
		return (EADDRINUSE);

	if (error = ip_addmulti_v6(v6group, ipif))
		return (error);
	if (!ipc->ipc_ilg) {
		/* Allocate first chunk */
		ipc->ipc_ilg = GETSTRUCT(ilg_t, ILG_ALLOC_CHUNK);
		if (ipc->ipc_ilg == NULL) {
			(void) ip_delmulti_v6(v6group, ipif);
			return (ENOMEM);
		}
		ipc->ipc_ilg_allocated = ILG_ALLOC_CHUNK;
		ipc->ipc_ilg_inuse = 0;
	}
	if (ipc->ipc_ilg_inuse >= ipc->ipc_ilg_allocated) {
		/* Allocate next larger chunk and copy old into new */
		ilg_t	*new;

		new = GETSTRUCT(ilg_t,
		    ipc->ipc_ilg_allocated + ILG_ALLOC_CHUNK);
		if (new == NULL) {
			(void) ip_delmulti_v6(v6group, ipif);
			return (ENOMEM);
		}
		bcopy(ipc->ipc_ilg, new,
		    sizeof (ilg_t) * ipc->ipc_ilg_allocated);
		mi_free((char *)ipc->ipc_ilg);
		ipc->ipc_ilg = new;
		ipc->ipc_ilg_allocated += ILG_ALLOC_CHUNK;
	}
	ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_v6group = *v6group;
	ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_ipif = ipif;
	ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_ill = ipif->ipif_ill;
	ipc->ipc_ilg_inuse++;
	return (0);
}

/*
 * Find an IPv4 ilg matching group and ill
 */
ilg_t *
ilg_lookup_ill(ipc_t *ipc, ipaddr_t group, ill_t *ill)
{
	in6_addr_t v6group;

	/*
	 * INADDR_ANY is represented as the IPv6 unspecifed addr.
	 */
	if (group == INADDR_ANY)
		v6group = ipv6_all_zeros;
	else
		IN6_IPADDR_TO_V4MAPPED(group, &v6group);

	return (ilg_lookup_ill_v6(ipc, &v6group, ill));
}

/*
 * Find an IPv6 ilg matching group and ill
 */
ilg_t *
ilg_lookup_ill_v6(ipc_t *ipc, const in6_addr_t *v6group, ill_t *ill)
{
	int	i;

	for (i = 0; i < ipc->ipc_ilg_inuse; i++) {
		if (ipc->ipc_ilg[i].ilg_ill == ill &&
		    IN6_ARE_ADDR_EQUAL(&ipc->ipc_ilg[i].ilg_v6group, v6group))
			return (&ipc->ipc_ilg[i]);
	}
	return (NULL);
}

/*
 * Find an IPv4 ilg matching group and ipif
 */
static ilg_t *
ilg_lookup_ipif(ipc_t *ipc, ipaddr_t group, ipif_t *ipif)
{
	in6_addr_t v6group;
	int	i;

	if (group == INADDR_ANY)
		v6group = ipv6_all_zeros;
	else
		IN6_IPADDR_TO_V4MAPPED(group, &v6group);

	for (i = 0; i < ipc->ipc_ilg_inuse; i++) {
		if (IN6_ARE_ADDR_EQUAL(&ipc->ipc_ilg[i].ilg_v6group,
		    &v6group) &&
		    ipc->ipc_ilg[i].ilg_ipif == ipif)
			return (&ipc->ipc_ilg[i]);
	}
	return (NULL);
}

static int
ilg_delete(ipc_t *ipc, ilg_t *ilg)
{
	int	i;

	if (ilg == NULL)
		return (ENOENT);

	ASSERT(ilg->ilg_ipif != NULL);
	if (IN6_IS_ADDR_V4MAPPED(&ilg->ilg_v6group)) {
		(void) ip_delmulti(V4_PART_OF_V6(ilg->ilg_v6group),
		    ilg->ilg_ipif);
	} else {
		(void) ip_delmulti_v6(&ilg->ilg_v6group, ilg->ilg_ipif);
	}
	i = ilg - &ipc->ipc_ilg[0];
	ASSERT(i >= 0 && i < ipc->ipc_ilg_inuse);

	/* Move other entries up one step */
	ipc->ipc_ilg_inuse--;
	for (; i < ipc->ipc_ilg_inuse; i++)
		ipc->ipc_ilg[i] = ipc->ipc_ilg[i+1];

	if (ipc->ipc_ilg_inuse == 0) {
		mi_free((char *)ipc->ipc_ilg);
		ipc->ipc_ilg = NULL;
	}
	return (0);
}

void
ilg_delete_all(ipc_t *ipc)
{
	int	i;

	if (ipc->ipc_ilg_inuse == 0)
		return;

	for (i = ipc->ipc_ilg_inuse - 1; i >= 0; i--) {
		(void) ilg_delete(ipc, &ipc->ipc_ilg[i]);
	}
}

/*
 * ipc_walk function for clearing ipc_ilg and ipc_multicast_ipif
 * for a given ipif
 */
static void
ipc_delete_ipif(ipc_t *ipc, caddr_t arg)
{
	ipif_t	*ipif = (ipif_t *)arg;
	int	i;
	char	group_buf[INET6_ADDRSTRLEN];
	ire_t	*ire;

	for (i = ipc->ipc_ilg_inuse - 1; i >= 0; i--) {
		if (ipc->ipc_ilg[i].ilg_ipif == ipif) {
			/* Blow away the membership */
			ip1dbg(("ipc_delete_ilg_ipif: %s on %s (%s)\n",
			    inet_ntop(AF_INET6, &ipc->ipc_ilg[i].ilg_v6group,
			    group_buf, IPV6_ADDR_LEN),
			    inet_ntop(AF_INET6, &ipif->ipif_v6lcl_addr,
			    group_buf, sizeof (group_buf)),
			    ipif->ipif_ill->ill_name));
			(void) ilg_delete(ipc, &ipc->ipc_ilg[i]);
		}
	}
	if (ipc->ipc_multicast_ipif == ipif) {
		/* Revert to late binding */
		ipc->ipc_multicast_ipif = NULL;
	}

	/*
	 * Look at the cached ires on ipcs which has pointers to ipifs.
	 * We just call ire_refrele which clears up the reference
	 * to ire. Eventually ipif_down should delete all the ires
	 * pointing at this ipif which will then free this ire.
	 *
	 * NOTE : Though we are called as writer, we grab the lock
	 *	  just to be consistent with other parts of the code.
	 */
	mutex_enter(&ipc->ipc_irc_lock);
	ire = ipc->ipc_ire_cache;
	if (ire != NULL && ire->ire_ipif == ipif) {
		ipc->ipc_ire_cache = NULL;
		mutex_exit(&ipc->ipc_irc_lock);
		ire_refrele(ire);
		return;
	}
	mutex_exit(&ipc->ipc_irc_lock);
}

/*
 * ipc_walk function for clearing ipc_ilg and ipc_multicast_ill for a given ill
 */
static void
ipc_delete_ill(ipc_t *ipc, caddr_t arg)
{
	ill_t	*ill = (ill_t *)arg;
	int	i;
	char	group_buf[INET6_ADDRSTRLEN];

	for (i = ipc->ipc_ilg_inuse - 1; i >= 0; i--) {
		if (ipc->ipc_ilg[i].ilg_ill == ill) {
			/* Blow away the membership */
			ip1dbg(("ipc_delete_ilg_ill: %s on %s\n",
			    inet_ntop(AF_INET6, &ipc->ipc_ilg[i].ilg_v6group,
			    group_buf, sizeof (group_buf)),
			    ill->ill_name));
			(void) ilg_delete(ipc, &ipc->ipc_ilg[i]);
		}
	}
	if (ipc->ipc_multicast_ill == ill) {
		/* Revert to late binding */
		ipc->ipc_multicast_ill = NULL;
	}
}

/*
 * Called when an ipif is unplumbed to make sure that there are no
 * dangling ipc references to that ipif.
 * Handles ilg_ipif and ipc_multicast_ipif
 */
void
reset_ipc_ipif(ipif)
	ipif_t	*ipif;
{
	ipc_walk(ipc_delete_ipif, (caddr_t)ipif);
}

/*
 * Called when an ill is unplumbed to make sure that there are no
 * dangling ipc references to that ill.
 * Handles ilg_ill, ipc_multicast_ill.
 */
void
reset_ipc_ill(ill)
	ill_t	*ill;
{
	ipc_walk(ipc_delete_ill, (caddr_t)ill);
}
