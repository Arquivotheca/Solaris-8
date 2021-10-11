/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)onc_fn_fs_host.cc	1.2	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FSHost.hh"


// Routines for constructing context handles from references of
// type onc_fn_fs_host.
//
// Entry points exported by fn_ctx_onc_fn_fs_host.so$(VERS):
//	onc_fn_fs_host__fn_ctx_svc_handle_from_ref_addr()
//	onc_fn_fs_host__fn_ctx_handle_from_ref_addr()


// Generate an FN_ctx_svc given an address of type onc_fn_fs_host.
// fn_ctx_svc_handle_from_ref_addr()

extern "C"
FN_ctx_svc_t *
ASonc_fn_fs_host(const FN_ref_addr_t *caddr,
    const FN_ref_t *cref, unsigned int auth, FN_status_t *cstat)
{
	const FN_ref_addr *addr = (const FN_ref_addr *)(caddr);
	const FN_ref *ref = (const FN_ref *)(cref);
	FN_status *stat = (FN_status *)(cstat);

	FN_ctx_svc *answer = FSHost::from_address(*addr, *ref, auth, *stat);
	return ((FN_ctx_svc_t *)answer);
}


// fn_ctx_handle_from_ref_addr()

extern "C"
FN_ctx_t *
Aonc_fn_fs_host(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_fs_host(addr, ref, auth, stat);

	return ((FN_ctx_t *)sctx);
}
