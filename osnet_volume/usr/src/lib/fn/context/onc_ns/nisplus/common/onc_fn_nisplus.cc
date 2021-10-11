/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)onc_fn_nisplus.cc	1.7	96/03/31 SMI"

#include "FNSP_nisplusOrgContext.hh"
#include "FNSP_ENSContext.hh"
#include "FNSP_nisplusFlatContext.hh"
#include "FNSP_nisplusWeakSlashContext.hh"
#include "FNSP_nisplusDotContext.hh"
#include "FNSP_HostnameContext.hh"
#include "FNSP_UsernameContext.hh"
#include "FNSP_nisplusPrinternameContext.hh"
#include "FNSP_nisplusPrinterObject.hh"
#include "FNSP_nisplusImpl.hh"
#include "fnsp_internal.hh"

// Generate an FN_ctx_svc given a an address with type "onc_fn_nisplus"
// fn_ctx_svc_handle_from_ref_addr()

extern "C"
FN_ctx_svc_t *
ASonc_fn_nisplus(const FN_ref_addr_t *caddr,
    const FN_ref_t *cref, unsigned int auth, FN_status_t *cstat)
{
	const FN_ref_addr *addr = (const FN_ref_addr *)(caddr);
	const FN_ref *ref = (const FN_ref *)(cref);
	FN_status *stat = (FN_status *)(cstat);

	unsigned context_type = FNSP_address_context_type(*addr);
	FN_ctx_svc *answer = 0;

	switch (context_type) {
	case FNSP_organization_context:
		answer = FNSP_nisplusOrgContext::from_address(*addr, *ref,
		    auth, *stat);
		break;

	case FNSP_enterprise_context:
		answer = FNSP_ENSContext::from_address(*addr, *ref,
		    auth, *stat);
		break;

	case FNSP_nsid_context:
	case FNSP_user_context:
	case FNSP_host_context:
		answer = FNSP_nisplusFlatContext::from_address(*addr, *ref,
		    auth, *stat);
		break;

	case FNSP_service_context:
	case FNSP_generic_context:
		answer = FNSP_nisplusWeakSlashContext::from_address(*addr,
		    *ref, auth, *stat);
		break;

	case FNSP_site_context:
		/* hierarchical dot separated names */
		answer = FNSP_nisplusDotContext::from_address(*addr, *ref,
		    auth, *stat);
		break;

	case FNSP_hostname_context:
		answer = FNSP_HostnameContext::from_address(*addr, *ref,
		    auth, *stat);
		break;

	case FNSP_username_context:
		answer = FNSP_UsernameContext::from_address(*addr, *ref,
		    auth, *stat);
		break;

	default:
		stat->set(FN_E_MALFORMED_REFERENCE);
		break;
	}

	return ((FN_ctx_svc_t *)answer);
}

// fn_ctx_handle_from_ref_addr()

extern "C"
FN_ctx_t *
Aonc_fn_nisplus(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_nisplus(addr, ref, auth, stat);

	return ((FN_ctx_t *)sctx);
}

// Printer contexts

static const FN_identifier
FNSP_printer_nisplus_address_type((unsigned char *) "onc_fn_printer_nisplus");

extern "C"
FN_ctx_svc_t *
ASonc_fn_printer_nisplus(const FN_ref_addr_t *caddr,
    const FN_ref_t *cref, unsigned int auth, FN_status_t *cstat)
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
		answer = FNSP_nisplusPrinternameContext::from_address(*new_addr,
		    *new_ref, auth, *stat);
	else if ((*ref_type) ==
	    (*FNSP_reftype_from_ctxtype(FNSP_printer_object)))
		answer = FNSP_nisplusPrinterObject::from_address(*new_addr,
		    *new_ref, auth, *stat);
	else
		stat->set(FN_E_MALFORMED_REFERENCE);

	return ((FN_ctx_svc_t *) answer);
}

extern "C"
FN_ctx_t *
Aonc_fn_printer_nisplus(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_printer_nisplus(
	    addr, ref, auth, stat);

	return ((FN_ctx_t *)sctx);
}
