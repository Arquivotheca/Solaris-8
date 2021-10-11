/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)onc_fn_files.cc	1.4	96/06/27 SMI"

#include <xfn/fn_p.hh>
#include "FNSP_filesOrgContext.hh"
#include "FNSP_filesHUContext.hh"
#include "FNSP_filesFlatContext.hh"
#include "FNSP_filesWeakSlashContext.hh"
#include "FNSP_filesDotContext.hh"
#include "FNSP_filesPrinternameContext.hh"
#include "FNSP_filesPrinterObject.hh"

// Simulation of Federated /etc files when FNS maps are absent
#include "FNSP_filesOrgContext_default.hh"
#include "FNSP_filesFlatContext_default.hh"
#include "fnsp_files_internal.hh"

static FN_ref *
FNSP_generate_ens_ref()
{
	FN_string *nnsobjname = FNSP_files_get_org_nns_objname(0);
	if (nnsobjname) {
		FN_ref *answer = FNSP_reference(
		    FNSP_files_address_type_name(), *nnsobjname,
		    FNSP_enterprise_context);
		delete nnsobjname;
		return (answer);
	}
	return (0);
}

// Generate an FN_ctx_svc_t given an address with type "onc_fn_files"
// fn_ctx_svc_handle_from_ref_addr()

extern "C"
FN_ctx_svc_t *
ASonc_fn_files(const FN_ref_addr_t *caddr,
    const FN_ref_t *cref, unsigned int /* auth */,
    FN_status_t *cstat)
{
	const FN_ref_addr *addr = (FN_ref_addr*)(caddr);
	const FN_ref *ref = (FN_ref*)(cref);
	FN_status *stat = (FN_status*)(cstat);
	unsigned context_type = FNSP_address_context_type(*addr);
	FN_ctx_svc *answer = 0;

	// Declarations for FNSP_enterprise_context
	FN_ref *ens_ref;
	const FN_ref_addr *ens_addr;
	void *ip;

	if (FNSP_files_is_fns_installed(addr)) {
		switch (context_type) {
		case FNSP_organization_context:
			answer = FNSP_filesOrgContext::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_enterprise_context:
			ens_ref = FNSP_generate_ens_ref();
			if (!ens_ref) {
				stat->set(FN_E_INSUFFICIENT_RESOURCES);
			} else {
				ens_addr = ens_ref->first(ip);
				answer = FNSP_filesFlatContext::from_address(
				    *ens_addr, *ens_ref, *stat);
				delete ens_ref;
			}
			break;

		case FNSP_nsid_context:
		case FNSP_user_context:
		case FNSP_host_context:
			answer = FNSP_filesFlatContext::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_hostname_context:
		case FNSP_username_context:
			answer = FNSP_filesHUContext::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_service_context:
		case FNSP_generic_context:
			answer = FNSP_filesWeakSlashContext::from_address(*addr,
			    *ref, *stat);
			break;

		case FNSP_site_context:
			/* hierarchical dot separated names */
			answer = FNSP_filesDotContext::from_address(*addr,
			    *ref, *stat);
			break;

		default:
			stat->set(FN_E_MALFORMED_REFERENCE);
			break;
		}
	} else {
		switch (context_type) {
		case FNSP_organization_context:
			answer = FNSP_filesOrgContext_default::from_address(
			    *addr, *ref, *stat);
			break;

		case FNSP_nsid_context:
		case FNSP_service_context:
			answer = FNSP_filesFlatContext_default::from_address(
			    *addr, *ref, *stat);
			break;

		case FNSP_enterprise_context:
			ens_ref = FNSP_generate_ens_ref();
			if (!ens_ref) {
				stat->set(FN_E_INSUFFICIENT_RESOURCES);
			} else {
				ens_addr = ens_ref->first(ip);
				answer =
				    FNSP_filesFlatContext_default::from_address(
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
Aonc_fn_files(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_files(addr, ref, auth, stat);
	return ((FN_ctx_t *)sctx);
}

extern "C"
FN_ctx_svc_t *
ASonc_fn_printer_files(const FN_ref_addr_t *caddr,
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
		answer = FNSP_filesPrinternameContext::from_address(*new_addr,
		    *new_ref, *stat);
	else if ((*ref_type) ==
	    (*FNSP_reftype_from_ctxtype(FNSP_printer_object)))
		answer = FNSP_filesPrinterObject::from_address(*new_addr,
		    *new_ref, *stat);
	else
		stat->set(FN_E_MALFORMED_REFERENCE);

	delete new_ref;
	return ((FN_ctx_svc_t *) answer);
}

extern "C"
FN_ctx_t *
Aonc_fn_printer_files(const FN_ref_addr_t *addr,
    const FN_ref_t *ref, unsigned int auth, FN_status_t *stat)
{
	FN_ctx *sctx = (FN_ctx_svc *)ASonc_fn_printer_files(
	    addr, ref, auth, stat);

	return ((FN_ctx_t *)sctx);
}
