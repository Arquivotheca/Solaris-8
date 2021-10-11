/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_INT_FMTIO_H
#define	_SYS_INT_FMTIO_H

#pragma ident	"@(#)int_fmtio.h	1.2	96/07/08 SMI"

/*
 * This file, <sys/int_fmtio.h>, is part of the Sun Microsystems implementation
 * of <inttypes.h> as proposed in the ISO/JTC1/SC22/WG14 C committee's working
 * draft for the revision of the current ISO C standard, ISO/IEC 9899:1990
 * Programming language - C.
 *
 * Programs/Modules should not directly include this file.  Access to the
 * types defined in this file should be through the inclusion of one of the
 * following files:
 *
 *	<sys/inttypes.h>	Provides the Kernel and Driver appropriate
 *				components of <inttypes.h>.
 *
 *	<inttypes.h>		For use by applications.
 *
 * See these files for more details.
 *
 * Use at your own risk.  This file will track the evolution of the revision
 * of the current ISO C standard.  As of February 1996, the committee is
 * squarely behind the fixed sized types; the "least" and "fast" types are
 * still being discussed.  The probability that the "fast" types may be
 * removed before the standard is finalized is high enough that they are
 * not currently implemented.  The form of the unimplemented formating
 * macros for the unimplemented "fast" types is PRI[dioxXu]FAST[0-9]*,
 * PRI[doxu]FAST and SCN[idox]FAST.
 */

#include <sys/isa_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Formatted I/O
 *
 * The following macros can be used even when an implementation has not
 * extended the printf/scanf family of functions.
 *
 * The form of the names of the macros is either "PRI" for printf specifiers
 * or "SCN" for scanf specifiers, followed by the conversion specifier letter
 * followed by the datatype size. For example, PRId32 is the macro for
 * the printf d conversion specifier with the flags for 32 bit datatype.
 *
 * Separate macros are given for printf and scanf because typically different
 * size flags must prefix the conversion specifier letter.
 *
 * An example using one of these macros:
 *
 *	uint64_t u;
 *	printf("u = %016" PRIx64 "\n", u);
 *
 * For the purpose of example, the definitions of the printf/scanf macros
 * below have the values appropriate for a machine with 16 bit shorts,
 * 32 bit ints, and 64 bit longs.
 */

/*
 * printf macros for signed integers
 */
#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRId8			"d"
#endif
#define	PRId16			"d"
#define	PRId32			"d"
#ifdef  _LP64
#define	PRId64			"ld"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRId64			"lld"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIdLEAST8		"d"
#endif
#define	PRIdLEAST16		"d"
#define	PRIdLEAST32		"d"
#ifdef  _LP64
#define	PRIdLEAST64		"ld"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIdLEAST64		"lld"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIi8			"i"
#endif
#define	PRIi16			"i"
#define	PRIi32			"i"
#ifdef  _LP64
#define	PRIi64			"li"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIi64			"lli"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIiLEAST8		"i"
#endif
#define	PRIiLEAST16		"i"
#define	PRIiLEAST32		"i"
#ifdef  _LP64
#define	PRIiLEAST64		"li"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIiLEAST64		"lli"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIo8			"o"
#endif
#define	PRIo16			"o"
#define	PRIo32			"o"
#ifdef  _LP64
#define	PRIo64			"lo"
#else	/* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIo64			"llo"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIoLEAST8		"o"
#endif
#define	PRIoLEAST16		"o"
#define	PRIoLEAST32		"o"
#ifdef  _LP64
#define	PRIoLEAST64		"lo"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIoLEAST64		"llo"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIx8			"x"
#endif
#define	PRIx16			"x"
#define	PRIx32			"x"
#ifdef  _LP64
#define	PRIx64			"lx"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIx64			"llx"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIxLEAST8		"x"
#endif
#define	PRIxLEAST16		"x"
#define	PRIxLEAST32		"x"
#ifdef  _LP64
#define	PRIxLEAST64		"lx"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIxLEAST64		"llx"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIX8			"X"
#endif
#define	PRIX16			"X"
#define	PRIX32			"X"
#ifdef  _LP64
#define	PRIX64			"lX"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIX64			"llX"
#endif
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	PRIXLEAST8		"X"
#endif
#define	PRIXLEAST16		"X"
#define	PRIXLEAST32		"X"
#ifdef  _LP64
#define	PRIXLEAST64		"lX"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIXLEAST64		"llX"
#endif
#endif

/*
 * printf macros for unsigned integers
 */
#define	PRIu8			"u"
#define	PRIu16			"u"
#define	PRIu32			"u"
#ifdef  _LP64
#define	PRIu64			"lu"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIu64			"llu"
#endif
#endif

#define	PRIuLEAST8		"u"
#define	PRIuLEAST16		"u"
#define	PRIuLEAST32		"u"
#ifdef  _LP64
#define	PRIuLEAST64		"lu"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIuLEAST64		"llu"
#endif
#endif

/*
 * scanf macros
 */
#define	SCNd16			"hd"
#define	SCNd32			"d"
#ifdef  _LP64
#define	SCNd64			"ld"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	SCNd64			"lld"
#endif
#endif

#define	SCNi16			"hi"
#define	SCNi32			"i"
#ifdef  _LP64
#define	SCNi64			"li"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	SCNi64			"lli"
#endif
#endif

#define	SCNo16			"ho"
#define	SCNo32			"o"
#ifdef  _LP64
#define	SCNo64			"lo"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	SCNo64			"llo"
#endif
#endif

#define	SCNu16			"hu"
#define	SCNu32			"u"
#ifdef  _LP64
#define	SCNu64			"lu"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	SCNu64			"llu"
#endif
#endif

#define	SCNx16			"hx"
#define	SCNx32			"x"
#ifdef  _LP64
#define	SCNx64			"lx"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	SCNx64			"llx"
#endif
#endif

/*
 * The following macros define I/O formats for intmax_t and uintmax_t.
 */
#ifdef  _LP64
#define	PRIdMAX			"ld"
#define	PRIoMAX			"lo"
#define	PRIxMAX			"lx"
#define	PRIuMAX			"lu"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	PRIdMAX			"lld"
#define	PRIoMAX			"llo"
#define	PRIxMAX			"llx"
#define	PRIuMAX			"llu"
#else	/* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */
#define	PRIdMAX			"d"
#define	PRIoMAX			"o"
#define	PRIxMAX			"x"
#define	PRIuMAX			"u"
#endif	/* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */
#endif	/* _ILP32 */

#ifdef  _LP64
#define	SCNiMAX			"li"
#define	SCNdMAX			"ld"
#define	SCNoMAX			"lo"
#define	SCNxMAX			"lx"
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
#define	SCNiMAX			"lli"
#define	SCNdMAX			"lld"
#define	SCNoMAX			"llo"
#define	SCNxMAX			"llx"
#else	/* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */
#define	SCNiMAX			"i"
#define	SCNdMAX			"d"
#define	SCNoMAX			"o"
#define	SCNxMAX			"x"
#endif	/* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */
#endif	/* _ILP32 */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_INT_FMTIO_H */
