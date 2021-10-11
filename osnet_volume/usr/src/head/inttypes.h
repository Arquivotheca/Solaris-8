/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _INTTYPES_H
#define	_INTTYPES_H

#pragma ident	"@(#)inttypes.h	1.1	96/04/02 SMI"

/*
 * This file, <inttypes.h>, is tracking the ISO/JTC1/SC22/WG14 C committee's
 * working draft for the revision of the current ISO C standard, ISO/IEC
 * 9899:1990 Programming language - C.
 *
 * ISO	International Organization for Standardization.
 * JTC1	Joint Technical Committee 1, Information technology.
 * SC22	Programming languages, their environments and system software
 *	interfaces.
 * WG14	Working Group 14, C
 *
 * The contents of this file (and the nested files of the implementation) will
 * track this standard without regard for release to release compatibility.
 * Use at your own risk.  As of February 1996, the committee is squarely
 * behind the fixed sized types; the "least" and "fast" types are still being
 * discussed.  The probability that the "fast" types may be removed before
 * the standard is finalized is high enough that they are not currently
 * implemented.
 */

#include <sys/inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Conversion functions
 *
 * The evolving standard proposes the following two routines convert from
 * strings to the largest supported integer types. They parallel the strtol
 * and strtoul functions in Standard C.
 *
 *	extern intmax_t strtoimax(const char *, char**, int);
 *	extern uintmax_t strtoumax(const char *, char**, int);
 *
 * Due to issues with the standard itself with respect to these interfaces
 * and the difficulties associated with tracking evolving binary interfaces,
 * they are not currently implemented and are unlikely to be implemented
 * until the standard is finalized.  At that time, the routines will appear
 * in libc and the prototypes will appear directly in this file.
 *
 * Note that Solaris provides both "long" and "long long" versions of these
 * string conversion functions, so if the underlying type is known, the
 * functionality exists in the system.
 */

#ifdef __cplusplus
}
#endif

#endif /* _INTTYPES_H */
