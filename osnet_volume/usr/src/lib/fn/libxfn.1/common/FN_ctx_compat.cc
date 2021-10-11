/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_ctx_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

/* %%%% Incompatibility Change Required for fn_ctx_handle_from_initial %%%% */

typedef FN_ref_t *(*fn_ctx_lookup_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" FN_ref_t *
fn_ctx_lookup(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_lookup", &func);

	return (((fn_ctx_lookup_func)func)(ctx, name, status));
}

typedef FN_namelist_t *(*fn_ctx_list_names_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" FN_namelist_t *
fn_ctx_list_names(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_list_names", &func);

	return (((fn_ctx_list_names_func)func)(ctx, name, status));
}

typedef FN_string_t *(*fn_namelist_next_func)(
	FN_namelist_t *nl,
	FN_status_t *status);

extern "C" FN_string_t *
fn_namelist_next(FN_namelist_t *nl, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_namelist_next", &func);

	return (((fn_namelist_next_func)func)(nl, status));
}

/* %%%% Incompatibility Change Required for fn_namelist_destroy %%%% */

typedef FN_bindinglist_t *(*fn_ctx_list_bindings_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" FN_bindinglist_t *
fn_ctx_list_bindings(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_list_bindings", &func);

	return (((fn_ctx_list_bindings_func)func)(ctx, name, status));
}

typedef FN_string_t *(*fn_bindinglist_next_func)(
	FN_bindinglist_t *bl,
	FN_ref_t **ref,
	FN_status_t *status);

extern "C" FN_string_t *
fn_bindinglist_next(FN_bindinglist_t *bl, FN_ref_t **ref,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_bindinglist_next", &func);

	return (((fn_bindinglist_next_func)func)(bl, ref, status));
}

/* %%%% Incompatibility Change Required for fn_bindinglist_destroy %%%% */

typedef int (*fn_ctx_bind_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_ref_t *ref,
	unsigned int exclusive,
	FN_status_t *status);

extern "C" int
fn_ctx_bind(FN_ctx_t *ctx, const FN_composite_name_t *name,
	const FN_ref_t *ref, unsigned int exclusive, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_bind", &func);

	return (((fn_ctx_bind_func)func)(ctx, name, ref, exclusive, status));
}

typedef int (*fn_ctx_unbind_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" int
fn_ctx_unbind(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_unbind", &func);

	return (((fn_ctx_unbind_func)func)(ctx, name, status));
}

typedef FN_ref_t *(*fn_ctx_create_subcontext_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" FN_ref_t *
fn_ctx_create_subcontext(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_create_subcontext", &func);

	return (((fn_ctx_create_subcontext_func)func)(ctx, name, status));
}

typedef int (*fn_ctx_destroy_subcontext_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" int
fn_ctx_destroy_subcontext(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_destroy_subcontext", &func);

	return (((fn_ctx_destroy_subcontext_func)func)(ctx, name, status));
}

typedef int (*fn_ctx_rename_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *oldname,
	const FN_composite_name_t *newname,
	unsigned int exclusive,
	FN_status_t *status);

extern "C" int
fn_ctx_rename(FN_ctx_t *ctx, const FN_composite_name_t *oldname,
	const FN_composite_name_t *newname,
	unsigned int exclusive,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_rename", &func);

	return (((fn_ctx_rename_func)func)(ctx, oldname, newname,
	    exclusive, status));
}

typedef FN_ref_t *(*fn_ctx_lookup_link_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" FN_ref_t *
fn_ctx_lookup_link(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_lookup_link", &func);

	return (((fn_ctx_lookup_link_func)func)(ctx, name, status));
}

/* %%%% Incompatibility Change Required for fn_ctx_handle_from_ref %%%% */

typedef FN_ref_t *(*fn_ctx_get_ref_func)(const FN_ctx_t *ctx,
	FN_status_t *status);

extern "C" FN_ref_t *
fn_ctx_get_ref(const FN_ctx_t *ctx, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_get_ref", &func);

	return (((fn_ctx_get_ref_func)func)(ctx, status));
}

typedef void (*fn_ctx_handle_destroy_func)(FN_ctx_t *ctx);

extern "C" void
fn_ctx_handle_destroy(FN_ctx_t *ctx)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_handle_destroy", &func);

	((fn_ctx_handle_destroy_func)func)(ctx);
}

/* %%%% Incompatibility Change Required for fn_attr_get %%%% */

/* %%%% Incompatibility Change Required for fn_attr_modify %%%% */

/* %%%% Incompatibility Change Required for fn_attr_get_values %%%% */

typedef FN_attrvalue_t *(*fn_valuelist_next_func)(
	FN_valuelist_t *vl,
	FN_identifier_t **attr_syntax,
	FN_status_t *status);

extern "C" FN_attrvalue_t *
fn_valuelist_next(FN_valuelist_t *vl, FN_identifier_t **attr_syntax,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_valuelist_next", &func);

	return (((fn_valuelist_next_func)func)(vl, attr_syntax, status));
}

/* %%%% Incompatibility Change Required for fn_valuelist_destroy %%%% */

/* %%%% Incompatibility Change Required for fn_attr_get_ids %%%% */

/* %%%% Incompatibility Change Required for fn_attr_multi_get %%%% */

typedef FN_attribute_t *(*fn_multigetlist_next_func)(
	FN_multigetlist_t *ml,
	FN_status_t *status);

extern "C" FN_attribute_t *
fn_multigetlist_next(FN_multigetlist_t *ml, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_multigetlist_next", &func);

	return (((fn_multigetlist_next_func)func)(ml, status));
}

/* %%%% Incompatibility Change Required for fn_multigetlist_destroy %%%% */

/* %%%% Incompatibility Change Required for fn_attr_multi_modify %%%% */

typedef FN_attrset_t *(*fn_ctx_get_syntax_attrs_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	FN_status_t *status);

extern "C" FN_attrset_t *
fn_ctx_get_syntax_attrs(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_get_syntax_attrs", &func);

	return (((fn_ctx_get_syntax_attrs_func)func)(ctx, name, status));
}
