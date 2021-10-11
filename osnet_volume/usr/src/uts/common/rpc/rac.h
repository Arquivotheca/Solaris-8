/*
 * Copyright (c) 1990, Sun Microsystems, Inc.
 */

#ifndef	_RPC_RAC_H
#define	_RPC_RAC_H

#pragma ident	"@(#)rac.h	1.7	97/04/29 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC__
void rac_drop(CLIENT *, void *);
enum clnt_stat	rac_poll(CLIENT *, void *);
enum clnt_stat	rac_recv(CLIENT *, void *);
void *rac_send(CLIENT *, rpcproc_t, xdrproc_t, void *, xdrproc_t,
		void *, struct timeval);
#else
void rac_drop();
enum clnt_stat	rac_poll();
enum clnt_stat	rac_recv();
void *rac_send();
#endif

/*
 *	If a rac_send fails, it returns (void *) 0.  The reason for failure
 *	is cached here.
 *	N.B.:  this is a global structure.
 */
extern struct rpc_err	rac_senderr;

#ifdef __cplusplus
}
#endif

#endif	/* _RPC_RAC_H */
