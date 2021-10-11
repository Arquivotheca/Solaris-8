/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_ctx_csvc.cc	1.4	96/03/31 SMI"

#include <xfn/fn_spi.hh>

FN_ctx_csvc::FN_ctx_csvc()
{
}

FN_ctx_csvc::~FN_ctx_csvc()
{
}

static FN_string
stringify(const FN_composite_name &n)
{
	if (n.count() == 1) {
		void *iter_pos;
		FN_string answer(*n.first(iter_pos));
		return (answer);
	} else {
		unsigned int status;
		FN_string *nstr = n.string(&status);
		if (nstr) {
			FN_string answer(*nstr);
			delete nstr;
			return (answer);
		} else {
			// should deal with bad status
		}
	}
}

FN_ref*
FN_ctx_csvc::cn_lookup(const FN_composite_name &n,
    unsigned int lookup_flags, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_lookup(stringify(n), lookup_flags, cs));
}

FN_namelist*
FN_ctx_csvc::cn_list_names(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_list_names(stringify(n), cs));
}

FN_bindinglist*
FN_ctx_csvc::cn_list_bindings(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_list_bindings(stringify(n), cs));
}

int
FN_ctx_csvc::cn_bind(const FN_composite_name &n, const FN_ref &r,
    unsigned f, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_bind(stringify(n), r, f, cs));
}

int
FN_ctx_csvc::cn_unbind(const FN_composite_name &n, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_unbind(stringify(n), cs));
}

FN_ref*
FN_ctx_csvc::cn_create_subcontext(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_create_subcontext(stringify(n), cs));
}

int
FN_ctx_csvc::cn_destroy_subcontext(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_destroy_subcontext(stringify(n), cs));
}

int
FN_ctx_csvc::cn_rename(const FN_composite_name &oldname,
    const FN_composite_name &newname, unsigned f, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_rename(stringify(oldname), newname, f, cs));
}


FN_attrset*
FN_ctx_csvc::cn_get_syntax_attrs(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_get_syntax_attrs(stringify(n), cs));
}

FN_attribute*
FN_ctx_csvc::cn_attr_get(const FN_composite_name &n, const FN_identifier &i,
    unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_get(stringify(n), i, fl, cs));
}

int
FN_ctx_csvc::cn_attr_modify(const FN_composite_name &n, unsigned int i,
    const FN_attribute &a, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_modify(stringify(n), i, a, fl, cs));
}

FN_valuelist*
FN_ctx_csvc::cn_attr_get_values(const FN_composite_name &n,
    const FN_identifier &i, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_get_values(stringify(n), i, fl, cs));
}

FN_attrset*
FN_ctx_csvc::cn_attr_get_ids(const FN_composite_name &n,
    unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_get_ids(stringify(n), fl, cs));
}

FN_multigetlist*
FN_ctx_csvc::cn_attr_multi_get(const FN_composite_name &n,
    const FN_attrset *a, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_multi_get(stringify(n), a, fl, cs));
}

int
FN_ctx_csvc::cn_attr_multi_modify(const FN_composite_name &n,
    const FN_attrmodlist &a, unsigned int fl,
    FN_attrmodlist **l, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_multi_modify(stringify(n), a, fl, l, cs));
}

int
FN_ctx_csvc::cn_attr_bind(const FN_composite_name &n, const FN_ref &r,
    const FN_attrset *a, unsigned f, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_bind(stringify(n), r, a, f, cs));
}

FN_ref*
FN_ctx_csvc::cn_attr_create_subcontext(const FN_composite_name &n,
    const FN_attrset *a, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_create_subcontext(stringify(n), a, cs));
}

FN_searchlist *FN_ctx_csvc::cn_attr_search(
	    const FN_composite_name &n,
	    const FN_attrset *match_attrs,
	    unsigned int rref,
	    const FN_attrset *rattrs,
	    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_search(stringify(n), match_attrs, rref, rattrs, cs));
}

FN_ext_searchlist *FN_ctx_csvc::cn_attr_ext_search(
	    const FN_composite_name &n,
	    const FN_search_control *ctl,
	    const FN_search_filter *fil,
	    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_ext_search(stringify(n), ctl, fil, cs));
}



FN_ref*
FN_ctx_csvc::cn_lookup_nns(const FN_composite_name &n,
    unsigned int lookup_flags, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_lookup_nns(stringify(n), lookup_flags,  cs));
}

FN_namelist*
FN_ctx_csvc::cn_list_names_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_list_names_nns(stringify(n), cs));
}

FN_bindinglist*
FN_ctx_csvc::cn_list_bindings_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_list_bindings_nns(stringify(n), cs));
}

int
FN_ctx_csvc::cn_bind_nns(const FN_composite_name &n, const FN_ref &r,
    unsigned f, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_bind_nns(stringify(n), r, f, cs));
}

int
FN_ctx_csvc::cn_unbind_nns(const FN_composite_name &n, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_unbind_nns(stringify(n), cs));
}

FN_ref*
FN_ctx_csvc::cn_create_subcontext_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_create_subcontext_nns(stringify(n), cs));
}

int
FN_ctx_csvc::cn_destroy_subcontext_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_destroy_subcontext_nns(stringify(n), cs));
}

int
FN_ctx_csvc::cn_rename_nns(const FN_composite_name &oldn,
    const FN_composite_name &newn, unsigned f, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_rename_nns(stringify(oldn), newn, f, cs));
}



FN_attrset*
FN_ctx_csvc::cn_get_syntax_attrs_nns(const FN_composite_name &n,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_get_syntax_attrs_nns(stringify(n), cs));
}

FN_attribute*
FN_ctx_csvc::cn_attr_get_nns(const FN_composite_name &n,
    const FN_identifier &i, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_get_nns(stringify(n), i, fl, cs));
}

int
FN_ctx_csvc::cn_attr_modify_nns(const FN_composite_name &n,
    unsigned int i, const FN_attribute &a, unsigned int fl,
    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_modify_nns(stringify(n), i, a, fl, cs));
}

FN_valuelist*
FN_ctx_csvc::cn_attr_get_values_nns(const FN_composite_name &n,
    const FN_identifier &i, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_get_values_nns(stringify(n), i, fl, cs));
}

FN_attrset*
FN_ctx_csvc::cn_attr_get_ids_nns(const FN_composite_name &n,
    unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_get_ids_nns(stringify(n), fl, cs));
}

FN_multigetlist*
FN_ctx_csvc::cn_attr_multi_get_nns(const FN_composite_name &n,
    const FN_attrset *a, unsigned int fl, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_multi_get_nns(stringify(n), a, fl, cs));
}

int
FN_ctx_csvc::cn_attr_multi_modify_nns(const FN_composite_name &n,
    const FN_attrmodlist &a,
    unsigned int fl, FN_attrmodlist **l, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_multi_modify_nns(stringify(n), a, fl, l, cs));
}

int
FN_ctx_csvc::cn_attr_bind_nns(const FN_composite_name &n, const FN_ref &r,
    const FN_attrset *a, unsigned f, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_bind_nns(stringify(n), r, a, f, cs));
}

FN_ref*
FN_ctx_csvc::cn_attr_create_subcontext_nns(const FN_composite_name &n,
    const FN_attrset *a, FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_create_subcontext_nns(stringify(n), a, cs));
}

FN_searchlist *
FN_ctx_csvc::cn_attr_search_nns(
	    const FN_composite_name &n,
	    const FN_attrset *match_attrs,
	    unsigned int rref,
	    const FN_attrset *rattrs,
	    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_search_nns(stringify(n), match_attrs, rref, rattrs, cs));
}

FN_ext_searchlist *
FN_ctx_csvc::cn_attr_ext_search_nns(
	    const FN_composite_name &n,
	    const FN_search_control *ctl,
	    const FN_search_filter *fil,
	    FN_status_cnsvc &cns)
{
	FN_status_csvc cs(cns);
	return (c_attr_ext_search_nns(stringify(n), ctl, fil, cs));
}
