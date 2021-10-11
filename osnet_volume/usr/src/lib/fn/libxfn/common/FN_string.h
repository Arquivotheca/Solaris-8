/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_STRING_H
#define	_XFN_FN_STRING_H

#pragma ident	"@(#)FN_string.h	1.4	96/03/31 SMI"

#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This file declares the XFN FN_string type and its methods
 * (ANSI C version).
 *
 * Two types of strings are supported:
 *	1. '\0' terminated (unsigned char *)s.
 *	3. Opaque code_set strings.
 *
 * Note:
 *	Currently, code set mismatches are not supported.
 *
 *	Where indices 'first' and 'last' are used, an inclusive range of
 *	characters is spcedified.
 *
 *	Compile with "-DNO_WC" if your system does not fully support wchar_t.
 */

typedef struct _FN_string FN_string_t;

enum {
	FN_STRING_CASE_INSENSITIVE = 0,
	FN_STRING_CASE_SENSITIVE = 1
};

enum {
	FN_STRING_INDEX_NONE = -1,
	FN_STRING_INDEX_FIRST = 0,
	FN_STRING_INDEX_LAST = INT_MAX
};

/*
 * Constructors and destructor.
 */

/* Create string with codeset of current locale by default. */
extern FN_string_t *fn_string_create(void);
extern void fn_string_destroy(FN_string_t *);

/*
 * Routines for char *.
 */

extern FN_string_t *fn_string_from_str(const unsigned char *str);
/* Allocate storlen chars (plus '\0'), copy only strlen(str) chars. */
extern FN_string_t *fn_string_from_str_n(const unsigned char *str,
					size_t storlen);
extern const unsigned char *fn_string_str(const FN_string_t *str,
					unsigned int *status);

/*
 * Routines for arbitrary code sets.
 */

extern FN_string_t *fn_string_from_contents(
	unsigned long code_set,
	unsigned long lang_terr,
	size_t charcount,
	size_t bytecount,
	const void *contents,
	unsigned int *status);
extern unsigned long fn_string_code_set(const FN_string_t *str);

extern unsigned long fn_string_lang_terr(const FN_string_t *str);

/*
 * Generic routines.
 */

/* String length in characters. */
extern size_t fn_string_charcount(const FN_string_t *str);
/* String size in bytes. */
extern size_t fn_string_bytecount(const FN_string_t *str);
/* Pointer to opaque string. */
extern const void *fn_string_contents(const FN_string_t *str);

extern FN_string_t *fn_string_copy(const FN_string_t *str);
extern FN_string_t *fn_string_assign(
	FN_string_t *dst,
	const FN_string_t *src);
/* Concat a null terminated list of strings. */
extern FN_string_t *fn_string_from_strings(
	unsigned int *status,
	const FN_string_t *s1,
	const FN_string_t *s2,
	...);
/* Make copy of string from index 'first' to 'last' */
extern FN_string_t *fn_string_from_substring(
	const FN_string_t *str,
	int first,
	int last);

extern int fn_string_is_empty(const FN_string_t *str);
/* Compare strings (simiar to strcmp(3)). */
extern int fn_string_compare(const FN_string_t *s1,
				const FN_string_t *s2,
				unsigned int string_case,
				unsigned int *status);
/* Compare substring [first, last] of 1st string with 2nd string. */
extern int fn_string_compare_substring(
	const FN_string_t *s1,
	int first,
	int last,
	const FN_string_t *s2,
	unsigned int string_case,
	unsigned int *status);
/*
 * Return index of next occurrence of 'sub' in 'str', or FN_STRING_INDEX_NONE
 * if not found.  Search begins at 'index' in 'str'.
 */
extern int fn_string_next_substring(
	const FN_string_t *str,
	const FN_string_t *sub,
	int index,
	unsigned int string_case,
	unsigned int *status);
/*
 * Return index of prev occurrence of 'sub' in 'str', or FN_STRING_INDEX_NONE
 * if not found.  Search begins at 'index' in 'str'.
 */
extern int fn_string_prev_substring(
	const FN_string_t *str,
	const FN_string_t *sub,
	int index,
	unsigned int string_case,
	unsigned int *status);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_STRING_H */
