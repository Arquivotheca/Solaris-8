/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fs_ctx_utils.cc	1.4	96/03/31 SMI"


// XFN-related utilities for fncreate_fs.


#include <stdlib.h>
#include <rpc/rpc.h>
#include <xfn/xfn.hh>
#include <xfn/fn_p.hh>
#include "fncreate_fs.hh"
#include "fncreate_attr_utils.hh"

// flag for obtaining authoritative handles
#define	AUTHORITATIVE 1

// Maximum size of XDR-encoded data in an address of a file system reference.
#define	ADDRESS_SIZE 1040


static const FN_identifier reftype((unsigned char *)"onc_fn_fs");
static const FN_identifier addrtype_mount((unsigned char *)"onc_fn_fs_mount");
static const FN_identifier addrtype_host((unsigned char *)"onc_fn_fs_host");
static const FN_identifier addrtype_user((unsigned char *)"onc_fn_fs_user");
static const FN_identifier
    addrtype_user_nisplus((unsigned char *)"onc_fn_fs_user_nisplus");

static const FN_string fs_string((unsigned char *)"fs");
static const FN_string _fs_string((unsigned char *)"_fs");
static const FN_string empty_string;


// Given a reference named by "resolved", create a context for it if
// it is not already a context, then create the subcontexts named by
// "remaining".  "remaining" may be null, indicating that there no
// additional contexts to create.  Return the reference of the context
// named by "resolved/remaining", and delete ref.

static FN_ref *create_fs_contexts(FN_ref *ref,
    const FN_composite_name *resolved, const FN_composite_name *remaining);


// This is the central file system context creation routine.  Create a
// subcontext of ctx with the given name and address if "context" is
// true, or a non-context binding if not.  If the name is already
// bound, modify the existing binding instead.  Ref is the existing
// reference if the name is already bound, and NULL otherwise.  Return
// the new or updated reference, and delete ref.

static FN_ref *set_fs_binding(FN_ctx *ctx, const FN_string *name,
    bool_t context, FN_ref *ref = NULL, const FN_ref_addr *addr = NULL);


// Given the reference of the non-context file system binding bound
// to "name" in ctx, replace the binding with a file system context
// and return the new reference.  Delete ref before returning.

static FN_ref *fs_binding_into_context(FN_ctx *ctx,
    const FN_string *name, FN_ref *ref);


// Create a new file system subcontext of ctx, bound to name, and
// return its reference.  If addr is non-null and non-empty, add it to
// the new reference.

static FN_ref *create_fs_context(FN_ctx *ctx, const FN_string *name,
    const FN_ref_addr *addr = NULL);


// Bind ref to name in ctx.  If ref has no addresses, then unbind name
// in ctx instead.

static void bind_fs_ref(FN_ctx *ctx, const FN_string *name, FN_ref *ref);


// Unbind name in ctx.

static void unbind_fs_ref(FN_ctx *ctx, const FN_string *name);


// Return the parent context of the given name.  Set parent_is_fs to true
// if this is a file system context.

static FN_ctx *parent_ctx(const FN_composite_name *name,
    bool_t &parent_is_fs);


// If ref contains addresses of any "fs" type, replace them with addr
// if addr is not empty, or delete them if addr is empty.  Return TRUE
// if ref is modified.

static bool_t replace_addr(FN_ref *ref, const FN_ref_addr *addr);


// Given a reference, its full name, and its parent context,
// recursively destroy all contexts and unbind all references at or
// below the reference.

static void destroy_ref(const FN_composite_name *fullname, FN_ctx *parent,
    FN_ref *ref);


// Create and return an address containing the XDR-encoding of the
// given string.

static FN_ref_addr *addr_from_str(const char *addrdata);


// Return true if addr is of type addrtype_mount and its data string is empty.

static bool_t addr_empty(const FN_ref_addr *addr);


FN_ctx *
penultimate_ctx(FN_composite_name *name, bool_t &nsid)
{
	// Get initial context.
	FN_status status;
	FN_ctx *init_ctx = FN_ctx::from_initial(AUTHORITATIVE, status);
	if (init_ctx == NULL) {
		error(&status, "Unable to get initial context");
	}

	// Resolve as much of name as possible, excluding last component.
	void *iter;
	const FN_string *last = name->last(iter);
	FN_composite_name *prefix = name->prefix(iter);
	mem_check(prefix);
	prefix->append_comp(empty_string);	// follow nns, if any
	FN_ref *ref = init_ctx->lookup(*prefix, status);
	delete init_ctx;

	// Set ref to the resolved portion of the reference.
	if (ref == NULL) {
		if (status.resolved_ref() == NULL) {
			error(&status, "Could not resolve %s", str(name));
		}
		ref = new FN_ref(*status.resolved_ref());
		mem_check(ref);
	}

	const FN_composite_name *resolved = status.resolved_name();
	const FN_composite_name *remaining = status.remaining_name();
	const FN_string *first;

	switch (status.code()) {
	case FN_SUCCESS:
		// This is an error unless either:
		// - the resolved reference is a file system ref, or
		// - the last component is "fs" or "_fs".

		resolved = prefix;
		remaining = NULL;
		nsid = (*ref->type() != reftype);
		if (nsid &&
		    last->compare(fs_string) != 0 &&
		    last->compare(_fs_string) != 0) {
			error(NULL,
				"Not a file system context: %s", str(name));
		}
		break;

	case FN_E_NOT_A_CONTEXT:
	case FN_E_NO_SUPPORTED_ADDRESS:
		// This is an error unless:
		// - the resolved reference is a file system ref.

		if (*ref->type() != reftype) {
			error(&status, "Could not resolve %s", str(name));
		}
		nsid = FALSE;
		break;

	case FN_E_NAME_NOT_FOUND:
		// This is an error unless either:
		// - the resolved reference is a file system ref, or
		// - the remaining name begins with "fs" or "_fs".

		first = remaining->first(iter);
		if (*ref->type() != reftype &&
		    first->compare(fs_string) != 0 &&
		    first->compare(_fs_string) != 0) {
			error(&status, "Could not resolve %s", str(name));
		}
		nsid = FALSE;
		break;

	default:
		error(&status, "Could not resolve %s", str(name));
	}

	if (verbose >= 3) {
		error(NULL, "Debug exit:  would create contexts \"%s\"",
			(remaining != NULL) ? str(remaining) : "");
	}

	// Create file system contexts for the part of name that could
	// not be resolved.
	ref = create_fs_contexts(ref, resolved, remaining);

	FN_ctx *ctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
	if (ctx == NULL) {
		error(&status, "Could not construct context handle for %s",
			str(prefix));
	}
	delete ref;
	delete prefix;
	return (ctx);
}


static FN_ref *
create_fs_contexts(FN_ref *ref, const FN_composite_name *resolved,
    const FN_composite_name *remaining)
{
	FN_status status;
	void *iter;
	bool_t ref_is_fs = (*ref->type() == reftype);

	// Make sure "resolved" names a context, not just a binding.

	FN_ctx *ctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
	if (ctx == NULL) {
		if (!ref_is_fs) {
			error(NULL, "%s is not a context", str(resolved));
		}
		bool_t parent_is_fs;
		ctx = parent_ctx(resolved, parent_is_fs);
		const FN_string *last = parent_is_fs
					? resolved->last(iter)
					: &fs_string;
		ref = set_fs_binding(ctx, last, TRUE, ref);
	}
	delete ctx;

	// Create new contexts for components of "remaining".

	if (remaining == NULL) {	// nothing more to create
		return (ref);
	}
	if (verbose >= 1) {
		info("creating subcontexts of %s:", str(resolved));
	}
	const FN_string *name = remaining->first(iter);
	if (*ref->type() != reftype) {
		name = &fs_string;
	}
	for (; name != NULL; name = remaining->next(iter)) {
		if (name->is_empty()) {
			continue;
		}
		ctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
		if (ctx == NULL) {
			error(&status, "Could not construct context handle");
		}
		delete ref;
		ref = set_fs_binding(ctx, name, TRUE);
		delete ctx;
	}
	return (ref);
}


void
update_namespace(FN_ctx *ctx, Dir *input, bool_t nsid)
{
	FN_status status;
	FN_ref *ref = ctx->lookup(*input->name, status);
	if (status.code() != FN_SUCCESS &&
	    status.code() != FN_E_NAME_NOT_FOUND) {
		error(&status,
			"Lookup of %s failed", str(input->name));
	}
	bool_t context = input->count() > 0;

	const FN_string *name = nsid ? &fs_string : input->name;

	if (input->location != NULL) {
		FN_ref_addr *addr = addr_from_str(input->location);
		ref = set_fs_binding(ctx, name, context, ref, addr);
		delete addr;
		if (verbose >= 1) {
			input->print_name_hierarchy();
			info(" %s", input->location);
		}
	} else {
		ref = set_fs_binding(ctx, name, context, ref);
	}
	if (context) {
		FN_ctx *subctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
		if (subctx == NULL) {
			error(&status, "Could not construct ctx handle for %s",
				str(name));
		}
		Dir *subtree;
		void *iter;
		for (subtree = (Dir *)input->first(iter);
		    subtree != NULL;
		    subtree = (Dir *)input->next(iter)) {
			update_namespace(subctx, subtree);
		}
		delete subctx;
	}
	delete ref;
}


static FN_ref *
set_fs_binding(FN_ctx *ctx, const FN_string *name, bool_t context,
    FN_ref *ref, const FN_ref_addr *addr)
{
	bool_t ref_changed = FALSE;	// Set to true if ref is updated

	if (ref == NULL) {
		// Name is not yet bound.
		if (context) {
			ref_changed = TRUE;
			ref = create_fs_context(ctx, name, addr);
		} else if ((addr != NULL) && !addr_empty(addr)) {
			ref_changed = TRUE;
			ref = new FN_ref(reftype);
			mem_check(ref);
			if (ref->append_addr(*addr) == 0) {
				error(NULL, "Memory allocation failure");
			}
			bind_fs_ref(ctx, name, ref);
		}
	} else {
		// Name is already bound.  Update reference if necessary.
		if (addr != NULL) {
			ref_changed = replace_addr(ref, addr);
		}

		FN_status status;
		FN_ctx *subctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
		delete subctx;	// only need the status code

		switch (status.code()) {
		case FN_SUCCESS:
			if (ref_changed) {
				bind_fs_ref(ctx, name, ref);
			}
			break;
		case FN_E_NO_SUPPORTED_ADDRESS:
		case FN_E_MALFORMED_REFERENCE:
			// Existing binding is not a context.
			if (context) {
				ref_changed = TRUE;
				ref = fs_binding_into_context(ctx, name, ref);
			} else if (ref_changed) {
				bind_fs_ref(ctx, name, ref);
			}
			break;
		default:
			error(&status, "Could not construct ctx handle for %s",
				str(name));
		}
	}

	// If name is "fs" in an nsid context, bind the ref to "_fs" as well.
	if (ref_changed && (name == &fs_string)) {
		bind_fs_ref(ctx, &_fs_string, ref);
	}
	return (ref);
}


static FN_ref *
fs_binding_into_context(FN_ctx *ctx, const FN_string *name, FN_ref *ref)
{
	if (verbose >= 1) {
		info("  replacing binding of %s with a context", str(name));
	}

	// Remove old binding.
	FN_status status;
	if (ctx->unbind(*name, status) == 0) {
		error(&status, "Could not remove binding of %s", str(name));
	}

	// Create new context.
	void *iter;
	FN_ref *new_ref = create_fs_context(ctx, name, ref->first(iter));
	delete ref;
	return (new_ref);
}


static FN_ref *
create_fs_context(FN_ctx *ctx, const FN_string *name,
    const FN_ref_addr *addr)
{
	if (verbose >= 1) {
		info("  creating context %s", str(name));
	}

	FN_status status;
	FN_attrset *attrs = generate_creation_attrs(FNSP_generic_context,
						    &reftype);
	if (attrs == NULL)
		error(NULL,
		    "Could not generate creation attributes for %s",
		    str(name));

	FN_ref *ref = ctx->attr_create_subcontext(*name, attrs, status);
	if (ref == NULL) {
		error(&status, "Could not create context %s", str(name));
	}
	if ((addr != NULL) && !addr_empty(addr)) {
		if (ref->append_addr(*addr) == 0) {
			error(NULL, "Could not construct reference for %s",
				str(name));
		}
		if (ctx->bind(*name, *ref, 0, status) == 0) {
			error(&status, "Could not rebind %s", str(name));
		}
	}
	return (ref);
}


static void
bind_fs_ref(FN_ctx *ctx, const FN_string *name, FN_ref *ref)
{
	if (ref->addrcount() == 0) {
		unbind_fs_ref(ctx, name);
		return;
	}
	if (verbose >= 1) {
		if (name == &_fs_string) {
			info("  binding alias %s", str(name));
		} else {
			info("  binding %s", str(name));
		}
	}
	FN_status status;
	if (ctx->bind(*name, *ref, 0, status) == 0) {
		error(&status, "Could not bind %s", str(name));
	}
}


static void
unbind_fs_ref(FN_ctx *ctx, const FN_string *name)
{
	if (verbose >= 1) {
		if (name == &_fs_string) {
			info("  unbinding alias %s", str(name));
		} else {
			info("  unbinding %s", str(name));
		}
	}
	FN_status status;
	if (ctx->unbind(*name, status) == 0) {
		error(&status, "Could not unbind %s", str(name));
	}
}


static FN_ctx *
parent_ctx(const FN_composite_name *name, bool_t &parent_is_fs)
{
	// Get initial context.
	FN_status status;
	FN_ctx *init_ctx = FN_ctx::from_initial(AUTHORITATIVE, status);
	if (init_ctx == NULL) {
		error(&status, "Unable to get initial context");
	}

	// Construct name of parent context.
	void *iter;
	const FN_string *last = name->last(iter);
	if (last->is_empty()) {		// ignore tailing empty component
		last = name->prev(iter);
	}
	FN_composite_name *parent = name->prefix(iter);
	if (parent == NULL ||
	    parent->append_comp(empty_string) == 0) {	// follow nns, if any
		error(NULL, "Memory allocation failure");
	}

	// Lookup parent context.
	FN_ref *ref = init_ctx->lookup(*parent, status);
	if (ref == NULL) {
		error(&status, "Could not resolve %s", str(parent));
	}
	delete init_ctx;

	// Check if parent context is a file system context.
	parent_is_fs = (*ref->type() == reftype);

	// Construct parent context handle.
	FN_ctx *ctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);
	if (ctx == NULL) {
		error(&status, "Could not construct context handle for %s",
			str(parent));
	}
	delete ref;
	delete parent;
	return (ctx);
}


static bool_t
replace_addr(FN_ref *ref, const FN_ref_addr *addr)
{
	bool_t ref_changed = FALSE;
	bool_t addr_to_be_added = !addr_empty(addr);

	size_t len = addr->length();
	const FN_ref_addr *a;
	void *iter;
	for (a = ref->first(iter); a != NULL; a = ref->next(iter)) {
		if (*a->type() == addrtype_mount ||
		    *a->type() == addrtype_user ||
		    *a->type() == addrtype_user_nisplus ||
		    *a->type() == addrtype_host) {
			if (!addr_to_be_added) {
				ref->delete_addr(iter);
				ref_changed = TRUE;
				continue;
			}
			if (*a->type() != addrtype_mount ||
			    a->length() != len ||
			    memcmp(a->data(), addr->data(), len) != 0) {
				ref->delete_addr(iter);
				ref->insert_addr(iter, *addr);
				ref_changed = TRUE;
			}
			addr_to_be_added = FALSE;
		}
	}
	if (addr_to_be_added) {
		ref->insert_addr(iter, *addr);
		ref_changed = TRUE;
	}
	return (ref_changed);
}


void
destroy(const FN_composite_name *fullname, FN_ctx *ctx, bool_t nsid)
{
	void *iter;
	const FN_string *name = nsid ? &fs_string : fullname->last(iter);

	FN_status status;
	FN_ref *ref = ctx->lookup(*name, status);
	if (ref != NULL) {
		destroy_ref(fullname, ctx, ref);
		delete ref;
	} else if (status.code() != FN_E_NAME_NOT_FOUND) {
		error(&status, "Lookup of %s failed", str(fullname));
	}

	if (nsid) {
		ref = ctx->lookup(_fs_string, status);
		if (ref == NULL) {
			return;
		}
		if (verbose >= 1) {
			info("  unbinding alias _fs of %s", str(fullname));
		}
		if (ctx->unbind(_fs_string, status) == 0) {
			error(&status, "Could not unbind alias _fs of %s",
				str(fullname));
		}
	}
}


static void
destroy_ref(const FN_composite_name *fullname, FN_ctx *parent, FN_ref *ref)
{
	if (*ref->type() != reftype) {
		error(NULL, "Attempt to destroy a non-fs reference");
	}

	void *iter;
	const FN_string *name = fullname->last(iter);

	FN_status status;
	FN_ctx *ctx = FN_ctx::from_ref(*ref, AUTHORITATIVE, status);

	switch (status.code()) {
	case FN_SUCCESS:
		break;
	case FN_E_NOT_A_CONTEXT:
	case FN_E_NO_SUPPORTED_ADDRESS:
	case FN_E_MALFORMED_REFERENCE:
		if (verbose >= 1) {
			info("  unbinding %s", str(fullname));
		}
		if (parent->unbind(*name, status) == 0) {
			error(&status, "Could not unbind %s", str(fullname));
		}
		return;
	default:
		error(&status, "Could not construct context handle for %s",
			str(fullname));
	}

	do {
		FN_bindinglist *bindings =
		    ctx->list_bindings(empty_string, status);
		if (bindings == NULL) {
			error(&status, "Could not list bindings of %s",
				str(fullname));
		}
		FN_string *child_name;
		FN_ref *child;
		while ((child_name = bindings->next(&child, status)) != NULL) {
			FN_composite_name child_fullname(*fullname);
			if (child_fullname.append_comp(*child_name) == 0) {
				error(NULL, "Memory allocation failure");
			}
			delete child_name;
			destroy_ref(&child_fullname, ctx, child);
			delete child;
		}
		delete bindings;
		switch (status.code()) {
		case FN_SUCCESS:
		case FN_E_INVALID_ENUM_HANDLE:	// try, try again
			break;
		default:
			error(&status, "Failure while listing bindings of %s",
				str(fullname));
		}
	} while (status.code() == FN_E_INVALID_ENUM_HANDLE);

	delete ctx;

	if (verbose >= 1) {
		info("  destroying %s", str(fullname));
	}
	if (parent->destroy_subcontext(*name, status) == 0) {
		error(&status, "Could not destroy %s", str(fullname));
	}
}


static FN_ref_addr *
addr_from_str(const char *addrdata)
{
	char buf[ADDRESS_SIZE];
	XDR xdr;
	xdrmem_create(&xdr, (caddr_t)buf, sizeof (buf), XDR_ENCODE);
	if (!xdr_string(&xdr, (char **)&addrdata, ~0)) {
		error(NULL, "XDR encode failed on: '%s'", addrdata);
	}
	FN_ref_addr *addr =
	    new FN_ref_addr(addrtype_mount, xdr_getpos(&xdr), buf);
	mem_check(addr);
	xdr_destroy(&xdr);
	return (addr);
}


static bool_t
addr_empty(const FN_ref_addr *addr)
{
	if (*addr->type() != addrtype_mount) {
		return (FALSE);
	}
	char buf[1];
	char *str = buf;
	XDR xdr;
	xdrmem_create(&xdr, (caddr_t)addr->data(), addr->length(), XDR_DECODE);
	bool_t string_empty = xdr_string(&xdr, &str, 1);
	xdr_destroy(&xdr);
	return (string_empty);
}
