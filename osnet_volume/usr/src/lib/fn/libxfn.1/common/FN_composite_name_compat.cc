/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_composite_name_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_composite_name_t *(*fn_composite_name_create_func)(void);

extern "C" FN_composite_name_t *
fn_composite_name_create(void)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_create", &func);

	return (((fn_composite_name_create_func)func)());
}

typedef void (*fn_composite_name_destroy_func)(FN_composite_name_t *);

extern "C" void
fn_composite_name_destroy(FN_composite_name_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_destroy", &func);

	((fn_composite_name_destroy_func)func)(arg0);
}

typedef FN_composite_name_t *(*fn_composite_name_from_string_func)(
	const FN_string_t *);

extern "C" FN_composite_name_t *
fn_composite_name_from_string(const FN_string_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_from_string", &func);

	return (((fn_composite_name_from_string_func)func)(arg0));
}

typedef FN_string_t *(*fn_string_from_composite_name_func)(
	const FN_composite_name_t *,
	unsigned int *status);

extern "C" FN_string_t *
fn_string_from_composite_name(
	const FN_composite_name_t *arg0, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_from_composite_name", &func);

	return (((fn_string_from_composite_name_func)func)(arg0, status));
}

typedef FN_composite_name_t *(*fn_composite_name_copy_func)(
	const FN_composite_name_t *);

extern "C" FN_composite_name_t *
fn_composite_name_copy(const FN_composite_name_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_copy", &func);

	return (((fn_composite_name_copy_func)func)(arg0));
}

typedef FN_composite_name_t *(*fn_composite_name_assign_func)(
	FN_composite_name_t *dst,
	const FN_composite_name_t *src);

extern "C" FN_composite_name_t *
fn_composite_name_assign(
	FN_composite_name_t *dst,
	const FN_composite_name_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_assign", &func);

	return (((fn_composite_name_assign_func)func)(dst, src));
}

typedef int (*fn_composite_name_is_empty_func)(const FN_composite_name_t *);

extern "C" int
fn_composite_name_is_empty(const FN_composite_name_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_is_empty", &func);

	return (((fn_composite_name_is_empty_func)func)(arg0));
}

typedef unsigned int (*fn_composite_name_count_func)(
	const FN_composite_name_t *);

extern "C" unsigned int
fn_composite_name_count(const FN_composite_name_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_count", &func);

	return (((fn_composite_name_count_func)func)(arg0));
}

typedef const FN_string_t *(*fn_composite_name_first_func)(
	const FN_composite_name_t *,
	void **iter_pos);

extern "C" const FN_string_t *
fn_composite_name_first(
	const FN_composite_name_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_first", &func);

	return (((fn_composite_name_first_func)func)(arg0, iter_pos));
}

typedef const FN_string_t *(*fn_composite_name_next_func)(
	const FN_composite_name_t *,
	void **iter_pos);

extern "C" const FN_string_t *
fn_composite_name_next(
	const FN_composite_name_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_next", &func);

	return (((fn_composite_name_next_func)func)(arg0, iter_pos));
}

typedef const FN_string_t *(*fn_composite_name_prev_func)(
	const FN_composite_name_t *,
	void **iter_pos);

extern "C" const FN_string_t *
fn_composite_name_prev(
	const FN_composite_name_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_prev", &func);

	return (((fn_composite_name_prev_func)func)(arg0, iter_pos));
}

typedef const FN_string_t *(*fn_composite_name_last_func)(
	const FN_composite_name_t *,
	void **iter_pos);

extern "C" const FN_string_t *
fn_composite_name_last(
	const FN_composite_name_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_last", &func);

	return (((fn_composite_name_last_func)func)(arg0, iter_pos));
}

typedef FN_composite_name_t *(*fn_composite_name_prefix_func)(
	const FN_composite_name_t *,
	const void *iter_pos);

extern "C" FN_composite_name_t *
fn_composite_name_prefix(
	const FN_composite_name_t *arg0, const void *iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_prefix", &func);

	return (((fn_composite_name_prefix_func)func)(arg0, iter_pos));
}

typedef FN_composite_name_t *(*fn_composite_name_suffix_func)(
	const FN_composite_name_t *,
	const void *iter_pos);

extern "C" FN_composite_name_t *
fn_composite_name_suffix(
	const FN_composite_name_t *arg0, const void *iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_suffix", &func);

	return (((fn_composite_name_suffix_func)func)(arg0, iter_pos));
}

typedef int (*fn_composite_name_is_equal_func)(
	const FN_composite_name_t *n1,
	const FN_composite_name_t *n2,
	unsigned int *status);

extern "C" int
fn_composite_name_is_equal(const FN_composite_name_t *n1,
	const FN_composite_name_t *n2, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_is_equal", &func);

	return (((fn_composite_name_is_equal_func)func)(n1, n2, status));
}

typedef int (*fn_composite_name_is_prefix_func)(
	const FN_composite_name_t *,
	const FN_composite_name_t *prefix,
	void **iter_pos,
	unsigned int *status);

extern "C" int
fn_composite_name_is_prefix(
	const FN_composite_name_t *arg0,
	const FN_composite_name_t *prefix,
	void **iter_pos,
	unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_is_prefix", &func);

	return (((fn_composite_name_is_prefix_func)func)(arg0, prefix,
	    iter_pos, status));
}

typedef int (*fn_composite_name_is_suffix_func)(
	const FN_composite_name_t *,
	const FN_composite_name_t *suffix,
	void **iter_pos,
	unsigned int *status);

extern "C" int
fn_composite_name_is_suffix(
	const FN_composite_name_t *arg0,
	const FN_composite_name_t *suffix,
	void **iter_pos,
	unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_is_suffix", &func);

	return (((fn_composite_name_is_suffix_func)func)(arg0, suffix,
	    iter_pos, status));
}

typedef int (*fn_composite_name_prepend_comp_func)(
	FN_composite_name_t *,
	const FN_string_t *);

extern "C" int
fn_composite_name_prepend_comp(FN_composite_name_t *arg0,
	const FN_string_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_prepend_comp", &func);

	return (((fn_composite_name_prepend_comp_func)func)(arg0, arg1));
}

typedef int (*fn_composite_name_append_comp_func)(
	FN_composite_name_t *,
	const FN_string_t *);

extern "C" int
fn_composite_name_append_comp(
	FN_composite_name_t *arg0, const FN_string_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_append_comp", &func);

	return (((fn_composite_name_append_comp_func)func)(arg0, arg1));
}

typedef int (*fn_composite_name_insert_comp_func)(
	FN_composite_name_t *,
	void **iter_pos,
	const FN_string_t *);

extern "C" int
fn_composite_name_insert_comp(
	FN_composite_name_t *arg0,
	void **iter_pos,
	const FN_string_t *arg2)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_insert_comp", &func);

	return (((fn_composite_name_insert_comp_func)func)(arg0, iter_pos,
	    arg2));
}

typedef int (*fn_composite_name_delete_comp_func)(
	FN_composite_name_t *,
	void **iter_pos);

extern "C" int
fn_composite_name_delete_comp(FN_composite_name_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_delete_comp", &func);

	return (((fn_composite_name_delete_comp_func)func)(arg0, iter_pos));
}

typedef int (*fn_composite_name_prepend_name_func)(
	FN_composite_name_t *,
	const FN_composite_name_t *);

extern "C" int
fn_composite_name_prepend_name(
	FN_composite_name_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_prepend_name", &func);

	return (((fn_composite_name_prepend_name_func)func)(arg0, arg1));
}

typedef int (*fn_composite_name_append_name_func)(
	FN_composite_name_t *,
	const FN_composite_name_t *);

extern "C" int
fn_composite_name_append_name(
	FN_composite_name_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_append_name", &func);

	return (((fn_composite_name_append_name_func)func)(arg0, arg1));
}

typedef int (*fn_composite_name_insert_name_func)(
	FN_composite_name_t *,
	void **iter_pos,
	const FN_composite_name_t *);

extern "C" int
fn_composite_name_insert_name(
	FN_composite_name_t *arg0,
	void **iter_pos,
	const FN_composite_name_t *arg2)
{
	static void *func = 0;

	fn_locked_dlsym("fn_composite_name_insert_name", &func);

	return (((fn_composite_name_insert_name_func)func)(arg0, iter_pos,
	    arg2));
}
