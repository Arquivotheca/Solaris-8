/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)from_ref.cc	1.4	96/07/08 SMI"


// This file contains the implementation of the from_ref of
// the root NIS+ context --
// the context that makes the target NIS+ root accessible when
// a from-ref is done on the reference.
//
// from_ref() ensures that the target NIS+ hierarchy is accessible
// by updating the local NIS+ cache.
// It then calls from_ref() on the enterprise-root reference
// of the target NIS+ hierarchy.

#include <string.h>
#include <malloc.h>
#include <xfn/xfn.hh>
#include <xfn/fn_spi.hh>
#include <xfn/fn_p.hh>
#include <rpc/types.h>
#include <rpc/xdr.h>

#define	__MAX_ERROR_MSG 256


extern int
update_nisplus_cache(const char *directory_name,
    const char *server_name, const char *server_addr);

extern int
FNSP_home_hierarchy_p(const FN_string &name);

extern int
FNSP_potential_ancestor_p(const FN_string &name);

static const FN_identifier
FNSP_root_addr_type((const unsigned char *)"onc_fn_nisplus_root");

static const FN_identifier
FNSP_nisplus_address_type((unsigned char *) "onc_fn_nisplus");

// Address type is "onc_fn_nisplus_root"
// Address contents is XDR encoded
//	root_domain_name server_name [server_address]

static char *
__decode_buffer(const char *encoded_buf, int esize)
{
	char *decoded_buf = 0;
	XDR xdrs;
	bool_t status;

	xdrmem_create(&xdrs, (caddr_t) encoded_buf, esize, XDR_DECODE);
	status = xdr_wrapstring(&xdrs, &decoded_buf);

	if (status == FALSE)
		return (0);

	xdr_destroy(&xdrs);

	return (decoded_buf);
}

static void
__set_error_msg(const char *msg, const char *dirname, FN_status &status)
{
	char emsg_str[__MAX_ERROR_MSG];
	sprintf(emsg_str, "\n%s: %s\n", msg, dirname);
	FN_string emsg((const unsigned char *)emsg_str);
	status.set(FN_E_CONFIGURATION_ERROR);
	status.set_diagnostic_message(&emsg);
}

static FN_ctx_svc *
nisplus_root_from_ref(const FN_ref_addr &addr,
    const FN_ref & /* ref */,
    unsigned int auth,
    FN_status &status)
{
	const FN_identifier *tp = addr.type();
	const char *contents = (char *)addr.data();
	const int contents_size = addr.length();

	if (tp == 0 || (*tp != FNSP_root_addr_type)) {
		status.set(FN_E_MALFORMED_REFERENCE);
		return (0);
	}

	// Decode address
	char *mycontents = __decode_buffer((const char *)contents,
	    contents_size);
	char *directory_name;
	char *server_name;
	char *server_addr = 0;
	char *ptr;

	directory_name = strtok_r(mycontents, " \t\n", &ptr);
	server_name = strtok_r(NULL, " \t\n", &ptr);
	server_addr = strtok_r(NULL, " \t\n", &ptr);

	if (directory_name == NULL ||
	    (server_name == NULL && server_addr == NULL)) {
		status.set(FN_E_MALFORMED_REFERENCE);
		free(mycontents);
		return (0);
	}

	FN_string root_domain((const unsigned char *)directory_name);

	if (FNSP_home_hierarchy_p(root_domain)) {
		/*
		 * If directory in same hierarchy, do nothing.
		 * Valid directory name should be already reachable.
		 * Invalid directory name is security problem
		 * if we try to insert it into the cache.
		 */
		;
	} else if (FNSP_potential_ancestor_p(root_domain)) {
		/*
		 * Problematic if asked to put in a domain
		 * that could confuse subsequent nis_local_root().
		 */
		__set_error_msg(
		    "Not allowed to add potential ancestor to NIS+ cache",
		    directory_name, status);
		free(mycontents);
		return (0);
	} else if (update_nisplus_cache(directory_name, server_name,
	    server_addr) == 0) {
		__set_error_msg("Cannot update NIS+ cache",
				directory_name, status);
		free(mycontents);
		return (0);
	}
	free(mycontents);

	/* Construct Enterprise reference using domain name */
	FN_ctx_svc *root_ctx = 0;
	FN_ref *root_ref = FNSP_reference(FNSP_nisplus_address_type,
	    root_domain, FNSP_enterprise_context);

	if (root_ref)
		root_ctx = FN_ctx_svc::from_ref(*root_ref, auth, status);
	else
		status.set(FN_E_INSUFFICIENT_RESOURCES);

	return (root_ctx);
}

// fn_ctx_svc_handle_from_ref_addr()

extern "C"
FN_ctx_svc_t *
ASonc_fn_nisplus_root(
    const FN_ref_addr_t *a,
    const FN_ref_t *r,
    unsigned int auth,
    FN_status_t *s)
{
	FN_ref *rr = (FN_ref *)r;
	FN_status *ss = (FN_status *)s;
	FN_ref_addr *aa = (FN_ref_addr *)a;

	FN_ctx_svc *newthing = nisplus_root_from_ref(*aa, *rr, auth, *ss);
	return ((FN_ctx_svc_t *)newthing);
}

// fn_ctx_handle_from_ref_addr()

extern "C"
FN_ctx_t *
Aonc_fn_nisplus_root(
    const FN_ref_addr_t *a,
    const FN_ref_t *r,
    unsigned int auth,
    FN_status_t *s)
{
	FN_ctx_svc_t *newthing = ASonc_fn_nisplus_root(a, r, auth, s);

	FN_ctx* ctxobj = (FN_ctx_svc*)newthing;

	return ((FN_ctx_t *)ctxobj);
}
