/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * An application should not include this header directly.  Instead it
 * should be included only through the inclusion of other Sun headers.
 *
 * The contents of this header is limited to identifiers specified in the
 * C Standard.  Any new identifiers specified in future amendments to the
 * C Standard must be placed in this header.  If these new identifiers
 * are required to also be in the C++ Standard "std" namespace, then for
 * anything other than macro definitions, corresponding "using" directives
 * must also be added to <string.h>.
 */

#ifndef _ISO_STRING_ISO_H
#define	_ISO_STRING_ISO_H

#pragma ident	"@(#)string_iso.h	1.2	99/11/09 SMI"
/* SVr4.0 1.7.1.12 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

#if !defined(_SIZE_T) || __cplusplus >= 199711L
#define	_SIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef unsigned long	size_t;		/* size of something in bytes */
#else
typedef unsigned int	size_t;		/* (historical version) */
#endif
#endif	/* !_SIZE_T */

#ifndef	NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#if defined(__STDC__)

extern int memcmp(const void *, const void *, size_t);
extern void *memcpy(void *, const void *, size_t);
extern void *memmove(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern char *strcat(char *, const char *);
extern int strcmp(const char *, const char *);
extern char *strcpy(char *, const char *);
extern int strcoll(const char *, const char *);
extern size_t strcspn(const char *, const char *);
extern char *strerror(int);
extern size_t strlen(const char *);
extern char *strncat(char *, const char *, size_t);
extern int strncmp(const char *, const char *, size_t);
extern char *strncpy(char *, const char *, size_t);
extern size_t strspn(const char *, const char *);
extern char *strtok(char *, const char *);
extern size_t strxfrm(char *, const char *, size_t);
#if __cplusplus >= 199711L
extern const char *strchr(const char *, int);
extern "C++" {
	inline char *strchr(char *__s, int __c) {
		return (char *)strchr((const char *)__s, __c);
	}
} 
extern const char *strpbrk(const char *, const char *);
extern "C++" {
	inline char * strpbrk(char *__s1, const char *__s2) {
		return (char *)strpbrk((const char *)__s1, __s2);
	}
}
extern const char *strrchr(const char *, int);
extern "C++" {
	inline char * strrchr(char *__s, int __c) {
		return (char *)strrchr((const char *)__s, __c);
	}
}
extern const char *strstr(const char *, const char *);
extern "C++" {
	inline char *strstr(char *__s1, const char *__s2) {
		return (char *)strstr((const char *)__s1, __s2);
	}
}
extern const void *memchr(const void *, int, size_t);
#ifndef _MEMCHR_INLINE
#define	_MEMCHR_INLINE
extern "C++" {
	inline void* memchr(void * __s, int __c, size_t __n) {
		return (void *)memchr((const void *)__s, __c, __n);
	}
}
#endif	/* _MEMCHR_INLINE */
#else /* __cplusplus >= 199711L */
extern void *memchr(const void *, int, size_t);
extern char *strchr(const char *, int);
extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);
extern char *strstr(const char *, const char *);
#endif /* __cplusplus >= 199711L */

#else	/* __STDC__ */

extern void *memchr();
extern int memcmp();
extern void *memcpy();
extern void *memmove();
extern void *memset();
extern char *strcat();
extern char *strchr();
extern int strcmp();
extern int strcoll();
extern char *strcpy();
extern size_t strcspn();
extern char *strerror();
extern size_t strlen();
extern char *strncat();
extern int strncmp();
extern char *strncpy();
extern char *strpbrk();
extern char *strrchr();
extern size_t strspn();
extern char *strstr();
extern char *strtok();
extern size_t strxfrm();

#endif	/* __STDC__ */

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_STRING_ISO_H */
