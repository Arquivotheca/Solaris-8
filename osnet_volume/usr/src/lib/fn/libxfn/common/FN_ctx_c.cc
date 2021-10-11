/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx_c.cc	1.8	96/09/07 SMI"

#include <xfn/FN_ctx.hh>
#include <string.h>

extern "C"
void
fn_ctx_handle_destroy(FN_ctx_t *p)
{
	delete (FN_ctx *)p;
}

extern "C"
FN_ctx_t *
fn_ctx_handle_from_initial(unsigned int authoritative, FN_status_t *s)
{
	FN_ctx *answer;
	if (s)
		answer = FN_ctx::from_initial(
		    authoritative, *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = FN_ctx::from_initial(authoritative, temp_s);
	}

	return ((FN_ctx_t *)answer);
}

extern "C"
FN_ctx_t *
_fn_ctx_handle_from_initial_with_uid(uid_t uid,
	unsigned int authoritative, FN_status_t *s)
{
	FN_ctx *answer;
	if (s)
		answer = FN_ctx::from_initial_with_uid(uid,
		    authoritative, *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = FN_ctx::from_initial_with_uid(uid, authoritative,
		    temp_s);
	}

	return ((FN_ctx_t *)answer);
}

extern "C"
FN_ctx_t *
_fn_ctx_handle_from_initial_with_ns(int ns,
	unsigned int authoritative, FN_status_t *s)
{
	FN_ctx *answer;
	if (s)
		answer = FN_ctx::from_initial_with_ns(ns,
		    authoritative, *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = FN_ctx::from_initial_with_ns(ns,
		    authoritative, temp_s);
	}

	return ((FN_ctx_t *)answer);
}


extern "C"
FN_ctx_t *
fn_ctx_handle_from_ref(const FN_ref_t *ref,
    unsigned int authoritative, FN_status_t *s)
{
	FN_ctx *answer;

	if (s)
		answer = FN_ctx::from_ref(
		    *((const FN_ref *)ref), authoritative, *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = FN_ctx::from_ref(
		    *((const FN_ref *)ref), authoritative, temp_s);
	}
	return ((FN_ctx_t *)answer);
}

extern "C"
FN_ref_t *
fn_ctx_get_ref(const FN_ctx_t *p, FN_status_t *s)
{
	FN_ref *answer;

	if (s)
		answer = ((const FN_ctx *)p)->get_ref(*((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((const FN_ctx *)p)->get_ref(temp_s);
	}

	return ((FN_ref_t *)answer);
}

extern "C"
FN_ref_t *
fn_ctx_lookup(FN_ctx_t *p, const FN_composite_name_t *name,
    FN_status_t *s)
{
	FN_ref *answer;
	if (s)
		answer =
		    ((FN_ctx *)p)->lookup(*((const FN_composite_name *)name),
			*((FN_status *)s));
	else {
		FN_status temp_s;
		answer =
		    ((FN_ctx *)p)->lookup(*((const FN_composite_name *)name),
		    temp_s);
	}
	return ((FN_ref_t *)answer);
}

extern "C"
FN_namelist_t *
fn_ctx_list_names(
	FN_ctx_t *p,
	const FN_composite_name_t *name,
	FN_status_t *s)
{
	FN_namelist *answer;
	if (s)
		answer = ((FN_ctx *)p)->list_names(
		    *((const FN_composite_name *)name), *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)p)->list_names(
		    *((const FN_composite_name *)name), temp_s);
	}
	return ((FN_namelist_t *)answer);
}


extern "C"
FN_string_t *
fn_namelist_next(FN_namelist_t *nl, FN_status_t *s)
{
	FN_string *answer;
	if (s)
		answer = ((FN_namelist *)nl)->next(*((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_namelist *)nl)->next(temp_s);
	}
	return ((FN_string_t *)answer);
}

extern "C"
void
fn_namelist_destroy(FN_namelist_t *nl)
{
	delete (FN_namelist *)nl;
}

extern "C"
FN_bindinglist_t *
fn_ctx_list_bindings(
	FN_ctx_t *p,
	const FN_composite_name_t *name,
	FN_status_t *s)
{
	FN_bindinglist *answer;
	if (s)
		answer = ((FN_ctx *)p)->list_bindings(
		    *((const FN_composite_name *)name), *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)p)->list_bindings(
		    *((const FN_composite_name *)name), temp_s);
	}

	return ((FN_bindinglist_t *)answer);
}


extern "C"
FN_string_t *
fn_bindinglist_next(FN_bindinglist_t *nl, FN_ref_t **ref, FN_status_t *s)
{
	FN_ref* retref = 0;
	FN_string *answer;

	if (s)
		answer = (((FN_bindinglist *)nl)->next(
		    &retref, *((FN_status *)s)));
	else {
		FN_status temp_s;
		answer = (((FN_bindinglist *)nl)->next(&retref, temp_s));
	}
	if (ref) {
		*ref = (FN_ref_t *)retref;
	}
	return ((FN_string_t *)answer);
}

extern "C"
void
fn_bindinglist_destroy(FN_bindinglist_t *nl)
{
	delete (FN_bindinglist *)nl;
}


extern "C"
int
fn_ctx_bind(FN_ctx_t *ctx,
	    const FN_composite_name_t *name,
	    const FN_ref_t *ref,
	    unsigned bind_flags,
	    FN_status_t *s)
{
	if (s)
		return (((FN_ctx *)ctx)->bind(
		    *((const FN_composite_name *)name),
		    *((const FN_ref *)ref), bind_flags, *((FN_status *)s)));
	else {
		FN_status temp_s;
		return (((FN_ctx *)ctx)->bind(
		    *((const FN_composite_name *)name),
		    *((const FN_ref *)ref), bind_flags, temp_s));
	}
}

extern "C"
int
fn_ctx_unbind(FN_ctx_t *ctx, const FN_composite_name_t *name, FN_status_t *s)
{
	if (s)
		return (((FN_ctx *)ctx)->unbind(
		    *((const FN_composite_name *)name), *((FN_status *)s)));
	else {
		FN_status temp_s;
		return (((FN_ctx *)ctx)->unbind(
		    *((const FN_composite_name *)name), temp_s));
	}
}

extern "C"
int
fn_ctx_rename(
	FN_ctx_t *ctx,
	const FN_composite_name_t *oldname,
	const FN_composite_name_t *newname,
	unsigned int exclusive,
	FN_status_t *s)
{
	if (s)
		return (((FN_ctx *)ctx)->rename(
		    *((const FN_composite_name *)oldname),
		    *((const FN_composite_name *)newname), exclusive,
		    *((FN_status *)s)));
	else {
		FN_status temp_s;
		return (((FN_ctx *)ctx)->rename(
		    *((const FN_composite_name *)oldname),
		    *((const FN_composite_name *)newname), exclusive,
		    temp_s));
	}
}

extern "C"
FN_ref_t *
fn_ctx_lookup_link(FN_ctx_t *p, const FN_composite_name_t *name,
    FN_status_t *s)
{
	FN_ref *answer;
	if (s)
		answer = ((FN_ctx *)p)->lookup_link(
		    *((const FN_composite_name *)name), *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)p)->lookup_link(
		    *((const FN_composite_name *)name), temp_s);
	}
	return ((FN_ref_t *)answer);
}

extern "C"
FN_ref_t *
fn_ctx_create_subcontext(FN_ctx_t *ctx, const FN_composite_name_t *name,
    FN_status_t *s)
{
	FN_ref *answer;
	if (s)
		answer = ((FN_ctx *)ctx)->create_subcontext(
		    *((const FN_composite_name *)name), *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)ctx)->create_subcontext(
		    *((const FN_composite_name *)name), temp_s);
	}
	return ((FN_ref_t *)answer);
}

extern "C"
int
fn_ctx_destroy_subcontext(FN_ctx_t *p, const FN_composite_name_t *name,
    FN_status_t *s)
{
	if (s)
		return (((FN_ctx *)p)->destroy_subcontext(
		    *((const FN_composite_name *)name), *((FN_status *)s)));
	else {
		FN_status temp_s;
		return (((FN_ctx *)p)->destroy_subcontext(
		    *((const FN_composite_name *)name), temp_s));
	}
}

extern "C"
FN_composite_name_t *
prelim_fn_ctx_equivalent_name(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_string_t *leading_name,
	FN_status_t *s)
{
	FN_composite_name *answer;
	if (s)
		answer = ((FN_ctx *)ctx)->equivalent_name(
		    *((const FN_composite_name *)name),
		    *((const FN_string *)leading_name),
		    *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)ctx)->equivalent_name(
		    *((const FN_composite_name *)name),
		    *((const FN_string *)leading_name),
		    temp_s);
	}
	return ((FN_composite_name_t *)answer);
}


extern "C"
FN_attrset_t *
fn_ctx_get_syntax_attrs(FN_ctx_t *p, const FN_composite_name_t *name,
    FN_status_t *s)
{
	FN_attrset *answer;
	if (s)
		answer = ((FN_ctx *)p)->get_syntax_attrs(
		    *((const FN_composite_name *)name), *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)p)->get_syntax_attrs(
		    *((const FN_composite_name *)name), temp_s);
	}
	return ((FN_attrset_t *)answer);
}


/* Attribute: Functions to access and modify attributes */

extern "C"
FN_attribute_t *
fn_attr_get(FN_ctx_t *ctx, const FN_composite_name_t *name,
	    const FN_identifier_t *attr_id,
	    unsigned int follow_link,
	    FN_status_t *status)
{
	FN_attribute *answer;
	if (status)
		answer = ((FN_ctx *)ctx)->attr_get(
		    *((const FN_composite_name *)name),
		    *((const FN_identifier_t *)attr_id),
		    follow_link,
		    *((FN_status *)status));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)ctx)->attr_get(
		    *((const FN_composite_name *)name),
		    *((const FN_identifier_t *)attr_id),
		    follow_link,
		    temp_s);
	}
	return ((FN_attribute_t *)answer);
}

extern "C"
int fn_attr_modify(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	unsigned int mod_op,
	const FN_attribute_t *attr,
	unsigned int follow_link,
	FN_status_t *status)
{
	if (status)
		return (((FN_ctx *)ctx)->attr_modify(
		    *((const FN_composite_name *)name), mod_op,
		    *((const FN_attribute *)attr),
		    follow_link,
		    *((FN_status *)status)));
	else {
		FN_status temp_s;
		return (((FN_ctx *)ctx)->attr_modify(
		    *((const FN_composite_name *)name), mod_op,
		    *((const FN_attribute *)attr),
		    follow_link,
		    temp_s));
	}
}

extern "C"
FN_valuelist_t *
fn_attr_get_values(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_identifier_t *attr_id,
	unsigned int follow_link,
	FN_status_t *status)
{
	FN_valuelist *answer;
	if (status)
		answer = ((FN_ctx *)ctx)->attr_get_values(
		    *((const FN_composite_name *)name),
		    *((const FN_identifier *)attr_id),
		    follow_link,
		    *((FN_status *)status));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)ctx)->attr_get_values(
		    *((const FN_composite_name *)name),
		    *((const FN_identifier *)attr_id),
		    follow_link,
		    temp_s);
	}
	return ((FN_valuelist_t *)answer);
}

extern "C"
FN_attrvalue_t *
fn_valuelist_next(FN_valuelist_t *vl, FN_identifier_t **attr_syntax,
    FN_status_t *status)
{
	FN_identifier *id = 0;
	FN_attrvalue *answer;

	if (status)
		answer = ((FN_valuelist *)vl)->next(&id,
		    *((FN_status *)status));
	else {
		FN_status temp_s;
		answer = ((FN_valuelist *)vl)->next(&id, temp_s);
	}
	if (id) {
		if (*attr_syntax)
			*attr_syntax = (FN_identifier_t *)id;
	}
	return ((FN_attrvalue_t *)answer);
}

extern "C"
void
fn_valuelist_destroy(FN_valuelist_t *vl)
{
	delete ((FN_valuelist *)vl);
}


extern "C"
FN_attrset_t *
fn_attr_get_ids(FN_ctx_t *ctx, const FN_composite_name_t *name,
    unsigned int follow_link,
    FN_status_t *status)
{
	FN_attrset *answer;
	if (status)
		answer = ((FN_ctx *)ctx)->attr_get_ids(
		    *((const FN_composite_name *)name), follow_link,
		    *((FN_status *)status));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)ctx)->attr_get_ids(
		    *((const FN_composite_name *)name), follow_link,
		    temp_s);
	}
	return ((FN_attrset_t *)answer);
}

extern "C"
FN_multigetlist_t *
fn_attr_multi_get(FN_ctx_t *ctx, const FN_composite_name_t *name,
    const FN_attrset_t *attr_ids, unsigned int follow_link, FN_status_t *status)
{
	FN_multigetlist *answer;

	if (status)
		answer = (((FN_ctx *)ctx)->
		    attr_multi_get(*((const FN_composite_name *)name),
		    (const FN_attrset *)attr_ids, follow_link,
		    *((FN_status *)status)));
	else {
		FN_status temp_s;
		answer = (((FN_ctx *)ctx)->
		    attr_multi_get(*((const FN_composite_name *)name),
		    (const FN_attrset *)attr_ids, follow_link,
		    temp_s));
	}

	return ((FN_multigetlist_t *)answer);
}

extern "C"
FN_attribute_t *
fn_multigetlist_next(FN_multigetlist_t *ml, FN_status_t *s)
{
	FN_attribute *answer;

	if (s)
		answer = ((FN_multigetlist *)ml)->next(*((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_multigetlist *)ml)->next(temp_s);
	}
	return ((FN_attribute_t *)answer);
}

extern "C"
void
fn_multigetlist_destroy(FN_multigetlist_t *ml)
{
	delete ((FN_multigetlist *)ml);
}

extern "C"
int fn_attr_multi_modify(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_attrmodlist_t *mods,
	unsigned int follow_link,
	FN_attrmodlist_t **unexecuted_mods,
	FN_status_t *status)
{
	if (status)
		return (((FN_ctx *)ctx)->attr_multi_modify(
		    *((const FN_composite_name *)name),
		    *((const FN_attrmodlist *)mods),
		    follow_link,
		    ((FN_attrmodlist **)unexecuted_mods),
		    *((FN_status *)status)));
	else {
		FN_status temp_s;
		return (((FN_ctx *)ctx)->attr_multi_modify(
		    *((const FN_composite_name *)name),
		    *((const FN_attrmodlist *)mods),
		    follow_link,
		    ((FN_attrmodlist **)unexecuted_mods),
		    temp_s));
	}
}

/* Extended Attribute Interface */

extern "C"
int
fn_attr_bind(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_ref_t *ref,
	const FN_attrset_t *attrs,
	unsigned int exclusive,
	FN_status_t *status)
{
	if (status)
		return (((FN_ctx *)ctx)->attr_bind(
		    *((const FN_composite_name *)name),
		    *((const FN_ref *)ref),
		    (const FN_attrset *)attrs,
		    exclusive,
		    *((FN_status *)status)));
	else {
		FN_status temp_s;
		return (((FN_ctx *)ctx)->attr_bind(
		    *((const FN_composite_name *)name),
		    *((const FN_ref *)ref),
		    (const FN_attrset *)attrs,
		    exclusive, temp_s));
	}
}

extern "C"
FN_ref_t *
fn_attr_create_subcontext(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_attrset_t *attrs,
	FN_status_t *status)
{
	FN_ref *answer;
	if (status)
		answer = ((FN_ctx *)ctx)->
		    attr_create_subcontext(*((const FN_composite_name *)name),
		    (const FN_attrset *)attrs, *(FN_status *)status);
	else {
		FN_status temp_status;

		answer = ((FN_ctx *)ctx)->
		    attr_create_subcontext(*((const FN_composite_name *)name),
		    (const FN_attrset *)attrs, temp_status);
	}
	return ((FN_ref_t *)answer);
}

extern "C"
FN_searchlist_t *
prelim_fn_attr_search(FN_ctx_t *ctx, const FN_composite_name_t *name,
    const FN_attrset_t *match_attrs,
    unsigned int return_ref,
    const FN_attrset_t *return_attr_ids,
    FN_status_t *status)
{
	FN_searchlist *answer;

	if (status)
		answer = ((FN_ctx *)ctx)->
		    attr_search(*((const FN_composite_name *)name),
		    (const FN_attrset *)match_attrs,
		    return_ref, (const FN_attrset *)return_attr_ids,
		    *((FN_status *)status));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)ctx)->
		    attr_search(*((const FN_composite_name *)name),
		    (const FN_attrset *)match_attrs,
		    return_ref, (const FN_attrset *)return_attr_ids,
		    temp_s);
	}

	return ((FN_searchlist_t *)answer);
}

extern "C"
FN_string_t *
prelim_fn_searchlist_next(FN_searchlist_t *sl,
    FN_ref_t **rref,
    FN_attrset_t **rattrs,
    FN_status_t *s)
{
	FN_ref *retref = 0;
	FN_attrset *retattrs = 0;
	FN_string *answer;

	if (s)
		answer = ((FN_searchlist *)sl)->next(
		    &retref, &retattrs, *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_searchlist *)sl)->next(&retref, &retattrs,
		    temp_s);
	}
	if (rref)
		*rref = (FN_ref_t *)retref;
	else
		delete retref;

	if (rattrs)
		*rattrs = (FN_attrset_t *)retattrs;
	else
		delete retattrs;

	return ((FN_string_t *)answer);
}

extern "C"
void
prelim_fn_searchlist_destroy(FN_searchlist_t *sl)
{
	delete ((FN_searchlist *)sl);
}

extern "C"
FN_ext_searchlist_t *
prelim_fn_attr_ext_search(FN_ctx_t *ctx, const FN_composite_name_t *name,
    const FN_search_control_t *scontrol,
    const FN_search_filter_t *sfilter,
    FN_status_t *status)
{
	FN_ext_searchlist *answer;

	if (status)
		answer = ((FN_ctx *)ctx)->
		    attr_ext_search(*((const FN_composite_name *)name),
		    (const FN_search_control *)scontrol,
		    (const FN_search_filter *)sfilter,
		    *((FN_status *)status));
	else {
		FN_status temp_s;
		answer = ((FN_ctx *)ctx)->
		    attr_ext_search(*((const FN_composite_name *)name),
		    (const FN_search_control *)scontrol,
		    (const FN_search_filter *)sfilter,
		    temp_s);
	}

	return ((FN_ext_searchlist_t *)answer);
}

extern "C"
FN_composite_name_t *
prelim_fn_ext_searchlist_next(FN_ext_searchlist_t *esl,
    FN_ref_t **rref,
    FN_attrset_t **rattrs,
    unsigned int *relative,
    FN_status_t *s)
{
	FN_ref *retref = 0;
	FN_attrset *retattrs = 0;
	FN_composite_name *answer;
	unsigned int rel = 1;

	if (s)
		answer = ((FN_ext_searchlist *)esl)->next(
		    &retref, &retattrs, rel, *((FN_status *)s));
	else {
		FN_status temp_s;
		answer = ((FN_ext_searchlist *)esl)->next(
		    &retref, &retattrs, rel, temp_s);
	}
	if (rref)
		*rref = (FN_ref_t *)retref;
	if (rattrs)
		*rattrs = (FN_attrset_t *)retattrs;
	if (relative)
		*relative = rel;

	return ((FN_composite_name_t *)answer);
}

extern "C"
void
prelim_fn_ext_searchlist_destroy(FN_ext_searchlist_t *esl)
{
	delete ((FN_ext_searchlist *)esl);
}
