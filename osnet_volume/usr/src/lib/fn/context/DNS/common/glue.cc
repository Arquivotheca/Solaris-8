/*
 * Copyright (c) 1993 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)glue.cc	1.6	96/03/31 SMI"

#include <stdio.h>

#include "cx.hh"


/*
 * this is the entry point into the DNScontext module as a shared object
 *
 * NOTE:
 *	1. declared extern "C" so we can (easily) predict the function name
 *	   in the .so.  the caller binds to this name using dlsym().
 */

// fn_ctx_svc_handle_from_addr_ref()

extern "C"
FN_ctx_svc_t *
ASinet_domain(
	const FN_ref_addr_t *caddr,
	const FN_ref_t * /* cr */,
	unsigned int authoritative,
	FN_status_t *cs)
{
	FN_status	*s = (FN_status *)cs;
	FN_ref_addr	*ap = (FN_ref_addr *)caddr;

	if (DNS_ctx::get_trace_level())
		fprintf(stderr,
		    "inet_domain__fn_ctx_svc_handle_from_address: call\n");

	s->set_success();

	FN_ctx_svc	*newthing = new DNS_ctx(*ap, authoritative);

	return ((FN_ctx_svc_t *)newthing);
}

// fn_ctx_handle_from_addr_ref()

extern "C"
FN_ctx_t *
Ainet_domain(
	const FN_ref_addr_t *addr,
	const FN_ref_t *r,
	unsigned int auth,
	FN_status_t *s)
{
	if (DNS_ctx::get_trace_level())
		fprintf(stderr, "Ainet_domain: call\n");

	FN_ctx_svc_t *newthing =
	    ASinet_domain(addr, r, auth, s);

	FN_ctx *ctxobj = (FN_ctx_svc *)newthing;

	return ((FN_ctx_t *)ctxobj);
}
