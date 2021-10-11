/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)icmp_opt_data.c	1.12	99/10/21 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/xti_xtiopt.h>
#include <sys/xti_inet.h>

#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
/*
 * MK_XXX Following 2 includes temporary to import ip6_rthdr_t
 *        definition. May not be needed if we fix ip6_dg_snd_attrs_t
 *        to do all extension headers in identical manner.
 */
#include <net/if.h>
#include <inet/ip6.h>

#include <netinet/tcp.h>
#include <netinet/ip_mroute.h>
#include "optcom.h"


extern int icmp_opt_default(queue_t *, t_scalar_t, t_scalar_t, uchar_t *);
extern int icmp_opt_get(queue_t *, t_scalar_t, t_scalar_t, uchar_t *);
extern int icmp_opt_set(queue_t *, uint_t, t_scalar_t, t_scalar_t,
    t_scalar_t, uchar_t *, t_scalar_t *, uchar_t *, void *);

/*
 * Table of all known options handled on a ICMP protocol stack.
 *
 * Note: This table contains options processed by both ICMP and IP levels
 *       and is the superset of options that can be performed on a ICMP over IP
 *       stack.
 */
opdes_t	icmp_opt_arr[] = {

{ SO_DEBUG,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

#ifdef	SO_PROTOTYPE
	/* icmp will only allow IPPROTO_ICMP for non-priviledged streams */
{ SO_PROTOTYPE, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
#endif

{ SO_TYPE,	SOL_SOCKET, OA_R, OA_R, OP_PASSNEXT, sizeof (int), 0 },
{ SO_SNDBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_RCVBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DGRAM_ERRIND, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initialized */ },
{ T_IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initialized */ },

{ IP_HDRINCL,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ T_IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TTL,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IP_MULTICAST_IF, IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (struct in_addr), 0 /* INADDR_ANY */ },

{ IP_MULTICAST_LOOP, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (uchar_t), -1 /* not initialized */},

{ IP_MULTICAST_TTL, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (uchar_t), -1 /* not initialized */ },

{ IP_ADD_MEMBERSHIP, IPPROTO_IP, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ip_mreq), -1 /* not initialized */ },

{ IP_DROP_MEMBERSHIP, 	IPPROTO_IP, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ip_mreq), 0 },

{ IP_SEC_OPT, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (ipsec_req_t), -1 /* not initialized */ },

{ IP_BOUND_IF, IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int),	0 /* no ifindex */ },

{ IP_UNSPEC_SRC, IPPROTO_IP, OA_R, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ MRT_INIT, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT), sizeof (int),
	-1 /* not initialized */ },

{ MRT_DONE, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT), 0,
	-1 /* not initialized */ },

{ MRT_ADD_VIF, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct vifctl), -1 /* not initialized */ },

{ MRT_DEL_VIF, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (vifi_t), -1 /* not initialized */ },

{ MRT_ADD_MFC, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct mfcctl), -1 /* not initialized */ },

{ MRT_DEL_MFC, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct mfcctl), -1 /* not initialized */ },

{ MRT_VERSION, 	IPPROTO_IP, OA_R, OA_R, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (int), -1 /* not initialized */ },

{ MRT_ASSERT, 	IPPROTO_IP, 0, OA_RW, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (int), -1 /* not initialized */ },

{ IPV6_MULTICAST_IF, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int), 0 },

{ IPV6_MULTICAST_HOPS, IPPROTO_IPV6, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ IPV6_MULTICAST_LOOP, IPPROTO_IPV6, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */},

{ IPV6_JOIN_GROUP, IPPROTO_IPV6, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ipv6_mreq), -1 /* not initialized */ },

{ IPV6_LEAVE_GROUP,	IPPROTO_IPV6, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ipv6_mreq), -1 /* not initialized */ },

{ IPV6_UNICAST_HOPS, IPPROTO_IPV6, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ IPV6_BOUND_IF, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int),	0 /* no ifindex */ },

{ IPV6_UNSPEC_SRC, IPPROTO_IPV6, OA_R, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IPV6_CHECKSUM, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), -1 },

{ ICMP6_FILTER, IPPROTO_ICMPV6, OA_RW, OA_RW, OP_DEF_FN,
	sizeof (icmp6_filter_t), 0 },
{ IPV6_PKTINFO, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_NODEFAULT|OP_VARLEN),
	sizeof (struct in6_pktinfo), -1 /* not initialized */ },
{ IPV6_HOPLIMIT, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_NODEFAULT|OP_VARLEN),
	sizeof (int), -1 /* not initialized */ },
{ IPV6_NEXTHOP, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_NODEFAULT|OP_VARLEN),
	sizeof (sin6_t), -1 /* not initialized */ },
{ IPV6_HOPOPTS, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	MAX_EHDR_LEN, -1 /* not initialized */ },
{ IPV6_DSTOPTS, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	MAX_EHDR_LEN, -1 /* not initialized */ },
{ IPV6_RTHDRDSTOPTS, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	MAX_EHDR_LEN, -1 /* not initialized */ },
{ IPV6_RTHDR, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	MAX_EHDR_LEN, -1 /* not initialized */ },
{ IPV6_RECVPKTINFO, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IPV6_RECVHOPLIMIT, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IPV6_RECVHOPOPTS, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IPV6_RECVDSTOPTS, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IPV6_RECVRTHDR, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IPV6_RECVRTHDRDSTOPTS, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int), 0 },
};

/*
 * Table of all supported levels
 * Note: Some levels (e.g. XTI_GENERIC) may be valid but may not have
 * any supported options so we need this info separately.
 *
 * This is needed only for topmost tpi providers and is used only by
 * XTI interfaces.
 */
optlevel_t	icmp_valid_levels_arr[] = {
	XTI_GENERIC,
	SOL_SOCKET,
	IPPROTO_ICMP,
	IPPROTO_IP,
	IPPROTO_IPV6,
	IPPROTO_ICMPV6
};

#define	ICMP_VALID_LEVELS_CNT	A_CNT(icmp_valid_levels_arr)
#define	ICMP_OPT_ARR_CNT		A_CNT(icmp_opt_arr)

uint_t	icmp_max_optbuf_len; /* initialized when ICMP driver is loaded */

/*
 * Initialize option database object for ICMP
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t icmp_opt_obj = {
	icmp_opt_default,	/* ICMP default value function pointer */
	icmp_opt_get,		/* ICMP get function pointer */
	icmp_opt_set,		/* ICMP set function pointer */
	B_TRUE,			/* ICMP is tpi provider */
	ICMP_OPT_ARR_CNT,	/* ICMP option database count of entries */
	icmp_opt_arr,		/* ICMP option database */
	ICMP_VALID_LEVELS_CNT,	/* ICMP valid level count of entries */
	icmp_valid_levels_arr	/* ICMP valid level array */
};
