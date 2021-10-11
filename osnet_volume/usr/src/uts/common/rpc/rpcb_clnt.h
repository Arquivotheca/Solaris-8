/*
 * Copyright (c) 1986 - 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * rpcb_clnt.h
 * Supplies C routines to get to rpcbind services.
 */

/*
 * Usage:
 *	success = rpcb_set(program, version, nconf, address);
 *	success = rpcb_unset(program, version, nconf);
 *	success = rpcb_getaddr(program, version, nconf, host);
 *	head = rpcb_getmaps(nconf, host);
 *	clnt_stat = rpcb_rmtcall(nconf, host, program, version, procedure,
 *		xdrargs, argsp, xdrres, resp, tout, addr_ptr)
 *	success = rpcb_gettime(host, timep)
 *	uaddr = rpcb_taddr2uaddr(nconf, taddr);
 *	taddr = rpcb_uaddr2uaddr(nconf, uaddr);
 */

#ifndef _RPC_RPCB_CLNT_H
#define	_RPC_RPCB_CLNT_H

#pragma ident	"@(#)rpcb_clnt.h	1.13	98/06/05 SMI"
/* rpcb_clnt.h 1.3 88/12/05 SMI */

#include <rpc/types.h>
#include <rpc/rpcb_prot.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC__
extern bool_t		rpcb_set(const rpcprog_t, const rpcvers_t,
	const struct netconfig  *, const struct netbuf *);
extern bool_t		rpcb_unset(const rpcprog_t, const rpcvers_t,
	const struct netconfig *);
extern rpcblist	*rpcb_getmaps(const struct netconfig *, const char *);
extern enum clnt_stat	rpcb_rmtcall(const struct netconfig *, const char *,
const rpcprog_t, const rpcvers_t, const rpcproc_t, const xdrproc_t,
	const caddr_t, const xdrproc_t, const caddr_t,
	const struct timeval, struct netbuf *);
extern bool_t		rpcb_getaddr(const rpcprog_t, const rpcvers_t,
	const struct netconfig *, struct netbuf *, const  char *);
extern bool_t		rpcb_gettime(const char *, time_t *);
extern char		*rpcb_taddr2uaddr(struct netconfig *, struct netbuf *);
extern struct netbuf	*rpcb_uaddr2taddr(struct netconfig *, char *);
#else
extern bool_t		rpcb_set();
extern bool_t		rpcb_unset();
extern rpcblist	*rpcb_getmaps();
extern enum clnt_stat	rpcb_rmtcall();
extern bool_t		rpcb_getaddr();
extern bool_t		rpcb_gettime();
extern char		*rpcb_taddr2uaddr();
extern struct netbuf	*rpcb_uaddr2taddr();
#endif

#ifdef __cplusplus
}
#endif

#endif	/* !_RPC_RPCB_CLNT_H */
