/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PROF_H
#define	_PROF_H

#pragma ident	"@(#)prof.h	1.11	99/03/11 SMI"	/* SVr4.0 1.10.1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	MARK
#define	MARK(K)	{}
#else
#undef	MARK

#if defined(__STDC__)

#if defined(__i386)
#define	MARK(K)	{\
		asm("	.data"); \
		asm("	.align 4"); \
		asm("."#K".:"); \
		asm("	.long 0"); \
		asm("	.text"); \
		asm("M."#K":"); \
		asm("	movl	$."#K"., %edx"); \
		asm("	call _mcount"); \
		}
#endif

#if defined(__sparc)
#define	MARK(K)	{\
		asm("	.reserve	."#K"., 4, \".bss\", 4"); \
		asm("M."#K":"); \
		asm("	sethi	%hi(."#K".), %o0"); \
		asm("	call	_mcount"); \
		asm("	or	%o0, %lo(."#K".), %o0"); \
		}
#endif

#else	/* __STDC__ */

#if defined(__i386)
#define	MARK(K)	{\
		asm("	.data"); \
		asm("	.align 4"); \
		asm(".K.:"); \
		asm("	.long 0"); \
		asm("	.text"); \
		asm("M.K:"); \
		asm("	movl	$.K., %edx"); \
		asm("	call _mcount"); \
		}
#endif

#if defined(__sparc)
#define	MARK(K)	{\
		asm("	.reserve	.K., 4, \".bss\", 4"); \
		asm("M.K:"); \
		asm("	sethi	%hi(.K.), %o0"); \
		asm("	call	_mcount"); \
		asm("	or	%o0, %lo(.K.), %o0"); \
		}
#endif

#endif	/* __STDC__ */

#endif	/* MARK */

#ifdef	__cplusplus
}
#endif

#endif	/* _PROF_H */
