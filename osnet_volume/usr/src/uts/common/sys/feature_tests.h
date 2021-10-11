/*
 * Copyright (c) 1993-1999, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FEATURE_TESTS_H
#define	_SYS_FEATURE_TESTS_H

#pragma ident	"@(#)feature_tests.h	1.18	99/07/26 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * 	Values of _POSIX_C_SOURCE
 *
 *		undefined	not a POSIX compilation
 *			1	POSIX.1-1990 compilation
 *			2	POSIX.2-1992 compilation
 *		  199309L	POSIX.1b-1993 compilation (Real Time)
 *		  199506L	POSIX.1c-1995 compilation (POSIX Threads)
 */
#if	defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE)
#define	_POSIX_C_SOURCE	1
#endif

/*
 * Large file interfaces:
 *
 *	_LARGEFILE_SOURCE
 *		1		large file-related additions to POSIX
 *				interfaces requested (fseeko, etc.)
 *	_LARGEFILE64_SOURCE
 *		1		transitional large-file-related interfaces
 *				requested (seek64, stat64, etc.)
 *
 * The corresponding announcement macros are respectively:
 *	_LFS_LARGEFILE
 *	_LFS64_LARGEFILE
 * (These are set in <unistd.h>.)
 *
 * Requesting _LARGEFILE64_SOURCE implies requesting _LARGEFILE_SOURCE as
 * well.
 *
 * The large file interfaces are made visible regardless of the initial values
 * of the feature test macros under certain circumstances:
 *    -	If no explicit standards-conformant environment is requested (neither
 *	of _POSIX_SOURCE nor _XOPEN_SOURCE is defined and the value of
 *	__STDC__ does not imply standards conformance).
 *    -	Extended system interfaces are explicitly requested (__EXTENSIONS__
 * 	is defined).
 *    -	Access to in-kernel interfaces is requested (_KERNEL or _KMEMUSER is
 *	defined).  (Note that this dependency is an artifact of the current
 *	kernel implementation and may change in future releases.)
 */
#if 	((__STDC__ - 0 == 0) && \
		!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
		defined(_KERNEL) || defined(_KMEMUSER) || \
		defined(__EXTENSIONS__)
#undef	_LARGEFILE64_SOURCE
#define	_LARGEFILE64_SOURCE	1
#endif
#if	_LARGEFILE64_SOURCE - 0 == 1
#undef	_LARGEFILE_SOURCE
#define	_LARGEFILE_SOURCE	1
#endif

/*
 * Large file compilation environment control:
 *
 * The setting of _FILE_OFFSET_BITS controls the size of various file-related
 * types and governs the mapping between file-related source function symbol
 * names and the corresponding binary entry points.
 *
 * In the 32-bit environment, the default value is 32; if not set, set it to
 * the default here, to simplify tests in other headers.
 *
 * In the 64-bit compilation environment, the only value allowed is 64.
 */
#if defined(_LP64)
#ifndef _FILE_OFFSET_BITS
#define	_FILE_OFFSET_BITS	64
#endif
#if	_FILE_OFFSET_BITS - 0 != 64
#error	"invalid _FILE_OFFSET_BITS value specified"
#endif
#else	/* _LP64 */
#ifndef	_FILE_OFFSET_BITS
#define	_FILE_OFFSET_BITS	32
#endif
#if	_FILE_OFFSET_BITS - 0 != 32 && _FILE_OFFSET_BITS - 0 != 64
#error	"invalid _FILE_OFFSET_BITS value specified"
#endif
#endif	/* _LP64 */

/*
 * UNIX 95 implementation
 *
 * As specified in the following X/Open specifications:
 *
 *   System Interfaces and Headers, Issue 4, Version 2
 *   Commands and Utilities, Issue 4, Version 2
 *   Networking Services, Issue 4
 *   X/Open Curses, Issue 4
 *
 * application writers wishing to use any functions specified
 * as X/Open UNIX Extension must define _XOPEN_SOURCE and
 * _XOPEN_SOURCE_EXTENDED=1.  The Sun internal macro _XPG4_2
 * should not be used in its place as unexpected results may
 * occur.
 */
#if (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE_EXTENDED - 0 == 1)
#define	_XPG4_2
#endif

/*
 * UNIX 98 implementation
 *
 * As specified in the following X/Open specfications:
 *
 *   System Interfaces and Headers, Issue 5
 *   Commands and Utilities, Issue 5
 *   Networking Services, Issue 5
 *   X/Open Curses, Issue 4, Version 2
 *
 * application writers wishing to utilize UNIX 98 functionality
 * must define _XOPEN_SOURCE=500.  This turns on UNIX 95 functionality
 * which is a subset of UNIX 98, and also turns on POSIX Realtime and
 * POSIX Threads functionality.
 */
#if (_XOPEN_SOURCE - 0 == 500)
#define	_XPG5
#define	_XPG4_2
#undef	_POSIX_C_SOURCE
#define	_POSIX_C_SOURCE			199506L
#define	_POSIX_PTHREAD_SEMANTICS	1
#endif

/*
 * _XOPEN_VERSION defined with the value of 3 indicates an XPG3
 * application.  _XOPEN_VERSION defined with the value of 4
 * indicates an XPG4 or XPG4v2 application.  _XOPEN_VERSION defined
 * with a value of 500 indicates a XSH5 (UNIX 98) application. The
 * appropriate version of XPG is indicated by use of the following
 * macros:
 *
 * _XOPEN_SOURCE				XPG, Issue 3
 * _XOPEN_SOURCE && _XOPEN_VERSION = 4		XPG, Issue 4
 * _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED = 1	XPG, Issue 4, Version 2 (XPG4v2)
 * _XOPEN_SOURCE = 500				XPG, Issue 5
 *
 * Defining both _XOPEN_SOURCE and _XOPEN_SOURCE_EXTENDED = 1 will
 * automatically define _XOPEN_VERSION = 4.
 */
#ifndef	_XOPEN_VERSION
#ifdef	_XPG5
#define	_XOPEN_VERSION 500
#else
#ifdef	_XPG4_2
#define	_XOPEN_VERSION	4
#else
#define	_XOPEN_VERSION	3
#endif
#endif
#endif

/*
 * The following macro indicates header support for the ANSI C++
 * standard.  The ISO/IEC designation for this is ISO/IEC FDIS 14882.
 */
#define	_ISO_CPP_14882_1998

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FEATURE_TESTS_H */
