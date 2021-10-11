/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_INT_CONST_H
#define	_SYS_INT_CONST_H

#pragma ident	"@(#)int_const.h	1.2	96/07/08 SMI"

/*
 * This file, <sys/int_const.h>, is part of the Sun Microsystems implementation
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
 * squarely behind the fixed sized types.
 */

#include <sys/isa_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Constants
 *
 * The following macros create constants of the types defined in
 * <sys/int_types.h>. The intent is that:
 *	Constants defined using these macros have a specific size and
 *	signedness. The suffix used for int64_t and uint64_t (ll and ull)
 *	are for examples only. Implementations are permitted to use other
 *	suffixes.
 *
 * The "CSTYLED" comments are flags to an internal code style analysis tool
 * telling it to silently accept the line which follows.  This internal
 * standard requires a space between arguments, but the historical,
 * non-ANSI-C ``method'' of concatenation can't tolerate those spaces.
 */
#ifdef __STDC__
/* CSTYLED */
#define	__CONCAT__(A,B) A ## B
#else
/* CSTYLED */
#define	__CONCAT__(A,B) A/**/B
#endif

#if defined(_CHAR_IS_SIGNED) || defined(__STDC__)
#define	INT8_C(c)	(c)
#endif
#define	INT16_C(c)	(c)
#define	INT32_C(c)	(c)
#ifdef  _LP64
/* CSTYLED */
#define	INT64_C(c)	__CONCAT__(c,l)
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
/* CSTYLED */
#define	INT64_C(c)	__CONCAT__(c,ll)
#endif
#endif

/* CSTYLED */
#define	UINT8_C(c)	__CONCAT__(c,u)
/* CSTYLED */
#define	UINT16_C(c)	__CONCAT__(c,u)
/* CSTYLED */
#define	UINT32_C(c)	__CONCAT__(c,u)
#ifdef  _LP64
/* CSTYLED */
#define	UINT64_C(c)	__CONCAT__(c,ul)
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
/* CSTYLED */
#define	UINT64_C(c)	__CONCAT__(c,ull)
#endif
#endif

#ifdef  _LP64
/* CSTYLED */
#define	INTMAX_C(c)	__CONCAT__(c,l)
/* CSTYLED */
#define	UINTMAX_C(c)	__CONCAT__(c,ul)
#else   /* _ILP32 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
/* CSTYLED */
#define	INTMAX_C(c)	__CONCAT__(c,ll)
/* CSTYLED */
#define	UINTMAX_C(c)	__CONCAT__(c,ull)
#else
#define	INTMAX_C(c)	(c)
#define	UINTMAX_C(c)	(c)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_INT_CONST_H */
