/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx_hook.cc	1.4	96/06/27 SMI"

#include <xfn/xfn.hh>
#include <string.h>
#include "fns_symbol.hh"

typedef FN_ctx_t *(*from_initial_func)(unsigned int, FN_status_t *);

// get initial context
FN_ctx *
FN_ctx::from_initial(unsigned int auth, FN_status &s)
{
	void *fh;
	FN_ctx *answer = 0;

	FN_status_t *stat = (FN_status_t *)(&s);
	FN_ctx_t *c_ctx = 0;

	// look in executable (and linked libraries)
	// and then look in loadable module -- done by fns_link_symbol
	if (fh = fns_link_symbol("initial__fn_ctx_handle_from_initial",
	    "fn_ctx_initial"))
		c_ctx = ((*((from_initial_func)fh))(auth, stat));

	// configuration error
	if (c_ctx == (FN_ctx_t *)0)
	    s.set(FN_E_CONFIGURATION_ERROR, 0, 0, 0);
	else {
		answer = (FN_ctx*)c_ctx;
	}

	return (answer);
}

typedef FN_ctx_t *(*from_initial_uid_func)(uid_t, unsigned int, FN_status_t *);

// get initial context
FN_ctx *
FN_ctx::from_initial_with_uid(uid_t uid, unsigned int auth, FN_status &s)
{
	void *fh;
	FN_ctx *answer = 0;

	FN_status_t *stat = (FN_status_t *)(&s);
	FN_ctx_t *c_ctx = 0;

	// look in executable (and linked libraries)
	// and then look in loadable module -- done by fns_link_symbol
	if (fh = fns_link_symbol(
	    "initial__fn_ctx_handle_from_initial_with_uid",
	    "fn_ctx_initial"))
		c_ctx = ((*((from_initial_uid_func)fh))(uid, auth, stat));

	// configuration error
	if (c_ctx == (FN_ctx_t *)0)
	    s.set(FN_E_CONFIGURATION_ERROR, 0, 0, 0);
	else {
		answer = (FN_ctx*)c_ctx;
	}
	return (answer);
}

typedef FN_ctx_t *(*from_initial_ns_func)(int, unsigned int, FN_status_t *);

// get initial context
FN_ctx *
FN_ctx::from_initial_with_ns(int ns,
    unsigned int auth, FN_status &s)
{
	void *fh;
	FN_ctx *answer = 0;

	FN_status_t *stat = (FN_status_t *)(&s);
	FN_ctx_t *c_ctx = 0;

	// look in executable (and linked libraries)
	// and then look in loadable module -- done by fns_link_symbol
	if (fh = fns_link_symbol(
	    "initial__fn_ctx_handle_from_initial_with_ns",
	    "fn_ctx_initial"))
		c_ctx = ((*((from_initial_ns_func)fh))(ns, auth, stat));

	// configuration error
	if (c_ctx == (FN_ctx_t *)0)
	    s.set(FN_E_CONFIGURATION_ERROR, 0, 0, 0);
	else {
		answer = (FN_ctx*)c_ctx;
	}
	return (answer);
}

#include <stdio.h>
#include <sys/param.h>

typedef FN_ctx_t *(*from_address_func)(const FN_ref_addr_t *,
    const FN_ref_t *, unsigned int, FN_status_t *);

typedef FN_ctx_t *(*from_ref_func)(const FN_ref_t *,
    unsigned int, FN_status_t *);

// function name prefix for from_ref_addr constructors

#define	FROM_REF_ADDR_PREFIX	'A'

static inline void
get_constructor_func_name(char *fname, const unsigned char *addr_type,
    size_t len)
{
	fname[0] = FROM_REF_ADDR_PREFIX;
	fns_legal_C_identifier(&fname[1], (const char *)addr_type, len);
}

// construct a context from a reference
FN_ctx *
FN_ctx::from_ref(const FN_ref &r, unsigned int auth, FN_status &s)
{
	FN_ctx_t *cp = 0;
	const FN_ref_addr *ap;
	const unsigned char *addr_type_str;
	void *ip, *fh;
	char mname[MAXPATHLEN], fname[MAXPATHLEN];
	const FN_identifier *addr_type;
	FN_ctx *answer = 0;

	// prime status for case of no supported addresses
	s.set(FN_E_NO_SUPPORTED_ADDRESS, 0, 0, 0);

	// look for supported addresses (and try them)
	for (ap = r.first(ip); ap; ap = r.next(ip)) {
		addr_type = ap->type();
		addr_type_str = (addr_type ? addr_type->str() : NULL);
		// If cannot get string form of address, or if addr too long
		if (addr_type_str == 0 ||
		    (addr_type->length() + sizeof ("fn_ctx_")) >= MAXPATHLEN)
			continue;

		get_constructor_func_name(fname,
		    addr_type_str, addr_type->length());

		// look in executable (and linked libraries)
		// and then look in loadable module -- done by fns_link_symbol
		strcpy(mname, "fn_ctx_");
		strcat(mname, (char *) addr_type_str);
		if (fh = fns_link_symbol(fname, mname)) {
			if (cp = (*((from_address_func)fh))(
			    (const FN_ref_addr_t *)ap,
			    (const FN_ref_t *)&r,
			    auth,
			    (FN_status_t *)&s)) {
				answer = (FN_ctx *)cp;
				return (answer);
			}
			continue;
		}
	}

	return (0);
}
