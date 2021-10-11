/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_ref_addr_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_ref_addr_t *(*fn_ref_addr_create_func)(
	const FN_identifier_t *type,
	size_t len,
	const void *data);

extern "C" FN_ref_addr_t *
fn_ref_addr_create(const FN_identifier_t *type, size_t len, const void *data)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_create", &func);

	return (((fn_ref_addr_create_func)func)(type, len, data));
}

typedef void (*fn_ref_addr_destroy_func)(FN_ref_addr_t *addr);

extern "C" void
fn_ref_addr_destroy(FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_destroy", &func);

	((fn_ref_addr_destroy_func)func)(addr);
}

typedef FN_ref_addr_t *(*fn_ref_addr_copy_func)(const FN_ref_addr_t *addr);

extern "C" FN_ref_addr_t *
fn_ref_addr_copy(const FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_copy", &func);

	return (((fn_ref_addr_copy_func)func)(addr));
}

typedef FN_ref_addr_t *(*fn_ref_addr_assign_func)(
	FN_ref_addr_t *dst,
	const FN_ref_addr_t *src);

extern "C" FN_ref_addr_t *
fn_ref_addr_assign(FN_ref_addr_t *dst, const FN_ref_addr_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_assign", &func);

	return (((fn_ref_addr_assign_func)func)(dst, src));
}

typedef const FN_identifier_t *(*fn_ref_addr_type_func)(
	const FN_ref_addr_t *addr);

extern "C" const FN_identifier_t *
fn_ref_addr_type(const FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_type", &func);

	return (((fn_ref_addr_type_func)func)(addr));
}

typedef size_t (*fn_ref_addr_length_func)(const FN_ref_addr_t *addr);

extern "C" size_t
fn_ref_addr_length(const FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_length", &func);

	return (((fn_ref_addr_length_func)func)(addr));
}

typedef const void *(*fn_ref_addr_data_func)(const FN_ref_addr_t *addr);

extern "C" const void *
fn_ref_addr_data(const FN_ref_addr_t *addr)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_data", &func);

	return (((fn_ref_addr_data_func)func)(addr));
}

typedef FN_string_t *(*fn_ref_addr_description_func)(const FN_ref_addr_t *addr,
	unsigned int detail,
	unsigned int *more_detail);

extern "C" FN_string_t *
fn_ref_addr_description(const FN_ref_addr_t *addr, unsigned int detail,
	unsigned int *more_detail)
{
	static void *func = 0;

	fn_locked_dlsym("fn_ref_addr_description", &func);

	return (((fn_ref_addr_description_func)func)(addr, detail,
	    more_detail));
}
