/*
 * Copyright (c) 1992, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)arp.c	1.52	99/10/21 SMI"

/* AR - Address Resolution Protocol */

#define	ARP_DEBUG

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/strlog.h>
#include <sys/dlpi.h>
#include <sys/sockio.h>
#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/snmpcom.h>
#include <net/if.h>
#include <inet/arp.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/ip_ire.h>
#include <inet/mib2.h>

#ifdef ARP_DEBUG
#define	arp0dbg(a)	printf a
#define	arp1dbg(a)	if (arp_debug) printf a
#define	arp2dbg(a)	if (arp_debug > 1) printf a
#define	arp3dbg(a)	if (arp_debug > 2) printf a
#else
#define	arp0dbg(a)	/* */
#define	arp1dbg(a)	/* */
#define	arp2dbg(a)	/* */
#define	arp3dbg(a)	/* */
#endif

#define	ACE_RESOLVED(ace)	((ace)->ace_flags & ACE_F_RESOLVED)

#define	AR_DEF_XMIT_INTERVAL	500	/* time in milliseconds */
#define	AR_LL_HDR_SLACK	32	/* Leave the lower layer some room */

#define	AR_SNMP_MSG		T_OPTMGMT_ACK

#ifndef	ARP_DL_SAP
/* DLPI SAPs are in host byte order for all systems */
#define	ARP_DL_SAP	0x0806
#endif

/* ARP Cache Entry */
typedef struct ace_s {
	struct ace_s	*ace_next;	/* Hash chain next pointer */
	struct ace_s	**ace_ptpn;	/* Pointer to previous next */
	struct arl_s	*ace_arl;	/* Associated arl */
	uint32_t	ace_proto;		/* Protocol for this ace */
	uint32_t	ace_flags;
	uchar_t	*ace_proto_addr;
	uint32_t	ace_proto_addr_length;
	uchar_t	*ace_proto_mask;	/* Mask for matching addr */
	uchar_t	*ace_proto_extract_mask; /* For mappings */
	uchar_t	*ace_hw_addr;
	uint32_t	ace_hw_addr_length;
	uint32_t	ace_hw_extract_start;	/* For mappings */
	mblk_t	*ace_mp;		/* mblk we are in */
	uint32_t	ace_query_count;
	mblk_t	*ace_query_mp;		/* Head of outstanding query chain */
} ace_t;

#define	ACE_EXTERNAL_FLAGS_MASK \
(ACE_F_PERMANENT | ACE_F_PUBLISH | ACE_F_MAPPING | ACE_F_MYADDR)

#define	ARH_FIXED_LEN	8

/* ARL Structure, one per link level device */
typedef struct arl_s {
	struct arl_s	*arl_next;		/* ARL chain at arl_g_head */
	queue_t		*arl_rq;		/* Read queue pointer */
	queue_t		*arl_wq;		/* Write queue pointer */
	t_uscalar_t	arl_ppa;		/* DL_ATTACH parameter */
	uchar_t		*arl_arp_addr;		/* multicast address to use */
	uchar_t		*arl_hw_addr;		/* Our hardware address */
	uint32_t	arl_hw_addr_length;
	uint32_t	arl_arp_hw_type;	/* Our hardware type */
	uchar_t		*arl_name;		/* Lower level name */
	uint32_t	arl_name_length;
	uchar_t		arl_name_set;
	mblk_t		*arl_xmit_template;	/* DL_UNITDATA_REQ template */
	t_uscalar_t	arl_xmit_template_addr_offset;
	t_uscalar_t	arl_xmit_template_sap_offset;
	t_scalar_t	arl_xmit_template_sap_length;
	mblk_t		*arl_unbind_mp;
	mblk_t		*arl_detach_mp;
	mblk_t		*arl_attach_mp;
	mblk_t		*arl_bind_mp;
	t_uscalar_t	arl_provider_style;	/* From DL_INFO_ACK */
	t_uscalar_t	arl_mac_sap;		/* From DL_INFO_ACK */
	uchar_t		arl_bind_pending;
	uchar_t		arl_unbind_pending;
	mblk_t		*arl_dlpiop_done;	/* DLPI opertion done */
	queue_t		*arl_ip_pending_queue;	/* Pending queue */
	mblk_t		*arl_queue;	/* Delayed due to pending bind */
	uint32_t	arl_flags;	/* Used for IFF_NOARP */
} arl_t;
#define	ARL_F_NOARP	 0x1

/* AR Structure, one per upper stream */
typedef struct ar_s {
	queue_t		*ar_rq;	/* Read queue pointer */
	queue_t		*ar_wq;	/* Write queue pointer */
	arl_t		*ar_arl;	/* Associated arl */
	boolean_t	ar_priv_stream;	/* Privileged client? */
} ar_t;

/*
 * MAC-specific intelligence.  Shouldn't be needed, but the DL_INFO_ACK
 * doesn't quite do it for us.
 */
typedef struct ar_m_s {
	t_uscalar_t	ar_mac_type;
	uint32_t	ar_mac_arp_hw_type;
	t_uscalar_t	ar_mac_sap;
	t_scalar_t	ar_mac_sap_length;
	uint32_t	ar_mac_hw_addr_length;
} ar_m_t;

/* Named Dispatch Parameter Management Structure */
typedef struct arpparam_s {
	uint32_t	arp_param_min;
	uint32_t	arp_param_max;
	uint32_t	arp_param_value;
	char		*arp_param_name;
} arpparam_t;

extern ire_stats_t ire_stats_v4;

static mblk_t	*ar_alloc(uint32_t cmd);
static int	ar_ce_create(arl_t *arl, uint32_t proto, uchar_t *hw_addr,
    uint32_t hw_addr_len, uchar_t *proto_addr,
    uint32_t proto_addr_len, uchar_t *proto_mask,
    uchar_t *proto_extract_mask, uint32_t hw_extract_start,
    uint32_t flags);
static void	ar_ce_delete(ace_t *ace);
static void	ar_ce_delete_per_arl(ace_t *ace, uchar_t *arl);
static ace_t	**ar_ce_hash(uint32_t proto, uchar_t *proto_addr,
    uint32_t proto_addr_length);
static ace_t	*ar_ce_lookup(arl_t *arl, uint32_t proto,
    uchar_t *proto_addr, uint32_t proto_addr_length);
static ace_t	*ar_ce_lookup_entry(arl_t *arl, uint32_t proto,
    uchar_t *proto_addr, uint32_t proto_addr_length);
static ace_t	*ar_ce_lookup_from_area(mblk_t *mp, ace_t *matchfn());
static ace_t	*ar_ce_lookup_mapping(arl_t *arl, uint32_t proto,
    uchar_t *proto_addr, uint32_t proto_addr_length);
static int	ar_ce_report(queue_t *q, mblk_t *mp, void *data);
static void	ar_ce_report1(ace_t *ace, uchar_t *mp_arg);
static void	ar_ce_resolve(ace_t *ace, uchar_t *hw_addr,
    uint32_t hw_addr_length);
static void	ar_ce_walk(pfi_t pfi, uchar_t *arg1);

static void	ar_cleanup(void);
static void	ar_client_notify(arl_t *arl, mblk_t *mp, int code);
static int	ar_close(queue_t *q);
static int	ar_cmd_dispatch(queue_t *q, mblk_t *mp);
static mblk_t	*ar_dlpi_comm(t_uscalar_t prim, size_t size);
static int	ar_entry_add(queue_t *q, mblk_t *mp);
static int	ar_entry_delete(queue_t *q, mblk_t *mp);
static int	ar_entry_query(queue_t *q, mblk_t *mp);
static int	ar_entry_squery(queue_t *q, mblk_t *mp);
static void	ar_freemsg(mblk_t *mp);
static int	ar_interface_up(queue_t *q, mblk_t *mp);
static int	ar_interface_down(queue_t *q, mblk_t *mp);
static int	ar_interface_on(queue_t *q, mblk_t *mp);
static int	ar_interface_off(queue_t *q, mblk_t *mp);
static int	ar_ll_down(arl_t *arl);
static void	ar_ll_down_delayed(arl_t *arl);
static void	ar_ll_enqueue(arl_t *arl, queue_t *q, mblk_t *mp);
static void	ar_ll_freequeue(arl_t *arl);
static arl_t	*ar_ll_lookup_by_name(uchar_t *name,
    uint32_t name_length);
static arl_t	*ar_ll_lookup_from_mp(mblk_t *mp);
static void	ar_ll_runqueue(arl_t *arl);
static void	ar_ll_subnet_defaults(queue_t *q, mblk_t *mp);
static int	ar_ll_up(arl_t *arl);
static void	ar_ll_up_delayed(arl_t *arl);
static int	ar_mapping_add(queue_t *q, mblk_t *mp);
static boolean_t	ar_mask_all_ones(uchar_t *mask, uint32_t mask_len);
static ar_m_t	*ar_m_lookup(t_uscalar_t mac_type);
static int	ar_nd_ioctl(queue_t *q, mblk_t *mp);
static int	ar_open(queue_t *q, dev_t *devp, int flag, int sflag,
    cred_t *credp);
static int	ar_param_get(queue_t *q, mblk_t *mp, void *cp);
static boolean_t	ar_param_register(arpparam_t *arppa, int cnt);
static int	ar_param_set(queue_t *q, mblk_t *mp, char *value,
    void *cp);
static int	ar_query_delete(ace_t *ace, uchar_t *ar);
static void	ar_query_reply(ace_t *ace, int ret_val,
    uchar_t *proto_addr, uint32_t proto_addr_len);
static clock_t	ar_query_xmit(ace_t *ace);
static void	ar_rput(queue_t *q, mblk_t *mp_orig);
static void	ar_rput_dlpi(queue_t *q, mblk_t *mp);
static void	ar_set_address(ace_t *ace, uchar_t *addrpos,
    uchar_t *proto_addr, uint32_t proto_addr_len);
static int	ar_slifname(queue_t *q, mblk_t *mp);
static int	ar_set_ppa(queue_t *q, mblk_t *mp);
static int	ar_snmp_msg(queue_t *q, mblk_t *mp);
static void	ar_snmp_msg2(ace_t *ace, uchar_t *mp_arg);
static void	ar_timer_init(queue_t *q);
static int	ar_trash(ace_t *ace, uchar_t *arg);
static void	ar_wput(queue_t *q, mblk_t *mp);
static void	ar_wsrv(queue_t *q);
static void	ar_xmit(arl_t *arl, uint32_t operation, uint32_t proto,
    uint32_t plen, uchar_t *haddr1, uchar_t *paddr1,
    uchar_t *haddr2, uchar_t *paddr2);
static int	ar_xmit_request(queue_t *q, mblk_t *mp);
static int	ar_xmit_response(queue_t *q, mblk_t *mp);

#if 0
static void	show_ace(char *str, ace_t *ace);
static void	show_arp(char *str, mblk_t *mp);
#endif

/* All of these are alterable, within the min/max values given, at run time */
static arpparam_t	arp_param_arr[] = {
	/* min		max		value	name */
	{ 0,		10,		0,	"arp_debug"},
	{ 30000,	3600000,	300000,	"arp_cleanup_interval"},
};

#define	arp_debug		arp_param_arr[0].arp_param_value
#define	arp_timer_interval	arp_param_arr[1].arp_param_value

static struct module_info info = {
	0, "arp", 0, INFPSZ, 512, 128
};

static struct qinit rinit = {
	(pfi_t)ar_rput, NULL, ar_open, ar_close, NULL, &info
};

static struct qinit winit = {
	(pfi_t)ar_wput, (pfi_t)ar_wsrv, ar_open, ar_close, NULL, &info
};

struct streamtab arpinfo = {
	&rinit, &winit
};

static int arpexcl = 0;
#define	ARP_ENTER() { if (arpexcl++) (void) mi_panic("ARP_ENTER"); }
#define	ARP_EXIT() { if (arpexcl-- != 1) (void) mi_panic("ARP_EXIT"); }

static void	*ar_g_head;	/* AR Instance Data List Head */
static caddr_t	ar_g_nd;	/* AR Named Dispatch Head */
static arl_t	*arl_g_head;	/* ARL List Head */

/*
 * TODO: we need a better mechanism to set the ARP hardware type since
 * the DLPI mac type does not include enough prodefined values.
 */
static ar_m_t	ar_m_tbl[] = {
	{ DL_CSMACD,	1,	ARP_DL_SAP,	-2,	6},	/* 802.3 */
	{ DL_TPB,	6,	ARP_DL_SAP,	-2,	6},	/* 802.4 */
	{ DL_TPR,	6,	ARP_DL_SAP,	-2,	6},	/* 802.5 */
	{ DL_METRO,	6,	ARP_DL_SAP,	-2,	6},	/* 802.6 */
	{ DL_ETHER,	1,	ARP_DL_SAP,	-2,	6},	/* Ethernet */
	{ DL_FDDI,	1,	ARP_DL_SAP,	-2,	6},	/* FDDI */
};

/* ARP Cache Entry Hash Table */
static ace_t	*ar_ce_hash_tbl[256];

static ace_t	*ar_ce_mask_entries;	/* proto_mask not all ones */

static mblk_t	*ar_timer_mp;		/* garbage collection timer */
static queue_t	*ar_timer_queue;	/* queue for garbage collection */

/*
 * Note that all routines which need to queue the message for later
 * processing have to be ioctl_aware to be able to queue the
 * complete message.
 */
static arct_t	ar_cmd_tbl[] = {
	{ ar_entry_add,		AR_ENTRY_ADD,		sizeof (area_t),
	    1, 1 },
	{ ar_entry_delete,	AR_ENTRY_DELETE,	sizeof (ared_t),
	    1, 1 },
	{ ar_entry_query,	AR_ENTRY_QUERY,		sizeof (areq_t),
	    1, 0 },
	{ ar_entry_squery,	AR_ENTRY_SQUERY,	sizeof (area_t),
	    1, 0 },
	{ ar_xmit_request,	AR_XMIT_REQUEST,	sizeof (areq_t),
	    1, 1 },
	{ ar_xmit_response,	AR_XMIT_RESPONSE,	sizeof (areq_t),
	    1, 1 },
	{ ar_mapping_add,	AR_MAPPING_ADD,		sizeof (arma_t),
	    1, 1 },
	{ ar_interface_up,	AR_INTERFACE_UP,	sizeof (arc_t),
	    0, 1 },
	{ ar_interface_down,	AR_INTERFACE_DOWN,	sizeof (arc_t),
	    0, 1 },
	{ ar_interface_on,	AR_INTERFACE_ON,	sizeof (arc_t),
	    0, 1 },
	{ ar_interface_off,	AR_INTERFACE_OFF,	sizeof (arc_t),
	    0, 1 },
	{ ar_set_ppa,		(uint32_t)IF_UNITSEL,	sizeof (int),
	    1, 1 },
	{ ar_nd_ioctl,		ND_GET,			1,
	    1, 0 },
	{ ar_nd_ioctl,		ND_SET,			1,
	    1, 1 },
	{ ar_snmp_msg,		AR_SNMP_MSG,	sizeof (struct T_optmgmt_ack),
	    0, 0},
	{ar_slifname,		(uint32_t)SIOCSLIFNAME,	sizeof (struct lifreq),
	    1, 1 }
};

/*
 * ARP Cache Entry creation routine.
 * Cache entries are allocated within timer messages and inserted into
 * the global hash list based on protocol and protocol address.
 */
static int
ar_ce_create(arl_t *arl, uint_t proto, uchar_t *hw_addr, uint_t hw_addr_len,
    uchar_t *proto_addr, uint_t proto_addr_len, uchar_t *proto_mask,
    uchar_t *proto_extract_mask, uint_t hw_extract_start, uint_t flags)
{
	static ace_t	ace_null;
	ace_t	*ace;
	ace_t	**acep;
	uchar_t	*dst;
	mblk_t	*mp;

	if ((flags & ~ACE_EXTERNAL_FLAGS_MASK) || !arl)
		return (EINVAL);
	if (flags & ACE_F_MYADDR)
		flags |= ACE_F_PUBLISH;

	if (!hw_addr && hw_addr_len == 0) {
		if (flags == ACE_F_PERMANENT) {	/* Not publish */
			/* 224.0.0.0 to zero length address */
			flags |= ACE_F_RESOLVED;
		} else {	/* local address and unresolved case */
			if ((hw_addr = arl->arl_hw_addr) != 0)
				hw_addr_len = arl->arl_hw_addr_length;
			if (flags & ACE_F_PUBLISH)
				flags |= ACE_F_RESOLVED;
		}
	} else {
		flags |= ACE_F_RESOLVED;
	}

	if (!proto_addr || proto_addr_len == 0)
		return (EINVAL);
	/* Handle hw_addr_len == 0 for DL_ENABMULTI_REQ etc. */
	if (hw_addr_len && !hw_addr)
		return (EINVAL);
	if (hw_addr_len < arl->arl_hw_addr_length && hw_addr_len != 0)
		return (EINVAL);
	if (!proto_extract_mask && (flags & ACE_F_MAPPING))
		return (EINVAL);
	/*
	 * Allocate the timer block to hold the ace.
	 * (ace + proto_addr + proto_addr_mask + proto_extract_mask + hw_addr)
	 */
	mp = mi_timer_alloc(sizeof (ace_t) + proto_addr_len + proto_addr_len +
	    proto_addr_len + hw_addr_len);
	if (!mp)
		return (ENOMEM);
	ace = (ace_t *)mp->b_rptr;
	*ace = ace_null;
	ace->ace_proto = proto;
	ace->ace_mp = mp;
	ace->ace_arl = arl;

	dst = (uchar_t *)&ace[1];

	ace->ace_proto_addr = dst;
	ace->ace_proto_addr_length = proto_addr_len;
	bcopy(proto_addr, dst, proto_addr_len);
	dst += proto_addr_len;
	/*
	 * The proto_mask allows us to add entries which will let us respond
	 * to requests for a group of addresses.  This makes it easy to provide
	 * proxy ARP service for machines that don't understand about the local
	 * subnet structure, if, for example, there are BSD4.2 systems lurking.
	 */
	ace->ace_proto_mask = dst;
	if (proto_mask) {
		bcopy(proto_mask, dst, proto_addr_len);
		dst += proto_addr_len;
	} else {
		while (proto_addr_len--)
			*dst++ = (uchar_t)~0;
	}

	if (proto_extract_mask) {
		ace->ace_proto_extract_mask = dst;
		bcopy(proto_extract_mask, dst, ace->ace_proto_addr_length);
		dst += ace->ace_proto_addr_length;
	} else {
		ace->ace_proto_extract_mask = NULL;
	}
	ace->ace_hw_extract_start = hw_extract_start;
	ace->ace_hw_addr_length = hw_addr_len;
	ace->ace_hw_addr = dst;
	if (hw_addr) {
		bcopy(hw_addr, dst, hw_addr_len);
		dst += hw_addr_len;
	}

	ace->ace_arl = arl;
	ace->ace_flags = flags;
	if (ar_mask_all_ones(ace->ace_proto_mask,
	    ace->ace_proto_addr_length)) {
		acep = ar_ce_hash(ace->ace_proto, ace->ace_proto_addr,
		    ace->ace_proto_addr_length);
	} else
		acep = &ar_ce_mask_entries;
	if ((ace->ace_next = *acep) != 0)
		ace->ace_next->ace_ptpn = &ace->ace_next;
	*acep = ace;
	ace->ace_ptpn = acep;
	return (0);
}

/* Delete a cache entry. */
static void
ar_ce_delete(ace_t *ace)
{
	ace_t	**acep;

	/* Get out of the hash list. */
	acep = ace->ace_ptpn;
	if (ace->ace_next)
		ace->ace_next->ace_ptpn = acep;
	acep[0] = ace->ace_next;
	/* Mark it dying in case we have a timer about to fire. */
	ace->ace_flags |= ACE_F_DYING;
	/* Complete any outstanding queries immediately. */
	ar_query_reply(ace, ENXIO, NULL, (uint32_t)0);
	/* Free the timer, immediately, or when it fires. */
	mi_timer_free(ace->ace_mp);
}

/*
 * ar_ce_walk routine.	Delete the ace if it is associated with the arl
 * that is going away.
 */
static void
ar_ce_delete_per_arl(ace_t *ace, uchar_t *arl)
{
	if (ace && ace->ace_arl == (arl_t *)arl) {
		ace->ace_flags &= ~ACE_F_PERMANENT;
		ar_ce_delete(ace);
	}
}

/* Cache entry hash routine, based on protocol and protocol address. */
static ace_t **
ar_ce_hash(uint32_t proto, uchar_t *proto_addr, uint32_t  proto_addr_length)
{
	uchar_t	*up = proto_addr;
	unsigned int hval = proto;
	int	len = proto_addr_length;

	while (--len >= 0)
		hval ^= *up++;
	return (&ar_ce_hash_tbl[hval % A_CNT(ar_ce_hash_tbl)]);
}

/* Cache entry lookup.	Try to find an ace matching the parameters passed. */
ace_t *
ar_ce_lookup(arl_t *arl, uint32_t proto, uchar_t *proto_addr,
    uint32_t proto_addr_length)
{
	ace_t	*ace;

	ace = ar_ce_lookup_entry(arl, proto, proto_addr, proto_addr_length);
	if (!ace)
		ace = ar_ce_lookup_mapping(arl, proto, proto_addr,
		    proto_addr_length);
	return (ace);
}

/*
 * Cache entry lookup.	Try to find an ace matching the parameters passed.
 * Look only for exact entries (no mappings)
 */
static ace_t *
ar_ce_lookup_entry(arl_t *arl, uint32_t proto, uchar_t *proto_addr,
    uint32_t proto_addr_length)
{
	ace_t	*ace;

	if (!proto_addr)
		return (NULL);
	ace = *ar_ce_hash(proto, proto_addr, proto_addr_length);
	for (; ace; ace = ace->ace_next) {
		if (ace->ace_arl == arl &&
		    ace->ace_proto_addr_length == proto_addr_length &&
		    ace->ace_proto == proto) {
			int	i1 = proto_addr_length;
			uchar_t	*ace_addr = ace->ace_proto_addr;
			uchar_t	*mask = ace->ace_proto_mask;
			/*
			 * Note that the ace_proto_mask is applied to the
			 * proto_addr before comparing to the ace_addr.
			 */
			do {
				if (--i1 < 0)
					return (ace);
			} while ((proto_addr[i1] &  mask[i1]) == ace_addr[i1]);
		}
	}
	return (ace);
}

/*
 * Extract cache entry lookup parameters from an external command message, then
 * call the supplied match function.
 */
static ace_t *
ar_ce_lookup_from_area(mblk_t *mp, ace_t *matchfn())
{
	uchar_t	*proto_addr;
	area_t	*area = (area_t *)mp->b_rptr;

	proto_addr = mi_offset_paramc(mp, area->area_proto_addr_offset,
	    area->area_proto_addr_length);
	if (!proto_addr)
		return (NULL);
	return ((*matchfn)(ar_ll_lookup_from_mp(mp), area->area_proto,
	    proto_addr, area->area_proto_addr_length));
}

/*
 * Cache entry lookup.	Try to find an ace matching the parameters passed.
 * Look only for mappings.
 */
static ace_t *
ar_ce_lookup_mapping(arl_t *arl, uint32_t proto, uchar_t *proto_addr,
    uint32_t proto_addr_length)
{
	ace_t	*ace;

	if (!proto_addr)
		return (NULL);
	ace = ar_ce_mask_entries;
	for (; ace; ace = ace->ace_next) {
		if (ace->ace_arl == arl &&
		    ace->ace_proto_addr_length == proto_addr_length &&
		    ace->ace_proto == proto) {
			int	i1 = proto_addr_length;
			uchar_t	*ace_addr = ace->ace_proto_addr;
			uchar_t	*mask = ace->ace_proto_mask;
			/*
			 * Note that the ace_proto_mask is applied to the
			 * proto_addr before comparing to the ace_addr.
			 */
			do {
				if (--i1 < 0)
					return (ace);
			} while ((proto_addr[i1] &  mask[i1]) == ace_addr[i1]);
		}
	}
	return (ace);
}

/*
 * Pass a cache report back out via NDD.
 * TODO:  Right now this report assumes IP proto address formatting.
 */
/* ARGSUSED */
static int
ar_ce_report(queue_t *q, mblk_t *mp, void *arg)
{
	(void) mi_mpprintf(mp,
	    "ifname   proto addr      proto mask      hardware addr     flags");
	/*   abcdefgh xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx xx:xx:xx:xx:xx:xx */
	ar_ce_walk((pfi_t)ar_ce_report1, (uchar_t *)mp);
	return (0);
}

/*
 * Add a single line to the ARP Cache Entry Report.
 * TODO:  Right now this report assumes IP proto address formatting.
 */
static void
ar_ce_report1(ace_t *ace, uchar_t *mp_arg)
{
	static uchar_t	zero_array[8];
	uint32_t	flags = ace->ace_flags;
	mblk_t	*mp = (mblk_t *)mp_arg;
	uchar_t	*p = ace->ace_proto_addr;
	uchar_t	*h = ace->ace_hw_addr;
	uchar_t	*m = ace->ace_proto_mask;
	char	*name = "unknown";

	if (ace->ace_arl && ace->ace_arl->arl_name)
		name = (char *)ace->ace_arl->arl_name;
	if (!p)
		p = zero_array;
	if (!h)
		h = zero_array;
	if (!m)
		m = zero_array;
	(void) mi_mpprintf(mp,
	    "%8s %03d.%03d.%03d.%03d "
	    "%03d.%03d.%03d.%03d %02x:%02x:%02x:%02x:%02x:%02x",
	    name,
	    p[0] & 0xFF, p[1] & 0xFF, p[2] & 0xFF, p[3] & 0xFF,
	    m[0] & 0xFF, m[1] & 0xFF, m[2] & 0xFF, m[3] & 0xFF,
	    h[0] & 0xFF, h[1] & 0xFF, h[2] & 0xFF, h[3] & 0xFF,
	    h[4] & 0xFF, h[5] & 0xFF);
	if (flags & ACE_F_PERMANENT)
		(void) mi_mpprintf_nr(mp, " PERM");
	if (flags & ACE_F_PUBLISH)
		(void) mi_mpprintf_nr(mp, " PUBLISH");
	if (flags & ACE_F_DYING)
		(void) mi_mpprintf_nr(mp, " DYING");
	if (!(flags & ACE_F_RESOLVED))
		(void) mi_mpprintf_nr(mp, " UNRESOLVED");
	if (flags & ACE_F_MAPPING)
		(void) mi_mpprintf_nr(mp, " MAPPING");
	if (flags & ACE_F_MYADDR)
		(void) mi_mpprintf_nr(mp, " MYADDR");
}

/*
 * ar_ce_resolve is called when a response comes in to an outstanding
 * request.
 */
static void
ar_ce_resolve(ace_t *ace, uchar_t *hw_addr, uint32_t hw_addr_length)
{
	if (hw_addr_length == ace->ace_hw_addr_length) {
		if (ace->ace_hw_addr)
			bcopy(hw_addr, ace->ace_hw_addr, hw_addr_length);
		/*
		 * ar_query_reply() blows away soft entries.
		 * Do not call it unless something is waiting.
		 */
		ace->ace_flags |= ACE_F_RESOLVED;
		if (ace->ace_query_mp)
			ar_query_reply(ace, 0, NULL, (uint32_t)0);
	}
}

/* Pass arg1 to the pfi supplied, along with each ace in existence. */
static void
ar_ce_walk(pfi_t pfi, uchar_t *arg1)
{
	ace_t	*ace;
	ace_t	*ace1;
	ace_t	**acep;

	for (acep = ar_ce_hash_tbl; acep < A_END(ar_ce_hash_tbl); acep++) {
		/*
		 * We walk the hash chain in a way that allows the current
		 * ace to get blown off by the called routine.
		 */
		for (ace = *acep; ace; ace = ace1) {
			ace1 = ace->ace_next;
			(*pfi)(ace, arg1);
		}
	}
	for (ace = ar_ce_mask_entries; ace; ace = ace1) {
		ace1 = ace->ace_next;
		(*pfi)(ace, arg1);
	}
}

/* Free the ND tables if the last ar has gone away. */
static void
ar_cleanup(void)
{
	if (!ar_g_head)
		nd_free(&ar_g_nd);
}

/* Send a copy of interesting packets to all clients. */
static void
ar_client_notify(arl_t *arl, mblk_t *mp, int code)
{
	ar_t	*ar;
	arcn_t	*arcn;
	ar_t	*first_ar = NULL;
	mblk_t	*mp1;
	mblk_t	*mp2;

	for (ar = (ar_t *)mi_first_ptr(&ar_g_head); ar != NULL;
	    ar = (ar_t *)mi_next_ptr(&ar_g_head, (IDP)ar)) {
		if (ar->ar_arl || !ar->ar_wq->q_next)
			continue;
		if (!first_ar) {
			first_ar = ar;
			mp1 = allocb(sizeof (arcn_t) + arl->arl_name_length,
			    BPRI_MED);
			if (!mp1) {
				freemsg(mp);
				return;
			}
			mp1->b_datap->db_type = M_CTL;
			mp1->b_cont = mp;
			arcn = (arcn_t *)mp1->b_rptr;
			mp1->b_wptr = (uchar_t *)&arcn[1] +
			    arl->arl_name_length;
			arcn->arcn_cmd = AR_CLIENT_NOTIFY;
			arcn->arcn_name_offset = sizeof (arcn_t);
			arcn->arcn_name_length = arl->arl_name_length;
			arcn->arcn_code = code;
			bcopy(arl->arl_name, &arcn[1], arl->arl_name_length);
			continue;
		}
		mp2 = copymsg(mp1);
		if (!mp2)
			break;
		putnext(ar->ar_wq, mp2);
	}
	if (first_ar)
		putnext(first_ar->ar_wq, mp1);
	else
		freemsg(mp);
}

/* ARP module close routine. */
static int
ar_close(queue_t *q)
{
	ar_t	*ar = (ar_t *)q->q_ptr;
	arl_t	*arl;
	arl_t	**arlp;

	TRACE_1(TR_FAC_ARP, TR_ARP_CLOSE,
	    "arp_close: q %p", q);

	ARP_ENTER();
	/* Delete all our pending queries, 'arl' is not dereferenced */
	ar_ce_walk(ar_query_delete, (uchar_t *)ar);
	if ((arl = ar->ar_arl) != 0) {
		/*
		 * If this is the control stream for an arl, delete anything
		 * hanging off our arl.
		 */
		ar_ce_walk((pfi_t)ar_ce_delete_per_arl, (uchar_t *)arl);
		/* Free any messages waiting for a bind_ack */
		ar_ll_freequeue(arl);
		/* Get the arl out of the chain. */
		for (arlp = &arl_g_head; arlp[0]; arlp = &arlp[0]->arl_next) {
			if (arlp[0] == arl) {
				arlp[0] = arl->arl_next;
				break;
			}
		}
		if (arl->arl_xmit_template)
			freemsg(arl->arl_xmit_template);
		if (arl->arl_unbind_mp)
			putnext(arl->arl_wq, arl->arl_unbind_mp);
		if (arl->arl_detach_mp)
			putnext(arl->arl_wq, arl->arl_detach_mp);
		freemsg(arl->arl_attach_mp);
		freemsg(arl->arl_bind_mp);
		freemsg(arl->arl_dlpiop_done);
		mi_free((char *)ar->ar_arl);
	}
	if (WR(q) == ar_timer_queue) {
		/* We were using this one for the garbage collection timer. */
		for (arl = arl_g_head; arl; arl = arl->arl_next)
			if (arl->arl_rq != q)
				break;
		if (arl) {
			ar_timer_queue = arl->arl_wq;
			/* Ask mi_timer to switch to the new queue. */
			mi_timer(ar_timer_queue, ar_timer_mp, -2);
		} else {
			mi_timer_free(ar_timer_mp);
			ar_timer_mp = NULL;
			ar_timer_queue = NULL;
		}
	}
	/* mi_close_comm frees the instance data. */
	(void) mi_close_comm(&ar_g_head, q);
	ar_cleanup();
	qprocsoff(q);
	ARP_EXIT();
	return (0);
}

/*
 * Dispatch routine for ARP commands.  This routine can be called out of
 * either ar_wput or ar_rput, in response to IOCTLs or M_PROTO messages.
 */
/* TODO: error reporting for M_PROTO case */
static int
ar_cmd_dispatch(queue_t *q, mblk_t *mp_orig)
{
	arct_t	*arct;
	uint32_t	cmd;
	ssize_t	len;
	mblk_t	*mp = mp_orig;

	if (!mp)
		return (ENOENT);
	/* We get both M_PROTO and M_IOCTL messages, so watch out! */
	if (mp->b_datap->db_type == M_IOCTL) {
		cmd = ((struct iocblk *)mp->b_rptr)->ioc_cmd;
		mp = mp->b_cont;
		if (!mp)
			return (ENOENT);
	}
	len = mp->b_wptr - mp->b_rptr;
	if (len < sizeof (uint32_t) || !OK_32PTR(mp->b_rptr))
		return (ENOENT);
	if (mp_orig == mp)
		cmd = *(uint32_t *)mp->b_rptr;
	for (arct = ar_cmd_tbl; ; arct++) {
		if (arct >= A_END(ar_cmd_tbl))
			return (ENOENT);
		if (arct->arct_cmd == cmd)
			break;
	}
	if (len < arct->arct_min_len)
		return (EINVAL);
	if (arct->arct_priv_cmd && !((ar_t *)q->q_ptr)->ar_priv_stream)
		return (EPERM);
	if (arct->arct_ioctl_aware)
		mp = mp_orig;
	return (*arct->arct_pfi)(q, mp);
}

/* Allocate and do common initializations for DLPI messages. */
static mblk_t *
ar_dlpi_comm(t_uscalar_t prim, size_t size)
{
	uchar_t	*cp;
	mblk_t	*mp;

	mp = allocb(size, BPRI_HI);
	if (mp) {
		mp->b_datap->db_type = M_PROTO;
		cp = mp->b_rptr;
		mp->b_wptr = &cp[size];
		bzero(cp, size);
		((dl_bind_req_t *)cp)->dl_primitive = prim;
	}
	return (mp);
}

/* Process entry add requests from external messages. */
static int
ar_entry_add(queue_t *q, mblk_t *mp_orig)
{
	area_t	*area;
	ace_t	*ace;
	uchar_t	*hw_addr;
	uint32_t	hw_addr_len;
	uchar_t	*proto_addr;
	uint32_t	proto_addr_len;
	uchar_t	*proto_mask;
	arl_t	*arl;
	mblk_t	*mp = mp_orig;
	int	err;

	/* We handle both M_IOCTL and M_PROTO messages. */
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl)
		return (EINVAL);
	if (arl->arl_bind_pending || arl->arl_unbind_pending) {
		arp1dbg(("ar_entry_add: pending (%d,%d)\n",
		    arl->arl_bind_pending, arl->arl_unbind_pending));
		ar_ll_enqueue(arl, q, mp_orig);
		return (EINPROGRESS);
	}

	area = (area_t *)mp->b_rptr;
	/* If this is a replacement, ditch the original. */
	if ((ace = ar_ce_lookup_from_area(mp, ar_ce_lookup_entry)) != 0)
		ar_ce_delete(ace);
	/* Extract parameters from the message. */
	hw_addr_len = area->area_hw_addr_length;
	hw_addr = mi_offset_paramc(mp, area->area_hw_addr_offset, hw_addr_len);
	proto_addr_len = area->area_proto_addr_length;
	proto_addr = mi_offset_paramc(mp, area->area_proto_addr_offset,
	    proto_addr_len);
	proto_mask = mi_offset_paramc(mp, area->area_proto_mask_offset,
	    proto_addr_len);
	if (!proto_mask)
		return (EINVAL);
	err = ar_ce_create(
	    arl,
		area->area_proto,
		hw_addr,
		hw_addr_len,
		proto_addr,
		proto_addr_len,
		proto_mask,
		NULL,
		(uint32_t)0,
		area->area_flags & ~ACE_F_MAPPING);
	if (err)
		return (err);
	if (area->area_flags & ACE_F_PUBLISH) {
		/*
		 * Transmit an arp request for this address to flush stale
		 * information froma arp caches.
		 */
		if (hw_addr == NULL || hw_addr_len == 0)
			hw_addr = arl->arl_hw_addr;
		ar_xmit(arl, ARP_REQUEST, area->area_proto, proto_addr_len,
		    hw_addr, proto_addr, arl->arl_arp_addr,
		    proto_addr);
	}
	return (0);
}

/* Process entry delete requests from external messages. */
static int
ar_entry_delete(queue_t *q, mblk_t *mp_orig)
{
	ace_t	*ace;
	arl_t	*arl;
	mblk_t	*mp = mp_orig;

	/* We handle both M_IOCTL and M_PROTO messages. */
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl)
		return (EINVAL);
	if (arl->arl_bind_pending || arl->arl_unbind_pending) {
		arp1dbg(("ar_entry_delete: pending (%d,%d)\n",
		    arl->arl_bind_pending, arl->arl_unbind_pending));
		ar_ll_enqueue(arl, q, mp_orig);
		return (EINPROGRESS);
	}

	/*
	 * Need to know if it is a mapping or an exact match.  Check exact
	 * match first.
	 */
	ace = ar_ce_lookup_from_area(mp, ar_ce_lookup);
	if (ace) {
		ar_ce_delete(ace);
		return (0);
	}
	return (ENXIO);
}

/*
 * Process entry query requests from external messages.
 * Bump up the ire_stats_freed for all errors except
 * EINPROGRESS - which means the packet has been queued.
 * For all other errors the packet is going to be freed
 * and hence we account for ire being freed if it
 * is a M_PROTO message.
 */
static int
ar_entry_query(queue_t *q, mblk_t *mp_orig)
{
	ace_t	*ace;
	areq_t	*areq;
	arl_t	*arl;
	int	err;
	mblk_t	*mp = mp_orig;
	uchar_t	*proto_addr;
	uint32_t proto_addr_len;
	clock_t	ms;
	boolean_t is_mproto = B_TRUE;

	/* We handle both M_IOCTL and M_PROTO messages. */
	if (mp->b_datap->db_type == M_IOCTL) {
		is_mproto = B_FALSE;
		mp = mp->b_cont;
	}
	arl = ar_ll_lookup_from_mp(mp);
	if (arl == NULL) {
		err = EINVAL;
		goto err_ret;
	}
	if (arl->arl_bind_pending || arl->arl_unbind_pending) {
		arp1dbg(("ar_entry_query: pending (%d,%d)\n",
		    arl->arl_bind_pending, arl->arl_unbind_pending));
		ar_ll_enqueue(arl, q, mp_orig);
		return (EINPROGRESS);
	}
	areq = (areq_t *)mp->b_rptr;
	proto_addr_len = areq->areq_target_addr_length;
	proto_addr = mi_offset_paramc(mp, areq->areq_target_addr_offset,
	    proto_addr_len);
	if (proto_addr == 0) {
		err = EINVAL;
		goto err_ret;
	}
	/* Stash the reply queue pointer for later use. */
	mp->b_prev = (mblk_t *)OTHERQ(q);
	mp->b_next = NULL;
	if (areq->areq_xmit_interval == 0)
		areq->areq_xmit_interval = AR_DEF_XMIT_INTERVAL;
	ace = ar_ce_lookup(arl, areq->areq_proto, proto_addr, proto_addr_len);
	if (ace) {
		mblk_t	**mpp;
		uint32_t	count = 0;
		/*
		 * There is already a cache entry.  This means there is either
		 * a permanent entry, or address resolution is in progress.
		 * If the latter, there should be one or more queries queued
		 * up.	We link the current one in at the end, if there aren't
		 * too many outstanding.
		 */
		for (mpp = &ace->ace_query_mp; mpp[0]; mpp = &mpp[0]->b_next) {
			if (++count > areq->areq_max_buffered) {
				mp->b_prev = NULL;
				err = EALREADY;
				goto err_ret;
			}
		}
		/* Put us on the list. */
		mpp[0] = mp;
		if (count != 0) {
			/*
			 * If a query was already queued up, then we must not
			 * have an answer yet.
			 */
			return (EINPROGRESS);
		}
		if (ACE_RESOLVED(ace)) {
			/*
			 * We have an answer already (must be a permanent
			 * entry).
			 * Keep a dup of mp since proto_addr points to it
			 * and mp has been placed on the ace_query_mp list.
			 */
			mblk_t *mp1;

			mp1 = dupmsg(mp);
			ar_query_reply(ace, 0, proto_addr, proto_addr_len);
			freemsg(mp1);
			return (EINPROGRESS);
		}
		if (ace->ace_flags & ACE_F_MAPPING) {
			/* Should never happen */
			arp0dbg(("ar_entry_query: unresolved mapping\n"));
			mpp[0] = mp->b_next;
			err = ENXIO;
			goto err_ret;
		}
		if (!arl->arl_xmit_template) {
			/* Can't get help if we don't know how. */
			mpp[0] = NULL;
			mp->b_prev = NULL;
			err = ENXIO;
			goto err_ret;
		}
	} else {
		/* No ace yet.	Make one now.  (This is the common case.) */
		if (areq->areq_xmit_count == 0 || !arl->arl_xmit_template) {
			mp->b_prev = NULL;
			err = ENXIO;
			goto err_ret;
		}
		err = ar_ce_create(arl, areq->areq_proto, NULL, 0,
		    proto_addr, proto_addr_len, NULL,
		    NULL, (uint32_t)0,
		    areq->areq_flags);
		if (err) {
			mp->b_prev = NULL;
			goto err_ret;
		}
		ace = ar_ce_lookup(arl, areq->areq_proto, proto_addr,
		    proto_addr_len);
		if (!ace || ace->ace_query_mp) {
			/* Shouldn't happen! */
			mp->b_prev = NULL;
			err = ENXIO;
			goto err_ret;
		}
		ace->ace_query_mp = mp;
	}
	ms = ar_query_xmit(ace);
	if (ms == 0) {
		/* Immediate reply requested. */
		ar_query_reply(ace, ENXIO, NULL, (uint32_t)0);
	} else
		mi_timer(arl->arl_wq, ace->ace_mp, ms);
	return (EINPROGRESS);
err_ret:
	if (is_mproto)
		BUMP_IRE_STATS(ire_stats_v4, ire_stats_freed);
	return (err);
}

/* Handle simple query requests. */
static int
ar_entry_squery(queue_t *q, mblk_t *mp_orig)
{
	ace_t	*ace;
	area_t	*area;
	arl_t	*arl;
	uchar_t	*hw_addr;
	uint32_t	hw_addr_len;
	mblk_t	*mp = mp_orig;
	uchar_t	*proto_addr;
	int	proto_addr_len;

	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl)
		return (EINVAL);
	if (arl->arl_bind_pending || arl->arl_unbind_pending) {
		arp1dbg(("ar_entry_squery: pending (%d,%d)\n",
		    arl->arl_bind_pending, arl->arl_unbind_pending));
		ar_ll_enqueue(arl, q, mp_orig);
		return (EINPROGRESS);
	}
	/* Extract parameters from the request message. */
	area = (area_t *)mp->b_rptr;
	proto_addr_len = area->area_proto_addr_length;
	proto_addr = mi_offset_paramc(mp, area->area_proto_addr_offset,
	    proto_addr_len);
	hw_addr_len = area->area_hw_addr_length;
	hw_addr = mi_offset_paramc(mp, area->area_hw_addr_offset, hw_addr_len);
	if (!proto_addr || !hw_addr)
		return (EINVAL);
	ace = ar_ce_lookup(arl, area->area_proto, proto_addr, proto_addr_len);
	if (!ace)
		return (ENXIO);
	if (hw_addr_len < ace->ace_hw_addr_length)
		return (EINVAL);
	if (ACE_RESOLVED(ace)) {
		/* Got it, prepare the response. */
		/* We should look at the ace for the length */
		area->area_hw_addr_length = ace->ace_hw_addr_length;
		ar_set_address(ace, hw_addr, proto_addr, proto_addr_len);
	} else {
		/*
		 * We have an incomplete entry.	 Set the length to zero and
		 * just return out the flags.
		 */
		area->area_hw_addr_length = 0;
	}
	area->area_flags = ace->ace_flags;
	if (mp == mp_orig) {
		/* Non-ioctl case */
		/* TODO: change message type? */
		arp1dbg(("ar_entry_squery: qreply\n"));
		mp->b_datap->db_type = M_CTL; /* Caught by ip_wput */
		qreply(q, mp);
		return (EINPROGRESS);
	}
	return (0);
}

/* Make sure b_next and b_prev are null and then free the message */
static void
ar_freemsg(mblk_t *mp)
{
	mblk_t *mp1;

	for (mp1 = mp; mp1; mp1 = mp1->b_cont)
		mp1->b_prev = mp1->b_next = NULL;
	freemsg(mp);
}

/* Process an interface down causing us to detach and unbind. */
/* ARGSUSED */
static int
ar_interface_down(queue_t *q, mblk_t *mp)
{
	arl_t	*arl;

	arp1dbg(("ar_interface_down\n"));
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl) {
		arp1dbg(("ar_interface_down: no arl\n"));
		return (EINVAL);
	}

	(void) ar_ll_down(arl);
	return (0);
}


/* Process an interface up causing the info req sequence to start. */
/* ARGSUSED */
static int
ar_interface_up(queue_t *q, mblk_t *mp)
{
	arl_t	*arl;

	arp1dbg(("ar_interface_up\n"));
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl) {
		arp1dbg(("ar_interface_up: no arl\n"));
		return (EINVAL);
	}
	ASSERT(arl->arl_ip_pending_queue == NULL);
	arl->arl_ip_pending_queue = WR(q);
	return (ar_ll_up(arl));
}

/*
 * Enable an interface to
 * process of ARP_REQUEST and ARP_RESPONSE messages
 */
/* ARGSUSED */
static int
ar_interface_on(queue_t *q, mblk_t *mp)
{
	arl_t	*arl;

	arp1dbg(("ar_interface_on\\n"));
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl) {
		arp1dbg(("ar_interface_on: no arl\\n"));
		return (EINVAL);
	}
	/* Turn off the IFF_NOARP flag  and activate ARP */
	arl->arl_flags = 0;
	return (0);
}

/*
 * Disable an interface from processing
 * ARP_REQUEST and ARP_RESPONSE messages
 */
/* ARGSUSED */
static int
ar_interface_off(queue_t *q, mblk_t *mp)
{
	arl_t	*arl;

	arp1dbg(("ar_interface_off\\n"));
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl) {
		arp1dbg(("ar_interface_off: no arl\\n"));
		return (EINVAL);
	}
	/* Turn on the IFF_NOARP flag and deactivate ARP */
	arl->arl_flags = ARL_F_NOARP;
	return (0);
}

static int
ar_ll_down(arl_t *arl)
{
	mblk_t	*mp;

	arp1dbg(("ar_ll_down\n"));
	/* Free all arp entries for this interface */
	ar_ce_walk((pfi_t)ar_ce_delete_per_arl, (uchar_t *)arl);

	if (arl->arl_unbind_pending)
		return (EINPROGRESS);

	if ((mp = arl->arl_unbind_mp) != NULL) {
		arp1dbg(("ar_ll_down: unbind\n"));
		arl->arl_unbind_mp = mp->b_next;
		mp->b_next = NULL;
		putnext(arl->arl_wq, mp);
		arl->arl_unbind_pending = 1;
		return (EINPROGRESS);
	}
	if ((mp = arl->arl_detach_mp) != NULL) {
		arp1dbg(("ar_ll_down: detach\n"));
		arl->arl_detach_mp = mp->b_next;
		mp->b_next = NULL;
		putnext(arl->arl_wq, mp);
	}
	return (0);
}

static void
ar_ll_down_delayed(arl_t *arl)
{
	mblk_t	*mp;

	arp1dbg(("ar_ll_down_delayed\n"));

	if ((mp = arl->arl_detach_mp) != NULL) {
		arp1dbg(("ar_ll_down_delayed: detach\n"));
		arl->arl_detach_mp = mp->b_next;
		mp->b_next = NULL;
		putnext(arl->arl_wq, mp);
	}
}

/*
 * Queue messages that are deferred due to
 * pending bind req.
 * The queue has to be saved so that we can later can reply on the correct
 * queue. (The queue could be the one for the stream between arp and ip
 * for which there is no arl.)
 */
static void
ar_ll_enqueue(arl_t *arl, queue_t *q, mblk_t *mp)
{
	mblk_t	**mpp;

	arp1dbg(("ar_ll_enqueue\n"));
	mp->b_prev = (mblk_t *)q;
	mp->b_next = NULL;

	for (mpp = &arl->arl_queue; *mpp; mpp = &(*mpp)->b_next)
		;
	*mpp = mp;
}

/*
 * Free messages that have been deferred due to
 * pending bind req.
 */
static void
ar_ll_freequeue(arl_t *arl)
{
	mblk_t	*mp;

	arp1dbg(("ar_ll_freequeue\n"));

	while ((mp = arl->arl_queue) != 0) {
		arl->arl_queue = mp->b_next;
		mp->b_prev = mp->b_next = NULL;
		arp1dbg(("ar_ll_freequeue: free msg %d\n",
		    mp->b_datap->db_type));
		if (mp->b_datap->db_type == M_PROTO &&
		    *(uint32_t *)mp->b_rptr == AR_ENTRY_QUERY) {
			BUMP_IRE_STATS(ire_stats_v4, ire_stats_freed);
		}
		freemsg(mp);
	}
}

/*
 * Look up a lower level tap by name.  Note that the name_length includes
 * the null terminator.
 */
static arl_t *
ar_ll_lookup_by_name(uchar_t *name, uint32_t name_length)
{
	arl_t	*arl;

	for (arl = arl_g_head; arl; arl = arl->arl_next) {
		if (arl->arl_name_length == name_length) {
			int	i1 = name_length;
			uchar_t	*cp1 = name;
			uchar_t	*cp2 = arl->arl_name;

			while (*cp1++ == *cp2++) {
				if (--i1 == 0)
					return (arl);
			}
		}
	}
	return (NULL);
}

/*
 * Look up a lower level tap using parameters extracted from the common
 * portion of the ARP command.
 */
static arl_t *
ar_ll_lookup_from_mp(mblk_t *mp)
{
	arc_t	*arc = (arc_t *)mp->b_rptr;
	uchar_t	*name;
	uint32_t	name_length = arc->arc_name_length;

	name = mi_offset_param(mp, arc->arc_name_offset, name_length);
	if (!name)
		return (NULL);
	return (ar_ll_lookup_by_name(name, name_length));
}

/*
 * Handle messages that have been deferred due to
 * pending bind req.
 */
static void
ar_ll_runqueue(arl_t *arl)
{
	queue_t	*q;
	mblk_t	*mp;

	arp1dbg(("ar_ll_runqueue: bind_pending %d\n", arl->arl_bind_pending));

	if (arl->arl_bind_pending)
		return;
	while ((mp = arl->arl_queue) != 0) {
		arl->arl_queue = mp->b_next;
		q = (queue_t *)mp->b_prev;
		mp->b_prev = mp->b_next = NULL;
		arp1dbg(("ar_ll_runqueue: put msg %d\n",
		    mp->b_datap->db_type));
		if (!q) {
			arp0dbg(("ar_ll_runqueue: no queue\n"));
			if (mp->b_datap->db_type == M_PROTO &&
			    *(uint32_t *)mp->b_rptr == AR_ENTRY_QUERY) {
				BUMP_IRE_STATS(ire_stats_v4, ire_stats_freed);
			}
			freemsg(mp);
		} else
			put(q, mp);
	}
}

/*
 * This routine is called during module initialization when the DL_INFO_ACK
 * comes back from the device.	We set up defaults for all the device dependent
 * doo-dads we are going to need.  This will leave us ready to roll if we are
 * attempting auto-configuration.  Alternatively, these defaults can be
 * overidden by initialization procedures possessing higher intelligence.
 */
static void
ar_ll_subnet_defaults(queue_t *q, mblk_t *mp)
{
	ar_t	*ar = (ar_t *)q->q_ptr;
	ar_m_t	*arm;
	arl_t	*arl = NULL;
	dl_info_ack_t *dlia = (dl_info_ack_t *)mp->b_rptr;
	dl_unitdata_req_t *dlur;
	int	hw_addr_length;
	int	i1;
	char	*ll_name;
	size_t	name_length;
	mblk_t	*template_mp = NULL;
	uchar_t	*up;
	queue_t	*wq;
	t_scalar_t	sap_length;

	/* Sanity checks... */
	if (ar->ar_arl || !(wq = ar->ar_wq))
		goto bad;
	while (wq->q_next)
		wq = wq->q_next;
	if (!(ll_name = wq->q_qinfo->qi_minfo->mi_idname) || !*ll_name ||
	    !(arm = ar_m_lookup(dlia->dl_mac_type)))
		goto bad;
	/*
	 * We initialize based on parameters in the (currently) not too
	 * exhaustive ar_m_tbl.
	 */
	if (dlia->dl_version == DL_VERSION_2) {
		hw_addr_length = dlia->dl_brdcst_addr_length;
	} else {
		hw_addr_length = arm->ar_mac_hw_addr_length;
	}
	name_length = mi_strlen(ll_name);
#ifndef MAX
#define	MAX(a, b)	((a) < (b) ? (b) : (a))
#endif
	arl = (arl_t *)(mi_zalloc(sizeof (arl_t) +
	    (2 * hw_addr_length) + MAX(name_length + 4, LIFNAMSIZ)));
	if (!arl)
		goto bad;
	arl->arl_arp_hw_type = arm->ar_mac_arp_hw_type;

	/*
	 * Someday DLPI will provide the multicast address?  Meanwhile we
	 * assume an address of all ones, known to work on some popular
	 * networks.
	 */
	up = (uchar_t *)&arl[1];
	arl->arl_arp_addr = up;
	if (dlia->dl_version == DL_VERSION_2) {
		uchar_t *up2;

		up2 = mi_offset_param(mp, dlia->dl_brdcst_addr_offset,
		    hw_addr_length);
		if (!up2) {
			goto bad;
		}
		bcopy(up2, up, hw_addr_length);
		up += hw_addr_length;
		/*
		 * TODO Note that sap_length can be 0 before binding according
		 * to the DLPI spec.
		 */
		sap_length = dlia->dl_sap_length;
	} else {
		for (i1 = 0; i1 < hw_addr_length; i1++)
			*up++ = (char)~0;
		sap_length = arm->ar_mac_sap_length;
	}
	/*
	 * The hardware address will be filled in when we see the DL_BIND_ACK.
	 * We reserve space for it here, and make arl_hw_addr point to it.
	 */
	arl->arl_hw_addr = up;
	arl->arl_hw_addr_length = hw_addr_length;
	up += arl->arl_hw_addr_length;

	/*
	 * Construct the name for this arl based on the module name of the
	 * device downstream and the ppa number.  We hand out ppa numbers
	 * in ascending order from zero as we see repeated opens of the
	 * same device.	 This means that in order to talk to the second
	 * device of a particular type (e.g., le1), we must see at least
	 * two opens of /dev/le.
	 */
	arl->arl_name = up;
	arl->arl_ppa = (unsigned)-1;
	do {
		if (++arl->arl_ppa >= 1000)
			goto bad;
		(void) mi_sprintf((char *)arl->arl_name, "%s%d", ll_name,
		    arl->arl_ppa);
		arl->arl_name_length = mi_strlen((char *)arl->arl_name) + 1;
	} while (ar_ll_lookup_by_name(arl->arl_name, arl->arl_name_length));

	/* Save things for ar_ll_up */
	arl->arl_provider_style = dlia->dl_provider_style;
	arl->arl_mac_sap = arm->ar_mac_sap;

	/*
	 * Make us a template DL_UNITDATA_REQ message which we will use for
	 * broadcasting resolution requests, and which we will clone to hand
	 * back as responses to the protocols.
	 */
	if (sap_length < 0) {
		arl->arl_xmit_template_sap_length = -sap_length;
	} else {
		/* The sap is first in the address */
		arl->arl_xmit_template_sap_length = sap_length;
	}
	template_mp = ar_dlpi_comm(DL_UNITDATA_REQ,
	    sizeof (dl_unitdata_req_t) +
	    arl->arl_hw_addr_length +
	    arl->arl_xmit_template_sap_length);
	if (!template_mp)
		goto bad;
	dlur = (dl_unitdata_req_t *)template_mp->b_rptr;
	dlur->dl_priority.dl_min = 0;
	dlur->dl_priority.dl_max = 0;
	dlur->dl_dest_addr_length = hw_addr_length +
	    arl->arl_xmit_template_sap_length;
	dlur->dl_dest_addr_offset = sizeof (dl_unitdata_req_t);

	/* Note the destination address offset permanently in the arl. */
	if (sap_length < 0) {
		arl->arl_xmit_template_addr_offset = dlur->dl_dest_addr_offset;
		arl->arl_xmit_template_sap_offset = dlur->dl_dest_addr_offset +
		    dlur->dl_dest_addr_length -
		    arl->arl_xmit_template_sap_length;
	} else {
		/* The sap is first in the address */
		arl->arl_xmit_template_addr_offset = dlur->dl_dest_addr_offset
		    + sap_length;
		arl->arl_xmit_template_sap_offset = dlur->dl_dest_addr_offset;
	}
	/* Copy in the ARP SAP value. */
	bcopy((char *)&arm->ar_mac_sap + sizeof (arm->ar_mac_sap) -
		arl->arl_xmit_template_sap_length,
		(char *)dlur + arl->arl_xmit_template_sap_offset,
		arl->arl_xmit_template_sap_length);

	arl->arl_xmit_template = template_mp;
	arl->arl_rq = ar->ar_rq;
	arl->arl_wq = ar->ar_wq;
	ASSERT(arl->arl_dlpiop_done == NULL);
	arl->arl_dlpiop_done = ar_alloc(AR_DLPIOP_DONE);
	if (arl->arl_dlpiop_done == NULL)
			goto bad;
	/* Chain in the new arl. */
	arl->arl_next = arl_g_head;
	arl_g_head = arl;
	ar->ar_arl = arl;

	freemsg(mp);
	return;
bad:
	if (arl)
		mi_free((char *)arl);
	if (template_mp)
		freemsg(template_mp);
	freemsg(mp);
}

/*
 * This routine is called during module initialization when the AR_INTERFACE_UP
 * comes from IP.
 */
static int
ar_ll_up(arl_t *arl)
{
	mblk_t	*attach_mp = NULL;
	mblk_t	*bind_mp = NULL;
	mblk_t	*detach_mp = NULL;
	mblk_t	*unbind_mp = NULL;
	int inprogress;

	arp1dbg(("ar_ll_up: bind_pending %d\n",
	    arl->arl_bind_pending));

	/* Make sure we are down */
	inprogress = (ar_ll_down(arl) == EINPROGRESS);

	if (arl->arl_provider_style == DL_STYLE2) {
		attach_mp =
		    ar_dlpi_comm(DL_ATTACH_REQ, sizeof (dl_attach_req_t));
		if (attach_mp == NULL)
			goto bad;
		((dl_attach_req_t *)attach_mp->b_rptr)->dl_ppa =
		    arl->arl_ppa;
		detach_mp =
		    ar_dlpi_comm(DL_DETACH_REQ, sizeof (dl_detach_req_t));
		if (detach_mp == NULL)
			goto bad;
	}

	/* Allocate and initialize a bind message. */
	bind_mp = ar_dlpi_comm(DL_BIND_REQ, sizeof (dl_bind_req_t));
	if (bind_mp == NULL)
		goto bad;
	((dl_bind_req_t *)bind_mp->b_rptr)->dl_sap = arl->arl_mac_sap;
	((dl_bind_req_t *)bind_mp->b_rptr)->dl_service_mode = DL_CLDLS;

	unbind_mp = ar_dlpi_comm(DL_UNBIND_REQ, sizeof (dl_unbind_req_t));
	if (unbind_mp == NULL)
		goto bad;

	/*
	 * Since all detach and unbind messages are indentical we can
	 * safely queue them in reverse order.
	 */
	if (detach_mp != NULL) {
		detach_mp->b_next = arl->arl_detach_mp;
		arl->arl_detach_mp = detach_mp;
	}
	unbind_mp->b_next = arl->arl_unbind_mp;
	arl->arl_unbind_mp = unbind_mp;

	if (inprogress) {
		arl->arl_attach_mp = attach_mp;
		arl->arl_bind_mp = bind_mp;
	} else {
		if (attach_mp != NULL)
			putnext(arl->arl_wq, attach_mp);
		arl->arl_bind_pending++;
		arp1dbg(("ar_ll_up: bind_pending %d\n", arl->arl_bind_pending));
		putnext(arl->arl_wq, bind_mp);
	}
	return (0);
bad:
	if (attach_mp != NULL)
		freemsg(attach_mp);
	if (bind_mp != NULL)
		freemsg(bind_mp);
	if (detach_mp != NULL)
		freemsg(detach_mp);
	if (unbind_mp != NULL)
		freemsg(unbind_mp);
	return (ENOMEM);
}

static void
ar_ll_up_delayed(arl_t *arl)
{
	mblk_t	*mp;

	arp1dbg(("ar_ll_up_delayed: bind_pending %d\n",
	    arl->arl_bind_pending));
	if ((mp = arl->arl_attach_mp) != NULL) {
		putnext(arl->arl_wq, mp);
		arl->arl_attach_mp = NULL;
	}
	if ((mp = arl->arl_bind_mp) != NULL) {
		arl->arl_bind_pending++;
		arp1dbg(("ar_ll_up_delayed: bind_pending %d\n",
		    arl->arl_bind_pending));
		putnext(arl->arl_wq, mp);
		arl->arl_bind_mp = NULL;
	}
}

/* Process mapping add requests from external messages. */
static int
ar_mapping_add(queue_t *q, mblk_t *mp_orig)
{
	arma_t	*arma;
	mblk_t	*mp = mp_orig;
	ace_t	*ace;
	uchar_t	*hw_addr;
	uint32_t	hw_addr_len;
	uchar_t	*proto_addr;
	uint32_t	proto_addr_len;
	uchar_t	*proto_mask;
	uchar_t	*proto_extract_mask;
	uint32_t	hw_extract_start;
	arl_t	*arl;

	arp1dbg(("ar_mapping_add\n"));
	/* We handle both M_IOCTL and M_PROTO messages. */
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl)
		return (EINVAL);
	if (arl->arl_bind_pending || arl->arl_unbind_pending) {
		arp1dbg(("ar_mapping_add: pending (%d,%d)\n",
		    arl->arl_bind_pending, arl->arl_unbind_pending));
		ar_ll_enqueue(arl, q, mp_orig);
		return (EINPROGRESS);
	}
	arma = (arma_t *)mp->b_rptr;
	if ((ace = ar_ce_lookup_from_area(mp, ar_ce_lookup_mapping)) != 0)
		ar_ce_delete(ace);
	hw_addr_len = arma->arma_hw_addr_length;
	hw_addr = mi_offset_paramc(mp, arma->arma_hw_addr_offset, hw_addr_len);
	proto_addr_len = arma->arma_proto_addr_length;
	proto_addr = mi_offset_paramc(mp, arma->arma_proto_addr_offset,
	    proto_addr_len);
	proto_mask = mi_offset_paramc(mp, arma->arma_proto_mask_offset,
	    proto_addr_len);
	proto_extract_mask = mi_offset_paramc(mp,
	    arma->arma_proto_extract_mask_offset, proto_addr_len);
	hw_extract_start = arma->arma_hw_mapping_start;
	if (!proto_mask || !proto_extract_mask) {
		arp0dbg(("ar_mapping_add: not masks\n"));
		return (EINVAL);
	}
	return (ar_ce_create(
	    arl,
		arma->arma_proto,
		hw_addr,
		hw_addr_len,
		proto_addr,
		proto_addr_len,
		proto_mask,
		proto_extract_mask,
		hw_extract_start,
		arma->arma_flags | ACE_F_MAPPING));
}

static boolean_t
ar_mask_all_ones(uchar_t *mask, uint32_t mask_len)
{
	if (mask == NULL)
		return (B_TRUE);

	while (mask_len-- > 0) {
		if (*mask++ != 0xFF) {
			return (B_FALSE);
		}
	}
	return (B_TRUE);
}

/* Find an entry for a particular MAC type in the ar_m_tbl. */
static ar_m_t	*
ar_m_lookup(t_uscalar_t mac_type)
{
	ar_m_t	*arm;

	for (arm = ar_m_tbl; arm < A_END(ar_m_tbl); arm++) {
		if (arm->ar_mac_type == mac_type)
			return (arm);
	}
	return (NULL);
}

/* Respond to Named Dispatch requests. */
static int
ar_nd_ioctl(queue_t *q, mblk_t *mp)
{
	if (mp->b_datap->db_type == M_IOCTL && nd_getset(q, ar_g_nd, mp))
		return (0);
	return (ENOENT);
}

/* ARP module open routine. */
static int
ar_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	ar_t	*ar;
	int	err;

	TRACE_1(TR_FAC_ARP, TR_ARP_OPEN,
	    "arp_open: q %p", q);
	ARP_ENTER();
	/* Allow a reopen. */
	if (q->q_ptr) {
		ARP_EXIT();
		return (0);
	}
	/* Load up the Named Dispatch tables, if not already done. */
	if (!ar_g_nd &&
	    (!nd_load(&ar_g_nd, "arp_cache_report", ar_ce_report, NULL,
		NULL) ||
		!ar_param_register(arp_param_arr, A_CNT(arp_param_arr)))) {
		ar_cleanup();
		ARP_EXIT();
		return (ENOMEM);
	}
	/* mi_open_comm allocates the instance data structure, etc. */
	err = mi_open_comm(&ar_g_head, sizeof (ar_t), q, devp, flag, sflag,
	    credp);
	if (err) {
		ar_cleanup();
		ARP_EXIT();
		return (err);
	}
	/*
	 * We are D_MTPERMOD so it is safe to do qprocson before
	 * the instance data has been initialized.
	 */
	qprocson(q);

	ar = (ar_t *)q->q_ptr;
	ar->ar_rq = q;
	q = WR(q);
	ar->ar_wq = q;
	if (credp && drv_priv(credp) == 0)
		ar->ar_priv_stream = B_TRUE;
	if (!ar_timer_mp)
		ar_timer_init(q);
	/*
	 * Probe for the DLPI info if we are not pushed on IP. Wait for
	 * the reply.
	 */
	if (q->q_next &&
	    strcmp(q->q_next->q_qinfo->qi_minfo->mi_idname, "ip") != 0) {
		/*
		 * Send down a DL_INFO_REQ so we can find out what we are
		 * talking to.
		 */
		mblk_t *mp = ar_dlpi_comm(DL_INFO_REQ, sizeof (dl_info_req_t));
		if (!mp) {
			qprocsoff(ar->ar_rq);
			(void) mi_close_comm(&ar_g_head, ar->ar_rq);
			ar_cleanup();
			ARP_EXIT();
			return (ENOMEM);
		}
		putnext(ar->ar_wq, mp);
		while (ar->ar_arl == NULL) {
			ARP_EXIT();
			if (!qwait_sig(ar->ar_rq)) {
				qprocsoff(ar->ar_rq);
				return (EINTR);
			}
			ARP_ENTER();
		}
	}
	ARP_EXIT();
	return (0);
}

/* Get current value of Named Dispatch item. */
/* ARGSUSED */
static int
ar_param_get(queue_t *q, mblk_t *mp, void *cp)
{
	arpparam_t	*arppa = (arpparam_t *)cp;

	(void) mi_mpprintf(mp, "%d", arppa->arp_param_value);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * named dispatch handler.
 */
static boolean_t
ar_param_register(arpparam_t *arppa, int cnt)
{
	for (; cnt-- > 0; arppa++) {
		if (arppa->arp_param_name && arppa->arp_param_name[0]) {
			if (!nd_load(&ar_g_nd, arppa->arp_param_name,
			    ar_param_get, ar_param_set,
			    (caddr_t)arppa)) {
				nd_free(&ar_g_nd);
				return (B_FALSE);
			}
		}
	}
	return (B_TRUE);
}

/* Set new value of Named Dispatch item. */
/* ARGSUSED */
static int
ar_param_set(queue_t *q, mblk_t *mp, char *value, void *cp)
{
	char	*end;
	int	new_value;
	arpparam_t	*arppa = (arpparam_t *)cp;

	new_value = (int)mi_strtol(value, &end, 10);
	if (end == value || new_value < arppa->arp_param_min ||
	    new_value > arppa->arp_param_max)
		return (EINVAL);
	arppa->arp_param_value = new_value;
	return (0);
}

/*
 * ar_ce_walk routine to delete any outstanding queries for an ar that is
 * going away.
 */
static int
ar_query_delete(ace_t *ace, uchar_t *ar)
{
	mblk_t	**mpp = &ace->ace_query_mp;
	mblk_t	*mp = mpp[0];

	if (!mp)
		return (0);
	do {
		/* The response queue was stored in the query b_prev. */
		if ((queue_t *)mp->b_prev == ((ar_t *)ar)->ar_wq ||
		    (queue_t *)mp->b_prev == ((ar_t *)ar)->ar_rq) {
			mpp[0] = mp->b_next;
			if (mp->b_datap->db_type == M_PROTO &&
			    *(uint32_t *)mp->b_rptr == AR_ENTRY_QUERY) {
				BUMP_IRE_STATS(ire_stats_v4, ire_stats_freed);
			}
			ar_freemsg(mp);
		} else {
			mpp = &mp->b_next;
		}
	} while ((mp = mpp[0]) != 0);
	return (0);
}

/*
 * This routine is called either when an address resolution has just been
 * found, or when it is time to give, or in some other error situation.
 * If a non-zero ret_val is provided, any outstanding queries for the
 * specified ace will be completed using that error value.  Otherwise,
 * the completion status will depend on whether the address has been
 * resolved.
 */
static void
ar_query_reply(ace_t *ace, int ret_val, uchar_t *proto_addr,
    uint32_t proto_addr_len)
{
	mblk_t	*areq_mp;
	arl_t	*arl = ace->ace_arl;
	mblk_t	*mp;
	mblk_t	*template;

	/* Cancel any outstanding timer. */
	mi_timer(arl->arl_wq, ace->ace_mp, -1L);
	/* Establish the return value appropriate. */
	if (ret_val == 0) {
		if (!ACE_RESOLVED(ace) || !arl->arl_xmit_template)
			ret_val = ENXIO;
	}
	/* Terminate all outstanding queries. */
	while ((mp = ace->ace_query_mp) != 0) {
		/* The response queue was saved in b_prev. */
		queue_t	*q = (queue_t *)mp->b_prev;
		mp->b_prev = NULL;
		ace->ace_query_mp = mp->b_next;
		mp->b_next = NULL;
		/*
		 * If we have the answer, attempt to get a copy of the xmit
		 * template to prepare for the client.
		 */
		if (ret_val == 0 &&
		    !(template = copyb(arl->arl_xmit_template))) {
			/* Too bad, buy more memory. */
			ret_val = ENOMEM;
		}
		/* Complete the response based on how the request arrived. */
		if (mp->b_datap->db_type == M_IOCTL) {
			struct iocblk *ioc =
			    (struct iocblk *)mp->b_rptr;
			ioc->ioc_error = ret_val;
			mp->b_datap->db_type = M_IOCACK;
			if (ret_val != 0) {
				ioc->ioc_count = 0;
				putnext(q, mp);
				continue;
			}
			/*
			 * Return the xmit template out with the successful
			 * IOCTL.
			 */
			ioc->ioc_count = template->b_wptr - template->b_rptr;
			/* Remove the areq mblk from the IOCTL. */
			areq_mp = mp->b_cont;
			mp->b_cont = areq_mp->b_cont;
		} else {
			if (ret_val != 0) {
				/* TODO: find some way to let the guy know? */
				ar_freemsg(mp);
				BUMP_IRE_STATS(ire_stats_v4, ire_stats_freed);
				continue;
			}
			/*
			 * In the M_PROTO case, the areq message is followed by
			 * a message chain to be returned to the protocol.  ARP
			 * doesn't know (or care) what is in this chain, but in
			 * the event that the reader is pondering the
			 * relationship between ARP and IP (for example), the
			 * areq is followed by an incipient IRE, and then the
			 * original outbound packet.  Here we detach the areq.
			 */
			areq_mp = mp;
			mp = mp->b_cont;
		}
		if (arl->arl_xmit_template_sap_length > 0) {
			/*
			 * Copy the SAP type specified in the request into
			 * the xmit template.
			 */
			areq_t	*areq = (areq_t *)areq_mp->b_rptr;
			bcopy(&areq->areq_sap[0],
			    (char *)template->b_rptr +
			    arl->arl_xmit_template_sap_offset,
			    arl->arl_xmit_template_sap_length);
		}
		/* Done with the areq message. */
		freeb(areq_mp);
		/*
		 * Copy the resolved hardware address into the xmit template
		 * or perform the mapping operation.
		 */
		ar_set_address(ace, (uchar_t *)template->b_rptr
		    + arl->arl_xmit_template_addr_offset,
		    proto_addr, proto_addr_len);
		/*
		 * Now insert the xmit template after the response message.  In
		 * the M_IOCTL case, it will be the returned data block.  In
		 * the M_PROTO case, (again using IP as an example) it will
		 * appear after the IRE and before the outbound packet.
		 */
		template->b_cont = mp->b_cont;
		mp->b_cont = template;
		putnext(q, mp);
	}
	/*
	 * Unless we are responding from a permanent cache entry, delete
	 * the ace.
	 */
	if (!(ace->ace_flags & (ACE_F_PERMANENT | ACE_F_DYING))) {
		ar_ce_delete(ace);
	}
}

/*
 * Returns number of milliseconds after which we should either rexmit or abort.
 * Return of zero means we should abort.
 */
static clock_t
ar_query_xmit(ace_t *ace)
{
	areq_t	*areq;
	arl_t	*arl;
	mblk_t	*mp;
	uchar_t	*proto_addr;
	uchar_t	*sender_addr;

	mp = ace->ace_query_mp;
	if (!mp)
		return (0);
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	areq = (areq_t *)mp->b_rptr;
	if (areq->areq_xmit_count == 0)
		return (0);
	areq->areq_xmit_count--;
	proto_addr = mi_offset_paramc(mp, areq->areq_target_addr_offset,
	    areq->areq_target_addr_length);
	sender_addr = mi_offset_paramc(mp, areq->areq_sender_addr_offset,
	    areq->areq_sender_addr_length);
	arl = ace->ace_arl;
	ar_xmit(arl, ARP_REQUEST, areq->areq_proto,
	    areq->areq_sender_addr_length, arl->arl_hw_addr, sender_addr,
	    arl->arl_arp_addr, proto_addr);
	return (areq->areq_xmit_interval);
}

/* Our read side put procedure. */
static void
ar_rput(queue_t *q, mblk_t *mp)
{
	arh_t	*arh;
	arl_t	*arl;
	ace_t	*dst_ace;
	uchar_t	*dst_paddr;
	int	err;
	uint32_t	hlen;
	struct iocblk	*ioc;
	mblk_t	*mp1;
	int	op;
	uint32_t	plen;
	uint32_t	proto;
	ace_t	*src_ace;
	uchar_t	*src_haddr;
	uchar_t	*src_paddr;
	dl_unitdata_ind_t	*dlui;

	TRACE_1(TR_FAC_ARP, TR_ARP_RPUT_START,
	    "arp_rput_start: q %p", q);

	ARP_ENTER();
	/*
	 * We handle ARP commands from below both in M_IOCTL and M_PROTO
	 * messages.  Actual ARP requests and responses will show up as
	 * M_PROTO messages containing DL_UNITDATA_IND blocks.
	 */
	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		err = ar_cmd_dispatch(q, mp);
		switch (err) {
		case ENOENT:
			mp->b_datap->db_type = M_IOCNAK;
			if ((mp1 = mp->b_cont) != 0) {
				/*
				 * Collapse the data as a note to the
				 * originator.
				 */
				mp1->b_wptr = mp1->b_rptr;
			}
			break;
		case EINPROGRESS:
			TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
			    "arp_rput_end: q %p (%S)", q, "ioctl/inprogress");
			ARP_EXIT();
			return;
		default:
			mp->b_datap->db_type = M_IOCACK;
			break;
		}
		ioc = (struct iocblk *)mp->b_rptr;
		ioc->ioc_error = err;
		if ((mp1 = mp->b_cont) != 0)
			ioc->ioc_count = mp1->b_wptr - mp1->b_rptr;
		else
			ioc->ioc_count = 0;
		qreply(q, mp);
		TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
		    "arp_rput_end: q %p (%S)", q, "ioctl");
		ARP_EXIT();
		return;
	case M_PCPROTO:
	case M_PROTO:
		err = ar_cmd_dispatch(q, mp);
		switch (err) {
		case ENOENT:
			break;
		case EINPROGRESS:
			TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
			    "arp_rput_end: q %p (%S)", q, "proto");
			ARP_EXIT();
			return;
		default:
			ar_freemsg(mp);
			ARP_EXIT();
			return;
		}
		if ((mp->b_wptr - mp->b_rptr) < sizeof (dl_unitdata_ind_t) ||
		    ((dl_unitdata_ind_t *)mp->b_rptr)->dl_primitive
		    != DL_UNITDATA_IND) {
			/* Miscellaneous DLPI messages get shuffled off. */
			ar_rput_dlpi(q, mp);
			TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
			    "arp_rput_end: q %p (%S)", q, "proto/dlpi");
			ARP_EXIT();
			return;
		}
		/* DL_UNITDATA_IND */
		arl = ((ar_t *)q->q_ptr)->ar_arl;
		if (arl) {
			/* Real messages from the wire! */
			break;
		}
		/* FALLTHRU */
	default:
		putnext(q, mp);
		TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
		    "arp_rput_end: q %p (%S)", q, "default");
		ARP_EXIT();
		return;
	}
	/*
	 * If the IFF_NOARP flag is on, then do not process any
	 * incoming ARP_REQUEST or incoming ARP_RESPONSE.
	 */
	if (arl->arl_flags & ARL_F_NOARP) {
		freemsg(mp);
		TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
		"arp_rput_end: q %p (%S)", q, "interface has IFF_NOARP set");
		ARP_EXIT();
		return;
	}

	/*
	 * What we should have at this point is a DL_UNITDATA_IND message
	 * followed by an ARP packet.  We do some initial checks and then
	 * get to work.
	 */
	dlui = (dl_unitdata_ind_t *)mp->b_rptr;
	mp1 = mp->b_cont;
	if (!mp1) {
		freemsg(mp);
		TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
		    "arp_rput_end: q %p (%S)", q, "baddlpi");
		ARP_EXIT();
		return;
	}
	if (!OK_32PTR(mp1->b_rptr) || mp1->b_cont) {
		/* No fooling around with funny messages. */
		if (!pullupmsg(mp1, -1)) {
			freemsg(mp);
			TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
			    "arp_rput_end: q %p (%S)", q, "pullupmsgfail");
			ARP_EXIT();
			return;
		}
	}
	arh = (arh_t *)mp1->b_rptr;
	hlen = (uint32_t)arh->arh_hlen & 0xFF;
	plen = (uint32_t)arh->arh_plen & 0xFF;
	if ((mp1->b_wptr - mp1->b_rptr)
	    < (ARH_FIXED_LEN + hlen + hlen + plen + plen)) {
		freemsg(mp);
		TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
		    "arp_rput_end: q %p (%S)", q, "short");
		ARP_EXIT();
		return;
	}
	if (hlen == 0 || plen == 0) {
		arp1dbg(("ar_rput: bogus arh\n"));
		freemsg(mp);
		TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
		    "arp_rput_end: q %p (%S)", q, "hlenzero/plenzero");
		ARP_EXIT();
		return;
	}
	proto = (uint32_t)BE16_TO_U16(arh->arh_proto);
	src_haddr = (uchar_t *)arh;
	src_haddr = &src_haddr[ARH_FIXED_LEN];
	src_paddr = &src_haddr[hlen];
	dst_paddr = &src_haddr[hlen + plen + hlen];
	op = BE16_TO_U16(arh->arh_operation);

	/* Now see if we have a cache entry for the source address. */
	src_ace = ar_ce_lookup_entry(arl, proto, src_paddr, plen);
	/*
	 * If so, and it is the entry for one of our IP addresses,
	 * we really don't expect to see this packet, so pretend we didn't.
	 * Tell IP that we received a bogon.
	 *
	 * If is a "published" (proxy arp) entry we can receive requests
	 * FROM the node but we should never see an ARP_RESPONSE. In this case
	 * we process the response but also inform IP.
	 */
	if (src_ace) {
		if (src_ace->ace_flags & ACE_F_MYADDR) {
			freeb(mp);
			ar_client_notify(arl, mp1, AR_CN_BOGON);
			TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
			    "arp_rput_end: q %p (%S)", q, "pubentry");
			ARP_EXIT();
			return;
		}
		if ((src_ace->ace_flags & ACE_F_PUBLISH) &&
		    op == ARP_RESPONSE) {
			mblk_t *mp2;

			mp2 = copymsg(mp1);
			if (mp2 != NULL)
				ar_client_notify(arl, mp2, AR_CN_BOGON);
		}
	}
	switch (op) {
	case ARP_REQUEST:
		/*
		 * If we know the answer, and it is "published", send out
		 * the response.
		 */
		dst_ace = ar_ce_lookup_entry(arl, proto, dst_paddr, plen);
		if (dst_ace && (dst_ace->ace_flags & ACE_F_PUBLISH) &&
		    ACE_RESOLVED(dst_ace)) {
			ar_xmit(arl, ARP_RESPONSE, dst_ace->ace_proto, plen,
			    dst_ace->ace_hw_addr, dst_ace->ace_proto_addr,
			    src_haddr, src_paddr);
		}
		/*
		 * Now fall through to the response side, and add a cache entry
		 * for the sender so we will have it when we need it.
		 */
		/* FALLTHRU */
	case ARP_RESPONSE:
		if (src_ace) {
			/* This may be one we are waiting for. */
			ar_ce_resolve(src_ace, src_haddr, hlen);
		} else {
			/* We may need this one sooner or later. */
			(void) ar_ce_create(arl, proto, src_haddr, hlen,
			    src_paddr, plen, NULL,
			    NULL, (uint32_t)0,
			    (uint32_t)0);
		}
		/* Let's see if this is a system ARPing itself. */
		do {
			if (*src_paddr++ != *dst_paddr++)
				break;
		} while (--plen);
		if (plen == 0) {
			/*
			 * An ARP message with identical src and dst
			 * protocol addresses.	This guy is trying to
			 * tell us something that our clients might
			 * find interesting.
			 */
			freeb(mp);
			ar_client_notify(arl, mp1, AR_CN_ANNOUNCE);
			TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
			    "arp_rput_end: q %p (%S)", q, "duplicate");
			ARP_EXIT();
			return;
		}
		/*
		 * A broadcast response may also be interesting.
		 */
		if (op == ARP_RESPONSE && dlui->dl_group_address) {
			freeb(mp);
			ar_client_notify(arl, mp1, AR_CN_ANNOUNCE);
			ARP_EXIT();
			return;
		}
		break;
	default:
		break;
	}
	freemsg(mp);
	TRACE_2(TR_FAC_ARP, TR_ARP_RPUT_END,
	    "arp_rput_end: q %p (%S)", q, "end");
	ARP_EXIT();
}

/* DLPI messages, other than DL_UNITDATA_IND are handled here. */
static void
ar_rput_dlpi(queue_t *q, mblk_t *mp)
{
	arl_t		*arl = ((ar_t *)q->q_ptr)->ar_arl;
	dl_bind_ack_t	*dlba;
	dl_error_ack_t	*dlea;
	dl_ok_ack_t	*dloa;
	dl_uderror_ind_t *dluei;
	char		*err_str;

	if ((mp->b_wptr - mp->b_rptr) < sizeof (dloa->dl_primitive)) {
		putnext(q, mp);
		return;
	}
	dloa = (dl_ok_ack_t *)mp->b_rptr;
	dlea = (dl_error_ack_t *)dloa;
	switch (dloa->dl_primitive) {
	case DL_ERROR_ACK:
		switch (dlea->dl_error_primitive) {
		case DL_UNBIND_REQ:
			arl->arl_unbind_pending = 0;
			/* Complete deferred work */
			ar_ll_down_delayed(arl);
			ar_ll_up_delayed(arl);
			err_str = "DL_UNBIND_REQ";
			break;
		case DL_DETACH_REQ:
			err_str = "DL_DETACH_REQ";
			break;
		case DL_ATTACH_REQ:
			err_str = "DL_ATTACH_REQ";
			break;
		case DL_BIND_REQ:
			arp1dbg(("Error to bind_req: bind_pending %d\n",
			    arl->arl_bind_pending));
			arl->arl_bind_pending--;
			if (arl->arl_bind_pending == 0) {
				arp1dbg(("Error to DL_BIND_REQ: freequeue\n"));
				ar_ll_freequeue(arl);
			}
			err_str = "DL_BIND_REQ";
			/*
			 * There is an ioctl waiting for us...
			 */
			if (arl->arl_dlpiop_done == NULL) {
				arl->arl_dlpiop_done =
				    ar_alloc(AR_DLPIOP_DONE);
			}
			if (arl->arl_dlpiop_done != NULL &&
			    arl->arl_ip_pending_queue != NULL) {
				putnext(arl->arl_ip_pending_queue,
				    arl->arl_dlpiop_done);
				arl->arl_dlpiop_done = NULL;
				arl->arl_ip_pending_queue = NULL;
			}
			break;
		default:
			err_str = "DL_???";
			break;
		}
		arp0dbg(("ar_rput_dlpi: "
		    "%s (%d) failed, dl_errno %d, dl_unix_errno %d\n",
		    err_str, (int)dlea->dl_error_primitive,
		    (int)dlea->dl_errno, (int)dlea->dl_unix_errno));
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "ar_rput_dlpi: %s failed, dl_errno %d, dl_unix_errno %d",
		    err_str, dlea->dl_errno, dlea->dl_unix_errno);
		break;
	case DL_INFO_ACK:
		/*
		 * We have a response back from the driver.  Go set up transmit
		 * defaults.
		 */
		ar_ll_subnet_defaults(q, mp);
		/* Kick off any awaiting messages */
		arp1dbg(("ar_rput: qenable\n"));
		qenable(WR(q));
		return;
	case DL_OK_ACK:
		switch (dloa->dl_correct_primitive) {
		case DL_UNBIND_REQ:
			arp1dbg((
			    "ar_rput: DL_OK_ACK/UNBIND - unbind_pending %d\n",
			    arl->arl_unbind_pending));
			arl->arl_unbind_pending = 0;
			/* Complete deferred work */
			ar_ll_down_delayed(arl);
			ar_ll_up_delayed(arl);
			break;
		}
		break;
	case DL_BIND_ACK:
		arp1dbg(("ar_rput: DL_BIND_ACK - bind_pending %d\n",
		    arl->arl_bind_pending));
		dlba = (dl_bind_ack_t *)dloa;
		bcopy((char *)dlba + dlba->dl_addr_offset,
		    arl->arl_hw_addr,
		    arl->arl_hw_addr_length);
		arl->arl_bind_pending--;
		if (arl->arl_bind_pending == 0) {
			arp1dbg(("DL_BIND_ACK: runqueue\n"));
			ar_ll_runqueue(arl);
		}
		/*
		 * There is an ioctl waiting for us...
		 */
		if (arl->arl_dlpiop_done == NULL) {
			arl->arl_dlpiop_done =
			    ar_alloc(AR_DLPIOP_DONE);
		}
		if (arl->arl_dlpiop_done != NULL &&
		    arl->arl_ip_pending_queue != NULL) {
			putnext(arl->arl_ip_pending_queue,
			    arl->arl_dlpiop_done);
			arl->arl_dlpiop_done = NULL;
			arl->arl_ip_pending_queue = NULL;
		}
		break;
	case DL_UDERROR_IND:
		dluei = (dl_uderror_ind_t *)dloa;
		(void) mi_strlog(q, 1, SL_ERROR | SL_TRACE,
		    "ar_rput_dlpi: "
		    "DL_UDERROR_IND, dl_dest_addr_length %d dl_errno %d",
		    dluei->dl_dest_addr_length, dluei->dl_errno);
		/* FALLTHRU */
	default:
		arp1dbg(("ar_rput_dlpi: default, primitive %d\n",
		    (int)dloa->dl_primitive));
		putnext(q, mp);
		return;
	}
	freemsg(mp);
}

static void
ar_set_address(ace_t *ace, uchar_t *addrpos, uchar_t *proto_addr,
    uint32_t proto_addr_len)
{
	uchar_t	*mask, *to;
	int	len;

	if (!ace->ace_hw_addr)
		return;

	bcopy(ace->ace_hw_addr, addrpos, ace->ace_hw_addr_length);
	if (ace->ace_flags & ACE_F_MAPPING &&
	    proto_addr != NULL &&
	    ace->ace_proto_extract_mask) {	/* careful */
		arp1dbg(("ar_set_address: MAPPING\n"));
		len = MIN((int)ace->ace_hw_addr_length
		    - ace->ace_hw_extract_start,
		    proto_addr_len);
		mask = ace->ace_proto_extract_mask;
		to = addrpos + ace->ace_hw_extract_start;
		while (len-- > 0)
			*to++ |= *mask++ & *proto_addr++;
	}
}

static int
ar_slifname(queue_t *q, mblk_t *mp_orig)
{
	ar_t	*ar = (ar_t *)q->q_ptr;
	arl_t	*arl = ar->ar_arl;
	struct lifreq *lifr;
	int newlength;
	mblk_t *mp = mp_orig;

	arp1dbg(("ar_slifname\n"));
	/* We handle both M_IOCTL and M_PROTO messages */
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	if (!q->q_next || !arl) {
		/*
		 * If the interface was just opened and
		 * the info ack has not yet come back from the driver
		 */
		arp1dbg(("ar_slifname no arl - queued\n"));
		(void) putq(q, mp_orig);
		return (EINPROGRESS);
	}
	if (arl->arl_name_set)
		return (EALREADY);

	lifr = (struct lifreq *)(mp->b_rptr);

	if ((newlength = mi_strlen(lifr->lifr_name) +1) > LIFNAMSIZ)
		return (ENXIO);

	/* Avoid finding ourself in the lookup by setting len to 0 */

	arl->arl_name_length = 0;
	if (ar_ll_lookup_by_name((uchar_t *)lifr->lifr_name, newlength)) {
		arp1dbg(("ar_slifname: %s busy\n", lifr->lifr_name));
		arl->arl_name_length = mi_strlen((char *)arl->arl_name) + 1;
		return (EBUSY);
	}
	bcopy(lifr->lifr_name, (char *)arl->arl_name, newlength);
	arl->arl_name_length = newlength;
	/* The ppa is sent down by ifconfig */
	arl->arl_ppa = lifr->lifr_ppa;
	arp1dbg(("ar_slifname: name is now %s, ppa %d\n",
	    (char *)arl->arl_name, arl->arl_ppa));
	arl->arl_name_set = 1;
	return (0);
}

static int
ar_set_ppa(queue_t *q, mblk_t *mp_orig)
{
	ar_t	*ar = (ar_t *)q->q_ptr;
	arl_t	*arl = ar->ar_arl;
	int	ppa;
	char	*cp;
	uint_t	name_length;
	mblk_t	*mp = mp_orig;

	arp1dbg(("ar_set_ppa\n"));
	/* We handle both M_IOCTL and M_PROTO messages. */
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	if (!q->q_next || !arl) {
		/*
		 * If the interface was just opened and
		 * the info ack has not yet come back from the driver.
		 */
		arp1dbg(("ar_set_ppa: no arl - queued\n"));
		(void) putq(q, mp_orig);
		return (EINPROGRESS);
	}

	if (arl->arl_name_set)
		return (EALREADY);

	do {
		q = q->q_next;
	} while (q->q_next);
	cp = q->q_qinfo->qi_minfo->mi_idname;

	ppa = *(int *)(mp->b_rptr);
	/* Set arl_name_length to 0 to avoid matching ourselves */
	(void) mi_sprintf((char *)arl->arl_name, "%s%d", cp, ppa);
	arl->arl_name_length = 0;
	name_length = mi_strlen((char *)arl->arl_name) + 1;
	if (ar_ll_lookup_by_name(arl->arl_name, name_length)) {
		arp1dbg(("ar_set_ppa: %s busy\n", arl->arl_name));
		/* Restore the name */
		(void) mi_sprintf((char *)arl->arl_name, "%s%d",
			cp, arl->arl_ppa);
		arl->arl_name_length =
		    mi_strlen((char *)arl->arl_name) + 1;
		return (EBUSY);
	}
	arl->arl_name_length = mi_strlen((char *)arl->arl_name) + 1;
	arp1dbg(("ar_set_ppa: %d\n", ppa));
	arl->arl_ppa = ppa;
	arl->arl_name_set = 1;
	return (0);
}

static int
ar_snmp_msg(queue_t *q, mblk_t *mp)
{
	mblk_t			*mpdata;
	struct opthdr		*optp;

	if (!mp)
		return (0);

	optp = (struct opthdr *)(&mp->b_rptr[sizeof (struct T_optmgmt_ack)]);
	if (optp->level != MIB2_IP || optp->name != MIB2_IP_MEDIA) {
		putnext(q, mp);
		return (EINPROGRESS);
	}
	/*
	 * this is an ipNetToMediaTable msg from IP that needs (unique)
	 * arp cache entries appended...
	 */
	if ((mpdata = mp->b_cont) == NULL) {
		arp0dbg(("ar_snmp_msg: b_cont == NULL for MIB2_IP msg\n"));
		freemsg(mp);
		return (0);
	}
	ar_ce_walk((pfi_t)ar_snmp_msg2, (uchar_t *)mpdata);
	optp->len = msgdsize(mpdata);
	putnext(q, mp);
	return (EINPROGRESS);	/* so that rput() exits doing nothing... */
}

static uchar_t *
ar_snmp_msg_element(mblk_t **mpp, uchar_t *oldptr, size_t len)
{
	mblk_t	*mp;

	mp = *mpp;
	if (!mp)
		return (NULL);
	if (oldptr)
		oldptr += len;
	else
		oldptr = mp->b_rptr;

	if (oldptr + len > mp->b_wptr) {
		mp = mp->b_cont;
		if (!mp)
			return (NULL);
		oldptr = mp->b_rptr;
		if (oldptr + len > mp->b_wptr)
			return (NULL);
	}
	*mpp = mp;
	return (oldptr);
}


static void
ar_snmp_msg2(ace_t *ace, uchar_t *mp_arg)
{
	mblk_t	*mpdata = (mblk_t *)mp_arg;
	char	*name = "unknown";
	mib2_ipNetToMediaEntry_t	ntme;
	mib2_ipNetToMediaEntry_t	*np;
	mblk_t	*mp1;

#if 0
	if (ace->ace_proto != IP_ARP_PROTO_TYPE)
		return;
#endif
	if (ace->ace_arl && ace->ace_arl->arl_name)
		name = (char *)ace->ace_arl->arl_name;
	ntme.ipNetToMediaIfIndex.o_length = MIN(OCTET_LENGTH, mi_strlen(name));
	bcopy(name, ntme.ipNetToMediaIfIndex.o_bytes,
	    ntme.ipNetToMediaIfIndex.o_length);

	bcopy(ace->ace_proto_addr, &ntme.ipNetToMediaNetAddress,
	    ace->ace_proto_addr_length);

	ntme.ipNetToMediaInfo.ntm_mask.o_length =
	    MIN(OCTET_LENGTH, ace->ace_proto_addr_length);
	bcopy(ace->ace_proto_mask, ntme.ipNetToMediaInfo.ntm_mask.o_bytes,
	    ntme.ipNetToMediaInfo.ntm_mask.o_length);
	/*
	 * Append this arp entry only if not already there...
	 * if found, verify/modify ipNetToMediaType to agree with arp cache
	 * entry.
	 */
	np = NULL;
	mp1 = mpdata;
	do {
		np = (mib2_ipNetToMediaEntry_t *)ar_snmp_msg_element(
		    &mp1, (uchar_t *)np, sizeof (mib2_ipNetToMediaEntry_t));
		if (np &&
		    np->ipNetToMediaNetAddress ==
		    ntme.ipNetToMediaNetAddress &&
		    np->ipNetToMediaInfo.ntm_mask.o_length ==
		    ntme.ipNetToMediaInfo.ntm_mask.o_length &&
		    (bcmp(np->ipNetToMediaInfo.ntm_mask.o_bytes,
			ntme.ipNetToMediaInfo.ntm_mask.o_bytes,
			ntme.ipNetToMediaInfo.ntm_mask.o_length) == 0) &&
		    (bcmp(np->ipNetToMediaIfIndex.o_bytes,
			ntme.ipNetToMediaIfIndex.o_bytes,
			ntme.ipNetToMediaIfIndex.o_length) == 0)) {
			if (ace->ace_flags & ACE_F_PERMANENT) {
				/* permanent arp entries are "static" */
				np->ipNetToMediaType = 4;
			}
			np->ipNetToMediaInfo.ntm_flags = ace->ace_flags;
			return;
		}
	} while (np);
	/*
	 * ace-> is a new entry to append
	 */
	ntme.ipNetToMediaPhysAddress.o_length =
	    MIN(OCTET_LENGTH, ace->ace_hw_addr_length);
	if ((ace->ace_flags & ACE_F_RESOLVED) == 0)
		ntme.ipNetToMediaPhysAddress.o_length = 0;
	bcopy(ace->ace_hw_addr, ntme.ipNetToMediaPhysAddress.o_bytes,
	    ntme.ipNetToMediaPhysAddress.o_length);
	ntme.ipNetToMediaType = (ace->ace_flags & ACE_F_PERMANENT) ? 4 : 3;

	ntme.ipNetToMediaInfo.ntm_flags = ace->ace_flags;
	(void) snmp_append_data(mpdata, (char *)&ntme, sizeof (ntme));
}

/* Start up the garbage collection timer on the queue provided. */
static void
ar_timer_init(queue_t *q)
{
	if (ar_timer_mp)
		return;
	ar_timer_mp = mi_timer_alloc(0);
	if (!ar_timer_mp)
		return;
	ar_timer_queue = q;
	mi_timer(ar_timer_queue, ar_timer_mp, arp_timer_interval);
}

/* ar_ce_walk routine to trash all non-permanent resolved entries. */
/* ARGSUSED */
static int
ar_trash(ace_t *ace, uchar_t *arg)
{
	if ((ace->ace_flags & (ACE_F_RESOLVED|ACE_F_PERMANENT)) ==
	    ACE_F_RESOLVED)
		ar_ce_delete(ace);
	return (0);
}

/* Write side put procedure. */
static void
ar_wput(queue_t *q, mblk_t *mp)
{
	int	err;
	struct iocblk	*ioc;
	mblk_t	*mp1;

	TRACE_1(TR_FAC_ARP, TR_ARP_WPUT_START,
	    "arp_wput_start: q %p", q);

	ARP_ENTER();
	/*
	 * Here we handle ARP commands coming from controlling processes
	 * either in the form of M_IOCTL messages, or M_PROTO messages.
	 */
	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		switch (err = ar_cmd_dispatch(q, mp)) {
		case ENOENT:
			/*
			 * If we don't recognize it, and there is someone
			 * downstream who might, pass it on.
			 */
			if (q->q_next) {
				putnext(q, mp);
				TRACE_2(TR_FAC_ARP, TR_ARP_WPUT_END,
				    "arp_wput_end: q %p (%S)",
				    q, "ioctl/enoent");
				ARP_EXIT();
				return;
			}
			/* Otherwise, reject it. */
			mp->b_datap->db_type = M_IOCNAK;
			if ((mp1 = mp->b_cont) != 0)
				mp1->b_wptr = mp1->b_rptr;
			break;
		case EINPROGRESS:
			/*
			 * If the request resulted in an attempt to resolve
			 * an address, we return out here.  The IOCTL will
			 * be completed in ar_rput if something comes back,
			 * or as a result of the timer expiring.
			 */
			TRACE_2(TR_FAC_ARP, TR_ARP_WPUT_END,
			    "arp_wput_end: q %p (%S)", q, "inprog");
			ARP_EXIT();
			return;
		default:
			mp->b_datap->db_type = M_IOCACK;
			break;
		}
		ioc = (struct iocblk *)mp->b_rptr;
		ioc->ioc_error = err;
		if ((mp1 = mp->b_cont) != 0)
			ioc->ioc_count = msgdsize(mp1);
		else
			ioc->ioc_count = 0;
		qreply(q, mp);
		TRACE_2(TR_FAC_ARP, TR_ARP_WPUT_END,
		    "arp_wput_end: q %p (%S)", q, "ioctl");
		ARP_EXIT();
		return;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq(q, FLUSHDATA);
		if (*mp->b_rptr & FLUSHR) {
			flushq(RD(q), FLUSHDATA);
			*mp->b_rptr &= ~FLUSHW;
			qreply(q, mp);
			TRACE_2(TR_FAC_ARP, TR_ARP_WPUT_END,
			    "arp_wput_end: q %p (%S)", q, "flush");
			ARP_EXIT();
			return;
		}
		break;
	case M_PROTO:
	case M_PCPROTO:
		/*
		 * Commands in the form of PROTO messages are handled very
		 * much the same as IOCTLs, but no response is returned.
		 */
		switch (err = ar_cmd_dispatch(q, mp)) {
		case ENOENT:
			if (q->q_next) {
				putnext(q, mp);
				TRACE_2(TR_FAC_ARP, TR_ARP_WPUT_END,
				    "arp_wput_end: q %p (%S)", q,
				    "proto/enoent");
				ARP_EXIT();
				return;
			}
			break;
		case EINPROGRESS:
			TRACE_2(TR_FAC_ARP, TR_ARP_WPUT_END,
			    "arp_wput_end: q %p (%S)", q, "proto/einprog");
			ARP_EXIT();
			return;
		default:
			break;
		}
		break;
	default:
		break;
	}
	freemsg(mp);
	TRACE_2(TR_FAC_ARP, TR_ARP_WPUT_END,
	    "arp_wput_end: q %p (%S)", q, "end");
	ARP_EXIT();
}

/*
 * Write side service routine.	The only action here is delivery of transmit
 * timer events and delayed messages while waiting for the info_ack (ar_arl
 * not yet set).
 */
static void
ar_wsrv(queue_t *q)
{
	ace_t	*ace;
	mblk_t	*mp;
	clock_t	ms;

	TRACE_1(TR_FAC_ARP, TR_ARP_WSRV_START,
	    "arp_wsrv_start: q %p", q);

	ARP_ENTER();
	while ((mp = getq(q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case M_PCSIG:
			if (!mi_timer_valid(mp))
				continue;
			if (mp == ar_timer_mp) {
				/* Garbage collection time. */
				ar_ce_walk(ar_trash, 0);
				mi_timer(ar_timer_queue, ar_timer_mp,
				    arp_timer_interval);
				continue;
			}
			ace = (ace_t *)mp->b_rptr;
			if (!ace->ace_query_mp)
				continue;
			/*
			 * ar_query_xmit returns the number of milliseconds to
			 * wait following this transmit.  If the number of
			 * allowed transmissions has been exhausted, it will
			 * return zero without transmitting.  If that happens
			 * we complete the operation with a failure indication.
			 * Otherwise, we restart the timer.
			 */
			ms = ar_query_xmit(ace);
			if (ms == 0)
				ar_query_reply(ace, ENXIO, NULL,
				    (uint32_t)0);
			else
				mi_timer(q, mp, ms);
			continue;
		default:
			put(q, mp);
			continue;
		}
	}
	TRACE_1(TR_FAC_ARP, TR_ARP_WSRV_END,
	    "arp_wsrv_end: q %p", q);
	ARP_EXIT();
}

/* ar_xmit is called to transmit an ARP Request or Response. */
static void
ar_xmit(arl_t *arl, uint32_t operation, uint32_t proto, uint32_t plen,
    uchar_t *haddr1, uchar_t *paddr1, uchar_t *haddr2, uchar_t *paddr2)
{
	arh_t	*arh;
	char	*cp;
	uint32_t	hlen = arl->arl_hw_addr_length;
	mblk_t	*mp;

	if (arl->arl_flags & ARL_F_NOARP) {
		/* IFF_NOARP flag is set. Do not send an arp request */
		return;
	}

	mp = arl->arl_xmit_template;
	if (!mp || !(mp = copyb(mp)))
		return;
	mp->b_cont = allocb(AR_LL_HDR_SLACK + ARH_FIXED_LEN + (hlen * 4) +
	    plen + plen, BPRI_MED);
	if (!mp->b_cont) {
		freeb(mp);
		return;
	}
	/*
	 * Figure out where the target hardware address goes in the
	 * DL_UNITDATA_REQ header, and copy it in.
	 */

	cp = (char *)mi_offset_param(mp, arl->arl_xmit_template_addr_offset,
	    hlen);
	if (!cp) {
		freemsg(mp);
		return;
	}
	bcopy(haddr2, cp, hlen);

	/* Fill in the ARP header. */
	cp = (char *)mp->b_cont->b_rptr + (AR_LL_HDR_SLACK + hlen + hlen);
	mp->b_cont->b_rptr = (uchar_t *)cp;
	arh = (arh_t *)cp;
	U16_TO_BE16(arl->arl_arp_hw_type, arh->arh_hardware);
	U16_TO_BE16(proto, arh->arh_proto);
	arh->arh_hlen = (uint8_t)hlen;
	arh->arh_plen = (uint8_t)plen;
	U16_TO_BE16(operation, arh->arh_operation);
	cp += ARH_FIXED_LEN;
	bcopy(haddr1, cp, hlen);
	cp += hlen;
	bcopy(paddr1, cp, plen);
	cp += plen;
	bcopy(haddr2, cp, hlen);
	cp += hlen;
	bcopy(paddr2, cp, plen);
	cp += plen;
	mp->b_cont->b_wptr = (uchar_t *)cp;
	/* Ship it out. */
	if (canputnext(arl->arl_wq))
		putnext(arl->arl_wq, mp);
	else
		freemsg(mp);
}

/*
 * Handle an external request to broadcast an ARP request.  This is used
 * by configuration programs to broadcast a request advertising our own
 * hardware and protocol addresses.
 */
static int
ar_xmit_request(queue_t *q, mblk_t *mp_orig)
{
	areq_t	*areq;
	arl_t	*arl;
	uchar_t	*sender;
	uint32_t	sender_length;
	uchar_t	*target;
	uint32_t	target_length;
	mblk_t	*mp = mp_orig;

	/* We handle both M_IOCTL and M_PROTO messages. */
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl)
		return (EINVAL);
	if (arl->arl_bind_pending || arl->arl_unbind_pending) {
		arp1dbg(("ar_xmit_request: pending (%d,%d)\n",
		    arl->arl_bind_pending, arl->arl_unbind_pending));
		ar_ll_enqueue(arl, q, mp_orig);
		return (EINPROGRESS);
	}
	areq = (areq_t *)mp->b_rptr;
	sender_length = areq->areq_sender_addr_length;
	sender = mi_offset_param(mp, areq->areq_sender_addr_offset,
	    sender_length);
	target_length = areq->areq_target_addr_length;
	target = mi_offset_param(mp, areq->areq_target_addr_offset,
	    target_length);
	if (!sender || !target)
		return (EINVAL);
	ar_xmit(arl, ARP_REQUEST, areq->areq_proto, sender_length,
	    arl->arl_hw_addr, sender, arl->arl_arp_addr, target);
	return (0);
}

/*
 * Handle an external request to broadcast an ARP response.  This is used
 * by configuration programs to broadcast a response advertising our own
 * hardware and protocol addresses.
 */
static int
ar_xmit_response(queue_t *q, mblk_t *mp_orig)
{
	areq_t	*areq;
	arl_t	*arl;
	uchar_t	*sender;
	uint32_t	sender_length;
	uchar_t	*target;
	uint32_t	target_length;
	mblk_t	*mp = mp_orig;

	/* We handle both M_IOCTL and M_PROTO messages. */
	if (mp->b_datap->db_type == M_IOCTL)
		mp = mp->b_cont;
	arl = ar_ll_lookup_from_mp(mp);
	if (!arl)
		return (EINVAL);
	if (arl->arl_bind_pending || arl->arl_unbind_pending) {
		arp1dbg(("ar_xmit_response: pending (%d,%d)\n",
		    arl->arl_bind_pending, arl->arl_unbind_pending));
		ar_ll_enqueue(arl, q, mp_orig);
		return (EINPROGRESS);
	}
	areq = (areq_t *)mp->b_rptr;
	sender_length = areq->areq_sender_addr_length;
	sender = mi_offset_param(mp, areq->areq_sender_addr_offset,
	    sender_length);
	target_length = areq->areq_target_addr_length;
	target = mi_offset_param(mp, areq->areq_target_addr_offset,
	    target_length);
	if (!sender || !target)
		return (EINVAL);
	ar_xmit(arl, ARP_RESPONSE, areq->areq_proto, sender_length,
	    arl->arl_hw_addr, sender, arl->arl_arp_addr, target);
	return (0);
}

#if 0
/*
 * Debug routine to display a particular ARP Cache Entry with an
 * accompanying text message.
 */
static void
show_ace(char *msg, ace_t *ace)
{
	if (msg)
		printf("%s", msg);
	printf("ace 0x%p:\n", ace);
	printf("\tace_next 0x%p, ace_ptpn 0x%p, ace_arl 0x%p\n",
	    ace->ace_next, ace->ace_ptpn, ace->ace_arl);
	printf("\tace_proto %x, ace_flags %x\n", ace->ace_proto,
	    ace->ace_flags);
	if (ace->ace_proto_addr && ace->ace_proto_addr_length)
		printf("\tace_proto_addr %x %x %x %x, len %d\n",
		    ace->ace_proto_addr[0], ace->ace_proto_addr[1],
		    ace->ace_proto_addr[2], ace->ace_proto_addr[3],
		    ace->ace_proto_addr_length);
	if (ace->ace_proto_mask)
		printf("\tace_proto_mask %x %x %x %x\n",
		    ace->ace_proto_mask[0], ace->ace_proto_mask[1],
		    ace->ace_proto_mask[2], ace->ace_proto_mask[3]);
	if (ace->ace_hw_addr && ace->ace_hw_addr_length)
		printf("\tace_hw_addr %x %x %x %x %x %x, len %d\n",
		    ace->ace_hw_addr[0], ace->ace_hw_addr[1],
		    ace->ace_hw_addr[2], ace->ace_hw_addr[3],
		    ace->ace_hw_addr[4], ace->ace_hw_addr[5],
		    ace->ace_hw_addr_length);
	printf("\tace_mp 0x%p\n", ace->ace_mp);
	printf("\tace_query_count %d, ace_query_mp 0x%x\n",
	    ace->ace_query_count, ace->ace_query_mp);
}

/* Debug routine to display an ARP packet with an accompanying text message. */
static void
show_arp(char *msg, mblk_t *mp)
{
	uchar_t	*up = mp->b_rptr;
	int	len;
	int	hlen = up[4] & 0xFF;
	char	fmt[64];
	char	buf[128];
	char	*op;
	int	plen = up[5] & 0xFF;
	uint_t	proto;

	if (msg && *msg)
		printf("%s", msg);
	len = mp->b_wptr - up;
	if (len < 8) {
		printf("ARP packet of %d bytes too small\n", len);
		return;
	}
	switch (BE16_TO_U16(&up[6])) {
	case ARP_REQUEST:
		op = "ARP request";
		break;
	case ARP_RESPONSE:
		op = "ARP response";
		break;
	case RARP_REQUEST:
		op = "RARP request";
		break;
	case RARP_RESPONSE:
		op = "RARP response";
		break;
	default:
		op = "unknown";
		break;
	}
	proto = (uint_t)BE16_TO_U16(&up[2]);
	printf("len %d, hardware %d, proto %d, hlen %d, plen %d, op %s\n",
	    len, (int)BE16_TO_U16(up), proto, hlen, plen, op);
	if (len < (8 + hlen + hlen + plen + plen))
		printf("ARP packet of %d bytes too small!\n", len);
	up += 8;

	(void) mi_sprintf(fmt, "sender hardware address %%%dM\n", hlen);
	(void) mi_sprintf(buf, fmt, up);
	printf(buf);
	up += hlen;
	if (proto == 0x800) {
		printf("sender proto address %d.%d.%d.%d\n",
		    up[0] & 0xFF, up[1] & 0xFF, up[2] & 0xFF,
		    up[3] & 0xFF);
	} else {
		(void) mi_sprintf(fmt, "sender proto address %%%dM\n", plen);
		(void) mi_sprintf(buf, fmt, up);
		printf(buf);
	}
	up += plen;

	(void) mi_sprintf(fmt, "target hardware address %%%dM\n", hlen);
	(void) mi_sprintf(buf, fmt, up);
	printf(buf);
	up += hlen;
	if (proto == 0x800) {
		printf("target proto address %d.%d.%d.%d\n",
		    up[0] & 0xFF, up[1] & 0xFF, up[2] & 0xFF,
		    up[3] & 0xFF);
	} else {
		(void) mi_sprintf(fmt, "target proto address %%%dM\n", plen);
		(void) mi_sprintf(buf, fmt, up);
		printf(buf);
	}
	up += plen;
}
#endif

static mblk_t *
ar_alloc(uint32_t cmd)
{
	uint32_t	len;
	mblk_t		*mp;
	mblk_t		*mp1;
	char		*cp;
	arc_t		*arc;

	/* For now only one type of command is accepted */
	if (cmd != AR_DLPIOP_DONE)
		return (NULL);
	len = sizeof (arc_t);
	mp = allocb(len, BPRI_HI);
	if (!mp)
		return (NULL);

	mp->b_datap->db_type = M_CTL;
	cp = (char *)mp->b_rptr;
	arc = (arc_t *)(mp->b_rptr);
	arc->arc_cmd = cmd;
	mp->b_wptr = (uchar_t *)&cp[len];
	len = sizeof (int);
	mp1 = allocb(len, BPRI_HI);
	if (!mp1) {
		freeb(mp);
		return (NULL);
	}
	cp = (char *)mp->b_rptr;
	/* Initialize the error code to 0 */
	*((int *)mp1->b_rptr) = 0;
	mp1->b_wptr = (uchar_t *)&cp[len];
	linkb(mp, mp1);
	return (mp);
}
