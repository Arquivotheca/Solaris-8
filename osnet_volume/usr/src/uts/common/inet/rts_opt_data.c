/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rts_opt_data.c	1.9	99/10/21 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/xti_xtiopt.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_mroute.h>
#include "optcom.h"

extern int rts_opt_default(queue_t *q, t_scalar_t level, t_scalar_t name,
    uchar_t *ptr);
extern int rts_opt_get(queue_t *q, t_scalar_t level, t_scalar_t name,
    uchar_t *ptr);
extern int rts_opt_set(queue_t *q, uint_t optset_context, t_scalar_t level,
    t_scalar_t name, uint_t inlen, uchar_t *invalp, uint_t *outlenp,
    uchar_t *outvalp, uchar_t *thisdg_attrs);

/*
 * Table of all known options handled on a RTS protocol stack.
 *
 * Note: This table contains options processed by both RTS and IP levels
 *       and is the superset of options that can be performed on a RTS over IP
 *       stack.
 */
opdes_t	rts_opt_arr[] = {

{ SO_DEBUG,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_TYPE,	SOL_SOCKET, OA_R, OA_R, OP_PASSNEXT, sizeof (int), 0 },
{ SO_SNDBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_RCVBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_PROTOTYPE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

};

/*
 * Table of all supported levels
 * Note: Some levels (e.g. XTI_GENERIC) may be valid but may not have
 * any supported options so we need this info separately.
 *
 * This is needed only for topmost tpi providers and is used only by
 * XTI interfaces.
 */
optlevel_t	rts_valid_levels_arr[] = {
	XTI_GENERIC,
	SOL_SOCKET,
	IPPROTO_IP,
	IPPROTO_IPV6
};

#define	RTS_VALID_LEVELS_CNT	A_CNT(rts_valid_levels_arr)

#define	RTS_OPT_ARR_CNT		A_CNT(rts_opt_arr)

uint_t rts_max_optbuf_len; /* initialized in _init() */

/*
 * Intialize option database object for RTS
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t rts_opt_obj = {
	rts_opt_default,	/* RTS default value function pointer */
	rts_opt_get,		/* RTS get function pointer */
	rts_opt_set,		/* RTS set function pointer */
	B_TRUE,			/* RTS is tpi provider */
	RTS_OPT_ARR_CNT,	/* RTS option database count of entries */
	rts_opt_arr,		/* RTS option database */
	RTS_VALID_LEVELS_CNT,	/* RTS valid level count of entries */
	rts_valid_levels_arr	/* RTS valid level array */
};
