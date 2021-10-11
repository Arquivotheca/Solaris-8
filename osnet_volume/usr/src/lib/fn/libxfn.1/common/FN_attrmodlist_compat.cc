/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_attrmodlist_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_attrmodlist_t *(*fn_attrmodlist_create_func)(void);

extern "C" FN_attrmodlist_t *
fn_attrmodlist_create(void)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_create", &func);

	return (((fn_attrmodlist_create_func)func)());
}

typedef void (*fn_attrmodlist_destroy_func)(FN_attrmodlist_t *);

extern "C" void
fn_attrmodlist_destroy(FN_attrmodlist_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_destroy", &func);

	((fn_attrmodlist_destroy_func)func)(arg0);
}

typedef FN_attrmodlist_t *(*fn_attrmodlist_copy_func)(const FN_attrmodlist_t *);

extern "C" FN_attrmodlist_t *
fn_attrmodlist_copy(const FN_attrmodlist_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_copy", &func);

	return (((fn_attrmodlist_copy_func)func)(arg0));
}

typedef FN_attrmodlist_t *(*fn_attrmodlist_assign_func)(
	FN_attrmodlist_t *dst,
	const FN_attrmodlist_t *src);

extern "C" FN_attrmodlist_t *
fn_attrmodlist_assign(FN_attrmodlist_t *dst, const FN_attrmodlist_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_assign", &func);

	return (((fn_attrmodlist_assign_func)func)(dst, src));
}

typedef unsigned int (*fn_attrmodlist_count_func)(const FN_attrmodlist_t *);

extern "C" unsigned int
fn_attrmodlist_count(const FN_attrmodlist_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_count", &func);

	return (((fn_attrmodlist_count_func)func)(arg0));
}

typedef const FN_attribute_t *(*fn_attrmodlist_first_func)(
	const FN_attrmodlist_t *,
	void **iter_pos,
	unsigned int *first_mod_op);

extern "C" const FN_attribute_t *
fn_attrmodlist_first(
	const FN_attrmodlist_t *arg0,
	void **iter_pos,
	unsigned int *first_mod_op)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_first", &func);

	return (((fn_attrmodlist_first_func)func)(arg0, iter_pos,
	    first_mod_op));
}

typedef const FN_attribute_t *(*fn_attrmodlist_next_func)(
	const FN_attrmodlist_t *,
	void **iter_pos,
	unsigned int *mod_op);

extern "C" const FN_attribute_t *
fn_attrmodlist_next(
	const FN_attrmodlist_t *arg0,
	void **iter_pos,
	unsigned int *mod_op)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_next", &func);

	return (((fn_attrmodlist_next_func)func)(arg0, iter_pos, mod_op));
}

typedef int (*fn_attrmodlist_add_func)(
	FN_attrmodlist_t *,
	unsigned int mod_op,
	const FN_attribute_t *mod_args);

extern "C" int
fn_attrmodlist_add(
	FN_attrmodlist_t *arg0,
	unsigned int mod_op,
	const FN_attribute_t *mod_args)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attrmodlist_add", &func);

	return (((fn_attrmodlist_add_func)func)(arg0, mod_op, mod_args));
}
