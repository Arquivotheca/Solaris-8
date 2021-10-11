/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_CTX_H
#define	_XFN_FN_CTX_H

#pragma ident	"@(#)FN_ctx.h	1.4	96/03/31 SMI"

#include <xfn/FN_ref.h>
#include <xfn/FN_composite_name.h>
#include <xfn/FN_attrset.h>
#include <xfn/FN_attribute.h>
#include <xfn/FN_attrmodlist.h>
#include <xfn/FN_status.h>
#include <xfn/FN_search_control.h>
#include <xfn/FN_search_filter.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_ctx FN_ctx_t;
typedef struct _FN_namelist_t FN_namelist_t;
typedef struct _FN_bindinglist_t FN_bindinglist_t;
typedef struct _FN_valuelist_t FN_valuelist_t;
typedef struct _FN_multigetlist_t FN_multigetlist_t;
typedef struct _FN_searchlist_t FN_searchlist_t;
typedef struct _FN_ext_searchlist_t FN_ext_searchlist_t;

/*
 * This is the basic context interface.
 */

extern FN_ctx_t *fn_ctx_handle_from_initial(
		unsigned int authoritative,
		FN_status_t *status);
extern FN_ref_t *fn_ctx_lookup(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);

extern FN_namelist_t *fn_ctx_list_names(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);
extern FN_string_t *fn_namelist_next(
		FN_namelist_t *nl,
		FN_status_t *status);
extern void fn_namelist_destroy(
		FN_namelist_t *nl);

extern FN_bindinglist_t *fn_ctx_list_bindings(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);
extern FN_string_t *fn_bindinglist_next(
		FN_bindinglist_t *bl,
		FN_ref_t **ref,
		FN_status_t *status);
extern void fn_bindinglist_destroy(
		FN_bindinglist_t *bl);

extern int fn_ctx_bind(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		const FN_ref_t *ref,
		unsigned int exclusive,
		FN_status_t *status);
extern int fn_ctx_unbind(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);

extern FN_ref_t *fn_ctx_create_subcontext(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);
extern int fn_ctx_destroy_subcontext(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);

extern int fn_ctx_rename(
		FN_ctx_t *ctx,
		const FN_composite_name_t *oldname,
		const FN_composite_name_t *newname,
		unsigned int exclusive,
		FN_status_t *status);

extern FN_ref_t *fn_ctx_lookup_link(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);

extern FN_ctx_t *fn_ctx_handle_from_ref(const FN_ref_t *ref,
					unsigned int authoritative,
					FN_status_t *status);
extern FN_ref_t *fn_ctx_get_ref(const FN_ctx_t *ctx, FN_status_t *status);

extern void fn_ctx_handle_destroy(FN_ctx_t *ctx);

extern FN_composite_name_t *prelim_fn_ctx_equivalent_name(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_string_t *leading_name,
	FN_status_t *status);

/*
 * The basic attribute interface.
 */

/* Ops on single attr values. */

enum {
	FN_ATTR_OP_ADD = 1,
	FN_ATTR_OP_ADD_EXCLUSIVE,
	FN_ATTR_OP_REMOVE,
	FN_ATTR_OP_ADD_VALUES,
	FN_ATTR_OP_REMOVE_VALUES
};

extern FN_attribute_t *fn_attr_get(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		const FN_identifier_t *attr_id,
		unsigned int follow_link,
		FN_status_t *status);
extern int fn_attr_modify(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		unsigned int mod_op,
		const FN_attribute_t *attr,
		unsigned int follow_link,
		FN_status_t *status);

/* Ops on multiple attr values. */
extern FN_valuelist_t *fn_attr_get_values(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		const FN_identifier_t *attr_id,
		unsigned int follow_link,
		FN_status_t *status);
extern FN_attrvalue_t *fn_valuelist_next(
		FN_valuelist_t *vl,
		FN_identifier_t **attr_syntax,
		FN_status_t *status);
extern void fn_valuelist_destroy(
		FN_valuelist_t *vl);

extern FN_attrset_t *fn_attr_get_ids(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		unsigned int follow_link,
		FN_status_t *status);

extern FN_multigetlist_t *fn_attr_multi_get(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		const FN_attrset_t *attr_ids,
		unsigned int follow_link,
		FN_status_t *status);
extern FN_attribute_t *fn_multigetlist_next(
		FN_multigetlist_t *ml,
		FN_status_t *status);
extern void fn_multigetlist_destroy(
		FN_multigetlist_t *ml);

extern int fn_attr_multi_modify(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		const FN_attrmodlist_t *mods,
		unsigned int follow_link,
		FN_attrmodlist_t **unexecuted_mods,
		FN_status_t *status);
/*
 * Ops for syntax.
 */

extern FN_attrset_t *fn_ctx_get_syntax_attrs(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		FN_status_t *status);

/*
 * Extended Attribute Interface
 */

extern int fn_attr_bind(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_ref_t *ref,
	const FN_attrset_t *attrs,
	unsigned int exclusive,
	FN_status_t *status);

extern FN_ref_t *fn_attr_create_subcontext(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_attrset_t *attr,
	FN_status_t *status);

extern FN_searchlist_t *prelim_fn_attr_search(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_attrset_t *match_attrs,
	unsigned int return_ref,
	const FN_attrset_t *return_attr_id,
	FN_status_t *status);

extern FN_string_t *prelim_fn_searchlist_next(
	FN_searchlist_t *sl,
	FN_ref_t **returned_ref,
	FN_attrset_t **returned_attrs,
	FN_status_t *status);

extern void prelim_fn_searchlist_destroy(
	FN_searchlist_t *sl);

extern FN_ext_searchlist_t *prelim_fn_attr_ext_search(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_search_control_t *control,
	const FN_search_filter_t *filter,
	FN_status_t *status);

extern FN_composite_name_t *prelim_fn_ext_searchlist_next(
	FN_ext_searchlist_t *esl,
	FN_ref_t **returned_ref,
	FN_attrset_t **returned_attrs,
	unsigned int *relative,
	FN_status_t *status);

extern void prelim_fn_ext_searchlist_destroy(
	FN_ext_searchlist_t *esl);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_CTX_H */
