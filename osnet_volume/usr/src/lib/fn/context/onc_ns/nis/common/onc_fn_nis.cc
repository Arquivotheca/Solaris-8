/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)onc_fn_nis.cc	1.7	96/06/27 SMI"

#include "FNSP_nisOrgContext.hh"
#include "FNSP_nisFlatContext.hh"
#include "FNSP_nisHUContext.hh"
#include "FNSP_nisWeakSlashContext.hh"
#include "FNSP_nisDotContext.hh"
#include "FNSP_nisPrinternameContext.hh"
#include "FNSP_nisPrinterObject.hh"
#include <xfn/fn_p.hh>
#include <rpc/types.h>
#include <rpc/xdr.h>

// Simulation of Federated NIS when FNS maps are absent
#include "FNSP_nisOrgContext_default.hh"
#include "FNSP_nisFlatContext_default.hh"
#include "FNSP_nisImpl.hh"
#include "fnsp_nis_internal.hh"

// Generate the reference for enterprise root
static FN_ref *
FNSP_generate_ens_ref(const FN_ref *ref)
{
	FN_string *dirname = FNSP_reference_to_internal_name(*ref);
	if (dirname == NULL)
		return (0);
	FN_string *nnsobjname = FNSP_nis_get_org_nns_objname(dirname);
	delete dirname;
	if (nnsobjname) {
		FN_ref *answer = FNSP_reference(
		    FNSP_nis_address_type_name(),
		    *nnsobjname, FNSP_enterprise_context);
		delete nnsobjname;
		return (answer);
	}
	return (0);
}

// Generate an FN_ctx given a an address with type "onc_fn_nis"
extern "C"
FN_ctx_svc_t *
ASonc_fn_nis(const FN_ref_addr_t *caddr,
    const FN_ref_t *cref, int /* auth */, FN_status_t *cstat)
{
	const FN_ref_addr *addr = (const FN_ref_addr *)(caddr);
	const FN_ref *ref = (const FN_ref *)(cref);
	FN_status *stat = (FN_status *)(cstat);

	unsigned context_type = FNSP_address_context_type(*addr);
	FN_ctx_svc *answer = 0;

	// Declarations for FNSP_enterprise_context
	FN_ref *ens_ref;
	const FN_ref_addr *ens_addr;
	void *ip;

	if (FNSP_is_fns_installed(addr)) {
		switch (context_type) {
		case FNSP_organization_context:
			answer = FNSP_nisOrgContext::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_enterprise_context:
			ens_ref = FNSP_generate_ens_ref(ref);
			if (!ens_ref) {
				stat->set(FN_E_INSUFFICIENT_RESOURCES);
			} else {
				ens_addr = ens_ref->first(ip);
				answer =
				    FNSP_nisFlatContext::from_address(*ens_addr,
				    *ens_ref, *stat);
				delete ens_ref;
			}
			break;

		case FNSP_nsid_context:
		case FNSP_user_context:
		case FNSP_host_context:
			answer = FNSP_nisFlatContext::from_address(*addr,
			    *ref, *stat);
			break;
		case FNSP_hostname_context:
		case FNSP_username_context:
			answer = FNSP_nisHUContext::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_service_context:
		case FNSP_generic_context:
			answer = FNSP_nisWeakSlashContext::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_site_context:
			/* hierarchical dot separated names */
			answer = FNSP_nisDotContext::from_address(*addr,
			    *ref, *stat);
			break;

		default:
			stat->set(FN_E_MALFORMED_REFERENCE);
			break;
		}
	} else {
		switch (context_type) {
		case FNSP_organization_context:
			answer = FNSP_nisOrgContext_default::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_nsid_context:
		case FNSP_service_context:
			answer = FNSP_nisFlatContext_default::from_address(
			    *addr, *ref, *stat);
			break;

		case FNSP_enterprise_context:
			ens_ref = FNSP_generate_ens_ref(ref);
			if (!ens_ref) {
				stat->set(FN_E_INSUFFICIENT_RESOURCES);
			} else {
				ens_addr = ens_ref->first(ip);
				answer =
				    FNSP_nisFlatContext_default::from_address(
				    *ens_addr, *ens_ref, *stat);
				delete ens_ref;
			}
			break;

		case FNSP_site_context:
		case FNSP_username_context:
		case FNSP_hostname_context:
		case FNSP_user_context:
		case FNSP_host_context:
		case FNSP_null_context:
		case FNSP_generic_context:
			stat->set(FN_E_OPERATION_NOT_SUPPORTED);
			break;

		default:
			stat->set(FN_E_MALFORMED_REFERENCE);
			break;
		}
	}
	return ((FN_ctx_svc_t *)answer);
}

// fn_ctx_handle_from_ref_addr()
extern "C"
FN_ctx_t *
Aonc_fn_nis(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_nis(addr, ref, auth, stat);
	return ((FN_ctx_t *)sctx);
}

extern "C"
FN_ctx_svc_t *
ASonc_fn_printer_nis(const FN_ref_addr_t *caddr,
    const FN_ref_t *cref, unsigned int, FN_status_t *cstat)
{
	const FN_ref_addr *addr = (const FN_ref_addr *) (caddr);
	const FN_ref *ref = (const FN_ref *) (cref);
	FN_status *stat = (FN_status *) cstat;

	// If the context type is of old format to convert to new format
	FN_ref *new_ref;
	const FN_ref_addr *new_addr;
	void *ip;
	unsigned ctx_type, repr_type;
	FN_string *iname = FNSP_address_to_internal_name(*addr,
	    &ctx_type, &repr_type);
	if ((ctx_type == 1) || (ctx_type == 2)) {
		if (ctx_type == 1)
			ctx_type = FNSP_printername_context;
		else if (ctx_type == 2)
			ctx_type = FNSP_printer_object;

		// Construct the new reference
		new_ref = FNSP_reference(*addr->type(),
		    *ref->type(), *iname, ctx_type, repr_type);

		// New address
		new_addr = new_ref->first(ip);

		for (addr = ref->first(ip); addr; addr = ref->next(ip)) {
			if ((*addr->type()) != (*new_addr->type()))
				new_ref->append_addr(*addr);
		}
	} else {
		new_ref = new FN_ref(*ref);
		new_addr = addr;
	}
	delete iname;

	FN_ctx_svc *answer = 0;
	const FN_identifier *ref_type = new_ref->type();
	if ((*ref_type) ==
	    (*FNSP_reftype_from_ctxtype(FNSP_printername_context)))
		answer = FNSP_nisPrinternameContext::from_address(*new_addr,
		    *new_ref, *stat);
	else if ((*ref_type) ==
	    (*FNSP_reftype_from_ctxtype(FNSP_printer_object)))
		answer = FNSP_nisPrinterObject::from_address(*new_addr,
		    *new_ref, *stat);
	else
		stat->set(FN_E_MALFORMED_REFERENCE);

	delete new_ref;
	return ((FN_ctx_svc_t *) answer);
}

extern "C"
FN_ctx_t *
Aonc_fn_printer_nis(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_printer_nis(
	    addr, ref, auth, stat);

	return ((FN_ctx_t *)sctx);
}

// Functions to support onc_fn_nis_root address types
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

extern "C"
FN_ctx_svc_t *
ASonc_fn_nis_root(const FN_ref_addr_t *caddr,
    const FN_ref_t * /* cref */, unsigned int, FN_status_t *cstat)
{
	FN_ctx_svc *answer = 0;
	const FN_ref_addr *addr = (const FN_ref_addr *) (caddr);
	FN_status *stat = (FN_status *) cstat;

	// Can only be of enterprise context
	char *nis_contents = __decode_buffer((char *) addr->data(),
	    addr->length());
	if (nis_contents == 0) {
		stat->set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	unsigned status;
	if ((status = FNSP_nis_bind(nis_contents)) != FN_SUCCESS) {
		free(nis_contents);
		stat->set(status);
		return (0);
	}

	// Get the internal name = domainname + machine name
	FN_string *dirname = new FN_string((unsigned char *) nis_contents);
	free(nis_contents);
	if (dirname == 0) {
		stat->set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	// Obtain the NNS-object name ie., _FNS_SELF....
	FN_string *nnsobjname = FNSP_nis_get_org_nns_objname(dirname);
	delete dirname;
	if (nnsobjname == 0) {
		stat->set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	// Construct the reference
	void *ip;
	FN_ref *ref = FNSP_reference(FNSP_nis_address_type_name(),
	    *nnsobjname, FNSP_enterprise_context);
	delete nnsobjname;
	if (ref == 0) {
		stat->set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	// Construct the enterprise context
	addr = ref->first(ip);
	if (addr == 0) {
		stat->set(FN_E_INSUFFICIENT_RESOURCES);
		delete ref;
		return (0);
	}
	answer = FNSP_nisFlatContext::from_address(*addr,
	    *ref, *stat);
	delete ref;

	return ((FN_ctx_svc_t *)answer);
}

extern "C"
FN_ctx_t *
Aonc_fn_nis_root(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_nis_root(
	    addr, ref, auth, stat);

	return ((FN_ctx_t *)sctx);
}
