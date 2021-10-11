/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FNSP_user_entries.cc	1.14	96/04/05 SMI"


#include <xfn/fn_p.hh>
#include <xfn/fnselect.hh>

#include "FNSP_entries.hh"
#include "FNSP_enterprise.hh"

extern int
FNSP_home_hierarchy_p(const FN_string &name);


// These are definitions of the subclass specific constructors and
// resolution methods for each type of user-related entry in
// the initial context.

FNSP_InitialContext::UserEntry::UserEntry(int ns, uid_t uid,
	const FNSP_enterprise_user_info *uinfo) : Entry(ns)
{
	my_uid = uid;
	my_user_info = uinfo;
}

FNSP_InitialContext::UserEntry::UserEntry(int ns, uid_t uid)
: Entry(ns)
{
	my_uid = uid;
	my_user_info = 0;
}


FNSP_InitialContext_UserOrgUnitEntry::
FNSP_InitialContext_UserOrgUnitEntry(int ns, uid_t uid,
	const FNSP_enterprise_user_info *uinfo) :
FNSP_InitialContext::UserEntry(ns, uid, uinfo)
{
	num_names = 2;
	stored_name_type = FNSP_MYORGUNIT;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_myorgunit");
	stored_names[1] = new FN_string((unsigned char *)"myorgunit");
}

void
FNSP_InitialContext_UserOrgUnitEntry::generate_equiv_names(void)
{
	FN_string *shortform;
	FN_string *longform =
	    FNSP_get_enterprise(name_service)->get_user_orgunit_name(
		my_uid, my_user_info, &shortform);

	if (shortform != NULL)
		++num_equiv_names;
	if (longform != NULL)
		++num_equiv_names;

	stored_equiv_names = new FN_string* [num_equiv_names];
	int i = 0;

	if (longform != NULL)
		stored_equiv_names[i++] = longform;
	if (shortform != NULL)
		stored_equiv_names[i++] = shortform;
}

void
FNSP_InitialContext_UserOrgUnitEntry::resolve(unsigned int auth)
{
	const FN_string *UserOrgUnit_name = unlocked_equiv_name();

	if (UserOrgUnit_name) {
		FN_ref *org_ref = FNSP_reference(
		    *FNSP_get_enterprise(name_service)->get_addr_type(),
		    *UserOrgUnit_name,
		    FNSP_organization_context);
		if (org_ref) {
			FN_status status;
			FN_ctx* ctx = FN_ctx::from_ref(*org_ref, auth, status);
			if (ctx) {
				stored_ref = ctx->lookup((unsigned char *)
				    "/", status);
				delete ctx;
			}
			delete org_ref;
			stored_status_code = status.code();
		} else
			stored_status_code = FN_E_INSUFFICIENT_RESOURCES;
	} else {
		stored_status_code = FN_E_NAME_NOT_FOUND;
	}
}


FNSP_InitialContext_ThisUserEntry::
FNSP_InitialContext_ThisUserEntry(int ns, uid_t uid,
	const FNSP_enterprise_user_info *uinfo)
: FNSP_InitialContext::UserEntry(ns, uid, uinfo)
{
	num_names = 3;
	stored_name_type = FNSP_MYSELF;
	stored_names = new FN_string* [num_names];
	stored_names[0] = new FN_string((unsigned char *)"_myself");
	stored_names[1] = new FN_string((unsigned char *)"myself");
	stored_names[2] = new FN_string((unsigned char *)"thisuser");
}

void
FNSP_InitialContext_ThisUserEntry::generate_equiv_names()
{
	num_equiv_names = 1;
	stored_equiv_names = new FN_string *[num_equiv_names];
	stored_equiv_names[0] =
	    FNSP_get_enterprise(name_service)->get_user_name(
	    my_uid, my_user_info);
}

void
FNSP_InitialContext_ThisUserEntry::resolve(unsigned int auth)
{
	const FN_string *username = unlocked_equiv_name();

	if (username == 0) {
		stored_status_code = FN_E_NAME_NOT_FOUND;
		return;
	}

	/* target name is _myorgunit/user/<username>/    */
	/* Need resolve piecewise to maintain authoritativeness */

	FN_status status;
	FN_ctx_svc* ctx = FNSP_InitialContext_from_initial(
	    auth, name_service, FNSP_USER_IC, my_uid, status);
	if (ctx) {
		FN_ctx *tctx;
		FN_ref *tref = ctx->lookup((unsigned char *)"_myorgunit/",
		    status);
		delete ctx;

		if (tref && (tctx = FN_ctx::from_ref(*tref, auth, status))) {
			delete tref;
			tref = tctx->lookup((unsigned char *)"_user/", status);
			if (tref &&
			    (tctx = FN_ctx::from_ref(*tref, auth, status))) {
				stored_ref = tctx->lookup(*username, status);
			}
		}
		delete tref;
	}
	stored_status_code = status.code();
}

FNSP_InitialContext_UserSiteEntry::
FNSP_InitialContext_UserSiteEntry(int ns, uid_t uid,
    const FNSP_enterprise_user_info *uinfo) :
FNSP_InitialContext::UserEntry(ns, uid, uinfo)
{
	num_names = 2;
	stored_name_type = FNSP_MYSITE;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_mysite");
	stored_names[1] = new FN_string((unsigned char *)"mysite");
}

void
FNSP_InitialContext_UserSiteEntry::resolve(unsigned int /* auth */)
{
	// %%% do not know to figure out affliation yet
	stored_status_code = FN_E_NAME_NOT_FOUND;
}

FNSP_InitialContext_UserENSEntry::
FNSP_InitialContext_UserENSEntry(int ns, uid_t uid,
	const FNSP_enterprise_user_info *uinfo):
FNSP_InitialContext::UserEntry(ns, uid, uinfo)
{
	num_names = 2;
	stored_name_type = FNSP_MYENS;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_myens");
	stored_names[1] = new FN_string((unsigned char *)"myens");
}

int
FNSP_InitialContext_UserENSEntry::is_equiv_name(const FN_string *str)
{
	get_reader_lock();

	if (stored_equiv_names == NULL) {
		release_reader_lock();
		lock_and_generate_equiv_names();
		get_reader_lock();
	}

	const FN_string *myequiv;
	int i;

	// For null 'str', mere existence of entry is sufficient
	// num_equiv_names == -1 if entry does not exist
	if (str == NULL) {
		release_reader_lock();
		if (num_equiv_names == 0)
			return (1);
		else
			return (0);
	}

	// use case-insensitive compare for most liberal interpretation of eq.
	// However, strictly speaking, true eq can only be determined
	// at the context where name is bound

	for (i = 0; i < num_equiv_names; i++) {
		myequiv = stored_equiv_names[i];
		if (myequiv != NULL &&
		    myequiv->compare(*str, FN_STRING_CASE_INSENSITIVE) == 0) {
			release_reader_lock();
			return (1);
		}
	}
	release_reader_lock();
	return (0);
}

void
FNSP_InitialContext_UserENSEntry::generate_equiv_names(void)
{
	if (num_equiv_names < 0)
		return;

	// If the name service is not NIS+, we need not check the
	// for user's and the current machine's domainname to be the same
	if (name_service != FNSP_nisplus_ns) {
		FNSP_InitialContext::Entry::generate_equiv_names();
		return;
	}

	FN_string *userorgunit_name =
	    FNSP_get_enterprise(name_service)->get_user_orgunit_name(
	    my_uid, my_user_info);

	// If user's orgunit is not in current NIS+ hierarchy
	// then, we don't know how to figure out myens
	if (userorgunit_name == NULL ||
	    !FNSP_home_hierarchy_p(*userorgunit_name)) {
		stored_status_code = FN_E_NAME_NOT_FOUND;
		delete userorgunit_name;
		num_equiv_names = -1;
		return;
	}
	delete userorgunit_name;

	FNSP_InitialContext::Entry::generate_equiv_names();
}


void
FNSP_InitialContext_UserENSEntry::resolve(unsigned int /* auth */)
{
	if (num_equiv_names < 0)
		return;

	// Since the FNSP_home_hierarchy is NIS+ specific call
	// the following instructions are executed only for NIS+
	// Moreover, there are no sub-domains in NIS and files
	if (name_service == FNSP_nisplus_ns) {
		FN_string *userorgunit_name =
		    FNSP_get_enterprise(name_service)->
		    get_user_orgunit_name(my_uid, my_user_info);

		// If user's orgunit is not in current NIS+ hierarchy
		// then, we don't know how to figure out myens
		if (userorgunit_name == NULL ||
		    !FNSP_home_hierarchy_p(*userorgunit_name)) {
			stored_status_code = FN_E_NAME_NOT_FOUND;
			delete userorgunit_name;
			num_equiv_names = -1;
			return;
		}
		delete userorgunit_name;
	}

	// Use thisens's orgroot name
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

#ifdef FN_IC_EXTENSIONS

/* functions enclosed here are currently not being used */

FNSP_InitialContext_UserOrgEntry::FNSP_InitialContext_UserOrgEntry(int ns)
: Entry(ns)
{
	num_names = 2;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_myorg");
	stored_names[1] = new FN_string((unsigned char *)"myorg");
}

void
FNSP_InitialContext_UserOrgEntry::resolve(unsigned int /* auth */)
{
	// %%% make that same as hostorg for now;
	const FN_string *root_dir =
	    FNSP_get_enterprise(name_service)->get_root_orgunit_name();

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

FNSP_InitialContext_UserUserEntry::FNSP_InitialContext_UserUserEntry(int ns)
: Entry(ns)
{
	num_names = 2;
	stored_names = new FN_string* [num_names];
	stored_names[0] = new FN_string((unsigned char *)"_myuser");
	stored_names[1] = new FN_string((unsigned char *)"myuser");
}


void
FNSP_InitialContext_UserUserEntry::resolve(unsigned int auth)
{
	// No other clean way to do this with the current design.
	// At this level the Entry doesn't "know" that it's a part of any
	// Table or context.
	FN_status status;
	FN_ctx_svc* ctx = FNSP_InitialContext_from_initial(auth,
	    name_service, status);
	if (ctx) {
		/* %%% should do this piecewise to maintain auth */
		stored_ref = ctx->lookup((unsigned char *)
		    "_myorgunit/_user/", status);
		stored_status_code = status.code();
		delete ctx;
	} else {
		stored_status_code = status.code();
	}
}



FNSP_InitialContext_UserHostEntry::FNSP_InitialContext_UserHostEntry(int ns)
: Entry(ns)
{
	num_names = 2;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_myhost");
	stored_names[1] = new FN_string((unsigned char *)"myhost");
}



void
FNSP_InitialContext_UserHostEntry::resolve(unsigned int auth)
{
	FN_status status;
	FN_ctx_svc *ctx = FNSP_InitialContext_from_initial(auth,
	    name_service, status);
	if (ctx) {
		/* %%% should do this piecewise to maintain auth */
		stored_ref = ctx->lookup((unsigned char *)
		    "_myorgunit/_host/", status);
		stored_status_code = status.code();
		delete ctx;
	} else {
		stored_status_code = status.code();
	}
}

FNSP_InitialContext_UserSiteRootEntry::
FNSP_InitialContext_UserSiteRootEntry(int ns)
: Entry(ns)
{
	num_names = 2;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_mysiteroot");
	stored_names[1] = new FN_string((unsigned char *)"mysiteroot");
}


void
FNSP_InitialContext_UserSiteRootEntry::resolve(unsigned int auth)
{
	FN_status status;
	FN_ctx_svc *ctx = FNSP_InitialContext_from_initial(auth,
	    name_service, status);
	if (ctx) {
		/* %%% should do this piecewise to maintain auth */
		stored_ref = ctx->lookup((unsigned char *)
		    "_myorg//_site/", status);
		stored_status_code = status.code();
		delete ctx;
	} else {
		stored_status_code = status.code();
	}
}

#endif /* FN_IC_EXTENSIONS */
