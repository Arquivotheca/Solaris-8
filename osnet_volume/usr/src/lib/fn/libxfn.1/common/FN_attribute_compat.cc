/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)FN_attribute_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_attribute_t *(*fn_attribute_create_func)(
	const FN_identifier_t *attr_id,
	const FN_identifier_t *attr_syntax);

extern "C" FN_attribute_t *
fn_attribute_create(const FN_identifier_t *attr_id,
	const FN_identifier_t *attr_syntax)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_create", &func);

	return (((fn_attribute_create_func)func)(attr_id, attr_syntax));
}

typedef void (*fn_attribute_destroy_func)(FN_attribute_t *);

extern "C" void
fn_attribute_destroy(FN_attribute_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_destroy", &func);

	((fn_attribute_destroy_func)func)(arg0);
}

typedef FN_attribute_t *(*fn_attribute_copy_func)(const FN_attribute_t *);

extern "C" FN_attribute_t *
fn_attribute_copy(const FN_attribute_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_copy", &func);

	return (((fn_attribute_copy_func)func)(arg0));
}

typedef FN_attribute_t *(*fn_attribute_assign_func)(
	FN_attribute_t *dst,
	const FN_attribute_t *src);

extern "C" FN_attribute_t *
fn_attribute_assign(FN_attribute_t *dst, const FN_attribute_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_assign", &func);

	return (((fn_attribute_assign_func)func)(dst, src));
}

typedef const FN_identifier_t *(*fn_attribute_identifier_func)(
		const FN_attribute_t *);

extern "C" const FN_identifier_t *
fn_attribute_identifier(const FN_attribute_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_identifier", &func);

	return (((fn_attribute_identifier_func)func)(arg0));
}

typedef const FN_identifier_t *(*fn_attribute_syntax_func)(
	const FN_attribute_t *);

extern "C" const FN_identifier_t *
fn_attribute_syntax(const FN_attribute_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_syntax", &func);

	return (((fn_attribute_syntax_func)func)(arg0));
}

typedef unsigned int (*fn_attribute_valuecount_func)(const FN_attribute_t *);

extern "C" unsigned int
fn_attribute_valuecount(const FN_attribute_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_valuecount", &func);

	return (((fn_attribute_valuecount_func)func)(arg0));
}

typedef const FN_attrvalue_t *(*fn_attribute_first_func)(
	const FN_attribute_t *,
	void **iter_pos);

extern "C" const FN_attrvalue_t *
fn_attribute_first(const FN_attribute_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_first", &func);

	return (((fn_attribute_first_func)func)(arg0, iter_pos));
}

typedef const FN_attrvalue_t *(*fn_attribute_next_func)(
	const FN_attribute_t *,
	void **iter_pos);

extern "C" const FN_attrvalue_t *
fn_attribute_next(const FN_attribute_t *arg0, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_next", &func);

	return (((fn_attribute_next_func)func)(arg0, iter_pos));
}

typedef int (*fn_attribute_add_func)(
	FN_attribute_t *,
	const FN_attrvalue_t *,
	unsigned int exclusive);

extern "C" int
fn_attribute_add(FN_attribute_t *arg0,
	const FN_attrvalue_t *arg1, unsigned int exclusive)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_add", &func);

	return (((fn_attribute_add_func)func)(arg0, arg1, exclusive));
}

typedef int (*fn_attribute_remove_func)(
	FN_attribute_t *,
	const FN_attrvalue_t *);

extern "C" int
fn_attribute_remove(FN_attribute_t *arg0, const FN_attrvalue_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_attribute_remove", &func);

	return (((fn_attribute_remove_func)func)(arg0, arg1));
}
