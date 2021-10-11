/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)from_initial.cc	1.12	96/04/05 SMI"

// Copyright (C) 1992 by Sun Microsystems, Inc.

#include "FNSP_InitialContext.hh"
#include "FNSP_enterprise.hh"
#include <xfn/fnselect.hh>
#include <synch.h>
#include <unistd.h>  /* for geteuid */

mutex_t uts_init_lock = DEFAULTMUTEX;   /* user table initialization lock */
mutex_t ht_init_lock = DEFAULTMUTEX;   /* host table initialization lock */
mutex_t gt_init_lock = DEFAULTMUTEX;   /* global table initialization lock */

// Initial Context that only has host bindings
FNSP_InitialContext::FNSP_InitialContext(unsigned int auth,
    int ns, HostTable *&ht)
: FN_ctx_svc(auth)
{
	if (ht == 0) {
		mutex_lock(&ht_init_lock);
		if (ht == 0) {
			ht = new HostTable(ns);
		}
		mutex_unlock(&ht_init_lock);
	}

	tables[FNSP_HOST_TABLE] = ht;
	tables[FNSP_USER_TABLE] = 0;
	tables[FNSP_GLOBAL_TABLE] = 0;
	tables[FNSP_CUSTOM_TABLE] = 0;
}


// Initial context for user related bindings only

FNSP_InitialContext::FNSP_InitialContext(unsigned int auth,
    int ns, uid_t uid, UserTable *&uts)
: FN_ctx_svc(auth)
{
	UserTable *ut = 0;

	if (uid) {
		// acquire lock here to examine list
		mutex_lock(&uts_init_lock);

		if (uts == 0)
			// never before set
			ut = uts = new UserTable(ns, uid, (UserTable*)0);
		else if ((ut = (UserTable*)(uts->find_user_table(uid))) == 0) {
			ut = new UserTable(ns, uid, uts);
			uts = ut;
		}
		mutex_unlock(&uts_init_lock);
	}

	tables[FNSP_HOST_TABLE] = 0;
	tables[FNSP_USER_TABLE] = ut;
	tables[FNSP_GLOBAL_TABLE] = 0;
	tables[FNSP_CUSTOM_TABLE] = 0;
}

// Initial Context for all bindigns

FNSP_InitialContext::FNSP_InitialContext(
    unsigned int auth,
    int ns,
    uid_t uid,
    HostTable *&ht,
    UserTable *&uts,
    GlobalTable *&gt,
    CustomTable *& /* ct */)
: FN_ctx_svc(auth)
{
	UserTable *ut = 0;

	if (ht == 0) {
		mutex_lock(&ht_init_lock);
		if (ht == 0) {
			ht = new HostTable(ns);
		}
		mutex_unlock(&ht_init_lock);
	}

	if (uid) {
		// acquire lock here to examine list
		mutex_lock(&uts_init_lock);

		if (uts == 0)
			// never before set
			ut = uts = new UserTable(ns, uid, (UserTable*)0);
		else if ((ut = (UserTable*)(uts->find_user_table(uid))) == 0) {
			ut = new UserTable(ns, uid, uts);
			uts = ut;
		}
		mutex_unlock(&uts_init_lock);
	}

	if (gt == 0) {
		mutex_lock(&gt_init_lock);
		if (gt == 0) {
			gt = new GlobalTable;
		}
		mutex_unlock(&gt_init_lock);
	}

	tables[FNSP_HOST_TABLE] = ht;
	tables[FNSP_USER_TABLE] = ut;
	tables[FNSP_GLOBAL_TABLE] = gt;
	tables[FNSP_CUSTOM_TABLE] = 0;
}

FNSP_InitialContext::~FNSP_InitialContext()
{
	// if a usage count scheme is implemented, then decrement usage
	// count for the table here.
}

FN_ctx_svc*
FNSP_InitialContext_from_initial(
    unsigned int auth, int ns,
    FNSP_IC_type initial_context_type,
    uid_t uid, FN_status &status)
{
	// All callers in this address space get a different FNSP_InitialContext
	// object but with a pointer to the same bindings tables.
	// They share state, but can delete the context object independently
	// without deleting the state.  State remains allocated until process
	// termination.

	// Note: C++ defn. guarantees local static is initialized to 0
	// before the first time through this block.

	// There is an authoritative and a default table for each type
	// The second index is for name service specific information
	// Index is made 4 to directly access the tables using ns
	static FNSP_InitialContext::UserTable* uts[2][4]; /* linked list */
	static FNSP_InitialContext::HostTable* ht[2][4];
	static FNSP_InitialContext::GlobalTable* gt[2][4];
	static FNSP_InitialContext::CustomTable* ct[2][4];

	if (auth != 0)
		auth = 1; /* auth can be either 1 or 0 for access of tables */

	FN_ctx_svc* ctx;
	switch (initial_context_type) {
	case FNSP_HOST_IC:
		ctx = (FN_ctx_svc*) new FNSP_InitialContext(auth,
		    ns, ht[auth][ns]);
		break;
	case FNSP_USER_IC:
		ctx = (FN_ctx_svc*) new FNSP_InitialContext(auth, ns,
		    uid, uts[auth][ns]);
		break;
	case FNSP_ALL_IC:
		ctx = (FN_ctx_svc*) new FNSP_InitialContext(auth, ns,
		    uid, ht[auth][ns], uts[auth][ns],
		    gt[auth][ns], ct[auth][ns]);
		break;
	default:
		ctx = 0;
	}

	if (ctx) {
		status.set_success();
	} else {
		status.set(FN_E_INSUFFICIENT_RESOURCES, 0, 0, 0);
	}
	return (ctx);
}

extern "C"
FN_ctx_svc_t *
initial__fn_ctx_svc_handle_from_initial(unsigned int auth, FN_status_t *status)
{
	uid_t uid = geteuid();
	int ns = fnselect();
	FN_ctx_svc *cc_ctx = FNSP_InitialContext_from_initial
	    (auth, ns, FNSP_ALL_IC, uid, *((FN_status *)status));

	return ((FN_ctx_svc_t *)cc_ctx);
}

extern "C"
FN_ctx_t *
initial__fn_ctx_handle_from_initial(unsigned int auth, FN_status_t *status)
{
	FN_ctx *cc_ctx = (FN_ctx_svc *)
	    initial__fn_ctx_svc_handle_from_initial(auth, status);

	return ((FN_ctx_t *)cc_ctx);
}

extern "C"
FN_ctx_t *
initial__fn_ctx_handle_from_initial_with_uid(
	uid_t uid,
	unsigned int auth,
	FN_status_t *status)
{
	int ns = fnselect();
	FN_ctx *cc_ctx = FNSP_InitialContext_from_initial
	    (auth, ns, FNSP_ALL_IC, uid, *((FN_status *)status));

	return ((FN_ctx_t *)cc_ctx);
}

extern "C"
FN_ctx_t *
initial__fn_ctx_handle_from_initial_with_ns(
	int ns,
	unsigned int auth,
	FN_status_t *status)
{
	uid_t uid = geteuid();
	FN_ctx *cc_ctx = (FN_ctx_svc *)
	    FNSP_InitialContext_from_initial
	    (auth, ns, FNSP_ALL_IC, uid, *((FN_status *)status));

	return ((FN_ctx_t *)cc_ctx);
}
