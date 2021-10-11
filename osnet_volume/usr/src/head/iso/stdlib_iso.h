/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * An application should not include this header directly.  Instead it
 * should be included only through the inclusion of other Sun headers.
 *
 * The contents of this header is limited to identifiers specified in the
 * C Standard.  Any new identifiers specified in future amendments to the
 * C Standard must be placed in this header.  If these new identifiers
 * are required to also be in the C++ Standard "std" namespace, then for
 * anything other than macro definitions, corresponding "using" directives
 * must also be added to <locale.h>.
 */

#ifndef _ISO_STDLIB_ISO_H
#define	_ISO_STDLIB_ISO_H

#pragma ident	"@(#)stdlib_iso.h	1.1	99/08/09 SMI" /* SVr4.0 1.22 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

typedef	struct {
	int	quot;
	int	rem;
} div_t;

typedef struct {
	long	quot;
	long	rem;
} ldiv_t;

#if !defined(_SIZE_T) || __cplusplus >= 199711L
#define	_SIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef unsigned long	size_t;		/* size of something in bytes */
#else
typedef unsigned int    size_t;		/* (historical version) */
#endif
#endif	/* !_SIZE_T */

#ifndef	NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS    0
#define	RAND_MAX	32767

#ifndef _WCHAR_T
#define	_WCHAR_T
#if defined(_LP64)
typedef	int	wchar_t;
#else
typedef long	wchar_t;
#endif
#endif	/* !_WCHAR_T */

#if defined(__STDC__)

extern unsigned char	__ctype[];
#define	MB_CUR_MAX	__ctype[520]

extern void abort(void);
extern int abs(int);
extern int atexit(void (*)(void));
extern double atof(const char *);
extern int atoi(const char *);
extern long int atol(const char *);
extern void *bsearch(const void *, const void *, size_t, size_t,
	int (*)(const void *, const void *));
extern void *calloc(size_t, size_t);
extern div_t div(int, int);
extern void exit(int);
extern void free(void *);
extern char *getenv(const char *);
extern long int labs(long);
extern ldiv_t ldiv(long, long);
extern void *malloc(size_t);
extern int mblen(const char *, size_t);
extern size_t mbstowcs(wchar_t *, const char *, size_t);
extern int mbtowc(wchar_t *, const char *, size_t);
extern void qsort(void *, size_t, size_t,
	int (*)(const void *, const void *));
extern int rand(void);
extern void *realloc(void *, size_t);
extern void srand(unsigned int);
extern double strtod(const char *, char **);
extern long int strtol(const char *, char **, int);
extern unsigned long int strtoul(const char *, char **, int);
extern int system(const char *);
extern int wctomb(char *, wchar_t);
extern size_t wcstombs(char *, const wchar_t *, size_t);

#else /* not __STDC__ */

extern unsigned char	_ctype[];
#define	MB_CUR_MAX	_ctype[520]

extern void abort();
extern int abs();
extern int atexit();
extern double atof();
extern int atoi();
extern long int atol();
extern void *bsearch();
extern void *calloc();
extern div_t div();
extern void exit();
extern void free();
extern char *getenv();
extern long int labs();
extern ldiv_t ldiv();
extern void *malloc();
extern int mblen();
extern size_t mbstowcs();
extern int mbtowc();
extern void qsort();
extern int rand();
extern void *realloc();
extern void srand();
extern double strtod();
extern long int strtol();
extern unsigned long strtoul();
extern int system();
extern int wctomb();
extern size_t wcstombs();

#endif	/* __STDC__ */

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_STDLIB_ISO_H */
