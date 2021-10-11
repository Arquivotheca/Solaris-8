/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_RPC_H
#define	_RPC_H

#pragma ident	"@(#)rpc.h	1.7	99/02/23 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Rather than include rpc/types.h, and run into the conflict of
 * the kernel/boot kmem_alloc prototypes, just include what we need
 * here.
 */
#ifndef	_RPC_TYPES_H
#define	_RPC_TYPES_H
#endif	/* _RPC_TYPES_H */

#define	bool_t	int
#define	enum_t	int
#define	__dontcare__	-1

#ifndef	TRUE
#define	TRUE	(1)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	NULL
#define	NULL	0
#endif

#define	RPC_ALLOWABLE_ERRORS	(10)	/* Threshold on receiving bad results */
#define	RPC_REXMIT_MSEC		(500)	/* default 1/2 second retransmissions */
#define	RPC_RCVWAIT_MSEC	(20000)	/* default response waittime */

enum clnt_stat brpc_call(rpcprog_t, rpcvers_t, rpcproc_t, xdrproc_t, caddr_t,
	xdrproc_t, caddr_t, int, int, struct sockaddr_in *,
	struct sockaddr_in *, u_int);

#ifdef	__cplusplus
}
#endif

#include <sys/time.h>

#endif	/* _RPC_H */
