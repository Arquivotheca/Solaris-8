/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tcp_opt_data.c	1.16	99/11/04 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/xti_xtiopt.h>
#include <sys/xti_inet.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_mroute.h>
#include "optcom.h"


extern int	tcp_opt_default(queue_t *q, int level, int name, uchar_t *ptr);
extern int	tcp_opt_get(queue_t *q, int level, int name, uchar_t *ptr);
extern int	tcp_opt_set(queue_t *q, uint_t optset_context, int level,
    int name, uint_t inlen, uchar_t *invalp, uint_t *outlenp, uchar_t *outvalp,
    uchar_t *thisdg_attrs);

/*
 * Table of all known options handled on a TCP protocol stack.
 *
 * Note: This table contains options processed by both TCP and IP levels
 *       and is the superset of options that can be performed on a TCP over IP
 *       stack.
 */
opdes_t	tcp_opt_arr[] = {

{ SO_LINGER,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (struct linger),
	0 },

{ SO_DEBUG,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_KEEPALIVE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_OOBINLINE, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_TYPE,	SOL_SOCKET, OA_R, OA_R, OP_PASSNEXT, sizeof (int), 0 },
{ SO_SNDBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_RCVBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DGRAM_ERRIND, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ TCP_NODELAY,	IPPROTO_TCP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ TCP_MAXSEG,	IPPROTO_TCP, OA_R, OA_R, OP_PASSNEXT, sizeof (uint_t), 536 },

{ TCP_NOTIFY_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ TCP_ABORT_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ TCP_CONN_NOTIFY_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_DEF_FN), sizeof (int), -1 /* not initialized */ },

{ TCP_CONN_ABORT_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ TCP_RECVDSTADDR, IPPROTO_TCP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ TCP_ANONPRIVBIND, IPPROTO_TCP, OA_R, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ TCP_EXCLBIND, IPPROTO_TCP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initialized */ },
{ T_IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initialized */ },

{ IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ T_IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TTL,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },
{ IP_ADD_PROXY_ADDR, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (in_prefix_t), -1 /* not initialized */ },

{ IP_SEC_OPT, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (ipsec_req_t), -1 /* not initialized */ },

{ IP_BOUND_IF, IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int),	0 /* no ifindex */ },
{ IP_UNSPEC_SRC, IPPROTO_IP, OA_R, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IPV6_UNICAST_HOPS, IPPROTO_IPV6, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ IPV6_BOUND_IF, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int),	0 /* no ifindex */ },

{ IPV6_UNSPEC_SRC, IPPROTO_IPV6, OA_R, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

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
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT), 255*8,
	-1 /* not initialized */ },
{ IPV6_DSTOPTS, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT), 255*8,
	-1 /* not initialized */ },
{ IPV6_RTHDRDSTOPTS, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT), 255*8,
	-1 /* not initialized */ },
{ IPV6_RTHDR, IPPROTO_IPV6, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT), 255*8,
	-1 /* not initialized */ },

/* Enable receipt of ancillary data */
{ IPV6_RECVPKTINFO, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int), 0 },
{ IPV6_RECVHOPLIMIT, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int), 0 },
{ IPV6_RECVHOPOPTS, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int), 0 },
{ IPV6_RECVDSTOPTS, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int), 0 },
{ IPV6_RECVRTHDR, IPPROTO_IPV6, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (int), 0 },
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
optlevel_t	tcp_valid_levels_arr[] = {
	XTI_GENERIC,
	SOL_SOCKET,
	IPPROTO_TCP,
	IPPROTO_IP,
	IPPROTO_IPV6
};


#define	TCP_OPT_ARR_CNT		A_CNT(tcp_opt_arr)
#define	TCP_VALID_LEVELS_CNT	A_CNT(tcp_valid_levels_arr)

uint_t tcp_max_optbuf_len; /* initialized when TCP driver is loaded */

/*
 * Initialize option database object for TCP
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t tcp_opt_obj = {
	tcp_opt_default,	/* TCP default value function pointer */
	tcp_opt_get,		/* TCP get function pointer */
	tcp_opt_set,		/* TCP set function pointer */
	B_TRUE,			/* TCP is tpi provider */
	TCP_OPT_ARR_CNT,	/* TCP option database count of entries */
	tcp_opt_arr,		/* TCP option database */
	TCP_VALID_LEVELS_CNT,	/* TCP valid level count of entries */
	tcp_valid_levels_arr	/* TCP valid level array */
};
