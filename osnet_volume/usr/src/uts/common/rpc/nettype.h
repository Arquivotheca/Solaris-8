/*
 * Copyright (c) 1986 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * nettype.h, Nettype definitions.
 * All for the topmost layer of rpc
 *
 */

#ifndef	_RPC_NETTYPE_H
#define	_RPC_NETTYPE_H

#pragma ident	"@(#)nettype.h	1.12	96/06/04 SMI"

#include <netconfig.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	_RPC_NONE		0
#define	_RPC_NETPATH		1
#define	_RPC_VISIBLE		2
#define	_RPC_CIRCUIT_V		3
#define	_RPC_DATAGRAM_V		4
#define	_RPC_CIRCUIT_N		5
#define	_RPC_DATAGRAM_N		6
#define	_RPC_TCP		7
#define	_RPC_UDP		8
#define	_RPC_LOCAL		9
#define	_RPC_DOOR		10
#define	_RPC_DOOR_LOCAL		11
#define	_RPC_DOOR_NETPATH	12

#ifdef __STDC__
extern void *__rpc_setconf(char *);
extern void __rpc_endconf(void *);
extern struct netconfig *__rpc_getconf(void *);
extern struct netconfig *__rpc_getconfip(char *);
#else
extern void *__rpc_setconf();
extern void __rpc_endconf();
extern struct netconfig *__rpc_getconf();
extern struct netconfig *__rpc_getconfip();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* !_RPC_NETTYPE_H */
