/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_INT_LIMITS_H
#define	_SYS_INT_LIMITS_H

#pragma ident	"@(#)int_limits.h	1.6	99/08/06 SMI"

/*
 * This file, <sys/int_limits.h>, is part of the Sun Microsystems implementation
 * of <inttypes.h> as proposed in the ISO/JTC1/SC22/WG14 C committee's working
 * draft for the revision of the current ISO C standard, ISO/IEC 9899:1990
 * Programming language - C.
 *
 * Programs/Modules should not directly include this file.  Access to the
 * types defined in this file should be through the inclusion of one of the
 * following files:
 *
 *	<limits.h>		This nested inclusion is disabled for strictly
 *				ANSI-C conforming compilations.  The *_MIN
 *				definitions are not visible to POSIX or XPG
 *				conforming applications (due to what may be
 *				a bug in the specification - this is under
 *				investigation)
 *
 *	<sys/inttypes.h>	Provides the Kernel and Driver appropriate
 *				components of <inttypes.h>.
 *
 *	<inttypes.h>		For use by applications.
 *
 * See these files for more details.
 *
 * Use at your own risk.  As of February 1996, the committee is squarely
 * behind the fixed sized types; the "least" and "fast" types are still being
 * discussed.  The probability that the "fast" types may be removed before
 * the standard is finalized is high enough that they are not currently
 * implemented.  The unimplemented limits for the unimplemented "fast"
 * types are of the form [U]INT_FAST[0-9]*_MAX, INT_FAST[0-9]*_MIN,
 * [U]INTFAST_MAX and INTFAST_MIN.
 */

#include <sys/isa_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Limits
 *
 * The following define the limits for the types defined in <sys/int_types.h>.
 *
 * INTMAX_MIN (minimum value of the largest supported signed integer type),
 * INTMAX_MAX (maximum value of the largest supported signed integer type),
 * and UINTMAX_MAX (maximum value of the largest supported unsigned integer
 * type) can be set to implementation defined limits.
 *
 * NOTE : A programmer can test to see whether an implementation supports
 * a particular size of integer by testing if the macro that gives the
 * maximum for that datatype is defined. For example, if #ifdef UINT64_MAX
 * tests false, the implementation does not support unsigned 64 bit integers.
 *
 * The type of these macros is intentionally unspecified.
 *
 * The types int8_t and int_least8_t are not defined for ISAs where the ABI
 * specifies "char" as unsigned when the translation mode in not ANSI-C.
 */
#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	INT8_MAX	(127)
#endif
#define	INT16_MAX	(32767)
#define	INT32_MAX	(2147483647)
#if defined(_LP64) || (__STDC__ - 0 == 0 && !defined(_NO_LONGLONG))
#define	INT64_MAX	(9223372036854775807LL)
#endif

#define	UINT8_MAX	(255U)
#define	UINT16_MAX	(65535U)
#define	UINT32_MAX	(4294967295U)
#if defined(_LP64) || (__STDC__ - 0 == 0 && !defined(_NO_LONGLONG))
#define	UINT64_MAX	(18446744073709551615ULL)
#endif

#ifdef INT64_MAX
#define	INTMAX_MAX	INT64_MAX
#else
#define	INTMAX_MAX	INT32_MAX
#endif

#ifdef UINT64_MAX
#define	UINTMAX_MAX	UINT64_MAX
#else
#define	UINTMAX_MAX	UINT32_MAX
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	INT_LEAST8_MAX	INT8_MAX
#endif
#define	INT_LEAST16_MAX INT16_MAX
#define	INT_LEAST32_MAX INT32_MAX
#ifdef INT64_MAX
#define	INT_LEAST64_MAX INT64_MAX
#endif

/*
 * The following four defines are currently missing from the standard and
 * that is assumed to be an oversight.
 */
#define	UINT_LEAST8_MAX	UINT8_MAX
#define	UINT_LEAST16_MAX UINT16_MAX
#define	UINT_LEAST32_MAX UINT32_MAX
#ifdef UINT64_MAX
#define	UINT_LEAST64_MAX UINT64_MAX
#endif

/*
 * The following 2 macros are provided for testing whether the types
 * intptr_t and uintptr_t (integers large enough to hold a void *) are
 * defined in this header. They are needed in case the architecture can't
 * represent a pointer in any standard integral type.
 */
#define	INTPTR_MAX
#define	UINTPTR_MAX

/*
 * It is probably a bug in the POSIX specification (IEEE-1003.1-1990) that
 * when including <limits.h> that the suffix _MAX is reserved but not the
 * suffix _MIN.  However, until that issue is resolved....
 */
#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	INT8_MIN	(-128)
#endif
#define	INT16_MIN	(-32767-1)
#define	INT32_MIN	(-2147483647-1)
#if defined(_LP64) || (__STDC__ - 0 == 0 && !defined(_NO_LONGLONG))
#define	INT64_MIN	(-9223372036854775807LL-1)
#endif

#ifdef INT64_MIN
#define	INTMAX_MIN	INT64_MIN
#else
#define	INTMAX_MIN	INT32_MIN
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	INT_LEAST8_MIN	INT8_MIN
#endif
#define	INT_LEAST16_MIN	INT16_MIN
#define	INT_LEAST32_MIN INT32_MIN
#ifdef INT64_MIN
#define	INT_LEAST64_MIN	INT64_MIN
#endif

#endif	/* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_INT_LIMITS_H */
