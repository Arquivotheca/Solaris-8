/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_ctx_incompat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_ctx_t *(*fn_ctx_handle_from_initial_func)(unsigned int auth,
	FN_status_t *status);

extern "C" FN_ctx_t *
fn_ctx_handle_from_initial(FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_handle_from_initial", &func);

	return (((fn_ctx_handle_from_initial_func)func)(0, status));
}

typedef void (*fn_namelist_destroy_func)(FN_namelist_t *nl);

extern "C" void
fn_namelist_destroy(FN_namelist_t *nl, FN_status_t * /* status */)
{
	static void *func = 0;

	fn_locked_dlsym("fn_namelist_destroy", &func);

	((fn_namelist_destroy_func)func)(nl);
}

typedef void (*fn_bindinglist_destroy_func)(FN_bindinglist_t *bl);

extern "C" void
fn_bindinglist_destroy(FN_bindinglist_t *bl, FN_status_t * /* status */)
{
	static void *func = 0;

	fn_locked_dlsym("fn_bindinglist_destroy", &func);

	((fn_bindinglist_destroy_func)func)(bl);
}

typedef FN_ctx_t *(*fn_ctx_handle_from_ref_func)(const FN_ref_t *ref,
	unsigned int auth, FN_status_t *status);

extern "C" FN_ctx_t *
fn_ctx_handle_from_ref(const FN_ref_t *ref, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ctx_handle_from_ref", &func);

	return (((fn_ctx_handle_from_ref_func)func)(ref, 0, status));
}

typedef FN_attribute_t *(*fn_attr_get_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_identifier_t *attr_id,
	unsigned int follow_link,
	FN_status_t *status);

extern "C" FN_attribute_t *
fn_attr_get(FN_ctx_t *ctx, const FN_composite_name_t *name,
	const FN_identifier_t *attr_id, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attr_get", &func);

	return (((fn_attr_get_func)func)(ctx, name, attr_id, 0, status));
}

typedef int (*fn_attr_modify_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	unsigned int mod_op,
	const FN_attribute_t *attr,
	unsigned int follow_link,
	FN_status_t *status);

extern "C" int
fn_attr_modify(FN_ctx_t *ctx, const FN_composite_name_t *name,
	unsigned int mod_op, const FN_attribute_t *attr, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attr_modify", &func);

	return (((fn_attr_modify_func)func)(ctx, name, mod_op, attr,
	    0, status));
}

typedef FN_valuelist_t *(*fn_attr_get_values_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_identifier_t *attr_id,
	unsigned int follow_link,
	FN_status_t *status);

extern "C" FN_valuelist_t *
fn_attr_get_values(FN_ctx_t *ctx, const FN_composite_name_t *name,
	const FN_identifier_t *attr_id, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attr_get_values", &func);

	return (((fn_attr_get_values_func)func)(ctx, name, attr_id, 0, status));
}

typedef void (*fn_valuelist_destroy_func)(FN_valuelist_t *vl);

extern "C" void
fn_valuelist_destroy(FN_valuelist_t *vl, FN_status_t * /* status */)
{
	static void *func = 0;

	fn_locked_dlsym("fn_valuelist_destroy", &func);

	((fn_valuelist_destroy_func)func)(vl);
}

typedef FN_attrset_t *(*fn_attr_get_ids_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	unsigned int follow_link,
	FN_status_t *status);

extern "C" FN_attrset_t *
fn_attr_get_ids(FN_ctx_t *ctx, const FN_composite_name_t *name,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attr_get_ids", &func);

	return (((fn_attr_get_ids_func)func)(ctx, name, 0, status));
}

typedef FN_multigetlist_t *(*fn_attr_multi_get_func)(
	FN_ctx_t *ctx,
	const FN_composite_name_t *name,
	const FN_attrset_t *attr_ids,
	unsigned int follow_link,
	FN_status_t *status);

extern "C" FN_multigetlist_t *
fn_attr_multi_get(FN_ctx_t *ctx, const FN_composite_name_t *name,
	const FN_attrset_t *attr_ids, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attr_multi_get", &func);

	return (((fn_attr_multi_get_func)func)(ctx, name, attr_ids, 0, status));
}


typedef void (*fn_multigetlist_destroy_func)(
		FN_multigetlist_t *ml);

extern "C" void
fn_multigetlist_destroy(FN_multigetlist_t *ml, FN_status_t * /* status */)
{
	static void *func = 0;

	fn_locked_dlsym("fn_multigetlist_destroy", &func);

	((fn_multigetlist_destroy_func)func)(ml);
}


typedef int (*fn_attr_multi_modify_func)(
		FN_ctx_t *ctx,
		const FN_composite_name_t *name,
		const FN_attrmodlist_t *mods,
		unsigned int follow_link,
		FN_attrmodlist_t **unexecuted_mods,
		FN_status_t *status);

extern "C" int
fn_attr_multi_modify(FN_ctx_t *ctx, const FN_composite_name_t *name,
	const FN_attrmodlist_t *mods, FN_attrmodlist_t **unexecuted_mods,
	FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attr_multi_modify", &func);

	return (((fn_attr_multi_modify_func)func)(ctx, name, mods, 0,
	    unexecuted_mods, status));
}
