/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_ref_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_ref_t *(*fn_ref_create_func)(const FN_identifier_t *ref_type);

extern "C" FN_ref_t *
fn_ref_create(const FN_identifier_t *ref_type)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_create", &func);

	return (((fn_ref_create_func)func)(ref_type));
}

typedef void (*fn_ref_destroy_func)(FN_ref_t *ref);

extern "C" void
fn_ref_destroy(FN_ref_t *ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_destroy", &func);

	((fn_ref_destroy_func)func)(ref);
}

typedef FN_ref_t *(*fn_ref_copy_func)(const FN_ref_t *ref);

extern "C" FN_ref_t *
fn_ref_copy(const FN_ref_t *ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_copy", &func);

	return (((fn_ref_copy_func)func)(ref));
}

typedef FN_ref_t *(*fn_ref_assign_func)(FN_ref_t *dst, const FN_ref_t *src);

extern "C" FN_ref_t *
fn_ref_assign(FN_ref_t *dst, const FN_ref_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_assign", &func);

	return (((fn_ref_assign_func)func)(dst, src));
}

typedef const FN_identifier_t *(*fn_ref_type_func)(const FN_ref_t *ref);

extern "C" const FN_identifier_t *
fn_ref_type(const FN_ref_t *ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_type", &func);

	return (((fn_ref_type_func)func)(ref));
}

typedef unsigned int (*fn_ref_addrcount_func)(const FN_ref_t *ref);

extern "C" unsigned int
fn_ref_addrcount(const FN_ref_t *ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addrcount", &func);

	return (((fn_ref_addrcount_func)func)(ref));
}

typedef const FN_ref_addr_t *(*fn_ref_first_func)(
	const FN_ref_t *ref,
	void **iter_pos);

extern "C" const FN_ref_addr_t *
fn_ref_first(const FN_ref_t *ref, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_first", &func);

	return (((fn_ref_first_func)func)(ref, iter_pos));
}

typedef const FN_ref_addr_t *(*fn_ref_next_func)(
	const FN_ref_t *ref,
	void **iter_pos);

extern "C" const FN_ref_addr_t *
fn_ref_next(const FN_ref_t *ref, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_next", &func);

	return (((fn_ref_next_func)func)(ref, iter_pos));
}

typedef int (*fn_ref_prepend_addr_func)(FN_ref_t *ref,
	const FN_ref_addr_t *addr);

extern "C" int
fn_ref_prepend_addr(FN_ref_t *ref, const FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_prepend_addr", &func);

	return (((fn_ref_prepend_addr_func)func)(ref, addr));
}

typedef int (*fn_ref_append_addr_func)(FN_ref_t *ref,
	const FN_ref_addr_t *addr);

extern "C" int
fn_ref_append_addr(FN_ref_t *ref, const FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_append_addr", &func);

	return (((fn_ref_append_addr_func)func)(ref, addr));
}

typedef int (*fn_ref_insert_addr_func)(
	FN_ref_t *ref,
	void **iter_pos,
	const FN_ref_addr_t *addr);

extern "C" int
fn_ref_insert_addr(FN_ref_t *ref, void **iter_pos, const FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_insert_addr", &func);

	return (((fn_ref_insert_addr_func)func)(ref, iter_pos, addr));
}

typedef int (*fn_ref_delete_addr_func)(FN_ref_t *ref, void **iter_pos);

extern "C" int
fn_ref_delete_addr(FN_ref_t *ref, void **iter_pos)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_delete_addr", &func);

	return (((fn_ref_delete_addr_func)func)(ref, iter_pos));
}

typedef int (*fn_ref_delete_all_func)(FN_ref_t *ref);

extern "C" int
fn_ref_delete_all(FN_ref_t *ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_delete_all", &func);

	return (((fn_ref_delete_all_func)func)(ref));
}

typedef FN_ref_t *(*fn_ref_create_link_func)(
	const FN_composite_name_t *link_name);

extern "C" FN_ref_t *
fn_ref_create_link(const FN_composite_name_t *link_name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_create_link", &func);

	return (((fn_ref_create_link_func)func)(link_name));
}

typedef int (*fn_ref_is_link_func)(const FN_ref_t *ref);

extern "C" int
fn_ref_is_link(const FN_ref_t *ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_is_link", &func);

	return (((fn_ref_is_link_func)func)(ref));
}

typedef FN_composite_name_t *(*fn_ref_link_name_func)(const FN_ref_t *link_ref);

extern "C" FN_composite_name_t *
fn_ref_link_name(const FN_ref_t *link_ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_link_name", &func);

	return (((fn_ref_link_name_func)func)(link_ref));
}

typedef FN_string_t *(*fn_ref_description_func)(
		const FN_ref_t *ref,
		unsigned int detail,
		unsigned int *more_detail);

extern "C" FN_string_t *
fn_ref_description(const FN_ref_t *ref, unsigned int detail,
	unsigned int *more_detail)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_description", &func);

	return (((fn_ref_description_func)func)(ref, detail, more_detail));
}
