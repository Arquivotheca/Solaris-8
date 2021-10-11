/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_MEMORY_H
#define	_MEMORY_H

#pragma ident	"@(#)memory.h	1.12	99/11/09 SMI"	/* SVr4.0 1.4.1.2 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern void *memccpy(void *, const void *, int, size_t);
#if __cplusplus >= 199711L
namespace std {
extern const void *memchr(const void *, int, size_t);
#ifndef _MEMCHR_INLINE
#define	_MEMCHR_INLINE
extern "C++" {
	inline void *memchr(void * __s, int __c, size_t __n) {
		return (void*)memchr((const void *) __s, __c, __n);
	}
}
#endif /* _MEMCHR_INLINE */
} /* end of namespace std */
using std::memchr;
#else
extern void *memchr(const void *, int, size_t);
#endif
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);
#else
extern void *memccpy(),
extern void *memchr(),
extern void *memcpy(),
extern void *memset();
extern int memcmp();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _MEMORY_H */
