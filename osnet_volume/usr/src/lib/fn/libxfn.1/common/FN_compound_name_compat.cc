/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_compound_name_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_compound_name_t *(*fn_compound_name_from_syntax_attrs_func)(
	const FN_attrset_t *aset,
	const FN_string_t *name,
	FN_status_t *status);

extern "C" FN_compound_name_t *
fn_compound_name_from_syntax_attrs(const FN_attrset_t *aset,
	const FN_string_t *name, FN_status_t *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_from_syntax_attrs", &func);

	return (((fn_compound_name_from_syntax_attrs_func)func)(aset,
	    name, status));
}

typedef FN_attrset_t *(*fn_compound_name_get_syntax_attrs_func)(
	const FN_compound_name_t *name);

extern "C" FN_attrset_t *
fn_compound_name_get_syntax_attrs(const FN_compound_name_t *name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_get_syntax_attrs", &func);

	return (((fn_compound_name_get_syntax_attrs_func)func)(name));
}

typedef void (*fn_compound_name_destroy_func)(FN_compound_name_t *name);

extern "C" void
fn_compound_name_destroy(FN_compound_name_t *name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_destroy", &func);

	((fn_compound_name_destroy_func)func)(name);
}

typedef FN_string_t *(*fn_string_from_compound_name_func)(
	const FN_compound_name_t *name);

extern "C" FN_string_t *
fn_string_from_compound_name(const FN_compound_name_t *name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_from_compound_name", &func);

	return (((fn_string_from_compound_name_func)func)(name));
}

typedef FN_compound_name_t *(*fn_compound_name_copy_func)(
	const FN_compound_name_t *name);

extern "C" FN_compound_name_t *
fn_compound_name_copy(const FN_compound_name_t *name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_copy", &func);

	return (((fn_compound_name_copy_func)func)(name));
}

typedef FN_compound_name_t *(*fn_compound_name_assign_func)(
	FN_compound_name_t *dst,
	const FN_compound_name_t *src);

extern "C" FN_compound_name_t *
fn_compound_name_assign(FN_compound_name_t *dst, const FN_compound_name_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_assign", &func);

	return (((fn_compound_name_assign_func)func)(dst, src));
}

typedef unsigned int (*fn_compound_name_count_func)(
	const FN_compound_name_t *name);

extern "C" unsigned int
fn_compound_name_count(const FN_compound_name_t *name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_count", &func);

	return (((fn_compound_name_count_func)func)(name));
}

typedef const FN_string_t *(*fn_compound_name_first_func)(
	const FN_compound_name_t *name,
	void **iter_pos);

extern "C" const FN_string_t *
fn_compound_name_first(const FN_compound_name_t *name, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_first", &func);

	return (((fn_compound_name_first_func)func)(name, iter_pos));
}

typedef const FN_string_t *(*fn_compound_name_next_func)(
	const FN_compound_name_t *name,
	void **iter_pos);

extern "C" const FN_string_t *
fn_compound_name_next(const FN_compound_name_t *name, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_next", &func);

	return (((fn_compound_name_next_func)func)(name, iter_pos));
}

typedef const FN_string_t *(*fn_compound_name_prev_func)(
	const FN_compound_name_t *name,
	void **iter_pos);

extern "C" const FN_string_t *
fn_compound_name_prev(const FN_compound_name_t *name, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_prev", &func);

	return (((fn_compound_name_prev_func)func)(name, iter_pos));
}

typedef const FN_string_t *(*fn_compound_name_last_func)(
	const FN_compound_name_t *name,
	void **iter_pos);

extern "C" const FN_string_t *
fn_compound_name_last(const FN_compound_name_t *name, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_last", &func);

	return (((fn_compound_name_last_func)func)(name, iter_pos));
}

typedef FN_compound_name_t *(*fn_compound_name_prefix_func)(
	const FN_compound_name_t *name,
	const void *iter_pos);

extern "C" FN_compound_name_t *
fn_compound_name_prefix(const FN_compound_name_t *name, const void *iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_prefix", &func);

	return (((fn_compound_name_prefix_func)func)(name, iter_pos));
}

typedef FN_compound_name_t *(*fn_compound_name_suffix_func)(
	const FN_compound_name_t *name,
	const void *iter_pos);

extern "C" FN_compound_name_t *
fn_compound_name_suffix(const FN_compound_name_t *name, const void *iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_suffix", &func);

	return (((fn_compound_name_suffix_func)func)(name, iter_pos));
}

typedef int (*fn_compound_name_is_empty_func)(const FN_compound_name_t *name);

extern "C" int
fn_compound_name_is_empty(const FN_compound_name_t *name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_is_empty", &func);

	return (((fn_compound_name_is_empty_func)func)(name));
}

typedef int (*fn_compound_name_is_equal_func)(
	const FN_compound_name_t *name,
	const FN_compound_name_t *,
	unsigned int *status);

extern "C" int
fn_compound_name_is_equal(const FN_compound_name_t *name,
	const FN_compound_name_t *arg1, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_is_equal", &func);

	return (((fn_compound_name_is_equal_func)func)(name, arg1, status));
}

typedef int (*fn_compound_name_is_prefix_func)(
	const FN_compound_name_t *name,
	const FN_compound_name_t *prefix,
	void **iter_pos,
	unsigned int *status);

extern "C" int
fn_compound_name_is_prefix(const FN_compound_name_t *name,
	const FN_compound_name_t *prefix,
	void **iter_pos,
	unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_is_prefix", &func);

	return (((fn_compound_name_is_prefix_func)func)(name, prefix,
	    iter_pos, status));
}

typedef int (*fn_compound_name_is_suffix_func)(
	const FN_compound_name_t *name,
	const FN_compound_name_t *suffix,
	void **iter_pos,
	unsigned int *status);

extern "C" int
fn_compound_name_is_suffix(const FN_compound_name_t *name,
	const FN_compound_name_t *suffix,
	void **iter_pos,
	unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_is_suffix", &func);

	return (((fn_compound_name_is_suffix_func)func)(name, suffix,
	    iter_pos, status));
}

typedef int (*fn_compound_name_prepend_comp_func)(
	FN_compound_name_t *name,
	const FN_string_t *atomic_comp,
	unsigned int *status);

extern "C" int
fn_compound_name_prepend_comp(FN_compound_name_t *name,
	const FN_string_t *atomic_comp, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_prepend_comp", &func);

	return (((fn_compound_name_prepend_comp_func)func)(name,
	    atomic_comp, status));
}

typedef int (*fn_compound_name_append_comp_func)(
	FN_compound_name_t *name,
	const FN_string_t *atomic_comp,
	unsigned int *status);

extern "C" int
fn_compound_name_append_comp(FN_compound_name_t *name,
	const FN_string_t *atomic_comp, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_append_comp", &func);

	return (((fn_compound_name_append_comp_func)func)(name,
	    atomic_comp, status));
}

typedef int (*fn_compound_name_insert_comp_func)(
	FN_compound_name_t *name,
	void **iter_pos,
	const FN_string_t *atomic_comp,
	unsigned int *status);

extern "C" int
fn_compound_name_insert_comp(FN_compound_name_t *name, void **iter_pos,
	const FN_string_t *atomic_comp, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_insert_comp", &func);

	return (((fn_compound_name_insert_comp_func)func)(name, iter_pos,
	    atomic_comp, status));
}

typedef int (*fn_compound_name_delete_comp_func)(
	FN_compound_name_t *name,
	void **iter_pos);

extern "C" int
fn_compound_name_delete_comp(FN_compound_name_t *name, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_delete_comp", &func);

	return (((fn_compound_name_delete_comp_func)func)(name, iter_pos));
}

typedef int (*fn_compound_name_delete_all_func)(FN_compound_name_t *name);

extern "C" int
fn_compound_name_delete_all(FN_compound_name_t *name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_compound_name_delete_all", &func);

	return (((fn_compound_name_delete_all_func)func)(name));
}
