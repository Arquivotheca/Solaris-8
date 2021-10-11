/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_compound_name_hook.cc	1.2	96/06/14 SMI"

#include <sys/param.h>		/* MAXPATHNAME */
#include <string.h>		/* str*() functions */

#include <xfn/FN_compound_name.hh>

#include "fns_symbol.hh"

typedef FN_compound_name_t *(*from_attrset_func)(const FN_attrset_t *,
    const FN_string_t *, FN_status_t *);

// function name prefix for from_ref_addr constructors

#define	FROM_SYNTAX_ATTRS_PREFIX	'S'

static inline void
get_constructor_func_name(char *fname, const char *addr_type, size_t len)
{
	fname[0] = FROM_SYNTAX_ATTRS_PREFIX;
	fns_legal_C_identifier(&fname[1], (const char *)addr_type, len);
}

// construct and initialize an FN_compound_name
FN_compound_name *
FN_compound_name::from_syntax_attrs(
	const FN_attrset &a,
	const FN_string &n,
	FN_status &s)
{
	void *fh;
	char mname[MAXPATHLEN], fname[MAXPATHLEN];
	const FN_attribute *syn_attr;
	const FN_attrvalue *tv;

	const FN_attrset_t *at = (const FN_attrset_t *)&a;
	const FN_string_t *nt = (const FN_string_t *)&n;
	FN_status_t *st = (FN_status_t *)&s;
	void * iter_pos;

	if ((syn_attr = a.get((const unsigned char *)"fn_syntax_type")) == 0 ||
	    (tv = syn_attr->first(iter_pos)) == 0) {
		s.set(FN_E_INVALID_SYNTAX_ATTRS, 0, 0, 0);
		return (0);
	}

	const char *syntax_type = (const char *)(tv->contents());

	// prime status for case of syntax not supported
	s.set(FN_E_SYNTAX_NOT_SUPPORTED, 0, 0, 0);

	if ((tv->length() + sizeof ("fn_compound_name_")) >= MAXPATHLEN)
		return (0);

	get_constructor_func_name(fname, syntax_type, tv->length());

	// look in executable (and linked libraries)
	// and then look in loadable module -- done by fns_link_symbol
	strcpy(mname, "fn_compound_name_");
	strncat(mname, syntax_type, tv->length());
	if (fh = fns_link_symbol(fname, mname)) {
		return ((FN_compound_name*)(*((from_attrset_func)fh))(at,
		    nt, st));
	}

	// give up
	return (0);
}
