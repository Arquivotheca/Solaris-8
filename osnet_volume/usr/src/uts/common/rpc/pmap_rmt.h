/*
 * Copyright (c) 1986-1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RPC_PMAPRMT_H
#define	_RPC_PMAPRMT_H

#pragma ident	"@(#)pmap_rmt.h	1.12	98/01/06 SMI"

/*	@(#)pmap_rmt.h 1.8 89/03/21 SMI	*/

#ifndef _KERNEL

#include <rpc/pmap_prot.h>

#else	/* ndef _KERNEL */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Structures and XDR routines for parameters to and replies from
 * the portmapper remote-call-service.
 */

struct rmtcallargs {
	rpcprog_t prog;
	rpcvers_t vers;
	rpcproc_t proc;
	unsigned int arglen;
	caddr_t	  args_ptr;
	xdrproc_t xdr_args;
};

#ifdef __STDC__
bool_t xdr_rmtcall_args(XDR *, struct rmtcallargs *);
#else
bool_t xdr_rmtcall_args();
#endif

struct rmtcallres {
	rpcport_t *port_ptr;
	uint_t resultslen;
	caddr_t results_ptr;
	xdrproc_t xdr_results;
};
typedef struct rmtcallres rmtcallres;
#ifdef __STDC__
bool_t xdr_rmtcall_args(XDR *, struct rmtcallargs *);
#else
bool_t xdr_rmtcall_args();
#endif

#ifdef __cplusplus
}
#endif

#endif	/* ndef _KERNEL */

#endif	/* _RPC_PMAPRMT_H */
