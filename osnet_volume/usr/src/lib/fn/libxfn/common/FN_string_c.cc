/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_string_c.cc	1.4	96/03/31 SMI"

#include <stdarg.h>

#include <xfn/FN_string.hh>
#include <xfn/FN_status.h>

/*
 * This is an implementation of the draft XFN FN_string data type.
 *
 * Note:
 *	All of the real work gets done in C++.
 *
 * Warning:
 *	This is incomplete.
 *	wchar_t and codeset unimplemented.
 */

extern "C"
FN_string_t *
fn_string_create(void)
{
	return ((FN_string_t *)new FN_string);
}

extern "C"
void
fn_string_destroy(FN_string_t *s)
{
	delete (FN_string *)s;
}

extern "C"
FN_string_t *
fn_string_from_str(const unsigned char *str)
{
	return ((FN_string_t *)new FN_string(str));
}

extern "C"
FN_string_t *
fn_string_from_str_n(const unsigned char *str, size_t len)
{
	return ((FN_string_t *)new FN_string(str, len));
}

extern "C"
FN_string_t *
fn_string_from_contents(unsigned long code_set,
			unsigned long lang_terr,
			size_t charcount,
			size_t bytecount,
			const void *contents,
			unsigned int *status)
{
	// optional status check made by FN_string()
	FN_string_t *answer = (FN_string_t *)new FN_string(code_set,
	    lang_terr, charcount, bytecount,
	    contents, status);

	return (answer);
}

extern "C"
unsigned long
fn_string_code_set(const FN_string_t *p)
{
	return (((const FN_string *)p)->code_set());
}

extern "C"
unsigned long
fn_string_lang_terr(const FN_string_t *p)
{
	return (((const FN_string *)p)->lang_terr());
}

extern "C"
size_t
fn_string_bytecount(const FN_string_t *s)
{
	return (((const FN_string *)s)->bytecount());
}

extern "C"
size_t
fn_string_charcount(const FN_string_t *s)
{
	return (((const FN_string *)s)->bytecount());
}

extern "C"
const void *
fn_string_contents(const FN_string_t *s)
{
	return (((const FN_string *)s)->contents());
}

extern "C"
const unsigned char *
fn_string_str(const FN_string_t *str, unsigned int *status)
{
	// optional status check made by str()
	const unsigned char *answer = (((const FN_string *)str)->str(status));
	return (answer);
}

extern "C"
FN_string_t *
fn_string_copy(const FN_string_t *s)
{
	return ((FN_string_t *)new FN_string(*(const FN_string *)s));
}

extern "C"
FN_string_t *
fn_string_assign(FN_string_t *dst, const FN_string_t *src)
{
	*(FN_string *)dst = *(const FN_string *)src;
	return (dst);
}

extern "C"
size_t
_fn_string_from_strings_va_size(
	const FN_string_t *s1,
	const FN_string_t *s2,
	va_list ap)
{
	return (FN_string::calculate_va_size((const FN_string *)s1,
	    (const FN_string *)s2, ap));
}

extern "C"
FN_string_t *
_fn_string_from_strings_va_alloc(
	size_t charcount,
	unsigned int *status,
	const FN_string_t *s1,
	const FN_string_t *s2,
	va_list ap)
{
	FN_string *ret;
	unsigned int s;

	ret = new FN_string(charcount, &s, (const FN_string *)s1,
	    (const FN_string *)s2, ap);

	if (status)
		*status = s;

	return ((FN_string_t *)ret);
}


extern "C"
FN_string_t *
fn_string_from_strings(
		unsigned int *status,
		const FN_string_t *s1,
		const FN_string_t *s2,
		...)
{
	FN_string_t *ret;
	va_list ap;
	size_t total_chars;

	va_start(ap, s2);
	total_chars = _fn_string_from_strings_va_size(s1, s2, ap);
	va_end(ap);

	va_start(ap, s2);
	ret =  _fn_string_from_strings_va_alloc(total_chars, status, s1,
	    s2, ap);
	va_end(ap);

	return (ret);
}

extern "C"
FN_string_t *
fn_string_from_substring(const FN_string_t *s, int first, int last)
{
	return ((FN_string_t *)new
		FN_string(*(const FN_string *)s, first, last));
}

extern "C"
int
fn_string_is_empty(const FN_string_t *s)
{
	return (((const FN_string *)s)->is_empty());
}


extern "C"
int
fn_string_compare(
	const FN_string_t *s1,
	const FN_string_t *s2,
	unsigned int string_case,
	unsigned int *status)
{
	// optional status check made by compare()
	int answer = ((const FN_string *)s1)->compare(*(const FN_string *)s2,
							string_case, status);
	return (answer);
}

extern "C"
int
fn_string_compare_substring(
	const FN_string_t *s1,
	int first,
	int last,
	const FN_string_t *s2,
	unsigned int string_case,
	unsigned int *status)
{
	// optional status check made by compare_substring()
	int answer = ((const FN_string *)s1)->compare_substring(first, last,
				*(const FN_string *)s2, string_case, status);

	return (answer);
}

extern "C"
int
fn_string_next_substring(
	const FN_string_t *str,
	const FN_string_t *sub,
	int index,
	unsigned int string_case,
	unsigned int *status)
{
	// optional status check made by next_substring()
	int answer = ((const FN_string *)str)->next_substring(
			*(const FN_string *)sub, index, string_case, status);
	return (answer);
}

extern "C"
int
fn_string_prev_substring(
	const FN_string_t *str,
	const FN_string_t *sub,
	int index,
	unsigned int string_case,
	unsigned int *status)
{
	// optional status check made by prev_substring()
	int answer = ((const FN_string *)str)->prev_substring(
			*(const FN_string *)sub, index, string_case, status);
	return (answer);
}
