/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_STRING_HH
#define	_XFN_FN_STRING_HH

#pragma ident	"@(#)FN_string.hh	1.3	96/03/31 SMI"

/*
 * This file declares the XFN FN_string type and its methods
 * (C++ version).
 *
 * Note:
 *      There is currently no support for strings with  mismatched code sets.
 *
 *	Where indices 'first' and 'last' are used, an inclusive range of
 *	characters is specified.
 */

#ifdef DEBUG
#include <stdio.h>	/* for FILE */
#endif
#include <stdarg.h>	/* for var args */

#include <xfn/FN_string.h>

class FN_string_rep;

class FN_string {

	// static int	nnodes;

	int constr(FN_string_rep *);
	int constr(FN_string_rep *, const void *, size_t);
	void destr(void);

	FN_string_rep	*rep;
	void init_va_rep(size_t,
	    unsigned int *status,
	    const FN_string *s1,
	    const FN_string *s2,
	    va_list ap);
    public:
	/*
	 * Constructors and destructor.
	 */

	// Create strings of code set of current locale by default.
	FN_string();
	~FN_string();

	FN_string(const FN_string &);
	FN_string &operator=(const FN_string &);

	/*
	 * Routines for unsigned char *.
	 */

	FN_string(const unsigned char *str);
	// Allocate storlen chars (plus '\0'), copy only strlen(str) chars.
	FN_string(const unsigned char *str, size_t maxchars);
	const unsigned char *str(unsigned int *status = 0) const;

	/*
	 * Routines for arbitrary code sets.
	 */

	FN_string(unsigned long code_set,
	    unsigned long lang_terr,
	    size_t charcount,
	    size_t bytecount,
	    const void *contents,
	    unsigned int *status = 0);
	unsigned long code_set() const;
	unsigned long lang_terr() const;

	/*
	 * Generic routines.
	 */

	// String length in characters.
	size_t charcount(void) const;
	// String size in bytes.
	size_t bytecount(void) const;
	// Pointer to opaque string.
	const void *contents(void) const;

	// Concat a null terminated list of strings.
	// this is called fn_string_from_strings in the XFN C interface.
	FN_string(unsigned int *status,
	    const FN_string *s1,
	    const FN_string *s2,
	    ...);

	static size_t calculate_va_size(const FN_string *, const FN_string *,
	    va_list rest);
	FN_string(size_t charcount, unsigned int *status,
	    const FN_string *s1, const FN_string *s2, va_list rest);

	// Make copy of string from index 'first' to 'last'.
	FN_string(const FN_string &, int first, int last);

	// Test for empty string.
	int is_empty(void) const;

	// Compare strings (simiar to strcmp(3)).
	int compare(const FN_string &,
	    unsigned int string_case = FN_STRING_CASE_SENSITIVE,
	    unsigned int *status = 0) const;

	// Compare substring [first, last] of 1st string with 2nd string.
	int compare_substring(int first, int last,
	    const FN_string &s2,
	    unsigned int string_case = FN_STRING_CASE_SENSITIVE,
	    unsigned int *status = 0) const;

	/*
	 * Return index of next occurrence of 'sub' in 'str', or
	 * FN_STRING_INDEX_NONE if not found.
	 * Search begins at 'index' in 'str'.
	 */
	int next_substring(const FN_string &substring,
	    int index = FN_STRING_INDEX_FIRST,
	    unsigned int string_case = FN_STRING_CASE_SENSITIVE,
	    unsigned int *status = 0) const;

	/*
	 * Return index of prev occurrence of 'sub' in 'str',
	 * or FN_STRING_INDEX_NONE if not found.
	 * Search begins at 'index' in 'str'.
	 */
	int prev_substring(const FN_string &substring,
	    int index = FN_STRING_INDEX_LAST,
	    unsigned int string_case = FN_STRING_CASE_SENSITIVE,
	    unsigned int *status = 0) const;

#ifdef DEBUG
	static void report(FILE *);
#endif
};

#endif /* _XFN_FN_STRING_HH */
