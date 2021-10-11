/*
 * Copyright (c) 1986-1991, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RPC_TYPES_H
#define	_RPC_TYPES_H

#pragma ident	"@(#)types.h	1.25	98/01/06 SMI"

/*	types.h 1.23 88/10/25 SMI	*/

/*
 * Rpc additions to <sys/types.h>
 */
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int bool_t;
typedef int enum_t;

/*
 * The following is part of a workaround for bug #1128007.
 * When it is fixed, this next typedef should be removed.
 */
typedef u_longlong_t ulonglong_t;

#if defined(_LP64) || defined(_I32LPx)
typedef	uint32_t rpcprog_t;
typedef	uint32_t rpcvers_t;
typedef	uint32_t rpcproc_t;
typedef uint32_t rpcprot_t;
typedef uint32_t rpcport_t;
typedef int32_t rpc_inline_t;
#else
typedef	unsigned long rpcprog_t;
typedef	unsigned long rpcvers_t;
typedef	unsigned long rpcproc_t;
typedef unsigned long rpcprot_t;
typedef unsigned long rpcport_t;
typedef long rpc_inline_t;
#endif


#define	__dontcare__	-1

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(1)
#endif

#ifndef	NULL
#define	NULL	0
#endif

#ifndef	_KERNEL
#define	mem_alloc(bsize)	malloc(bsize)
#define	mem_free(ptr, bsize)	free(ptr)
#else
#include <sys/kmem.h>		/* XXX */

#define	mem_alloc(bsize)	kmem_alloc(bsize, KM_SLEEP)
#define	mem_free(ptr, bsize)	kmem_free(ptr, bsize)

extern const char *rpc_tpiprim2name(uint_t prim);
extern const char *rpc_tpierr2name(uint_t err);

#if defined(DEBUG) && !defined(RPCDEBUG)
#define	RPCDEBUG
#endif

#ifdef RPCDEBUG
extern uint_t	rpclog;

#define	RPCLOG(A, B, C)	\
	((void)((rpclog) && (rpclog & (A)) && (printf((B), (C)), TRUE)))
#define	RPCLOG0(A, B)	\
	((void)((rpclog) && (rpclog & (A)) && (printf(B), TRUE)))
#else
#define		RPCLOG(A, B, C)
#define		RPCLOG0(A, B)
#endif

#endif

#ifdef _NSL_RPC_ABI
/* For internal use only when building the libnsl RPC routines */
#define	select	_abi_select
#define	gettimeofday	_abi_gettimeofday
#define	syslog	_abi_syslog
#define	getgrent	_abi_getgrent
#define	endgrent	_abi_endgrent
#define	setgrent	_abi_setgrent
#endif

/* messaging stuff. */
#ifndef _KERNEL
#ifdef __STDC__
extern const char __nsl_dom[];
#else
extern char __nsl_dom[];
#endif
#endif

#ifdef __cplusplus
}
#endif

#include <sys/time.h>

#endif	/* _RPC_TYPES_H */
