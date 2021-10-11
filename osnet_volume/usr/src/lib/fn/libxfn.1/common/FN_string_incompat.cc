/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_string_incompat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"
#include "stdarg.h"

typedef FN_string_t *(*fn_string_from_contents_func)(
	unsigned long code_set,
	unsigned long lang_err,
	size_t charcount,
	size_t bytecount,
	const void *contents,
	unsigned int *status);

extern "C" FN_string_t *
fn_string_from_contents(unsigned long code_set,
	const void * /* locale_info */,
	size_t /* locale_info_len */,
	size_t charcount, size_t bytecount,
	const void *contents, unsigned int *status)
{
	static void *func = 0;

	fn_locked_dlsym("fn_string_from_contents", &func);

	/* forced to only support 0 (default) lang/terr */
	return (((fn_string_from_contents_func)func)(code_set, 0, charcount,
	    bytecount, contents, status));
}

typedef unsigned long (*fn_string_code_set_func)(
	const FN_string_t *str);

extern "C" unsigned long
fn_string_code_set(const FN_string_t *str, const void **locale_info,
	size_t *locale_info_len)
{
	static void *func = 0;
	unsigned long answer;

	fn_locked_dlsym("fn_string_code_set", &func);

	answer = ((fn_string_code_set_func)func)(str);
	*locale_info_len = 0;
	*locale_info = NULL;
	return (answer);
}

/* compatible interface, but manual intervention required for var args */

typedef FN_string_t *(*fn_string_from_strings_va_alloc_func)(
	size_t,
	unsigned int *status,
	const FN_string_t *s1,
	const FN_string_t *s2,
	va_list);

typedef size_t (*fn_string_from_strings_va_size_func)(
	const FN_string_t *s1,
	const FN_string_t *s2,
	va_list);

extern "C" FN_string_t *
fn_string_from_strings(unsigned int *status, const FN_string_t *s1,
	const FN_string_t *s2, ...)
{
	static void *size_func = 0;
	static void *alloc_func = 0;
	FN_string_t *answer;
	size_t charcount;

	fn_locked_dlsym("_fn_string_from_strings_va_size", &size_func);
	fn_locked_dlsym("_fn_string_from_strings_va_alloc", &alloc_func);

	va_list ap;
	va_start(ap, s2);
	charcount = ((fn_string_from_strings_va_size_func)size_func)(s1, s2,
	    ap);
	va_end(ap);

	va_start(ap, s2);
	answer = ((fn_string_from_strings_va_alloc_func)alloc_func)(charcount,
	    status, s1, s2, ap);
	va_end(ap);

	return (answer);
}
