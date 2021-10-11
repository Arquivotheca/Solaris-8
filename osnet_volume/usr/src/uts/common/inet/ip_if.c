/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip_if.c	1.137	99/11/07 SMI"

/*
 * This file contains the interface control functions for IP.
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/sysmacros.h>
#include <sys/strlog.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/kstat.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>

#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/socket.h>
#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>
#include <sys/isa_defs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/igmp_var.h>
#include <sys/strick.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/mib2.h>
#include <inet/ip.h>
#include <inet/ipsec_conf.h>
#include <inet/ip6.h>
#include <inet/tcp.h>
#include <inet/ip_multi.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>
#include <inet/ip_ndp.h>
#include <inet/ip_if.h>

#include <netinet/igmp.h>
#include <netinet/ip_mroute.h>

/* The character which tells where the ill_name ends */
#define	IPIF_SEPARATOR_CHAR	':'
#define	IP_ARP_HW_MAPPING_START	2

#define	IP_MULTI_EXTRACT_MASK		0x007fffff

/* IP ioctl function table entry */
typedef struct ipft_s {
	int	ipft_cmd;
	pfi_t	ipft_pfi;
	int	ipft_min_size;
	int	ipft_flags;
} ipft_t;
#define	IPFT_F_NO_REPLY		0x1

typedef struct ip_sock_ar_s {
	union {
		area_t	ip_sock_area;
		ared_t	ip_sock_ared;
		areq_t	ip_sock_areq;
	} ip_sock_ar_u;
	queue_t	*ip_sock_ar_q;
} ip_sock_ar_t;

/* XXX Should these be in a .h file? */
extern void ipsec_config_flush(queue_t *, mblk_t *);
extern void ipsec_config_list(queue_t *, mblk_t *);
extern int ipsec_config_add(mblk_t *);
extern int ipsec_config_delete(mblk_t *);

static int	if_unitsel(queue_t *q, mblk_t *mp, uint_t ppa);
static int	ill_dl_up(ill_t *ill, ipif_t *ipif, mblk_t *mp, queue_t *q);
static void	ill_downi(ire_t *ire, char *ill_arg);
static boolean_t ip_addr_availability_check(ipif_t *new_ipif);
static boolean_t ip_local_addr_ok(ipaddr_t addr, ipaddr_t subnet_mask);
static boolean_t ip_remote_addr_ok(ipaddr_t addr, ipaddr_t subnet_mask);
static ip_m_t	*ip_m_lookup(ill_t *ill, t_uscalar_t mac_type);
static int	ip_siocaddrt(struct rtentry *rt);
static int	ip_siocdelrt(struct rtentry *rt);
static int	ip_sioctl_addif(ipif_t *ipif, char *name, sin_t *ipa,
    boolean_t isv6, queue_t *q, mblk_t *mp);
static int	ip_sioctl_brdaddr(ipif_t *ipif, sin_t *ipa, queue_t *q,
    mblk_t *mp);
static int	ip_sioctl_dstaddr(ipif_t *ipif, sin_t *ipa, queue_t *q,
    mblk_t *mp);
static int	ip_sioctl_flags(ipif_t *ipif, int flags, queue_t *q, mblk_t *mp,
		    int cmd);
static int	ip_sioctl_lnkinfo(ipif_t *ipif, struct lif_ifinfo_req *lir,
    queue_t *q, mblk_t *mp);
static int	ip_sioctl_mtu(ipif_t *ipif, int mtu, queue_t *q,
    mblk_t *mp);
static int	ip_sioctl_netmask(ipif_t *ipif, sin_t *ipa, queue_t *q,
    mblk_t *mp);
static int	ip_sioctl_removeif(ipif_t *ipif, sin_t *ipa);
static int	ip_sioctl_slifindex(ipif_t *ipif, uint_t index);
static int	ip_sioctl_slifname(struct lifreq *lifr,
    queue_t *q, mblk_t *mp);
static int	ip_sioctl_subnet(ipif_t *ipif, sin_t *ipa, int addrlen,
    queue_t *q, mblk_t *mp);
static int	ip_sioctl_token(ipif_t *ipif, sin6_t *sin6, int addrlen,
    queue_t *q, mblk_t *mp);
static ipaddr_t	ip_subnet_mask(ipaddr_t addr);

static void	ip_wput_ioctl(queue_t *q, mblk_t *mp);

static ipif_t	*ipif_allocate(ill_t *ill, int id, uint_t ire_type);
static void	ipif_arp_down(ipif_t *ipif);
static int	ipif_arp_off(ipif_t *ipif, uint32_t addr);
static int	ipif_arp_on(ipif_t *ipif, uint32_t addr);
static void	ipif_check_bcast_ires(ipif_t *test_ipif);
static void	ipif_down_delete_ire(ire_t *ire, char *ipif);
static void	ipif_free(ipif_t *ipif);
static void	ipif_mask_reply(ipif_t *);
static void	ipif_mtu_change(ire_t *, char *);
static void	ipif_set_default(ipif_t *ipif);
static int	ipif_set_values(queue_t *q, mblk_t *mp,
		    char *interf_name, uint_t *ppa);

/*
 * Multicast address mappings used over Ethernet/802.X.
 * This address is used as base for mappings.
 *
 * TODO Make these configurable e.g. by putting them in (the user extensible?)
 * ip_m_t.
 */
static uint8_t	ip_g_phys_multi_addr[] =
		{ 0x01, 0x00, 0x5e, 0x00, 0x00, 0x00 };

#define	IP_PHYS_MULTI_ADDR_LENGTH 	sizeof (ip_g_phys_multi_addr)
/*
 * The field values are larger than strictly necessary for simple
 * AR_ENTRY_ADDs but the padding lets us accomodate the socket ioctls.
 */
static area_t	ip_area_template = {
	AR_ENTRY_ADD,			/* area_cmd */
	sizeof (ip_sock_ar_t) + (IP_ADDR_LEN*2) + sizeof (sin_t),
					/* area_name_offset */
	/* area_name_length temporarily holds this structure length */
	sizeof (area_t),			/* area_name_length */
	IP_ARP_PROTO_TYPE,		/* area_proto */
	sizeof (ip_sock_ar_t),		/* area_proto_addr_offset */
	IP_ADDR_LEN,			/* area_proto_addr_length */
	sizeof (ip_sock_ar_t) + IP_ADDR_LEN,
					/* area_proto_mask_offset */
	0,				/* area_flags */
	sizeof (ip_sock_ar_t) + IP_ADDR_LEN + IP_ADDR_LEN,
					/* area_hw_addr_offset */
	/* Zero length hw_addr_length means 'use your idea of the address' */
	0				/* area_hw_addr_length */
};

static ared_t	ip_ared_template = {
	AR_ENTRY_DELETE,
	sizeof (ared_t) + IP_ADDR_LEN,
	sizeof (ared_t),
	IP_ARP_PROTO_TYPE,
	sizeof (ared_t),
	IP_ADDR_LEN
};

static areq_t	ip_areq_template = {
	AR_ENTRY_QUERY,			/* cmd */
	sizeof (areq_t)+(2*IP_ADDR_LEN),	/* name offset */
	sizeof (areq_t),	/* name len (filled by ill_arp_alloc) */
	IP_ARP_PROTO_TYPE,		/* protocol, from arps perspective */
	sizeof (areq_t),			/* target addr offset */
	IP_ADDR_LEN,			/* target addr_length */
	0,				/* flags */
	sizeof (areq_t) + IP_ADDR_LEN,	/* sender addr offset */
	IP_ADDR_LEN,			/* sender addr length */
	6,				/* xmit_count */
	1000,				/* (re)xmit_interval in milliseconds */
	4				/* max # of requests to buffer */
	/* anything else filled in by the code */
};

static arc_t	ip_aru_template = {
	AR_INTERFACE_UP,
	sizeof (arc_t),		/* Name offset */
	sizeof (arc_t)		/* Name length (set by ill_arp_alloc) */
};

static arc_t	ip_ard_template = {
	AR_INTERFACE_DOWN,
	sizeof (arc_t),		/* Name offset */
	sizeof (arc_t)		/* Name length (set by ill_arp_alloc) */
};

static arc_t	ip_aron_template = {
	AR_INTERFACE_ON,
	sizeof (arc_t),		/* Name offset */
	sizeof (arc_t)		/* Name length (set by ill_arp_alloc) */
};

static arc_t	ip_aroff_template = {
	AR_INTERFACE_OFF,
	sizeof (arc_t),		/* Name offset */
	sizeof (arc_t)		/* Name length (set by ill_arp_alloc) */
};


static arma_t	ip_arma_multi_bcast_template = {
	AR_MAPPING_ADD,
	sizeof (arma_t) + 3*IP_ADDR_LEN + IP_MAX_HW_LEN,
				/* Name offset */
	sizeof (arma_t),	/* Name length (set by ill_arp_alloc) */
	IP_ARP_PROTO_TYPE,
	sizeof (arma_t),			/* proto_addr_offset */
	IP_ADDR_LEN,				/* proto_addr_length */
	sizeof (arma_t) + IP_ADDR_LEN,		/* proto_mask_offset */
	sizeof (arma_t) + 2*IP_ADDR_LEN,	/* proto_extract_mask_offset */
	ACE_F_PERMANENT | ACE_F_MAPPING,	/* flags */
	sizeof (arma_t) + 3*IP_ADDR_LEN,	/* hw_addr_offset */
	IP_MAX_HW_LEN,				/* hw_addr_length */
	0,					/* hw_mapping_start */
};

static arma_t	ip_arma_multi_template = {
	AR_MAPPING_ADD,
	sizeof (arma_t) + 3*IP_ADDR_LEN + IP_PHYS_MULTI_ADDR_LENGTH,
				/* Name offset */
	sizeof (arma_t),		/* Name length (set by ill_arp_alloc) */
	IP_ARP_PROTO_TYPE,
	sizeof (arma_t),			/* proto_addr_offset */
	IP_ADDR_LEN,				/* proto_addr_length */
	sizeof (arma_t) + IP_ADDR_LEN,		/* proto_mask_offset */
	sizeof (arma_t) + 2*IP_ADDR_LEN,	/* proto_extract_mask_offset */
	ACE_F_PERMANENT | ACE_F_MAPPING,	/* flags */
	sizeof (arma_t) + 3*IP_ADDR_LEN,	/* hw_addr_offset */
	IP_PHYS_MULTI_ADDR_LENGTH,		/* hw_addr_length */
	IP_ARP_HW_MAPPING_START,		/* hw_mapping_start */
};

static ipft_t	ip_ioctl_ftbl[] = {
	{ IP_IOC_IRE_DELETE, ip_ire_delete, sizeof (ipid_t), 0 },
	{ IP_IOC_IRE_DELETE_NO_REPLY, ip_ire_delete, sizeof (ipid_t),
		IPFT_F_NO_REPLY },
	{ IP_IOC_IRE_ADVISE_NO_REPLY, ip_ire_advise, sizeof (ipic_t),
		IPFT_F_NO_REPLY },
	{ IP_IOC_RTS_REQUEST, ip_rts_request, 0, 0 },
	{ 0 }
};

/* Simple ICMP IP Header Template */
static ipha_t icmp_ipha = {
	IP_SIMPLE_HDR_VERSION, 0, 0, 0, 0, 0, IPPROTO_ICMP
};

/* Flag descriptors for ip_ipif_report */
static nv_t	ipif_nv_tbl[] = {
	{ IFF_UP,		"UP" },
	{ IFF_BROADCAST,	"BROADCAST" },
	{ IFF_DEBUG,		"DEBUG" },
	{ IFF_LOOPBACK,		"LOOPBACK" },
	{ IFF_POINTOPOINT,	"POINTOPOINT" },
	{ IFF_NOTRAILERS,	"NOTRAILERS" },
	{ IFF_RUNNING,		"RUNNING" },
	{ IFF_NOARP,		"NOARP" },
	{ IFF_PROMISC,		"PROMISC" },
	{ IFF_ALLMULTI,		"ALLMULTI" },
	{ IFF_INTELLIGENT,	"INTELLIGENT" },
	{ IFF_MULTICAST,	"MULTICAST" },
	{ IFF_MULTI_BCAST,	"MULTI_BCAST" },
	{ IFF_UNNUMBERED,	"UNNUMBERED" },
	{ IFF_DHCPRUNNING,	"DHCP" },
	{ IFF_PRIVATE,		"PRIVATE" },
	{ IFF_NOXMIT,		"NOXMIT" },
	{ IFF_NOLOCAL,		"NOLOCAL" },
	{ IFF_DEPRECATED,	"DEPRECATED" },
	{ IFF_ADDRCONF,		"ADDRCONF" },
	{ IFF_ROUTER,		"ROUTER" },
	{ IFF_NONUD,		"NONUD" },
	{ IFF_ANYCAST,		"ANYCAST" },
	{ IFF_NORTEXCH,		"NORTEXCH" },
	{ IFF_IPV4,		"IPV4" },
	{ IFF_IPV6,		"IPV6" },
	{ IFF_MIPRUNNING,	"MIP" },
};

static uchar_t	ip_six_byte_all_ones[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static ip_m_t	ip_m_tbl[] = {
	{ DL_ETHER, IFT_ETHER, IP_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_CSMACD, IFT_ISO88023, IP_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_TPB, IFT_ISO88024, IP_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_TPR, IFT_ISO88025, IP_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_FDDI, IFT_FDDI, IP_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_OTHER, IFT_OTHER, IP_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] }
};

ip_m_t	ipv6_m_tbl[] = {
	{ DL_ETHER, IFT_ETHER, IP6_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_CSMACD, IFT_ISO88023, IP6_DL_SAP, -2, 6,
		&ip_six_byte_all_ones[0] },
	{ DL_TPB, IFT_ISO88024, IP6_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_TPR, IFT_ISO88025, IP6_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_FDDI, IFT_FDDI, IP6_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] },
	{ DL_OTHER, IFT_OTHER, IP6_DL_SAP, -2, 6, &ip_six_byte_all_ones[0] }
};

static ill_t	ill_null;		/* Empty ILL for init. */
char	ipif_loopback_name[] = "lo0";
static char ill_loopback_ndd[] = "lo0:ip_forwarding";
static char ill_forward_name_suffix[] = ":ip_forwarding";
static kstat_t *loopback_ksp = NULL;
static	sin6_t	sin6_null;	/* Zero address for quick clears */
static	sin_t	sin_null;	/* Zero address for quick clears */

static uint_t	ill_index = 1;	/* Used to asssign interface indicies */
static boolean_t index_wrap = B_FALSE;	/* When set search for unused index */

/*
 * An _interface group_ (ifgrp) is a collection of IP addresses for a machine
 * that share the same subnet prefix.  Inside the Solaris kernel, the
 * abstraction for a machine's IP address is the ipif_t, so an ifgrp can be
 * viewed as a collection of ipif_t structures.  For example, if a machine has:
 *		le0:0	109.146.86.177/24
 *		qe0:0	109.103.10.1/24
 *		qe1:0	109.103.10.3/24
 *		qe2:0	109.103.11.1/24
 *		qe3:0	109.103.11.3/24
 *		qe3:1	109.103.10.5/24
 * then this machine has three ifgrps, each comprised of:
 *
 *		109.146.86.177/24
 *
 *		109.103.10.1/24
 *		109.103.10.3/24
 *		109.103.10.5/24
 *
 *		109.103.11.1/24
 *		109.103.11.3/24
 *
 * Each of the three groups above has an ifgrp_t associated with it.
 *
 * Each ipif is singly-linked in its ifgrp, and each has a pointer to the
 * next ipif to be scheduled in the ifgrp.  See ifgrp_*() functions for
 * what ifgrp primitives are.
 */

kmutex_t ifgrp_l_mutex;			/* Mutex for ifgrp head list. */
static  ifgrp_t *ifgrp_head;		/* Head of ifgrp list. */

/*
 * Allocate per-interface mibs. Only used for ipv6.
 * Returns true if ok. False otherwise.
 */
static boolean_t
ill_allocate_mibs(ill_t *ill)
{
	ASSERT(ill->ill_isv6);

	/* Already allocated? */
	if (ill->ill_ip6_mib != NULL) {
		ASSERT(ill->ill_icmp6_mib != NULL);
		return (B_TRUE);
	}

	ill->ill_ip6_mib = kmem_zalloc(sizeof (*ill->ill_ip6_mib),
	    KM_NOSLEEP);
	if (ill->ill_ip6_mib == NULL) {
		return (B_FALSE);
	}
	ill->ill_icmp6_mib = kmem_zalloc(sizeof (*ill->ill_icmp6_mib),
	    KM_NOSLEEP);
	if (ill->ill_icmp6_mib == NULL) {
		kmem_free(ill->ill_ip6_mib, sizeof (*ill->ill_ip6_mib));
		ill->ill_ip6_mib = NULL;
		return (B_FALSE);
	}
	ill->ill_ip6_mib->ipv6IfIndex = ill->ill_index;
	ill->ill_icmp6_mib->ipv6IfIcmpIfIndex = ill->ill_index;
	return (B_TRUE);
}

/*
 * Common code for preparation of ARP commands.  Two points to remember:
 * 	1) The ill_name is tacked on at the end of the allocated space so
 *	   the templates name_offset field must contain the total space
 *	   to allocate less the name length.
 *
 *	2) The templates name_length field should contain the *template*
 *	   length.  We use it as a parameter to bcopy() and then write
 *	   the real ill_name_length into the name_length field of the copy.
 * (Always called as writer.)
 */
mblk_t *
ill_arp_alloc(ill_t *ill, uchar_t *template, ipaddr_t addr)
{
	arc_t	*arc = (arc_t *)template;
	char	*cp;
	int	len;
	mblk_t	*mp;
	uint_t	name_length = ill->ill_name_length;
	uint_t	template_len = arc->arc_name_length;

	len = arc->arc_name_offset + name_length;
	mp = allocb(len, BPRI_HI);
	if (!mp)
		return (NULL);
	cp = (char *)mp->b_rptr;
	mp->b_wptr = (uchar_t *)&cp[len];
	if (template_len)
		bcopy(template, cp, template_len);
	if (len > template_len)
		bzero(&cp[template_len], len - template_len);
	mp->b_datap->db_type = M_PROTO;

	arc = (arc_t *)cp;
	arc->arc_name_length = name_length;
	cp = (char *)arc + arc->arc_name_offset;
	bcopy(ill->ill_name, cp, name_length);

	if (addr) {
		area_t	*area = (area_t *)mp->b_rptr;

		cp = (char *)area + area->area_proto_addr_offset;
		bcopy(&addr, cp, area->area_proto_addr_length);
		if (area->area_cmd == AR_ENTRY_ADD) {
			cp = (char *)area;
			len = area->area_proto_addr_length;
			if (area->area_proto_mask_offset)
				cp += area->area_proto_mask_offset;
			else
				cp += area->area_proto_addr_offset + len;
			while (len-- > 0)
				*cp++ = (char)~0;
		}
	}
	return (mp);
}

/*
 * Completely vaporize a lower level tap and all associated interfaces.
 * ill_delete is called only out of ip_close/ip_wsrv when the device control
 * stream is being closed.  The ill structure itself is freed when
 * ip_close calls mi_close_comm.  (Always called as writer.)
 */
void
ill_delete(ill_t *ill)
{
	ill_t	**illp;
	mblk_t	**mpp;

	/*
	 * ip_wsrv has already made sure no upper stream is flow
	 * controlled due to this interface stream being flow controlled.
	 * Note that ipif_down is handled
	 * automatically since the DL_UNBIND_REQ will cause the driver to
	 * send M_FLUSH(FLUSHRW) which will backenable ip_wsrv if canput()
	 * had failed on the driver stream.
	 */

	/*
	 * Nuke all interfaces.  ipif_free will take down the interface,
	 * remove it from the list, and free the data structure.
	 * Walk down the ipif list and remove the logical interfaces
	 * first before removing the main ipif. One can't unplumb the
	 * zeroth interface first because of arp and multicast dependencies
	 * of other ipifs on the zeroth ipif.
	 *
	 * If ill_ipif was not properly initialized (ie: low on memory), then
	 * no interfaces to clean up. In this case just clean up the ill.
	 */
	if (ill->ill_ipif != NULL) {
		while (ill->ill_ipif->ipif_next != NULL)
			ipif_free(ill->ill_ipif->ipif_next);

		/* Now free up the main ipif zeroth instance */
		ipif_free(ill->ill_ipif);
		ill->ill_ipif = NULL;
	}

	/* Clean up msgs on pending upcalls for mrouted */
	reset_mrt_ill(ill);
	/* Clean up ipc references */
	reset_ipc_ill(ill);

	/*
	 * ill_down will arrange to blow off any IRE's dependent on this
	 * ILL, and shut down fragmentation reassembly.
	 */
	ill_down(ill);
	/* Take us out of the list of ILLs. */
	for (illp = &ill_g_head; illp[0]; illp = &illp[0]->ill_next) {
		if (illp[0] == ill) {
			illp[0] = ill->ill_next;
			break;
		}
	}

	if (ill->ill_ndd_name)
		nd_unload(&ip_g_nd, ill->ill_ndd_name);

	if (ill->ill_frag_ptr != NULL) {
		uint_t count;

		for (count = 0; count < ILL_FRAG_HASH_TBL_COUNT; count++) {
			mutex_destroy(&ill->ill_frag_hash_tbl[count].ipfb_lock);
		}
		mi_free(ill->ill_frag_ptr);
		ill->ill_frag_ptr = NULL;
		ill->ill_frag_hash_tbl = NULL;
		ill->ill_name = NULL;
		ill->ill_name_length = 0;
	}
	/* Free all retained control messages. */
	mpp = &ill->ill_first_mp_to_free;
	do {
		while (mpp[0]) {
			mblk_t  *mp;
			mblk_t  *mp1;

			mp = mpp[0];
			mpp[0] = mp->b_next;
			for (mp1 = mp; mp1 != NULL; mp1 = mp1->b_cont) {
				mp1->b_next = NULL;
				mp1->b_prev = NULL;
			}
			freemsg(mp);
		}
	} while (mpp++ != &ill->ill_last_mp_to_free);

	if (ip_timer_ill == ill) {
		ill_t	*ill2;
		/* The IRE expiration timer is running on this ill. */
		for (ill2 = ill_g_head; ill2; ill2 = ill2->ill_next) {
			if (ill2 != ip_timer_ill && ill2->ill_rq != NULL)
				break;
		}
		if (ip_ire_expire_id != 0)
			(void) quntimeout(ill->ill_rq, ip_ire_expire_id);
		if (ill2) {
			ip_timer_ill = ill2;
			ip_ire_expire_id = qtimeout(ill2->ill_rq,
			    ip_trash_timer_expire, ill2->ill_rq,
			    MSEC_TO_TICK(ip_timer_interval));
		} else {
			ip_ire_expire_id = 0;
			ip_timer_ill = NULL;
		}
	}
	if (igmp_timer_ill == ill) {
		ill_t	*ill2;

		/* The IGMP timer is running on this ill. */
		for (ill2 = ill_g_head; ill2; ill2 = ill2->ill_next) {
			if (ill2 != igmp_timer_ill &&
			    (ill2->ill_rq != NULL) &&
			    (!(ill2->ill_isv6))) {
				break;
			}
		}
		if (igmp_slowtimeout_id != 0) {
			/* The igmp slowtimer is running on this ill_rq */
			(void) quntimeout(ill->ill_rq, igmp_slowtimeout_id);
		}
		if (ill2 != NULL) {
			/* Ask mi_timer to switch queues. */
			igmp_timer_ill = ill2;
			mi_timer(igmp_timer_ill->ill_rq, igmp_timer_mp, -2);

			if (igmp_slowtimeout_id != 0) {
				/* Ask qtimeout for slowtimo to start */
				igmp_slowtimeout_id =
				    qtimeout(ill2->ill_rq, igmp_slowtimo,
					ill2->ill_rq,
					MSEC_TO_TICK(IGMP_SLOWTIMO_INTERVAL));
			}
		} else {
			mi_timer_free(igmp_timer_mp);
			igmp_timer_mp = NULL;
			igmp_timer_ill = NULL;
			igmp_slowtimeout_id = 0;
		}
	}

	if (mld_timer_ill == ill) {
		ill_t   *ill2;

		/* The MLD timer is running on this ill. */
		for (ill2 = ill_g_head; ill2; ill2 = ill2->ill_next) {
			if (ill2 != mld_timer_ill &&
			    (ill2->ill_rq != NULL) &&
			    (ill2->ill_isv6)) {
				break;
			}
		}
		if (ill2 != NULL) {
			/*
			 * Since we are removing this ill, just
			 * assign this mld timer notification
			 * mesg blck to another IPV6 ill that is still
			 * up. Ask mi_timer to switch queues.
			 */
			mld_timer_ill = ill2;
			mi_timer(mld_timer_ill->ill_rq, mld_timer_mp, -2);
		} else {
			mi_timer_free(mld_timer_mp);
			mld_timer_mp = nilp(mblk_t);
			mld_timer_ill = nilp(ill_t);
		}
	}

	if (proxy_frag_ill == ill) {
		ill_t	*ill2;

		/* The proxy frags reassembly is running on this ill. */
		for (ill2 = ill_g_head; ill2; ill2 = ill2->ill_next) {
			if (ill2 != proxy_frag_ill &&
			    ill2->ill_frag_hash_tbl != NULL)
				break;
		}
		proxy_frag_ill = ill2;
	}
	mutex_enter(&ip_mi_lock);
	if (ill_ire_gc == ill) {
		ill_t *ill2;

		if (ip_ire_reclaim_id != 0) {
			(void) quntimeout(ill->ill_rq, ip_ire_reclaim_id);
		}

		/* The kmem_alloc GC callback is running on this ill */
		for (ill2 = ill_g_head; ill2; ill2 = ill2->ill_next) {
			if (ill2->ill_rq != NULL)
				break;
		}
		/*
		 * If this is the last one and  we still have no
		 * queue, just have no queue, if reclaim is called,
		 * it won't do anything.
		 */
		ill_ire_gc = ill2;
	}
	mutex_exit(&ip_mi_lock);
	if (ill->ill_ip6_mib != NULL) {
		kmem_free(ill->ill_ip6_mib, sizeof (*ill->ill_ip6_mib));
		ill->ill_ip6_mib = NULL;
	}
	if (ill->ill_icmp6_mib != NULL) {
		kmem_free(ill->ill_icmp6_mib, sizeof (*ill->ill_icmp6_mib));
		ill->ill_icmp6_mib = NULL;
	}

	/* That's it.  mi_close_comm will free the ill itself. */
}

/*
 * Concatenate together a physical address and a sap.
 *
 * Sap_lengths are interpreted as follows:
 *   sap_length == 0	==>	no sap
 *   sap_length > 0	==>	sap is at the head of the dlpi address
 *   sap_length < 0	==>	sap is at the tail of the dlpi address
 */
static void
ill_dlur_copy_address(uchar_t *phys_src, uint_t phys_length, t_scalar_t sap_src,
			t_scalar_t sap_length, uchar_t *dst)
{
	uint16_t sap_addr = (uint16_t)sap_src;

	if (sap_length == 0) {
		if (phys_src == NULL)
			bzero(dst, phys_length);
		else
			bcopy(phys_src, dst, phys_length);
	} else if (sap_length < 0) {
		if (phys_src == NULL)
			bzero(dst, phys_length);
		else
			bcopy(phys_src, dst, phys_length);
		bcopy(&sap_addr, (char *)dst + phys_length, sizeof (sap_addr));
	} else {
		bcopy(&sap_addr, dst, sizeof (sap_addr));
		if (phys_src == NULL)
			bzero((char *)dst + sap_length, phys_length);
		else
			bcopy(phys_src, (char *)dst + sap_length, phys_length);
	}
}

/*
 * Generate a dl_unitdata_req mblk for the device and address given.
 * addr_length is the length of the physical portion of the address.
 * If addr is NULL include an all zero address of the specified length.
 * TRUE? In any case, addr_length is taken to be the entire length of the
 * dlpi address, including the absolute value of sap_length.
 */
mblk_t *
ill_dlur_gen(uchar_t *addr, uint_t addr_length, t_uscalar_t sap,
		t_scalar_t sap_length)
{
	dl_unitdata_req_t *dlur;
	mblk_t	*mp;
	t_scalar_t	abs_sap_length;		/* absolute value */

	abs_sap_length = ABS(sap_length);
	mp = ip_dlpi_alloc(sizeof (*dlur) + addr_length + abs_sap_length,
		DL_UNITDATA_REQ);
	if (!mp)
		return (NULL);
	dlur = (dl_unitdata_req_t *)mp->b_rptr;
	/* HACK: accomodate incompatible DLPI drivers */
	if (addr_length == 8)
		addr_length = 6;
	dlur->dl_dest_addr_length = addr_length + abs_sap_length;
	dlur->dl_dest_addr_offset = sizeof (*dlur);
	dlur->dl_priority.dl_min = 0;
	dlur->dl_priority.dl_max = 0;
	ill_dlur_copy_address(addr, addr_length, sap, sap_length,
	    (uchar_t *)&dlur[1]);
	return (mp);
}

/*
 * ipc_walk function for cleaning up ipc_*_ill fields.
 */
static void
ipc_cleanup_ill(ipc, arg)
	ipc_t	*ipc;
	caddr_t arg;
{
	ill_t	*ill = (ill_t *)arg;

	if (ipc->ipc_multicast_ill == ill) {
		/* Revert to late binding */
		ipc->ipc_multicast_ill = NULL;
		ipc->ipc_multicast_ipif = NULL;
	}
	if (ipc->ipc_incoming_ill == ill)
		ipc->ipc_incoming_ill = NULL;
	if (ipc->ipc_outgoing_ill == ill)
		ipc->ipc_outgoing_ill = NULL;
}


/*
 * ill_down is called either out of ill_delete when the device control stream
 * is closing, or if an M_ERROR or M_HANGUP is passed up from the device.  We
 * shut down all associated interfaces, but do not tear down any plumbing or
 * ditch any information.  (Always called as writer.)
 */
void
ill_down(ill_t *ill)
{
	ipif_t	*ipif;

	/* Down the interfaces, without destroying them. */
	for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next)
		ipif_down(ipif);
	/* Blow off any IREs dependent on this ILL. */
	ire_walk(ill_downi, (char *)ill);
	/* Shut down fragmentation reassembly. */
	if (ill->ill_frag_timer_id != 0)
		(void) quntimeout(ill->ill_rq, ill->ill_frag_timer_id);
	ill->ill_frag_timer_id = 0;
	/* Ditch any incomplete packets. */
	(void) ill_frag_timeout(ill, 0);
	/* Remove any ipc_*_ill depending on this ill */
	ipc_walk(ipc_cleanup_ill, (caddr_t)ill);
}

/*
 * ire_walk routine used to delete every IRE that depends on queues
 * associated with 'ill'.  (Always called as writer.)
 */
static void
ill_downi(ire_t *ire, char *ill_arg)
{
	ill_t	*ill = (ill_t *)ill_arg;

	if (ire->ire_ipif != NULL && ire->ire_ipif->ipif_ill == ill)
		ire_delete(ire);
}

/* Consume an M_IOCACK of the fastpath probe. */
void
ill_fastpath_ack(ill_t *ill, mblk_t *mp)
{
	mblk_t	*mp1 = mp;

	/*
	 * If this was the first attempt turn on the fastpath probing.
	 */
	if (ill->ill_dlpi_fastpath_state == IDMS_INPROGRESS)
		ill->ill_dlpi_fastpath_state = IDMS_OK;

	/* Free the M_IOCACK mblk, hold on to the data */
	mp = mp->b_cont;
	freeb(mp1);
	if (mp == NULL)
		return;
	if (mp->b_cont != NULL) {
		/*
		 * Update all IRE's or NCE's that are not in
		 * fastpath mode and have an ire/nce_fp_mp that
		 * matches mp->b_cont that have
		 * an ire_wq matching this ill.
		 */
		if (ill->ill_isv6) {
			ndp_walk(ill, (pfi_t)ndp_fastpath_update,
			    (uchar_t *)mp);
		} else {
			ire_walk_ill_v4(ill, ire_fastpath_update,
			    (char *)mp);
		}
		mp1 = mp->b_cont;
		freeb(mp);
		mp = mp1;
	} else {
		ip0dbg(("ill_fastpath_ack:  no b_cont\n"));
	}

	freeb(mp);
}

/*
 * Throw an M_IOCTL message downstream asking "do you know fastpath?"
 * The data portion of the request is a dl_unitdata_req_t template for
 * what we would send downstream in the absence of a fastpath confirmation.
 */
void
ill_fastpath_probe(ill_t *ill, mblk_t *dlur_mp)
{
	struct iocblk	*ioc;
	mblk_t	*mp;

	if (dlur_mp == NULL)
		return;

	switch (ill->ill_dlpi_fastpath_state) {
	case IDMS_FAILED:
		/*
		 * Driver NAKed the first fastpath ioctl - assume it doesn't
		 * support it.
		 */
		return;
	case IDMS_UNKNOWN:
		/* This is the first probe */
		ill->ill_dlpi_fastpath_state = IDMS_INPROGRESS;
		break;
	default:
		break;
	}

	if ((mp = mkiocb(DL_IOC_HDR_INFO)) == NULL)
		return;

	mp->b_cont = copyb(dlur_mp);
	if (mp->b_cont == NULL) {
		freeb(mp);
		return;
	}

	ioc = (struct iocblk *)mp->b_rptr;
	ioc->ioc_count = msgdsize(mp->b_cont);

	putnext(ill->ill_wq, mp);
}

/*
 * This routine is called to scan the fragmentation reassembly table for the
 * This routine is called to scan the fragmentation reassembly table for the
 * specified ILL for any packets that are starting to smell.  dead_interval is
 * the maximum time in seconds that will be tolerated.  It will either be
 * the value specified in ip_g_frag_timeout, or zero if the ILL is shutting
 * down and it is time to blow everything off.
 * (Sometimes called as writer though not required by this function.)
 */
boolean_t
ill_frag_timeout(ill_t *ill, time_t dead_interval)
{
	ipfb_t	*ipfb;
	ipfb_t	*endp;
	ipf_t	*ipf;
	ipf_t	*ipfnext;
	mblk_t	*mp;
	time_t	current_time = hrestime.tv_sec;
	boolean_t	some_outstanding = B_FALSE;
	uint32_t	hdr_length;
	mblk_t	*send_icmp_head;
	mblk_t	*send_icmp_head_v6;

	ipfb = ill->ill_frag_hash_tbl;
	if (ipfb == NULL)
		return (B_FALSE);
	endp = &ipfb[ILL_FRAG_HASH_TBL_COUNT];
	/* Walk the frag hash table. */
	for (; ipfb < endp; ipfb++) {
		send_icmp_head = NULL;
		send_icmp_head_v6 = NULL;
		mutex_enter(&ipfb->ipfb_lock);
		while ((ipf = ipfb->ipfb_ipf) != 0) {
			if ((current_time - ipf->ipf_timestamp)
			    < dead_interval) {
				/* Note that there are more outstanding. */
				some_outstanding = B_TRUE;
				break;
			}
			/* Time's up.  Get it out of here. */
			hdr_length = ipf->ipf_nf_hdr_len;
			ipfnext = ipf->ipf_hash_next;
			if (ipfnext)
				ipfnext->ipf_ptphn = ipf->ipf_ptphn;
			*ipf->ipf_ptphn = ipfnext;
			mp = ipf->ipf_mp->b_cont;
			for (; mp; mp = mp->b_cont) {
				/* Extra points for neatness. */
				IP_REASS_SET_START(mp, 0);
				IP_REASS_SET_END(mp, 0);
			}
			mp = ipf->ipf_mp->b_cont;
			ill->ill_frag_count -= ipf->ipf_count;
			ASSERT(ipfb->ipfb_count >= ipfb->ipfb_count);
			ipfb->ipfb_count -= ipf->ipf_count;
			/*
			 * We do not send any icmp message from here because
			 * we currently are holding the ipfb_lock for this
			 * hash chain. If we try and send any icmp messages
			 * from here we may end up via a put back into ip
			 * trying to get the same lock, causing a recursive
			 * mutex panic. Instead we build a list and send all
			 * the icmp messages after we have dropped the lock.
			 */
			if (ill->ill_isv6) {
				BUMP_MIB(ill->ill_ip6_mib->ipv6ReasmFails);
				if (hdr_length != 0) {
					mp->b_next = send_icmp_head_v6;
					send_icmp_head_v6 = mp;
				} else {
					freemsg(mp);
				}
			} else {
				BUMP_MIB(ip_mib.ipReasmFails);
				mp->b_rptr -= ipf->ipf_stripped_hdr_len;
				mp->b_next = send_icmp_head;
				send_icmp_head = mp;
			}
			freeb(ipf->ipf_mp);
		}
		mutex_exit(&ipfb->ipfb_lock);
		/*
		 * Now need to send any icmp messages that we delayed from
		 * above.
		 */
		while (send_icmp_head_v6 != NULL) {
			mp = send_icmp_head_v6;
			send_icmp_head_v6 = send_icmp_head_v6->b_next;
			mp->b_next = NULL;
			icmp_time_exceeded_v6(ill->ill_wq, mp,
			    ICMP_REASSEMBLY_TIME_EXCEEDED, B_FALSE, B_FALSE);
		}
		while (send_icmp_head != NULL) {
			mp = send_icmp_head;
			send_icmp_head = send_icmp_head->b_next;
			mp->b_next = NULL;
			icmp_time_exceeded(ill->ill_wq, mp,
			    ICMP_REASSEMBLY_TIME_EXCEEDED);
		}
	}
	/*
	 * A non-dying ILL will use the return value to decide whether to
	 * restart the frag timer.
	 */
	return (some_outstanding);
}

/*
 * This routine is called when the approximate count of mblk memory used
 * for the specified ILL has exceeded max_count.
 * The fragmentation reassembly table is scaned for the oldest fragment
 * queue, its resources freed, while the ILL's count is to high.
 */
void
ill_frag_prune(ill_t *ill, uint_t max_count)
{
	ipfb_t	*ipfb;
	ipf_t	*ipf;
	ipf_t	**ipfp;
	mblk_t	*mp;
	mblk_t	*tmp;
	size_t	count;

	/*
	 * While the reassembly list for this ILL is to big prune a fragment
	 * queue by age, oldest first.  Note that the per ILL count is
	 * approximate, while the per frag hash bucket counts are accurate.
	 */
	while (ill->ill_frag_count > max_count) {
		int	ix;
		ipfb_t	*oipfb = NULL;
		uint_t	oldest = (uint_t)MAX_UINT;

		count = 0;
		for (ix = 0; ix < ILL_FRAG_HASH_TBL_COUNT; ix++) {
			ipfb = &ill->ill_frag_hash_tbl[ix];
			ipfp = &ipfb->ipfb_ipf;
			ipf = ipfp[0];
			if (ipf && ipf->ipf_gen < oldest) {
				oldest = ipf->ipf_gen;
				oipfb = ipfb;
			}
			count += ipfb->ipfb_count;
		}
		/* Refresh the per ILL count */
		ill->ill_frag_count = count;
		if (oipfb == NULL) {
			ill->ill_frag_count = 0;
			break;
		}
		if (count <= max_count)
			return;	/* Somebody beat us to it, nothing to do */
		mutex_enter(&oipfb->ipfb_lock);
		ipfp = &oipfb->ipfb_ipf;
		ipf = ipfp[0];
		if (ipf == NULL) {
			/* Somebody beat us to it, try again */
			mutex_exit(&oipfb->ipfb_lock);
			continue;
		}
		count = ipf->ipf_count;
		mp = ipf->ipf_mp;
		ipf = ipf->ipf_hash_next;
		if (ipf)
			ipf->ipf_ptphn = ipfp;
		ipfp[0] = ipf;
		for (tmp = mp; tmp; tmp = tmp->b_cont) {
			IP_REASS_SET_START(tmp, 0);
			IP_REASS_SET_END(tmp, 0);
		}
		ill->ill_frag_count -= count;
		ASSERT(oipfb->ipfb_count >= count);
		oipfb->ipfb_count -= count;
		mutex_exit(&oipfb->ipfb_lock);
		freemsg(mp);
	}
}

/*
 * For per-physical-interface forwarding configuration.
 */
/* ARGSUSED */
static int
ill_forward_get(queue_t *q, mblk_t *mp, void *cp)
{
	ill_t *ill = (ill_t *)cp;

	(void) mi_mpprintf(mp, "%d", ill->ill_forwarding);
	return (0);
}

/* ARGSUSED */
int
ill_forward_set(queue_t *q, mblk_t *mp, char *value, void *cp)
{
	char *end;
	int new_value;
	ill_t *ill = (ill_t *)cp;

	new_value = (int)mi_strtol(value, &end, 10);
	if (end == value || new_value < 0 || new_value > 1)
		return (EINVAL);

	if (new_value == ill->ill_forwarding)
		return (0);

	ill->ill_forwarding = new_value;

#ifdef notyet
	if (ill->ill_forwarding) {
		/* If we cache this in ires, clean them up. */
	} else {
		/* If we cache this in ires, clean them up. */
	}
#endif

	return (0);
}

/*
 * Given an ill with a _valid_ name, add the ip_forwarding ndd variable
 * for this ill.
 */
static int
ill_set_ndd_name(ill_t *ill)
{
	/* XXX For now, bail on an IPv6 one. */
	if (ill->ill_isv6)
		return (0);

	ill->ill_ndd_name = ill->ill_name + ill->ill_name_length;
	bcopy(ill->ill_name, ill->ill_ndd_name, ill->ill_name_length - 1);
	bcopy(ill_forward_name_suffix,
	    ill->ill_ndd_name + ill->ill_name_length - 1,
	    sizeof (ill_forward_name_suffix));
	ill->ill_forwarding = ip_g_forward;

	if (!nd_load(&ip_g_nd, ill->ill_ndd_name, ill_forward_get,
	    ill_forward_set, (caddr_t)ill)) {
		/*
		 * If the nd_load failed, it only meant that it could not
		 * allocate a new bunch of room for further NDD expansion.
		 * Because of that, the ill_ndd_name will be set to 0, and
		 * this interface is at the mercy of the global ip_forwarding
		 * variable.
		 */
		ill->ill_ndd_name = NULL;
		return (ENOMEM);
	}
	return (0);
}

/*
 * ill_init is called by ip_open when a device control stream is opened.
 * It does a few initializations, and shoots a DL_INFO_REQ message down
 * to the driver.  The response is later picked up in ip_rput_dlpi and
 * used to set up default mechanisms for talking to the driver.  (Always
 * called as writer.)
 */
int
ill_init(queue_t *q, ill_t *ill)
{
	int	count, errno;
	dl_info_req_t	*dlir;
	mblk_t	*info_mp;
	uchar_t *frag_ptr;
	char	*cp;

	/* Start clean. */
	*ill = ill_null;
	ill->ill_rq = q;
	q = WR(q);
	ill->ill_wq = q;
	/*
	 * Don't look!  We walk downstream to pick up the name of the
	 * driver so we can construct the ILL name.
	 */
	do {
		q = q->q_next;
	} while (q->q_next);
	cp = q->q_qinfo->qi_minfo->mi_idname;
	if (cp == NULL || !*cp)
		return (ENXIO);

	info_mp = allocb(MAX(sizeof (dl_info_req_t), sizeof (dl_info_ack_t)),
	    BPRI_HI);
	if (info_mp == NULL)
		return (ENOMEM);

	/*
	 * Allocate sufficient space to contain our fragment hash table and
	 * the device name.
	 *
	 * NOTE: The allocation of MAX(mi_strlen(), LIFNAMSIZ) guarantees we
	 *	 don't need to regrow the space later.
	 */
	frag_ptr = mi_alloc(ILL_FRAG_HASH_TBL_SIZE +
	    2 * MAX(mi_strlen(cp) + 4, LIFNAMSIZ) + 4 +
	    strlen(ill_forward_name_suffix), BPRI_MED);
	if (frag_ptr == NULL) {
		freemsg(info_mp);
		return (ENOMEM);
	}
	ill->ill_frag_ptr = frag_ptr;
	ill->ill_frag_hash_tbl = (ipfb_t *)frag_ptr;
	ill->ill_name = (char *)(frag_ptr + ILL_FRAG_HASH_TBL_SIZE);
	for (count = 0; count < ILL_FRAG_HASH_TBL_COUNT; count++) {
		ill->ill_frag_hash_tbl[count].ipfb_ipf = NULL;
		mutex_init(&ill->ill_frag_hash_tbl[count].ipfb_lock,
		    NULL, MUTEX_DEFAULT, NULL);
		ill->ill_frag_hash_tbl[count].ipfb_count = 0;
	}

	/*
	 * Construct a name from our devices 'q_qinfo->qi_minfo->mi_idname'
	 * and the lowest 'unit number' that is unused.  We assume here that
	 * devices are opened in ascending order.
	 */
	count = -1;
	do {
		if (++count >= 1000) {
			mi_free(frag_ptr);
			freemsg(info_mp);
			return (ENXIO);
		}
		(void) sprintf(ill->ill_name, "%s%d", cp, count);
		ill->ill_name_length = (uint_t)(mi_strlen(ill->ill_name) + 1);
	} while (ill_lookup_on_name(ill->ill_name, ill->ill_name_length,
	    B_FALSE, B_FALSE));
	/* Remember the unit number for subsequent DLPI attach requests. */
	ill->ill_ppa = count;

	/* Now that I can't return with an error, set of the ill_ndd_name. */
	errno = ill_set_ndd_name(ill);
	if (errno != 0) {
		/*
		 * XXX Oooohh boy.  For now, assume the rest of the nd table
		 * isn't trashed.
		 */
		mi_free(frag_ptr);
		freemsg(info_mp);
		return (errno);
	}

	/* Chain us in at the end of the ill list. */
	if (ill_g_head != NULL) {
		ill_t	*till;

		till = ill_g_head;
		while (till->ill_next != NULL)
			till = till->ill_next;
		till->ill_next = ill;
	} else {
		ill_g_head = ill;
	}

	/* Send down the Info Request to the driver. */
	info_mp->b_datap->db_type = M_PROTO;
	dlir = (dl_info_req_t *)info_mp->b_rptr;
	info_mp->b_wptr = (uchar_t *)&dlir[1];
	dlir->dl_primitive = DL_INFO_REQ;
	ill_dlpi_send(ill, info_mp);

	/* If there is no IRE expiration timer running, get one started. */
	if (ip_ire_expire_id == 0) {
		ip_ire_expire_id = qtimeout(ill->ill_rq, ip_trash_timer_expire,
		    ill->ill_rq, MSEC_TO_TICK(ip_timer_interval));
		if (ip_ire_expire_id != 0) {
			ip_timer_ill = ill;
		}
	}

	/* For reassembling fragments that get picked up by proxy listeners */
	if (proxy_frag_ill == NULL)
		proxy_frag_ill = ill;

	/* If there is no IRE GC running get one registered */
	if (ill_ire_gc == NULL && ill->ill_rq != NULL)
		ill_ire_gc = ill;

	/* Frag queue limit stuff */
	ill->ill_frag_count = 0;
	ill->ill_ipf_gen = 0;

	ill->ill_multicast_type = IGMP_V2_ROUTER;
	ill->ill_multicast_time = 0;

	/*
	 * Initialize IPv6 configuration variables.  The IP module is always
	 * opened as an IPv4 module.  Instead tracking down the cases where
	 * it switches to do ipv6, we'll just initialize the IPv6 configuration
	 * here for convenience, this has no effect until the ill is set to do
	 * IPv6.
	 */
	ill->ill_reachable_time = ND_REACHABLE_TIME;
	ill->ill_reachable_retrans_time = ND_RETRANS_TIMER;
	ill->ill_xmit_count = ND_MAX_MULTICAST_SOLICIT;
	ill->ill_max_buf = ND_MAX_Q;
	return (0);
}

/*
 * ill_dls_info
 * creates datalink socket info from the device.
 */
int
ill_dls_info(struct sockaddr_dl *sdl, ipif_t *ipif)
{
	size_t	length;
	ill_t	*ill = ipif->ipif_ill;

	sdl->sdl_family = AF_LINK;
	sdl->sdl_index = ipif->ipif_index;
	sdl->sdl_type = ipif->ipif_type;
	(void) ipif_get_name(ipif, sdl->sdl_data, sizeof (sdl->sdl_data));
	length = mi_strlen(sdl->sdl_data);
	ASSERT(length < 256);
	sdl->sdl_nlen = (uchar_t)length;
	sdl->sdl_alen = ill->ill_phys_addr_length;
	if (ill->ill_phys_addr_length != 0 && ill->ill_hw_addr != NULL) {
		bcopy(ill->ill_hw_addr, &sdl->sdl_data[length],
		    ill->ill_phys_addr_length);
	}
	sdl->sdl_slen = 0;
	return (sizeof (struct sockaddr_dl));
}

static int
loopback_kstat_update(kstat_t *ksp, int rw)
{
	kstat_named_t *kn = KSTAT_NAMED_PTR(ksp);

	if (rw == KSTAT_WRITE)
		return (EACCES);
	kn[0].value.ui32 = loopback_packets;
	kn[1].value.ui32 = loopback_packets;
	return (0);
}

/*
 * Assign a unique interface index for the ill.
 * When both IPv4 and IPv6 are used for an interface there are two different
 * ill. This routine ensures that they both have the same interface index.
 */
static void
ill_assign_index(ill_t *ill)
{
	ill_t *till;

	/*
	 * First look to see if there is an existing ill representation
	 * for this physical interface. Remember we create 2 separate
	 * ills for v4 and v6 correspondingly per physical interface.
	 */
	for (till = ill_g_head; till; till = till->ill_next) {
		if (till->ill_isv6 != ill->ill_isv6 &&
		    till->ill_name_length == ill->ill_name_length &&
		    bcmp(till->ill_name, ill->ill_name,
			ill->ill_name_length) == 0) {
			/*
			 * Found an existing one. Assign the same
			 * index to this second new
			 * representation of this
			 * physical interface.
			*/
			ill->ill_index = till->ill_index;
			return;
		}
	}
	/*
	 * None found. So this is the first representation
	 * of this physical interface. Assign a unique
	 * index. Indexes dont have to be contiguous sequence.
	 * Gaps that occur due to unplumb/plumb instances
	 * are acceptable. We dont check for unique
	 * index until the index number reaches MAX_UINT, and
	 * wrap around. Then we start looking for
	 * index number being not in use.
	 */
	if (!index_wrap) {
		ill->ill_index = ill_index++;
		if (ill_index == 0) {
			/* Reached the uint_t limit Next time wrap  */
			index_wrap = B_TRUE;
		}
		return;
	}
	/* start reusing unused indexes */
	for (ill_index = 1; ill_index <= MAX_UINT; ill_index++) {
		boolean_t index_used;

		index_used = B_FALSE;
		/*
		 * Walk ill chain to see if this index
		 * is already in use
		 */
		for (till = ill_g_head; till != NULL; till = till->ill_next) {
			if (ill_index == till->ill_index) {
				/* this one is used */
				index_used = B_TRUE;
				break;
			}
		}
		if (!index_used) {
			/* found unused index - use it */
			ill->ill_index = ill_index;
			return;
		}
	}
	cmn_err(CE_PANIC, "ip: all interfaces indicies are used!\n");
}

/*
 * Return a pointer to the ill which matches the supplied name.  Note that
 * the ill name length includes the null termination character.  (May be
 * called as writer.)
 * If do_alloc and the interface is "lo0" it will be automatically created.
 */
ill_t *
ill_lookup_on_name(char *name, size_t namelen, boolean_t do_alloc,
    boolean_t isv6)
{
	ill_t	*ill;
	ipif_t	*ipif;
	kstat_named_t	*kn;

	if (namelen == 0)
		return (NULL);
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		if (ill->ill_name_length == namelen &&
		    ill->ill_isv6 == isv6) {
			size_t	i1 = namelen;

			do {
				if (i1-- == 0)
					return (ill);
			} while (ill->ill_name[i1] == name[i1]);
		}
	}
	/*
	 * Couldn't find it.  Does this happen to be a lookup for the
	 * loopback device?
	 */
	if (namelen != sizeof (ipif_loopback_name) ||
	    mi_strcmp(name, ipif_loopback_name) != 0) {
		/* Nope, some bogon. */
		return (NULL);
	}
	if (!do_alloc)
		return (NULL);

	/* Create the loopback device on demand */
	ill = (ill_t *)(mi_alloc(sizeof (ill_t) +
	    sizeof (ipif_loopback_name), BPRI_MED));
	if (ill == NULL)
		return (NULL);

	*ill = ill_null;
	ill->ill_max_frag = IP_LOOPBACK_MTU;
	/* Add room for tcp+ip headers */
	if (isv6)
		ill->ill_max_frag += IPV6_HDR_LEN + 20;	/* for TCP */
	else
		ill->ill_max_frag += IP_SIMPLE_HDR_LENGTH + 20;
	ill->ill_max_mtu = ill->ill_max_frag;
	ill->ill_name = ipif_loopback_name;
	ill->ill_name_length = sizeof (ipif_loopback_name);

	/* Forwarding NDD stuff. */
	ill->ill_ndd_name = ill_loopback_ndd;
	if (!nd_load(&ip_g_nd, ill->ill_ndd_name, ill_forward_get,
	    ill_forward_set, (caddr_t)ill)) {
		cmn_err(CE_WARN, "Couldn't load NDD for lo0 device.\n");
		return (NULL);
	}

	/* No resolver here. */
	ill->ill_net_type = IRE_LOOPBACK;
	if (isv6) {
		ill->ill_isv6 = B_TRUE;
		if (!ill_allocate_mibs(ill)) {
			mi_free(ill);
			return (NULL);
		}
	}
	if (ill->ill_index == 0)
		ill_assign_index(ill);

	ipif = ipif_allocate(ill, 0L, IRE_LOOPBACK);
	if (ipif == NULL) {
		mi_free((char *)ill);
		return (NULL);
	}

	ipif->ipif_flags = IFF_RUNNING | IFF_LOOPBACK | IFF_MULTICAST;

	/* Set up default loopback address and mask. */
	if (!isv6) {
		ipaddr_t inaddr_loopback = htonl(INADDR_LOOPBACK);

		IN6_IPADDR_TO_V4MAPPED(inaddr_loopback, &ipif->ipif_v6lcl_addr);
		ipif->ipif_v6src_addr = ipif->ipif_v6lcl_addr;
		V4MASK_TO_V6(htonl(IN_CLASSA_NET), ipif->ipif_v6net_mask);
		V6_MASK_COPY(ipif->ipif_v6lcl_addr, ipif->ipif_v6net_mask,
		    ipif->ipif_v6subnet);
		ipif->ipif_flags |= IFF_IPV4;
	} else {
		ipif->ipif_v6lcl_addr = ipv6_loopback;
		ipif->ipif_v6src_addr = ipif->ipif_v6lcl_addr;
		ipif->ipif_v6net_mask = ipv6_all_ones;
		V6_MASK_COPY(ipif->ipif_v6lcl_addr, ipif->ipif_v6net_mask,
		    ipif->ipif_v6subnet);
		ipif->ipif_flags |= IFF_IPV6;
	}

	/* Chain us in at the end of the ill list. */
	ill->ill_next = NULL;
	if (ill_g_head) {
		ill_t	*till;

		till = ill_g_head;
		while (till->ill_next != NULL)
			till = till->ill_next;
		till->ill_next = ill;
	} else {
		ill_g_head = ill;
	}

	if (loopback_ksp == NULL) {
		/* Export loopback interface statistics */
		loopback_ksp = kstat_create("lo", 0, ipif_loopback_name, "net",
		    KSTAT_TYPE_NAMED, 2, 0);
		if (loopback_ksp == NULL)
			return (ill);
		loopback_ksp->ks_update = loopback_kstat_update;
		kn = KSTAT_NAMED_PTR(loopback_ksp);
		kstat_named_init(&kn[0], "ipackets", KSTAT_DATA_UINT32);
		kstat_named_init(&kn[1], "opackets", KSTAT_DATA_UINT32);
		kstat_install(loopback_ksp);
	}
	return (ill);
}

/*
 * Named Dispatch routine to produce a formatted report on all ILLs.
 * This report is accessed by using the ndd utility to "get" ND variable
 * "ip_ill_status".
 */
/* ARGSUSED */
int
ip_ill_report(queue_t *q, mblk_t *mp, void *arg)
{
	ill_t	*ill;

	(void) mi_mpprintf(mp,
	    "ILL      " MI_COL_HDRPAD_STR
	/*   01234567[89ABCDEF] */
	    "rq       " MI_COL_HDRPAD_STR
	/*   01234567[89ABCDEF] */
	    "wq       " MI_COL_HDRPAD_STR
	/*   01234567[89ABCDEF] */
	    "upcnt mxfrg err name");
	/*   12345 12345 123 xxxxxxxx  */
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		(void) mi_mpprintf(mp,
		    MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR MI_COL_PTRFMT_STR
		    "%05u %05u %03d %s",
		    (void *)ill, (void *)ill->ill_rq, (void *)ill->ill_wq,
		    ill->ill_ipif_up_count,
		    ill->ill_max_frag, ill->ill_error, ill->ill_name);
	}
	return (0);
}

/*
 * Named Dispatch routine to produce a formatted report on all IPIFs.
 * This report is accessed by using the ndd utility to "get" ND variable
 * "ip_ipif_status".
 */
/* ARGSUSED */
int
ip_ipif_report(queue_t *q, mblk_t *mp, void *arg)
{
	char	buf1[INET6_ADDRSTRLEN];
	char	buf2[INET6_ADDRSTRLEN];
	char	buf3[INET6_ADDRSTRLEN];
	char	buf4[INET6_ADDRSTRLEN];
	char	buf5[INET6_ADDRSTRLEN];
	char	buf6[INET6_ADDRSTRLEN];
	char	buf[LIFNAMSIZ];
	ill_t	*ill;
	ipif_t	*ipif;
	nv_t	*nvp;
	uint_t	flags;
	ippc_t ippc1;

	(void) mi_mpprintf(mp,
	    "IPIF     " MI_COL_HDRPAD_STR
	/*   01234567[89ABCDEF] */
	    "addr            mask            broadcast       "
	/*   123.123.123.123 123.123.123.123 123.123.123.123 */
	    "p-p-dst         metr mtu   in/out/forward name");
	/*   123.123.123.123 0123 12345 in/out/forward sle0/1 */
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			ippc1.ippc_v6addr = ipif->ipif_v6lcl_addr;
			ippc1.ippc_ib_pkt_count = ipif->ipif_ib_pkt_count;
			ippc1.ippc_ob_pkt_count = ipif->ipif_ob_pkt_count;
			ippc1.ippc_fo_pkt_count = ipif->ipif_fo_pkt_count;
			if (ill->ill_isv6)
				ire_walk_v6(ire_pkt_count_v6, (char *)&ippc1);
			else
				ire_walk_v4(ire_pkt_count, (char *)&ippc1);
			(void) mi_mpprintf(mp,
			    MI_COL_PTRFMT_STR
			    "%04u %05u %u/%u/%u %s",
			    (void *)ipif,
			    ipif->ipif_metric, ipif->ipif_mtu,
			    ippc1.ippc_ib_pkt_count,
			    ippc1.ippc_ob_pkt_count,
			    ippc1.ippc_fo_pkt_count,
			    ipif_get_name(ipif, buf, sizeof (buf)));
			flags = ipif->ipif_flags;
			/* Tack on text strings for any flags. */
			nvp = ipif_nv_tbl;
			for (; nvp < A_END(ipif_nv_tbl); nvp++) {
				if (nvp->nv_value & flags)
					(void) mi_mpprintf_nr(mp, " %s",
					    nvp->nv_name);
			}
			(void) mi_mpprintf(mp,
			    "\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
			    inet_ntop(AF_INET6,
				&ipif->ipif_v6lcl_addr, buf1, sizeof (buf1)),
			    inet_ntop(AF_INET6,
				&ipif->ipif_v6src_addr, buf2, sizeof (buf2)),
			    inet_ntop(AF_INET6,
				&ipif->ipif_v6subnet, buf3, sizeof (buf3)),
			    inet_ntop(AF_INET6,
				&ipif->ipif_v6net_mask, buf4, sizeof (buf4)),
			    inet_ntop(AF_INET6,
				&ipif->ipif_v6brd_addr, buf5, sizeof (buf5)),
			    inet_ntop(AF_INET6,
				&ipif->ipif_v6pp_dst_addr,
				buf6, sizeof (buf6)));
		}
	}
	return (0);
}

/*
 * ip_ll_subnet_defaults is called when we get the DL_INFO_ACK back from the
 * driver.  We construct best guess defaults for lower level information that
 * we need.  If an interface is brought up without injection of any overriding
 * information from outside, we have to be ready to go with these defaults.
 * When we get the first DL_INFO_ACK (from ip_open() sending a DL_INFO_REQ)
 * we primarely want the dl_provider_style.
 * The subsequent DL_INFO_ACK is received after doing a DL_ATTACH and DL_BIND
 * at which point we assume the other part of the information is valid.
 */
void
ip_ll_subnet_defaults(ill_t *ill, mblk_t *mp)
{
	uchar_t		*brdcst_addr;
	uint_t		brdcst_addr_length, phys_addr_length;
	t_scalar_t	sap_length;
	dl_info_ack_t	*dlia;
	ip_m_t		*ipm;
	boolean_t	exists;

	dlia = (dl_info_ack_t *)mp->b_rptr;
	ill->ill_mactype = dlia->dl_mac_type;

	ipm = ip_m_lookup(ill, dlia->dl_mac_type);
	if (ipm == NULL) {
		ipm = ip_m_lookup(ill, DL_OTHER);
		ASSERT(ipm != NULL);
	}
	if (!ill->ill_dlpi_style_set) {
		if (dlia->dl_provider_style == DL_STYLE2)
			ill->ill_needs_attach = 1;

		/*
		 * When the new DLPI stuff is ready we'll pull lengths
		 * from dlia
		 */
		if (dlia->dl_version == DL_VERSION_2) {
			brdcst_addr_length = dlia->dl_brdcst_addr_length;
			brdcst_addr = mi_offset_param(mp,
			    dlia->dl_brdcst_addr_offset,
			    brdcst_addr_length);
			if (brdcst_addr == NULL) {
				brdcst_addr_length = 0;
			}
			sap_length = dlia->dl_sap_length;
			phys_addr_length = dlia->dl_addr_length -
			    ABS(sap_length);
			ip1dbg(("ip: bcast_len %d, sap_len %d, phys_len %d\n",
			    brdcst_addr_length, sap_length, phys_addr_length));
		} else {
			brdcst_addr_length = ipm->ip_m_brdcst_addr_length;
			brdcst_addr = ipm->ip_m_brdcst_addr;
			sap_length = ipm->ip_m_sap_length;
			phys_addr_length = brdcst_addr_length;
		}

		/*
		 * We don't know if we are IPv4 or IPv6 yet hence can't
		 * use ip_m_sap.
		 */
		ill->ill_type = ipm->ip_m_type;

		ill->ill_bcast_addr_length = brdcst_addr_length;
		ill->ill_phys_addr_length = phys_addr_length;
		ill->ill_sap_length = sap_length;

		ill->ill_max_frag = dlia->dl_max_sdu;
		ill->ill_max_mtu = ill->ill_max_frag;
		if (ill->ill_max_frag > ip_max_mtu)
			ip_max_mtu = ill->ill_max_frag;

		/*
		 * Allocate the first ipif on this ill - we do not want to
		 * delay its creation until the first ioctl references it since
		 * we want to report it in SIOCGIFCONF ioctls.
		 * For some reason we get 2 DL_INFO_ACK messages here thus
		 * we need to check for duplicates by calling
		 * ipif_lookup_on_name instead of just ipif_allocate.
		 */
		(void) ipif_lookup_on_name(ill->ill_name, ill->ill_name_length,
		    B_TRUE, &exists, ill->ill_isv6);

		ASSERT(ill->ill_dlpi_style_set == 0);
		ill->ill_dlpi_style_set = 1;
		freemsg(mp);
		return;
	}
	ill->ill_sap = ipm->ip_m_sap;
	ill->ill_type = ipm->ip_m_type;
	if (dlia->dl_version == DL_VERSION_2) {
		brdcst_addr = mi_offset_param(mp, dlia->dl_brdcst_addr_offset,
		    ill->ill_bcast_addr_length);
	} else {
		brdcst_addr = ipm->ip_m_brdcst_addr;
	}
	if (ill->ill_bcast_addr_length == 0) {
		ASSERT(ill->ill_resolver_mp == NULL);
		ill->ill_net_type = IRE_IF_NORESOLVER;
		ill->ill_resolver_mp = ill_dlur_gen(NULL,
		    ill->ill_phys_addr_length,
		    ill->ill_sap,
		    ill->ill_sap_length);
		/* Use this one for a fastpath probe later */
		if (ill->ill_bcast_mp == NULL)
			ill->ill_bcast_mp = copymsg(ill->ill_resolver_mp);
	} else {
		ill->ill_net_type = IRE_IF_RESOLVER;
		if (ill->ill_bcast_mp == NULL)
			ill->ill_bcast_mp = ill_dlur_gen(brdcst_addr,
			    ill->ill_bcast_addr_length,
			    ill->ill_sap,
			    ill->ill_sap_length);
	}
	ill->ill_max_frag = dlia->dl_max_sdu;
	ill->ill_max_mtu = ill->ill_max_frag;
	if (ill->ill_max_frag > ip_max_mtu)
		ip_max_mtu = ill->ill_max_frag;

	/* Clear any previous error indication. */
	ill->ill_error = 0;

	if (ill->ill_index == 0)
		ill_assign_index(ill);

	freemsg(mp);
}

/*
 * Perform various checks to verify that an address would make sense as a local
 * interface address.  This is currently only called when an attempt is made
 * to set a local address.
 */
static boolean_t
ip_local_addr_ok(ipaddr_t addr, ipaddr_t subnet_mask)
{
	ipaddr_t	net_mask;

	/*
	 * Don't allow all zeroes or ones as host part, but allow
	 * all ones netmask.
	 */

	net_mask = ip_net_mask(addr);
	if (net_mask == 0 || addr == (ipaddr_t)0 || addr == ~(ipaddr_t)0 ||
	    addr == (addr & net_mask) || addr == (addr | ~net_mask) ||
	    ((addr == (addr & subnet_mask)) &&
	    (subnet_mask != ~(ipaddr_t)0)) ||
	    ((addr == (addr | ~subnet_mask)) &&
	    (subnet_mask != ~(ipaddr_t)0)))
		return (B_FALSE);
	if (CLASSD(addr))
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * Perform various checks to verify that an address would make sense as a
 * remote or subnet interface address.
 */
static boolean_t
ip_remote_addr_ok(ipaddr_t addr, ipaddr_t subnet_mask)
{
	ipaddr_t	net_mask;

	/*
	 * Don't allow all zeroes or ones as host part, but allow
	 * all ones netmask.
	 */
	net_mask = ip_net_mask(addr);
	if (net_mask == 0 || addr == (ipaddr_t)0 || addr == ~(ipaddr_t)0 ||
	    addr == (addr & net_mask) || addr == (addr | ~net_mask) ||
	    ((addr == (addr & subnet_mask)) &&
	    (subnet_mask != ~(ipaddr_t)0)) ||
	    ((addr == (addr | ~subnet_mask)) &&
	    (subnet_mask != ~(ipaddr_t)0)))
		return (B_FALSE);
	if (CLASSD(addr))
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * ipif_lookup_group
 */
ipif_t *
ipif_lookup_group(ipaddr_t group)
{
	ire_t	*ire;
	ipif_t	*ipif;

	ire = ire_lookup_loop_multi(group);
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
ipif_lookup_interface(ipaddr_t if_addr, ipaddr_t dst)
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
		if (ill->ill_isv6)
			continue;
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			/* Allow the ipif to be down */
			if ((ipif->ipif_flags & IFF_POINTOPOINT) &&
			    (ipif->ipif_lcl_addr == if_addr) &&
			    (ipif->ipif_pp_dst_addr == dst)) {
				return (ipif);
			}
		}
	}
	/* lookup the ipif based on interface address */
	ipif = ipif_lookup_addr(if_addr, NULL);
	ASSERT(ipif == NULL || !ipif->ipif_isv6);
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
ipif_lookup_addr(ipaddr_t addr, ill_t *match_ill)
{
	ipif_t	*ipif;
	ipif_t	*fallback;
	ill_t	*ill;

	fallback = NULL;
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		if (ill->ill_isv6)
			continue;
		if (match_ill != NULL && ill != match_ill)
			continue;
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			/* Allow the ipif to be down */
			if (ipif->ipif_lcl_addr == addr &&
			    (ipif->ipif_flags & IFF_UNNUMBERED) == 0) {
				return (ipif);
			}
			if (ipif->ipif_flags & IFF_POINTOPOINT &&
			    ipif->ipif_pp_dst_addr == addr) {
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
 * IRE lookup and pick the first ipif corresponding to the source address in the
 * ire.
 */
ipif_t *
ipif_lookup_remote(ill_t *ill, ipaddr_t addr)
{
	ipif_t	*ipif;
	ire_t	*ire;

	ASSERT(!ill->ill_isv6);

	for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
		/* Allow the ipif to be down */
		if (ipif->ipif_flags & IFF_POINTOPOINT) {
			if (ipif->ipif_pp_dst_addr == addr)
				return (ipif);
			if (!(ipif->ipif_flags & IFF_UNNUMBERED) &&
			    ipif->ipif_lcl_addr == addr)
				return (ipif);
		} else if (ipif->ipif_subnet == (addr & ipif->ipif_net_mask)) {
			return (ipif);
		}
	}
	ire = ire_route_lookup(addr, 0, 0, 0, NULL, NULL, NULL,
	    MATCH_IRE_RECURSIVE);
	if (ire != NULL) {
		ipif = ire->ire_ipif;
		ire_refrele(ire);
		if (ipif != NULL) {
			return (ipif);
		}
	}
	/* Pick the first interface */
	return (ill->ill_ipif);
}

/*
 * TODO: make this table extendible at run time
 * Return a pointer to the mac type info for 'mac_type'
 */
/* ARGSUSED */
static ip_m_t *
ip_m_lookup(ill_t *ill, t_uscalar_t mac_type)
{
	ip_m_t	*ipm;

	if (ill->ill_isv6) {
		for (ipm = ipv6_m_tbl; ipm < A_END(ipv6_m_tbl); ipm++) {
			if (ipm->ip_m_mac_type == mac_type)
				return (ipm);
		}
	} else {
		for (ipm = ip_m_tbl; ipm < A_END(ip_m_tbl); ipm++) {
			if (ipm->ip_m_mac_type == mac_type)
				return (ipm);
		}
	}
	return (NULL);
}

/*
 * ip_rt_add is called to add an IPv4 route to the forwarding table.
 * ipif_arg is passed in to associate it with the correct interface.
 */
int
ip_rt_add(ipaddr_t dst_addr, ipaddr_t mask, ipaddr_t gw_addr, int flags,
    ipif_t *ipif_arg, ire_t **ire_arg, boolean_t ioctl_msg)
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
	if (gw_addr == 0)
		return (ENETUNREACH);

	/*
	 * If this is the case of RTF_HOST being set, then we set the netmask
	 * to all ones (regardless if one was supplied).
	 */
	if (flags & RTF_HOST)
		mask = IP_HOST_MASK;

	/*
	 * Get the ipif, if any, corresponding to the gw_addr
	 */
	ipif = ipif_lookup_interface(gw_addr, dst_addr);

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
		 *
		 * Since a netmask isn't passed in via the ioctl interface
		 * (SIOCADDRT), we don't check for a matching netmask in that
		 * case.
		 */
		if (!ioctl_msg)
			match_flags |= MATCH_IRE_MASK;
		ire = ire_ftable_lookup(dst_addr, mask, 0, IRE_INTERFACE, ipif,
		    NULL, NULL, 0, match_flags);
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
		ire = ire_create(
		    (uchar_t *)&dst_addr,
		    (uint8_t *)&mask,
		    (uint8_t *)&ipif->ipif_src_addr,
		    NULL,
		    ipif->ipif_mtu,
		    NULL,
		    NULL,
		    stq,
		    ipif->ipif_net_type,
		    ipif->ipif_resolver_mp,
		    ipif,
		    0,
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
		 * routine, but rather using ire_create() directly.
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
	gw_ire = ire_ftable_lookup(gw_addr, 0, 0, IRE_INTERFACE, ipif_arg, NULL,
	    NULL, 0, match_flags);
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
	if (mask == IP_HOST_MASK)
		type = IRE_HOST;
	else if (mask == 0)
		type = IRE_DEFAULT;
	else
		type = IRE_PREFIX;

	/* check for a duplicate entry */
	ire = ire_ftable_lookup(dst_addr, mask, gw_addr, type, ipif_arg, NULL,
	    NULL, 0, match_flags | MATCH_IRE_MASK | MATCH_IRE_GW);
	if (ire != NULL) {
		ire_refrele(gw_ire);
		ire_refrele(ire);
		return (EEXIST);
	}

	/* Create the IRE. */
	ire = ire_create(
	    (uchar_t *)&dst_addr,		/* dest address */
	    (uchar_t *)&mask,			/* mask */
	    NULL,				/* no source address */
	    (uchar_t *)&gw_addr,		/* gateway address */
	    gw_ire->ire_max_frag,
	    NULL,				/* no Fast Path header */
	    NULL,				/* no recv-from queue */
	    NULL,				/* no send-to queue */
	    (ushort_t)type,			/* IRE type */
	    NULL,
	    ipif_arg,
	    0,
	    0,
	    0,
	    flags,
	    &gw_ire->ire_uinfo);		/* Inherit ULP info from gw */
	ire_refrele(gw_ire);
	if (ire == NULL)
		return (ENOMEM);

	/*
	 * POLICY: should we allow an RTF_HOST with address INADDR_ANY?
	 * SUN/OS socket stuff does but do we really want to allow 0.0.0.0?
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
			ifrt->ifrt_addr = ire->ire_addr;
			ifrt->ifrt_gateway_addr = ire->ire_gateway_addr;
			ifrt->ifrt_mask = ire->ire_mask;
			ifrt->ifrt_flags = ire->ire_flags;
			ifrt->ifrt_max_frag = ire->ire_max_frag;
			mutex_enter(&ipif->ipif_saved_ire_lock);
			save_mp->b_cont = ipif->ipif_saved_ire_mp;
			ipif->ipif_saved_ire_mp = save_mp;
			mutex_exit(&ipif->ipif_saved_ire_lock);
		}
	}
	if (ioctl_msg)
		ip_rts_rtmsg(RTM_OLDADD, ire, 0);
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
 * ip_rt_delete is called to delete an IPv4 route.
 * ipif_arg is passed in to associate it with the correct interface.
 */
/* ARGSUSED4 */
int
ip_rt_delete(ipaddr_t dst_addr, ipaddr_t mask, ipaddr_t gw_addr,
    uint_t rtm_addrs, int flags, ipif_t *ipif_arg, boolean_t ioctl_msg)
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
		mask = IP_HOST_MASK;
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
	 * interface index, see the comments in ip_rt_add().
	 */
	ipif = ipif_lookup_interface(gw_addr, dst_addr);
	if (ipif != NULL) {
		if (ipif_arg != NULL) {
			ipif = ipif_arg;
			match_flags |= MATCH_IRE_ILL;
		} else {
			match_flags |= MATCH_IRE_IPIF;
		}

		if (ipif->ipif_ire_type == IRE_LOOPBACK)
			ire = ire_ctable_lookup(dst_addr, 0, IRE_LOOPBACK, ipif,
			    NULL, match_flags);
		if (ire == NULL)
			ire = ire_ftable_lookup(dst_addr, mask, 0,
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
		if (mask == IP_HOST_MASK)
			type = IRE_HOST;
		else if (mask == 0)
			type = IRE_DEFAULT;
		else
			type = IRE_PREFIX;
		ire = ire_ftable_lookup(dst_addr, mask, gw_addr, type, ipif_arg,
		    NULL, NULL, 0, match_flags);
		if (ire == NULL && type == IRE_HOST) {
			ire = ire_ftable_lookup(dst_addr, mask, gw_addr,
			    IRE_HOST_REDIRECT, ipif_arg, NULL, NULL, 0,
			    match_flags);
		}
	}

	if (ire == NULL)
		return (ESRCH);
	ipif = ire->ire_ipif;
	if (ipif != NULL) {
		mblk_t	**mpp;
		mblk_t	*mp;
		ifrt_t	*ifrt;

		/* Remove from ipif_saved_ire_mp list if it is there */
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
			if (ifrt->ifrt_addr == ire->ire_addr &&
			    ifrt->ifrt_gateway_addr == ire->ire_gateway_addr &&
			    ifrt->ifrt_mask == ire->ire_mask) {
				*mpp = mp->b_cont;
				freeb(mp);
				break;
			}
		}
		mutex_exit(&ipif->ipif_saved_ire_lock);
	}
	if (ioctl_msg)
		ip_rts_rtmsg(RTM_OLDDEL, ire, 0);
	ire_delete(ire);
	ire_refrele(ire);
	return (0);
}

/*
 * ip_siocaddrt is called by ip_sioctl_copyin_done to complete
 * processing of an SIOCADDRT IOCTL.
 */
static int
ip_siocaddrt(struct rtentry *rt)
{
	ipaddr_t dst_addr;
	ipaddr_t gw_addr;
	ipaddr_t mask;
	int error;

	dst_addr = ((sin_t *)&rt->rt_dst)->sin_addr.s_addr;
	gw_addr = ((sin_t *)&rt->rt_gateway)->sin_addr.s_addr;

	/*
	 * If the RTF_HOST flag is on, this is a request to assign a gateway
	 * to a particular host address.  In this case, we set the netmask to
	 * all ones for the particular destination address.  Otherwise,
	 * determine the netmask to be used based on dst_addr and the interfaces
	 * in use.
	 */
	if (rt->rt_flags & RTF_HOST) {
		mask = IP_HOST_MASK;
	} else {
		/*
		 * Note that ip_subnet_mask returns a zero mask in the case of
		 * default (an all-zeroes address).
		 */
		mask = ip_subnet_mask(dst_addr);
	}

	error = ip_rt_add(dst_addr, mask, gw_addr, rt->rt_flags, NULL, NULL,
	    B_TRUE);
	return (error);
}

/*
 * ip_siocdelrt is called by ip_sioctl_copyin_done to complete
 * processing of an SIOCDELRT IOCTL.
 */
static int
ip_siocdelrt(struct rtentry *rt)
{
	ipaddr_t dst_addr;
	ipaddr_t gw_addr;
	ipaddr_t mask;
	int error;

	dst_addr = ((sin_t *)&rt->rt_dst)->sin_addr.s_addr;
	gw_addr = ((sin_t *)&rt->rt_gateway)->sin_addr.s_addr;

	/*
	 * If the RTF_HOST flag is on, this is a request to delete a gateway
	 * to a particular host address.  In this case, we set the netmask to
	 * all ones for the particular destination address.  Otherwise,
	 * determine the netmask to be used based on dst_addr and the interfaces
	 * in use.
	 */
	if (rt->rt_flags & RTF_HOST) {
		mask = IP_HOST_MASK;
	} else {
		/*
		 * Note that ip_subnet_mask returns a zero mask in the case of
		 * default (an all-zeroes address).
		 */
		mask = ip_subnet_mask(dst_addr);
	}

	error = ip_rt_delete(dst_addr, mask, gw_addr,
	    RTA_DST | RTA_GATEWAY | RTA_NETMASK, rt->rt_flags, NULL, B_TRUE);
	return (error);
}

/*
 * Given an ipif_t, make damned sure that all permanent ARP entries are there
 * for all of the other UP ipif_ts that share its ill_t.  See SIOCSIFADDR
 * handling for why this is needed.
 *
 * This function is only called if the "affected" ipif has ipif_id of 0.
 * This is because:
 *
 *	1. An ipif_down()/ipif_up() sequence will not clobber other ARP
 *	   entries if "affected" is not ipif_id of 0.  (So this function would
 *	   be redundant.)
 *
 *	2. If "affected" is non-zero, then the walkthrough will call
 *	   ipif_arp_up() on the ipif_id of 0.  Guess what?  That clobbers
 *	   all ARP entries.  Since we're trying to recover from that in the
 *	   first place, we're not going to put the intelligence here.
 */
static void
ill_reset_arp(ipif_t *affected)
{
	ipif_t *ipif;

	for (ipif = affected->ipif_ill->ill_ipif; ipif != NULL;
	    ipif = ipif->ipif_next)
		if (ipif != affected && (ipif->ipif_flags & IFF_UP))
			(void) ipif_arp_up(ipif, ipif->ipif_lcl_addr);
}

/*
 * Continue SIOC ioctls following copyin completion.  (Called
 * as writer when ip_sioctl_copyin_writer returns 1.)
 */
void
ip_sioctl_copyin_done(queue_t *q, mblk_t *mp)
{
	char		*addr;
	struct arpreq	*ar;
	area_t		*area;
	int		err = 0;
	struct ifreq	*ifr;
	struct lifreq	*lifr;
	STRUCT_HANDLE(ifconf, ifc);
	STRUCT_HANDLE(lifconf, lifc);
	ill_t		*ill;
	struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;
	sin_t		*sin = NULL;
	sin6_t		*sin6 = NULL;
	ipif_t		*ipif;
	ire_t		*ire;
	mblk_t		*mp1;
	mblk_t		*mp2;
	boolean_t	isv6;
	boolean_t	exists;

	if (q->q_next != NULL) {
		ill = (ill_t *)q->q_ptr;
		isv6 = ill->ill_isv6;
	} else {
		ill = NULL;
		isv6 = ((ipc_t *)q->q_ptr)->ipc_af_isv6;
	}

	if (!(mp1 = mp->b_cont) || !(mp1 = mp1->b_cont)) {
		err = EPROTO;
		goto done;
	}
	addr = (char *)mp1->b_rptr;
	switch (iocp->ioc_cmd) {
	case SIOCADDRT:
		/*
		 * The size of struct rtentry for kernel and user space is
		 * different (smaller for kernel by the size of a pointer).
		 * This allows the struct to be datamodel independent and
		 * no special ioctl macros are needed.
		 */
		err = ip_siocaddrt((struct rtentry *)addr);
		goto done;
	case SIOCDELRT:
		/* See the comment above */
		err = ip_siocdelrt((struct rtentry *)addr);
		goto done;
	case SIOCGETVIFCNT:
		err = mrt_ioctl(iocp->ioc_cmd, (intptr_t)addr);
		/* Break to copyout. */
		break;
	case SIOCGETSGCNT:
	case SIOCGETLSGCNT:
		err = mrt_ioctl(iocp->ioc_cmd, (intptr_t)addr);
		/* Break to copyout. */
		break;
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCGIFDSTADDR:
	case SIOCSIFFLAGS:
	case SIOCGIFFLAGS:
	case SIOCSIFMTU:
	case SIOCGIFMTU:
	case SIOCSIFBRDADDR:
	case SIOCGIFBRDADDR:
	case SIOCSIFNETMASK:
	case SIOCGIFNETMASK:
	case SIOCSIFMETRIC:
	case SIOCGIFMETRIC:
	case SIOCSIFMUXID:
	case SIOCGIFMUXID:
	case SIOCSIFINDEX:
	case SIOCGIFINDEX:
	case SIOCSIFNAME:
	{
		/* Pull out common information before we switch again. */
		ifr = (struct ifreq *)addr;
		sin = (sin_t *)&ifr->ifr_addr;
		ipif = ipif_lookup_on_name(ifr->ifr_name,
		    mi_strlen(ifr->ifr_name) + 1, B_FALSE, &exists, isv6);
		if (ipif == NULL && ill != NULL && ill->ill_ipif != NULL) {
			/*
			 * Handle a SIOCSIFNAME or a SIOC?IF* with a null name
			 * during plumb (on the ill queue before the I_PLINK).
			 */
			if (iocp->ioc_cmd == SIOCSIFNAME ||
			    ifr->ifr_name[0] == '\0')
				ipif = ill->ill_ipif;
		}
		if (ipif == NULL) {
			err = ENXIO;
			goto done;
		}
		if (ipif->ipif_isv6) {
			err = ENXIO;
			goto done;
		}
		switch (iocp->ioc_cmd) {
		case SIOCSIFADDR:
			/* Set the interface address. */
			err = ip_sioctl_addr(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGIFADDR:
			/* Get the interface address. */
			*sin = sin_null;
			sin->sin_family = AF_INET;
			ASSERT(IN6_IS_ADDR_V4MAPPED(&ipif->ipif_v6lcl_addr));
			sin->sin_addr.s_addr = ipif->ipif_lcl_addr;
			break;	/* Break to copyout */
		case SIOCSIFDSTADDR:
			/* Set point to point destination address. */
			err = ip_sioctl_dstaddr(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGIFDSTADDR:
			/* Get point to point destination address. */
			if ((ipif->ipif_flags & IFF_POINTOPOINT) == 0) {
				err = EADDRNOTAVAIL;
				goto done;
			}
			*sin = sin_null;
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = ipif->ipif_pp_dst_addr;
			break;	/* Break to copyout */
		case SIOCSIFFLAGS:
			/*
			 * Set interface flags.
			 *
			 * Since ip_sioctl_flags expects an int and ifr_flags
			 * is a short we need to cast ifr_flags into an int
			 * to avoid have sign extension cause bits to get
			 * set that should not be. bug 4261497.
			 */
			err = ip_sioctl_flags(ipif, (int)ifr->ifr_flags &
			    0x0000ffff, q, mp, iocp->ioc_cmd);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGIFFLAGS:
			/* Get interface flags (low 16 only). */
			ifr->ifr_flags = (short)(ipif->ipif_flags & 0xFFFF);
			break;	/* Break to copyout */
		case SIOCSIFMTU:
			err = ip_sioctl_mtu(ipif, ifr->ifr_metric, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGIFMTU:
			/* Get interface MTU. */
			ifr->ifr_metric = ipif->ipif_mtu;
			break;	/* Break to copyout */
		case SIOCSIFBRDADDR:
			/* Set the interface broadcast address. */
			err = ip_sioctl_brdaddr(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGIFBRDADDR:
			/* Get interface broadcast address. */
			if (!(ipif->ipif_flags & IFF_BROADCAST)) {
				err = EADDRNOTAVAIL;
				goto done;
			}
			*sin = sin_null;
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = ipif->ipif_brd_addr;
			break;	/* Break to copyout */
		case SIOCSIFNETMASK:
			/* Set interface net mask. */
			err = ip_sioctl_netmask(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGIFNETMASK:
			/* Get interface net mask. */
			*sin = sin_null;
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = ipif->ipif_net_mask;
			break;	/* Break to copyout */
		case SIOCSIFMETRIC:
			/*
			 * Set interface metric.  We don't use this for
			 * anything but we keep track of it in case it is
			 * important to routing applications or such.
			 */
			ipif->ipif_metric = ifr->ifr_metric;
			goto done;
		case SIOCGIFMETRIC:
			/* Get interface metric. */
			ifr->ifr_metric = ipif->ipif_metric;
			break;	/* Break to copyout */
		case SIOCSIFMUXID:
			/*
			 * Set the muxid returned from I_PLINK.
			 */
			ipif->ipif_ill->ill_ip_muxid = ifr->ifr_ip_muxid;
			ipif->ipif_ill->ill_arp_muxid = ifr->ifr_arp_muxid;
			goto done;
		case SIOCGIFMUXID:
			/*
			 * Get the muxid saved in ill for I_PUNLINK.
			 */
			ifr->ifr_ip_muxid = ipif->ipif_ill->ill_ip_muxid;
			ifr->ifr_arp_muxid = ipif->ipif_ill->ill_arp_muxid;
			break;	/* Break to copyout */
		case SIOCGIFINDEX:
			/* Get the interface index */
			ifr->ifr_index = ipif->ipif_index;
			break;	/* Break to copyout */
		case SIOCSIFINDEX:
			err = ip_sioctl_slifindex(ipif, ifr->ifr_index);
			goto done;
		case SIOCSIFNAME:
			err = ENXIO;
			goto done;
		}
		break;	/* Break to copyout */
	}

	case SIOCLIFREMOVEIF:
	case SIOCLIFADDIF:
	case SIOCSLIFADDR:
	case SIOCGLIFADDR:
	case SIOCSLIFDSTADDR:
	case SIOCGLIFDSTADDR:
	case SIOCSLIFFLAGS:
	case SIOCGLIFFLAGS:
	case SIOCSLIFMTU:
	case SIOCGLIFMTU:
	case SIOCSLIFBRDADDR:
	case SIOCGLIFBRDADDR:
	case SIOCSLIFNETMASK:
	case SIOCGLIFNETMASK:
	case SIOCSLIFMETRIC:
	case SIOCGLIFMETRIC:
	case SIOCSLIFMUXID:
	case SIOCGLIFMUXID:
	case SIOCSLIFINDEX:
	case SIOCGLIFINDEX:
	case SIOCSLIFNAME:
	case SIOCSLIFSUBNET:
	case SIOCGLIFSUBNET:
	case SIOCSLIFTOKEN:
	case SIOCGLIFTOKEN:
	case SIOCSLIFLNKINFO:
	case SIOCGLIFLNKINFO:
	case SIOCLIFDELND:
	case SIOCLIFGETND:
	case SIOCLIFSETND:
	{
		/* Pull out common information before we switch again. */
		lifr = (struct lifreq *)addr;
		sin = (sin_t *)&lifr->lifr_addr;
		sin6 = (sin6_t *)&lifr->lifr_addr;
		ipif = ipif_lookup_on_name(lifr->lifr_name,
		    mi_strlen(lifr->lifr_name) + 1, B_FALSE, &exists, isv6);

		/*
		 * Allow creating lo0 using SIOCLIFADDIF
		 */
		if (ipif == NULL && iocp->ioc_cmd == SIOCLIFADDIF &&
		    (mi_strlen(lifr->lifr_name) + 1 ==
		    sizeof (ipif_loopback_name)) &&
		    mi_strcmp(lifr->lifr_name, ipif_loopback_name) == 0) {
			ipif = ipif_lookup_on_name(lifr->lifr_name,
			    mi_strlen(lifr->lifr_name) + 1,
			    B_TRUE, &exists, isv6);
			/* Prevent any further action */
			if (ipif == NULL) {
				err = ENOBUFS;
				goto done;
			}
			break;	/* Break to copyout */
		}

		if (ipif == NULL && ill != NULL && ill->ill_ipif != NULL) {
			/*
			 * Handle a SIOCSLIFNAME or a SIOC?LIF* with a null name
			 * during plumb (on the ill queue before the I_PLINK).
			 */
			if (iocp->ioc_cmd == SIOCSLIFNAME ||
			    lifr->lifr_name[0] == '\0')
				ipif = ill->ill_ipif;
		}
		/*
		 * For SIOCLIFADDIF the logical interface might not exist
		 * as long as the physical interface does.
		 */
		if (ipif == NULL && iocp->ioc_cmd != SIOCLIFADDIF) {
			err = ENXIO;
			goto done;
		}
		switch (iocp->ioc_cmd) {
		case SIOCLIFREMOVEIF:
			err = ip_sioctl_removeif(ipif, sin);
			goto done;
		case SIOCLIFADDIF:
			err = ip_sioctl_addif(ipif, lifr->lifr_name, sin, isv6,
			    q, mp);
			if (err == EINPROGRESS)
				return;
			if (err)
				goto done;
			break;	/* Break to copyout */
		case SIOCSLIFADDR:
			err = ip_sioctl_addr(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFADDR:
			if (ipif->ipif_isv6) {
				*sin6 = sin6_null;
				sin6->sin6_family = AF_INET6;
				sin6->sin6_addr =
				    ipif->ipif_v6lcl_addr;
				lifr->lifr_addrlen =
				    ip_mask_to_index_v6(&ipif->ipif_v6net_mask);
			} else {
				*sin = sin_null;
				sin->sin_family = AF_INET;
				sin->sin_addr.s_addr =
				    ipif->ipif_lcl_addr;
				lifr->lifr_addrlen =
				    ip_mask_to_index(ipif->ipif_net_mask);
			}
			break;	/* Break to copyout */
		case SIOCSLIFDSTADDR:
			/* Set point to point destination address. */
			err = ip_sioctl_dstaddr(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFDSTADDR:
			/* Get point to point destination address. */
			if ((ipif->ipif_flags & IFF_POINTOPOINT) == 0) {
				err = EADDRNOTAVAIL;
				goto done;
			}
			if (ipif->ipif_isv6) {
				*sin6 = sin6_null;
				sin6->sin6_family = AF_INET6;
				sin6->sin6_addr =
				    ipif->ipif_v6pp_dst_addr;
			} else {
				*sin = sin_null;
				sin->sin_family = AF_INET;
				sin->sin_addr.s_addr =
				    ipif->ipif_pp_dst_addr;
			}
			break;	/* Break to copyout */
		case SIOCSLIFFLAGS:
			/* Set interface flags. */
			err = ip_sioctl_flags(ipif, lifr->lifr_flags, q, mp,
			    iocp->ioc_cmd);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFFLAGS:
			/* Get interface flags. */
			lifr->lifr_flags = ipif->ipif_flags;
			break;	/* Break to copyout */
		case SIOCSLIFMTU:
			err = ip_sioctl_mtu(ipif, lifr->lifr_mtu, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFMTU:
			/* Get interface MTU. */
			lifr->lifr_mtu = ipif->ipif_mtu;
			break;	/* Break to copyout */
		case SIOCSLIFBRDADDR:
			/* Set the interface broadcast address. */
			err = ip_sioctl_brdaddr(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFBRDADDR:
			/* Get interface broadcast address. */
			if (!(ipif->ipif_flags & IFF_BROADCAST)) {
				err = EADDRNOTAVAIL;
				goto done;
			}
			/* IFF_BROADCAST not possible with IPv6 */
			ASSERT(!ipif->ipif_isv6);
			*sin = sin_null;
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = ipif->ipif_brd_addr;
			break;	/* Break to copyout */
		case SIOCSLIFNETMASK:
			/* Set interface net mask. */
			err = ip_sioctl_netmask(ipif, sin, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFNETMASK:
			/* Get interface net mask. */
			if (ipif->ipif_isv6) {
				*sin6 = sin6_null;
				sin6->sin6_family = AF_INET6;
				sin6->sin6_addr =
				    ipif->ipif_v6net_mask;
				lifr->lifr_addrlen =
				    ip_mask_to_index_v6(&ipif->ipif_v6net_mask);
			} else {
				*sin = sin_null;
				sin->sin_family = AF_INET;
				sin->sin_addr.s_addr =
				    ipif->ipif_net_mask;
				lifr->lifr_addrlen =
				    ip_mask_to_index(ipif->ipif_net_mask);
			}
			break;	/* Break to copyout */
		case SIOCSLIFMETRIC:
			/*
			 * Set interface metric.  We don't use this for
			 * anything but we keep track of it in case it is
			 * important to routing applications or such.
			 */
			ipif->ipif_metric = lifr->lifr_metric;
			goto done;
		case SIOCGLIFMETRIC:
			/* Get interface metric. */
			lifr->lifr_metric = ipif->ipif_metric;
			break;	/* Break to copyout */
		case SIOCSLIFMUXID:
			/*
			 * Set the muxid returned from I_PLINK.
			 */
			ipif->ipif_ill->ill_ip_muxid = lifr->lifr_ip_muxid;
			ipif->ipif_ill->ill_arp_muxid = lifr->lifr_arp_muxid;
			goto done;
		case SIOCGLIFMUXID:
			/*
			 * Get the muxid saved in ill for I_PUNLINK.
			 */
			lifr->lifr_ip_muxid = ipif->ipif_ill->ill_ip_muxid;
			lifr->lifr_arp_muxid = ipif->ipif_ill->ill_arp_muxid;
			break;	/* Break to copyout */
		case SIOCGLIFINDEX:
			/* Get the interface index */
			lifr->lifr_index = ipif->ipif_index;
			break;	/* Break to copyout */
		case SIOCSLIFINDEX:
			err = ip_sioctl_slifindex(ipif, lifr->lifr_index);
			goto done;
		case SIOCSLIFNAME:
			err = ip_sioctl_slifname(lifr, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCSLIFSUBNET:
			err = ip_sioctl_subnet(ipif, sin, lifr->lifr_addrlen,
			    q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFSUBNET:
			if (ipif->ipif_isv6) {
				*sin6 = sin6_null;
				sin6->sin6_family = AF_INET6;
				sin6->sin6_addr =
				    ipif->ipif_v6subnet;
				lifr->lifr_addrlen =
				    ip_mask_to_index_v6(&ipif->ipif_v6net_mask);
			} else {
				*sin = sin_null;
				sin->sin_family = AF_INET;
				sin->sin_addr.s_addr =
				    ipif->ipif_subnet;
				lifr->lifr_addrlen =
				    ip_mask_to_index(ipif->ipif_net_mask);
			}
			break;	/* Break to copyout */
		case SIOCSLIFTOKEN:
			err = ip_sioctl_token(ipif, sin6,
			    lifr->lifr_addrlen, q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;
		case SIOCGLIFTOKEN: {
			ill_t *ill;

			if (ipif->ipif_id != 0) {
				err = EINVAL;
				goto done;
			}
			ill = ipif->ipif_ill;

			if (!ill->ill_isv6) {
				err = ENXIO;
				goto done;
			}
			*sin6 = sin6_null;
			sin6->sin6_family = AF_INET6;
			ASSERT(!IN6_IS_ADDR_V4MAPPED(&ill->ill_token));
			sin6->sin6_addr = ill->ill_token;
			lifr->lifr_addrlen = ill->ill_token_length;
			break;	/* Break to copyout */
		}
		case SIOCSLIFLNKINFO:
			err = ip_sioctl_lnkinfo(ipif, &lifr->lifr_ifinfo,
			    q, mp);
			if (err == EINPROGRESS)
				return;
			goto done;

		case SIOCGLIFLNKINFO: {
			struct lif_ifinfo_req *lir;
			ill_t *ill = ipif->ipif_ill;

			if (ipif->ipif_id != 0) {
				err = EINVAL;
				goto done;
			}
			lir = &lifr->lifr_ifinfo;
			lir->lir_maxhops = ill->ill_max_hops;
			lir->lir_reachtime = ill->ill_reachable_time;
			lir->lir_reachretrans = ill->ill_reachable_retrans_time;
			lir->lir_maxmtu = ill->ill_max_mtu;
			break;	/* Break to copyout */
		}
		case SIOCLIFDELND:
			err = ip_siocdelndp_v6(ipif, &lifr->lifr_nd, q);
			goto done;
		case SIOCLIFGETND:
			err = ip_siocqueryndp_v6(ipif, &lifr->lifr_nd, q, mp);
			if (err != 0)
				goto done;
			break;	/* Break to copyout */
		case SIOCLIFSETND:
			err = ip_siocsetndp_v6(ipif, &lifr->lifr_nd, q, mp);
			goto done;
		default:
			err = ENXIO;
			goto done;
		}
		break;	/* Break to copyout */
	}

	case SIOCGIFCONF:
	/*
	 * The original SIOCGIFCONF passed in a struct ifconf which specified
	 * the user buffer address and length into which the list of struct
	 * ifreqs was to be copied.  Since AT&T Streams does not seem to
	 * allow M_COPYOUT to be used in conjunction with I_STR IOCTLS,
	 * the SIOCGIFCONF operation was redefined to simply provide
	 * a large output buffer into which we are supposed to jam the ifreq
	 * array.  The same ioctl command code was used, despite the fact that
	 * both the applications and the kernel code had to change, thus making
	 * it impossible to support both interfaces.
	 *
	 * For reasons not good enough to try to explain, the following
	 * algorithm is used for deciding what to do with one of these:
	 * If the IOCTL comes in as an I_STR, it is assumed to be of the new
	 * form with the output buffer coming down as the continuation message.
	 * If it arrives as a TRANSPARENT IOCTL, it is assumed to be old style,
	 * and we have to copy in the ifconf structure to find out how big the
	 * output buffer is and where to copy out to.  Sure no problem...
	 *
	 */
		STRUCT_SET_HANDLE(ifc, iocp->ioc_flag, NULL);
		if ((mp1->b_wptr - mp1->b_rptr) == STRUCT_SIZE(ifc)) {
			int numifs = 0;
			size_t ifc_bufsize;

			/*
			 * Must be (better be!) continuation of a TRANSPARENT
			 * IOCTL.  We just copied in the ifconf structure.
			 */
			STRUCT_SET_HANDLE(ifc, iocp->ioc_flag,
			    (struct ifconf *)addr);

			/*
			 * Allocate a buffer to hold requested information.
			 *
			 * If ifc_len is larger than what is needed, we only
			 * allocate what we will use.
			 *
			 * If ifc_len is smaller than what is needed, return
			 * EINVAL.
			 *
			 * XXX: the ill_t structure can hava 2 counters, for
			 * v4 and v6 (not just ill_ipif_up_count) to store the
			 * number of interfaces for a device, so we don't need
			 * to count them here...
			 */
			for (ill = ill_g_head; ill; ill = ill->ill_next) {
				if (ill->ill_isv6)
					continue;
				for (ipif = ill->ill_ipif; ipif;
				    ipif = ipif->ipif_next)
					numifs++;
			}

			ifc_bufsize = numifs * sizeof (struct ifreq);
			if (ifc_bufsize > STRUCT_FGET(ifc, ifc_len)) {
				err = EINVAL;
				goto done;
			}

			mp1 = mi_copyout_alloc(q, mp,
			    STRUCT_FGETP(ifc, ifc_buf), ifc_bufsize);
			if (mp1 == NULL)
				return;
			mp1->b_wptr = mp1->b_rptr + ifc_bufsize;
		}
		bzero(mp1->b_rptr, mp1->b_wptr - mp1->b_rptr);
		ifr = (struct ifreq *)mp1->b_rptr;
		for (ill = ill_g_head; ill; ill = ill->ill_next) {
			/*
			 * the SIOCGIFCONF ioctl only knows about
			 * IPv4 addresses, so don't try to tell
			 * it about interfaces with IPv6-only
			 * addresses.
			 */
			if (ill->ill_isv6)
				continue;
			for (ipif = ill->ill_ipif; ipif;
			    ipif = ipif->ipif_next) {
				if ((uchar_t *)&ifr[1] > mp1->b_wptr) {
					err = EINVAL;
					goto done;
				}
				(void) ipif_get_name(ipif,
				    ifr->ifr_name,
				    sizeof (ifr->ifr_name));
				sin = (sin_t *)&ifr->ifr_addr;
				*sin = sin_null;
				sin->sin_family = AF_INET;
				sin->sin_addr.s_addr = ipif->ipif_lcl_addr;
				ifr++;
			}
		}
		mp1->b_wptr = (uchar_t *)ifr;
		if (STRUCT_BUF(ifc) != NULL) {
			STRUCT_FSET(ifc, ifc_len,
				(int)((uchar_t *)ifr - mp1->b_rptr));
		}
		mi_copyout(q, mp);
		return;

	case SIOCGIFNUM: {
		/* Get number of interfaces. */
		int num = 0;
		int *nump = (int *)addr;

		for (ill = ill_g_head; ill; ill = ill->ill_next) {
			if (ill->ill_isv6)
				continue;
			for (ipif = ill->ill_ipif; ipif;
			    ipif = ipif->ipif_next)
				num++;
		}
		*nump = num;
		break;	/* Break to copyout */
	}
	case SIOCGLIFCONF: {
		sa_family_t	family;
		int		flags;
		int		numlifs = 0;
		size_t		lifc_bufsize;

		/*
		 * An extended version of SIOCGIFCONF that takes an
		 * additional address family and flags field.
		 * AF_UNSPEC retrieve both IPv4 and IPv6.
		 * Unless LIFC_NOXMIT is specified the IFF_NOXMIT
		 * interfaces are omitted.
		 * If LIFC_EXTERNAL_SOURCE is specified, IFF_NOXMIT,
		 * IFF_NOLOCAL, IFF_LOOPBACK, IFF_DEPRECATED and
		 * not IFF_UP interfaces are omitted. LIFC_EXTERNAL_SOURCE
		 * has priority over LIFC_NOXMIT.
		 */
		STRUCT_SET_HANDLE(lifc, iocp->ioc_flag, NULL);


		if ((mp1->b_wptr - mp1->b_rptr) != STRUCT_SIZE(lifc)) {
			err = EINVAL;
			goto done;
		}
		/*
		 * Must be (better be!) continuation of a TRANSPARENT
		 * IOCTL.  We just copied in the lifconf structure.
		 */
		STRUCT_SET_HANDLE(lifc, iocp->ioc_flag,
		    (struct lifconf *)addr);

		family = STRUCT_FGET(lifc, lifc_family);
		flags = STRUCT_FGET(lifc, lifc_flags);

		switch (family) {
		case AF_UNSPEC:
		case AF_INET:
		case AF_INET6:
			break;
		default:
			err = EAFNOSUPPORT;
			goto done;
		}

		/*
		 * Allocate a buffer to hold requested information.
		 *
		 * If lifc_len is larger than what is needed, we only
		 * allocate what we will use.
		 *
		 * If lifc_len is smaller than what is needed, return
		 * EINVAL.
		 */
		for (ill = ill_g_head; ill; ill = ill->ill_next) {
			if (family == AF_INET && ill->ill_isv6)
				continue;
			if (family == AF_INET6 && !ill->ill_isv6)
				continue;
			for (ipif = ill->ill_ipif; ipif;
			    ipif = ipif->ipif_next) {
				if ((ipif->ipif_flags & IFF_NOXMIT) &&
				    !(flags & LIFC_NOXMIT))
					continue;
				if (((ipif->ipif_flags &
				    (IFF_NOXMIT|IFF_NOLOCAL|IFF_LOOPBACK|
				    IFF_DEPRECATED)) ||
				    !(ipif->ipif_flags & IFF_UP)) &&
				    (flags & LIFC_EXTERNAL_SOURCE))
					continue;
				numlifs++;
			}
		}

		lifc_bufsize = numlifs * sizeof (struct lifreq);
		if (lifc_bufsize > STRUCT_FGET(lifc, lifc_len)) {
			err = EINVAL;
			goto done;
		}

		mp1 = mi_copyout_alloc(q, mp,
		    STRUCT_FGETP(lifc, lifc_buf), lifc_bufsize);
		if (mp1 == NULL)
			return;
		mp1->b_wptr = mp1->b_rptr + lifc_bufsize;

		bzero(mp1->b_rptr, mp1->b_wptr - mp1->b_rptr);
		lifr = (struct lifreq *)mp1->b_rptr;
		for (ill = ill_g_head; ill; ill = ill->ill_next) {
			if (family == AF_INET && ill->ill_isv6)
				continue;
			if (family == AF_INET6 && !ill->ill_isv6)
				continue;
			for (ipif = ill->ill_ipif; ipif;
			    ipif = ipif->ipif_next) {
				if ((ipif->ipif_flags & IFF_NOXMIT) &&
				    !(flags & LIFC_NOXMIT))
					continue;
				if (((ipif->ipif_flags &
				    (IFF_NOXMIT|IFF_NOLOCAL|IFF_LOOPBACK|
				    IFF_DEPRECATED)) ||
				    !(ipif->ipif_flags & IFF_UP)) &&
				    (flags & LIFC_EXTERNAL_SOURCE))
					continue;

				if ((uchar_t *)&lifr[1] > mp1->b_wptr) {
					err = EINVAL;
					goto done;
				}
				(void) ipif_get_name(ipif,
				    lifr->lifr_name,
				    sizeof (lifr->lifr_name));
				if (ipif->ipif_isv6) {
					sin6 = (sin6_t *)&lifr->lifr_addr;
					*sin6 = sin6_null;
					sin6->sin6_family = AF_INET6;
					sin6->sin6_addr =
					    ipif->ipif_v6lcl_addr;
					lifr->lifr_addrlen =
					    ip_mask_to_index_v6(
					    &ipif->ipif_v6net_mask);
				} else {
					sin = (sin_t *)&lifr->lifr_addr;
					*sin = sin_null;
					sin->sin_family = AF_INET;
					sin->sin_addr.s_addr =
					    ipif->ipif_lcl_addr;
					lifr->lifr_addrlen =
					    ip_mask_to_index(
					    ipif->ipif_net_mask);
				}
				lifr++;
			}
		}
		mp1->b_wptr = (uchar_t *)lifr;
		if (STRUCT_BUF(lifc) != NULL) {
			STRUCT_FSET(lifc, lifc_len,
				(int)((uchar_t *)lifr - mp1->b_rptr));
		}
		mi_copyout(q, mp);
		return;
	}
	case SIOCGLIFNUM: {
		/*
		 * An extended version of SIOCGIFNUM that takes an
		 * additional address family and flags field.
		 * AF_UNSPEC retrieve both IPv4 and IPv6.
		 * Unless LIFC_NOXMIT is specified the IFF_NOXMIT
		 * interfaces are omitted.
		 * If LIFC_EXTERNAL_SOURCE is specified, IFF_NOXMIT,
		 * IFF_NOLOCAL, IFF_LOOPBACK, IFF_DEPRECATED and
		 * not IFF_UP interfaces are omitted. LIFC_EXTERNAL_SOURCE
		 * has priority over LIFC_NOXMIT.
		 */
		int num = 0;
		struct lifnum *lifn;

		lifn = (struct lifnum *)addr;
		switch (lifn->lifn_family) {
		case AF_UNSPEC:
		case AF_INET:
		case AF_INET6:
			break;
		default:
			err = EAFNOSUPPORT;
			goto done;
		}

		for (ill = ill_g_head; ill; ill = ill->ill_next) {
			if (lifn->lifn_family == AF_INET && ill->ill_isv6)
				continue;
			if (lifn->lifn_family == AF_INET6 && !ill->ill_isv6)
				continue;
			for (ipif = ill->ill_ipif; ipif;
			    ipif = ipif->ipif_next) {
				if ((ipif->ipif_flags & IFF_NOXMIT) &&
				    !(lifn->lifn_flags & LIFC_NOXMIT))
					continue;
				if (((ipif->ipif_flags &
				    (IFF_NOXMIT|IFF_NOLOCAL|IFF_LOOPBACK|
				    IFF_DEPRECATED)) ||
				    !(ipif->ipif_flags & IFF_UP)) &&
				    (lifn->lifn_flags & LIFC_EXTERNAL_SOURCE))
					continue;
				num++;
			}
		}
		lifn->lifn_count = num;
		break;	/* Break to copyout */
	}
	case SIOCTMYADDR: {
		/*
		 * Check if this is an address assigned to this machine.
		 * Skips interfaces that are down by using ire checks.
		 * Translates mapped addresses to v4 addresses and then
		 * treats them as such, returning true if the v4 address
		 * associated with this mapped address is configured.
		 * Note: Applications will have to be careful what they do
		 * with the response; use of mapped addresses limits
		 * what can be done with the socket, especially with
		 * respect to socket options and ioctls - neither IPv4
		 * options nor IPv6 sticky options/ancillary data options
		 * may be used.
		 */
		struct sioc_addrreq *sia;
		sin_t *sin;
		ire_t *ire;

		sia = (struct sioc_addrreq *)addr;
		sin = (sin_t *)&sia->sa_addr;
		switch (sin->sin_family) {
		case AF_INET6: {
			sin6_t *sin6 = (sin6_t *)sin;

			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				ipaddr_t v4_addr;

				IN6_V4MAPPED_TO_IPADDR(&sin6->sin6_addr,
				    v4_addr);
				ire = ire_ctable_lookup(v4_addr, 0,
				    IRE_LOCAL|IRE_LOOPBACK, NULL, NULL,
				    MATCH_IRE_TYPE);
			} else {
				in6_addr_t v6addr;

				v6addr = sin6->sin6_addr;
				ire = ire_ctable_lookup_v6(&v6addr, 0,
				    IRE_LOCAL|IRE_LOOPBACK, NULL, NULL,
				    MATCH_IRE_TYPE);
			}
			break;
		}
		case AF_INET: {
			ipaddr_t v4addr;

			v4addr = sin->sin_addr.s_addr;
			ire = ire_ctable_lookup(v4addr, 0,
			    IRE_LOCAL|IRE_LOOPBACK, NULL, NULL,
			    MATCH_IRE_TYPE);
			break;
		}
		default:
			err = EAFNOSUPPORT;
			goto done;
		}
		if (ire != NULL) {
			sia->sa_res = 1;
			ire_refrele(ire);
		} else {
			sia->sa_res = 0;
		}
		break;	/* Break to copyout */
	}
	case SIOCTONLINK: {
		/*
		 * Check if this is an address assigned on-link i.e.
		 * neighbor.
		 * Skips interfaces that are down by using ire checks.
		 * Includes neighbors caused by redirect etc i.e.
		 * it does not only check for interface IREs.
		 * Returns true for my addresses as well.
		 * Translates mapped addresses to v4 addresses and then
		 * treats them as such, returning true if the v4 address
		 * associated with this mapped address is configured.
		 * Note: Applications will have to be careful what they do
		 * with the response; use of mapped addresses limits
		 * what can be done with the socket, especially with
		 * respect to socket options and ioctls - neither IPv4
		 * options nor IPv6 sticky options/ancillary data options
		 * may be used.
		 */
		struct sioc_addrreq *sia;
		sin_t *sin;
		ire_t *ire;

		sia = (struct sioc_addrreq *)addr;
		sin = (sin_t *)&sia->sa_addr;

		/*
		 * Match addresses with a zero gateway field to avoid
		 * routes going through a router.
		 * Exclude broadcast and multicast addresses.
		 */
		switch (sin->sin_family) {
		case AF_INET6: {
			sin6_t *sin6 = (sin6_t *)sin;

			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				ipaddr_t v4_addr;

				IN6_V4MAPPED_TO_IPADDR(&sin6->sin6_addr,
				    v4_addr);
				if (CLASSD(v4_addr))
					ire = NULL;
				else
					ire = ire_route_lookup(v4_addr, 0, 0, 0,
					    NULL, NULL, NULL, MATCH_IRE_GW);
			} else {
				in6_addr_t v6addr;
				in6_addr_t v6gw;

				v6addr = sin6->sin6_addr;
				v6gw = ipv6_all_zeros;
				if (IN6_IS_ADDR_MULTICAST(&v6addr))
					ire = NULL;
				else
					ire = ire_route_lookup_v6(&v6addr, 0,
					    &v6gw, 0, NULL, NULL, NULL,
					    MATCH_IRE_GW);
			}
			break;
		}
		case AF_INET: {
			ipaddr_t v4addr;

			v4addr = sin->sin_addr.s_addr;
			if (CLASSD(v4addr))
				ire = NULL;
			else
				ire = ire_route_lookup(v4addr, 0, 0, 0,
				    NULL, NULL, NULL, MATCH_IRE_GW);
			break;
		}
		default:
			err = EAFNOSUPPORT;
			goto done;
		}
		if (ire != NULL) {
			if (ire->ire_type & (IRE_INTERFACE|IRE_CACHE|
			    IRE_LOCAL|IRE_LOOPBACK)) {
				sia->sa_res = 1;
			}
			ire_refrele(ire);
		} else {
			sia->sa_res = 0;
		}
		break;	/* Break to copyout */
	}
	case SIOCTMYSITE: {
		/*
		 * TBD: implement when kernel maintaines a list of site
		 * prefixes.
		 */
		err = ENXIO;
		goto done;
	}
	case SIOCSTUNPARAM:
	case SIOCGTUNPARAM: {
		struct iftun_req *ta;
		ipc_t *ipc;
		mblk_t *mp1;

		ta = (struct iftun_req *)addr;

		/* Disallows implicit create */
		ipif = ipif_lookup_on_name(ta->ifta_lifr_name,
		    mi_strlen(ta->ifta_lifr_name) + 1, B_FALSE, &exists, isv6);
		if (ipif == NULL) {
			err = ENXIO;
			goto done;
		}

		if (ipif->ipif_id != 0) {
			/*
			 * We really don't want to set/get tunnel parameters
			 * on virtual tunnel interfaces.  Only allow the
			 * base tunnel to do these.
			 */
			err = EINVAL;
			goto done;
		}

		/*
		 * Send down to tunnel mod for ioctl processing.
		 * Will finish ioctl in ip_rput_other().
		 */
		ill = ipif->ipif_ill;
		if (ill->ill_net_type == IRE_LOOPBACK) {
			err = EOPNOTSUPP;
			goto done;
		}
		if (ill->ill_wq == NULL) {
			err = ENXIO;
			goto done;
		}
		if (ill->ill_pending_mp != NULL) {
			err = EAGAIN;
			goto done;
		}
		mp->b_datap->db_type = M_IOCTL;
		mp1 = copymsg(mp);
		if (mp1 == NULL) {
			err = ENOMEM;
			goto done;
		}
		ASSERT(ill->ill_pending_mp == NULL &&
		    ill->ill_pending_ipif == NULL);
		ill->ill_pending_ipif = ipif;
		ill->ill_pending_mp = mp1;
		ASSERT(WR(q)->q_next == NULL);
		ipc = q->q_ptr;
		ASSERT(ill->ill_pending_q == NULL &&
		    ipc->ipc_pending_ill == NULL);
		ill->ill_pending_q = q;
		ipc->ipc_pending_ill = ill;
		/*
		 * Mark the ioctl as coming from an IPv6 interface for
		 * tun's convienence.
		 */
		if (ill->ill_isv6)
			ta->ifta_flags |= 0x80000000;

		putnext(ill->ill_wq, mp);
		return;
	}
	case IF_UNITSEL:  {
		uint_t ppa = *(uint_t *)addr;
		/*
		 * if no error,
		 * Ack is delayed until DL_PHYS_ADDR_ACK (or DL_ERROR_ACK)
		 */
		err = if_unitsel(q, mp, ppa);
		if (err == EINPROGRESS)
			return;
		goto done;
	}

/*
 * ARP IOCTLs.
 * How does IP get in the business of fronting ARP configuration/queries?
 * Well its like this, the Berkeley ARP IOCTLs (SIOCGARP, SIOCDARP, SIOCSARP)
 * are by tradition passed in through a datagram socket.  That lands in IP.
 * As it happens, this is just as well since the interface is quite crude in
 * that it passes in no information about protocol or hardware types, or
 * interface association.  After making the protocol assumption, IP is in
 * the position to look up the name of the ILL, which ARP will need, and
 * format a request that can be handled by ARP.	 The request is passed up
 * stream to ARP, and the original IOCTL is completed by IP when ARP passes
 * back a response.  ARP supports its own set of more general IOCTLs, in
 * case anyone is interested.
 */
	case SIOCGARP:
	case SIOCSARP:
	case SIOCDARP: {
		ipaddr_t ipaddr;

		if (isv6) {
			err = ENXIO;
			goto done;
		}
		/*
		 * Convert the SIOC{G|S|D}ARP calls into our AR_ENTRY_xxx
		 * calls.
		 */
		ar = (struct arpreq *)addr;
		sin = (sin_t *)&ar->arp_pa;
		ire = ire_ftable_lookup(sin->sin_addr.s_addr,
		    0, 0, IRE_IF_RESOLVER, NULL, NULL, NULL, 0, MATCH_IRE_TYPE);
		if (!ire || !(ill = ire_to_ill(ire))) {
			if (ire != NULL)
				ire_refrele(ire);
			if (iocp->ioc_cmd == SIOCDARP) {
				ipaddr = sin->sin_addr.s_addr;
				ire = ire_ctable_lookup(ipaddr, 0, IRE_CACHE,
				    NULL, NULL, MATCH_IRE_TYPE);
				if (ire) {
					ire_delete(ire);
					ire_refrele(ire);
					goto done;
				}
			}
			err = ENXIO;
			goto done;
		}
	/*
	 * We are going to pass up to ARP a packet chain that looks
	 * like:
	 *
	 * M_IOCTL-->ARP_op_MBLK-->ORIG_M_IOCTL-->MI_COPY_MBLK-->ARPREQ_MBLK
	 */

		/* Get a copy of the original IOCTL mblk to head the chain. */
		mp1 = copyb(mp);
		if (mp1 == NULL) {
			err = ENOMEM;
			if (ire != NULL)
				ire_refrele(ire);
			goto done;
		}

		ipaddr = sin->sin_addr.s_addr;
		mp2 = ill_arp_alloc(ill, (uchar_t *)&ip_area_template, ipaddr);
		if (mp2 == NULL) {
			freeb(mp1);
			err = ENOMEM;
			if (ire != NULL)
				ire_refrele(ire);
			goto done;
		}
		/* Put together the chain. */
		mp1->b_cont = mp2;
		mp1->b_datap->db_type = M_IOCTL;
		mp2->b_cont = mp;
		mp2->b_datap->db_type = M_DATA;

		/* Set the proper command in the ARP message. */
		area = (area_t *)mp2->b_rptr;
		iocp = (struct iocblk *)mp1->b_rptr;
		switch (iocp->ioc_cmd) {
		case SIOCDARP:
			/*
			 * We defer deleting the corresponding IRE until
			 * we return from arp.
			 */
			area->area_cmd = AR_ENTRY_DELETE;
			area->area_proto_mask_offset = 0;
			break;
		case SIOCGARP:
			area->area_cmd = AR_ENTRY_SQUERY;
			area->area_proto_mask_offset = 0;
			break;
		case SIOCSARP: {
			ire_t	*ire1;
			/*
			 * Delete the corresponding ire to make sure IP will
			 * pick up any change from arp.
			 */
			ire1 = ire_ctable_lookup(ipaddr, 0, IRE_CACHE, NULL,
			    NULL, MATCH_IRE_TYPE);
			if (ire1 != NULL) {
				ire_delete(ire1);
				ire_refrele(ire1);
			}
			break;
		}
		}

		iocp->ioc_cmd = area->area_cmd;

		/*
		 * Remember the originating queue pointer so we know where
		 * to complete the request when the resolver reply comes back.
		 */
		((ip_sock_ar_t *)area)->ip_sock_ar_q = q;

		/* Fill in the rest of the ARP operation fields. */

		/*
		 * Theoretically, the sa_family could tell us what link layer
		 * type this operation is trying to deal with.	By common
		 * usage AF_UNSPEC means ethernet.  We'll assume any attempt
		 * to use the SIOC?ARP ioctls is for ethernet, for now.	 Our
		 * new ARP ioctls can be used more generally.
		 */
		switch (ar->arp_ha.sa_family) {
		case AF_UNSPEC:
		default:
			area->area_hw_addr_length = 6;
			break;
		}
		bcopy(ar->arp_ha.sa_data,
		    (char *)area + area->area_hw_addr_offset,
		    area->area_hw_addr_length);

		/* Translate the flags. */
		if (ar->arp_flags & ATF_PERM)
			area->area_flags |= ACE_F_PERMANENT;
		if (ar->arp_flags & ATF_PUBL)
			area->area_flags |= ACE_F_PUBLISH;

		/*
		 * Up to ARP it goes.  The response will come back in
		 * ip_wput as an M_IOCACK message, and will be handed to
		 * ip_sioctl_iocack for completion.
		 */
		putnext(ire->ire_stq, mp1);
		ire_refrele(ire);
		return;
	}
	default:
		break;
	}

	/*
	 * We should only get here when we are simply copying out the single
	 * control structure that we copied in.	 Where nothing is copied out,
	 * we should have branched to "done" below.  Where we are doing a
	 * multi-part copyout, it is handled in line.
	 */
	mi_copyout(q, mp);
	return;

done:;
	mi_copy_done(q, mp, err);
}

/*
 * ip_sioctl_copyin_setup is called by ip_wput with any M_IOCTL message
 * that arrives.  Most of the IOCTLs are "socket" IOCTLs which we handle
 * in either I_STR or TRANSPARENT form, using the mi_copy facility.
 * We establish here the size of the block to be copied in.  mi_copyin
 * arranges for this to happen, an processing continues in ip_wput with
 * an M_IOCDATA message.
 */
void
ip_sioctl_copyin_setup(queue_t *q, mblk_t *mp)
{
	int	copyin_size;
	struct iocblk *iocp = (struct iocblk *)mp->b_rptr;

	if (mp->b_cont == NULL) {
		iocp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocp->ioc_count = 0;
		qreply(q, mp);
		return;
	}
	/*
	 * Only allow a very small subset of IP ioctls on this stream if
	 * IP is a module and not a driver. Allowing ioctls to be processed
	 * in this case may cause assert failures or data corruption.
	 *
	 * Any new ioctl defined for IP needs to be properly added to this
	 * switch.
	 */
	if (q->q_next != NULL) {
		switch (iocp->ioc_cmd) {
			/*
			 * These are the only ioctls that come on an IP
			 * module stream normally, after which this stream
			 * normally becomes a multiplexor (at which time the
			 * stream head will fail all ioctls).
			 */
			case SIOCGIFFLAGS:
			case SIOCGLIFFLAGS:
			case SIOCSLIFNAME:
			case IF_UNITSEL:
				break;
			case SIOCADDRT:
			case SIOCDELRT:
			case SIOCGETVIFCNT:
			case SIOCGETSGCNT:
			case SIOCGETLSGCNT:
			case SIOCSIFADDR:
			case SIOCGIFADDR:
			case SIOCSIFDSTADDR:
			case SIOCGIFDSTADDR:
			case SIOCSIFFLAGS:
			case SIOCSIFMTU:
			case SIOCGIFMTU:
			case SIOCGIFBRDADDR:
			case SIOCSIFBRDADDR:
			case SIOCGIFNETMASK:
			case SIOCSIFNETMASK:
			case SIOCGIFMETRIC:
			case SIOCSIFMETRIC:
			case SIOCSIFMUXID:
			case SIOCGIFMUXID:
			case SIOCSIFINDEX:
			case SIOCGIFINDEX:
			case SIOCLIFREMOVEIF:
			case SIOCLIFADDIF:
			case SIOCSLIFADDR:
			case SIOCGLIFADDR:
			case SIOCSLIFDSTADDR:
			case SIOCGLIFDSTADDR:
			case SIOCSLIFFLAGS:
			case SIOCSLIFMTU:
			case SIOCGLIFMTU:
			case SIOCSLIFBRDADDR:
			case SIOCGLIFBRDADDR:
			case SIOCSLIFNETMASK:
			case SIOCGLIFNETMASK:
			case SIOCSLIFMETRIC:
			case SIOCGLIFMETRIC:
			case SIOCSLIFMUXID:
			case SIOCGLIFMUXID:
			case SIOCSLIFINDEX:
			case SIOCGLIFINDEX:
			case SIOCSLIFSUBNET:
			case SIOCGLIFSUBNET:
			case SIOCSLIFTOKEN:
			case SIOCGLIFTOKEN:
			case SIOCSLIFLNKINFO:
			case SIOCGLIFLNKINFO:
			case SIOCLIFDELND:
			case SIOCLIFGETND:
			case SIOCLIFSETND:
			case SIOCGIFCONF:
			case SIOCGIFNUM:
			case SIOCGLIFCONF:
			case SIOCGLIFNUM:
			case SIOCTMYADDR:
			case SIOCTONLINK:
			case SIOCTMYSITE:
			case SIOCSARP:
			case SIOCGARP:
			case SIOCDARP:
			case SIOCLIPSECONFIG:
			case SIOCFIPSECONFIG:
			case SIOCSIPSECONFIG:
			case SIOCDIPSECONFIG:
			case SIOCSTUNPARAM:
			case SIOCGTUNPARAM:
			case ND_GET:
			case ND_SET:
				/*
				 * These ioctls are not allowed on the IP
				 * stream when IP is a module. So fail the
				 * ioctl now.
				 */
				if (mp->b_cont) {
					freemsg(mp->b_cont);
					mp->b_cont = NULL;
				}
				iocp->ioc_error = EINVAL;
				mp->b_datap->db_type = M_IOCNAK;
				iocp->ioc_count = 0;
				qreply(q, mp);
				return;
			default:
				/*
				 * The ioctl is not one we understand or own.
				 * Pass it along to be processed down stream.
				 */
				putnext(q, mp);
				return;
		}
	}
	if (iocp->ioc_cr && drv_priv(iocp->ioc_cr) != 0) {
		/* Only privileged users can do the following operations. */
		switch (iocp->ioc_cmd) {
		case SIOCSIFADDR:
		case SIOCSIFBRDADDR:
		case SIOCSIFDSTADDR:
		case SIOCSIFFLAGS:
		case SIOCSIFINDEX:
		case SIOCSIFMETRIC:
		case SIOCSIFMTU:
		case SIOCSIFMUXID:
		case SIOCSIFNAME:
		case SIOCSIFNETMASK:
		case SIOCSLIFADDR:
		case SIOCSLIFBRDADDR:
		case SIOCSLIFDSTADDR:
		case SIOCSLIFFLAGS:
		case SIOCSLIFINDEX:
		case SIOCSLIFLNKINFO:
		case SIOCSLIFMETRIC:
		case SIOCSLIFMTU:
		case SIOCSLIFMUXID:
		case SIOCSLIFNAME:
		case SIOCSLIFNETMASK:
		case SIOCSLIFSUBNET:
		case SIOCSLIFTOKEN:

		case SIOCLIFREMOVEIF:
		case SIOCLIFADDIF:

		case SIOCSARP:
		case SIOCDARP:

		case SIOCLIFDELND:
		case SIOCLIFSETND:

		case SIOCADDRT:
		case SIOCDELRT:
		case SIOCSTUNPARAM:

		case SIOCSIPSECONFIG:
		case SIOCDIPSECONFIG:
		case SIOCFIPSECONFIG:
		case SIOCLIPSECONFIG:

		case I_LINK:
		case I_UNLINK:
		case I_PLINK:
		case I_PUNLINK:
		case ND_SET:
		case IP_IOCTL:
		case IF_UNITSEL:
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_error = EPERM;
			iocp->ioc_count = 0;
			qreply(q, mp);
			return;
		}
	}
	switch (iocp->ioc_cmd) {
	case SIOCADDRT:
	case SIOCDELRT:
		/*
		 * The size of struct rtentry for kernel and user space is
		 * different (smaller for kernel by the size of a pointer).
		 * This allows the struct to be datamodel independent and
		 * no special ioctl macros are needed.
		 *
		 * Also note that in case of non-transparent ioctl, we will
		 * have more data than the size of kernel rtentry structure
		 * (not a problem if you are aware about it).
		 */
		copyin_size = sizeof (struct rtentry);
		break;
	case SIOCGETVIFCNT:
		copyin_size = sizeof (struct sioc_vif_req);
		break;
	case SIOCGETSGCNT:
		copyin_size = sizeof (struct sioc_sg_req);
		break;
	case SIOCGETLSGCNT:
		copyin_size = sizeof (struct sioc_lsg_req);
		break;
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCGIFDSTADDR:
	case SIOCSIFFLAGS:
	case SIOCGIFFLAGS:
	case SIOCSIFMTU:
	case SIOCGIFMTU:
	case SIOCGIFBRDADDR:
	case SIOCSIFBRDADDR:
	case SIOCGIFNETMASK:
	case SIOCSIFNETMASK:
	case SIOCGIFMETRIC:
	case SIOCSIFMETRIC:
	case SIOCSIFMUXID:
	case SIOCGIFMUXID:
	case SIOCSIFINDEX:
	case SIOCGIFINDEX:
		copyin_size = sizeof (struct ifreq);
		break;
	case SIOCLIFREMOVEIF:
	case SIOCLIFADDIF:
	case SIOCSLIFADDR:
	case SIOCGLIFADDR:
	case SIOCSLIFDSTADDR:
	case SIOCGLIFDSTADDR:
	case SIOCSLIFFLAGS:
	case SIOCGLIFFLAGS:
	case SIOCSLIFMTU:
	case SIOCGLIFMTU:
	case SIOCSLIFBRDADDR:
	case SIOCGLIFBRDADDR:
	case SIOCSLIFNETMASK:
	case SIOCGLIFNETMASK:
	case SIOCSLIFMETRIC:
	case SIOCGLIFMETRIC:
	case SIOCSLIFMUXID:
	case SIOCGLIFMUXID:
	case SIOCSLIFINDEX:
	case SIOCGLIFINDEX:
	case SIOCSLIFNAME:
	case SIOCSLIFSUBNET:
	case SIOCGLIFSUBNET:
	case SIOCSLIFTOKEN:
	case SIOCGLIFTOKEN:
	case SIOCSLIFLNKINFO:
	case SIOCGLIFLNKINFO:
	case SIOCLIFDELND:
	case SIOCLIFGETND:
	case SIOCLIFSETND:
		copyin_size = sizeof (struct lifreq);
		break;
	case SIOCGIFCONF:
		/*
		 * This IOCTL is hilarious.  See comments in
		 * ip_sioctl_copyin_done for the story.
		 */
		if (iocp->ioc_count == TRANSPARENT)
			copyin_size = SIZEOF_STRUCT(ifconf, iocp->ioc_flag);
		else
			copyin_size = iocp->ioc_count;
		break;
	case SIOCGIFNUM:
		copyin_size = sizeof (int);
		break;
	case SIOCGLIFCONF:
		copyin_size = SIZEOF_STRUCT(lifconf, iocp->ioc_flag);
		break;
	case SIOCGLIFNUM:
		copyin_size = sizeof (struct lifnum);
		break;
	case SIOCTMYADDR:
	case SIOCTONLINK:
	case SIOCTMYSITE:
		copyin_size = sizeof (struct sioc_addrreq);
		break;
	case IF_UNITSEL:
		copyin_size = sizeof (int);
		break;
	case SIOCSARP:
	case SIOCGARP:
	case SIOCDARP:
		copyin_size = sizeof (struct arpreq);
		break;
	case I_LINK:
	case I_UNLINK:
	case I_PLINK:
	case I_PUNLINK:
		if (q->q_next) {
			/* Not for us! */
			putnext(q, mp);
			return;
		}
		/*
		 * We handle linked streams as a convenience only.  Simply
		 * complete the operation successfully, and forget about it.
		 * (Configuration utilities may want to permanently link
		 * plumbing streams under IP for lack of any better place
		 * to put them.)
		 */
		iocp->ioc_count = 0;
done:;
		iocp->ioc_error = 0;
		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		return;

	case SIOCLIPSECONFIG:
		ipsec_config_list(q, mp);
		return;
	case SIOCFIPSECONFIG:
		ipsec_config_flush(q, mp);
		return;
	case SIOCSIPSECONFIG:
		iocp->ioc_count = sizeof (ipsec_conf_t);
		iocp->ioc_error = ipsec_config_add(mp);
		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		return;
	case SIOCDIPSECONFIG:
		iocp->ioc_count = 0;
		iocp->ioc_error = ipsec_config_delete(mp);
		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		return;

	case SIOCSTUNPARAM:
	case SIOCGTUNPARAM:
		copyin_size = sizeof (struct iftun_req);
		break;

	case ND_GET:
		if (nd_getset(q, ip_g_nd, mp))
			goto done;
		/* Didn't like it.  Maybe someone downstream wants it. */
		/* FALLTHRU */
	default:
		/*
		 * The only other IOCTLs that require writer status is
		 * ND_SET. If it isn't one of those, don't bother becoming
		 * writer.
		 */
		if (iocp->ioc_cmd == IP_IOCTL) {
			ip_wput_ioctl(q, mp);
			return;
		} else if (iocp->ioc_cmd == ND_SET) {
			become_exclusive(q, mp, ip_wput_ioctl);
			return;
		}
		if (q->q_next) {
			putnext(q, mp);
			return;
		}
		mp->b_datap->db_type = M_IOCNAK;
		iocp->ioc_error = ENOENT;
		iocp->ioc_count = 0;
		qreply(q, mp);
		return;
	}
	mi_copyin(q, mp, NULL, copyin_size);
}

/*
 * Return 1 if ip_sioctl_copyin_done has to be called as a writer
 * for this ioctl.
 */
int
ip_sioctl_copyin_writer(mblk_t *mp)
{
	struct iocblk *iocp = (struct iocblk *)mp->b_rptr;

	switch (iocp->ioc_cmd) {
	case SIOCGIFADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFCONF:
	case SIOCGIFDSTADDR:
	case SIOCGIFFLAGS:
	case SIOCGIFINDEX:
	case SIOCGIFMETRIC:
	case SIOCGIFMTU:
	case SIOCGIFMUXID:
	case SIOCGIFNETMASK:
	case SIOCGIFNUM:
	case SIOCGLIFADDR:
	case SIOCGLIFBRDADDR:
	case SIOCGLIFCONF:
	case SIOCGLIFDSTADDR:
	case SIOCGLIFFLAGS:
	case SIOCGLIFINDEX:
	case SIOCGLIFLNKINFO:
	case SIOCGLIFMETRIC:
	case SIOCGLIFMTU:
	case SIOCGLIFMUXID:
	case SIOCGLIFNETMASK:
	case SIOCGLIFNUM:
	case SIOCGLIFSUBNET:
	case SIOCGLIFTOKEN:
	case SIOCGARP:
	case SIOCLIFGETND:
	case SIOCGETVIFCNT:
	case SIOCGETSGCNT:
	case SIOCGETLSGCNT:

	case SIOCADDRT:
	case SIOCDELRT:
	case SIOCDARP:
	case SIOCSARP:
	case SIOCLIFDELND:
	case SIOCLIFSETND:
		return (0);

	case SIOCGTUNPARAM: /* Uses ill_pending_q etc */
	default:
		return (1);
	}
}

/* ip_wput hands off ARP IOCTL responses to us */
void
ip_sioctl_iocack(queue_t *q, mblk_t *mp)
{
	struct arpreq *ar;
	area_t	*area;
	mblk_t	*area_mp;
	struct iocblk *iocp;
	mblk_t	*orig_ioc_mp;
	queue_t	*q1;

	/*
	 * We should get back from ARP a packet chain that looks like:
	 * M_IOCACK-->ARP_op_MBLK-->ORIG_M_IOCTL-->MI_COPY_MBLK-->ARPREQ_MBLK
	 */
	if (!(area_mp = mp->b_cont) ||
	    (area_mp->b_wptr - area_mp->b_rptr) < sizeof (ip_sock_ar_t) ||
	    !(orig_ioc_mp = area_mp->b_cont) ||
	    !orig_ioc_mp->b_cont || !orig_ioc_mp->b_cont->b_cont) {
		freemsg(mp);
		return;
	}

	/*
	 * Pick out the originating queue. This should be replaced by a safer
	 * key with lookup.
	 */
	area = (area_t *)area_mp->b_rptr;
	q1 = ((ip_sock_ar_t *)area)->ip_sock_ar_q;
	if (q1 != q) {
		put(q1, mp);
		return;
	}

	/* Uncouple the internally generated IOCTL from the original one */
	area_mp->b_cont = NULL;

	/*
	 * We're done if there was an error or if this is not an SIOCGARP
	 * Catch the case where there is an IRE_CACHE by no entry in the
	 * arp table.
	 */
	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_error && iocp->ioc_cmd == AR_ENTRY_SQUERY) {
		ire_t			*ire;
		dl_unitdata_req_t	*dlup;
		ipaddr_t		addr;
		mblk_t			*llmp;
		int			addr_len;
		ill_t			*ill;
		sin_t			*sin;

		ar = (struct arpreq *)orig_ioc_mp->b_cont->b_cont->b_rptr;
		sin = (sin_t *)&ar->arp_pa;
		addr = sin->sin_addr.s_addr;
		ire = ire_ctable_lookup(addr, 0, IRE_CACHE, NULL, NULL,
		    MATCH_IRE_TYPE);
		if (ire != NULL) {
			ar->arp_flags = ATF_INUSE;
			llmp = ire->ire_dlureq_mp;
			if (llmp != 0 &&
			    (ill = ire_to_ill(ire)) != 0) {
				uchar_t *addr;

				ar->arp_flags |= ATF_COM;
				addr_len = ill->ill_phys_addr_length;
				dlup = (dl_unitdata_req_t *)llmp->b_rptr;
				if (ill->ill_sap_length < 0)
					addr = llmp->b_rptr +
					    dlup->dl_dest_addr_offset;
				else
					addr = llmp->b_rptr +
					    dlup->dl_dest_addr_offset +
					    ill->ill_sap_length;
				bcopy(addr, ar->arp_ha.sa_data, addr_len);
			}
			/* Ditch the internal IOCTL. */
			freemsg(mp);
			/* Complete the original. */
			mi_copyout(q, orig_ioc_mp);
			ire_refrele(ire);
			return;
		}
	}
	/*
	 * Delete the coresponding IRE_CACHE if any.
	 * Reset the error if there was one (in case there was no entry
	 * in arp.)
	 */
	if (iocp->ioc_cmd == AR_ENTRY_DELETE) {
		ire_t			*ire;
		ipaddr_t		addr;
		sin_t			*sin;

		ar = (struct arpreq *)orig_ioc_mp->b_cont->b_cont->b_rptr;
		sin = (sin_t *)&ar->arp_pa;
		addr = sin->sin_addr.s_addr;
		ire = ire_ctable_lookup(addr, 0, IRE_CACHE, NULL, NULL,
		    MATCH_IRE_TYPE);
		if (ire) {
			ire_delete(ire);
			ire_refrele(ire);
			iocp->ioc_error = 0;
		}
	}
	if (iocp->ioc_error || iocp->ioc_cmd != AR_ENTRY_SQUERY) {
		int error = iocp->ioc_error;

		freemsg(mp);
		mi_copy_done(q, orig_ioc_mp, error);
		return;
	}

	/*
	 * Completion of an SIOCGARP.  Translate the information from the
	 * area_t into the struct arpreq.
	 */
	ar = (struct arpreq *)orig_ioc_mp->b_cont->b_cont->b_rptr;
	ar->arp_flags = ATF_INUSE;
	if (area->area_flags & ACE_F_PERMANENT)
		ar->arp_flags |= ATF_PERM;
	if (area->area_flags & ACE_F_PUBLISH)
		ar->arp_flags |= ATF_PUBL;
	if (area->area_hw_addr_length) {
		ar->arp_flags |= ATF_COM;
		bcopy((char *)area + area->area_hw_addr_offset,
		    ar->arp_ha.sa_data, area->area_hw_addr_length);
	}

	/* Ditch the internal IOCTL. */
	freemsg(mp);
	/* Complete the original. */
	mi_copyout(q, orig_ioc_mp);
}

/*
 * Create a new logical interface. If ipif_id is zero (i.e. not a logical
 * interface) create the next available logical interface for this
 * physical interface.
 * If ipif is NULL (i.e. the lookup didn't find one) attempt to create an
 * ipif with the specified name.
 *
 * If the address family is not AF_UNSPEC then set the address as well.
 *
 * If ip_sioctl_addr returns EINPROGRESS then the ioctl (the copyout)
 * is completed when the DL_BIND_ACK arrive in ip_rput_dlpi_writer.
 */
static int
ip_sioctl_addif(ipif_t *ipif, char *name, sin_t *sin, boolean_t isv6,
    queue_t *q, mblk_t *mp)
{
	int err = 0;

	if (ipif == NULL) {
		boolean_t exists;

		/* Look for a matching ill and create the ipif */
		ipif = ipif_lookup_on_name(name, mi_strlen(name) + 1,
		    B_TRUE, &exists, isv6);
		if (ipif == NULL)
			return (ENXIO);
		if (exists)
			return (EEXIST);
	} else {
		unsigned int unit;
		ipif_t *tipif;
		uint_t ire_type;

		/*
		 * Find the lowest unused logical interface id.
		 * Assumes this was called on the physical (id = 0) instance.
		 */
		if (ipif->ipif_id != 0)
			return (EINVAL);

		for (unit = 1; unit <= ip_addrs_per_if; unit++) {
			boolean_t found = B_FALSE;

			for (tipif = ipif->ipif_ill->ill_ipif; tipif;
			    tipif = tipif->ipif_next) {
				if (tipif->ipif_id == unit) {
					found = B_TRUE;
					break;
				}
			}
			if (!found)
				break;
		}
		/*
		 * Use IRE_LOOPBACK only for lun 0 to support "receive only"
		 * use of lo0:1 etc.
		 */
		if (ipif->ipif_ill->ill_net_type == IRE_LOOPBACK && unit == 0)
			ire_type = IRE_LOOPBACK;
		else
			ire_type = IRE_LOCAL;
		ipif = ipif_allocate(ipif->ipif_ill, unit, ire_type);
		if (ipif == NULL) {
			return (ENOBUFS);
		}
	}
	/* Return created name with ioctl */
	(void) sprintf(name, "%s%c%d",
	    ipif->ipif_ill->ill_name, IPIF_SEPARATOR_CHAR,
	    ipif->ipif_id);

	/* Set address */
	if (sin->sin_family != AF_UNSPEC)
		err = ip_sioctl_addr(ipif, sin, q, mp);
	return (err);
}

/*
 * Remove an existing logical interface. If ipif_id is zero (i.e. not a logical
 * interface) delete it based on the IP address (on this physical interface).
 * Otherwise delete it based on the ipif_id.
 * Also, special handling to allow a removeif of lo0.
 */
static int
ip_sioctl_removeif(ipif_t *ipif, sin_t *sin)
{
	if (ipif->ipif_id == 0 && ipif->ipif_net_type == IRE_LOOPBACK) {
		/* unplumb the loopback interface */
		ipif_free(ipif);
		return (0);
	}
	if (ipif->ipif_id == 0) {
		/* Find based on address */
		if (ipif->ipif_isv6) {
			sin6_t *sin6;

			if (sin->sin_family != AF_INET6)
				return (EAFNOSUPPORT);

			sin6 = (sin6_t *)sin;
			ipif = ipif_lookup_addr_v6(&sin6->sin6_addr,
			    ipif->ipif_ill);
		} else {
			ipaddr_t addr;

			if (sin->sin_family != AF_INET)
				return (EAFNOSUPPORT);

			addr = sin->sin_addr.s_addr;
			ipif = ipif_lookup_addr(addr, ipif->ipif_ill);
		}
		if (ipif == NULL)
			return (EADDRNOTAVAIL);
	}

	/*
	 * Can not delete instance zero since it is tied to the ill.
	 */
	if (ipif->ipif_id == 0)
		return (EBUSY);

	ipif_free(ipif);
	return (0);
}

/*
 * Set the local interface address.
 * Allow an address of all zero when the interface is down.
 */
int
ip_sioctl_addr(ipif_t *ipif, sin_t *sin, queue_t *q, mblk_t *mp)
{
	int err = 0;
	in6_addr_t v6addr;
	int need_up = 0;

	if (ipif->ipif_isv6) {
		sin6_t *sin6;

		if (sin->sin_family != AF_INET6)
			return (EAFNOSUPPORT);

		sin6 = (sin6_t *)sin;
		v6addr = sin6->sin6_addr;

		/*
		 * Enforce that true multicast interfaces have a link-local
		 * address for logical unit 0.
		 */
		if (ipif->ipif_id == 0 &&
		    (ipif->ipif_flags & IFF_MULTICAST) &&
		    !(ipif->ipif_flags & (IFF_LOOPBACK|IFF_POINTOPOINT)) &&
		    !IN6_IS_ADDR_LINKLOCAL(&v6addr)) {
			return (EADDRNOTAVAIL);
		}

		if (!ip_local_addr_ok_v6(&v6addr, &ipif->ipif_v6net_mask) &&
		    ((ipif->ipif_flags & IFF_UP) ||
		    !IN6_IS_ADDR_UNSPECIFIED(&v6addr)))
			return (EADDRNOTAVAIL);
	} else {
		ipaddr_t addr;

		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);

		addr = sin->sin_addr.s_addr;
		if (!ip_local_addr_ok(addr, ipif->ipif_net_mask) &&
		    ((ipif->ipif_flags & IFF_UP) || addr != 0))
			return (EADDRNOTAVAIL);

		IN6_IPADDR_TO_V4MAPPED(addr, &v6addr);
	}

	/*
	 * Even if there is no change we redo things just to rerun
	 * ipif_set_default.
	 */

	if (ipif->ipif_flags & IFF_UP) {
		/*
		 * Setting a new local address, make sure
		 * we have net and subnet bcast ire's for
		 * the old address if we need them.
		 */
		if (!ipif->ipif_isv6)
			ipif_check_bcast_ires(ipif);
		/*
		 * If the interface is already marked up,
		 * we call ipif_down which will take care
		 * of ditching any IREs that have been set
		 * up based on the old interface address.
		 */
		ipif_down(ipif);
		need_up = 1;
	}

	if (ipif->ipif_flags & IFF_NOLOCAL) {
		ipif->ipif_v6lcl_addr = ipv6_all_zeros;
		ipif->ipif_v6src_addr = ipv6_all_zeros;
	} else if (ipif->ipif_flags & IFF_ANYCAST) {
		ipif->ipif_v6lcl_addr = v6addr;
		ipif->ipif_v6src_addr = ipv6_all_zeros;
	} else {
		/* Set the new address. */
		ipif->ipif_v6lcl_addr = v6addr;
		ipif->ipif_v6src_addr = v6addr;
	}
	ipif_set_default(ipif);

	if (need_up) {
		/*
		 * Now bring the interface back up.  If this
		 * is the only IPIF for the ILL, ipif_up
		 * will have to re-bind to the device, so
		 * we may get back EINPROGRESS, in which
		 * case, this IOCTL will get completed in
		 * ip_rput_dlpi when we see the DL_BIND_ACK.
		 */
		err = ipif_up(ipif, q, mp);
	}
	return (err);
}

/*
 * Set the destination address for a pt-pt interface.
 */
static int
ip_sioctl_dstaddr(ipif_t *ipif, sin_t *sin, queue_t *q, mblk_t *mp)
{
	int err = 0;
	in6_addr_t v6addr;
	int need_up = 0;

	if (ipif->ipif_isv6) {
		sin6_t *sin6;

		if (sin->sin_family != AF_INET6)
			return (EAFNOSUPPORT);

		sin6 = (sin6_t *)sin;
		v6addr = sin6->sin6_addr;

		if (!ip_remote_addr_ok_v6(&v6addr, &ipif->ipif_v6net_mask))
			return (EADDRNOTAVAIL);
	} else {
		ipaddr_t addr;

		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);

		addr = sin->sin_addr.s_addr;
		if (!ip_remote_addr_ok(addr, ipif->ipif_net_mask))
			return (EADDRNOTAVAIL);

		IN6_IPADDR_TO_V4MAPPED(addr, &v6addr);
	}

	/* Set point to point destination address. */
	if ((ipif->ipif_flags & IFF_POINTOPOINT) == 0) {
		/*
		 * Allow this as a means of creating logical
		 * pt-pt interfaces on top of e.g. an Ethernet.
		 * XXX Undocumented HACK for testing.
		 * pt-pt interfaces are created with NUD disabled.
		 */
		ipif->ipif_flags |= IFF_POINTOPOINT;
		ipif->ipif_flags &= ~IFF_BROADCAST;
		if (ipif->ipif_isv6)
			ipif->ipif_flags |= IFF_NONUD;
	}

	if (!(ipif->ipif_flags & IFF_POINTOPOINT))
		return (EINVAL);

	if (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6pp_dst_addr, &v6addr))
		return (0);	/* No change */

	if (ipif->ipif_flags & IFF_UP) {
		/*
		 * If the interface is already marked up,
		 * we call ipif_down which will take care
		 * of ditching any IREs that have been set
		 * up based on the old pp dst address.
		 */
		ipif_down(ipif);
		need_up = 1;
	}
	/* Set the new address. */
	ipif->ipif_v6pp_dst_addr = v6addr;
	/* Make sure subnet tracks pp_dst */
	ipif->ipif_v6subnet = ipif->ipif_v6pp_dst_addr;

	if (need_up) {
		/*
		 * Now bring the interface back up.  If this
		 * is the only IPIF for the ILL, ipif_up
		 * will have to re-bind to the device, so
		 * we may get back EINPROGRESS, in which
		 * case, this IOCTL will get completed in
		 * ip_rput_dlpi when we see the DL_BIND_ACK.
		 */
		err = ipif_up(ipif, q, mp);
	}
	return (err);
}

/*
 * Set interface flags.
 * Need to do special action for IFF_UP, IFF_DEPRECATED, IFF_NOXMIT,
 * IFF_NOLOCAL, IFF_NONUD, IFF_NOARP, IFF_PRIVATE, IFF_ANYCAST and
 * IFF_ROUTER.
 */
static int
ip_sioctl_flags(ipif_t *ipif, int flags, queue_t *q, mblk_t *mp,
    int cmd)
{
	uint_t	turn_on;
	uint_t	turn_off;
	int	err;
	int	need_up = 0;

	/*
	 * Compare the new flags to the old, and partition
	 * into those coming on and those going off.
	 * For the 16 bit command keep the bits above bit 16 unchanged.
	 */
	if (cmd == SIOCSIFFLAGS)
		flags |= ipif->ipif_flags & ~0xFFFF;

	/*
	 * First check which bits will change and then which will
	 * go on and off
	 */
	turn_on = (flags ^ ipif->ipif_flags) & ~IFF_CANTCHANGE;
	if (!turn_on)
		return (0);	/* No change */

	turn_off = ipif->ipif_flags & turn_on;
	turn_on ^= turn_off;
	err = 0;

	/*
	 * Sanity check for bringing an interface down.
	 * Check that nobody tries to bring down
	 * the ipif with id 0 if there are other
	 * ipif's for this ill.
	 */
	if (turn_off & IFF_UP) {
		if (ipif->ipif_id == 0 &&
		    ipif->ipif_ill->ill_ipif_up_count > 1) {
			return (EBUSY);
		}
	} else if (turn_on & IFF_UP) {
		/*
		 * Check that nobody tries to bring up the
		 * ipif with non-zero id if the ipif with
		 * zero id is not there yet.
		 */
		if (ipif->ipif_id != 0 &&
		    ipif->ipif_ill->ill_ipif_up_count == 0) {
			return (EBUSY);
		}
	}

	if (!(ipif->ipif_flags & IFF_UP) &&
	    !(turn_on & IFF_UP)) {
		/* Record new flags */
		ipif->ipif_flags |= turn_on;
		ipif->ipif_flags &= ~turn_off;
		return (0);
	}

	/*
	 * The only flag changes that we currently take specific action
	 * on is IFF_UP, IFF_DEPRECATED, IFF_NOXMIT, IFF_NOLOCAL, IFF_NOARP,
	 * IFF_NONUD, IFF_PRIVATE, and IFF_ANYCAST.
	 * This is done by bring the ipif down, changing the flags and
	 * bringing it back up again.
	 */
	if ((turn_on|turn_off) &
	    (IFF_UP|IFF_DEPRECATED|IFF_NOXMIT|IFF_NOLOCAL|IFF_NOARP|IFF_ROUTER|
	    IFF_NONUD|IFF_PRIVATE|IFF_ANYCAST)) {
		/*
		 * Taking this ipif down, make sure we have
		 * valid net and subnet bcast ire's for other
		 * logical interfaces, if we need them.
		 */
		if (!ipif->ipif_isv6)
			ipif_check_bcast_ires(ipif);

		if (((ipif->ipif_flags | turn_on) & IFF_UP) &&
		    !(turn_off & IFF_UP)) {
			need_up = 1;
			turn_on &= ~IFF_UP;
		}
		ipif_down(ipif);
	}

	/* Now we change the ipif flags */
	/* Track current value of other flags. */
	ipif->ipif_flags |= turn_on;
	ipif->ipif_flags &= ~turn_off;

	if (ipif->ipif_flags & (IFF_NOLOCAL|IFF_ANYCAST)) {
		ipif->ipif_v6src_addr = ipv6_all_zeros;
	} else {
		/*
		 * Perform autoconfig for the link-local address on :0
		 */
		if (ipif->ipif_id == 0 && ipif->ipif_isv6 &&
		    IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6lcl_addr))
			(void) ipif_setlinklocal(ipif);

		ipif->ipif_v6src_addr = ipif->ipif_v6lcl_addr;
	}

	if (need_up) {
		err = ipif_up(ipif, q, mp);
	} else {
		/*
		 * Make sure routing socket sees all changes to the flags.
		 * ipif_up_done* handles this when we use ipif_up.
		 */
		ip_rts_ifmsg(ipif);
	}
	return (err);
}

/* ARGSUSED2 */
static int
ip_sioctl_mtu(ipif_t *ipif, int mtu, queue_t *q, mblk_t *mp)
{
	int ip_min_mtu;

	/* Set interface MTU. */
	if (ipif->ipif_isv6)
		ip_min_mtu = IPV6_MIN_MTU;
	else
		ip_min_mtu = IP_MIN_MTU;

	/* Assumes that ill_max_mtu is set to ill_max_frag by default */
	if (mtu > ipif->ipif_ill->ill_max_mtu || mtu < ip_min_mtu)
		return (EINVAL);

	ipif->ipif_mtu = mtu;

	if (ipif->ipif_flags & IFF_UP) {
		if (ipif->ipif_isv6)
			ire_walk_v6(ipif_mtu_change, (char *)ipif);
		else
			ire_walk_v4(ipif_mtu_change, (char *)ipif);
	}
	return (0);
}

/* Set interface broadcast address. */
/* ARGSUSED2 */
static int
ip_sioctl_brdaddr(ipif_t *ipif, sin_t *sin, queue_t *q, mblk_t *mp)
{
	ipaddr_t addr;
	ire_t	*ire;

	if (!(ipif->ipif_flags & IFF_BROADCAST))
		return (EADDRNOTAVAIL);

	ASSERT(!(ipif->ipif_isv6));	/* No IPv6 broadcast */

	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);

	addr = sin->sin_addr.s_addr;
	if (ipif->ipif_flags & IFF_UP) {
		/*
		 * If we are already up, make sure the new
		 * broadcast address makes sense.  If it does,
		 * there should be an IRE for it already.
		 * Don't match on ipif, only on the ill
		 * since we are sharing these now.
		 */
		ire = ire_ctable_lookup(addr, 0, IRE_BROADCAST,
		    ipif, NULL, (MATCH_IRE_ILL | MATCH_IRE_TYPE));
		if (ire == NULL) {
			return (EINVAL);
		} else {
			ire_refrele(ire);
		}
	}
	/*
	 * Changing the broadcast addr for this ipif.
	 * Make sure we have valid net and subnet bcast
	 * ire's for other logical interfaces, if needed.
	 */
	if (addr != ipif->ipif_brd_addr)
		ipif_check_bcast_ires(ipif);
	IN6_IPADDR_TO_V4MAPPED(addr, &ipif->ipif_v6brd_addr);
	return (0);
}

/*
 * This routine is called by ip_sioctl_copyin_done to handle the
 * SIOCS*IFNETMASK IOCTL.
 */
static int
ip_sioctl_netmask(ipif_t *ipif, sin_t *sin, queue_t *q, mblk_t *mp)
{
	int err = 0;
	in6_addr_t v6mask;

	if (ipif->ipif_isv6) {
		sin6_t *sin6;

		if (sin->sin_family != AF_INET6)
			return (EAFNOSUPPORT);

		sin6 = (sin6_t *)sin;
		v6mask = sin6->sin6_addr;
	} else {
		ipaddr_t mask;

		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);

		mask = sin->sin_addr.s_addr;
		V4MASK_TO_V6(mask, v6mask);
	}
	/*
	 * No big deal if the interface isn't already up, or the mask
	 * isn't really changing, or this is pt-pt.
	 */
	if (!(ipif->ipif_flags & IFF_UP) ||
	    IN6_ARE_ADDR_EQUAL(&v6mask, &ipif->ipif_v6net_mask) ||
	    (ipif->ipif_flags & IFF_POINTOPOINT)) {
		ipif->ipif_v6net_mask = v6mask;
		if ((ipif->ipif_flags & IFF_POINTOPOINT) == 0) {
			V6_MASK_COPY(ipif->ipif_v6lcl_addr,
			    ipif->ipif_v6net_mask,
			    ipif->ipif_v6subnet);
		}
		return (0);
	}
	/*
	 * Make sure we have valid net and subnet broadcast ire's
	 * for the old netmask, if needed by other logical interfaces.
	 */
	if (!ipif->ipif_isv6)
		ipif_check_bcast_ires(ipif);

	ipif_down(ipif);
	ipif->ipif_v6net_mask = v6mask;
	if ((ipif->ipif_flags & IFF_POINTOPOINT) == 0) {
		V6_MASK_COPY(ipif->ipif_v6lcl_addr, ipif->ipif_v6net_mask,
		    ipif->ipif_v6subnet);
	}
	err = ipif_up(ipif, q, mp);
	if (err && err != EINPROGRESS)
		return (err);

	if (!ipif->ipif_isv6) {
		/* Potentially broadcast an address mask reply. */
		ipif_mask_reply(ipif);
	}

	return (err);
}

/*
 * Set the subnet prefix. Does not modify the broadcast address.
 */
static int
ip_sioctl_subnet(ipif_t *ipif, sin_t *sin, int addrlen, queue_t *q, mblk_t *mp)
{
	int err = 0;
	in6_addr_t v6addr;
	in6_addr_t v6mask;
	int need_up = 0;

	if (ipif->ipif_isv6) {
		sin6_t *sin6;

		if (sin->sin_family != AF_INET6)
			return (EAFNOSUPPORT);

		sin6 = (sin6_t *)sin;
		v6addr = sin6->sin6_addr;
		if (!ip_remote_addr_ok_v6(&v6addr, &ipv6_all_ones))
			return (EADDRNOTAVAIL);
	} else {
		ipaddr_t addr;

		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);

		addr = sin->sin_addr.s_addr;
		if (!ip_remote_addr_ok(addr, 0xFFFFFFFF))
			return (EADDRNOTAVAIL);
		IN6_IPADDR_TO_V4MAPPED(addr, &v6addr);
		/* Add 96 bits */
		addrlen += IPV6_ABITS - IP_ABITS;
	}

	if (addrlen > IPV6_ABITS)
		return (EINVAL);

	(void) ip_index_to_mask_v6(addrlen, &v6mask);

	/* Check if bits in the address is set past the mask */
	if (!V6_MASK_EQ(v6addr, v6mask, v6addr))
		return (EINVAL);

	if (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6subnet, &v6addr) &&
	    IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6net_mask, &v6mask))
		return (0);	/* No change */

	if (ipif->ipif_flags & IFF_UP) {
		/*
		 * If the interface is already marked up,
		 * we call ipif_down which will take care
		 * of ditching any IREs that have been set
		 * up based on the old interface address.
		 */
		ipif_down(ipif);
		need_up = 1;
	}
	/* Set the new address. */
	ipif->ipif_v6net_mask = v6mask;
	if ((ipif->ipif_flags & IFF_POINTOPOINT) == 0) {
		V6_MASK_COPY(v6addr, ipif->ipif_v6net_mask,
		    ipif->ipif_v6subnet);
	}

	if (need_up) {
		/*
		 * Now bring the interface back up.  If this
		 * is the only IPIF for the ILL, ipif_up
		 * will have to re-bind to the device, so
		 * we may get back EINPROGRESS, in which
		 * case, this IOCTL will get completed in
		 * ip_rput_dlpi when we see the DL_BIND_ACK.
		 */
		err = ipif_up(ipif, q, mp);
	}
	return (err);
}

/*
 * Set the IPv6 address token.
 */
static int
ip_sioctl_token(ipif_t *ipif, sin6_t *sin6,
    int addrlen, queue_t *q, mblk_t *mp)
{
	ill_t *ill = ipif->ipif_ill;
	int err = 0;
	in6_addr_t v6addr;
	in6_addr_t v6mask;
	int need_up = 0;
	int i;

	/* Only allow for logical unit zero i.e. not on "le0:17" */
	if (ipif->ipif_id != 0)
		return (EINVAL);

	if (!ipif->ipif_isv6)
		return (EINVAL);

	if (addrlen > IPV6_ABITS)
		return (EINVAL);

	v6addr = sin6->sin6_addr;

	/*
	 * The length of the token is the length from the end.  To get
	 * the proper mask for this, compute the mask of the bits not
	 * in the token; ie. the prefix, and then xor to get the mask.
	 */
	(void) ip_index_to_mask_v6(IPV6_ABITS - addrlen, &v6mask);
	for (i = 0; i < 4; i++) {
		v6mask.s6_addr32[i] ^= (uint32_t)0xffffffff;
	}

	if (V6_MASK_EQ(v6addr, v6mask, ill->ill_token) &&
	    ill->ill_token_length == addrlen)
		return (0);	/* No change */

	if (ipif->ipif_flags & IFF_UP) {
		ipif_down(ipif);
		need_up = 1;
	}

	V6_MASK_COPY(v6addr, v6mask, ill->ill_token);
	ill->ill_token_length = addrlen;

	if (need_up) {
		/*
		 * Now bring the interface back up.  If this
		 * is the only IPIF for the ILL, ipif_up
		 * will have to re-bind to the device, so
		 * we may get back EINPROGRESS, in which
		 * case, this IOCTL will get completed in
		 * ip_rput_dlpi when we see the DL_BIND_ACK.
		 */
		err = ipif_up(ipif, q, mp);
	}
	return (err);

}

/*
 * Set (hardware) link specific information that might override
 * what was acquired through the DL_INFO_ACK.
 */
/* ARGSUSED2 */
static int
ip_sioctl_lnkinfo(ipif_t *ipif, lif_ifinfo_req_t *lir,
    queue_t *q, mblk_t *mp)
{
	ill_t		*ill = ipif->ipif_ill;
	ipif_t		*nipif;
	int		ip_min_mtu;
	boolean_t	mtu_walk = B_FALSE;

	/* Only allow for logical unit zero i.e. not on "le0:17" */
	if (ipif->ipif_id != 0)
		return (EINVAL);

	/* Set interface MTU. */
	if (ipif->ipif_isv6)
		ip_min_mtu = IPV6_MIN_MTU;
	else
		ip_min_mtu = IP_MIN_MTU;

	/*
	 * Verify values before we set anything. Allow zero to
	 * mean unspecified.
	 */
	if (lir->lir_maxmtu != 0 &&
	    (lir->lir_maxmtu > ill->ill_max_frag ||
	    lir->lir_maxmtu < ip_min_mtu))
		return (EINVAL);
	if (lir->lir_reachtime != 0 &&
	    lir->lir_reachtime > ND_MAX_REACHTIME)
		return (EINVAL);
	if (lir->lir_reachretrans != 0 &&
	    lir->lir_reachretrans > ND_MAX_REACHRETRANSTIME)
		return (EINVAL);

	if (lir->lir_maxmtu != 0) {
		ill->ill_max_mtu = lir->lir_maxmtu;
		mtu_walk = B_TRUE;
	}

	if (lir->lir_reachtime != 0)
		ill->ill_reachable_time = lir->lir_reachtime;

	if (lir->lir_reachretrans != 0)
		ill->ill_reachable_retrans_time = lir->lir_reachretrans;

	if (lir->lir_maxhops != 0)
		ill->ill_max_hops = lir->lir_maxhops;

	ill->ill_max_buf = ND_MAX_Q;

	if (mtu_walk) {
		for (nipif = ill->ill_ipif; nipif; nipif = nipif->ipif_next) {
			if (nipif->ipif_mtu > ill->ill_max_mtu) {
				nipif->ipif_mtu = ill->ill_max_mtu;
				if (!(nipif->ipif_flags & IFF_UP))
					continue;

				if (ill->ill_isv6)
					ire_walk_ill_v6(ill, ipif_mtu_change,
					    (char *)nipif);
				else
					ire_walk_ill_v4(ill, ipif_mtu_change,
					    (char *)nipif);
			}
		}
	}
	return (0);
}

/*
 * Return best guess as to the subnet mask for the specified address.
 * Based on the subnet masks for all the configured interfaces.
 *
 * We end up returning a zero mask in the case of default, multicast or
 * experimental.
 */
static ipaddr_t
ip_subnet_mask(ipaddr_t addr)
{
	ipaddr_t net_mask, subnet_mask = 0;
	ill_t	*ill;
	ipif_t	*ipif;

	net_mask = ip_net_mask(addr);
	if (net_mask == 0)
		return (0);
	/* Let's check to see if this is maybe a local subnet route. */
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		/* this function only applies to IPv4 interfaces */
		if (ill->ill_isv6)
			continue;
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			if ((ipif->ipif_flags & IFF_UP) == 0)
				continue;
			if ((ipif->ipif_subnet & net_mask) ==
			    (addr & net_mask)) {
				/*
				 * Don't trust pt-pt interfaces if there are
				 * other interfaces.
				 */
				if (ipif->ipif_flags & IFF_POINTOPOINT) {
					subnet_mask = ipif->ipif_net_mask;
					continue;
				}
				/*
				 * Fine.  Just assume the same net mask as the
				 * directly attached subnet interface is using.
				 */
				return (ipif->ipif_net_mask);
			}
		}
	}
	if (subnet_mask != 0)
		return (subnet_mask);
	return (net_mask);
}

/*
 * ip_sioctl_copyin_setup calls ip_wput_ioctl to process any IOCTLs it
 * doesn't recognize. It is called as writer (exclusive access to IP
 * perimeter) only for ND_SET.
 */
static void
ip_wput_ioctl(queue_t *q, mblk_t *mp)
{
	IOCP	iocp;
	ipft_t	*ipft;
	ipllc_t	*ipllc;
	mblk_t	*mp1;

	iocp = (IOCP)mp->b_rptr;
	mp1 = mp->b_cont;
	if (!mp1) {
		iocp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocp->ioc_count = 0;
		qreply(q, mp);
		return;
	}
	iocp->ioc_error = 0;
	switch (iocp->ioc_cmd) {
	case IP_IOCTL:
		/*
		 * These IOCTLs provide various control capabilities to
		 * upstream agents such as ULPs and processes.	There
		 * are currently two such IOCTLs implemented.  They
		 * are used by TCP to provide update information for
		 * existing IREs and to forcibly delete an IRE for a
		 * host that is not responding, thereby forcing an
		 * attempt at a new route.
		 */
		iocp->ioc_error = EINVAL;
		if (!pullupmsg(mp1, sizeof (ipllc->ipllc_cmd)))
			break;
		ipllc = (ipllc_t *)mp1->b_rptr;
		for (ipft = ip_ioctl_ftbl; ipft->ipft_pfi; ipft++) {
			if (ipllc->ipllc_cmd == ipft->ipft_cmd)
				break;
		}
		if (ipft->ipft_pfi &&
		    ((mp1->b_wptr - mp1->b_rptr) >= ipft->ipft_min_size ||
			pullupmsg(mp1, ipft->ipft_min_size)))
			iocp->ioc_error = (*ipft->ipft_pfi)(q, mp1);
		if (ipft->ipft_flags & IPFT_F_NO_REPLY) {
			freemsg(mp);
			return;
		}
		break;
	case ND_SET:
		if (nd_getset(q, ip_g_nd, mp))
			break;
		/* FALLTHRU */
	default:
		/*
		 * Nothing unexpected should get through.
		 * ip_sioctl_copyin_setup terminates anything else.
		 * We can leave the "default" line in anyway.
		 * Note the "fallthru" above, however.	The code below
		 * is not extraneous.
		 */
		if (q->q_next) {
			putnext(q, mp);
			return;
		}
		iocp->ioc_error = ENOENT;
		mp->b_datap->db_type = M_IOCNAK;
		iocp->ioc_count = 0;
		qreply(q, mp);
		return;
	}
	mp->b_datap->db_type = M_IOCACK;
	if (iocp->ioc_error)
		iocp->ioc_count = 0;
	qreply(q, mp);
}

/*
 * Allocate and initialize a new interface control structure.  (Always
 * called as writer.)
 */
static ipif_t *
ipif_allocate(ill_t *ill, int id, uint_t ire_type)
{
static ipif_t	ipif_zero;
	ipif_t	*ipif;

	/*
	 * Crowbar dynamic allocations at the value of ip_addrs_per_if as a
	 * simple guard against runaway configuration.
	 */
	if ((uint_t)id > ip_addrs_per_if - 1)
		return (NULL);

	ipif = (ipif_t *)mi_alloc(sizeof (ipif_t), BPRI_MED);
	if (ipif == NULL) {
		return (NULL);
	}
	/* Start clean. */
	*ipif = ipif_zero;
	mutex_init(&ipif->ipif_saved_ire_lock, NULL, MUTEX_DEFAULT, NULL);
	ipif->ipif_id = id;
	ipif->ipif_flags = IFF_RUNNING;
	ipif->ipif_ire_type = ire_type;
	ipif->ipif_ill = ill;
	if (ill) {
		ipif_t	*ipif1;

		ASSERT(ill->ill_max_mtu != 0);
		ipif->ipif_mtu = ill->ill_max_mtu;
		/* Insert at tail */
		ipif->ipif_next = NULL;
		if (ill->ill_ipif) {
			for (ipif1 = ill->ill_ipif; ipif1->ipif_next;
			    ipif1 = ipif1->ipif_next)
				;
			ipif1->ipif_next = ipif;
		} else
			ill->ill_ipif = ipif;
		if (ill->ill_bcast_addr_length != 0) {
			/*
			 * Later detect lack of DLPI driver multicast
			 * capability by catching DL_ENABMULTI errors in
			 * ip_rput_dlpi.
			 */
			ipif->ipif_flags |= IFF_MULTICAST;
			if (!ipif->ipif_isv6)
				ipif->ipif_flags |= IFF_BROADCAST;
		} else {
			if (ill->ill_net_type != IRE_LOOPBACK) {
				if (ipif->ipif_isv6)
					ipif->ipif_flags |= IFF_NONUD;
				else
					ipif->ipif_flags |= IFF_NOARP;
			}
			if (ill->ill_phys_addr_length == 0) {
				/* pt-pt supports multicast. */
				if (ill->ill_net_type == IRE_LOOPBACK) {
					ipif->ipif_flags |=
					    IFF_LOOPBACK |
					    IFF_MULTICAST;
				} else {
					ipif->ipif_flags |=
					    IFF_POINTOPOINT |
					    IFF_MULTICAST;
				}
			}
		}
		if (ipif->ipif_isv6) {
			ipif->ipif_flags |= IFF_IPV6;
		} else {
			ipaddr_t inaddr_any = INADDR_ANY;

			ipif->ipif_flags |= IFF_IPV4;

			/* Keep the IN6_IS_ADDR_V4MAPPED assertions happy */
			IN6_IPADDR_TO_V4MAPPED(inaddr_any,
			    &ipif->ipif_v6lcl_addr);
			IN6_IPADDR_TO_V4MAPPED(inaddr_any,
			    &ipif->ipif_v6src_addr);
			IN6_IPADDR_TO_V4MAPPED(inaddr_any,
			    &ipif->ipif_v6subnet);
			IN6_IPADDR_TO_V4MAPPED(inaddr_any,
			    &ipif->ipif_v6net_mask);
			IN6_IPADDR_TO_V4MAPPED(inaddr_any,
			    &ipif->ipif_v6brd_addr);
			IN6_IPADDR_TO_V4MAPPED(inaddr_any,
			    &ipif->ipif_v6pp_dst_addr);
		}
	}
	return (ipif);
}

/*
 * If appropriate, send a message up to the resolver delete the entry
 * for the address of this interface which is going out of business.
 * Assumes that each message is only one message block.	 (Always called
 * as writer.)
 */
static void
ipif_arp_down(ipif_t *ipif)
{
	mblk_t	*mp;

	ip1dbg(("ipif_arp_down(%s:%u)\n",
		ipif->ipif_ill->ill_name, ipif->ipif_id));

	while ((mp = ipif->ipif_down_mp) != 0) {
		ipif->ipif_down_mp = mp->b_cont;
		mp->b_cont = NULL;
		ip1dbg(("ipif_arp_down: %s (%u) for %s:%u\n",
			dlpi_prim_str(*(int *)mp->b_rptr), *(int *)mp->b_rptr,
			ipif->ipif_ill->ill_name, ipif->ipif_id));
		putnext(ipif->ipif_ill->ill_rq, mp);
	}
}

/*
 * Get the resolver set up for a new interface address.	 (Always called
 * as writer.)
 * Honors IFF_NOARP.
 * Returns FALSE on failure.
 */
boolean_t
ipif_arp_up(ipif_t *ipif, ipaddr_t addr)
{
	mblk_t	*arp_up_mp = NULL;
	mblk_t	*arp_down_mp = NULL;
	mblk_t	*arp_add_mp = NULL;
	mblk_t	*arp_del_mp = NULL;
	mblk_t	*arp_add_mapping_mp = NULL;
	mblk_t	*arp_del_mapping_mp = NULL;
	ill_t	*ill = ipif->ipif_ill;
	int	err;

	ip1dbg(("ipif_arp_up(%s:%u) flags 0x%x\n",
		ipif->ipif_ill->ill_name, ipif->ipif_id, ipif->ipif_flags));

	/* Any work to do? */
	if (ill->ill_net_type != IRE_IF_RESOLVER || addr == INADDR_ANY)
		return (B_TRUE);


	if ((ipif->ipif_flags & IFF_UNNUMBERED) == 0) {
		/*
		 * Allocate an ARP deletion message so we know we can tell ARP
		 * when the interface goes down.
		 */
		arp_del_mp = ill_arp_alloc(ill, (uchar_t *)&ip_ared_template,
		    addr);
		if (arp_del_mp == NULL)
			goto failed;

		/* Now ask ARP to publish our address. */
		arp_add_mp = ill_arp_alloc(ill, (uchar_t *)&ip_area_template,
		    addr);
		if (arp_add_mp == NULL)
			goto failed;

		((area_t *)arp_add_mp->b_rptr)->area_flags =
		    ACE_F_PERMANENT | ACE_F_PUBLISH | ACE_F_MYADDR;

	}
	/*
	 * If there are multiple logical interfaces for the same stream
	 * (e.g. le0:1) we only add a multicast mapping for the primary one.
	 * Note: there will be no multicast info in arp if le0 is down
	 * but le0:1 is up.
	 * We also only bring arp up/down on the primary interface.
	 */

	if (ipif->ipif_id != 0)
		goto done;

	/*
	 * Allocate an ARP down message (to be saved) and an ARP up
	 * message.
	 */
	arp_down_mp = ill_arp_alloc(ill, (uchar_t *)&ip_ard_template, 0);
	if (arp_down_mp == NULL)
		goto failed;

	arp_up_mp = ill_arp_alloc(ill, (uchar_t *)&ip_aru_template, 0);
	if (arp_up_mp == NULL)
		goto failed;

	if (ipif->ipif_flags & IFF_POINTOPOINT)
		goto done;

	/*
	 * Check IFF_MULTI_BCAST and possible length of physical
	 * address != 6(?) to determine if we use the mapping or the
	 * broadcast address.
	 */
	if (ipif->ipif_flags & IFF_MULTI_BCAST) {
		ipaddr_t addr;
		arma_t	*arma;
		ipaddr_t extract_mask;
		ipaddr_t mask;
		dl_unitdata_req_t *dlur;

		/* Remove 224.0.0.0 mapping */

		/*
		 * Check that the address is not to long for the constant
		 * length reserved in the template arma_t.
		 */
		if (ill->ill_phys_addr_length > IP_MAX_HW_LEN)
			goto failed;

		addr = htonl(INADDR_ALLHOSTS_GROUP);
		/* Make sure this will not match the "exact" entry. */

		arp_del_mapping_mp = ill_arp_alloc(ill,
			(uchar_t *)&ip_ared_template, addr);
		if (arp_del_mapping_mp == NULL)
			goto failed;

		/* Add mapping mblk */
		addr = (ipaddr_t)htonl(INADDR_UNSPEC_GROUP);
		mask = (ipaddr_t)htonl(IN_CLASSD_NET);
		extract_mask = 0;

		arp_add_mapping_mp = ill_arp_alloc(ill,
			(uchar_t *)&ip_arma_multi_bcast_template, addr);
		if (arp_add_mapping_mp == NULL)
			goto failed;
		arma = (arma_t *)arp_add_mapping_mp->b_rptr;
		bcopy(&mask, (char *)arma + arma->arma_proto_mask_offset,
		    IP_ADDR_LEN);
		bcopy(&extract_mask,
		    (char *)arma + arma->arma_proto_extract_mask_offset,
		    IP_ADDR_LEN);
		/* Use the link-layer broadcast address for MULTI_BCAST */
		dlur = (dl_unitdata_req_t *)ill->ill_bcast_mp->b_rptr;
		arma->arma_hw_addr_length = ill->ill_phys_addr_length;
		if (ill->ill_sap_length < 0)
			bcopy((char *)dlur + dlur->dl_dest_addr_offset,
			    (char *)arma + arma->arma_hw_addr_offset,
			    ill->ill_phys_addr_length);
		else
			bcopy((char *)dlur + dlur->dl_dest_addr_offset
			    + ill->ill_sap_length,
			    (char *)arma + arma->arma_hw_addr_offset,
			    ill->ill_phys_addr_length);
		ip2dbg(("ipif_arp_up: adding MULTI_BCAST ARP setup for %s\n",
			ill->ill_name));
	} else if (ipif->ipif_flags & IFF_MULTICAST) {
		ipaddr_t addr;
		arma_t	*arma;
		ipaddr_t extract_mask;
		ipaddr_t mask;

		/* Remove mapping mblk */
		addr = (ipaddr_t)htonl(INADDR_ALLHOSTS_GROUP);
			/* Make sure this will not match the "exact" entry. */

		arp_del_mapping_mp = ill_arp_alloc(ill,
			(uchar_t *)&ip_ared_template, addr);
		if (arp_del_mapping_mp == NULL)
			goto failed;

		/* Add mapping mblk */
		addr = (ipaddr_t)htonl(INADDR_UNSPEC_GROUP);
		mask = (ipaddr_t)htonl(IN_CLASSD_NET);	/* 0xf0000000 */
		/* 0x007fffff */
		extract_mask = (ipaddr_t)htonl(IP_MULTI_EXTRACT_MASK);

		arp_add_mapping_mp = ill_arp_alloc(ill,
			(uchar_t *)&ip_arma_multi_template, addr);
		if (arp_add_mapping_mp == NULL)
			goto failed;
		arma = (arma_t *)arp_add_mapping_mp->b_rptr;
		bcopy(&mask, (char *)arma + arma->arma_proto_mask_offset,
		    IP_ADDR_LEN);
		bcopy(&extract_mask,
		    (char *)arma + arma->arma_proto_extract_mask_offset,
		    IP_ADDR_LEN);
		bcopy(ip_g_phys_multi_addr,
		    (char *)arma + arma->arma_hw_addr_offset,
		    ill->ill_phys_addr_length);
		ip2dbg(("ipif_arp_up: adding multicast ARP setup for %s\n",
		    ill->ill_name));
	}
done:;
	ipif_arp_down(ipif);
	ipif->ipif_down_mp = arp_down_mp;
	if (arp_del_mp != NULL) {
		arp_del_mp->b_cont = ipif->ipif_down_mp;
		ipif->ipif_down_mp = arp_del_mp;
	}
	if (arp_del_mapping_mp != NULL) {
		arp_del_mapping_mp->b_cont = ipif->ipif_down_mp;
		ipif->ipif_down_mp = arp_del_mapping_mp;
	}

	if (arp_up_mp != NULL) {
		ip1dbg(("ipif_arp_up: ARP_UP for %s:%u\n",
			ipif->ipif_ill->ill_name, ipif->ipif_id));
		putnext(ill->ill_rq, arp_up_mp);
	}
	if (arp_add_mp != NULL) {
		ip1dbg(("ipif_arp_up: ARP_ADD for %s:%u\n",
			ipif->ipif_ill->ill_name, ipif->ipif_id));
		putnext(ill->ill_rq, arp_add_mp);
	}
	if (arp_add_mapping_mp != NULL) {
		ip1dbg(("ipif_arp_up: MAPPING_ADD for %s:%u\n",
			ipif->ipif_ill->ill_name, ipif->ipif_id));
		putnext(ill->ill_rq, arp_add_mapping_mp);
	}
	if (ipif->ipif_flags & IFF_NOARP)
		err = ipif_arp_off(ipif, addr);
	else
		err = ipif_arp_on(ipif, addr);
	if (err) {
		ip0dbg(("ipif_arp_up: arp_on/off failed %d\n", err));
		freemsg(ipif->ipif_down_mp);
		freemsg(ipif->ipif_arp_on_mp);
		ipif->ipif_down_mp = NULL;
		ipif->ipif_arp_on_mp = NULL;
		return (B_FALSE);
	}
	return (B_TRUE);

failed:;
	ip1dbg(("ipif_arp_up: FAILED\n"));
	freemsg(arp_add_mp);
	freemsg(arp_del_mp);
	freemsg(arp_add_mapping_mp);
	freemsg(arp_del_mapping_mp);
	return (B_FALSE);
}

static void
ill_dl_down(ill_t *ill)
{
	/*
	 * The ill is completely out of business.
	 * Get it to unbind, and detach.
	 */
	mblk_t	*mp;

	ip1dbg(("ill_dl_down(%s)\n", ill->ill_name));
	while ((mp = ill->ill_down_mp) != NULL) {
		ill->ill_down_mp = mp->b_next;
		mp->b_next = NULL;
		ip1dbg(("ill_dl_down: %s (%u) for %s\n",
			dlpi_prim_str(*(int *)mp->b_rptr), *(int *)mp->b_rptr,
			ill->ill_name));
		ill_dlpi_send(ill, mp);
	}
}

/* Save messages for ill_dl_down */
static void
ill_dlpi_queue_down(ill_t *ill, mblk_t *mp)
{
	mblk_t **mpp;

	ip1dbg(("ill_dlpi_queue_down: %s (%u) for %s\n",
	    dlpi_prim_str(*(int *)mp->b_rptr), *(int *)mp->b_rptr,
	    ill->ill_name));
	mpp = &ill->ill_down_mp;
	while (*mpp != NULL)
		mpp = &((*mpp)->b_next);
	*mpp = mp;
}

/*
 * Send a DLPI control message to the driver but make sure there
 * is only one outstanding message. Uses ill_dlpi_pending to tell
 * when it must queue. ip_rput_dlpi_writer calls ill_dlpi_done()
 * when an ACK or a NAK is received to process the next queued message.
 */
void
ill_dlpi_send(ill_t *ill, mblk_t *mp)
{
	mblk_t **mpp;

	if (ill->ill_dlpi_pending) {
		/* Must queue message. Tail insertion */
		ip1dbg(("ill_dlpi_send: Queue %s (%u) for %s\n",
			dlpi_prim_str(*(int *)mp->b_rptr), *(int *)mp->b_rptr,
			ill->ill_name));
		mpp = &ill->ill_dlpi_deferred;
		while (*mpp != NULL)
			mpp = &((*mpp)->b_next);
		*mpp = mp;
		return;
	}
	ip1dbg(("ill_dlpi_send: Send %s (%u) for %s\n",
	    dlpi_prim_str(*(int *)mp->b_rptr), *(int *)mp->b_rptr,
	    ill->ill_name));
	ill->ill_dlpi_pending = 1;
	putnext(ill->ill_wq, mp);
}

/*
 * Called when an DLPI control message has been acked or nacked to
 * send down the next queued message (if any).
 */
void
ill_dlpi_done(ill_t *ill)
{
	mblk_t *mp;

	ip1dbg(("ill_dlpi_done(%s)\n", ill->ill_name));
	ASSERT(ill->ill_dlpi_pending);
	ill->ill_dlpi_pending = 0;
	mp = ill->ill_dlpi_deferred;
	if (mp != NULL) {
		ill->ill_dlpi_deferred = mp->b_next;
		mp->b_next = NULL;
		ip1dbg(("ill_dlpi_done: %s (%u) for %s\n",
			dlpi_prim_str(*(int *)mp->b_rptr), *(int *)mp->b_rptr,
			ill->ill_name));
		ill->ill_dlpi_pending = 1;
		putnext(ill->ill_wq, mp);
	}
}

/*
 * Take down a specific interface, but don't lose any information about it.
 * Also delete interface from its interface group (ifgrp).
 * (Always called as writer.)
 */
void
ipif_down(ipif_t *ipif)
{
	ill_t	*ill = ipif->ipif_ill;

	ip1dbg(("ipif_down(%s:%u)\n", ill->ill_name, ipif->ipif_id));

	/*
	 * For IPv4 we assume that the driver will drop all memberships
	 * (including allmulti) when we detach. We can't leave the groups
	 * here due to the asynchronous passing of the DL_DISABMULTI_REQ
	 * through arp - they might arrive in the driver after
	 * the unbind/detach.
	 * However, for IPv6 there is no asynch behavior and we need to
	 * remove the solicited node MC address and the all-nodes MC address
	 * when an ipif goes away.
	 */
	if (ipif->ipif_isv6)
		ipif_multicast_down(ipif);

	/*
	 * Delete this ipif from its interface group, possibly deleting
	 * the interface group itself.
	 *
	 * Even if ip_enable_group_ifs is false, call this function, because
	 * this ipif may have been inserted, and then ip_enable_group_ifs got
	 * turned off.  Not calling ifgrp_delete would have dire
	 * consequences if ip_enable_group_ifs was turned on again.
	 */
	ifgrp_delete(ipif);

	/*
	 * Remove from the mapping for __sin6_src_id
	 */
	if (!IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6lcl_addr) &&
	    !(ipif->ipif_flags & IFF_NOLOCAL) &&
	    (ipif->ipif_flags & IFF_UP)) {
		int err;

		err = ip_srcid_remove(&ipif->ipif_v6lcl_addr);
		if (err != 0) {
			ip0dbg(("ipif_down: srcid_remove %d\n", err));
		}
	}

	if (ipif->ipif_isv6)
		ire_walk_v6(ipif_down_delete_ire, (char *)ipif);
	else
		ire_walk_v4(ipif_down_delete_ire, (char *)ipif);

	if (ipif->ipif_flags & IFF_UP) {
		ipif->ipif_flags &= ~IFF_UP;
		--ill->ill_ipif_up_count;
		/*
		 * Skip any loopback interface (null wq).
		 * If this is the last logical interface on the ill
		 * have ill_dl_down tell the driver we are gone (unbind and
		 * detach). Note that lun 0 can ipif_down even though
		 * there are other logical units that are up.
		 * This occurs e.g. when we change a "significant" IFF_ flag.
		 */
		if (ipif->ipif_ill->ill_wq != NULL) {
			ipif_g_count--;
			if (ill->ill_ipif_up_count == 0) {
				ill_dl_down(ill);
			}
		}
	}
	/*
	 * Update any other ipifs which have used "our" local address as
	 * a source address. This entails removing and recreating IRE_INTERFACE
	 * entries for such ipifs.
	 */
	if (ipif->ipif_isv6)
		ipif_update_other_ipifs_v6(ipif);
	else
		ipif_update_other_ipifs(ipif);

	/*
	 * Have to be after removing the routes in ipif_down_delete_ire.
	 */
	if (ipif->ipif_isv6)
		ipif_ndp_down(ipif);
	else
		ipif_arp_down(ipif);
	ip_rts_ifmsg(ipif);
	ip_rts_newaddrmsg(RTM_DELETE, 0, ipif);
}

/*
 * ire_walk routine to delete every IRE dependent on the interface
 * address that is going down.	(Always called as writer.)
 * Works for both v4 and v6.
 * In addition for checking for ire_ipif matches it also checks for
 * IRE_CACHE entries which have the same source address as the
 * disappearing ipif since ipif_select_source might have picked
 * that source. Note that ipif_down/ipif_update_other_ipifs takes
 * care of any IRE_INTERFACE with the disappearing source address.
 */
static void
ipif_down_delete_ire(ire_t *ire, char *ipif_arg)
{
	ipif_t	*ipif = (ipif_t *)ipif_arg;

	if (ire->ire_ipif == NULL ||
	    ire->ire_ipif->ipif_ill != ipif->ipif_ill)
		return;

	if (ire->ire_ipif != ipif) {
		/*
		 * Look for matching source address.
		 */
		if (ire->ire_type != IRE_CACHE)
			return;
		if (ipif->ipif_flags & IFF_NOLOCAL)
			return;

		if (ire->ire_ipversion == IPV4_VERSION) {
			if (ire->ire_src_addr != ipif->ipif_src_addr)
				return;
		} else {
			if (!IN6_ARE_ADDR_EQUAL(&ire->ire_src_addr_v6,
			    &ipif->ipif_v6lcl_addr))
				return;
		}
		ire_delete(ire);
		return;
	}
	/*
	 * ire_delete() will do an ire_flush_cache which will delete
	 * all ire_ipif matches
	 */
	ire_delete(ire);
}

/*
 * ire_walk_ill function for deleting all IRE_CACHE entries for an ill when
 * 1) an ipif (on that ill) changes the IFF_DEPRECATED flags, or
 * 2) when an interface is brought up or down (on that ill).
 * This ensures that the IRE_CACHE entries don't retain stale source
 * address selection results.
 */
void
ill_cache_delete(ire_t *ire, char *ill_arg)
{
	ill_t	*ill = (ill_t *)ill_arg;

	if (ire->ire_type != IRE_CACHE)
		return;
	ASSERT(ire->ire_ipif != NULL && ire->ire_ipif->ipif_ill == ill);

	ire_delete(ire);
}

/* Deallocate an IPIF.	(Always called as writer.) */
static void
ipif_free(ipif_t *ipif)
{
	ipif_t	**ipifp;
	mblk_t *mp;

	/*
	 * Free state for addition IRE_IF_[NO]RESOLVER ire's.
	 * Currently, we are called as writer and hence we don't
	 * need to hold locks. This is just to be consitent with
	 * other accesses to ipif_saved_ire_mp.
	 */
	mutex_enter(&ipif->ipif_saved_ire_lock);
	mp = ipif->ipif_saved_ire_mp;
	ipif->ipif_saved_ire_mp = NULL;
	mutex_exit(&ipif->ipif_saved_ire_lock);
	freemsg(mp);

	freemsg(ipif->ipif_arp_on_mp);
	ipif->ipif_arp_on_mp = NULL;

	/* Remove ipc references */
	reset_ipc_ipif(ipif);

	/*
	 * Make sure we have valid net and subnet broadcast ire's for the
	 * other ipif's which share them with this ipif.
	 */
	if (!ipif->ipif_isv6)
		ipif_check_bcast_ires(ipif);

	/* Take down the interface. */
	ipif_down(ipif);

	/*
	 * Remove all multicast memberships on the interface now.
	 * The IPv6 multicast group memberships were already disabled
	 * through ipif_down() and deleted if ilm_refcnt = 0.  This is the
	 * reason why we call ilm_free() after ipif_down() which calls
	 * ipif_multicast_down().
	 */
	ilm_free(ipif);

	/* Remove pointers to this ill in the multicast routing tables */
	reset_mrt_vif_ipif(ipif);

	/* Get it out of the ILL interface list. */
	ipifp = &ipif->ipif_ill->ill_ipif;
	for (; *ipifp != NULL; ipifp = &ipifp[0]->ipif_next) {
		if (*ipifp == ipif) {
			*ipifp = ipif->ipif_next;
			break;
		}
	}
	mutex_destroy(&ipif->ipif_saved_ire_lock);
	/* Free the memory. */
	mi_free((char *)ipif);
}

/*
 * Returns an ipif name in the form "ill_name/unit" if ipif_id is not zero,
 * "ill_name" otherwise.
 */
char *
ipif_get_name(ipif_t *ipif, char *buf, int len)
{
	char	lbuf[32];
	char	*name;
	size_t	name_len;

	buf[0] = '\0';
	if (!ipif)
		return (buf);
	name = ipif->ipif_ill->ill_name;
	name_len = ipif->ipif_ill->ill_name_length;
	if (ipif->ipif_id != 0) {
		(void) sprintf(lbuf, "%s%c%d", name, IPIF_SEPARATOR_CHAR,
		    ipif->ipif_id);
		name = lbuf;
		name_len = mi_strlen(name) + 1;
	}
	len -= 1;
	buf[len] = '\0';
	len = MIN(len, name_len);
	bcopy(name, buf, len);
	return (buf);
}

/*
 * Find an IPIF based on the name passed in.  Names can be of the
 * form <phys> (e.g., le0), <phys>:<#> (e.g., le0:1),
 * The <phys> string can have forms like <dev><#> (e.g., le0),
 * <dev><#>.<module> (e.g. le0.foo), or <dev>.<module><#> (e.g. ip.tun3).
 * When there is no colon, the implied unit id is zero. <phys> must
 * correspond to the name of an ILL.  (May be called as writer.)
 */
ipif_t *
ipif_lookup_on_name(char *name, size_t namelen, boolean_t do_alloc,
    boolean_t *exists, boolean_t isv6)
{
	char	*cp;
	char	*endp;
	int	id;
	ill_t	*ill;
	ipif_t	*ipif;
	uint_t	ire_type;

	*exists = 0;
	/* Look for a colon in the name. */
	endp = &name[namelen - 1];
	for (cp = endp; --cp > name; ) {
		if (*cp == IPIF_SEPARATOR_CHAR)
			break;
	}
	if (cp <= name) {
		cp = endp;
	} else {
		*cp = '\0';
	}

	/*
	 * Look up the ILL, based on the portion of the name
	 * before the slash.
	 */
	ill = ill_lookup_on_name(name, (cp + 1) - name, do_alloc, isv6);
	if (cp != endp)
		*cp = IPIF_SEPARATOR_CHAR;
	if (!ill)
		return (NULL);

	/* Establish the unit number in the name. */
	id = 0;
	if (cp < endp && *endp == '\0') {
		/* If there was a colon, the unit number follows. */
		cp++;
		id = (int)mi_strtol(cp, &endp, 0);
		if (endp == cp)
			return (NULL);
	}

	/* Now see if there is an IPIF with this unit number. */
	for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
		if (ipif->ipif_id == id) {
			*exists = 1;
			return (ipif);
		}
	}

	/*
	 * If none found and allocation not requested, return NULL
	 */
	if (!do_alloc)
		return (NULL);
	/*
	 * If none found, allocate and return a new one.
	 * Use IRE_LOOPBACK only for lun 0 to support "receive only" use of
	 * lo0:1 etc.
	 */
	if (ill->ill_net_type == IRE_LOOPBACK && id == 0)
		ire_type = IRE_LOOPBACK;
	else
		ire_type = IRE_LOCAL;
	return (ipif_allocate(ill, id, ire_type));
}

/*
 * This routine is called whenever a new address comes up on an ipif.  If
 * we are configured to respond to address mask requests, then we are supposed
 * to broadcast an address mask reply at this time.  This routine is also
 * called if we are already up, but a netmask change is made.  This is legal
 * but might not make the system manager very popular.	(May be called
 * as writer.)
 */
static void
ipif_mask_reply(ipif_t *ipif)
{
	icmph_t	*icmph;
	ipha_t	*ipha;
	mblk_t	*mp;

#define	REPLY_LEN	(sizeof (icmp_ipha) + sizeof (icmph_t) + IP_ADDR_LEN)

	if (!ip_respond_to_address_mask_broadcast)
		return;

	/* ICMP mask reply is IPv4 only */
	ASSERT(!ipif->ipif_isv6);

	mp = allocb(REPLY_LEN, BPRI_HI);
	if (!mp)
		return;
	mp->b_wptr = mp->b_rptr + REPLY_LEN;

	ipha = (ipha_t *)mp->b_rptr;
	bzero(ipha, REPLY_LEN);
	*ipha = icmp_ipha;
	ipha->ipha_ttl = ip_broadcast_ttl;
	ipha->ipha_src = ipif->ipif_src_addr;
	ipha->ipha_dst = ipif->ipif_brd_addr;
	ipha->ipha_length = htons(REPLY_LEN);
	ipha->ipha_ident = 0;

	icmph = (icmph_t *)&ipha[1];
	icmph->icmph_type = ICMP_ADDRESS_MASK_REPLY;
	bcopy(&ipif->ipif_net_mask, &icmph[1], IP_ADDR_LEN);
	icmph->icmph_checksum = IP_CSUM(mp, sizeof (ipha_t), 0);

	put(ipif->ipif_wq, mp);

#undef	REPLY_LEN
}

/*
 * When the mtu in the ipif changes, we call this routine through ire_walk
 * to update all the relevant IREs.
 * Skip IRE_LOCAL and "loopback" IRE_BROADCAST by checking ire_stq.
 */
static void
ipif_mtu_change(ire_t *ire, char *ipif_arg)
{
	ipif_t	*ipif = (ipif_t *)ipif_arg;

	if (ire->ire_stq == NULL || ire->ire_ipif != ipif)
		return;
	ire->ire_max_frag = ipif->ipif_mtu;
}

/*
 * Join the ipif specific multicast groups.
 * Must be called after a mapping has been set up in the resolver.  (Always
 * called as writer.)
 */
void
ipif_multicast_up(ipif_t *ipif)
{
	int err;

	ip1dbg(("ipif_multicast_up\n"));
	if (!(ipif->ipif_flags & IFF_MULTICAST) || ipif->ipif_multicast_up)
		return;

	if (ipif->ipif_isv6) {
		if (IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6lcl_addr))
			return;

		/* Join the all hosts multicast address */
		ip1dbg(("ipif_multicast_up - addmulti\n"));
		err = ip_addmulti_v6(&ipv6_all_hosts_mcast, ipif);
		if (err) {
			ip0dbg(("ipif_multicast_up: "
			    "all_hosts_mcast failed %d\n",
			    err));
			return;
		}
		/*
		 * Enable multicast for the solicited node multicast address
		 */
		if (!(ipif->ipif_flags & IFF_NOLOCAL)) {
			in6_addr_t ipv6_multi = ipv6_solicited_node_mcast;

			ipv6_multi.s6_addr32[3] |=
			    ipif->ipif_v6lcl_addr.s6_addr32[3];

			err = ip_addmulti_v6(&ipv6_multi, ipif);
			if (err) {
				ip0dbg(("ipif_multicast_up: sol MC failed %d\n",
				    err));
				return;
			}
		}
	} else {
		if (ipif->ipif_lcl_addr == INADDR_ANY)
			return;

		/* Join the all hosts multicast address */
		ip1dbg(("ipif_multicast_up - addmulti\n"));
		err = ip_addmulti(htonl(INADDR_ALLHOSTS_GROUP), ipif);
		if (err) {
			ip0dbg(("ipif_multicast_up: failed %d\n", err));
			return;
		}
	}
	ipif->ipif_multicast_up = 1;
}

/*
 * Leave the ipif specific multicast groups.
 *
 * For IPv4 we assume that the driver will drop all memberships
 * (including allmulti) when we detach. We can't leave the groups
 * here due to the asynchronous passing of the DL_DISABMULTI_REQ
 * through arp - they might arrive in the driver after
 * the unbind/detach.
 * However, for IPv6 there is no asynch behavior and we need to
 * remove the solicited node MC address and the all-nodes MC address
 * when an ipif goes away.
 */
void
ipif_multicast_down(ipif_t *ipif)
{
	int err;

	ip1dbg(("ipif_multicast_down\n"));
	if (!ipif->ipif_multicast_up)
		return;

	if (ipif->ipif_isv6) {
		ip1dbg(("ipif_multicast_down - delmulti\n"));
		/* Leave the all hosts multicast address */
		err = ip_delmulti_v6(&ipv6_all_hosts_mcast, ipif);
		if (err) {
			ip0dbg(("ipif_multicast_down: "
			    "all_hosts_mcast failed %d\n",
			    err));
		}
		/*
		 * Disable multicast for the solicited node multicast address
		 */
		if (!(ipif->ipif_flags & IFF_NOLOCAL)) {
			in6_addr_t ipv6_multi = ipv6_solicited_node_mcast;

			ipv6_multi.s6_addr32[3] |=
			    ipif->ipif_v6lcl_addr.s6_addr32[3];

			err = ip_delmulti_v6(&ipv6_multi, ipif);
			if (err) {
				ip0dbg(("ipif_multicast_down: "
				    "sol MC failed %d\n",
				    err));
			}
		}
	} else {
		/* Leave the all hosts multicast address */
		ip1dbg(("ipif_multicast_down - delmulti\n"));
		err = ip_delmulti(htonl(INADDR_ALLHOSTS_GROUP), ipif);
		if (err) {
			ip0dbg(("ipif_multicast_down: delmulti failed %d\n",
			    err));
		}
	}
	ipif->ipif_multicast_up = 0;
}

/*
 * Used when an interface comes up to recreate any extra routes on this
 * interface.
 */
void
ipif_recover_ire(ipif_t *ipif)
{
	mblk_t	*mp;

	ip1dbg(("ipif_recover_ire(%s:%u)", ipif->ipif_ill->ill_name,
	    ipif->ipif_id));

	ASSERT(!ipif->ipif_isv6);

	mutex_enter(&ipif->ipif_saved_ire_lock);
	for (mp = ipif->ipif_saved_ire_mp; mp != NULL; mp = mp->b_cont) {
		ire_t		*ire;
		queue_t		*stq;
		ifrt_t		*ifrt;
		uchar_t		*src_addr;
		uchar_t		*gateway_addr;
		mblk_t		*resolver_mp;
		ushort_t	type;

		/*
		 * When the ire was initially created and then added in
		 * ip_rt_add(), it was created either using ipif->ipif_net_type
		 * in the case of a traditional interface route, or as one of
		 * the IRE_OFFSUBNET types (with the exception of
		 * IRE_HOST_REDIRECT which is created by icmp_redirect() and
		 * which we don't need to save or recover).  In the case where
		 * ipif->ipif_net_type was IRE_LOOPBACK, ip_rt_add() will update
		 * the ire_type to IRE_IF_NORESOLVER before calling ire_add()
		 * to satisfy software like GateD and Sun Cluster which creates
		 * routes using the the loopback interface's address as a
		 * gateway.
		 *
		 * As ifrt->ifrt_type reflects the already updated ire_type and
		 * since ire_create() expects that IRE_IF_NORESOLVER will have
		 * a valid ire_dlureq_mp field (which doesn't make sense for a
		 * IRE_LOOPBACK), ire_create() will be called in the same way
		 * here as in ip_rt_add(), namely using ipif->ipif_net_type when
		 * the route looks like a traditional interface route (where
		 * ifrt->ifrt_type & IRE_INTERFACE is true) and otherwise using
		 * the saved ifrt->ifrt_type.  This means that in the case where
		 * ipif->ipif_net_type is IRE_LOOPBACK, the ire created by
		 * ire_create() will be an IRE_LOOPBACK, it will then be turned
		 * into an IRE_IF_NORESOLVER and then added by ire_add().
		 */
		ifrt = (ifrt_t *)mp->b_rptr;
		if (ifrt->ifrt_type & IRE_INTERFACE) {
			stq = (ipif->ipif_net_type == IRE_IF_RESOLVER)
			    ? ipif->ipif_rq : ipif->ipif_wq;
			src_addr = (uint8_t *)&ipif->ipif_src_addr;
			gateway_addr = NULL;
			resolver_mp = ipif->ipif_resolver_mp;
			type = ipif->ipif_net_type;
		} else {
			stq = NULL;
			src_addr = NULL;
			gateway_addr = (uint8_t *)&ifrt->ifrt_gateway_addr;
			resolver_mp = NULL;
			type = ifrt->ifrt_type;
		}

		/*
		 * Create a copy of the IRE with the saved address and netmask.
		 */
		ip1dbg(("ipif_recover_ire: creating IRE %s (%d) for "
		    "0x%x/0x%x\n",
		    ip_nv_lookup(ire_nv_tbl, ifrt->ifrt_type), ifrt->ifrt_type,
		    ntohl(ifrt->ifrt_addr),
		    ntohl(ifrt->ifrt_mask)));
		ire = ire_create(
		    (uint8_t *)&ifrt->ifrt_addr,
		    (uint8_t *)&ifrt->ifrt_mask,
		    src_addr,
		    gateway_addr,
		    ifrt->ifrt_max_frag,
		    NULL,
		    NULL,
		    stq,
		    type,
		    resolver_mp,
		    ipif,
		    0,
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
 * Used to set the netmask and broadcast address to default values when the
 * interface is brought up.  (Always called as writer.)
 */
static void
ipif_set_default(ipif_t *ipif)
{
	if (!ipif->ipif_isv6) {
		/*
		 * Interface holds an IPv4 address. Default
		 * mask is the natural netmask.
		 */
		if (!ipif->ipif_net_mask) {
			ipaddr_t	v4mask;

			v4mask = ip_net_mask(ipif->ipif_lcl_addr);
			V4MASK_TO_V6(v4mask, ipif->ipif_v6net_mask);
		}
		if (ipif->ipif_flags & IFF_POINTOPOINT) {
			/* ipif_subnet is ipif_pp_dst_addr for pt-pt */
			ipif->ipif_v6subnet = ipif->ipif_v6pp_dst_addr;
		} else {
			V6_MASK_COPY(ipif->ipif_v6lcl_addr,
			    ipif->ipif_v6net_mask, ipif->ipif_v6subnet);
		}
		/*
		 * NOTE: SunOS 4.X does this even if the broadcast address
		 * has been already set thus we do the same here.
		 */
		if (ipif->ipif_flags & IFF_BROADCAST) {
			ipaddr_t	v4addr;

			v4addr = ipif->ipif_subnet | ~ipif->ipif_net_mask;
			IN6_IPADDR_TO_V4MAPPED(v4addr, &ipif->ipif_v6brd_addr);
		}
	} else {
		/*
		 * Interface holds an IPv6-only address.  Default
		 * mask is all-ones.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ipif->ipif_v6net_mask))
			ipif->ipif_v6net_mask = ipv6_all_ones;
		if (ipif->ipif_flags & IFF_POINTOPOINT) {
			/* ipif_subnet is ipif_pp_dst_addr for pt-pt */
			ipif->ipif_v6subnet = ipif->ipif_v6pp_dst_addr;
		} else {
			V6_MASK_COPY(ipif->ipif_v6lcl_addr,
			    ipif->ipif_v6net_mask, ipif->ipif_v6subnet);
		}
	}
}

/*
 * Return true if this address can be used as local address
 * without causing duplicate address problems.
 * Special checks are needed to allow the same IPv6 link-local address
 * on different ills.
 * TODO: allowing the same site-local address on different ill's.
 */
static boolean_t
ip_addr_availability_check(ipif_t *new_ipif)
{
	in6_addr_t	our_v6addr;
	ill_t *ill;
	ipif_t *ipif;

	new_ipif->ipif_flags &= ~IFF_UNNUMBERED;
	if (IN6_IS_ADDR_UNSPECIFIED(&new_ipif->ipif_v6lcl_addr) ||
	    IN6_IS_ADDR_V4MAPPED_ANY(&new_ipif->ipif_v6lcl_addr))
		return (B_TRUE);

	our_v6addr = new_ipif->ipif_v6lcl_addr;

	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		if (ill->ill_isv6 != new_ipif->ipif_isv6)
			continue;
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			if ((ipif == new_ipif) ||
			    ((ipif->ipif_flags & IFF_UP) == 0) ||
			    (ipif->ipif_flags & IFF_UNNUMBERED))
				continue;
			if (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6lcl_addr,
			    &our_v6addr)) {
				if (new_ipif->ipif_flags & IFF_POINTOPOINT)
					new_ipif->ipif_flags |= IFF_UNNUMBERED;
				else if (ipif->ipif_flags & IFF_POINTOPOINT)
					ipif->ipif_flags |= IFF_UNNUMBERED;
				else if (IN6_IS_ADDR_LINKLOCAL(&our_v6addr) &&
				    new_ipif->ipif_ill != ill)
					continue;
				else if (IN6_IS_ADDR_SITELOCAL(&our_v6addr) &&
				    new_ipif->ipif_ill != ill)
					continue;
				else
					return (B_FALSE);
			}
		}
	}
	return (B_TRUE);
}

/*
 * Bring up an ipif: bring up arp/ndp, bring up the DLPI stream, and add
 * IREs for the ipif.
 * When the routine returns EINPROGRESS then mp has been consumed and
 * the ioctl will be acked from ip_rput_dlpi.
 */
int
ipif_up(ipif_t *ipif, queue_t *q, mblk_t *mp)
{
	ill_t	*ill = ipif->ipif_ill;
	boolean_t isv6 = ipif->ipif_isv6;
	int	err = 0;

	ip1dbg(("ipif_up(%s:%u)\n",
		ipif->ipif_ill->ill_name, ipif->ipif_id));

	/* Shouldn't get here if it is already up. */
	if (ipif->ipif_flags & IFF_UP)
		return (EALREADY);

	/* Check if this address can be used on this interface  */
	if (!ip_addr_availability_check(ipif))
		return (EADDRNOTAVAIL);

	/* Skip arp/ndp for any loopback interface. */
	if (ipif->ipif_ill->ill_wq == NULL) {
		if (isv6)
			return (ipif_up_done_v6(ipif));
		else
			return (ipif_up_done(ipif));
	}

	if (ipif->ipif_ipif_up_count > 0 &&
	    !(ipif->ipif_flags & IFF_NOLOCAL)) {
		if (!isv6) {
			/*
			 * Crank up ARP on the new address.
			 * Inform ARP about the IFF_NOARP setting.
			 */
			if (!ipif_arp_up(ipif, ipif->ipif_lcl_addr))
				return (ENOMEM);
		} else {
			/*
			 * Crank up IPv6 neighbor discovery
			 * Unlike ARP, this should complete when
			 * ipif_ndp_up returns.
			 */
			if ((err = ipif_ndp_up(ipif,
				&ipif->ipif_v6lcl_addr)) != 0)
				return (err);
		}
	}
	/*
	 * If this is the first logical interface on the ill to become "up"
	 * have ill_dl_up tell the driver to get going (attach and bind)
	 * Note that lun 0 can ipif_down even though
	 * there are other logical units that are up.
	 * This occurs e.g. when we change a "significant" IFF_ flag.
	 */
	if (ill->ill_ipif_up_count != 0) {
		if (isv6)
			return (ipif_up_done_v6(ipif));
		else
			return (ipif_up_done(ipif));
	}

	return (ill_dl_up(ill, ipif, mp, q));
}

/*
 * Perform an attach and bind for the physical device.
 * When the routine returns EINPROGRESS then mp has been consumed and
 * the ioctl will be acked from ip_rput_dlpi.
 * Allocate an unbind and detach message and save them until ipif_down.
 */
static int
ill_dl_up(ill_t *ill, ipif_t *ipif, mblk_t *mp, queue_t *q)
{
	mblk_t	*bind_mp = NULL;
	mblk_t	*unbind_mp = NULL;
	mblk_t	*attach_mp = NULL;
	mblk_t	*detach_mp = NULL;
	ipc_t	*ipc;

	ip1dbg(("ill_dl_up(%s)\n", ill->ill_name));

	ASSERT(mp != NULL);
	if (ill->ill_pending_mp != NULL)
		return (EAGAIN);

	/* Create a resolver cookie for ARP */
	if (!ill->ill_isv6 && ill->ill_net_type == IRE_IF_RESOLVER) {
		mblk_t		*areq_mp;
		areq_t		*areq;
		uint16_t	sap_addr;

		areq_mp = ill_arp_alloc(ill,
			(uchar_t *)&ip_areq_template, 0);
		if (areq_mp == NULL) {
			return (ENOMEM);
		}
		freemsg(ill->ill_resolver_mp);
		ill->ill_resolver_mp = areq_mp;
		areq = (areq_t *)areq_mp->b_rptr;
		sap_addr = ill->ill_sap;
		bcopy(&sap_addr, areq->areq_sap, sizeof (sap_addr));
	}
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
	 * Record state needed to complete this operation when the
	 * DL_BIND_ACK shows up.  Also remember the pre-allocated mblks.
	 */
	ASSERT(ill->ill_pending_mp == NULL && ill->ill_pending_ipif == NULL);
	ill->ill_pending_ipif = ipif;
	ill->ill_pending_mp = mp;
	ASSERT(WR(q)->q_next == NULL);
	ipc = q->q_ptr;
	ASSERT(ill->ill_pending_q == NULL && ipc->ipc_pending_ill == NULL);
	ill->ill_pending_q = q;
	ipc->ipc_pending_ill = ill;
	/*
	 * Save detach and unbind messages for ill_dl_down.
	 * They will be consumed when the interface goes down.
	 */
	ill_dlpi_queue_down(ill, unbind_mp);
	if (detach_mp != NULL)
		ill_dlpi_queue_down(ill, detach_mp);

	if (attach_mp != NULL)
		ill_dlpi_send(ill, attach_mp);
	ill_dlpi_send(ill, bind_mp);

	/*
	 * The attach is on its way.  We don't wait for
	 * it to come back.  If it fails, the bind will too,
	 * and we will note that in ip_rput_dlpi.
	 */
	if (ill->ill_ick.ick_magic == 0 &&
	    !ill->ill_wq->q_next->q_next && dohwcksum) {
		/*
		 * We had to wait til after device attach and
		 * we've not done a hardware inetcksum probe
		 * and the next module is a driver, so	send
		 * down the inetcksum probe to the driver.
		 * The M_CTL mblk will either be freemsg()ed
		 * by the driver if it doesn't know about
		 * inetcksum probes or turned-around with the
		 * negotiated state which will be processed
		 * by ip_rput().
		 */
		inetcksum_t *ick;
		mblk_t	*ick_mp;

		ick_mp = allocb(sizeof (inetcksum_t), BPRI_HI);
		if (ick_mp) {
			ick = (inetcksum_t *)ick_mp->b_rptr;
			ick_mp->b_datap->db_type = M_CTL;
			ick_mp->b_wptr = (uchar_t *)&ick[1];
			ick->ick_magic = ICK_M_CTL_MAGIC;
			if (ill->ill_isv6) {
				ick->ick_split = IPV6_HDR_LEN +
				    TCP_MIN_HEADER_LENGTH;
			} else {
				ick->ick_split = IP_SIMPLE_HDR_LENGTH +
				    TCP_MIN_HEADER_LENGTH;
			}
			ick->ick_split_align = (int)ptob(1);
			ick->ick_xmit = 0;
			ill->ill_ick.ick_magic = ~ICK_M_CTL_MAGIC;
			ip1dbg(("ill_dl_up: checksum\n"));
			putnext(ill->ill_wq, ick_mp);
		} else {
			/* ??? not enough resources so punt. */
			ip0dbg(("no resources for ick probe of %s\n",
			    ill->ill_name));
		}
	}
	/*
	 * This operation will complete in ip_rput_dlpi with either
	 * a DL_BIND_ACK or DL_ERROR_ACK.
	 */
	return (EINPROGRESS);
bad:
	ip1dbg(("ill_dl_up(%s) FAILED\n", ill->ill_name));
	/* Check here for possible removal from ifgrp. */
	if (ipif->ipif_ifgrpschednext != NULL)
	    ifgrp_delete(ipif);

	if (bind_mp != NULL)
		freemsg(bind_mp);
	if (unbind_mp != NULL)
		freemsg(unbind_mp);
	if (attach_mp != NULL)
		freemsg(attach_mp);
	if (detach_mp != NULL)
		freemsg(detach_mp);
	return (ENOMEM);
}

/*
 * DLPI and ARP is up.
 * Create all the IREs associated with an interface bring up multicast.
 * Set the interface flag and finish other initialization
 * that potentially had to be differed to after DL_BIND_ACK.
 */
int
ipif_up_done(ipif_t *ipif)
{
	ire_t	*ire_array[20];
	ire_t	**irep = ire_array;
	ire_t	**irep1;
	ipaddr_t net_mask = 0;
	ipaddr_t subnet_mask, route_mask;
	ill_t	*ill = ipif->ipif_ill;
	queue_t	*stq;
	ipif_t	 *src_ipif;
	int	err;

	ASSERT(!ipif->ipif_isv6);
	/*
	 * Make sure ip_forwarding is set if this interface is a router.
	 * XXX per-interface ip_forwarding.
	 */
	if ((ipif->ipif_flags & IFF_ROUTER) && !ip_g_forward) {
		ip1dbg(("ipif_up_done: setting ip_forwarding\n"));
		ip_g_forward = 1;
	}

	/*
	 * Remove any IRE_CACHE entries for this ill to make
	 * sure source address selection gets to take this new ipif
	 * into account.
	 */
	ire_walk_ill_v4(ill, ill_cache_delete, (char *)ipif->ipif_ill);

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
		src_ipif = ipif_select_source(ipif->ipif_ill,
		    ipif->ipif_subnet);
		if (src_ipif == NULL)
			src_ipif = ipif;	/* Last resort */
	} else {
		src_ipif = ipif;
	}

	/* Create all the IREs associated with this interface */
	if (ipif->ipif_lcl_addr && !(ipif->ipif_flags & IFF_NOLOCAL)) {
		/* Register the source address for __sin6_src_id */
		err = ip_srcid_insert(&ipif->ipif_v6lcl_addr);
		if (err != 0) {
			ip0dbg(("ipif_up_done: srcid_insert %d\n", err));
			return (err);
		}
		/* If the interface address is set, create the local IRE. */
		ip1dbg(("ipif_up_done: 0x%p creating IRE 0x%x for 0x%x\n",
			(void *)ipif,
			ipif->ipif_ire_type,
			ntohl(ipif->ipif_lcl_addr)));
		*irep++ = ire_create(
		    (uchar_t *)&ipif->ipif_lcl_addr,	/* dest address */
		    (uchar_t *)&ip_g_all_ones,		/* mask */
		    (uchar_t *)&src_ipif->ipif_src_addr, /* source address */
		    NULL,				/* no gateway */
		    IP_LOOPBACK_MTU + IP_SIMPLE_HDR_LENGTH + 20,
							/* max frag size */
		    NULL,
		    ipif->ipif_rq,			/* recv-from queue */
		    NULL,				/* no send-to queue */
		    ipif->ipif_ire_type,		/* LOCAL or LOOPBACK */
		    NULL,
		    ipif,
		    0,
		    0,
		    0,
		    (ipif->ipif_flags & IFF_PRIVATE) ? RTF_PRIVATE : 0,
		    &ire_uinfo_null);
	} else {
		ip1dbg((
		    "ipif_up_done: not creating IRE %d for 0x%x: flags 0x%x\n",
		    ipif->ipif_ire_type,
		    ntohl(ipif->ipif_lcl_addr),
		    ipif->ipif_flags));
	}
	if (ipif->ipif_lcl_addr && !(ipif->ipif_flags & IFF_NOLOCAL))
		net_mask = ip_net_mask(ipif->ipif_lcl_addr);
	else
		net_mask = htonl(IN_CLASSA_NET);	/* fallback */

	subnet_mask = ipif->ipif_net_mask;

	/*
	 * If mask was not specified, use natural netmask of
	 * interface address. Also, store this mask back into the
	 * ipif struct.
	 */
	if (subnet_mask == 0) {
		subnet_mask = net_mask;
		V4MASK_TO_V6(subnet_mask, ipif->ipif_v6net_mask);
		V6_MASK_COPY(ipif->ipif_v6lcl_addr, ipif->ipif_v6net_mask,
		    ipif->ipif_v6subnet);
	}

	/* Set up the IRE_IF_RESOLVER or IRE_IF_NORESOLVER, as appropriate. */
	if (stq && !(ipif->ipif_flags & IFF_NOXMIT) && ipif->ipif_subnet) {
		/* ipif_subnet is ipif_pp_dst_addr for pt-pt */

		if (ipif->ipif_flags & IFF_POINTOPOINT) {
			route_mask = IP_HOST_MASK;
		} else {
			route_mask = subnet_mask;
		}

		ip1dbg(("ipif_up_done: ipif 0x%p ill 0x%p "
		    "creating if IRE ill_net_type 0x%x for 0x%x\n",
			(void *)ipif, (void *)ill,
			ill->ill_net_type,
			ntohl(ipif->ipif_subnet)));
		*irep++ = ire_create(
		    (uchar_t *)&ipif->ipif_subnet,	/* dest address */
		    (uchar_t *)&route_mask,		/* mask */
		    (uchar_t *)&src_ipif->ipif_src_addr, /* src addr */
		    NULL,				/* no gateway */
		    ipif->ipif_mtu,			/* max frag */
		    NULL,
		    NULL,				/* no recv queue */
		    stq,				/* send-to queue */
		    ill->ill_net_type,			/* IF_[NO]RESOLVER */
		    ill->ill_resolver_mp,		/* xmit header */
		    ipif,
		    0,
		    0,
		    0,
		    (ipif->ipif_flags & IFF_PRIVATE) ? RTF_PRIVATE : 0,
		    &ire_uinfo_null);
	}

	/*
	 * If the interface address is set, create the broadcast IREs.
	 *
	 * ire_create_bcast checks if the proposed new IRE matches
	 * any existing IRE's with the same physical interface (ILL).
	 * This should get rid of duplicates.
	 * ire_create_bcast also check IFF_NOXMIT and if set only appends
	 * the "loopback" IRE_BROADCAST entries.
	 */
	if (ipif->ipif_subnet && (ipif->ipif_flags & IFF_BROADCAST)) {
		ipaddr_t addr;

		ip1dbg(("ipif_up_done: creating broadcast IRE\n"));
		irep = ire_create_bcast(ipif, 0, irep,
		    (MATCH_IRE_TYPE | MATCH_IRE_ILL));
		irep = ire_create_bcast(ipif, INADDR_BROADCAST, irep,
		    (MATCH_IRE_TYPE | MATCH_IRE_ILL));

		addr = net_mask & ipif->ipif_subnet;
		irep = ire_create_bcast(ipif, addr, irep,
		    (MATCH_IRE_TYPE | MATCH_IRE_ILL));
		irep = ire_create_bcast(ipif, ~net_mask | addr, irep,
		    (MATCH_IRE_TYPE | MATCH_IRE_ILL));

		addr = ipif->ipif_subnet;
		irep = ire_create_bcast(ipif, addr, irep,
		    (MATCH_IRE_TYPE | MATCH_IRE_ILL));
		irep = ire_create_bcast(ipif, ~subnet_mask|addr, irep,
		    (MATCH_IRE_TYPE | MATCH_IRE_ILL));

	}

	/* If an earlier ire_create failed, get out now */
	for (irep1 = irep; irep1 > ire_array; ) {
		irep1--;
		if (*irep1 == NULL) {
			ip1dbg(("ipif_up_done: bad ire found in ire_array\n"));
			goto bad;
		}
	}

	/*
	 * Add in all newly created IREs.  ire_create_bcast() has
	 * already checked for duplicates of the IRE_BROADCAST type.
	 * We want to add before we call ifgrp_insert which wants
	 * to know whether IRE_IF_RESOLVER exists or not.
	 *
	 * NOTE : We refrele the ire though we may branch to "bad"
	 *	later on where we do ire_delete. This is okay
	 *	because nobody can delete it as we are running
	 *	exclusively.
	 */
	for (irep1 = irep; irep1 > ire_array; ) {
		irep1--;
		*irep1 = ire_add(*irep1);
		if (*irep1 != NULL) {
			ire_refrele(*irep1);		/* Held in ire_add */
		}
	}
	/*
	 * If grouping interfaces and ipif address is not 0, insert this ipif
	 * into the appropriate interface group, or create a new one.
	 */
	if (ip_enable_group_ifs && ipif->ipif_subnet) {
		if (!ifgrp_insert(ipif)) { /* If ifgrp allocation failed. */
			ip1dbg(("ipif_up_done: ifgrp allocation failed\n"));
			goto bad;
		}
	}

	/* Recover any additional IRE_IF_[NO]RESOLVER entries for this ipif */
	ipif_recover_ire(ipif);

	/* If this is a loopback interface, we are done. */
	if (ipif->ipif_ill->ill_wq == NULL) {
		ipif->ipif_flags |= IFF_UP;
		ipif->ipif_ipif_up_count++;
		/* This one doesn't count towards ipif_g_count. */

		/* Join the allhosts multicast address */
		ipif_multicast_up(ipif);

		ip_rts_ifmsg(ipif);
		ip_rts_newaddrmsg(RTM_ADD, 0, ipif);
		return (0);
	}

	/*
	 * If the broadcast address has been set, make sure it makes sense
	 * based on the interface address.
	 * Only match on ill since we are sharing broadcast addresses.
	 */
	if (ipif->ipif_brd_addr && (ipif->ipif_flags & IFF_BROADCAST)) {
		ire_t	*ire;

		ire = ire_ctable_lookup(ipif->ipif_brd_addr, 0,
		    IRE_BROADCAST, ipif, NULL,
		    (MATCH_IRE_TYPE | MATCH_IRE_ILL));

		if (ire == NULL) {
			/*
			 * If there isn't a matching broadcast IRE,
			 * revert to the default for this netmask.
			 */
			ipif->ipif_v6brd_addr = ipv6_all_zeros;
			ipif_set_default(ipif);
		} else {
			ire_refrele(ire);
		}
	}

	/* Mark it up, and increment counters. */
	ill->ill_ipif_up_count++;
	ipif->ipif_flags |= IFF_UP;
	ipif_g_count++;

	/* This is the first interface on this ill */
	if (ipif->ipif_ipif_up_count == 1) {
		/*
		 * Need to recover all multicast memberships in the driver.
		 * This had to be deferred until we had attached.
		 */
		ill_recover_multicast(ill);
	}
	/* Join the allhosts multicast address */
	ipif_multicast_up(ipif);

	/*
	 * This had to be deferred until we had bound.
	 * tell routing sockets that this interface is up
	 */
	ip_rts_ifmsg(ipif);
	ip_rts_newaddrmsg(RTM_ADD, 0, ipif);

	/*
	 * For IPv4, ipif_id 0 is special. If we did a down,
	 * then an up on that, then we clobbered the
	 * published arp entries for all the other ipif's.
	 * Put the other arp entries back in.
	 */
	if (ipif->ipif_id == 0)
		ill_reset_arp(ipif);

	/* Broadcast an address mask reply. */
	ipif_mask_reply(ipif);
	return (0);

bad:
	ip1dbg(("ipif_up_done: FAILED \n"));
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
* Turn off the ARP with the IFF_NOARP flag
*/
static int
ipif_arp_off(ipif_t *ipif, uint32_t addr)
{
	mblk_t	*arp_off_mp = NULL;
	mblk_t	*arp_on_mp = NULL;
	ill_t	*ill = ipif->ipif_ill;

	ip1dbg(("ipif_arp_off(%s:%u)\n",
		ipif->ipif_ill->ill_name, ipif->ipif_id));

	ASSERT(!ipif->ipif_isv6);

	/*
	 * Make sure that the interface has been configured with a
	 * corresponding IRE_IF_RESOLVER before deactivating ARP
	 */
	if (ill->ill_net_type != IRE_IF_RESOLVER || (addr == 0))
		return (EINVAL);

	/*
	 * If the on message is still around we've already done
	 * an arp_off without doing an arp_on thus there is no
	 * work needed.
	 */
	if (ipif->ipif_arp_on_mp != NULL)
		return (0);

	/*
	 * Allocate an ARP on message (to be saved)
	 * and an ARP off message
	 */
	arp_off_mp = ill_arp_alloc(ill, (uchar_t *)&ip_aroff_template, 0);
	if (!arp_off_mp)
		return (ENOMEM);

	arp_on_mp = ill_arp_alloc(ill, (uchar_t *)&ip_aron_template, 0);
	if (!arp_on_mp)
		goto failed;

	ASSERT(ipif->ipif_arp_on_mp == NULL);
	ipif->ipif_arp_on_mp = arp_on_mp;

	/* Send an AR_INTERFACE_OFF request */
	putnext(ill->ill_rq, arp_off_mp);
	return (0);
failed:

	if (arp_off_mp)
		freemsg(arp_off_mp);
	return (ENOMEM);
}

/*
* Turn on ARP by turning off the IFF_NOARP flag
*/
static int
ipif_arp_on(ipif_t *ipif, uint32_t addr)
{
	mblk_t	*mp;
	ill_t	*ill = ipif->ipif_ill;

	ip1dbg(("ipif_arp_on(%s:%u)\n",
		ipif->ipif_ill->ill_name, ipif->ipif_id));

	ASSERT(!ipif->ipif_isv6);

	/*
	 * Make sure that the interface has been configured
	 * before activating  ARP i.e. addr is not 0
	 */
	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    (addr == 0))
		return (EINVAL);

	/*
	 * Send an AR_INTERFACE_ON request if we have already done
	 * an arp_off (which allocated the message).
	 */
	if (ipif->ipif_arp_on_mp != NULL) {
		mp = ipif->ipif_arp_on_mp;
		ipif->ipif_arp_on_mp = NULL;
		putnext(ill->ill_rq, mp);
	}
	return (0);
}

/*
 * The following functions deal with "interface groups", or
 * ifgrp's.  An interface group is a group of ipifs that share a
 * common subnet prefix and lie on a common physical subnet.  When new
 * routes of IRE_CACHE are created, any member of the same interface
 * group as the ire that is the "parent" of the new ire is fair game
 * for outgoing traffic.  This is useful for when (say) a server uses
 * multiple ports on a 10/100-Base T switch so as to increase its
 * bandwidth by the amount of ports it plugs into.
 *
 * The basic operations are insertion, deletion, and scheduling, where
 * ipif's are inserted and deleted into an interface group.  When a
 * new IRE_CACHE is created (in ip_newroute()) or when TCP needs a
 * source address (in ip_bind()), the "scheduler" selects an interface
 * for that new route based on the ipif of the parent route.
 *
 * Note that the unit of scheduling is the ipif, or as a user will see it,
 * a "logical interface".  An ifgrp has a "set" of active ipifs, linked in
 * a circular list with their ipif_ifgrpnext pointers.  A "set" spans multiple
 * ills (physical interfaces).
 *
 * PLEASE DO NOT CONFUSE ifgrps WITH MULTICAST GROUPS!  Thank you.
 */

/*
 * Tell NDD if we're grouping or not grouping interfaces.
 */
/* ARGSUSED */
int
ifgrp_get(queue_t *q, mblk_t *mp, void *cp)
{
	(void) mi_mpprintf(mp, "%d", ip_enable_group_ifs);
	return (0);
}

/*
 * Clobber all cache entries for a given ipif.
 */
static void
ipif_delete_cache(ire_t *ire, char *ipif_arg)
{
	ipif_t	*ipif = (ipif_t *)ipif_arg;

	if (ire->ire_ipif != ipif)
	    return;
	if (ire->ire_type == IRE_CACHE)
	    ire_delete(ire);
}

/*
 * For a given ipif, find all ipifs on its ill that would be in the same
 * ifgrp, and make their ipif_ifgrp entries NULL.  This ensures that
 * they are cleaned out so ifgrp_delete() won't consider them as replacments.
 */
static void
ifgrp_ill_clean(ipif_t *ipif)
{
	ipif_t *walker;

	for (walker = ipif->ipif_ill->ill_ipif; walker != NULL;
	    walker = walker->ipif_next) {
		if (walker != ipif && walker->ipif_ifgrp == ipif->ipif_ifgrp) {
			ASSERT(walker->ipif_ifgrpnext == NULL);
			walker->ipif_ifgrp = NULL;
		}
	}
}

/*
 * Enable or disable grouping of interfaces, and all of the subsequent
 * stuff.  I assume this is called as a writer (NDD set routine is, I
 * believe, see further up in this file for ND_SET handling).
 */
/* ARGSUSED */
int
ifgrp_set(queue_t *q, mblk_t *mp, char *value, void *cp)
{
	int new_value;
	char *end;
	ill_t *ill;
	ipif_t *ipif;

	new_value = (int)mi_strtol(value, &end, 10);
	if ((end == value) ||
	    ((new_value != 0) && (new_value != 1)))
		return (EINVAL);
	if (new_value == ip_enable_group_ifs)
		return (0);
	ip_enable_group_ifs = new_value;
	if (ip_enable_group_ifs) {
		for (ill = ill_g_head; ill != NULL; ill = ill->ill_next) {
			for (ipif = ill->ill_ipif; ipif != NULL;
			    ipif = ipif->ipif_next) {
				if (!(ipif->ipif_flags & IFF_UP)) {
					/* Just in case... */
					ifgrp_delete(ipif);
					continue;
				}
				if (ipif->ipif_isv6 ?
				    !IN6_IS_ADDR_UNSPECIFIED(
				    &ipif->ipif_v6subnet) :
				    ipif->ipif_subnet != INADDR_ANY) {
					if (!ifgrp_insert(ipif)) {
						/*
						 * Bail and set to 0?
						 * Return error?
						 */
						ip0dbg(("sudden ifgrp"
						    " insert failed"));
					}
				}
			}
		}
	} else {
		/*
		 * We'll have to duplicate some of the ifgrp_delete work here,
		 * but not a lot.
		 */
		while (ifgrp_head != NULL) {
			if (ifgrp_head->ifgrp_schednext->ipif_ifgrpnext ==
			    ifgrp_head->ifgrp_schednext) {
				/* Singleton */
				ifgrp_ill_clean(ifgrp_head->ifgrp_schednext);
				ifgrp_delete(ifgrp_head->ifgrp_schednext);
			} else {
				/* Multiple-member ifgrp, flush routes! */
				ifgrp_t *ifgrp;
				ipif_t *ipif, *tmp;

				ifgrp = ifgrp_head;
				ifgrp_head = ifgrp_head->ifgrp_next;
				ipif = ifgrp->ifgrp_schednext;
				do {
					ire_walk(ipif_delete_cache,
					    (char *)ipif);
					ifgrp_ill_clean(ipif);
					ipif->ipif_ifgrp = NULL;
					tmp = ipif;
					ipif = ipif->ipif_ifgrpnext;
					tmp->ipif_ifgrpnext = NULL;
				} while (ipif != ifgrp->ifgrp_schednext);

				mi_free((char *)ifgrp);
			}
		}
	}
	return (0);
}

/*
 * Insert an ipif into an appropriate interface group.  Return TRUE if
 * inserted, or there's a better one there.  Return FALSE if there are real
 * problems (like memory allocation failures).  Assume that I'm a writer at
 * this point.
 */
boolean_t
ifgrp_insert(ipif_t *ipif)
{
	ifgrp_t *ifgrp;
	ipif_t *walker;
	ire_t *ipif_ire = NULL;

	mutex_enter(&ifgrp_l_mutex);

	if ((ipif->ipif_type == IFT_OTHER) ||
	    (ipif->ipif_flags & IFF_POINTOPOINT) ||
	    (ipif_ire = (ipif->ipif_isv6 ? ipif_to_ire_v6(ipif) :
	    ipif_to_ire(ipif))) == NULL) {
		/*
		 * When (IFF_NOLOCAL|IFF_NOXMIT|IFF_DEPRECATED|IFF_ANYCAST) is
		 * set ipif_to_ire might return NULL which would break
		 * ifgrp scheduling causing packets to be dropped.
		 * When any of the above flags are modified the
		 * ifgrp scheduling is redone through ipif_down/ipif_up
		 * transitions.
		 *
		 * IFT_OTHER's aren't allowed into ifgrps... for now.
		 * Multiple other IFT_TYPE's can mix and match in a single
		 * ifgrp, but the ip_newroute() code catches conflicts
		 * and makes sure that if the original is different than
		 * the results of ifgrp_scheduler().
		 *
		 * Additionally, IFF_POINTOPOINT interfaces usually
		 * have no topological relationship to each other, so
		 * grouping them is generally a Bad Idea (TM).
		 *
		 * Ensure that ipif_to_ire* returns an IRE. It might not
		 * due to various flags e.g. IFF_NOLOCAL or IFF_NOXMIT.
		 * If ipif_to_ire* were to return NULL it would break
		 * ifgrp scheduling causing packets to be dropped.
		 * When any of the above flags are modified the
		 * ifgrp scheduling is redone through ipif_down/ipif_up
		 * transitions.
		 */
		ipif->ipif_ifgrp = NULL;
		ipif->ipif_ifgrpnext = ipif;

		mutex_exit(&ifgrp_l_mutex);
		return (B_TRUE);
	}
	ASSERT(ipif_ire != NULL);
	ire_refrele(ipif_ire);

	/*
	 * Locate the ifgrp, keyed off of the ipif's prefix.  (That is,
	 * (addr & netmask).)
	 */
	for (ifgrp = ifgrp_head; ifgrp != NULL; ifgrp = ifgrp->ifgrp_next) {
		if (ifgrp->ifgrp_schednext->ipif_isv6 != ipif->ipif_isv6)
			continue;
		if (IN6_ARE_ADDR_EQUAL(
		    &ifgrp->ifgrp_schednext->ipif_v6net_mask,
		    &ipif->ipif_v6net_mask) &&
		    IN6_ARE_ADDR_EQUAL(&ifgrp->ifgrp_schednext->ipif_v6subnet,
		    &ipif->ipif_v6subnet)) {
			break;
		}
	}

	/*
	 * Now that I've found (or not found) an ifgrp, either...
	 */

	if (ifgrp == NULL) {
		/* ...allocate new ifgrp, and insert a new singleton... */
		ifgrp = (ifgrp_t *)mi_alloc(sizeof (ifgrp_t), BPRI_MED);
		if (ifgrp == NULL) {
			/* Allocation failure, return false! */
			mutex_exit(&ifgrp_l_mutex);
			cmn_err(CE_WARN, "ifgrp_insert() failed on %s:%d.\n",
			    ipif->ipif_ill->ill_name, ipif->ipif_id);
			return (B_FALSE);
		}
		ifgrp->ifgrp_next = ifgrp_head;
		ifgrp->ifgrp_schednext = ipif;
		ifgrp_head = ifgrp;

		ipif->ipif_ifgrpnext = ipif;
	} else {
		/* ... or insert a new member into an existing group. */
		walker = ifgrp->ifgrp_schednext;
		do {
			if (walker->ipif_ill == ipif->ipif_ill)
				break;	/* out of do-loop. */
			walker = walker->ipif_ifgrpnext;
		} while (walker != ifgrp->ifgrp_schednext);

		if (walker->ipif_ill == ipif->ipif_ill) {
			/*
			 * There's an ipif with this ill already.  Replace
			 * it if the ipif's id (e.g. 0 in le1:0) is lower
			 * than the one already here.
			 */
			ASSERT(walker->ipif_id != ipif->ipif_id);
			if (walker->ipif_id > ipif->ipif_id) {
				/*
				 * I'm lower than the current one on this ill.
				 * Replace it with me.
				 *
				 * This will cause slowdown if multiple
				 * concurrent ifgrp_inserts() on the same
				 * ifgrp are happening.  How often do multiple
				 * concurrent ifgrp_inserts() on the same
				 * ifgrp happen?
				 */
				ire_walk(ipif_delete_cache, (char *)walker);
				mutex_exit(&ifgrp_l_mutex);
				ifgrp_delete(walker);

				/*
				 * The compiler should catch this tail
				 * recursion.
				 */
				return (ifgrp_insert(ipif));
			} else {
				/*
				 * Else there's a lower-numbered ipif on this
				 * ill.  Null me out and just let things happen
				 * in ifgrp_scheduler()/ifgrp_delete().
				 */
				ipif->ipif_ifgrpnext = NULL;
			}
		} else {
			/*
			 * New ill, put this ipif in ifgrp's set.
			 *
			 * For now, insert right after current ifgrp_schednext.
			 */
			ipif->ipif_ifgrpnext = walker->ipif_ifgrpnext;
			walker->ipif_ifgrpnext = ipif;
		}
	}

	/* Common code for both cases. */
	ipif->ipif_ifgrp = ifgrp;

	mutex_exit(&ifgrp_l_mutex);
	return (B_TRUE);
}

/*
 * Delete an ipif from its interface group.  Return TRUE if succeeded.
 */
void
ifgrp_delete(ipif_t *ipif)
{
	ifgrp_t *ifgrp, *floater;
	ipif_t *current = NULL, *replacement;

	mutex_enter(&ifgrp_l_mutex);

	if (ipif->ipif_ifgrp != NULL && ipif->ipif_ifgrpnext != NULL) {
		/*
		 * Find a replacement if I'm an actual ifgrp member in a set.
		 * If there is another ipif on the passed-in's ill with the
		 * same prefix (but a higher number) mark it as a replacment.
		 *
		 * NOTE:  I'll find the lowest-numbered replacement.  If the
		 *	  lowest-numbered replacement was created last, then
		 *	  this will be a slow linear search.
		 *
		 * NOTE2: I'm checking the ipif_ifgrp pointer only because of
		 *	  the clobber condition in ifgrp_set().  The check
		 *	  is equivalent to:
		 *
		 *	(replacement->ipif_flags & IFF_UP) &&
		 *	    replacement->ipif_net_mask == ipif->ipif_net_mask &&
		 *	(replacement->ipif_lcl_addr &
		 *	    replacement->ipif_net_mask) ==
		 *	(ipif->ipif_lcl_addr & ipif->ipif_net_mask)
		 */
		ipif_t *best_so_far = NULL;

		for (replacement = ipif->ipif_ill->ill_ipif;
		    replacement != NULL; replacement = replacement->ipif_next) {
			if (replacement != ipif &&
			    replacement->ipif_ifgrp == ipif->ipif_ifgrp) {
				if (best_so_far == NULL ||
				    best_so_far->ipif_id > replacement->ipif_id)
					best_so_far = replacement;

				/*
				 * Optimization:  If the best one so far's ID
				 * is one less that what it's replacing, we
				 * don't need to keep searching.
				 */
				ASSERT(best_so_far != NULL);
				if (best_so_far->ipif_id - 1 == ipif->ipif_id)
					break;
			}
		}
		replacement = best_so_far;
	} else {
		replacement = NULL;
		ip1dbg(("ifgrp_delete() on an ipif (%s:%d) that wasn't\n",
		    ipif->ipif_ill->ill_name, ipif->ipif_id));
		ip1dbg(("inserted, was IFT_OTHER, or was not lowest on"
		    " its ill.\n"));
		goto bail;	/* We're really all done here. */
	}

	/*
	 * If I'm actually replacing the ipif to be deleted, then make sure
	 * the replacment isn't lower than me!
	 */
	ASSERT(replacement == NULL || replacement->ipif_id > ipif->ipif_id);

	if (ipif->ipif_ifgrpnext == ipif) {
		/* Last entry, delete this whole group. */

		/* Reality check */
		ASSERT(*(ipif->ipif_ifgrpschednext) == ipif);

		ifgrp = ipif->ipif_ifgrp;
		if (ifgrp_head == ifgrp) {
		    ifgrp_head = ifgrp->ifgrp_next;
		} else {
			floater = ifgrp_head;
			while (floater->ifgrp_next != ifgrp)
			    floater = floater->ifgrp_next;
			floater->ifgrp_next = ifgrp->ifgrp_next;
		}
		mi_free((char *)ifgrp);
	} else {
		/* Delete this particular entry. */

		/* Advance scheduler pointer if needed. */
		if (*(ipif->ipif_ifgrpschednext) == ipif)
			*(ipif->ipif_ifgrpschednext) = ipif->ipif_ifgrpnext;

		/*
		 * Scan down list for predecessor.  (We do this to save the
		 * "Previous pointer" field bytes.)
		 */
		current = ipif;
		while (current->ipif_ifgrpnext != ipif)
			current = current->ipif_ifgrpnext;
		current->ipif_ifgrpnext = ipif->ipif_ifgrpnext;
	}

bail:
	/* Null pointers here are indicators for other ifgrp routines. */
	ipif->ipif_ifgrp = NULL;
	ipif->ipif_ifgrpnext = NULL;

	mutex_exit(&ifgrp_l_mutex);
	if (replacement != NULL)
		(void) ifgrp_insert(replacement);
}

/*
 * Given an ipif, find the next ipif in its interface group to use.
 * (This should be called by ip_newroute() before ire_create().)
 *
 * The reason insert and delete are relatively convoluted is because that
 * convolution buys much simplicity in a simple round-robin ifgrp_scheduler.
 *
 * Even if an ipif isn't in the ipif_ifgrpnext circle, this function will
 * find one that is and return it.
 *
 * If round-robin isn't sufficient for interface groups, then make the
 * gyrations in ifgrp_insert() and ifgrp_delete().  This function should be
 * FAST, FAST, FAST!
 */
ipif_t *
ifgrp_scheduler(ipif_t *ipif)
{
	ipif_t *retval;

	/* If I'm not in an ifgrp, return the ipif. */
	if (ipif->ipif_ifgrp == NULL)
		return (ipif);

	mutex_enter(&ifgrp_l_mutex);
	retval = *(ipif->ipif_ifgrpschednext);
	*(ipif->ipif_ifgrpschednext) = retval->ipif_ifgrpnext;
	mutex_exit(&ifgrp_l_mutex);
	return (retval);
}

/* ARGSUSED */
int
ifgrp_report(queue_t *q, mblk_t *mp, void *arg)
{
	ipif_t	*ipif, *first_ipif;
	ifgrp_t	*ifgrp;
	char	buf1[INET6_ADDRSTRLEN];
	char	buf2[INET6_ADDRSTRLEN];
	char	buf3[INET6_ADDRSTRLEN];
	char	buf4[INET6_ADDRSTRLEN];
	int	i = 0;

	mutex_enter(&ifgrp_l_mutex);
	for (ifgrp = ifgrp_head; ifgrp != NULL;
	    ifgrp = ifgrp->ifgrp_next, i++) {
		ipif = ifgrp->ifgrp_schednext;
		(void) inet_ntop(AF_INET6,
		    &ipif->ipif_v6lcl_addr, buf1, sizeof (buf1));
		(void) inet_ntop(AF_INET6,
		    &ipif->ipif_v6src_addr, buf2, sizeof (buf2));
		(void) inet_ntop(AF_INET6,
		    &ipif->ipif_v6subnet, buf3, sizeof (buf3));
		(void) inet_ntop(AF_INET6,
		    &ipif->ipif_v6net_mask, buf4, sizeof (buf4));
		if (ipif->ipif_ifgrpnext == ipif) {
			(void) mi_mpprintf((mblk_t *)mp,
			    "ifgrp %d: singleton fl 0x%x %s\n"
			    "\tlcl_addr %s\n"
			    "\tsrc_addr %s\n"
			    "\tsubnet %s\n"
			    "\tnetmask %s\n",
			    i, ipif->ipif_flags, ipif->ipif_ill->ill_name,
			    buf1, buf2, buf3, buf4);
			continue;
		}
		(void) mi_mpprintf((mblk_t *)mp,
		    "ifgrp %d: fl 0x%x %s\n"
		    "\tlcl_addr %s\n"
		    "\tsrc_addr %s\n"
		    "\tsubnet %s\n"
		    "\tnetmask %s\n",
		    i, ipif->ipif_flags, ipif->ipif_ill->ill_name,
		    buf1, buf2, buf3, buf4);
		first_ipif = ipif;
		for (ipif = first_ipif->ipif_ifgrpnext; ipif != first_ipif;
		    ipif = ipif->ipif_ifgrpnext) {
			(void) inet_ntop(AF_INET6,
			    &ipif->ipif_v6lcl_addr, buf1, sizeof (buf1));
			(void) inet_ntop(AF_INET6,
			    &ipif->ipif_v6src_addr, buf2, sizeof (buf2));
			(void) inet_ntop(AF_INET6,
			    &ipif->ipif_v6subnet, buf3, sizeof (buf3));
			(void) inet_ntop(AF_INET6,
			    &ipif->ipif_v6net_mask, buf4, sizeof (buf4));
			(void) mi_mpprintf((mblk_t *)mp,
			    "ifgrp %d: fl 0x%x %s\n"
			    "\tlcl_addr %s\n"
			    "\tsrc_addr %s\n"
			    "\tsubnet %s\n"
			    "\tnetmask %s\n",
			    i, ipif->ipif_flags, ipif->ipif_ill->ill_name,
			    buf1, buf2, buf3, buf4);
		}
	}
	mutex_exit(&ifgrp_l_mutex);
	return (0);
}

/*
 * Determine the best source address given a destination address and an ill.
 * Prefers non-deprecated over deprecated but will return a deprecated
 * address if there is no other choice.
 *
 * Returns NULL if there is no suitable source address for the ill.
 * This only occurs when there is no valid source address for the ill.
 */
ipif_t *
ipif_select_source(ill_t *ill, ipaddr_t dst)
{
	ipif_t *ipif;
	ipif_t *ipif_dep = NULL;	/* Fallback to deprecated */

	for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
		/* Always skip NOLOCAL and ANYCAST interfaces */
		if (ipif->ipif_flags & (IFF_NOLOCAL|IFF_ANYCAST))
			continue;
		if (!(ipif->ipif_flags & IFF_UP))
			continue;
		if (ipif->ipif_flags & IFF_DEPRECATED) {
			ipif_dep = ipif;
			continue;
		}
		break;
	}
	if (ipif == NULL)
		ipif = ipif_dep;

#ifdef DEBUG
	if (ipif == NULL) {
		char buf1[INET6_ADDRSTRLEN];

		ip1dbg(("ipif_select_source(%s, %s) -> NULL\n",
		    ill->ill_name,
		    inet_ntop(AF_INET, &dst, buf1, sizeof (buf1))));
	} else {
		char buf1[INET6_ADDRSTRLEN];
		char buf2[INET6_ADDRSTRLEN];

		ip1dbg(("ipif_select_source(%s, %s) -> %s\n",
		    ill->ill_name,
		    inet_ntop(AF_INET, &dst, buf1, sizeof (buf1)),
		    inet_ntop(AF_INET, &ipif->ipif_lcl_addr,
		    buf2, sizeof (buf2))));
	}
#endif /* DEBUG */
	return (ipif);
}

/*
 * This old_ipif is going away.
 *
 * Determine if any other ipif's is using our address as
 * ipif_lcl_addr (due to those being IFF_NOLOCAL, IFF_ANYCAST, or
 * IFF_DEPRECATED).
 * Find the IRE_INTERFACE for such ipifs and recreate them
 * to use an different source address following the rules in
 * ipif_up_done.
 */
void
ipif_update_other_ipifs(ipif_t *old_ipif)
{
	ipif_t *ipif;
	ipif_t *nipif;
	ire_t *ire;
	queue_t *stq;
	char	buf1[INET6_ADDRSTRLEN];

	ASSERT(!(old_ipif->ipif_flags & IFF_UP));

	/* Is there any work to be done? */
	if (old_ipif->ipif_lcl_addr == 0 ||
	    old_ipif->ipif_ill->ill_wq == NULL)
		return;

	ip1dbg(("ipif_update_other_ipifs(%s, %s)\n",
	    old_ipif->ipif_ill->ill_name,
	    inet_ntop(AF_INET, &old_ipif->ipif_lcl_addr,
	    buf1, sizeof (buf1))));

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
		 * in ipif_up_done.
		 */
		if (!(ipif->ipif_flags & IFF_UP))
			continue;
		if ((ipif->ipif_flags & IFF_NOXMIT) ||
		    (ipif->ipif_subnet == INADDR_ANY))
			continue;
		/*
		 * We know that ipif uses some other source for its
		 * IRE_INTERFACE. Is it using the source of this old_ipif?
		 */
		ire = ipif_to_ire(ipif);
		if (ire == NULL)
			continue;
		if (old_ipif->ipif_lcl_addr != ire->ire_src_addr)
			continue;

		ip1dbg(("ipif_update_other_ipifs: deleting IRE for src %s\n",
		    inet_ntop(AF_INET, &ire->ire_src_addr, buf1,
		    sizeof (buf1))));

		stq = ire->ire_stq;

		/* Remove the ire and recreate one */
		ire_delete(ire);
		ire_refrele(ire);

		/*
		 * Can't use our source address. Select a different
		 * source address for the IRE_INTERFACE.
		 */
		nipif = ipif_select_source(ipif->ipif_ill, ipif->ipif_subnet);
		if (nipif == NULL) {
			/* Last resort - all ipif's have IFF_NOLOCAL */
			nipif = ipif;
		}
		ip1dbg(("ipif_update_other_ipifs: create if IRE %d for %s\n",
		    ipif->ipif_ill->ill_net_type,
		    inet_ntop(AF_INET, &ipif->ipif_subnet, buf1,
		    sizeof (buf1))));

		ire = ire_create(
		    (uchar_t *)&ipif->ipif_subnet,	/* dest pref */
		    (uchar_t *)&ipif->ipif_net_mask,	/* mask */
		    (uchar_t *)&nipif->ipif_src_addr,	/* src addr */
		    NULL,				/* no gateway */
		    ipif->ipif_mtu,			/* max frag */
		    NULL,				/* fast path header */
		    NULL,				/* no recv from queue */
		    stq,				/* send-to queue */
		    ipif->ipif_ill->ill_net_type, 	/* IF_[NO]RESOLVER */
		    ipif->ipif_ill->ill_resolver_mp,	/* xmit header */
		    ipif,
		    0,
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

int
if_unitsel(queue_t *q, mblk_t *mp, uint_t ppa)
{
	queue_t		*q1 = q;
	char 		*cp;
	char		interf_name[LIFNAMSIZ];

	if (!q->q_next) {
		ip1dbg((
		    "ip_sioctl_copyin_done: IF_UNITSEL: no q_next\n"));
		return (EINVAL);
	}

	do {
		q1 = q1->q_next;
	} while (q1->q_next);
	cp = q1->q_qinfo->qi_minfo->mi_idname;
	(void) sprintf(interf_name, "%s%d", cp, ppa);

	/*
	 * Here we are not going to delay the ioack until after
	 * ACKs from DL_ATTACH_REQ/DL_BIND_REQ. So no need to save the
	 * original ioctl message before sending the requests.
	*/
	return (ipif_set_values(q, mp, interf_name, &ppa));
}

/*
 * Net and subnet broadcast ire's are now specific to the particular
 * physical interface (ill) and not to any one locigal interface (ipif).
 * However, if a particular logical interface is being taken down, it's
 * associated ire's will be taken down as well.  Hence, when we go to
 * take down or change the local address, broadcast address or netmask
 * of a specific logical interface, we must check to make sure that we
 * have valid net and subnet broadcast ire's for the other logical
 * interfaces which may have been shared with the logical interface
 * being brought down or changed.
 *
 * Note: assume that the ipif passed in is still up so that it's IRE
 * entries are still valid.
 */
static void
ipif_check_bcast_ires(ipif_t *test_ipif)
{
	ipif_t	*ipif;
	ire_t	*test_subnet_ire, *test_net_ire;
	ire_t	*ire_array[8];
	ire_t	**irep = &ire_array[0];

	ipaddr_t net_addr, subnet_addr, net_mask, subnet_mask;
	ipaddr_t test_net_addr, test_subnet_addr;
	ipaddr_t test_net_mask, test_subnet_mask;
	int	need_net_bcast_ire = 0, need_subnet_bcast_ire = 0;
	int	net_bcast_ire_created = 0, subnet_bcast_ire_created = 0;


	ASSERT(!test_ipif->ipif_isv6);

	test_net_mask = ip_net_mask(test_ipif->ipif_subnet);
	test_subnet_mask = test_ipif->ipif_net_mask;

	/*
	 * If no net mask set, assume the default based on net class.
	 */
	if (test_subnet_mask == 0)
		test_subnet_mask = test_net_mask;

	/*
	 * Check if there is a network broadcast ire associated with this ipif
	 */
	test_net_addr = test_net_mask  & test_ipif->ipif_subnet;
	test_net_ire = ire_ctable_lookup(test_net_addr, 0, IRE_BROADCAST,
	    test_ipif, NULL, (MATCH_IRE_TYPE | MATCH_IRE_IPIF));

	/*
	 * No broadcast ire's associated with this ipif.
	 * Short circuit here for performance.
	 */
	if ((test_net_mask == test_subnet_mask) && (test_net_ire == NULL))
		return;

	/*
	 * Check if there is a subnet broadcast IRE associated with this ipif
	 */
	test_subnet_addr = test_subnet_mask  & test_ipif->ipif_subnet;
	test_subnet_ire = ire_ctable_lookup(test_subnet_addr, 0, IRE_BROADCAST,
	    test_ipif, NULL, (MATCH_IRE_TYPE | MATCH_IRE_IPIF));

	/*
	 * No broadcast ire's associated with this ipif.
	 */
	if ((test_subnet_ire == NULL) && (test_net_ire == NULL))
		return;

	/*
	 * Look at all ipif's for this ill.  We are looking for ipif's
	 * whose broadcast addr match the ipif passed in, but do not have
	 * their own broadcast ire's.
	 */
	for (ipif = test_ipif->ipif_ill->ill_ipif; ipif;
	    ipif = ipif->ipif_next) {

		ASSERT(!ipif->ipif_isv6);
		/*
		 * Already checked the ipif passed in.
		 */
		if (ipif == test_ipif) {
			continue;
		}

		/*
		 * Only interested in logical interfaces with valid local
		 * addresses or with the ability to broadcast.
		 */
		if ((ipif->ipif_subnet == 0) ||
		    !(ipif->ipif_flags & IFF_BROADCAST) ||
		    !(ipif->ipif_flags & IFF_UP)) {
			continue;
		}

		/*
		 * Check if there is a net broadcast ire for this
		 * net address.  If it turns out that the ipif we are
		 * about to take down owns this ire, we must make a
		 * new one because it is potentially going away.
		 */
		if (test_net_ire) {
			net_mask = ip_net_mask(ipif->ipif_subnet);
			net_addr = net_mask & ipif->ipif_subnet;
			if (!net_bcast_ire_created) {
				if (net_addr == test_net_addr) {
					need_net_bcast_ire = 1;
				}
			}
		}

		/*
		 * Check if there is a subnet broadcast ire for this
		 * net address.  If it turns out that the ipif we are
		 * about to take down owns this ire, we must make a
		 * new one because it is potentially going away.
		 */
		if (test_subnet_ire) {
			if (!subnet_bcast_ire_created) {
				subnet_mask = ipif->ipif_net_mask;
				subnet_addr = ipif->ipif_subnet;
				if (subnet_addr == test_subnet_addr) {
					need_subnet_bcast_ire = 1;
				}
			}
		}


		/*
		 * Found an ipif which has the same broadcast ire as the
		 * ipif passed in and the ipif passed in "owns" the ire.
		 * Create new broadcast ire's for this broadcast addr.
		 */

		if (need_net_bcast_ire && !net_bcast_ire_created) {
			irep = ire_create_bcast(ipif, net_addr, irep,
			    (MATCH_IRE_TYPE | MATCH_IRE_IPIF));
			irep = ire_create_bcast(ipif, ~net_mask | net_addr,
			    irep, (MATCH_IRE_TYPE | MATCH_IRE_IPIF));

			net_bcast_ire_created = 1;
		}

		if (need_subnet_bcast_ire && !subnet_bcast_ire_created) {
			irep = ire_create_bcast(ipif, subnet_addr, irep,
			    (MATCH_IRE_TYPE | MATCH_IRE_IPIF));
			irep = ire_create_bcast(ipif, ~subnet_mask|subnet_addr,
			    irep, (MATCH_IRE_TYPE | MATCH_IRE_IPIF));

			subnet_bcast_ire_created = 1;
		}

		/*
		 * add in any IRE's that we might have created.
		 */
		while (irep > ire_array) {
			irep--;
			*irep = ire_add(*irep);
			if (*irep != NULL) {
				ire_refrele(*irep);	/* Held in ire_add */
			}
		}

		/*
		 * Once we have created both the net broadcast ire and the
		 * subnet broadcast ire to replace the ones going away
		 * when test_ipif is brought down, return.
		 */
		if (test_net_ire) {
			if (test_subnet_ire) {
				if (net_bcast_ire_created &&
				    subnet_bcast_ire_created) {
					break;
				}
			} else if (net_bcast_ire_created) {
				break;
			}
		} else if (test_subnet_ire) {
			if (subnet_bcast_ire_created) {
				break;
			}
		}
	}
	if (test_net_ire != NULL)
		ire_refrele(test_net_ire);
	if (test_subnet_ire != NULL)
		ire_refrele(test_subnet_ire);
}

/*
 * Extract both the flags (including IFF_CANTCHANGE) such as IFF_IPV*
 * from lifr_flags and the name from lifr_name.
 * Set IFF_IPV* and ill_isv6 prior to doing the lookup
 * since ipif_lookup_on_name uses the _isv6 flags when matching.
 * Returns EINPROGRESS when mp has been consumed by queueing it on
 * ill_pending_mp and the ioctl will complete in ip_rput.
 */
static int
ip_sioctl_slifname(struct lifreq *lifr, queue_t *q, mblk_t *mp)
{
	ill_t		*ill = (ill_t *)q->q_ptr;
	ipif_t		*ipif = ill->ill_ipif;

	ASSERT(ipif != NULL);
	ip1dbg(("ip_sioctl_slifname %s\n",
	    lifr->lifr_name));

	if (q->q_next == NULL) {
		/* Not an ill queue */
		return (EINVAL);
	}

	ill = (ill_t *)q->q_ptr;
	ipif = ill->ill_ipif;
	ASSERT(ipif != NULL);
	if (ill->ill_name_set)
		return (EALREADY);

	if ((mi_strlen(lifr->lifr_name) + 1) > LIFNAMSIZ) {
		return (ENXIO);
	}

	/*
	 * Set all the flags. Allows all kinds of override. Provide some
	 * sanity checking by not allowing IFF_BROADCAST and IFF_MULTICAST
	 * unless there is either multicast/broadcast support in the driver
	 * or it is a pt-pt link.
	 */
	if (lifr->lifr_flags & (IFF_PROMISC|IFF_ALLMULTI)) {
		/* Meaningless to IP thus don't allow them to be set. */
		ip1dbg(("ip_setname: EINVAL 1\n"));
		return (EINVAL);
	}
	if ((lifr->lifr_flags & IFF_MULTICAST) &&
	    !(lifr->lifr_flags & IFF_POINTOPOINT) &&
	    ill->ill_bcast_addr_length == 0) {
		/* Link not broadcast/pt-pt capable i.e. no multicast */
		ip1dbg(("ip_setname: EINVAL 2\n"));
		return (EINVAL);
	}
	if ((lifr->lifr_flags & IFF_BROADCAST) &&
	    ((lifr->lifr_flags & IFF_IPV6) ||
	    ill->ill_bcast_addr_length == 0)) {
		/* Link not broadcast capable or IPv6 i.e. no broadcast */
		ip1dbg(("ip_setname: EINVAL 3\n"));
		return (EINVAL);
	}
	if (lifr->lifr_flags & IFF_UP) {
		/* Can only be set with SIOCSLIFFLAGS */
		ip1dbg(("ip_setname: EINVAL 4\n"));
		return (EINVAL);
	}
	if ((lifr->lifr_flags & (IFF_IPV6|IFF_IPV4)) != IFF_IPV6 &&
	    (lifr->lifr_flags & (IFF_IPV6|IFF_IPV4)) != IFF_IPV4) {
		ip1dbg(("ip_setname: EINVAL 5\n"));
		return (EINVAL);
	}
	ipif->ipif_flags = lifr->lifr_flags;
	return (ipif_set_values(q, mp, lifr->lifr_name, &lifr->lifr_ppa));
}

/*
 * Return a pointer to the ill which matches the index and IP version type.
 */
ill_t *
ill_lookup_on_ifindex(uint_t index, boolean_t isv6)
{
	ill_t   *ill;

	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		if (ill->ill_index == index &&
		    ill->ill_isv6 == isv6)
			return (ill);
	}
	return (NULL);
}

/*
 * We first need to ensure that the new index is unique, and
 * then carry the change across both v4 and v6 ill representation
 * of the physical interface.
 */
static int
ip_sioctl_slifindex(ipif_t *ipif, uint_t index)
{
	ill_t		*ill_other;
	ipif_t		*ipif_other;
	boolean_t	exists;

	/* Only allow on physical interface. Also, index zero is illegal. */
	if (ipif->ipif_id != 0 || index == 0)
		return (EINVAL);

	/*
	 * Use ill_lookup_on_ifindex to determine if the
	 * new index is unused and if so allow the change.
	*/
	if ((ill_lookup_on_ifindex(index, B_TRUE)) &&
	    (ill_lookup_on_ifindex(index, B_FALSE)))
		return (EBUSY);

	/*
	 * The new index is unused. We need to change
	 * the corresponding ill_index of both IPv4 and IPv6
	 * ills pertaining to this physical interface.
	 * Store the original index before making the change,
	 * so that we can use it to find the any other ill pertaining
	 * to the same physical interface
	*/
	ill_other = ill_lookup_on_ifindex(ipif->ipif_index, !(ipif->ipif_isv6));

	/* Apply the index change accross all corresponding ills */
	ipif->ipif_ill->ill_index = index;
	ip_rts_ifmsg(ipif);

	if (ill_other != NULL) {
		ill_other->ill_index = index;
		ipif_other = ipif_lookup_on_name(
		    ill_other->ill_name,
		    ill_other->ill_name_length, B_FALSE, &exists,
		    ill_other->ill_isv6);
		if (ipif_other != NULL)
			ip_rts_ifmsg(ipif_other);
	}
	return (0);
}

/*
 * Common routine for ppa and ifname setting.
 *
 * Returns EINPROGRESS when mp has been consumed by queueing it on
 * ill_pending_mp and the ioctl will complete in ip_rput.
 *
 * NOTE : If ppa is UNIT_MAX, we assign the next valid ppa and return
 * the new name and new ppa in lifr_name and lifr_ppa respectively.
 * For SLIFNAME, we pass these values back to the userland.
 */
int
ipif_set_values(queue_t *q, mblk_t *mp, char *interf_name, uint_t *ppa)
{
	ill_t		*ill;
	ipif_t		*ipif;
	size_t		newlength;
	ill_t		*ill_tmp;
	char savebuf[LIFNAMSIZ + 4];
	int err;

	ASSERT(q->q_next != NULL);

	ill = (ill_t *)q->q_ptr;

	if (ill->ill_name_set)
		return (EALREADY);
	ip1dbg(("ipif_set_values: interface %s\n", interf_name));

	/* Save off the old NDD name for this ill. */
	bcopy(ill->ill_ndd_name, savebuf, ill->ill_name_length +
	    sizeof (ill_forward_name_suffix));
	/*
	 * Unload it now too, so subsequent rewhack's don't cause nd_unload()
	 * to fail.
	 */
	nd_unload(&ip_g_nd, savebuf);

	if (*ppa == UINT_MAX) {
		char buf[40];
		char *ptr = interf_name;
		char *save_ptr;
		int name_length, count;
		int ppa_space = 0;

		/*
		 * interf_name has the ppa which we don't need.
		 */
		while (ptr != NULL && *ptr != '\0') {
			if (*ptr >= '0' && *ptr <= '9') {
				break;
			}
			ptr++;
		}
		if (*ptr == '\0') {
			/*
			 * There was no ppa in interf_name.
			 */
			return (EINVAL);
		}
		/*
		 * We need to insert the ppa into interf_name
		 * and null terminate the string. It's easy
		 * if we fill the interf_name with NULLs here
		 * rather than after inserting the ppa.
		 */
		save_ptr = ptr;
		while (*ptr != '\0')
			*ptr++ = '\0';
		/*
		 * If we don't have space to insert the ppa,
		 * we will fail below.
		 */
		ppa_space = ptr - save_ptr;
		/*
		 * We don't wan't to find ourself on the list when we
		 * do the ill_lookup_on_name below. ppa is initialized
		 * in ill_init and normally valid for non-tunnels. Thus,
		 * we would like to re-use them here.
		 */
		ill->ill_name_length = 0;
		/*
		 * We are repeating the logic that was used in ill_init
		 * to find a non-used ppa number and hence the restriction
		 * for 1000.
		 */
		count = -1;
		do {
			if (++count >= 1000) {
				return (ENXIO);
			}
			/* Do we have enough space to insert ppa ? */
			numtos(count, buf);
			if (mi_strlen(buf) > ppa_space)
				return (EINVAL);
			(void) sprintf(save_ptr, "%d", count);
			name_length = (uint_t)(mi_strlen(interf_name) + 1);
		} while (ill_lookup_on_name(interf_name, name_length,
		    B_FALSE, (ill->ill_ipif->ipif_flags & IFF_IPV6)));
		*ppa = count;
	}

	newlength = mi_strlen(interf_name) + 1;

	/* Avoid finding ourself in the lookup by setting len to 0 */
	ill->ill_name_length = 0;
	if (ill_tmp = ill_lookup_on_name(interf_name, newlength, B_FALSE,
	    (ill->ill_ipif->ipif_flags & IFF_IPV6))) {
		ip1dbg(("ipif_set_values: found 0x%p %s isv6 %d\n",
		    (void *)ill_tmp, ill_tmp->ill_name, ill_tmp->ill_isv6));
		ip1dbg(("ipif_set_values: 0x%p %s is busy isv6 %d\n",
		    (void *)ill, interf_name, ill->ill_isv6));
		ill->ill_name_length =  mi_strlen(interf_name) + 1;
		return (EBUSY);
	}

	bcopy(interf_name, ill->ill_name, newlength);
	ill->ill_name_length = newlength;

	/* Rewhack the NDD per-interface forwarding name. */
	err = ill_set_ndd_name(ill);
	if (err != 0) {
		cmn_err(CE_WARN, "ipif_set_values: ill_set_ndd_name (%d)\n",
		    err);
	}

	ipif = ill->ill_ipif;
	if (!(ipif->ipif_flags & (IFF_IPV4|IFF_IPV6)))
		ipif->ipif_flags |= IFF_IPV4;

	ASSERT(ipif->ipif_next == NULL);	/* Only one ipif on ill */
	ASSERT((ipif->ipif_flags & IFF_UP) == 0);

	if (ipif->ipif_flags & IFF_IPV6) {
		ill->ill_isv6 = B_TRUE;

		/*
		 * If there is no MLD timer running, get one started.
		 * Note: There is only one mld timer notification
		 * message blk created for all V6 interfaces
		*/
		if (mld_timer_mp == NULL) {
			mld_timer_mp = mi_timer_alloc(0);
			if (mld_timer_mp != NULL) {
				mld_timer_ill = ill;
				mi_timer(ill->ill_rq, mld_timer_mp, 1);
			}
		}

		/* allocate v6 mib */
		if (!ill_allocate_mibs(ill)) {
			/* just to make sure nobody tries to use them */
			ill->ill_isv6 = B_FALSE;
			return (ENOMEM);
		}
		if (ill->ill_rq != NULL) {
			ill->ill_rq->q_qinfo = &rinit_ipv6;
			ill->ill_wq->q_qinfo = &winit_ipv6;
		}

		/* Keep the !IN6_IS_ADDR_V4MAPPED assertions happy */
		ipif->ipif_v6lcl_addr = ipv6_all_zeros;
		ipif->ipif_v6src_addr = ipv6_all_zeros;
		ipif->ipif_v6subnet = ipv6_all_zeros;
		ipif->ipif_v6net_mask = ipv6_all_zeros;
		ipif->ipif_v6brd_addr = ipv6_all_zeros;
		ipif->ipif_v6pp_dst_addr = ipv6_all_zeros;
		/*
		 * point-to-point or Non-mulicast capable
		 * interfaces won't do NUD unless explicitly
		 * configured to do so.
		 */
		if (ipif->ipif_flags & IFF_POINTOPOINT ||
		    !(ipif->ipif_flags & IFF_MULTICAST)) {
			ipif->ipif_flags |= IFF_NONUD;
		}
		/* Make sure IPv4 specific flag is not set on IPv6 if */
		if (ipif->ipif_flags & IFF_NOARP) {
			ipif->ipif_flags &= ~IFF_NOARP;
		}
	} else if (ipif->ipif_flags & IFF_IPV4) {
		ill->ill_isv6 = B_FALSE;
		IN6_IPADDR_TO_V4MAPPED(INADDR_ANY, &ipif->ipif_v6lcl_addr);
		IN6_IPADDR_TO_V4MAPPED(INADDR_ANY, &ipif->ipif_v6src_addr);
		IN6_IPADDR_TO_V4MAPPED(INADDR_ANY, &ipif->ipif_v6subnet);
		IN6_IPADDR_TO_V4MAPPED(INADDR_ANY, &ipif->ipif_v6net_mask);
		IN6_IPADDR_TO_V4MAPPED(INADDR_ANY, &ipif->ipif_v6brd_addr);
		IN6_IPADDR_TO_V4MAPPED(INADDR_ANY, &ipif->ipif_v6pp_dst_addr);

		/* If there is no IGMP timer running, get one started. */
		if (igmp_timer_mp == NULL) {
			igmp_timer_mp = mi_timer_alloc(0);
			if (igmp_timer_mp != NULL) {
				igmp_timer_ill = ill;
				mi_timer(ill->ill_rq, igmp_timer_mp,
				    igmp_timer_interval);
			}
		}

		/* If there is no slowtimeout running, get one started. */
		if (igmp_slowtimeout_id == 0) {
			igmp_slowtimeout_id = qtimeout(ill->ill_rq,
			    igmp_slowtimo, ill->ill_rq,
			    MSEC_TO_TICK(IGMP_SLOWTIMO_INTERVAL));
			if (ip_mrtdebug > 0)
				(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
				    "ipif_set_values: start qtimeout");
		}

	}
	/*
	 * Pick a default sap until we get the DL_INFO_ACK back from
	 * the driver.
	 */
	if (ill->ill_sap == 0) {
		if (ill->ill_isv6)
			ill->ill_sap  = IP6_DL_SAP;
		else
			ill->ill_sap  = IP_DL_SAP;
	}
	/* The ppa is sent down by ifconfig or chosen above. */
	ill->ill_ppa = *ppa;
	ill->ill_name_set = 1;
	ill->ill_ifname_pending = 1;
	return (ill_dl_phys(ill, ill->ill_ipif, mp, q));
}
