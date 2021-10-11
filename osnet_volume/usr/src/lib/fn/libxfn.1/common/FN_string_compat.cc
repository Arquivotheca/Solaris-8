/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_string_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_string_t *(*fn_string_create_func)(void);

extern "C" FN_string_t *
fn_string_create(void)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_create", &func);

	return (((fn_string_create_func)func)());
}

typedef void (*fn_string_destroy_func)(FN_string_t *);

extern "C" void
fn_string_destroy(FN_string_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_destroy", &func);

	((fn_string_destroy_func)func)(arg0);
}

typedef FN_string_t *(*fn_string_from_str_func)(const unsigned char *str);

extern "C" FN_string_t *
fn_string_from_str(const unsigned char *str)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_from_str", &func);

	return (((fn_string_from_str_func)func)(str));
}

typedef FN_string_t *(*fn_string_from_str_n_func)(const unsigned char *str,
	size_t storlen);

extern "C" FN_string_t *
fn_string_from_str_n(const unsigned char *str, size_t storlen)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_from_str_n", &func);

	return (((fn_string_from_str_n_func)func)(str, storlen));
}

typedef const unsigned char *(*fn_string_str_func)(const FN_string_t *str,
	unsigned int *status);

extern "C" const unsigned char *
fn_string_str(const FN_string_t *str, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_str", &func);

	return (((fn_string_str_func)func)(str, status));
}

/* %%%% Incompatibility Change Required for fn_string_from_contents %%%% */

/* %%%% Incompatibility Change Required for fn_string_code_set %%%% */

typedef size_t (*fn_string_charcount_func)(const FN_string_t *str);

extern "C" size_t
fn_string_charcount(const FN_string_t *str)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_charcount", &func);

	return (((fn_string_charcount_func)func)(str));
}

typedef size_t (*fn_string_bytecount_func)(const FN_string_t *str);

extern "C" size_t
fn_string_bytecount(const FN_string_t *str)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_bytecount", &func);

	return (((fn_string_bytecount_func)func)(str));
}

typedef const void *(*fn_string_contents_func)(const FN_string_t *str);

extern "C" const void *
fn_string_contents(const FN_string_t *str)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_contents", &func);

	return (((fn_string_contents_func)func)(str));
}

typedef FN_string_t *(*fn_string_copy_func)(const FN_string_t *str);

extern "C" FN_string_t *
fn_string_copy(const FN_string_t *str)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_copy", &func);

	return (((fn_string_copy_func)func)(str));
}

typedef FN_string_t *(*fn_string_assign_func)(
	FN_string_t *dst,
	const FN_string_t *src);

extern "C" FN_string_t *
fn_string_assign(FN_string_t *dst, const FN_string_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_assign", &func);

	return (((fn_string_assign_func)func)(dst, src));
}

/* %%%% Incompatibility Change Required for fn_string_from_strings %%%% */

typedef FN_string_t *(*fn_string_from_substring_func)(
	const FN_string_t *str,
	int first,
	int last);

extern "C" FN_string_t *
fn_string_from_substring(const FN_string_t *str, int first, int last)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_from_substring", &func);

	return (((fn_string_from_substring_func)func)(str, first, last));
}

typedef int (*fn_string_is_empty_func)(const FN_string_t *str);

extern "C" int
fn_string_is_empty(const FN_string_t *str)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_is_empty", &func);

	return (((fn_string_is_empty_func)func)(str));
}

typedef int (*fn_string_compare_func)(const FN_string_t *s1,
	const FN_string_t *s2,
	unsigned int string_case,
	unsigned int *status);

extern "C" int
fn_string_compare(const FN_string_t *s1, const FN_string_t *s2,
	unsigned int string_case,
	unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_compare", &func);

	return (((fn_string_compare_func)func)(s1, s2, string_case, status));
}

typedef int (*fn_string_compare_substring_func)(
	const FN_string_t *s1,
	int first,
	int last,
	const FN_string_t *s2,
	unsigned int string_case,
	unsigned int *status);

extern "C" int
fn_string_compare_substring(const FN_string_t *s1, int first, int last,
	const FN_string_t *s2,
	unsigned int string_case,
	unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_compare_substring", &func);

	return (((fn_string_compare_substring_func)func)(s1, first, last,
	    s2, string_case, status));
}

typedef int (*fn_string_next_substring_func)(
	const FN_string_t *str,
	const FN_string_t *sub,
	int index,
	unsigned int string_case,
	unsigned int *status);

extern "C" int
fn_string_next_substring(const FN_string_t *str, const FN_string_t *sub,
	int index, unsigned int string_case, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_next_substring", &func);

	return (((fn_string_next_substring_func)func)(str, sub, index,
	    string_case, status));
}

typedef int (*fn_string_prev_substring_func)(
	const FN_string_t *str,
	const FN_string_t *sub,
	int index,
	unsigned int string_case,
	unsigned int *status);

extern "C" int
fn_string_prev_substring(const FN_string_t *str,
	const FN_string_t *sub,
	int index,
	unsigned int string_case,
	unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_prev_substring", &func);

	return (((fn_string_prev_substring_func)func)(str, sub, index,
	    string_case, status));
}
