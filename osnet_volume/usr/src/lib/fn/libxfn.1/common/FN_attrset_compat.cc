/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_attrset_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_attrset_t *(*fn_attrset_create_func)(void);

extern "C" FN_attrset_t *
fn_attrset_create(void)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_create", &func);

	return (((fn_attrset_create_func)func)());
}

typedef void (*fn_attrset_destroy_func)(FN_attrset_t *);

extern "C" void
fn_attrset_destroy(FN_attrset_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_destroy", &func);

	((fn_attrset_destroy_func)func)(arg0);
}

typedef FN_attrset_t *(*fn_attrset_copy_func)(const FN_attrset_t *);

extern "C" FN_attrset_t *
fn_attrset_copy(const FN_attrset_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_copy", &func);

	return (((fn_attrset_copy_func)func)(arg0));
}

typedef FN_attrset_t *(*fn_attrset_assign_func)(
	FN_attrset_t *dst,
	const FN_attrset_t *src);

extern "C" FN_attrset_t *
fn_attrset_assign(FN_attrset_t *dst, const FN_attrset_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_assign", &func);

	return (((fn_attrset_assign_func)func)(dst, src));
}

typedef const FN_attribute_t *(*fn_attrset_get_func)(
	const FN_attrset_t *,
	const FN_identifier_t *attr);

extern "C" const FN_attribute_t *
fn_attrset_get(const FN_attrset_t *arg0, const FN_identifier_t *attr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_get", &func);

	return (((fn_attrset_get_func)func)(arg0, attr));
}

typedef unsigned int (*fn_attrset_count_func)(const FN_attrset_t *);

extern "C" unsigned int
fn_attrset_count(const FN_attrset_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_count", &func);

	return (((fn_attrset_count_func)func)(arg0));
}

typedef const FN_attribute_t *(*fn_attrset_first_func)(
	const FN_attrset_t *,
	void **iter_pos);

extern "C" const FN_attribute_t *
fn_attrset_first(const FN_attrset_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_first", &func);

	return (((fn_attrset_first_func)func)(arg0, iter_pos));
}

typedef const FN_attribute_t *(*fn_attrset_next_func)(
	const FN_attrset_t *,
	void **iter_pos);

extern "C" const FN_attribute_t *
fn_attrset_next(const FN_attrset_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_next", &func);

	return (((fn_attrset_next_func)func)(arg0, iter_pos));
}

typedef int (*fn_attrset_add_func)(
	FN_attrset_t *,
	const FN_attribute_t *attr,
	unsigned int exclusive);

extern "C" int
fn_attrset_add(
	FN_attrset_t *arg0,
	const FN_attribute_t *attr,
	unsigned int exclusive)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_add", &func);

	return (((fn_attrset_add_func)func)(arg0, attr, exclusive));
}

typedef int (*fn_attrset_remove_func)(FN_attrset_t *,
	const FN_identifier_t *attr);

extern "C" int
fn_attrset_remove(FN_attrset_t *arg0, const FN_identifier_t *attr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrset_remove", &func);

	return (((fn_attrset_remove_func)func)(arg0, attr));
}
