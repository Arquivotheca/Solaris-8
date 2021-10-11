/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_INTTYPES_H
#define	_SYS_INTTYPES_H

#pragma ident	"@(#)inttypes.h	1.2	98/01/16 SMI"

/*
 * This file, <sys/inttypes.h>, contains (through nested inclusion) the
 * vast majority of the facilities specified for <inttypes.h> as proposed
 * in the ISO/JTC1/SC22/WG14 C committee's working draft for the revision
 * of the current ISO C standard, ISO/IEC 9899:1990 Programming language - C.
 *
 * ISO	International Organization for Standardization.
 * JTC1	Joint Technical Committee 1, Information technology.
 * SC22	Programming languages, their environments and system software
 *	interfaces.
 * WG14	Working Group 14, C
 *
 * Only the prototypes for the strtoimax(3) and strtoumax(3) functions are
 * not included in this header.  Kernel/Driver developers are encouraged to
 * include this file to access the fixed size types, limits and utility
 * macros with the understanding that the contents of this file (and nested
 * files of the implementation) will track this standard without regard for
 * release to release compatibility.  Application developers should use the
 * standard defined header <inttypes.h>.
 *
 * Use at your own risk.  As of February 1996, the committee is squarely
 * behind the fixed sized types; the "least" and "fast" types are still being
 * discussed.  The probability that the "fast" types may be removed before
 * the standard is finalized is high enough that they are not currently
 * implemented.
 */

#include <sys/int_types.h>
#if !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
#include <sys/int_limits.h>
#include <sys/int_const.h>
#include <sys/int_fmtio.h>
#endif

#endif /* _SYS_INTTYPES_H */
