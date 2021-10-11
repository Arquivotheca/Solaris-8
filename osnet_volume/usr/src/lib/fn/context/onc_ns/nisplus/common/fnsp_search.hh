/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_SEARCH_HH
#define	_FNSP_SEARCH_HH

#pragma ident	"@(#)fnsp_search.hh	1.3	96/08/29 SMI"

/* search-related operations */
#include <xfn/xfn.hh>
#include <xfn/FN_searchset.hh>
#include <FNSP_Address.hh>

extern FN_searchset *
FNSP_search_attr(const FNSP_Address&, const FN_identifier &,
	const FN_identifier &, const FN_attrvalue *, unsigned int &,
	const FN_string *table = 0);

extern FN_searchset *
FNSP_search_attrset(const FNSP_Address&, const FN_attrset *match_attrs,
	unsigned int return_ref, const FN_attrset *return_attr_ids,
	unsigned int &status);

extern FN_searchset*
FNSP_slist_names(const FNSP_Address& parent, unsigned & status);

extern FN_searchset *
FNSP_search_host_table(const FNSP_Address &, const FN_string &,
    const char *, unsigned int &);

extern unsigned
FNSP_search_add_refs(const FNSP_Address &, FN_searchset *,
    unsigned int return_ref = 1, unsigned int check_subctx = 0);

extern unsigned
FNSP_search_add_attrs(const FNSP_Address  &, FN_searchset *,
    const FN_attrset *attrs_ids);

#endif	/* _FNSP_SEARCH_HH */
