/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)keysock_opt_data.c 1.2	99/10/21 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 1
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/xti_xtiopt.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <inet/optcom.h>
#include <inet/keysock.h>

/*
 * Table of all known options handled on a keysock (PF_KEY) protocol stack.
 *
 * Note: This table contains options processed by both KEYSOCK and IP levels
 *       and is the superset of options that can be performed on a KEYSOCK over
 *	 IP stack.
 */

opdes_t keysock_opt_arr[] = {
	{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT,
	    (t_uscalar_t)sizeof (int), 0 },
	{ SO_SNDBUF, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT,
	    (t_uscalar_t)sizeof (int), 0 },
	{ SO_RCVBUF, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT,
	    (t_uscalar_t)sizeof (int), 0 },
};

/*
 * Table of all supported levels
 * Note: Some levels (e.g. XTI_GENERIC) may be valid but may not have
 * any supported options so we need this info separately.
 *
 * This is needed only for topmost tpi providers.
 */
optlevel_t	keysock_valid_levels_arr[] = {
	SOL_SOCKET
};

#define	KEYSOCK_VALID_LEVELS_CNT	A_CNT(keysock_valid_levels_arr)

#define	KEYSOCK_OPT_ARR_CNT		A_CNT(keysock_opt_arr)

uint_t keysock_max_optbuf_len; /* initialized in keysock_ddi_init() */

/*
 * Intialize option database object for KEYSOCK
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t keysock_opt_obj = {
	NULL,			/* KEYSOCK default value function pointer */
	keysock_opt_get,	/* KEYSOCK get function pointer */
	keysock_opt_set,	/* KEYSOCK set function pointer */
	B_TRUE,			/* KEYSOCK is tpi provider */
	KEYSOCK_OPT_ARR_CNT,	/* KEYSOCK option database count of entries */
	keysock_opt_arr,	/* KEYSOCK option database */
	KEYSOCK_VALID_LEVELS_CNT, /* KEYSOCK valid level count of entries */
	keysock_valid_levels_arr  /* KEYSOCK valid level array */
};
