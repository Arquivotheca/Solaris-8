/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_host_entries.cc	1.14	98/11/10 SMI"


#include <xfn/fn_p.hh>

#include "FNSP_entries.hh"
#include "FNSP_enterprise.hh"

// These are definitions of the subclass specific constructors and
// resolution methods for each type of host-related entry in
// the initial context.


FNSP_InitialContext_HostOrgUnitEntry::
    FNSP_InitialContext_HostOrgUnitEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 2;
	stored_name_type = FNSP_THISORGUNIT;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_thisorgunit");
	stored_names[1] = new FN_string((unsigned char *)"thisorgunit");
}

void
FNSP_InitialContext_HostOrgUnitEntry::generate_equiv_names()
{
	FN_string *shortform;
	FN_string *longform =
	    FNSP_get_enterprise(name_service)->
	    get_host_orgunit_name(&shortform);

	if (shortform != NULL)
		++num_equiv_names;
	if (longform != NULL)
		++num_equiv_names;

	stored_equiv_names = new FN_string* [num_equiv_names];
	int i = 0;

	if (longform)
		stored_equiv_names[i++] = longform;
	if (shortform)
		stored_equiv_names[i++] = shortform;
}

void
FNSP_InitialContext_HostOrgUnitEntry::resolve(unsigned int auth)
{
	const FN_string *hostorgunit_name = unlocked_equiv_name();
	if (hostorgunit_name) {
		FN_ref *org_ref = FNSP_reference(
		    *FNSP_get_enterprise(name_service)->get_addr_type(),
		    *hostorgunit_name,
		    FNSP_organization_context);
		if (org_ref) {
			FN_status status;
			FN_ctx* ctx = FN_ctx::from_ref(*org_ref, auth, status);
			if (ctx) {
				stored_ref =
				    ctx->lookup((unsigned char *)"/",
				    status);
				delete ctx;
			}
			stored_status_code = status.code();
			delete org_ref;
		}
	} else {
		stored_status_code = FN_E_NAME_NOT_FOUND;
	}
}

FNSP_InitialContext_ThisHostEntry::
    FNSP_InitialContext_ThisHostEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 2;
	stored_name_type = FNSP_THISHOST;
	stored_names = new FN_string* [num_names];
	stored_names[0] = new FN_string((unsigned char *)"_thishost");
	stored_names[1] = new FN_string((unsigned char *)"thishost");
}


void
FNSP_InitialContext_ThisHostEntry::generate_equiv_names()
{
	num_equiv_names = 1;
	stored_equiv_names = new FN_string *[num_equiv_names];

	stored_equiv_names[0] =
	    FNSP_get_enterprise(name_service)->get_host_name();
}


void
FNSP_InitialContext_ThisHostEntry::resolve(unsigned int auth)
{
	const FN_string *hostname = unlocked_equiv_name();
	FN_status status;
	FN_ctx_svc* ctx;

	if (hostname == 0) {
		stored_status_code = FN_E_NAME_NOT_FOUND;
		return;
	}

	/* do resolution piecewise to maintained authoritativeness */

	if (ctx = FNSP_InitialContext_from_initial(auth,
	    name_service, FNSP_HOST_IC, 0, status)) {
		FN_ctx *hctx;
		FN_ref *hn_ref =
			ctx->lookup((unsigned char *)"_host/", status);
		delete ctx;
		if (hn_ref &&
		    (hctx = FN_ctx::from_ref(*hn_ref, auth, status))) {
			stored_ref = hctx->lookup(*hostname, status);
			delete hctx;
		}
		delete hn_ref;
	}

	stored_status_code = status.code();
}

FNSP_InitialContext_HostSiteEntry::FNSP_InitialContext_HostSiteEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 2;
	stored_name_type = FNSP_THISSITE;
	stored_names = new FN_string* [num_names];
	stored_names[0] = new FN_string((unsigned char *)"_thissite");
	stored_names[1] = new FN_string((unsigned char *)"thissite");
}

void
FNSP_InitialContext_HostSiteEntry::resolve(unsigned int /* auth */)
{
	// %%% do not know how to figure out affliation yet
	stored_status_code = FN_E_NAME_NOT_FOUND;
}

FNSP_InitialContext_HostENSEntry::FNSP_InitialContext_HostENSEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 2;
	stored_name_type = FNSP_THISENS;
	stored_names = new FN_string* [num_names];
	stored_names[0] = new FN_string((unsigned char *)"_thisens");
	stored_names[1] = new FN_string((unsigned char *)"thisens");
}

void
FNSP_InitialContext_HostENSEntry::resolve(unsigned int /* auth */)
{
	const FN_string *root_dir =
	    FNSP_get_enterprise(name_service)->get_root_orgunit_name();

	if (root_dir) {
		stored_ref = FNSP_reference(
		    *FNSP_get_enterprise(name_service)->get_addr_type(),
		    *root_dir,
		    FNSP_enterprise_context);
		if (stored_ref) {
			stored_status_code = FN_SUCCESS;
		} else {
			stored_status_code = FN_E_INSUFFICIENT_RESOURCES;
		}
	} else {
		stored_status_code = FN_E_NAME_NOT_FOUND;
	}
}

// *************************************************************
// the following chooses host-centric, org-centric relationships

FNSP_InitialContext_HostOrgEntry::FNSP_InitialContext_HostOrgEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 3;
	stored_name_type = FNSP_ORGUNIT;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_orgunit");
	stored_names[1] = new FN_string((unsigned char *)"orgunit");
	stored_names[2] = new FN_string((unsigned char *)"org");
}

void
FNSP_InitialContext_HostOrgEntry::generate_equiv_names()
{
	const FN_string *tmp =
	    FNSP_get_enterprise(name_service)->get_root_orgunit_name();

	if (tmp) {
		num_equiv_names = 2;
		stored_equiv_names = new FN_string* [num_equiv_names];
		stored_equiv_names[0] = new FN_string (*tmp);
		stored_equiv_names[1] = new FN_string((unsigned char *)"");
	}
}

void
FNSP_InitialContext_HostOrgEntry::resolve(unsigned int /* auth */)
{
	const FN_string *root_dir = unlocked_equiv_name();

	if (root_dir) {
		stored_ref = FNSP_reference(
		    *FNSP_get_enterprise(name_service)->get_addr_type(),
		    *root_dir,
		    FNSP_organization_context);
		if (stored_ref) {
			stored_status_code = FN_SUCCESS;
		} else {
			stored_status_code = FN_E_INSUFFICIENT_RESOURCES;
		}
	} else {
		stored_status_code = FN_E_NAME_NOT_FOUND;
	}
}


FNSP_InitialContext_HostSiteRootEntry::
    FNSP_InitialContext_HostSiteRootEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 2;
	stored_name_type = FNSP_SITE;
	stored_names = new FN_string* [num_names];
	stored_names[0] = new FN_string((unsigned char *)"_site");
	stored_names[1] = new FN_string((unsigned char *)"site");
}


void
FNSP_InitialContext_HostSiteRootEntry::resolve(unsigned int auth)
{
	FN_status status;
	FN_ctx_svc *ctx = FNSP_InitialContext_from_initial(auth,
	    name_service, FNSP_HOST_IC, 0, status);

	if (ctx) {
		/* to maintain authoritative, must do resolution piece wise */
		FN_ref *ens_ref = ctx->lookup((unsigned char *)"_thisens/",
		    status);
		delete ctx;
		if (ens_ref) {
			FN_ctx *ectx;
			if (ectx = FN_ctx::from_ref(*ens_ref, auth, status)) {
				stored_ref = ectx->lookup(
				    (unsigned char *)"_site/", status);
				delete ectx;
			}
			delete ens_ref;
		}
	}
	stored_status_code = status.code();
}

FNSP_InitialContext_HostUserEntry::FNSP_InitialContext_HostUserEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 2;
	stored_name_type = FNSP_USER;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_user");
	stored_names[1] = new FN_string((unsigned char *)"user");
}

void
FNSP_InitialContext_HostUserEntry::resolve(unsigned int auth)
{
	FN_status status;
	FN_ctx_svc* ctx = FNSP_InitialContext_from_initial(auth,
	    name_service, FNSP_HOST_IC, 0, status);

	/* must do the resolution piecewise to maintain authoritativeness */
	if (ctx) {
		FN_ctx *octx;
		FN_ref *org_ref = ctx->lookup((unsigned char *)"_thisorgunit/",
		    status);
		delete ctx;
		if (org_ref &&
		    (octx = FN_ctx::from_ref(*org_ref, auth, status))) {
			stored_ref = octx->lookup((unsigned char *)"_user/",
			    status);
			delete octx;
		}
		delete org_ref;
	}
	stored_status_code = status.code();
}

FNSP_InitialContext_HostHostEntry::FNSP_InitialContext_HostHostEntry(int ns)
: FNSP_InitialContext::Entry(ns)
{
	num_names = 2;
	stored_name_type = FNSP_HOST;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_host");
	stored_names[1] = new FN_string((unsigned char *)"host");
}

void
FNSP_InitialContext_HostHostEntry::resolve(unsigned int auth)
{
	FN_status status;
	FN_ctx_svc* ctx = FNSP_InitialContext_from_initial(auth,
	    name_service, FNSP_HOST_IC, 0, status);

	/* must do the resolution piecewise to maintain authoritativeness */
	if (ctx) {
		FN_ctx *octx;
		FN_ref *org_ref = ctx->lookup((unsigned char *)"_thisorgunit/",
		    status);
		delete ctx;
		if (org_ref &&
		    (octx = FN_ctx::from_ref(*org_ref, auth, status))) {
			stored_ref = octx->lookup((unsigned char *)"_host/",
			    status);
			delete octx;
		}
		delete org_ref;
	}
	stored_status_code = status.code();
}
